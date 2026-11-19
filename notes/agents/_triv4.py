import re
miss = set(l.strip() for l in open(r"D:\universal modder\trainz12-x64\notes\agents\RenderSystem_missing.txt", encoding="utf-8-sig") if l.strip())
path = r"D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c"
addr_hits = {}
with open(path, 'r', encoding='utf-8', errors='ignore') as f:
    for line in f:
        mm = re.match(r'^\s+0x([0-9a-fA-F]+)\s+\d+\s+(\?\S+)', line)
        if mm:
            a = 0x10000000 + int(mm.group(1),16) if int(mm.group(1),16) < 0x10000000 else int(mm.group(1),16)
            # in file they show as 0x21870 (relative-like shown?) wait earlier: "0x21870  739  ?Begin..." and marker was // ==== 10021870
            addr_hits.setdefault(a, set()).add(mm.group(2))
# but in file addresses in those lines are written as 0x21870 (without 1000 prefix) in the example I saw? "0x21870  310  ??1TextureStage"
# let's detect
addr_hits2 = {}
with open(path, 'r', encoding='utf-8', errors='ignore') as f:
    for line in f:
        mm = re.match(r'^\s+0x([0-9a-fA-F]+)\s+\d+\s+(\?\S+)', line)
        if mm:
            v = int(mm.group(1),16)
            if v < 0x100000: v += 0x10000000
            addr_hits2.setdefault(v, set()).add(mm.group(2))

targets = [0x10021870,0x100015d0,0x10009500,0x1001dec0,0x10038c50,0x10004500,0x1000ae00,0x1000ae30,0x1000ae40,0x1000ae50,0x1000ae70,0x1000ae10,0x100106d0,0x10010940,0x1000b2b0,0x1000b2c0,0x1000d2d0,0x1000d300,0x1003ea80,0x10031640,0x10031680,0x1000ac40,0x1000acd0,0x10031430,0x1000af00,0x1000af40]
for va in targets:
    names = addr_hits2.get(va, set())
    hit = sorted([n for n in names if n in miss])
    print('VA', hex(va), 'MISSING', len(hit), 'TOTAL', len(names))
    for n in hit[:12]:
        print(' ', n)
    if len(hit)>12: print('  ...', len(hit)-12, 'more')
