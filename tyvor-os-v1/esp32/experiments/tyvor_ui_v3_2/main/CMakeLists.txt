get_filename_component(TYVOR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(UI32 "${TYVOR_ROOT}/experiments/tyvor_ui_v3_2")
set(TFT "${TYVOR_ROOT}/experiments/tft_spi_320x240")
set(TFT_ESP "${TYVOR_ROOT}/esp32/experiments/tft_spi_320x240/main")
idf_component_register(SRCS "main.c" "${TFT_ESP}/tft_spi_backend.c" "${TFT_ESP}/board_pins.c"
    "${UI32}/ui.c" "${UI32}/ui_view.c" "${UI32}/ui_theme.c" "${UI32}/ui_widgets.c" "${UI32}/ui_pages.c" "${UI32}/ui_shapes.c" "${UI32}/timer_font.c" "${UI32}/branding.c" "${UI32}/brand_assets.c" "${UI32}/render_quality.c" "${UI32}/icons.c" "${UI32}/icon_data.c" "${TYVOR_ROOT}/experiments/tyvor_ui_v2/ui_view.c"
    "${TFT}/display_contract.c" "${TFT}/tft_presenter.c"
    "${TYVOR_ROOT}/shared/tyvor_protocol.c" "${TYVOR_ROOT}/shared/tyvor_link.c" "${TYVOR_ROOT}/shared/tyvor_ui.c"
    "${TYVOR_ROOT}/shared/tyvor_core_wire.c" "${TYVOR_ROOT}/shared/tyvor_ui_state.c" "${TYVOR_ROOT}/shared/tyvor_ui_endpoint.c"
    "${TYVOR_ROOT}/shared/tyvor_async_tx.c" "${TYVOR_ROOT}/shared/tyvor_bench_transport.c" "${TYVOR_ROOT}/shared/tyvor_diagnostics.c"
    INCLUDE_DIRS "." "${UI32}" "${TFT}" "${TFT_ESP}" "${TYVOR_ROOT}/shared"
    REQUIRES lvgl esp_lcd_ili9341 esp_lcd esp_driver_spi esp_driver_uart esp_driver_gpio esp_timer esp_psram heap esp_hw_support bootloader_support)
target_compile_definitions(${COMPONENT_LIB} PRIVATE TYVOR_BENCH_ONLY=1 TYVOR_DIAGNOSTICS=1)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -Wall -Wextra -Werror)
