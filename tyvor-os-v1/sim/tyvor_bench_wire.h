#ifndef TYVOR_BENCH_WIRE_H
#define TYVOR_BENCH_WIRE_H
#include "tyvor_protocol.h"
#define TYVOR_SIM_WIRE_CAP 32u
typedef struct {
    uint8_t bytes[TYVOR_SIM_WIRE_CAP][TYVOR_FRAME_MAX];uint16_t length[TYVOR_SIM_WIRE_CAP];
    uint64_t due_ms[TYVOR_SIM_WIRE_CAP],now,serialized_until_ms;
    unsigned count,drop,fail,partial;
    uint32_t seed,loss_every,corrupt_every,duplicate_every,delay_ms,jitter_ms,chunk_size,byte_gap_ms,frame_count;
    uint32_t dropped,corrupted,duplicated,delivered,overflows;
    uint16_t offset[TYVOR_SIM_WIRE_CAP];uint8_t hold;
} tyvor_sim_wire_t;
int tyvor_bench_wire_write(void *,const uint8_t *,uint16_t);
/* Returns one due chunk; bounded 32-event scan, no sleep or device access. */
size_t tyvor_bench_wire_take(tyvor_sim_wire_t *,uint64_t now,uint8_t out[64]);
void tyvor_bench_wire_clear(tyvor_sim_wire_t *);
#endif
