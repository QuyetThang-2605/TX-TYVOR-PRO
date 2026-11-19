cmake_minimum_required(VERSION 3.20)
project(tyvor_discovery_bench_v1 C ASM)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic" OR NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
  message(FATAL_ERROR "Discovery requires the ARM cross toolchain and a separate build directory")
endif()
option(TYVOR_DISCOVERY_UART_ENABLED "Unimplemented Discovery UART hardware gate" OFF)
if(TYVOR_DISCOVERY_UART_ENABLED)
  message(FATAL_ERROR "Discovery UART remains OFF: this target has no UART pin assignment")
endif()
get_filename_component(TYVOR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(SDK_ROOT "${TYVOR_ROOT}/.tools/sdk")
set(CMSIS "${SDK_ROOT}/CMSIS_5-5.9.0/CMSIS")
set(DEVICE "${SDK_ROOT}/cmsis-device-f4-2.6.10")
set(HAL "${SDK_ROOT}/stm32f4xx-hal-driver-1.8.3")
foreach(required "${CMSIS}/Core/Include/core_cm4.h" "${DEVICE}/Include/stm32f407xx.h" "${HAL}/Src/stm32f4xx_hal.c")
  if(NOT EXISTS "${required}")
    message(FATAL_ERROR "Missing pinned SDK: ${required}")
  endif()
endforeach()
set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
include("${TYVOR_ROOT}/stm32/engine/sources.cmake")
add_executable(tyvor_discovery_bench
  main.c discovery_board.c discovery_demo.c
  ${TYVOR_ENGINE_SOURCES} "${TYVOR_ROOT}/shared/tyvor_timing.c"
  "${DEVICE}/Source/Templates/gcc/startup_stm32f407xx.s"
  "${DEVICE}/Source/Templates/system_stm32f4xx.c"
  "${HAL}/Src/stm32f4xx_hal.c" "${HAL}/Src/stm32f4xx_hal_cortex.c"
  "${HAL}/Src/stm32f4xx_hal_rcc.c" "${HAL}/Src/stm32f4xx_hal_rcc_ex.c"
  "${HAL}/Src/stm32f4xx_hal_flash.c" "${HAL}/Src/stm32f4xx_hal_flash_ex.c"
  "${HAL}/Src/stm32f4xx_hal_gpio.c" "${HAL}/Src/stm32f4xx_hal_iwdg.c"
  "${HAL}/Src/stm32f4xx_hal_pwr.c" "${HAL}/Src/stm32f4xx_hal_pwr_ex.c")
target_include_directories(tyvor_discovery_bench PRIVATE . "${TYVOR_ROOT}/stm32/engine" "${TYVOR_ROOT}/shared")
target_include_directories(tyvor_discovery_bench SYSTEM PRIVATE "${CMSIS}/Core/Include" "${DEVICE}/Include" "${HAL}/Inc")
target_compile_definitions(tyvor_discovery_bench PRIVATE STM32F407xx USE_HAL_DRIVER HSE_VALUE=8000000U
  TYVOR_BENCH_ONLY=1 TYVOR_ENABLE_RF=0 TYVOR_DISCOVERY_MB997E=1 TYVOR_DISCOVERY_UART_ENABLED=0)
target_compile_options(tyvor_discovery_bench PRIVATE ${CPU_FLAGS} -Os -g3 -ffunction-sections -fdata-sections -fstack-usage -Wall -Wextra)
target_link_options(tyvor_discovery_bench PRIVATE ${CPU_FLAGS} -nostartfiles --specs=nano.specs --specs=nosys.specs
  "-T${CMAKE_CURRENT_LIST_DIR}/STM32F407VGT6.ld" -Wl,--gc-sections
  "-Wl,-Map=${CMAKE_BINARY_DIR}/tyvor_discovery_bench.map" -Wl,--print-memory-usage)
set_target_properties(tyvor_discovery_bench PROPERTIES SUFFIX .elf LINK_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/STM32F407VGT6.ld")
add_custom_command(TARGET tyvor_discovery_bench POST_BUILD
  COMMAND "${CMAKE_OBJCOPY}" -O binary $<TARGET_FILE:tyvor_discovery_bench> "${CMAKE_BINARY_DIR}/tyvor_discovery_bench.bin"
  COMMAND "${CMAKE_SIZE}" $<TARGET_FILE:tyvor_discovery_bench>)
