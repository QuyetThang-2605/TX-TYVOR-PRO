"""Compile actual platform TX helpers with register/FIFO fakes, never hardware."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/'build/phase2c/evidence'
def compile_run(name,code,config):
    path=OUT/(name+'-'+config+'.c');path.write_text(code,encoding='utf-8');exe=path.with_suffix('.exe')
    cmd=[str(ROOT/'.tools/llvm-mingw-20250613-ucrt-x86_64/bin/clang.exe'),'-std=c11','-Wall','-Wextra','-Werror',str(path),'-I'+str(ROOT/'shared'),'-o',str(exe)]
    if config=='Release':cmd+=['-O2','-DNDEBUG']
    cmd+=[str(ROOT/'shared/tyvor_protocol.c'),str(ROOT/'shared/tyvor_async_tx.c')]
    subprocess.run(cmd,check=True);run=subprocess.run([str(exe)],capture_output=True,text=True)
    (OUT/(name+'-'+config+'.txt')).write_text(run.stdout+run.stderr)
    print(name,config,run.returncode,run.stdout.strip())
    if run.returncode:raise ValueError('Platform fake assertion failed')
    return dict(name=name,config=config,exit=run.returncode,hardware_tested=False,command=cmd)
COMMON='''#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "tyvor_async_tx.h"
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\\n",__LINE__,#x);return 1;}}while(0)
'''
def esp():
    s=(ROOT/'esp32/main/main.c').read_text();helpers=s[s.index('static int tx_start('):s.index('static uint64_t fresh_boot_id(')]
    return COMMON+'''#include <stdbool.h>
#define BRIDGE_UART 1
static bool uart_ready=true;
static tyvor_async_tx_t tx_queue;
static const uint8_t *tx_bytes;
static uint16_t tx_count,tx_offset;
static uint64_t now;
static uint8_t fifo[1024];static unsigned used;static int limit=7;
static uint64_t millis(void){return now;}
static int uart_tx_chars(int port,const char *b,unsigned n){
 (void)port;if(limit<0)return -1;if(n>(unsigned)limit)n=(unsigned)limit;
 memcpy(fifo+used,b,n);used+=n;return (int)n;}
'''+helpers+'''
int main(void){
 uint8_t b[64],payload[32]={0};tyvor_session_tag_t tag={1,2,1,1};
 size_t n=tyvor_encode(TYVOR_DIAGNOSTIC,1,&tag,payload,32,b,64);
 tyvor_async_tx_init(&tx_queue,tx_start,tx_state,tx_abort,NULL);
 CHECK(write_uart(NULL,b,(uint16_t)n)==64);CHECK(!used);
 for(now=0;now<12;++now)tyvor_async_tx_poll(&tx_queue,now);
 CHECK(used==64 && !memcmp(fifo,b,64) && tx_queue.completed==1);
 limit=0;CHECK(write_uart(NULL,b,64)==64);
 for(;now<40;++now)tyvor_async_tx_poll(&tx_queue,now);
 CHECK(tx_queue.timeouts==1 && !tx_queue.busy);
 limit=-1;CHECK(write_uart(NULL,b,64)==64);tyvor_async_tx_poll(&tx_queue,now++);tyvor_async_tx_poll(&tx_queue,now++);
 CHECK(tx_queue.errors==1 && !tx_queue.busy);
 limit=64;CHECK(write_uart(NULL,b,64)==64);tyvor_async_tx_poll(&tx_queue,now++);tyvor_async_tx_poll(&tx_queue,now++);
 CHECK(tx_queue.completed==2 && used==128 && !memcmp(fifo+64,b,64));
 puts("PASS actual ESP helpers: partial FIFO retains suffix; stall/error bounded; recovery");return 0;
}
'''
def stm():
    s=(ROOT/'stm32/BSP/board.c').read_text();helpers=s[s.index('static const uint8_t *tx_bytes;'):s.index('\n#endif\n#if TYVOR_ENABLE_IWDG')]
    irq=s[s.index('void USART2_IRQHandler('):s.index('\n#endif\nvoid board_watchdog_checkpoint')]
    return COMMON+'''
enum {USART_SR_RXNE=1,USART_SR_ORE=2,USART_SR_FE=4,USART_SR_NE=8,USART_SR_PE=16,USART_SR_TXE=32,USART_SR_TC=64};
enum {USART_CR1_TXEIE=32,USART_CR1_TCIE=64,UART_IT_TXE=32,UART_IT_TC=64,UART_FLAG_TC=64};
static struct {uint32_t SR,DR,CR1;} peripheral;
#define USART2 (&peripheral)
static struct {unsigned uart_rx_errors,uart_overflows;} board_diagnostics;
static uint8_t rx_ring[256];static volatile uint16_t rx_head,rx_tail;static volatile int rx_loss;
static uint32_t mask;
static uint32_t __get_PRIMASK(void){return mask;}
static void __disable_irq(void){mask=1;}
static void __set_PRIMASK(uint32_t m){mask=m;}
#define __DMB() ((void)0)
#define __HAL_UART_CLEAR_FLAG(h,f) (peripheral.SR&=~(f))
#define __HAL_UART_ENABLE_IT(h,f) (peripheral.CR1|=(f))
#define __HAL_UART_DISABLE_IT(h,f) (peripheral.CR1&=~(f))
'''+helpers+irq+'''
int main(void){
 uint8_t b[3]={11,22,33};mask=1;CHECK(!tx_start(NULL,b,3));CHECK(mask==1 && tx_busy);
 CHECK(tx_start(NULL,b,3)<0 && mask==1);mask=0;
 peripheral.SR=USART_SR_TXE;USART2_IRQHandler();CHECK(peripheral.DR==11 && tx_index==1 && tx_busy);
 peripheral.SR=USART_SR_TXE|USART_SR_ORE;USART2_IRQHandler();CHECK(board_diagnostics.uart_rx_errors==1 && tx_index==2 && peripheral.DR==22);
 peripheral.SR=USART_SR_TXE;USART2_IRQHandler();CHECK(peripheral.DR==33 && tx_busy && !(peripheral.CR1&USART_CR1_TXEIE) && (peripheral.CR1&USART_CR1_TCIE));
 peripheral.SR=USART_SR_TC;USART2_IRQHandler();CHECK(tx_state(NULL)==1 && !(peripheral.CR1&USART_CR1_TCIE));
 CHECK(!tx_start(NULL,b,3));mask=1;tx_abort(NULL);CHECK(mask==1 && !tx_busy && !tx_bytes && !peripheral.CR1);
 puts("PASS actual STM helpers/ISR: RX error keeps TX progress; TC completion; abort; PRIMASK preserved");return 0;
}
'''
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    rows=[compile_run(name,code(),config) for config in ('Debug','Release') for name,code in (('esp-fifo',esp),('stm-isr',stm))]
    sources={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in
             ('stm32/BSP/board.c','esp32/main/main.c','shared/tyvor_async_tx.c','scripts/test_phase2c_platform.py')}
    (OUT/'platform-helper-tests.json').write_text(json.dumps(dict(tests=rows,sources=sources),indent=2))
if __name__=='__main__':main()
