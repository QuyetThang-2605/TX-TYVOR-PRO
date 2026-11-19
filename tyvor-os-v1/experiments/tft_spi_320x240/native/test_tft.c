#include "mock_display.h"
#include "tft_lvgl.h"
#include "touch_adapter.h"
#include "tyvor_sim_runtime.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d FAIL %s\n",__FILE__,__LINE__,#x);return 0;}}while(0)
static tyvor_tft_mock_t mock;
static tyvor_display_pipeline_t pipe;
static tyvor_display_config_t config;
static tyvor_tft_lvgl_t ui;
static uint8_t buffer[TYVOR_DISPLAY_BUFFER_BYTES];
static tyvor_display_region_t region={0,0,4,1};
static uint8_t pixels[]={0xf8,0,0x07,0xe0,0,0x1f,0xff,0xff};
static int setup(void) {return tyvor_display_config(&config,0) || (tyvor_tft_mock_init(&mock,&config),tyvor_display_pipeline_init(&pipe,&config,tyvor_tft_mock_backend(&mock)));}
static int tft_config(void) {for(unsigned r=0;r<4;++r){CHECK(!tyvor_display_config(&config,r));CHECK(config.width==(r%2?320:240) && config.height==(r%2?240:320) && tyvor_display_config_valid(&config));}CHECK(tyvor_display_config(&config,4)<0);CHECK(!tyvor_display_config(&config,0));config.profile=2;CHECK(!tyvor_display_config_valid(&config));return 1;}
static tyvor_display_pins_t fixture_pins(void) {return (tyvor_display_pins_t){12,11,13,10,9,8,7,1,1,1};}
static int tft_pins_disabled(void) {tyvor_display_pins_t p={-1,-1,-1,-1,-1,-1,-1,0,0,0};CHECK(tyvor_display_pin_policy(&p)==TYVOR_DISPLAY_DISABLED);return 1;}
static int tft_pins_evidence(void) {tyvor_display_pins_t p=fixture_pins();CHECK(!tyvor_display_pin_policy(&p));p.board_verified=0;CHECK(tyvor_display_pin_policy(&p)<0);p.board_verified=1;p.module_verified=0;CHECK(tyvor_display_pin_policy(&p)<0);return 1;}
static int tft_pins_reserved(void) {int excluded[]={0,3,17,18,19,20,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,39,40,41,42,43,44,45,46,47,48};for(unsigned i=0;i<sizeof(excluded)/sizeof(excluded[0]);++i){tyvor_display_pins_t p=fixture_pins();p.mosi=excluded[i];CHECK(tyvor_display_pin_policy(&p)<0);}return 1;}
static int tft_pins_duplicates(void) {tyvor_display_pins_t p=fixture_pins();p.dc=p.cs;CHECK(tyvor_display_pin_policy(&p)<0);p=fixture_pins();p.miso=-1;p.backlight=-1;CHECK(!tyvor_display_pin_policy(&p));p.reset=-1;CHECK(tyvor_display_pin_policy(&p)<0);return 1;}
static int tft_region_bounds(void) {CHECK(!setup());CHECK(tyvor_display_region_valid(&config,&region,8));tyvor_display_region_t r={0,0,240,320};CHECK(!tyvor_display_region_valid(&config,&r,153600));r=(tyvor_display_region_t){-1,0,3,1};CHECK(!tyvor_display_region_valid(&config,&r,8));r=(tyvor_display_region_t){0,0,241,1};CHECK(!tyvor_display_region_valid(&config,&r,482));r=(tyvor_display_region_t){0,0,0,1};CHECK(!tyvor_display_region_valid(&config,&r,0));CHECK(!tyvor_display_region_valid(&config,&region,7));return 1;}
static int tft_rgb565(void) {uint16_t native[]={0xf800,0x07e0,0x001f,0xffff};tyvor_rgb565_wire((uint8_t *)native,4);CHECK(!memcmp(native,pixels,8));CHECK(!setup());CHECK(!tyvor_display_begin(&pipe,&region,pixels,8));CHECK(!tyvor_display_complete(&pipe));CHECK(mock.framebuffer[0]==0xf800 && mock.framebuffer[1]==0x07e0 && mock.framebuffer[2]==0x001f);return 1;}
static int tft_dma_complete(void) {CHECK(!setup());CHECK(!tyvor_display_begin(&pipe,&region,pixels,8));CHECK(pipe.busy && pipe.owned_buffer==pixels && mock.active==pixels && !pipe.transfers);CHECK(!tyvor_display_complete(&pipe));CHECK(!pipe.busy && !pipe.owned_buffer && !mock.active && pipe.transfers==1 && pipe.pixels==4 && pipe.last_transfer_us==18);return 1;}
static int tft_dma_busy(void) {CHECK(!setup());CHECK(!tyvor_display_begin(&pipe,&region,pixels,8));CHECK(tyvor_display_begin(&pipe,&region,pixels,8)==TYVOR_DISPLAY_BUSY && mock.starts==1);CHECK(tyvor_display_rotate(&pipe,1)==TYVOR_DISPLAY_BUSY);return 1;}
static int tft_dma_timeout(void) {CHECK(!setup());mock.wait_error=1;CHECK(!tyvor_display_begin(&pipe,&region,pixels,8));CHECK(tyvor_display_complete(&pipe)<0);CHECK(pipe.fault && pipe.busy && pipe.owned_buffer==pixels && pipe.timeouts==1 && !pipe.transfers);CHECK(tyvor_display_begin(&pipe,&region,pixels,8)<0 && mock.starts==1);return 1;}
static int tft_dma_start_error(void) {CHECK(!setup());mock.start_error=1;CHECK(tyvor_display_begin(&pipe,&region,pixels,8)<0);CHECK(pipe.fault && pipe.busy && pipe.owned_buffer==pixels && !pipe.transfers);return 1;}
static int tft_dma_clock(void) {CHECK(!setup());mock.clock_rollback=1;CHECK(!tyvor_display_begin(&pipe,&region,pixels,8));CHECK(tyvor_display_complete(&pipe)<0 && pipe.fault && pipe.busy && !pipe.transfers);return 1;}
static int tft_rotate(void) {CHECK(!setup());for(unsigned r=0;r<4;++r){CHECK(!tyvor_display_rotate(&pipe,r));CHECK(pipe.config.rotation==r && mock.width==pipe.config.width);}CHECK(mock.rotations==4 && tyvor_display_rotate(&pipe,4)<0);return 1;}
static int project(unsigned scenario,uint64_t now,int sim,tyvor_tft_view_t *v) {tyvor_ui_state_t s;tyvor_tft_fixture(&s,1000,scenario);return tyvor_tft_project(&s,now,sim,65000,v);}
static int tft_valid_safe(void) {tyvor_tft_view_t v;CHECK(!project(0,1000,1,&v));CHECK(v.quality==TYVOR_TFT_VALID && v.safe && v.battery_valid && v.lq_valid && strstr(v.battery,"12.24") && strstr(v.aircraft_arm,"UNKNOWN"));CHECK(!project(3,1000,1,&v));CHECK(!v.safe && strstr(v.safety,"BENCH ARMED") && strstr(v.aircraft_arm,"UNKNOWN"));return 1;}
static int tft_stale(void) {tyvor_tft_view_t v;CHECK(!project(0,1500,1,&v));CHECK(v.quality==TYVOR_TFT_STALE && !v.battery_valid && !v.timer_valid && !v.safe && !strcmp(v.model,"--"));return 1;}
static int tft_disconnected(void) {tyvor_tft_view_t v;CHECK(!project(2,1000,1,&v));CHECK(v.quality==TYVOR_TFT_DISCONNECTED && !v.battery_valid && !v.gps_valid && !v.safe && !v.timer_valid);return 1;}
static int tft_telemetry_expiry(void) {tyvor_ui_state_t s;tyvor_tft_view_t v;tyvor_tft_fixture(&s,1000,0);s.telemetry_expires_ms[0]=1000;s.telemetry_expires_ms[1]=1000;CHECK(!tyvor_tft_project(&s,1000,1,0,&v));CHECK(v.quality==TYVOR_TFT_VALID && !v.battery_valid && !v.lq_valid && v.rssi_valid);return 1;}
static int tft_live_timer_unknown(void) {tyvor_tft_view_t v;CHECK(!project(0,1000,0,&v));CHECK(!v.mock && !v.timer_valid && strstr(v.flight_timer,"--:--"));return 1;}
static int tft_clock_regression(void) {tyvor_tft_view_t v;CHECK(!project(0,999,1,&v));CHECK(v.quality==TYVOR_TFT_STALE && !v.safe && !v.battery_valid);return 1;}
static int tft_invalid_safety(void) {tyvor_ui_state_t s;tyvor_tft_view_t v;tyvor_tft_fixture(&s,1000,0);s.snapshot.safety_state=255;CHECK(!tyvor_tft_project(&s,1000,1,0,&v));CHECK(v.quality==TYVOR_TFT_STALE && !v.safe && !v.battery_valid);return 1;}
static unsigned touch_reads;static tyvor_touch_sample_t touch_sample;
static int touch_read(void *ctx,tyvor_touch_sample_t *s) {(void)ctx;++touch_reads;*s=touch_sample;return 0;}
static int tft_touch_disabled(void) {tyvor_touch_adapter_t a;tyvor_touch_sample_t s;CHECK(tyvor_touch_adapter_init(&a,0,touch_read,NULL,240,320)==1);CHECK(tyvor_touch_adapter_read(&a,10,&s)==1 && !touch_reads && !s.pressed);return 1;}
static int tft_touch_validation(void) {tyvor_touch_adapter_t a;tyvor_touch_sample_t s;CHECK(!tyvor_touch_adapter_init(&a,1,touch_read,NULL,240,320));touch_sample=(tyvor_touch_sample_t){239,319,1,10};CHECK(!tyvor_touch_adapter_read(&a,10,&s) && s.pressed);touch_sample.x=240;CHECK(tyvor_touch_adapter_read(&a,11,&s)<0 && !s.pressed);touch_sample.x=0;touch_sample.sampled_ms=12;CHECK(tyvor_touch_adapter_read(&a,11,&s)<0);touch_sample.sampled_ms=0;CHECK(tyvor_touch_adapter_read(&a,100,&s)<0);return 1;}
static int lv_setup(void) {if(setup())return -1;lv_init();return tyvor_tft_lvgl_init(&ui,&pipe,buffer,sizeof(buffer));}
static int tft_render_home(void) {CHECK(!lv_setup());tyvor_tft_view_t v;CHECK(!project(0,1000,1,&v));tyvor_tft_lvgl_update(&ui,&v);tyvor_tft_lvgl_frame(&ui);CHECK(pipe.transfers>1 && pipe.pixels==240u*320u && !pipe.busy);CHECK(!strcmp(lv_label_get_text(ui.labels[1]),"TYVOR DEMO"));CHECK(strstr(lv_label_get_text(ui.labels[5]),"UNKNOWN"));return 1;}
static int tft_render_partial(void) {CHECK(!lv_setup());tyvor_tft_view_t v;CHECK(!project(0,1000,1,&v));tyvor_tft_lvgl_update(&ui,&v);tyvor_tft_lvgl_frame(&ui);uint64_t before=pipe.pixels;(void)snprintf(v.lq,sizeof(v.lq),"LQ 70%%");tyvor_tft_lvgl_update(&ui,&v);tyvor_tft_lvgl_frame(&ui);CHECK(pipe.pixels>before && pipe.pixels-before<240u*320u);return 1;}
static int tft_render_rotation(void) {CHECK(!lv_setup());for(unsigned r=0;r<4;++r){CHECK(!tyvor_tft_lvgl_rotate(&ui,r));tyvor_tft_lvgl_diagnostic(&ui,TYVOR_TFT_DIAG_RGB565);tyvor_tft_lvgl_frame(&ui);unsigned y=80,w=mock.width;CHECK(mock.framebuffer[y*w+10]==0xf800 && mock.framebuffer[y*w+w/2]==0x07e0 && mock.framebuffer[y*w+w-10]==0x001f);}return 1;}
static int tft_navigation_readonly(void) {static tyvor_sim_t sim;CHECK(!tyvor_sim_init(&sim));CHECK(!tyvor_sim_advance(&sim,200));tyvor_core_t before=sim.core;CHECK(!lv_setup());tyvor_tft_view_t v;CHECK(!tyvor_tft_project(&sim.ui.state,sim.now,0,0,&v));tyvor_tft_lvgl_update(&ui,&v);for(unsigned i=0;i<12;++i){lv_obj_send_event(ui.next,LV_EVENT_CLICKED,NULL);CHECK(ui.page==(i+1)%4);}CHECK(!memcmp(&before,&sim.core,sizeof(before)) && !tyvor_core_rf_enabled());return 1;}
static int session_reset(unsigned kind) {static tyvor_sim_t s;CHECK(!tyvor_sim_init(&s));CHECK(!tyvor_sim_advance(&s,700));tyvor_tft_view_t v;CHECK(!tyvor_tft_project(&s.ui.state,s.now,0,0,&v));CHECK(v.quality==TYVOR_TFT_VALID);if(kind==0 || kind==2)CHECK(!tyvor_sim_reset_control(&s));if(kind==1 || kind==2)CHECK(!tyvor_sim_reset_ui(&s));CHECK(!tyvor_sim_advance(&s,10));CHECK(!tyvor_tft_project(&s.ui.state,s.now,0,0,&v));CHECK(v.quality!=TYVOR_TFT_VALID);CHECK(!tyvor_sim_advance(&s,1600));CHECK(!tyvor_tft_project(&s.ui.state,s.now,0,0,&v));CHECK(v.quality==TYVOR_TFT_VALID && !v.timer_valid && !s.core.channels[0] && !tyvor_core_rf_enabled());return 1;}
static int tft_session_control_reset(void){return session_reset(0);}static int tft_session_ui_reset(void){return session_reset(1);}static int tft_session_both_reset(void){return session_reset(2);}
static int tft_session_loss_recovery(void) {static tyvor_sim_t s;CHECK(!tyvor_sim_init(&s));CHECK(!tyvor_sim_advance(&s,700));s.control_to_ui.drop=1;CHECK(!tyvor_sim_advance(&s,5000));tyvor_tft_view_t v;CHECK(!tyvor_tft_project(&s.ui.state,s.now,0,0,&v));CHECK(v.quality!=TYVOR_TFT_VALID && !v.battery_valid);s.control_to_ui.drop=0;CHECK(!tyvor_sim_advance(&s,2500));CHECK(!tyvor_tft_project(&s.ui.state,s.now,0,0,&v));CHECK(v.quality==TYVOR_TFT_VALID);return 1;}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
#define CASE(x) if(!strcmp(argv[1],#x))return x()?0:1
    CASE(tft_config);CASE(tft_pins_disabled);CASE(tft_pins_evidence);CASE(tft_pins_reserved);CASE(tft_pins_duplicates);CASE(tft_region_bounds);
    CASE(tft_rgb565);CASE(tft_dma_complete);CASE(tft_dma_busy);CASE(tft_dma_timeout);CASE(tft_dma_start_error);CASE(tft_dma_clock);CASE(tft_rotate);
    CASE(tft_valid_safe);CASE(tft_stale);CASE(tft_disconnected);CASE(tft_telemetry_expiry);CASE(tft_live_timer_unknown);CASE(tft_clock_regression);CASE(tft_invalid_safety);
    CASE(tft_touch_disabled);CASE(tft_touch_validation);CASE(tft_render_home);CASE(tft_render_partial);CASE(tft_render_rotation);CASE(tft_navigation_readonly);
    CASE(tft_session_control_reset);CASE(tft_session_ui_reset);CASE(tft_session_both_reset);CASE(tft_session_loss_recovery);
    return 2;
}
