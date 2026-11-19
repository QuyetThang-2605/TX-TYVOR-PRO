# TYVOR OS V1 - Phase 1 reliability fix report

Ngày 09/11/2026, Asia/Bangkok. Workspace: `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Kết luận: READY cho Phase 2A ở mức baseline phần mềm bench, chờ người dùng review.** Hai lỗi HIGH đã được tái hiện bằng C trước sửa và có regression PASS sau sửa ở Debug/Release. Hai MCU build thành công; package ESP và provenance được kiểm tra offline. Không tuyên bố boot/hardware PASS và không tự chuyển sang Phase 2A.

Đã đọc đầy đủ `PHASE1_IMPLEMENTATION_REPORT.md` và `PHASE1_FINAL_REVIEW.md`. Giữ nguyên hai báo cáo lịch sử, initial audit và TX15 reference; tài liệu protocol/build hiện hành đã cập nhật. Không sửa repository TYVOR V1 STABLE/EdgeTX, flash, mở serial, commit, push, deploy hoặc Git global. UART gate cả hai phía vẫn OFF.

## 1. Root cause và cách khắc phục từng phát hiện

| Phát hiện | Root cause | Kết quả |
|---|---|---|
| H1 - HIGH | Deadline thuộc header lỗi cũ nhưng timeout path xóa cả suffix còn dở, kể cả đầu frame mới đến t=49 ms | Đã sửa ownership theo từng byte/candidate; regression trước FAIL, sau PASS |
| H2 - HIGH | Sequence 8-bit sống qua reset transport nhưng không có session identity; sender reboot về 0 bị so với baseline 128 | Đã bổ sung protocol v2 với HELLO/SESSION_ACK, boot identities + epochs; kiểm chứng bằng C endpoint tests |
| M1 - MEDIUM | Package làm phẳng bootloader/partition nhưng giữ metadata dùng đường dẫn con của SDK | Giữ nguyên cấu trúc SDK; offline validator kiểm tra mọi reference/offset/image |
| M2 - MEDIUM | Hash source hiện tại cạnh binary/log có sẵn, không ràng buộc build inputs, inventory và skipped tests | Bổ sung receipts từ build thật; packager từ chối receipt thiếu/cũ hoặc output/test không khớp |
| L1 - LOW | Source ban đầu chưa được track nên Git diff không biểu diễn WIP | Giữ trạng thái Git theo yêu cầu; có snapshot trước sửa, source change inventory và diff file để review. Chưa có tracked baseline |
| L2 - LOW | Main và ISR cùng `volatile uart_errors++`, có thể mất increment | Tách `uart_tx_errors` do main sở hữu và `uart_rx_errors` do ISR sở hữu; không còn phép ++ cùng counter |

### H1: frame ownership và timeout

Parser chỉ sở hữu framing, không quyết định session hoặc sequence. Buffer cố định 64 byte có 64 timestamp arrival đi kèm. Khi bỏ prefix, giữ nguyên timestamp của suffix. Header/CRC/tag/payload sai hoặc candidate dở dang hết hạn chỉ loại một byte rồi rescan. Suffix mới được xét bằng deadline riêng, không bằng deadline cũ.

Frame hợp lệ sở hữu cả payload; không tách nhầm nested frame trong payload đã có CRC hợp lệ. Candidate dở dang có deadline tuyệt đối 50 ms; byte đến thêm không kéo dài deadline. Frame hoàn chỉnh được recovery muộn dùng timestamp byte cuối lúc thực nhận, không dùng thời điểm poll để làm mới telemetry. CRC CCITT-FALSE, payload tối đa 32 và mọi semantic validation được giữ nguyên; không bỏ CRC/nới validation để test PASS. Frame v2 tối đa 64 byte là do thêm session header, không tăng payload allowance.

### H2: session negotiation và reboot recovery

Cả firmware dùng chung `tyvor_link`. Mỗi boot cần nonzero fresh 64-bit identity từ RNG; mỗi lần thương lượng tăng local epoch uint32, không wrap khi cạn. Mọi data frame mang cả hai boot IDs và cả hai epochs, nằm trong CRC. Major version chuyển 1 -> 2, minor handshake=0. V1 hoặc minor lạ bị từ chối; phải cập nhật hai endpoint đồng thời.

Pending session và active session tách biệt. Chỉ SESSION_ACK echo đúng pending boot ID/epoch, đúng candidate/role và deadline mới được commit; pending epoch phải đã được HELLO hoặc SESSION_ACK gửi đủ byte thành công. Failed/partial TX hoặc ACK đến trước offer không chứng minh handshake. Commit xóa toàn bộ old snapshot, reset sequence theo type và yêu cầu STATUS hợp lệ thuộc tuple mới để valid trở lại. Không đổi session chỉ vì sequence nhỏ hơn.

Mỗi attempt tối đa 5 HELLO, cách nhau 200 ms, deadline 1000 ms, cooldown 1000 ms sau timeout; không busy loop/catch-up burst. ACK control có fixed slots và rate limit 100 ms. Duplicate active HELLO/ACK idempotent. Delayed data/ACK sai tuple/epoch bị loại; HELLO không chứng minh peer mới nên chỉ mở probe, giữ active session đến khi có ACK proof. Peer reboot có thể thay một liveness probe đang trỏ tới boot cũ bằng challenge epoch mới.

Sequence trong active session: delta=0 duplicate/drop; 1 accept (gồm 255->0); 2..127 accept/count gap; 128..255 reject. Transport loss không xóa history. Sau 2 s không có peer data, hoặc UI không có STATUS dù ACK vẫn tiếp diễn, bắt đầu handshake epoch mới để thoát half-range loss. Không reset sequence chỉ vì stale. Snapshot stale ở age >=2000 ms/clock regression; ACK/DIAGNOSTIC/duplicate không làm mới STATUS. Chi tiết wire/header/state/ownership: [PROTOCOL_PHASE1.md](PROTOCOL_PHASE1.md).

## 2. File đã sửa và thêm

So với snapshot đầu phiên 37 file: **22 file sửa, 8 file mới, 15 file giữ nguyên**, tổng 45 file source/docs/scripts hiện hành. Không tính `.tools`, generated config, build hoặc artifacts. Tệp sửa trong reliability work:

- `shared/tyvor_protocol.c`, `.h`: parser per-byte arrival ownership, strict v2 header/tag/CRC; chuyển sequence acceptance lên link.
- `stm32/Core/Inc/tyvor_bridge.h`, `stm32/Core/Src/tyvor_bridge.c`, `main.c`: common session endpoint CONTROL, publish chỉ sau ready, dispatcher read-only.
- `stm32/BSP/board.c`, `.h`, `stm32/Core/Inc/stm32f4xx_hal_conf.h`, `stm32/CMakeLists.txt`: boot RNG, HSI-fed PLL48 chỉ cho RNG, separate error owners, bounded TX 10 ms và link/RNG build integration.
- `esp32/main/main.c`, `esp32/main/CMakeLists.txt`, `esp32/CMakeLists.txt`: common endpoint UI, boot entropy trước task, mutex bảo vệ cache/link/stats, UART error queue recovery; project version `0.1.1-reliability`.
- `CMakeLists.txt`, `sim/test_protocol.c`: giữ 33 test case/invariant cũ nhưng migrate wire/API sang v2, đăng ký thêm 31 reliability cases; các trường hợp payload/header/nested điều chỉnh theo frame size thực, không nới payload validation.
- `scripts/build-phase1.ps1`, `bootstrap-tools.ps1`, `tools.lock.json`, `package_artifacts.py`, `.gitignore`: build receipts, absolute native compiler, managed Python cho mọi target, cấu trúc package/manifest portable và loại Python cache.
- `README.md`, `docs/BUILD_PHASE1.md`, `docs/PROTOCOL_PHASE1.md`: hướng dẫn/trạng thái/compatibility mới.

Tệp mới: `shared/tyvor_link.c`, `.h`; `sim/test_reliability.c`, `sim/test_transport.h`; `scripts/build_provenance.py`, `validate_esp_package.py`, `test_package_validation.py`; báo cáo này. Không sửa `shared/tyvor_status.h`, UI view model, linker memory map, pin definitions hoặc partition layout khi chưa có nhu cầu đã chứng minh.

Danh sách/hash trước-sau đầy đủ: `build/evidence/reliability-source-changes.json`; diff: `build/evidence/reliability-diff.patch`. Artifact package kèm source snapshot patch, source hashes và Git WIP status.

## 3. Regression trước và sau sửa

Trước khi sửa parser/link, đã đóng băng 37 file source/docs/scripts và old manifest vào `build/reliability-baseline`, cùng bản v1 `shared` để compile probe C độc lập. Harness giữ nhánh adapter v1; cùng điều kiện acceptance của hai probe được compile ở Debug và Release/NDEBUG. Hai expected failures đã xảy ra thực tế, không suy ra từ static review.

| Probe | V1 Debug trước sửa | V1 Release/NDEBUG trước sửa | V2 Debug sau sửa | V2 Release/NDEBUG sau sửa |
|---|---|---|---|---|
| H1 header H t=0; nửa F t=49; poll t=50; phần còn lại t=51 | **FAIL**, received=0, used=0, timeout=1, exit=1 | **FAIL**, cùng kết quả, exit=1 | **PASS**, received=1, used=0, timeout=1, exit=0 | **PASS**, cùng kết quả, exit=0 |
| H2 nhận seq128; sender reboot gửi 0..128 | **FAIL**, sequence reject=128, duplicate=1, accepted=0, exit=1 | **FAIL**, cùng kết quả, exit=1 | **PASS**, session count=2, new-session STATUS seq0 accepted, exit=0 | **PASS**, cùng kết quả, exit=0 |

H2 trước sửa mất tổng 129 STATUS, không phải chỉ mất 128. Sau sửa không chấp nhận seq0 vào session cũ: test thật tạo boot ID mới, hoàn thành handshake rồi mới nhận STATUS seq0. Vì giải pháp thêm state machine mới, nhánh v2 kiểm tra handshake + invalidation + acceptance; nhánh v1 giữ đúng scenario lỗi ban đầu. Không diễn giải đây là byte-for-byte test chung giữa hai wire format khác nhau.

Bằng chứng trước sửa: `build/reliability-baseline/evidence/{Debug,Release}-h1_timeout_suffix.txt` và `*-h2_sender_reboot.txt`; sau sửa: `build/evidence/{Debug,Release}-h1_timeout_suffix-after.txt`, `*-h2_sender_reboot-after.txt`. Mỗi log ghi actual exit code. Package giữ các log trước trong `evidence/before-fix`. Cách compile/rerun probe có trong [BUILD_PHASE1.md](BUILD_PHASE1.md).

## 4. Kiểm thử thực tế

| Kiểm tra cuối | PASS | FAIL / skip | Bằng chứng |
|---|---:|---:|---|
| Native Debug | 64 | 0 / 0 | `build/evidence/native-debug.xml`, `native-debug-ctest.txt`, receipt |
| Native Release/NDEBUG | 64 | 0 / 0 | `native-release.xml`, `native-release-ctest.txt`, receipt; compile commands có `-DNDEBUG` ở cả hai test source |
| Python offline package/provenance | 21 | 0 / 0 | `package-tests.json`, `.txt` |
| STM32 configure/build/link/BSP candidate compile-check | PASS | 0 lỗi build cuối | `stm32-build.txt`, `stm32-receipt.json` |
| ESP32 IDF configure/build/size | PASS | 0 lỗi build cuối | `esp32-build.txt`, `esp32-receipt.json` |
| Offline package + manifest/source check | PASS | 0 mismatch/missing | `esp-package-final-validation.json` và output validator cuối |

Tổng native cuối là **128 lượt PASS / 0 FAIL**, cùng 64 case ở hai cấu hình; không cộng các lần rerun/probe vào số này. Python là 21 case riêng. CTest inventory gồm 33 case cũ migrate v2 + 31 case mới. Cả Debug/Release dùng CHECK luôn chạy; receipt kiểm tra chính executable/argument và NDEBUG mode, không chỉ con số XML.

Các nhóm chứng minh cụ thể:

- H1 có 37 điểm chia STATUS v2 x 3 biên 49/50/51 = 111 tình huống trong `timeout_every_split`, cùng recovered last-byte timestamp, truncation/slow frame/noise/header/CRC/nested/consecutive tests. Max/empty/oversize, output guard bytes, CRC bit flips và overflow/reset vẫn được test.
- `randomized_uart_seed`: ba seed `0x13579BDF`, `0x2468ACE0`, `0x00C0FFEE`; mỗi seed 2000 vòng chèn nhiễu, split ngẫu nhiên, CRC lỗi; mỗi cấu hình khôi phục đủ 6000 frame biết trước. Mỗi seed có 286 CRC errors đúng injection và không cập nhật sai acceptance count. Đây là bounded deterministic campaign, không phải exhaustive fuzz proof.
- `randomized_reboots_seed`: seed `0x5EED1234`, 80 vòng reboot sender/receiver/cả hai, có ACK loss. Kiểm tra cả hai đã ready, hết pending và crossed IDs/epochs khớp; không nhầm active session cũ với handshake mới.
- Reboot riêng mỗi MCU/cả hai; duplicate HELLO/ACK; lost HELLO/SESSION_ACK; retry/deadline/cooldown; no-session no-data; delayed old packets; wrong echoed epoch; retired epoch; late ACK; same-ID collision/candidate collision; nonzero identity/epoch exhaustion; clock regression; stale invalidation; loss >=128; ACK traffic không che STATUS stale; reboot trong liveness probe.
- `ack_before_offer` và `control_tx_failure_recovery` chứng minh không commit khi TX chưa gửi đầy đủ và vẫn recover sau lỗi/partial TX. Data-TX error/partial và application ACK rate limit tiếp tục có test.
- Compatibility không chỉ test encode/decode: common C CONTROL/UI endpoints trao đổi byte stream thật qua queue giả lập; `bridge_requests`/`bridge_tx` đi qua dispatcher STM32 thật. ESP dùng chính link implementation này, build bằng IDF; hardware UART/FreeRTOS scheduling chưa được chạy trên board. `protocol_version_compatibility` từ chối v1 và minor khác; không có fallback.

Trong triển khai, randomized reboot test đã bắt một lỗi mới: pending liveness probe khóa vào boot cũ làm boot mới không hoàn tất. Đã sửa bằng fresh challenge khi thay probe, thêm regression riêng rồi rerun cả bộ. Rà soát tiếp bắt thêm ACK trước offer và STATUS loss bị che bởi ACK traffic; cả hai có test riêng PASS. Không bỏ/skip kiểm tra để lấy kết quả xanh.

## 5. Build hai MCU, static memory và firmware size

Toolchain thực: native Clang 20.1.7, Arm GCC 14.2.1, ESP-IDF 5.5.1/Xtensa GCC 14.2.0, CMake 3.30.9/Ninja 1.12.1; Python 3.11.13 portable. Dùng SDK đã cài, không đổi Git global/system PATH hay cài tool vào repository khác. Receipts lưu compiler path/version/SHA và build input/output hashes.

### STM32F407VET6

ELF32 ARM/Thumb-M4F, linker VE giữ nguyên: Flash `0x08000000`/512 KiB, SRAM `0x20000000`/128 KiB, CCM `0x10000000`/64 KiB. Không dùng Flash 1 MiB của VG. Đã dựng lại BIN từ flash PT_LOAD của ELF và so byte-for-byte; kiểm tra mọi allocated section nằm trong vùng hợp lệ, 98 vector entries, SP=`0x20020000`, Reset/entry=`0x080014B1` và Thumb/address bounds hợp lệ.

| Thông số cuối | Byte |
|---|---:|
| BIN / Flash load | 8420 / 524288 (1,61%) |
| ELF có debug | 1030664 |
| Vector / text / exidx | 392 / 8008 / 8 |
| `.data` / `.bss` | 12 / 996 |
| Stack reserve tối thiểu / section gồm padding | 4096 / 4100 |
| SRAM region gồm data/bss/stack/alignment | 5112 / 131072 (3,90%) |
| CCM dùng | 0 / 65536 |

Không dùng heap trong mã STM32. File `.su` có largest per-function frame 128 byte (`main`), không phải peak call-chain/interrupt stack. BSP candidate UART object compile-check PASS nhưng không link candidate vào ELF mặc định. Cache vẫn `TYVOR_UART2_PA2_PA3_VERIFIED=OFF`, IWDG ON. SYSCLK HSI16 giữ nguyên; PLL48/RNG mới cần boot/timing/entropy test thật sau này. HAL TX tăng 5 -> 10 ms vì 64 byte ở 115200 8N1 cần khoảng 5,56 ms; không thể giữ deadline cũ 5 ms cho frame tối đa mới.

### ESP32-S3

| Thông số cuối | Byte |
|---|---:|
| App BIN | 214928 |
| App ELF có debug | 3586760 |
| Bootloader BIN / partition BIN | 22336 / 3072 |
| Factory 3 MiB còn trống | 2930800 |
| DIRAM tổng dùng / được cấp trong map | 59719 / 341760 (17,47%) |
| DIRAM text / data / bss | 45211 / 11404 / 3104 |
| Vùng IRAM riêng trong IDF size | 16384 / 16384 |
| Flash code / rodata | 104262 / 37260 |

IRAM riêng 100% không có nghĩa hết toàn bộ MCU RAM; đây là cấu trúc linker/config hiện có. Số static không gồm heap/stack runtime và framebuffer tương lai. Hai task vẫn dùng 4096 byte stack mỗi task; high-water/heap logs là instrumentation chưa đo trên board.

esptool `image_info --version 2` chỉ đọc file: app và bootloader ESP32-S3 chip ID 9, 16 MB/80 MHz, checksum/hash hợp lệ; app version `0.1.1-reliability`; app descriptor ELF SHA khớp ELF. SDK config QIO sử dụng ROM header DIO theo cơ chế SDK, không tự đổi mode để làm test PASS. Evidence size/map/ELF/BIN: `reliability-artifact-inspection.json`, `stm32-size.txt`, `esp32-size.json`, `esp32-image-info.txt`, `esp32-bootloader-info.txt`.

## 6. ESP package, offsets, manifest và provenance

Package hiện tại giữ cấu trúc:

```text
artifacts/phase1/esp32/
  bootloader/bootloader.bin             @ 0x0
  partition_table/partition-table.bin   @ 0x8000
  tyvor_display.bin                    @ 0x10000
  tyvor_display.elf / .map
  flasher_args.json
  partitions.csv / sdkconfig.build
```

M1 đã được tái hiện offline trước sửa: old package FAIL vì `bootloader/bootloader.bin` không tồn tại. Log `build/reliability-baseline/evidence/esp-package-before.txt`. Bản cũ được giữ ở `build/reliability-baseline/phase1-old-package-*`; không thử flash để tái hiện lỗi.

`validate_esp_package.py` không import serial, không gọi flash/esptool write, không mở thiết bị. Nó resolve mọi metadata reference bên trong package, từ chối absolute/traversal/missing/duplicate reference, kiểm tra boot/app offsets và secondary references nhất quán; image header/chip/segment bounds/XOR checksum/appended SHA; app ELF identity; partition binary MD5/alignment/nonoverlap/Flash bounds; CSV name/offset/size đối chiếu binary; app nằm gọn factory; bench UART gate/pins và PSRAM mode config đúng.

| Partition | Offset | Size | End exclusive |
|---|---:|---:|---:|
| nvs | 0x9000 | 0x6000 | 0xF000 |
| phy_init | 0xF000 | 0x1000 | 0x10000 |
| factory | 0x10000 | 0x300000 | 0x310000 |
| ui_assets | 0x310000 | 0xC00000 | 0xF10000 |
| logs | 0xF10000 | 0xF0000 | 0x1000000 |

Flash layout kết thúc đúng 16 MiB. `ui_assets`/`logs` vẫn chỉ reserved, chưa mount/write. PSRAM vẫn octal40 MHz/TYPE_AUTO/IGNORE_NOTFOUND; đây là cấu hình cho module mục tiêu, **không xác minh PSRAM 8 MiB thực tế**.

M2: build bắt đầu xóa receipt thành công trước đó; chỉ sau configure/build/test thành công mới tạo receipt mới. Packager yêu cầu đủ native Debug/Release, STM32 và ESP receipts, kiểm tra hash inputs/compiler/outputs và native exact inventory/count/skips/NDEBUG. Receipt thiếu/cũ/changed output bị chặn; tests=0, skipped hoặc sai inventory bị chặn. Negative tests dùng bản sao tạm trong workspace, không làm hỏng artifacts thật. Đây là kiểm tra correspondence trong quy trình tin cậy, không phải chống người cố ý giả receipt/compiler/SDK.

Manifest schema 2 dùng artifact paths tương đối trong package, source paths tương đối workspace; kiểm tra exact inventory, size và SHA-256. Package có receipts, 21 SVG, before/after logs, source snapshot và reliability diff. Log Phase 1 cũ còn giá trị lịch sử được đặt dưới `evidence/historical` để không nhầm số 33/size cũ với receipt mới. Manifest không tự hash chính nó; danh sách/hash artifact hiện hành là nguồn kiểm chứng, không chèn một hash tự tham chiếu vào báo cáo được chính manifest hash.

Lệnh rerun offline, không flash:

```powershell
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\validate_esp_package.py artifacts\phase1\esp32 --manifest artifacts\phase1\SHA256_MANIFEST.json --workspace .
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\test_package_validation.py --report build\evidence\package-tests.json
```

## 7. Ownership, authority và rủi ro còn lại

ESP bridge là owner duy nhất của feed/poll/TX/session/cache. Diagnostics copy snapshot, protocol/session counters, epochs và ready dưới cùng mutex, unlock rồi render local copy và poll stale trên copy. Không đọc rách timestamp 64-bit từ task UI, không giữ mutex khi logging/render. SDK UART ISR không gọi parser/cache. Error path flush input/reset event queue rồi thoát vòng event để không xử lý lặp các event lỗi cũ và liên tục flush byte mới. Đây là source review + compile evidence, không phải RTOS race detector/runtime latency proof.

STM32 ISR chỉ đưa byte vào bounded RX ring và cập nhật RX-owned counters; main drain trong critical section có restore PRIMASK rồi parse/poll/TX. Tách TX/RX counters giải quyết đúng lost increment đã nêu ở L2; không thêm atomic/lock không cần thiết vào parser. UART candidate vẫn bị gate OFF.

ESP không được gửi STATUS hoặc application ACK ở role UI. STM32 link từ chối STATUS từ UI trước dispatcher, chỉ xử lý allowlisted read-only UI_REQUEST và ACK unsupported. DIAGNOSTIC không thực thi lệnh; HELLO/SESSION_ACK chỉ điều khiển link. Main STM32 tự tạo mock STATUS ARM invalid, không lấy control state từ ESP. Không có RF/ARM/mixer/output write path mới; UI/RF authority vẫn tách biệt.

Rủi ro/giới hạn còn lại:

- Chưa có boot/board/UART hardware test, kiểm tra đúng MCU VE, HSI/PLL/RNG startup/entropy, watchdog/tick/latency, stack/heap runtime hoặc error injection điện. TX 10 ms là bound trong mã, không phải kết quả đo.
- Fresh random boot identity là giả định rõ ràng. Same-ID local/candidate collision được test fail-closed; nếu tái sử dụng chính xác toàn bộ tuple ID+epoch thì packet cũ không thể phân biệt bằng protocol này. CRC/session không phải authentication; peer có thể giả current tag nếu bị xâm nhập. Không có persistent monotonic counter hoặc cryptographic auth trong scope bench.
- Handshake có bounded attempt/cooldown và recovery tests, không bảo đảm real-time convergence dưới nhiễu liên tục, scheduler starvation hoặc malicious peer. Trong lúc replacement chưa có proof, active session cũ tiếp tục theo lease/stale rules; commit mới là thời điểm invalidation bắt buộc.
- PSRAM 8 MiB/Flash vật lý, AMOLED CO5300, FPC/power/QSPI/touch controller/pinout chưa xác minh; chưa có display/touch driver, LVGL hay framebuffer integration. Không suy đoán pinout.
- Chưa có model storage/editor, ADC/mixer/calibration/control loop, RF/CRSF outputs, ARM policy, reliable write transactions/OTA. Các trang UI vẫn read-only fixtures, không phải radio chức năng hoàn chỉnh.
- Bootstrap từ máy trống, long-duration fuzz/sanitizer campaign, hardware-in-loop, bit-identical builds và full extracted SDK hashing chưa được thực hiện. Không gọi native fake-clock/session tests là kiểm thử board.

Không còn HIGH/BLOCKER đã biết trong các đường phần mềm thuộc scope sau test/review này. L1 Git tracking vẫn là việc của người dùng ở bước riêng; các giới hạn hardware nêu trên không được coi là đã đóng chỉ vì build PASS.

## 8. Git diff / WIP và bằng chứng review

Repository Git root là thư mục cha starter; source vẫn `??`, tracked/staged diff rỗng. Không stage/commit để tạo baseline giả. So sánh reliability dựa trên snapshot/hash đã chụp trước sửa, không dựa vào `git diff --stat` rỗng.

- Trước sửa: `build/reliability-baseline/source/`, `source-state.json`, old `SHA256_MANIFEST.json`, v1 regression executables/logs.
- Thay đổi thực: `build/evidence/reliability-source-changes.json`, `reliability-diff.patch` so snapshot ban đầu với source hiện tại; package copy vào `evidence/`.
- Snapshot hiện hành: `artifacts/phase1/evidence/source-snapshot.patch`, source hashes trong `SHA256_MANIFEST.json`, `git-status.txt`, `git-diff-stat.txt`.
- Build/test evidence cuối có receipt tương ứng. Không dùng log đầu phiên còn 33 test làm bằng chứng cho 64 test mới.

Historical implementation/final review/initial audit/TX15 reference giữ nguyên hash theo snapshot trước sửa. Không thay linker region, partition layout, shared STATUS semantic rules hoặc UI fixtures ngoài scope.

## 9. Kết luận và bước tiếp theo

**READY cho Phase 2A: baseline phần mềm reliability đã đạt các cổng bắt buộc.** Bằng chứng: H1/H2 expected FAIL trước sửa và PASS sau; 64 Debug +64 Release/NDEBUG, 21 offline Python tests; both MCU build/link; STM32 ELF/BIN/vector/memory bounds và ESP image/partition/package/manifest correspondence PASS; UART gates vẫn OFF, STM32 control authority giữ nguyên.

Sau khi người dùng review, Phase 2A nên bắt đầu bằng xác minh board revision/schematic/nguồn/level/pin UART và kế hoạch bench riêng; đo boot/entropy/tick/watchdog, PSRAM detected 8 MiB/memtest/heap, UART loss/reset/reconnect trên hai MCU trước khi đổi gate hoặc tích hợp panel. AMOLED/touch chỉ triển khai khi có datasheet đúng module và pinout được xác nhận. Mọi flash/hardware action cần yêu cầu riêng; lần này đã dừng ở sửa source, build/test offline, đóng gói và báo cáo.
