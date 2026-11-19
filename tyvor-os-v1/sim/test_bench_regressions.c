#include "tyvor_sim_runtime.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d FAIL: %s\n",__FILE__,__LINE__,#x);return 0;}}while(0)
static int arm(tyvor_sim_t *s) {
    if(tyvor_sim_init(s) || tyvor_sim_advance(s,20))return -1;
    s->switches[0]=1;return tyvor_sim_advance(s,20) || s->core.safety.state!=TYVOR_BENCH_ARMED?-1:0;
}
static int transient_input_loss(void) {
    tyvor_sim_t s;CHECK(!arm(&s));
    tyvor_input_sample_t bad=tyvor_bench_input(&s.core.models.active,s.axes,s.switches,++s.input_sequence,41);
    bad.axes_valid=0;CHECK(tyvor_core_input(&s.core,&bad,41)<0);
    tyvor_input_sample_t good=tyvor_bench_input(&s.core.models.active,s.axes,s.switches,++s.input_sequence,42);
    CHECK(!tyvor_core_input(&s.core,&good,42));CHECK(tyvor_core_poll(&s.core,50)==1);
    printf("Transient loss final state=%u\n",s.core.safety.state);
    CHECK(s.core.safety.state==TYVOR_FAILSAFE && s.core.safety.throttle_lock);return 1;
}
static int delayed_telemetry(void) {
    tyvor_sim_t s;CHECK(!tyvor_sim_init(&s));CHECK(!tyvor_sim_advance(&s,200));
    tyvor_core_snapshot_t v;tyvor_core_snapshot(&s.core,200,&v);v.generation=s.ui.state.generation+1;
#if TYVOR_CORE_WIRE_REV >= 2
    v.generation=s.ui.read_token;
#endif
    uint8_t packets[5][64];size_t sizes[5];
    tyvor_session_tag_t tag={s.bridge.transport.link.local_id,s.ui.link.local_id,s.bridge.transport.link.local_epoch,s.ui.link.local_epoch};
    for(unsigned k=1;k<=5;++k) {
        uint8_t payload[32];size_t n=tyvor_core_wire_encode(&v,k,200,payload,sizeof(payload));CHECK(n);
        sizes[k-1]=tyvor_encode(TYVOR_DIAGNOSTIC,s.bridge.transport.link.tx_sequence[TYVOR_DIAGNOSTIC]++,&tag,payload,(uint8_t)n,packets[k-1],64);CHECK(sizes[k-1]);
    }
    tyvor_ui_endpoint_poll(&s.ui,1500);
    for(unsigned i=0;i<5;++i)CHECK(!tyvor_ui_endpoint_feed(&s.ui,packets[i],sizes[i],1500));
    printf("Delayed source_age>=1300ms UI valid=%u battery_valid=%u\n",s.ui.state.valid,!!(s.ui.state.snapshot.telemetry.valid&1));
    CHECK(!s.ui.state.valid && !(s.ui.state.snapshot.telemetry.valid&1));return 1;
}
static int expired_input_outputs(void) {
    tyvor_sim_t s;CHECK(!arm(&s));CHECK(!tyvor_sim_advance(&s,200) && s.ui.state.valid);
    s.ui.state.input_expires_ms=s.now;s.ui.state.expires_ms=s.now+100;
    CHECK(s.ui.state.snapshot.outputs_valid);tyvor_ui_state_poll(&s.ui.state,s.now);
    printf("Expired input valid=%u outputs_valid=%u\n",s.ui.state.snapshot.input_valid,s.ui.state.snapshot.outputs_valid);
    CHECK(!s.ui.state.snapshot.input_valid && !s.ui.state.snapshot.outputs_valid);return 1;
}
static unsigned reads;
static int count_read(void *ctx,unsigned slot,uint8_t *b,size_t cap,size_t *n) {
    (void)ctx;(void)slot;(void)b;(void)cap;(void)n;++reads;return -1;
}
static int unsafe_storage_read(void) {
    tyvor_sim_t s;CHECK(!arm(&s));reads=0;s.core.models.storage.read=count_read;
    CHECK(tyvor_core_load_model(&s.core,0,s.now)<0);
    printf("Unsafe model selection storage reads=%u\n",reads);CHECK(!reads);return 1;
}
typedef struct {const char *name;int (*run)(void);} item;
static const item cases[]={{"transient_input_loss",transient_input_loss},{"delayed_telemetry",delayed_telemetry},
    {"expired_input_outputs",expired_input_outputs},{"unsafe_storage_read",unsafe_storage_read}};
int main(int argc,char **argv) {
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i)if(argc==2 && !strcmp(argv[1],cases[i].name)) {
        int ok=cases[i].run();printf("%s %s\n",ok?"PASS":"FAIL",cases[i].name);return ok?0:1;
    }
    return 2;
}
