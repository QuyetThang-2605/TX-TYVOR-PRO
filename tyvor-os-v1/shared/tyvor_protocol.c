#include "tyvor_protocol.h"
#include "tyvor_status.h"
#include <string.h>
uint16_t tyvor_crc16(const uint8_t *p,size_t n) {
    uint16_t crc=0xffffu;
    if (!p && n) return 0;
    for (size_t i=0;i<n;++i) {
        crc^=(uint16_t)((uint16_t)p[i]<<8);
        for (unsigned b=0;b<8;++b)
            crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1);
    }
    return crc;
}
static int header_valid(uint8_t version,uint8_t type,size_t n) {
    if (version!=TYVOR_VERSION || n>TYVOR_PAYLOAD_MAX) return 0;
    switch (type) {
    case TYVOR_STATUS:return n==TYVOR_STATUS_LEN;
    case TYVOR_UI_REQUEST:case TYVOR_ACK:case TYVOR_HELLO:case TYVOR_SESSION_ACK:return n==2;
    case TYVOR_DIAGNOSTIC:return 1;
    default:return 0;
    }
}
int tyvor_payload_valid(uint8_t type,const uint8_t *b,size_t n) {
    tyvor_status_t s;
    if ((!b && n) || !header_valid(TYVOR_VERSION,type,n)) return 0;
    switch (type) {
    case TYVOR_STATUS:return tyvor_status_unpack(&s,b,n)==0;
    case TYVOR_UI_REQUEST:return (b[0]==TYVOR_REQ_MODEL_INFO && !b[1]) ||
                                (b[0]==TYVOR_REQ_PAGE && b[1]<7) ||
                                (b[0]==TYVOR_REQ_QUICKSET);
    case TYVOR_ACK:return b[0]>=1 && b[0]<=3 && b[1]<=3;
    case TYVOR_HELLO:case TYVOR_SESSION_ACK:
        return (b[0]==TYVOR_ROLE_CONTROL || b[0]==TYVOR_ROLE_UI) && b[1]==0;
    case TYVOR_DIAGNOSTIC:return 1;
    default:return 0;
    }
}
static void put_le(uint8_t *p,uint64_t v,unsigned n) {
    for (unsigned i=0;i<n;++i) {p[i]=(uint8_t)v;v>>=8;}
}
static uint64_t get_le(const uint8_t *p,unsigned n) {
    uint64_t v=0;for (unsigned i=0;i<n;++i) v|=(uint64_t)p[i]<<(8*i);return v;
}
static int tag_valid(uint8_t type,const tyvor_session_tag_t *s,uint8_t seq) {
    if (!s || !s->sender_id || !s->sender_epoch) return 0;
    if (type==TYVOR_HELLO) return seq==0 &&
        ((!s->receiver_id && !s->receiver_epoch) || (s->receiver_id && s->receiver_epoch));
    if (!s->receiver_id || !s->receiver_epoch) return 0;
    return type!=TYVOR_SESSION_ACK || seq==0;
}
size_t tyvor_encode(uint8_t type,uint8_t seq,const tyvor_session_tag_t *s,
                    const uint8_t *b,uint8_t n,uint8_t *out,size_t cap) {
    if (!out || cap<(size_t)n+TYVOR_FRAME_MIN ||
        !tag_valid(type,s,seq) || !tyvor_payload_valid(type,b,n)) return 0;
    out[0]=TYVOR_SYNC0;out[1]=TYVOR_SYNC1;out[2]=TYVOR_VERSION;
    out[3]=type;out[4]=seq;out[5]=n;
    put_le(out+6,s->sender_id,8);put_le(out+14,s->receiver_id,8);
    put_le(out+22,s->sender_epoch,4);put_le(out+26,s->receiver_epoch,4);
    if (n) memcpy(out+TYVOR_HEADER_LEN,b,n); /* Source/output must not overlap. */
    uint16_t crc=tyvor_crc16(out+2,(size_t)n+TYVOR_HEADER_LEN-2);
    put_le(out+TYVOR_HEADER_LEN+n,crc,2);
    return (size_t)n+TYVOR_FRAME_MIN;
}
int tyvor_decode(const uint8_t *in,size_t n,tyvor_frame_t *out) {
    if (!in || !out || n<TYVOR_FRAME_MIN || in[0]!=TYVOR_SYNC0 || in[1]!=TYVOR_SYNC1 ||
        !header_valid(in[2],in[3],in[5]) || n!=(size_t)in[5]+TYVOR_FRAME_MIN) return TYVOR_ERR_FRAME;
    if (tyvor_crc16(in+2,n-4)!=(uint16_t)get_le(in+n-2,2)) return TYVOR_ERR_CRC;
    tyvor_frame_t f={0};
    f.session.sender_id=get_le(in+6,8);f.session.receiver_id=get_le(in+14,8);
    f.session.sender_epoch=(uint32_t)get_le(in+22,4);
    f.session.receiver_epoch=(uint32_t)get_le(in+26,4);
    if (!tag_valid(in[3],&f.session,in[4])) return TYVOR_ERR_SESSION;
    if (!tyvor_payload_valid(in[3],in+TYVOR_HEADER_LEN,in[5])) return TYVOR_ERR_PAYLOAD;
    f.version=in[2];f.type=in[3];f.sequence=in[4];f.length=in[5];
    if (f.length) memcpy(f.payload,in+TYVOR_HEADER_LEN,f.length);
    *out=f;return TYVOR_OK;
}
void tyvor_parser_init(tyvor_parser_t *p,uint32_t timeout_ms) {
    if (p) {memset(p,0,sizeof(*p));p->timeout_ms=timeout_ms?timeout_ms:TYVOR_FRAME_TIMEOUT_MS;}
}
static void discard(tyvor_parser_t *p,size_t n) {
    p->used-=n;
    memmove(p->raw,p->raw+n,p->used);
    memmove(p->arrival_ms,p->arrival_ms+n,p->used*sizeof(p->arrival_ms[0]));
}
static int expired(const tyvor_parser_t *p,uint64_t now) {
    return now<p->arrival_ms[0] || now-p->arrival_ms[0]>=p->timeout_ms;
}
static void drain(tyvor_parser_t *p,uint64_t now,tyvor_frame_fn callback,void *ctx) {
    while (p->used) {
        if (p->raw[0]!=TYVOR_SYNC0 || (p->used>=2 && p->raw[1]!=TYVOR_SYNC1)) {
            ++p->stats.noise_bytes;discard(p,1);continue;
        }
        if (p->used>=6 && !header_valid(p->raw[2],p->raw[3],p->raw[5])) {
            ++p->stats.frame_errors;discard(p,1);continue;
        }
        size_t want=p->used>=6?(size_t)p->raw[5]+TYVOR_FRAME_MIN:TYVOR_FRAME_MIN;
        if (p->used<want) {
            if (expired(p,now)) {++p->stats.timeouts;discard(p,1);continue;}
            return; /* A recent suffix keeps its own original arrival deadline. */
        }
        uint64_t completed=p->arrival_ms[want-1];
        if (now<p->arrival_ms[0] || completed<p->arrival_ms[0] ||
            completed-p->arrival_ms[0]>=p->timeout_ms) {
            ++p->stats.timeouts;discard(p,1);continue;
        }
        tyvor_frame_t f;int result=tyvor_decode(p->raw,want,&f);
        if (result) {
            if (result==TYVOR_ERR_CRC) ++p->stats.crc_errors;
            else if (result==TYVOR_ERR_PAYLOAD || result==TYVOR_ERR_SESSION) ++p->stats.semantic_errors;
            else ++p->stats.frame_errors;
            discard(p,1);continue;
        }
        discard(p,want);++p->stats.frames;
        if (callback) callback(ctx,&f,completed);
    }
}
void tyvor_parser_poll(tyvor_parser_t *p,uint64_t now,tyvor_frame_fn cb,void *ctx) {
    if (p) drain(p,now,cb,ctx);
}
int tyvor_parser_feed(tyvor_parser_t *p,const uint8_t *bytes,size_t n,uint64_t now,
                      tyvor_frame_fn cb,void *ctx) {
    if (!p || (!bytes && n)) return -1;
    drain(p,now,cb,ctx);
    for (size_t i=0;i<n;++i) {
        if (p->used>=sizeof(p->raw)) {++p->stats.overflows;discard(p,1);}
        p->raw[p->used]=bytes[i];p->arrival_ms[p->used]=now;++p->used;
        drain(p,now,cb,ctx);
    }
    return 0;
}
void tyvor_parser_transport_error(tyvor_parser_t *p,int overflow) {
    if (!p) return;
    if (overflow) ++p->stats.overflows;else ++p->stats.frame_errors;
    p->used=0; /* Does not change link session or sequence history. */
}
