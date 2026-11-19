# TYVOR DISCOVERY BENCH V1 — review and first-flash procedure

Date: 2026-11-10. **Current state: BLOCKED before target access and flashing.**
This is a preparation procedure. No firmware write, reset, erase, driver installation,
ST-LINK update or option-byte change was performed by this session.

## 1. Board reference and checks that remain physical

The user reports **MB997E**. The official reference is
[ST MB997 E01 schematic, 09/04/2020](https://www.st.com/resource/en/schematic_pack/mb997-f407vgt6-e01_schematic.pdf)
(sheets 3, 4, 7). The complete assembly suffix and actual MCU marking still need
front/back photos. The BSP implements this reference, not the Mini Type-C board.

| Reference net | Assignment in new BSP | Actual board status |
|---|---|---|
| LD4 green / PD12 | Heartbeat | UNTESTED |
| LD3 orange / PD13 | Simulator activity | UNTESTED |
| LD5 red / PD14 | Fault / simulated failsafe | UNTESTED |
| LD6 blue / PD15 | Debounced USER indication | UNTESTED |
| B1 / PA0, active high | Demo selector, no ARM authority | UNTESTED |
| PA13 / PA14 | SWD preserved | Jumpers/continuity UNVERIFIED |
| PH0 | HSE bypass, 8 MHz MCO via R68; X2 marked not fitted | Population/clock UNVERIFIED |
| UART / RF | No assignment or driver | Gates OFF |

The [UM1472 Rev9](https://www.st.com/resource/en/user_manual/dm00039084-discovery-kit-with-stm32f407vg-mcu-stmicroelectronics.pdf)
§7.3.4, §7.10–7.12 documents CN3 both ON for onboard SWD, JP1 ON for target
supply, and clock options. Compare actual jumpers, R25/R68 and SB13/SB14 with
the E01 reference while unpowered; record any modification. Do not connect an
external target at CN2. Actual power rails and jumper positions were not observed.
VCP enumeration alone does not establish a target UART connection (§7.3.3).

Before any future write: photograph the actual `STM32F407VGT6` marking, board
suffix, jumpers and USB connector; verify the intended USB power path and target
rail with a meter. Stop on identity, voltage or clock uncertainty. No external
power source, ESP connection, RF module, actuator or AMOLED is needed for this demo.

## 2. Rebuild and verify offline

Run from the workspace root. PowerShell's execution-policy override below is
limited to this child process; it does not set a machine/user policy.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-discovery.ps1 -Target all
& ./.tools/espressif/python_env/idf5.5_py3.11_env/Scripts/python.exe scripts/discovery_artifacts.py inspect
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/discovery_artifacts.py verify
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/test_discovery_platform.py
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/test_discovery_tools.py
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/discovery_artifacts.py package
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/discovery_programmer.py preflight
```

Native inventory: **198 Debug +198 Release/NDEBUG**. The default root CMake
configuration keeps its original 177 tests; the build script explicitly opts into
the Discovery tests. The legacy VE image is rebuilt separately with its UART gate OFF.

The package is `artifacts/stm32f4-discovery-bench-v1/`. Its manifest checks every
packaged file. `inspect` verifies ARM ELF/BIN correspondence, vector entries,
SRAM/Flash/stack placement, linked Core, absence of linked output drivers, and
unchanged legacy VE BIN. Preflight is offline and never opens USB or a COM port.
After a rebuild, regenerate inspection and package; do not reuse stale manifests.

## 3. Resolve the host blockers before attempting a backup

Observed probe USB serial: `066DFF575377524867063531`, VID/PID `0483:374B`.
Windows debug interface **Code28**, no bound driver. Composite/MSD/COM6 enumeration
works; SWD readiness does not follow. CubeProgrammer/CLT was not found in checked
PATH/common install locations or installed-ST inventory. No MCU ID, Flash size,
RDP/nWRP or original firmware was read.

An operator must install the official ST debug driver/CubeProgrammer separately,
then supply the actual CLI path if it is outside PATH/the script's standard
location. Do not update ST-LINK firmware, reset the target or clear protection
as an automatic workaround. Re-run only the read-only inventory:

```powershell
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/discovery_programmer.py inventory --cli 'C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe'
```

`HOST_ENUMERATED_ONLY` still is not target identification. If host enumeration
fails, stop; retain the log. Current `readonly-inventory.json` reports BLOCKED.

## 4. Read-only identification and original firmware backup

Only after the host/physical prerequisites above, run the explicit backup action
with the selected probe. **Do not execute this step against an unidentified or
different board.** The command below was not run in this session.

```powershell
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/discovery_programmer.py backup --serial 066DFF575377524867063531 --cli 'C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe'
```

Implementation: exact-serial `SWD mode=HOTPLUG freq=1000`, bounded CLI calls;
read ID at `0xE0042000`, Flash-size word at `0x1FFF7A22`, FLASH_OPTCR at
`0x40023C14`, and UID at `0x1FFF7A10`. The addresses come from the pinned ST
`stm32f407xx.h` and `stm32f4xx_hal_flash_ex.h` definitions. No halt/reset fallback.
`DEV_ID=0x413` identifies a shared family, **not the precise package**; Flash must
read 1024 KiB and physical marking must independently confirm VG. VE/512 KiB is rejected.

Any unknown/RDP-protected state stops the process; no RDP regression or unprotect.
Two full 1 MiB reads at `0x08000000` must have identical SHA-256; option register
must remain equal before/after. `-ob displ` records display only. Existing files
are never overwritten. Each attempt creates its own
`build/discovery/backups/<timestamp-random>/backup-record.json`, raw reads and logs.
Copy a successful backup and its record to separate storage before approval.
No such original-firmware backup currently exists.

The command direction and HOTPLUG mode follow the
[official CubeProgrammer CLI reference](https://dev.st.com/stm32cube-docs/prog/2.23.0/en/docs/markup/CubeProg_Command_Lines.html).
In this CLI, `--upload` reads memory into a host file. `-w` writes and implicitly
erases affected Flash sectors; it must not be used during inventory/backup.

## 5. Review record and printed plan — no automatic flashing

Create a local review JSON only after completing the checks. Template (all
physical approvals intentionally false):

```json
{
  "board_marking": "UNVERIFIED",
  "mcu_marking": "UNVERIFIED",
  "jumpers_verified": false,
  "power_verified": false,
  "hse_mco_8mhz_verified": false,
  "overwrite_original_approved": false,
  "recovery_plan_reviewed": false,
  "reviewer": "",
  "date": ""
}
```

Record photos, meter/clock evidence and an explicit decision to replace the
unknown current application. A valid read-only backup does not itself authorize
overwriting. Before a later manual flash, repeat identity/protection/UID/SN checks
on the still-selected target and verify the backup's SHA-256 again.

```powershell
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/discovery_programmer.py flash-plan --serial 066DFF575377524867063531 --backup-record build/discovery/backups/ACTUAL_RUN/backup-record.json --review-record build/discovery/ACTUAL_REVIEW.json --cli 'C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe'
```

This prints an argument array for **future human review only** and performs no
CubeProgrammer call. It requires the Discovery package hashes/identity/vector,
1024 KiB backup and raw-read hashes, RDP `0xAA`, nWRP `0xFFF`, correct probe,
physical markings and all approvals. Flash address is **`0x08000000`**, with no
bootloader offset. The script has no `flash` action. Never pass its printed array
into `Invoke-Expression` or an automatic executor.

Recovery plan to review before authorizing a first write: preserve onboard SWD
pins/jumpers; retain full original BIN, identity/options, probe serial and hashes;
use ST-LINK connect-under-reset if the future demo does not respond. This may
reset/halt the application and belongs to the later authorized hardware session.
Stop if connection/protection differs; do not mass-erase or change option bytes.
Restoring the original image is a separate explicitly approved write, not an
automatic fallback. No recovery action was exercised here.

## 6. Future first-run observations

After a separately authorized program-and-verify, start in SAFE mode. Observe
PD12 heartbeat (500 ms on/off), PD13 activity (100 ms on/off), and PA0 USER/PD15
with 30 ms debounce. A held button during boot must not advance modes until
released. Cycle SAFE → MIXER → INPUT_LOSS → MODEL_TELEMETRY → SAFE; inspect
`tyvor_discovery_demo`, `discovery_board_diagnostics`, `discovery_timing` over SWD.
All ARM/channel values are RAM-only simulation. Red in INPUT_LOSS is expected;
latched application/clock/self-test fault is a different stop condition.

Capture reset reason, clocks, timing max/over-budget/lateness, stack and watchdog
counters for each mode. IWDG remains enabled during debugger halt and can reset
the target; no debug-freeze bypass is installed. Hardware LED/button, actual
clock, watchdog period, WCET and recovery all remain **NOT RUN**.
