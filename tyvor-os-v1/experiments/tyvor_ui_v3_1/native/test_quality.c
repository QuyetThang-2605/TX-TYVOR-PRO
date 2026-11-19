#include "brand_assets.h"
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
static tyvor_ui31_t ui;
static tyvor_ui31_mock_t mock;
static uint8_t buffer[TYVOR_DISPLAY_BUFFER_BYTES];
static int init(int wide) {
  lv_init();
  return tyvor_ui31_init(
      &ui, tyvor_ui31_mock_port(&mock, wide ? 448 : 320, wide ? 368 : 240),
      buffer, sizeof(buffer));
}
static int quality_logo_assets(void) {
  const lv_image_dsc_t *images[] = {
      &tyvor_ui31_owl_compact, &tyvor_ui31_owl_large,
      &tyvor_ui31_wordmark_compact, &tyvor_ui31_wordmark_large};
  for (unsigned i = 0; i < 4; ++i) {
    const lv_image_dsc_t *a = images[i];
    CHECK(a->header.cf == LV_COLOR_FORMAT_RGB565 &&
          a->data_size == (uint32_t)a->header.w * a->header.h * 2);
    unsigned white = 0, blue = 0;
    for (unsigned j = 0; j < a->data_size; j += 2) {
      unsigned v = a->data[j] | ((unsigned)a->data[j + 1] << 8);
      white += v == 0xffff;
      blue += ((v & 31) > ((v >> 11) & 31) + 2);
    }
    CHECK(white > 5 && blue > 0);
  }
  return 1;
}
static int quality_logo_usage(void) {
  CHECK(!init(0));
  CHECK(lv_display_get_color_format(ui.display) == LV_COLOR_FORMAT_RGB888);
  CHECK(lv_image_get_src(lv_obj_get_child(ui.brand_mark, 0)) ==
        &tyvor_ui31_owl_compact);
  CHECK(lv_image_get_src(lv_obj_get_child(ui.brand_wordmark, 0)) ==
        &tyvor_ui31_wordmark_compact);
  CHECK(!ui.field[6]);
  return 1;
}
static int quality_logo_aspect(void) {
  CHECK(!init(0));
  tyvor_ui31_frame(&ui);
  lv_obj_t *im = lv_obj_get_child(ui.brand_mark, 0);
  CHECK(lv_obj_get_width(im) == 26 && lv_obj_get_height(im) == 14);
  CHECK(abs(26 * 144 - 14 * 260) <= 260 / 2);
  CHECK(lv_obj_get_width(ui.brand_mark) == 30 &&
        lv_obj_get_height(ui.brand_mark) == 18);
  return 1;
}
static int quality_rgb565(void) {
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x) {
      CHECK(tyvor_ui31_rgb565(255, 0, 0, x, y) == 0xf800);
      CHECK(tyvor_ui31_rgb565(0, 255, 0, x, y) == 0x07e0);
      CHECK(tyvor_ui31_rgb565(0, 0, 255, x, y) == 0x001f);
      CHECK(!tyvor_ui31_rgb565(0, 0, 0, x, y));
      CHECK(tyvor_ui31_rgb565(255, 255, 255, x, y) == 0xffff);
    }
  return 1;
}
static int quality_wire_stride(void) {
  uint8_t b[24] = {0, 0, 255, 0, 255, 0, 255, 0, 0, 99, 99, 99,
                   0, 0, 255, 0, 255, 0, 255, 0, 0, 99, 99, 99};
  uint8_t expected[] = {0xf8, 0, 7, 0xe0, 0, 0x1f, 0xf8, 0, 7, 0xe0, 0, 0x1f};
  CHECK(!tyvor_ui31_pack_rgb888(b, sizeof(b), 12, 3, 2, 7, 9));
  CHECK(!memcmp(b, expected, sizeof(expected)));
  return 1;
}
static int quality_buffer_limits(void) {
  static uint8_t b[TYVOR_DISPLAY_BUFFER_BYTES];
  memset(b, 77, sizeof(b));
  CHECK(tyvor_ui31_pack_rgb888(b, sizeof(b), 960, 320, 22, 0, 0) < 0 &&
        b[0] == 77);
  CHECK(!tyvor_ui31_pack_rgb888(b, sizeof(b), 960, 320, 21, 0, 0));
  CHECK(tyvor_ui31_pack_rgb888(b, sizeof(b), 1344, 448, 16, 0, 0) < 0);
  CHECK(!tyvor_ui31_pack_rgb888(b, sizeof(b), 1344, 448, 15, 0, 0));
  CHECK(tyvor_ui31_pack_rgb888(b, sizeof(b), 1, 3, 1, 0, 0) < 0);
  return 1;
}
static int quality_font_aa(void) {
  lv_init();
  lv_font_glyph_dsc_t g;
  CHECK(lv_font_get_glyph_dsc(&tyvor_ui31_timer_compact, &g, '0', ':'));
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
static int quality_partial_invariance(void) {
  CHECK(!init(0));
  static uint16_t baseline[448 * 368];
  for (unsigned profile = 0; profile < 2; ++profile) {
    if (profile) {
      tyvor_ui31_destroy(&ui);
      CHECK(!tyvor_ui31_init(&ui, tyvor_ui31_mock_port(&mock, 448, 368), buffer,
                             sizeof(buffer)));
    }
    tyvor_ui_state_t s;
    tyvor_ui31_view_t v;
    tyvor_ui31_fixture(&s, 1000, 3);
    CHECK(!tyvor_ui31_project(&s, 1000, 1, 0, &v));
    tyvor_ui31_update(&ui, &v);
    tyvor_ui31_frame(&ui);
    size_t n = (size_t)mock.width * mock.height * 2;
    memcpy(baseline, mock.pixels, n);
    strcpy(v.quality, "33%");
    tyvor_ui31_update(&ui, &v);
    tyvor_ui31_frame(&ui);
    strcpy(v.quality, "98%");
    tyvor_ui31_update(&ui, &v);
    tyvor_ui31_frame(&ui);
    CHECK(!memcmp(baseline, mock.pixels, n));
    lv_obj_invalidate(ui.root);
    tyvor_ui31_frame(&ui);
    CHECK(!memcmp(baseline, mock.pixels, n));
  }
  return 1;
}
static int quality_dither_mean(void) {
  for (unsigned value = 0; value < 256; ++value) {
    double r = 0, g = 0, b = 0;
    for (int y = 0; y < 4; ++y)
      for (int x = 0; x < 4; ++x) {
        unsigned c = tyvor_ui31_rgb565(value, value, value, x, y);
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
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
#define CASE(x)                                                                \
  if (!strcmp(argv[1], #x))                                                    \
  return x() ? 0 : 1
  CASE(quality_logo_assets);
  CASE(quality_logo_usage);
  CASE(quality_logo_aspect);
  CASE(quality_rgb565);
  CASE(quality_wire_stride);
  CASE(quality_buffer_limits);
  CASE(quality_font_aa);
  CASE(quality_partial_invariance);
  CASE(quality_dither_mean);
  return 2;
}
