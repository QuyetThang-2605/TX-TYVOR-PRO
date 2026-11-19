#ifndef TYVOR_UI_V32_WIDGETS_H
#define TYVOR_UI_V32_WIDGETS_H
#include "ui_theme.h"
enum {
  UI32_ICON_PIN,
  UI32_ICON_WAVE,
  UI32_ICON_HOME,
  UI32_ICON_MODEL,
  UI32_ICON_INPUT,
  UI32_ICON_MIX,
  UI32_ICON_OUTPUT,
  UI32_ICON_TELEMETRY,
  UI32_ICON_BACK,
  UI32_ICON_MORE,
  UI32_ICON_SETTINGS,
  UI32_ICON_BATTERY,
  UI32_ICON_LINK,
  UI32_ICON_QUICK,
  UI32_ICON_HELP
};
lv_obj_t *tyvor_ui32_box(lv_obj_t *, int x, int y, int w, int h,
                         uint32_t color);
lv_obj_t *tyvor_ui32_label(lv_obj_t *, int x, int y, int w, const char *,
                           const lv_font_t *, uint32_t);
void tyvor_ui32_text(lv_obj_t *, const char *);
int tyvor_ui32_text_fit(lv_obj_t *, const char *, int width,
                        int maximum_font_size);
lv_obj_t *tyvor_ui32_icon(lv_obj_t *, int x, int y, unsigned icon,
                          uint32_t color);
lv_obj_t *tyvor_ui32_button(lv_obj_t *, int x, int y, int w, int h,
                            const char *, unsigned icon);
lv_obj_t *tyvor_ui32_bar(lv_obj_t *, int x, int y, int w, int h);
void tyvor_ui32_bar_set(lv_obj_t *, int value, int valid);
#endif
