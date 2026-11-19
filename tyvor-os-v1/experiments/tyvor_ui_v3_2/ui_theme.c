#include "ui_theme.h"
int tyvor_ui32_layout(int w, int h, tyvor_ui32_layout_t *l) {
  if (!l || !((w == 320 && h == 240) || (w == 448 && h == 368)))
    return -1;
  int top = h * 36 / 315;
  *l = (tyvor_ui32_layout_t){
      w, h, w == 320 ? 14 : 20, w == 320 ? 6 : 10, top, top, h - top, w == 448};
  return 0;
}
void tyvor_ui32_surface(lv_obj_t *o, uint32_t color, int radius) {
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_border_width(o, 0, 0);
}
