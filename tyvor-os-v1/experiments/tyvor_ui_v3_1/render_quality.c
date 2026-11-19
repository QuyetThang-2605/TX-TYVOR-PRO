#include "render_quality.h"
#include "display_contract.h"
uint16_t tyvor_ui31_rgb565(unsigned r, unsigned g, unsigned b, int x, int y) {
  static const unsigned bayer[4][4] = {
      {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  unsigned noise = bayer[(unsigned)y & 3][(unsigned)x & 3] * 255;
  unsigned rr = (r * 31 * 16 + noise) / 4080, gg = (g * 63 * 16 + noise) / 4080,
           bb = (b * 31 * 16 + noise) / 4080;
  return (uint16_t)((rr << 11) | (gg << 5) | bb);
}
int tyvor_ui31_pack_rgb888(uint8_t *px, size_t cap, size_t stride, int w, int h,
                           int x0, int y0) {
  if (!px || w <= 0 || h <= 0 || x0 < 0 || y0 < 0 ||
      cap > TYVOR_DISPLAY_BUFFER_BYTES || stride < (size_t)w * 3 ||
      stride > cap / (size_t)h)
    return -1;
  size_t out = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      size_t in = (size_t)y * stride + (size_t)x * 3;
      unsigned b = px[in], g = px[in + 1], r = px[in + 2];
      uint16_t value = tyvor_ui31_rgb565(r, g, b, x + x0, y + y0);
      px[out++] = (uint8_t)(value >> 8);
      px[out++] = (uint8_t)value;
    }
  return 0;
}
