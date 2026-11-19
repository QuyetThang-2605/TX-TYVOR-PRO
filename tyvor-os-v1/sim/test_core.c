#include "tyvor_sim_runtime.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d FAIL: %s\n",__FILE__,__LINE__,#x);return 0;}} while(0)
static int core_start(tyvor_core_t *c,tyvor_model_t *m) {
    tyvor_model_default(m,1);return tyvor_core_init(c,m,(tyvor_model_storage_t){0},0);
}
static int feed(tyvor_core_t *c,int throttle,int arm,uint32_t seq,uint64_t now) {
    const int16_t axes[4]={0,0,(int16_t)throttle,0};const int8_t switches[2]={(int8_t)arm,1};
    tyvor_input_sample_t v=tyvor_bench_input(&c->models.active,axes,switches,seq,now);
    return tyvor_core_input(c,&v,now);
}
static int armed(tyvor_sim_t *s) {
    if(tyvor_sim_init(s) || tyvor_sim_advance(s,20))return -1;
    s->switches[0]=1;
    return tyvor_sim_advance(s,20) || s->core.safety.state!=TYVOR_BENCH_ARMED?-1:0;
}
static int input_calibration(void) {
    tyvor_core_t c;tyvor_model_t m;CHECK(!core_start(&c,&m));
    int16_t axes[4]={-1000,1000,-1000,0};const int8_t sw[2]={0,1};
    tyvor_input_sample_t v=tyvor_bench_input(&m,axes,sw,1,0);
    CHECK(!tyvor_core_input(&c,&v,0));CHECK(c.inputs.filtered[0]==-1000 && c.inputs.filtered[1]==1000 && c.inputs.filtered[3]==0);
    m.axes[0]=(tyvor_axis_config_t){100,2000,3900,0,1};c.models.active=m;
    v.axes[0]=4095;v.sequence=2;v.sampled_ms=1;CHECK(!tyvor_core_input(&c,&v,1));CHECK(c.inputs.filtered[0]==1000);
    v.axes[0]=0;v.sequence=3;v.sampled_ms=2;CHECK(!tyvor_core_input(&c,&v,2));CHECK(c.inputs.filtered[0]==-1000);return 1;
}
static int input_deadband_filter(void) {
    tyvor_core_t c;tyvor_model_t m;CHECK(!core_start(&c,&m));c.models.active.axes[0].filter_divisor=4;
    CHECK(!feed(&c,-1000,0,1,0));
    tyvor_input_sample_t v={.axes={2050,2048,0,2048},.switches={0,1},.axes_valid=15,.switches_valid=3,.sequence=2,.sampled_ms=10};
    CHECK(!tyvor_core_input(&c,&v,10));CHECK(!c.inputs.immediate[0] && !c.inputs.filtered[0]);
    v.axes[0]=4095;v.sequence=3;v.sampled_ms=20;CHECK(!tyvor_core_input(&c,&v,20));
    CHECK(c.inputs.immediate[0]==1000 && c.inputs.filtered[0]==250);
    v.sequence=4;v.sampled_ms=30;CHECK(!tyvor_core_input(&c,&v,30));CHECK(c.inputs.filtered[0]==437);return 1;
}
static int input_invalid(void) {
    tyvor_core_t c;tyvor_model_t m;CHECK(!core_start(&c,&m));CHECK(!feed(&c,-1000,0,1,0));
    tyvor_input_sample_t v={.axes={2048,2048,0,2048},.switches={0,1},.axes_valid=15,.switches_valid=3,.sequence=2,.sampled_ms=10};
    v.axes[0]=4096;CHECK(tyvor_core_input(&c,&v,10)<0 && !c.inputs.valid);
    v.axes[0]=2048;v.switches[0]=2;CHECK(tyvor_core_input(&c,&v,10)<0);
    v.switches[0]=0;v.switches_valid=1;CHECK(tyvor_core_input(&c,&v,10)<0);
    v.switches_valid=3;v.sampled_ms=11;CHECK(tyvor_core_input(&c,&v,10)<0);
    v.sampled_ms=10;CHECK(!tyvor_core_input(&c,&v,10));CHECK(c.inputs.valid);return 1;
}
static int input_timeout(void) {
    tyvor_core_t c;tyvor_model_t m;CHECK(!core_start(&c,&m));CHECK(!feed(&c,-1000,0,1,0));
    CHECK(tyvor_inputs_poll(&c.inputs,&m,99));CHECK(!tyvor_inputs_poll(&c.inputs,&m,100));
    for(unsigned i=0;i<4;++i)CHECK(c.inputs.filtered[i]==0);
    CHECK(!feed(&c,-1000,0,2,101));CHECK(!tyvor_inputs_poll(&c.inputs,&m,100));return 1;
}
static int input_sequence(void) {
    tyvor_core_t c;tyvor_model_t m;CHECK(!core_start(&c,&m));CHECK(!feed(&c,-1000,0,UINT32_MAX,0));
    CHECK(feed(&c,1000,1,UINT32_MAX,10)<0 && c.inputs.sampled_ms==0);
    CHECK(!feed(&c,-1000,0,0,20));CHECK(feed(&c,1000,1,UINT32_MAX,30)<0 && c.inputs.sequence==0);
    CHECK(!tyvor_inputs_poll(&c.inputs,&m,120));return 1;
}
static int mixer_weights(void) {
    tyvor_model_t m;tyvor_model_default(&m,1);m.mixes[0]=(tyvor_mix_t){0,0,TYVOR_MIX_ADD,-150,100,50};
    int16_t in[6]={500,0,-1000,0,0,0},out[8];CHECK(!tyvor_mixer_run(&m,in,out));
    CHECK(out[0]==-600 && out[2]==-1000);return 1;
}
static int mixer_ordering(void) {
    tyvor_model_t m;tyvor_model_default(&m,1);m.mix_count=3;
    m.mixes[0]=(tyvor_mix_t){0,0,TYVOR_MIX_ADD,100,0,0};
    m.mixes[1]=(tyvor_mix_t){0,0,TYVOR_MIX_REPLACE,50,0,0};
    m.mixes[2]=(tyvor_mix_t){0,0,TYVOR_MIX_ADD,0,100,0};
    int16_t in[6]={1000},out[8];CHECK(!tyvor_mixer_run(&m,in,out) && out[0]==600);
    tyvor_mix_t tmp=m.mixes[1];m.mixes[1]=m.mixes[2];m.mixes[2]=tmp;
    CHECK(!tyvor_mixer_run(&m,in,out) && out[0]==500);return 1;
}
static int mixer_routing(void) {
    tyvor_model_t m;tyvor_model_default(&m,1);m.channel_map[0]=7;m.channel_map[7]=0;
    m.mixes[4]=(tyvor_mix_t){5,4,TYVOR_MIX_ADD,100,0,0};m.mix_count=5;
    int16_t in[6]={1000,0,-1000,0,0,-1000},out[8];CHECK(!tyvor_mixer_run(&m,in,out));
    CHECK(out[7]==1000 && out[0]==0 && out[4]==-1000);return 1;
}
static int mixer_clamp_overflow(void) {
    tyvor_model_t m;tyvor_model_default(&m,1);m.mix_count=16;
    for(unsigned i=0;i<16;++i)m.mixes[i]=(tyvor_mix_t){0,0,TYVOR_MIX_ADD,200,1000,500};
    int16_t in[6]={1000},out[8];CHECK(!tyvor_mixer_run(&m,in,out) && out[0]==1000);
    for(unsigned i=0;i<16;++i)m.mixes[i]=(tyvor_mix_t){0,0,TYVOR_MIX_ADD,-200,-1000,-500};
    CHECK(!tyvor_mixer_run(&m,in,out) && out[0]==-1000);
    m.mix_count=2;m.mixes[0]=(tyvor_mix_t){0,0,TYVOR_MIX_ADD,200,0,0};m.mixes[1]=(tyvor_mix_t){0,0,TYVOR_MIX_ADD,-150,0,0};
    CHECK(!tyvor_mixer_run(&m,in,out) && out[0]==500);
    m.mixes[0].weight=INT16_MAX;CHECK(tyvor_mixer_run(&m,in,out)<0);
    m.mixes[0].weight=100;in[0]=INT16_MIN;CHECK(tyvor_mixer_run(&m,in,out)<0);return 1;
}
static int model_roundtrip(void) {
    tyvor_model_t m,decoded;tyvor_model_default(&m,2);uint8_t a[256],b[256];
    size_t n=tyvor_model_serialize(&m,a,sizeof(a));CHECK(n==137);
    CHECK(!tyvor_model_deserialize(&decoded,a,n));CHECK(decoded.id==2 && !strcmp(decoded.name,"BENCH MODEL B"));
    CHECK(tyvor_model_serialize(&decoded,b,sizeof(b))==n && !memcmp(a,b,n));
    CHECK(!tyvor_model_serialize(&m,b,n-1));CHECK(!tyvor_model_serialize(&m,NULL,256));
    for(size_t size=0;size<n;++size)CHECK(tyvor_model_deserialize(&decoded,a,size)<0);return 1;
}
static int model_crc(void) {
    CHECK(tyvor_model_crc32((const uint8_t *)"123456789",9)==UINT32_C(0xcbf43926));
    tyvor_model_t m,out,before;tyvor_model_default(&m,1);memset(&out,0xa5,sizeof(out));before=out;
    uint8_t b[256];size_t n=tyvor_model_serialize(&m,b,sizeof(b));CHECK(n);
    for(size_t i=0;i<n;++i)for(unsigned bit=0;bit<8;++bit) {
        b[i]^=(uint8_t)(1u<<bit);CHECK(tyvor_model_deserialize(&out,b,n)<0 && !memcmp(&out,&before,sizeof(out)));b[i]^=(uint8_t)(1u<<bit);
    }
    return 1;
}
static int model_validation(void) {
    tyvor_model_t good,m;tyvor_model_default(&good,1);CHECK(!tyvor_model_validate(&good));
#define BAD(change) do {m=good;change;CHECK(tyvor_model_validate(&m)<0);} while(0)
    BAD(m.id=0);BAD(m.name[14]='x');BAD(m.name[0]=0);BAD(m.axes[0].center=0);
    BAD(m.axes[0].maximum=4096);BAD(m.axes[0].deadband=201);BAD(m.axes[0].filter_divisor=0);
    BAD(m.input_timeout_ms=0);BAD(m.switch_positions[0]=3);BAD(m.throttle_axis=4);
    BAD(m.mix_count=17);BAD(m.mixes[0].source=6);BAD(m.mixes[0].destination=8);BAD(m.mixes[0].operation=2);
    BAD(m.mixes[0].weight=201);BAD(m.mixes[0].offset=1001);BAD(m.mixes[0].trim=-501);
    BAD(m.channel_map[0]=1);BAD(m.failsafe[2]=0);BAD(m.failsafe[0]=1001);
#undef BAD
    return 1;
}
static void fix_crc(uint8_t *b,size_t n) {
    uint32_t crc=tyvor_model_crc32(b,n-4);for(unsigned i=0;i<4;++i)b[n-4+i]=(uint8_t)(crc>>(8*i));
}
static int model_version(void) {
    tyvor_model_t m,out;tyvor_model_default(&m,1);uint8_t b[256];size_t n=tyvor_model_serialize(&m,b,sizeof(b));CHECK(n);
    b[4]=2;fix_crc(b,n);CHECK(tyvor_model_deserialize(&out,b,n)<0);
    b[4]=1;b[8]=0;b[9]=0;b[10]=0;b[11]=0;fix_crc(b,n);CHECK(tyvor_model_deserialize(&out,b,n)<0);return 1;
}
static int model_storage(void) {
    tyvor_mock_storage_t store;CHECK(!tyvor_bench_seed_storage(&store));tyvor_model_t m,loaded;tyvor_model_default(&m,1);
    tyvor_model_manager_t owner;CHECK(!tyvor_model_manager_init(&owner,&m,tyvor_mock_storage_adapter(&store)));
    CHECK(!tyvor_model_manager_read(&owner,1,&loaded) && loaded.id==2);
    owner.active=loaded;CHECK(!tyvor_model_manager_save(&owner,0));
    tyvor_core_t rebooted;CHECK(!tyvor_model_manager_read(&owner,0,&m));
    CHECK(!tyvor_core_init(&rebooted,&m,tyvor_mock_storage_adapter(&store),100));
    CHECK(rebooted.models.active.id==2 && rebooted.safety.state==TYVOR_STARTUP);return 1;
}
static int model_atomic_failure(void) {
    tyvor_mock_storage_t store,before;CHECK(!tyvor_bench_seed_storage(&store));before=store;
    tyvor_model_t m,out;tyvor_model_default(&m,2);tyvor_model_manager_t owner;
    CHECK(!tyvor_model_manager_init(&owner,&m,tyvor_mock_storage_adapter(&store)));store.fail_write=1;
    CHECK(tyvor_model_manager_save(&owner,0)<0 && !memcmp(store.bytes,before.bytes,sizeof(store.bytes)));
    store.fail_write=0;store.bytes[0][20]^=1;CHECK(tyvor_model_manager_read(&owner,0,&out)<0);
    store.fail_read=1;CHECK(tyvor_model_manager_read(&owner,1,&out)<0);
    CHECK(tyvor_model_manager_read(&owner,2,&out)<0);return 1;
}
static int model_switch_guard(void) {
    tyvor_sim_t s;CHECK(!armed(&s));CHECK(tyvor_sim_model(&s,1)<0 && s.core.models.active.id==1);
    s.switches[0]=0;s.axes[2]=1000;CHECK(!tyvor_sim_advance(&s,20));CHECK(tyvor_sim_model(&s,1)<0);
    s.axes[2]=-1000;CHECK(!tyvor_sim_advance(&s,20));CHECK(!tyvor_sim_model(&s,1));
    CHECK(s.core.models.active.id==2 && s.core.safety.state==TYVOR_STARTUP && !s.core.inputs.valid);
    CHECK(s.core.channels[2]==-1000);return 1;
}
static int safety_high_throttle(void) {
    tyvor_sim_t s;CHECK(!tyvor_sim_init(&s));s.axes[2]=1000;CHECK(!tyvor_sim_advance(&s,20));
    CHECK(s.core.safety.state==TYVOR_STARTUP && (s.core.safety.reasons&TYVOR_SAFE_THROTTLE));
    s.switches[0]=1;CHECK(!tyvor_sim_advance(&s,20));CHECK(s.core.safety.state!=TYVOR_BENCH_ARMED && s.core.channels[2]==-1000);return 1;
}
static int safety_startup_switch(void) {
    tyvor_sim_t s;CHECK(!tyvor_sim_init(&s));s.switches[0]=1;CHECK(!tyvor_sim_advance(&s,100));
    CHECK(s.core.safety.state==TYVOR_STARTUP && s.core.safety.throttle_lock);
    s.switches[0]=0;CHECK(!tyvor_sim_advance(&s,20));CHECK(s.core.safety.state==TYVOR_DISARMED);
    s.switches[0]=1;CHECK(!tyvor_sim_advance(&s,20));CHECK(s.core.safety.state==TYVOR_BENCH_ARMED);return 1;
}
static int safety_hold_switch(void) {
    tyvor_sim_t s;CHECK(!tyvor_sim_init(&s));CHECK(!tyvor_sim_advance(&s,20));
    s.axes[2]=1000;s.switches[0]=1;CHECK(!tyvor_sim_advance(&s,20));CHECK(s.core.safety.state==TYVOR_DISARMED);
    s.axes[2]=-1000;CHECK(!tyvor_sim_advance(&s,100));CHECK(s.core.safety.state==TYVOR_DISARMED);
    s.switches[0]=0;CHECK(!tyvor_sim_advance(&s,20));s.switches[0]=1;CHECK(!tyvor_sim_advance(&s,20));CHECK(s.core.safety.state==TYVOR_BENCH_ARMED);return 1;
}
static int safety_input_loss(void) {
    tyvor_sim_t s;CHECK(!armed(&s));s.axes[0]=1000;CHECK(!tyvor_sim_advance(&s,20));CHECK(s.core.channels[0]==1000);
    s.input_loss=1;CHECK(!tyvor_sim_advance(&s,150));
    CHECK(s.core.safety.state==TYVOR_FAILSAFE && (s.core.safety.reasons&TYVOR_SAFE_INPUT));
    CHECK(s.core.channels[0]==0 && s.core.channels[2]==-1000 && s.core.safety.throttle_lock);return 1;
}
static int safety_recovery(void) {
    tyvor_sim_t s;CHECK(!armed(&s));s.input_loss=1;CHECK(!tyvor_sim_advance(&s,150));
    s.input_loss=0;CHECK(!tyvor_sim_advance(&s,50));CHECK(s.core.safety.state==TYVOR_FAILSAFE);
    s.switches[0]=0;CHECK(!tyvor_sim_advance(&s,20));CHECK(s.core.safety.state==TYVOR_DISARMED);
    s.switches[0]=1;CHECK(!tyvor_sim_advance(&s,20));CHECK(s.core.safety.state==TYVOR_BENCH_ARMED);return 1;
}
static int safety_kill(void) {
    tyvor_sim_t s;CHECK(!armed(&s));s.switches[1]=2;CHECK(!tyvor_sim_advance(&s,20));
    CHECK(s.core.safety.state==TYVOR_FAILSAFE && (s.core.safety.reasons&TYVOR_SAFE_SWITCH));
    s.switches[1]=1;CHECK(!tyvor_sim_advance(&s,20));CHECK(s.core.safety.state==TYVOR_FAILSAFE);
    s.switches[0]=0;CHECK(!tyvor_sim_advance(&s,20));CHECK(s.core.safety.state==TYVOR_DISARMED);return 1;
}
static int safety_reset(void) {
    tyvor_sim_t s;CHECK(!armed(&s));CHECK(!tyvor_sim_reset_control(&s));
    CHECK(s.core.safety.state==TYVOR_STARTUP && s.core.channels[2]==-1000 && s.core.channels[0]==0);
    CHECK(!tyvor_sim_advance(&s,100));CHECK(s.core.safety.state==TYVOR_STARTUP && !s.core.faults);return 1;
}
static int safety_filter_bypass(void) {
    tyvor_core_t c;tyvor_model_t m;CHECK(!core_start(&c,&m));c.models.active.axes[2].filter_divisor=16;
    CHECK(!feed(&c,-1000,0,1,0));CHECK(tyvor_core_poll(&c,10)==1 && c.safety.state==TYVOR_DISARMED);
    CHECK(!feed(&c,-500,1,2,20));CHECK(c.inputs.filtered[2]<=-950 && c.inputs.immediate[2]>-950);
    CHECK(tyvor_core_poll(&c,20)==1 && c.safety.state==TYVOR_DISARMED);return 1;
}
static int scheduler_deterministic(void) {
    tyvor_core_t a,b;tyvor_model_t m;CHECK(!core_start(&a,&m));CHECK(!core_start(&b,&m));
    CHECK(!feed(&a,-1000,0,1,0));CHECK(!feed(&b,-1000,0,1,0));
    const uint64_t jitter[]={11,23,34,42,56,61,78,85,94};
    for(unsigned i=0;i<9;++i){CHECK(tyvor_core_poll(&a,(i+1)*10)==1);CHECK(tyvor_core_poll(&b,jitter[i])==1);}
    CHECK(a.ticks==9 && b.ticks==9 && a.next_tick_ms==100 && b.next_tick_ms==100 && b.last_tick_ms==90);
    CHECK(!a.faults && !b.faults && !memcmp(a.channels,b.channels,sizeof(a.channels)));
    CHECK(tyvor_core_poll(&b,99)==0 && b.ticks==9);return 1;
}
static int scheduler_overrun(void) {
    tyvor_core_t c;tyvor_model_t m;CHECK(!core_start(&c,&m));CHECK(!feed(&c,-1000,0,1,0));
    CHECK(tyvor_core_poll(&c,35)==1 && c.ticks==1 && c.missed_ticks==2 && c.next_tick_ms==40);
    CHECK(c.safety.state==TYVOR_FAILSAFE && (c.safety.reasons&TYVOR_SAFE_SCHEDULER));
    CHECK(tyvor_core_poll(&c,35)==0 && c.ticks==1);CHECK(tyvor_core_poll(&c,40)==1 && c.safety.state==TYVOR_FAILSAFE);return 1;
}
static int scheduler_clock(void) {
    tyvor_core_t c;tyvor_model_t m;CHECK(!core_start(&c,&m));CHECK(!feed(&c,-1000,0,1,0));CHECK(tyvor_core_poll(&c,10)==1);
    CHECK(tyvor_core_poll(&c,9)<0 && c.safety.state==TYVOR_FAILSAFE && (c.faults&TYVOR_SAFE_CLOCK));
    CHECK(c.channels[2]==-1000 && !c.inputs.valid);
    CHECK(tyvor_core_init(&c,&m,(tyvor_model_storage_t){0},UINT64_MAX)<0);return 1;
}
static int telemetry_values(void) {
    tyvor_telemetry_t t;tyvor_telemetry_init(&t);tyvor_telemetry_sample_t p=tyvor_bench_telemetry(0);
    CHECK(!tyvor_telemetry_feed(&t,&p,0));tyvor_telemetry_view_t v;tyvor_telemetry_view(&t,1,&v);
    CHECK(v.valid==15 && v.battery_mv==8100 && v.lq==99 && v.rssi_dbm==-60 && v.satellites==7 && v.age_ms[0]==1);
    p.present=1;p.values.valid=1;p.values.battery_mv=7900;p.sampled_ms=50;
    CHECK(!tyvor_telemetry_feed(&t,&p,50));CHECK(t.values.lq==99 && t.sampled_ms[1]==0 && t.values.battery_mv==7900);return 1;
}
static int telemetry_stale(void) {
    tyvor_telemetry_t t;tyvor_telemetry_init(&t);tyvor_telemetry_sample_t p=tyvor_bench_telemetry(0);CHECK(!tyvor_telemetry_feed(&t,&p,0));
    tyvor_telemetry_poll(&t,999);CHECK(t.values.valid==15);tyvor_telemetry_poll(&t,1000);CHECK(t.values.valid==8 && !t.values.battery_mv && !t.values.rssi_dbm);
    tyvor_telemetry_poll(&t,2000);CHECK(!t.values.valid && !t.values.satellites);
    p.sampled_ms=2001;CHECK(!tyvor_telemetry_feed(&t,&p,2001));tyvor_telemetry_poll(&t,2000);CHECK(!t.values.valid);return 1;
}
static int telemetry_invalid(void) {
    tyvor_telemetry_t t,before;tyvor_telemetry_init(&t);tyvor_telemetry_sample_t p=tyvor_bench_telemetry(0);CHECK(!tyvor_telemetry_feed(&t,&p,0));before=t;
    p.sampled_ms=1;p.values.lq=101;CHECK(tyvor_telemetry_feed(&t,&p,1)<0 && !memcmp(&t,&before,sizeof(t)));
    p.values.lq=99;p.values.longitude_e7=INT32_MAX;CHECK(tyvor_telemetry_feed(&t,&p,1)<0);
    p.values.longitude_e7=0;p.values.rssi_dbm=-151;CHECK(tyvor_telemetry_feed(&t,&p,1)<0);
    p.values.rssi_dbm=-60;p.sampled_ms=0;CHECK(tyvor_telemetry_feed(&t,&p,1)<0);
    p.sampled_ms=2;CHECK(tyvor_telemetry_feed(&t,&p,1)<0);
    p.sampled_ms=1;p.values.valid=0;CHECK(!tyvor_telemetry_feed(&t,&p,1) && !t.values.valid);return 1;
}
static void snapshot_fixture(tyvor_core_snapshot_t *v,uint32_t generation,uint64_t now) {
    tyvor_core_t c;tyvor_model_t m;(void)core_start(&c,&m);(void)feed(&c,-1000,0,1,0);
    tyvor_telemetry_sample_t sample=tyvor_bench_telemetry(0);(void)tyvor_core_telemetry(&c,&sample,0);
    (void)tyvor_core_poll(&c,10);tyvor_core_snapshot(&c,10,v);
    v->generation=generation;v->captured_ms=now;
}
static void cache_link(tyvor_link_t *l) {
    memset(l,0,sizeof(*l));l->ready=1;l->role=TYVOR_ROLE_UI;l->local_id=2;l->peer_id=1;l->local_epoch=1;l->peer_epoch=1;
}
static int cache_part(tyvor_ui_state_t *s,tyvor_link_t *l,const tyvor_core_snapshot_t *v,unsigned kind,uint64_t now) {
    uint8_t payload[32],packet[64];size_t n=tyvor_core_wire_encode(v,kind,now,payload,sizeof(payload));if(!n)return -1;
    tyvor_session_tag_t tag={l->peer_id,l->local_id,l->peer_epoch,l->local_epoch};
    size_t size=tyvor_encode(TYVOR_DIAGNOSTIC,0,&tag,payload,(uint8_t)n,packet,sizeof(packet));if(!size)return -1;
    tyvor_frame_t f;if(tyvor_decode(packet,size,&f))return -1;
    tyvor_ui_state_receive(s,l,&f,now);return 0;
}
static int cache_all(tyvor_ui_state_t *s,tyvor_link_t *l,const tyvor_core_snapshot_t *v,uint64_t now) {
    for(unsigned k=1;k<=5;++k)if(cache_part(s,l,v,k,now))return -1;
    return 0;
}
static int core_wire_roundtrip(void) {
    tyvor_core_snapshot_t a,b={0};snapshot_fixture(&a,UINT32_MAX,100);unsigned k;uint16_t age;
    for(unsigned part=1;part<=5;++part) {
        uint8_t bytes[34];memset(bytes,0xa5,sizeof(bytes));size_t n=tyvor_core_wire_encode(&a,part,110,bytes+1,32);
        CHECK(n && bytes[0]==0xa5 && bytes[33]==0xa5);CHECK(!tyvor_core_wire_decode(&b,bytes+1,n,&k,&age));CHECK(k==part && age==10);
    }
    CHECK(b.model_id==1 && !strcmp(b.model_name,a.model_name) && b.generation==UINT32_MAX && b.input_valid && b.safety_state==TYVOR_DISARMED);
    CHECK(!memcmp(b.channels,a.channels,sizeof(b.channels)) && b.telemetry.valid==15 && !b.rf_enabled);return 1;
}
static int core_wire_rejection(void) {
    tyvor_core_snapshot_t a,out,before;snapshot_fixture(&a,1,0);memset(&out,0xa5,sizeof(out));before=out;
    uint8_t b[32];unsigned kind=99;uint16_t age=99;size_t n=tyvor_core_wire_encode(&a,TYVOR_CORE_SAFETY,0,b,sizeof(b));CHECK(n==17);
    b[1]=99;CHECK(tyvor_core_wire_decode(&out,b,n,&kind,&age)<0 && !memcmp(&out,&before,sizeof(out)) && kind==99 && age==99);
    b[1]=TYVOR_CORE_WIRE_REV;b[9]=TYVOR_BENCH_ARMED;CHECK(tyvor_core_wire_decode(&out,b,n,&kind,&age)<0);
    a.rf_enabled=1;CHECK(!tyvor_core_wire_encode(&a,TYVOR_CORE_OUTPUTS,0,b,sizeof(b)));
    a.rf_enabled=0;CHECK(!tyvor_core_wire_encode(&a,TYVOR_CORE_MODEL,500,b,sizeof(b)));
    for(size_t cap=0;cap<32;++cap)CHECK(!tyvor_core_wire_encode(&a,TYVOR_CORE_MODEL,0,b,cap));return 1;
}
static int ui_atomic_snapshot(void) {
    tyvor_ui_state_t s;tyvor_ui_state_init(&s);tyvor_link_t l;cache_link(&l);tyvor_core_snapshot_t v;snapshot_fixture(&v,1,0);
    for(unsigned part=1;part<5;++part){CHECK(!cache_part(&s,&l,&v,part,0));CHECK(!s.valid);}
    CHECK(!cache_part(&s,&l,&v,5,0));CHECK(s.valid && s.commits==1 && s.snapshot.model_id==1 && s.snapshot.generation==1);return 1;
}
static int ui_mixed_generation(void) {
    tyvor_ui_state_t s;tyvor_ui_state_init(&s);tyvor_link_t l;cache_link(&l);tyvor_core_snapshot_t a,b;
    snapshot_fixture(&a,UINT32_MAX,0);b=a;b.generation=0;b.model_id=2;
    CHECK(!cache_part(&s,&l,&a,1,0));CHECK(!cache_part(&s,&l,&b,2,0));
    CHECK(!cache_part(&s,&l,&a,3,0));CHECK(!s.valid && s.rejected==1);
    CHECK(!cache_part(&s,&l,&b,1,0));CHECK(!cache_part(&s,&l,&b,3,0));CHECK(!cache_part(&s,&l,&b,4,0));CHECK(!cache_part(&s,&l,&b,5,0));
    CHECK(s.valid && s.snapshot.generation==0 && s.snapshot.model_id==2);return 1;
}
static int ui_duplicate_parts(void) {
    tyvor_ui_state_t s;tyvor_ui_state_init(&s);tyvor_link_t l;cache_link(&l);tyvor_core_snapshot_t a,b;
    snapshot_fixture(&a,1,0);b=a;b.model_id=2;
    CHECK(!cache_part(&s,&l,&a,1,0));CHECK(!cache_part(&s,&l,&b,1,10));CHECK(s.rejected==1 && s.pending.model_id==1);
    for(unsigned k=2;k<=5;++k)CHECK(!cache_part(&s,&l,&a,k,20));
    CHECK(s.valid && s.snapshot.model_id==1);uint64_t deadline=s.expires_ms;
    CHECK(!cache_all(&s,&l,&a,30));CHECK(s.expires_ms==deadline && s.commits==1);return 1;
}
static int ui_stale_no_revival(void) {
    tyvor_ui_state_t s;tyvor_ui_state_init(&s);tyvor_link_t l;cache_link(&l);tyvor_core_snapshot_t a;
    snapshot_fixture(&a,10,0);CHECK(!cache_all(&s,&l,&a,0) && s.valid);
    tyvor_ui_state_poll(&s,499);CHECK(s.valid);tyvor_ui_state_poll(&s,500);CHECK(!s.valid && !s.snapshot.model_id && !s.snapshot.telemetry.valid);
    a.captured_ms=500;CHECK(!cache_all(&s,&l,&a,500) && !s.valid);
    a.generation=9;CHECK(!cache_all(&s,&l,&a,500) && !s.valid);
    a.generation=11;CHECK(!cache_all(&s,&l,&a,500) && s.valid);return 1;
}
static int ui_partial_timeout(void) {
    tyvor_ui_state_t s;tyvor_ui_state_init(&s);tyvor_link_t l;cache_link(&l);tyvor_core_snapshot_t a;snapshot_fixture(&a,1,0);
    CHECK(!cache_part(&s,&l,&a,1,0));tyvor_ui_state_poll(&s,300);CHECK(!s.parts && s.partial_timeouts==1);
    a.captured_ms=300;CHECK(!cache_all(&s,&l,&a,300) && !s.valid);
    a.generation=2;CHECK(!cache_all(&s,&l,&a,300) && s.valid);return 1;
}
static int ui_source_age(void) {
    tyvor_ui_state_t s;tyvor_ui_state_init(&s);tyvor_link_t l;cache_link(&l);tyvor_core_snapshot_t a;snapshot_fixture(&a,1,0);
    a.telemetry.age_ms[0]=990;a.input_age_ms=490;
    CHECK(!cache_all(&s,&l,&a,20) && s.valid);
    CHECK(!s.snapshot.input_valid && !(s.snapshot.telemetry.valid&1));
    /* A delayed parser callback must not refresh an expired generation. */
    tyvor_ui_state_poll(&s,1000);a.generation=2;a.captured_ms=100;
    CHECK(!cache_all(&s,&l,&a,100));CHECK(!s.valid);return 1;
}
static int ui_session_recovery(void) {
    tyvor_sim_t s;CHECK(!tyvor_sim_init(&s));CHECK(!tyvor_sim_advance(&s,250) && s.ui.state.valid);
    uint64_t previous=s.ui.link.peer_id;CHECK(!tyvor_sim_reset_control(&s));CHECK(!tyvor_sim_advance(&s,500));
    CHECK(s.ui.state.valid && s.ui.link.peer_id!=previous && s.ui.state.snapshot.model_id==1);
    uint32_t old_epoch=s.ui.state.peer_epoch;s.ui.link.peer_epoch++;tyvor_ui_state_sync(&s.ui.state,&s.ui.link,s.now);
    CHECK(!s.ui.state.valid && s.ui.state.peer_epoch!=old_epoch);return 1;
}
static int ui_reset_independence(void) {
    tyvor_sim_t s;CHECK(!armed(&s));tyvor_core_t before=s.core;CHECK(!tyvor_sim_reset_ui(&s));
    CHECK(!memcmp(&before,&s.core,sizeof(before)) && !s.ui.state.valid);
    CHECK(!tyvor_sim_advance(&s,500) && s.ui.state.valid && s.core.safety.state==TYVOR_BENCH_ARMED);
    s.control_to_ui.drop=1;s.ui_to_control.drop=1;CHECK(!tyvor_sim_advance(&s,6000));
    CHECK(s.core.safety.state==TYVOR_BENCH_ARMED && s.core.ticks==s.now/10 && !s.ui.state.valid);return 1;
}
static int ui_authority(void) {
    tyvor_sim_t s;CHECK(!tyvor_sim_init(&s));CHECK(!tyvor_sim_advance(&s,200));tyvor_core_t before=s.core;
    CHECK(tyvor_ui_endpoint_request(&s.ui,99,0)<0);
    const uint8_t commands[][4]={{'A','R','M',1},{'R','F',1,0},{'O','U','T',1},{'M','O','D',2}};
    for(unsigned i=0;i<4;++i) {
        CHECK(!tyvor_link_send(&s.ui.link,TYVOR_DIAGNOSTIC,commands[i],4));
        tyvor_sim_wire_t *w=&s.ui_to_control;CHECK(w->count);
        for(unsigned j=0;j<w->count;++j)CHECK(!tyvor_core_bridge_feed(&s.bridge,w->bytes[j],w->length[j],s.now));w->count=0;
    }
    CHECK(!memcmp(&before,&s.core,sizeof(before)) && s.core.safety.state==TYVOR_DISARMED);
    const uint8_t status[6]={0,0,0,0,0,4};CHECK(tyvor_link_send(&s.ui.link,TYVOR_STATUS,status,6)<0);
    /* Even a manually crafted CRC-valid STATUS is rejected on CONTROL RX. */
    uint8_t b[64];tyvor_session_tag_t tag={s.ui.link.local_id,s.ui.link.peer_id,s.ui.link.local_epoch,s.ui.link.peer_epoch};
    size_t n=tyvor_encode(TYVOR_STATUS,0,&tag,status,6,b,sizeof(b));CHECK(n && !tyvor_core_bridge_feed(&s.bridge,b,n,s.now));
    CHECK(s.bridge.transport.link.stats.wrong_direction && !memcmp(&before,&s.core,sizeof(before)));return 1;
}
static int ui_read_ack(void) {
    tyvor_sim_t s;CHECK(!tyvor_sim_init(&s));CHECK(!tyvor_sim_advance(&s,200));tyvor_core_t before=s.core;
    CHECK(!tyvor_ui_endpoint_request(&s.ui,TYVOR_REQ_MODEL_INFO,0));tyvor_sim_wire_t *w=&s.ui_to_control;
    for(unsigned i=0;i<w->count;++i)CHECK(!tyvor_core_bridge_feed(&s.bridge,w->bytes[i],w->length[i],s.now));w->count=0;
    CHECK(!memcmp(&s.core,&before,sizeof(before)) && s.bridge.read_requests==1);
    CHECK(!tyvor_sim_advance(&s,1));CHECK(s.ui.state.ack_valid && s.ui.state.ack_result==0 && s.ui.state.ack_opcode==1);
    CHECK(tyvor_ui_endpoint_request(&s.ui,TYVOR_REQ_PAGE,7)<0);return 1;
}
static int ui_renderer(void) {
    tyvor_sim_t s;CHECK(!tyvor_sim_init(&s));tyvor_ui_view_t v;
    CHECK(!tyvor_ui_core_view(0,&s.ui.state,&v) && strstr(v.banner,"DISCONNECTED") && !strcmp(v.rows[1],"UNKNOWN"));
    CHECK(!tyvor_sim_advance(&s,250));
    for(unsigned page=0;page<7;++page)CHECK(!tyvor_ui_core_view(page,&s.ui.state,&v) && v.title && strstr(v.banner,"BENCH"));
    CHECK(!tyvor_ui_core_view(2,&s.ui.state,&v) && strstr(v.rows[2],"-1000"));
    CHECK(!tyvor_ui_core_view(0,&s.ui.state,&v) && strstr(v.rows[5],"UNKNOWN"));
    CHECK(tyvor_ui_core_view(7,&s.ui.state,&v)<0);return 1;
}
static int ui_telemetry_recovery(void) {
    for(unsigned phase=0;phase<TYVOR_READ_PERIOD_MS;++phase) {
        tyvor_sim_t s;CHECK(!tyvor_sim_init(&s));CHECK(!tyvor_sim_advance(&s,200));
        s.telemetry_loss=1;CHECK(!tyvor_sim_advance(&s,2300+phase));CHECK(!s.core.telemetry.values.valid);
        s.telemetry_loss=0;CHECK(!tyvor_sim_advance(&s,400));
        CHECK(s.ui.state.valid && s.ui.state.snapshot.telemetry.valid==15);
    }
    return 1;
}
static int safety_invalid_model(void) {
    tyvor_sim_t s;CHECK(!armed(&s));s.core.models.active.channel_map[0]=8;
    CHECK(!tyvor_sim_advance(&s,20));CHECK(s.core.safety.state==TYVOR_FAILSAFE && (s.core.safety.reasons&TYVOR_SAFE_MODEL));
    CHECK(s.core.channels[2]==-1000 && !tyvor_core_rf_enabled());return 1;
}
static int safety_reset_corrupt_storage(void) {
    tyvor_sim_t s;CHECK(!armed(&s));s.storage.bytes[0][12]^=1;
    CHECK(!tyvor_sim_reset_control(&s));CHECK(s.core.safety.state==TYVOR_FAILSAFE && !s.core.models.valid && s.core.channels[2]==-1000);
    CHECK(!tyvor_sim_advance(&s,100));CHECK(s.core.safety.state==TYVOR_FAILSAFE && (s.core.faults&TYVOR_SAFE_MODEL));return 1;
}
static int core_no_rf(void) {
    tyvor_sim_t s;CHECK(!armed(&s));s.axes[2]=1000;CHECK(!tyvor_sim_advance(&s,200));
    CHECK(s.core.channels[2]==1000 && !tyvor_core_rf_enabled() && s.ui.state.valid && !s.ui.state.snapshot.rf_enabled);
    CHECK(s.ui.link.snapshot.valid && !s.ui.link.snapshot.status.arm_valid && !s.ui.link.snapshot.status.arm && (s.ui.link.snapshot.status.flags&TYVOR_SOURCE_MOCK));return 1;
}
static uint32_t random_next(uint32_t *s) {*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return *s;}
static int core_randomized_models(void) {
    uint32_t seed=UINT32_C(0x2a407);tyvor_model_t m,out;uint8_t b[256],again[256];
    for(unsigned iteration=0;iteration<1000;++iteration) {
        tyvor_model_default(&m,iteration+1);m.mix_count=(uint8_t)(1+random_next(&seed)%16);
        for(unsigned i=0;i<m.mix_count;++i)m.mixes[i]=(tyvor_mix_t){(uint8_t)(random_next(&seed)%6),(uint8_t)(random_next(&seed)%8),
            (uint8_t)(random_next(&seed)%2),(int16_t)((int)(random_next(&seed)%401)-200),
            (int16_t)((int)(random_next(&seed)%2001)-1000),(int16_t)((int)(random_next(&seed)%1001)-500)};
        size_t n=tyvor_model_serialize(&m,b,sizeof(b));CHECK(n && !tyvor_model_deserialize(&out,b,n));
        CHECK(tyvor_model_serialize(&out,again,sizeof(again))==n && !memcmp(b,again,n));
        int16_t inputs[6],channels[8];for(unsigned i=0;i<6;++i)inputs[i]=(int16_t)((int)(random_next(&seed)%2001)-1000);
        CHECK(!tyvor_mixer_run(&out,inputs,channels));for(unsigned i=0;i<8;++i)CHECK(channels[i]>=-1000 && channels[i]<=1000);
        b[random_next(&seed)%n]^=1;CHECK(tyvor_model_deserialize(&out,b,n)<0);
    }
    puts("MODEL seed=0002a407 iterations=1000 PASS");return 1;
}
static int core_safety_randomized(void) {
    uint32_t seed=UINT32_C(0x5afe2a);tyvor_sim_t s;CHECK(!tyvor_sim_init(&s));
    for(unsigned step=0;step<1000;++step) {
        s.axes[2]=(int16_t)((random_next(&seed)&1)?-1000:1000);s.switches[0]=(int8_t)(random_next(&seed)&1);
        s.switches[1]=(int8_t)(random_next(&seed)%3);s.input_loss=(uint8_t)(random_next(&seed)%5==0);
        if(random_next(&seed)%50==0)CHECK(!tyvor_sim_reset_control(&s));
        CHECK(!tyvor_sim_advance(&s,20));CHECK(!tyvor_core_rf_enabled());
        for(unsigned i=0;i<8;++i)CHECK(s.core.channels[i]>=-1000 && s.core.channels[i]<=1000);
        if(s.core.safety.state!=TYVOR_BENCH_ARMED)CHECK(!memcmp(s.core.channels,s.core.models.active.failsafe,sizeof(s.core.channels)) && s.core.safety.throttle_lock);
        if(s.core.safety.state==TYVOR_BENCH_ARMED)CHECK(s.core.inputs.valid && s.core.inputs.switches[0]==1 && s.core.inputs.switches[1]!=2 && !s.core.faults);
    }
    puts("SAFETY seed=005afe2a iterations=1000 PASS");return 1;
}
typedef struct {const char *name;int (*run)(void);} test_case_t;
#define CASE(x) {#x,x}
static const test_case_t tests[]={
    CASE(input_calibration),CASE(input_deadband_filter),CASE(input_invalid),CASE(input_timeout),CASE(input_sequence),
    CASE(mixer_weights),CASE(mixer_ordering),CASE(mixer_routing),CASE(mixer_clamp_overflow),
    CASE(model_roundtrip),CASE(model_crc),CASE(model_validation),CASE(model_version),CASE(model_storage),CASE(model_atomic_failure),CASE(model_switch_guard),
    CASE(safety_high_throttle),CASE(safety_startup_switch),CASE(safety_hold_switch),CASE(safety_input_loss),CASE(safety_recovery),CASE(safety_kill),CASE(safety_reset),CASE(safety_filter_bypass),
    CASE(scheduler_deterministic),CASE(scheduler_overrun),CASE(scheduler_clock),CASE(telemetry_values),CASE(telemetry_stale),CASE(telemetry_invalid),
    CASE(core_wire_roundtrip),CASE(core_wire_rejection),CASE(ui_atomic_snapshot),CASE(ui_mixed_generation),CASE(ui_duplicate_parts),CASE(ui_stale_no_revival),CASE(ui_partial_timeout),CASE(ui_source_age),
    CASE(ui_session_recovery),CASE(ui_reset_independence),CASE(ui_authority),CASE(ui_read_ack),CASE(ui_renderer),CASE(ui_telemetry_recovery),CASE(safety_invalid_model),CASE(safety_reset_corrupt_storage),CASE(core_no_rf),CASE(core_randomized_models),CASE(core_safety_randomized)
};
int main(int argc,char **argv) {
    unsigned passed=0,failed=0;
    for(unsigned i=0;i<sizeof(tests)/sizeof(tests[0]);++i) {
        if(argc==2 && strcmp(argv[1],tests[i].name))continue;
        int ok=tests[i].run();printf("%s %s\n",ok?"PASS":"FAIL",tests[i].name);if(ok)++passed;else ++failed;
    }
    printf("%u PASS / %u FAIL; software-only BENCH; RF disabled\n",passed,failed);return !passed || failed?1:0;
}
