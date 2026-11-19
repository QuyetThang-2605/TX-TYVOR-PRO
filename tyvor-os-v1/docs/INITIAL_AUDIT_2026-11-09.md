# TYVOR OS V1 — Initial audit

Ngày: 09/11/2026, Asia/Bangkok. Phạm vi: audit, thử công cụ build/test và lập kế hoạch; không triển khai tính năng.

## 1. Kết luận

- Đây là **starter scaffold dùng trên bàn thử**, chưa phải firmware radio hoàn chỉnh.
- `stm32/` chỉ có bridge UART độc lập HAL. **Không có target firmware STM32F407VET6 có thể link/boot** trong cây nguồn hiện tại.
- `esp32/` có cấu trúc ứng dụng ESP-IDF và code nhận/log STATUS. **Chưa xác minh build thành công** vì môi trường hiện tại không có ESP-IDF/toolchain khả dụng. Không có bằng chứng đủ để kết luận code ESP-IDF chắc chắn build lỗi hoặc chắc chắn build được.
- Native CMake có khai báo thư viện protocol, bridge và một unit test. **Chưa build/chạy được unit test C** vì thiếu CMake/CTest/compiler khả dụng.
- Các thiếu sót ưu tiên: parser không có timeout/resync đủ tốt, trạng thái stale chưa được vô hiệu hóa trong dữ liệu, test phụ thuộc `assert`, chưa có board support và cấu hình build tái lập.
- Chưa kiểm tra board, đường UART thực, RF, Flash/PSRAM thực, AMOLED hoặc touch. Không xác nhận pinout hay driver panel.

Ký hiệu kết quả: PASS = thao tác/kiểm tra đã thực hiện đạt; FAIL = thao tác đã chạy và thất bại; BLOCKED = không thể thực hiện bước phụ thuộc do thiếu điều kiện. Static review không thay thế compile, unit test hay kiểm tra phần cứng.

## 2. Phạm vi và cấu trúc dự án

Đã liệt kê và đọc toàn bộ **13 tệp, 12.122 byte** trong firmware source trước khi tạo báo cáo này, gồm README, ba CMakeLists, toàn bộ C/H và tài liệu. Không tìm thấy `AGENTS.md` trong cây source hoặc các thư mục cha đã kiểm tra.

```text
D:\TYVOR\TYVOR-OS-V1-Starter\       <- Git root được git rev-parse trả về
└── tyvor-os-v1\                    <- firmware source / thư mục làm việc
    ├── .gitignore
    ├── README.md
    ├── CMakeLists.txt              <- native C11, không build firmware MCU
    ├── shared\
    │   ├── tyvor_protocol.h
    │   ├── tyvor_protocol.c        <- encode/decode một frame + CRC
    │   └── tyvor_status.h          <- pack/unpack STATUS 6 byte
    ├── stm32\Core\
    │   ├── Inc\tyvor_bridge.h
    │   └── Src\tyvor_bridge.c      <- callback TX, RX bỏ qua request
    ├── esp32\
    │   ├── CMakeLists.txt          <- entrypoint ESP-IDF
    │   └── main\
    │       ├── CMakeLists.txt      <- main + shared protocol, driver/esp_timer
    │       └── main.c             <- UART1, parser byte stream, log STATUS
    ├── sim\test_protocol.c        <- một executable test, không phải UI simulator
    └── docs\TX15_REFERENCE.md     <- feature map và lộ trình dự kiến
```

`git status --short` trước audit trả `?? ./` khi chạy trong firmware source: starter đang chưa được track trong Git root cha. Điều này không có nghĩa source đã được commit. Không init Git, sửa index, sửa Git global hay thao tác remote.

Không có thư viện bên thứ ba được vendor, submodule, manifest dependency/lockfile, preset toolchain hay CI trong 13 tệp. Native dùng C standard library; ESP dùng API ESP-IDF/FreeRTOS. Không thấy LVGL, SDL, CO5300 driver, touch driver, STM32 HAL hoặc CMSIS trong source.

## 3. Môi trường Windows

Host báo Windows NT `10.0.26100.0`, kiến trúc X64. Các phép kiểm tra là read-only; không cài SDK hay đổi PATH.

| Thành phần | Bằng chứng trong phiên audit | Kết luận |
|---|---|---|
| Git | `D:\App\Git\cmd\git.exe`, phiên bản `2.51.2.windows.1` | PASS |
| CMake, CTest, Ninja | `Get-Command` không tìm thấy; tìm tên executable ở các vị trí bên dưới không thấy | Không khả dụng |
| Native compiler/build tool | Không tìm thấy `gcc`, `g++`, `clang`, `clang++`, `cl`, `make`, `mingw32-make` trên PATH; không thấy gcc/clang/cl ở vùng quét | Native build bị chặn |
| Python | PATH trỏ tới `C:\Users\Welcome\AppData\Local\Microsoft\WindowsApps\python.exe`; `python --version` lỗi `The file cannot be accessed by the system`; không có `py` | Python trên PATH không dùng được; chưa xác định interpreter thật ở vị trí khác |
| ARM GNU toolchain | Không tìm thấy `arm-none-eabi-gcc`, `arm-none-eabi-g++`, `arm-none-eabi-size` trên PATH; quét không thấy compiler | Chưa có toolchain khả dụng cho STM32 |
| STM32 SDK/tools | Không thấy CubeIDE/CubeMX/CubeF4 ở các vị trí/registry được kiểm tra; không có `STM32_Programmer_CLI` trên PATH | Chưa xác minh có bản cài dùng được |
| ESP-IDF | Không có `idf.py` trên PATH/vùng quét; không có biến môi trường IDF/ESP liên quan | Chưa có môi trường IDF được kích hoạt/tìm thấy |
| Xtensa tools | Không thấy `xtensa-esp32s3-elf-gcc` trên PATH; quét cả tên này và `xtensa-esp-elf-gcc` không thấy | Chưa có compiler ESP khả dụng |
| esptool | Không có `esptool.py` trên PATH; không có Python khả dụng để kiểm tra module | Chưa xác minh cài đặt |
| WSL | `wsl --list --quiet` trả `Wsl/EnumerateDistros/Service/E_ACCESSDENIED` | Không xác minh được fallback Linux trong phiên này |
| Thông tin OS qua CIM | `Get-CimInstance Win32_OperatingSystem` bị Access denied; lấy version bằng .NET thay thế | Giới hạn quyền đọc CIM, không phải lỗi firmware |

Phạm vi tìm SDK ngoài PATH: liệt kê thư mục gốc C:/D:, `C:\Program Files`, `C:\Program Files (x86)`, `D:\App`, `D:\Program Files`, `C:\app`, Local Programs; kiểm tra các vị trí thông dụng `C:/D:/Espressif`, `C:/D:/ST`, `C:/D:/msys64`, `C:/MinGW`, `%USERPROFILE%/.espressif`, `esp`, `.platformio`, WinGet Packages. Không thấy các thư mục SDK thông dụng đó tồn tại, ngoại trừ các thư mục ứng dụng chung đã liệt kê.

Đã tìm tên executable/header bằng `rg --files --hidden --no-ignore` trong các vùng tồn tại: `.codex`, `.vscode`, Local Programs, `D:\Android`, Flutter, MSBuild, Downloads, Documents, `D:\Downloads-Backup`, `C:\ProgramData`, Local Temp và `D:\TYVOR`. Đồng thời đọc registry Uninstall HKLM/HKCU, tìm Visual Studio/Build Tools, Python, CMake, STM32, Espressif, LLVM, MinGW/MSYS/GCC/Ninja; không thấy mục SDK phù hợp. Không thấy `vswhere.exe` ở vị trí installer chuẩn.

**Giới hạn:** không quét byte-by-byte toàn bộ ổ đĩa; vùng không có quyền đọc và bản portable ở vị trí khác có thể chưa được phát hiện. Kết luận chính xác là “không tìm thấy công cụ khả dụng trong phiên/vùng kiểm tra”, không khẳng định tuyệt đối Windows chưa từng cài SDK.

## 4. Kết quả build/test

| Hạng mục | Kết quả | Chi tiết |
|---|---|---|
| Inventory/đọc source | PASS | Đọc đủ 13 tệp gốc |
| Native configure: `cmake -S . -B build` | FAIL — môi trường | PowerShell `CommandNotFoundException`: `cmake` không được nhận diện; compiler chưa được gọi |
| Native compile: `cmake --build build` | BLOCKED | Không chạy vì configure chưa thực hiện được; không có build tree |
| Native test: `ctest --test-dir build --output-on-failure` | FAIL — môi trường | `ctest` không được nhận diện; test executable chưa tồn tại/chưa chạy |
| Kết quả unit test C | BLOCKED | **Không có PASS/FAIL của logic C trong phiên này** |
| ESP tool check: `idf.py --version` | FAIL — môi trường | `idf.py` không được nhận diện |
| ESP-IDF firmware build | BLOCKED | Không có IDF/Python/toolchain khả dụng; không chạy set-target/build |
| STM32F407VET6 firmware build | BLOCKED — source và môi trường | Không có target board/startup/linker/HAL, đồng thời không có ARM compiler khả dụng |
| Hardware/flash/RF/panel | CHƯA THỰC HIỆN | Không kết nối/nạp/đọc board |

Không có `.elf`, `.bin`, `.map` hay log compiler chứng minh build firmware thành công. Dòng `puts("... PASS ...")` trong source test không phải bằng chứng test đã chạy.

Native CMake hiện chỉ tạo hai thư viện host `tyvor_protocol`, `tyvor_stm32_bridge` và executable `test_protocol`. Test chỉ link protocol, không link bridge. Nếu native build sau này thành công, kết quả đó vẫn không xác nhận firmware STM32 đã build đúng MCU.

## 5. Đánh giá từng thành phần

### shared

- C11; frame buffer tĩnh, không cấp phát heap trong protocol.
- Encode từ chối payload >32, output capacity thiếu và payload NULL khi length khác 0.
- Decode kiểm tra input/output NULL, kích thước tối thiểu, sync, version, length tối đa, kích thước chính xác và CRC trước khi copy payload.
- STATUS được serialize từng byte nên không phụ thuộc padding/layout của `tyvor_status_t`.
- `tyvor_encode` chưa kiểm tra output NULL. Quyền sở hữu buffer, lifetime và quy ước con trỏ API chưa được tài liệu hóa đầy đủ.
- Chưa có schema cho UI_REQUEST/ACK, allowlist request, capability/version negotiation hoặc validation ý nghĩa của trường STATUS.

### stm32

`tyvor_bridge.c` là adapter độc lập board: lưu callback `write_uart`, tăng sequence, đóng gói STATUS rồi gọi callback. RX nhận **một packet hoàn chỉnh**, gọi decoder và bỏ qua frame hợp lệ. Không có stream assembler phía STM32.

Thiếu toàn bộ phần biến thành firmware STM32F407VET6: entrypoint `main`, startup/vector table, system clock, CMSIS/device define `STM32F407xx`, HAL hoặc driver thanh ghi, linker script, map bộ nhớ, target cross-compile, `.ioc`, IRQ/DMA/UART initialization và board pin configuration. Không có ADC/gimbal, scheduler/mixer, persistence, RF/CRSF output hay watchdog.

Khi tạo target mới cần chọn đúng **STM32F407VET6**. ST công bố F407VE có 512 KB Flash, Cortex-M4/FPU tới 168 MHz; RAM gồm các vùng khác nhau, có 64 KB CCM. Không dùng linker 1 MB của biến thể VG và không đặt DMA buffer vào CCM chỉ CPU truy cập. Đây là yêu cầu thiết kế từ tài liệu ST, chưa phải thông số đọc từ board. [ST product](https://www.st.com/en/microcontrollers-microprocessors/stm32f407ve.html), [ST reference manual](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405-407-415-417-437-455-469-application-note-stmicroelectronics.pdf).

Callback TX trả `int` nhưng bridge bỏ qua kết quả; chưa quy định đó là số byte hay status. Nếu adapter tương lai dùng DMA bất đồng bộ, không được giữ con trỏ tới `packet` nằm trên stack sau khi `publish` kết thúc. Đây là ràng buộc tích hợp cần giải quyết, chưa phải lỗi HAL đã quan sát vì chưa có HAL adapter.

### esp32

- Hai CMakeLists có hình dạng dự án/component ESP-IDF; app dùng UART, esp_timer, logging và FreeRTOS được SDK cung cấp.
- UART1, 115200 baud, 8N1, không flow control; GPIO17 TX/GPIO18 RX là hằng số ví dụ chưa xác minh board.
- RX buffer driver 2048 byte, đọc tối đa 64 byte/lần, đợi 20 ms; buffer parser tối đa 40 byte.
- Chỉ nhận/log STATUS. Không gửi UI_REQUEST/ACK; không renderer, touch, LVGL hoặc driver panel.
- Không pin phiên bản ESP-IDF; không `sdkconfig.defaults`, partition layout riêng hay cấu hình chứng minh target S3, Flash 16 MB, PSRAM 8 MB và USB console. Đây là thiếu cấu hình tái lập, không tự động đồng nghĩa app logger tối thiểu không thể build.
- README nói USB console nhưng source chưa cấu hình console tương ứng. Cần chốt console path khi có SDK/board; không xem mô tả README là xác minh USB hoạt động.
- Kiểm tra source cho thấy cách gọi API UART phù hợp mô hình driver IDF; khả năng compile/link với một release cụ thể vẫn phải kiểm bằng build thật. Espressif yêu cầu môi trường IDF được kích hoạt và chọn `esp32s3` trước build. [IDF Windows workflow](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/windows-start-project.html), [UART API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/uart.html).
- Việc dùng PSRAM cần cấu hình SDK/dependency phù hợp release đã chọn; không suy từ chữ N16R8 rằng code đã dùng PSRAM. [Espressif PSRAM guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/external-ram.html).

### sim

Chỉ có một test executable, kiểm tra roundtrip STATUS, sequence/type, một số trường STATUS, một bit lỗi CRC, packet ngắn và payload NULL. Chưa có simulator giao diện, parser streaming test, bridge callback test, timeout test hoặc fuzzing.

Các kiểm tra nằm trong `assert`. Khi `NDEBUG` được bật, lời gọi decode trong assert cũng biến mất; `f.payload` sau đó bị đọc khi `f` chưa được khởi tạo, trong khi chương trình vẫn có đường đi in PASS. CMake không bảo đảm assert luôn bật cho target test.

## 6. Giao thức UART và xử lý packet lỗi

### Wire format hiện có

| Offset | Trường | Giá trị/quy tắc |
|---|---|---|
| 0–1 | Sync | `54 59`, ASCII `TY` |
| 2 | Version | `01` |
| 3 | Type | `01` STATUS, `02` UI_REQUEST, `03` ACK; decoder chưa allowlist type |
| 4 | Sequence | uint8, wrap sau 255; RX chưa theo dõi |
| 5 | Payload length | 0..32 byte |
| 6.. | Payload | STATUS có đúng 6 byte |
| Cuối −2, −1 | CRC16 | byte thấp trước, rồi byte cao |

Frame dài `payload_length + 8`, tối đa 40 byte. STATUS dài 14 byte. CRC dùng polynomial `0x1021`, init `0xFFFF`, không phản chiếu, không XOR cuối: phù hợp CCITT-FALSE. CRC bao phủ version/type/sequence/length/payload, không bao phủ hai byte sync.

STATUS payload: `arm_valid`, `arm`, `battery_mv` little-endian 16 bit, `lq`, `flags`. Chưa quy định miền giá trị/đơn vị LQ và ý nghĩa flags; không có validity riêng cho pin/LQ. ARM hợp lệ phải xuất phát từ dữ liệu đã xác minh; không được coi lệnh ARM cục bộ là bằng chứng trạng thái ARM từ flight controller.

Ở 115200 8N1, một STATUS chiếm khoảng 1,22 ms trên dây; frame tối đa khoảng 3,47 ms. Đây là tính toán lý thuyết, chưa đo throughput/jitter. Chu kỳ publish chưa được xác định.

### Những gì code đã xử lý và chưa xử lý

- Decoder từ chối CRC sai (`-2`); input sai framing/version/length trả `-1`.
- ESP bỏ byte noise trước sync, có hỗ trợ sync chồng `54 54 59`, xử lý được frame liên tiếp đủ byte. Nếu frame hoàn chỉnh sai CRC nhưng frame kế tiếp bắt đầu đúng ranh giới, parser có thể nhận frame kế tiếp.
- Parser chỉ kiểm tra length ngay sau header; version chỉ bị loại khi đã gom đủ frame.
- Nếu mất byte hoặc nhận header giả có length hợp lệ, parser giữ trạng thái `used` và có thể ăn byte của frame sau vào packet lỗi. Sau lỗi nó xóa cả buffer, không quét lại phần có thể chứa sync hợp lệ.
- Không có timeout giữa byte hay deadline cho frame đang gom. Tham số 20 ms của `uart_read_bytes` chỉ là thời gian đợi đọc, **không reset parser**.
- Timeout 2 giây chỉ được kiểm tra để log stale, mỗi khoảng một giây; cảnh báo thực tế có thể trễ thêm chu kỳ log/lịch chạy. Không có code chuyển `live.arm_valid` về 0 hoặc invalidate snapshot.
- Frame STATUS trùng/replay với CRC đúng vẫn cập nhật `last_rx_us`: sequence chưa phát hiện trùng, mất frame, đảo thứ tự hoặc peer reboot. CRC chỉ kiểm tra lỗi dữ liệu, không chứng minh độ mới.
- Không có UART event queue (`uart_driver_install(..., 0, NULL, ...)`), bộ đếm CRC/length/framing/overflow, negative read diagnostics hoặc chính sách flush/resync khi lỗi FIFO/ring buffer.
- STM32 RX không hỗ trợ packet bị chia nhỏ/gộp; caller tương lai phải tách frame. Không thể đưa trực tiếp chunk DMA bất kỳ vào `tyvor_bridge_receive` và coi đó là stream parser.
- Các UI_REQUEST/ACK chỉ là enum; chưa có command execution. Việc không nhận lệnh RF/ARM hiện tại là ranh giới phù hợp cần duy trì.

### Chuỗi byte tái hiện từ phân tích

Frame STATUS tương ứng dữ liệu test hiện có:

```text
F = 54 59 01 01 07 06 00 00 A4 1F 63 00 CE 12
    arm_valid=0, arm=0, battery=8100 mV, LQ=99, flags=0
```

Đã chạy một mô hình JavaScript độc lập trong công cụ audit để tính CRC và trace nhánh tương ứng `ingest`; **không biên dịch/chạy C, không gọi đây là native unit test**. Mô hình tính `CRC("123456789") = 0x29B1`, CRC body của F là `0x12CE`.

| Input cho mô hình | Frame STATUS được nhận | Buffer còn giữ | Ý nghĩa cần tái hiện bằng C sau khi có toolchain |
|---|---:|---:|---|
| F | 1 | 0 byte | Frame đầy đủ |
| F + F | 2 | 0 byte | Frame liên tiếp |
| `54` + F | 1 | 0 byte | Sync chồng |
| F sai byte CRC cuối + F | 1 | 0 byte | Frame lỗi bị loại, frame kế đúng ranh giới được nhận |
| 6 byte header đầu F + khoảng dừng bất kỳ + F | 0 | 0 byte | Header dở dang nuốt 8 byte đầu F rồi reset; phần còn lại không có sync nên F bị mất |
| `54 59 01 01 00 20` + F + F | 0 | 34 byte | Length giả 32 giữ cả hai frame hợp lệ trong packet dở; không timeout để giải phóng |

Khoảng dừng không thay đổi kết quả vì code không có nhánh timeout cho `used`. Các trace này hỗ trợ kết luận static review; chưa đo trên UART thật.

## 7. Phát hiện và mức độ nghiêm trọng

Mức độ xét trong phạm vi starter hiện tại: Cao = cần giải quyết trước khi dựa vào kết quả test hoặc dùng cầu nối cho dữ liệu vận hành; Trung bình = thiếu độ bền/API/quan sát cần xử lý trong Phase 1; Thấp = tài liệu/cấu hình cần làm rõ. Chưa phát hiện đường thực thi điều khiển RF/ARM nên không gán lỗi an toàn phần cứng đã xảy ra.

| ID | Mức | Bằng chứng | Ảnh hưởng / hướng khắc phục đề xuất, chưa thực hiện |
|---|---|---|---|
| A01 | Cao | `esp32/main/main.c:28`, `:33`, `:36` | Không timeout partial frame, reset mất cả buffer khi lỗi; frame hợp lệ có thể bị nuốt. Tách parser portable, bổ sung deadline và resync có kiểm thử noise/truncation. |
| A02 | Cao | `sim/test_protocol.c:9`, `:10`, `:14`; root `CMakeLists.txt` | `NDEBUG` loại kiểm tra và decode, tiếp đó đọc dữ liệu chưa khởi tạo; dòng PASS không đáng tin. Dùng assertion luôn hoạt động và tránh side effect trong assert; xác minh Debug/Release. |
| A03 | Trung bình | `esp32/main/main.c:24`, `:52` | Stale chỉ log, snapshot vẫn giữ dữ liệu cũ. Thêm validity/age cho snapshot và dùng chúng trong renderer tương lai. Hiện chưa có renderer để khẳng định đã hiển thị ARM sai. |
| A04 | Trung bình | `shared/tyvor_protocol.c:5`, `:6` | `tyvor_encode(..., out=NULL, cap>=length+8)` ghi qua NULL nếu điều kiện còn lại hợp lệ. Caller hiện có dùng buffer hợp lệ; cần bổ sung hợp đồng/guard API và test. |
| A05 | Trung bình | `stm32/Core/Src/tyvor_bridge.c:12` | Bỏ qua kết quả TX; không phát hiện lỗi/gửi thiếu. Quy định callback và chính sách retry/drop/counter, tránh chặn control loop. |
| A06 | Trung bình | `esp32/main/main.c:22`, `:24`; `shared/tyvor_protocol.h:12` | Sequence chỉ truyền đi, RX không kiểm tra; frame lặp có thể giữ link alive. Cần định nghĩa sequence wrap, session/reboot và freshness. |
| A07 | Trung bình | `esp32/main/main.c:41`, `:47` | Không quan sát lỗi UART/framing/overflow, decode lỗi bị bỏ im lặng. Cần thống kê có giới hạn tốc độ và recovery rõ ràng. |
| A08 | Trung bình | `sim/test_protocol.c`; root `CMakeLists.txt:10` | Test chỉ protocol cơ bản; không test parser, timeout, bridge, maximum frame, schema hay tải lỗi. Cần test ma trận lỗi thực tế. |
| A09 | Trung bình | `shared/tyvor_status.h:8`, `:9`; `esp32/main/main.c:23` | Chỉ kiểm tra length, không validation semantic; giá trị arm_valid/arm bất kỳ khác 0 được coi true, pin/LQ không có validity. Cần schema và quy tắc miền giá trị trước UI thật. |
| A10 | Thấp | README; `docs/TX15_REFERENCE.md:20`; không có panel datasheet | Tài liệu nhắc CST820/QSPI trong khi model touch/interface panel chưa có bằng chứng. Giữ dưới dạng giả thuyết cần xác minh, không dùng làm pin/driver specification. |

Các blocker dự án riêng, không phải lỗi runtime đã tái hiện: thiếu môi trường build (Cao), thiếu target STM32 hoàn chỉnh (Cao), chưa pin/configure ESP-IDF cho board (Trung bình), thiếu toàn bộ panel/UI (theo phạm vi scaffold đã công bố).

## 8. Những phần còn thiếu so với nhóm chức năng tham khảo EdgeTX/TX15

TX15 dùng STM32 H7 và màn IPS cảm ứng 480×320; nền tảng khác F407 + S3 + AMOLED portrait. Chỉ tham khảo nhóm chức năng, không coi là target firmware/pinout tương thích. [RadioMaster TX15](https://radiomasterrc.com/products/tx15-radio-controller-elrs-m2), [EdgeTX platform table](https://edgetx.org/edgetx/latest/hardware/stm32-platforms/).

Các nhóm dưới đây được tổng hợp để lập roadmap TYVOR; không phải cam kết tính năng giống EdgeTX. Danh mục model/radio tham khảo từ [EdgeTX Model Settings](https://manual.edgetx.org/color-radios/model-settings) và [EdgeTX Radio Settings](https://manual.edgetx.org/color-radios/radio-settings).

| Nhóm | Tình trạng hiện tại | Phần cần phát triển / nơi sở hữu |
|---|---|---|
| Model management | Chỉ có enum MODEL_INFO | Schema model, tạo/chọn/lưu/version/migration, đổi model có điều kiện; STM32 sở hữu cấu hình đang dùng, ESP hiển thị/editor sau này |
| Inputs, trims, calibration | Chưa có | ADC/DMA, calibration, deadband, debounce switches/encoder/trims, fault detection; STM32 |
| Mixer, curves, flight modes | Chưa có | Mixer xác định thời gian, expo/rates/curves, flight modes, logical switches/global variables; STM32, test native trước RF |
| Outputs / channel monitor | Chưa có | Limits/reverse/subtrim, throttle cut, channel snapshots; STM32 tính, ESP hiển thị |
| RF / module / receiver | Chưa có CRSF TX/RX | Module interface, channel output, telemetry, link-loss policy và điều kiện RF; STM32, giai đoạn riêng |
| Telemetry | STATUS 6 byte, chưa có nguồn thật | CRSF parser, sensor registry, units/validity/timeouts, battery/LQ/RSSI/GPS nếu có dữ liệu; STM32 cung cấp snapshot |
| Timers, warnings, special functions | Chưa có | Timer, startup switch/throttle checks, warnings, alarms, giới hạn special functions; STM32 quyết định cảnh báo quan trọng, ESP hỗ trợ trình bày |
| Home/navigation/touch | Chỉ log console | HOME, MODELS, INPUTS, OUTPUTS, TELEMETRY, RADIO, TOOLS; UI portrait, LVGL và touch; ESP |
| Storage, backup, logs | Chưa có | Atomic config, CRC/version, migration, backup/restore, log quota; STM32 lưu authority, ESP có thể cache/assets |
| Audio/haptic/display power | Chưa có | Chọn phần cứng rồi thiết kế driver, brightness/sleep/audio/haptic; phân MCU theo đường tín hiệu và deadline |
| Trainer, USB joystick, scripts | Chưa có | Cần đánh giá nhu cầu/tài nguyên/ranh giới quyền; để sau baseline, không bắt buộc Phase 1 |
| Diagnostics/update | Chưa có | Firmware/build IDs, counters, reset reason, memory/timing stats, update/recovery riêng từng MCU; không update/flash tự động |

## 9. Cấu trúc firmware đề xuất

Đây là đề xuất, chưa tạo module hoặc thay đổi code:

```text
shared/
  protocol/       wire codec, CRC, streaming parser, version/capabilities
  schema/         status, validity, units, read-only request definitions
stm32/
  board/          STM32F407VET6 startup, linker, clocks, HAL, pin mapping đã xác minh
  drivers/        ADC, inputs, timers, UART/DMA, storage
  control/        input -> calibration -> mixer -> limits -> RF adapter
  safety/         startup checks, watchdog, fault/output policy
  services/       model/config authority, telemetry, diagnostics
  transport/     bounded UART queues, snapshot publisher, read-only request handler
esp32/
  main/           startup/task composition
  components/
    bridge/       UART I/O, counters, snapshot cache, freshness
    ui/           LVGL screens, navigation, view models
    panel/        driver/BSP sau khi xác minh module
    touch/        I2C controller sau khi xác minh part number
    storage/      UI preferences/assets, không ghi đè authority STM32
sim/
  protocol/       golden vectors, malformed/stream/timeout tests
  control/        deterministic input/mixer tests khi module tồn tại
  ui/             simulator/mocks, luôn đánh dấu dữ liệu giả
docs/             protocol spec, board evidence, build matrix, commissioning
```

Luồng sở hữu dữ liệu: inputs vật lý -> STM32 xử lý và quyết định -> snapshot UART -> ESP cache có validity/age -> UI. Touch tạo ý định UI; mọi yêu cầu thay đổi vận hành tương lai phải qua kiểm tra/transaction/physical gating trên STM32. Phase 1 chỉ dùng request đọc, không có command ARM/RF.

Control loop STM32 phải chạy độc lập khi ESP reset, treo hoặc bão dữ liệu. UART dùng buffer hữu hạn, timeout hữu hạn, không block control loop; giới hạn tốc độ publish/request và counter lỗi. Nếu dùng RTOS thì ISR chỉ thu dữ liệu/đánh thức task, UI không tham gia lịch control. Bare-metal hay RTOS cần chốt theo yêu cầu timing, không mặc định vì code hiện tại chưa có scheduler STM32.

STM32 giữ model/calibration/output policy chuẩn; ESP lưu preferences/assets và bản cache có revision. Không đồng bộ hai bản cấu hình bằng cơ chế “ghi cuối thắng”. Mỗi MCU cần build độc lập; shared portable không include HAL/ESP headers để native test được.

## 10. Phase 1 và bước tiếp theo

Phase 1 đề xuất: **baseline build được, link read-only đủ bền, UI mô phỏng**. Không mở RF và không phụ thuộc việc đoán pinout AMOLED.

1. Chuẩn hóa môi trường: chọn/pin native compiler + CMake/CTest/Ninja, ARM toolchain/CubeF4 và một release ESP-IDF hỗ trợ S3; có Python thật. Ghi version và cách kích hoạt trong tài liệu. Việc cài đặt chưa được thực hiện trong audit này.
2. Chạy baseline native trong Debug và Release; khắc phục A02 trước khi tin dòng PASS. Giữ code firmware thay đổi ở công việc triển khai riêng sau audit.
3. Đặc tả protocol v1: byte order, CRC vectors, valid types/lengths, validity của từng field, publish cadence, timeout, sequence wrap/reboot, counters và quota. Thêm parser timeout/resync và các case A01 vào C test.
4. Build ESP logger thật cho `esp32s3`, lưu config tái lập; xác nhận Flash/PSRAM/console theo đúng board revision. Không cần panel để xác minh compile. Mọi nạp board phải thuộc bước được yêu cầu riêng.
5. Tạo project STM32 mới cho F407VET6, độc lập firmware STABLE; startup/linker/clock + bridge adapter có hợp đồng TX rõ ràng. Trước khi nối dây, dùng schematic thực để chọn UART/pin và xác nhận mức điện áp. Chỉ publish trạng thái thực đã xác minh, không dựng ARM thật từ mock.
6. Tạo simulator UI 368×448 với dữ liệu mock được đánh dấu; trước mắt HOME/INPUTS/TELEMETRY/TOOLS read-only. Mất link hoặc telemetry không hợp lệ phải hiện UNKNOWN/STALE theo dữ liệu validity, không suy từ số 0.
7. Thu thập datasheet/module SKU/FPC pinout/init sequence của màn CO5300, interface điện thực, nguồn/reset/touch part number và I2C address. Kiểm tra tương thích rồi mới chọn driver; không suy QSPI, MIPI hay CST820 chỉ từ tên controller/resolution. Driver panel/touch vẫn là bước sau khi có bằng chứng.

Tiêu chí kết thúc Phase 1: native test thực chạy ở Debug/Release; case noise/truncated/bad CRC/bad length/timeout/sequence có kết quả; có ELF/map cho đúng STM32F407VET6 và binary ESP32-S3 từ build log; UI mock chứng minh UNKNOWN/STALE; ESP bị ngắt không chi phối control scheduling; hồ sơ panel chỉ ghi thông tin có nguồn. Những tiêu chí có board cần được ghi rõ là chưa thực hiện nếu chưa có phần cứng.

Các kiểm tra nên bổ sung trong triển khai: CRC golden vector; payload 0/32/>32; cap thiếu; NULL input/output; sync/version/type/length sai; lỗi từng byte; packet split/coalesced; sync trong payload; header giả; UART overflow; timeout và recovery; wrap/duplicate/reboot; pack/unpack đủ trường; callback gửi thiếu/lỗi; boot và stale trước frame đầu tiên. Sanitizer/fuzzing dùng trên native toolchain phù hợp khi đã có baseline.

Lệnh native dùng lại sau khi môi trường sẵn sàng, chọn generator phù hợp compiler:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`CMAKE_BUILD_TYPE` dành cho generator một cấu hình; `--config`/`-C` áp dụng với generator nhiều cấu hình. Không dùng Release để xác nhận test hiện tại trước khi giải quyết A02.

Với ESP-IDF đã được kích hoạt, chạy trong `esp32/`: `idf.py set-target esp32s3`, rồi `idf.py build`. Đây là chỉ dẫn cho bước tiếp theo; **không chạy flash/monitor trong audit này**.

## 11. Dấu vết và giới hạn audit

- Đã kiểm tra source, CMake, tài liệu, công cụ trên PATH, vùng SDK thông dụng và các thư mục mở rộng nêu trên; dùng SHA-256 cho 13 tệp gốc để đối chiếu sau audit.
- Bổ sung duy nhất báo cáo Markdown này; không sửa firmware/CMake/test hiện có và không triển khai tính năng.
- Không cài công cụ, nạp firmware, mở serial, thay cấu hình global, commit, push, deploy hoặc sửa repository EdgeTX/TYVOR STABLE.
- Chỉ đọc tài liệu công khai của ST, Espressif, RadioMaster, EdgeTX và repository WeAct để đối chiếu; không đồng nhất tài liệu với kiểm tra board thực. [WeAct source reference](https://github.com/WeActStudio/WeActStudio.ESP32S3-AorB).
- Các lỗi code trong báo cáo là kết luận static review, bổ sung trace mô hình độc lập khi nêu rõ. Không có compiler C/SDK/hardware run để chứng minh thêm trong môi trường phiên này.
