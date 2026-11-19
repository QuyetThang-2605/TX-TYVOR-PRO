#include "tyvor_models.h"
#include <string.h>
void tyvor_model_default(tyvor_model_t *m,uint32_t id) {
    if (!m) return;
    memset(m,0,sizeof(*m));m->id=id;m->revision=1;
    memcpy(m->name,id==2?"BENCH MODEL B":"BENCH MODEL A",14);
    for (unsigned i=0;i<TYVOR_AXES;++i) {
        m->axes[i]=(tyvor_axis_config_t){0,2048,4095,10,1};
        m->mixes[i]=(tyvor_mix_t){(uint8_t)i,(uint8_t)i,TYVOR_MIX_ADD,100,0,0};
    }
    m->input_timeout_ms=100;m->switch_positions[0]=2;m->switch_positions[1]=3;
    m->throttle_axis=2;m->mix_count=4;
    for (unsigned i=0;i<TYVOR_CHANNELS;++i) m->channel_map[i]=(uint8_t)i;
    m->failsafe[2]=-1000;
    if (id==2) m->mixes[0].weight=50;
}
int tyvor_model_validate(const tyvor_model_t *m) {
    if (!m || !m->id || !m->revision || !m->name[0] || m->name[TYVOR_MODEL_NAME_CAP-1] ||
        !m->mix_count || m->mix_count>TYVOR_MIXES_MAX || m->throttle_axis>=TYVOR_AXES ||
        m->input_timeout_ms<20 || m->input_timeout_ms>1000 ||
        m->switch_positions[0]!=2 || m->switch_positions[1]!=3) return -1;
    int ended=0;
    for (unsigned i=0;i<TYVOR_MODEL_NAME_CAP;++i) {
        unsigned c=(unsigned char)m->name[i];
        if (!c) ended=1;
        else if (ended || c<32 || c>126) return -1;
    }
    for (unsigned i=0;i<TYVOR_AXES;++i) {
        const tyvor_axis_config_t *a=&m->axes[i];
        if (a->maximum>4095 || a->minimum>=a->center || a->center>=a->maximum ||
            a->center-a->minimum<16 || a->maximum-a->center<16 || a->deadband>200 ||
            a->filter_divisor<1 || a->filter_divisor>16) return -1;
    }
    unsigned mask=0;
    for (unsigned i=0;i<TYVOR_CHANNELS;++i) {
        if (m->channel_map[i]>=TYVOR_CHANNELS || (mask&(1u<<m->channel_map[i])) ||
            m->failsafe[i]<-1000 || m->failsafe[i]>1000) return -1;
        mask|=1u<<m->channel_map[i];
    }
    /* Logical bus 2 is the bench throttle bus; its safe endpoint is mandatory. */
    if (m->failsafe[m->channel_map[2]]!=-1000) return -1;
    for (unsigned i=0;i<m->mix_count;++i) {
        const tyvor_mix_t *x=&m->mixes[i];
        if (x->source>=TYVOR_LOGICAL_INPUTS || x->destination>=TYVOR_CHANNELS ||
            x->operation>TYVOR_MIX_REPLACE || x->weight<-200 || x->weight>200 ||
            x->offset<-1000 || x->offset>1000 || x->trim<-500 || x->trim>500) return -1;
    }
    return 0;
}
uint32_t tyvor_model_crc32(const uint8_t *b,size_t n) {
    uint32_t crc=UINT32_MAX;
    if (!b && n) return 0;
    for (size_t i=0;i<n;++i) {
        crc^=b[i];
        for (unsigned bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1)?UINT32_C(0xedb88320):0);
    }
    return ~crc;
}
static void put(uint8_t *b,size_t *p,uint32_t v,unsigned n) {
    for (unsigned i=0;i<n;++i) {b[(*p)++]=(uint8_t)v;v>>=8;}
}
static uint32_t get(const uint8_t *b,size_t *p,unsigned n) {
    uint32_t v=0;for (unsigned i=0;i<n;++i) v|=(uint32_t)b[(*p)++]<<(8*i);return v;
}
size_t tyvor_model_serialize(const tyvor_model_t *m,uint8_t *out,size_t cap) {
    if (!out || tyvor_model_validate(m)) return 0;
    size_t size=101u+9u*m->mix_count;
    if (cap<size || size>TYVOR_MODEL_BYTES_MAX) return 0;
    uint8_t b[TYVOR_MODEL_BYTES_MAX]={0};size_t p=0;
    memcpy(b,"TYMD",4);p=4;put(b,&p,TYVOR_MODEL_SCHEMA,2);put(b,&p,(uint32_t)size,2);
    put(b,&p,m->id,4);put(b,&p,m->revision,4);
    memcpy(b+p,m->name,TYVOR_MODEL_NAME_CAP);p+=TYVOR_MODEL_NAME_CAP;
    for (unsigned i=0;i<TYVOR_AXES;++i) {
        const tyvor_axis_config_t *a=&m->axes[i];
        put(b,&p,a->minimum,2);put(b,&p,a->center,2);put(b,&p,a->maximum,2);
        put(b,&p,a->deadband,2);put(b,&p,a->filter_divisor,1);
    }
    put(b,&p,m->input_timeout_ms,2);put(b,&p,m->switch_positions[0],1);
    put(b,&p,m->switch_positions[1],1);put(b,&p,m->throttle_axis,1);put(b,&p,m->mix_count,1);
    for (unsigned i=0;i<m->mix_count;++i) {
        const tyvor_mix_t *x=&m->mixes[i];
        put(b,&p,x->source,1);put(b,&p,x->destination,1);put(b,&p,x->operation,1);
        put(b,&p,(uint16_t)x->weight,2);put(b,&p,(uint16_t)x->offset,2);put(b,&p,(uint16_t)x->trim,2);
    }
    for (unsigned i=0;i<TYVOR_CHANNELS;++i) put(b,&p,m->channel_map[i],1);
    for (unsigned i=0;i<TYVOR_CHANNELS;++i) put(b,&p,(uint16_t)m->failsafe[i],2);
    put(b,&p,tyvor_model_crc32(b,p),4);
    if (p!=size) return 0;
    memcpy(out,b,size);return size;
}
int tyvor_model_deserialize(tyvor_model_t *out,const uint8_t *b,size_t n) {
    if (!out || !b || n<110 || n>TYVOR_MODEL_BYTES_MAX || memcmp(b,"TYMD",4)) return -1;
    size_t p=4;
    if (get(b,&p,2)!=TYVOR_MODEL_SCHEMA || get(b,&p,2)!=n) return -1;
    size_t crcpos=n-4;
    if (get(b,&crcpos,4)!=tyvor_model_crc32(b,n-4)) return -1;
    tyvor_model_t m={0};m.id=get(b,&p,4);m.revision=get(b,&p,4);
    memcpy(m.name,b+p,TYVOR_MODEL_NAME_CAP);p+=TYVOR_MODEL_NAME_CAP;
    for (unsigned i=0;i<TYVOR_AXES;++i) {
        tyvor_axis_config_t *a=&m.axes[i];
        a->minimum=(uint16_t)get(b,&p,2);a->center=(uint16_t)get(b,&p,2);a->maximum=(uint16_t)get(b,&p,2);
        a->deadband=(uint16_t)get(b,&p,2);a->filter_divisor=(uint8_t)get(b,&p,1);
    }
    m.input_timeout_ms=(uint16_t)get(b,&p,2);m.switch_positions[0]=(uint8_t)get(b,&p,1);
    m.switch_positions[1]=(uint8_t)get(b,&p,1);m.throttle_axis=(uint8_t)get(b,&p,1);m.mix_count=(uint8_t)get(b,&p,1);
    if (m.mix_count>TYVOR_MIXES_MAX || n!=101u+9u*m.mix_count) return -1;
    for (unsigned i=0;i<m.mix_count;++i) {
        tyvor_mix_t *x=&m.mixes[i];x->source=(uint8_t)get(b,&p,1);x->destination=(uint8_t)get(b,&p,1);
        x->operation=(uint8_t)get(b,&p,1);x->weight=(int16_t)get(b,&p,2);
        x->offset=(int16_t)get(b,&p,2);x->trim=(int16_t)get(b,&p,2);
    }
    for (unsigned i=0;i<TYVOR_CHANNELS;++i) m.channel_map[i]=(uint8_t)get(b,&p,1);
    for (unsigned i=0;i<TYVOR_CHANNELS;++i) m.failsafe[i]=(int16_t)get(b,&p,2);
    if (p!=n-4 || tyvor_model_validate(&m)) return -1;
    *out=m;return 0;
}
