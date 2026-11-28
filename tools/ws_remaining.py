import re, collections
tsv=[l.split('\t') for l in open(r'D:\trainz12-decomp\exports\WaterSystem_7c.tsv',encoding='utf-8-sig').read().splitlines()[1:] if l.strip()]
impl=open(r'src/jet/graphics/WaterSystem_7c/implemented.txt',encoding='utf-8').read().splitlines()
local=set()
for l in impl:
    l=l.strip()
    if not l or l.startswith('#'): continue
    local.add(l.split('=')[0].strip())
def cls(d):
    m=re.search(r'Jet::([A-Za-z0-9_]+)::',d)
    if m: return m.group(1)
    m=re.search(r'\bJet::([A-Za-z0-9_]+)\(',d)
    if m: return m.group(1)
    return '?'
groups=collections.defaultdict(lambda:[0,0])
rem=collections.defaultdict(list)
for ord_,rva,mang,dem in tsv:
    c=cls(dem)
    groups[c][0]+=1
    if mang not in local:
        groups[c][1]+=1
        rem[c].append((ord_,rva,mang,dem))
print('exports',len(tsv),'local',sum(1 for t in tsv if t[2] in local),'remaining',sum(1 for t in tsv if t[2] not in local))
for c in sorted(groups, key=lambda c:-groups[c][1]):
    if groups[c][1]: print(f'{c:40s} {groups[c][0]:4d} total {groups[c][1]:4d} remaining')
with open('notes/ws_remaining.txt','w',encoding='utf-8') as f:
    for c in sorted(rem):
        f.write(f'==== {c} ({len(rem[c])})\n')
        for ord_,rva,mang,dem in rem[c]:
            f.write(f'{ord_}\t{rva}\t{mang}\t{dem}\n')
print('written notes/ws_remaining.txt')
