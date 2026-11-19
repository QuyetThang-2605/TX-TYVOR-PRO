# TYVOR UART protocol v2 - Phase 1 reliability contract

This contract supersedes the historical Phase 1 v1 wire format. Both STM32 and ESP32 use the same `shared/tyvor_protocol.c` and `shared/tyvor_link.c`. Version 1 is rejected; there is no mixed-version fallback or claimed compatibility with TYVOR STABLE.

## Wire format and validation

115200 baud, 8N1, no flow control. Electrical levels and routing still require actual board verification. UART gates remain OFF in both default builds; this contract implies no GPIO mapping.

| Offset | Size | Meaning |
|---|---|---|
| 0 | 2 | Sync `54 59` (`TY`) |
| 2 | 1 | Major wire version `02` |
| 3 | 1 | Allowlisted type |
| 4 | 1 | Sequence modulo 256; handshake messages use zero |
| 5 | 1 | Payload length 0..32 |
| 6 | 8 | Sender boot identity, nonzero uint64 little-endian |
| 14 | 8 | Receiver boot identity, uint64 little-endian |
| 22 | 4 | Sender session epoch, nonzero uint32 little-endian |
| 26 | 4 | Receiver session epoch, uint32 little-endian |
| 30 | length | Payload |
| 30+length | 2 | CRC low byte then high byte |

Frames are 32..64 bytes; STATUS is 38 bytes and HELLO/SESSION_ACK are 34. Only broadcast HELLO permits receiver identity/epoch both zero. All other tags require nonzero identities/epochs. Half-broadcast tags are rejected.

CRC16 CCITT-FALSE remains polynomial `0x1021`, initial `0xFFFF`, refin/refout=false, xorout=0. CRC covers version through payload, including all session fields. Sync is checked separately. `123456789` yields `0x29B1`. Golden STATUS in `sim/test_protocol.c` uses sender=1, receiver=2, epochs=1/1, sequence=7, payload `00 00 A4 1F 63 07`; CRC is `0x21A9` (wire `A9 21`), independently checked with Python `binascii.crc_hqx`.

| Type | Payload | Allowed data direction |
|---|---|---|
| 1 STATUS | Exactly 6 bytes | CONTROL -> UI |
| 2 UI_REQUEST | Exactly 2 bytes | UI -> CONTROL |
| 3 ACK (application) | Exactly 2 bytes | CONTROL -> UI |
| 4 DIAGNOSTIC | 0..32 opaque bytes | Either; never executable |
| 5 HELLO | `[role,minor_version=0]` | Either, opposite-role peer required |
| 6 SESSION_ACK | `[role,minor_version=0]` | Either, opposite-role peer required |

Roles are CONTROL=1 and UI=2. Unknown major/minor, type, length, tag, CRC or payload semantics fail closed. Decode failure does not modify output. UI_REQUEST remains `[1,0]` model info or `[2,page]`, page 0..6. Application ACK is `[opcode,result]`, opcode 1..2, result 0..3. STM32 only returns unsupported=1, at most once per 100 ms. No write/ARM/RF/output opcode exists.

STATUS remains `arm_valid, arm, battery_mv_le16, lq, flags`. Boolean fields are 0/1; invalid ARM requires arm=0 and renders UNKNOWN. Flag 0 marks battery valid (then 1..65535 mV; otherwise zero); flag 1 marks LQ valid (0..100%; otherwise zero); flag 2 marks MOCK (requires ARM invalid/zero). Other flags are rejected. No chemistry, cell count or source LQ conversion is inferred. CRC/schema checks were not relaxed for recovery tests.

## Frame ownership and timeout

The parser owns framing only; the link owns session/sequence acceptance. Parser storage is fixed: 64 raw bytes and 64 uint64 arrival timestamps. It appends bounded bytes, validates header, then CRC/tag/payload. A valid outer frame owns its entire payload, even if payload resembles a valid nested frame.

Each possible frame start retains its original local arrival time. On CRC/schema failure or an incomplete candidate reaching its absolute 50 ms deadline, discard one byte and rescan the retained suffix with its own arrival timestamps. The old candidate cannot expire a fresh suffix. New bytes do not extend a candidate's deadline. All split points around 49/50/51 ms are tested.

A complete valid suffix recovered during a later poll is accepted only if its first-to-last byte span was strictly less than 50 ms; its callback timestamp is the actual last-byte arrival time. Delayed polling cannot turn old STATUS into fresh telemetry. Incomplete suffixes remain buffered only until their own deadline. Feed and poll both drain; callers must poll during silence. Clock regression invalidates the active link and starts negotiation with a fresh epoch on the next poll.

A plausible incomplete header may delay a suffix until completion or timeout. A CRC-valid outer frame containing a full inner frame is inherently ambiguous; the outer ownership rule is deliberate. Transport loss/overflow resets damaged parser bytes, retains session/sequence history, and requires the owner to discard the damaged UART input.

## Session identity and negotiation

Each boot obtains a fresh nonzero 64-bit hardware-random identity before normal service starts. STM32 uses HAL RNG with an internal-HSI PLL48 source; SYSCLK stays HSI 16 MHz. ESP briefly enables SDK internal entropy before ADC/Wi-Fi/BT/application tasks, fills its identity, then disables that source. Both have bounded nonzero attempts and no fixed fallback. RNG failure prevents link startup. This is source/build behavior, not measured hardware entropy quality.

A boot issues monotonically increasing nonzero 32-bit local epochs. Epoch exhaustion fails closed instead of wrapping. A session is the exact ordered tuple of both boot identities and both epochs. A smaller sequence never creates or changes a session.

1. Each side creates a pending epoch and sends HELLO, initially with broadcast destination if no peer is known.
2. Receiving an opposite-role HELLO binds a pending candidate identity/peer epoch and queues SESSION_ACK echoing that peer's identity/epoch alongside our pending identity/epoch. A concurrent candidate with a different identity is rejected, except a fresh boot may replace a liveness probe bound to the previous active peer; replacement issues a fresh local epoch.
3. A receiver commits only on SESSION_ACK addressed to its boot identity and current pending epoch, from the bound candidate (if bound), not a retired lower peer epoch, and arriving within the pending deadline. Its pending epoch must already have been fully offered through a successful HELLO or SESSION_ACK TX callback. A queued, failed or partial TX is insufficient proof.
4. Commit resets per-type TX/RX sequences, invalidates the entire old STATUS snapshot, stores the exact tuple, and queues a confirming ACK. No STATUS is valid until an accepted new-session STATUS arrives. The peer may commit later if an ACK was lost; repeated HELLO elicits an idempotent active ACK.
5. Duplicate active HELLO/ACK does not reset sequence/cache or freshness. Lower retired peer epochs, mismatched echoed epochs, old data tags, wrong roles and a peer identity equal to our own are rejected/counted.

One pending attempt sends at most five HELLOs, one per 200 ms (nominal t=0,200,400,600,800); deadline is 1000 ms. Poll never bursts to catch up. A failed attempt enters 1000 ms cooldown before another bounded discovery burst. SESSION_ACK output is rate-limited to one per 100 ms with fixed pending/active slots. Main loops must call poll; these are algorithmic limits, not wall-clock scheduling guarantees.

Existing active data may continue during an unproven replacement probe; a mere unknown HELLO cannot replace the active tuple. Commit invalidates old data immediately. Generic peer silence for 2000 ms, or UI STATUS silence for 2000 ms even while application ACKs continue, initiates a fresh handshake. A loss of >=128 STATUS frames therefore recovers through proven rekey, not by clearing sequence history on stale. Negotiation failure cannot revive an invalid snapshot.

Delayed old packets cannot match a newly issued epoch. Delayed unknown HELLO may trigger a bounded probe, but an old ACK cannot echo that probe's fresh local epoch. Same-ID and pending-candidate collisions are tested to reject. Exact reuse of an entire boot/session tuple is indistinguishable on the wire: fresh entropy is an explicit assumption, not an absolute collision guarantee.

## Sequence, freshness and TX

For each data type in the active session, first frame establishes the baseline; delta=`(new-last) mod 256`: 0 duplicate/drop; 1 accept including 255->0; 2..127 accept/count missing delta-1; 128..255 reject as backward/ambiguous. Wrong-session/direction/schema/CRC frames cannot change the baseline or snapshot. Parser frame counters count syntactically valid frames; link/session counters record subsequent rejection.

Snapshot uses separate received_once/valid/timestamp. At age >=2000 ms or clock regression it becomes INVALID with every STATUS field/flag cleared. Timestamp zero is valid. ACK/DIAGNOSTIC/duplicate packets never refresh STATUS. Session change invalidates immediately even before the age deadline. A new valid STATUS may have its own source validity bits unset; session readiness is not telemetry validity.

TX callback synchronously consumes/copies the full byte array, retains no stack pointer, and returns exact length or error. Negative/partial results are counted and reported; data is dropped without unlimited retry, with the next attempt using a new sequence. Handshake retries obey the bounds above; `offered` is set only after full TX. STM32 HAL deadline is 10 ms: a maximum 64-byte frame takes about 5.56 ms at 115200 8N1. ESP FIFO copy is nonblocking; a partial copy is a TX error. No reliable application transaction completion is claimed.

## Ownership and limits

No parser/link reentry or concurrent mutation is permitted. Callback frame is borrowed until return. STM32 ISR owns only RX-ring production/error flags; main drains in a bounded critical section and owns link/TX. TX and ISR RX diagnostics use separate counters. ESP bridge owns feed/poll/TX/cache; diagnostics copies snapshot/stats/epochs under the same mutex and renders a local copy after unlock. No ISR or UI task mutates session/cache. Future UI/driver additions must preserve this ownership and reassess latency.

All counters are uint32 modulo counters, not persistent/saturating. Encode source/output must not overlap. NULL input is allowed only for empty diagnostic; CRC(NULL,0)=0xFFFF and CRC(NULL,n>0)=0 is defensive behavior, not a validation API.

This is a trusted point-to-point bench protocol with accidental-corruption detection and session replay separation. It is not cryptographic authentication or protection against a peer forging current tags. STM32 remains the sole future authority for control/RF/outputs. No physical link, entropy, PSRAM, panel or timing verification is claimed by native handshake tests.
