"""Native/ARM compile guards, offline and without linking any GPIO firmware."""
import json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
def main():
    evidence=ROOT/'build/phase2c/evidence';evidence.mkdir(parents=True,exist_ok=True);rows=[];logs=[]
    compilers=(('native',ROOT/'.tools/llvm-mingw-20250613-ucrt-x86_64/bin/clang.exe',[]),
               ('stm32',ROOT/'.tools/bin/arm-none-eabi-gcc.exe',['-mcpu=cortex-m4','-mthumb']))
    for target,compiler,flags in compilers:
        for name,defs,success in (
            ('bench',['-DTYVOR_BENCH_ONLY=1'],True),
            ('rf_forbidden',['-DTYVOR_BENCH_ONLY=1','-DTYVOR_ENABLE_RF=1'],False),
            ('bench_required',[],False),
            ('diagnostics_off_still_bench',['-DTYVOR_BENCH_ONLY=1','-DTYVOR_DIAGNOSTICS=0'],True)):
            cmd=[str(compiler),'-std=c11','-x','c','-c','-','-I'+str(ROOT/'stm32/engine'),'-I'+str(ROOT/'shared'),*flags,*defs,
                 '-o',str(evidence/f'guard-{target}-{name}.o')]
            run=subprocess.run(cmd,input='#include "tyvor_core.h"\n',text=True,capture_output=True)
            passed=(run.returncode==0)==success
            rows.append(dict(target=target,case=name,expected_compile_success=success,actual_exit=run.returncode,pass_check=passed,command=cmd))
            logs.append(target+'/'+name+'\n'+run.stdout+run.stderr)
            if not passed:raise ValueError('Unexpected compile guard result')
    (evidence/'bench-build-guards.json').write_text(json.dumps(rows,indent=2))
    (evidence/'bench-build-guards.txt').write_text('\n'.join(logs))
    print('PASS: 8 compile guards; expected RF/missing-BENCH rejection retained')
if __name__=='__main__':main()
