#include <stdio.h>
#include <string.h>
#include "tyvor_protocol.h"
#include "tyvor_status.h"
#if TYVOR_VERSION >= 2
#include "tyvor_link.h"
static int session_sender_reboot(void);
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d FAIL: %s\n",__FILE__,__LINE__,#x); return 0; } } while (0)
static unsigned received_count;
static uint64_t delivered_ms;
static void count_frame(void *ctx,const tyvor_frame_t *f,uint64_t now) {
    (void)ctx; (void)f; ++received_count; delivered_ms=now;
}
static size_t make_packet(uint8_t type,uint8_t seq,const uint8_t *b,uint8_t n,uint8_t *out) {
#if TYVOR_VERSION == 1
    return tyvor_encode(type,seq,b,n,out,TYVOR_FRAME_MAX);
#else
    const tyvor_session_tag_t tag={1,2,1,1};
    return tyvor_encode(type,seq,&tag,b,n,out,TYVOR_FRAME_MAX);
#endif
}
static size_t make_status(uint8_t seq,uint8_t *out) {
    const tyvor_status_t s={.arm_valid=0,.arm=0,.lq=99,.flags=7,.battery_mv=8100};
    uint8_t b[6]; if (tyvor_status_pack(&s,b,sizeof(b))) return 0;
    return make_packet(TYVOR_STATUS,seq,b,sizeof(b),out);
}
static int h1_timeout_suffix(void) {
    tyvor_parser_t p; uint8_t f[TYVOR_FRAME_MAX],outer[TYVOR_FRAME_MAX],payload[32]={0};
    size_t n=make_status(7,f); CHECK(n > 0);
    CHECK(make_packet(TYVOR_DIAGNOSTIC,0,payload,32,outer) == TYVOR_FRAME_MAX);
#if TYVOR_VERSION == 1
    size_t header=6;
#else
    size_t header=TYVOR_HEADER_LEN;
#endif
    tyvor_parser_init(&p,50); received_count=0;
    CHECK(tyvor_parser_feed(&p,outer,header,0,count_frame,NULL) == 0);
    CHECK(tyvor_parser_feed(&p,f,n/2,49,count_frame,NULL) == 0);
    tyvor_parser_poll(&p,50,count_frame,NULL);
    CHECK(tyvor_parser_feed(&p,f+n/2,n-n/2,51,count_frame,NULL) == 0);
    printf("H1 received=%u used=%zu timeouts=%u\n",received_count,p.used,p.stats.timeouts);
    CHECK(received_count == 1 && p.used == 0);
    return 1;
}
static int h2_sender_reboot(void) {
#if TYVOR_VERSION == 1
    tyvor_parser_t p; uint8_t f[TYVOR_FRAME_MAX]; tyvor_parser_init(&p,50); received_count=0;
    size_t n=make_status(128,f); CHECK(n > 0);
    CHECK(tyvor_parser_feed(&p,f,n,0,count_frame,NULL) == 0);
    received_count=0;
    for (unsigned seq=0;seq<129;++seq) {
        n=make_status((uint8_t)seq,f);
        CHECK(tyvor_parser_feed(&p,f,n,1000+100*seq,count_frame,NULL) == 0);
    }
    printf("H2 reboot rejected=%u duplicate=%u accepted=%u\n",p.stats.sequence_errors,p.stats.duplicates,received_count);
    CHECK(received_count > 0);
    return 1;
#else
    return session_sender_reboot();
#endif
}

#if TYVOR_VERSION >= 2
#include "test_transport.h"
static const tyvor_status_t reliability_mock={.arm_valid=0,.arm=0,.lq=99,.flags=7,.battery_mv=8100};
static int send_status(tyvor_link_t *l) {
    uint8_t b[6];if(tyvor_status_pack(&reliability_mock,b,sizeof(b)))return -1;
    return tyvor_link_send(l,TYVOR_STATUS,b,sizeof(b));
}
static int unconnected(test_pair_t *p,uint64_t aid,uint64_t bid,uint64_t now) {
    memset(p,0,sizeof(*p));
    return tyvor_link_init(&p->a,TYVOR_ROLE_CONTROL,aid,test_write,&p->ab,NULL,NULL,now) ||
           tyvor_link_init(&p->b,TYVOR_ROLE_UI,bid,test_write,&p->ba,NULL,NULL,now);
}
static int finish_handshake(test_pair_t *p,uint64_t start,uint64_t limit) {
    for(uint64_t now=start;now<=start+limit;now+=10) {
        test_pump(&p->a,&p->b,&p->ab,&p->ba,now);
        if(p->a.ready && p->b.ready && !p->a.pending.active && !p->b.pending.active &&
           p->a.peer_id==p->b.local_id && p->b.peer_id==p->a.local_id &&
           p->a.peer_epoch==p->b.local_epoch && p->b.peer_epoch==p->a.local_epoch)return 1;
    }
    return 0;
}
static int session_sender_reboot(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,100,200));
    p.a.tx_sequence[TYVOR_STATUS]=128;
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,10);
    CHECK(p.b.snapshot.valid && p.b.rx_sequence[TYVOR_STATUS]==128);
    CHECK(!tyvor_link_init(&p.a,TYVOR_ROLE_CONTROL,101,test_write,&p.ab,NULL,NULL,100));
    CHECK(finish_handshake(&p,100,900));
    CHECK(p.b.peer_id==101 && p.b.stats.sessions==2 && !p.b.snapshot.valid);
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,1000);
    printf("H2 v2 new_session=%u new_status_accepted=%u sequence=%u\n",
        p.b.stats.sessions,p.b.snapshot.valid,p.b.rx_sequence[TYVOR_STATUS]);
    CHECK(p.b.snapshot.valid && p.b.rx_sequence[TYVOR_STATUS]==0);return 1;
}
static int timeout_every_split(void) {
    uint8_t f[TYVOR_FRAME_MAX],outer[TYVOR_FRAME_MAX],b[32]={0};
    size_t n=make_status(7,f);CHECK(n>0);
    CHECK(make_packet(TYVOR_DIAGNOSTIC,0,b,32,outer)==TYVOR_FRAME_MAX);
    for(size_t cut=1;cut<n;++cut)for(unsigned edge=49;edge<=51;++edge) {
        tyvor_parser_t p;tyvor_parser_init(&p,50);received_count=0;
        CHECK(!tyvor_parser_feed(&p,outer,TYVOR_HEADER_LEN,0,count_frame,NULL));
        CHECK(!tyvor_parser_feed(&p,f,cut,edge,count_frame,NULL));
        tyvor_parser_poll(&p,50+edge-49,count_frame,NULL);
        CHECK(!tyvor_parser_feed(&p,f+cut,n-cut,edge+2,count_frame,NULL));
        CHECK(received_count==1 && !p.used && delivered_ms==edge+2);
    }
    return 1;
}
static int recovered_arrival_timestamp(void) {
    uint8_t outer[TYVOR_FRAME_MAX],inner[TYVOR_FRAME_MIN],payload[32]={0};
    CHECK(make_packet(TYVOR_DIAGNOSTIC,0,payload,32,outer)==TYVOR_FRAME_MAX);
    CHECK(make_packet(TYVOR_DIAGNOSTIC,1,NULL,0,inner)==TYVOR_FRAME_MIN);
    tyvor_parser_t p;tyvor_parser_init(&p,50);received_count=0;
    CHECK(!tyvor_parser_feed(&p,outer,TYVOR_HEADER_LEN,0,count_frame,NULL));
    CHECK(!tyvor_parser_feed(&p,inner,sizeof(inner),1,count_frame,NULL));CHECK(!received_count);
    tyvor_parser_poll(&p,500,count_frame,NULL);
    CHECK(received_count==1 && delivered_ms==1 && !p.used);return 1;
}
static int receiver_reboot(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,100,200));CHECK(!send_status(&p.a));
    test_deliver(&p.ab,&p.b,10);CHECK(p.b.snapshot.valid);
    CHECK(!tyvor_link_init(&p.b,TYVOR_ROLE_UI,201,test_write,&p.ba,NULL,NULL,100));
    CHECK(!p.b.snapshot.valid);
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,100);CHECK(!p.b.snapshot.valid);
    CHECK(finish_handshake(&p,100,900));CHECK(p.a.peer_id==201 && !p.b.snapshot.valid);
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,1000);
    CHECK(p.b.snapshot.valid && !p.b.rx_sequence[TYVOR_STATUS]);return 1;
}
static int both_reboot(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,100,200));
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,10);
    CHECK(!unconnected(&p,101,201,100));CHECK(!p.a.ready && !p.b.ready);
    CHECK(finish_handshake(&p,100,900));CHECK(!p.b.snapshot.valid);
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,1000);CHECK(p.b.snapshot.valid);return 1;
}
static int duplicate_hello_ack(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,1,2));p.a.tx_sequence[TYVOR_STATUS]=7;
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,5);
    uint8_t raw[TYVOR_FRAME_MAX],b[2]={TYVOR_ROLE_CONTROL,0};
    tyvor_session_tag_t tag={1,0,1,0};size_t n=tyvor_encode(TYVOR_HELLO,0,&tag,b,2,raw,sizeof(raw));CHECK(n);
    for(unsigned i=0;i<10;++i)CHECK(!tyvor_link_feed(&p.b,raw,n,10+i));
    tag.receiver_id=2;tag.receiver_epoch=1;
    n=tyvor_encode(TYVOR_SESSION_ACK,0,&tag,b,2,raw,sizeof(raw));CHECK(n);
    CHECK(!tyvor_link_feed(&p.b,raw,n,20));
    CHECK(p.b.stats.sessions==1 && p.b.snapshot.valid && p.b.snapshot.received_ms==5 && p.b.rx_sequence[1]==7);
    return 1;
}
static int lost_hello(void) {
    test_pair_t p;CHECK(!unconnected(&p,1,2,0));p.ab.drop_hello=1;p.ba.drop_hello=1;
    CHECK(finish_handshake(&p,0,990));CHECK(p.a.stats.sessions==1 && p.b.stats.sessions==1);return 1;
}
static int lost_session_ack(void) {
    test_pair_t p;CHECK(!unconnected(&p,1,2,0));p.ab.drop_ack=1;p.ba.drop_ack=1;
    CHECK(finish_handshake(&p,0,990));CHECK(!p.b.snapshot.valid);
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,1000);CHECK(p.b.snapshot.valid);return 1;
}
static int retry_budget(void) {
    test_pair_t p;CHECK(!unconnected(&p,1,2,0));p.ab.drop_all=1;p.ba.drop_all=1;
    for(unsigned now=0;now<1000;now+=10)test_pump(&p.a,&p.b,&p.ab,&p.ba,now);
    CHECK(!p.a.ready && !p.b.ready && p.a.pending.attempts==5 && p.a.stats.tx_frames==5);
    test_pump(&p.a,&p.b,&p.ab,&p.ba,1000);
    CHECK(!p.a.pending.active && p.a.stats.handshake_timeouts==1);
    uint32_t before=p.a.stats.tx_frames;
    test_pump(&p.a,&p.b,&p.ab,&p.ba,1999);CHECK(p.a.stats.tx_frames==before);
    p.ab.drop_all=0;p.ba.drop_all=0;CHECK(finish_handshake(&p,2000,990));return 1;
}
static int no_session_no_data(void) {
    test_pair_t p;CHECK(!unconnected(&p,1,2,0));uint8_t raw[TYVOR_FRAME_MAX];size_t n=make_status(0,raw);CHECK(n);
    CHECK(!tyvor_link_feed(&p.b,raw,n,0));CHECK(!p.b.ready && !p.b.snapshot.valid && p.b.stats.old_session==1);
    CHECK(send_status(&p.a)==TYVOR_ERR_SESSION);return 1;
}
static int delayed_old_packets(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,1,2));
    uint8_t data[TYVOR_FRAME_MAX],hello[TYVOR_FRAME_MAX],ack[TYVOR_FRAME_MAX],b[2]={1,0};
    CHECK(!send_status(&p.a));CHECK(p.ab.count==1);size_t dn=p.ab.size[0];memcpy(data,p.ab.packet[0],dn);
    test_deliver(&p.ab,&p.b,10);
    tyvor_session_tag_t tag={1,0,1,0};size_t hn=tyvor_encode(TYVOR_HELLO,0,&tag,b,2,hello,sizeof(hello));
    tag.receiver_id=2;tag.receiver_epoch=1;size_t an=tyvor_encode(TYVOR_SESSION_ACK,0,&tag,b,2,ack,sizeof(ack));CHECK(hn && an);
    CHECK(!tyvor_link_init(&p.a,1,3,test_write,&p.ab,NULL,NULL,100));
    CHECK(finish_handshake(&p,100,900));CHECK(!p.b.snapshot.valid);
    CHECK(!tyvor_link_feed(&p.b,data,dn,1000));CHECK(!p.b.snapshot.valid);
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,1001);CHECK(p.b.snapshot.valid);
    uint32_t epoch=p.b.local_epoch,sessions=p.b.stats.sessions;
    CHECK(!tyvor_link_feed(&p.b,hello,hn,1002));CHECK(!tyvor_link_feed(&p.b,ack,an,1003));
    CHECK(!tyvor_link_feed(&p.b,data,dn,1004));
    CHECK(p.b.peer_id==3 && p.b.local_epoch==epoch && p.b.stats.sessions==sessions);
    CHECK(p.b.snapshot.valid && p.b.snapshot.received_ms==1001);return 1;
}
static int sequence_does_not_change_session(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,1,2));p.a.tx_sequence[1]=128;
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,10);
    p.a.tx_sequence[1]=0;CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,11);
    CHECK(p.b.stats.sessions==1 && p.b.rx_sequence[1]==128 && p.b.snapshot.received_ms==10);
    CHECK(p.b.parser.stats.sequence_errors==1);return 1;
}
static int loss_window_rehandshake(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,1,2));CHECK(!send_status(&p.a));
    test_deliver(&p.ab,&p.b,1);p.a.tx_sequence[1]=129;
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,10);
    CHECK(p.b.parser.stats.sequence_errors==1 && p.b.snapshot.received_ms==1);
    CHECK(finish_handshake(&p,2100,900));
    CHECK(p.a.local_epoch>1 && p.b.local_epoch>1 && !p.b.snapshot.valid);
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,3100);
    CHECK(p.b.snapshot.valid && !p.b.rx_sequence[1]);return 1;
}
static int identity_collision(void) {
    test_pair_t p;CHECK(!unconnected(&p,7,7,0));
    CHECK(!finish_handshake(&p,0,990));
    CHECK(!p.a.ready && !p.b.ready && p.a.stats.collisions && p.b.stats.collisions);
    CHECK(!p.b.snapshot.valid);return 1;
}
static int candidate_collision(void) {
    test_pair_t p;CHECK(!unconnected(&p,1,2,0));
    uint8_t raw[TYVOR_FRAME_MAX],b[2]={1,0};tyvor_session_tag_t tag={1,0,1,0};
    size_t n=tyvor_encode(TYVOR_HELLO,0,&tag,b,2,raw,sizeof(raw));CHECK(n);
    CHECK(!tyvor_link_feed(&p.b,raw,n,0));CHECK(p.b.pending.peer_id==1);
    tag.sender_id=3;tag.receiver_id=2;tag.receiver_epoch=1;
    n=tyvor_encode(TYVOR_SESSION_ACK,0,&tag,b,2,raw,sizeof(raw));CHECK(n);
    CHECK(!tyvor_link_feed(&p.b,raw,n,1));
    CHECK(!p.b.ready && p.b.stats.collisions==1 && p.b.pending.peer_id==1);return 1;
}
static int late_ack(void) {
    test_pair_t p;CHECK(!unconnected(&p,1,2,0));uint8_t b[2]={1,0},raw[TYVOR_FRAME_MAX];
    tyvor_session_tag_t tag={1,2,1,1};
    size_t n=tyvor_encode(TYVOR_SESSION_ACK,0,&tag,b,2,raw,sizeof(raw));CHECK(n);
    CHECK(!tyvor_link_feed(&p.b,raw,n,1000));CHECK(!p.b.ready);
    tyvor_link_poll(&p.b,1000);CHECK(p.b.stats.handshake_timeouts==1);return 1;
}
static int stale_after_session_change(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,1,2));CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,5);
    CHECK(p.b.snapshot.valid);
    CHECK(!tyvor_link_init(&p.a,1,3,test_write,&p.ab,NULL,NULL,100));
    CHECK(finish_handshake(&p,100,900));
    CHECK(!p.b.snapshot.valid && !p.b.snapshot.status.flags && !p.b.snapshot.status.battery_mv);
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,1000);CHECK(p.b.snapshot.valid);
    tyvor_link_poll(&p.b,3000);CHECK(!p.b.snapshot.valid && !p.b.snapshot.status.flags);return 1;
}
static int protocol_version_compatibility(void) {
    const uint8_t v1[]={0x54,0x59,1,1,7,6,0,0,0xa4,0x1f,0x63,7,0x29,0x62};
    test_pair_t p;CHECK(!unconnected(&p,1,2,0));
    CHECK(!tyvor_link_feed(&p.b,v1,sizeof(v1),0));CHECK(!p.b.ready && p.b.parser.stats.frame_errors);
    CHECK(finish_handshake(&p,0,990));CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,1000);
    CHECK(p.b.snapshot.valid);return 1;
}
static int tag_crc_and_schema(void) {
    uint8_t raw[TYVOR_FRAME_MAX];size_t n=make_status(1,raw);tyvor_frame_t f;CHECK(n);
    for(size_t i=6;i<TYVOR_HEADER_LEN;++i){raw[i]^=1;CHECK(tyvor_decode(raw,n,&f)==TYVOR_ERR_CRC);raw[i]^=1;}
    tyvor_session_tag_t tag={0,2,1,1};uint8_t b[2]={1,0};
    CHECK(!tyvor_encode(TYVOR_HELLO,0,&tag,b,2,raw,sizeof(raw)));
    tag.sender_id=1;tag.receiver_id=0;tag.receiver_epoch=0;
    n=tyvor_encode(TYVOR_HELLO,0,&tag,b,2,raw,sizeof(raw));CHECK(n);
    raw[TYVOR_HEADER_LEN+1]=1;uint16_t c=tyvor_crc16(raw+2,n-4);raw[n-2]=(uint8_t)c;raw[n-1]=(uint8_t)(c>>8);
    CHECK(tyvor_decode(raw,n,&f)==TYVOR_ERR_PAYLOAD);return 1;
}
static int direction_authority(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,1,2));uint8_t status[6],req[2]={1,0};
    CHECK(!tyvor_status_pack(&reliability_mock,status,6));
    CHECK(tyvor_link_send(&p.b,TYVOR_STATUS,status,6)==TYVOR_ERR_PAYLOAD);
    CHECK(tyvor_link_send(&p.a,TYVOR_UI_REQUEST,req,2)==TYVOR_ERR_PAYLOAD);
    uint8_t raw[TYVOR_FRAME_MAX];tyvor_session_tag_t tag={2,1,1,1};
    size_t n=tyvor_encode(TYVOR_STATUS,0,&tag,status,6,raw,sizeof(raw));CHECK(n);
    CHECK(!tyvor_link_feed(&p.a,raw,n,10));CHECK(p.a.stats.wrong_direction==1 && !p.a.snapshot.valid);
    return 1;
}
static int invalid_identity_epoch_limit(void) {
    tyvor_link_t l;memset(&l,0xff,sizeof(l));
    CHECK(tyvor_link_init(&l,1,0,NULL,NULL,NULL,NULL,0)==-1 && !l.ready && !l.local_id);
    CHECK(tyvor_link_init(&l,0,1,NULL,NULL,NULL,NULL,0)==-1 && !l.local_id);
    CHECK(!tyvor_link_init(&l,1,1,NULL,NULL,NULL,NULL,0));
    l.issued_epoch=UINT32_MAX;l.pending.active=0;tyvor_link_poll(&l,0);
    CHECK(!l.pending.active && !l.ready && l.issued_epoch==UINT32_MAX && l.stats.epoch_exhausted==1);
    tyvor_link_poll(&l,1);CHECK(l.stats.epoch_exhausted==1);return 1;
}
static int link_clock_regression(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,1,2));CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,100);
    tyvor_link_poll(&p.b,100);CHECK(p.b.snapshot.valid);
    tyvor_link_poll(&p.b,99);
    CHECK(!p.b.ready && !p.b.snapshot.valid && p.b.pending.local_epoch>1);return 1;
}
static uint32_t random_next(uint32_t *s) {*s=*s*1664525u+1013904223u;return *s;}
static int randomized_uart_seed(void) {
    const uint32_t seeds[]={0x13579bdfu,0x2468ace0u,0xc0ffeeu};
    for(unsigned run=0;run<3;++run) {
        uint32_t rng=seeds[run];tyvor_parser_t p;tyvor_parser_init(&p,50);received_count=0;
        uint8_t f[TYVOR_FRAME_MAX];size_t n=make_status(7,f);CHECK(n);
        unsigned expected=0;uint64_t clock=0;
        for(unsigned iteration=0;iteration<2000;++iteration) {
            unsigned noise=random_next(&rng)%65;
            for(unsigned i=0;i<noise;++i) {
                uint8_t byte=(uint8_t)(random_next(&rng)>>24);
                CHECK(!tyvor_parser_feed(&p,&byte,1,clock,count_frame,NULL));
                CHECK(p.used<=TYVOR_FRAME_MAX);
            }
            /* Noise cannot consume a following frame after its own deadline. */
            clock+=100;tyvor_parser_poll(&p,clock,count_frame,NULL);
            size_t cut=random_next(&rng)%n;
            CHECK(!tyvor_parser_feed(&p,f,cut,clock,count_frame,NULL));
            clock+=random_next(&rng)%10;
            CHECK(!tyvor_parser_feed(&p,f+cut,n-cut,clock,count_frame,NULL));
            ++expected;CHECK(received_count==expected && p.used==0);
            if(iteration%7==0) {
                uint8_t corrupt[TYVOR_FRAME_MAX];memcpy(corrupt,f,n);corrupt[n-1]^=1;
                CHECK(!tyvor_parser_feed(&p,corrupt,n,++clock,count_frame,NULL));
                clock+=100;tyvor_parser_poll(&p,clock,count_frame,NULL);
                CHECK(received_count==expected && p.used<=TYVOR_FRAME_MAX);
            }
            ++clock;
        }
        printf("UART seed=%08x recovered=%u crc_errors=%u\n",seeds[run],expected,p.stats.crc_errors);
    }
    return 1;
}

static int reboot_during_idle_probe(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,1,2));
    tyvor_link_poll(&p.a,2100);
    CHECK(p.a.pending.active && p.a.pending.peer_id==2);
    CHECK(!tyvor_link_init(&p.b,2,3,test_write,&p.ba,NULL,NULL,2100));
    CHECK(finish_handshake(&p,2100,900));
    CHECK(p.a.peer_id==3 && p.b.peer_id==1 && !p.b.snapshot.valid);
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,3100);CHECK(p.b.snapshot.valid);return 1;
}
static int control_tx_failure_recovery(void) {
    test_pair_t p;CHECK(!unconnected(&p,1,2,0));p.ab.mode=1;p.ba.mode=2;
    test_pump(&p.a,&p.b,&p.ab,&p.ba,0);
    CHECK(!p.a.ready && !p.b.ready && p.a.stats.tx_errors && p.b.stats.tx_partial);
    test_pump(&p.a,&p.b,&p.ab,&p.ba,60);
    p.ab.mode=0;p.ba.mode=0;
    CHECK(finish_handshake(&p,100,890));CHECK(!p.b.snapshot.valid);return 1;
}
static int wrong_ack_epoch(void) {
    test_pair_t p;CHECK(!unconnected(&p,1,2,0));uint8_t raw[TYVOR_FRAME_MAX],b[2]={1,0};
    tyvor_session_tag_t tag={1,2,1,2};
    size_t n=tyvor_encode(TYVOR_SESSION_ACK,0,&tag,b,2,raw,sizeof(raw));CHECK(n);
    CHECK(!tyvor_link_feed(&p.b,raw,n,10));CHECK(!p.b.ready && p.b.stats.old_session==1);
    CHECK(finish_handshake(&p,10,980));return 1;
}
static int ack_before_offer(void) {
    test_pair_t p;CHECK(!unconnected(&p,1,2,0));uint8_t raw[TYVOR_FRAME_MAX],b[2]={1,0};
    tyvor_session_tag_t tag={1,2,1,1};
    size_t n=tyvor_encode(TYVOR_SESSION_ACK,0,&tag,b,2,raw,sizeof(raw));CHECK(n);
    CHECK(!tyvor_link_feed(&p.b,raw,n,0));
    CHECK(!p.b.ready && !p.b.snapshot.valid && p.b.stats.control_dropped==1);
    CHECK(finish_handshake(&p,0,990));return 1;
}
static int retired_epoch_hello(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,1,2));CHECK(finish_handshake(&p,2100,900));
    CHECK(p.b.peer_epoch>1);CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,3100);
    uint8_t raw[TYVOR_FRAME_MAX],b[2]={1,0};tyvor_session_tag_t tag={1,0,1,0};
    size_t n=tyvor_encode(TYVOR_HELLO,0,&tag,b,2,raw,sizeof(raw));CHECK(n);
    CHECK(!tyvor_link_feed(&p.b,raw,n,3101));
    CHECK(!p.b.pending.active && p.b.snapshot.valid && p.b.snapshot.received_ms==3100 && p.b.stats.old_session);return 1;
}
static int status_stale_with_ack_traffic(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,1,2));CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,0);
    p.a.tx_sequence[1]=129;CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,10);
    const uint8_t req[2]={1,0},ack[2]={1,1};
    for(unsigned now=1000;now<=2000;now+=1000) {
        CHECK(!tyvor_link_send(&p.b,TYVOR_UI_REQUEST,req,2));test_deliver(&p.ba,&p.a,now);
        CHECK(!tyvor_link_send(&p.a,TYVOR_ACK,ack,2));test_deliver(&p.ab,&p.b,now);
    }
    CHECK(p.a.last_peer_ms==2000 && p.b.last_peer_ms==2000);
    CHECK(finish_handshake(&p,2100,900));
    CHECK(p.a.local_epoch>1 && p.b.local_epoch>1 && !p.b.snapshot.valid);
    CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,3100);CHECK(p.b.snapshot.valid);return 1;
}

static int randomized_reboots_seed(void) {
    test_pair_t p;CHECK(!test_pair_init(&p,100,200));uint32_t rng=0x5eed1234u;
    uint64_t now=0,aid=100,bid=200;
    for(unsigned i=0;i<80;++i) {
        now+=100;
        unsigned choice=random_next(&rng)%3;
        if(choice!=1) {aid+=1000;CHECK(!tyvor_link_init(&p.a,1,aid,test_write,&p.ab,NULL,NULL,now));}
        if(choice!=0) {bid+=1000;CHECK(!tyvor_link_init(&p.b,2,bid,test_write,&p.ba,NULL,NULL,now));}
        p.ab.drop_ack=random_next(&rng)%2;p.ba.drop_ack=random_next(&rng)%2;
        int complete=finish_handshake(&p,now,1900);
        if(!complete)printf("reboot iteration=%u choice=%u A ready=%u peer=%llu pending=%u candidate=%llu epoch=%u attempts=%u B ready=%u peer=%llu pending=%u candidate=%llu epoch=%u attempts=%u\n",
            i,choice,p.a.ready,(unsigned long long)p.a.peer_id,p.a.pending.active,(unsigned long long)p.a.pending.peer_id,p.a.pending.local_epoch,p.a.pending.attempts,
            p.b.ready,(unsigned long long)p.b.peer_id,p.b.pending.active,(unsigned long long)p.b.pending.peer_id,p.b.pending.local_epoch,p.b.pending.attempts);
        CHECK(complete);now+=1900;
        CHECK(!p.b.snapshot.valid);CHECK(!send_status(&p.a));test_deliver(&p.ab,&p.b,now);
        CHECK(p.b.snapshot.valid && p.b.peer_id==aid && p.a.peer_id==bid);
    }
    puts("REBOOT seed=5eed1234 iterations=80 PASS");return 1;
}
#endif

struct reliability_case { const char *name; int (*run)(void); };
static const struct reliability_case cases[]={
    {"h1_timeout_suffix",h1_timeout_suffix},{"h2_sender_reboot",h2_sender_reboot}
#if TYVOR_VERSION >= 2
#define CASE(x) ,{#x,x}
    CASE(timeout_every_split) CASE(recovered_arrival_timestamp)
    CASE(receiver_reboot) CASE(both_reboot) CASE(duplicate_hello_ack)
    CASE(lost_hello) CASE(lost_session_ack) CASE(retry_budget) CASE(no_session_no_data)
    CASE(delayed_old_packets) CASE(sequence_does_not_change_session) CASE(loss_window_rehandshake)
    CASE(identity_collision) CASE(candidate_collision) CASE(late_ack)
    CASE(stale_after_session_change) CASE(protocol_version_compatibility) CASE(tag_crc_and_schema)
    CASE(direction_authority) CASE(invalid_identity_epoch_limit) CASE(link_clock_regression)
    CASE(randomized_uart_seed) CASE(randomized_reboots_seed)
    CASE(reboot_during_idle_probe) CASE(control_tx_failure_recovery) CASE(wrong_ack_epoch) CASE(retired_epoch_hello)
    CASE(ack_before_offer)
    CASE(status_stale_with_ack_traffic)
#endif
};
int main(int argc,char **argv) {
    unsigned passed=0,failed=0;
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        if (argc == 2 && strcmp(argv[1],cases[i].name)) continue;
        int ok=cases[i].run(); printf("%s %s\n",ok?"PASS":"FAIL",cases[i].name);
        if(ok)++passed;else++failed;
    }
    printf("%u PASS / %u FAIL; protocol=%u\n",passed,failed,TYVOR_VERSION);
    return failed || !passed ? 1:0;
}
