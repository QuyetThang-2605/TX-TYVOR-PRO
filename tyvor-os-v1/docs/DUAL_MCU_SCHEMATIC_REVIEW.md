# TYVOR OS V1 — Dual MCU schematic review

Ngày **11/11/2026, Asia/Bangkok**. Workspace `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Kết luận: phương án UART hợp lý ở mức MCU/reference, giữ PROPOSED. Chưa đủ bằng
chứng để chốt chân trên Mini Type-C, đấu nối, cấp nguồn liên-board hoặc mở gates.**

Kiến trúc giữ nguyên: **STM32F407VET6 Mini Type-C làm CONTROL**, **WeAct ESP32-S3-B
N16R8 làm UI**, protocol v2 qua UART; AMOLED2.04 inch368×448 chưa có exact module
pinout. Target STM32F4 Discovery trong workspace vẫn tách biệt; không dùng pin,
clock, LED hoặc USB/ST-LINK của MB997E làm bằng chứng cho Mini Type-C.

## 1. Tài liệu đã đọc và giới hạn bằng chứng

### STM32: DevEBox, chưa xác nhận là board đã mua

Đã tải và đọc cả hai trang PDF được người dùng chỉ định:
[STM32F407VX_M_schematics.pdf](https://github.com/mcauser/MCUDEV_DEVEBOX_F407VET6/blob/b32028d57e29be8eef169e3c027b25d4e74fb9a8/docs/STM32F407VX_M_schematics.pdf).
Trang1 là schematic, trang2 là PCB/layout reference có chữ **DevEBox**; title block
ghi2018-11-23. U2 ghi STM32F407VET6, connector J5 là **Micro-USB**.

**Đây là schematic của MCUDEV/DevEBox được mirror trong repository mcauser;
không phải schematic đã xác minh khớp STM32F407VET6 Mini Type-C đã mua.**
Cùng MCU không chứng minh cùng regulator, oscillator, header, USB hay revision.
Khác connector Micro-USB/Type-C là khác biệt thấy ngay, đủ để không sao chép
header/power assignment sang actual board.

Provenance:

- Repository commit `b32028d57e29be8eef169e3c027b25d4e74fb9a8` (master lúc tra cứu).
- PDF297838 bytes, Git blob `bfbcedec89b91af682df820e94f72285cc37571b`, khớp
  blob hash tính từ file tải; SHA-256
  `ca347dc63edcdee43e21f6b6dd65e33136483e6cb7350d4c462f039eab915edc`.
- Local: `build/dual-mcu-review/sources/STM32F407VX_M_schematics.pdf`;
  extracted text, full-page PNG và header crops nằm cùng thư mục. Đã xem cả
  schematic lẫn layout, không chỉ đọc text extractor.

### ESP32: có snapshot WeAct-B trong workspace

Không tìm thấy một attachment riêng mang đúng tên `ESP32_S3_B_Sch.pdf`. Có bản
reference đã lưu từ Phase2D: `build/phase2d/sources/weact-b.pdf`, một trang,
gồm schematic và layout B. Nguồn của file là
[repository WeAct chính thức, ESP32S3-B](https://github.com/WeActStudio/WeActStudio.ESP32S3-AorB/blob/c53183ea4aa96c6b7abf2b1dc153bafa22d2b96e/ESP32S3-B/Hardware/ESP32_S3_B_Sch%20.pdf)
(tên upstream có dấu cách trước `.pdf`). Dùng đúng bản B này, không dùng bản A
hoặc DevKitC thay thế.

Đã đọc text, xem render toàn trang và phóng to block PIN. Kiểm tra lại size815817
bytes, SHA-256
`910a41eea1b8d111e8efc78667eb01d95a30b6e7ae2b9ce7567afcafd3913e68`,
khớp retrieval/identity của snapshot; upstream commit
`c53183ea4aa96c6b7abf2b1dc153bafa22d2b96e`, blob
`d27f6ad987b174aa9515e7207ec9a8fa4be45e95`.

Schematic ghi U1 ESP32-S3-R8 và U6 FLASH; **không ghi capacity16MB của U6**.
Tên variant N16R8 và configuration16MB không thay cho Flash ID/capacity thực tế.
Revision/suffix/marking và routing của WeAct đã mua vẫn chưa đối chiếu ảnh PCB.
Không coi snapshot này là một attachment người dùng đã được nhận diện độc lập.

### Silicon và API references

- [ST DS8626 Rev12](https://www.st.com/resource/en/datasheet/stm32f407ve.pdf),
  Fig13/Tables7,9: LQFP100 package pins và AF; đây là silicon reference, không là
  pinout của Mini Type-C.
- [Espressif S3 datasheet v2.2](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf),
  Tables2-1/2-4, memory và boot sections: GPIO domains, UART IO_MUX, straps.
- [IDF5.5.1 GPIO restrictions](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/gpio.html)
  và [UART pin routing](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/uart.html):
  memory-reserved GPIO và `uart_set_pin`.

Phân loại trong hồ sơ: **REFERENCE ONLY** là nội dung đã đọc ở đúng tài liệu;
**PROPOSED** là assignment logic đang đề xuất; **UNVERIFIED** là actual PCB/net/
điện áp chưa xác nhận. Không có hàng **VERIFIED-PCB**. Sơ đồ DevEBox không nâng
bất kỳ chân Mini Type-C nào lên VERIFIED.

## 2. Pin matrix và UART proposal

[DUAL_MCU_PIN_MATRIX.md](DUAL_MCU_PIN_MATRIX.md) chứa đầy đủ:

- DevEBox J2.1–44 và J3.1–44; debug J1, TFT J4, USB J5, boot/reset, crystals,
  analog/internal-regulator/backup rails và MCU pins ngoài headers.
- WeAct-B L01–22/R01–22 theo orientation reference; USB1/2, reset/boot/auto-reset,
  external Flash/in-package PSRAM và reserved/power/RF/crystal pads.
- Board pin reference, package pin, function, voltage domain, conflict/load,
  assignment, source và trạng thái actual cho từng dòng. GPIO chưa gán không
  đồng nghĩa free GPIO. Toàn bộ100 chân STM LQFP100 và57 pad ESP gồm EP được
  account bằng header rows hoặc grouped internal/power rows; không chọn RF pins.
- AMOLED/touch interface inventory để TBD; không bịa số chân FPC hoặc bus.

**Chỉ đề xuất hai đường tín hiệu sau:**

| Chiều | Endpoint phát | Endpoint nhận | Header trong reference | Trạng thái |
|---|---|---|---|---|
| CONTROL → UI | STM PA2, U2.25, USART2_TX AF7 | ESP GPIO18, U1.24, UART1_RX | DevEBox J3.32 →WeAct L11; actual locations TBD | **PROPOSED** |
| UI → CONTROL | ESP GPIO17, U1.23, UART1_TX | STM PA3, U2.26, USART2_RX AF7 | WeAct L10 →DevEBox J3.31; actual locations TBD | **PROPOSED** |

Common signal ground cũng mới là **PROPOSED**, actual GND points/continuity và
power/ground plan TBD. Không nối TX↔TX; không dùng GPIO17 làm RX của phương án
này. Không đề xuất nối5V/3V3/VBAT hoặc reset giữa hai board.

Parameters hiện có phù hợp proposal:115200 baud,8 data bits,no parity,1 stop bit,
no RTS/CTS. ESP GPIO17/18 có U1TXD/U1RXD trong IO_MUX, và driver hiện gọi
`uart_set_pin` khi được gate cho phép. Không cần đổi protocol v2 hoặc session/
CRC/parser để lập proposal. Các đường UART là logic nominal3.3V, không RS-232
và không UART5V; mức actual vẫn cần đo từng board và kiểm tra khi một board mất nguồn.

STM giữ quyền quyết định input/model/mixer/safety và logical outputs; UI tiếp tục
read-only/request architecture hiện có. Proposal không gán dây ESP→STM NRST,
BOOT0, ARM hay physical output. Không thêm đường bypass safety qua phần cứng.

## 3. Xung đột GPIO

| Nhóm | STM32 /DevEBox reference | ESP32 /WeAct-B reference | Kết luận cho UART proposal |
|---|---|---|---|
| Internal Flash | Internal MCU Flash không chiếm PA2/3 | U6 SPI0/1 memory bus GPIO26–32 | Không trùng candidate; chưa xác nhận capacity actual |
| External Flash | U3 W25Q16: PA15 CS, PB3 CLK, PB4 SO, PB5 SI | U6 nối VDD_SPI/bus memory | Giữ các chân đó ngoài proposal |
| PSRAM | Không có PSRAM peripheral được chọn trong BSP hiện tại | R8 Octal PSRAM dùng thêm33–37;35/36/37 vẫn xuất hiện trên header | Giữ26–37 reserved; header không chứng minh free |
| USB | PA11/12 ở J5 Micro-USB reference | GPIO19/20 native USB2; USB1 đi U7 UART0 | PA2/3 và17/18 không trùng USB pins |
| Boot | BOOT0 và PB2/BOOT1, pulldowns trong DevEBox | GPIO0/3/45/46 straps | Không dùng strap pins làm UART |
| Debug | PA13/14 SWD; PA15/PB3/PB4 full JTAG/trace có chia sẻ Flash | GPIO39–42 external JTAG;19/20 USB-JTAG | Không cần chiếm debug pins |
| Boot/log UART | PA9/10 USART1 ở reference | GPIO43/44 UART0 còn nối U7 | Không chọn những cặp có boot/log/USB bridge load |
| Onboard peripherals | MicroSD PC8–12/PD2; TFT PB12–15/PC5/PB1; LED PA1; USER PA0 | GPIO48 RGB LED;45 USER/strap;0 BOOT; CHIP_PU reset | Không thấy nối vào PA2/3 hoặc17/18 trong tài liệu reference |
| Analog/timer | PA2/3 còn ADC123_IN2/3 và nhiều AF | GPIO17/18 còn ADC2_CH6/7,18 CLK_OUT3 | Không dùng ADC/timer đồng thời trên UART pins; gimbal ADC real chưa allocate |
| AMOLED/touch | Không dùng J4 DevEBox làm sơ đồ panel TYVOR | Exact module/FPC/bus/control/touch pins chưa có | **Chưa thể kết luận không xung đột panel** |

Net labels `SPI1_SCK/MISO/MOSI` trong DevEBox nối PB3/4/5; theo ST AF mapping,
đây là **SPI3 AF6**, không phải chứng cứ Flash dùng PA5/6/7. Khi review BSP/peripheral
assignment phải theo GPIO+AF, không theo tên net thiếu chính xác.

Không thấy xung đột giữa candidate UART và các nhóm đã xác định ở mức reference;
điều đó **không chứng minh** các chân được đưa ra header hoặc không gắn tải trên
Mini Type-C/WeAct thực tế. Chưa có bằng chứng lỗi Core cần sửa trong review này.

## 4. Power, boot và reset cần xác minh

| Hạng mục | Điều đọc được từ reference | Cần đối chiếu actual PCB |
|---|---|---|
| STM input/rails | J5 Micro-USB VBUS qua L1 (label500mA) tới5V; U1 AMS1117-3.3 ra3V3; J2/J3 có supply pins | USB Type-C actual power path/CC, regulator part và rating, polarity, protection/backfeed, rail measurement; không suy L1 là current limiter của board đã mua |
| STM analog | VDDA qua R34, VREF+ qua R11; VCAP1/2 có tụ, VBAT có battery network | Đúng pins/net, population/decoupling; không dùng VCAP/VBAT/VREF+ làm rail nuôi ESP/panel |
| STM SWD/reset | J1:1 BOOT0,2 3V3,3 GND,4 SWDIO,5 SWCLK; NRST ở U2.14/RST button, không có trên J1 | Pin1/orientation/SWD actual, reset pad/routing, boot pull-resistors; không lấy J1.1 làm NRST |
| STM clock | Y2=8MHz HSE, Y1=32.768kHz LSE trong DevEBox | Crystal marking/population/route actual. BSP Mini vẫn HSI16MHz, không sửa sang8MHz từ tài liệu này |
| ESP input/rails | USB1/2 VBUS qua D1/D5 tới+5V; U8/U9 là bypass links; U4 LDO ra VCC_3V3, không ghi exact part/rating | Markings, bypass population, nguồn input/output limits/current, backfeed khi dùng USB khác nhau; không tự bridge bypass hoặc chập rail hai board |
| ESP reset | CHIP_PU active-low qua SW3, pull-up/RC và Q1/Q2 auto-reset | R/C/transistor routing và RESET button actual; reset ESP phải độc lập với STM |
| ESP boot/USER | SW2 GPIO0; SW1 GPIO45 có pull-up, đồng thời là VDD_SPI strap | Actual boot policy/voltage/eFuse chưa đọc; không sửa pull/strap/eFuse để thử |
| USB-UART | U7 UART0 và DTR/RTS auto-reset ESP | Bridge marking, physical connector identity. Mở serial trong bước hardware sau có thể tác động reset ESP; task này không mở cổng |
| Inter-board | Chưa có approved electrical drawing | Common reference/idle levels, voltage thresholds, unpowered-I/O/backfeed và power sequence; chưa nối chung nguồn |
| Display/touch | Hai schematic board không chứa exact AMOLED module | Module part/revision, FPC orientation/pin1, rails/logic, bus, reset/TE, touch IC/pull-ups/IRQ; thiếu thì giữ TBD |

Các hàng power chỉ là kiểm kê. Không có hướng dẫn cấp nguồn hay nối dây actual
trong phiên này. Không mặc định GPIO ESP chịu được5V vì STM có một số chân FT.

## 5. Đối chiếu BSP/config hiện có — không thay runtime mapping

| File /evidence | Trạng thái đọc được | Kết quả review |
|---|---|---|
| [stm32/CMakeLists.txt](../stm32/CMakeLists.txt) | `TYVOR_UART2_PA2_PA3_VERIFIED` defaultOFF; compile definition ánh xạ gate sang `TYVOR_UART_ENABLED` | Chưa mở gate |
| [stm32/BSP/board.c](../stm32/BSP/board.c) | USART2 PA2/PA3 AF7 chỉ trong `#if TYVOR_UART_ENABLED`;115200/8N1/no flow; SYSCLK HSI16MHz | Candidate software phù hợp proposal, không xác minh PCB |
| STM compile-check object | CMake còn compile branch USART2 với `TYVOR_UART_ENABLED=1` để type/API check, không link object đó vào executable | Compile được không đồng nghĩa UART runtime bật |
| [esp32/main/main.c](../esp32/main/main.c) | `BRIDGE_UART=UART_NUM_1`; gate chưa xác minh trảfalse trước UART/GPIO init; `uart_set_pin(tx,rx)` dùng config | Chưa áp GPIO17/18 vào runtime |
| [esp32/main/Kconfig.projbuild](../esp32/main/Kconfig.projbuild) / [defaults](../esp32/sdkconfig.defaults) | Gate defaultn, TX/RX default-1 | Giữ nguyên |
| Generated `esp32/sdkconfig` | Gate not set, TX=-1,RX=-1; console USB_SERIAL_JTAG;16MB Flash/OCT PSRAM config | Firmware pin proposal vẫn chưa enable; config không là detection16MB/8MB |
| Existing CMakeCache | Phase2C STM và Discovery legacy-VE: UART2 gateOFF; Discovery target riêng: UART gateOFF | Chỉ đọc cache, không rebuild hoặc reconfigure |
| [shared/tyvor_protocol.h](../shared/tyvor_protocol.h) | `TYVOR_VERSION=2`, frame max64, HELLO/SESSION_ACK tồn tại | Không sửa protocol/parser/session |

`GPIO_IS_VALID_*` trong ESP init kiểm tra khả năng GPIO nói chung; **không thay cho
kiểm tra reserved memory/USB/strap hoặc board load**. Cần giữ manual evidence gate
cho đến khi actual pin matrix được phê duyệt. Không sửa guard/allocator/Core trong
task phân tích tài liệu này. Mock inputs/gimbal hiện chưa gán ADC hardware nên cần
I/O budget riêng trước khi sử dụng PA2/3 cho bất kỳ input khác.

## 6. Checklist thông tin PCB còn thiếu

1. Ảnh rõ hai mặt Mini Type-C: logo/revision/full marking STM32F407VET6,
   regulator/Flash/USB components, header labels và pin1/orientation, crystal,
   SWD/NRST/BOOT buttons/straps; ảnh WeAct-B cũng đủ các mục tương ứng.
2. Exact schematic/BOM/revision matching Mini đã mua. Dùng ảnh để so với DevEBox
   chỉ giúp phát hiện khác biệt, không tự tạo quyền dùng mapping DevEBox.
3. Trong phiên hardware được cho phép riêng: continuity không cấp nguồn từ header
   PA2/PA3 tới MCU25/26, WeAct17/18 tới pads23/24, GND/reset/USB/boot nets;
   tìm tải onboard và đo rail resistance, không suy từ silkscreen đơn lẻ.
4. Sau review power plan: đo rail/logic thực, chọn ground/power sequence và kiểm tra
   unpowered backfeed; xác minh MCU/Flash identity/capacity và PSRAM8MB bằng tool/
   memtest thích hợp. Chưa thực hiện các bước này hoặc đọc option/eFuse trong task này.
5. Exact AMOLED/touch module schematic/FPC pin table và power requirements;
   đối chiếu với UART proposal trước khi allocate bus/display pins.
6. Reviewer/date và approved drawing xác định **actual** TX/RX/GND, supply policy,
   reset independence. Chỉ một phiên implementation/hardware được cho phép sau đó
   mới xem xét đổi runtime config/gates; review tài liệu hiện tại không mở gates.

## 7. Kiểm chứng tài liệu, thay đổi và kết luận

Đã render/read2 trang DevEBox và1 trang WeAct-B; kiểm tra hash provenance. Matrix
bao phủ **88 STM header positions +44 ESP header positions**, các groups bổ sung
account100 STM package pins và57 ESP pads gồm EP. Đối chiếu thêm **74 GPIO header
entries** DevEBox với embedded connectivity text trong PDF, không mismatch.
Đây là kiểm chứng transcription/tài liệu, không phải continuity hoặc hardware PASS.

Baseline142 source/docs/scripts đã hash trước review. Sau review chỉ thêm:

- `docs/DUAL_MCU_SCHEMATIC_REVIEW.md`.
- `docs/DUAL_MCU_PIN_MATRIX.md`.

142 file cũ và generated UART config được kiểm tra giữ nguyên; không sửa BSP,
Core, tests, SDK, artifacts cũ hoặc build configuration. Không chạy native/MCU
build/test vì nhiệm vụ chỉ phân tích tài liệu; không lấy kết quả test lịch sử làm
PASS mới. Không mở serial/SWD, flash/erase, cấp nguồn/đấu board, đổi option/eFuse,
bật UART/RF, sửa STABLE, commit/push/deploy hoặc Git global.

Source PDFs, renders, extracted text, before/after hashes và kiểm tra coverage/
transcription ở `build/dual-mcu-review/`. Git vẫn báo workspace untracked từ root
cha; dùng hash baseline để mô tả WIP, không giả tracked diff hoặc stage/commit.

**Kết luận: UART PA2/PA3 ↔GPIO18/17 là PROPOSED, phù hợp silicon và source hiện
có; chưa phải pinout đã xác minh của hai board đã mua. Actual wiring/hardware
bring-up NOT READY vì Mini schematic/revision match, power/net evidence và panel
pinout còn thiếu. Giữ gates OFF và dừng để review.**
