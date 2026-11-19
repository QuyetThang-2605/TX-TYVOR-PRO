#include "tyvor_inputs.h"
#include <string.h>
static void invalidate(tyvor_inputs_t *s) {
    s->valid=0;s->filter_initialized=0;
    memset(s->immediate,0,sizeof(s->immediate));memset(s->filtered,0,sizeof(s->filtered));
    memset(s->switches,0,sizeof(s->switches));
}
void tyvor_inputs_init(tyvor_inputs_t *s) {if(s) memset(s,0,sizeof(*s));}
static int16_t calibrated(uint16_t raw,const tyvor_axis_config_t *a) {
    int32_t x=raw;
    if (x<a->minimum) x=a->minimum;
    if (x>a->maximum) x=a->maximum;
    int32_t v=(x-(int32_t)a->center)*1000/
        (x<a->center?(int32_t)a->center-a->minimum:(int32_t)a->maximum-a->center);
    int32_t magnitude=v<0?-v:v;
    if (magnitude<=a->deadband) return 0;
    magnitude=(magnitude-a->deadband)*1000/(1000-a->deadband);
    return (int16_t)(v<0?-magnitude:magnitude);
}
int tyvor_inputs_feed(tyvor_inputs_t *s,const tyvor_model_t *m,const tyvor_input_sample_t *v,uint64_t now) {
    if (!s) return -1;
    if (!v || tyvor_model_validate(m) || v->axes_valid!=15 || v->switches_valid!=3 ||
        v->sampled_ms>now || now-v->sampled_ms>=m->input_timeout_ms) {
        ++s->rejected;invalidate(s);return -1;
    }
    for (unsigned i=0;i<TYVOR_AXES;++i) if(v->axes[i]>4095) {++s->rejected;invalidate(s);return -1;}
    for (unsigned i=0;i<TYVOR_SWITCHES;++i) if(v->switches[i]<0 || v->switches[i]>=m->switch_positions[i]) {
        ++s->rejected;invalidate(s);return -1;
    }
    if (s->seen && (v->sampled_ms<s->sampled_ms || !(uint32_t)(v->sequence-s->sequence) ||
        (uint32_t)(v->sequence-s->sequence)>=UINT32_C(0x80000000))) {
        ++s->rejected;return -1; /* Duplicate/late packet cannot extend freshness. */
    }
    for (unsigned i=0;i<TYVOR_AXES;++i) {
        int16_t x=calibrated(v->axes[i],&m->axes[i]);s->immediate[i]=x;
        if (!s->filter_initialized) s->filtered[i]=x;
        else s->filtered[i]=(int16_t)(s->filtered[i]+(x-s->filtered[i])/m->axes[i].filter_divisor);
    }
    memcpy(s->switches,v->switches,sizeof(s->switches));
    s->sequence=v->sequence;s->sampled_ms=v->sampled_ms;s->seen=1;s->valid=1;s->filter_initialized=1;return 0;
}
int tyvor_inputs_poll(tyvor_inputs_t *s,const tyvor_model_t *m,uint64_t now) {
    if (!s || !m) return 0;
    if (s->valid && (now<s->sampled_ms || now-s->sampled_ms>=m->input_timeout_ms)) invalidate(s);
    return s->valid;
}
void tyvor_inputs_logical(const tyvor_inputs_t *s,int16_t out[TYVOR_LOGICAL_INPUTS]) {
    if (!out) return;
    memset(out,0,TYVOR_LOGICAL_INPUTS*sizeof(*out));
    if (!s || !s->valid) return;
    memcpy(out,s->filtered,sizeof(s->filtered));
    out[4]=s->switches[0]?1000:-1000;out[5]=(int16_t)((s->switches[1]-1)*1000);
}
