#ifndef TYVOR321_ICON_DATA_H
#define TYVOR321_ICON_DATA_H
#include "lvgl.h"
#include <stdint.h>
typedef struct {
  const int16_t (*segments)[4];
  unsigned count;
  const lv_image_dsc_t *images[3];
} tyvor_ui321_icon_data_t;
extern const tyvor_ui321_icon_data_t tyvor_ui321_icon_data[15];
extern const lv_image_dsc_t tyvor_ui321_ring_58, tyvor_ui321_ring_80,
    tyvor_ui321_ring_88, tyvor_ui321_ring_94, tyvor_ui321_ring_130;
#endif
