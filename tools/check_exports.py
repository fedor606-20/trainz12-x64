"""Compare the DLL's export table against the original export TSV (names and ordinals)."""
import csv
import struct
import sys
from pathlib import Path

dll = Path(sys.argv[1])
tsv = Path(sys.argv[2])
d = dll.read_bytes()
pe = struct.unpack_from("<I", d, 0x3c)[0]
opt = pe + 24
exp_rva = struct.unpack_from("<I", d, opt + 96)[0]
nsec = struct.unpack_from("<H", d, pe + 6)[0]
optsz = struct.unpack_from("<H", d, pe + 20)[0]
secs = [struct.unpack_from("<8sIIII", d, opt + optsz + 40 * i) for i in range(nsec)]


def off(rva):
    for _name, vs, va, rs, ro in secs:
        if va <= rva < va + max(vs, rs):
            return ro + rva - va
    raise ValueError(rva)


o = off(exp_rva)
base = struct.unpack_from("<I", d, o + 16)[0]
nf = struct.unpack_from("<I", d, o + 20)[0]
nn = struct.unpack_from("<I", d, o + 24)[0]
an = struct.unpack_from("<I", d, o + 32)[0]
ordtbl = struct.unpack_from("<I", d, o + 36)[0]
found = {}
for i in range(nn):
    nr = struct.unpack_from("<I", d, off(an) + 4 * i)[0]
    so = off(nr)
    name = d[so:d.index(b"\0", so)].decode("latin1")
    idx = struct.unpack_from("<H", d, off(ordtbl) + 2 * i)[0]
    ordinal = base + idx
    found[name] = ordinal

with tsv.open(encoding="utf-8") as f:
    expected = {e["mangled"]: int(e["ordinal"]) for e in csv.DictReader(f, delimiter="\t")}

missing = [(n, o) for n, o in expected.items() if found.get(n) != o]
extra = [(n, o) for n, o in found.items() if n not in expected]
print(f"DLL {len(found)} exports, TSV {len(expected)}; missing/wrong {len(missing)}, extra {len(extra)}")
for n, o in sorted(missing)[:20]:
    print("  MISSING", o, n, "got", found.get(n))
for n, o in sorted(extra)[:20]:
    print("  EXTRA  ", o, n)
