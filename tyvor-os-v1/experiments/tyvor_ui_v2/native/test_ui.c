#include "mock.h"
#include "tyvor_sim_runtime.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "%s:%d FAIL %s\n", __FILE__, __LINE__, #x);              \
      return 0;                                                                \
    }                                                                          \
  } while (0)
static tyvor_ui2_t ui;
static tyvor_ui2_mock_t mock;
static uint8_t buffer[TYVOR_DISPLAY_BUFFER_BYTES];
static int init(int wide) {
  lv_init();
  return tyvor_ui2_init(
      &ui, tyvor_ui2_mock_port(&mock, wide ? 368 : 240, wide ? 448 : 320),
      buffer, sizeof(buffer));
}
static int view(unsigned state, int live, tyvor_ui2_view_t *v) {
  tyvor_ui_state_t s;
  tyvor_ui2_fixture(&s, 1000, state);
  return tyvor_ui2_project(&s, 1000, !live, 754000, v);
}
static int ui2_profiles(void) {
  tyvor_ui2_layout_t l;
  CHECK(!tyvor_ui2_layout(240, 320, &l) && !l.wide);
  CHECK(!tyvor_ui2_layout(368, 448, &l) && l.wide);
  CHECK(tyvor_ui2_layout(320, 240, &l) < 0);
  CHECK(l.nav >= 44);
  return 1;
}
static int ui2_valid(void) {
  tyvor_ui2_view_t v;
  CHECK(!view(0, 0, &v));
  CHECK(v.base.quality == TYVOR_TFT_VALID && !v.base.safe &&
        !strcmp(v.safety, "BENCH ARM / SIM") && !strcmp(v.timer, "12:34"));
  CHECK(!strcmp(v.base.aircraft_arm, "ARM: UNKNOWN / RO"));
  return 1;
}
static int ui2_safe(void) {
  tyvor_ui2_view_t v;
  CHECK(!view(3, 0, &v));
  CHECK(v.base.safe && v.throttle_lock && !strcmp(v.safety, "SAFE / DISARMED"));
  return 1;
}
static int ui2_invalid_model(void) {
  tyvor_ui2_view_t v;
  CHECK(!view(4, 0, &v));
  CHECK(!v.model_valid && !v.outputs_valid && !v.mixes_valid &&
        !strcmp(v.safety, "INVALID MODEL"));
  return 1;
}
static int ui2_stale(void) {
  tyvor_ui2_view_t v;
  CHECK(!view(1, 0, &v));
  CHECK(v.base.quality == TYVOR_TFT_STALE && !v.base.safe && !v.inputs_valid &&
        !v.outputs_valid && !v.mixes_valid && !v.rf_valid &&
        !strcmp(v.timer, "--:--") && !strcmp(v.safety, "UNKNOWN"));
  return 1;
}
static int ui2_disconnect(void) {
  tyvor_ui2_view_t v;
  CHECK(!view(2, 0, &v));
  CHECK(v.base.quality == TYVOR_TFT_DISCONNECTED && !v.base.safe &&
        !v.base.battery_valid && !v.rf_valid);
  return 1;
}
static int ui2_live_unknown(void) {
  tyvor_ui2_view_t v;
  CHECK(!view(0, 1, &v));
  CHECK(!v.base.mock && !v.base.timer_valid && !strcmp(v.timer, "--:--") &&
        !strcmp(v.receiver, "RX UNAVAILABLE") &&
        !strcmp(v.flight_mode, "UNAVAILABLE"));
  return 1;
}
static int ui2_field_expiry(void) {
  tyvor_ui_state_t s;
  tyvor_ui2_view_t v;
  tyvor_ui2_fixture(&s, 1000, 0);
  s.telemetry_expires_ms[0] = s.telemetry_expires_ms[3] = 1000;
  CHECK(!tyvor_ui2_project(&s, 1000, 1, 0, &v));
  CHECK(!v.base.battery_valid && !v.base.gps_valid && v.base.lq_valid &&
        !strcmp(v.voltage, "--.-- V") && !strcmp(v.gps_lat, "UNAVAILABLE"));
  return 1;
}
static int ui2_input_expiry(void) {
  tyvor_ui_state_t s;
  tyvor_ui2_view_t v;
  tyvor_ui2_fixture(&s, 1000, 0);
  s.input_expires_ms = 1000;
  CHECK(!tyvor_ui2_project(&s, 1000, 1, 0, &v));
  CHECK(!v.inputs_valid && !v.outputs_valid && !v.inputs[0] && !v.channels[0]);
  return 1;
}
static int ui2_mix_expiry(void) {
  tyvor_ui_state_t s;
  tyvor_ui2_view_t v;
  tyvor_ui2_fixture(&s, 1000, 0);
  s.mix_expires_ms = 1000;
  CHECK(!tyvor_ui2_project(&s, 1000, 1, 0, &v));
  CHECK(!v.mixes_valid && !v.mix_count);
  return 1;
}
static int ui2_mix_generation(void) {
  tyvor_ui_state_t s;
  tyvor_ui2_view_t v;
  tyvor_ui2_fixture(&s, 1000, 0);
  s.mix_generation++;
  CHECK(!tyvor_ui2_project(&s, 1000, 1, 0, &v) && !v.mixes_valid);
  return 1;
}
static int ui2_clock(void) {
  tyvor_ui_state_t s;
  tyvor_ui2_view_t v;
  tyvor_ui2_fixture(&s, 1000, 0);
  CHECK(!tyvor_ui2_project(&s, 999, 1, 0, &v));
  CHECK(v.base.quality != TYVOR_TFT_VALID && !v.outputs_valid && !v.rf_valid);
  return 1;
}
static int ui2_timer(void) {
  tyvor_ui_state_t s;
  tyvor_ui2_view_t v;
  tyvor_ui2_fixture(&s, 1000, 0);
  CHECK(!tyvor_ui2_project(&s, 1000, 1, UINT64_MAX, &v));
  CHECK(!strcmp(v.timer, "99:59"));
  return 1;
}
static int ui2_navigation(void) {
  CHECK(!init(0));
  for (unsigned i = 0; i < 7; ++i) {
    tyvor_ui2_action(&ui, UI2_NEXT);
    CHECK(ui.page == (i + 1) % 7);
  }
  tyvor_ui2_action(&ui, UI2_PREVIOUS);
  CHECK(ui.page == UI2_SETTINGS);
  return 1;
}
static int ui2_back(void) {
  CHECK(!init(0));
  tyvor_ui2_open(&ui, UI2_MODEL);
  tyvor_ui2_open(&ui, UI2_INPUTS);
  tyvor_ui2_action(&ui, UI2_BACK);
  CHECK(ui.page == UI2_MODEL);
  tyvor_ui2_action(&ui, UI2_BACK);
  CHECK(ui.page == UI2_HOME);
  return 1;
}
static int ui2_home(void) {
  CHECK(!init(0));
  for (unsigned i = 0; i < 40; ++i)
    tyvor_ui2_open(&ui, (i + 1) % 7);
  CHECK(ui.history_count <= 16);
  tyvor_ui2_action(&ui, UI2_GO_HOME);
  CHECK(ui.page == UI2_HOME && !ui.history_count);
  return 1;
}
static int ui2_gestures(void) {
  CHECK(!init(0));
  tyvor_ui2_gesture(&ui, -80, 10);
  CHECK(ui.page == UI2_MODEL);
  tyvor_ui2_action(&ui, UI2_GO_HOME);
  tyvor_ui2_gesture(&ui, 4, -80);
  CHECK(ui.page == UI2_QUICK);
  return 1;
}
static int ui2_quick(void) {
  CHECK(!init(0));
  tyvor_ui2_open(&ui, UI2_QUICK);
  lv_obj_send_event(ui.actions[1], LV_EVENT_CLICKED, NULL);
  CHECK(ui.page == UI2_OUTPUTS);
  return 1;
}
static int ui2_mouse_buttons(void) {
  CHECK(!init(0));
  lv_obj_send_event(ui.nav[2], LV_EVENT_CLICKED, NULL);
  CHECK(ui.page == UI2_MODEL);
  lv_obj_send_event(ui.nav[0], LV_EVENT_CLICKED, NULL);
  CHECK(ui.page == UI2_HOME);
  return 1;
}
static int ui2_pagination(void) {
  CHECK(!init(0));
  tyvor_ui2_view_t v;
  CHECK(!view(0, 0, &v));
  tyvor_ui2_update(&ui, &v);
  tyvor_ui2_open(&ui, UI2_OUTPUTS);
  tyvor_ui2_action(&ui, UI2_MORE);
  CHECK(ui.subpage == 1 && !strcmp(lv_label_get_text(ui.field[0]), "CH 05"));
  tyvor_ui2_action(&ui, UI2_BACK);
  CHECK(!ui.subpage);
  return 1;
}
static int ui2_layout(void) {
  CHECK(!init(0));
  char reason[160];
  for (unsigned profile = 0; profile < 2; ++profile) {
    if (profile) {
      tyvor_ui2_destroy(&ui);
      CHECK(!tyvor_ui2_init(&ui, tyvor_ui2_mock_port(&mock, 368, 448), buffer,
                            sizeof(buffer)));
    }
    for (unsigned state = 0; state < 5; ++state) {
      tyvor_ui2_view_t v;
      CHECK(!view(state, 0, &v));
      for (unsigned p = 0; p < 8; ++p) {
        tyvor_ui2_open(&ui, p);
        tyvor_ui2_update(&ui, &v);
        tyvor_ui2_frame(&ui);
        if (tyvor_ui2_layout_errors(&ui, reason, sizeof(reason)))
          fprintf(stderr, "profile%u state%u page%u: %s\n", profile, state, p,
                  reason);
        CHECK(!tyvor_ui2_layout_errors(&ui, reason, sizeof(reason)));
      }
    }
  }
  return 1;
}
static int ui2_partial(void) {
  CHECK(!init(0));
  tyvor_ui2_view_t v;
  CHECK(!view(0, 0, &v));
  tyvor_ui2_update(&ui, &v);
  tyvor_ui2_frame(&ui);
  uint64_t old = ui.pixels;
  strcpy(v.quality, "70%");
  tyvor_ui2_update(&ui, &v);
  tyvor_ui2_frame(&ui);
  CHECK(ui.pixels > old && ui.pixels - old < 240u * 320u / 2);
  return 1;
}
static int ui2_idle(void) {
  CHECK(!init(0));
  tyvor_ui2_view_t v;
  CHECK(!view(0, 0, &v));
  tyvor_ui2_update(&ui, &v);
  tyvor_ui2_frame(&ui);
  uint32_t writes = mock.writes, builds = ui.allocations_on_navigation;
  for (unsigned i = 0; i < 10; ++i) {
    tyvor_ui2_update(&ui, &v);
    tyvor_ui2_frame(&ui);
  }
  CHECK(writes == mock.writes && builds == ui.allocations_on_navigation);
  return 1;
}
static int ui2_memory(void) {
  CHECK(!init(1));
  tyvor_ui2_view_t v;
  CHECK(!view(0, 0, &v));
  tyvor_ui2_update(&ui, &v);
  tyvor_ui2_frame(&ui);
  lv_mem_monitor_t before, after;
  lv_mem_monitor(&before);
  for (unsigned i = 0; i < 500; ++i) {
    tyvor_ui2_open(&ui, (i + 1) % 8);
    tyvor_ui2_frame(&ui);
  }
  tyvor_ui2_action(&ui, UI2_GO_HOME);
  tyvor_ui2_frame(&ui);
  lv_mem_monitor(&after);
  CHECK(after.free_size + 512 >= before.free_size && after.free_size > 8192);
  return 1;
}
static int ui2_readonly(void) {
  static tyvor_sim_t sim;
  CHECK(!tyvor_sim_init(&sim) && !tyvor_sim_advance(&sim, 700));
  tyvor_core_t core = sim.core;
  tyvor_ui_endpoint_t endpoint = sim.ui;
  CHECK(!init(0));
  tyvor_ui2_view_t v;
  CHECK(!tyvor_ui2_project(&sim.ui.state, sim.now, 0, 0, &v));
  tyvor_ui2_update(&ui, &v);
  for (unsigned p = 0; p < 8; ++p) {
    tyvor_ui2_open(&ui, p);
    tyvor_ui2_action(&ui, UI2_MORE);
  }
  CHECK(!memcmp(&core, &sim.core, sizeof(core)) &&
        !memcmp(&endpoint, &sim.ui, sizeof(endpoint)) &&
        !tyvor_core_rf_enabled());
  return 1;
}
static int reset(unsigned kind) {
  static tyvor_sim_t sim;
  CHECK(!tyvor_sim_init(&sim) && !tyvor_sim_advance(&sim, 700));
  tyvor_ui2_view_t v;
  CHECK(!tyvor_ui2_project(&sim.ui.state, sim.now, 0, 0, &v) &&
        v.base.quality == TYVOR_TFT_VALID);
  if (kind != 1)
    CHECK(!tyvor_sim_reset_control(&sim));
  if (kind != 0)
    CHECK(!tyvor_sim_reset_ui(&sim));
  CHECK(!tyvor_sim_advance(&sim, 10));
  CHECK(!tyvor_ui2_project(&sim.ui.state, sim.now, 0, 0, &v) &&
        v.base.quality != TYVOR_TFT_VALID && !v.outputs_valid);
  CHECK(!tyvor_sim_advance(&sim, 1800));
  CHECK(!tyvor_ui2_project(&sim.ui.state, sim.now, 0, 0, &v) &&
        v.base.quality == TYVOR_TFT_VALID && !v.base.timer_valid);
  return 1;
}
static int ui2_reset_control(void) { return reset(0); }
static int ui2_reset_ui(void) { return reset(1); }
static int ui2_reset_both(void) { return reset(2); }
static int ui2_loss_recovery(void) {
  static tyvor_sim_t sim;
  CHECK(!tyvor_sim_init(&sim) && !tyvor_sim_advance(&sim, 700));
  sim.control_to_ui.drop = 1;
  CHECK(!tyvor_sim_advance(&sim, 5000));
  tyvor_ui2_view_t v;
  CHECK(!tyvor_ui2_project(&sim.ui.state, sim.now, 0, 0, &v) &&
        v.base.quality != TYVOR_TFT_VALID && !v.outputs_valid);
  sim.control_to_ui.drop = 0;
  CHECK(!tyvor_sim_advance(&sim, 2500));
  CHECK(!tyvor_ui2_project(&sim.ui.state, sim.now, 0, 0, &v) &&
        v.base.quality == TYVOR_TFT_VALID);
  return 1;
}
static int ui2_input_bounds(void) {
  CHECK(!init(0));
  tyvor_ui2_view_t v;
  CHECK(!view(0, 0, &v));
  v.inputs[0] = INT16_MAX;
  v.inputs[1] = INT16_MIN;
  tyvor_ui2_update(&ui, &v);
  tyvor_ui2_open(&ui, UI2_INPUTS);
  tyvor_ui2_frame(&ui);
  lv_area_t a, b;
  lv_obj_get_coords(ui.sticks[1], &a);
  lv_obj_get_coords(ui.stick_dots[1], &b);
  CHECK(b.x1 >= a.x1 && b.x2 <= a.x2 && b.y1 >= a.y1 && b.y2 <= a.y2);
  return 1;
}
static int ui2_layout_live(void) {
  CHECK(!init(0));
  char reason[180];
  for (unsigned profile = 0; profile < 2; ++profile) {
    if (profile) {
      tyvor_ui2_destroy(&ui);
      CHECK(!tyvor_ui2_init(&ui, tyvor_ui2_mock_port(&mock, 368, 448), buffer,
                            sizeof(buffer)));
    }
    tyvor_ui2_view_t v;
    CHECK(!view(3, 1, &v));
    for (unsigned p = 0; p < 8; ++p) {
      tyvor_ui2_open(&ui, p);
      tyvor_ui2_update(&ui, &v);
      tyvor_ui2_frame(&ui);
      if (tyvor_ui2_layout_errors(&ui, reason, sizeof(reason)))
        fprintf(stderr, "%s\n", reason);
      CHECK(!tyvor_ui2_layout_errors(&ui, reason, sizeof(reason)));
    }
  }
  return 1;
}
static int ui2_long_model(void) {
  CHECK(!init(0));
  tyvor_ui_state_t s;
  tyvor_ui2_view_t v;
  tyvor_ui2_fixture(&s, 1000, 3);
  strcpy(s.snapshot.model_name, "WWWWWWWWWWWWWW");
  CHECK(!tyvor_ui2_project(&s, 1000, 1, 0, &v));
  tyvor_ui2_update(&ui, &v);
  tyvor_ui2_frame(&ui);
  char reason[180];
  CHECK(!strcmp(lv_label_get_text(ui.field[1]), s.snapshot.model_name));
  CHECK(!tyvor_ui2_layout_errors(&ui, reason, sizeof(reason)));
  tyvor_ui2_open(&ui, UI2_MODEL);
  tyvor_ui2_frame(&ui);
  CHECK(!strcmp(lv_label_get_text(ui.field[0]), s.snapshot.model_name));
  CHECK(!tyvor_ui2_layout_errors(&ui, reason, sizeof(reason)));
  return 1;
}
static int ui2_stale_detail(void) {
  CHECK(!init(0));
  tyvor_ui2_view_t v;
  CHECK(!view(0, 0, &v));
  tyvor_ui2_update(&ui, &v);
  tyvor_ui2_open(&ui, UI2_MIXES);
  tyvor_ui2_action(&ui, UI2_MORE);
  CHECK(ui.subpage == 1);
  CHECK(!view(1, 0, &v));
  tyvor_ui2_update(&ui, &v);
  CHECK(!ui.subpage &&
        !strcmp(lv_label_get_text(ui.field[0]), "-- / MIX UNAVAILABLE"));
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
#define CASE(x)                                                                \
  if (!strcmp(argv[1], #x))                                                    \
  return x() ? 0 : 1
  CASE(ui2_profiles);
  CASE(ui2_valid);
  CASE(ui2_safe);
  CASE(ui2_invalid_model);
  CASE(ui2_stale);
  CASE(ui2_disconnect);
  CASE(ui2_live_unknown);
  CASE(ui2_field_expiry);
  CASE(ui2_input_expiry);
  CASE(ui2_mix_expiry);
  CASE(ui2_mix_generation);
  CASE(ui2_clock);
  CASE(ui2_timer);
  CASE(ui2_navigation);
  CASE(ui2_back);
  CASE(ui2_home);
  CASE(ui2_gestures);
  CASE(ui2_quick);
  CASE(ui2_mouse_buttons);
  CASE(ui2_pagination);
  CASE(ui2_layout);
  CASE(ui2_partial);
  CASE(ui2_idle);
  CASE(ui2_memory);
  CASE(ui2_readonly);
  CASE(ui2_reset_control);
  CASE(ui2_reset_ui);
  CASE(ui2_reset_both);
  CASE(ui2_loss_recovery);
  CASE(ui2_input_bounds);
  CASE(ui2_layout_live);
  CASE(ui2_long_model);
  CASE(ui2_stale_detail);
  return 2;
}
