# Portable STM32-owned bench services; no HAL/RTOS/RF dependencies.
set(TYVOR_ENGINE_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/tyvor_models.c
  ${CMAKE_CURRENT_LIST_DIR}/tyvor_model_store.c
  ${CMAKE_CURRENT_LIST_DIR}/tyvor_inputs.c
  ${CMAKE_CURRENT_LIST_DIR}/tyvor_mixer.c
  ${CMAKE_CURRENT_LIST_DIR}/tyvor_safety.c
  ${CMAKE_CURRENT_LIST_DIR}/tyvor_telemetry.c
  ${CMAKE_CURRENT_LIST_DIR}/tyvor_bench.c
  ${CMAKE_CURRENT_LIST_DIR}/tyvor_crsf.c
  ${CMAKE_CURRENT_LIST_DIR}/tyvor_core.c)
