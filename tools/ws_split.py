import re, os, collections
src=open(r'D:\trainz12-decomp\decomp\WaterSystem_7c.dll.c',encoding='utf-8').read()
blocks=re.split(r'(?=^// ==== )',src,flags=re.M)
hdr=re.compile(r'^// ==== ([0-9a-f]+)\s+(.*?)\s+\[(\w+), (\d+) bytes\]',re.M)
funcs=[]
for b in blocks:
    m=hdr.match(b)
    if m: funcs.append((m.group(1),m.group(2),int(m.group(4)),b))
print('func blocks',len(funcs))
# exports
exports=[l.split('\t') for l in open(r'D:\trainz12-decomp\exports\WaterSystem_7c.tsv',encoding='utf-8-sig').read().splitlines()[1:] if l.strip()]
byname={f[1]:f for f in funcs}
byaddr={f[0]:f for f in funcs}
# gather class for each export via mangled demangled; use name from mangled class token
def cls_from_dem(d):
    m=re.search(r'Jet::([A-Za-z0-9_]+)::',d)
    if m: return m.group(1)
    m=re.search(r'\bJet::([A-Za-z0-9_]+)\(',d)
    if m: return m.group(1)
    return 'Global'
classes=collections.defaultdict(lambda:{'exports':[],'funcs':[]})
impl=open(r'src/jet/graphics/WaterSystem_7c/implemented.txt',encoding='utf-8').read().splitlines()
local=set(l.split('=')[0].strip() for l in impl if l.strip() and not l.startswith('#'))
for e in exports:
    ord_,rva,mang,dem=e
    c=cls_from_dem(dem)
    classes[c]['exports'].append(e)
# assign funcs to class by name token
for f in funcs:
    name=f[1]
    c=None
    for cc in classes:
        if '::'+cc+'::' in name:
            c=cc; break
    if c: classes[c]['funcs'].append(f)
outdir='notes/agents/ws_workstreams'
os.makedirs(outdir,exist_ok=True)
for c,v in classes.items():
    # always write for target classes
    if not v['funcs'] and not v['exports']: continue
    with open(f'{outdir}/{c}_decomp.c','w',encoding='utf-8') as fh:
        for f in sorted(v['funcs']): fh.write(f[3])
    with open(f'{outdir}/{c}_exports.tsv','w',encoding='utf-8') as fh:
        for e in v['exports']:
            fh.write('\t'.join(e)+('\tLOCAL' if e[2] in local else '\tTODO')+'\n')
for c,v in sorted(classes.items(), key=lambda kv:-len(kv[1]['exports'])):
    if v['exports']:
        print(f'{c:40s} funcs={len(v["funcs"]):4d} exports={len(v["exports"]):4d}')
print('files written',outdir)
