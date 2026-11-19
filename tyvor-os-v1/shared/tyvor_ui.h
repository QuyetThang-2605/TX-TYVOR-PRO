#ifndef TYVOR_UI_H
#define TYVOR_UI_H
#include "tyvor_status.h"
#define TYVOR_UI_PAGES 7
#define TYVOR_UI_ROWS 6
typedef struct {
    const char *title;
    char banner[40];
    char rows[TYVOR_UI_ROWS][48];
} tyvor_ui_view_t;
/* Display-independent view model. All configuration pages are fixtures, no writes.
   The caller polls snapshot freshness before rendering. */
int tyvor_ui_build(unsigned page, const tyvor_snapshot_t *snapshot, tyvor_ui_view_t *view);
#endif
