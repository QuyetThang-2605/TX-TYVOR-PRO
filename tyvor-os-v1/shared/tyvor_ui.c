#include "tyvor_ui.h"
#include <stdio.h>
#include <string.h>
int tyvor_ui_build(unsigned page, const tyvor_snapshot_t *s, tyvor_ui_view_t *v) {
    static const char *titles[TYVOR_UI_PAGES]={"HOME", "MODEL", "INPUTS", "MIXES", "OUTPUTS", "TELEMETRY", "RADIO SETTINGS"};
    static const char *fixtures[TYVOR_UI_PAGES][TYVOR_UI_ROWS]={
        {"TYVOR OS / Phase 1", "Model: BENCH MOCK", "", "", "", "Read-only / RF unavailable"},
        {"BENCH MOCK", "Profile: simulator fixture", "Model selection: unavailable", "Storage: not implemented", "Configuration: read-only", "No active RF model"},
        {"MOCK channels / not live ADC", "Roll: 0%", "Pitch: 0%", "Throttle: -100%", "Yaw: 0%", "Calibration: unavailable"},
        {"MOCK mixer layout", "CH1: Roll", "CH2: Pitch", "CH3: Throttle", "CH4: Yaw", "Mixer engine: not implemented"},
        {"MOCK channel monitor", "CH1: 0%    CH2: 0%", "CH3: -100% CH4: 0%", "Hardware output: disabled", "RF module: unavailable", "No ARM control"},
        {"", "", "", "", "GPS: UNKNOWN", "Telemetry source: bench only"},
        {"MOCK radio preferences", "Canvas: 368 x 448", "Panel driver: unavailable", "Touch driver: unverified", "Brightness: fixture 60%", "Configuration: read-only"}
    };
    if (page >= TYVOR_UI_PAGES || !s || !v) return -1;
    memset(v, 0, sizeof(*v)); v->title=titles[page];
    (void)snprintf(v->banner, sizeof(v->banner), "%s", s->valid ?
        ((s->status.flags & TYVOR_SOURCE_MOCK) ? "MOCK / READ ONLY" : "READ ONLY / STATUS") :
        (s->received_once ? "STALE / INVALID" : "NO DATA / INVALID"));
    for (unsigned i=0; i<TYVOR_UI_ROWS; ++i)
        (void)snprintf(v->rows[i], sizeof(v->rows[i]), "%s", fixtures[page][i]);
    if (page == 0 || page == 5) {
        unsigned first=page == 0 ? 2u : 0u;
        (void)snprintf(v->rows[first], sizeof(v->rows[first]), "ARM: %s",
            s->valid && s->status.arm_valid ? (s->status.arm ? "ON" : "OFF") : "UNKNOWN");
        if (s->valid && (s->status.flags & TYVOR_VALID_BATTERY))
            (void)snprintf(v->rows[first+1], sizeof(v->rows[0]), "Battery: %u mV", (unsigned)s->status.battery_mv);
        else (void)snprintf(v->rows[first+1], sizeof(v->rows[0]), "Battery: UNKNOWN");
        if (s->valid && (s->status.flags & TYVOR_VALID_LQ))
            (void)snprintf(v->rows[first+2], sizeof(v->rows[0]), "LQ: %u%%", (unsigned)s->status.lq);
        else (void)snprintf(v->rows[first+2], sizeof(v->rows[0]), "LQ: UNKNOWN");
        if (page == 5) (void)snprintf(v->rows[3], sizeof(v->rows[0]), "Snapshot: %s", s->valid ? "VALID" : "INVALID");
    }
    return 0;
}
