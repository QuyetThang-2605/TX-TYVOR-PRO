"""Exporter regressions. --before evaluates the old400x328 comparison policy."""
import sys,tempfile,unittest
from pathlib import Path
from ui321_final_images import ROOT,previous,fit_native,native_pair,png,read_png,ppm
BEFORE='--before' in sys.argv
if BEFORE:sys.argv.remove('--before')
class ComparisonContract(unittest.TestCase):
    def test_native_comparison_keeps_actual_pixels(self):
        for w,h in [(320,240),(448,368)]:
            actual=(w,h,bytes(c for y in range(h) for x in range(w) for c in ([255,255,255] if x%7==0 else [0,0,0])))
            result=previous.fit(actual,400,328) if BEFORE else native_pair((w*2,h*2,bytes([30,60,120])*(w*2*h*2)),actual)[1]
            self.assertEqual(result[:2],actual[:2],'Device dimensions must stay1:1 in the comparison')
            self.assertTrue(result[2]==actual[2],'Comparison changed native device pixels')
    def test_smaller_inputs_are_padded_not_enlarged(self):
        im=(2,2,bytes([255,0,0])*4)
        result=previous.fit(im,4,4) if BEFORE else fit_native(im,4,4)
        self.assertEqual(sum(result[2][i:i+3]==b'\xff\0\0' for i in range(0,len(result[2]),3)),4)
    def test_native_png_roundtrip_is_lossless(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build/ui-v3-2-1-final') as folder:
            path=Path(folder)/'raw.png';im=(7,3,bytes((i*17)%256 for i in range(63)));png(path,*im);self.assertEqual(read_png(path),im)
    def test_previous_native_png_matches_renderer_ppm(self):
        for w,h in [(320,240),(448,368)]:
            self.assertEqual(read_png(ROOT/f'artifacts/tyvor-ui-v3-2-1/home-{w}x{h}.png'),ppm(ROOT/f'build/ui-v3-2-1/rendered/ui-{w}x{h}-state3-page0.ppm'))
    def test_previous_rgb888_capture_is_not_565_alias(self):
        for w,h in [(320,240),(448,368)]:
            raw=read_png(ROOT/f'artifacts/tyvor-ui-v3-2-1/home-rgb888-{w}x{h}.png')
            self.assertEqual(raw,ppm(ROOT/f'build/ui-v3-2-1/rendered/home-rgb888-{w}x{h}.ppm'))
            self.assertNotEqual(raw[2],read_png(ROOT/f'artifacts/tyvor-ui-v3-2-1/home-{w}x{h}.png')[2])
if __name__=='__main__':unittest.main(verbosity=2)
