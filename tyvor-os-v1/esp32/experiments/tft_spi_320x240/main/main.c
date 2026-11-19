/* Isolated compatibility shim: preserve the tested bridge and its single owner.
 * Its publication queue is read using xQueuePeek, exactly as its diagnostics UI.
 * No copy/fork of protocol/UART logic and no modifications to the Phase2C file. */
#define app_main tyvor_existing_bridge_start
#include "../../../main/main.c"
#undef app_main
#include "tft_build_config.h"
#include "board_pins.h"
#include "tft_spi_backend.h"
#include "tft_lvgl.h"
static tyvor_tft_spi_t tft_backend;
static tyvor_display_pipeline_t display_pipeline;
static tyvor_tft_lvgl_t tft_ui;
static uint32_t tick_ms(void) {return (uint32_t)(esp_timer_get_time()/1000);}
static void tft_task(void *context) {
    (void)context;ESP_ERROR_CHECK(esp_task_wdt_add(NULL));lv_init();lv_tick_set_cb(tick_ms);
    tyvor_display_config_t config;if(tyvor_display_config(&config,CONFIG_TYVOR_TFT_ROTATION))abort();
    tyvor_display_pins_t pins=tyvor_tft_board_pins();tyvor_display_backend_t backend;
    int result=tyvor_tft_spi_init(&tft_backend,&config,&pins,&backend);
    if(result<0) {ESP_LOGE("TYVOR-TFT","Hardware configuration rejected before display start");abort();}
    if(tyvor_display_pipeline_init(&display_pipeline,&config,backend))abort();
    void *buffer=heap_caps_aligned_alloc(64,TYVOR_DISPLAY_BUFFER_BYTES,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!buffer || tyvor_tft_lvgl_init(&tft_ui,&display_pipeline,buffer,TYVOR_DISPLAY_BUFFER_BYTES))abort();
    ESP_LOGW("TYVOR-TFT","backend=%s; UART gate OFF; touch runtime OFF; no aircraft ARM authority",
        tft_backend.hardware?"ILI9341 CANDIDATE":"MOCK / NO DISPLAY GPIO");
#if CONFIG_TYVOR_TFT_BOOT_DIAGNOSTICS
    for(unsigned pattern=0;pattern<=TYVOR_TFT_DIAG_GEOMETRY;++pattern) {
        tyvor_tft_lvgl_diagnostic(&tft_ui,pattern);tyvor_tft_lvgl_frame(&tft_ui);ESP_ERROR_CHECK(esp_task_wdt_reset());vTaskDelay(pdMS_TO_TICKS(750));
    }
    for(unsigned r=0;r<4;++r) {
        if(tyvor_tft_lvgl_rotate(&tft_ui,r))abort();
        tyvor_tft_lvgl_diagnostic(&tft_ui,TYVOR_TFT_DIAG_GEOMETRY);tyvor_tft_lvgl_frame(&tft_ui);ESP_ERROR_CHECK(esp_task_wdt_reset());vTaskDelay(pdMS_TO_TICKS(750));
    }
    if(tyvor_tft_lvgl_rotate(&tft_ui,CONFIG_TYVOR_TFT_ROTATION))abort();
#endif
    uint64_t next_log=0,next_page=millis()+5000;static ui_publication_t copy;
    for(;;) {
        uint64_t now=millis();tyvor_ui_state_t state;
#if CONFIG_TYVOR_TFT_SIMULATOR
        tyvor_tft_fixture(&state,now,(unsigned)((now/4000)%4));
#else
        tyvor_ui_state_init(&state);if(xQueuePeek(publications,&copy,0)==pdTRUE)state=copy.screen;
#endif
        (void)copy;tyvor_tft_view_t view;
        if(tyvor_tft_project(&state,now,CONFIG_TYVOR_TFT_SIMULATOR,now,&view))abort();
        tyvor_tft_lvgl_update(&tft_ui,&view);
        if(now>=next_page) {tyvor_tft_lvgl_navigate(&tft_ui,1);next_page=now+5000;}
        tyvor_tft_lvgl_frame(&tft_ui);(void)lv_timer_handler();
        if(now>=next_log) {
            ESP_LOGI("TYVOR-TFT","%ux%u R%u backend=%s frames=%" PRIu32 " frame_us=%" PRIu64 " max_us=%" PRIu64
                " chunk_max_us=%" PRIu64 " bytes=%" PRIu64 " heap_internal=%u largest=%u psram_free=%u UI_stack_free=%u",
                display_pipeline.config.width,display_pipeline.config.height,display_pipeline.config.rotation*90,
                tft_backend.hardware?"SPI-DMA":"MOCK: NOT SPI TIMING",tft_ui.frames,tft_ui.last_frame_us,tft_ui.max_frame_us,
                display_pipeline.max_transfer_us,display_pipeline.pixels*2,(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                (unsigned)uxTaskGetStackHighWaterMark(NULL));next_log=now+1000;
        }
        ESP_ERROR_CHECK(esp_task_wdt_reset());vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void app_main(void) {
    tyvor_existing_bridge_start(); /* emits its unchanged historical bridge banner */
    ESP_LOGW("TYVOR-TFT","TYVOR TFT 3.2 EXPERIMENT; panel controller PROBABLE only; hardware NOT VERIFIED");
    if(xTaskCreate(tft_task,"tyvor_tft",8192,NULL,2,NULL)!=pdPASS)abort();
}
