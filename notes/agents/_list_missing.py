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
for r in missing[:200]:
    print(r['rva'], r['mangled'], '||', r['demangled'])
