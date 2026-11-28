// WaterSystem_7c unit U10: the generic water manager ("WaterManagerGeneric"). It builds a water material, the
// detail animation and its bump textures, a cube map, then the surface animation and its geometry animation.
// Layout 0x190 = WaterManager + {material +0x16c, frame animation +0x170, texture animation +0x174, kernel
// +0x178, cube map +0x17c, scaleNormXY +0x180, vtordisp +0x184, ReferenceCount +0x188}. Behaviour follows the
// TS12 assembly (addresses per member in WaterManagerGeneric.cpp).
#pragma once

#include "WaterManager.h"

#include <jet/FrameAnimation.h>
#include <jet/PString.h>

namespace Jet {

class RenderKernel;
class ResourceDatabase;
class Texture;
class WaterMaterial;
class WaterTextureAnimation;

class WaterManagerGeneric : public WaterManager {
public:
    WaterManagerGeneric();
    ~WaterManagerGeneric() override;

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

    WaterTextureAnimation* __thiscall GetTextureAnimation();  // +0x174

    static const PString MANAGER_NAME;  // "WaterManagerGeneric"

protected:
    bool __thiscall CreateCubeMapTexture(RenderKernel* kernel, const ResourceDatabase* db, const PString& name,
                                         const PString* const cubeTextures, Texture*& out);

    static int instance;  // +0x38fbc, counts the managers (the material and cube map names use it)

    WaterMaterial* material;              // +0x16c
    FrameAnimation* frameAnimation;       // +0x170
    WaterTextureAnimation* textureAnimation;  // +0x174
    RenderKernel* renderKernel;           // +0x178
    Texture* cubeMapTexture;              // +0x17c
    float scaleNormXY;                    // +0x180
};
JET_LAYOUT32(sizeof(WaterManagerGeneric) == 0x190);

}  // namespace Jet
