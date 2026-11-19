#include "tyvor_mixer.h"
#include <string.h>
int tyvor_mixer_run(const tyvor_model_t *m,const int16_t in[TYVOR_LOGICAL_INPUTS],int16_t out[TYVOR_CHANNELS]) {
    if (!out) return -1;
    memset(out,0,TYVOR_CHANNELS*sizeof(*out));
    if (!in || tyvor_model_validate(m)) return -1;
    for (unsigned i=0;i<TYVOR_LOGICAL_INPUTS;++i) if(in[i]<-1000 || in[i]>1000) return -1;
    int64_t bus[TYVOR_CHANNELS]={0};
    for (unsigned i=0;i<m->mix_count;++i) {
        const tyvor_mix_t *x=&m->mixes[i];
        int64_t value=((int64_t)in[x->source]*x->weight)/100+x->offset+x->trim;
        if (x->operation==TYVOR_MIX_REPLACE) bus[x->destination]=value;
        else bus[x->destination]+=value;
    }
    for (unsigned i=0;i<TYVOR_CHANNELS;++i) {
        int64_t v=bus[i];if(v<-1000) v=-1000;if(v>1000) v=1000;
        out[m->channel_map[i]]=(int16_t)v;
    }
    return 0;
}
