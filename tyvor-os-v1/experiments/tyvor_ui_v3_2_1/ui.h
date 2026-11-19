#ifndef TYVOR_UI_V321_H
#define TYVOR_UI_V321_H
#include "display_contract.h"
#include "timer_font.h"
#include "ui_view.h"
#include "ui_widgets.h"
enum {
  UI321_HOME,
  UI321_MODEL,
  UI321_INPUTS,
  UI321_MIXES,
  UI321_OUTPUTS,
  UI321_TELEMETRY,
  UI321_SETTINGS,
  UI321_QUICK,
  UI321_CONNECTIONS,
  UI321_PAGE_COUNT
};
enum {
  UI321_GO_HOME,
  UI321_BACK,
  UI321_NEXT,
  UI321_PREVIOUS,
  UI321_SWIPE_LEFT,
  UI321_SWIPE_UP,
  UI321_MORE,
  UI321_SWIPE_DOWN
};
typedef struct {
  void *context;
  int width, height;
  int (*write)(void *, const tyvor_display_region_t *, const uint8_t *, size_t);
  uint64_t (*micros)(void *);
} tyvor_ui321_port_t;
typedef struct {
  int16_t x, y;
  uint8_t valid;
} tyvor_ui321_stick_t;
typedef struct {
  tyvor_ui321_port_t port;
  /* Optional synchronous diagnostic tap before in-place RGB565 packing.
     Copies only; callback must not retain or mutate LVGL-owned buffer. */
  void (*observe_rgb888)(void *, const tyvor_display_region_t *,
                         const uint8_t *, size_t);
  void *observe_context;
  tyvor_ui321_layout_t layout;
  tyvor_ui321_view_t view;
  lv_display_t *display;
  lv_obj_t *root, *body, *top_model, *top_state, *top_detail, *safety,
      *throttle, *page_title;
  lv_obj_t *link_icon;
  lv_obj_t *brand_mark, *brand_wordmark;
  lv_obj_t *field[48], *bars[8], *sticks[2], *actions[12], *hero, *footer[4],
      *back, *home, *more;
  tyvor_ui321_stick_t stick_data[2];
  unsigned page, subpage, history[16], history_count;
  uint32_t allocations_on_navigation, frames, flushes;
  uint8_t has_view;
  uint64_t frame_us, frame_max_us, pixels;
} tyvor_ui321_t;
int tyvor_ui321_init(tyvor_ui321_t *, tyvor_ui321_port_t, void *buffer,
                     size_t bytes);
void tyvor_ui321_update(tyvor_ui321_t *, const tyvor_ui321_view_t *);
void tyvor_ui321_open(tyvor_ui321_t *, unsigned page);
void tyvor_ui321_action(tyvor_ui321_t *, unsigned action);
void tyvor_ui321_gesture(tyvor_ui321_t *, int dx, int dy);
void tyvor_ui321_frame(tyvor_ui321_t *);
void tyvor_ui321_destroy(tyvor_ui321_t *);
int tyvor_ui321_layout_errors(tyvor_ui321_t *, char *reason, size_t capacity);
void tyvor_ui321_pages_create(tyvor_ui321_t *);
void tyvor_ui321_pages_update(tyvor_ui321_t *);
void tyvor_ui321_bind(lv_obj_t *, tyvor_ui321_t *, unsigned action, int page);
const char *tyvor_ui321_page_name(unsigned);
int tyvor_ui321_x(const tyvor_ui321_t *, int canonical);
int tyvor_ui321_y(const tyvor_ui321_t *, int canonical);
void tyvor_ui321_background(lv_event_t *);
lv_obj_t *tyvor_ui321_dial(lv_obj_t *, int, int, int, int,
                           tyvor_ui321_stick_t *);
#endif
