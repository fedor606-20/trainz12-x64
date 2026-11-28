import pefile, struct
pe=pefile.PE(r'D:\trainz12-decomp\bin\WaterSystem_7c.dll',fast_load=True)
base=pe.OPTIONAL_HEADER.ImageBase
def read(va,n): return pe.get_data(va-base,n)
names={}
for line in open('ghidra_maps/jet/graphics/WaterSystem_7c.dll.functions.tsv',encoding='utf-8'):
    p=line.rstrip('\n').split('\t')
    if len(p)>=2:
        try: names[int(p[0],16)]=p[1]
        except ValueError: pass
vt={'Water':0x1d80c,'WaterMaterial':0x1e4b4,'WaterMaterialPlugin':0x1e578,'WaterBumpMaterial':0x1d9ec,'WaterBumpMaterialPlugin':0x1dac0,
'WaterOceanMaterial':0x1ece4,'WaterOceanMaterialPlugin':0x1edb8,'WaterMaterialFresWave':0x1e6cc,'WaterMaterialFresWavePlugin':0x1e798,
'WaterMaterialOneTex':0x1ea6c,'WaterMaterialOneTexPlugin':0x1eb38,'WaterTile':0x1d7dc,'WaterTiled':0x1f014,'WaterSurface':0x1ef28,
'WaterManager':0x1df20,'WaterManagerPlugin':0x1df44,'WaterManagerGeneric':0x1e30c,'WaterManagerGenericPlugin':0x1e330,
'WaterManagerOceanSim':0x1e364,'WaterManagerOceanSimPlugin':0x1e388}
for k,rva in vt.items():
    va=base+rva
    print('\n==',k,hex(va))
    for i in range(0,40):
        w=struct.unpack('<I',read(va+i*4,4))[0]
        nm=names.get(w,'')
        print(f'{i:3d} {w:08x} {nm}')
        if i>4 and all(struct.unpack('<I',read(va+j*4,4))[0]==0 for j in range(i,min(i+4,40))): break
