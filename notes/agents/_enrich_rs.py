import re, json, csv, os

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
    names=re.findall(r'\?[^\s\"\)\,;]+', b[:2000])
    for n in names:
        if n in miss_set and n not in info:
            info[n]={'addr':addr,'size':size,'block':b}

out=[]
for r in miss:
    n=r['mangled']; d=r.get('demangled','')
    it=info.get(n)
    kind='data'
    if n.startswith('??_7'): kind='vtable'
    elif n.startswith('??_8'): kind='vbtable'
    elif '(' in d or n.startswith('??0') or n.startswith('??1') or n.startswith('??2') or n.startswith('??3'): kind='func'
    else:
        # could be data
        if '@@2' in n or '@@1' in n or '@@3' in n: kind='data'
        else: kind='func'
    virtual = (' virtual ' in (' '+d)) or ('@@U' in n) or ('@@M' in n)
    cls_m=re.search(r'@([A-Za-z0-9_\$]+)@Jet@@', n)
    cls=cls_m.group(1) if cls_m else None
    out.append({
        'name':n,'demangled':d,'ordinal':r['ordinal'],'rva':r['rva'],'kind':kind,'virtual':virtual,'cls':cls,
        'addr': it['addr'] if it else None, 'size': it['size'] if it else None
    })
with open(r'D:\universal modder\trainz12-x64\notes\agents\rs_missing_enriched.json','w',encoding='utf-8') as f:
    json.dump(out,f,indent=2)
# summary
from collections import Counter
print('total',len(out))
print(Counter(o['kind'] for o in out))
print('with_body',sum(1 for o in out if o['addr'] is not None))
print('virtual_funcs',sum(1 for o in out if o['kind']=='func' and o['virtual']))
print('nonvirtual_funcs',sum(1 for o in out if o['kind']=='func' and not o['virtual']))
print('small_nonvirt<=24',sum(1 for o in out if o['kind']=='func' and not o['virtual'] and o['size'] is not None and o['size']<=24))
PY
