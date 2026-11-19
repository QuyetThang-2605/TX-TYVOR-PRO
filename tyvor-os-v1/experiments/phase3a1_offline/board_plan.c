#include "offline.h"
#include <string.h>
#ifndef TYVOR_OFFLINE_HSE12_PLAN
#define TYVOR_OFFLINE_HSE12_PLAN 0
#endif
void off_board_default(off_board_t *b) {
    if (!b) return;
    memset(b,0,sizeof(*b));
    for (unsigned i=0;i<OFF_PIN_COUNT;++i) b->gpio[i]=-1;
}
int off_board_validate(const off_board_t *b) {
    if (!b || b->hardware_gates) return OFF_INVALID;
    for (unsigned i=0;i<OFF_PIN_COUNT;++i) if (b->gpio[i]!=-1) return OFF_INVALID;
    return OFF_OK;
}
static int apb_div(unsigned d) { return d==1 || d==2 || d==4 || d==8 || d==16; }
static int ahb_div(unsigned d) { return d==1 || d==2 || d==4 || d==8 || d==16 || d==64 || d==128; }
int off_clock_calculate(const off_pll_t *p,off_clocks_t *out) {
    if (!out) return OFF_INVALID;
    memset(out,0,sizeof(*out));
    if (!p || p->source_hz<4000000 || p->source_hz>26000000 || p->m<2 || p->m>63 ||
        p->n<50 || p->n>432 || (p->p!=2 && p->p!=4 && p->p!=6 && p->p!=8) ||
        p->q<2 || p->q>15 || !ahb_div(p->ahb) || !apb_div(p->apb1) || !apb_div(p->apb2) ||
        (p->adc!=2 && p->adc!=4 && p->adc!=6 && p->adc!=8)) return OFF_INVALID;
    /* Keep exact rational arithmetic until divisibility is proven. */
    uint64_t src=p->source_hz, vco=src*p->n;
    if (src<(uint64_t)p->m*1000000 || src>(uint64_t)p->m*2000000 ||
        vco<(uint64_t)p->m*100000000 || vco>(uint64_t)p->m*432000000 ||
        vco%((uint64_t)p->m*p->p) || vco%((uint64_t)p->m*p->q)) return OFF_INVALID;
    uint64_t sys=vco/(p->m*p->p), usb=vco/(p->m*p->q);
    if (sys>168000000 || usb!=48000000 || sys%p->ahb) return OFF_INVALID;
    uint32_t h=(uint32_t)(sys/p->ahb);
    if (h%p->apb1 || h%p->apb2) return OFF_INVALID;
    uint32_t a=h/p->apb1,b=h/p->apb2;
    if (a>42000000 || b>84000000 || b%p->adc || b/p->adc>36000000) return OFF_INVALID;
    *out=(off_clocks_t){(uint32_t)sys,h,a,b,a*(p->apb1==1?1u:2u),b*(p->apb2==1?1u:2u),b/p->adc,(uint32_t)usb};
    return OFF_OK;
}
int off_clock_profile(unsigned profile,off_clocks_t *out) {
    if (!out) return OFF_INVALID;
    memset(out,0,sizeof(*out));
    if (profile==0) {
        *out=(off_clocks_t){16000000,16000000,16000000,16000000,16000000,16000000,8000000,48000000};
        /* ADC /2 is a planning value, not an initialized ADC peripheral. */
        return OFF_OK;
    }
    if (profile!=1) return OFF_INVALID;
#if TYVOR_OFFLINE_HSE12_PLAN
    const off_pll_t p={12000000,12,336,2,7,1,4,2,4};
    return off_clock_calculate(&p,out);
#else
    return OFF_DISABLED;
#endif
}
