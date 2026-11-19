import csv, re
# build missing list
impl=set()
for l in open(r'src/jet/graphics/RenderSystem_7c/implemented.txt',encoding='ascii',errors='ignore'):
    l=l.strip()
    if not l or l.startswith('#'): continue
    impl.add(l.split('=',1)[0])
want=[]
with open(r'D:\trainz12-decomp\exports\RenderSystem_7c.tsv',encoding='utf-8') as f:
    for row in csv.DictReader(f,delimiter='\t'):
        if row['mangled'] not in impl:
            want.append(row['mangled'])
wantset=set(want)
path=r'D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c'
text=open(path,'r',encoding='utf-8',errors='ignore').read()
blocks=re.split(r'\n(?=// ==== )', text)
found={}
for b in blocks:
    # check first few lines for name match
    head='\n'.join(b.splitlines()[:8])
    for n in list(wantset):
        if n in head:
            found[n]=b
            wantset.remove(n)
            break
    else:
        # fallback: sometimes name appears later in signature line
        for n in list(wantset):
            if n in b[:500]:
                found[n]=b
                wantset.remove(n)
                break
out=[]
miss=[]
for n in want:
    if n in found:
        out.append('\n@@@'+n)
        out.extend(found[n].splitlines()[:200])
    else:
        miss.append(n)
open(r'notes/agents/_all_missing_bodies.txt','w',encoding='utf-8',newline='\n').write('\n'.join(out))
open(r'notes/agents/_all_missing_no_body.txt','w',encoding='utf-8',newline='\n').write('\n'.join(miss))
print('found',len(found),'miss',len(miss))
