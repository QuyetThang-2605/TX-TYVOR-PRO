#ifndef TYVOR_UI_V3_WIDGETS_H
#define TYVOR_UI_V3_WIDGETS_H
#include "ui_theme.h"
enum {
  UI3_ICON_HOME,
  UI3_ICON_MODEL,
  UI3_ICON_INPUT,
  UI3_ICON_MIX,
  UI3_ICON_OUTPUT,
  UI3_ICON_TELEMETRY,
  UI3_ICON_SETTINGS,
  UI3_ICON_BACK,
  UI3_ICON_MORE,
  UI3_ICON_PIN,
  UI3_ICON_WAVE,
  UI3_ICON_OWL
};
lv_obj_t *tyvor_ui3_box(lv_obj_t *, int x, int y, int w, int h, uint32_t color);
lv_obj_t *tyvor_ui3_label(lv_obj_t *, int x, int y, int w, const char *,
                          const lv_font_t *, uint32_t);
void tyvor_ui3_text(lv_obj_t *, const char *);
int tyvor_ui3_text_fit(lv_obj_t *, const char *, int width,
                       int maximum_font_size);
lv_obj_t *tyvor_ui3_icon(lv_obj_t *, int x, int y, unsigned icon,
                         uint32_t color);
lv_obj_t *tyvor_ui3_button(lv_obj_t *, int x, int y, int w, int h, const char *,
                           unsigned icon);
lv_obj_t *tyvor_ui3_bar(lv_obj_t *, int x, int y, int w, int h);
void tyvor_ui3_bar_set(lv_obj_t *, int value, int valid);
#endif
