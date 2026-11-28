import pefile, struct
pe=pefile.PE(r'D:\trainz12-decomp\bin\WaterSystem_7c.dll',fast_load=True); base=pe.OPTIONAL_HEADER.ImageBase
def read(va,n): return pe.get_data(va-base,n)
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
vt={'Water':0x1d80c,'WaterMaterial':0x1e4b4,'WaterBumpMaterial':0x1d9ec,'WaterOceanMaterial':0x1ece4,'WaterMaterialFresWave':0x1e6cc,'WaterMaterialOneTex':0x1ea6c,'WaterTile':0x1d7dc,'WaterTiled':0x1f014,'WaterSurface':0x1ef28,'WaterManager':0x1df20,'WaterManagerGeneric':0x1e30c,'WaterManagerOceanSim':0x1e364}
for k,rva in vt.items():
    va=base+rva; print('\n==',k)
    for i in range(0,80):
        w=struct.unpack('<I',read(va+i*4,4))[0]
        if w==0 and i>0: break
        print(f'{i:3d} {w:08x} {vtn.get(w) or names.get(w) or ""}')
