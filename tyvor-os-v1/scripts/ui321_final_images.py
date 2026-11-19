"""Lossless native screenshots and comparisons that never enlarge device pixels."""
import importlib.util,struct,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('ui321_previous_export',ROOT/'scripts/export-ui-v3-2-1.py')
previous=importlib.util.module_from_spec(spec);spec.loader.exec_module(previous)
png,ppm,sheet,resize,crop=previous.png,previous.ppm,previous.sheet,previous.resize,previous.crop
def read_png(path):
    data=path.read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n';offset=8;compressed=[];w=h=None
    while offset<len(data):
        n=struct.unpack_from('>I',data,offset)[0];kind=data[offset+4:offset+8];payload=data[offset+8:offset+8+n]
        assert zlib.crc32(kind+payload)&0xffffffff==struct.unpack_from('>I',data,offset+8+n)[0]
        if kind==b'IHDR':
            w,h,depth,color,comp,filtering,interlace=struct.unpack('>IIBBBBB',payload)
            assert (depth,color,comp,filtering,interlace)==(8,2,0,0,0)
        elif kind==b'IDAT':compressed.append(payload)
        offset+=n+12
    raw=zlib.decompress(b''.join(compressed));assert len(raw)==h*(1+w*3)
    assert all(raw[y*(1+w*3)]==0 for y in range(h)),'Native exporter contract is unfiltered RGB rows'
    return w,h,b''.join(raw[y*(1+w*3)+1:(y+1)*(1+w*3)] for y in range(h))
def fit_native(image,w,h):
    """One uniform bilinear policy: shrink or identity; never enlarge either side."""
    iw,ih,_=image;factor=min(1.0,w/iw,h/ih);nw,nh=round(iw*factor),round(ih*factor)
    scaled=resize(image,nw,nh)
    if (nw,nh)==(w,h):return scaled
    buf=bytearray(bytes([3,15,40])*(w*h));x=(w-nw)//2;y=(h-nh)//2
    for yy in range(nh):buf[((y+yy)*w+x)*3:((y+yy)*w+x+nw)*3]=scaled[2][yy*nw*3:(yy+1)*nw*3]
    return w,h,bytes(buf)
def native_pair(reference,actual):
    w,h,_=actual
    return [fit_native(reference,w,h),fit_native(actual,w,h)]
