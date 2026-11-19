import re, json

with open(r'D:\universal modder\trainz12-x64\notes\agents\_rs_small_targets.json',encoding='utf-8') as f:
    targets=json.load(f)

need=set()
# all non-Display targets and we can decide
for cls,arr in targets.items():
    for name,dem,addr,size in arr:
        need.add(name)

path=r'D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c'
with open(path,'r',encoding='utf-8',errors='ignore') as f:
    text=f.read()
blocks=re.split(r'\n(?=// ==== )', text)
found={}
for b in blocks:
    m=re.match(r'// ==== ([0-9a-fA-F]+)  (.*?)  \[(.*?), (\d+) bytes\]', b)
    if not m: continue
    names=re.findall(r'\?[^ \s\"\)\,;]+', b[:5000])
    for n in names:
        if n in need and n not in found:
            found[n]=b

for cls,arr in targets.items():
    print('\n##### CLASS',cls)
    for name,dem,addr,size in arr:
        print('\n---',name,'size',size)
        print(dem)
        b=found.get(name)
        if not b:
            print('NO BODY')
            continue
        print('\n'.join(b.splitlines()[:60]))
