$ErrorActionPreference='Stop'
$Ui31Root=Split-Path -Parent $PSScriptRoot
$Ui31Out=Join-Path $Ui31Root 'build/ui-v3-1/brand'
New-Item -ItemType Directory -Force $Ui31Out | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;using System.Drawing;using System.Drawing.Drawing2D;using System.Drawing.Imaging;using System.IO;
public static class Tyvor31Brand {
 public static int[] Bake(string source,string target,int width,bool owl) {
  using(Bitmap src=new Bitmap(source)) {
   int minX=src.Width,minY=src.Height,maxX=0,maxY=0,limit=owl?178:src.Height;
   for(int y=0;y<limit;y++)for(int x=0;x<src.Width;x++){Color c=src.GetPixel(x,y);if(c.R<200||c.G<200||c.B<200){minX=Math.Min(minX,x);minY=Math.Min(minY,y);maxX=Math.Max(maxX,x);maxY=Math.Max(maxY,y);}}
   minX=Math.Max(0,minX-3);minY=Math.Max(0,minY-3);maxX=Math.Min(src.Width-1,maxX+3);maxY=Math.Min(limit-1,maxY+3);
   int cropW=maxX-minX+1,cropH=maxY-minY+1,height=(int)Math.Round((double)width*cropH/cropW);
   using(Bitmap dst=new Bitmap(width,height,PixelFormat.Format32bppArgb)) {
    using(Graphics g=Graphics.FromImage(dst)){g.Clear(Color.White);g.InterpolationMode=InterpolationMode.HighQualityBicubic;g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.DrawImage(src,new Rectangle(0,0,width,height),new Rectangle(minX,minY,cropW,cropH),GraphicsUnit.Pixel);}
    using(BinaryWriter writer=new BinaryWriter(File.Create(target))){for(int y=0;y<height;y++)for(int x=0;x<width;x++){Color c=dst.GetPixel(x,y);writer.Write((ushort)(((c.R>>3)<<11)|((c.G>>2)<<5)|(c.B>>3)));}}
   }return new int[]{minX,minY,cropW,cropH,width,height};
  }
 }
}
'@
$Ui31Records=@()
foreach($spec in @(@('owl_compact','reference-01.png',26,$true),@('owl_large','reference-01.png',36,$true),@('wordmark_compact','reference-02.png',52,$false),@('wordmark_large','reference-02.png',74,$false))){
 $source=Join-Path $Ui31Root ('docs/ui-reference/oxbot/'+$spec[1]);$target=Join-Path $Ui31Out ($spec[0]+'.rgb565')
 $dims=[Tyvor31Brand]::Bake($source,$target,$spec[2],$spec[3])
 $Ui31Records += [PSCustomObject]@{name=$spec[0];source=('docs/ui-reference/oxbot/'+$spec[1]);source_sha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLower();crop=@($dims[0],$dims[1],$dims[2],$dims[3]);width=$dims[4];height=$dims[5];raw_sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLower()}
}
$Ui31Records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Ui31Out 'assets.json') -Encoding UTF8
$Ui31Python=Join-Path $Ui31Root '.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe'
& $Ui31Python (Join-Path $PSScriptRoot 'pack-ui-v3-1-brand.py')
if($LASTEXITCODE -ne 0){throw 'Brand packing failed'}
