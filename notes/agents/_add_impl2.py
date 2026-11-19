path=r'src/jet/graphics/RenderSystem_7c/implemented.txt'
add=[
'# Simple Display getters batch 2',
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
]
s=open(path,'r',encoding='ascii',errors='ignore').read().rstrip('\r\n')
open(path,'w',encoding='ascii',newline='\n').write(s+'\n'+'\n'.join(add)+'\n')
print(sum(1 for l in open(path,encoding='ascii') if l.strip()))
