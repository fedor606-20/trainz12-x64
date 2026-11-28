// Differential test of Render3DCore_7c U5b wave 5 (U5bFinal): the local static data, the multipass material display
// and cache paths, the light-render records, and the render-state object image, ours against
// Render3DCore_7c_orig.dll in one process.
//
// usage: diff_r3d_u5b_final.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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
    if (std::memcmp(a, b, n) != 0) {
        const auto* x = static_cast<const unsigned char*>(a);
        const auto* y = static_cast<const unsigned char*>(b);
        for (unsigned i = 0; i < n; i += 4)
            if (std::memcmp(x + i, y + i, 4) != 0) {
                std::printf("%s at +0x%x: ours %08x orig %08x\n", what, i,
                            *reinterpret_cast<const unsigned*>(x + i),
                            *reinterpret_cast<const unsigned*>(y + i));
                break;
            }
    }
    Check(std::memcmp(a, b, n) == 0, what);
}

HMODULE ours, orig, renderSystem;
const char* kOrig = "Render3DCore_7c_orig.dll";

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
    const int delta = static_cast<int>(reinterpret_cast<std::uintptr_t>(hook) -
                                       (reinterpret_cast<std::uintptr_t>(patch.target) + 5));
    std::memcpy(patch.target + 1, &delta, 4);
    VirtualProtect(patch.target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), patch.target, 5);
    patch.active = true;
}
void PatchRestore(Patch& patch)
{
    if (!patch.active) return;
    DWORD old = 0;
    VirtualProtect(patch.target, 5, PAGE_EXECUTE_READWRITE, &old);
    std::memcpy(patch.target, patch.saved, 5);
    VirtualProtect(patch.target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), patch.target, 5);
    patch.active = false;
}
void* OrigData(const char* name) { return reinterpret_cast<void*>(GetProcAddress(orig, name)); }

// ---- display / render-state call log ---------------------------------------------------------------------------
enum Event : int {
    E_NONE, E_LIGHTING, E_ALPHA_FUNC, E_ALPHA_MODE, E_CULL, E_VT_AC, E_VT_160, E_VT_70, E_VT_54, E_SET_SCISSOR,
    E_GET_SCISSOR, E_VT_12C
};
int eventCount;
Event events[512];
int eventArg[512];

void Push(Event e, int a = 0)
{
    if (eventCount < 512) {
        events[eventCount] = e;
        eventArg[eventCount] = a;
        ++eventCount;
    }
}
void __cdecl RecLighting(void*, int on) { Push(E_LIGHTING, on); }
void __cdecl RecAlphaFunc(void*, int src, int dst) { Push(E_ALPHA_FUNC, src | (dst << 8)); }
void __cdecl RecAlphaMode(void*, int on) { Push(E_ALPHA_MODE, on); }
void __cdecl RecCull(void*, int mode) { Push(E_CULL, mode); }
void __cdecl RecVtAc(void*, int a) { Push(E_VT_AC, a); }
void __cdecl RecVt160(void*, int a) { Push(E_VT_160, a); }
void __cdecl RecVt70(void*, void* out) { Push(E_VT_70, out ? 1 : 0); }
void __cdecl RecVt54(void*, void* m) { Push(E_VT_54, m ? 1 : 0); }
void __cdecl RecVt12C(void*, int i) { Push(E_VT_12C, i); }
int materialAdds;
void __cdecl RecMaterialAdd(void*, const void* n, const void* v)
{
    ++materialAdds;
    (void)n;
    (void)v;
}
void __cdecl RecSetScissor(void*, const void* r)
{
    Push(E_SET_SCISSOR, r ? *reinterpret_cast<const int*>(r) : -1);
}
void* __cdecl RecGetScissor(void*, void* out)
{
    auto* r = static_cast<int*>(out);
    r[0] = 10;
    r[1] = 20;
    r[2] = 30;
    r[3] = 40;
    Push(E_GET_SCISSOR, 1);
    return out;
}

// ---- fakes ------------------------------------------------------------------------------------------------------
unsigned char fakeDisplay[0x240];
unsigned char displayVtable[0x200 * 4];
unsigned char displayState[0x100];
unsigned char fakeView[0x80];
unsigned char fakeCamera[0x280];
unsigned char fakeChunk[0x100];
unsigned char chunkVtable[0x100 * 4];
unsigned char fakeCache[2][0x100];
unsigned char cacheVtable[0x40 * 4];

void InitFakes()
{
    std::memset(fakeDisplay, 0, sizeof fakeDisplay);
    std::memset(fakeView, 0, sizeof fakeView);
    std::memset(fakeCamera, 0, sizeof fakeCamera);
    std::memset(fakeChunk, 0, sizeof fakeChunk);
    std::memset(displayState, 0, sizeof displayState);
    for (unsigned i = 0; i < 0x200; ++i) {
        void* fn = reinterpret_cast<void*>(&RecVtAc);
        switch (i * 4) {
        case 0xac: fn = reinterpret_cast<void*>(&RecVtAc); break;
        case 0x160: fn = reinterpret_cast<void*>(&RecVt160); break;
        case 0x70: fn = reinterpret_cast<void*>(&RecVt70); break;
        case 0x54: fn = reinterpret_cast<void*>(&RecVt54); break;
        case 0x12c: fn = reinterpret_cast<void*>(&RecVt12C); break;
        default: break;
        }
        *reinterpret_cast<void**>(displayVtable + i * 4) = fn;
    }
    *reinterpret_cast<void**>(fakeDisplay) = displayVtable;
    *reinterpret_cast<void**>(fakeDisplay + 0x54) = displayState;
    *reinterpret_cast<int*>(fakeDisplay + 0x218) = 0;
    *reinterpret_cast<void**>(fakeView + 0x20) = fakeCamera;
    *reinterpret_cast<void**>(fakeView + 0x60) = nullptr;
    *reinterpret_cast<unsigned char*>(displayState + 0xa1) = 0x03;
    *reinterpret_cast<int*>(displayState + 0xa4) = 0;
    *reinterpret_cast<int*>(displayState + 0xa8) = 0;
    *reinterpret_cast<int*>(displayState + 0xac) = 0;
    *reinterpret_cast<int*>(displayState + 0x68) = 4;
    *reinterpret_cast<int*>(displayState + 0x6c) = 5;
    for (unsigned i = 0; i < 0x40; ++i) *reinterpret_cast<void**>(cacheVtable + i * 4) = reinterpret_cast<void*>(&RecVtAc);
    for (unsigned i = 0; i < 2; ++i) {
        std::memset(fakeCache[i], 0, sizeof fakeCache[i]);
        *reinterpret_cast<void**>(fakeCache[i]) = cacheVtable;
    }
}

struct MaterialImage {
    unsigned char bytes[0x100];
};
void MakeMaterial(MaterialImage& m, bool oneTexture)
{
    std::memset(m.bytes, 0xcd, sizeof m.bytes);
    *reinterpret_cast<int*>(m.bytes + 0x14) = 0x11;
    *reinterpret_cast<int*>(m.bytes + 0x18) = 0x22;
    m.bytes[0x1c] = 0x05;
    *reinterpret_cast<int*>(m.bytes + 0x78) = 4;
    *reinterpret_cast<int*>(m.bytes + 0x7c) = 5;
    *reinterpret_cast<float*>(m.bytes + 0x80) = 0.75f;
    m.bytes[0x84] = 0;
    *reinterpret_cast<void**>(m.bytes + 0x9c) = fakeCache[0];
    *reinterpret_cast<void**>(m.bytes + 0xa0) = fakeCache[1];
    *reinterpret_cast<int*>(m.bytes + 0xac) = 0;
    *reinterpret_cast<int*>(m.bytes + 0xb0) = 0;
    *reinterpret_cast<int*>(m.bytes + 0xb4) = 0;
    *reinterpret_cast<int*>(m.bytes + 0xb8) = 0;
    *reinterpret_cast<void**>(m.bytes + 0xc4) = nullptr;
    *reinterpret_cast<void**>(m.bytes + 0xd0) = nullptr;
    if (oneTexture) {
        *reinterpret_cast<int*>(m.bytes + 0xc8) = 0;
        *reinterpret_cast<int*>(m.bytes + 0xcc) = 0;
    }
}

// ---- tests ------------------------------------------------------------------------------------------------------
void TestData()
{
    struct { const char* name; unsigned bytes; } items[] = {
        {"??_7MultiPassNoTextureMaterial@Jet@@6BMaterial@1@@", 24 * 4},
        {"??_7MultiPassOneTextureMaterial@Jet@@6BMaterial@1@@", 26 * 4},
        {"??_7MultiPassBumpMaterial@Jet@@6BMaterial@1@@", 26 * 4},
        {"??_7MultiPassBump2Material@Jet@@6BMaterial@1@@", 26 * 4},
        {"??_7MultiPassBumpSpecMaterial@Jet@@6BMaterial@1@@", 26 * 4},
        {"??_7MultiPassBumpSpec2Material@Jet@@6BMaterial@1@@", 26 * 4},
        {"??_7MultiPassNoTextureMaterial@Jet@@6BPluginInstance@1@@", 8},
        {"??_7MultiPassOneTextureMaterial@Jet@@6BPluginInstance@1@@", 8},
        {"??_7MultiPassBumpMaterial@Jet@@6BPluginInstance@1@@", 8},
        {"??_7MultiPassBump2Material@Jet@@6BPluginInstance@1@@", 8},
        {"??_7MultiPassBumpSpecMaterial@Jet@@6BPluginInstance@1@@", 8},
        {"??_7MultiPassBumpSpec2Material@Jet@@6BPluginInstance@1@@", 8},
        {"??_7MultiPassNoTextureMaterialPlugin@Jet@@6B@", 7 * 4},
        {"??_7MultiPassOneTextureMaterialPlugin@Jet@@6B@", 7 * 4},
        {"??_7MultiPassBumpMaterialPlugin@Jet@@6B@", 7 * 4},
        {"??_7MultiPassBump2MaterialPlugin@Jet@@6B@", 7 * 4},
        {"??_7MultiPassBumpSpecMaterialPlugin@Jet@@6B@", 7 * 4},
        {"??_7MultiPassBumpSpec2MaterialPlugin@Jet@@6B@", 7 * 4},
        {"??_8MultiPassNoTextureMaterial@Jet@@7B@", 8},
        {"??_8MultiPassOneTextureMaterial@Jet@@7B@", 8},
        {"??_8MultiPassBumpMaterial@Jet@@7B@", 8},
        {"??_7MultiPassBumpChunkBase@Jet@@6B@", 4},
        {"??_7MultiPassBumpChunkT@Jet@@6B?$GeometryChunkTemplate@UPackedVertexPNTA@Jet@@VGeometryChunkSingleStream@2@@1@@", 73 * 4},
        {"??_7MultiPassBumpChunkT@Jet@@6BMultiPassBumpChunkBase@1@@", 4},
        {"??_7MultiPassBumpChunkT2@Jet@@6B?$GeometryChunkTemplate@UPackedVertexPNTTA@Jet@@VGeometryChunkSingleStream@2@@1@@", 73 * 4},
        {"??_7MultiPassBumpChunkT2@Jet@@6BMultiPassBumpChunkBase@1@@", 4},
        {"??_7MultiPassLightRender@Jet@@6B@", 8},
        {"??_7MPLightRender@MultiPassRenderState@Jet@@6B@", 8},
        {"?UID@MultiPassNoTextureMaterial@Jet@@2VUId@2@B", 16},
        {"?UID@MultiPassOneTextureMaterial@Jet@@2VUId@2@B", 16},
        {"?UID@MultiPassBumpMaterial@Jet@@2VUId@2@B", 16},
        {"?UID@MultiPassBump2Material@Jet@@2VUId@2@B", 16},
        {"?UID@MultiPassBumpSpecMaterial@Jet@@2VUId@2@B", 16},
        {"?UID@MultiPassBumpSpec2Material@Jet@@2VUId@2@B", 16},
    };
    for (auto& i : items) {
        const void* const a = GetProcAddress(ours, i.name);
        const void* const b = GetProcAddress(orig, i.name);
        if (!a || !b) {
            Check(false, i.name);
            continue;
        }
        CheckBytes(a, b, i.bytes, i.name);
    }
    const char* types[] = {"MultiPassNoTextureMaterial", "MultiPassOneTextureMaterial", "MultiPassBumpMaterial",
                           "MultiPassBump2Material", "MultiPassBumpSpecMaterial", "MultiPassBumpSpec2Material"};
    char name[256];
    for (const char* t : types) {
        std::snprintf(name, sizeof name, "?DECORATED_TYPE@%s@Jet@@2VPString@2@B", t);
        const void* const a = GetProcAddress(ours, name);
        const void* const b = GetProcAddress(orig, name);
        Check(a && b, name);
        if (a && b) {
            const unsigned* na = *static_cast<const unsigned* const*>(a);
            const unsigned* nb = *static_cast<const unsigned* const*>(b);
            Check(na && nb, "type node");
            if (na && nb) {
                Check(na[4] == nb[4] && na[5] == nb[5], "type node refs/length");
                CheckBytes(reinterpret_cast<const char*>(na) + 0x18, reinterpret_cast<const char*>(nb) + 0x18,
                           na[5] + 1, "type text");
            }
        }
    }
}

using BindFn = void(__cdecl*)(void*, void*, void*, unsigned);
using StateFn = void(__cdecl*)(void*, void*);
using CacheFn = void(__cdecl*)(void*, void*, void*, void*, const void*, const void*);

void TestDisplayPaths()
{
    const char* names[] = {
        "?Bind@MultiPassNoTextureMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
        "?Unbind@MultiPassNoTextureMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
        "?Bind@MultiPassOneTextureMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
        "?Unbind@MultiPassOneTextureMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
    };
    for (const char* name : names) {
        BindFn fn[2] = {Proc<BindFn>(ours, name, "ours"), Proc<BindFn>(orig, name, "orig")};
        for (unsigned pass = 0; pass < 2; ++pass) {
            MaterialImage m[2];
            const bool one = std::strstr(name, "OneTexture") != nullptr;
            MakeMaterial(m[0], one);
            MakeMaterial(m[1], one);
            eventCount = 0;
            fn[0](m[0].bytes, fakeDisplay, fakeView, pass);
            const int n0 = eventCount;
            Event e0[512];
            int a0[512];
            std::memcpy(e0, events, sizeof e0);
            std::memcpy(a0, eventArg, sizeof a0);
            eventCount = 0;
            fn[1](m[1].bytes, fakeDisplay, fakeView, pass);
            Check(n0 == eventCount, name);
            Check(std::memcmp(e0, events, n0 * sizeof(Event)) == 0, "display event order");
            Check(std::memcmp(a0, eventArg, n0 * sizeof(int)) == 0, "display event args");
            if (n0 != eventCount || std::memcmp(e0, events, n0 * sizeof(Event)) != 0 ||
                std::memcmp(a0, eventArg, n0 * sizeof(int)) != 0) {
                std::printf("  %s pass %u: ours", name, pass);
                for (int i = 0; i < n0; ++i) std::printf(" (%d,%d)", int(e0[i]), a0[i]);
                std::printf(" orig");
                for (int i = 0; i < eventCount; ++i) std::printf(" (%d,%d)", int(events[i]), eventArg[i]);
                std::printf("\n");
            }
            CheckBytes(m[0].bytes, m[1].bytes, sizeof m[0].bytes, "material after bind");
        }
    }

    const char* states[] = {
        "?SetDisplayState@MultiPassNoTextureMaterial@Jet@@QBAXPAVDisplay@2@@Z",
        "?ClearDisplayState@MultiPassNoTextureMaterial@Jet@@QBAXPAVDisplay@2@@Z",
    };
    for (const char* name : states) {
        StateFn fn[2] = {Proc<StateFn>(ours, name, "ours"), Proc<StateFn>(orig, name, "orig")};
        for (unsigned flags = 0; flags < 8; ++flags) {
            MaterialImage m[2];
            MakeMaterial(m[0], false);
            MakeMaterial(m[1], false);
            m[0].bytes[0x84] = m[1].bytes[0x84] = static_cast<unsigned char>(flags);
            eventCount = 0;
            fn[0](m[0].bytes, fakeDisplay);
            const int n0 = eventCount;
            Event e0[512];
            int a0[512];
            std::memcpy(e0, events, sizeof e0);
            std::memcpy(a0, eventArg, sizeof a0);
            eventCount = 0;
            fn[1](m[1].bytes, fakeDisplay);
            Check(n0 == eventCount, name);
            Check(std::memcmp(e0, events, n0 * sizeof(Event)) == 0, "state event order");
            Check(std::memcmp(a0, eventArg, n0 * sizeof(int)) == 0, "state event args");
            if (std::memcmp(a0, eventArg, n0 * sizeof(int)) != 0) {
                std::printf("  %s flags %u: ours", name, flags);
                for (int i = 0; i < n0; ++i) std::printf(" (%d,%d)", int(e0[i]), a0[i]);
                std::printf(" orig");
                for (int i = 0; i < eventCount; ++i) std::printf(" (%d,%d)", int(events[i]), eventArg[i]);
                std::printf("\n");
            }
            CheckBytes(m[0].bytes, m[1].bytes, sizeof m[0].bytes, "state material");
        }
    }

    // CacheChunk alpha-blend branch (product below 1.0); the cache is a fake object whose slot 0x24 records.
    const char* caches[] = {
        "?CacheChunk@MultiPassNoTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z",
        "?CacheChunk@MultiPassOneTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z",
    };
    for (const char* name : caches) {
        CacheFn fn[2] = {Proc<CacheFn>(ours, name, "ours"), Proc<CacheFn>(orig, name, "orig")};
        MaterialImage m[2];
        const bool one = std::strstr(name, "OneTexture") != nullptr;
        MakeMaterial(m[0], one);
        MakeMaterial(m[1], one);
        *reinterpret_cast<void**>(m[0].bytes + 0xa0) = fakeCache[1];
        *reinterpret_cast<void**>(m[1].bytes + 0xa0) = fakeCache[1];
        *reinterpret_cast<void**>(m[0].bytes + 0x9c) = fakeCache[0];
        *reinterpret_cast<void**>(m[1].bytes + 0x9c) = fakeCache[0];
        unsigned char chunk[0x40] = {};
        *reinterpret_cast<float*>(chunk + 0x3c) = 0.5f;
        eventCount = 0;
        fn[0](m[0].bytes, fakeDisplay, fakeView, chunk, nullptr, nullptr);
        const int n0 = eventCount;
        Event e0[512];
        int a0[512];
        std::memcpy(e0, events, sizeof e0);
        std::memcpy(a0, eventArg, sizeof a0);
        eventCount = 0;
        fn[1](m[1].bytes, fakeDisplay, fakeView, chunk, nullptr, nullptr);
        Check(n0 == eventCount, name);
        Check(std::memcmp(e0, events, n0 * sizeof(Event)) == 0, "cache event order");
        Check(std::memcmp(a0, eventArg, n0 * sizeof(int)) == 0, "cache event args");
    }
}

void TestLightRender()
{
    using CtorFn = void*(__thiscall*)(void*, unsigned, unsigned, unsigned, unsigned, unsigned);
    using DtorFn = void(__thiscall*)(void*);
    using NewPassFn = void(__thiscall*)(void*, void*, void*);
    const char* ctorName = "??0MultiPassLightRender@Jet@@QAE@IIIII@Z";
    CtorFn ctor[2] = {Proc<CtorFn>(ours, ctorName, "ours"), Proc<CtorFn>(orig, ctorName, "orig")};
    DtorFn dtor[2] = {Proc<DtorFn>(ours, "??1MultiPassLightRender@Jet@@UAE@XZ", "ours"),
                      Proc<DtorFn>(orig, "??1MultiPassLightRender@Jet@@UAE@XZ", "orig")};
    NewPassFn pass[2] = {Proc<NewPassFn>(ours, "?CreateNewLightPass@MultiPassLightRender@Jet@@QAEXPAVVertexProgram@2@PAVTextureCombiner@2@@Z", "ours"),
                         Proc<NewPassFn>(orig, "?CreateNewLightPass@MultiPassLightRender@Jet@@QAEXPAVVertexProgram@2@PAVTextureCombiner@2@@Z", "orig")};
    // fake reference-counted program/combiner with the vbase table at +4
    int vbtable[2] = {0, 0};
    unsigned char program[0x40] = {};
    unsigned char combiner[0x40] = {};
    *reinterpret_cast<void**>(program + 4) = vbtable;
    *reinterpret_cast<int*>(program + 8) = 1000;
    *reinterpret_cast<void**>(combiner + 4) = vbtable;
    *reinterpret_cast<int*>(combiner + 8) = 1000;

    alignas(16) unsigned char a[0x100], b[0x100];
    std::memset(a, 0, sizeof a);
    std::memset(b, 0, sizeof b);
    ctor[0](a, 1, 2, 3, 4, 5);
    ctor[1](b, 1, 2, 3, 4, 5);
    CheckBytes(a, b, 0x38, "light render ctor");

    *reinterpret_cast<int*>(program + 8) = 1000;
    *reinterpret_cast<int*>(combiner + 8) = 1000;
    pass[0](a, program, combiner);
    const int refsP0 = *reinterpret_cast<int*>(program + 8);
    const int refsC0 = *reinterpret_cast<int*>(combiner + 8);
    *reinterpret_cast<int*>(program + 8) = 1000;
    *reinterpret_cast<int*>(combiner + 8) = 1000;
    pass[1](b, program, combiner);
    const int refsP1 = *reinterpret_cast<int*>(program + 8);
    const int refsC1 = *reinterpret_cast<int*>(combiner + 8);
    Check(std::memcmp(a, b, 0x0c) == 0, "light pass count");
    Check(*reinterpret_cast<unsigned*>(a + 8) == 1 && *reinterpret_cast<unsigned*>(b + 8) == 1, "one pass");
    const unsigned char* pa = *reinterpret_cast<unsigned char* const*>(a + 0x10);
    const unsigned char* pb = *reinterpret_cast<unsigned char* const*>(b + 0x10);
    Check(pa && pb, "pass data");
    if (pa && pb) {
        Check(*reinterpret_cast<void* const*>(pa + 0xfc) == *reinterpret_cast<void* const*>(pb + 0xfc),
              "pass program");
        Check(*reinterpret_cast<void* const*>(pa + 0x100) == *reinterpret_cast<void* const*>(pb + 0x100),
              "pass combiner");
        CheckBytes(static_cast<const unsigned char*>(pa) + 0x0c, static_cast<const unsigned char*>(pb) + 0x0c, 0xf0,
                   "pass arrays");
    }
    Check(refsP0 == 1001, "program ref after pass");
    Check(refsC0 == 1001, "combiner ref after pass");
    Check(refsP0 == refsP1 && refsC0 == refsC1, "pass refs agree");

    *reinterpret_cast<int*>(program + 8) = 1001;
    *reinterpret_cast<int*>(combiner + 8) = 1001;
    dtor[0](a);
    const int dP0 = *reinterpret_cast<int*>(program + 8);
    const int dC0 = *reinterpret_cast<int*>(combiner + 8);
    *reinterpret_cast<int*>(program + 8) = 1001;
    *reinterpret_cast<int*>(combiner + 8) = 1001;
    dtor[1](b);
    Check(dP0 == 1000 && dC0 == 1000, "program ref after dtor");
    Check(dP0 == *reinterpret_cast<int*>(program + 8) && dC0 == *reinterpret_cast<int*>(combiner + 8),
          "dtor refs agree");
    std::memcpy(a + 0x10, b + 0x10, 4);
    CheckBytes(a, b, 0x38, "light render dtor");
}

void TestState()
{
    using Ctor1 = void*(__thiscall*)(void*, const void*, void*, void*, void*, const void*, float, bool,
                                     const void*, const void*, bool, bool);
    using Dtor = void(__thiscall*)(void*);
    const char* n1 = "??0MultiPassRenderState@Jet@@QAE@PBVMaterial@1@PAVDisplay@1@PAVViewPort@1@PAVGeometryChunk@1@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@1@M_NPBVMatrix4@1@ABV?$Array@PBVTexture@Jet@@$09@1@55@Z";
    Ctor1 ctor[2] = {Proc<Ctor1>(ours, n1, "ours"), Proc<Ctor1>(orig, n1, "orig")};
    Dtor dtor[2] = {Proc<Dtor>(ours, "??1MultiPassRenderState@Jet@@QAE@XZ", "ours"),
                    Proc<Dtor>(orig, "??1MultiPassRenderState@Jet@@QAE@XZ", "orig")};

    MaterialImage material;
    MakeMaterial(material, true);
    *reinterpret_cast<float*>(material.bytes + 0x34) = 1.0f;
    *reinterpret_cast<float*>(material.bytes + 0x38) = 0.5f;
    *reinterpret_cast<float*>(material.bytes + 0x3c) = 0.25f;
    *reinterpret_cast<float*>(material.bytes + 0x64) = 0.1f;
    *reinterpret_cast<float*>(material.bytes + 0x68) = 0.2f;
    *reinterpret_cast<float*>(material.bytes + 0x6c) = 0.3f;
    *reinterpret_cast<float*>(material.bytes + 0x50) = 2.0f;
    unsigned char textures[0x0c] = {};

    alignas(16) unsigned char a[0x200], b[0x200];
    std::memset(a, 0, sizeof a);
    std::memset(b, 0, sizeof b);
    void* emptyLights = nullptr;
    float modelView[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    eventCount = 0;
    ctor[0](a, material.bytes, fakeDisplay, fakeView, fakeChunk, &emptyLights, 0.8f, true, modelView, textures, false, false);
    const int n0 = eventCount;
    Event e0[512];
    int a0[512];
    std::memcpy(e0, events, sizeof e0);
    std::memcpy(a0, eventArg, sizeof a0);
    eventCount = 0;
    ctor[1](b, material.bytes, fakeDisplay, fakeView, fakeChunk, &emptyLights, 0.8f, true, modelView, textures, false, false);
    Check(n0 == eventCount, "state ctor events");
    Check(std::memcmp(e0, events, n0 * sizeof(Event)) == 0, "state ctor event order");
    Check(std::memcmp(a0, eventArg, n0 * sizeof(int)) == 0, "state ctor event args");
    if (n0 != eventCount || std::memcmp(a0, eventArg, n0 * sizeof(int)) != 0) {
        std::printf("  state ctor: ours");
        for (int i = 0; i < n0; ++i) std::printf(" (%d,%d)", int(e0[i]), a0[i]);
        std::printf(" orig");
        for (int i = 0; i < eventCount; ++i) std::printf(" (%d,%d)", int(events[i]), eventArg[i]);
        std::printf("\n");
    }
    unsigned char na[0x200], nb[0x200];
    std::memcpy(na, a, sizeof na);
    std::memcpy(nb, b, sizeof nb);
    std::memset(na + 0x2c, 0, 6 * 4);
    std::memset(nb + 0x2c, 0, 6 * 4);
    CheckBytes(na, nb, 0x120, "state ctor image");

    using Ctor2 = void*(__thiscall*)(void*, const void*, void*, void*, void*, const void*, float, bool,
                                     const void*, const void*, bool, bool, unsigned int, const unsigned short*,
                                     unsigned int, int);
    const char* n2 = "??0MultiPassRenderState@Jet@@QAE@PBVMaterial@1@PAVDisplay@1@PAVViewPort@1@PAVGeometryChunk@1@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@1@M_NPBVMatrix4@1@ABV?$Array@PBVTexture@Jet@@$09@1@55IPBGIW4DPrimitive@1@@Z";
    Ctor2 ctor2[2] = {Proc<Ctor2>(ours, n2, "ours"), Proc<Ctor2>(orig, n2, "orig")};
    unsigned short indices[6] = {0, 1, 2, 2, 3, 0};
    alignas(16) unsigned char c[0x200], d[0x200];
    std::memset(c, 0, sizeof c);
    std::memset(d, 0, sizeof d);
    eventCount = 0;
    ctor2[0](c, material.bytes, fakeDisplay, fakeView, fakeChunk, &emptyLights, 0.5f, false, modelView, textures,
             true, true, 6, indices, 2, 4);
    const int cn = eventCount;
    eventCount = 0;
    ctor2[1](d, material.bytes, fakeDisplay, fakeView, fakeChunk, &emptyLights, 0.5f, false, modelView, textures,
             true, true, 6, indices, 2, 4);
    Check(cn == eventCount, "state ctor2 events");
    std::memcpy(na, c, sizeof na);
    std::memcpy(nb, d, sizeof nb);
    std::memset(na + 0x2c, 0, 6 * 4);
    std::memset(nb + 0x2c, 0, 6 * 4);
    CheckBytes(na, nb, 0x120, "state ctor2 image");

    using CopyCtor = void*(__thiscall*)(void*, const void*);
    CopyCtor copy[2] = {Proc<CopyCtor>(ours, "??0MultiPassRenderState@Jet@@QAE@ABV01@@Z", "ours"),
                        Proc<CopyCtor>(orig, "??0MultiPassRenderState@Jet@@QAE@ABV01@@Z", "orig")};
    alignas(16) unsigned char e[0x200], f[0x200];
    std::memset(e, 0, sizeof e);
    std::memset(f, 0, sizeof f);
    copy[0](e, c);
    copy[1](f, d);
    std::memcpy(na, e, sizeof na);
    std::memcpy(nb, f, sizeof nb);
    std::memset(na + 0x2c, 0, 6 * 4);
    std::memset(nb + 0x2c, 0, 6 * 4);
    CheckBytes(na, nb, 0x120, "state copy image");
    dtor[0](e);
    dtor[1](f);

    dtor[0](c);
    dtor[1](d);
    dtor[0](a);
    dtor[1](b);
    CheckBytes(a, b, 0x120, "state dtor image");
}

void TestChunks()
{
    using CreateFn = void*(__cdecl*)(void*, unsigned, unsigned, unsigned, unsigned, int, void*);
    using AllocFn = void*(__cdecl*)(void*, unsigned, unsigned, unsigned, unsigned, unsigned);
    const struct { const char* create; const char* alloc; bool bump; } entries[] = {
        {"?CreateGeometryChunk@MultiPassNoTextureMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAVGeometryChunkCreateHint@2@@Z",
         nullptr, false},
        {"?CreateGeometryChunk@MultiPassOneTextureMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAVGeometryChunkCreateHint@2@@Z",
         "?AllocateGeometryMemory@MultiPassOneTextureMaterial@Jet@@MBAPAVGeometryChunk@2@IIIII@Z", false},
        {"?CreateGeometryChunk@MultiPassBumpMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAVGeometryChunkCreateHint@2@@Z",
         "?AllocateGeometryMemory@MultiPassBumpMaterial@Jet@@MBAPAVGeometryChunk@2@IIIII@Z", true},
    };
    for (const auto& e : entries) {
        CreateFn fn[2] = {Proc<CreateFn>(ours, e.create, "ours"), Proc<CreateFn>(orig, e.create, "orig")};
        MaterialImage m[2];
        MakeMaterial(m[0], true);
        MakeMaterial(m[1], true);
        *reinterpret_cast<void**>(m[0].bytes + 0xcc) = nullptr;
        *reinterpret_cast<void**>(m[1].bytes + 0xcc) = nullptr;
        static void* materialVtable[0x100];
        for (unsigned i = 0; i < 0x100; ++i) materialVtable[i] = reinterpret_cast<void*>(&RecVtAc);
        materialVtable[0x60 / 4] = e.alloc ? OrigData(e.alloc) : nullptr;
        *reinterpret_cast<void**>(m[0].bytes) = materialVtable;
        *reinterpret_cast<void**>(m[1].bytes) = materialVtable;
        for (unsigned variant = 0; variant < 3; ++variant) {
            unsigned char hint[0x20];
            std::memset(hint, 0, sizeof hint);
            *reinterpret_cast<unsigned*>(hint + 0) = variant;      // version 0/1/2
            *reinterpret_cast<unsigned*>(hint + 4) = variant == 1 ? 1 : 2;
            *reinterpret_cast<unsigned*>(hint + 8) = 1;             // system bit 0
            *reinterpret_cast<unsigned*>(hint + 0x10) = variant == 2 ? 6 : 0;
            unsigned char hintA[0x20], hintB[0x20];
            std::memcpy(hintA, hint, sizeof hint);
            std::memcpy(hintB, hint, sizeof hint);
            void* const a = fn[0](m[0].bytes, 8, 4, 12, 2, 0, hintA);
            void* const b = fn[1](m[1].bytes, 8, 4, 12, 2, 0, hintB);
            Check(a && b, e.create);
            if (!a || !b) continue;
            const unsigned offsets[] = {0x18, 0x1c, 0x20, 0x24, 0x28, 0x3c, 0x40};
            for (unsigned offset : offsets) {
                const unsigned va = *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(a) + offset);
                const unsigned vb = *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(b) + offset);
                if (va != vb) std::printf("  chunk %s var %u +%x: ours %08x orig %08x\n", e.create, variant, offset, va, vb);
                Check(va == vb, "chunk field");
            }
            CheckBytes(hintA, hintB, sizeof hintA, "chunk hint");
            if (e.bump) {
                Check(*reinterpret_cast<void**>(a) == *reinterpret_cast<void**>(b), "chunk vtable");
                Check(*reinterpret_cast<void**>(static_cast<unsigned char*>(a) + 0x60) ==
                          *reinterpret_cast<void**>(static_cast<unsigned char*>(b) + 0x60),
                      "chunk base vtable");
            }
        }
        if (e.alloc) {
            AllocFn alloc[2] = {Proc<AllocFn>(ours, e.alloc, "ours"), Proc<AllocFn>(orig, e.alloc, "orig")};
            void* const a = alloc[0](m[0].bytes, 8, 4, 12, 2, 0x125);
            void* const b = alloc[1](m[1].bytes, 8, 4, 12, 2, 0x125);
            Check(a && b, e.alloc);
            if (a && b) {
                const unsigned offsets[] = {0x18, 0x1c, 0x20, 0x24, 0x28, 0x3c, 0x40};
                for (unsigned offset : offsets) {
                    Check(*reinterpret_cast<unsigned*>(static_cast<unsigned char*>(a) + offset) ==
                              *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(b) + offset),
                          "alloc field");
                }
                if (e.bump) Check(*reinterpret_cast<void**>(a) == *reinterpret_cast<void**>(b), "alloc vtable");
            }
        }
    }
}

void TestNullRender()
{
    using RenderFn = void(__cdecl*)(void*, void*, void*, void*, const void*, const void*, int);
    const char* names[] = {
        "?RenderChunk@MultiPassNoTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@H@Z",
        "?RenderChunk@MultiPassOneTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@H@Z",
    };
    for (const char* name : names) {
        RenderFn fn[2] = {Proc<RenderFn>(ours, name, "ours"), Proc<RenderFn>(orig, name, "orig")};
        MaterialImage m[2];
        const bool one = std::strstr(name, "OneTexture") != nullptr;
        MakeMaterial(m[0], one);
        MakeMaterial(m[1], one);
        eventCount = 0;
        fn[0](m[0].bytes, fakeDisplay, nullptr, fakeChunk, nullptr, nullptr, 0);
        const int n0 = eventCount;
        fn[1](m[1].bytes, fakeDisplay, nullptr, fakeChunk, nullptr, nullptr, 0);
        Check(n0 == 0 && eventCount == 0, "null view render");
        CheckBytes(m[0].bytes, m[1].bytes, sizeof m[0].bytes, "null view material");
    }
}

void TestAddProperty()
{
    const char* name = "?AddProperty@MultiPassNoTextureMaterial@Jet@@UAAXABVPString@2@0@Z";
    using AddFn = void(__cdecl*)(void*, const void*, const void*);
    AddFn fn[2] = {Proc<AddFn>(ours, name, "ours"), Proc<AddFn>(orig, name, "orig")};

    // The original's interned property nodes live next to its NoTexture type export.
    const unsigned char* const anchor =
        reinterpret_cast<const unsigned char*>(GetProcAddress(orig, "?UNDECORATED_TYPE@MultiPassNoTextureMaterial@Jet@@2VPString@2@B"));
    if (!anchor) {
        Check(false, "property anchors");
        return;
    }
    const unsigned char* const base = anchor - 0xdcc58;
    struct PS { void* node; };
    PS falseValue{*reinterpret_cast<void* const*>(base + 0xdcc6c)};
    PS offValue{*reinterpret_cast<void* const*>(base + 0xdcc70)};
    PS smoothName{*reinterpret_cast<void* const*>(base + 0xdcc74)};
    PS singleName{*reinterpret_cast<void* const*>(base + 0xdcc78)};
    PS intensityName{*reinterpret_cast<void* const*>(base + 0xdcc7c)};
    PS debugName{*reinterpret_cast<void* const*>(base + 0xdcc80)};
    PS otherName{*reinterpret_cast<void* const*>(anchor)};

    const PS* names[] = {&smoothName, &singleName, &intensityName, &debugName, &otherName};
    const PS* values[] = {&falseValue, &offValue, &otherName};
    for (const PS* prop : names) {
        for (const PS* value : values) {
            MaterialImage m[2];
            std::memset(m[0].bytes, 0, 0x100);
            std::memset(m[1].bytes, 0, 0x100);
            fn[0](m[0].bytes, prop, value);
            fn[1](m[1].bytes, prop, value);
            CheckBytes(m[0].bytes + 0xac, m[1].bytes + 0xac, 4, "AddProperty mode bytes");
        }
    }
}}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u5b_final <bin dir> [rounds]\n");
        return 2;
    }
    char fullDir[MAX_PATH] = {};
    GetFullPathNameA(argv[1], MAX_PATH, fullDir, nullptr);
    SetDllDirectoryA(fullDir);
    const std::string dir = fullDir;
    for (const char* name : {"KernelStandard_7c_orig.dll", "Kernel_7c_orig.dll", "KernelResource_7c_orig.dll",
                             "KernelResources_7c_orig.dll", "RenderSystem_7c_orig.dll", "Render2DCore_7c_orig.dll"}) {
        if (!LoadLibraryA((dir + "\\" + name).c_str())) std::printf("preload %s failed %lu\n", name, GetLastError());
    }
    if (!LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str())) std::printf("orig load failed %lu\n", GetLastError());
    ours = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    if (!ours) std::printf("ours load failed %lu\n", GetLastError());
    orig = GetModuleHandleA(kOrig);
    renderSystem = GetModuleHandleA("RenderSystem_7c.dll");
    if (!renderSystem) renderSystem = LoadLibraryA("RenderSystem_7c.dll");
    if (!ours || !orig || !renderSystem) {
        std::printf("cannot load the modules (%lu)\n", GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    AddVectoredExceptionHandler(1, VectoredHandler);
    InitFakes();

    Patch pLighting, pAlphaFunc, pAlphaMode, pCull, pGetScissor, pSetScissor, pMaterialAdd;
    struct Target { const char* name; void* hook; Patch* patch; } targets[] = {
        {"?SetLightingMode@Display@Jet@@QAAX_N@Z", reinterpret_cast<void*>(&RecLighting), &pLighting},
        {"?SetAlphaBlendFunction@Display@Jet@@QAAXW4Factor@Src@2@W43Dst@2@@Z",
         reinterpret_cast<void*>(&RecAlphaFunc), &pAlphaFunc},
        {"?SetAlphaBlendMode@Display@Jet@@QAAX_N@Z", reinterpret_cast<void*>(&RecAlphaMode), &pAlphaMode},
        {"?SetCullMode@Display@Jet@@QAAXW4DCull@2@@Z", reinterpret_cast<void*>(&RecCull), &pCull},
        {"?GetScissor@Display@Jet@@QAA?AURect@2@XZ", reinterpret_cast<void*>(&RecGetScissor), &pGetScissor},
        {"?SetScissor@Display@Jet@@QAAXABURect@2@@Z", reinterpret_cast<void*>(&RecSetScissor), &pSetScissor},
        {"?AddProperty@Material@Jet@@UAAXABVPString@2@0@Z", reinterpret_cast<void*>(&RecMaterialAdd), &pMaterialAdd},
    };
    for (auto& t : targets) {
        void* const address = reinterpret_cast<void*>(GetProcAddress(renderSystem, t.name));
        if (address) PatchEnter(*t.patch, address, t.hook);
        else std::printf("missing rss export %s\n", t.name);
    }

    const int rounds = argc > 2 ? std::atoi(argv[2]) : 1;
    TestData();
    TestAddProperty();
    TestChunks();
    for (int round = 0; round < rounds; ++round) {
        TestDisplayPaths();
        TestNullRender();
        TestLightRender();
        TestState();
    }

    for (auto& t : targets) PatchRestore(*t.patch);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
