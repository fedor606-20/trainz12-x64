import re, json, csv

# load missing
impl=set()
with open(r'D:\universal modder\trainz12-x64\src\jet\graphics\RenderSystem_7c\implemented.txt',encoding='utf-8',errors='replace') as f:
    for line in f:
        line=line.split('#',1)[0].strip()
        if not line: continue
        impl.add(line.split('=',1)[0].strip())
rows=[]
with open(r'D:\trainz12-decomp\exports\RenderSystem_7c.tsv',encoding='utf-8') as f:
    for row in csv.DictReader(f, delimiter='\t'): rows.append(row)
miss=[r['mangled'] for r in rows if r['mangled'] not in impl]
miss_set=set(miss)

path=r'D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c'
with open(path,'r',encoding='utf-8',errors='ignore') as f:
    text=f.read()
blocks=re.split(r'\n(?=// ==== )', text)
info={}
for b in blocks:
    m=re.match(r'// ==== ([0-9a-fA-F]+)  (.*?)  \[(.*?), (\d+) bytes\]', b)
    if not m: continue
    addr=int(m.group(1),16); size=int(m.group(4))
    names=re.findall(r'\?[^\s\"\)\,]+', b[:1500])
    for n in names:
        if n in miss_set and n not in info:
            info[n]={'addr':addr,'size':size,'block':b}

# buckets
small_funcs=[] # <=40 bytes, not data/vt
for n in miss:
    if n not in info: 
        continue
    it=info[n]
    if n.startswith('??_7') or n.startswith('??_8'): continue
    # data?
    d=[r['demangled'] for r in rows if r['mangled']==n][0]
    if '(' not in d and not n.startswith('??'): 
        continue
    if it['size'] <= 40:
        small_funcs.append((it['size'], it['addr'], n, it['block']))
small_funcs.sort()
print('SMALL',len(small_funcs))
for sz,addr,n,b in small_funcs[:220]:
    print('\n###',hex(addr),sz,n)
    print('\n'.join(b.splitlines()[:45]))
PY
