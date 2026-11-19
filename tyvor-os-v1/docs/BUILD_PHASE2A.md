# Build and exercise the Phase 2A software bench

Use the same pinned portable tools described in `BUILD_PHASE1.md`. This phase needs no new SDK install, board, serial connection, panel or network dependency. Run from the workspace root in Windows PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-phase2a.ps1 -Target native
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-phase2a.ps1 -Target stm32
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-phase2a.ps1 -Target esp32
```

`-Target all` runs all three. Build trees and receipts are isolated under `build/phase2a/`; evidence is `build/phase2a/evidence/`. Native executes the complete existing protocol/reliability suite plus Core tests and seven script scenarios in Debug and Release/NDEBUG. All C checks execute regardless of NDEBUG. Provenance gates require exact CTest inventory/executable/arguments, no skipped cases, input/compiler/output hashes and explicit BENCH_ONLY compile guards. The process-only execution policy/PATH/environment changes do not modify global settings.

Core compilation requires `TYVOR_BENCH_ONLY=1`; `TYVOR_ENABLE_RF=1` is a compile error. STM32 UART candidate is explicitly OFF; ESP sdkconfig must keep UART gate OFF with GPIOs -1. Default STM32 input remains a software fixture: centered roll/pitch/yaw, throttle minimum, ARM OFF, mode center. Flash and PSRAM configuration are unchanged; no physical memory claim follows from a successful build.

## Native simulator

```powershell
build\phase2a\native-debug\tyvor_core_sim.exe
build\phase2a\native-release\tyvor_core_sim.exe --script sim\scenarios\scenario_safe_startup.txt
```

The executable reads commands from stdin interactively or from a scenario file. Each failed command/expectation returns nonzero. A scenario is bounded text, uses a fake clock and never accesses hardware.

| Command | Meaning |
|---|---|
| `gimbal roll 1000` | Set roll/pitch/throttle/yaw normalized -1000..1000; software sample provider converts to raw calibration values |
| `switch arm 1` / `switch mode 2` | Set local two-/three-position mock switches; mode=2 is kill |
| `advance 200` | Advance fake milliseconds, maximum 60000 per command |
| `model 1` / `load 1` | Load model slot 0/1 through Core safety guard |
| `save 0` | Save active validated model to RAM persistence adapter, subject to same safe guard |
| `reject_model 1` | Require model load to be refused (used by unsafe-selection scenarios) |
| `input loss` / `input recover` | Stop/resume mock samples; normal input deadline still applies |
| `telemetry battery 7900` / `telemetry lq 72` / `telemetry rssi -85` | Change mock sensor values |
| `telemetry gps 107600000 1066600000 12` | Set mock lat/lon E7 and satellites |
| `telemetry off` / `telemetry on` | Stop/resume telemetry source; independent sensor expiration applies |
| `reset stm32` / `reset esp32` | Reinitialize the selected software MCU endpoint with a new deterministic test boot ID |
| `link off` / `link on` | Drop/restore simulated UART traffic independently of Core execution |
| `page 0`..`page 6` / `status` | Print read-only UI view/state/monitor channels |
| `expect safety DISARMED` / `expect channel 3 -1000` | Always-executed scenario assertions |

On Windows stdin ends with Ctrl-Z then Enter. Scripts in `sim/scenarios` are the executable examples; their final output is also saved in evidence. UI reset invalidates UI state and renegotiates; it cannot disarm or arm STM32. STM32 reset always enters safe startup; valid model storage can survive that object reset, but mock storage does not survive process/power loss.

## Inspect, package and validate offline

ELF inspection needs pyelftools already provided by the pinned IDF Python environment:

```powershell
.tools\espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe scripts\phase2a_artifacts.py inspect
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\phase2a_artifacts.py package
$env:TYVOR_BUILD_PHASE='phase2a'
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\test_package_validation.py --report build\phase2a\evidence\package-tests.json
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\phase2a_artifacts.py package
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\validate_esp_package.py artifacts\phase2a\esp32 --manifest artifacts\phase2a\SHA256_MANIFEST.json --workspace . --report build\phase2a\evidence\final-manifest-validation.json
```

The second package run incorporates fresh package-test evidence; older Phase 2A packages are preserved under `build/phase2a-baseline/package-*`. Phase 1's artifact directory is never replaced by the new script. Its source hashes describe the frozen Phase 1 snapshot, not the changed Phase 2A workspace.

Inspection verifies STM32 vector/load/memory bounds and BIN identity, checks ESP lacks STM32 Core authority symbols, records static size/per-function stack estimates and binds inspection to ELF/BIN hashes. Packaging requires matching successful receipts and memory evidence, preserves ESP SDK subdirectories/offsets, validates ESP images/partitions/config offline and creates portable manifest schema 2 plus full source snapshot and baseline diff. When the initial baseline snapshot is absent in a fresh checkout, full source hashes/snapshot are still produced; historical diff is available only where that saved baseline exists.

The validator never opens serial or writes a device. SDK build logs can print suggested flash commands automatically; they are not executed. Flashing, panel/touch work, pin enablement, hardware PSRAM/clock/watchdog tests and Phase 2B implementation remain outside these build commands.
