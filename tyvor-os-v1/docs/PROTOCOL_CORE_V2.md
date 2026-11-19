# Read-only Core monitor extension inside protocol v2

The outer framing, session negotiation, CRC, sequence rules, roles and STATUS schema remain byte-for-byte unchanged from `PROTOCOL_PHASE1.md`. This extension uses existing type 4 DIAGNOSTIC with a maximum payload of 32 bytes. No new control opcode is introduced. STM32 ignores DIAGNOSTIC on its dispatcher; UI accepts the extension only after the common link has accepted the active session and sequence.

All multibyte fields below are little-endian. Signed values are two's complement, normalized axis/channel units are -1000..1000. Marker `C2`, extension revision 1 and the bench view contract identify **software bench data**, not flight telemetry or aircraft ARM.

## Common payload header

| Offset | Bytes | Meaning |
|---|---:|---|
| 0 | 1 | `C2` marker |
| 1 | 1 | Extension revision `01` |
| 2 | 1 | Part kind 1..5 |
| 3 | 4 | Frozen snapshot generation, uint32 modulo |
| 7 | 2 | Capture age in ms when serialized; >=500 rejected |
| 9 | variable | Part body |

Generation ordering accepts forward deltas 1..0x7FFFFFFF, including UINT32_MAX->0. Duplicate parts cannot overwrite an already accepted part or refresh its deadline. Older/ambiguous generations are dropped; a timed-out or committed generation cannot be resumed/replayed. History resets only with a new proven outer session. Missing parts do not create a partially valid model/input/output display.

## Parts

| Kind | Total payload length | Body, in order |
|---|---:|---|
| 1 MODEL | 32 | model_id u32; revision u32; canonical name[15] (1..14 printable ASCII, null plus zero padding) |
| 2 INPUTS | 22 | valid u8; sample age at capture u16; four axes i16; ARM/mode positions i8[2] |
| 3 OUTPUTS | 27 | valid u8; rf_enabled u8 (must be zero); eight monitor channels i16 |
| 4 SAFETY | 17 | state u8; reasons u16; throttle_lock u8; tick_count u32 |
| 5 TELEMETRY | 32 | valid mask u8; battery mV u16; LQ u8; RSSI dBm i16; latitude E7 i32; longitude E7 i32; satellites u8; four source ages at capture u16[4] |

INPUTS valid is 0/1; invalid axes/switches must be zero. ARM position is 0/1, mode is 0/1/2. A valid source input age must be <1000 ms (maximum model input timeout); receiver input monitor expiration is 500 ms including all source/capture age.

OUTPUTS valid is 0/1. Invalid output values can still contain the known bench failsafe vector, but renderer displays UNKNOWN when invalid. `rf_enabled=1`, out-of-range channels or invalid flags are rejected.

SAFETY state values: 0 STARTUP, 1 DISARMED, 2 BENCH_ARMED, 3 FAILSAFE. Reasons: bit0 startup, bit1 throttle, bit2 input, bit3 model, bit4 scheduler, bit5 clock, bit6 switch/kill. Other bits rejected. BENCH_ARMED requires reasons=0 and lock=0; every other state requires lock=1. This does not set legacy STATUS.arm_valid.

TELEMETRY mask bits 0 battery, 1 LQ, 2 RSSI, 3 GPS; unknown bits rejected. Valid field bounds/TTLs match `PHASE2A_ARCHITECTURE.md`. Invalid fields are zero, with age=65535. Valid source ages must be less than their TTLs. GPS validity covers lat/lon/satellites as one field. Capture age + sensor age + local waiting time counts toward expiration; later parsing cannot rejuvenate a sensor.

## Publication, commit and compatibility

Publisher freezes Core once per 120 ms cycle and sends legacy STATUS then the five parts at 20 ms intervals. Base STATUS is MOCK with ARM invalid/zero; battery/LQ follow their individual validity bits. Caches continue to have the outer link's 2 s STATUS validity, but the Core monitor cache has a stricter 500 ms deadline and never uses ACK/other DIAGNOSTIC traffic as freshness.

The UI stages parts for at most 300 ms, keeping the earliest inferred source expiry. A complete generation commits atomically, then independently expired input/telemetry fields are cleared. A new local/peer boot/epoch invalidates both pending and committed state. Callback timestamps are last-byte arrival time; a recovered old frame is evaluated against the latest owner poll time before becoming visible.

An old protocol-v2 UI can still decode base STATUS and ignore opaque DIAGNOSTIC. A new UI paired with a Phase 1 v2 publisher has a ready link but INVALID Core cache because no Core parts arrive. Unknown extension revisions are rejected without fallback; outer v1 remains rejected by the existing link. No capability or write-command negotiation is implied by a successful v2 session handshake.
