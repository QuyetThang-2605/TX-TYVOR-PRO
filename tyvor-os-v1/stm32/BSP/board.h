#ifndef TYVOR_BOARD_H
#define TYVOR_BOARD_H
#include <stdint.h>
#include <stddef.h>
#include "tyvor_timing.h"
typedef struct {
    volatile uint32_t fault_code, uart_tx_errors, uart_rx_errors, uart_overflows, loop_count, tx_suppressed;
    volatile uint32_t reset_flags, clock_hz, watchdog_refreshes;
    volatile uint32_t tx_full,tx_coalesced,tx_expired,tx_timeouts,tx_completed,tx_high_water;
} board_diagnostics_t;
extern board_diagnostics_t board_diagnostics;
extern tyvor_timing_t board_timing;
uint32_t board_cycles(void);
void board_uart_service(uint64_t now);
int board_init(void);
uint64_t board_millis(void);
uint64_t board_boot_identity(void);
uint32_t board_stack_free(void);
int board_uart_write(void *context, const uint8_t *bytes, uint16_t n);
size_t board_uart_read(uint8_t *bytes, size_t cap, int *transport_error);
int board_uart_enabled(void);
void board_watchdog_checkpoint(uint64_t started, uint64_t completed);
void board_idle(void);
void board_fault(uint32_t code);
#endif
