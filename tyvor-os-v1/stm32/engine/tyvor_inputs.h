#ifndef TYVOR_INPUTS_H
#define TYVOR_INPUTS_H
#include "tyvor_models.h"
typedef struct {
    uint16_t axes[TYVOR_AXES];int8_t switches[TYVOR_SWITCHES];
    uint8_t axes_valid,switches_valid;uint32_t sequence;uint64_t sampled_ms;
} tyvor_input_sample_t;
typedef struct {
    int16_t immediate[TYVOR_AXES],filtered[TYVOR_AXES];
    int8_t switches[TYVOR_SWITCHES];
    uint64_t sampled_ms;uint32_t sequence,rejected;
    uint8_t valid,seen,filter_initialized;
} tyvor_inputs_t;
void tyvor_inputs_init(tyvor_inputs_t *);
int tyvor_inputs_feed(tyvor_inputs_t *,const tyvor_model_t *,const tyvor_input_sample_t *,uint64_t now);
int tyvor_inputs_poll(tyvor_inputs_t *,const tyvor_model_t *,uint64_t now);
void tyvor_inputs_logical(const tyvor_inputs_t *,int16_t out[TYVOR_LOGICAL_INPUTS]);
#endif
