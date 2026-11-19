# TYVOR OS V1 - Phase 2C timing & hardware readiness

Ngày **10/11/2026, Asia/Bangkok**. Workspace `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Software timing preparation: PASS. Hardware bring-up: NOT READY.** Đã loại synchronous STM TX wait khỏi candidate branch, bổ sung bounded async transport, tách UI khỏi bridge owner, instrument timing và chạy build/test offline. Chưa xác nhận revision/power/header/logic của board thực tế hoặc đo WCET/ISR/RTOS. Không flash, cấp nguồn, mở serial/debugger, enable UART gates, bật RF, thêm AMOLED/touch driver, commit/push/deploy hoặc đổi Git global.

Đã đọc đầy đủ bốn tài liệu được yêu cầu và audit source hiện hành. Baseline Phase2B: xác minh **140 artifacts /108 source hashes**, đóng băng108 file ở `build/phase2c-baseline/source`, chạy lại **154/154 Debug +154/154 Release**. Historical Phase1/2A/2B reports/artifact directories giữ nguyên. Build/package mới tách `build/phase2c` / `artifacts/phase2c`. Firmware version `0.2.2-timing-bench`; outer protocol2/Core monitor revision2 giữ nguyên.

## 1. Timing architecture

### STM32 Inputs → Mixer → Safety → logical outputs

Source trace: `main.c` mock sample → `tyvor_core_input` → input timeout/fault latch/calibration/deadband/IIR → `tyvor_core_poll` → input/telemetry expiry → logical6 inputs → ordered mixer → safety state → clamp/routing/failsafe channel memory. Input feed được đo riêng; mixer/safety/apply nằm trong Core tick measurement. Không thay Core policy hoặc đưa UI vào authority path.

| Đường chạy | Work bound trong source | Period/deadline dự kiến |
|---|---|---|
| Inputs | 4 axes,2 switches, model validation≤16 mixes; không I/O/heap | Mock source10 ms; default validity100 ms |
| Mixer | ≤16 ordered ADD/REPLACE, int64 accumulation,8 buses/map, final clamp±1000 | Mỗi Core tick |
| Safety/apply | Fixed state machine +8 channels; throttle/ARM edge/loss/model/timing faults | Mỗi Core tick; không RF callback |
| Core scheduler | Anchored10 ms, một tick/poll, không catch-up burst; skipped slots latch scheduler fault | Proposed completion deadline release+10 ms; proposed Core work budget<1 ms |
| UART RX | Max64 bytes/poll; parser bounded frame64, suffix recovery, rate-limited read/ACK | Sau Core, không peripheral wait |
| TX service | Tối đa4 expired removals +1 start/poll; active driver progress hữu hạn | Sau Core và sau bridge poll; không busy wait |
| Diagnostics | Fixed-size copy/counters,250 ms | Không thay safety/WDT policy |

Core period/deadline/budget là **software design target**, không số đo board. Source HSI16 MHz, WFI giữa iterations, SysTick1 ms. Full-loop budget counter10 ms; Core budget1 ms không bao gồm input feed trước tick. Cần đo tổng input+Core completion/release lateness, UART/IRQ/critical section và loop dưới tải; không kết luận schedulable từ test native. Existing missed-slot detection ở một full period late không thay được phép đo completion deadline.

### Queue và wire budget

Hai phía dùng `shared/tyvor_async_tx.c/.h`: **4 pending +1 active**, mỗi frame≤64 bytes, owned copy, không heap/sleep/retry loop. `write` không gọi driver. Full success chỉ chứng minh copied/accepted; frame vẫn có thể mất do expiry/abort/wire faults, giống packet loss trong existing session protocol.

Pending STATUS được bỏ bản cũ rồi append latest; làm được khi queue đầy nếu có pending STATUS. Active buffer immutable tới completion/abort. Queue đầy không có replaceable STATUS →drop newest/return-1 ngay. Retained control/ACK/DIAGNOSTIC giữ FIFO; không claim control priority hoặc session recovery trong congestion vô hạn. STATUS/read publication đã rate-limited; CRC/schema không bị nới.

Pending residence deadline20 ms; active deadline20 ms riêng, exact boundary expired. Poll checks timeout/clock regression **trước** driver progress. Expired/drop/error/full/coalesced/completed/high-water counters có sẵn; main/task không chờ queue trống. Service bị CPU stall thì expiry xử lý khi owner được chạy lại, không phải watchdog độc lập bảo đảm20 ms wall time. Một byte đã vào shift register/FIFO có thể hoàn tất sau abort; parser/session/read lease xử lý truncation/delayed packets.

115200 8N1 là planned baud:64 bytes cần khoảng5,56 ms trên wire; TX+RX continuous tương đương tối đa khoảng23.040 byte interrupts/s với STM byte ISR. Đây là phép tính lý thuyết, **không throughput/CPU load measurement**. Four queued max frames có thể bị deadline drop khi tải cao; loss/retry được test, không ép nhận dữ liệu quá tuổi. Read lease500 ms vẫn bao gồm local queue/roundtrip delay, không cần đồng bộ clocks.

## 2. Blocking paths, ownership và instrumentation

**STM:** candidate cũ gọi `HAL_UART_Transmit(...,10)` từ mọi bridge/link TX callback, gồm handshake/ACK trong RX processing. Đã thay bằng enqueue và TXE/TC ISR; không dùng HAL polling transmit, không thêm DMA dependency hoặc chọn GPIO mới. ISR ghi≤1 TX byte/lần, xử lý RX/error cùng IRQ không early-return làm mất TX progress. TC mới kết thúc active frame. Init HAL error →boot fault; start/progress error/timeout →abort/counter, Core không chờ peripheral. Clock/RNG/IWDG init có HAL timeouts lúc boot; runtime watchdog refresh không có UART TX wait. Model persistence hiện RAM mock; future physical persistence chưa có timing contract.

Main owns queue/Core/protocol. ISR chỉ owns RX producer, TX index/busy/TC; không chạm queue/parser/Core. Active bytes sống trong queue tới TC/abort. Save/restore PRIMASK ở start/abort và RX read≤64; không disable IRQ cho cả frame. SysTick priority0, USART2 priority5; ISR không gọi FreeRTOS API. Priority không chứng minh main có đủ CPU dưới sustained UART flood; cần đo IRQ load/overrun thật. Default ELF gate OFF, candidate chỉ Arm object compile-check; không có GPIO được enable trong phiên này.

**ESP:** bridge task priority5 là sole owner UART1/endpoint/cache/TX queue. Shared cache mutex đã bỏ; bridge publish value projection qua FreeRTOS length1 overwrite queue. UI/diagnostics priority2 peek nonblocking, poll stale/render/log bản copy. Render chậm chỉ bỏ intermediate projection; không giữ owner lock hoặc gọi UART. Queue có internal bounded copy/spinlock theo SDK, chưa đo latency SMP trên board. Publication1152 bytes; hai task-owned static copies tránh nhân đôi stack; heap queue thêm item1152 bytes +SDK overhead, không giả free-heap measurement.

ESP TX retains suffix/offset khi `uart_tx_chars` nhận partial; stall/error bounded bởi queue deadline. RX ring2048/event16, drain≤256 bytes/iteration, nominal task delay10 ms. Observed event queue full được coi là possible lost error event: reset parser/flush input/reset events. Không báo chính xác số event đã mất. CRC/session/stale/lease bảo vệ dữ liệu, recovery sau khi saturation dừng có test.

Audit SDK5.5.1 `components/esp_driver_uart/src/uart.c`: `uart_tx_chars` không chờ FIFO space nhưng có internal tx mutex `portMAX_DELAY`; only bridge calls UART1 nên không có UI owner giữ mutex. `uart_flush_input` lấy rx mutex, disable RX interrupts rồi drain hữu hạn; không có application `uart_write_bytes`/`uart_wait_tx_done`. RTOS corruption/CPU starvation/SDK lock latency chưa được chứng minh bằng native fakes. ESP reset không có flow-control/interlock chặn STM Core.

**Instrumentation:** debugger global `board_timing`: DWT CYCCNT checked advancing, INPUT/CORE/UART/LOOP last/max/sample/over-budget, max release lateness/count. Elapsed cycles có IRQ time trong region, không gồm WFI; chỉ hợp lệ interval<2^32 cycles, debugger halt/long fault không làm WCET evidence. ESP published max work-before-publication microseconds và max bridge start-gap; work metric không gồm publication/WDT/delay. Timing tests dùng fake cycle arithmetic, `hardware_measured=0`; **chưa có runtime board samples, hardware WCET/jitter/IRQ timing = UNVERIFIED**. Diagnostics không disable checks/feed WDT từ UI/ISR.

## 3. Regression và kết quả trước/sau

| Phát hiện | Mức / root cause | Trước sửa thực tế | Sau sửa thực tế |
|---|---|---|---|
| C-M1 / Phase2B B-M3 | MEDIUM bench; candidate synchronous UART timeout10 ms cạnh Core10 ms | Trích nguyên hàm BSP cũ, fake HAL dùng đúng timeout argument: Debug/NDEBUG exit1, simulated_wait10 ms,1 blocking call | Cùng probe current BSP exit0, wait0,0 blocking HAL calls; queue copy only |
| C-M2, phát hiện khi phát triển queue | MEDIUM; state callback có thể advance ESP FIFO trước timeout check | `tx_deadline_before_driver`: Debug/Release exit1 vì driver vẫn được gọi ở deadline20 ms | Check deadline trước state: cùng regression exit0 cả hai cấu hình, driver call count0, abort/timeouts1 |
| UI mutex / partial FIFO / event saturation | Timing/recovery risks được harden, không khẳng định đã quan sát deadlock hardware | Source ownership/SDK evidence; không có hardware probe hoặc invented FAIL | Sole-owner publication, suffix retention, bounded queue và fault tests PASS trong software scope |

Probe C-M1 không đo hardware hoặc chứng minh HAL thực tế luôn tốn10 ms: nó chứng minh actual callback có thể đưa toàn timeout đó vào Core loop. Logs/commands ở `build/phase2c-baseline/{Debug,Release}-blocking-before.*` và `build/phase2c/evidence/*-blocking-after.*`; script `probe_timing_regression.py` tái lập từ frozen/current source.

C-M2 không tồn tại trong Phase2B chưa có queue; giữ intermediate source `build/phase2c-baseline/async-poll-before.c`, two `development-deadline-*-before.txt`, after logs và source hashes trong regression JSON. Không dùng count PASS để thay FAIL-before evidence. CRC/parser/sequence/session validation và Core safety source giữ nguyên.

## 4. Build/test thực tế

| Hạng mục cuối | Kết quả | Evidence |
|---|---|---|
| Baseline native |154 Debug +154 Release PASS | `build/phase2c-baseline/{debug,release}.xml` |
| Current native Debug | **177 PASS /0 FAIL /0 skipped** | `build/phase2c/evidence/native-debug.xml`, receipt |
| Current native Release/NDEBUG | **177 PASS /0 FAIL /0 skipped** | release XML/receipt; CHECK luôn chạy |
| Platform helper fakes | **4 executable probes PASS** | Actual STM start/ISR/abort và ESP TX helpers ×Debug/Release; source-hash-bound JSON |
| Compile guards | **8 expected outcomes PASS** | Native/Arm ×bench/RF forbidden/BENCH required/diag-off-safe |
| UI export | **35 SVG PASS**,7 pages×5 states | Actual Core/cache exporter +XML/hash validation; không đổi renderer |
| STM32F407VET6 | **ELF/BIN/MAP PASS** | ARM/M4F hard-float/VE layout; candidate UART object API/type check |
| ESP32-S3 | **app/bootloader/partition PASS** | IDF5.5.1 receipt, ELF/S3 image/config inspection |
| Package offline negative tests | **21 PASS**,0 failures/errors/skips | package-tests JSON/text |
| Package/manifest | **PASS**,0 missing/mismatch | final-manifest-validation JSON +manifest exact file/source hashes |

177 = retained154 +23 timing/queue/integration cases, mỗi cấu hình **166 unit/regression +11 scenario**; tổng cuối **354 native executions =332 unit/regression +22 scenarios**. Không cộng baseline, reruns, expected-FAIL development probes, helper probes, Python hoặc SVG vào354.

23 cases mới:12 queue (copy ownership,full/coalesce cả full queue,active immutable,expiry,timeout,start/state errors,clock,max64/invalidCRC,order,seed,deadline-before-driver),4 instrumentation arithmetic/budget/lateness,7 actual-endpoint async integrations (duplex,STM reset,ESP reset,both reset,TX saturation,RX saturation,slow UI).

Integration queues dùng cùng production async implementation, mock six-ms transfer và real protocol/session/Core/UI endpoint; Core không missed ticks/RF dưới fake-clock schedule, stale views không hồi sinh, sau fault/reconnect UI/session hồi hợp lệ. Không chạy FreeRTOS scheduler hoặc electrical UART trong native. Actual platform helper tests giữ suffix khi FIFO chỉ nhận7 bytes/call, zero-space timeout, negative error/recovery; STM tests TC completion, RX error vẫn tiến TX, abort và PRIMASK restore.

Retain toàn bộ protocol/session/parser regressions,7 Core scenarios +4 bench scenarios, authority/ARM/input loss/model/mixer/stale tests. New queue campaign seed `0x002CF407`,10.000 steps/config; giữ UART3 seeds/6000 frames, reboot seed0x5EED1234/80 rounds, model0x0002A407/1000 và safety0x005AFE2A/1000. Đây là deterministic tests, không sanitizer/HIL campaign.

Compiler/SDK giữ Clang20.1.7, ArmGCC14.2.1, IDF5.5.1/XtensaGCC14.2.0, CMake3.30.9/Ninja1.12.1/Python3.11.13. Native warnings-as-errors PASS; vendor unused-Banks warnings giữ nguyên. Tool đọc PDF PyMuPDF1.26.6 cài riêng trong build folder; không sửa toolchain/global Python. Lệnh replay: [BUILD_PHASE2C.md](BUILD_PHASE2C.md).

## 5. Memory, firmware artifacts và package

| STM32 default gate-OFF build | Byte |
|---|---:|
| BIN /Flash capacity | **17196 /524288** |
| ELF debug | **1144564** |
| vector /text /exidx |392 /16784 /8 |
| data /bss |12 /2616 |
| SRAM region gồm4096 stack reserve/alignment | **6728 /131072** |
| CCM used /capacity |0 /65536 |

VE bounds Flash0x08000000/512 KiB, SRAM0x20000000/128 KiB, CCM0x10000000/64 KiB.98 vectors hợp lệ; SP0x20020000, reset/entry0x08003631; ELF PT_LOAD →BIN khớp byte-for-byte. Default Flash +444, SRAM region +112 so Phase2B. Candidate queue472 bytes và RX ring256 +metadata chỉ ở compile-checked gated branch, **không tính như đã link/đo UART-enabled firmware**. Candidate object không reference bất kỳ HAL_UART_Transmit function nào. Largest compiled per-function stack552, main424; không phải full call-chain/IRQ peak.

| ESP32-S3 | Byte |
|---|---:|
| app BIN /ELF | **223888 /3671228** |
| bootloader /partition BIN |22336 /3072 |
| static DIRAM used /allocated map | **63943 /341760** |
| DIRAM text /data /bss |45563 /11404 /6976 |
| separate IRAM linker region |16384 /16384 |
| Flash code /rodata |112414 /37712 |
| factory3 MiB remaining |2921840 |

App +1840, DIRAM +3076 so Phase2B. Two task stacks4096 bytes; compiler per-function estimates diagnostics1568, bridge176, nested decode/cache frames tới368, chưa cộng SDK/log/IRQ call-chain. Queue init heap item/control-block và runtime heap/stack vẫn cần logs; không suy free RAM từ static DIRAM. Separate IRAM100% không có nghĩa hết MCU RAM. ESP ELF không link Core/mixer/safety authority symbols.

Package giữ flasher_args đúng tree: bootloader@0x0, partition@0x8000, app@0x10000; header/checksum/SHA/S3/16MB/partition MD5/layout/config PASS offline. Flash16MB, PSRAM octal40MHz/IGNORE_NOTFOUND config giữ nguyên; chưa physical PSRAM PASS. Manifest binds current four receipts/compiler/source/output/test inventory, memory hashes, platform helper hashes, UI hashes. **G?i cu?i156 artifacts /123 source hashes**; final validator0 mismatch/missing. Không tự hash manifest hoặc chèn hash tự tham chiếu vào report.

## 6. Bảng board/pin có nguồn và phần cần xác nhận

Nguồn official đã đọc: [ST DS8626](https://www.st.com/resource/en/datasheet/stm32f407ve.pdf), [WeAct exact B reference schematic](https://github.com/WeActStudio/WeActStudio.ESP32S3-AorB/blob/c53183ea4aa96c6b7abf2b1dc153bafa22d2b96e/ESP32S3-B/Hardware/ESP32_S3_B_Sch%20.pdf), [Espressif GPIO restrictions](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/gpio.html). Seller exact products: [ThegioiIC Type-C](https://www.thegioiic.com/mach-stm32f407vet6-type-c), [Tuyết Nga S3-B N16R8](https://dientutuyetnga.com/products/weact-thu-phat-wifi-ble-esp32-s3-b-n16r8). Seller Tuyết Nga link trực tiếp WeAct repo. Seller association **INFERRED**, không xác nhận revision đã giao.

| Tham chiếu | VERIFIED trong tài liệu | Actual board / kết nối |
|---|---|---|
| STM UART candidate | PA2/PA3 USART2 TX/RX AF7 ở MCU | Header availability/domain/conflicts **UNVERIFIED**, gate OFF |
| STM power | ST VDD1.8..3.6V; seller lặp silicon range | Regulator/input/USB/backfeed/GND/limits **UNVERIFIED**, không dùng VDD làm board input rating |
| STM USB/debug/boot | Silicon PA11/12 USB,PA13/14 SWD; boot reference | Board routes/straps/buttons/revision **UNVERIFIED** |
| WeAct-B power/UART | B PDF VCC_3V3, U0 UART còn nối USB-UART U7; schematic có +5V/LDO | Actual regulator rating/population/3.3V logic/limits/header **UNVERIFIED** |
| WeAct GPIO17/18 | Net names có ở B reference header | Chỉ document evidence; **chưa chọn TX/RX**, actual free GPIO **UNVERIFIED** |
| ESP reserved | Espressif26..32 Flash/PSRAM; octal/R8 thêm33..37; B PDF vẫn xuất35..37 ra header | Không dùng nhóm này làm UART candidate; actual chip/revision **UNVERIFIED** |
| ESP USB/boot/onboard | Reference19/20 native USB,0/3/45/46 straps,U0→U7,48→LED,45→USER | Giữ ngoài UART selection; actual routing **UNVERIFIED** |
| GND / electrical compatibility | WeAct reference có GND nội bộ | Common reference/power isolation/backfeed/cross-board drawing **UNVERIFIED** |

Đây là evidence table, **không phải sơ đồ đấu nối hoặc lệnh cấp nguồn**. Exact ThegioiIC schematic chưa tìm được trong trang sản phẩm; WeAct B PDF đã tìm được/đọc/render nhưng không có revision rõ và chưa khớp ảnh PCB thực tế. Không dùng DevKitC/board A/cùng MCU để lấp chỗ trống. Không có approved GPIO/power/header/common-GND connection.

PDF SHA256 `910a41eea1b8d111e8efc78667eb01d95a30b6e7ae2b9ce7567afcafd3913e68`, git blob d27f6ad987b174aa9515e7207ec9a8fa4be45e95 khớp tree snapshot c53183e. Raw sources/retrieval hashes/text/render được đóng gói. Chi tiết classification và evidence checklist: [BOARD_EVIDENCE_PHASE2C.md](BOARD_EVIDENCE_PHASE2C.md), [BOARD_BRINGUP_CHECKLIST.md](BOARD_BRINGUP_CHECKLIST.md).

Cần ảnh rõ hai mặt, version/logo/markings MCU/Flash/regulator/USB bridge, connector/button/jumpers; exact schematic revision và seller variant. Công việc hardware riêng sau approval cần continuity/resistance/polarity record khi unpowered và rail/current/idle-level/ground/backfeed measurements theo plan đã review; không tự chốt điện áp hoặc chân từ ảnh seller. Còn cần actual Flash/PSRAM ID/capacity/memtest, clock/RNG/tick/watchdog, full-duplex UART/error storm/recovery, DWT WCET/jitter/deadline/critical-section/IRQ load, RTOS heap/stack peak. AMOLED vẫn UNVERIFIED và ngoài scope.

## 7. Rủi ro còn lại

- Hardware **NOT READY**: missing actual revision/net/power evidence; source/reference facts không thay schematic match và phép đo điện.
- Async architecture loại peripheral wait, chưa chứng minh hard realtime. IRQ storm có thể lấy CPU của main; SDK internal locks/publication copy, interrupt masking, active bytes sau abort và scheduler service gaps phải đo. Không claim deadline hardware PASS.
- Session/read lease giữ trusted-peer assumptions, không authentication; entropy/boot identity quality chưa kiểm chứng board. Queue overflow có thể mất control/ACK; finite retries/cooldown recovery sau congestion, không bảo đảm live dưới fault vô hạn.
- Static stack estimates không peak; ESP heap queue/task allocations, physical PSRAM8MiB và lifetime reset counters chưa đo. IGNORE_NOTFOUND không được dùng làm PSRAM success proof.
- RAM model storage, mock input/telemetry, no RF/output hardware/ADC/LVGL/AMOLED/touch remain bench-only; không firmware dùng bay. ESP không có authority ARM/RF/outputs; read-only path và ELF boundary vẫn PASS.
- Source untracked WIP; không có commit baseline thật. Snapshot/hash/diff giúp review, không thay Git history.

## 8. Git diff / WIP

So108 baseline: **13 modified +15 new +95 unchanged =123 source/docs/scripts**. Modified: root CMake/README; STM BSP C/H/main/CMake; ESP project/component CMake/main; shared diagnostics version; build_provenance; communication spec/checklist. New: async TX/timing C/H, native timing/probe C, Phase2C build/artifact/probe/platform/guard/fetch scripts và ba tài liệu Phase2C.

Giữ nguyên protocol/parser/link/STATUS, Core Inputs/Mixer/Model/Safety/Telemetry/bridge, read-lease/wire/cache state, VE linker và Flash/PSRAM/partition defaults; đã hash đối chiếu baseline. Historical Phase1/2A/2B reports và artifacts không sửa. Board checklist/spec là living docs được cập nhật rõ Phase2C, không viết lại historical reports.

Git root vẫn starter cha, sources untracked; tracked/staged diff rỗng không nghĩa WIP rỗng. Không stage/commit/index baseline giả. Evidence `source-changes.json`, `phase2c-diff.patch`, full `source-snapshot.patch`, `git-status.txt`, manifest, receipts và frozen baseline biểu diễn WIP thực tế. Không sửa SDK/vendor, EdgeTX hoặc TYVOR STABLE.

## 9. Kết luận và điều kiện hardware bring-up

**NOT READY cho hardware bring-up.** Software deliverables và regression/build/package gates PASS; vẫn chưa có bằng chứng exact PCB/revision/power/pin/voltage và timing đo thật. Không chuyển compile/native PASS thành hardware approval.

Sau khi người dùng review: cần exact board evidence/approved power-and-net plan, approved phạm vi hardware từng board riêng, backup/diagnostic/rollback plan và measurement acceptance criteria. Chỉ công việc hardware mới được yêu cầu rõ ràng mới được flash/attach/đo/đổi UART gate. Inter-MCU UART chỉ sau pin/level/reference-ground review và timing/IRQ/error recovery evidence; RF vẫn ngoài scope. **Dừng ở software implementation, build/test offline, package và báo cáo để người dùng review.**
