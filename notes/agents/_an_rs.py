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
# classify
cats={'vtable/vbtable':[], 'data_static':[], 'virtual_member':[], 'nonvirtual_member_or_global':[]}
for r in miss:
    n=r['mangled']; d=r.get('demangled','')
    if n.startswith('??_7') or n.startswith('??_8'):
        cats['vtable/vbtable'].append(r); continue
    # data: mangled contains @@2 or @@1 as static storage? MSVC: static data member @@2, global @@3 sometimes? easier: demangled often has 'static ...' or check tokens
    # function members have @@Q/U/M/A/I... after class; data often @@2U, @@2P, @@2V, @@2W4, @@1? but need care
    # heuristic: if demangled contains '(' it's function, else data
    if '(' not in d and d and not d.startswith('const ') and not d.startswith('class ') and not d.startswith('struct ') and not d.startswith('enum '):
        # maybe data like vftable string? unlikely
        pass
    is_func = '(' in d or n.startswith('??0') or n.startswith('??1') or n.startswith('??_') or n.startswith('?')
    # better: look for calling conv in mangled: @@QAE, @@UAE (virtual), @@MAE, @@IAE, @@AAE, @@QBE, @@UBE, @@SAX/SA? static cdecl, @@YAX global cdecl etc.
    if '@@Y' in n or '@@6' in n or '@@3' in n:
        # global func or data
        if '(' in d:
            cats['nonvirtual_member_or_global'].append(r)
        else:
            cats['data_static'].append(r)
        continue
    # member
    if '@@U' in n or '@@M' in n: # virtual/protected virtual in some cases? MAA = protected virtual
        # UAA public virtual, UAE public virtual thiscall dtor-like? UBE public virtual const, MAE protected virtual
        cats['virtual_member'].append(r)
    elif '@@Q' in n or '@@I' in n or '@@A' in n or '@@S' in n:
        # QAA/QBA public, IAA protected ctor etc, AAA private, SAA/SA? static
        if '(' in d:
            cats['nonvirtual_member_or_global'].append(r)
        else:
            cats['data_static'].append(r)
    else:
        # data members: @@2...
        if '@@2' in n:
            cats['data_static'].append(r)
        elif '(' in d:
            cats['virtual_member'].append(r) # fallback
        else:
            cats['data_static'].append(r)
for k,v in cats.items():
    print(k,len(v))
# breakdown of virtual_member by class
from collections import Counter
vm=cats['virtual_member']
def cls(n):
    m=re.search(r'@([A-Za-z0-9_\$]+)@Jet@@', n)
    return m.group(1) if m else '(g)'
print('virtual by class:', Counter(cls(r['mangled']) for r in vm).most_common(20))
# show some data/vtable names
print('---data sample---')
for r in cats['data_static'][:40]:
    print(r['mangled'])
print('---vt---')
for r in cats['vtable/vbtable']:
    print(r['mangled'])
PY
