get_filename_component(TYVOR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(UI2 "${TYVOR_ROOT}/experiments/tyvor_ui_v2")
set(TFT "${TYVOR_ROOT}/experiments/tft_spi_320x240")
set(TFT_ESP "${TYVOR_ROOT}/esp32/experiments/tft_spi_320x240/main")
idf_component_register(SRCS "main.c" "${TFT_ESP}/tft_spi_backend.c" "${TFT_ESP}/board_pins.c"
    "${UI2}/ui.c" "${UI2}/ui_view.c" "${UI2}/ui_theme.c" "${UI2}/ui_widgets.c" "${UI2}/ui_pages.c"
    "${TFT}/display_contract.c" "${TFT}/tft_presenter.c"
    "${TYVOR_ROOT}/shared/tyvor_protocol.c" "${TYVOR_ROOT}/shared/tyvor_link.c" "${TYVOR_ROOT}/shared/tyvor_ui.c"
    "${TYVOR_ROOT}/shared/tyvor_core_wire.c" "${TYVOR_ROOT}/shared/tyvor_ui_state.c" "${TYVOR_ROOT}/shared/tyvor_ui_endpoint.c"
    "${TYVOR_ROOT}/shared/tyvor_async_tx.c" "${TYVOR_ROOT}/shared/tyvor_bench_transport.c" "${TYVOR_ROOT}/shared/tyvor_diagnostics.c"
    INCLUDE_DIRS "." "${UI2}" "${TFT}" "${TFT_ESP}" "${TYVOR_ROOT}/shared"
    REQUIRES lvgl esp_lcd_ili9341 esp_lcd esp_driver_spi esp_driver_uart esp_driver_gpio esp_timer esp_psram heap esp_hw_support bootloader_support)
target_compile_definitions(${COMPONENT_LIB} PRIVATE TYVOR_BENCH_ONLY=1 TYVOR_DIAGNOSTICS=1)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -Wall -Wextra -Werror)
