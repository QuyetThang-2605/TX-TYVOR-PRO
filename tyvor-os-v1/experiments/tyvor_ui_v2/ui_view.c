#include "ui_view.h"
#include <stdio.h>
#include <string.h>
static void coordinate(char *out, size_t cap, int32_t value) {
  int64_t v = value;
  uint32_t a = (uint32_t)(v < 0 ? -v : v);
  (void)snprintf(out, cap, "%s%u.%05u", v < 0 ? "-" : "",
                 (unsigned)(a / 10000000), (unsigned)(a % 10000000 / 100));
}
int tyvor_ui2_project(const tyvor_ui_state_t *source, uint64_t now,
                      int simulator, uint64_t timer_ms, tyvor_ui2_view_t *v) {
  if (!source || !v)
    return -1;
  tyvor_ui_state_t state = *source;
  tyvor_ui_state_poll(&state, now);
  memset(v, 0, sizeof(*v));
  if (tyvor_tft_project(&state, now, simulator, timer_ms, &v->base))
    return -1;
  strcpy(v->timer, "--:--");
  strcpy(v->voltage, "--.-- V");
  strcpy(v->quality, "--%");
  strcpy(v->signal, "-- dBm");
  strcpy(v->gps_lat, "UNAVAILABLE");
  strcpy(v->gps_lon, "UNAVAILABLE");
  strcpy(v->safety, "UNKNOWN");
  strcpy(v->throttle, "LOCK UNKNOWN");
  strcpy(v->receiver, "RX UNAVAILABLE");
  strcpy(v->flight_mode, "UNAVAILABLE");
  if (v->base.quality != TYVOR_TFT_VALID)
    return 0;
  const tyvor_core_snapshot_t *s = &state.snapshot;
  v->model_valid = (uint8_t)(!(s->safety_reasons & TYVOR_SAFE_MODEL));
  v->model_id = s->model_id;
  v->model_revision = s->model_revision;
  v->safety_state = s->safety_state;
  v->throttle_lock = (uint8_t)(s->throttle_lock != 0);
  strcpy(v->throttle, v->throttle_lock ? "THR LOCKED" : "THR UNLOCKED");
  strcpy(v->safety, !v->model_valid                        ? "INVALID MODEL"
                    : s->safety_state == TYVOR_BENCH_ARMED ? "BENCH ARM / SIM"
                    : s->safety_state == TYVOR_FAILSAFE    ? "FAILSAFE"
                    : s->safety_state == TYVOR_STARTUP     ? "SAFE / STARTUP"
                                                           : "SAFE / DISARMED");
  if (now >= s->captured_ms && now - s->captured_ms <= UINT32_MAX)
    v->age_ms = (uint32_t)(now - s->captured_ms);
  v->inputs_valid = s->input_valid;
  v->outputs_valid =
      (uint8_t)(s->outputs_valid && v->inputs_valid && v->model_valid);
  if (v->inputs_valid) {
    memcpy(v->inputs, s->inputs, sizeof(v->inputs));
    memcpy(v->switches, s->switches, sizeof(v->switches));
  }
  if (v->outputs_valid)
    memcpy(v->channels, s->channels, sizeof(v->channels));
  v->mixes_valid =
      (uint8_t)(v->model_valid && state.mix_valid &&
                state.mix_generation == s->generation && state.mix_count <= 16);
  if (v->mixes_valid) {
    v->mix_count = state.mix_count;
    memcpy(v->mixes, state.mixes, sizeof(v->mixes));
  }
  v->rf_valid = 1;
  v->rf_enabled =
      (uint8_t)(s->rf_enabled !=
                0); /* Published read-only logical flag, never enables RF. */
  if (v->base.timer_valid) {
    uint64_t sec = timer_ms / 1000;
    if (sec > 5999)
      sec = 5999;
    (void)snprintf(v->timer, sizeof(v->timer), "%02u:%02u",
                   (unsigned)(sec / 60), (unsigned)(sec % 60));
  }
  if (v->base.battery_valid)
    (void)snprintf(v->voltage, sizeof(v->voltage), "%u.%02u V",
                   s->telemetry.battery_mv / 1000,
                   s->telemetry.battery_mv % 1000 / 10);
  if (v->base.lq_valid)
    (void)snprintf(v->quality, sizeof(v->quality), "%u%%", s->telemetry.lq);
  if (v->base.rssi_valid)
    (void)snprintf(v->signal, sizeof(v->signal), "%d dBm",
                   s->telemetry.rssi_dbm);
  if (v->base.gps_valid) {
    coordinate(v->gps_lat, sizeof(v->gps_lat), s->telemetry.latitude_e7);
    coordinate(v->gps_lon, sizeof(v->gps_lon), s->telemetry.longitude_e7);
  }
  /* UART Core link, RF flag and LQ are not proof of an actual receiver
   * connection. */
  if (simulator) {
    strcpy(v->receiver, "RX DEMO");
    strcpy(v->flight_mode, "BENCH DEMO");
  }
  return 0;
}
void tyvor_ui2_fixture(tyvor_ui_state_t *s, uint64_t now, unsigned scenario) {
  tyvor_tft_fixture(s, now, scenario == 1 ? 1 : scenario == 2 ? 2 : 0);
  s->snapshot.captured_ms = now;
  s->snapshot.model_id = 1;
  s->snapshot.model_revision = 3;
  strcpy(s->snapshot.model_name, "Kestrel 5");
  s->snapshot.throttle_lock = 1;
  s->snapshot.input_valid = 1;
  s->snapshot.outputs_valid = 1;
  const int16_t values[] = {245, -160, -1000, 80};
  memcpy(s->snapshot.inputs, values, sizeof(values));
  s->snapshot.switches[0] = -1;
  s->snapshot.switches[1] = 1;
  for (unsigned i = 0; i < 8; ++i)
    s->snapshot.channels[i] = (int16_t)((int)i * 210 - 720);
  s->mix_valid = 1;
  s->mix_count = 6;
  s->mix_generation = s->snapshot.generation;
  s->mix_expires_ms = now + 500;
  for (unsigned i = 0; i < 6; ++i)
    s->mixes[i] = (tyvor_mix_view_t){(uint8_t)(i % 4),
                                     (uint8_t)i,
                                     0,
                                     (int16_t)(i == 4 ? -65 : 100),
                                     (int16_t)(i == 5 ? 10 : 0),
                                     0};
  if (scenario == 0)
    s->snapshot.safety_state = TYVOR_BENCH_ARMED;
  if (scenario == 4) {
    s->snapshot.safety_state = TYVOR_FAILSAFE;
    s->snapshot.safety_reasons = TYVOR_SAFE_MODEL;
    s->snapshot.outputs_valid = 0;
  }
}
