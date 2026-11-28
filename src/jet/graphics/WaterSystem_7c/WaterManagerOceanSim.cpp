// Unit U10: WaterManagerOceanSim. Behaviour follows the TS12 assembly (addresses per function).
#include "WaterManagerOceanSim.h"

#include <jet/Bitmap.h>
#include <jet/KernelStandard.h>
#include <jet/Material.h>
#include <jet/RenderTypes.h>
#include <jet/Resource.h>
#include <jet/Texture.h>
#include <jet/VCall.h>
#include <jet/WaterAnimation.h>
#include <jet/WaterGeometry.h>
#include <jet/WaterMaterial.h>
#include <jet/WaterTextureAnimation.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

extern "C" const void* WsOceanTextureVftable;
#pragma comment(linker, "/alternatename:_WsOceanTextureVftable=__imp_??_7Texture@Jet@@6B@")

using TextureCtorFn = void(__thiscall*)(void*, const PString&, TextureManager*, DisplayTexture*, int);
extern "C" TextureCtorFn WsOceanTextureCtor;
#pragma comment(linker, "/alternatename:_WsOceanTextureCtor=__imp_??0Texture@Jet@@QAE@ABVPString@1@PAVTextureManager@1@PAVDisplayTexture@1@W4TextureAlpha@1@@Z")

template <class T, class... A>
T* NewAllocObject(A... args)
{
    void* const memory = Alloc::New(sizeof(T));
    return memory ? ::new (memory) T(args...) : nullptr;
}

template <class T>
void DeleteAllocObject(T* object)
{
    if (object) {
        object->~T();
        Alloc::Delete(object);
    }
}

void* GetDisplayTexture(RenderKernel* kernel)
{
    void* const display = *reinterpret_cast<void**>(reinterpret_cast<char*>(kernel) + 0x7c);
    return VCall<void*>(display, 0x98);
}

Texture* MakeTexture(RenderKernel* kernel, void* display, const PString& name)
{
    auto* const texture = static_cast<Texture*>(Alloc::New(0x34));
    if (!texture)
        return nullptr;
    auto* const manager = *reinterpret_cast<TextureManager**>(reinterpret_cast<char*>(kernel) + 0x94);
    WsOceanTextureCtor(texture, name, manager, static_cast<DisplayTexture*>(display), 0);
    *reinterpret_cast<const void**>(texture) = WsOceanTextureVftable;
    return texture;
}

bool InitializeAnimation(WaterAnimation* animation, FrameAnimation* frameAnimation, const Size& size,
                         const AnimProp& prop)
{
    if (auto* const pn = dynamic_cast<WaterAnimationPN*>(animation))
        return pn->Initialize(frameAnimation, size, prop);
    return VCall<bool>(animation, 0, frameAnimation, &size, &prop);
}

using SetTextureRefFn = int*(__cdecl*)(void*, int*, int, const void*, float);
int SetTextureRef(WaterMaterial* material, int type, const void* textureReference, float amount)
{
    void* const* const vtable = *reinterpret_cast<void* const* const*>(material);
    void* const address = vtable[9];
    SetTextureRefFn function;
    std::memcpy(&function, &address, sizeof function);
    int result = 0;
    function(material, &result, type, textureReference, amount);
    return result;
}

}  // namespace

// 0x100390c8
const PString WaterManagerOceanSim::MANAGER_NAME("WaterManagerOceanSim");

// 0x1000c290 (the tile fields are not initialized by the original)
WaterManagerOceanSim::WaterManagerOceanSim()
    : material(nullptr)
    , surfaceFrameAnimation(nullptr)
    , detailFrameAnimation(nullptr)
    , textureAnimation(nullptr)
    , geometryAnimation(nullptr)
    , renderKernel(nullptr)
    , cubeMapTexture(nullptr)
    , scaleNormXY(1.0f)
{
}

// 0x1000c3a0
WaterManagerOceanSim::~WaterManagerOceanSim()
{
    if (material)
        material->ReleaseReference();
    DeleteAllocObject(textureAnimation);
    Alloc::Delete(surfaceFrameAnimation);
    Alloc::Delete(detailFrameAnimation);
    DeleteAllocObject(geometryAnimation);
    if (cubeMapTexture)
        cubeMapTexture->ReleaseReference();
}

// 0x1000b1e0: the shared tail jump the original folds onto WaterManager::Initialize
bool WaterManagerOceanSim::Initialize(RenderKernel* kernel, const ResourceDatabase* db, const PString& name,
                                      int flags)
{
    return WaterManager::Initialize(kernel, db, name, flags);
}

// 0x1000c130
WaterMaterial* WaterManagerOceanSim::GetMaterial()
{
    return material;
}

// 0x1000c140
WaterTextureAnimation* WaterManagerOceanSim::GetTextureAnimation()
{
    return textureAnimation;
}

// 0x1000c150
WaterGeometryAnimation* WaterManagerOceanSim::GetGeometryAnimation()
{
    return geometryAnimation;
}

// 0x1000c160
const Vector3& WaterManagerOceanSim::GetTileScale() const
{
    return tileScale;
}

// 0x1000c170
const Size& WaterManagerOceanSim::GetTileSize() const
{
    return tileSize;
}

// 0x1000c220
bool WaterManagerOceanSim::SetReflectionType(int type)
{
    if (!material)
        return false;
    return material->SetReflectionType(type);
}

// 0x1000c240
int WaterManagerOceanSim::GetReflectionType() const
{
    if (!material)
        return 1;
    return material->GetReflectionType();
}

// 0x1000c260 (the ocean manager does not forward the scale to the material)
void WaterManagerOceanSim::SetScaleNormXY(float value)
{
    scaleNormXY = value;
}

// 0x1000c280
float WaterManagerOceanSim::GetScaleNormXY() const
{
    return scaleNormXY;
}

// 0x1000cb90: both frame clocks (the detail one only when allocated), then the geometry animation
void WaterManagerOceanSim::Update(float time)
{
    surfaceFrameAnimation->Update(time);
    if (detailFrameAnimation)
        detailFrameAnimation->Update(time);
    if (geometryAnimation)
        geometryAnimation->Update(surfaceFrameAnimation);
}

// 0x1000c4a0
bool WaterManagerOceanSim::Initialize(RenderKernel* kernel, const ResourceDatabase* db,
                                      const WaterManagerConfig* config, const PString&, int flags)
{
    renderKernel = kernel;

    const PString& materialName = config->GetMaterialProperties().materialName;
    MaterialManager* const manager = *reinterpret_cast<MaterialManager**>(reinterpret_cast<char*>(kernel) + 0x90);
    Material* rawMaterial = nullptr;
    manager->Create(rawMaterial, materialName);
    material = dynamic_cast<WaterMaterial*>(rawMaterial);
    if (!material)
        return false;

    const AnimProp& detail = config->GetDetailAnimation();
    if (detail.animMaxHeight > 0.0f) {
        detailFrameAnimation =
            NewAllocObject<FrameAnimation>(detail.animSampleRate, detail.animPeriod);
        WaterAnimation* animation = WaterAnimation::CreateInstance(detail.animName);
        if (!animation)
            return false;
        InitializeAnimation(animation, detailFrameAnimation, detail.animSize, detail);
        textureAnimation = NewAllocObject<WaterTextureAnimation>();
        if (!textureAnimation ||
            !textureAnimation->Initialize(kernel, detailFrameAnimation, animation, detail.animWorldSize, scaleNormXY))
            return false;
        animation->ReleaseReference();
    }

    if (!material->Initialize(db, config, detailFrameAnimation, textureAnimation, flags))
        return false;

    if (CreateCubeMapTexture(kernel, db, config->GetCubeTextures(), cubeMapTexture)) {
        cubeMapTexture->AddReference();
        Texture* const reference = cubeMapTexture;
        SetTextureRef(material, 9, &reference, 1.0f);
        reference->ReleaseReference();
    }

    const AnimProp& surface = config->GetSurfaceAnimation();
    surfaceFrameAnimation = NewAllocObject<FrameAnimation>(surface.animSampleRate, surface.animPeriod);
    WaterAnimation* surfaceAnimation = WaterAnimation::CreateInstance(surface.animName);
    if (!surfaceAnimation)
        return false;
    InitializeAnimation(surfaceAnimation, surfaceFrameAnimation, surface.animSize, surface);
    tileGridWidth = 7;
    tileGridHeight = 7;

    if (surface.animGeometry == 0) {
        surfaceAnimation->ReleaseReference();
        return true;
    }

    geometryAnimation = NewAllocObject<WaterGeometryAnimation>();
    const TileProp& tile = config->GetTileProperties();
    if (!geometryAnimation->Create(kernel, material, surfaceFrameAnimation, surfaceAnimation, tile.tileSize,
                                   tile.tileScale, tile.tileUVScale))
        return false;
    surfaceAnimation->ReleaseReference();
    return true;
}

// 0x1000c7d0
bool WaterManagerOceanSim::CreateCubeMapTexture(RenderKernel* kernel, const ResourceDatabase* db,
                                                const PString* const cubeTextures, Texture*& out)
{
    Bitmap bitmaps[6];
    const PString empty;
    for (int i = 0; i < 6; ++i) {
        bool loaded;
        if (db) {
            PString fileName = db->GetFileName(cubeTextures[i]);
            loaded = bitmaps[i].Load(cubeTextures[i], fileName, false);
        } else {
            loaded = bitmaps[i].Load(cubeTextures[i], empty, false);
        }
        if (!loaded)
            return false;
    }

    void* const display = GetDisplayTexture(kernel);
    DisplayFormat format(static_cast<DFormatId>(5));
    ReferenceCount* reference = nullptr;
    const bool made = VCall<bool>(display, 4, bitmaps, &format, 1, 1, 0, 0, 1.0f, 1, 1, 1, 1, 1, 1, &reference);
    if (reference)
        reference->ReleaseReference();
    if (!made)
        return false;

    for (int i = 0; i < 6; ++i) {
        DisplayFormat faceFormat(DFORMAT_DEFAULT);
        if (!VCall<bool>(display, 12, 0, &faceFormat, &bitmaps[i], i))
            return false;
    }

    out = MakeTexture(kernel, display, PString("cubeMap"));
    return true;
}

}  // namespace Jet
