"""Export real native LVGL renders to PNG + local gallery; no display hardware."""
import hashlib,json,struct,subprocess,zlib
from pathlib import Path
from tft_artifacts import inputs,outputs,native_check
ROOT=Path(__file__).resolve().parent.parent
PPM=ROOT/'build/tft/rendered';OUT=ROOT/'artifacts/tft-ui-experiment'
def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
def png(path,w,h,rgb):
    if len(rgb)!=w*h*3:raise ValueError('RGB size mismatch')
    rows=b''.join(b'\0'+rgb[y*w*3:(y+1)*w*3] for y in range(h))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows,9))+chunk(b'IEND',b''))
def ppm(path):
    magic,geometry,maximum,pixels=path.read_bytes().split(b'\n',3);w,h=map(int,geometry.split())
    if magic!=b'P6' or maximum!=b'255' or len(pixels)!=w*h*3:raise ValueError('Bad PPM')
    return w,h,pixels
def main():
    record=json.loads((ROOT/'build/tft/evidence/native-release-receipt.json').read_text())
    if record['inputs']!=inputs('native') or record['outputs']!=outputs('native','Release') or record['tests']!=native_check('Release'):
        raise ValueError('Native render build/receipt is stale')
    PPM.mkdir(parents=True,exist_ok=True);OUT.mkdir(parents=True,exist_ok=True)
    exe=ROOT/'build/tft/native-release/experiments/tft_spi_320x240/tyvor_tft_render.exe'
    run=subprocess.run([str(exe),str(PPM)],capture_output=True,text=True,check=True)
    rows=[]
    for p in sorted(PPM.glob('*.ppm')):
        w,h,pixels=ppm(p);destination=OUT/(p.stem+'.png');png(destination,w,h,pixels)
        rows.append(dict(file=destination.name,width=w,height=h,sha256=hashlib.sha256(destination.read_bytes()).hexdigest()))
    if len(rows)!=84:raise ValueError('Expected64 UI +20 diagnostic renders')
    for r in [0,90]:
        images=[ppm(PPM/f'ui-r{r}-state{s}-page0.ppm') for s in range(4)];w,h,_=images[0];pad=8;gw=4*w+5*pad;gh=h+2*pad
        canvas=bytearray(bytes([16,24,40])*(gw*gh))
        for i,(_,_,data) in enumerate(images):
            for y in range(h):start=((y+pad)*gw+pad+i*(w+pad))*3;canvas[start:start+w*3]=data[y*w*3:(y+1)*w*3]
        png(OUT/('preview-portrait.png' if r==0 else 'preview-landscape.png'),gw,gh,canvas)
    html='<!doctype html><meta charset="utf-8"><title>TYVOR TFT software rendering</title><style>body{background:#0b1220;color:#e8efff;font:15px system-ui;margin:24px}section{display:flex;flex-wrap:wrap;gap:14px}figure{margin:0;padding:8px;background:#17283e}img{max-width:320px}figcaption{font-size:12px}</style><h1>TYVOR TFT / LVGL software renders</h1><p>2026-11-12. Mock backend only. No panel, touch, GPIO or hardware timing PASS. State0 VALID/SAFE;1 STALE;2 DISCONNECTED;3 BENCH ARMED simulation. Timer SIM-only; aircraft ARM unknown.</p><section>'
    html+=''.join('<figure><img src="'+r['file']+'" alt="'+r['file']+'"><figcaption>'+r['file']+'</figcaption></figure>' for r in rows)+'</section>'
    (OUT/'index.html').write_text(html,encoding='utf-8')
    result=dict(render_engine='LVGL9.2.2 software rasterizer',render_exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),images=rows,
                count=len(rows),hardware_tested=False,timing='deterministic mock cost, not hardware measurement',stdout=run.stdout)
    (ROOT/'build/tft/evidence/render-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(run.stdout.strip());print('PNG gallery:',OUT)
if __name__=='__main__':main()
