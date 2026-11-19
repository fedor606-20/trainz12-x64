import re

miss = [l.strip() for l in open(r"D:\universal modder\trainz12-x64\notes\agents\RenderSystem_missing.txt",encoding="utf-8") if l.strip()]
tsv = {}
with open(r"D:\trainz12-decomp\exports\RenderSystem_7c.tsv",encoding="utf-8") as f:
    next(f)
    for line in f:
        p=line.rstrip("\n").split("\t")
        if len(p)>=4:
            tsv[p[2]]=(int(p[1],16),p[3])

decomp=r"D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c"
markers={}
with open(decomp,"r",encoding="utf-8",errors="ignore") as f:
    for line in f:
        m=re.match(r"^// ==== ([0-9a-fA-F]+)\s+.*?\[.*?,\s*(\d+)\s+bytes\]", line)
        if m:
            markers[int(m.group(1),16)]=int(m.group(2))

def is_data_export(name,sig):
    if name.startswith("??_7") or name.startswith("??_8"): return True
    if "`vftable" in sig or "`vbtable" in sig: return True
    m = re.search(r"@@([0-3])", name)
    if m and "(" not in sig:
        return True
    if sig.startswith("const ") and "(" not in sig: return True
    return False

funcs=[]
for name in miss:
    if name not in tsv: continue
    addr,sig = tsv[name]
    if is_data_export(name,sig): continue
    size = markers.get(addr, -1)
    funcs.append((size, addr,name,sig))

# classify
buckets={}
for s,a,n,sig in funcs:
    # try to extract class
    cls='::global::'
    m=re.search(r'@([A-Za-z_]\w*(?:@@[A-Za-z_]\w*)*)@@', n) # rough
    # better: look in sig
    m2=re.search(r'(?:class|struct|enum)\s+Jet::([A-Za-z0-9_:]+)', sig)
    if m2:
        cls=m2.group(1).split('::')[0]
    else:
        m3=re.search(r'Jet::([A-Za-z0-9_]+)::', sig)
        if m3: cls=m3.group(1)
        elif 'JetGetPluginsRenderSystem' in n: cls='Plugin'
        elif n.startswith('?Convert@Jet') or n.startswith('?Repitch@Jet') or n.startswith('?FlushScratchBuffer@Jet'): cls='Jet free'
    buckets.setdefault(cls, []).append((s,a,n,sig))

for cls,items in sorted(buckets.items(), key=lambda x: -len(x[1])):
    total=sum(s for s,_,_,_ in items if s>0)
    unk=sum(1 for s,_,_,_ in items if s<0)
    print(f"{len(items):3} {cls:30} bytes~{total:6} unk={unk}")
    # show a few
    for s,a,n,sig in sorted(items)[:3]:
        print(f"      {s:6} 0x{a:05x} {n}")
