# TYVOR OS V1 — Phase 1 implementation report

Ngày 09/11/2026, Asia/Bangkok. Workspace: `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Kết quả: đã triển khai baseline Phase 1, build native/STM32/ESP32 và chạy test thật. Trạng thái WIP, bench-only, chờ người dùng review. Chưa flash hoặc kiểm tra board.**

## 1. Kết quả chính

| Hạng mục | Kết quả thực tế |
|---|---|
| Native Debug | Build PASS; CTest **33 PASS / 0 FAIL** |
| Native Release | Build PASS; CTest **33 PASS / 0 FAIL**, xác nhận lệnh compile test có `-DNDEBUG` |
| Tổng lượt test cuối | **66 PASS / 0 FAIL**, cùng 33 case ở hai cấu hình; không tính các lượt chạy trước vào tổng này |
| STM32F407VET6 | Cross-compile/link PASS, ELF/BIN/MAP; dùng `STM32F407xx`, Cortex-M4F hard-float và linker 512 KiB Flash |
| Nhánh BSP UART có điều kiện | Compile-check PASS trong object riêng, không link vào ELF mặc định và không xác nhận wiring |
| ESP32-S3 | ESP-IDF 5.5.1 configure/build/size PASS; app ELF/BIN/MAP, bootloader, partition table |
| ESP image inspection | esptool `image_info` PASS, checksum và validation hash hợp lệ; chỉ đọc file, không mở thiết bị |
| STM32 vector inspection | SP=`0x20020000`; Reset=`0x080009C5`, Thumb bit đúng, nằm trong Flash VE |
| UI virtual display | Xuất **21 SVG**, 7 trang × mock/stale/no-data; tất cả parse XML được và có canvas 368×448 |
| Dependency cache | VerifyOnly PASS: SHA-256 và installed marker của **19 archive** |
| Hardware / UART hai board / PSRAM thật / panel / touch / watchdog timing | **CHƯA KIỂM TRA** |

Không đồng nhất compile/link PASS với boot PASS, wiring đúng hay tính năng radio an toàn. Hai build mặc định đều khóa UART vì pinout chưa được xác nhận.

## 2. File đã sửa và tạo

So với starter và báo cáo audit đầu phiên: **12 tệp sửa, 22 tệp mới**, 2 tệp tài liệu gốc giữ nguyên. Không tính `.tools`, `build`, `artifacts` hoặc generated `esp32/sdkconfig`.

Tệp sửa:

- `.gitignore`: loại tools/build/artifacts/generated config khỏi Git.
- `CMakeLists.txt`: thư viện UI portable, simulator, 33 CTest case, cảnh báo compiler nghiêm ngặt.
- `README.md`: trạng thái Phase 1, cách build và giới hạn phần cứng hiện tại.
- `shared/tyvor_protocol.h`, `shared/tyvor_protocol.c`: schema validation, parser streaming, timeout/recovery, sequence và counters.
- `shared/tyvor_status.h`: semantic validation, NULL/length checks, snapshot timestamp/validity/invalidation.
- `stm32/Core/Inc/tyvor_bridge.h`, `stm32/Core/Src/tyvor_bridge.c`: bridge có instance, RX streaming, TX result/error/partial counters, read-only ACK rate limit.
- `sim/test_protocol.c`: bỏ assert có side effect, dùng CHECK luôn thực thi.
- `esp32/CMakeLists.txt`, `esp32/main/CMakeLists.txt`, `esp32/main/main.c`: target S3, dependency closure UART-only, tasks/cache/UART diagnostics.

Tệp mới:

- `cmake/arm-none-eabi.cmake`.
- `stm32/CMakeLists.txt`, `stm32/BSP/STM32F407VET6.ld`, `stm32/BSP/board.h`, `stm32/BSP/board.c`, `stm32/Core/Inc/stm32f4xx_hal_conf.h`, `stm32/Core/Src/main.c`.
- `esp32/sdkconfig.defaults`, `esp32/partitions.csv`, `esp32/main/Kconfig.projbuild`.
- `shared/tyvor_ui.h`, `shared/tyvor_ui.c`, `sim/tyvor_ui_sim.c`.
- `scripts/activate.ps1`, `scripts/bootstrap-tools.ps1`, `scripts/build-phase1.ps1`, `scripts/package_artifacts.py`, `scripts/tools.lock.json`, `scripts/esp-idf-python-lock.txt`.
- `docs/PROTOCOL_PHASE1.md`, `docs/BUILD_PHASE1.md`, báo cáo này.

`docs/INITIAL_AUDIT_2026-11-09.md` và `docs/TX15_REFERENCE.md` được giữ nguyên làm tài liệu lịch sử/tham khảo; kế hoạch triển khai hiện hành nằm trong báo cáo này.

## 3. Toolchain đã xác minh

Đầu Phase 1 kiểm tra lại PATH vẫn không có CMake/Ninja/compiler native/ARM/IDF khả dụng, Python chỉ là WindowsApps alias không chạy được. Đã tải/cài portable vào `.tools` trên D:, không thay SDK của dự án khác.

| Công cụ | Phiên bản đã chạy | Đường dẫn tương đối từ workspace |
|---|---|---|
| Git có sẵn | 2.51.2.windows.1 | `D:\App\Git\cmd\git.exe` |
| CMake / CTest | 3.30.9 | `.tools/cmake-3.30.9-windows-x86_64/bin` |
| Ninja | 1.12.1 | `.tools/ninja` |
| Native Clang / LLVM-MinGW | 20.1.7 / 20250613 UCRT x64 | `.tools/llvm-mingw-20250613-ucrt-x86_64/bin` |
| Arm GNU | 14.2.Rel1; GCC 14.2.1 | `.tools/bin/arm-none-eabi-*` |
| CMSIS Core / F4 device / ST HAL | 5.9.0 / 2.6.10 / 1.8.3 | `.tools/sdk` |
| uv | 0.8.22 | `.tools/uv/uv.exe` |
| CPython | 3.11.13, managed standalone distribution | `.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe` |
| ESP-IDF source | v5.5.1 | `.tools/idf-source/esp-idf-5.5.1` |
| IDF Python virtualenv | Python 3.11.13 | `.tools/espressif/python_env/idf5.5_py3.11_env` |
| Xtensa GCC | 14.2.0, esp-14.2.0_20241119 | `.tools/espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin` |
| ULP / ROM symbols | 2.38_20240113 / 20241011 | `.tools/espressif/tools` |
| esptool | 4.13.0 | Trong private IDF Python env |

Activation chỉ đổi environment của process PowerShell. Không sửa Git global hay PATH hệ thống. Lần gọi uv ban đầu đã tạo thêm launcher user-local `C:\Users\Welcome\.local\bin\python3.11.exe`; interpreter thực vẫn nằm trên D: và launcher không được thêm vào PATH. Script bootstrap lưu trong repo dùng `--no-bin --no-registry` cho các lần cài sau.

SHA-256 download và URL upstream được pin trong `scripts/tools.lock.json`; compiler/ULP/ROM ELF khớp thêm manifest `tools/tools.json` của IDF. Python dependency versions được lưu đầy đủ trong lock file. Hướng dẫn: [BUILD_PHASE1.md](BUILD_PHASE1.md).

Giới hạn tái lập: bootstrap VerifyOnly đã chạy với cache hiện có; chưa replay toàn bộ từ máy trống. SDK tối thiểu đủ build app này, chưa cài toàn bộ submodule cho Wi-Fi/Bluetooth/OpenThread. Chưa cam kết binary bit-identical giữa host/path/timestamp khác nhau.

## 4. Protocol và xử lý lỗi đã triển khai

- Giữ framing `TY`, version 1, type, sequence, length 0..32, payload và CRC16 CCITT-FALSE little-endian; đặc tả đầy đủ tại [PROTOCOL_PHASE1.md](PROTOCOL_PHASE1.md).
- Allowlist type/length; STATUS có validity riêng battery/LQ và nhãn MOCK, từ chối boolean/flags/LQ không hợp lệ. Mock không được claim ARM hợp lệ.
- Chặn NULL/capacity lỗi trước khi ghi; decode lỗi không sửa output.
- Parser portable dùng buffer 40 byte. Khi CRC/header/semantic lỗi, giữ suffix để resync; frame hợp lệ có thể chứa cả header/frame trong payload mà không bị tách nhầm.
- Deadline tuyệt đối 50 ms cho frame dở dang. Khi timeout, quét lại suffix để cứu frame đầy đủ nằm sau header giả. Poll hoạt động cả khi UART im lặng.
- Sequence độc lập theo type: duplicate bị drop; wrap 255→0 được chấp nhận; gap 2..127 được chấp nhận và đếm mất packet; delta 128..255 bị coi bất thường và drop.
- Thống kê frames, CRC/header/semantic errors, timeout, overflow, noise, duplicate, sequence anomaly, missing; TX có lỗi/gửi thiếu.
- Snapshot giữ timestamp monotonic cục bộ và valid tách biệt. Tuổi >=2000 ms hoặc clock regression làm INVALID và xóa toàn bộ trường STATUS. Duplicate/ACK/diagnostic không làm mới telemetry.
- STM32 callback TX có hợp đồng đồng bộ, trả exact byte count; không giữ con trỏ stack sau callback. Không retry vô hạn. Request chỉ đọc được allowlist, ACK unsupported được rate limit.

Chưa có session/boot-token handshake. Reset một MCU hoặc mất >=128 packet có thể cần phối hợp reinitialize session, hoặc chờ sequence đi lại vào cửa sổ được chấp nhận. Không reset baseline chỉ vì stale. Đây là giới hạn phải giải quyết trước vận hành thực, không được diễn giải sequence hiện tại thành anti-replay bảo mật.

## 5. Unit test đã chạy

CTest đăng ký 33 test độc lập; CHECK không phụ thuộc `NDEBUG` và lời gọi test không bị tối ưu bỏ như assert cũ. Root target bật `-Wall -Wextra -Wpedantic -Werror`.

Các case: `roundtrip`, `golden_crc`, `crc_error`, `truncated`, `header_error`, `payload_oversize`, `null_buffers`, `output_capacity`, `payload_types`, `semantic_status`, `parser_timeout`, `uart_noise`, `payload_header`, `nested_recovery`, `false_length`, `consecutive`, `truncated_recovery`, `absolute_deadline`, `sequence_half_range`, `split_every_boundary`, `duplicate_sequence`, `sequence_wrap`, `sequence_abnormal`, `independent_sequences`, `stale_snapshot`, `clock_regression`, `transport_overflow`, `max_and_empty`, `crc_all_bits`, `bounded_noise_recovery`, `bridge_tx`, `bridge_requests`, `ui_states`.

Đã kiểm tra mọi điểm chia STATUS, mọi vị trí cắt 1..13 trước một frame mới, bit-flip các byte được CRC bảo vệ, guard byte output, payload 0/32/>32, nhiễu giả ngẫu nhiên 10.000 byte và stale đúng biên timeout. CRC golden vector được đối chiếu độc lập bằng Python `binascii.crc_hqx`.

Bằng chứng: `build/evidence/native-debug.xml`, `native-release.xml`, các log CTest, compile_commands Release có `-DNDEBUG`. Test C thực đã chạy trên Windows; không dùng kết quả mô hình JavaScript của audit làm PASS cho Phase 1.

Chưa chạy test trên MCU, hardware-in-loop, đo timing hoặc fuzzing/sanitizer campaign dài hạn.

## 6. STM32 target và memory

Target dùng ST startup/vector table + SystemInit từ CMSIS-F4 đã pin; HAL được link từ SDK riêng. BSP có HSI 16 MHz, SysTick 1 ms, error/fault handlers, reset reason, IWDG nominal ~2 s và owner-loop health checkpoint. Không giả định tần số crystal bên ngoài.

UART candidate có RX IRQ/ring 256 byte, bounded drain 64 byte/lượt, error/overflow recovery và synchronous HAL TX timeout 5 ms. ISR không parse frame hoặc feed watchdog. Nhánh này compile-check được, nhưng **ELF mặc định không bật UART/GPIO candidate**. Khi UART bị khóa, mock publish trả lỗi và tăng counter theo thiết kế; không có dữ liệu thực được phát ra đường chưa xác minh.

| Thông số STM32 bản mặc định | Giá trị |
|---|---:|
| BIN / Flash load | **5.428 byte** / 524.288 byte, khoảng 1,04% |
| ELF có debug info | **997.028 byte**; không phải dung lượng Flash cần nạp |
| `.text` tổng theo GNU size, gồm vector/exidx | 5.416 byte |
| `.data` | 12 byte |
| `.bss` thực | 232 byte |
| Stack reserve | 4.096 byte |
| RAM linker tính cả reserve/alignment | **4.344 / 131.072 byte**, 3,31% |
| CCM dùng | **0 / 65.536 byte** |

Linker dùng Flash 0x08000000/512 KiB, SRAM 0x20000000/128 KiB, CCM 0x10000000/64 KiB; stack top 0x20020000, theo [ST STM32F407VE datasheet](https://www.st.com/resource/en/datasheet/stm32f407ve.pdf). Không dùng cấu hình Flash 1 MiB của VG và không đặt UART buffer vào CCM. Map/ELF đã được inspect; chưa đọc MCU ID hay memory thực của board.

File `.su`/`stm32-stack-usage.txt` là static per-function stack estimates, không phải đo peak stack/call-chain trên board. TYVOR STM32 hiện không cấp phát heap; watchdog/tick/UART latency chưa đo thực tế.

## 7. ESP32 target và memory

Ứng dụng có bridge task và diagnostics task; bridge sở hữu parser/TX và mutation cache, diagnostics copy snapshot/counter bằng mutex rồi kiểm tra freshness trước render. RX có queue lỗi UART, bounded work và recovery. TX read-only model-info request không block chờ truyền xong. Có Task WDT cho bridge, canary stack, heap poisoning nhẹ và log internal/free/min/largest heap, PSRAM và stack high-water marks.

Config: esp32s3, Flash 16 MB, QIO/80 MHz theo generated sdkconfig, PSRAM octal/40 MHz, USB Serial/JTAG console. PSRAM size được phát hiện/log lúc boot; không xác nhận đã dùng được 8 MB thật. Missing PSRAM được phép tiếp tục trong scaffold bench để báo lỗi. UART mặc định gate OFF, RX/TX=-1. Không có panel/touch driver, Wi-Fi/Bluetooth task hoặc filesystem mount.

Partition: NVS 24 KiB ở 0x9000; phy_init 4 KiB ở 0xF000; factory app 3 MiB ở 0x10000; ui_assets 12 MiB ở 0x310000; logs 960 KiB ở 0xF10000; kết thúc đúng 0x1000000. Hai data partition cuối mới được reserve, chưa được sử dụng. Không có OTA/update implementation.

| Thông số ESP32-S3 | Giá trị từ build/size |
|---|---:|
| App BIN | **210.608 byte** (`0x336B0`) |
| App ELF có debug info | **3.550.420 byte** |
| Bootloader BIN | **22.336 byte** (`0x5740`) |
| Partition table BIN | **3.072 byte** |
| App partition còn trống | **2.935.120 byte**, khoảng 93% |
| Flash Code / Flash Data | 100.186 / 37.404 byte |
| DIRAM | **58.839 / 341.760 byte**, 17,22% |
| Trong DIRAM: text / data / bss | 45.083 / 11.404 / 2.352 byte |
| Vùng IRAM riêng theo IDF size | 16.384 / 16.384 byte |
| RTC SLOW / FAST | 32 / 24 byte |

IRAM 100% trong bảng IDF là vùng riêng được linker/config hiện tại cấp; không có nghĩa toàn bộ RAM MCU hết. Báo cáo này là static size, không gồm heap/stack runtime và framebuffer tương lai. Log monitoring chưa chạy trên board. Một framebuffer RGB565 368×448 cần 329.728 byte; driver/LVGL sau này phải có budget và chiến lược DMA/buffer riêng, không suy rằng PSRAM tự giải quyết display interface.

## 8. UI mock

Shared C view model độc lập LVGL/display driver, compile được ở native và ESP-IDF. Đã chuẩn bị HOME, Model, Inputs, Mixes, Outputs, Telemetry và Radio Settings. Config/input/channel values là fixture được đánh dấu MOCK/read-only; không có engine mixer, settings writer hay hardware output.

Simulator native xuất SVG 368×448 ở `artifacts/ui-mock/`: `mock-0.svg`..`mock-6.svg`, `stale-0.svg`..`stale-6.svg`, `no-data-0.svg`..`no-data-6.svg`. HOME/Telemetry chuyển battery/LQ sang UNKNOWN và ARM UNKNOWN khi snapshot invalid. Đây là virtual screen tĩnh, không phải GUI có tương tác/touch hoặc emulator board.

## 9. Artifact, lỗi đã gặp và giới hạn còn lại

Gói `artifacts/phase1/` chứa STM32 ELF/BIN/MAP, ESP app ELF/BIN/MAP + bootloader/partition table/generated config, native executables, log/JUnit, size/stack evidence. `SHA256_MANIFEST.json` ghi kích thước và SHA-256 từng artifact cùng source hash. `scripts/package_artifacts.py` chỉ đóng gói file đã build; không nạp board.

Lỗi thực tế đã gặp và cách xử lý:

- Network sandbox mặc định chặn download; các bước tải đã được chạy với quyền network được duyệt. GitHub trả HTTP 504 cho Xtensa/ROM pack; mirror chính thức Espressif tải được và hash khớp manifest.
- Full release ZIP IDF khoảng 1,4 GB timeout; không dùng bản thiếu. Đã dùng source ZIP + đúng commit submodule đủ cho dependency closure app này.
- STM32 link ban đầu thiếu `_init` do ST startup gọi newlib constructor initialization khi không dùng CRT startup. Bổ sung `_init/_fini`, link lại PASS; lỗi đầu có trong lịch sử tool của phiên, `build/evidence/stm32-build-fixed.txt` là bằng chứng build sau sửa. Log hiện tại là build cuối.
- PowerShell tách argument toolchain chưa quote và cache configure lỗi; xử lý bằng argument có quote/absolute path, clean configure, rồi script đã được chạy thành công.
- ESP `-B` tương đối ban đầu trỏ ngoài workspace và bị quyền file chặn; sửa build script dùng absolute build path bên trong workspace. Không có source nào được tạo ở đường ngoài đó.
- SDK archive không có Git metadata, parent Git chưa có HEAD commit; CMake lookup lỗi khi đi lên repo cha. Dùng version.txt đúng v5.5.1, PROJECT_VER rõ ràng và process-local Git search ceiling. Không tạo commit giả hoặc chỉnh SDK core để vượt qua build.
- Thiếu esp-mqtt submodule ở bước CMake registration: bổ sung source đúng commit upstream; không bật MQTT/network tính năng trong app.
- Thiếu ROM symbol pack gây cảnh báo gdbinit: đã tải đúng pack, cấu hình `ESP_ROM_ELF_DIR`, reconfigure/build lại PASS.
- ST HAL 1.8.3 có ba cảnh báo unused parameter `Banks` trong flash_ex khi build cho F407. Không sửa vendor hoặc che cảnh báo. Native code và BSP UART compile-check dùng Werror.

Không có build failure chưa xử lý ở ba target cuối. Những phần **chưa hoàn thành/chưa được xác minh**: session/reboot negotiation; giao tiếp UART hai board thực; timing/watchdog/PSRAM/console thực; interactive LVGL UI, AMOLED/touch driver; model storage/editor/mixer/calibration; config-write transactions; ADC/gimbal; telemetry CRSF thật; RF/ARM output; OTA/recovery updater. Không claim các nhóm chức năng tham khảo EdgeTX đã được triển khai chỉ vì có mock page.

## 10. Rủi ro phần cứng, điều kiện Phase 2 và Git WIP

Trước Phase 2 cần review source/protocol và chốt scope; xác minh board revision/schematic, MCU đúng VE, nguồn/level UART, pin routing và xung/tick thực; đo boot/watchdog/runtime stack/heap, UART error injection và single-peer reset. Chỉ sau xác nhận mới thay gate UART. Nếu mapping STM32 khác candidate, phải đổi BSP theo bằng chứng.

Panel cần datasheet đúng SKU/FPC, nguồn/reset, interface QSPI thực, init sequence/format/timing và touch part number/address/interrupt. **Không gán CST820, chân nguồn, QSPI hoặc touch từ suy đoán.** Có thể tiếp tục UI simulator trước khi có panel; driver hardware là công việc sau khi được review và có tài liệu.

Trước bất kỳ vận hành RF thật nào cần thêm control/mixer/output policy, fault containment, model/config authority trên STM32, tests và review riêng. ESP UI/reset/logging không được trở thành điều kiện sống của control loop STM32. Hiện không có RF/ARM implementation để đánh giá an toàn.

Git root vẫn là thư mục cha `D:\TYVOR\TYVOR-OS-V1-Starter`. Tất cả **36 tệp source/docs/scripts hiện là untracked (`??`)**, giống trạng thái starter chưa track từ đầu. `git diff --stat` không có tracked diff; điều này không có nghĩa chưa sửa code. Có thể review file inventory và source hashes trong gói artifact; bản export patch ở evidence biểu diễn toàn cây hiện tại như tệp mới do không có tracked baseline.

Không stage, commit, push, force push, deploy, flash hoặc mở serial. Không sửa Git global, firmware TYVOR V1 STABLE hay repository EdgeTX cũ. Dừng sau đóng gói/kiểm tra artifact để người dùng review; chưa tự chuyển sang Phase 2.
