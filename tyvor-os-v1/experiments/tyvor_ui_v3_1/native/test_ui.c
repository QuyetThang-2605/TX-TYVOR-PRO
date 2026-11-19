#include "mock.h"
#include "tyvor_sim_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "%s:%d FAIL %s\n", __FILE__, __LINE__, #x);              \
      return 0;                                                                \
    }                                                                          \
  } while (0)
static tyvor_ui31_t ui;
static tyvor_ui31_mock_t mock;
static uint8_t buffer[TYVOR_DISPLAY_BUFFER_BYTES];
static int init(int wide) {
  lv_init();
  return tyvor_ui31_init(
      &ui, tyvor_ui31_mock_port(&mock, wide ? 448 : 320, wide ? 368 : 240),
      buffer, sizeof(buffer));
}
static int view(unsigned scenario, int live, tyvor_ui31_view_t *v) {
  tyvor_ui_state_t s;
  tyvor_ui31_fixture(&s, 1000, scenario);
  return tyvor_ui31_project(&s, 1000, !live, 0, v);
}
static int ui31_profiles(void) {
  tyvor_ui31_layout_t l;
  CHECK(!tyvor_ui31_layout(320, 240, &l));
  CHECK(!tyvor_ui31_layout(448, 368, &l));
  CHECK(tyvor_ui31_layout(240, 320, &l) < 0);
  return 1;
}
static int ui31_geometry(void) {
  CHECK(!init(0));
  tyvor_ui31_frame(&ui);
  lv_area_t a;
  lv_obj_get_coords(ui.hero, &a);
  CHECK(abs(a.x1 - 14) <= 1 && abs(a.y1 - 36) <= 1 &&
        abs(lv_obj_get_width(ui.hero) - 292) <= 1);
  lv_obj_get_coords(ui.footer[0], &a);
  CHECK(abs(a.x1 - 14) <= 1 && abs(a.y1 - 161) <= 1);
  CHECK(lv_obj_get_width(ui.actions[0]) >= 44 &&
        lv_obj_get_height(ui.actions[0]) >= 44);
  return 1;
}
static int ui31_no_footer(void) {
  CHECK(!init(0));
  CHECK(!ui.back && !ui.home);
  CHECK(ui.hero && ui.footer[0] && ui.footer[3]);
  tyvor_ui31_open(&ui, UI31_MODEL);
  CHECK(ui.back && ui.home);
  return 1;
}
static int ui31_home_valid(void) {
  CHECK(!init(0));
  tyvor_ui31_view_t v;
  CHECK(!view(3, 0, &v));
  tyvor_ui31_update(&ui, &v);
  tyvor_ui31_frame(&ui);
  CHECK(!strcmp(lv_label_get_text(ui.field[0]), "00:00") &&
        strstr(lv_label_get_text(ui.safety), "SAFE"));
  CHECK(v.base.safe && !strcmp(v.base.aircraft_arm, "ARM: UNKNOWN / RO"));
  return 1;
}
static int ui31_stale(void) {
  CHECK(!init(0));
  tyvor_ui31_view_t v;
  CHECK(!view(1, 0, &v));
  tyvor_ui31_update(&ui, &v);
  CHECK(!v.base.safe && !v.inputs_valid && !v.outputs_valid && !v.mixes_valid &&
        !v.rf_valid && !strcmp(v.timer, "--:--") &&
        !strcmp(lv_label_get_text(ui.safety), "STALE"));
  return 1;
}
static int ui31_disconnect(void) {
  tyvor_ui31_view_t v;
  CHECK(!view(2, 0, &v));
  CHECK(v.base.quality == TYVOR_TFT_DISCONNECTED && !v.base.safe &&
        !v.base.battery_valid && !v.outputs_valid);
  return 1;
}
static int ui31_invalid_model(void) {
  tyvor_ui31_view_t v;
  CHECK(!view(4, 0, &v));
  CHECK(!v.model_valid && !v.outputs_valid && !v.mixes_valid &&
        !strcmp(v.safety, "INVALID MODEL"));
  return 1;
}
static int ui31_live_unknown(void) {
  tyvor_ui31_view_t v;
  CHECK(!view(3, 1, &v));
  CHECK(!v.base.timer_valid && !strcmp(v.timer, "--:--") &&
        !strcmp(v.receiver, "RX UNAVAILABLE"));
  return 1;
}
static int expiry(unsigned type) {
  tyvor_ui_state_t s;
  tyvor_ui31_view_t v;
  tyvor_ui31_fixture(&s, 1000, 3);
  if (type == 0)
    s.telemetry_expires_ms[0] = s.telemetry_expires_ms[3] = 1000;
  if (type == 1)
    s.input_expires_ms = 1000;
  if (type == 2)
    s.mix_expires_ms = 1000;
  CHECK(!tyvor_ui31_project(&s, 1000, 1, 0, &v));
  if (type == 0)
    CHECK(!v.base.battery_valid && !v.base.gps_valid && v.base.lq_valid);
  if (type == 1)
    CHECK(!v.inputs_valid && !v.outputs_valid);
  if (type == 2)
    CHECK(!v.mixes_valid);
  return 1;
}
static int ui31_field_expiry(void) { return expiry(0); }
static int ui31_input_expiry(void) { return expiry(1); }
static int ui31_mix_expiry(void) { return expiry(2); }
static int layouts(int details) {
  CHECK(!init(0));
  char reason[180];
  for (unsigned profile = 0; profile < 2; ++profile) {
    if (profile) {
      tyvor_ui31_destroy(&ui);
      CHECK(!tyvor_ui31_init(&ui, tyvor_ui31_mock_port(&mock, 448, 368), buffer,
                             sizeof(buffer)));
    }
    for (unsigned state = 0; state < 5; ++state) {
      tyvor_ui31_view_t v;
      CHECK(!view(state, 0, &v));
      for (unsigned page = 0; page < UI31_PAGE_COUNT; ++page) {
        tyvor_ui31_open(&ui, page);
        tyvor_ui31_update(&ui, &v);
        if (details)
          tyvor_ui31_action(&ui, UI31_MORE);
        tyvor_ui31_frame(&ui);
        if (tyvor_ui31_layout_errors(&ui, reason, sizeof(reason)))
          fprintf(stderr, "profile%u state%u page%u sub%u %s\n", profile, state,
                  page, ui.subpage, reason);
        CHECK(!tyvor_ui31_layout_errors(&ui, reason, sizeof(reason)));
      }
    }
  }
  return 1;
}
static int ui31_layout(void) { return layouts(0); }
static int ui31_detail_layout(void) { return layouts(1); }
static int ui31_long_model(void) {
  CHECK(!init(0));
  tyvor_ui_state_t s;
  tyvor_ui31_view_t v;
  CHECK(!view(3, 0, &v));
  tyvor_ui31_update(&ui, &v);
  tyvor_ui31_fixture(&s, 1000, 3);
  s.snapshot.model_id = UINT32_MAX;
  strcpy(s.snapshot.model_name, "WWWWWWWWWWWWWW");
  CHECK(!tyvor_ui31_project(&s, 1000, 1, 0, &v));
  tyvor_ui31_update(&ui, &v);
  CHECK(!strcmp(lv_label_get_text(ui.top_model), "MODEL"));
  tyvor_ui31_open(&ui, UI31_MODEL);
  tyvor_ui31_frame(&ui);
  char reason[180];
  CHECK(!strcmp(lv_label_get_text(ui.field[1]), s.snapshot.model_name));
  CHECK(!tyvor_ui31_layout_errors(&ui, reason, sizeof(reason)));
  return 1;
}
static int ui31_input_bounds(void) {
  CHECK(!init(0));
  tyvor_ui31_view_t v;
  CHECK(!view(3, 0, &v));
  v.inputs[0] = INT16_MAX;
  v.inputs[1] = INT16_MIN;
  tyvor_ui31_update(&ui, &v);
  tyvor_ui31_frame(&ui);
  CHECK(mock.writes > 0 && mock.width == 320 && mock.height == 240);
  static uint16_t before[320 * 240];
  memcpy(before, mock.pixels, sizeof(before));
  v.inputs[0] = 1000;
  v.inputs[1] = -1000;
  tyvor_ui31_update(&ui, &v);
  tyvor_ui31_frame(&ui);
  CHECK(!memcmp(before, mock.pixels, sizeof(before)));
  return 1;
}
static int ui31_font(void) {
  lv_font_glyph_dsc_t d;
  CHECK(lv_font_get_glyph_dsc(&tyvor_ui31_timer_compact, &d, '0', ':'));
  CHECK(d.box_w > 10 && d.box_h > 30);
  CHECK(lv_font_get_glyph_dsc(&tyvor_ui31_timer_large, &d, ':', '0'));
  CHECK(!lv_font_get_glyph_dsc(&tyvor_ui31_timer_compact, &d, 'A', 0));
  return 1;
}
static int ui31_navigation(void) {
  CHECK(!init(0));
  for (unsigned p = 0; p < 8; ++p) {
    tyvor_ui31_action(&ui, UI31_NEXT);
    CHECK(ui.page == (p + 1) % 8);
  }
  return 1;
}
static int ui31_back(void) {
  CHECK(!init(0));
  tyvor_ui31_open(&ui, UI31_MODEL);
  tyvor_ui31_open(&ui, UI31_INPUTS);
  tyvor_ui31_action(&ui, UI31_BACK);
  CHECK(ui.page == UI31_MODEL);
  tyvor_ui31_action(&ui, UI31_BACK);
  CHECK(ui.page == UI31_HOME);
  return 1;
}
static int ui31_home(void) {
  CHECK(!init(0));
  for (unsigned p = 0; p < 40; ++p)
    tyvor_ui31_open(&ui, (p + 1) % 9);
  CHECK(ui.history_count <= 16);
  tyvor_ui31_action(&ui, UI31_GO_HOME);
  CHECK(ui.page == UI31_HOME && !ui.history_count);
  return 1;
}
static int ui31_gestures(void) {
  CHECK(!init(0));
  tyvor_ui31_gesture(&ui, -80, 3);
  CHECK(ui.page == UI31_MODEL);
  tyvor_ui31_action(&ui, UI31_GO_HOME);
  tyvor_ui31_gesture(&ui, 0, -80);
  CHECK(ui.page == UI31_QUICK);
  tyvor_ui31_action(&ui, UI31_GO_HOME);
  tyvor_ui31_gesture(&ui, 0, 80);
  CHECK(ui.page == UI31_CONNECTIONS);
  return 1;
}
static int ui31_quick(void) {
  CHECK(!init(0));
  tyvor_ui31_open(&ui, UI31_QUICK);
  lv_obj_send_event(ui.actions[1], LV_EVENT_CLICKED, NULL);
  CHECK(ui.page == UI31_OUTPUTS);
  return 1;
}
static int ui31_disabled_bind(void) {
  CHECK(!init(0));
  CHECK(lv_obj_has_state(ui.actions[1], LV_STATE_DISABLED));
  CHECK(lv_obj_get_event_count(ui.actions[1]) == 0);
  return 1;
}
static int ui31_mouse_buttons(void) {
  CHECK(!init(0));
  lv_obj_send_event(ui.actions[0], LV_EVENT_CLICKED, NULL);
  CHECK(ui.page == UI31_TELEMETRY);
  lv_obj_send_event(ui.back, LV_EVENT_CLICKED, NULL);
  CHECK(ui.page == UI31_HOME);
  return 1;
}
static int ui31_pagination(void) {
  CHECK(!init(0));
  tyvor_ui31_view_t v;
  CHECK(!view(3, 0, &v));
  tyvor_ui31_update(&ui, &v);
  tyvor_ui31_open(&ui, UI31_OUTPUTS);
  tyvor_ui31_action(&ui, UI31_MORE);
  CHECK(ui.subpage == 1 && !strcmp(lv_label_get_text(ui.field[0]), "CH 05"));
  return 1;
}
static int ui31_stale_detail(void) {
  CHECK(!init(0));
  tyvor_ui31_view_t v;
  CHECK(!view(3, 0, &v));
  tyvor_ui31_update(&ui, &v);
  tyvor_ui31_open(&ui, UI31_MIXES);
  tyvor_ui31_action(&ui, UI31_MORE);
  CHECK(ui.subpage == 1);
  CHECK(!view(1, 0, &v));
  tyvor_ui31_update(&ui, &v);
  CHECK(!ui.subpage &&
        !strcmp(lv_label_get_text(ui.field[0]), "MIX UNAVAILABLE"));
  return 1;
}
static int ui31_partial(void) {
  CHECK(!init(0));
  tyvor_ui31_view_t v;
  CHECK(!view(3, 0, &v));
  tyvor_ui31_update(&ui, &v);
  tyvor_ui31_frame(&ui);
  uint64_t old = ui.pixels;
  strcpy(v.quality, "70%");
  tyvor_ui31_update(&ui, &v);
  tyvor_ui31_frame(&ui);
  CHECK(ui.pixels > old && ui.pixels - old < 320u * 240u / 2);
  return 1;
}
static int ui31_idle(void) {
  CHECK(!init(0));
  tyvor_ui31_view_t v;
  CHECK(!view(3, 0, &v));
  tyvor_ui31_update(&ui, &v);
  tyvor_ui31_frame(&ui);
  uint64_t pixels = ui.pixels;
  unsigned builds = ui.allocations_on_navigation;
  for (unsigned i = 0; i < 10; ++i) {
    tyvor_ui31_update(&ui, &v);
    tyvor_ui31_frame(&ui);
  }
  CHECK(ui.pixels == pixels && ui.allocations_on_navigation == builds);
  return 1;
}
static int ui31_memory(void) {
  CHECK(!init(1));
  tyvor_ui31_view_t v;
  CHECK(!view(3, 0, &v));
  tyvor_ui31_update(&ui, &v);
  tyvor_ui31_frame(&ui);
  lv_mem_monitor_t before, after;
  lv_mem_monitor(&before);
  for (unsigned p = 0; p < 500; ++p) {
    tyvor_ui31_open(&ui, (p + 1) % 9);
    tyvor_ui31_frame(&ui);
  }
  tyvor_ui31_action(&ui, UI31_GO_HOME);
  tyvor_ui31_frame(&ui);
  lv_mem_monitor(&after);
  CHECK(after.free_size + 512 >= before.free_size && after.free_size > 8192);
  return 1;
}
static int ui31_readonly(void) {
  static tyvor_sim_t sim;
  CHECK(!tyvor_sim_init(&sim) && !tyvor_sim_advance(&sim, 700));
  tyvor_core_t core = sim.core;
  tyvor_ui_endpoint_t endpoint = sim.ui;
  CHECK(!init(0));
  tyvor_ui31_view_t v;
  CHECK(!tyvor_ui31_project(&sim.ui.state, sim.now, 0, 0, &v));
  tyvor_ui31_update(&ui, &v);
  for (unsigned p = 0; p < 9; ++p) {
    tyvor_ui31_open(&ui, p);
    tyvor_ui31_action(&ui, UI31_MORE);
  }
  CHECK(!memcmp(&core, &sim.core, sizeof(core)) &&
        !memcmp(&endpoint, &sim.ui, sizeof(endpoint)) &&
        !tyvor_core_rf_enabled());
  return 1;
}
static int reset(unsigned kind) {
  static tyvor_sim_t sim;
  CHECK(!tyvor_sim_init(&sim) && !tyvor_sim_advance(&sim, 700));
  tyvor_ui31_view_t v;
  CHECK(!tyvor_ui31_project(&sim.ui.state, sim.now, 0, 0, &v) &&
        v.base.quality == TYVOR_TFT_VALID);
  if (kind != 1)
    CHECK(!tyvor_sim_reset_control(&sim));
  if (kind != 0)
    CHECK(!tyvor_sim_reset_ui(&sim));
  CHECK(!tyvor_sim_advance(&sim, 10));
  CHECK(!tyvor_ui31_project(&sim.ui.state, sim.now, 0, 0, &v) &&
        v.base.quality != TYVOR_TFT_VALID);
  CHECK(!tyvor_sim_advance(&sim, 1800));
  CHECK(!tyvor_ui31_project(&sim.ui.state, sim.now, 0, 0, &v) &&
        v.base.quality == TYVOR_TFT_VALID && !v.base.timer_valid);
  return 1;
}
static int ui31_reset_control(void) { return reset(0); }
static int ui31_reset_ui(void) { return reset(1); }
static int ui31_reset_both(void) { return reset(2); }
static int ui31_loss_recovery(void) {
  static tyvor_sim_t sim;
  CHECK(!tyvor_sim_init(&sim) && !tyvor_sim_advance(&sim, 700));
  sim.control_to_ui.drop = 1;
  CHECK(!tyvor_sim_advance(&sim, 5000));
  tyvor_ui31_view_t v;
  CHECK(!tyvor_ui31_project(&sim.ui.state, sim.now, 0, 0, &v) &&
        v.base.quality != TYVOR_TFT_VALID);
  sim.control_to_ui.drop = 0;
  CHECK(!tyvor_sim_advance(&sim, 2500));
  CHECK(!tyvor_ui31_project(&sim.ui.state, sim.now, 0, 0, &v) &&
        v.base.quality == TYVOR_TFT_VALID);
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
#define CASE(x)                                                                \
  if (!strcmp(argv[1], #x))                                                    \
  return x() ? 0 : 1
  CASE(ui31_profiles);
  CASE(ui31_geometry);
  CASE(ui31_no_footer);
  CASE(ui31_home_valid);
  CASE(ui31_stale);
  CASE(ui31_disconnect);
  CASE(ui31_invalid_model);
  CASE(ui31_live_unknown);
  CASE(ui31_field_expiry);
  CASE(ui31_input_expiry);
  CASE(ui31_mix_expiry);
  CASE(ui31_layout);
  CASE(ui31_detail_layout);
  CASE(ui31_long_model);
  CASE(ui31_input_bounds);
  CASE(ui31_font);
  CASE(ui31_navigation);
  CASE(ui31_back);
  CASE(ui31_home);
  CASE(ui31_gestures);
  CASE(ui31_quick);
  CASE(ui31_disabled_bind);
  CASE(ui31_mouse_buttons);
  CASE(ui31_pagination);
  CASE(ui31_stale_detail);
  CASE(ui31_partial);
  CASE(ui31_idle);
  CASE(ui31_memory);
  CASE(ui31_readonly);
  CASE(ui31_reset_control);
  CASE(ui31_reset_ui);
  CASE(ui31_reset_both);
  CASE(ui31_loss_recovery);
  return 2;
}
