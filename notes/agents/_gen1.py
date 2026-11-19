import re, json, csv, textwrap

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
    names=re.findall(r'\?[^\s\"\)\,;]+', b[:3000])
    for n in names:
        if n in miss_set and n not in info:
            info[n]={'addr':addr,'size':size,'block':b,'kind':m.group(3),'title':m.group(2)}

# generate implementations for very simple patterns
# 1) return *(T*)(this_or_param + off) or state-indirect, 2) *(..)=val, 3) return false/true/0/null, 4) empty void
# We'll do by analyzing decomp body text.
def gen():
    impls=[]
    done=set()
    for n,it in info.items():
        if n in done: continue
        if n.startswith('??_7') or n.startswith('??_8'): continue
        d=demap[n]
        if '(' not in d and not n.startswith('??'): continue
        b=it['block']
        # skip complex
        if 'switch' in b or 'for' in b or 'while' in b or 'goto' in b: continue
        # get body
        if '\n{\n' in b:
            body=b.split('\n{\n',1)[1]
        else:
            continue
        # remove comments lines at start
        blines=body.splitlines()
        # find return lines
        code='\n'.join([ln for ln in blines if not ln.strip().startswith('/*') and '*/' not in ln and not ln.strip().startswith('//')])
        # heuristic: simple return of constant/field or simple assignment
        # identify class and member
        # but we need C++ signature to get mangling? MSVC mangling is produced by compiler for definition matching declaration/signature.
        # easier: define as extern "C"? no, mangled names are C++ exports; we can define with correct signature in namespace/class.
        # hard part: exact signature types. But decomp gives commented demangled signature in many cases.
        # extract demangled signature line
        dm_line=None
        for ln in b.splitlines()[:15]:
            if ln.strip().startswith('/*') and '__' in ln:
                dm_line=ln.strip().strip('/*').strip().strip('*/').strip()
                break
        if not dm_line:
            # try next
            for ln in b.splitlines()[:20]:
                if ln.strip().startswith('/* public:') or ln.strip().startswith('/* protected:') or ln.strip().startswith('/* private:'):
                    dm_line=ln.strip().strip('/*').strip().strip('*/').strip()
                    break
        if not dm_line:
            continue
        impls.append((n,d,dm_line,it['addr'],it['size'],b))
    return impls

impls=gen()
print(len(impls))
# show a few
for x in impls[:20]:
    print('---',x[0])
    print(x[2])
PY
