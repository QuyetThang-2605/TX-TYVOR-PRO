# TYVOR OS V3.2.1 — Final visual correction

Ngày: **2026-11-12**. Workspace: `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Minor correction đã hoàn thành code/build/test/render. Visual Final PENDING USER REVIEW; hardware UNVERIFIED.** Giữ version V3.2.1/app`0.5.3-ui-v3-2-1`, không redesign hoặc tự nâng phiên bản lớn.

## 1. Điều tra sharpness trước khi sửa

Đã đọc source LVGL/font/branding/packRGB565/exporter, báo cáo V3.2.1, approved gốc, nativeRGB888/565 và ảnh comparison người dùng review. Không có AGENTS.md áp dụng trong workspace/thư mục cha đã tìm. Git status`?? ./`; tracked diff trống vì project untracked. Đã kiểm receipts và292file package V3.2.1, freeze434source/vendor/docs/scripts vào`build/ui-v3-2-1-final-baseline/source/` trước thay đổi; không reset/restore/clean/undo WIP.

**Nguồn làm mềm thêm nằm ở comparison exporter.** Native PNG320/448 giải nén khớp byte với PPM/framebuffer renderer; PNG writer chỉ đóng gói RGB rows lossless. Code timer/logo không runtime scale hoặc blur; font native52/74px Montserrat Bold4bpp, logo A8 đúng pixel-size/alpha hiện có. Không có bằng chứng để sửa font phỏng đoán, nên font và logo được giữ nguyên SHA.

| Trường hợp cũ | Nguồn LVGL | Viewport comparison | Tác động |
|---|---:|---:|---|
| Compact | 320×240 | 400×300 trong400×328 | Phóng bilinear1,25×, tạo thêm pixel cạnh trung gian |
| Wide | 448×368 | 399×328 | Thu bilinear0,8913×, mất chi tiết native |
| Approved | ~704×570 | 400×324 | Thu từ nguồn có độ phân giải cao hơn; cùng filter chưa làm ngân sách pixel bằng nhau |

Đã kiểm độc lập mỗi pixel RGB888 trước pack với output565: mọi pixel khớp công thức Bayer hiện có, sai số tối đa R9/G4/B9 trên8bit, mean≈R1,91/G1,25/B2,25 ở320. Đây là color quantization/dither, không có trộn pixel lân cận để gây blur. Nền vốn làRGB565 asset; rawRGB888 compositor không khôi phục24bit của source PNG.

Old-resize rồi trả về native thay đổi pixels timer/logo: MAE timer3,5679/3,7992 và logo8,1524/8,1871 tại320/448. Số này chỉ chứng minh resampling không identity; không chấm visual fidelity hoặc lấy làm chứng nhận font sắc. Anti-aliasing4bpp, logo16/23px và nguồn JPEG vẫn có giới hạn native, nên không quy mọi soft edge cho exporter.

Evidence tạo **trước firmware edits**: `evidence/sharpness-before.json`, `export-tests-before.txt`. Policy cũ FAIL2/5 exporter contracts (native size/no upscale); các kiểm tra PNG và rawcapture vẫn PASS. Policy cuối PASS5/5. Không thay renderer/font để che lỗi comparison.

## 2. Sửa exporter và các thay đổi nhỏ HOME

Exporter mới dùng `native_pair`: cùng một policy **uniform bilinear shrink-or-identity, never upscale**, fit/pad approved vào mỗi viewport320×240 hoặc448×368. Phía LVGL luôn1:1 và byte-identical, không còn kéo mọi màn về400×328. Không dùng nearest cho một bên rồi nội suy cho bên khác. Hàng compact/wide giữ native dimensions, padding riêng; native PNG không filter/upscale.

| Phần | Thay đổi cuối |
|---|---|
| Timer/font | Không đổi bitmap/TTF/metrics/font opacity hoặc RGB565 conversion. Giữ glyph rendering native. Chỉ cách trình bày comparison được sửa. |
| Logo | Không scale lại, sharpen, thay alpha hoặc vẽ lại logo. Pixel crops trước/sau khớp byte; logo rõ hơn trong comparison vì bỏ resampling phía LVGL. |
| Card | Hero opacity212→224, border65→80; footer214→220. Giữ màu/layout/radius, giảm wave interference nhẹ dưới chữ. |
| GPS/Bind | Giữ map-pin Lucide và tăng stroke2→2,25/24. Bind từ radio arcs thành5bar waveform đối xứng theo approved, dựa stroke convention Lucide; icon flat, vẫn disabled/N/A. |
| Gimbal | Tick1→1,25px, thêm rim phẳng0,75px/38% alpha trong static A8 cache. Không thêm dynamic line raster, metallic bevel, shadow/neon. Dot/validity/clamp giữ nguyên từ UI state. |
| Safety/data | Giữ SIM GATE SAFE / CORE GATE SAFE và ARM UNKNOWN. Không đổi ARM/Mixer/RF logic, không chuyển sang SAFE/DISARMED. |

Chỉ4file baseline sửa: `experiments/tyvor_ui_v3_2_1/ui_pages.c`, `icon_data.c`, `assets/lucide/SOURCES.json`, `scripts/generate-ui-v3-2-1-icons.py`. Header generated được format về nguyên bản; font/branding/palette/background/Core/protocol không đổi. Icon generator ghi metadata vào build-final riêng; artifact và metadata lịch sử của build cũ được bảo toàn.

Các file mới là exporter/image helpers/tests, build/provenance helper riêng, source SVG original+adaptation và báo cáo này. Full inventory/hash/patch ở`evidence/source-changes.json`, `ui-v3-2-1-diff.patch`, `preservation.json`. Không thay root CMake hoặc build/profile cũ. Assets gốc `approved-home.png.png`, `blue-wave-background.png` và logo nguồn không ghi đè.

## 3. Kiểm chứng safety và LQ

98% trong HOME mặc định đến từ **SIM fixture hợp lệ**, có SIM ở header; không phải telemetry được đo trên board. Trace: `tft_presenter.c:45` gán98 trong `tyvor_tft_fixture`, qua fixture V2/V3.2.1; `native/render.c:53` gọi project với simulator=1. Presenter vẫn poll TTL/session, chỉ hiện quality khi status/field hợp lệ; hết hạn hoặc disconnect thì unknown/mute theo regression cũ. Không hardcode98% thành trạng thái thiết bị. CORE mode vẫn đòi snapshot hợp lệ; aircraft ARM không có nguồn authoritative vẫn UNKNOWN.

Không mở Bind/UNAVAILABLE, không có UI event ARM/RF/output writer. STM32F407VET6 Core, ESP32-S3 N16R8 architecture, protocolv2, Safety Engine và Mixer giữ nguyên. Hardware Display/Touch/UART/RF gatesOFF; GPIO chưa xác minh=-1. Không flash, đọc/nạp chip, commit/push/deploy, sửa Git global hoặc TYVOR STABLE/EdgeTX.

## 4. Build, regression và package

| Kiểm tra | Kết quả cuối | Evidence |
|---|---|---|
| Native Debug | **445/445 PASS**,0fail/skip | `native-debug.xml`, `native-debug-receipt.json` |
| Native Release/NDEBUG | **445/445 PASS**,0fail/skip; CHECK luôn chạy | `native-release.xml`, `native-release-receipt.json` |
| ESP32-S3 | **BUILD PASS**, ELF/BIN/MAP, app/bootloader/partition hợp lệ | `esp32-receipt.json`, `esp32-build.txt`, `memory-report.json` |
| Exporter contracts | Old policy FAIL2/5 → final **5/5 PASS** | `export-tests-before.txt`, `export-tests-after.txt` |
| Native render |152RGB565screens+2rawRGB888HOME,0detected layout errors | `render-validation.json` |
| Debug/Release render regression | **154/154 PNG hash-identical** | `render-consistency.json` |
| Geometry/preservation | HOME allocations giữ nguyên; font/logo/background/palette byte-identical | `final-source-evidence.json` |
| Packaged PC simulator | Hidden host mouse/keyboard/swipe PASS ở320/448 | `packaged-simulator-smoke.json` |
| Offline package | Paths/offsets/image checksums/SHA/ELF/partition/config và positive+8negative fixtures PASS | Manifest, `package-negative-checks.json` |

445 native tests là toàn bộ inventory cũ, không giảm/nới assertions.5Python exporter checks là nhóm riêng, không cộng vào445. Full build cuối exit0; ba receipts current khớp source/compiler/output. Native Clang20.1.7, Xtensa GCC14.2.0/ESP-IDF5.5.1, LVGL9.2.2 và ILI9341component2.1.0;839dependency source hashes còn khớp lock. Không build/flash STM32 vì Core không đổi; Core/session/fault regressions vẫn chạy native.

## 5. Memory, redraw và Flash

| Mục | Byte / kết quả |
|---|---:|
| ESP app BIN | **1069520**, giảm688 so bản trước |
| ELF / MAP | 7146884 /3941990 |
| Bootloader / partition BIN | 20896 /3072 |
| Factory partition | 3145728; còn2076208byte |
| DIRAM static | **175491/341760 (51,35%)**, không tăng |
| Data /BSS /DIRAM text | 11760 /107192 /56539 |
| Dedicated IRAM | 16384 |
| Flash code /rodata /app descriptor | 285430 /699000 /256 |
| Background+logo object | 490078Flash, không đổi |
| Timer font object | 22305Flash, không đổi |
| Icon/tick object | 86864Flash, giảm696; thay radio geometry và cùng mask sizes |
| LVGL arena /draw buffer | 98304BSS /20480runtime, không đổi |
| Known app heap lower bound | 36864 =draw buffer+3task stacks; chưa tínhTCB/queues/SDK |
| Native arena used /peak320 /peak448 | 16240 /20744 /30552 |
| LQ partial redraw320 /448 | 3696 /5126pixels; idle0pixel |

Không có per-frame full-framebuffer allocation trên ESP; chỉ PC observer có rawRGB888 framebuffer. Không đo lại hoặc tự nhận boardFPS/heap/stack/PSRAM PASS. Native frame_us vẫn dùng clock giả; redraw counters là software evidence. Image16MB Flash/OCT PSRAM config và factory fit là offline validation, chưa phải chip vật lý. ELF size có debug data không phải Flash usage; section metric1069369 khác paddedBIN1069520.

## 6. Ảnh cuối và cách review

Package: `artifacts/tyvor-ui-v3-2-1-final/`. Bốn capture không upscale:

- [320×240 RGB888](../artifacts/tyvor-ui-v3-2-1-final/home-rgb888-320x240.png), [320×240 RGB565](../artifacts/tyvor-ui-v3-2-1-final/home-rgb565-320x240.png).
- [448×368 RGB888](../artifacts/tyvor-ui-v3-2-1-final/home-rgb888-448x368.png), [448×368 RGB565](../artifacts/tyvor-ui-v3-2-1-final/home-rgb565-448x368.png).
- [Approved vs Final LVGL](../artifacts/tyvor-ui-v3-2-1-final/approved-vs-final-lvgl.png), [Before vs After](../artifacts/tyvor-ui-v3-2-1-final/before-vs-after.png).
- [RGB888 vs RGB565](../artifacts/tyvor-ui-v3-2-1-final/rgb888-vs-rgb565.png), [timer/logo native detail](../artifacts/tyvor-ui-v3-2-1-final/timer-logo-native-detail.png).
- [Báo cáo nguyên nhân mờ](../artifacts/tyvor-ui-v3-2-1-final/BLUR_CAUSE.md), [contact sheet](../artifacts/tyvor-ui-v3-2-1-final/all-pages-contact-sheet.png).

Đã xem native HOME, raw/565, approved comparison và before/after thực tế. So sánh chính: approved trái/final phải, compact trên/wide dưới; mỗi phía dùng cùng viewport policy. Native bytes của actual không bị exporter biến đổi; JSON có vị trí/dimensions để kiểm lại. Không dùng blur/unsharp mask hoặc ảnh UI giả thay LVGL.

Còn khác approved: font shape/weight và vài tỉ lệ responsive vốn có, card flatter/darker, disabled Bind mờ hơn glyph active trong approved, raster logo nhỏ có giới hạn chi tiết, RGB565 grain khi zoom. Chính sách so sánh mới không làm các khác biệt đó biến mất. Không sửa font theo phỏng đoán hoặc đổi layout để che chúng. **Visual Final vẫn chờ người dùng**; không dựa test/build thành công để tuyên bố đạt.

## 7. SHA-256, WIP và tái kiểm tra

`SHA256_MANIFEST.json` ghi hash/size toàn bộ package và catalog source hiện tại. Old package/screenshot giữ nguyên, baseline434file có430unchanged và4reviewed modifications; new files liệt kê chính xác trong preservation evidence. Git tracked diff trống được bổ sung bằng frozen snapshot/hash/fullpatch. Các historical manifests và runtimeconfig/BSP cũ được đối chiếu lại, không undo WIP.

App SHA-256:

```text
73cd93d7822c008aec021588b8a85a16c938c9b2a9af59964588adfe53ba7d9b
```

ELF SHA-256:

```text
2d50ef8d0620b1c6d73abc52e11ae1f61cae2a660c6e8e6e5982edc9aafa20a7
```

Read-only recheck:

```powershell
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/ui321_final_artifacts.py verify
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/ui321_final_artifacts.py validate
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/test-ui321-final-images.py
```

Khi compile inputs thay đổi mới chạy lại child PowerShell `-NoProfile -ExecutionPolicy Bypass -File scripts/build-ui-v3-2-1-final.ps1 -Target all`, sau đó inspect/export/package. Export dùng`export-ui321-final.py`; đường build/artifact riêng, giữ firmware versionV3.2.1. Launcher chỉ mở PC simulator; không flash script nào tự chạy.

**Dừng tại review Visual Final.** Không có test/build FAIL còn mở trong source cuối; hardware/pinout/display/touch/timing/Flash/PSRAM vẫn UNVERIFIED và cần phê duyệt riêng trước bất kỳ bước thiết bị nào.
