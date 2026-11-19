#ifndef TYVOR_SIM_RUNTIME_H
#define TYVOR_SIM_RUNTIME_H
#include "tyvor_core_bridge.h"
#include "tyvor_bench.h"
#include "tyvor_ui_endpoint.h"
#include "tyvor_bench_wire.h"
#include "tyvor_bench_transport.h"
typedef struct {
    tyvor_core_t core;tyvor_core_bridge_t bridge;tyvor_ui_endpoint_t ui;
    tyvor_mock_storage_t storage;tyvor_sim_wire_t control_to_ui,ui_to_control;
    tyvor_bench_transport_t control_adapter,ui_adapter;
    int16_t axes[4];int8_t switches[2];tyvor_telemetry_sample_t telemetry;
    uint64_t now,next_input,next_telemetry,control_boot,ui_boot;uint32_t input_sequence;
    uint8_t input_loss,telemetry_loss;
    uint32_t control_resets,ui_resets;
} tyvor_sim_t;
int tyvor_sim_init(tyvor_sim_t *);
int tyvor_sim_advance(tyvor_sim_t *,uint32_t ms);
int tyvor_sim_reset_control(tyvor_sim_t *);
int tyvor_sim_reset_ui(tyvor_sim_t *);
int tyvor_sim_model(tyvor_sim_t *,unsigned slot);
#endif
