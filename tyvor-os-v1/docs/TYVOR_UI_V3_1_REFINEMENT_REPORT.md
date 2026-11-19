# TYVOR OS V3.1 — HOME refinement report

Ngày hoàn thiện: **2026-11-12**. Workspace: `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Kết luận: phần mềm đã build/test và đủ hồ sơ để review HOME. Visual acceptance chờ người dùng; hardware UNVERIFIED.** Không coi build/render thành công là Visual PASS. Không flash, mở GPIO/UART, RF, commit, push, deploy hoặc sửa Git global.

## 1. Khôi phục sau gián đoạn

Tiếp tục WIP hiện có; không reset/clean/restore/undo, không xóa hoặc tạo lại V3.1. Kiểm tra workspace, tìm AGENTS.md trong project và các thư mục cha: không có instruction file áp dụng. Đã đọc Git status/diff, README V3.1, nguồn renderer/ESP, regression tests, build log, receipts và artifact.

`build/ui-v3-1/final-build-console.txt` kết thúc bằng `Project build complete`, receipt ESP32 và thông báo build/test-only. Lệnh full build trước gián đoạn đã trả exit0. Trong lần tiếp tục, ba receipts được kiểm tra lại với hash source/build output/compiler và kết quả JUnit hiện tại; đều hợp lệ. Các dòng `System.Management.Automation.RemoteException` xen giữa ghi chú stderr của IDF trong transcript PowerShell không phải kết luận build FAIL: image identity, ELF/BIN/MAP và receipt khớp đầy đủ.

Đã hoàn thành trước gián đoạn: source LVGL V3.1, logo thật, font, dithering, quality tests, Debug/Release và ESP build; phần README gần nhất đã có nội dung V3.1. Phần hoàn thiện sau khôi phục: sửa nhãn gallery/metadata còn ghi V3, chạy lại exporter Release/Win32 smoke để tạo bộ ảnh cuối, đóng gói evidence/manifest và viết báo cáo. **Không cần sửa lại firmware hoặc chạy lại toàn bộ những build/tests còn hợp lệ.**

## 2. Phạm vi và các file thay đổi

V3 được giữ làm baseline; V3.1 là experiment riêng. HOME được ưu tiên; không mở rộng V4 hoặc viết lại các trang còn lại.

| Nhóm | File / thay đổi |
|---|---|
| Build opt-in | `CMakeLists.txt`: thêm `TYVOR_BUILD_UI_V31`, defaultOFF, yêu cầu V3 dependencies. |
| HOME / components | `experiments/tyvor_ui_v3_1/ui_pages.c`, `ui_shapes.c`, `ui_theme.*`, `ui_widgets.*`: hero/card, gradient, dial, typography và nhãn HOME. |
| Renderer | `ui.c`, `render_quality.*`, `lv_conf.h`: RGB888 compositing → RGB565 dithered, partial regions, stride và bounds. |
| Branding / font | `branding.*`, `brand_assets.*`, `timer_font.*`: logo nguồn thật và font số AA. |
| State / PC | `ui_view.*`, `native/mock.*`, `native/render.c`, `native/simulator_win32.c`, `native/test_ui.c`, `native/test_quality.c`: read-only presenter, actual LVGL rendering và kiểm thử. |
| ESP project riêng | `esp32/experiments/tyvor_ui_v3_1/`: 9 file CMake/components/main/config/profiles/partitions, app `0.5.1-ui-v3-1`. |
| Công cụ / tài liệu | 7 script `*-ui-v3-1*`/`ui31_artifacts.py`, README V3.1 và báo cáo này. |

Danh sách chính xác từng file, SHA và unified patch: `artifacts/tyvor-ui-v3-1/evidence/source-changes.json` và `ui-v3-1-diff.patch`. Tổng **44 file mới**, một file baseline sửa; **297 source/docs/scripts** trong catalog. Artifact ảnh/binary nằm ngoài số này.

## 3. Những điểm HOME đã sửa

Giữ bố cục Oxbot: top model/status, hero với GPS–timer–bind, safety pill, hai dial và hai card dưới. Thay nền đa giác phẳng nổi bật của V3 bằng gradient xanh đen–xanh ngọc, các light pool và lớp hình học mờ. Card có gradient nhẹ, viền1px và bo góc/padding nhất quán. Dial bớt vòng đồng tâm, thêm mặt gradient, rim mỏng, tick và knob nhỏ hơn theo dữ liệu input.

Timer00:00 sáng hơn; font số dùng4×4 supersampling thay2×2, glyph alpha qua API LVGL thật. Bỏ nhãn debug dư ở giữa/dưới HOME; vẫn giữ SIM, SAFE/DISARMED, aircraft ARM UNKNOWN và RF OFF để không làm fixture trông như trạng thái bay thật. Battery voltage/LQ/model dùng state; charge percent, radio battery, trim và bind chưa có nguồn xác thực nên vẫn unknown/disabled.

UI state tiếp tục delegate presenter/freshness V2; ARM/outputs không có command writer. STM32F407VET6 Core, Mixer, Safety Engine, ARM logic, protocol v2 và cấu hình AMOLED cũ không đổi. WeAct ESP32-S3 N16R8 vẫn là Display Controller.

## 4. Logo TYVOR và font

Đã xem cả ba ảnh trong `docs/ui-reference/oxbot`. Tuy đuôi`.png`, cả ba là JPEG/JFIF theo nội dung; decoder đọc theo nội dung, không sửa file gốc. Dùng source01 cho owl và source02 cho horizontal; source03 chỉ kiểm tra, không đưa vào firmware. Không dùng logo Oxbot hoặc chữ placeholder.

| Asset | Crop nguồn x,y,w,h | Kích thước compact / large |
|---|---|---|
| Owl từ `reference-01.png` | 83,26,260,144 | 26×14 / 36×20 |
| Horizontal từ `reference-02.png` | 6,11,218,45 | 52×11 / 74×15 |

HQ bicubic resize theo tỉ lệ nguồn, chiều cao làm tròn tới pixel gần nhất; không stretch độc lập hai trục, không recolor/redraw. Giữ nền trắng của logo và padding2px, không cắt horn/corner. Bốn asset RGB565 có **5532 byte pixels**; object kèm descriptors **5628 byte Flash**, không thêm bitmap heap. Logo dùng `lv_image` component thực.

SHA-256 nguồn:

```text
reference-01.png  085323e49da863c5c9295c9433f99112d6f0a82c9832e7937ecd8cf5c011f771
reference-02.png  f2bfdba8ac1410c44cd4f33f800d7408a20ae20d96ea89e8ca7000fe74d9da79
eference-03.png   481ab4d209997ebec9a1d38c579860a70779ccdcdfc6fa086d43f43bb5718510
```

Font số dẫn xuất Montserrat48 của LVGL/MIT; bitmap7080+12615=19695byte; object20095byte Flash. Hash nguồn, generated C và asset raw có trong `brand-assets.json`, `font-generation.json` và `recovery-verification.json`. Không dùng font từ screenshot hoặc firmware Oxbot.

## 5. RGB888 → RGB565, stride, buffer và redraw

LVGL9.2.2 compositing nội bộ RGB888; flush dùng stride thật từ active draw-buffer header. Packer đọc BGR888 theo từng hàng, ghi MSB-first RGB565 tại đầu cùng buffer theo chiều tiến, không cấp full-framebuffer thứ hai trên ESP. Đích nhỏ hơn nguồn nên contraction không đè dữ liệu chưa đọc; bounds/stride được kiểm tra trước ghi.

Draw buffer giữ **20480byte**, INTERNAL|DMA|8BIT, alignment64. Full-width RGB888 chứa tối đa21hàng320px hoặc15hàng448px ở stride hiện tại. Bayer4×4 dùng tọa độ tuyệt đối, không reset pha theo dirty region; partial/full redraw cho cùng byte output. Dither giảm lỗi lượng tử trung bình, nhưng vẫn thấy texture khi zoom; chất lượng trên panel thật chưa đo. Asset nguồn RGB565 vẫn chỉ có độ chính xác5/6/5bit.

Write pipeline vẫn hoàn tất transaction trước `lv_display_flush_ready`, giữ ownership buffer; gatesOFF nên backend MOCK. Chưa chứng minh DMA/SPI hoặc FPS trên board. Instrumentation đã có, không coi native wall time là timing phần cứng.

## 6. Test và build thực tế

| Hạng mục | Kết quả | Bằng chứng |
|---|---|---|
| Native Debug | **345/345 PASS**,0fail/skip, NDEBUG không bật | `evidence/native-debug.xml`, `native-debug-receipt.json` |
| Native Release/NDEBUG | **345/345 PASS**,0fail/skip; checks luôn chạy | `evidence/native-release.xml`, `native-release-receipt.json`, compile commands được hash |
| ESP32-S3 | **BUILD PASS**, ELF/BIN/MAP, bootloader/partition và app identity hợp lệ | `evidence/esp32-receipt.json`, `esp32-build.txt`, `memory-report.json` |
| Release LVGL export | **152 ảnh**,0layout errors trong corpus | `evidence/render-validation.json`, `render-console.txt` |
| PC simulator | Hidden Win32 host mouse/keyboard/swipe **PASS**, cả hai profile trên exe đã package | `packaged-simulator-smoke.json`, `render-validation.json`, `simulator-preview.png` |
| Offline package | Paths/offsets/image CRC+SHA/ELF/config/profile/CSV đối chiếu | `SHA256_MANIFEST.json`, validator; không mở serial |

345 gồm302 regression cũ giữ nguyên,34 test frontend V3.1 và9 quality tests mới. Cả Debug/Release có đúng inventory; kiểm tra compile commands chứng minh NDEBUG đúng từng cấu hình. Không cộng152 ảnh hay package fixtures vào345.

| Quality test mới | Điều đã kiểm chứng |
|---|---|
| `quality_logo_assets` / `quality_logo_usage` / `quality_logo_aspect` | RGB565 descriptors, actual component asset pointers, pixels có màu nguồn, aspect với sai số làm tròn; không placeholder |
| `quality_rgb565` | Đen/trắng/đỏ/lục/lam chính xác mọi pha dither |
| `quality_wire_stride` | Rowstride12 cho width3, padding bị bỏ qua, byte order RGB565 đúng |
| `quality_buffer_limits` | 320×21/448×15 chấp nhận;320×22/448×16 và stride quá ngắn từ chối |
| `quality_font_aa` | LVGL glyph bitmap có>50pixel alpha trung gian và≥8level |
| `quality_partial_invariance` | Đổi LQ rồi khôi phục và fullinvalidate: framebuffer byte-identical, cả hai profile |
| `quality_dither_mean` | 256mức xám: sai số trung bình4×4 làm tròn≤1đơn vị RGB888 |

Regression cũ bao gồm parser/session recovery, stale validity, safety/read-only UI, display guards và saturation. Không sửa test để PASS, không thay CRC hoặc nới validation. Lần khôi phục không phát hiện lỗi firmware mới nên không có before/after bugfix mới để tuyên bố.

Native compiler Clang20.1.7; ESP Xtensa GCC14.2.0, ESP-IDF5.5, LVGL9.2.2, ILI9341 component2.1.0.839 dependency source hashes khớp lock. Không build lại STM32 riêng cho thẩm mỹ; source STM32 không đổi và native regression vẫn chạy.

## 7. Render và visual comparison

Ảnh cuối là **Release LVGL thật**, không phải bản vẽ Python/HTML hoặc ảnh full-screen thế UI. Python chỉ đổi framebuffer PPM thành PNG và tạo comparison sheets; Win32/GDI chỉ present framebuffer. Firmware widgets tạo từ LVGL objects/state. Bộ152ảnh gồm90primary(9trang×5state×2profile),50detail và12edge. Các trang ngoài HOME kế thừa logic/components hiện có, không có reference Oxbot gốc để chứng minh fidelity.

Đã xem trực tiếp HOME320/448, Oxbot comparison, V3/V3.1 và hai state sheets. HOME mặc định là SAFE simulator; STALE/DISCONNECTED xóa số liệu hết hạn, timer unavailable, dial muted; INVALID MODEL cảnh báo rõ. Không thấy clipping/overflow trong bộ đã kiểm; đây không phải bằng chứng mọi dữ liệu tương lai đều vừa.

- [HOME320×240](../artifacts/tyvor-ui-v3-1/home-320x240.png)
- [HOME448×368](../artifacts/tyvor-ui-v3-1/home-448x368.png)
- [Oxbot vs V3.1](../artifacts/tyvor-ui-v3-1/oxbot-vs-tyvor-v3-1.png)
- [V3 vs V3.1](../artifacts/tyvor-ui-v3-1/v3-vs-v3-1.png)
- [Visual review chi tiết](../artifacts/tyvor-ui-v3-1/visual-review.md)

Oxbot lấy đúng panel trái từ `artifacts/tyvor-ui-v3-oxbot/home-reference-comparison.png`, crop14,14,400,315. Hash file nguồn và pixel export nằm trong `reference-source.json`; pixel khớp reference cache cũ. So sánh Oxbot đặt ba panel cùng400×315, TYVOR dùng bilinear resize; HOME native không filter. Comparison V3 cũ dùng nearest-neighbor, vì vậy không gán toàn bộ độ mượt trong comparison mới cho firmware. Ảnh V3/V3.1 đặt native pixels cạnh nhau để đối chứng.

Geometry IoU dựa vùng ước lượng bằng mắt, không phải perceptual score. Timer estimate là glyph còn allocation là label nên không đồng nhất loại bounding box; không dùng để tự chấm Visual PASS. Khác Oxbot còn: font số rộng/tròn hơn, màu/nền và shading chưa trùng, icon/tick/knob khác, logo trắng TYVOR khác icon Oxbot. Required safety/unknown labels và battery/bind unavailable được giữ. Cần người dùng quyết định mức fidelity chấp nhận được; không tự tuyên bố1:1.

## 8. Memory / Flash

| Mục | Byte / kết quả |
|---|---:|
| App BIN | **496912** |
| ELF / MAP | 6569600 / 3926131 |
| Bootloader / partition BIN | 20896 / 3072 |
| Factory app partition | 3145728; app còn2648816byte |
| DIRAM static, gồm code/data/BSS | **175483/341760 (51.35%)**, còn166277 |
| DIRAM data / BSS / text | 11760 / 107184 / 56539 |
| Dedicated IRAM vectors+text | 1028+15356=16384 |
| Flash code / rodata / app descriptor | 286358 / 125464 / 256 |
| LVGL arena | 98304, đã nằm trong BSS |
| Draw buffer runtime | 20480, không cộng vào static BSS |
| UI / bridge / diagnostics task stack config | 8192 / 4096 / 4096 |
| Native HOME used arena, cả hai profile | 18536 |
| Native arena peak320 /448 | 26784 / 28296 |
| LQ-only redraw320 /448 | 3696/76800px (4.81%) /5126/164864px (3.11%) |
| Idle redraw | 0pixel cả hai profile |

So V3: app+9696byte, DIRAM+8byte. `idf-size.json` total496761 là section-based metric; console496793 bao gồm32byte RTC segment; BIN496912 còn image headers/checksum/padding. Không đánh đồng các số này. Dedicated IRAM sử dụng hết vùng16384 theo linker không có nghĩa toàn bộ SRAM đã hết.

Compiler `.su`: UI task frame1792, diagnostics1568, V2project992, TFTproject960byte. Đây là single-function frames, **không phải** call-chain peak hoặc stack high-water đo trên board. Không tuyên bố còn đủ heap/stack phần cứng chỉ từ native arena và static map.

Flash16MB, OCT PSRAM config và DIO80MHz được kiểm trong image/sdkconfig; **chưa xác minh chip Flash16MB/PSRAM8MB vật lý**, màu panel, ILI9341, AMOLED hoặc touch. `SPIRAM_IGNORE_NOTFOUND` vẫn bật cho software experiment; không coi boot được là PSRAM PASS.

## 9. Artifact, SHA-256 và tái kiểm tra

Package: `artifacts/tyvor-ui-v3-1/`; manifest `SHA256_MANIFEST.json` ghi hash/size **263file artifact**, kèm catalog **297source/docs/scripts**. Package bao gồm ảnh, Debug/Release PC exe, ELF/BIN/MAP, bootloader, partition, flash metadata, logs/JUnit/receipts, memory, visual review và licenses. Không có flash entry point trong simulator launcher.

App BIN SHA-256:

```text
39a4b56f8f12a2a4d3997e99c9e4c80563fd947f0e0e71d0828679b4760a0674
```

ELF SHA-256:

```text
c88f2bffbe7406295afe0c8062b3690222b530d6a4a75151f8688fe09e0f3d57
```

Offsets offline: bootloader0x0, partition0x8000, app0x10000; partition CSV/binary phải khớp, không overlap. Validator kiểm mọi đường dẫn nằm trong package trước khi đọc. Positive fixture và8negative fixture kiểm gate bật, GPIO gán, thiếu image, sai offset, path traversal, binary corruption và partitionCSV sai; kết quả cuối trong `evidence/package-negative-checks.json`. Chỉ thao tác fixture offline, không flash.

Recheck không rebuild:

```powershell
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/ui31_artifacts.py verify
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/ui31_artifacts.py validate
```

Khi thực sự cần render lại: `scripts/export-ui-v3-1.py` bằng CPython local trên. Đọc receipts trước; nếu sửa source compile input, phải build/test lại target liên quan trước package. Full build khi cần: child PowerShell `-NoProfile -ExecutionPolicy Bypass -File scripts/build-ui-v3-1.ps1 -Target all`; không thay execution policy global. Không cần chạy lại cho báo cáo/metadata đã hoàn thiện.

## 10. Git diff/WIP, lỗi còn lại và điều kiện phần cứng

Git root cha báo `?? ./`; tracked `git diff --stat` trống vì project vẫn untracked. Không coi diff trống là không có WIP. Frozen baseline253file tại `build/ui-v3-1-baseline/source/`: **252file byte-identical**, chỉ root CMake thêm optionOFF.44file V3.1 mới và patch/hash được lưu evidence. Các manifest artifact phase1/2a/2b/2c/Discovery/TFT/V2/V3 cũ được đối chiếu hash; cấu hình runtime cũ và logo input giữ nguyên. Không thay firmware STABLE hoặc repo EdgeTX.

Không có test/build FAIL còn mở trong phạm vi kiểm chứng. Các vấn đề còn lại:

1. **Visual review pending:** fidelity chưa1:1; khác font/nền/icon/dial và có hạt dither khi zoom. Cần duyệt ảnh native và comparison, không suy từ số test.
2. **Hardware UNVERIFIED:** board revision, nguồn, pinout, driver, DMA, color order, FPS/WCET, heap/stack, Flash/PSRAM, AMOLED/touch chưa đo. Chưa đủ điều kiện tự flash hoặc mở gate.
3. **Data limitations:** radio battery, charge percent, trim, bind và aircraft ARM chưa có nguồn authoritative; tiếp tục unknown/disabled. Không sửa Core để làm UI đẹp hơn.
4. **Reference limitations:** chỉ có still HOME; không có font gốc, clean vector logo master, reference cho các trang khác hoặc motion evidence. Corpus152ảnh không bao phủ mọi chuỗi runtime có thể có.

Review PC: mở `artifacts/tyvor-ui-v3-1/START_SIMULATOR.cmd`, thêm`--wide` cho448×368; keys1–5 đổi fixture, chỉ UI local. Hidden-host smoke đã chạy; không tuyên bố tương tác nhìn bằng mắt trên board.

Hardware deployment chỉ xét sau khi người dùng review, có schematic/PCB/pin/nguồn đúng board, xác minh display/controller, backup/recovery và phê duyệt flash riêng. **Lần này dừng ở software review; DISPLAY/TOUCH/UART/RF gatesOFF, GPIO chưa xác minh=-1.**
