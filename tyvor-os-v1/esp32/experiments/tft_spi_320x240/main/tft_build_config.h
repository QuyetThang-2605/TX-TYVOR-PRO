#ifndef TYVOR_TFT_BUILD_CONFIG_H
#define TYVOR_TFT_BUILD_CONFIG_H
#include "sdkconfig.h"
/* Kconfig omits disabled booleans from sdkconfig.h; normalize only experiment flags. */
#ifndef CONFIG_TYVOR_TFT_BGR
#define CONFIG_TYVOR_TFT_BGR 0
#endif
#ifndef CONFIG_TYVOR_TFT_BACKLIGHT_ACTIVE_LOW
#define CONFIG_TYVOR_TFT_BACKLIGHT_ACTIVE_LOW 0
#endif
#ifndef CONFIG_TYVOR_TFT_SIMULATOR
#define CONFIG_TYVOR_TFT_SIMULATOR 0
#endif
#endif
