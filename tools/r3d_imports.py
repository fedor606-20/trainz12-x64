import pefile
pe=pefile.PE(r'build32\Release\Render3DCore_7c.dll')
for imp in pe.DIRECTORY_ENTRY_IMPORT:
    if b'orig' in imp.dll:
        print(imp.dll.decode(), len(imp.imports))
        for i in imp.imports:
            print(i.name.decode() if i.name else 'ordinal %d'%i.ordinal)
