#ifndef TYVOR_DISCOVERY_DEMO_H
#define TYVOR_DISCOVERY_DEMO_H
#include "tyvor_bench.h"
#define DISCOVERY_DEMO_VERSION "TYVOR DISCOVERY BENCH V1"
#define DISCOVERY_DEMO_DEBOUNCE_MS 30u
enum {DISCOVERY_SAFE,DISCOVERY_MIXER,DISCOVERY_INPUT_LOSS,DISCOVERY_MODEL_TELEMETRY,DISCOVERY_MODES};
enum {DISCOVERY_GREEN=1u<<12,DISCOVERY_ORANGE=1u<<13,DISCOVERY_RED=1u<<14,DISCOVERY_BLUE=1u<<15,
      DISCOVERY_LED_MASK=0xf000};
enum {DISCOVERY_FAULT_SELFTEST=1,DISCOVERY_FAULT_CORE=2,DISCOVERY_FAULT_CLOCK=4,DISCOVERY_FAULT_MODEL=8};
typedef struct {
    tyvor_core_t core;tyvor_mock_storage_t storage;
    uint64_t last_poll_ms,mode_started_ms,next_input_ms,next_telemetry_ms,button_changed_ms;
    uint32_t input_sequence,mode_changes,model_changes,input_samples,telemetry_samples,selftest_mask,faults;
    uint16_t leds;
    uint8_t mode,button_raw,button_stable,button_released;
} discovery_demo_t;
/* USER selects software scenarios only. All input/ARM/sensor/output values are
 * in-memory fixtures. This module has no HAL/GPIO/UART/RF/actuator callbacks. */
int discovery_demo_selftest(void);
int discovery_demo_init(discovery_demo_t *,uint64_t now,int raw_button);
int discovery_demo_poll(discovery_demo_t *,uint64_t now,int raw_button);
#endif
