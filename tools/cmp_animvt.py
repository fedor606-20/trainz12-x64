import pefile,struct
def dump(path, vtname):
    pe=pefile.PE(path); base=pe.OPTIONAL_HEADER.ImageBase
    exps={}
    for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if e.name: exps[base+e.address]=e.name.decode('latin1')
    hdr=[e for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name and e.name.decode('latin1')==vtname]
    if not hdr: print(path,'no vtable'); return
    rva=hdr[0].address; print(path,hex(base+rva))
    for i in range(6):
        w=struct.unpack('<I',pe.get_data(rva+i*4,4))[0]
        print(' ',i,exps.get(w,'%08x'%w))
for p in [r'build32\Release\WaterSystem_7c.dll',r'D:\trainz12-decomp\bin\WaterSystem_7c.dll']:
    dump(p,'??_7WaterAnimationPN@Jet@@6BWaterAnimation@1@@')
