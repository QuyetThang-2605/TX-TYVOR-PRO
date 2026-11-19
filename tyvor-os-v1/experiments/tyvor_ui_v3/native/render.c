#include "mock.h"
#include <stdio.h>
#include <string.h>
static tyvor_ui3_t ui;
static tyvor_ui3_mock_t mock;
static uint8_t buffer[TYVOR_DISPLAY_BUFFER_BYTES];
static int geometry(const char *folder, int w, int h) {
  char path[1024];
  snprintf(path, sizeof(path), "%s/home-geometry-%dx%d.json", folder, w, h);
  FILE *f = fopen(path, "w");
  if (!f)
    return -1;
  const char *names[] = {"hero",    "find",   "bind",
                         "timer",   "safety", "stick_left",
                         "battery", "mode",   "stick_right"};
  lv_obj_t *objects[] = {ui.hero,
                         ui.actions[0],
                         ui.actions[1],
                         ui.field[0],
                         lv_obj_get_parent(ui.safety),
                         ui.footer[0],
                         ui.footer[1],
                         ui.footer[2],
                         ui.footer[3]};
  fprintf(f, "{\n");
  for (unsigned i = 0; i < 9; ++i) {
    lv_area_t a;
    lv_obj_get_coords(objects[i], &a);
    fprintf(f, "\"%s\":[%ld,%ld,%ld,%ld]%s\n", names[i], (long)a.x1, (long)a.y1,
            (long)lv_obj_get_width(objects[i]),
            (long)lv_obj_get_height(objects[i]), i < 8 ? "," : "");
  }
  fprintf(f, "}\n");
  return fclose(f) ? -1 : 0;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  lv_init();
  unsigned count = 0, errors = 0;
  for (unsigned profile = 0; profile < 2; ++profile) {
    int w = profile ? 448 : 320, h = profile ? 368 : 240;
    if (tyvor_ui3_init(&ui, tyvor_ui3_mock_port(&mock, w, h), buffer,
                       sizeof(buffer)))
      return 1;
    for (unsigned state = 0; state < 5; ++state)
      for (unsigned page = 0; page < UI3_PAGE_COUNT; ++page) {
        tyvor_ui_state_t s;
        tyvor_ui3_view_t v;
        tyvor_ui3_fixture(&s, 1000, state);
        if (tyvor_ui3_project(&s, 1000, 1, 0, &v))
          return 1;
        tyvor_ui3_open(&ui, page);
        tyvor_ui3_update(&ui, &v);
        tyvor_ui3_frame(&ui);
        char path[1024], reason[180];
        if (tyvor_ui3_layout_errors(&ui, reason, sizeof(reason))) {
          fprintf(stderr, "LAYOUT %dx%d state%u page%u %s\n", w, h, state, page,
                  reason);
          ++errors;
        }
        (void)snprintf(path, sizeof(path), "%s/ui-%dx%d-state%u-page%u.ppm",
                       argv[1], w, h, state, page);
        if (tyvor_ui3_mock_ppm(&mock, path))
          return 1;
        if (state == 3 && page == UI3_HOME && geometry(argv[1], w, h))
          return 1;
        ++count;
        if (page == UI3_MODEL || page == UI3_MIXES || page == UI3_OUTPUTS ||
            page == UI3_TELEMETRY || page == UI3_SETTINGS) {
          tyvor_ui3_action(&ui, UI3_MORE);
          tyvor_ui3_frame(&ui);
          if (tyvor_ui3_layout_errors(&ui, reason, sizeof(reason))) {
            fprintf(stderr, "LAYOUT %dx%d state%u page%u sub%u %s\n", w, h,
                    state, page, ui.subpage, reason);
            ++errors;
          }
          (void)snprintf(path, sizeof(path),
                         "%s/ui-%dx%d-state%u-page%u-detail.ppm", argv[1], w, h,
                         state, page);
          if (tyvor_ui3_mock_ppm(&mock, path))
            return 1;
          ++count;
        }
      }
    /* Missing live fields, maximum name, and independent telemetry expiry. */
    for (unsigned edge = 0; edge < 3; ++edge)
      for (unsigned p = 0; p < 2; ++p) {
        tyvor_ui_state_t s;
        tyvor_ui3_view_t v;
        tyvor_ui3_fixture(&s, 1000, 3);
        if (edge == 1)
          strcpy(s.snapshot.model_name, "WWWWWWWWWWWWWW");
        if (edge == 2)
          s.telemetry_expires_ms[0] = s.telemetry_expires_ms[3] = 1000;
        if (tyvor_ui3_project(&s, 1000, edge != 0, 0, &v))
          return 1;
        unsigned page = p ? (edge == 2 ? UI3_TELEMETRY : UI3_MODEL) : UI3_HOME;
        tyvor_ui3_open(&ui, page);
        tyvor_ui3_update(&ui, &v);
        tyvor_ui3_frame(&ui);
        char reason[180], path[1024];
        if (tyvor_ui3_layout_errors(&ui, reason, sizeof(reason))) {
          fprintf(stderr, "EDGE %u profile%u: %s\n", edge, profile, reason);
          ++errors;
        }
        (void)snprintf(path, sizeof(path), "%s/edge-%dx%d-case%u-page%u.ppm",
                       argv[1], w, h, edge, page);
        if (tyvor_ui3_mock_ppm(&mock, path))
          return 1;
        ++count;
      }
    tyvor_ui3_view_t stable;
    tyvor_ui_state_t fixture;
    tyvor_ui3_fixture(&fixture, 1000, 3);
    tyvor_ui3_project(&fixture, 1000, 1, 0, &stable);
    tyvor_ui3_action(&ui, UI3_GO_HOME);
    tyvor_ui3_update(&ui, &stable);
    tyvor_ui3_frame(&ui);
    uint64_t before = ui.pixels;
    strcpy(stable.quality, "70%");
    tyvor_ui3_update(&ui, &stable);
    tyvor_ui3_frame(&ui);
    uint64_t partial = ui.pixels - before;
    before = ui.pixels;
    tyvor_ui3_update(&ui, &stable);
    tyvor_ui3_frame(&ui);
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    printf("METRIC %dx%d partial_LQ_pixels=%llu idle_pixels=%llu "
           "arena_used=%zu arena_peak=%zu arena_free=%zu\n",
           w, h, (unsigned long long)partial,
           (unsigned long long)(ui.pixels - before),
           memory.total_size - memory.free_size, memory.max_used,
           memory.free_size);
    tyvor_ui3_destroy(&ui);
  }
  printf("LVGL9.2.2 actual RGB565 renderer: %u screenshots; %u layout errors; "
         "hardware NOT TESTED\n",
         count, errors);
  return errors ? 1 : 0;
}
