"""Flatten pinned Lucide SVG strokes into LVGL line segments; no runtime SVG heap."""
import hashlib,json,math,re,xml.etree.ElementTree as ET
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent;DEST=ROOT/'experiments/tyvor_ui_v3_2';SOURCE=DEST/'assets/lucide'
NAMES=['map-pin','radio','house','box','joystick','sliders-horizontal','signal','activity','arrow-left','ellipsis','settings','battery','link','sliders-horizontal','circle-question-mark']
def arc(start,rx,ry,angle,large,sweep,end):
    if start==end:return []
    if rx==0 or ry==0:return [end]
    rx,ry=abs(rx),abs(ry);phi=math.radians(angle);co,si=math.cos(phi),math.sin(phi)
    dx,dy=(start[0]-end[0])/2,(start[1]-end[1])/2;xp,yp=co*dx+si*dy,-si*dx+co*dy
    scale=max(1,math.sqrt(xp*xp/(rx*rx)+yp*yp/(ry*ry)));rx*=scale;ry*=scale
    factor=math.sqrt(max(0,(rx*rx*ry*ry-rx*rx*yp*yp-ry*ry*xp*xp)/(rx*rx*yp*yp+ry*ry*xp*xp)))
    if bool(large)==bool(sweep):factor=-factor
    cxp,cyp=factor*rx*yp/ry,-factor*ry*xp/rx
    cx=co*cxp-si*cyp+(start[0]+end[0])/2;cy=si*cxp+co*cyp+(start[1]+end[1])/2
    t0=math.atan2((yp-cyp)/ry,(xp-cxp)/rx);t1=math.atan2((-yp-cyp)/ry,(-xp-cxp)/rx);delta=(t1-t0)%(2*math.pi)
    if not sweep:delta-=2*math.pi
    steps=max(2,math.ceil(abs(delta)*max(rx,ry)/1.2))
    return [(cx+co*rx*math.cos(t0+delta*i/steps)-si*ry*math.sin(t0+delta*i/steps),cy+si*rx*math.cos(t0+delta*i/steps)+co*ry*math.sin(t0+delta*i/steps)) for i in range(1,steps+1)]
def paths(d):
    tokens=re.findall(r'[A-Za-z]|[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?',d);i=0;p=(0,0);start=p;lines=[];command=None
    def line(q):
        nonlocal p
        if q!=p:lines.append((*p,*q))
        p=q
    while i<len(tokens):
        if tokens[i].isalpha():command=tokens[i];i+=1
        c=command.upper();relative=command.islower()
        if c=='Z':line(start);command=None;continue
        if c=='A':
            v=list(map(float,tokens[i:i+3]));i+=3
            for flag in range(2):
                token=tokens[i];assert token[0] in '01'
                v.append(float(token[0]))
                if len(token)>1:tokens[i]=token[1:]
                else:i+=1
            v+=list(map(float,tokens[i:i+2]));i+=2
        else:
            count={'M':2,'L':2,'H':1,'V':1,'C':6}[c];v=list(map(float,tokens[i:i+count]));i+=count
        def point(x,y):return (x+p[0],y+p[1]) if relative else (x,y)
        if c=='M':p=point(*v);start=p;command='l' if relative else 'L'
        elif c=='L':line(point(*v))
        elif c=='H':line((v[0]+p[0] if relative else v[0],p[1]))
        elif c=='V':line((p[0],v[0]+p[1] if relative else v[0]))
        elif c=='C':
            a=p;b=point(*v[:2]);cc=point(*v[2:4]);end=point(*v[4:]);steps=16
            for n in range(1,steps+1):
                t=n/steps;u=1-t;line(tuple(u*u*u*a[k]+3*u*u*t*b[k]+3*u*t*t*cc[k]+t*t*t*end[k] for k in range(2)))
        elif c=='A':
            for q in arc(p,*v[:5],point(*v[5:])):line(q)
    return lines
def flatten(path):
    result=[]
    for e in ET.fromstring(path.read_text()):
        tag=e.tag.split('}')[-1]
        if tag=='path':result+=paths(e.attrib['d'])
        elif tag=='circle':
            cx,cy,r=[float(e.attrib[k]) for k in ['cx','cy','r']];points=[(cx+r*math.cos(i*math.pi/24),cy+r*math.sin(i*math.pi/24)) for i in range(49)]
            result += [(*a,*b) for a,b in zip(points,points[1:])]
        elif tag=='rect':
            x,y,w,h=[float(e.get(k,'0')) for k in ['x','y','width','height']];r=float(e.get('rx','0'))
            result+=paths(f'M{x+r} {y} H{x+w-r} A{r} {r} 0 0 1 {x+w} {y+r} V{y+h-r} A{r} {r} 0 0 1 {x+w-r} {y+h} H{x+r} A{r} {r} 0 0 1 {x} {y+h-r} V{y+r} A{r} {r} 0 0 1 {x+r} {y} Z')
        else:raise ValueError('Unsupported SVG element '+tag)
    return [tuple(round(v*256) for v in segment) for segment in result]
lock=json.loads((SOURCE/'SOURCES.json').read_text())
for r in lock['files']:assert hashlib.sha256((SOURCE/r['name']).read_bytes()).hexdigest()==r['sha256']
def raster(segments,size,stroke=2):
    # Four-by-four supersampled union of round-capped SVG strokes; immutable A8.
    scale=size*4/24;side=size*4;radius=scale*stroke/2;pixels=bytearray(side*side)
    for values in segments:
        ax,ay,bx,by=[v/256*scale for v in values];dx,dy=bx-ax,by-ay;length=dx*dx+dy*dy
        for y in range(max(0,int(min(ay,by)-radius)),min(side,math.ceil(max(ay,by)+radius))):
            for x in range(max(0,int(min(ax,bx)-radius)),min(side,math.ceil(max(ax,bx)+radius))):
                xx,yy=x+0.5,y+0.5;t=max(0,min(1,((xx-ax)*dx+(yy-ay)*dy)/length)) if length else 0
                if (xx-ax-t*dx)**2+(yy-ay-t*dy)**2<=radius*radius:pixels[y*side+x]=1
    return bytes(round(sum(pixels[(y*4+j)*side+x*4+i] for i in range(4) for j in range(4))*255/16) for y in range(size) for x in range(size))
c=['/* Derived from Lucide ISC/Feather MIT. Full notices: assets/lucide/LICENSE. */','#include "icon_data.h"'];rows=[]
for index,name in enumerate(NAMES):
    segments=flatten(SOURCE/(name+'.svg'));assert segments
    assert all(-256<=v<=6400 for s in segments for v in s)
    c+=['static const int16_t icon_'+str(index)+'[][4]={',*['  {'+','.join(map(str,s))+'},' for s in segments],'};']
    for slot,size in enumerate([20,28,34]):
        raw=raster(segments,size);name=f'icon_{index}_a{slot}'
        c+=['static const uint8_t '+name+'_pixels[]={',*['  '+','.join(map(str,raw[i:i+32]))+',' for i in range(0,len(raw),32)],'};',
            'static const lv_image_dsc_t '+name+'={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A8,.w='+str(size)+',.h='+str(size)+',.stride='+str(size)+'},.data_size='+str(len(raw))+',.data='+name+'_pixels};']
    rows.append(dict(name=name,segments=len(segments)))
c+=['const tyvor_ui32_icon_data_t tyvor_ui32_icon_data[]={',*[f'  {{icon_{i},{r["segments"]},{{&icon_{i}_a0,&icon_{i}_a1,&icon_{i}_a2}}}},' for i,r in enumerate(rows)],'};']
for diameter in [58,80,88,94,130]:
    size=diameter+2;radius=diameter/2;count=60 if diameter>80 else 40;segments=[]
    for i in range(count):
        angle=i*2*math.pi/count;inner=radius-(6 if i%(count//4)==0 else 3)
        segments.append(tuple(round(v*24/size*256) for v in [size/2+math.cos(angle)*inner,size/2+math.sin(angle)*inner,size/2+math.cos(angle)*radius,size/2+math.sin(angle)*radius]))
    raw=raster(segments,size,24/size);name='tyvor_ui32_ring_'+str(diameter)
    c+=['static const uint8_t '+name+'_pixels[]={',*['  '+','.join(map(str,raw[i:i+32]))+',' for i in range(0,len(raw),32)],'};',
         'const lv_image_dsc_t '+name+'={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A8,.w='+str(size)+',.h='+str(size)+',.stride='+str(size)+'},.data_size='+str(len(raw))+',.data='+name+'_pixels};']
(DEST/'icon_data.c').write_text('\n'.join(c)+'\n')
(DEST/'icon_data.h').write_text('#ifndef TYVOR32_ICON_DATA_H\n#define TYVOR32_ICON_DATA_H\n#include <stdint.h>\n#include "lvgl.h"\ntypedef struct {const int16_t (*segments)[4];unsigned count;const lv_image_dsc_t *images[3];} tyvor_ui32_icon_data_t;\nextern const tyvor_ui32_icon_data_t tyvor_ui32_icon_data[15];\nextern const lv_image_dsc_t tyvor_ui32_ring_58,tyvor_ui32_ring_80,tyvor_ui32_ring_88,tyvor_ui32_ring_94,tyvor_ui32_ring_130;\n#endif\n')
e=ROOT/'build/ui-v3-2/evidence';e.mkdir(parents=True,exist_ok=True)
(e/'icon-generation.json').write_text(json.dumps(dict(source_commit=lock['commit'],icons=rows,stroke_width=2,viewbox=[24,24],fixed_point_scale=256,curve_flattening='cubic16 samples; arc <=1.2 SVG unit segments; circle48',a8_sizes=[20,28,34],supersampling=4,pixel_bytes=15*(400+784+1156),dial_diameters=[58,80,88,94,130],dial_pixel_bytes=sum((d+2)**2 for d in [58,80,88,94,130]),generated_sha256=hashlib.sha256((DEST/'icon_data.c').read_bytes()).hexdigest()),indent=2)+'\n')
print('Generated15 flat LVGL icons from pinned Lucide SVGs')
