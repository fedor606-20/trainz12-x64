import re

miss = set(l.strip() for l in open(r"D:\universal modder\trainz12-x64\notes\agents\RenderSystem_missing.txt", encoding="utf-8-sig") if l.strip())
path = r"D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c"

# collect mangled names per marker addr
addr_names = {}
with open(path, "r", encoding="utf-8", errors="ignore") as f:
    cur = None
    in_comment = False
    for line in f:
        m = re.match(r"^// ==== ([0-9a-fA-F]+)  ", line)
        if m:
            cur = int(m.group(1), 16)
            in_comment = True
            continue
        if cur is not None and in_comment:
            s = line.rstrip()
            if s == "" or s.startswith("/*") or s.startswith(" *") or s.startswith("*/") or re.match(r"^[A-Za-z_].*\(", s):
                # end of header comments area roughly, but names can appear on lines like "                       0x21870  739  ?Begin@..."
                pass
            # extract names from lines containing "0x...  <ord?>  ?..."
            for mm in re.finditer(r"\s(0x[0-9a-fA-F]+)\s+\d+\s+(\S+)", s):
                a = int(mm.group(1), 16)
                name = mm.group(2)
                addr_names.setdefault(a, set()).add(name)
            # stop scanning names when we hit actual code start heuristics
            if re.match(r"^(void|bool|int|uint|ulong|float|double|Layer|Display|PString|IList|Color|TextureAlpha|ApiDataBase|uint|undefined|char|uchar|ushort|short|long|Matrix)", s):
                in_comment = False
                cur = None

for addr in [0x10021870, 0x100015d0, 0x10009500, 0x1001dec0, 0x10038c50]:
    names = addr_names.get(addr, set())
    hit = sorted(n for n in names if n in miss)
    print("ADDR", hex(addr), "total", len(names), "missing", len(hit))
    for n in hit:
        print(" ", n)
