#ifndef TYVOR_CORE_H
#define TYVOR_CORE_H
#if !defined(TYVOR_BENCH_ONLY) || TYVOR_BENCH_ONLY != 1
#error TYVOR_Phase2A_requires_explicit_BENCH_ONLY_build
#endif
#if defined(TYVOR_ENABLE_RF) && TYVOR_ENABLE_RF
#error RF_output_is_not_implemented_or_permitted_in_Phase2A
#endif
#include "tyvor_model_store.h"
#include "tyvor_mixer.h"
#include "tyvor_safety.h"
#include "tyvor_telemetry.h"
typedef struct {
    tyvor_model_manager_t models;
    tyvor_inputs_t inputs;
    tyvor_safety_t safety;
    tyvor_telemetry_t telemetry;
    int16_t desired[TYVOR_CHANNELS],channels[TYVOR_CHANNELS];
    uint64_t next_tick_ms,last_poll_ms,last_tick_ms;
    uint32_t ticks,missed_ticks;uint16_t faults;uint8_t initialized,input_fault_pending;
} tyvor_core_t;
int tyvor_core_init(tyvor_core_t *,const tyvor_model_t *,tyvor_model_storage_t,uint64_t now);
int tyvor_core_input(tyvor_core_t *,const tyvor_input_sample_t *,uint64_t now);
int tyvor_core_telemetry(tyvor_core_t *,const tyvor_telemetry_sample_t *,uint64_t now);
/* At most one tick per poll. Missed periods latch a fault; no catch-up burst. */
int tyvor_core_poll(tyvor_core_t *,uint64_t now);
/* Local bench owner only. No UI bridge exposes these functions. */
int tyvor_core_select_model(tyvor_core_t *,const tyvor_model_t *,uint64_t now);
int tyvor_core_load_model(tyvor_core_t *,unsigned slot,uint64_t now);
int tyvor_core_save_model(tyvor_core_t *,unsigned slot,uint64_t now);
void tyvor_core_snapshot(const tyvor_core_t *,uint64_t now,tyvor_core_snapshot_t *);
int tyvor_core_rf_enabled(void);
#endif
