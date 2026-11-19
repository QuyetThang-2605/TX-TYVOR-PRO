#ifndef TYVOR_DISPLAY_CONTRACT_H
#define TYVOR_DISPLAY_CONTRACT_H
#include <stddef.h>
#include <stdint.h>
enum {TFT_SPI_320X240=1,TYVOR_DISPLAY_DISABLED=1,TYVOR_DISPLAY_INVALID=-1,TYVOR_DISPLAY_BUSY=-2,TYVOR_DISPLAY_IO=-3};
#define TYVOR_DISPLAY_BUFFER_BYTES (320u*64u*2u)
#define TYVOR_DISPLAY_WAIT_MS 250u
typedef struct {unsigned profile,rotation;uint16_t width,height;uint32_t spi_hz;} tyvor_display_config_t;
typedef struct {int sclk,mosi,miso,cs,dc,reset,backlight;uint8_t board_verified,module_verified,hardware_gate;} tyvor_display_pins_t;
typedef struct {int x1,y1,x2,y2;} tyvor_display_region_t; /* exclusive right/bottom */
typedef struct {
    void *context;
    int (*start)(void *,const tyvor_display_region_t *,const uint8_t *wire_rgb565,size_t);
    int (*wait)(void *,unsigned timeout_ms); /* 0 only when DMA no longer owns buffer */
    uint64_t (*micros)(void *);
    int (*rotate)(void *,unsigned);
} tyvor_display_backend_t;
typedef struct {
    tyvor_display_config_t config;tyvor_display_backend_t backend;
    const uint8_t *owned_buffer;size_t owned_bytes;
    uint64_t started_us,last_transfer_us,max_transfer_us,total_transfer_us;
    uint64_t pixels;uint32_t transfers,errors,timeouts;uint8_t busy,fault;
} tyvor_display_pipeline_t;
int tyvor_display_config(tyvor_display_config_t *,unsigned rotation);
int tyvor_display_config_valid(const tyvor_display_config_t *);
int tyvor_display_pin_policy(const tyvor_display_pins_t *);
int tyvor_display_region_valid(const tyvor_display_config_t *,const tyvor_display_region_t *,size_t);
void tyvor_rgb565_wire(uint8_t *native_pixels,size_t pixels);
int tyvor_display_pipeline_init(tyvor_display_pipeline_t *,const tyvor_display_config_t *,tyvor_display_backend_t);
int tyvor_display_begin(tyvor_display_pipeline_t *,const tyvor_display_region_t *,const uint8_t *,size_t);
int tyvor_display_complete(tyvor_display_pipeline_t *);
int tyvor_display_rotate(tyvor_display_pipeline_t *,unsigned rotation);
#endif
