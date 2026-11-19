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

funcs=[]; data=[]
for name in miss:
    if name not in tsv: continue
    addr,sig = tsv[name]
    is_data = False
    if name.startswith("??_7") or name.startswith("??_8"): is_data=True
    elif "`vftable" in sig or "`vbtable" in sig: is_data=True
    elif "(" not in sig and not name.startswith("??0") and not name.startswith("??1") and not name.startswith("??2") and not name.startswith("??3"): is_data=True
    elif sig.startswith("const "): is_data=True
    elif re.search(r'static .* (?:[A-Za-z_]\w*::)?[A-Za-z_]\w*(?:\[|$)', sig) and '(' not in sig: is_data=True
    size = markers.get(addr)
    if is_data:
        data.append((addr,name,sig))
    else:
        funcs.append((size if size is not None else 10**9, addr,name,sig))

funcs.sort()
print("MISSING FUNCS", len(funcs))
print("MISSING DATA", len(data))
print("--- small funcs (<200 bytes) ---")
for s,a,n,sig in funcs:
    if s is not None and s < 200:
        print(f"{s:5} 0x{a:08x} {n}")
print("--- mid funcs 200-1000 ---")
count=0
for s,a,n,sig in funcs:
    if s is not None and 200 <= s < 1000:
        print(f"{s:5} 0x{a:08x} {n}")
        count+=1
        if count>=80: break
print("--- large funcs >=1000 ---")
count=0
for s,a,n,sig in funcs:
    if s is not None and s >= 1000 and s < 10**9:
        print(f"{s:5} 0x{a:08x} {n}")
        count+=1
        if count>=40: break
print("--- data ---")
for a,n,sig in sorted(data):
    print(f"0x{a:08x} {n}  ::  {sig[:120]}")
