// Differential test of Render3DCore_7c unit U5a wave 5 (U5aFinal): the material/plugin data exports, destructors,
// plugin resource factories and Reflective's three render paths. Ours against Render3DCore_7c_orig.dll in one
// process.
//
// usage: diff_r3d_u5a_final.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

#include <jet/Geometry.h>
#include <jet/Math.h>
#include <jet/PString.h>

using Jet::Matrix4;

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
std::mt19937 rng(0x5eed5a5);
int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }
float Bits(std::uint32_t u)
{
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
float Awkward()
{
    switch (Pick(0, 11)) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: return Bits(0x7f800000u);
    case 3: return Bits(0xff800000u);
    case 4: return Bits(0x7fc00000u);
    case 5: return Bits(0x7fa00000u);
    default: return std::uniform_real_distribution<float>(-10.0f, 10.0f)(rng);
    }
}
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
                            *reinterpret_cast<const unsigned*>(x + i), *reinterpret_cast<const unsigned*>(y + i));
                break;
            }
    }
    Check(std::memcmp(a, b, n) == 0, what);
}

template <class T>
void* AsPtr(const T* p) { return const_cast<void*>(static_cast<const void*>(p)); }

HMODULE ours, orig;

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
void* Optional(HMODULE module, const char* name) { return reinterpret_cast<void*>(GetProcAddress(module, name)); }

// ---------------------------------------------------------------------------------------------------------------
// IAT patching

void* PatchImport(HMODULE module, const char* name, void* replacement)
{
    auto* const base = reinterpret_cast<unsigned char*>(module);
    auto* const dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* const nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
    for (; desc->Name; ++desc) {
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);
        auto* nameThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->OriginalFirstThunk);
        for (; thunk->u1.Function; ++thunk, ++nameThunk) {
            if (nameThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            auto* const imported = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + nameThunk->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(imported->Name), name) != 0) continue;
            DWORD old = 0;
            VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            void* const previous = reinterpret_cast<void*>(thunk->u1.Function);
            thunk->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);
            VirtualProtect(&thunk->u1.Function, sizeof(void*), old, &old);
            return previous;
        }
    }
    return nullptr;
}

bool InText(HMODULE module, const void* p)
{
    auto* const base = reinterpret_cast<unsigned char*>(module);
    auto* const dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* const nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto* const sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const auto& s = sec[i];
        if (!(s.Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        auto* const lo = base + s.VirtualAddress;
        if (p >= lo && p < lo + s.Misc.VirtualSize) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------
// fake Display (all virtuals are no-ops unless recorded below)

unsigned char displayBytes[0x280];
unsigned char displayState[0x400];
void* displayVtable[0x280 / 4];
struct DisplayCall {
    int slot;            // byte offset
    unsigned arg0, arg1, arg2;
    float f0;
    const void* p0;
};
DisplayCall displayCalls[64];
int displayCallCount;
int displayAnswer[0x40];
const void* stageResult = reinterpret_cast<const void*>(0x5a5a0000);

int __cdecl SlotNoop() { return 0; }
int __cdecl SlotFormat(const void* display, int id) { (void)display; return displayAnswer[id & 0x3f]; }
int __cdecl SlotStage(const void* display, int flag, const void* first, const void* second)
{
    DisplayCall& c = displayCalls[displayCallCount++];
    c.slot = 0x11c;
    c.arg0 = flag;
    c.p0 = first;
    c.arg1 = static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(second));
    (void)display;
    return reinterpret_cast<int>(stageResult);
}
int __cdecl SlotMatrix(const void* display, const void* matrix)
{
    DisplayCall& c = displayCalls[displayCallCount++];
    c.slot = 0x64;
    c.p0 = matrix;
    (void)display;
    return 0;
}
int __cdecl SlotMatrix1(const void* display, int unit, const void* matrix)
{
    DisplayCall& c = displayCalls[displayCallCount++];
    c.slot = 0x74;
    c.arg0 = unit;
    c.p0 = matrix;
    (void)display;
    return 0;
}
int __cdecl SlotModelView(const void* display, const void* matrix)
{
    DisplayCall& c = displayCalls[displayCallCount++];
    c.slot = 0x54;
    c.p0 = matrix;
    (void)display;
    return 0;
}
int __cdecl SlotFlag(const void* display, int flag)
{
    DisplayCall& c = displayCalls[displayCallCount++];
    c.slot = 0x78;
    c.arg0 = flag;
    (void)display;
    return 0;
}
int __cdecl SlotStageFree(const void* display, const void* stage)
{
    DisplayCall& c = displayCalls[displayCallCount++];
    c.slot = 0x124;
    c.p0 = stage;
    (void)display;
    return 0;
}
void InitDisplay()
{
    for (unsigned i = 0; i < sizeof displayVtable / sizeof displayVtable[0]; ++i) displayVtable[i] = reinterpret_cast<void*>(SlotNoop);
    displayVtable[0x1c / 4] = reinterpret_cast<void*>(SlotFormat);
    displayVtable[0x11c / 4] = reinterpret_cast<void*>(SlotStage);
    displayVtable[0x64 / 4] = reinterpret_cast<void*>(SlotMatrix);
    displayVtable[0x74 / 4] = reinterpret_cast<void*>(SlotMatrix1);
    displayVtable[0x54 / 4] = reinterpret_cast<void*>(SlotModelView);
    displayVtable[0x78 / 4] = reinterpret_cast<void*>(SlotFlag);
    displayVtable[0x124 / 4] = reinterpret_cast<void*>(SlotStageFree);
}
void ResetDisplay()
{
    std::memset(displayBytes, 0, sizeof displayBytes);
    std::memset(displayState, 0, sizeof displayState);
    *reinterpret_cast<void**>(displayBytes) = AsPtr(displayVtable);
    *reinterpret_cast<void**>(displayBytes + 0x54) = displayState;
    displayCallCount = 0;
}
void* Display() { return displayBytes; }

// ---------------------------------------------------------------------------------------------------------------
// fake viewport, camera and chunk

unsigned char cameraBytes[0x300];
unsigned char viewBytes[0x40];
unsigned char chunkBytes[0x80];
void* chunkVtable[0xf4 / 4];
void* viewVtable[0x80 / 4];
int renderCalls, primitiveCalls;
const void* lastProgram;
const void* lastLights;
const void* lastModelView;
unsigned lastNumIndices, lastNumPrimitives, lastPrimitive;
bool lastShared, lastFlag;

int __cdecl ChunkProgram(const void* chunk, int stream)
{
    (void)chunk;
    (void)stream;
    return reinterpret_cast<int>(lastProgram);
}
int __cdecl ChunkRender(const void* chunk, const void* display, const void* view, const void* program, int shared,
                        int s)
{
    (void)chunk;
    (void)display;
    (void)view;
    (void)program;
    (void)s;
    ++renderCalls;
    lastShared = shared != 0;
    lastFlag = s != 0;
    return 0;
}
int __cdecl ChunkPrimitives(const void* chunk, const void* display, const void* view, const void* program,
                            unsigned numIndices, const void* indices, unsigned numPrimitives, int primitive,
                            int shared, int s)
{
    (void)chunk;
    (void)display;
    (void)view;
    (void)indices;
    (void)s;
    (void)program;
    ++primitiveCalls;
    lastNumIndices = numIndices;
    lastNumPrimitives = numPrimitives;
    lastPrimitive = primitive;
    lastShared = shared != 0;
    return 0;
}
void InitChunk()
{
    for (unsigned i = 0; i < sizeof chunkVtable / sizeof chunkVtable[0]; ++i) chunkVtable[i] = reinterpret_cast<void*>(SlotNoop);
    chunkVtable[0xf0 / 4] = reinterpret_cast<void*>(ChunkProgram);
    chunkVtable[0x38 / 4] = reinterpret_cast<void*>(ChunkRender);
    chunkVtable[0x3c / 4] = reinterpret_cast<void*>(ChunkPrimitives);
    *reinterpret_cast<void**>(chunkBytes) = AsPtr(chunkVtable);
    *reinterpret_cast<unsigned*>(viewBytes) = 0;
    *reinterpret_cast<void**>(viewBytes + 0x20) = AsPtr(cameraBytes);
    *reinterpret_cast<unsigned*>(cameraBytes + 0x25c) = 0;
    for (int i = 0; i < 16; ++i) *reinterpret_cast<float*>(cameraBytes + 0x1b8 + i * 4) = 0.25f * i;
}
void ResetChunk(float opacity, const void* program)
{
    std::memset(chunkBytes, 0, sizeof chunkBytes);
    *reinterpret_cast<void**>(chunkBytes) = AsPtr(chunkVtable);
    *reinterpret_cast<float*>(chunkBytes + 0x3c) = opacity;
    lastProgram = program;
    renderCalls = primitiveCalls = 0;
    lastLights = lastModelView = nullptr;
    lastNumIndices = lastNumPrimitives = lastPrimitive = 0;
    lastShared = lastFlag = false;
}

// ---------------------------------------------------------------------------------------------------------------
// Material::SetLightingState recorder (patched in both IATs)

struct LightingCall {
    const void* self;
    const void* display;
    const void* view;       // second parameter (camera view)
    const void* chunk;
    const void* program;
    const void* lights;
    const void* modelView;  // sixth parameter
    unsigned opacityBits;
    int flag;
    int count;
};
LightingCall lighting;

void __cdecl RecordLighting(const void* self, const void* display, const void* view, const void* chunk,
                            const void* program, const void* lights, const void* modelView, float opacity, int flag)
{
    ++lighting.count;
    lighting.self = self;
    lighting.display = display;
    lighting.view = view;
    lighting.chunk = chunk;
    lighting.program = program;
    lighting.lights = lights;
    lighting.modelView = modelView;
    std::memcpy(&lighting.opacityBits, &opacity, 4);
    lighting.flag = flag;
}
void ResetLighting() { std::memset(&lighting, 0, sizeof lighting); }

// ---------------------------------------------------------------------------------------------------------------
// data tables

struct Kind {
    const char* cls;
    int size;
    bool oneTexture;  // 26-slot table (CacheAlphaHint/Set/ClearDisplayState)
    bool generic;     // 23-slot table with its own IsLoaded
};
const Kind kinds[10] = {
    {"NoTextureMaterial", 0xac, false, false},
    {"OneTextureMaterial", 0xb4, true, false},
    {"OneTextureNoAlphaMaterial", 0xb4, true, false},
    {"BillboardMaterial", 0xb4, true, false},
    {"ColorOneTextureMaterial", 0xb4, true, false},
    {"AutoTexMaterial", 0xc4, true, false},
    {"MVHSelectMaterial", 0xc8, true, false},
    {"ReflectiveMaterial", 0x108, true, false},
    {"GlossMaterial", 0x108, true, false},
    {"GenericShaderMaterial", 0xc0, false, true},
};
char nameBuffer[128];
const char* Member(const Kind& k, const char* member, const char* signature)
{
    std::snprintf(nameBuffer, sizeof nameBuffer, "?%s@%s@Jet@@%s", member, k.cls, signature);
    return nameBuffer;
}

void TestData()
{
    static const struct { const char* cls; const char* text; const char* uid; } info[10] = {
        {"NoTextureMaterial", "notex", "F9DE4A98-FDF1-459b-BBD4-F4BD2FD034EB"},
        {"OneTextureMaterial", "onetex", "45DBFE90-6DE6-46dd-B37F-F3F0A471E068"},
        {"OneTextureNoAlphaMaterial", "onetexnoalpha", "45DBFE90-6DE6-46dd-B37F-F3F0A471E178"},
        {"BillboardMaterial", "billboard", "822E6580-52B3-11d5-9AEF-0001025364B8"},
        {"ColorOneTextureMaterial", "coloronetex", "5831FE90-AFE6-7DEE-B37F-1561A476E049"},
        {"AutoTexMaterial", "autotex", "F6178437-1CFB-4212-82C2-912C37D23D20"},
        {"MVHSelectMaterial", "mvhselect", "A5DB7890-34E6-46EE-AA7F-13F9A471E065"},
        {"ReflectiveMaterial", "reflect", "F1AA4E1B-761A-40dd-89E4-B2B13F9B679B"},
        {"GlossMaterial", "gloss", "2F5C2951-4808-493b-ACF5-2E9D26963B05"},
        {"GenericShaderMaterial", "shader", "54FB9F73-172E-4d01-852E-20D3D96D93C7"},
    };
    for (int i = 0; i < 10; ++i) {
        // type string (the exported PString object, through its interned node)
        const char* const getType = Member(kinds[i], "GetType", "UBAABVPString@2@XZ");
        auto fn0 = Proc<const void*(__cdecl*)(const void*)>(ours, getType, "ours");
        auto fn1 = Proc<const void*(__cdecl*)(const void*)>(orig, getType, "orig");
        alignas(8) unsigned char object[0x180];
        std::memset(object, 0, sizeof object);
        const void* const p0 = fn0(object);
        const void* const p1 = fn1(object);
        const char* const t0 = p0 && *static_cast<void* const*>(p0) ? static_cast<const char*>(*static_cast<void* const*>(p0)) + 0x18 : nullptr;
        const char* const t1 = p1 && *static_cast<void* const*>(p1) ? static_cast<const char*>(*static_cast<void* const*>(p1)) + 0x18 : nullptr;
        Check(t0 && t1 && std::strcmp(t0, info[i].text) == 0 && std::strcmp(t0, t1) == 0, "GetType text");

        // UID: 16 bytes
        std::snprintf(nameBuffer, sizeof nameBuffer, "?UID@%s@Jet@@2VUId@2@B", info[i].cls);
        const void* const u0 = Optional(ours, nameBuffer);
        const void* const u1 = Optional(orig, nameBuffer);
        Check(u0 && u1 && std::memcmp(u0, u1, 16) == 0, "UID bytes");

        // vbtable: {0, vbase delta}
        std::snprintf(nameBuffer, sizeof nameBuffer, "??_8%s@Jet@@7B@", info[i].cls);
        const int* const vb0 = static_cast<const int*>(Optional(ours, nameBuffer));
        const int* const vb1 = static_cast<const int*>(Optional(orig, nameBuffer));
        Check(vb0 && vb1 && vb0[0] == vb1[0] && vb0[1] == vb1[1], "vbtable");

        // vftable: equal length, every slot inside the module's text (never null/purecall)
        std::snprintf(nameBuffer, sizeof nameBuffer, "??_7%s@Jet@@6BMaterial@1@@", info[i].cls);
        auto* const vt0 = static_cast<void* const*>(Optional(ours, nameBuffer));
        auto* const vt1 = static_cast<void* const*>(Optional(orig, nameBuffer));
        int n0 = 0, n1 = 0;
        while (vt0 && n0 < 40 && InText(ours, vt0[n0])) ++n0;
        while (vt1 && n1 < 40 && InText(orig, vt1[n1])) ++n1;
        Check(vt0 && vt1 && n0 == n1 && n0 == (kinds[i].oneTexture ? 26 : 23), "vftable length");
        // the RTTI pointer in front of the table is a real pointer inside the module's read-only data
        Check(vt0 && vt0[-1] != nullptr && vt1 && vt1[-1] != nullptr, "vftable RTTI");

        // the PluginInstance vtable is two slots (DeleteThis, dtor)
        std::snprintf(nameBuffer, sizeof nameBuffer, "??_7%s@Jet@@6BPluginInstance@1@@", info[i].cls);
        auto* const iv0 = static_cast<void* const*>(Optional(ours, nameBuffer));
        auto* const iv1 = static_cast<void* const*>(Optional(orig, nameBuffer));
        int m0 = 0, m1 = 0;
        while (iv0 && m0 < 8 && InText(ours, iv0[m0])) ++m0;
        while (iv1 && m1 < 8 && InText(orig, iv1[m1])) ++m1;
        Check(iv0 && iv1 && m0 == 2 && m1 == 2, "PluginInstance vftable length");

        // DECORATED_TYPE text
        std::snprintf(nameBuffer, sizeof nameBuffer, "?DECORATED_TYPE@%s@Jet@@2VPString@2@B", info[i].cls);
        const void* const d0 = Optional(ours, nameBuffer);
        const void* const d1 = Optional(orig, nameBuffer);
        const char* const dt0 = d0 && *static_cast<void* const*>(d0) ? static_cast<const char*>(*static_cast<void* const*>(d0)) + 0x18 : nullptr;
        const char* const dt1 = d1 && *static_cast<void* const*>(d1) ? static_cast<const char*>(*static_cast<void* const*>(d1)) + 0x18 : nullptr;
        Check(dt0 && dt1 && std::strcmp(dt0, dt1) == 0, "DECORATED_TYPE text");
    }
}

unsigned char fakeTexture[0x40];
void* textureVtable[0x80 / 4];
int __cdecl TexAvailable(const void*) { return 1; }
int __cdecl TexDimension(const void*) { return 0; }

void InitFakeTexture()
{
    for (unsigned i = 0; i < sizeof textureVtable / sizeof textureVtable[0]; ++i)
        textureVtable[i] = reinterpret_cast<void*>(SlotNoop);
    textureVtable[0x0c / 4] = reinterpret_cast<void*>(TexAvailable);
    textureVtable[0x2c / 4] = reinterpret_cast<void*>(TexDimension);
    *reinterpret_cast<void**>(fakeTexture) = textureVtable;
    *reinterpret_cast<int*>(fakeTexture + 4) = 1;
}

// A vtable-dispatch check on a zeroed image: install each module's own table and call a slot.
void TestVtableDispatch()
{
    for (int k = 0; k < 10; ++k) {
        std::snprintf(nameBuffer, sizeof nameBuffer, "??_7%s@Jet@@6BMaterial@1@@", kinds[k].cls);
        void* const* const vt0 = static_cast<void* const*>(Optional(ours, nameBuffer));
        void* const* const vt1 = static_cast<void* const*>(Optional(orig, nameBuffer));
        if (!vt0 || !vt1) continue;
        alignas(8) unsigned char a[0x180], b[0x180];

        // slot 2 SetResourcePriority(float): writes the field at +0x94 and the texture reference priority
        {
            std::memset(a, 0, sizeof a);
            std::memset(b, 0, sizeof b);
            *reinterpret_cast<void**>(a) = AsPtr(vt0);
            *reinterpret_cast<void**>(b) = AsPtr(vt1);
            const float priority = Awkward();
            reinterpret_cast<void(__cdecl*)(void*, float)>(vt0[2])(a, priority);
            reinterpret_cast<void(__cdecl*)(void*, float)>(vt1[2])(b, priority);
            CheckBytes(a + 4, b + 4, 0xa0 - 4, "vtable SetResourcePriority");
        }
        // slot 1 operator==
        {
            std::memset(a, 0, sizeof a);
            std::memset(b, 0, sizeof b);
            *reinterpret_cast<void**>(a) = AsPtr(vt0);
            *reinterpret_cast<void**>(b) = AsPtr(vt1);
            const bool e0 = reinterpret_cast<bool(__cdecl*)(const void*, const void*)>(vt0[1])(a, a);
            const bool e1 = reinterpret_cast<bool(__cdecl*)(const void*, const void*)>(vt1[1])(b, b);
            Check(e0 == e1, "vtable operator==");
        }
        // slot 3 GetType
        {
            std::memset(a, 0, sizeof a);
            std::memset(b, 0, sizeof b);
            *reinterpret_cast<void**>(a) = AsPtr(vt0);
            *reinterpret_cast<void**>(b) = AsPtr(vt1);
            const void* const p0 = reinterpret_cast<const void*(__cdecl*)(const void*)>(vt0[3])(a);
            const void* const p1 = reinterpret_cast<const void*(__cdecl*)(const void*)>(vt1[3])(b);
            const char* t0 = p0 && *static_cast<void* const*>(p0) ? static_cast<const char*>(*static_cast<void* const*>(p0)) + 0x18 : nullptr;
            const char* t1 = p1 && *static_cast<void* const*>(p1) ? static_cast<const char*>(*static_cast<void* const*>(p1)) + 0x18 : nullptr;
            Check(t0 && t1 && std::strcmp(t0, t1) == 0, "vtable GetType");
        }
        // slot 11 GetNumPasses
        {
            std::memset(a, 0, sizeof a);
            std::memset(b, 0, sizeof b);
            *reinterpret_cast<void**>(a) = AsPtr(vt0);
            *reinterpret_cast<void**>(b) = AsPtr(vt1);
            // Reflective's GetNumPasses returns 2; the base returns 1
            const unsigned n0 = reinterpret_cast<unsigned(__cdecl*)(const void*)>(vt0[11])(a);
            const unsigned n1 = reinterpret_cast<unsigned(__cdecl*)(const void*)>(vt1[11])(b);
            Check(n0 == n1, "vtable GetNumPasses");
        }
        // slot 22 IsLoaded
        {
            std::memset(a, 0, sizeof a);
            std::memset(b, 0, sizeof b);
            *reinterpret_cast<void**>(a) = AsPtr(vt0);
            *reinterpret_cast<void**>(b) = AsPtr(vt1);
            int r0 = 0, r1 = 0;
            reinterpret_cast<void(__cdecl*)(const void*, int*)>(vt0[22])(a, &r0);
            reinterpret_cast<void(__cdecl*)(const void*, int*)>(vt1[22])(b, &r1);
            Check(r0 == r1, "vtable IsLoaded");
        }
        // slot 24/25 SetDisplayState/ClearDisplayState on the 26-slot classes, plus 23 CacheAlphaHint
        if (kinds[k].oneTexture) {
            std::memset(a, 0, sizeof a);
            std::memset(b, 0, sizeof b);
            *reinterpret_cast<void**>(a) = AsPtr(vt0);
            *reinterpret_cast<void**>(b) = AsPtr(vt1);
            if (k == 7 || k == 8) {
                // ClearDisplayState dereferences the cube map unconditionally in the original
                *reinterpret_cast<void**>(a + 0xa8) = fakeTexture;
                *reinterpret_cast<void**>(b + 0xa8) = fakeTexture;
            }
            ResetDisplay();
            reinterpret_cast<void(__cdecl*)(const void*, void*)>(vt0[24])(a, Display());
            const int calls0 = displayCallCount;
            ResetDisplay();
            reinterpret_cast<void(__cdecl*)(const void*, void*)>(vt1[24])(b, Display());
            Check(calls0 == displayCallCount, "vtable SetDisplayState calls");
            ResetDisplay();
            reinterpret_cast<void(__cdecl*)(const void*, void*)>(vt0[25])(a, Display());
            const int clear0 = displayCallCount;
            ResetDisplay();
            reinterpret_cast<void(__cdecl*)(const void*, void*)>(vt1[25])(b, Display());
            Check(clear0 == displayCallCount, "vtable ClearDisplayState calls");
        }
    }
}

// The exported destructors, called with the PluginInstance/vbase pointer (the original's ABI: the body subtracts
// the vbase offset). A zeroed image has no members to release; the visible effect is the vtable stores.
void TestDestructors()
{
    for (int k = 0; k < 10; ++k) {
        std::snprintf(nameBuffer, sizeof nameBuffer, "??1%s@Jet@@UAE@XZ", kinds[k].cls);
        auto fn0 = Proc<void(__thiscall*)(void*)>(ours, nameBuffer, "ours");
        auto fn1 = Proc<void(__thiscall*)(void*)>(orig, nameBuffer, "orig");
        std::snprintf(nameBuffer, sizeof nameBuffer, "??_8%s@Jet@@7B@", kinds[k].cls);
        const int* const vb = static_cast<const int*>(Optional(ours, nameBuffer));
        if (!vb) continue;
        const unsigned vbase = 4 + static_cast<unsigned>(vb[1]);
        alignas(8) unsigned char a[0x200], b[0x200];
        std::memset(a, 0, sizeof a);
        std::memset(b, 0, sizeof b);
        // the destructor body reads the class's vbtable at +4
        std::snprintf(nameBuffer, sizeof nameBuffer, "??_8%s@Jet@@7B@", kinds[k].cls);
        *reinterpret_cast<const void**>(a + 4) = Optional(ours, nameBuffer);
        *reinterpret_cast<const void**>(b + 4) = Optional(orig, nameBuffer);
        fn0(a + vbase);
        fn1(b + vbase);
        // the two vtables/vbtable words are module data; the rest (the base-destructor's field clears) must match
        bool same = true;
        for (unsigned off = 0; off + 4 <= kinds[k].size; off += 4) {
            if (off == 0 || off == 4 || off == 4 + static_cast<unsigned>(vb[1])) continue;
            if (std::memcmp(a + off, b + off, 4) != 0) {
                std::printf("destructor %s at +0x%x\n", kinds[k].cls, off);
                same = false;
                break;
            }
        }
        Check(same, "destructor");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Reflective render paths

void TestReflectiveRender()
{
    const char* const names[3] = {
        "?RenderChunk@ReflectiveMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDyna"
        "micArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@H@Z",
        "?RenderNumChunks@ReflectiveMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAPAVGeometryChunk@2@PAPBV?$"
        "FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PAPBVMatrix4@2@IH"
        "@Z",
        "?RenderPrimitives@ReflectiveMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$Fas"
        "tDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@IPBGIW4"
        "DPrimitive@2@H@Z"
    };
    // The mangled names above are easy to get wrong; fall back to a scan for the three render entries.
    for (int i = 0; i < 3; ++i) {
        const void* f0 = Optional(ours, names[i]);
        const void* f1 = Optional(orig, names[i]);
        if (!f0 || !f1) {
            std::printf("skip render export %d (%s)\n", i, f0 ? "orig" : "ours");
            continue;
        }
        alignas(16) unsigned char material[0x180];
        std::memset(material, 0, sizeof material);
        *reinterpret_cast<unsigned*>(material + 0x80) = 0x3f000000;  // opacity 0.5
        *reinterpret_cast<unsigned char*>(material + 0xb8) = 1;     // flags2 bit 0: no spherical path
        *reinterpret_cast<unsigned*>(material + 0xa0) = 0xffffffffu;  // alphaHint
        *reinterpret_cast<void**>(material + 0xa8) = fakeTexture;    // the render body dereferences the cube map

        Matrix4 view, modelView;
        for (int j = 0; j < 16; ++j) {
            view.m[j] = 0.5f * j;
            modelView.m[j] = -0.25f * j;
        }
        const unsigned lightBytes[2] = {0x11111111u, 0x22222222u};
        const unsigned lightBytes2[2] = {0x33333333u, 0x44444444u};
        const void* lights[2] = {lightBytes, lightBytes2};
        unsigned char program1[4] = {1, 2, 3, 4};

        auto call = [&](HMODULE module, const void* fn) {
            // ClearDisplayState is virtual for the one-texture family: the render body calls it through the
            // material's own vtable, so the image must carry the module's table.
            *reinterpret_cast<void**>(material) = Optional(module, "??_7ReflectiveMaterial@Jet@@6BMaterial@1@@");
            ResetDisplay();
            ResetLighting();
            ResetChunk(0.25f, program1);
            if (i == 0) {
                reinterpret_cast<void(__cdecl*)(void*, void*, void*, void*, const void*, const void*, int)>(
                    const_cast<void*>(fn))(material, Display(), viewBytes, chunkBytes, lights[0], &modelView, -1);
            } else if (i == 1) {
                void* chunks[2] = {chunkBytes, chunkBytes};
                const void* const lightLists[2] = {lights[0], lights[1]};
                const Matrix4* const matrices[2] = {&modelView, &modelView};
                const void* const* const lightListPtrs = lightLists;
                const Matrix4* const* const matrixPtrs = matrices;
                reinterpret_cast<void(__cdecl*)(void*, void*, void*, void**, const void* const*, const Matrix4* const*, unsigned,
                                              int)>(const_cast<void*>(fn))(
                    material, Display(), viewBytes, chunks, lightListPtrs, matrixPtrs, 2, -1);
            } else {
                const unsigned short indices[3] = {0, 1, 2};
                reinterpret_cast<void(__cdecl*)(void*, void*, void*, void*, const void*, const void*, unsigned,
                                              const void*, unsigned, int, int)>(const_cast<void*>(fn))(
                    material, Display(), viewBytes, chunkBytes, lights[0], &modelView, 3, indices, 2, 4, -1);
            }
            (void)module;
        };
        call(ours, f0);
        const LightingCall l0 = lighting;
        const int dc0 = displayCallCount;
        const DisplayCall d0[8] = {displayCalls[0], displayCalls[1], displayCalls[2], displayCalls[3],
                                   displayCalls[4], displayCalls[5], displayCalls[6], displayCalls[7]};
        const int rc0 = renderCalls, pc0 = primitiveCalls;
        const unsigned ni0 = lastNumIndices, np0 = lastNumPrimitives;
        call(orig, f1);
        const LightingCall l1 = lighting;
        Check(l0.count == l1.count && l0.view != nullptr && l0.view == l1.view &&
                  l0.chunk == reinterpret_cast<const void*>(chunkBytes) &&
                  l1.chunk == reinterpret_cast<const void*>(chunkBytes) && l0.program == program1 &&
                  l1.program == program1 && l0.opacityBits == l1.opacityBits && l0.flag == l1.flag,
              "Reflective render lighting");
        Check(dc0 == displayCallCount, "Reflective render display calls");
        bool sameCalls = true;
        for (int j = 0; j < dc0 && j < 8; ++j)
            if (d0[j].slot != displayCalls[j].slot) sameCalls = false;
        Check(sameCalls, "Reflective render display slots");
        Check(rc0 == renderCalls && pc0 == primitiveCalls && ni0 == lastNumIndices && np0 == lastNumPrimitives,
              "Reflective render chunk calls");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Plugin resource construction: fake kernel with a real RenderingCacheManager at +0x6c and a display at +0x7c.

// The real cache manager's ring is only valid inside a RenderKernel; the factories only ask it for a cache by name,
// so the imports are replaced with a miss + a no-op insert (the caches themselves are the real objects).
Jet::RenderingCache* __cdecl StubFindCache(void*, const Jet::PString*) { return nullptr; }
void __cdecl StubAddCache(void*, Jet::RenderingCache*) {}

void TestPluginResources()
{
    const char* const kFind = "?FindCache@RenderingCacheManager@Jet@@QAAPAVRenderingCache@2@ABVPString@2@@Z";
    const char* const kAdd = "?AddCache@RenderingCacheManager@Jet@@QAAXPAVRenderingCache@2@@Z";
    void* const f0 = PatchImport(ours, kFind, reinterpret_cast<void*>(StubFindCache));
    void* const f1 = PatchImport(orig, kFind, reinterpret_cast<void*>(StubFindCache));
    void* const a0 = PatchImport(ours, kAdd, reinterpret_cast<void*>(StubAddCache));
    void* const a1 = PatchImport(orig, kAdd, reinterpret_cast<void*>(StubAddCache));
    if (!f0 || !f1 || !a0 || !a1) {
        std::printf("skip plugin resources: imports not patched\n");
        return;
    }

    alignas(16) unsigned char kernelA[0x100];
    alignas(16) unsigned char kernelB[0x100];
    struct PluginInfo { const char* cls; int size; bool isAutoTex; };
    const PluginInfo plugins[8] = {
        {"NoTextureMaterialPlugin", 0x2c, false},
        {"OneTextureMaterialPlugin", 0x34, false},
        {"OneTextureNoAlphaMaterialPlugin", 0x34, false},
        {"ColorOneTextureMaterialPlugin", 0x34, false},
        {"GenericShaderMaterialPlugin", 0x28, false},
        {"MVHSelectMaterialPlugin", 0x5c, false},
        {"ReflectiveMaterialPlugin", 0x5c, false},
        {"GlossMaterialPlugin", 0x5c, false},
    };
    for (int i = 0; i < 8; ++i) {
        std::memset(kernelA, 0, sizeof kernelA);
        std::memset(kernelB, 0, sizeof kernelB);
        *reinterpret_cast<void**>(kernelA + 0x7c) = Display();
        *reinterpret_cast<void**>(kernelB + 0x7c) = Display();

        alignas(16) unsigned char a[0x80], b[0x80];
        std::memset(a, 0xcd, sizeof a);
        std::memset(b, 0xcd, sizeof b);
        // Construct through the exported default constructor (base ctor only registers; no manager needed).
        std::snprintf(nameBuffer, sizeof nameBuffer, "??0%s@Jet@@QAE@XZ", plugins[i].cls);
        auto ctor0 = Proc<void(__thiscall*)(void*)>(ours, nameBuffer, "ours");
        auto ctor1 = Proc<void(__thiscall*)(void*)>(orig, nameBuffer, "orig");
        ctor0(a);
        ctor1(b);
        // The exported CreatePluginResources (non-virtual for the one-texture family, virtual for Std-only).
        std::snprintf(nameBuffer, sizeof nameBuffer, "?CreatePluginResources@%s@Jet@@IBAXPAVRenderKernel@2@@Z",
                      plugins[i].cls);
        void* const cp0 = Optional(ours, nameBuffer);
        void* const cp1 = Optional(orig, nameBuffer);
        if (!cp0 || !cp1) { // StdMaterialPlugin's is MBAX (protected virtual); try that form
            std::snprintf(nameBuffer, sizeof nameBuffer, "?CreatePluginResources@%s@Jet@@MBAXPAVRenderKernel@2@@Z",
                          plugins[i].cls);
        }
        if (!cp0 || !cp1) continue;
        ResetDisplay();
        reinterpret_cast<void(__cdecl*)(void*, void*)>(cp0)(a, kernelA);
        const int calls0 = displayCallCount;
        ResetDisplay();
        reinterpret_cast<void(__cdecl*)(void*, void*)>(cp1)(b, kernelB);
        Check(calls0 == displayCallCount, "plugin resource stage calls");
        // cache slots at +0x20/+0x24/+0x28 are non-null on both (the objects differ, their priority/name must not)
        for (unsigned off = 0x20; off <= 0x28; off += 4) {
            const bool present0 = *reinterpret_cast<void**>(a + off) != nullptr;
            const bool present1 = *reinterpret_cast<void**>(b + off) != nullptr;
            if (present0 != present1) {
                std::printf("plugin %s +0x%x ours %p orig %p\n", plugins[i].cls, off,
                            *reinterpret_cast<void**>(a + off), *reinterpret_cast<void**>(b + off));
            }
            Check(present0 == present1, "plugin resource cache");
        }
        // calling again must reuse the slots (same pointers)
        void* const keep = *reinterpret_cast<void**>(a + 0x20);
        reinterpret_cast<void(__cdecl*)(void*, void*)>(cp0)(a, kernelA);
        Check(*reinterpret_cast<void**>(a + 0x20) == keep, "plugin resource reuse");
    }
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: %s <fixture dir> [rounds]\n", argv[0]);
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 100;
    SetDllDirectoryA(argv[1]);
    AddVectoredExceptionHandler(1, VectoredHandler);
    char path[MAX_PATH];
    std::snprintf(path, sizeof path, "%s\\Render3DCore_7c_orig.dll", argv[1]);
    HMODULE origTry = LoadLibraryA(path);
    if (!origTry) std::printf("orig load failed %lu\n", GetLastError());
    std::snprintf(path, sizeof path, "%s\\Render3DCore_7c.dll", argv[1]);
    ours = LoadLibraryA(path);
    if (!ours) {
        std::printf("ours load failed %lu\n", GetLastError());
        return 2;
    }
    orig = GetModuleHandleA("Render3DCore_7c_orig.dll");
    if (!orig) return 2;

    InitDisplay();
    InitChunk();
    InitFakeTexture();

    const char* const kSetLighting =
        "?SetLightingState@Material@Jet@@QBAXPAVDisplay@2@PBVMatrix4@2@PBVGeometryChunk@2@PBVVertexProgram@2@PBV?$"
        "FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@1M_N@Z";
    void* const patched0 = PatchImport(ours, kSetLighting, reinterpret_cast<void*>(&RecordLighting));
    void* const patched1 = PatchImport(orig, kSetLighting, reinterpret_cast<void*>(&RecordLighting));
    if (!patched0 || !patched1) {
        std::printf("SetLightingState import not patched (ours %p orig %p)\n", patched0, patched1);
        return 2;
    }

    TestData();
    TestVtableDispatch();
    TestDestructors();
    for (int round = 0; round < rounds; ++round) TestReflectiveRender();
    TestPluginResources();

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad == 0 ? 0 : 1;
}
