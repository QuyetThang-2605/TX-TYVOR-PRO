#include "tft_spi_backend.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d FAIL %s\n",__FILE__,__LINE__,#x);return 0;}}while(0)
static unsigned hardware_calls,draws;static int auto_complete=1,draw_error;
static fake_semaphore_t semaphore;static int64_t now_us=1000;
static esp_lcd_panel_io_spi_config_t io_config;static spi_bus_config_t bus_config;
static esp_lcd_panel_dev_config_t panel_config;static int host,dma;static bool swapped,mirror_x,mirror_y;
SemaphoreHandle_t xSemaphoreCreateBinary(void){++hardware_calls;return &semaphore;}
BaseType_t xSemaphoreTake(SemaphoreHandle_t s,unsigned ticks){if(ticks && auto_complete){BaseType_t ignored;io_config.on_color_trans_done(NULL,NULL,io_config.user_ctx);(void)ignored;now_us+=200;}if(s->count){--s->count;return pdTRUE;}return pdFALSE;}
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t s,BaseType_t *wake){++s->count;*wake=pdTRUE;return pdTRUE;}
int gpio_config(const gpio_config_t *cfg){(void)cfg;++hardware_calls;return ESP_OK;}
int gpio_set_level(int pin,unsigned level){(void)pin;(void)level;++hardware_calls;return ESP_OK;}
int spi_bus_initialize(int h,const spi_bus_config_t *cfg,int d){++hardware_calls;host=h;dma=d;bus_config=*cfg;return ESP_OK;}
int esp_lcd_new_panel_io_spi(esp_lcd_spi_bus_handle_t h,const esp_lcd_panel_io_spi_config_t *cfg,esp_lcd_panel_io_handle_t *out){(void)h;++hardware_calls;io_config=*cfg;*out=(void *)(uintptr_t)1;return ESP_OK;}
int esp_lcd_new_panel_ili9341(esp_lcd_panel_io_handle_t io,const esp_lcd_panel_dev_config_t *cfg,esp_lcd_panel_handle_t *out){(void)io;++hardware_calls;panel_config=*cfg;*out=(void *)(uintptr_t)2;return ESP_OK;}
int esp_lcd_panel_reset(esp_lcd_panel_handle_t p){(void)p;++hardware_calls;return ESP_OK;}
int esp_lcd_panel_init(esp_lcd_panel_handle_t p){(void)p;++hardware_calls;return ESP_OK;}
int esp_lcd_panel_disp_on_off(esp_lcd_panel_handle_t p,bool on){(void)p;(void)on;++hardware_calls;return ESP_OK;}
int esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t p,int x1,int y1,int x2,int y2,const void *pixels){(void)p;(void)x1;(void)y1;(void)x2;(void)y2;(void)pixels;++draws;return draw_error?-1:ESP_OK;}
int esp_lcd_panel_swap_xy(esp_lcd_panel_handle_t p,bool yes){(void)p;++hardware_calls;swapped=yes;return ESP_OK;}
int esp_lcd_panel_mirror(esp_lcd_panel_handle_t p,bool x,bool y){(void)p;++hardware_calls;mirror_x=x;mirror_y=y;return ESP_OK;}
int64_t esp_timer_get_time(void){return now_us;}
static tyvor_tft_spi_t spi;static tyvor_display_config_t config;static tyvor_display_backend_t backend;static tyvor_display_pipeline_t pipeline;
static tyvor_display_pins_t pins={12,11,13,10,9,8,7,1,1,1}; /* synthetic fixture, NOT board assignment */
static uint8_t pixel[2]={0xf8,0};static tyvor_display_region_t area={0,0,1,1};
static int initialize(void){if(tyvor_display_config(&config,0))return -1;return tyvor_tft_spi_init(&spi,&config,&pins,&backend);}
static int tft_spi_gate_off(void){pins.hardware_gate=0;CHECK(initialize()==TYVOR_DISPLAY_DISABLED && !hardware_calls && !spi.hardware);CHECK(!backend.start(backend.context,&area,pixel,2));CHECK(!backend.wait(backend.context,250) && !hardware_calls && !draws);return 1;}
static int tft_spi_unverified(void){pins.module_verified=0;CHECK(initialize()<0 && !hardware_calls);pins.module_verified=1;pins.board_verified=0;CHECK(initialize()<0 && !hardware_calls);return 1;}
static int tft_spi_bad_config(void){CHECK(!tyvor_display_config(&config,0));config.width=999;CHECK(tyvor_tft_spi_init(&spi,&config,&pins,&backend)<0 && !hardware_calls);pins.mosi=35;CHECK(initialize()<0 && !hardware_calls);return 1;}
static int tft_spi_dma_config(void){CHECK(!initialize());CHECK(spi.hardware && host==SPI2_HOST && dma==SPI_DMA_CH_AUTO);CHECK(bus_config.max_transfer_sz==TYVOR_DISPLAY_BUFFER_BYTES && io_config.pclk_hz==10000000 && io_config.trans_queue_depth==2 && panel_config.bits_per_pixel==16);CHECK(!tyvor_display_pipeline_init(&pipeline,&config,backend));CHECK(!tyvor_display_begin(&pipeline,&area,pixel,2));CHECK(pipeline.busy && !pipeline.transfers && draws==1);CHECK(!tyvor_display_complete(&pipeline));CHECK(!pipeline.busy && pipeline.transfers==1 && pipeline.last_transfer_us==200);return 1;}
static int tft_spi_timeout(void){CHECK(!initialize());auto_complete=0;CHECK(!tyvor_display_pipeline_init(&pipeline,&config,backend));CHECK(!tyvor_display_begin(&pipeline,&area,pixel,2));CHECK(tyvor_display_complete(&pipeline)<0 && pipeline.busy && pipeline.fault && pipeline.owned_buffer==pixel && !pipeline.transfers);return 1;}
static int tft_spi_rotation(void){CHECK(!initialize());for(unsigned r=0;r<4;++r){CHECK(!backend.rotate(backend.context,r));CHECK(swapped==(bool)(r%2) && mirror_x==(r==1 || r==2) && mirror_y==(r==2 || r==3));}return 1;}
int main(int argc,char **argv){if(argc!=2)return 2;
#define CASE(x) if(!strcmp(argv[1],#x))return x()?0:1
 CASE(tft_spi_gate_off);CASE(tft_spi_unverified);CASE(tft_spi_bad_config);CASE(tft_spi_dma_config);CASE(tft_spi_timeout);CASE(tft_spi_rotation);
 return 2;}
