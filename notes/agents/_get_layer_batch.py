import re
want=[
'?SetViewRect@LayerGroup@Jet@@QAAXABURect@2@@Z',
'?GetClippedGroupViewRect@LayerGroup@Jet@@QAAXAAURect@2@@Z',
'?GetGroupViewRect@LayerGroup@Jet@@QAAXAAURect@2@@Z',
'?BringToFront@Layer@Jet@@QAAXXZ',
'?BringToBack@Layer@Jet@@QAAXXZ',
'?BringToFront@LayerGroup@Jet@@QAAXPAVLayer@2@@Z',
'?BringToBack@LayerGroup@Jet@@QAAXPAVLayer@2@@Z',
'?BringToFront@LayerManager@Jet@@QAAXPAVLayer@2@@Z',
'?BringToBack@LayerManager@Jet@@QAAXPAVLayer@2@@Z',
]
path=r'D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c'
text=open(path,'r',encoding='utf-8',errors='ignore').read()
blocks=re.split(r'\n(?=// ==== )', text)
found={}
for b in blocks:
    for n in want:
        if n not in found and n in b:
            found[n]=b
out=[]
for n in want:
    if n in found:
        out.append('\n@@@'+n)
        out.extend(found[n].splitlines()[:120])
    else:
        out.append('MISSING '+n)
open(r'notes/agents/_layer_batch.txt','w',encoding='utf-8',newline='\n').write('\n'.join(out))
print(len(found), 'of', len(want))
