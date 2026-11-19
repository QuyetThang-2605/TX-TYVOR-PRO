#ifndef TYVOR_UI_V31_THEME_H
#define TYVOR_UI_V31_THEME_H
#include "lvgl.h"
enum {
  UI31_BG = 0x1A2E43,
  UI31_SURFACE = 0x45687B,
  UI31_CARD = 0x405A70,
  UI31_ACCENT = 0x78CDEA,
  UI31_SUCCESS = 0x86D35B,
  UI31_WARNING = 0xFFB030,
  UI31_DANGER = 0xFF3C4E,
  UI31_TEXT = 0xD7E6EF,
  UI31_MUTED = 0xA4B9C7
};
typedef struct {
  int width, height, pad, gap, top, body_y, body_h, wide;
} tyvor_ui31_layout_t;
int tyvor_ui31_layout(int, int, tyvor_ui31_layout_t *);
void tyvor_ui31_surface(lv_obj_t *, uint32_t color, int radius);
#endif
