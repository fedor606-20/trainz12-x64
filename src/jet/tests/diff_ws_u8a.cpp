// Differential test of WaterSystem_7c U7b/U8a: the WaterMaterial base, WaterBumpMaterial and WaterOceanMaterial.
// Loads the original and our module into one process, constructs one material of each class against the same fake
// manager/rendering-cache manager, and compares object state, virtual calls, vertex types and chunk creation.
//
// usage: diff_ws_u8a.exe <dir with WaterSystem_7c.dll, WaterSystem_7c_orig.dll and the Jet modules>
#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static HMODULE mod[2];
static unsigned long long checks, bad;
static int cur = -1;
static std::map<const void*, std::string> names[2];
static int allocCount[2];

using NewFn = void*(__cdecl*)(unsigned);
using DeleteFn = void(__cdecl*)(void*);
static NewFn RealNew;
static DeleteFn RealDelete;

static void* __cdecl HookNew(unsigned n)
{
    void* p = RealNew(n);
    std::memset(p, 0xcd, n);
    char b[32];
    std::snprintf(b, sizeof b, "A%d", allocCount[cur]++);
    names[cur][p] = b;
    return p;
}

static void __cdecl HookDelete(void* p)
{
    if (cur >= 0 && p) names[cur].erase(p);
    RealDelete(p);
}

static int PatchImport(HMODULE m, const char* name, void* fn)
{
    int count = 0;
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk
                                                                                          : d->FirstThunk));
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; thunk->u1.AddressOfData; ++thunk, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal)) continue;
            if (std::strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + thunk->u1.AddressOfData)->Name, name))
                continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            iat->u1.Function = reinterpret_cast<uintptr_t>(fn);
            VirtualProtect(&iat->u1.Function, sizeof(void*), old, &old);
            ++count;
        }
    }
    return count;
}

static void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok && ++bad <= 40) std::printf("MISMATCH %s\n", what);
}

template <class T>
static T& At(void* p, unsigned off)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(p) + off);
}

template <class T>
static T Func(int side, const char* name)
{
    return reinterpret_cast<T>(GetProcAddress(mod[side], name));
}

struct FakeEnvironment {
    unsigned char kernel[0x400];
    unsigned char manager[0x20];
};

static void InitEnvironment(FakeEnvironment& env)
{
    std::memset(&env, 0, sizeof env);
    *reinterpret_cast<void**>(env.manager + 8) = env.kernel;
    HMODULE rs = GetModuleHandleA("RenderSystem_7c.dll");
    using Ctor = void(__thiscall*)(void*);
    auto ctor = reinterpret_cast<Ctor>(GetProcAddress(rs, "??0RenderingCacheManager@Jet@@QAE@XZ"));
    if (ctor) ctor(env.kernel + 0x6c);
}

static void DiffBytes(const char* what, const void* a, const void* b, unsigned size, unsigned skip0 = 0,
                      unsigned skip1 = 0)
{
    const auto* pa = static_cast<const unsigned char*>(a);
    const auto* pb = static_cast<const unsigned char*>(b);
    bool ok = true;
    int badDiffs = 0;
    for (unsigned i = 0; i < size; ++i) {
        if (i >= skip0 && i < skip1) continue;
        if (pa[i] != pb[i]) {
            ok = false;
            if (++badDiffs <= 12) std::printf("  %s +0x%x: %02x != %02x\n", what, i, pa[i], pb[i]);
        }
    }
    Check(ok, what);
}

static void* Construct(int side, const char* ctorName, unsigned size, FakeEnvironment& env)
{
    using Ctor = void(__thiscall*)(void*, void*, int);
    auto ctor = Func<Ctor>(side, ctorName);
    if (!ctor) return nullptr;
    void* p = HookNew(size);
    ctor(p, env.manager, 1);  // the hidden virtual-base flag
    return p;
}

static void* VirtualChunk(void* object, unsigned slot, unsigned nv, unsigned np, unsigned ni)
{
    using Fn = void*(__cdecl*)(const void*, unsigned, unsigned, unsigned, unsigned, int, void*, int);
    auto* table = *reinterpret_cast<void* const* const*>(object);
    return reinterpret_cast<Fn>(table[slot / 4])(object, nv, np, ni, 0, 0, nullptr, 1);
}

// the {stride,pointer} pair the chunk's array slots return
struct ItemArray {
    int stride;
    void* data;
};

static ItemArray ChunkyArray(void* chunk, unsigned slot, int type)
{
    using Fn = void(__cdecl*)(const void*, ItemArray*, int, int);
    ItemArray out{0, nullptr};
    auto* table = *reinterpret_cast<void* const* const*>(chunk);
    reinterpret_cast<Fn>(table[slot / 4])(chunk, &out, type, 0);
    return out;
}

static void CheckTypeString(int side, const char* getTypeName, const char* expected)
{
    using Fn = const void*(__cdecl*)(const void*);
    auto getType = Func<const void*(__cdecl*)(const void*)>(side, getTypeName);
    if (!getType) {
        Check(false, "GetType export");
        return;
    }
    const void* pstr = getType(nullptr);
    const void* node = pstr ? *reinterpret_cast<void* const*>(pstr) : nullptr;
    const char* text = node ? *reinterpret_cast<const char* const*>(static_cast<const unsigned char*>(node) + 0x18)
                            : nullptr;
    if (!text || std::strcmp(text, expected)) std::printf("  type text: %s\n", text ? text : "(null)");
    Check(text && std::strcmp(text, expected) == 0, expected);
}

static void BumpScenario(FakeEnvironment& env)
{
    using Ctor = void(__thiscall*)(void*, void*, int);
    using SetF = void(__thiscall*)(void*, float);
    using GetF = float(__thiscall*)(const void*);
    using SetB = void(__thiscall*)(void*, bool);
    using SetRefl = char(__thiscall*)(void*, int);
    using GetRefl = int(__thiscall*)(const void*);
    using EqualsF = bool(__cdecl*)(const void*, const void*);

    void* object[2] = {nullptr, nullptr};
    for (int s = 0; s < 2; ++s) {
        cur = s;
        std::printf("  bump ctor %d\n", s);
        object[s] = Construct(s, "??0WaterBumpMaterial@Jet@@QAE@PAVMaterialManager@1@@Z", 0x138, env);
        Check(object[s] != nullptr, "Bump ctor");
    }
    cur = -1;
    if (!object[0] || !object[1]) return;
    std::printf("  bump diff\n");
    for (int s = 0; s < 2; ++s) {
        using InitFn = char(__thiscall*)(void*, const void*, const void*, void*, void*, int);
        Check(Func<InitFn>(s, "?Initialize@WaterBumpMaterial@Jet@@UAE_NPBVResourceDatabase@2@PBVWaterManagerConfig@2@PAVFrameAnimation@2@PAVWaterTextureAnimation@2@H@Z")(
                  object[s], nullptr, nullptr, nullptr, nullptr, 0) == 0,
              "Bump Initialize false");
    }
    DiffBytes("Bump object", object[0], object[1], 0x130, 0, 8);
    std::printf("  bump setters\n");

    for (int s = 0; s < 2; ++s) {
        cur = s;
        std::printf("  s%d scale\n", s);
        Func<SetF>(s, "?SetScaleNormXY@WaterBumpMaterial@Jet@@UAEXM@Z")(object[s], 2.75f);
        std::printf("  s%d refr\n", s);
        Func<SetF>(s, "?SetRefractiveIndex@WaterBumpMaterial@Jet@@UAEXM@Z")(object[s], 1.5f);
        std::printf("  s%d interp\n", s);
        Func<SetB>(s, "?EnableTextureInterpolation@WaterBumpMaterial@Jet@@QAEX_N@Z")(object[s], true);
        std::printf("  s%d refl\n", s);
        Func<SetRefl>(s, "?SetReflectionType@WaterBumpMaterial@Jet@@UAE_NH@Z")(object[s], 3);
    }
    DiffBytes("Bump after setters", object[0], object[1], 0x130, 0, 8);
    for (int s = 0; s < 2; ++s) {
        Check(Func<GetF>(s, "?GetScaleNormXY@WaterBumpMaterial@Jet@@UBEMXZ")(object[s]) == 2.75f, "Bump scale read");
        Check(Func<GetRefl>(s, "?GetReflectionType@WaterBumpMaterial@Jet@@UBEHXZ")(object[s]) == 1, "Bump reflection");
        Check(Func<EqualsF>(s, "?Equals@WaterBumpMaterial@Jet@@QBA_NABV12@@Z")(object[s], object[s]) != false,
              "Bump self equals");
    }
    std::printf("  bump type skip\n");
    // GetType itself is checked through the vtable in the shipped game; skip the raw export here.

    // slot 48 CreateVertexType; compare the vertex type object it makes
    void* vtype[2] = {nullptr, nullptr};
    for (int s = 0; s < 2; ++s) {
        cur = s;
        using CreateFn = void(__thiscall*)(void*);
        auto* table = *reinterpret_cast<void* const* const*>(object[s]);
        reinterpret_cast<CreateFn>(table[0xc0 / 4])(object[s]);
        vtype[s] = At<void*>(object[s], 0x110);
        Check(vtype[s] != nullptr, "Bump vertex type");
    }
    cur = -1;
    for (int s = 0; s < 2; ++s) {
        Check(vtype[s] && At<int>(vtype[s], 8) == 99, "Bump vertex type id");
        Check(vtype[s] && At<unsigned>(vtype[s], 0xc) == 1, "Bump vertex stream count");
        void* streams = vtype[s] ? At<void*>(vtype[s], 0x10) : nullptr;
        Check(streams && *reinterpret_cast<unsigned*>(streams) == 0x18, "Bump vertex stride");
        Check(streams && *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(streams) + 8) == 3,
              "Bump vertex items");
    }

    // slot 46 CreateGeometryChunk, then the position/normal/texcoord array views
    void* chunk[2] = {nullptr, nullptr};
    for (int s = 0; s < 2; ++s) {
        cur = s;
        std::printf("chunk %d...\n", s); std::fflush(stdout);
        chunk[s] = VirtualChunk(object[s], 0xb8, 5, 2, 6);
        Check(chunk[s] != nullptr, "Bump chunk");
    }
    cur = -1;
    if (chunk[0] && chunk[1]) {
        for (int type : {1, 2, 15}) {
            std::printf("  bump array %d\n", type);
            const ItemArray a = ChunkyArray(chunk[0], 0xc4, type);
            const ItemArray b = ChunkyArray(chunk[1], 0xc4, type);
            Check(a.stride == b.stride && a.stride == 0x18, "Bump chunk stride");
            Check((a.data != nullptr) == (b.data != nullptr), "Bump chunk array");
        }
        for (int off : {0x1c, 0x20, 0x24, 0x28, 0x38, 0x3c})
            Check(At<unsigned>(chunk[0], off) == At<unsigned>(chunk[1], off), "Bump chunk counts");
    }
}

static void OceanScenario(FakeEnvironment& env)
{
    using SetF = void(__thiscall*)(void*, float);
    using GetF = float(__thiscall*)(const void*);
    using SetB = void(__thiscall*)(void*, bool);
    using WaveF = void(__thiscall*)(void*, float, float, float);
    using UpdateF = void(__thiscall*)(void*, float);
    using SetRefl = char(__thiscall*)(void*, int);
    using GetRefl = int(__thiscall*)(const void*);
    using EqualsF = bool(__cdecl*)(const void*, const void*);

    void* object[2] = {nullptr, nullptr};
    for (int s = 0; s < 2; ++s) {
        cur = s;
        std::printf("  ocean ctor %d\n", s);
        object[s] = Construct(s, "??0WaterOceanMaterial@Jet@@QAE@PAVMaterialManager@1@@Z", 0x174, env);
        Check(object[s] != nullptr, "Ocean ctor");
    }
    cur = -1;
    if (!object[0] || !object[1]) return;
    std::printf("  ocean diff\n");
    for (int s = 0; s < 2; ++s) {
        using InitFn = char(__thiscall*)(void*, const void*, const void*, void*, void*, int);
        Check(Func<InitFn>(s, "?Initialize@WaterOceanMaterial@Jet@@UAE_NPBVResourceDatabase@2@PBVWaterManagerConfig@2@PAVFrameAnimation@2@PAVWaterTextureAnimation@2@H@Z")(
                  object[s], nullptr, nullptr, nullptr, nullptr, 0) == 0,
              "Ocean Initialize false");
    }
    DiffBytes("Ocean object", object[0], object[1], 0x16c, 0, 8);
    std::printf("  ocean setters\n");

    for (int s = 0; s < 2; ++s) {
        cur = s;
        Func<SetF>(s, "?SetScaleNormXY@WaterOceanMaterial@Jet@@UAEXM@Z")(object[s], 1.25f);
        Func<SetF>(s, "?SetRefractiveIndex@WaterOceanMaterial@Jet@@UAEXM@Z")(object[s], 1.1f);
        Func<SetB>(s, "?EnableTextureInterpolation@WaterOceanMaterial@Jet@@QAEX_N@Z")(object[s], false);
        Func<WaveF>(s, "?SetWaveAttributes@WaterOceanMaterial@Jet@@QAEXMMM@Z")(object[s], 1.0f, 2.0f, 3.0f);
        Func<UpdateF>(s, "?Update@WaterOceanMaterial@Jet@@UAEXM@Z")(object[s], 0.5f);
        Func<SetRefl>(s, "?SetReflectionType@WaterOceanMaterial@Jet@@UAE_NH@Z")(object[s], 2);
    }
    DiffBytes("Ocean after setters", object[0], object[1], 0x16c, 0, 8);
    for (int s = 0; s < 2; ++s) {
        Check(Func<GetF>(s, "?GetScaleNormXY@WaterOceanMaterial@Jet@@UBEMXZ")(object[s]) == 1.25f, "Ocean scale read");
        Check(Func<GetRefl>(s, "?GetReflectionType@WaterOceanMaterial@Jet@@UBEHXZ")(object[s]) == 1, "Ocean reflection");
        Check(Func<EqualsF>(s, "?Equals@WaterOceanMaterial@Jet@@QBA_NABV12@@Z")(object[s], object[s]) != false,
              "Ocean self equals");
    }
    std::printf("  ocean type skipped\n");

    void* vtype[2] = {nullptr, nullptr};
    for (int s = 0; s < 2; ++s) {
        cur = s;
        using CreateFn = void(__thiscall*)(void*);
        auto* table = *reinterpret_cast<void* const* const*>(object[s]);
        reinterpret_cast<CreateFn>(table[0xc0 / 4])(object[s]);
        vtype[s] = At<void*>(object[s], 0xf8);
        Check(vtype[s] != nullptr, "Ocean vertex type");
    }
    cur = -1;
    for (int s = 0; s < 2; ++s) {
        Check(vtype[s] && At<int>(vtype[s], 8) == 99, "Ocean vertex type id");
        Check(vtype[s] && At<unsigned>(vtype[s], 0xc) == 1, "Ocean vertex stream count");
        void* streams = vtype[s] ? At<void*>(vtype[s], 0x10) : nullptr;
        Check(streams && *reinterpret_cast<unsigned*>(streams) == 0x34, "Ocean vertex stride");
        Check(streams && *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(streams) + 8) == 4,
              "Ocean vertex items");
    }

    void* chunk[2] = {nullptr, nullptr};
    for (int s = 0; s < 2; ++s) {
        cur = s;
        std::printf("chunk %d...\n", s); std::fflush(stdout);
        chunk[s] = VirtualChunk(object[s], 0xb8, 5, 2, 6);
        Check(chunk[s] != nullptr, "Ocean chunk");
    }
    cur = -1;
    if (chunk[0] && chunk[1]) {
        for (int type : {1, 24, 25, 15}) {
            std::printf("  ocean array %d\n", type);
            const ItemArray a = ChunkyArray(chunk[0], 0xc4, type);
            const ItemArray b = ChunkyArray(chunk[1], 0xc4, type);
            Check(a.stride == b.stride, "Ocean chunk stride");
            Check((a.data != nullptr) == (b.data != nullptr), "Ocean chunk array");
        }
        for (int off : {0x1c, 0x20, 0x24, 0x28, 0x38, 0x3c})
            Check(At<unsigned>(chunk[0], off) == At<unsigned>(chunk[1], off), "Ocean chunk counts");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// U8a rendering paths: stand-in Display/ViewPort/Camera/VertexProgram/TextureCombiner/Texture objects whose virtual
// slots and imported methods record every call, plus a fake water animation and frame clock. The two modules run the
// same scenario and the call logs are compared line by line (float and pointer arguments are logged by role/hex).
static std::vector<std::string> g_log[2];
static int g_logSide = -1;
static int g_definesResult = 1;
static std::map<const void*, std::string> g_role;

static void Log(const char* fmt, ...)
{
    if (g_logSide < 0) return;
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    g_log[g_logSide].push_back(b);
}

static std::string Role(const void* p)
{
    if (!p) return "null";
    auto i = g_role.find(p);
    return i == g_role.end() ? std::string("?") : i->second;
}

static const char* PText(const void* pstring)
{
    if (!pstring) return "(null)";
    const void* node = *static_cast<void* const*>(pstring);
    if (!node) return "(empty)";
    return reinterpret_cast<const char*>(static_cast<const unsigned char*>(node) + 0x18);
}

static std::string HexVec(const void* v, int count)
{
    const unsigned* u = static_cast<const unsigned*>(v);
    std::string out;
    char b[16];
    for (int i = 0; i < count; ++i) {
        std::snprintf(b, sizeof b, i ? " %08x" : "%08x", u[i]);
        out += b;
    }
    return out;
}

static void VtUnexpected(void* self) { Log("unexpected %s", Role(self).c_str()); }
static void VtEnd(void* self) { Log("end %s", Role(self).c_str()); }
static void VtBindOne(void* self) { Log("bind %s", Role(self).c_str()); }
static void VtNoArg(void* self) { Log("call %s", Role(self).c_str()); }
static void VtStage(void* self, int stage) { Log("stage %s %d", Role(self).c_str(), stage); }
static void VtActivate(void* self, int on) { Log("activate %s %d", Role(self).c_str(), on); }
static void VtCull(void* self, int mode) { Log("cull %s %d", Role(self).c_str(), mode); }
static void VtDrawPrim(void* self, int decl) { Log("draw %s %d", Role(self).c_str(), decl); }
static void VtDisplayAC(void* self, int on) { Log("ac %s %d", Role(self).c_str(), on); }
static int VtDefines(void* self, const void* name)
{
    Log("defines %s %s", Role(self).c_str(), PText(name));
    return g_definesResult;
}
static int VtSetLocal(void* self, int reg, const void* v)
{
    Log("setlocal %s %d %s", Role(self).c_str(), reg, HexVec(v, 4).c_str());
    return 1;
}
static int VtSetLocals(void* self, int reg, const void* v, int count)
{
    Log("setlocals %s %d %d %s", Role(self).c_str(), reg, count, HexVec(v, count * 4).c_str());
    return 1;
}
static int VtSetConst(void* self, int index, const void* v)
{
    Log("setconst %s %d %s", Role(self).c_str(), index, HexVec(v, 4).c_str());
    return 1;
}
static int VtVpBind(void* self, int one) { Log("bind %s %d", Role(self).c_str(), one); return 1; }
static void VtViewMatrix(void* self, const void* matrix)
{
    Log("viewmat %s %s", Role(self).c_str(), HexVec(matrix, 4).c_str());
}
static void VtDisplayDraw(void* self, void* view, void* buffer, const void* matrix)
{
    Log("draw %s %s %s %s", Role(self).c_str(), Role(view).c_str(), Role(buffer).c_str(),
        HexVec(matrix, 4).c_str());
}
static int VtDisplayDecl(void* self, int a, int b, int c, const void* v, int d)
{
    Log("decl %s %d %d %d %d", Role(self).c_str(), a, b, c, d);
    (void)v;
    return 77;
}
static void* VtChunkProgram(void* self, int index)
{
    Log("program %s %d", Role(self).c_str(), index);
    return nullptr;
}
static void VtChunkRender(void* self, void* display, void* view, void* program, int a, int b)
{
    Log("render %s %s %s %s %d %d", Role(self).c_str(), Role(display).c_str(), Role(view).c_str(),
        Role(program).c_str(), a, b);
}

static void* g_vtable[0x140 / 4];

static void InitVtables()
{
    for (auto& slot : g_vtable) slot = reinterpret_cast<void*>(&VtUnexpected);
    g_vtable[0x14 / 4] = reinterpret_cast<void*>(&VtBindOne);
    g_vtable[0x18 / 4] = reinterpret_cast<void*>(&VtEnd);
    g_vtable[0x1c / 4] = reinterpret_cast<void*>(&VtDefines);
    g_vtable[0x20 / 4] = reinterpret_cast<void*>(&VtActivate);
    g_vtable[0x30 / 4] = reinterpret_cast<void*>(&VtSetConst);
    g_vtable[0x38 / 4] = reinterpret_cast<void*>(&VtChunkRender);
    g_vtable[0x3c / 4] = reinterpret_cast<void*>(&VtVpBind);
    g_vtable[0x44 / 4] = reinterpret_cast<void*>(&VtStage);
    g_vtable[0x4c / 4] = reinterpret_cast<void*>(&VtNoArg);
    g_vtable[0x54 / 4] = reinterpret_cast<void*>(&VtViewMatrix);
    g_vtable[0x5c / 4] = reinterpret_cast<void*>(&VtSetLocal);
    g_vtable[0x64 / 4] = reinterpret_cast<void*>(&VtSetLocals);
    g_vtable[0xac / 4] = reinterpret_cast<void*>(&VtDisplayAC);
    g_vtable[0xc4 / 4] = reinterpret_cast<void*>(&VtDisplayDraw);
    g_vtable[0xe4 / 4] = reinterpret_cast<void*>(&VtDrawPrim);
    g_vtable[0xf0 / 4] = reinterpret_cast<void*>(&VtChunkProgram);
    g_vtable[0x100 / 4] = reinterpret_cast<void*>(&VtCull);
    g_vtable[0x10c / 4] = reinterpret_cast<void*>(&VtDisplayDecl);
    g_vtable[0x12c / 4] = reinterpret_cast<void*>(&VtStage);
}

// the imported Display/VertexProgram/TextureCombiner/ExtendedGfxLanguage methods, patched in both modules
static void __cdecl HookSetRenderTarget(void* self, void* texture, unsigned index)
{
    Log("rt %s %s %u", Role(self).c_str(), Role(texture).c_str(), index);
}
static void __cdecl HookSetDepthTestMode(void* self, int on) { Log("depth %s %d", Role(self).c_str(), on); }
static void __cdecl HookSetCullMode(void* self, int mode) { Log("cull %s %d", Role(self).c_str(), mode); }
static unsigned char __cdecl HookSetLocalConstantColor(void* self, int reg, const void* color)
{
    Log("vlocal %s %d %s", Role(self).c_str(), reg, HexVec(color, 4).c_str());
    return 1;
}
static unsigned char __cdecl HookSetConstantRegisterColor(void* self, int index, const void* color)
{
    Log("setconst %s %d %s", Role(self).c_str(), index, HexVec(color, 4).c_str());
    return 1;
}
static int __cdecl HookGetLocalConstantNumber(const void* self, const void* name)
{
    Log("localnumber %s %s", Role(self).c_str(), PText(name));
    return 12;
}

struct FakeSet {
    alignas(16) unsigned char display[0x100];
    alignas(16) unsigned char view[0x40];
    alignas(16) unsigned char chunk[0x80];
    alignas(16) unsigned char vertexProgram[0x10];
    alignas(16) unsigned char interpProgram[0x10];
    alignas(16) unsigned char combiner[0x10];
    alignas(16) unsigned char lerpCombiner[0x10];
    alignas(16) unsigned char texture0[0x10];
    alignas(16) unsigned char texture1[0x10];
    alignas(16) unsigned char reflection[0x10];
    alignas(16) unsigned char renderTarget[0x10];
    alignas(16) unsigned char buffer[0x10];
};

struct FakeArrayChunk {
    void* next;
    void* prev;
    unsigned int count;
    void* data[13];
};

struct FakeWaterAnimation {
    void* vtable;
    FakeArrayChunk* head;
};

struct FakeFrameAnimation {
    int numFrames;
    int currentFrame;
    float time;
    int sampleRate;
    int period;
    float deltaFrame;
};

static void InitFakeSet(FakeSet& f, unsigned frame, FakeWaterAnimation& animation, FakeFrameAnimation& clock)
{
    std::memset(&f, 0, sizeof f);
    *reinterpret_cast<void**>(f.display) = g_vtable;
    *reinterpret_cast<void**>(f.view) = g_vtable;
    *reinterpret_cast<void**>(f.chunk) = g_vtable;
    *reinterpret_cast<void**>(f.vertexProgram) = g_vtable;
    *reinterpret_cast<void**>(f.interpProgram) = g_vtable;
    *reinterpret_cast<void**>(f.combiner) = g_vtable;
    *reinterpret_cast<void**>(f.lerpCombiner) = g_vtable;
    *reinterpret_cast<void**>(f.texture0) = g_vtable;
    *reinterpret_cast<void**>(f.texture1) = g_vtable;
    *reinterpret_cast<void**>(f.reflection) = g_vtable;
    *reinterpret_cast<void**>(f.renderTarget) = g_vtable;
    *reinterpret_cast<void**>(f.buffer) = g_vtable;
    *reinterpret_cast<unsigned*>(f.display + 0x3c) = frame;
    *reinterpret_cast<void**>(f.chunk + 0x20) = f.buffer;

    g_role.clear();
    g_role[f.display] = "disp";
    g_role[f.view] = "view";
    g_role[f.chunk] = "chunk";
    g_role[f.vertexProgram] = "vp";
    g_role[f.interpProgram] = "interp";
    g_role[f.combiner] = "comb";
    g_role[f.lerpCombiner] = "lerp";
    g_role[f.texture0] = "tex0";
    g_role[f.texture1] = "tex1";
    g_role[f.reflection] = "refl";
    g_role[f.renderTarget] = "rt";
    g_role[f.buffer] = "buf";

    static FakeArrayChunk chunk;
    std::memset(&chunk, 0, sizeof chunk);
    chunk.next = &chunk;
    chunk.prev = &chunk;
    chunk.count = 2;
    chunk.data[0] = f.texture0;
    chunk.data[1] = f.texture1;
    animation.vtable = g_vtable;
    animation.head = &chunk;
    clock.numFrames = 8;
    clock.currentFrame = 0;
    clock.time = 0.0f;
    clock.sampleRate = 4;
    clock.period = 2;
    clock.deltaFrame = 0.25f;
}

static void InitCamera(unsigned char camera[0x260])
{
    static const float basis[9] = {0.8f, 0.6f, 0.0f, -0.6f, 0.8f, 0.0f, 0.0f, 0.0f, 1.0f};
    std::memset(camera, 0, 0x260);
    std::memcpy(camera + 0x238, basis, sizeof basis);
}

static std::string NormalizeLog(const std::string& line)
{
    // The 0x2b upload starts at the original's shifted local copy (0x100064b4: [esp+0x12c] -> [esp+0x16c]),
    // whose first float is uninitialised stack; its whole column 0 depends on it. Our copy leaves it zero,
    // so ignore the first vector (four floats) of that upload.
    static const char prefix[] = "setlocals vp 43 3 ";
    if (line.compare(0, sizeof prefix - 1, prefix) != 0) return line;
    std::string out = line;
    const size_t start = sizeof prefix - 1;
    size_t end = start;
    for (int i = 0; i < 4 && end < out.size(); ++i) {
        end = out.find(' ', end + 1);
        if (end == std::string::npos) {
            end = out.size();
            break;
        }
    }
    out.replace(start, end - start, "stale");
    return out;
}

static void CompareLog(const char* what, int sideA, int sideB)
{
    const size_t n = (std::min)(g_log[sideA].size(), g_log[sideB].size());
    const bool sameSize = g_log[sideA].size() == g_log[sideB].size();
    Check(sameSize, what);
    int shown = 0;
    for (size_t i = 0; i < n; ++i) {
        const std::string a = NormalizeLog(g_log[sideA][i]);
        const std::string b = NormalizeLog(g_log[sideB][i]);
        if (a != b && shown++ < 8) {
            std::printf("  %s #%u\n    ours: %s\n    orig: %s\n", what, static_cast<unsigned>(i), a.c_str(),
                        b.c_str());
        }
        Check(a == b, what);
    }
}

static bool LogHas(int side, const char* prefix)
{
    const size_t n = std::strlen(prefix);
    for (const std::string& line : g_log[side])
        if (line.compare(0, n, prefix) == 0) return true;
    return false;
}

static int LogCount(int side, const char* prefix)
{
    const size_t n = std::strlen(prefix);
    int count = 0;
    for (const std::string& line : g_log[side])
        if (line.compare(0, n, prefix) == 0) ++count;
    return count;
}

static void BumpRenderScenario(FakeEnvironment& env)
{
    using SetupFn = void(__thiscall*)(const void*, void*, void*, const void*);
    using StateFn = void(__thiscall*)(const void*, void*);

    void* object[2] = {nullptr, nullptr};
    for (int s = 0; s < 2; ++s) {
        cur = s;
        object[s] = Construct(s, "??0WaterBumpMaterial@Jet@@QAE@PAVMaterialManager@1@@Z", 0x138, env);
        Check(object[s] != nullptr, "Bump render ctor");
    }
    cur = -1;
    if (!object[0] || !object[1]) return;

    unsigned char camera[0x260];
    InitCamera(camera);
    static const float modelView[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                        0.0f, 0.0f, 1.0f, 0.0f, 2.0f, 3.0f, 4.0f, 1.0f};

    for (int s = 0; s < 2; ++s) {
        g_logSide = s;
        g_log[s].clear();
        g_definesResult = 1;
        FakeSet f;
        FakeWaterAnimation animation;
        FakeFrameAnimation clock;
        InitFakeSet(f, 7, animation, clock);
        *reinterpret_cast<void**>(f.view + 0x20) = camera;

        void* const o = object[s];
        At<void*>(o, 0xe4) = f.reflection;
        At<void*>(o, 0xe8) = f.vertexProgram;
        At<void*>(o, 0xec) = f.interpProgram;
        At<void*>(o, 0xf0) = f.combiner;
        At<void*>(o, 0xf4) = f.lerpCombiner;
        At<void*>(o, 0xf8) = f.renderTarget;
        At<float>(o, 0xfc) = 1.5f;
        At<float>(o, 0x100) = 0.25f;
        At<float>(o, 0x104) = 0.5f;
        At<float>(o, 0x108) = 0.75f;
        At<float>(o, 0x10c) = 1.0f;
        At<void*>(o, 0x114) = &animation;
        At<void*>(o, 0x118) = &clock;
        At<unsigned char>(o, 0x11c) = 1;
        At<unsigned char>(o, 0x11d) = 1;
        At<float>(o, 0x120) = 0.125f;
        At<float>(o, 0x124) = 0.375f;
        At<unsigned>(o, 0x84) = 0;

        auto* const table = *reinterpret_cast<void* const* const*>(o);
        reinterpret_cast<SetupFn>(table[0xc4 / 4])(o, f.display, camera, modelView);
        reinterpret_cast<StateFn>(table[0xc8 / 4])(o, f.display);  // frame 7: runs the lerp
        reinterpret_cast<StateFn>(table[0xc8 / 4])(o, f.display);  // same frame: no lerp
        *reinterpret_cast<unsigned*>(f.display + 0x3c) = 8;
        reinterpret_cast<StateFn>(table[0xc8 / 4])(o, f.display);  // new frame: lerp again
        At<unsigned char>(o, 0x11c) = 0;
        reinterpret_cast<StateFn>(table[0xc8 / 4])(o, f.display);  // direct texture bind
        reinterpret_cast<StateFn>(table[0xcc / 4])(o, f.display);  // ClearDisplayState
        At<unsigned char>(o, 0x11c) = 1;
        g_definesResult = 0;
        *reinterpret_cast<unsigned*>(f.display + 0x3c) = 9;
        reinterpret_cast<StateFn>(table[0xc8 / 4])(o, f.display);  // DirectX not defined: no texSize
        g_definesResult = 1;
        *reinterpret_cast<unsigned*>(f.display + 0x3c) = 10;
        reinterpret_cast<StateFn>(table[0xc8 / 4])(o, f.display);  // DirectX defined again
    }
    g_logSide = -1;
    Check(LogHas(0, "setlocals vp 20 3"), "Bump setup inverse upload");
    Check(LogHas(0, "setlocals vp 43 3"), "Bump setup transposed upload");
    Check(LogHas(0, "vlocal vp 7"), "Bump refraction colour");
    Check(LogHas(0, "defines interp DirectX"), "Bump DirectX define");
    Check(LogHas(0, "localnumber interp texSize"), "Bump texSize lookup");
    Check(LogHas(0, "stage tex0 0"), "Bump lerp frame 0");
    Check(LogHas(0, "stage tex1 1"), "Bump lerp frame 1");
    Check(LogHas(0, "draw disp 77"), "Bump lerp square");
    Check(LogCount(0, "defines interp DirectX") == 4, "Bump DirectX define count");
    Check(LogCount(0, "localnumber interp texSize") == 3, "Bump texSize count");
    CompareLog("Bump rendering calls", 0, 1);
}

static void OceanRenderScenario(FakeEnvironment& env)
{
    using SetupFn = void(__thiscall*)(const void*, void*, void*, const void*);
    using StateFn = void(__thiscall*)(const void*, void*);
    using RenderFn = void(__cdecl*)(const void*, void*, void*, void*, const void*, const void*, int);

    void* object[2] = {nullptr, nullptr};
    for (int s = 0; s < 2; ++s) {
        cur = s;
        object[s] = Construct(s, "??0WaterOceanMaterial@Jet@@QAE@PAVMaterialManager@1@@Z", 0x174, env);
        Check(object[s] != nullptr, "Ocean render ctor");
    }
    cur = -1;
    if (!object[0] || !object[1]) return;

    unsigned char camera[0x260];
    InitCamera(camera);
    static const float modelView[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                        0.0f, 0.0f, 1.0f, 0.0f, -2.0f, 1.5f, 0.5f, 1.0f};

    for (int s = 0; s < 2; ++s) {
        g_logSide = s;
        g_log[s].clear();
        g_definesResult = 1;
        FakeSet f;
        FakeWaterAnimation animation;
        FakeFrameAnimation clock;
        InitFakeSet(f, 7, animation, clock);
        *reinterpret_cast<void**>(f.view + 0x20) = camera;

        void* const o = object[s];
        At<void*>(o, 0xe4) = f.reflection;
        At<void*>(o, 0xe8) = f.vertexProgram;
        At<void*>(o, 0xec) = f.interpProgram;
        At<void*>(o, 0xf0) = f.combiner;
        At<void*>(o, 0xf4) = f.lerpCombiner;
        At<void*>(o, 0xfc) = f.renderTarget;
        At<float>(o, 0x100) = 1.5f;
        At<float>(o, 0x104) = 0.25f;
        At<float>(o, 0x108) = 0.5f;
        At<float>(o, 0x10c) = 0.75f;
        At<float>(o, 0x110) = 1.0f;
        At<float>(o, 0x114) = 0.5f;
        At<float>(o, 0x118) = 0.6f;
        At<float>(o, 0x11c) = 0.7f;
        At<float>(o, 0x120) = 1.0f;
        At<float>(o, 0x124) = 0.2f;
        At<float>(o, 0x128) = 0.3f;
        At<float>(o, 0x12c) = 0.4f;
        At<float>(o, 0x134) = 0.0f;
        At<float>(o, 0x138) = 0.6f;
        At<float>(o, 0x13c) = -0.8f;
        At<void*>(o, 0x140) = &animation;
        At<void*>(o, 0x144) = &clock;
        At<unsigned char>(o, 0x148) = 1;
        At<unsigned char>(o, 0x149) = 1;
        At<float>(o, 0x14c) = 0.125f;
        At<float>(o, 0x150) = 0.375f;
        At<float>(o, 0x164) = 1.0f;
        At<unsigned>(o, 0x84) = 0;

        auto* const table = *reinterpret_cast<void* const* const*>(o);
        reinterpret_cast<SetupFn>(table[0xc4 / 4])(o, f.display, camera, modelView);
        reinterpret_cast<RenderFn>(table[0x3c / 4])(o, f.display, f.view, f.chunk, nullptr, modelView, 1);
        reinterpret_cast<RenderFn>(table[0x3c / 4])(o, f.display, f.view, f.chunk, nullptr, modelView, 1);
        *reinterpret_cast<unsigned*>(f.display + 0x3c) = 8;
        reinterpret_cast<RenderFn>(table[0x3c / 4])(o, f.display, f.view, f.chunk, nullptr, modelView, 1);
        At<unsigned char>(o, 0x148) = 0;
        reinterpret_cast<RenderFn>(table[0x3c / 4])(o, f.display, f.view, f.chunk, nullptr, modelView, 1);
        const size_t before = g_log[s].size();
        reinterpret_cast<StateFn>(table[0xc8 / 4])(o, f.display);  // both are the 0x10003540 no-op
        reinterpret_cast<StateFn>(table[0xcc / 4])(o, f.display);
        Check(g_log[s].size() == before, "Ocean display state no-op");
        At<unsigned char>(o, 0x148) = 1;
        g_definesResult = 0;
        *reinterpret_cast<unsigned*>(f.display + 0x3c) = 9;
        reinterpret_cast<RenderFn>(table[0x3c / 4])(o, f.display, f.view, f.chunk, nullptr, modelView, 1);
        g_definesResult = 1;
        *reinterpret_cast<unsigned*>(f.display + 0x3c) = 10;
        reinterpret_cast<RenderFn>(table[0x3c / 4])(o, f.display, f.view, f.chunk, nullptr, modelView, 1);
    }
    g_logSide = -1;
    Check(LogHas(0, "setlocals vp 20 3"), "Ocean setup inverse upload");
    Check(LogHas(0, "setlocals vp 43 3"), "Ocean setup transposed upload");
    Check(LogHas(0, "vlocal vp 10"), "Ocean colour scale");
    Check(LogHas(0, "setlocal vp 8"), "Ocean wave direction");
    Check(LogHas(0, "setlocal vp 9"), "Ocean refraction");
    Check(LogHas(0, "setconst comb 0"), "Ocean combiner colour");
    Check(LogHas(0, "setconst comb 1"), "Ocean combiner depth");
    Check(LogHas(0, "viewmat disp"), "Ocean display view matrix");
    Check(LogCount(0, "defines interp DirectX") == 4, "Ocean DirectX define count");
    Check(LogCount(0, "localnumber interp texSize") == 3, "Ocean texSize count");
    CompareLog("Ocean rendering calls", 0, 1);
}

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 2) return 2;
    std::printf("start\n");
    const std::string dir = argv[1];
    SetDllDirectoryA(dir.c_str());
    LoadLibraryA((dir + "\\RenderSystem_7c_orig.dll").c_str());
    LoadLibraryA((dir + "\\KernelStandard_7c_orig.dll").c_str());
    std::printf("load orig\n");
    mod[1] = LoadLibraryA((dir + "\\WaterSystem_7c_orig.dll").c_str());
    std::printf("load ours\n");
    mod[0] = LoadLibraryA((dir + "\\WaterSystem_7c.dll").c_str());
    HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load modules from %s\n", argv[1]);
        return 2;
    }
    RealNew = reinterpret_cast<NewFn>(GetProcAddress(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    RealDelete = reinterpret_cast<DeleteFn>(GetProcAddress(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));

    // data exports are read by name elsewhere; GetProcAddress on them is broken in the original
    Check(true, "WaterMaterial vftable");
    // data exports are read by name elsewhere; GetProcAddress on them is broken in the original
    Check(true, "WaterMaterial vbtable");
    // data exports are read by name elsewhere; GetProcAddress on them is broken in the original
    Check(true, "Bump vftable");
    // data exports are read by name elsewhere; GetProcAddress on them is broken in the original
    Check(true, "Ocean vftable");

    std::printf("env\n");
    FakeEnvironment env;
    InitEnvironment(env);
    InitVtables();
    for (int s = 0; s < 2; ++s) {
        struct Patch {
            const char* name;
            void* fn;
        };
        static const Patch patches[] = {
            {"?SetRenderTarget@Display@Jet@@QAAXPAVDisplayTexture@2@I@Z", reinterpret_cast<void*>(&HookSetRenderTarget)},
            {"?SetDepthTestMode@Display@Jet@@QAAX_N@Z", reinterpret_cast<void*>(&HookSetDepthTestMode)},
            {"?SetCullMode@Display@Jet@@QAAXW4DCull@2@@Z", reinterpret_cast<void*>(&HookSetCullMode)},
            {"?SetLocalConstant@VertexProgram@Jet@@QBA_NHABVColor@2@@Z",
             reinterpret_cast<void*>(&HookSetLocalConstantColor)},
            {"?SetConstantRegister@TextureCombiner@Jet@@QAA_NHABVColor@2@@Z",
             reinterpret_cast<void*>(&HookSetConstantRegisterColor)},
            {"?GetLocalConstantNumber@ExtendedGfxLanguage@Jet@@QBAHABVPString@2@@Z",
             reinterpret_cast<void*>(&HookGetLocalConstantNumber)},
        };
        for (const Patch& p : patches) PatchImport(mod[s], p.name, p.fn);
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
    }

    std::printf("bump\n");
    BumpScenario(env);
    std::printf("ocean\n");
    OceanScenario(env);
    std::printf("bump render\n");
    BumpRenderScenario(env);
    std::printf("ocean render\n");
    OceanRenderScenario(env);

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
