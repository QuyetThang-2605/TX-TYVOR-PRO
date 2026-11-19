# TYVOR Tactical Dark — UI V2

Isolated LVGL 9.2.2 software experiment. STM32 Core, protocol v2, existing TFT V1
and AMOLED files remain unchanged. The ESP image has DISPLAY/TOUCH/UART gates OFF
and all display GPIOs -1. No RF/aircraft command handlers exist here.

`ui_view` projects an expiring copy of the published cache; `ui_theme` selects
layout constraints for 240x320 or 368x448; `ui_widgets` owns reusable components
and original vector glyphs; `ui_pages` builds seven pages plus Quick Settings;
`ui` owns LVGL/display flush/navigation. Only the current page is allocated.
Value updates do not rebuild the page. Text updates and style changes are checked
before invalidation; a single 20KiB partial draw buffer is used.

The port accepts synchronous completion of RGB565 wire-order regions. The ESP
adapter reuses the bounded TFT DMA ownership pipeline; a future verified AMOLED
adapter does not need Core changes. The 368x448 profile is software-only, not a
CO5300 hardware driver. No physical input-device driver is registered.

Build: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-ui-v2.ps1 -Target all`.
Export: use the workspace Python to run `scripts/export-ui-v2.py` after a current
Release build. Preview and executables: `artifacts/tyvor-ui-v2/` after packaging.
See `docs/TYVOR_UI_V2_REDESIGN_REPORT.md` for evidence and hardware limitations.

Simulator: mouse clicks, drag left/up on HOME, H/Home, Esc/Backspace, left/right,
M/I/X/O/T/R/Q and PageDown. Keys1–5 select VALID bench ARM,STALE,DISCONNECTED,SAFE,
INVALID MODEL fixtures. These are UI fixture changes; they cannot write Core.
Default SAFE. `--wide` selects368x448; `--smoke` checks a hidden Win32 host then exits.

Live protocol has no flight timer, receiver connectivity or flight-mode field.
They are UNAVAILABLE; Core ticks/LQ never substitute for those fields. Model
selection, calibration, widget editing and hardware-only settings are disabled.
BENCH ARM / SIM never means an aircraft is armed. Stale/expired state cannot keep
active ARM/output/telemetry values on the software-rendered UI.
