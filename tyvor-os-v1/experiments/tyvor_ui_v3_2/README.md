# TYVOR V3.2 — HOME visual refinement

Run the isolated software build with `scripts/build-ui-v3-2.ps1 -Target all` in a
child PowerShell with ExecutionPolicy Bypass. No flash or hardware entry point.
Artifacts: `artifacts/tyvor-ui-v3-2/`; report:
`docs/TYVOR_UI_V3_2_IMPLEMENTATION_REPORT.md`.

HOME follows the approved TYVOR middle/right panels in
`docs/ui-reference/tyvor-v3-2/approved-home.png.png` (actual supplied filename).
The exact user `blue-wave-background.png` is center-cover cropped at uniform
scale into320x240/448x368 RGB565 const Flash assets. No hue, saturation or
brightness edit. Timer/cards/buttons are real LVGL objects, not a UI screenshot.

LVGL9.2.2 composites RGB888 partial regions, then the same in-place packer emits
RGB565 wire data with global-coordinate ordered dither. Draw buffer remains20KiB.
Row stride is respected, and partial redraw matches a full redraw. Win32/GDI presents the
finished LVGL framebuffer; it does not implement another UI renderer. Main
modules: theme,widgets,shape draw events,pages,navigation. Current page only.
Font generator derives two numeric subsets from LVGL's MIT Montserrat48 source,
with4x4 supersampling. Actual user owl/horizontal source shapes are unmatted to
A8 alpha, resized preserving aspect, and tinted white as approved; no white box.
Pinned Lucide ISC/Feather MIT SVGs generate4x4 supersampled flat A8 icon assets
at20/28/34px. Full notices live under `assets/lucide/LICENSE`. No3D or glow.
Static tick masks avoid clip-dependent line rasterization. No large allocation
per frame; original20KiB buffer and96KiB LVGL arena remain unchanged.
Generated C is deterministic and uses normal LVGL font APIs, not screenshot UI.

`ui_view` delegates to the unchanged V2 read-only presenter and freshness logic.
No controller writer/ARM/RF/motor API is accepted. Unknown/live-missing data stays
unavailable; transmitter battery,charge percent,trim positions and bind have no
authoritative source and remain unknown/disabled. SIM timer is caller-provided.

Landscape320x240 and448x368 have explicit profile configuration under
`esp32/experiments/tyvor_ui_v3_2/display_profiles.json`. TFT uses rotation1 in its
own sdkconfig/driver; AMOLED layout is software-only, hardware rotation unknown.
HOME safety uses the published STARTUP/FAILSAFE label, not the broad safe flag.
Expired GPS/LQ are muted/unavailable. Core and old AMOLED configuration are unchanged. DISPLAY/TOUCH/UART/RF OFF;
display GPIOs -1, no physical touch input device.

Mouse click/drag: HOME left→MODEL,up→QUICK,down→CONNECTIONS. Keyboard H/Home,
Esc/Backspace,left/right,M/I/X/O/T/R/Q,Down,PageDown;1–5 change UI fixtures only.
No fixed four-button footer. Secondary page logic is inherited unchanged;shared
theme/renderer improvements apply consistently. Non-HOME pages have local Back/Home buttons. Bind
is disabled. No animation timing was verified from a still image; navigation
is immediate. Physical touch and display timing remain UNVERIFIED.
