// Differential test of Render3DCore_7c U5b last wave (U5bLast): the bump tangent generator, the six light-type
// initialisers of the multipass plugins, InitializeLightRenders, the empty LoadLightPrograms path and the SetScissor
// early exit, ours against Render3DCore_7c_orig.dll in one process.
//
// usage: diff_r3d_u5b_last.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

LONG CALLBACK VectoredHandler(EXCEPTION_POINTERS* info)
{
    HMODULE module = nullptr;
    const void* address = info->ExceptionRecord->ExceptionAddress;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCSTR>(address), &module);
    char name[MAX_PATH] = "?";
    if (module) GetModuleFileNameA(module, name, sizeof name);
    std::printf("FAULT %p code %08x in %s+%x\n", address, info->ExceptionRecord->ExceptionCode, name,
                module ? unsigned(std::uintptr_t(address) - std::uintptr_t(module)) : 0);
    std::fflush(stdout);
    return EXCEPTION_CONTINUE_SEARCH;
}

unsigned long long checks, bad;
void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        std::printf("MISMATCH %s\n", what);
    }
}
void CheckBytes(const void* a, const void* b, unsigned n, const char* what)
{
    Check(std::memcmp(a, b, n) == 0, what);
}
void CheckFloats(const float* a, const float* b, unsigned n, const char* what)
{
    for (unsigned i = 0; i < n; ++i)
        if (std::memcmp(&a[i], &b[i], 4) != 0) {
            std::printf("%s at %u: ours %.9g orig %.9g\n", what, i, a[i], b[i]);
            Check(false, what);
            return;
        }
    Check(true, what);
}

HMODULE ours, orig, renderSystem;

template <class F>
F Proc(HMODULE module, const char* name, const char* side)
{
    auto fn = reinterpret_cast<F>(GetProcAddress(module, name));
    if (!fn) {
        std::printf("missing export %s in %s\n", name, side);
        std::exit(2);
    }
    return fn;
}

struct Patch {
    unsigned char* target = nullptr;
    unsigned char saved[5] = {};
    bool active = false;
};
void PatchEnter(Patch& patch, void* target, void* hook)
{
    patch.target = static_cast<unsigned char*>(target);
    DWORD old = 0;
    VirtualProtect(patch.target, 5, PAGE_EXECUTE_READWRITE, &old);
    std::memcpy(patch.saved, patch.target, 5);
    patch.target[0] = 0xe9;
    const std::intptr_t delta = static_cast<unsigned char*>(hook) - (patch.target + 5);
    *reinterpret_cast<std::int32_t*>(patch.target + 1) = static_cast<std::int32_t>(delta);
    VirtualProtect(patch.target, 5, old, &old);
    patch.active = true;
}
void PatchRestore(Patch& patch)
{
    if (!patch.active) return;
    DWORD old = 0;
    VirtualProtect(patch.target, 5, PAGE_EXECUTE_READWRITE, &old);
    std::memcpy(patch.target, patch.saved, 5);
    VirtualProtect(patch.target, 5, old, &old);
    patch.active = false;
}

struct ItemView {
    unsigned int stride;
    void* data;
};
struct Vec3 {
    float x, y, z;
};
struct Vec2 {
    float x, y;
};

// A GeometryChunk-shaped object for CreateTangentVectors. Only the fields the original reads are present.
struct FakeChunk {
    void** vtable;                 // +0x00
    unsigned char pad04[0x14];     // +0x04
    unsigned int flags;            // +0x18
    unsigned int numPrimitives;    // +0x1c
    unsigned int numIndices;       // +0x20
    unsigned int numVertices;      // +0x24
    unsigned char pad28[4];        // +0x28
    unsigned char* indexBlock;     // +0x2c
};
static_assert(sizeof(FakeChunk) == 0x30, "FakeChunk");

// The index array block GetTriangleIndexes reads: the words start at +0x10.
struct IndexBlock {
    unsigned int refs;
    unsigned int count;
    unsigned int itemSize;
    unsigned int capacity;
    unsigned short data[16];
};
static_assert(offsetof(IndexBlock, data) == 0x10, "IndexBlock");

ItemView g_tangent, g_normal, g_position, g_uv;
int g_lastType;
int g_viewCalls;
void __cdecl GetVector3Item(void* self, ItemView* out, int type, int stream)
{
    (void)self;
    (void)stream;
    g_lastType = type;
    ++g_viewCalls;
    *out = type == 0x1a ? g_tangent : type == 1 ? g_normal : g_position;
}
void __cdecl GetVector2Item(void* self, ItemView* out, int type, int stream)
{
    (void)self;
    (void)stream;
    g_lastType = type;
    *out = g_uv;
}

// A fake plugin vtable: the six per-type initialisers and InitializeLightRenders call only these slots.
struct Recorder {
    int caches;
    int lightRenders;
    int fixed;
    int types[6];
    int loadCalls;
    char vp[8][64];
    char tc[8][64];
};
Recorder g_rec;

bool __cdecl RecCaches(void*, void*)
{
    ++g_rec.caches;
    return true;
}
void __cdecl RecFixed(void*, void*)
{
    ++g_rec.fixed;
}
bool __cdecl RecType(void*, void*)
{
    ++g_rec.types[g_rec.lightRenders % 6];
    return true;
}
bool __cdecl RecLoad(void*, void* render, void*, const char* vp, const char* tc)
{
    using CreatePass = void(__thiscall*)(void*, void*, void*);
    static CreatePass create = Proc<CreatePass>(
        orig, "?CreateNewLightPass@MultiPassLightRender@Jet@@QAEXPAVVertexProgram@2@PAVTextureCombiner@2@@Z", "orig");
    create(render, nullptr, nullptr);
    if (g_rec.loadCalls < 8) {
        std::snprintf(g_rec.vp[g_rec.loadCalls], sizeof g_rec.vp[0], "%s", vp ? vp : "(null)");
        std::snprintf(g_rec.tc[g_rec.loadCalls], sizeof g_rec.tc[0], "%s", tc ? tc : "(null)");
    }
    ++g_rec.loadCalls;
    return true;
}

struct FakePlugin {
    void** vtable;             // +0x00
    unsigned char pad04[0x1c]; // +0x04
    void* blendCache;          // +0x20
    void* chunkCache;          // +0x24
    void* lightRender0;        // +0x28
    void* lightRender1;        // +0x2c
    void* vertexType;          // +0x30
    unsigned char names[0x40]; // +0x34 .. +0x73
};
static_assert(offsetof(FakePlugin, lightRender0) == 0x28, "FakePlugin");
static_assert(offsetof(FakePlugin, names) == 0x34, "FakePlugin names");

void BuildPluginVtable(void** vtable)
{
    std::memset(vtable, 0, 0x48);
    vtable[0x18 / 4] = reinterpret_cast<void*>(&RecCaches);
    vtable[0x24 / 4] = reinterpret_cast<void*>(&RecFixed);
    for (unsigned int i = 0x28; i <= 0x3c; i += 4) vtable[i / 4] = reinterpret_cast<void*>(&RecType);
    vtable[0x40 / 4] = reinterpret_cast<void*>(&RecLoad);
}

using MlrCtor = void*(__thiscall*)(void*);
MlrCtor g_mlrCtor;

void* NewMPLightRender()
{
    void* memory = std::calloc(1, 0x50);
    g_mlrCtor(memory);
    return memory;
}

bool SameMultiPassRender(const void* a, const void* b)
{
    const unsigned char* x = static_cast<const unsigned char*>(a);
    const unsigned char* y = static_cast<const unsigned char*>(b);
    if (std::memcmp(x + 0x20, y + 0x20, 0x18) != 0) {
        std::printf("multi render counts ours:");
        for (int i = 0; i < 6; ++i) std::printf(" %u", *reinterpret_cast<const unsigned int*>(x + 0x20 + i * 4));
        std::printf(" orig:");
        for (int i = 0; i < 6; ++i) std::printf(" %u", *reinterpret_cast<const unsigned int*>(y + 0x20 + i * 4));
        std::printf("\n");
        return false;
    }
    const unsigned int passesA = *reinterpret_cast<const unsigned int*>(x + 0x08);
    const unsigned int passesB = *reinterpret_cast<const unsigned int*>(y + 0x08);
    if (passesA != passesB) {
        std::printf("passes ours %u orig %u\n", passesA, passesB);
        return false;
    }
    const unsigned char* pa = *reinterpret_cast<unsigned char* const*>(x + 0x10);
    const unsigned char* pb = *reinterpret_cast<unsigned char* const*>(y + 0x10);
    if (!pa || !pb) return pa == pb;
    for (unsigned int p = 0; p < passesA; ++p) {
        const unsigned char* ra = pa + p * 0x104;
        const unsigned char* rb = pb + p * 0x104;
        for (unsigned int list = 0; list < 21; ++list) {
            const unsigned int offset =
                list < 1 ? 0x00 : list < 11 ? 0x0c + (list - 1) * 0x0c : 0x84 + (list - 11) * 0x0c;
            const unsigned int countA = *reinterpret_cast<const unsigned int*>(ra + offset);
            const unsigned int countB = *reinterpret_cast<const unsigned int*>(rb + offset);
            if (countA != countB) {
                std::printf("property list %u count ours %u orig %u\n", list, countA, countB);
                return false;
            }
            if (!countA) continue;
            if (countA > 64) return false;
            const unsigned char* dataA = *reinterpret_cast<unsigned char* const*>(ra + offset + 8);
            const unsigned char* dataB = *reinterpret_cast<unsigned char* const*>(rb + offset + 8);
            if (std::memcmp(dataA, dataB, countA * 8) != 0) {
                const unsigned int* ia = reinterpret_cast<const unsigned int*>(dataA);
                const unsigned int* ib = reinterpret_cast<const unsigned int*>(dataB);
                std::printf("property list %u data ours %u,%u orig %u,%u\n", list, ia[0], ia[1], ib[0], ib[1]);
                return false;
            }
        }
    }
    return true;
}

// Compare an MPLightRender: six FastDynamicArray<MPLightRender*> heads, each element compared field by field.
bool SameLightRender(const void* a, const void* b)
{
    const unsigned char* x = static_cast<const unsigned char*>(a);
    const unsigned char* y = static_cast<const unsigned char*>(b);
    for (unsigned int array = 0; array < 6; ++array) {
        const unsigned int offset = 0x08 + array * 0x0c;
        const unsigned int countA = *reinterpret_cast<const unsigned int*>(x + offset);
        const unsigned int countB = *reinterpret_cast<const unsigned int*>(y + offset);
        if (countA != countB) {
            std::printf("mlr array %u count ours %u orig %u\n", array, countA, countB);
            return false;
        }
        if (!countA) continue;
        void** const dataA =
            *reinterpret_cast<void***>(const_cast<unsigned char*>(x) + offset + 8);
        void** const dataB =
            *reinterpret_cast<void***>(const_cast<unsigned char*>(y) + offset + 8);
        for (unsigned int i = 0; i < countA; ++i)
            if (!SameMultiPassRender(dataA[i], dataB[i])) return false;
    }
    return true;
}

using InitFn = bool(__cdecl*)(void*, void*);
using TangentFn = void(__cdecl*)(void*, void*);

void TestTangent()
{
    const unsigned int vertexCount = 5;
    const unsigned int primitiveCount = 0;
    // Two triangles sharing an edge; vertex 3 duplicates vertex 0's position and normal.
    const Vec3 positions[5] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 0}, {1, 1, 0}};
    const Vec3 normals[5] = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
    const Vec2 uvs[5] = {{0, 0}, {1, 0}, {0, 1}, {0, 0}, {1, 1}};
    const unsigned short indices[6] = {0, 1, 2, 0, 2, 4};

    for (int round = 0; round < 3; ++round) {
        Vec3 tangentA[5], tangentB[5];
        std::memset(tangentA, round == 1 ? 0x11 : 0, sizeof tangentA);
        std::memcpy(tangentB, tangentA, sizeof tangentA);

        FakeChunk chunkA{}, chunkB{};
        void* vt[0x34] = {};
        vt[0xc4 / 4] = reinterpret_cast<void*>(&GetVector3Item);
        vt[0xbc / 4] = reinterpret_cast<void*>(&GetVector2Item);
        IndexBlock blockA{}, blockB{};
        blockA.refs = blockB.refs = 1;
        blockA.count = blockB.count = 6;
        blockA.itemSize = blockB.itemSize = 2;
        blockA.capacity = blockB.capacity = 16;
        std::memcpy(blockA.data, indices, sizeof indices);
        std::memcpy(blockB.data, indices, sizeof indices);

        FakeChunk* chunks[2] = {&chunkA, &chunkB};
        for (int ci = 0; ci < 2; ++ci) {
            FakeChunk* chunk = chunks[ci];
            chunk->vtable = vt;
            chunk->flags = 0;
            chunk->numPrimitives = primitiveCount;
            chunk->numIndices = 6;
            chunk->numVertices = vertexCount;
        }
        chunkA.indexBlock = reinterpret_cast<unsigned char*>(&blockA);
        chunkB.indexBlock = reinterpret_cast<unsigned char*>(&blockB);

        g_tangent = {sizeof(Vec3), tangentA};
        g_normal = {sizeof(Vec3), const_cast<Vec3*>(normals)};
        g_position = {sizeof(Vec3), const_cast<Vec3*>(positions)};
        g_uv = {sizeof(Vec2), const_cast<Vec2*>(uvs)};
        TangentFn local = Proc<TangentFn>(
            ours, "?CreateTangentVectors@MultiPassBumpChunkBase@Jet@@UAAXPAVGeometryChunk@2@@Z", "ours");
        local(nullptr, &chunkA);

        g_tangent = {sizeof(Vec3), tangentB};
        TangentFn original = Proc<TangentFn>(
            orig, "?CreateTangentVectors@MultiPassBumpChunkBase@Jet@@UAAXPAVGeometryChunk@2@@Z", "orig");
        original(nullptr, &chunkB);
        CheckFloats(reinterpret_cast<const float*>(tangentA), reinterpret_cast<const float*>(tangentB),
                    vertexCount * 3, "CreateTangentVectors");

        // The flags == 0 early exit must leave the tangent array untouched.
        std::memset(tangentA, 0x5a, sizeof tangentA);
        std::memcpy(tangentB, tangentA, sizeof tangentA);
        chunkA.flags = 0;
        chunkB.flags = 0;
        g_tangent = {sizeof(Vec3), tangentA};
        local(nullptr, &chunkA);
        g_tangent = {sizeof(Vec3), tangentB};
        original(nullptr, &chunkB);
        Check(std::memcmp(tangentA, tangentB, sizeof tangentA) == 0, "CreateTangentVectors zero flags");
    }
}

void TestLightInitializers()
{
    Patch patches[3];
    PatchEnter(patches[0], static_cast<unsigned char*>(static_cast<void*>(orig)) + 0x362f0,
               reinterpret_cast<void*>(&RecLoad));
    PatchEnter(patches[1], static_cast<unsigned char*>(static_cast<void*>(orig)) + 0x30fe0,
               reinterpret_cast<void*>(&RecLoad));
    PatchEnter(patches[2], static_cast<unsigned char*>(static_cast<void*>(orig)) + 0x393d0,
               reinterpret_cast<void*>(&RecLoad));

    struct Variant {
        const char* name;
        const char* family;
    };
    static const Variant variants[] = {
        {"?InitializeAmbientLightRenders@MultiPassNoTextureMaterialPlugin@Jet@@IBA_NPAVRenderKernel@2@@Z", "notexture"},
        {"?InitializeAmbientLightRenders@MultiPassBumpMaterialPlugin@Jet@@MBA_NPAVRenderKernel@2@@Z", "bump"},
        {"?InitializeAmbientLightRenders@MultiPassOneTextureMaterialPlugin@Jet@@IBA_NPAVRenderKernel@2@@Z", "onetexture"},
        {"?InitializeParallelLightRenders@MultiPassNoTextureMaterialPlugin@Jet@@IBA_NPAVRenderKernel@2@@Z", "notexture"},
        {"?InitializeParallelLightRenders@MultiPassBumpSpecMaterialPlugin@Jet@@MBA_NPAVRenderKernel@2@@Z", "bumpspec"},
        {"?InitializeParallelLightRenders@MultiPassBumpMaterialPlugin@Jet@@MBA_NPAVRenderKernel@2@@Z", "bump"},
        {"?InitializeParallelLightRenders@MultiPassOneTextureMaterialPlugin@Jet@@IBA_NPAVRenderKernel@2@@Z", "onetexture"},
        {"?InitializePointLightRenders@MultiPassNoTextureMaterialPlugin@Jet@@IBA_NPAVRenderKernel@2@@Z", "notexture"},
        {"?InitializePointLightRenders@MultiPassBumpSpecMaterialPlugin@Jet@@MBA_NPAVRenderKernel@2@@Z", "bumpspec"},
        {"?InitializePointLightRenders@MultiPassBumpMaterialPlugin@Jet@@MBA_NPAVRenderKernel@2@@Z", "bump"},
        {"?InitializePointLightRenders@MultiPassOneTextureMaterialPlugin@Jet@@IBA_NPAVRenderKernel@2@@Z", "onetexture"},
        {"?InitializeSpotLightRenders@MultiPassNoTextureMaterialPlugin@Jet@@IBA_NPAVRenderKernel@2@@Z", "notexture"},
        {"?InitializeSpotLightRenders@MultiPassBumpMaterialPlugin@Jet@@MBA_NPAVRenderKernel@2@@Z", "bump"},
        {"?InitializeSpotLightRenders@MultiPassOneTextureMaterialPlugin@Jet@@IBA_NPAVRenderKernel@2@@Z", "onetexture"},
        {"?InitializeCubeMapLightRenders@MultiPassNoTextureMaterialPlugin@Jet@@IBA_NPAVRenderKernel@2@@Z", "notexture"},
        {"?InitializeCubeMapLightRenders@MultiPassBumpMaterialPlugin@Jet@@MBA_NPAVRenderKernel@2@@Z", "bump"},
        {"?InitializeCubeMapLightRenders@MultiPassOneTextureMaterialPlugin@Jet@@IBA_NPAVRenderKernel@2@@Z", "onetexture"},
        {"?InitializeCubeSpotLightRenders@MultiPassNoTextureMaterialPlugin@Jet@@IBA_NPAVRenderKernel@2@@Z", "notexture"},
        {"?InitializeCubeSpotLightRenders@MultiPassBumpMaterialPlugin@Jet@@MBA_NPAVRenderKernel@2@@Z", "bump"},
        {"?InitializeCubeSpotLightRenders@MultiPassOneTextureMaterialPlugin@Jet@@IBA_NPAVRenderKernel@2@@Z", "onetexture"},
    };

    void* vtable[0x48 / 4];
    BuildPluginVtable(vtable);
    const char* names[15] = {"vp0", "vp1", "vp2", "vp3", "vp4", "tc5", "tc6", "tc7", "tc8", "tc9",
                             "tc10", "tc11", "tc12", "tc13", "tc14"};

    for (const Variant& variant : variants) {
        FakePlugin pluginA{}, pluginB{};
        FakePlugin* plugins[2] = {&pluginA, &pluginB};
        for (int pi = 0; pi < 2; ++pi) {
            FakePlugin* plugin = plugins[pi];
            plugin->vtable = vtable;
            plugin->lightRender0 = NewMPLightRender();
            plugin->lightRender1 = NewMPLightRender();
            for (unsigned int i = 0; i < 15; ++i)
                *reinterpret_cast<const char**>(plugin->names + i * 4) = names[i];
        }

        InitFn local = Proc<InitFn>(ours, variant.name, "ours");
        InitFn original = Proc<InitFn>(orig, variant.name, "orig");

        std::memset(&g_rec, 0, sizeof g_rec);
        Check(local(&pluginA, nullptr), "light init result");
        Recorder recordA = g_rec;
        std::memset(&g_rec, 0, sizeof g_rec);
        Check(original(&pluginB, nullptr), "light init original result");
        Recorder recordB = g_rec;

        Check(recordA.loadCalls == recordB.loadCalls, "light init load calls");
        if (std::memcmp(recordA.vp, recordB.vp, sizeof recordA.vp) != 0) {
            for (int i = 0; i < recordA.loadCalls && i < 8; ++i)
                std::printf("  vp[%d] ours '%s' orig '%s'\n", i, recordA.vp[i], recordB.vp[i]);
        }
        if (std::memcmp(recordA.tc, recordB.tc, sizeof recordA.tc) != 0) {
            for (int i = 0; i < recordA.loadCalls && i < 8; ++i)
                std::printf("  tc[%d] ours '%s' orig '%s'\n", i, recordA.tc[i], recordB.tc[i]);
        }
        Check(std::memcmp(recordA.vp, recordB.vp, sizeof recordA.vp) == 0, "light init vp names");
        Check(std::memcmp(recordA.tc, recordB.tc, sizeof recordA.tc) == 0, "light init tc names");
        Check((pluginA.lightRender0 != nullptr) == (pluginB.lightRender0 != nullptr), "light init first render");
        Check((pluginA.lightRender1 != nullptr) == (pluginB.lightRender1 != nullptr), "light init second render");
    }

    for (int pi = 0; pi < 3; ++pi) PatchRestore(patches[pi]);
}

void TestLightRenders()
{
    void* vtable[0x48 / 4];
    BuildPluginVtable(vtable);
    FakePlugin pluginA{}, pluginB{};
    pluginA.vtable = vtable;
    pluginB.vtable = vtable;

    using Fn = bool(__cdecl*)(void*, void*);
    Fn local = Proc<Fn>(ours, "?InitializeLightRenders@MultiPassBumpMaterialPlugin@Jet@@MBA_NPAVRenderKernel@2@@Z", "ours");
    Fn original =
        Proc<Fn>(orig, "?InitializeLightRenders@MultiPassBumpMaterialPlugin@Jet@@MBA_NPAVRenderKernel@2@@Z", "orig");

    std::memset(&g_rec, 0, sizeof g_rec);
    Check(local(&pluginA, nullptr), "InitializeLightRenders");
    int callsA = g_rec.caches + g_rec.lightRenders;
    int typesA[6];
    std::memcpy(typesA, g_rec.types, sizeof typesA);
    std::memset(&g_rec, 0, sizeof g_rec);
    Check(original(&pluginB, nullptr), "InitializeLightRenders orig");
    int callsB = g_rec.caches + g_rec.lightRenders;
    Check(callsA == callsB, "InitializeLightRenders vtable calls");
    Check(std::memcmp(typesA, g_rec.types, sizeof typesA) == 0, "InitializeLightRenders type order");
    Check((pluginA.lightRender0 != nullptr) == (pluginB.lightRender0 != nullptr), "InitializeLightRenders first");
    Check((pluginA.lightRender1 != nullptr) == (pluginB.lightRender1 != nullptr), "InitializeLightRenders second");
    if (pluginA.lightRender0 && pluginB.lightRender0) {
        Check(*reinterpret_cast<unsigned int*>(pluginA.lightRender0) ==
                  *reinterpret_cast<unsigned int*>(pluginB.lightRender0),
              "InitializeLightRenders vtable");
    }
}

void TestEmptyLoadPrograms()
{
    static const char* names[] = {
        "?LoadLightPrograms@MultiPassBumpMaterialPlugin@Jet@@MBA_NPAVMultiPassLightRender@2@PAVRenderKernel@2@PBD2@Z",
        "?LoadLightPrograms@MultiPassNoTextureMaterialPlugin@Jet@@IBA_NPAVMultiPassLightRender@2@PAVRenderKernel@2@PBD2@Z",
        "?LoadLightPrograms@MultiPassOneTextureMaterialPlugin@Jet@@IBA_NPAVMultiPassLightRender@2@PAVRenderKernel@2@PBD2@Z",
    };
    using Fn = bool(__cdecl*)(void*, void*, void*, const char*, const char*);
    for (const char* name : names) {
        void* renderA = NewMPLightRender();
        void* renderB = NewMPLightRender();
        Fn local = Proc<Fn>(ours, name, "ours");
        Fn original = Proc<Fn>(orig, name, "orig");
        const bool ra = local(nullptr, renderA, nullptr, nullptr, nullptr);
        const bool rb = original(nullptr, renderB, nullptr, nullptr, nullptr);
        Check(ra == rb, "empty LoadLightPrograms");
        Check(*reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(renderA) + 0x08) ==
                  *reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(renderB) + 0x08),
              "empty LoadLightPrograms record");
    }
}

void TestScissorEarlyOut()
{
    unsigned char stateA[0x120], stateB[0x120];
    std::memset(stateA, 0, sizeof stateA);
    std::memset(stateB, 0, sizeof stateB);
    void* view = std::calloc(1, 0x80);
    *reinterpret_cast<int*>(static_cast<unsigned char*>(view) + 0x60) = 2;
    *reinterpret_cast<void**>(stateA + 0x08) = view;
    *reinterpret_cast<void**>(stateB + 0x08) = view;
    unsigned int indices = 0;
    using Fn = void(__cdecl*)(void*, const void*, const void*);
    Fn local = Proc<Fn>(ours, "?SetScissor@MultiPassRenderState@Jet@@QBAXPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@ABV?$Array@I$00@2@@Z", "ours");
    Fn original = Proc<Fn>(orig, "?SetScissor@MultiPassRenderState@Jet@@QBAXPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@ABV?$Array@I$00@2@@Z", "orig");
    local(stateA, nullptr, &indices);
    original(stateB, nullptr, &indices);
    CheckBytes(stateA, stateB, sizeof stateA, "SetScissor early exit");
    std::free(view);
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: %s <fixture dir> [rounds]\n", argv[0]);
        return 2;
    }
    AddVectoredExceptionHandler(1, &VectoredHandler);
    SetCurrentDirectoryA(argv[1]);

    HMODULE kernelStandard = LoadLibraryA("KernelStandard_7c_orig.dll");
    HMODULE kernel = LoadLibraryA("Kernel_7c_orig.dll");
    renderSystem = LoadLibraryA("RenderSystem_7c_orig.dll");
    orig = LoadLibraryA("Render3DCore_7c_orig.dll");
    ours = LoadLibraryA("Render3DCore_7c.dll");
    if (!orig || !ours || !renderSystem || !kernelStandard || !kernel) {
        std::printf("cannot load the fixture modules\n");
        return 2;
    }
    g_mlrCtor = Proc<MlrCtor>(orig, "??0MPLightRender@MultiPassRenderState@Jet@@QAE@XZ", "orig");

    TestTangent();
    TestLightInitializers();
    TestLightRenders();
    TestEmptyLoadPrograms();
    TestScissorEarlyOut();

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
