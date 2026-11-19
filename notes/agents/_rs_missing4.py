import re

miss = [l.strip() for l in open(r"D:\universal modder\trainz12-x64\notes\agents\RenderSystem_missing.txt",encoding="utf-8") if l.strip()]
tsv = {}
with open(r"D:\trainz12-decomp\exports\RenderSystem_7c.tsv",encoding="utf-8") as f:
    next(f)
    for line in f:
        p=line.rstrip("\n").split("\t")
        if len(p)>=4:
            tsv[p[2]]=(int(p[1],16),p[3])

markers={}
with open(r"D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c","r",encoding="utf-8",errors="ignore") as f:
    for line in f:
        m=re.match(r"^// ==== ([0-9a-fA-F]+)\s+.*?\[.*?,\s*(\d+)\s+bytes\]", line)
        if m:
            markers[int(m.group(1),16)]=int(m.group(2))

def is_data_export(name,sig):
    if name.startswith("??_7") or name.startswith("??_8"): return True
    if "`vftable" in sig or "`vbtable" in sig: return True
    if re.search(r"@@([0-3])", name) and "(" not in sig:
        return True
    if sig.startswith("const ") and "(" not in sig: return True
    return False

funcs=[]; data=[]
for name in miss:
    if name not in tsv: continue
    addr,sig = tsv[name]
    if is_data_export(name,sig):
        data.append((addr,name,sig))
    else:
        size = markers.get(addr+0x10000000, -1)
        funcs.append((size, addr,name,sig))

funcs.sort()
print("MISSING FUNCS", len(funcs), "DATA", len(data))
for lo,hi,label in [(0,20,'<20'),(20,60,'20-59'),(60,150,'60-149'),(150,400,'150-399'),(400,1200,'400-1199'),(1200,10**9,'>=1200'),(-1,0,'unk')]:
    sel=[x for x in funcs if (x[0]>=lo and x[0]<hi) or (label=='unk' and x[0]==-1)]
    print(label, len(sel))
print("--- <150 ---")
for s,a,n,sig in funcs:
    if 0 <= s < 150:
        print(f"{s:5} 0x{a:05x} {n} :: {sig[:90]}")
