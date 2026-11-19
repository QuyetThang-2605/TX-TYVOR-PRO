# TYVOR Core Phase 2A architecture

This is an original software bench implementation organized by the Inputs, Mixes, Outputs, Models, Safety and Telemetry feature groups referenced in `TX15_REFERENCE.md`. No EdgeTX/STABLE source, pin map, RF implementation or safety certification is imported. STM32 owns the Core; ESP32 owns presentation. All numbers below describe software policy, not measured board behavior.

## Ownership and module boundaries

| Module | Implementation | Owner and contract |
|---|---|---|
| Input Manager | `stm32/engine/tyvor_inputs.*` | Main owner accepts timestamped/sequence-numbered mock ADC samples; four axes and two switches; no UI writer |
| Mixer Engine | `tyvor_mixer.*` | Pure calculation from six logical inputs to eight logical monitor channels; no I/O callbacks |
| Model Manager | `tyvor_models.*`, `tyvor_model_store.*` | Validated active model, canonical versioned storage, two-slot RAM storage adapter; active selection guarded by Core |
| Safety Engine | `tyvor_safety.*` | Local input/model/scheduler policy; logical bench ARM only; selects failsafe monitor values |
| Telemetry Manager | `tyvor_telemetry.*` | Four independent sensor validity/timestamps/expirations; mock source only |
| Core / scheduler | `tyvor_core.*` | Single owner; fixed 10 ms schedule, no heap and no RF/actuator API |
| UI Bridge | `tyvor_core_bridge.*` + existing bridge/link | Holds `const tyvor_core_t *`; frozen read-only snapshots over protocol v2; cannot call selection/safety mutation |
| UI state/cache | `shared/tyvor_ui_state.*`, `tyvor_ui_endpoint.*` | Accepted session-tagged data only; coherent generation commit; no STM32 engine linked into ESP |
| Bench sources / simulator | `tyvor_bench.*`, `sim/tyvor_sim_runtime.*` | Explicit software input/telemetry providers; deterministic fake clock and UART queues |

`stm32/engine` is portable C11 without HAL/RTOS dependencies. `stm32/Core/Src/main.c` composes it with the existing gated BSP. ESP links shared wire/cache/view code, not the engine. The `engine` name also avoids creating a second directory that differs from existing `Core` only by case on Windows.

## Input and mixer units

Raw mock axes are 12-bit values 0..4095. Per-axis calibration specifies min/center/max with at least 16 raw counts on either side, deadband 0..200 and filter divisor 1..16. Calibration clamps to endpoints and maps each half independently to -1000..1000. Deadband is applied in normalized units, then the remaining range is rescaled to retain full endpoints. Integer IIR is `filtered += (immediate-filtered)/divisor`, once per accepted sample; first/recovered samples seed the filter. Truncation is toward zero.

Samples require all four axes and both switches valid. Switch 0 has positions 0/1 (local ARM intention); switch 1 has positions 0/1/2 (mode, with 2 as bench kill). Invalid shape/range/future/stale sample invalidates input immediately. Duplicate/backward sequence/timestamp cannot refresh freshness. Input sequence is uint32 with half-range ordering and wrap; defaults expire at age >=100 ms, configurable 20..1000 ms. Expiration clears normalized values and flags; the safety owner observes that on its next tick.

Logical sources 0..3 are filtered axes; source 4 is the two-position switch mapped to -1000/+1000; source 5 is the three-position switch mapped to -1000/0/+1000. Up to 16 ordered terms each calculate `source * weight / 100 + offset + trim`, with weight -200..200%, offset -1000..1000 and trim -500..500. ADD and REPLACE execute in array order. Accumulators are int64; clamping to -1000..1000 happens after the final term. A unique permutation maps eight logical buses to eight monitor channels.

Logical bus 2 is the bench throttle bus; its mapped failsafe endpoint must be -1000. This is a bench model invariant, not a complete production throttle/mixer/RF safety policy. Arbitrary valid mix terms can still produce non-airworthy configurations. There is no channel-to-PWM/CRSF/RF adapter.

## Safety state machine

States: STARTUP, DISARMED, BENCH_ARMED, FAILSAFE. BENCH_ARMED is a local software state, never aircraft ARM. The legacy STATUS always keeps aircraft ARM invalid/zero and the MOCK flag.

- Startup and reset enter STARTUP with throttle lock and configured failsafe monitor values. No ARM intent is persisted.
- STARTUP/FAILSAFE can become DISARMED only with valid input/model, ARM switch OFF, kill released and both immediate and filtered throttle <=-950. This prevents a slow filter from hiding an unsafe throttle sample.
- DISARMED can become BENCH_ARMED only on a new OFF->ON edge after observing OFF and safe throttle. A failed high-throttle ON attempt consumes that edge; lowering throttle while holding ON does not arm.
- ARM switch OFF disarms. Input invalid/expired, invalid model, kill, scheduler or clock fault forces FAILSAFE and failsafe monitor values. Input/kill recovery with ARM held ON cannot rearm.
- Scheduler/clock faults are latched until Core reset. A corrupt storage restore in the simulator resets the old Core first, then latches MODEL/FAILSAFE; it cannot retain the previous armed instance.
- Model selection/load/save through Core requires DISARMED, fresh valid input, safe throttle, ARM OFF and no latched fault. Selection resets input filter/history and safety to STARTUP, immediately applying the new model's failsafe values. Only local bench APIs expose selection.

Configured failsafe values are used when the model validates. An invalid model uses the fixed bench fallback (zeros, channel 3=-1000); no hardware safety inference is made from that fallback.

## Scheduler and transport timing

Core deadlines are anchored at initialization +10 ms, then exact multiples of 10 ms. A poll runs at most one due tick. Lateness of a full period or more counts missed slots, preserves the next anchored deadline and latches SCHEDULER/FAILSAFE; it never executes an unlimited catch-up loop. A regressing clock or deadline overflow latches CLOCK/FAILSAFE immediately. Tick order is input expiration -> telemetry expiration -> logical inputs -> mixer -> safety -> gated monitor values.

Firmware mock input is supplied every approximately 10 ms, telemetry every 100 ms; Core polling occurs before bounded UART work in main. The native simulator uses a fake 1 ms clock and feeds samples with reproducible cadence. Determinism means identical accepted samples/model/clock events produce the specified state/tick order; it is not a wall-clock/WCET guarantee on the MCU.

Bridge sends at most one monitor frame per 20 ms: legacy STATUS then five frozen Core parts, for a 120 ms cycle. Full cycle is 328 UART bytes, theoretically about 23.7% of a 115200 8N1 line before handshakes/read ACKs. The current BSP can block up to 10 ms per TX, and handshake/read ACK work adds bounded calls. **A verified hardware transport needs DMA/queue scheduling and measured deadlines before any real-time control claim.** UART gates are OFF in these artifacts.

## Model storage

Model schema 1 serializes explicit little-endian bytes: `TYMD`, version uint16, exact total length uint16, ID/revision uint32 each, zero-padded printable ASCII name[15], four calibration records (9 bytes each), input timeout uint16, switch positions[2], throttle-axis byte, mix-count byte, ordered terms (9 bytes each), channel map[8], failsafe int16[8], CRC32/IEEE uint32. Name is 1..14 printable characters with canonical zero padding. Exact size is `101+9*mix_count` (110..245 bytes); buffer cap is 256. CRC includes magic/version/length/body; `123456789` gives `CBF43926`.

Decode checks length/count/CRC/version and validates a temporary model before replacing the caller's output. Unknown versions are rejected; no implicit migration exists. Persistence callbacks must implement atomic replace or preserve the old slot on failure. The RAM mock obeys this contract; corruption is rejected on read. Simulator storage survives reset of the Core object within that process. It does not survive process/power loss. STM32 startup seeds mock defaults; there is no Flash erase/program or real nonvolatile persistence driver.

## Telemetry and UI

Battery mV, LQ percent, RSSI dBm (-150..0) and GPS lat/lon E7 (+/-90/+/-180 degrees, 0..64 satellites) have independent timestamps. Battery/LQ/RSSI expire at 1000 ms, GPS at 2000 ms. Partial updates change only present fields. Invalid/future/duplicate/older values cannot refresh state. Expired fields are cleared and marked INVALID, retaining timestamp history for ordering.

Core snapshots use the read-only DIAGNOSTIC sub-schema documented in `PROTOCOL_CORE_V2.md`. UI commits only all five parts of one generation. Partial timeout is 300 ms; cache expires at 500 ms of inferred source age. Generation high-water survives stale invalidation, preventing replay from reviving it; session change clears cache/partial/history immediately. Local input monitor TTL is 500 ms and independent telemetry TTLs also include source and capture age. A stale view is UNKNOWN rather than a zero-valued live reading.

ESP bridge is the sole owner of endpoint/link/cache mutations. Diagnostics copies the state/counters under one mutex, then polls freshness and builds text outside the lock. Seven renderer-independent views are HOME, MODEL, INPUTS, OUTPUTS, SAFETY, TELEMETRY and CONNECTION. Console output is the current renderer; no panel/touch/LVGL driver exists.

UI request opcodes remain model-info or read page 0..6. ACK OK means the periodic read-only snapshot is queued; it does not mean a model or safety command was committed. No configuration/write/ARM/output opcode or UI-to-Core mutable pointer exists. Future write transactions need separately versioned requests, correlation/validation/physical policy on STM32 and a new review.
