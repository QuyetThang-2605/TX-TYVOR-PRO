#ifndef TYVOR_MODEL_STORE_H
#define TYVOR_MODEL_STORE_H
#include "tyvor_models.h"
#define TYVOR_MODEL_SLOTS 2u
typedef int (*tyvor_store_read_fn)(void *,unsigned,uint8_t *,size_t,size_t *);
typedef int (*tyvor_store_write_fn)(void *,unsigned,const uint8_t *,size_t);
/* Adapter must replace atomically or preserve the old slot on failure. */
typedef struct {void *context;tyvor_store_read_fn read;tyvor_store_write_fn write;} tyvor_model_storage_t;
typedef struct {tyvor_model_t active;tyvor_model_storage_t storage;uint8_t valid;} tyvor_model_manager_t;
typedef struct {
    uint8_t bytes[TYVOR_MODEL_SLOTS][TYVOR_MODEL_BYTES_MAX];
    size_t length[TYVOR_MODEL_SLOTS];uint8_t fail_read,fail_write;
} tyvor_mock_storage_t;
void tyvor_mock_storage_init(tyvor_mock_storage_t *);
tyvor_model_storage_t tyvor_mock_storage_adapter(tyvor_mock_storage_t *);
int tyvor_model_manager_init(tyvor_model_manager_t *,const tyvor_model_t *,tyvor_model_storage_t);
int tyvor_model_manager_save(const tyvor_model_manager_t *,unsigned);
int tyvor_model_manager_read(const tyvor_model_manager_t *,unsigned,tyvor_model_t *);
#endif
