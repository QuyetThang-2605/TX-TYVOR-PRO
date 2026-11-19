#include "display_contract.h"
#include <string.h>
int tyvor_display_config(tyvor_display_config_t *c,unsigned rotation) {
    if(!c || rotation>3)return TYVOR_DISPLAY_INVALID;
    *c=(tyvor_display_config_t){TFT_SPI_320X240,rotation,(uint16_t)(rotation%2?320:240),(uint16_t)(rotation%2?240:320),10000000};return 0;
}
static int allowed_pin(int pin) {return pin==1 || pin==2 || (pin>=4 && pin<=16) || pin==21 || pin==38;}
int tyvor_display_config_valid(const tyvor_display_config_t *c) {
    tyvor_display_config_t expected;
    return c && !tyvor_display_config(&expected,c->rotation) && c->profile==expected.profile && c->width==expected.width &&
        c->height==expected.height && c->spi_hz>=1000000 && c->spi_hz<=20000000;
}
int tyvor_display_pin_policy(const tyvor_display_pins_t *p) {
    if(!p)return TYVOR_DISPLAY_INVALID;
    if(!p->hardware_gate)return TYVOR_DISPLAY_DISABLED;
    if(!p->board_verified || !p->module_verified)return TYVOR_DISPLAY_INVALID;
    int pins[]={p->sclk,p->mosi,p->miso,p->cs,p->dc,p->reset,p->backlight};
    for(unsigned i=0;i<7;++i) {
        if((i==2 || i==6) && pins[i]==-1)continue;
        if(!allowed_pin(pins[i]))return TYVOR_DISPLAY_INVALID;
        for(unsigned j=0;j<i;++j)if(pins[i]==pins[j])return TYVOR_DISPLAY_INVALID;
    }
    return 0;
}
int tyvor_display_region_valid(const tyvor_display_config_t *c,const tyvor_display_region_t *r,size_t bytes) {
    if(!c || !r || c->profile!=TFT_SPI_320X240 || c->rotation>3 || !c->width || !c->height ||
       r->x1<0 || r->y1<0 || r->x2<=r->x1 || r->y2<=r->y1 || r->x2>c->width || r->y2>c->height)return 0;
    return bytes==(size_t)(r->x2-r->x1)*(size_t)(r->y2-r->y1)*2 && bytes<=TYVOR_DISPLAY_BUFFER_BYTES;
}
void tyvor_rgb565_wire(uint8_t *pixels,size_t count) {
    if(!pixels)return;
    for(size_t i=0;i<count;++i) {uint16_t color;memcpy(&color,pixels+2*i,2);pixels[2*i]=(uint8_t)(color>>8);pixels[2*i+1]=(uint8_t)color;}
}
int tyvor_display_pipeline_init(tyvor_display_pipeline_t *p,const tyvor_display_config_t *c,tyvor_display_backend_t b) {
    if(!p || !tyvor_display_config_valid(c) || !b.start || !b.wait || !b.micros || !b.rotate)return -1;
    memset(p,0,sizeof(*p));p->config=*c;p->backend=b;return 0;
}
int tyvor_display_begin(tyvor_display_pipeline_t *p,const tyvor_display_region_t *r,const uint8_t *bytes,size_t n) {
    if(!p || !bytes || p->fault || !tyvor_display_region_valid(&p->config,r,n))return TYVOR_DISPLAY_INVALID;
    if(p->busy)return TYVOR_DISPLAY_BUSY;
    p->owned_buffer=bytes;p->owned_bytes=n;p->started_us=p->backend.micros(p->backend.context);
    p->busy=1;
    int result=p->backend.start(p->backend.context,r,bytes,n);
    if(result) {++p->errors;p->fault=1;return TYVOR_DISPLAY_IO;}
    return 0;
}
int tyvor_display_complete(tyvor_display_pipeline_t *p) {
    if(!p || !p->busy || p->fault)return TYVOR_DISPLAY_INVALID;
    if(p->backend.wait(p->backend.context,TYVOR_DISPLAY_WAIT_MS)) {
        ++p->errors;++p->timeouts;p->fault=1;return TYVOR_DISPLAY_IO;
        /* Keep ownership latched: never free/reuse a potentially active DMA buffer. */
    }
    uint64_t end=p->backend.micros(p->backend.context);
    if(end<p->started_us) {++p->errors;p->fault=1;return TYVOR_DISPLAY_IO;}
    p->last_transfer_us=end-p->started_us;
    if(p->last_transfer_us>p->max_transfer_us)p->max_transfer_us=p->last_transfer_us;
    p->total_transfer_us+=p->last_transfer_us;++p->transfers;p->pixels+=(uint32_t)(p->owned_bytes/2);
    p->busy=0;p->owned_buffer=NULL;p->owned_bytes=0;return 0;
}
int tyvor_display_rotate(tyvor_display_pipeline_t *p,unsigned rotation) {
    tyvor_display_config_t next;
    if(!p || p->fault || tyvor_display_config(&next,rotation))return TYVOR_DISPLAY_INVALID;
    if(p->busy)return TYVOR_DISPLAY_BUSY;
    if(p->backend.rotate(p->backend.context,rotation)) {p->fault=1;++p->errors;return TYVOR_DISPLAY_IO;}
    next.spi_hz=p->config.spi_hz;p->config=next;return 0;
}
