#include "touch_adapter.h"
#include <string.h>
int tyvor_touch_adapter_init(tyvor_touch_adapter_t *a,int verified,tyvor_touch_spi_read_fn read,void *ctx,uint16_t w,uint16_t h) {
    if(!a || !w || !h)return -1;
    memset(a,0,sizeof(*a));a->width=w;a->height=h;
    if(!verified || !read)return 1;
    a->verified=1;a->read=read;a->context=ctx;return 0;
}
int tyvor_touch_adapter_read(tyvor_touch_adapter_t *a,uint64_t now,tyvor_touch_sample_t *out) {
    if(!a || !out)return -1;
    memset(out,0,sizeof(*out));
    if(!a->verified || !a->read)return 1;
    if(now<a->last_ms)return -1;
    a->last_ms=now;tyvor_touch_sample_t sample={0};
    if(a->read(a->context,&sample) || sample.x<0 || sample.y<0 || sample.x>=a->width || sample.y>=a->height ||
       sample.sampled_ms>now || now-sample.sampled_ms>=100)return -1;
    *out=sample;out->pressed=out->pressed!=0;return 0;
}
