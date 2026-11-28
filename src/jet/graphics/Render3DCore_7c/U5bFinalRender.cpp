// Render3DCore_7c U5b wave 5 (U5bFinal): the multipass material render, cache and geometry-chunk paths.
//
// The multipass materials drive a stack MultiPassRenderState built per chunk/pass; the state's construction and
// leaves that only touch the state block are local (U5bFinalState.cpp), the per-light property pass is still the
// original's. Everything here is the original's cdecl entry ABI (`this` on the stack).
#include "U5bFinal.h"

#include <jet/DynamicArray.h>
#include <jet/Render3DCoreGeometry.h>
#include <jet/Render3DCoreMaterials.h>

#include <cstring>

namespace Jet {
namespace MultiPass {
namespace Final {

namespace {

constexpr unsigned int kMaterialOpacity = 0x80;
constexpr unsigned int kMaterialFlags = 0x84;
constexpr unsigned int kMaterialChunkCache = 0x9c;
constexpr unsigned int kMaterialBlendCache = 0xa0;
constexpr unsigned int kSmoothMode = 0xac;
constexpr unsigned int kSingleMode = 0xad;
constexpr unsigned int kIntensityMode = 0xae;
constexpr unsigned int kDebugMode = 0xaf;
constexpr unsigned int kLightRender0 = 0xb0;
constexpr unsigned int kLightRender1 = 0xb4;
constexpr unsigned int kTextureArray = 0xb8;
constexpr unsigned int kPrimaryTexture = 0xc4;
constexpr unsigned int kAlphaHint = 0xc8;
constexpr unsigned int kVertexType = 0xcc;
constexpr unsigned int kBumpTexture = 0xd0;

using MaterialAddFn = void(__cdecl*)(void*, void*, const void*, void*, const void*, const void*);
using BlendAddFn = void(__cdecl*)(void*, void*, const void*, void*, const void*, const void*);
using ChunkVFn = void(__cdecl*)(void*);

struct ChunkHeader {
    void* vtable;
};

void* ChunkCacheAdd() { return Original("?Add@MaterialChunkCache@Jet@@QAAXPAVDisplay@2@PAVViewPort@2@PBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z"); }

void MaterialChunkCacheAdd(void* cache, void* display, void* view, const void* material, void* chunk,
                           const void* lights, const void* modelView)
{
    // MaterialChunkCache::Add is a cdecl leaf with the same argument order.
    using Fn = void(__cdecl*)(void*, void*, void*, const void*, void*, const void*, const void*);
    reinterpret_cast<Fn>(reinterpret_cast<std::uintptr_t>(ChunkCacheAdd()))(cache, display, view, material, chunk, lights, modelView);
}

void AlphaBlendAdd(void* cache, void* display, void* view, const void* material, void* chunk, const void* lights,
                   const void* modelView, float distance)
{
    using Fn = void(__cdecl*)(void*, void*, void*, const void*, void*, const void*, const void*, float);
    reinterpret_cast<Fn>((*reinterpret_cast<void***>(cache))[0x24 / 4])(cache, display, view, material, chunk, lights,
                                                                        modelView, distance);
}

void ChunkAddMaterial(void* cache, void* display, void* view, const void* material, void* chunk, const void* lights,
                      const void* modelView)
{
    using Fn = void(__cdecl*)(void*, void*, void*, const void*, void*, const void*, const void*);
    reinterpret_cast<Fn>((*reinterpret_cast<void***>(cache))[0x24 / 4])(cache, display, view, material, chunk, lights,
                                                                        modelView);
}

float ChunkOpacity(void* chunk) { return Field<float>(chunk, 0x3c); }
float MaterialOpacity(void* material) { return Field<float>(material, kMaterialOpacity); }

void BuildStateAndRender(void* state, void* material, void* display, void* view, void* chunk, const void* lights,
                         const void* modelView, int numIndices, const unsigned short* indices, int numPrimitives,
                         int primitive, bool withPrimitives)
{
    const float opacity = MaterialOpacity(material) * ChunkOpacity(chunk);
    const bool opaque = 1.0f > opacity;
    Thunks* const thunks = reinterpret_cast<Thunks*>(state);
    if (withPrimitives)
        thunks->StateCtor2(material, display, view, chunk, lights, opacity, opaque, modelView,
                           (void*)(Bytes(material) + kTextureArray), Field<unsigned char>(material, kSmoothMode) != 0,
                           false, static_cast<unsigned int>(numIndices), indices,
                           static_cast<unsigned int>(numPrimitives), primitive);
    else
        thunks->StateCtor1(material, display, view, chunk, lights, opacity, opaque, modelView,
                           (void*)(Bytes(material) + kTextureArray), Field<unsigned char>(material, kSmoothMode) != 0,
                           false);

    void* const lightRender = Field<void*>(material, Field<unsigned char>(material, kIntensityMode) ? kLightRender1
                                                                                                   : kLightRender0);
    StateRenderSingleLightPasses(state, lightRender);
    StateCleanUp(state);
}

void RestoreDisplayState(void* material, void* display)
{
    Display* const d = static_cast<Display*>(display);
    d->SetCullMode(static_cast<DCull>(1));
    SetAlphaBlendModeRaw(display, Field<unsigned char>(material, 0x1c));
    d->SetAlphaBlendFunction(static_cast<Src::Factor>(Field<int>(material, 0x14)),
                             static_cast<Dst::Factor>(Field<int>(material, 0x18)));
    reinterpret_cast<void(__cdecl*)(void*, int)>((*reinterpret_cast<void***>(display))[0x160 / 4])(display, 0);
}

}  // namespace

// ---- MultiPassNoTextureMaterial ---------------------------------------------------------------------------------

void __cdecl NoTextureBind(void* self, void* display, void* view, unsigned int pass)
{
    if (pass == 0) NoTextureSetDisplayState(self, display);
    Display* const d = static_cast<Display*>(display);
    d->SetAlphaBlendFunction(static_cast<Src::Factor>(Field<int>(self, 0x78)),
                             static_cast<Dst::Factor>(Field<int>(self, 0x7c)));
    d->SetAlphaBlendMode(true);
    reinterpret_cast<void(__cdecl*)(void*, int)>((*reinterpret_cast<void***>(display))[0xac / 4])(display, 0);
}

void __cdecl NoTextureUnbind(void* self, void* display, void* view, unsigned int pass)
{
    (void)view;
    if (pass == 0) {
        Display* const d = static_cast<Display*>(display);
        d->SetCullMode(static_cast<DCull>(1));
        SetAlphaBlendModeRaw(display, Field<unsigned char>(self, 0x1c));
        d->SetAlphaBlendFunction(static_cast<Src::Factor>(Field<int>(self, 0x14)),
                                 static_cast<Dst::Factor>(Field<int>(self, 0x18)));
        reinterpret_cast<void(__cdecl*)(void*, int)>((*reinterpret_cast<void***>(display))[0x160 / 4])(display, 0);
    }
    reinterpret_cast<void(__cdecl*)(void*, int)>((*reinterpret_cast<void***>(display))[0xac / 4])(display, 1);
}

void __cdecl NoTextureSetDisplayState(void* self, void* display)
{
    Display* const d = static_cast<Display*>(display);
    d->SetLightingMode(false);
    reinterpret_cast<void(__cdecl*)(void*, int)>((*reinterpret_cast<void***>(display))[0xac / 4])(display, 1);
    d->SetCullMode(static_cast<DCull>((Field<unsigned char>(self, kMaterialFlags) & 1) ? 0 : 1));
    reinterpret_cast<void(__cdecl*)(void*, int)>((*reinterpret_cast<void***>(display))[0x160 / 4])(
        display, (Field<unsigned char>(self, kMaterialFlags) >> 2) & 1);

    unsigned char* const state = DisplayState(display);
    Field<unsigned char>(self, 0x1c) = state[0xa1] & 1;
    Field<int>(self, 0x14) = *reinterpret_cast<int*>(state + 0x68);
    Field<int>(self, 0x18) = *reinterpret_cast<int*>(state + 0x6c);
}

void __cdecl NoTextureClearDisplayState(void* self, void* display)
{
    Display* const d = static_cast<Display*>(display);
    d->SetCullMode(static_cast<DCull>(1));
    SetAlphaBlendModeRaw(display, Field<unsigned char>(self, 0x1c));
    d->SetAlphaBlendFunction(static_cast<Src::Factor>(Field<int>(self, 0x14)),
                             static_cast<Dst::Factor>(Field<int>(self, 0x18)));
    reinterpret_cast<void(__cdecl*)(void*, int)>((*reinterpret_cast<void***>(display))[0x160 / 4])(display, 0);
}

void __cdecl NoTextureRenderState(void* self, void* state)
{
    void* const lightRender = Field<void*>(self, Field<unsigned char>(self, kIntensityMode) ? kLightRender1
                                                                                           : kLightRender0);
    StateRenderSingleLightPasses(state, lightRender);
}

void __cdecl NoTextureAddProperty(void* self, const PString& name, const PString& value)
{
    PStringCache* const cache = PString::Cache();
    PString lowerName;
    PString lowerValue;
    if (cache) {
        lowerName.node = cache->ToLower(name.node);
        lowerValue.node = cache->ToLower(value.node);
    }
    static const PString falseNode("false");
    static const PString offNode("off");
    static const PString smoothNode("smoothblend");
    static const PString singleNode("singlepass");
    static const PString intensityNode("intensity");
    static const PString debugNode("debug");

    const bool on = lowerValue.node != falseNode.node && lowerValue.node != offNode.node;
    if (lowerName.node == smoothNode.node) Field<unsigned char>(self, kSmoothMode) = on;
    else if (lowerName.node == singleNode.node) Field<unsigned char>(self, kSingleMode) = on;
    else if (lowerName.node == intensityNode.node) Field<unsigned char>(self, kIntensityMode) = on;
    else if (lowerName.node == debugNode.node) Field<unsigned char>(self, kDebugMode) = on;

    static_cast<Material*>(self)->Material::AddProperty(name, value);
}

void __cdecl NoTextureCacheChunk(void* self, void* display, void* view, void* chunk, const void* lights,
                                 const void* modelView)
{
    const float product = MaterialOpacity(self) * ChunkOpacity(chunk);
    if (1.0f <= product) {
        MaterialChunkCacheAdd(Field<void*>(self, kMaterialChunkCache), display, view, self, chunk, lights, modelView);
        return;
    }
    AlphaBlendAdd(Field<void*>(self, kMaterialBlendCache), display, view, self, chunk, lights, modelView, 0.0f);
}

void __cdecl NoTextureRenderChunk(void* self, void* display, void* view, void* chunk, const void* lights,
                                  const void* modelView, int pass)
{
    (void)pass;
    if (!view) return;
    NoTextureSetDisplayState(self, display);
    unsigned char state[0x120];
    BuildStateAndRender(state, self, display, view, chunk, lights, modelView, 0, nullptr, 0, 0, false);
    reinterpret_cast<Thunks*>(state)->StateDtor();
    RestoreDisplayState(self, display);
}

void __cdecl NoTextureRenderNumChunks(void* self, void* display, void* view, void** chunks, const void** lights,
                                      const void** modelViews, unsigned int numChunks, int pass)
{
    (void)pass;
    if (!view) return;
    NoTextureSetDisplayState(self, display);
    for (unsigned int i = 0; i < numChunks; ++i) {
        unsigned char state[0x120];
        BuildStateAndRender(state, self, display, view, chunks[i], lights[i], modelViews[i], 0, nullptr, 0, 0, false);
        reinterpret_cast<Thunks*>(state)->StateDtor();
    }
    RestoreDisplayState(self, display);
}

void __cdecl NoTextureRenderPrimitives(void* self, void* display, void* view, void* chunk, const void* lights,
                                       const void* modelView, unsigned int numIndices, const unsigned short* indices,
                                       unsigned int numPrimitives, int primitive, int pass)
{
    (void)pass;
    if (!view) return;
    NoTextureSetDisplayState(self, display);
    unsigned char state[0x120];
    BuildStateAndRender(state, self, display, view, chunk, lights, modelView, static_cast<int>(numIndices), indices,
                        static_cast<int>(numPrimitives), primitive, true);
    reinterpret_cast<Thunks*>(state)->StateDtor();
    RestoreDisplayState(self, display);
}

// ---- MultiPassOneTextureMaterial --------------------------------------------------------------------------------

void __cdecl OneTextureBind(void* self, void* display, void* view, unsigned int pass)
{
    if (pass == 0) NoTextureSetDisplayState(self, display);
    void* const texture = Field<void*>(self, kPrimaryTexture);
    if (texture) {
        reinterpret_cast<void(__cdecl*)(void*, int)>((*reinterpret_cast<void***>(texture))[0x44 / 4])(texture, 0);
    } else {
        reinterpret_cast<void(__cdecl*)(void*, int)>((*reinterpret_cast<void***>(display))[0xa0 / 4])(display, 0);
    }
    Display* const d = static_cast<Display*>(display);
    d->SetAlphaBlendFunction(static_cast<Src::Factor>(Field<int>(self, 0x78)),
                             static_cast<Dst::Factor>(Field<int>(self, 0x7c)));
    d->SetAlphaBlendMode(true);
}

void __cdecl OneTextureUnbind(void* self, void* display, void* view, unsigned int pass)
{
    (void)view;
    if (pass == 0) {
        Display* const d = static_cast<Display*>(display);
        d->SetCullMode(static_cast<DCull>(1));
        SetAlphaBlendModeRaw(display, Field<unsigned char>(self, 0x1c));
        d->SetAlphaBlendFunction(static_cast<Src::Factor>(Field<int>(self, 0x14)),
                                 static_cast<Dst::Factor>(Field<int>(self, 0x18)));
        reinterpret_cast<void(__cdecl*)(void*, int)>((*reinterpret_cast<void***>(display))[0x160 / 4])(display, 0);
    }
}

void __cdecl OneTextureCacheChunk(void* self, void* display, void* view, void* chunk, const void* lights,
                                  const void* modelView)
{
    if (Field<int>(self, kAlphaHint) == -1) {
        int result = 0;
        using IsLoadedFn = void(__cdecl*)(void*, void*);
        reinterpret_cast<IsLoadedFn>((*reinterpret_cast<void***>(self))[0x58 / 4])(self, &result);
        if (result == 0) {
            void* const texture = Field<void*>(self, kPrimaryTexture);
            Field<int>(self, kAlphaHint) =
                reinterpret_cast<int(__cdecl*)(void*)>((*reinterpret_cast<void***>(texture))[0x24 / 4])(texture);
        }
    }
    const float product = MaterialOpacity(self) * ChunkOpacity(chunk);
    const bool opaque = Field<int>(self, kAlphaHint) != 1 && 1.0f <= product;
    if (opaque) {
        MaterialChunkCacheAdd(Field<void*>(self, kMaterialChunkCache), display, view, self, chunk, lights, modelView);
        return;
    }
    AlphaBlendAdd(Field<void*>(self, kMaterialBlendCache), display, view, self, chunk, lights, modelView, 0.0f);
}

void __cdecl OneTextureRenderChunk(void* self, void* display, void* view, void* chunk, const void* lights,
                                   const void* modelView, int pass)
{
    (void)pass;
    if (!view) return;
    NoTextureSetDisplayState(self, display);
    if (Field<int>(view, 0x60) > 1) {
        unsigned char state[0x120];
        BuildStateAndRender(state, self, display, view, chunk, lights, modelView, 0, nullptr, 0, 0, false);
        reinterpret_cast<Thunks*>(state)->StateDtor();
    } else {
        using PerVertexFn = void(__cdecl*)(void*, void*, void*, void*, const void*, const void*, int, unsigned int,
                                           const unsigned short*, unsigned int, int);
        reinterpret_cast<PerVertexFn>((*reinterpret_cast<void***>(self))[0x64 / 4])(
            self, display, view, chunk, lights, modelView, pass, 0, nullptr, 0, 3);
    }
    RestoreDisplayState(self, display);
}

void __cdecl OneTextureRenderNumChunks(void* self, void* display, void* view, void** chunks, const void** lights,
                                       const void** modelViews, unsigned int numChunks, int pass)
{
    (void)pass;
    if (!view) return;
    NoTextureSetDisplayState(self, display);
    for (unsigned int i = 0; i < numChunks; ++i) {
        unsigned char state[0x120];
        BuildStateAndRender(state, self, display, view, chunks[i], lights[i], modelViews[i], 0, nullptr, 0, 0, false);
        reinterpret_cast<Thunks*>(state)->StateDtor();
    }
    RestoreDisplayState(self, display);
}

void __cdecl OneTextureRenderPrimitives(void* self, void* display, void* view, void* chunk, const void* lights,
                                        const void* modelView, unsigned int numIndices, const unsigned short* indices,
                                        unsigned int numPrimitives, int primitive, int pass)
{
    (void)pass;
    if (!view) return;
    NoTextureSetDisplayState(self, display);
    unsigned char state[0x120];
    BuildStateAndRender(state, self, display, view, chunk, lights, modelView, static_cast<int>(numIndices), indices,
                        static_cast<int>(numPrimitives), primitive, true);
    reinterpret_cast<Thunks*>(state)->StateDtor();
    RestoreDisplayState(self, display);
}

// ---- geometry chunks --------------------------------------------------------------------------------------------

namespace {

void* AllocateMultiPassChunk(unsigned int vertices, unsigned int primitives, unsigned int indices,
                             unsigned int faceNormals, unsigned int flags, bool tangents2, void* self)
{
    void* const memory = Alloc::New(0x64);
    if (!memory) return nullptr;
    using CtorFn = void*(__thiscall*)(void*, unsigned, unsigned, unsigned, unsigned, unsigned);
    const char* const name = tangents2 ? "??0MultiPassBumpChunkT2@Jet@@QAE@IIIII@Z"
                                       : "??0MultiPassBumpChunkT@Jet@@QAE@IIIII@Z";
    reinterpret_cast<CtorFn>(Local(name))(memory, vertices, primitives, indices, faceNormals, flags);
    (void)self;
    return memory;
}

}  // namespace

void* __cdecl CreateGeometryChunkShared(void* self, unsigned int vertices, unsigned int primitives,
                                       unsigned int indices, unsigned int faceNormals, int type, void* hint)
{
    const unsigned int flags = Material::MakeGeometryChunkFlags(static_cast<Material::GeometryType>(type),
                                                                static_cast<GeometryChunkCreateHint*>(hint));
    using AllocFn = void*(__cdecl*)(void*, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
    using RawAllocFn = void*(__cdecl*)(void*, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
    void* const chunk = reinterpret_cast<RawAllocFn>((*reinterpret_cast<void***>(self))[0x60 / 4])(
        self, vertices, primitives, indices, faceNormals, flags);
    FinishChunkAllocation(chunk, hint, false);
    void* const vertexType = Field<void*>(self, kVertexType);
    if (vertexType)
        reinterpret_cast<void(__cdecl*)(void*, void*, int)>((*reinterpret_cast<void***>(chunk))[0xa0 / 4])(
            chunk, vertexType, 0);
    return chunk;
}

void* __cdecl NoTextureCreateGeometryChunk(void* self, unsigned int vertices, unsigned int primitives,
                                          unsigned int indices, unsigned int faceNormals, int type, void* hint)
{
    (void)self;
    const unsigned int flags = Material::MakeGeometryChunkFlags(static_cast<Material::GeometryType>(type),
                                                                static_cast<GeometryChunkCreateHint*>(hint));
    const bool nml = hint && (Field<unsigned int>(hint, 8) & 1) && (Field<unsigned int>(hint, 0x10) & 1);
    void* chunk;
    if (nml) {
        chunk = Alloc::New(sizeof(PackedVertexPCGeometryNmlChunk));
        if (chunk)
            ::new (chunk) PackedVertexPCGeometryNmlChunk(vertices, primitives, indices, faceNormals, flags);
    } else {
        chunk = Alloc::New(sizeof(PackedVertexPNGeometryChunk));
        if (chunk) ::new (chunk) PackedVertexPNGeometryChunk(vertices, primitives, indices, faceNormals, flags);
    }
    FinishChunkAllocation(chunk, hint, nml);
    return chunk;
}

void* __cdecl OneTextureCreateGeometryChunk(void* self, unsigned int vertices, unsigned int primitives,
                                           unsigned int indices, unsigned int faceNormals, int type, void* hint)
{
    const unsigned int flags = Material::MakeGeometryChunkFlags(static_cast<Material::GeometryType>(type),
                                                                static_cast<GeometryChunkCreateHint*>(hint));
    const bool nml = hint && (Field<unsigned int>(hint, 8) & 1) && (Field<unsigned int>(hint, 0x10) & 1);
    void* chunk;
    if (nml) {
        chunk = Alloc::New(sizeof(PackedVertexPCTGeometryNmlChunk));
        if (chunk)
            ::new (chunk) PackedVertexPCTGeometryNmlChunk(vertices, primitives, indices, faceNormals, flags);
    } else {
        chunk = Alloc::New(sizeof(PackedVertexPNTGeometryChunk));
        if (chunk) ::new (chunk) PackedVertexPNTGeometryChunk(vertices, primitives, indices, faceNormals, flags);
    }
    FinishChunkAllocation(chunk, hint, nml);
    void* const vertexType = Field<void*>(self, kVertexType);
    if (vertexType)
        reinterpret_cast<void(__cdecl*)(void*, void*, int)>((*reinterpret_cast<void***>(chunk))[0xa0 / 4])(
            chunk, vertexType, 0);
    return chunk;
}

void* __cdecl BumpCreateGeometryChunk(void* self, unsigned int vertices, unsigned int primitives,
                                     unsigned int indices, unsigned int faceNormals, int type, void* hint)
{
    (void)type;
    (void)faceNormals;
    (void)vertices;
    (void)primitives;
    (void)indices;
    return CreateGeometryChunkShared(self, vertices, primitives, indices, faceNormals, type, hint);
}

void* __cdecl OneTextureAllocateGeometryMemory(void* self, unsigned int vertices, unsigned int primitives,
                                               unsigned int indices, unsigned int faceNormals, unsigned int flags)
{
    (void)self;
    void* const chunk = Alloc::New(sizeof(PackedVertexPNTGeometryChunk));
    if (!chunk) return nullptr;
    ::new (chunk) PackedVertexPNTGeometryChunk(vertices, primitives, indices, faceNormals, flags);
    return chunk;
}

void* __cdecl BumpAllocateGeometryMemory(void* self, unsigned int vertices, unsigned int primitives,
                                          unsigned int indices, unsigned int faceNormals, unsigned int flags)
{
    (void)self;
    return AllocateMultiPassChunk(vertices, primitives, indices, faceNormals, flags, false, self);
}

void* __cdecl Bump2AllocateGeometryMemory(void* self, unsigned int vertices, unsigned int primitives,
                                           unsigned int indices, unsigned int faceNormals, unsigned int flags)
{
    (void)self;
    return AllocateMultiPassChunk(vertices, primitives, indices, faceNormals, flags, true, self);
}

void __cdecl FinishChunkAllocation(void* chunk, void* hint, bool dynamicHint)
{
    (void)dynamicHint;
    bool dynamic = false;
    unsigned int version = 0;
    if (hint) {
        version = Field<unsigned int>(hint, 0);
        unsigned int bits = Field<unsigned int>(hint, 0x10);
        const bool bit1 = (bits & 2) != 0;
        if (bit1 && (bits & 4)) dynamic = true;
        bits &= ~1u;
        if (bit1 && !dynamic) {
            bits |= 4;
            dynamic = true;
        }
        Field<unsigned int>(hint, 0x10) = bits;
    }
    if (!hint || !(Field<unsigned int>(hint, 8) & 1)) dynamic = false;
    if (hint && (version == 1 || dynamic))
        reinterpret_cast<void(__cdecl*)(void*, int, int, int)>((*reinterpret_cast<void***>(chunk))[0x64 / 4])(
            chunk, 0, -2, 1);
    if (dynamic)
        reinterpret_cast<void(__cdecl*)(void*, int, int, int)>((*reinterpret_cast<void***>(chunk))[0x6c / 4])(
            chunk, 0, 0, 0);
    if (hint && Field<unsigned int>(hint, 4) == 1)
        reinterpret_cast<void(__cdecl*)(void*)>((*reinterpret_cast<void***>(chunk))[0x74 / 4])(chunk);
}

}  // namespace Final
}  // namespace MultiPass
}  // namespace Jet
