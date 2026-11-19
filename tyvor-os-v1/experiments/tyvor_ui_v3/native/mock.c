#include "mock.h"
#include <stdio.h>
#include <string.h>
static int write_pixels(void *ctx, const tyvor_display_region_t *r,
                        const uint8_t *p, size_t n) {
  tyvor_ui3_mock_t *m = ctx;
  if (r->x1 < 0 || r->y1 < 0 || r->x2 > m->width || r->y2 > m->height ||
      n != (size_t)(r->x2 - r->x1) * (size_t)(r->y2 - r->y1) * 2 ||
      n > TYVOR_DISPLAY_BUFFER_BYTES)
    return -1;
  size_t i = 0;
  for (int y = r->y1; y < r->y2; ++y)
    for (int x = r->x1; x < r->x2; ++x) {
      m->pixels[y * m->width + x] = (uint16_t)((uint16_t)p[i] << 8 | p[i + 1]);
      i += 2;
    }
  ++m->writes;
  m->pixels_sent += n / 2;
  m->time += n / 8 + 17;
  return 0;
}
static uint64_t micros(void *ctx) { return ((tyvor_ui3_mock_t *)ctx)->time; }
tyvor_ui3_port_t tyvor_ui3_mock_port(tyvor_ui3_mock_t *m, int w, int h) {
  memset(m, 0, sizeof(*m));
  m->width = w;
  m->height = h;
  return (tyvor_ui3_port_t){m, w, h, write_pixels, micros};
}
int tyvor_ui3_mock_ppm(tyvor_ui3_mock_t *m, const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f)
    return -1;
  fprintf(f, "P6\n%d %d\n255\n", m->width, m->height);
  for (int i = 0; i < m->width * m->height; ++i) {
    unsigned p = m->pixels[i];
    uint8_t c[3] = {(uint8_t)((p >> 11) * 255 / 31),
                    (uint8_t)(((p >> 5) & 63) * 255 / 63),
                    (uint8_t)((p & 31) * 255 / 31)};
    if (fwrite(c, 1, 3, f) != 3) {
      fclose(f);
      return -1;
    }
  }
  return fclose(f) ? -1 : 0;
}
