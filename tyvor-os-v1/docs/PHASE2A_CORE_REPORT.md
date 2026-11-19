# TYVOR OS V1 - Phase 2A Core report

Ngày 09/11/2026, Asia/Bangkok. Workspace: `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Đã triển khai Core software bench, build hai MCU và chạy test native thực tế. Phase 2B readiness: READY WITH CONDITIONS cho bước review/chốt phạm vi software/bench tiếp theo.** Chưa có board/hardware PASS hoặc firmware dùng bay. Dừng ở báo cáo để người dùng review; không tự thực hiện Phase 2B.

Đã đọc đầy đủ initial audit, implementation report, final review và reliability fix report. Bốn báo cáo giữ nguyên. Đầu phiên đã xác minh 101 artifact/45 source của baseline Phase 1, đóng băng source trước sửa tại `build/phase2a-baseline/source` và giữ nguyên `artifacts/phase1`. Build mới ở `build/phase2a`, package mới ở `artifacts/phase2a`.

## 1. Module architecture

STM32 sở hữu input/model/mixer/safety/telemetry. ESP sở hữu UART endpoint, read-only cache và UI state. `stm32/engine` là C11 không include HAL/FreeRTOS, dùng cùng implementation ở native và MCU. Tổ chức chức năng tham khảo nhóm Inputs/Mixes/Outputs/Models/Telemetry trong tài liệu TX15 hiện có; đây là implementation gốc, không copy source/pin/RF từ EdgeTX hay TYVOR STABLE.

| Module | Hành vi đã triển khai |
|---|---|
| Input Manager | 4 gimbal raw mô phỏng 0..4095, switch 2/3 vị trí; calibration, deadband/rescale, integer IIR, immediate/filtered values; validity, sequence uint32 và timeout mặc định 100 ms |
| Mixer Engine | 6 logical sources; tối đa 16 ordered ADD/REPLACE terms; weight/offset/trim; 8 buses/channel-map permutation; int64 accumulation và final clamp -1000..1000 |
| Model Manager | ID/revision/name, input/mixer/channel/failsafe config; strict validation; serialization schema1 little-endian +CRC32/IEEE; hai-slot mock storage và persistence callback contract |
| Safety Engine | STARTUP/DISARMED/BENCH_ARMED/FAILSAFE; throttle lock, OFF-before-ON, safe-throttle edge, kill, input/model/timing faults; failsafe monitor values khi không bench-armed |
| Telemetry Manager | Mock battery/LQ/RSSI/GPS; validity/timestamp/expiration độc lập; partial update; không gọi dữ liệu giả là nguồn radio thật |
| Core scheduler | Deadline cố định 10 ms, tối đa một tick/poll; missed-slot counter và latched fault thay catch-up burst; Core chạy trước UART |
| UI Bridge | `const tyvor_core_t *`; frozen STATUS +5 Core parts cùng generation qua protocol v2; read-only request/ACK; không gọi safety/model mutation |
| ESP UI state | Atomic generation cache, age/stale/session handling; model/input/channel/safety/telemetry/connection và read ACK; renderer độc lập |

Core yêu cầu `TYVOR_BENCH_ONLY=1`; `TYVOR_ENABLE_RF=1` gây compile error. Không có RF/CRSF/PWM/output hardware adapter. BENCH_ARMED chỉ là logic bench. Legacy STATUS luôn ARM invalid/zero +MOCK; không biến trạng thái bench thành aircraft ARM.

Kiến trúc/API/units/policy: [PHASE2A_ARCHITECTURE.md](PHASE2A_ARCHITECTURE.md). Outer protocol v2/parser/CRC/session/STATUS schema giữ nguyên; thêm read-only DIAGNOSTIC sub-schema revision1, không đổi wire version. Đặc tả: [PROTOCOL_CORE_V2.md](PROTOCOL_CORE_V2.md).

## 2. Files changed

So với45 file baseline: **11 file sửa, 43 file mới, 34 file giữ nguyên**, tổng88 source/docs/scripts hiện hành. Không tính generated config/build/tools/artifacts.

Tệp mới:

- `stm32/engine/`: `tyvor_inputs`, `tyvor_mixer`, `tyvor_models`, `tyvor_model_store`, `tyvor_safety`, `tyvor_telemetry`, `tyvor_core`, `tyvor_core_bridge`, `tyvor_bench` (mỗi module `.c/.h`) và `sources.cmake`.
- `shared/`: `tyvor_core_schema.h`, `tyvor_core_wire.c/.h`, `tyvor_ui_state.c/.h`, `tyvor_ui_endpoint.c/.h`.
- `sim/`: `test_core.c`, `tyvor_sim_runtime.c/.h`, `tyvor_core_sim.c`, bảy scenario text trong `sim/scenarios/`.
- `scripts/build-phase2a.ps1`, `scripts/phase2a_artifacts.py`; bốn tài liệu Phase 2A: report, architecture, build, protocol extension.

Tệp sửa: root CMake/README; STM32 CMake/main/bridge C/H; ESP project/component CMake và main C; `scripts/build_provenance.py`, `test_package_validation.py` để nhận Phase 2A profile/inventory/input/output.

Giữ nguyên shared protocol/link/status, BSP/pin gates/clock/RNG/watchdog, linker VE, partition/sdkconfig defaults, Phase 1 test source và báo cáo lịch sử. ESP project version là `0.2.0-core-bench`. Không thêm AMOLED/touch driver hoặc sửa repository ngoài workspace.

Inventory/hash/diff đầy đủ: `build/phase2a/evidence/source-changes.json`, `phase2a-diff.patch`; package kèm source snapshot và Git status. Không tính SDK/tools/build/generated config vào source footprint.

## 3. Build result

| Target | Kết quả | Evidence trong `build/phase2a/evidence/` |
|---|---|---|
| Native Debug | Configure/build/CTest PASS | `native-debug.xml`, receipt/log |
| Native Release/NDEBUG | Configure/build/CTest PASS; CHECK vẫn chạy | `native-release.xml`, receipt/compile commands |
| STM32F407VET6 | ELF/BIN/MAP PASS; M4F hard-float, STM32F407xx, Flash512 KiB | `stm32-build.txt`, receipt, memory report |
| UART candidate object | Compile-check PASS, không link candidate vào default ELF | CMake target/symbol inspection; gate OFF |
| ESP32-S3 | IDF configure/build/link/size PASS, app/bootloader/partition | `esp32-build.txt`, receipt |
| ESP image/package | Offline checksum/hash/chip/offset/partition/config PASS | image-info logs và packaged validation JSON |
| SHA/source correspondence | Portable manifest +four current build receipts PASS | final offline validation và `artifacts/phase2a/SHA256_MANIFEST.json` |

Dùng tools đã pin: Clang20.1.7, Arm GCC14.2.1, IDF5.5.1/Xtensa GCC14.2.0, CMake3.30.9/Ninja1.12.1, Python3.11.13. Không tải SDK mới hay đổi system PATH/Git global. Ba HAL vendor warnings `unused Banks` trên F407 vẫn như Phase1, không sửa/che vendor warnings. Native bật `-Wall -Wextra -Wpedantic -Werror`.

Lệnh tái lập: [BUILD_PHASE2A.md](BUILD_PHASE2A.md). Receipt ràng buộc profile/compiler/source/config/build-driver/output hashes. Native gate kiểm tra exact inventory/executable/arguments, zero skips, NDEBUG và bench flags. Memory evidence bind ELF/BIN hashes; package từ chối inspection cũ.

## 4. Unit test counts

| Suite cuối | PASS | FAIL | Skipped |
|---|---:|---:|---:|
| Debug: 64 protocol/reliability +49 Core +7 scenarios | **120** | 0 | 0 |
| Release/NDEBUG: cùng inventory | **120** | 0 | 0 |
| Python offline package/provenance | **21** | 0 | 0 |
| Compile guards: native/Arm x3 cấu hình | **6** | 0 unexpected | 0 |

Tổng native cuối **240 lượt PASS**, gồm 226 lượt unit và 14 lượt scenario. Không cộng rerun/probe/Python vào số240. CHECK chạy cả Release; không có assert bị NDEBUG loại. Compile checks xác nhận bench build hợp lệ, RF=1 và thiếu BENCH_ONLY đều bị từ chối; đây là expected rejection.

49 Core cases bao phủ input/calibration/deadband/filter/timeout/ordering; mixer weight/order/routing/clamp/overflow; model bytes/CRC/version/validation/storage/selection; safety/scheduler; telemetry; wire/cache/generation/duplicate/source age/stale/session; UI authority/renderer và randomized campaigns.

Các campaign tái lập:

- Model/mixer seed `0x0002A407`: 1000 model ngẫu nhiên hợp lệ; canonical roundtrip, mixer bounds, bit-corruption rejection.
- Safety seed `0x005AFE2A`: 1000 bước throttle/switch/input loss/reset; kiểm tra failsafe-vector và input/kill/RF invariants.
- Giữ UART seeds `0x13579BDF`, `0x2468ACE0`, `0x00C0FFEE`: 6000 known frames/config; reboot seed `0x5EED1234`: 80 vòng/config. Hai HIGH regression Phase1 vẫn PASS.
- `ui_telemetry_recovery` quét 120 pha lịch và kiểm chứng bound320 ms khi nguồn telemetry trở lại trong simulator. Không gọi đó là latency board.

Những lỗi/tinh chỉnh thực tế:

1. Scenario đầu kiểm tra UI ở200 ms sau telemetry resume, trước khi một pha snapshot hoàn tất. Đã xác định source/cycle/last-part latency, dùng400 ms trong scenario và bổ sung sweep120 pha bound320 ms. Không nới stale/CRC để lấy PASS.
2. Test source-age đầu dùng battery age950+capture20, vẫn nhỏ hơn TTL1000. Đã sửa fixture thành990+20 để thực sự test expiration; deadline implementation không đổi.
3. Static review bắt simulator đọc persisted model trước reset: CRC hỏng có thể trả lỗi và giữ instance bench-armed cũ. Đã safe-reset trước storage read, latch MODEL/FAILSAFE khi restore lỗi. `safety_reset_corrupt_storage` link cùng C test với runtime library trước sửa **FAIL exit1**, runtime mới **PASS exit0**. Evidence: `reset-storage-before.txt`, `reset-storage-after.txt`, library hash trong `reset-storage-regression.json`.

## 5. Simulator scenarios

Simulator dùng Core/bridge/UI endpoint C thật, fake clock và queue UART cố định/split byte stream qua parser thật. Có stdin interactive hoặc script tự động, không mở board.

| Scenario | Hành vi kiểm tra | Debug/Release |
|---|---|---|
| safe_startup | Safe boot, OFF/low ->ON edge; gimbal/channel endpoints; bảy monitor pages | PASS |
| throttle_interlock | Startup ARM ON; high throttle; giữ ON rồi hạ throttle không arm; cần OFF/ON lại | PASS |
| input_loss | Đang bench-armed ->timeout ->failsafe values; input hồi với ON không rearm | PASS |
| model_mixer | Model B weight50%; safe select; unsafe select bị từ chối; save adapter | PASS |
| reset_recovery | STM reset safe startup/held ARM; ESP reset invalid/cache/session hồi phục | PASS |
| telemetry_expiry | Chỉnh battery/LQ/RSSI/GPS, expiration độc lập và phục hồi | PASS |
| ui_independence | Link mất4,7 s: UI invalid, Core vẫn chạy/local state; reconnect không ra lệnh ARM | PASS |

File có prefix `scenario_` trong `sim/scenarios/`; log riêng được lưu ở evidence. Simulator điều chỉnh được gimbal/switch/model/telemetry, input/link loss, reset STM/ESP, `expect` state/channel và in bảy UI views. Model selection là API bench local, không qua ESP. Đây là text view từ cache thật, chưa có graphical navigation/AMOLED/touch/LVGL.

RAM mock storage sống qua reset Core object trong cùng process, không sống qua process/power loss. Simulator reset đọc slot0, không khôi phục armed state. STM32 main hiện seed mock defaults; chưa có Flash persistence driver.

## 6. Safety test results và authority

| Yêu cầu | Bằng chứng | Kết quả |
|---|---|---|
| Throttle chưa an toàn không ARM | high_throttle, hold_switch, filter_bypass; immediate và filtered <=-950 | PASS |
| Startup ARM switch ON | startup_switch +scenario; held ON không arm | PASS |
| Input loss ->safe state | timeout/input_loss/recovery; failsafe throttle=-1000 theo model mapping | PASS |
| Reset MCU | STM STARTUP/FAILSAFE, corrupt-storage regression; ESP chỉ reset cache | PASS simulator |
| Mixer clamp/overflow | 16x3500=56000 trước clamp, cả dấu âm, cancellation/order, invalid extremes | PASS |
| Model CRC/config lỗi | CRC golden; mọi bit trong137-byte model; invalid config/version; decode output không đổi | PASS |
| Sequence/session recovery | 64 test cũ và ui_session_recovery/new epochs/cache invalidation | PASS |
| Status stale | Legacy stale +Core no-revival/partial/generation/source-age tests | PASS |
| Scheduler deterministic | Anchored deadlines, no catch-up burst, overrun/clock latch | PASS |
| Outputs không RF | core_no_rf, compile guards, source boundary, default STM UART TX not linked | PASS software scope |
| ESP không thay safety/output/model | ui_authority/read_ack: payload ARM/RF/OUT/MOD, wrong-direction valid STATUS và unknown op không sửa Core | PASS |

STM reset luôn mất state armed và yêu cầu startup lại. ESP reset không tự disarm/arm STM: Core tiếp tục theo input/model/fault cục bộ; UI INVALID đến khi coherent snapshot/session mới. Đây là sự độc lập hai MCU, không phải bỏ qua reset safety.

Trace authority: ESP read API ->v2 session/direction/schema/sequence validation ->STM read service ->ACK/snapshot queue. Bridge giữ const Core pointer; không có đường gọi model select/safety/channel writer từ request. DIAGNOSTIC từ UI bị dispatcher bỏ qua. Test so byte toàn bộ Core trước/sau request; ELF ESP không link Core/mixer/safety mutation symbols. CRC/session không phải authentication.

STM Core/bridge chỉ chạy main; ISR chỉ thu RX ring. ESP bridge là sole owner của endpoint/cache dưới mutex; diagnostics copy state/stats/epochs dưới cùng lock, unlock rồi stale-poll/render bản copy. Không thêm UI writer hoặc đảo thứ tự khóa. Đây là source/native integration evidence, chưa phải RTOS race detector.

## 7. Memory usage và artifacts

| STM32F407VE | Byte cuối |
|---|---:|
| BIN / Flash capacity | **14960 / 524288** (~2,85%) |
| ELF có debug | **1112500** |
| Vector / text / exidx | 392 /14548 /8 |
| `.data` /`.bss` | **12 /2132** |
| Stack reserve tối thiểu /section gồm padding | 4096 /4100 |
| SRAM region gồm data/bss/stack/alignment | **6248 /131072** (~4,77%) |
| CCM dùng /capacity | **0 /65536** |

VE linker giữ Flash0x08000000/512 KiB, SRAM0x20000000/128 KiB, CCM0x10000000/64 KiB; không dùng VG1 MiB. Đã dựng BIN từ ELF PT_LOAD so byte-for-byte; allocated bounds và98 vector hợp lệ; SP0x20020000, Reset/entry0x08002D79/Thumb đúng. Default ELF có Core/mixer/safety, không link unverified HAL_UART_Transmit.

Core không heap. Largest per-function stack estimate552 (`bench_seed_storage`), main400, deserialize312, serialize304 byte; không phải call-chain/IRQ peak hoặc runtime. So Phase1 reliability: Flash +6540, SRAM region +1136 byte.

| ESP32-S3 | Byte cuối |
|---|---:|
| App BIN /ELF debug | **218368 /3625668** |
| Bootloader /partition BIN | **22336 /3072** |
| Static DIRAM /allocated map | **60095 /341760** (~17,58%) |
| DIRAM text /data /bss | 45211 /11404 /3480 |
| Vùng IRAM riêng của linker | 16384 /16384 |
| Flash code /rodata | 107966 /37004 |
| Factory3 MiB còn trống | **2927360** |

IRAM riêng100% không có nghĩa hết MCU RAM. Static DIRAM không phải free heap; hai task stack4096, watermark/heap logs chưa chạy trên board. So Phase1 reliability: app +3440 và DIRAM +376 byte.

Flash16 MB config, PSRAM octal40 MHz/TYPE_AUTO/IGNORE_NOTFOUND, partition layout kết thúc0x1000000 giữ nguyên. Chưa xác minh Flash vật lý/PSRAM8 MiB. App/bootloader S3 image header/checksum/hash hợp lệ; descriptor ELF SHA khớp ELF. QIO config/DIO ROM header theo SDK giữ nguyên.

Memory evidence lưu ELF/BIN hashes và stack estimates. Package giữ SDK subdirectories: bootloader@0x0, partition@0x8000, app@0x10000, khớp mọi metadata reference. Offline validator kiểm tra image/partition/config và schema2 manifest exact inventory/source hashes. Manifest không tự hash chính nó; report không chèn hash tự tham chiếu.

## 8. Remaining risks

- Chưa boot/đo board, RNG/PLL/HSI/tick/IWDG, pin/levels/UART FIFO/ISR, MCU đúng VE hoặc runtime stack/heap. UART gates OFF. Native scheduler không thay WCET. Bounded TX10 ms có thể làm lỡ tick10 ms khi bật transport; cần queue/DMA và đo jitter/deadline trước realtime integration.
- Safety/model/mixer là policy bench tối thiểu. Chưa physical debounce/ADC-DMA/calibration workflow, production throttle/mixer constraints, radio/receiver/CRSF/actuator, failsafe điện hay safety certification. Invalid-model fallback là bench zeros/channel3=-1000, không suy thành an toàn hardware.
- Mock storage chưa nonvolatile/atomic Flash driver, wear/power-loss recovery hoặc schema migration; callback contract mới test trên RAM. Name tối đa14 ASCII; chưa curves/expo/flight modes/trainer/audio/scripts.
- ESP có state/text view, chưa graphical UI/panel/touch/LVGL. Không suy đoán pinout/interface/init/controller hoặc xác nhận PSRAM/AMOLED.
- RNG boot identity và session assumptions như Phase1; không chống peer giả current tag, exact reused tuples không phân biệt replay. Read ACK chỉ xác nhận queue snapshot, không có write/capability transaction negotiation.
- Chưa sustained sanitizer/fuzz/HIL campaign, adversarial RTOS load, bootstrap từ máy trống hoặc bit-identical cross-host build. Seeded native/fake-clock campaigns chỉ chứng minh phạm vi đã mô tả.

## 9. Git diff / WIP

Git root vẫn là starter cha; source untracked, tracked/staged diff rỗng. Không stage/commit/push/deploy/Git global. Diff thực dựa snapshot45 source đầu phiên, không tạo baseline commit giả.

Evidence: `build/phase2a-baseline/source-state.json`; `source-changes.json`, `phase2a-diff.patch`; packaged `source-snapshot.patch`, `git-status.txt`, manifest; four current receipts, Debug/Release JUnit, bảy scenario logs,21 package tests,6 compile checks và reset regression trước/sau.

Package cuối có70 artifact và88 source hashes; final offline validation đối chiếu exact inventory/size/SHA-256 với0 mismatch/missing.

Không đổi Phase1 artifact directory hoặc repository STABLE/EdgeTX. Báo cáo lịch sử, protocol/link/status, BSP/linker/config defaults được đối chiếu hash giữ nguyên. Source hiện hành là WIP cần người dùng review.

## 10. Phase 2B readiness

**READY WITH CONDITIONS cho review/chốt scope Phase 2B software/bench.** Đã đạt module/safety policy,120 Debug +120 Release, bảy scenarios, negative package/compile guards, two-MCU build/link, memory/layout, read-only authority boundary và SHA/source-to-artifact correspondence.

Sau review nên chọn scope tiếp theo: native model/UI tooling hoặc kế hoạch hardware bench có schematic/revision/pin/level xác nhận. Hardware cần boot/entropy/clock/watchdog/stack/heap/PSRAM8 MiB và transport deadline đo thật trước bật UART gate; panel/touch cần tài liệu đúng module. Persistence thật/future UI writes cần design/test/review riêng trên authority STM32. Không tự triển khai Phase2B hoặc nạp thiết bị trong phiên này.
