# TYVOR OS V1 — STM32F4 Discovery bring-up report

Ngày **10/11/2026, Asia/Bangkok**. Workspace `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Software build/regression: PASS. Hardware identification, backup và first flash: BLOCKED / NOT READY.**
Đã tạo BSP Discovery và demo riêng; chưa ghi firmware lên board. ST-LINK xuất hiện
qua USB nhưng giao diện debug thiếu driver (Code28); chưa tìm thấy CubeProgrammer/CLT.
Chưa đọc Device ID/Flash/protection và chưa có backup firmware gốc. Không được
coi USB enumeration hoặc native simulation là xác nhận MCU/board hoạt động.

## 1. Baseline và phạm vi

Đã đọc các báo cáo Phase2A–2D, architecture, communication specification, board
evidence/checklist/test plan và đối chiếu source/tests hiện hành. Bối cảnh thay
board này chỉ áp dụng cho target mới; những kết luận pinout Mini Type-C trong
Phase2C/2D không được chuyển sang Discovery.

Trước sửa đã đóng băng **126 source/docs/scripts** trong
`build/discovery-baseline/source/` và `source-state.json`. Kết quả cuối:
**125 file baseline không đổi, chỉ root `CMakeLists.txt` sửa; 16 file mới**,
tổng142 file trong source catalog. Toàn bộ `stm32/engine`, `shared`, `esp32`,
tests cũ, BSP/linker/startup/main của VE giữ nguyên byte. Các package lịch sử
Phase1/2A/2B/2C kiểm tra lại lần lượt **101/70/140/156 file hash**, không mismatch.
Không sửa repository STABLE/EdgeTX hoặc Git global; không flash/erase, đổi option
bytes, mở COM/UART, nối ESP/RF, commit/push/deploy hoặc cài driver/tool mới.

## 2. Nhận diện board và ST-LINK — dừng trước target access

| Hạng mục | Bằng chứng thực tế | Trạng thái |
|---|---|---|
| Board/PCB | Người dùng cung cấp STM32F4 Discovery / MB997E; chưa có ảnh hai mặt/suffix | USER-REPORTED, chưa match assembly thực tế |
| MCU `STM32F407VGT6` | Mục tiêu build; chưa thấy marking, chưa SWD-read | UNVERIFIED |
| USB probe | VID0483/PID374B, composite serial `066DFF575377524867063531` | DETECTED |
| Debug interface MI_00 | `ST-Link Debug`, Status Error, ProblemCode28; DriverInfPath/Service trống | BLOCKED: driver chưa bind |
| USB MSD / VCP | MI_01 OK, MI_02 `USB Serial Device (COM6)` OK | ENUMERATED ONLY; không mở cổng |
| CubeProgrammer / CLT | Không thấy CLI trong PATH/common ST locations; installed-ST inventory rỗng | NOT FOUND trong phạm vi đã kiểm tra |
| Device ID, silicon revision, UID | Chưa đọc | BLOCKED |
| Flash size / RDP / nWRP / voltage | Chưa đọc/đo | BLOCKED |
| CN3 / JP1 / boot / MCO / nguồn thực tế | Chưa có ảnh hoặc số đo | UNVERIFIED |
| Backup firmware gốc | Chưa tạo; không dùng artifact TYVOR thay cho backup | BLOCKED |

Read-only Windows PnP inventory thành công (214 present devices lúc đầu), ghi nhận
bốn interface liên quan. Script inventory mới cũng trả **BLOCKED**, `cube_cli=null`,
`target_identified=false`, `backup_verified=false`, `flashed=false`. Không thử
attach SWD, halt/reset hay thao tác chip khi điều kiện host chưa đạt.

Evidence: `artifacts/stm32f4-discovery-bench-v1/evidence/host-inventory.json`,
`stlink-driver-properties.json`, `readonly-inventory.json`, `hardware-identification.json`.
Không kết luận board lỗi điện hoặc sai MCU từ lỗi driver Windows.

## 3. Hardware reference và BSP mới

Đã tra trang [STM32F4DISCOVERY của ST](https://www.st.com/en/evaluation-tools/stm32f4discovery.html),
schematic chính thức MB997 E01 và UM1472 Rev9. Bảng LED/USER/SWD/clock cùng nguồn
và checklist đối chiếu actual board ở
[STM32F4_DISCOVERY_FLASH_GUIDE.md](STM32F4_DISCOVERY_FLASH_GUIDE.md).
E01 là reference đúng họ/revision chữ E; full assembly suffix, population và
marking chip thực tế vẫn cần ảnh xác nhận, không được mặc định đã match.
Các PDF ST đọc được qua web; tải trực tiếp vào workspace timeout, không có local
PDF/hash để giả làm bằng chứng. Log timeout ở `evidence/board-sources/retrieval.json`.

BSP `stm32/boards/stm32f4_discovery/` độc lập, standalone CMake cross target:

- `STM32F407xx`, Cortex-M4F/hard-float; pinned CMSIS5.9.0/device2.6.10/HAL1.8.3;
  startup/vector dùng template ST F407, own linker và HAL configuration.
- Linker VG: **1 MiB Flash** tại `0x08000000`, **128 KiB SRAM** tại `0x20000000`,
  **64 KiB CCM** tại `0x10000000`. Không cấp phát CCM hoặc heap. Memory capacities
  đối chiếu [ST DS8626 Rev12, family F405/F407 gồm VG](https://www.st.com/resource/en/datasheet/stm32f407ve.pdf),
  §5 memory map và quy ước part-number G=1024 KiB; không lấy board pinout VE.
- Clock implementation: **HSE BYPASS 8 MHz**, PLL M8/N336/P2/Q7, SYSCLK/HCLK168 MHz,
  APB1 42 MHz, APB2 84 MHz, voltage-scale1/Flash latency5. Không dùng HSE crystal
  mode của Mini Type-C, không tự fallback nếu MCO thiếu. Đây là cấu hình phần mềm,
  chưa là clock đã đo trên board.
- SysTick1 ms, priority0; Core schedule10 ms giữ nguyên. Main owner duy nhất;
  ISR chỉ tăng HAL tick. USER polling, không EXTI; LED ghi BSRR giới hạn PD12–15.
- IWDG prescaler64/reload999, danh định khoảng2 s, chỉ refresh sau vòng khỏe;
  không refresh từ ISR. Boot clock fault trước khi bật IWDG giữ đỏ/halt;
  application fault sau khi bật watchdog ngừng refresh. Chu kỳ thực chưa đo.
- `TYVOR_BENCH_ONLY=1`, RF0, UART0. Gate UART ON bị CMake/header từ chối;
  chưa có UART pin assignment/driver. SWD được giữ nguyên. Không link UART,
  CRSF, RF, PWM/SPI output hoặc driver AMOLED/touch.

Board diagnostics debugger-visible: reset flags, clock, HSE mode, watchdog status,
refresh count, loop count, fault code và stack remaining tại checkpoint; không
ghi nhận các giá trị này như hardware measurement khi firmware chưa chạy.

## 4. TYVOR DISCOVERY BENCH V1

`discovery_demo.c` không phụ thuộc HAL, sở hữu Core instance và RAM mock storage;
cùng implementation được compile vào native và firmware. Không sửa hành vi Core
Phase2C. Input cung cấp mỗi10 ms, telemetry mỗi100 ms khi scenario cho phép.

| Mode | Hành vi trong RAM | Expected safety |
|---|---|---|
| SAFE (boot default) | Mock sticks trung tính, throttle thấp, mock ARM OFF | STARTUP → DISARMED |
| MIXER | Chu kỳ4 s, gimbal tam giác, mock switch/off-before-on/throttle thấp trước sweep | BENCH_ARMED chỉ trong mô phỏng; channels clamp ±1000 |
| INPUT_LOSS | Chu kỳ3 s, ngắt mock input1.0–1.3 s, giữ mock switch ON lúc recovery rồi OFF | FAILSAFE sau timeout, không tự rearm khi ON |
| MODEL_TELEMETRY | Chọn model A/B mỗi2 s khi disarmed, tạo khoảng trống telemetry trong chu kỳ6 s | DISARMED, telemetry expired invalid |

USER: debounce30 ms, một lần/mỗi press; giữ lúc boot không đổi mode cho đến khi
release. Đổi mode khởi tạo Core về trạng thái safe. Không dùng USER làm ARM thật.
Self-test boot chạy model serialization/CRC-invalid/config-invalid, mixer positive/
negative clamp, safety input loss và telemetry expiry. Green heartbeat1 Hz,
orange activity5 Hz, blue theo nút debounced, red khi simulated FAILSAFE hoặc fault.
Không có physical channel/output adapter; BENCH_ARMED chỉ là enum và RAM values.

### Lỗi được phát hiện trong implementation mới

1. **Demo model-read fault trùng USER edge**: mode MODEL_TELEMETRY đọc model lỗi
   đúng mốc2 s; USER vừa đủ debounce có thể chuyển mode và gọi safe_restart trước
   khi trả lỗi, che nguyên nhân fault khỏi trạng thái Core vừa reset. Đã latch/
   kiểm tra fault ngay sau model load, trước xử lý nút. Cùng test
   `discovery_model_fault` **FAIL exit1 trước sửa ở cả Debug/Release**, sau sửa
   **PASS exit0**, giữ nguyên mode, counter, fault và đỏ. Không sửa Core hoặc
   nới validation. Evidence `model-fault-{Debug,Release}-{before,after}.txt`,
   `model-fault-regression.json`, gồm source/test hashes.
2. **Build driver mới bỏ nhánh STM khi `-Target all`**: dot-source `activate.ps1`
   có parameter cùng tên `$Target`, ghi đè biến caller. Lần trước fix chỉ có native
   receipts; đã giữ `$DiscoveryRequestedTarget` trước activate. Full run sau sửa
   tạo đủ bốn receipts; `build-driver-regression.json` ghi before/after.

Đây là lỗi của phần Discovery mới trong phiên này; không gán chúng thành lỗi
Phase2C cũ. Các lỗi cấu hình harness ban đầu đã sửa; chỉ kết quả cuối bên dưới là
PASS được dùng cho package.

## 5. Build/test thực tế

| Kiểm tra | Kết quả | Evidence |
|---|---|---|
| Native Debug | **198/198 PASS**,0 fail/skip | `native-debug.xml`, receipt/config/compile commands |
| Native Release/NDEBUG | **198/198 PASS**,0 fail/skip | `native-release.xml`, NDEBUG verified |
| Regression cũ | 177 case mỗi config, gồm11 scripted scenarios | Exact CTest/JUnit inventory, không thay test cũ |
| Discovery mới | 21 case mỗi config | `sim/test_discovery.c`, assertions CHECK luôn chạy |
| Actual GPIO helper bodies + fake registers | **2 runs PASS** Debug/Release,65536 LED masks mỗi run; PA0 active-high cases | `discovery-platform-checks.json` — không phải đo GPIO |
| Compile guards native/ARM | **14/14 PASS**, expected rejection UART/RF/missing BENCH/wrong board | `discovery-platform-checks.txt/json` |
| Programmer/package guard tests | **17/17 PASS**, synthetic fixtures only | `discovery-tools-tests.txt/json` |
| Discovery STM32 build | **ELF/BIN/MAP PASS** | `discovery-receipt.json`, build/config logs |
| Old VE target rebuild | **PASS**, BIN byte-identical Phase2C | `legacy-ve-receipt.json`, memory report |
| ELF/BIN/vector/linker/symbol inspection | **PASS** | `memory-report.json` |
| Discovery package SHA/inventory/board preflight | **PASS offline** | Manifest và offline validation evidence |
| ST-LINK debug / target ID / backup | **BLOCKED** | DriverCode28, tool missing |
| Hardware LED/button/clock/WCET/watchdog | **NOT RUN** | Không có firmware được nạp |

198 =177 baseline +21 Discovery. Không cộng17 Python tests hoặc16 platform checks
vào số CTest. Regression gồm parser/session/reboot/CRC/stale/async saturation/
determinism/safety/mixer/model; protocol v2 không đổi. ESP source/artifacts cũ
không đổi, không chạy ESP build mới trong task Discovery này.

Discovery test cases bao phủ safe boot/selftests, debounce/bounce/held boot/long
hold/release/cycle, safe mode transition, mixer sweep/clamp, loss/recovery, model
selection/error, telemetry expiry, clock/missed-tick fault, USER không xóa timing
fault, LED logical state, reset safe, no-RF, deterministic seed `0xD15C0407`.
Python tests từ chối write/erase/reset/halt/unprotect/option assignment trong
read-only dispatcher, sai family/VE size/RDP, path escape/overwrite, package
tamper/wrong board/missing backup; valid synthetic flash-plan cũng không gọi subprocess.

Tools: LLVM/Clang20.1.7 native, Arm GCC14.2.1, CMake3.30.9/Ninja, Python3.11.13;
pyelftools của IDF environment chỉ dùng inspect ELF offline. Không cài global tools.
Lần full cross build có ba warning unused parameter `Banks` trên mỗi target trong pinned
`stm32f4xx_hal_flash_ex.c`; không sửa vendor SDK để che warning. Reconfigure còn
báo `CMAKE_TOOLCHAIN_FILE` manually-specified unused trên build tree đã cấu hình;
compiler receipt/ARM ELF vẫn xác minh đúng cross toolchain. Không có application/
native warning mới trong final build logs.

## 6. Artifact và memory

Package: `artifacts/stm32f4-discovery-bench-v1/`, manifest `SHA256_MANIFEST.json`.
Build files độc lập ở `build/discovery/discovery/`; ELF/BIN/MAP được copy vào
`artifacts/stm32f4-discovery-bench-v1/stm32/` cùng cấu hình và evidence.

| Giá trị từ ELF/linker | Kết quả |
|---|---|
| Flash image / BIN | **11,560 bytes /1,048,576**, khoảng1.10% |
| ELF có debug symbols | 1,044,840 bytes; không phải Flash footprint |
| MAP | 337,067 bytes |
| Static data +BSS +padding | **1,240 bytes** SRAM |
| Stack capacity reserved | **8,192 bytes** |
| Tổng SRAM footprint kể cả stack/padding | **9,432 /131,072 bytes**, khoảng7.20% |
| CCM sử dụng | 0 /65,536 bytes |
| Vector table | 98 entries /392 bytes tại `0x08000000` |
| Initial MSP / reset vector | `0x20020000` / `0x08001EFD` (Thumb) |
| Stack lower budget limit | `0x2001E000`; static overlap assertion PASS |

BIN SHA-256:
`1a3c1691c7a9c9c648f3ced7c9d3dfbce8229a041ae5762f7fd19e370e062062`.

ELF PT_LOAD tái dựng khớp BIN; mọi vector nonzero là Thumb address trong image;
allocated sections nằm trong VG regions. Section sum9428 bytes cộng4 bytes
alignment thành footprint9432; không nhầm linker padding với data. `.su` lớn
nhất theo từng function là `model_check`784 bytes; đây **không phải** call-chain
peak hoặc interrupt-nesting stack đo thực tế. 8 KiB là budget, chưa xác minh high-water.

Legacy VE BIN SHA-256:
`70a59841b44cf36b23a1c3be64fe066e3e584efa0f44fd95213a08fb050ea9e7`,
trùng Phase2C. Không đổi linker512 KiB hoặc clock16 MHz của target cũ.

## 7. Timing instrumentation và test plan trên board

Main loop chạy Core trước LED/checkpoint rồi WFI; không UART TX/RX, queue hoặc
traffic UI trong target này. Core deadline10 ms giữ nguyên; fault nếu bỏ lỡ slot,
không chạy catch-up vô hạn. Không có shared mutable Core owner giữa main và ISR.
SysTick chỉ cập nhật tick; 32-bit HAL tick được mở rộng uint64 ở main owner.

`discovery_timing`: DWT CYCCNT ở clock cấu hình168 MHz, CORE/LOOP last/max/sample/
over-budget và lateness. Phạm vi CORE của target này gồm **demo providers +Core
poll +model/button processing**, rộng hơn scope Core-only ở BSP cũ. LOOP thêm
LED/checkpoint, loại WFI; boot self-test không nằm trong mẫu runtime này.
Budgets kế thừa là mục tiêu **CORE<1 ms, owner LOOP<10 ms**, không phải kết quả đo.
DWT32-bit wrap khoảng25.565 s tại168 MHz; delta hợp lệ khi mẫu không kéo dài qua
nhiều wrap. Counter unavailable phải được báo, không đổi simulation thành hardware PASS.

Sau khi đủ điều kiện và được người dùng cho phép flash ở phiên sau:

1. Ghi firmware identity/UID/SN/reset flags, xác nhận HSE/MCO và clock sau boot;
   đo rail/clock thực. Sai clock hoặc boot fault thì STOP, không tiếp tục các mode.
2. Chạy SAFE và từng mode ít nhất60 s; ghi tick count/missed/fault, last/max cycles,
   over-budget, lateness, watchdog refresh và stack remaining; thử reset/held USER.
3. Đối chiếu nút/LED với timestamps, model1/2, failsafe/input validity/telemetry
   expiry/channel clamp. Không có RF/actuator kết nối.
4. Đo WCET dưới model switch, button bounce, input loss/recovery; tách thời gian
   ISR, boot selftests, runtime và debugger overhead. Không freeze watchdog để
   làm số đo đẹp; debugger halt hiện có thể gây IWDG reset.
5. Kiểm tra watchdog/recovery trong phiên hardware riêng có phê duyệt; giữ backup
   và SWD. Chưa có kết quả đo nào trong các bước này.

Native tests kiểm tra logic/timing wrap/budgets/schedule với clock giả lập, không
đo tốc độ168 MHz, chất lượng oscillator, jitter hoặc ISR latency trên Discovery.

## 8. File changes và Git/WIP

Modified duy nhất: root `CMakeLists.txt`, thêm option `TYVOR_BUILD_DISCOVERY_TESTS`
defaultOFF và registration21 test mới; không thay inventory/build cũ mặc định.

New:

- `stm32/boards/stm32f4_discovery/`: `CMakeLists.txt`, `STM32F407VGT6.ld`,
  `stm32f4xx_hal_conf.h`, `discovery_board.c/.h`, `discovery_demo.c/.h`, `main.c`.
- `sim/test_discovery.c`.
- `scripts/build-discovery.ps1`, `discovery_artifacts.py`, `discovery_programmer.py`,
  `test_discovery_platform.py`, `test_discovery_tools.py`.
- Báo cáo này và `docs/STM32F4_DISCOVERY_FLASH_GUIDE.md`.

Git root vẫn ở thư mục cha; workspace được Git báo `?? ./`, nên tracked diff
không đại diện đầy đủ WIP. Không stage/commit hoặc tạo baseline Git giả. Frozen
126-file snapshot được dùng để tạo `evidence/discovery-diff.patch` (full contents
file mới), `source-changes.json`, `baseline-preservation.json`; manifest bind
artifact hashes và current source hashes. Các file generated nằm trong ignored
`build/` và `artifacts/`; package Discovery tách biệt package lịch sử.

## 9. Điều kiện cho phép flash lần đầu / rủi ro còn lại

**Chưa đủ điều kiện flash.** Cần hoàn tất tất cả:

1. Driver ST-LINK debug hoạt động, CLI official có sẵn và đúng probe serial.
2. Ảnh board/suffix/MCU marking khớp; jumper/power/boot/clock actual được kiểm tra,
   không dùng pinout Mini Type-C và không đổi clock bằng suy đoán.
3. Read-only ID/Flash1024 KiB/RDP/nWRP/UID/SN hợp lệ; hai bản đọc full Flash1 MiB
   hash giống nhau, option bytes không thay đổi, backup lưu riêng. Mọi lỗi đọc/
   protection/identity đều BLOCKED, không reset/unprotect/erase fallback.
4. Package Discovery đúng board, BIN/address/vector/SHA kiểm tra lại; người dùng
   phê duyệt **ghi đè firmware gốc** sau khi đã review backup và recovery plan.
5. Phiên flash thủ công riêng, sau phê duyệt; bản chạy Codex này chỉ chuẩn bị
   script/hướng dẫn và dừng. `flash-plan` chỉ in command, không thực thi.

Quy trình, record template và commands trong
[STM32F4_DISCOVERY_FLASH_GUIDE.md](STM32F4_DISCOVERY_FLASH_GUIDE.md).
Rủi ro mở: actual assembly/MCO/rails chưa xác minh; firmware gốc chưa biết;
WCET/stack/IWDG/reset/LED/button chưa kiểm tra trên board. UART/ESP bench integration
chưa có trong Discovery demo; protocol v2 regression vẫn PASS ở native.

**Kết luận cuối: phần mềm sẵn sàng để review; hardware bring-up / flash NOT READY,
BLOCKED ở host debug driver + tool và hồ sơ nhận diện/backup. Dừng chờ review.**
