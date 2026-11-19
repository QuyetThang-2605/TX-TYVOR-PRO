#ifndef TYVOR_BENCH_H
#define TYVOR_BENCH_H
#include "tyvor_core.h"
/* Software fixtures only. No ADC/GPIO/radio adapter. */
tyvor_input_sample_t tyvor_bench_input(const tyvor_model_t *,const int16_t axes[4],
                                        const int8_t switches[2],uint32_t sequence,uint64_t now);
tyvor_telemetry_sample_t tyvor_bench_telemetry(uint64_t now);
int tyvor_bench_seed_storage(tyvor_mock_storage_t *);
#endif
