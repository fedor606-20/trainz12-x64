// U11 data mirrors: one table shared by DataStatics.cpp and diff_rs_u11.cpp.
#pragma once

#define RS_PLAIN_DATA(X) \
    X(RsRsD001, "?ALPHA_HINT_DEFAULT@TextureResource@Jet@@0W4TextureAlpha@2@A", 4) \
    X(RsRsD002, "?ANISOTROPY_DEFAULT@TextureResource@Jet@@0MA", 4) \
    X(RsRsD003, "?ANISOTROPY_DEFAULT_TOKEN_VALUE@TextureResource@Jet@@2MA", 4) \
    X(RsRsD004, "?AUTO_MIP_DEFAULT@TextureResource@Jet@@0W4TextureAutoMip@2@A", 4) \
    X(RsRsD005, "?DISPLAYFORMATID_DEFAULT@TextureResource@Jet@@0W4DFormatId@2@A", 4) \
    X(RsRsD006, "?FILE_COMPRESSION_DEFAULT@TextureResource@Jet@@0PADA", 8) \
    X(RsRsD007, "?HINT_DEFAULT@TextureResource@Jet@@0W4TextureHint@2@A", 4) \
    X(RsRsD008, "?MAG_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A", 4) \
    X(RsRsD009, "?MAX_STREAMING_TEXTURE_MIP_COUNT@TextureManager@Jet@@2HA", 4) \
    X(RsRsD010, "?MAX_STREAMING_TEXTURE_SIZE@TextureManager@Jet@@2HA", 4) \
    X(RsRsD011, "?MIN_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A", 4) \
    X(RsRsD012, "?MIN_STREAMING_TEXTURE_MIP_COUNT@TextureManager@Jet@@2HA", 4) \
    X(RsRsD013, "?MIN_STREAMING_TEXTURE_SIZE@TextureManager@Jet@@2HA", 4) \
    X(RsRsD014, "?MIP_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A", 4) \
    X(RsRsD015, "?NORMAL_MAP_HINT_DEFAULT@TextureResource@Jet@@0W4NormalMapHint@2@A", 4) \
    X(RsRsD016, "?TEXTURE_STREAMING_ENABLED@TextureManager@Jet@@2_NA", 1) \
    X(RsRsD017, "?WRAP_S_DEFAULT@TextureResource@Jet@@0W4TextureWrap@2@A", 4) \
    X(RsRsD018, "?WRAP_T_DEFAULT@TextureResource@Jet@@0W4TextureWrap@2@A", 4) \
    X(RsRsD019, "?__info@?1??Render@LayerManager@Jet@@QAAXXZ@4UInfo@Profile@3@A", 32) \
    X(RsRsD020, "?__info@?1??Update@LayerManager@Jet@@QAAXM@Z@4UInfo@Profile@3@A", 32) \
    X(RsRsD021, "?addressSymbol@VertexProgramBasic@Jet@@1PBDB", 4) \
    X(RsRsD022, "?anisotropySupport@DisplayTextureNull@Jet@@1_NA", 1) \
    X(RsRsD023, "?apiName@VertexProgramBasic@Jet@@1PBDB", 4) \
    X(RsRsD024, "?bound@DisplayTextureNull@Jet@@1PAPBV12@A", 4) \
    X(RsRsD025, "?builtInProgramList@TextureCombinerManager@Jet@@1V?$Array@UProgramItem@TextureCombinerManager@Jet@@$0IA@@2@A", 12) \
    X(RsRsD026, "?builtInProgramList@VertexProgramManager@Jet@@1V?$Array@UProgramItem@VertexProgramManager@Jet@@$0IA@@2@A", 12) \
    X(RsRsD027, "?canAutoMip@DisplayTextureNull@Jet@@1_NA", 1) \
    X(RsRsD028, "?currentVisited@Element@Jet@@0IA", 4) \
    X(RsRsD029, "?delayedLoading@TextureRes@Jet@@1_NA", 1) \
    X(RsRsD030, "?display@VertexType@Jet@@1PBVDisplay@2@B", 4) \
    X(RsRsD031, "?envParameters@VertexProgramBasic@Jet@@1PAVVector4@2@A", 4) \
    X(RsRsD032, "?expInstr@VertexProgramBasic@Jet@@1PBDB", 4) \
    X(RsRsD033, "?frameStats@DisplayTexture@Jet@@1UDisplayTextureFrameStats@12@A", 24) \
    X(RsRsD034, "?globalSkipLevels@TextureManager@Jet@@1IA", 4) \
    X(RsRsD035, "?globalSkipThreshold@TextureManager@Jet@@1IA", 4) \
    X(RsRsD036, "?instSeperator@VertexProgramBasic@Jet@@1PBDB", 4) \
    X(RsRsD037, "?internalColorFormat@Display@Jet@@1W4DFormatId@2@A", 4) \
    X(RsRsD038, "?layerManager@Layer@Jet@@1PAVLayerManager@2@A", 4) \
    X(RsRsD039, "?logInstr@VertexProgramBasic@Jet@@1PBDB", 4) \
    X(RsRsD040, "?maxEnvRegs@VertexProgram@Jet@@1HA", 4) \
    X(RsRsD041, "?maxLocalRegs@VertexProgram@Jet@@1HA", 4) \
    X(RsRsD042, "?maxTotalRegs@VertexProgram@Jet@@1HA", 4) \
    X(RsRsD043, "?modifierListLock@ModifierList@Jet@@1VCriticalSection@2@A", 16) \
    X(RsRsD044, "?needsAlignedMatrix@VertexProgramBasic@Jet@@1_NA", 1) \
    X(RsRsD045, "?nextFreeId@VertexType@Jet@@1HA", 4) \
    X(RsRsD046, "?noPath@AsciiTextureResource@Jet@@1_NA", 1) \
    X(RsRsD047, "?numEnvParameters@VertexProgramBasic@Jet@@1HA", 4) \
    X(RsRsD048, "?numLocalParameters@VertexProgramBasic@Jet@@1HA", 4) \
    X(RsRsD049, "?numObjects@DisplayTextureNull@Jet@@1IA", 4) \
    X(RsRsD050, "?numTmpRegisters@VertexProgramBasic@Jet@@1HA", 4) \
    X(RsRsD051, "?numTotalParameters@VertexProgramBasic@Jet@@1HA", 4) \
    X(RsRsD052, "?opcodeMap@VertexProgramBasic@Jet@@1PBUOpcodeMap@12@B", 4) \
    X(RsRsD053, "?projectorCache@Projector@Jet@@1PAVProjectorCache@2@A", 4) \
    X(RsRsD054, "?registeredProgramList@VertexProgramManager@Jet@@1V?$HashTable@ABVPString@Jet@@PAURegisteredName@VertexProgramManager@2@UNameHash@42@$0N@@2@A", 12) \
    X(RsRsD055, "?registrations@GeometryChunk@Jet@@1V?$HashTable@IPAURegistrationData@GeometryChunk@Jet@@URegistrationDataOp@23@$0N@@2@A", 12) \
    X(RsRsD056, "?registrations@VertexType@Jet@@1V?$HashTable@IPAURegistrationData@VertexType@Jet@@URegistrationDataOp@23@$0N@@2@A", 12) \
    X(RsRsD057, "?resultMap@VertexProgramBasic@Jet@@1PBUResultMap@12@B", 4) \
    X(RsRsD058, "?resultSymbol@VertexProgramBasic@Jet@@1PBDB", 4) \
    X(RsRsD059, "?results@TextureCombinerManager@Jet@@2QBUResultTable@2@B", 4) \
    X(RsRsD060, "?results@VertexProgramManager@Jet@@2QBUResultTable@2@B", 4) \
    X(RsRsD061, "?staticParameterInit@VertexProgramBasic@Jet@@1HA", 4) \
    X(RsRsD062, "?supportChecked@TextureCombiner@Jet@@1_NA", 1) \
    X(RsRsD063, "?supportChecked@VertexProgram@Jet@@1PA_NA", 4) \
    X(RsRsD064, "?supported@TextureCombiner@Jet@@1_NA", 1) \
    X(RsRsD065, "?supported@VertexProgram@Jet@@1PA_NA", 4) \
    X(RsRsD066, "?supportsProgramMatrix@VertexProgramBasic@Jet@@1_NA", 1) \
    X(RsRsD067, "?tempSymbol@VertexProgramBasic@Jet@@1PBDB", 4) \
    X(RsRsD068, "?textureArray@SharedTextureCollection@Jet@@1PAV?$AutoReference@VTexture@Jet@@@2@A", 4) \
    X(RsRsD069, "?textureNameArray@SharedTextureCollection@Jet@@1PAVPString@2@A", 4) \
    X(RsRsD070, "?textureStats@DisplayTexture@Jet@@1UDisplayTextureStats@12@A", 48) \
    X(RsRsD071, "?theManager@TextureCombinerManager@Jet@@1PAV12@A", 4) \
    X(RsRsD072, "?theManager@VertexProgramManager@Jet@@1PAV12@A", 4) \
    X(RsRsD073, "?uniqueTexId@Texture@Jet@@1KA", 4) \
    X(RsRsD074, "?vertexMap@VertexProgramBasic@Jet@@1PBUVertexMap@12@B", 4) \
    X(RsRsD075, "?vertexSymbol@VertexProgramBasic@Jet@@1PBDB", 4)

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
