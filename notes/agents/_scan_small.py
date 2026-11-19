import re, json, csv

impl=set()
with open(r'D:\universal modder\trainz12-x64\src\jet\graphics\RenderSystem_7c\implemented.txt',encoding='utf-8',errors='replace') as f:
    for line in f:
        line=line.split('#',1)[0].strip()
        if not line: continue
        impl.add(line.split('=',1)[0].strip())
rows=[]
with open(r'D:\trainz12-decomp\exports\RenderSystem_7c.tsv',encoding='utf-8') as f:
    for row in csv.DictReader(f, delimiter='\t'): rows.append(row)
miss=[r for r in rows if r['mangled'] not in impl]
miss_set={r['mangled'] for r in miss}

path=r'D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c'
with open(path,'r',encoding='utf-8',errors='ignore') as f:
    text=f.read()
blocks=re.split(r'\n(?=// ==== )', text)
info={}
for b in blocks:
    m=re.match(r'// ==== ([0-9a-fA-F]+)  (.*?)  \[(.*?), (\d+) bytes\]', b)
    if not m: continue
    addr=int(m.group(1),16); size=int(m.group(4))
    names=re.findall(r'\?[^\s\"\)\,;]+', b[:2500])
    for n in names:
        if n in miss_set and n not in info:
            info[n]={'addr':addr,'size':size,'block':b,'kind':m.group(3)}

out=[]
for n in sorted(miss_set):
    if n not in info: continue
    it=info[n]
    if it['size']>30: continue
    d=[r['demangled'] for r in rows if r['mangled']==n][0]
    if n.startswith('??_7') or n.startswith('??_8'): continue
    if '(' not in d and not n.startswith('??'): continue
    # skip if body has call? crude
    body=it['block']
    calls=len(re.findall(r'\b[A-Za-z_][A-Za-z0-9_]*\s*\(', body)) 
    # count occurrences excluding function signature line roughly
    # get code after first '{'
    code = body.split('{',1)[1] if '{' in body else body
    callsites=re.findall(r'\b(?!if\b|return\b|switch\b|for\b|while\b)[A-Za-z_][A-Za-z0-9_:]*\s*\(', code)
    # remove casts-like false positives a bit
    simple_calls=[c for c in callsites if 'reinterpret_cast' not in c and 'static_cast' not in c and 'memcpy' not in c]
    out.append((len(simple_calls), it['size'], n, d, it['addr']))
out.sort()
for x in out[:200]:
    print(x[0], x[1], hex(x[4]), x[2], '::', x[3][:120])
print('TOTAL',len(out))
PY
