#ifndef TYVOR_TFT_PRESENTER_H
#define TYVOR_TFT_PRESENTER_H
#include "tyvor_ui_state.h"
enum {TYVOR_TFT_VALID,TYVOR_TFT_STALE,TYVOR_TFT_DISCONNECTED};
typedef struct {
    uint8_t quality,safe,mock,battery_valid,lq_valid,rssi_valid,gps_valid,timer_valid;
    char connection[24],safety[32],model[TYVOR_MODEL_NAME_CAP],aircraft_arm[24];
    char battery[32],lq[24],rssi[24],gps[64],flight_timer[32];
    uint32_t generation;
} tyvor_tft_view_t;
int tyvor_tft_project(const tyvor_ui_state_t *,uint64_t now,int mock,uint64_t simulator_timer_ms,tyvor_tft_view_t *);
void tyvor_tft_fixture(tyvor_ui_state_t *,uint64_t now,unsigned scenario);
#endif
