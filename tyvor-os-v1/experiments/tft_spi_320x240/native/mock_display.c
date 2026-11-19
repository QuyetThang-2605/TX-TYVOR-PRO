#include "mock_display.h"
#include <stdio.h>
#include <string.h>
static int start(void *ctx,const tyvor_display_region_t *r,const uint8_t *bytes,size_t n) {
    tyvor_tft_mock_t *m=ctx;++m->starts;if(m->start_error)return -1;
    if(m->active || n!=(size_t)(r->x2-r->x1)*(size_t)(r->y2-r->y1)*2 || r->x1<0 || r->y1<0 || r->x2>m->width || r->y2>m->height)return -1;
    m->active=bytes;size_t i=0;
    for(int y=r->y1;y<r->y2;++y)for(int x=r->x1;x<r->x2;++x) {m->framebuffer[(size_t)y*m->width+x]=(uint16_t)((uint16_t)bytes[i]<<8|bytes[i+1]);i+=2;}
    m->clock+=n/8+17;return 0; /* deterministic simulated transfer cost, never hardware timing */
}
static int wait_done(void *ctx,unsigned timeout) {
    tyvor_tft_mock_t *m=ctx;++m->waits;if(timeout!=TYVOR_DISPLAY_WAIT_MS || m->wait_error)return -1;
    m->active=NULL;if(m->clock_rollback)m->clock=0;return 0;
}
static uint64_t micros(void *ctx) {return ((tyvor_tft_mock_t *)ctx)->clock;}
static int rotate(void *ctx,unsigned rotation) {
    tyvor_tft_mock_t *m=ctx;if(m->active)return -1;tyvor_display_config_t c;
    if(tyvor_display_config(&c,rotation))return -1;
    m->width=c.width;m->height=c.height;++m->rotations;memset(m->framebuffer,0,sizeof(m->framebuffer));return 0;
}
void tyvor_tft_mock_init(tyvor_tft_mock_t *m,const tyvor_display_config_t *c) {memset(m,0,sizeof(*m));m->width=c->width;m->height=c->height;m->clock=1000;}
tyvor_display_backend_t tyvor_tft_mock_backend(tyvor_tft_mock_t *m) {return (tyvor_display_backend_t){m,start,wait_done,micros,rotate};}
int tyvor_tft_mock_ppm(const tyvor_tft_mock_t *m,const char *path) {
    FILE *f=fopen(path,"wb");if(!f)return -1;
    if(fprintf(f,"P6\n%u %u\n255\n",m->width,m->height)<0){fclose(f);return -1;}
    for(size_t i=0;i<(size_t)m->width*m->height;++i) {
        unsigned p=m->framebuffer[i];uint8_t rgb[3]={(uint8_t)(((p>>11)&31)*255/31),(uint8_t)(((p>>5)&63)*255/63),(uint8_t)((p&31)*255/31)};
        if(fwrite(rgb,1,3,f)!=3){fclose(f);return -1;}
    }
    return fclose(f)?-1:0;
}
