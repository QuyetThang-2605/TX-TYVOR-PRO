#include "board.h"
#include "stm32f4xx_hal.h"
#include "tyvor_protocol.h"
#include "tyvor_async_tx.h"
board_diagnostics_t board_diagnostics;
tyvor_timing_t board_timing;
static uint64_t boot_identity;
/* newlib constructor hooks: ST startup calls __libc_init_array; no CRT startup. */
void _init(void) {}
void _fini(void) {}
#if TYVOR_UART_ENABLED
static UART_HandleTypeDef bridge_uart;
static uint8_t rx_ring[256];
static volatile uint16_t rx_head, rx_tail;
static volatile int rx_loss;
static tyvor_async_tx_t tx_queue; /* main owner only, not accessed by ISR */
static const uint8_t *tx_bytes;
static volatile uint16_t tx_index,tx_length;
static volatile int tx_busy;
static int tx_start(void *ctx,const uint8_t *bytes,uint16_t n) {
    (void)ctx;
    uint32_t mask=__get_PRIMASK();__disable_irq();
    if(tx_busy) {__set_PRIMASK(mask);return -1;}
    tx_bytes=bytes;tx_index=0;tx_length=n;tx_busy=1;
    __HAL_UART_CLEAR_FLAG(&bridge_uart,UART_FLAG_TC);__DMB();
    __HAL_UART_ENABLE_IT(&bridge_uart,UART_IT_TXE);
    __set_PRIMASK(mask);return 0;
}
static int tx_state(void *ctx) {(void)ctx;return tx_busy?0:1;}
static void tx_abort(void *ctx) {
    (void)ctx;uint32_t mask=__get_PRIMASK();__disable_irq();
    __HAL_UART_DISABLE_IT(&bridge_uart,UART_IT_TXE);__HAL_UART_DISABLE_IT(&bridge_uart,UART_IT_TC);
    tx_busy=0;tx_bytes=NULL;__DMB();__set_PRIMASK(mask);
    /* A byte already in the shift register may finish. Parser handles truncation. */
}
#endif
#if TYVOR_ENABLE_IWDG
static IWDG_HandleTypeDef watchdog;
#endif
int board_init(void) {
    board_diagnostics.reset_flags=RCC->CSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    if (HAL_Init() != HAL_OK) return -1;
    /* Timebase must preempt UART priority 5. Core remains main, no RTOS API in ISR. */
    HAL_NVIC_SetPriority(SysTick_IRQn,0,0);
    /* SYSCLK remains HSI 16 MHz. HSI PLL provides 48 MHz only for hardware RNG.
       No external oscillator, GPIO or board crystal frequency is assumed. */
    RCC_OscInitTypeDef osc={0};
    osc.OscillatorType=RCC_OSCILLATORTYPE_HSI;
    osc.HSIState=RCC_HSI_ON; osc.HSICalibrationValue=RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState=RCC_PLL_ON; osc.PLL.PLLSource=RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM=16; osc.PLL.PLLN=192; osc.PLL.PLLP=RCC_PLLP_DIV2; osc.PLL.PLLQ=4;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) return -1;
    RCC_ClkInitTypeDef clock={0};
    clock.ClockType=RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    clock.SYSCLKSource=RCC_SYSCLKSOURCE_HSI; clock.AHBCLKDivider=RCC_SYSCLK_DIV1;
    clock.APB1CLKDivider=RCC_HCLK_DIV1; clock.APB2CLKDivider=RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_0) != HAL_OK) return -1;
    SystemCoreClockUpdate(); board_diagnostics.clock_hz=SystemCoreClock;
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT=0;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
    uint32_t counter_before=DWT->CYCCNT;__NOP();__NOP();__NOP();
    tyvor_timing_init(&board_timing,SystemCoreClock,DWT->CYCCNT!=counter_before,1);
    __HAL_RCC_RNG_CLK_ENABLE();
    RNG_HandleTypeDef entropy={0}; entropy.Instance=RNG;
    if (HAL_RNG_Init(&entropy)!=HAL_OK) return -1;
    for (unsigned attempt=0;attempt<4 && !boot_identity;++attempt) {
        uint32_t high,low;
        if (HAL_RNG_GenerateRandomNumber(&entropy,&high)!=HAL_OK ||
            HAL_RNG_GenerateRandomNumber(&entropy,&low)!=HAL_OK) return -1;
        boot_identity=((uint64_t)high<<32)|low;
    }
    if (HAL_RNG_DeInit(&entropy)!=HAL_OK || !boot_identity) return -1;
    __HAL_RCC_RNG_CLK_DISABLE();
#if TYVOR_UART_ENABLED
    /* Conditional candidate mapping, NOT a claim about the Mini Type-C board.
       Build flag must remain OFF until PA2/PA3 routing is independently verified. */
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_USART2_CLK_ENABLE();
    GPIO_InitTypeDef pins={0};
    pins.Pin=GPIO_PIN_2|GPIO_PIN_3; pins.Mode=GPIO_MODE_AF_PP;
    pins.Pull=GPIO_PULLUP; pins.Speed=GPIO_SPEED_FREQ_LOW; pins.Alternate=GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA,&pins);
    bridge_uart.Instance=USART2; bridge_uart.Init.BaudRate=115200;
    bridge_uart.Init.WordLength=UART_WORDLENGTH_8B; bridge_uart.Init.StopBits=UART_STOPBITS_1;
    bridge_uart.Init.Parity=UART_PARITY_NONE; bridge_uart.Init.Mode=UART_MODE_TX_RX;
    bridge_uart.Init.HwFlowCtl=UART_HWCONTROL_NONE; bridge_uart.Init.OverSampling=UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&bridge_uart) != HAL_OK) return -1;
    tyvor_async_tx_init(&tx_queue,tx_start,tx_state,tx_abort,NULL);
    HAL_NVIC_SetPriority(USART2_IRQn,5,0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    __HAL_UART_ENABLE_IT(&bridge_uart,UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&bridge_uart,UART_IT_ERR);
#endif
#if TYVOR_ENABLE_IWDG
    watchdog.Instance=IWDG; watchdog.Init.Prescaler=IWDG_PRESCALER_64;
    watchdog.Init.Reload=999; /* nominal ~2 s at LSI 32 kHz, not measured. */
    if (HAL_IWDG_Init(&watchdog) != HAL_OK) return -1;
#endif
    return 0;
}
void SysTick_Handler(void) { HAL_IncTick(); }
uint64_t board_millis(void) {
    static uint32_t previous; static uint64_t high;
    uint32_t now=HAL_GetTick();
    if (now < previous) high+=UINT64_C(1)<<32;
    previous=now; return high+now;
}
int board_uart_enabled(void) { return TYVOR_UART_ENABLED; }
uint64_t board_boot_identity(void) { return boot_identity; }
uint32_t board_cycles(void) {return DWT->CYCCNT;}
void board_uart_service(uint64_t now) {
#if TYVOR_UART_ENABLED
    tyvor_async_tx_poll(&tx_queue,now);
    board_diagnostics.tx_full=tx_queue.full;board_diagnostics.tx_coalesced=tx_queue.coalesced;
    board_diagnostics.tx_expired=tx_queue.expired;board_diagnostics.tx_timeouts=tx_queue.timeouts;
    board_diagnostics.tx_completed=tx_queue.completed;board_diagnostics.tx_high_water=tx_queue.high_water;
    board_diagnostics.uart_tx_errors=tx_queue.errors;
#else
    (void)now;
#endif
}
uint32_t board_stack_free(void) {
    extern uint32_t _estack;
    uintptr_t top=(uintptr_t)&_estack,sp=(uintptr_t)__get_MSP();
    /* Current main-stack sample within the 4 KiB allowance, not a high-water
       mark and not a measurement of interrupt/runtime peak usage. */
    return sp<=top && sp>=top-4096?(uint32_t)(sp-(top-4096)):0;
}
int board_uart_write(void *context, const uint8_t *bytes, uint16_t n) {
    (void)context;
    if (!bytes || !n || n > TYVOR_FRAME_MAX) return -1;
#if TYVOR_UART_ENABLED
    return tyvor_async_tx_write(&tx_queue,bytes,n,board_millis());
#else
    ++board_diagnostics.tx_suppressed; return -1;
#endif
}
size_t board_uart_read(uint8_t *bytes, size_t cap, int *transport_error) {
    if (!bytes || !transport_error) return 0;
    *transport_error=0;
#if TYVOR_UART_ENABLED
    uint32_t primask=__get_PRIMASK();
    __disable_irq();
    size_t used=0;
    if (rx_loss) {
        *transport_error=rx_loss; rx_loss=0; rx_tail=rx_head;
    } else while (used < cap && used < 64 && rx_tail != rx_head) {
        bytes[used++]=rx_ring[rx_tail]; rx_tail=(uint16_t)((rx_tail+1u)&255u);
    }
    __set_PRIMASK(primask);
    return used;
#else
    (void)cap; return 0;
#endif
}
#if TYVOR_UART_ENABLED
void USART2_IRQHandler(void) {
    uint32_t status=USART2->SR;
    if (status & (USART_SR_RXNE|USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) {
        uint8_t byte=(uint8_t)USART2->DR; /* SR then DR clears RX/error conditions. */
        if (status & (USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) {
            if (!rx_loss) rx_loss=1;
            ++board_diagnostics.uart_rx_errors;
        } else {
            uint16_t next=(uint16_t)((rx_head+1u)&255u);
            if (next == rx_tail) { rx_loss=2; ++board_diagnostics.uart_overflows; }
            else {rx_ring[rx_head]=byte; __DMB(); rx_head=next;}
        }
    }
    if((USART2->SR&USART_SR_TXE) && (USART2->CR1&USART_CR1_TXEIE)) {
        if(tx_busy && tx_index<tx_length)USART2->DR=tx_bytes[tx_index++];
        if(tx_index>=tx_length) {
            __HAL_UART_DISABLE_IT(&bridge_uart,UART_IT_TXE);__HAL_UART_ENABLE_IT(&bridge_uart,UART_IT_TC);
        }
    }
    if((USART2->SR&USART_SR_TC) && (USART2->CR1&USART_CR1_TCIE)) {
        __HAL_UART_DISABLE_IT(&bridge_uart,UART_IT_TC);__DMB();tx_busy=0;
    }
}
#endif
void board_watchdog_checkpoint(uint64_t started, uint64_t completed) {
    static uint64_t last_refresh;
    ++board_diagnostics.loop_count;
    /* Feed only after the owner loop completed and the tick advanced.
       ISR, UART errors and UI requests never refresh the watchdog. */
    if (!board_diagnostics.fault_code && completed >= started && completed-started < 100 && completed > last_refresh) {
#if TYVOR_ENABLE_IWDG
        if (HAL_IWDG_Refresh(&watchdog) != HAL_OK) board_fault(4);
        ++board_diagnostics.watchdog_refreshes;
#endif
        last_refresh=completed;
    }
}
void board_idle(void) { __WFI(); }
void board_fault(uint32_t code) {
    board_diagnostics.fault_code=code;
    __disable_irq();
    for (;;) { __NOP(); } /* IWDG resets if already started; otherwise debugger halt loop. */
}
void HardFault_Handler(void) { board_fault(10); }
void MemManage_Handler(void) { board_fault(11); }
void BusFault_Handler(void) { board_fault(12); }
void UsageFault_Handler(void) { board_fault(13); }
void NMI_Handler(void) { board_fault(14); }
