import pefile,struct
pe=pefile.PE(r'D:\trainz12-decomp\bin\WaterSystem_7c.dll',fast_load=True)
names={}
for line in open('ghidra_maps/jet/graphics/WaterSystem_7c.dll.functions.tsv',encoding='utf-8'):
    p=line.rstrip().split('\t')
    if len(p)>=2:
        try: names[int(p[0],16)]=p[1]
        except: pass
vtn={}
for line in open('ghidra_maps/jet/graphics/vtnames_WaterSystem_7c.dll.tsv',encoding='utf-8'):
    p=line.rstrip('\n').split('\t')
    if len(p)>=3:
        try: vtn[int(p[0],16)]=p[1]
        except: pass
for k,rva in [('WaterAnimation',0x1d918),('WaterAnimationPN',0x1d950)]:
    print('==',k)
    for i in range(10):
        w=struct.unpack('<I',pe.get_data(rva+i*4,4))[0]
        print(i,hex(w),vtn.get(w) or names.get(w) or '')
