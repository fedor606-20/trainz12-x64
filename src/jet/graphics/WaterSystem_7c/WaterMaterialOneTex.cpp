// WaterSystem_7c U8b: WaterMaterialOneTex. Behaviour follows the TS12 assembly (addresses per function). The class
// keeps one texture at +0xe4 and inherits the whole base state; the render paths use the same display calls as the
// original.
#include "WaterMaterialOneTex.h"

#include <jet/Geometry.h>
#include <jet/RenderSystem.h>
#include <jet/VCall.h>
#include <jet/WaterManagerConfig.h>
#include <jet/WaterTextureAnimation.h>

#include <cstring>

#include "WaterMaterialFresWave.h"  // the shared chunk helper and the camera accessor

namespace Jet {

// ---------------------------------------------------------------------------------------------------------------
// statics

const UId WaterMaterialOneTex::uid("4F7EA82E-1CB4-47B4-B1C2-2FCEA5A7D7CA");
const PString WaterMaterialOneTex::UNDECORATED_TYPE("WaterMaterialOneTex");
const PString WaterMaterialOneTex::DECORATED_TYPE("m.WaterMaterialOneTex");

namespace {

// the camera the viewport exposes at +0x20 (the original passes it to Material::SetLightingState)
const Matrix4* ViewMatrix(ViewPort* view)
{
    if (!view) return nullptr;
    const Camera* const camera =
        *reinterpret_cast<Camera* const*>(reinterpret_cast<unsigned char*>(view) + 0x20);
    return camera ? GetWaterViewTransform(camera) : nullptr;
}

}  // namespace

// 0x130d0
WaterMaterialOneTex::WaterMaterialOneTex(MaterialManager* manager) : WaterMaterial(manager), texture(nullptr) {}

// 0x13190
WaterMaterialOneTex::~WaterMaterialOneTex()
{
    if (texture) texture->ReleaseReference();
}

// 0x13250
float WaterMaterialOneTex::GetScaleNormXY() const { return 1.0f; }

// 0x3540 (shared with WaterMaterial's no-op setter)
void WaterMaterialOneTex::SetScaleNormXY(float) {}

// 0x13040
void WaterMaterialOneTex::SetDisplayState(Display* display) const
{
    display->SetAlphaBlendMode(false);
    display->SetCullMode(static_cast<DCull>((flags & 1) ? 0 : 1));
    if (texture)
        texture->Bind(0);
    else
        display->SetTexture(0);
}

// 0x130b0
void WaterMaterialOneTex::ClearDisplayState(Display* display) const
{
    display->SetAlphaBlendMode(false);
    display->SetCullMode(static_cast<DCull>((flags & 1) ? 0 : 1));
}

// 0x13260
Material* __cdecl WaterMaterialOneTex::CreateInstance() const
{
    Material* const material = Material::CreateInstance(UNDECORATED_TYPE, materialManager);
    if (material) WaterMaterial::InitInstance(*static_cast<WaterMaterial*>(material));
    return material;
}

// 0x13290
bool __cdecl WaterMaterialOneTex::operator==(const Material& other) const
{
    if (!dynamic_cast<const WaterMaterialOneTex*>(&other)) return false;
    return Material::Equals(other);
}

// 0x132c0
const PString& __cdecl WaterMaterialOneTex::GetType() const { return UNDECORATED_TYPE; }

// 0x13230
bool WaterMaterialOneTex::Initialize(const ResourceDatabase*, const WaterManagerConfig*, FrameAnimation*,
                                     WaterTextureAnimation*, int)
{
    return true;
}

// 0x13240
bool WaterMaterialOneTex::SetReflectionType(int) { return true; }

// 0x15db0 (shared stub)
int WaterMaterialOneTex::GetReflectionType() const { return 0; }

// 0x134b0
void __cdecl WaterMaterialOneTex::Bind(Display* display, ViewPort* view, unsigned int pass)
{
    if (pass == 0) VCall<void>(display, 0xc0, view);
}

// 0x134d0
void __cdecl WaterMaterialOneTex::Unbind(Display* display, ViewPort* view, unsigned int pass)
{
    if (pass == 0) VCall<void>(display, 0xc4, view);
}

// 0x134f0
void __cdecl WaterMaterialOneTex::CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                             const DisplayLightPtrList* lights, const Matrix4* modelView) const
{
    if (renderCache) VCall<void>(renderCache, 0x24, display, view, static_cast<const Material*>(this), chunk, lights,
                                 modelView);
}

// 0x13520
void __cdecl WaterMaterialOneTex::RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                              const DisplayLightPtrList* lights, const Matrix4* modelView,
                                              int pass) const
{
    const VertexProgram* const program = chunk ? chunk->GetVertexProgram(0) : nullptr;
    VCall<void>(display, 0xc0, view);
    SetWaterMaterialLightingState(this, display, modelView, chunk, program, lights, ViewMatrix(view), opacity, (flags & 1) != 0);
    VCall<void>(display, 0x54, program);
    if (chunk) chunk->RenderGeometry(display, view, program, true, 0);
    VCall<void>(display, 0xc4, view);
    (void)pass;
}

// 0x135f0
void __cdecl WaterMaterialOneTex::RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                                  const DisplayLightPtrList** lights,
                                                  const Matrix4** modelViews, unsigned int numChunks,
                                                  int pass) const
{
    VCall<void>(display, 0xc0, view);
    for (unsigned int i = 0; i < numChunks; ++i) {
        GeometryChunk* const chunk = chunks[i];
        if (!chunk) continue;
        const VertexProgram* const program = chunk->GetVertexProgram(0);
        SetWaterMaterialLightingState(this, display, modelViews[i], chunk, program, lights[i], ViewMatrix(view), opacity, (flags & 1) != 0);
        VCall<void>(display, 0x54, program);
        chunk->RenderGeometry(display, view, program, true, 0);
    }
    VCall<void>(display, 0xc4, view);
    (void)pass;
}

// 0x13720
void __cdecl WaterMaterialOneTex::RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                                   const DisplayLightPtrList* lights, const Matrix4* modelView,
                                                   unsigned int numIndices, const unsigned short* indices,
                                                   unsigned int numPrimitives, DPrimitive primitive,
                                                   int pass) const
{
    const VertexProgram* const program = chunk ? chunk->GetVertexProgram(0) : nullptr;
    VCall<void>(display, 0xc0, view);
    SetWaterMaterialLightingState(this, display, modelView, chunk, program, lights, ViewMatrix(view), opacity, (flags & 1) != 0);
    VCall<void>(display, 0x54, program);
    if (chunk)
        chunk->RenderPrimitives(display, view, program, numIndices, indices, numPrimitives, primitive, true, 0);
    VCall<void>(display, 0xc4, view);
    (void)pass;
}

// 0x13430
WaterMaterialOneTex::Result __cdecl WaterMaterialOneTex::DuplicateTexture(int type)
{
    if (type != 1) return Result(2);
    if (!texture) return Result(3);
    Texture* const copy = texture->CreateInstance();
    texture->ReleaseReference();
    texture = copy;
    return Result(0);
}

// 0x13490 (both the const and non-const export)
Texture* __cdecl WaterMaterialOneTex::GetTexture(int type) { return type == 1 ? texture : nullptr; }
const Texture* __cdecl WaterMaterialOneTex::GetTexture(int type) const { return type == 1 ? texture : nullptr; }

// 0x13390
WaterMaterialOneTex::Result __cdecl WaterMaterialOneTex::SetTexture(int type, const AutoReference<Texture>& value,
                                                                    float)
{
    if (type != 6) return Result(2);
    Texture* const source = value.Get();
    if (!source) return Result(3);
    Texture* const old = texture;
    texture = source;
    if (texture) texture->AddReference();
    if (old) old->ReleaseReference();
    texture->SetResourcePriority(resourcePriority);
    SetWaterMaterialTextureDefinition(this, type, texture->GetResourceName(), 1.0f);
    return Result(0);
}

// 0x132d0
WaterMaterialOneTex::Result __cdecl WaterMaterialOneTex::SetTexture(int type, const ResourceDatabase* database,
                                                                    const PString& name, float amount,
                                                                    const String& request)
{
    if (type != 6) return Result(2);
    if (amount == 0.0f) return Result(3);
    TextureManager* const manager =
        *reinterpret_cast<TextureManager**>(reinterpret_cast<unsigned char*>(materialManager) + 0xc);
    if (!manager) return Result(3);
    Texture* const made = manager->Create(database, name, request);
    if (texture) texture->ReleaseReference();
    texture = made;
    if (!texture) return Result(3);
    texture->SetResourcePriority(resourcePriority);
    SetWaterMaterialTextureDefinition(this, type, name, amount);
    return Result(0);
}

// 0x100036c0: the six-argument wrapper forwards to the seven-argument virtual
GeometryChunk* __cdecl WaterMaterialOneTex::CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                                                unsigned int numIndices, unsigned int numFaceNormals,
                                                                GeometryType type, GeometryChunkCreateHint* hint) const
{
    return CreateGeometryChunk(numVertices, numPrimitives, numIndices, numFaceNormals, type, hint, false);
}

// 0x13020 (vbase destructor is the compiler's)
// 0x13800
GeometryChunk* __cdecl WaterMaterialOneTex::CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                                                unsigned int numIndices, unsigned int numFaceNormals,
                                                                GeometryType type, GeometryChunkCreateHint* hint,
                                                                bool) const
{
    return CreateOneTexGeometryChunk(numVertices, numPrimitives, numIndices, numFaceNormals, type, hint);
}

// the original has a non-virtual one-texture Activate through the base; the vtable slot is the shared body
void __cdecl WaterMaterialOneTex::Activate(int) {}

// 0x13430? the original's IsLoaded walks the texture's availability
WaterMaterialOneTex::Result __cdecl WaterMaterialOneTex::IsLoaded() const
{
    if (texture && !texture->IsAvailable()) return Result(4);
    return Result(0);
}

}  // namespace Jet





