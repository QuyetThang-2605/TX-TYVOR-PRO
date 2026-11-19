#ifndef TYVOR_UI_STATE_H
#define TYVOR_UI_STATE_H
#include "tyvor_core_wire.h"
#include "tyvor_link.h"
#include "tyvor_ui.h"
typedef struct {
    tyvor_core_snapshot_t snapshot,pending;
    uint64_t expires_ms,pending_expires_ms,pending_started_ms,input_expires_ms,pending_input_expires_ms;
    uint64_t telemetry_expires_ms[4],pending_telemetry_expires_ms[4];
    uint64_t local_id,peer_id,last_poll_ms,last_ack_ms;
    uint32_t local_epoch,peer_epoch,generation,rejected,partial_timeouts,commits;
    tyvor_mix_view_t mixes[16];uint32_t mix_generation;uint64_t mix_expires_ms;
    uint8_t mix_mask,mix_count,mix_valid,mix_scroll,disconnected;
    uint8_t valid,ready,parts,generation_seen,ack_opcode,ack_result,ack_valid;
} tyvor_ui_state_t;
void tyvor_ui_state_init(tyvor_ui_state_t *);
void tyvor_ui_state_sync(tyvor_ui_state_t *,const tyvor_link_t *,uint64_t now);
void tyvor_ui_state_receive(tyvor_ui_state_t *,const tyvor_link_t *,const tyvor_frame_t *,uint64_t now);
void tyvor_ui_state_poll(tyvor_ui_state_t *,uint64_t now);
/* Builds text only; no display, touch, safety or model mutation dependencies. */
int tyvor_ui_core_view(unsigned page,const tyvor_ui_state_t *,tyvor_ui_view_t *);
#endif
