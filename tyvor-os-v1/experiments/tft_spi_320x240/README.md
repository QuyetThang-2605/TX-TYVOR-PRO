# TYVOR TFT SPI experiment

Software bench only. Main controller remains STM32F407VET6; UI controller remains
WeAct ESP32-S3-B N16R8. ILI9341 is **PROBABLE**, actual module/pinout/touch unverified.

- Portable presenter, bounded RGB565 region/DMA-buffer contract, LVGL9.2.2 UI,
  mock framebuffer, tests and SPI-touch adapter contract live here.
- Independent IDF project: `esp32/experiments/tft_spi_320x240/`.
- Original STM32 Core, protocol v2, bridge and AMOLED368×448 files remain unchanged.
- TFT defaults: portrait240×320; optional rotations90/180/270. Profile identity
  `TFT_SPI_320X240` also identifies its landscape320×240 orientation.
- Display hardware gate/board/module confirmations OFF; every pin=-1; UART gate
  OFF; touch runtime OFF. Synthetic pins in native tests are not assignments.
- Default simulator fixtures are clearly labelled. Aircraft ARM stays UNKNOWN;
  flight timer is SIM-only because current protocol has no flight-timer field.

```powershell
& ./.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/fetch-tft-dependencies.py --verify-only
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-tft-experiment.ps1 -Target all
```

Setup on a new workstation: run dependency fetch without `--verify-only` after
setting up the workspace's existing pinned SDK/toolchain. Downloads are immutable
commits plus SHA-256 locks; no component-manager/global installation is required.
The upstream ILI9341 source stays unmodified; the wrapper supplies its2.1.0 version
macros rather than depending on the component-manager CMake version helper.

Native rendering uses the actual LVGL software rasterizer and writes PPM/PNG
artifacts. It does not emulate the panel's electrical behavior or prove touch,
colour order, power, SPI clock, PSRAM, timing, heap or stack on real hardware.
See `docs/TFT_3_2_DISPLAY_EXPERIMENT_REPORT.md` for results and remaining gates.
