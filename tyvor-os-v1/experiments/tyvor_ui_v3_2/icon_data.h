#ifndef TYVOR32_ICON_DATA_H
#define TYVOR32_ICON_DATA_H
#include <stdint.h>
#include "lvgl.h"
typedef struct {const int16_t (*segments)[4];unsigned count;const lv_image_dsc_t *images[3];} tyvor_ui32_icon_data_t;
extern const tyvor_ui32_icon_data_t tyvor_ui32_icon_data[15];
extern const lv_image_dsc_t tyvor_ui32_ring_58,tyvor_ui32_ring_80,tyvor_ui32_ring_88,tyvor_ui32_ring_94,tyvor_ui32_ring_130;
#endif
