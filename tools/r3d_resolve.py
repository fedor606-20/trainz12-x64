"""Resolve an absolute address seen in a diff test against the built module's export table.

usage: python tools/r3d_resolve.py <dll> <abs-export-addr> <abs-sym-addr> [<abs-sym_addr> ...]
"""
import sys
import pefile

dll = sys.argv[1]
pe = pefile.PE(dll, fast_load=True)
pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
exports = {}
for sym in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if sym.name:
        exports[sym.address] = sym.name.decode('utf-8', 'replace')
first_export = int(sys.argv[2], 16)
named_rva = None
for rva, name in exports.items():
    if name == '?GetGeometryRadius@RenderBox@Jet@@MBAMXZ':
        named_rva = rva
base = first_export - named_rva
print('image base 0x%08X' % base)
for arg in sys.argv[2:]:
    value = int(arg, 16)
    rva = value - base
    name = exports.get(rva)
    print('0x%08X  rva 0x%06X  %s' % (value, rva, name or '<not an export start>'))
