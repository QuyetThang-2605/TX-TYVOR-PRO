#include "tyvor_crsf.h"
#include <string.h>

uint8_t tyvor_crsf_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0u;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ 0xD5u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

void tyvor_crsf_init(tyvor_crsf_t *c, tyvor_crsf_tx_fn tx_fn, void *tx_ctx) {
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->tx_fn = tx_fn;
    c->tx_ctx = tx_ctx;
}

static uint16_t tyvor_to_crsf_ch(int16_t raw) {
    if (raw < -1000) raw = -1000;
    if (raw > 1000) raw = 1000;
    /* Map -1000..+1000 -> 172..1811 (center 992) */
    int32_t v = 992 + ((int32_t)raw * 819) / 1000;
    if (v < 172) v = 172;
    if (v > 1811) v = 1811;
    return (uint16_t)v;
}

int tyvor_crsf_send_rc(tyvor_crsf_t *c, const int16_t *channels, size_t ch_count) {
    if (!c || !c->tx_fn) return -1;
    uint8_t frame[26];
    frame[0] = CRSF_ADDR_CRSF_TRANSMITTER; /* 0xEE */
    frame[1] = 24u;                        /* type(1) + payload(22) + crc(1) */
    frame[2] = CRSF_FRAMETYPE_RC_CHANNELS; /* 0x16 */

    uint16_t ch[TYVOR_CRSF_RC_CHANNELS];
    for (size_t i = 0; i < TYVOR_CRSF_RC_CHANNELS; ++i) {
        ch[i] = (channels && i < ch_count) ? tyvor_to_crsf_ch(channels[i]) : 992u;
    }

    uint32_t bitbuf = 0u;
    uint8_t bitcnt = 0u;
    size_t out = 3u;
    for (size_t i = 0; i < TYVOR_CRSF_RC_CHANNELS; ++i) {
        bitbuf |= ((uint32_t)(ch[i] & 0x07FFu)) << bitcnt;
        bitcnt += 11u;
        while (bitcnt >= 8u) {
            frame[out++] = (uint8_t)(bitbuf & 0xFFu);
            bitbuf >>= 8;
            bitcnt -= 8u;
        }
    }
    frame[25] = tyvor_crsf_crc8(&frame[2], 23u);
    if (c->tx_fn(c->tx_ctx, frame, sizeof(frame)) != 0) return -1;
    ++c->tx_frames;
    return 0;
}

int tyvor_crsf_send_param(tyvor_crsf_t *c, uint8_t param_id, uint8_t value) {
    if (!c || !c->tx_fn) return -1;
    uint8_t frame[8];
    frame[0] = CRSF_ADDR_CRSF_TRANSMITTER;   /* 0xEE */
    frame[1] = 6u;                           /* type + dst + src + id + val + crc */
    frame[2] = CRSF_FRAMETYPE_PARAMETER_WRITE; /* 0x2D */
    frame[3] = CRSF_ADDR_CRSF_TRANSMITTER;   /* Destination: TX module */
    frame[4] = CRSF_ADDR_RADIO_TRANSMITTER;  /* Origin: Handset */
    frame[5] = param_id;
    frame[6] = value;
    frame[7] = tyvor_crsf_crc8(&frame[2], 5u);
    if (c->tx_fn(c->tx_ctx, frame, sizeof(frame)) != 0) return -1;
    ++c->tx_frames;
    return 0;
}

int tyvor_crsf_send_ping(tyvor_crsf_t *c) {
    if (!c || !c->tx_fn) return -1;
    uint8_t frame[6];
    frame[0] = CRSF_ADDR_CRSF_TRANSMITTER;
    frame[1] = 4u;
    frame[2] = CRSF_FRAMETYPE_DEVICE_PING;
    frame[3] = 0x00u; /* Broadcast */
    frame[4] = CRSF_ADDR_RADIO_TRANSMITTER;
    frame[5] = tyvor_crsf_crc8(&frame[2], 3u);
    if (c->tx_fn(c->tx_ctx, frame, sizeof(frame)) != 0) return -1;
    ++c->tx_frames;
    return 0;
}

static int32_t read_be32(const uint8_t *p) {
    return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static int tyvor_crsf_decode_frame(tyvor_crsf_t *c, uint64_t now_ms) {
    uint8_t len = c->rx_buf[1];
    uint8_t type = c->rx_buf[2];
    const uint8_t *pl = &c->rx_buf[3];
    size_t pl_len = (len >= 2u) ? (size_t)(len - 2u) : 0u;
    int updated = 0;

    if (type == CRSF_FRAMETYPE_LINK_STATISTICS && pl_len >= 10u) {
        c->telem.values.rssi_dbm = (int16_t)(-(int16_t)pl[0]);
        c->telem.values.lq = pl[2];
        c->telem.present |= (uint8_t)(TYVOR_T_RSSI | TYVOR_T_LQ);
        c->telem.values.valid |= (uint8_t)(TYVOR_T_RSSI | TYVOR_T_LQ);
        updated = 1;
    } else if (type == CRSF_FRAMETYPE_BATTERY_SENSOR && pl_len >= 2u) {
        /* CRSF voltage is big-endian in 0.1V units -> convert to mV */
        uint16_t dv = read_be16(pl);
        c->telem.values.battery_mv = (uint16_t)(dv * 100u);
        c->telem.present |= TYVOR_T_BATTERY;
        c->telem.values.valid |= TYVOR_T_BATTERY;
        updated = 1;
    } else if (type == CRSF_FRAMETYPE_GPS && pl_len >= 15u) {
        c->telem.values.latitude_e7 = read_be32(&pl[0]);
        c->telem.values.longitude_e7 = read_be32(&pl[4]);
        c->telem.values.satellites = pl[14];
        c->telem.present |= TYVOR_T_GPS;
        c->telem.values.valid |= TYVOR_T_GPS;
        updated = 1;
    }

    if (updated) {
        c->telem.sampled_ms = now_ms;
        c->last_rx_ms = now_ms;
    }
    ++c->rx_frames;
    return updated;
}

int tyvor_crsf_feed_byte(tyvor_crsf_t *c, uint8_t byte, uint64_t now_ms) {
    if (!c) return 0;
    if (c->rx_pos == 0u) {
        if (byte == CRSF_ADDR_RADIO_TRANSMITTER ||
            byte == CRSF_ADDR_FLIGHT_CONTROLLER ||
            byte == CRSF_ADDR_CRSF_TRANSMITTER) {
            c->rx_buf[c->rx_pos++] = byte;
        }
        return 0;
    }

    if (c->rx_pos == 1u) {
        if (byte < 2u || byte > (TYVOR_CRSF_MAX_FRAME_LEN - 2u)) {
            c->rx_pos = 0u;
            return 0;
        }
        c->rx_buf[c->rx_pos++] = byte;
        c->rx_expected_len = (uint8_t)(byte + 2u);
        return 0;
    }

    c->rx_buf[c->rx_pos++] = byte;
    if (c->rx_pos < c->rx_expected_len) {
        return 0;
    }

    uint8_t len = c->rx_buf[1];
    uint8_t calc_crc = tyvor_crsf_crc8(&c->rx_buf[2], (size_t)(len - 1u));
    uint8_t rx_crc = c->rx_buf[c->rx_expected_len - 1u];
    c->rx_pos = 0u;

    if (calc_crc != rx_crc) {
        ++c->rx_crc_errors;
        return 0;
    }
    return tyvor_crsf_decode_frame(c, now_ms);
}

int tyvor_crsf_telem_live(const tyvor_crsf_t *c, uint64_t now_ms) {
    if (!c || c->last_rx_ms == 0u || now_ms < c->last_rx_ms) return 0;
    return (now_ms - c->last_rx_ms) <= 1500u ? 1 : 0;
}
