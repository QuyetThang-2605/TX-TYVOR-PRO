#include "ui_widgets.h"
#include "icons.h"
#include <string.h>
lv_obj_t *tyvor_ui32_box(lv_obj_t *p, int x, int y, int w, int h,
                         uint32_t color) {
  lv_obj_t *o = lv_obj_create(p);
  tyvor_ui32_surface(o, color, 8);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  return o;
}
lv_obj_t *tyvor_ui32_label(lv_obj_t *p, int x, int y, int w, const char *s,
                           const lv_font_t *font, uint32_t color) {
  lv_obj_t *o = lv_label_create(p);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_width(o, w);
  lv_label_set_text(o, s);
  lv_obj_set_style_text_font(o, font, 0);
  lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
  lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}
void tyvor_ui32_text(lv_obj_t *o, const char *s) {
  if (o && strcmp(lv_label_get_text(o), s))
    lv_label_set_text(o, s);
}
int tyvor_ui32_text_fit(lv_obj_t *o, const char *s, int width,
                        int maximum_font_size) {
  const lv_font_t *fonts[] = {&lv_font_montserrat_20, &lv_font_montserrat_14,
                              &lv_font_montserrat_12};
  for (unsigned i = maximum_font_size < 20 ? 1 : 0; i < 3; ++i) {
    lv_point_t size;
    lv_text_get_size(&size, s, fonts[i], 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x <= width) {
      if (lv_obj_get_style_text_font(o, 0) != fonts[i])
        lv_obj_set_style_text_font(o, fonts[i], 0);
      tyvor_ui32_text(o, s);
      return 0;
    }
  }
  return -1;
}
/* Pinned Lucide outlines. Actual LVGL draw events, no icon screenshot. */
lv_obj_t *tyvor_ui32_icon(lv_obj_t *p, int x, int y, unsigned icon,
                          uint32_t c) {
  return tyvor_ui32_icon_sized(p, x, y, 20, icon, c);
}
lv_obj_t *tyvor_ui32_button(lv_obj_t *p, int x, int y, int w, int h,
                            const char *s, unsigned icon) {
  lv_obj_t *o = lv_button_create(p);
  tyvor_ui32_surface(o, UI32_CARD, 10);
  lv_obj_set_style_bg_opa(o, 180, 0);
  lv_obj_set_style_border_width(o, 1, 0);
  lv_obj_set_style_border_color(o, lv_color_hex(0x659ACF), 0);
  lv_obj_set_style_border_opa(o, 65, 0);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(UI32_CARD), LV_STATE_PRESSED);
  if (s) {
    lv_obj_t *l = tyvor_ui32_label(o, 4, h - 17, w - 8, s,
                                   &lv_font_montserrat_12, UI32_MUTED);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  }
  int size = !s && w >= 45 && h >= 45 ? (w >= 70 ? 34 : 28) : 20;
  tyvor_ui32_icon_sized(o, (w - size) / 2, s ? 3 : (h - size) / 2, size, icon,
                        UI32_TEXT);
  return o;
}
lv_obj_t *tyvor_ui32_bar(lv_obj_t *p, int x, int y, int w, int h) {
  lv_obj_t *o = lv_bar_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_bar_set_range(o, -1000, 1000);
  lv_obj_set_style_bg_color(o, lv_color_hex(UI32_SURFACE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(o, 3, LV_PART_MAIN);
  lv_obj_set_style_bg_color(o, lv_color_hex(UI32_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(o, 3, LV_PART_INDICATOR);
  return o;
}
void tyvor_ui32_bar_set(lv_obj_t *o, int value, int valid) {
  if (!o)
    return;
  int next = valid ? value : 0;
  if (next < -1000)
    next = -1000;
  if (next > 1000)
    next = 1000;
  if (lv_bar_get_value(o) != next)
    lv_bar_set_value(o, next, LV_ANIM_OFF);
  lv_opa_t opacity = valid ? LV_OPA_COVER : LV_OPA_30;
  if (lv_obj_get_style_opa(o, 0) != opacity)
    lv_obj_set_style_opa(o, opacity, 0);
}
