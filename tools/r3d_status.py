import os,re,collections
# total per class from TSV
tsv=[l.split('\t') for l in open(r'D:\trainz12-decomp\exports\Render3DCore_7c.tsv',encoding='utf-8-sig').read().splitlines()[1:] if l.strip()]
def cls(d):
    m=re.search(r'Jet::([A-Za-z0-9_]+)::',d)
    return m.group(1) if m else '?'
allc=collections.Counter(cls(x[3]) for x in tsv)
# local per fragment
for f in sorted(os.listdir('src/jet/graphics/Render3DCore_7c/implemented.d')):
    if not f.endswith('.txt'): continue
    names=[l.strip() for l in open('src/jet/graphics/Render3DCore_7c/implemented.d/'+f,encoding='utf-8') if l.strip() and not l.startswith('#')]
    print(f, len(names))
print('total tsv',len(tsv))
