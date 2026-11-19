"""UI V3.2 exact build provenance, preservation and offline package validation."""
import argparse,difflib,json,re,shutil,subprocess,sys,time
from pathlib import Path
import xml.etree.ElementTree as ET
from tft_artifacts import sha,check,dependencies,expected_tests as tft_tests
from package_artifacts import source_paths
from build_provenance import compiler_record
from validate_esp_package import image_info,partition_info,safe_file
ROOT=Path(__file__).resolve().parent.parent;BUILD=ROOT/'build/ui-v3-2';EVIDENCE=BUILD/'evidence';OUT=ROOT/'artifacts/tyvor-ui-v3-2'
def sources():
    return sorted(set(source_paths())|{p for p in (ROOT/'experiments').rglob('*') if p.is_file() and (p.suffix in ['.c','.h','.md','.txt','.cmake','.svg','.json'] or p.name=='LICENSE')})
def inputs(target):
    files={ROOT/'CMakeLists.txt',ROOT/'scripts/build-ui-v3-2.ps1',ROOT/'scripts/tft-dependencies.lock.json'}
    files.update((ROOT/'docs/ui-reference/oxbot').glob('*.png'))
    files.update((ROOT/'docs/ui-reference/tyvor-v3-2').glob('*.png'))
    folders=['experiments/tyvor_ui_v2','esp32/experiments/tyvor_ui_v2','shared','experiments/tyvor_ui_v3_2','experiments/tft_spi_320x240','esp32/experiments/tyvor_ui_v3_2','esp32/experiments/tft_spi_320x240','esp32/main']
    if target=='native':folders+=['experiments/tyvor_ui_v3','experiments/tyvor_ui_v3_1','sim','stm32/engine','stm32/Core','stm32/boards/stm32f4_discovery']
    for folder in folders:
        files.update(p for p in (ROOT/folder).rglob('*') if p.is_file() and 'build' not in p.relative_to(ROOT/folder).parts and (p.suffix in ['.c','.h','.cmake','.projbuild','.defaults','.csv'] or p.name=='CMakeLists.txt'))
    return [dict(path=p.relative_to(ROOT).as_posix(),sha256=sha(p)) for p in sorted(files)]
def name(target,config):return 'native-'+config.lower() if target=='native' else 'esp32'
def tests(config):
    cmake=(ROOT/'experiments/tyvor_ui_v3_2/CMakeLists.txt').read_text();extra=re.search(r'set\(TYVOR_UI32_CASES\s+(.*?)\)',cmake,re.S).group(1).split()
    old=(ROOT/'experiments/tyvor_ui_v2/CMakeLists.txt').read_text();previous=re.search(r'set\(TYVOR_UI2_CASES\s+(.*?)\)',old,re.S).group(1).split()
    v3=(ROOT/'experiments/tyvor_ui_v3/CMakeLists.txt').read_text();prior3=re.search(r'set\(TYVOR_UI3_CASES\s+(.*?)\)',v3,re.S).group(1).split()
    quality=re.search(r'set\(TYVOR_UI32_QUALITY_CASES\s+(.*?)\)',cmake,re.S).group(1).split()
    prior31=(ROOT/'experiments/tyvor_ui_v3_1/CMakeLists.txt').read_text();cases31=re.search(r'set\(TYVOR_UI31_CASES\s+(.*?)\)',prior31,re.S).group(1).split();quality31=re.search(r'set\(TYVOR_UI31_QUALITY_CASES\s+(.*?)\)',prior31,re.S).group(1).split()
    expected=tft_tests()+previous+['ui2_pc_mouse_keyboard']+prior3+['ui3_pc_mouse_keyboard']+cases31+['ui31_pc_mouse_keyboard']+quality31+extra+['ui32_pc_mouse_keyboard']+quality;suite=ET.parse(EVIDENCE/('native-'+config.lower()+'.xml')).getroot();cases=suite.findall('testcase')
    check(len(cases)==len(expected) and {c.get('name') for c in cases}==set(expected),'Exact native test inventory differs')
    check(all(int(suite.get(k,'0'))==0 for k in ['failures','errors','skipped','disabled']) and not any(c.find(k) is not None for c in cases for k in ['failure','error','skipped']),'Tests failed/skipped')
    commands=json.loads((BUILD/('native-'+config.lower())/'compile_commands.json').read_text())
    for file in ['test_ui.c','test_quality.c','ui.c','ui_pages.c','ui_view.c','test_core.c','test_protocol.c','test_tft.c','simulator_win32.c']:
        matches=[c for c in commands if Path(c['file']).name==file];check(bool(matches),'Missing compile command '+file)
        for command in matches:check(('-DNDEBUG' in command['command'])==(config=='Release'),'NDEBUG mismatch '+command['file'])
    return dict(tests=len(cases),passed=len(cases),failures=0,skipped=0,ndebug=config=='Release')
def outputs(target,config):
    folder=BUILD/name(target,config)
    if target=='native':files=list(folder.rglob('*.exe'))+[folder/'CMakeCache.txt',folder/'compile_commands.json',EVIDENCE/('native-'+config.lower()+'.xml')]
    else:files=[folder/p for p in ['tyvor_ui_v3_2.elf','tyvor_ui_v3_2.bin','tyvor_ui_v3_2.map','sdkconfig','flasher_args.json','bootloader/bootloader.bin','partition_table/partition-table.bin','CMakeCache.txt','compile_commands.json']]
    return [dict(path=p.relative_to(ROOT).as_posix(),size=p.stat().st_size,sha256=sha(p)) for p in sorted(files)]
def begin(target,config,compiler):
    EVIDENCE.mkdir(parents=True,exist_ok=True);receipt=EVIDENCE/(name(target,config)+'-receipt.json')
    if receipt.exists():receipt.unlink()
    (EVIDENCE/(name(target,config)+'-pending.json')).write_text(json.dumps(dict(target=target,config=config,compiler=compiler_record(compiler),inputs=inputs(target),started_ns=time.time_ns()),indent=2))
def finish(target,config):
    record=json.loads((EVIDENCE/(name(target,config)+'-pending.json')).read_text());check(record['inputs']==inputs(target),'Build inputs changed during build')
    check(record['compiler']==compiler_record(record['compiler']['path']),'Compiler changed')
    record.update(outputs=outputs(target,config),tests=tests(config) if target=='native' else None,finished_ns=time.time_ns())
    (EVIDENCE/(name(target,config)+'-receipt.json')).write_text(json.dumps(record,indent=2)+'\n');print('UI V3.2 BUILD RECEIPT',name(target,config),record['tests'])
def verify_native(config):
    record=json.loads((EVIDENCE/('native-'+config.lower()+'-receipt.json')).read_text())
    check(record['inputs']==inputs('native') and record['outputs']==outputs('native',config) and record['tests']==tests(config) and record['compiler']==compiler_record(record['compiler']['path']),'Stale native receipt')
def verify():
    dependencies()
    profiles=json.loads((ROOT/'esp32/experiments/tyvor_ui_v3_2/display_profiles.json').read_text())['profiles']
    check([(p['logical_width'],p['logical_height']) for p in profiles]==[(320,240),(448,368)] and profiles[0]['rotation_default']==1 and profiles[1]['hardware_rotation'] is None and all(p['hardware_gate']=='OFF' and p['hardware_verified'] is False for p in profiles),'Unexpected display profiles')
    for config in ['Debug','Release']:verify_native(config)
    r=json.loads((EVIDENCE/'esp32-receipt.json').read_text());check(r['inputs']==inputs('esp32') and r['outputs']==outputs('esp32','IDF') and r['compiler']==compiler_record(r['compiler']['path']),'Stale ESP receipt')
    print('PASS three current UI V3.2 receipts')
def config_check(path):
    lines=path.read_text().splitlines()
    expected=['# CONFIG_TYVOR_UART_PINOUT_VERIFIED is not set','# CONFIG_TYVOR_TFT_HARDWARE_GATE is not set','# CONFIG_TYVOR_TFT_TOUCH_RUNTIME is not set',
              '# CONFIG_TYVOR_TFT_BOARD_VERIFIED is not set','# CONFIG_TYVOR_TFT_MODULE_VERIFIED is not set','CONFIG_TYVOR_UART_TX_GPIO=-1','CONFIG_TYVOR_UART_RX_GPIO=-1',
              'CONFIG_TYVOR_TFT_ROTATION=1','CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y','CONFIG_SPIRAM_MODE_OCT=y','CONFIG_TYVOR_TFT_SIMULATOR=y']
    expected+=['CONFIG_TYVOR_TFT_'+p+'=-1' for p in ['SCLK','MOSI','MISO','CS','DC','RESET','BACKLIGHT']]
    for line in expected:
        check(line in lines,'Unsafe/unexpected SDK config: '+line)
        if line.startswith('# CONFIG_'):check(not any(x.startswith(line.split()[1]+'=') for x in lines),'Contradictory gate config')
def inspect():
    from elftools.elf.elffile import ELFFile
    verify();b=BUILD/'esp32';config_check(b/'sdkconfig');app=image_info(b/'tyvor_ui_v3_2.bin')
    with (b/'tyvor_ui_v3_2.elf').open('rb') as stream:
        elf=ELFFile(stream);check(elf['e_machine']=='EM_XTENSA','Wrong architecture');table=elf.get_section_by_name('.symtab');symbols={s.name for s in table.iter_symbols()}
        check(all(s in symbols for s in ['tyvor_ui32_init','tyvor_ui32_project','tyvor_ui32_pages_create','tyvor_ui32_timer_compact','tyvor_ui32_timer_large','tyvor_ui32_brand','tyvor_ui32_owl_compact','tyvor_ui32_wordmark_large','tyvor_ui32_background_320','tyvor_ui32_background_448','tyvor_ui32_icon_data','tyvor_ui32_icon_sized','tyvor_ui32_pack_rgb888','tyvor_ui_endpoint_poll','esp_lcd_new_panel_ili9341']),'Missing UI/protocol/driver/brand symbols')
        check(not any(s in symbols for s in ['tyvor_core_init','tyvor_safety_step','tyvor_mixer_run','tyvor_core_load_model','esp_wifi_start','esp_bt_controller_enable','esp_lcd_touch_new_spi_xpt2046']),'Forbidden Core/radio/touch runtime linked')
        fonts=[dict(name=s.name,size=s['st_size']) for s in table.iter_symbols() if 'lv_font_montserrat' in s.name]
        sections=[dict(name=s.name,size=s['sh_size'],address=hex(s['sh_addr'])) for s in elf.iter_sections() if s['sh_flags']&2 and s['sh_size']]
    check(app['elf_sha256']==sha(b/'tyvor_ui_v3_2.elf') and app.get('app_version')=='0.5.2-ui-v3-2','Wrong image identity')
    size=subprocess.run([sys.executable,'-m','esp_idf_size','--format','json',str(b/'tyvor_ui_v3_2.map')],check=True,capture_output=True,text=True)
    (EVIDENCE/'idf-size.json').write_text(size.stdout)
    file_sizes=subprocess.run([sys.executable,'-m','esp_idf_size','--files','--format','json',str(b/'tyvor_ui_v3_2.map')],check=True,capture_output=True,text=True)
    (EVIDENCE/'idf-file-sizes.json').write_text(file_sizes.stdout)
    frames=[]
    for p in b.rglob('*.su'):
        for line in p.read_text().splitlines():
            f=line.split('\t')
            if len(f)>2 and f[1].isdigit():frames.append(dict(function=f[0],bytes=int(f[1]),kind=f[2]))
    report=dict(app=app,bootloader=image_info(b/'bootloader/bootloader.bin'),partitions=partition_info(b/'partition_table/partition-table.bin'),
                elf_sha256=sha(b/'tyvor_ui_v3_2.elf'),bin_sha256=sha(b/'tyvor_ui_v3_2.bin'),sections=sections,font_descriptors=fonts,
                draw_buffer=20480,lvgl_arena=98304,ui_task_stack=8192,largest_stack_frames=sorted(frames,key=lambda x:x['bytes'],reverse=True)[:15],hardware_tested=False)
    (EVIDENCE/'memory-report.json').write_text(json.dumps(report,indent=2)+'\n');print('MEMORY/IMAGE IDENTITY PASS',app['size'],report['bin_sha256'])
def preservation():
    baseline=json.loads((ROOT/'build/ui-v3-2-baseline/source-state.json').read_text());modified=[];patch=[];changes=[];old={r['path']:r['sha256'] for r in baseline}
    for p in sources():
        name=p.relative_to(ROOT).as_posix();state='new' if name not in old else 'unchanged' if sha(p)==old[name] else 'modified'
        if state=='modified':modified.append(name)
        changes.append(dict(path=name,status=state,sha256=sha(p)))
        if state!='unchanged':
            before=ROOT/'build/ui-v3-2-baseline/source'/name;patch+=list(difflib.unified_diff(before.read_text(encoding='utf-8-sig').splitlines(True) if before.exists() else [],p.read_text(encoding='utf-8-sig').splitlines(True),fromfile='a/'+name if before.exists() else '/dev/null',tofile='b/'+name))
    check(modified==['CMakeLists.txt'] and all((ROOT/name).is_file() for name in old),'Unexpected legacy source mutation')
    (EVIDENCE/'source-changes.json').write_text(json.dumps(changes,indent=2));(EVIDENCE/'ui-v3-2-diff.patch').write_text(''.join(patch),encoding='utf-8')
    git=subprocess.run(['git','status','--short','--untracked-files=normal'],cwd=ROOT,check=True,capture_output=True,text=True);(EVIDENCE/'git-status.txt').write_text(git.stdout+git.stderr)
    from ui31_artifacts import validate as old_validate,OUT as PREVIOUS
    prior=old_validate(PREVIOUS)
    (EVIDENCE/'preservation.json').write_text(json.dumps(dict(baseline=len(old),unchanged=len(old)-1,modified=modified,new_files=[r['path'] for r in changes if r['status']=='new'],original_ui_v31_package=prior,hardware_tested=False),indent=2)+'\n')
    print('PRESERVATION PASS',len(old)-1,'old files unchanged; only root CMake modified')
def validate(folder):
    folder=Path(folder).resolve();m=json.loads((folder/'SHA256_MANIFEST.json').read_text());check(m['profile']=='TYVOR_UI_V32' and m['hardware_tested'] is False and m['gates']=='DISPLAY/TOUCH/UART/RF OFF','Wrong manifest policy')
    listed=set()
    for r in m['files']:
        f=safe_file(folder,r['path']);check(r['path'] not in listed and sha(f)==r['sha256'] and f.stat().st_size==r['size'],'Artifact mismatch '+r['path']);listed.add(r['path'])
    actual={p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file() and p!=folder/'SHA256_MANIFEST.json'};check(actual==listed,'Package inventory differs')
    b=folder/'esp32';config_check(b/'sdkconfig.build');args=json.loads((b/'flasher_args.json').read_text())
    profiles=json.loads((b/'display_profiles.json').read_text())['profiles']
    check([(p['logical_width'],p['logical_height']) for p in profiles]==[(320,240),(448,368)] and profiles[0]['rotation_default']==1 and profiles[1]['hardware_rotation'] is None and all(p['hardware_gate']=='OFF' and p['hardware_verified'] is False for p in profiles),'Packaged profile policy differs')
    check(args['flash_settings']==dict(flash_mode='dio',flash_size='16MB',flash_freq='80m') and args['extra_esptool_args']['chip']=='esp32s3','Wrong flash settings')
    check(args['write_flash_args']==['--flash_mode','dio','--flash_size','16MB','--flash_freq','80m'],'Wrong write args')
    paths={int(k,0):safe_file(b,v) for k,v in args['flash_files'].items()};check(set(paths)=={0,0x8000,0x10000},'Wrong offsets')
    for name,addr in [('bootloader',0),('partition-table',0x8000),('app',0x10000)]:check(int(args[name]['offset'],0)==addr and safe_file(b,args[name]['file'])==paths[addr] and args[name]['encrypted']=='false','Metadata inconsistency')
    boot=image_info(paths[0]);app=image_info(paths[0x10000]);parts=partition_info(paths[0x8000]);factory=[p for p in parts if p['name']=='factory']
    check(boot['size']<=0x8000 and len(factory)==1 and factory[0]['offset']==0x10000 and app['size']<=factory[0]['size'],'Partition overlap')
    check(app.get('app_version')=='0.5.2-ui-v3-2' and app['elf_sha256']==sha(b/'tyvor_ui_v3_2.elf'),'Wrong image/ELF identity')
    for image in [app,boot]:check(image['chip']=='esp32s3' and image['flash_size']==16777216 and image['flash_freq_mhz']==80 and image['rom_flash_mode']=='dio','Image header config differs')
    import csv
    rows=csv.reader(x for x in (b/'partitions.csv').read_text().splitlines() if x.strip() and not x.lstrip().startswith('#'))
    check([(x[0].strip(),int(x[3],0),int(x[4],0)) for x in rows]==[(x['name'],x['offset'],x['size']) for x in parts],'Partition CSV/binary differs')
    return dict(status='OFFLINE_PACKAGE_PASS',files=len(listed),app_size=app['size'],hardware_tested=False,flashed=False,serial_opened=False)
def package():
    verify();preservation();memory=json.loads((EVIDENCE/'memory-report.json').read_text());check(memory['bin_sha256']==sha(BUILD/'esp32/tyvor_ui_v3_2.bin'),'Stale size report')
    render=json.loads((EVIDENCE/'render-validation.json').read_text())
    check(not render['draft'] and render['count']==152 and render['layout_errors']==0 and render['renderer_sha256']==sha(BUILD/'native-release/experiments/tyvor_ui_v3_2/tyvor_ui32_render.exe'),'Stale/draft render evidence')
    for row in render['images']:check(sha(OUT/row['path'])==row['sha256'],'Stale screenshot '+row['path'])
    def copy(p,name):d=OUT/name;d.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,d)
    for p in ['tyvor_ui_v3_2.elf','tyvor_ui_v3_2.bin','tyvor_ui_v3_2.map','flasher_args.json','bootloader/bootloader.bin','partition_table/partition-table.bin']:copy(BUILD/'esp32'/p,'esp32/'+p)
    copy(BUILD/'esp32/sdkconfig','esp32/sdkconfig.build');copy(ROOT/'esp32/experiments/tyvor_ui_v3_2/partitions.csv','esp32/partitions.csv')
    copy(ROOT/'esp32/experiments/tyvor_ui_v3_2/display_profiles.json','esp32/display_profiles.json')
    for c in ['debug','release']:
        for p in (BUILD/('native-'+c)).rglob('*.exe'):copy(p,'native/'+c+'/'+p.name)
    for p in EVIDENCE.iterdir():
        if p.is_file() and p.suffix in ['.json','.txt','.xml','.patch'] and 'pending' not in p.name:copy(p,'evidence/'+p.name)
    copy(ROOT/'experiments/tyvor_ui_v3_2/assets/lucide/LICENSE','licenses/Lucide-ISC-Feather-MIT.txt')
    copy(ROOT/'.tools/sdk/lvgl-9.2.2/LICENCE.txt','licenses/LVGL-MIT.txt');copy(ROOT/'.tools/sdk/esp_lcd_ili9341-2.1.0/license.txt','licenses/ILI9341-Apache2.txt')
    manifest=dict(schema=1,profile='TYVOR_UI_V32',hardware_tested=False,gates='DISPLAY/TOUCH/UART/RF OFF',wire_protocol=2,
                  files=[dict(path=p.relative_to(OUT).as_posix(),size=p.stat().st_size,sha256=sha(p)) for p in sorted(OUT.rglob('*')) if p.is_file() and p.name!='SHA256_MANIFEST.json'],
                  sources=[dict(path=p.relative_to(ROOT).as_posix(),sha256=sha(p)) for p in sources()])
    (OUT/'SHA256_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(validate(OUT),indent=2))
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['begin','finish','verify','inspect','preservation','package','validate']);p.add_argument('--target');p.add_argument('--config',default='Debug');p.add_argument('--compiler');p.add_argument('--package',type=Path,default=OUT);a=p.parse_args()
    if a.action=='begin':begin(a.target,a.config,a.compiler)
    elif a.action=='finish':finish(a.target,a.config)
    elif a.action=='validate':print(json.dumps(validate(a.package),indent=2))
    else:globals()[a.action]()
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError,subprocess.CalledProcessError) as e:print('UI V3.2 FAIL:',e,file=sys.stderr);sys.exit(1)
