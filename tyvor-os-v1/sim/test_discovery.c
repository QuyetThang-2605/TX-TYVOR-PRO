#include "discovery_demo.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d FAIL %s\n",__FILE__,__LINE__,#x);return 0;}}while(0)
static int step(discovery_demo_t *d,uint64_t until,int button) {
    for(uint64_t t=d->last_poll_ms+1;t<=until;++t)if(discovery_demo_poll(d,t,button)<0)return -1;
    return 0;
}
static int press(discovery_demo_t *d) {
    uint64_t t=d->last_poll_ms;
    return step(d,t+40,1) || step(d,t+80,0)?-1:0;
}
static int mode(discovery_demo_t *d,unsigned wanted) {
    if(discovery_demo_init(d,0,0))return -1;
    for(unsigned i=0;i<wanted;++i)if(press(d))return -1;
    return d->mode==wanted?0:-1;
}
static int discovery_boot_safe(void) {discovery_demo_t d;CHECK(!mode(&d,0));CHECK(!step(&d,500,0));CHECK(d.core.safety.state==TYVOR_DISARMED && d.core.channels[2]==-1000 && !d.faults && d.mode_changes==0);return 1;}
static int discovery_selftest(void) {discovery_demo_t d;CHECK(!discovery_demo_selftest() && !mode(&d,0) && d.selftest_mask==15);return 1;}
static int discovery_button_debounce(void) {
    discovery_demo_t d;CHECK(!mode(&d,0));CHECK(!step(&d,10,1));CHECK(!step(&d,20,0));CHECK(!step(&d,30,1));CHECK(!step(&d,40,0));CHECK(!d.mode_changes);
    CHECK(!step(&d,70,1));CHECK(!d.mode_changes);CHECK(!step(&d,71,1));CHECK(d.mode_changes==1);return 1;
}
static int discovery_button_held_boot(void) {discovery_demo_t d;CHECK(!discovery_demo_init(&d,0,1));CHECK(!step(&d,2000,1));CHECK(d.mode==0 && !d.mode_changes && (d.leds&DISCOVERY_BLUE));return 1;}
static int discovery_button_long_hold(void) {discovery_demo_t d;CHECK(!mode(&d,0));CHECK(!step(&d,5000,1));CHECK(d.mode_changes==1 && d.mode==1);return 1;}
static int discovery_button_release(void) {discovery_demo_t d;CHECK(!discovery_demo_init(&d,0,1));CHECK(!step(&d,100,0));CHECK(!press(&d));CHECK(d.mode_changes==1);return 1;}
static int discovery_mode_cycle(void) {discovery_demo_t d;CHECK(!mode(&d,0));for(unsigned i=0;i<12;++i){CHECK(!press(&d));CHECK(d.mode==(i+1)%4);}return 1;}
static int discovery_mode_safe_reset(void) {discovery_demo_t d;CHECK(!mode(&d,1));CHECK(!step(&d,d.mode_started_ms+700,0));CHECK(d.core.safety.state==TYVOR_BENCH_ARMED);CHECK(!press(&d));CHECK(d.mode==2 && d.core.safety.state!=TYVOR_BENCH_ARMED && d.core.channels[2]==-1000);return 1;}
static int discovery_mixer_sweep(void) {
    discovery_demo_t d;CHECK(!mode(&d,1));uint64_t end=d.mode_started_ms+9000;int minimum=1000,maximum=-1000;
    for(uint64_t t=d.last_poll_ms+1;t<=end;++t){CHECK(discovery_demo_poll(&d,t,0)>=0);if(d.core.safety.state==TYVOR_BENCH_ARMED){if(d.core.channels[0]<minimum)minimum=d.core.channels[0];if(d.core.channels[0]>maximum)maximum=d.core.channels[0];}for(unsigned ch=0;ch<8;++ch)CHECK(d.core.channels[ch]>=-1000 && d.core.channels[ch]<=1000);}
    CHECK(minimum<-800 && maximum>800 && !d.core.missed_ticks && !d.faults);return 1;
}
static int discovery_input_loss(void) {discovery_demo_t d;CHECK(!mode(&d,2));CHECK(!step(&d,d.mode_started_ms+1150,0));CHECK(d.core.safety.state==TYVOR_FAILSAFE && d.core.safety.throttle_lock && d.core.channels[2]==-1000 && (d.leds&DISCOVERY_RED));return 1;}
static int discovery_loss_recovery(void) {discovery_demo_t d;CHECK(!mode(&d,2));CHECK(!step(&d,d.mode_started_ms+1600,0));CHECK(d.core.safety.state==TYVOR_FAILSAFE);CHECK(!step(&d,d.mode_started_ms+1900,0));CHECK(d.core.safety.state==TYVOR_DISARMED);CHECK(!step(&d,d.mode_started_ms+2100,0));CHECK(d.core.safety.state==TYVOR_BENCH_ARMED);return 1;}
static int discovery_model_selection(void) {discovery_demo_t d;CHECK(!mode(&d,3));CHECK(!step(&d,d.mode_started_ms+2200,0));CHECK(d.core.models.active.id==2 && d.model_changes==1 && d.core.safety.state==TYVOR_DISARMED);CHECK(!step(&d,d.mode_started_ms+4200,0));CHECK(d.core.models.active.id==1 && d.model_changes==2 && !d.faults);return 1;}
static int discovery_model_fault(void) {
    discovery_demo_t d;CHECK(!mode(&d,3));uint64_t start=d.mode_started_ms;
    CHECK(!step(&d,start+1969,0));CHECK(!step(&d,start+1999,1));d.storage.fail_read=1;
    CHECK(discovery_demo_poll(&d,start+2000,1)<0);
    CHECK((d.faults&DISCOVERY_FAULT_MODEL) && d.mode==DISCOVERY_MODEL_TELEMETRY && d.mode_changes==3 && d.leds==DISCOVERY_RED);return 1;
}
static int discovery_telemetry_expiry(void) {discovery_demo_t d;CHECK(!mode(&d,3));CHECK(!step(&d,d.mode_started_ms+3100,0));CHECK(!d.core.telemetry.values.valid);CHECK(!step(&d,d.mode_started_ms+4200,0));CHECK(d.core.telemetry.values.valid==15);return 1;}
static int discovery_clock_fault(void) {discovery_demo_t d;CHECK(!mode(&d,0));CHECK(!step(&d,100,0));CHECK(discovery_demo_poll(&d,99,1)<0);CHECK(d.faults&DISCOVERY_FAULT_CLOCK);CHECK(d.core.safety.state==TYVOR_FAILSAFE);return 1;}
static int discovery_scheduler_fault(void) {discovery_demo_t d;CHECK(!mode(&d,0));CHECK(discovery_demo_poll(&d,100,0)<0);CHECK(d.faults && d.core.missed_ticks && d.core.safety.state==TYVOR_FAILSAFE);return 1;}
static int discovery_fault_button(void) {discovery_demo_t d;CHECK(!mode(&d,0));CHECK(!step(&d,1,1));CHECK(discovery_demo_poll(&d,100,1)<0);CHECK(!d.mode_changes);CHECK(discovery_demo_poll(&d,200,1)<0 && !d.mode_changes && d.leds==DISCOVERY_RED);return 1;}
static int discovery_leds(void) {discovery_demo_t d;CHECK(!mode(&d,0));CHECK(!step(&d,510,0));CHECK(d.leds&DISCOVERY_GREEN);CHECK(!step(&d,550,1));CHECK(d.leds&DISCOVERY_BLUE);CHECK(!step(&d,590,0));CHECK(!(d.leds&DISCOVERY_BLUE));CHECK(!(d.leds&~DISCOVERY_LED_MASK));return 1;}
static int discovery_reset(void) {discovery_demo_t d;CHECK(!mode(&d,1));CHECK(!step(&d,d.mode_started_ms+700,0));CHECK(d.core.safety.state==TYVOR_BENCH_ARMED);CHECK(!discovery_demo_init(&d,10000,1));CHECK(d.mode==0 && d.core.safety.state==TYVOR_STARTUP && d.core.channels[2]==-1000 && !d.mode_changes);return 1;}
static int discovery_no_rf(void) {discovery_demo_t d;for(unsigned i=0;i<4;++i){CHECK(!mode(&d,i));CHECK(!step(&d,d.mode_started_ms+7000,0));tyvor_core_snapshot_t s;tyvor_core_snapshot(&d.core,d.last_poll_ms,&s);CHECK(!s.rf_enabled && !tyvor_core_rf_enabled());}return 1;}
static int discovery_deterministic(void) {
    discovery_demo_t a,b;CHECK(!mode(&a,0) && !mode(&b,0));uint32_t seed=0xd15c0407;int button=0;
    for(unsigned i=1;i<20000;++i) {
        if(i%80==1){seed=seed*1664525u+1013904223u;button=(int)(seed>>31);}
        CHECK(discovery_demo_poll(&a,i,button)>=0 && discovery_demo_poll(&b,i,button)>=0);
        /* Storage callback contexts correctly point at different instances.
           Compare observable state and value-owned records, not pointer bytes. */
        CHECK(a.mode==b.mode && a.mode_changes==b.mode_changes && a.model_changes==b.model_changes && a.leds==b.leds);
        CHECK(!memcmp(&a.core.inputs,&b.core.inputs,sizeof(a.core.inputs)));
        CHECK(!memcmp(&a.core.safety,&b.core.safety,sizeof(a.core.safety)));
        CHECK(!memcmp(&a.core.models.active,&b.core.models.active,sizeof(a.core.models.active)));
        CHECK(!memcmp(a.core.channels,b.core.channels,sizeof(a.core.channels)));
        CHECK(!memcmp(&a.core.telemetry,&b.core.telemetry,sizeof(a.core.telemetry)));
    }
    CHECK(a.mode_changes>10);return 1;
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
#define CASE(x) if(!strcmp(argv[1],#x))return x()?0:1
    CASE(discovery_boot_safe);CASE(discovery_selftest);CASE(discovery_button_debounce);CASE(discovery_button_held_boot);
    CASE(discovery_button_long_hold);CASE(discovery_button_release);CASE(discovery_mode_cycle);CASE(discovery_mode_safe_reset);
    CASE(discovery_mixer_sweep);CASE(discovery_input_loss);CASE(discovery_loss_recovery);CASE(discovery_model_selection);
    CASE(discovery_model_fault);CASE(discovery_telemetry_expiry);CASE(discovery_clock_fault);CASE(discovery_scheduler_fault);CASE(discovery_fault_button);
    CASE(discovery_leds);CASE(discovery_reset);CASE(discovery_no_rf);CASE(discovery_deterministic);return 2;
}
