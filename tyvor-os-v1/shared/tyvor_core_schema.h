#ifndef TYVOR_CORE_SCHEMA_H
#define TYVOR_CORE_SCHEMA_H
#include <stdint.h>
#define TYVOR_AXES 4u
#define TYVOR_SWITCHES 2u
#define TYVOR_CHANNELS 8u
#define TYVOR_LOGICAL_INPUTS 6u
#define TYVOR_MODEL_NAME_CAP 15u
#define TYVOR_CORE_PERIOD_MS 4u
#define TYVOR_UI_CORE_STALE_MS 500u
#define TYVOR_UI_PARTIAL_MS 300u
#define TYVOR_TELEMETRY_FIELDS 4u
enum {TYVOR_STARTUP, TYVOR_DISARMED, TYVOR_BENCH_ARMED, TYVOR_FAILSAFE};
enum {TYVOR_SAFE_STARTUP=1, TYVOR_SAFE_THROTTLE=2, TYVOR_SAFE_INPUT=4,
      TYVOR_SAFE_MODEL=8, TYVOR_SAFE_SCHEDULER=16, TYVOR_SAFE_CLOCK=32,
      TYVOR_SAFE_SWITCH=64};
enum {TYVOR_T_BATTERY=1, TYVOR_T_LQ=2, TYVOR_T_RSSI=4, TYVOR_T_GPS=8};
/* Read-only monitor schema. BENCH_ARMED never means RF/aircraft ARM. */
typedef struct {
    uint16_t battery_mv;
    uint8_t lq, satellites, valid;
    int16_t rssi_dbm;
    int32_t latitude_e7, longitude_e7;
    uint16_t age_ms[TYVOR_TELEMETRY_FIELDS];
} tyvor_telemetry_view_t;
typedef struct {uint8_t source,destination,operation;int16_t weight,offset,trim;} tyvor_mix_view_t;
typedef struct {
    uint32_t generation, model_id, model_revision, ticks;
    char model_name[TYVOR_MODEL_NAME_CAP];
    int16_t inputs[TYVOR_AXES], channels[TYVOR_CHANNELS];
    int8_t switches[TYVOR_SWITCHES];
    uint16_t input_age_ms, safety_reasons;
    uint8_t input_valid, outputs_valid, safety_state, throttle_lock, rf_enabled;
    tyvor_telemetry_view_t telemetry;
    tyvor_mix_view_t mixes[16];uint8_t mix_count,mix_page;
    uint64_t captured_ms; /* local only, never serialized as a shared clock */
} tyvor_core_snapshot_t;
static inline uint32_t tyvor_telemetry_ttl(unsigned field) {return field==3?2000u:1000u;}
static inline uint16_t tyvor_age16(uint64_t now,uint64_t then) {
    return now<then || now-then>=UINT16_MAX?UINT16_MAX:(uint16_t)(now-then);
}
#endif
