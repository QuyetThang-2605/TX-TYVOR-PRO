"""Actual LVGL screenshot export/comparison. No invented screenshot layouts."""
import hashlib,json,subprocess,sys,shutil,importlib.util
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('png_tools',ROOT/'scripts/export-tft-preview.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
png,ppm=module.png,module.ppm
OUT=ROOT/'artifacts/tyvor-ui-v2';PPM=ROOT/'build/ui-v2/rendered'
def sheet(paths,out,columns=4,pad=16):
    data=[ppm(p) for p in paths];cw=max(x[0] for x in data);ch=max(x[1] for x in data);rows=(len(data)+columns-1)//columns
    w=columns*cw+(columns+1)*pad;h=rows*ch+(rows+1)*pad;canvas=bytearray(bytes([11,18,32])*(w*h))
    for i,(iw,ih,pixels) in enumerate(data):
        x=pad+(i%columns)*(cw+pad);y=pad+(i//columns)*(ch+pad)
        for yy in range(ih):start=((y+yy)*w+x)*3;canvas[start:start+iw*3]=pixels[yy*iw*3:(yy+1)*iw*3]
    png(out,w,h,canvas)
def main():
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--debug',action='store_true');a=p.parse_args();config='debug' if a.debug else 'release'
    from ui2_artifacts import verify_native
    verify_native(config.title())
    exe=ROOT/f'build/ui-v2/native-{config}/experiments/tyvor_ui_v2/tyvor_ui2_render.exe'
    OUT.mkdir(parents=True,exist_ok=True);PPM.mkdir(parents=True,exist_ok=True)
    r=subprocess.run([str(exe),str(PPM)],capture_output=True,text=True)
    (ROOT/'build/ui-v2/evidence/render-console.txt').write_text(r.stdout+r.stderr)
    if r.returncode:print(r.stdout+r.stderr,file=sys.stderr);sys.exit(r.returncode)
    records=[]
    for file in sorted(PPM.glob('*.ppm')):
        w,h,pixels=ppm(file);png(OUT/(file.stem+'.png'),w,h,pixels)
        records.append(dict(path=file.stem+'.png',width=w,height=h,sha256=hashlib.sha256((OUT/(file.stem+'.png')).read_bytes()).hexdigest()))
    if len(records)!=142:raise ValueError('Expected 130 primary/detail +12 edge screenshots at both profiles')
    for w,h in [(240,320),(368,448)]:
        shutil.copy2(OUT/f'ui-{w}x{h}-state3-page0.png',OUT/f'home-{w}x{h}.png')
        sheet([PPM/f'ui-{w}x{h}-state3-page{page}.ppm' for page in range(7)],OUT/f'pages-{w}x{h}.png')
        sheet([PPM/f'ui-{w}x{h}-state{state}-page0.ppm' for state in range(5)],OUT/f'states-{w}x{h}.png',columns=5)
    sheet([PPM/f'ui-{w}x{h}-state3-page{page}.ppm' for w,h in [(240,320),(368,448)] for page in range(7)],OUT/'all-pages-contact-sheet.png',columns=7)
    old=ROOT/'build/tft/rendered/ui-r0-state0-page0.ppm'
    if not old.exists():raise ValueError('Frozen real before screenshot missing')
    sheet([old,PPM/'ui-240x320-state3-page0.ppm',PPM/'ui-368x448-state3-page0.ppm'],OUT/'before-after-comparison.png',columns=3)
    # Include a screenshot from the actual hidden Win32 host after mouse/keyboard/swipe checks.
    host=exe.parent/'tyvor_ui2_simulator.exe';capture=PPM/'host-home.ppm'
    host_result=subprocess.run([str(host),'--smoke','--capture',str(capture)],capture_output=True,text=True,check=True)
    png(OUT/'simulator-preview.png',*ppm(capture))
    capture.unlink() # only our generated single capture, not renderer inventory
    body=''.join(f'<figure><img src="{r["path"]}" alt="{r["path"]}"><figcaption>{r["path"]}</figcaption></figure>' for r in records)
    html='''<!doctype html><meta charset="utf-8"><title>TYVOR UI V2 • LVGL review</title><style>body{background:#0B1220;color:#F1F5F9;font:15px system-ui;margin:24px}section{display:flex;flex-wrap:wrap;gap:16px}figure{margin:0;padding:12px;background:#111C2D;border-radius:8px}img{max-width:368px}figcaption{color:#94A3B8;font-size:12px;margin-top:8px}a{color:#38BDF8}</style><h1>TYVOR Tactical Dark / UI V2</h1><p>Actual LVGL 9.2.2 RGB565 renders. D = DEMO. State0 VALID bench-arm;1 STALE;2 DISCONNECTED;3 SAFE;4 INVALID MODEL. No hardware tested.</p><p><a href="all-pages-contact-sheet.png">All pages</a> · <a href="before-after-comparison.png">Before / after</a> · <a href="RUN_SIMULATOR.md">PC simulator</a></p><section>'''+body+'</section>'
    (OUT/'index.html').write_text(html,encoding='utf-8')
    (ROOT/'build/ui-v2/evidence/render-validation.json').write_text(json.dumps(dict(engine='LVGL9.2.2 RGB565 software rasterizer',count=len(records),layout_errors=0,hardware_tested=False,images=records,renderer_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),renderer_stdout=r.stdout,win32_stdout=host_result.stdout),indent=2)+'\n')
    print(r.stdout.strip());print(host_result.stdout.strip());print(OUT)
if __name__=='__main__':main()
