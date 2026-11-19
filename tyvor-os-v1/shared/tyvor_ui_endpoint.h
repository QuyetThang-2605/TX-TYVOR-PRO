#ifndef TYVOR_UI_ENDPOINT_H
#define TYVOR_UI_ENDPOINT_H
#include "tyvor_ui_state.h"
typedef struct {
    tyvor_link_t link;tyvor_ui_state_t state;
    uint64_t read_started_ms,next_read_ms,owner_now;
    uint32_t read_token,session_count,lease_rejected;
    uint8_t read_active,read_offered;
} tyvor_ui_endpoint_t;
int tyvor_ui_endpoint_init(tyvor_ui_endpoint_t *,uint64_t boot_id,tyvor_uart_write_fn,void *,uint64_t now);
int tyvor_ui_endpoint_feed(tyvor_ui_endpoint_t *,const uint8_t *,size_t,uint64_t now);
void tyvor_ui_endpoint_poll(tyvor_ui_endpoint_t *,uint64_t now);
/* The only UI request API: read model summary or page 0..6. No writes. */
int tyvor_ui_endpoint_request(tyvor_ui_endpoint_t *,uint8_t opcode,uint8_t page);
#endif
