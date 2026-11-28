import pefile,struct
pe=pefile.PE(r'D:\trainz12-decomp\bin\WaterSystem_7c.dll',fast_load=True)
for rva in range(0x1d918,0x1d960,4):
    w=struct.unpack('<I',pe.get_data(rva,4))[0]
    print(hex(rva),hex(w))
