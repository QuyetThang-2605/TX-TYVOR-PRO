# Phase 2C timing & hardware readiness (software bench)

Current review: [PHASE2C_HARDWARE_READINESS_REPORT.md](docs/PHASE2C_HARDWARE_READINESS_REPORT.md).
Build/test/package instructions: [BUILD_PHASE2C.md](docs/BUILD_PHASE2C.md).
UART gates OFF; no RF, device flash, AMOLED/touch driver or hardware timing PASS.

# TYVOR OS V1 — Phase 1 bench firmware

Independent dual-MCU starter for STM32F407VET6 + WeAct ESP32-S3-B N16R8. **WIP / BENCH ONLY. No RF output, flight control or ARM command.**

Phase 2B adds a gated transport adapter, deterministic two-way UART fault injection, request-correlated read-only Core snapshots, diagnostics and seven simulator pages. The native suite has 154 cases per Debug/Release configuration: the previous 120 plus four proven regressions, 26 bench tests and four integration scripts. Both MCU builds pass; hardware remains UNVERIFIED.

**BENCH_ARMED is software state only. Channel outputs are memory values; RF and aircraft ARM remain unavailable.**

## Layout

- `shared/`: protocol/CRC/parser, common session link, STATUS validity/snapshot, display-independent UI model.
- `stm32/engine/`: portable Core modules, validation/serialization and RAM storage adapter; no RF/actuator backend.
- `stm32/`: ST startup + HAL/CMSIS dependency integration, F407VE linker, HSI/SysTick, BSP, watchdog, mock publisher, bounded UART transport.
- `esp32/`: IDF 5.5.1 S3 app, 16 MB partitions, octal PSRAM configuration, bridge/diagnostic tasks, status cache and stack/heap instrumentation.
- `sim/`: always-executed CHECK tests, command-driven Core simulator and automatic scenarios; previous Phase 1 SVG exporter retained.
- `scripts/`: isolated tools, build/test receipts, offline package validation and dependency locks.
- `docs/`: audit, protocol contract, build instructions and Phase 1 report.

## Build on Windows

From the project directory:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\bootstrap-tools.ps1 -IncludeEsp32
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-phase2b.ps1 -Target all
build\phase2b\native-release\tyvor_core_sim.exe --script sim\bench_scenarios\bench_fault_reconnect.txt
.tools\espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe scripts\phase2b_artifacts.py inspect
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\phase2b_artifacts.py package
```

Tools remain isolated under `.tools` without changing system PATH or Git global configuration. See [Phase 2B build/replay](docs/BUILD_PHASE2B.md), [bench report](docs/PHASE2B_BENCH_REPORT.md), [MCU communication specification](docs/MCU_COMMUNICATION_SPEC.md), and [board bring-up checklist](docs/BOARD_BRINGUP_CHECKLIST.md). New evidence: `build/phase2b/evidence/`; package: `artifacts/phase2b/`; 35 renderer previews: `artifacts/phase2b-ui/`. Phase 1/2A artifacts and reports remain historical snapshots.

Root CMake is native-only. STM32 and ESP32 use their own CMake projects and build directories. Release tests execute the same checks with `NDEBUG` enabled.

## Hardware gates

Both firmware builds keep bridge UART pins **disabled by default**. STM32's optional USART2 PA2/PA3 candidate is not a verified Mini Type-C pinout. ESP pins default to -1; GPIO17/18 are not assumed. Check actual board revision, routing and electrical levels before enabling either side. HSI 16 MHz avoids assuming the STM32 board crystal; timing still needs bench measurement.

No AMOLED or touch driver is integrated. The planned panel is 368x448 / CO5300; exact module datasheet, QSPI electrical/interface details, FPC pinout, initialization and touch controller remain unverified. Do not infer CST820 or a connector pin assignment from the earlier scaffold notes.

STM32 base STATUS remains MOCK with aircraft ARM UNKNOWN. Core safety state, calibrated inputs and gated channel monitor values are separate bench data. Model selection is local to the Core/simulator; ESP read-only requests cannot select a model or change safety. Session recovery is tested over simulated UART. Protocol v2 rejects v1; Core monitor revision 2 uses receiver-issued read tokens inside DIAGNOSTIC to bound freshness across delayed delivery. Physical UART, entropy quality, PSRAM, clocks and panel remain unverified.

No automatic flash, commit, push or deployment. Do not overwrite TYVOR V1 STABLE or modify an old EdgeTX repository.
