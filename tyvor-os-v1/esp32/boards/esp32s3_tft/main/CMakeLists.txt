get_filename_component(TYVOR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(LVGL_ROOT "${TYVOR_ROOT}/.tools/sdk/lvgl-9.2.2")
set(UI321 "${TYVOR_ROOT}/experiments/tyvor_ui_v3_2_1")
set(TFT "${TYVOR_ROOT}/experiments/tft_spi_320x240")
file(GLOB_RECURSE LVGL_SOURCES "${LVGL_ROOT}/src/*.c")

idf_component_register(
    SRCS
        "main.c"
        "${UI321}/ui.c"
        "${UI321}/ui_view.c"
        "${UI321}/ui_theme.c"
        "${UI321}/ui_widgets.c"
        "${UI321}/ui_pages.c"
        "${UI321}/ui_shapes.c"
        "${UI321}/timer_font.c"
        "${UI321}/branding.c"
        "${UI321}/brand_assets.c"
        "${UI321}/render_quality.c"
        "${UI321}/icons.c"
        "${UI321}/icon_data.c"
        "${TYVOR_ROOT}/experiments/tyvor_ui_v2/ui_view.c"
        "${TFT}/display_contract.c"
        "${TFT}/tft_presenter.c"
        "${TYVOR_ROOT}/shared/tyvor_protocol.c"
        "${TYVOR_ROOT}/shared/tyvor_link.c"
        "${TYVOR_ROOT}/shared/tyvor_ui.c"
        "${TYVOR_ROOT}/shared/tyvor_core_wire.c"
        "${TYVOR_ROOT}/shared/tyvor_ui_state.c"
        "${TYVOR_ROOT}/shared/tyvor_ui_endpoint.c"
        "${TYVOR_ROOT}/shared/tyvor_async_tx.c"
        "${TYVOR_ROOT}/shared/tyvor_bench_transport.c"
        "${TYVOR_ROOT}/shared/tyvor_diagnostics.c"
        ${LVGL_SOURCES}
    INCLUDE_DIRS
        "."
        "${UI321}"
        "${TFT}"
        "${TYVOR_ROOT}/shared"
        "${LVGL_ROOT}"
    REQUIRES
        esp_driver_uart
        esp_driver_gpio
        esp_driver_spi
        esp_adc
        esp_timer
        esp_psram
        heap
        esp_hw_support
        bootloader_support
        nvs_flash
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    TYVOR_BENCH_ONLY=1
    TYVOR_DIAGNOSTICS=1
    LV_CONF_INCLUDE_SIMPLE=1
    LV_KCONFIG_IGNORE=1
)
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -Wno-unused-parameter)
