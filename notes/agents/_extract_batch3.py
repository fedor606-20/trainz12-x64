names = [
"?FlushScratchBuffer@Jet@@YAXXZ",
"?GetExtensionString@Display@Jet@@QBAPBDW4DExtension@2@@Z",
"?SetRenderTarget@Display@Jet@@QAAXPAVDisplayTexture@2@I@Z",
"?RestoreGamma@Display@Jet@@QAAXXZ",
"?GetTargetPPI@Display@Jet@@QBAHXZ",
"?AddScene@SceneGraph@Jet@@QAAXPAVElement@2@@Z",
"?DelayedDelete@SceneGraph@Jet@@QAAXPAVElement@2@@Z",
"?UnlinkScene@SceneGraph@Jet@@QAAXPAVElement@2@@Z",
"?Update@SceneGraph@Jet@@QAAXM@Z",
"?AddSystemResource@RenderKernel@Jet@@QAAXPAVSystemResourceManager@2@@Z",
"?RemoveSystemResource@RenderKernel@Jet@@QAAXPAVSystemResourceManager@2@@Z",
"?DestroySceneGraph@RenderKernel@Jet@@QAAXXZ",
"?DumpScreen@RenderKernel@Jet@@QAAXI@Z",
"?FindLayer@LayerManager@Jet@@QAAPAVLayer@2@ABVPString@2@@Z",
"?GetLayerAt@LayerManager@Jet@@QAAPAVLayer@2@ABVVector2@2@@Z",
"?GetLayerUnder@LayerManager@Jet@@QAAPAVLayer@2@ABVVector2@2@0@Z",
"?AddSystemResourceViewer@LayerManager@Jet@@QAAXPAVSystemResourceViewer@2@@Z",
"?RemoveSystemResourceViewer@LayerManager@Jet@@QAAXPAVSystemResourceViewer@2@@Z",
"?BringToFront@LayerManager@Jet@@QAAXPAVLayer@2@@Z",
"?BringToBack@LayerManager@Jet@@QAAXPAVLayer@2@@Z",
"?Add@LayerGroup@Jet@@QAAXPAVLayer@2@@Z",
"?BringToFront@LayerGroup@Jet@@QAAXPAVLayer@2@@Z",
"?BringToBack@LayerGroup@Jet@@QAAXPAVLayer@2@@Z",
"?FindLayer@LayerGroup@Jet@@QAAPAVLayer@2@ABVPString@2@@Z",
"?SetPriority@Layer@Jet@@QAAXH@Z",
"?BringToFront@Layer@Jet@@QAAXXZ",
"?BringToBack@Layer@Jet@@QAAXXZ",
"?SetParent@Layer@Jet@@QAAXPAVLayerGroup@2@@Z",
]
import re
text=open(r'D:/trainz12-decomp/decomp/RenderSystem_7c.dll.c','r',encoding='utf-8',errors='ignore').read()
for b in re.split(r'\n(?=// ==== )', text):
    head=b[:400]
    for n in names:
        if n in head:
            print('====', n)
            print(b[:1000])
            print()
            names.remove(n)
            break
