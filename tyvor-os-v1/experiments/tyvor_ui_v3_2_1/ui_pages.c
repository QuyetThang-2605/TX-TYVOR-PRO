#include "branding.h"
#include "icons.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>
static lv_obj_t *label(tyvor_ui321_t *u, unsigned i, lv_obj_t *p, int x, int y,
                       int w, const char *s, const lv_font_t *font,
                       uint32_t c) {
  u->field[i] = tyvor_ui321_label(p, x, y, w, s, font, c);
  return u->field[i];
}
static void center(lv_obj_t *o) {
  lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
}
static int X(tyvor_ui321_t *u, int v) { return tyvor_ui321_x(u, v); }
static int Y(tyvor_ui321_t *u, int v) { return tyvor_ui321_y(u, v); }
static void header(tyvor_ui321_t *u) {
  int w = u->port.width;
  u->back = tyvor_ui321_button(u->body, 4, 0, 44, 44, NULL, UI321_ICON_BACK);
  tyvor_ui321_bind(u->back, u, UI321_BACK, -1);
  u->home =
      tyvor_ui321_button(u->body, w - 48, 0, 44, 44, NULL, UI321_ICON_HOME);
  tyvor_ui321_bind(u->home, u, UI321_GO_HOME, -1);
  u->page_title =
      tyvor_ui321_label(u->body, 56, 5, w - 164, tyvor_ui321_page_name(u->page),
                        &lv_font_montserrat_14, UI321_TEXT);
  label(u, 46, u->body, 56, 24, w - 164, "VIEW ONLY", &lv_font_montserrat_12,
        UI321_MUTED);
  if (u->page != UI321_INPUTS && u->page != UI321_QUICK) {
    u->more =
        tyvor_ui321_button(u->body, w - 96, 0, 44, 44, NULL, UI321_ICON_MORE);
    tyvor_ui321_bind(u->more, u, UI321_MORE, -1);
  }
}
static void home(tyvor_ui321_t *u) {
  int top = u->layout.top;
  u->hero = tyvor_ui321_box(u->body, X(u, 17), Y(u, 47) - top, X(u, 365),
                            Y(u, 154), UI321_SURFACE);
  lv_obj_set_style_radius(u->hero, X(u, 22), 0);
  lv_obj_set_style_bg_opa(u->hero, 224, 0);
  lv_obj_set_style_bg_grad_color(u->hero, lv_color_hex(0x12417B), 0);
  lv_obj_set_style_bg_grad_dir(u->hero, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(u->hero, 1, 0);
  lv_obj_set_style_border_color(u->hero, lv_color_hex(0x639DD4), 0);
  lv_obj_set_style_border_opa(u->hero, 80, 0);
  lv_obj_t *notch = tyvor_ui321_box(u->body, X(u, 128), Y(u, 47) - top,
                                    X(u, 144), Y(u, 29), UI321_BG);
  lv_obj_set_style_radius(notch, X(u, 14), 0);
  u->safety = tyvor_ui321_label(notch, 2, 4, X(u, 144) - 4, "UNKNOWN",
                                &lv_font_montserrat_12, UI321_DANGER);
  center(u->safety);
  u->actions[0] = tyvor_ui321_button(u->body, X(u, 30), Y(u, 90) - top,
                                     X(u, 65), Y(u, 62), NULL, UI321_ICON_PIN);
  tyvor_ui321_bind(u->actions[0], u, 0, UI321_TELEMETRY);
  u->actions[1] = tyvor_ui321_button(u->body, X(u, 308), Y(u, 90) - top,
                                     X(u, 65), Y(u, 62), NULL, UI321_ICON_WAVE);
  lv_obj_add_state(u->actions[1], LV_STATE_DISABLED);
  lv_obj_set_style_opa(u->actions[1], LV_OPA_70, 0);
  center(label(u, 9, u->body, X(u, 21), Y(u, 155) - top, X(u, 88), "GPS",
               &lv_font_montserrat_12, UI321_TEXT));
  center(label(u, 10, u->body, X(u, 299), Y(u, 155) - top, X(u, 82), "BIND N/A",
               &lv_font_montserrat_12, UI321_MUTED));
  lv_obj_t *timer = label(
      u, 0, u->body, X(u, 100), Y(u, 77) - top, X(u, 200), "--:--",
      u->layout.wide ? &tyvor_ui321_timer_large : &tyvor_ui321_timer_compact,
      0xF3F8FF);
  center(timer);
  center(label(u, 1, u->body, X(u, 99), Y(u, 178) - top, X(u, 202),
               "RX UNAVAILABLE", &lv_font_montserrat_12, UI321_WARNING));
  const int left[] = {17, 111, 205, 287}, width[] = {90, 88, 78, 95};
  for (unsigned i = 0; i < 4; ++i) {
    u->footer[i] = tyvor_ui321_box(u->body, X(u, left[i]), Y(u, 211) - top,
                                   X(u, width[i]), Y(u, 92), UI321_CARD);
    lv_obj_set_style_bg_opa(u->footer[i], 220, 0);
    lv_obj_set_style_radius(u->footer[i], X(u, 18), 0);
    lv_obj_set_style_bg_grad_color(u->footer[i], lv_color_hex(0x0B2E5D), 0);
    lv_obj_set_style_bg_grad_dir(u->footer[i], LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(u->footer[i], 1, 0);
    lv_obj_set_style_border_color(u->footer[i], lv_color_hex(0x649BCC), 0);
    lv_obj_set_style_border_opa(u->footer[i], 55, 0);
  }
  u->sticks[0] = tyvor_ui321_dial(u->footer[0], 2, 2, X(u, 90) - 4,
                                  Y(u, 92) - 4, &u->stick_data[0]);
  u->sticks[1] = tyvor_ui321_dial(u->footer[3], 2, 2, X(u, 95) - 4,
                                  Y(u, 92) - 4, &u->stick_data[1]);
  tyvor_ui321_icon(u->footer[1], 8, 5, UI321_ICON_BATTERY, UI321_TEXT);
  center(label(u, 2, u->footer[1], 3, Y(u, 36), X(u, 88) - 6, "--.-- V",
               &lv_font_montserrat_14, UI321_TEXT));
  u->bars[0] = tyvor_ui321_bar(u->footer[1], 7, Y(u, 60), X(u, 88) - 14, 2);
  lv_bar_set_range(u->bars[0], 0, 1);
  center(label(u, 3, u->footer[1], 2, Y(u, 70), X(u, 88) - 4, "BAT --",
               &lv_font_montserrat_12, UI321_SUCCESS));
  center(label(u, 4, u->footer[2], 2, Y(u, 7), X(u, 78) - 4, "MODE",
               &lv_font_montserrat_12, UI321_MUTED));
  center(label(u, 5, u->footer[2], 2, Y(u, 34), X(u, 78) - 4, "N/A",
               &lv_font_montserrat_12, UI321_WARNING));
  int brand_width = u->layout.wide ? 78 : 56;
  u->brand_wordmark = tyvor_ui321_brand(
      u->footer[2], (X(u, 78) - brand_width) / 2, Y(u, 65), 1, u->layout.wide);
  /* Unavailable trim rails have no position marker; never display fake trim0.
   */
  const int rails[][4] = {{2, 94, 4, 198},
                          {394, 94, 4, 198},
                          {29, 307, 121, 4},
                          {229, 307, 130, 4}};
  for (unsigned i = 0; i < 4; ++i) {
    lv_obj_t *r = tyvor_ui321_box(
        u->body, X(u, rails[i][0]), Y(u, rails[i][1]) - top,
        X(u, rails[i][2]) > 1 ? X(u, rails[i][2]) : 2,
        Y(u, rails[i][3]) > 1 ? Y(u, rails[i][3]) : 2, UI321_MUTED);
    lv_obj_set_style_bg_opa(r, 80, 0);
  }
}
static void model(tyvor_ui321_t *u) {
  header(u);
  int w = u->port.width, p = u->layout.pad, y = 52,
      ch = u->layout.wide ? 153 : 108;
  lv_obj_t *panel = tyvor_ui321_box(u->body, p, y, w - 2 * p, ch, UI321_CARD);
  lv_obj_set_style_radius(panel, 18, 0);
  lv_obj_set_style_bg_opa(panel, 190, 0);
  label(u, 0, panel, 14, 10, w - 2 * p - 28, "PUBLISHED MODEL",
        &lv_font_montserrat_12, UI321_MUTED);
  label(u, 1, panel, 14, 33, w - 2 * p - 28, "--", &lv_font_montserrat_20,
        UI321_TEXT);
  label(u, 2, panel, 14, 66, w - 2 * p - 28, "ID -- / REV --",
        &lv_font_montserrat_12, UI321_MUTED);
  label(u, 3, u->body, p, y + ch + 8, w - 2 * p, "MODEL SELECTION UNAVAILABLE",
        &lv_font_montserrat_12, UI321_WARNING);
  if (u->layout.wide)
    label(u, 4, u->body, p, y + ch + 35, w - 2 * p,
          "Layout editor unavailable / no Core commands",
          &lv_font_montserrat_12, UI321_MUTED);
}
static void inputs(tyvor_ui321_t *u) {
  header(u);
  int p = u->layout.pad, w = u->port.width, d = u->layout.wide ? 139 : 88;
  for (unsigned i = 0; i < 2; ++i) {
    int x = i ? w - p - d : p;
    u->sticks[i] = tyvor_ui321_dial(u->body, x, 48, d, d, &u->stick_data[i]);
  }
  int cx = p + d + 8, cw = w - 2 * cx;
  label(u, 0, u->body, cx, 57, cw, "R --", &lv_font_montserrat_12, UI321_TEXT);
  label(u, 1, u->body, cx, 80, cw, "P --", &lv_font_montserrat_12, UI321_TEXT);
  label(u, 2, u->body, cx, 103, cw, "T --", &lv_font_montserrat_12, UI321_TEXT);
  label(u, 3, u->body, cx, 126, cw, "Y --", &lv_font_montserrat_12, UI321_TEXT);
  label(u, 4, u->body, p, 48 + d + 16, w - 2 * p, "SA --    SB --    TRIM N/A",
        &lv_font_montserrat_12, UI321_MUTED);
}
static void channels(tyvor_ui321_t *u, int mixes) {
  header(u);
  int w = u->port.width, p = u->layout.pad, row = u->layout.wide ? 59 : 36;
  for (unsigned i = 0; i < 4; ++i) {
    lv_obj_t *r = tyvor_ui321_box(u->body, p, 50 + (int)i * row, w - 2 * p,
                                  row - 3, UI321_CARD);
    lv_obj_set_style_bg_opa(r, 216, 0);
    lv_obj_set_style_radius(r, 12, 0);
    label(u, i * 2, r, 10, 2, mixes ? w - 2 * p - 20 : 70, "--",
          &lv_font_montserrat_12, UI321_TEXT);
    if (mixes)
      label(u, i * 2 + 1, r, 10, u->layout.wide ? 29 : 17, w - 2 * p - 20, "--",
            &lv_font_montserrat_12, UI321_MUTED);
    else {
      lv_obj_t *value = label(u, i * 2 + 1, r, w - 2 * p - 90, 2, 80, "--",
                              &lv_font_montserrat_12, UI321_TEXT);
      lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
      u->bars[i] =
          tyvor_ui321_bar(r, 10, u->layout.wide ? 36 : 23, w - 2 * p - 20, 6);
    }
  }
}
static void telemetry(tyvor_ui321_t *u) {
  header(u);
  int p = u->layout.pad, w = u->port.width, row = u->layout.wide ? 60 : 36;
  const char *titles[] = {"BATTERY", "LQ / RSSI", "GPS", "CORE AGE"};
  const char *detail[] = {"LATITUDE", "LONGITUDE", "VALIDITY", "AIRCRAFT ARM"};
  for (unsigned i = 0; i < 4; ++i) {
    lv_obj_t *r = tyvor_ui321_box(u->body, p, 50 + (int)i * row, w - 2 * p,
                                  row - 3, UI321_CARD);
    lv_obj_set_style_radius(r, 12, 0);
    lv_obj_set_style_bg_opa(r, 216, 0);
    tyvor_ui321_label(r, 10, 2, w - 2 * p - 20,
                      u->subpage ? detail[i] : titles[i],
                      &lv_font_montserrat_12, UI321_MUTED);
    label(u, i, r, 10, u->layout.wide ? 29 : 17, w - 2 * p - 20, "--",
          &lv_font_montserrat_14, UI321_TEXT);
  }
}
static void settings(tyvor_ui321_t *u) {
  header(u);
  int p = u->layout.pad, w = u->port.width, row = u->layout.wide ? 60 : 36;
  const char *titles[] = {"DISPLAY", "SYSTEM", "TOUCH / UART",
                          "MODEL / LAYOUT"};
  for (unsigned i = 0; i < 4; ++i) {
    lv_obj_t *r = tyvor_ui321_box(u->body, p, 50 + (int)i * row, w - 2 * p,
                                  row - 3, UI321_CARD);
    lv_obj_set_style_bg_opa(r, 216, 0);
    lv_obj_set_style_radius(r, 12, 0);
    tyvor_ui321_label(r, 10, 2, w - 2 * p - 20, titles[i],
                      &lv_font_montserrat_12, UI321_MUTED);
    label(u, i, r, 10, u->layout.wide ? 29 : 17, w - 2 * p - 20, "UNAVAILABLE",
          &lv_font_montserrat_14, UI321_TEXT);
  }
}
static void quick(tyvor_ui321_t *u) {
  header(u);
  int w = u->port.width, p = u->layout.pad, g = u->layout.gap,
      cw = (w - 2 * p - 2 * g) / 3, ch = u->layout.wide ? 108 : 70;
  const char *names[] = {"INPUTS",    "OUTPUTS", "MIXES",
                         "TELEMETRY", "MODEL",   "SETTINGS"};
  unsigned pages[] = {UI321_INPUTS,    UI321_OUTPUTS, UI321_MIXES,
                      UI321_TELEMETRY, UI321_MODEL,   UI321_SETTINGS},
           icons[] = {UI321_ICON_INPUT, UI321_ICON_OUTPUT,
                      UI321_ICON_MIX,   UI321_ICON_TELEMETRY,
                      UI321_ICON_MODEL, UI321_ICON_SETTINGS};
  for (unsigned i = 0; i < 6; ++i) {
    u->actions[i] = tyvor_ui321_button(u->body, p + (int)(i % 3) * (cw + g),
                                       50 + (int)(i / 3) * (ch + g), cw, ch,
                                       names[i], icons[i]);
    lv_obj_set_style_radius(u->actions[i], 16, 0);
    tyvor_ui321_bind(u->actions[i], u, 0, (int)pages[i]);
  }
}
void tyvor_ui321_pages_create(tyvor_ui321_t *u) {
  lv_obj_clean(u->body);
  memset(u->field, 0, sizeof(u->field));
  for (unsigned i = 0; i < 7; ++i)
    u->bars[i] = NULL;
  memset(u->sticks, 0, sizeof(u->sticks));
  memset(u->actions, 0, sizeof(u->actions));
  u->safety = u->throttle = u->back = u->home = u->more = NULL;
  ++u->allocations_on_navigation;
  switch (u->page) {
  case UI321_HOME:
    home(u);
    break;
  case UI321_MODEL:
    model(u);
    break;
  case UI321_INPUTS:
    inputs(u);
    break;
  case UI321_MIXES:
    channels(u, 1);
    break;
  case UI321_OUTPUTS:
    channels(u, 0);
    break;
  case UI321_TELEMETRY:
    telemetry(u);
    break;
  case UI321_SETTINGS:
  case UI321_CONNECTIONS:
    settings(u);
    break;
  case UI321_QUICK:
    quick(u);
    break;
  default:
    break;
  }
}
static void set(tyvor_ui321_t *u, unsigned i, const char *s) {
  tyvor_ui321_text(u->field[i], s);
}
void tyvor_ui321_pages_update(tyvor_ui321_t *u) {
  const tyvor_ui321_view_t *v = &u->view;
  char s[96];
  if (u->field[46]) {
    snprintf(s, sizeof(s), "%s%s", v->base.mock ? "DEMO / " : "",
             v->base.connection);
    set(u, 46, s);
  }
  for (unsigned i = 0; i < 2; ++i)
    if (u->sticks[i]) {
      tyvor_ui321_stick_t next = {(int16_t)(i ? v->inputs[0] : v->inputs[3]),
                                  (int16_t)(i ? v->inputs[1] : v->inputs[2]),
                                  v->inputs_valid};
      if (next.x != u->stick_data[i].x || next.y != u->stick_data[i].y ||
          next.valid != u->stick_data[i].valid) {
        u->stick_data[i] = next;
        lv_obj_invalidate(u->sticks[i]);
      }
    }
  switch (u->page) {
  case UI321_HOME: {
    set(u, 0, v->timer);
    /* Preserve the authoritative Core safety state, including STARTUP/FAILSAFE.
       base.safe only means "not bench-armed", never "definitely disarmed". */
    const char *safety =
        v->base.quality != TYVOR_TFT_VALID ? v->base.connection : v->safety;
    if (v->base.quality == TYVOR_TFT_VALID && v->model_valid &&
        v->safety_state == TYVOR_DISARMED)
      safety = v->base.mock ? "SIM GATE SAFE" : "CORE GATE SAFE";
    tyvor_ui321_text(u->safety, safety);
    uint32_t color = v->base.quality == TYVOR_TFT_STALE ? UI321_WARNING
                     : v->base.quality != TYVOR_TFT_VALID || !v->model_valid
                         ? UI321_DANGER
                     : v->safety_state == TYVOR_FAILSAFE ? UI321_DANGER
                     : v->safety_state == TYVOR_STARTUP  ? UI321_WARNING
                     : v->base.safe                      ? UI321_SUCCESS
                                                         : UI321_WARNING;
    if (!lv_color_eq(lv_obj_get_style_text_color(u->safety, 0),
                     lv_color_hex(color)))
      lv_obj_set_style_text_color(u->safety, lv_color_hex(color), 0);
    if (v->base.quality != TYVOR_TFT_VALID)
      set(u, 1, "RX / ARM UNKNOWN");
    else if (v->base.lq_valid) {
      snprintf(s, sizeof(s), "ARM UNKNOWN | LQ %s", v->quality);
      set(u, 1, s);
    } else
      set(u, 1, "ARM UNKNOWN | LQ --%");
    tyvor_ui321_icon_state(lv_obj_get_child(u->actions[0], 0),
                           v->base.gps_valid ? UI321_ICON_VALID
                                             : UI321_ICON_MUTED);
    set(u, 9, v->base.gps_valid ? "GPS" : "GPS --");
    set(u, 2, v->voltage);
    set(u, 3, v->base.battery_valid ? "--%" : "NO DATA");
    int value = 0; /* Cell count/chemistry/charge percent unavailable. */
    if (lv_bar_get_value(u->bars[0]) != value)
      lv_bar_set_value(u->bars[0], value, LV_ANIM_OFF);
    set(u, 5,
        v->base.mock && v->base.quality == TYVOR_TFT_VALID ? "BENCH" : "N/A");
    break;
  }
  case UI321_MODEL:
    (void)tyvor_ui321_text_fit(u->field[1], v->base.model,
                               u->port.width - 2 * u->layout.pad - 28, 20);
    snprintf(s, sizeof(s), "ID %u / REV %u", (unsigned)v->model_id,
             (unsigned)v->model_revision);
    set(u, 2, v->base.quality == TYVOR_TFT_VALID ? s : "ID -- / REV --");
    set(u, 0,
        v->base.quality != TYVOR_TFT_VALID ? "MODEL UNAVAILABLE"
        : v->model_valid                   ? "ACTIVE / PUBLISHED"
                                           : "INVALID MODEL");
    if (u->subpage)
      set(u, 3, "LAYOUT EDITOR UNAVAILABLE");
    break;
  case UI321_INPUTS:
    for (unsigned i = 0; i < 4; ++i) {
      if (v->inputs_valid)
        snprintf(s, sizeof(s), "%c %+d", "RPTY"[i], v -> inputs[i]);
      else
        snprintf(s, sizeof(s), "%c --", "RPTY"[i]);
      set(u, i, s);
    }
    if (v->inputs_valid)
      snprintf(s, sizeof(s), "SA %+d    SB %+d    TRIM N/A", v->switches[0],
               v->switches[1]);
    else
      strcpy(s, "SWITCHES / TRIM UNAVAILABLE");
    set(u, 4, s);
    break;
  case UI321_MIXES:
    for (unsigned i = 0; i < 4; ++i) {
      unsigned index = u->subpage * 4 + i;
      if (v->mixes_valid && index < v->mix_count) {
        const tyvor_mix_view_t *m = &v->mixes[index];
        snprintf(s, sizeof(s), "%02u I%u > B%u %s", index + 1, m->source + 1,
                 m->destination + 1, m->operation ? "REPLACE" : "ADD");
        set(u, i * 2, s);
        snprintf(s, sizeof(s), "W %+d%%  O %+d  T %+d", m->weight, m->offset,
                 m->trim);
        set(u, i * 2 + 1, s);
      } else {
        set(u, i * 2, "MIX UNAVAILABLE");
        set(u, i * 2 + 1, "W --  O --  T --");
      }
    }
    break;
  case UI321_OUTPUTS:
    for (unsigned i = 0; i < 4; ++i) {
      unsigned index = u->subpage * 4 + i;
      snprintf(s, sizeof(s), "CH %02u", index + 1);
      set(u, i * 2, s);
      if (v->outputs_valid)
        snprintf(s, sizeof(s), "%+d", v->channels[index]);
      else
        strcpy(s, "--");
      set(u, i * 2 + 1, s);
      tyvor_ui321_bar_set(u->bars[i], v->channels[index], v->outputs_valid);
    }
    break;
  case UI321_TELEMETRY:
    if (u->subpage) {
      set(u, 0, v->gps_lat);
      set(u, 1, v->gps_lon);
      set(u, 2, v->base.gps_valid ? "GPS VALID" : "GPS UNAVAILABLE");
      set(u, 3, "UNKNOWN / READ ONLY");
    } else {
      set(u, 0, v->voltage);
      snprintf(s, sizeof(s), "%s / %s", v->quality, v->signal);
      set(u, 1, s);
      set(u, 2,
          v->base.gps_valid ? (v->base.mock ? "VALID / GPS DEMO" : "GPS VALID")
                            : "GPS UNAVAILABLE");
      snprintf(s, sizeof(s), "%u ms", (unsigned)v->age_ms);
      set(u, 3, v->base.quality == TYVOR_TFT_VALID ? s : "UNAVAILABLE");
    }
    break;
  case UI321_CONNECTIONS:
    set(u, 0, "BRIGHTNESS UNAVAILABLE");
    set(u, 1, "WIRELESS UNAVAILABLE");
    set(u, 2, "GATES OFF / NO TOUCH IC");
    set(u, 3, "NO CONNECTION COMMANDS");
    break;
  case UI321_SETTINGS:
    snprintf(s, sizeof(s), "%dx%d / LANDSCAPE", u->port.width, u->port.height);
    set(u, 0, s);
    set(u, 1, u->subpage ? "0.5.1-ui-v3-2-1 / PROTOCOL2" : "TYVOR / LVGL9.2.2");
    set(u, 2, "HARDWARE OFF");
    set(u, 3, "UNAVAILABLE / READ ONLY");
    break;
  default:
    break;
  }
}
