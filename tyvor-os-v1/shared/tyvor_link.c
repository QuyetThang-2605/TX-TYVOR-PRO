#include "tyvor_link.h"
#include <string.h>
static int role_valid(uint8_t r) {return r==TYVOR_ROLE_CONTROL || r==TYVOR_ROLE_UI;}
static int begin(tyvor_link_t *l,uint64_t peer,uint32_t epoch,uint64_t now) {
    if (l->issued_epoch==UINT32_MAX) {
        if (!l->stats.epoch_exhausted) ++l->stats.epoch_exhausted;
        l->next_discovery_ms=UINT64_MAX;return -1;
    }
    memset(&l->pending,0,sizeof(l->pending));
    l->pending_ack.valid=0;
    l->pending.active=1;l->pending.local_epoch=++l->issued_epoch;
    l->pending.peer_id=peer;l->pending.peer_epoch=epoch;
    l->pending.started_ms=now;l->pending.next_tx_ms=now;return 0;
}
static int transmit(tyvor_link_t *l,uint8_t type,uint8_t seq,const tyvor_session_tag_t *tag,
                    const uint8_t *b,uint8_t n) {
    uint8_t packet[TYVOR_FRAME_MAX];
    size_t size=tyvor_encode(type,seq,tag,b,n,packet,sizeof(packet));
    if (!size || !l->write) {++l->stats.tx_errors;return -1;}
    int sent=l->write(l->write_context,packet,(uint16_t)size);
    if (sent!=(int)size) {
        ++l->stats.tx_errors;
        if (sent>=0 && sent<(int)size) ++l->stats.tx_partial;
        return -1;
    }
    ++l->stats.tx_frames;return 0;
}
static void queue_ack(tyvor_ack_slot_t *slot,uint64_t peer,uint32_t local_epoch,uint32_t peer_epoch) {
    slot->valid=1;slot->peer_id=peer;slot->local_epoch=local_epoch;slot->peer_epoch=peer_epoch;
}
static void commit(tyvor_link_t *l,const tyvor_frame_t *f,uint64_t now) {
    l->peer_id=f->session.sender_id;l->peer_epoch=f->session.sender_epoch;
    l->local_epoch=l->pending.local_epoch;l->ready=1;l->last_peer_ms=now;
    l->session_started_ms=now;l->status_seen=0;
    l->pending.active=0;l->next_discovery_ms=now;
    memset(l->tx_sequence,0,sizeof(l->tx_sequence));
    memset(l->seen,0,sizeof(l->seen));memset(l->rx_sequence,0,sizeof(l->rx_sequence));
    tyvor_snapshot_invalidate(&l->snapshot);
    l->pending_ack.valid=0;
    queue_ack(&l->active_ack,l->peer_id,l->local_epoch,l->peer_epoch);
    ++l->stats.sessions;
}
static int data_direction(uint8_t role,uint8_t type,int sending) {
    if (type==TYVOR_DIAGNOSTIC) return 1;
    if (type==TYVOR_STATUS) return sending?role==TYVOR_ROLE_CONTROL:role==TYVOR_ROLE_UI;
    if (type==TYVOR_UI_REQUEST) return sending?role==TYVOR_ROLE_UI:role==TYVOR_ROLE_CONTROL;
    if (type==TYVOR_ACK) return sending?role==TYVOR_ROLE_CONTROL:role==TYVOR_ROLE_UI;
    return 0;
}
static int sequence_accept(tyvor_link_t *l,const tyvor_frame_t *f) {
    unsigned t=f->type;
    if (l->seen[t]) {
        uint8_t delta=(uint8_t)(f->sequence-l->rx_sequence[t]);
        if (!delta) {++l->parser.stats.duplicates;return 0;}
        if (delta>=128) {++l->parser.stats.sequence_errors;return 0;}
        if (delta>1) {++l->parser.stats.sequence_errors;l->parser.stats.missing+=(uint32_t)delta-1;}
    }
    l->seen[t]=1;l->rx_sequence[t]=f->sequence;return 1;
}
static void control_frame(tyvor_link_t *l,const tyvor_frame_t *f,uint64_t now) {
    const tyvor_session_tag_t *s=&f->session;
    if (f->payload[0]==l->role || s->sender_id==l->local_id) {++l->stats.collisions;return;}
    if (s->receiver_id && s->receiver_id!=l->local_id) {++l->stats.old_session;return;}
    if (f->type==TYVOR_SESSION_ACK) {
        ++l->stats.ack_rx;
        if (l->ready && s->sender_id==l->peer_id && s->sender_epoch==l->peer_epoch &&
            s->receiver_epoch==l->local_epoch) return; /* Idempotent ACK, no reset. */
        if (!l->pending.active || s->receiver_epoch!=l->pending.local_epoch) {
            ++l->stats.old_session;return;
        }
        if (now<l->pending.started_ms || now-l->pending.started_ms>=TYVOR_SESSION_TIMEOUT_MS) {
            ++l->stats.old_session;return;
        }
        if (l->pending.peer_id && s->sender_id!=l->pending.peer_id) {++l->stats.collisions;return;}
        if (l->pending.peer_id && s->sender_epoch<l->pending.peer_epoch) {++l->stats.old_session;return;}
        if (l->ready && s->sender_id==l->peer_id && s->sender_epoch<l->peer_epoch) {
            ++l->stats.old_session;return;
        }
        if (!l->pending.offered) {++l->stats.control_dropped;return;}
        commit(l,f,now);return;
    }
    ++l->stats.hello_rx;
    if (l->ready && s->sender_id==l->peer_id && s->sender_epoch==l->peer_epoch) {
        if (!s->receiver_epoch || s->receiver_epoch==l->local_epoch) {
            queue_ack(&l->active_ack,l->peer_id,l->local_epoch,l->peer_epoch);return;
        }
    }
    if (l->ready && s->sender_id==l->peer_id && s->sender_epoch<l->peer_epoch) {
        ++l->stats.old_session;return;
    }
    if (s->receiver_epoch && s->receiver_epoch!=l->local_epoch &&
        (!l->pending.active || s->receiver_epoch!=l->pending.local_epoch)) {
        ++l->stats.old_session;return;
    }
    if (!l->pending.active) {
        if (now<l->next_discovery_ms) {++l->stats.control_dropped;return;}
        if (begin(l,s->sender_id,s->sender_epoch,now)) return;
    }
    if (l->pending.peer_id && l->pending.peer_id!=s->sender_id) {
        /* A rebooted peer may arrive while we probe the previous active peer.
           Replace that probe with a fresh challenge; keep the active session
           until the new candidate proves possession of the fresh epoch. */
        if (l->ready && l->pending.peer_id==l->peer_id && l->pending.peer_epoch==l->peer_epoch) {
            if (begin(l,s->sender_id,s->sender_epoch,now)) return;
        } else {++l->stats.collisions;return;}
    }
    if (l->pending.peer_id && s->sender_epoch<l->pending.peer_epoch) {++l->stats.old_session;return;}
    l->pending.peer_id=s->sender_id;l->pending.peer_epoch=s->sender_epoch;
    queue_ack(&l->pending_ack,s->sender_id,l->pending.local_epoch,s->sender_epoch);
}
static void decoded(void *ctx,const tyvor_frame_t *f,uint64_t now) {
    tyvor_link_t *l=ctx;
    if (f->type==TYVOR_HELLO || f->type==TYVOR_SESSION_ACK) {control_frame(l,f,now);return;}
    const tyvor_session_tag_t *s=&f->session;
    if (!l->ready || s->receiver_id!=l->local_id || s->sender_id!=l->peer_id ||
        s->receiver_epoch!=l->local_epoch || s->sender_epoch!=l->peer_epoch) {
        ++l->stats.old_session;return;
    }
    if (!data_direction(l->role,f->type,0)) {++l->stats.wrong_direction;return;}
    if (!sequence_accept(l,f)) return;
    l->last_peer_ms=now;
    if (f->type==TYVOR_STATUS) {
        tyvor_status_t status;
        if (!tyvor_status_unpack(&status,f->payload,f->length))
            { (void)tyvor_snapshot_set(&l->snapshot,&status,now);l->status_seen=1; }
    }
    if (l->receive) l->receive(l->receive_context,f,now);
}
int tyvor_link_init(tyvor_link_t *l,uint8_t role,uint64_t id,tyvor_uart_write_fn write,
                    void *write_context,tyvor_frame_fn receive,void *receive_context,uint64_t now) {
    if (!l) return -1;
    memset(l,0,sizeof(*l));
    if (!role_valid(role) || !id) return -1;
    l->role=role;l->local_id=id;l->write=write;l->write_context=write_context;
    l->receive=receive;l->receive_context=receive_context;l->last_poll_ms=now;
    tyvor_parser_init(&l->parser,TYVOR_FRAME_TIMEOUT_MS);return begin(l,0,0,now);
}
int tyvor_link_feed(tyvor_link_t *l,const uint8_t *bytes,size_t n,uint64_t now) {
    if (!l || !l->local_id) return -1;
    return tyvor_parser_feed(&l->parser,bytes,n,now,decoded,l);
}
void tyvor_link_poll(tyvor_link_t *l,uint64_t now) {
    if (!l || !l->local_id) return;
    if (now<l->last_poll_ms) {
        l->ready=0;l->pending.active=0;l->pending_ack.valid=0;l->active_ack.valid=0;
        tyvor_snapshot_invalidate(&l->snapshot);l->parser.used=0;
        l->next_ack_ms=now;l->next_discovery_ms=now;
    }
    l->last_poll_ms=now;
    tyvor_parser_poll(&l->parser,now,decoded,l);
    (void)tyvor_snapshot_poll(&l->snapshot,now,TYVOR_STALE_MS);
    if (l->pending.active && now>=l->pending.started_ms &&
        now-l->pending.started_ms>=TYVOR_SESSION_TIMEOUT_MS) {
        l->pending.active=0;l->pending_ack.valid=0;
        ++l->stats.handshake_timeouts;l->next_discovery_ms=now+TYVOR_SESSION_COOLDOWN_MS;
    }
    uint64_t status_time=l->status_seen?l->snapshot.received_ms:l->session_started_ms;
    int status_missing=l->ready && l->role==TYVOR_ROLE_UI && now>=status_time && now-status_time>=TYVOR_STALE_MS;
    if (!l->pending.active && now>=l->next_discovery_ms &&
        (!l->ready || status_missing || (now>=l->last_peer_ms && now-l->last_peer_ms>=TYVOR_SESSION_IDLE_MS))) {
        (void)begin(l,l->ready?l->peer_id:0,l->ready?l->peer_epoch:0,now);
    }
    tyvor_ack_slot_t *ack=l->pending_ack.valid?&l->pending_ack:&l->active_ack;
    if (ack->valid && now>=l->next_ack_ms) {
        tyvor_session_tag_t tag={l->local_id,ack->peer_id,ack->local_epoch,ack->peer_epoch};
        uint8_t b[2]={l->role,0};
        int result=transmit(l,TYVOR_SESSION_ACK,0,&tag,b,2);
        if (!result && l->pending.active && tag.sender_epoch==l->pending.local_epoch)
            l->pending.offered=1;
        ack->valid=0;l->next_ack_ms=now+TYVOR_CONTROL_ACK_MS;
    }
    if (l->pending.active && l->pending.attempts<TYVOR_SESSION_ATTEMPTS && now>=l->pending.next_tx_ms) {
        tyvor_session_tag_t tag={l->local_id,l->pending.peer_id,l->pending.local_epoch,l->pending.peer_epoch};
        uint8_t b[2]={l->role,0};
        if (!transmit(l,TYVOR_HELLO,0,&tag,b,2)) l->pending.offered=1;
        ++l->pending.attempts;l->pending.next_tx_ms=now+TYVOR_SESSION_RETRY_MS;
    }
}
int tyvor_link_send(tyvor_link_t *l,uint8_t type,const uint8_t *b,uint8_t n) {
    if (!l || !l->ready) return TYVOR_ERR_SESSION;
    if (!data_direction(l->role,type,1) || !tyvor_payload_valid(type,b,n)) return TYVOR_ERR_PAYLOAD;
    tyvor_session_tag_t tag={l->local_id,l->peer_id,l->local_epoch,l->peer_epoch};
    return transmit(l,type,l->tx_sequence[type]++,&tag,b,n);
}
void tyvor_link_transport_error(tyvor_link_t *l,int overflow) {
    if (l) tyvor_parser_transport_error(&l->parser,overflow);
}
