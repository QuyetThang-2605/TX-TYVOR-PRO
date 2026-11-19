#ifndef TYVOR_STATUS_H
#define TYVOR_STATUS_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define TYVOR_STATUS_LEN 6u
#define TYVOR_VALID_BATTERY 0x01u
#define TYVOR_VALID_LQ 0x02u
#define TYVOR_SOURCE_MOCK 0x04u
typedef struct {
    uint8_t arm_valid, arm, lq, flags;
    uint16_t battery_mv;
} tyvor_status_t;
static inline int tyvor_status_valid(const tyvor_status_t *s) {
    if (!s || s->arm_valid > 1 || s->arm > 1 || (!s->arm_valid && s->arm)) return 0;
    if (s->flags & ~(TYVOR_VALID_BATTERY | TYVOR_VALID_LQ | TYVOR_SOURCE_MOCK)) return 0;
    if (s->flags & TYVOR_VALID_BATTERY) { if (!s->battery_mv) return 0; }
    else if (s->battery_mv) return 0;
    if (s->flags & TYVOR_VALID_LQ) { if (s->lq > 100) return 0; }
    else if (s->lq) return 0;
    if ((s->flags & TYVOR_SOURCE_MOCK) && (s->arm_valid || s->arm)) return 0;
    return 1;
}
static inline int tyvor_status_pack(const tyvor_status_t *s, uint8_t *b, size_t cap) {
    if (!b || cap < TYVOR_STATUS_LEN || !tyvor_status_valid(s)) return -1;
    b[0]=s->arm_valid; b[1]=s->arm; b[2]=(uint8_t)s->battery_mv;
    b[3]=(uint8_t)(s->battery_mv >> 8); b[4]=s->lq; b[5]=s->flags;
    return 0;
}
static inline int tyvor_status_unpack(tyvor_status_t *s, const uint8_t *b, size_t len) {
    tyvor_status_t candidate;
    if (!s || !b || len != TYVOR_STATUS_LEN) return -1;
    candidate.arm_valid=b[0]; candidate.arm=b[1];
    candidate.battery_mv=(uint16_t)((uint16_t)b[2] | ((uint16_t)b[3] << 8));
    candidate.lq=b[4]; candidate.flags=b[5];
    if (!tyvor_status_valid(&candidate)) return -1;
    *s=candidate;
    return 0;
}
typedef struct {
    tyvor_status_t status;
    uint64_t received_ms;
    uint8_t valid, received_once;
} tyvor_snapshot_t;
static inline void tyvor_snapshot_init(tyvor_snapshot_t *s) {
    if (s) memset(s, 0, sizeof(*s));
}
static inline int tyvor_snapshot_set(tyvor_snapshot_t *s, const tyvor_status_t *v, uint64_t now) {
    if (!s || !tyvor_status_valid(v)) return -1;
    s->status=*v; s->received_ms=now; s->valid=1; s->received_once=1;
    return 0;
}
static inline void tyvor_snapshot_invalidate(tyvor_snapshot_t *s) {
    if (s) { memset(&s->status, 0, sizeof(s->status)); s->valid=0; }
}
static inline int tyvor_snapshot_poll(tyvor_snapshot_t *s, uint64_t now, uint64_t stale_ms) {
    if (!s) return 0;
    if (s->valid && (now < s->received_ms || now-s->received_ms >= stale_ms))
        tyvor_snapshot_invalidate(s);
    return s->valid;
}
#endif
