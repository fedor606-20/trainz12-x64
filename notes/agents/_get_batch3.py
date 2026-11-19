import re
want=[
'?SetZBiasValues@Display@Jet@@UAAXMM@Z',
'?LoadProjectionMatrix@Display@Jet@@UAAXABVMatrix4@2@@Z',
'?FlushState@Display@Jet@@UAEXXZ',
'?BeginFrame@Display@Jet@@UAAXM@Z',
'?EndFrame@Display@Jet@@UAAXM@Z',
'?ResetDynamicBufferOffsets@Display@Jet@@IAAXXZ',
'?CheckResetDynamicBufferOffsets@Display@Jet@@QAAXXZ',
'?AllocateStaticVertexBuffer@Display@Jet@@UAAPBVStaticBuffer@2@II@Z',
'?AllocateStaticIndexBuffer@Display@Jet@@UAAPBVStaticBuffer@2@I_N@Z',
'?GetBuiltInVertexType@Display@Jet@@UBAPBVVertexType@2@W4VertexTypeID@2@@Z',
'?GetExtensionString@Display@Jet@@QBAPBDW4DExtension@2@@Z',
'?CreateTexture@Display@Jet@@UAAPAVDisplayTexture@2@XZ',
'?CreateTextureCombiner@Display@Jet@@UAAPAVTextureCombiner@2@XZ',
'?EndLightingUpdate@Display@Jet@@UAAXXZ',
'?InitializeVertexTypeForAPI@Display@Jet@@UBAXAAVVertexType@2@PBXPBD@Z',
'?IsGroup@Layer@Jet@@QBA_NXZ',
'?Render@LayerGroup@Jet@@MAAXXZ',
'?Reset@LayerManager@Jet@@QAAXXZ',
'?AddSystemResourceViewer@LayerManager@Jet@@QAAXPAVSystemResourceViewer@2@@Z',
'?GetLayerAt@LayerManager@Jet@@QAAPAVLayer@2@ABVVector2@2@@Z',
'?FindLayer@LayerManager@Jet@@QAAPAVLayer@2@ABVPString@2@@Z',
'?GetLayerUnder@LayerManager@Jet@@QAAPAVLayer@2@ABVVector2@2@0@Z',
'?AddScene@SceneGraph@Jet@@QAAXPAVElement@2@@Z',
'?DelayedDelete@SceneGraph@Jet@@QAAXPAVElement@2@@Z',
'?ConvertToWindow@Layer@Jet@@QBAXAAUPoint@2@@Z',
'?ConvertToWindow@LayerGroup@Jet@@QBAXAAUPoint@2@@Z',
'?ConvertToWindowAndClip@Layer@Jet@@QBAXAAURect@2@@Z',
'?ConvertToWindowAndClip@LayerGroup@Jet@@QBAXAAURect@2@@Z',
'?GetAllVideoModes@Display@Jet@@QBA_NAAPBV?$Array@UVideoMode@Display@Jet@@$0BE@@2@@Z',
'?GetAllVideoFormats@Display@Jet@@QBA_NAAPBV?$Array@UVideoFormat@Display@Jet@@$0BE@@2@@Z',
'?ConvertTexCompressionString@Display@Jet@@QBAPBDW4DTexCompression@2@@Z',
'?ConvertTexCompressionString@Display@Jet@@QBA?AW4DTexCompression@2@PBD@Z',
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
open(r'notes/agents/_batch3.txt','w',encoding='utf-8',newline='\n').write('\n'.join(out))
print(len(found), 'of', len(want))
