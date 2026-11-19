#ifndef TYVOR_CORE_BRIDGE_H
#define TYVOR_CORE_BRIDGE_H
#include "tyvor_core.h"
#include "tyvor_bridge.h"
#include "tyvor_core_wire.h"
typedef struct {
    tyvor_bridge_t transport;
    const tyvor_core_t *core; /* read-only: cannot select model/ARM/write outputs */
    tyvor_core_snapshot_t frozen;
    uint64_t next_tx_ms;uint32_t generation,session_count,tx_errors,read_requests;
    uint8_t part,refresh_requested,response_active,mix_page;
    uint32_t read_token,read_rejected;
    uint64_t last_read_ms,last_status_ms;
    uint64_t last_status_success_ms;uint8_t status_sent;
} tyvor_core_bridge_t;
int tyvor_core_bridge_init(tyvor_core_bridge_t *,const tyvor_core_t *,uint64_t boot_id,
                            tyvor_uart_write_fn,void *,uint64_t now);
void tyvor_core_bridge_poll(tyvor_core_bridge_t *,uint64_t now);
int tyvor_core_bridge_feed(tyvor_core_bridge_t *,const uint8_t *,size_t,uint64_t now);
#endif
