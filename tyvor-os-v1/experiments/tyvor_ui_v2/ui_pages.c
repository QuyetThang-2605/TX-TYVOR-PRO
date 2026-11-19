#include "ui.h"
#include <stdio.h>
#include <string.h>
static lv_obj_t *field(tyvor_ui2_t *u, unsigned slot, lv_obj_t *p, int x, int y,
                       int w, const char *s, const lv_font_t *font,
                       uint32_t c) {
  u->field[slot] = tyvor_ui2_label(p, x, y, w, s, font, c);
  return u->field[slot];
}
static void center(lv_obj_t *o) {
  lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
}
static void header(tyvor_ui2_t *u) {
  int w = u->layout.width - 2 * u->layout.pad;
  u->page_title =
      tyvor_ui2_label(u->body, 0, 2, w - 50, tyvor_ui2_page_name(u->page),
                      &lv_font_montserrat_14, UI2_TEXT);
  if (u->page == UI2_MODEL || u->page == UI2_MIXES || u->page == UI2_OUTPUTS ||
      u->page == UI2_TELEMETRY || u->page == UI2_SETTINGS) {
    u->actions[7] =
        tyvor_ui2_button(u->body, w - 44, 0, 44, 44, NULL, UI2_ICON_MORE);
    tyvor_ui2_bind(u->actions[7], u, UI2_MORE, -1);
    lv_obj_t *t = tyvor_ui2_label(u->body, 0, 23, w - 52, "",
                                  &lv_font_montserrat_12, UI2_MUTED);
    u->field[47] = t;
  }
}
static void home(tyvor_ui2_t *u) {
  int w = u->layout.width - 2 * u->layout.pad, wide = u->layout.wide,
      g = u->layout.gap;
  int hero_h = wide ? 150 : 100, stats_h = wide ? 72 : 48, stat_y = hero_h + g,
      ww = (w - g) / 2;
  lv_obj_t *hero = tyvor_ui2_box(u->body, 0, 0, w, hero_h, UI2_SURFACE);
  center(field(u, 1, hero, 12, wide ? 14 : 6, w - 24, "SIMULATOR TIMER",
               &lv_font_montserrat_12, UI2_MUTED));
  center(field(u, 0, hero, 6, wide ? 43 : 22, w - 12, "--:--",
               &lv_font_montserrat_48, UI2_TEXT));
  field(u, 2, hero, 12, hero_h - 22, w - 24, "RX / MODE UNAVAILABLE",
        &lv_font_montserrat_12, UI2_MUTED);
  lv_obj_t *mode = field(u, 3, hero, w / 2, hero_h - 22, w / 2 - 12,
                         "UNAVAILABLE", &lv_font_montserrat_12, UI2_ACCENT);
  lv_obj_set_style_text_align(mode, LV_TEXT_ALIGN_RIGHT, 0);
  const char *titles[] = {"BATTERY", "LINK QUALITY"};
  for (unsigned i = 0; i < 2; ++i) {
    lv_obj_t *card = tyvor_ui2_box(u->body, (int)i * (ww + g), stat_y, ww,
                                   stats_h, UI2_CARD);
    tyvor_ui2_label(card, 10, 7, ww - 20, titles[i], &lv_font_montserrat_12,
                    UI2_MUTED);
    field(u, 4 + i, card, 10, wide ? 30 : 23, ww - 20, "--",
          &lv_font_montserrat_20, i ? UI2_ACCENT : UI2_TEXT);
  }
  int y = stat_y + stats_h + g;
  u->actions[0] =
      tyvor_ui2_button(u->body, 0, y, ww, 44, "INPUTS", UI2_ICON_INPUT);
  tyvor_ui2_bind(u->actions[0], u, 0, UI2_INPUTS);
  u->actions[1] = tyvor_ui2_button(u->body, ww + g, y, ww, 44, "TELEMETRY",
                                   UI2_ICON_TELEMETRY);
  tyvor_ui2_bind(u->actions[1], u, 0, UI2_TELEMETRY);
  if (wide) {
    int yy = y + 54;
    tyvor_ui2_label(u->body, 0, yy, w, "STICK ACTIVITY / READ ONLY",
                    &lv_font_montserrat_12, UI2_MUTED);
    for (unsigned i = 0; i < 4; ++i)
      u->bars[i] =
          tyvor_ui2_bar(u->body, (int)i * (w / 4), yy + 22, w / 4 - 10, 6);
  }
}
static void model(tyvor_ui2_t *u) {
  header(u);
  int w = u->layout.width - 2 * u->layout.pad, wide = u->layout.wide, y = 48;
  lv_obj_t *card = tyvor_ui2_box(u->body, 0, y, w, wide ? 114 : 88, UI2_CARD);
  tyvor_ui2_label(card, 12, 10, w - 24,
                  u->subpage ? "MODEL DETAIL / READ ONLY" : "PUBLISHED MODEL",
                  &lv_font_montserrat_12, UI2_ACCENT);
  field(u, 0, card, 12, wide ? 36 : 30, w - 24, "--", &lv_font_montserrat_20,
        UI2_TEXT);
  field(u, 1, card, 12, wide ? 80 : 62, w - 24, "ID -- / REV --",
        &lv_font_montserrat_12, UI2_MUTED);
  y += wide ? 126 : 96;
  field(u, 2, u->body, 0, y, w, "MODEL UNAVAILABLE", &lv_font_montserrat_12,
        UI2_MUTED);
  field(u, 3, u->body, 0, y + 22, w,
        u->subpage ? "Switch model: UNAVAILABLE" : "SELECTION ON STM32 ONLY",
        &lv_font_montserrat_12, UI2_MUTED);
  if (wide) {
    field(u, 4, u->body, 0, y + 58, w, "One model published by protocol v2.",
          &lv_font_montserrat_14, UI2_MUTED);
    field(u, 5, u->body, 0, y + 82, w, "No remote model selection enabled.",
          &lv_font_montserrat_14, UI2_MUTED);
  }
}
static void inputs(tyvor_ui2_t *u) {
  header(u);
  int w = u->layout.width - 2 * u->layout.pad, wide = u->layout.wide,
      g = u->layout.gap;
  int size = wide ? 120 : 78, y = wide ? 50 : 31;
  for (unsigned i = 0; i < 2; ++i) {
    int x = i ? w - size : 0;
    lv_obj_t *box = tyvor_ui2_box(u->body, x, y, size, size, UI2_CARD);
    u->sticks[i] = box;
    tyvor_ui2_box(box, size / 2, 8, 1, size - 16, UI2_MUTED);
    tyvor_ui2_box(box, 8, size / 2, size - 16, 1, UI2_MUTED);
    u->stick_dots[i] =
        tyvor_ui2_box(box, size / 2 - 4, size / 2 - 4, 9, 9, UI2_ACCENT);
    lv_obj_set_style_radius(u->stick_dots[i], LV_RADIUS_CIRCLE, 0);
  }
  field(u, 0, u->body, 0, y + size + 6, w, "R --   P --",
        &lv_font_montserrat_12, UI2_TEXT);
  field(u, 1, u->body, 0, y + size + 25, w, "T --   Y --",
        &lv_font_montserrat_12, UI2_TEXT);
  field(u, 2, u->body, 0, y + size + 48, w, "SA --    SB --",
        &lv_font_montserrat_12, UI2_MUTED);
  field(u, 3, u->body, 0, y + size + 69, w, "NORMALIZED / READ ONLY",
        &lv_font_montserrat_12, UI2_MUTED);
  (void)g;
}
static void channels(tyvor_ui2_t *u, int mixes) {
  header(u);
  int w = u->layout.width - 2 * u->layout.pad, wide = u->layout.wide,
      row = wide ? 59 : 37;
  for (unsigned i = 0; i < 4; ++i) {
    int y = 48 + (int)i * row;
    lv_obj_t *card = tyvor_ui2_box(u->body, 0, y, w, row - 4, UI2_CARD);
    field(u, i * 2, card, 8, 3, mixes ? w - 16 : 55, "--",
          &lv_font_montserrat_12, UI2_TEXT);
    if (mixes)
      field(u, i * 2 + 1, card, 8, wide ? 29 : 18, w - 16, "--",
            &lv_font_montserrat_12, UI2_MUTED);
    else {
      lv_obj_t *value = field(u, i * 2 + 1, card, w - 70, 5, 62, "--",
                              &lv_font_montserrat_12, UI2_ACCENT);
      lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
      u->bars[i] = tyvor_ui2_bar(card, 8, wide ? 35 : 24, w - 16, wide ? 8 : 4);
    }
  }
  if (wide)
    field(u, 10, u->body, 0, 300, w,
          mixes ? "Logical mixes / no editing" : "MEMORY CHANNELS / NO RF",
          &lv_font_montserrat_12, UI2_MUTED);
}
static void telemetry(tyvor_ui2_t *u) {
  header(u);
  int w = u->layout.width - 2 * u->layout.pad, wide = u->layout.wide;
  if (!u->subpage) {
    int gap = u->layout.gap, cw = (w - gap) / 2, ch = wide ? 82 : 54;
    const char *names[] = {"BATTERY", "LINK QUALITY", "RSSI", "GPS"};
    for (unsigned i = 0; i < 4; ++i) {
      lv_obj_t *card =
          tyvor_ui2_box(u->body, (int)(i % 2) * (cw + gap),
                        48 + (int)(i / 2) * (ch + gap), cw, ch, UI2_CARD);
      tyvor_ui2_label(card, 8, 7, cw - 16, names[i], &lv_font_montserrat_12,
                      UI2_MUTED);
      field(u, i, card, 8, wide ? 36 : 27, cw - 16, "--",
            &lv_font_montserrat_14, i == 1 ? UI2_ACCENT : UI2_TEXT);
    }
    field(u, 4, u->body, 0, 48 + 2 * (ch + gap) + 3, w, "AGE --",
          &lv_font_montserrat_12, UI2_MUTED);
    if (wide)
      field(u, 5, u->body, 0, 48 + 2 * (ch + gap) + 30, w,
            "Telemetry fields expire independently.", &lv_font_montserrat_12,
            UI2_MUTED);
  } else {
    const char *names[] = {"LATITUDE", "LONGITUDE", "GPS VALIDITY",
                           "CORE STATUS AGE"};
    for (unsigned i = 0; i < 4; ++i) {
      int y = 48 + (int)i * (wide ? 64 : 38);
      tyvor_ui2_label(u->body, 0, y, w, names[i], &lv_font_montserrat_12,
                      UI2_MUTED);
      field(u, i, u->body, 0, y + 16, w, "--", &lv_font_montserrat_14,
            UI2_TEXT);
    }
  }
}
static void settings(tyvor_ui2_t *u) {
  header(u);
  int w = u->layout.width - 2 * u->layout.pad, wide = u->layout.wide,
      row = wide ? 62 : 38;
  const char *a[] = {"DISPLAY / LOCAL UI", "SYSTEM", "DIAGNOSTICS", "HARDWARE"};
  const char *b[] = {"UART SESSION", "FIRMWARE", "TOUCH CALIBRATION",
                     "WIDGET LAYOUT"};
  for (unsigned i = 0; i < 4; ++i) {
    lv_obj_t *card =
        tyvor_ui2_box(u->body, 0, 48 + (int)i * row, w, row - 4, UI2_SURFACE);
    tyvor_ui2_label(card, 8, 3, w - 16, u->subpage ? b[i] : a[i],
                    &lv_font_montserrat_12, UI2_MUTED);
    field(u, i, card, 8, wide ? 27 : 18, w - 16, "UNAVAILABLE",
          &lv_font_montserrat_12, UI2_TEXT);
  }
  if (wide)
    field(u, 6, u->body, 0, 307, w, "Hardware-only options are disabled.",
          &lv_font_montserrat_12, UI2_MUTED);
}
static void quick(tyvor_ui2_t *u) {
  header(u);
  int w = u->layout.width - 2 * u->layout.pad, g = u->layout.gap,
      cw = (w - g) / 2, ch = u->layout.wide ? 74 : 48;
  const char *names[] = {"INPUTS",    "OUTPUTS", "MIXES",
                         "TELEMETRY", "MODEL",   "SETTINGS"};
  unsigned page[] = {UI2_INPUTS,    UI2_OUTPUTS, UI2_MIXES,
                     UI2_TELEMETRY, UI2_MODEL,   UI2_SETTINGS};
  unsigned icons[] = {UI2_ICON_INPUT,     UI2_ICON_OUTPUT, UI2_ICON_MIX,
                      UI2_ICON_TELEMETRY, UI2_ICON_MODEL,  UI2_ICON_SETTINGS};
  for (unsigned i = 0; i < 6; ++i) {
    u->actions[i] = tyvor_ui2_button(u->body, (int)(i % 2) * (cw + g),
                                     34 + (int)(i / 2) * (ch + g), cw, ch,
                                     names[i], icons[i]);
    tyvor_ui2_bind(u->actions[i], u, 0, (int)page[i]);
  }
  if (u->layout.wide)
    field(u, 0, u->body, 0, 300, w, "VIEW ONLY / NO AIRCRAFT COMMANDS",
          &lv_font_montserrat_12, UI2_MUTED);
}
void tyvor_ui2_pages_create(tyvor_ui2_t *u) {
  for (unsigned i = 0; i < 4; ++i) {
    int active = (i == 1 && u->page == UI2_HOME) ||
                 (i == 2 && u->page == UI2_MODEL) ||
                 (i == 3 && u->page == UI2_QUICK);
    lv_obj_set_style_bg_color(u->nav[i],
                              lv_color_hex(active ? UI2_CARD : UI2_SURFACE), 0);
    lv_obj_t *caption = lv_obj_get_child(u->nav[i], 0);
    lv_obj_set_style_text_color(caption,
                                lv_color_hex(active ? UI2_TEXT : UI2_MUTED), 0);
  }
  lv_obj_clean(u->body);
  memset(u->field, 0, sizeof(u->field));
  memset(u->bars, 0, sizeof(u->bars));
  memset(u->actions, 0, sizeof(u->actions));
  memset(u->sticks, 0, sizeof(u->sticks));
  lv_obj_set_size(u->body, u->layout.width - 2 * u->layout.pad,
                  u->layout.body_h);
  ++u->allocations_on_navigation;
  switch (u->page) {
  case UI2_HOME:
    home(u);
    break;
  case UI2_MODEL:
    model(u);
    break;
  case UI2_INPUTS:
    inputs(u);
    break;
  case UI2_MIXES:
    channels(u, 1);
    break;
  case UI2_OUTPUTS:
    channels(u, 0);
    break;
  case UI2_TELEMETRY:
    telemetry(u);
    break;
  case UI2_SETTINGS:
    settings(u);
    break;
  case UI2_QUICK:
    quick(u);
    break;
  default:
    break;
  }
}
static void set(tyvor_ui2_t *u, unsigned i, const char *s) {
  tyvor_ui2_text(u->field[i], s);
}
void tyvor_ui2_pages_update(tyvor_ui2_t *u) {
  const tyvor_ui2_view_t *v = &u->view;
  char s[80];
  if (u->field[47]) {
    unsigned total = u->page == UI2_MIXES ? (v->mix_count + 3) / 4 : 2;
    if (!total)
      total = 1;
    (void)snprintf(s, sizeof(s), "%u/%u  MORE >", u->subpage + 1, total);
    set(u, 47, s);
  }
  switch (u->page) {
  case UI2_HOME:
    set(u, 0, v->timer);
    set(u, 1,
        v->base.mock ? "SIM TIMER / ARM UNKNOWN" : "FLIGHT TIMER UNAVAILABLE");
    if (strcmp(lv_label_get_text(u->top_model), v->base.model))
      set(u, 1, v->base.model); /* Long names remain complete on HOME. */
    set(u, 2,
        v->base.mock && v->base.quality == TYVOR_TFT_VALID
            ? v->receiver
            : "RX / MODE UNAVAILABLE");
    set(u, 3,
        v->base.mock && v->base.quality == TYVOR_TFT_VALID ? v->flight_mode
                                                           : "");
    set(u, 4, v->voltage);
    set(u, 5, v->quality);
    for (unsigned i = 0; i < 4; ++i)
      tyvor_ui2_bar_set(u->bars[i], v->inputs[i], v->inputs_valid);
    break;
  case UI2_MODEL:
    (void)tyvor_ui2_text_fit(u->field[0], v->base.model,
                             u->layout.width - 2 * u->layout.pad - 24, 20);
    (void)snprintf(s, sizeof(s), "ID %u  /  REV %u", (unsigned)v->model_id,
                   (unsigned)v->model_revision);
    set(u, 1, v->base.quality == TYVOR_TFT_VALID ? s : "ID --  /  REV --");
    set(u, 2,
        v->base.quality != TYVOR_TFT_VALID ? "MODEL UNAVAILABLE"
        : v->model_valid                   ? "ACTIVE / PUBLISHED"
                                           : "INVALID MODEL / BLOCKED");
    break;
  case UI2_INPUTS:
    if (v->inputs_valid) {
      (void)snprintf(s, sizeof(s), "R %+d   P %+d", v->inputs[0], v->inputs[1]);
      set(u, 0, s);
      (void)snprintf(s, sizeof(s), "T %+d   Y %+d", v->inputs[2], v->inputs[3]);
      set(u, 1, s);
      (void)snprintf(s, sizeof(s), "SA %+d    SB %+d", v->switches[0],
                     v->switches[1]);
      set(u, 2, s);
    } else {
      set(u, 0, "R --   P --");
      set(u, 1, "T --   Y --");
      set(u, 2, "SWITCHES UNAVAILABLE");
    }
    for (unsigned i = 0; i < 2; ++i) {
      int size = u->layout.wide ? 120 : 78, x = i ? v->inputs[0] : v->inputs[3],
          y = i ? v->inputs[1] : v->inputs[2];
      if (x < -1000)
        x = -1000;
      if (x > 1000)
        x = 1000;
      if (y < -1000)
        y = -1000;
      if (y > 1000)
        y = 1000;
      lv_obj_set_pos(
          u->stick_dots[i],
          size / 2 - 4 + (v->inputs_valid ? x * (size / 2 - 12) / 1000 : 0),
          size / 2 - 4 - (v->inputs_valid ? y * (size / 2 - 12) / 1000 : 0));
      lv_color_t color = lv_color_hex(v->inputs_valid ? UI2_ACCENT : UI2_MUTED);
      if (!lv_color_eq(lv_obj_get_style_bg_color(u->stick_dots[i], 0), color))
        lv_obj_set_style_bg_color(u->stick_dots[i], color, 0);
    }
    break;
  case UI2_MIXES:
    for (unsigned i = 0; i < 4; ++i) {
      unsigned index = u->subpage * 4 + i;
      if (v->mixes_valid && index < v->mix_count) {
        const tyvor_mix_view_t *m = &v->mixes[index];
        (void)snprintf(s, sizeof(s), "%02u  I%u > B%u  %s", index + 1,
                       m->source + 1, m->destination + 1,
                       m->operation ? "REPLACE" : "ADD");
        set(u, i * 2, s);
        (void)snprintf(s, sizeof(s), "W %+d%%   O %+d   T %+d", m->weight,
                       m->offset, m->trim);
        set(u, i * 2 + 1, s);
      } else {
        set(u, i * 2, "-- / MIX UNAVAILABLE");
        set(u, i * 2 + 1, "W --   O --   T --");
      }
    }
    break;
  case UI2_OUTPUTS:
    for (unsigned i = 0; i < 4; ++i) {
      unsigned index = u->subpage * 4 + i;
      (void)snprintf(s, sizeof(s), "CH %02u", index + 1);
      set(u, i * 2, s);
      if (v->outputs_valid)
        (void)snprintf(s, sizeof(s), "%+d", v->channels[index]);
      else
        strcpy(s, "--");
      set(u, i * 2 + 1, s);
      tyvor_ui2_bar_set(u->bars[i], v->channels[index], v->outputs_valid);
    }
    break;
  case UI2_TELEMETRY:
    if (!u->subpage) {
      set(u, 0, v->voltage);
      set(u, 1, v->quality);
      set(u, 2, v->signal);
      set(u, 3,
          v->base.gps_valid ? (v->base.mock ? "VALID (D)" : "VALID")
                            : "NO DATA");
      (void)snprintf(s, sizeof(s), "STATUS AGE  %u ms%s", (unsigned)v->age_ms,
                     v->base.mock ? " / DEMO" : "");
      set(u, 4,
          v->base.quality == TYVOR_TFT_VALID ? s : "STATUS AGE  UNAVAILABLE");
    } else {
      set(u, 0, v->gps_lat);
      set(u, 1, v->gps_lon);
      set(u, 2,
          v->base.gps_valid ? (v->base.mock ? "VALID / DEMO" : "VALID")
                            : "UNAVAILABLE");
      (void)snprintf(s, sizeof(s), "%u ms", (unsigned)v->age_ms);
      set(u, 3, v->base.quality == TYVOR_TFT_VALID ? s : "UNAVAILABLE");
    }
    break;
  case UI2_SETTINGS:
    if (!u->subpage) {
      (void)snprintf(s, sizeof(s), "%dx%d / RGB565", u->port.width,
                     u->port.height);
      set(u, 0, s);
      set(u, 1, "TYVOR UI V2 / LVGL 9.2.2");
      set(u, 2, "LOGS / LOCAL MONITOR");
      set(u, 3, "DISPLAY / TOUCH / UART OFF");
    } else {
      set(u, 0, v->base.connection);
      set(u, 1, "0.4.0-ui-v2 / PROTOCOL 2");
      set(u, 2, "DISABLED / IC UNVERIFIED");
      set(u, 3, "UNAVAILABLE / VIEW ONLY");
    }
    break;
  default:
    break;
  }
}
