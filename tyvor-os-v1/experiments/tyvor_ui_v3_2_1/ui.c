#include "ui.h"
#include "branding.h"
#include "render_quality.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *names[] = {"HOME",     "MODEL",          "INPUTS",
                              "MIXES",    "OUTPUTS",        "TELEMETRY",
                              "SETTINGS", "QUICK SETTINGS", "CONNECTIONS"};
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px) {
  tyvor_ui321_t *u = lv_display_get_user_data(d);
  tyvor_display_region_t r = {a->x1, a->y1, a->x2 + 1, a->y2 + 1};
  size_t count = (size_t)(r.x2 - r.x1) * (size_t)(r.y2 - r.y1), n = count * 2;
  if (r.x1 < 0 || r.y1 < 0 || r.x2 > u->port.width || r.y2 > u->port.height ||
      count * 3 > TYVOR_DISPLAY_BUFFER_BYTES)
    abort();
  /* RGB888 BGR-order draw buffer -> MSB-first RGB565, safely contracting in
     place. Global-coordinate dither is stable across partial/whole refreshes.
   */
  size_t stride = lv_display_get_buf_active(d)->header.stride;
  if (u->observe_rgb888)
    u->observe_rgb888(u->observe_context, &r, px, stride);
  if (tyvor_ui321_pack_rgb888(px, TYVOR_DISPLAY_BUFFER_BYTES, stride,
                              r.x2 - r.x1, r.y2 - r.y1, r.x1, r.y1))
    abort();
  if (u->port.write(u->port.context, &r, px, n))
    abort();
  u->pixels += n / 2;
  ++u->flushes;
  lv_display_flush_ready(d);
}
static void clicked(lv_event_t *e) {
  tyvor_ui321_t *u = lv_event_get_user_data(e);
  unsigned code =
      (unsigned)(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
  if (code >= 100)
    tyvor_ui321_open(u, code - 100);
  else
    tyvor_ui321_action(u, code);
}
void tyvor_ui321_bind(lv_obj_t *o, tyvor_ui321_t *u, unsigned action,
                      int page) {
  lv_obj_set_user_data(
      o, (void *)(uintptr_t)(page >= 0 ? 100 + (unsigned)page : action));
  lv_obj_add_event_cb(o, clicked, LV_EVENT_CLICKED, u);
}
static void gesture(lv_event_t *e) {
  tyvor_ui321_t *u = lv_event_get_user_data(e);
  lv_indev_t *i = lv_indev_active();
  if (!i)
    return;
  lv_dir_t d = lv_indev_get_gesture_dir(i);
  if (d == LV_DIR_LEFT)
    tyvor_ui321_action(u, UI321_SWIPE_LEFT);
  if (d == LV_DIR_TOP)
    tyvor_ui321_action(u, UI321_SWIPE_UP);
  if (d == LV_DIR_BOTTOM)
    tyvor_ui321_action(u, UI321_SWIPE_DOWN);
  lv_indev_wait_release(i);
}
int tyvor_ui321_init(tyvor_ui321_t *u, tyvor_ui321_port_t p, void *buf,
                     size_t n) {
  if (!u || !buf || n < TYVOR_DISPLAY_BUFFER_BYTES || !p.write || !p.micros)
    return -1;
  memset(u, 0, sizeof(*u));
  if (tyvor_ui321_layout(p.width, p.height, &u->layout))
    return -1;
  u->port = p;
  u->display = lv_display_create(p.width, p.height);
  if (!u->display)
    return -1;
  lv_display_set_color_format(u->display, LV_COLOR_FORMAT_RGB888);
  lv_display_set_user_data(u->display, u);
  lv_display_set_flush_cb(u->display, flush);
  lv_display_set_buffers(u->display, buf, NULL, TYVOR_DISPLAY_BUFFER_BYTES,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_default(u->display);
  u->root = lv_display_get_screen_active(u->display);
  tyvor_ui321_surface(u->root, UI321_BG, 0);
  lv_obj_add_event_cb(u->root, tyvor_ui321_background, LV_EVENT_DRAW_MAIN, u);
  int w = p.width, top = u->layout.top;
  int wide = u->layout.wide;
  u->brand_mark = tyvor_ui321_brand(u->root, 8, wide ? 4 : 3, 2, wide);
  int mx = wide ? 138 : 96, mw = wide ? 104 : 70;
  lv_obj_t *divider =
      tyvor_ui321_box(u->root, mx - 8, 7, 1, wide ? 22 : 17, UI321_MUTED);
  lv_obj_set_style_bg_opa(divider, 80, 0);
  u->top_model = tyvor_ui321_label(
      u->root, mx, wide ? 9 : 7, mw, "--",
      wide ? &lv_font_montserrat_14 : &lv_font_montserrat_12, UI321_TEXT);
  u->top_state = tyvor_ui321_label(u->root, wide ? 258 : 172, wide ? 9 : 7, 36,
                                   "SIM", &lv_font_montserrat_12, UI321_TEXT);
  u->top_detail =
      tyvor_ui321_label(u->root, wide ? 307 : 216, wide ? 9 : 7, 46, "RF --",
                        &lv_font_montserrat_12, UI321_TEXT);
  u->link_icon = tyvor_ui321_icon(u->root, wide ? 378 : 270, wide ? 6 : 4,
                                  UI321_ICON_OUTPUT, UI321_TEXT);
  lv_obj_t *help =
      tyvor_ui321_button(u->root, w - (wide ? 35 : 27), 3, wide ? 30 : 24,
                         wide ? 30 : 24, NULL, UI321_ICON_HELP);
  tyvor_ui321_bind(help, u, 0, UI321_SETTINGS);
  u->body = tyvor_ui321_box(u->root, 0, top, w, p.height - top, UI321_BG);
  lv_obj_set_style_bg_opa(u->body, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(u->body, 0, 0);
  lv_obj_add_event_cb(u->root, gesture, LV_EVENT_GESTURE, u);
  tyvor_ui321_pages_create(u);
  return 0;
}
void tyvor_ui321_update(tyvor_ui321_t *u, const tyvor_ui321_view_t *v) {
  if (!u || !v)
    return;
  if (u->has_view && !memcmp(&u->view, v, sizeof(*v)))
    return;
  u->view = *v;
  u->has_view = 1;
  if (tyvor_ui321_text_fit(u->top_model, v->base.model,
                           u->layout.wide ? 104 : 70,
                           u->layout.wide ? 14 : 12)) {
    char id[24];
    snprintf(id, sizeof(id), "MODEL %u", (unsigned)v->model_id);
    if (tyvor_ui321_text_fit(u->top_model, id, u->layout.wide ? 104 : 70, 12))
      tyvor_ui321_text(u->top_model, "MODEL");
  }
  tyvor_ui321_text(u->top_state, v->base.mock ? "SIM" : "CORE");
  char status[32];
  snprintf(status, sizeof(status), "RF %s",
           v->rf_valid ? (v->rf_enabled ? "ON" : "OFF") : "--");
  tyvor_ui321_text(u->top_detail, status);
  lv_opa_t link_opa = v->base.lq_valid ? LV_OPA_COVER : LV_OPA_30;
  if (lv_obj_get_style_opa(u->link_icon, 0) != link_opa)
    lv_obj_set_style_opa(u->link_icon, link_opa, 0);
  if (u->page == UI321_MIXES && u->subpage && u->subpage * 4 >= v->mix_count) {
    u->subpage = 0;
    tyvor_ui321_pages_create(u);
  }
  tyvor_ui321_pages_update(u);
}
void tyvor_ui321_open(tyvor_ui321_t *u, unsigned page) {
  if (!u || page >= UI321_PAGE_COUNT || page == u->page)
    return;
  if (u->history_count == 16) {
    memmove(u->history, u->history + 1, 15 * sizeof(unsigned));
    u->history_count = 15;
  }
  u->history[u->history_count++] = u->page;
  u->page = page;
  u->subpage = 0;
  tyvor_ui321_pages_create(u);
  tyvor_ui321_pages_update(u);
}
void tyvor_ui321_action(tyvor_ui321_t *u, unsigned a) {
  if (!u)
    return;
  if (a == UI321_GO_HOME) {
    tyvor_ui321_open(u, UI321_HOME);
    u->history_count = 0;
  } else if (a == UI321_BACK) {
    if (u->subpage)
      u->subpage = 0;
    else if (u->history_count)
      u->page = u->history[--u->history_count];
    else
      u->page = UI321_HOME;
    tyvor_ui321_pages_create(u);
    tyvor_ui321_pages_update(u);
  } else if (a == UI321_NEXT || a == UI321_PREVIOUS)
    tyvor_ui321_open(u, (u->page + (a == UI321_NEXT ? 1 : 7)) % 8);
  else if (a == UI321_SWIPE_LEFT && u->page == UI321_HOME)
    tyvor_ui321_open(u, UI321_MODEL);
  else if (a == UI321_SWIPE_UP && u->page == UI321_HOME)
    tyvor_ui321_open(u, UI321_QUICK);
  else if (a == UI321_SWIPE_DOWN && u->page == UI321_HOME)
    tyvor_ui321_open(u, UI321_CONNECTIONS);
  else if (a == UI321_MORE && u->page != UI321_HOME) {
    unsigned count = u->page == UI321_MIXES ? (u->view.mix_count + 3) / 4 : 2;
    if (!count)
      count = 1;
    u->subpage = (u->subpage + 1) % count;
    tyvor_ui321_pages_create(u);
    tyvor_ui321_pages_update(u);
  }
}
void tyvor_ui321_gesture(tyvor_ui321_t *u, int dx, int dy) {
  if (dx < -35 && -dx > abs(dy))
    tyvor_ui321_action(u, UI321_SWIPE_LEFT);
  else if (dy < -35 && -dy > abs(dx))
    tyvor_ui321_action(u, UI321_SWIPE_UP);
  else if (dy > 35 && dy > abs(dx))
    tyvor_ui321_action(u, UI321_SWIPE_DOWN);
}
void tyvor_ui321_frame(tyvor_ui321_t *u) {
  uint64_t start = u->port.micros(u->port.context);
  lv_refr_now(u->display);
  uint64_t end = u->port.micros(u->port.context);
  u->frame_us = end >= start ? end - start : 0;
  if (u->frame_us > u->frame_max_us)
    u->frame_max_us = u->frame_us;
  ++u->frames;
}
void tyvor_ui321_destroy(tyvor_ui321_t *u) {
  if (u && u->display) {
    lv_display_delete(u->display);
    u->display = NULL;
  }
}
const char *tyvor_ui321_page_name(unsigned p) {
  return p < UI321_PAGE_COUNT ? names[p] : "UNKNOWN";
}
int tyvor_ui321_x(const tyvor_ui321_t *u, int x) {
  return x * u->port.width / 400;
}
int tyvor_ui321_y(const tyvor_ui321_t *u, int y) {
  return y * u->port.height / 315;
}
static int inspect(lv_obj_t *o, lv_area_t bounds, char *reason, size_t cap) {
  if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN))
    return 0;
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  if (a.x1 < bounds.x1 || a.y1 < bounds.y1 || a.x2 > bounds.x2 ||
      a.y2 > bounds.y2) {
    (void)snprintf(reason, cap, "object outside parent (%ld,%ld)-(%ld,%ld)",
                   (long)a.x1, (long)a.y1, (long)a.x2, (long)a.y2);
    return 1;
  }
  if (lv_obj_check_type(o, &lv_label_class)) {
    lv_point_t size;
    lv_text_get_size(&size, lv_label_get_text(o),
                     lv_obj_get_style_text_font(o, 0), 0, 0, LV_COORD_MAX,
                     LV_TEXT_FLAG_NONE);
    if (size.x > lv_obj_get_width(o) || size.y > lv_obj_get_height(o)) {
      (void)snprintf(reason, cap, "text cropped: %s", lv_label_get_text(o));
      return 1;
    }
  }
  for (unsigned i = 0; i < lv_obj_get_child_count(o); ++i)
    if (inspect(lv_obj_get_child(o, (int32_t)i), a, reason, cap))
      return 1;
  /* Check actual glyph bounds, including alignment, rather than overlapping
     label allocation widths (right-aligned values deliberately share rows). */
  for (unsigned i = 0; i < lv_obj_get_child_count(o); ++i) {
    lv_obj_t *left = lv_obj_get_child(o, (int32_t)i);
    if (!lv_obj_check_type(left, &lv_label_class) ||
        !lv_label_get_text(left)[0])
      continue;
    lv_point_t ls;
    lv_text_get_size(&ls, lv_label_get_text(left),
                     lv_obj_get_style_text_font(left, 0), 0, 0, LV_COORD_MAX,
                     LV_TEXT_FLAG_NONE);
    lv_area_t la;
    lv_obj_get_coords(left, &la);
    int align = lv_obj_get_style_text_align(left, 0);
    if (align == LV_TEXT_ALIGN_RIGHT)
      la.x1 += lv_obj_get_width(left) - ls.x;
    else if (align == LV_TEXT_ALIGN_CENTER)
      la.x1 += (lv_obj_get_width(left) - ls.x) / 2;
    la.x2 = la.x1 + ls.x - 1;
    la.y2 = la.y1 + ls.y - 1;
    for (unsigned j = i + 1; j < lv_obj_get_child_count(o); ++j) {
      lv_obj_t *right = lv_obj_get_child(o, (int32_t)j);
      if (!lv_obj_check_type(right, &lv_label_class) ||
          !lv_label_get_text(right)[0])
        continue;
      lv_point_t rs;
      lv_text_get_size(&rs, lv_label_get_text(right),
                       lv_obj_get_style_text_font(right, 0), 0, 0, LV_COORD_MAX,
                       LV_TEXT_FLAG_NONE);
      lv_area_t ra;
      lv_obj_get_coords(right, &ra);
      align = lv_obj_get_style_text_align(right, 0);
      if (align == LV_TEXT_ALIGN_RIGHT)
        ra.x1 += lv_obj_get_width(right) - rs.x;
      else if (align == LV_TEXT_ALIGN_CENTER)
        ra.x1 += (lv_obj_get_width(right) - rs.x) / 2;
      ra.x2 = ra.x1 + rs.x - 1;
      ra.y2 = ra.y1 + rs.y - 1;
      if (la.x1 <= ra.x2 && la.x2 >= ra.x1 && la.y1 <= ra.y2 &&
          la.y2 >= ra.y1) {
        (void)snprintf(reason, cap, "overlapping text: %s / %s",
                       lv_label_get_text(left), lv_label_get_text(right));
        return 1;
      }
    }
  }
  return 0;
}
int tyvor_ui321_layout_errors(tyvor_ui321_t *u, char *reason, size_t cap) {
  if (!u || !reason || !cap)
    return -1;
  reason[0] = 0;
  lv_obj_update_layout(u->root);
  lv_area_t bounds = {0, 0, u->port.width - 1, u->port.height - 1};
  return inspect(u->root, bounds, reason, cap);
}
