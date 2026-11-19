#include "tyvor_core_wire.h"
#include <string.h>
static const uint8_t lengths[7]={0,32,22,27,17,32,29};
static void put(uint8_t *b,size_t *p,uint32_t v,unsigned n) {for(unsigned i=0;i<n;++i){b[(*p)++]=(uint8_t)v;v>>=8;}}
static uint32_t get(const uint8_t *b,size_t *p,unsigned n) {uint32_t v=0;for(unsigned i=0;i<n;++i)v|=(uint32_t)b[(*p)++]<<(8*i);return v;}
static int valid_name(const char *name) {
    if(!name[0] || name[TYVOR_MODEL_NAME_CAP-1])return 0;
    unsigned ended=0;
    for(unsigned i=0;i<TYVOR_MODEL_NAME_CAP;++i){unsigned c=(unsigned char)name[i];if(!c)ended=1;else if(ended || c<32 || c>126)return 0;}
    return 1;
}
size_t tyvor_core_wire_encode(const tyvor_core_snapshot_t *s,unsigned k,uint64_t now,uint8_t *out,size_t cap) {
    if(!s || !out || k<1 || k>6 || cap<lengths[k])return 0;
    uint8_t b[32]={TYVOR_CORE_MAGIC,TYVOR_CORE_WIRE_REV,(uint8_t)k};size_t p=3;
    put(b,&p,s->generation,4);put(b,&p,tyvor_age16(now,s->captured_ms),2);
    switch(k) {
    case TYVOR_CORE_MIXES:
        put(b,&p,s->mix_count,1);put(b,&p,s->mix_page,1);
        for(unsigned row=0;row<2;++row) {
            unsigned index=2u*s->mix_page+row;
            tyvor_mix_view_t zero={0};const tyvor_mix_view_t *x=index<s->mix_count && index<16?&s->mixes[index]:&zero;
            put(b,&p,x->source,1);put(b,&p,x->destination,1);put(b,&p,x->operation,1);
            put(b,&p,(uint16_t)x->weight,2);put(b,&p,(uint16_t)x->offset,2);put(b,&p,(uint16_t)x->trim,2);
        }
        break;
    case TYVOR_CORE_MODEL:
        put(b,&p,s->model_id,4);put(b,&p,s->model_revision,4);memcpy(b+p,s->model_name,TYVOR_MODEL_NAME_CAP);p+=TYVOR_MODEL_NAME_CAP;break;
    case TYVOR_CORE_INPUTS:
        put(b,&p,s->input_valid,1);put(b,&p,s->input_age_ms,2);
        for(unsigned i=0;i<4;++i)put(b,&p,(uint16_t)s->inputs[i],2);
        for(unsigned i=0;i<2;++i)put(b,&p,(uint8_t)s->switches[i],1);
        break;
    case TYVOR_CORE_OUTPUTS:
        put(b,&p,s->outputs_valid,1);put(b,&p,s->rf_enabled,1);
        for(unsigned i=0;i<8;++i)put(b,&p,(uint16_t)s->channels[i],2);
        break;
    case TYVOR_CORE_SAFETY:
        put(b,&p,s->safety_state,1);put(b,&p,s->safety_reasons,2);put(b,&p,s->throttle_lock,1);put(b,&p,s->ticks,4);break;
    case TYVOR_CORE_TELEMETRY: {
        const tyvor_telemetry_view_t *v=&s->telemetry;
        put(b,&p,v->valid,1);put(b,&p,v->battery_mv,2);put(b,&p,v->lq,1);put(b,&p,(uint16_t)v->rssi_dbm,2);
        put(b,&p,(uint32_t)v->latitude_e7,4);put(b,&p,(uint32_t)v->longitude_e7,4);put(b,&p,v->satellites,1);
        for(unsigned i=0;i<4;++i)put(b,&p,v->age_ms[i],2);
        break;
    }
    default:return 0;
    }
    if(p!=lengths[k])return 0;
    tyvor_core_snapshot_t check=*s;unsigned decoded_kind;uint16_t age;
    if(tyvor_core_wire_decode(&check,b,p,&decoded_kind,&age))return 0;
    memcpy(out,b,p);return p;
}
int tyvor_core_wire_decode(tyvor_core_snapshot_t *out,const uint8_t *b,size_t n,unsigned *kind,uint16_t *capture_age) {
    if(!out || !b || !kind || !capture_age || n<9 || b[0]!=TYVOR_CORE_MAGIC || b[1]!=TYVOR_CORE_WIRE_REV ||
        b[2]<1 || b[2]>6 || n!=lengths[b[2]])return -1;
    unsigned k=b[2];size_t p=3;tyvor_core_snapshot_t s=*out;
    s.generation=get(b,&p,4);uint16_t age=(uint16_t)get(b,&p,2);
    if(age>=TYVOR_UI_CORE_STALE_MS)return -1;
    switch(k) {
    case TYVOR_CORE_MIXES:
        s.mix_count=(uint8_t)get(b,&p,1);s.mix_page=(uint8_t)get(b,&p,1);
        if(!s.mix_count || s.mix_count>16 || s.mix_page>=(s.mix_count+1u)/2u)return -1;
        for(unsigned row=0;row<2;++row) {
            unsigned index=2u*s.mix_page+row;tyvor_mix_view_t x;
            x.source=(uint8_t)get(b,&p,1);x.destination=(uint8_t)get(b,&p,1);x.operation=(uint8_t)get(b,&p,1);
            x.weight=(int16_t)get(b,&p,2);x.offset=(int16_t)get(b,&p,2);x.trim=(int16_t)get(b,&p,2);
            if(index>=s.mix_count) {if(x.source || x.destination || x.operation || x.weight || x.offset || x.trim)return -1;}
            else {
                if(x.source>=6 || x.destination>=8 || x.operation>1 || x.weight<-200 || x.weight>200 ||
                    x.offset<-1000 || x.offset>1000 || x.trim<-500 || x.trim>500)return -1;
                s.mixes[index]=x;
            }
        }
        break;
    case TYVOR_CORE_MODEL:
        s.model_id=get(b,&p,4);s.model_revision=get(b,&p,4);memcpy(s.model_name,b+p,TYVOR_MODEL_NAME_CAP);p+=TYVOR_MODEL_NAME_CAP;
        if(!s.model_id || !s.model_revision || !valid_name(s.model_name))return -1;
        break;
    case TYVOR_CORE_INPUTS:
        s.input_valid=(uint8_t)get(b,&p,1);s.input_age_ms=(uint16_t)get(b,&p,2);
        if(s.input_valid>1)return -1;
        for(unsigned i=0;i<4;++i){s.inputs[i]=(int16_t)get(b,&p,2);if(s.inputs[i]<-1000 || s.inputs[i]>1000 || (!s.input_valid && s.inputs[i]))return -1;}
        for(unsigned i=0;i<2;++i){s.switches[i]=(int8_t)get(b,&p,1);if(s.switches[i]<0 || s.switches[i]>(i?2:1) || (!s.input_valid && s.switches[i]))return -1;}
        if(s.input_valid && s.input_age_ms>=1000)return -1;
        break;
    case TYVOR_CORE_OUTPUTS:
        s.outputs_valid=(uint8_t)get(b,&p,1);s.rf_enabled=(uint8_t)get(b,&p,1);
        if(s.outputs_valid>1 || s.rf_enabled)return -1;
        for(unsigned i=0;i<8;++i){s.channels[i]=(int16_t)get(b,&p,2);if(s.channels[i]<-1000 || s.channels[i]>1000)return -1;}
        break;
    case TYVOR_CORE_SAFETY:
        s.safety_state=(uint8_t)get(b,&p,1);s.safety_reasons=(uint16_t)get(b,&p,2);s.throttle_lock=(uint8_t)get(b,&p,1);s.ticks=get(b,&p,4);
        if(s.safety_state>TYVOR_FAILSAFE || s.safety_reasons&~127u || s.throttle_lock>1 ||
            (s.safety_state==TYVOR_BENCH_ARMED?(s.throttle_lock || s.safety_reasons):!s.throttle_lock))return -1;
        break;
    case TYVOR_CORE_TELEMETRY: {
        tyvor_telemetry_view_t *v=&s.telemetry;
        v->valid=(uint8_t)get(b,&p,1);v->battery_mv=(uint16_t)get(b,&p,2);v->lq=(uint8_t)get(b,&p,1);v->rssi_dbm=(int16_t)get(b,&p,2);
        v->latitude_e7=(int32_t)get(b,&p,4);v->longitude_e7=(int32_t)get(b,&p,4);v->satellites=(uint8_t)get(b,&p,1);
        for(unsigned i=0;i<4;++i)v->age_ms[i]=(uint16_t)get(b,&p,2);
        if(v->valid&~15u || ((v->valid&1)?!v->battery_mv:v->battery_mv!=0) ||
            ((v->valid&2)?v->lq>100:v->lq!=0) || ((v->valid&4)?(v->rssi_dbm<-150 || v->rssi_dbm>0):v->rssi_dbm!=0) ||
            ((v->valid&8)?(v->latitude_e7<-900000000 || v->latitude_e7>900000000 || v->longitude_e7<-1800000000 || v->longitude_e7>1800000000 || v->satellites>64):
              (v->latitude_e7 || v->longitude_e7 || v->satellites)))return -1;
        for(unsigned i=0;i<4;++i)if((v->valid&(1u<<i))?v->age_ms[i]>=tyvor_telemetry_ttl(i):v->age_ms[i]!=UINT16_MAX)return -1;
        break;
    }
    default:return -1;
    }
    if(p!=n)return -1;
    *out=s;*kind=k;*capture_age=age;return 0;
}
size_t tyvor_read_encode(uint32_t token,uint8_t out[7]) {
    if(!out || !token)return 0;
    out[0]=TYVOR_READ_MAGIC;out[1]=TYVOR_READ_REV;out[2]=1;
    size_t p=3;put(out,&p,token,4);return p;
}
int tyvor_read_decode(const uint8_t *b,size_t n,uint32_t *token) {
    if(!b || !token || n!=7 || b[0]!=TYVOR_READ_MAGIC || b[1]!=TYVOR_READ_REV || b[2]!=1)return -1;
    size_t p=3;uint32_t t=get(b,&p,4);if(!t)return -1;*token=t;return 0;
}
