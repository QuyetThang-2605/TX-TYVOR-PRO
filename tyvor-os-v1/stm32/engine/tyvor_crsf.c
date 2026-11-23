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

static uint8_t tyvor_crsf_crc8_ba(const uint8_t *data, size_t len) {
    uint8_t crc = 0u;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ 0xBAu);
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
    /* ExpressLRS hardcodes CH5 (AUX1, index 4) > 1500us as isArmed=true ("[ ! Armed ! ]"),
     * which blocks entering Bind mode, Sync packets, and RF parameter changes!
     * Keep CH5 disarmed (172 = 988us) whenever no RX is connected yet or while Binding is active. */
    if (c->binding_active || c->bind_seq_step > 0u || !(c->elrs_flags & 0x01u)) {
        ch[4] = 172u;
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

int tyvor_crsf_send_param_read(tyvor_crsf_t *c, uint8_t param_id, uint8_t chunk) {
    if (!c || !c->tx_fn) return -1;
    uint8_t frame[8];
    frame[0] = CRSF_ADDR_CRSF_TRANSMITTER;     /* 0xEE */
    frame[1] = 6u;                             /* type + dst + src + id + chunk + crc */
    frame[2] = CRSF_FRAMETYPE_PARAMETER_READ;  /* 0x2C */
    frame[3] = CRSF_ADDR_CRSF_TRANSMITTER;     /* 0xEE: Destination TX module */
    frame[4] = CRSF_ADDR_RADIO_TRANSMITTER;    /* 0xEA: Origin Handset (matches elrs.lua) */
    frame[5] = param_id;
    frame[6] = chunk;
    frame[7] = tyvor_crsf_crc8(&frame[2], 5u);
    if (c->tx_fn(c->tx_ctx, frame, sizeof(frame)) != 0) return -1;
    ++c->tx_frames;
    return 0;
}

int tyvor_crsf_send_param(tyvor_crsf_t *c, uint8_t param_id, uint8_t value) {
    if (!c || !c->tx_fn) return -1;
    uint8_t frame[8];
    frame[0] = CRSF_ADDR_CRSF_TRANSMITTER;     /* 0xEE */
    frame[1] = 6u;                             /* type + dst + src + id + val + crc */
    frame[2] = CRSF_FRAMETYPE_PARAMETER_WRITE; /* 0x2D */
    frame[3] = CRSF_ADDR_CRSF_TRANSMITTER;     /* 0xEE: Destination TX module */
    frame[4] = CRSF_ADDR_RADIO_TRANSMITTER;    /* 0xEA: Origin Handset (matches elrs.lua) */
    frame[5] = param_id;
    frame[6] = value;
    frame[7] = tyvor_crsf_crc8(&frame[2], 5u);
    if (c->tx_fn(c->tx_ctx, frame, sizeof(frame)) != 0) return -1;
    ++c->tx_frames;
    return 0;
}

static int tyvor_crsf_send_cmd_bind(tyvor_crsf_t *c) {
    if (!c || !c->tx_fn) return -1;
    uint8_t cmd[9];
    cmd[0] = CRSF_ADDR_CRSF_TRANSMITTER;     /* 0xEE */
    cmd[1] = 7u;                             /* type + dst + src + subcmd_rx + subcmd_bind + crc_ba + crc_d5 */
    cmd[2] = CRSF_FRAMETYPE_COMMAND;         /* 0x32 */
    cmd[3] = CRSF_ADDR_CRSF_TRANSMITTER;     /* 0xEE: Destination TX Module */
    cmd[4] = CRSF_ADDR_RADIO_TRANSMITTER;    /* 0xEA: Origin Handset */
    cmd[5] = CRSF_COMMAND_SUBCMD_RX;         /* 0x10 */
    cmd[6] = CRSF_COMMAND_SUBCMD_RX_BIND;    /* 0x01 */
    cmd[7] = tyvor_crsf_crc8_ba(&cmd[2], 5u);
    cmd[8] = tyvor_crsf_crc8(&cmd[2], 6u);
    if (c->tx_fn(c->tx_ctx, cmd, sizeof(cmd)) != 0) return -1;
    ++c->tx_frames;
    return 0;
}

int tyvor_crsf_send_bind(tyvor_crsf_t *c, uint64_t now_ms) {
    if (!c || !c->tx_fn) return -1;
    c->binding_active = 1u;
    c->bind_until_ms = now_ms + 3500u; /* Next continuous bind pulse cycle in 3.5s if RX not connected yet */
    c->bind_seq_step = 1u;
    c->bind_next_step_ms = now_ms + 24u; /* First 24ms sends disarmed CH5=172 RC frames to clear isArmed */
    return 0;
}

int tyvor_crsf_cancel_bind(tyvor_crsf_t *c) {
    if (!c) return -1;
    c->binding_active = 0u;
    c->bind_pending_lua = 0u;
    c->bind_seq_step = 0u;
    c->bind_until_ms = 0u;
    if (c->bind_param_id > 0u) {
        (void)tyvor_crsf_send_param(c, c->bind_param_id, 5u); /* lcsCancel = 5 */
    }
    return 0;
}

int tyvor_crsf_set_wifi(tyvor_crsf_t *c, uint8_t enable, uint64_t now_ms) {
    if (!c || !c->tx_fn) return -1;
    if (enable) {
        c->binding_active = 0u;
        c->bind_seq_step = 0u;
        c->wifi_active = 1u;
        c->wifi_saw_silence = 0u;
        c->wifi_started_ms = now_ms;
        if (c->wifi_param_id > 0u) {
            c->wifi_pending = 0u;
            c->wifi_seq_step = 1u;
            c->wifi_next_step_ms = 0u;
            return 0;
        } else {
            /* Wait for scan_param_id to locate "Enable WiFi" and trigger immediately */
            c->wifi_pending = 1u;
            if (c->field_count == 0u) {
                (void)tyvor_crsf_send_ping(c);
            } else if (c->scan_param_id == 0u) {
                c->scan_param_id = c->field_count;
            }
            return 0;
        }
    } else {
        c->wifi_active = 0u;
        c->wifi_pending = 0u;
        c->wifi_saw_silence = 0u;
        c->wifi_seq_step = 0u;
        if (c->wifi_param_id > 0u) {
            return tyvor_crsf_send_param(c, c->wifi_param_id, 5u); /* lcsCancel = 5 -> reboots back to RF */
        }
        return 0;
    }
}

int tyvor_crsf_send_ping(tyvor_crsf_t *c) {
    if (!c || !c->tx_fn) return -1;
    uint8_t frame[6];
    frame[0] = CRSF_ADDR_CRSF_TRANSMITTER;
    frame[1] = 4u;
    frame[2] = CRSF_FRAMETYPE_DEVICE_PING;
    frame[3] = 0x00u; /* Broadcast */
    frame[4] = CRSF_ADDR_RADIO_TRANSMITTER;    /* 0xEA: Origin Handset (matches elrs.lua) */
    frame[5] = tyvor_crsf_crc8(&frame[2], 3u);
    if (c->tx_fn(c->tx_ctx, frame, sizeof(frame)) != 0) return -1;
    ++c->tx_frames;
    return 0;
}

int tyvor_crsf_poll(tyvor_crsf_t *c, uint64_t now_ms) {
    if (!c || !c->tx_fn) return 0;

    /* 0. Non-colliding WiFi AP sequence (spaced 50ms apart so CRSFHandset never drops a frame) */
    if (c->wifi_seq_step > 0u && now_ms >= c->wifi_next_step_ms) {
        uint8_t wid = c->wifi_param_id ? c->wifi_param_id : 16u;
        if (c->wifi_seq_step == 1u) {
            (void)tyvor_crsf_send_param_read(c, wid, 0u); /* Reset command state to lcsIdle */
            c->wifi_seq_step = 2u;
            c->wifi_next_step_ms = now_ms + 50u;
            return 1;
        } else if (c->wifi_seq_step == 2u) {
            (void)tyvor_crsf_send_param(c, wid, 1u); /* lcsClick = 1 */
            c->wifi_seq_step = 3u;
            c->wifi_next_step_ms = now_ms + 50u;
            return 1;
        } else if (c->wifi_seq_step == 3u) {
            (void)tyvor_crsf_send_param(c, wid, 4u); /* lcsConfirmed = 4 */
            c->wifi_seq_step = 4u;
            c->wifi_next_step_ms = now_ms + 120u;
            return 1;
        } else {
            (void)tyvor_crsf_send_param(c, wid, 4u); /* Backup lcsConfirmed = 4 */
            c->wifi_seq_step = 0u;
            return 1;
        }
    }

    /* Auto-exit WiFi/Bind mode when user finishes setup (module reboots & CRSF resumes, or RX binds) */
    if (c->wifi_active && c->wifi_seq_step == 0u && !c->wifi_pending) {
        int live = tyvor_crsf_telem_live(c, now_ms);
        if (!live && (now_ms >= c->wifi_started_ms + 3000u)) {
            c->wifi_saw_silence = 1u;
        }
        if ((c->wifi_saw_silence && live) ||
            (c->elrs_flags & 0x01u) ||
            ((c->telem.present & (TYVOR_T_LQ | TYVOR_T_RSSI)) && c->telem.values.lq > 0u)) {
            c->wifi_active = 0u;
            c->wifi_saw_silence = 0u;
        }
    }

    /* 1. Multi-step non-colliding Bind sequence (runs after CH5=172 disarmed RC frames have cleared isArmed) */
    if (c->bind_seq_step > 0u && now_ms >= c->bind_next_step_ms) {
        uint8_t step = c->bind_seq_step;
        uint8_t bid = (c->bind_param_id > 0u) ? c->bind_param_id
                      : ((c->field_count >= 2u) ? (uint8_t)(c->field_count - 1u) : 15u);
        if (step == 1u) {
            /* Ensure we know field_count; if not, ping first */
            if (c->field_count == 0u) {
                (void)tyvor_crsf_send_ping(c);
            } else if (c->model_match_param_id > 0u) {
                /* Disable Model Match so any RX in bind mode can link immediately */
                (void)tyvor_crsf_send_param(c, c->model_match_param_id, 0u);
            } else {
                (void)tyvor_crsf_send_cmd_bind(c);
            }
            c->bind_seq_step = 2u;
            c->bind_next_step_ms = now_ms + 45u;
            return 1;
        } else if (step == 2u) {
            /* Send official CRSF Command Bind (0x32: SUBCMD_RX=0x10, SUBCMD_RX_BIND=0x01) */
            (void)tyvor_crsf_send_cmd_bind(c);
            c->bind_seq_step = 3u;
            c->bind_next_step_ms = now_ms + 45u;
            return 1;
        } else if (step == 3u) {
            /* Read chunk 0 of Bind parameter (0x2C) to reset luaBind.step = lcsIdle */
            (void)tyvor_crsf_send_param_read(c, bid, 0u);
            c->bind_seq_step = 4u;
            c->bind_next_step_ms = now_ms + 50u;
            return 1;
        } else if (step == 4u) {
            /* Trigger ExpressLRS Lua Parameter Bind (0x2D: lcsClick = 1) */
            (void)tyvor_crsf_send_param(c, bid, 1u);
            c->bind_seq_step = 5u;
            c->bind_next_step_ms = now_ms + 120u;
            return 1;
        } else if (step == 5u) {
            /* Backup repeat of 0x2D lcsClick=1 to guarantee EnterBindingModeSafely() */
            (void)tyvor_crsf_send_param(c, bid, 1u);
            c->bind_seq_step = 6u;
            c->bind_next_step_ms = now_ms + 120u;
            return 1;
        } else {
            (void)tyvor_crsf_send_cmd_bind(c);
            c->bind_seq_step = 0u;
            c->bind_until_ms = now_ms + 3200u; /* Schedule next continuous bind burst after 2s bind + 1.2s sync window */
            return 1;
        }
    }

    /* Continuous Bind loop: if still binding and RX has not connected yet, re-trigger Bind burst every 3.2s */
    if (c->binding_active) {
        if ((c->elrs_flags & 0x01u) ||
            ((c->telem.present & (TYVOR_T_LQ | TYVOR_T_RSSI)) && c->telem.values.lq > 0u)) {
            /* Receiver bound & linked! Exit continuous bind mode automatically */
            c->binding_active = 0u;
            c->bind_seq_step = 0u;
            c->bind_until_ms = 0u;
        } else if (c->bind_until_ms > 0u && now_ms >= c->bind_until_ms) {
            c->bind_until_ms = now_ms + 3200u;
            c->bind_seq_step = 2u; /* Re-trigger 0x32 + 0x2C + 0x2D Bind burst */
            c->bind_next_step_ms = now_ms + 10u;
        }
    }

    /* 2. Background discovery & status polling (spaced so it never collides with 0x16 RC packets) */
    if (now_ms < c->next_poll_cmd_ms) return 0;

    if (!tyvor_crsf_telem_live(c, now_ms) || c->field_count == 0u) {
        c->next_poll_cmd_ms = now_ms + 800u;
        (void)tyvor_crsf_send_ping(c);
        return 1;
    }

    /* Scan parameter entries (from field_count down to 1) until bind_param_id is verified by name "Bind" */
    if (c->scan_param_id > 0u && c->scan_param_id <= c->field_count) {
        uint8_t pid = c->scan_param_id--;
        c->next_poll_cmd_ms = now_ms + 110u;
        (void)tyvor_crsf_send_param_read(c, pid, 0u);
        return 1;
    }

    /* Poll ExpressLRS 0x2E status packet (good/bad packets + RX connected flag) every 150ms */
    c->next_poll_cmd_ms = now_ms + 150u;
    (void)tyvor_crsf_send_param(c, 0u, 0u);
    return 1;
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
        uint8_t raw_rssi = (pl[4] == 1u && pl[1] > 0u && pl[1] <= 140u) ? pl[1]
                         : (pl[0] > 0u && pl[0] <= 140u) ? pl[0]
                         : (pl[1] > 0u && pl[1] <= 140u) ? pl[1]
                         : (pl[7] > 0u && pl[7] <= 140u) ? pl[7] : 0u;
        int16_t rssi = (raw_rssi > 0u) ? (int16_t)(-(int16_t)raw_rssi) : 0;
        if (rssi < -150) rssi = -150;
        if (rssi > 0) rssi = 0;
        uint8_t lq = (pl[2] > 100u) ? 100u : pl[2];
        c->telem.values.rssi_dbm = rssi;
        c->telem.values.lq = lq;
        c->uplink_snr = pl[3];
        c->downlink_rssi = pl[7];
        c->downlink_lq = pl[8];
        c->downlink_snr = (int8_t)pl[9];
        c->telem.present = (uint8_t)(TYVOR_T_RSSI | TYVOR_T_LQ);
        c->telem.values.valid = c->telem.present;
        updated = 1;
    } else if (type == CRSF_FRAMETYPE_BATTERY_SENSOR && pl_len >= 2u) {
        /* CRSF voltage is big-endian in 0.1V units -> convert to mV */
        uint16_t dv = read_be16(pl);
        uint16_t mv = (uint16_t)(dv * 100u);
        if (pl_len >= 4u) c->current_deci_a = read_be16(&pl[2]);
        if (pl_len >= 7u) {
            c->capacity_mah = ((uint32_t)pl[4] << 16) | ((uint32_t)pl[5] << 8) | (uint32_t)pl[6];
        }
        if (pl_len >= 8u) c->bat_pct = pl[7];
        if (mv > 0u) {
            c->telem.values.battery_mv = mv;
            c->telem.present = TYVOR_T_BATTERY;
            c->telem.values.valid = TYVOR_T_BATTERY;
            updated = 1;
        }
    } else if (type == CRSF_FRAMETYPE_GPS && pl_len >= 15u) {
        int32_t lat = read_be32(&pl[0]);
        int32_t lon = read_be32(&pl[4]);
        uint8_t sats = (pl[14] > 64u) ? 64u : pl[14];
        if (lat >= -900000000 && lat <= 900000000 &&
            lon >= -1800000000 && lon <= 1800000000) {
            c->telem.values.latitude_e7 = lat;
            c->telem.values.longitude_e7 = lon;
            c->telem.values.satellites = sats;
            c->telem.present = TYVOR_T_GPS;
            c->telem.values.valid = TYVOR_T_GPS;
            updated = 1;
        }
    } else if (type == CRSF_FRAMETYPE_ATTITUDE && pl_len >= 6u) {
        int16_t p_rad10k = (int16_t)read_be16(&pl[0]);
        int16_t r_rad10k = (int16_t)read_be16(&pl[2]);
        int16_t y_rad10k = (int16_t)read_be16(&pl[4]);
        c->pitch_deg = (int16_t)(((int32_t)p_rad10k * 180) / 31416);
        c->roll_deg  = (int16_t)(((int32_t)r_rad10k * 180) / 31416);
        c->yaw_deg   = (int16_t)(((int32_t)y_rad10k * 180) / 31416);
        c->last_rx_ms = now_ms;
        updated = 2;
    } else if (type == CRSF_FRAMETYPE_FLIGHT_MODE && pl_len >= 1u) {
        /* Active Flight Mode string configured on Drone FC (e.g. "ACRO", "ANGLE", "RTH", "!FS!") */
        size_t j = 0u;
        for (size_t i = 0u; i < pl_len && j < 11u; ++i) {
            uint8_t ch = pl[i];
            if (ch == 0u) break;
            if (ch >= 32u && ch <= 126u) {
                c->flight_mode[j++] = (char)ch;
            }
        }
        c->flight_mode[j] = '\0';
        c->last_rx_ms = now_ms;
        updated = 2;
    } else if (type == CRSF_FRAMETYPE_DEVICE_INFO && pl_len >= 16u) {
        /* Extended header: [dst][src][device_name\0][serial(4)][hw(4)][sw(4)][fieldCnt(1)][paramVer(1)] */
        if (pl[1] == CRSF_ADDR_CRSF_TRANSMITTER) {
            size_t j = 0u;
            for (size_t i = 2u; i < pl_len && j < sizeof(c->device_name) - 1u; ++i) {
                uint8_t ch = pl[i];
                if (ch == 0u) break;
                if (ch >= 32u && ch <= 126u) {
                    c->device_name[j++] = (char)ch;
                }
            }
            c->device_name[j] = '\0';
            uint8_t fcnt = pl[pl_len - 2u];
            if (fcnt >= 2u && fcnt < 64u) {
                if (c->field_count != fcnt) {
                    c->field_count = fcnt;
                    c->scan_param_id = fcnt; /* Start scanning parameters from fcnt down to 1 */
                }
                /* In ExpressLRS TXModuleParameters.cpp, luaBind is registered right before luaELRSversion */
                if (c->bind_param_id == 0u) {
                    c->bind_param_id = (uint8_t)(fcnt - 1u);
                }
            }
        }
        c->last_rx_ms = now_ms;
        updated = 2;
    } else if (type == CRSF_FRAMETYPE_PARAM_ENTRY && pl_len >= 7u) {
        uint8_t pid = pl[2];
        uint8_t chunks_rem = pl[3];
        uint8_t dtype = (uint8_t)(pl[5] & 0x7Fu);
        const char *pname = (const char *)&pl[6];
        c->last_param_id = pid;
        c->last_param_val = chunks_rem;

        /* Match parameter names in chunk 0 from ExpressLRS TXModuleParameters.cpp */
        if (pl_len >= 11u && strncmp(pname, "Bind", 5) == 0 && dtype == 13u) {
            c->bind_param_id = pid;
            if (chunks_rem == 0u && pl_len >= 13u) {
                uint8_t step = pl[11]; /* commandStep_e: 0=lcsIdle, 2=lcsExecuting */
                if (step == 2u) {
                    ++c->bind_ack_count;
                }
            }
        } else if (pl_len >= 18u && strncmp(pname, "Model Match", 12) == 0) {
            c->model_match_param_id = pid;
        } else if (pl_len >= 18u && strncmp(pname, "Packet Rate", 12) == 0) {
            c->rate_param_id = pid;
            if (chunks_rem == 0u && pl_len > 18u) {
                size_t pos = 18u;
                c->rate_hz_count = 0u;
                while (pos < pl_len && pl[pos] != 0u && c->rate_hz_count < 12u) {
                    while (pos < pl_len && pl[pos] != 0u && pl[pos] != ';' &&
                           !(pl[pos] >= '0' && pl[pos] <= '9')) {
                        ++pos;
                    }
                    uint32_t hz = 0u;
                    int has_digit = 0;
                    while (pos < pl_len && pl[pos] >= '0' && pl[pos] <= '9') {
                        hz = hz * 10u + (uint32_t)(pl[pos] - '0');
                        has_digit = 1;
                        ++pos;
                    }
                    if (has_digit && hz >= 25u && hz <= 2000u) {
                        c->rate_hz_options[c->rate_hz_count++] = (uint16_t)hz;
                    }
                    while (pos < pl_len && pl[pos] != 0u && pl[pos] != ';') ++pos;
                    if (pos < pl_len && pl[pos] == ';') ++pos;
                }
            }
        } else if (pl_len >= 18u && strncmp(pname, "Telem Ratio", 12) == 0) {
            c->tlm_param_id = pid;
            if (chunks_rem == 0u && pl_len > 18u) {
                size_t pos = 18u;
                c->tlm_denom_count = 0u;
                while (pos < pl_len && pl[pos] != 0u && c->tlm_denom_count < 12u) {
                    uint16_t denom = 0u; /* 0 = Std/Off/Race */
                    if (pos + 2u < pl_len && pl[pos] == '1' && pl[pos + 1u] == ':') {
                        pos += 2u;
                        uint32_t d = 0u;
                        while (pos < pl_len && pl[pos] >= '0' && pl[pos] <= '9') {
                            d = d * 10u + (uint32_t)(pl[pos] - '0');
                            ++pos;
                        }
                        if (d <= 1024u) denom = (uint16_t)d;
                    }
                    c->tlm_denom_options[c->tlm_denom_count++] = denom;
                    while (pos < pl_len && pl[pos] != 0u && pl[pos] != ';') ++pos;
                    if (pos < pl_len && pl[pos] == ';') ++pos;
                }
            }
        } else if (pl_len >= 16u && strncmp(pname, "Max Power", 10) == 0) {
            c->power_param_id = pid;
            if (chunks_rem == 0u && pl_len > 16u) {
                /* Parse ';' separated mW options after "Max Power\0" (e.g. "10;25;50;100;250;500mW") */
                size_t pos = 16u;
                c->power_mw_count = 0u;
                while (pos < pl_len && pl[pos] != 0u && c->power_mw_count < 8u) {
                    uint32_t mw = 0u;
                    int has_digit = 0;
                    while (pos < pl_len && pl[pos] >= '0' && pl[pos] <= '9') {
                        mw = mw * 10u + (uint32_t)(pl[pos] - '0');
                        has_digit = 1;
                        ++pos;
                    }
                    if (has_digit && mw > 0u && mw <= 5000u) {
                        c->power_mw_options[c->power_mw_count++] = (uint16_t)mw;
                    }
                    while (pos < pl_len && pl[pos] != 0u && pl[pos] != ';') ++pos;
                    if (pos < pl_len && pl[pos] == ';') ++pos;
                }
            }
        } else if (pl_len >= 18u && strncmp(pname, "Enable WiFi", 12) == 0 && dtype == 13u) {
            c->wifi_param_id = pid;
            if (c->wifi_pending) {
                c->wifi_pending = 0u;
                c->wifi_seq_step = 1u;
                c->wifi_next_step_ms = 0u;
            } else if (c->wifi_seq_step == 0u && chunks_rem == 0u && pl_len >= 20u) {
                uint8_t step = pl[18]; /* lcsExecuting = 2 */
                if (step == 2u) c->wifi_active = 1u;
            }
        }
        c->last_rx_ms = now_ms;
        updated = 2;
    } else if (type == CRSF_FRAMETYPE_ELRS_STATUS && pl_len >= 6u) {
        /* [dst][src][pktsBad(1)][pktsGood(2 BE)][flags(1)][msg\0] */
        c->elrs_bad_pkts = pl[2];
        c->elrs_good_pkts = read_be16(&pl[3]);
        c->elrs_flags = pl[5];
        c->last_rx_ms = now_ms;
        if (!(c->elrs_flags & 0x01u)) {
            /* ELRS TX explicitly reports RX disconnected: clear LQ/RSSI and stale flight mode immediately */
            c->flight_mode[0] = '\0';
            c->telem.present = (uint8_t)(TYVOR_T_RSSI | TYVOR_T_LQ);
            c->telem.values.valid = 0u;
            c->telem.values.lq = 0u;
            c->telem.values.rssi_dbm = 0;
            updated = 1;
        } else if (!(c->telem.present & TYVOR_T_LQ) || c->telem.values.lq == 0u) {
            /* ELRS TX reports RX connected before next 0x14 Link Stats: surface LQ=100 immediately */
            c->telem.present = TYVOR_T_LQ;
            c->telem.values.valid = TYVOR_T_LQ;
            c->telem.values.lq = 100u;
            updated = 1;
        } else {
            updated = 2;
        }
    }

    /* Every valid CRC8-verified CRSF frame from ELRS TX module proves 2-way UART link is alive */
    c->last_rx_ms = now_ms;
    if (updated == 1) {
        c->telem.sampled_ms = now_ms;
    }
    ++c->rx_frames;
    return (updated > 0) ? updated : 2;
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
    return (now_ms - c->last_rx_ms) <= 1000u ? 1 : 0;
}

int tyvor_crsf_set_power_mw(tyvor_crsf_t *c, uint16_t target_mw) {
    if (!c) return -1;
    uint8_t pid = c->power_param_id ? c->power_param_id : 6u;
    uint8_t idx = 3u;
    if (c->power_mw_count > 0u) {
        uint32_t best_diff = 0xFFFFFFFFu;
        for (uint8_t i = 0u; i < c->power_mw_count; ++i) {
            uint16_t opt = c->power_mw_options[i];
            uint32_t diff = (opt >= target_mw) ? (uint32_t)(opt - target_mw) : (uint32_t)(target_mw - opt);
            if (diff < best_diff) {
                best_diff = diff;
                idx = i;
            }
        }
    } else {
        /* Standard ExpressLRS 2.4GHz option order: 0=10, 1=25, 2=50, 3=100, 4=250, 5=500, 6=1000mW */
        if (target_mw <= 50u) idx = 2u;
        else if (target_mw <= 100u) idx = 3u;
        else if (target_mw <= 250u) idx = 4u;
        else if (target_mw <= 500u) idx = 5u;
        else idx = 6u;
    }
    return tyvor_crsf_send_param(c, pid, idx);
}

int tyvor_crsf_set_rate_hz(tyvor_crsf_t *c, uint16_t target_hz) {
    if (!c) return -1;
    uint8_t pid = c->rate_param_id ? c->rate_param_id : 1u;
    uint8_t idx = 0u;
    if (c->rate_hz_count > 0u) {
        uint32_t best_diff = 0xFFFFFFFFu;
        for (uint8_t i = 0u; i < c->rate_hz_count; ++i) {
            uint16_t opt = c->rate_hz_options[i];
            uint32_t diff = (opt >= target_hz) ? (uint32_t)(opt - target_hz) : (uint32_t)(target_hz - opt);
            if (diff < best_diff) {
                best_diff = diff;
                idx = i;
            }
        }
    } else {
        if (target_hz <= 50u) idx = 0u;
        else if (target_hz <= 150u) idx = 2u;
        else if (target_hz <= 250u) idx = 3u;
        else idx = 5u;
    }
    return tyvor_crsf_send_param(c, pid, idx);
}

int tyvor_crsf_set_tlm_ratio(tyvor_crsf_t *c, uint16_t target_denom) {
    if (!c) return -1;
    uint8_t pid = c->tlm_param_id ? c->tlm_param_id : 2u;
    uint8_t idx = 0u;
    if (c->tlm_denom_count > 0u) {
        uint32_t best_diff = 0xFFFFFFFFu;
        for (uint8_t i = 0u; i < c->tlm_denom_count; ++i) {
            uint16_t opt = c->tlm_denom_options[i];
            if (opt == 0u) continue;
            uint32_t diff = (opt >= target_denom) ? (uint32_t)(opt - target_denom) : (uint32_t)(target_denom - opt);
            if (diff < best_diff) {
                best_diff = diff;
                idx = i;
            }
        }
    } else {
        /* ExpressLRS 3.x standard order: 0=Std, 1=Off, 2=1:128, 3=1:64, 4=1:32, 5=1:16, 6=1:8, 7=1:4, 8=1:2 */
        if (target_denom <= 2u) idx = 8u;
        else if (target_denom <= 16u) idx = 5u;
        else if (target_denom <= 64u) idx = 3u;
        else idx = 2u;
    }
    return tyvor_crsf_send_param(c, pid, idx);
}
