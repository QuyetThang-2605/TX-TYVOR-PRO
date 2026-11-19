#ifndef TYVOR_CORE_WIRE_H
#define TYVOR_CORE_WIRE_H
#include <stddef.h>
#include "tyvor_core_schema.h"
#define TYVOR_CORE_MAGIC 0xc2u
#define TYVOR_CORE_WIRE_REV 2u
#define TYVOR_CORE_PARTS 5u
enum {TYVOR_CORE_MODEL=1,TYVOR_CORE_INPUTS,TYVOR_CORE_OUTPUTS,TYVOR_CORE_SAFETY,TYVOR_CORE_TELEMETRY,TYVOR_CORE_MIXES};
#define TYVOR_READ_MAGIC 0xc3u
#define TYVOR_READ_REV 1u
#define TYVOR_READ_PERIOD_MS 10u
#define TYVOR_READ_TIMEOUT_MS 200u
size_t tyvor_read_encode(uint32_t token,uint8_t out[7]);
int tyvor_read_decode(const uint8_t *,size_t,uint32_t *token);
/* Read-only DIAGNOSTIC sub-schema, inside unchanged protocol-v2 envelope. */
size_t tyvor_core_wire_encode(const tyvor_core_snapshot_t *,unsigned kind,uint64_t now,uint8_t *,size_t);
/* On failure leaves output unchanged. On success modifies only this part's fields. */
int tyvor_core_wire_decode(tyvor_core_snapshot_t *,const uint8_t *,size_t,unsigned *kind,uint16_t *capture_age);
#endif
