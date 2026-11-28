import pefile
pe=pefile.PE(r'D:\trainz12-decomp\bin\WaterSystem_7c.dll')
targets={0x1001D130,0x1001D12C,0x1001D3E4}
for imp in pe.DIRECTORY_ENTRY_IMPORT:
    for i in imp.imports:
        va=i.address
        if va in targets:
            print(hex(va), imp.dll.decode(), i.name)
