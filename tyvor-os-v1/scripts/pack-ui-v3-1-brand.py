"""Pack user-provided TYVOR artwork, not a recreated logo. No UI screenshot asset."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent;folder=ROOT/'build/ui-v3-1/brand';rows=json.loads((folder/'assets.json').read_text(encoding='utf-8-sig'));output=['#include "brand_assets.h"'];decl=[]
for r in rows:
    data=(folder/(r['name']+'.rgb565')).read_bytes();assert len(data)==r['width']*r['height']*2
    name='tyvor_ui31_'+r['name'];decl.append('extern const lv_image_dsc_t '+name+';')
    output+=['static const uint8_t '+name+'_pixels[]={'+','.join(hex(b) for b in data)+'};',
        'const lv_image_dsc_t '+name+'={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w='+str(r['width'])+',.h='+str(r['height'])+',.stride='+str(r['width']*2)+'},.data_size='+str(len(data))+',.data='+name+'_pixels};']
out=ROOT/'experiments/tyvor_ui_v3_1';(out/'brand_assets.c').write_text('\n'.join(output)+'\n');(out/'brand_assets.h').write_text('#ifndef TYVOR31_BRAND_ASSETS_H\n#define TYVOR31_BRAND_ASSETS_H\n#include "lvgl.h"\n'+'\n'.join(decl)+'\n#endif\n')
(ROOT/'build/ui-v3-1/evidence/brand-assets.json').write_text(json.dumps(dict(assets=rows,format='RGB565 opaque little endian, source white retained',transform='content crop + aspect-preserving HQ bicubic resize; no recoloring/redrawing',pixel_bytes=sum(r['width']*r['height']*2 for r in rows),source_c_sha256=hashlib.sha256((out/'brand_assets.c').read_bytes()).hexdigest()),indent=2)+'\n')
print('Actual TYVOR logo assets packed:',sum(r['width']*r['height']*2 for r in rows),'Flash pixel bytes')
