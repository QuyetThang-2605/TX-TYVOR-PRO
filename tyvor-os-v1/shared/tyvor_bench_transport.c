#include "tyvor_bench_transport.h"
#include <string.h>
void tyvor_bench_transport_init(tyvor_bench_transport_t *t,tyvor_uart_write_fn fn,void *ctx,int enabled) {
    if(t) {memset(t,0,sizeof(*t));t->backend=fn;t->context=ctx;t->enabled=(uint8_t)(enabled!=0);}
}
int tyvor_bench_transport_write(void *ctx,const uint8_t *b,uint16_t n) {
    tyvor_bench_transport_t *t=ctx;if(!t)return -1;
    ++t->tx_attempts;
    if(!b || !n || n>TYVOR_FRAME_MAX || !t->backend) {++t->tx_errors;return -1;}
    if(!t->enabled) {++t->suppressed;return -1;}
    int sent=t->backend(t->context,b,n);
    if(sent!=(int)n) {++t->tx_errors;if(sent>=0 && sent<(int)n)++t->tx_partial;return sent<(int)n?sent:-1;}
    ++t->tx_frames;t->tx_bytes+=n;return sent;
}
int tyvor_bench_transport_receive(tyvor_bench_transport_t *t,const uint8_t *b,size_t n,uint64_t now,tyvor_transport_feed_fn feed,void *ctx) {
    if(!t || !feed || (!b && n) || n>64)return -1;
    if(!t->enabled) {++t->suppressed;return -1;}
    ++t->rx_chunks;t->rx_bytes+=(uint32_t)n;return feed(ctx,b,n,now);
}
void tyvor_bench_transport_error(tyvor_bench_transport_t *t,tyvor_link_t *l,int overflow) {
    if(t)++t->rx_errors;
    tyvor_link_transport_error(l,overflow);
}
