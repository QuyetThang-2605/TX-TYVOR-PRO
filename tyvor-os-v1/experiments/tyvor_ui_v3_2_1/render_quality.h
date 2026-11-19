#ifndef TYVOR_V31_RENDER_QUALITY_H
#define TYVOR_V31_RENDER_QUALITY_H
#include <stddef.h>
#include <stdint.h>
uint16_t tyvor_ui321_rgb565(unsigned r, unsigned g, unsigned b, int x, int y);
int tyvor_ui321_pack_rgb888(uint8_t *buffer, size_t capacity, size_t stride,
                            int width, int height, int x, int y);
#endif
