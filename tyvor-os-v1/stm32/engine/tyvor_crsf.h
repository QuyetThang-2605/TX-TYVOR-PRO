#ifndef TYVOR_CRSF_H
#define TYVOR_CRSF_H

#include <stddef.h>
#include <stdint.h>
#include "tyvor_telemetry.h"

#define TYVOR_CRSF_BAUDRATE        400000u
#define TYVOR_CRSF_MAX_FRAME_LEN   64u
#define TYVOR_CRSF_RC_CHANNELS     16u

/* Standard CRSF addresses & frame types */
#define CRSF_ADDR_FLIGHT_CONTROLLER 0xC8u
#define CRSF_ADDR_RADIO_TRANSMITTER 0xEAu
#define CRSF_ADDR_CRSF_RECEIVER     0xECu
#define CRSF_ADDR_CRSF_TRANSMITTER  0xEEu
#define CRSF_ADDR_ELRS_LUA          0xEFu

#define CRSF_FRAMETYPE_GPS              0x02u
#define CRSF_FRAMETYPE_BATTERY_SENSOR   0x08u
#define CRSF_FRAMETYPE_LINK_STATISTICS  0x14u
#define CRSF_FRAMETYPE_RC_CHANNELS      0x16u
#define CRSF_FRAMETYPE_ATTITUDE         0x1Eu
#define CRSF_FRAMETYPE_FLIGHT_MODE      0x21u
#define CRSF_FRAMETYPE_DEVICE_PING      0x28u
#define CRSF_FRAMETYPE_DEVICE_INFO      0x29u
#define CRSF_FRAMETYPE_PARAM_ENTRY      0x2Bu
#define CRSF_FRAMETYPE_PARAMETER_READ   0x2Cu
#define CRSF_FRAMETYPE_PARAMETER_WRITE  0x2Du
#define CRSF_FRAMETYPE_ELRS_STATUS      0x2Eu
#define CRSF_FRAMETYPE_COMMAND          0x32u

#define CRSF_COMMAND_SUBCMD_RX          0x10u
#define CRSF_COMMAND_SUBCMD_RX_BIND     0x01u

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
    /* Extended drone / FC telemetry & settings received over CRSFv2 */
    char flight_mode[16];
    char device_name[24];
    int16_t pitch_deg;
    int16_t roll_deg;
    int16_t yaw_deg;
    uint16_t current_deci_a;
    uint32_t capacity_mah;
    uint8_t bat_pct;
    uint8_t uplink_snr;
    uint8_t downlink_rssi;
    uint8_t downlink_lq;
    int8_t downlink_snr;
    uint8_t last_param_id;
    uint8_t last_param_val;
    /* ExpressLRS TX module parameter & Bind state */
    uint8_t field_count;
    uint8_t bind_param_id;
    uint8_t model_match_param_id;
    uint8_t rate_param_id;
    uint16_t rate_hz_options[12];
    uint8_t rate_hz_count;
    uint8_t tlm_param_id;
    uint16_t tlm_denom_options[12];
    uint8_t tlm_denom_count;
    uint8_t power_param_id;
    uint16_t power_mw_options[8];
    uint8_t power_mw_count;
    uint8_t wifi_param_id;
    uint8_t wifi_active;
    uint8_t wifi_pending;
    uint8_t wifi_saw_silence;
    uint8_t wifi_seq_step;
    uint64_t wifi_next_step_ms;
    uint64_t wifi_started_ms;
    uint8_t scan_param_id;
    uint8_t binding_active;
    uint8_t bind_pending_lua;
    uint8_t bind_seq_step;
    uint64_t bind_next_step_ms;
    uint64_t next_poll_cmd_ms;
    uint32_t bind_ack_count;
    uint64_t bind_until_ms;
    uint8_t elrs_flags;       /* bit 0: RX connected, bit 2: model mismatch, bit 3: armed */
    uint16_t elrs_good_pkts;
    uint8_t elrs_bad_pkts;
} tyvor_crsf_t;

void tyvor_crsf_init(tyvor_crsf_t *c, tyvor_crsf_tx_fn tx_fn, void *tx_ctx);
uint8_t tyvor_crsf_crc8(const uint8_t *data, size_t len);

/* Pack 16 channels (-1000..+1000 -> 172..1811) and transmit CRSF 0x16 RC frame */
int tyvor_crsf_send_rc(tyvor_crsf_t *c, const int16_t *channels, size_t ch_count);

/* Send CRSF 0x2C parameter read to ELRS TX module (address 0xEE from 0xEA) */
int tyvor_crsf_send_param_read(tyvor_crsf_t *c, uint8_t param_id, uint8_t chunk);

/* Send CRSF 0x2D parameter write to ELRS TX module (address 0xEE from 0xEA) */
int tyvor_crsf_send_param(tyvor_crsf_t *c, uint8_t param_id, uint8_t value);
int tyvor_crsf_set_power_mw(tyvor_crsf_t *c, uint16_t target_mw);
int tyvor_crsf_set_rate_hz(tyvor_crsf_t *c, uint16_t target_hz);
int tyvor_crsf_set_tlm_ratio(tyvor_crsf_t *c, uint16_t target_denom);

/* Send CRSF 0x32 + 0x2D Bind command sequence to put ExpressLRS TX module into Binding Mode */
int tyvor_crsf_send_bind(tyvor_crsf_t *c, uint64_t now_ms);

/* Cancel active Bind mode on ExpressLRS TX module */
int tyvor_crsf_cancel_bind(tyvor_crsf_t *c);

/* Enable or cancel ExpressLRS TX module WiFi AP ("ExpressLRS TX") */
int tyvor_crsf_set_wifi(tyvor_crsf_t *c, uint8_t enable, uint64_t now_ms);

/* Send CRSF 0x28 device ping to discover ELRS TX module */
int tyvor_crsf_send_ping(tyvor_crsf_t *c);

/* Periodic non-colliding command/bind scheduler; returns 1 if a command frame was sent this tick */
int tyvor_crsf_poll(tyvor_crsf_t *c, uint64_t now_ms);

/* Feed incoming UART bytes from ELRS TX module; returns 1 when telemetry updated */
int tyvor_crsf_feed_byte(tyvor_crsf_t *c, uint8_t byte, uint64_t now_ms);

/* Returns 1 if real ELRS telemetry was received within the last 1500ms */
int tyvor_crsf_telem_live(const tyvor_crsf_t *c, uint64_t now_ms);

#endif
