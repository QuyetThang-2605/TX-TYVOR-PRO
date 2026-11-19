"""Pin original Montserrat outlines and OFL; no font installation or SDK edits."""
import hashlib,json,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent;DEST=ROOT/'experiments/tyvor_ui_v3_2_1/assets/font'
def get(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'TYVOR-UI-font-source'}),timeout=30) as r:return r.read()
lock=DEST/'SOURCES.json'
if lock.exists():
    for r in json.loads(lock.read_text())['files']:assert hashlib.sha256((DEST/r['name']).read_bytes()).hexdigest()==r['sha256']
    print('PASS pinned font sources')
else:
    DEST.mkdir(parents=True,exist_ok=True)
    commit=json.loads(get('https://api.github.com/repos/JulietaUla/Montserrat/commits/master'))['sha'];rows=[]
    for name,path in [('Montserrat-SemiBold.ttf','fonts/ttf/Montserrat-SemiBold.ttf'),('Montserrat-Bold.ttf','fonts/ttf/Montserrat-Bold.ttf'),('OFL.txt','OFL.txt')]:
        url='https://raw.githubusercontent.com/JulietaUla/Montserrat/'+commit+'/'+path;data=get(url);(DEST/name).write_bytes(data);rows.append(dict(name=name,url=url,sha256=hashlib.sha256(data).hexdigest()))
    lock.write_text(json.dumps(dict(repository='https://github.com/JulietaUla/Montserrat',commit=commit,files=rows),indent=2)+'\n');print('Pinned original font',commit)
