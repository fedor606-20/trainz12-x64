"""Check PE32 export names, ordinals and optional forbidden DLL dependencies."""
import argparse
import csv
import struct
import sys
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("dll", type=Path)
parser.add_argument("tsv", type=Path)
parser.add_argument("--forbid-dll", action="append", default=[],
                    help="Reject normal imports, delay imports and export forwarders to this DLL.")
args = parser.parse_args()
dll = args.dll
tsv = args.tsv
d = dll.read_bytes()
pe = struct.unpack_from("<I", d, 0x3c)[0]
opt = pe + 24
exp_rva, exp_size = struct.unpack_from("<II", d, opt + 96)
nsec = struct.unpack_from("<H", d, pe + 6)[0]
optsz = struct.unpack_from("<H", d, pe + 20)[0]
secs = [struct.unpack_from("<8sIIII", d, opt + optsz + 40 * i) for i in range(nsec)]


def off(rva):
    for _name, vs, va, rs, ro in secs:
        if va <= rva < va + max(vs, rs):
            return ro + rva - va
    raise ValueError(rva)


def text(rva):
    start = off(rva)
    return d[start:d.index(b"\0", start)].decode("latin1")


def import_modules(delay):
    slot, stride = (13, 32) if delay else (1, 20)
    rva, size = struct.unpack_from("<II", d, opt + 96 + slot * 8)
    if not rva:
        return
    start = off(rva)
    for pos in range(start, start + size, stride):
        if not any(d[pos:pos + stride]):
            break
        name_rva = struct.unpack_from("<I", d, pos + (4 if delay else 12))[0]
        if delay and not (struct.unpack_from("<I", d, pos)[0] & 1):
            name_rva -= struct.unpack_from("<I", d, opt + 28)[0]
        yield text(name_rva)


o = off(exp_rva)
base = struct.unpack_from("<I", d, o + 16)[0]
nf = struct.unpack_from("<I", d, o + 20)[0]
nn = struct.unpack_from("<I", d, o + 24)[0]
an = struct.unpack_from("<I", d, o + 32)[0]
af = struct.unpack_from("<I", d, o + 28)[0]
ordtbl = struct.unpack_from("<I", d, o + 36)[0]
found = {}
forwarded = set()
for i in range(nn):
    nr = struct.unpack_from("<I", d, off(an) + 4 * i)[0]
    name = text(nr)
    idx = struct.unpack_from("<H", d, off(ordtbl) + 2 * i)[0]
    ordinal = base + idx
    found[name] = ordinal

for i in range(nf):
    function_rva = struct.unpack_from("<I", d, off(af) + 4 * i)[0]
    if exp_rva <= function_rva < exp_rva + exp_size:
        forwarded.add(text(function_rva).rsplit(".", 1)[0])

with tsv.open(encoding="utf-8") as f:
    expected = {e["mangled"]: int(e["ordinal"]) for e in csv.DictReader(f, delimiter="\t")}

missing = [(n, o) for n, o in expected.items() if found.get(n) != o]
extra = [(n, o) for n, o in found.items() if n not in expected]
print(f"DLL {len(found)} exports, TSV {len(expected)}; missing/wrong {len(missing)}, extra {len(extra)}")
for n, o in sorted(missing)[:20]:
    print("  MISSING", o, n, "got", found.get(n))
for n, o in sorted(extra)[:20]:
    print("  EXTRA  ", o, n)

def normalized(name):
    return name.casefold().removesuffix(".dll")


forbidden = {normalized(n) for n in args.forbid_dll}
blocked = []
if forbidden:
    for kind, modules in (("import", import_modules(False)), ("delay import", import_modules(True)),
                          ("export forwarder", sorted(forwarded))):
        blocked.extend((kind, n) for n in modules if normalized(n) in forbidden)
    for kind, name in sorted(set(blocked)):
        print(f"  FORBIDDEN {kind}: {name}")
sys.exit(1 if missing or extra or blocked else 0)
