"""Pack native-size outline coverage as LVGL4bpp numeric fonts, no bitmap scaling."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent;folder=ROOT/'build/ui-v3-2-1/font';out=ROOT/'experiments/tyvor_ui_v3_2_1'
fonts=json.loads((folder/'glyphs.json').read_text(encoding='utf-8-sig'));c=['/* Native-size Montserrat outline subset; SIL OFL, assets/font/OFL.txt. */','#include "timer_font.h"']
for font in fonts:
    raw=[];rows=['{0,0,0,0,0,0}']
    for g in font['glyphs']:
        data=(folder/(font['name']+'-'+str(g['code'])+'.a8')).read_bytes();assert len(data)==g['width']*g['height']
        values=[(v*15+127)//255 for v in data];start=len(raw)
        if len(values)%2:values.append(0)
        raw.extend(values[i]*16+values[i+1] for i in range(0,len(values),2))
        rows.append('{'+','.join(map(str,[start,g['advance16'],g['width'],g['height'],g['ofs_x'],g['ofs_y']]))+'}')
    name='ui321_timer_'+font['name'];font['bitmap_bytes']=len(raw)
    c+=['static const uint8_t '+name+'_bitmap[]={'+','.join(map(str,raw))+'};',
        'static const lv_font_fmt_txt_glyph_dsc_t '+name+'_glyphs[]={'+','.join(rows)+'};',
        'static const lv_font_fmt_txt_cmap_t '+name+'_map[]={{.range_start=45,.range_length=14,.glyph_id_start=1,.type=LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}};',
        'static const lv_font_fmt_txt_dsc_t '+name+'_dsc={.glyph_bitmap='+name+'_bitmap,.glyph_dsc='+name+'_glyphs,.cmaps='+name+'_map,.cmap_num=1,.bpp=4};',
        'const lv_font_t tyvor_'+name+'={.get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,.get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,.line_height='+str(font['line_height'])+',.base_line='+str(font['base_line'])+',.dsc=&'+name+'_dsc};']
(out/'timer_font.c').write_text('\n'.join(c)+'\n')
e=ROOT/'build/ui-v3-2-1/evidence';e.mkdir(parents=True,exist_ok=True)
(e/'font-generation.json').write_text(json.dumps(dict(fonts=fonts,source='experiments/tyvor_ui_v3_2_1/assets/font/Montserrat-Bold.ttf',source_sha256=hashlib.sha256((out/'assets/font/Montserrat-Bold.ttf').read_bytes()).hexdigest(),supersampling=4,font_weight=700,source_style="Bold",no_bitmap_rescale=True,no_dilation=True,generated_sha256=hashlib.sha256((out/'timer_font.c').read_bytes()).hexdigest()),indent=2)+'\n')
print('Generated native-size outline fonts',[(f['name'],f['family'],f['line_height'],f['bitmap_bytes']) for f in fonts])
