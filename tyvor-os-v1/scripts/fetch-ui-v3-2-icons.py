"""Vendor only the required Lucide SVGs and complete license, pinned by commit/hash."""
import argparse,hashlib,json,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
DEST=ROOT/'experiments/tyvor_ui_v3_2/assets/lucide'
NAMES=['map-pin','signal','battery','link','settings','box','joystick','activity','arrow-left','house','sliders-horizontal','circle-question-mark','ellipsis','chart-no-axes-column','radio']
def get(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'TYVOR-UI-source-vendor'}),timeout=30) as r:return r.read()
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--verify-only',action='store_true');a=parser.parse_args()
    lock=DEST/'SOURCES.json'
    if lock.exists():
        record=json.loads(lock.read_text())
        for r in record['files']:
            p=DEST/r['name']
            if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=r['sha256']:raise ValueError('Lucide asset mismatch: '+str(p))
        print('PASS Lucide pinned SVG/license hashes',len(record['files']));return
    if a.verify_only:raise ValueError('Missing vendored Lucide source lock')
    DEST.mkdir(parents=True,exist_ok=True);pending=DEST/'DOWNLOAD_PENDING.json'
    commit=json.loads(pending.read_text())['commit'] if pending.exists() else json.loads(get('https://api.github.com/repos/lucide-icons/lucide/commits/main'))['sha']
    pending.write_text(json.dumps(dict(commit=commit))+'\n');files=[]
    for name in ['LICENSE']+[n+'.svg' for n in NAMES]:
        url='https://raw.githubusercontent.com/lucide-icons/lucide/'+commit+'/'+('' if name=='LICENSE' else 'icons/')+name
        data=get(url);(DEST/name).write_bytes(data)
        files.append(dict(name=name,url=url,sha256=hashlib.sha256(data).hexdigest()))
    lock.write_text(json.dumps(dict(repository='https://github.com/lucide-icons/lucide',commit=commit,license='ISC; Feather MIT portions retained in LICENSE',files=files),indent=2)+'\n')
    pending.unlink()
    print('Vendored Lucide commit',commit,len(files),'files')
if __name__=='__main__':main()
