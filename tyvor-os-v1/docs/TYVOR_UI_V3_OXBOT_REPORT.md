# TYVOR OS V3 — Oxbot visual replication review

Ngày12/11/2026,Asia/Bangkok. Workspace
`D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Software build/test PASS. HOME visual replication prepared for review;
pixel-perfect/full screen equivalence NOT VERIFIED. Hardware UNVERIFIED.**

V3 dùng landscape320×240 và448×368,không dùng HOME Tactical Dark V2 hoặc thanh
4 nút cố định ở đáy. Đã triển khai LVGL thật,PC simulator,ESP image và bộ đối chiếu.
**302/302 Debug +302/302 Release/NDEBUG PASS**,ESP32-S3 build PASS,152 ảnh LVGL
với0 lỗi layout trong corpus. Không lấy build PASS làm kết luận visual PASS.

## 1. Reference audit và mức độ tương đồng

Đã mở **mọi ảnh** tại`docs/ui-reference/oxbot/`. Cả ba là **logo TYVOR**,không phải
ảnh màn hình Oxbot:`reference-01.png`,`reference-02.png`,`eference-03.png`.
Giữ nguyên tên và bytes; file cuối thiếu chữ`r` trong tên thật. Không có direct
user UI screenshots để xác nhận yêu cầu1:1. Không coi các logo là reference
resolution hoặc bố cục của HOME/Model/Quick.
Ba file mang đuôi`.png` nhưng nội dung là JPEG/JFIF;đã decode theo content để xem,
không đổi tên hoặc convert source. Dimensions/hashes ghi trong reference audit.

Reference HOME hợp lệ bổ sung: [official Oxbot R1 QSG](https://oxbot.cn/product/OxbotR1/Oxbot-R1-QSG-cn.html)
và [HOME still poster400×315](https://oxbot.cn/product/OxbotR1/img_cn_files/oxbot_r1_11_poster.png).
Đã tải lại và xác nhận cùng SHA256`a8e170fde606a390d527ceb43fd10c4752bc525c38055f8bbd86dc48f6c23b6c`.
Reference này có status/model row,safety notch,timer trung tâm,hai function zones,
notice,bốn lower widgets,trim rails và nền teal/blue. Gesture mô tả trong manual
hỗ trợ left→Model,up→Quick,down→connection menu. Still poster không chứng minh
animation timing hoặc mọi trạng thái tương tác.

**Đánh giá:** HOME bám gần tổ chức hình khối/vị trí của poster; typography,background,
icon và English labels vẫn khác. Không gán một phần trăm tương đồng tổng thể.
Các trang còn lại là **derived monitor pages** dùng ngôn ngữ từ HOME,không tuyên
bố đã tái tạo màn Oxbot chưa có ảnh. Điều này tách rõ direct-reference HOME với
phần mở rộng7 trang/Quick/connection theo yêu cầu.

Chi tiết: [reference analysis](../artifacts/tyvor-ui-v3-oxbot/oxbot-reference-analysis.md)
và [visual differences](../artifacts/tyvor-ui-v3-oxbot/visual-difference-report.md).
Reference images chỉ xuất hiện trong hồ sơ đối chiếu,không nhúng vào UI/firmware.

## 2. Những phần đã tái tạo và cấu trúc component

HOME dùng cùng thứ tự vùng: narrow status/model/icons → panel bo tròn với safety
notch → timer lớn → trái GPS VIEW/phải BIND N/A → receiver/safety notice → hai
dial widgets,voltage/mode tiles → trim rails. Không thêm web dashboard hoặc
Material/iOS controls thay cho cấu trúc này.

| Module mới | Trách nhiệm |
|---|---|
| `experiments/tyvor_ui_v3/ui_theme.*` | Palette/constraints landscape |
| `ui_widgets.*`, `ui_shapes.c` | Rounded components,original icons,gradient/triangle backdrop,dial draw events |
| `ui_pages.c` | HOME +8 derived pages; current-page allocation và field updates |
| `ui.*` | LVGL flush,gesture/events,history16,local Back/Home,layout inspection |
| `ui_view.*` | Delegate read-only presenter/freshness V2; không fork protocol/cache logic |
| `timer_font.*` | Generated numeric LVGL fonts từ MIT Montserrat48,không là font Oxbot |
| `native/` | RGB565 mock transport,actual LVGL renderer,Win32 presentation và tests |
| `esp32/experiments/tyvor_ui_v3/` | Project IDF riêng,app version`0.5.0-ui-v3`,pin/backend reuse |
| `scripts/` | Builder,reference decode,font generation,render/overlay/export,provenance/package validation |

Nền`#23394C`→`#163C48`,hero`#4B7380`,lower tile`#3B526A`,text`#E2ECF1`,timer
`#AFC6D0`,warning`#FFB030`. Radius tham chiếu khoảng20–22 trên grid400×315.
Text12/14/20;timer dùng hai numeric subsets,đậm hơn bản thử đầu. Không có full-screen
PNG,ảnh mock hoặc renderer web thay LVGL. Win32/GDI chỉ trình bày framebuffer đã
được LVGL vẽ;HTML là gallery ảnh.

TFT rotation lấy từ **`CONFIG_TYVOR_TFT_ROTATION=1`** trong project riêng và được
truyền tới driver/display pipeline,không nằm trong Core. Profile448×368 chỉ là
software layout: actual CO5300 swap/mirror/rotation vẫn UNIMPLEMENTED/UNVERIFIED,
không tự gán một rotation register chưa biết. Hai profile và trạng thái này được
ghi tại`esp32/experiments/tyvor_ui_v3/display_profiles.json`. Portrait V1/V2 và
AMOLED config cũ giữ nguyên,không dùng làm chuẩn đối chiếu Oxbot.

## 3. Các trang và interaction

| Trang | Behavior |
|---|---|
| HOME | Reference-led timer/action/dial/voltage/mode/trim arrangement; không fixed footer |
| MODEL | Published model,active/invalid indicator,ID/revision; selection/layout editor unavailable |
| INPUTS | Hai dials và normalized axes/switches; trim unavailable |
| MIXES | Source→logical bus,operation,weight/offset/trim;4 terms/bank |
| OUTPUTS |8 memory channels,4/bank,numeric/bars; invalid values được ẩn |
| TELEMETRY | Voltage,LQ/RSSI,GPS,age;detail coordinates/aircraft ARM UNKNOWN |
| SETTINGS | Display/system/hardware/model categories; hardware controls disabled |
| QUICK SETTINGS |6 local monitor shortcuts,không RF/ARM actions |
| CONNECTIONS |Menu theo swipe-down; brightness/wireless/connection commands unavailable |

LVGL events xử lý click/navigation. Swipe HOME:left→MODEL,up→QUICK,down→CONNECTIONS.
Trang phụ có Back/Home cục bộ,ellipsis đổi detail/bank. Keyboard H/Home,
Esc/Backspace,left/right,M/I/X/O/T/R/Q,Down,PageDown;1–5 đổi fixture UI.
PC host có real pointer input vào LVGL. Decorative children không giữ click;
GPS VIEW mở telemetry,BIND không có event handler và ở disabled state.

Navigation tức thời,không bịa timing/curve animation từ ảnh tĩnh. Không tuyên bố
đã tái tạo animation Oxbot hoặc thử cảm ứng vật lý. [Simulator instructions](../artifacts/tyvor-ui-v3-oxbot/RUN_SIMULATOR.md).

## 4. Đối chiếu ảnh và vòng visual QA

- [HOME320×240](../artifacts/tyvor-ui-v3-oxbot/home-320x240.png), [HOME448×368](../artifacts/tyvor-ui-v3-oxbot/home-448x368.png).
- [Reference /TYVOR320 /TYVOR448](../artifacts/tyvor-ui-v3-oxbot/home-reference-comparison.png).
- [Overlay320](../artifacts/tyvor-ui-v3-oxbot/home-reference-overlay-320x240.png), [overlay448](../artifacts/tyvor-ui-v3-oxbot/home-reference-overlay-448x368.png).
- [All pages](../artifacts/tyvor-ui-v3-oxbot/all-pages-contact-sheet.png), [5 trạng thái](../artifacts/tyvor-ui-v3-oxbot/states-320x240.png).
- [Before V2 /after V3](../artifacts/tyvor-ui-v3-oxbot/before-v2-after-v3.png), [gallery](../artifacts/tyvor-ui-v3-oxbot/index.html), [PC host capture](../artifacts/tyvor-ui-v3-oxbot/simulator-preview.png).

Comparison normalize mỗi render về400×315 để đặt cạnh reference;các PNG native
giữ đúng resolution riêng. Aspect ratio khác nhau tạo stretch trong comparison,
không nhầm với hardware screenshot. Overlay50/50:red=estimated reference region,
cyan=actual LVGL object allocation. Geometry JSON được export từ object thật.

Panel/button/footer anchors có center delta0.69–1.29px ở320×240 và0.86–1.43px ở
448×368 so với **hand-estimated** reference. Các số này không chứng minh likeness
của font/texture;không lấy anchor IoU làm overall similarity. Safety tab rộng hơn
để đọc đủ English state. Timer metric dùng estimated glyph region so với label
allocation lớn hơn nên **không phải equivalent boxes**; không dùng để suy một
tỉ lệ visual PASS. Raw RGB MAE chỉ là diagnostic,mọi nội dung/noise/font khác đều
ảnh hưởng. Evidence:`reference-comparison.json` và`home-geometry-*.json`.

Đã lặp render/inspect/fix:font timer quá mảnh,safety notch rộng,status icon thiếu,
GPS caption bị cắt,child decorations chặn pointer. Đã sửa font/radius/geometry/
caption/hit testing và render lại. Đã xem HOME hai profile,side-by-side,overlays,
contact sheets,stale/disconnected/invalid,secondary/detail và missing-field cases.
`visual-qa.json` giữ log trước sửa và những giới hạn còn lại.

152 ảnh gồm90 primary(9 pages×5 states×2 profiles),50 detail và12 edge(live missing
fields,long model,independent expiry). Bounds trong parent,full text width/height
và sibling glyph overlap: **0 lỗi trong corpus**,không có nghĩa mọi chuỗi/runtime
combination đã được exhaustive test.

## 5. Những khác reference và lý do

Không có proprietary Oxbot logo,wallpaper,font hoặc firmware trong sản phẩm.
TYVOR text/original vector mark,MIT-derived font và abstract geometry thay thế.
Font vẫn khác kiểu condensed gốc;background không tái tạo bitmap gốc. Các icon
được vẽ lại,không claim logo TYVOR chi tiết từ ba brand images đã được vectorize1:1.

Values dùng nguồn có thật: **SIM** trên status row phân biệt fixtures;flight timer
SIM do caller cấp,không suy từ ARM/uptime/Core ticks. Live protocol chưa publish
flight timer,RX connectivity hoặc flight mode nên để unavailable/UNKNOWN.
Aircraft ARM luôn UNKNOWN;BENCH ARM /SIM không là aircraft armed. RF là read-only
published flag,không thay thế400mW bằng một công suất bịa.

Radio battery chưa được publish:top battery là`?`. Điện áp telemetry ở lower tile
không dùng làm radio battery. Không tính charge% từ voltage khi chưa biết chemistry/
cell count;bar không giả đầy. Trim rails không có position marker vì thiếu trim
snapshot. Bind disabled,GPS chỉ view. Những khác biệt này giữ trung thực chức năng.

Secondary pages chưa có direct UI references;không tuyên bố đã khớp ảnh Oxbot.
Exact animation,drag behavior/layout editor cần thêm captures/video,bị giữ disabled
hoặc chưa triển khai thay vì giả thành tính năng hoàn chỉnh.

## 6. Safety,cache và bridge

STM32F407VET6 CONTROL /ESP32-S3 UI /UART v2 giữ nguyên. Không sửa Core,safety,mixer,
ARM machine hoặc RF behavior. Presenter là wrapper gọi V2 đã kiểm thử trên value
copy;poll expiry theo local clock. Stale/session loss xóa active validity của timer,
model,inputs,outputs,mixes,telemetry,RF;không giữ ARM cũ như đang có hiệu lực.
Input expiry invalidate outputs;telemetry từng field hết hạn độc lập.

ESP dùng compile-include **nguyên source** legacy bridge và `xQueuePeek` value copy.
UI task priority2,bridge priority5;UI không giữ cache-owner lock. LVGL ở một owner,
DMA ISR chỉ signal completion. Draw buffer chỉ release sau synchronous completion
qua pipeline cũ;timeout/error vẫn fault/abort. UI task WDT được feed sau progress.
SDK submit/command có thể chờ ngoài colour wait250ms;không claim WCET từ timeout.

V3 không nhận Core writer/ARM/output callback. Read-only test so byte state Core
và UI endpoint trước/sau navigation. ESP ELF không link Core safety/mixer/model
writers,Wi-Fi/BT runtime hoặc unverified touch IC driver. Physical pixels có thể
giữ hình cũ khi panel/SPI treo;software freshness không bảo đảm hardware blanking.

## 7. Build/test thực tế

| Kiểm tra | Kết quả |
|---|---|
| Native Debug | **302/302 PASS**,0 fail/skip |
| Native Release/NDEBUG | **302/302 PASS**,0 fail/skip;compile commands kiểm tra cảV2/V3 |
| Existing regression |268 tests cũ không sửa;33 V3 functional/integration +1 Win32 host mới |
| Actual LVGL renderer |152 renders,0 layout errors trong corpus |
| Mouse/keyboard/swipe |Actual hidden Win32 host PASS;capture dùng cùng UI functions |
| Validity/navigation |Stale/disconnect/invalid model,field expiry,bank recovery,bounds,font,disabled bind,read-only PASS |
| Session recovery |Control reset/UI reset/both reset/loss-recovery dùng real protocol simulator PASS |
| Memory navigation |500 transitions,quay HOME,free-pool trong budget512B;không full-screen allocation mỗi update |
| ESP-IDF5.5.1/S3 |App ELF/BIN/MAP,bootloader,partition build **PASS** |
| Package/SHA/config/paths |**PASS**;offline positive +8 negative fixture checks,artifact/source hashes |
| Display/touch/physicalPSRAM/timing |**NOT RUN /UNVERIFIED** |

CHECK assertions luôn chạy cả Release;không dùng NDEBUG-disabled assert để tạo
PASS. Native exact test inventory/JUnit/compiler/source/output hashes nằm trong
3 current receipts. Dependencies839 source files vẫn khớp pinned LVGL9.2.2 và
ILI9341 candidate. Builder không mở serial hoặc flash;IDF footer chỉ in gợi ý.

Đã tái hiện lỗi header giữ tên cũ khi tên mới14 ký tự rộng và model IDUINT32_MAX
đều không fit. Model ID này không bị model validator cấm(chỉ yêu cầu nonzero).
Regression`ui3_long_model` **FAIL trước sửa(exit1)**;sau sửa fallback`MODEL` và vẫn
giữ đủ tên ở MODEL page. Chỉ sửa V3,không đổi V2/Core. Before/after logs và exit
records được giữ trong evidence;không thêm một test giả để tăng số lượng PASS.

Package checks từ chối đúng UART/display gate ON,GPIO assigned,missing image,
sai offset,path traversal,BIN corruption và CSV/binary mismatch ngay cả khi hash
fixture được cập nhật. Chỉ bản sao được sửa,không đổi package/firmware thật;9 checks
này tách riêng,không cộng vào302 CTests.

Toolchain:Clang20.1.7,Xtensa GCC14.2.0,ESP-IDF5.5.1,CMake3.30.9,Python3.11.13.
Không disable warning hoặc sửa test cũ để PASS. Export cuối yêu cầu Release receipt
khớp hiện tại;draft được lưu ởbuild riêng,không là final evidence.

## 8. Memory và performance

| Kết quả cuối | Giá trị |
|---|---|
| App BIN |**487216 bytes**,fit factory3145728 bytes |
| ELF debug /MAP |6517020 /3913544 bytes;không là Flash footprint |
| Bootloader /partition |20896 /3072 bytes |
| Flash /offsets |16MB,DIO80MHz;0 /0x8000 /0x10000 |
| Flash code /rodata+appdesc |282350 /120040 bytes |
| DIRAM linked |**175475 /341760 bytes**,51.34%;BSS107176,data11760,text56539 |
| Linked DIRAM còn lại |166285 bytes,trước runtime heap/stack allocations |
| Dedicated IRAM region |16384 bytes;không có nghĩa toàn bộ SRAM đã hết |
| LVGL arena |98304 bytes đã trong BSS |
| DMA draw buffer |20480 bytes INTERNAL/DMA/8BIT aligned64,partial,một buffer |
| UI stack |8192 bytes;legacy bridge/diagnostics4096 mỗi task |
| .su frames lớn |UI1792,diagnostics1568,presenter992,V1 projection960;không là call-chain peak |
| Numeric font Flash object |**20095 bytes**;bitmap compact7080 +large12615,metadata phần còn lại |
| Other font objects |20px21799 /14px13645 /12px11484 bytes |

BIN SHA256:`dbacb05a2e21ed8bc24e20283744470a6cf5b2741fbf847eb2ff70dd5ed3c6a7`.
SoV2:BIN giảm74896 bytes,DIRAM tăng24 bytes. Full Montserrat48 không cần link vào
V3;numeric fonts là MIT-derived normal LVGL glyphs,không screenshot text.

Native-only:đổi LQ gửi3696 pixels(4.81% full frame) ở320×240,5126(3.11%) ở448×368;
same view gửi0 pixels. HOME arena-used18936 bytes;observed peak25192 /25728 bytes.
Draw event geometry chỉ invalidate khi inputs thay đổi;tránh so padding của struct.
Trang chỉ rebuild khi navigation;field text/style updates kiểm tra thay đổi.

Các số native không là ESP heap/stack high-water,FPS,SPI throughput hoặc WCET.
Mock transfer cost deterministic. PSRAM OCT40MHz/IGNORE_NOTFOUND vẫn chỉ là config,
chưa chứng minh8MB physical PSRAM;Flash16MB cũng là target configuration.

## 9. Git diff/WIP và điều kiện tiếp theo

Baseline218 source/docs/scripts được freeze trước sửa. **217 file cũ không đổi**;
chỉ root`CMakeLists.txt` thêm `TYVOR_BUILD_UI_V3` **defaultOFF**.35 file mới:
20 portable/native files,9 ESP project/profile files,5 scripts và báo cáo này;
tổng253 source/docs/scripts. Ba input files đuôi`.png` được hash riêng trong build inputs.

Snapshot`build/ui-v3-baseline/source/`;inventory/hash/patch trong package evidence.
Git root cha vẫn báo`?? ./`;không lấy empty tracked diff làm bằng chứng không WIP.
V2/TFT/historical packages giữ nguyên và được hash/validate lại. Không sửa repo
STABLE,commit,push,deploy hoặc Git global.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-ui-v3.ps1 -Target all
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/export-ui-v3-references.ps1
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/export-ui-v3.py
& ./.tools/espressif/python_env/idf5.5_py3.11_env/Scripts/python.exe scripts/ui3_artifacts.py inspect
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/ui3_artifacts.py package
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/ui3_artifacts.py validate
```

Reference decode cần`build/ui-v3/evidence/reference/official-home.png` đã được
ghi URL/hash trong evidence. Font regeneration có script riêng và output hash.
PowerShell policy override chỉ cho child process. `START_SIMULATOR.cmd` trong
package chạy UI PC;không có flash step.

**DISPLAY/TOUCH/UART/RF gates OFF;display GPIO toàn bộ=-1.** Không flash,không mở
UART,không kiểm thử thiết bị. Board/power/pinout/ILI9341/touch/AMOLED/PSRAM/timing
vẫn UNVERIFIED. Cần hardware evidence và phê duyệt riêng trước bước phần cứng.

Visual cần review thêm:clean HOME capture,Model/Quick/Settings/monitor screenshots
đúng phiên bản và animation recordings. Đã hoàn tất source,build/test,comparison
và hồ sơ review phần mềm trong phạm vi reference hiện có;**dừng để người dùng review**.
