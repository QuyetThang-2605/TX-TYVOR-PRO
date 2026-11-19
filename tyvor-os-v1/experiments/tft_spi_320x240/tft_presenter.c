#include "tft_presenter.h"
#include <stdio.h>
#include <string.h>
int tyvor_tft_project(const tyvor_ui_state_t *source,uint64_t now,int mock,uint64_t timer,tyvor_tft_view_t *v) {
    if(!source || !v)return -1;
    tyvor_ui_state_t state=*source;tyvor_ui_state_poll(&state,now);
    memset(v,0,sizeof(*v));v->mock=(uint8_t)(mock!=0);
    (void)snprintf(v->aircraft_arm,sizeof(v->aircraft_arm),"ARM: UNKNOWN / RO");
    (void)snprintf(v->model,sizeof(v->model),"--");
    (void)snprintf(v->battery,sizeof(v->battery),"Battery --");
    (void)snprintf(v->lq,sizeof(v->lq),"LQ --");
    (void)snprintf(v->rssi,sizeof(v->rssi),"RSSI --");
    (void)snprintf(v->gps,sizeof(v->gps),"GPS --");
    (void)snprintf(v->flight_timer,sizeof(v->flight_timer),"Timer --:--");
    (void)snprintf(v->safety,sizeof(v->safety),"Safety UNKNOWN");
    v->quality=!state.ready || state.disconnected?TYVOR_TFT_DISCONNECTED:!state.valid || state.snapshot.safety_state>TYVOR_FAILSAFE?TYVOR_TFT_STALE:TYVOR_TFT_VALID;
    (void)snprintf(v->connection,sizeof(v->connection),"%s",v->quality==TYVOR_TFT_DISCONNECTED?"DISCONNECTED":v->quality==TYVOR_TFT_STALE?"STALE":"VALID");
    if(v->quality!=TYVOR_TFT_VALID)return 0;
    const tyvor_core_snapshot_t *s=&state.snapshot;v->generation=s->generation;
    v->safe=(uint8_t)(s->safety_state!=TYVOR_BENCH_ARMED);
    const char *safety=(s->safety_reasons&TYVOR_SAFE_MODEL)?"INVALID MODEL / SAFE":s->safety_state==TYVOR_FAILSAFE?"FAILSAFE / SAFE":
        s->safety_state==TYVOR_STARTUP?"STARTUP / SAFE":s->safety_state==TYVOR_DISARMED?"SAFE / DISARMED":"BENCH ARMED (SIM)";
    (void)snprintf(v->safety,sizeof(v->safety),"%s",safety);
    (void)snprintf(v->model,sizeof(v->model),"%.*s",(int)sizeof(s->model_name)-1,s->model_name);
    if(s->telemetry.valid&TYVOR_T_BATTERY) {v->battery_valid=1;(void)snprintf(v->battery,sizeof(v->battery),"Battery %u.%02u V",s->telemetry.battery_mv/1000,(s->telemetry.battery_mv%1000)/10);}
    if(s->telemetry.valid&TYVOR_T_LQ) {v->lq_valid=1;(void)snprintf(v->lq,sizeof(v->lq),"LQ %u%%",s->telemetry.lq);}
    if(s->telemetry.valid&TYVOR_T_RSSI) {v->rssi_valid=1;(void)snprintf(v->rssi,sizeof(v->rssi),"RSSI %d dBm",s->telemetry.rssi_dbm);}
    if(s->telemetry.valid&TYVOR_T_GPS) {
        v->gps_valid=1;int64_t lat=s->telemetry.latitude_e7,lon=s->telemetry.longitude_e7;
        uint32_t a=(uint32_t)(lat<0?-lat:lat),b=(uint32_t)(lon<0?-lon:lon);
        (void)snprintf(v->gps,sizeof(v->gps),"GPS %s%u.%05u / %s%u.%05u\nSatellites %u",lat<0?"-":"",(unsigned)(a/10000000),(unsigned)((a%10000000)/100),
            lon<0?"-":"",(unsigned)(b/10000000),(unsigned)((b%10000000)/100),s->telemetry.satellites);
    }
    if(mock) {v->timer_valid=1;uint64_t seconds=timer/1000;if(seconds>5999)seconds=5999;
        (void)snprintf(v->flight_timer,sizeof(v->flight_timer),"SIM timer %02u:%02u",(unsigned)(seconds/60),(unsigned)(seconds%60));}
    /* Protocol v2 has no flight-timer field; never convert Core ticks into a flight timer. */
    return 0;
}
void tyvor_tft_fixture(tyvor_ui_state_t *s,uint64_t now,unsigned scenario) {
    if(!s)return;
    tyvor_ui_state_init(s);s->ready=1;s->valid=1;s->expires_ms=now+500;s->input_expires_ms=now+500;s->last_poll_ms=now;
    s->snapshot.generation=1;s->snapshot.safety_state=scenario==3?TYVOR_BENCH_ARMED:TYVOR_DISARMED;
    (void)snprintf(s->snapshot.model_name,sizeof(s->snapshot.model_name),"TYVOR DEMO");
    s->snapshot.input_valid=1;s->snapshot.telemetry.valid=15;s->snapshot.telemetry.battery_mv=12240;
    s->snapshot.telemetry.lq=98;s->snapshot.telemetry.rssi_dbm=-62;s->snapshot.telemetry.satellites=12;
    s->snapshot.telemetry.latitude_e7=137560000;s->snapshot.telemetry.longitude_e7=1005010000;
    for(unsigned i=0;i<4;++i)s->telemetry_expires_ms[i]=now+1000;
    if(scenario==1)s->expires_ms=now;
    if(scenario==2) {s->ready=0;s->disconnected=1;}
}
