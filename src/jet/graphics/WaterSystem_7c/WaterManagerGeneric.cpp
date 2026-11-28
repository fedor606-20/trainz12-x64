// Unit U10: WaterManagerGeneric. Behaviour follows the TS12 assembly (addresses per function). The material is
// found through the RenderKernel's material manager (+0x90) by the "Water<instance><config name>" name; texture
// and cube-map work goes through the raw Display vtable slots the original calls (0x98, 1 and 3).
#include "WaterManagerGeneric.h"

#include <jet/Bitmap.h>
#include <jet/KernelStandard.h>
#include <jet/Material.h>
#include <jet/PString.h>
#include <jet/RenderTypes.h>  // DisplayFormat
#include <jet/Resource.h>     // ResourceDatabase
#include <jet/Texture.h>
#include <jet/VCall.h>
#include <jet/WaterAnimation.h>
#include <jet/WaterMaterial.h>
#include <jet/WaterTextureAnimation.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

// the original's explicit `*texture = &Jet::Texture::vftable` (imported); the linker aliases this onto the
// RenderSystem import slot
extern "C" const void* WsMgrTextureVftable;
#pragma comment(linker, "/alternatename:_WsMgrTextureVftable=__imp_??_7Texture@Jet@@6B@")

using TextureCtorFn = void(__thiscall*)(void*, const PString&, TextureManager*, DisplayTexture*, int);
extern "C" TextureCtorFn WsMgrTextureCtor;
#pragma comment(linker, "/alternatename:_WsMgrTextureCtor=__imp_??0Texture@Jet@@QAE@ABVPString@1@PAVTextureManager@1@PAVDisplayTexture@1@W4TextureAlpha@1@@Z")

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

// RenderKernel + 0x7c is the Display; its vtable slot 0x98 hands back the DisplayTexture the original uses
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
    WsMgrTextureCtor(texture, name, manager, static_cast<DisplayTexture*>(display), 0);
    *reinterpret_cast<const void**>(texture) = WsMgrTextureVftable;
    return texture;
}

bool HasColon(const PString& text)
{
    if (!text.node || text.node->length == 0)
        return false;
    return PString::Cache()->Find(text.node, ':', 0) != -1;
}

// The original calls the animation's vtable slot 0 (WaterAnimationPN::Initialize). The U4 class's table puts
// GetXYFrame first, so call the concrete class directly when it is ours and keep the raw slot for any other
// plugin object (which follows the retail order).
bool InitializeAnimation(WaterAnimation* animation, FrameAnimation* frameAnimation, const Size& size,
                         const AnimProp& prop)
{
    if (auto* const pn = dynamic_cast<WaterAnimationPN*>(animation))
        return pn->Initialize(frameAnimation, size, prop);
    return VCall<bool>(animation, 0, frameAnimation, &size, &prop);
}

// Material vtable slot 9 is the AutoReference<Texture> SetTexture overload in the retail table (slot 10 is the
// database/name overload; the local Material header has those two swapped).
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

using SetTextureNameFn = int*(__cdecl*)(void*, int*, int, const ResourceDatabase*, const PString*, float,
                                         const String*);
int SetTextureName(WaterMaterial* material, int type, const ResourceDatabase* db, const PString& name, float amount,
                   const String& request)
{
    void* const* const vtable = *reinterpret_cast<void* const* const*>(material);
    void* const address = vtable[10];
    SetTextureNameFn function;
    std::memcpy(&function, &address, sizeof function);
    int result = 0;
    function(material, &result, type, db, &name, amount, &request);
    return result;
}

// The opacity texture branch: a relative name is resolved against the config's directory and handed to
// SetTexture(6, db, resolvedName, opacityAmount); a failed or skipped lookup retries with the plain name.
void ApplyOpacityTexture(WaterMaterial* material, const ResourceDatabase* db, const PString& name,
                         const WaterManagerConfig& config)
{
    const MaterialProp& properties = config.GetMaterialProperties();
    const PString& opacity = properties.opacityTex;
    if (!opacity.node || opacity.node->length == 0)
        return;

    PString lower(opacity);
    lower.node = PString::Cache()->ToLower(lower.node);

    bool fallback = !name.node || name.node->length == 0 || !lower.node || lower.node->text[0] == '\\' ||
                    lower.node->text[0] == '/' || HasColon(lower);
    if (!fallback) {
        File::Path path(name.Text());
        path.AddPath(lower.Text());
        PString resolved(path.text);
        String request;
        if (SetTextureName(material, 6, db, resolved, properties.opacityAmount, request) != 0)
            fallback = true;
    }
    if (fallback) {
        String request;
        SetTextureName(material, 6, db, lower, properties.opacityAmount, request);
    }
}

}  // namespace

// 0x10038fbc
int WaterManagerGeneric::instance = 0;

// 0x100390c4
const PString WaterManagerGeneric::MANAGER_NAME("WaterManagerGeneric");

// 0x1000b270
WaterManagerGeneric::WaterManagerGeneric()
    : material(nullptr)
    , frameAnimation(nullptr)
    , textureAnimation(nullptr)
    , renderKernel(nullptr)
    , cubeMapTexture(nullptr)
    , scaleNormXY(1.0f)
{
}

// 0x1000b370
WaterManagerGeneric::~WaterManagerGeneric()
{
    if (material)
        material->ReleaseReference();
    DeleteAllocObject(textureAnimation);
    Alloc::Delete(frameAnimation);
    if (cubeMapTexture)
        cubeMapTexture->ReleaseReference();
}

// 0x1000b1e0: the shared tail jump the original folds onto WaterManager::Initialize
bool WaterManagerGeneric::Initialize(RenderKernel* kernel, const ResourceDatabase* db, const PString& name,
                                     int flags)
{
    return WaterManager::Initialize(kernel, db, name, flags);
}

// 0x1000b120
WaterMaterial* WaterManagerGeneric::GetMaterial()
{
    return material;
}

// 0x1000b130
WaterTextureAnimation* WaterManagerGeneric::GetTextureAnimation()
{
    return textureAnimation;
}

// 0x1000b1f0
bool WaterManagerGeneric::SetReflectionType(int type)
{
    if (!material)
        return false;
    return material->SetReflectionType(type);
}

// 0x1000b210
int WaterManagerGeneric::GetReflectionType() const
{
    if (!material)
        return 1;
    return material->GetReflectionType();
}

// 0x1000b230
void WaterManagerGeneric::SetScaleNormXY(float value)
{
    scaleNormXY = value;
    if (material)
        material->SetScaleNormXY(value);
}

// 0x1000b260
float WaterManagerGeneric::GetScaleNormXY() const
{
    return scaleNormXY;
}

// 0x1000c060: the frame clock inline, then the material's update
void WaterManagerGeneric::Update(float time)
{
    if (frameAnimation)
        frameAnimation->Update(time);
    if (material)
        material->Update(time);
}

// 0x1000b450
bool WaterManagerGeneric::Initialize(RenderKernel* kernel, const ResourceDatabase* db,
                                     const WaterManagerConfig* config, const PString& name, int flags)
{
    renderKernel = kernel;
    ++instance;

    PString materialName;
    materialName.Setf("Water%d%s", instance, config->GetMaterialProperties().materialName.Text());
    MaterialManager* const manager = *reinterpret_cast<MaterialManager**>(reinterpret_cast<char*>(kernel) + 0x90);
    Material* rawMaterial = nullptr;
    manager->Create(rawMaterial, materialName);
    material = dynamic_cast<WaterMaterial*>(rawMaterial);
    if (!material)
        return false;

    const AnimProp& detail = config->GetDetailAnimation();
    if (detail.animMaxHeight > 0.0f) {
        frameAnimation = NewAllocObject<FrameAnimation>(detail.animSampleRate, detail.animPeriod);
        if (!detail.frameData) {
            WaterAnimation* animation = WaterAnimation::CreateInstance(detail.animName);
            if (!animation)
                return false;
            InitializeAnimation(animation, frameAnimation, detail.animSize, detail);
            if (scaleNormXY == 1.0f)
                scaleNormXY = detail.animScaleNormXY;
            textureAnimation = NewAllocObject<WaterTextureAnimation>();
            if (!textureAnimation ||
                !textureAnimation->Initialize(kernel, frameAnimation, animation, detail.animWorldSize, scaleNormXY))
                return false;
            animation->ReleaseReference();
        } else {
            textureAnimation = NewAllocObject<WaterTextureAnimation>();
            if (!textureAnimation ||
                !textureAnimation->Initialize(kernel, frameAnimation->GetNumFrames(), detail.animSize,
                                              static_cast<unsigned char*>(detail.frameData), detail.animWorldSize,
                                              detail.animMaxHeight))
                return false;
        }
    }

    if (!material->Initialize(db, config, frameAnimation, textureAnimation, flags))
        return false;
    material->SetScaleNormXY(scaleNormXY);

    if (CreateCubeMapTexture(kernel, db, name, config->GetCubeTextures(), cubeMapTexture)) {
        cubeMapTexture->AddReference();
        Texture* const reference = cubeMapTexture;
        SetTextureRef(material, 9, &reference, 1.0f);
        reference->ReleaseReference();
    }

    ApplyOpacityTexture(material, db, name, *config);
    return true;
}

// 0x1000ba00
bool WaterManagerGeneric::CreateCubeMapTexture(RenderKernel* kernel, const ResourceDatabase* db,
                                               const PString& name, const PString* const cubeTextures,
                                               Texture*& out)
{
    Bitmap bitmaps[6];
    if (!cubeTextures[0].node || cubeTextures[0].node->length == 0)
        return false;

    const PString empty;
    bool simple = !name.node || name.node->length == 0 || cubeTextures[0].node->text[0] == '\\' ||
                  cubeTextures[0].node->text[0] == '/' || HasColon(cubeTextures[0]);
    if (!simple) {
        int i = 0;
        for (; i < 6; ++i) {
            File::Path path(name.Text());
            path.AddPath(cubeTextures[i].Text());
            PString relative(path.text);
            bool loaded;
            if (db) {
                PString fileName = db->GetFileName(relative);
                loaded = bitmaps[i].Load(cubeTextures[i], fileName, false);
            } else {
                loaded = bitmaps[i].Load(cubeTextures[i], relative, false);
            }
            if (!loaded)
                break;
        }
        if (i < 6)
            simple = true;
    }
    if (simple) {
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
    }

    void* const display = GetDisplayTexture(kernel);
    DisplayFormat format(DFORMAT_DEFAULT);
    ReferenceCount* reference = nullptr;
    const bool made = VCall<bool>(display, 4, bitmaps, &format, 1, 1, 0, 6, 1.0f, 1, 1, 1, 1, 1, 1, &reference);
    if (reference)
        reference->ReleaseReference();
    if (!made)
        return false;

    for (int i = 0; i < 6; ++i) {
        DisplayFormat faceFormat(DFORMAT_DEFAULT);
        if (!VCall<bool>(display, 12, 0, &faceFormat, &bitmaps[i], i))
            return false;
    }

    PString textureName;
    textureName.Setf("WaterCubeMap_%d", instance);
    out = MakeTexture(kernel, display, textureName);
    return true;
}

}  // namespace Jet
