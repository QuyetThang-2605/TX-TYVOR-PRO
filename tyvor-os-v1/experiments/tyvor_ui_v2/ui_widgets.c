#include "ui_widgets.h"
#include <string.h>
lv_obj_t *tyvor_ui2_box(lv_obj_t *p, int x, int y, int w, int h,
                        uint32_t color) {
  lv_obj_t *o = lv_obj_create(p);
  tyvor_ui2_surface(o, color, 8);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  return o;
}
lv_obj_t *tyvor_ui2_label(lv_obj_t *p, int x, int y, int w, const char *s,
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
void tyvor_ui2_text(lv_obj_t *o, const char *s) {
  if (o && strcmp(lv_label_get_text(o), s))
    lv_label_set_text(o, s);
}
int tyvor_ui2_text_fit(lv_obj_t *o, const char *s, int width,
                       int maximum_font_size) {
  const lv_font_t *fonts[] = {&lv_font_montserrat_20, &lv_font_montserrat_14,
                              &lv_font_montserrat_12};
  for (unsigned i = maximum_font_size < 20 ? 1 : 0; i < 3; ++i) {
    lv_point_t size;
    lv_text_get_size(&size, s, fonts[i], 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x <= width) {
      if (lv_obj_get_style_text_font(o, 0) != fonts[i])
        lv_obj_set_style_text_font(o, fonts[i], 0);
      tyvor_ui2_text(o, s);
      return 0;
    }
  }
  return -1;
}
/* Original 20x20 line glyphs, built from LVGL vector primitives; no brand
 * artwork/font icons. */
static void segment(lv_obj_t *p, int x, int y, int w, int h, uint32_t c) {
  lv_obj_t *o = tyvor_ui2_box(p, x, y, w, h, c);
  lv_obj_set_style_radius(o, 1, 0);
  lv_obj_add_flag(o, LV_OBJ_FLAG_EVENT_BUBBLE);
}
lv_obj_t *tyvor_ui2_icon(lv_obj_t *p, int x, int y, unsigned icon, uint32_t c) {
  lv_obj_t *o = tyvor_ui2_box(p, x, y, 20, 20, UI2_SURFACE);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(o, LV_OBJ_FLAG_EVENT_BUBBLE);
  switch (icon) {
  case UI2_ICON_HOME:
    segment(o, 4, 7, 12, 2, c);
    segment(o, 4, 7, 2, 10, c);
    segment(o, 14, 7, 2, 10, c);
    segment(o, 4, 15, 12, 2, c);
    segment(o, 7, 3, 6, 2, c);
    break;
  case UI2_ICON_MODEL:
    segment(o, 3, 4, 14, 2, c);
    segment(o, 3, 9, 14, 2, c);
    segment(o, 3, 14, 14, 2, c);
    break;
  case UI2_ICON_INPUT:
    segment(o, 9, 2, 2, 16, c);
    segment(o, 2, 9, 16, 2, c);
    segment(o, 6, 6, 8, 2, c);
    segment(o, 6, 12, 8, 2, c);
    break;
  case UI2_ICON_MIX:
    segment(o, 3, 3, 2, 14, c);
    segment(o, 9, 3, 2, 14, c);
    segment(o, 15, 3, 2, 14, c);
    segment(o, 1, 6, 6, 3, c);
    segment(o, 7, 12, 6, 3, c);
    segment(o, 13, 7, 6, 3, c);
    break;
  case UI2_ICON_OUTPUT:
  case UI2_ICON_TELEMETRY:
    segment(o, 3, 12, 3, 5, c);
    segment(o, 8, 8, 3, 9, c);
    segment(o, 13, 3, 3, 14, c);
    break;
  case UI2_ICON_BACK:
    segment(o, 4, 9, 13, 2, c);
    segment(o, 4, 6, 2, 8, c);
    segment(o, 6, 4, 2, 2, c);
    segment(o, 6, 14, 2, 2, c);
    break;
  case UI2_ICON_MORE:
    segment(o, 2, 8, 3, 3, c);
    segment(o, 8, 8, 3, 3, c);
    segment(o, 14, 8, 3, 3, c);
    break;
  default:
    segment(o, 3, 4, 14, 2, c);
    segment(o, 3, 9, 14, 2, c);
    segment(o, 3, 14, 14, 2, c);
    segment(o, 7, 2, 3, 6, c);
    segment(o, 12, 7, 3, 6, c);
    segment(o, 6, 12, 3, 6, c);
    break;
  }
  return o;
}
lv_obj_t *tyvor_ui2_button(lv_obj_t *p, int x, int y, int w, int h,
                           const char *s, unsigned icon) {
  lv_obj_t *o = lv_button_create(p);
  tyvor_ui2_surface(o, UI2_SURFACE, 6);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(UI2_CARD), LV_STATE_PRESSED);
  if (s) {
    lv_obj_t *l = tyvor_ui2_label(o, 4, h - 17, w - 8, s,
                                  &lv_font_montserrat_12, UI2_MUTED);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  }
  tyvor_ui2_icon(o, (w - 20) / 2, s ? 3 : (h - 20) / 2, icon, UI2_ACCENT);
  return o;
}
lv_obj_t *tyvor_ui2_bar(lv_obj_t *p, int x, int y, int w, int h) {
  lv_obj_t *o = lv_bar_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_bar_set_range(o, -1000, 1000);
  lv_obj_set_style_bg_color(o, lv_color_hex(UI2_SURFACE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(o, 3, LV_PART_MAIN);
  lv_obj_set_style_bg_color(o, lv_color_hex(UI2_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(o, 3, LV_PART_INDICATOR);
  return o;
}
void tyvor_ui2_bar_set(lv_obj_t *o, int value, int valid) {
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
