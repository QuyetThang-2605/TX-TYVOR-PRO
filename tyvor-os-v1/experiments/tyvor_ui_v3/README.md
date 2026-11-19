# TYVOR V3 — landscape reference-led LVGL UI

Run the isolated software build with `scripts/build-ui-v3.ps1 -Target all` in a
child PowerShell with ExecutionPolicy Bypass. No flash or hardware entry point.
Artifacts: `artifacts/tyvor-ui-v3-oxbot/`; report:
`docs/TYVOR_UI_V3_OXBOT_REPORT.md`.

HOME follows the official400x315 Oxbot still poster geometry. The user reference
folder currently holds three TYVOR logos, not Oxbot UI screenshots. Other pages
share HOME shapes/colors but have no direct screen reference; do not claim1:1.
No Oxbot images,backgrounds,fonts,logos or firmware are linked into the UI.

LVGL9.2.2 renders both ESP and native RGB565 regions. Win32/GDI presents the
finished LVGL framebuffer; it does not implement another UI renderer. Main
modules: theme,widgets,shape draw events,pages,navigation. Current page only.
Font generator derives two numeric subsets from LVGL's MIT Montserrat48 source.
Generated C is deterministic and uses normal LVGL font APIs, not screenshot UI.

`ui_view` delegates to the unchanged V2 read-only presenter and freshness logic.
No controller writer/ARM/RF/motor API is accepted. Unknown/live-missing data stays
unavailable; transmitter battery,charge percent,trim positions and bind have no
authoritative source and remain unknown/disabled. SIM timer is caller-provided.

Landscape320x240 and448x368 have explicit profile configuration under
`esp32/experiments/tyvor_ui_v3/display_profiles.json`. TFT uses rotation1 in its
own sdkconfig/driver; AMOLED layout is software-only, hardware rotation unknown.
Core and old AMOLED configuration are unchanged. DISPLAY/TOUCH/UART/RF OFF;
display GPIOs -1, no physical touch input device.

Mouse click/drag: HOME left→MODEL,up→QUICK,down→CONNECTIONS. Keyboard H/Home,
Esc/Backspace,left/right,M/I/X/O/T/R/Q,Down,PageDown;1–5 change UI fixtures only.
No fixed four-button footer. Non-HOME pages have local Back/Home buttons. Bind
is disabled. No animation timing was verified from a still image; navigation
is immediate. Physical touch and display timing remain UNVERIFIED.
