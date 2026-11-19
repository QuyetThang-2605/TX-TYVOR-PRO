#ifndef TYVOR_LINK_H
#define TYVOR_LINK_H
#include "tyvor_protocol.h"
#include "tyvor_status.h"
#define TYVOR_SESSION_RETRY_MS 200u
#define TYVOR_SESSION_TIMEOUT_MS 1000u
#define TYVOR_SESSION_ATTEMPTS 5u
#define TYVOR_SESSION_COOLDOWN_MS 1000u
#define TYVOR_SESSION_IDLE_MS 2000u
#define TYVOR_CONTROL_ACK_MS 100u
typedef int (*tyvor_uart_write_fn)(void *,const uint8_t *,uint16_t);
typedef struct {
    uint32_t tx_frames,tx_errors,tx_partial,sessions,hello_rx,ack_rx;
    uint32_t handshake_timeouts,collisions,old_session,control_dropped,wrong_direction,epoch_exhausted;
} tyvor_link_stats_t;
typedef struct {
    uint8_t active,attempts,offered;
    uint64_t started_ms,next_tx_ms,peer_id;
    uint32_t local_epoch,peer_epoch;
} tyvor_pending_t;
typedef struct {uint8_t valid;uint64_t peer_id;uint32_t local_epoch,peer_epoch;} tyvor_ack_slot_t;
typedef struct {
    tyvor_parser_t parser;
    tyvor_snapshot_t snapshot;
    tyvor_uart_write_fn write;
    void *write_context;
    tyvor_frame_fn receive;
    void *receive_context;
    uint64_t local_id,peer_id,last_peer_ms,next_discovery_ms,last_poll_ms,next_ack_ms,session_started_ms;
    uint32_t local_epoch,peer_epoch,issued_epoch;
    uint8_t role,ready,status_seen,tx_sequence[5],seen[5],rx_sequence[5];
    tyvor_pending_t pending;
    tyvor_ack_slot_t pending_ack,active_ack;
    tyvor_link_stats_t stats;
} tyvor_link_t;
/* Boot identity is a nonzero fresh hardware-random uint64 per boot.
   Role and entropy failures are fatal to the link; no fixed-ID fallback.
   This is a trusted point-to-point bench protocol, not authentication. */
int tyvor_link_init(tyvor_link_t *l,uint8_t role,uint64_t boot_id,tyvor_uart_write_fn write,
                    void *write_context,tyvor_frame_fn receive,void *receive_context,uint64_t now);
int tyvor_link_feed(tyvor_link_t *l,const uint8_t *bytes,size_t n,uint64_t now);
void tyvor_link_poll(tyvor_link_t *l,uint64_t now);
int tyvor_link_send(tyvor_link_t *l,uint8_t type,const uint8_t *payload,uint8_t n);
void tyvor_link_transport_error(tyvor_link_t *l,int overflow);
#endif
