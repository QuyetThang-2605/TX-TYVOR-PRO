"""Export actual LVGL screens and geometry comparisons, never substitute mock UI.
Reference artwork appears only in labelled review comparisons, never firmware.
"""
import argparse,hashlib,importlib.util,json,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('png_tools',ROOT/'scripts/export-tft-preview.py');png_tools=importlib.util.module_from_spec(spec);spec.loader.exec_module(png_tools)
png,ppm=png_tools.png,png_tools.ppm
BUILD=ROOT/'build/ui-v3';EVIDENCE=BUILD/'evidence';PPM=BUILD/'rendered';OUT=ROOT/'artifacts/tyvor-ui-v3-oxbot'
# Hand-measured estimates from official 400x315 poster, not user screen captures.
REFERENCE={"hero":[17,47,365,154],"find":[30,90,65,62],"bind":[308,90,65,62],"timer":[110,88,181,68],"safety":[133,47,135,29],"stick_left":[17,211,90,92],"battery":[111,211,88,92],"mode":[205,211,78,92],"stick_right":[287,211,95,92]}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def resize(image,w,h):
    iw,ih,rgb=image;out=bytearray(w*h*3)
    for y in range(h):
        sy=y*ih//h
        for x in range(w):sx=x*iw//w;out[(y*w+x)*3:(y*w+x)*3+3]=rgb[(sy*iw+sx)*3:(sy*iw+sx)*3+3]
    return w,h,bytes(out)
def sheet(images,path,columns=4,pad=14):
    rows=[images[i:i+columns] for i in range(0,len(images),columns)];w=max(sum(im[0] for im in row)+(len(row)+1)*pad for row in rows);h=sum(max(im[1] for im in row)+pad for row in rows)+pad
    canvas=bytearray(bytes([23,43,59])*(w*h));y=pad
    for row in rows:
        x=pad
        for iw,ih,pixels in row:
            for yy in range(ih):start=((y+yy)*w+x)*3;canvas[start:start+iw*3]=pixels[yy*iw*3:(yy+1)*iw*3]
            x+=iw+pad
        y+=max(im[1] for im in row)+pad
    png(path,w,h,canvas)
def outline(buf,w,h,r,color):
    x,y,rw,rh=[int(round(v)) for v in r]
    for xx in range(max(0,x),min(w,x+rw)):
        for yy in [y,y+rh-1]:
            if 0<=yy<h:buf[(yy*w+xx)*3:(yy*w+xx)*3+3]=bytes(color)
    for yy in range(max(0,y),min(h,y+rh)):
        for xx in [x,x+rw-1]:
            if 0<=xx<w:buf[(yy*w+xx)*3:(yy*w+xx)*3+3]=bytes(color)
def compare(out):
    reference=ppm(EVIDENCE/'reference/official-home.ppm');results=[]
    for w,h in [(320,240),(448,368)]:
        im=ppm(PPM/f'ui-{w}x{h}-state3-page0.ppm');ref=resize(reference,w,h);overlay=bytearray((a+b)//2 for a,b in zip(im[2],ref[2]));geometry=json.loads((PPM/f'home-geometry-{w}x{h}.json').read_text());rows=[]
        for name,r in REFERENCE.items():
            rx,ry,rw,rh=r;target=[rx*w/400,ry*h/315,rw*w/400,rh*h/315];actual=geometry[name]
            outline(overlay,w,h,target,[255,90,90]);outline(overlay,w,h,actual,[80,240,255]);x,y,aw,ah=actual;tx,ty,tw,th=target
            overlap=max(0,min(x+aw,tx+tw)-max(x,tx))*max(0,min(y+ah,ty+th)-max(y,ty));iou=overlap/(aw*ah+tw*th-overlap)
            rows.append(dict(region=name,reference_estimate=target,actual_lvgl=actual,geometry_iou=round(iou,4),center_distance_px=round(((x+aw/2-tx-tw/2)**2+(y+ah/2-ty-th/2)**2)**0.5,2),
                             caveat='reference estimated glyph region vs actual label allocation; not equivalent box types' if name=='timer' else 'hand-estimated reference panel/button bounds'))
        png(out/f'home-reference-overlay-{w}x{h}.png',w,h,overlay)
        results.append(dict(profile=[w,h],regions=rows,raw_rgb_mae=round(sum(abs(a-b) for a,b in zip(im[2],ref[2]))/len(im[2]),3)))
    sheet([reference,resize(ppm(PPM/'ui-320x240-state3-page0.ppm'),400,315),resize(ppm(PPM/'ui-448x368-state3-page0.ppm'),400,315)],out/'home-reference-comparison.png',3)
    (EVIDENCE/'reference-comparison.json').write_text(json.dumps(dict(reference_resolution=[400,315],source='official manual still poster',reference_sha256=sha(EVIDENCE/'reference/official-home.png'),
        measurement='hand-estimated region bounds; geometry-only metrics; not perceptual similarity or pixel-perfect proof',comparisons=results),indent=2)+'\n')
    return results
def main():
    p=argparse.ArgumentParser();p.add_argument('--draft',action='store_true');a=p.parse_args();config='debug' if a.draft else 'release';out=BUILD/'draft' if a.draft else OUT;out.mkdir(parents=True,exist_ok=True);PPM.mkdir(parents=True,exist_ok=True)
    if not a.draft:
        from ui3_artifacts import verify_native
        verify_native('Release')
    exe=BUILD/f'native-{config}/experiments/tyvor_ui_v3/tyvor_ui3_render.exe';run=subprocess.run([str(exe),str(PPM)],capture_output=True,text=True);(EVIDENCE/'render-console.txt').write_text(run.stdout+run.stderr)
    rows=[]
    for file in sorted(PPM.glob('*.ppm')):
        w,h,pixels=ppm(file);png(out/(file.stem+'.png'),w,h,pixels);rows.append(dict(path=file.stem+'.png',width=w,height=h,sha256=sha(out/(file.stem+'.png'))))
    if len(rows)!=152:raise ValueError('Expected90 primary +50 detail +12 edge LVGL screenshots')
    for w,h in [(320,240),(448,368)]:
        shutil.copy2(out/f'ui-{w}x{h}-state3-page0.png',out/f'home-{w}x{h}.png')
        sheet([ppm(PPM/f'ui-{w}x{h}-state3-page{page}.ppm') for page in range(9)],out/f'pages-{w}x{h}.png',3)
        sheet([ppm(PPM/f'ui-{w}x{h}-state{s}-page0.ppm') for s in range(5)],out/f'states-{w}x{h}.png',5)
    sheet([ppm(PPM/f'ui-{w}x{h}-state3-page{page}.ppm') for w,h in [(320,240),(448,368)] for page in range(9)],out/'all-pages-contact-sheet.png',3)
    sheet([ppm(ROOT/'build/ui-v2/rendered/ui-240x320-state3-page0.ppm'),ppm(PPM/'ui-320x240-state3-page0.ppm'),ppm(PPM/'ui-448x368-state3-page0.ppm')],out/'before-v2-after-v3.png',3)
    compare(out)
    if run.returncode:print(run.stdout+run.stderr,file=sys.stderr);sys.exit(run.returncode)
    host=exe.parent/'tyvor_ui3_simulator.exe';capture=BUILD/'host-capture.ppm';result=subprocess.run([str(host),'--smoke','--capture',str(capture)],capture_output=True,text=True,check=True);png(out/'simulator-preview.png',*ppm(capture))
    html='''<!doctype html><meta charset="utf-8"><title>TYVOR V3 • landscape LVGL review</title><style>body{background:#23394c;color:#e2ecf1;font:15px system-ui;margin:24px}section{display:flex;flex-wrap:wrap;gap:14px}figure{margin:0;padding:12px;background:#385b6b;border-radius:14px}img{max-width:448px}figcaption{font-size:12px}a{color:#78cdea}</style><h1>TYVOR V3 / LANDSCAPE</h1><p>Actual LVGL9.2.2. HOME geometry reference: official400x315 poster. User folder contains TYVOR logos, no Oxbot UI captures. Other pages are derived, not directly matched. DEMO only; hardware UNVERIFIED.</p><p><a href="home-reference-comparison.png">Reference / TYVOR320 / TYVOR448</a> · <a href="home-reference-overlay-320x240.png">Overlay320</a> · <a href="all-pages-contact-sheet.png">All pages</a> · <a href="RUN_SIMULATOR.md">PC simulator</a></p><section>'''
    html+=''.join(f'<figure><img src="{r["path"]}" alt="{r["path"]}"><figcaption>{r["path"]}</figcaption></figure>' for r in rows)+'</section>';(out/'index.html').write_text(html,encoding='utf-8')
    (EVIDENCE/'render-validation.json').write_text(json.dumps(dict(engine='LVGL9.2.2 actual RGB565 rasterizer',count=152,draft=a.draft,layout_errors=0,renderer_sha256=sha(exe),images=rows,renderer_stdout=run.stdout,win32_stdout=result.stdout,hardware_tested=False),indent=2)+'\n')
    print(run.stdout.strip());print(result.stdout.strip());print(out)
if __name__=='__main__':main()
