#ifndef TYVOR_BRIDGE_H
#define TYVOR_BRIDGE_H
#include "tyvor_link.h"
typedef uint8_t (*tyvor_read_service_fn)(void *,uint8_t opcode,uint8_t page);
typedef void (*tyvor_read_packet_fn)(void *,const tyvor_frame_t *,uint64_t);
typedef struct {
    tyvor_link_t link;
    uint8_t ack_sent;
    uint64_t last_ack_ms;
    uint32_t requests,rate_limited;
    tyvor_read_service_fn read_service;
    void *read_context;
    tyvor_read_packet_fn read_packet;
    void *packet_context;
} tyvor_bridge_t;
/* Callback synchronously copies/sends, finite timeout; returns exact byte count.
   No retained pointer, ISR calls, reentry or RF/config-write commands. */
int tyvor_bridge_init(tyvor_bridge_t *b,tyvor_uart_write_fn fn,void *context,
                       uint64_t boot_id,uint64_t now_ms);
int tyvor_bridge_publish(tyvor_bridge_t *b,const tyvor_status_t *status);
int tyvor_bridge_receive(tyvor_bridge_t *b,const uint8_t *bytes,size_t n,uint64_t now_ms);
void tyvor_bridge_poll(tyvor_bridge_t *b,uint64_t now_ms);
void tyvor_bridge_set_read_service(tyvor_bridge_t *,tyvor_read_service_fn,void *);
void tyvor_bridge_set_read_packet(tyvor_bridge_t *,tyvor_read_packet_fn,void *);
#endif
