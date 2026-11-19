#ifndef TYVOR_TELEMETRY_H
#define TYVOR_TELEMETRY_H
#include "tyvor_core_schema.h"
typedef struct {tyvor_telemetry_view_t values;uint8_t present;uint64_t sampled_ms;} tyvor_telemetry_sample_t;
typedef struct {tyvor_telemetry_view_t values;uint64_t sampled_ms[TYVOR_TELEMETRY_FIELDS];uint8_t seen;} tyvor_telemetry_t;
void tyvor_telemetry_init(tyvor_telemetry_t *);
int tyvor_telemetry_feed(tyvor_telemetry_t *,const tyvor_telemetry_sample_t *,uint64_t now);
void tyvor_telemetry_poll(tyvor_telemetry_t *,uint64_t now);
void tyvor_telemetry_view(const tyvor_telemetry_t *,uint64_t now,tyvor_telemetry_view_t *);
#endif
