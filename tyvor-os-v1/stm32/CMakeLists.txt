cmake_minimum_required(VERSION 3.20)
project(tyvor_stm32f407ve C ASM)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic" OR NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
  message(FATAL_ERROR "STM32 requires cmake/arm-none-eabi.cmake and a separate cross-build directory")
endif()
message(STATUS "STM32F407VE cross toolchain: ${CMAKE_TOOLCHAIN_FILE}")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.tools/sdk" CACHE PATH "Isolated, pinned CMSIS/HAL dependencies")
set(CMSIS "${SDK_ROOT}/CMSIS_5-5.9.0/CMSIS")
set(DEVICE "${SDK_ROOT}/cmsis-device-f4-2.6.10")
set(HAL "${SDK_ROOT}/stm32f4xx-hal-driver-1.8.3")
foreach(required "${CMSIS}/Core/Include/core_cm4.h" "${DEVICE}/Include/stm32f407xx.h" "${HAL}/Src/stm32f4xx_hal.c")
  if(NOT EXISTS "${required}")
    message(FATAL_ERROR "Missing pinned SDK: ${required}. See scripts/bootstrap-tools.ps1")
  endif()
endforeach()
option(TYVOR_UART2_PA2_PA3_VERIFIED "Enable USART2 PA2/PA3 ONLY after actual board schematic verification" ON)
option(TYVOR_ENABLE_IWDG "Enable bench watchdog (reset until loop health progresses)" ON)
set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
include(engine/sources.cmake)
add_executable(tyvor_stm32
  Core/Src/main.c Core/Src/tyvor_bridge.c BSP/board.c
  ${TYVOR_ENGINE_SOURCES} engine/tyvor_core_bridge.c
  ../shared/tyvor_protocol.c ../shared/tyvor_link.c ../shared/tyvor_core_wire.c ../shared/tyvor_bench_transport.c ../shared/tyvor_diagnostics.c ../shared/tyvor_async_tx.c ../shared/tyvor_timing.c
  "${DEVICE}/Source/Templates/gcc/startup_stm32f407xx.s"
  "${DEVICE}/Source/Templates/system_stm32f4xx.c"
  "${HAL}/Src/stm32f4xx_hal.c" "${HAL}/Src/stm32f4xx_hal_cortex.c"
  "${HAL}/Src/stm32f4xx_hal_rcc.c" "${HAL}/Src/stm32f4xx_hal_rcc_ex.c"
  "${HAL}/Src/stm32f4xx_hal_flash.c" "${HAL}/Src/stm32f4xx_hal_flash_ex.c"
  "${HAL}/Src/stm32f4xx_hal_gpio.c" "${HAL}/Src/stm32f4xx_hal_uart.c"
  "${HAL}/Src/stm32f4xx_hal_dma.c" "${HAL}/Src/stm32f4xx_hal_iwdg.c" "${HAL}/Src/stm32f4xx_hal_rng.c"
  "${HAL}/Src/stm32f4xx_hal_pwr.c" "${HAL}/Src/stm32f4xx_hal_pwr_ex.c")
target_include_directories(tyvor_stm32 PRIVATE Core/Inc BSP engine ../shared)
target_include_directories(tyvor_stm32 SYSTEM PRIVATE "${CMSIS}/Core/Include" "${DEVICE}/Include" "${HAL}/Inc")
target_compile_definitions(tyvor_stm32 PRIVATE STM32F407xx USE_HAL_DRIVER HSE_VALUE=0U TYVOR_BENCH_ONLY=1 TYVOR_DIAGNOSTICS=1
  TYVOR_UART_ENABLED=$<BOOL:${TYVOR_UART2_PA2_PA3_VERIFIED}>
  TYVOR_ENABLE_IWDG=$<BOOL:${TYVOR_ENABLE_IWDG}>)
target_compile_options(tyvor_stm32 PRIVATE ${CPU_FLAGS} -Os -g3 -ffunction-sections -fdata-sections -fstack-usage -Wall -Wextra)
target_link_options(tyvor_stm32 PRIVATE ${CPU_FLAGS} -nostartfiles --specs=nano.specs --specs=nosys.specs
  "-T${CMAKE_CURRENT_LIST_DIR}/BSP/STM32F407VET6.ld"
  -Wl,--gc-sections "-Wl,-Map=${CMAKE_BINARY_DIR}/tyvor_stm32.map" -Wl,--print-memory-usage)
set_target_properties(tyvor_stm32 PROPERTIES SUFFIX .elf LINK_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/BSP/STM32F407VET6.ld")
add_custom_command(TARGET tyvor_stm32 POST_BUILD
  COMMAND "${CMAKE_OBJCOPY}" -O binary $<TARGET_FILE:tyvor_stm32> "${CMAKE_BINARY_DIR}/tyvor_stm32.bin"
  COMMAND "${CMAKE_SIZE}" $<TARGET_FILE:tyvor_stm32>)
# Compile the gated UART branch for API/type checking without enabling it in ELF.
# This object target does not assert that any board wiring has been verified.
add_library(tyvor_bsp_uart_compile_check OBJECT BSP/board.c)
target_include_directories(tyvor_bsp_uart_compile_check PRIVATE Core/Inc BSP ../shared)
target_include_directories(tyvor_bsp_uart_compile_check SYSTEM PRIVATE "${CMSIS}/Core/Include" "${DEVICE}/Include" "${HAL}/Inc")
target_compile_definitions(tyvor_bsp_uart_compile_check PRIVATE STM32F407xx USE_HAL_DRIVER HSE_VALUE=0U TYVOR_UART_ENABLED=1 TYVOR_ENABLE_IWDG=1)
target_compile_options(tyvor_bsp_uart_compile_check PRIVATE ${CPU_FLAGS} -Os -Wall -Wextra -Werror)
