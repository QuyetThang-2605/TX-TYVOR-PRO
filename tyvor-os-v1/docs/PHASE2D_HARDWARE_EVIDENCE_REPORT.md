# TYVOR OS V1 — Phase 2D hardware evidence report

Ngày **10/11/2026, Asia/Bangkok**. Workspace `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1`.

**Kết luận: NOT READY cho hardware bring-up.** Đã hoàn thành hồ sơ tài liệu, power/pin matrix và test plan để review. Chưa có ảnh PCB thực tế, revision match, approved power/net plan hoặc số đo board. Không nâng document/software verification thành hardware PASS.

Deliverables:

- [BOARD_PIN_MATRIX.md](BOARD_PIN_MATRIX.md): power/MCU pin/function/logic/conflict/peripheral/evidence/status; 44 header positions của đúng WeAct-B reference; STM header actual để TBD.
- [HARDWARE_BRINGUP_TEST_PLAN.md](HARDWARE_BRINGUP_TEST_PLAN.md): đủ10 bước, điều kiện vào, PASS/FAIL/STOP, evidence và record; tất cả hardware NOT RUN/BLOCKED.
- Báo cáo này: provenance, verified/unverified findings, thiếu evidence, rủi ro và Git/WIP.

## 1. Phạm vi và baseline

Đã đọc đầy đủ `PHASE2C_HARDWARE_READINESS_REPORT.md`, `BOARD_EVIDENCE_PHASE2C.md`, `BOARD_BRINGUP_CHECKLIST.md`, `MCU_COMMUNICATION_SPEC.md`; đối chiếu các phần source/config/diagnostics liên quan để tránh dùng mô tả cũ làm hardware expectation.

Đầu phiên xác minh Phase2C **156 artifacts /123 source hashes**,0 mismatch. Manifest SHA256 `a3591e6cfd16e6636f692897a5a8eef86b5b4499a0d3664f71cb926b4ba5acfd`. Snapshot123 source hiện hành và manifest lưu riêng trong `build/phase2d/baseline/`; baseline check ở `build/phase2d/baseline-verification.json`.

Trong Phase2D chỉ tra cứu public documents, đọc source/artifacts, tạo ba Markdown và evidence cục bộ. **Không thay firmware/source/config, không build firmware mới, không flash/erase/open serial/debug attach, cấp nguồn/USB để thử, nối hai board, đổi UART gates/RF, sửa STABLE, commit/push/deploy hoặc Git global.** Native177+177/build PASS là evidence lịch sử Phase2C, không phải test chạy lại/hardware result trong Phase2D.

## 2. Đối chiếu đúng sản phẩm và nguồn

### STM32F407VET6 Mini Type-C / ThegioiIC

Đúng trang [Mạch STM32F407VET6 Type-C](https://www.thegioiic.com/mach-stm32f407vet6-type-c), mã hiển thị **IC44303**, product ID44303, ảnh tham chiếu64729. HTML có `manufacturer_code=""`, `sheetfile=""`, `sheetname=""`; chưa thấy schematic/revision link trong snapshot đã đọc. Supplier field là OEM, còn property brand là STMicroelectronics; những trường này chưa xác định maker/revision của PCB đã nhận.

**Phát hiện mới:** cùng trang ghi thuộc tính “Điện áp cấp” là **5V**, còn `desc_small` và mô tả ghi **1.8–3.6V**. Đây là dữ kiện seller **VERIFIED**, nhưng phạm vi board-input và silicon VDD chưa được tách rõ bằng power schematic/regulator. Có thể hai giá trị thuộc hai domain khác nhau; chưa đủ căn cứ khẳng định board sai hoặc chọn một giá trị để cấp nguồn. Board input limits, connector roles, rail/current/backfeed remain **UNVERIFIED**.

Tên người dùng “Mini Type-C” liên hệ với IC44303 là **INFERRED** cho đến khi có order/photo/PCB identity. Không lấy CAN485/Core407V/Mini-USB cùng MCU để điền schematic hoặc header map.

### WeAct ESP32-S3-B N16R8 / Điện tử Tuyết Nga

Đúng trang [WEACT Thu Phát Wifi BLE ESP32-S3-B N16R8](https://dientutuyetnga.com/products/weact-thu-phat-wifi-ble-esp32-s3-b-n16r8). Metadata: product ID**1064270597**, variant**1146593109**, SKU `TRẮNGA7-XXX`; SKU là trường inventory seller, không phải revision PCB/WeAct. Trang nêu variant16MB Flash/8MB PSRAM và liên kết trực tiếp repository WeAct. Các seller fields/link là **VERIFIED**; actual variant/capacity/revision đã nhận **UNVERIFIED**, association **INFERRED**.

Đã đối chiếu [schematic đúng ESP32S3-B của WeAct](https://github.com/WeActStudio/WeActStudio.ESP32S3-AorB/blob/c53183ea4aa96c6b7abf2b1dc153bafa22d2b96e/ESP32S3-B/Hardware/ESP32_S3_B_Sch%20.pdf), đọc text và xem bản render. Reference gồm header44 positions, USB-UART/native USB, LDO/power paths, buttons, Flash và layout image. Không thấy revision rõ trên một trang này; chưa có ảnh board thực tế để match. Không dùng schematic A hoặc DevKitC compatibility như bằng chứng actual nets.

Current upstream tree vẫn commit `c53183ea4aa96c6b7abf2b1dc153bafa22d2b96e`. PDF815817 bytes; blob `d27f6ad987b174aa9515e7207ec9a8fa4be45e95` khớp git blob hash tính từ PDF; SHA256 `910a41eea1b8d111e8efc78667eb01d95a30b6e7ae2b9ce7567afcafd3913e68`. Commit/hash là identity tài liệu, không phải revision PCB.

### Silicon / SDK / boot references

- [ST DS8626 Rev12](https://www.st.com/resource/en/datasheet/stm32f407ve.pdf): đối chiếu pin/AF, memory VE, VDD và boot pins; chỉ silicon reference.
- [ST AN2606 Rev71, mục34](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf): đọc các phần STM32F40xxx/41xxx V3.x/V9.x, USART và DFU/HSE. Không tự gán ROM version/HSE cho board.
- [Espressif S3 datasheet v2.2](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf), pin/power/strap và Table5-2; [IDF5.5.1 GPIO restrictions](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/gpio.html), [boot mode/DTR/RTS](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html), [Flash/PSRAM config](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/flash_psram_config.html). Những tài liệu này không xác nhận actual board routing hoặc dung lượng vật lý.

Seller/official sources và fetched hashes lưu trong `build/phase2d/sources/retrieval.json`; derived product fields, WeAct identity JSON, PDF text và PNG cũng lưu ở đó. Tải trực tiếp7/9 URL thành công; hai ST PDF download timeout, đã tra đọc được qua web công khai chính thức ở các phần nêu trên. Không ghi hai file timeout như PDF đã download/hash thành công. PyMuPDF1.26.6 có sẵn từ Phase2C dùng để đọc/render PDF cục bộ; không cài tool mới/global.

## 3. VERIFIED / INFERRED / UNVERIFIED

| Hạng mục | VERIFIED trong phạm vi nào | INFERRED | UNVERIFIED actual hardware |
|---|---|---|---|
| STM seller identity | URL/title/IC44303/product ID/field values | Liên hệ “Mini Type-C” với item user có | Maker/revision/markings/order match |
| STM power | Seller fields5V và1.8–3.6V; silicon VDD reference | Chưa chọn power-domain interpretation | USB input limits, LDO, polarity/current, ground/backfeed/testpoints |
| STM UART | MCU PA2/PA3 USART2 AF7; source candidate OFF | Không suy header | Header route/free pins/logic levels/conflicts |
| STM USB/debug/boot | MCU AF/ROM boot reference; source app dùng HSI và không có USB device stack | Không suy connector hỗ trợ serial/DFU | USB data routing, HSE, ROM version, SWD header/straps |
| WeAct seller/source link | Product/variant/SKU/link và pinned B PDF identity | Seller B board có thể khớp reference | Actual board B/revision match, component population |
| WeAct header/net | 44 reference positions trong B PDF | Không chuyển tọa độ REF-L/R thành actual map | Actual header numbering/orientation/continuity |
| ESP power/UART0 | B reference VCC_3V3, U0 nối U7, two USB/power bypass paths | Nominal3.3V logic trên actual cần đo | Regulator ratings/links/backfeed/VOH/VOL/VIH/VIL/current |
| ESP reserved | Espressif26–32 memory và R8 octal thêm33–37; USB19/20; straps0/3/45/46 | Không suy GPIO free chỉ từ header | Actual chip/board revision/efuse policy/loads |
| Flash/PSRAM | Seller claims +image/config16MB/octal-PSRAM settings | Variant name gợi ý N16R8 | Actual IDs/capacity8MiB PSRAM/memtest/heap |
| Firmware/software | Baseline manifest/config/source; gates OFF; protocol2/monitor2 | Không suy image đang có trên board | Current flashed image, boot/RF policy, actual timing/stack/heap |

Không có “actual power/pin VERIFIED” nào được tạo trong phiên này. [Pin matrix](BOARD_PIN_MATRIX.md) ghi đầy đủ tám cột yêu cầu, source IDs và V/U scope. **Actual UART assignment vẫn UNASSIGNED:** STM TX/RX TBD, ESP TX/RX TBD; UART1 chỉ là software peripheral hiện có, pins=-1. GPIO17/18 và PA2/PA3 không tự trở thành lựa chọn được phê duyệt.

## 4. Rủi ro nguồn, boot và UART

| ID | Mức ưu tiên đối với readiness | Bằng chứng / tác động | Điều kiện đóng |
|---|---|---|---|
| D-P1 | HIGH evidence gap | IC44303 có source labels5V/1.8–3.6V chưa phân domain; exact power path/LDO thiếu | Schematic/regulator/rails/input limits khớp PCB, approved power plan và số đo |
| D-P2 | HIGH evidence gap | WeAct reference có hai USB nguồn, diode/bypass links và LDO không ghi rõ part/rating; actual population chưa biết | Ảnh/link population/net evidence, regulator datasheet, USB/probe backfeed review, power tests |
| D-B1 | HIGH evidence gap | Chưa biết factory/current firmware; normal power-on có thể chạy image chưa được xác nhận bench/RF policy | Known ROM/hold/reset hoặc trusted firmware boot plan đúng actual board; identity/authorization trước runtime |
| D-U1 | HIGH evidence gap | Chưa actual UART pins/levels/ground; WeAct U0 còn nối USB-UART, reserved/strap pins có thể bị chọn nhầm | Actual matrix/net/level/conflict evidence; approved drawing; gates giữ OFF đến nhiệm vụ riêng |
| D-B2 | MEDIUM preparation risk | STM app không có USB device stack; ROM DFU yêu cầu HSE/boot evidence khác app HSI | Tách SWD/probe/USB bridge/native USB/ROM/app test expectations |
| D-B3 | MEDIUM preparation risk | ESP DTR/RTS nối reset/BOOT trong reference; opening host port có thể đổi boot/reset, GPIO45 USER cũng là strap | Exact reset/strap/eFuse read-only evidence, tool/cable/boot policy được review |
| D-T1 | HIGH evidence gap | Chưa đo deadline/IRQ/critical section/heap/stack/PSRAM; native PASS không bảo đảm actual | T10 measurement coverage và memory/PSRAM tests đạt criteria đã review |
| D-D1 | MEDIUM documentation issue | Checklist/spec còn đoạn Phase2B và lỗi mã hóa; có thể làm sai test expectation | Dùng errata bên dưới +source/pinned artifacts; historical files giữ nguyên trong Phase2D |

Đây là severity của **thiếu hồ sơ/risks**, không phải khẳng định đã quan sát hư phần cứng. Không lấy chip5V-tolerant pins làm lý do dùng5V UART; không cấp điện theo net labels/source marketing; không đổi straps/eFuse để vượt lỗi. Không tự nối GND hai board trước review power/ground topology.

### Errata tài liệu đầu vào, không sửa baseline

`BOARD_BRINGUP_CHECKLIST.md` còn nói STM callback block10 ms; phần đầu `MCU_COMMUNICATION_SPEC.md` còn nói ISR RX-only, ESP cache mutex và synchronous TX. Những đoạn đó không mô tả source Phase2C hiện hành. Phần append Phase2C spec và một cụm trong report cũng có dấu `?` do lỗi mã hóa văn bản; không dùng đoạn lỗi làm execution instructions.

Đối chiếu source actual: STM `board_uart_write` enqueue `tyvor_async_tx_write`; ISR TXE/TC +RX; main/task-owned4-pending+1-active queue, deadline20 ms. ESP bridge sole owner, `xQueueOverwrite` publication, UI `xQueuePeek` bản copy; không còn application cache mutex. Gates OFF đã kiểm tra qua source/config/artifact evidence. Source không đổi trong Phase2D; cần clean-up living docs trong nhiệm vụ documentation riêng sau review để không viết lại hồ sơ/hash lịch sử trong phiên này.

## 5. Hồ sơ ảnh/đo còn thiếu

Mỗi board cần:

1. Ảnh actual mặt trước/mặt sau, không che linh kiện, ảnh toàn board có orientation/ruler; silkscreen/logo/version/PCB code, order/product variant. Không lấy ảnh trên seller page làm proof board đã nhận.
2. Close-up MCU marking đầy đủ: STM32F407VET6 và package; ESP32-S3R8/package; external Flash/USB bridge/LDO marking, diode/bypass/jumper population, connector/button labels và pin1/header numbering.
3. Annotated map actual board→schematic/net: USB power/data, regulator input/output, ground, boot/reset/debug và UART candidates. Board revision mismatch phải lập delta record, không dùng reference như exact match.
4. Trong hardware job được cho phép riêng: unpowered continuity/resistance/polarity records tại approved testpoints; sau power review mới có rail/current/idle/data-level/backfeed measurements. Không có đo kiểm nào đã được thực hiện ở Phase2D.
5. Physical MCU/Flash/PSRAM identity, boot/reset/security state, runtime firmware identity/readback, clock/WDG, stack/heap, timing/IRQ evidence và approved interconnect drawing/reviewer/date.

Nếu exact STM schematic chưa có, danh sách cần lấy từ vendor/maker: schematic revision khớp IC44303 PCB, BOM/regulator limits, USB/power isolation/bypass, boot/debug/UART header net map, crystal/bootloader assumptions. Với WeAct cần actual revision/photo match và U4/Flash/USB bridge markings/population; PDF B reference không ghi hết part/rating. Phiên này không gửi email/tin nhắn cho vendor hoặc người khác.

## 6. Test plan và những dependency chưa có

[HARDWARE_BRINGUP_TEST_PLAN.md](HARDWARE_BRINGUP_TEST_PLAN.md) có đủ:

| Step | Phạm vi | Hardware result hiện tại |
|---|---|---|
| T01 | Visual/unpowered identity/net record | NOT RUN/BLOCKED |
| T02 | USB power từng board riêng | NOT RUN/BLOCKED |
| T03 | Rail3V3/ripple/overshoot | NOT RUN/BLOCKED |
| T04 | SWD/USB probe, bridge/native/ROM/app distinction | NOT RUN/BLOCKED |
| T05 | Trusted boot diagnostics/reset/watchdog | NOT RUN/BLOCKED |
| T06 | Offline image +physical firmware identity | Offline hash PASS; hardware NOT RUN/BLOCKED |
| T07 | ESP physical Flash/PSRAM detection và memtest | NOT RUN/BLOCKED |
| T08 | Single-board UART loopback | NOT RUN/BLOCKED |
| T09 | Two-MCU protocol integration | NOT RUN/BLOCKED |
| T10 | Measured timing/load/recovery/safety | NOT RUN/BLOCKED |

**Hardware PASS0, observed electrical FAIL0, steps NOT RUN/BLOCKED10.** Không đo nên chưa có electrical result. PASS/FAIL criteria, stop conditions và raw-evidence requirements đều ghi theo từng bước; power/current/ripple/logic thresholds chưa có nguồn đúng board được để TBD và ngăn execution.

Dependency quan trọng: current firmware gate OFF không có loopback echo mode; self-loop HELLO phải bị session collision rejection, không phải session PASS. PSRAM byte detection log không thay allocation/address/data memtest; memtest fixture chưa triển khai. Current mock inputs trên STM không phải physical input loss fixture. `board_timing` chưa tự đo đủ correlated release→output completion/ISR/critical-section peak; ESP work metric bỏ publication/WDT/delay. Những phần thiếu cần nhiệm vụ software/diagnostic riêng, không được ghi như đã tồn tại hoặc dùng native timing thay board measurements.

Không yêu cầu nạp firmware như một bước ngầm của test plan. Nếu cần bench image/fixture hoặc mở UART sau này, phải dừng tại gate và có nhiệm vụ hardware/firmware cụ thể được review trước. RF không có gate mở trong plan này.

## 7. Kiểm chứng hồ sơ và Git diff/WIP

Phase2D thay đổi **đúng3 file mới trong `docs/`**; toàn bộ123 baseline files giữ nguyên: catalog hiện126 =123 unchanged +3 new. Source STM/ESP/shared/tests, CMake/toolchain, gates, sdkconfig/partition/linker, historical reports và package Phase1/2A/2B/2C không sửa.

Git root vẫn là `D:\TYVOR\TYVOR-OS-V1-Starter`; workspace untracked. Tracked/staged diff rỗng không thể hiện phần WIP; không stage/commit để tạo baseline giả. Diff xác định bằng frozen catalog/hash và danh sách3 Markdown mới. Các raw research/snapshot/check files chỉ nằm dưới ignored `build/phase2d/`.

Evidence: `baseline-verification.json`, `source-changes.json`, `phase2d-docs.patch`, `git-status.txt`, `document-validation.json` và `EVIDENCE_MANIFEST.json` trong `build/phase2d/`; manifest đó ràng buộc3 Markdown/research evidence, không thay manifest firmware cũ và không tự hash chính nó. Kiểm tra UTF-8, relative links, eight-column matrix,44 header-reference positions,10 test sections/criteria và baseline hashes bằng offline checks. Không viết unit tests hoặc rebuild firmware cho thay đổi chỉ tài liệu; không tuyên bố hardware đã PASS.

## 8. Điều kiện còn thiếu và kết luận

**NOT READY**. Trước một hardware task riêng cần: actual board/revision/photo và net/power evidence; chốt power/boot/debug plan cùng ngưỡng đo; actual pin matrix/logic/ground compatibility; trusted image identity và backup/read-only/rollback policy; diagnostic fixtures phù hợp; test acceptance criteria/instrumentation và phạm vi thao tác được review. Trước khi nối hai MCU còn cần single-board tests PASS và approved interconnect drawing.

Tài liệu reference mới làm rõ điểm phải đối chiếu, chưa thay được evidence thực tế. Không flash, cấp nguồn, nối board, mở UART gates/RF hoặc thay STABLE trong Phase2D. **Đã dừng ở hồ sơ để người dùng review.**
