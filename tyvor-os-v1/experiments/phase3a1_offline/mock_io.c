#include "offline.h"
#include <string.h>
void off_mock_input_init(off_mock_input_t *m) { if (m) memset(m,0,sizeof(*m)); }
int off_mock_input_set(off_mock_input_t *m,const tyvor_input_sample_t *s) {
    if (!m || !s) return OFF_INVALID;
    m->sample=*s;m->present=1;return OFF_OK;
}
static int read_mock(void *ctx,uint64_t now,uint64_t end,tyvor_input_sample_t *out) {
    off_mock_input_t *m=ctx;
    if (!m || !out) return OFF_INVALID;
    if (m->seen_clock && now<m->last_now) return OFF_CLOCK;
    m->seen_clock=1;m->last_now=now;
    if (now>=end || m->delay_ms>=end-now) return OFF_TIMEOUT;
    if (!m->present) return OFF_ABSENT;
    if (m->failure) return m->failure;
    *out=m->sample;return OFF_OK;
}
off_input_port_t off_mock_input_port(off_mock_input_t *m,uint32_t ttl) {
    return (off_input_port_t){m,read_mock,ttl};
}
int off_input_read(off_input_port_t *p,uint64_t now,uint64_t end,tyvor_input_sample_t *out) {
    if (!out) return OFF_INVALID;
    memset(out,0,sizeof(*out));
    if (!p || !p->read || !p->ttl_ms) return OFF_INVALID;
    if (now>=end) return OFF_TIMEOUT;
    tyvor_input_sample_t value={0};int r=p->read(p->context,now,end,&value);
    if (r) return r;
    if (!value.axes_valid || !value.switches_valid || value.switches[0]<0 || value.switches[0]>1 ||
        value.switches[1]<0 || value.switches[1]>2) return OFF_INVALID;
    for (unsigned i=0;i<TYVOR_AXES;++i) if (value.axes[i]>4095) return OFF_INVALID;
    if (value.sampled_ms>now || now-value.sampled_ms>=p->ttl_ms) return OFF_STALE;
    *out=value;return OFF_OK;
}
