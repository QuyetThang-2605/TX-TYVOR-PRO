#include "board_pins.h"
#include "sdkconfig.h"
tyvor_display_pins_t tyvor_tft_board_pins(void) {
    return (tyvor_display_pins_t){.sclk=CONFIG_TYVOR_TFT_SCLK,.mosi=CONFIG_TYVOR_TFT_MOSI,.miso=CONFIG_TYVOR_TFT_MISO,
        .cs=CONFIG_TYVOR_TFT_CS,.dc=CONFIG_TYVOR_TFT_DC,.reset=CONFIG_TYVOR_TFT_RESET,.backlight=CONFIG_TYVOR_TFT_BACKLIGHT,
#if CONFIG_TYVOR_TFT_BOARD_VERIFIED
        .board_verified=1,
#endif
#if CONFIG_TYVOR_TFT_MODULE_VERIFIED
        .module_verified=1,
#endif
#if CONFIG_TYVOR_TFT_HARDWARE_GATE
        .hardware_gate=1,
#endif
    };
}
