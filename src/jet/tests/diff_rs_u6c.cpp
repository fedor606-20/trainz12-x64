// Differential test of RenderSystem_7c unit U6c1: DisplayTexture and DisplayTextureNull (DisplayTexture.cpp), ours
// against RenderSystem_7c_orig.dll in one process.
//
// As in diff_rs_u6: both modules' imports of Jet::Alloc::New / Delete are hooked (blocks filled with 0xcd and
// numbered, frees logged by name), each scenario runs the same random script on both sides with that side's
// exports and the logs must be equal. A stand-in Display logs its slots (caps query 0x1c, maximum anisotropy 0x28,
// CreateTexture 0x98 making a null texture of the same side, SetTexture 0xa0). The null display's format table
// (the original's 0x100b8228, used by both sides until U9) is filled per scenario: formats supported as they are,
// compressed ones, and formats converted (through the original's Convert) to the one they link to. The textures'
// bytes, the 1000 texture units (the original's statics) and every getter are logged after each step.
//
// usage: diff_rs_u6c.exe <bin dir> [rounds]
//   env RS_U6C_ECHO (echo the log), RS_U6C_DUMP=<prefix> (both logs of the first mismatch)
#define NOMINMAX
#include <windows.h>
#include <intrin.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

static HMODULE mod[2];  // 0 ours, 1 the original
static HMODULE ks;
static unsigned long long checks, bad;
static std::mt19937 rng(6603);

static void* Proc(HMODULE m, const char* name)
{
    void* p = reinterpret_cast<void*>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}
template <class T>
static T As(void* p) { return reinterpret_cast<T>(p); }

using NewFn = void*(__cdecl*)(unsigned);
using DeleteFn = void(__cdecl*)(void*);
static NewFn RealNew;
static DeleteFn RealDelete;

static int cur = -1;
static std::string logText[2];
static std::map<const void*, std::string> names[2];
static std::map<const void*, unsigned> blockSize[2];
static int allocCount[2];

static void Log(const char* fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (cur >= 0) logText[cur] += buf;
    static const bool echo = std::getenv("RS_U6C_ECHO") != nullptr;
    if (echo) std::fputs(buf, stdout);
}

static std::map<const void*, std::string> fixedNames;
static std::string Name(const void* p)
{
    if (!p) return "null";
    auto f = fixedNames.find(p);
    if (f != fixedNames.end()) return f->second;
    if (cur < 0) return "?";
    auto it = names[cur].find(p);
    if (it != names[cur].end()) return it->second;
    auto b = blockSize[cur].upper_bound(p);
    if (b != blockSize[cur].begin()) {
        --b;
        const auto off = static_cast<const char*>(p) - static_cast<const char*>(b->first);
        if (off >= 0 && static_cast<unsigned>(off) <= b->second) {
            char t[64];
            std::snprintf(t, sizeof t, "%s+0x%x", names[cur][b->first].c_str(), static_cast<unsigned>(off));
            return t;
        }
    }
    return "?";
}

// the original's Convert (0x1000e600..0x1000ef00) keeps a scratch buffer of its own across calls, shared by the
// two sides: its allocations are not logged
static bool FromConvert(void* ret)
{
    const auto o = reinterpret_cast<uintptr_t>(ret) - reinterpret_cast<uintptr_t>(mod[1]);
    return o >= 0xe600 && o < 0xef00;
}

static void* __cdecl HookNew(unsigned n)
{
    void* p = RealNew(n);
    if (FromConvert(_ReturnAddress())) return p;
    if (p && n < 0x4000000) std::memset(p, 0xcd, n);
    if (cur >= 0 && p) {
        blockSize[cur][p] = n;
        char b[32];
        std::snprintf(b, sizeof b, "A%d", allocCount[cur]++);
        names[cur][p] = b;
        Log("  new %u -> %s\n", n, b);
    }
    return p;
}
static void __cdecl HookDelete(void* p)
{
    if (cur >= 0 && p && !FromConvert(_ReturnAddress())) {
        Log("  free %s\n", Name(p).c_str());
        names[cur].erase(p);
        blockSize[cur].erase(p);
    }
    RealDelete(p);
}

static void PatchImport(HMODULE m, const char* name, void* fn)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        auto* nm = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; nm->u1.AddressOfData; ++nm, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(nm->u1.Ordinal)) continue;
            if (std::strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + nm->u1.AddressOfData)->Name, name)) continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            iat->u1.Function = reinterpret_cast<uintptr_t>(fn);
            VirtualProtect(&iat->u1.Function, sizeof(void*), old, &old);
        }
    }
}

static void* TestNew(unsigned n, const char* fmt, int k)
{
    void* p = RealNew(n);
    std::memset(p, 0, n);
    char b[32];
    std::snprintf(b, sizeof b, fmt, k);
    names[cur][p] = b;
    blockSize[cur][p] = n;
    return p;
}

static std::string Hex(const void* p, unsigned n)
{
    std::string s;
    char b[4];
    for (unsigned i = 0; i < n; ++i) {
        std::snprintf(b, sizeof b, "%02x", static_cast<const unsigned char*>(p)[i]);
        s += b;
    }
    return s;
}
static unsigned long long Fnv(const void* p, size_t n)
{
    unsigned long long h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 1099511628211ull;
    return h;
}
static unsigned FloatBits(float f)
{
    unsigned u;
    std::memcpy(&u, &f, 4);
    return u;
}
template <class T>
static T& At(const void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(const_cast<void*>(p)) + off); }
template <class R, class... T>
static R SC(const void* c, int slot, T... args)
{
    using Fn = R(__cdecl*)(const void*, T...);
    return (*static_cast<Fn* const*>(c))[slot](c, args...);
}

// ---- per-side entry points

#define DT "DisplayTexture@Jet@@"
#define DN "DisplayTextureNull@Jet@@"

struct Api {
    void*(__fastcall* ctor)(void*, void*, void* display);
    void*(__fastcall* copy)(void*, void*, const void*);
    void*(__fastcall* assign)(void*, void*, const void*);
    void*(__fastcall* baseCopy)(void*, void*, const void*);
    bool(__cdecl* subLoadAll)(void*, const void* bitmap, const int* rect, void* format, unsigned face);
    void(__cdecl* initInstance)(const void*, void*);
    int(__cdecl* numMips)(const int*);
    int*(__cdecl* levelSize)(int*, const int*, unsigned);
    void(__cdecl* setActive)(void*, unsigned);
    unsigned(__cdecl* getActive)(const void*);
    int(__cdecl* target)(const void*);
    void(__cdecl* setLost)(void*, bool);
    bool(__cdecl* canMip)(const void*);
    bool(__cdecl* autoMip)(const void*);
    int(__cdecl* autoMipHint)(const void*);
    float(__cdecl* aniso)(const void*);
    bool(__cdecl* checkInit)(const void*, const char*);
    unsigned(__cdecl* getU[4])(const void*);  // depth levels, mips, memory, dimension
    const void*(__cdecl* getDisplay)(const void*);
    const int*(__cdecl* getSize)(const void*);
    const void*(__cdecl* stats)();
    void*(__cdecl* checkFormat)(void*, void*, void**, int*, int*, int*, int, int);
    void(__cdecl* bindTexture)(const void*, unsigned);
    void(__cdecl* unbindRender)(const void*, unsigned);
    const void*(__cdecl* textureBitmap)(const void*, int, unsigned, void*, int);
    const void*(__cdecl* renderContext)(const void*);
    const void*(__cdecl* winInterface)(const void*);
};
static Api api[2];

static void LoadApi(int s)
{
    Api& a = api[s];
    HMODULE m = mod[s];
#define A(field, name) a.field = As<decltype(a.field)>(Proc(m, name))
    A(ctor, "??0" DN "QAE@PAVDisplay@1@@Z");
    A(copy, "??0" DN "QAE@ABV01@@Z");
    A(assign, "??4" DN "QAEAAV01@ABV01@@Z");
    A(baseCopy, "??0" DT "QAE@ABV01@@Z");
    A(subLoadAll, "?SubLoad@" DT "QAA_NABVBitmap@2@ABURect@2@AAVDisplayFormat@2@I@Z");
    A(initInstance, "?InitInstance@" DT "IBAXAAV12@@Z");
    A(numMips, "?GetNumMipLevels@" DT "SAHABUSize@2@@Z");
    A(levelSize, "?GetMipLevelSize@" DT "SA?AUSize@2@ABU32@I@Z");
    A(setActive, "?SetActiveRenderLevel@" DT "QAAXI@Z");
    A(getActive, "?GetActiveRenderLevel@" DT "QBAIXZ");
    A(target, "?GetRenderTargetType@" DT "QBA?AW4TextureTarget@2@XZ");
    A(setLost, "?SetDataLost@" DT "QAAX_N@Z");
    A(canMip, "?GetCanMip@" DT "QBA_NXZ");
    A(autoMip, "?GetAutoMip@" DT "QBA_NXZ");
    A(autoMipHint, "?GetAutoMipHint@" DT "QBA?AW4TextureAutoMip@2@XZ");
    A(aniso, "?GetAnisotropy@" DT "QBAMXZ");
    A(checkInit, "?CheckInitialized@" DT "IBA_NPBD@Z");
    A(getU[0], "?GetDepthLevels@" DT "QBAIXZ");
    A(getU[1], "?GetNumMipLevels@" DT "QBAIXZ");
    A(getU[2], "?GetMemorySize@" DT "QBAIXZ");
    A(getU[3], "?GetDimension@" DT "QBA?AW4TextureDimension@2@XZ");
    A(getDisplay, "?GetDisplay@" DT "QBAPAVDisplay@2@XZ");
    A(getSize, "?GetSize@" DT "QBAABUSize@2@XZ");
    A(stats, "?GetTextureStats@" DT "SAABUDisplayTextureStats@12@XZ");
    A(checkFormat, "?CheckFormat@" DN "IAAPAVNULLFormat@2@AAVDisplayFormat@2@AAPAXAAH22HH@Z");
    A(bindTexture, "?BindTexture@" DN "IBAXI@Z");
    A(unbindRender, "?UnbindRenderTexture@" DN "IBAXI@Z");
    A(textureBitmap, "?GetTextureBitmap@" DN "IBAPAVBitmap@2@HIAAVDisplayFormat@2@W4DFormatId@2@@Z");
    A(renderContext, "?GetRenderContext@" DN "QBAPAUPBufferContext@2@XZ");
    A(winInterface, "?GetWinInterface@" DN "IBAPAVOPENGL_INTERFACE_CLASS@2@XZ");
#undef A
}

// the original's statics and helpers both sides use
static const void*** bound;
static unsigned* numObjects;
static unsigned char* nullTable;  // 29 x 36 bytes
static void*(__fastcall* formatCtor)(void*, void*, int id);
static void*(__fastcall* rtcCtor)(void*, void*, int target, bool, bool, bool, const void* shared);
static void(__fastcall* rtcDtor)(void*, void*);
static void*(__fastcall* bitmapCtor)(void*, void*, const int* size, int format, unsigned char* data);
static void(__fastcall* bitmapDtor)(void*, void*);

static std::string BitmapText(const void* p)
{
    if (!p) return "null";
    const int* b = static_cast<const int*>(p);
    // (the row padding is not hashed: KernelStandard leaves a copy's padding uninitialised)
    unsigned long long h = 0;
    if (b[3] && b[1] > 0) {
        const unsigned char* d = reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(b[3]));
        h = 1469598103934665603ull;
        for (int y = 0; y < b[1]; ++y) h ^= Fnv(d + static_cast<size_t>(b[4]) * y, static_cast<size_t>(b[0]) * b[5]) + y;
    }
    char t[192];
    std::snprintf(t, sizeof t, "%s %dx%d pitch %d format %d %016llx", Name(p).c_str(), b[0], b[1], b[4], b[6], h);
    return t;
}

// ---- the stand-in display

static std::mt19937 standRng;
static void* displayVt[0x60];
static unsigned char displayObj[0x40];
static float maxAniso;
static int made;
static int __cdecl DisplaySlot(void*) { Log("  display slot\n"); return 0; }
static bool __cdecl DisplayQuery(void*, int what)
{
    const bool r = standRng() % 3 != 0;
    Log("  display query %d: %d\n", what, r);
    return r;
}
static float __cdecl DisplayMaxAniso(void*)
{
    Log("  display max anisotropy\n");
    return maxAniso;
}
static void* __cdecl DisplayCreateTexture(void* d)
{
    void* t = TestNew(0x70, "T%d", made++);
    Log("  display create texture %s\n", Name(t).c_str());
    api[cur].ctor(t, nullptr, d);
    return t;
}
static void __cdecl DisplaySetTexture(void*, unsigned unit) { Log("  display set texture %u\n", unit); }

// ---- the null display's format table

static const int kIds[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28};
static int bitsOf[29];

static void FillTable(std::mt19937& r)
{
    std::memset(nullTable, 0, 29 * 36);
    for (int id = 0; id < 29; ++id) {
        unsigned char* e = nullTable + id * 36;
        const bool compressed = id == 10 || id == 11 || id == 12 || (id >= 0x19 && id <= 0x1c);
        unsigned flags = r() % 4 ? 1u : 0u;
        if (compressed) flags |= 2;
        int link = 3;  // XRGB
        if (!(flags & 1)) {
            // (Create follows the link of an unsupported format without a check, in both modules: never null)
            link = r() % 3 ? (r() % 2 ? 2 : 3) : 1;
            if (link) {
                unsigned char* l = nullTable + link * 36;
                *reinterpret_cast<unsigned*>(l) |= 1;  // the link target is supported
            }
        }
        *reinterpret_cast<unsigned*>(e) = flags;
        *reinterpret_cast<int*>(e + 0x10) = compressed ? (r() % 2 ? 4 : 8) * 16 : 0;
        *reinterpret_cast<void**>(e + 0x14) = link ? nullTable + link * 36 : nullptr;
        formatCtor(e + 0x18, nullptr, link ? link : id);
    }
    // the id 0 (unknown) entry is supported (a format Classify cannot name is looked up there)
    *reinterpret_cast<unsigned*>(nullTable) |= 1;
}

// ---- the dumps

static void DumpTex(const char* tag, const void* t)
{
    const unsigned char* b = static_cast<const unsigned char*>(t);
    Log("  %s %s: state %d mem %u display %s size %dx%d dim %d depth %u mips %u aniso %08x %08x flags %d "
        "automip %d construct %s [%s] null %s dirty %d %d binds %d f68 %d image %s\n",
        tag, Name(t).c_str(), At<int>(t, 4), At<unsigned>(t, 8), Name(At<void*>(t, 0xc)).c_str(), At<int>(t, 0x10), At<int>(t, 0x14),
        At<int>(t, 0x18), At<unsigned>(t, 0x1c), At<unsigned>(t, 0x20), At<unsigned>(t, 0x24), At<unsigned>(t, 0x28),
        b[0x2c], static_cast<signed char>(b[0x30]), Name(At<void*>(t, 0x34)).c_str(),
        At<void*>(t, 0x34) ? Hex(static_cast<char*>(At<void*>(t, 0x34)) + 4, 12).c_str() : "",
        Hex(b + 0x38, 0x24).c_str(), b[0x60], b[0x61], At<int>(t, 0x64), b[0x68], BitmapText(At<void*>(t, 0x6c)).c_str());
}

static void DumpGetters(const Api& a, const void* t)
{
    const int* sz = a.getSize(t);
    Log("  getters %s: size %dx%d depth %u mips %u mem %u dim %u display %s canmip %d automip %d hint %d aniso %08x "
        "init %d target %d active %u lost %d context %s win %s stats %s create rc %d jet %d api %s\n",
        Name(t).c_str(), sz[0], sz[1], a.getU[0](t), a.getU[1](t), a.getU[2](t), a.getU[3](t),
        Name(a.getDisplay(t)).c_str(), a.canMip(t), a.autoMip(t), a.autoMipHint(t), FloatBits(a.aniso(t)),
        a.checkInit(t, "x"), a.target(t), a.getActive(t), SC<bool>(t, 12), Name(a.renderContext(t)).c_str(),
        Name(a.winInterface(t)).c_str(), Name(a.stats()).c_str(), SC<bool>(t, 6),
        SC<int>(t, 13), Name(SC<void*>(t, 16)).c_str());
}

static void DumpBound()
{
    std::string t;
    if (*bound)
        for (int i = 0; i < 1000; ++i)
            if ((*bound)[i]) t += " " + std::to_string(i) + "=" + Name((*bound)[i]);
    Log("  units%s (objects %u)\n", t.c_str(), *numObjects);
}

static void DestroyTex(void* t)
{
    const auto f = reinterpret_cast<void(__fastcall*)(void*, void*, int)>((*static_cast<void***>(t))[0]);
    f(t, nullptr, 1);
}

// a made-up bitmap of a 32-bit, 24-bit or 16-bit format
static void* MakeBitmap(std::mt19937& r)
{
    static const int formats[] = {2, 3, 2, 3, 1, 4};
    const int size[2] = {1 + static_cast<int>(r() % 40), 1 + static_cast<int>(r() % 40)};
    void* b = TestNew(0x44, "BMP%d", made++);
    bitmapCtor(b, nullptr, size, formats[r() % 6], nullptr);
    const int* f = static_cast<const int*>(b);
    unsigned char* data = reinterpret_cast<unsigned char*>(static_cast<uintptr_t>(f[3]));
    const unsigned n = static_cast<unsigned>(f[4]) * static_cast<unsigned>(f[1]);
    for (unsigned i = 0; i < n; ++i) data[i] = static_cast<unsigned char>(r());
    return b;
}
static void FreeBitmap(const void* b)
{
    if (!b) return;
    bitmapDtor(const_cast<void*>(b), nullptr);
    HookDelete(const_cast<void*>(b));
}

// a display format: a named one of at least 8 bits (the null texture divides the pitch by bits / 8), sometimes a
// custom layout for Classify
static void MakeFormat(std::mt19937& r, unsigned char* f)
{
    for (;;) {
        const int id = kIds[r() % 28];
        formatCtor(f, nullptr, id);
        if (f[1] >= 8) break;
    }
    if (r() % 6 == 0) {
        f[0] = 0;  // the layout stays, Classify names it again
        if (r() % 3 == 0) f[3] ^= 1;
    }
}

// ---- the scenario

static void RunNull(int s, unsigned seed)
{
    std::mt19937 r(seed);
    standRng.seed(seed ^ 0x6c6c);
    const Api& a = api[s];
    made = 0;
    maxAniso = static_cast<float>(1 << (r() % 5));
    FillTable(r);
    Log("statics: max aniso %08x\n", FloatBits(maxAniso));
    std::vector<void*> tex;
    std::vector<void*> bitmaps;
    auto newTex = [&]() {
        void* t = TestNew(0x70, "T%d", made++);
        Log("op new %s\n", Name(t).c_str());
        a.ctor(t, nullptr, displayObj);
        DumpTex("made", t);
        DumpBound();
        tex.push_back(t);
        return t;
    };
    newTex();
    for (int step = 0, steps = 8 + static_cast<int>(r() % 20); step < steps; ++step) {
        void* t = tex[r() % tex.size()];
        const int dim = At<int>(t, 0x18);
        const unsigned op = r() % 22;
        if (std::getenv("RS_U6C_ECHO")) Log("step %u\n", op);
        switch (op) {
        case 0:
            newTex();
            break;
        case 1: case 2: case 3: {  // Create
            unsigned char f[12];
            MakeFormat(r, f);
            const int size[2] = {static_cast<int>(r() % 300), static_cast<int>(r() % 300)};
            const int d = r() % 5 ? static_cast<int>(r() % 4) : 0;
            const unsigned mips = r() % 10;
            const int automip = static_cast<int>(r() % 4), hint = static_cast<int>(r() % 3);
            const unsigned depth = r() % 8;
            static const float anis[] = {1.0f, 2.0f, 16.0f, 0.5f, -1.0f, 3.0f, 0.0f};
            float an = anis[r() % 7];
            if (r() % 5 == 0) an = *static_cast<float*>(Proc(mod[1], "?ANISOTROPY_DEFAULT_TOKEN_VALUE@TextureResource@Jet@@2MA"));
            const int filters[3] = {static_cast<int>(r() % 3), static_cast<int>(r() % 3), static_cast<int>(r() % 3)};
            const int wraps[2] = {static_cast<int>(r() % 3), static_cast<int>(r() % 3)};
            unsigned char rtc[0x10];
            rtcCtor(rtc, nullptr, r() % 4 == 0 ? 1 + static_cast<int>(r() % 3) : 0, r() % 2 != 0, r() % 2 != 0, false, nullptr);
            using CreateFn = bool(__cdecl*)(void*, const int*, const void*, int, unsigned, int, unsigned, float, int,
                                           int, int, int, int, int, const void*);
            const bool ok = (*static_cast<CreateFn* const*>(t))[1](t, size, f, d, mips, automip, depth, an, hint,
                                                                  filters[0], filters[1], filters[2], wraps[0], wraps[1], rtc);
            rtcDtor(rtc, nullptr);
            Log("op create %s %dx%d format %s dim %d mips %u automip %d depth %u aniso %08x hint %d: %d\n",
                Name(t).c_str(), size[0], size[1], Hex(f, 12).c_str(), d, mips, automip, depth, FloatBits(an), hint, ok);
            DumpTex("created", t);
            break;
        }
        case 4: case 5: {  // Load
            unsigned char f[12];
            MakeFormat(r, f);
            void* b = r() % 8 ? MakeBitmap(r) : nullptr;
            if (b) bitmaps.push_back(b);
            const unsigned level = r() % 6, face = r() % 7;
            using LoadFn = bool(__cdecl*)(void*, unsigned, void*, const void*, unsigned);
            const bool ok = (*static_cast<LoadFn* const*>(t))[3](t, level, f, b, face);
            Log("op load %s %u %s %s face %u: %d format now %s\n", Name(t).c_str(), level, BitmapText(b).c_str(),
                Hex(f, 2).c_str(), face, ok, Hex(f, 12).c_str());
            DumpTex("loaded", t);
            break;
        }
        case 6: case 7: {  // SubLoad (the slot, or the non-virtual one that walks the levels)
            if (r() % 4 == 0) break;
            unsigned char f[12];
            MakeFormat(r, f);
            void* b = MakeBitmap(r);
            bitmaps.push_back(b);
            const int rect[4] = {static_cast<int>(r() % 20), static_cast<int>(r() % 20), 1 + static_cast<int>(r() % 64),
                                 1 + static_cast<int>(r() % 64)};
            bool ok;
            if (r() % 2) {
                const unsigned level = r() % 6;
                using SubFn = bool(__cdecl*)(void*, unsigned, const int*, void*, const void*, unsigned);
                ok = (*static_cast<SubFn* const*>(t))[4](t, level, rect, f, b, 0);
                Log("op subload %s level %u", Name(t).c_str(), level);
            } else {
                // (the smaller levels come from Bitmap::Scale, which only halves powers of two: a null bitmap
                // otherwise, which both modules read through) the area is a power-of-two bitmap's own size
                FreeBitmap(b);
                bitmaps.pop_back();
                const int size[2] = {1 << (r() % 7), 1 << (r() % 7)};
                b = TestNew(0x44, "BMP%d", made++);
                bitmapCtor(b, nullptr, size, r() % 2 ? 2 : 3, nullptr);  // (Scale makes 32-bit ones only)
                bitmaps.push_back(b);
                const int* bf = static_cast<const int*>(b);
                unsigned char* data = reinterpret_cast<unsigned char*>(static_cast<uintptr_t>(bf[3]));
                for (unsigned i = 0, n = static_cast<unsigned>(bf[4]) * static_cast<unsigned>(bf[1]); i < n; ++i)
                    data[i] = static_cast<unsigned char>(r());
                const int all[4] = {rect[0], rect[1], size[0], size[1]};
                std::memcpy(const_cast<int*>(rect), all, sizeof all);
                ok = a.subLoadAll(t, b, rect, f, r() % 6);
                Log("op subload all %s", Name(t).c_str());
            }
            Log(" %d,%d %dx%d %s: %d\n", rect[0], rect[1], rect[2], rect[3], BitmapText(b).c_str(), ok);
            DumpTex("subloaded", t);
            break;
        }
        case 8: {  // from the frame, Destroy
            const int rect[4] = {static_cast<int>(r() % 20), static_cast<int>(r() % 20), 1 + static_cast<int>(r() % 64),
                                 1 + static_cast<int>(r() % 64)};
            const int at[2] = {1, 2};
            const unsigned level = r() % 4 == 0 ? static_cast<unsigned>(-1) : r() % 6;
            using FrameFn = bool(__cdecl*)(void*, unsigned, const int*, const int*, unsigned);
            const bool ok = (*static_cast<FrameFn* const*>(t))[5](t, level, at, rect, 0);
            Log("op frame %s %d: %d\n", Name(t).c_str(), static_cast<int>(level), ok);
            if (r() % 3 == 0) {
                Log("op destroy %s\n", Name(t).c_str());
                SC<void>(t, 2);
            }
            DumpTex("framed", t);
            break;
        }
        case 9: case 10: {  // Bind
            const unsigned unit = r() % 2 ? r() % 4 : r() % 1000;
            if (r() % 2) {
                Log("op bind %s %u: %d\n", Name(t).c_str(), unit, SC<bool>(t, 8, unit));
            } else {
                static const float anis[] = {1.0f, 4.0f, 32.0f, -1.0f, 0.0f};
                const float an = anis[r() % 5];
                const bool keep = r() % 2 != 0;
                using BindFn = bool(__cdecl*)(void*, unsigned, bool, float, int, int, int, int, int);
                const int w1 = static_cast<int>(r() % 3), w2 = static_cast<int>(r() % 3);
                const int f1 = static_cast<int>(r() % 3), f2 = static_cast<int>(r() % 3);
                const bool ok = (*static_cast<BindFn* const*>(t))[7](t, unit, keep, an, w1, w2, f1, f2, 2);
                Log("op bind full %s %u keep %d aniso %08x %d %d %d %d: %d\n", Name(t).c_str(), unit, keep,
                    FloatBits(an), w1, w2, f1, f2, ok);
            }
            if (r() % 4 == 0) {
                if (*bound) {
                    a.unbindRender(t, unit);
                    Log("op unbind render %u\n", unit);
                }
            }
            DumpTex("bound", t);
            DumpBound();
            break;
        }
        case 11: {  // GetBitmap, GetTextureBitmap
            unsigned char f[12];
            MakeFormat(r, f);
            const unsigned level = r() % 4, face = r() % 8;
            using GetFn = void*(__fastcall*)(const void*, void*, unsigned, unsigned, void*, int);
            const void* b = (*static_cast<GetFn* const*>(t))[11](t, nullptr, level, face, f, 3);
            Log("op get bitmap %s %u %u: %s / %s\n", Name(t).c_str(), level, face, Name(b).c_str(),
                Name(a.textureBitmap(t, 0, 0, f, 3)).c_str());
            break;
        }
        case 12: {  // CreateInstance (not with a kept bitmap on a cube map: TS12 frees it once per face)
            void* image = At<void*>(t, 0x6c);
            // (with a kept bitmap it loads it with a format whose id and bit count are 0, and the null texture's
            // Load divides by bits / 8: both modules fault. TS12 also frees it once per level and face)
            if (image) break;
            Log("op create instance %s\n", Name(t).c_str());
            void* n = SC<void*>(t, 10);
            Log("  instance %s\n", Name(n).c_str());
            if (image) At<void*>(t, 0x6c) = nullptr;  // (freed by CreateInstance, left pointing)
            if (n) {
                DumpTex("instance", n);
                tex.push_back(n);
            }
            DumpTex("source", t);
            break;
        }
        case 13: {  // CopySamplerAttributesFrom, InitInstance
            void* o = tex[r() % tex.size()];
            if (r() % 2) {
                Log("op copy sampler %s <- %s\n", Name(t).c_str(), Name(o).c_str());
                SC<void>(t, 14, o);
            } else {
                if (At<void*>(o, 0x34) || o == t) break;  // (the target's construct would leak)
                Log("op init instance %s -> %s\n", Name(t).c_str(), Name(o).c_str());
                a.initInstance(t, o);
            }
            DumpTex("to", o == t ? t : o);
            break;
        }
        case 14: {  // the copy and the assignment (the shared bitmap and construct are dropped from the copy)
            void* y = TestNew(0x70, "T%d", made++);
            a.copy(y, nullptr, t);
            DumpTex("copy", y);
            a.assign(y, nullptr, t);
            DumpTex("assigned", y);
            At<void*>(y, 0x6c) = nullptr;
            At<void*>(y, 0x34) = nullptr;
            At<int>(y, 0x64) = 0;
            ++*numObjects;  // (the copy did not count itself)
            DestroyTex(y);
            break;
        }
        case 15: {  // the setters
            a.setLost(t, r() % 2 != 0);
            a.setActive(t, r() % 5);
            Log("op setters %s\n", Name(t).c_str());
            DumpTex("set", t);
            break;
        }
        case 16: {  // the statics
            const int size[2] = {static_cast<int>(r() % 3000) - 5, static_cast<int>(r() % 3000) - 5};
            const unsigned level = r() % 14;
            int out[2] = {-7, -7};
            a.levelSize(out, size, level);
            Log("op mips of %dx%d: %d, level %u %dx%d\n", size[0], size[1], a.numMips(size), level, out[0], out[1]);
            break;
        }
        case 17: {  // CheckFormat directly
            unsigned char f[12];
            MakeFormat(r, f);
            void* b = MakeBitmap(r);
            bitmaps.push_back(b);
            void* data = r() % 4 ? reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<int*>(b)[3])) : nullptr;
            int p = static_cast<int*>(b)[0], x = 5, y = 6;
            void* e = a.checkFormat(t, f, &data, &p, &x, &y, static_cast<int*>(b)[0], static_cast<int*>(b)[1]);
            Log("op check format %s: entry %d data %s p %d x %d y %d\n", Hex(f, 12).c_str(),
                e ? static_cast<int>((static_cast<unsigned char*>(e) - nullTable) / 36) : -1,
                data == reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<int*>(b)[3])) ? "same" : data ? "new" : "null",
                p, x, y);
            break;
        }
        case 18: {  // release one
            if (tex.size() < 2) break;
            const size_t k = r() % tex.size();
            Log("op release %s\n", Name(tex[k]).c_str());
            DestroyTex(tex[k]);
            tex.erase(tex.begin() + static_cast<long>(k));
            DumpBound();
            break;
        }
        case 19: {  // BindTexture directly
            const unsigned unit = r() % 4;
            if (!*bound) break;
            a.bindTexture(t, unit);
            Log("op bind texture %s %u\n", Name(t).c_str(), unit);
            DumpBound();
            break;
        }
        default:
            Log("op getters %s\n", Name(t).c_str());
            DumpGetters(a, t);
            break;
        }
    }
    for (void* t : tex) {
        Log("op release %s\n", Name(t).c_str());
        DestroyTex(t);
    }
    DumpBound();
    for (void* b : bitmaps) FreeBitmap(b);
}

// ---- running

static void Compare(int round)
{
    ++checks;
    if (logText[0] == logText[1]) return;
    if (const char* dump = std::getenv("RS_U6C_DUMP"); dump && !bad)
        for (int s = 0; s < 2; ++s)
            if (FILE* f = std::fopen((std::string(dump) + (s ? ".orig.txt" : ".ours.txt")).c_str(), "w")) {
                std::fputs(logText[s].c_str(), f);
                std::fclose(f);
            }
    if (++bad > 6) return;
    const std::string &x = logText[0], &y = logText[1];
    size_t i = 0;
    while (i < x.size() && i < y.size() && x[i] == y[i]) ++i;
    size_t from = x.rfind("op ", i);
    if (from == std::string::npos || i - from > 3000) from = i > 600 ? i - 600 : 0;
    std::printf("MISMATCH round %d\n-- ours:\n%s\n-- original:\n%s\n", round, x.substr(from, i - from + 600).c_str(),
                y.substr(from, i - from + 600).c_str());
}

static LONG WINAPI Crashed(EXCEPTION_POINTERS* e)
{
    const auto at = reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress);
    for (int s = 0; s < 2; ++s) {
        const auto b = reinterpret_cast<uintptr_t>(mod[s]);
        if (at - b < 0x200000) std::printf("fault in %s +0x%x\n", s ? "the original" : "ours", static_cast<unsigned>(at - b));
    }
    std::printf("fault %08lx at %p, side %d; log end:\n", e->ExceptionRecord->ExceptionCode,
                e->ExceptionRecord->ExceptionAddress, cur);
#if defined(_M_IX86)
    // return addresses on the stack that point into the modules
    const uintptr_t* sp = reinterpret_cast<const uintptr_t*>(e->ContextRecord->Esp);
    for (int k = 0; k < 64; ++k) {
        for (int s = 0; s < 2; ++s)
            if (sp[k] - reinterpret_cast<uintptr_t>(mod[s]) < 0x100000)
                std::printf("  stack +%x: %s +0x%x\n", k * 4, s ? "orig" : "ours", static_cast<unsigned>(sp[k] - reinterpret_cast<uintptr_t>(mod[s])));
        if (sp[k] - reinterpret_cast<uintptr_t>(ks) < 0x100000)
            std::printf("  stack +%x: ks +0x%x\n", k * 4, static_cast<unsigned>(sp[k] - reinterpret_cast<uintptr_t>(ks)));
    }
#endif
    if (cur >= 0) {
        const std::string& t = logText[cur];
        std::printf("%s\n", t.substr(t.size() > 3000 ? t.size() - 3000 : 0).c_str());
    }
    std::fflush(stdout);
    ExitProcess(3);
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(&Crashed);
    if (argc < 2) {
        std::printf("usage: diff_rs_u6c <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 500;
    const std::string binDir = argv[1];
    SetDllDirectoryA(argv[1]);
    mod[0] = LoadLibraryA((binDir + "\\RenderSystem_7c.dll").c_str());
    mod[1] = GetModuleHandleA("RenderSystem_7c_orig.dll");
    ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    RealNew = static_cast<NewFn>(Proc(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    RealDelete = static_cast<DeleteFn>(Proc(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    bitmapCtor = As<decltype(bitmapCtor)>(Proc(ks, "??0Bitmap@Jet@@QAE@ABUSize@1@W4Format@01@PAE@Z"));
    bitmapDtor = As<decltype(bitmapDtor)>(Proc(ks, "??1Bitmap@Jet@@QAE@XZ"));
    bound = static_cast<const void***>(Proc(mod[1], "?bound@" DN "1PAPBV12@A"));
    numObjects = static_cast<unsigned*>(Proc(mod[1], "?numObjects@" DN "1IA"));
    nullTable = reinterpret_cast<unsigned char*>(mod[1]) + 0xb8228;
    formatCtor = As<decltype(formatCtor)>(Proc(mod[1], "??0DisplayFormat@Jet@@QAE@W4DFormatId@1@@Z"));
    rtcCtor = As<decltype(rtcCtor)>(Proc(mod[1], "??0RenderTextureConstruct@Jet@@QAE@W4TextureTarget@1@_N11PBVSharedContext@1@@Z"));
    rtcDtor = As<decltype(rtcDtor)>(Proc(mod[1], "??1RenderTextureConstruct@Jet@@QAE@XZ"));
    fixedNames[Proc(mod[1], "?textureStats@" DT "1UDisplayTextureStats@12@A")] = "STATS";
    for (void*& p : displayVt) p = reinterpret_cast<void*>(&DisplaySlot);
    displayVt[0x1c / 4] = reinterpret_cast<void*>(&DisplayQuery);
    displayVt[0x28 / 4] = reinterpret_cast<void*>(&DisplayMaxAniso);
    displayVt[0x98 / 4] = reinterpret_cast<void*>(&DisplayCreateTexture);
    displayVt[0xa0 / 4] = reinterpret_cast<void*>(&DisplaySetTexture);
    *reinterpret_cast<void**>(displayObj) = displayVt;
    fixedNames[displayObj] = "D";
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
    }
    for (int round = 0; round < rounds; ++round) {
        const unsigned seed = rng();
        for (int s = 0; s < 2; ++s) {
            cur = s;
            logText[s].clear();
            names[s].clear();
            blockSize[s].clear();
            allocCount[s] = 0;
            RunNull(s, seed);
            cur = -1;
        }
        Compare(round);
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
