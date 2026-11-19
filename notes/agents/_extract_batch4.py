names = [
"?SetAntiAliasing@Display@Jet@@QAAX_N@Z",
"?SetStencilTestMode@Display@Jet@@QAAX_N0@Z",
"?SetStencilFunction@Display@Jet@@QAAXW4DFunction@2@HKW4DFace@2@@Z",
"?SetStencilOperation@Display@Jet@@QAAXW4DOperation@2@00W4DFace@2@@Z",
"?SetStencilSetup@Display@Jet@@QAAXW4DFunction@2@HKW4DOperation@2@11W4DFace@2@@Z",
"?SetTextureCoordGen@Display@Jet@@QAAXHW4DTexGenMode@2@I@Z",
"?SetGamma@Display@Jet@@QAAXM_N@Z",
"?SetMaxAnisotropy@Display@Jet@@UAAXM@Z",
"?SetClipping@Overlay@Jet@@QAAXABURect@2@@Z",
"?GetClipping@Overlay@Jet@@QBA?AURect@2@XZ",
"?SetBlendMode@Material@Jet@@QAAXW4Factor@Src@2@W43Dst@2@@Z",
]
import re
text=open(r'D:/trainz12-decomp/decomp/RenderSystem_7c.dll.c','r',encoding='utf-8',errors='ignore').read()
for b in re.split(r'\n(?=// ==== )', text):
    head=b[:400]
    for n in names:
        if n in head:
            print('====', n)
            print(b[:1200])
            print()
            names.remove(n)
            break
