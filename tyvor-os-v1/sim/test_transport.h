#ifndef TYVOR_TEST_TRANSPORT_H
#define TYVOR_TEST_TRANSPORT_H
#include "tyvor_link.h"
#include <string.h>
#define TEST_WIRE_CAP 32u
typedef struct {
    uint8_t packet[TEST_WIRE_CAP][TYVOR_FRAME_MAX];
    uint16_t size[TEST_WIRE_CAP];
    unsigned count,mode,drop_ack,drop_hello,drop_all;
} test_wire_t;
/* Modes: 0 full, 1 driver failure, 2 transmitted prefix only. Drop simulates
   wire loss after a successful driver copy. No implementation logic mocked. */
static int test_write(void *ctx,const uint8_t *bytes,uint16_t n) {
    test_wire_t *w=ctx;
    if (w->mode==1) return -1;
    if (w->drop_all) return n;
    unsigned *drop=bytes[3]==TYVOR_SESSION_ACK?&w->drop_ack:
                   bytes[3]==TYVOR_HELLO?&w->drop_hello:NULL;
    if (drop && *drop) {--*drop;return n;}
    if (w->count==TEST_WIRE_CAP || n>TYVOR_FRAME_MAX) return -1;
    uint16_t sent=w->mode==2?(uint16_t)(n-1):n;
    memcpy(w->packet[w->count],bytes,sent);w->size[w->count++]=sent;
    return sent;
}
static void test_deliver(test_wire_t *w,tyvor_link_t *to,uint64_t now) {
    unsigned count=w->count;w->count=0;
    for (unsigned i=0;i<count;++i) (void)tyvor_link_feed(to,w->packet[i],w->size[i],now);
}
static void test_pump(tyvor_link_t *a,tyvor_link_t *b,test_wire_t *ab,test_wire_t *ba,uint64_t now) {
    for (unsigned i=0;i<8;++i) {
        tyvor_link_poll(a,now);tyvor_link_poll(b,now);
        test_deliver(ab,b,now);test_deliver(ba,a,now);
    }
}
typedef struct {tyvor_link_t a,b;test_wire_t ab,ba;} test_pair_t;
static int test_pair_init(test_pair_t *p,uint64_t a_id,uint64_t b_id) {
    memset(p,0,sizeof(*p));
    if (tyvor_link_init(&p->a,TYVOR_ROLE_CONTROL,a_id,test_write,&p->ab,NULL,NULL,0) ||
        tyvor_link_init(&p->b,TYVOR_ROLE_UI,b_id,test_write,&p->ba,NULL,NULL,0)) return -1;
    test_pump(&p->a,&p->b,&p->ab,&p->ba,0);
    return p->a.ready && p->b.ready?0:-1;
}
#endif
