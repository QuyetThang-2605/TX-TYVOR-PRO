# TYVOR OS V1 — TFT 3.2 SPI display experiment

Ngày **12/11/2026, Asia/Bangkok**. Workspace `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Software experiment: PASS, sẵn sàng review. Display hardware bring-up: NOT READY.**
Đã triển khai project ESP-IDF riêng, ILI9341/esp_lcd candidate, LVGL prototype và
native mock renderer. Native **234/234 Debug +234/234 Release/NDEBUG PASS**;
ESP32-S3 application/bootloader/partition build PASS. Không flash hoặc chạy trên
màn thật. ILI9341 là **PROBABLE**, chưa VERIFIED; actual GPIO, nguồn/backlight,
colour order, rotation, touch IC, PSRAM và hardware timing chưa xác minh.

Preview dùng **LVGL software rasterizer thật**, không phải ảnh vẽ thay cho render:
[portrait](../artifacts/tft-ui-experiment/preview-portrait.png),
[landscape](../artifacts/tft-ui-experiment/preview-landscape.png),
[gallery84 ảnh](../artifacts/tft-ui-experiment/index.html).

## 1. Display investigation và bằng chứng còn thiếu

Đã đọc [LCDWIKI MSP3218](https://www.lcdwiki.com/3.2inch_SPI_Module_ILI9341_SKU:MSP3218),
[manual Rev1.0](https://www.lcdwiki.com/res/MSP3218/3.2inch_SPI_Module_MSP3218_User_Manual_EN.pdf)
và [schematic MSP3218](https://www.lcdwiki.com/res/MSP3218/MSP3218-3.2-SPI.pdf).
Reference mô tả ILI9341, SPI4-wire, RGB565/65K, portrait240×320 và landscape320×240;
MSP3218 có touch, MSP3217 không touch. Schematic reference có U2 XPT2046 và
circuit backlight Q1, regulator XC6206, bypass J1.

**Không tìm thấy ảnh PCB màn người dùng trong workspace/attachments hiện có.**
Vì vậy chưa thực hiện được đối chiếu mặt trước/sau, silkscreen/revision, IC/FPC,
pin1, connector hoặc population. Không dùng hình/module reference làm bằng chứng
actual. Không khẳng định XPT2046 hiện diện trên màn của người dùng. Không dùng
touch I2C của AMOLED cho TFT này.

Local evidence ở `build/tft/evidence/reference/`: schematic525576 bytes SHA256
`3df65023deed7955a4c9d55ed25b6bb78e8b91e79b22e22cc48661b36c08643e`;
manual1006632 bytes SHA256
`9798f5a91ce10842ac922dbbaaf81ad8e76d5945294c448c4329f8708d79ebad`;
PDF/text/render và retrieval JSON. Đã xem schematic render. Controller datasheet
link riêng trên LCDWIKI timeout; không ghi nó như một PDF đã tải/đọc thành công.
API implementation đối chiếu pinned source Espressif và tài liệu IDF/LVGL bên dưới.

## 2. Kiến trúc và phạm vi source

Kiến trúc cố định giữ nguyên: STM32F407VET6 CONTROL, WeAct ESP32-S3-B N16R8 UI,
UART protocol v2. Core STM32 không được link vào firmware UI; native simulator
mới sử dụng Core để kiểm thử integration. Không có UAV/RF/CRSF/PWM output mới.

| Thành phần mới | Trách nhiệm |
|---|---|
| `experiments/tft_spi_320x240/display_contract.*` | Geometry/profile, GPIO policy, bounded region, RGB565 wire order, buffer ownership và transfer counters |
| `tft_presenter.*` | Chiếu read-only cache thành UI view; freshness/field expiry, SAFE/STALE/DISCONNECTED, SIM fixtures |
| `tft_lvgl.*`, `lv_conf.h` | LVGL owner duy nhất, HOME/TELEMETRY/MODEL/SETTINGS, navigation, diagnostic và rotation |
| `touch_adapter.*` | Contract cho callback SPI-touch đã được xác minh/calibrate sau này; hiện không có IC driver/runtime |
| `native/` | Actual LVGL mock framebuffer/export,30 functional/integration tests và6 actual-SPI-backend tests dùng SDK fakes |
| `esp32/experiments/tft_spi_320x240/` | Project IDF riêng, pin/config layer, esp_lcd SPI backend, compatibility entry và component wrappers |

Entry mới compile-include **nguyên file** `esp32/main/main.c`, đổi tên entry thành
`tyvor_existing_bridge_start`, rồi thêm task TFT. Reuse tested UART/session owner
và queue publication; không fork/copy UART state machine hoặc sửa file cũ.
TFT task dùng `xQueuePeek` lấy value copy, không sửa cache owner. Logger cũ vẫn có
banner/version Phase2C; banner experiment và app descriptor mới phân biệt image
`0.3.0-tft-experiment`. Không đổi diagnostic wire version để phục vụ display.

TFT/LVGL chỉ ở UI layer; backend interface `start/wait/micros/rotate` không nhận
Core writer, ARM callback hoặc physical output. Thay sang AMOLED sau này cần UI
profile/backend tương ứng sau khi biết module, không cần sửa Core/protocol.
**Hiện chưa triển khai AMOLED hardware backend.** Các file/mock geometry368×448
cũ giữ nguyên, không thay cấu hình AMOLED bằng TFT.

## 3. Driver, geometry, DMA và ownership

- Profile **`TFT_SPI_320X240`**; orientation0 mặc định **portrait240×320**.
  Rotation90/270 là320×240;180 là240×320. LVGL resolution đổi cùng panel
  swap/mirror; không xoay lần hai trong software. Mapping vật lý còn cần test glass.
- Dùng IDF5.5.1 `esp_lcd` và source Espressif `esp_lcd_ili9341` metadata2.1.0,
  pinned commit `67f41a169401d1f23e827188dd08fb351cb0febe`.
  [Driver source](https://github.com/espressif/esp-bsp/tree/67f41a169401d1f23e827188dd08fb351cb0febe/components/lcd/esp_lcd_ili9341)
  giữ nguyên byte. Wrapper cấp version macros2.1.0 thay CMake version-helper của
  component manager; không tải/cài global package.
- LVGL9.2.2 pinned commit `7f07a129e8d77f4984fff8e623fd5be18ff42e74`;
  archive SHA256 `8a5e30223fd5b08cdeccb060f45b37a29868d6b1fdfc5bdbdf0dd89aaefb5a0e`.
  Lock kiểm tra **839 dependency source files** trong `.tools/sdk`.
  MIT/Apache2 license được giữ và copy vào package.
- RGB565 native pixels chuyển thành wire MSB-first đúng một lần trước submit.
  Tests xác minh red`F800`,green`07E0`,blue`001F`. RGB/BGR element order là lựa
  chọn riêng, không nhầm với byte swap. DefaultRGB chưa chứng minh màu actual.
- SPI2_HOST, mode0, `SPI_DMA_CH_AUTO`, queue depth2, candidate10MHz. Một persistent
  buffer DMA-capable internal RAM **20480 bytes**, partial render; không cấp full
  framebuffer153600 bytes trên ESP. Đây là single-buffer DMA, chưa có overlap
  render/transfer kiểu double-buffer.
- Bounds check exclusive right/bottom, byte count, profile/resolution và capacity
  trước submit. Full frame do LVGL chia vùng; không truyền một block quá buffer.
- ISR chỉ give semaphore. Task UI chờ completion rồi mới gọi
  `lv_display_flush_ready`; ISR không sửa LVGL/cache và không refresh watchdog.
  Busy/timeout/start-error/clock regression latch fault và giữ ownership; không
  tái dùng buffer có thể còn DMA. UI abort/reboot ESP trên transfer fault.
- Colour-completion wait250ms/chunk. Lệnh điều khiển/submit trong vendor driver có
  thể chờ ở SDK trước điểm wait này; không tuyên bố250ms là WCET toàn pipeline.
  TFT task được subscribe task-WDT5s, feed sau progress, kể cả diagnostic loop.
  Bridge giữ task riêng priority5; TFT priority2 không giữ mutex của bridge.
  Watchdog/SPI/ISR/RTOS timing actual chưa đo. Reset ESP không có dây reset STM.

Đối chiếu [IDF SPI LCD](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/lcd/spi_lcd.html)
và [LVGL9.2 display/buffer contract](https://docs.lvgl.io/9.2/porting/display.html).
Command/data polarity theo `esp_lcd` SPI defaults, không sao chép một mô tả mức
DC/RS thiếu nhất quán trên trang interface. Init/gamma/inversion/colour order
chuẩn của vendor chưa phải init sequence đã xác minh đúng module đang có.

## 4. GPIO, power và touch gates

GPIO mapping tách trong `board_pins.*`/Kconfig, không nằm trong ILI9341 driver.
**Tất cả GPIO TFT hiện=-1.** Hardware gate, board/module verification flags,
touch runtime đều OFF. UART gate của cả project cũ và experiment giữ OFF;
project experiment từ chối cấu hình bật UART hoặc touch runtime.

Bảng dưới chỉ là **MSP3218 reference**, không là sơ đồ đấu màn thực tế:

| Reference header | Label | Vai trò | ESP assignment actual |
|---|---|---|---|
| 1 | VCC | Module supply, regulator/bypass policy phụ thuộc PCB | **TBD /UNVERIFIED**, chưa duyệt mức cấp |
| 2 | GND | Ground reference | TBD /UNVERIFIED |
| 3 | CS | Display chip-select | -1 |
| 4 | RESET | Panel reset | -1 |
| 5 | DC/RS | Display command/data | -1 |
| 6 | SDI/MOSI | Display data out từ ESP | -1 |
| 7 | SCK | Display SPI clock | -1 |
| 8 | LED | Backlight control trên reference, không coi là bare LED power | -1 |
| 9 | SDO/MISO | Display read, optional | -1 |
| 10 | T_CLK | Touch SPI clock nếu có IC | Chưa gán, runtime OFF |
| 11 | T_CS | Touch select nếu có | Chưa gán |
| 12 | T_DIN | Touch SPI input nếu có | Chưa gán |
| 13 | T_DO | Touch SPI output nếu có | Chưa gán |
| 14 | T_IRQ | Touch IRQ nếu có | Chưa gán |

GPIO policy loại memory26–37, USB19/20, boot/strap0/3/45/46, debug39–42,
USB-UART43/44, RGB LED48 và dual-domain47. **17/18 tiếp tục dành cho UART proposal**,
không dùng cho TFT ngay cả khi UART gate đangOFF. GPIO22–25 không thuộc tập GPIO
của S3. Các chân còn được policy cho phép cũng chưa được xác minh routing/load
trên actual board; whitelist không là approval pinout. Duplicate pins bị reject;
chỉ MISO/backlight có thể=-1 khi hardware được xác minh, các tín hiệu bắt buộc phải đủ.

Policy kiểm tra cả hardware gate và hai verification flags **trước bất kỳ
SPI/GPIO/LCD initialization nào**. Six SDK-fake tests compile actual backend body
và kiểm tra zero peripheral calls khi gateOFF/thiếu evidence/config sai.
Các GPIO12/11/... trong test là synthetic fixture, **không** là wiring proposal.

Nguồn reference có XC6206 và J1 bypass; net VCC,logic3V3 và backlight là các domain
khác nhau. Phải xác minh actual regulator/bypass/population/current trước quyết
định nguồn. Không suy “cấp5V được” từ một module gần giống, không cấp backlight
load trực tiếp từ GPIO, không tự nối rail3V3/5V giữa hai MCU.

Touch adapter chỉ nhận callback đã được xác minh, tọa độ đã normalize/calibrate,
valid timestamp/bounds. Disabled adapter không gọi callback; stale/future/out-of-
range samples bị bỏ. Không có driver XPT2046 hoặc LVGL hardware input-device đăng
ký trong firmware này. Sau khi xác nhận IC mới có thể thêm runtime SPI/calibration/
rotation adapter. Sự kiện UI hiện chỉ đổi trang; không có ARM/output action.

## 5. UI và diagnostics

HOME có model name,battery,LQ,SIM timer,connection/safety và **ARM UNKNOWN /RO**.
TELEMETRY có battery,LQ,RSSI,GPS bằng độ thập phân và satellites. MODEL/SETTINGS
chỉ đọc; PREV/NEXT và auto-navigation không phát command điều khiển.

Presenter lấy value copy rồi poll expiration theo clock hiện tại. Khi STALE hoặc
DISCONNECTED: model/telemetry/timer bị ẩn, SAFE không được suy ra từ cache cũ.
Telemetry fields hết hạn độc lập không được dùng như dữ liệu đang hoạt động.
Safety enum không hợp lệ không được đổi thành SAFE. `BENCH ARMED (SIM)` luôn tách
khỏi aircraft ARM; không hiển thị nó thành UAV armed.

Default simulator được gắn nhãn **SIMULATOR**; bốn scenario VALID/SAFE,STALE,
DISCONNECTED,BENCH_ARMED chạy ở UI layer. Source STM32 dùng existing publication
queue; UART hiệnOFF nên không có live board data. **Protocol v2 hiện không có
flight-timer field**: live mode hiện`--:--`, không lấy Core ticks làm flight time.
Timer có giá trị chỉ trong SIM và bị invalid cùng stale/disconnect.

Boot diagnostic chuẩn bị red/green/blue,RGB565 bars, chữ **TYVOR OS**, rectangle/
border, corner TL/TR/BL/BR và bốn rotation. Runtime log có render/frame/chunk last/
max time,internal heap/largest block,PSRAM free và UI stack watermark. Backend
mock ghi rõ **NOT SPI TIMING**; counters trong native dùng deterministic simulated
cost, không phải số đo màn. Diagnostic không tắt safety/watchdog checks.

## 6. Kết quả build/test thực tế

| Kiểm tra | Kết quả |
|---|---|
| Native Debug | **234/234 PASS**,0 fail/skip |
| Native Release/NDEBUG | **234/234 PASS**,0 fail/skip; compile commands xác minhNDEBUG |
| Baseline regression | 177 Phase2C +21 Discovery mỗi config, không sửa test cũ |
| TFT tests mới | 30 functional/render/integration +6 actual-SPI-backend SDK-fake cases mỗi config |
| LVGL mock rendering | **84/84 renders**:64 UI (4 states×4 pages×4 rotations),20 diagnostic |
| Config/bounds/RGB565 | PASS; reserved/duplicate/unverified pins reject, invalid regions reject |
| DMA ownership/error | PASS; busy/timeout/start-error/clock regression không release sớm |
| Partial update | PASS; đổi riêngLQ có transfer nhỏ hơn full frame |
| Stale/session recovery | PASS; control reset,UI reset,both reset và loss/reconnect invalidate rồi recover |
| UI authority | PASS; native navigation giữ nguyên Core state; ESP ELF không link Core safety/mixer/RF hoặc touch IC driver |
| ESP-IDF5.5.1 /S3 | **Application ELF/BIN/MAP +bootloader +partition build PASS** |
| Offline package/hash/path/offset validation | **PASS**; không mở serial/flash |
| Display/touch/PSRAM/electrical/WCET hardware | **NOT RUN /UNVERIFIED** |

Assertions mới dùng CHECK luôn chạy, không dựa vào C `assert` bị NDEBUG tắt.
Test session/render tiêu thụ cache thật từ native dual-MCU simulator/protocol v2,
không giả handshake thành công. Những test DMA/SPI là SDK fakes, không là bench
electrical result. Existing tests vẫn kiểm tra parser/CRC/timeout/sequence/session/
stale/async saturation và safety; không đổi protocol để làm UI test PASS.

Các lỗi tích hợp của source mới đã được xử lý: `CONFIGURE_DEPENDS` không dùng
được trong IDF script-mode requirement scan; version macros driver cần wrapper
cấp; Kconfig booleanOFF không tồn tại như C macro nên được normalize trong header
experiment; GPS format cần cast rõ vì uint32_t khác underlying type giữa native
và Xtensa. Không sửa vendor source hoặc hạ validation/warnings để che lỗi ứng dụng.
Preview review cũng sửa banner wrapping và battery unit ở landscape. Kết quả
trong bảng là **final successful receipts**, không lấy số của các build lỗi trước.

Build logs/JUnit/receipts/config/size/stack/renderer hashes tại `build/tft/evidence/`.
`inspect`/export từ chối stale build receipts trước khi công nhận artifact/render.

Regression offline của package validator: bản sao fixture ghi UART ON trong
`sdkconfig.build` nhưng manifest vẫn ghi OFF và hashes được cập nhật. Trước sửa,
validator chấp nhận sai (**FAIL**); sau sửa, từ chối đúng (**PASS**). Bằng chứng:
`package-policy-before.json` / `package-policy-after.json`. Firmware và cấu hình
thật không bật UART. Validator cũng đối chiếu GPIO/gates, chip/flash arguments,
image identity và partition CSV/binary; đây là kiểm tra đóng gói riêng, không
cộng vào234 native tests hay coi là kiểm thử phần cứng.

## 7. Artifacts và memory

Package riêng: `artifacts/tft-spi-320x240-experiment/`, manifest `SHA256_MANIFEST.json`.
ESP artifacts trong subfolder `esp32/`; native executables,preview,evidence và
licenses được đóng gói riêng, không thay Phase1–2D/Discovery package.

| Thông số cuối | Giá trị /ý nghĩa |
|---|---|
| App BIN | **455232 bytes**, fit factory partition3145728 bytes |
| ELF có debug info | 6362104 bytes; không phải Flash footprint |
| MAP | 3868757 bytes |
| Bootloader /partition | 20896 /3072 bytes |
| Offsets | Bootloader`0x0`,partition`0x8000`,app`0x10000` |
| Flash config/header | ESP32-S3,DIO80MHz,16MB; không là physical detection |
| IDF Flash code /rodata+appdesc | 271498 /98904 bytes |
| DIRAM allocated | **174667 /341760 bytes**,51.11%; gồmBSS106368,data11760,text56539 |
| Dedicated IRAM section | 16384 bytes (vectors1028 +text15356); không có nghĩa toàn bộ SRAM hết |
| Remaining linked DIRAM | 167093 bytes, chưa trừ mọi runtime allocation/stack |
| LVGL builtin arena | **98304 bytes** đã nằm trongBSS; không cộng hai lần |
| Persistent draw buffer | **20480 bytes** heap INTERNAL/DMA/8BIT aligned64, runtime allocation |
| Added UI task stack | **8192 bytes** runtime; existing bridge/diagnostics mỗi4096 |
| Largest application .su frame | diagnostics_task1568,tft_task1408,presenter960 bytes; không là call-chain peak |

BIN SHA256:
`185da43096beb52c205d05f10f5d8eae1fa1853390580aaae11d3b15e698768c`.

App image checksum/appended SHA và ELF identity kiểm tra khớp; binary partition
bounds và metadata paths được kiểm tra offline. PSRAM modeOCT40MHz, N16R8 là
target/config; `IGNORE_NOTFOUND` vẫn bật nên boot không chứng minh8MB. Buffer DMA
đặt internal RAM; heap/PSRAM/stack high-water actual chưa đo.

Ở10MHz, payload full-frame RGB565153600 bytes cần tối thiểu122.88ms theo phép tính
bit-rate, chưa tính command/driver/render overhead. Đây **không** phải FPS/WCET
đã đo. Partial updates giúp giảm lượng dữ liệu; frame duration thực và worst-case
load phải đo trên board. Pixels có thể giữ hình cũ nếu panel/SPI hỏng hoặc reset;
software freshness tests không chứng minh một hardware stale-blanking mechanism.

## 8. Files changed, WIP và tái lập

So với144 file baseline: **143 file cũ không đổi**, sửa duy nhất root
`CMakeLists.txt` để thêm option **defaultOFF** cho experiment; **45 file mới**,
tổng189 source/docs/scripts (không tính tools/build/artifacts). STM32 Core/BSP,
shared protocol/cache, original ESP main/config và AMOLED368×448 files giữ nguyên.
Original generated `esp32/sdkconfig` vẫn khớp hash schematic review trước.

New files theo nhóm:39 file dưới `experiments/tft_spi_320x240/` và
`esp32/experiments/tft_spi_320x240/`;5 script/lock files (`build-tft-experiment.ps1`,
`fetch-tft-dependencies.py`, `tft-dependencies.lock.json`, `tft_artifacts.py`,
`export-tft-preview.py`); báo cáo này. Full file inventory/source hashes và patch
so baseline ở `evidence/source-changes.json`, `tft-diff.patch` trong package.

Git root cha vẫn báo workspace untracked; không dùng tracked diff rỗng làm bằng
chứng không đổi. Snapshot ở `build/tft-baseline/source/` và preservation JSON giữ
bằng chứng WIP. Package lịch sử kiểm tra lại hashes; không tự stage/commit/push/
deploy, sửa Git global hoặc repository STABLE.

Tái lập từ workspace:

```powershell
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/fetch-tft-dependencies.py --verify-only
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-tft-experiment.ps1 -Target all
& ./.tools/espressif/python_env/idf5.5_py3.11_env/Scripts/python.exe scripts/tft_artifacts.py inspect
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/export-tft-preview.py
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/tft_artifacts.py package
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/tft_artifacts.py validate
```

Dependencies missing thì dùng fetch không có`--verify-only` để lấy đúng locks vào
`.tools`. PowerShell policy override chỉ áp dụng child process. Build script không
có flash/serial step; các lệnh flash mà IDF in ở footer **không được thực thi**.

## 9. Điều kiện trước bước hardware

Cần ảnh PCB thực tế hai mặt, labels/revision/pin1, TFT/FPC/controller và touch-IC
markings; xác minh regulator/bypass/backlight và điện áp/current. Đối chiếu WeAct
revision/Flash/PSRAM, chốt actual GPIO/ground/power plan tránh reserved pins/UART.
Khi đó mới review init/colour order/inversion/orientation và một phiên flash/test
riêng có cho phép. Touch SPI cần xác nhận IC +driver/calibration riêng; không mặc
định XPT2046, không lấy pinout/touch I2C từ AMOLED.

Chưa có phép nạp/đấu dây/power test trong phiên này. **READY cho software UI review;
NOT READY cho display hardware bring-up. Gates OFF, RF OFF, dừng chờ review.**
