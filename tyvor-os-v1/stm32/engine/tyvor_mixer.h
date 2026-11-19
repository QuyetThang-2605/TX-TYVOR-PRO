#ifndef TYVOR_MIXER_H
#define TYVOR_MIXER_H
#include "tyvor_models.h"
/* Fixed array order, int64 accumulation, clamp only after the final term. */
int tyvor_mixer_run(const tyvor_model_t *,const int16_t inputs[TYVOR_LOGICAL_INPUTS],
                    int16_t channels[TYVOR_CHANNELS]);
#endif
