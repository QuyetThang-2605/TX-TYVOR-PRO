#include "brand_assets.h"
#include "icon_data.h"
#include "icons.h"
#include "mock.h"
#include "render_quality.h"
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
static tyvor_ui321_t ui;
static tyvor_ui321_mock_t mock;
static uint8_t buffer[TYVOR_DISPLAY_BUFFER_BYTES];
static int init(int wide) {
  lv_init();
  return tyvor_ui321_init(
      &ui, tyvor_ui321_mock_port(&mock, wide ? 448 : 320, wide ? 368 : 240),
      buffer, sizeof(buffer));
}
static int ui321_quality_logo_assets(void) {
  const lv_image_dsc_t *images[] = {
      &tyvor_ui321_owl_compact, &tyvor_ui321_owl_large,
      &tyvor_ui321_wordmark_compact, &tyvor_ui321_wordmark_large};
  for (unsigned i = 0; i < 4; ++i) {
    const lv_image_dsc_t *a = images[i];
    CHECK(a->header.cf == LV_COLOR_FORMAT_A8 &&
          a->data_size == (uint32_t)a->header.w * a->header.h);
    unsigned transparent = 0, opaque = 0, aa = 0;
    for (unsigned j = 0; j < a->data_size; j++) {
      transparent += a->data[j] == 0;
      opaque += a->data[j] > 200;
      aa += a->data[j] > 0 && a->data[j] < 255;
    }
    CHECK(transparent > 5 && opaque > 0 && aa > 0);
  }
  return 1;
}
static int ui321_quality_logo_usage(void) {
  CHECK(!init(0));
  CHECK(lv_display_get_color_format(ui.display) == LV_COLOR_FORMAT_RGB888);
  CHECK(lv_image_get_src(lv_obj_get_child(ui.brand_mark, 0)) ==
        &tyvor_ui321_header_compact);
  CHECK(lv_image_get_src(lv_obj_get_child(ui.brand_wordmark, 0)) ==
        &tyvor_ui321_wordmark_compact);
  CHECK(!ui.field[6]);
  return 1;
}
static int ui321_quality_logo_aspect(void) {
  CHECK(!init(0));
  tyvor_ui321_frame(&ui);
  lv_obj_t *im = lv_obj_get_child(ui.brand_mark, 0);
  CHECK(lv_obj_get_width(im) == 76 && lv_obj_get_height(im) == 16);
  CHECK(abs(76 * 45 - 16 * 218) <= 218 / 2);
  CHECK(lv_obj_get_width(ui.brand_mark) == 80 &&
        lv_obj_get_height(ui.brand_mark) == 20);
  return 1;
}
static int ui321_quality_rgb565(void) {
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x) {
      CHECK(tyvor_ui321_rgb565(255, 0, 0, x, y) == 0xf800);
      CHECK(tyvor_ui321_rgb565(0, 255, 0, x, y) == 0x07e0);
      CHECK(tyvor_ui321_rgb565(0, 0, 255, x, y) == 0x001f);
      CHECK(!tyvor_ui321_rgb565(0, 0, 0, x, y));
      CHECK(tyvor_ui321_rgb565(255, 255, 255, x, y) == 0xffff);
    }
  return 1;
}
static int ui321_quality_wire_stride(void) {
  uint8_t b[24] = {0, 0, 255, 0, 255, 0, 255, 0, 0, 99, 99, 99,
                   0, 0, 255, 0, 255, 0, 255, 0, 0, 99, 99, 99};
  uint8_t expected[] = {0xf8, 0, 7, 0xe0, 0, 0x1f, 0xf8, 0, 7, 0xe0, 0, 0x1f};
  CHECK(!tyvor_ui321_pack_rgb888(b, sizeof(b), 12, 3, 2, 7, 9));
  CHECK(!memcmp(b, expected, sizeof(expected)));
  return 1;
}
static int ui321_quality_buffer_limits(void) {
  static uint8_t b[TYVOR_DISPLAY_BUFFER_BYTES];
  memset(b, 77, sizeof(b));
  CHECK(tyvor_ui321_pack_rgb888(b, sizeof(b), 960, 320, 22, 0, 0) < 0 &&
        b[0] == 77);
  CHECK(!tyvor_ui321_pack_rgb888(b, sizeof(b), 960, 320, 21, 0, 0));
  CHECK(tyvor_ui321_pack_rgb888(b, sizeof(b), 1344, 448, 16, 0, 0) < 0);
  CHECK(!tyvor_ui321_pack_rgb888(b, sizeof(b), 1344, 448, 15, 0, 0));
  CHECK(tyvor_ui321_pack_rgb888(b, sizeof(b), 1, 3, 1, 0, 0) < 0);
  return 1;
}
static int ui321_quality_font_aa(void) {
  lv_init();
  lv_font_glyph_dsc_t g;
  CHECK(lv_font_get_glyph_dsc(&tyvor_ui321_timer_compact, &g, '0', ':'));
  lv_draw_buf_t *b =
      lv_draw_buf_create(g.box_w, g.box_h, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
  CHECK(b && lv_font_get_glyph_bitmap(&g, b) == b);
  unsigned aa = 0, levels[256] = {0};
  for (unsigned y = 0; y < g.box_h; ++y)
    for (unsigned x = 0; x < g.box_w; ++x) {
      unsigned a = b->data[y * b->header.stride + x];
      aa += a > 0 && a < 255;
      levels[a] = 1;
    }
  unsigned unique = 0;
  for (unsigned i = 0; i < 256; ++i)
    unique += levels[i];
  lv_draw_buf_destroy(b);
  CHECK(aa > 50 && unique >= 8);
  return 1;
}
static int ui321_quality_partial_invariance(void) {
  CHECK(!init(0));
  static uint16_t baseline[448 * 368];
  for (unsigned profile = 0; profile < 2; ++profile) {
    if (profile) {
      tyvor_ui321_destroy(&ui);
      CHECK(!tyvor_ui321_init(&ui, tyvor_ui321_mock_port(&mock, 448, 368),
                              buffer, sizeof(buffer)));
    }
    tyvor_ui_state_t s;
    tyvor_ui321_view_t v;
    tyvor_ui321_fixture(&s, 1000, 3);
    CHECK(!tyvor_ui321_project(&s, 1000, 1, 0, &v));
    tyvor_ui321_update(&ui, &v);
    tyvor_ui321_frame(&ui);
    size_t n = (size_t)mock.width * mock.height * 2;
    memcpy(baseline, mock.pixels, n);
    strcpy(v.quality, "33%");
    tyvor_ui321_update(&ui, &v);
    tyvor_ui321_frame(&ui);
    strcpy(v.quality, "98%");
    tyvor_ui321_update(&ui, &v);
    tyvor_ui321_frame(&ui);
    CHECK(!memcmp(baseline, mock.pixels, n));
    int16_t original_x = v.inputs[0], original_y = v.inputs[1];
    v.inputs[0] = -1000;
    v.inputs[1] = 1000;
    tyvor_ui321_update(&ui, &v);
    tyvor_ui321_frame(&ui);
    v.inputs[0] = original_x;
    v.inputs[1] = original_y;
    tyvor_ui321_update(&ui, &v);
    tyvor_ui321_frame(&ui);
    CHECK(!memcmp(baseline, mock.pixels, n));
    lv_obj_invalidate(ui.root);
    tyvor_ui321_frame(&ui);
    CHECK(!memcmp(baseline, mock.pixels, n));
  }
  return 1;
}
static int ui321_quality_dither_mean(void) {
  for (unsigned value = 0; value < 256; ++value) {
    double r = 0, g = 0, b = 0;
    for (int y = 0; y < 4; ++y)
      for (int x = 0; x < 4; ++x) {
        unsigned c = tyvor_ui321_rgb565(value, value, value, x, y);
        r += ((c >> 11) & 31) * 255.0 / 31;
        g += ((c >> 5) & 63) * 255.0 / 63;
        b += (c & 31) * 255.0 / 31;
      }
    CHECK(abs((int)(r / 16 + 0.5) - (int)value) <= 1 &&
          abs((int)(g / 16 + 0.5) - (int)value) <= 1 &&
          abs((int)(b / 16 + 0.5) - (int)value) <= 1);
  }
  return 1;
}
static int ui321_background_assets(void) {
  CHECK(tyvor_ui321_background_320.header.w == 320 &&
        tyvor_ui321_background_320.header.h == 240);
  CHECK(tyvor_ui321_background_448.header.w == 448 &&
        tyvor_ui321_background_448.header.h == 368);
  CHECK(tyvor_ui321_background_320.data_size == 320u * 240u * 2u &&
        tyvor_ui321_background_448.data_size == 448u * 368u * 2u);
  CHECK(!init(0));
  tyvor_ui321_frame(&ui);
  const uint8_t *p = tyvor_ui321_background_320.data + (239 * 320) * 2;
  unsigned expected = p[0] | (unsigned)p[1] << 8,
           actual = mock.pixels[239 * 320];
  CHECK(abs((int)((expected >> 11) & 31) - (int)((actual >> 11) & 31)) <= 1);
  CHECK(abs((int)((expected >> 5) & 63) - (int)((actual >> 5) & 63)) <= 1);
  CHECK(abs((int)(expected & 31) - (int)(actual & 31)) <= 1);
  return 1;
}
static int ui321_icons(void) {
  CHECK(!init(0));
  for (unsigned i = 0; i < 15; i++) {
    CHECK(tyvor_ui321_icon_data[i].count > 0);
    lv_obj_t *o = tyvor_ui321_icon_sized(ui.root, 0, 0, 20, i, UI321_TEXT);
    CHECK(o);
    tyvor_ui321_icon_state(o, UI321_ICON_MUTED);
    CHECK(lv_obj_get_style_opa(o, 0) == LV_OPA_30);
    tyvor_ui321_icon_state(o, UI321_ICON_ERROR);
    CHECK(lv_color_eq(lv_obj_get_style_text_color(o, 0),
                      lv_color_hex(UI321_DANGER)));
    tyvor_ui321_icon_state(o, UI321_ICON_VALID);
    CHECK(lv_obj_get_style_opa(o, 0) == LV_OPA_COVER);
    lv_obj_delete(o);
  }
  CHECK(!tyvor_ui321_icon_sized(ui.root, 0, 0, 20, 15, UI321_TEXT));
  return 1;
}
static int ui321_safety_labels(void) {
  CHECK(!init(0));
  tyvor_ui_state_t s;
  tyvor_ui321_view_t v;
  tyvor_ui321_fixture(&s, 1000, 3);
  s.snapshot.safety_state = TYVOR_FAILSAFE;
  CHECK(!tyvor_ui321_project(&s, 1000, 1, 0, &v));
  tyvor_ui321_update(&ui, &v);
  CHECK(!strcmp(lv_label_get_text(ui.safety), "FAILSAFE"));
  s.snapshot.safety_state = TYVOR_STARTUP;
  CHECK(!tyvor_ui321_project(&s, 1000, 1, 0, &v));
  tyvor_ui321_update(&ui, &v);
  CHECK(!strcmp(lv_label_get_text(ui.safety), "SAFE / STARTUP"));
  return 1;
}
static int ui321_home_validity(void) {
  CHECK(!init(0));
  tyvor_ui_state_t s;
  tyvor_ui321_view_t v;
  tyvor_ui321_fixture(&s, 1000, 3);
  s.telemetry_expires_ms[0] = s.telemetry_expires_ms[1] =
      s.telemetry_expires_ms[3] = 1000;
  CHECK(!tyvor_ui321_project(&s, 1001, 1, 0, &v));
  tyvor_ui321_update(&ui, &v);
  CHECK(!strstr(lv_label_get_text(ui.field[1]), "98%"));
  CHECK(!v.base.lq_valid && !v.base.gps_valid);
  CHECK(lv_obj_get_style_opa(lv_obj_get_child(ui.actions[0], 0), 0) ==
        LV_OPA_30);
  return 1;
}
static int ui321_dial_mask_fit(void) {
  const int diameters[] = {58, 80, 88, 94, 130};
  for (unsigned i = 0; i < 5; i++) {
    const lv_image_dsc_t *mask = tyvor_ui321_ring_for(diameters[i]);
    CHECK(mask && mask->header.w == diameters[i] + 2 &&
          mask->header.h == diameters[i] + 2);
    CHECK(mask->data_size == (uint32_t)mask->header.stride * mask->header.h);
  }
  return 1;
}
static int ui321_capture_pipeline(void) {
  CHECK(!init(0));
  for (unsigned profile = 0; profile < 2; profile++) {
    if (profile) {
      tyvor_ui321_destroy(&ui);
      CHECK(!tyvor_ui321_init(&ui, tyvor_ui321_mock_port(&mock, 448, 368),
                              buffer, sizeof(buffer)));
    }
    ui.observe_rgb888 = tyvor_ui321_mock_capture;
    ui.observe_context = &mock;
    tyvor_ui_state_t state;
    tyvor_ui321_view_t view;
    tyvor_ui321_fixture(&state, 1000, 3);
    CHECK(!tyvor_ui321_project(&state, 1000, 1, 0, &view));
    tyvor_ui321_update(&ui, &view);
    tyvor_ui321_frame(&ui);
    for (int y = 0; y < mock.height; y++)
      for (int x = 0; x < mock.width; x++) {
        size_t i = ((size_t)y * mock.width + x) * 3;
        CHECK(mock.pixels[y * mock.width + x] ==
              tyvor_ui321_rgb565(mock.rgb888[i], mock.rgb888[i + 1],
                                 mock.rgb888[i + 2], x, y));
      }
  }
  return 1;
}
static int ui321_safety_semantics(void) {
  CHECK(!init(0));
  tyvor_ui_state_t state;
  tyvor_ui321_view_t view;
  tyvor_ui321_fixture(&state, 1000, 3);
  CHECK(!tyvor_ui321_project(&state, 1000, 1, 0, &view));
  tyvor_ui321_update(&ui, &view);
  CHECK(!strcmp(lv_label_get_text(ui.safety), "SIM GATE SAFE"));
  CHECK(strstr(lv_label_get_text(ui.field[1]), "ARM UNKNOWN"));
  CHECK(!tyvor_ui321_project(&state, 1000, 0, 0, &view));
  tyvor_ui321_update(&ui, &view);
  CHECK(!strcmp(lv_label_get_text(ui.safety), "CORE GATE SAFE"));
  CHECK(!strstr(lv_label_get_text(ui.safety), "DISARMED"));
  tyvor_ui321_fixture(&state, 1000, 2);
  CHECK(!tyvor_ui321_project(&state, 1000, 0, 0, &view));
  tyvor_ui321_update(&ui, &view);
  CHECK(!strcmp(lv_label_get_text(ui.safety), "DISCONNECTED"));
  return 1;
}
static int ui321_quick_group(void) {
  CHECK(!init(1));
  tyvor_ui321_open(&ui, UI321_QUICK);
  tyvor_ui321_frame(&ui);
  for (unsigned i = 0; i < 6; i++) {
    lv_obj_t *button = ui.actions[i], *label = lv_obj_get_child(button, 0),
             *icon = lv_obj_get_child(button, 1);
    CHECK(lv_obj_get_y(label) -
              (lv_obj_get_y(icon) + lv_obj_get_height(icon)) ==
          8);
    CHECK(lv_obj_get_y(icon) > 15 && lv_obj_get_y(label) < 75);
  }
  return 1;
}
static int ui321_font_fit(void) {
  CHECK(!init(0));
  for (unsigned profile = 0; profile < 2; profile++) {
    if (profile) {
      tyvor_ui321_destroy(&ui);
      CHECK(!tyvor_ui321_init(&ui, tyvor_ui321_mock_port(&mock, 448, 368),
                              buffer, sizeof(buffer)));
    }
    tyvor_ui_state_t state;
    tyvor_ui321_view_t view;
    tyvor_ui321_fixture(&state, 1000, 3);
    CHECK(!tyvor_ui321_project(&state, 1000, 1, 0, &view));
    const char *timers[] = {"00:00", "99:59", "--:--"};
    for (unsigned i = 0; i < 3; i++) {
      strcpy(view.timer, timers[i]);
      tyvor_ui321_update(&ui, &view);
      tyvor_ui321_frame(&ui);
      char reason[200];
      CHECK(!tyvor_ui321_layout_errors(&ui, reason, sizeof(reason)));
    }
  }
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
#define CASE(x)                                                                \
  if (!strcmp(argv[1], #x))                                                    \
  return x() ? 0 : 1
  CASE(ui321_capture_pipeline);
  CASE(ui321_safety_semantics);
  CASE(ui321_quick_group);
  CASE(ui321_font_fit);
  CASE(ui321_dial_mask_fit);
  CASE(ui321_background_assets);
  CASE(ui321_icons);
  CASE(ui321_safety_labels);
  CASE(ui321_home_validity);
  CASE(ui321_quality_logo_assets);
  CASE(ui321_quality_logo_usage);
  CASE(ui321_quality_logo_aspect);
  CASE(ui321_quality_rgb565);
  CASE(ui321_quality_wire_stride);
  CASE(ui321_quality_buffer_limits);
  CASE(ui321_quality_font_aa);
  CASE(ui321_quality_partial_invariance);
  CASE(ui321_quality_dither_mean);
  return 2;
}
