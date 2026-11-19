#include "tyvor_async_tx.h"
#include <string.h>
void tyvor_async_tx_init(tyvor_async_tx_t *q,tyvor_tx_start_fn start,tyvor_tx_state_fn state,tyvor_tx_abort_fn abort,void *ctx) {
    if(!q)return;
    memset(q,0,sizeof(*q));q->start=start;q->state=state;q->abort=abort;q->context=ctx;
}
static void remove_at(tyvor_async_tx_t *q,unsigned i) {
    for(unsigned j=i+1;j<q->count;++j)q->pending[j-1]=q->pending[j];
    --q->count;
}
int tyvor_async_tx_write(tyvor_async_tx_t *q,const uint8_t *b,uint16_t n,uint64_t now) {
    if(!q)return -1;
    tyvor_frame_t frame;
    if(!q->start || !q->state || !q->abort || !b || n>TYVOR_FRAME_MAX || tyvor_decode(b,n,&frame)) {
        ++q->invalid;return -1;
    }
    /* Only pending STATUS is replaceable; never mutate an active ISR buffer.
       Remove then append, preserving enqueue order for all retained packets. */
    if(frame.type==TYVOR_STATUS)for(unsigned i=0;i<q->count;++i) {
        if(q->pending[i].bytes[3]==TYVOR_STATUS) {remove_at(q,i);++q->coalesced;break;}
    }
    if(q->count==TYVOR_TX_PENDING) {++q->full;return -1;}
    tyvor_tx_slot_t *slot=&q->pending[q->count++];
    memcpy(slot->bytes,b,n);slot->n=n;slot->offered_ms=now;++q->accepted;
    if(q->count>q->high_water)q->high_water=q->count;
    return n;
}
void tyvor_async_tx_poll(tyvor_async_tx_t *q,uint64_t now) {
    if(!q || !q->start || !q->state || !q->abort)return;
    q->now_ms=now;
    if(q->busy) {
        /* state may advance a nonblocking FIFO driver. Check the deadline BEFORE
           calling it, so a delayed poll cannot push expired bytes into hardware. */
        if(now<q->started_ms || now-q->started_ms>=TYVOR_TX_DEADLINE_MS) {
            q->abort(q->context);q->busy=0;++q->timeouts;
        } else {
            int result=q->state(q->context);
            if(result<0) {q->abort(q->context);q->busy=0;++q->errors;}
            else if(result>0) {q->busy=0;++q->completed;}
        }
    }
    if(q->busy)return;
    /* At most four expired entries and one start per invocation. */
    while(q->count) {
        q->active=q->pending[0];remove_at(q,0);
        if(now<q->active.offered_ms || now-q->active.offered_ms>=TYVOR_TX_DEADLINE_MS) {++q->expired;continue;}
        q->started_ms=now;
        if(q->start(q->context,q->active.bytes,q->active.n)) {q->abort(q->context);++q->errors;}
        else q->busy=1;
        break;
    }
}
