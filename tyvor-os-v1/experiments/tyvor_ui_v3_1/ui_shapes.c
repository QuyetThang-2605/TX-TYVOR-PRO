#include "ui.h"
static void disc(lv_layer_t *layer, int x, int y, int diameter, uint32_t top,
                 uint32_t bottom, int border, lv_opa_t opacity) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE;
  d.bg_color = lv_color_hex(top);
  d.bg_opa = opacity;
  d.bg_grad.dir = LV_GRAD_DIR_VER;
  d.bg_grad.stops_count = 2;
  d.bg_grad.stops[0].color = lv_color_hex(top);
  d.bg_grad.stops[0].opa = opacity;
  d.bg_grad.stops[0].frac = 0;
  d.bg_grad.stops[1].color = lv_color_hex(bottom);
  d.bg_grad.stops[1].opa = opacity;
  d.bg_grad.stops[1].frac = 255;
  d.border_width = border;
  d.border_color = lv_color_hex(0xA5BACA);
  d.border_opa = 100;
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
void tyvor_ui31_background(lv_event_t *e) {
  tyvor_ui31_t *u = lv_event_get_user_data(e);
  lv_layer_t *layer = lv_event_get_layer(e);
  int w = u->port.width, h = u->port.height;
  /* Low-opacity vector light pools and wing layers, not a wallpaper bitmap. */
  disc(layer, w / 3, h / 4, w, 0x176C70, 0x104354, 0, 32);
  disc(layer, -w / 2, h / 2, w, 0x35566B, 0x0C3043, 0, 22);
  const int points[][6] = {{24, 263, 225, 144, 150, 269},
                           {107, 275, 375, 168, 286, 287}};
  for (unsigned i = 0; i < 2; ++i) {
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.bg_color = lv_color_hex(i ? 0x0B2A40 : 0x49949B);
    d.bg_opa = i ? 28 : 20;
    for (unsigned j = 0; j < 3; ++j) {
      d.p[j].x = tyvor_ui31_x(u, points[i][2 * j]);
      d.p[j].y = tyvor_ui31_y(u, points[i][2 * j + 1]);
    }
    lv_draw_triangle(layer, &d);
  }
}
static void dial_draw(lv_event_t *e) {
  lv_obj_t *o = lv_event_get_current_target(e);
  tyvor_ui31_stick_t *s = lv_event_get_user_data(e);
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  int w = lv_obj_get_width(o), h = lv_obj_get_height(o),
      d = (w < h ? w : h) - 8, x = a.x1 + (w - d) / 2, y = a.y1 + (h - d) / 2,
      cx = x + d / 2, cy = y + d / 2;
  disc(layer, x, y + 2, d, 0x22394C, 0x1C3045, 0, 160);
  disc(layer, x, y, d, 0x8090A5, 0x4A5C78, 1, 255);
  disc(layer, x + 5, y + 5, d - 10, 0x5D758B, 0x425972, 0, 255);
  for (int side = -1; side <= 1; side += 2) {
    line(layer, cx + side * (d / 2 - 8), cy, cx + side * (d / 2 - 3), cy, 2,
         0xD4E2EA);
    line(layer, cx, cy + side * (d / 2 - 8), cx, cy + side * (d / 2 - 3), 2,
         0xD4E2EA);
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
  int r = d / 7, dx = s->valid ? vx * (d / 7) / 1000 : 0,
      dy = s->valid ? -vy * (d / 7) / 1000 : 0;
  disc(layer, cx - r - 3 + dx, cy - r - 3 + dy, 2 * r + 6, 0xEDF4F7, 0xA8BCCF,
       0, 255);
  disc(layer, cx - r + dx, cy - r + dy, 2 * r, s->valid ? 0x67C8EE : 0x8CA1B2,
       s->valid ? 0x349ACB : 0x657B90, 0, 255);
}
lv_obj_t *tyvor_ui31_dial(lv_obj_t *p, int x, int y, int w, int h,
                          tyvor_ui31_stick_t *data) {
  lv_obj_t *o = tyvor_ui31_box(p, x, y, w, h, UI31_CARD);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(o, dial_draw, LV_EVENT_DRAW_MAIN, data);
  return o;
}
