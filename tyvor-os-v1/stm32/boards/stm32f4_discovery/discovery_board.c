#include "discovery_board.h"
#include "stm32f4xx_hal.h"
discovery_board_diagnostics_t discovery_board_diagnostics;
tyvor_timing_t discovery_timing;
__attribute__((used,section(".tyvor_identity")))
const char tyvor_discovery_firmware_identity[]="TYVOR DISCOVERY BENCH V1|MB997E|STM32F407VG|UART_OFF|RF_OFF";
static IWDG_HandleTypeDef watchdog;
void _init(void) {}
void _fini(void) {}
_Static_assert(DISCOVERY_LED_MASK==(GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15),"Discovery LED map");
void discovery_board_leds(uint16_t mask) {
    uint32_t on=mask&DISCOVERY_LED_MASK;
    GPIOD->BSRR=((DISCOVERY_LED_MASK&~on)<<16)|on;
}
int discovery_board_button(void) {return HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_0)==GPIO_PIN_SET;}
int discovery_board_init(void) {
    discovery_board_diagnostics.reset_flags=RCC->CSR;__HAL_RCC_CLEAR_RESET_FLAGS();
    if(HAL_Init()!=HAL_OK)return -1;
    __HAL_RCC_GPIOA_CLK_ENABLE();__HAL_RCC_GPIOD_CLK_ENABLE();
    discovery_board_leds(0);
    GPIO_InitTypeDef gpio={0};gpio.Pin=DISCOVERY_LED_MASK;gpio.Mode=GPIO_MODE_OUTPUT_PP;
    gpio.Pull=GPIO_NOPULL;gpio.Speed=GPIO_SPEED_FREQ_LOW;HAL_GPIO_Init(GPIOD,&gpio);
    gpio.Pin=GPIO_PIN_0;gpio.Mode=GPIO_MODE_INPUT;gpio.Pull=GPIO_NOPULL;HAL_GPIO_Init(GPIOA,&gpio);
    HAL_NVIC_SetPriority(SysTick_IRQn,0,0);
    __HAL_RCC_PWR_CLK_ENABLE();__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    /* MB997-E01: X2 not fitted; 8 MHz ST-LINK MCO -> PH0 through R68.
       Use external clock BYPASS, not crystal mode or Mini Type-C assumptions.
       Missing MCO/changed solder bridges is a boot fault, never silent fallback. */
    RCC_OscInitTypeDef osc={0};osc.OscillatorType=RCC_OSCILLATORTYPE_HSE;
    osc.HSEState=RCC_HSE_BYPASS;osc.PLL.PLLState=RCC_PLL_ON;osc.PLL.PLLSource=RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM=8;osc.PLL.PLLN=336;osc.PLL.PLLP=RCC_PLLP_DIV2;osc.PLL.PLLQ=7;
    if(HAL_RCC_OscConfig(&osc)!=HAL_OK)return -2;
    RCC_ClkInitTypeDef clock={0};clock.ClockType=RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    clock.SYSCLKSource=RCC_SYSCLKSOURCE_PLLCLK;clock.AHBCLKDivider=RCC_SYSCLK_DIV1;
    clock.APB1CLKDivider=RCC_HCLK_DIV4;clock.APB2CLKDivider=RCC_HCLK_DIV2;
    if(HAL_RCC_ClockConfig(&clock,FLASH_LATENCY_5)!=HAL_OK)return -3;
    SystemCoreClockUpdate();discovery_board_diagnostics.clock_hz=SystemCoreClock;discovery_board_diagnostics.hse_bypass=1;
    if(SystemCoreClock!=DISCOVERY_SYSCLK_HZ)return -4;
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;DWT->CYCCNT=0;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
    uint32_t before=DWT->CYCCNT;__NOP();__NOP();__NOP();
    tyvor_timing_init(&discovery_timing,SystemCoreClock,DWT->CYCCNT!=before,1);
    watchdog.Instance=IWDG;watchdog.Init.Prescaler=IWDG_PRESCALER_64;watchdog.Init.Reload=999;
    if(HAL_IWDG_Init(&watchdog)!=HAL_OK)return -5;
    discovery_board_diagnostics.watchdog_enabled=1;return 0;
}
void SysTick_Handler(void) {HAL_IncTick();}
uint64_t discovery_board_millis(void) {
    static uint32_t previous;static uint64_t high;uint32_t now=HAL_GetTick();
    if(now<previous)high+=UINT64_C(1)<<32;
    previous=now;return high+now;
}
uint32_t discovery_board_cycles(void) {return DWT->CYCCNT;}
void discovery_board_checkpoint(uint64_t started,uint64_t ended,int healthy) {
    static uint64_t last;extern uint32_t _estack;uintptr_t top=(uintptr_t)&_estack,sp=__get_MSP();
    discovery_board_diagnostics.stack_free_now=sp<=top && sp>=top-DISCOVERY_STACK_RESERVE?(uint32_t)(sp-(top-DISCOVERY_STACK_RESERVE)):0;
    ++discovery_board_diagnostics.loop_count;
    if(healthy && !discovery_board_diagnostics.fault_code && ended>=started && ended-started<100 && ended>last) {
        if(HAL_IWDG_Refresh(&watchdog)!=HAL_OK)discovery_board_fault(6);
        ++discovery_board_diagnostics.watchdog_refreshes;last=ended;
    }
}
void discovery_board_idle(void) {__WFI();}
void discovery_board_fault(uint32_t code) {
    discovery_board_diagnostics.fault_code=code;
    __HAL_RCC_GPIOD_CLK_ENABLE();
    /* Fault before GPIO init: configure only the red LED output, preserve SWD. */
    GPIOD->MODER=(GPIOD->MODER&~(3u<<(14*2)))|(1u<<(14*2));discovery_board_leds(DISCOVERY_RED);
    __disable_irq();for(;;)__NOP(); /* IWDG resets only if already started. */
}
void HardFault_Handler(void) {discovery_board_fault(10);}
void MemManage_Handler(void) {discovery_board_fault(11);}
void BusFault_Handler(void) {discovery_board_fault(12);}
void UsageFault_Handler(void) {discovery_board_fault(13);}
void NMI_Handler(void) {discovery_board_fault(14);}
