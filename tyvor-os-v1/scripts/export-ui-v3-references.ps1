# Decode only for analysis/comparison; no bitmap is embedded in the firmware.
$ErrorActionPreference='Stop'
$Ui3Root=Split-Path -Parent $PSScriptRoot
$Ui3Ref=Join-Path $Ui3Root 'build/ui-v3/evidence/reference'
New-Item -ItemType Directory -Force $Ui3Ref | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.IO;
using System.Drawing;
public static class TyvorUi3Ppm {
  public static void Export(string source,string target) {
    using (Bitmap image=new Bitmap(source))
    using (FileStream stream=File.Create(target)) {
      byte[] header=System.Text.Encoding.ASCII.GetBytes("P6\n"+image.Width+" "+image.Height+"\n255\n");stream.Write(header,0,header.Length);
      byte[] row=new byte[image.Width*3];
      for(int y=0;y<image.Height;y++){for(int x=0;x<image.Width;x++){Color c=image.GetPixel(x,y);row[x*3]=c.R;row[x*3+1]=c.G;row[x*3+2]=c.B;}stream.Write(row,0,row.Length);}
    }
  }
}
'@
$Ui3Images=@((Join-Path $Ui3Ref 'official-home.png'))
$Ui3Images+=Get-ChildItem -LiteralPath (Join-Path $Ui3Root 'docs/ui-reference/oxbot') -File | Where-Object {$_.Extension -eq '.png'} | Select-Object -ExpandProperty FullName
$Ui3Images | ForEach-Object { [TyvorUi3Ppm]::Export($_,(Join-Path $Ui3Ref (([IO.Path]::GetFileNameWithoutExtension($_))+'.ppm'))) }
Write-Host 'Reference PNGs decoded to review-only PPM. User files unchanged.'
