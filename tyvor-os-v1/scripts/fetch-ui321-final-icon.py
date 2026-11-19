"""Add the approved-like flat Bind waveform from the already-pinned Lucide commit."""
import hashlib,json,sys,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent;folder=ROOT/'experiments/tyvor_ui_v3_2_1/assets/lucide';lock=folder/'SOURCES.json';record=json.loads(lock.read_text());name='audio-lines.svg'
existing=next((r for r in record['files'] if r['name']==name),None)
if existing:
    assert hashlib.sha256((folder/name).read_bytes()).hexdigest()==existing['sha256'];print('PASS pinned Bind icon')
else:
    if '--verify-only' in sys.argv:raise ValueError('Missing pinned Bind icon')
    url='https://raw.githubusercontent.com/lucide-icons/lucide/'+record['commit']+'/icons/'+name
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'TYVOR-UI-icon-source'}),timeout=30) as response:data=response.read()
    assert b'<svg' in data and b'<script' not in data
    (folder/name).write_bytes(data);record['files'].append(dict(name=name,url=url,sha256=hashlib.sha256(data).hexdigest()));lock.write_text(json.dumps(record,indent=2)+'\n');print('Pinned original Lucide audio-lines SVG')
