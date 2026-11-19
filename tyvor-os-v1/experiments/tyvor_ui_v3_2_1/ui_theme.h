#ifndef TYVOR_UI_V321_THEME_H
#define TYVOR_UI_V321_THEME_H
#include "lvgl.h"
enum {
  UI321_BG = 0x031B48,
  UI321_SURFACE = 0x164F8C,
  UI321_CARD = 0x174C85,
  UI321_ACCENT = 0x65CFFF,
  UI321_SUCCESS = 0x72EA49,
  UI321_WARNING = 0xFFD23F,
  UI321_DANGER = 0xFF3C4E,
  UI321_TEXT = 0xF4F8FF,
  UI321_MUTED = 0xB9D2EC
};
typedef struct {
  int width, height, pad, gap, top, body_y, body_h, wide;
} tyvor_ui321_layout_t;
int tyvor_ui321_layout(int, int, tyvor_ui321_layout_t *);
void tyvor_ui321_surface(lv_obj_t *, uint32_t color, int radius);
#endif
