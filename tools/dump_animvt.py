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
for l in open(r'D:\trainz12-decomp\exports\WaterSystem_7c.tsv',encoding='utf-8-sig').read().splitlines()[1:]:
    p=l.split('\t')
    if p[2] in ('??_7WaterAnimation@Jet@@6B@','??_7WaterAnimationPN@Jet@@6B@'):
        rva=int(p[1],16); print('==',p[3])
        for i in range(12):
            w=struct.unpack('<I',pe.get_data(rva+i*4,4))[0]
            print(i,hex(w),vtn.get(w) or names.get(w) or '')
