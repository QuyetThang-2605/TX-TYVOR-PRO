#ifndef TYVOR32_ICONS_H
#define TYVOR32_ICONS_H
#include "lvgl.h"
enum { UI32_ICON_VALID, UI32_ICON_MUTED, UI32_ICON_WARNING, UI32_ICON_ERROR };
lv_obj_t *tyvor_ui32_icon_sized(lv_obj_t *, int, int, int, unsigned, uint32_t);
void tyvor_ui32_icon_state(lv_obj_t *, unsigned);
const lv_image_dsc_t *tyvor_ui32_ring_for(int diameter);
#endif
