#include "ui_view.h"
int tyvor_ui32_project(const tyvor_ui_state_t *s, uint64_t now, int sim,
                       uint64_t timer, tyvor_ui32_view_t *v) {
  return tyvor_ui2_project(s, now, sim, timer, v);
}
void tyvor_ui32_fixture(tyvor_ui_state_t *s, uint64_t now, unsigned scenario) {
  tyvor_ui2_fixture(s, now, scenario);
}
