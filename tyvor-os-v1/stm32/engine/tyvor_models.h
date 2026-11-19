#ifndef TYVOR_MODELS_H
#define TYVOR_MODELS_H
#include <stddef.h>
#include "tyvor_core_schema.h"
#define TYVOR_MODEL_SCHEMA 1u
#define TYVOR_MIXES_MAX 16u
#define TYVOR_MODEL_BYTES_MAX 256u
enum {TYVOR_MIX_ADD, TYVOR_MIX_REPLACE};
typedef struct {uint16_t minimum,center,maximum,deadband;uint8_t filter_divisor;} tyvor_axis_config_t;
typedef struct {uint8_t source,destination,operation;int16_t weight,offset,trim;} tyvor_mix_t;
typedef struct {
    uint32_t id,revision;
    char name[TYVOR_MODEL_NAME_CAP];
    tyvor_axis_config_t axes[TYVOR_AXES];
    uint16_t input_timeout_ms;
    uint8_t switch_positions[TYVOR_SWITCHES],throttle_axis,mix_count;
    tyvor_mix_t mixes[TYVOR_MIXES_MAX]; /* array order is execution order */
    uint8_t channel_map[TYVOR_CHANNELS]; /* logical bus -> monitor channel */
    int16_t failsafe[TYVOR_CHANNELS]; /* physical monitor channel order */
} tyvor_model_t;
void tyvor_model_default(tyvor_model_t *model,uint32_t id);
int tyvor_model_validate(const tyvor_model_t *model);
uint32_t tyvor_model_crc32(const uint8_t *bytes,size_t size);
size_t tyvor_model_serialize(const tyvor_model_t *,uint8_t *,size_t);
int tyvor_model_deserialize(tyvor_model_t *,const uint8_t *,size_t);
#endif
