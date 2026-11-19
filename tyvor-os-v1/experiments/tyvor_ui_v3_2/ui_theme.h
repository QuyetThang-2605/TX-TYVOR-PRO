#ifndef TYVOR_UI_V32_THEME_H
#define TYVOR_UI_V32_THEME_H
#include "lvgl.h"
enum {
  UI32_BG = 0x031B48,
  UI32_SURFACE = 0x164F8C,
  UI32_CARD = 0x174C85,
  UI32_ACCENT = 0x65CFFF,
  UI32_SUCCESS = 0x72EA49,
  UI32_WARNING = 0xFFD23F,
  UI32_DANGER = 0xFF3C4E,
  UI32_TEXT = 0xF4F8FF,
  UI32_MUTED = 0xB9D2EC
};
typedef struct {
  int width, height, pad, gap, top, body_y, body_h, wide;
} tyvor_ui32_layout_t;
int tyvor_ui32_layout(int, int, tyvor_ui32_layout_t *);
void tyvor_ui32_surface(lv_obj_t *, uint32_t color, int radius);
#endif
