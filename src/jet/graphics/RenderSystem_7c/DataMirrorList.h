// U11 data mirrors: one table shared by DataStatics.cpp and diff_rs_u11.cpp.
#pragma once

#define RS_PLAIN_DATA(X) \
    X(RsRsD019, "?__info@?1??Render@LayerManager@Jet@@QAAXXZ@4UInfo@Profile@3@A", 32) \
    X(RsRsD020, "?__info@?1??Update@LayerManager@Jet@@QAAXM@Z@4UInfo@Profile@3@A", 32) \
    X(RsRsD022, "?anisotropySupport@DisplayTextureNull@Jet@@1_NA", 1) \
    X(RsRsD033, "?frameStats@DisplayTexture@Jet@@1UDisplayTextureFrameStats@12@A", 24) \
    X(RsRsD037, "?internalColorFormat@Display@Jet@@1W4DFormatId@2@A", 4) \
    X(RsRsD038, "?layerManager@Layer@Jet@@1PAVLayerManager@2@A", 4) \
    X(RsRsD045, "?nextFreeId@VertexType@Jet@@1HA", 4) \
    X(RsRsD053, "?projectorCache@Projector@Jet@@1PAVProjectorCache@2@A", 4) \
    X(RsRsD059, "?results@TextureCombinerManager@Jet@@2QBUResultTable@2@B", 4) \
    X(RsRsD060, "?results@VertexProgramManager@Jet@@2QBUResultTable@2@B", 4) \
    X(RsRsD068, "?textureArray@SharedTextureCollection@Jet@@1PAV?$AutoReference@VTexture@Jet@@@2@A", 4) \
    X(RsRsD069, "?textureNameArray@SharedTextureCollection@Jet@@1PAVPString@2@A", 4)

#define RS_VFTABLE_DATA(X) \
    X(RsRsV001, "??_7Display@Jet@@6B01@@", 80) \
    X(RsRsV002, "??_7Display@Jet@@6BPluginInstance@1@@", 2) \
    X(RsRsV003, "??_7DisplayContext@Display@Jet@@6B@", 1) \
    X(RsRsV004, "??_7DisplaySaveBuffer@Jet@@6B@", 4) \
    X(RsRsV005, "??_7DisplaySelector@Jet@@6B@", 7) \
    X(RsRsV006, "??_7ElementLoader@Jet@@6B@", 2) \
    X(RsRsV007, "??_7NonRenderContext@Jet@@6B@", 5) \
    X(RsRsV008, "??_7RenderKernel@Jet@@6BLayerManager@1@@", 2) \
    X(RsRsV009, "??_7RenderKernel@Jet@@6BTask@1@@", 6) \
    X(RsRsV010, "??_7SharedContext@Jet@@6B@", 2)

#define RS_VBTABLE_DATA(X) \
    X(RsRsVb001, "??_8Display@Jet@@7B@", 2)
