#include "discovery_demo.h"
#include <string.h>
static int model_check(void) {
    tyvor_model_t model,decoded;uint8_t bytes[TYVOR_MODEL_BYTES_MAX];
    tyvor_model_default(&model,1);size_t n=tyvor_model_serialize(&model,bytes,sizeof(bytes));
    if(!n || tyvor_model_deserialize(&decoded,bytes,n) || decoded.id!=1)return -1;
    bytes[n-1]^=1;
    if(!tyvor_model_deserialize(&decoded,bytes,n))return -1;
    model.mixes[0].weight=201;return tyvor_model_validate(&model)?0:-1;
}
static int mixer_check(void) {
    tyvor_model_t model;tyvor_model_default(&model,1);model.mix_count=16;
    int16_t inputs[6]={1000,0,0,0,0,0},channels[8];
    for(unsigned i=0;i<16;++i)model.mixes[i]=(tyvor_mix_t){0,0,TYVOR_MIX_ADD,200,1000,500};
    if(tyvor_mixer_run(&model,inputs,channels) || channels[0]!=1000)return -1;
    for(unsigned i=0;i<16;++i)model.mixes[i]=(tyvor_mix_t){0,0,TYVOR_MIX_ADD,-200,-1000,-500};
    return tyvor_mixer_run(&model,inputs,channels) || channels[0]!=-1000?-1:0;
}
static int safety_check(void) {
    tyvor_safety_t safety;tyvor_inputs_t input;tyvor_model_t model;
    tyvor_model_default(&model,1);tyvor_inputs_init(&input);input.valid=1;
    input.immediate[2]=input.filtered[2]=-1000;tyvor_safety_init(&safety);
    tyvor_safety_step(&safety,&input,&model,0);if(safety.state!=TYVOR_DISARMED)return -1;
    input.switches[0]=1;tyvor_safety_step(&safety,&input,&model,0);
    if(safety.state!=TYVOR_BENCH_ARMED)return -1;
    input.valid=0;tyvor_safety_step(&safety,&input,&model,0);
    return safety.state==TYVOR_FAILSAFE && safety.throttle_lock?0:-1;
}
static int telemetry_check(void) {
    tyvor_telemetry_t telemetry;tyvor_telemetry_init(&telemetry);
    tyvor_telemetry_sample_t sample=tyvor_bench_telemetry(0);
    if(tyvor_telemetry_feed(&telemetry,&sample,0))return -1;
    tyvor_telemetry_poll(&telemetry,2000);return telemetry.values.valid?-1:0;
}
int discovery_demo_selftest(void) {
    return model_check() || mixer_check() || safety_check() || telemetry_check()?-1:0;
}
static int safe_restart(discovery_demo_t *d,uint64_t now) {
    tyvor_model_t model;tyvor_model_default(&model,1);
    if(tyvor_core_init(&d->core,&model,tyvor_mock_storage_adapter(&d->storage),now))return -1;
    d->input_sequence=0;d->mode_started_ms=now;d->next_input_ms=now;d->next_telemetry_ms=now;
    return 0;
}
int discovery_demo_init(discovery_demo_t *d,uint64_t now,int raw_button) {
    if(!d || now>UINT64_MAX-1000)return -1;
    memset(d,0,sizeof(*d));d->last_poll_ms=now;d->button_changed_ms=now;
    d->button_raw=d->button_stable=(uint8_t)(raw_button!=0);d->button_released=(uint8_t)!raw_button;
    if(tyvor_bench_seed_storage(&d->storage) || safe_restart(d,now) || discovery_demo_selftest()) {
        d->faults=DISCOVERY_FAULT_SELFTEST;d->leds=DISCOVERY_RED;return -1;
    }
    d->selftest_mask=15;return 0;
}
static int triangle(uint64_t elapsed) {
    unsigned phase=(unsigned)(elapsed%2000);
    return phase<1000?(int)(phase*2)-1000:3000-(int)(phase*2);
}
static void feed(discovery_demo_t *d,uint64_t now,uint64_t elapsed) {
    if(now>=d->next_input_ms) {
        int16_t axes[4]={0,0,-1000,0};int8_t switches[2]={0,1};int loss=0;
        if(d->mode==DISCOVERY_MIXER) {
            unsigned phase=(unsigned)(elapsed%4000);switches[0]=(int8_t)(phase>=300 && phase<3000);
            axes[0]=(int16_t)triangle(elapsed);axes[1]=(int16_t)-axes[0];axes[3]=(int16_t)triangle(elapsed+500);
            if(phase>=400 && phase<3000)axes[2]=(int16_t)triangle(elapsed);
        } else if(d->mode==DISCOVERY_INPUT_LOSS) {
            unsigned phase=(unsigned)(elapsed%3000);
            switches[0]=(int8_t)((phase>=300 && phase<1800) || (phase>=2000 && phase<2200));
            loss=phase>=1000 && phase<1300;
        }
        if(!loss) {
            tyvor_input_sample_t sample=tyvor_bench_input(&d->core.models.active,axes,switches,++d->input_sequence,now);
            if(tyvor_core_input(&d->core,&sample,now))d->faults|=DISCOVERY_FAULT_CORE;
            ++d->input_samples;
        }
        d->next_input_ms=now+10;
    }
    if(now>=d->next_telemetry_ms) {
        unsigned phase=(unsigned)(elapsed%6000);
        if(d->mode!=DISCOVERY_MODEL_TELEMETRY || phase<1000 || phase>=4000) {
            tyvor_telemetry_sample_t sample=tyvor_bench_telemetry(now);
            if(tyvor_core_telemetry(&d->core,&sample,now))d->faults|=DISCOVERY_FAULT_CORE;
            ++d->telemetry_samples;
        }
        d->next_telemetry_ms=now+100;
    }
}
int discovery_demo_poll(discovery_demo_t *d,uint64_t now,int raw_button) {
    if(!d || !d->core.initialized)return -1;
    if(now<d->last_poll_ms || now>UINT64_MAX-1000) {
        (void)tyvor_core_poll(&d->core,now);d->faults|=DISCOVERY_FAULT_CLOCK;d->leds=DISCOVERY_RED;return -1;
    }
    d->last_poll_ms=now;
    if(d->faults) {d->leds=DISCOVERY_RED;return -1;}
    uint64_t elapsed=now-d->mode_started_ms;
    feed(d,now,elapsed);
    int tick=tyvor_core_poll(&d->core,now);
    if(tick<0 || d->core.faults)d->faults|=DISCOVERY_FAULT_CORE;
    /* Process timing/safety before USER. A mode change cannot hide a missed tick. */
    if(d->faults) {d->leds=DISCOVERY_RED;return -1;}
    if(d->mode==DISCOVERY_MODEL_TELEMETRY) {
        unsigned slot=(unsigned)((elapsed/2000)%2);
        if(d->core.models.active.id!=slot+1 && d->core.safety.state==TYVOR_DISARMED) {
            if(tyvor_core_load_model(&d->core,slot,now))d->faults|=DISCOVERY_FAULT_MODEL;
            else {++d->model_changes;d->next_input_ms=now+10;}
        }
    }
    /* A simultaneous USER edge must not restart Core after a failed model read. */
    if(d->faults) {d->leds=DISCOVERY_RED;return -1;}
    uint8_t raw=(uint8_t)(raw_button!=0);
    if(raw!=d->button_raw) {d->button_raw=raw;d->button_changed_ms=now;}
    if(raw!=d->button_stable && now-d->button_changed_ms>=DISCOVERY_DEMO_DEBOUNCE_MS) {
        d->button_stable=raw;
        if(!raw)d->button_released=1;
        else if(d->button_released) {
            d->button_released=0;d->mode=(uint8_t)((d->mode+1)%DISCOVERY_MODES);++d->mode_changes;
            if(safe_restart(d,now))d->faults|=DISCOVERY_FAULT_CORE;
        }
    }
    d->leds=(uint16_t)(((now/500)%2?DISCOVERY_GREEN:0) |
        ((now/100)%2?DISCOVERY_ORANGE:0) | (d->button_stable?DISCOVERY_BLUE:0) |
        (d->faults || d->core.safety.state==TYVOR_FAILSAFE?DISCOVERY_RED:0));
    return d->faults?-1:tick;
}
