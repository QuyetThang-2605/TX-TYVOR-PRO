"""Compile exact user background and real logo alpha into const Flash descriptors."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent;folder=ROOT/'build/ui-v3-2/assets';out=ROOT/'experiments/tyvor_ui_v3_2'
rows=json.loads((folder/'assets.json').read_text(encoding='utf-8-sig'));c=['#include "brand_assets.h"'];h=['#ifndef TYVOR32_ASSETS_H','#define TYVOR32_ASSETS_H','#include "lvgl.h"']
for r in rows:
    raw=(folder/r['file']).read_bytes();bpp=2 if r['format']=='RGB565' else 1
    assert len(raw)==r['width']*r['height']*bpp
    name='tyvor_ui32_'+r['name'];h.append('extern const lv_image_dsc_t '+name+';')
    c+=['static const uint8_t '+name+'_pixels[]={',*['  '+','.join(str(b) for b in raw[i:i+32])+',' for i in range(0,len(raw),32)],'};']
    c.append('const lv_image_dsc_t '+name+'={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_'+r['format']+',.w='+str(r['width'])+',.h='+str(r['height'])+',.stride='+str(r['width']*bpp)+'},.data_size='+str(len(raw))+',.data='+name+'_pixels};')
h.append('#endif');(out/'brand_assets.c').write_text('\n'.join(c)+'\n');(out/'brand_assets.h').write_text('\n'.join(h)+'\n')
e=ROOT/'build/ui-v3-2/evidence';e.mkdir(parents=True,exist_ok=True)
(e/'brand-assets.json').write_text(json.dumps(dict(assets=rows,background_pixel_bytes=sum(r['width']*r['height']*2 for r in rows if r['format']=='RGB565'),logo_pixel_bytes=sum(r['width']*r['height'] for r in rows if r['format']=='A8'),source_c_sha256=hashlib.sha256((out/'brand_assets.c').read_bytes()).hexdigest()),indent=2)+'\n')
print('Packed exact background and alpha logo assets')
