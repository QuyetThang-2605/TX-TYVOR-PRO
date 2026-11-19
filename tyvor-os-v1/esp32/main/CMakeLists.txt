idf_component_register(SRCS "main.c" "../../shared/tyvor_protocol.c" "../../shared/tyvor_link.c" "../../shared/tyvor_ui.c"
    "../../shared/tyvor_core_wire.c" "../../shared/tyvor_ui_state.c" "../../shared/tyvor_ui_endpoint.c"
    "../../shared/tyvor_async_tx.c" "../../shared/tyvor_bench_transport.c" "../../shared/tyvor_diagnostics.c"
    INCLUDE_DIRS "." "../../shared"
    REQUIRES esp_driver_uart esp_driver_gpio esp_timer esp_psram heap esp_hw_support bootloader_support)
target_compile_definitions(${COMPONENT_LIB} PRIVATE TYVOR_BENCH_ONLY=1 TYVOR_DIAGNOSTICS=1)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage)
