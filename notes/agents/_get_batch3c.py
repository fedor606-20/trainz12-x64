import re
want=[
'?ConvertToWindowAndClip@LayerGroup@Jet@@QBAXAAURect@2@@Z',
'?ConvertToWindow@LayerGroup@Jet@@QBAXAAUPoint@2@@Z',
'?ConvertToWindow@Layer@Jet@@QBAXAAUPoint@2@@Z',
'?ConvertToWindowAndClip@Layer@Jet@@QBAXAAURect@2@@Z',
'?FindLayer@LayerGroup@Jet@@QAAPAVLayer@2@ABVPString@2@@Z',
'?GetLayerAt@LayerGroup@Jet@@QAAPAVLayer@2@ABVVector2@2@@Z',
'?GetLayerUnder@LayerGroup@Jet@@QAAPAVLayer@2@ABVVector2@2@0@Z',
'?Reset@LayerGroup@Jet@@UAAXXZ',
'?Render@LayerGroup@Jet@@MAAXXZ',
'?GetLayerAt@LayerManager@Jet@@QAAPAVLayer@2@ABVVector2@2@@Z',
'?FindLayer@LayerManager@Jet@@QAAPAVLayer@2@ABVPString@2@@Z',
'?GetLayerUnder@LayerManager@Jet@@QAAPAVLayer@2@ABVVector2@2@0@Z',
'?Reset@LayerManager@Jet@@QAAXXZ',
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
        out.extend(found[n].splitlines()[:140])
    else:
        out.append('MISSING '+n)
open(r'notes/agents/_batch3c.txt','w',encoding='utf-8',newline='\n').write('\n'.join(out))
print(len(found), 'of', len(want))
