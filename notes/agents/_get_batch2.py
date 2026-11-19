import re
names = [
'?SetZBiasValues@Display@Jet@@UAAXMM@Z',
'?LoadProjectionMatrix@Display@Jet@@UAAXABVMatrix4@2@@Z',
'?FlushState@Display@Jet@@UAEXXZ',
'?CreateTexture@Display@Jet@@UAAPAVDisplayTexture@2@XZ',
'?CreateTextureCombiner@Display@Jet@@UAAPAVTextureCombiner@2@XZ',
'?EndLightingUpdate@Display@Jet@@UAAXXZ',
'?InitializeVertexTypeForAPI@Display@Jet@@UBAXAAVVertexType@2@PBXPBD@Z',
'?GetBuiltInVertexType@Display@Jet@@UBAPBVVertexType@2@W4VertexTypeID@2@@Z',
'?GetExtensionString@Display@Jet@@QBAPBDW4DExtension@2@@Z',
'?SupportsAntiAliasing@Display@Jet@@QBA_NXZ',
'?GetAntiAliasing@Display@Jet@@QBA_NXZ',
'?GetAlphaBlendFunction@Display@Jet@@QBAXAAW4Factor@Src@2@AAW43Dst@2@@Z',
'?GetColorWriteMask@Display@Jet@@QBAKXZ',
'?GetDepthWriteMode@Display@Jet@@QBA_NXZ',
'?GetDepthTestMode@Display@Jet@@QBA_NXZ',
'?GetDepthFunction@Display@Jet@@QBA?AW4DFunction@2@XZ',
'?GetDepthClamp@Display@Jet@@QBA_NXZ',
'?GetAlphaTestMode@Display@Jet@@QBA_NXZ',
'?GetAlphaTestReference@Display@Jet@@QBAMXZ',
'?GetAlphaTestFunction@Display@Jet@@QBA?AW4DFunction@2@XZ',
'?GetAlphaBlendMode@Display@Jet@@QBA_NXZ',
'?GetLightingMode@Display@Jet@@QBA_NXZ',
'?GetFogDensity@Display@Jet@@QBAMXZ',
'?GetColorMaterialMode@Display@Jet@@QAA?AW4DColorMaterialTypes@2@XZ',
'?GetVertexWeightMode@Display@Jet@@QBA_NXZ',
'?GetStencilTestMode@Display@Jet@@QBA_NXZ',
'?GetStencilWriteMask@Display@Jet@@QBAKW4DFace@2@@Z',
'?GetWireFrameMode@Display@Jet@@QBA_NXZ',
'?BeginFrame@Display@Jet@@UAAXM@Z',
'?EndFrame@Display@Jet@@UAAXM@Z',
'?ResetDynamicBufferOffsets@Display@Jet@@IAAXXZ',
'?AllocateStaticVertexBuffer@Display@Jet@@UAAPBVStaticBuffer@2@II@Z',
'?AllocateStaticIndexBuffer@Display@Jet@@UAAPBVStaticBuffer@2@I_N@Z',
'?GetAllVideoModes@Display@Jet@@QBA_NAAPBV?$Array@UVideoMode@Display@Jet@@$0BE@@2@@Z',
'?GetAllVideoFormats@Display@Jet@@QBA_NAAPBV?$Array@UVideoFormat@Display@Jet@@$0BE@@2@@Z',
'?ConvertTexCompressionString@Display@Jet@@QBAPBDW4DTexCompression@2@@Z',
'?ConvertTexCompressionString@Display@Jet@@QBA?AW4DTexCompression@2@PBD@Z',
'?Render@LayerGroup@Jet@@MAAXXZ',
'?IsGroup@Layer@Jet@@QBA_NXZ',
'?ConvertToWindow@Layer@Jet@@QBAXAAUPoint@2@@Z',
'?ConvertToWindowAndClip@Layer@Jet@@QBAXAAURect@2@@Z',
'?AddScene@SceneGraph@Jet@@QAAXPAVElement@2@@Z',
'?DelayedDelete@SceneGraph@Jet@@QAAXPAVElement@2@@Z',
'?Reset@LayerManager@Jet@@QAAXXZ',
'?AddSystemResourceViewer@LayerManager@Jet@@QAAXPAVSystemResourceViewer@2@@Z',
'?FindLayer@LayerManager@Jet@@QAAPAVLayer@2@ABVPString@2@@Z',
'?GetLayerAt@LayerManager@Jet@@QAAPAVLayer@2@ABVVector2@2@@Z',
'?GetLayerUnder@LayerManager@Jet@@QAAPAVLayer@2@ABVVector2@2@0@Z',
'?GetColorWriteMask@Display@Jet@@QBAKXZ',
]
# dedupe
seen=set(); names2=[]
for n in names:
    if n not in seen: seen.add(n); names2.append(n)
names=names2
path=r'D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c'
with open(path,'r',encoding='utf-8',errors='ignore') as f:
    text=f.read()
blocks=re.split(r'\n(?=// ==== )', text)
found={}
for b in blocks:
    for n in names:
        if n not in found and n in b:
            found[n]=b
print('FOUND',len(found),'OF',len(names))
out=[]
for n in names:
    if n in found:
        out.append('\n@@@'+n)
        out.extend(found[n].splitlines()[:70])
    else:
        out.append('MISSING '+n)
with open(r'D:\universal modder\trainz12-x64\notes\agents\_batch2.txt','w',encoding='utf-8') as f:
    f.write('\n'.join(out))
