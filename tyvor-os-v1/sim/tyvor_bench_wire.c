#include "tyvor_bench_wire.h"
#include <string.h>
static uint32_t next_random(tyvor_sim_wire_t *w) {
    if(!w->seed)w->seed=UINT32_C(0x2b407);
    w->seed^=w->seed<<13;w->seed^=w->seed>>17;w->seed^=w->seed<<5;return w->seed;
}
static int append(tyvor_sim_wire_t *w,const uint8_t *b,uint16_t n,uint64_t due,int corrupt) {
    if(w->count>=TYVOR_SIM_WIRE_CAP) {++w->overflows;return -1;}
    unsigned i=w->count++;memcpy(w->bytes[i],b,n);w->length[i]=n;w->offset[i]=0;w->due_ms[i]=due;
    if(corrupt) {w->bytes[i][n-1]^=1;++w->corrupted;}
    return 0;
}
int tyvor_bench_wire_write(void *ctx,const uint8_t *b,uint16_t n) {
    tyvor_sim_wire_t *w=ctx;
    if(!w || !b || !n || n>TYVOR_FRAME_MAX || w->fail || w->delay_ms>60000 || w->jitter_ms>60000 ||
        w->chunk_size>64 || w->byte_gap_ms>1000 || w->now>UINT64_MAX-250000)return -1;
    ++w->frame_count;
    if(w->drop || (w->loss_every && w->frame_count%w->loss_every==0)) {++w->dropped;return n;}
    unsigned copies=w->duplicate_every && w->frame_count%w->duplicate_every==0?2u:1u;
    if(w->count+copies>TYVOR_SIM_WIRE_CAP) {++w->overflows;return -1;}
    uint32_t jitter=w->jitter_ms?next_random(w)%(w->jitter_ms+1u):0;
    uint64_t due=w->now+w->delay_ms+jitter;
    /* Preserve byte order of a UART direction. Artificial old packets can be
       explicitly retained over reconnect; they still must pass the read lease. */
    if(due<w->serialized_until_ms)due=w->serialized_until_ms;
    uint16_t sent=w->partial?(uint16_t)(n-1):n;
    if(!sent)return 0;
    unsigned chunk=w->chunk_size?w->chunk_size:64;
    if(chunk>64)chunk=64;
    uint64_t span=(uint64_t)((sent+chunk-1u)/chunk-1u)*w->byte_gap_ms;
    int corrupt=w->corrupt_every && w->frame_count%w->corrupt_every==0;
    for(unsigned copy=0;copy<copies;++copy) {
        if(append(w,b,sent,due,corrupt))return -1;
        due+=span;w->serialized_until_ms=due;
        if(copy)++w->duplicated;
    }
    return sent;
}
size_t tyvor_bench_wire_take(tyvor_sim_wire_t *w,uint64_t now,uint8_t out[64]) {
    if(!w || !out || w->hold || !w->count || now<w->due_ms[0])return 0;
    uint16_t remaining=(uint16_t)(w->length[0]-w->offset[0]);
    size_t n=w->chunk_size?w->chunk_size:64;if(n>64)n=64;if(n>remaining)n=remaining;
    memcpy(out,w->bytes[0]+w->offset[0],n);w->offset[0]+=(uint16_t)n;
    if(w->offset[0]==w->length[0]) {
        ++w->delivered;--w->count;
        memmove(w->bytes,w->bytes+1,w->count*sizeof(w->bytes[0]));
        memmove(w->length,w->length+1,w->count*sizeof(w->length[0]));
        memmove(w->due_ms,w->due_ms+1,w->count*sizeof(w->due_ms[0]));
        memmove(w->offset,w->offset+1,w->count*sizeof(w->offset[0]));
    } else w->due_ms[0]=now+w->byte_gap_ms;
    return n;
}
void tyvor_bench_wire_clear(tyvor_sim_wire_t *w) {
    if(w) {w->count=0;w->serialized_until_ms=w->now;}
}
