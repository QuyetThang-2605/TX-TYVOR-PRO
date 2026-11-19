#include "board.h"
#include "tyvor_protocol.h"
#include <stdio.h>
extern unsigned probe_elapsed,probe_hal_calls;
void probe_init(void);
int main(void) {
    probe_init();
    uint8_t bytes[64],payload[2]={TYVOR_ROLE_CONTROL,0};
    tyvor_session_tag_t tag={1,0,1,0};
    size_t n=tyvor_encode(TYVOR_HELLO,0,&tag,payload,2,bytes,sizeof(bytes));
    int result=board_uart_write(NULL,bytes,(uint16_t)n);
    printf("actual BSP write: accepted=%d simulated_wait_ms=%u HAL_blocking_calls=%u\n",result,probe_elapsed,probe_hal_calls);
    /* A fake HAL consumes its actual timeout argument. No hardware timing claim. */
    if(result!=(int)n || probe_elapsed || probe_hal_calls) {puts("FAIL: TX callback waits on peripheral");return 1;}
    puts("PASS: owned copy without peripheral wait");return 0;
}
