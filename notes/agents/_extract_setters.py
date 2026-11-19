import re
text=open(r'D:/trainz12-decomp/decomp/RenderSystem_7c.dll.c','r',encoding='utf-8',errors='ignore').read()
names=['?SetClearDepth@Display@Jet@@QAAXM@Z','?SetFogDensity@Display@Jet@@QAAXM@Z','?SetFogMode@Display@Jet@@QAAXW4DFogMode@2@@Z','?SetColorWriteMask@Display@Jet@@QAAXK@Z','?SetDepthFunction@Display@Jet@@QAAXW4DFunction@2@@Z','?SetSeperateSpecularMode@Display@Jet@@QAAX_N@Z','?SetLocalViewSpecularMode@Display@Jet@@QAAX_N@Z','?SetAlphaBlendEquation@Display@Jet@@QAAXW4DBlendEquation@2@@Z','?SetAlphaBlendMode@Display@Jet@@QAAX_N@Z','?SetAlphaTestMode@Display@Jet@@QAAX_N@Z','?SetDepthTestMode@Display@Jet@@QAAX_N@Z','?SetReflectionCull@Display@Jet@@QAAX_N@Z','?SetVertexWeightMode@Display@Jet@@QAAX_N@Z','?SetDepthWriteMode@Display@Jet@@QAAX_N@Z','?SetWireFrameMode@Display@Jet@@QAAX_N@Z','?SetAlphaTestFunction@Display@Jet@@QAAXW4DFunction@2@M@Z','?SetCullMode@Display@Jet@@QAAXW4DCull@2@@Z','?SetDepthClamp@Display@Jet@@QAAX_N@Z','?SetScissorMode@Display@Jet@@QAAX_N@Z','?SetStencilWriteMask@Display@Jet@@QAAXKW4DFace@2@@Z','?SetFogColor@Display@Jet@@QAAXABVColor@2@@Z','?SetGlobalAmbient@Display@Jet@@QAAXABVColor@2@@Z','?SetClearColor@Display@Jet@@QAAXABVColor@2@@Z','?SetClearStencil@Display@Jet@@QAAXJ@Z','?SetColorMaterialMode@Display@Jet@@QAAXW4DColorMaterialTypes@2@@Z','?SetLightingMode@Display@Jet@@QAAX_N@Z']
for b in re.split(r'\n(?=// ==== )', text):
    head=b[:400]
    for n in names:
        if n in head:
            print('====', n)
            print(b[:700])
            print()
            names.remove(n)
            break
