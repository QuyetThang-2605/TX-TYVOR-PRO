cmake_minimum_required(VERSION 3.20)
project(tyvor_discovery_tft C ASM)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic" OR NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
  message(FATAL_ERROR "Discovery TFT requires the ARM cross toolchain and a separate build directory")
endif()
get_filename_component(TYVOR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(SDK_ROOT "${TYVOR_ROOT}/.tools/sdk")
set(CMSIS "${SDK_ROOT}/CMSIS_5-5.9.0/CMSIS")
set(DEVICE "${SDK_ROOT}/cmsis-device-f4-2.6.10")
set(HAL "${SDK_ROOT}/stm32f4xx-hal-driver-1.8.3")
set(LVGL_ROOT "${SDK_ROOT}/lvgl-9.2.2")
set(UI321 "${TYVOR_ROOT}/experiments/tyvor_ui_v3_2_1")
set(TFT "${TYVOR_ROOT}/experiments/tft_spi_320x240")
file(GLOB_RECURSE LVGL_SOURCES "${LVGL_ROOT}/src/*.c")
set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
include("${TYVOR_ROOT}/stm32/engine/sources.cmake")

add_executable(tyvor_discovery_tft
  main.c
  "${TYVOR_ROOT}/stm32/boards/stm32f4_discovery/discovery_demo.c"
  "${TYVOR_ROOT}/stm32/Core/Src/tyvor_bridge.c"
  "${TYVOR_ROOT}/stm32/engine/tyvor_core_bridge.c"
  ${TYVOR_ENGINE_SOURCES}
  "${UI321}/ui.c" "${UI321}/ui_view.c" "${UI321}/ui_theme.c"
  "${UI321}/ui_widgets.c" "${UI321}/ui_pages.c" "${UI321}/ui_shapes.c"
  "${UI321}/timer_font.c" "${UI321}/branding.c" "${UI321}/brand_assets.c"
  "${UI321}/render_quality.c" "${UI321}/icons.c" "${UI321}/icon_data.c"
  "${TYVOR_ROOT}/experiments/tyvor_ui_v2/ui_view.c"
  "${TFT}/display_contract.c" "${TFT}/tft_presenter.c"
  "${TYVOR_ROOT}/shared/tyvor_protocol.c" "${TYVOR_ROOT}/shared/tyvor_link.c"
  "${TYVOR_ROOT}/shared/tyvor_ui.c" "${TYVOR_ROOT}/shared/tyvor_core_wire.c"
  "${TYVOR_ROOT}/shared/tyvor_ui_state.c" "${TYVOR_ROOT}/shared/tyvor_ui_endpoint.c"
  "${TYVOR_ROOT}/shared/tyvor_async_tx.c" "${TYVOR_ROOT}/shared/tyvor_bench_transport.c"
  "${TYVOR_ROOT}/shared/tyvor_diagnostics.c" "${TYVOR_ROOT}/shared/tyvor_timing.c"
  ${LVGL_SOURCES}
  "${DEVICE}/Source/Templates/gcc/startup_stm32f407xx.s"
  "${DEVICE}/Source/Templates/system_stm32f4xx.c"
  "${HAL}/Src/stm32f4xx_hal.c" "${HAL}/Src/stm32f4xx_hal_adc.c" "${HAL}/Src/stm32f4xx_hal_cortex.c"
  "${HAL}/Src/stm32f4xx_hal_rcc.c" "${HAL}/Src/stm32f4xx_hal_rcc_ex.c"
  "${HAL}/Src/stm32f4xx_hal_flash.c" "${HAL}/Src/stm32f4xx_hal_flash_ex.c"
  "${HAL}/Src/stm32f4xx_hal_gpio.c" "${HAL}/Src/stm32f4xx_hal_iwdg.c"
  "${HAL}/Src/stm32f4xx_hal_pwr.c" "${HAL}/Src/stm32f4xx_hal_pwr_ex.c"
  "${HAL}/Src/stm32f4xx_hal_dma.c" "${HAL}/Src/stm32f4xx_hal_spi.c"
  "${HAL}/Src/stm32f4xx_hal_uart.c" "${HAL}/Src/stm32f4xx_hal_rng.c")

target_include_directories(tyvor_discovery_tft PRIVATE
  .
  "${TYVOR_ROOT}/stm32/boards/stm32f4_discovery"
  "${TYVOR_ROOT}/stm32/Core/Inc"
  "${TYVOR_ROOT}/stm32/engine"
  "${UI321}"
  "${TFT}"
  "${TYVOR_ROOT}/shared")

target_include_directories(tyvor_discovery_tft SYSTEM PRIVATE
  "${LVGL_ROOT}"
  "${CMSIS}/Core/Include"
  "${DEVICE}/Include"
  "${HAL}/Inc")

target_compile_definitions(tyvor_discovery_tft PRIVATE
  STM32F407xx USE_HAL_DRIVER HSE_VALUE=8000000U
  TYVOR_BENCH_ONLY=1 TYVOR_ENABLE_RF=0 TYVOR_DIAGNOSTICS=1
  LV_CONF_INCLUDE_SIMPLE=1 LV_KCONFIG_IGNORE=1)

target_compile_options(tyvor_discovery_tft PRIVATE
  ${CPU_FLAGS} -O2 -g3 -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unused-parameter)

target_link_options(tyvor_discovery_tft PRIVATE
  ${CPU_FLAGS} -nostartfiles --specs=nano.specs --specs=nosys.specs
  "-T${CMAKE_CURRENT_LIST_DIR}/STM32F407VGT6.ld" -Wl,--gc-sections
  "-Wl,-Map=${CMAKE_BINARY_DIR}/tyvor_discovery_tft.map" -Wl,--print-memory-usage)

set_target_properties(tyvor_discovery_tft PROPERTIES SUFFIX .elf LINK_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/STM32F407VGT6.ld")
add_custom_command(TARGET tyvor_discovery_tft POST_BUILD
  COMMAND "${CMAKE_OBJCOPY}" -O binary $<TARGET_FILE:tyvor_discovery_tft> "${CMAKE_BINARY_DIR}/tyvor_discovery_tft.bin"
  COMMAND "${CMAKE_SIZE}" $<TARGET_FILE:tyvor_discovery_tft>)
