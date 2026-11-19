#include "mock.h"
#include <stdio.h>
#include <windows.h>
static tyvor_ui321_t ui;
static tyvor_ui321_mock_t mock;
static uint8_t buffer[TYVOR_DISPLAY_BUFFER_BYTES];
int main(void) {
  LARGE_INTEGER frequency, start, end;
  QueryPerformanceFrequency(&frequency);
  lv_init();
  for (unsigned profile = 0; profile < 2; profile++) {
    int w = profile ? 448 : 320, h = profile ? 368 : 240;
    if (tyvor_ui321_init(&ui, tyvor_ui321_mock_port(&mock, w, h), buffer,
                         sizeof(buffer)))
      return 1;
    tyvor_ui_state_t state;
    tyvor_ui321_view_t view;
    tyvor_ui321_fixture(&state, 1000, 3);
    if (tyvor_ui321_project(&state, 1000, 1, 0, &view))
      return 1;
    tyvor_ui321_update(&ui, &view);
    tyvor_ui321_frame(&ui);
    uint64_t pixels = ui.pixels;
    QueryPerformanceCounter(&start);
    for (unsigned i = 0; i < 240; i++) {
      view.inputs[0] = (int16_t)((int)(i % 100) * 20 - 1000);
      tyvor_ui321_update(&ui, &view);
      tyvor_ui321_frame(&ui);
    }
    QueryPerformanceCounter(&end);
    double seconds =
        (double)(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    printf("BENCH %dx%d frames=240 seconds=%.6f unpaced_redraw_fps=%.2f "
           "pixels=%llu\n",
           w, h, seconds, 240 / seconds,
           (unsigned long long)(ui.pixels - pixels));
    pixels = ui.pixels;
    for (unsigned i = 0; i < 240; i++) {
      tyvor_ui321_update(&ui, &view);
      tyvor_ui321_frame(&ui);
    }
    if (ui.pixels != pixels)
      return 1;
    tyvor_ui321_destroy(&ui);
  }
  puts(
      "PC QPC only; RGB565 memory sink; no SPI, no hardware FPS, observer OFF");
  return 0;
}
