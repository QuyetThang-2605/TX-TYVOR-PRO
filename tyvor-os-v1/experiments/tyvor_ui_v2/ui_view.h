#ifndef TYVOR_UI_V2_VIEW_H
#define TYVOR_UI_V2_VIEW_H
#include "tft_presenter.h"
typedef struct {
  tyvor_tft_view_t base;
  uint8_t inputs_valid, outputs_valid, mixes_valid, model_valid, throttle_lock,
      rf_valid;
  uint8_t safety_state, mix_count, rf_enabled;
  int16_t inputs[4], channels[8];
  int8_t switches[2];
  tyvor_mix_view_t mixes[16];
  uint32_t model_id, model_revision, age_ms;
  char timer[8], voltage[16], quality[12], signal[16], gps_lat[20], gps_lon[20];
  char safety[24], throttle[20], receiver[24], flight_mode[24];
} tyvor_ui2_view_t;
int tyvor_ui2_project(const tyvor_ui_state_t *, uint64_t now, int simulator,
                      uint64_t timer_ms, tyvor_ui2_view_t *);
void tyvor_ui2_fixture(tyvor_ui_state_t *, uint64_t now, unsigned scenario);
#endif
