import pefile, struct
pe=pefile.PE(r'D:\trainz12-decomp\bin\WaterSystem_7c.dll',fast_load=True); base=pe.OPTIONAL_HEADER.ImageBase
names={}
for line in open('ghidra_maps/jet/graphics/WaterSystem_7c.dll.functions.tsv',encoding='utf-8'):
    p=line.rstrip().split('\t')
    if len(p)>=2:
        try: names[int(p[0],16)]=p[1]
        except: pass
for k,rva in [('BumpPlugin',0x1dbfc),('OceanPlugin',0x1eef4)]:
    print('==',k)
    for i in range(8):
        w=struct.unpack('<I',pe.get_data(rva+i*4,4))[0]
        print(i,hex(w),names.get(w,''))
