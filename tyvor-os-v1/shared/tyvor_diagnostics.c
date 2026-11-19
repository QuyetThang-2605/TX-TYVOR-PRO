#include "tyvor_diagnostics.h"
#include <string.h>
void tyvor_diagnostics_init(tyvor_diagnostics_t *d,uint8_t role,uint32_t reason,uint32_t observed,int valid) {
    if(!d)return;
    memset(d,0,sizeof(*d));d->protocol=TYVOR_VERSION;d->role=role;d->boot_reason=reason;
    d->resets_observed=observed;d->reset_observed_valid=(uint8_t)(valid!=0);
    /* Lifetime/power-cycle count is unknown: no unverified retention/Flash write. */
}
void tyvor_diagnostics_update(tyvor_diagnostics_t *d,const tyvor_link_t *l,const tyvor_bench_transport_t *t,uint64_t now) {
    if(!d || !l || !t)return;
    d->local_epoch=l->local_epoch;d->peer_epoch=l->peer_epoch;d->sessions=l->stats.sessions;d->collisions=l->stats.collisions;
    d->session_state=!l->ready?TYVOR_DIAG_WAIT:l->pending.active?TYVOR_DIAG_REKEY:TYVOR_DIAG_READY;
    d->uart_gate=t->enabled;d->tx_errors=t->tx_errors;d->link_tx_errors=l->stats.tx_errors;d->suppressed=t->suppressed;
    d->rx_errors=t->rx_errors;d->crc_errors=l->parser.stats.crc_errors;d->parser_timeouts=l->parser.stats.timeouts;d->duplicates=l->parser.stats.duplicates;
    if(!d->status_is_tx) {
        d->status_age_valid=l->snapshot.received_once && now>=l->snapshot.received_ms;
        d->status_age_ms=d->status_age_valid?now-l->snapshot.received_ms:0;
    }
}
