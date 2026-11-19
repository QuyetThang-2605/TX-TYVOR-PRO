$ErrorActionPreference='Stop'
$Ui321Root=Split-Path -Parent $PSScriptRoot
$Ui321Out=Join-Path $Ui321Root 'build/ui-v3-2-1/font'
New-Item -ItemType Directory -Force $Ui321Out | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;using System.Drawing;using System.Drawing.Drawing2D;using System.Drawing.Imaging;using System.Drawing.Text;using System.IO;using System.Collections.Generic;
public static class Tyvor321Font {
 public static string Bake(string ttf,string folder,int em,string name) {
  using(PrivateFontCollection fonts=new PrivateFontCollection()) {
   fonts.AddFontFile(ttf);FontFamily family=fonts.Families[0];FontStyle style=FontStyle.Bold;if(!family.IsStyleAvailable(style))throw new Exception("Original Bold face unavailable");
   int ascent=(int)Math.Ceiling((double)family.GetCellAscent(style)*em/family.GetEmHeight(style));
   int descent=(int)Math.Ceiling((double)family.GetCellDescent(style)*em/family.GetEmHeight(style));
   List<string> records=new List<string>();
   using(Bitmap measure=new Bitmap(1,1))using(Graphics mg=Graphics.FromImage(measure))using(Font font=new Font(family,em,style,GraphicsUnit.Pixel)) {
    for(int code=45;code<=58;code++)using(GraphicsPath path=new GraphicsPath()) {
     string text=((char)code).ToString();path.AddString(text,family,(int)style,em,new PointF(0,0),StringFormat.GenericTypographic);
     RectangleF bounds=path.GetBounds();int left=(int)Math.Floor(bounds.Left)-1,top=(int)Math.Floor(bounds.Top)-1;
     int w=(int)Math.Ceiling(bounds.Right)-left+1,h=(int)Math.Ceiling(bounds.Bottom)-top+1;
     int advance=(int)Math.Round(mg.MeasureString(text,font,1000,StringFormat.GenericTypographic).Width*16);
     using(Bitmap bitmap=new Bitmap(w*4,h*4,PixelFormat.Format32bppArgb)) {
      using(Graphics g=Graphics.FromImage(bitmap))using(Matrix matrix=new Matrix(4,0,0,4,-left*4,-top*4)) {
       g.Clear(Color.Transparent);g.SmoothingMode=SmoothingMode.None;g.Transform=matrix;g.FillPath(Brushes.White,path);
      }
      using(BinaryWriter writer=new BinaryWriter(File.Create(Path.Combine(folder,name+"-"+code+".a8")))) {
       for(int y=0;y<h;y++)for(int x=0;x<w;x++){int alpha=0;for(int j=0;j<4;j++)for(int i=0;i<4;i++)alpha+=bitmap.GetPixel(x*4+i,y*4+j).A;writer.Write((byte)((alpha+8)/16));}
      }
     }
     records.Add("{\"code\":"+code+",\"width\":"+w+",\"height\":"+h+",\"advance16\":"+advance+",\"ofs_x\":"+left+",\"ofs_y\":"+(ascent-top-h)+"}");
    }
   }
   return "{\"name\":\""+name+"\",\"family\":\""+family.Name+"\",\"em_pixels\":"+em+",\"line_height\":"+(ascent+descent+2)+",\"base_line\":"+(descent+1)+",\"glyphs\":["+String.Join(",",records)+"]}";
  }
 }
}
'@
$Ui321Font=Join-Path $Ui321Root 'experiments/tyvor_ui_v3_2_1/assets/font/Montserrat-Bold.ttf'
$Ui321Rows=@([Tyvor321Font]::Bake($Ui321Font,$Ui321Out,52,'compact'),[Tyvor321Font]::Bake($Ui321Font,$Ui321Out,74,'large'))
('['+($Ui321Rows -join ',')+']') | Set-Content (Join-Path $Ui321Out 'glyphs.json') -Encoding UTF8
& (Join-Path $Ui321Root '.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe') (Join-Path $PSScriptRoot 'pack-ui-v3-2-1-font.py')
if($LASTEXITCODE -ne 0){throw 'Font pack failed'}
