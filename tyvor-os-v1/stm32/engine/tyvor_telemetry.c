#include "tyvor_telemetry.h"
#include <string.h>
static void clear(tyvor_telemetry_view_t *v,unsigned field) {
    v->valid&=(uint8_t)~(1u<<field);v->age_ms[field]=UINT16_MAX;
    switch(field) {
    case 0:v->battery_mv=0;break;
    case 1:v->lq=0;break;
    case 2:v->rssi_dbm=0;break;
    case 3:v->latitude_e7=0;v->longitude_e7=0;v->satellites=0;break;
    default:break;
    }
}
void tyvor_telemetry_init(tyvor_telemetry_t *s) {if(s) {memset(s,0,sizeof(*s));for(unsigned i=0;i<4;++i)s->values.age_ms[i]=UINT16_MAX;}}
int tyvor_telemetry_feed(tyvor_telemetry_t *s,const tyvor_telemetry_sample_t *p,uint64_t now) {
    if (!s || !p || !p->present || (p->present&~15u) || (p->values.valid&~p->present) || p->sampled_ms>now) return -1;
    const tyvor_telemetry_view_t *v=&p->values;
    if (((v->valid&1) && !v->battery_mv) || ((v->valid&2) && v->lq>100) ||
        ((v->valid&4) && (v->rssi_dbm<-150 || v->rssi_dbm>0)) ||
        ((v->valid&8) && (v->latitude_e7<-900000000 || v->latitude_e7>900000000 ||
         v->longitude_e7<-1800000000 || v->longitude_e7>1800000000 || v->satellites>64))) return -1;
    for (unsigned i=0;i<4;++i) if ((p->present&(1u<<i)) && (s->seen&(1u<<i)) && p->sampled_ms<=s->sampled_ms[i]) return -1;
    for (unsigned i=0;i<4;++i) {
        uint8_t bit=(uint8_t)(1u<<i);if(!(p->present&bit))continue;
        s->sampled_ms[i]=p->sampled_ms;s->seen|=bit;
        if (!(v->valid&bit)) {clear(&s->values,i);continue;}
        s->values.valid|=bit;
        switch(i) {
        case 0:s->values.battery_mv=v->battery_mv;break;
        case 1:s->values.lq=v->lq;break;
        case 2:s->values.rssi_dbm=v->rssi_dbm;break;
        case 3:s->values.latitude_e7=v->latitude_e7;s->values.longitude_e7=v->longitude_e7;s->values.satellites=v->satellites;break;
        default:break;
        }
    }
    tyvor_telemetry_poll(s,now);return 0;
}
void tyvor_telemetry_poll(tyvor_telemetry_t *s,uint64_t now) {
    if (!s) return;
    for (unsigned i=0;i<4;++i) if((s->values.valid&(1u<<i)) &&
        (now<s->sampled_ms[i] || now-s->sampled_ms[i]>=tyvor_telemetry_ttl(i))) clear(&s->values,i);
}
void tyvor_telemetry_view(const tyvor_telemetry_t *s,uint64_t now,tyvor_telemetry_view_t *v) {
    if (!s || !v) return;
    *v=s->values;
    for (unsigned i=0;i<4;++i) {
        uint16_t age=tyvor_age16(now,s->sampled_ms[i]);
        if(!(v->valid&(1u<<i)) || age>=tyvor_telemetry_ttl(i)) clear(v,i);
        else v->age_ms[i]=age;
    }
}
