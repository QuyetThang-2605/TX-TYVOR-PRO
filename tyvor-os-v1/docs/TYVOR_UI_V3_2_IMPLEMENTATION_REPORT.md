# TYVOR OS V3.2 — Premium UI implementation

Ngày: **2026-11-12**. Workspace: `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**BUILD PASS · TEST PASS · VISUAL REVIEW PENDING · HARDWARE UNVERIFIED.** Đã triển khai HOME LVGL thật theo hai panel TYVOR được duyệt, dùng đúng nền người dùng và logo alpha. Không tuyên bố fidelity100% hoặc phần cứng PASS. Dừng để người dùng review.

## 1. Tiếp tục WIP và nguồn thiết kế

Đã đọc yêu cầu đính kèm, V3/V3.1 source/theme/renderer/profiles/PC simulator/build scripts, báo cáo V3.1 và Git status/diff. Không có AGENTS.md áp dụng trong project/đường dẫn cha đã kiểm tra. Trước sửa đã xác minh ba receipts V3.1 và package263file; freeze **297source/docs/scripts** tại `build/ui-v3-2-baseline/source/`. Không reset, clean, restore, undo hoặc ghi đè source V3.1. V3.2 nằm trong experiment riêng, kế thừa V3.1 và presenter V2.

Ảnh thiết kế thực tế có tên **`docs/ui-reference/tyvor-v3-2/approved-home.png.png`**. Đã mở và đọc: Oxbot trái chỉ là tham chiếu; TYVOR giữa/phải là thiết kế duyệt. Không đổi tên/ghi đè ảnh gốc. Khi bắt đầu chưa có nền; người dùng đã bổ sung `blue-wave-background.png`, sau đó đã xác minh file đọc được, kích thước617×355 và SHA-256:

```text
49549c7139f1df7bd77fec4d1dfdf081eaa5f22d5c97279810907a949ddf0b2a
```

`reference-source.json` ghi hash ảnh approved và crop giữa `[734,72,704,570]`, phải `[1456,72,702,570]`. Đây chỉ là panel so sánh; không đưa ảnh UI ba màn hoặc panel screenshot vào firmware.

## 2. HOME, background và responsive layout

HOME giữ top bar, hero GPS–timer–bind, safety pill, ARM/LQ và bốn widget dưới; không footer V2. Header dùng logo horizontal thật, model Kestrel5 trong SIM, divider, SIM/CORE, RF flag, signal và help→SETTINGS. Header compact/wide có vị trí, font và kích thước logo riêng; các trang phụ giữ dimension/row/pagination riêng như trước. Không scale một framebuffer UI hoàn chỉnh.

Nền lấy **chính xác file blue-wave**: center-cover crop theo tỉ lệ rồi HQ bicubic scale vào320×240 và448×368; không stretch, đổi hue/saturation/brightness hoặc dựng nền khác. Crop tương ứng x≈71,833,w≈473,333,h355 và x≈92,413,w≈432,174,h355 trên ảnh617×355, y0. Lượng tử RGB565 là sai số pixel-format, không phải chỉnh palette. Không thêm lớp tăng sáng hoặc dùng screenshot UI làm nền.

Hai asset RGB565 raw, const Flash: **153600+329728=483328byte**. Không decode PNG mỗi frame, không full-screen RAM copy trên ESP. LVGL clipping lấy đúng phần dirty từ cùng bitmap. Card xanh dương bán trong suốt, bo góc/viền nhẹ, timer trắng, icon outline; dial mặt phẳng/tick/knob, không metallic bevel/3D/neon glow. Input knob theo state hợp lệ, không đặt giữa chỉ để giống ảnh duyệt.

Timer vẫn là numeric font LVGL Montserrat-derived/MIT,4×4 supersampling và alpha glyph; nguồn không từ font Oxbot. Renderer dùng RGB888 partial compositing rồi pack in-place sang MSB-first RGB565 với Bayer4×4 theo tọa độ toàn màn, giữ stride active buffer. Không cấp large buffer mỗi frame; draw buffer **20480byte**, INTERNAL|DMA|8BIT/alignment64; LVGL arena **98304byte** như V3.1.

## 3. Branding và icon system

Logo thật từ `docs/ui-reference/oxbot/reference-01.png` (owl) và `reference-02.png` (horizontal); cả hai thực chất JPEG/JFIF dù đuôi.png. Source không sửa. Bóc nền trắng sang A8 alpha, loại fringe trắng rất nhạt, resize giữ tỉ lệ nguồn và tint trắng theo approved. Không vẽ lại mắt cú/wordmark, không logo AI hoặc placeholder. Khác V3.1: container trong suốt, không hộp/viền trắng. Chiều cao làm tròn tới pixel gần nhất; không stretch hai trục.

| Variant | Compact | Wide |
|---|---:|---:|
| Owl-only | 26×14 | 36×20 |
| Horizontal dưới card | 52×11 | 74×15 |
| Horizontal header | 76×16 | 112×23 |

Tổng logo pixels A8 **6558byte**. Background+logos kèm descriptors chiếm **490078byte Flash**,0 static RAM theo IDF file-size report. JPEG source vẫn giới hạn chi tiết ở kích thước nhỏ; không coi alpha conversion là khôi phục vector master.

Icon từ [Lucide chính thức](https://github.com/lucide-icons/lucide), commit **`bfe90f03d97992041b4a8007e6e3581b8e948517`**,15SVG có hash trong `assets/lucide/SOURCES.json`. Giữ đầy đủ ISC/Feather MIT notices trong `assets/lucide/LICENSE` và package licenses. Bộ icon gồm GPS, radio/signal, battery, link, settings, model/box, inputs/joystick, telemetry/activity, back, home, quick/sliders, help, ellipsis và outputs. Stroke thống nhất2/24-unit; không gradient/material/shadow trong icon.

SVG curves được flatten thành geometry cố định; generate immutable A8 masks4×4 supersampling tại20/28/34px, dùng `lv_image` components, recolor/opacity variants VALID/MUTED/WARNING/ERROR. Không SVG decoder/heap lớn runtime. Logo và icon là asset component nhỏ; timer/card/text/button vẫn là LVGL objects/state thật.

Icon A8 pixels **35100byte**; năm tick-mask diameters58/80/88/94/130 có **45064byte** A8. `icon_data.c.obj` gồm masks, geometry và descriptors **87560byte Flash**,0 static RAM. Tick masks cũng là vector-derived flat assets; knob tiếp tục vẽ theo input state. HOME và INPUTS dùng mask đúng dimension, không fallback bằng bitmap kích thước khác.

## 4. Data integrity, safety và kiến trúc

Giữ STM32F407VET6 Main Controller, ESP32-S3 N16R8 UI Controller; không sửa STM32 Core, Mixer, Safety Engine, ARM/RF logic hoặc protocol v2. ESP app kế thừa bridge single-owner/value-copy publication; UI chỉ đọc state. Navigation/help/fixture không có API ARM, RF, motor hoặc outputs writer.

LQ/voltage/GPS lấy presenter và TTL state; không hardcode98% như telemetry thật. Demo98% chỉ xuất hiện khi fixture SIM hợp lệ. GPS hết hạn được mute và hiện`GPS --`; LQ hết hạn là`--%`; STALE/DISCONNECTED bỏ timer và số liệu hết hạn. Bind disabled/N/A. Unknown battery charge vẫn`--%`; không dựng charge bar đầy, RF400mW hay radio-battery giá trị giả.

SAFE/DISARMED trên SIM là trạng thái **Core bench**, không xác nhận aircraft ARM. Dòng **ARM UNKNOWN** vẫn hiện rõ. STARTUP/FAILSAFE lấy nhãn safety cụ thể đã công bố, không suy từ boolean`safe`. Live chưa có flight-timer/receiver/flight-mode authoritative thì tiếp tục unavailable. Các trạng thái bench ARM giữ SIM/read-only.

DISPLAY/TOUCH/UART/RF gates **OFF**, board/module verified flagsOFF,9GPIO display/UART chưa xác minh đều **-1**. Runtime config/BSP cũ và AMOLED cũ không đổi. Firmware default TFT320×240 rotation1 dùng backend MOCK;448×368 chỉ là layout phần mềm, chưa có AMOLED driver hoặc physical rotation xác minh. Không flash, mở UART, RF, sửa Git global, commit, push, deploy hoặc sửa repo STABLE/EdgeTX.

## 5. Lỗi có bằng chứng và sửa trong phạm vi UI

| Vấn đề | Trước sửa | Sửa / sau sửa |
|---|---|---|
| Nhãn safety HOME kế thừa | `ui32_safety_labels` exit1: FAILSAFE bị trình bày SAFE/DISARMED vì`base.safe` chỉ có nghĩa không BENCH_ARMED. | Dùng`v.safety`, giữ STARTUP/FAILSAFE, màu warning/danger đúng. Cùng regression PASS. Không đổi safety state hoặc Core. |
| Tick lines partial redraw | `ui32_input_bounds` FAIL:1pixel x259,y167 khác dù hai giá trị input clamp về cùng vị trí; log old1251/new2af3. | Static A8 tick cache thay raster line clipping theo dirty strip. Giữ assertion byte-identical; test PASS. Đây là lỗi trình bày UI, không sửa thư viện LVGL. |
| Tick cache không đủ size INPUTS | Visual contact sheet có tick đứt/bleed;5ảnh448 INPUTS khác hash Debug/Release. `ui32_dial_mask_fit` exit1 vì selection fallback94px cho vùng80/131px. | Đủ5mask size, normalize diameter chẵn, lookup strict; không dùng source/area khác kích thước. Regression PASS và toàn bộ152ảnh Debug/Release khớp. |

Evidence trong `regression-before.json`/`regression-after.json`, `input-redraw-before.json`/`input-redraw-after.json`, `mask-before-fix.json`, `mask-fit-before.json`/`mask-fit-after.json`, snapshots source trước sửa và `render-consistency.json`. Bằng chứng ban đầu giữ nguyên để review.

Một probe TTL ban đầu chỉ expire battery/GPS nhưng assertion lại đòi LQ hết hạn; đó là **lỗi fixture**, không phải bug telemetry. Đã sửa fixture để expire thêm index1=LQ, ghi rõ trong`regression-before.json`; không nhận probe sai làm bằng chứng product bug. Test cuối kiểm cả số liệu hết hạn và opacity GPS. Không nới CRC/validation hoặc đổi expected chỉ để PASS.

## 6. Build và test cuối

| Hạng mục | Kết quả cuối | Evidence trong package |
|---|---|---|
| Native Debug | **393/393 PASS**,0failure/skip, không NDEBUG | `native-debug.xml`, `native-debug-receipt.json` |
| Native Release/NDEBUG | **393/393 PASS**,0failure/skip; checks luôn chạy | `native-release.xml`, `native-release-receipt.json`, compile-command hashes |
| ESP32-S3 | **BUILD PASS**, app `0.5.2-ui-v3-2`, ELF/BIN/MAP + bootloader/partition | `esp32-receipt.json`, `esp32-build.txt`, `memory-report.json` |
| PC simulator | Actual LVGL mouse/keyboard/swipe hidden host PASS,320 và448 | `packaged-simulator-smoke.json`, `simulator-preview.png` |
| RGB565 / stride / bounds | Primaries, padded stride,20KiB limits, glyph alpha và dither mean PASS | Quality tests trong hai JUnit files |
| Render consistency | **152/152 PNG byte-identical Debug vs Release** | `render-consistency.json` |
| Layout |152final Release screenshots,0detected object/text layout errors | `render-validation.json`, `render-console.txt` |
| Offline package | Valid file inventory/SHA, offsets/image hashes/partition/config; positive+8negative fixtures PASS | `SHA256_MANIFEST.json`, `package-negative-checks.json` |

393 = **345regression cũ giữ nguyên +34frontend V3.2 +14quality tests V3.2**. Trong14quality:9nhóm từ V3.1 (logo alpha tests được thích nghi đúng định dạng mới; RGB565/stride/bounds/font/partial/dither assertions giữ tính chất), thêm background assets, icon states, safety labels, HOME validity và dial-mask fit. Partial invariance gồm đổi/khôi phục LQ, gimbal rồi fullinvalidate, cả hai profile.

Assertions dùng CHECK luôn hoạt động; helper kiểm inventory chính xác và NDEBUG trong compile commands. Không cộng152ảnh, package fixtures hay host smoke lặp vào393. Lượt Debug đầu có1FAIL trước sửa tick; lượt trung gian392PASS vẫn phát hiện lỗi hình INPUTS bằng đối chiếu thực tế. Chỉ số cuối ở trên thuộc source/artifacts đã sửa đầy đủ và full build exit0.

Native Clang20.1.7; ESP Xtensa GCC14.2.0; ESP-IDF5.5.1; LVGL9.2.2/ILI9341component2.1.0.839dependency source hashes khớp lock. `build/ui-v3-2/final-build-console.txt` ghi Project build complete và receipts; IDF in gợi ý flash nhưng **không chạy flash command**.

## 7. Memory, Flash và timing

| Mục | Byte / kết quả |
|---|---:|
| App BIN | **1067856** |
| ELF / MAP | 7146136 / 3941648 |
| Bootloader / partition BIN | 20896 / 3072 |
| Factory app partition | 3145728; còn2077872byte |
| DIRAM static | **175483/341760 (51,35%)**, còn166277 |
| DIRAM data / BSS / text | 11760 / 107184 / 56539 |
| Dedicated IRAM vectors+text | 1028+15356=16384 |
| Flash code / rodata / app descriptor | 285318 / 697448 / 256 |
| LVGL arena / runtime draw buffer | 98304 đã trong BSS /20480 cấp một lần |
| UI / bridge / diagnostics stack config | 8192 /4096 /4096 |
| Native HOME arena used, cả hai profile | 16240 |
| Native arena peak320 /448 trong corpus | 20768 /30552 |
| LQ-only redraw320 /448 | 3696/76800px (4,81%) /5126/164864px (3,11%) |
| Idle redraw | 0pixel cả hai profile |

So V3.1: app tăng570944byte chủ yếu vì background/masks trong Flash; static DIRAM không tăng. Native full framebuffer chỉ dành PC, không có trong ESP. Section-based IDF total1067705 khác padded BIN1067856; không lấy kích thước ELF có debug symbols làm Flash usage. Dedicated IRAM đầy vùng linker16384 không đồng nghĩa toàn bộ SRAM hết.

Compiler `.su` lớn nhất UI1792, diagnostics1568, V2project992, TFTproject960byte; đây là single-function frames, không phải call-chain peak hay high-water board. Dynamic heap/task stacks/draw buffer phải kiểm tra trên board sau; không suy PSRAM8MB hardware PASS từ config OCT/16MB Flash. `SPIRAM_IGNORE_NOTFOUND` vẫn bật cho experiment; không phụ thuộc một full-screen PSRAM framebuffer.

`native_renderer_wall_seconds` trong render evidence đo PC process gồm layout/navigation và ghi152PPM; không suy FPS/SPI timing từ số này. Counter`frame_us` native dùng clock giả n/8+17, chỉ để test deterministic instrumentation. ESP đã có esp_timer/heap/stack logging nhưng **chưa đo hardware**. UART bridge/task architecture không đổi; gatesOFF và backend MOCK nên chưa chứng minh arbitration/bandwidth khi display thật chạy.

## 8. Visual QA và khác biệt còn lại

Đã đọc approved gốc, xem HOME320/448, approved comparison, V3.1/V3.2, state sheets và contact sheet; sau lỗi INPUTS đã xem lại capture đã sửa và kiểm toàn bộ152ảnh Debug/Release. Corpus gồm90primary (9trang×5state×2profile),50detail,12edge. Các trang MODEL/INPUTS/MIXES/OUTPUTS/TELEMETRY/SETTINGS/QUICK và CONNECTIONS kế thừa logic/navigation read-only; shared palette/icons/background đồng bộ. Không triển khai tính năng mới ngoài UI.

- [HOME320×240](../artifacts/tyvor-ui-v3-2/home-320x240.png)
- [HOME448×368](../artifacts/tyvor-ui-v3-2/home-448x368.png)
- [Approved vs actual LVGL](../artifacts/tyvor-ui-v3-2/approved-vs-lvgl.png)
- [V3.1 vs V3.2](../artifacts/tyvor-ui-v3-2/v3-1-vs-v3-2.png)
- [Contact sheet](../artifacts/tyvor-ui-v3-2/all-pages-contact-sheet.png)
- [Visual differences](../artifacts/tyvor-ui-v3-2/visual-differences.md)

Native PNG không resize/filter. Approved comparison: approved TYVOR trái, LVGL phải; dòng trên320,dưới448, uniform bilinear contain400×328 và letterbox. V3.1/V3.2 đặt native pixels cạnh nhau. Python chỉ đổi framebuffer/ghép ảnh, không vẽ UI thay LVGL. Build thành công,0layout errors hoặc hash khớp giữa compiler modes không chứng minh visual fidelity với thiết kế duyệt.

Khác biệt còn thấy: typography regular/độ đậm, timer shape/proportion, hero/card tối và phẳng hơn approved glass, icon outline khác filled/bold glyph, tick density thấp hơn ở compact, knob lệch tâm theo mock input hợp lệ, charge percent unknown khác màu, crop nền và RGB565 grain/banding. Help target24px/30px chưa đủ để mặc nhiên coi là target touch vật lý44px; touch vẫn OFF và cần layout/hit-area review riêng khi có driver/panel xác minh. Không làm giả data hoặc thêm bevel/3D để bù khác biệt.

## 9. Artifacts, SHA-256 và tái lập

Package `artifacts/tyvor-ui-v3-2/` gồm các ảnh yêu cầu, gallery, RUN_SIMULATOR/launcher, Debug/Release executables, ESP ELF/BIN/MAP, bootloader/partition/flash metadata, evidence/JUnit/receipts/patch/memory, reference review assets và licenses. `SHA256_MANIFEST.json` ghi inventory/hash/size chính xác và source catalog. Validator kiểm mọi đường dẫn, config gates và partition CSV/binary; không mở serial. Offset bootloader0x0, partition0x8000, app0x10000; DIO80MHz/16MB là image config, không physical verification.

App BIN SHA-256:

```text
6181b03ee024fa5fdbb88c8cdf97bdc26fb14962052b970df2d2250a6e9b7e8e
```

ELF SHA-256:

```text
b13fcd61cd624814106e28bb46a0ccdea999283a94a029f5867f6f288202e716
```

Recheck bằng local Python, không rebuild khi receipts còn current:

```powershell
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/ui32_artifacts.py verify
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/ui32_artifacts.py validate
```

Full build khi compile inputs thay đổi: child PowerShell `-NoProfile -ExecutionPolicy Bypass -File scripts/build-ui-v3-2.ps1 -Target all`. Export bằng`extract-ui-v3-2-reference.ps1` rồi`export-ui-v3-2.py`; asset generators/fetcher nằm riêng, source/hash/commit được ghi evidence. Fetcher `--verify-only` chỉ xác minh SVG/license local, không tải lại. Đóng gói bằng`ui32_artifacts.py package` sau inspect và final Release render; helper từ chối draft/stale screenshots. Simulator launcher chỉ mở UI PC, không flash.

## 10. Git diff/WIP, rủi ro và điều kiện tiếp theo

Baseline297file: **296file byte-identical**, chỉ root`CMakeLists.txt` thêm option`TYVOR_BUILD_UI_V32` defaultOFF và dependencyV31. **66file mới**, tổng**363source/docs/scripts/vendor text files** trong catalog; PNG nguồn được pin riêng trong build receipts. New files thuộc`experiments/tyvor_ui_v3_2/`,9file ESP project,9script và báo cáo này. Danh sách/hash/fullpatch chính xác tại`evidence/source-changes.json`, `ui-v3-2-diff.patch`, `preservation.json`.

Git root cha vẫn báo`?? ./`; tracked diff trống vì project untracked, không có nghĩa không có WIP. V3.1 package/artifacts không đổi; receipts V3.1 là lịch sử theo root CMake cũ, còn345regressions của nó và các phase cũ đã chạy mới trong V3.2. Các9historical manifests/artifact hashes và runtimeconfig/BSP cũ được đối chiếu lại. Không có Git write hoặc STABLE modifications.

Không còn test/build FAIL ở trạng thái cuối. Vấn đề còn mở: visual acceptance của người dùng; khác biệt trình bày liệt kê trên; dữ liệu receiver/aircraft/timer/charge chưa authoritative thì giữ unavailable; JPEG logo không phải vector master; corpus hữu hạn không bao quát mọi text/sequence tương lai. Chưa chứng minh physical display, touch, Flash/PSRAM, heap/stack hoặc timing/băng thông UART khi phần cứng chạy.

**Đủ hồ sơ cho review PC, chưa cho phép tự flash/hardware bring-up.** Cần người dùng duyệt ảnh/simulator, schematic/PCB/pin/nguồn/controller phù hợp, hardware evidence/backup/recovery và phê duyệt riêng trước flash hoặc mở gate. Giữ hai MCU độc lập và mọi gateOFF trong lần bàn giao này.
