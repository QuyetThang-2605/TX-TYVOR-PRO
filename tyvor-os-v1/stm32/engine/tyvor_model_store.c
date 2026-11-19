#include "tyvor_model_store.h"
#include <string.h>
static int mock_read(void *ctx,unsigned slot,uint8_t *b,size_t cap,size_t *n) {
    tyvor_mock_storage_t *s=ctx;
    if (!s || slot>=TYVOR_MODEL_SLOTS || !b || !n || s->fail_read || !s->length[slot] || cap<s->length[slot]) return -1;
    memcpy(b,s->bytes[slot],s->length[slot]);*n=s->length[slot];return 0;
}
static int mock_write(void *ctx,unsigned slot,const uint8_t *b,size_t n) {
    tyvor_mock_storage_t *s=ctx;
    if (!s || slot>=TYVOR_MODEL_SLOTS || !b || !n || n>TYVOR_MODEL_BYTES_MAX || s->fail_write) return -1;
    memcpy(s->bytes[slot],b,n);s->length[slot]=n;return 0;
}
void tyvor_mock_storage_init(tyvor_mock_storage_t *s) {if(s) memset(s,0,sizeof(*s));}
tyvor_model_storage_t tyvor_mock_storage_adapter(tyvor_mock_storage_t *s) {
    return (tyvor_model_storage_t){s,mock_read,mock_write};
}
int tyvor_model_manager_init(tyvor_model_manager_t *m,const tyvor_model_t *v,tyvor_model_storage_t s) {
    if (!m || tyvor_model_validate(v)) return -1;
    memset(m,0,sizeof(*m));m->active=*v;m->storage=s;m->valid=1;return 0;
}
int tyvor_model_manager_save(const tyvor_model_manager_t *m,unsigned slot) {
    uint8_t b[TYVOR_MODEL_BYTES_MAX];
    if (!m || !m->valid || !m->storage.write || slot>=TYVOR_MODEL_SLOTS) return -1;
    size_t n=tyvor_model_serialize(&m->active,b,sizeof(b));
    return n?m->storage.write(m->storage.context,slot,b,n):-1;
}
int tyvor_model_manager_read(const tyvor_model_manager_t *m,unsigned slot,tyvor_model_t *v) {
    uint8_t b[TYVOR_MODEL_BYTES_MAX];size_t n=0;
    if (!m || !v || !m->storage.read || slot>=TYVOR_MODEL_SLOTS ||
        m->storage.read(m->storage.context,slot,b,sizeof(b),&n) || n>sizeof(b)) return -1;
    return tyvor_model_deserialize(v,b,n);
}
