#include "tyvor_sim_runtime.h"
#include "tyvor_diagnostics.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d FAIL: %s\n",__FILE__,__LINE__,#x);return 0;}}while(0)
static int start(tyvor_sim_t *s) {return tyvor_sim_init(s) || tyvor_sim_advance(s,300) || !s->ui.state.valid?-1:0;}
static int recover(tyvor_sim_t *s) {
    s->control_to_ui.drop=s->ui_to_control.drop=0;s->control_to_ui.hold=s->ui_to_control.hold=0;
    s->control_to_ui.partial=s->ui_to_control.partial=0;s->control_to_ui.fail=s->ui_to_control.fail=0;
    s->control_to_ui.loss_every=s->ui_to_control.loss_every=0;
    s->control_to_ui.corrupt_every=s->ui_to_control.corrupt_every=0;
    s->control_to_ui.duplicate_every=s->ui_to_control.duplicate_every=0;
    s->control_to_ui.delay_ms=s->ui_to_control.delay_ms=0;s->control_to_ui.jitter_ms=s->ui_to_control.jitter_ms=0;
    s->control_to_ui.chunk_size=s->ui_to_control.chunk_size=7;s->control_to_ui.byte_gap_ms=s->ui_to_control.byte_gap_ms=0;
    tyvor_bench_wire_clear(&s->control_to_ui);tyvor_bench_wire_clear(&s->ui_to_control);
    tyvor_link_transport_error(&s->bridge.transport.link,1);tyvor_link_transport_error(&s->ui.link,1);
    return tyvor_sim_advance(s,2500) || !s->ui.state.valid?-1:0;
}
static int bench_bidirectional(void) {
    tyvor_sim_t s;CHECK(!start(&s));CHECK(s.control_adapter.tx_frames && s.control_adapter.rx_chunks && s.ui_adapter.tx_frames && s.ui_adapter.rx_chunks);
    CHECK(s.ui.state.valid && s.ui.state.mix_valid && s.ui.state.mix_count==4 && s.ui.state.mixes[0].weight==100);
    CHECK(!tyvor_ui_endpoint_request(&s.ui,TYVOR_REQ_PAGE,3));CHECK(!tyvor_sim_advance(&s,1));CHECK(s.ui.state.ack_valid);return 1;
}
static int bench_loss(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.control_to_ui.loss_every=7;s.ui_to_control.loss_every=9;
    CHECK(!tyvor_sim_advance(&s,2000));CHECK(s.control_to_ui.dropped && s.ui_to_control.dropped);CHECK(!recover(&s));return 1;
}
static int bench_delay(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.control_to_ui.delay_ms=20;s.ui_to_control.delay_ms=15;
    s.control_to_ui.jitter_ms=10;s.ui_to_control.jitter_ms=10;s.control_to_ui.byte_gap_ms=1;s.ui_to_control.byte_gap_ms=1;
    CHECK(!tyvor_sim_advance(&s,2500));CHECK(s.ui.state.valid && s.ui.state.snapshot.telemetry.valid==15);
    CHECK(s.ui.state.expires_ms<=s.ui.read_started_ms+TYVOR_READ_TIMEOUT_MS);return 1;
}
static int bench_corruption(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.control_to_ui.corrupt_every=5;s.ui_to_control.corrupt_every=7;
    CHECK(!tyvor_sim_advance(&s,2000));CHECK(s.control_to_ui.corrupted && s.ui_to_control.corrupted);
    CHECK(s.ui.link.parser.stats.crc_errors && s.bridge.transport.link.parser.stats.crc_errors);CHECK(!recover(&s));return 1;
}
static int bench_duplication(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.control_to_ui.duplicate_every=2;s.ui_to_control.duplicate_every=3;
    CHECK(!tyvor_sim_advance(&s,2000));CHECK(s.control_to_ui.duplicated && s.ui_to_control.duplicated && s.ui.state.valid);
    CHECK(s.ui.link.parser.stats.duplicates && s.bridge.transport.link.parser.stats.duplicates);return 1;
}
static int bench_reconnect(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.control_to_ui.drop=s.ui_to_control.drop=1;
    CHECK(!tyvor_sim_advance(&s,2200));CHECK(!s.ui.state.valid && s.ui.state.disconnected);
    CHECK(!recover(&s));CHECK(!s.ui.state.disconnected && !tyvor_core_rf_enabled());return 1;
}
static int bench_reset_stm32(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.switches[0]=1;CHECK(!tyvor_sim_advance(&s,100));CHECK(s.core.safety.state==TYVOR_BENCH_ARMED);
    uint64_t old=s.ui.link.peer_id;CHECK(!tyvor_sim_reset_control(&s));CHECK(s.core.safety.state==TYVOR_STARTUP && s.core.channels[2]==-1000);
    CHECK(!tyvor_sim_advance(&s,600));CHECK(s.ui.link.peer_id!=old && s.ui.state.valid && s.core.safety.state==TYVOR_STARTUP && s.control_resets==2);return 1;
}
static int bench_reset_esp32(void) {
    tyvor_sim_t s;CHECK(!start(&s));tyvor_core_t before=s.core;CHECK(!tyvor_sim_reset_ui(&s));
    CHECK(!s.ui.state.valid && !memcmp(&before,&s.core,sizeof(before)));CHECK(!tyvor_sim_advance(&s,600));
    CHECK(s.ui.state.valid && s.ui_resets==2);return 1;
}
static int bench_reset_both(void) {
    tyvor_sim_t s;CHECK(!start(&s));CHECK(!tyvor_sim_reset_control(&s));CHECK(!tyvor_sim_reset_ui(&s));
    CHECK(!s.ui.state.valid && s.core.safety.state==TYVOR_STARTUP);CHECK(!tyvor_sim_advance(&s,600));
    CHECK(s.ui.state.valid && s.control_resets==2 && s.ui_resets==2 && s.core.safety.state==TYVOR_DISARMED);return 1;
}
static int bench_session_collision(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.control_boot=s.ui.link.local_id-1;CHECK(!tyvor_sim_reset_control(&s));
    CHECK(!tyvor_sim_advance(&s,1300));CHECK(!s.ui.state.valid && (s.ui.link.stats.collisions || s.bridge.transport.link.stats.collisions));
    s.control_boot=9000;s.ui_boot=10000;CHECK(!tyvor_sim_reset_control(&s));CHECK(!tyvor_sim_reset_ui(&s));
    CHECK(!tyvor_sim_advance(&s,600) && s.ui.state.valid);return 1;
}
static int bench_stale_cache(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.control_to_ui.drop=1;CHECK(!tyvor_sim_advance(&s,600));
    CHECK(!s.ui.state.valid && !s.ui.state.snapshot.model_id && !s.ui.state.snapshot.telemetry.valid);
    tyvor_ui_view_t v;CHECK(!tyvor_ui_core_view(5,&s.ui.state,&v));CHECK(!strstr(v.rows[0],"8100"));return 1;
}
static int bench_long_delay_rejected(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.control_to_ui.delay_ms=1400;CHECK(!tyvor_sim_advance(&s,1800));
    CHECK(!s.ui.state.valid && s.ui.lease_rejected);CHECK(!recover(&s));return 1;
}
static int bench_delay_within_lease(void) {
    tyvor_sim_t s;CHECK(!start(&s));uint32_t commits=s.ui.state.commits;
    s.control_to_ui.delay_ms=250;s.ui_to_control.delay_ms=30;
    CHECK(!tyvor_sim_advance(&s,2200));CHECK(s.ui.state.commits>commits+2 && s.ui.lease_rejected<10);
    CHECK(!recover(&s));return 1;
}
static int bench_telemetry_age_bound(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.telemetry_loss=1;
    uint64_t expires=s.core.telemetry.sampled_ms[0]+1000;
    s.control_to_ui.delay_ms=150;s.ui_to_control.delay_ms=20;
    uint32_t commits=s.ui.state.commits;
    for(unsigned tick=0;tick<1600;++tick) {
        CHECK(!tyvor_sim_advance(&s,1));
        if(s.now>=expires)CHECK(!(s.ui.state.snapshot.telemetry.valid&1));
    }
    CHECK(s.ui.state.commits>commits && !(s.ui.state.snapshot.telemetry.valid&1));return 1;
}
static int bench_old_packets(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.control_to_ui.hold=1;CHECK(!tyvor_sim_advance(&s,200));CHECK(s.control_to_ui.count);
    CHECK(!tyvor_sim_reset_ui(&s));s.control_to_ui.hold=0;CHECK(!tyvor_sim_advance(&s,1));CHECK(!s.ui.state.valid);
    CHECK(!tyvor_sim_advance(&s,800) && s.ui.state.valid && s.ui.link.stats.old_session);return 1;
}
static int bench_partial_tx(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.control_to_ui.partial=1;s.ui_to_control.partial=1;
    CHECK(!tyvor_sim_advance(&s,700));CHECK(!s.ui.state.valid && s.control_adapter.tx_partial && s.ui_adapter.tx_partial);
    CHECK(!recover(&s));return 1;
}
static int bench_interbyte_timeout(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.control_to_ui.chunk_size=1;s.control_to_ui.byte_gap_ms=5;
    CHECK(!tyvor_sim_advance(&s,1000));CHECK(!s.ui.state.valid && s.ui.link.parser.stats.timeouts);CHECK(!recover(&s));return 1;
}
static int bench_buffer_bound(void) {
    tyvor_sim_wire_t w={0};uint8_t b[64]={1},out[66];memset(out,0xa5,sizeof(out));
    for(unsigned i=0;i<TYVOR_SIM_WIRE_CAP;++i)CHECK(tyvor_bench_wire_write(&w,b,64)==64);
    CHECK(tyvor_bench_wire_write(&w,b,64)<0 && w.count==TYVOR_SIM_WIRE_CAP && w.overflows==1);
    CHECK(tyvor_bench_wire_take(&w,0,out+1)==64 && out[0]==0xa5 && out[65]==0xa5);
    w.jitter_ms=UINT32_MAX;CHECK(tyvor_bench_wire_write(&w,b,64)<0);
    tyvor_bench_wire_clear(&w);CHECK(!w.count);return 1;
}
static unsigned backend_calls;
static int spy(void *ctx,const uint8_t *b,uint16_t n) {(void)ctx;(void)b;++backend_calls;return n;}
static int spy_feed(void *ctx,const uint8_t *b,size_t n,uint64_t now) {(void)ctx;(void)b;(void)n;(void)now;++backend_calls;return 0;}
static int bench_disabled_gate(void) {
    tyvor_bench_transport_t t;uint8_t b[64]={0};backend_calls=0;tyvor_bench_transport_init(&t,spy,NULL,0);
    CHECK(tyvor_bench_transport_write(&t,b,64)<0 && !backend_calls && t.suppressed==1);
    CHECK(tyvor_bench_transport_receive(&t,b,64,0,spy_feed,NULL)<0 && !backend_calls && t.suppressed==2);
    CHECK(tyvor_bench_transport_write(&t,b,65)<0 && !backend_calls);return 1;
}
static int bench_lease_exhaustion(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.ui.read_token=UINT32_MAX;s.ui.read_active=0;
    CHECK(!tyvor_sim_advance(&s,700));CHECK(!s.ui.state.valid && s.ui.read_token==UINT32_MAX);return 1;
}
static int bench_read_replay(void) {
    tyvor_sim_t s;CHECK(!start(&s));tyvor_core_t before=s.core;uint8_t payload[7];CHECK(tyvor_read_encode(s.bridge.read_token,payload)==7);
    CHECK(!tyvor_link_send(&s.ui.link,TYVOR_DIAGNOSTIC,payload,7));
    tyvor_sim_wire_t *w=&s.ui_to_control;
    for(unsigned i=0;i<w->count;++i)CHECK(!tyvor_core_bridge_feed(&s.bridge,w->bytes[i],w->length[i],s.now));w->count=0;
    CHECK(s.bridge.read_rejected && !memcmp(&before,&s.core,sizeof(before)));return 1;
}
static int bench_read_before_offer(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.ui.read_offered=0;s.ui.state.valid=0;
    CHECK(!tyvor_sim_advance(&s,80));CHECK(!s.ui.state.valid);CHECK(!tyvor_sim_advance(&s,400) && s.ui.state.valid);return 1;
}
static int bench_all_mixer_terms(void) {
    tyvor_sim_t s;CHECK(!start(&s));s.core.models.active.mix_count=16;
    for(unsigned i=0;i<16;++i)s.core.models.active.mixes[i]=(tyvor_mix_t){(uint8_t)(i%6),(uint8_t)(i%8),(uint8_t)(i%2),(int16_t)(100-i),0,(int16_t)i};
    CHECK(!tyvor_sim_advance(&s,500));CHECK(s.ui.state.mix_valid && s.ui.state.mix_count==16);
    for(unsigned i=0;i<16;++i)CHECK(s.ui.state.mixes[i].weight==100-(int)i && s.ui.state.mixes[i].trim==(int)i);
    s.ui.state.mix_scroll=11;tyvor_ui_view_t v;CHECK(!tyvor_ui_core_view(3,&s.ui.state,&v) && strstr(v.rows[5],"16 ") && strstr(v.rows[0],"logical bus"));return 1;
}
static int bench_view_states(void) {
    tyvor_sim_t s;CHECK(!start(&s));tyvor_ui_view_t v;
    static const char *names[]={"HOME","MODEL","INPUTS","MIXES","OUTPUTS","TELEMETRY","SETTINGS"};
    for(unsigned i=0;i<7;++i)CHECK(!tyvor_ui_core_view(i,&s.ui.state,&v) && !strcmp(v.title,names[i]) && strstr(v.banner,"VALID") && strstr(v.banner,"SAFE"));
    s.storage.bytes[0][12]^=1;CHECK(!tyvor_sim_reset_control(&s));CHECK(!tyvor_sim_advance(&s,600));
    CHECK(!tyvor_ui_core_view(1,&s.ui.state,&v) && strstr(v.banner,"INVALID MODEL"));return 1;
}
static int bench_diagnostics(void) {
    tyvor_sim_t s;CHECK(!start(&s));tyvor_core_t before=s.core;tyvor_diagnostics_t d;
    tyvor_diagnostics_init(&d,TYVOR_ROLE_UI,7,s.ui_resets-1,1);tyvor_diagnostics_update(&d,&s.ui.link,&s.ui_adapter,s.now);
    CHECK(d.protocol==2 && d.boot_reason==7 && d.status_age_valid && d.sessions && d.reset_observed_valid);
    CHECK(!d.lifetime_reset_valid && !d.heap_valid && !d.stack_valid && !memcmp(&before,&s.core,sizeof(before)));
    CHECK(!tyvor_sim_reset_ui(&s));tyvor_diagnostics_init(&d,TYVOR_ROLE_UI,8,s.ui_resets-1,1);CHECK(d.resets_observed==1);return 1;
}
static uint32_t random_next(uint32_t *x) {*x^=*x<<13;*x^=*x>>17;*x^=*x<<5;return *x;}
static int bench_fault_seed(void) {
    tyvor_sim_t s;CHECK(!start(&s));uint32_t seed=UINT32_C(0x2bf407);
    for(unsigned step=0;step<120;++step) {
        s.control_to_ui.loss_every=3+random_next(&seed)%13;s.ui_to_control.corrupt_every=4+random_next(&seed)%11;
        s.control_to_ui.duplicate_every=2+random_next(&seed)%7;s.ui_to_control.delay_ms=random_next(&seed)%40;
        s.control_to_ui.delay_ms=random_next(&seed)%60;s.control_to_ui.chunk_size=1+random_next(&seed)%64;
        if(step%17==0)CHECK(!tyvor_sim_reset_control(&s));if(step%19==0)CHECK(!tyvor_sim_reset_ui(&s));
        CHECK(!tyvor_sim_advance(&s,150));CHECK(!tyvor_core_rf_enabled());
        for(unsigned i=0;i<8;++i)CHECK(s.core.channels[i]>=-1000 && s.core.channels[i]<=1000);
        if(s.ui.state.valid)CHECK(s.now<s.ui.state.expires_ms && !s.ui.state.snapshot.rf_enabled);
    }
    CHECK(!recover(&s));puts("FAULT seed=002bf407 iterations=120 PASS");return 1;
}
typedef struct {const char *name;int (*run)(void);} item;
#define CASE(x) {#x,x}
static const item cases[]={CASE(bench_bidirectional),CASE(bench_loss),CASE(bench_delay),CASE(bench_corruption),CASE(bench_duplication),CASE(bench_reconnect),
 CASE(bench_reset_stm32),CASE(bench_reset_esp32),CASE(bench_reset_both),CASE(bench_session_collision),CASE(bench_stale_cache),CASE(bench_long_delay_rejected),CASE(bench_old_packets),
 CASE(bench_partial_tx),CASE(bench_interbyte_timeout),CASE(bench_buffer_bound),CASE(bench_disabled_gate),CASE(bench_lease_exhaustion),CASE(bench_read_replay),CASE(bench_read_before_offer),
 CASE(bench_all_mixer_terms),CASE(bench_view_states),CASE(bench_diagnostics),CASE(bench_fault_seed),CASE(bench_delay_within_lease),CASE(bench_telemetry_age_bound)};
int main(int argc,char **argv) {
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i)if(argc==2 && !strcmp(argv[1],cases[i].name)) {
        int ok=cases[i].run();printf("%s %s\n",ok?"PASS":"FAIL",cases[i].name);return ok?0:1;
    }
    return 2;
}
