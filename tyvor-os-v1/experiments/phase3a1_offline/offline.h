#ifndef TYVOR_PHASE3A1_OFFLINE_H
#define TYVOR_PHASE3A1_OFFLINE_H
#include <stddef.h>
#include <stdint.h>
#include "tyvor_inputs.h"
#include "tyvor_model_store.h"
/* Host contracts only. No HAL, GPIO, filesystem, DAC or RF implementation. */
#if (defined(TYVOR_OFFLINE_HARDWARE_GATE) && TYVOR_OFFLINE_HARDWARE_GATE) || \
    (defined(TYVOR_ENABLE_RF) && TYVOR_ENABLE_RF)
#error Phase3A1_requires_all_hardware_gates_OFF
#endif
enum { OFF_OK=0, OFF_DISABLED=1, OFF_INVALID=-1, OFF_TIMEOUT=-2,
       OFF_IO=-3, OFF_ABSENT=-4, OFF_READONLY=-5, OFF_STALE=-6,
       OFF_CLOCK=-7, OFF_FULL=-8 };
enum { OFF_PIN_COUNT=24, OFF_MEDIA_BLOCK=512, OFF_MEDIA_SECTORS=8, OFF_AUDIO_QUEUE=8 };
typedef struct { int16_t gpio[OFF_PIN_COUNT]; uint32_t hardware_gates; } off_board_t;
void off_board_default(off_board_t *);
int off_board_validate(const off_board_t *);
typedef struct {
    uint32_t source_hz; uint16_t m,n; uint8_t p,q,ahb,apb1,apb2,adc;
} off_pll_t;
typedef struct {
    uint32_t sysclk,hclk,pclk1,pclk2,timer1,timer2,adc,pll48;
} off_clocks_t;
int off_clock_calculate(const off_pll_t *,off_clocks_t *);
/* Profile0 reflects current HSI SYSCLK; profile1 is proposed HSE12 PLL168. */
int off_clock_profile(unsigned profile,off_clocks_t *);
typedef int (*off_input_read_fn)(void *,uint64_t,uint64_t,tyvor_input_sample_t *);
typedef struct { void *context; off_input_read_fn read; uint32_t ttl_ms; } off_input_port_t;
typedef struct {
    tyvor_input_sample_t sample; uint64_t last_now; uint32_t delay_ms;
    int failure; uint8_t present,seen_clock;
} off_mock_input_t;
void off_mock_input_init(off_mock_input_t *);
int off_mock_input_set(off_mock_input_t *,const tyvor_input_sample_t *);
off_input_port_t off_mock_input_port(off_mock_input_t *,uint32_t ttl_ms);
int off_input_read(off_input_port_t *,uint64_t now,uint64_t deadline,tyvor_input_sample_t *);
typedef struct {
    void *context;
    int (*read)(void *,uint32_t,uint32_t,uint8_t *,size_t,uint64_t,uint64_t);
    int (*write)(void *,uint32_t,uint32_t,const uint8_t *,size_t,uint64_t,uint64_t);
    int (*sync)(void *,uint64_t,uint64_t);
} off_block_port_t;
typedef struct {
    uint8_t bytes[OFF_MEDIA_SECTORS][OFF_MEDIA_BLOCK];
    uint32_t delay_ms,partial_bytes;
    uint8_t present,readonly,fail_read,fail_write,fail_sync;
} off_mock_media_t;
void off_mock_media_init(off_mock_media_t *);
off_block_port_t off_mock_media_port(off_mock_media_t *);
/* RAM transactional model adapter. This is NOT FAT or a power-fail-safe SD driver. */
typedef struct {
    uint8_t committed[TYVOR_MODEL_SLOTS][TYVOR_MODEL_BYTES_MAX];
    size_t length[TYVOR_MODEL_SLOTS];
    uint8_t staging[TYVOR_MODEL_BYTES_MAX];
    uint8_t present,readonly,fail_stage,fail_sync,fail_commit;
} off_mock_store_t;
void off_mock_store_init(off_mock_store_t *);
tyvor_model_storage_t off_mock_store_adapter(off_mock_store_t *);
enum { OFF_SOUND_BOOT,OFF_SOUND_WARNING,OFF_SOUND_INPUT_LOSS,OFF_SOUND_MODEL_ERROR,OFF_SOUND_COUNT };
typedef struct { uint8_t event; uint32_t duration_ms; uint64_t expires_ms; } off_sound_t;
typedef struct {
    void *context;
    int (*shutdown)(void *,int enabled);
    int (*start)(void *,unsigned event);
    int (*stop)(void *);
} off_audio_port_t;
typedef struct {
    off_audio_port_t port; off_sound_t queue[OFF_AUDIO_QUEUE];
    uint64_t last_now,end_ms; uint32_t dropped,played;
    uint8_t used,head,active,muted,fault,initialized;
} off_audio_t;
typedef struct { uint32_t starts,stops,shutdowns; int shutdown_active,last_event,fail; } off_mock_audio_t;
off_audio_port_t off_mock_audio_port(off_mock_audio_t *);
int off_audio_init(off_audio_t *,off_audio_port_t,uint64_t now);
int off_audio_enqueue(off_audio_t *,off_sound_t,uint64_t now);
int off_audio_poll(off_audio_t *,uint64_t now);
int off_audio_mute(off_audio_t *);
#endif
