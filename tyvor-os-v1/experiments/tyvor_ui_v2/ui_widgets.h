#ifndef TYVOR_UI_V2_WIDGETS_H
#define TYVOR_UI_V2_WIDGETS_H
#include "ui_theme.h"
enum {
  UI2_ICON_HOME,
  UI2_ICON_MODEL,
  UI2_ICON_INPUT,
  UI2_ICON_MIX,
  UI2_ICON_OUTPUT,
  UI2_ICON_TELEMETRY,
  UI2_ICON_SETTINGS,
  UI2_ICON_BACK,
  UI2_ICON_MORE
};
lv_obj_t *tyvor_ui2_box(lv_obj_t *, int x, int y, int w, int h, uint32_t color);
lv_obj_t *tyvor_ui2_label(lv_obj_t *, int x, int y, int w, const char *,
                          const lv_font_t *, uint32_t);
void tyvor_ui2_text(lv_obj_t *, const char *);
int tyvor_ui2_text_fit(lv_obj_t *, const char *, int width,
                       int maximum_font_size);
lv_obj_t *tyvor_ui2_icon(lv_obj_t *, int x, int y, unsigned icon,
                         uint32_t color);
lv_obj_t *tyvor_ui2_button(lv_obj_t *, int x, int y, int w, int h, const char *,
                           unsigned icon);
lv_obj_t *tyvor_ui2_bar(lv_obj_t *, int x, int y, int w, int h);
void tyvor_ui2_bar_set(lv_obj_t *, int value, int valid);
#endif
