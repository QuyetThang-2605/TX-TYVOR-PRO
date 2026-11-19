#ifndef TYVOR_TFT_FAKE_SDK_H
#define TYVOR_TFT_FAKE_SDK_H
/* Native SDK fakes ONLY. Not part of the ESP32 component's include paths. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef int esp_err_t;
#define ESP_OK 0
typedef int BaseType_t;
#define pdFALSE 0
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
typedef struct {unsigned count;} fake_semaphore_t;
typedef fake_semaphore_t *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateBinary(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t,unsigned);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t,BaseType_t *);
typedef void *esp_lcd_panel_handle_t;
typedef void *esp_lcd_panel_io_handle_t;
typedef int esp_lcd_spi_bus_handle_t;
typedef struct {int placeholder;} esp_lcd_panel_io_event_data_t;
typedef struct {int mosi_io_num,miso_io_num,sclk_io_num,quadwp_io_num,quadhd_io_num,max_transfer_sz;} spi_bus_config_t;
typedef struct {
    int dc_gpio_num,cs_gpio_num;unsigned pclk_hz;int spi_mode,trans_queue_depth,lcd_cmd_bits,lcd_param_bits;
    bool (*on_color_trans_done)(esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t *,void *);void *user_ctx;
} esp_lcd_panel_io_spi_config_t;
typedef struct {int reset_gpio_num,bits_per_pixel,rgb_ele_order;} esp_lcd_panel_dev_config_t;
typedef struct {uint64_t pin_bit_mask;int mode;} gpio_config_t;
#define SPI2_HOST 2
#define SPI_DMA_CH_AUTO 3
#define GPIO_MODE_OUTPUT 2
#define LCD_RGB_ELEMENT_ORDER_RGB 0
#define LCD_RGB_ELEMENT_ORDER_BGR 1
int gpio_config(const gpio_config_t *);
int gpio_set_level(int,unsigned);
int spi_bus_initialize(int,const spi_bus_config_t *,int);
int esp_lcd_new_panel_io_spi(esp_lcd_spi_bus_handle_t,const esp_lcd_panel_io_spi_config_t *,esp_lcd_panel_io_handle_t *);
int esp_lcd_new_panel_ili9341(esp_lcd_panel_io_handle_t,const esp_lcd_panel_dev_config_t *,esp_lcd_panel_handle_t *);
int esp_lcd_panel_reset(esp_lcd_panel_handle_t);
int esp_lcd_panel_init(esp_lcd_panel_handle_t);
int esp_lcd_panel_disp_on_off(esp_lcd_panel_handle_t,bool);
int esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t,int,int,int,int,const void *);
int esp_lcd_panel_swap_xy(esp_lcd_panel_handle_t,bool);
int esp_lcd_panel_mirror(esp_lcd_panel_handle_t,bool,bool);
int64_t esp_timer_get_time(void);
#define CONFIG_TYVOR_TFT_BGR 0
#define CONFIG_TYVOR_TFT_BACKLIGHT_ACTIVE_LOW 0
#endif
