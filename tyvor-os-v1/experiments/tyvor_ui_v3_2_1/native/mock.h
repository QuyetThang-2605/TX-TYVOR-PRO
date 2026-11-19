#ifndef TYVOR_UI321_MOCK_H
#define TYVOR_UI321_MOCK_H
#include "ui.h"
typedef struct {
  uint16_t pixels[368 * 448];
  uint8_t rgb888[368 * 448 * 3];
  int width, height;
  uint64_t time, pixels_sent;
  uint32_t writes;
} tyvor_ui321_mock_t;
tyvor_ui321_port_t tyvor_ui321_mock_port(tyvor_ui321_mock_t *, int width,
                                         int height);
int tyvor_ui321_mock_ppm(tyvor_ui321_mock_t *, const char *path);
void tyvor_ui321_mock_capture(void *, const tyvor_display_region_t *,
                              const uint8_t *, size_t);
int tyvor_ui321_mock_rgb888_ppm(tyvor_ui321_mock_t *, const char *path);
#endif
