# TYVOR OS V1 - Board bring-up checklist

Ngày đối chiếu tài liệu: **10/11/2026, Asia/Bangkok**. Đây là kế hoạch kiểm tra, không phải biên bản hardware PASS hoặc cho phép đấu dây/flash. Hai UART gates giữ OFF; không bật RF/actuator. Chưa có board được cấp nguồn/đo/đọc/nạp trong phiên Phase 2B.

## Quy ước và nguồn bằng chứng

**SOFTWARE VERIFIED** chỉ áp dụng source/config/ELF/BIN/test. **UNVERIFIED** là thông tin board/net/điện/timing chưa xác minh. Ô trống bên dưới phải được điền bằng ảnh marking, schematic đúng revision, số đo/log và người review trước khi chuyển trạng thái. Compile-check không xác nhận đường mạch.

Nguồn chính thức đã đọc:

- [ST STM32F407VE product](https://www.st.com/en/microcontrollers-microprocessors/stm32f407ve.html) và [DS8626 Rev 12, March 2026](https://www.st.com/resource/en/datasheet/stm32f407ve.pdf): silicon/memory/clock/boot/alternate-function reference. Chúng không phải schematic của board Mini Type-C cụ thể.
- [WeActStudio ESP32S3-AorB repository](https://github.com/WeActStudio/WeActStudio.ESP32S3-AorB) và [README chính thức](https://github.com/WeActStudio/WeActStudio.ESP32S3-AorB/blob/main/README.md): phân biệt A dùng module và B dùng ESP32S3-R8. Chưa đối chiếu schematic/revision của board người dùng; không suy pin/header từ board A hoặc tên N16R8.
- [Espressif IDF 5.5.1 Flash/PSRAM configuration](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/flash_psram_config.html): chọn cấu hình theo linh kiện thực tế; cấu hình không chứng minh dung lượng board.
- [Espressif USB Serial/JTAG](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/usb-serial-jtag-console.html), [boot mode reference](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html), [reset reason API](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/system/misc_system_api.html), [watchdogs](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/system/wdts.html), [heap diagnostics](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/system/heap_debug.html): SDK/silicon reference, không xác nhận connector/net của WeAct board cụ thể.

Không dùng sơ đồ seller/ảnh board tương tự làm pinout đã chốt. Chưa đọc/đối chiếu được schematic đúng revision của hai board trong phiên này. Mọi connector/header/nguồn/GPIO thực tế dưới đây đều **UNVERIFIED**.

## STM32F407VET6 Mini Type-C

| Mục | Software/reference hiện có | Bằng chứng cần thu thập trước thao tác | Board status |
|---|---|---|---|
| Identity/revision | Target build STM32F407xx/VE | Maker/product link, revision silkscreen, ảnh hai mặt/MCU marking; schematic đúng revision và hash | UNVERIFIED |
| Power | Chưa xác nhận board regulator/power path | Connector pin numbering, polarity, regulator/input limits, protection, USB/external supply backfeed paths, reference ground; phương án current limit theo schematic | UNVERIFIED |
| Boot mode | ELF tại Flash0x08000000 | Đối chiếu straps/buttons/net với boot-mode table; ghi initial state/normal boot path, không tự nối straps | UNVERIFIED |
| System clock | SYSCLK HSI16 MHz; HSI PLL48 phục vụ RNG trong source | Đo tick/clock, quan sát PLL/RNG startup/error paths, đúng nguồn clock; không suy crystal từ tên board | UNVERIFIED |
| UART mapping | Candidate branch compile-check, gate OFF | Net MCU/peripheral/AF ->header thực, electrical domain, idle level, conflicts; schematic và continuity evidence được review | UNVERIFIED |
| Watchdog | IWDG ON, nominal khoảng2 s theo cấu hình; main health checkpoint | Reset-cause log, measured reset interval, hang/fault test plan và debugger effects; diagnostic không được feed/disable WDT | UNVERIFIED |
| Flash/RAM | VE512 KiB Flash, SRAM128 KiB +CCM64 KiB trong linker; vectors/load bounds SOFTWARE VERIFIED | MCU identity/memory evidence trên board; main/IRQ stack watermark, fault logs, correct reset vector/boot mapping | UNVERIFIED |
| Diagnostics access | Global `tyvor_diagnostics`, reset flags, current MSP sample | Debug interface/net đúng board, tool access plan; chưa attach debugger hoặc giả định header pin | UNVERIFIED |

Không có sơ đồ nối STM32 TX/RX/nguồn trong tài liệu này. Candidate PA2/PA3 trong source là giả định cũ bị gate OFF, không phải pinout được phê duyệt.

## WeAct ESP32-S3-B N16R8

| Mục | Software/reference hiện có | Bằng chứng cần thu thập trước thao tác | Board status |
|---|---|---|---|
| Identity/revision | App target S3; tên N16R8 do người dùng cung cấp | Ảnh marking chip/Flash/board, product/revision và schematic đúng B; không dùng DevKitC/board A thay thế | UNVERIFIED |
| Power | Chưa xác nhận regulator/USB/external power paths | Pin/polarity/rated rails/protection/backfeed theo schematic; kế hoạch cấp nguồn/current limit riêng | UNVERIFIED |
| Boot mode | Silicon/SDK boot reference có sẵn | Kiểm tra buttons/straps/connector routing đúng board; initial boot behavior/logs; chưa thay strap/eFuse | UNVERIFIED |
| USB flashing preparation | Console config USB Serial/JTAG; offline package chip S3 | Xác định connector đúng, routing/controller, enumeration/driver/cable plan, backup/rollback và artifact review; **chưa cho phép chạy flash** | UNVERIFIED |
| Flash16 MB | Config/header16 MB; partition end0x1000000 SOFTWARE VERIFIED | Linh kiện/ID và detected physical capacity, read/verification plan được phê duyệt | UNVERIFIED |
| PSRAM8 MiB | Octal40 MHz/TYPE_AUTO, IGNORE_NOTFOUND=y | Detected8388608 bytes, memtest/heap allocation and failure behavior; boot thành công đơn thuần không chứng minh PSRAM | UNVERIFIED |
| UART mapping | Gate OFF; RX/TX=-1 | Exact free GPIO/peripheral/header nets/electrical domain; tránh strap/Flash/PSRAM/USB conflicts theo đúng schematic | UNVERIFIED |
| Heap/stack | Free/min/largest internal heap và task high-water APIs có trong source | Runtime logs under bench load, canary/heap checks, stack budget và headroom được review | UNVERIFIED |
| Watchdog/reset | Task WDT configured; reset reason/status collection | Subscription/reset-cause/hang test logs; diagnostics không thay đổi safety hoặc watchdog behavior | UNVERIFIED |

Firmware chỉ có counter trong phạm vi boot/process đang quan sát. Lifetime reset/power-cycle totals là UNVERIFIED, không có Flash/retention driver giả định. Không thay cấu hình eFuse hoặc điện áp Flash/PSRAM để thử vượt lỗi.

## Communication/electrical record - chưa điền pin

| Trường cần phê duyệt | Giá trị hiện tại |
|---|---|
| STM32 board maker/revision/schematic | UNVERIFIED |
| ESP32 board revision/schematic | UNVERIFIED |
| STM32 UART peripheral / TX net / RX net / header position | UNVERIFIED |
| ESP32 UART peripheral / TX net / RX net / header position | UNVERIFIED |
| I/O voltage domains / level compatibility / reference-ground plan | UNVERIFIED |
| Power source / connector / polarity / power sequencing / backfeed isolation | UNVERIFIED |
| Approved interconnect drawing, reviewer, date | UNVERIFIED |
| AMOLED/touch FPC/power/interface/controller/pinout | UNVERIFIED; ngoài scope Phase2B |

Không điền GPIO/chân nguồn/kết nối điện từ suy đoán. Không có bước tự nối TX/RX/nguồn hoặc tự thay UART gate trong script build/package.

## Điều kiện bắt đầu hardware bring-up

1. Người dùng review software report, manifest, tests và các hạn chế; mở một công việc hardware riêng với phạm vi thao tác rõ ràng. Lần này chỉ chuẩn bị tài liệu/software.
2. Có board identity/revision và schematic/net evidence khớp board thực; nguồn/điện áp/ground/backfeed/boot/debug mapping được người có trách nhiệm review, điền bảng trên.
3. Kế hoạch kiểm tra từng board độc lập, diagnostics và rollback được chốt. Chưa nối hai MCU hoặc panel khi power/pin domains còn UNVERIFIED.
4. Trước UART enable: review TX blocking/WCET (hiện callback STM32 có thể block10 ms trong tick10 ms), chọn/kiểm chứng transport schedule phù hợp, error/reset injection plan và đo deadline; giữ failsafe/watchdogs/RF guards.
5. Trước bất kỳ flash/serial/debug attach/đổi gate nào phải có yêu cầu mới cho thao tác cụ thể. Không coi checklist này hoặc build PASS là sự cho phép tự thao tác hardware.

**Hardware readiness hiện tại: NOT READY cho đấu nối/flash/enable UART.** Software bench và checklist đã chuẩn bị; toàn bộ dấu xác nhận board/timing/PSRAM/panel còn UNVERIFIED.
