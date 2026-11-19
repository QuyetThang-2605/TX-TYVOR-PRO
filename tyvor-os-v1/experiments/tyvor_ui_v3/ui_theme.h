#ifndef TYVOR_UI_V3_THEME_H
#define TYVOR_UI_V3_THEME_H
#include "lvgl.h"
enum {
  UI3_BG = 0x23394C,
  UI3_SURFACE = 0x4B7380,
  UI3_CARD = 0x3B526A,
  UI3_ACCENT = 0x78CDEA,
  UI3_SUCCESS = 0x86D35B,
  UI3_WARNING = 0xFFB030,
  UI3_DANGER = 0xFF3C4E,
  UI3_TEXT = 0xE2ECF1,
  UI3_MUTED = 0xA4B9C7
};
typedef struct {
  int width, height, pad, gap, top, body_y, body_h, wide;
} tyvor_ui3_layout_t;
int tyvor_ui3_layout(int, int, tyvor_ui3_layout_t *);
void tyvor_ui3_surface(lv_obj_t *, uint32_t color, int radius);
#endif
