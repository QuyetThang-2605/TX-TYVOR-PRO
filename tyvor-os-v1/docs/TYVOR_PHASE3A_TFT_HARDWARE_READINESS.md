# TYVOR OS — Phase 3A TFT Hardware Readiness Audit

Ngày: **12/11/2026, Asia/Bangkok**. Workspace: `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Khuyến nghị: NO-GO cho đấu nối, cấp nguồn TFT, flash hoặc chạy display hardware. GO cho review hồ sơ và chuẩn bị phần mềm tách biệt.** Chưa xác minh được module TFT thực tế, nguồn/backlight, revision WeAct và đường chân trên PCB. ILI9341 và mọi GPIO candidate đều chưa phải hardware VERIFIED. Không có thao tác phần cứng trong Phase 3A.

Người dùng xác nhận UI V3.2.1 đã được duyệt trong yêu cầu Phase 3A. Báo cáo correction trước đó còn ghi chờ Visual Final; audit này ghi nhận sự duyệt mới đó, không sửa báo cáo/artifact lịch sử. Duyệt hình ảnh không tương đương duyệt phần cứng.

## 1. Phạm vi và cách đọc trạng thái

| Trạng thái | Ý nghĩa trong báo cáo |
|---|---|
| VERIFIED | Có bằng chứng kiểm tra được; luôn ghi rõ phạm vi **source/config**, **tài liệu reference** hay **board thực tế** |
| PROPOSED | Phương án phần mềm/kết nối logic cần review; chưa được phép dùng để đấu dây |
| UNKNOWN | Thiếu bằng chứng cho thiết bị thực tế; không tự suy từ board gần giống |
| BLOCKED | Thao tác không được tiếp tục do thiếu bằng chứng/approval hoặc bị gate hiện tại chặn |

Không có thông số điện hoặc kết nối của bộ board + TFT thực tế nào được nâng lên VERIFIED trong phiên này. Các trạng thái UNVERIFIED/INFERRED/PROBABLE trong tài liệu cũ được diễn giải lại theo bốn mức trên, không biến thành xác nhận phần cứng.

Đã đối chiếu các nhóm tài liệu hiện có: [Phase 2C](PHASE2C_HARDWARE_READINESS_REPORT.md), [evidence Phase 2C](BOARD_EVIDENCE_PHASE2C.md), [Phase 2D](PHASE2D_HARDWARE_EVIDENCE_REPORT.md), [board checklist](BOARD_BRINGUP_CHECKLIST.md), [board pin matrix](BOARD_PIN_MATRIX.md), [bring-up plan](HARDWARE_BRINGUP_TEST_PLAN.md), [communication spec](MCU_COMMUNICATION_SPEC.md), [dual-MCU schematic review](DUAL_MCU_SCHEMATIC_REVIEW.md), [dual-MCU matrix](DUAL_MCU_PIN_MATRIX.md), [TFT experiment](TFT_3_2_DISPLAY_EXPERIMENT_REPORT.md), [V3.2.1 final review](TYVOR_UI_V3_2_1_FINAL_REVIEW.md) và [final correction](TYVOR_UI_V3_2_1_FINAL_CORRECTION.md). Không tìm thấy AGENTS.md áp dụng trong workspace/các cấp cha đã kiểm tra.

## 2. Hardware identification và nguồn bằng chứng

| Hạng mục | Bằng chứng hiện có | Trạng thái và giới hạn |
|---|---|---|
| Main controller | STM32F407VET6 Mini Type-C theo kiến trúc; source STM32 riêng | VERIFIED kiến trúc phần mềm; actual PCB/revision UNKNOWN; không dùng cấu hình Discovery hoặc DevEBox để chốt chân Mini |
| Display controller | WeAct ESP32-S3-B N16R8 theo người dùng; schematic B chính thức đã lưu | VERIFIED tài liệu B; actual board/revision/component match UNKNOWN |
| Flash / PSRAM | Build target 16 MB Flash, Octal PSRAM 40 MHz; seller/model N16R8 | VERIFIED config; dung lượng vật lý 16 MB / 8 MiB UNKNOWN, chưa detect/memtest |
| Màn TFT | Người dùng mô tả SPI 3.2 inch, native portrait 240×320 | Kích thước/interface là thông tin đầu vào; exact maker/SKU/revision/FPC/pinout UNKNOWN |
| IC LCD | LCDWIKI MSP3218 dùng ILI9341; source có driver candidate | VERIFIED cho reference MSP3218; **actual IC UNKNOWN**, ILI9341 **PROPOSED** để điều tra, chưa xác nhận |
| Touch | Reference MSP3218 có U2 XPT2046, MSP3217 là biến thể không touch | VERIFIED reference; actual touch layer/IC/population UNKNOWN; runtime BLOCKED |
| Ảnh PCB thực tế | Không tìm thấy ảnh nhận diện mặt trước/sau của module TFT hoặc board WeAct đã nhận trong tài liệu/ảnh workspace đã rà | UNKNOWN; các PNG schematic, UI reference và render không thay thế ảnh thiết bị thực tế |

Nguồn chính thức đã tra cứu lại:

- [WeActStudio repository](https://github.com/WeActStudio/WeActStudio.ESP32S3-AorB): phân biệt A dùng module, B dùng chip ESP32-S3R8. [Schematic B tại commit cố định](https://github.com/WeActStudio/WeActStudio.ESP32S3-AorB/blob/c53183ea4aa96c6b7abf2b1dc153bafa22d2b96e/ESP32S3-B/Hardware/ESP32_S3_B_Sch%20.pdf). Bản PDF cache được đọc text, xem render và kiểm tra hash; endpoint raw không parse được bởi web tool trong phiên này, không ghi nhận như một lần tải PDF mới thành công. Schematic chưa chứng minh revision của board đang có.
- [LCDWIKI MSP3218](https://www.lcdwiki.com/3.2inch_SPI_Module_ILI9341_SKU:MSP3218), [schematic một trang](https://www.lcdwiki.com/res/MSP3218/MSP3218-3.2-SPI.pdf) và [manual](https://www.lcdwiki.com/res/MSP3218/3.2inch_SPI_Module_MSP3218_User_Manual_EN.pdf): chỉ là reference sản phẩm; đã xem hình schematic, không dùng nó để xác nhận module người dùng.
- [Espressif ESP32-S3 datasheet v2.2](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf), mục 2.5, bảng 5-2/5-4; [GPIO IDF 5.5.1](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/gpio.html); [SPI LCD IDF 5.5.1](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/lcd/spi_lcd.html). Đây là silicon/SDK references, không xác nhận board routing.

PDF đã kiểm tra tại workspace:

| File | Bytes | SHA-256 |
|---|---:|---|
| `build/phase2d/sources/weact-b.pdf` | 815817 | `910a41eea1b8d111e8efc78667eb01d95a30b6e7ae2b9ce7567afcafd3913e68` |
| `build/tft/evidence/reference/msp3218-schematic.pdf` | 525576 | `3df65023deed7955a4c9d55ed25b6bb78e8b91e79b22e22cc48661b36c08643e` |
| `build/tft/evidence/reference/msp3218-user-manual.pdf` | 1006632 | `9798f5a91ce10842ac922dbbaaf81ad8e76d5945294c448c4329f8708d79ebad` |

## 3. Kiến trúc và hardware gates

**VERIFIED source/config:** STM32 giữ Inputs/Mixer/Model/Safety/Logical Outputs; ESP32 giữ UART endpoint/cache/read-only UI. TFT backend nằm trong project ESP32; không có đường SPI TFT sang STM32. Native simulator có thể link Core để kiểm thử, không có nghĩa ESP firmware được cấp quyền Core.

V3.2.1 dùng `esp32/experiments/tyvor_ui_v3_2_1/`; entry compile-include bridge hiện có, UI task priority 2 lấy value copy từ publication queue, bridge task riêng priority 5. Không đổi UART protocol v2, cache ownership, Safety Engine, ARM hoặc mixer. Không có RF/CRSF/output điều khiển UAV được thêm.

| Kiểm tra tại source và generated `build/ui-v3-2-1-final/esp32/sdkconfig` | Kết quả |
|---|---|
| `CONFIG_TYVOR_TFT_HARDWARE_GATE`, `BOARD_VERIFIED`, `MODULE_VERIFIED`, `TOUCH_RUNTIME` | OFF |
| TFT `SCLK/MOSI/MISO/CS/DC/RESET/BACKLIGHT` | Tất cả **-1** |
| `CONFIG_TYVOR_UART_PINOUT_VERIFIED`, UART TX/RX | OFF, **-1/-1** |
| `CONFIG_TYVOR_TFT_SIMULATOR`, rotation | ON, **1 = logical 320×240 landscape** |
| Display profile | `ILI9341_CANDIDATE`, `hardware_verified=false`, gate OFF; AMOLED profile riêng giữ nguyên |
| Entry/CMake guards | Bật display/UART/touch gate trong project V3.2.1 hiện tại sẽ bị từ chối compile/configure |
| UI authority / safety labels | SIM/CORE GATE SAFE tách aircraft **ARM UNKNOWN**; dữ liệu SIM không thành telemetry thật |

Do đó artifact V3.2.1 hiện tại là **MOCK/software baseline**, không phải firmware đã sẵn sàng kích hoạt TFT bằng cách sửa một Kconfig. Không gỡ guard trong Phase 3A. `LQ 98%` khi chạy SIM đến từ fixture trong `experiments/tft_spi_320x240/tft_presenter.c`, không phải bằng chứng thu telemetry thiết bị.

## 4. GPIO compatibility matrix

Matrix dưới đối chiếu [restrictions chính thức của Espressif](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/gpio.html), schematic B và whitelist `experiments/tft_spi_320x240/display_contract.c`. **VERIFIED là giới hạn tài liệu/phần mềm; không GPIO nào được xác nhận free trên actual board.** Chưa chỉ định vị trí header hoặc chân MCU vật lý cho dây TFT.

| GPIO / net | Xung đột / vai trò reference | Trạng thái cho TFT |
|---|---|---|
| 1, 2 | Không nằm trong nhóm reserved hiện tại | PROPOSED pool; actual routing/load UNKNOWN |
| 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 | Whitelist phần mềm cho phép | PROPOSED pool; actual routing/load UNKNOWN |
| 15, 16 | Whitelist cho phép; có alternate XTAL32K | PROPOSED pool; kiểm tra population/chức năng khác |
| 21, 38 | Whitelist cho phép | PROPOSED pool; actual routing/load UNKNOWN |
| 17, 18 | Dành cho inter-MCU UART proposal; runtime vẫn -1 | BLOCKED cho TFT theo allocation policy; UART chưa được duyệt |
| 19, 20 | Native USB D-/D+ / USB Serial-JTAG | BLOCKED để giữ console/debug |
| 0, 3, 45, 46 | Strapping; GPIO0 BOOT/autoreset, GPIO45 USER trên reference | BLOCKED; không thay boot/strap/eFuse |
| 26–32 | SPI0/1 Flash/PSRAM | BLOCKED memory |
| 33–37 | Octal PSRAM trên ESP32-S3R8; header 35–37 không chứng minh free | BLOCKED memory |
| 39–42 | External JTAG | BLOCKED theo policy giữ debug |
| 43, 44 | UART0 nối USB-UART U7 trên schematic B | BLOCKED; tránh download/log contention |
| 47 | Pad SPICLK_P, power-domain/mode cần đối chiếu | BLOCKED theo whitelist bảo thủ; không khẳng định silicon luôn cấm dùng GPIO47 |
| 48 | WS2812B data qua R12 trên schematic B; pad/domain cũng cần kiểm tra | BLOCKED onboard LED/policy |
| 22–25 | Không có trong tập GPIO ESP32-S3 | BLOCKED, không được gán |
| EN/CHIP_PU | Reset MCU, RC/autoreset | BLOCKED; không dùng làm TFT RESET |

Whitelist có **17 GPIO candidate** (1,2,4–16,21,38), không phải 17 chân đã duyệt. Bộ pin policy reject duplicate, reserved và thiếu evidence trước peripheral init. Các pin số cụ thể trong unit-test fixtures là dữ liệu synthetic, không là đề xuất đấu dây.

## 5. Power compatibility

| Tham số | Bằng chứng / giới hạn | Trạng thái thực tế |
|---|---|---|
| ESP logic | Datasheet S3 bảng 5-2: rails chính 3.0–3.6 V, nominal 3.3 V; GPIO thresholds phụ thuộc power domain | VERIFIED silicon; actual rail/levels UNKNOWN; **không đưa 5 V vào GPIO** |
| TFT module VCC | LCDWIKI nêu 3.3–5 V cho MSP3218; schematic có XC6206 và bypass J1 | VERIFIED reference, actual input range/jumper UNKNOWN; chưa chọn mức cấp |
| TFT logic I/O | MSP3218 reference 3.3 V TTL | Actual VIH/VIL/VOH/VOL và MISO/pull-up voltage UNKNOWN |
| TFT backlight | Reference Q1 S8050, R6 1k, R5 10Ω; header TFT_BL điều khiển transistor | Actual LED pin là control hay tải nguồn, polarity/current UNKNOWN |
| Current / rail budget | Reference ghi 0.72 W, không phải peak/rating cho module đang có | UNKNOWN; không dùng con số này làm current limit hoặc rating của WeAct |
| WeAct regulator | U4 tạo VCC_3V3 từ +5 V trên schematic; chưa có part/rating/thermal budget actual | UNKNOWN khả năng cấp thêm TFT/backlight |
| USB / nguồn song song | Reference có USB1 qua U7, USB2 native; D1/D5 cùng các bypass U8/U9 | UNKNOWN population/isolation actual; chưa duyệt dùng hai USB hoặc nguồn ngoài đồng thời |
| GND / reset / thứ tự cấp | Cần cùng reference cho tín hiệu và tránh backfeed qua I/O | PROPOSED nguyên tắc; actual pin/source/sequence UNKNOWN |

Nguồn: [datasheet S3](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf), [MSP3218 specifications](https://www.lcdwiki.com/3.2inch_SPI_Module_ILI9341_SKU:MSP3218), schematic B và MSP3218 đã dẫn. Không lấy rail memory VDD_SPI làm nguồn TFT. Không cấp tải backlight trực tiếp bằng GPIO; driver hiện hỗ trợ logic backlight GPIO, chưa chứng minh tương thích với LED pin của module thực tế. Không nối rail STM32 với ESP/TFT trong proposal này.

**BLOCKED power decision:** chưa đủ cơ sở chọn 3V3 hay 5V, đầu nối nguồn, mức current limit hoặc đường ground cụ thể. Báo cáo không có hướng dẫn cấp nguồn dựa trên pinout reference.

## 6. Wiring proposal — NOT APPROVED

Đây chỉ là signal contract cho allocation sau khi đủ bằng chứng. **Không phải sơ đồ đấu dây.** Không sao chép số chân header MSP3218 sang TFT thực tế. MCU phía display luôn là ESP32; STM32 không tham gia bus TFT.

| Tín hiệu logic TFT | Hướng dự kiến | ESP GPIO hiện tại | Điều kiện phải xác minh |
|---|---|---:|---|
| SCK | ESP → TFT | -1 | Exact connector/net, SPI mode/clock và loading |
| SDI / MOSI | ESP → TFT | -1 | Tên/đường data input, logic domain |
| SDO / MISO | TFT → ESP, chỉ khi cần read | -1 | Có thật, mức output, tri-state; write-only chỉ bỏ sau khi xác minh |
| CS | ESP → TFT | -1 | Active level, idle pulls, không dùng chung CS |
| DC / RS | ESP → TFT | -1 | Command/data polarity theo controller/driver đã xác nhận |
| RESET | ESP → panel reset | -1 | Reset polarity/timing/pulls, tách EN/reset MCU |
| LED / BL | Chưa xác định control hay power | -1 | Schematic driver/transistor/current/polarity; không coi là GPIO-compatible trước bằng chứng |
| VCC | Chưa chọn nguồn | Không áp dụng | Exact rail, regulator/bypass, current/thermal/backfeed plan |
| GND | Reference chung theo drawing được duyệt sau | Không áp dụng | Ground nets/connector và return path actual |
| T_CLK/T_CS/T_DIN/T_DO/T_IRQ nếu có | Không kết nối trong phase này | Chưa gán | Có IC touch thật, type/voltage/bus/pinout trước runtime |
| SD card / chân còn lại | Ngoài scope | Chưa gán | Không cấp chức năng ngoài màn hiển thị |

Touch giữ OFF, không tái dùng I²C touch của AMOLED. GPIO17/18 vẫn chỉ là UART **PROPOSED**, không trở thành TFT pin hay live UART. Tất cả GPIO chưa xác minh giữ **-1**.

## 7. Driver feasibility và thiết kế cấu hình độc lập

**VERIFIED software feasibility:** IDF 5.5.1 `esp_lcd` có SPI LCD IO, DMA và callback kết thúc color transfer. Repo đã build driver Espressif ILI9341 component 2.1.0 cùng LVGL 9.2.2. Đây là hỗ trợ SDK/compile, không xác minh IC hoặc init sequence của TFT đang có. [SPI LCD API](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/lcd/spi_lcd.html), [pinned driver](https://github.com/espressif/esp-bsp/tree/67f41a169401d1f23e827188dd08fb351cb0febe/components/lcd/esp_lcd_ili9341).

Source integration đang có:

- `esp32/experiments/tft_spi_320x240/main/board_pins.*` và Kconfig tách GPIO khỏi driver; init kiểm tra gate + board/module flags + pin policy trước SPI/GPIO/LCD calls.
- `tft_spi_backend.c`: SPI2_HOST, mode 0, DMA AUTO, queue depth 2, candidate 10 MHz; RGB/BGR option, 16 bpp, reset/init/swap/mirror. Các giá trị này VERIFIED source, hardware selection vẫn PROPOSED. Range validator 1–20 MHz không chứng minh TFT chịu được 20 MHz.
- `experiments/tyvor_ui_v3_2_1/ui.c`/`render_quality.c`: LVGL RGB888 partial rendering, packing in-place thành RGB565 wire MSB-first đúng một lần; dithering theo tọa độ toàn cục. Byte order tách biệt RGB/BGR. Không cần thay renderer, logo, font, background hoặc palette để dùng SPI backend.
- Một buffer **20,480 B**, internal DMA/8-bit aligned 64; không double buffer hoặc cấp framebuffer toàn màn trên ESP. Với RGB888 nguồn, capacity tối đa tương ứng 21 hàng ×320 px mỗi vùng đầy chiều ngang. Output RGB565 toàn màn 320×240 là 153,600 B, chia vùng exclusive right/bottom.
- ISR chỉ give semaphore; UI task chờ completion trước `lv_display_flush_ready`. Timeout/start error latch fault và giữ buffer ownership; không tái dùng bộ nhớ có thể còn DMA. Completion wait 250 ms/chunk không phải upper bound mọi lời gọi SDK/init/submit. Hardware timing và fault recovery chưa được đo.

**Thiết kế PROPOSED cho project bench tương lai, chưa tạo hoặc bật trong Phase 3A:**

1. Tạo entry/project riêng, ví dụ `esp32/bench/tft_v321/`, giữ nguyên project/mock baseline và các compile guards V3.2.1 hiện tại. Reuse nguyên UI321 components + port interface; không fork renderer hoặc bridge/protocol.
2. Một board profile riêng lưu evidence revision/hash, module/controller, resolution, rotation, RGB/BGR/inversion/gaps/reset/backlight và GPIO. Default disabled, GPIO=-1, verification flags false; không cung cấp pin default đoán. Manifest/build identity phải phân biệt BENCH hardware với MOCK baseline.
3. Chỉ sau review evidence/power/drawing và cho phép ở phase sau mới cân nhắc display gate riêng. UART/touch/RF vẫn OFF, không sửa STM32/Safety/Mixer. Nguồn UI ban đầu là fixture có nhãn SIM, aircraft ARM UNKNOWN.
4. Orchestrator bench chạy diagnostics trước, rồi cố định HOME/state để so ảnh/timing; entry baseline đang đổi fixture mỗi 8 s và page mỗi 10 s. Không sửa các component UI đã duyệt.
5. Cờ `CONFIG_TYVOR_TFT_BOOT_DIAGNOSTICS=y` hiện có nhưng **entry V3.2.1 không gọi diagnostic sequence**. Generic TFT experiment có sequence color/geometry/rotation, dùng UI prototype cũ. Bench entry mới cần dùng diagnostic stage tương ứng và sau đó UI321, không lấy flag ON hoặc app boot làm bằng chứng màn đã test.
6. Touch hiện chỉ có callback contract `touch_adapter.*`, không driver XPT2046/runtime indev. Không bổ sung touch trong bước display-only khi IC chưa rõ.

Tại 10 MHz, riêng payload full-screen RGB565 cần tối thiểu **122.88 ms** theo bitrate, chưa tính command/render/RTOS/transfer overhead. Đây là phép tính, không FPS/WCET phần cứng. `vTaskDelay(33 ms)` không bảo đảm 30 FPS; đo partial-update latency riêng và chọn tiêu chí bench thực tế trước run. Không đổi clock để đạt một FPS chưa được đo.

## 8. Test plan — chưa thực thi trên hardware

Mọi bước có cấp nguồn, flash, nối dây hoặc đo live bên dưới đều **BLOCKED hiện tại** và chỉ là kế hoạch cho phiên được cho phép sau. Dừng ngay nếu bước tiên quyết FAIL; không tiếp tục bằng cách bỏ validation. Không gán hardware PASS từ native tests.

| ID / mục tiêu | Tiên quyết và nội dung khi được phép | PASS / FAIL và điều kiện dừng | Bằng chứng cần lưu |
|---|---|---|---|
| T0 Identification, power plan | Đủ ảnh hai mặt/revision/IC/FPC, schematic đúng thiết bị, connector orientation; review net/power budget | PASS khi toàn bộ actual nets/limits được reviewer xác nhận; thiếu/mismatch → STOP trước power/wiring | Ảnh gốc, source URLs/PDF/hash, signed pin/power matrix |
| T1 Electrical bring-up riêng ESP và TFT | T0 PASS + authorization riêng; xác minh ground, short/backfeed/power path trước; đo rail/current theo giới hạn đã duyệt | PASS trong giới hạn datasheet actual; sai rail/polarity/current/nóng/brownout → STOP, không thử nguồn khác theo suy đoán | Meter/scope/current records, setup photo, supply policy |
| T2 Identity / Flash / PSRAM | T1 PASS; kế hoạch backup/recovery + image approval trước bất kỳ flash nào; log chip/flash ID, detected Flash, PSRAM size và memtest | PASS đúng target16 MB và PSRAM8 MiB; thiếu/sai memory → STOP; `IGNORE_NOTFOUND=y` hoặc boot OK không đủ | Device/boot logs, detected bytes, heap caps/memtest, firmware SHA |
| T3 Display initialization | T0–T2 PASS; entry bench riêng, verified pins/init/reset, backlight off trước khi init nếu đã xác nhận polarity; UART/touch/RF OFF | PASS panel init ổn định, no resets/errors; blank/unstable/bus fault → STOP, kiểm lại datasheet/logic capture | Init log, reset/DC/CS/SPI traces, photo, selected profile |
| T4 Color bars / RGB565 | T3 PASS; red F800, green07E0, blue001F, black0000/whiteFFFF, bars/gradient | PASS primaries/byte order/RGB-BGR đúng; sai màu/shift → FAIL, không che bằng asset khác | Native565 reference, panel photo, MOSI decode, config |
| T5 Orientation / bounds | Pattern corner TL/TR/BL/BR + 1px border; rotation1 logical320×240 | PASS đủ bốn góc, không crop/mirror/double rotation/off-by-one; sai mapping → FAIL | Panel photos, geometry/profile/log |
| T6 Logo và HOME V3.2.1 | Reuse baseline assets/renderer, fixed labeled SIM fixture; compare cùng geometry/native resolution | PASS logo alpha/tỉ lệ, timer, waves, ticks/text không clip; SIM rõ, ARM UNKNOWN; không gọi SIM98% là telemetry | Baseline PNG/hash, panel photo, fixture ID, user review |
| T7 Freshness states | Lần lượt valid SIM, stale, disconnected, invalid model theo fixture; không cần UART thật | PASS stale telemetry không được dùng như live, không khẳng định disarmed khi ARM unknown; false-safe → STOP | State timestamps, screenshots/photos, log assertions |
| T8 Partial redraw / DMA | Cập nhật riêng LQ/timer; idle then dirty region; kiểm giới hạn/stride và completion | PASS vùng/byte count đúng, idle không redraw vô cớ, buffer không reuse trước completion, không tearing/corruption; timeout → giữ fault, không tiếp tục DMA mù | Region counters, bytes/latencies, trace/frame captures |
| T9 Frame timing | Đo render, pack, submit, completion, full-refresh và partial-update; latency min/p95/max dưới workload định nghĩa trước | PASS khi đáp ứng budget đã được review trước run; budget hiện chưa chốt → chưa có timing PASS; WDT/timeout → STOP | `esp_timer` logs + logic analyzer SCK/CS, sample count, clock/load/version |
| T10 Runtime memory / soak | Theo dõi free/min/largest internal + PSRAM, UI/bridge stack high-water, canary, heap integrity, reset reason; đề xuất soak30 phút | PASS không leak/corruption/reset và headroom theo budget được duyệt; allocation fail/stack warning → STOP | Time-series counters, actual PSRAM allocations, stack/heap diagnostics |
| T11 Fault/recovery | Native fault injection trước; hardware reset/reconnect chỉ theo plan được duyệt, không hot-unplug tùy tiện | PASS error được latch, không reuse buffer, reboot về state an toàn/UNKNOWN, không reset/control STM; ảnh cũ trên panel không chứng minh fresh data | Fault/boot logs, timeout counters, panel trace/video |

Timing probes dùng timestamp/SPI traces trước; không tự allocate thêm GPIO đo timing chưa duyệt. Physical display có thể giữ hình cũ khi bus/ESP lỗi; chưa có cơ chế độc lập chứng minh panel sẽ tự xóa trạng thái stale. Đây là rủi ro cần review, không thay safety logic để che.

## 9. Software evidence / memory hiện có

Phase 3A chỉ audit tài liệu/cấu hình, **không rebuild/re-render hoặc chạy lại test suites** vì không sửa firmware. Đã chạy lại read-only receipt/dependency verification và offline package validator trên artifact hiện tại:

| Kiểm tra | Kết quả Phase 3A |
|---|---|
| `ui321_final_artifacts.py verify` | PASS: 839 pinned dependency files, 3 build receipts khớp inputs/outputs/compiler hiện tại |
| Debug JUnit/inventory/compile metadata | Receipt VERIFIED **445/445**, 0 fail/skip; kết quả build/test đã có, không phải suite chạy mới |
| Release/NDEBUG JUnit/inventory/compile metadata | Receipt VERIFIED **445/445**, 0 fail/skip; NDEBUG đúng; không phải suite chạy mới |
| ESP32-S3 build | Receipt VERIFIED ELF/BIN/MAP/bootloader/partition/config; app version `0.5.3-ui-v3-2-1`; không flash |
| `ui321_final_artifacts.py validate` | **OFFLINE_PACKAGE_PASS**, 296 files, app1,069,520 B; hardware_tested/flashed/serial_opened=false |
| Source preservation so final package | **443/443** source hashes khớp trước khi thêm tài liệu Phase 3A |
| Electrical / display / PSRAM / timing hardware | **NOT RUN / UNKNOWN**, hardware actions BLOCKED |

Recheck offline bằng Python environment có sẵn:

```powershell
& ./.tools/espressif/python_env/idf5.5_py3.11_env/Scripts/python.exe scripts/ui321_final_artifacts.py verify
& ./.tools/espressif/python_env/idf5.5_py3.11_env/Scripts/python.exe scripts/ui321_final_artifacts.py validate
```

Artifacts baseline: `artifacts/tyvor-ui-v3-2-1-final/`. Manifest SHA-256:
`6a1039b3300e22dec42c6fabde8b22262ab01e31f2d66c5a4b264566164b341d`.
App BIN SHA-256:
`73cd93d7822c008aec021588b8a85a16c938c9b2a9af59964588adfe53ba7d9b`.

| Memory / partition baseline | Giá trị đã có trong build evidence | Giới hạn kết luận |
|---|---:|---|
| App BIN / factory | 1,069,520 /3,145,728 B | Còn2,076,208 B trong factory, không phải dung lượng Flash đã detect |
| Bootloader / partition table / app offsets | 0x0 /0x8000 /0x10000 | VERIFIED offline package; không ghi thiết bị |
| Linked DIRAM | 175,491 /341,760 B (51.35%) | Slack166,269 B trước runtime allocations, không là free heap đo thực |
| Dedicated IRAM sections | 16,384 B | Không đồng nghĩa toàn bộ SRAM đã hết |
| LVGL arena / DMA draw buffer | 98,304 B trong BSS /20,480 B heap internal | Không cộng arena hai lần; buffer runtime phải allocate được |
| UI / bridge / diagnostics stack | 8192 /4096 /4096 B | Known allocation minimum36,864 B gồm buffer+stacks; chưa gồm TCB/queues/SDK |
| Background+logo objects | 490,078 B Flash | Không có full-screen heap bitmap thêm trong backend này; actual allocation/PSRAM vẫn cần đo |

`CONFIG_SPIRAM_IGNORE_NOTFOUND=y`, mode Octal40 MHz và TYPE_AUTO được giữ nguyên. Boot thành công không chứng minh PSRAM có thật hoặc đủ8 MiB. Không dùng benchmark memory-sink native như FPS/SPI latency phần cứng.

## 10. Missing evidence và điều kiện gỡ NO-GO

1. **TFT identity:** ảnh rõ mặt trước/sau toàn PCB, connector labels và pin1/orientation, revision/logo/SKU, FPC marking; ảnh cận IC LCD nếu lộ, touch IC/population, regulator, transistor/resistor backlight và jumper. Nếu IC nằm dưới glass/FPC, cần exact manufacturer document/BOM/part association; không yêu cầu tháo phá màn để đoán IC.
2. **TFT electrical:** exact schematic/manual khớp revision; VCC range với jumper thực, I/O domain, reset/DC timing, SPI mode/max clock, LED control hay power/polarity/current/inrush. Header giống MSP3218 hoặc chỉ màn3.2 inch không đủ xác minh ILI9341/XPT2046.
3. **WeAct identity:** ảnh hai mặt/markings/revision ESP32-S3-B đã nhận, chip/Flash/regulator marking, USB/power bypass population; đối chiếu exact B schematic. Seller/model name không thay actual evidence.
4. **Net verification:** review mapping từ GPIO tới header actual, onboard loads, rail/ground/reset; continuity khi không cấp nguồn chỉ trong phiên hardware được cho phép. Chưa có actual mapping để chọn SCK/MOSI/CS/DC/RESET/LED.
5. **Power approval:** nguồn duy nhất/USB role, current/thermal budget và limits theo thiết bị đúng, ground/return và chống backfeed. Không lấy U4 output hoặc LED pin làm nguồn tùy ý.
6. **Software bench approval:** tách entry/config theo mục7, version/SHA riêng, khôi phục firmware/backup plan và phương án recovery; giữ UI/Core/protocol baseline. Board/module flags chỉ đổi sau khi evidence matrix được review, không đổi cho build PASS.
7. **Test approval riêng:** cho phép rõ việc cấp nguồn/đấu nối/flash ở phase sau với drawing đã duyệt; trước đó không thực thi T1–T11. Touch cần hồ sơ/driver riêng, vẫn OFF trong display-only bench.

## 11. Git WIP và kết luận

Git status hiện báo `?? ./` từ repository cha, tracked/staged diff rỗng; vì workspace đang untracked, diff rỗng không chứng minh WIP không đổi. Snapshot SHA trước audit ở `build/phase3a/source-before.json` gồm443 source catalog entries và2 ảnh approved/background. Kiểm tra sau audit: **445/445 entries giữ nguyên hash**, source catalog tăng443→444 chỉ do thêm tài liệu này. Kết quả receipt/package/preservation được lưu tại `build/phase3a/audit-verification.json`; evidence audit nằm trong thư mục build được ignore. Không reset/clean/restore/stage hoặc hoàn tác các thay đổi cũ.

Không sửa source/config/asset UI V3.2.1, STM32 Core, protocol v2, Safety/Mixer/ARM/RF, AMOLED hoặc GPIO runtime. Không truy cập serial/SWD/board, đấu dây, cấp nguồn, flash/erase, sửa eFuse/option bytes, commit/push/deploy hoặc Git global. Display/Touch/UART/RF gates giữ OFF; GPIO chưa xác minh=-1.

**GO:** review hồ sơ Phase 3A và kiến trúc bench/display backend độc lập. **NO-GO hardware:** thiếu exact TFT/controller/connector/power/backlight evidence và actual WeAct revision/net verification; project V3.2.1 hiện có cố ý chưa cho bật display gate. Không có hardware PASS, không có pinout đấu dây được duyệt. Dừng để người dùng review trước phase tiếp theo.
