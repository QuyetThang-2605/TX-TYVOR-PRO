# TX15-inspired TYVOR OS feature map (original implementation)

TX15 reference: RadioMaster TX15, EdgeTX on STM32H7, 3.5-inch IPS touch 480x320. Source: https://radiomasterrc.com/products/tx15-radio-controller-elrs-m2 ; https://edgetx.org/edgetx/latest/hardware/stm32-platforms/

Proposed TYVOR UI navigation on 368x448 portrait AMOLED:
- HOME: model, ARM known/unknown, radio battery, LQ, timer; stale data clearly indicated.
- MODELS: select/create, edit model, no hot switching when unsafe.
- INPUTS: calibrated gimbals and trims; visualization only on ESP.
- MIXER: mixer view & proposals; commit via verified STM32 main core.
- OUTPUTS: channel monitor & endpoint visualization.
- TELEMETRY: CRSF derived fields, freshness timeout, GPS if known.
- RADIO: date, audio, display brightness, battery warning, calibration.
- TOOLS: diagnostics, bridge health and firmware versions.

A display UI does not control ARM or RF directly. Switch/ARM decisions stay on main STM32, and all changes require input validation and physical safety gating. Feature list is a specification, not implemented pages.

### Planned phases
P0: native protocol tests, UART status logs.
P1: read-only LVGL UI using simulator/mocks (no RF).
P2: confirmed CO5300 QSPI driver and touch controller.
P3: add real STM32 UART HAL integration after PCB schematic audit.
P4: safety regression, stale link and EMI tests, RF module work separately.
