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
    if name.startswith('??_7') or name.startswith('??_8'): return True
    if '`vftable' in sig or '`vbtable' in sig: return True
    # static data members: @@0, @@1, @@2, @@3 followed by type encoding in many cases
    # but be careful: functions have @@Q, @@U, @@Y, @@S? static methods SA/...
    # data members pattern: ...@@<access><type>  e.g. 0W4, 1I, 2H, 2_N, 2P, 2V, 1V
    m = re.search(r'@@([0-3])', name)
    if m and '(' not in sig:
        # likely static data member (and not a function - functions with () won't match? ctor/dtor have () but start ??0/??1)
        # but there could be edge cases; also check sig not having parens
        return True
    if sig.startswith('const ') and '(' not in sig: return True
    return False

funcs=[]; data=[]
for name in miss:
    if name not in tsv: continue
    addr,sig = tsv[name]
    if is_data_export(name,sig):
        data.append((addr,name,sig))
    else:
        size = markers.get(addr)
        funcs.append((size if size is not None else 10**9, addr,name,sig))

funcs.sort()
print("MISSING FUNCS", len(funcs))
print("MISSING DATA", len(data))
print("--- tiny funcs < 40 ---")
for s,a,n,sig in funcs:
    if s < 40:
        print(f"{s:5} 0x{a:08x} {n}")
print("--- small funcs 40-149 ---")
c=0
for s,a,n,sig in funcs:
    if 40 <= s < 150:
        print(f"{s:5} 0x{a:08x} {n}")
        c+=1
        if c>=120: break
print("--- medium 150-600 ---")
c=0
for s,a,n,sig in funcs:
    if 150 <= s < 600:
        print(f"{s:5} 0x{a:08x} {n}")
        c+=120
print("--- data list ---")
for a,n,sig in sorted(data):
    print(f"D 0x{a:08x} {n}")
