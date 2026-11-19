# TYVOR OS V1 — Phase 1 final review

Ngày: 09/11/2026, Asia/Bangkok. Workspace: `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

## Kết luận: NOT READY

**Chưa nên chốt Phase 1 làm baseline tích hợp cho Phase 2A.** Hai vấn đề HIGH còn tồn tại trong recovery của protocol: frame mới có thể bị mất khi deadline của header lỗi cũ hết hạn; sender reboot có thể làm link từ chối dữ liệu hợp lệ trong nhiều giây. Cần sửa và bổ sung regression test trước lần review tiếp theo. Không sửa các vấn đề đó trong lần review này.

Build artifact, CRC, layout STM32, partition ESP32 và toàn bộ SHA-256 đã kiểm tra đều đạt. Chạy lại bộ test hiện có đạt **33/33 Debug và 33/33 Release**. Các kết quả PASS này không bao phủ hai tình huống HIGH vừa tái hiện. Không phát hiện đường xử lý ESP32 trực tiếp quyết định ARM/RF/outputs trong source hiện tại.

Phân loại: **0 BLOCKER, 2 HIGH, 2 MEDIUM, 2 LOW**. BLOCKER dành cho lỗi làm hỏng baseline kiểm chứng hoặc đường điều khiển nguy hiểm đã xác định; HIGH là lỗi correctness/availability phải xử lý trước tích hợp. Không cần có BLOCKER mới kết luận NOT READY.

Chưa có định nghĩa Phase 2A riêng trong các tài liệu được cung cấp; kết luận xét khả năng dùng baseline này để tích hợp UART/UI hai MCU. Việc đọc datasheet hoặc nghiên cứu UI offline không bị coi là đã vượt qua cổng review này.

## 1. Phạm vi, Git diff và tính bất biến

Đã đọc `INITIAL_AUDIT_2026-11-09.md`, `PHASE1_IMPLEMENTATION_REPORT.md`, manifest, toàn bộ 36 tệp source/docs/scripts của snapshot Phase 1, các config/build metadata liên quan và full-tree export patch.

- Git root: `D:\TYVOR\TYVOR-OS-V1-Starter`.
- Đầu review có **36 tệp untracked**. `git diff --stat` và `git diff --cached --stat` đều rỗng vì starter chưa có tracked baseline, không phải vì không có thay đổi.
- `artifacts/phase1/evidence/phase1-review.patch` chứa 36 tệp dưới dạng new-file diff. Đã tái tạo diff read-only bằng `git diff --no-index -- NUL <file>` và đối chiếu byte-for-byte: **khớp toàn bộ patch hiện tại**.
- Vì không có tracked baseline, không thể chứng minh riêng con số “12 sửa/22 mới” bằng lịch sử Git; đó là thông tin từ audit/implementation report. Review thực hiện trên toàn cây hiện tại, không giả lập commit hoặc staging để tạo diff.
- Không sửa firmware, CMake, test, script, config, SVG hoặc artifact đã đóng gói. Không chạy lại script package/build làm thay manifest. Chỉ thêm báo cáo này vào `docs/`; probe executable và ảnh tổng hợp phục vụ review nằm trong `build/final-review/`, được Git ignore.
- Không flash, mở serial, commit, push, deploy hay sửa Git global. Không kiểm tra board hoặc sửa firmware STABLE/EdgeTX cũ.

## 2. Phát hiện cần xử lý

### H1 — HIGH: timeout của header cũ làm mất frame mới đang nhận

**Vị trí:** `shared/tyvor_protocol.c:83`, `:91`, `:110`; `sim/test_protocol.c:151`, `:174`.

Khi `tyvor_parser_poll` phát hiện timeout, `drain(..., expired=1, ...)` bỏ mọi candidate chưa đủ byte trong suffix. Nó không phân biệt phần dữ liệu mới đến gần đây với header cũ đã hết hạn. Một frame hợp lệ bắt đầu ở t=49 ms có thể bị xóa tại t=50 ms do header giả bắt đầu ở t=0.

Đã biên dịch probe C từ stdin bằng Clang, link **trực tiếp source `shared/tyvor_protocol.c` hiện tại**, không dùng mô hình JavaScript hoặc sửa source/test repo. Lịch input:

```text
H = 54 59 01 04 00 20                       # DIAGNOSTIC length=32 giả
F = 54 59 01 01 07 06 00 00 A4 1F 63 07 29 62  # STATUS hợp lệ

t= 0 ms: feed H
t=49 ms: feed 7 byte đầu F
t=50 ms: poll timeout
t=51 ms: feed 7 byte cuối F
```

Kết quả thực:

```text
partial_new_frame_across_old_timeout: received=0 used=0 timeouts=1
```

F hoàn tất chỉ 2 ms sau khi bắt đầu nhưng không được giao cho callback. Case `false_length` hiện có chỉ chứa các frame đã đủ byte khi timeout; case `absolute_deadline` chỉ kiểm tra một candidate, nên cả hai vẫn PASS.

**Ảnh hưởng:** mất STATUS hợp lệ trong recovery sau nhiễu/header giả, phụ thuộc thời điểm chia chunk. Không có buffer overflow được quan sát và trạng thái vẫn fail-invalid khi stale, nhưng yêu cầu parser chống nuốt frame chưa đạt cho trường hợp này.

**Yêu cầu trước review lại:** giữ được suffix/candidate mới phù hợp khi header cũ hết hạn, có quy tắc deadline rõ ràng; thêm regression tại các điểm chia frame và ngay trước/đúng/sau biên timeout. Không được chỉ đổi kỳ vọng test thành “mất frame là PASS”.

### H2 — HIGH: chưa có recovery session khi sender reboot

**Vị trí:** `shared/tyvor_protocol.c:66`, `:131`; `stm32/Core/Src/tyvor_bridge.c:6`; `esp32/main/main.c:53`, `:76`. Giới hạn này đã được công khai trong `docs/PROTOCOL_PHASE1.md`, không phải phát hiện một tính năng được tuyên bố đã triển khai.

Receiver giữ baseline sequence qua timeout/lỗi transport; sender khởi động lại sequence từ 0. Với baseline=128, receiver từ chối sequence 0..127 và coi 128 là duplicate; sequence 129 mới được nhận.

Probe C thực đã chạy:

```text
sender_reboot_after_sequence128:
  rejected=128 duplicate=1 delivered_since_reboot=0
sender_reboot_first_accepted_seq129:
  delivered_since_reboot=1
```

Ở publish 10 Hz, khoảng từ frame reboot đầu tiên đến frame được nhận lại là **12,9 giây** trong lịch probe. Với stream request 1 Hz, cùng cửa sổ sequence có thể chặn request khoảng **129 giây**. Đây là tính toán/lịch native, chưa đo trên UART thật.

**Ảnh hưởng:** reset riêng STM32 hoặc ESP có thể làm đường dữ liệu/request mất khả dụng kéo dài dù byte và CRC đều đúng. STATUS chuyển INVALID sau 2 giây theo thiết kế; không có bằng chứng ARM/RF bị thực thi sai.

**Yêu cầu:** đặc tả/chứng minh cơ chế session/boot identity hoặc resynchronization rõ ràng cho single-peer reboot và mất >=128 frame. Giữ bảo vệ duplicate/freshness; không đơn giản xóa history khi stale để frame cũ có thể làm sống lại telemetry. Thêm test reboot từng phía, wrap, long loss, duplicate và delayed packet.

### M1 — MEDIUM: metadata trong gói ESP trỏ sai vị trí bootloader/partition

**Vị trí:** `scripts/package_artifacts.py:32`, `:34`, `:35`; `artifacts/phase1/esp32/flasher_args.json`.

SDK metadata được copy nguyên nhưng binary được đóng gói phẳng:

| Path tương đối trong metadata | Tồn tại trong thư mục gói ESP? | Binary thực |
|---|---|---|
| `bootloader/bootloader.bin` | **Không** | `bootloader.bin` |
| `partition_table/partition-table.bin` | **Không** | `partition-table.bin` |
| `tyvor_display.bin` | Có | `tyvor_display.bin` |

Đã kiểm tra file existence thực tế. Các binary và SHA đúng; lỗi là sự tương ứng đường dẫn khi dùng gói độc lập với build tree. Không thử flash để kiểm tra lỗi này.

**Yêu cầu:** bảo toàn cấu trúc SDK hoặc cập nhật nhất quán mọi đường dẫn metadata trong bước package; kiểm tra tất cả reference resolve được, offset/image khớp rồi tạo manifest mới ở lần triển khai được phép sửa.

### M2 — MEDIUM: package chưa buộc test/build evidence tương ứng source được ký hash

**Vị trí:** `scripts/package_artifacts.py:29`, `:43`, `:59`.

Script copy binary/log có sẵn, chỉ kiểm tra `failures/errors` trong XML rồi hash source hiện tại. Nó không kiểm tra số test=33, skipped=0, test inventory, freshness/build inputs hay quan hệ source revision với binary. Một XML có tests=0, failures=0 cũng qua điều kiện hiện tại; source thay đổi sau build vẫn có thể được đưa vào manifest cạnh binary cũ.

Đây là kết luận static review về cơ chế đóng gói, **không phải khẳng định gói hiện tại có binary cũ hoặc test giả**. Gói hiện tại đã được kiểm tra độc lập, rerun 66 test và đối chiếu source hash, ELF/BIN cùng build outputs.

**Yêu cầu:** thêm build/test provenance và acceptance checks đầy đủ trước khi dùng manifest làm cổng release: đúng inventory/count, không skipped, đúng cấu hình/compiler flags, build input identity và binary tương ứng. Hash file chỉ chứng minh tính toàn vẹn của file đó.

### L1 — LOW: chưa có tracked Git baseline để review thay đổi theo lịch sử

**Bằng chứng:** toàn bộ 36 tệp đầu review là `??`; tracked/staged diff đều rỗng. Full-tree export và manifest cho phép review snapshot, nhưng không thay thế lịch sử trước/sau của Phase 1.

Người dùng cần quyết định việc đưa source vào quản lý phiên bản ở bước riêng sau review. Không thực hiện stage/commit để “sửa” điều này trong phiên review.

### L2 — LOW: counter UART STM32 có thể mất increment giữa main và ISR

**Vị trí:** `stm32/BSP/board.c:71`, `:101`; `stm32/BSP/board.h:6`.

`board_diagnostics.uart_errors` được tăng cả trong main TX error path và USART2 ISR. `volatile` không làm phép `++` trở thành atomic. Ví dụ main load 7, ISR lưu 8, main tiếp tục lưu 8 làm mất một lần tăng.

Ảnh hưởng giới hạn ở độ chính xác diagnostics; parser/cache/control state không dùng counter này để quyết định output. Nhánh UART hiện OFF trong firmware mặc định. Đây là nguy cơ static khi bật UART, **chưa quan sát trên board**.

**Đề xuất:** counter riêng theo owner hoặc cập nhật atomic/critical section phù hợp, trước khi dùng thống kê lỗi làm tiêu chí nghiệm thu tích hợp.

## 3. Xác minh 33 test Debug + 33 test Release

Đã chạy lại executable hiện có qua:

```powershell
.tools\cmake-3.30.9-windows-x86_64\bin\ctest.exe --test-dir build/native-debug --output-on-failure
.tools\cmake-3.30.9-windows-x86_64\bin\ctest.exe --test-dir build/native-release --output-on-failure
```

| Cấu hình | PASS | FAIL | Exit code |
|---|---:|---:|---:|
| Debug | 33 | 0 | 0 |
| Release | 33 | 0 | 0 |

Packaged JUnit ở `artifacts/phase1/evidence/native-{debug,release}.xml` đều ghi tests=33, failures=0, skipped=0. Đã đọc `compile_commands.json`: Debug không có `-DNDEBUG`, Release có `-DNDEBUG`. CHECK luôn thực thi; không còn assert có side effect trong test.

Packaged `native/test_protocol.exe` byte-for-byte khớp executable Release vừa chạy. Source hash khớp snapshot Phase 1. Không rebuild firmware MCU hoặc thay binary trong lần review này.

Các nhóm CRC/null/length/semantic/noise/nested frame/coalescing/wrap/duplicate/stale/TX callback đều có test đạt. H1/H2 không được bộ 33 test kiểm chứng đầy đủ. Probe reviewer là kiểm tra bổ sung, không tăng con số 33 hay sửa kết quả bộ test gốc.

## 4. STM32 ELF/BIN/MAP và memory layout

Đã dùng `arm-none-eabi-readelf`, `size`, `objdump`, parse ELF với pyelftools và kiểm tra vector/binary:

| Kiểm tra | Bằng chứng |
|---|---|
| Kiến trúc | ELF32 little-endian ARM, v7E-M/Thumb-2, VFPv4, hard-float ABI |
| Flash VE | Map: origin `0x08000000`, length `0x80000` = 512 KiB |
| SRAM | Origin `0x20000000`, length `0x20000` = 128 KiB |
| CCM | Origin `0x10000000`, length `0x10000` = 64 KiB; dùng 0 byte |
| Stack | `_estack=0x20020000`; reserve 4096 byte |
| Vector | 98 entry; SP đúng; reset/ELF entry `0x080009C5`; các handler khác là zero hoặc Thumb address trong Flash |
| Flash payload | Dựng lại BIN từ các flash PT_LOAD segment của ELF: **khớp byte-for-byte**, 5428 byte |
| RAM tĩnh | `.data=12`, `.bss=232`, `.stack_reserve=4096`, tổng region tính alignment 4344 byte |
| Artifact | ELF 997028 byte; BIN 5428 byte; MAP có đủ memory configuration |

Layout phù hợp [ST STM32F407VE](https://www.st.com/en/microcontrollers-microprocessors/stm32f407ve.html) và [datasheet DS8626](https://www.st.com/resource/en/datasheet/stm32f407ve.pdf): không dùng giới hạn Flash 1 MiB của VG. 128 KiB SRAM + 64 KiB CCM là các vùng khác nhau; backup SRAM chưa sử dụng. Chưa xác minh MCU trên board thật.

`.stack_reserve` là allowance tối thiểu trong linker; không phải đo peak stack runtime. Clock HSI-only, SysTick, IWDG và UART chỉ được review source/build evidence, không đo thời gian. Cache build ghi UART candidate OFF, IWDG ON. Disassembly của `board_uart_write` mặc định chỉ tăng tx_suppressed/trả lỗi; không gọi HAL TX. Weak USART2 handler trong ELF mặc định không phải nhánh UART candidate đã bật.

## 5. ESP artifact, partition, Flash và PSRAM

Đã chạy **chỉ đọc file** với esptool 4.13.0 `image_info --version 2` cho app và bootloader; decode/verify partition binary bằng `gen_esp32part.py`.

- App ELF là `EM_XTENSA`, entry `0x40375430`.
- App BIN 210608 byte và bootloader BIN 22336 byte đều là chip ID 9 / ESP32-S3, header Flash 16 MB, 80 MHz, checksum/hash nội bộ hợp lệ, IDF v5.5.1.
- App descriptor chứa ELF SHA-256 `23a0b44174f7bcb8a0c6403d52a72e11e54b8fdb996666c291662b241c73b93c`, khớp ELF trong gói và manifest.
- Packaged ELF/BIN/MAP, bootloader, partition và sdkconfig đều khớp file hiện có trong build/source tree tương ứng.
- Header DIO không phải lỗi QIO config: SDK v5.5.1 `components/esptool_py/Kconfig.projbuild:76` quy định bootloader/header DIO cho lựa chọn QIO; bootloader nâng sang quad mode khi khởi tạo. `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y` trong config đúng với cơ chế này.

| Partition | Offset | Size | End exclusive |
|---|---:|---:|---:|
| nvs | 0x9000 | 0x6000 | 0xF000 |
| phy_init | 0xF000 | 0x1000 | 0x10000 |
| factory | 0x10000 | 0x300000 | 0x310000 |
| ui_assets | 0x310000 | 0xC00000 | 0xF10000 |
| logs | 0xF10000 | 0xF0000 | 0x1000000 |

Partition table 3072 byte hợp lệ, không overlap, kết thúc đúng biên Flash 16 MiB. Bootloader/partition/app offset trong metadata là 0x0/0x8000/0x10000; app nằm gọn factory partition. Đây không phải xác minh dung lượng Flash vật lý bằng board.

**PSRAM 8 MB chưa được xác nhận.** Generated config bật octal PSRAM 40 MHz, `TYPE_AUTO`, malloc integration, memtest và `IGNORE_NOTFOUND=y`; không có kiểm tra app bắt buộc detected size phải bằng 8388608. Boot log code có báo initialized/size, nhưng chưa boot board. Không thể suy dung lượng PSRAM thực từ app BIN hoặc tên N16R8.

Điều kiện trước dùng LVGL/framebuffer: xác nhận đúng revision và log detected PSRAM 8 MiB, memtest/heap allocation, console/clock thực; chốt cách xử lý thiếu PSRAM. Missing PSRAM được dung nạp có chủ đích cho mock bench hiện tại, không được gọi là PSRAM hardware PASS.

## 6. CRC, timeout, sequence, stale và TX

CRC implementation đúng CCITT-FALSE: polynomial 0x1021, init 0xFFFF, không reflect/xor cuối; wire CRC low byte trước. Golden `123456789=0x29B1`; STATUS mock có CRC 0x6229. CRC bao phủ version/type/sequence/length/payload; sync được kiểm tra riêng.

Decode kiểm tra pointer, minimum/exact length, version/type/schema, maximum payload, CRC và semantic trước copy output. STATUS boolean/flags/LQ/invalid-value rules có hiệu lực; mock không claim ARM valid. Không phát hiện copy vượt buffer trong các đường đã review/test. Encode không hỗ trợ buffer overlap theo hợp đồng tài liệu.

Parser buffer 40 byte và rescan suffix giải quyết được nhiều case lỗi đã test; valid outer frame giữ quyền sở hữu payload. Tuy nhiên timeout/recovery chưa đạt H1, và sequence recovery sau reboot chưa đạt H2. CRC/sequence không phải cơ chế authentication/anti-replay session.

Snapshot có `received_once`, `received_ms`, `valid` tách biệt: timestamp 0 hợp lệ. Khi age >=2000 ms hoặc clock regression, status/flags bị xóa và valid=0. Duplicate, ACK và diagnostic không cập nhật STATUS freshness. Timestamp là thời gian local parser delivery, không phải chứng cứ flight-controller telemetry mới.

STM32 TX callback đồng bộ, lifetime packet đúng trong callback; negative/partial result được trả lỗi và đếm, không retry vô hạn. BSP TX deadline 5 ms cần đo thực tế. ESP kiểm tra số byte FIFO copy, đếm gửi thiếu/lỗi; không có ACK transaction/retry completion, đúng giới hạn read-only hiện tại.

## 7. Race condition và ownership

**ESP32: chưa phát hiện data race cache/parser/UI trong source hiện tại**, dựa trên các đường truy cập sau, không phải kết quả race detector trên RTOS:

- Cache/parser được init trước tạo task (`esp32/main/main.c:124`). `uart_ready` được set trước tạo hai task.
- Bridge là owner của feed/poll/TX; callback `received` chỉ được gọi từ feed/poll khi bridge đang giữ `cache_lock` (`:57`, `:72`, `:83`).
- Snapshot, parser stats và tx_errors được diagnostics copy dưới cùng mutex (`:96`); tránh đọc rách timestamp 64-bit trên MCU 32-bit.
- Sau unlock, renderer dùng snapshot/view local, poll stale trên bản copy và log ngoài lock (`:99`). Không sửa cache bằng UI.
- UART ISR SDK không gọi parser/cache callback trực tiếp. Không tìm thấy đường lấy khóa theo thứ tự ngược gây deadlock hiện tại. Lock vẫn bao quanh UART operations; khi thêm UI/driver khác phải giữ nguyên ownership và đánh giá latency thay vì chỉ dựa vào mutex.

STM32 ring có ISR producer/main consumer, main drain/loss-reset trong critical section có restore PRIMASK; parser/bridge chỉ ở main. Ngoại lệ diagnostics counter là L2. Không có task UI chạy trên STM32; timing control thật chưa tồn tại để đo/khẳng định đáp ứng realtime.

## 8. UI mock 21 SVG

Đã parse XML, kiểm tra canvas, title, banner và giá trị telemetry của **toàn bộ 21 tệp**. Đã render bằng Chrome headless CPU và xem trực tiếp contact sheet `build/final-review/ui-sheet.png`, không chỉ đọc tên file.

| Page | Title | Mock / Stale / No-data |
|---:|---|---|
| 0 | HOME | Đủ 3 |
| 1 | MODEL | Đủ 3 |
| 2 | INPUTS | Đủ 3 |
| 3 | MIXES | Đủ 3 |
| 4 | OUTPUTS | Đủ 3 |
| 5 | TELEMETRY | Đủ 3 |
| 6 | RADIO SETTINGS | Đủ 3 |

Tất cả có kích thước 368×448, banner đúng MOCK / READ ONLY, STALE / INVALID hoặc NO DATA / INVALID. HOME/Telemetry luôn ARM UNKNOWN với mock; pin/LQ là 8100 mV/99% ở mock và UNKNOWN khi invalid. Không thấy text bị cắt/đè trong contact sheet. Inputs/Mixes/Outputs vẫn là fixture có nhãn MOCK khi link invalid, không thể gọi là kênh ADC/output thật.

Đây là SVG tĩnh, chưa có interactive navigation, LVGL, panel hoặc touch; không đánh giá trải nghiệm trên AMOLED thực. Không có pinout panel/touch mới được suy đoán trong review.

## 9. Ranh giới ARM/RF/outputs

Đã trace từ ESP TX đến STM RX/dispatcher, không chỉ tìm từ khóa:

1. ESP tạo duy nhất request `[TYVOR_REQ_MODEL_INFO,0]` ở `esp32/main/main.c:75`; không có opcode ARM/RF/output trong allowlist.
2. Shared schema chỉ nhận STATUS, hai read-only UI_REQUEST, ACK và opaque DIAGNOSTIC.
3. STM dispatcher `stm32/Core/Src/tyvor_bridge.c:30` chỉ xử lý UI_REQUEST; nhận STATUS/ACK/DIAGNOSTIC không cập nhật điều khiển. Request hợp lệ chỉ tạo ACK unsupported, có rate limit.
4. STM main tự tạo mock STATUS với arm_valid=0/arm=0; dữ liệu này không lấy từ ESP và không đi qua mixer/output engine.
5. Không có RF/CRSF TX, PWM/channel output, ARM actuator hoặc function pointer dispatch tới logic đó. GPIO candidate của BSP chỉ phục vụ UART và bị gate OFF mặc định.

**PASS trong phạm vi code hiện có: không có đường xử lý cho phép ESP trực tiếp quyết định ARM/RF/outputs.** Đây không phải chứng minh an toàn điện, cách ly phần cứng hoặc firmware dùng bay; khi thêm write commands phải review lại ranh giới.

## 10. SHA-256 và điều kiện review lại

Đã tính lại SHA-256 cho **59 artifact và 36 tệp source được manifest liệt kê**, kiểm tra size khi có: **0 mismatch, 0 missing**.

Manifest SHA-256 lúc review:

```text
3e9ea52671c9accfde0c8aa371b04561d0d28bcb43d560b06078d7075bf287ab
```

Hai binary chính:

```text
STM32 BIN  248557c51524254a585b89819b4951a5021144a72d4e08e17bad6a0a84330feb
ESP32 BIN  bc8fe1dce6ee662a81a60d0b6a6de2ef0d72598c5c5648286d1041cf08b894cd
```

Báo cáo final review này là tệp mới ngoài snapshot 36 source của Phase 1; không cập nhật manifest để tự đưa báo cáo vào snapshot đang được review. Source/artifact được manifest bảo vệ phải tiếp tục không đổi sau review.

Điều kiện để chuyển kết luận sang READY:

1. Sửa H1 và H2 trong một công việc triển khai riêng, có regression test mới và giữ toàn bộ test cũ ở Debug/Release.
2. Sửa M1, bổ sung acceptance/provenance cho M2, build/package lại và kiểm tra lại manifest cùng các reference trong metadata.
3. Giữ UART gates OFF cho đến khi có xác nhận board/pin/điện áp; xác nhận runtime PSRAM/clock/watchdog/UART ở một giai đoạn hardware được yêu cầu riêng. Review này không cho phép flash.
4. Khi baseline thay đổi, review lại source-to-artifact correspondence và ranh giới request; không chuyển RF/ARM authority sang ESP.

Đã dừng ở báo cáo review. Không sửa source để khắc phục phát hiện, không tự tiếp tục Phase 2A.
