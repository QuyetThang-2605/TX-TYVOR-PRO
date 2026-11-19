# TYVOR MCU communication - Phase 2C timing bench specification

Ngày10/11/2026. Scope: software bench, STM32F407 CONTROL và ESP32-S3 UI, RF OFF, UART gates OFF. Không quy định pinout hoặc kết nối điện; xem `BOARD_BRINGUP_CHECKLIST.md`.

## Layers and ownership

Firmware và native integration dùng chung protocol/parser/link/Core bridge/UI endpoint và `tyvor_bench_transport` adapter. Adapter không init GPIO/open serial/flash/RF. Nó kiểm tra cap64 byte, gate, exact synchronous backend-copy result và đếm error/partial/suppressed/RX. Callback full success chỉ xác nhận backend đã consume/copy toàn frame; không chứng minh peer nhận được.

STM main owns Core/transport/bridge; ISR vẫn chỉ thu RX ring. ESP bridge owns link/cache/transport dưới mutex; diagnostics copy rồi unlock/render. Cache helpers được unit-test độc lập, nhưng firmware phải nhận Core monitor qua `tyvor_ui_endpoint`, nơi kiểm chứng read-token/RTT. Không đưa UART trực tiếp vào generic cache helper để bỏ qua bước này.

Default firmware adapter disabled bởi gate hiện hữu: STM candidate OFF, ESP pins=-1/gate OFF. Native adapter enabled chỉ với in-memory wire backend. API không có remote command để đổi gate. STM BSP TX callback hiện vẫn synchronous deadline10 ms trong nhánh candidate; adapter không giả vờ đã giải quyết DMA/WCET. Hardware gate không được bật trước review timing/pin evidence.

## Outer protocol v2 unchanged

Giữ đúng `PROTOCOL_PHASE1.md`: 115200 8N1 dự kiến, frame TY/version2/type/sequence/length/sender-ID/receiver-ID/sender-epoch/receiver-epoch/payload/CRC16 CCITT-FALSE. Header30, min32/max64 byte, payload<=32. CRC/tag/schema/direction validation không được nới lỏng. STATUS vẫn 6-byte MOCK, aircraft ARM invalid/zero.

HELLO/SESSION_ACK roles CONTROL/UI, fresh boot identities +monotonic epochs, offered-TX proof,5 attempts x200 ms/1000 ms deadline/1000 ms cooldown giữ nguyên. Active-session sequence theo type: duplicate drop, delta1..127 forward,128..255 reject, wrap255->0 accept. Transport loss không reset history; session commit mới invalidate cache/reset per-type sequence. CRC/session không authentication; fresh RNG và trusted point-to-point peer là giả định như Phase1.

## Read-only freshness lease - Core monitor revision2

Review Phase2A đã tái hiện C: giữ một batch1,3 s rồi giao nguyên frame CRC-valid có thể làm telemetry cũ hiển thị VALID, vì receiver chỉ biết age trước khi frame vào đường truyền. Revision2 dùng UI-issued read token và local elapsed time để không cần đồng bộ clock.

UI request dùng DIAGNOSTIC, chính xác7 payload byte:

| Offset | Size | Meaning |
|---|---:|---|
| 0 | 1 | C3 read marker |
| 1 | 1 | Read schema1 |
| 2 | 1 | Read snapshot opcode1; không ARM/model/output opcode |
| 3 | 4 | Nonzero uint32 token little-endian |

UI phát request không nhanh hơn200 ms; chỉ một outstanding lease. Khi response/config chưa hoàn tất, chờ đến hạn500 ms thay vì đổi token liên tục; TX offer lỗi có thể thử token mới ở kỳ tiếp theo. Token không wrap; UINT32_MAX cạn thì fail-closed đến UI reset/session identity mới. Token tăng suốt boot kể cả session rekey. Không dùng token như mật mã.

STM chỉ nhận request trong active validated v2 session. Duplicate/backward token hoặc read mới trong100 ms bị từ chối. Session thay đổi xóa peer-token history. STM freeze snapshot từ **const Core** sau khi xử lý request; echo token vào field generation. Không có đường gọi safety/model/channel mutation. Request cũ đến trễ có thể gây một bounded read response, nhưng UI không nhận nếu token/session/deadline không còn hiện hành.

UI chỉ nhận Core parts khi đã full-offer request, đúng token, đúng local/peer session tuple và elapsed<500 ms; unknown revision/expired token bị drop. Source capture chắc chắn xảy ra sau request trong implementation này. Vì vậy local request-to-arrival elapsed là upper bound của capture age; receiver dùng `max(reported capture age, local elapsed)` và cộng sensor age. Tuổi không giảm do parser recovery, queue delay, duplicate hoặc polling muộn. Đây là freshness proof trong trusted software workflow, không chống peer độc hại giả current token.

Outer frame/parser/schema CRC được kiểm tra trước callback. Endpoint chỉ tạo bản sao **derived internal age bound** của decoded frame để cache tính deadline, không sửa wire bytes/CRC hoặc chấp nhận packet CRC sai. Sensor expired bị clear; cache deadline vẫn500 ms; ACK/legacy STATUS không refresh Core telemetry.

## Core payloads

Giữ layout9-byte header C2 / revision / kind / uint32 generation / uint16 capture-age. **Revision chuyển1 ->2**, generation bây giờ là receiver-issued token. Kind1..5 giữ byte layout Model/Inputs/Outputs/Safety/Telemetry ở `PROTOCOL_CORE_V2.md`; cache atomic5 phần cùng token, partial timeout300 ms, high-water không bị xóa chỉ vì stale.

Kind6 MIXES, payload29 byte:

| Offset | Size | Meaning |
|---|---:|---|
| 0..8 | 9 | Common C2/revision2/kind6/token/capture-age header |
| 9 | 1 | Total ordered mix count1..16 |
| 10 | 1 | Page index0..ceil(count/2)-1 |
| 11 | 9 | Term index2*page |
| 20 | 9 | Term index2*page+1; unused final term all-zero |

Mỗi term: source u8, logical-bus destination u8, ADD/REPLACE u8, weight i16, offset i16, trim i16; bounds giống Model Manager. UI chỉ công bố mix configuration khi đủ các page cùng token/model snapshot; duplicate/count mismatch không overwrite dữ liệu đã nhận. MIXES hiển thị B=logical bus; OUTPUTS hiển thị channel order sau routing/safety. Không suy channel mapping từ bus label.

Publisher gửi tối đa một monitor frame mỗi10 ms: STATUS,5 Core parts, tối đa8 mix pages. Batch mặc định4 terms:8 frames, khoảng70 ms; max16 terms:14 frames, khoảng130 ms, trước backend/driver delays. STATUS heartbeat thêm ở100 ms khi không có batch. UI core commit sau5 parts; config valid sau đủ mix pages. Các con số là lịch software, chưa đo physical UART/WCET.

Legacy model-info/page0..6 UI_REQUEST và application ACK vẫn read-only/rate-limited. ACK OK không đồng nghĩa model/safety thay đổi hoặc snapshot đã tới peer. Không còn unsolicited Core revision1 publication cho UI mới. Old-v2 peers vẫn handshake/parse base STATUS, nhưng new Core UI cache INVALID nếu không có revision2/read response; không silent downgrade. Outer v1 vẫn bị reject.

## Fault model and reconnect

Native mỗi hướng có bounded32-frame event queue và independent seed/config: drop every N, fixed delay+jitter, CRC-byte corruption, duplication, chunk size1..64, inter-chunk gap, negative/partial TX, hold, disconnect/reconnect. Không heap, sleep hoặc serial device. FIFO giữ byte order mỗi hướng; fault scheduler chỉ mô phỏng, không phải mô hình điện/baud/ISR đo thật.

Queue full/TX failure trả lỗi thật; drop/corruption trên wire sau full-copy không giả backend failure. Duplicate giữ cùng sequence nên không refresh cache. Inter-byte deadline50 ms không đổi. Reset/reconnect có thể giữ old queued bytes để test session/lease rejection; `fault clear` discard queue và bỏ fault settings. Native seed0x002BF407 chạy120 bước trộn faults/reset, có final recovery check.

Required recovery matrix đã test: STM reset, ESP reset, reset đồng thời, local-ID collision, stale cache, ACK/session loss, partial TX, corrupt CRC, interbyte timeout, delayed old session, token replay/exhaustion/before-offer, long delay rejected, delay250+30 ms vẫn hoàn tất lease. Input/safety/mixer chạy độc lập với UI traffic; ESP reset không trực tiếp disarm/arm STM32. RF always OFF.

## Diagnostics and limits

FW0.2.2-timing-bench, protocol2. Shared diagnostic collector read-only: boot reason, session epochs/state/collisions, transport/link TX errors riêng (không cộng trùng), RX/CRC/timeout/duplicates/suppressed, observed reset counts, heap/stack flags, watchdog subscription/checkpoint status, last accepted STATUS age.

STM debugger global `tyvor_diagnostics` ghi raw RCC reset flags, HSI clock variable, current MSP free allowance trong4 KiB, IWDG refresh counter và successful STATUS backend-copy age. ESP console ghi `esp_reset_reason`, task-WDT status, internal heap free/min/largest, bridge stack high-water và accepted STATUS RX age. Diagnostics không feed/disable WDT, không gọi Core safety mutation; `TYVOR_DIAGNOSTICS=0` chỉ bỏ collection/logging, không bỏ safety/bench guards.

Reset counter scope: simulator observer đếm resets sau lần init trong cùng process; firmware hiện chỉ biết scope boot hiện hành (zero resets observed). Lifetime/power-cycle total UNVERIFIED, không tự tạo Flash/.noinit/retention assumption. STM không dùng application heap; native runtime heap/stack/WDT physical metrics UNVERIFIED. STATUS age là local frame/copy observation, không thay read-lease/sensor validity và không được gọi là peer delivery proof.

Không có electrical pinout, hardware PASS, persistent storage, RF command, automatic flashing hoặc verified realtime transport trong đặc tả này. Các giới hạn10 ms synchronous TX, board identity/pins/power và runtime memory cần đóng bằng hardware work được phê duyệt riêng.

## Phase 2C TX queue / timing contract

`tyvor_async_tx`: main/task owner,4 pending slots +1 immutable active frame, max64 bytes m?i slot; kh?ng heap/wait. Enqueue full result l? owned-copy proof cho existing session protocol, kh?ng ph?i physical completion/peer delivery. Decode/CRC/schema v?n ???c ki?m tra tr??c accept; outer v2/C2 rev2/read lease500 ms kh?ng ??i.

Pending STATUS ???c remove/coalesce r?i append latest, k? c? queue ??y n?u c? pending STATUS. Active buffer kh?ng bao gi? s?a. N?u ??y m? kh?ng c? STATUS thay th?, drop newest/return-1 ngay; c?c DIAGNOSTIC/control/ACK retained gi? FIFO. Kh?ng preempt active, kh?ng infinite retry/?u ti?n flood. Traffic congestion c? th? m?t read/handshake frame, retry/session/lease hi?n h?u recovery sau khi ngh?n d?ng. Kh?ng h?a session success trong saturation v? h?n.

Pending residence v? active transfer deadline ri?ng ??u20 ms; ? ??ng deadline b? drop/abort. Poll ki?m tra active deadline TR??C driver progress (ESP state c? th? ghi FIFO); timeout/clock regression kh?ng ph?t th?m suffix ?? h?t h?n. Max residence t?i active completion <40 ms trong owner service b?nh th??ng; kh?ng l? wall-time guarantee n?u CPU b? stall. Expiry ???c x? l? khi main/task ???c ch?y l?i. Timeout ISR m?t kh?ng th? t?o UART wait trong Core; main IWDG c?n b?o v? main hang.

STM TXE ISR ghi nhi?u nh?t1 byte/l?n, TC x?c nh?n last byte r?i peripheral; RX/error ???c x? l? c?ng IRQ kh?ng early-return b? TX. Main ch? start/abort d??i save/restore PRIMASK, ISR kh?ng g?i queue/Core/protocol/HAL polling. SysTick priority0, USART2 priority5, kh?ng RTOS ISR API. RX ring255 usable bytes, read max64 d??i critical section. TX active points v?o queue-owned stable buffer cho t?i TC/abort. USART l?i RX l?m parser transport-error, kh?ng s?a safety; HAL_UART_Init l?i d?ng boot fail-closed. Async start/progress errors, queue full, expiry, timeout c? counter; active byte ?? v?o shift register/FIFO c? th? ho?n t?t sau abort, parser/session/lease x? l? truncation/old data.

ESP bridge priority5, delay10 ms, RX2048 bytes/event16, drain max256 bytes/iteration. Event queue observed full ???c coi nh? possible lost error event: parser reset/input flush/event reset. UART callbacks ch? enqueue; driver progress `uart_tx_chars` gi? offset suffix khi FIFO nh?n m?t ph?n. SDK internally c? tx/rx mutex portMAX_DELAY, nh?ng ch? bridge g?i UART1 driver; UI kh?ng th? s? h?u mutex ??. Init/flush ??c SDK code: flush disable RX interrupts tr??c drain buffer h?u h?n; kh?ng c? application TX FIFO wait ho?c uart_wait_tx_done. RTOS/SDK corruption, CPU starvation v? spinlock latency ch?a c? hardware proof.

Bridge publish fixed-size immutable projection qua `xQueueOverwrite` length1; UI `xQueuePeek(...,0)` r?i poll freshness/render/log b?n copy. Slow UI ch? b? qua intermediate publications; kh?ng gi? endpoint/cache/transport lock v? kh?ng g?i UART ho?c ARM/output writer. FreeRTOS internal copy/spinlock v?n c?n ?o khi ch?y board. Dynamic queue allocation fail ?abort; static footprint kh?ng gi? l? heap measurement.

STM `board_timing`: DWT CYCCNT availability check, max/last cycles v? sample/budget counters cho INPUT/CORE/UART/LOOP, release-lateness counters. Core nominal10 ms; proposed Core budget1 ms, full-loop budget10 ms, kh?ng hard-real-time guarantee. ISR time n?m trong elapsed cycles c?a main regions; WFI kh?ng n?m trong work time. Single-wrap subtraction ch? h?p l? cho interval <2^32 cycles (~268.4 s ?16 MHz); debugger halt/long faults kh?ng d?ng l?m WCET evidence. ESP ghi max work-before-publication (microseconds, kh?ng g?m publication/WDT/delay) v? max bridge start-gap. T?t c? hardware timing hi?n UNVERIFIED; native timing tests ch? clock arithmetic/scheduling/fault behavior.
