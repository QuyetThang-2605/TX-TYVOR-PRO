"""Inventory/read-only backup/offline preflight/printed flash plan. NO flash action.

Only allowlisted CubeProgrammer list, HOTPLUG upload and option-display commands
can reach subprocess. A flash-plan prints a reviewed command, never executes it.
"""
import argparse,datetime,hashlib,json,re,shutil,struct,subprocess,sys,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
PACKAGE=ROOT/'artifacts/stm32f4-discovery-bench-v1'
EVIDENCE=ROOT/'build/discovery/evidence'
TAG=b'TYVOR DISCOVERY BENCH V1|MB997E|STM32F407VG|UART_OFF|RF_OFF'
READS={(0xe0042000,4),(0x1fff7a22,2),(0x40023c14,4),(0x1fff7a10,12),(0x08000000,1048576)}
def check(x,message):
    if not x:raise ValueError('BLOCKED: '+message)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def inside(p):
    p=Path(p).resolve();check(p.is_relative_to(ROOT) and p!=ROOT,'path must stay inside workspace');return p
def json_read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def locate_cli(explicit=None):
    if explicit:
        path=Path(explicit).resolve();check(path.is_file(),'CubeProgrammer CLI path missing');return path
    found=shutil.which('STM32_Programmer_CLI.exe')
    if found:return Path(found).resolve()
    for p in [Path('C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe')]:
        if p.is_file():return p
    return None
def pnp():
    # Fixed read-only PowerShell query. No user text interpolation or port open.
    query="$ErrorActionPreference='Stop'; @(Get-PnpDevice -PresentOnly | Where-Object {$_.InstanceId -match 'VID_0483' -or $_.FriendlyName -match 'ST-LINK|STM32'} | Select-Object Status,Class,FriendlyName,InstanceId) | ConvertTo-Json -Depth 4 -Compress"
    r=subprocess.run(['powershell.exe','-NoProfile','-NonInteractive','-Command',query],capture_output=True,timeout=30)
    check(r.returncode==0,'PnP query failed; do not infer no-device from access errors')
    text=r.stdout.decode('utf-8-sig',errors='replace').strip()
    data=json.loads(text) if text else []
    return data if isinstance(data,list) else [data]
def allow_readonly(args,backup_dir):
    if args==['-l','stlink']:return
    check(len(args)>=7 and args[:2]==['-c','port=SWD'],'unexpected CLI operation')
    check(re.fullmatch(r'sn=[A-Fa-f0-9]{24}',args[2]) is not None,'explicit probe serial required')
    check(args[3:5]==['mode=HOTPLUG','freq=1000'],'only HOTPLUG/read-only SWD is allowed')
    suffix=args[5:]
    if suffix==['-ob','displ']:return
    check(len(suffix)==4 and suffix[0]=='--upload','only allowlisted upload/option display permitted')
    try:region=(int(suffix[1],0),int(suffix[2],0))
    except ValueError:raise ValueError('BLOCKED: invalid read range')
    check(region in READS,'read range outside device-ID/size/options/UID/Flash allowlist')
    dest=inside(suffix[3]);check(dest.is_relative_to(inside(backup_dir)),'upload destination outside this backup')
    check(not dest.exists(),'backup output must not overwrite an existing file')
def cube_read(cli,args,directory,name):
    allow_readonly(args,directory)
    run=subprocess.run([str(cli),*args],capture_output=True,timeout=60)
    (directory/(name+'.txt')).write_bytes(run.stdout+b'\n'+run.stderr)
    check(run.returncode==0,'Cube read failed; no reset/unprotect/erase fallback: '+name)
def inventory(cli_path=None):
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    rows=pnp();cli=locate_cli(cli_path)
    debug=[x for x in rows if 'MI_00' in x['InstanceId'] and 'PID_374B' in x['InstanceId']]
    result=dict(action='inventory',devices=rows,cube_cli=str(cli) if cli else None,
                target_identified=False,backup_verified=False,flashed=False,status='BLOCKED')
    if cli and debug and all(x['Status']=='OK' for x in debug):
        cube_read(cli,['-l','stlink'],EVIDENCE,'cube-probe-list');result['status']='HOST_ENUMERATED_ONLY'
    (EVIDENCE/'readonly-inventory.json').write_text(json.dumps(result,indent=2)+'\n')
    return result
def decode_identity(id_bytes,size_bytes,option_bytes):
    check(len(id_bytes)==4 and len(size_bytes)==2 and len(option_bytes)==4,'short identity/options upload')
    idcode=struct.unpack('<I',id_bytes)[0];flash_kib=struct.unpack('<H',size_bytes)[0];options=struct.unpack('<I',option_bytes)[0]
    check(idcode&0xfff==0x413,'unexpected STM32 device family; do not probe another mapping')
    check(flash_kib==1024,'expected VG 1024KiB Flash; no fallback to VE')
    rdp=(options>>8)&255;check(rdp==0xaa,'read protection enabled/unknown; never remove protection')
    return dict(device_id=hex(idcode&0xfff),revision_id=hex(idcode>>16),flash_kib=flash_kib,rdp=rdp,nwrp=(options>>16)&0xfff,optcr=hex(options))
def backup(serial,cli_path=None):
    check(re.fullmatch(r'[A-Fa-f0-9]{24}',serial or '') is not None,'explicit24-digit ST-LINK serial required')
    cli=locate_cli(cli_path);check(cli is not None,'CubeProgrammer/CubeCLT not found')
    rows=pnp();check(any(serial.upper() in x['InstanceId'].upper() for x in rows),'selected probe not present')
    parents=[x for x in rows if re.fullmatch(r'USB\\VID_0483&PID_374B\\[A-Fa-f0-9]{24}',x['InstanceId'],re.I)]
    debug=[x for x in rows if 'MI_00' in x['InstanceId'] and 'PID_374B' in x['InstanceId']]
    check(len(parents)==1 and debug and all(x['Status']=='OK' for x in debug),'single-probe debug interface/driver is not ready; ambiguous probes must be reviewed')
    stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]
    directory=inside(ROOT/'build/discovery/backups'/stamp);directory.mkdir(parents=True)
    record=dict(status='BLOCKED',backup_verified=False,flashed=False,probe_serial=serial,tool=str(cli),tool_sha256=sha(cli),started=stamp)
    conn=['-c','port=SWD','sn='+serial,'mode=HOTPLUG','freq=1000']
    try:
        for name,address,size in [('device-id',0xe0042000,4),('flash-size',0x1fff7a22,2),('options-before',0x40023c14,4)]:
            cube_read(cli,conn+['--upload',hex(address),str(size),str(directory/(name+'.bin'))],directory,name)
        identity=decode_identity((directory/'device-id.bin').read_bytes(),(directory/'flash-size.bin').read_bytes(),(directory/'options-before.bin').read_bytes());record.update(identity)
        cube_read(cli,conn+['-ob','displ'],directory,'option-display')
        cube_read(cli,conn+['--upload','0x1fff7a10','12',str(directory/'uid.bin')],directory,'uid')
        check((directory/'uid.bin').stat().st_size==12,'short UID read')
        for suffix in ['a','b']:
            cube_read(cli,conn+['--upload','0x08000000','1048576',str(directory/('original-flash-'+suffix+'.bin'))],directory,'flash-'+suffix)
        a=directory/'original-flash-a.bin';b=directory/'original-flash-b.bin'
        check(a.stat().st_size==1048576 and b.stat().st_size==1048576 and sha(a)==sha(b),'full two-read backup inconsistent/incomplete')
        cube_read(cli,conn+['--upload','0x40023c14','4',str(directory/'options-after.bin')],directory,'options-after')
        check((directory/'options-before.bin').read_bytes()==(directory/'options-after.bin').read_bytes(),'option register changed during read; stop')
        record.update(status='READ_ONLY_BACKUP_VERIFIED',backup_verified=True,flash_address='0x08000000',backup_path=a.relative_to(ROOT).as_posix(),backup_sha256=sha(a),uid_hex=(directory/'uid.bin').read_bytes().hex(),physical_mcu_marking_verified=False)
    finally:
        record['evidence_files']=[dict(path=p.relative_to(ROOT).as_posix(),size=p.stat().st_size,sha256=sha(p)) for p in sorted(directory.iterdir()) if p.is_file()]
        (directory/'backup-record.json').write_text(json.dumps(record,indent=2)+'\n')
    return record
def contained(base,relative):
    check(not Path(relative).is_absolute(),'absolute package path rejected')
    p=(base/relative).resolve();check(p.is_relative_to(base.resolve()),'package path traversal');return p
def preflight(package=PACKAGE):
    package=Path(package).resolve();m=json_read(package/'SHA256_MANIFEST.json')
    check(m['board']=='STM32F4 Discovery MB997E' and m['mcu']=='STM32F407VGT6','wrong board target')
    check(m['flash_address']=='0x08000000' and m['uart_gate']=='OFF' and m['rf_outputs'] is False,'wrong image policy/address')
    paths=set()
    for item in m['files']:
        check(item['path'] not in paths,'duplicate artifact path');paths.add(item['path']);p=contained(package,item['path'])
        check(p.is_file() and p.stat().st_size==item['size'] and sha(p)==item['sha256'],'missing/tampered artifact: '+item['path'])
    actual={p.relative_to(package).as_posix() for p in package.rglob('*') if p.is_file() and p!=package/'SHA256_MANIFEST.json'}
    check(actual==paths,'package inventory mismatch')
    binary=package/'stm32/tyvor_discovery_bench.bin';data=binary.read_bytes()
    check(TAG in data and 392<=len(data)<=1048576,'not a Discovery bench BIN')
    sp,reset=struct.unpack_from('<II',data);check(sp==0x20020000 and reset&1 and 0x08000000<=reset<0x08000000+len(data),'invalid SP/reset')
    return dict(status='OFFLINE_PREFLIGHT_PASS',board=m['board'],bin=str(binary),bin_sha256=sha(binary),flash_address=m['flash_address'],files_verified=len(paths),flashed=False,hardware_ready=False)
def flash_plan(package,backup_record,review_record,serial,cli_path=None):
    check(re.fullmatch(r'[A-Fa-f0-9]{24}',serial or '') is not None,'explicit24-digit ST-LINK serial required')
    image=preflight(package);b=json_read(backup_record);review=json_read(review_record)
    check(b.get('backup_verified') is True and b.get('probe_serial')==serial,'verified backup for selected probe missing')
    check(b.get('device_id')=='0x413' and b.get('flash_kib')==1024 and b.get('rdp')==0xaa and b.get('nwrp')==0xfff,'target ID/size/protection not suitable')
    backup_file=contained(ROOT,b['backup_path']);check(backup_file.stat().st_size==1048576 and sha(backup_file)==b['backup_sha256'],'backup changed')
    check(bool(b.get('evidence_files')),'backup read evidence missing')
    for item in b['evidence_files']:
        p=contained(ROOT,item['path'])
        check(p.is_file() and p.stat().st_size==item['size'] and sha(p)==item['sha256'],'backup evidence changed')
    directory=Path(backup_record).resolve().parent
    observed=decode_identity((directory/'device-id.bin').read_bytes(),(directory/'flash-size.bin').read_bytes(),(directory/'options-before.bin').read_bytes())
    check(all(b.get(k)==v for k,v in observed.items()),'backup identity record differs from raw reads')
    check((directory/'options-before.bin').read_bytes()==(directory/'options-after.bin').read_bytes(),'backup options inconsistent')
    check(review.get('board_marking')=='MB997E' and review.get('mcu_marking')=='STM32F407VGT6','physical board/MCU marking not confirmed')
    for flag in ['jumpers_verified','power_verified','hse_mco_8mhz_verified','overwrite_original_approved','recovery_plan_reviewed']:
        check(review.get(flag) is True,'missing physical/overwrite review: '+flag)
    check(bool(review.get('reviewer')) and bool(review.get('date')),'reviewer/date missing')
    cli=locate_cli(cli_path);check(cli is not None,'Cube CLI missing')
    # This vector intentionally NEVER reaches cube_read/subprocess. Human review only.
    command=[str(cli),'-c','port=SWD','sn='+serial,'mode=UR','reset=HWrst','freq=1000','-w',image['bin'],'0x08000000','-v']
    return dict(status='PRINTED_PLAN_ONLY',command=command,bin_sha256=image['bin_sha256'],requires_fresh_readonly_identity_before_manual_execution=True,flashed=False)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['inventory','backup','preflight','flash-plan']);p.add_argument('--cli');p.add_argument('--serial');p.add_argument('--package',type=Path,default=PACKAGE);p.add_argument('--backup-record',type=Path);p.add_argument('--review-record',type=Path);a=p.parse_args()
    if a.action=='inventory':result=inventory(a.cli)
    elif a.action=='backup':result=backup(a.serial,a.cli)
    elif a.action=='preflight':result=preflight(a.package)
    else:
        check(a.backup_record is not None and a.review_record is not None,'backup/review records required')
        result=flash_plan(a.package,a.backup_record,a.review_record,a.serial,a.cli)
    print(json.dumps(result,indent=2));return 2 if result['status']=='BLOCKED' else 0
if __name__=='__main__':
    try:sys.exit(main())
    except (ValueError,OSError,KeyError,TypeError,subprocess.SubprocessError) as e:print(str(e),file=sys.stderr);sys.exit(2)
