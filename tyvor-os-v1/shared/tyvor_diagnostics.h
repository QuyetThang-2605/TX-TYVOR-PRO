#ifndef TYVOR_DIAGNOSTICS_H
#define TYVOR_DIAGNOSTICS_H
#include "tyvor_bench_transport.h"
#define TYVOR_FIRMWARE_VERSION "0.2.2-timing-bench"
#ifndef TYVOR_DIAGNOSTICS
#define TYVOR_DIAGNOSTICS 1
#endif
enum {TYVOR_DIAG_WAIT,TYVOR_DIAG_READY,TYVOR_DIAG_REKEY};
typedef struct {
    uint32_t boot_reason,resets_observed,lifetime_resets,clock_hz;
    uint32_t local_epoch,peer_epoch,sessions,collisions,tx_errors,link_tx_errors,rx_errors,crc_errors,parser_timeouts,duplicates,suppressed;
    uint32_t heap_free,heap_min,heap_largest,stack_free,stack_reserve,watchdog_checkpoints;
    uint64_t status_age_ms;
    uint8_t protocol,role,session_state,uart_gate,reset_observed_valid,lifetime_reset_valid;
    uint8_t heap_valid,stack_valid,watchdog_configured,watchdog_subscribed,watchdog_status_valid,status_age_valid,status_is_tx;
} tyvor_diagnostics_t;
/* Collection only; no Core pointer, safety mutation, watchdog feed or I/O. */
void tyvor_diagnostics_init(tyvor_diagnostics_t *,uint8_t role,uint32_t boot_reason,uint32_t observed,int count_valid);
void tyvor_diagnostics_update(tyvor_diagnostics_t *,const tyvor_link_t *,const tyvor_bench_transport_t *,uint64_t now);
#endif
