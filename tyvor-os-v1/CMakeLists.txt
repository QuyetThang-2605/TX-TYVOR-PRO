cmake_minimum_required(VERSION 3.16)
project(tyvor_os_v1 C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

add_library(tyvor_protocol shared/tyvor_protocol.c shared/tyvor_link.c shared/tyvor_core_wire.c shared/tyvor_bench_transport.c shared/tyvor_diagnostics.c shared/tyvor_async_tx.c shared/tyvor_timing.c)
target_include_directories(tyvor_protocol PUBLIC shared)
add_library(tyvor_stm32_bridge stm32/Core/Src/tyvor_bridge.c)
target_include_directories(tyvor_stm32_bridge PUBLIC stm32/Core/Inc)
target_link_libraries(tyvor_stm32_bridge PUBLIC tyvor_protocol)
add_library(tyvor_ui shared/tyvor_ui.c)
target_link_libraries(tyvor_ui PUBLIC tyvor_protocol)
include(stm32/engine/sources.cmake)
add_library(tyvor_core ${TYVOR_ENGINE_SOURCES})
target_include_directories(tyvor_core PUBLIC stm32/engine shared)
target_compile_definitions(tyvor_core PUBLIC TYVOR_BENCH_ONLY=1)
add_library(tyvor_core_bridge stm32/engine/tyvor_core_bridge.c)
target_link_libraries(tyvor_core_bridge PUBLIC tyvor_core tyvor_stm32_bridge)
add_library(tyvor_ui_endpoint shared/tyvor_ui_state.c shared/tyvor_ui_endpoint.c)
target_link_libraries(tyvor_ui_endpoint PUBLIC tyvor_ui)
add_executable(tyvor_ui_sim sim/tyvor_ui_sim.c)
target_link_libraries(tyvor_ui_sim PRIVATE tyvor_ui)
add_executable(test_protocol sim/test_protocol.c)
target_link_libraries(test_protocol PRIVATE tyvor_stm32_bridge tyvor_ui)
enable_testing()
set(TYVOR_TEST_CASES
  roundtrip golden_crc crc_error truncated header_error payload_oversize null_buffers
  output_capacity payload_types semantic_status parser_timeout uart_noise payload_header
  nested_recovery false_length consecutive truncated_recovery absolute_deadline sequence_half_range split_every_boundary duplicate_sequence
  sequence_wrap sequence_abnormal independent_sequences stale_snapshot clock_regression
  transport_overflow max_and_empty crc_all_bits bounded_noise_recovery bridge_tx bridge_requests ui_states)
foreach(case IN LISTS TYVOR_TEST_CASES)
  add_test(NAME ${case} COMMAND test_protocol ${case})
endforeach()
add_executable(test_reliability sim/test_reliability.c)
target_link_libraries(test_reliability PRIVATE tyvor_protocol)
set(TYVOR_RELIABILITY_CASES
  h1_timeout_suffix h2_sender_reboot timeout_every_split recovered_arrival_timestamp
  receiver_reboot both_reboot duplicate_hello_ack lost_hello lost_session_ack retry_budget
  no_session_no_data delayed_old_packets sequence_does_not_change_session loss_window_rehandshake
  identity_collision candidate_collision late_ack stale_after_session_change
  protocol_version_compatibility tag_crc_and_schema direction_authority
  invalid_identity_epoch_limit link_clock_regression randomized_uart_seed randomized_reboots_seed
  reboot_during_idle_probe control_tx_failure_recovery wrong_ack_epoch retired_epoch_hello ack_before_offer
  status_stale_with_ack_traffic)
foreach(case IN LISTS TYVOR_RELIABILITY_CASES)
  add_test(NAME ${case} COMMAND test_reliability ${case})
endforeach()
add_library(tyvor_sim_runtime sim/tyvor_sim_runtime.c sim/tyvor_bench_wire.c)
target_include_directories(tyvor_sim_runtime PUBLIC sim)
target_link_libraries(tyvor_sim_runtime PUBLIC tyvor_core_bridge tyvor_ui_endpoint)
add_executable(tyvor_core_sim sim/tyvor_core_sim.c)
target_link_libraries(tyvor_core_sim PRIVATE tyvor_sim_runtime)
add_executable(test_core sim/test_core.c)
target_link_libraries(test_core PRIVATE tyvor_sim_runtime)
set(TYVOR_CORE_CASES
  input_calibration input_deadband_filter input_invalid input_timeout input_sequence
  mixer_weights mixer_ordering mixer_routing mixer_clamp_overflow
  model_roundtrip model_crc model_validation model_version model_storage model_atomic_failure model_switch_guard
  safety_high_throttle safety_startup_switch safety_hold_switch safety_input_loss safety_recovery safety_kill safety_reset safety_filter_bypass
  scheduler_deterministic scheduler_overrun scheduler_clock
  telemetry_values telemetry_stale telemetry_invalid
  core_wire_roundtrip core_wire_rejection
  ui_atomic_snapshot ui_mixed_generation ui_duplicate_parts ui_stale_no_revival ui_partial_timeout ui_source_age
  ui_session_recovery ui_reset_independence ui_authority ui_read_ack ui_renderer ui_telemetry_recovery safety_invalid_model safety_reset_corrupt_storage
  core_no_rf core_randomized_models core_safety_randomized)
foreach(case IN LISTS TYVOR_CORE_CASES)
  add_test(NAME ${case} COMMAND test_core ${case})
endforeach()
set(TYVOR_SCENARIO_CASES
  scenario_safe_startup scenario_throttle_interlock scenario_input_loss
  scenario_model_mixer scenario_reset_recovery scenario_telemetry_expiry scenario_ui_independence)
foreach(case IN LISTS TYVOR_SCENARIO_CASES)
  add_test(NAME ${case} COMMAND tyvor_core_sim --script ${CMAKE_CURRENT_SOURCE_DIR}/sim/scenarios/${case}.txt)
endforeach()
add_executable(test_bench_regressions sim/test_bench_regressions.c)
target_link_libraries(test_bench_regressions PRIVATE tyvor_sim_runtime)
set(TYVOR_BENCH_REGRESSION_CASES transient_input_loss delayed_telemetry expired_input_outputs unsafe_storage_read)
foreach(case IN LISTS TYVOR_BENCH_REGRESSION_CASES)
  add_test(NAME ${case} COMMAND test_bench_regressions ${case})
endforeach()
add_executable(test_bench sim/test_bench.c)
target_link_libraries(test_bench PRIVATE tyvor_sim_runtime)
set(TYVOR_BENCH_CASES
  bench_bidirectional bench_loss bench_delay bench_corruption bench_duplication bench_reconnect
  bench_reset_stm32 bench_reset_esp32 bench_reset_both bench_session_collision bench_stale_cache bench_long_delay_rejected bench_old_packets
  bench_partial_tx bench_interbyte_timeout bench_buffer_bound bench_disabled_gate bench_lease_exhaustion bench_read_replay bench_read_before_offer
  bench_all_mixer_terms bench_view_states bench_diagnostics bench_fault_seed bench_delay_within_lease bench_telemetry_age_bound)
foreach(case IN LISTS TYVOR_BENCH_CASES)
  add_test(NAME ${case} COMMAND test_bench ${case})
endforeach()
add_executable(tyvor_bench_ui_export sim/tyvor_bench_ui_export.c)
target_link_libraries(tyvor_bench_ui_export PRIVATE tyvor_sim_runtime)
set(TYVOR_BENCH_SCENARIOS bench_fault_reconnect bench_reset_matrix bench_aged_packet bench_seven_pages)
foreach(case IN LISTS TYVOR_BENCH_SCENARIOS)
  add_test(NAME ${case} COMMAND tyvor_core_sim --script ${CMAKE_CURRENT_SOURCE_DIR}/sim/bench_scenarios/${case}.txt)
endforeach()

add_executable(test_timing sim/test_timing.c)
target_link_libraries(test_timing PRIVATE tyvor_sim_runtime)
set(TYVOR_TIMING_CASES tx_copy tx_full tx_coalesce tx_active_immutable tx_expiry tx_timeout tx_driver_errors tx_clock tx_validation tx_order tx_seed tx_deadline_before_driver
  timing_wrap timing_disabled timing_budget timing_lateness async_duplex async_reset_control async_reset_ui async_reset_both async_tx_saturation async_rx_saturation async_slow_ui)
foreach(case IN LISTS TYVOR_TIMING_CASES)
  add_test(NAME ${case} COMMAND test_timing ${case})
endforeach()

# Isolated Discovery tests; legacy Phase2C build/test inventory remains 177 by default.
option(TYVOR_BUILD_DISCOVERY_TESTS "Build the Discovery software bench demo and tests" OFF)
if(TYVOR_BUILD_DISCOVERY_TESTS)
  add_library(tyvor_discovery_demo stm32/boards/stm32f4_discovery/discovery_demo.c)
  target_include_directories(tyvor_discovery_demo PUBLIC stm32/boards/stm32f4_discovery)
  target_link_libraries(tyvor_discovery_demo PUBLIC tyvor_core)
  add_executable(test_discovery sim/test_discovery.c)
  target_link_libraries(test_discovery PRIVATE tyvor_discovery_demo)
  set(TYVOR_DISCOVERY_CASES discovery_boot_safe discovery_selftest discovery_button_debounce discovery_button_held_boot
    discovery_button_long_hold discovery_button_release discovery_mode_cycle discovery_mode_safe_reset
    discovery_mixer_sweep discovery_input_loss discovery_loss_recovery discovery_model_selection discovery_model_fault discovery_telemetry_expiry
    discovery_clock_fault discovery_scheduler_fault discovery_fault_button discovery_leds discovery_reset discovery_no_rf discovery_deterministic)
  foreach(case IN LISTS TYVOR_DISCOVERY_CASES)
    add_test(NAME ${case} COMMAND test_discovery ${case})
  endforeach()
endif()

# Separate TFT/LVGL experiment, optional so all existing default inventories stay unchanged.
option(TYVOR_BUILD_TFT_EXPERIMENT "Build isolated TFT SPI/LVGL prototype and software tests" OFF)
if(TYVOR_BUILD_TFT_EXPERIMENT)
  add_subdirectory(experiments/tft_spi_320x240)
endif()

option(TYVOR_BUILD_UI_V2 "Build isolated TYVOR Tactical Dark LVGL UI and PC simulator" OFF)
if(TYVOR_BUILD_UI_V2)
  if(NOT TYVOR_BUILD_TFT_EXPERIMENT)
    message(FATAL_ERROR "UI V2 reuses the TFT display/presenter library; enable TYVOR_BUILD_TFT_EXPERIMENT")
  endif()
  add_subdirectory(experiments/tyvor_ui_v2)
endif()

option(TYVOR_BUILD_UI_V3 "Build landscape reference-led TYVOR LVGL UI and PC simulator" OFF)
if(TYVOR_BUILD_UI_V3)
  if(NOT TYVOR_BUILD_UI_V2)
    message(FATAL_ERROR "UI V3 regression build includes UI V2; enable TYVOR_BUILD_UI_V2")
  endif()
  add_subdirectory(experiments/tyvor_ui_v3)
endif()

option(TYVOR_BUILD_UI_V31 "Build isolated HOME visual refinement" OFF)
if(TYVOR_BUILD_UI_V31)
  if(NOT TYVOR_BUILD_UI_V3)
    message(FATAL_ERROR "V3.1 regression build requires TYVOR_BUILD_UI_V3")
  endif()
  add_subdirectory(experiments/tyvor_ui_v3_1)
endif()

option(TYVOR_BUILD_UI_V32 "Build approved blue-wave HOME and flat component experiment" OFF)
if(TYVOR_BUILD_UI_V32)
  if(NOT TYVOR_BUILD_UI_V31)
    message(FATAL_ERROR "V3.2 regression build requires TYVOR_BUILD_UI_V31")
  endif()
  add_subdirectory(experiments/tyvor_ui_v3_2)
endif()

option(TYVOR_BUILD_UI_V321 "Build isolated V3.2.1 visual polish and pixel capture" OFF)
if(TYVOR_BUILD_UI_V321)
  if(NOT TYVOR_BUILD_UI_V32)
    message(FATAL_ERROR "V3.2.1 regression build requires TYVOR_BUILD_UI_V32")
  endif()
  add_subdirectory(experiments/tyvor_ui_v3_2_1)
endif()
