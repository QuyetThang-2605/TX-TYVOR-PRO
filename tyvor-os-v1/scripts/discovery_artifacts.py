"""Discovery build receipts / ELF inspection / offline package. Never device I/O."""
import argparse,difflib,hashlib,json,os,shutil,struct,subprocess,sys,time,uuid
from pathlib import Path
import xml.etree.ElementTree as ET
from build_provenance import inputs as base_inputs, inventory, group_inventory, compiler_record
from package_artifacts import source_paths
ROOT=Path(__file__).resolve().parent.parent
BUILD=ROOT/'build/discovery';EVIDENCE=BUILD/'evidence';OUTPUT=ROOT/'artifacts/stm32f4-discovery-bench-v1'
TAG=b'TYVOR DISCOVERY BENCH V1|MB997E|STM32F407VG|UART_OFF|RF_OFF'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def check(value,message):
    if not value:raise ValueError(message)
def inside(path):
    path=Path(path).resolve();check(path.is_relative_to(ROOT.resolve()) and path!=ROOT.resolve(),'Mutation outside workspace')
    return path
def label(target,config):return 'native-'+config.lower() if target=='native' else target
def inputs(target):
    paths={ROOT/x['path'] for x in base_inputs('native' if target=='native' else 'stm32')}
    paths.update(p for p in (ROOT/'stm32/boards/stm32f4_discovery').glob('*') if p.is_file())
    paths.add(ROOT/'scripts/build-discovery.ps1')
    return [dict(path=p.relative_to(ROOT).as_posix(),sha256=sha(p)) for p in sorted(paths)]
def native_check(config):
    build=BUILD/label('native',config);expected=inventory()+group_inventory('TYVOR_DISCOVERY_CASES')
    suite=ET.parse(EVIDENCE/(label('native',config)+'.xml')).getroot();cases=suite.findall('testcase')
    check(int(suite.get('tests','-1'))==len(expected) and len(cases)==len(expected) and {c.get('name') for c in cases}==set(expected),'Native exact inventory mismatch')
    check(all(int(suite.get(k,'0'))==0 for k in ['failures','errors','skipped','disabled']),'Failed/skipped native cases')
    check(not any(c.find(k) is not None for c in cases for k in ['failure','error','skipped']),'Failed/skipped native case')
    listing=json.loads(subprocess.run([str(ROOT/'.tools/cmake-3.30.9-windows-x86_64/bin/ctest.exe'),'--test-dir',str(build),'--show-only=json-v1'],capture_output=True,text=True,check=True).stdout)
    check({x['name'] for x in listing['tests']}==set(expected) and len(listing['tests'])==len(expected),'CTest registration mismatch')
    mapping=[('TYVOR_TEST_CASES','test_protocol.exe'),('TYVOR_RELIABILITY_CASES','test_reliability.exe'),('TYVOR_CORE_CASES','test_core.exe'),('TYVOR_BENCH_REGRESSION_CASES','test_bench_regressions.exe'),('TYVOR_BENCH_CASES','test_bench.exe'),('TYVOR_TIMING_CASES','test_timing.exe'),('TYVOR_DISCOVERY_CASES','test_discovery.exe')]
    for case in listing['tests']:
        name=case['name'];exe=Path(case['command'][0]).name
        known=next((e for key,e in mapping if name in group_inventory(key)),None)
        if known:check(exe==known and case['command'][1:]==[name],'CTest command mismatch: '+name)
        else:
            folder='sim/bench_scenarios' if name in group_inventory('TYVOR_BENCH_SCENARIOS') else 'sim/scenarios'
            check(exe=='tyvor_core_sim.exe' and case['command'][1]=='--script' and Path(case['command'][2])==ROOT/folder/(name+'.txt'),'Scenario command mismatch')
    commands=json.loads((build/'compile_commands.json').read_text())
    for source in ['test_protocol.c','test_reliability.c','test_core.c','test_bench.c','test_bench_regressions.c','test_timing.c','test_discovery.c','discovery_demo.c','tyvor_core.c']:
        command=next(x['command'] for x in commands if Path(x['file']).name==source)
        check(('-DNDEBUG' in command)==(config=='Release'),'Wrong NDEBUG: '+source)
        if source in ['test_discovery.c','discovery_demo.c','tyvor_core.c']:check('-DTYVOR_BENCH_ONLY=1' in command,'Bench guard missing')
    return dict(tests=len(cases),passed=len(cases),failed=0,skipped=0,ndebug=config=='Release')
def outputs(target,config):
    name=label(target,config);build=BUILD/name
    if target=='native':
        files=list(build.glob('*.exe'))+[build/'compile_commands.json',build/'CMakeCache.txt',EVIDENCE/(name+'.xml')]
    else:
        stem='tyvor_discovery_bench' if target=='discovery' else 'tyvor_stm32'
        files=[build/(stem+e) for e in ['.elf','.bin','.map']]+[build/'CMakeCache.txt',build/'compile_commands.json']
    return [dict(path=p.relative_to(ROOT).as_posix(),size=p.stat().st_size,sha256=sha(p)) for p in sorted(files)]
def begin(target,config,compiler):
    EVIDENCE.mkdir(parents=True,exist_ok=True);name=label(target,config)
    receipt=EVIDENCE/(name+'-receipt.json')
    if receipt.exists():receipt.unlink()
    rec=dict(profile='discovery',target=target,config=config,started_ns=time.time_ns(),compiler=compiler_record(compiler),inputs=inputs(target))
    (EVIDENCE/(name+'-pending.json')).write_text(json.dumps(rec,indent=2))
def finish(target,config):
    name=label(target,config);rec=json.loads((EVIDENCE/(name+'-pending.json')).read_text())
    check(rec['inputs']==inputs(target),'Inputs changed during build');check(rec['compiler']==compiler_record(rec['compiler']['path']),'Compiler changed')
    rec['outputs']=outputs(target,config);rec['tests']=native_check(config) if target=='native' else None;rec['finished_ns']=time.time_ns()
    (EVIDENCE/(name+'-receipt.json')).write_text(json.dumps(rec,indent=2)+'\n');print('CURRENT BUILD RECEIPT',name,rec['tests'] or '')
def receipts():
    for target,config in [('native','Debug'),('native','Release'),('discovery','MinSizeRel'),('legacy-ve','MinSizeRel')]:
        rec=json.loads((EVIDENCE/(label(target,config)+'-receipt.json')).read_text())
        check(rec['profile']=='discovery' and rec['inputs']==inputs(target),'Stale receipt inputs: '+target)
        check(rec['compiler']==compiler_record(rec['compiler']['path']),'Compiler mismatch')
        check(rec['outputs']==outputs(target,config),'Stale build outputs: '+target)
        if target=='native':check(rec['tests']==native_check(config),'Stale native evidence')
def inspect():
    from elftools.elf.elffile import ELFFile
    build=BUILD/'discovery';binary=(build/'tyvor_discovery_bench.bin').read_bytes();check(TAG in binary,'Wrong Discovery image identity')
    with (build/'tyvor_discovery_bench.elf').open('rb') as f:
        elf=ELFFile(f);check(elf.header['e_machine']=='EM_ARM','Expected ARM ELF')
        segments=[s for s in elf.iter_segments() if s['p_type']=='PT_LOAD' and s['p_filesz'] and 0x08000000<=s['p_paddr']<0x08100000]
        end=max(s['p_paddr']+s['p_filesz'] for s in segments);image=bytearray(end-0x08000000)
        for s in segments:image[s['p_paddr']-0x08000000:s['p_paddr']-0x08000000+s['p_filesz']]=s.data()
        check(image==binary,'ELF/BIN differ');vectors=struct.unpack_from('<98I',binary)
        check(vectors[0]==0x20020000 and vectors[1]==elf.header['e_entry'],'Invalid SP/reset vector')
        check(all(x==0 or (x&1 and 0x08000000<=x<end) for x in vectors[1:]),'Vector outside image')
        sections={}
        for s in elf.iter_sections():
            if s['sh_flags']&2 and s['sh_size']:
                lo=s['sh_addr'];hi=lo+s['sh_size'];check(any(a<=lo and hi<=b for a,b in [(0x08000000,0x08100000),(0x20000000,0x20020000),(0x10000000,0x10010000)]),'Section outside VG layout')
                sections[s.name]=dict(address=hex(lo),size=s['sh_size'])
        symbols={s.name:s for s in elf.get_section_by_name('.symtab').iter_symbols()}
        check(all(x in symbols for x in ['tyvor_core_poll','tyvor_safety_step','tyvor_mixer_run','discovery_demo_poll','discovery_board_leds']),'Core/demo not linked')
        check(not any(x in symbols for x in ['HAL_UART_Transmit','HAL_UART_Transmit_IT','HAL_UART_Transmit_DMA','HAL_UART_Init',
                  'HAL_SPI_Transmit','HAL_TIM_PWM_Start','HAL_FLASH_Program','HAL_FLASHEx_Erase','malloc','_malloc_r']),
              'Forbidden communication/output/Flash-write/heap path linked')
        check(symbols['_Min_Stack_Size']['st_value']==8192 and symbols['__StackLimit']['st_value']==0x2001e000,'Wrong stack budget')
        ram_sections=[x for x in sections.values() if 0x20000000<=int(x['address'],16)<0x20020000]
        ram_sum=sum(x['size'] for x in ram_sections)
        ram=max(int(x['address'],16)+x['size'] for x in ram_sections)-0x20000000
        result=dict(board='STM32F4 Discovery MB997E',target='STM32F407VGT6',firmware='TYVOR DISCOVERY BENCH V1',flash_address='0x08000000',flash_capacity=1048576,
                    sram_capacity=131072,ccm_capacity=65536,sections=sections,ram_allocated_with_stack=ram,ram_section_bytes=ram_sum,alignment_bytes=ram-ram_sum,stack_reserve=8192,
                    bin_bytes=len(binary),elf_bytes=(build/'tyvor_discovery_bench.elf').stat().st_size,sp=hex(vectors[0]),reset=hex(vectors[1]),vectors=98,
                    bin_sha256=sha(build/'tyvor_discovery_bench.bin'),elf_sha256=sha(build/'tyvor_discovery_bench.elf'),uart_gate='OFF',rf_outputs=False,hardware_tested=False)
    cache=(build/'CMakeCache.txt').read_text();check('TYVOR_DISCOVERY_UART_ENABLED:BOOL=OFF' in cache.splitlines(),'UART gate ON')
    frames=[]
    for p in build.rglob('*.su'):
        for line in p.read_text().splitlines():
            fields=line.split('\t')
            if len(fields)>=3 and fields[1].isdigit():frames.append(dict(function=fields[0],bytes=int(fields[1]),kind=fields[2]))
    result['largest_per_function_stack']=sorted(frames,key=lambda x:x['bytes'],reverse=True)[:20]
    result['legacy_ve_bin_sha256']=sha(BUILD/'legacy-ve/tyvor_stm32.bin')
    result['legacy_matches_phase2c']=result['legacy_ve_bin_sha256']==sha(ROOT/'artifacts/phase2c/stm32/tyvor_stm32.bin')
    check(result['legacy_matches_phase2c'],'Legacy VE BIN changed unexpectedly')
    (EVIDENCE/'memory-report.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:result[k] for k in ['bin_bytes','ram_allocated_with_stack','legacy_matches_phase2c']},indent=2))
def package():
    receipts();memory=json.loads((EVIDENCE/'memory-report.json').read_text())
    check(memory['bin_sha256']==sha(BUILD/'discovery/tyvor_discovery_bench.bin') and memory['elf_sha256']==sha(BUILD/'discovery/tyvor_discovery_bench.elf'),'Stale memory inspection')
    sources=[dict(path=p.relative_to(ROOT).as_posix(),sha256=sha(p)) for p in source_paths()]
    baseline=json.loads((ROOT/'build/discovery-baseline/source-state.json').read_text());old={x['path']:x['sha256'] for x in baseline};changes=[];patch=[]
    check(all((ROOT/name).is_file() for name in old),'Baseline file deleted; review deletion explicitly')
    for item in sources:
        name=item['path'];status='new' if name not in old else 'unchanged' if old[name]==item['sha256'] else 'modified'
        changes.append(dict(path=name,status=status,before_sha256=old.get(name),after_sha256=item['sha256']))
        if status!='unchanged':
            before=ROOT/'build/discovery-baseline/source'/name
            patch.extend(difflib.unified_diff(before.read_text(encoding='utf-8-sig').splitlines(True) if before.exists() else [],(ROOT/name).read_text(encoding='utf-8-sig').splitlines(True),fromfile='a/'+name if before.exists() else '/dev/null',tofile='b/'+name))
    (EVIDENCE/'source-changes.json').write_text(json.dumps(changes,indent=2));(EVIDENCE/'discovery-diff.patch').write_text(''.join(patch),encoding='utf-8')
    stage=inside(ROOT/('artifacts/discovery.staging-'+uuid.uuid4().hex[:8]));stage.mkdir(parents=True)
    def copy(p,dst):
        target=inside(stage/dst);check(target.is_relative_to(stage),'Copy outside staging');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target)
    for ext in ['elf','bin','map']:copy(BUILD/'discovery'/('tyvor_discovery_bench.'+ext),'stm32/tyvor_discovery_bench.'+ext)
    copy(BUILD/'discovery/CMakeCache.txt','stm32/build-config.txt')
    for config in ['debug','release']:
        for p in (BUILD/('native-'+config)).glob('*.exe'):copy(p,'native/'+config+'/'+p.name)
    for p in EVIDENCE.rglob('*'):
        if p.is_file() and p.suffix in ['.json','.txt','.xml','.patch','.pdf'] and 'pending' not in p.name:copy(p,'evidence/'+p.relative_to(EVIDENCE).as_posix())
    files=[dict(path=p.relative_to(stage).as_posix(),size=p.stat().st_size,sha256=sha(p)) for p in sorted(stage.rglob('*')) if p.is_file()]
    manifest=dict(schema=1,board='STM32F4 Discovery MB997E',mcu='STM32F407VGT6',flash_address='0x08000000',uart_gate='OFF',rf_outputs=False,hardware_tested=False,files=files,sources=sources)
    (stage/'SHA256_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
    for x in files:check(sha(stage/x['path'])==x['sha256'],'Package hash failure')
    if OUTPUT.exists():
        check(not OUTPUT.is_symlink(),'Unsafe old package')
        backup=inside(ROOT/('build/discovery-baseline/package-'+uuid.uuid4().hex[:8]));inside(OUTPUT).rename(backup)
    inside(stage).rename(inside(OUTPUT));print('DISCOVERY OFFLINE PACKAGE PASS',len(files),'artifacts',len(sources),'sources')
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['begin','finish','inspect','package','verify']);p.add_argument('--target',choices=['native','discovery','legacy-ve']);p.add_argument('--config',default='Debug');p.add_argument('--compiler');a=p.parse_args()
    if a.action=='begin':begin(a.target,a.config,a.compiler)
    elif a.action=='finish':finish(a.target,a.config)
    elif a.action=='inspect':inspect()
    elif a.action=='package':package()
    else:receipts();print('Four current Discovery receipts PASS')
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError,subprocess.CalledProcessError) as e:print('DISCOVERY ARTIFACT FAIL:',e,file=sys.stderr);sys.exit(1)
