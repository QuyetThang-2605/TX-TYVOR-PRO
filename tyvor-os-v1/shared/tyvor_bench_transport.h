#ifndef TYVOR_BENCH_TRANSPORT_H
#define TYVOR_BENCH_TRANSPORT_H
#include "tyvor_link.h"
typedef int (*tyvor_transport_feed_fn)(void *,const uint8_t *,size_t,uint64_t);
typedef struct {
    tyvor_uart_write_fn backend;void *context;uint8_t enabled;
    uint32_t tx_attempts,tx_frames,tx_bytes,tx_errors,tx_partial,rx_chunks,rx_bytes,rx_errors,suppressed;
} tyvor_bench_transport_t;
/* Single owner; copies/consumes exact bytes through a synchronous backend.
   enabled is supplied by an existing compile-time board gate, never a UI command.
   No GPIO setup, serial opening, flash, RF, heap, retry loop or hidden I/O. */
void tyvor_bench_transport_init(tyvor_bench_transport_t *,tyvor_uart_write_fn,void *,int enabled);
int tyvor_bench_transport_write(void *,const uint8_t *,uint16_t);
int tyvor_bench_transport_receive(tyvor_bench_transport_t *,const uint8_t *,size_t,uint64_t,tyvor_transport_feed_fn,void *);
void tyvor_bench_transport_error(tyvor_bench_transport_t *,tyvor_link_t *,int overflow);
#endif
