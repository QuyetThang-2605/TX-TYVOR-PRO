#include "tyvor_ui.h"
#include "tyvor_protocol.h"
#include <stdio.h>
#include <string.h>
/* Fixed, escaped fixture text only. SVG virtual display; no panel dependency. */
static void xml(FILE *f, const char *s) {
    for (; *s; ++s) {
        if (*s == '&') fputs("&amp;", f);
        else if (*s == '<') fputs("&lt;", f);
        else if (*s == '>') fputs("&gt;", f);
        else fputc(*s, f);
    }
}
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: tyvor_ui_sim EXISTING_OUTPUT_DIRECTORY\n"); return 2; }
    tyvor_snapshot_t snapshot;
    tyvor_snapshot_init(&snapshot);
    tyvor_status_t mock={.arm_valid=0, .arm=0, .lq=99, .flags=7, .battery_mv=8100};
    const char *states[]={"mock", "stale", "no-data"};
    for (unsigned state=0; state<3; ++state) {
        tyvor_snapshot_init(&snapshot);
        if (state != 2 && tyvor_snapshot_set(&snapshot, &mock, 100) != 0) return 1;
        if (state == 1) (void)tyvor_snapshot_poll(&snapshot, 100+TYVOR_STALE_MS, TYVOR_STALE_MS);
        for (unsigned page=0; page<TYVOR_UI_PAGES; ++page) {
            tyvor_ui_view_t v;
            if (tyvor_ui_build(page, &snapshot, &v) != 0) return 1;
            char path[1024];
            int n=snprintf(path, sizeof(path), "%s/%s-%u.svg", argv[1], states[state], page);
            if (n < 0 || (size_t)n >= sizeof(path)) return 1;
            FILE *f=fopen(path, "wb");
            if (!f) { perror(path); return 1; }
            fputs("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"368\" height=\"448\" viewBox=\"0 0 368 448\"><rect width=\"368\" height=\"448\" rx=\"22\" fill=\"#101923\"/><g font-family=\"Segoe UI,sans-serif\" fill=\"#e7edf4\"><text x=\"24\" y=\"35\" font-size=\"13\" fill=\"#9fb1c6\">TYVOR OS / VIRTUAL DISPLAY</text><text x=\"24\" y=\"78\" font-size=\"25\" font-weight=\"700\">", f);
            xml(f, v.title);
            fputs("</text><rect x=\"20\" y=\"98\" width=\"328\" height=\"34\" rx=\"8\" fill=\"#293d4c\"/><text x=\"32\" y=\"120\" font-size=\"13\" fill=\"#ffc676\">", f);
            xml(f, v.banner); fputs("</text>", f);
            for (unsigned row=0; row<TYVOR_UI_ROWS; ++row) {
                fprintf(f, "<text x=\"24\" y=\"%u\" font-size=\"15\">", 171+row*35);
                xml(f, v.rows[row]); fputs("</text>", f);
            }
            fprintf(f, "<text x=\"24\" y=\"420\" font-size=\"12\" fill=\"#9fb1c6\">PAGE %u / 7 - MOCK UI, NO HARDWARE OUTPUT</text></g></svg>\n", page+1);
            if (fclose(f) != 0) return 1;
        }
    }
    puts("Generated 21 read-only 368x448 SVG views (mock, stale, no-data).");
    return 0;
}
