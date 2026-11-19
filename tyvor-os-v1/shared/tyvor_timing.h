#ifndef TYVOR_TIMING_H
#define TYVOR_TIMING_H
#include <stdint.h>
enum {TYVOR_TIME_INPUT,TYVOR_TIME_CORE,TYVOR_TIME_UART,TYVOR_TIME_LOOP,TYVOR_TIME_SECTIONS};
typedef struct {uint32_t samples,last_cycles,max_cycles,over_budget;} tyvor_timing_section_t;
typedef struct {
    tyvor_timing_section_t section[TYVOR_TIME_SECTIONS];
    uint32_t clock_hz,core_budget_cycles,loop_budget_cycles,max_lateness_ms,late_ticks;
    uint8_t counter_available,hardware_measured;
} tyvor_timing_t;
void tyvor_timing_init(tyvor_timing_t *,uint32_t hz,int available,int measured);
/* Unsigned cycle subtraction supports one wrap only. Sample intervals MUST be
 * less than 2^32 cycles. Halts/long faults require discarding the measurement. */
void tyvor_timing_record(tyvor_timing_t *,unsigned section,uint32_t start,uint32_t end);
void tyvor_timing_lateness(tyvor_timing_t *,uint64_t now,uint64_t deadline);
#endif
