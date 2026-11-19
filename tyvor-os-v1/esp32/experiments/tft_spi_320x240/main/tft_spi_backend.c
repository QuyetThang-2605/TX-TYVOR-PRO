#include "tft_spi_backend.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_ili9341.h"
#include "esp_timer.h"
#include "tft_build_config.h"
#include <string.h>
static bool completed(esp_lcd_panel_io_handle_t io,esp_lcd_panel_io_event_data_t *event,void *context) {
    (void)io;(void)event;tyvor_tft_spi_t *s=context;BaseType_t wake=pdFALSE;
    xSemaphoreGiveFromISR(s->done,&wake);return wake==pdTRUE;
}
static uint64_t micros(void *context) {(void)context;return (uint64_t)esp_timer_get_time();}
static int start(void *context,const tyvor_display_region_t *r,const uint8_t *pixels,size_t n) {
    tyvor_tft_spi_t *s=context;
    if(!s->hardware) {s->mock_bytes+=(uint32_t)n;for(size_t i=0;i<n;++i)s->mock_checksum=s->mock_checksum*33u+pixels[i];return 0;}
    if(xSemaphoreTake(s->done,0)==pdTRUE)return -1; /* unexpected completion from a prior transfer */
    return esp_lcd_panel_draw_bitmap(s->panel,r->x1,r->y1,r->x2,r->y2,pixels)==ESP_OK?0:-1;
}
static int wait_done(void *context,unsigned timeout) {
    tyvor_tft_spi_t *s=context;if(!s->hardware)return 0;
    return xSemaphoreTake(s->done,pdMS_TO_TICKS(timeout))==pdTRUE?0:-1;
}
static int rotation(void *context,unsigned value) {
    tyvor_tft_spi_t *s=context;if(!s->hardware)return 0;
    if(esp_lcd_panel_swap_xy(s->panel,(value&1)!=0)!=ESP_OK)return -1;
    return esp_lcd_panel_mirror(s->panel,value==1 || value==2,value==2 || value==3)==ESP_OK?0:-1;
}
int tyvor_tft_spi_init(tyvor_tft_spi_t *s,const tyvor_display_config_t *c,const tyvor_display_pins_t *pins,tyvor_display_backend_t *backend) {
    if(!s || !tyvor_display_config_valid(c) || !backend)return -1;
    memset(s,0,sizeof(*s));int policy=tyvor_display_pin_policy(pins);
    if(policy<0)return policy;
    *backend=(tyvor_display_backend_t){s,start,wait_done,micros,rotation};
    if(policy==TYVOR_DISPLAY_DISABLED)return policy; /* before any SPI/GPIO/LCD API */
    s->done=xSemaphoreCreateBinary();if(!s->done)return -1;
    if(pins->backlight>=0) {
        gpio_config_t bl={.pin_bit_mask=UINT64_C(1)<<pins->backlight,.mode=GPIO_MODE_OUTPUT};
        if(gpio_config(&bl)!=ESP_OK || gpio_set_level(pins->backlight,CONFIG_TYVOR_TFT_BACKLIGHT_ACTIVE_LOW?1:0)!=ESP_OK)return -1;
    }
    spi_bus_config_t bus={.mosi_io_num=pins->mosi,.miso_io_num=pins->miso,.sclk_io_num=pins->sclk,
        .quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=TYVOR_DISPLAY_BUFFER_BYTES};
    if(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO)!=ESP_OK)return -1;
    esp_lcd_panel_io_handle_t io=NULL;
    esp_lcd_panel_io_spi_config_t cfg={.dc_gpio_num=pins->dc,.cs_gpio_num=pins->cs,.pclk_hz=c->spi_hz,.spi_mode=0,
        .trans_queue_depth=2,.lcd_cmd_bits=8,.lcd_param_bits=8,.on_color_trans_done=completed,.user_ctx=s};
    if(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,&cfg,&io)!=ESP_OK)return -1;
    esp_lcd_panel_dev_config_t panel={.reset_gpio_num=pins->reset,.bits_per_pixel=16,
        .rgb_ele_order=CONFIG_TYVOR_TFT_BGR?LCD_RGB_ELEMENT_ORDER_BGR:LCD_RGB_ELEMENT_ORDER_RGB};
    if(esp_lcd_new_panel_ili9341(io,&panel,&s->panel)!=ESP_OK || esp_lcd_panel_reset(s->panel)!=ESP_OK ||
       esp_lcd_panel_init(s->panel)!=ESP_OK)return -1;
    s->hardware=1;
    if(rotation(s,c->rotation) || esp_lcd_panel_disp_on_off(s->panel,true)!=ESP_OK)return -1;
    if(pins->backlight>=0 && gpio_set_level(pins->backlight,CONFIG_TYVOR_TFT_BACKLIGHT_ACTIVE_LOW?0:1)!=ESP_OK)return -1;
    return 0;
}
