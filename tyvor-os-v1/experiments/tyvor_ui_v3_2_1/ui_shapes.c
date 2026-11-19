#include "brand_assets.h"
#include "icon_data.h"
#include "ui.h"
void tyvor_ui321_background(lv_event_t *e) {
  tyvor_ui321_t *u = lv_event_get_user_data(e);
  lv_draw_image_dsc_t d;
  lv_draw_image_dsc_init(&d);
  d.src = u->layout.wide ? &tyvor_ui321_background_448
                         : &tyvor_ui321_background_320;
  lv_area_t a = {0, 0, u->port.width - 1, u->port.height - 1};
  /* Const Flash RGB565; LVGL clips to dirty region. No full-screen RAM copy. */
  lv_draw_image(lv_event_get_layer(e), &d, &a);
}
static void disc(lv_layer_t *layer, int x, int y, int size, uint32_t color,
                 int border) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE;
  d.bg_color = lv_color_hex(color);
  d.bg_opa = 255;
  d.border_width = border;
  d.border_color = lv_color_hex(0xB9E7FF);
  d.border_opa = 180;
  lv_area_t a = {x, y, x + size - 1, y + size - 1};
  lv_draw_rect(layer, &d, &a);
}
const lv_image_dsc_t *tyvor_ui321_ring_for(int diameter) {
  switch (diameter) {
  case 58:
    return &tyvor_ui321_ring_58;
  case 80:
    return &tyvor_ui321_ring_80;
  case 88:
    return &tyvor_ui321_ring_88;
  case 94:
    return &tyvor_ui321_ring_94;
  case 130:
    return &tyvor_ui321_ring_130;
  default:
    return NULL;
  }
}
static void dial_draw(lv_event_t *e) {
  lv_obj_t *o = lv_event_get_current_target(e);
  tyvor_ui321_stick_t *s = lv_event_get_user_data(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  int w = lv_obj_get_width(o), h = lv_obj_get_height(o);
  int diameter = ((w < h ? w : h) - 8) / 2 * 2, cx = a.x1 + w / 2,
      cy = a.y1 + h / 2, radius = diameter / 2;
  lv_layer_t *layer = lv_event_get_layer(e);
  disc(layer, cx - radius, cy - radius, diameter, 0x123D70, 0);
  /* Cached flat A8 ticks avoid line-rasterizer clipping differences between
     a full RGB888 strip and a small dirty region. No frame allocation. */
  lv_draw_image_dsc_t ticks;
  lv_draw_image_dsc_init(&ticks);
  ticks.src = tyvor_ui321_ring_for(diameter);
  ticks.recolor = lv_color_hex(0xDBEEFF);
  ticks.recolor_opa = 255;
  lv_area_t tick_area = {cx - radius - 1, cy - radius - 1, cx + radius,
                         cy + radius};
  lv_draw_image(layer, &ticks, &tick_area);
  int x = s->x, y = s->y;
  if (x < -1000)
    x = -1000;
  if (x > 1000)
    x = 1000;
  if (y < -1000)
    y = -1000;
  if (y > 1000)
    y = 1000;
  int knob = diameter / 6, dx = s->valid ? x * radius / 3 / 1000 : 0,
      dy = s->valid ? -y * radius / 3 / 1000 : 0;
  disc(layer, cx - knob + dx, cy - knob + dy, knob * 2,
       s->valid ? 0x62C7F3 : 0x536C89, 1);
}
lv_obj_t *tyvor_ui321_dial(lv_obj_t *p, int x, int y, int w, int h,
                           tyvor_ui321_stick_t *data) {
  if (!tyvor_ui321_ring_for(((w < h ? w : h) - 8) / 2 * 2))
    return NULL;
  lv_obj_t *o = tyvor_ui321_box(p, x, y, w, h, UI321_CARD);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_add_event_cb(o, dial_draw, LV_EVENT_DRAW_MAIN, data);
  return o;
}
