#ifndef TYVOR_UI_V321_WIDGETS_H
#define TYVOR_UI_V321_WIDGETS_H
#include "ui_theme.h"
enum {
  UI321_ICON_PIN,
  UI321_ICON_WAVE,
  UI321_ICON_HOME,
  UI321_ICON_MODEL,
  UI321_ICON_INPUT,
  UI321_ICON_MIX,
  UI321_ICON_OUTPUT,
  UI321_ICON_TELEMETRY,
  UI321_ICON_BACK,
  UI321_ICON_MORE,
  UI321_ICON_SETTINGS,
  UI321_ICON_BATTERY,
  UI321_ICON_LINK,
  UI321_ICON_QUICK,
  UI321_ICON_HELP
};
lv_obj_t *tyvor_ui321_box(lv_obj_t *, int x, int y, int w, int h,
                          uint32_t color);
lv_obj_t *tyvor_ui321_label(lv_obj_t *, int x, int y, int w, const char *,
                            const lv_font_t *, uint32_t);
void tyvor_ui321_text(lv_obj_t *, const char *);
int tyvor_ui321_text_fit(lv_obj_t *, const char *, int width,
                         int maximum_font_size);
lv_obj_t *tyvor_ui321_icon(lv_obj_t *, int x, int y, unsigned icon,
                           uint32_t color);
lv_obj_t *tyvor_ui321_button(lv_obj_t *, int x, int y, int w, int h,
                             const char *, unsigned icon);
lv_obj_t *tyvor_ui321_bar(lv_obj_t *, int x, int y, int w, int h);
void tyvor_ui321_bar_set(lv_obj_t *, int value, int valid);
#endif
