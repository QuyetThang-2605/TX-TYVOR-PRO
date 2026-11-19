#ifndef TYVOR_TFT_TOUCH_ADAPTER_H
#define TYVOR_TFT_TOUCH_ADAPTER_H
#include <stdint.h>
typedef struct {int x,y,pressed;uint64_t sampled_ms;} tyvor_touch_sample_t;
typedef int (*tyvor_touch_spi_read_fn)(void *,tyvor_touch_sample_t *);
typedef struct {tyvor_touch_spi_read_fn read;void *context;uint16_t width,height;uint8_t verified;uint64_t last_ms;} tyvor_touch_adapter_t;
/* Chip-specific SPI transactions/calibration intentionally supplied only after IC verification. */
int tyvor_touch_adapter_init(tyvor_touch_adapter_t *,int ic_verified,tyvor_touch_spi_read_fn,void *,uint16_t width,uint16_t height);
int tyvor_touch_adapter_read(tyvor_touch_adapter_t *,uint64_t now,tyvor_touch_sample_t *);
#endif
