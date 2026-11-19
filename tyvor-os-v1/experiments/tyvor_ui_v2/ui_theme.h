#ifndef TYVOR_UI_V2_THEME_H
#define TYVOR_UI_V2_THEME_H
#include "lvgl.h"
enum {
  UI2_BG = 0x0B1220,
  UI2_SURFACE = 0x111C2D,
  UI2_CARD = 0x17263C,
  UI2_ACCENT = 0x38BDF8,
  UI2_SUCCESS = 0x22C55E,
  UI2_WARNING = 0xF59E0B,
  UI2_DANGER = 0xEF4444,
  UI2_TEXT = 0xF1F5F9,
  UI2_MUTED = 0x94A3B8
};
typedef struct {
  int width, height, pad, gap, top, safety, nav, body_y, body_h, wide;
} tyvor_ui2_layout_t;
int tyvor_ui2_layout(int width, int height, tyvor_ui2_layout_t *);
void tyvor_ui2_surface(lv_obj_t *, uint32_t color, int radius);
#endif
