$ErrorActionPreference='Stop'
$Ui32Root=Split-Path -Parent $PSScriptRoot
$Ui32Out=Join-Path $Ui32Root 'build/ui-v3-2/assets'
New-Item -ItemType Directory -Force $Ui32Out | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;using System.Drawing;using System.Drawing.Drawing2D;using System.Drawing.Imaging;using System.IO;
public static class Tyvor32Assets {
 public static int[] Background(string source,string target,int width,int height) {
  using(Bitmap src=new Bitmap(source)) {
   double scale=Math.Max((double)width/src.Width,(double)height/src.Height);
   float cw=(float)(width/scale),ch=(float)(height/scale),cx=(src.Width-cw)/2,cy=(src.Height-ch)/2;
   using(Bitmap dst=new Bitmap(width,height,PixelFormat.Format24bppRgb)) {
    using(Graphics g=Graphics.FromImage(dst)){g.InterpolationMode=InterpolationMode.HighQualityBicubic;g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.DrawImage(src,new RectangleF(0,0,width,height),new RectangleF(cx,cy,cw,ch),GraphicsUnit.Pixel);}
    using(BinaryWriter writer=new BinaryWriter(File.Create(target))){for(int y=0;y<height;y++)for(int x=0;x<width;x++){Color c=dst.GetPixel(x,y);writer.Write((ushort)((((c.R*31+127)/255)<<11)|(((c.G*63+127)/255)<<5)|((c.B*31+127)/255)));}}
   }
   return new int[]{src.Width,src.Height,width,height};
  }
 }
 public static int[] Brand(string source,string target,int width,bool owl) {
  using(Bitmap src=new Bitmap(source)) {
   int x0=src.Width,y0=src.Height,x1=0,y1=0,limit=owl?178:src.Height;
   for(int y=0;y<limit;y++)for(int x=0;x<src.Width;x++){Color c=src.GetPixel(x,y);if(Math.Min(c.R,Math.Min(c.G,c.B))<200){x0=Math.Min(x0,x);y0=Math.Min(y0,y);x1=Math.Max(x1,x);y1=Math.Max(y1,y);}}
   x0=Math.Max(0,x0-3);y0=Math.Max(0,y0-3);x1=Math.Min(src.Width-1,x1+3);y1=Math.Min(limit-1,y1+3);
   int cw=x1-x0+1,ch=y1-y0+1,height=(int)Math.Round((double)width*ch/cw);
   using(Bitmap mask=new Bitmap(cw,ch,PixelFormat.Format32bppArgb))using(Bitmap dst=new Bitmap(width,height,PixelFormat.Format32bppArgb)) {
    for(int y=0;y<ch;y++)for(int x=0;x<cw;x++){Color c=src.GetPixel(x0+x,y0+y);int min=Math.Min(c.R,Math.Min(c.G,c.B));int alpha=min>=246?0:Math.Min(255,(255-min)*255/243);mask.SetPixel(x,y,Color.FromArgb(alpha,255,255,255));}
    using(Graphics g=Graphics.FromImage(dst)){g.Clear(Color.Transparent);g.InterpolationMode=InterpolationMode.HighQualityBicubic;g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.DrawImage(mask,new Rectangle(0,0,width,height));}
    using(BinaryWriter writer=new BinaryWriter(File.Create(target))){for(int y=0;y<height;y++)for(int x=0;x<width;x++)writer.Write(dst.GetPixel(x,y).A);}
   }return new int[]{x0,y0,cw,ch,width,height};
  }
 }
}
'@
$Ui32Rows=@()
$Ui32Bg=Join-Path $Ui32Root 'docs/ui-reference/tyvor-v3-2/blue-wave-background.png'
foreach($size in @(@(320,240),@(448,368))){
 $name='background_'+$size[0];$target=Join-Path $Ui32Out ($name+'.rgb565')
 $dims=[Tyvor32Assets]::Background($Ui32Bg,$target,$size[0],$size[1])
 $Ui32Rows += [PSCustomObject]@{name=$name;format='RGB565';source='docs/ui-reference/tyvor-v3-2/blue-wave-background.png';source_sha256=(Get-FileHash $Ui32Bg -Algorithm SHA256).Hash.ToLower();source_size=@($dims[0],$dims[1]);width=$size[0];height=$size[1];file=($name+'.rgb565');raw_sha256=(Get-FileHash $target -Algorithm SHA256).Hash.ToLower();transform='center-cover crop, uniform HQ bicubic scale, RGB565 quantization; no hue/saturation/brightness edits'}
}
foreach($spec in @(@('owl_compact','reference-01.png',26,$true),@('owl_large','reference-01.png',36,$true),@('wordmark_compact','reference-02.png',52,$false),@('wordmark_large','reference-02.png',74,$false),@('header_compact','reference-02.png',76,$false),@('header_large','reference-02.png',112,$false))){
 $source=Join-Path $Ui32Root ('docs/ui-reference/oxbot/'+$spec[1]);$target=Join-Path $Ui32Out ($spec[0]+'.a8')
 $dims=[Tyvor32Assets]::Brand($source,$target,$spec[2],$spec[3])
 $Ui32Rows += [PSCustomObject]@{name=$spec[0];format='A8';source=('docs/ui-reference/oxbot/'+$spec[1]);source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash.ToLower();crop=@($dims[0],$dims[1],$dims[2],$dims[3]);width=$dims[4];height=$dims[5];file=($spec[0]+'.a8');raw_sha256=(Get-FileHash $target -Algorithm SHA256).Hash.ToLower();transform='white JPEG background unmatted to alpha; aspect-preserving scale; white foreground tint follows approved design; no redrawn geometry'}
}
$Ui32Rows | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $Ui32Out 'assets.json') -Encoding UTF8
Write-Host 'Original blue-wave and alpha branding baked; no UI screenshot asset.'
