"""Actual LVGL export, with device pixels kept1:1 in every final comparison."""
import argparse,hashlib,json,shutil,subprocess,time
from pathlib import Path
from ui321_final_images import ROOT,png,ppm,sheet,crop,native_pair,read_png
BUILD=ROOT/'build/ui-v3-2-1-final';EVIDENCE=BUILD/'evidence';PPM=BUILD/'rendered';OUT=ROOT/'artifacts/tyvor-ui-v3-2-1-final'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--draft',action='store_true');args=parser.parse_args();config='debug' if args.draft else 'release';out=BUILD/'draft' if args.draft else OUT
    if not args.draft:
        from ui321_final_artifacts import verify_native
        verify_native('Release')
    out.mkdir(parents=True,exist_ok=True);PPM.mkdir(parents=True,exist_ok=True)
    exe=BUILD/f'native-{config}/experiments/tyvor_ui_v3_2_1/tyvor_ui321_render.exe';started=time.perf_counter();r=subprocess.run([str(exe),str(PPM)],capture_output=True,text=True,check=True);elapsed=time.perf_counter()-started
    (EVIDENCE/'render-console.txt').write_text(r.stdout+r.stderr);images=[]
    for p in sorted(PPM.glob('*.ppm')):
        im=ppm(p);target=out/(p.stem+'.png');png(target,*im);assert read_png(target)==im
        images.append(dict(path=target.name,width=im[0],height=im[1],sha256=sha(target)))
    if len(images)!=154:raise ValueError('Expected152 RGB565 screens plus2 real RGB888 captures')
    approved=[];raw_approved=[];before_after=[];rgb_pairs=[];details=[];methods=[];row_y=14
    for w,h,panel in [(320,240,'approved-middle'),(448,368,'approved-right')]:
        actual=ppm(PPM/f'ui-{w}x{h}-state3-page0.ppm');raw=ppm(PPM/f'home-rgb888-{w}x{h}.ppm');ref=ppm(EVIDENCE/f'reference/{panel}.ppm')
        before=ppm(ROOT/f'build/ui-v3-2-1/rendered/ui-{w}x{h}-state3-page0.ppm')
        approved+=native_pair(ref,actual);raw_approved+=native_pair(ref,raw);before_after+=[before,actual];rgb_pairs+=[raw,actual]
        assert approved[-1]==actual and raw_approved[-1]==raw
        shutil.copy2(out/f'ui-{w}x{h}-state3-page0.png',out/f'home-{w}x{h}.png')
        shutil.copy2(out/f'home-{w}x{h}.png',out/f'home-rgb565-{w}x{h}.png')
        shutil.copy2(ROOT/f'artifacts/tyvor-ui-v3-2-1/home-{w}x{h}.png',out/f'before-home-{w}x{h}.png')
        sheet([ppm(PPM/f'ui-{w}x{h}-state{s}-page0.ppm') for s in range(5)],out/f'states-{w}x{h}.png',5)
        sheet([ppm(PPM/f'ui-{w}x{h}-state3-page{p}.ppm') for p in range(9)],out/f'pages-{w}x{h}.png',3)
        timer=json.loads((PPM/f'home-geometry-{w}x{h}.json').read_text())['timer'];logo=[10,6 if w==448 else 5,112 if w==448 else 76,23 if w==448 else 16]
        details += [crop(before,timer),crop(actual,timer),crop(before,logo),crop(actual,logo)]
        assert crop(before,logo)==crop(actual,logo),'Logo pixels must remain untouched'
        methods.append(dict(profile=[w,h],reference_size=list(ref[:2]),actual_size=list(actual[:2]),actual_scale=1.0,actual_rect=[28+w,row_y,w,h],reference_rect=[14,row_y,w,h],actual_panel_pixels_identical=True,reference_policy='Uniform bilinear downsample or identity; pad; never upscale',logo_before_after_exact=True))
        row_y+=h+14
    sheet(approved,out/'approved-vs-final-lvgl.png',2);sheet(raw_approved,out/'approved-vs-final-rgb888.png',2)
    sheet(before_after,out/'before-vs-after.png',2);sheet(rgb_pairs,out/'rgb888-vs-rgb565.png',2);sheet(details,out/'timer-logo-native-detail.png',4)
    sheet([ppm(PPM/f'ui-{w}x{h}-state3-page{p}.ppm') for w,h in [(320,240),(448,368)] for p in range(9)],out/'all-pages-contact-sheet.png',3)
    (EVIDENCE/'comparison-method.json').write_text(json.dumps(dict(actual_never_resized=True,reference_and_actual_use_same_native_pair_function=True,rows=methods,native_png_filter='none; lossless byte-identical PPM conversion',visual_acceptance='PENDING_USER_REVIEW'),indent=2)+'\n')
    host=exe.parent/'tyvor_ui321_simulator.exe';capture=BUILD/'host-capture.ppm';smoke=subprocess.run([str(host),'--smoke','--capture',str(capture)],capture_output=True,text=True,check=True);png(out/'simulator-preview.png',*ppm(capture))
    metadata=dict(engine='LVGL9.2.2 RGB888 before pack and RGB565 after pack',count=152,raw_rgb888=2,total_images=154,draft=args.draft,layout_errors=0,renderer_sha256=sha(exe),images=images,renderer_stdout=r.stdout,win32_stdout=smoke.stdout,native_renderer_wall_seconds=round(elapsed,6),hardware_tested=False)
    (EVIDENCE/'render-validation.json').write_text(json.dumps(metadata,indent=2)+'\n')
    html='<!doctype html><meta charset="utf-8"><title>TYVOR V3.2.1 final correction</title><style>body{background:#031b48;color:white;font:15px system-ui}img{display:block;width:auto;max-width:none;margin:12px}a{color:#9edcff}</style><h1>V3.2.1 correction / native pixels</h1><p>Actual LVGL. No actual-side resize. Approved source is downsampled to each native viewport. Visual acceptance pending; hardware unverified.</p><p><a href="approved-vs-final-lvgl.png">Approved / final</a> | <a href="before-vs-after.png">Before / after</a> | <a href="rgb888-vs-rgb565.png">RGB888 / RGB565</a> | <a href="BLUR_CAUSE.md">Blur investigation</a> | <a href="RUN_SIMULATOR.md">Simulator</a></p>'
    html+=''.join(f'<p>{name}</p><img src="{name}" width="{w}" height="{h}">' for w,h in [(320,240),(448,368)] for name in [f'home-rgb888-{w}x{h}.png',f'home-rgb565-{w}x{h}.png']);(out/'index.html').write_text(html,encoding='utf-8')
    print(r.stdout.strip());print(smoke.stdout.strip());print(out)
if __name__=='__main__':main()
