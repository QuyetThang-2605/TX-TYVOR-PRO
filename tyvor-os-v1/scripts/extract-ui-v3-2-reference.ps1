$ErrorActionPreference='Stop'
$Ui32Root=Split-Path -Parent $PSScriptRoot
$Ui32Source=Join-Path $Ui32Root 'docs/ui-reference/tyvor-v3-2/approved-home.png'
if(!(Test-Path -LiteralPath $Ui32Source)){$Ui32Source=Join-Path $Ui32Root 'docs/ui-reference/tyvor-v3-2/approved-home.png.png'}
$Ui32Out=Join-Path $Ui32Root 'build/ui-v3-2/evidence/reference'
New-Item -ItemType Directory -Force $Ui32Out | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;using System.Drawing;using System.IO;using System.Text;
public static class Tyvor32Reference {
 public static int[] Export(string source,string target,int x,int y,int w,int h) {
  using(Bitmap b=new Bitmap(source))using(FileStream f=File.Create(target)) {
   if(x<0||y<0||x+w>b.Width||y+h>b.Height)throw new ArgumentException("Crop exceeds approved source");
   byte[] head=Encoding.ASCII.GetBytes("P6\n"+w+" "+h+"\n255\n");f.Write(head,0,head.Length);byte[] row=new byte[w*3];
   for(int yy=0;yy<h;yy++){for(int xx=0;xx<w;xx++){Color c=b.GetPixel(x+xx,y+yy);row[xx*3]=c.R;row[xx*3+1]=c.G;row[xx*3+2]=c.B;}f.Write(row,0,row.Length);}
   return new int[]{b.Width,b.Height};
  }
 }
}
'@
$Ui32Rows=@()
foreach($spec in @(@('approved-middle',734,72,704,570),@('approved-right',1456,72,702,570))){
 $target=Join-Path $Ui32Out ($spec[0]+'.ppm')
 $dims=[Tyvor32Reference]::Export($Ui32Source,$target,$spec[1],$spec[2],$spec[3],$spec[4])
 $Ui32Rows += [PSCustomObject]@{panel=$spec[0];crop=@($spec[1],$spec[2],$spec[3],$spec[4]);source_size=$dims;pixel_export_sha256=(Get-FileHash $target -Algorithm SHA256).Hash.ToLower()}
}
[PSCustomObject]@{source=$Ui32Source.Substring($Ui32Root.Length+1).Replace('\','/');source_sha256=(Get-FileHash $Ui32Source -Algorithm SHA256).Hash.ToLower();panels=$Ui32Rows;purpose='Approved TYVOR middle/right panels for review only; never linked as UI screenshot'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $Ui32Out '../reference-source.json') -Encoding UTF8
Write-Host 'Approved TYVOR panels extracted for comparisons only.'
