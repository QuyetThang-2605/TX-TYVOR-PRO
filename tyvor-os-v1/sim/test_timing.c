#include "tyvor_async_tx.h"
#include "tyvor_timing.h"
#include "tyvor_sim_runtime.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d FAIL %s\n",__FILE__,__LINE__,#x);return 0;}}while(0)
typedef struct {
    tyvor_async_tx_t q;tyvor_sim_wire_t *wire;
    const uint8_t *bytes;uint16_t n;uint64_t started;
    unsigned starts,aborts,state_calls;int stall,start_fail,state_fail,done;
} driver_t;
static int start_tx(void *ctx,const uint8_t *b,uint16_t n) {
    driver_t *d=ctx;++d->starts;if(d->start_fail)return -1;
    d->bytes=b;d->n=n;d->started=d->q.now_ms;d->done=0;return 0;
}
static int state_tx(void *ctx) {
    driver_t *d=ctx;++d->state_calls;if(d->state_fail)return -1;if(d->stall)return 0;
    if(d->q.now_ms-d->started<6)return 0;
    if(!d->done && d->wire) {
        d->done=1;
        if(tyvor_bench_wire_write(d->wire,d->bytes,d->n)!=d->n)return -1;
    }
    return 1;
}
static void abort_tx(void *ctx) {driver_t *d=ctx;++d->aborts;d->bytes=NULL;}
static void init(driver_t *d,tyvor_sim_wire_t *wire) {
    memset(d,0,sizeof(*d));d->wire=wire;tyvor_async_tx_init(&d->q,start_tx,state_tx,abort_tx,d);
}
static size_t frame(uint8_t *out,unsigned type,unsigned seq) {
    tyvor_session_tag_t tag={1,2,1,1};uint8_t bytes[32]={0};unsigned n=32;
    if(type==TYVOR_STATUS) {n=6;bytes[5]=TYVOR_SOURCE_MOCK;}
    if(type==TYVOR_HELLO) {n=2;bytes[0]=TYVOR_ROLE_CONTROL;seq=0;}
    return tyvor_encode((uint8_t)type,(uint8_t)seq,&tag,bytes,(uint8_t)n,out,64);
}
static int queue_copy(void) {
    driver_t d;init(&d,NULL);uint8_t b[64],original[64];size_t n=frame(b,TYVOR_DIAGNOSTIC,1);memcpy(original,b,n);
    CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,0)==(int)n);memset(b,0,sizeof(b));CHECK(!d.starts);
    tyvor_async_tx_poll(&d.q,0);CHECK(d.q.busy && !memcmp(d.bytes,original,n));tyvor_async_tx_poll(&d.q,6);
    CHECK(d.q.completed==1 && !d.q.busy);return 1;
}
static int queue_full(void) {
    driver_t d;init(&d,NULL);uint8_t b[64];size_t n=frame(b,TYVOR_DIAGNOSTIC,0);
    for(unsigned i=0;i<4;++i)CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,0)==(int)n);
    CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,0)<0);CHECK(d.q.count==4 && d.q.full==1 && d.q.high_water==4 && !d.starts);return 1;
}
static int queue_status_coalesce(void) {
    driver_t d;init(&d,NULL);uint8_t b[64];size_t n=frame(b,TYVOR_STATUS,1);
    CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,0)>0);
    size_t x=frame(b,TYVOR_DIAGNOSTIC,1);CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)x,0)>0);
    n=frame(b,TYVOR_STATUS,2);CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,1)>0);
    CHECK(d.q.count==2 && d.q.coalesced==1 && d.q.pending[0].bytes[3]==TYVOR_DIAGNOSTIC && d.q.pending[1].bytes[4]==2);
    x=frame(b,TYVOR_DIAGNOSTIC,2);CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)x,1)>0);
    x=frame(b,TYVOR_DIAGNOSTIC,3);CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)x,1)>0);
    n=frame(b,TYVOR_STATUS,3);CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,2)>0);
    CHECK(d.q.count==4 && d.q.coalesced==2 && !d.q.full && d.q.pending[3].bytes[4]==3);return 1;
}
static int queue_active_immutable(void) {
    driver_t d;init(&d,NULL);uint8_t b[64],saved[64];size_t n=frame(b,TYVOR_STATUS,1);
    CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,0)>0);tyvor_async_tx_poll(&d.q,0);memcpy(saved,d.bytes,n);
    for(unsigned i=2;i<50;++i) {n=frame(b,TYVOR_STATUS,i);CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,1)>0);}
    CHECK(!memcmp(saved,d.bytes,n) && d.q.count==1 && d.q.coalesced==47);return 1;
}
static int queue_expiry(void) {
    driver_t d;init(&d,NULL);uint8_t b[64];size_t n=frame(b,TYVOR_DIAGNOSTIC,0);
    CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,0)>0);tyvor_async_tx_poll(&d.q,20);
    CHECK(d.q.expired==1 && !d.starts && !d.q.count);return 1;
}
static int queue_timeout(void) {
    driver_t d;init(&d,NULL);d.stall=1;uint8_t b[64];size_t n=frame(b,TYVOR_DIAGNOSTIC,0);
    CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,0)>0);tyvor_async_tx_poll(&d.q,0);tyvor_async_tx_poll(&d.q,19);CHECK(d.q.busy);
    tyvor_async_tx_poll(&d.q,20);CHECK(d.aborts==1 && d.q.timeouts==1 && !d.q.busy);
    d.stall=0;CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,21)>0);tyvor_async_tx_poll(&d.q,21);tyvor_async_tx_poll(&d.q,27);CHECK(d.q.completed==1);return 1;
}
static int queue_driver_errors(void) {
    driver_t d;init(&d,NULL);d.start_fail=1;uint8_t b[64];size_t n=frame(b,TYVOR_DIAGNOSTIC,0);
    CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,0)>0);tyvor_async_tx_poll(&d.q,0);CHECK(d.q.errors==1 && d.aborts==1);
    d.start_fail=0;d.state_fail=1;CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,1)>0);tyvor_async_tx_poll(&d.q,1);tyvor_async_tx_poll(&d.q,2);
    CHECK(d.q.errors==2 && d.aborts==2);return 1;
}
static int queue_clock(void) {
    driver_t d;init(&d,NULL);uint8_t b[64];size_t n=frame(b,TYVOR_DIAGNOSTIC,0);
    CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,100)>0);tyvor_async_tx_poll(&d.q,99);CHECK(d.q.expired==1);
    CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,100)>0);tyvor_async_tx_poll(&d.q,100);tyvor_async_tx_poll(&d.q,99);CHECK(d.q.timeouts==1);return 1;
}
static int queue_deadline_before_driver(void) {
    driver_t d;init(&d,NULL);uint8_t b[64];size_t n=frame(b,TYVOR_DIAGNOSTIC,0);
    CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,0)>0);tyvor_async_tx_poll(&d.q,0);
    tyvor_async_tx_poll(&d.q,20);
    CHECK(d.q.timeouts==1 && d.aborts==1 && d.state_calls==0);return 1;
}
static int queue_validation(void) {
    driver_t d;init(&d,NULL);uint8_t b[65];size_t n=frame(b,TYVOR_DIAGNOSTIC,0);CHECK(n==64);
    CHECK(tyvor_async_tx_write(&d.q,b,64,0)==64);CHECK(tyvor_async_tx_write(&d.q,b,65,0)<0);
    b[63]^=1;CHECK(tyvor_async_tx_write(&d.q,b,64,0)<0);CHECK(tyvor_async_tx_write(&d.q,NULL,0,0)<0);
    CHECK(d.q.invalid==3 && d.q.count==1);return 1;
}
static int queue_order(void) {
    driver_t d;init(&d,NULL);uint8_t b[64];
    for(unsigned i=0;i<4;++i) {size_t n=frame(b,TYVOR_DIAGNOSTIC,i);CHECK(tyvor_async_tx_write(&d.q,b,(uint16_t)n,i*6)>0);}
    for(unsigned i=0;i<4;++i) {tyvor_async_tx_poll(&d.q,i*6);CHECK(d.q.busy && d.bytes[4]==i);}
    tyvor_async_tx_poll(&d.q,24);CHECK(d.q.completed==4);return 1;
}
static int queued_write(void *ctx,const uint8_t *b,uint16_t n) {
    driver_t *d=ctx;return tyvor_async_tx_write(&d->q,b,n,d->wire->now);
}
static void bind(tyvor_sim_t *s,driver_t *c,driver_t *u) {
    tyvor_bench_transport_init(&s->control_adapter,queued_write,c,1);
    tyvor_bench_transport_init(&s->ui_adapter,queued_write,u,1);
}
static int advance(tyvor_sim_t *s,driver_t *c,driver_t *u,unsigned ms) {
    for(unsigned i=0;i<ms;++i) {
        tyvor_async_tx_poll(&c->q,s->now);tyvor_async_tx_poll(&u->q,s->now);
        if(tyvor_sim_advance(s,1))return -1;
        tyvor_async_tx_poll(&c->q,s->now);tyvor_async_tx_poll(&u->q,s->now);
        if(c->q.count>4 || u->q.count>4 || s->core.missed_ticks || tyvor_core_rf_enabled())return -1;
    }
    return 0;
}
static int integration(unsigned mode) {
    tyvor_sim_t s;driver_t c,u;CHECK(!tyvor_sim_init(&s));init(&c,&s.control_to_ui);init(&u,&s.ui_to_control);bind(&s,&c,&u);
    CHECK(!advance(&s,&c,&u,1000));CHECK(s.ui.state.valid && s.ui.state.mix_valid);
    uint32_t ticks=s.core.ticks;
    if(mode==1) {CHECK(!tyvor_sim_reset_control(&s));init(&c,&s.control_to_ui);bind(&s,&c,&u);}
    if(mode==2) {CHECK(!tyvor_sim_reset_ui(&s));init(&u,&s.ui_to_control);bind(&s,&c,&u);}
    if(mode==3) {CHECK(!tyvor_sim_reset_control(&s));CHECK(!tyvor_sim_reset_ui(&s));init(&c,&s.control_to_ui);init(&u,&s.ui_to_control);bind(&s,&c,&u);}
    if(mode==4) {
        c.stall=u.stall=1;uint8_t b[64];size_t n=frame(b,TYVOR_DIAGNOSTIC,1);
        for(unsigned ms=0;ms<600;++ms) {
            for(unsigned j=0;j<8;++j) {(void)queued_write(&c,b,(uint16_t)n);(void)queued_write(&u,b,(uint16_t)n);}
            CHECK(!advance(&s,&c,&u,1));
        }
        CHECK(c.q.full && u.q.full && c.q.timeouts && u.q.timeouts);c.stall=u.stall=0;
    }
    if(mode==5) {
        s.control_to_ui.hold=s.ui_to_control.hold=1;uint8_t b[64];size_t n=frame(b,TYVOR_DIAGNOSTIC,1);
        for(unsigned i=0;i<40;++i) {(void)tyvor_bench_wire_write(&s.control_to_ui,b,(uint16_t)n);(void)tyvor_bench_wire_write(&s.ui_to_control,b,(uint16_t)n);}
        CHECK(s.control_to_ui.overflows && s.ui_to_control.overflows);CHECK(!advance(&s,&c,&u,600));
        tyvor_bench_wire_clear(&s.control_to_ui);tyvor_bench_wire_clear(&s.ui_to_control);s.control_to_ui.hold=s.ui_to_control.hold=0;
        tyvor_bench_transport_error(&s.control_adapter,&s.bridge.transport.link,1);tyvor_bench_transport_error(&s.ui_adapter,&s.ui.link,1);
    }
    if(mode==6) {
        tyvor_ui_state_t old=s.ui.state;CHECK(!advance(&s,&c,&u,6000)); /* renderer never runs */
        tyvor_ui_state_poll(&old,s.now);CHECK(!old.valid && s.ui.state.valid);
        tyvor_ui_view_t view;CHECK(!tyvor_ui_core_view(0,&old,&view));CHECK(strstr(view.banner,"STALE"));
    }
    CHECK(!advance(&s,&c,&u,3000));CHECK(s.ui.state.valid && s.ui.state.mix_valid && s.bridge.transport.link.ready && s.ui.link.ready);
    CHECK(!s.core.missed_ticks && !s.core.faults && !s.ui.state.snapshot.rf_enabled);
    if(mode!=1 && mode!=3)CHECK(s.core.ticks>=ticks+300);
    return 1;
}
static int timing_wrap(void) {tyvor_timing_t t;tyvor_timing_init(&t,16000000,1,0);tyvor_timing_record(&t,TYVOR_TIME_CORE,UINT32_MAX-9,10);CHECK(t.section[TYVOR_TIME_CORE].max_cycles==20 && !t.hardware_measured);return 1;}
static int timing_disabled(void) {tyvor_timing_t t;tyvor_timing_init(&t,16000000,0,1);tyvor_timing_record(&t,TYVOR_TIME_CORE,0,100);CHECK(!t.section[TYVOR_TIME_CORE].samples && !t.hardware_measured);return 1;}
static int timing_budget(void) {tyvor_timing_t t;tyvor_timing_init(&t,16000000,1,0);tyvor_timing_record(&t,TYVOR_TIME_CORE,0,15999);tyvor_timing_record(&t,TYVOR_TIME_CORE,0,16000);tyvor_timing_record(&t,TYVOR_TIME_LOOP,0,160000);CHECK(t.section[TYVOR_TIME_CORE].over_budget==1 && t.section[TYVOR_TIME_LOOP].over_budget==1);return 1;}
static int timing_lateness(void) {tyvor_timing_t t;tyvor_timing_init(&t,1,1,0);tyvor_timing_lateness(&t,10,10);tyvor_timing_lateness(&t,12,10);tyvor_timing_lateness(&t,UINT64_MAX,0);CHECK(t.late_ticks==2 && t.max_lateness_ms==UINT32_MAX);return 1;}
static int queue_seed(void) {
    driver_t d;init(&d,NULL);uint32_t seed=0x2CF407;uint8_t b[64];
    for(unsigned i=0;i<10000;++i) {
        seed=seed*1664525u+1013904223u;d.stall=(seed&31u)==0;
        size_t n=frame(b,(seed&1)?TYVOR_STATUS:TYVOR_DIAGNOSTIC,i&255);
        (void)tyvor_async_tx_write(&d.q,b,(uint16_t)n,i);tyvor_async_tx_poll(&d.q,i);CHECK(d.q.count<=4);
    }
    d.stall=0;tyvor_async_tx_poll(&d.q,10030);CHECK(!d.q.busy && !d.q.count && d.q.full && d.q.coalesced);return 1;
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    struct {const char *name;int (*run)(void);} cases[]={
        {"tx_copy",queue_copy},{"tx_full",queue_full},{"tx_coalesce",queue_status_coalesce},{"tx_active_immutable",queue_active_immutable},
        {"tx_expiry",queue_expiry},{"tx_timeout",queue_timeout},{"tx_driver_errors",queue_driver_errors},{"tx_clock",queue_clock},
        {"tx_validation",queue_validation},{"tx_order",queue_order},{"tx_seed",queue_seed},{"tx_deadline_before_driver",queue_deadline_before_driver},
        {"timing_wrap",timing_wrap},{"timing_disabled",timing_disabled},{"timing_budget",timing_budget},{"timing_lateness",timing_lateness}};
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i)if(!strcmp(argv[1],cases[i].name))return cases[i].run()?0:1;
    const char *names[]={"async_duplex","async_reset_control","async_reset_ui","async_reset_both","async_tx_saturation","async_rx_saturation","async_slow_ui"};
    for(unsigned i=0;i<7;++i)if(!strcmp(argv[1],names[i]))return integration(i)?0:1;
    return 2;
}
