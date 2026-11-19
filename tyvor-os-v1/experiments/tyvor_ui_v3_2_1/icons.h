#ifndef TYVOR32_ICONS_H
#define TYVOR32_ICONS_H
#include "lvgl.h"
enum {
  UI321_ICON_VALID,
  UI321_ICON_MUTED,
  UI321_ICON_WARNING,
  UI321_ICON_ERROR
};
lv_obj_t *tyvor_ui321_icon_sized(lv_obj_t *, int, int, int, unsigned, uint32_t);
void tyvor_ui321_icon_state(lv_obj_t *, unsigned);
const lv_image_dsc_t *tyvor_ui321_ring_for(int diameter);
#endif
