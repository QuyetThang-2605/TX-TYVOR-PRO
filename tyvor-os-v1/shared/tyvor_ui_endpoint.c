#include "tyvor_ui_endpoint.h"
#include <string.h>
static void received(void *ctx,const tyvor_frame_t *f,uint64_t now) {
    tyvor_ui_endpoint_t *e=ctx;
    if(f->type==TYVOR_DIAGNOSTIC && f->length && f->payload[0]==TYVOR_CORE_MAGIC) {
        tyvor_core_snapshot_t part={0};unsigned kind;uint16_t age;
        uint64_t owner=e->owner_now>now?e->owner_now:now;
        if(tyvor_core_wire_decode(&part,f->payload,f->length,&kind,&age) || e->session_count!=e->link.stats.sessions || !e->read_active || !e->read_offered ||
            part.generation!=e->read_token || owner<e->read_started_ms || owner-e->read_started_ms>=TYVOR_READ_TIMEOUT_MS) {
            ++e->lease_rejected;return;
        }
        /* Fresh producer capture must be after our request. Full local RTT is
           an upper bound on capture age without synchronized MCU clocks. */
        uint16_t bound=(uint16_t)(owner-e->read_started_ms);if(bound<age)bound=age;
        tyvor_frame_t derived=*f;derived.payload[7]=(uint8_t)bound;derived.payload[8]=(uint8_t)(bound>>8);
        tyvor_ui_state_receive(&e->state,&e->link,&derived,now);return;
    }
    tyvor_ui_state_receive(&e->state,&e->link,f,now);
}
int tyvor_ui_endpoint_init(tyvor_ui_endpoint_t *e,uint64_t id,tyvor_uart_write_fn write,void *ctx,uint64_t now) {
    if(!e)return -1;
    memset(e,0,sizeof(*e));e->owner_now=now;e->next_read_ms=now;
    tyvor_ui_state_init(&e->state);return tyvor_link_init(&e->link,TYVOR_ROLE_UI,id,write,ctx,received,e,now);
}
int tyvor_ui_endpoint_feed(tyvor_ui_endpoint_t *e,const uint8_t *b,size_t n,uint64_t now) {
    if(!e)return -1;
    if(now<e->owner_now)tyvor_ui_endpoint_poll(e,now);
    e->owner_now=now;
    int result=tyvor_link_feed(&e->link,b,n,now);tyvor_ui_state_sync(&e->state,&e->link,now);return result;
}
void tyvor_ui_endpoint_poll(tyvor_ui_endpoint_t *e,uint64_t now) {
    if(!e)return;
    e->owner_now=now;
    tyvor_link_poll(&e->link,now);tyvor_ui_state_sync(&e->state,&e->link,now);
    if(!e->link.ready || e->session_count!=e->link.stats.sessions) {
        e->read_active=0;e->read_offered=0;e->next_read_ms=now;e->session_count=e->link.stats.sessions;
    }
    if(e->read_active && (now<e->read_started_ms || now-e->read_started_ms>=TYVOR_READ_TIMEOUT_MS))e->read_active=0;
    int complete=e->state.mix_valid && e->state.mix_generation==e->read_token;
    if(e->link.ready && now>=e->next_read_ms && (!e->read_active || !e->read_offered || complete) && e->read_token!=UINT32_MAX) {
        uint8_t request[7];++e->read_token;e->read_started_ms=now;e->read_active=1;
        (void)tyvor_read_encode(e->read_token,request);
        e->read_offered=(uint8_t)(tyvor_link_send(&e->link,TYVOR_DIAGNOSTIC,request,sizeof(request))==0);
        e->next_read_ms=now+TYVOR_READ_PERIOD_MS;
    }
}
int tyvor_ui_endpoint_request(tyvor_ui_endpoint_t *e,uint8_t op,uint8_t page) {
    uint8_t b[2]={op,page};if(!e)return -1;
    return tyvor_link_send(&e->link,TYVOR_UI_REQUEST,b,2);
}
