#ifndef TYVOR_DISCOVERY_BOARD_H
#define TYVOR_DISCOVERY_BOARD_H
#include <stdint.h>
#include "tyvor_timing.h"
#include "discovery_demo.h"
#if !defined(TYVOR_DISCOVERY_MB997E) || TYVOR_DISCOVERY_MB997E!=1
#error Discovery_BSP_requires_explicit_MB997E_target
#endif
#if defined(TYVOR_DISCOVERY_UART_ENABLED) && TYVOR_DISCOVERY_UART_ENABLED
#error Discovery_UART_is_not_enabled_or_implemented_for_this_bench_target
#endif
#define DISCOVERY_STACK_RESERVE 8192u
#define DISCOVERY_HSE_HZ 8000000u
#define DISCOVERY_SYSCLK_HZ 168000000u
typedef struct {
    uint32_t reset_flags,clock_hz,fault_code,watchdog_refreshes,loop_count;
    uint32_t stack_free_now;
    uint8_t hse_bypass,watchdog_enabled,uart_enabled,rf_enabled;
} discovery_board_diagnostics_t;
extern discovery_board_diagnostics_t discovery_board_diagnostics;
extern tyvor_timing_t discovery_timing;
extern const char tyvor_discovery_firmware_identity[];
int discovery_board_init(void);
uint64_t discovery_board_millis(void);
uint32_t discovery_board_cycles(void);
int discovery_board_button(void);
void discovery_board_leds(uint16_t mask);
void discovery_board_checkpoint(uint64_t started,uint64_t ended,int healthy);
void discovery_board_idle(void);
void discovery_board_fault(uint32_t code);
#endif
