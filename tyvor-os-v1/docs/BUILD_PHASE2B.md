# Phase 2B software bench build and replay

Run from `D:\TYVOR\TYVOR-OS-V1-Starter\tyvor-os-v1` with the pinned portable tools already installed. No board, serial device, pin enablement or network download is required by these build commands.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-phase2b.ps1 -Target native
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-phase2b.ps1 -Target stm32
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-phase2b.ps1 -Target esp32
```

`-Target all` runs all three. New trees/receipts/logs are under `build/phase2b`; earlier Phase1/2A artifacts are preserved. Native runs all154 registered cases in Debug and Release/NDEBUG,11 simulator scripts and exports35 SVGs to `artifacts/phase2b-ui`. UI XML validation checks exact page/state inventory, safe labels, canvas and no stale active telemetry; its receipt binds hashes to the Release exporter.

```powershell
build\phase2b\native-release\tyvor_core_sim.exe
build\phase2b\native-release\tyvor_core_sim.exe --script sim\bench_scenarios\bench_fault_reconnect.txt
```

Existing gimbal/switch/model/telemetry/input/reset/page/status/expect commands remain. Added software-only controls:

| Command | Meaning |
|---|---|
| `fault c2u delay 250` | Hold CONTROL->UI packets by250 ms |
| `fault u2c jitter 20` | Seeded extra0..20 ms delay on UI->CONTROL |
| `fault both loss 7` | Drop every7th backend-copied packet;0 disables periodic fault |
| `fault c2u corrupt 11` | Flip CRC byte every11th packet; receiver still validates CRC |
| `fault both duplicate 3` | Duplicate every3rd packet without changing sequence |
| `fault c2u chunk 1` / `fault c2u gap 5` | Split into byte chunks with5 ms gap to exercise parser timeout |
| `fault c2u partial 1` / `fault u2c fail 1` | Backend partial or negative TX result |
| `fault c2u hold 1` | Retain queued packets for delayed-old-packet tests |
| `fault both clear 0` | Remove fault settings and discard queued damaged bytes |
| `link off` / `link on` | Disconnect/reconnect simulated UART; does not change Core safety |
| `reset both` | Reset both simulated MCU instances; existing single-MCU resets retained |
| `model_corrupt 0` | Corrupt RAM storage slot0 for local CRC/invalid-model tests |
| `mixscroll 11` / `page 3` | Scroll through read-only ordered mixer terms |
| `diag` | Print version/session/errors/observed reset counters/status age; physical heap/stack/WDT remains UNVERIFIED in native |

Directions: `c2u`, `u2c`, `both`. Fake clock advances only via `advance`. On Windows interactive stdin ends with Ctrl-Z then Enter. Seven views: HOME, MODEL, INPUTS, MIXES, OUTPUTS, TELEMETRY, SETTINGS. SVGs represent five states: VALID, STALE, DISCONNECTED, SAFE, INVALID MODEL. They come from actual simulated Core/cache states; no panel/touch driver is used.

Inspect and package offline:

```powershell
.tools\espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe scripts\phase2b_artifacts.py inspect
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\phase2b_artifacts.py package
$env:TYVOR_BUILD_PHASE='phase2b'
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\test_package_validation.py --report build\phase2b\evidence\package-tests.json
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\phase2b_artifacts.py package
.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe scripts\validate_esp_package.py artifacts\phase2b\esp32 --manifest artifacts\phase2b\SHA256_MANIFEST.json --workspace . --report build\phase2b\evidence\final-manifest-validation.json
```

Second packaging incorporates current package-test evidence. Prior Phase2B packages are preserved under `build/phase2b-baseline/package-*`. The exact artifact/source inventory and SHA-256 are in the manifest; four successful/current build receipts, memory/ELF identity and UI evidence are required. No flash/monitor/serial-open command is executed. SDK build logs can print suggested flash commands automatically.

Review baseline before changes: `build/phase2b-baseline/source/` (88 files), old manifest, Debug/Release120-case logs/JUnit. Four regression probes live in `sim/test_bench_regressions.c`; expected FAIL logs compiled against old Phase2A code are in baseline `*-before.txt`, fixed Debug/Release logs in new evidence `*-after.txt`. For reproducing old compilation use the frozen include tree with the old libraries, not current headers: Core/sim structures and monitor revision changed in Phase2B. Original Phase2A source, artifacts and their hashes remain preserved.

New wire read correlation/config pages are documented in `MCU_COMMUNICATION_SPEC.md`; outer protocol v2 unchanged, Core monitor revision1->2. Hardware/electrical approval gates and UNVERIFIED records are in `BOARD_BRINGUP_CHECKLIST.md`. These instructions authorize only software replay, not hardware bring-up.
