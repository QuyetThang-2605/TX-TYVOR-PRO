#ifndef TYVOR_UI_V2_H
#define TYVOR_UI_V2_H
#include "display_contract.h"
#include "ui_view.h"
#include "ui_widgets.h"
enum {
  UI2_HOME,
  UI2_MODEL,
  UI2_INPUTS,
  UI2_MIXES,
  UI2_OUTPUTS,
  UI2_TELEMETRY,
  UI2_SETTINGS,
  UI2_QUICK,
  UI2_PAGE_COUNT
};
enum {
  UI2_GO_HOME,
  UI2_BACK,
  UI2_NEXT,
  UI2_PREVIOUS,
  UI2_SWIPE_LEFT,
  UI2_SWIPE_UP,
  UI2_MORE
};
typedef struct {
  void *context;
  int width, height;
  int (*write)(void *, const tyvor_display_region_t *,
               const uint8_t *wire_rgb565, size_t);
  uint64_t (*micros)(void *);
} tyvor_ui2_port_t;
typedef struct {
  tyvor_ui2_port_t port;
  tyvor_ui2_layout_t layout;
  tyvor_ui2_view_t view;
  lv_display_t *display;
  lv_obj_t *root, *body, *top_model, *top_state, *top_detail, *safety,
      *throttle, *page_title;
  lv_obj_t *nav[4], *field[48], *bars[8], *sticks[2], *stick_dots[2],
      *actions[8];
  unsigned page, subpage, history[16], history_count;
  uint32_t allocations_on_navigation, frames;
  uint8_t has_view;
  uint64_t frame_us, frame_max_us, pixels;
  uint32_t flushes;
} tyvor_ui2_t;
int tyvor_ui2_init(tyvor_ui2_t *, tyvor_ui2_port_t, void *buffer, size_t bytes);
void tyvor_ui2_update(tyvor_ui2_t *, const tyvor_ui2_view_t *);
void tyvor_ui2_open(tyvor_ui2_t *, unsigned page);
void tyvor_ui2_action(tyvor_ui2_t *, unsigned action);
void tyvor_ui2_gesture(tyvor_ui2_t *, int dx, int dy);
void tyvor_ui2_frame(tyvor_ui2_t *);
void tyvor_ui2_destroy(tyvor_ui2_t *);
/* Layout inspection reports real object geometry/text cropping, not image
 * creation alone. */
int tyvor_ui2_layout_errors(tyvor_ui2_t *, char *reason, size_t capacity);
void tyvor_ui2_pages_create(tyvor_ui2_t *);
void tyvor_ui2_pages_update(tyvor_ui2_t *);
const char *tyvor_ui2_page_name(unsigned page);
void tyvor_ui2_bind(lv_obj_t *, tyvor_ui2_t *, unsigned action, int page);
#endif
