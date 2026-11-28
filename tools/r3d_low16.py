"""Find exports whose RVA low 16 bits match a value (ASLR base is 64K aligned)."""
import sys
import pefile

pe = pefile.PE(sys.argv[1], fast_load=True)
pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
wanted = [int(x, 16) for x in sys.argv[2:]]
for sym in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if not sym.name:
        continue
    for w in wanted:
        if (sym.address & 0xFFFF) == w:
            print('low16 0x%04X  rva 0x%06X  %s' % (w, sym.address, sym.name.decode('utf-8', 'replace')))
