import re,os
tsv=[l.split('\t') for l in open(r'D:\trainz12-decomp\exports\Render3DCore_7c.tsv',encoding='utf-8-sig').read().splitlines()[1:] if l.strip()]
local=set(); base='src/jet/graphics/Render3DCore_7c/'
for f in [base+'implemented.txt']+[base+'implemented.d/'+x for x in os.listdir(base+'implemented.d') if x.endswith('.txt')]:
    for l in open(f,encoding='utf-8'):
        l=l.strip()
        if l and not l.startswith('#'): local.add(l.split('=',1)[0].strip())
rem=[x for x in tsv if x[2] not in local]
for x in rem:
    d=x[3]
    if 'Jet::' not in d or '::' not in d.split('Jet::',1)[1]:
        print(x[0],x[1],d[:140])
