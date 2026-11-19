/* Same single-owner bridge and value-copy publication, source byte unchanged.
 */
#define app_main tyvor_existing_bridge_start
#include "../../../main/main.c"
#undef app_main
#include "board_pins.h"
#include "tft_build_config.h"
#include "tft_spi_backend.h"
#include "ui.h"
#if CONFIG_TYVOR_TFT_HARDWARE_GATE || CONFIG_TYVOR_TFT_TOUCH_RUNTIME
#error "TYVOR UI V3: display/touch hardware gates must stay OFF"
#endif
static tyvor_ui3_t ui;
static tyvor_tft_spi_t backend;
static tyvor_display_pipeline_t pipeline;
static int write_region(void *ctx, const tyvor_display_region_t *r,
                        const uint8_t *bytes, size_t n) {
  tyvor_display_pipeline_t *p = ctx;
  if (tyvor_display_begin(p, r, bytes, n))
    return -1;
  return tyvor_display_complete(p);
}
static uint64_t ui_micros(void *ctx) {
  (void)ctx;
  return (uint64_t)esp_timer_get_time();
}
static uint32_t ui_tick(void) {
  return (uint32_t)(esp_timer_get_time() / 1000);
}
static void ui_task(void *context) {
  (void)context;
  ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
  lv_init();
  lv_tick_set_cb(ui_tick);
  tyvor_display_config_t config;
  if (tyvor_display_config(&config, CONFIG_TYVOR_TFT_ROTATION) ||
      config.width != 320 || config.height != 240)
    abort();
  tyvor_display_pins_t pins = tyvor_tft_board_pins();
  tyvor_display_backend_t transport;
  if (tyvor_tft_spi_init(&backend, &config, &pins, &transport) < 0 ||
      tyvor_display_pipeline_init(&pipeline, &config, transport))
    abort();
  void *buffer = heap_caps_aligned_alloc(64, TYVOR_DISPLAY_BUFFER_BYTES,
                                         MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA |
                                             MALLOC_CAP_8BIT);
  tyvor_ui3_port_t port = {&pipeline, 320, 240, write_region, ui_micros};
  if (!buffer || tyvor_ui3_init(&ui, port, buffer, TYVOR_DISPLAY_BUFFER_BYTES))
    abort();
  uint64_t next_log = 0, next_page = millis() + 10000;
  static ui_publication_t publication;
  ESP_LOGW(
      "TYVOR-UI3",
      "0.5.0-ui-v3 / LVGL9.2.2; backend MOCK; gates OFF; aircraft ARM UNKNOWN");
  for (;;) {
    uint64_t now = millis();
    tyvor_ui_state_t state;
#if CONFIG_TYVOR_TFT_SIMULATOR
    tyvor_ui3_fixture(&state, now, (unsigned)((now / 8000 + 3) % 5));
#else
    tyvor_ui_state_init(&state);
    if (xQueuePeek(publications, &publication, 0) == pdTRUE)
      state = publication.screen;
#endif
    (void)publication;
    tyvor_ui3_view_t view;
    if (tyvor_ui3_project(&state, now, CONFIG_TYVOR_TFT_SIMULATOR, now, &view))
      abort();
    tyvor_ui3_update(&ui, &view);
    if (now >= next_page) {
      tyvor_ui3_action(&ui, UI3_NEXT);
      next_page = now + 10000;
    }
    (void)lv_timer_handler();
    tyvor_ui3_frame(&ui);
    if (now >= next_log) {
      lv_mem_monitor_t memory;
      lv_mem_monitor(&memory);
      ESP_LOGI("TYVOR-UI3",
               "%s frame_us=%" PRIu64 " max_us=%" PRIu64 " pixels=%" PRIu64
               " LVGL_used=%u heap=%u stack_free=%u; MOCK NOT SPI TIMING",
               tyvor_ui3_page_name(ui.page), ui.frame_us, ui.frame_max_us,
               ui.pixels, (unsigned)(memory.total_size - memory.free_size),
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL |
                                                 MALLOC_CAP_8BIT),
               (unsigned)uxTaskGetStackHighWaterMark(NULL));
      next_log = now + 1000;
    }
    ESP_ERROR_CHECK(esp_task_wdt_reset());
    vTaskDelay(pdMS_TO_TICKS(33));
  }
}
void app_main(void) {
  tyvor_existing_bridge_start();
  if (xTaskCreate(ui_task, "tyvor_ui3", 8192, NULL, 2, NULL) != pdPASS)
    abort();
}
