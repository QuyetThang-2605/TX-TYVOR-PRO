#include "icons.h"
#include "icon_data.h"
#include "ui_theme.h"
#include <stdint.h>
lv_obj_t *tyvor_ui321_icon_sized(lv_obj_t *p, int x, int y, int size,
                                 unsigned icon, uint32_t c) {
  if (icon >= 15 || (size != 20 && size != 28 && size != 34))
    return NULL;
  unsigned slot = size == 20 ? 0 : size == 28 ? 1 : 2;
  lv_obj_t *o = lv_image_create(p);
  lv_obj_remove_style_all(o);
  lv_image_set_src(o, tyvor_ui321_icon_data[icon].images[slot]);
  lv_obj_set_style_image_recolor(o, lv_color_hex(c), 0);
  lv_obj_set_style_image_recolor_opa(o, 255, 0);
  lv_obj_set_style_text_color(o, lv_color_hex(c), 0);
  lv_obj_set_pos(o, x, y);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(o, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_user_data(o, (void *)(uintptr_t)icon);
  return o;
}
void tyvor_ui321_icon_state(lv_obj_t *o, unsigned state) {
  uint32_t color = state == UI321_ICON_ERROR     ? UI321_DANGER
                   : state == UI321_ICON_WARNING ? UI321_WARNING
                                                 : UI321_TEXT;
  lv_opa_t opa = state == UI321_ICON_MUTED ? LV_OPA_30 : LV_OPA_COVER;
  if (lv_obj_get_style_opa(o, 0) != opa)
    lv_obj_set_style_opa(o, opa, 0);
  if (!lv_color_eq(lv_obj_get_style_text_color(o, 0), lv_color_hex(color))) {
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_image_recolor(o, lv_color_hex(color), 0);
  }
}
