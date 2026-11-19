#ifndef TYVOR_TFT_MOCK_DISPLAY_H
#define TYVOR_TFT_MOCK_DISPLAY_H
#include "display_contract.h"
typedef struct {
    uint16_t framebuffer[320*320];uint16_t width,height;
    uint64_t clock;unsigned starts,waits,rotations;int start_error,wait_error,clock_rollback;
    const uint8_t *active;
} tyvor_tft_mock_t;
void tyvor_tft_mock_init(tyvor_tft_mock_t *,const tyvor_display_config_t *);
tyvor_display_backend_t tyvor_tft_mock_backend(tyvor_tft_mock_t *);
int tyvor_tft_mock_ppm(const tyvor_tft_mock_t *,const char *path);
#endif
