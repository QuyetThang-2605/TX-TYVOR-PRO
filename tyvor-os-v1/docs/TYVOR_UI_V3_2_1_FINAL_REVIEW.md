# TYVOR OS V3.2.1 — Final visual polish

Ngày: **2026-11-12**. Workspace: `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**BUILD PASS · TEST PASS · VISUAL REVIEW PENDING · HARDWARE UNVERIFIED.** Hoàn thành code/test/render/package cho review; không tự chấm Visual PASS và không flash phần cứng.

## 1. Bảo toàn WIP và reference

Đã đọc source/renderer/theme/profiles/build scripts, báo cáo V3.2, ảnh approved, comparison và contact sheet. Tìm AGENTS.md trong project và thư mục cha: không có instruction file áp dụng. Git status vẫn`?? ./`, tracked diff trống vì project chưa tracked; không coi đó là không có WIP.

Xác minh ba receipts và package V3.2 trước sửa; freeze **364source/vendor/docs/scripts** vào`build/ui-v3-2-1-baseline/source/`. **363file cũ byte-identical**, chỉ root CMake thêm option`TYVOR_BUILD_UI_V321` defaultOFF, yêu cầu V32. Không reset/restore/clean/undo hoặc sửa các experiment cũ. V3.2.1 là nhánh experiment riêng, kế thừa behavior/state từ V3.2.

Đúng asset nguồn: `docs/ui-reference/tyvor-v3-2/approved-home.png.png` và `blue-wave-background.png`. Đã đọc cả hai, giữ byte gốc. Background SHA-256:

```text
49549c7139f1df7bd77fec4d1dfdf081eaa5f22d5c97279810907a949ddf0b2a
```

Không crop lại, đổi palette/hue/saturation hoặc thay nền mới. Dùng nguyên hai raw background320/448 và alpha branding từ V3.2, chỉ đổi namespace descriptors. Pixel bytes được đối chiếu với baseline; ảnh approved chỉ dùng ở review, không phải UI framebuffer firmware. V3.2 report còn inventory66/363; baseline/manifest thực tế364 là số dùng cho lần này, không sửa ngược báo cáo cũ.

## 2. Các tinh chỉnh thực tế

| Phần | Thay đổi và lý do |
|---|---|
| Timer | V3.2 phóng bitmap Montserrat48, non-uniform scaling và dilation làm mềm nét. Thay bằng coverage raster trực tiếp từ outline **Montserrat Bold**, em52compact/em74wide,4×4 supersampling→LVGL4bpp; không blur, fake bold hoặc runtime scaling. |
| Card | Hero opacity180→212, border35→65; footer185→214, border28→55, radius21→18 canonical units. Giữ màu xanh cũ, giảm nhiễu sóng phía sau chữ; không thêm shadow/neon/3D. |
| Gimbal | Tâm tick cache khớp face/knob tại half-pixel, khoảng chia góc đều. Count48/64/72/76/104 theo diameter58/80/88/94/130. Mask là staticA8; dot và validity vẫn từ state, không đặt tâm giả. |
| Header | Giữ logo alpha; thu hẹp RF allocation và tách signal về x270compact/x378wide; model/SIM/CORE cùng baseline, kiểm glyph/image không chồng. |
| Safety | Nếu snapshot Core hợp lệ và DISARMED, trình bày **SIM GATE SAFE** hoặc **CORE GATE SAFE**; không dùng chữ DISARMED làm khẳng định aircraft ARM. Dòng **ARM UNKNOWN** giữ riêng. STARTUP/FAILSAFE/INVALID/STALE/DISCONNECTED vẫn rõ. |
| Trang phụ | Telemetry/Settings row opacity216, value font14 tại320; Quick Settings icon/label thành nhóm giữa button, gap8px, wide label14. Không đổi logic/navigation hoặc mở UNAVAILABLE. |
| Diagnostics | Optional synchronous tap trước RGB565 packing, ghi RGB888 PC thật. ESP không đăng ký callback và không cấp full-frame RGB888 buffer. QPC benchmark native riêng. |

Source chính: `experiments/tyvor_ui_v3_2_1/ui.c`, `ui.h`, `ui_pages.c`, `ui_widgets.c`, `ui_shapes.c`, `icon_data.*`, `timer_font.*`, `native/mock.*`, `render.c`, `benchmark.c`, `test_quality.c`; ESP project riêng tại`esp32/experiments/tyvor_ui_v3_2_1/`; build/export/asset scripts mới. Danh sách từng file/hash/fullpatch nằm trong package`evidence/source-changes.json`, `ui-v3-2-1-diff.patch`, `preservation.json`.

## 3. Font, logo và background memory

Font gốc từ [Montserrat project](https://github.com/JulietaUla/Montserrat), commit **555facfb2a18c72c3c0380f0d9c0f060453a9058**; pinTTF/OFL/hash ở`assets/font/SOURCES.json`. GDI PrivateFontCollection đọc file riêng, không cài font Windows/global. Dùng Bold face gốc; 4×4 hình học→alpha coverage native-size, không xử lý Gaussian hoặc phóng bitmap. Font subset đổi tên TYVOR, giữ SIL OFL notices. SemiBold được thử rồi chọn Bold theo đối chiếu, source trial giữ lại nhưng không link TTF vào app.

Raw timer bitmaps7436+14469=**21905byte**, object**22305byte Flash**, không runtime FreeType hoặc font heap. Prototype em56/80 ban đầu vượt allocation160/224; đã hạ52/74 và thêm regression00:00/99:59/--:-- cả hai profile. Không sửa assertion để bỏ qua clipping.

Logo owl/horizontal giữ đúng A8 pixels, tỉ lệ và white tint V3.2; không vẽ lại logo. Background **483328byte RGB565**, logo **6558byte A8**; combined object **490078byte Flash**, không full-screen RAM copy. Lucide/tick object **87560byte Flash**. Giữ Lucide ISC/Feather MIT và Montserrat OFL trong package licenses.

## 4. RGB888 và RGB565 — bằng chứng riêng

LVGL9.2.2 compositing RGB888 BGR-order vào20KiB buffer. Observer chép pixels **trước** in-place pack theo stride thật. RGB565 dùng Bayer4×4 tọa độ tuyệt đối, MSB-first như trước. `ui321_capture_pipeline` kiểm mọi pixel ở320/448: pack(raw RGB888)==RGB565 framebuffer. Không tạo RGB888 bằng up-convert565.

Các file native:

- [HOME320 RGB565](../artifacts/tyvor-ui-v3-2-1/home-320x240.png), [HOME448 RGB565](../artifacts/tyvor-ui-v3-2-1/home-448x368.png).
- [HOME320 RGB888](../artifacts/tyvor-ui-v3-2-1/home-rgb888-320x240.png), [HOME448 RGB888](../artifacts/tyvor-ui-v3-2-1/home-rgb888-448x368.png).
- [RGB888 vs RGB565](../artifacts/tyvor-ui-v3-2-1/rgb888-vs-rgb565.png).

Background nguồn đã là RGB565 asset giữ nguyên; đường RGB888 không khôi phục chi tiết24bit đã lượng tử của background. Hai output khác ở compositing/quantization cuối; không đánh đồng raw888 với ảnh source PNG nguyên bản.

## 5. Build/test/render thực tế

| Hạng mục | Kết quả cuối | Evidence |
|---|---|---|
| Native Debug | **445/445 PASS**,0fail/skip | `native-debug.xml`, `native-debug-receipt.json` |
| Native Release/NDEBUG | **445/445 PASS**,0fail/skip; CHECK luôn chạy | `native-release.xml`, `native-release-receipt.json`; compile-command hashes |
| ESP32-S3 | **BUILD PASS**, app`0.5.3-ui-v3-2-1`, ELF/BIN/MAP, bootloader/partition | `esp32-receipt.json`, `esp32-build.txt`, `memory-report.json` |
| RGB565 / bounds / stride | Primaries, row padding,20KiB limits, alpha, dither và partial/full invariance PASS | Quality cases trong hai JUnit files |
| Regression cũ | Giữ nguyên **393tests** từ các phase/V3.2, đều PASS | Exact inventory validator |
| Native PC host | Mouse/keyboard/swipe hidden-host PASS, packaged exe320/448 | `packaged-simulator-smoke.json`, `simulator-preview.png` |
| Render consistency | **154/154 PNG Debug/Release hash-identical** | `render-consistency.json` |
| Layout |152RGB565screens+2raw888HOME,0detected layout errors | `render-validation.json`, `render-console.txt` |
| Offline package | Paths/image CRC+SHA/ELF/partition/profiles/config PASS; positive+8negative fixtures PASS | `SHA256_MANIFEST.json`, `package-negative-checks.json` |

445=393cũ+34frontend V3.2.1+18quality. Bốn case mới: `ui321_capture_pipeline`, `ui321_safety_semantics`, `ui321_quick_group`, `ui321_font_fit`; các kiểm tra cũ không bị yếu đi. Không cộng ảnh/benchmark/package fixtures vào445. Native Clang20.1.7; ESP Xtensa GCC14.2.0/ESP-IDF5.5.1; LVGL9.2.2/ILI9341component2.1.0.839dependency source hashes khớp lock.

Full build cuối exit0, ba receipts được kiểm hash source/compiler/output hiện tại. `build/ui-v3-2-1/final-build-console.txt` có Project build complete. IDF in gợi ý flash, không thực thi flash. Không build/flash STM32 mới cho thẩm mỹ; nguồn STM32 không đổi và regression Core vẫn chạy native.

## 6. Visual QA và phép scale thống nhất

Đã xem native HOME320/448, approved gốc, font crops, RGB888/565, Settings/Telemetry và contact sheets. Không có clipping/overflow phát hiện trong corpus152screens (90primary,50detail,12edge); không coi corpus hữu hạn là mọi runtime sequence đều đúng.

- [Approved vs V3.2.1](../artifacts/tyvor-ui-v3-2-1/approved-vs-v3-2-1.png): approved TYVOR trái, actual LVGL565 phải; compact trên, wide dưới. Cả hai **cùng uniform bilinear contain400×328**, letterbox, không stretch.
- [V3.2 vs V3.2.1](../artifacts/tyvor-ui-v3-2-1/v3-2-vs-v3-2-1.png): cùng native profile, không filter.
- [Timer comparison](../artifacts/tyvor-ui-v3-2-1/timer-font-comparison.png): approved/V3.2/V3.2.1, glyph crop cùng uniform bilinear contain300×100. Không dùng nearest cho một bên rồi nội suy cho bên khác.
- [Contact sheet](../artifacts/tyvor-ui-v3-2-1/all-pages-contact-sheet.png), [visual review](../artifacts/tyvor-ui-v3-2-1/visual-review.md).

Native PNG không resize/blur. Python chỉ export framebuffer và ghép comparison; UI/widgets render bằng LVGL thật. Reference/UI full screenshot không được nhúng làm firmware UI. Nhận xét: timer sắc hơn V3.2, weight gần hơn, border/card contrast rõ hơn, gimbal chia đều hơn, Quick grouping và row text dễ đọc hơn. Đây là đánh giá nội bộ có đối chiếu ảnh, **không phải user Visual PASS**.

Khác mẫu còn: font thực của approved chưa biết, Montserrat0 rộng/tròn hơn; glass/card sheen/typography weight chưa trùng; icon/tick/knob vẫn flat; crop nền đúng nguồn nhưng màu nhìn trên panel chưa đo. Nhãn gate khác SAFE/DISARMED theo yêu cầu an toàn. Chi tiết12px ở mix rows và help24/30px còn là giới hạn compact/touch; không mở touch hoặc UNAVAILABLE để bù. RGB565 có grain/banding tại pixel zoom.

## 7. Memory / Flash / runtime heap estimate

| Mục | Byte / kết quả |
|---|---:|
| App BIN | **1070208** |
| ELF / MAP | 7146908 /3941990 |
| Bootloader / partition BIN | 20896 /3072 |
| Factory app partition | 3145728; còn2075520byte |
| DIRAM static | **175491/341760 (51,35%)**, còn166269 |
| DIRAM data / BSS / text | 11760 /107192 /56539 |
| Dedicated IRAM vectors+text | 1028+15356=16384 |
| Flash code / rodata / app descriptor | 285430 /699688 /256 |
| LVGL arena, đã trong BSS | 98304 |
| Draw buffer runtime, cấp một lần | 20480 |
| App task stacks UI/bridge/diagnostics | 8192 /4096 /4096 |
| Known app heap requests, lower bound | **36864** =draw buffer+3task stacks; chưa tínhTCB/queues/SDK |
| Native HOME arena used / peak320 /peak448 | 16240 /20744 /30552 |
| LQ-only redraw320 /448 | 3696/76800px (4,81%) /5126/164864px (3,11%) |
| Idle redraw | 0pixel |

So V3.2: app+2352byte, staticDIRAM+8byte cho observer pointers; callback NULL trên ESP. Chỉ PC mock có fullRGB888 framebuffer, không có trong ESP target. Immutable background/font/icon không cấp large memory mỗi frame. Section metric1070057 khác paddedBIN1070208; ELF debug-file size không phải Flash usage.

Heap estimate là lower bound, không phải measured free heap: SDK allocations, TCB/queues, fragmentation và task call chains cần board diagnostics. Không trừ linker slack rồi coi là free heap thực. `.su` UI1792/diagnostics1568/project992byte là individual frames, không stack high-water. Flash16MB/OCT PSRAM config không chứng minh physical Flash16MB/PSRAM8MB; IGNORE_NOTFOUND vẫn bật như trước.

## 8. Native performance, không phải hardware FPS

Windows QPC benchmark Release,240changing-gimbal redraws/profile, RGB565 memory sink, observerOFF, không window presentation hoặc I/O:

| Profile | Thời gian240frames | Unpaced redraw throughput | Trung bình/frame |
|---|---:|---:|---:|
| 320×240 | 0,012196s | 19679,07redraw/s | ≈50,82µs |
| 448×368 | 0,032004s | 7499,16redraw/s | ≈133,35µs |

Đây **không phải FPS monitor, ESP, SPI hoặc panel**, không có frame pacing; không đem so trực tiếp với display30/60Hz.240idle frames sau đó không redrawpixel. `native-benchmark.json` có điều kiện đo/rawstdout; `frame_us` mock cũ dùngclock giả n/8+17 chỉ phục vụ deterministic tests. Visible-window presentFPS và hardwareFPS **NOT MEASURED**.

UART bridge/task ownership không đổi. Những số native này không chứng minh scheduling/bandwidth khi SPI/UART thật chạy. ESP vẫn có timing/heap/stack instrumentation cho bước board sau; hardware gatesOFF lần này.

## 9. Package, SHA-256 và WIP

Artifact: `artifacts/tyvor-ui-v3-2-1/`. Manifest ghi hash/size mọi image/binary/exe/config/evidence/license và current source catalog. Include raw888 riêng,565aliases, report visual, PC launcher, ESP images, JUnit/receipts, memory, consistency, benchmark, preservation/fullpatch. Package validation offline kiểm offset boot0x0, partition0x8000, app0x10000, factory3MB, chipESP32-S3/DIO80MHz/16MB, CSV/binary match và gates/pins. Không mở serial.

App SHA-256:

```text
edeffe0ab395057cf0215c0574ecd38b62a68e977c5bad15ecb05fa08607e797
```

ELF SHA-256:

```text
6a45821356e056134e5b21994d3c4b6011ebcd976ab9b4112e9801e0e519fcd8
```

Root Git vẫn untracked WIP; không commit/push/deploy. **70file mới**, tổng**434source/vendor/docs/scripts**, gồm binaryTTF chỉ để generate. Hash inventory/snapshot thể hiện thay đổi đầy đủ thay cho tracked diff trống. Các historical packages, original PNG/logo/blue palette/background pixels và runtimeconfig/BSP cũ được đối chiếu lại. Không sửa TYVOR STABLE/EdgeTX hoặc Git global.

Recheck không rebuild khi receipts current:

```powershell
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/ui321_artifacts.py verify
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/ui321_artifacts.py validate
```

Nếu compile inputs đổi: child PowerShell `-NoProfile -ExecutionPolicy Bypass -File scripts/build-ui-v3-2-1.ps1 -Target all`; sau đó inspect, export bằng`export-ui-v3-2-1.py`, package. Font generation riêng`generate-ui-v3-2-1-font.ps1`; nguồn/pinned notices nằm trong experiment; không cài system font. Package launcher chỉ chạy simulator.

## 10. Kết luận và điều kiện còn lại

Không còn test/build FAIL trong source cuối; firmware/image/package validation là software-only. **Visual review pending**, khác biệt được ghi rõ thay vì tự chấm100%. Điều kiện phần cứng vẫn thiếu: đúng board revision/power/pinout/controller, touch/AMOLED driver, Flash/PSRAM detection, heap/stack và timing measurements, backup/recovery cùng phê duyệt flash riêng.

Giữ STM32F407VET6 Core, ESP32-S3 N16R8, protocolv2, Mixer và ARM/RF safety logic. UI gate label chỉ là Core interlock; aircraft ARM telemetry vẫn UNKNOWN. DISPLAY/TOUCH/UART/RF gatesOFF, unverified GPIO=-1. Không flash/commit/push/deploy hoặc thay đổi firmware STABLE. **Dừng để người dùng review native images và simulator.**
