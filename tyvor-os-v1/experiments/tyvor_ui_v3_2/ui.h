#ifndef TYVOR_UI_V32_H
#define TYVOR_UI_V32_H
#include "display_contract.h"
#include "timer_font.h"
#include "ui_view.h"
#include "ui_widgets.h"
enum {
  UI32_HOME,
  UI32_MODEL,
  UI32_INPUTS,
  UI32_MIXES,
  UI32_OUTPUTS,
  UI32_TELEMETRY,
  UI32_SETTINGS,
  UI32_QUICK,
  UI32_CONNECTIONS,
  UI32_PAGE_COUNT
};
enum {
  UI32_GO_HOME,
  UI32_BACK,
  UI32_NEXT,
  UI32_PREVIOUS,
  UI32_SWIPE_LEFT,
  UI32_SWIPE_UP,
  UI32_MORE,
  UI32_SWIPE_DOWN
};
typedef struct {
  void *context;
  int width, height;
  int (*write)(void *, const tyvor_display_region_t *, const uint8_t *, size_t);
  uint64_t (*micros)(void *);
} tyvor_ui32_port_t;
typedef struct {
  int16_t x, y;
  uint8_t valid;
} tyvor_ui32_stick_t;
typedef struct {
  tyvor_ui32_port_t port;
  tyvor_ui32_layout_t layout;
  tyvor_ui32_view_t view;
  lv_display_t *display;
  lv_obj_t *root, *body, *top_model, *top_state, *top_detail, *safety,
      *throttle, *page_title;
  lv_obj_t *link_icon;
  lv_obj_t *brand_mark, *brand_wordmark;
  lv_obj_t *field[48], *bars[8], *sticks[2], *actions[12], *hero, *footer[4],
      *back, *home, *more;
  tyvor_ui32_stick_t stick_data[2];
  unsigned page, subpage, history[16], history_count;
  uint32_t allocations_on_navigation, frames, flushes;
  uint8_t has_view;
  uint64_t frame_us, frame_max_us, pixels;
} tyvor_ui32_t;
int tyvor_ui32_init(tyvor_ui32_t *, tyvor_ui32_port_t, void *buffer,
                    size_t bytes);
void tyvor_ui32_update(tyvor_ui32_t *, const tyvor_ui32_view_t *);
void tyvor_ui32_open(tyvor_ui32_t *, unsigned page);
void tyvor_ui32_action(tyvor_ui32_t *, unsigned action);
void tyvor_ui32_gesture(tyvor_ui32_t *, int dx, int dy);
void tyvor_ui32_frame(tyvor_ui32_t *);
void tyvor_ui32_destroy(tyvor_ui32_t *);
int tyvor_ui32_layout_errors(tyvor_ui32_t *, char *reason, size_t capacity);
void tyvor_ui32_pages_create(tyvor_ui32_t *);
void tyvor_ui32_pages_update(tyvor_ui32_t *);
void tyvor_ui32_bind(lv_obj_t *, tyvor_ui32_t *, unsigned action, int page);
const char *tyvor_ui32_page_name(unsigned);
int tyvor_ui32_x(const tyvor_ui32_t *, int canonical);
int tyvor_ui32_y(const tyvor_ui32_t *, int canonical);
void tyvor_ui32_background(lv_event_t *);
lv_obj_t *tyvor_ui32_dial(lv_obj_t *, int, int, int, int, tyvor_ui32_stick_t *);
#endif
