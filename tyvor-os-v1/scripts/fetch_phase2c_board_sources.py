"""Read-only official/seller documents; save evidence in workspace, no devices."""
import hashlib,json,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
FOLDER=ROOT/'build/phase2c/evidence/board-sources'
URLS={
 'weact-b.pdf':'https://raw.githubusercontent.com/WeActStudio/WeActStudio.ESP32S3-AorB/main/ESP32S3-B/Hardware/ESP32_S3_B_Sch%20.pdf',
 'weact-tree.json':'https://api.github.com/repos/WeActStudio/WeActStudio.ESP32S3-AorB/git/trees/main?recursive=1',
 'thegioiic-typec.html':'https://www.thegioiic.com/mach-stm32f407vet6-type-c',
 'tuyetnga.html':'https://dientutuyetnga.com/products/weact-thu-phat-wifi-ble-esp32-s3-b-n16r8'}
def main():
 FOLDER.mkdir(parents=True,exist_ok=True);records=[]
 for name,url in URLS.items():
  try:
   data=urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'TYVOR-readonly-audit'}),timeout=25).read()
   (FOLDER/name).write_bytes(data);records.append(dict(name=name,url=url,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()));print(name,len(data))
  except Exception as ex:records.append(dict(name=name,url=url,error=str(ex)));print(name,str(ex))
 (FOLDER/'retrieval.json').write_text(json.dumps(records,indent=2))
if __name__=='__main__':main()
