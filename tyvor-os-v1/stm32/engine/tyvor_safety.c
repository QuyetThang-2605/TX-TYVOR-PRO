#include "tyvor_safety.h"
#include <string.h>
void tyvor_safety_init(tyvor_safety_t *s) {
    if (!s) return;
    memset(s,0,sizeof(*s));s->state=TYVOR_STARTUP;s->throttle_lock=1;s->reasons=TYVOR_SAFE_STARTUP;
}
void tyvor_safety_step(tyvor_safety_t *s,const tyvor_inputs_t *in,const tyvor_model_t *m,uint16_t faults) {
    if (!s) return;
    if (!in || !in->valid) faults|=TYVOR_SAFE_INPUT;
    if (tyvor_model_validate(m)) faults|=TYVOR_SAFE_MODEL;
    if (!faults && in->switches[1]==2) faults|=TYVOR_SAFE_SWITCH; /* local 3-position kill */
    if (faults) {
        s->state=TYVOR_FAILSAFE;s->reasons=faults;s->throttle_lock=1;s->off_seen=0;s->previous_arm=1;return;
    }
    unsigned arm=(unsigned)in->switches[0];
    int low=in->immediate[m->throttle_axis]<=-950 && in->filtered[m->throttle_axis]<=-950;
    if (s->state==TYVOR_STARTUP || s->state==TYVOR_FAILSAFE) {
        s->throttle_lock=1;
        if (!arm && low) {s->state=TYVOR_DISARMED;s->reasons=0;s->off_seen=1;}
        else s->reasons=(uint16_t)((s->state==TYVOR_STARTUP?TYVOR_SAFE_STARTUP:0) |
                                   (arm?TYVOR_SAFE_SWITCH:0) | (low?0:TYVOR_SAFE_THROTTLE));
    } else if (s->state==TYVOR_BENCH_ARMED) {
        if (!arm) {s->state=TYVOR_DISARMED;s->throttle_lock=1;s->off_seen=1;s->reasons=0;}
    } else if (!arm) {
        s->off_seen=1;s->reasons=low?0:TYVOR_SAFE_THROTTLE;s->throttle_lock=1;
    } else if (!s->previous_arm) {
        if (s->off_seen && low) {s->state=TYVOR_BENCH_ARMED;s->throttle_lock=0;s->reasons=0;}
        else {s->reasons=TYVOR_SAFE_THROTTLE;s->throttle_lock=1;}
        s->off_seen=0;
    }
    s->previous_arm=(uint8_t)arm;
}
void tyvor_safety_apply(const tyvor_safety_t *s,const tyvor_model_t *m,const int16_t desired[TYVOR_CHANNELS],int16_t out[TYVOR_CHANNELS]) {
    if (!out) return;
    memset(out,0,TYVOR_CHANNELS*sizeof(*out));out[2]=-1000;
    if (tyvor_model_validate(m)) return;
    memcpy(out,m->failsafe,sizeof(m->failsafe));
    if (s && s->state==TYVOR_BENCH_ARMED && !s->throttle_lock && desired)
        memcpy(out,desired,TYVOR_CHANNELS*sizeof(*out));
}
