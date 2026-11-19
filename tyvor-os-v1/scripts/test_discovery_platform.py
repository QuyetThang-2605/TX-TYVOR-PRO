"""Actual LED/button helper bodies with fake registers; compile guards. No hardware."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/'build/discovery/evidence'

def main():
    OUT.mkdir(parents=True,exist_ok=True);rows=[];logs=[]
    board=ROOT/'stm32/boards/stm32f4_discovery'
    source=(board/'discovery_board.c').read_text()
    helpers=source[source.index('void discovery_board_leds('):source.index('int discovery_board_init(')]
    code='''#include <stdint.h>
#include <stdio.h>
#include "discovery_demo.h"
static struct {uint32_t BSRR;} fake_d;
#define GPIOD (&fake_d)
#define GPIOA 0
#define GPIO_PIN_0 1u
#define GPIO_PIN_SET 1
static unsigned raw;
static int HAL_GPIO_ReadPin(int port,unsigned pin){(void)port;(void)pin;return (raw&1u)?1:0;}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\\n",__LINE__,#x);return 1;}}while(0)
'''+helpers+'''
int main(void){
 for(unsigned mask=0;mask<65536;++mask){
  discovery_board_leds((uint16_t)mask);
  CHECK((fake_d.BSRR&0xffffu)==(mask&0xf000u));
  CHECK((fake_d.BSRR>>16)==((~mask)&0xf000u));
 }
 raw=0;CHECK(!discovery_board_button());raw=1;CHECK(discovery_board_button());
 raw=0xfffe;CHECK(!discovery_board_button());raw=0xffff;CHECK(discovery_board_button());
 puts("PASS actual LED/button helpers: all 65536 masks, active-high PA0; register fake only");return 0;
}
'''
    native=ROOT/'.tools/llvm-mingw-20250613-ucrt-x86_64/bin/clang.exe'
    for config,flags in [('Debug',[]),('Release',['-O2','-DNDEBUG'])]:
        path=OUT/('discovery-gpio-'+config+'.c');path.write_text(code);exe=path.with_suffix('.exe')
        cmd=[str(native),'-std=c11','-Wall','-Wextra','-Werror',str(path),'-I'+str(board),'-I'+str(ROOT/'stm32/engine'),'-I'+str(ROOT/'shared'),'-DTYVOR_BENCH_ONLY=1',*flags,'-o',str(exe)]
        subprocess.run(cmd,check=True);run=subprocess.run([str(exe)],capture_output=True,text=True)
        (OUT/('discovery-gpio-'+config+'.txt')).write_text(run.stdout+run.stderr)
        if run.returncode:raise ValueError('GPIO fake failed')
        rows.append(dict(case='actual-gpio-helpers',config=config,passed=True,hardware_tested=False,command=cmd))
    for target,compiler,flags in [('native',native,[]),('arm',ROOT/'.tools/bin/arm-none-eabi-gcc.exe',['-mcpu=cortex-m4','-mthumb'])]:
        for name,defs,success in [
            ('safe',['-DTYVOR_BENCH_ONLY=1','-DTYVOR_DISCOVERY_MB997E=1'],True),
            ('uart-off',['-DTYVOR_BENCH_ONLY=1','-DTYVOR_DISCOVERY_MB997E=1','-DTYVOR_DISCOVERY_UART_ENABLED=0'],True),
            ('uart-on',['-DTYVOR_BENCH_ONLY=1','-DTYVOR_DISCOVERY_MB997E=1','-DTYVOR_DISCOVERY_UART_ENABLED=1'],False),
            ('rf-on',['-DTYVOR_BENCH_ONLY=1','-DTYVOR_DISCOVERY_MB997E=1','-DTYVOR_ENABLE_RF=1'],False),
            ('no-bench',['-DTYVOR_DISCOVERY_MB997E=1'],False),
            ('no-board',['-DTYVOR_BENCH_ONLY=1'],False),
            ('wrong-board',['-DTYVOR_BENCH_ONLY=1','-DTYVOR_DISCOVERY_MB997E=0'],False)]:
            cmd=[str(compiler),'-std=c11','-x','c','-c','-','-I'+str(board),'-I'+str(ROOT/'stm32/engine'),'-I'+str(ROOT/'shared'),*flags,*defs,'-o',str(OUT/f'discovery-guard-{target}-{name}.o')]
            run=subprocess.run(cmd,input='#include "discovery_board.h"\n',text=True,capture_output=True)
            passed=(run.returncode==0)==success
            rows.append(dict(case=name,target=target,expected_compile_success=success,actual_exit=run.returncode,passed=passed,command=cmd))
            logs.append(target+'/'+name+'\n'+run.stdout+run.stderr)
            if not passed:raise ValueError('Unexpected compile guard result: '+name)
    (OUT/'discovery-platform-checks.txt').write_text('\n'.join(logs))
    result=dict(passed=len(rows),failed=0,hardware_tested=False,checks=rows,source_sha256=hashlib.sha256((board/'discovery_board.c').read_bytes()).hexdigest())
    (OUT/'discovery-platform-checks.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: 2 actual GPIO-helper fake runs + 14 native/ARM compile guards; no hardware test')
if __name__=='__main__':main()
