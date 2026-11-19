#ifndef TYVOR_UI32_MOCK_H
#define TYVOR_UI32_MOCK_H
#include "ui.h"
typedef struct {
  uint16_t pixels[368 * 448];
  int width, height;
  uint64_t time, pixels_sent;
  uint32_t writes;
} tyvor_ui32_mock_t;
tyvor_ui32_port_t tyvor_ui32_mock_port(tyvor_ui32_mock_t *, int width,
                                       int height);
int tyvor_ui32_mock_ppm(tyvor_ui32_mock_t *, const char *path);
#endif
