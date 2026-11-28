import sys
import pefile

vt_name = sys.argv[1] if len(sys.argv) > 1 else '??_7RenderPrimitive@Jet@@6B@'


def load(path):
    pe = pefile.PE(path)
    base = pe.OPTIONAL_HEADER.ImageBase
    exp = {}
    for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if e.name:
            exp[e.name.decode('ascii', 'replace')] = e.address
    def rva_of(addr):
        return addr - base
    return pe, base, exp


orig_pe, orig_base, orig_exp = load(r'D:\universal modder\trainz12-x64\testbin-r3d\Render3DCore_7c_orig.dll')
our_pe, our_base, our_exp = load(r'D:\universal modder\trainz12-x64\testbin-r3d\Render3DCore_7c.dll')

def read_vtable(pe, base, name):
    rva = dict((k, v) for k, v in ((n, a) for n, a in []))  # unused
    # find export
    for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if e.name and e.name.decode('ascii', 'replace') == name:
            addr = base + e.address
            data = pe.get_data(e.address, 4 * 80)
            return [int.from_bytes(data[i:i+4], 'little') for i in range(0, len(data), 4)]
    raise SystemExit('vtable not exported: ' + name)

def name_at(pe, base, exp, rva):
    for n, a in exp.items():
        if a == rva:
            return n
    return ''

for tag, pe, base, exp in (('OURS', our_pe, our_base, our_exp), ('ORIG', orig_pe, orig_base, orig_exp)):
    slots = read_vtable(pe, base, vt_name)
    n = 0
    while n < len(slots) and slots[n] >= pe.OPTIONAL_HEADER.ImageBase:
        n += 1
    print('====', tag, 'slots', n)
    for i in range(n):
        rva = slots[i] - base
        print('%3d  %08x  %s' % (i, rva, name_at(pe, base, exp, rva)))
