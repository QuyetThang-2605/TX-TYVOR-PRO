#ifndef TYVOR_UI_V31_VIEW_H
#define TYVOR_UI_V31_VIEW_H
#include "../tyvor_ui_v2/ui_view.h"
typedef tyvor_ui2_view_t tyvor_ui31_view_t;
int tyvor_ui31_project(const tyvor_ui_state_t *, uint64_t now, int simulator,
                       uint64_t timer_ms, tyvor_ui31_view_t *);
void tyvor_ui31_fixture(tyvor_ui_state_t *, uint64_t now, unsigned scenario);
#endif
