#include "ui.h"
static void circle(lv_layer_t *layer, int x, int y, int diameter,
                   uint32_t color, int border) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE;
  d.bg_color = lv_color_hex(color);
  d.bg_opa = border ? LV_OPA_TRANSP : LV_OPA_COVER;
  d.border_width = border;
  d.border_color = lv_color_hex(color);
  d.border_opa = LV_OPA_COVER;
  lv_area_t a = {x, y, x + diameter - 1, y + diameter - 1};
  lv_draw_rect(layer, &d, &a);
}
static void line(lv_layer_t *layer, int x1, int y1, int x2, int y2, int width,
                 uint32_t color) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = x1;
  d.p1.y = y1;
  d.p2.x = x2;
  d.p2.y = y2;
  d.width = width;
  d.color = lv_color_hex(color);
  d.opa = LV_OPA_COVER;
  d.round_start = 1;
  d.round_end = 1;
  lv_draw_line(layer, &d);
}
void tyvor_ui3_background(lv_event_t *e) {
  tyvor_ui3_t *u = lv_event_get_user_data(e);
  lv_layer_t *layer = lv_event_get_layer(e);
  /* Original abstract wing/wave geometry; never a full-screen image asset. */
  const int points[][6] = {{0, 202, 164, 122, 118, 244},
                           {96, 217, 290, 111, 233, 224},
                           {122, 244, 400, 170, 277, 277},
                           {0, 265, 145, 209, 84, 315}};
  for (unsigned i = 0; i < 4; ++i) {
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.bg_color = lv_color_hex(i % 2 ? 0x105264 : 0x35768A);
    d.bg_opa = i % 2 ? 115 : 88;
    for (unsigned j = 0; j < 3; ++j) {
      d.p[j].x = tyvor_ui3_x(u, points[i][j * 2]);
      d.p[j].y = tyvor_ui3_y(u, points[i][j * 2 + 1]);
    }
    lv_draw_triangle(layer, &d);
  }
}
static void dial_draw(lv_event_t *e) {
  lv_obj_t *o = lv_event_get_current_target(e);
  tyvor_ui3_stick_t *s = lv_event_get_user_data(e);
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  int w = lv_obj_get_width(o), h = lv_obj_get_height(o),
      d = (w < h ? w : h) - 8, x = a.x1 + (w - d) / 2, y = a.y1 + (h - d) / 2,
      center_x = x + d / 2, center_y = y + d / 2;
  circle(layer, x, y, d, 0x69748A, 0);
  circle(layer, x + 5, y + 5, d - 10, 0x485D70, 0);
  circle(layer, x + 10, y + 10, d - 20, 0x9DAAB8, 0);
  circle(layer, x + 14, y + 14, d - 28, 0x435B70, 0);
  for (int i = -1; i <= 1; i += 2) {
    line(layer, center_x + i * (d / 2 - 9), center_y,
         center_x + i * (d / 2 - 4), center_y, 2, UI3_TEXT);
    line(layer, center_x, center_y + i * (d / 2 - 9), center_x,
         center_y + i * (d / 2 - 4), 2, UI3_TEXT);
  }
  int vx = s->x, vy = s->y;
  if (vx < -1000)
    vx = -1000;
  if (vx > 1000)
    vx = 1000;
  if (vy < -1000)
    vy = -1000;
  if (vy > 1000)
    vy = 1000;
  int r = d / 6, dx = s->valid ? vx * (d / 7) / 1000 : 0,
      dy = s->valid ? -vy * (d / 7) / 1000 : 0;
  circle(layer, center_x - r - 2 + dx, center_y - r - 2 + dy, 2 * r + 4,
         UI3_TEXT, 0);
  circle(layer, center_x - r + dx, center_y - r + dy, 2 * r,
         s->valid ? 0x66BCE3 : UI3_MUTED, 0);
}
lv_obj_t *tyvor_ui3_dial(lv_obj_t *p, int x, int y, int w, int h,
                         tyvor_ui3_stick_t *data) {
  lv_obj_t *o = tyvor_ui3_box(p, x, y, w, h, UI3_CARD);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(o, dial_draw, LV_EVENT_DRAW_MAIN, data);
  return o;
}
