# TYVOR Phase 2D — Power & pin matrix

Ngày 10/11/2026, Asia/Bangkok. **Chưa có pin assignment được phê duyệt để đấu nối. UART gates OFF.** Bảng dùng để đối chiếu tài liệu và ảnh PCB; không phải sơ đồ nối hai board hoặc hướng dẫn cấp nguồn.

## Cách đọc và phạm vi xác minh

- **VERIFIED:** dữ kiện đã đọc trong đúng tài liệu được chỉ ra. Phải ghi phạm vi: seller field, silicon, reference schematic hoặc software.
- **INFERRED:** liên hệ có căn cứ giữa sản phẩm và tài liệu nhưng chưa chứng minh board/revision đã nhận.
- **UNVERIFIED:** chưa có ảnh, schematic khớp revision, continuity hoặc số đo để xác nhận board thực tế.

Trong các bảng reference, `V/U` nghĩa **VERIFIED ở tài liệu reference / UNVERIFIED trên board thực tế**. Nó không phải trạng thái được phép sử dụng chân. `TBD` là trường chưa điền; mọi test phụ thuộc trường đó phải BLOCKED. MCU pin được ghi bằng tên GPIO/pad, không phải số vị trí header hay số chân package, trừ khi có ghi rõ.

Không có ảnh PCB thực tế trong phiên này. Tên STM32 Mini Type-C chỉ liên hệ **INFERRED** với sản phẩm IC44303; revision cả hai board **UNVERIFIED**. Tên seller, N16R8 hoặc “DevKitC compatible” không xác nhận đường mạch.

## Sources

| ID | Nguồn | Đã xác minh / giới hạn |
|---|---|---|
| S1 | [ThegioiIC STM32F407VET6 Type-C](https://www.thegioiic.com/mach-stm32f407vet6-type-c) | Mã hiển thị **IC44303**, product ID44303. `manufacturer_code`, `sheetfile`, `sheetname` rỗng trong HTML đã tải. Thuộc tính nguồn5V, mô tả1.8–3.6V; chưa xác định đường cấp tương ứng |
| S2 | [Tuyết Nga ESP32-S3-B N16R8](https://dientutuyetnga.com/products/weact-thu-phat-wifi-ble-esp32-s3-b-n16r8) | Product ID1064270597, variant1146593109; SKU metadata `TRẮNGA7-XXX`. Đây là mã seller, không phải revision PCB. Trang liên kết repository WeAct |
| S3 | [Schematic WeAct ESP32S3-B, pinned commit](https://github.com/WeActStudio/WeActStudio.ESP32S3-AorB/blob/c53183ea4aa96c6b7abf2b1dc153bafa22d2b96e/ESP32S3-B/Hardware/ESP32_S3_B_Sch%20.pdf) | Một trang B reference có header nets/layout. Không thấy revision rõ và chưa đối chiếu PCB đang có; không dùng bản A |
| S4 | [ST DS8626 Rev12](https://www.st.com/resource/en/datasheet/stm32f407ve.pdf) | MCU STM32F407VE, Table7/9 pin/AF, Table14 điều kiện hoạt động; không phải schematic Mini Type-C |
| S5 | [Espressif S3 datasheet v2.2](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf) | Pin overview, power domains, straps, recommended operating conditions; không phải header mapping của board thực tế |
| S6 | [IDF5.5.1 GPIO restrictions](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/gpio.html) | GPIO26–32 memory; octal/R8 thêm33–37; straps0/3/45/46; USB19/20 |
| S7 | [ST AN2606, STM32F40xxx/41xxx](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf) | Rev71, mục34: ROM boot/DFU phụ thuộc bootloader version và HSE; không suy từ clock của app |
| S8 | [Espressif S3 boot mode](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html) | Boot straps và tác động DTR/RTS; không tự áp dụng thao tác nút/strap khi board chưa được xác minh |
| SW | [STM BSP](../stm32/BSP/board.c), [ESP main](../esp32/main/main.c), [ESP defaults](../esp32/sdkconfig.defaults) | Software candidate USART2 PA2/PA3 OFF; ESP UART1 pins=-1/gate OFF. Không xác nhận hardware |

S3 PDF SHA256: `910a41eea1b8d111e8efc78667eb01d95a30b6e7ae2b9ce7567afcafd3913e68`; commit `c53183ea4aa96c6b7abf2b1dc153bafa22d2b96e`; blob `d27f6ad987b174aa9515e7207ec9a8fa4be45e95`. Evidence snapshot, HTML fields và ảnh render ở `../build/phase2d/sources/`.

## 1. STM32F407VET6 Mini Type-C — physical header chưa xác định

Chưa có schematic đúng board/revision hoặc ảnh hai mặt. Vì vậy **không gán số header**. Các PA/PB dưới đây là MCU reference để kiểm tra sau này, không là pinout đã chọn. Silicon VDD1.8–3.6V không xác định mức cấp vào USB/header; dải board input và rail3V3 còn UNVERIFIED.

| Board pin | MCU pin | Chức năng | Logic voltage | Boot/debug conflict | Peripheral assignment | Evidence source | Verification status |
|---|---|---|---|---|---|---|---|
| USB Type-C: routing TBD | VDD qua power path TBD | Nguồn vào board | Seller5V vs mô tả1.8–3.6V; chưa duyệt | Regulator/backfeed/USB power roles chưa rõ | Chưa gán nguồn | S1/S4 | VERIFIED seller conflict; UNVERIFIED power path |
| Header rail: vị trí TBD | VDD/VDDA: net TBD | Rail nominal3V3 cần xác nhận | Actual TBD; LDO tolerance TBD | Không coi VCAP/VBAT là3V3 | Chưa duyệt điểm đo/cấp | S1/S4 | UNVERIFIED PCB |
| Header GND: vị trí TBD | VSS/VSSA | Reference ground | 0V reference; continuity TBD | Probe/USB ground paths TBD | Chưa duyệt GND kết nối | S4 | V/U |
| UART TX: vị trí TBD | PA2 | USART2_TX, AF7 | Theo VDD; actual TBD | Có AF khác; onboard load TBD | Candidate software OFF; chưa chọn header | S4 Table9/SW | V/U |
| UART RX: vị trí TBD | PA3 | USART2_RX, AF7 | Theo VDD; actual TBD | ADC/AF khác; onboard load TBD | Candidate software OFF; chưa chọn header | S4 Table9/SW | V/U |
| USB D−: routing TBD | PA11 | USB OTG_FS_DM, AF10 | USB PHY domain; không là UART TTL | USB data | Không gán UART | S4/S7 | V/U |
| USB D+: routing TBD | PA12 | USB OTG_FS_DP, AF10 | USB PHY domain | USB data | Không gán UART | S4/S7 | V/U |
| Debug: vị trí TBD | PA13 | SWDIO/JTMS | Theo target VDD; actual TBD | SWD/JTAG | Giữ cho debug; không chọn UART | S4 Table9 | V/U |
| Debug: vị trí TBD | PA14 | SWCLK/JTCK | Theo target VDD; actual TBD | SWD/JTAG | Giữ cho debug; không chọn UART | S4 Table9 | V/U |
| Debug: vị trí TBD | PB3 | SWO/JTDO | Theo target VDD; actual TBD | Trace/JTAG | Chưa gán trace hoặc UART | S4 Table9 | V/U |
| Reset: nút/header TBD | NRST | Reset MCU | Theo reset specifications/VDD | Reset/debugger tác động boot | Không gán UART | S4 | V/U |
| Boot strap: vị trí TBD | BOOT0 | Boot selection | Theo target VDD; actual TBD | Thay đổi startup target | Không gán UART; chưa đổi strap | S4/S7 | V/U |
| Boot strap: vị trí TBD | PB2/BOOT1 | Boot selection/GPIO | Theo target VDD; actual TBD | BOOT1 sampling | Không gán UART | S4 Table7/S7 | V/U |
| Crystal: vị trí/marking TBD | PH0/PH1 | HSE oscillator | Clock domain | ROM DFU cần đối chiếu HSE | App hiện HSI; không suy board crystal | S4/S7/SW | V/U |
| Các header/pins còn lại | TBD | Chưa inventory vật lý | TBD | TBD | UNASSIGNED | Cần ảnh/schematic đúng revision | UNVERIFIED |

Không lấy header mapping của Core407V, CAN485, WeAct STM32 khác hoặc Mini-USB cùng MCU để điền bảng này. Không có pin nguồn/debug/UART nào đã được phê duyệt trên IC44303 thực tế.

## 2. WeAct ESP32-S3-B — matrix của tài liệu reference

`REF-L01..L22` / `REF-R01..R22` là **tọa độ do hồ sơ này đặt trên hình reference S3**, nhìn mặt linh kiện trong hình, anten phía trên, hai USB phía dưới. Đây không phải tên connector in trên PCB và không tự gán cho ảnh board đã nhận. Sau khi có ảnh phải đối chiếu orientation, pin1, số hàng/chân và revision trước khi chuyển sang actual map.

`3V3-ref` = nominal VCC_3V3 domain trong S3/S5, actual voltage chưa đo. `GPIO chưa gán` không chứng minh free GPIO: vẫn phải kiểm tra load/onboard routing và future peripherals. Mọi ô V/U dưới đây giữ **UNVERIFIED actual PCB**, gồm cả GND/nguồn.

### Header trái của reference S3

| Board pin | MCU pin | Chức năng | Logic voltage | Boot/debug conflict | Peripheral assignment | Evidence source | Verification status |
|---|---|---|---|---|---|---|---|
| REF-L01 / actual TBD | Supply net VCC_3V3 | Rail3V3 | 3.3V nominal ref | LDO output/backfeed | Không gán cấp nguồn | S3/S5 | V/U |
| REF-L02 / actual TBD | Supply net VCC_3V3 | Rail3V3 | 3.3V nominal ref | Chung net L01 trong ref | Không gán cấp nguồn | S3/S5 | V/U |
| REF-L03 / actual TBD | CHIP_PU | Enable/reset | 3V3-ref | Reset, SW3, auto-reset | Không gán UART | S3/S8 | V/U |
| REF-L04 / actual TBD | GPIO4 | GPIO | 3V3-ref | Onboard load actual TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-L05 / actual TBD | GPIO5 | GPIO | 3V3-ref | Onboard load actual TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-L06 / actual TBD | GPIO6 | GPIO | 3V3-ref | Onboard load actual TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-L07 / actual TBD | GPIO7 | GPIO | 3V3-ref | Onboard load actual TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-L08 / actual TBD | GPIO15 | GPIO | 3V3-ref | AF/clock uses cần review | GPIO chưa gán | S3/S5 | V/U |
| REF-L09 / actual TBD | GPIO16 | GPIO | 3V3-ref | AF/clock uses cần review | GPIO chưa gán | S3/S5 | V/U |
| REF-L10 / actual TBD | GPIO17 | GPIO | 3V3-ref | Actual routing/load TBD | UART **chưa chọn** | S3/S5/SW | V/U |
| REF-L11 / actual TBD | GPIO18 | GPIO | 3V3-ref | Actual routing/load TBD | UART **chưa chọn** | S3/S5/SW | V/U |
| REF-L12 / actual TBD | GPIO8 | GPIO | 3V3-ref | Actual routing/load TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-L13 / actual TBD | GPIO3 | GPIO/strap | 3V3-ref | Boot/JTAG strap | Loại khỏi UART selection ban đầu | S3/S5/S6 | V/U |
| REF-L14 / actual TBD | GPIO46 | GPIO/strap | 3V3-ref | Boot strap | Loại khỏi UART selection ban đầu | S3/S5/S8 | V/U |
| REF-L15 / actual TBD | GPIO9 | GPIO | 3V3-ref | Actual routing/load TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-L16 / actual TBD | GPIO10 | GPIO | 3V3-ref | Actual routing/load TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-L17 / actual TBD | GPIO11 | GPIO | 3V3-ref | Actual routing/load TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-L18 / actual TBD | GPIO12 | GPIO | 3V3-ref | Actual routing/load TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-L19 / actual TBD | GPIO13 | GPIO | 3V3-ref | Actual routing/load TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-L20 / actual TBD | GPIO14 | GPIO | 3V3-ref | Actual routing/load TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-L21 / actual TBD | Net +5V, không phải MCU GPIO | Power rail | +5V label ref; input limits TBD | USB diode/bypass/power sharing | Không gán cấp nguồn | S3 | V/U |
| REF-L22 / actual TBD | GND | Ground | 0V reference; actual TBD | Ground/backfeed plan | Chưa duyệt nối ground | S3 | V/U |

### Header phải của reference S3

| Board pin | MCU pin | Chức năng | Logic voltage | Boot/debug conflict | Peripheral assignment | Evidence source | Verification status |
|---|---|---|---|---|---|---|---|
| REF-R01 / actual TBD | GND | Ground | 0V reference; actual TBD | Ground paths TBD | Chưa duyệt nối ground | S3 | V/U |
| REF-R02 / actual TBD | U0TXD/GPIO43 | UART0 TX | 3V3-ref | Còn nối RXD của USB-UART U7 | Không chọn inter-MCU UART | S3/S5 | V/U |
| REF-R03 / actual TBD | U0RXD/GPIO44 | UART0 RX | 3V3-ref | Còn nối TXD của USB-UART U7 | Không chọn inter-MCU UART | S3/S5 | V/U |
| REF-R04 / actual TBD | GPIO1 | GPIO | 3V3-ref | Actual routing/load TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-R05 / actual TBD | GPIO2 | GPIO | 3V3-ref | Actual routing/load TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-R06 / actual TBD | GPIO42/MTMS | GPIO/JTAG TMS | 3V3-ref | External JTAG | Giữ ngoài UART selection ban đầu | S3/S5 | V/U |
| REF-R07 / actual TBD | GPIO41/MTDI | GPIO/JTAG TDI | 3V3-ref | External JTAG | Giữ ngoài UART selection ban đầu | S3/S5 | V/U |
| REF-R08 / actual TBD | GPIO40/MTDO | GPIO/JTAG TDO | 3V3-ref | External JTAG | Giữ ngoài UART selection ban đầu | S3/S5 | V/U |
| REF-R09 / actual TBD | GPIO39/MTCK | GPIO/JTAG TCK | 3V3-ref | External JTAG | Giữ ngoài UART selection ban đầu | S3/S5 | V/U |
| REF-R10 / actual TBD | GPIO38 | GPIO | 3V3-ref | Actual routing/load TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-R11 / actual TBD | GPIO37 | Octal memory use | VDD_SPI-related; actual TBD | R8 PSRAM/memory | **Không dùng UART/GPIO khác trong plan này** | S3/S5/S6 | V/U |
| REF-R12 / actual TBD | GPIO36 | Octal memory use | VDD_SPI-related; actual TBD | R8 PSRAM/memory | **Không dùng UART/GPIO khác trong plan này** | S3/S5/S6 | V/U |
| REF-R13 / actual TBD | GPIO35 | Octal memory use | VDD_SPI-related; actual TBD | R8 PSRAM/memory | **Không dùng UART/GPIO khác trong plan này** | S3/S5/S6 | V/U |
| REF-R14 / actual TBD | GPIO0 | BOOT/strap | 3V3-ref | SW2, download mode, auto-reset | Không chọn UART | S3/S8 | V/U |
| REF-R15 / actual TBD | GPIO45 | USER/strap | 3V3-ref | SW1, voltage/boot strap policy | Không chọn UART; không tự đổi strap | S3/S5/S6 | V/U |
| REF-R16 / actual TBD | GPIO48 | RGB LED data | 3V3-ref GPIO; LED +5V ref | WS2812B/R12 onboard | Không chọn UART | S3/S5 | V/U |
| REF-R17 / actual TBD | GPIO47 | GPIO | 3V3-ref | Actual routing/load TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-R18 / actual TBD | GPIO21 | GPIO | 3V3-ref | Actual routing/load TBD | GPIO chưa gán | S3/S5 | V/U |
| REF-R19 / actual TBD | GPIO20 | USB D+ | USB PHY /3V3 domain | Native USB/Serial-JTAG | Giữ cho USB; không chọn UART | S3/S5/S6 | V/U |
| REF-R20 / actual TBD | GPIO19 | USB D− | USB PHY /3V3 domain | Native USB/Serial-JTAG | Giữ cho USB; không chọn UART | S3/S5/S6 | V/U |
| REF-R21 / actual TBD | GND | Ground | 0V reference; actual TBD | Ground paths TBD | Chưa duyệt nối ground | S3 | V/U |
| REF-R22 / actual TBD | GND | Ground | 0V reference; actual TBD | Ground paths TBD | Chưa duyệt nối ground | S3 | V/U |

### Onboard nets ngoài header / power path

| Board pin | MCU pin | Chức năng | Logic voltage | Boot/debug conflict | Peripheral assignment | Evidence source | Verification status |
|---|---|---|---|---|---|---|---|
| S3 USB1 / actual connector TBD | Qua U7 đến UART0 | USB-UART | USB PHY; UART side3V3-ref | DTR/RTS→Q1/Q2→GPIO0/CHIP_PU | Host diagnostic path chỉ sau review | S3/S8 | V/U |
| S3 USB2 / actual connector TBD | GPIO19/20 | Native USB | USB PHY | Console/JTAG/ROM boot | Không gán UART1 vào19/20 | S3/S6 | V/U |
| U6/SPI nets; không gán header | GPIO26–32 memory signals | External Flash /memory bus | VDD_SPI policy; actual TBD | Flash access | **Reserved** | S3/S5/S6 | V/U |
| Internal/package octal nets | GPIO33–37 | R8 octal PSRAM | Memory domain | PSRAM | **Reserved**, cả khi có header35–37 | S5/S6 | VERIFIED silicon restriction; UNVERIFIED actual chip |
| U4 input/output / actual marking TBD | +5V→VCC_3V3 in ref | LDO | Nominal labels; max/dropout/current TBD | USB bypass links U8/U9, D1/D5 | Chưa duyệt input/current limit | S3 | V/U |
| SW1/SW2/SW3 / actual labels TBD | GPIO45/GPIO0/CHIP_PU | USER/BOOT/RESET | 3V3-ref | Boot voltage/reset/strap | Không thao tác nút trong phiên này | S3/S8 | V/U |

GPIO35–37 xuất hiện ở header không chứng minh sử dụng được với R8. Không suy GPIO46 “input-only” từ ESP32-S2: silicon target ở đây là ESP32-S3; dù vậy GPIO46 vẫn bị giữ ngoài lựa chọn UART do strap. Không tự đổi eFuse hoặc Flash/PSRAM voltage.

## 3. Assignment record cho kết nối tương lai — chưa điền chân

| Board pin | MCU pin | Chức năng | Logic voltage | Boot/debug conflict | Peripheral assignment | Evidence source | Verification status |
|---|---|---|---|---|---|---|---|
| STM TX actual TBD | TBD | Control→UI | Actual VOH/VOL/domain TBD | TBD | USART TBD | Exact schematic +photo +continuity +measurement cần bổ sung | UNVERIFIED / UNASSIGNED |
| STM RX actual TBD | TBD | UI→Control | Actual VIH/VIL/domain TBD | TBD | USART TBD | Như trên | UNVERIFIED / UNASSIGNED |
| ESP TX actual TBD | TBD | UI→Control | Actual VOH/VOL/domain TBD | TBD | UART1 software; GPIO TBD | Như trên | UNVERIFIED / UNASSIGNED |
| ESP RX actual TBD | TBD | Control→UI | Actual VIH/VIL/domain TBD | TBD | UART1 software; GPIO TBD | Như trên | UNVERIFIED / UNASSIGNED |
| Ground/reference actual TBD | VSS/GND TBD | Reference/return | Ground differential TBD | USB/debug ground/backfeed TBD | Chưa duyệt topology | Approved power/interconnect drawing cần bổ sung | UNVERIFIED |
| Power actual TBD | Rail/path TBD | Cấp nguồn riêng từng board | Limits/current/sequence TBD | USB/rail/probe backfeed TBD | Chưa duyệt nguồn | Exact regulator/schematic +review | UNVERIFIED |

Để chuyển một actual row sang VERIFIED cần: ảnh có orientation/pin marker, schematic đúng revision, MCU/header continuity khi unpowered, measured domain/idle levels, conflict/peripheral review, evidence file/hash, người xác nhận và ngày. Không tự chọn PA2/PA3 hoặc GPIO17/18 chỉ vì có trong source/reference. Xem [test plan](HARDWARE_BRINGUP_TEST_PLAN.md) và [báo cáo 2D](PHASE2D_HARDWARE_EVIDENCE_REPORT.md).
