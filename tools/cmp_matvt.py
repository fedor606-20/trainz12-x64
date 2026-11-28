import pefile,struct
def slots(path,vtname,n):
    pe=pefile.PE(path); base=pe.OPTIONAL_HEADER.ImageBase
    exps={}
    for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if e.name: exps[base+e.address]=e.name.decode('latin1')
    hdr=[e for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name and e.name.decode('latin1')==vtname]
    if not hdr: return None
    rva=hdr[0].address
    return [exps.get(struct.unpack('<I',pe.get_data(rva+i*4,4))[0],'') for i in range(n)]
a=slots(r'build32\Release\WaterSystem_7c.dll','??_7WaterBumpMaterial@Jet@@6BMaterial@1@@',52)
b=slots(r'D:\trainz12-decomp\bin\WaterSystem_7c.dll','??_7WaterBumpMaterial@Jet@@6BMaterial@1@@',52)
for i,(x,y) in enumerate(zip(a,b)):
    mark='' if (x and x==y) else '  <--'
    print(f'{i:2d} {x[:60]:60s} | {y[:60]}{mark}')
