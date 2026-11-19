# TYVOR OS V1 - Phase 2B software & bench preparation

Ngày **10/11/2026, Asia/Bangkok**. Workspace: `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Software bench preparation: PASS, chờ người dùng review. Hardware readiness: NOT READY / UNVERIFIED.** Đã review baseline, sửa bốn lỗi có probe C, bổ sung transport/fault injection/diagnostics/UI, build hai MCU và kiểm chứng package offline. Không flash, mở serial/debugger, nối hai board, enable UART GPIO, bật RF, commit/push/deploy hoặc đổi Git global.

Đã đọc reliability/Phase2A reports và các source/tests hiện hành. Đầu phiên xác minh70 Phase2A artifacts +88 source hashes; lưu snapshot tại `build/phase2b-baseline`, chạy lại đủ120/120 Debug và120/120 Release, gồm7 scenario cũ. Historical Phase1/2A reports/artifacts được giữ nguyên. Build/package mới tách riêng `build/phase2b` và `artifacts/phase2b`.

## 1. Review Phase 2A và phân loại

Phạm vi severity là software bench hiện tại; gate OFF không biến compile/test thành hardware PASS. Tổng review software: **0 BLOCKER, 2 HIGH đã sửa, 3 MEDIUM (2 đã sửa,1 còn mở),1 LOW còn mở**. Board evidence thiếu là điều kiện chặn hardware bring-up riêng, chưa phải lỗi điện được quan sát.

| ID | Severity | Bằng chứng / root cause | Kết quả |
|---|---|---|---|
| B-H1 | HIGH | `tyvor_core_input`: invalid sample rồi good sample trước tick kế có thể ghi đè validity; safety không nhìn thấy loss và vẫn BENCH_ARMED | Latch input-fault event đến khi safety owner xử lý; recovery với ARM held ON không rearm |
| B-H2 | HIGH | C2 rev1 age chỉ tính đến lúc serialize; giữ batch CRC-valid1,3 s rồi giao lại làm battery cũ VALID | Receiver-issued read token +deadline/RTT age bound, C2 rev2; producer freeze sau request; expired/wrong-session/token rejected |
| B-M1 | MEDIUM | UI poll hết hạn input nhưng để outputs_valid=1 | Clear output validity/channel view khi input hết hạn; không sửa Core outputs |
| B-M2 | MEDIUM | `core_load_model` đọc storage trước safe-selection guard; callback vẫn chạy khi bench-armed | Guard trước storage I/O và revalidate khi select; không gọi storage cho unsafe request |
| B-M3 | MEDIUM, OPEN | Candidate BSP TX có synchronous timeout10 ms trong Core period10 ms; còn có bounded handshake/ACK work | Không giả vờ adapter đã giải quyết WCET; giữ UART gates OFF. Cần queue/DMA hoặc timing evidence/schedule review trước UART hardware |
| B-L1 | LOW, OPEN | Source vẫn untracked trong Git root cha, tracked diff không biểu diễn WIP | Không tự stage/commit; dùng frozen baseline/source hashes/full snapshot và patch để review |

Đã trace Inputs ->Mixer ->Safety ->channel memory, Model validation/CRC/store, Telemetry timestamp/expiry, Core schedule và UI cache ownership. Không thấy RF/actuator implementation hoặc UI request path gọi model-select/safety/channel writer. Model serialization/CRC, mixer math và outer protocol/parser/link không cần sửa để đạt Phase2B. Những cải tiến UI/diagnostics dưới đây là yêu cầu mới, không gán thành lỗi Phase2A vì khác scope trang cũ.

### Probe trước/sau - không chỉ đếm test

`sim/test_bench_regressions.c` được compile với Phase2A code trước sửa ở cả Debug và Release/NDEBUG; mỗi case thực sự exit1. Log tại `build/phase2b-baseline/{Debug,Release}-*-before.txt`. Bản sửa dùng cùng probe, adapter theo monitor revision khi cần; current logs tại `build/phase2b/evidence/{Debug,Release}-*-after.txt` đều exit0.

| Probe | Trước sửa, cả Debug/Release | Sau sửa, cả Debug/Release |
|---|---|---|
| transient_input_loss | invalid t41, good t42, tick t50: state2/BENCH_ARMED ->FAIL | state3/FAILSAFE, throttle lock ->PASS |
| delayed_telemetry | source batch t200, delivery t1500: UI valid1, battery-valid1 ->FAIL | lease/token expired, UI/battery invalid ->PASS |
| expired_input_outputs | input-valid0 nhưng outputs-valid1 ->FAIL | cả hai valid0 ->PASS |
| unsafe_storage_read | unsafe selection vẫn gọi read1 lần ->FAIL | read0 lần ->PASS |

Với B-H2 wire monitor đã đổi rev1->2, probe branch mới dùng current read token; không mô tả đây là byte-for-byte wire-compatible test. Outer v2/CRC validation không bị bỏ/nới. Original chronological8 expected-FAIL logs và8 post-fix logs được giữ riêng; không cộng vào tổng suite cuối.

## 2. Thay đổi đã triển khai

**Core fixes:** input-fault event chỉ safety tick được acknowledge; timeout/reacquisition không che mất sự kiện. `can_change` kiểm tra pending fault/model validity; model load không đọc storage trước guard. Không thay ARM/throttle/kill policy bằng diagnostics hoặc UI.

**Bench adapter:** `shared/tyvor_bench_transport.c/.h` dùng cho cả firmware và native. Không tự init GPIO/open serial/RF/flash; gate lấy từ compile-time board policy. Backend full/negative/partial results được phản ánh thật, có bounded cap/error/suppressed counters. Native backend `sim/tyvor_bench_wire.c/.h` có32-event FIFO mỗi chiều, split chunks, delay/jitter, loss, CRC corruption, duplicates, partial/failing TX, queue overflow, hold và reconnect. Không heap/sleep/device access trong wire model.

**Fresh read correlation:** UI gửi DIAGNOSTIC C3/schema1/read-only token7 bytes, nominal200 ms; một lease outstanding, deadline500 ms. STM freeze sau request, echo token trong C2 generation. UI yêu cầu full-offer/current session/token và dùng local elapsed upper bound để tính age. Response chưa xong được chờ đến deadline, tránh starvation khi RTT lớn hơn request period; delay250+30 ms có test hoàn tất. Token không wrap; cạn thì fail-closed. CRC/session không authentication, không chống peer độc hại giả current token.

**Mixer monitor:** C2 rev2 giữ kind1..5 layout; thêm kind6 paged two terms/29 bytes, lấy đủ16 ordered terms nếu model có16. UI chỉ công bố config khi đủ pages cùng snapshot/token; B=logical bus, không gán nhầm bus thành channel sau routing. Không có write/ARM/output opcode. Outer protocol v2 và base STATUS schema giữ nguyên, ARM invalid/zero +MOCK.

**Diagnostics:** shared read-only collector +STM debugger global/ESP console/native `diag`. Gồm boot reason, FW0.2.1-bench, protocol2, session state/epochs, RX/CRC/timeout/duplicate/TX/partial/suppressed counters, reset-observer scope, heap/stack flags, WDT status/checkpoints và last STATUS age. Không feed/disable WDT hoặc thay safety checks.

STM ghi raw RCC reset flags, current MSP sample trong4 KiB allowance, IWDG refresh counter và successful backend-copy STATUS TX age; không suy đó là peer delivery hoặc peak stack. ESP dùng reset-reason/task-WDT/heap/stack APIs, RX STATUS age. Native counters đếm reset sau init trong process; firmware boot-local observed-reset count0, lifetime/power-cycle count UNVERIFIED. Không thêm Flash/retention driver giả định. STM app không heap; native physical heap/stack/WDT metrics UNVERIFIED. Chi tiết scope/flags: `MCU_COMMUNICATION_SPEC.md`.

**UI:** đúng7 trang HOME/MODEL/INPUTS/MIXES/OUTPUTS/TELEMETRY/SETTINGS, renderer-independent. Banner thể hiện VALID/STALE/DISCONNECTED/SAFE/INVALID MODEL; không render expired sensor value như số live. ESP reset chỉ invalidates UI và renegotiates, không trực tiếp arm/disarm STM. Native có fault/reset-both/mix-scroll/model-corruption/diagnostic commands và4 integration scripts mới.

File groups thay đổi:

- Core/core bridge và existing bridge C/H; shared Core schema/wire, UI state/endpoint; main/BSP diagnostic observation; target CMake.
- New transport/diagnostics shared modules, native wire/tests/regression/exporter,4 scripts trong `sim/bench_scenarios`.
- New `build-phase2b.ps1`, `phase2b_artifacts.py`, `validate_bench_ui.py`; extend provenance/package-test profiles; README và Phase2B documents.

Không thay board GPIO definitions, hardware gate defaults, linker VE, Flash/PSRAM/partition defaults, tools lock, protocol/parser/link/STATUS source hoặc Phase1 tests. Exact modified/new/unchanged inventory nằm trong `source-changes.json`; không dựa vào Git tracked diff rỗng.

## 3. Kết quả kiểm thử và build

| Hạng mục cuối | PASS | FAIL / skipped | Evidence |
|---|---:|---:|---|
| Baseline Phase2A Debug /Release trước sửa | 120 /120 | 0 /0 | `build/phase2b-baseline/{debug,release}.xml` |
| Native Debug mới | **154** | 0 /0 | `build/phase2b/evidence/native-debug.xml`, receipt |
| Native Release/NDEBUG mới | **154** | 0 /0 | release XML/receipt, compile commands |
| Package/provenance negative tests | **21** | 0 /0 | `package-tests.json/.txt` |
| Compile guard checks, native/Arm | **8** | 0 unexpected | `bench-build-guards.json/.txt` |
| UI structural inventory | **35 SVG**,7 pages x5 states | 0 mismatch | `ui-validation.json` +exporter hash |
| STM32F407VE configure/build/link | PASS | 0 final build errors | STM receipt/log/ELF/BIN/MAP |
| ESP32-S3 IDF app/bootloader/partition | PASS | 0 final build errors | ESP receipt/log/image-info |
| Offline package /SHA/source inventory | PASS | 0 mismatch/missing | manifest +final validator output |

Suite154 gồm120 retained cases (đã review/migrate khi monitor revision đổi),4 regressions,26 bench tests và4 integration scenarios. Mỗi cấu hình có143 unit/regression cases +11 scenario scripts. **Tổng cuối308 native test executions =286 unit/regression +22 scenarios**; không cộng baseline reruns, probes, Python, compile checks hay GUI exports vào308. CHECK luôn thực thi trong NDEBUG; provenance gate xác minh inventory/executable/arguments/flags, zero skips và input/compiler/output hashes.

Giữ nguyên7 kịch bản cũ; thêm `bench_fault_reconnect`, `bench_reset_matrix`, `bench_aged_packet`, `bench_seven_pages`. Bao phủ independent STM/ESP reset, simultaneous reset, held ARM, session collision/recovery, stale cache, delayed old session, >lease delay, within-lease delay, partial TX, interbyte timeout, overflow/gate suppression, token replay/exhaustion/before-offer, all16 mixer terms và telemetry-age bound.

Seed mới0x002BF407:120 bước mixed loss/corruption/duplication/delay/chunk/reset, kiểm tra bounds/RF OFF và final recovery. Giữ campaigns cũ UART3 seeds/6000 known frames, reboot0x5EED1234/80 vòng, model0x0002A407/1000 và safety0x005AFE2A/1000. Telemetry recovery sweep mở rộng đủ200 pha theo read period mới, bound400 ms trong lịch native không fault; Phase2A bound320 ms không được dùng như SLA của revision mới. Test `bench_telemetry_age_bound` kiểm tra không còn battery-valid sau source expiry dù packet đang delay.

Toolchain pin giữ nguyên: Clang20.1.7, Arm GCC14.2.1, IDF5.5.1/Xtensa GCC14.2.0, CMake3.30.9/Ninja1.12.1, Python3.11.13. Native warnings-as-errors PASS; vendor HAL unused-Banks warnings vẫn như baseline. Không tải/cài SDK mới.

Trở ngại công cụ đã xử lý: packager từ chối README bị ghi CP1252 trong một lần cập nhật bằng Python/Windows; chuẩn hóa UTF-8 rồi package PASS, không publish source/binary correspondence sai. Chrome headless mặc định lỗi launch; local-only renderer với private profile và sandbox option phù hợp chạy được. Đã render/xem contact sheet35 views `evidence/ui-contact.png`: labels/readings rõ, không thấy text đè/cắt trong sheet. Đây là desktop software preview, không phải panel/PSRAM/hardware PASS.

## 4. Memory và firmware size

| STM32F407VET6 | Giá trị cuối |
|---|---:|
| BIN /Flash VE capacity | **16752 /524288 byte**, khoảng3,20% |
| ELF debug | **1137216 byte** |
| Vector /text /exidx | 392 /16340 /8 byte |
| `.data` /`.bss` | **12 /2504 byte** |
| Stack reserve | **4096 byte** |
| SRAM region gồm reserve/alignment | **6616 /131072 byte**, khoảng5,05% |
| CCM dùng | **0 /65536 byte** |

VE layout giữ Flash0x08000000/512 KiB, SRAM0x20000000/128 KiB, CCM0x10000000/64 KiB. BIN dựng từ ELF PT_LOAD khớp byte-for-byte; allocated bounds và98 vector hợp lệ; SP0x20020000, reset/entry0x08003475/Thumb hợp lệ. Default ELF không link HAL_UART_Transmit, có Core/mixer/safety. Largest per-function stack552 byte; main416; Core wire decode368/encode360. Không gọi đó là peak call-chain/IRQ stack. So Phase2A: Flash +1792, SRAM region +368 byte.

| ESP32-S3 | Giá trị cuối |
|---|---:|
| App BIN /ELF debug | **222048 /3660056 byte** |
| Bootloader /partition BIN | **22336 /3072 byte** |
| DIRAM used /allocated map | **60867 /341760 byte**, khoảng17,81% |
| DIRAM text /data /bss | 45271 /11404 /4192 byte |
| Vùng IRAM riêng | 16384 /16384 byte |
| Flash code /rodata | 111066 /37520 byte |
| Factory3 MiB còn trống | **2923680 byte** |

IRAM riêng100% không có nghĩa hết MCU RAM. Static numbers không phải free heap/stack runtime. Hai task vẫn stack4096; watermark/heap/WDT logs chỉ được instrument, chưa chạy board. So Phase2A: app +3680, DIRAM +772 byte.

`memory-report.json` binds ELF/BIN hashes, checks correct architecture/layout/vector and ESP không link Core authority symbols. ESP image inspection offline checksum/appended hash/chip/header PASS; app version0.2.1-bench. Package bootloader@0, partition@0x8000, app@0x10000 giữ SDK subdirectories và flasher_args references đúng. Flash16 MB/QIO, ROM DIO header, PSRAM octal40 MHz/IGNORE_NOTFOUND và partition layout giữ nguyên; không suy ra physical Flash/PSRAM đã PASS.

## 5. Hardware readiness và pinout

**NOT READY cho đấu nối/flash/enable UART.** Chưa đối chiếu được maker/revision/schematic/net của STM32 Mini Type-C và WeAct board thực tế; chưa cấp nguồn/đo/attach/đọc chip. Tất cả actual power pins/polarity/input rails, boot straps/buttons, connector positions, UART GPIO/header routing/electrical domains/reference-ground/backfeed plan là **UNVERIFIED**. AMOLED/touch FPC/power/interface/controller/pinout vẫn UNVERIFIED.

Checklist tại [BOARD_BRINGUP_CHECKLIST.md](BOARD_BRINGUP_CHECKLIST.md) dùng nguồn ST/WeAct/Espressif chính thức và phân biệt silicon reference, software config với board evidence. Không lấy board A/DevKitC/seller pinout để chốt WeAct-B hoặc gán candidate GPIO thành wiring đã xác nhận. Script không tự đổi gate hoặc nối nguồn/UART.

Diagnostic reset totals qua power cycle, physical PSRAM8 MiB, clock/RNG/tick/watchdog timing, runtime stack/heap và full duplex UART throughput/error recovery cần kiểm tra trên board riêng. Diagnostic mode không vô hiệu hóa safety/watchdog checks. No RF/real drone control implementation được thêm.

## 6. Lỗi/giới hạn còn lại

- B-M3 còn mở: bounded blocking TX không phải realtime-safe DMA/queue solution. Gate OFF; trước UART bring-up cần schedule/WCET redesign hoặc measurements được review. Adapter không chứng minh timing hardware.
- B-L1: untracked Git baseline; chỉ người dùng quyết định tracking/commit ở việc riêng. Frozen source/diff/manifest hỗ trợ review, không thay lịch sử Git.
- Read-token/CRC là freshness/corruption contract với trusted software peers, không cryptographic authentication. Exact boot identity reuse/RNG quality chưa đo; token exhaustion fail-closed cần UI reset. Old-v2 peer chỉ có base STATUS, new monitor cache INVALID; không silent rev1 downgrade.
- RAM model storage chưa nonvolatile/atomic Flash/power-fail recovery. Real ADC/debounce/calibration workflow, RF/actuator/mixer production constraints, graphical navigation/LVGL/panel/touch và persistent reset history ngoài scope.
- Fault model deterministic/bounded không thay electrical/baud/ISR/RTOS test, long-duration fuzz/sanitizer/HIL, physical noise/EMI hoặc board stack/heap peak measurements.

## 7. Git diff/WIP và artifacts

So baseline88 file: **28 file sửa,20 file mới,60 file giữ nguyên**, tổng108 source/docs/scripts hiện hành. Không tính SDK/tools/build/generated config/artifacts.

Git root vẫn là starter cha, source untracked. Tracked/staged diff rỗng không có nghĩa không có thay đổi; không tạo commit/index giả. `source-changes.json` so current sources với88-file frozen Phase2A baseline; package kèm `phase2b-diff.patch`, full `source-snapshot.patch`, Git status, source hashes, receipts, before/after regressions, scenario logs,35 SVG và contact sheet.

`artifacts/phase2b/SHA256_MANIFEST.json` là exact portable artifact inventory/hash/size và workspace-relative source hashes. Packaging yêu cầu current successful receipts/memory evidence/UI hashes;21 negative tests kiểm tra missing/path/offset/corruption/config/stale receipt/JUnit inventory/skips. Không dùng hash file như lời khẳng định hardware đã được đo hoặc chống compiler/receipt giả mạo.

Gói cuối gồm140 artifacts và108 source hashes, được kiểm tra exact inventory/size/SHA-256 với0 mismatch/missing. Manifest không tự hash chính nó; không chèn hash tự tham chiếu vào report được manifest bảo vệ.

Phase1/2A artifact directories và historical reports được đối chiếu giữ nguyên. Không sửa SDK/vendor source, repository EdgeTX/TYVOR STABLE hoặc Git global. Các source changes là WIP chờ review.

## 8. Điều kiện được phép bắt đầu hardware bring-up

Software bench đã chuẩn bị và các gates software PASS. Cần người dùng review report/diff/manifest, xác nhận exact board/revision/schematic và approved power/pin/electrical/boot/debug plan, đóng vấn đề TX timing trước enable UART, cùng backup/rollback/diagnostic measurement plan. Mỗi board nên được kiểm chứng riêng trước interconnect; PSRAM/clock/watchdog/stack/heap cần log/số đo thực.

Mọi flash, mở serial/debugger, đấu nối hoặc thay UART gate phải nằm trong công việc hardware mới được yêu cầu rõ ràng. Report/checklist này không tự cho phép các thao tác đó. **Đã dừng ở triển khai software, build/test/fault injection, package offline và báo cáo để người dùng review.**
