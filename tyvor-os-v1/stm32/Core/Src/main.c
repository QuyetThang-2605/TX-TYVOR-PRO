#include "board.h"
#include "tyvor_core_bridge.h"
#include "tyvor_bench.h"
#include "tyvor_diagnostics.h"
static tyvor_core_t core;
static tyvor_core_bridge_t bridge;
static tyvor_mock_storage_t mock_storage;
static tyvor_bench_transport_t uart_transport;
tyvor_diagnostics_t tyvor_diagnostics; /* read-only debugger/bench observation */
static int receive_core(void *ctx,const uint8_t *b,size_t n,uint64_t now) {return tyvor_core_bridge_feed(ctx,b,n,now);}
int main(void) {
    if(board_init())board_fault(1);
    if(tyvor_bench_seed_storage(&mock_storage))board_fault(2);
    tyvor_model_t model;tyvor_model_default(&model,1);
    uint64_t now=board_millis();
    if(tyvor_core_init(&core,&model,tyvor_mock_storage_adapter(&mock_storage),now))board_fault(3);
    tyvor_bench_transport_init(&uart_transport,board_uart_write,NULL,board_uart_enabled());
    tyvor_diagnostics_init(&tyvor_diagnostics,TYVOR_ROLE_CONTROL,board_diagnostics.reset_flags,0,1);
    tyvor_diagnostics.clock_hz=board_diagnostics.clock_hz;
    tyvor_diagnostics.watchdog_configured=TYVOR_ENABLE_IWDG;
    tyvor_diagnostics.watchdog_subscribed=TYVOR_ENABLE_IWDG;
    tyvor_diagnostics.watchdog_status_valid=1;
    tyvor_diagnostics.stack_reserve=4096;tyvor_diagnostics.status_is_tx=1;
    if(tyvor_core_bridge_init(&bridge,&core,board_boot_identity(),tyvor_bench_transport_write,&uart_transport,now))board_fault(4);
    uint64_t next_input=now,next_telemetry=now,next_diagnostic=now;uint32_t input_sequence=0;
    for(;;) {
        uint64_t started=board_millis();
        uint32_t loop_cycles=board_cycles(),input_cycles=loop_cycles;
        /* Explicit bench source: never reads a board gimbal/switch or enables RF. */
        if(started>=next_input) {
            const int16_t axes[4]={0,0,-1000,0};const int8_t switches[2]={0,1};
            tyvor_input_sample_t sample=tyvor_bench_input(&core.models.active,axes,switches,++input_sequence,started);
            (void)tyvor_core_input(&core,&sample,started);next_input=started+10;
        }
        if(started>=next_telemetry) {
            tyvor_telemetry_sample_t sample=tyvor_bench_telemetry(started);
            (void)tyvor_core_telemetry(&core,&sample,started);next_telemetry=started+100;
        }
        tyvor_timing_record(&board_timing,TYVOR_TIME_INPUT,input_cycles,board_cycles());
        uint32_t core_cycles=board_cycles();
        if(started>=core.next_tick_ms)tyvor_timing_lateness(&board_timing,started,core.next_tick_ms);
        int tick=tyvor_core_poll(&core,started); /* core always runs before UART work */
        if(tick)tyvor_timing_record(&board_timing,TYVOR_TIME_CORE,core_cycles,board_cycles());
        uint32_t uart_cycles=board_cycles();board_uart_service(started);
        uint8_t bytes[64];int transport_error;
        size_t n=board_uart_read(bytes,sizeof(bytes),&transport_error);
        if(transport_error)tyvor_bench_transport_error(&uart_transport,&bridge.transport.link,transport_error==2);
        else if(n)(void)tyvor_bench_transport_receive(&uart_transport,bytes,n,started,receive_core,&bridge);
        tyvor_core_bridge_poll(&bridge,started);
        board_uart_service(board_millis());
        tyvor_timing_record(&board_timing,TYVOR_TIME_UART,uart_cycles,board_cycles());
#if TYVOR_DIAGNOSTICS
        if(started>=next_diagnostic) {
            tyvor_diagnostics_update(&tyvor_diagnostics,&bridge.transport.link,&uart_transport,started);
            tyvor_diagnostics.stack_free=board_stack_free();tyvor_diagnostics.stack_valid=1;
            tyvor_diagnostics.watchdog_checkpoints=board_diagnostics.watchdog_refreshes;
            tyvor_diagnostics.status_age_valid=bridge.status_sent && started>=bridge.last_status_success_ms;
            tyvor_diagnostics.status_age_ms=tyvor_diagnostics.status_age_valid?started-bridge.last_status_success_ms:0;
            next_diagnostic=started+250;
        }
#endif
        board_watchdog_checkpoint(started,board_millis());
        tyvor_timing_record(&board_timing,TYVOR_TIME_LOOP,loop_cycles,board_cycles());board_idle();
    }
}
