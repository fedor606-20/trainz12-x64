// WaterSystem_7c U7b: the WaterMaterial base (state, lifetime and the appended water virtuals). Behaviour
// follows the TS12 assembly; the concrete materials are U8a (WaterBumpMaterial.cpp / WaterOceanMaterial.cpp).
#include <jet/WaterMaterial.h>

#include "WaterMaterialInternal.h"

#include <jet/Geometry.h>
#include <jet/Plugin.h>
#include <jet/WaterManagerConfig.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

// The shader program names the original interns at load; the texts themselves are registered by U10.
const PString WaterMaterial::psWaterMatInterp("psWaterMatInterp");
const PString WaterMaterial::psWaterMatInterpFull("psWaterMatInterpFull");
const PString WaterMaterial::psWaterMatWaveAnim("psWaterMatWaveAnim");
const PString WaterMaterial::psWaterMatWaveAnimRefl("psWaterMatWaveAnimRefl");
const PString WaterMaterial::psWaterMatWaveRefl("psWaterMatWaveRefl");
const PString WaterMaterial::vsWaterMat("vsWaterMat");
const PString WaterMaterial::vsWaterMatAnimRefl("vsWaterMatAnimRefl");
const PString WaterMaterial::vsWaterMatRefl("vsWaterMatRefl");
const PString WaterMaterial::vsWaterMatWave("vsWaterMatWave");
const PString WaterMaterial::vsWaterMatWaveAnim("vsWaterMatWaveAnim");
const PString WaterMaterial::vsWaterMatWaveAnimRefl("vsWaterMatWaveAnimRefl");
const PString WaterMaterial::vsWaterMatWaveRefl("vsWaterMatWaveRefl");
const PString WaterMaterial::vsWaterTexInterp("vsWaterTexInterp");

namespace {

// A rendering cache's ReferenceCount lives in its virtual base: the vbtable at +0xc gives the offset from
// +0xc. The original's inlined helper does the same address arithmetic.
ReferenceCount* CacheReferenceCount(void* cache)
{
    auto* const bytes = static_cast<unsigned char*>(cache);
    int const* const vbtable = *reinterpret_cast<int**>(bytes + 0xc);
    const int offset = vbtable[1];
    return reinterpret_cast<ReferenceCount*>(bytes + 0xc + offset);
}

void AddCacheReference(void* cache)
{
    if (cache) CacheReferenceCount(cache)->AddReference();
}

void ReleaseCacheReference(void* cache)
{
    if (cache) CacheReferenceCount(cache)->ReleaseReference();
}

// The default ctors of Render3DCore's caches publish their own vtables and use the cache-ID PStrings. The
// original imports them from Render3DCore_7c; resolve them from the same module without a new static import.
using CacheCtor = void(__thiscall*)(void*, int);

CacheCtor CacheConstructor(const char* mangled)
{
    HMODULE module = GetModuleHandleA("Render3DCore_7c.dll");
    if (!module) module = LoadLibraryA("Render3DCore_7c.dll");
    if (!module) return nullptr;
    return reinterpret_cast<CacheCtor>(GetProcAddress(module, mangled));
}

// 0x1000ce47: FindCache on the kernel's +0x6c cache manager; create and add the cache when it is missing.
void* GetOrCreateCache(MaterialManager* manager, const PString& id, unsigned int size, const char* mangled)
{
    auto* const kernel = *reinterpret_cast<unsigned char**>(reinterpret_cast<unsigned char*>(manager) + 8);
    auto* const caches = reinterpret_cast<RenderingCacheManager*>(kernel + 0x6c);
    RenderingCache* cache = caches->FindCache(id);
    if (cache) {
        AddCacheReference(cache);
        return cache;
    }
    void* const memory = Alloc::New(size);
    if (memory) {
        if (CacheCtor ctor = CacheConstructor(mangled)) ctor(memory, 1);
    }
    caches->AddCache(static_cast<RenderingCache*>(memory));
    return memory;
}

}  // namespace

// 0x1000cd00
WaterMaterial::WaterMaterial(MaterialManager* manager)
    : Material(manager)
    , renderCache(nullptr)
    , alphaBlendCache(nullptr)
    , lightDir(0.0f, 0.0f, -1.0f)
    , reflectEyeVector(false)
{
    waterColor = Color{0.15f, 0.35f, 0.35f, 1.0f};
    lightColor = Color{1.0f, 1.0f, 1.0f, 1.0f};
    ambientColor = Color{0.1f, 0.1f, 0.1f, 1.0f};
    renderCache = GetOrCreateCache(manager, PString("ChunkCache"), 0xd2c, "??0MaterialChunkCache@Jet@@QAE@XZ");
    alphaBlendCache = GetOrCreateCache(manager, PString("AlphaBlendCache"), 0x80, "??0AlphaBlendCache@Jet@@QAE@XZ");
    WsInstallMaterialVtables(this, 0);
}

// 0x1000cfc0: the caches are referenced; the base class releases them.
WaterMaterial::~WaterMaterial()
{
    ReleaseCacheReference(renderCache);
    ReleaseCacheReference(alphaBlendCache);
}

// 0x1000d080: base state then the two caches (the reference flag is not copied by the original)
void WaterMaterial::InitInstance(WaterMaterial& instance) const
{
    Material::InitInstance(instance);
    if (renderCache) CacheReferenceCount(renderCache)->AddReference();
    ReleaseCacheReference(instance.renderCache);
    instance.renderCache = renderCache;
    if (alphaBlendCache) CacheReferenceCount(alphaBlendCache)->AddReference();
    ReleaseCacheReference(instance.alphaBlendCache);
    instance.alphaBlendCache = alphaBlendCache;

    const float* source = &lightColor.r;
    float* dest = &instance.lightColor.r;
    for (int i = 0; i < 4; ++i) {
        __asm {
            mov eax, source
            mov edx, dest
            mov ecx, i
            fld dword ptr [eax+ecx*4]
            fstp dword ptr [edx+ecx*4]
        }
    }
    source = &waterColor.r;
    dest = &instance.waterColor.r;
    for (int i = 0; i < 4; ++i) {
        __asm {
            mov eax, source
            mov edx, dest
            mov ecx, i
            fld dword ptr [eax+ecx*4]
            fstp dword ptr [edx+ecx*4]
        }
    }
    source = &lightColor.r;
    dest = &instance.lightColor.r;
    for (int i = 0; i < 4; ++i) {
        __asm {
            mov eax, source
            mov edx, dest
            mov ecx, i
            fld dword ptr [eax+ecx*4]
            fstp dword ptr [edx+ecx*4]
        }
    }
    source = &ambientColor.r;
    dest = &instance.ambientColor.r;
    for (int i = 0; i < 4; ++i) {
        __asm {
            mov eax, source
            mov edx, dest
            mov ecx, i
            fld dword ptr [eax+ecx*4]
            fstp dword ptr [edx+ecx*4]
        }
    }
    source = &lightDir.x;
    dest = &instance.lightDir.x;
    for (int i = 0; i < 3; ++i) {
        __asm {
            mov eax, source
            mov edx, dest
            mov ecx, i
            fld dword ptr [eax+ecx*4]
            fstp dword ptr [edx+ecx*4]
        }
    }
}

// 0x100036c0: forward to the concrete seven-parameter form with the vertex-program flag set.
GeometryChunk* WaterMaterial::CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                                          unsigned int numIndices, unsigned int numFaceNormals,
                                                          GeometryType type, GeometryChunkCreateHint* hint) const
{
    return CreateGeometryChunk(numVertices, numPrimitives, numIndices, numFaceNormals, type, hint, true);
}

// 0x1000d220 / 0x1000d210 (the compiler's vbase destructor and adjustor thunks are generated from the shape)

// ---------------------------------------------------------------------------------------------------------------
// Base stubs (the shared "return 0" bodies at 0x10015db0 except GetUseVertexProgram at 0x10013f10)

Material* WaterMaterial::CreateInstance() const { return nullptr; }
bool WaterMaterial::operator==(const Material&) const { return false; }
const PString& WaterMaterial::GetType() const { return *static_cast<const PString*>(nullptr); }
void WaterMaterial::Activate(int) {}
Material::Result WaterMaterial::DuplicateTexture(int) { return Result(0); }
Texture* WaterMaterial::GetTexture(int) { return nullptr; }
const Texture* WaterMaterial::GetTexture(int) const { return nullptr; }
void WaterMaterial::Bind(Display*, ViewPort*, unsigned int) {}
void WaterMaterial::Unbind(Display*, ViewPort*, unsigned int) {}
void WaterMaterial::CacheChunk(Display*, ViewPort*, GeometryChunk*, const DisplayLightPtrList*,
                                       const Matrix4*) const
{
}
void WaterMaterial::RenderChunk(Display*, ViewPort*, GeometryChunk*, const DisplayLightPtrList*,
                                        const Matrix4*, int) const
{
}
void WaterMaterial::RenderNumChunks(Display*, ViewPort*, GeometryChunk**, const DisplayLightPtrList**,
                                            const Matrix4**, unsigned int, int) const
{
}
void WaterMaterial::RenderPrimitives(Display*, ViewPort*, GeometryChunk*, const DisplayLightPtrList*,
                                             const Matrix4*, unsigned int, const unsigned short*, unsigned int,
                                             DPrimitive, int) const
{
}
bool WaterMaterial::Initialize(const ResourceDatabase*, const WaterManagerConfig*, FrameAnimation*,
                               WaterTextureAnimation*, int)
{
    return false;
}
bool WaterMaterial::SetReflectionType(int) { return false; }
int WaterMaterial::GetReflectionType() const { return 0; }
void WaterMaterial::SetDetailAnim(bool) {}
bool WaterMaterial::GetDetailAnim() const { return false; }
void WaterMaterial::SetScaleNormXY(float) {}
float WaterMaterial::GetScaleNormXY() const { return 0.0f; }
void WaterMaterial::Update(float) {}
void WaterMaterial::SetRefractiveIndex(float) {}

namespace {
// x87 loads/stores quiet signalling NaNs exactly as the original setters.
void CopyFloatBlock(float* destination, const float* source, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        __asm {
            mov eax, source
            mov edx, destination
            mov ecx, i
            fld dword ptr [eax+ecx*4]
            fstp dword ptr [edx+ecx*4]
        }
    }
}
}  // namespace

void WaterMaterial::SetWaterColor(const Color& color) { CopyFloatBlock(&waterColor.r, &color.r, 4); }
const Color& WaterMaterial::GetWaterColor() const { return waterColor; }
void WaterMaterial::SetAmbientColor(const Color& color) { CopyFloatBlock(&ambientColor.r, &color.r, 4); }
const Color& WaterMaterial::GetAmbientColor() const { return ambientColor; }
void WaterMaterial::SetLightDir(const Vector3& direction) { CopyFloatBlock(&lightDir.x, &direction.x, 3); }
const Vector3& WaterMaterial::GetLightDir() const { return lightDir; }
void WaterMaterial::SetLightColor(const Color& color) { CopyFloatBlock(&lightColor.r, &color.r, 4); }
const Color& WaterMaterial::GetLightColor() const { return lightColor; }
void WaterMaterial::SetUVScrollVelocity(Vector2) {}
Vector2 WaterMaterial::GetUVScrollVelocity() const { return Vector2(0.0f, 0.0f); }
void WaterMaterial::EnableDetailAnimInterpolation(bool) {}
void WaterMaterial::SetReflectEyeVector(bool value) { reflectEyeVector = value; }
bool WaterMaterial::IsReflectEyeVector() const { return reflectEyeVector; }
bool WaterMaterial::GetUseVertexProgram() { return true; }

GeometryChunk* WaterMaterial::CreateGeometryChunk(unsigned int, unsigned int, unsigned int, unsigned int,
                                                          GeometryType, GeometryChunkCreateHint*, bool) const
{
    return nullptr;
}

}  // namespace Jet