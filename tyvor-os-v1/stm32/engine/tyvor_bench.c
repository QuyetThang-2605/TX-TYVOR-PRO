#include "tyvor_bench.h"
tyvor_input_sample_t tyvor_bench_input(const tyvor_model_t *m,const int16_t axes[4],const int8_t switches[2],uint32_t seq,uint64_t now) {
    tyvor_input_sample_t s={0};s.sequence=seq;s.sampled_ms=now;
    if(!m || !axes || !switches || tyvor_model_validate(m))return s;
    for(unsigned i=0;i<4;++i) {
        int32_t v=axes[i];if(v<-1000 || v>1000)return s;
        int32_t magnitude=v<0?-v:v;
        if(magnitude)magnitude=m->axes[i].deadband+(magnitude*(1000-m->axes[i].deadband)+999)/1000;
        int32_t span=v<0?m->axes[i].center-m->axes[i].minimum:m->axes[i].maximum-m->axes[i].center;
        s.axes[i]=(uint16_t)(m->axes[i].center+(v<0?-1:1)*(magnitude*span+500)/1000);
    }
    for(unsigned i=0;i<2;++i)s.switches[i]=switches[i];
    s.axes_valid=15;s.switches_valid=3;return s;
}
tyvor_telemetry_sample_t tyvor_bench_telemetry(uint64_t now) {
    tyvor_telemetry_sample_t s={0};s.present=15;s.sampled_ms=now;
    s.values.valid=15;s.values.battery_mv=8100;s.values.lq=99;s.values.rssi_dbm=-60;
    s.values.latitude_e7=0;s.values.longitude_e7=0;s.values.satellites=7;return s;
}
int tyvor_bench_seed_storage(tyvor_mock_storage_t *s) {
    if(!s)return -1;
    tyvor_mock_storage_init(s);tyvor_model_storage_t a=tyvor_mock_storage_adapter(s);
    for(unsigned slot=0;slot<TYVOR_MODEL_SLOTS;++slot) {
        tyvor_model_t m;uint8_t b[TYVOR_MODEL_BYTES_MAX];tyvor_model_default(&m,slot+1);
        size_t n=tyvor_model_serialize(&m,b,sizeof(b));if(!n || a.write(a.context,slot,b,n))return -1;
    }
    return 0;
}
