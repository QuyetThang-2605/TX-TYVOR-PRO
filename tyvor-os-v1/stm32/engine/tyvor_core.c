#include "tyvor_core.h"
#include <string.h>
int tyvor_core_init(tyvor_core_t *c,const tyvor_model_t *m,tyvor_model_storage_t storage,uint64_t now) {
    if (!c) return -1;
    memset(c,0,sizeof(*c));tyvor_safety_init(&c->safety);
    if (now>UINT64_MAX-TYVOR_CORE_PERIOD_MS || tyvor_model_manager_init(&c->models,m,storage)) return -1;
    tyvor_inputs_init(&c->inputs);tyvor_telemetry_init(&c->telemetry);
    c->next_tick_ms=now+TYVOR_CORE_PERIOD_MS;c->last_poll_ms=now;c->initialized=1;
    tyvor_safety_apply(&c->safety,m,NULL,c->channels);return 0;
}
int tyvor_core_input(tyvor_core_t *c,const tyvor_input_sample_t *v,uint64_t now) {
    if(!c || !c->initialized)return -1;
    uint8_t was_valid=c->inputs.valid;
    (void)tyvor_inputs_poll(&c->inputs,&c->models.active,now);
    if(was_valid && !c->inputs.valid)c->input_fault_pending=1;
    int result=tyvor_inputs_feed(&c->inputs,&c->models.active,v,now);
    if(!c->inputs.valid)c->input_fault_pending=1;
    return result;
}
int tyvor_core_telemetry(tyvor_core_t *c,const tyvor_telemetry_sample_t *v,uint64_t now) {
    return c && c->initialized?tyvor_telemetry_feed(&c->telemetry,v,now):-1;
}
int tyvor_core_poll(tyvor_core_t *c,uint64_t now) {
    if (!c || !c->initialized) return -1;
    if (now<c->last_poll_ms || now>UINT64_MAX-TYVOR_CORE_PERIOD_MS) {
        c->faults|=TYVOR_SAFE_CLOCK;c->last_poll_ms=now;
        c->safety.state=TYVOR_FAILSAFE;c->safety.reasons=c->faults;c->safety.throttle_lock=1;
        tyvor_inputs_init(&c->inputs);tyvor_safety_apply(&c->safety,&c->models.active,NULL,c->channels);return -1;
    }
    c->last_poll_ms=now;
    if(now<c->next_tick_ms)return 0;
    uint64_t skipped=(now-c->next_tick_ms)/TYVOR_CORE_PERIOD_MS;
    if(skipped) {
        c->faults|=TYVOR_SAFE_SCHEDULER;
        c->missed_ticks=skipped>UINT32_MAX-c->missed_ticks?UINT32_MAX:c->missed_ticks+(uint32_t)skipped;
    }
    c->last_tick_ms=c->next_tick_ms+skipped*TYVOR_CORE_PERIOD_MS;
    c->next_tick_ms=c->last_tick_ms+TYVOR_CORE_PERIOD_MS;++c->ticks;
    (void)tyvor_inputs_poll(&c->inputs,&c->models.active,now);tyvor_telemetry_poll(&c->telemetry,now);
    int16_t logical[TYVOR_LOGICAL_INPUTS];tyvor_inputs_logical(&c->inputs,logical);
    uint16_t faults=c->faults;
    if(c->input_fault_pending)faults|=TYVOR_SAFE_INPUT;
    if (!c->models.valid || tyvor_mixer_run(&c->models.active,logical,c->desired)) faults|=TYVOR_SAFE_MODEL;
    tyvor_safety_step(&c->safety,&c->inputs,&c->models.active,faults);
    c->input_fault_pending=0; /* only the safety owner acknowledges the event */
    tyvor_safety_apply(&c->safety,&c->models.active,c->desired,c->channels);return 1;
}
static int can_change(tyvor_core_t *c,uint64_t now) {
    return c && c->initialized && c->models.valid && !c->faults && !c->input_fault_pending && !tyvor_model_validate(&c->models.active) && c->safety.state==TYVOR_DISARMED &&
        tyvor_inputs_poll(&c->inputs,&c->models.active,now) && !c->inputs.switches[0] &&
        c->inputs.switches[1]!=2 && c->inputs.immediate[c->models.active.throttle_axis]<=-950 &&
        c->inputs.filtered[c->models.active.throttle_axis]<=-950;
}
int tyvor_core_select_model(tyvor_core_t *c,const tyvor_model_t *m,uint64_t now) {
    if (tyvor_model_validate(m) || !can_change(c,now)) return -1;
    c->models.active=*m;c->models.valid=1;
    tyvor_inputs_init(&c->inputs);tyvor_safety_init(&c->safety);
    memset(c->desired,0,sizeof(c->desired));tyvor_safety_apply(&c->safety,m,NULL,c->channels);return 0;
}
int tyvor_core_load_model(tyvor_core_t *c,unsigned slot,uint64_t now) {
    tyvor_model_t m;
    if(!can_change(c,now) || tyvor_model_manager_read(&c->models,slot,&m))return -1;
    return tyvor_core_select_model(c,&m,now);
}
int tyvor_core_save_model(tyvor_core_t *c,unsigned slot,uint64_t now) {
    return can_change(c,now)?tyvor_model_manager_save(&c->models,slot):-1;
}
void tyvor_core_snapshot(const tyvor_core_t *c,uint64_t now,tyvor_core_snapshot_t *s) {
    if(!s)return;
    memset(s,0,sizeof(*s));
    if(!c || !c->initialized)return;
    s->captured_ms=now;s->model_id=c->models.active.id;s->model_revision=c->models.active.revision;
    memcpy(s->model_name,c->models.active.name,sizeof(s->model_name));
    s->input_valid=c->inputs.valid && now>=c->inputs.sampled_ms && now-c->inputs.sampled_ms<c->models.active.input_timeout_ms;
    s->input_age_ms=c->inputs.seen?tyvor_age16(now,c->inputs.sampled_ms):UINT16_MAX;
    if(s->input_valid) {memcpy(s->inputs,c->inputs.filtered,sizeof(s->inputs));memcpy(s->switches,c->inputs.switches,sizeof(s->switches));}
    memcpy(s->channels,c->channels,sizeof(s->channels));
    s->outputs_valid=(uint8_t)(s->input_valid && c->models.valid && !c->faults && !c->input_fault_pending && !tyvor_model_validate(&c->models.active));
    if(!tyvor_model_validate(&c->models.active)) {
        s->mix_count=c->models.active.mix_count;
        for(unsigned i=0;i<s->mix_count;++i) {
            const tyvor_mix_t *x=&c->models.active.mixes[i];
            s->mixes[i]=(tyvor_mix_view_t){x->source,x->destination,x->operation,x->weight,x->offset,x->trim};
        }
    }
    s->safety_state=c->safety.state;s->safety_reasons=c->safety.reasons;s->throttle_lock=c->safety.throttle_lock;
    s->ticks=c->ticks;s->rf_enabled=0;tyvor_telemetry_view(&c->telemetry,now,&s->telemetry);
}
int tyvor_core_rf_enabled(void) {return 0;}
