import csv
impl=set()
for l in open(r'src/jet/graphics/RenderSystem_7c/implemented.txt',encoding='ascii',errors='ignore'):
    l=l.strip()
    if not l or l.startswith('#'): continue
    impl.add(l.split('=',1)[0])
missing=[]
with open(r'D:\trainz12-decomp\exports\RenderSystem_7c.tsv',encoding='utf-8') as f:
    for row in csv.DictReader(f,delimiter='\t'):
        if row['mangled'] not in impl:
            missing.append(row)
open(r'notes/agents/rs_missing_now.txt','w',encoding='utf-8',newline='\n').write('\n'.join(r['mangled'] for r in missing))
print(len(missing))
# categorize
cats={}
for r in missing:
    d=r['demangled']
    # rough categories
    if '::`vftable' in d or "vftable'" in d or 'vbtable' in d: k='vtable'
    elif d.startswith('class ') or d.startswith('struct ') or d.startswith('enum '): k='data/typeinfo?'
    elif '??_G' in r['mangled'] or '??_E' in r['mangled']: k='vector deleting dtor helpers'
    elif '??1' in r['mangled']: k='dtor'
    elif '??0' in r['mangled']: k='ctor'
    elif '??4' in r['mangled'] or 'operator=' in d: k='assign'
    elif '??_7' in r['mangled']: k='vftable_data'
    elif d.startswith('public: virtual') or d.startswith('protected: virtual'): k='virtual'
    else: k='nonvirtual/other'
    cats[k]=cats.get(k,0)+1
for k,v in sorted(cats.items(), key=lambda x:-x[1]):
    print(k,v)
