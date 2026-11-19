#ifndef TYVOR_SAFETY_H
#define TYVOR_SAFETY_H
#include "tyvor_inputs.h"
typedef struct {uint8_t state,throttle_lock,off_seen,previous_arm;uint16_t reasons;} tyvor_safety_t;
void tyvor_safety_init(tyvor_safety_t *);
void tyvor_safety_step(tyvor_safety_t *,const tyvor_inputs_t *,const tyvor_model_t *,uint16_t faults);
void tyvor_safety_apply(const tyvor_safety_t *,const tyvor_model_t *,const int16_t desired[TYVOR_CHANNELS],
                         int16_t out[TYVOR_CHANNELS]);
#endif
