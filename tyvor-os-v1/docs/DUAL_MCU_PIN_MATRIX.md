# TYVOR — Dual MCU pin matrix (schematic review)

Ngày **11/11/2026, Asia/Bangkok**. Kiến trúc: STM32F407VET6 Mini Type-C CONTROL,
WeAct ESP32-S3-B N16R8 UI, protocol v2/UART; AMOLED2.04 inch368×448 chưa có pinout.
**Đây là reference inventory và proposal, không phải bản đấu dây được duyệt.**

## Phạm vi và trạng thái

- **D** = [DevEBox PDF được người dùng chỉ định, pinned commit](https://github.com/mcauser/MCUDEV_DEVEBOX_F407VET6/blob/b32028d57e29be8eef169e3c027b25d4e74fb9a8/docs/STM32F407VX_M_schematics.pdf),
  p1 schematic, p2 PCB reference; date2018-11-23. J1–J5 bên dưới là **DevEBox**, không là header Mini Type-C.
- **W** = [WeAct ESP32S3-B PDF pinned](https://github.com/WeActStudio/WeActStudio.ESP32S3-AorB/blob/c53183ea4aa96c6b7abf2b1dc153bafa22d2b96e/ESP32S3-B/Hardware/ESP32_S3_B_Sch%20.pdf),
  p1; local `build/phase2d/sources/weact-b.pdf`. Chưa xác minh revision board đã mua.
- **S** = [ST DS8626 Rev12](https://www.st.com/resource/en/datasheet/stm32f407ve.pdf), Fig13, Tables7/9.
- **E** = [Espressif datasheet v2.2](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf),
  Tables2-1/2-4 và memory/strap sections;
  [IDF5.5.1 GPIO](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/gpio.html).
- **REFERENCE ONLY** = đã đọc tài liệu đó, **không** xác nhận board thực tế.
  **PROPOSED** = phương án logic cần review. **UNVERIFIED** = actual PCB/net/điện áp chưa có bằng chứng.
  Không có chân actual PCB nào được đánh dấu VERIFIED trong hồ sơ này.

Mọi dòng reference mang trạng thái **R/U = REFERENCE ONLY / ACTUAL UNVERIFIED**.
`U2.n` = chân LQFP100 trong D/S; `U1.n` = pad QFN56(+EP57) trong W/E,
không phải số header. Số header actual Mini/WeAct đều TBD đến khi match ảnh/revision.
`UNASSIGNED` không có nghĩa free GPIO; vẫn cần kiểm tra tải, AF và I/O budget.
L/R của WeAct là ký hiệu hồ sơ trên block PIN: trái/phải1–22 từ trên xuống;
hình layout tham chiếu: anten trên, USB dưới. Đây không phải tên connector chính thức.

Voltage keys (chưa đo): **S3**=STM nominal3V3/VDD; **E3**=ESP nominal3V3 domain;
**EMUX**=ESP pad dual-domain theo mode, không mặc định3V3; **MEM**=VDD_SPI-related,
không gán làm GPIO; **G0**=GND reference; **POWER5**=rail có tên5V trong PDF,
không phải input rating được phê duyệt; **AN**=analog rail/reference; **CLK**=oscillator;
**USB**=USB PHY signal, không phải TTL UART. VDD silicon không là điện áp cấp vào board.

## 1. UART proposal — chưa đấu nối / gates OFF

| Endpoint | MCU package pin | Reference header | Role | Logical peer | Voltage | Status |
|---|---|---|---|---|---|---|
| STM PA2 | U2.25 | D J3.32; Mini actual TBD | USART2_TX AF7 | ESP GPIO18 RX | S3→E3, actual chưa đo | **PROPOSED** |
| STM PA3 | U2.26 | D J3.31; Mini actual TBD | USART2_RX AF7 | ESP GPIO17 TX | E3→S3, actual chưa đo | **PROPOSED** |
| ESP GPIO17 | U1.23 | W L10; actual TBD | UART1_TX | STM PA3 RX | E3 | **PROPOSED** |
| ESP GPIO18 | U1.24 | W L11; actual TBD | UART1_RX | STM PA2 TX | E3 | **PROPOSED** |
| Common reference | VSS /GND | Actual locations TBD | Signal ground | Cross-board GND plan | G0 | **PROPOSED**, chưa nối |

115200 baud,8N1, không RTS/CTS; protocol v2 không đổi. Không nối TX↔TX.
Không đề xuất nối5V/3V3/VBAT hoặc reset hai MCU với nhau. MCU control tiếp tục
sở hữu safety/ARM/output logic; proposal không gán dây reset/boot/control-output từ UI tới STM.

## 2. DevEBox reference J2 — đủ44 vị trí

Không chuyển J2/J3 sang Mini Type-C chỉ vì cùng STM32F407VE.

| Board pin reference (actual TBD) | MCU pin | Function /net | Voltage | Conflict /load | Assignment | Source | Status |
|---|---|---|---|---|---|---|---|
| D J2.1 | Không phải MCU pin | 5V rail | POWER5 | USB J5 qua L1; không phải UART logic | POWER HOLD | D p1 /S | R/U |
| D J2.2 | Không phải MCU pin | 5V rail | POWER5 | USB J5 qua L1; không phải UART logic | POWER HOLD | D p1 /S | R/U |
| D J2.3 | VDD/VSS net | VDD rail | S3 | U1 output; không tự nối nguồn hai board | POWER HOLD | D p1 /S | R/U |
| D J2.4 | VDD/VSS net | VDD rail | S3 | U1 output; không tự nối nguồn hai board | POWER HOLD | D p1 /S | R/U |
| D J2.5 | VDD/VSS net | VSS/GND | G0 | Actual ground/continuity TBD | GND PROPOSED; vị trí actual TBD | D p1 /S | R/U |
| D J2.6 | VDD/VSS net | VSS/GND | G0 | Actual ground/continuity TBD | GND PROPOSED; vị trí actual TBD | D p1 /S | R/U |
| D J2.7 | U2.55 /PD8 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.8 | U2.54 /PB15 | J4 SPI2_MOSI | S3 | TFT connector DevEBox; không phải pinout AMOLED | RESERVED reference TFT | D p1 /S | R/U |
| D J2.9 | U2.57 /PD10 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.10 | U2.56 /PD9 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.11 | U2.59 /PD12 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.12 | U2.58 /PD11 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.13 | U2.61 /PD14 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.14 | U2.60 /PD13 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.15 | U2.63 /PC6 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.16 | U2.62 /PD15 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.17 | U2.65 /PC8 | SDIO_D0 | S3 | U5 microSD; chia sẻ AF khác | RESERVED reference SDIO | D p1 /S | R/U |
| D J2.18 | U2.64 /PC7 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.19 | U2.67 /PA8 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.20 | U2.66 /PC9 | SDIO_D1 | S3 | U5 microSD; chia sẻ AF khác | RESERVED reference SDIO | D p1 /S | R/U |
| D J2.21 | U2.69 /PA10 | USART1_RX | S3 | UART1 boot/download candidate; không chọn UART liên MCU | RESERVED from proposal | D p1 /S | R/U |
| D J2.22 | U2.68 /PA9 | USART1_TX | S3 | UART1 boot/download candidate; không chọn UART liên MCU | RESERVED from proposal | D p1 /S | R/U |
| D J2.23 | U2.71 /PA12 | USB_FS_DP | USB | J5 data qua R2/R3; USB device | RESERVED USB | D p1 /S | R/U |
| D J2.24 | U2.70 /PA11 | USB_FS_DM | USB | J5 data qua R2/R3; USB device | RESERVED USB | D p1 /S | R/U |
| D J2.25 | U2.78 /PC10 | SDIO_D2 | S3 | U5 microSD; chia sẻ AF khác | RESERVED reference SDIO | D p1 /S | R/U |
| D J2.26 | U2.77 /PA15 | U3 W25Q16 CS /JTDI | S3 | SPI3 AF6 theo MCU; tên net SPI1_* trong PDF không đổi AF | RESERVED reference Flash/debug | D p1 /S | R/U |
| D J2.27 | U2.80 /PC12 | SDIO_CK | S3 | U5 microSD; chia sẻ AF khác | RESERVED reference SDIO | D p1 /S | R/U |
| D J2.28 | U2.79 /PC11 | SDIO_D3 | S3 | U5 microSD; chia sẻ AF khác | RESERVED reference SDIO | D p1 /S | R/U |
| D J2.29 | U2.82 /PD1 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.30 | U2.81 /PD0 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.31 | U2.84 /PD3 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.32 | U2.83 /PD2 | SDIO_CMD | S3 | U5 microSD; chia sẻ AF khác | RESERVED reference SDIO | D p1 /S | R/U |
| D J2.33 | U2.86 /PD5 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.34 | U2.85 /PD4 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.35 | U2.88 /PD7 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.36 | U2.87 /PD6 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.37 | U2.91 /PB5 | U3 SI | S3 | SPI3 AF6 theo MCU; tên net SPI1_* trong PDF không đổi AF | RESERVED reference Flash/debug | D p1 /S | R/U |
| D J2.38 | U2.89 /PB3 | U3 CLK /SWO-JTDO | S3 | SPI3 AF6 theo MCU; tên net SPI1_* trong PDF không đổi AF | RESERVED reference Flash/debug | D p1 /S | R/U |
| D J2.39 | U2.93 /PB7 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.40 | U2.92 /PB6 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.41 | U2.96 /PB9 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.42 | U2.95 /PB8 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.43 | U2.98 /PE1 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J2.44 | U2.97 /PE0 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |

## 3. DevEBox reference J3 — đủ44 vị trí

| Board pin reference (actual TBD) | MCU pin | Function /net | Voltage | Conflict /load | Assignment | Source | Status |
|---|---|---|---|---|---|---|---|
| D J3.1 | VDD/VSS net | VDD rail | S3 | U1 output; không tự nối nguồn hai board | POWER HOLD | D p1 /S | R/U |
| D J3.2 | VDD/VSS net | VDD rail | S3 | U1 output; không tự nối nguồn hai board | POWER HOLD | D p1 /S | R/U |
| D J3.3 | VDD/VSS net | VSS/GND | G0 | Actual ground/continuity TBD | GND PROPOSED; vị trí actual TBD | D p1 /S | R/U |
| D J3.4 | VDD/VSS net | VSS/GND | G0 | Actual ground/continuity TBD | GND PROPOSED; vị trí actual TBD | D p1 /S | R/U |
| D J3.5 | VDD/VSS net | VSS/GND | G0 | Actual ground/continuity TBD | GND PROPOSED; vị trí actual TBD | D p1 /S | R/U |
| D J3.6 | VDD/VSS net | VSS/GND | G0 | Actual ground/continuity TBD | GND PROPOSED; vị trí actual TBD | D p1 /S | R/U |
| D J3.7 | U2.22 /VDDA | Analog supply U2.22 | AN | R34 từ3V3; không làm nguồn liên-board | POWER HOLD | D p1 /S | R/U |
| D J3.8 | U2.21 /VREF+ | Analog reference U2.21 | AN | R11 về VDDA; không làm GPIO/nguồn tải | RESERVED | D p1 /S | R/U |
| D J3.9 | U2.53 /PB14 | J4 SPI2_MISO | S3 | TFT connector DevEBox; không phải pinout AMOLED | RESERVED reference TFT | D p1 /S | R/U |
| D J3.10 | U2.52 /PB13 | J4 SPI2_SCK | S3 | TFT connector DevEBox; không phải pinout AMOLED | RESERVED reference TFT | D p1 /S | R/U |
| D J3.11 | U2.51 /PB12 | J4 CS | S3 | TFT connector DevEBox; không phải pinout AMOLED | RESERVED reference TFT | D p1 /S | R/U |
| D J3.12 | U2.48 /PB11 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.13 | U2.47 /PB10 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.14 | U2.46 /PE15 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.15 | U2.45 /PE14 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.16 | U2.44 /PE13 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.17 | U2.43 /PE12 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.18 | U2.42 /PE11 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.19 | U2.41 /PE10 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.20 | U2.40 /PE9 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.21 | U2.39 /PE8 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.22 | U2.38 /PE7 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.23 | U2.36 /PB1 | J4 BLK | S3 | TFT connector DevEBox; không phải pinout AMOLED | RESERVED reference TFT | D p1 /S | R/U |
| D J3.24 | U2.35 /PB0 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.25 | U2.34 /PC5 | J4 RS | S3 | TFT connector DevEBox; không phải pinout AMOLED | RESERVED reference TFT | D p1 /S | R/U |
| D J3.26 | U2.33 /PC4 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.27 | U2.32 /PA7 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.28 | U2.31 /PA6 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.29 | U2.30 /PA5 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.30 | U2.29 /PA4 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.31 | U2.26 /PA3 | USART2_RX AF7 | S3 | ADC123_IN3 /TIM2/5/9/ETH/ULPI | PROPOSED UART; gate OFF | D p1 /S | R/U |
| D J3.32 | U2.25 /PA2 | USART2_TX AF7 | S3 | ADC123_IN2 /TIM2/5/9/ETH_MDIO | PROPOSED UART; gate OFF | D p1 /S | R/U |
| D J3.33 | U2.24 /PA1 | LED D2 | S3 | LED active-low qua R14; ADC/timer khác | ONBOARD REFERENCE | D p1 /S | R/U |
| D J3.34 | U2.23 /PA0 | WKUP / USER K1 | S3 | K1 qua R16; không dùng làm ARM thật | ONBOARD REFERENCE | D p1 /S | R/U |
| D J3.35 | U2.18 /PC3 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.36 | U2.17 /PC2 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.37 | U2.16 /PC1 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.38 | U2.15 /PC0 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.39 | U2.7 /PC13 | GPIO | S3 | RTC/TAMPER, limited drive theo S; không mặc định LED | UNASSIGNED | D p1 /S | R/U |
| D J3.40 | U2.5 /PE6 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.41 | U2.4 /PE5 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.42 | U2.3 /PE4 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.43 | U2.2 /PE3 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |
| D J3.44 | U2.1 /PE2 | GPIO | S3 | AF khác; tải actual/future functions TBD | UNASSIGNED | D p1 /S | R/U |

## 4. DevEBox — debug, power, reset và MCU pins ngoài J2/J3

J1 pin1 là **BOOT0**, tuyệt đối không suy thành NRST của một pinout SWD5-pin khác.
Tất cả vị trí actual của Mini Type-C vẫn TBD; không dùng các hàng này để đấu dây.

| Board pin reference (actual TBD) | MCU pin | Function /net | Voltage | Conflict /load | Assignment | Source | Status |
|---|---|---|---|---|---|---|---|
| D J1.1 | U2.94 /BOOT0 | R7 pull-down reference | S3 | BOOT strap; không phải NRST | RESERVED BOOT | D p1 /S | R/U |
| D J1.2 | VDD net | 3V3 reference | S3 | Target reference, không tự cấp từ debugger | POWER HOLD | D p1 /S | R/U |
| D J1.3 | VSS net | GND | G0 | Continuity actual TBD | RESERVED DEBUG | D p1 /S | R/U |
| D J1.4 | U2.72 /PA13 | SWDIO/JTMS | S3 | Debug | RESERVED DEBUG | D p1 /S | R/U |
| D J1.5 | U2.76 /PA14 | SWCLK/JTCK | S3 | Debug | RESERVED DEBUG | D p1 /S | R/U |
| D J4.1 | VDD net | 3V3 | S3 | TFT supply reference, không AMOLED approval | POWER HOLD | D p1 /S | R/U |
| D J4.2 | VSS net | GND | G0 | TFT connector reference | POWER HOLD | D p1 /S | R/U |
| D J4.3 | U2.54 /PB15 | SPI2_MOSI /SDI | S3 | TFT reference | RESERVED | D p1 /S | R/U |
| D J4.4 | U2.52 /PB13 | SPI2_SCK /SCL | S3 | SCL ở đây là SPI clock, không là touch I2C | RESERVED | D p1 /S | R/U |
| D J4.5 | U2.51 /PB12 | CS | S3 | TFT reference | RESERVED | D p1 /S | R/U |
| D J4.6 | U2.53 /PB14 | SPI2_MISO /SDO | S3 | TFT reference | RESERVED | D p1 /S | R/U |
| D J4.7 | U2.34 /PC5 | RS | S3 | TFT reference | RESERVED | D p1 /S | R/U |
| D J4.8 | U2.36 /PB1 | BLK | S3 | TFT backlight net; không là AMOLED power | RESERVED | D p1 /S | R/U |
| D J5.1 | Không phải MCU pin | USB VBUS →L1→5V rail | POWER5 | Micro-USB reference, không Type-C; L1 label500mA không xác nhận limit actual | POWER HOLD | D p1 /S | R/U |
| D J5.2 | U2.70 /PA11 | D− qua R2=0R | USB | USB_FS_DM | RESERVED USB | D p1 /S | R/U |
| D J5.3 | U2.71 /PA12 | D+ qua R3=0R | USB | USB_FS_DP | RESERVED USB | D p1 /S | R/U |
| D J5.4 | NC reference | USB ID | USB | Không nối trong schematic reference | RESERVED | D p1 /S | R/U |
| D J5.5 /shell6–9 | VSS net | GND/shell reference | G0 | Actual Type-C shield/ground paths TBD | POWER HOLD | D p1 /S | R/U |
| D RST button/net | U2.14 /NRST | Active-low reset, C6=100nF | S3 | Không xuất NRST trên J1; actual reset pad TBD | RESERVED RESET | D p1 /S | R/U |
| D R8 net | U2.37 /PB2 | BOOT1 pull-down10k | S3 | Boot strap, không gán GPIO | RESERVED BOOT | D p1 /S | R/U |
| D U3 Flash SO | U2.90 /PB4 | MISO /NJTRST | S3 | W25Q16 và full-JTAG conflict | RESERVED FLASH/DEBUG | D p1 /S | R/U |
| D Y1 pins | U2.8 /PC14; U2.9 /PC15 | 32.768kHz LSE | CLK | Crystal+caps, không GPIO free | RESERVED CLOCK | D p1 /S | R/U |
| D Y2 pins | U2.12 /PH0; U2.13 /PH1 | 8MHz HSE | CLK | Chỉ clock DevEBox; BSP Mini hiện HSI16MHz | RESERVED CLOCK | D p1 /S | R/U |
| D U2 power | U2.11,19,28,50,75,100 | VDD /3V3 | S3 | U1 AMS1117-3.3 reference; capacity/thermal actual chưa biết | POWER HOLD | D p1 /S | R/U |
| D U2 ground | U2.10,27,74,99 | VSS | G0 | Reference ground | POWER HOLD | D p1 /S | R/U |
| D U2 analog ground | U2.20 | VSSA; PDF gọi VREF− | G0 | Không tách rail GND khi chưa xác minh | RESERVED ANALOG | D p1 /S | R/U |
| D U2 analog supply | U2.22 /VDDA | 3V3 qua R34=0R | AN | J3.7, decoupling C23 | POWER HOLD | D p1 /S | R/U |
| D U2 reference | U2.21 /VREF+ | VDDA qua R11=0R | AN | J3.8; không dùng làm nguồn tải | RESERVED ANALOG | D p1 /S | R/U |
| D VCAP capacitors | U2.49 /VCAP1; U2.73 /VCAP2 | C21/C22=2.2uF xuống GND | AN | Internal regulator nodes; không nối vào3V3/GPIO | RESERVED — no external load | D p1 /S | R/U |
| D B1/Q1/C7 | U2.6 /VBAT | Battery reference /BAT54C | AN | Backup domain, không dùng làm nguồn UI; actual battery type/path TBD | POWER HOLD | D p1 /S | R/U |

DevEBox U3 **W25Q16**: CS=PA15, CLK=PB3, SO=PB4, SI=PB5. Các net `SPI1_*`
trên PDF nằm trên GPIO có peripheral **SPI3 AF6**; không suy rằng Flash dùng PA5/6/7.
PB3 còn SWO/JTDO, PB4 NJTRST, PA15 JTDI; SWD PA13/14 vẫn là cặp riêng.
U5 microSD dùng PC8/9/10/11/12 và PD2. Không thấy các tải này nối PA2/PA3 trong D.
Đây không là bằng chứng tải trên actual Mini Type-C.

## 5. WeAct-B reference — đủ44 vị trí header

R11–R13 (GPIO37/36/35) có trên header trong W nhưng **không được dùng** cho UART,
AMOLED/touch hoặc GPIO khác trong phương án N16R8. Giữ GPIO26–37 cho memory;
W không thể hiện toàn bộ kết nối PSRAM bên trong chip R8.

### Header L — reference orientation

| Board pin reference (actual TBD) | MCU pin | Function /net | Voltage | Conflict /load | Assignment | Source | Status |
|---|---|---|---|---|---|---|---|
| W L01 | VDD3P3 net | VCC_3V3 rail | E3 | U4 LDO output; không cấp tải/chập rail khi chưa review | POWER HOLD | W p1 /E | R/U |
| W L02 | VDD3P3 net | VCC_3V3 rail | E3 | U4 LDO output; không cấp tải/chập rail khi chưa review | POWER HOLD | W p1 /E | R/U |
| W L03 | U1.4 /CHIP_PU | EN/reset, SW3 | E3 | R1 pull-up, C7 RC và auto-reset Q1/Q2 | RESERVED RESET | W p1 /E | R/U |
| W L04 | U1.9 /GPIO4 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W L05 | U1.10 /GPIO5 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W L06 | U1.11 /GPIO6 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W L07 | U1.12 /GPIO7 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W L08 | U1.21 /GPIO15 | GPIO | E3 | Có XTAL32K alternate; future panel TBD | UNASSIGNED | W p1 /E | R/U |
| W L09 | U1.22 /GPIO16 | GPIO | E3 | Có XTAL32K alternate; future panel TBD | UNASSIGNED | W p1 /E | R/U |
| W L10 | U1.23 /GPIO17 | UART1_TX /U1TXD IO_MUX | E3 | ADC2_CH6; future AMOLED/touch TBD | PROPOSED UART TX; config vẫn -1 | W p1 /E | R/U |
| W L11 | U1.24 /GPIO18 | UART1_RX /U1RXD IO_MUX | E3 | ADC2_CH7 /CLK_OUT3; future panel TBD | PROPOSED UART RX; config vẫn -1 | W p1 /E | R/U |
| W L12 | U1.13 /GPIO8 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W L13 | U1.8 /GPIO3 | JTAG-source strap | E3 | Strapping pin | RESERVED STRAP | W p1 /E | R/U |
| W L14 | U1.52 /GPIO46 | Boot /ROM-print strap | E3 | Strapping pin | RESERVED STRAP | W p1 /E | R/U |
| W L15 | U1.14 /GPIO9 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W L16 | U1.15 /GPIO10 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W L17 | U1.16 /GPIO11 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W L18 | U1.17 /GPIO12 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W L19 | U1.18 /GPIO13 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W L20 | U1.19 /GPIO14 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W L21 | Không phải MCU pin | +5V rail | POWER5 | USB1/2 diodes +U8/U9 bypass; không nối rail liên-board | POWER HOLD | W p1 /E | R/U |
| W L22 | EP57 /GND net | GND /EP57 | G0 | Actual ground/continuity TBD | GND PROPOSED; actual TBD | W p1 /E | R/U |
### Header R — reference orientation

| Board pin reference (actual TBD) | MCU pin | Function /net | Voltage | Conflict /load | Assignment | Source | Status |
|---|---|---|---|---|---|---|---|
| W R01 | EP57 /GND net | GND /EP57 | G0 | Actual ground/continuity TBD | GND PROPOSED; actual TBD | W p1 /E | R/U |
| W R02 | U1.49 /GPIO43 | U0TXD | E3 | Còn nối USB-UART U7; download/log contention | RESERVED USB-UART | W p1 /E | R/U |
| W R03 | U1.50 /GPIO44 | U0RXD | E3 | Còn nối USB-UART U7; download/log contention | RESERVED USB-UART | W p1 /E | R/U |
| W R04 | U1.6 /GPIO1 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W R05 | U1.7 /GPIO2 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W R06 | U1.48 /GPIO42 | MTMS | E3 | External JTAG, không dùng trong proposal | RESERVED DEBUG | W p1 /E | R/U |
| W R07 | U1.47 /GPIO41 | MTDI | E3 | External JTAG, không dùng trong proposal | RESERVED DEBUG | W p1 /E | R/U |
| W R08 | U1.45 /GPIO40 | MTDO | E3 | External JTAG, không dùng trong proposal | RESERVED DEBUG | W p1 /E | R/U |
| W R09 | U1.44 /GPIO39 | MTCK | E3 | External JTAG, không dùng trong proposal | RESERVED DEBUG | W p1 /E | R/U |
| W R10 | U1.43 /GPIO38 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W R11 | U1.42 /GPIO37 | R8 Octal PSRAM | MEM | SPIIO6 /SPIIO7 /SPIDQS; header không đồng nghĩa free | RESERVED MEMORY — không dùng | W p1 /E | R/U |
| W R12 | U1.41 /GPIO36 | R8 Octal PSRAM | MEM | SPIIO6 /SPIIO7 /SPIDQS; header không đồng nghĩa free | RESERVED MEMORY — không dùng | W p1 /E | R/U |
| W R13 | U1.40 /GPIO35 | R8 Octal PSRAM | MEM | SPIIO6 /SPIIO7 /SPIDQS; header không đồng nghĩa free | RESERVED MEMORY — không dùng | W p1 /E | R/U |
| W R14 | U1.5 /GPIO0 | BOOT /SW2 | E3 | Boot strap, auto-reset Q1/Q2 | RESERVED BOOT | W p1 /E | R/U |
| W R15 | U1.51 /GPIO45 | USER SW1 /VDD_SPI strap | E3 | Pull-up R8; actual voltage/eFuse policy chưa đọc | RESERVED STRAP/USER | W p1 /E | R/U |
| W R16 | U1.36 /GPIO48 | RGB LED1 data | EMUX | R12→WS2812B; LED rail +5V ref | RESERVED LED | W p1 /E | R/U |
| W R17 | U1.37 /GPIO47 | GPIO | EMUX | SPICLK_P pad37, IO domain phụ thuộc mode; actual TBD | UNASSIGNED | W p1 /E | R/U |
| W R18 | U1.27 /GPIO21 | GPIO | E3 | AF khác/future panel chưa gán | UNASSIGNED | W p1 /E | R/U |
| W R19 | U1.26 /GPIO20 | USB D+ | USB | USB2 native USB /Serial-JTAG console | RESERVED USB | W p1 /E | R/U |
| W R20 | U1.25 /GPIO19 | USB D- | USB | USB2 native USB /Serial-JTAG console | RESERVED USB | W p1 /E | R/U |
| W R21 | EP57 /GND net | GND /EP57 | G0 | Actual ground/continuity TBD | GND PROPOSED; actual TBD | W p1 /E | R/U |
| W R22 | EP57 /GND net | GND /EP57 | G0 | Actual ground/continuity TBD | GND PROPOSED; actual TBD | W p1 /E | R/U |

## 6. WeAct-B — internal/reserved pins, nguồn và reset

Các nhóm sau bổ sung các pad không được liệt kê trong header; không là lựa chọn
cho inter-MCU UART. Tất cả vẫn R/U, actual positions/markings TBD.

| Board pin reference (actual TBD) | MCU pin | Function /net | Voltage | Conflict /load | Assignment | Source | Status |
|---|---|---|---|---|---|---|---|
| W U1 RF | U1.1 /LNA_IN | RF matching/antenna | RF | Không tác động; không RF feature trong task | RESERVED | W p1 /E | R/U |
| W U1 power | U1.2,3 /VDD3P3; U1.20 /VDD3P3_RTC; U1.46 /VDD3P3_CPU; U1.55,56 /VDDA | VCC_3V3 /VDD3P3 rails | E3 | U4 LDO, L2 filter, decoupling; actual measurements TBD | POWER HOLD | W p1 /E | R/U |
| W U1 ground | U1.57 /EP | GND | G0 | Actual ground/backfeed plan TBD | POWER HOLD | W p1 /E | R/U |
| W U1 memory CS | U1.28 /GPIO26 SPICS1 | Memory chip-select | MEM | R8 memory reserved; schematic shows NC at pad28, không chứng minh free | RESERVED MEMORY | W p1 /E | R/U |
| W U1 memory supply | U1.29 /VDD_SPI | Memory rail | MEM | U6 Flash supply; strap/eFuse policy chưa đọc | RESERVED MEMORY POWER | W p1 /E | R/U |
| W U1 memory bus | U1.30 /GPIO27; .31 /GPIO28; .32 /GPIO29; .33 /GPIO30; .34 /GPIO31; .35 /GPIO32 | SPIHD/WP/CS0/CLK/Q/D | MEM | SPI0/1 memory, U6 Flash | RESERVED MEMORY | W p1 /E | R/U |
| W U1 memory upper IO | U1.38 /GPIO33; .39 /GPIO34 | SPIIO4/5 | MEM | In-package R8 Octal PSRAM | RESERVED MEMORY | W p1 /E | R/U |
| W U1 memory upper IO | U1.40 /GPIO35; .41 /GPIO36; .42 /GPIO37 | SPIIO6/7/DQS | MEM | Đã thấy trên R13/R12/R11, vẫn reserved | RESERVED MEMORY | W p1 /E | R/U |
| W U1 crystal | U1.53 /XTAL_N; .54 /XTAL_P | X1 40MHz trong W | CLK | Không dùng làm GPIO hoặc áp sang panel | RESERVED CLOCK | W p1 /E | R/U |
| W USB1 A4/B9,A9/B4 | Không phải MCU pin | VBUS →D1→+5V, bypass U8 | POWER5 | Bypass population/backfeed chưa xác minh; không tự bridge | POWER HOLD | W p1 /E | R/U |
| W USB1 D± | U7 USB bridge, không GPIO19/20 | USB_DP/USB_DN | USB | USB-UART path | RESERVED USB | W p1 /E | R/U |
| W USB2 A4/B9,A9/B4 | Không phải MCU pin | VBUS →D5→+5V, bypass U9 | POWER5 | Chung rail nội bộ reference; không suy cho phép nối nhiều nguồn | POWER HOLD | W p1 /E | R/U |
| W USB2 D± | U1.25 /GPIO19; U1.26 /GPIO20 | Native USB /Serial-JTAG | USB | ESP console đang dùng đường này trong software | RESERVED USB | W p1 /E | R/U |
| W USB1/USB2 CC | Không phải MCU pin | CC1/CC2 resistors5.1k xuống GND | USB | USB role/power thật cần revision match | POWER HOLD | W p1 /E | R/U |
| W USB1/USB2 GND/shell | GND net /EP57 | Reference ground | G0 | Không suy đường shield/ground của actual PCB | POWER HOLD | W p1 /E | R/U |
| W U4 /L01,L02 | VCC_3V3 net | LDO3.3V output | E3 | Schematic không ghi exact part/rating U4 | POWER HOLD | W p1 /E | R/U |
| W U6 | U1 memory bus /VDD_SPI | External SPI Flash | MEM | W chỉ ghi FLASH; không chứng minh16MB thực tế | RESERVED MEMORY | W p1 /E | R/U |
| W SW3 /L03 | U1.4 /CHIP_PU | Active-low reset /EN | E3 | R1/C7 +Q1/Q2 auto-reset; không nối STM NRST | RESERVED RESET | W p1 /E | R/U |
| W SW2 /R14 | U1.5 /GPIO0 | BOOT | E3 | Q1/Q2 DTR/RTS; không làm UART bridge | RESERVED BOOT | W p1 /E | R/U |
| W SW1 /R15 | U1.51 /GPIO45 | USER /strap | E3 | R8 pull-up; voltage/eFuse policy chưa đo/đọc | RESERVED STRAP | W p1 /E | R/U |
| W U7 DTR/RTS | Q1/Q2→CHIP_PU/GPIO0 | Auto-reset của ESP | E3 | Mở USB-UART có thể reset ESP; không nối vào STM | RESERVED RESET | W p1 /E | R/U |

GPIO17/18 thuộc domain VDD3P3_RTC và có U1TXD/U1RXD trong IO_MUX, không phải
Flash/PSRAM pins. Header có net tương ứng trong W; actual continuity và routing
phải xác minh riêng. GPIO47/48 là pad SPICLK_P/N (37/36), domain có thể phụ thuộc
mode; không cần dùng chúng cho proposal UART này. GPIO45 có USER load và strap
VDD_SPI; không đổi button/strap/eFuse dựa trên suy đoán.

## 7. AMOLED/touch — chưa có connector pin inventory

Kích thước2.04 inch và độ phân giải368×448 không xác định bus, pin numbering hoặc
nguồn module. CO5300 là thông tin tham chiếu từ phase trước, không chứng minh
actual panel/adapter. Không tự gán chân từ TFT J4 DevEBox hoặc module gần giống.

| Interface cần tài liệu | Module pin | MCU assignment | Voltage | Conflict review | Source | Status |
|---|---|---|---|---|---|---|
| Power/return rails | TBD | TBD, không dùng UART pins làm nguồn | TBD | Regulator/power sequencing/current | Exact module/FPC/BOM chưa có | UNVERIFIED |
| Display data/clock/control | TBD; chưa chốt SPI/QSPI/other | ESP GPIO TBD | TBD | Flash/PSRAM/USB/boot/debug, UART17/18 phải giữ proposal | Exact schematic/pinout chưa có | UNVERIFIED |
| Panel reset /TE/IRQ nếu có | TBD | ESP GPIO TBD | TBD | Chưa biết signal presence/polarity | Exact module chưa có | UNVERIFIED |
| Touch SDA/SCL/INT/RST nếu có | TBD | ESP GPIO TBD | TBD | Touch IC/address/pull-ups/domain chưa biết | Exact touch schematic chưa có | UNVERIFIED |

**Không kết luận “không xung đột AMOLED” khi chưa có panel schematic.** Không có
UART/display/power physical wiring nào được phê duyệt từ bảng này. Xem điều kiện
đối chiếu PCB và BSP trong [DUAL_MCU_SCHEMATIC_REVIEW.md](DUAL_MCU_SCHEMATIC_REVIEW.md).
