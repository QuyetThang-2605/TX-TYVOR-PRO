#include "tyvor_core_bridge.h"
#include <string.h>
static uint8_t read_request(void *ctx,uint8_t opcode,uint8_t page) {
    tyvor_core_bridge_t *b=ctx;(void)opcode;(void)page;
    ++b->read_requests;return 0; /* legacy read ACK, never a command */
}
static void read_packet(void *ctx,const tyvor_frame_t *f,uint64_t now) {
    tyvor_core_bridge_t *b=ctx;uint32_t token;
    if(tyvor_read_decode(f->payload,f->length,&token))return;
    if(b->session_count!=b->transport.link.stats.sessions) {
        b->session_count=b->transport.link.stats.sessions;b->read_token=0;b->response_active=0;
    }
    /* Bounded read-only snapshots, no repeated freeze from replay/flood. */
    if(token<=b->read_token || (b->read_token && (now<b->last_read_ms || now-b->last_read_ms<100))) {++b->read_rejected;return;}
    b->read_token=token;b->last_read_ms=now;b->response_active=1;b->part=0;b->mix_page=0;b->next_tx_ms=now;
    tyvor_core_snapshot(b->core,now,&b->frozen);b->frozen.generation=token;b->generation=token;
}
int tyvor_core_bridge_init(tyvor_core_bridge_t *b,const tyvor_core_t *core,uint64_t id,
                            tyvor_uart_write_fn fn,void *ctx,uint64_t now) {
    if(!b || !core || !core->initialized)return -1;
    memset(b,0,sizeof(*b));b->core=core;b->next_tx_ms=now;b->last_status_ms=now;
    int result=tyvor_bridge_init(&b->transport,fn,ctx,id,now);
    if(!result) {
        tyvor_bridge_set_read_service(&b->transport,read_request,b);
        tyvor_bridge_set_read_packet(&b->transport,read_packet,b);
    }
    return result;
}
int tyvor_core_bridge_feed(tyvor_core_bridge_t *b,const uint8_t *bytes,size_t n,uint64_t now) {
    return b?tyvor_bridge_receive(&b->transport,bytes,n,now):-1;
}
static void status(tyvor_core_bridge_t *b,uint64_t now) {
    tyvor_telemetry_view_t t;tyvor_telemetry_view(&b->core->telemetry,now,&t);
    tyvor_status_t s={0};s.flags=TYVOR_SOURCE_MOCK;
    if(t.valid&TYVOR_T_BATTERY) {s.flags|=TYVOR_VALID_BATTERY;s.battery_mv=t.battery_mv;}
    if(t.valid&TYVOR_T_LQ) {s.flags|=TYVOR_VALID_LQ;s.lq=t.lq;}
    if(tyvor_bridge_publish(&b->transport,&s))++b->tx_errors;
    else {b->last_status_success_ms=now;b->status_sent=1;}
    b->last_status_ms=now;
}
void tyvor_core_bridge_poll(tyvor_core_bridge_t *b,uint64_t now) {
    if(!b)return;
    tyvor_bridge_poll(&b->transport,now);tyvor_link_t *l=&b->transport.link;
    if(b->session_count!=l->stats.sessions) {
        b->session_count=l->stats.sessions;b->read_token=0;b->response_active=0;b->last_status_ms=now;
    }
    if(!l->ready || now<b->next_tx_ms)return;
    if(b->response_active && (now<b->frozen.captured_ms || now-b->frozen.captured_ms>=TYVOR_READ_TIMEOUT_MS))b->response_active=0;
    if(!b->response_active) {
        if(now>=b->last_status_ms && now-b->last_status_ms>=100)status(b,now);
        return;
    }
    b->next_tx_ms=now+10;
    if(!b->part) {status(b,now);b->part=1;return;}
    uint8_t payload[32];unsigned kind=b->part<=5?b->part:TYVOR_CORE_MIXES;
    b->frozen.mix_page=b->mix_page;
    size_t n=tyvor_core_wire_encode(&b->frozen,kind,now,payload,sizeof(payload));
    if(!n || tyvor_link_send(l,TYVOR_DIAGNOSTIC,payload,(uint8_t)n))++b->tx_errors;
    if(b->part<=5) {
        ++b->part;
        if(b->part==6 && !b->frozen.mix_count)b->response_active=0;
    } else if(++b->mix_page>=(b->frozen.mix_count+1u)/2u)b->response_active=0;
}
