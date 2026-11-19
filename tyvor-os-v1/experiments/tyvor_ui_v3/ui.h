#ifndef TYVOR_UI_V3_H
#define TYVOR_UI_V3_H
#include "display_contract.h"
#include "timer_font.h"
#include "ui_view.h"
#include "ui_widgets.h"
enum {
  UI3_HOME,
  UI3_MODEL,
  UI3_INPUTS,
  UI3_MIXES,
  UI3_OUTPUTS,
  UI3_TELEMETRY,
  UI3_SETTINGS,
  UI3_QUICK,
  UI3_CONNECTIONS,
  UI3_PAGE_COUNT
};
enum {
  UI3_GO_HOME,
  UI3_BACK,
  UI3_NEXT,
  UI3_PREVIOUS,
  UI3_SWIPE_LEFT,
  UI3_SWIPE_UP,
  UI3_MORE,
  UI3_SWIPE_DOWN
};
typedef struct {
  void *context;
  int width, height;
  int (*write)(void *, const tyvor_display_region_t *, const uint8_t *, size_t);
  uint64_t (*micros)(void *);
} tyvor_ui3_port_t;
typedef struct {
  int16_t x, y;
  uint8_t valid;
} tyvor_ui3_stick_t;
typedef struct {
  tyvor_ui3_port_t port;
  tyvor_ui3_layout_t layout;
  tyvor_ui3_view_t view;
  lv_display_t *display;
  lv_obj_t *root, *body, *top_model, *top_state, *top_detail, *safety,
      *throttle, *page_title;
  lv_obj_t *link_icon;
  lv_obj_t *field[48], *bars[8], *sticks[2], *actions[12], *hero, *footer[4],
      *back, *home, *more;
  tyvor_ui3_stick_t stick_data[2];
  unsigned page, subpage, history[16], history_count;
  uint32_t allocations_on_navigation, frames, flushes;
  uint8_t has_view;
  uint64_t frame_us, frame_max_us, pixels;
} tyvor_ui3_t;
int tyvor_ui3_init(tyvor_ui3_t *, tyvor_ui3_port_t, void *buffer, size_t bytes);
void tyvor_ui3_update(tyvor_ui3_t *, const tyvor_ui3_view_t *);
void tyvor_ui3_open(tyvor_ui3_t *, unsigned page);
void tyvor_ui3_action(tyvor_ui3_t *, unsigned action);
void tyvor_ui3_gesture(tyvor_ui3_t *, int dx, int dy);
void tyvor_ui3_frame(tyvor_ui3_t *);
void tyvor_ui3_destroy(tyvor_ui3_t *);
int tyvor_ui3_layout_errors(tyvor_ui3_t *, char *reason, size_t capacity);
void tyvor_ui3_pages_create(tyvor_ui3_t *);
void tyvor_ui3_pages_update(tyvor_ui3_t *);
void tyvor_ui3_bind(lv_obj_t *, tyvor_ui3_t *, unsigned action, int page);
const char *tyvor_ui3_page_name(unsigned);
int tyvor_ui3_x(const tyvor_ui3_t *, int canonical);
int tyvor_ui3_y(const tyvor_ui3_t *, int canonical);
void tyvor_ui3_background(lv_event_t *);
lv_obj_t *tyvor_ui3_dial(lv_obj_t *, int, int, int, int, tyvor_ui3_stick_t *);
#endif
