#ifndef TYVOR_UI_V31_WIDGETS_H
#define TYVOR_UI_V31_WIDGETS_H
#include "ui_theme.h"
enum {
  UI31_ICON_HOME,
  UI31_ICON_MODEL,
  UI31_ICON_INPUT,
  UI31_ICON_MIX,
  UI31_ICON_OUTPUT,
  UI31_ICON_TELEMETRY,
  UI31_ICON_SETTINGS,
  UI31_ICON_BACK,
  UI31_ICON_MORE,
  UI31_ICON_PIN,
  UI31_ICON_WAVE,
  UI31_ICON_OWL
};
lv_obj_t *tyvor_ui31_box(lv_obj_t *, int x, int y, int w, int h,
                         uint32_t color);
lv_obj_t *tyvor_ui31_label(lv_obj_t *, int x, int y, int w, const char *,
                           const lv_font_t *, uint32_t);
void tyvor_ui31_text(lv_obj_t *, const char *);
int tyvor_ui31_text_fit(lv_obj_t *, const char *, int width,
                        int maximum_font_size);
lv_obj_t *tyvor_ui31_icon(lv_obj_t *, int x, int y, unsigned icon,
                          uint32_t color);
lv_obj_t *tyvor_ui31_button(lv_obj_t *, int x, int y, int w, int h,
                            const char *, unsigned icon);
lv_obj_t *tyvor_ui31_bar(lv_obj_t *, int x, int y, int w, int h);
void tyvor_ui31_bar_set(lv_obj_t *, int value, int valid);
#endif
