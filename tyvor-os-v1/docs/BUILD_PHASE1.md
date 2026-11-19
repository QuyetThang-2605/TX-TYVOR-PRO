# Phase 1 build and review

Windows x64, PowerShell 5.1. Project path must not contain whitespace for ESP-IDF. Portable tools/dependencies live under `.tools` on D:. No system PATH/Git global change is needed. Build scripts contain no device flash/monitor actions.

## Install or verify

```powershell
cd D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\bootstrap-tools.ps1 -IncludeEsp32
```

This process-only execution-policy option does not change persistent machine/user policy. Bootstrap downloads versioned upstream archives, checks `scripts/tools.lock.json` SHA-256, extracts into `.tools`, installs CPython 3.11.13 through uv with no registry/bin integration, and creates a private ESP-IDF virtualenv. To verify current cached archives/install markers without network or installation:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\bootstrap-tools.ps1 -IncludeEsp32 -VerifyOnly
```

Tested versions: CMake 3.30.9, Ninja 1.12.1, LLVM-MinGW 20250613 / Clang 20.1.7, Arm GNU 14.2.Rel1 / GCC 14.2.1, CMSIS Core 5.9.0, CMSIS-F4 2.6.10, STM32F4 HAL 1.8.3, uv 0.8.22, CPython 3.11.13, ESP-IDF 5.5.1, Xtensa GCC 14.2.0 (`esp-14.2.0_20241119`), ULP binutils 2.38_20240113, ROM ELF pack 20241011. Python packages are recorded/pinned in `scripts/esp-idf-python-lock.txt`.

The IDF source archive plus seven pinned submodule archives is sufficient for this UART-only application and CMake component registration. It is **not a complete installation for arbitrary Wi-Fi/Bluetooth/OpenThread examples**. Add their matching dependencies explicitly if such scope is authorized later. Compiler/ULP/ROM pack hashes were also compared with upstream `tools/tools.json`. The large full-history ESP-IDF release archive timed out; its partial download is unused.

Bootstrap's archive/install checks have run on this machine. The complete bootstrap flow has not been replayed from an empty installation. Build instructions pin inputs and tool versions; bit-identical binaries across host paths/timestamps are not promised.

## Build

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-phase1.ps1 -Target native
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-phase1.ps1 -Target stm32
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-phase1.ps1 -Target esp32
```

`-Target all` runs all three. Native builds Debug and Release/NDEBUG, runs the same 64 CTest cases in both and exports 21 SVG virtual screens. Logs/JUnit live in `build/evidence`. Root CMake remains host-only; STM32 and ESP32 have separate projects/build trees. Scripts check every external process exit code and stop on a failed step. Native test/compiler flags include `-Wall -Wextra -Wpedantic -Werror`; CHECK assertions execute in both configurations.

Each target invalidates its old successful receipt before configure/build. A successful receipt binds source/config/build-driver inputs, compiler executable SHA/version and output hashes. Native receipts require the exact CMake test inventory, zero failures/skips, executable/argument correspondence, and correct NDEBUG mode in both test compilation commands. Changing an input/output or failing a later build makes packaging fail until a successful matching build is recorded. Portable Python is consequently required for native/STM32 as well as ESP builds. SDK source archives are pinned by the tool lock; receipts do not hash every extracted SDK file or promise bit-identical builds across machines.

For an interactive shell, use a process with script execution enabled and dot-source `scripts/activate.ps1 -Target native|stm32|esp32|all`. It prepends tool paths for that process only. ESP activation sets `IDF_PATH`, `IDF_TOOLS_PATH`, private Python path, ROM symbols, and disables network component-manager/submodule fetching. Source archive hashes replace Git submodule checks. A process-local Git search ceiling prevents IDF from deriving version metadata from the empty parent WIP repository; use a separate normal shell for Git.

STM32 uses ST's pinned startup/vector table and SystemInit. Linker map: Flash at 0x08000000, 512 KiB; SRAM at 0x20000000, 128 KiB; CCM at 0x10000000, 64 KiB. A 4 KiB main stack is reserved; no heap is used by TYVOR's STM32 code. SYSCLK remains internal HSI 16 MHz; an HSI-fed PLL (M=16, N=192, Q=4) supplies 48 MHz to HAL RNG for a fresh boot identity. No external crystal is assumed. RNG/PLL failure prevents link startup; this clock/entropy path requires later board validation. IWDG is enabled by default, nominal ~2 s, fed only after owner-loop/tick progress. No measured watchdog timing is claimed. UART TX is bounded to 10 ms for protocol-v2 maximum 64-byte frames.

The USART2 PA2/PA3 mapping is a **conditional candidate from MCU alternate functions, not verified board routing**. `TYVOR_UART2_PA2_PA3_VERIFIED=OFF` is the default. The candidate branch is type-checked in an unlinked object target. Do not enable it based on this compile result. If the actual board needs different UART pins/peripheral, adapt BSP after schematic review. RX uses a 256-byte ISR ring; parsing/logical work stays in main. No ADC/mixer/output/RF code exists.

ESP configuration targets S3, 16 MB flash, QIO flash, octal PSRAM at 40 MHz and USB Serial/JTAG console. SPI flash frequency comes from the checked SDK config (80 MHz). PSRAM size is detected at boot; missing PSRAM is logged/tolerated for this bench scaffold. `sdkconfig.defaults` plus custom partitions reproduce a clean configuration; generated `sdkconfig` is ignored and packaged as evidence. Existing `sdkconfig` overrides defaults, so review it before interpreting a later build as equivalent. No OTA partitions/updater or filesystem mount is implemented.

ESP UART is disabled unless `CONFIG_TYVOR_UART_PINOUT_VERIFIED=y` and explicit distinct valid RX/TX pins are supplied. Default GPIO values are -1. The backend remains in the translation unit for compiler API checking. No GPIO17/18/panel/touch mapping is assumed. The bridge task owns parser/TX/cache mutation; diagnostics copies under a mutex and rechecks freshness before rendering. Heap/stack logs are instrumentation, not measurements made in this implementation session.

## Package and inspect

```powershell
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\package_artifacts.py
```

Packaging requires matching successful receipts for all four configurations and validates the ESP package before publishing. ESP files retain SDK-relative subdirectories: `esp32/bootloader/bootloader.bin` and `esp32/partition_table/partition-table.bin`; the app remains `esp32/tyvor_display.bin`. Offsets are 0x0, 0x8000 and 0x10000. The previous package is preserved under `build/reliability-baseline/phase1-old-package-*` before replacement.

Artifacts go to `artifacts/phase1`, previews to `artifacts/ui-mock` (also copied into the portable package). Manifest schema 2 records package-relative artifact paths/sizes/hashes and workspace-relative source hashes. Build receipts and before-fix regression evidence are copied into the package. ELF debug information is not flash-image size. SDK logs print suggested flash commands; no script here executes them.

Validate without opening serial or connecting a board:

```powershell
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\validate_esp_package.py artifacts\phase1\esp32 --manifest artifacts\phase1\SHA256_MANIFEST.json --workspace . --report build\evidence\esp-package-final-validation.json
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\test_package_validation.py --report build\evidence\package-tests.json
```

The validator checks every firmware reference, contained relative paths, exact offsets/settings, image segment bounds/checksum/appended hash/chip ID/Flash header, app ELF identity, partition MD5/layout/CSV correspondence, app fit and bench UART/PSRAM configuration. Optional manifest validation checks the complete artifact inventory and source hashes. It does not verify physical Flash/PSRAM or authorize flashing. The 21 Python tests include negative corrupted/missing/unsafe package cases and stale receipt/empty or skipped JUnit rejection.

Protocol v2 requires both endpoints to be updated together. A v1 peer is rejected, never silently downgraded. Read [PROTOCOL_PHASE1.md](PROTOCOL_PHASE1.md) before modifying the session contract.

## Reproduce HIGH regressions

`sim/test_reliability.c` retains a version-1 adapter for the exact pre-fix timeout/reboot probes. The original source snapshot and v1 headers/parser are preserved in `build/reliability-baseline`, with hashes and the old artifact manifest. For example, after native activation:

```powershell
clang -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O0 -Ibuild/reliability-baseline/shared sim/test_reliability.c build/reliability-baseline/shared/tyvor_protocol.c -o build/reliability-baseline/regressions-Debug.exe
build\reliability-baseline\regressions-Debug.exe h1_timeout_suffix
build\reliability-baseline\regressions-Debug.exe h2_sender_reboot
```

Both commands intentionally exit 1 on v1. Repeat compilation with `-O2 -DNDEBUG` for Release. Fixed executables are `build/native-{debug,release}/test_reliability.exe`; both cases exit 0 there. Before/after logs report actual acceptance and exit codes, not only total test counts. Full coverage adds split/deadline boundaries, session loss/collision/reboot scenarios and deterministic randomized UART/reboot streams; see the reliability report. No board is involved.

Open `artifacts/ui-mock/mock-0.svg`, `stale-0.svg` and `no-data-0.svg` to compare HOME states. Pages 0..6 are HOME, Model, Inputs, Mixes, Outputs, Telemetry and Radio Settings. These are static virtual-display exports from a shared C view model, not a desktop input-event emulator or implemented settings editor. The same view model compiles in ESP-IDF and contains no panel/LVGL API.

Third-party source and licenses stay in their isolated SDK archive trees: CMSIS `LICENSE.txt`, ST device/HAL `LICENSE.md`, ESP-IDF `LICENSE` and each submodule's license. LLVM, Arm GNU and uv distributions include their upstream license notices. No third-party binary firmware was copied from EdgeTX/TYVOR STABLE.
