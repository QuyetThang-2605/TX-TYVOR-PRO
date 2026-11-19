#include "tyvor_timing.h"
#include <string.h>
void tyvor_timing_init(tyvor_timing_t *t,uint32_t hz,int available,int measured) {
    if(!t)return;
    memset(t,0,sizeof(*t));t->clock_hz=hz;t->counter_available=(uint8_t)(available!=0);
    /* Proposed budgets: Core <1 ms and full owner loop <10 ms. Not measurements. */
    t->core_budget_cycles=hz/1000;t->loop_budget_cycles=hz/100;
    t->hardware_measured=(uint8_t)(available && measured);
}
void tyvor_timing_record(tyvor_timing_t *t,unsigned section,uint32_t start,uint32_t end) {
    if(!t || section>=TYVOR_TIME_SECTIONS || !t->counter_available)return;
    tyvor_timing_section_t *s=&t->section[section];s->last_cycles=end-start;
    if(s->samples!=UINT32_MAX)++s->samples;
    if(s->last_cycles>s->max_cycles)s->max_cycles=s->last_cycles;
    uint32_t budget=section==TYVOR_TIME_CORE?t->core_budget_cycles:section==TYVOR_TIME_LOOP?t->loop_budget_cycles:0;
    if(budget && s->last_cycles>=budget && s->over_budget!=UINT32_MAX)++s->over_budget;
}
void tyvor_timing_lateness(tyvor_timing_t *t,uint64_t now,uint64_t deadline) {
    if(!t || now<=deadline)return;
    uint64_t late=now-deadline;uint32_t value=late>UINT32_MAX?UINT32_MAX:(uint32_t)late;
    if(value>t->max_lateness_ms)t->max_lateness_ms=value;
    if(t->late_ticks!=UINT32_MAX)++t->late_ticks;
}
