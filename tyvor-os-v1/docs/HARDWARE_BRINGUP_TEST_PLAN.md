# TYVOR Phase 2D — Hardware bring-up test plan

Ngày 10/11/2026, Asia/Bangkok. **Kế hoạch để review; không phải lệnh thực hiện hardware trong Phase2D.** Trạng thái hiện tại: **0/10 bước hardware đã chạy; cả10 bước NOT RUN/BLOCKED**. Không ghi PASS hoặc FAIL điện khi chưa có phép đo.

Giữ nguyên: không flash/erase, không nối hai board, không tự cấp nguồn ngoài hoặc cắm USB để thử, không mở UART gates, không RF/drone/actuator, không đổi STABLE, option bytes/eFuse/Git global, commit/push/deploy. USB cũng cấp nguồn; “chỉ cắm USB” không được miễn power gate. Không có pin vật lý/điện áp cấp nào được chốt trong tài liệu này.

## 1. Trạng thái và safety gates

`PASS` chỉ khi đủ toàn bộ tiêu chí và evidence; `FAIL` khi đã thực hiện và có sai lệch đo được; `BLOCKED` khi thiếu điều kiện/ngưỡng/ủy quyền; `NOT RUN` là chưa chạy. Không thay BLOCKED bằng PASS vì tài liệu/compile/native tests thành công. Sau STOP không tự thử chân/nguồn/boot mode khác, không unprotect/erase/flash để vượt lỗi.

| Gate | Điều kiện cần trước khi mở trong công việc hardware riêng | Phase2D |
|---|---|---|
| G0 — hồ sơ | Người dùng review ba deliverable; source/artifact identity ghi rõ | Chờ review |
| G1 — actual board | Ảnh hai mặt/markings/revision; schematic phù hợp hoặc net evidence được review; actual pin matrix; visual/continuity record | CLOSED |
| G2 — power/boot/debug | Power path, input/current/rail limits, test points, USB/probe backfeed, boot/reset plan và exact thao tác được cho phép | CLOSED |
| G3 — trusted bench identity | Biết firmware/ROM nào sẽ chạy, RF policy; artifact hash/readback hoặc trusted identity evidence; không tự chạy firmware nhà bán chưa biết | CLOSED |
| G4 — single-board UART fixture | T01–T07 phù hợp đã PASS; UART actual pins/logic verified; loopback diagnostic fixture đã review và được cho phép riêng | CLOSED |
| G5 — two-board integration | T08 từng board PASS; approved interconnect/power/common-reference drawing; exact test firmware/UART changes được review | CLOSED |
| RF | Ngoài scope; không có điều kiện mở trong plan này | CLOSED |

Không có bước “flash cho tiện” giữa các gate. Nếu board chưa có firmware bench phù hợp, dừng và mở một nhiệm vụ firmware/flash riêng sau review; Phase2D không triển khai/nạp diagnostic fixture. Normal boot từ Flash không được coi mặc nhiên RF OFF nếu firmware hiện có chưa biết nguồn gốc. Boot-ROM/hold-reset route cũng phải được xác minh đúng board trước khi dùng.

Các số T01–T10 bám danh sách yêu cầu, không cho phép chạy mù theo thứ tự. T06 offline identity được làm trước power; T06 device identity/readback phải có trước phần TYVOR-runtime của T05/T07. T04 USB/ROM enumeration độc lập với việc app TYVOR có đang chạy.

## 2. Hồ sơ đo và ngưỡng phải điền trước thao tác

Mỗi run có: run ID/timezone/operator/reviewer, board ID/revision/photo hashes, schematic revision/hash, actual pin-matrix version, firmware/build/config/ELF/BIN hashes, tool/probe/cable/instrument model/calibration/resolution, approved drawing, boot state, min/max/units, raw logs/waveform path +SHA256, PASS/FAIL/BLOCKED và lý do STOP. Không chỉ lưu ảnh màn hình một giá trị ổn định.

| Parameter | Giá trị hiện tại | Cách chốt trước test |
|---|---|---|
| Exact USB connector/power source/protection | TBD từng board | Đối chiếu PCB/power schematic; một nguồn được duyệt mỗi board, không giả định USB/probe cách ly |
| `VUSB_min/max`, `I_trip`, `I_inrush`, `I_steady`, observation interval | TBD | USB path, regulator/protection/component ratings và power budget được review; không lấy giá trị MCU VDD làm rating USB |
| Actual3V3/GND test points, `V3V3_min/max`, ripple/overshoot max, temperature stop limit | TBD | Intersection regulator tolerance/transient spec, load và silicon operating conditions; instrument bandwidth/probe connection được duyệt |
| MCU I/O domain, allowed VOH/VOL/VIH/VIL, baud-error budget | TBD | Actual rail +chip datasheet +USART clocks +measured idle/data levels, không suy “3.3V TTL” từ tên board |
| USB/debug/boot mode, HSE/bootloader version, reset behavior | TBD | Exact schematic/markings và ST/Espressif docs; không chọn strap/header từ board khác |
| UART pins/fixture/ground/backfeed protection | TBD | Actual matrix đã VERIFIED; gate/change review riêng |
| STM proposed Core/loop/deadline targets | Core work<1 ms; loop work<10 ms; release→logical-output completion<10 ms | Source targets Phase2C; cần review như acceptance criteria trước đo, không là kết quả đã đo |
| Healthy bench recovery acceptance | Đề xuất coherent Core+mix cache ≤5 s sau khi fault đã bỏ và nguồn/peer ổn định | Acceptance target để review, không phải protocol/hardware guarantee; ghi rõ fault-clear timestamp và task scheduling |
| Soak/campaign | Đề xuất10 phút normal+saturation load; reset10 lần mỗi loại;1000 complete frames/chiều trong loopback | Chốt duration/load trước test; không sửa sau khi thấy failure để lấy PASS |
| Stack/heap minimum headroom và watchdog interval tolerance | TBD | Reviewed task/main call-chain/ISR budget, allocator load, LSI/IWDG spec; watermark hiện có chưa là proof |

TBD của một bước chưa được điền → **BLOCKED**, chưa có PASS threshold hợp lệ. Silicon reference: STM VDD1.8–3.6V (DS8626 Table14), S3 main3V3 domains3.0/3.3/3.6V min/typ/max (S3 datasheet Table5-2). Đây không phải điện áp được phép cấp vào board hoặc dải đủ để PASS toàn bộ power test. Xem [pin matrix](BOARD_PIN_MATRIX.md) Sources S1–S8.

## T01 — Visual inspection / unpowered evidence

**Điều kiện vào:** G0; có ảnh actual board rõ, không dùng ảnh seller làm ảnh board đã nhận. Nếu cần đo continuity trong tương lai phải có phạm vi đo được cho phép, board hoàn toàn không được cấp nguồn và đã kiểm tra không có nguồn/battery/USB/debugger còn nối.

**Nội dung dự kiến:** gán ID riêng STM-A/ESP-A; ảnh mặt linh kiện, mặt sau, orientation/pin1, silkscreen/revision, MCU/Flash/LDO/USB-bridge markings, buttons/straps/bypass links, connectors. Đối chiếu S1 IC44303/S2 product1064270597 với order/variant và S3 đúng B; lập actual header inventory. Continuity/resistance record cho các net cần dùng chỉ sau khi có approved test points; không coi continuity đơn thuần là voltage verification.

**PASS:** identity/revision/net evidence đủ cho phần hardware định kiểm tra; ảnh đọc được, không thấy hư hỏng/short/rework bất thường; continuity map khớp schematic được review. Không có schematic exact thì cần net tracing/ảnh/measurement evidence tương đương được người có trách nhiệm chấp nhận; không tự nâng trạng thái từ tương tự ngoại hình.

**FAIL / STOP:** marking/variant/layout khác reference, damage/short, connector orientation không rõ hoặc resistance nghi vấn. Thiếu ảnh/schematic →BLOCKED. Không sang power khi regulator/power path còn chưa biết.

**Evidence:** `T01-photos-front-back`, component-marking table, matched/changed-net list, unpowered meter raw readings, annotated actual matrix, reviewer/date. **Hiện tại: NOT RUN/BLOCKED — chưa có ảnh actual PCB.**

## T02 — USB power verification, từng board riêng

**Điều kiện vào:** T01 và G2 PASS/được duyệt; connector/cable/USB source/current protection, input limits và boot state đã xác định; board kia và mọi peripheral chưa nối. Kiểm tra cả power output của debugger/USB-UART để tránh thêm nguồn thứ hai. Unknown factory firmware chưa được phép normal-boot.

**Nội dung dự kiến:** trong hardware job được cho phép, dùng duy nhất USB path đã duyệt; quan sát input voltage/current/inrush/reset và nhiệt theo thời gian. Dụng cụ đo không tự thêm ground/power path chưa review; không nối hai USB của cùng WeAct chỉ để “thử cổng”.

**PASS:** input/inrush/steady current trong giới hạn đã ký, không current-limit trip/abnormal heating/reset oscillation, nguồn không bị backfeed qua cổng khác theo plan.

**FAIL / STOP:** vượt giới hạn, nóng bất thường, rail collapse, nguồn dòng ngược hoặc reset lặp. Dừng cấp nguồn theo quy trình đã duyệt; không chuyển sang nguồn ngoài/đổi USB/header để thử. Thiếu ngưỡng/boot-control evidence →BLOCKED.

**Evidence:** USB connector photo, cable/source identity, current/voltage trace với timestamps, configured trip limits và nhiệt/observations. **Hiện tại: NOT RUN/BLOCKED.**

## T03 — Đo rail3V3

**Điều kiện vào:** T02 PASS; actual3V3/GND test points và meter/scope ground plan đã được xác minh. Không dò chân nguồn đang hoạt động dựa trên reference image.

**Nội dung dự kiến:** ghi startup ramp, steady min/max và ripple/overshoot ở idle và load được duyệt; đối chiếu LDO marking/ratings, MCU domain và brownout/reset logs. Chưa thay regulator/jumper/rail source.

**PASS:** toàn bộ trace nằm trong reviewed rail/ripple/thermal limits, không brownout, testpoint đúng net; đủ measurement uncertainty/bandwidth record.

**FAIL / STOP:** rail vượt giới hạn, unstable/oscillating/droop, ground offset, hot part hoặc testpoint không chắc. DMM3.3V đơn lẻ không đủ PASS transient/ripple. Không tự thêm supply hoặc đổi strap để chữa.

**Evidence:** testpoint annotated photo, meter min/max/raw export, scope captures/probe settings, LDO datasheet reference và current/reset correlation. **Hiện tại: NOT RUN/BLOCKED.**

## T04 — SWD / USB enumeration

**Điều kiện vào:** T02/T03 PASS; exact debug/USB mapping và target-reference voltage/boot mode đã duyệt; tool không tự erase/program/unprotect. DTR/RTS/reset behavior phải được ghi trước khi mở bất kỳ host port nào.

**Nội dung dự kiến:** STM: phân biệt host thấy SWD probe với probe đọc được target identity. USB Type-C không chứng minh USB-UART; source TYVOR STM hiện không có USB device application stack. Chỉ đánh giá ROM DFU trong boot mode/clock/bootloader version đúng ST AN2606 mục34; ROM DFU cần HSE theo tài liệu, trong khi app TYVOR dùng HSI. ESP: phân biệt USB-UART U7 với native USB endpoint của MCU; một USB bridge enumerate không chứng minh ESP app/PSRAM đã chạy.

**PASS:** path đã chọn enumerate/identify đúng device/interface/boot target và ổn định theo plan; SWD đọc đúng supported MCU identity, không có writes. Path chưa implement được ghi “not applicable cho app này” có nguồn; không gán hardware USB PASS cho path đó.

**FAIL / STOP:** device/voltage/interface sai, target read lỗi/reconnect lặp, unexpected reset/security state. Thiếu HSE/boot evidence cho DFU →BLOCKED hoặc N/A theo path được chọn, không gán lỗi cho Core app. Không bỏ read protection/erase để làm tool connect.

**Evidence:** host hardware IDs/interface descriptors, probe firmware/driver version, target ID/raw read log, boot mode/HSE evidence, reset trace và tool options. **Hiện tại: NOT RUN/BLOCKED.**

## T05 — Boot diagnostics

**Điều kiện vào:** T04 và G3; T06 offline identity trước power, T06 device identity trước khi kết luận TYVOR boot. Nếu chỉ boot ROM, kết quả chỉ thuộc ROM observation; TYVOR-runtime vẫn BLOCKED.

**Nội dung dự kiến:** capture từ reset, ít nhất10 controlled resets sau review. STM known bench: quan sát `board_diagnostics`, `tyvor_diagnostics`, `board_timing`, clock/reset/fault/watchdog progress qua debug path đã duyệt; không giả có UART log khi gate OFF. ESP known bench: ROM reason +app version/protocol/UART-disabled log, actual PSRAM detected bytes và heap/stack/WDT status. Không boot image nhà bán chưa biết rồi tuyên bố RF OFF.

**PASS:** no unexpected reset/fault/brownout, version/role/bench policy đúng identity, clock/counters tiến hợp lý, watchdog chạy theo reviewed policy; reset STM trở về safe/startup và không restore bench-armed state. ESP reset không có quyền ARM/RF/outputs.

**FAIL / STOP:** wrong image/role, RF/Wi-Fi/BT activity ngoài policy, reset loop/fault, watchdog không được bảo vệ, unsafe state hoặc logs không phù hợp. Reset flags/lifetime counts không được suy thành số lần reset qua power-cycle nếu firmware không lưu.

**Evidence:** full boot logs/debug observations từng reset, reason/register snapshots, unique run/board IDs, firmware hashes và watchdog timing evidence. **Hiện tại: NOT RUN/BLOCKED.**

## T06 — Firmware identity verification

**Điều kiện vào:** offline branch không cần board; device branch cần T04/G2 và phạm vi read-only được duyệt. Không chạy flash command, “repair”, unprotect, option-byte/eFuse writer hoặc auto-update.

**Nội dung dự kiến:** offline xác minh manifest/artifacts/build config trước khi đề xuất dùng image. Baseline Phase2C version `0.2.2-timing-bench`, protocol2/Core monitor2, BENCH_ONLY, RF OFF, gates OFF; STM VE512KiB/128KiB+64KiB linker, ESP app/bootloader/partition offsets0x10000/0/0x8000. Device branch dùng marking/chip-ID/revision/Flash size +readback được duyệt, so đúng image ranges/offsets với reviewed artifact, phân biệt ROM/factory/TYVOR identity. Version string đơn lẻ không chứng minh binary match.

**PASS:** offline hashes/config/layout match và device identity/readback match đúng expected image/target trong phạm vi readable regions. Flash/NVS mutable regions không được gộp nhầm vào app hash. ELF SHA embedded của ESP so đúng ELF; BIN hash so raw BIN range, không coi hai hash là một.

**FAIL / STOP:** hash/chip/layout/security policy khác, firmware origin unknown hoặc tool cần write/erase. Readback bị khóa/encrypted không cho phép so trực tiếp →UNVERIFIED/BLOCKED; không tự mở protection. Nếu cần nạp bench image thì dừng cho nhiệm vụ flash riêng.

**Evidence:** offline validation output, selected manifest/ELF/BIN hashes/config, read-only tool options/raw reads/region map, security-state observations, identity comparison. **Hiện tại: offline baseline hash kiểm tra PASS trong Phase2D; device branch NOT RUN/BLOCKED, toàn bước hardware chưa PASS.**

## T07 — ESP32 Flash / PSRAM detection

**Điều kiện vào:** T03–T06 phù hợp PASS, known diagnostic/bench image được cho phép chạy; không sửa Flash/PSRAM voltage/frequency/eFuse để thử. Nếu current firmware không đủ memtest instrumentation, ghi thiếu test fixture; không tự triển khai trong Phase2D.

**Nội dung dự kiến:** ghi chip marking/features, actual external Flash JEDEC/capacity và SDK detected size; expected N16R8 Flash16.777.216 bytes, PSRAM8.388.608 bytes. Phân biệt physical capacity với image-header/config size, total PSRAM với free PSRAM heap. Với buffer PSRAM đã được allocator cấp hợp lệ, kiểm thử address/data patterns, read/write/compare/checksum nhiều lần; không ghi đè vùng đang dùng và không làm destructive Flash capacity test. Ghi heap_internal/min/largest, PSRAM heap và task high-water.

**PASS:** actual capacity/part evidence khớp variant; PSRAM init/detection đủ8MiB, allocations xác nhận ở external memory, pattern checks0 mismatch, no reset/corruption/canary errors, headroom đạt reviewed budget. Package16MB và `IGNORE_NOTFOUND=y`/boot thành công không đủ PASS.

**FAIL / STOP:** thiếu/khác capacity, memtest mismatch, fallback internal RAM bị gọi nhầm là PSRAM, heap corruption/reset/brownout hoặc test đòi destructive writes. Không thay strap/eFuse/PSRAM voltage để ép PASS.

**Evidence:** JEDEC/chip/features logs, detected-byte counts, allocator capability/address-range evidence, buffer size/pattern/iteration/checksum/errors, heap/stack traces; source/hash của memtest fixture. **Hiện tại: NOT RUN/BLOCKED.**

## T08 — UART loopback từng board độc lập

**Điều kiện vào:** G4; actual TX/RX/domain/ground verified, boot/USB/debug conflicts loại trừ; có loopback fixture được review và thao tác gate/diagnostic firmware riêng được cho phép. Gate vẫn OFF trong Phase2D. Không dùng board kia làm loopback adapter.

**Nội dung dự kiến:** chỉ dùng local loopback harness trong approved single-board drawing. Fixture phát/thu/so byte với fixed seed, patterns00/FF/55/AA và encoded v2 frames hợp lệ dài32–64 bytes; đề xuất≥1000 complete frames cho mỗi board, split/burst và FIFO saturation có kiểm soát. Đo baud/levels/edge quality theo reviewed limits; count TX/RX/CRC/framing/overrun/timeout, capture recovery sau khi bỏ tải.

**PASS:** clean campaign exact byte match/ordering/count,0 unintended framing/overrun/CRC errors; injected saturation cho expected loss/error counters, queue bounded và recovery; Core scheduling/safety/outputs memory không bị test sửa. Timestamp/trace tương ứng với sent/received records.

**FAIL / STOP:** pin/level conflict, missing/reordered/corrupt data ngoài fault scenario, stuck TX/ISR, unexpected reset/deadline fault hoặc safety mutation. Current production session engine phải từ chối self-identity HELLO: tự loopback không phải handshake thành công. Không bỏ CRC/session validation để biến self-loop thành session PASS; low-level loopback fixture kiểm bytes tách khỏi link acceptance.

**Evidence:** approved local harness drawing/version, instrument/probe trace, exact pattern/seed/frame-count/error report, fixture source/binary hash, counters trước/sau, timing/safety logs. **Hiện tại: NOT RUN/BLOCKED — không có loopback mode/echo fixture hiện hành; không nối jumper.**

## T09 — STM32 ↔ ESP32 UART integration

**Điều kiện vào:** G5, T08 từng board PASS; actual matrix và interconnect/power/reference-ground plan được review, USB/probe/shared-ground/backfeed compatibility đo xong, được cho phép riêng nối hai board và bật bench UART. Không có drawing được phê duyệt trong hồ sơ hiện tại.

**Nội dung dự kiến:** sau phê duyệt, thử protocol2/monitor2 trên planned1152008N1, no hardware flow control, read-only requests. Capture full HELLO/SESSION_ACK tags/epochs/sequence/CRC và read-token responses. Theo dõi cả hai MCU; traffic tốt phải cho coherent model/inputs/outputs/safety/telemetry/mixes cache. Nguồn telemetry là MOCK vẫn phải ghi MOCK.

**PASS:** hai phía ready cùng session tuple, cache chỉ VALID với đủ parts/current lease; UART error counters không tăng ở clean link; ESP chỉ read, không quyết định ARM/RF/output. Core vẫn vận hành cục bộ khi ESP mất kết nối/reset. Không có RF/output hardware activity.

**FAIL / STOP:** contention/rail/ground/logic-level sai, wrong session/replayed frame được coi fresh, expired telemetry live, unsafe authority path, hangs/deadline fault hoặc noise ngoài budget. Không chấp nhận ACK OK như proof physical delivery hoặc proof mutation. Khi lỗi điện dừng test, không hot-rewire/thử GPIO khác.

**Evidence:** actual approved drawing/measurements, firmware/config hashes, bidirectional logic-analyzer UART capture/raw decoded frames, boot/session/queue/cache/safety/timing counters và fault-clear timestamps. **Hiện tại: NOT RUN/BLOCKED — không nối hai board.**

## T10 — Timing measurement & recovery

**Điều kiện vào:** T09 PASS cho integration campaign; có thể làm isolated gate-OFF timing baseline sau T05/T06 nếu được cho phép riêng. Ngưỡng/load/duration, approved fault fixture và memory budgets đã ký trước đo. Không biến native max thành hardware WCET.

**Nội dung dự kiến:** ghi max/distribution/samples dưới idle/normal/bidirectional saturation/slow UI; dùng actual clock và DWT CYCCNT/ESP timer. STM `TYVOR_TIME_INPUT` hiện bao gồm bench input/telemetry provider work; CORE không bao gồm calibration/feed trước tick; LOOP không gồm WFI. ESP work-before-publication không gồm publication/WDT/delay. Release→logical-output completion, ISR latency và critical-section peak cần correlated measurements; instrumentation hiện có chưa tự cung cấp đầy đủ. Không tự bật GPIO marker chưa xác minh; nếu cần thêm software instrumentation thì ghi nhiệm vụ riêng trước test.

Fault campaign dự kiến: drop/delay/duplicate/corrupt/truncate, TX FIFO/queue full, RX overflow, disconnect/reconnect, STM-only/ESP-only/both reset10 lần/loại. Dùng controlled digital fault fixture sau level review; không tạo short/power glitch. Xác minh old session/sequence/CRC/token bị loại, wrap/duplicate đúng policy, stale invalidation và recovery sau fault clear. STM reset phải safe startup; ESP reset không trực tiếp arm/disarm STM; input loss cần approved local simulation fixture vì firmware board hiện feed mock inputs liên tục.

**PASS:** measured budgets/deadline đạt acceptance đã review,0 missed Core ticks trong healthy/load campaign, no unbounded peripheral wait hoặc buffer growth, watchdog không bị diagnostics bypass; safe/failsafe đúng các fault case, sensors hết TTL không live, coherent recovery đạt target được ký. Queue timeout được xử lý ở first owner poll khi age≥20 ms; không đòi shift register dừng tức thì sau abort. Under deliberate fault, expected drops/failsafe phải khớp scenario; recovery khi fault đã bỏ, không hứa liveness dưới fault vô hạn.

**FAIL / STOP:** bất kỳ unsafe/RF condition, healthy deadline miss, measured IRQ starvation/unbounded wait, expired data VALID, memory corruption/headroom không đạt, watchdog reset ngoài scenario hoặc session không hồi trong acceptance window. Counter không đo được/halt debugger/thiếu correlated samples →BLOCKED timing conclusion, không PASS bằng zero counter mặc định.

**Evidence:** raw cycles/timestamps/actual clock, sample counts/max/distribution, DWT availability, debugger-halt exclusions, instrument uncertainty, ISR/critical section traces, exact load/seed/fault schedule, before/after session/cache/queue/safety counters, stack/heap/wdt logs và final signed results. DWT delta cần interval<2^32 cycles; observed max trong campaign không là formal WCET proof. **Hiện tại: NOT RUN/BLOCKED.**

## 3. Các mốc protocol để đối chiếu phép đo sau này

| Contract từ source hiện hành | Giá trị | Cách dùng trong test |
|---|---|---|
| Core tick |10 ms, một tick/poll | Deadline/scheduling evidence, không catch-up burst để che miss |
| Async TX queue |4 pending +1 active; max64 bytes | High-water bounded; coalesce chỉ pending STATUS, drop newest khi full không replace được |
| Queue deadlines |20 ms pending;20 ms active | Owner-service semantics; không phải autonomous wall-time watchdog |
| Parser timeout |50 ms candidate ownership | Truncate/interbyte recovery; không mất suffix frame mới |
| HELLO retry/deadline/cooldown |200 ms;5 attempts;1000 ms;1000 ms | Log đúng retry budget; không force ready trước proof |
| Read period/lease /partial cache |200/500/300 ms | Wrong/expired token hoặc aged batch không refresh cache |
| Legacy STATUS/peer stale |2000 ms | ACK-only traffic không hồi data freshness |
| Telemetry TTL |Battery/LQ/RSSI1000 ms; GPS2000 ms | Source age +lease RTT bound; expiry phải INVALID |
| Default input timeout |100 ms model default | Local loss→safety handling; không điều khiển input loss từ ESP |

Nguồn: `shared/tyvor_protocol.h`, `tyvor_link.h`, `tyvor_core_wire.h`, `tyvor_core_schema.h`, `tyvor_async_tx.h`, `stm32/engine/tyvor_models.c`. Test variant sau này thay config/model phải ghi contract tương ứng và reviewed hash; không dùng bảng này cho firmware khác.

## 4. Kết quả và review record

| Step | STM32 actual | ESP32 actual | Evidence hiện có | Disposition |
|---|---|---|---|---|
| T01 | NOT RUN | NOT RUN | Chỉ seller/reference docs | BLOCKED: thiếu ảnh/revision actual |
| T02 | NOT RUN | NOT RUN | Chưa có power measurements | BLOCKED |
| T03 | NOT RUN | NOT RUN | Chưa có rail measurements | BLOCKED |
| T04 | NOT RUN | NOT RUN | Chưa enumerate/attach probe | BLOCKED |
| T05 | NOT RUN | NOT RUN | Chỉ firmware source/offline receipts | BLOCKED |
| T06 | Device NOT RUN | Device NOT RUN | Offline Phase2C156 artifacts/123 hashes match | Hardware identity BLOCKED |
| T07 | N/A physical PSRAM target | NOT RUN | Config16MB/octal PSRAM, không physical detection | BLOCKED |
| T08 | NOT RUN | NOT RUN | Không có actual pin approval/loopback fixture | BLOCKED |
| T09 | NOT RUN | NOT RUN | Hai board chưa nối | BLOCKED |
| T10 | NOT RUN | NOT RUN | Chỉ native/simulation historical evidence | BLOCKED |

Reviewer/date/approved next scope: **TBD**. Tổng hardware PASS0; electrical FAIL0 observed (chưa đo không có nghĩa không lỗi). Hồ sơ này kết thúc ở chuẩn bị để review, không cho phép bắt đầu hardware bring-up. Xem [báo cáo](PHASE2D_HARDWARE_EVIDENCE_REPORT.md).
