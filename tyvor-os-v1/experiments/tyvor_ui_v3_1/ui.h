#ifndef TYVOR_UI_V31_H
#define TYVOR_UI_V31_H
#include "display_contract.h"
#include "timer_font.h"
#include "ui_view.h"
#include "ui_widgets.h"
enum {
  UI31_HOME,
  UI31_MODEL,
  UI31_INPUTS,
  UI31_MIXES,
  UI31_OUTPUTS,
  UI31_TELEMETRY,
  UI31_SETTINGS,
  UI31_QUICK,
  UI31_CONNECTIONS,
  UI31_PAGE_COUNT
};
enum {
  UI31_GO_HOME,
  UI31_BACK,
  UI31_NEXT,
  UI31_PREVIOUS,
  UI31_SWIPE_LEFT,
  UI31_SWIPE_UP,
  UI31_MORE,
  UI31_SWIPE_DOWN
};
typedef struct {
  void *context;
  int width, height;
  int (*write)(void *, const tyvor_display_region_t *, const uint8_t *, size_t);
  uint64_t (*micros)(void *);
} tyvor_ui31_port_t;
typedef struct {
  int16_t x, y;
  uint8_t valid;
} tyvor_ui31_stick_t;
typedef struct {
  tyvor_ui31_port_t port;
  tyvor_ui31_layout_t layout;
  tyvor_ui31_view_t view;
  lv_display_t *display;
  lv_obj_t *root, *body, *top_model, *top_state, *top_detail, *safety,
      *throttle, *page_title;
  lv_obj_t *link_icon;
  lv_obj_t *brand_mark, *brand_wordmark;
  lv_obj_t *field[48], *bars[8], *sticks[2], *actions[12], *hero, *footer[4],
      *back, *home, *more;
  tyvor_ui31_stick_t stick_data[2];
  unsigned page, subpage, history[16], history_count;
  uint32_t allocations_on_navigation, frames, flushes;
  uint8_t has_view;
  uint64_t frame_us, frame_max_us, pixels;
} tyvor_ui31_t;
int tyvor_ui31_init(tyvor_ui31_t *, tyvor_ui31_port_t, void *buffer,
                    size_t bytes);
void tyvor_ui31_update(tyvor_ui31_t *, const tyvor_ui31_view_t *);
void tyvor_ui31_open(tyvor_ui31_t *, unsigned page);
void tyvor_ui31_action(tyvor_ui31_t *, unsigned action);
void tyvor_ui31_gesture(tyvor_ui31_t *, int dx, int dy);
void tyvor_ui31_frame(tyvor_ui31_t *);
void tyvor_ui31_destroy(tyvor_ui31_t *);
int tyvor_ui31_layout_errors(tyvor_ui31_t *, char *reason, size_t capacity);
void tyvor_ui31_pages_create(tyvor_ui31_t *);
void tyvor_ui31_pages_update(tyvor_ui31_t *);
void tyvor_ui31_bind(lv_obj_t *, tyvor_ui31_t *, unsigned action, int page);
const char *tyvor_ui31_page_name(unsigned);
int tyvor_ui31_x(const tyvor_ui31_t *, int canonical);
int tyvor_ui31_y(const tyvor_ui31_t *, int canonical);
void tyvor_ui31_background(lv_event_t *);
lv_obj_t *tyvor_ui31_dial(lv_obj_t *, int, int, int, int, tyvor_ui31_stick_t *);
#endif
