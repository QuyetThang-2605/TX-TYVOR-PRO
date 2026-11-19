$ErrorActionPreference='Stop'
$Ui31Root=Split-Path -Parent $PSScriptRoot
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System.Drawing;using System.IO;using System.Text;
public static class Tyvor31Reference {
 public static void Export(string source,string target){
  using(Bitmap b=new Bitmap(source))using(FileStream f=File.Create(target)){
   byte[] head=Encoding.ASCII.GetBytes("P6\n400 315\n255\n");f.Write(head,0,head.Length);byte[] row=new byte[1200];
   for(int y=0;y<315;y++){for(int x=0;x<400;x++){Color c=b.GetPixel(14+x,14+y);row[x*3]=c.R;row[x*3+1]=c.G;row[x*3+2]=c.B;}f.Write(row,0,row.Length);}
  }
 }
}
'@
$Ui31Source=Join-Path $Ui31Root 'artifacts/tyvor-ui-v3-oxbot/home-reference-comparison.png'
$Ui31Target=Join-Path $Ui31Root 'build/ui-v3-1/evidence/reference/official-home.ppm'
[Tyvor31Reference]::Export($Ui31Source,$Ui31Target)
[PSCustomObject]@{source='artifacts/tyvor-ui-v3-oxbot/home-reference-comparison.png';source_sha256=(Get-FileHash $Ui31Source -Algorithm SHA256).Hash.ToLower();crop=@(14,14,400,315);pixel_export_sha256=(Get-FileHash $Ui31Target -Algorithm SHA256).Hash.ToLower();purpose='Left Oxbot panel, exact requested visual source; not embedded firmware'} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $Ui31Root 'build/ui-v3-1/evidence/reference-source.json') -Encoding UTF8
Write-Host 'Exact left reference panel extracted for comparison only.'
