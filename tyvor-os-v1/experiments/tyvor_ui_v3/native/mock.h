#ifndef TYVOR_UI3_MOCK_H
#define TYVOR_UI3_MOCK_H
#include "ui.h"
typedef struct {
  uint16_t pixels[368 * 448];
  int width, height;
  uint64_t time, pixels_sent;
  uint32_t writes;
} tyvor_ui3_mock_t;
tyvor_ui3_port_t tyvor_ui3_mock_port(tyvor_ui3_mock_t *, int width, int height);
int tyvor_ui3_mock_ppm(tyvor_ui3_mock_t *, const char *path);
#endif
