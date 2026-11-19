# TYVOR OS V1 — Tactical Dark UI redesign V2

Ngày **12/11/2026**, Asia/Bangkok. Software-only experiment, workspace
`D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**READY FOR SOFTWARE/UI REVIEW. NOT READY FOR DISPLAY HARDWARE BRING-UP.**
Native **268/268 Debug +268/268 Release/NDEBUG PASS**; ESP32-S3 app,bootloader,
partition build PASS. **142 actual LVGL screenshots**, hai profile, không phát
hiện parent overflow,text clipping hoặc text overlap trong bộ render kiểm tra.
Không flash hoặc chạy trên board/màn thật; không có hardware PASS.

## 1. Oxbot reference analysis

Nguồn: [Oxbot R1 official QSG](https://oxbot.cn/product/OxbotR1/Oxbot-R1-QSG-cn.html),
các mục2.2,3.5,4.1,5 và6. Đã xem hình HOME và poster minh họa; không coi
poster đầu video là bằng chứng đã xem mọi frame animation.

Patterns rút ra: status bar gọn, timer trung tâm, safety tách khỏi dữ liệu,
widget chia nhóm. Tài liệu mô tả vuốt trái vào Model, vuốt lên vào Quick Settings,
monitor kênh, chỉnh layout, settings phân nhóm và telemetry. Throttle lock là
trạng thái cần thấy rõ. TYVOR áp dụng phân cấp và đường điều hướng này, nhưng
giữ tương tác chỉ đọc; không mang các lệnh RF/bind/ARM sang UI.

HTML/ảnh reference chỉ lưu trong `build/ui-v2/evidence/reference/` để nghiên cứu.
Không đưa logo,trademark,ảnh,video hoặc source/firmware Oxbot vào sản phẩm hay
package review. `reference-evidence.json` ghi URL/hash và giới hạn quan sát.

## 2. Design system

| Token | Giá trị |
|---|---|
| Background /Surface /Card | `#0B1220` /`#111C2D` /`#17263C` |
| Primary /Success | `#38BDF8` /`#22C55E` |
| Warning /Danger | `#F59E0B` /`#EF4444` |
| Primary text /Secondary text | `#F1F5F9` /`#94A3B8` |

Dark-first, radius6–8px, không gradient/shadow nặng. Original line glyphs dùng
LVGL vector primitives, cùng kích thước20px. Không tải một icon/font thương mại.
Montserrat12 cho captions,14 cho thông tin,20 cho giá trị/model,48 cho timer.
Font48 là font đầy đủ từ pinned LVGL, chưa subset; footprint được đo ở mục7.

HOME dùng timer lớn, safety strip riêng và hai metric cards; màn phụ dùng header,
cards và pagination thống nhất. Global navigation BACK/HOME/MODEL/QUICK luôn ở
đáy, nút tối thiểu44px cao; trang đang chọn được highlight. Không dùng smooth
page animation: chuyển trang tức thời để giảm lượng SPI traffic.

Hai layout dùng constraints riêng, không scale đồng loạt: 240×320 có pad10/gap6,
368×448 có pad16/gap10; nav44px cố định. Profile lớn có hero/card cao hơn,
gimbal lớn hơn, stick activity và thêm ngữ cảnh. Các danh sách dùng bank/detail
pagination; không có hidden off-screen controls. Tên model dài chọn font phù hợp;
header có thể dùng model ID, trong khi HOME/MODEL vẫn giữ đủ tên.

## 3. Component architecture

| Module mới | Trách nhiệm |
|---|---|
| `experiments/tyvor_ui_v2/ui_view.*` | Chiếu value-copy cache, gọi freshness logic đã có; field validity và SIM/live separation |
| `ui_theme.*`, `lv_conf.h` | Tokens,profile constraints, LVGL9.2.2 config; giữ arena96KiB, thêm font48 |
| `ui_widgets.*` | Cards,buttons,labels,original vector icons,bars,font fitting |
| `ui_pages.c` | 7 trang +Quick Settings; current-page construction và value updates |
| `ui.*` | LVGL owner,partial flush,history16 entries,gestures,local navigation,layout inspection |
| `native/mock.*`, `render.c` | RGB565 framebuffer backend và actual LVGL screenshot exporter |
| `native/simulator_win32.c` | PC window/mouse/keyboard/swipe, cùng UI code và LVGL renderer |
| `native/test_ui.c` | 33 functional/integration tests; thêm1 hidden-host test trong CMake |
| `esp32/experiments/tyvor_ui_v2/` | Project ESP-IDF riêng, version`0.4.0-ui-v2`, không thay firmware experiment V1 |

ESP entry compile-include nguyên `esp32/main/main.c`, đổi tên entry rồi thêm UI
task; reuse original bridge/publication queue. UI đọc bằng `xQueuePeek` value copy,
không sửa owner cache, không giữ lock của bridge. Original logger/banner vẫn có
identity Phase2C; app descriptor và log UI2 phân biệt image mới. Protocol/session
code không fork hoặc sửa. UI task priority2/8192-byte stack; bridge priority5 vẫn
độc lập, queue một phần tử overwrite để UI chậm không giữ backlog.

Generic UI port nhận vùng RGB565 wire-order và chỉ trả thành công sau completion.
ESP adapter dùng lại TFT bounded DMA pipeline; buffer chỉ release sau DMA, timeout
vẫn fault/abort. Chỉ UI task gọi LVGL; ISR không sửa object/cache. UI subscribe
task watchdog, feed sau progress. SPI SDK command/submit có thể chờ ngoài colour
wait250ms; watchdog không chứng minh WCET. Không có lock UI giữ mà bridge cần.

368×448 là **software layout profile**, không phải CO5300/AMOLED driver. Việc thêm
backend đã xác minh sau này không cần đổi Core. Cấu hình AMOLED cũ giữ nguyên.

## 4. Các trang và authority

| Trang | Nội dung/interaction |
|---|---|
| HOME | Model,connection,battery/RF published flag,safety/throttle lock,timer,LQ,RX/mode state; shortcuts INPUTS/TELEMETRY; profile lớn có stick activity |
| MODEL MANAGER | Published model card,active/invalid indicator,ID/revision,detail; remote selection **UNAVAILABLE** |
| INPUTS | Hai gimbal visualizers,4 normalized axes,2 switches,validity; chỉ đọc |
| MIXES | Logical source→bus,ADD/REPLACE,weight/offset/trim;4 terms/bank,tối đa16; thiếu/mismatched generation/expired cache là UNAVAILABLE |
| OUTPUTS | 8 memory-channel values,4/bank,bars; input/model/output invalid thì không hiển thị giá trị cũ |
| TELEMETRY | Battery,LQ,RSSI,GPS validity,status age; detail latitude/longitude; field expiry độc lập |
| RADIO SETTINGS | Display/system/diagnostics/hardware categories,session/version detail; hardware options,touch calibration,layout editor không bật |
| QUICK SETTINGS | Local shortcuts tới6 monitor pages; không có RF/ARM/output action |

Mouse: click hoặc drag left/up trên HOME. Keyboard: H/Home,Esc/Backspace,
left/right,M/I/X/O/T/R/Q,PageDown. BACK quay trang trước; trong detail quay bank0;
HOME xóa navigation history. Chỉ các lựa chọn navigation được hỗ trợ; không có
remote model mutation hoặc controls giả dạng chức năng đã hoạt động.

**DEMO** và **SIM TIMER** phân biệt fixtures. Aircraft ARM luôn UNKNOWN; BENCH ARM
là trạng thái software bench, không là aircraft armed. Live protocol v2 chưa có
flight timer,receiver connectivity hoặc flight mode: UI hiển thị UNAVAILABLE,
không suy từ Core ticks,throttle,LQ hoặc uptime. Timer SIM do caller cung cấp và
được invalidate khi stale/disconnected. Valid RF flag chỉ là published logical
flag; UI không bật RF. INVALID MODEL có màu đỏ và chặn rendering valid outputs.

Presenter poll bản sao state theo clock hiện tại. Session chưa ready/disconnected
hoặc hết TTL thì clear active model/inputs/outputs/mixes/telemetry/timer/RF validity;
safety trở UNKNOWN. Input hết hạn cũng invalidate outputs; telemetry từng field
hết hạn không trở thành data đang hoạt động. Không giữ trạng thái ARM cũ có hiệu
lực. Software không bảo đảm xóa pixels vật lý khi panel/SPI treo hoặc ESP reset.

## 5. Preview trước/sau và visual QA

Tất cả ảnh mới dùng **LVGL9.2.2 software rasterizer RGB565 thật**, compile cùng
`ui.*`/pages/widgets/config như ESP image. HTML chỉ là gallery, không là một web
renderer thay thế LVGL. Không có ảnh AI/generated mock làm bằng chứng firmware.

- [HOME240×320](../artifacts/tyvor-ui-v2/home-240x320.png).
- [HOME368×448](../artifacts/tyvor-ui-v2/home-368x448.png).
- [7 trang ở hai profile](../artifacts/tyvor-ui-v2/all-pages-contact-sheet.png).
- [VALID/STALE/DISCONNECTED/SAFE/INVALID MODEL](../artifacts/tyvor-ui-v2/states-240x320.png).
- [Trước/sau](../artifacts/tyvor-ui-v2/before-after-comparison.png): V1 bên trái,V2 TFT giữa,V2 AMOLED-layout bên phải; fixtures/timer khác nhau, không so pixel tương đương dữ liệu.
- [Gallery142 ảnh](../artifacts/tyvor-ui-v2/index.html), [PC host capture](../artifacts/tyvor-ui-v2/simulator-preview.png), [hướng dẫn simulator](../artifacts/tyvor-ui-v2/RUN_SIMULATOR.md).

142 ảnh gồm80 primary (8 pages×5 states×2 profiles),50 detail/bank và12 edge
(live missing fields,longest model name,independent telemetry expiry). Kiểm tra
real object bounds trong parent,full text width/height và actual glyph overlap
giữa sibling labels; **0 lỗi trong bộ này**. Đã xem contact sheets,HOME cả hai
profile,các trạng thái,Quick,telemetry detail/expiry,long model và before/after.
Đây không phải bằng chứng mọi chuỗi/model/tổ hợp runtime đều đã được exhaustively
kiểm tra, hoặc physical touch target đã được đo trên TFT.

Các lỗi thực tế bắt được và sửa: width chưa layout khiến trang đầu/gimbal lệch;
label DOT có thể đổi nội dung trước layout; RX/GPS unavailable wrap; line-height
font12 thực tế15px gây chồng1px; Release/IDF strict warnings; idle style updates
gây redraw; ReleaseCapture trước LVGL release làm mất click; bank MIXES cũ sau
expiry. Sửa source/constraints/ordering, giữ assertions. Các log lần đầu FAIL và
visual-qa JSON được giữ; final receipts dùng build thành công cuối.

## 6. Build/test

| Kiểm tra thực tế | Kết quả |
|---|---|
| Native Debug | **268/268 PASS**,0 fail/skip |
| Native Release/NDEBUG | **268/268 PASS**,0 fail/skip; compile commands xác minh NDEBUG |
| Baseline | 234 tests cũ (177 Phase2C +21 Discovery +36 TFT), source tests cũ không sửa |
| UI V2 | 33 assertions-based functional/integration tests +1 Win32 host test mỗi config |
| LVGL render/layout | **142/142**,0 bounds/clipping/text-overlap errors trong corpus |
| PC interaction | Actual hidden Win32 host mouse click/keyboard/swipe PASS; không mở cửa sổ visible để chạy dài |
| Freshness | STALE/DISCONNECTED,invalid model,input/telemetry/mix expiry,clock regression,mix generation,missing live timer PASS |
| Session recovery | Independent control reset/UI reset/both reset,loss/recovery dùng simulator/protocol thật PASS |
| Read-only | Navigation giữ nguyên byte Core/endpoint; RF disabled; ESP ELF không link Core safety/mixer/model writers hoặc radio/touch runtime |
| ESP-IDF5.5.1 /S3 | Application ELF/BIN/MAP +bootloader +partition **PASS** |
| Package/hash/config/paths | **PASS**;checksum/ELF identity/offsets/partition/gates/GPIO;1 positive +8 negative fixture checks |
| Display/touch/PSRAM/timing hardware | **NOT RUN /UNVERIFIED** |

Assertions mới là CHECK luôn chạy cả NDEBUG. Memory test đi500 lần navigation,
quay HOME và kiểm tra free-pool không giảm ngoài budget512B; không chỉ đếm số test.
Tests reset/loss không giả handshake thành công; dùng native dual-MCU simulator.
Strict compile warnings được sửa, không tắt warning hoặc sửa test để PASS.

Toolchain: Clang20.1.7,ESP-IDF5.5.1,Xtensa GCC14.2.0,CMake3.30.9,Python3.11.13.
839 pinned vendor source hashes giữ nguyên. Receipt ghi hashes source/compiler/
executables,JUnits và config; export từ chối native receipt stale. Bộ evidence và
source patch trong package giúp đối chiếu kết quả với code hiện tại.

Package negative checks chỉ sửa bản sao fixture rồi cập nhật hashes tương ứng:
UART ON,display ON,GPIO được gán,missing image,sai offset,path traversal,BIN bị
corrupt và CSV khác binary đều bị reject. Package thật và GPIO/gates không đổi.
Các kiểm tra offline này tách riêng,không cộng vào268 CTests.

## 7. Memory và performance

| Thông số | Kết quả cuối |
|---|---|
| App BIN | **562112 bytes**,factory partition3145728 bytes |
| ELF debug /MAP | 6562888 /3898036 bytes; không phải Flash footprint |
| Bootloader /partition | 20896 /3072 bytes |
| Flash config | 16MB,DIO80MHz;offsets0 /0x8000 /0x10000 |
| Flash code /rodata+appdesc | 280286 /197000 bytes |
| DIRAM linked | **175451 /341760 bytes**,51.34%;BSS107152,data11760,text56539 |
| Linked DIRAM còn lại | 166309 bytes,trước runtime allocations/stacks |
| Dedicated IRAM region | 16384 bytes; không có nghĩa toàn bộ SRAM đã hết |
| LVGL builtin arena | **98304 bytes** đã trong BSS,không cộng hai lần |
| Draw buffer | **20480 bytes**,INTERNAL/DMA/8BIT aligned64,runtime allocation |
| UI task stack | 8192 bytes;bridge/diagnostics giữ4096 mỗi task |
| Largest application .su frames | UI task1792,diagnostics1568,UI2 presenter992,V1 presenter960 bytes;không là call-chain peak |
| Font Flash object contributions | Font48 **96725**,font20 21799,font14 13645,font12 11484 bytes;map/object totals,không chỉ font descriptor |

App BIN SHA256:
`c66e3ba6136276a229463b14addf02f713aa1608ffc4eb7ff4878db2aa30bd0e`.
So V1: BIN +106880 bytes; linked DIRAM +784 bytes,không tăng LVGL arena/draw buffer.
Font48 đầy đủ chiếm phần lớn Flash tăng; còn dư factory partition. Subset font số
có thể giảm footprint sau này, chưa thực hiện hoặc tuyên bố đã tối ưu xong.

Native renderer: đổi riêng LQ gửi3038 pixels ở240×320 (3.96% full frame),5082 ở
368×448 (3.08%); lặp cùng view gửi**0 pixels**. HOME arena-used21328 /23080 bytes;
observed navigation/render peak26136 /26496 trong tiến trình native, không phải
ESP heap/stack high-water. Current page chỉ rebuild khi navigation; updates đổi
text/value/style khi cần. Không cấp framebuffer đầy trên ESP; buffer đủ vùng
partial ở cả hai chiều rộng. Mock native framebuffer riêng không vào firmware.

Mock transfer cost là deterministic simulation. Không báo FPS,WCET,SPI throughput,
heap hoặc PSRAM thực từ các số native. PSRAM OCT40MHz/IGNORE_NOTFOUND vẫn là config,
chưa chứng minh8MB. Cần đo SPI/DMA,heap/stack/watchdog trên actual board sau này.

## 8. Hardware gates và giới hạn

**DISPLAY OFF; TOUCH OFF; UART HARDWARE OFF; RF OFF; GPIO display toàn bộ=-1.**
Board/module verification flags OFF. CMake và compile-time guard của V2 từ chối
bật display/touch/UART. Driver pin mapping/policy giữ nguyên V1; GPIO17/18 vẫn dành
cho UART proposal. Không gán hoặc chốt chân mới. Touch adapter/IC driver không
được bật; PC pointer là Win32 input duy nhất được thêm.

ILI9341 vẫn **PROBABLE**,actual PCB/pinout/regulator/backlight chưa xác minh;
touch IC UNKNOWN,không mặc định XPT2046. Không dùng touch I2C của AMOLED cho TFT.
368×448 render không là AMOLED driver PASS. Không kết nối RF/UAV,flash,serial/
ST-LINK,option bytes hoặc hardware timing test trong phiên này.

## 9. Git diff/WIP và tái lập

Baseline V1 được freeze trước chỉnh sửa:189 source/docs/scripts. **188 file cũ
không đổi**; chỉ root`CMakeLists.txt` thêm option`TYVOR_BUILD_UI_V2` **defaultOFF**.
29 file mới:17 portable/native files,8 ESP project files,3 scripts và báo cáo này;
tổng218 source/docs/scripts. Core STM32,safety,mixer,ARM machine,shared protocol,
cache/bridge,existing TFT experiment và AMOLED source/config không sửa.

Snapshot tại`build/ui-v2-baseline/source/`,inventory/hash/diff ở
`evidence/source-changes.json`, `ui-v2-diff.patch`, `preservation.json`. Git root
cha vẫn báo`?? ./`; tracked diff rỗng không được coi là bằng chứng không có WIP.
Package TFT V1 được validate/hash lại,giữ nguyên; không thay historical artifacts.
Không stage,commit,push,deploy,sửa Git global hoặc repo STABLE.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-ui-v2.ps1 -Target all
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/export-ui-v2.py
& ./.tools/espressif/python_env/idf5.5_py3.11_env/Scripts/python.exe scripts/ui2_artifacts.py inspect
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/ui2_artifacts.py package
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/ui2_artifacts.py validate
```

PowerShell policy override chỉ cho child process,không sửa policy/global config.
Các lệnh flash mà IDF in ở footer build log **không được thực thi**.
Package và previews ở`artifacts/tyvor-ui-v2/`,có`SHA256_MANIFEST.json`,ELF/BIN/MAP,
bootloader,partition,native executables,JUnits,evidence và licenses.

## 10. Điều kiện để lên TFT thật

Cần ảnh hai mặt PCB màn/ESP board,revision/markings/controller/touch population;
xác minh power/regulator/bypass/backlight/logic voltage,actual GPIO và xung đột
Flash/PSRAM/USB/BOOT/UART. Sau review riêng mới có thể phê duyệt pin map,gates và
phiên flash/test. Đo RGB565 order,inversion/rotation,partial DMA/error recovery,
heap/stack và actual frame time. Touch cần xác nhận IC,driver/calibration riêng.

Những chức năng Core chưa publish (flight timer,RX connectivity,flight mode/model
catalog) cần thiết kế authoritative schema ở phase riêng; không tự mở rộng
protocol hoặc suy dữ liệu trong UI này. Editor/calibration/model switching không
được bật chỉ vì đã có trang đẹp.

**Đã chuẩn bị source,build,test,render và evidence cho review phần mềm. Hardware
bring-up chưa được phép; gates OFF. Dừng để người dùng review.**
