"""Fetch/check locked TFT dependencies inside .tools. No global installation/device I/O."""
import argparse,hashlib,json,tarfile,urllib.request,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def checked(p):
    p=p.resolve()
    if not p.is_relative_to(ROOT/'.tools'):raise ValueError('Dependency path outside workspace .tools')
    return p
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--verify-only',action='store_true');args=p.parse_args()
    lock=json.loads((ROOT/'scripts/tft-dependencies.lock.json').read_text())
    if not args.verify_only:
        for item in lock['downloads']:
            dst=checked(ROOT/item['path']);dst.parent.mkdir(parents=True,exist_ok=True)
            if not dst.exists():
                with urllib.request.urlopen(urllib.request.Request(item['url'],headers={'User-Agent':'TYVOR-TFT-setup'}),timeout=45) as src,dst.open('xb') as out:
                    while chunk:=src.read(1024*1024):out.write(chunk)
            if dst.stat().st_size!=item['bytes'] or sha(dst)!=item['sha256']:raise ValueError('Download hash mismatch: '+item['path'])
        destination=checked(ROOT/'.tools/sdk/lvgl-9.2.2');destination.mkdir(parents=True,exist_ok=True)
        expected={x['path']:x['sha256'] for x in lock['files']}
        with tarfile.open(checked(ROOT/'.tools/downloads/lvgl-9.2.2.tar.gz')) as archive:
            for member in archive.getmembers():
                relative=Path(*Path(member.name).parts[1:]);dst=checked(destination/relative)
                if dst.relative_to(ROOT).as_posix() not in expected:continue
                if not member.isfile():raise ValueError('Unexpected archive member type')
                if not dst.exists():dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(archive.extractfile(member).read())
    for item in lock['files']:
        file=checked(ROOT/item['path'])
        if not file.is_file() or sha(file)!=item['sha256']:raise ValueError('Dependency source mismatch: '+item['path'])
    print('PASS:',len(lock['files']),'locked sources; LVGL'+lock['lvgl_version']+' /ILI9341 '+lock['ili9341_version'])
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,tarfile.TarError) as e:print('TFT dependency setup FAIL:',e,file=sys.stderr);sys.exit(1)
