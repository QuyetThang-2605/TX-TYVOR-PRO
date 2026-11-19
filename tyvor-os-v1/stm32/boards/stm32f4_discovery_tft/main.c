#include "stm32f4xx_hal.h"
#include "discovery_demo.h"
#include "tyvor_core_bridge.h"
#include "tyvor_ui_endpoint.h"
#include "tyvor_bench_transport.h"
#include "tyvor_async_tx.h"
#include "tyvor_crsf.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * PIN MAPPING — STM32F407G-DISC1 (P1/P2) <-> 3.2" TFT SPI 240x320 V1.0
 * ============================================================================
 *  1. VCC       -> 5V (or VDD/3V)
 *  2. GND       -> GND
 *  3. CS        -> PB12
 *  4. RESET     -> PD8
 *  5. DC        -> PD9
 *  6. SDI(MOSI) -> PB15 (SPI2_MOSI AF5)
 *  7. SCK       -> PB13 (SPI2_SCK  AF5)
 *  8. LED       -> PD10 (Active HIGH backlight)
 *  9. SDO(MISO) -> PB14 (SPI2_MISO AF5)
 * 10. T_CLK     -> PB10 (XPT2046 Touch Clock)
 * 11. T_CS      -> PE15 (XPT2046 Touch Chip Select)
 * 12. T_DIN     -> PE14 (XPT2046 Touch MOSI)
 * 13. T_DO      -> PE13 (XPT2046 Touch MISO, Pull-up)
 * 14. T_IRQ     -> PE12 (XPT2046 Touch IRQ, Active LOW, Pull-up)
 *
 * Inter-MCU UART2 (Protocol v2 @ 115200 8N1):
 *  - PA2        -> USART2_TX
 *  - PA3        -> USART2_RX
 * ========================================================================== */

#define TFT_CS_PORT     GPIOB
#define TFT_CS_PIN      GPIO_PIN_12
#define TFT_RST_PORT    GPIOD
#define TFT_RST_PIN     GPIO_PIN_8
#define TFT_DC_PORT     GPIOD
#define TFT_DC_PIN      GPIO_PIN_9
#define TFT_LED_PORT    GPIOD
#define TFT_LED_PIN     GPIO_PIN_10

#define TOUCH_CLK_PORT  GPIOB
#define TOUCH_CLK_PIN   GPIO_PIN_10
#define TOUCH_CS_PORT   GPIOE
#define TOUCH_CS_PIN    GPIO_PIN_15
#define TOUCH_DIN_PORT  GPIOE
#define TOUCH_DIN_PIN   GPIO_PIN_14
#define TOUCH_DO_PORT   GPIOB
#define TOUCH_DO_PIN    GPIO_PIN_13
#define TOUCH_IRQ_PORT  GPIOE
#define TOUCH_IRQ_PIN   GPIO_PIN_12

void _init(void) {}
void _fini(void) {}

/* Place the 20 KiB LVGL partial draw buffer in CCMRAM (64 KiB available) */
__attribute__((section(".ccmram"), aligned(8)))
static uint8_t lvgl_draw_buffer[TYVOR_DISPLAY_BUFFER_BYTES];

discovery_demo_t tyvor_discovery_demo;
volatile uint32_t tyvor_tft_diag[32];
static tyvor_ui321_t oxbot_ui;
static lv_indev_t *touch_indev;
static SPI_HandleTypeDef hspi2;
static ADC_HandleTypeDef hadc1;
static UART_HandleTypeDef huart2;
static UART_HandleTypeDef huart6;
static IWDG_HandleTypeDef hiwdg;

/* UART2 + Local Protocol v2 Dual-MCU Bridge State */
static tyvor_core_bridge_t uart_bridge;
static tyvor_ui_endpoint_t ui_endpoint;
static tyvor_bench_transport_t uart_transport;
static tyvor_async_tx_t tx_queue;
static uint8_t rx_ring[256];
static volatile uint16_t rx_head, rx_tail;
static volatile int rx_loss;
static const uint8_t *tx_bytes;
static volatile uint16_t tx_index, tx_length;
static volatile int tx_busy;

void SysTick_Handler(void) {
    HAL_IncTick();
}

static uint64_t board_millis(void) {
    static uint32_t prev;
    static uint64_t high;
    uint32_t now = HAL_GetTick();
    if (now < prev) high += UINT64_C(1) << 32;
    prev = now;
    return high + now;
}

static uint32_t lvgl_tick_cb(void) {
    return HAL_GetTick();
}

static uint64_t ui_micros_cb(void *ctx) {
    (void)ctx;
    uint32_t ms = HAL_GetTick();
    uint32_t cyc = DWT->CYCCNT;
    uint32_t mhz = SystemCoreClock / 1000000u;
    if (!mhz) mhz = 168u;
    return (uint64_t)ms * 1000u + (cyc / mhz) % 1000u;
}

/* ============================================================================
 * DETERMINISTIC GPIO BIT-BANG SPI ILI9341 DISPLAY DRIVER (320x240 Landscape)
 * Note: Auto-scanner detected TFT SCK is soldered to PE13 and T_DO to PB13.
 * ========================================================================== */
#define TFT_SCK_PORT    GPIOE
#define TFT_SCK_PIN     GPIO_PIN_13
#define TFT_MISO_PORT   GPIOB
#define TFT_MISO_PIN    GPIO_PIN_14
#define TFT_MOSI_PORT   GPIOB
#define TFT_MOSI_PIN    GPIO_PIN_15

static void service_core_and_uart(void);

static inline void tft_pin_delay(void) {
    for (volatile unsigned i = 0; i < 8; ++i) __NOP();
}

static inline void spi_bb_write_byte(uint8_t b) {
    for (int i = 7; i >= 0; --i) {
        if (b & (1u << i)) {
            TFT_MOSI_PORT->BSRR = TFT_MOSI_PIN;
        } else {
            TFT_MOSI_PORT->BSRR = (uint32_t)TFT_MOSI_PIN << 16;
        }
        TFT_SCK_PORT->BSRR = (uint32_t)TFT_SCK_PIN << 16;
        __NOP(); __NOP();
        TFT_SCK_PORT->BSRR = TFT_SCK_PIN;
        __NOP(); __NOP();
    }
    TFT_SCK_PORT->BSRR = (uint32_t)TFT_SCK_PIN << 16;
}

static void ili9341_cmd(uint8_t cmd) {
    TFT_DC_PORT->BSRR = (uint32_t)TFT_DC_PIN << 16; /* DC = 0 */
    TFT_CS_PORT->BSRR = (uint32_t)TFT_CS_PIN << 16; /* CS = 0 */
    tft_pin_delay();
    spi_bb_write_byte(cmd);
    tft_pin_delay();
    TFT_CS_PORT->BSRR = TFT_CS_PIN;                 /* CS = 1 */
    tft_pin_delay();
}

static void ili9341_data(const uint8_t *data, size_t len) {
    if (!len) return;
    TFT_DC_PORT->BSRR = TFT_DC_PIN;                 /* DC = 1 */
    TFT_CS_PORT->BSRR = (uint32_t)TFT_CS_PIN << 16; /* CS = 0 */
    tft_pin_delay();
    for (size_t i = 0; i < len; ++i) {
        spi_bb_write_byte(data[i]);
    }
    tft_pin_delay();
    TFT_CS_PORT->BSRR = TFT_CS_PIN;                 /* CS = 1 */
    tft_pin_delay();
}

/* Read register testing both MISO (PB14) and bidirectional SDA (PB15) */
static void ili9341_read_reg_dual(uint8_t reg, unsigned bits, uint32_t pull,
                                  uint32_t *out_miso, uint32_t *out_sda) {
    GPIO_InitTypeDef g = {0};
    g.Pin = TFT_MISO_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = pull;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(TFT_MISO_PORT, &g);

    TFT_DC_PORT->BSRR = (uint32_t)TFT_DC_PIN << 16; /* DC = 0 */
    TFT_CS_PORT->BSRR = (uint32_t)TFT_CS_PIN << 16; /* CS = 0 */
    tft_pin_delay();
    spi_bb_write_byte(reg);
    TFT_DC_PORT->BSRR = TFT_DC_PIN;                 /* DC = 1 */

    /* Switch MOSI (PB15) to input as well in case panel uses bidirectional SDA */
    g.Pin = TFT_MOSI_PIN;
    HAL_GPIO_Init(TFT_MOSI_PORT, &g);
    tft_pin_delay();

    uint32_t v_miso = 0, v_sda = 0;
    for (unsigned i = 0; i < bits; ++i) {
        TFT_SCK_PORT->BSRR = (uint32_t)TFT_SCK_PIN << 16;
        tft_pin_delay();
        TFT_SCK_PORT->BSRR = TFT_SCK_PIN;
        tft_pin_delay();
        v_miso = (v_miso << 1) | ((TFT_MISO_PORT->IDR & TFT_MISO_PIN) ? 1u : 0u);
        v_sda  = (v_sda  << 1) | ((TFT_MOSI_PORT->IDR & TFT_MOSI_PIN) ? 1u : 0u);
    }
    TFT_SCK_PORT->BSRR = (uint32_t)TFT_SCK_PIN << 16;
    TFT_CS_PORT->BSRR = TFT_CS_PIN;                 /* CS = 1 */

    /* Restore MOSI (PB15) to output */
    g.Pin = TFT_MOSI_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(TFT_MOSI_PORT, &g);
    *out_miso = v_miso;
    *out_sda  = v_sda;
}

static uint16_t xpt2046_xfer(uint8_t cmd);

static void ili9341_read_diag(void) {
    uint32_t m0, s0, m1, s1;
    ili9341_read_reg_dual(0x09, 32, GPIO_PULLDOWN, &m0, &s0);
    ili9341_read_reg_dual(0x09, 32, GPIO_PULLUP,   &m1, &s1);
    tyvor_tft_diag[0] = m0; /* MISO with PULLDOWN (if >0, MISO is driven HIGH!) */
    tyvor_tft_diag[1] = s0; /* SDA  with PULLDOWN (if >0, SDA  is driven HIGH!) */
    tyvor_tft_diag[2] = m1; /* MISO with PULLUP   (if <0xFFFFFFFF, MISO is driven LOW!) */
    tyvor_tft_diag[3] = s1; /* SDA  with PULLUP   (if <0xFFFFFFFF, SDA  is driven LOW!) */

    ili9341_read_reg_dual(0x0A, 16, GPIO_PULLDOWN, &m0, &s0);
    ili9341_read_reg_dual(0x0A, 16, GPIO_PULLUP,   &m1, &s1);
    tyvor_tft_diag[4] = (m0 << 16) | (s0 & 0xFFFFu);
    tyvor_tft_diag[5] = (m1 << 16) | (s1 & 0xFFFFu);

    /* Probe XPT2046 on remapped TOUCH_DO (PB13) with PULLDOWN then PULLUP */
    GPIO_InitTypeDef g = {0};
    g.Pin = TOUCH_DO_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(TOUCH_DO_PORT, &g);
    TOUCH_CS_PORT->BSRR = (uint32_t)TOUCH_CS_PIN << 16;
    for (volatile unsigned i = 0; i < 48; ++i) __NOP();
    uint16_t t0_pd = xpt2046_xfer(0x84);
    uint16_t t1_pd = xpt2046_xfer(0xF4);
    TOUCH_CS_PORT->BSRR = TOUCH_CS_PIN;
    tyvor_tft_diag[6] = ((uint32_t)t0_pd << 16) | t1_pd;

    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(TOUCH_DO_PORT, &g);
    TOUCH_CS_PORT->BSRR = (uint32_t)TOUCH_CS_PIN << 16;
    for (volatile unsigned i = 0; i < 48; ++i) __NOP();
    uint16_t t0_pu = xpt2046_xfer(0x84);
    uint16_t t1_pu = xpt2046_xfer(0xF4);
    TOUCH_CS_PORT->BSRR = TOUCH_CS_PIN;
    tyvor_tft_diag[7] = ((uint32_t)t0_pu << 16) | t1_pu;
}

static void scan_all_pinouts(void) {
    static const struct { GPIO_TypeDef *port; uint16_t pin; } cand[10] = {
        {GPIOB, GPIO_PIN_12}, /* 0: PB12 (expected CS)    */
        {GPIOD, GPIO_PIN_8 }, /* 1: PD8  (expected RESET) */
        {GPIOD, GPIO_PIN_9 }, /* 2: PD9  (expected DC)    */
        {GPIOB, GPIO_PIN_15}, /* 3: PB15 (expected MOSI)  */
        {GPIOB, GPIO_PIN_13}, /* 4: PB13 (expected SCK)   */
        {GPIOB, GPIO_PIN_14}, /* 5: PB14 (expected MISO)  */
        {GPIOB, GPIO_PIN_10}, /* 6: PB10 (expected T_CLK) */
        {GPIOE, GPIO_PIN_15}, /* 7: PE15 (expected T_CS)  */
        {GPIOE, GPIO_PIN_14}, /* 8: PE14 (expected T_DIN) */
        {GPIOE, GPIO_PIN_13}, /* 9: PE13 (expected T_DO)  */
    };
    GPIO_InitTypeDef g = {0};
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    uint32_t hits = 0;

    for (unsigned cs = 0; cs < 10; ++cs) {
        for (unsigned dc = 0; dc < 10; ++dc) {
            if (dc == cs) continue;
            for (unsigned sck = 0; sck < 10; ++sck) {
                if (sck == cs || sck == dc) continue;
                for (unsigned mosi = 0; mosi < 10; ++mosi) {
                    if (mosi == cs || mosi == dc || mosi == sck) continue;

                    /* Set all 10 candidate pins to INPUT_PULLUP so RESET=1 and MISO=pulled high */
                    g.Mode = GPIO_MODE_INPUT;
                    g.Pull = GPIO_PULLUP;
                    g.Pin = GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
                    HAL_GPIO_Init(GPIOB, &g);
                    g.Pin = GPIO_PIN_8 | GPIO_PIN_9;
                    HAL_GPIO_Init(GPIOD, &g);
                    g.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
                    HAL_GPIO_Init(GPIOE, &g);

                    /* Configure CS, DC, SCK, MOSI as OUTPUT_PP */
                    g.Mode = GPIO_MODE_OUTPUT_PP;
                    g.Pull = GPIO_NOPULL;
                    g.Pin = cand[cs].pin;   HAL_GPIO_Init(cand[cs].port, &g);
                    g.Pin = cand[dc].pin;   HAL_GPIO_Init(cand[dc].port, &g);
                    g.Pin = cand[sck].pin;  HAL_GPIO_Init(cand[sck].port, &g);
                    g.Pin = cand[mosi].pin; HAL_GPIO_Init(cand[mosi].port, &g);

                    cand[cs].port->BSRR  = cand[cs].pin;                 /* CS = 1 */
                    cand[sck].port->BSRR = (uint32_t)cand[sck].pin << 16; /* SCK = 0 */
                    for (volatile unsigned d = 0; d < 80; ++d) __NOP();

                    /* Send command 0x09 (RDDST) with CS=0, DC=0 */
                    cand[dc].port->BSRR = (uint32_t)cand[dc].pin << 16;  /* DC = 0 */
                    cand[cs].port->BSRR = (uint32_t)cand[cs].pin << 16;  /* CS = 0 */
                    uint8_t cmd = 0x09;
                    for (int b = 7; b >= 0; --b) {
                        if (cmd & (1u << b)) cand[mosi].port->BSRR = cand[mosi].pin;
                        else cand[mosi].port->BSRR = (uint32_t)cand[mosi].pin << 16;
                        cand[sck].port->BSRR = (uint32_t)cand[sck].pin << 16;
                        __NOP(); __NOP(); __NOP(); __NOP();
                        cand[sck].port->BSRR = cand[sck].pin;
                        __NOP(); __NOP(); __NOP(); __NOP();
                    }
                    cand[sck].port->BSRR = (uint32_t)cand[sck].pin << 16;
                    cand[dc].port->BSRR  = cand[dc].pin; /* DC = 1 */

                    /* Clock 24 read bits and watch all other 6 pins for BOTH 0 and 1 transitions */
                    uint32_t rx_bits[10] = {0};
                    for (int b = 0; b < 24; ++b) {
                        cand[sck].port->BSRR = (uint32_t)cand[sck].pin << 16;
                        __NOP(); __NOP(); __NOP(); __NOP();
                        cand[sck].port->BSRR = cand[sck].pin;
                        __NOP(); __NOP(); __NOP(); __NOP();
                        for (unsigned m = 0; m < 10; ++m) {
                            if (m == cs || m == dc || m == sck || m == mosi) continue;
                            uint32_t bit = (cand[m].port->IDR & cand[m].pin) ? 1u : 0u;
                            rx_bits[m] = (rx_bits[m] << 1) | bit;
                        }
                    }
                    cand[cs].port->BSRR = cand[cs].pin; /* CS = 1 */

                    for (unsigned m = 0; m < 10; ++m) {
                        if (m == cs || m == dc || m == sck || m == mosi) continue;
                        if (rx_bits[m] != 0x00FFFFFFu && rx_bits[m] != 0u) {
                            if (!hits) {
                                tyvor_tft_diag[14] = (cs << 24) | (dc << 16) | (sck << 8) | mosi;
                                tyvor_tft_diag[15] = (m << 24) | (rx_bits[m] & 0x00FFFFFFu);
                            }
                            ++hits;
                        }
                    }
                }
            }
        }
    }
    tyvor_tft_diag[6] = hits;
}

static void ili9341_init(void) {
    /* Pulse backlight OFF briefly then ON so we know init ran */
    TFT_LED_PORT->BSRR = (uint32_t)TFT_LED_PIN << 16;
    TFT_CS_PORT->BSRR = TFT_CS_PIN;
    TFT_SCK_PORT->BSRR = (uint32_t)TFT_SCK_PIN << 16;
    TFT_RST_PORT->BSRR = TFT_RST_PIN;
    HAL_Delay(50);
    TFT_RST_PORT->BSRR = (uint32_t)TFT_RST_PIN << 16;
    HAL_Delay(50);
    TFT_RST_PORT->BSRR = TFT_RST_PIN;
    TFT_LED_PORT->BSRR = TFT_LED_PIN; /* Backlight ON */
    HAL_Delay(150);

    ili9341_cmd(0x01); /* Software Reset */
    HAL_Delay(150);

    { const uint8_t d[] = {0x39, 0x2C, 0x00, 0x34, 0x02}; ili9341_cmd(0xCB); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x00, 0xC1, 0x30};             ili9341_cmd(0xCF); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x85, 0x00, 0x78};             ili9341_cmd(0xE8); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x00, 0x00};                   ili9341_cmd(0xEA); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x64, 0x03, 0x12, 0x81};       ili9341_cmd(0xED); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x20};                         ili9341_cmd(0xF7); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x23};                         ili9341_cmd(0xC0); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x10};                         ili9341_cmd(0xC1); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x3E, 0x28};                   ili9341_cmd(0xC5); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x86};                         ili9341_cmd(0xC7); ili9341_data(d, sizeof(d)); }
    /* MADCTL = 0x28 (MV=1, BGR=1) -> 320x240 Landscape */
    { const uint8_t d[] = {0x28};                         ili9341_cmd(0x36); ili9341_data(d, sizeof(d)); }
    /* COLMOD = 0x55 (16-bit RGB565) */
    { const uint8_t d[] = {0x55};                         ili9341_cmd(0x3A); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x00, 0x18};                   ili9341_cmd(0xB1); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x08, 0x82, 0x27};             ili9341_cmd(0xB6); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x00};                         ili9341_cmd(0xF2); ili9341_data(d, sizeof(d)); }
    { const uint8_t d[] = {0x01};                         ili9341_cmd(0x26); ili9341_data(d, sizeof(d)); }

    ili9341_cmd(0x11); /* Sleep Out */
    HAL_Delay(150);
    ili9341_cmd(0x29); /* Display ON */
    HAL_Delay(50);
    ili9341_read_diag();
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

    TFT_DC_PORT->BSRR = (uint32_t)TFT_DC_PIN << 16; /* DC = 0 */
    TFT_CS_PORT->BSRR = (uint32_t)TFT_CS_PIN << 16; /* CS = 0 */
    tft_pin_delay();
    spi_bb_write_byte(0x2C);                        /* RAMWR */
    tft_pin_delay();

    TFT_DC_PORT->BSRR = TFT_DC_PIN;                 /* DC = 1 */
    tft_pin_delay();
    for (size_t i = 0; i < n; ++i) {
        spi_bb_write_byte(bytes[i]);
        if ((i & 2047u) == 2047u) {
            service_core_and_uart();
        }
    }
    tft_pin_delay();
    TFT_CS_PORT->BSRR = TFT_CS_PIN;                 /* CS = 1 */
    tft_pin_delay();
    service_core_and_uart();
    return 0;
}

/* ============================================================================
 * XPT2046 TOUCH CONTROLLER DRIVER (PB10/PE15/PE14/PE13/PE12)
 * ========================================================================== */
static void touch_delay(void) {
    for (volatile unsigned i = 0; i < 24; ++i) __NOP();
}

static uint16_t xpt2046_xfer(uint8_t cmd) {
    for (int i = 7; i >= 0; --i) {
        if (cmd & (1u << i)) TOUCH_DIN_PORT->BSRR = TOUCH_DIN_PIN;
        else TOUCH_DIN_PORT->BSRR = (uint32_t)TOUCH_DIN_PIN << 16;
        touch_delay();
        TOUCH_CLK_PORT->BSRR = TOUCH_CLK_PIN;
        touch_delay();
        TOUCH_CLK_PORT->BSRR = (uint32_t)TOUCH_CLK_PIN << 16;
    }
    touch_delay();
    uint16_t val = 0;
    for (int i = 15; i >= 0; --i) {
        TOUCH_CLK_PORT->BSRR = TOUCH_CLK_PIN;
        touch_delay();
        if (TOUCH_DO_PORT->IDR & TOUCH_DO_PIN) val |= (uint16_t)(1u << i);
        TOUCH_CLK_PORT->BSRR = (uint32_t)TOUCH_CLK_PIN << 16;
        touch_delay();
    }
    return (uint16_t)((val >> 3) & 0x0FFFu);
}

static uint16_t median3(uint16_t a, uint16_t b, uint16_t c) {
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) { b = c; }
    return a > b ? a : b;
}

static int touch_last_x = 160, touch_last_y = 120;
static int touch_down, touch_start_x, touch_start_y;

static int xpt2046_sample(int *out_x, int *out_y) {
    /* Active-low T_IRQ (pulled up when idle or unplugged) */
    if ((TOUCH_IRQ_PORT->IDR & TOUCH_IRQ_PIN) != 0) return 0;

    TOUCH_CS_PORT->BSRR = (uint32_t)TOUCH_CS_PIN << 16; /* CS = 0 */
    touch_delay();
    uint16_t z1 = xpt2046_xfer(0xB0);
    uint16_t z2 = xpt2046_xfer(0xC0);
    /* Reject if unplugged (all 1s = 4095 or all 0s) or insufficient pressure */
    if (z1 < 60 || z1 > 4000 || z2 > 4090) {
        TOUCH_CS_PORT->BSRR = TOUCH_CS_PIN;
        return 0;
    }
    uint16_t rx[3], ry[3];
    for (int i = 0; i < 3; ++i) {
        rx[i] = xpt2046_xfer(0xD0);
        ry[i] = xpt2046_xfer(0x90);
    }
    TOUCH_CS_PORT->BSRR = TOUCH_CS_PIN;                 /* CS = 1 */

    uint16_t raw_x = median3(rx[0], rx[1], rx[2]);
    uint16_t raw_y = median3(ry[0], ry[1], ry[2]);
    if (raw_x < 150 || raw_x > 3950 || raw_y < 150 || raw_y > 3950) return 0;

    /* Map calibrated XPT2046 range (240..3850) to 320x240 landscape */
    int x = ((int)raw_y - 240) * 320 / (3850 - 240);
    int y = (3850 - (int)raw_x) * 240 / (3850 - 240);
    if (x < 0) x = 0;
    if (x > 319) x = 319;
    if (y < 0) y = 0;
    if (y > 239) y = 239;
    *out_x = x;
    *out_y = y;
    return 1;
}

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    int x, y;
    if (xpt2046_sample(&x, &y)) {
        touch_last_x = x;
        touch_last_y = y;
        if (!touch_down) {
            touch_down = 1;
            touch_start_x = x;
            touch_start_y = y;
        }
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        if (touch_down) {
            touch_down = 0;
            int dx = touch_last_x - touch_start_x;
            int dy = touch_last_y - touch_start_y;
            if (abs(dx) > 35 || abs(dy) > 35) {
                tyvor_ui321_gesture(&oxbot_ui, dx, dy);
            }
        }
        data->point.x = touch_last_x;
        data->point.y = touch_last_y;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* ============================================================================
 * USART2 (PA2/PA3) PROTOCOL V2 BRIDGE
 * ========================================================================== */
static int uart_tx_start(void *ctx, const uint8_t *bytes, uint16_t n) {
    (void)ctx;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (tx_busy) { __set_PRIMASK(mask); return -1; }
    tx_bytes = bytes;
    tx_index = 0;
    tx_length = n;
    tx_busy = 1;
    __HAL_UART_CLEAR_FLAG(&huart2, UART_FLAG_TC);
    __DMB();
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_TXE);
    __set_PRIMASK(mask);
    return 0;
}

static int uart_tx_state(void *ctx) { (void)ctx; return tx_busy ? 0 : 1; }

static void uart_tx_abort(void *ctx) {
    (void)ctx;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);
    __HAL_UART_DISABLE_IT(&huart2, UART_IT_TC);
    tx_busy = 0;
    tx_bytes = NULL;
    __DMB();
    __set_PRIMASK(mask);
}

static int uart_write_cb(void *ctx, const uint8_t *bytes, uint16_t n) {
    (void)ctx;
    if (!bytes || !n || n > TYVOR_FRAME_MAX) return -1;
    /* Feed Protocol v2 frame from Core Bridge into UI Endpoint AND physical USART2 TX */
    (void)tyvor_ui_endpoint_feed(&ui_endpoint, bytes, n, board_millis());
    (void)tyvor_async_tx_write(&tx_queue, bytes, n, board_millis());
    return (int)n;
}

static volatile int external_uart_peer = 0;

static int ui_endpoint_write_cb(void *ctx, const uint8_t *bytes, uint16_t n) {
    (void)ctx;
    if (!bytes || !n || n > TYVOR_FRAME_MAX) return -1;
    if (external_uart_peer) return (int)n;
    /* Feed Protocol v2 frame from UI Endpoint into Core Bridge */
    if (tyvor_core_bridge_feed(&uart_bridge, bytes, n, board_millis()) != 0) return -1;
    return (int)n;
}

static size_t uart_read_bytes_local(uint8_t *bytes, size_t cap, int *err) {
    *err = 0;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    size_t used = 0;
    if (rx_loss) {
        *err = rx_loss;
        rx_loss = 0;
        rx_tail = rx_head;
    } else {
        while (used < cap && used < 64 && rx_tail != rx_head) {
            bytes[used++] = rx_ring[rx_tail];
            rx_tail = (uint16_t)((rx_tail + 1u) & 255u);
        }
    }
    __set_PRIMASK(mask);
    return used;
}

void USART2_IRQHandler(void) {
    uint32_t sr = USART2->SR;
    if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        uint8_t b = (uint8_t)USART2->DR;
        if (sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
            if (!rx_loss) rx_loss = 1;
        } else {
            uint16_t next = (uint16_t)((rx_head + 1u) & 255u);
            if (next == rx_tail) rx_loss = 2;
            else { rx_ring[rx_head] = b; __DMB(); rx_head = next; }
        }
    }
    if ((USART2->SR & USART_SR_TXE) && (USART2->CR1 & USART_CR1_TXEIE)) {
        if (tx_busy && tx_index < tx_length) USART2->DR = tx_bytes[tx_index++];
        if (tx_index >= tx_length) {
            __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);
            __HAL_UART_ENABLE_IT(&huart2, UART_IT_TC);
        }
    }
    if ((USART2->SR & USART_SR_TC) && (USART2->CR1 & USART_CR1_TCIE)) {
        __HAL_UART_DISABLE_IT(&huart2, UART_IT_TC);
        __DMB();
        tx_busy = 0;
    }
}

static int receive_core_cb(void *ctx, const uint8_t *b, size_t n, uint64_t now) {
    return tyvor_core_bridge_feed(ctx, b, n, now);
}

/* ============================================================================
 * CRSFv2 UART6 (PC6 TX -> ELRS RX, PC7 RX -> ELRS TX, 420000 bps)
 * ========================================================================== */
static tyvor_crsf_t crsf_link;
static volatile uint8_t crsf_rx_ring[256];
static volatile uint16_t crsf_rx_head = 0;
static volatile uint16_t crsf_rx_tail = 0;
static uint64_t crsf_last_rc_ms = 0;
static uint64_t crsf_last_ping_ms = 0;
static const uint16_t crsf_rate_period_ms[4] = {20u, 7u, 4u, 2u}; /* 50/150/250/500 Hz */

void USART6_IRQHandler(void) {
    uint32_t sr = USART6->SR;
    if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        uint8_t b = (uint8_t)USART6->DR;
        if (!(sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE))) {
            uint16_t next = (uint16_t)((crsf_rx_head + 1u) & 255u);
            if (next != crsf_rx_tail) {
                crsf_rx_ring[crsf_rx_head] = b;
                __DMB();
                crsf_rx_head = next;
            }
        }
    }
}

static int crsf_uart_tx(void *ctx, const uint8_t *bytes, size_t len) {
    (void)ctx;
    if (!bytes || !len) return -1;
    return (HAL_UART_Transmit(&huart6, (uint8_t *)bytes, (uint16_t)len, 5) == HAL_OK) ? 0 : -1;
}

/* ============================================================================
 * CLOCK + GPIO + SPI2 + UART2 INITIALIZATION
 * ========================================================================== */
static int try_clock_hse(uint32_t hse_state) {
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = hse_state;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 8;
    osc.PLL.PLLN = 336;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 7;
    return HAL_RCC_OscConfig(&osc) == HAL_OK ? 0 : -1;
}

static int hardware_init(void) {
    if (HAL_Init() != HAL_OK) return -1;
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Try 8MHz Crystal (X2), then 8MHz ST-LINK MCO Bypass, then 16MHz HSI PLL */
    if (try_clock_hse(RCC_HSE_ON) != 0 && try_clock_hse(RCC_HSE_BYPASS) != 0) {
        RCC_OscInitTypeDef osc = {0};
        osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
        osc.HSIState = RCC_HSI_ON;
        osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        osc.PLL.PLLState = RCC_PLL_ON;
        osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
        osc.PLL.PLLM = 16;
        osc.PLL.PLLN = 336;
        osc.PLL.PLLP = RCC_PLLP_DIV2;
        osc.PLL.PLLQ = 7;
        if (HAL_RCC_OscConfig(&osc) != HAL_OK) return -2;
    }

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) return -3;
    SystemCoreClockUpdate();

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    /* 15-pin Short-Circuit Matrix Probe across P1 soldered region */
    static const struct { GPIO_TypeDef *port; uint16_t pin; } pmap[15] = {
        {GPIOB, GPIO_PIN_12}, /*  0: Pin 3  CS    */
        {GPIOD, GPIO_PIN_8 }, /*  1: Pin 4  RESET */
        {GPIOD, GPIO_PIN_9 }, /*  2: Pin 5  DC    */
        {GPIOB, GPIO_PIN_15}, /*  3: Pin 6  MOSI  */
        {GPIOB, GPIO_PIN_13}, /*  4: Pin 7  SCK   */
        {GPIOD, GPIO_PIN_10}, /*  5: Pin 8  LED   */
        {GPIOB, GPIO_PIN_14}, /*  6: Pin 9  MISO  */
        {GPIOB, GPIO_PIN_10}, /*  7: Pin 10 T_CLK */
        {GPIOE, GPIO_PIN_15}, /*  8: Pin 11 T_CS  */
        {GPIOE, GPIO_PIN_14}, /*  9: Pin 12 T_DIN */
        {GPIOE, GPIO_PIN_13}, /* 10: Pin 13 T_DO  */
        {GPIOE, GPIO_PIN_12}, /* 11: Pin 14 T_IRQ */
        {GPIOB, GPIO_PIN_11}, /* 12: Neighbor PB11 */
        {GPIOE, GPIO_PIN_11}, /* 13: Neighbor PE11 */
        {GPIOD, GPIO_PIN_11}, /* 14: Neighbor PD11 */
    };
    GPIO_InitTypeDef g = {0};
    for (unsigned k = 0; k < 15; ++k) {
        /* Test driving HIGH against PULLDOWN */
        g.Mode = GPIO_MODE_INPUT;
        g.Speed = GPIO_SPEED_FREQ_HIGH;
        g.Pull = GPIO_PULLDOWN;
        g.Pin = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
        HAL_GPIO_Init(GPIOB, &g);
        g.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
        HAL_GPIO_Init(GPIOD, &g);
        g.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
        HAL_GPIO_Init(GPIOE, &g);

        g.Mode = GPIO_MODE_OUTPUT_PP;
        g.Pull = GPIO_NOPULL;
        g.Pin = pmap[k].pin;
        HAL_GPIO_Init(pmap[k].port, &g);
        pmap[k].port->BSRR = pmap[k].pin;
        for (volatile unsigned d = 0; d < 400; ++d) __NOP();
        uint16_t hi_mask = 0;
        for (unsigned m = 0; m < 15; ++m) {
            if (pmap[m].port->IDR & pmap[m].pin) hi_mask |= (uint16_t)(1u << m);
        }

        /* Test driving LOW against PULLUP */
        g.Mode = GPIO_MODE_INPUT;
        g.Pull = GPIO_PULLUP;
        g.Pin = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
        HAL_GPIO_Init(GPIOB, &g);
        g.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
        HAL_GPIO_Init(GPIOD, &g);
        g.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
        HAL_GPIO_Init(GPIOE, &g);

        g.Mode = GPIO_MODE_OUTPUT_PP;
        g.Pull = GPIO_NOPULL;
        g.Pin = pmap[k].pin;
        HAL_GPIO_Init(pmap[k].port, &g);
        pmap[k].port->BSRR = (uint32_t)pmap[k].pin << 16;
        for (volatile unsigned d = 0; d < 400; ++d) __NOP();
        uint16_t lo_mask = 0;
        for (unsigned m = 0; m < 15; ++m) {
            if (!(pmap[m].port->IDR & pmap[m].pin)) lo_mask |= (uint16_t)(1u << m);
        }
        tyvor_tft_diag[16 + k] = ((uint32_t)hi_mask << 16) | lo_mask;
    }

    /* Probe all 12 soldered pins with PULLDOWN and PULLUP to identify external pull-ups/loads */
    g.Mode = GPIO_MODE_INPUT;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pull = GPIO_PULLDOWN;
    g.Pin = GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOD, &g);
    g.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &g);
    HAL_Delay(2);
    tyvor_tft_diag[8]  = GPIOB->IDR;
    tyvor_tft_diag[9]  = GPIOD->IDR;
    tyvor_tft_diag[10] = GPIOE->IDR;

    g.Pull = GPIO_PULLUP;
    g.Pin = GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOD, &g);
    g.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &g);
    HAL_Delay(2);
    tyvor_tft_diag[11] = GPIOB->IDR;
    tyvor_tft_diag[12] = GPIOD->IDR;
    tyvor_tft_diag[13] = GPIOE->IDR;
    (void)scan_all_pinouts;

    /* Discovery LEDs (PD12..PD15) + TFT control pins (PD8=RST, PD9=DC, PD10=LED) */
    g.Pin = DISCOVERY_LED_MASK | TFT_RST_PIN | TFT_DC_PIN | TFT_LED_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOD, &g);

    /* User Button PA0 */
    g.Pin = GPIO_PIN_0;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    /* GPIOB Outputs: TFT CS (PB12), MOSI (PB15) + Touch CLK (PB10) */
    g.Pin = TFT_CS_PIN | TFT_MOSI_PIN | TOUCH_CLK_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &g);
    TFT_CS_PORT->BSRR = TFT_CS_PIN;

    /* GPIOB Inputs: TFT MISO (PB14) + Touch DO (PB13) */
    g.Pin = TFT_MISO_PIN | TOUCH_DO_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &g);
    (void)hspi2;

    /* GPIOE Outputs: TFT SCK (PE13) + Touch CS (PE15), Touch DIN (PE14) */
    g.Pin = TFT_SCK_PIN | TOUCH_CS_PIN | TOUCH_DIN_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOE, &g);
    TOUCH_CS_PORT->BSRR = TOUCH_CS_PIN;
    TFT_SCK_PORT->BSRR = (uint32_t)TFT_SCK_PIN << 16;

    /* GPIOE Input: Touch IRQ (PE12) + Gimbal Switches SA (PE7) & SB (PE8) with pull-up */
    g.Pin = TOUCH_IRQ_PIN | GPIO_PIN_7 | GPIO_PIN_8;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOE, &g);

    /* Hardware Gimbal ADC1 Inputs: PC1(IN11=Roll), PC2(IN12=Pitch), PC4(IN14=Thr), PC5(IN15=Yaw) */
    __HAL_RCC_ADC1_CLK_ENABLE();
    g.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &g);

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    (void)HAL_ADC_Init(&hadc1);

    /* USART2: PA2 (TX), PA3 (RX) @ 115200 8N1 */
    g.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &g);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 460800;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) == HAL_OK) {
        tyvor_async_tx_init(&tx_queue, uart_tx_start, uart_tx_state, uart_tx_abort, NULL);
        HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART2_IRQn);
        __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
        __HAL_UART_ENABLE_IT(&huart2, UART_IT_ERR);
    }

    /* USART6: PC6 (TX -> ELRS RX), PC7 (RX -> ELRS TX) @ 420000 bps CRSFv2 */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &g);

    huart6.Instance = USART6;
    huart6.Init.BaudRate = TYVOR_CRSF_BAUDRATE;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) == HAL_OK) {
        tyvor_crsf_init(&crsf_link, crsf_uart_tx, NULL);
        HAL_NVIC_SetPriority(USART6_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(USART6_IRQn);
        __HAL_UART_ENABLE_IT(&huart6, UART_IT_RXNE);
    }

    /* Initialize TFT ILI9341 before starting IWDG */
    ili9341_init();

    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg.Init.Reload = 1999; /* ~4 s timeout */
    (void)HAL_IWDG_Init(&hiwdg);
    return 0;
}

static unsigned active_model_slot = 0;

static void select_next_model(void) {
    uint64_t now = board_millis();
    unsigned next_slot = (active_model_slot + 1u) % 2u;
    /* Ensure Core is in DISARMED SAFE state with fresh input so model change is permitted */
    tyvor_discovery_demo.mode = DISCOVERY_SAFE;
    service_core_and_uart();
    now = board_millis();
    if (tyvor_core_load_model(&tyvor_discovery_demo.core, next_slot, now) == 0) {
        active_model_slot = next_slot;
        tyvor_discovery_demo.core.models.active.revision = (uint32_t)(UI321_MODEL + 1u);
        tyvor_discovery_demo.next_input_ms = now;
        /* Trigger immediate Protocol v2 read lease cycle for the new model */
        uart_bridge.last_read_ms = 0;
        ui_endpoint.read_active = 0;
        ui_endpoint.next_read_ms = now;
        service_core_and_uart();
    }
}

static uint8_t stm32_qs_power_idx = 2;
static uint8_t stm32_qs_rate_idx = 2;
static uint8_t stm32_qs_finder_on = 0;
static uint8_t stm32_qs_rf_on = 0;
static uint8_t stm32_qs_bind_on = 0;
static uint8_t stm32_qs_telem_idx = 2;
static uint8_t stm32_qs_model_match = 1;
static uint8_t stm32_qs_ext_tx = 0;
static uint8_t stm32_qs_thr_lock = 1;
static uint8_t stm32_qs_stick_mode = 2;

static uint8_t on_remote_ui_request(void *ctx, uint8_t opcode, uint8_t page) {
    (void)ctx;
    ++uart_bridge.read_requests;
    if (opcode == TYVOR_REQ_PAGE) {
        if (page == 1) {
            if (oxbot_ui.page != UI321_MODEL) tyvor_ui321_open(&oxbot_ui, UI321_MODEL);
            select_next_model();
        } else if (page == 2) {
            tyvor_ui321_action(&oxbot_ui, UI321_NEXT);
            tyvor_discovery_demo.core.models.active.revision = (uint32_t)(oxbot_ui.page + 1u);
            uart_bridge.last_read_ms = 0;
        }
    } else if (opcode == TYVOR_REQ_QUICKSET) {
        uint8_t group = page & 0xF0u;
        uint8_t val   = page & 0x0Fu;
        if (group == 0x00u && val < 5u) { stm32_qs_power_idx = val; (void)tyvor_crsf_send_param(&crsf_link, 6u, val); }
        else if (group == 0x10u && val < 4u) { stm32_qs_rate_idx = val; (void)tyvor_crsf_send_param(&crsf_link, 1u, val); }
        else if (group == 0x20u) stm32_qs_finder_on = (val != 0u);
        else if (group == 0x30u) stm32_qs_rf_on = (val != 0u);
        else if (group == 0x40u) { stm32_qs_bind_on = (val != 0u); if (val != 0u) (void)tyvor_crsf_send_param(&crsf_link, 0xFFu, 1u); }
        else if (group == 0x50u && val < 4u) { stm32_qs_telem_idx = val; (void)tyvor_crsf_send_param(&crsf_link, 2u, val); }
        else if (group == 0x60u) { stm32_qs_model_match = (val != 0u); (void)tyvor_crsf_send_param(&crsf_link, 4u, val ? 1u : 0u); }
        else if (group == 0x70u) stm32_qs_ext_tx = (val != 0u);
        else if (group == 0x80u) {
            stm32_qs_thr_lock = (val != 0u);
            tyvor_discovery_demo.mode = stm32_qs_thr_lock ? DISCOVERY_SAFE : DISCOVERY_MIXER;
        } else if (group == 0x90u) {
            stm32_qs_stick_mode = (val != 0u) ? 2u : 1u;
        }
        uart_bridge.last_read_ms = 0;
    }
    return 0;
}

static void on_top_model_clicked(lv_event_t *e) {
    (void)e;
    if (oxbot_ui.page != UI321_MODEL) {
        tyvor_ui321_open(&oxbot_ui, UI321_MODEL);
    } else {
        select_next_model();
    }
}

static void on_body_clicked(lv_event_t *e) {
    (void)e;
    if (oxbot_ui.page == UI321_MODEL) {
        select_next_model();
    }
}

static uint16_t stm32_read_adc1_ch(uint32_t ch) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ch;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) return 2048u;
    if (HAL_ADC_Start(&hadc1) != HAL_OK) return 2048u;
    if (HAL_ADC_PollForConversion(&hadc1, 2) != HAL_OK) {
        (void)HAL_ADC_Stop(&hadc1);
        return 2048u;
    }
    uint16_t v = (uint16_t)HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);
    return v;
}

static void stm32_poll_gimbal_adc(uint64_t now) {
    static const uint32_t adc_chs[4] = {
        ADC_CHANNEL_11, /* PC1 -> CH1 Roll */
        ADC_CHANNEL_12, /* PC2 -> CH2 Pitch */
        ADC_CHANNEL_14, /* PC4 -> CH3 Throttle */
        ADC_CHANNEL_15  /* PC5 -> CH4 Yaw */
    };
    static uint8_t hw_present[4] = {0, 0, 0, 0};
    static int32_t filt[4] = {2048, 2048, 2048, 2048};
    static uint32_t seq = 1u;
    static uint64_t next_ms = 0;

    if (now < next_ms) return;
    next_ms = now + TYVOR_CORE_PERIOD_MS;

    int any_hw = 0;
    int16_t axes[4] = {0, 0, -1000, 0};
    for (int i = 0; i < 4; ++i) {
        uint16_t raw = stm32_read_adc1_ch(adc_chs[i]);
        if (raw >= 120u && raw <= 3975u) {
            hw_present[i] = 1;
        }
        if (hw_present[i]) {
            any_hw = 1;
            filt[i] = (filt[i] * 3 + (int32_t)raw) / 4;
            /* Map ~300..3795 -> -1000..+1000 */
            int32_t v = ((filt[i] - 2048) * 1000) / 1747;
            if (v < -1000) v = -1000;
            if (v > 1000) v = 1000;
            if (i != 2 && v > -18 && v < 18) v = 0; /* Center deadband for Roll/Pitch/Yaw */
            axes[i] = (int16_t)v;
        }
    }

    if (any_hw) {
        int8_t sw[2] = {
            (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_7) == GPIO_PIN_RESET) ? 1 : 0, /* SA (PE7) */
            (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_8) == GPIO_PIN_RESET) ? 1 : 0  /* SB (PE8) */
        };
        tyvor_input_sample_t sample = tyvor_bench_input(&tyvor_discovery_demo.core.models.active,
                                                        axes, sw, ++seq, now);
        (void)tyvor_core_input(&tyvor_discovery_demo.core, &sample, now);
        (void)tyvor_core_poll(&tyvor_discovery_demo.core, now);
        tyvor_discovery_demo.next_input_ms = now + 100u; /* Prevent synthetic bench overwrite */
    }
}

static void service_core_and_uart(void) {
    if (!tyvor_discovery_demo.core.initialized) return;
    uint64_t target = board_millis();
    while (tyvor_discovery_demo.last_poll_ms + TYVOR_CORE_PERIOD_MS <= target) {
        uint64_t step = tyvor_discovery_demo.last_poll_ms + TYVOR_CORE_PERIOD_MS;
        (void)discovery_demo_poll(&tyvor_discovery_demo, step, 0);
        tyvor_core_bridge_poll(&uart_bridge, step);
        tyvor_ui_endpoint_poll(&ui_endpoint, step);
    }
    if (target >= tyvor_discovery_demo.last_poll_ms) {
        (void)discovery_demo_poll(&tyvor_discovery_demo, target, 0);
    }
    stm32_poll_gimbal_adc(target);

    /* Service CRSFv2 2-way link on USART6 (RX telemetry + TX RC channels) */
    while (crsf_rx_tail != crsf_rx_head) {
        uint8_t b = crsf_rx_ring[crsf_rx_tail];
        __DMB();
        crsf_rx_tail = (uint16_t)((crsf_rx_tail + 1u) & 255u);
        if (tyvor_crsf_feed_byte(&crsf_link, b, target) > 0) {
            (void)tyvor_core_telemetry(&tyvor_discovery_demo.core, &crsf_link.telem, target);
        }
    }
    if (!tyvor_crsf_telem_live(&crsf_link, target) && (target - crsf_last_ping_ms) >= 2000u) {
        crsf_last_ping_ms = target;
        (void)tyvor_crsf_send_ping(&crsf_link);
    }
    if (stm32_qs_rf_on && tyvor_core_rf_enabled()) {
        uint16_t period = crsf_rate_period_ms[stm32_qs_rate_idx & 3u];
        if ((target - crsf_last_rc_ms) >= period) {
            crsf_last_rc_ms = target;
            (void)tyvor_crsf_send_rc(&crsf_link, tyvor_discovery_demo.core.channels, TYVOR_CHANNELS);
        }
    }

    tyvor_async_tx_poll(&tx_queue, target);
    uint8_t rx_buf[64];
    int rx_err = 0;
    size_t rx_n = uart_read_bytes_local(rx_buf, sizeof(rx_buf), &rx_err);
    if (rx_err) {
        tyvor_bench_transport_error(&uart_transport, &uart_bridge.transport.link, rx_err == 2);
    } else if (rx_n) {
        external_uart_peer = 1;
        (void)tyvor_bench_transport_receive(&uart_transport, rx_buf, rx_n, target, receive_core_cb, &uart_bridge);
    }
    tyvor_core_bridge_poll(&uart_bridge, target);
    tyvor_ui_endpoint_poll(&ui_endpoint, target);
    tyvor_async_tx_poll(&tx_queue, board_millis());

    uint16_t leds = tyvor_discovery_demo.leds;
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET || touch_down) {
        leds |= DISCOVERY_BLUE;
    }
    GPIOD->BSRR = ((uint32_t)(DISCOVERY_LED_MASK & ~leds) << 16) | (leds & DISCOVERY_LED_MASK);
    (void)HAL_IWDG_Refresh(&hiwdg);
}

int main(void) {
    if (hardware_init() != 0) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
        GPIOD->BSRR = DISCOVERY_RED;
        for (;;) __NOP();
    }

    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);

    tyvor_ui321_port_t port = {NULL, 320, 240, tft_write_region, ui_micros_cb};
    if (tyvor_ui321_init(&oxbot_ui, port, lvgl_draw_buffer, sizeof(lvgl_draw_buffer)) != 0) {
        GPIOD->BSRR = DISCOVERY_RED;
        for (;;) __NOP();
    }

    /* Make top-bar model label and body interactive for Oxbot Model Selection */
    lv_obj_add_flag(oxbot_ui.top_model, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(oxbot_ui.top_model, on_top_model_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(oxbot_ui.body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(oxbot_ui.body, on_body_clicked, LV_EVENT_CLICKED, NULL);

    touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(touch_indev, oxbot_ui.display);
    lv_indev_set_read_cb(touch_indev, lvgl_touch_read_cb);

    /* Initialize Core TX15 in stationary SAFE mode (no auto-moving sticks!) */
    uint64_t now = board_millis();
    (void)discovery_demo_init(&tyvor_discovery_demo, now, 0);
    tyvor_discovery_demo.mode = DISCOVERY_SAFE;

    tyvor_bench_transport_init(&uart_transport, uart_write_cb, NULL, 1);
    (void)tyvor_core_bridge_init(&uart_bridge, &tyvor_discovery_demo.core,
                                 UINT64_C(0x5459564F52444953),
                                 tyvor_bench_transport_write, &uart_transport, now);
    tyvor_bridge_set_read_service(&uart_bridge.transport, on_remote_ui_request, NULL);
    (void)tyvor_ui_endpoint_init(&ui_endpoint,
                                 UINT64_C(0x5459564F52554933),
                                 ui_endpoint_write_cb, NULL, now);

    uint64_t next_ui_ms = now;
    int btn_prev = 0;
    uint64_t btn_down_ms = 0;
    int btn_long_handled = 0;

    for (;;) {
        service_core_and_uart();
        now = board_millis();

        /* Handle PA0 Blue Button:
         *  - Short press (< 450ms): Next Oxbot UI Page (HOME -> MODEL -> INPUTS -> MIXES -> OUTPUTS -> TELEMETRY -> SETTINGS -> QUICK -> CONNECTIONS)
         *  - Hold (450ms..1600ms): Select / Switch Model (BENCH MODEL A <-> BENCH MODEL B)
         *  - Long Hold (>= 1600ms): Toggle Stick Gimbal Test (SAFE stationary <-> MIXER live stick sweep)
         */
        int btn = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET;
        if (btn && !btn_prev) {
            btn_down_ms = now;
            btn_long_handled = 0;
        } else if (btn && btn_long_handled == 1 && now - btn_down_ms >= 1600) {
            btn_long_handled = 2;
            tyvor_discovery_demo.mode = (tyvor_discovery_demo.mode == DISCOVERY_SAFE) ? DISCOVERY_MIXER : DISCOVERY_SAFE;
            tyvor_ui321_open(&oxbot_ui, UI321_INPUTS);
            tyvor_discovery_demo.core.models.active.revision = (uint32_t)(UI321_INPUTS + 1u);
            uart_bridge.last_read_ms = 0;
            lv_obj_invalidate(oxbot_ui.root);
        } else if (btn && !btn_long_handled && now - btn_down_ms >= 450) {
            btn_long_handled = 1;
            if (oxbot_ui.page != UI321_MODEL) {
                tyvor_ui321_open(&oxbot_ui, UI321_MODEL);
            }
            select_next_model();
            lv_obj_invalidate(oxbot_ui.root);
        } else if (!btn && btn_prev) {
            if (!btn_long_handled && now - btn_down_ms >= 25) {
                unsigned next_page = (oxbot_ui.page + 1u) % UI321_PAGE_COUNT;
                tyvor_ui321_open(&oxbot_ui, next_page);
                tyvor_discovery_demo.core.models.active.revision = (uint32_t)(oxbot_ui.page + 1u);
                uart_bridge.last_read_ms = 0;
                lv_obj_invalidate(oxbot_ui.root);
            }
        }
        btn_prev = btn;

        /* Update LVGL Oxbot UI at ~30 FPS strictly from Protocol v2 UI Endpoint (mock = 0 -> CORE) */
        if (now >= next_ui_ms) {
            next_ui_ms = now + 33;
            tyvor_ui321_view_t ui_view;
            if (tyvor_ui321_project(&ui_endpoint.state, now, 0, now, &ui_view) == 0) {
                tyvor_ui321_update(&oxbot_ui, &ui_view);
            }
            if (oxbot_ui.page == UI321_MODEL && oxbot_ui.field[3]) {
                char sel_buf[64];
                snprintf(sel_buf, sizeof(sel_buf),
                         active_model_slot == 0
                             ? "> [1] MODEL A (4 MIX)   [2] MODEL B"
                             : "  [1] MODEL A         > [2] MODEL B (2 MIX)");
                tyvor_ui321_text(oxbot_ui.field[3], sel_buf);
                lv_obj_set_style_text_color(oxbot_ui.field[3], lv_color_hex(UI321_SUCCESS), 0);
            }
            (void)lv_timer_handler();
            tyvor_ui321_frame(&oxbot_ui);
        }
    }
}

void HardFault_Handler(void)  { GPIOD->BSRR = DISCOVERY_RED; for (;;) __NOP(); }
void MemManage_Handler(void)  { GPIOD->BSRR = DISCOVERY_RED; for (;;) __NOP(); }
void BusFault_Handler(void)   { GPIOD->BSRR = DISCOVERY_RED; for (;;) __NOP(); }
void UsageFault_Handler(void) { GPIOD->BSRR = DISCOVERY_RED; for (;;) __NOP(); }
void NMI_Handler(void)        { GPIOD->BSRR = DISCOVERY_RED; for (;;) __NOP(); }
