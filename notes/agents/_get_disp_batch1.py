import re
names = [
'?GetWindow@Display@Jet@@QBAPAVWindow@2@XZ',
'?GetOS@Display@Jet@@QBAPAVOS@2@XZ',
'?GetWindowSize@Display@Jet@@QBAABUSize@2@XZ',
'?GetDisplaySelector@Display@Jet@@QBAPAVDisplaySelector@2@XZ',
'?GetCurrentScreenMode@Display@Jet@@QBA?AW4ScreenMode@12@XZ',
'?GetScissorMode@Display@Jet@@QBA_NXZ',
'?GetViewport@Display@Jet@@QBAABURect@2@XZ',
'?GetClearColor@Display@Jet@@QBAABVColor@2@XZ',
'?GetClearDepth@Display@Jet@@QBAMXZ',
'?GetClearStencil@Display@Jet@@QBAJXZ',
'?GetCullMode@Display@Jet@@QBA?AW4DCull@2@XZ',
'?GetAlphaBlendEquation@Display@Jet@@QBA?AW4DBlendEquation@2@XZ',
'?GetFogMode@Display@Jet@@QBA?AW4DFogMode@2@XZ',
'?GetFogColor@Display@Jet@@QBAABVColor@2@XZ',
'?GetGlobalAmbient@Display@Jet@@QBAABVColor@2@XZ',
'?GetGlobalAmbientConverted@Display@Jet@@QBAKXZ',
'?GetTextureMode@Display@Jet@@UBA_NXZ',
'?GetGlobalTextureMode@Display@Jet@@UBA_NXZ',
'?GetNumStencilBits@Display@Jet@@UBAHXZ',
'?GetTextureCombinerManager@Display@Jet@@UBAPAVTextureCombinerManager@2@XZ',
'?TextureCombinerSupport@Display@Jet@@QBA_NXZ',
'?GetVertexProgramManager@Display@Jet@@UBAPAVVertexProgramManager@2@XZ',
'?VertexProgramSupport@Display@Jet@@QBA_NXZ',
'?HardwareAnimationSupport@Display@Jet@@QBA_NXZ',
'?GetVertexBufferManager@Display@Jet@@QBAPAVVertexBufferManager@2@XZ',
'?CheckPlatform@Display@Jet@@UAA_NPAVOS@2@@Z',
'?GetCurrentFrame@Display@Jet@@QBAIXZ',
'?GetNumberOfHardwareLights@Display@Jet@@QBAHXZ',
'?GetZBiasEnabled@Display@Jet@@UAA_NXZ',
'?IsBetweenBeginAndEndFrame@Display@Jet@@UAA_NXZ',
'?GetFrameStats@Display@Jet@@QBAABUDisplayFrameStats@12@XZ',
'?GetCurrentFrameStats@Display@Jet@@QBAABUDisplayFrameStats@12@XZ',
'?GetTargetSize@Display@Jet@@QBAABUSize@2@XZ',
'?GetMaxTextureCombinerStages@Display@Jet@@QAAHXZ',
'?GetMaxTextureImageUnits@Display@Jet@@QAAHXZ',
'?GetMaxTextureCoordUnits@Display@Jet@@QAAHXZ',
'?GetAlphaToCoverage@Display@Jet@@QAA_NXZ',
'?GetOutlinesMode@Display@Jet@@QBA_NXZ',
'?GetVertexNormalsMode@Display@Jet@@QBA_NXZ',
'?GetTriangleNormalsMode@Display@Jet@@QBA_NXZ',
'?GetLocalAxesMode@Display@Jet@@QBA_NXZ',
'?GetSeperateSpecularMode@Display@Jet@@QAA_NXZ',
'?GetLocalViewSpecularMode@Display@Jet@@QBA_NXZ',
'?SetOutlinesMode@Display@Jet@@QAAX_N@Z',
'?SetVertexNormalsMode@Display@Jet@@QAAX_N@Z',
'?SetTriangleNormalsMode@Display@Jet@@QAAX_N@Z',
'?SetNormalLength@Display@Jet@@QAAXM@Z',
'?SetLocalAxesMode@Display@Jet@@QAAX_N@Z',
'?SetChunkColorMode@Display@Jet@@QAAX_N@Z',
'?SetChunkVertexCountMode@Display@Jet@@QAAX_N@Z',
'?SetDynamicGeometryDisplayMode@Display@Jet@@QAAX_N@Z',
'?SetStaticGeometryDisplayMode@Display@Jet@@QAAX_N@Z',
'?BindToThread@NonRenderContext@Jet@@UAA_NXZ',
'?Initialize@NonRenderContext@Jet@@UAA_NPAVDisplay@2@@Z',
'?UnbindFromThread@NonRenderContext@Jet@@UAA_NXZ',
'?GetName@SystemResourceViewer@Jet@@QBAABVPString@2@XZ',
'?GetLayerList@LayerGroup@Jet@@QBAABV?$IList@VLayer@Jet@@@2@XZ',
'?GetViewRect@LayerGroup@Jet@@QBAABURect@2@XZ',
'?GetLayerGroup@LayerManager@Jet@@QAAPAVLayerGroup@2@XZ',
'?GetOS@RenderKernel@Jet@@QAAPAVOS@2@XZ',
'?GetSceneGraph@RenderKernel@Jet@@QAAPAVSceneGraph@2@XZ',
'?GetDisplay@RenderKernel@Jet@@QAAPAVDisplay@2@XZ',
'?GetMaterialManager@RenderKernel@Jet@@QAAPAVMaterialManager@2@XZ',
'?GetTextureManager@RenderKernel@Jet@@QAAPAVTextureManager@2@XZ',
'?GetRenderKernel@RenderKernel@Jet@@MAAPAV12@XZ',
'?IsEmpty@SceneGraph@Jet@@QBA_NXZ',
'?GetModifierTask@SceneGraph@Jet@@QAAPAVModifierUpdateTask@2@XZ',
'?SetModifierTask@SceneGraph@Jet@@QAAXPAVModifierUpdateTask@2@@Z',
'?GetType@DisplaySaveBuffer@Jet@@QBAIXZ',
'?SetViewPortMode@Overlay@Jet@@QAAX_N@Z',
]
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
        out.extend(found[n].splitlines()[:45])
    else:
        out.append('MISSING '+n)
with open(r'D:\universal modder\trainz12-x64\notes\agents\_disp_batch1.txt','w',encoding='utf-8') as f:
    f.write('\n'.join(out))
