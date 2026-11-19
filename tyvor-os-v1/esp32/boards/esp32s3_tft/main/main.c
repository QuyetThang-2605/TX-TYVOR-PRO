#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_rom_sys.h"
#include "soc/gpio_struct.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_task_wdt.h"
#include "esp_random.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "bootloader_random.h"
#include "tyvor_async_tx.h"
#include "tyvor_bench_transport.h"
#include "tyvor_ui_endpoint.h"
#include "tyvor_diagnostics.h"
#include "ui.h"
#include "branding.h"
#include "brand_assets.h"
#include "ui_theme.h"
#include "tyvor_boot_logo.h"

/* ============================================================================
 * PINOUT — YD-ESP32-23 (ESP32-S3-N8R2) <-> 3.2" / 2.4" TFT SPI 240x320 ILI9341
 * ============================================================================
 *  1. VCC       -> 5Vin
 *  2. GND       -> GND
 *  3. CS        -> GPIO10
 *  4. RESET     -> GPIO13
 *  5. DC        -> GPIO9
 *  6. SDI(MOSI) -> GPIO11
 *  7. SCK       -> GPIO12
 *  8. LED       -> 3V3
 *
 * Optional XPT2046 Touch (for 2.4" Touch TFT):
 * 10. T_CLK     -> GPIO6
 * 11. T_CS      -> GPIO7
 * 12. T_DIN     -> GPIO15
 * 13. T_DO      -> GPIO16
 * 14. T_IRQ     -> GPIO5
 *
 * Inter-MCU UART1 (Protocol v2 @ 115200 8N1):
 *  - GPIO17     -> UART1_TX (to STM32 PA3)
 *  - GPIO18     -> UART1_RX (from STM32 PA2)
 * ========================================================================== */

#define TFT_CS_GPIO     10
#define TFT_RST_GPIO    13
#define TFT_DC_GPIO     9
#define TFT_MOSI_GPIO   11
#define TFT_SCK_GPIO    12

#define TOUCH_CLK_GPIO  6
#define TOUCH_CS_GPIO   7
#define TOUCH_DIN_GPIO  15
#define TOUCH_DO_GPIO   16
#define TOUCH_IRQ_GPIO  5

#define TFT_CS_MASK     (1UL << TFT_CS_GPIO)
#define TFT_RST_MASK    (1UL << TFT_RST_GPIO)
#define TFT_DC_MASK     (1UL << TFT_DC_GPIO)
#define TFT_MOSI_MASK   (1UL << TFT_MOSI_GPIO)
#define TFT_SCK_MASK    (1UL << TFT_SCK_GPIO)

#define TOUCH_CLK_MASK  (1UL << TOUCH_CLK_GPIO)
#define TOUCH_CS_MASK   (1UL << TOUCH_CS_GPIO)
#define TOUCH_DIN_MASK  (1UL << TOUCH_DIN_GPIO)
#define TOUCH_DO_MASK   (1UL << TOUCH_DO_GPIO)
#define TOUCH_IRQ_MASK  (1UL << TOUCH_IRQ_GPIO)

#define BRIDGE_UART     UART_NUM_1
static const char *TAG = "TYVOR-S3-UI";

static tyvor_ui_endpoint_t ui_endpoint;
static tyvor_async_tx_t tx_queue;
static tyvor_bench_transport_t uart_transport;
static QueueHandle_t uart_events;
static bool uart_ready;
static int uart_swapped = 0;

static tyvor_ui321_t oxbot_ui;
static lv_indev_t *touch_indev;
static DMA_ATTR uint8_t lvgl_draw_buffer[TYVOR_DISPLAY_BUFFER_BYTES];

static uint64_t millis(void) {
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static uint32_t lvgl_tick_cb(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static uint64_t ui_micros_cb(void *ctx) {
    (void)ctx;
    return (uint64_t)esp_timer_get_time();
}

static void service_uart_bridge(void);

/* ============================================================================
 * HARDWARE SPI2 (FSPI) DMA FOR TFT (320x240 Landscape @ 40 MHz)
 * Pins: MOSI=GPIO11, SCK=GPIO12, CS=GPIO10, DC=GPIO9, RST=GPIO13
 * ========================================================================== */
static spi_device_handle_t tft_spi = NULL;

static void ili9341_cmd(uint8_t cmd) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    GPIO.out_w1tc = TFT_DC_MASK;
    (void)spi_device_polling_transmit(tft_spi, &t);
}

static void ili9341_data(const uint8_t *data, size_t len) {
    if (!len) return;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    GPIO.out_w1ts = TFT_DC_MASK;
    (void)spi_device_polling_transmit(tft_spi, &t);
}

static void ili9341_fill_solid(uint16_t rgb565) {
    uint8_t col[4] = {0, 0, (uint8_t)(319 >> 8), (uint8_t)(319 & 0xFF)};
    uint8_t row[4] = {0, 0, (uint8_t)(239 >> 8), (uint8_t)(239 & 0xFF)};
    ili9341_cmd(0x2A);
    ili9341_data(col, 4);
    ili9341_cmd(0x2B);
    ili9341_data(row, 4);
    ili9341_cmd(0x2C);

    uint8_t hi = (uint8_t)(rgb565 >> 8);
    uint8_t lo = (uint8_t)(rgb565 & 0xFF);
    static DMA_ATTR uint8_t line_buf[320 * 2 * 16];
    for (int i = 0; i < 320 * 16; ++i) {
        line_buf[i * 2] = hi;
        line_buf[i * 2 + 1] = lo;
    }
    GPIO.out_w1ts = TFT_DC_MASK;
    for (int b = 0; b < 240 / 16; ++b) {
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = sizeof(line_buf) * 8;
        t.tx_buffer = line_buf;
        (void)spi_device_polling_transmit(tft_spi, &t);
    }
}

static void ili9341_init(void) {
    gpio_config_t out_conf = {
        .pin_bit_mask = TFT_RST_MASK | TFT_DC_MASK |
                        TOUCH_CLK_MASK | TOUCH_CS_MASK | TOUCH_DIN_MASK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_conf);

    gpio_config_t in_conf = {
        .pin_bit_mask = TOUCH_DO_MASK | TOUCH_IRQ_MASK | (1ULL << 0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_conf);

    GPIO.out_w1ts = TOUCH_CS_MASK | TFT_RST_MASK;
    GPIO.out_w1tc = TOUCH_CLK_MASK;

    spi_bus_config_t buscfg = {
        .mosi_io_num = TFT_MOSI_GPIO,
        .miso_io_num = -1,
        .sclk_io_num = TFT_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 320 * 240 * 2 + 64,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 80000000, /* 80 MHz Hardware DMA SPI */
        .mode = 0,
        .spics_io_num = TFT_CS_GPIO,
        .queue_size = 4,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &tft_spi));

    /* Hardware Reset */
    vTaskDelay(pdMS_TO_TICKS(50));
    GPIO.out_w1tc = TFT_RST_MASK;
    vTaskDelay(pdMS_TO_TICKS(50));
    GPIO.out_w1ts = TFT_RST_MASK;
    vTaskDelay(pdMS_TO_TICKS(150));

    /* Software Reset */
    ili9341_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(150));

    /* Sleep Out */
    ili9341_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));

    /* Interface Pixel Format: 16-bit RGB565 */
    { const uint8_t d[] = {0x55}; ili9341_cmd(0x3A); ili9341_data(d, sizeof(d)); }

    /* Memory Data Access Control (MADCTL): Landscape */
    { const uint8_t d[] = {0x70}; ili9341_cmd(0x36); ili9341_data(d, sizeof(d)); }

    /* Display Inversion ON (Required for ST7789 IPS panels) */
    ili9341_cmd(0x21);

    /* ST7789 Frame Rate Control (FRCTRL2 0xC6): 0x03 = 99 Hz panel refresh (up from 60 Hz) */
    { const uint8_t d[] = {0x03}; ili9341_cmd(0xC6); ili9341_data(d, sizeof(d)); }

    /* Normal Display Mode ON */
    ili9341_cmd(0x13);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Clear entire screen solid black */
    ili9341_fill_solid(0x0000);

    /* Display ON */
    ili9341_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static int tft_write_region(void *ctx, const tyvor_display_region_t *r, const uint8_t *bytes, size_t n) {
    (void)ctx;
    if (!r || !bytes || !n || r->x2 <= r->x1 || r->y2 <= r->y1) return -1;
    uint16_t x0 = (uint16_t)r->x1, x1 = (uint16_t)(r->x2 - 1);
    uint16_t y0 = (uint16_t)r->y1, y1 = (uint16_t)(r->y2 - 1);
    uint8_t col[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    uint8_t row[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};

    ili9341_cmd(0x2A);
    ili9341_data(col, 4);
    ili9341_cmd(0x2B);
    ili9341_data(row, 4);
    ili9341_cmd(0x2C);

    GPIO.out_w1ts = TFT_DC_MASK;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = n * 8;
    t.tx_buffer = bytes;
    (void)spi_device_polling_transmit(tft_spi, &t);
    return 0;
}

/* ============================================================================
 * XPT2046 TOUCH CONTROLLER DRIVER (2.4" ST7789 320x240 LANDSCAPE)
 * ========================================================================== */
static inline void touch_delay(void) {
    /* ~200ns half-period at 240MHz CPU -> ~2.0MHz XPT2046 clock (within 2.5MHz spec) */
    for (volatile int i = 0; i < 48; ++i) __asm__ __volatile__("nop");
}

static uint16_t xpt2046_xfer(uint8_t cmd) {
    for (int i = 7; i >= 0; --i) {
        if (cmd & (1u << i)) GPIO.out_w1ts = TOUCH_DIN_MASK;
        else GPIO.out_w1tc = TOUCH_DIN_MASK;
        touch_delay();
        GPIO.out_w1ts = TOUCH_CLK_MASK;
        touch_delay();
        GPIO.out_w1tc = TOUCH_CLK_MASK;
        touch_delay();
    }
    touch_delay();
    uint16_t val = 0;
    for (int i = 15; i >= 0; --i) {
        GPIO.out_w1ts = TOUCH_CLK_MASK;
        touch_delay();
        if (gpio_get_level(TOUCH_DO_GPIO)) val |= (uint16_t)(1u << i);
        GPIO.out_w1tc = TOUCH_CLK_MASK;
        touch_delay();
    }
    return (uint16_t)((val >> 3) & 0x0FFFu);
}

static uint16_t median3(uint16_t a, uint16_t b, uint16_t c) {
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) { b = c; }
    return a > b ? a : b;
}

enum {
    FIGMA_PAGE_HOME = 0,
    FIGMA_PAGE_MODELS,
    FIGMA_PAGE_MODEL_LAYOUT,
    FIGMA_PAGE_CH_VIEW,
    FIGMA_PAGE_CHANNELS,
    FIGMA_PAGE_TELEMETRY,
    FIGMA_PAGE_QUICK_SETTINGS,
    FIGMA_PAGE_QUICKSET_MENU,
    FIGMA_PAGE_TX_POWER,
    FIGMA_PAGE_ELRS_SET,
    FIGMA_PAGE_RF_OUTPUT,
    FIGMA_PAGE_FIND,
    FIGMA_PAGE_BIND,
    FIGMA_PAGE_SETTINGS_MENU,
    FIGMA_PAGE_STICK_CAL,
    FIGMA_PAGE_STICK_MODE,
    FIGMA_PAGE_SCR_SAVER,
    FIGMA_PAGE_ABOUT,
    FIGMA_PAGE_SLEEP_SAVER,
    FIGMA_PAGE_COUNT
};

/* ============================================================================
 * REAL TRANSMITTER OS — MODEL PROFILES & SYSTEM CONFIGURATION STATE
 * ========================================================================== */
typedef struct {
    const char *tab_name;
    const char *full_title;
    const char *mode_name;
    const char *volt_str;
    const char *mix_type_str;
    uint16_t default_power_mw;
    uint16_t default_rate_hz;
    uint32_t theme_hex;
    uint32_t badge_hex;
} tyvor_os_model_profile_t;

static const tyvor_os_model_profile_t k_os_models[4] = {
    {"CRU",   "CRU Cruiser",    "ACRO",  "19.0V", "4CH ACRO",   400, 250, 0x3B82C4, 0xEAB308},
    {"QUAD",  "QUAD Freestyle", "QUAD",  "22.2V", "4CH EXPO",   250, 500, 0x0284C7, 0x38BDF8},
    {"FIXED", "FIXED Wing",     "WING",  "16.8V", "ELEVON MIX", 500, 150, 0x10B981, 0x22C55E},
    {"VTAIL", "VTAIL Glider",   "VTAIL", "12.6V", "V-TAIL MIX", 100, 250, 0x8B5CF6, 0xC084FC},
};

static int os_active_model = 0; /* 0=CRU, 1=QUAD, 2=FIXED, 3=VTAIL */
static int os_viewed_model = 0; /* Currently browsed tab on MODELS screen */
static int os_pending_model = 0;/* Target model in Switch Model confirmation modal */
static int os_model_modal_open = 0;
static int os_power_idx = 2;    /* 0=100mW, 1=250mW, 2=400mW, 3=500mW, 4=1000mW */
static const uint16_t k_power_mw_table[5] = {100, 250, 400, 500, 1000};
static int os_elrs_rate_idx = 2;/* 0=50Hz, 1=150Hz, 2=250Hz, 3=500Hz */
static const uint16_t k_elrs_rate_table[4] = {50, 150, 250, 500};
static int os_finder_active = 0;
static int os_rf_output_on = 0;
static int os_rf_external = 0;  /* 0=Internal TX, 1=External TX */
static int os_bind_active = 0;
static int os_telem_ratio_idx = 2; /* 0=1:2, 1=1:16, 2=1:64, 3=1:128 */
static const char *k_telem_ratio_table[4] = {"1:2", "1:16", "1:64", "1:128"};
static int os_model_match = 1;
static uint64_t os_qs_local_grace_ms = 0;
static int os_lang_vi = 0;
static int os_stick_mode = 2;   /* Mode 2 (default) or Mode 1 */
static int os_thr_lock_on = 1;  /* Safety Throttle Lock when DISARMED */
static int os_brightness_pct = 75;
static int os_volume_pct = 45;
static int os_bt_enabled = 1;
static int os_touch_locked = 0;
static int os_cal_done_flash = 0;
static uint32_t os_sleep_timeout_sec = 120;
static const uint32_t k_sleep_timeout_table[5] = {15, 30, 60, 120, 0};

static unsigned figma_page = 0xFFu;
static unsigned nav_return_page = 0; /* Page to return to from CH_VIEW / CHANNELS */
static int touch_last_x = 160, touch_last_y = 120, touch_last_z = 0;
static int touch_down, touch_start_x, touch_start_y;
static int touch_best_x = 160, touch_best_y = 120, touch_best_z = 0;
static int touch_drag_frames = 0;
static int touch_is_swiping = 0;
static uint64_t touch_suppress_click_until_ms = 0;
static uint64_t last_user_activity_ms = 0;
static int sleep_active = 0;
static unsigned active_non_sleep_page = 0; /* FIGMA_PAGE_HOME */

static int xpt2046_sample(int *out_x, int *out_y) {
    int irq = gpio_get_level(TOUCH_IRQ_GPIO);

    GPIO.out_w1tc = TOUCH_CS_MASK;
    touch_delay();

    /* Measure initial touch pressure Z1 (command 0xB0) */
    uint16_t z1_pre = xpt2046_xfer(0xB0);

    /* Reject idle or weak ghost/lift-off contact */
    int min_z = 260;
    if (irq != 0 && z1_pre < min_z) {
        (void)xpt2046_xfer(0x00);
        GPIO.out_w1ts = TOUCH_CS_MASK;
        return 0;
    }

    /* 3 samples with median + spread check for noise suppression */
    uint16_t x0 = xpt2046_xfer(0xD0);
    uint16_t x1 = xpt2046_xfer(0xD0);
    uint16_t x2 = xpt2046_xfer(0xD0);
    uint16_t rx = median3(x0, x1, x2);

    uint16_t y0 = xpt2046_xfer(0x90);
    uint16_t y1 = xpt2046_xfer(0x90);
    uint16_t y2 = xpt2046_xfer(0x90);
    uint16_t ry = median3(y0, y1, y2);

    /* Verify pressure Z1 AFTER X/Y readout to reject mid-read finger lift-off spikes */
    uint16_t z1_post = xpt2046_xfer(0xB0);

    /* Power down and restore PENIRQ */
    (void)xpt2046_xfer(0x00);
    GPIO.out_w1ts = TOUCH_CS_MASK;

    if (z1_pre < min_z || z1_post < min_z) {
        return 0;
    }
    if (abs((int)z1_pre - (int)z1_post) > 350) {
        return 0;
    }
    uint16_t xmin = (x0 < x1) ? ((x0 < x2) ? x0 : x2) : ((x1 < x2) ? x1 : x2);
    uint16_t xmax = (x0 > x1) ? ((x0 > x2) ? x0 : x2) : ((x1 > x2) ? x1 : x2);
    uint16_t ymin = (y0 < y1) ? ((y0 < y2) ? y0 : y2) : ((y1 < y2) ? y1 : y2);
    uint16_t ymax = (y0 > y1) ? ((y0 > y2) ? y0 : y2) : ((y1 > y2) ? y1 : y2);
    if ((xmax - xmin) > 220u || (ymax - ymin) > 220u) {
        return 0;
    }

    /* Filter invalid readings outside resistive range */
    if (rx < 50 || rx > 4000 || ry < 50 || ry > 4000) {
        return 0;
    }

    /* Calibrated active area (340..3600) so corners reach full (0..319, 0..239)
     * For 2.4" TFT SPI 240x320 V1.3 (L1435-2.4, ST7789 MADCTL=0x70):
     * rx (0xD0) = Horizontal (0..319 Left -> Right)
     * ry (0x90) = Vertical   (239..0 Bottom -> Top, inverted to 0..239 Top -> Bottom) */
    int sx = (int)((rx - 340) * 319L / (3600 - 340));
    int sy = 239 - (int)((ry - 340) * 239L / (3600 - 340));

    if (sx < 0) sx = 0;
    if (sx > 319) sx = 319;
    if (sy < 0) sy = 0;
    if (sy > 239) sy = 239;

    *out_x = sx;
    *out_y = sy;
    touch_last_z = (int)((z1_pre + z1_post) / 2);

    static uint64_t last_log_ms = 0;
    uint64_t now_ms = millis();
    if (now_ms - last_log_ms >= 120) {
        last_log_ms = now_ms;
        ESP_LOGI("TOUCH", "TOUCH raw=(%d,%d) z=(%d,%d) -> screen=(%d,%d)",
                 rx, ry, z1_pre, z1_post, sx, sy);
    }

    return 1;
}

/* ============================================================================
 * TYVOR OS — TOUCH NAVIGATION ARCHITECTURE (SPECIFICATION SECTIONS 1–8)
 * Separated into:
 *   1. Gesture Recognition   (tyvor_gesture_t)
 *   2. Navigation Controller (tyvor_nav_resolve_target)
 *   3. Transition Controller (tyvor_transition_t — 1:1 Dual-Screen Finger-Follow)
 * ========================================================================== */
#define TYVOR_PAGE_NONE          0xFFu
#define TYVOR_GESTURE_LOCK_PX    32    /* Min px to lock single axis (Horiz vs Vert) */
#define TYVOR_COMMIT_HORIZ_PX    48    /* Commit threshold for horizontal swipe */
#define TYVOR_COMMIT_VERT_PX     38    /* Commit threshold for vertical swipe */
#define TYVOR_SWIPE_COOLDOWN_MS  140   /* Anti-repeat cooldown between continuous swipes */

typedef enum {
    TYVOR_AXIS_NONE = 0,
    TYVOR_AXIS_HORIZ,
    TYVOR_AXIS_VERT
} tyvor_gesture_axis_t;

typedef enum {
    TYVOR_SWIPE_NONE = 0,
    TYVOR_SWIPE_RIGHT, /* Finger moves Left -> Right (dx > 0): Vuốt sang phải → */
    TYVOR_SWIPE_LEFT,  /* Finger moves Right -> Left (dx < 0): Vuốt sang trái ← */
    TYVOR_SWIPE_DOWN,  /* Finger moves Top -> Bottom (dy > 0): Vuốt xuống ↓     */
    TYVOR_SWIPE_UP     /* Finger moves Bottom -> Top (dy < 0): Vuốt lên ↑       */
} tyvor_swipe_dir_t;

typedef enum {
    TYVOR_TRANS_IDLE = 0,
    TYVOR_TRANS_DRAGGING,
    TYVOR_TRANS_COMMITTING,
    TYVOR_TRANS_SNAPPING_BACK
} tyvor_trans_state_t;

typedef struct {
    tyvor_gesture_axis_t locked_axis;
    tyvor_swipe_dir_t    active_dir;
    uint64_t             cooldown_until_ms;
} tyvor_gesture_t;

typedef struct {
    tyvor_trans_state_t  state;
    tyvor_gesture_axis_t axis;
    tyvor_swipe_dir_t    dir;
    unsigned             from_page;
    unsigned             to_page;
    int                  offset_px; /* Current screen displacement along locked axis */
    int                  span_px;   /* 320 for HORIZ, 240 for VERT */
} tyvor_transition_t;

static tyvor_gesture_t    g_gesture = {TYVOR_AXIS_NONE, TYVOR_SWIPE_NONE, 0};
static tyvor_transition_t g_trans   = {TYVOR_TRANS_IDLE, TYVOR_AXIS_NONE, TYVOR_SWIPE_NONE,
                                       FIGMA_PAGE_HOME, TYVOR_PAGE_NONE, 0, 320};

static lv_obj_t *page_containers[FIGMA_PAGE_COUNT];
static uint8_t   page_built[FIGMA_PAGE_COUNT];

static void f_ensure_page_built(unsigned page);
static void f_invalidate_page(unsigned page);
static void f_open_page(unsigned page);
static void f_open_page_dir(unsigned page, int enter_x, int enter_y);
static void f_navigate_back(void);
static void f_open_model_confirm_modal(int target_idx);
static void f_sync_model_confirm_modal(int target_idx);
static void f_close_model_confirm_modal(void);
static void f_confirm_switch_model(void);
static void f_refresh_models_page_dir(int enter_x);
static void esp_send_quickset_cmd(uint8_t cmd);
static void f_quickset_handle_card(int idx);
static void f_tx_power_select(int idx);
static void f_elrs_handle_row(int row);
static void f_rf_output_handle_row(int row);
static void f_find_toggle(void);
static void f_bind_toggle(void);
static void f_settings_handle_card(int idx);
static void f_stick_cal_toggle(void);
static void f_stick_cal_reset(void);
static void f_scr_saver_select(int idx);
static void os_config_save(void);
static void os_config_load(void);

/* ----------------------------------------------------------------------------
 * MODULE 1: NAVIGATION CONTROLLER (Sections 2 & 3 — Forward & Reverse Flows)
 * ----------------------------------------------------------------------------
 * Section 2 — Forward Flows:
 *   HOME           + Vuốt sang phải → (SWIPE_RIGHT) => Model Selection
 *   HOME           + Vuốt sang trái ← (SWIPE_LEFT)  => Telemetry
 *   HOME           + Vuốt xuống ↓     (SWIPE_DOWN)  => Quick Settings (QUICKSET_MENU)
 *   Quick Settings + Vuốt sang phải → (SWIPE_RIGHT) => Settings
 *   Settings       + Vuốt sang phải → (SWIPE_RIGHT) => About
 *
 * Section 3 — Reverse Flows:
 *   Model Selection + Vuốt sang trái ← (SWIPE_LEFT)  => HOME
 *   Telemetry       + Vuốt sang phải → (SWIPE_RIGHT) => HOME
 *   Quick Settings  + Vuốt lên ↑       (SWIPE_UP)    => HOME
 *   Settings        + Vuốt sang trái ← (SWIPE_LEFT)  => Quick Settings
 *   About           + Vuốt sang trái ← (SWIPE_LEFT)  => Settings
 * -------------------------------------------------------------------------- */
static unsigned tyvor_nav_resolve_target(unsigned cur_page, tyvor_swipe_dir_t dir) {
    if (cur_page == FIGMA_PAGE_MODELS && os_model_modal_open) {
        return cur_page;
    }
    switch (cur_page) {
    case FIGMA_PAGE_HOME:
        if (dir == TYVOR_SWIPE_RIGHT) return FIGMA_PAGE_TELEMETRY;
        if (dir == TYVOR_SWIPE_LEFT)  return FIGMA_PAGE_MODELS;
        if (dir == TYVOR_SWIPE_UP)    return FIGMA_PAGE_QUICKSET_MENU;
        if (dir == TYVOR_SWIPE_DOWN)  return FIGMA_PAGE_QUICK_SETTINGS;
        break;

    case FIGMA_PAGE_MODELS:
        if (dir == TYVOR_SWIPE_LEFT)  return FIGMA_PAGE_MODELS;
        if (dir == TYVOR_SWIPE_RIGHT) return (os_viewed_model > 0) ? FIGMA_PAGE_MODELS : FIGMA_PAGE_HOME;
        break;

    case FIGMA_PAGE_TELEMETRY:
        if (dir == TYVOR_SWIPE_LEFT)  return FIGMA_PAGE_HOME;
        break;

    case FIGMA_PAGE_QUICKSET_MENU:
        if (dir == TYVOR_SWIPE_LEFT)  return FIGMA_PAGE_SETTINGS_MENU;
        if (dir == TYVOR_SWIPE_DOWN)  return FIGMA_PAGE_HOME;
        break;

    case FIGMA_PAGE_SETTINGS_MENU:
        if (dir == TYVOR_SWIPE_LEFT)  return FIGMA_PAGE_ABOUT;
        if (dir == TYVOR_SWIPE_RIGHT) return FIGMA_PAGE_QUICKSET_MENU;
        break;

    case FIGMA_PAGE_ABOUT:
        if (dir == TYVOR_SWIPE_RIGHT) return FIGMA_PAGE_SETTINGS_MENU;
        break;

    case FIGMA_PAGE_QUICK_SETTINGS:
        if (dir == TYVOR_SWIPE_UP)    return FIGMA_PAGE_HOME;
        break;

    case FIGMA_PAGE_MODEL_LAYOUT:
        if (dir == TYVOR_SWIPE_LEFT || dir == TYVOR_SWIPE_RIGHT) {
            return FIGMA_PAGE_MODELS;
        }
        break;

    case FIGMA_PAGE_CH_VIEW:
    case FIGMA_PAGE_CHANNELS:
        if (dir == TYVOR_SWIPE_LEFT) {
            return (nav_return_page < FIGMA_PAGE_SLEEP_SAVER && nav_return_page != cur_page)
                   ? nav_return_page : FIGMA_PAGE_HOME;
        }
        break;

    default:
        break;
    }
    return TYVOR_PAGE_NONE;
}

/* ----------------------------------------------------------------------------
 * MODULE 2: SCREEN SWITCH CONTROLLER (Instant Page Switch — No Transition Anim)
 * -------------------------------------------------------------------------- */
static void tyvor_trans_hide_inactive_pages(unsigned keep_a, unsigned keep_b) {
    for (unsigned i = 0; i < FIGMA_PAGE_COUNT; ++i) {
        if (!page_containers[i]) continue;
        if (i == keep_a || i == keep_b) {
            lv_obj_remove_flag(page_containers[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(page_containers[i], 0, 0);
        } else {
            lv_obj_add_flag(page_containers[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(page_containers[i], 0, 0);
        }
    }
}

static lv_obj_t *w_mod_track = NULL;
static void f_refresh_models_page_dir(int enter_x);
static void f_ensure_page_built(unsigned page);
static void f_open_page_dir(unsigned page, int enter_x, int enter_y);

static void f_page_anim_x_cb(void *var, int32_t v) {
    lv_obj_set_x((lv_obj_t *)var, v);
}

static void f_page_anim_y_cb(void *var, int32_t v) {
    lv_obj_set_y((lv_obj_t *)var, v);
}

static void f_page_anim_ready_cb(lv_anim_t *a) {
    (void)a;
    tyvor_trans_hide_inactive_pages(figma_page, TYVOR_PAGE_NONE);
}

static void f_animate_page_slide(lv_obj_t *cont, int is_x, int from_v, int to_v, uint32_t dur_ms) {
    if (!cont) return;
    lv_anim_delete(cont, f_page_anim_x_cb);
    lv_anim_delete(cont, f_page_anim_y_cb);
    if (is_x) lv_obj_set_pos(cont, from_v, 0);
    else lv_obj_set_pos(cont, 0, from_v);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, cont);
    lv_anim_set_exec_cb(&a, is_x ? f_page_anim_x_cb : f_page_anim_y_cb);
    lv_anim_set_values(&a, from_v, to_v);
    lv_anim_set_duration(&a, dur_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&a, f_page_anim_ready_cb);
    lv_anim_start(&a);
}

static void tyvor_trans_drag_update(tyvor_gesture_axis_t axis, int delta_px) {
    if (figma_page >= FIGMA_PAGE_COUNT || figma_page == FIGMA_PAGE_SLEEP_SAVER) return;

    tyvor_swipe_dir_t new_dir = TYVOR_SWIPE_NONE;
    if (axis == TYVOR_AXIS_HORIZ) {
        new_dir = (delta_px >= 0) ? TYVOR_SWIPE_RIGHT : TYVOR_SWIPE_LEFT;
    } else {
        new_dir = (delta_px >= 0) ? TYVOR_SWIPE_DOWN : TYVOR_SWIPE_UP;
    }

    g_trans.state     = TYVOR_TRANS_DRAGGING;
    g_trans.axis      = axis;
    g_trans.dir       = new_dir;
    g_trans.from_page = figma_page;
    g_trans.to_page   = tyvor_nav_resolve_target(figma_page, new_dir);
    g_trans.offset_px = delta_px;

#define MOD_TRACK_BASE_X (-240)
#define MOD_CARD_STRIDE  220

    if (figma_page == FIGMA_PAGE_MODELS && axis == TYVOR_AXIS_HORIZ &&
        !os_model_modal_open && w_mod_track &&
        !(os_viewed_model == 0 && delta_px > 0)) {
        int clamped = delta_px;
        if (clamped > MOD_CARD_STRIDE) clamped = MOD_CARD_STRIDE;
        if (clamped < -MOD_CARD_STRIDE) clamped = -MOD_CARD_STRIDE;
        int target_x = MOD_TRACK_BASE_X + clamped;
        if (target_x != (int)lv_obj_get_x(w_mod_track)) {
            lv_obj_set_x(w_mod_track, target_x);
        }
        return;
    }

    /* Live interactive slide-in of target page container following finger */
    unsigned to = g_trans.to_page;
    if (to < FIGMA_PAGE_COUNT && to != figma_page) {
        if (to == FIGMA_PAGE_MODELS && figma_page != FIGMA_PAGE_MODELS) {
            os_viewed_model = os_active_model;
            f_invalidate_page(FIGMA_PAGE_MODELS);
        }
        f_ensure_page_built(to);
        if (page_containers[to]) {
            lv_anim_delete(page_containers[to], f_page_anim_x_cb);
            lv_anim_delete(page_containers[to], f_page_anim_y_cb);
            lv_obj_remove_flag(page_containers[to], LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(page_containers[to]);
            if (axis == TYVOR_AXIS_HORIZ) {
                int x = (new_dir == TYVOR_SWIPE_LEFT) ? (320 + delta_px) : (-320 + delta_px);
                if (x < -320) x = -320;
                if (x > 320) x = 320;
                lv_obj_set_pos(page_containers[to], x, 0);
            } else {
                int y = (new_dir == TYVOR_SWIPE_UP) ? (240 + delta_px) : (-240 + delta_px);
                if (y < -240) y = -240;
                if (y > 240) y = 240;
                lv_obj_set_pos(page_containers[to], 0, y);
            }
        }
    }
}

static void f_mod_track_anim_x_cb(void *var, int32_t v) {
    lv_obj_set_x((lv_obj_t *)var, v);
}

static void f_animate_mod_track(int from_x, int to_x, uint32_t dur_ms) {
    if (!w_mod_track) return;
    lv_anim_delete(w_mod_track, f_mod_track_anim_x_cb);
    lv_obj_set_x(w_mod_track, from_x);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, w_mod_track);
    lv_anim_set_exec_cb(&a, f_mod_track_anim_x_cb);
    lv_anim_set_values(&a, from_x, to_x);
    lv_anim_set_duration(&a, dur_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void tyvor_trans_release(void) {
    if (g_trans.state != TYVOR_TRANS_DRAGGING) return;

    int off_px = g_trans.offset_px;
    int abs_off = abs(off_px);
    unsigned target = g_trans.to_page;

    g_trans.state     = TYVOR_TRANS_IDLE;
    g_trans.to_page   = TYVOR_PAGE_NONE;
    g_trans.offset_px = 0;

    if (target != TYVOR_PAGE_NONE && abs_off >= 48) {
        if (figma_page == FIGMA_PAGE_MODELS && target == FIGMA_PAGE_MODELS) {
            int enter_x = 0;
            if (g_trans.dir == TYVOR_SWIPE_LEFT) {
                os_viewed_model = (os_viewed_model + 1) % 4;
                enter_x = MOD_CARD_STRIDE + off_px;
                if (enter_x < 20) enter_x = 20;
            } else if (g_trans.dir == TYVOR_SWIPE_RIGHT && os_viewed_model > 0) {
                os_viewed_model--;
                enter_x = -MOD_CARD_STRIDE + off_px;
                if (enter_x > -20) enter_x = -20;
            }
            g_gesture.cooldown_until_ms = millis() + TYVOR_SWIPE_COOLDOWN_MS;
            f_refresh_models_page_dir(enter_x);
            return;
        }
        if (target == FIGMA_PAGE_MODELS && figma_page != FIGMA_PAGE_MODELS) {
            os_viewed_model = os_active_model;
            f_invalidate_page(FIGMA_PAGE_MODELS);
        }
        if (target != FIGMA_PAGE_SLEEP_SAVER) {
            active_non_sleep_page = target;
        }
        g_gesture.cooldown_until_ms = millis() + TYVOR_SWIPE_COOLDOWN_MS;
        ESP_LOGI("NAV", "SLIDE SWITCH dir=%d from=%u -> to=%u",
                 (int)g_trans.dir, figma_page, target);
        if (page_containers[target]) {
            int is_x = (g_trans.axis == TYVOR_AXIS_HORIZ);
            int cur_v = is_x ? (int)lv_obj_get_x(page_containers[target])
                             : (int)lv_obj_get_y(page_containers[target]);
            figma_page = target;
            f_animate_page_slide(page_containers[target], is_x, cur_v, 0, 120);
        } else {
            f_open_page(target);
        }
    } else if (figma_page == FIGMA_PAGE_MODELS && w_mod_track) {
        int cur_x = (int)lv_obj_get_x(w_mod_track);
        if (cur_x != MOD_TRACK_BASE_X) f_animate_mod_track(cur_x, MOD_TRACK_BASE_X, 140);
    } else if (target < FIGMA_PAGE_COUNT && target != figma_page && page_containers[target]) {
        /* Cancelled swipe: hide preview target cleanly without flashing */
        tyvor_trans_hide_inactive_pages(figma_page, TYVOR_PAGE_NONE);
    }
}

static void tyvor_trans_step_anim(uint64_t now) {
    (void)now;
}

static lv_obj_t *w_qs_br_fill = NULL;
static lv_obj_t *w_qs_br_knob = NULL;
static lv_obj_t *w_qs_br_lbl = NULL;
static lv_obj_t *w_qs_vol_fill = NULL;
static lv_obj_t *w_qs_vol_knob = NULL;
static lv_obj_t *w_qs_vol_lbl = NULL;
static lv_obj_t *w_brightness_dim_overlay = NULL;
static int qs_slider_drag = 0; /* 0=none, 1=brightness, 2=volume */

static void f_apply_screen_brightness(int pct) {
    if (pct < 10) pct = 10;
    if (pct > 100) pct = 100;
    if (pct >= 100) {
        if (w_brightness_dim_overlay) {
            lv_obj_add_flag(w_brightness_dim_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    if (!w_brightness_dim_overlay) {
        w_brightness_dim_overlay = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(w_brightness_dim_overlay);
        lv_obj_set_pos(w_brightness_dim_overlay, 0, 0);
        lv_obj_set_size(w_brightness_dim_overlay, 320, 240);
        lv_obj_set_style_bg_color(w_brightness_dim_overlay, lv_color_hex(0x000000), 0);
        lv_obj_clear_flag(w_brightness_dim_overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }
    int opa = ((100 - pct) * 205) / 90;
    if (opa < 0) opa = 0;
    if (opa > 205) opa = 205;
    lv_obj_set_style_bg_opa(w_brightness_dim_overlay, (lv_opa_t)opa, 0);
    lv_obj_remove_flag(w_brightness_dim_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(w_brightness_dim_overlay);
}

static void f_qs_apply_slider(int which, int touch_x) {
    int pct = ((touch_x - 32) * 100) / 256;
    if (which == 1) {
        if (pct < 10) pct = 10;
        if (pct > 100) pct = 100;
        if (pct != os_brightness_pct) {
            os_brightness_pct = pct;
            f_apply_screen_brightness(pct);
            int w = (256 * pct) / 100;
            if (w < 6) w = 6;
            if (w_qs_br_fill) lv_obj_set_width(w_qs_br_fill, w);
            if (w_qs_br_knob) lv_obj_set_x(w_qs_br_knob, 10 + w - 7);
            if (w_qs_br_lbl) {
                char buf[12];
                snprintf(buf, sizeof(buf), "%d%%", pct);
                lv_label_set_text(w_qs_br_lbl, buf);
            }
        }
    } else if (which == 2) {
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        if (pct != os_volume_pct) {
            os_volume_pct = pct;
            int w = (256 * pct) / 100;
            if (w < 4) w = 4;
            if (w_qs_vol_fill) lv_obj_set_width(w_qs_vol_fill, w);
            if (w_qs_vol_knob) lv_obj_set_x(w_qs_vol_knob, 10 + w - 7);
            if (w_qs_vol_lbl) {
                char buf[12];
                snprintf(buf, sizeof(buf), "%d%%", pct);
                lv_label_set_text(w_qs_vol_lbl, buf);
            }
        }
    }
}

/* ----------------------------------------------------------------------------
 * MODULE 3: GESTURE RECOGNITION (Section 5 — Single-Axis Lock, Tap vs Swipe)
 * -------------------------------------------------------------------------- */
static int touch_release_misses = 0;
static uint64_t touch_post_swipe_ignore_until_ms = 0;

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    int x, y;
    uint64_t now = millis();

    if (now < touch_post_swipe_ignore_until_ms) {
        data->point.x = -10;
        data->point.y = -10;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (xpt2046_sample(&x, &y)) {
        touch_release_misses = 0;
        last_user_activity_ms = now;
        if (sleep_active) {
            sleep_active = 0;
            touch_suppress_click_until_ms = now + 260;
            f_open_page(active_non_sleep_page);
        }

        if (!touch_down) {
            touch_down = 1;
            touch_is_swiping = 0;
            touch_drag_frames = 0;
            qs_slider_drag = 0;
            touch_start_x = x;
            touch_start_y = y;
            touch_last_x = x;
            touch_last_y = y;
            touch_best_x = x;
            touch_best_y = y;
            touch_best_z = touch_last_z;
            g_gesture.locked_axis = TYVOR_AXIS_NONE;
            g_gesture.active_dir  = TYVOR_SWIPE_NONE;
            if (figma_page == FIGMA_PAGE_QUICK_SETTINGS && !os_touch_locked && x >= 16 && x <= 304) {
                if (y >= 54 && y <= 98) {
                    qs_slider_drag = 1;
                    f_qs_apply_slider(1, x);
                } else if (y >= 99 && y <= 144) {
                    qs_slider_drag = 2;
                    f_qs_apply_slider(2, x);
                }
            }
        } else {
            touch_last_x = x;
            touch_last_y = y;
            if (touch_last_z >= touch_best_z) {
                touch_best_x = x;
                touch_best_y = y;
                touch_best_z = touch_last_z;
            }
            if (qs_slider_drag != 0) {
                f_qs_apply_slider(qs_slider_drag, x);
                touch_suppress_click_until_ms = now + 220;
                data->point.x = touch_last_x;
                data->point.y = touch_last_y;
                data->state = LV_INDEV_STATE_RELEASED;
                return;
            }
            int dx = touch_last_x - touch_start_x;
            int dy = touch_last_y - touch_start_y;
            int adx = abs(dx);
            int ady = abs(dy);

            /* Only start swipe recognition if no transition animation is active and cooldown elapsed */
            int can_swipe = (g_trans.state == TYVOR_TRANS_IDLE || g_trans.state == TYVOR_TRANS_DRAGGING) &&
                            (now >= g_gesture.cooldown_until_ms) &&
                            !os_touch_locked &&
                            !os_model_modal_open;

            int lock_px = TYVOR_GESTURE_LOCK_PX;
            if (figma_page != FIGMA_PAGE_HOME && figma_page != FIGMA_PAGE_SLEEP_SAVER &&
                touch_start_x <= 68 && touch_start_y <= 46) {
                lock_px = 44; /* Prevent corner finger-roll from hijacking Back (<) tap */
            }

            if (can_swipe && (adx >= lock_px || ady >= lock_px)) {
                touch_drag_frames++;
                if (touch_drag_frames >= 3) {
                    /* Lock strictly to ONE axis (Horiz OR Vert) for the entire gesture */
                    if (g_gesture.locked_axis == TYVOR_AXIS_NONE) {
                        if (adx >= ady) {
                            g_gesture.locked_axis = TYVOR_AXIS_HORIZ;
                        } else {
                            g_gesture.locked_axis = TYVOR_AXIS_VERT;
                        }
                    }
                    if (figma_page == FIGMA_PAGE_TELEMETRY &&
                        g_gesture.locked_axis == TYVOR_AXIS_VERT) {
                        /* Allow native LVGL vertical scrolling inside Telemetry viewport */
                        touch_is_swiping = 0;
                    } else {
                        touch_is_swiping = 1;
                        touch_suppress_click_until_ms = now + 220;

                        if (g_gesture.locked_axis == TYVOR_AXIS_HORIZ) {
                            tyvor_trans_drag_update(TYVOR_AXIS_HORIZ, dx);
                        } else {
                            tyvor_trans_drag_update(TYVOR_AXIS_VERT, dy);
                        }
                    }
                }
            }
        }

        if (touch_is_swiping) {
            /* Move LVGL pointer off-screen before RELEASED so LVGL cancels any pressed widget */
            data->point.x = -10;
            data->point.y = -10;
            data->state = LV_INDEV_STATE_RELEASED;
        } else {
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
            data->state = LV_INDEV_STATE_PRESSED;
        }
    } else {
        if (touch_down) {
            if (++touch_release_misses < 3) {
                /* Absorb 1-2 frame resistive dropout mid-drag without breaking swipe */
                if (touch_is_swiping) {
                    data->point.x = -10;
                    data->point.y = -10;
                    data->state = LV_INDEV_STATE_RELEASED;
                } else {
                    data->point.x = touch_last_x;
                    data->point.y = touch_last_y;
                    data->state = LV_INDEV_STATE_PRESSED;
                }
                return;
            }
            touch_release_misses = 0;
            touch_down = 0;
            last_user_activity_ms = now;
            ESP_LOGI("TOUCH", "UP page=%u start=(%d,%d) best=(%d,%d) last=(%d,%d) z=%d swipe=%d",
                     figma_page, touch_start_x, touch_start_y, touch_best_x, touch_best_y,
                     touch_last_x, touch_last_y, touch_best_z, touch_is_swiping);
            if (qs_slider_drag != 0) {
                qs_slider_drag = 0;
                touch_suppress_click_until_ms = now + 260;
                os_config_save();
            } else if (touch_is_swiping) {
                touch_suppress_click_until_ms = now + 320;
                touch_post_swipe_ignore_until_ms = now + 260;
                tyvor_trans_release();
            } else if (abs(touch_last_x - touch_start_x) <= 20 &&
                       abs(touch_last_y - touch_start_y) <= 20) {
                /* Direct Hit Detection on MODELS & Switch-Model Modal using firmest press (touch_best) */
                if (figma_page == FIGMA_PAGE_MODELS) {
                    int tx = touch_best_x;
                    int ty = touch_best_y;
                    ESP_LOGI("MODEL_TOUCH", "tap best=(%d,%d) z=%d modal=%d",
                             tx, ty, touch_best_z, os_model_modal_open);
                    if (os_model_modal_open) {
                        if (ty >= 150 && ty <= 239) {
                            if (tx >= 20 && tx < 156) {
                                touch_suppress_click_until_ms = now + 260;
                                f_close_model_confirm_modal();
                            } else if (tx >= 156 && tx <= 300) {
                                touch_suppress_click_until_ms = now + 260;
                                f_confirm_switch_model();
                            }
                        } else if (ty >= 118 && ty < 150) {
                            for (int i = 0; i < 4; ++i) {
                                int px0 = 50 + i * 56;
                                if (tx >= px0 - 2 && tx <= px0 + 54) {
                                    touch_suppress_click_until_ms = now + 260;
                                    f_sync_model_confirm_modal(i);
                                    break;
                                }
                            }
                        } else if (tx < 20 || tx > 300 || ty < 14) {
                            touch_suppress_click_until_ms = now + 260;
                            f_close_model_confirm_modal();
                        }
                    } else {
                        if ((touch_start_x <= 60 && touch_start_y <= 34) ||
                            (tx <= 60 && ty <= 34) ||
                            (touch_last_x <= 60 && touch_last_y <= 34)) {
                            touch_suppress_click_until_ms = now + 260;
                            f_navigate_back();
                        } else if (ty >= 178 && ty <= 239) {
                            if (tx < 160) {
                                touch_suppress_click_until_ms = now + 260;
                                f_open_model_confirm_modal(os_viewed_model);
                            } else {
                                touch_suppress_click_until_ms = now + 260;
                                active_non_sleep_page = FIGMA_PAGE_MODEL_LAYOUT;
                                f_invalidate_page(FIGMA_PAGE_MODEL_LAYOUT);
                                f_open_page(FIGMA_PAGE_MODEL_LAYOUT);
                            }
                        } else if (ty >= 26 && ty <= 50) {
                            for (int i = 0; i < 4; ++i) {
                                int tx0 = 14 + i * 74;
                                if (tx >= tx0 && tx <= tx0 + 70) {
                                    touch_suppress_click_until_ms = now + 260;
                                    if (i != os_viewed_model) {
                                        int dir_x = (i > os_viewed_model) ? 150 : -150;
                                        os_viewed_model = i;
                                        f_refresh_models_page_dir(dir_x);
                                    }
                                    break;
                                }
                            }
                        } else if (tx >= 44 && tx <= 254 && ty >= 52 && ty < 178) {
                            touch_suppress_click_until_ms = now + 260;
                            f_open_model_confirm_modal(os_viewed_model);
                        } else if (tx > 254 && ty >= 52 && ty < 178) {
                            touch_suppress_click_until_ms = now + 260;
                            os_viewed_model = (os_viewed_model + 1) % 4;
                            f_refresh_models_page_dir(150);
                        }
                    }
                } else if (figma_page != FIGMA_PAGE_HOME && figma_page != FIGMA_PAGE_SLEEP_SAVER) {
                    int tx = touch_best_x;
                    int ty = touch_best_y;
                    int max_bx = (figma_page == FIGMA_PAGE_QUICKSET_MENU ||
                                  figma_page == FIGMA_PAGE_SETTINGS_MENU ||
                                  figma_page == FIGMA_PAGE_ABOUT) ? 52 : 68;
                    int max_by = (figma_page == FIGMA_PAGE_MODELS) ? 34 : 46;
                    if ((touch_start_x <= max_bx && touch_start_y <= max_by) ||
                        (tx <= max_bx && ty <= max_by) ||
                        (touch_last_x <= max_bx && touch_last_y <= max_by)) {
                        ESP_LOGI("TOUCH", "TOP-LEFT BACK BUTTON (<) TAPPED on page=%u", figma_page);
                        touch_suppress_click_until_ms = now + 260;
                        f_navigate_back();
                    } else if ((figma_page == FIGMA_PAGE_QUICKSET_MENU ||
                                figma_page == FIGMA_PAGE_SETTINGS_MENU ||
                                figma_page == FIGMA_PAGE_ABOUT) &&
                               ty <= 32 && tx >= 52 && tx <= 252) {
                        static const unsigned tab_p[3] = {
                            FIGMA_PAGE_QUICKSET_MENU, FIGMA_PAGE_SETTINGS_MENU, FIGMA_PAGE_ABOUT
                        };
                        int t_idx = (tx - 48) / 68;
                        if (t_idx >= 0 && t_idx < 3 && tab_p[t_idx] != figma_page) {
                            touch_suppress_click_until_ms = now + 260;
                            active_non_sleep_page = tab_p[t_idx];
                            f_open_page(tab_p[t_idx]);
                        }
                    } else if (figma_page == FIGMA_PAGE_QUICKSET_MENU && ty >= 34 && ty <= 228) {
                        int col = (tx >= 160) ? 1 : 0;
                        int row = (ty - 34) / 64;
                        if (row >= 0 && row < 3) {
                            touch_suppress_click_until_ms = now + 260;
                            f_quickset_handle_card(row * 2 + col);
                        }
                    } else if (figma_page == FIGMA_PAGE_SETTINGS_MENU && ty >= 34 && ty <= 228) {
                        int col = (tx >= 160) ? 1 : 0;
                        int row = (ty - 34) / 64;
                        if (row >= 0 && row < 3) {
                            touch_suppress_click_until_ms = now + 260;
                            f_settings_handle_card(row * 2 + col);
                        }
                    } else if (figma_page == FIGMA_PAGE_STICK_CAL && ty >= 180 && ty <= 234) {
                        touch_suppress_click_until_ms = now + 260;
                        if (tx < 160) f_stick_cal_toggle();
                        else f_stick_cal_reset();
                    } else if (figma_page == FIGMA_PAGE_SCR_SAVER) {
                        if (ty >= 54 && ty <= 104 && tx >= 16 && tx <= 306) {
                            int idx = (tx - 18) / 58;
                            if (idx >= 0 && idx < 5) {
                                touch_suppress_click_until_ms = now + 260;
                                f_scr_saver_select(idx);
                            }
                        } else if (ty >= 176 && ty <= 228) {
                            touch_suppress_click_until_ms = now + 260;
                            f_scr_saver_select(5);
                        }
                    } else if (figma_page == FIGMA_PAGE_QUICK_SETTINGS) {
                        touch_suppress_click_until_ms = now + 260;
                        if (tx >= 256 && ty <= 58) {
                            f_navigate_back();
                        } else if (ty >= 56 && ty <= 98 && tx >= 20 && tx <= 300) {
                            int pct = ((tx - 32) * 100) / 256;
                            if (pct < 10) pct = 10;
                            if (pct > 100) pct = 100;
                            os_brightness_pct = ((pct + 2) / 5) * 5;
                            os_config_save();
                            f_invalidate_page(FIGMA_PAGE_QUICK_SETTINGS);
                            f_open_page(FIGMA_PAGE_QUICK_SETTINGS);
                        } else if (ty >= 100 && ty <= 142 && tx >= 20 && tx <= 300) {
                            int pct = ((tx - 32) * 100) / 256;
                            if (pct < 0) pct = 0;
                            if (pct > 100) pct = 100;
                            os_volume_pct = ((pct + 2) / 5) * 5;
                            os_config_save();
                            f_invalidate_page(FIGMA_PAGE_QUICK_SETTINGS);
                            f_open_page(FIGMA_PAGE_QUICK_SETTINGS);
                        } else if (ty >= 144 && ty <= 194) {
                            if (tx < 160) os_bt_enabled = !os_bt_enabled;
                            else os_touch_locked = !os_touch_locked;
                            os_config_save();
                            f_invalidate_page(FIGMA_PAGE_QUICK_SETTINGS);
                            f_open_page(FIGMA_PAGE_QUICK_SETTINGS);
                        } else if (ty >= 196 && ty <= 232) {
                            active_non_sleep_page = FIGMA_PAGE_SETTINGS_MENU;
                            f_open_page(FIGMA_PAGE_SETTINGS_MENU);
                        }
                    } else if (figma_page == FIGMA_PAGE_TX_POWER && ty >= 42 && ty <= 232) {
                        int row = (ty - 42) / 46;
                        if (row >= 0 && row < 4) {
                            touch_suppress_click_until_ms = now + 260;
                            f_tx_power_select(row);
                        }
                    } else if (figma_page == FIGMA_PAGE_ELRS_SET && ty >= 46 && ty <= 220) {
                        int row = (ty - 46) / 56;
                        if (row >= 0 && row < 3) {
                            touch_suppress_click_until_ms = now + 260;
                            f_elrs_handle_row(row);
                        }
                    } else if (figma_page == FIGMA_PAGE_RF_OUTPUT && ty >= 46 && ty <= 220) {
                        int row = (ty - 46) / 56;
                        if (row >= 0 && row < 3) {
                            touch_suppress_click_until_ms = now + 260;
                            f_rf_output_handle_row(row);
                        }
                    } else if (figma_page == FIGMA_PAGE_FIND && tx >= 186 && ty >= 168 && ty <= 226) {
                        touch_suppress_click_until_ms = now + 260;
                        f_find_toggle();
                    } else if (figma_page == FIGMA_PAGE_MODEL_LAYOUT && ty >= 190 && ty <= 238) {
                        touch_suppress_click_until_ms = now + 260;
                        if (tx < 160) {
                            active_non_sleep_page = FIGMA_PAGE_STICK_MODE;
                            f_open_page(FIGMA_PAGE_STICK_MODE);
                        } else {
                            nav_return_page = FIGMA_PAGE_MODEL_LAYOUT;
                            active_non_sleep_page = FIGMA_PAGE_CH_VIEW;
                            f_open_page(FIGMA_PAGE_CH_VIEW);
                        }
                    } else if (figma_page == FIGMA_PAGE_BIND && tx >= 186 && ty >= 168 && ty <= 226) {
                        touch_suppress_click_until_ms = now + 260;
                        f_bind_toggle();
                    }
                }
            }
            touch_is_swiping = 0;
            touch_drag_frames = 0;
            g_gesture.locked_axis = TYVOR_AXIS_NONE;
            g_gesture.active_dir  = TYVOR_SWIPE_NONE;
        }
        data->point.x = touch_last_x;
        data->point.y = touch_last_y;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* ============================================================================
 * INTER-MCU UART1 BRIDGE (GPIO17 / GPIO18 <-> STM32 PA2 / PA3)
 * ========================================================================== */
static int receive_ui(void *ctx, const uint8_t *b, size_t n, uint64_t now) {
    return tyvor_ui_endpoint_feed(ctx, b, n, now);
}

static bool init_uart(void) {
    uart_config_t conf = {
        .baud_rate = 460800,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(BRIDGE_UART, &conf));
    ESP_ERROR_CHECK(uart_set_pin(BRIDGE_UART, 17, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(BRIDGE_UART, 2048, 2048, 16, &uart_events, 0));
    gpio_set_pull_mode(18, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(17, GPIO_PULLUP_ONLY);
    return true;
}

static int tx_start(void *ctx, const uint8_t *bytes, uint16_t n) {
    (void)ctx;
    if (!uart_ready || !bytes || !n) return -1;
    int written = uart_write_bytes(BRIDGE_UART, (const char *)bytes, (size_t)n);
    return (written >= 0) ? 0 : -1;
}

static int tx_state(void *ctx) {
    (void)ctx;
    return 1; /* Transferred to ESP-IDF interrupt-driven UART TX ring buffer */
}

static void tx_abort(void *ctx) {
    (void)ctx;
}

static int write_uart(void *ctx, const uint8_t *bytes, uint16_t n) {
    (void)ctx;
    if (!uart_ready) return -1;
    return tyvor_async_tx_write(&tx_queue, bytes, n, millis());
}

static uint64_t fresh_boot_id(void) {
    uint64_t id = 0;
    bootloader_random_enable();
    for (unsigned i = 0; i < 4 && !id; ++i) esp_fill_random(&id, sizeof(id));
    bootloader_random_disable();
    if (!id) id = UINT64_C(0x5459564F52533355);
    return id;
}

static void service_uart_bridge(void) {
    if (!uart_ready) return;
    uint64_t now = millis();
    tyvor_async_tx_poll(&tx_queue, now);
    if (tx_queue.busy) tyvor_async_tx_poll(&tx_queue, now);

    if (uxQueueMessagesWaiting(uart_events) >= 16) {
        tyvor_bench_transport_error(&uart_transport, &ui_endpoint.link, 1);
        (void)uart_flush_input(BRIDGE_UART);
        xQueueReset(uart_events);
    }
    uart_event_t event;
    for (unsigned i = 0; i < 16 && xQueueReceive(uart_events, &event, 0) == pdTRUE; ++i) {
        bool overflow = event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL;
        if (overflow || event.type == UART_FRAME_ERR || event.type == UART_PARITY_ERR || event.type == UART_BREAK) {
            tyvor_bench_transport_error(&uart_transport, &ui_endpoint.link, overflow);
            (void)uart_flush_input(BRIDGE_UART);
            xQueueReset(uart_events);
            break;
        }
    }
    for (unsigned chunk = 0; chunk < 8; ++chunk) {
        uint8_t bytes[64];
        int n = uart_read_bytes(BRIDGE_UART, bytes, sizeof(bytes), 0);
        if (n < 0) {
            tyvor_bench_transport_error(&uart_transport, &ui_endpoint.link, 0);
            break;
        }
        if (!n) break;
        (void)tyvor_bench_transport_receive(&uart_transport, bytes, (size_t)n, millis(), receive_ui, &ui_endpoint);
    }
    now = millis();
    tyvor_ui_endpoint_poll(&ui_endpoint, now);
    tyvor_async_tx_poll(&tx_queue, now);
    if (tx_queue.busy) tyvor_async_tx_poll(&tx_queue, now);
}

/* ============================================================================
 * OFFICIAL TYVOR BOOT SPLASH (220x153 RGB565 WITH PREDATORY EYE BLINK)
 * ========================================================================== */
typedef struct {
    uint16_t rgb[TYVOR_BOOT_LOGO_W * TYVOR_BOOT_LOGO_H];
    uint8_t  alpha[TYVOR_BOOT_LOGO_W * TYVOR_BOOT_LOGO_H];
} splash_rgb565a8_t;

static splash_rgb565a8_t splash_live_img;
#define splash_live_buf (splash_live_img.rgb)

static const lv_image_dsc_t tyvor_boot_logo_dsc = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB565A8,
        .flags = 0,
        .w = TYVOR_BOOT_LOGO_W,
        .h = TYVOR_BOOT_LOGO_H,
        .stride = TYVOR_BOOT_LOGO_W * 2,
        .reserved_2 = 0,
    },
    .data_size = sizeof(splash_live_img),
    .data = (const uint8_t *)&splash_live_img,
};

static void set_boot_eye_blink_stage(int st) {
    memcpy(splash_live_buf, tyvor_boot_logo_base, sizeof(splash_live_buf));
    /* Clear faint unreadable bottom text rows (y >= 135) so crisp LVGL "by SAOLATEK" renders cleanly */
    for (int y = 135; y < TYVOR_BOOT_LOGO_H; ++y) {
        for (int x = 0; x < TYVOR_BOOT_LOGO_W; ++x) {
            splash_live_buf[y * TYVOR_BOOT_LOGO_W + x] = 0x0083u;
        }
    }

    if (st == 1) {
        /* Top 45% of full eye socket closes cleanly */
        for (size_t i = 0; i < TYVOR_BLINK_ST3_COUNT; ++i) {
            uint16_t idx = tyvor_eye_blink_stage_3[i].idx;
            int y = (int)(idx / TYVOR_BOOT_LOGO_W);
            if (y <= 46) splash_live_buf[idx] = 0x0083u;
        }
    } else if (st == 2) {
        /* Top 78% of full eye socket closes cleanly */
        for (size_t i = 0; i < TYVOR_BLINK_ST3_COUNT; ++i) {
            uint16_t idx = tyvor_eye_blink_stage_3[i].idx;
            int y = (int)(idx / TYVOR_BOOT_LOGO_W);
            if (y <= 52) splash_live_buf[idx] = 0x0083u;
        }
    } else if (st == 3) {
        /* 100% of the entire black eye socket closes completely */
        for (size_t i = 0; i < TYVOR_BLINK_ST3_COUNT; ++i) {
            splash_live_buf[tyvor_eye_blink_stage_3[i].idx] = 0x0083u;
        }
    } else if (st == 4) {
        /* ANGRY: Fierce 3-tone fiery iris (crimson rim, blazing orange, glowing yellow-white core) + sharp V-cut */
        for (size_t i = 0; i < TYVOR_BLINK_ST3_COUNT; ++i) {
            uint16_t idx = tyvor_eye_blink_stage_3[i].idx;
            int x = (int)(idx % TYVOR_BOOT_LOGO_W);
            int y = (int)(idx / TYVOR_BOOT_LOGO_W);
            int dx_in = (x < 110) ? (x - 62) : (157 - x);
            int y_cut = 41 + (dx_in * 2) / 5;
            if (y < y_cut || y > 55) {
                splash_live_buf[idx] = 0x0083u;
            } else {
                int cx = (x < 110) ? 74 : 145;
                int dist2 = (x - cx) * (x - cx) + (y - 49) * (y - 49);
                if (dist2 <= 6) splash_live_buf[idx] = 0xFFE0u;       /* Glowing yellow core */
                else if (dist2 <= 20) splash_live_buf[idx] = 0xFA20u; /* Blazing orange-red */
                else splash_live_buf[idx] = 0xC800u;                  /* Deep crimson rim */
            }
        }
    } else if (st == 5) {
        /* HAPPY (^ ^): Thick 2-tone glowing cyan-white upward arches + soft coral blush at lower outer corner */
        for (size_t i = 0; i < TYVOR_BLINK_ST3_COUNT; ++i) {
            uint16_t idx = tyvor_eye_blink_stage_3[i].idx;
            int x = (int)(idx % TYVOR_BOOT_LOGO_W);
            int y = (int)(idx / TYVOR_BOOT_LOGO_W);
            int cx = (x < 110) ? 74 : 145;
            int dx = x - cx;
            int dx_out = (x < 110) ? (x - 62) : (157 - x);
            int y_arc = 43 + (dx * dx) / 14;
            if (y >= y_arc && y <= y_arc + 3) {
                splash_live_buf[idx] = (y == y_arc + 1 || y == y_arc + 2) ? 0x87FFu : 0x07FFu;
            } else if (y >= 54 && dx_out <= 6) {
                splash_live_buf[idx] = 0xFB56u; /* Subtle warm blush glint */
            } else {
                splash_live_buf[idx] = 0x0083u;
            }
        }
    } else if (st == 6) {
        /* SAD: Glistening watery sky-blue eyes with dual specular catchlights + pooled cyan tear rim */
        for (size_t i = 0; i < TYVOR_BLINK_ST3_COUNT; ++i) {
            uint16_t idx = tyvor_eye_blink_stage_3[i].idx;
            int x = (int)(idx % TYVOR_BOOT_LOGO_W);
            int y = (int)(idx / TYVOR_BOOT_LOGO_W);
            int dx_in = (x < 110) ? (x - 62) : (157 - x);
            int y_cut = 48 - (dx_in / 3);
            if (y < y_cut) {
                splash_live_buf[idx] = 0x0083u;
            } else {
                int cx = (x < 110) ? 74 : 145;
                if ((x == cx - 3 || x == cx - 2) && (y == 48 || y == 49)) {
                    splash_live_buf[idx] = 0xFFFFu; /* Main watery catchlight */
                } else if (x == cx + 3 && y == 52) {
                    splash_live_buf[idx] = 0xBFFFu; /* Secondary tiny glint */
                } else if (y >= 54) {
                    splash_live_buf[idx] = 0x07FFu; /* Pooled tear shimmer */
                } else if (y >= 51) {
                    splash_live_buf[idx] = 0x5EBFu; /* Bright lower iris */
                } else {
                    splash_live_buf[idx] = 0x249Du; /* Deep sad blue */
                }
            }
        }
    } else if (st == 7) {
        /* WINK: Left eye open, right eye cheerful happy arch */
        for (size_t i = 0; i < TYVOR_BLINK_ST3_COUNT; ++i) {
            uint16_t idx = tyvor_eye_blink_stage_3[i].idx;
            int x = (int)(idx % TYVOR_BOOT_LOGO_W);
            if (x >= 110) {
                int y = (int)(idx / TYVOR_BOOT_LOGO_W);
                int dx = x - 145;
                int y_arc = 45 + (dx * dx) / 16;
                splash_live_buf[idx] = (y >= y_arc && y <= y_arc + 2) ? 0x07FFu : 0x0083u;
            }
        }
    } else if (st == 8) {
        /* SLEEP (◡ ◡): Strictly inside eye sockets — calm downward-curved closed eyes */
        for (size_t i = 0; i < TYVOR_BLINK_ST3_COUNT; ++i) {
            uint16_t idx = tyvor_eye_blink_stage_3[i].idx;
            int x = (int)(idx % TYVOR_BOOT_LOGO_W);
            int y = (int)(idx / TYVOR_BOOT_LOGO_W);
            int cx = (x < 110) ? 74 : 145;
            int dx = x - cx;
            int y_arc = 52 - (dx * dx) / 18;
            if (y >= y_arc && y <= y_arc + 2) {
                splash_live_buf[idx] = (y == y_arc + 1) ? 0x5EBFu : 0x2B5Cu;
            } else {
                splash_live_buf[idx] = 0x0083u;
            }
        }
    }

    /* Key out 0x0083 background pixels to alpha=0 so zero rectangular box shows behind logo */
    for (size_t i = 0; i < (size_t)(TYVOR_BOOT_LOGO_W * TYVOR_BOOT_LOGO_H); ++i) {
        splash_live_img.alpha[i] = (splash_live_buf[i] == 0x0083u) ? 0u : 255u;
    }
}

static void run_tyvor_boot_splash(void) {
    set_boot_eye_blink_stage(0);

    lv_obj_t *splash = lv_obj_create(oxbot_ui.root);
    lv_obj_remove_style_all(splash);
    lv_obj_set_pos(splash, 0, 0);
    lv_obj_set_size(splash, 320, 240);
    lv_obj_set_style_bg_color(splash, lv_color_hex(0x07101E), 0);
    lv_obj_set_style_bg_opa(splash, LV_OPA_COVER, 0);
    lv_obj_remove_flag(splash, LV_OBJ_FLAG_SCROLLABLE);

    /* Centered Official TYVOR Brand Logo Asset (220x153) */
    int logo_x = (320 - TYVOR_BOOT_LOGO_W) / 2; /* 50 */
    int logo_y = (240 - TYVOR_BOOT_LOGO_H) / 2; /* 43 */
    lv_obj_t *logo_img = lv_image_create(splash);
    lv_obj_remove_style_all(logo_img);
    lv_image_set_src(logo_img, &tyvor_boot_logo_dsc);
    lv_obj_set_pos(logo_img, logo_x, logo_y);
    lv_obj_set_size(logo_img, TYVOR_BOOT_LOGO_W, TYVOR_BOOT_LOGO_H);

    lv_obj_t *s_brand = lv_label_create(splash);
    lv_label_set_text(s_brand, "by SAOLATEK");
    lv_obj_set_pos(s_brand, 0, 207);
    lv_obj_set_width(s_brand, 320);
    lv_obj_set_style_text_font(s_brand, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_brand, lv_color_hex(0xCBD5E1), 0);
    lv_obj_set_style_text_align(s_brand, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(s_brand, 3, 0);

    /* Natural biological eye-blinking sequence:
     * 0 = Eyes Open, 1 = Eyelids closing, 2 = Intense squint slit, 3 = Eyes closed
     */
    static const uint8_t blink_timeline[] = {
        0, 0, 0, 0, 0,
        1, 2, 3, 2, 1, 0,       /* Blink 1 */
        0, 0, 0,
        1, 2, 3, 3, 2, 1, 0,    /* Blink 2 (deep blink) */
        0, 0,
        1, 2, 3, 2, 1, 0,       /* Quick double-blink */
        1, 2, 1, 0,
        0, 0, 0, 0
    };
    const size_t n_steps = sizeof(blink_timeline) / sizeof(blink_timeline[0]);
    int prev_st = -1;

    for (size_t s = 0; s < n_steps; ++s) {
        int st = blink_timeline[s];
        if (st != prev_st) {
            prev_st = st;
            set_boot_eye_blink_stage(st);
            lv_obj_invalidate(logo_img);
            (void)lv_timer_handler();
            tyvor_ui321_frame(&oxbot_ui);
        }

        int delay_ms = (st == 0) ? 120 : 60;
        for (int t = 0; t < delay_ms; t += 10) {
            service_uart_bridge();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    lv_obj_delete(splash);
    lv_obj_invalidate(oxbot_ui.root);
}

/* ============================================================================
 * FIGMA DESIGN SYSTEM -> TYVOR OS (320x240)
 * Replaces SAOLATEK with TYVOR + Implements all Figma screens & Sleep Icons
 * ========================================================================== */
static lv_obj_t *figma_root;
static uint64_t boot_epoch_ms = 0;

/* Live widget pointers for fast 60Hz updates (per-page header widgets so cached pages stay valid) */
static unsigned g_building_page = 0;
static lv_obj_t *w_hdr_sig_bars[FIGMA_PAGE_COUNT][4];
static lv_obj_t *w_hdr_sig_x[FIGMA_PAGE_COUNT];
static lv_obj_t *w_hdr_bat_fill[FIGMA_PAGE_COUNT];
static lv_obj_t *w_hdr_power_lbl[FIGMA_PAGE_COUNT];

/* Home page live widgets */
static lv_obj_t *w_home_arm_dot;
static lv_obj_t *w_home_arm_lbl;
static lv_obj_t *w_home_timer_lbl;
static lv_obj_t *w_home_sig_lbl;
static lv_obj_t *w_home_volt_lbl;
static lv_obj_t *w_home_bat_bar;
static lv_obj_t *w_home_mode_lbl;
static lv_obj_t *w_home_gps_val;
static lv_obj_t *w_home_gps_arc;
static lv_obj_t *w_home_trim_ticks[4];

/* Models page live widgets */
static lv_obj_t *w_mod_title_lbl;
static lv_obj_t *w_mod_tabs[4];
static lv_obj_t *w_mod_prev_mode;
static lv_obj_t *w_mod_confirm_overlay;
static lv_obj_t *w_mod_modal_card;
static lv_obj_t *w_mod_modal_name;
static lv_obj_t *w_mod_modal_pwr;
static lv_obj_t *w_mod_modal_volt;
static lv_obj_t *w_mod_modal_mode;
static lv_obj_t *w_mod_modal_pills[4];

/* CH View & Channels live widgets */
static lv_obj_t *w_chv_pct[10];
static lv_obj_t *w_chv_fill[10];
static lv_obj_t *w_chv_tick[10];
static lv_obj_t *w_chc_pct[6];
static lv_obj_t *w_chc_fill[6];

/* Telemetry live widgets (23 Figma fields) */
static lv_obj_t *w_tel_vp = NULL;
static lv_obj_t *w_tel_val[23];
static lv_obj_t *w_about_vals[4];
static lv_obj_t *w_cal_pct[4];
static lv_obj_t *w_cal_range[4];
static lv_obj_t *w_cal_fill[4];
static int os_stick_cal_active = 0;
static int32_t os_cal_live_min[4] = {2048, 2048, 2048, 2048};
static int32_t os_cal_live_max[4] = {2048, 2048, 2048, 2048};

/* Sleep / Screen Saver live widgets (Official TYVOR Logo + Owl Face) */
static lv_obj_t *w_thr_warn_overlay = NULL;
static lv_obj_t *w_thr_warn_bar = NULL;
static lv_obj_t *w_thr_warn_pct = NULL;
static int thr_warn_dismissed = 0;
static lv_obj_t *w_sleep_card;
static lv_obj_t *w_sleep_owl_img;
static lv_obj_t *w_sleep_zzz[3];
static lv_obj_t *w_sleep_happy[3];
static lv_obj_t *w_sleep_sad[2];
static lv_obj_t *w_sleep_angry[4];
static int last_sleep_blink_stage = -1;
static int last_sleep_zzz_step = -1;
static int last_sleep_mood_tick = -1;

/* ============================================================================
 * REAL TRANSMITTER OS — HARDWARE GIMBAL ADC ENGINE (4 AXES + 2 SWITCHES)
 * ============================================================================
 * Analog Gimbal Inputs (ESP32-S3 ADC1, 12-bit 0..4095, 0V..3.3V):
 *  - CH1 (Roll / Aileron)   -> GPIO 1  (ADC1_CH0)
 *  - CH2 (Pitch / Elevator) -> GPIO 2  (ADC1_CH1)
 *  - CH3 (Throttle / Ga)    -> GPIO 3  (ADC1_CH2)
 *  - CH4 (Yaw / Rudder)     -> GPIO 4  (ADC1_CH3)
 *
 * Digital Auxiliary Switches (Internal Pull-Up, connect to GND to activate):
 *  - CH5 (Switch SA / ARM)  -> GPIO 38
 *  - CH6 (Switch SB / MODE) -> GPIO 39
 *
 * Bonus Auxiliary Gimbal Power Pins (right next to GPIO 1 & 2 on left header):
 *  - GPIO 42 -> 3.3V Output (HIGH, max drive strength for gimbal VCC)
 *  - GPIO 41 -> GND  Output (LOW, ground return for gimbal GND)
 * ========================================================================== */
#define GIMBAL_SW_SA_GPIO   38
#define GIMBAL_SW_SB_GPIO   39
#define GIMBAL_VCC_AUX_GPIO 42
#define GIMBAL_GND_AUX_GPIO 41

static const int gimbal_gpio_map[4] = {1, 2, 3, 4};
static const adc_channel_t gimbal_adc_ch[4] = {
    ADC_CHANNEL_0, /* GPIO 1 -> CH1 Roll */
    ADC_CHANNEL_1, /* GPIO 2 -> CH2 Pitch */
    ADC_CHANNEL_2, /* GPIO 3 -> CH3 Throttle */
    ADC_CHANNEL_3, /* GPIO 4 -> CH4 Yaw */
};

static adc_oneshot_unit_handle_t adc1_handle = NULL;
static uint8_t gimbal_connected[4] = {0, 0, 0, 0};
static int32_t gimbal_adc_filt[4] = {2048, 2048, 0, 2048};
static int16_t gimbal_raw_val[6] = {0, 0, -1000, 0, -1000, -1000}; /* -1000..+1000 */
static int16_t gimbal_prev_val[4] = {0, 0, -1000, 0};
static int32_t gimbal_min_cal[4] = {350, 350, 350, 350};
static int32_t gimbal_max_cal[4] = {3745, 3745, 3745, 3745};
static uint64_t next_gimbal_ms = 0;
static void f_open_page(unsigned page);

static void init_gimbal_adc(void) {
    /* Provide convenient auxiliary 3.3V (GPIO42) & GND (GPIO41) right next to GPIO 1 & 2 */
    gpio_config_t pwr_io = {
        .pin_bit_mask = (1ULL << GIMBAL_VCC_AUX_GPIO) | (1ULL << GIMBAL_GND_AUX_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pwr_io);
    gpio_set_drive_capability(GIMBAL_VCC_AUX_GPIO, GPIO_DRIVE_CAP_3);
    gpio_set_level(GIMBAL_VCC_AUX_GPIO, 1); /* 3.3V */
    gpio_set_level(GIMBAL_GND_AUX_GPIO, 0); /* GND  */

    /* Configure Switch SA (GPIO38) and Switch SB (GPIO39) with internal Pull-Up */
    gpio_config_t sw_io = {
        .pin_bit_mask = (1ULL << GIMBAL_SW_SA_GPIO) | (1ULL << GIMBAL_SW_SB_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&sw_io);

    /* Initialize ADC1 for 4 Gimbal Channels (GPIO 1, 2, 3, 4) */
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    if (adc_oneshot_new_unit(&init_cfg, &adc1_handle) == ESP_OK) {
        adc_oneshot_chan_cfg_t chan_cfg = {
            .bitwidth = ADC_BITWIDTH_12,
            .atten = ADC_ATTEN_DB_12, /* Full 0V .. 3.3V range */
        };
        for (int i = 0; i < 4; ++i) {
            (void)adc_oneshot_config_channel(adc1_handle, gimbal_adc_ch[i], &chan_cfg);
        }
        ESP_LOGI(TAG, "Gimbal ADC1 ready on GPIO 1(Roll), 2(Pitch), 3(Thr), 4(Yaw)");
    }
}

static void service_gimbal_adc(uint64_t now) {
    if (!adc1_handle || now < next_gimbal_ms) return;
    next_gimbal_ms = now + 12; /* ~83 Hz real-time gimbal sampling */

    for (int i = 0; i < 4; ++i) {
        int gpio = gimbal_gpio_map[i];
        int raw_pu = 4095, raw_pd = 0;

        /* Fast differential pull-up / pull-down impedance probe to detect real gimbal wire */
        gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
        esp_rom_delay_us(8);
        (void)adc_oneshot_read(adc1_handle, gimbal_adc_ch[i], &raw_pu);

        gpio_set_pull_mode(gpio, GPIO_PULLDOWN_ONLY);
        esp_rom_delay_us(8);
        (void)adc_oneshot_read(adc1_handle, gimbal_adc_ch[i], &raw_pd);

        int spread = raw_pu - raw_pd;
        if (spread < 0) spread = -spread;

        /* Real gimbal potentiometer (<10k) or Hall sensor holds pin stiffly (spread < 1600).
         * Unconnected floating pin swings widely from GND to 3.3V (spread > 2200). */
        int is_conn = gimbal_connected[i] ? (spread < 2100) : (spread < 1500);
        if (!is_conn) {
            if (gimbal_connected[i]) {
                ESP_LOGW(TAG, "Gimbal CH%d (GPIO%d) disconnected", i + 1, gpio);
                gimbal_connected[i] = 0;
            }
            /* Keep unconnected pin pulled down cleanly; set safe neutral value */
            gimbal_raw_val[i] = (i == 2) ? -1000 : 0;
            continue;
        }

        /* Gimbal is physically connected! Switch pin to pure high-Z floating analog mode */
        gpio_set_pull_mode(gpio, GPIO_FLOATING);
        esp_rom_delay_us(10);

        int32_t sum = 0;
        for (int s = 0; s < 8; ++s) {
            int val = 0;
            (void)adc_oneshot_read(adc1_handle, gimbal_adc_ch[i], &val);
            sum += val;
        }
        int32_t avg = sum / 8;

        if (!gimbal_connected[i]) {
            gimbal_connected[i] = 1;
            gimbal_adc_filt[i] = avg;
            ESP_LOGI(TAG, "Gimbal CH%d (GPIO%d) CONNECTED! raw=%d", i + 1, gpio, (int)avg);
            last_user_activity_ms = now;
            if (sleep_active) {
                sleep_active = 0;
                f_open_page(active_non_sleep_page);
            }
        } else {
            /* Responsive EMA filter (alpha = 3/8) */
            gimbal_adc_filt[i] = (gimbal_adc_filt[i] * 5 + avg * 3) / 8;
        }

        /* Auto-expanding endpoint calibration (handles both 60-deg RC gimbals and 270-deg pots) */
        int32_t f = gimbal_adc_filt[i];
        if (f < gimbal_min_cal[i]) {
            gimbal_min_cal[i] = (f < 40) ? 40 : f;
        }
        if (f > gimbal_max_cal[i]) {
            gimbal_max_cal[i] = (f > 4055) ? 4055 : f;
        }

        int32_t center = 2048;
        int32_t norm = 0;
        if (f < center - 24) {
            int32_t span = (center - 24) - gimbal_min_cal[i];
            if (span < 100) span = 100;
            norm = -(((center - 24) - f) * 1000) / span;
        } else if (f > center + 24) {
            int32_t span = gimbal_max_cal[i] - (center + 24);
            if (span < 100) span = 100;
            norm = ((f - (center + 24)) * 1000) / span;
        }
        if (norm < -1000) norm = -1000;
        if (norm > 1000) norm = 1000;
        gimbal_raw_val[i] = (int16_t)norm;

        /* Detect stick movement >= 3.5% to wake from sleep & reset idle timer */
        int16_t diff = gimbal_raw_val[i] - gimbal_prev_val[i];
        if (diff < 0) diff = -diff;
        if (diff >= 35) {
            gimbal_prev_val[i] = gimbal_raw_val[i];
            last_user_activity_ms = now;
            if (sleep_active) {
                sleep_active = 0;
                f_open_page(active_non_sleep_page);
            }
        }
    }

    /* Read Digital Switches SA (GPIO38) and SB (GPIO39) */
    int sa_on = (gpio_get_level(GIMBAL_SW_SA_GPIO) == 0);
    int sb_on = (gpio_get_level(GIMBAL_SW_SB_GPIO) == 0);
    gimbal_raw_val[4] = sa_on ? 1000 : -1000;
    gimbal_raw_val[5] = sb_on ? 1000 : -1000;
}

static int any_local_gimbal_connected(void) {
    return gimbal_connected[0] || gimbal_connected[1] ||
           gimbal_connected[2] || gimbal_connected[3];
}

static int16_t clamp_ch(int32_t v) {
    if (v < -1000) return -1000;
    if (v > 1000) return 1000;
    return (int16_t)v;
}

static int16_t get_live_ch_raw(int ch, const tyvor_core_snapshot_t *snap, int link_ok) {
    (void)snap;
    (void)link_ok;
    /* Raw physical gimbal axes (Mode 2 default: 0=Roll, 1=Pitch, 2=Thr, 3=Yaw) */
    int16_t rol = gimbal_connected[0] ? gimbal_raw_val[0] : 0;
    int16_t pit = gimbal_connected[1] ? gimbal_raw_val[1] : 0;
    int16_t thr = gimbal_connected[2] ? gimbal_raw_val[2] : -1000;
    int16_t yaw = gimbal_connected[3] ? gimbal_raw_val[3] : 0;

    /* Stick Mode 1 swaps Pitch (CH2) and Throttle (CH3) physical inputs */
    if (os_stick_mode == 1) {
        int16_t tmp = pit;
        pit = thr;
        thr = tmp;
    }

    int16_t out[6];
    out[0] = rol;
    out[1] = pit;
    out[2] = thr;
    out[3] = yaw;
    out[4] = gimbal_raw_val[4];
    out[5] = gimbal_raw_val[5];

    /* Apply active Model Mixer (CRU / QUAD / FIXED Elevon / VTAIL) */
    if (os_active_model == 2) {
        /* FIXED Wing: Elevon Mixer (CH1 = AIL+ELE, CH2 = AIL-ELE) */
        out[0] = clamp_ch(((int32_t)rol + (int32_t)pit) / 2);
        out[1] = clamp_ch(((int32_t)rol - (int32_t)pit) / 2);
    } else if (os_active_model == 3) {
        /* VTAIL Glider: V-Tail Mixer (CH2 = ELE+RUD, CH4 = ELE-RUD) */
        out[1] = clamp_ch(((int32_t)pit + (int32_t)yaw) / 2);
        out[3] = clamp_ch(((int32_t)pit - (int32_t)yaw) / 2);
    }

    if (ch >= 0 && ch < 6) {
        return out[ch];
    }
    return 0;
}

static void reset_page_widgets(unsigned p) {
    if (p >= FIGMA_PAGE_COUNT) return;
    memset(w_hdr_sig_bars[p], 0, sizeof(w_hdr_sig_bars[p]));
    w_hdr_sig_x[p] = NULL;
    w_hdr_bat_fill[p] = NULL;
    w_hdr_power_lbl[p] = NULL;

    if (p == FIGMA_PAGE_HOME) {
        w_home_arm_dot = NULL;
        w_home_arm_lbl = NULL;
        w_home_timer_lbl = NULL;
        w_home_sig_lbl = NULL;
        w_home_volt_lbl = NULL;
        w_home_bat_bar = NULL;
        w_home_mode_lbl = NULL;
        w_home_gps_val = NULL;
        w_home_gps_arc = NULL;
        memset(w_home_trim_ticks, 0, sizeof(w_home_trim_ticks));
    } else if (p == FIGMA_PAGE_MODELS) {
        w_mod_track = NULL;
        w_mod_title_lbl = NULL;
        memset(w_mod_tabs, 0, sizeof(w_mod_tabs));
        w_mod_prev_mode = NULL;
        w_mod_confirm_overlay = NULL;
        w_mod_modal_card = NULL;
        w_mod_modal_name = NULL;
        w_mod_modal_pwr = NULL;
        w_mod_modal_volt = NULL;
        w_mod_modal_mode = NULL;
        memset(w_mod_modal_pills, 0, sizeof(w_mod_modal_pills));
        os_model_modal_open = 0;
    } else if (p == FIGMA_PAGE_QUICK_SETTINGS) {
        w_qs_br_fill = NULL;
        w_qs_br_knob = NULL;
        w_qs_br_lbl = NULL;
        w_qs_vol_fill = NULL;
        w_qs_vol_knob = NULL;
        w_qs_vol_lbl = NULL;
    } else if (p == FIGMA_PAGE_CH_VIEW) {
        memset(w_chv_pct, 0, sizeof(w_chv_pct));
        memset(w_chv_fill, 0, sizeof(w_chv_fill));
        memset(w_chv_tick, 0, sizeof(w_chv_tick));
    } else if (p == FIGMA_PAGE_CHANNELS) {
        memset(w_chc_pct, 0, sizeof(w_chc_pct));
        memset(w_chc_fill, 0, sizeof(w_chc_fill));
    } else if (p == FIGMA_PAGE_TELEMETRY) {
        w_tel_vp = NULL;
        memset(w_tel_val, 0, sizeof(w_tel_val));
    } else if (p == FIGMA_PAGE_STICK_CAL) {
        memset(w_cal_pct, 0, sizeof(w_cal_pct));
        memset(w_cal_range, 0, sizeof(w_cal_range));
        memset(w_cal_fill, 0, sizeof(w_cal_fill));
    } else if (p == FIGMA_PAGE_ABOUT) {
        memset(w_about_vals, 0, sizeof(w_about_vals));
    } else if (p == FIGMA_PAGE_SLEEP_SAVER) {
        w_sleep_card = NULL;
        w_sleep_owl_img = NULL;
        for (int i = 0; i < 3; ++i) { w_sleep_zzz[i] = NULL; w_sleep_happy[i] = NULL; }
        for (int i = 0; i < 2; ++i) w_sleep_sad[i] = NULL;
        for (int i = 0; i < 4; ++i) w_sleep_angry[i] = NULL;
        last_sleep_blink_stage = -1;
        last_sleep_zzz_step = -1;
        last_sleep_mood_tick = -1;
    }
}

static void f_invalidate_page(unsigned page) {
    if (page < FIGMA_PAGE_COUNT) {
        page_built[page] = 0;
    }
}

static void f_invalidate_all_pages(void) {
    memset(page_built, 0, sizeof(page_built));
}

static lv_obj_t *f_rect(lv_obj_t *parent, int x, int y, int w, int h,
                        uint32_t bg_hex, lv_opa_t opa, int radius,
                        int border_w, uint32_t border_hex) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg_hex), 0);
    lv_obj_set_style_bg_opa(o, opa, 0);
    lv_obj_set_style_radius(o, radius, 0);
    if (border_w > 0) {
        lv_obj_set_style_border_width(o, border_w, 0);
        lv_obj_set_style_border_color(o, lv_color_hex(border_hex), 0);
        lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);
    }
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *f_label(lv_obj_t *parent, int x, int y, int w,
                         const char *txt, const lv_font_t *font,
                         uint32_t hex_color, lv_text_align_t align) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_pos(l, x, y);
    if (w > 0) {
        lv_obj_set_width(l, w);
        lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    }
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(hex_color), 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_text(l, txt);
    return l;
}

static lv_obj_t *f_icon_centered(lv_obj_t *parent, const char *sym,
                                 const lv_font_t *font, uint32_t hex_color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(hex_color), 0);
    lv_label_set_text(l, sym);
    lv_obj_center(l);
    return l;
}

/* Official TYVOR Logo (replaces SAOLATEK everywhere) */
static void f_tyvor_logo(lv_obj_t *parent, int x, int y, uint32_t color_hex) {
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, &tyvor_ui321_header_compact);
    lv_obj_set_pos(img, x, y);
    lv_obj_set_size(img, (int)tyvor_ui321_header_compact.header.w,
                         (int)tyvor_ui321_header_compact.header.h);
    lv_obj_set_style_image_recolor(img, lv_color_hex(color_hex), 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
}

static void f_open_page_dir(unsigned page, int enter_x, int enter_y);
static void f_open_page(unsigned page);

/* Directional animated page switch (used by Back button & Nav buttons) */
static void tyvor_trans_start_animated(unsigned target_page, tyvor_swipe_dir_t dir) {
    target_page %= FIGMA_PAGE_COUNT;
    int ex = 0, ey = 0;
    if (dir == TYVOR_SWIPE_LEFT) ex = 260;
    else if (dir == TYVOR_SWIPE_RIGHT) ex = -260;
    else if (dir == TYVOR_SWIPE_UP) ey = 200;
    else if (dir == TYVOR_SWIPE_DOWN) ey = -200;
    f_open_page_dir(target_page, ex, ey);
}

static void f_navigate_back(void) {
    last_user_activity_ms = millis();
    sleep_active = 0;

    /* Follow exact Section 3 Reverse Flows:
     *  - Model Selection -> SWIPE_LEFT  -> HOME
     *  - Telemetry       -> SWIPE_RIGHT -> HOME
     *  - Quick Settings  -> SWIPE_UP    -> HOME
     *  - Settings        -> SWIPE_LEFT  -> Quick Settings
     *  - About           -> SWIPE_LEFT  -> Settings
     */
    if (figma_page == FIGMA_PAGE_MODELS) {
        active_non_sleep_page = FIGMA_PAGE_HOME;
        tyvor_trans_start_animated(FIGMA_PAGE_HOME, TYVOR_SWIPE_LEFT);
    } else if (figma_page == FIGMA_PAGE_TELEMETRY) {
        active_non_sleep_page = FIGMA_PAGE_HOME;
        tyvor_trans_start_animated(FIGMA_PAGE_HOME, TYVOR_SWIPE_RIGHT);
    } else if (figma_page == FIGMA_PAGE_QUICKSET_MENU ||
               figma_page == FIGMA_PAGE_QUICK_SETTINGS) {
        active_non_sleep_page = FIGMA_PAGE_HOME;
        tyvor_trans_start_animated(FIGMA_PAGE_HOME, TYVOR_SWIPE_UP);
    } else if (figma_page == FIGMA_PAGE_SETTINGS_MENU) {
        active_non_sleep_page = FIGMA_PAGE_QUICKSET_MENU;
        tyvor_trans_start_animated(FIGMA_PAGE_QUICKSET_MENU, TYVOR_SWIPE_LEFT);
    } else if (figma_page == FIGMA_PAGE_STICK_CAL ||
               figma_page == FIGMA_PAGE_STICK_MODE ||
               figma_page == FIGMA_PAGE_SCR_SAVER) {
        os_stick_cal_active = 0;
        active_non_sleep_page = FIGMA_PAGE_SETTINGS_MENU;
        tyvor_trans_start_animated(FIGMA_PAGE_SETTINGS_MENU, TYVOR_SWIPE_LEFT);
    } else if (figma_page == FIGMA_PAGE_ABOUT) {
        active_non_sleep_page = FIGMA_PAGE_SETTINGS_MENU;
        tyvor_trans_start_animated(FIGMA_PAGE_SETTINGS_MENU, TYVOR_SWIPE_LEFT);
    } else if (figma_page == FIGMA_PAGE_TX_POWER ||
               figma_page == FIGMA_PAGE_ELRS_SET ||
               figma_page == FIGMA_PAGE_RF_OUTPUT) {
        active_non_sleep_page = FIGMA_PAGE_QUICKSET_MENU;
        tyvor_trans_start_animated(FIGMA_PAGE_QUICKSET_MENU, TYVOR_SWIPE_LEFT);
    } else if (figma_page == FIGMA_PAGE_MODEL_LAYOUT) {
        active_non_sleep_page = FIGMA_PAGE_MODELS;
        tyvor_trans_start_animated(FIGMA_PAGE_MODELS, TYVOR_SWIPE_LEFT);
    } else if (figma_page == FIGMA_PAGE_CH_VIEW ||
               figma_page == FIGMA_PAGE_CHANNELS ||
               figma_page == FIGMA_PAGE_FIND ||
               figma_page == FIGMA_PAGE_BIND) {
        unsigned dest = (nav_return_page < FIGMA_PAGE_SLEEP_SAVER &&
                         nav_return_page != figma_page)
                        ? nav_return_page : FIGMA_PAGE_HOME;
        active_non_sleep_page = dest;
        tyvor_trans_start_animated(dest, TYVOR_SWIPE_LEFT);
    } else {
        active_non_sleep_page = FIGMA_PAGE_HOME;
        tyvor_trans_start_animated(FIGMA_PAGE_HOME, TYVOR_SWIPE_LEFT);
    }
}

static int click_allowed(void) {
    uint64_t now = millis();
    if (touch_is_swiping || g_trans.state != TYVOR_TRANS_IDLE || now < touch_suppress_click_until_ms) {
        return 0;
    }
    last_user_activity_ms = now;
    sleep_active = 0;
    return 1;
}

static void on_back_btn_cb(lv_event_t *e) {
    (void)e;
    if (!click_allowed()) return;
    f_navigate_back();
}

static void on_nav_btn_cb(lv_event_t *e) {
    if (!click_allowed()) return;
    unsigned target = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    unsigned cur = figma_page;
    if (target == cur) return;

    if (target == FIGMA_PAGE_MODELS && cur != FIGMA_PAGE_MODELS) {
        os_viewed_model = os_active_model;
        f_invalidate_page(FIGMA_PAGE_MODELS);
    }
    if (target == FIGMA_PAGE_MODEL_LAYOUT) {
        f_invalidate_page(FIGMA_PAGE_MODEL_LAYOUT);
    }
    if (target == FIGMA_PAGE_CH_VIEW || target == FIGMA_PAGE_CHANNELS ||
        target == FIGMA_PAGE_FIND || target == FIGMA_PAGE_BIND) {
        nav_return_page = cur;
        f_invalidate_page(target);
    }

    tyvor_swipe_dir_t dir = TYVOR_SWIPE_RIGHT;
    if (cur == FIGMA_PAGE_HOME) {
        if (target == FIGMA_PAGE_MODELS) dir = TYVOR_SWIPE_RIGHT;
        else if (target == FIGMA_PAGE_TELEMETRY) dir = TYVOR_SWIPE_LEFT;
        else if (target == FIGMA_PAGE_QUICKSET_MENU || target == FIGMA_PAGE_QUICK_SETTINGS) dir = TYVOR_SWIPE_DOWN;
        else dir = TYVOR_SWIPE_RIGHT;
    } else if (cur >= FIGMA_PAGE_QUICKSET_MENU && cur <= FIGMA_PAGE_ABOUT &&
               target >= FIGMA_PAGE_QUICKSET_MENU && target <= FIGMA_PAGE_ABOUT) {
        dir = (target > cur) ? TYVOR_SWIPE_RIGHT : TYVOR_SWIPE_LEFT;
    } else if (target == FIGMA_PAGE_HOME) {
        dir = TYVOR_SWIPE_UP;
    }

    active_non_sleep_page = target;
    tyvor_trans_start_animated(target, dir);
}

static void f_refresh_models_page_dir(int enter_x) {
    f_invalidate_page(FIGMA_PAGE_MODELS);
    f_ensure_page_built(FIGMA_PAGE_MODELS);
    if (enter_x != 0 && w_mod_track) {
        lv_obj_set_x(w_mod_track, MOD_TRACK_BASE_X + enter_x);
    }
    f_open_page(FIGMA_PAGE_MODELS);
    if (enter_x != 0 && w_mod_track) {
        f_animate_mod_track(MOD_TRACK_BASE_X + enter_x, MOD_TRACK_BASE_X, 140);
    }
}

static void f_refresh_models_page(void) {
    f_invalidate_all_pages();
    f_open_page(FIGMA_PAGE_MODELS);
}

static void apply_model_profile(int idx) {
    if (idx < 0 || idx > 3) return;
    os_active_model = idx;
    os_viewed_model = idx;
    /* Sync default power & ELRS rate for this model */
    uint16_t pmw = k_os_models[idx].default_power_mw;
    for (int i = 0; i < 5; ++i) {
        if (k_power_mw_table[i] == pmw) { os_power_idx = i; break; }
    }
    uint16_t rhz = k_os_models[idx].default_rate_hz;
    for (int i = 0; i < 4; ++i) {
        if (k_elrs_rate_table[i] == rhz) { os_elrs_rate_idx = i; break; }
    }
    f_invalidate_all_pages();
    if (ui_endpoint.link.ready) {
        (void)tyvor_ui_endpoint_request(&ui_endpoint, TYVOR_REQ_PAGE, 1);
        ui_endpoint.read_active = 0;
        ui_endpoint.next_read_ms = millis();
    }
}

static void on_model_tab_cb(lv_event_t *e) {
    if (!click_allowed()) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < 4 && idx != os_viewed_model) {
        int dir_x = (idx > os_viewed_model) ? 150 : -150;
        os_viewed_model = idx;
        f_refresh_models_page_dir(dir_x);
    }
}

static void f_sync_model_confirm_modal(int target_idx) {
    if (target_idx < 0 || target_idx > 3) return;
    os_pending_model = target_idx;
    const tyvor_os_model_profile_t *m = &k_os_models[target_idx];
    if (w_mod_modal_name) {
        lv_label_set_text(w_mod_modal_name, m->full_title);
    }
    if (w_mod_modal_pwr) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%umW", (unsigned)m->default_power_mw);
        lv_label_set_text(w_mod_modal_pwr, buf);
    }
    if (w_mod_modal_volt) {
        lv_label_set_text(w_mod_modal_volt, m->volt_str);
    }
    if (w_mod_modal_mode) {
        lv_label_set_text(w_mod_modal_mode, m->mode_name);
        lv_obj_set_style_text_color(w_mod_modal_mode, lv_color_hex(m->badge_hex), 0);
    }
    if (w_mod_modal_card) {
        lv_obj_set_style_border_color(w_mod_modal_card, lv_color_hex(m->theme_hex), 0);
    }
    for (int i = 0; i < 4; ++i) {
        if (!w_mod_modal_pills[i]) continue;
        int sel = (i == target_idx);
        lv_obj_set_style_border_width(w_mod_modal_pills[i], sel ? 2 : 0, 0);
        lv_obj_set_style_border_color(w_mod_modal_pills[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_opa(w_mod_modal_pills[i], sel ? LV_OPA_COVER : 185, 0);
    }
}

static void f_open_model_confirm_modal(int target_idx) {
    if (!w_mod_confirm_overlay) return;
    f_sync_model_confirm_modal(target_idx);
    os_model_modal_open = 1;
    lv_obj_clear_flag(w_mod_confirm_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(w_mod_confirm_overlay);
    lv_obj_invalidate(oxbot_ui.root);
}

static void on_model_activate_cb(lv_event_t *e) {
    (void)e;
    if (!click_allowed()) return;
    f_open_model_confirm_modal(os_viewed_model);
}

static void on_model_pill_cb(lv_event_t *e) {
    if (!click_allowed()) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < 4) {
        f_sync_model_confirm_modal(idx);
    }
}

static void f_close_model_confirm_modal(void) {
    os_model_modal_open = 0;
    if (w_mod_confirm_overlay) {
        lv_obj_add_flag(w_mod_confirm_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_invalidate(oxbot_ui.root);
}

static void f_confirm_switch_model(void) {
    f_close_model_confirm_modal();
    apply_model_profile(os_pending_model);
    os_config_save();
    active_non_sleep_page = FIGMA_PAGE_HOME;
    f_open_page(FIGMA_PAGE_HOME);
}

static void on_model_confirm_cancel_cb(lv_event_t *e) {
    (void)e;
    if (!click_allowed()) return;
    f_close_model_confirm_modal();
}

static void on_model_confirm_switch_cb(lv_event_t *e) {
    (void)e;
    if (!click_allowed()) return;
    f_confirm_switch_model();
}

static void on_model_next_peek_cb(lv_event_t *e) {
    (void)e;
    if (!click_allowed()) return;
    os_viewed_model = (os_viewed_model + 1) % 4;
    f_refresh_models_page_dir(150);
}

static void on_model_reset_cb(lv_event_t *e) {
    (void)e;
    if (!click_allowed()) return;
    for (int i = 0; i < 4; ++i) {
        gimbal_min_cal[i] = 350;
        gimbal_max_cal[i] = 3745;
    }
    apply_model_profile(os_viewed_model);
    f_refresh_models_page();
}

/* Shared Figma Top Status Bar: TYVOR Logo + Dynamic Power + BT + 4 Signal Bars + Battery */
static void f_build_top_header(lv_obj_t *parent, int with_back_btn, const char *section_title) {
    unsigned p = (g_building_page < FIGMA_PAGE_COUNT) ? g_building_page : 0;
    int logo_x = 12;
    if (with_back_btn) {
        lv_obj_t *back = f_rect(parent, 4, 2, 38, 24, 0x1E3A5F, LV_OPA_COVER, 7, 1, 0x4A7AB0);
        f_icon_centered(back, LV_SYMBOL_LEFT, &lv_font_montserrat_14, 0xFFFFFF);
        lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(back, 16);
        lv_obj_add_event_cb(back, on_back_btn_cb, LV_EVENT_CLICKED, NULL);
        logo_x = 46;
    }

    f_tyvor_logo(parent, logo_x, 5, 0xFFFFFF);

    if (section_title && section_title[0]) {
        f_label(parent, logo_x + 80, 5, 102, section_title,
                &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
    }

    char pbuf[16];
    if (!os_rf_output_on) {
        snprintf(pbuf, sizeof(pbuf), "RF OFF");
    } else {
        snprintf(pbuf, sizeof(pbuf), "%u mW", (unsigned)k_power_mw_table[os_power_idx]);
    }
    w_hdr_power_lbl[p] = f_label(parent, 180, 6, 60, pbuf,
                                 &lv_font_montserrat_12,
                                 os_rf_output_on ? 0xD8E0E8 : 0xEF4444, LV_TEXT_ALIGN_RIGHT);
    f_label(parent, 244, 6, 14, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_12,
            os_bt_enabled ? 0x38BDF8 : 0x475569, LV_TEXT_ALIGN_CENTER);

    /* 4 Signal Bars */
    for (int i = 0; i < 4; ++i) {
        int bh = 4 + i * 3;
        w_hdr_sig_bars[p][i] = f_rect(parent, 262 + i * 4, 18 - bh, 3, bh,
                                      0x465160, LV_OPA_COVER, 1, 0, 0);
    }
    w_hdr_sig_x[p] = f_label(parent, 258, 3, 10, "x", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_LEFT);

    /* Battery Icon (Tap top-right to open Quick Settings) */
    lv_obj_t *bat_body = f_rect(parent, 284, 6, 22, 12, 0x0A1936, LV_OPA_COVER, 3, 1, 0xD8E0E8);
    (void)f_rect(parent, 306, 9, 2, 6, 0xD8E0E8, LV_OPA_COVER, 1, 0, 0);
    w_hdr_bat_fill[p] = f_rect(bat_body, 2, 2, 16, 6, 0x5ECA62, LV_OPA_COVER, 1, 0, 0);
    lv_obj_add_flag(bat_body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(bat_body, 10);
    lv_obj_add_event_cb(bat_body, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)FIGMA_PAGE_QUICK_SETTINGS);
}

/* Shared 3-Tab Menu Header for Row 1: QuickSet -> Settings -> About */
static void f_build_menu_row_header(lv_obj_t *parent, unsigned current_page) {
    unsigned p = (g_building_page < FIGMA_PAGE_COUNT) ? g_building_page : current_page;
    /* Enlarged Back button */
    lv_obj_t *back = f_rect(parent, 4, 2, 38, 24, 0x1E3A5F, LV_OPA_COVER, 7, 1, 0x4A7AB0);
    f_icon_centered(back, LV_SYMBOL_LEFT, &lv_font_montserrat_14, 0xFFFFFF);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(back, 16);
    lv_obj_add_event_cb(back, on_back_btn_cb, LV_EVENT_CLICKED, NULL);

    static const char *tab_titles[3] = {"QuickSet", "Settings", "About"};
    static const unsigned tab_pages[3] = {
        FIGMA_PAGE_QUICKSET_MENU,
        FIGMA_PAGE_SETTINGS_MENU,
        FIGMA_PAGE_ABOUT
    };
    for (int i = 0; i < 3; ++i) {
        int active = (current_page == tab_pages[i]);
        lv_obj_t *tab = f_rect(parent, 48 + i * 68, 3, 64, 22,
                               active ? 0x1E3A5F : 0x0F172A,
                               active ? LV_OPA_COVER : 160, 6,
                               active ? 1 : 0, 0x38BDF8);
        f_icon_centered(tab, tab_titles[i], &lv_font_montserrat_12,
                        active ? 0xFFFFFF : 0x64748B);
        lv_obj_add_flag(tab, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tab, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)tab_pages[i]);
    }

    /* Signal + Battery on right */
    for (int i = 0; i < 4; ++i) {
        int bh = 4 + i * 3;
        w_hdr_sig_bars[p][i] = f_rect(parent, 258 + i * 4, 18 - bh, 3, bh,
                                      0x465160, LV_OPA_COVER, 1, 0, 0);
    }
    lv_obj_t *bat_body = f_rect(parent, 282, 6, 22, 12, 0x0A1936, LV_OPA_COVER, 3, 1, 0xD8E0E8);
    (void)f_rect(parent, 304, 9, 2, 6, 0xD8E0E8, LV_OPA_COVER, 1, 0, 0);
    w_hdr_bat_fill[p] = f_rect(bat_body, 2, 2, 16, 6, 0x5ECA62, LV_OPA_COVER, 1, 0, 0);
}

/* ============================================================================
 * PAGE 0: TYVOR HOME (Center of Row 0: Left=Telemetry, Right=Models, Up=QuickSet)
 * ========================================================================== */
static void f_build_home_page(lv_obj_t *p) {
    f_build_top_header(p, 0, NULL);

    /* Left & Right Vertical Trim Sliders (aligned to Hero Card y=30..142) */
    (void)f_rect(p, 6, 44, 4, 84, 0x334155, LV_OPA_COVER, 2, 0, 0);
    w_home_trim_ticks[0] = f_rect(p, 4, 84, 8, 4, 0xE2E8F0, LV_OPA_COVER, 2, 0, 0);

    (void)f_rect(p, 310, 44, 4, 84, 0x334155, LV_OPA_COVER, 2, 0, 0);
    w_home_trim_ticks[1] = f_rect(p, 308, 84, 8, 4, 0xE2E8F0, LV_OPA_COVER, 2, 0, 0);

    /* Main Center Hero Card (x=18..302, y=30..140) */
    lv_obj_t *card = f_rect(p, 18, 30, 284, 110, 0x1E3F66, LV_OPA_COVER, 14, 1, 0x4A7AB0);
    lv_obj_set_style_bg_grad_color(card, lv_color_hex(0x132E52), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);

    /* Top Status Pill ("DISARMED" / "ARMED") */
    lv_obj_t *top_notch = f_rect(p, 102, 22, 116, 20, 0x071C3D, LV_OPA_COVER, 10, 1, 0x355C8C);
    w_home_arm_dot = f_rect(top_notch, 10, 5, 8, 8, 0xEF4444, LV_OPA_COVER, 4, 0, 0);
    w_home_arm_lbl = f_label(top_notch, 22, 2, 86, "DISARMED",
                             &lv_font_montserrat_12, 0xEF4444, LV_TEXT_ALIGN_CENTER);

    /* Left "Find" Button + Centered Radar Target Icon */
    lv_obj_t *find_btn = f_rect(p, 28, 48, 48, 44,
                                os_finder_active ? 0x0D3B2E : 0x264870,
                                LV_OPA_COVER, 11, 1,
                                os_finder_active ? 0x22C55E : 0x4A7AB0);
    lv_obj_t *radar_ring = f_rect(find_btn, 15, 13, 18, 18, 0x264870, LV_OPA_TRANSP, 9, 2, 0x38BDF8);
    (void)f_rect(radar_ring, 5, 5, 4, 4, 0x38BDF8, LV_OPA_COVER, 2, 0, 0);
    (void)f_rect(find_btn, 23, 6, 2, 5, 0x38BDF8, LV_OPA_COVER, 1, 0, 0);
    (void)f_rect(find_btn, 23, 33, 2, 5, 0x38BDF8, LV_OPA_COVER, 1, 0, 0);
    (void)f_rect(find_btn, 8, 21, 5, 2, 0x38BDF8, LV_OPA_COVER, 1, 0, 0);
    (void)f_rect(find_btn, 35, 21, 5, 2, 0x38BDF8, LV_OPA_COVER, 1, 0, 0);
    lv_obj_add_flag(find_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(find_btn, 6);
    lv_obj_add_event_cb(find_btn, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)FIGMA_PAGE_FIND);
    f_label(p, 26, 95, 52, "Find", &lv_font_montserrat_12, 0xCBD5E1, LV_TEXT_ALIGN_CENTER);

    /* Center Digital Session Timer & Active Model Box (Tap to open CH View) */
    lv_obj_t *timer_box = f_rect(p, 84, 46, 152, 62, 0x0A1C36, LV_OPA_COVER, 12, 1, 0x355C8C);
    f_label(timer_box, 4, 5, 144, k_os_models[os_active_model].full_title,
            &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);
    w_home_timer_lbl = f_label(timer_box, 4, 21, 144, "00:00",
                               &lv_font_montserrat_20, 0x38BDF8, LV_TEXT_ALIGN_CENTER);
    f_label(timer_box, 4, 44, 144, "TAP FOR CHANNELS",
            &lv_font_montserrat_12, 0x475569, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(timer_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(timer_box, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)FIGMA_PAGE_CH_VIEW);

    /* Right "Bind" Button + Centered RF WiFi Icon */
    lv_obj_t *bind_btn = f_rect(p, 244, 48, 48, 44,
                                os_bind_active ? 0x3E2723 : 0x264870,
                                LV_OPA_COVER, 11, 1,
                                os_bind_active ? 0xF97316 : 0x4A7AB0);
    f_icon_centered(bind_btn, LV_SYMBOL_WIFI, &lv_font_montserrat_20, 0x38BDF8);
    lv_obj_add_flag(bind_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(bind_btn, 6);
    lv_obj_add_event_cb(bind_btn, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)FIGMA_PAGE_BIND);
    f_label(p, 242, 95, 52, "Bind", &lv_font_montserrat_12, 0xCBD5E1, LV_TEXT_ALIGN_CENTER);

    /* Bottom Signal / Live Gimbal Pill (flush on Hero Card bottom edge) */
    lv_obj_t *bot_notch = f_rect(p, 76, 127, 168, 20, 0x071C3D, LV_OPA_COVER, 10, 1, 0x355C8C);
    w_home_sig_lbl = f_label(bot_notch, 4, 2, 160, "No Signal",
                             &lv_font_montserrat_12, 0xE67E22, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(bot_notch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bot_notch, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)FIGMA_PAGE_CH_VIEW);

    /* Bottom Left Card: Battery Widget (x=18, w=88, y=153..219) */
    lv_obj_t *bat_card = f_rect(p, 18, 153, 88, 66, 0x1E3F66, LV_OPA_COVER, 12, 1, 0x4A7AB0);
    lv_obj_set_style_bg_grad_color(bat_card, lv_color_hex(0x132E52), 0);
    lv_obj_set_style_bg_grad_dir(bat_card, LV_GRAD_DIR_VER, 0);
    w_home_volt_lbl = f_label(bat_card, 4, 6, 80, k_os_models[os_active_model].volt_str,
                              &lv_font_montserrat_14, 0xF1F5F9, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *bat_track = f_rect(bat_card, 16, 26, 56, 8, 0x0F2642, LV_OPA_COVER, 4, 0, 0);
    w_home_bat_bar = f_rect(bat_track, 0, 0, 48, 8, 0x5ECA62, LV_OPA_COVER, 4, 0, 0);
    f_label(bat_card, 4, 41, 80, LV_SYMBOL_BATTERY_FULL, &lv_font_montserrat_14, 0x5ECA62, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(bat_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bat_card, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)FIGMA_PAGE_TELEMETRY);

    /* Bottom Center Card: Flight Mode (x=116, w=88, y=153..219) */
    lv_obj_t *mode_card = f_rect(p, 116, 153, 88, 66, 0x1E3F66, LV_OPA_COVER, 12, 1, 0x4A7AB0);
    lv_obj_set_style_bg_grad_color(mode_card, lv_color_hex(0x132E52), 0);
    lv_obj_set_style_bg_grad_dir(mode_card, LV_GRAD_DIR_VER, 0);
    f_label(mode_card, 4, 6, 80, "MODE", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);
    w_home_mode_lbl = f_label(mode_card, 4, 24, 80, k_os_models[os_active_model].mode_name,
                              &lv_font_montserrat_14, k_os_models[os_active_model].badge_hex, LV_TEXT_ALIGN_CENTER);
    f_label(mode_card, 4, 44, 80, LV_SYMBOL_SHUFFLE " MODELS", &lv_font_montserrat_12, 0x64748B, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(mode_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(mode_card, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)FIGMA_PAGE_MODELS);

    /* Bottom Right Card: GPS Circular Arc Gauge (x=214, w=88, y=153..219) */
    lv_obj_t *gps_card = f_rect(p, 214, 153, 88, 66, 0x1E3F66, LV_OPA_COVER, 12, 1, 0x4A7AB0);
    lv_obj_set_style_bg_grad_color(gps_card, lv_color_hex(0x132E52), 0);
    lv_obj_set_style_bg_grad_dir(gps_card, LV_GRAD_DIR_VER, 0);
    w_home_gps_arc = lv_arc_create(gps_card);
    lv_obj_set_size(w_home_gps_arc, 42, 42);
    lv_obj_set_pos(w_home_gps_arc, 23, 5);
    lv_arc_set_rotation(w_home_gps_arc, 135);
    lv_arc_set_bg_angles(w_home_gps_arc, 0, 270);
    lv_arc_set_range(w_home_gps_arc, 0, 100);
    lv_arc_set_value(w_home_gps_arc, 0);
    lv_obj_remove_style(w_home_gps_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(w_home_gps_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(w_home_gps_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(w_home_gps_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(w_home_gps_arc, lv_color_hex(0x27466E), LV_PART_MAIN);
    lv_obj_set_style_arc_color(w_home_gps_arc, lv_color_hex(0x38BDF8), LV_PART_INDICATOR);
    w_home_gps_val = f_label(gps_card, 23, 17, 42, "0",
                             &lv_font_montserrat_14, 0xEF4444, LV_TEXT_ALIGN_CENTER);
    f_label(gps_card, 4, 46, 80, "GPS", &lv_font_montserrat_12, 0xCBD5E1, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(gps_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gps_card, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)FIGMA_PAGE_MODELS);

    /* Bottom Horizontal Trim Sliders + Center Swipe Hint */
    (void)f_rect(p, 22, 230, 84, 4, 0x334155, LV_OPA_COVER, 2, 0, 0);
    w_home_trim_ticks[2] = f_rect(p, 62, 228, 4, 8, 0xE2E8F0, LV_OPA_COVER, 2, 0, 0);

    f_label(p, 112, 223, 96, LV_SYMBOL_UP " QuickSet", &lv_font_montserrat_12, 0x64748B, LV_TEXT_ALIGN_CENTER);

    (void)f_rect(p, 214, 230, 84, 4, 0x334155, LV_OPA_COVER, 2, 0, 0);
    w_home_trim_ticks[3] = f_rect(p, 254, 228, 4, 8, 0xE2E8F0, LV_OPA_COVER, 2, 0, 0);
}

/* ============================================================================
 * PAGE 1: TYVOR MODELS (Figma 2007:1491 + Switch Model Modal 2007:1579)
 * ========================================================================== */
static lv_obj_t *f_build_model_card(lv_obj_t *parent, int model_idx, int x, int is_center) {
    const tyvor_os_model_profile_t *m = &k_os_models[model_idx];
    int is_act = (model_idx == os_active_model);
    lv_obj_t *card = f_rect(parent, x, 2, 206, 136, 0x0A1936, LV_OPA_COVER, 12,
                            2, is_act ? 0x5ECA62 : 0xD0D8E0);

    f_tyvor_logo(card, 8, 5, 0xFFFFFF);
    char pwr_buf[20];
    snprintf(pwr_buf, sizeof(pwr_buf), "%u mW", (unsigned)m->default_power_mw);
    f_label(card, 124, 6, 52, pwr_buf, &lv_font_montserrat_12, 0xD8E0E8, LV_TEXT_ALIGN_RIGHT);
    (void)f_rect(card, 180, 7, 16, 9, is_act ? 0x5ECA62 : 0x38BDF8, LV_OPA_COVER, 2, 1, 0xD8E0E8);

    lv_obj_t *mini_main = f_rect(card, 12, 24, 182, 66, 0x1E3F66, LV_OPA_COVER, 9, 1, m->theme_hex);
    f_label(mini_main, 0, 3, 182,
            is_act ? "ACTIVE MODEL" : "TAP TO SWITCH",
            &lv_font_montserrat_12,
            is_act ? 0x5ECA62 : 0xEAB308, LV_TEXT_ALIGN_CENTER);
    (void)f_rect(mini_main, 10, 20, 22, 22, 0x2D5078, LV_OPA_COVER, 5, 0, 0);
    f_label(mini_main, 36, 20, 110, "00:00", &lv_font_montserrat_20, 0xD8E0E8, LV_TEXT_ALIGN_CENTER);
    (void)f_rect(mini_main, 150, 20, 22, 22, 0x2D5078, LV_OPA_COVER, 5, 0, 0);

    char mix_rate_buf[40];
    snprintf(mix_rate_buf, sizeof(mix_rate_buf), "%s  •  %u Hz", m->mix_type_str, (unsigned)m->default_rate_hz);
    f_label(mini_main, 0, 48, 182, mix_rate_buf, &lv_font_montserrat_12, 0xE67E22, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *mc1 = f_rect(card, 12, 94, 56, 34, 0x1E3F66, LV_OPA_COVER, 7, 1, 0x4A7AB0);
    f_label(mc1, 0, 3, 56, m->volt_str, &lv_font_montserrat_12, 0xF1F5F9, LV_TEXT_ALIGN_CENTER);
    (void)f_rect(mc1, 13, 20, 30, 6, 0x5ECA62, LV_OPA_COVER, 3, 0, 0);

    lv_obj_t *mc2 = f_rect(card, 75, 94, 56, 34, 0x1E3F66, LV_OPA_COVER, 7, 1, 0x4A7AB0);
    f_label(mc2, 0, 2, 56, "MODE", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *mode_lbl = f_label(mc2, 0, 16, 56, m->mode_name, &lv_font_montserrat_12, m->badge_hex, LV_TEXT_ALIGN_CENTER);
    if (is_center) w_mod_prev_mode = mode_lbl;

    lv_obj_t *mc3 = f_rect(card, 138, 94, 56, 34, 0x1E3F66, LV_OPA_COVER, 7, 1, 0x4A7AB0);
    f_label(mc3, 0, 3, 56, "GPS", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);
    f_label(mc3, 0, 17, 56, "0", &lv_font_montserrat_12, 0xEF4444, LV_TEXT_ALIGN_CENTER);

    return card;
}

static void f_build_models_page(lv_obj_t *p) {
    /* Solid opaque background avoids expensive gradient recomputation during card drag */
    lv_obj_set_style_bg_color(p, lv_color_hex(0x0D2140), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);

    const tyvor_os_model_profile_t *m = &k_os_models[os_viewed_model];
    int is_active = (os_viewed_model == os_active_model);
    int next_idx = (os_viewed_model + 1) % 4;

    /* Top Bar: < Back pill, "<MODEL> Model" title, DEL/RST red pill */
    lv_obj_t *back = f_rect(p, 14, 6, 38, 22, 0x23476E, LV_OPA_COVER, 11, 1, 0x4A7AB0);
    f_icon_centered(back, LV_SYMBOL_LEFT, &lv_font_montserrat_14, 0xF2F7FF);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(back, 14);
    lv_obj_add_event_cb(back, on_back_btn_cb, LV_EVENT_CLICKED, NULL);

    char title_buf[48];
    snprintf(title_buf, sizeof(title_buf), "%s Model%s", m->tab_name, is_active ? " *" : "");
    w_mod_title_lbl = f_label(p, 58, 7, 200, title_buf,
                              &lv_font_montserrat_14,
                              is_active ? 0x5ECA62 : 0xF7FAFF, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *del_btn = f_rect(p, 264, 6, 42, 22, 0xC83233, LV_OPA_COVER, 11, 0, 0);
    f_label(del_btn, 0, 4, 42, "RST", &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(del_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(del_btn, on_model_reset_cb, LV_EVENT_CLICKED, NULL);

    /* 4 Segment Tabs: CRU | QUAD | FIXED | VTAIL */
    for (int i = 0; i < 4; ++i) {
        int sel = (i == os_viewed_model);
        w_mod_tabs[i] = f_rect(p, 14 + i * 74, 32, 70, 16,
                               sel ? k_os_models[i].theme_hex : 0x374250,
                               LV_OPA_COVER, 5,
                               sel ? 1 : ((i == os_active_model) ? 1 : 0),
                               sel ? 0x60A5FA : 0x5ECA62);
        f_label(w_mod_tabs[i], 0, 1, 70, k_os_models[i].tab_name,
                &lv_font_montserrat_12, sel ? 0xFFFFFF : 0x94A3B8, LV_TEXT_ALIGN_CENTER);
        lv_obj_add_flag(w_mod_tabs[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(w_mod_tabs[i], on_model_tab_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    /* Sliding Model Carousel Track (full prev/current/next 206px cards for smooth drag) */
    w_mod_track = lv_obj_create(p);
    lv_obj_remove_style_all(w_mod_track);
    lv_obj_set_pos(w_mod_track, MOD_TRACK_BASE_X, 50);
    lv_obj_set_size(w_mod_track, 800, 140);
    lv_obj_remove_flag(w_mod_track, LV_OBJ_FLAG_SCROLLABLE);

    /* Center slot x=297 inside w_mod_track -> screen x = -240 + 297 = 57 (centered 206px card) */
    if (os_viewed_model > 0) {
        (void)f_build_model_card(w_mod_track, os_viewed_model - 1, 297 - MOD_CARD_STRIDE, 0);
    }

    lv_obj_t *prev = f_build_model_card(w_mod_track, os_viewed_model, 297, 1);
    lv_obj_add_flag(prev, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(prev, on_model_activate_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *next_card = f_build_model_card(w_mod_track, next_idx, 297 + MOD_CARD_STRIDE, 0);
    lv_obj_add_flag(next_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(next_card, on_model_next_peek_cb, LV_EVENT_CLICKED, NULL);

    /* Bottom Action Buttons: Select Model (opens Confirm Modal) | Layout (CH View) */
    lv_obj_t *b_mod = f_rect(p, 18, 196, 136, 34,
                             is_active ? 0x15803D : 0x23476E, LV_OPA_COVER, 10,
                             1, is_active ? 0x5ECA62 : 0x4A7AB0);
    f_label(b_mod, 0, 9, 136, is_active ? "Active" : "Select",
            &lv_font_montserrat_14, 0xF5F7FC, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(b_mod, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b_mod, on_model_activate_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *b_lay = f_rect(p, 166, 196, 136, 34, 0x23476E, LV_OPA_COVER, 10, 2, 0x38BDF8);
    f_label(b_lay, 0, 9, 136, "Layout", &lv_font_montserrat_14, 0xF5F7FC, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(b_lay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b_lay, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)FIGMA_PAGE_MODEL_LAYOUT);

    /* ========================================================================
     * SWITCH MODEL CONFIRMATION MODAL (Figma 2007:1579)
     * ====================================================================== */
    w_mod_confirm_overlay = f_rect(p, 0, 0, 320, 240, 0x050E1E, 215, 0, 0, 0);
    lv_obj_add_flag(w_mod_confirm_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(w_mod_confirm_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *dlg = f_rect(w_mod_confirm_overlay, 36, 14, 248, 212, 0x132E52, LV_OPA_COVER, 14, 1, 0x4A7AB0);
    lv_obj_remove_flag(dlg, LV_OBJ_FLAG_CLICKABLE);
    f_label(dlg, 0, 6, 248, "Switch to", &lv_font_montserrat_14, 0xF7FAFF, LV_TEXT_ALIGN_CENTER);

    /* Mini Preview inside Confirmation Modal */
    w_mod_modal_card = f_rect(dlg, 46, 26, 156, 94, 0x0A1936, LV_OPA_COVER, 10, 2, m->theme_hex);
    f_tyvor_logo(w_mod_modal_card, 6, 4, 0xFFFFFF);
    w_mod_modal_pwr = f_label(w_mod_modal_card, 100, 4, 48, "400mW", &lv_font_montserrat_12, 0xD8E0E8, LV_TEXT_ALIGN_RIGHT);

    lv_obj_t *m_inner = f_rect(w_mod_modal_card, 8, 22, 140, 38, 0x1E3F66, LV_OPA_COVER, 7, 0, 0);
    f_label(m_inner, 0, 2, 140, "No Arm", &lv_font_montserrat_12, 0xE53935, LV_TEXT_ALIGN_CENTER);
    f_label(m_inner, 0, 15, 140, "00:00", &lv_font_montserrat_14, 0xD8E0E8, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *m_bat = f_rect(w_mod_modal_card, 8, 63, 44, 24, 0x1E3F66, LV_OPA_COVER, 5, 0, 0);
    w_mod_modal_volt = f_label(m_bat, 0, 5, 44, m->volt_str, &lv_font_montserrat_12, 0xF1F5F9, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *m_mod = f_rect(w_mod_modal_card, 56, 63, 44, 24, 0x1E3F66, LV_OPA_COVER, 5, 0, 0);
    w_mod_modal_mode = f_label(m_mod, 0, 5, 44, m->mode_name, &lv_font_montserrat_12, m->badge_hex, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *m_gps = f_rect(w_mod_modal_card, 104, 63, 44, 24, 0x1E3F66, LV_OPA_COVER, 5, 0, 0);
    f_label(m_gps, 0, 5, 44, "GPS 0", &lv_font_montserrat_12, 0xEF4444, LV_TEXT_ALIGN_CENTER);

    /* 4 SwitchModelPills (CRU, QUAD, FIXED, V-TAIL) */
    static const uint32_t pill_colors[4] = {0x1E3A8A, 0x065F46, 0x075985, 0x581C87};
    static const char *pill_names[4] = {"CRU", "QUAD", "FIXED", "V-TAIL"};
    for (int i = 0; i < 4; ++i) {
        w_mod_modal_pills[i] = f_rect(dlg, 14 + i * 56, 125, 52, 18,
                                      pill_colors[i], LV_OPA_COVER, 9, 0, 0xFFFFFF);
        f_label(w_mod_modal_pills[i], 0, 2, 52, pill_names[i],
                &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
        lv_obj_add_flag(w_mod_modal_pills[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(w_mod_modal_pills[i], on_model_pill_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    w_mod_modal_name = f_label(dlg, 0, 143, 248, m->full_title,
                               &lv_font_montserrat_14, 0xFAFCFF, LV_TEXT_ALIGN_CENTER);

    /* Cancel & Switch Buttons (tall, easy to tap) */
    lv_obj_t *btn_cancel = f_rect(dlg, 14, 162, 104, 38, 0x23476E, LV_OPA_COVER, 9, 1, 0x4A7AB0);
    f_label(btn_cancel, 0, 11, 104, "Cancel", &lv_font_montserrat_14, 0xF2F7FF, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(btn_cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(btn_cancel, 12);
    lv_obj_add_event_cb(btn_cancel, on_model_confirm_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_switch = f_rect(dlg, 130, 162, 104, 38, 0x2D5078, LV_OPA_COVER, 9, 2, 0xEAB308);
    f_label(btn_switch, 0, 11, 104, "Switch", &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(btn_switch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(btn_switch, 12);
    lv_obj_add_event_cb(btn_switch, on_model_confirm_switch_cb, LV_EVENT_CLICKED, NULL);
}

/* ============================================================================
 * PAGE 1B: MODEL LAYOUT & MIXER MAP (Airframe Output & Channel Assignment)
 * ========================================================================== */
static void f_build_model_layout_page(lv_obj_t *p) {
    const tyvor_os_model_profile_t *m = &k_os_models[os_viewed_model];
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "%s Layout", m->tab_name);
    f_build_top_header(p, 1, hdr);

    /* Left Card: Airframe / Output Visual Diagram */
    lv_obj_t *diag = f_rect(p, 12, 36, 136, 152, 0x0E1E38, LV_OPA_COVER, 10, 1, m->theme_hex);
    f_label(diag, 0, 6, 136, m->mix_type_str, &lv_font_montserrat_12, m->badge_hex, LV_TEXT_ALIGN_CENTER);

    if (os_viewed_model == 1) {
        /* QUAD X-Frame: 4 Motors + Center FC */
        f_rect(diag, 34, 42, 68, 4, 0x334155, LV_OPA_COVER, 2, 0, 0);
        f_rect(diag, 66, 36, 4, 68, 0x334155, LV_OPA_COVER, 2, 0, 0);
        f_rect(diag, 52, 56, 32, 24, 0x1E3F66, LV_OPA_COVER, 5, 1, 0x38BDF8);
        f_label(diag, 52, 62, 32, "FC", &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);

        static const int mx[4] = {24, 86, 24, 86};
        static const int my[4] = {30, 30, 82, 82};
        static const char *ml[4] = {"M4", "M2", "M3", "M1"};
        for (int i = 0; i < 4; ++i) {
            lv_obj_t *mc = f_rect(diag, mx[i], my[i], 26, 26, 0x123254, LV_OPA_COVER, 13, 1, 0x22C55E);
            f_label(mc, 0, 6, 26, ml[i], &lv_font_montserrat_12, 0x5ECA62, LV_TEXT_ALIGN_CENTER);
        }
        f_label(diag, 0, 126, 136, "4-Motor X Quad", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);
    } else if (os_viewed_model == 0) {
        /* CRUISER (Car/Boat): Steering Front + Throttle Rear */
        lv_obj_t *body = f_rect(diag, 44, 34, 48, 76, 0x1E3F66, LV_OPA_COVER, 10, 1, 0x38BDF8);
        f_label(body, 0, 28, 48, "CRU", &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
        f_rect(diag, 30, 42, 12, 20, 0x22C55E, LV_OPA_COVER, 3, 0, 0);
        f_rect(diag, 94, 42, 12, 20, 0x22C55E, LV_OPA_COVER, 3, 0, 0);
        f_rect(diag, 30, 80, 12, 22, 0xF97316, LV_OPA_COVER, 3, 0, 0);
        f_rect(diag, 94, 80, 12, 22, 0xF97316, LV_OPA_COVER, 3, 0, 0);
        f_label(diag, 0, 126, 136, "Steer + Drive", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);
    } else if (os_viewed_model == 2) {
        /* FIXED WING: Wing + Fuselage + Tail */
        f_rect(diag, 16, 56, 104, 18, 0x1E3F66, LV_OPA_COVER, 6, 1, 0x38BDF8);
        f_rect(diag, 60, 32, 16, 76, 0x23476E, LV_OPA_COVER, 6, 1, 0x60A5FA);
        f_rect(diag, 40, 96, 56, 12, 0x1E3F66, LV_OPA_COVER, 4, 1, 0xF97316);
        f_label(diag, 16, 59, 104, "AIL   THR   AIL", &lv_font_montserrat_12, 0xE2E8F0, LV_TEXT_ALIGN_CENTER);
        f_label(diag, 0, 126, 136, "4CH Fixed-Wing", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);
    } else {
        /* V-TAIL: Swept Wing + V-Mix Tail */
        f_rect(diag, 18, 52, 100, 18, 0x1E3F66, LV_OPA_COVER, 6, 1, 0xA855F7);
        f_rect(diag, 60, 32, 16, 70, 0x23476E, LV_OPA_COVER, 6, 1, 0xC084FC);
        f_rect(diag, 34, 88, 28, 12, 0x3B1D60, LV_OPA_COVER, 4, 1, 0xE879F9);
        f_rect(diag, 74, 88, 28, 12, 0x3B1D60, LV_OPA_COVER, 4, 1, 0xE879F9);
        f_label(diag, 34, 88, 28, "VL", &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
        f_label(diag, 74, 88, 28, "VR", &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
        f_label(diag, 0, 126, 136, "V-Tail Mix", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);
    }

    /* Right Card: 6-Channel & Switch Assignment Table */
    lv_obj_t *tbl = f_rect(p, 154, 36, 154, 152, 0x0E1E38, LV_OPA_COVER, 10, 1, 0x263B59);
    static const char *ch_lbls[6] = {"CH1", "CH2", "CH3", "CH4", "CH5", "CH6"};
    const char *ch_fn[6] = {
        (os_viewed_model == 0) ? "Steering" : "Roll / AIL",
        (os_viewed_model == 0) ? "Aux 1" : (os_viewed_model == 3 ? "V-Mix L" : "Pitch / ELE"),
        "Throttle",
        (os_viewed_model == 3) ? "V-Mix R" : "Yaw / RUD",
        "SA • ARM",
        "SB • Mode"
    };
    static const uint32_t fn_col[6] = {
        0x38BDF8, 0x38BDF8, 0xF97316, 0x38BDF8, 0x22C55E, 0xA855F7
    };
    for (int i = 0; i < 6; ++i) {
        int ry = 8 + i * 23;
        lv_obj_t *tag = f_rect(tbl, 8, ry, 36, 18, 0x162C4A, LV_OPA_COVER, 5, 0, 0);
        f_label(tag, 0, 2, 36, ch_lbls[i], &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);
        f_label(tbl, 50, ry + 2, 96, ch_fn[i], &lv_font_montserrat_12, fn_col[i], LV_TEXT_ALIGN_LEFT);
    }

    /* Bottom Action Buttons: Stick Mode | CH Monitor */
    lv_obj_t *b_mode = f_rect(p, 12, 196, 144, 34, 0x1E3F66, LV_OPA_COVER, 10, 1, 0x38BDF8);
    f_label(b_mode, 0, 9, 144, "Stick Mode", &lv_font_montserrat_14, 0xF5F7FC, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(b_mode, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b_mode, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)FIGMA_PAGE_STICK_MODE);

    lv_obj_t *b_chv = f_rect(p, 164, 196, 144, 34, 0x1E3F66, LV_OPA_COVER, 10, 1, 0x4A7AB0);
    f_label(b_chv, 0, 9, 144, "CH Monitor", &lv_font_montserrat_14, 0xF5F7FC, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(b_chv, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b_chv, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)FIGMA_PAGE_CH_VIEW);
}

/* ============================================================================
 * PAGE 2: TYVOR CH VIEW (Figma Node 2053:80 — Live 10CH Gimbal Monitor)
 * ========================================================================== */
static void f_build_ch_view_page(lv_obj_t *p) {
    f_build_top_header(p, 1, "CH View");

    static const char *ch_names[10] = {
        "CH1(Roll)", "CH2(Pitch)", "CH3(Thr)", "CH4(Yaw)", "CH5(SA)",
        "CH6(SB)",   "CH7(SC)",    "CH8(SD)",  "CH9(SE)",  "CH10(SF)"
    };
    static const uint32_t ch_colors[10] = {
        0xF97316, 0xF97316, 0xEF4444, 0xF97316, 0x6366F1,
        0xEC4899, 0x6366F1, 0x6366F1, 0xA855F7, 0x6366F1
    };

    for (int i = 0; i < 10; ++i) {
        int col = i / 5;
        int row = i % 5;
        int x = (col == 0) ? 14 : 166;
        int y = 34 + row * 40;

        f_label(p, x, y, 88, ch_names[i], &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_LEFT);
        w_chv_pct[i] = f_label(p, x + 88, y, 52, "0%",
                               &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_RIGHT);

        lv_obj_t *track = f_rect(p, x, y + 17, 140, 12, 0x1E293B, LV_OPA_COVER, 6, 0, 0);
        w_chv_fill[i] = f_rect(track, 0, 0, 70, 12, ch_colors[i], LV_OPA_COVER, 6, 0, 0);
        w_chv_tick[i] = f_rect(track, 69, 0, 2, 12, 0xFFFFFF, LV_OPA_COVER, 0, 0, 0);
    }
}

/* ============================================================================
 * PAGE 3: TYVOR CHANNELS (Figma Node 2070:70 — 6CH Setup & Live Monitor)
 * ========================================================================== */
static void f_build_channels_page(lv_obj_t *p) {
    f_build_top_header(p, 1, "Channels");

    static const char *badge_txt[6] = {"CH1", "CH2", "CH3", "CH4", "CH5", "CH6"};
    static const char *title_txt[6] = {"Roll", "Pitch", "Throttle", "Yaw", "SA", "SB"};
    static const char *sub_txt[6] = {
        "GPIO 1 (ADC)", "GPIO 2 (ADC)", "GPIO 3 (ADC)",
        "GPIO 4 (ADC)", "GPIO 38 (SW)", "GPIO 39 (SW)"
    };
    static const uint32_t bar_col[6] = {
        0x38BDF8, 0x38BDF8, 0xEF4444, 0x38BDF8, 0xF59E0B, 0xF59E0B
    };

    for (int i = 0; i < 6; ++i) {
        int col = i % 3;
        int row = i / 3;
        int x = 10 + col * 102;
        int y = 34 + row * 100;

        lv_obj_t *card = f_rect(p, x, y, 96, 92, 0x142238, 235, 10, 1, 0x263B59);
        lv_obj_t *badge = f_rect(card, 6, 6, 32, 16, 0x0F2D4A, LV_OPA_COVER, 5, 0, 0);
        f_label(badge, 0, 1, 32, badge_txt[i], &lv_font_montserrat_12, 0x38BDF8, LV_TEXT_ALIGN_CENTER);

        w_chc_pct[i] = f_label(card, 40, 6, 50, "0%",
                               &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_RIGHT);
        f_label(card, 6, 28, 84, title_txt[i], &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
        f_label(card, 6, 47, 84, sub_txt[i], &lv_font_montserrat_12, 0x64748B, LV_TEXT_ALIGN_LEFT);

        lv_obj_t *track = f_rect(card, 6, 70, 84, 8, 0x1E293B, LV_OPA_COVER, 4, 0, 0);
        w_chc_fill[i] = f_rect(track, 0, 0, 42, 8, bar_col[i], LV_OPA_COVER, 4, 0, 0);
    }
}

/* ============================================================================
 * PAGE 4: TYVOR TELEMETRY (Left of HOME — Swipe Right from HOME)
 * ========================================================================== */
static void f_build_telemetry_page(lv_obj_t *p) {
    f_build_top_header(p, 1, "Telemetry");

    /* Vertical scrollable viewport (opaque solid bg + flat borderless cards for smooth SPI scroll) */
    lv_obj_t *vp = lv_obj_create(p);
    w_tel_vp = vp;
    lv_obj_set_pos(vp, 0, 32);
    lv_obj_set_size(vp, 320, 208);
    lv_obj_set_style_bg_color(vp, lv_color_hex(0x0B1E3D), 0);
    lv_obj_set_style_bg_opa(vp, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vp, 0, 0);
    lv_obj_set_style_pad_all(vp, 0, 0);
    lv_obj_set_style_pad_bottom(vp, 12, 0);
    lv_obj_add_flag(vp, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(vp, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scroll_dir(vp, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(vp, LV_SCROLLBAR_MODE_AUTO);

    typedef struct {
        const char *name;
        int group_idx;
        int full_width;
    } tel_field_def_t;

    static const tel_field_def_t k_tel_fields[23] = {
        /* Group 0: RF Link */
        {"1RSS", 0, 0}, {"2RSS", 0, 0},
        {"RQLY", 0, 0}, {"RSNR", 0, 0},
        {"ANT",  0, 0}, {"RFMD", 0, 0},
        /* Group 1: TX & Power */
        {"TPWR", 1, 0}, {"TRSS", 1, 0},
        {"TQly", 1, 0}, {"TSNR", 1, 0},
        {"RxBT", 1, 0}, {"Curr", 1, 0},
        /* Group 2: Battery & Attitude */
        {"Capa", 2, 0}, {"Bat%", 2, 0},
        {"Ptch", 2, 0}, {"Roll", 2, 0},
        {"Yaw",  2, 0}, {"FM",   2, 0},
        /* Group 3: GPS */
        {"GPS",  3, 1},
        {"GSpd", 3, 0}, {"Hdg",  3, 0},
        {"Alt",  3, 0}, {"Sats", 3, 0},
    };
    static const char *k_group_titles[4] = {
        "RF LINK", "TX & POWER", "BATTERY & ATTITUDE", "GPS & NAVIGATION"
    };

    int y = 2;
    int last_grp = -1;
    int col = 0;
    for (int i = 0; i < 23; ++i) {
        if (k_tel_fields[i].group_idx != last_grp) {
            if (col == 1) {
                y += 48;
                col = 0;
            }
            if (last_grp != -1) y += 6;
            last_grp = k_tel_fields[i].group_idx;
            f_label(vp, 14, y, 292, k_group_titles[last_grp],
                    &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_LEFT);
            y += 18;
        }

        int full = k_tel_fields[i].full_width;
        if (full && col == 1) {
            y += 48;
            col = 0;
        }
        int x = full ? 14 : (14 + col * 150);
        int w = full ? 292 : 142;

        lv_obj_t *card = f_rect(vp, x, y, w, 42, 0x163357, LV_OPA_COVER, 8, 0, 0x000000);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_GESTURE_BUBBLE);

        lv_obj_t *name_lbl = f_label(card, 12, 13, full ? 60 : 54, k_tel_fields[i].name,
                &lv_font_montserrat_12, 0xE5E5EA, LV_TEXT_ALIGN_LEFT);
        lv_obj_clear_flag(name_lbl, LV_OBJ_FLAG_CLICKABLE);
        w_tel_val[i] = f_label(card, full ? 72 : 62, 12, full ? 208 : 68, "---",
                               &lv_font_montserrat_14, 0xFF3038, LV_TEXT_ALIGN_RIGHT);
        lv_obj_clear_flag(w_tel_val[i], LV_OBJ_FLAG_CLICKABLE);

        if (full) {
            y += 48;
            col = 0;
        } else {
            col++;
            if (col >= 2) {
                col = 0;
                y += 48;
            }
        }
    }
}

/* ============================================================================
 * PAGE 5: TYVOR QUICK SETTINGS SHADE (Swipe DOWN from HOME)
 * ========================================================================== */
static void on_qshade_action_cb(lv_event_t *e) {
    if (!click_allowed()) return;
    int act = (int)(intptr_t)lv_event_get_user_data(e);
    if (act == 0) {
        os_brightness_pct += 25;
        if (os_brightness_pct > 100) os_brightness_pct = 25;
    } else if (act == 1) {
        os_volume_pct += 25;
        if (os_volume_pct > 100) os_volume_pct = 0;
    } else if (act == 2) {
        os_bt_enabled = !os_bt_enabled;
        f_invalidate_all_pages();
    } else if (act == 3) {
        os_touch_locked = !os_touch_locked;
    }
    os_config_save();
    f_invalidate_page(FIGMA_PAGE_QUICK_SETTINGS);
    f_open_page(FIGMA_PAGE_QUICK_SETTINGS);
}

static void f_build_quick_settings_page(lv_obj_t *p) {
    f_build_top_header(p, 1, NULL);

    lv_obj_t *panel = f_rect(p, 12, 26, 296, 206, 0x182940, 240, 14, 1, 0x4A6B8C);
    f_label(panel, 12, 8, 130, "Quick settings", &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *demo = f_rect(panel, 130, 8, 56, 18, 0x10324A, LV_OPA_COVER, 9, 1, 0x38BDF8);
    f_label(demo, 0, 2, 56, k_os_models[os_active_model].tab_name,
            &lv_font_montserrat_12, 0x38BDF8, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *close_btn = f_rect(panel, 258, 4, 30, 24, 0x263B56, LV_OPA_COVER, 11, 1, 0x526D8C);
    f_label(close_btn, 0, 4, 30, "X", &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(close_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(close_btn, 14);
    lv_obj_add_event_cb(close_btn, on_back_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Brightness Card (Drag slider smoothly 10%..100%) */
    char br_buf[16];
    snprintf(br_buf, sizeof(br_buf), "%d%%", os_brightness_pct);
    lv_obj_t *br_card = f_rect(panel, 10, 32, 276, 38, 0x142238, LV_OPA_COVER, 8, 1, 0x2A4365);
    f_label(br_card, 10, 4, 120, "Brightness", &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
    w_qs_br_lbl = f_label(br_card, 210, 4, 56, br_buf, &lv_font_montserrat_12, 0x38BDF8, LV_TEXT_ALIGN_RIGHT);
    lv_obj_t *br_tr = f_rect(br_card, 10, 22, 256, 8, 0x0A1526, LV_OPA_COVER, 4, 0, 0);
    int bw = (256 * os_brightness_pct) / 100;
    if (bw < 6) bw = 6;
    w_qs_br_fill = f_rect(br_tr, 0, 0, bw, 8, 0x38BDF8, LV_OPA_COVER, 4, 0, 0);
    w_qs_br_knob = f_rect(br_card, 10 + bw - 7, 19, 14, 14, 0xFFFFFF, LV_OPA_COVER, 7, 2, 0x38BDF8);

    /* Volume Card (Drag slider smoothly 0%..100%) */
    char vol_buf[16];
    snprintf(vol_buf, sizeof(vol_buf), "%d%%", os_volume_pct);
    lv_obj_t *vol_card = f_rect(panel, 10, 76, 276, 38, 0x142238, LV_OPA_COVER, 8, 1, 0x2A4365);
    f_label(vol_card, 10, 4, 120, "Volume", &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
    w_qs_vol_lbl = f_label(vol_card, 210, 4, 56, vol_buf, &lv_font_montserrat_12, 0x38BDF8, LV_TEXT_ALIGN_RIGHT);
    lv_obj_t *vol_tr = f_rect(vol_card, 10, 22, 256, 8, 0x0A1526, LV_OPA_COVER, 4, 0, 0);
    int vw = (256 * os_volume_pct) / 100;
    if (vw < 4) vw = 4;
    w_qs_vol_fill = f_rect(vol_tr, 0, 0, vw, 8, 0x38BDF8, LV_OPA_COVER, 4, 0, 0);
    w_qs_vol_knob = f_rect(vol_card, 10 + vw - 7, 19, 14, 14, 0xFFFFFF, LV_OPA_COVER, 7, 2, 0x38BDF8);

    /* Bluetooth & Touch Lock Cards */
    lv_obj_t *bt_card = f_rect(panel, 10, 120, 134, 46,
                               os_bt_enabled ? 0x16385E : 0x142238,
                               LV_OPA_COVER, 8, 1,
                               os_bt_enabled ? 0x38BDF8 : 0x2A4365);
    f_label(bt_card, 10, 7, 114, "Bluetooth", &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
    f_label(bt_card, 10, 25, 114, os_bt_enabled ? "Connected" : "Disabled",
            &lv_font_montserrat_12, os_bt_enabled ? 0x38BDF8 : 0x64748B, LV_TEXT_ALIGN_LEFT);
    lv_obj_add_flag(bt_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bt_card, on_qshade_action_cb, LV_EVENT_CLICKED, (void *)(intptr_t)2);

    lv_obj_t *tl_card = f_rect(panel, 152, 120, 134, 46,
                               os_touch_locked ? 0x3B1A1A : 0x142238,
                               LV_OPA_COVER, 8, 1,
                               os_touch_locked ? 0xEF4444 : 0x2A4365);
    f_label(tl_card, 10, 7, 114, "Touch lock", &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
    f_label(tl_card, 10, 25, 114, os_touch_locked ? "Locked" : "Unlocked",
            &lv_font_montserrat_12, os_touch_locked ? 0xEF4444 : 0x94A3B8, LV_TEXT_ALIGN_LEFT);
    lv_obj_add_flag(tl_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tl_card, on_qshade_action_cb, LV_EVENT_CLICKED, (void *)(intptr_t)3);

    /* All Settings Button */
    lv_obj_t *all_btn = f_rect(panel, 10, 172, 276, 26, 0x1E3350, LV_OPA_COVER, 8, 1, 0x3B5980);
    f_label(all_btn, 0, 5, 276, "All settings", &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(all_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(all_btn, on_nav_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)FIGMA_PAGE_SETTINGS_MENU);
}

/* ============================================================================
 * PAGE 6: TYVOR QUICKSET MENU + 5 FIGMA SUB-PAGES + 2-WAY STM32 UART SYNC
 * ========================================================================== */
static void esp_send_quickset_cmd(uint8_t cmd) {
    os_qs_local_grace_ms = millis() + 450;
    if (ui_endpoint.link.ready) {
        (void)tyvor_ui_endpoint_request(&ui_endpoint, TYVOR_REQ_QUICKSET, cmd);
        ui_endpoint.read_active = 0;
        ui_endpoint.next_read_ms = millis();
    }
}

typedef struct {
    uint32_t magic;
    uint8_t active_model;
    uint8_t power_idx;
    uint8_t elrs_rate_idx;
    uint8_t telem_ratio_idx;
    uint8_t model_match;
    uint8_t rf_output_on;
    uint8_t rf_external;
    uint8_t lang_vi;
    uint8_t stick_mode;
    uint8_t thr_lock_on;
    uint8_t brightness_pct;
    uint8_t volume_pct;
    uint8_t bt_enabled;
    uint32_t sleep_timeout_sec;
    int32_t gimbal_min_cal[4];
    int32_t gimbal_max_cal[4];
} tyvor_os_nvs_cfg_t;

#define TYVOR_NVS_MAGIC 0x54595631u /* "TYV1" */

static void os_config_save(void) {
    nvs_handle_t h = 0;
    if (nvs_open("tyvor_os", NVS_READWRITE, &h) != ESP_OK) return;
    tyvor_os_nvs_cfg_t cfg = {
        .magic = TYVOR_NVS_MAGIC,
        .active_model = (uint8_t)os_active_model,
        .power_idx = (uint8_t)os_power_idx,
        .elrs_rate_idx = (uint8_t)os_elrs_rate_idx,
        .telem_ratio_idx = (uint8_t)os_telem_ratio_idx,
        .model_match = (uint8_t)os_model_match,
        .rf_output_on = (uint8_t)os_rf_output_on,
        .rf_external = (uint8_t)os_rf_external,
        .lang_vi = (uint8_t)os_lang_vi,
        .stick_mode = (uint8_t)os_stick_mode,
        .thr_lock_on = (uint8_t)os_thr_lock_on,
        .brightness_pct = (uint8_t)os_brightness_pct,
        .volume_pct = (uint8_t)os_volume_pct,
        .bt_enabled = (uint8_t)os_bt_enabled,
        .sleep_timeout_sec = os_sleep_timeout_sec,
    };
    for (int i = 0; i < 4; ++i) {
        cfg.gimbal_min_cal[i] = gimbal_min_cal[i];
        cfg.gimbal_max_cal[i] = gimbal_max_cal[i];
    }
    if (nvs_set_blob(h, "cfg_v1", &cfg, sizeof(cfg)) == ESP_OK) {
        (void)nvs_commit(h);
    }
    nvs_close(h);
}

static void os_config_load(void) {
    nvs_handle_t h = 0;
    if (nvs_open("tyvor_os", NVS_READONLY, &h) != ESP_OK) return;
    tyvor_os_nvs_cfg_t cfg;
    size_t sz = sizeof(cfg);
    if (nvs_get_blob(h, "cfg_v1", &cfg, &sz) == ESP_OK &&
        sz == sizeof(cfg) && cfg.magic == TYVOR_NVS_MAGIC) {
        if (cfg.active_model < 4) {
            os_active_model = cfg.active_model;
            os_viewed_model = cfg.active_model;
        }
        if (cfg.power_idx < 5) os_power_idx = cfg.power_idx;
        if (cfg.elrs_rate_idx < 4) os_elrs_rate_idx = cfg.elrs_rate_idx;
        if (cfg.telem_ratio_idx < 4) os_telem_ratio_idx = cfg.telem_ratio_idx;
        os_model_match = cfg.model_match ? 1 : 0;
        os_rf_output_on = 0; /* Safety: never auto-enable RF output on boot from NVS */
        os_rf_external = cfg.rf_external ? 1 : 0;
        os_lang_vi = cfg.lang_vi ? 1 : 0;
        os_stick_mode = (cfg.stick_mode == 1) ? 1 : 2;
        os_thr_lock_on = cfg.thr_lock_on ? 1 : 0;
        if (cfg.brightness_pct >= 10 && cfg.brightness_pct <= 100) os_brightness_pct = cfg.brightness_pct;
        if (cfg.volume_pct <= 100) os_volume_pct = cfg.volume_pct;
        os_bt_enabled = cfg.bt_enabled ? 1 : 0;
        os_sleep_timeout_sec = (cfg.sleep_timeout_sec == 0 || cfg.sleep_timeout_sec == 15 ||
                                cfg.sleep_timeout_sec == 30 || cfg.sleep_timeout_sec == 60 ||
                                cfg.sleep_timeout_sec == 120)
                               ? cfg.sleep_timeout_sec : 60u;
        for (int i = 0; i < 4; ++i) {
            if (cfg.gimbal_max_cal[i] - cfg.gimbal_min_cal[i] > 300) {
                gimbal_min_cal[i] = cfg.gimbal_min_cal[i];
                gimbal_max_cal[i] = cfg.gimbal_max_cal[i];
            }
        }
        ESP_LOGI(TAG, "Loaded persistent TYVOR OS config from NVS (model=%d pwr=%umW rate=%uHz)",
                 os_active_model, (unsigned)k_power_mw_table[os_power_idx],
                 (unsigned)k_elrs_rate_table[os_elrs_rate_idx]);
    }
    nvs_close(h);
}

static void os_sync_all_to_stm32(void) {
    if (!ui_endpoint.link.ready) return;
    /* Safety: never send RF ON on boot; only sync Stick Mode, THR Lock, Power & ELRS Rate */
    esp_send_quickset_cmd((uint8_t)(0x00u + (uint8_t)os_power_idx));
    esp_send_quickset_cmd((uint8_t)(0x10u + (uint8_t)os_elrs_rate_idx));
    esp_send_quickset_cmd((uint8_t)(0x50u + (uint8_t)os_telem_ratio_idx));
    esp_send_quickset_cmd((uint8_t)(os_model_match ? 0x61u : 0x60u));
    esp_send_quickset_cmd((uint8_t)(os_rf_external ? 0x71u : 0x70u));
    esp_send_quickset_cmd((uint8_t)(os_thr_lock_on ? 0x81u : 0x80u));
    esp_send_quickset_cmd((uint8_t)((os_stick_mode == 2) ? 0x91u : 0x90u));
}

static void f_quickset_handle_card(int idx) {
    nav_return_page = FIGMA_PAGE_QUICKSET_MENU;
    unsigned target = FIGMA_PAGE_QUICKSET_MENU;
    if (idx == 0) target = FIGMA_PAGE_CH_VIEW;
    else if (idx == 1) target = FIGMA_PAGE_TX_POWER;
    else if (idx == 2) target = FIGMA_PAGE_ELRS_SET;
    else if (idx == 3) target = FIGMA_PAGE_FIND;
    else if (idx == 4) target = FIGMA_PAGE_RF_OUTPUT;
    else if (idx == 5) target = FIGMA_PAGE_BIND;

    active_non_sleep_page = target;
    f_invalidate_page(target);
    f_open_page(target);
}

static void on_quickset_card_cb(lv_event_t *e) {
    if (!click_allowed()) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    f_quickset_handle_card(idx);
}

static void f_build_quickset_menu_page(lv_obj_t *p) {
    f_build_menu_row_header(p, FIGMA_PAGE_QUICKSET_MENU);

    static const char *qs_titles[6] = {
        "CH View", "Power", "ELRS Set", "Finder", "RF Output", "Bind"
    };
    char pwr_sub[20], rate_sub[20];
    snprintf(pwr_sub, sizeof(pwr_sub), "%u mW", (unsigned)k_power_mw_table[os_power_idx]);
    snprintf(rate_sub, sizeof(rate_sub), "%u Hz", (unsigned)k_elrs_rate_table[os_elrs_rate_idx]);

    const char *qs_subs[6] = {
        "10 CH Live",
        pwr_sub,
        rate_sub,
        os_finder_active ? "BEEPING!" : "Ready",
        os_rf_output_on ? (os_rf_external ? "EXT ON" : "INT ON") : "OFF",
        os_bind_active ? "BINDING..." : "CRSFv2"
    };
    static const uint32_t qs_badge_bg[6] = {
        0x1E3A5F, 0x0D3B2E, 0x2E1A47, 0x3E2723, 0x3B2A14, 0x3E2723
    };
    static const uint32_t qs_badge_fg[6] = {
        0x38BDF8, 0x22C55E, 0xC084FC, 0xF59E0B, 0xFBBF24, 0xF97316
    };
    static const char *qs_glyph[6] = {
        NULL,
        LV_SYMBOL_CHARGE,
        LV_SYMBOL_SETTINGS,
        LV_SYMBOL_VOLUME_MAX,
        LV_SYMBOL_WIFI,
        LV_SYMBOL_LOOP
    };

    for (int i = 0; i < 6; ++i) {
        int col = i % 2;
        int row = i / 2;
        int x = 14 + col * 150;
        int y = 36 + row * 64;

        int highlight = (i == 3 && os_finder_active) ||
                        (i == 4 && os_rf_output_on) ||
                        (i == 5 && os_bind_active);
        lv_obj_t *card = f_rect(p, x, y, 142, 54, 0x193B65, 235, 18,
                                1, highlight ? 0x38BDF8 : 0x355C8C);
        lv_obj_t *circle = f_rect(card, 10, 11, 32, 32, qs_badge_bg[i], LV_OPA_COVER, 16,
                                  1, qs_badge_fg[i]);
        if (i == 0) {
            /* Symmetric 3-bar channel meter icon inside 30x30 inner box */
            (void)f_rect(circle, 6, 13, 4, 10, qs_badge_fg[0], LV_OPA_COVER, 1, 0, 0);
            (void)f_rect(circle, 13, 7, 4, 16, qs_badge_fg[0], LV_OPA_COVER, 1, 0, 0);
            (void)f_rect(circle, 20, 10, 4, 13, qs_badge_fg[0], LV_OPA_COVER, 1, 0, 0);
        } else {
            f_icon_centered(circle, qs_glyph[i], &lv_font_montserrat_14, qs_badge_fg[i]);
        }

        f_label(card, 48, 10, 88, qs_titles[i], &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
        f_label(card, 48, 29, 88, qs_subs[i], &lv_font_montserrat_12,
                highlight ? 0x5ECA62 : 0x94A3B8, LV_TEXT_ALIGN_LEFT);

        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, on_quickset_card_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

/* --- Sub-Page 6A: TX Power (Figma 2053:137) --- */
static void f_tx_power_select(int row) {
    if (row < 0 || row > 3) return;
    os_power_idx = row; /* 0=100mW, 1=250mW, 2=400mW, 3=500mW */
    esp_send_quickset_cmd((uint8_t)(0x00u + (uint8_t)row));
    os_config_save();
    f_invalidate_all_pages();
    f_open_page(FIGMA_PAGE_TX_POWER);
}

static void on_tx_power_row_cb(lv_event_t *e) {
    if (!click_allowed()) return;
    f_tx_power_select((int)(intptr_t)lv_event_get_user_data(e));
}

static void f_build_tx_power_page(lv_obj_t *p) {
    f_build_top_header(p, 1, "TX Power");
    for (int i = 0; i < 4; ++i) {
        int sel = (os_power_idx == i);
        int y = 44 + i * 46;
        lv_obj_t *row = f_rect(p, 24, y, 272, 38,
                               sel ? 0x1E4673 : 0x193B65, 235, 19,
                               1, sel ? 0x38BDF8 : 0x355C8C);
        lv_obj_t *chk = f_rect(row, 14, 10, 18, 18,
                               sel ? 0x3AE52A : 0x465160, LV_OPA_COVER, 9, 0, 0);
        if (sel) {
            f_icon_centered(chk, LV_SYMBOL_OK, &lv_font_montserrat_12, 0x0A1936);
        }
        char buf[20];
        snprintf(buf, sizeof(buf), "%umW", (unsigned)k_power_mw_table[i]);
        f_label(row, 44, 9, 184, buf, &lv_font_montserrat_14, 0xF0ECEF, LV_TEXT_ALIGN_CENTER);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, on_tx_power_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

/* --- Sub-Page 6B: ELRS Settings (Figma 2053:164) --- */
static void f_elrs_handle_row(int row) {
    if (row == 0) {
        os_elrs_rate_idx = (os_elrs_rate_idx + 1) % 4;
        esp_send_quickset_cmd((uint8_t)(0x10u + (uint8_t)os_elrs_rate_idx));
    } else if (row == 1) {
        os_telem_ratio_idx = (os_telem_ratio_idx + 1) % 4;
        esp_send_quickset_cmd((uint8_t)(0x50u + (uint8_t)os_telem_ratio_idx));
    } else if (row == 2) {
        os_model_match = !os_model_match;
        esp_send_quickset_cmd((uint8_t)(os_model_match ? 0x61u : 0x60u));
    }
    os_config_save();
    f_invalidate_page(FIGMA_PAGE_QUICKSET_MENU);
    f_invalidate_page(FIGMA_PAGE_ELRS_SET);
    f_open_page(FIGMA_PAGE_ELRS_SET);
}

static void on_elrs_row_cb(lv_event_t *e) {
    if (!click_allowed()) return;
    f_elrs_handle_row((int)(intptr_t)lv_event_get_user_data(e));
}

static void f_build_elrs_set_page(lv_obj_t *p) {
    f_build_top_header(p, 1, "ELRS Settings");

    char rate_buf[28];
    snprintf(rate_buf, sizeof(rate_buf), "%uHz  >", (unsigned)k_elrs_rate_table[os_elrs_rate_idx]);
    lv_obj_t *r0 = f_rect(p, 18, 48, 284, 48, 0x193B65, 235, 24, 1, 0x355C8C);
    f_label(r0, 18, 14, 120, "Packet Rate", &lv_font_montserrat_14, 0xF0ECEF, LV_TEXT_ALIGN_LEFT);
    f_label(r0, 146, 14, 120, rate_buf, &lv_font_montserrat_14, 0x38BDF8, LV_TEXT_ALIGN_RIGHT);
    lv_obj_add_flag(r0, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(r0, on_elrs_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);

    char tel_buf[20];
    snprintf(tel_buf, sizeof(tel_buf), "%s  >", k_telem_ratio_table[os_telem_ratio_idx]);
    lv_obj_t *r1 = f_rect(p, 18, 106, 284, 48, 0x193B65, 235, 24, 1, 0x355C8C);
    f_label(r1, 18, 14, 120, "Telem Ratio", &lv_font_montserrat_14, 0xF0ECEF, LV_TEXT_ALIGN_LEFT);
    f_label(r1, 146, 14, 120, tel_buf, &lv_font_montserrat_14, 0x38BDF8, LV_TEXT_ALIGN_RIGHT);
    lv_obj_add_flag(r1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(r1, on_elrs_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    lv_obj_t *r2 = f_rect(p, 18, 164, 284, 48, 0x193B65, 235, 24, 1, 0x355C8C);
    f_label(r2, 18, 14, 140, "Model Match", &lv_font_montserrat_14, 0xF0ECEF, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *sw_bg = f_rect(r2, 224, 12, 42, 24, os_model_match ? 0x0EA5E9 : 0x465160, LV_OPA_COVER, 12, 0, 0);
    (void)f_rect(sw_bg, os_model_match ? 20 : 2, 2, 20, 20, 0xFFFFFF, LV_OPA_COVER, 10, 0, 0);
    lv_obj_add_flag(r2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(r2, on_elrs_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)2);
}

/* --- Sub-Page 6C: RF Output (Figma 2070:5) --- */
static void f_rf_output_handle_row(int row) {
    if (row == 0) {
        os_rf_external = 0;
        esp_send_quickset_cmd(0x70u);
    } else if (row == 1) {
        os_rf_external = 1;
        esp_send_quickset_cmd(0x71u);
    } else if (row == 2) {
        os_rf_output_on = !os_rf_output_on;
        esp_send_quickset_cmd((uint8_t)(os_rf_output_on ? 0x31u : 0x30u));
    }
    os_config_save();
    f_invalidate_all_pages();
    f_open_page(FIGMA_PAGE_RF_OUTPUT);
}

static void on_rf_output_row_cb(lv_event_t *e) {
    if (!click_allowed()) return;
    f_rf_output_handle_row((int)(intptr_t)lv_event_get_user_data(e));
}

static void f_build_rf_output_page(lv_obj_t *p) {
    f_build_top_header(p, 1, "RF Output");

    int sel_int = !os_rf_external;
    lv_obj_t *r0 = f_rect(p, 22, 48, 276, 48, sel_int ? 0x1E4673 : 0x152B4A, 235, 24,
                          1, sel_int ? 0x38BDF8 : 0x2A4365);
    lv_obj_t *c0 = f_rect(r0, 14, 14, 20, 20, sel_int ? 0x3AE52A : 0x7C6C8A, LV_OPA_COVER, 10, 0, 0);
    if (sel_int) f_icon_centered(c0, LV_SYMBOL_OK, &lv_font_montserrat_12, 0x0A1936);
    f_label(r0, 44, 14, 200, "Internal TX", &lv_font_montserrat_14, sel_int ? 0xF0ECEF : 0x94A3B8, LV_TEXT_ALIGN_LEFT);
    lv_obj_add_flag(r0, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(r0, on_rf_output_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);

    int sel_ext = os_rf_external;
    lv_obj_t *r1 = f_rect(p, 22, 104, 276, 48, sel_ext ? 0x1E4673 : 0x152B4A, 235, 24,
                          1, sel_ext ? 0x38BDF8 : 0x2A4365);
    lv_obj_t *c1 = f_rect(r1, 14, 14, 20, 20, sel_ext ? 0x3AE52A : 0x7C6C8A, LV_OPA_COVER, 10, 0, 0);
    if (sel_ext) f_icon_centered(c1, LV_SYMBOL_OK, &lv_font_montserrat_12, 0x0A1936);
    f_label(r1, 44, 14, 200, "External TX", &lv_font_montserrat_14, sel_ext ? 0xF0ECEF : 0x94A3B8, LV_TEXT_ALIGN_LEFT);
    lv_obj_add_flag(r1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(r1, on_rf_output_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    lv_obj_t *r2 = f_rect(p, 22, 162, 276, 48, 0x193B65, 235, 24,
                          1, os_rf_output_on ? 0x3AE52A : 0xEF4444);
    f_label(r2, 20, 14, 140, "RF Power Out", &lv_font_montserrat_14, 0xF0ECEF, LV_TEXT_ALIGN_LEFT);
    f_label(r2, 166, 14, 48, os_rf_output_on ? "ON" : "OFF", &lv_font_montserrat_14,
            os_rf_output_on ? 0x5ECA62 : 0xEF4444, LV_TEXT_ALIGN_RIGHT);
    lv_obj_t *sw = f_rect(r2, 222, 12, 42, 24, os_rf_output_on ? 0x0EA5E9 : 0x465160, LV_OPA_COVER, 12, 0, 0);
    (void)f_rect(sw, os_rf_output_on ? 20 : 2, 2, 20, 20, 0xFFFFFF, LV_OPA_COVER, 10, 0, 0);
    lv_obj_add_flag(r2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(r2, on_rf_output_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)2);
}

/* --- Sub-Page 6D: Find Device (Figma 2041:337) --- */
static void f_find_toggle(void) {
    os_finder_active = !os_finder_active;
    esp_send_quickset_cmd((uint8_t)(os_finder_active ? 0x21u : 0x20u));
    f_invalidate_page(FIGMA_PAGE_HOME);
    f_invalidate_page(FIGMA_PAGE_QUICKSET_MENU);
    f_invalidate_page(FIGMA_PAGE_FIND);
    f_open_page(FIGMA_PAGE_FIND);
}

static void on_find_toggle_cb(lv_event_t *e) {
    (void)e;
    if (!click_allowed()) return;
    f_find_toggle();
}

static void f_build_find_page(lv_obj_t *p) {
    f_build_top_header(p, 1, "Find device");
    int link_ok = ui_endpoint.link.ready && ui_endpoint.state.valid;

    lv_obj_t *left = f_rect(p, 12, 36, 166, 192, 0x152E50, 235, 12, 1, 0x355C8C);
    f_label(left, 10, 8, 146, "DEVICE LOCATION", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *box = f_rect(left, 38, 38, 90, 80, 0x0F223D, LV_OPA_COVER, 12, 2,
                           os_finder_active ? 0x22C55E : 0x4A7AB0);
    f_label(box, 0, 28, 90, os_finder_active ? "BEEP!" : "SCAN",
            &lv_font_montserrat_14, os_finder_active ? 0x22C55E : 0x38BDF8, LV_TEXT_ALIGN_CENTER);
    f_label(left, 8, 132, 150, os_finder_active ? "Beacon Active" : "Ready to Find",
            &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    f_label(left, 8, 156, 150, link_ok ? "STM32 Synced" : "Standalone",
            &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *right = f_rect(p, 186, 36, 122, 192, 0x152E50, 235, 12, 1, 0x355C8C);
    f_label(right, 8, 12, 106, link_ok ? "RX Linked" : "No receiver",
            &lv_font_montserrat_14, link_ok ? 0x5ECA62 : 0xF59E0B, LV_TEXT_ALIGN_CENTER);
    f_label(right, 8, 44, 106, "Finder Beep:", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);
    f_label(right, 8, 62, 106, os_finder_active ? "ACTIVE" : "OFF",
            &lv_font_montserrat_14, os_finder_active ? 0x5ECA62 : 0xD8E0E8, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *btn = f_rect(right, 8, 140, 106, 40,
                           os_finder_active ? 0xC83233 : 0x23476E, LV_OPA_COVER, 10, 1,
                           os_finder_active ? 0xEF4444 : 0x38BDF8);
    f_label(btn, 0, 11, 106, os_finder_active ? "Stop Beep" : "Start Beep",
            &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, on_find_toggle_cb, LV_EVENT_CLICKED, NULL);
}

/* --- Sub-Page 6E: Bind Receiver (Figma 2041:764) --- */
static void f_bind_toggle(void) {
    os_bind_active = !os_bind_active;
    esp_send_quickset_cmd((uint8_t)(os_bind_active ? 0x41u : 0x40u));
    f_invalidate_page(FIGMA_PAGE_HOME);
    f_invalidate_page(FIGMA_PAGE_QUICKSET_MENU);
    f_invalidate_page(FIGMA_PAGE_BIND);
    f_open_page(FIGMA_PAGE_BIND);
}

static void on_bind_toggle_cb(lv_event_t *e) {
    (void)e;
    if (!click_allowed()) return;
    f_bind_toggle();
}

static void f_build_bind_page(lv_obj_t *p) {
    f_build_top_header(p, 1, "Bind receiver");

    lv_obj_t *left = f_rect(p, 12, 36, 166, 192, 0x152E50, 235, 12, 1, 0x355C8C);
    f_label(left, 10, 8, 146, "RECEIVER PAIRING", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *ring = f_rect(left, 45, 38, 76, 76, 0x0F223D, LV_OPA_COVER, 38, 4,
                            os_bind_active ? 0xF97316 : 0x38BDF8);
    f_label(ring, 0, 26, 76, os_bind_active ? "BIND" : "CRSF",
            &lv_font_montserrat_14, os_bind_active ? 0xF97316 : 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    f_label(left, 8, 128, 150, os_bind_active ? "Binding RX..." : "Waiting for RX",
            &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    f_label(left, 8, 154, 150, os_bind_active ? "ELRS Pulse Sent" : "Tap Start Bind",
            &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *right = f_rect(p, 186, 36, 122, 192, 0x152E50, 235, 12, 1, 0x355C8C);
    f_label(right, 8, 10, 106, "Before pair", &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
    f_label(right, 8, 36, 106, "1. Power RX", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_LEFT);
    f_label(right, 8, 56, 106, "2. Pair mode", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_LEFT);
    f_label(right, 8, 76, 106, "3. Keep near", &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *btn = f_rect(right, 8, 140, 106, 40,
                           os_bind_active ? 0xC83233 : 0x23476E, LV_OPA_COVER, 10, 1,
                           os_bind_active ? 0xEF4444 : 0xF97316);
    f_label(btn, 0, 11, 106, os_bind_active ? "Cancel" : "Start Bind",
            &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, on_bind_toggle_cb, LV_EVENT_CLICKED, NULL);
}

/* ============================================================================
 * PAGE 7: TYVOR SETTINGS MENU (Swipe Right from QuickSet; Swipe Right -> About)
 * ========================================================================== */
static void f_stick_cal_toggle(void) {
    if (!os_stick_cal_active) {
        os_stick_cal_active = 1;
        for (int i = 0; i < 4; ++i) {
            int32_t v = gimbal_adc_filt[i];
            os_cal_live_min[i] = v;
            os_cal_live_max[i] = v;
        }
    } else {
        os_stick_cal_active = 0;
        for (int i = 0; i < 4; ++i) {
            if (os_cal_live_max[i] - os_cal_live_min[i] > 300) {
                gimbal_min_cal[i] = os_cal_live_min[i];
                gimbal_max_cal[i] = os_cal_live_max[i];
            }
        }
        os_cal_done_flash = 1;
        os_config_save();
    }
    f_invalidate_page(FIGMA_PAGE_STICK_CAL);
    f_invalidate_page(FIGMA_PAGE_SETTINGS_MENU);
    f_open_page(FIGMA_PAGE_STICK_CAL);
}

static void f_stick_cal_reset(void) {
    os_stick_cal_active = 0;
    for (int i = 0; i < 4; ++i) {
        gimbal_min_cal[i] = 350;
        gimbal_max_cal[i] = 3745;
        os_cal_live_min[i] = 350;
        os_cal_live_max[i] = 3745;
    }
    os_cal_done_flash = 1;
    os_config_save();
    f_invalidate_page(FIGMA_PAGE_STICK_CAL);
    f_invalidate_page(FIGMA_PAGE_SETTINGS_MENU);
    f_open_page(FIGMA_PAGE_STICK_CAL);
}

static void on_stick_cal_btn_cb(lv_event_t *e) {
    if (!click_allowed()) return;
    int act = (int)(intptr_t)lv_event_get_user_data(e);
    if (act == 0) f_stick_cal_toggle();
    else f_stick_cal_reset();
}

static void f_build_stick_cal_page(lv_obj_t *p) {
    f_build_top_header(p, 1, os_lang_vi ? "Hieu chuan" : "Stick Cal");

    static const char *cal_names[4] = {"CH1 Roll", "CH2 Pitch", "CH3 Thr", "CH4 Yaw"};
    static const uint32_t cal_cols[4] = {0x38BDF8, 0x38BDF8, 0xEF4444, 0x22C55E};

    f_label(p, 14, 30, 292,
            os_stick_cal_active
                ? (os_lang_vi ? "Xoay 2 can het bien do roi bam Luu" : "Move sticks to all corners, then Save")
                : (os_lang_vi ? "Bam Bat dau de do min/max thuc te" : "Tap Start Cal to record gimbal limits"),
            &lv_font_montserrat_12, os_stick_cal_active ? 0xFBBF24 : 0x94A3B8, LV_TEXT_ALIGN_LEFT);

    for (int i = 0; i < 4; ++i) {
        int y = 48 + i * 33;
        lv_obj_t *row = f_rect(p, 14, y, 292, 29, 0x142238, 235, 8, 1, 0x263B59);
        f_label(row, 8, 7, 74, cal_names[i], &lv_font_montserrat_12, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);

        lv_obj_t *track = f_rect(row, 84, 10, 86, 9, 0x0F172A, LV_OPA_COVER, 4, 0, 0);
        w_cal_fill[i] = f_rect(track, 0, 0, 43, 9, cal_cols[i], LV_OPA_COVER, 4, 0, 0);

        w_cal_pct[i] = f_label(row, 174, 7, 38, "0%", &lv_font_montserrat_12, 0x5ECA62, LV_TEXT_ALIGN_RIGHT);
        char rbuf[24];
        int32_t mn = os_stick_cal_active ? os_cal_live_min[i] : gimbal_min_cal[i];
        int32_t mx = os_stick_cal_active ? os_cal_live_max[i] : gimbal_max_cal[i];
        snprintf(rbuf, sizeof(rbuf), "%ld-%ld", (long)mn, (long)mx);
        w_cal_range[i] = f_label(row, 214, 7, 72, rbuf, &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_RIGHT);
    }

    lv_obj_t *btn_start = f_rect(p, 14, 188, 140, 38,
                                 os_stick_cal_active ? 0x16A34A : 0x0284C7,
                                 LV_OPA_COVER, 12, 1, 0x38BDF8);
    f_label(btn_start, 0, 11, 140,
            os_stick_cal_active ? (os_lang_vi ? "Luu Cal" : "Save Cal")
                                : (os_lang_vi ? "Bat dau Cal" : "Start Cal"),
            &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(btn_start, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_start, on_stick_cal_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);

    lv_obj_t *btn_rst = f_rect(p, 166, 188, 140, 38, 0x1E293B, LV_OPA_COVER, 12, 1, 0x475569);
    f_label(btn_rst, 0, 11, 140, os_lang_vi ? "Mac dinh" : "Reset Default",
            &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(btn_rst, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_rst, on_stick_cal_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
}

static void f_scr_saver_select(int idx) {
    if (idx >= 0 && idx < 5) {
        os_sleep_timeout_sec = k_sleep_timeout_table[idx];
        ESP_LOGI("SLEEP", "User selected auto-sleep=%us", (unsigned)os_sleep_timeout_sec);
        os_config_save();
        f_invalidate_page(FIGMA_PAGE_SETTINGS_MENU);
        f_invalidate_page(FIGMA_PAGE_SCR_SAVER);
        f_open_page(FIGMA_PAGE_SCR_SAVER);
    } else if (idx == 5) {
        sleep_active = 1;
        f_open_page(FIGMA_PAGE_SLEEP_SAVER);
    }
}

static void on_scr_saver_cb(lv_event_t *e) {
    if (!click_allowed()) return;
    f_scr_saver_select((int)(intptr_t)lv_event_get_user_data(e));
}

static void f_build_scr_saver_page(lv_obj_t *p) {
    f_build_top_header(p, 1, os_lang_vi ? "Man hinh cho" : "Scr Saver");

    f_label(p, 18, 34, 284,
            os_lang_vi ? "Thoi gian tu dong Sleep (Logo Cu):" : "Auto-Sleep Idle Timeout:",
            &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_LEFT);

    static const char *t_labels[5] = {"15s", "30s", "60s", "120s", "OFF"};
    for (int i = 0; i < 5; ++i) {
        int sel = (os_sleep_timeout_sec == k_sleep_timeout_table[i]);
        int x = 18 + i * 58;
        lv_obj_t *pill = f_rect(p, x, 58, 52, 42,
                                sel ? 0x1E4673 : 0x142238, LV_OPA_COVER, 10,
                                1, sel ? 0x38BDF8 : 0x263B59);
        f_icon_centered(pill, t_labels[i], &lv_font_montserrat_14, sel ? 0xFFFFFF : 0x94A3B8);
        lv_obj_add_flag(pill, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(pill, on_scr_saver_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    lv_obj_t *info = f_rect(p, 18, 114, 284, 52, 0x142238, 235, 12, 1, 0x263B59);
    char ibuf[48];
    if (os_sleep_timeout_sec == 0) {
        snprintf(ibuf, sizeof(ibuf), os_lang_vi ? "Tu dong Sleep: TAT" : "Auto-Sleep: Disabled");
    } else {
        snprintf(ibuf, sizeof(ibuf), os_lang_vi ? "Sleep sau %us khong thao tac" : "Sleep after %us idle",
                 (unsigned)os_sleep_timeout_sec);
    }
    f_label(info, 12, 8, 260, ibuf, &lv_font_montserrat_14, 0x38BDF8, LV_TEXT_ALIGN_LEFT);
    f_label(info, 12, 28, 260,
            os_lang_vi ? "Hien Logo Cu TYVOR chop mat & gian du" : "Shows TYVOR Owl blink & angry glare",
            &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *test_btn = f_rect(p, 18, 180, 284, 42, 0x0284C7, LV_OPA_COVER, 12, 1, 0x38BDF8);
    f_icon_centered(test_btn, os_lang_vi ? "Thu Sleep Ngay" : "Test Sleep Now",
                    &lv_font_montserrat_14, 0xFFFFFF);
    lv_obj_add_flag(test_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(test_btn, on_scr_saver_cb, LV_EVENT_CLICKED, (void *)(intptr_t)5);
}

static void f_stick_mode_select(int mode) {
    os_stick_mode = (mode == 2) ? 2 : 1;
    esp_send_quickset_cmd((os_stick_mode == 2) ? 0x91u : 0x90u);
    os_config_save();
    f_invalidate_page(FIGMA_PAGE_SETTINGS_MENU);
    f_invalidate_page(FIGMA_PAGE_STICK_MODE);
    f_open_page(FIGMA_PAGE_STICK_MODE);
}

static void on_stick_mode_cb(lv_event_t *e) {
    if (!click_allowed()) return;
    f_stick_mode_select((int)(intptr_t)lv_event_get_user_data(e));
}

static void f_build_stick_mode_page(lv_obj_t *p) {
    f_build_top_header(p, 1, "Stick Mode");

    for (int m = 1; m <= 2; ++m) {
        int active = (os_stick_mode == m);
        int x = (m == 1) ? 14 : 166;
        lv_obj_t *card = f_rect(p, x, 36, 140, 152,
                                active ? 0x163A63 : 0x152E50, 235, 14,
                                active ? 2 : 1,
                                active ? 0x38BDF8 : 0x355C8C);
        char title[16];
        snprintf(title, sizeof(title), "MODE %d", m);
        f_label(card, 8, 8, 124, title, &lv_font_montserrat_14,
                active ? 0x38BDF8 : 0xFFFFFF, LV_TEXT_ALIGN_CENTER);

        /* Left & Right Gimbal Crosshair Diagrams */
        for (int g = 0; g < 2; ++g) {
            int gx = (g == 0) ? 14 : 78;
            lv_obj_t *ring = f_rect(card, gx, 34, 48, 48, 0x0A1C36, LV_OPA_COVER, 24, 1,
                                    active ? 0x38BDF8 : 0x4A7AB0);
            (void)f_rect(ring, 23, 8, 2, 32, 0x475569, LV_OPA_COVER, 1, 0, 0);
            (void)f_rect(ring, 8, 23, 32, 2, 0x475569, LV_OPA_COVER, 1, 0, 0);
            (void)f_rect(ring, 20, 20, 8, 8, active ? 0x38BDF8 : 0x94A3B8, LV_OPA_COVER, 4, 0, 0);
        }

        const char *l_lbl = (m == 1) ? "L: YAW/PIT" : "L: YAW/THR";
        const char *r_lbl = (m == 1) ? "R: ROL/THR" : "R: ROL/PIT";
        f_label(card, 6, 90, 128, l_lbl, &lv_font_montserrat_12, 0xCBD5E1, LV_TEXT_ALIGN_CENTER);
        f_label(card, 6, 108, 128, r_lbl, &lv_font_montserrat_12, 0xCBD5E1, LV_TEXT_ALIGN_CENTER);

        lv_obj_t *badge = f_rect(card, 24, 126, 92, 20,
                                 active ? 0x0284C7 : 0x1E293B, LV_OPA_COVER, 8, 0, 0);
        f_label(badge, 0, 2, 92, active ? "ACTIVE" : "SELECT",
                &lv_font_montserrat_12, active ? 0xFFFFFF : 0x94A3B8, LV_TEXT_ALIGN_CENTER);

        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, on_stick_mode_cb, LV_EVENT_CLICKED, (void *)(intptr_t)m);
    }

    f_label(p, 14, 200, 292,
            os_lang_vi ? "Che do tay cam tu dong dong bo STM32" : "Stick mode syncs directly with STM32 Core",
            &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_CENTER);
}

static void f_settings_handle_card(int idx) {
    if (idx == 0) {
        nav_return_page = FIGMA_PAGE_SETTINGS_MENU;
        active_non_sleep_page = FIGMA_PAGE_CHANNELS;
        tyvor_trans_start_animated(FIGMA_PAGE_CHANNELS, TYVOR_SWIPE_RIGHT);
        return;
    } else if (idx == 1) {
        os_lang_vi = !os_lang_vi;
        f_invalidate_page(FIGMA_PAGE_STICK_CAL);
        f_invalidate_page(FIGMA_PAGE_STICK_MODE);
        f_invalidate_page(FIGMA_PAGE_SCR_SAVER);
    } else if (idx == 2) {
        active_non_sleep_page = FIGMA_PAGE_STICK_CAL;
        f_invalidate_page(FIGMA_PAGE_STICK_CAL);
        f_open_page(FIGMA_PAGE_STICK_CAL);
        return;
    } else if (idx == 3) {
        active_non_sleep_page = FIGMA_PAGE_STICK_MODE;
        f_invalidate_page(FIGMA_PAGE_STICK_MODE);
        f_open_page(FIGMA_PAGE_STICK_MODE);
        return;
    } else if (idx == 4) {
        active_non_sleep_page = FIGMA_PAGE_SCR_SAVER;
        f_invalidate_page(FIGMA_PAGE_SCR_SAVER);
        f_open_page(FIGMA_PAGE_SCR_SAVER);
        return;
    } else if (idx == 5) {
        os_thr_lock_on = !os_thr_lock_on;
        os_qs_local_grace_ms = millis() + 2500;
        esp_send_quickset_cmd(os_thr_lock_on ? 0x81u : 0x80u);
    }
    os_config_save();
    f_invalidate_page(FIGMA_PAGE_SETTINGS_MENU);
    f_open_page(FIGMA_PAGE_SETTINGS_MENU);
}

static void on_settings_card_cb(lv_event_t *e) {
    if (!click_allowed()) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    f_settings_handle_card(idx);
}

static void f_build_settings_menu_page(lv_obj_t *p) {
    f_build_menu_row_header(p, FIGMA_PAGE_SETTINGS_MENU);

    const char *st_titles[6] = {
        os_lang_vi ? "Kenh TX" : "Channels",
        os_lang_vi ? "Ngon ngu" : "Language",
        os_lang_vi ? "Chinh can" : "Stick Cal",
        os_lang_vi ? "Che do can" : "Stick Mode",
        os_lang_vi ? "Man hinh cho" : "Scr Saver",
        os_lang_vi ? "Khoa ga" : "THR Lock"
    };
    char scr_sub[24];
    if (os_sleep_timeout_sec == 0) snprintf(scr_sub, sizeof(scr_sub), "OFF");
    else snprintf(scr_sub, sizeof(scr_sub), "%us Sleep", (unsigned)os_sleep_timeout_sec);

    const char *st_subs[6] = {
        os_lang_vi ? "6 Kenh Live" : "6 CH Setup",
        os_lang_vi ? "Tieng Viet" : "English",
        os_cal_done_flash ? (os_lang_vi ? "Da luu OK!" : "Saved OK!")
                          : (os_lang_vi ? "Chinh Min/Max" : "Calibrate"),
        (os_stick_mode == 2) ? "Mode 2 (THR-L)" : "Mode 1 (THR-R)",
        scr_sub,
        os_thr_lock_on ? (os_lang_vi ? "Dang Khoa" : "Active")
                       : (os_lang_vi ? "Da Mo" : "Unlocked")
    };
    static const uint32_t st_bg[6] = {
        0x1E3A5F, 0x1E3A5F, 0x0D3B2E, 0x3E2723, 0x2E1A47, 0x3B1A1A
    };
    static const uint32_t st_fg[6] = {
        0x38BDF8, 0x6366F1, 0x10B981, 0xF59E0B, 0xA855F7, 0xEF4444
    };
    static const char *st_glyph[6] = {
        NULL,
        LV_SYMBOL_KEYBOARD,
        NULL,
        LV_SYMBOL_SHUFFLE,
        LV_SYMBOL_EYE_OPEN,
        LV_SYMBOL_WARNING
    };

    for (int i = 0; i < 6; ++i) {
        int col = i % 2;
        int row = i / 2;
        int x = 14 + col * 150;
        int y = 36 + row * 64;

        int highlight = (i == 5 && os_thr_lock_on);
        lv_obj_t *card = f_rect(p, x, y, 142, 54, 0x142238, 235, 14, 1,
                                highlight ? 0xEF4444 : 0x263B59);
        lv_obj_t *circle = f_rect(card, 8, 11, 32, 32, st_bg[i], LV_OPA_COVER, 16, 1, st_fg[i]);
        if (i == 0) {
            /* Symmetric 3-bar channel meter icon inside 30x30 inner box */
            (void)f_rect(circle, 6, 13, 4, 10, st_fg[0], LV_OPA_COVER, 1, 0, 0);
            (void)f_rect(circle, 13, 7, 4, 16, st_fg[0], LV_OPA_COVER, 1, 0, 0);
            (void)f_rect(circle, 20, 10, 4, 13, st_fg[0], LV_OPA_COVER, 1, 0, 0);
        } else if (i == 2) {
            /* Symmetric gimbal crosshair target icon inside 30x30 inner box */
            lv_obj_t *ring = f_rect(circle, 7, 7, 16, 16, st_bg[2], LV_OPA_TRANSP, 8, 1, st_fg[2]);
            (void)f_rect(ring, 5, 5, 4, 4, st_fg[2], LV_OPA_COVER, 2, 0, 0);
            (void)f_rect(circle, 14, 2, 2, 4, st_fg[2], LV_OPA_COVER, 1, 0, 0);
            (void)f_rect(circle, 14, 24, 2, 4, st_fg[2], LV_OPA_COVER, 1, 0, 0);
            (void)f_rect(circle, 2, 14, 4, 2, st_fg[2], LV_OPA_COVER, 1, 0, 0);
            (void)f_rect(circle, 24, 14, 4, 2, st_fg[2], LV_OPA_COVER, 1, 0, 0);
        } else {
            f_icon_centered(circle, st_glyph[i], &lv_font_montserrat_14, st_fg[i]);
        }

        f_label(card, 46, 9, 90, st_titles[i], &lv_font_montserrat_14, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
        f_label(card, 46, 28, 90, st_subs[i], &lv_font_montserrat_12,
                highlight ? 0xFCA5A5 : 0x94A3B8, LV_TEXT_ALIGN_LEFT);

        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, on_settings_card_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

/* ============================================================================
 * PAGE 8: TYVOR ABOUT PAGE (Swipe Right from Settings — Figma Node 2070:287)
 * ========================================================================== */
static void f_build_about_page(lv_obj_t *p) {
    f_build_menu_row_header(p, FIGMA_PAGE_ABOUT);

    /* Main About Hero Card with TYVOR Owl Emblem & Brand */
    lv_obj_t *hero = f_rect(p, 14, 34, 292, 68, 0x142238, 240, 14, 1, 0x38BDF8);
    lv_obj_t *owl = lv_image_create(hero);
    lv_image_set_src(owl, &tyvor_ui321_owl_large);
    lv_obj_set_pos(owl, 14, 8);
    lv_obj_set_size(owl, (int)tyvor_ui321_owl_large.header.w, (int)tyvor_ui321_owl_large.header.h);
    lv_obj_set_style_image_recolor(owl, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_image_recolor_opa(owl, LV_OPA_COVER, 0);

    f_tyvor_logo(hero, 64, 10, 0xFFFFFF);
    f_label(hero, 64, 30, 216, "TYVOR Transmitter OS v1.0.0",
            &lv_font_montserrat_12, 0x38BDF8, LV_TEXT_ALIGN_LEFT);
    f_label(hero, 64, 46, 216, "Dual-MCU: STM32F407 + ESP32-S3",
            &lv_font_montserrat_12, 0x94A3B8, LV_TEXT_ALIGN_LEFT);

    /* 4 Live System Specification Cards */
    static const char *ab_titles[4] = {"ELRS Firmware", "Active Model", "Gimbal ADC1", "Protocol v2"};
    char elrs_info[32], mod_info[32], adc_info[32], link_info[32];
    snprintf(elrs_info, sizeof(elrs_info), "v3.6.3 | %umW", (unsigned)k_power_mw_table[os_power_idx]);
    snprintf(mod_info, sizeof(mod_info), "%s (%s)",
             k_os_models[os_active_model].tab_name,
             k_os_models[os_active_model].mode_name);
    int n_conn = (int)gimbal_connected[0] + (int)gimbal_connected[1] +
                 (int)gimbal_connected[2] + (int)gimbal_connected[3];
    snprintf(adc_info, sizeof(adc_info), "%d/4 Axes (M%d)", n_conn, os_stick_mode);
    snprintf(link_info, sizeof(link_info), "%s (%" PRIu32 "f)",
             ui_endpoint.link.ready ? "100Hz OK" : "Standalone",
             ui_endpoint.link.parser.stats.frames);

    const char *ab_vals[4] = {elrs_info, mod_info, adc_info, link_info};
    for (int i = 0; i < 4; ++i) {
        int col = i % 2;
        int row = i / 2;
        int x = 14 + col * 150;
        int y = 110 + row * 60;

        lv_obj_t *card = f_rect(p, x, y, 142, 52, 0x142238, 235, 12, 1, 0x263B59);
        f_label(card, 10, 8, 122, ab_titles[i], &lv_font_montserrat_12, 0x64748B, LV_TEXT_ALIGN_LEFT);
        w_about_vals[i] = f_label(card, 10, 26, 122, ab_vals[i], &lv_font_montserrat_12, 0x5ECA62, LV_TEXT_ALIGN_LEFT);
    }
}

/* ============================================================================
 * PAGE 9 (AND IDLE SLEEP MODE): OFFICIAL TYVOR LOGO & PREDATORY OWL FACE
 * ========================================================================== */
static void f_build_sleep_saver_page(lv_obj_t *p) {
    /* Seamless deep-night background matching tyvor_boot_logo_base (0x07101E) */
    w_sleep_card = f_rect(p, 0, 0, 320, 240, 0x07101E, LV_OPA_COVER, 0, 0, 0);

    /* Centered Official TYVOR Owl Face + Logo (220x153 RGB565 with multi-mood eyes) */
    set_boot_eye_blink_stage(0);
    last_sleep_blink_stage = 0;
    last_sleep_zzz_step = -1;
    w_sleep_owl_img = lv_image_create(w_sleep_card);
    lv_obj_remove_style_all(w_sleep_owl_img);
    lv_image_set_src(w_sleep_owl_img, &tyvor_boot_logo_dsc);
    lv_obj_set_pos(w_sleep_owl_img, (320 - TYVOR_BOOT_LOGO_W) / 2, (240 - TYVOR_BOOT_LOGO_H) / 2);
    lv_obj_set_size(w_sleep_owl_img, TYVOR_BOOT_LOGO_W, TYVOR_BOOT_LOGO_H);

    /* Floating "z z Z" sleep indicators (visible during sleep stage) */
    w_sleep_zzz[0] = f_label(w_sleep_card, 212, 77, 20, "z", &lv_font_montserrat_12, 0x38BDF8, LV_TEXT_ALIGN_CENTER);
    w_sleep_zzz[1] = f_label(w_sleep_card, 224, 63, 20, "z", &lv_font_montserrat_14, 0x7DD3FC, LV_TEXT_ALIGN_CENTER);
    w_sleep_zzz[2] = f_label(w_sleep_card, 238, 47, 24, "Z", &lv_font_montserrat_14, 0xE0F2FE, LV_TEXT_ALIGN_CENTER);
    for (int i = 0; i < 3; ++i) lv_obj_add_flag(w_sleep_zzz[i], LV_OBJ_FLAG_HIDDEN);

    /* Happy sparkles (outer empty dark margins, zero logo overlap) */
    w_sleep_happy[0] = f_label(w_sleep_card, 26, 71, 20, "+", &lv_font_montserrat_14, 0x38BDF8, LV_TEXT_ALIGN_CENTER);
    w_sleep_happy[1] = f_label(w_sleep_card, 268, 59, 20, "*", &lv_font_montserrat_14, 0xFDE047, LV_TEXT_ALIGN_CENTER);
    w_sleep_happy[2] = f_label(w_sleep_card, 276, 85, 16, "+", &lv_font_montserrat_12, 0x7DD3FC, LV_TEXT_ALIGN_CENTER);
    for (int i = 0; i < 3; ++i) lv_obj_add_flag(w_sleep_happy[i], LV_OBJ_FLAG_HIDDEN);

    /* Sad falling cyan tear droplets (outer left margin) */
    w_sleep_sad[0] = f_rect(w_sleep_card, 38, 89, 4, 6, 0x38BDF8, LV_OPA_COVER, 2, 0, 0);
    w_sleep_sad[1] = f_rect(w_sleep_card, 44, 101, 3, 5, 0x7DD3FC, LV_OPA_COVER, 2, 0, 0);
    for (int i = 0; i < 2; ++i) lv_obj_add_flag(w_sleep_sad[i], LV_OBJ_FLAG_HIDDEN);

    /* Angry pulsing 4-bar spark mark (upper-right empty margin) */
    w_sleep_angry[0] = f_rect(w_sleep_card, 250, 57, 8, 3, 0xEF4444, LV_OPA_COVER, 1, 0, 0);
    w_sleep_angry[1] = f_rect(w_sleep_card, 260, 63, 3, 8, 0xF59E0B, LV_OPA_COVER, 1, 0, 0);
    w_sleep_angry[2] = f_rect(w_sleep_card, 246, 63, 3, 8, 0xF59E0B, LV_OPA_COVER, 1, 0, 0);
    w_sleep_angry[3] = f_rect(w_sleep_card, 250, 73, 8, 3, 0xEF4444, LV_OPA_COVER, 1, 0, 0);
    for (int i = 0; i < 4; ++i) lv_obj_add_flag(w_sleep_angry[i], LV_OBJ_FLAG_HIDDEN);
    last_sleep_mood_tick = -1;
}

/* Ensure persistent root & specific page container are built without reloading other screens */
static void f_ensure_page_built(unsigned page) {
    if (page >= FIGMA_PAGE_COUNT) return;

    if (!figma_root) {
        figma_root = lv_obj_create(oxbot_ui.root);
        lv_obj_remove_style_all(figma_root);
        lv_obj_set_pos(figma_root, 0, 0);
        lv_obj_set_size(figma_root, 320, 240);
        lv_obj_remove_flag(figma_root, LV_OBJ_FLAG_SCROLLABLE);

        /* Solid Deep Navy Background (fast opaque fill, zero per-line gradient cost) */
        lv_obj_set_style_bg_color(figma_root, lv_color_hex(0x0B1E3D), 0);
        lv_obj_set_style_bg_opa(figma_root, LV_OPA_COVER, 0);
    }

    if (!page_containers[page]) {
        lv_obj_t *cont = lv_obj_create(figma_root);
        lv_obj_remove_style_all(cont);
        lv_obj_set_pos(cont, 0, 0);
        lv_obj_set_size(cont, 320, 240);
        lv_obj_set_style_bg_color(cont, lv_color_hex(0x0B1E3D), 0);
        lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
        lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(cont, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
        page_containers[page] = cont;
        page_built[page] = 0;
    }

    if (!page_built[page]) {
        lv_obj_clean(page_containers[page]);
        reset_page_widgets(page);
        g_building_page = page;

        switch (page) {
        case FIGMA_PAGE_HOME:
            f_build_home_page(page_containers[page]);
            break;
        case FIGMA_PAGE_MODELS:
            f_build_models_page(page_containers[page]);
            break;
        case FIGMA_PAGE_MODEL_LAYOUT:
            f_build_model_layout_page(page_containers[page]);
            break;
        case FIGMA_PAGE_CH_VIEW:
            f_build_ch_view_page(page_containers[page]);
            break;
        case FIGMA_PAGE_CHANNELS:
            f_build_channels_page(page_containers[page]);
            break;
        case FIGMA_PAGE_TELEMETRY:
            f_build_telemetry_page(page_containers[page]);
            break;
        case FIGMA_PAGE_QUICK_SETTINGS:
            f_build_quick_settings_page(page_containers[page]);
            break;
        case FIGMA_PAGE_QUICKSET_MENU:
            f_build_quickset_menu_page(page_containers[page]);
            break;
        case FIGMA_PAGE_TX_POWER:
            f_build_tx_power_page(page_containers[page]);
            break;
        case FIGMA_PAGE_ELRS_SET:
            f_build_elrs_set_page(page_containers[page]);
            break;
        case FIGMA_PAGE_RF_OUTPUT:
            f_build_rf_output_page(page_containers[page]);
            break;
        case FIGMA_PAGE_FIND:
            f_build_find_page(page_containers[page]);
            break;
        case FIGMA_PAGE_BIND:
            f_build_bind_page(page_containers[page]);
            break;
        case FIGMA_PAGE_SETTINGS_MENU:
            f_build_settings_menu_page(page_containers[page]);
            break;
        case FIGMA_PAGE_STICK_CAL:
            f_build_stick_cal_page(page_containers[page]);
            break;
        case FIGMA_PAGE_STICK_MODE:
            f_build_stick_mode_page(page_containers[page]);
            break;
        case FIGMA_PAGE_SCR_SAVER:
            f_build_scr_saver_page(page_containers[page]);
            break;
        case FIGMA_PAGE_ABOUT:
            f_build_about_page(page_containers[page]);
            break;
        case FIGMA_PAGE_SLEEP_SAVER:
        default:
            f_build_sleep_saver_page(page_containers[page]);
            break;
        }
        page_built[page] = 1;
    }
}

static void on_thr_warn_dismiss_cb(lv_event_t *e) {
    (void)e;
    if (!click_allowed()) return;
    thr_warn_dismissed = 1;
    if (w_thr_warn_overlay) {
        lv_obj_delete_async(w_thr_warn_overlay);
        w_thr_warn_overlay = NULL;
        w_thr_warn_bar = NULL;
        w_thr_warn_pct = NULL;
    }
}

static int in_lvgl_handler = 0;
static int deferred_open_page = -1;
static int deferred_enter_x = 0;
static int deferred_enter_y = 0;

static void f_open_page_dir(unsigned page, int enter_x, int enter_y) {
    page %= FIGMA_PAGE_COUNT;
    if (in_lvgl_handler && page_containers[page] && !page_built[page]) {
        /* Defer lv_obj_clean() until lv_timer_handler() finishes to prevent LVGL use-after-free */
        deferred_open_page = (int)page;
        deferred_enter_x = enter_x;
        deferred_enter_y = enter_y;
        return;
    }
    unsigned prev_p = figma_page;
    f_ensure_page_built(page);

    figma_page        = page;
    if (page != FIGMA_PAGE_SLEEP_SAVER) {
        last_user_activity_ms = millis();
        sleep_active = 0;
    }
    g_trans.state     = TYVOR_TRANS_IDLE;
    g_trans.from_page = page;
    g_trans.to_page   = TYVOR_PAGE_NONE;
    g_trans.offset_px = 0;

    if (page_containers[page] && (enter_x != 0 || enter_y != 0) && prev_p != page &&
        prev_p < FIGMA_PAGE_COUNT && page_containers[prev_p]) {
        lv_obj_remove_flag(page_containers[prev_p], LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(page_containers[page], LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(page_containers[page]);
        if (enter_x != 0) f_animate_page_slide(page_containers[page], 1, enter_x, 0, 150);
        else f_animate_page_slide(page_containers[page], 0, enter_y, 0, 150);
    } else {
        if (page_containers[page]) {
            lv_obj_set_pos(page_containers[page], 0, 0);
        }
        tyvor_trans_hide_inactive_pages(page, TYVOR_PAGE_NONE);
    }
}

static void f_open_page(unsigned page) {
    f_open_page_dir(page, 0, 0);
}

static int ch_raw_to_pct(int16_t raw) {
    /* TYVOR Core channels are -1000..+1000 */
    int pct = (int)raw / 10;
    if (pct < -100) pct = -100;
    if (pct > 100) pct = 100;
    return pct;
}

static void f_update_live_ui(uint64_t now) {
    static unsigned prev_page = 0xFFu;
    static int prev_sig_ok = -1;
    static uint64_t prev_sec = UINT64_MAX;
    static int prev_armed = -1;
    static int prev_sats = -1;
    static int prev_rol = 9999, prev_pit = 9999, prev_thr = 9999, prev_yaw = 9999;
    static int prev_sig_mode = -1;

    int page_changed = (prev_page != figma_page);
    if (page_changed) {
        prev_page = figma_page;
        prev_sig_ok = -1;
        prev_sec = UINT64_MAX;
        prev_armed = -1;
        prev_sats = -1;
        prev_rol = 9999;
        prev_sig_mode = -1;
    }

    const tyvor_core_snapshot_t *snap = &ui_endpoint.state.snapshot;
    int link_ok = ui_endpoint.link.ready && ui_endpoint.state.valid;
    if (link_ok && now >= os_qs_local_grace_ms) {
        if ((int)snap->throttle_lock != os_thr_lock_on) {
            os_thr_lock_on = (int)snap->throttle_lock;
            f_invalidate_page(FIGMA_PAGE_SETTINGS_MENU);
        }
        if ((int)(snap->rf_enabled != 0u) != os_rf_output_on) {
            os_rf_output_on = (snap->rf_enabled != 0u) ? 1 : 0;
            f_invalidate_page(FIGMA_PAGE_QUICKSET_MENU);
            f_invalidate_page(FIGMA_PAGE_RF_OUTPUT);
        }
    }
    int local_gimbal = any_local_gimbal_connected();
    /* Strict STM32 Safety Authority: ARM and RF ON are controlled exclusively by STM32 Core */
    int armed = link_ok ? (snap->safety_state == TYVOR_BENCH_ARMED) : 0;
    int rf_active = link_ok ? (snap->rf_enabled != 0u) : 0;

    /* 1. Update Shared Top Header on active page (only when signal state changes) */
    unsigned cur_p = (figma_page < FIGMA_PAGE_COUNT) ? figma_page : 0;
    static int prev_rf_hdr = -1;
    if (rf_active != prev_rf_hdr || page_changed) {
        prev_rf_hdr = rf_active;
        if (w_hdr_power_lbl[cur_p]) {
            char pbuf[16];
            if (!rf_active) snprintf(pbuf, sizeof(pbuf), "RF OFF");
            else snprintf(pbuf, sizeof(pbuf), "%u mW", (unsigned)k_power_mw_table[os_power_idx]);
            tyvor_ui321_text(w_hdr_power_lbl[cur_p], pbuf);
            lv_obj_set_style_text_color(w_hdr_power_lbl[cur_p],
                                        lv_color_hex(rf_active ? 0xD8E0E8 : 0xEF4444), 0);
        }
    }
    int sig_ok = (rf_active && link_ok) || local_gimbal;
    if (sig_ok != prev_sig_ok) {
        prev_sig_ok = sig_ok;
        if (w_hdr_sig_bars[cur_p][0]) {
            for (int i = 0; i < 4; ++i) {
                lv_obj_set_style_bg_color(w_hdr_sig_bars[cur_p][i],
                                          lv_color_hex(sig_ok ? 0x5ECA62 : 0x465160), 0);
            }
        }
        if (w_hdr_sig_x[cur_p]) {
            if (sig_ok) lv_obj_add_flag(w_hdr_sig_x[cur_p], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_remove_flag(w_hdr_sig_x[cur_p], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 2. Update HOME Screen */
    if (figma_page == FIGMA_PAGE_HOME) {
        uint64_t elapsed_s = (now - boot_epoch_ms) / 1000ULL;
        if (elapsed_s != prev_sec) {
            prev_sec = elapsed_s;
            unsigned mm = (unsigned)((elapsed_s / 60ULL) % 100ULL);
            unsigned ss = (unsigned)(elapsed_s % 60ULL);
            if (w_home_timer_lbl) {
                char tbuf[16];
                snprintf(tbuf, sizeof(tbuf), "%02u:%02u", mm, ss);
                tyvor_ui321_text(w_home_timer_lbl, tbuf);
            }
        }
        if (armed != prev_armed) {
            prev_armed = armed;
            if (w_home_arm_lbl && w_home_arm_dot) {
                tyvor_ui321_text(w_home_arm_lbl, armed ? "ARMED" : "DISARMED");
                lv_obj_set_style_text_color(w_home_arm_lbl,
                                            lv_color_hex(armed ? 0x22C55E : 0xEF4444), 0);
                lv_obj_set_style_bg_color(w_home_arm_dot,
                                          lv_color_hex(armed ? 0x22C55E : 0xEF4444), 0);
            }
        }

        int p_rol = ch_raw_to_pct(get_live_ch_raw(0, snap, link_ok));
        int p_pit = ch_raw_to_pct(get_live_ch_raw(1, snap, link_ok));
        int p_thr = ch_raw_to_pct(get_live_ch_raw(2, snap, link_ok));
        int p_yaw = ch_raw_to_pct(get_live_ch_raw(3, snap, link_ok));
        int sticks_changed = (p_rol != prev_rol || p_pit != prev_pit ||
                              p_thr != prev_thr || p_yaw != prev_yaw);

        int sig_mode = local_gimbal ? 2 : (link_ok ? 1 : 0);
        if (w_home_sig_lbl && (sig_mode != prev_sig_mode || (local_gimbal && sticks_changed))) {
            prev_sig_mode = sig_mode;
            if (local_gimbal) {
                char sbuf[40];
                snprintf(sbuf, sizeof(sbuf), "R:%d%% P:%d%% T:%d%%", p_rol, p_pit, p_thr);
                tyvor_ui321_text(w_home_sig_lbl, sbuf);
                lv_obj_set_style_text_color(w_home_sig_lbl, lv_color_hex(0x38BDF8), 0);
            } else if (link_ok) {
                char sbuf[40];
                snprintf(sbuf, sizeof(sbuf), "%s • 100Hz OK", k_os_models[os_active_model].tab_name);
                tyvor_ui321_text(w_home_sig_lbl, sbuf);
                lv_obj_set_style_text_color(w_home_sig_lbl, lv_color_hex(0x5ECA62), 0);
            } else {
                tyvor_ui321_text(w_home_sig_lbl, "READY • PLUG GIMBAL");
                lv_obj_set_style_text_color(w_home_sig_lbl, lv_color_hex(0x38BDF8), 0);
            }
        }
        if (w_home_mode_lbl && page_changed) {
            tyvor_ui321_text(w_home_mode_lbl, k_os_models[os_active_model].mode_name);
        }
        int sats = (int)((link_ok && snap->telemetry.valid) ? snap->telemetry.satellites : 0u);
        if (w_home_gps_val && sats != prev_sats) {
            prev_sats = sats;
            char gbuf[12];
            snprintf(gbuf, sizeof(gbuf), "%d", sats);
            tyvor_ui321_text(w_home_gps_val, gbuf);
            lv_obj_set_style_text_color(w_home_gps_val,
                                        lv_color_hex(sats > 0 ? 0x5ECA62 : 0xEF4444), 0);
            if (w_home_gps_arc) {
                int arc_v = (sats >= 12) ? 100 : (sats * 100) / 12;
                lv_arc_set_value(w_home_gps_arc, arc_v);
                lv_obj_set_style_arc_color(w_home_gps_arc,
                                           lv_color_hex(sats > 0 ? 0x5ECA62 : 0x38BDF8),
                                           LV_PART_INDICATOR);
            }
        }
        /* Live Gimbal Stick / Trim Ticks from CH1..CH4 */
        if (w_home_trim_ticks[0] && sticks_changed) {
            prev_rol = p_rol;
            prev_pit = p_pit;
            prev_thr = p_thr;
            prev_yaw = p_yaw;
            lv_obj_set_pos(w_home_trim_ticks[0], 4,   84 - (p_thr * 36) / 100);
            lv_obj_set_pos(w_home_trim_ticks[1], 308, 84 - (p_pit * 36) / 100);
            lv_obj_set_pos(w_home_trim_ticks[2], 62 + (p_yaw * 38) / 100, 228);
            lv_obj_set_pos(w_home_trim_ticks[3], 254 + (p_rol * 38) / 100, 228);
        }
    }

    /* 3. Update CH VIEW Screen (Live Gimbal + Switch Bars) */
    if (figma_page == FIGMA_PAGE_CH_VIEW) {
        for (int i = 0; i < 10; ++i) {
            int pct = 0;
            if (i < 6) {
                pct = ch_raw_to_pct(get_live_ch_raw(i, snap, link_ok));
            } else {
                pct = 0;
            }
            if (w_chv_pct[i]) {
                char pbuf[12];
                snprintf(pbuf, sizeof(pbuf), "%d%%", pct);
                tyvor_ui321_text(w_chv_pct[i], pbuf);
            }
            int w_px = ((pct + 100) * 140) / 200;
            if (w_px < 4) w_px = 4;
            if (w_px > 140) w_px = 140;
            if (w_chv_fill[i]) lv_obj_set_width(w_chv_fill[i], w_px);
            if (w_chv_tick[i]) lv_obj_set_x(w_chv_tick[i], w_px - 2);
        }
    }

    /* 4. Update CHANNELS Screen (Live Gimbal Primary Axes + Switches) */
    if (figma_page == FIGMA_PAGE_CHANNELS) {
        for (int i = 0; i < 6; ++i) {
            int pct = ch_raw_to_pct(get_live_ch_raw(i, snap, link_ok));
            if (w_chc_pct[i]) {
                char pbuf[12];
                snprintf(pbuf, sizeof(pbuf), "%d%%", pct);
                tyvor_ui321_text(w_chc_pct[i], pbuf);
            }
            int w_px = ((pct + 100) * 84) / 200;
            if (w_px < 4) w_px = 4;
            if (w_px > 84) w_px = 84;
            if (w_chc_fill[i]) lv_obj_set_width(w_chc_fill[i], w_px);
        }
    }

    /* 5. Update TELEMETRY Screen (Throttled to 5Hz, paused during active scroll for 60fps glide) */
    if (figma_page == FIGMA_PAGE_TELEMETRY) {
        static uint64_t next_tel_ms = 0;
        static uint32_t prev_tel_col[23];
        if (page_changed) {
            next_tel_ms = 0;
            for (int i = 0; i < 23; ++i) prev_tel_col[i] = 0xFFFFFFFFu;
        }
        int tel_scrolling = (w_tel_vp && lv_obj_is_scrolling(w_tel_vp)) ||
                            (touch_down && g_gesture.locked_axis == TYVOR_AXIS_VERT);
        if (!tel_scrolling && now >= next_tel_ms) {
            next_tel_ms = now + 200;
            uint8_t tv = (link_ok) ? snap->telemetry.valid : 0u;
            int has_rssi = rf_active && ((tv & TYVOR_T_RSSI) != 0u);
            int has_lq   = rf_active && ((tv & TYVOR_T_LQ) != 0u);
            int has_bat  = link_ok && ((tv & TYVOR_T_BATTERY) != 0u);
            int has_gps  = link_ok && ((tv & TYVOR_T_GPS) != 0u);

            char b_1rss[16], b_2rss[16], b_rqly[16], b_rsnr[16], b_rfmd[16];
            char b_tpwr[16], b_rxbt[16], b_batp[16], b_gps[32], b_sats[16];
            const char *vals[23];
            uint32_t cols[23];
            for (int i = 0; i < 23; ++i) {
                vals[i] = "---";
                cols[i] = 0xFF3038;
            }

            if (has_rssi) {
                snprintf(b_1rss, sizeof(b_1rss), "%d dBm", (int)snap->telemetry.rssi_dbm);
                snprintf(b_2rss, sizeof(b_2rss), "%d dBm", (int)snap->telemetry.rssi_dbm - 2);
                int snr = 12 + ((int)snap->telemetry.rssi_dbm + 60) / 5;
                if (snr < -10) snr = -10;
                if (snr > 18) snr = 18;
                snprintf(b_rsnr, sizeof(b_rsnr), "%d dB", snr);
                vals[0] = b_1rss; cols[0] = 0x5ECA62;
                vals[1] = b_2rss; cols[1] = 0x5ECA62;
                vals[3] = b_rsnr; cols[3] = 0x38BDF8;
                vals[4] = "ANT 1"; cols[4] = 0x5ECA62;
                vals[7] = b_1rss; cols[7] = 0x5ECA62;
            }
            if (has_lq) {
                snprintf(b_rqly, sizeof(b_rqly), "%u %%", (unsigned)snap->telemetry.lq);
                vals[2] = b_rqly; cols[2] = 0x5ECA62;
                vals[8] = b_rqly; cols[8] = 0x5ECA62;
            }
            if (rf_active && link_ok) {
                snprintf(b_rfmd, sizeof(b_rfmd), "%u Hz", (unsigned)k_elrs_rate_table[os_elrs_rate_idx]);
                snprintf(b_tpwr, sizeof(b_tpwr), "%u mW", (unsigned)k_power_mw_table[os_power_idx]);
                vals[5] = b_rfmd; cols[5] = 0x38BDF8;
                vals[6] = b_tpwr; cols[6] = 0x38BDF8;
            }
            if (has_bat) {
                unsigned mv = (unsigned)snap->telemetry.battery_mv;
                snprintf(b_rxbt, sizeof(b_rxbt), "%u.%02u V", mv / 1000u, (mv % 1000u) / 10u);
                int bpct = (mv <= 13200u) ? 0 : (mv >= 16800u) ? 100 : (int)((mv - 13200u) * 100u / 3600u);
                if (mv > 0u && mv < 9000u) {
                    bpct = (mv <= 6600u) ? 0 : (mv >= 8400u) ? 100 : (int)((mv - 6600u) * 100u / 1800u);
                }
                snprintf(b_batp, sizeof(b_batp), "%d %%", bpct);
                vals[10] = b_rxbt; cols[10] = (bpct > 25) ? 0x5ECA62 : 0xF59E0B;
                vals[13] = b_batp; cols[13] = (bpct > 25) ? 0x5ECA62 : 0xF59E0B;
            }
            if (link_ok) {
                vals[17] = k_os_models[os_active_model].mode_name;
                cols[17] = 0x38BDF8;
            }
            if (has_gps) {
                double lat = (double)snap->telemetry.latitude_e7 / 1e7;
                double lon = (double)snap->telemetry.longitude_e7 / 1e7;
                snprintf(b_gps, sizeof(b_gps), "%.4f, %.4f", lat, lon);
                snprintf(b_sats, sizeof(b_sats), "%u", (unsigned)snap->telemetry.satellites);
                vals[18] = b_gps;  cols[18] = (snap->telemetry.satellites > 0) ? 0x5ECA62 : 0xF59E0B;
                vals[22] = b_sats; cols[22] = (snap->telemetry.satellites > 0) ? 0x5ECA62 : 0xEF4444;
            }

            for (int i = 0; i < 23; ++i) {
                if (w_tel_val[i]) {
                    tyvor_ui321_text(w_tel_val[i], vals[i]);
                    if (cols[i] != prev_tel_col[i]) {
                        prev_tel_col[i] = cols[i];
                        lv_obj_set_style_text_color(w_tel_val[i], lv_color_hex(cols[i]), 0);
                    }
                }
            }
        }
    }

    /* 6. Update STICK CAL Screen (Live 4-axis Gimbal ADC + Min/Max Capture) */
    if (figma_page == FIGMA_PAGE_STICK_CAL) {
        for (int i = 0; i < 4; ++i) {
            if (os_stick_cal_active) {
                int32_t v = gimbal_adc_filt[i];
                if (v < os_cal_live_min[i]) os_cal_live_min[i] = v;
                if (v > os_cal_live_max[i]) os_cal_live_max[i] = v;
            }
            int pct = ch_raw_to_pct(gimbal_raw_val[i]);
            if (w_cal_pct[i]) {
                char pbuf[12];
                snprintf(pbuf, sizeof(pbuf), "%d%%", pct);
                tyvor_ui321_text(w_cal_pct[i], pbuf);
            }
            if (w_cal_range[i]) {
                char rbuf[24];
                int32_t mn = os_stick_cal_active ? os_cal_live_min[i] : gimbal_min_cal[i];
                int32_t mx = os_stick_cal_active ? os_cal_live_max[i] : gimbal_max_cal[i];
                snprintf(rbuf, sizeof(rbuf), "%ld-%ld", (long)mn, (long)mx);
                tyvor_ui321_text(w_cal_range[i], rbuf);
            }
            int w_px = ((pct + 100) * 86) / 200;
            if (w_px < 4) w_px = 4;
            if (w_px > 86) w_px = 86;
            if (w_cal_fill[i]) lv_obj_set_width(w_cal_fill[i], w_px);
        }
    }

    /* 7. Update ABOUT Screen (Live Model, Power, Gimbal Axes, Protocol v2) */
    if (figma_page == FIGMA_PAGE_ABOUT) {
        char elrs_info[32], mod_info[32], adc_info[32], link_info[32];
        snprintf(elrs_info, sizeof(elrs_info), "v3.6.3 | %umW", (unsigned)k_power_mw_table[os_power_idx]);
        snprintf(mod_info, sizeof(mod_info), "%s (%s)",
                 k_os_models[os_active_model].tab_name,
                 k_os_models[os_active_model].mode_name);
        int n_conn = (int)gimbal_connected[0] + (int)gimbal_connected[1] +
                     (int)gimbal_connected[2] + (int)gimbal_connected[3];
        snprintf(adc_info, sizeof(adc_info), "%d/4 Axes (M%d)", n_conn, os_stick_mode);
        snprintf(link_info, sizeof(link_info), "%s (%" PRIu32 "f)",
                 ui_endpoint.link.ready ? "100Hz OK" : "Standalone",
                 ui_endpoint.link.parser.stats.frames);
        const char *ab_vals[4] = {elrs_info, mod_info, adc_info, link_info};
        for (int i = 0; i < 4; ++i) {
            if (w_about_vals[i]) tyvor_ui321_text(w_about_vals[i], ab_vals[i]);
        }
    }

    /* 7. Update SLEEP / SCREEN SAVER: Multi-Mood Owl Eyes (Blink, Happy, Wink, Sad, Angry, Sleep Zzz) */
    if (figma_page == FIGMA_PAGE_SLEEP_SAVER && w_sleep_owl_img) {
        /* 12000ms expressive cycle: Open -> Blink -> Happy -> Wink -> Sad -> Angry -> Sleep (Zzz) */
        uint32_t phase_ms = (uint32_t)(now % 12000ULL);
        int st = 0;
        if (phase_ms < 1200u) st = 0;            /* Open */
        else if (phase_ms < 1270u) st = 1;       /* Blink */
        else if (phase_ms < 1340u) st = 2;
        else if (phase_ms < 1460u) st = 3;
        else if (phase_ms < 1530u) st = 2;
        else if (phase_ms < 1600u) st = 1;
        else if (phase_ms < 3100u) st = 5;       /* Happy (^ ^) */
        else if (phase_ms < 3200u) st = 2;
        else if (phase_ms < 3320u) st = 3;
        else if (phase_ms < 3400u) st = 1;
        else if (phase_ms < 4100u) st = 0;       /* Open */
        else if (phase_ms < 4700u) st = 7;       /* Wink */
        else if (phase_ms < 4850u) st = 0;
        else if (phase_ms < 6400u) st = 6;       /* Sad */
        else if (phase_ms < 6520u) st = 2;
        else if (phase_ms < 6640u) st = 3;
        else if (phase_ms < 6720u) st = 1;
        else if (phase_ms < 8500u) st = 4;       /* Angry */
        else if (phase_ms < 8620u) st = 2;
        else if (phase_ms < 11600u) st = 8;      /* Sleep (◡ ◡ + Zzz) */
        else if (phase_ms < 11750u) st = 2;      /* Wake blink */
        else if (phase_ms < 11850u) st = 1;
        else st = 0;

        if (st != last_sleep_blink_stage) {
            last_sleep_blink_stage = st;
            set_boot_eye_blink_stage(st);
            lv_obj_invalidate(w_sleep_owl_img);
        }

        int z_step = (st == 8) ? (int)((phase_ms - 8620u) / 350u) % 4 : -1;
        if (z_step != last_sleep_zzz_step) {
            last_sleep_zzz_step = z_step;
            for (int i = 0; i < 3; ++i) {
                if (!w_sleep_zzz[i]) continue;
                if (z_step > i) lv_obj_remove_flag(w_sleep_zzz[i], LV_OBJ_FLAG_HIDDEN);
                else lv_obj_add_flag(w_sleep_zzz[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

        int m_tick = (int)((now / 260ULL) & 1ULL);
        int m_key = (st * 2) + m_tick;
        if (m_key != last_sleep_mood_tick) {
            last_sleep_mood_tick = m_key;
            for (int i = 0; i < 3; ++i) {
                if (!w_sleep_happy[i]) continue;
                if (st == 5 && ((i + m_tick) % 2 == 0 || i == 0)) lv_obj_remove_flag(w_sleep_happy[i], LV_OBJ_FLAG_HIDDEN);
                else lv_obj_add_flag(w_sleep_happy[i], LV_OBJ_FLAG_HIDDEN);
            }
            for (int i = 0; i < 2; ++i) {
                if (!w_sleep_sad[i]) continue;
                if (st == 6 && i == m_tick) lv_obj_remove_flag(w_sleep_sad[i], LV_OBJ_FLAG_HIDDEN);
                else lv_obj_add_flag(w_sleep_sad[i], LV_OBJ_FLAG_HIDDEN);
            }
            for (int i = 0; i < 4; ++i) {
                if (!w_sleep_angry[i]) continue;
                if (st == 4 && m_tick == 1) lv_obj_remove_flag(w_sleep_angry[i], LV_OBJ_FLAG_HIDDEN);
                else lv_obj_add_flag(w_sleep_angry[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    /* 8. Safety: THROTTLE WARNING Overlay (Figma SAOLALINK — THROTTLE WARNING) */
    int thr_pct = ch_raw_to_pct(get_live_ch_raw(2, snap, link_ok));
    int thr_high = (link_ok && ((snap->safety_reasons & TYVOR_SAFE_THROTTLE) != 0u)) ||
                   (gimbal_connected[2] && thr_pct > -70 && !armed);
    if (!thr_high) {
        thr_warn_dismissed = 0;
        if (w_thr_warn_overlay) {
            lv_obj_del(w_thr_warn_overlay);
            w_thr_warn_overlay = NULL;
            w_thr_warn_bar = NULL;
            w_thr_warn_pct = NULL;
        }
    } else if (!thr_warn_dismissed && !sleep_active && figma_page != FIGMA_PAGE_STICK_CAL) {
        if (!w_thr_warn_overlay) {
            w_thr_warn_overlay = f_rect(lv_layer_top(), 0, 0, 320, 240, 0x050C18, 210, 0, 0, 0);
            lv_obj_add_flag(w_thr_warn_overlay, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_t *card = f_rect(w_thr_warn_overlay, 24, 24, 272, 192, 0x1E1418, 250, 16, 2, 0xEF4444);
            lv_obj_t *ico_bg = f_rect(card, 114, 12, 44, 44, 0x3F1D1D, LV_OPA_COVER, 22, 2, 0xF59E0B);
            f_icon_centered(ico_bg, LV_SYMBOL_WARNING, &lv_font_montserrat_20, 0xF59E0B);
            f_label(card, 12, 62, 248, "THROTTLE WARNING",
                    &lv_font_montserrat_14, 0xF59E0B, LV_TEXT_ALIGN_CENTER);
            f_label(card, 12, 82, 248,
                    os_lang_vi ? "Ha can ga ve muc thap nhat!" : "Throttle not at zero! Lower stick.",
                    &lv_font_montserrat_12, 0xE2E8F0, LV_TEXT_ALIGN_CENTER);
            lv_obj_t *trk = f_rect(card, 24, 108, 224, 14, 0x0F172A, LV_OPA_COVER, 7, 1, 0x475569);
            w_thr_warn_bar = f_rect(trk, 0, 0, 112, 14, 0xEF4444, LV_OPA_COVER, 7, 0, 0);
            w_thr_warn_pct = f_label(card, 24, 126, 224, "THR: 0%",
                                     &lv_font_montserrat_12, 0xFCA5A5, LV_TEXT_ALIGN_CENTER);
            lv_obj_t *btn = f_rect(card, 66, 146, 140, 34, 0x7F1D1D, LV_OPA_COVER, 10, 1, 0xEF4444);
            f_icon_centered(btn, os_lang_vi ? "Bo qua" : "Dismiss", &lv_font_montserrat_14, 0xFFFFFF);
            lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(btn, on_thr_warn_dismiss_cb, LV_EVENT_CLICKED, NULL);
        }
        if (w_thr_warn_bar && w_thr_warn_pct) {
            int pct01 = (thr_pct + 100) / 2;
            if (pct01 < 0) pct01 = 0;
            if (pct01 > 100) pct01 = 100;
            lv_obj_set_width(w_thr_warn_bar, (pct01 * 224) / 100);
            char wbuf[24];
            snprintf(wbuf, sizeof(wbuf), "THR: %d%% (%d)", pct01, thr_pct);
            tyvor_ui321_text(w_thr_warn_pct, wbuf);
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "TYVOR Transmitter OS (ESP32-S3 + Gimbal ADC + Owl Sleep) starting... reset_reason=%d",
             (int)esp_reset_reason());
    ESP_LOGI(TAG, "PSRAM initialized=%d size=%u bytes",
             esp_psram_is_initialized(), (unsigned)esp_psram_get_size());

    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);
    os_config_load();

    uint64_t now = millis();
    boot_epoch_ms = now;
    last_user_activity_ms = now;
    uint64_t id = fresh_boot_id();
    uart_ready = init_uart();
    init_gimbal_adc();
    tyvor_async_tx_init(&tx_queue, tx_start, tx_state, tx_abort, NULL);
    tyvor_bench_transport_init(&uart_transport, write_uart, NULL, uart_ready);
    if (tyvor_ui_endpoint_init(&ui_endpoint, id, tyvor_bench_transport_write, &uart_transport, now) != 0) {
        abort();
    }

    ili9341_init();

    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);

    tyvor_ui321_port_t port = {NULL, 320, 240, tft_write_region, ui_micros_cb};
    if (tyvor_ui321_init(&oxbot_ui, port, lvgl_draw_buffer, sizeof(lvgl_draw_buffer)) != 0) {
        ESP_LOGE(TAG, "tyvor_ui321_init failed");
        abort();
    }

    touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(touch_indev, oxbot_ui.display);
    lv_indev_set_read_cb(touch_indev, lvgl_touch_read_cb);
    lv_indev_set_scroll_limit(touch_indev, 4);
    lv_indev_set_scroll_throw(touch_indev, 6);

    /* 1. Show ONLY TYVOR Logo + Blinking Owl Eyes on Startup */
    run_tyvor_boot_splash();

    /* 2. Open Figma Home Screen (with TYVOR branding) */
    f_open_page(FIGMA_PAGE_HOME);
    f_apply_screen_brightness(os_brightness_pct);

    now = millis();
    last_user_activity_ms = now;
    uint64_t next_ui_ms = now;
    uint64_t next_log_ms = now + 1000;
    uint64_t last_pin_check_ms = now;
    uint32_t last_model_id = 0;
    uint32_t last_model_rev = 0;
    uint8_t last_safety_state = 0xFFu;
    active_non_sleep_page = FIGMA_PAGE_HOME;

    int boot_btn_prev = 0;
    uint64_t boot_btn_down_ms = 0;
    int boot_btn_long_handled = 0;
    int prev_link_ready = 0;

    for (;;) {
        service_uart_bridge();
        now = millis();
        service_gimbal_adc(now);

        if (ui_endpoint.link.ready && !prev_link_ready) {
            prev_link_ready = 1;
            os_sync_all_to_stm32();
        } else if (!ui_endpoint.link.ready) {
            prev_link_ready = 0;
        }

        /* Automatic UART TX/RX crossover detection (17/18 <-> 18/17) if no frames received */
        if (ui_endpoint.link.parser.stats.frames == 0 && now - last_pin_check_ms >= 1500) {
            last_pin_check_ms = now;
            uart_swapped = !uart_swapped;
            int tx_pin = uart_swapped ? 18 : 17;
            int rx_pin = uart_swapped ? 17 : 18;
            ESP_LOGW(TAG, "No Protocol v2 frames yet; probing UART1 TX=%d RX=%d", tx_pin, rx_pin);
            (void)uart_set_pin(BRIDGE_UART, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
            gpio_set_pull_mode(rx_pin, GPIO_PULLUP_ONLY);
        }

        /* Handle ESP32-S3 BOOT Button (GPIO0, active LOW):
         *  - Short press: Wake from Sleep or cycle to next Figma screen
         *  - Hold (>= 600ms): Enter Sleep / Screen Saver immediately
         */
        int boot_btn = (gpio_get_level(GPIO_NUM_0) == 0);
        if (boot_btn && !boot_btn_prev) {
            boot_btn_down_ms = now;
            boot_btn_long_handled = 0;
            last_user_activity_ms = now;
        } else if (boot_btn && !boot_btn_long_handled && (now - boot_btn_down_ms >= 600)) {
            boot_btn_long_handled = 1;
            sleep_active = 1;
            f_open_page(FIGMA_PAGE_SLEEP_SAVER);
        } else if (!boot_btn && boot_btn_prev) {
            last_user_activity_ms = now;
            if (!boot_btn_long_handled && (now - boot_btn_down_ms >= 25)) {
                if (sleep_active) {
                    sleep_active = 0;
                    f_open_page(active_non_sleep_page);
                } else {
                    unsigned next_p = (figma_page + 1u) % FIGMA_PAGE_COUNT;
                    if (next_p != FIGMA_PAGE_SLEEP_SAVER) {
                        active_non_sleep_page = next_p;
                    }
                    f_open_page(next_p);
                }
            }
        }
        boot_btn_prev = boot_btn;

        /* Follow page / model / arm changes from STM32 Core PA0 button over Protocol v2 */
        if (ui_endpoint.state.valid) {
            uint32_t mid = ui_endpoint.state.snapshot.model_id;
            uint32_t rev = ui_endpoint.state.snapshot.model_revision;
            uint8_t sstate = ui_endpoint.state.snapshot.safety_state;

            if (last_model_id != 0 && mid != last_model_id) {
                last_user_activity_ms = now;
                sleep_active = 0;
                active_non_sleep_page = FIGMA_PAGE_MODELS;
                f_open_page(FIGMA_PAGE_MODELS);
            } else if (last_model_rev != 0 && rev != last_model_rev && rev >= 1) {
                last_user_activity_ms = now;
                sleep_active = 0;
                unsigned target_page = (unsigned)((rev - 1u) % FIGMA_PAGE_COUNT);
                if (target_page == FIGMA_PAGE_SLEEP_SAVER) {
                    target_page = (figma_page + 1u) % (FIGMA_PAGE_COUNT - 1u);
                }
                active_non_sleep_page = target_page;
                f_open_page(target_page);
            } else if (last_safety_state != 0xFFu && sstate != last_safety_state) {
                last_user_activity_ms = now;
                if (sleep_active) {
                    sleep_active = 0;
                    f_open_page(active_non_sleep_page);
                }
            }
            if (mid != 0) last_model_id = mid;
            if (rev != 0) last_model_rev = rev;
            last_safety_state = sstate;
        }

        /* Auto-Sleep Screen Saver after configurable inactivity timeout (never while interacting) */
        now = millis();
        if (os_sleep_timeout_sec > 0 && !sleep_active &&
            !touch_down && qs_slider_drag == 0 &&
            g_trans.state == TYVOR_TRANS_IDLE &&
            !os_stick_cal_active && !os_bind_active && !os_finder_active &&
            figma_page != FIGMA_PAGE_SLEEP_SAVER &&
            now >= last_user_activity_ms &&
            (now - last_user_activity_ms >= (uint64_t)os_sleep_timeout_sec * 1000ULL)) {
            ESP_LOGI("SLEEP", "Auto-sleep after %us idle (page=%u)", (unsigned)os_sleep_timeout_sec, figma_page);
            sleep_active = 1;
            f_open_page(FIGMA_PAGE_SLEEP_SAVER);
        }

        if (now >= next_ui_ms) {
            next_ui_ms = now + 8; /* 125 Hz UI & touch polling rate */
            f_update_live_ui(now);
            in_lvgl_handler = 1;
            (void)lv_timer_handler();
            in_lvgl_handler = 0;
            if (deferred_open_page >= 0) {
                int dp = deferred_open_page;
                int dx = deferred_enter_x;
                int dy = deferred_enter_y;
                deferred_open_page = -1;
                f_open_page_dir((unsigned)dp, dx, dy);
            }
        }

        if (now >= next_log_ms) {
            next_log_ms = now + 1000;
            ESP_LOGI(TAG, "LINK ready=%u frames=%" PRIu32 " gimbal=[%d:%d%%, %d:%d%%, %d:%d%%, %d:%d%%] page=%u sleep=%d",
                     ui_endpoint.link.ready,
                     ui_endpoint.link.parser.stats.frames,
                     gimbal_connected[0], ch_raw_to_pct(get_live_ch_raw(0, &ui_endpoint.state.snapshot, ui_endpoint.state.valid)),
                     gimbal_connected[1], ch_raw_to_pct(get_live_ch_raw(1, &ui_endpoint.state.snapshot, ui_endpoint.state.valid)),
                     gimbal_connected[2], ch_raw_to_pct(get_live_ch_raw(2, &ui_endpoint.state.snapshot, ui_endpoint.state.valid)),
                     gimbal_connected[3], ch_raw_to_pct(get_live_ch_raw(3, &ui_endpoint.state.snapshot, ui_endpoint.state.valid)),
                     figma_page,
                     sleep_active);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
