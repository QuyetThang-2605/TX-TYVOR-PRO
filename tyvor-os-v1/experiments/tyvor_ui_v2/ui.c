#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *names[] = {"HOME",           "MODEL MANAGER", "INPUTS",
                              "MIXES",          "OUTPUTS",       "TELEMETRY",
                              "RADIO SETTINGS", "QUICK SETTINGS"};
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px) {
  tyvor_ui2_t *u = lv_display_get_user_data(d);
  tyvor_display_region_t r = {a->x1, a->y1, a->x2 + 1, a->y2 + 1};
  size_t n = (size_t)(r.x2 - r.x1) * (size_t)(r.y2 - r.y1) * 2;
  if (r.x1 < 0 || r.y1 < 0 || r.x2 > u->port.width || r.y2 > u->port.height ||
      n > TYVOR_DISPLAY_BUFFER_BYTES)
    abort();
  tyvor_rgb565_wire(px, n / 2);
  if (u->port.write(u->port.context, &r, px, n))
    abort();
  u->pixels += n / 2;
  ++u->flushes;
  lv_display_flush_ready(d);
}
static void clicked(lv_event_t *e) {
  tyvor_ui2_t *u = lv_event_get_user_data(e);
  lv_obj_t *target = lv_event_get_current_target(e);
  unsigned code = (unsigned)(uintptr_t)lv_obj_get_user_data(target);
  if (code >= 100)
    tyvor_ui2_open(u, code - 100);
  else
    tyvor_ui2_action(u, code);
}
void tyvor_ui2_bind(lv_obj_t *o, tyvor_ui2_t *u, unsigned action, int page) {
  lv_obj_set_user_data(
      o, (void *)(uintptr_t)(page >= 0 ? 100 + (unsigned)page : action));
  lv_obj_add_event_cb(o, clicked, LV_EVENT_CLICKED, u);
}
static void gesture(lv_event_t *e) {
  tyvor_ui2_t *u = lv_event_get_user_data(e);
  lv_indev_t *i = lv_indev_active();
  if (!i)
    return;
  lv_dir_t d = lv_indev_get_gesture_dir(i);
  if (d == LV_DIR_LEFT)
    tyvor_ui2_action(u, UI2_SWIPE_LEFT);
  if (d == LV_DIR_TOP)
    tyvor_ui2_action(u, UI2_SWIPE_UP);
  lv_indev_wait_release(i);
}
int tyvor_ui2_init(tyvor_ui2_t *u, tyvor_ui2_port_t port, void *buf,
                   size_t bytes) {
  if (!u || !buf || bytes < TYVOR_DISPLAY_BUFFER_BYTES || !port.write ||
      !port.micros)
    return -1;
  memset(u, 0, sizeof(*u));
  if (tyvor_ui2_layout(port.width, port.height, &u->layout))
    return -1;
  u->port = port;
  u->display = lv_display_create(port.width, port.height);
  if (!u->display)
    return -1;
  lv_display_set_color_format(u->display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_user_data(u->display, u);
  lv_display_set_flush_cb(u->display, flush);
  lv_display_set_buffers(u->display, buf, NULL, TYVOR_DISPLAY_BUFFER_BYTES,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_default(u->display);
  u->root = lv_display_get_screen_active(u->display);
  tyvor_ui2_surface(u->root, UI2_BG, 0);
  int w = port.width, h = port.height, p = u->layout.pad;
  tyvor_ui2_label(u->root, p, 2, 52, "TYVOR", &lv_font_montserrat_12,
                  UI2_ACCENT);
  u->top_model = tyvor_ui2_label(u->root, p, 18, w - 110 - p - 2, "--",
                                 &lv_font_montserrat_14, UI2_TEXT);
  u->top_state = tyvor_ui2_label(u->root, w - 100, 5, 90, "LINK --",
                                 &lv_font_montserrat_12, UI2_MUTED);
  lv_obj_set_style_text_align(u->top_state, LV_TEXT_ALIGN_RIGHT, 0);
  u->top_detail = tyvor_ui2_label(u->root, w - 110, 20, 100, "--V  RF --",
                                  &lv_font_montserrat_12, UI2_MUTED);
  lv_obj_set_style_text_align(u->top_detail, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_t *strip = tyvor_ui2_box(u->root, p, 38, w - 2 * p, 26, UI2_CARD);
  u->safety = tyvor_ui2_label(strip, 8, 6, w - 2 * p - 80, "UNKNOWN",
                              &lv_font_montserrat_12, UI2_MUTED);
  u->throttle = tyvor_ui2_label(strip, w - 2 * p - 77, 6, 70, "RO",
                                &lv_font_montserrat_12, UI2_MUTED);
  lv_obj_set_style_text_align(u->throttle, LV_TEXT_ALIGN_RIGHT, 0);
  u->body = tyvor_ui2_box(u->root, p, u->layout.body_y, w - 2 * p,
                          u->layout.body_h, UI2_BG);
  lv_obj_set_style_radius(u->body, 0, 0);
  const char *nav_names[] = {"BACK", "HOME", "MODEL", "QUICK"};
  unsigned nav_icons[] = {UI2_ICON_BACK, UI2_ICON_HOME, UI2_ICON_MODEL,
                          UI2_ICON_SETTINGS};
  for (unsigned n = 0; n < 4; ++n) {
    int nw = w / 4;
    u->nav[n] = tyvor_ui2_button(u->root, (int)n * nw, h - 44, nw, 44,
                                 nav_names[n], nav_icons[n]);
    tyvor_ui2_bind(u->nav[n], u, n == 0 ? UI2_BACK : UI2_GO_HOME,
                   n < 2    ? -1
                   : n == 2 ? UI2_MODEL
                            : UI2_QUICK);
  }
  lv_obj_add_event_cb(u->root, gesture, LV_EVENT_GESTURE, u);
  tyvor_ui2_pages_create(u);
  return 0;
}
void tyvor_ui2_update(tyvor_ui2_t *u, const tyvor_ui2_view_t *v) {
  if (!u || !v)
    return;
  if (u->has_view && !memcmp(&u->view, v, sizeof(*v)))
    return;
  u->view = *v;
  u->has_view = 1;
  int model_width = u->layout.width - 110 - u->layout.pad - 2;
  if (tyvor_ui2_text_fit(u->top_model, v->base.model, model_width, 14)) {
    char id[24];
    (void)snprintf(id, sizeof(id), "MODEL %u", (unsigned)v->model_id);
    (void)tyvor_ui2_text_fit(u->top_model, id, model_width, 14);
  }
  char connection[24];
  (void)snprintf(connection, sizeof(connection), "%s%s",
                 v->base.mock ? "DEMO " : "LINK ",
                 v->base.quality == TYVOR_TFT_VALID   ? "OK"
                 : v->base.quality == TYVOR_TFT_STALE ? "STALE"
                                                      : "OFF");
  tyvor_ui2_text(u->top_state, connection);
  char detail[28];
  (void)snprintf(detail, sizeof(detail), "%s  RF %s",
                 v->base.battery_valid ? v->voltage : "-- V",
                 v->rf_valid ? (v->rf_enabled ? "ON" : "OFF") : "--");
  tyvor_ui2_text(u->top_detail, detail);
  const char *s = v->base.quality == TYVOR_TFT_VALID   ? v->safety
                  : v->base.quality == TYVOR_TFT_STALE ? "STALE / UNKNOWN"
                                                       : "DISCONNECTED";
  tyvor_ui2_text(u->safety, s);
  tyvor_ui2_text(u->throttle, v->base.quality == TYVOR_TFT_VALID
                                  ? (v->throttle_lock ? "THR LOCK" : "THR FREE")
                                  : "ARM ?");
  uint32_t c = v->base.quality == TYVOR_TFT_DISCONNECTED ? UI2_DANGER
               : v->base.quality == TYVOR_TFT_STALE      ? UI2_WARNING
               : !v->model_valid                         ? UI2_DANGER
               : v->base.safe                            ? UI2_SUCCESS
                                                         : UI2_WARNING;
  if (!lv_color_eq(lv_obj_get_style_text_color(u->safety, 0), lv_color_hex(c)))
    lv_obj_set_style_text_color(u->safety, lv_color_hex(c), 0);
  if (u->page == UI2_MIXES && u->subpage && u->subpage * 4 >= v->mix_count) {
    u->subpage = 0;
    tyvor_ui2_pages_create(u);
  }
  tyvor_ui2_pages_update(u);
}
void tyvor_ui2_open(tyvor_ui2_t *u, unsigned page) {
  if (!u || page >= UI2_PAGE_COUNT || page == u->page)
    return;
  if (u->history_count == 16) {
    memmove(u->history, u->history + 1, 15 * sizeof(u->history[0]));
    u->history_count = 15;
  }
  u->history[u->history_count++] = u->page;
  u->page = page;
  u->subpage = 0;
  tyvor_ui2_pages_create(u);
  tyvor_ui2_pages_update(u);
}
void tyvor_ui2_action(tyvor_ui2_t *u, unsigned a) {
  if (!u)
    return;
  if (a == UI2_GO_HOME) {
    tyvor_ui2_open(u, UI2_HOME);
    u->history_count = 0;
  } else if (a == UI2_BACK) {
    if (u->subpage) {
      u->subpage = 0;
      tyvor_ui2_pages_create(u);
      tyvor_ui2_pages_update(u);
    } else if (u->history_count) {
      u->page = u->history[--u->history_count];
      tyvor_ui2_pages_create(u);
      tyvor_ui2_pages_update(u);
    }
  } else if (a == UI2_NEXT || a == UI2_PREVIOUS)
    tyvor_ui2_open(u, (u->page + (a == UI2_NEXT ? 1 : 6)) % 7);
  else if (a == UI2_SWIPE_LEFT && u->page == UI2_HOME)
    tyvor_ui2_open(u, UI2_MODEL);
  else if (a == UI2_SWIPE_UP && u->page == UI2_HOME)
    tyvor_ui2_open(u, UI2_QUICK);
  else if (a == UI2_MORE && (u->page == UI2_OUTPUTS || u->page == UI2_MIXES ||
                             u->page == UI2_TELEMETRY ||
                             u->page == UI2_SETTINGS || u->page == UI2_MODEL)) {
    unsigned count = u->page == UI2_MIXES ? (u->view.mix_count + 3) / 4 : 2;
    if (count < 1)
      count = 1;
    u->subpage = (u->subpage + 1) % count;
    tyvor_ui2_pages_create(u);
    tyvor_ui2_pages_update(u);
  }
}
void tyvor_ui2_gesture(tyvor_ui2_t *u, int dx, int dy) {
  if (dx < -35 && -dx > abs(dy))
    tyvor_ui2_action(u, UI2_SWIPE_LEFT);
  else if (dy < -35 && -dy > abs(dx))
    tyvor_ui2_action(u, UI2_SWIPE_UP);
}
void tyvor_ui2_frame(tyvor_ui2_t *u) {
  uint64_t start = u->port.micros(u->port.context);
  lv_refr_now(u->display);
  uint64_t end = u->port.micros(u->port.context);
  u->frame_us = end >= start ? end - start : 0;
  if (u->frame_us > u->frame_max_us)
    u->frame_max_us = u->frame_us;
  ++u->frames;
}
void tyvor_ui2_destroy(tyvor_ui2_t *u) {
  if (u && u->display) {
    lv_display_delete(u->display);
    u->display = NULL;
  }
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
int tyvor_ui2_layout_errors(tyvor_ui2_t *u, char *reason, size_t cap) {
  if (!u || !reason || !cap)
    return -1;
  reason[0] = 0;
  lv_obj_update_layout(u->root);
  lv_area_t bounds = {0, 0, u->port.width - 1, u->port.height - 1};
  return inspect(u->root, bounds, reason, cap);
}
/* Shared by page header generation, no protocol/command writer dependency. */
const char *tyvor_ui2_page_name(unsigned page) {
  return page < UI2_PAGE_COUNT ? names[page] : "UNKNOWN";
}
