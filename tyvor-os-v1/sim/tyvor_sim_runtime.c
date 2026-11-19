#include "tyvor_sim_runtime.h"
#include <string.h>
int tyvor_sim_reset_control(tyvor_sim_t *s) {
    if(!s)return -1;
    ++s->control_resets;s->control_to_ui.now=s->now;
    tyvor_bench_transport_init(&s->control_adapter,tyvor_bench_wire_write,&s->control_to_ui,1);
    tyvor_model_t m;tyvor_model_default(&m,1);
    tyvor_model_storage_t adapter=tyvor_mock_storage_adapter(&s->storage);
    /* A reset enters safe state before attempting any persistence operation.
       Failed storage restore must never leave the previous armed core alive. */
    if(tyvor_core_init(&s->core,&m,adapter,s->now))return -1;
    uint8_t bytes[TYVOR_MODEL_BYTES_MAX];size_t n=0;
    if(adapter.read(adapter.context,0,bytes,sizeof(bytes),&n) || tyvor_model_deserialize(&m,bytes,n)) {
        s->core.models.valid=0;s->core.faults|=TYVOR_SAFE_MODEL;
        s->core.safety.state=TYVOR_FAILSAFE;s->core.safety.reasons=TYVOR_SAFE_MODEL;
    } else if(tyvor_core_init(&s->core,&m,adapter,s->now))return -1;
    s->next_input=s->now;s->next_telemetry=s->now;s->input_sequence=0;
    return tyvor_core_bridge_init(&s->bridge,&s->core,++s->control_boot,tyvor_bench_transport_write,&s->control_adapter,s->now);
}
int tyvor_sim_reset_ui(tyvor_sim_t *s) {
    if(!s)return -1;
    ++s->ui_resets;s->ui_to_control.now=s->now;
    tyvor_bench_transport_init(&s->ui_adapter,tyvor_bench_wire_write,&s->ui_to_control,1);
    return tyvor_ui_endpoint_init(&s->ui,++s->ui_boot,tyvor_bench_transport_write,&s->ui_adapter,s->now);
}
int tyvor_sim_init(tyvor_sim_t *s) {
    if(!s)return -1;
    memset(s,0,sizeof(*s));s->axes[2]=-1000;s->switches[1]=1;
    s->control_to_ui.chunk_size=7;s->ui_to_control.chunk_size=7;
    s->control_to_ui.seed=0x2b407;s->ui_to_control.seed=0x32b;
    s->control_boot=1000;s->ui_boot=2000;s->telemetry=tyvor_bench_telemetry(0);
    if(tyvor_bench_seed_storage(&s->storage) || tyvor_sim_reset_control(s) || tyvor_sim_reset_ui(s))return -1;
    return 0;
}
static int feed_ui(void *ctx,const uint8_t *b,size_t n,uint64_t now) {return tyvor_ui_endpoint_feed(ctx,b,n,now);}
static int feed_control(void *ctx,const uint8_t *b,size_t n,uint64_t now) {return tyvor_core_bridge_feed(ctx,b,n,now);}
static void deliver(tyvor_sim_t *s,tyvor_sim_wire_t *w,int to_ui) {
    if(w->drop)return;
    for(unsigned chunk=0;chunk<32;++chunk) {
        uint8_t bytes[64];size_t n=tyvor_bench_wire_take(w,s->now,bytes);if(!n)break;
        if(to_ui)(void)tyvor_bench_transport_receive(&s->ui_adapter,bytes,n,s->now,feed_ui,&s->ui);
        else (void)tyvor_bench_transport_receive(&s->control_adapter,bytes,n,s->now,feed_control,&s->bridge);
    }
}
int tyvor_sim_advance(tyvor_sim_t *s,uint32_t ms) {
    if(!s || ms>60000 || s->now>UINT64_MAX-ms)return -1;
    for(uint32_t i=0;i<ms;++i) {
        ++s->now;s->control_to_ui.now=s->now;s->ui_to_control.now=s->now;
        if(s->now>=s->next_input) {
            if(!s->input_loss) {
                tyvor_input_sample_t sample=tyvor_bench_input(&s->core.models.active,s->axes,s->switches,++s->input_sequence,s->now);
                (void)tyvor_core_input(&s->core,&sample,s->now);
            }
            s->next_input=s->now+10;
        }
        if(s->now>=s->next_telemetry) {
            if(!s->telemetry_loss) {
                s->telemetry.sampled_ms=s->now;
                (void)tyvor_core_telemetry(&s->core,&s->telemetry,s->now);
            }
            s->next_telemetry=s->now+100;
        }
        (void)tyvor_core_poll(&s->core,s->now);
        for(unsigned round=0;round<4;++round) {
            tyvor_core_bridge_poll(&s->bridge,s->now);tyvor_ui_endpoint_poll(&s->ui,s->now);
            deliver(s,&s->control_to_ui,1);deliver(s,&s->ui_to_control,0);
        }
        if(tyvor_core_rf_enabled() || s->ui.state.snapshot.rf_enabled)return -1;
    }
    return 0;
}
int tyvor_sim_model(tyvor_sim_t *s,unsigned slot) {return s?tyvor_core_load_model(&s->core,slot,s->now):-1;}
