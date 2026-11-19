import re

miss = set(l.strip() for l in open(r"D:\universal modder\trainz12-x64\notes\agents\RenderSystem_missing.txt", encoding="utf-8-sig") if l.strip())

path = r"D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c"
addr_map = {}  # va -> set(names)
cur = None
with open(path, "r", encoding="utf-8", errors="ignore") as f:
    for line in f:
        m = re.match(r"^// ==== ([0-9a-fA-F]+)  ", line)
        if m:
            cur = int(m.group(1), 16)
            addr_map.setdefault(cur, set())
            continue
        if cur is not None:
            # collect name lines like: "                       0x21870  739  ?Begin@Layer@Jet@@MAAXXZ"
            for mm in re.finditer(r"0x([0-9a-fA-F]+)\s+\d+\s+(\S+)", line):
                a = int(mm.group(1), 16)
                name = mm.group(2)
                addr_map.setdefault(a, set()).add(name)
            # heuristic stop: when we reach next marker handled above; also stop if function body ends and new marker comes (handled)
            # but easier to not stop aggressively; names only appear in header comment block

# filter to interesting trivial addresses
targets = [0x10021870, 0x100015d0, 0x10009500, 0x1001dec0, 0x10038c50, 0x10004500]
for va in targets:
    names = addr_map.get(va, set())
    hit = sorted([n for n in names if n in miss])
    print("VA", hex(va), "MISSING", len(hit), "TOTAL", len(names))
    for n in hit:
        print("  ", n)
