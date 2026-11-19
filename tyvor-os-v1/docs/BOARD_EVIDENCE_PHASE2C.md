# Phase 2C board evidence - chưa phải sơ đồ đấu nối

Ngày 10/11/2026. Board do người dùng chỉ định: STM32F407VET6 Mini Type-C / ThegioiIC; WeAct ESP32-S3-B N16R8 / Điện tử Tuyết Nga. **Chưa có ảnh/marking/revision của hai PCB thực tế. Không chốt UART/power/header hoặc cho phép cấp nguồn/flash.**

VERIFIED = đã đối chiếu đúng nội dung tài liệu được nêu; không tự mở rộng sang PCB thực tế. INFERRED = mối liên hệ có căn cứ nhưng chưa đối chiếu revision. UNVERIFIED = chưa đủ bằng chứng. Cột actual board bên dưới tách riêng khỏi document evidence.

## Nguồn và identity

- [ThegioiIC - đúng sản phẩm Type-C](https://www.thegioiic.com/mach-stm32f407vet6-type-c): product ID44303, image64729 trong HTML. Đọc được HTML gốc dù web text extractor không có readable lines. Mô tả chủ yếu là silicon features; không tìm được exact-revision schematic/netlist trong trang đã tải. Không dùng sản phẩm CAN485/Core407V/kit Mini khác thay thế. Product association **INFERRED**; maker/revision actual **UNVERIFIED**.
- [Điện tử Tuyết Nga - ESP32-S3-B N16R8](https://dientutuyetnga.com/products/weact-thu-phat-wifi-ble-esp32-s3-b-n16r8) liên kết trực tiếp repository WeAct. Seller xác định variant N16R8 nhưng không xác nhận revision PCB đã giao. Association **INFERRED**.
- [WeAct chính thức - B schematic](https://github.com/WeActStudio/WeActStudio.ESP32S3-AorB/blob/c53183ea4aa96c6b7abf2b1dc153bafa22d2b96e/ESP32S3-B/Hardware/ESP32_S3_B_Sch%20.pdf), một trang, có netlist schematic và ảnh layout B; không có revision rõ trong trang này. Đã đọc text và xem render toàn trang. Snapshot tree commit `c53183ea4aa96c6b7abf2b1dc153bafa22d2b96e`, PDF blob `d27f6ad987b174aa9515e7207ec9a8fa4be45e95`, PDF SHA256 `910a41eea1b8d111e8efc78667eb01d95a30b6e7ae2b9ce7567afcafd3913e68`. Reference document **VERIFIED**; matching actual PCB **UNVERIFIED**.
- [ST DS8626 Rev12](https://www.st.com/resource/en/datasheet/stm32f407ve.pdf) (March2026), pp51/64 pin/AF và p81 operating conditions; [Espressif S3 datasheet v2.2](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf) và [IDF5.5.1 GPIO restrictions](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/gpio.html): **VERIFIED silicon/reference**, không phải bằng chứng net của board người dùng.

Raw HTML/PDF, retrieval URL/size/SHA256, tree JSON, extracted text và schematic PNG: `build/phase2c/evidence/board-sources/`, được copy vào package. Ngày là ngày audit; không tự gán đó là ngày phát hành schematic.

## Bảng nguồn / chân tham chiếu

Đây là bảng evidence, **không phải bảng nối TX→RX hay hướng dẫn cấp nguồn**. Không có header position hoặc GPIO UART nào được chọn cho firmware hiện tại.

| Mục | Tài liệu đã xác minh | Phân loại tài liệu / liên hệ | Actual board / điều kiện còn thiếu |
|---|---|---|---|
| STM power/input limits | Seller ghi1.8..3.6 V giống VDD silicon; ST xác nhận dải VDD, không xác nhận USB/regulator board | VERIFIED silicon; board rating UNVERIFIED | Regulator marking, nguồn vào/ra, polarity, regulator limits/protection/backfeed, actual3V3/GND nets UNVERIFIED. Không dùng dải VDD làm mức cấp vào USB/header |
| STM UART candidate | DS8626: PA2 USART2_TX/AF7, PA3 USART2_RX/AF7; LQFP100 pins25/26 | VERIFIED MCU AF; header route UNVERIFIED | Không xác nhận PA2/PA3 ra header, free/conflicts/levels; candidate gate OFF |
| STM USB/debug/boot | MCU AF: PA11/12 USB FS, PA13/14 SWD; silicon boot reference | VERIFIED silicon | Connector/straps/buttons/oscillator/header mapping UNVERIFIED; không dùng các chân này làm UART mặc định |
| STM memory | VE512 KiB Flash, SRAM128 KiB +CCM64 KiB; software linker khớp | VERIFIED source/reference | Actual MCU marking/identity/RAM health UNVERIFIED |
| ESP power | B schematic: USB VBUS qua diode/bypass links tới net +5V; U4 LDO ra VCC_3V3; U7 USB-UART cấp VCC_3V3 | VERIFIED reference schematic | U4 không ghi part/rating rõ; exact input limits, bypass population và backfeed actual UNVERIFIED. Net +5V không là approval cấp5V |
| ESP UART logic | MCU/USB-UART dùng domain3V3 trong B reference; U0TXD/U0RXD còn nối U7 | VERIFIED reference; actual3.3V compatibility INFERRED | Không chọn U0 làm inter-MCU UART; cần xác nhận voltage/route/conflicts actual |
| ESP GPIO có thể review | B reference có header nets GPIO17/18 và nhiều GPIO khác, không tự chọn TX/RX | VERIFIED net names in B PDF | Free GPIO/header position/electrical domain trên actual board UNVERIFIED; app UART1 pins=-1 |
| ESP USB | USB1→U7 USB-UART; USB2→GPIO19/20 native USB ở B PDF | VERIFIED reference | Board revision/connector orientation/USB actual UNVERIFIED; giữ19/20 ngoài UART selection |
| ESP boot/debug | GPIO0/3/45/46 là straps theo Espressif; B PDF SW2 GPIO0, SW3 CHIP_PU, SW1 GPIO45; GPIO39..42 có JTAG functions | VERIFIED reference | Button polarity/population/straps/debug actual UNVERIFIED; không tự thay strap/eFuse hoặc dùng chúng cho UART |
| ESP memory reserved | Espressif: GPIO26..32 dùng Flash/PSRAM; octal PSRAM/R8 còn chiếm33..37; B PDF vẫn vẽ35..37 ra header | VERIFIED reference restriction | Không chọn26..37 cho UART; header xuất hiện không chứng minh GPIO free. Actual R8/Flash marking/capacity UNVERIFIED |
| ESP LED / onboard use | B PDF GPIO48→WS2812B, GPIO45→USER button, U0→U7 | VERIFIED reference | Exclude from initial UART candidates; actual population/revision UNVERIFIED |
| GND/common reference | B reference dùng GND chung nội bộ | VERIFIED schematic only | Actual GND pins/continuity, cross-board common reference, isolation/backfeed plan UNVERIFIED; chưa có interconnect drawing |
| Flash16MB/PSRAM8MB | Seller variant, software config/header16MB +octal PSRAM; reference U1=ESP32-S3-R8 | VERIFIED config/document; physical capacity INFERRED | Actual IDs/detected bytes/memtest UNVERIFIED; IGNORE_NOTFOUND vẫn bật, boot đơn thuần không chứng minh PSRAM |
| AMOLED/touch | Không có exact module evidence | UNVERIFIED | Ngoài Phase2C; không suy pinout/nguồn/driver |

## Evidence cần bổ sung bằng board thật

1. Ảnh rõ hai mặt từng PCB, toàn bộ silkscreen/version/logo, MCU/Flash/regulator/USB bridge markings, connectors/buttons/jumpers và ruler/orientation. Seller order/product variant và schematic revision tương ứng. Không dùng ảnh seller thay bằng chứng board đã nhận.
2. Trong công việc hardware được cho phép riêng: record continuity khi board không cấp nguồn cho header→MCU TX/RX candidate, GND, USB nets, straps; record resistance rails/GND, polarity và bypass-link population theo schematic đúng revision. Không đoán rail chỉ từ label.
3. Sau khi power plan được review: voltage/current/idle logic measurements, backfeed/ground compatibility và power-sequence evidence từng board riêng; không có trị số giới hạn cấp nguồn được phê duyệt trong tài liệu này.
4. Runtime evidence còn cần: actual MCU/Flash ID/capacity, PSRAM8388608 bytes +memtest, clock/RNG/tick/IWDG, UART full duplex/overrun/error recovery, DWT timing dưới saturation, IRQ latency/critical section và main/task stack/heap peak.

Điều kiện chốt chân: actual identity/revision +schematic/net/level evidence +reviewer/date +approved drawing. UART gates giữ OFF; chưa có approved TX/RX/power/GND connection.
