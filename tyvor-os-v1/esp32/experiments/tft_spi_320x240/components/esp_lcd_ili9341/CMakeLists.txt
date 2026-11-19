get_filename_component(TYVOR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(ILI_ROOT "${TYVOR_ROOT}/.tools/sdk/esp_lcd_ili9341-2.1.0")
idf_component_register(SRCS "${ILI_ROOT}/esp_lcd_ili9341.c" INCLUDE_DIRS "${ILI_ROOT}/include" REQUIRES esp_lcd esp_driver_gpio)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ESP_LCD_ILI9341_VER_MAJOR=2 ESP_LCD_ILI9341_VER_MINOR=1 ESP_LCD_ILI9341_VER_PATCH=0)
