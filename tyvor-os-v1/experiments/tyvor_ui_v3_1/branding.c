#include "branding.h"
#include "ui_theme.h"
lv_obj_t *tyvor_ui31_brand(lv_obj_t *p, int x, int y, int horizontal,
                           int wide) {
  const lv_image_dsc_t *a =
      horizontal
          ? (wide ? &tyvor_ui31_wordmark_large : &tyvor_ui31_wordmark_compact)
          : (wide ? &tyvor_ui31_owl_large : &tyvor_ui31_owl_compact);
  lv_obj_t *o = lv_obj_create(p);
  tyvor_ui31_surface(o, 0xffffff, 3);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_clip_corner(o, true, 0);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, a->header.w + 4, a->header.h + 4);
  lv_obj_t *im = lv_image_create(o);
  lv_image_set_src(im, a);
  lv_obj_set_pos(im, 2, 2);
  lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
  return o;
}
