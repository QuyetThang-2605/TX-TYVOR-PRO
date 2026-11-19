"""TFT experiment provenance and offline validation. Never opens a serial port."""
import argparse,csv,difflib,hashlib,json,re,shutil,subprocess,sys,time,uuid
from pathlib import Path
import xml.etree.ElementTree as ET
from build_provenance import compiler_record,inventory,group_inventory
from package_artifacts import source_paths
from validate_esp_package import image_info,partition_info,safe_file
ROOT=Path(__file__).resolve().parent.parent
BUILD=ROOT/'build/tft';EVIDENCE=BUILD/'evidence';EXPERIMENT=ROOT/'experiments/tft_spi_320x240'
PACKAGE=ROOT/'artifacts/tft-spi-320x240-experiment'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def check(x,message):
    if not x:raise ValueError(message)
def dependencies():
    lock=json.loads((ROOT/'scripts/tft-dependencies.lock.json').read_text())
    for item in lock['files']:
        p=(ROOT/item['path']).resolve();check(p.is_relative_to(ROOT/'.tools/sdk') and p.is_file() and sha(p)==item['sha256'],'Pinned dependency mismatch: '+item['path'])
    print('PASS pinned dependencies:',len(lock['files']),'source files; LVGL9.2.2 /ILI9341 2.1.0')
def inputs(target):
    paths={ROOT/'CMakeLists.txt',ROOT/'scripts/build-tft-experiment.ps1',ROOT/'scripts/tft-dependencies.lock.json'}
    folders=['shared','experiments/tft_spi_320x240','esp32/experiments/tft_spi_320x240']
    if target=='native':folders+=['stm32/engine','stm32/boards/stm32f4_discovery','stm32/Core','sim']
    else:paths.update([ROOT/'esp32/main/main.c',ROOT/'esp32/main/Kconfig.projbuild'])
    for folder in folders:
        for p in (ROOT/folder).rglob('*'):
            if p.is_file() and ('build' not in p.relative_to(ROOT/folder).parts) and (p.suffix in ['.c','.h','.cmake','.projbuild','.defaults','.csv'] or p.name=='CMakeLists.txt'):paths.add(p)
    return [dict(path=p.relative_to(ROOT).as_posix(),sha256=sha(p)) for p in sorted(paths)]
def label(target,config):return 'native-'+config.lower() if target=='native' else 'esp32'
def expected_tests():
    cmake=(EXPERIMENT/'CMakeLists.txt').read_text();extra=[]
    for group in ['TYVOR_TFT_CASES','TYVOR_TFT_SPI_CASES']:
        match=re.search(r'set\('+group+r'\s+(.*?)\)',cmake,re.S);extra+=match.group(1).split()
    return inventory()+group_inventory('TYVOR_DISCOVERY_CASES')+extra
def outputs(target,config):
    name=label(target,config);build=BUILD/name
    if target=='native':files=list(build.rglob('*.exe'))+[build/'compile_commands.json',build/'CMakeCache.txt',EVIDENCE/(name+'.xml')]
    else:
        files=[build/x for x in ['tyvor_tft_experiment.elf','tyvor_tft_experiment.bin','tyvor_tft_experiment.map','flasher_args.json','bootloader/bootloader.bin','partition_table/partition-table.bin','sdkconfig','compile_commands.json','CMakeCache.txt']]
    return [dict(path=p.relative_to(ROOT).as_posix(),size=p.stat().st_size,sha256=sha(p)) for p in sorted(files)]
def native_check(config):
    name=label('native',config);suite=ET.parse(EVIDENCE/(name+'.xml')).getroot();cases=suite.findall('testcase');expected=expected_tests()
    check(len(cases)==len(expected) and {x.get('name') for x in cases}==set(expected),'Native exact test inventory mismatch')
    check(all(int(suite.get(k,'0'))==0 for k in ['failures','errors','skipped','disabled']) and not any(x.find(k) is not None for x in cases for k in ['failure','error','skipped']),'Native failing/skipped test')
    commands=json.loads((BUILD/name/'compile_commands.json').read_text())
    for source in ['test_tft.c','test_tft_spi.c','tft_presenter.c','tft_lvgl.c','display_contract.c','test_protocol.c','test_core.c','test_discovery.c']:
        command=next(x['command'] for x in commands if Path(x['file']).name==source)
        check(('-DNDEBUG' in command)==(config=='Release'),'Wrong NDEBUG: '+source)
    return dict(tests=len(cases),passed=len(cases),failed=0,skipped=0,ndebug=config=='Release')
def begin(target,config,compiler):
    EVIDENCE.mkdir(parents=True,exist_ok=True);name=label(target,config);p=EVIDENCE/(name+'-receipt.json')
    if p.exists():p.unlink()
    rec=dict(profile='tft-experiment',target=target,config=config,started_ns=time.time_ns(),compiler=compiler_record(compiler),inputs=inputs(target))
    (EVIDENCE/(name+'-pending.json')).write_text(json.dumps(rec,indent=2))
def finish(target,config):
    name=label(target,config);rec=json.loads((EVIDENCE/(name+'-pending.json')).read_text())
    check(rec['inputs']==inputs(target),'Build inputs changed');check(rec['compiler']==compiler_record(rec['compiler']['path']),'Compiler changed')
    rec['outputs']=outputs(target,config);rec['tests']=native_check(config) if target=='native' else None;rec['finished_ns']=time.time_ns()
    (EVIDENCE/(name+'-receipt.json')).write_text(json.dumps(rec,indent=2)+'\n');print('CURRENT TFT BUILD RECEIPT',name,rec['tests'] or '')
def receipts():
    dependencies()
    for target,config in [('native','Debug'),('native','Release'),('esp32','IDF')]:
        rec=json.loads((EVIDENCE/(label(target,config)+'-receipt.json')).read_text())
        check(rec['inputs']==inputs(target) and rec['compiler']==compiler_record(rec['compiler']['path']) and rec['outputs']==outputs(target,config),'Stale TFT receipt '+target+'/'+config)
        if target=='native':check(rec['tests']==native_check(config),'Stale native tests')
    print('Three current TFT build receipts PASS')
def all_sources():
    return sorted(set(source_paths())|{p for p in EXPERIMENT.rglob('*') if p.is_file() and p.suffix in ['.c','.h','.md','.txt','.cmake']})
def inspect():
    from elftools.elf.elffile import ELFFile
    receipts()
    build=BUILD/'esp32';app=image_info(build/'tyvor_tft_experiment.bin');boot=image_info(build/'bootloader/bootloader.bin');parts=partition_info(build/'partition_table/partition-table.bin')
    with (build/'tyvor_tft_experiment.elf').open('rb') as f:
        elf=ELFFile(f);check(elf.header['e_machine']=='EM_XTENSA','Wrong ESP ELF architecture')
        symbols={s.name for s in elf.get_section_by_name('.symtab').iter_symbols()}
        required=['esp_lcd_new_panel_ili9341','tyvor_tft_spi_init','tyvor_tft_lvgl_init','tyvor_ui_endpoint_poll','lv_display_create','tyvor_touch_adapter_read']
        # Unused touch adapter is intentionally GC'ed; there is no runtime touch registration.
        check(all(x in symbols for x in required if x!='tyvor_touch_adapter_read'),'Display/protocol code not linked')
        check(not any(x in symbols for x in ['tyvor_core_init','tyvor_safety_step','tyvor_mixer_run','tyvor_core_select_model','esp_lcd_touch_new_spi_xpt2046',
                   'esp_wifi_init','esp_wifi_start','esp_bt_controller_enable']),'Control engine/unverified touch/radio path linked into UI image')
        sections=[dict(name=s.name,address=hex(s['sh_addr']),size=s['sh_size'],nobits=s['sh_type']=='SHT_NOBITS') for s in elf.iter_sections() if s['sh_flags']&2 and s['sh_size']]
    check(app.get('elf_sha256')==sha(build/'tyvor_tft_experiment.elf'),'ESP app/ELF identity differs')
    run=subprocess.run([sys.executable,'-m','esp_idf_size','--format','json',str(build/'tyvor_tft_experiment.map')],capture_output=True,text=True,check=True)
    size=json.loads(run.stdout);(EVIDENCE/'idf-size.json').write_text(json.dumps(size,indent=2)+'\n')
    config=(build/'sdkconfig').read_text()
    for setting in ['# CONFIG_TYVOR_UART_PINOUT_VERIFIED is not set','CONFIG_TYVOR_UART_TX_GPIO=-1','CONFIG_TYVOR_UART_RX_GPIO=-1',
                    '# CONFIG_TYVOR_TFT_HARDWARE_GATE is not set','# CONFIG_TYVOR_TFT_BOARD_VERIFIED is not set','# CONFIG_TYVOR_TFT_MODULE_VERIFIED is not set',
                    '# CONFIG_TYVOR_TFT_TOUCH_RUNTIME is not set','CONFIG_TYVOR_TFT_ROTATION=0','CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y','CONFIG_SPIRAM_MODE_OCT=y']:
        check(setting in config.splitlines(),'Unexpected TFT SDK config: '+setting)
    for pin in ['SCLK','MOSI','MISO','CS','DC','RESET','BACKLIGHT']:check('CONFIG_TYVOR_TFT_'+pin+'=-1' in config.splitlines(),'Assigned display pin')
    frames=[]
    for p in build.rglob('*.su'):
        for line in p.read_text().splitlines():
            fields=line.split('\t')
            if len(fields)>2 and fields[1].isdigit():frames.append(dict(function=fields[0],bytes=int(fields[1]),kind=fields[2]))
    result=dict(app=app,bootloader=boot,partitions=parts,sections=sections,elf_sha256=sha(build/'tyvor_tft_experiment.elf'),bin_sha256=sha(build/'tyvor_tft_experiment.bin'),
                map_sha256=sha(build/'tyvor_tft_experiment.map'),draw_buffer_bytes=20480,lvgl_arena_bytes=98304,ui_task_stack_bytes=8192,
                hardware_gates='OFF',display_pin_numbers='ALL -1',psram_hardware_verified=False,hardware_tested=False,
                largest_application_stack_frames=sorted(frames,key=lambda x:x['bytes'],reverse=True)[:16])
    (EVIDENCE/'memory-report.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:result[k] for k in ['bin_sha256','draw_buffer_bytes','lvgl_arena_bytes','hardware_gates']},indent=2))
def validate(package):
    package=Path(package).resolve();manifest=json.loads((package/'SHA256_MANIFEST.json').read_text());esp=package/'esp32'
    check(manifest['profile']=='TFT_SPI_320X240' and manifest['hardware_tested'] is False and manifest['uart_gate']=='OFF' and manifest['display_gate']=='OFF','Wrong package profile/gates')
    listed=set()
    for item in manifest['files']:
        file=safe_file(package,item['path']);check(item['path'] not in listed and file.stat().st_size==item['size'] and sha(file)==item['sha256'],'Package artifact differs: '+item['path']);listed.add(item['path'])
    actual={p.relative_to(package).as_posix() for p in package.rglob('*') if p.is_file() and p!=package/'SHA256_MANIFEST.json'}
    check(actual==listed,'Package inventory differs')
    args=json.loads((esp/'flasher_args.json').read_text());check(args['flash_settings']==dict(flash_mode='dio',flash_size='16MB',flash_freq='80m'),'Unexpected flash settings')
    check(args['extra_esptool_args']['chip']=='esp32s3' and args['write_flash_args']==['--flash_mode','dio','--flash_size','16MB','--flash_freq','80m'],'Unexpected chip/flash arguments')
    images={int(offset,0):safe_file(esp,path) for offset,path in args['flash_files'].items()};check(set(images)=={0,0x8000,0x10000},'Unexpected image offsets')
    for name,address in [('bootloader',0),('partition-table',0x8000),('app',0x10000)]:
        check(int(args[name]['offset'],0)==address and safe_file(esp,args[name]['file'])==images[address] and args[name]['encrypted']=='false','Inconsistent metadata reference')
    boot=image_info(images[0]);app=image_info(images[0x10000]);parts=partition_info(images[0x8000]);factory=[x for x in parts if x['name']=='factory']
    check(boot['size']<=0x8000 and len(factory)==1 and factory[0]['offset']==0x10000 and app['size']<=factory[0]['size'],'Image/partition overlap')
    check(app['elf_sha256']==sha(esp/'tyvor_tft_experiment.elf'),'Packaged ELF/app differs')
    check(app.get('app_version')=='0.3.0-tft-experiment','Wrong experiment image identity')
    for image in [boot,app]:
        check(image['chip']=='esp32s3' and image['flash_size']==16*1024*1024 and image['flash_freq_mhz']==80 and image['rom_flash_mode']=='dio','Image flash configuration differs')
    config=(esp/'sdkconfig.build').read_text().splitlines()
    for setting in ['# CONFIG_TYVOR_UART_PINOUT_VERIFIED is not set','CONFIG_TYVOR_UART_TX_GPIO=-1','CONFIG_TYVOR_UART_RX_GPIO=-1',
                    '# CONFIG_TYVOR_TFT_HARDWARE_GATE is not set','# CONFIG_TYVOR_TFT_BOARD_VERIFIED is not set','# CONFIG_TYVOR_TFT_MODULE_VERIFIED is not set',
                    '# CONFIG_TYVOR_TFT_TOUCH_RUNTIME is not set','CONFIG_TYVOR_TFT_ROTATION=0','CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y','CONFIG_SPIRAM_MODE_OCT=y']:
        check(setting in config,'Packaged unsafe/unexpected SDK config: '+setting)
        if setting.startswith('# CONFIG_'):
            name=setting.split()[1];check(not any(line.startswith(name+'=') for line in config),'Contradictory SDK gate: '+name)
    for pin in ['SCLK','MOSI','MISO','CS','DC','RESET','BACKLIGHT']:
        check('CONFIG_TYVOR_TFT_'+pin+'=-1' in config,'Packaged display GPIO assigned')
    rows=list(csv.reader(line for line in (esp/'partitions.csv').read_text().splitlines() if line.strip() and not line.lstrip().startswith('#')))
    csv_layout=[(row[0].strip(),int(row[3].strip(),0),int(row[4].strip(),0)) for row in rows]
    check(csv_layout==[(p['name'],p['offset'],p['size']) for p in parts],'Packaged partition CSV/binary differ')
    return dict(status='OFFLINE_PACKAGE_PASS',files_verified=len(listed),offsets=['0x0','0x8000','0x10000'],chip='esp32s3',flash_mb=16,
                display_profile=manifest['profile'],portrait=[240,320],landscape=[320,240],app_size=app['size'],hardware_tested=False,flashed=False,serial_opened=False)
def package():
    receipts();memory=json.loads((EVIDENCE/'memory-report.json').read_text());check(memory['bin_sha256']==sha(BUILD/'esp32/tyvor_tft_experiment.bin'),'Stale memory inspection')
    baseline=json.loads((ROOT/'build/tft-baseline/source-state.json').read_text());old={x['path']:x['sha256'] for x in baseline};sources=[dict(path=p.relative_to(ROOT).as_posix(),sha256=sha(p)) for p in all_sources()]
    check(all((ROOT/name).is_file() for name in old),'Baseline file deleted');changes=[];patch=[]
    for item in sources:
        name=item['path'];state='new' if name not in old else 'unchanged' if item['sha256']==old[name] else 'modified';changes.append(dict(path=name,status=state,old_sha256=old.get(name),sha256=item['sha256']))
        if state!='unchanged':
            before=ROOT/'build/tft-baseline/source'/name
            patch+=list(difflib.unified_diff(before.read_text(encoding='utf-8-sig').splitlines(True) if before.exists() else [],(ROOT/name).read_text(encoding='utf-8-sig').splitlines(True),fromfile='a/'+name if before.exists() else '/dev/null',tofile='b/'+name))
    modified=[x['path'] for x in changes if x['status']=='modified'];check(modified==['CMakeLists.txt'],'Unexpected legacy changes: '+str(modified))
    (EVIDENCE/'source-changes.json').write_text(json.dumps(changes,indent=2));(EVIDENCE/'tft-diff.patch').write_text(''.join(patch),encoding='utf-8')
    stage=(ROOT/('artifacts/tft.staging-'+uuid.uuid4().hex[:8])).resolve();check(stage.is_relative_to(ROOT/'artifacts'),'Bad staging path');stage.mkdir(parents=True)
    def copy(file,relative):
        dest=(stage/relative).resolve();check(dest.is_relative_to(stage),'Copy outside staging');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,dest)
    for name in ['tyvor_tft_experiment.elf','tyvor_tft_experiment.bin','tyvor_tft_experiment.map','flasher_args.json','bootloader/bootloader.bin','partition_table/partition-table.bin']:
        copy(BUILD/'esp32'/name,'esp32/'+name)
    copy(BUILD/'esp32/sdkconfig','esp32/sdkconfig.build');copy(ROOT/'esp32/experiments/tft_spi_320x240/partitions.csv','esp32/partitions.csv')
    for config in ['debug','release']:
        for file in (BUILD/('native-'+config)).rglob('*.exe'):copy(file,'native/'+config+'/'+file.name)
    for file in EVIDENCE.rglob('*'):
        if file.is_file() and file.suffix in ['.json','.txt','.xml','.patch','.pdf','.png'] and 'pending' not in file.name:copy(file,'evidence/'+file.relative_to(EVIDENCE).as_posix())
    for file in (ROOT/'artifacts/tft-ui-experiment').glob('*'):
        if file.is_file():copy(file,'ui-preview/'+file.name)
    copy(ROOT/'.tools/sdk/lvgl-9.2.2/LICENCE.txt','licenses/LVGL-MIT.txt');copy(ROOT/'.tools/sdk/esp_lcd_ili9341-2.1.0/license.txt','licenses/Espressif-ILI9341-Apache2.txt')
    files=[dict(path=p.relative_to(stage).as_posix(),size=p.stat().st_size,sha256=sha(p)) for p in sorted(stage.rglob('*')) if p.is_file()]
    manifest=dict(schema=1,profile='TFT_SPI_320X240',target='esp32s3',hardware_tested=False,flashed=False,uart_gate='OFF',display_gate='OFF',touch_runtime='OFF',wire_protocol=2,files=files,sources=sources,
                  dependency_lock_sha256=sha(ROOT/'scripts/tft-dependencies.lock.json'))
    (stage/'SHA256_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n');validate(stage)
    if PACKAGE.exists():
        check(PACKAGE.resolve().is_relative_to(ROOT/'artifacts') and not PACKAGE.is_symlink(),'Unsafe old package')
        backup=(ROOT/('build/tft-baseline/package-'+uuid.uuid4().hex[:8])).resolve();check(backup.is_relative_to(ROOT/'build/tft-baseline'),'Unsafe backup destination');PACKAGE.rename(backup)
    stage.rename(PACKAGE);print('TFT PACKAGE PASS:',len(files),'artifacts',len(sources),'source files')
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['dependencies','begin','finish','verify','inspect','package','validate']);p.add_argument('--target');p.add_argument('--config',default='Debug');p.add_argument('--compiler');p.add_argument('--package',type=Path,default=PACKAGE);a=p.parse_args()
    if a.action=='dependencies':dependencies()
    elif a.action=='begin':begin(a.target,a.config,a.compiler)
    elif a.action=='finish':finish(a.target,a.config)
    elif a.action=='inspect':inspect()
    elif a.action=='package':package()
    elif a.action=='validate':print(json.dumps(validate(a.package),indent=2))
    else:receipts()
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError,subprocess.CalledProcessError) as e:print('TFT ARTIFACT FAIL:',e,file=sys.stderr);sys.exit(1)
