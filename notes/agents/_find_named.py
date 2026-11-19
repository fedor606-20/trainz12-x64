import re, csv
text=open(r'D:/trainz12-decomp/decomp/RenderSystem_7c.dll.c','r',encoding='utf-8',errors='ignore').read()
# by export name
names = [
 '?IsNormalized2DViewport@Display@Jet@@QBA_NXZ',
 '?GetCurrentVideoMode@Display@Jet@@QBA_NAAUVideoMode@12@@Z',
 '?GetViewModeParams@Display@Jet@@QBAXAAUViewModeParams@12@@Z',
 '?SetViewModeParams@Display@Jet@@QAAXABUViewModeParams@12@@Z',
 '?GetFogRange@Display@Jet@@QBAXAAM0@Z',
 '?SetFogRange@Display@Jet@@QAAXMM@Z',
 '?GetScissor@Display@Jet@@QAA?AURect@2@XZ',
 '?SetScissor@Display@Jet@@QAAXABURect@2@@Z',
 '?SetViewport@Display@Jet@@QAAXABURect@2@@Z',
 '?GetLightMode@Display@Jet@@QBA_NH@Z',
 '?Set2DView@Display@Jet@@QAAXMMMMMM@Z',
 '?Set2DViewNormalized@Display@Jet@@QAAX_N@Z',
 '?Set3DView@Display@Jet@@QAAXMMMM@Z',
 '?Set3DViewOffset@Display@Jet@@QAAXMMMMM@Z',
 '?Set3DTransitionalView@Display@Jet@@QAAXMMMMMMM@Z',
 '?GetExtensionString@Display@Jet@@QBAPBDW4DExtension@2@@Z',
 '?ConvertTexCompressionString@Display@Jet@@QBAPBDW4DTexCompression@2@@Z',
 '?ConvertTexCompressionString@Display@Jet@@QBA?AW4DTexCompression@2@PBD@Z',
 '?FlushScratchBuffer@Jet@@YAXXZ',
 '?Render@LayerGroup@Jet@@MAAXXZ',
 '?SetName@SystemResourceViewer@Jet@@QAAXABVPString@2@@Z',
 '?GetClearColor@ClearLayer@Jet@@QAA?AVColor@2@XZ',
 '?SetClearColor@ClearLayer@Jet@@QAAXABVColor@2@@Z',
 '?SetZBiasEnabled@Display@Jet@@UAAX_N@Z',
 '?EndFrame@Display@Jet@@UAAXM@Z',
 '?BeginFrame@Display@Jet@@UAAXM@Z',
 '?FlushState@Display@Jet@@UAEXXZ',
]
for b in re.split(r'\n(?=// ==== )', text):
    head=b[:400]
    for n in names:
        if n in head:
            print('---NAME:'+n+'---')
            print(b[:2000])
            print('===END===')
            break
