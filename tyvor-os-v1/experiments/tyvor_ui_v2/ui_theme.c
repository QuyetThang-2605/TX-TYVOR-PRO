#include "ui_theme.h"
int tyvor_ui2_layout(int w, int h, tyvor_ui2_layout_t *l) {
  if (!l || !((w == 240 && h == 320) || (w == 368 && h == 448)))
    return -1;
  *l = (tyvor_ui2_layout_t){
      w,  h,  w == 240 ? 10 : 16, w == 240 ? 6 : 10, 36, 26,
      44, 68, h - 68 - 44,        w == 368};
  return 0;
}
void tyvor_ui2_surface(lv_obj_t *o, uint32_t color, int radius) {
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_border_width(o, 0, 0);
}
