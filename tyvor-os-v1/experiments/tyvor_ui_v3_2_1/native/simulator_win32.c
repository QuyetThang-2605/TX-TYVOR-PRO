#define WIN32_LEAN_AND_MEAN
#include "mock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>
static tyvor_ui321_t ui;
static tyvor_ui321_mock_t mock;
static lv_indev_t *pointer;
static uint8_t draw[TYVOR_DISPLAY_BUFFER_BYTES];
static uint32_t bitmap[368 * 448];
static int mouse_x, mouse_y, press_x, press_y, pressed, scenario = 3, smoke,
                                                        failed;
static DWORD started, last_tick;
static const char *capture;
static void read_pointer(lv_indev_t *i, lv_indev_data_t *d) {
  (void)i;
  d->point.x = mouse_x;
  d->point.y = mouse_y;
  d->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static void update(void) {
  DWORD now = GetTickCount();
  lv_tick_inc(now - last_tick);
  last_tick = now;
  tyvor_ui_state_t state;
  tyvor_ui321_view_t view;
  tyvor_ui321_fixture(&state, (uint64_t)(now - started) + 1000,
                      (unsigned)scenario);
  if (tyvor_ui321_project(&state, (uint64_t)(now - started) + 1000, 1,
                          (uint64_t)(now - started), &view))
    abort();
  tyvor_ui321_update(&ui, &view);
  (void)lv_timer_handler();
  tyvor_ui321_frame(&ui);
}
static void key(WPARAM k) {
  switch (k) {
  case VK_HOME:
  case 'H':
    tyvor_ui321_action(&ui, UI321_GO_HOME);
    break;
  case VK_ESCAPE:
  case VK_BACK:
    tyvor_ui321_action(&ui, UI321_BACK);
    break;
  case VK_RIGHT:
    tyvor_ui321_action(&ui, UI321_NEXT);
    break;
  case VK_LEFT:
    tyvor_ui321_action(&ui, UI321_PREVIOUS);
    break;
  case VK_PRIOR:
  case VK_NEXT:
    tyvor_ui321_action(&ui, UI321_MORE);
    break;
  case 'M':
    tyvor_ui321_open(&ui, UI321_MODEL);
    break;
  case 'I':
    tyvor_ui321_open(&ui, UI321_INPUTS);
    break;
  case 'X':
    tyvor_ui321_open(&ui, UI321_MIXES);
    break;
  case 'O':
    tyvor_ui321_open(&ui, UI321_OUTPUTS);
    break;
  case 'T':
    tyvor_ui321_open(&ui, UI321_TELEMETRY);
    break;
  case 'R':
    tyvor_ui321_open(&ui, UI321_SETTINGS);
    break;
  case 'Q':
  case VK_UP:
    tyvor_ui321_open(&ui, UI321_QUICK);
    break;
  case VK_DOWN:
    tyvor_ui321_open(&ui, UI321_CONNECTIONS);
    break;
  default:
    if (k >= '1' && k <= '5')
      scenario = (int)(k - '1');
    break;
  }
}
static LRESULT CALLBACK proc(HWND window, UINT message, WPARAM w, LPARAM l) {
  switch (message) {
  case WM_TIMER:
    update();
    InvalidateRect(window, NULL, FALSE);
    return 0;
  case WM_KEYDOWN:
    key(w);
    update();
    InvalidateRect(window, NULL, FALSE);
    return 0;
  case WM_LBUTTONDOWN:
  case WM_MOUSEMOVE:
  case WM_LBUTTONUP: {
    RECT area;
    GetClientRect(window, &area);
    if (!area.right || !area.bottom)
      return 0;
    mouse_x = GET_X_LPARAM(l) * ui.port.width / area.right;
    mouse_y = GET_Y_LPARAM(l) * ui.port.height / area.bottom;
    if (message == WM_LBUTTONDOWN) {
      pressed = 1;
      press_x = mouse_x;
      press_y = mouse_y;
      SetCapture(window);
    }
    if (message == WM_LBUTTONUP) {
      pressed = 0;
      if (abs(mouse_x - press_x) > 35 || abs(mouse_y - press_y) > 35) {
        lv_indev_reset(pointer, NULL);
        tyvor_ui321_gesture(&ui, mouse_x - press_x, mouse_y - press_y);
      }
    }
    lv_indev_read(pointer);
    if (message == WM_LBUTTONUP)
      ReleaseCapture();
    update();
    InvalidateRect(window, NULL, FALSE);
    return 0;
  }
  case WM_CAPTURECHANGED:
    pressed = 0;
    if (pointer)
      lv_indev_reset(pointer, NULL);
    return 0;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(window, &ps);
    RECT area;
    GetClientRect(window, &area);
    for (int i = 0; i < mock.width * mock.height; ++i) {
      unsigned p = mock.pixels[i];
      bitmap[i] = ((p >> 11) * 255 / 31) << 16 |
                  (((p >> 5) & 63) * 255 / 63) << 8 | (p & 31) * 255 / 31;
    }
    BITMAPINFO info;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = mock.width;
    info.bmiHeader.biHeight = -mock.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    StretchDIBits(dc, 0, 0, area.right, area.bottom, 0, 0, mock.width,
                  mock.height, bitmap, &info, DIB_RGB_COLORS, SRCCOPY);
    EndPaint(window, &ps);
    return 0;
  }
  case WM_CLOSE:
    DestroyWindow(window);
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  default:
    return DefWindowProcA(window, message, w, l);
  }
}
int main(int argc, char **argv) {
  int wide = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--wide"))
      wide = 1;
    else if (!strcmp(argv[i], "--smoke"))
      smoke = 1;
    else if (!strcmp(argv[i], "--capture") && i + 1 < argc)
      capture = argv[++i];
    else
      return 2;
  }
  lv_init();
  if (tyvor_ui321_init(
          &ui, tyvor_ui321_mock_port(&mock, wide ? 448 : 320, wide ? 368 : 240),
          draw, sizeof(draw)))
    return 1;
  pointer = lv_indev_create();
  lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
  lv_indev_set_display(pointer, ui.display);
  lv_indev_set_read_cb(pointer, read_pointer);
  started = last_tick = GetTickCount();
  update();
  HINSTANCE module = GetModuleHandleA(NULL);
  WNDCLASSA wc;
  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = proc;
  wc.hInstance = module;
  wc.lpszClassName = "TyvorUI321SoftwareOnly";
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  if (!RegisterClassA(&wc))
    return 1;
  RECT r = {0, 0, mock.width * 2, mock.height * 2};
  AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  HWND window = CreateWindowA(
      wc.lpszClassName, "TYVOR UI V3.2.1 | PC SIMULATOR | NO HARDWARE",
      WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left,
      r.bottom - r.top, NULL, NULL, module, NULL);
  if (!window)
    return 1;
  if (smoke) {
    RECT a;
    GetClientRect(window, &a);
    int x = a.right * 62 / 400, y = a.bottom * 120 / 315;
    SendMessage(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessage(window, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
    if (ui.page != UI321_TELEMETRY) {
      fprintf(stderr, "mouse click page=%u expected TELEMETRY\n", ui.page);
      failed = 1;
    }
    SendMessage(window, WM_KEYDOWN, VK_HOME, 0);
    if (ui.page != UI321_HOME)
      failed = 1;
    SendMessage(window, WM_KEYDOWN, 'Q', 0);
    if (ui.page != UI321_QUICK)
      failed = 1;
    SendMessage(window, WM_KEYDOWN, VK_HOME, 0);
    SendMessage(window, WM_LBUTTONDOWN, MK_LBUTTON,
                MAKELPARAM(a.right / 2, a.bottom / 2));
    SendMessage(window, WM_LBUTTONUP, 0,
                MAKELPARAM(a.right / 2, a.bottom / 2 - 160));
    if (ui.page != UI321_QUICK) {
      fprintf(stderr, "swipe page=%u expected QUICK\n", ui.page);
      failed = 1;
    }
    SendMessage(window, WM_KEYDOWN, VK_HOME, 0);
    if (capture && tyvor_ui321_mock_ppm(&mock, capture))
      failed = 1;
    printf(
        "WIN32 hidden host: mouse/keyboard/swipe %s; real LVGL; no hardware\n",
        failed ? "FAIL" : "PASS");
    DestroyWindow(window);
  } else {
    ShowWindow(window, SW_SHOW);
    SetTimer(window, 1, 33, NULL);
    printf("Mouse click/swipe. H Home, Esc Back, arrows pages, Q Quick, M "
           "Model, I Inputs, X Mixes, O Outputs, T Telemetry, R Settings, PgDn "
           "detail, 1..5 states. Close window to quit.\n");
  }
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  lv_indev_delete(pointer);
  tyvor_ui321_destroy(&ui);
  return failed ? 1 : 0;
}
