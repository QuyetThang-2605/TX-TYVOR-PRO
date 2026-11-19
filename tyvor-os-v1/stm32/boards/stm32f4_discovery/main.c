#include "discovery_board.h"
discovery_demo_t tyvor_discovery_demo; /* debugger-visible bench state, no physical outputs */
int main(void) {
    int result=discovery_board_init();if(result)discovery_board_fault((uint32_t)(100-result));
    if(discovery_demo_init(&tyvor_discovery_demo,discovery_board_millis(),discovery_board_button()))discovery_board_fault(2);
    for(;;) {
        uint64_t now=discovery_board_millis();uint32_t cycles=discovery_board_cycles();
        uint64_t deadline=tyvor_discovery_demo.core.next_tick_ms;
        if(now>=deadline)tyvor_timing_lateness(&discovery_timing,now,deadline);
        int healthy=discovery_demo_poll(&tyvor_discovery_demo,now,discovery_board_button())>=0;
        tyvor_timing_record(&discovery_timing,TYVOR_TIME_CORE,cycles,discovery_board_cycles());
        discovery_board_leds(tyvor_discovery_demo.leds);
        discovery_board_checkpoint(now,discovery_board_millis(),healthy);
        tyvor_timing_record(&discovery_timing,TYVOR_TIME_LOOP,cycles,discovery_board_cycles());
        discovery_board_idle();
    }
}
