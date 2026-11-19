"""Compile the actual BSP write function against a fake slow HAL. No device I/O."""
import argparse,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
def main():
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=ROOT);p.add_argument('--config',choices=('Debug','Release'),required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=True)
    source=(a.source/'stm32/BSP/board.c').read_text(encoding='utf-8')
    start=source.index('int board_uart_write(');end=source.index('\nsize_t board_uart_read(',start)
    function=source[start:end]
    queued='tyvor_async_tx_write' in function
    wrapper='''#define TYVOR_UART_ENABLED 1
#include "board.h"
#include "tyvor_protocol.h"
unsigned probe_elapsed,probe_hal_calls;
board_diagnostics_t board_diagnostics;
'''
    if queued:
        wrapper+='''#include "tyvor_async_tx.h"
static tyvor_async_tx_t tx_queue;
static int start(void *c,const uint8_t *b,uint16_t n){(void)c;(void)b;(void)n;return 0;}
static int state(void *c){(void)c;return 0;}
static void stop(void *c){(void)c;}
uint64_t board_millis(void){return 0;}
void probe_init(void){tyvor_async_tx_init(&tx_queue,start,state,stop,NULL);}
'''
    else:
        wrapper+='''typedef int UART_HandleTypeDef;
static UART_HandleTypeDef bridge_uart;
#define HAL_OK 0
static int HAL_UART_Transmit(UART_HandleTypeDef *h,uint8_t *b,uint16_t n,unsigned timeout){
 (void)h;(void)b;(void)n;++probe_hal_calls;probe_elapsed+=timeout;return HAL_OK;}
void probe_init(void){}
'''
    wrapper+=function
    target=a.output/(a.config.lower()+'-bsp-probe.c');target.write_text(wrapper,encoding='utf-8')
    exe=a.output/(a.config.lower()+'-bsp-probe.exe')
    cmd=[str(ROOT/'.tools/llvm-mingw-20250613-ucrt-x86_64/bin/clang.exe'),'-std=c11','-Wall','-Wextra','-Werror',str(target),str(ROOT/'sim/test_timing_regression.c'),str(a.source/'shared/tyvor_protocol.c'),'-I'+str(a.source/'stm32/BSP'),'-I'+str(a.source/'shared'),'-o',str(exe)]
    if a.config=='Release':cmd+=['-O2','-DNDEBUG']
    if queued:cmd+=[str(a.source/'shared/tyvor_async_tx.c')]
    subprocess.run(cmd,check=True)
    run=subprocess.run([str(exe)],capture_output=True,text=True)
    stem=a.config+'-blocking-'+('after' if queued else 'before')
    (a.output/(stem+'.txt')).write_text(run.stdout+run.stderr,encoding='utf-8')
    print(run.stdout,end='');print('exit',run.returncode)
    (a.output/(stem+'.json')).write_text(json.dumps(dict(exit=run.returncode,queued=queued,command=cmd),indent=2))
    return run.returncode
if __name__=='__main__':sys.exit(main())
