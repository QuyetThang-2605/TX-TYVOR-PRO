#ifndef TYVOR_ASYNC_TX_H
#define TYVOR_ASYNC_TX_H
#include "tyvor_protocol.h"
#define TYVOR_TX_PENDING 4u
#define TYVOR_TX_DEADLINE_MS 20u
/* Single task/main owner. Driver callbacks MUST be nonblocking. start borrows
 * immutable active bytes until completion/abort. state: 0 busy, 1 done, -1 error.
 * An ISR may own driver progress, never this queue. No allocation or waiting. */
typedef int (*tyvor_tx_start_fn)(void *,const uint8_t *,uint16_t);
typedef int (*tyvor_tx_state_fn)(void *);
typedef void (*tyvor_tx_abort_fn)(void *);
typedef struct {uint8_t bytes[TYVOR_FRAME_MAX];uint16_t n;uint64_t offered_ms;} tyvor_tx_slot_t;
typedef struct {
    tyvor_tx_slot_t pending[TYVOR_TX_PENDING],active;
    tyvor_tx_start_fn start;tyvor_tx_state_fn state;tyvor_tx_abort_fn abort;void *context;
    uint64_t now_ms,started_ms;
    uint32_t accepted,completed,full,coalesced,expired,timeouts,errors,invalid,high_water;
    uint8_t count,busy;
} tyvor_async_tx_t;
void tyvor_async_tx_init(tyvor_async_tx_t *,tyvor_tx_start_fn,tyvor_tx_state_fn,tyvor_tx_abort_fn,void *);
/* Full return means owned copy, NOT wire completion or peer delivery. */
int tyvor_async_tx_write(tyvor_async_tx_t *,const uint8_t *,uint16_t,uint64_t now);
void tyvor_async_tx_poll(tyvor_async_tx_t *,uint64_t now);
#endif
