import re,collections,os
tsv=[l.split('\t') for l in open(r'D:\trainz12-decomp\exports\Render3DCore_7c.tsv',encoding='utf-8-sig').read().splitlines()[1:] if l.strip()]
local=set()
base='src/jet/graphics/Render3DCore_7c/'
for f in [base+'implemented.txt']+[base+'implemented.d/'+x for x in os.listdir(base+'implemented.d') if x.endswith('.txt')]:
    for l in open(f,encoding='utf-8'):
        l=l.strip()
        if l and not l.startswith('#'): local.add(l.split('=',1)[0].strip())
def cls(d):
    m=re.search(r'Jet::([A-Za-z0-9_]+)::',d)
    return m.group(1) if m else '?'
rem=[x for x in tsv if x[2] not in local]
c=collections.Counter(cls(x[3]) for x in rem)
print('remaining',len(rem))
for name,n in c.most_common(50): print(f'{name:38s} {n}')
