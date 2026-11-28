// WaterSystem_7c unit U10: the ocean-simulation water manager ("WaterManagerOceanSim"). It builds a water
// material, the detail animation and bump textures, a "cubeMap" cube texture, then the surface animation and the
// morphing geometry animation. Layout 0x1bc = WaterManager + {tileSize +0x16c, tileScale +0x174, tileUVScale
// +0x180, two ints set to 7 by Initialize +0x188/+0x18c, material +0x190, surface frame animation +0x194, detail
// frame animation +0x198, texture animation +0x19c, geometry animation +0x1a0, kernel +0x1a4, cube map +0x1a8,
// scaleNormXY +0x1ac, vtordisp +0x1b0, ReferenceCount +0x1b4}. Behaviour follows the TS12 assembly (addresses
// per member in WaterManagerOceanSim.cpp).
#pragma once

#include "WaterManager.h"

#include <jet/FrameAnimation.h>
#include <jet/Math.h>
#include <jet/PString.h>

namespace Jet {

class RenderKernel;
class ResourceDatabase;
class Texture;
class WaterGeometryAnimation;
class WaterMaterial;
class WaterTextureAnimation;

class WaterManagerOceanSim : public WaterManager {
public:
    WaterManagerOceanSim();
    ~WaterManagerOceanSim() override;

    // slots 0..7, in the base's declaration order
    bool __thiscall Initialize(RenderKernel* kernel, const ResourceDatabase* db, const PString& name,
                               int flags) override;
    bool __thiscall Initialize(RenderKernel* kernel, const ResourceDatabase* db, const WaterManagerConfig* config,
                               const PString& name, int flags) override;
    bool __thiscall SetReflectionType(int type) override;
    int __thiscall GetReflectionType() const override;
    void __thiscall SetScaleNormXY(float value) override;
    float __thiscall GetScaleNormXY() const override;
    void __thiscall Update(float time) override;
    WaterMaterial* __thiscall GetMaterial() override;

    WaterTextureAnimation* __thiscall GetTextureAnimation();        // +0x19c
    WaterGeometryAnimation* __thiscall GetGeometryAnimation();      // +0x1a0
    const Vector3& __thiscall GetTileScale() const;                 // +0x174
    const Size& __thiscall GetTileSize() const;                     // +0x16c

    static const PString MANAGER_NAME;  // "WaterManagerOceanSim"

protected:
    bool __thiscall CreateCubeMapTexture(RenderKernel* kernel, const ResourceDatabase* db,
                                         const PString* const cubeTextures, Texture*& out);

    // The original constructor never writes these three (GetTileSize/GetTileScale return them as-is).
    Size tileSize;             // +0x16c
    Vector3 tileScale;         // +0x174
    Vector2 tileUVScale;       // +0x180
    int tileGridWidth;         // +0x188 set to 7 by Initialize
    int tileGridHeight;        // +0x18c set to 7 by Initialize
    WaterMaterial* material;   // +0x190
    FrameAnimation* surfaceFrameAnimation;  // +0x194
    FrameAnimation* detailFrameAnimation;   // +0x198
    WaterTextureAnimation* textureAnimation;  // +0x19c
    WaterGeometryAnimation* geometryAnimation;  // +0x1a0
    RenderKernel* renderKernel;                 // +0x1a4
    Texture* cubeMapTexture;                    // +0x1a8
    float scaleNormXY;                          // +0x1ac
};
JET_LAYOUT32(sizeof(WaterManagerOceanSim) == 0x1bc);

}  // namespace Jet
