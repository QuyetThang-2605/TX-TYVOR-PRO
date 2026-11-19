#ifndef TYVOR_CRSF_H
#define TYVOR_CRSF_H

#include <stddef.h>
#include <stdint.h>
#include "tyvor_telemetry.h"

#define TYVOR_CRSF_BAUDRATE        420000u
#define TYVOR_CRSF_MAX_FRAME_LEN   64u
#define TYVOR_CRSF_RC_CHANNELS     16u

/* Standard CRSF addresses & frame types */
#define CRSF_ADDR_FLIGHT_CONTROLLER 0xC8u
#define CRSF_ADDR_RADIO_TRANSMITTER 0xEAu
#define CRSF_ADDR_CRSF_TRANSMITTER  0xEEu

#define CRSF_FRAMETYPE_GPS              0x02u
#define CRSF_FRAMETYPE_BATTERY_SENSOR   0x08u
#define CRSF_FRAMETYPE_LINK_STATISTICS  0x14u
#define CRSF_FRAMETYPE_RC_CHANNELS      0x16u
#define CRSF_FRAMETYPE_DEVICE_PING      0x28u
#define CRSF_FRAMETYPE_PARAMETER_WRITE  0x2Du

typedef int (*tyvor_crsf_tx_fn)(void *ctx, const uint8_t *bytes, size_t len);

typedef struct {
    tyvor_crsf_tx_fn tx_fn;
    void *tx_ctx;
    uint8_t rx_buf[TYVOR_CRSF_MAX_FRAME_LEN];
    uint8_t rx_pos;
    uint8_t rx_expected_len;
    uint32_t tx_frames;
    uint32_t rx_frames;
    uint32_t rx_crc_errors;
    uint64_t last_rx_ms;
    tyvor_telemetry_sample_t telem;
} tyvor_crsf_t;

void tyvor_crsf_init(tyvor_crsf_t *c, tyvor_crsf_tx_fn tx_fn, void *tx_ctx);
uint8_t tyvor_crsf_crc8(const uint8_t *data, size_t len);

/* Pack 16 channels (-1000..+1000 -> 172..1811) and transmit CRSF 0x16 RC frame */
int tyvor_crsf_send_rc(tyvor_crsf_t *c, const int16_t *channels, size_t ch_count);

/* Send CRSF 0x2D parameter write to ELRS TX module (address 0xEE from 0xEA) */
int tyvor_crsf_send_param(tyvor_crsf_t *c, uint8_t param_id, uint8_t value);

/* Send CRSF 0x28 device ping to discover ELRS TX module */
int tyvor_crsf_send_ping(tyvor_crsf_t *c);

/* Feed incoming UART bytes from ELRS TX module; returns 1 when telemetry updated */
int tyvor_crsf_feed_byte(tyvor_crsf_t *c, uint8_t byte, uint64_t now_ms);

/* Returns 1 if real ELRS telemetry was received within the last 1500ms */
int tyvor_crsf_telem_live(const tyvor_crsf_t *c, uint64_t now_ms);

#endif
