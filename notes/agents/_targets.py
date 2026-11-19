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
demap={r['mangled']:r['demangled'] for r in rows}

path=r'D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c'
with open(path,'r',encoding='utf-8',errors='ignore') as f:
    text=f.read()
blocks=re.split(r'\n(?=// ==== )', text)
info={}
for b in blocks:
    m=re.match(r'// ==== ([0-9a-fA-F]+)  (.*?)  \[(.*?), (\d+) bytes\]', b)
    if not m: continue
    addr=int(m.group(1),16); size=int(m.group(4))
    names=re.findall(r'\?[^\s\"\)\,;]+', b[:4000])
    for n in names:
        if n in miss_set and n not in info:
            info[n]={'addr':addr,'size':size,'block':b,'kind_cat':m.group(3)}

# focus on very simple functions we can implement as raw-offset member/free funcs
# We'll categorize: Display, Layer, LayerManager, RenderKernel, SceneGraph, SystemResourceViewer, Overlay, LayerGroup, NonRenderContext, DisplaySelector, DisplaySaveBuffer, SharedContext, SystemResourceManager, ClearLayer, global Jet funcs
targets={}
for n,it in info.items():
    if n.startswith('??_7') or n.startswith('??_8'): continue
    d=demap.get(n,'')
    if '(' not in d and not n.startswith('??'): continue
    if it['size']>30: continue
    # extract class
    m=re.search(r'@([A-Za-z0-9_\$]+)@Jet@@', n)
    cls=m.group(1) if m else None
    targets.setdefault(cls, []).append((n,d,it))

# print summary
for cls,arr in sorted(targets.items(), key=lambda x:-len(x[1])):
    print(cls, len(arr))
# save
with open(r'D:\universal modder\trainz12-x64\notes\agents\_rs_small_targets.json','w',encoding='utf-8') as f:
    json.dump({str(k):[(a,b,c['addr'],c['size']) for a,b,c in v] for k,v in targets.items()}, f, indent=2)
PY
