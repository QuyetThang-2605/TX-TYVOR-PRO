#ifndef TYVOR_TFT_SPI_BACKEND_H
#define TYVOR_TFT_SPI_BACKEND_H
#include "display_contract.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
typedef struct {esp_lcd_panel_handle_t panel;SemaphoreHandle_t done;uint8_t hardware;uint32_t mock_bytes,mock_checksum;} tyvor_tft_spi_t;
int tyvor_tft_spi_init(tyvor_tft_spi_t *,const tyvor_display_config_t *,const tyvor_display_pins_t *,tyvor_display_backend_t *);
#endif
