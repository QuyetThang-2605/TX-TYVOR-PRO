#include "tyvor_bridge.h"
#include <string.h>
static void request(void *ctx,const tyvor_frame_t *frame,uint64_t now) {
    tyvor_bridge_t *b=ctx;
    if(frame->type==TYVOR_DIAGNOSTIC && b->read_packet) {b->read_packet(b->packet_context,frame,now);return;}
    if (frame->type!=TYVOR_UI_REQUEST) return;
    ++b->requests;
    if (b->ack_sent && now>=b->last_ack_ms && now-b->last_ack_ms<100) {
        ++b->rate_limited;return;
    }
    uint8_t result=b->read_service?b->read_service(b->read_context,frame->payload[0],frame->payload[1]):1;
    if(result>3)result=3;
    uint8_t ack[2]={frame->payload[0],result}; /* Only validated read-only requests. */
    b->ack_sent=1;b->last_ack_ms=now;
    (void)tyvor_link_send(&b->link,TYVOR_ACK,ack,sizeof(ack));
}
int tyvor_bridge_init(tyvor_bridge_t *b,tyvor_uart_write_fn fn,void *ctx,uint64_t id,uint64_t now) {
    if (!b) return -1;
    memset(b,0,sizeof(*b));
    return tyvor_link_init(&b->link,TYVOR_ROLE_CONTROL,id,fn,ctx,request,b,now);
}
int tyvor_bridge_publish(tyvor_bridge_t *b,const tyvor_status_t *status) {
    uint8_t bytes[TYVOR_STATUS_LEN];
    if (!b || tyvor_status_pack(status,bytes,sizeof(bytes))) return TYVOR_ERR_PAYLOAD;
    return tyvor_link_send(&b->link,TYVOR_STATUS,bytes,sizeof(bytes));
}
int tyvor_bridge_receive(tyvor_bridge_t *b,const uint8_t *bytes,size_t n,uint64_t now) {
    return b?tyvor_link_feed(&b->link,bytes,n,now):-1;
}
void tyvor_bridge_poll(tyvor_bridge_t *b,uint64_t now) {
    if (b) tyvor_link_poll(&b->link,now);
}
void tyvor_bridge_set_read_service(tyvor_bridge_t *b,tyvor_read_service_fn fn,void *ctx) {
    if(b) {b->read_service=fn;b->read_context=ctx;}
}
void tyvor_bridge_set_read_packet(tyvor_bridge_t *b,tyvor_read_packet_fn fn,void *ctx) {
    if(b) {b->read_packet=fn;b->packet_context=ctx;}
}
