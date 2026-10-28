// Differential test of RenderSystem_7c unit U6a: TextureResource (the binary .texture reader and writer) and
// BinaryTextureResourceExport (TextureResource.cpp), ours against RenderSystem_7c_orig.dll in one process.
//
// As in diff_rs_u5: both modules' imports of Jet::Alloc::New / Delete are hooked (blocks filled with 0xcd and
// numbered, frees logged by name), each scenario runs the same random script on both sides with that side's
// exports, and the logs must be equal. The inputs are real .texture files of the install (the stand-in resource
// database hands their paths out), files written by both sides are compared byte for byte, and stand-in API
// textures and a stand-in Display log what they are handed. The TextureResource defaults and the TextureManager
// limits are the original's statics on both sides; each scenario sets them first.
//
// usage: diff_rs_u6.exe <bin dir> [rounds] [texture root]
//   env RS_U6_ECHO (echo the log), RS_U6_DUMP=<prefix> (both logs of the first mismatch)
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <random>
#include <string>
#include <vector>

static HMODULE mod[2];  // 0 ours, 1 the original
static HMODULE ks;
static unsigned long long checks, bad;
static std::mt19937 rng(6601);

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

// ---- the heap hooks and the logs

using NewFn = void*(__cdecl*)(unsigned);
using DeleteFn = void(__cdecl*)(void*);
static NewFn RealNew;
static DeleteFn RealDelete;
static void** cache;
static void* (*CreateNode)(void*, const unsigned char*);
static void (*DestroyNode)(void*, void*);
static bool(__fastcall* ReleaseRef)(const void*);

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
    static const bool echo = std::getenv("RS_U6_ECHO") != nullptr;
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

static void* __cdecl HookNew(unsigned n)
{
    void* p = RealNew(n);
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
    if (cur >= 0 && p) {
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
    std::memset(p, 0xcd, n);
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

// ---- per-side entry points

#define TR "TextureResource@Jet@@"
#define BX "BinaryTextureResourceExport@Jet@@"
#define RES "?AV?$ResultClass@$1?results@Resource@Jet@@2QBUResultTable@2@BW4Results@Resource@Jet@@@2@"

struct Api {
    void*(__fastcall* ctor)(void*, void*, const void* name, const void* db);
    void*(__fastcall* copy)(void*, void*, const void*);
    void*(__fastcall* assign)(void*, void*, const void*);
    void* vft;
    int*(__cdecl* loadStatic)(int*, const void* db, const char* name, const void* key, int* first, int* last, void* info);
    int*(__cdecl* loadMips)(int*, void* info, void* texture, int first, int last);
    const void*(__cdecl* getBitmap)(const void*, unsigned, unsigned, const int*);
    unsigned char*(__cdecl* getData)(const void*, unsigned*, unsigned, unsigned);
    bool(__cdecl* canAsync)(const void*);
    int(__cdecl* diskLevels)(const void*);
    void(__cdecl* unloadData)(void*);
    int*(__fastcall* sizeAt)(const void*, void*, int*, int);
    int*(__cdecl* sizeAt3)(int*, int, int, int);
    int*(__cdecl* sizeAt2)(int*, int, int, int);
    bool(__cdecl* saveFile)(const void*, void* file, void* display);
    void*(__fastcall* infoCtor)(void*, void*);
    void(__fastcall* infoDtor)(void*, void*);
    // BinaryTextureResourceExport
    void*(__fastcall* xCtor)(void*, void*, const void* name);
    void*(__fastcall* xCopy)(void*, void*, const void*);
    void*(__fastcall* xAssign)(void*, void*, const void*);
    void* xVft;
    void(__cdecl* xSet[16])(void*, unsigned);
    void(__cdecl* xSetAniso)(void*, float);
    void(__cdecl* xSetCompression)(void*, const void*);
    void(__cdecl* xSetData)(void*, void*, int);
    unsigned(__cdecl* defGet[11])();
    void(__cdecl* defSet[11])(unsigned);
    float(__cdecl* defGetAniso)();
    void(__cdecl* defSetAniso)(float);
    void(__cdecl* defGetComp)(char*);
    void(__cdecl* defSetComp)(char*);
};
static const char* kDefaults[11][2] = {
    {"?GetDefaultWrapS@" TR "SA?AW4TextureWrap@2@XZ", "?SetDefaultWrapS@" TR "SAXW4TextureWrap@2@@Z"},
    {"?GetDefaultWrapT@" TR "SA?AW4TextureWrap@2@XZ", "?SetDefaultWrapT@" TR "SAXW4TextureWrap@2@@Z"},
    {"?GetDefaultMinFilter@" TR "SA?AW4TextureFilter@2@XZ", "?SetDefaultMinFilter@" TR "SAXW4TextureFilter@2@@Z"},
    {"?GetDefaultMagFilter@" TR "SA?AW4TextureFilter@2@XZ", "?SetDefaultMagFilter@" TR "SAXW4TextureFilter@2@@Z"},
    {"?GetDefaultMipFilter@" TR "SA?AW4TextureFilter@2@XZ", "?SetDefaultMipFilter@" TR "SAXW4TextureFilter@2@@Z"},
    {"?GetDefaultAutoMip@" TR "SA?AW4TextureAutoMip@2@XZ", "?SetDefaultAutoMip@" TR "SAXW4TextureAutoMip@2@@Z"},
    {"?GetDefaultAlphaHint@" TR "SA?AW4TextureAlpha@2@XZ", "?SetDefaultAlphaHint@" TR "SAXW4TextureAlpha@2@@Z"},
    {"?GetDefaultTextureHint@" TR "SA?AW4TextureHint@2@XZ", "?SetDefaultTextureHint@" TR "SAXW4TextureHint@2@@Z"},
    {"?GetDefaultDisplayFormatId@" TR "SA?AW4DFormatId@2@XZ", "?SetDefaultDisplayFormatId@" TR "SAXW4DFormatId@2@@Z"},
    {nullptr, nullptr}, {nullptr, nullptr}};
static Api api[2];
static const char* kSetters[16] = {
    "?SetNumMipLevels@" BX "QAAXI@Z",       "?SetDimension@" BX "QAAXW4TextureDimension@2@@Z",
    "?SetDisplayFormatId@" BX "QAAXW4DFormatId@2@@Z", "?SetImageFormatId@" BX "QAAXW4DFormatId@2@@Z",
    "?SetDepth@" BX "QAAXI@Z",              "?SetHint@" BX "QAAXW4TextureHint@2@@Z",
    "?SetWrapS@" BX "QAAXW4TextureWrap@2@@Z", "?SetWrapT@" BX "QAAXW4TextureWrap@2@@Z",
    "?SetMinFilter@" BX "QAAXW4TextureFilter@2@@Z", "?SetMagFilter@" BX "QAAXW4TextureFilter@2@@Z",
    "?SetMipFilter@" BX "QAAXW4TextureFilter@2@@Z", "?SetAlphaHint@" BX "QAAXW4TextureAlpha@2@@Z",
    "?SetAutoMipHint@" BX "QAAXW4TextureAutoMip@2@@Z", "?SetFileCompressionRatio@" BX "QAAXI@Z",
    nullptr, nullptr};

static void LoadApi(int s)
{
    Api& a = api[s];
    HMODULE m = mod[s];
#define F(name) Proc(m, name)
#define A(field, name) a.field = As<decltype(a.field)>(F(name))
    A(ctor, "??0" TR "QAE@ABVPString@1@PBVResourceDatabase@1@@Z");
    A(copy, "??0" TR "QAE@ABV01@@Z");
    A(assign, "??4" TR "QAEAAV01@ABV01@@Z");
    a.vft = F("??_7" TR "6B@");
    A(loadStatic, "?Load@" TR "SA" RES "PBVResourceDatabase@2@PBDABVString@2@AAH3PAUTextureInfo@12@@Z");
    A(loadMips, "?LoadMipLevels@" TR "SA" RES "PAUTextureInfo@12@PAVDisplayTexture@2@HH@Z");
    A(getBitmap, "?GetBitmap@" TR "QBAPBVBitmap@2@IIABUSize@2@@Z");
    A(getData, "?GetData@" TR "ABAPAEAAIII@Z");
    A(canAsync, "?CanAsyncLoadMips@" TR "QBA_NXZ");
    A(diskLevels, "?GetMipLevelCountOnDisk@" TR "QBAHXZ");
    A(unloadData, "?UnloadDataOnly@" TR "QAAXXZ");
    A(sizeAt, "?GetMipSizeAtReverseLevel@" TR "QBE?AUSize@2@H@Z");
    A(sizeAt3, "?GetMipSizeAtReverseLevel@" TR "SA?AUSize@2@HHH@Z");
    A(sizeAt2, "?GetMipSizeAtReverseLevel@" TR "SA?AUSize@2@HU32@@Z");
    A(saveFile, "?Save@" TR "ABA_NAAVJIRFFile@2@PAVDisplay@2@@Z");
    A(infoCtor, "??0TextureInfo@" TR "QAE@XZ");
    A(infoDtor, "??1TextureInfo@" TR "QAE@XZ");
    A(xCtor, "??0" BX "QAE@ABVPString@1@@Z");
    A(xCopy, "??0" BX "QAE@ABV01@@Z");
    A(xAssign, "??4" BX "QAEAAV01@ABV01@@Z");
    a.xVft = F("??_7" BX "6B@");
    for (int k = 0; k < 14; ++k) a.xSet[k] = As<void(__cdecl*)(void*, unsigned)>(F(kSetters[k]));
    A(xSetAniso, "?SetAnisotropyFilterLevel@" BX "QAAXM@Z");
    A(xSetCompression, "?SetFileCompression@" BX "QAAXABVAnsiString@2@@Z");
    A(xSetData, "?SetData@" BX "QAAXPAVBitmap@2@W4DFormatId@2@@Z");
    for (int k = 0; k < 9; ++k) {
        a.defGet[k] = As<unsigned(__cdecl*)()>(F(kDefaults[k][0]));
        a.defSet[k] = As<void(__cdecl*)(unsigned)>(F(kDefaults[k][1]));
    }
    A(defGetAniso, "?GetDefaultAnisotropyFilterLevel@" TR "SAMXZ");
    A(defSetAniso, "?SetDefaultAnisotropyFilterLevel@" TR "SAXM@Z");
    A(defGetComp, "?GetDefaultFileCompression@" TR "SAXQAD@Z");
    A(defSetComp, "?SetDefaultFileCompression@" TR "SAXPAD@Z");
#undef A
#undef F
}

// the original's statics both sides use
static int* minStreamMips;
static int* maxStreamSize;
static void(__cdecl* setDefault[5])(int);  // min, mag, mip filter, display format, alpha hint
static void(__cdecl* setDefaultCompression)(char*);
static void(__cdecl* getDefaultCompression)(char*);

// KernelStandard
static void*(__fastcall* bitmapCtor)(void*, void*, const int* size, int format, unsigned char* data);
static void(__fastcall* bitmapDtor)(void*, void*);
static void*(__fastcall* ansiCtor)(void*, void*, const char*);
static void(__fastcall* ansiDtor)(void*, void*);
static void*(__fastcall* jirfCtor)(void*, void*);
static void(__fastcall* jirfDtor)(void*, void*);
static int*(__cdecl* jirfOpen)(void*, int*, const char*, const void* key, unsigned long mode);
static void(__cdecl* jirfClose)(void*);
static void*(__fastcall* stringCtor)(void*, void*);
static void(__fastcall* stringDtor)(void*, void*);

// ---- the stand-ins

// the resource database: slot 3 (GetFileName) hands back the name as the path
static void* dbVt[8];
static unsigned char dbObj[0x48];
static int __cdecl DbSlot(void*) { Log("  db slot\n"); return 0; }
static void** __cdecl DbFileName(void* db, void** out, void* const* name)
{
    const char* text = *name ? static_cast<const char*>(*name) + 0x18 : "";
    *out = CreateNode(*cache, reinterpret_cast<const unsigned char*>(text));
    return out;
}

static std::string BitmapText(const void* p)
{
    if (!p) return "null";
    const int* b = static_cast<const int*>(p);
    unsigned size = static_cast<unsigned>(b[0x10]);
    // a bitmap over a stored block may claim more bytes than the block holds: only the block's are hashed
    if (cur >= 0 && b[3]) {
        const char* data = reinterpret_cast<const char*>(static_cast<uintptr_t>(b[3]));
        auto it = blockSize[cur].upper_bound(data);
        if (it != blockSize[cur].begin()) {
            --it;
            const char* start = static_cast<const char*>(it->first);
            if (data >= start && data <= start + it->second && size > static_cast<unsigned>(start + it->second - data))
                size = static_cast<unsigned>(start + it->second - data);
        }
    }
    char t[192];
    std::snprintf(t, sizeof t, "%dx%d pitch %d bpp %d format %d alloc %d bytes %u data %s %016llx", b[0], b[1], b[4],
                  b[5], b[6], *reinterpret_cast<const unsigned char*>(b + 7), size,
                  *reinterpret_cast<const unsigned char*>(b + 7) ? "own"
                      : Name(reinterpret_cast<void*>(static_cast<uintptr_t>(b[3]))).c_str(),
                  b[3] ? Fnv(reinterpret_cast<void*>(static_cast<uintptr_t>(b[3])), size) : 0ull);
    return t;
}
static void FreeBitmap(const void* b)
{
    if (!b) return;
    bitmapDtor(const_cast<void*>(b), nullptr);
    HookDelete(const_cast<void*>(b));
}

// the API texture: slot 1 makes it, slot 3 loads a level; +0x20 its level count, +0x30 "makes its own mips"
static void* texVt[8];
static unsigned char texObj[0x40];
static std::mt19937 standRng;
static bool __cdecl TexMake(void* t, const int* size, const unsigned char* format, int dim, unsigned levels, int autoMip,
                            unsigned depth, float aniso, int hint, int minF, int magF, int mipF, int wrapS, int wrapT,
                            void* args)
{
    unsigned bits;
    std::memcpy(&bits, &aniso, 4);
    const bool ok = standRng() % 5 != 0;
    Log("  texture make %dx%d %s dim %d levels %u automip %d depth %u aniso %08x hint %d filters %d %d %d wrap %d %d "
        "args %s: %d\n",
        size[0], size[1], Hex(format, 12).c_str(), dim, levels, autoMip, depth, bits, hint, minF, magF, mipF, wrapS,
        wrapT, Name(args).c_str(), ok);
    return ok;
}
static bool __cdecl TexLevel(void* t, int level, const unsigned char* format, const void* bitmap, unsigned face)
{
    const bool ok = standRng() % 12 != 0;
    Log("  texture level %d %s face %u %s: %d\n", level, Hex(format, 12).c_str(), face, BitmapText(bitmap).c_str(), ok);
    return ok;
}
static int __cdecl TexSlot(void*) { Log("  texture slot\n"); return 0; }

// the display: slot 0x148 converts a bitmap to a compressed format
static void* displayVt[0x60];
static unsigned char displayObj[0x40];
static int conversions;
static bool __cdecl DisplayConvert(void* d, void** out, const void* bitmap, int to, int from)
{
    const int mode = static_cast<int>(standRng() % 4);
    Log("  convert %s %d <- %d: mode %d\n", BitmapText(bitmap).c_str(), to, from, mode);
    if (mode == 0) return false;
    if (mode == 1) return true;  // (and no bitmap)
    const int* b = static_cast<const int*>(bitmap);
    const int size[2] = {b[0], b[1]};
    void* nb = TestNew(0x44, "CV%d", conversions++);
    bitmapCtor(nb, nullptr, size, 5, nullptr);
    unsigned char* data = reinterpret_cast<unsigned char*>(static_cast<uintptr_t>(static_cast<int*>(nb)[3]));
    const unsigned n = static_cast<unsigned>(static_cast<int*>(nb)[0x10]);
    for (unsigned k = 0; k < n; ++k) data[k] = static_cast<unsigned char>(k * 7 + to);
    *out = nb;
    return true;
}
static int __cdecl DisplaySlot(void*) { Log("  display slot\n"); return 0; }

// ---- the inputs

static std::vector<std::string> files;

struct Info {  // TextureResource::TextureInfo
    unsigned w[0x18];
    int levelsOnDisk;
    unsigned** blocks;
};
static_assert(sizeof(Info) == 0x68, "info");

static void DumpInfo(const char* tag, const void* p)
{
    if (!p) {
        Log("  %s info null\n", tag);
        return;
    }
    const Info* i = static_cast<const Info*>(p);
    Log("  %s info %s: %s on disk %d blocks %s\n", tag, Name(p).c_str(), Hex(i->w, 0x60).c_str(), i->levelsOnDisk,
        Name(i->blocks).c_str());
    if (!i->blocks) return;
    const unsigned n = i->w[2] * i->w[3];
    for (unsigned k = 0; k < n && k < 64; ++k) {
        const unsigned* b = i->blocks[k];
        if (!b)
            Log("    %u: null\n", k);
        else
            Log("    %u: %s size %u %016llx %s\n", k, Name(b).c_str(), b[0], Fnv(b + 1, b[0]),
                Hex(b + 1, b[0] < 24 ? b[0] : 24).c_str());
    }
}

// a virtual slot through the object's own vftable
template <class R, class... T>
static R SC(const void* c, int slot, T... args)
{
    using Fn = R(__cdecl*)(const void*, T...);
    return (*static_cast<Fn* const*>(c))[slot](c, args...);
}

static unsigned FloatBits(float f)
{
    unsigned u;
    std::memcpy(&u, &f, 4);
    return u;
}

// every getter of a texture resource (slots 18..41)
static void DumpGetters(const void* r, bool withColor = true)
{
    int size[2] = {-1, -1};
    float color[4] = {};
    char comp[9] = {};
    SC<void*>(r, 24, size);
    if (withColor) SC<void*>(r, 21, color);
    SC<void>(r, 26, comp);
    using FloatFn = float(__cdecl*)(const void*);
    const float aniso = (*static_cast<FloatFn* const*>(r))[25](r);
    Log("  getters: avail %d normal %d alpha %d color %s dim %d depth %u size %dx%d aniso %08x comp %s ratio %u "
        "format %d image %d display %d hint %d wrap %d %d filters %d %d %d levels %u automip %d must %d\n",
        SC<bool>(r, 18), SC<int>(r, 19), SC<int>(r, 20), Hex(color, 16).c_str(), SC<int>(r, 22), SC<unsigned>(r, 23),
        size[0], size[1], FloatBits(aniso), Hex(comp, 8).c_str(), SC<unsigned>(r, 27), SC<int>(r, 31), SC<int>(r, 32),
        SC<int>(r, 33), SC<int>(r, 34), SC<int>(r, 35), SC<int>(r, 36), SC<int>(r, 37), SC<int>(r, 38), SC<int>(r, 39),
        SC<unsigned>(r, 40), SC<int>(r, 41), SC<bool>(r, 7));
}

static void DestroyResource(void* o)
{
    const auto f = reinterpret_cast<void(__fastcall*)(void*, void*, int)>((*static_cast<void***>(o))[8]);
    f(o, nullptr, 1);
}

struct Resource {
    void* p;
    bool exportKind;
};

static int savedFiles[2];
static unsigned startDefaults[9];
static float startAniso;
static std::vector<std::string> kept[2];  // saved files kept for loading

static void RunTex(int s, unsigned seed)
{
    std::mt19937 r(seed);
    standRng.seed(seed ^ 0x6a6a);
    conversions = 0;
    savedFiles[s] = 0;
    const Api& a = api[s];
    int made = 0;

    // the statics, the same on both sides
    for (int k = 0; k < 9; ++k) api[1].defSet[k](startDefaults[k]);
    api[1].defSetAniso(startAniso);
    api[1].defSetComp(const_cast<char*>(""));
    kept[s].clear();
    *minStreamMips = 1 + static_cast<int>(r() % 14);
    *maxStreamSize = 16 << (r() % 9);
    static const int filters[] = {0, 1, 2, 3};
    for (int k = 0; k < 3; ++k) setDefault[k](filters[r() % 4]);
    static const int displayFormats[] = {1, 1, 3, 5, 10, 11, 12, 0x19, 0x1a, 0x1b, 0x1c, 2};
    setDefault[3](displayFormats[r() % 12]);
    setDefault[4](static_cast<int>(r() % 4));
    Log("statics: min mips %d max size %d\n", *minStreamMips, *maxStreamSize);

    std::vector<Resource> res;
    auto makeName = [&](const char* text) {
        void* n = CreateNode(*cache, reinterpret_cast<const unsigned char*>(text));
        return n;
    };
    auto pickFile = [&]() -> const std::string& { return files[r() % files.size()]; };

    auto newResource = [&]() {
        void* p = TestNew(0x64, "R%d", made++);
        const std::string& f = pickFile();
        void* name = makeName(f.c_str());
        Log("op new resource %s %s\n", Name(p).c_str(), std::filesystem::path(f).filename().string().c_str());
        a.ctor(p, nullptr, &name, dbObj);
        DestroyNode(*cache, name);
        res.push_back({p, false});
        return p;
    };
    for (int k = 0, n = 1 + static_cast<int>(r() % 2); k < n; ++k) newResource();

    for (int step = 0, steps = 6 + static_cast<int>(r() % 14); step < steps; ++step) {
        Resource& rr = res[r() % res.size()];
        void* p = rr.p;
        const Info* info = *reinterpret_cast<Info* const*>(static_cast<char*>(p) + 0x5c);
        switch (r() % 17) {
        case 0:
            newResource();
            break;
        case 1: case 2: {  // Load(name) (slot 10)
            if (rr.exportKind) break;
            const char* name = static_cast<const char*>(*reinterpret_cast<void**>(static_cast<char*>(p) + 4)) + 0x18;
            int result = -7;
            SC<int*>(p, 10, &result, name);
            Log("op load %s: %d\n", Name(p).c_str(), result);
            DumpInfo("loaded", *reinterpret_cast<void**>(static_cast<char*>(p) + 0x5c));
            Log("  flag %d async %d disk %d data size %u size %u\n", static_cast<unsigned char*>(p)[0x60],
                a.canAsync(p), a.diskLevels(p), SC<unsigned>(p, 16), SC<unsigned>(p, 15));
            break;
        }
        case 3: {  // the getters and the mips
            if (!info && !rr.exportKind) break;
            Log("op getters %s\n", Name(p).c_str());
            DumpGetters(p);
            if (rr.exportKind) break;
            const unsigned levels = info->w[2], depth = info->w[3];
            for (int k = r() % 3; k >= 0; --k) {
                const unsigned level = r() % levels, face = r() % depth;
                int size[2];
                a.sizeAt(p, nullptr, size, static_cast<int>(levels - 1 - level));
                const void* b = SC<const void*>(p, 43, level, face, size);
                Log("  mip %u %u %dx%d: %s\n", level, face, size[0], size[1], BitmapText(b).c_str());
                FreeBitmap(b);
                const void* c = a.getBitmap(p, level, face, size);
                Log("  bitmap: %s\n", BitmapText(c).c_str());
                FreeBitmap(c);
                unsigned n = 0;
                const unsigned char* d = a.getData(p, &n, level, face);
                Log("  data %s %u\n", Name(d).c_str(), n);
            }
            break;
        }
        case 4: {  // the static loader with a range
            const std::string& f = pickFile();
            void* in = TestNew(0x68, "I%d", made++);
            std::memset(in, 0, 0x68);
            a.infoCtor(in, nullptr);
            int first = static_cast<int>(r() % 16) - 3, last = static_cast<int>(r() % 16) - 3;
            void* key = TestNew(4, "K%d", made++);
            stringCtor(key, nullptr);
            int result = -7;
            Log("op static load %s %d %d\n", std::filesystem::path(f).filename().string().c_str(), first, last);
            a.loadStatic(&result, dbObj, f.c_str(), key, &first, &last, in);
            Log("  result %d first %d last %d\n", result, first, last);
            DumpInfo("static", in);
            if (result == 0 && r() % 2) {
                *reinterpret_cast<int*>(texObj + 0x20) = 1 + static_cast<int>(r() % 14);
                const int f1 = static_cast<int>(r() % 14) - 2, f2 = static_cast<int>(r() % 14) - 2;
                int res2 = -7;
                Log("op load mips %d %d %d\n", *reinterpret_cast<int*>(texObj + 0x20), f1, f2);
                a.loadMips(&res2, in, texObj, f1, f2);
                Log("  result %d\n", res2);
            }
            a.infoDtor(in, nullptr);
            stringDtor(key, nullptr);
            RealDelete(key);
            HookDelete(in);
            break;
        }
        case 5: {  // Load(DisplayTexture*) (slot 28)
            if (!info && !rr.exportKind) break;
            texObj[0x30] = static_cast<unsigned char>(r() % 3 == 0);
            int result = -7;
            Log("op load texture %s own mips %d\n", Name(p).c_str(), texObj[0x30]);
            SC<int*>(p, 28, &result, texObj);
            Log("  result %d\n", result);
            break;
        }
        case 6: case 7: {  // Save (slot 30), the file compared
            if (!info && !rr.exportKind) break;
            char path[MAX_PATH];
            std::snprintf(path, sizeof path, "%s\\u6_%d_%d.texture", std::getenv("TEMP"), s, savedFiles[s]++);
            DeleteFileA(path);
            const bool withDisplay = r() % 4 != 0;
            const bool ok = SC<bool>(p, 30, path, withDisplay ? static_cast<void*>(displayObj) : nullptr);
            std::string content;
            if (FILE* f = std::fopen(path, "rb")) {
                char buf[65536];
                size_t n;
                while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) content.append(buf, n);
                std::fclose(f);
            }
            Log("op save %s %d: %d file %u %016llx head %s\n", Name(p).c_str(), withDisplay, ok,
                static_cast<unsigned>(content.size()), Fnv(content.data(), content.size()),
                Hex(content.data(), static_cast<unsigned>(std::min<size_t>(content.size(), 0x80))).c_str());
            if (ok && r() % 2) {  // read back
                void* in = TestNew(0x68, "I%d", made++);
                std::memset(in, 0, 0x68);
                a.infoCtor(in, nullptr);
                int first = 0, last = 99, result = -7;
                void* key = TestNew(4, "K%d", made++);
                stringCtor(key, nullptr);
                a.loadStatic(&result, dbObj, path, key, &first, &last, in);
                Log("  read back %d %d %d\n", result, first, last);
                DumpInfo("back", in);
                a.infoDtor(in, nullptr);
                stringDtor(key, nullptr);
                RealDelete(key);
                HookDelete(in);
            }
            if (ok && r() % 2)
                kept[s].push_back(path);
            else
                DeleteFileA(path);
            break;
        }
        case 8: {  // UnloadDataOnly, then load again
            if (rr.exportKind) break;
            Log("op unload data %s\n", Name(p).c_str());
            a.unloadData(p);
            Log("  flag %d info %s\n", static_cast<unsigned char*>(p)[0x60],
                Name(*reinterpret_cast<void**>(static_cast<char*>(p) + 0x5c)).c_str());
            break;
        }
        case 9: {  // the mip sizes
            const int level = static_cast<int>(r() % 14) - 2, w = static_cast<int>(r() % 3000) - 5,
                      h = static_cast<int>(r() % 3000) - 5;
            int out1[2], out2[2];
            a.sizeAt3(out1, level, w, h);
            a.sizeAt2(out2, level, w, h);
            Log("op size at %d %dx%d: %dx%d %dx%d\n", level, w, h, out1[0], out1[1], out2[0], out2[1]);
            break;
        }
        case 10: case 11: {  // an export over a bitmap, saved
            int shift = 1 + static_cast<int>(r() % 6);
            static const int formats[] = {1, 2, 3, 5};
            const int bformat = formats[r() % 4];
            // (24-bit rows are padded to 4 bytes and a scaled copy leaves the padding unwritten: RGB bitmaps get
            // at least 4 columns and one level)
            if (bformat == 1 && shift < 2) shift = 2;
            const int size[2] = {1 << shift, 1 << (1 + static_cast<int>(r() % 6))};
            void* bmp = TestNew(0x44, "BM%d", made++);
            unsigned char* own = bformat == 5 ? static_cast<unsigned char*>(TestNew(size[0] * size[1] * 4 + 16, "PX%d", made++)) : nullptr;
            bitmapCtor(bmp, nullptr, size, bformat, own);
            unsigned char* data = reinterpret_cast<unsigned char*>(static_cast<uintptr_t>(static_cast<int*>(bmp)[3]));
            const unsigned n = own ? static_cast<unsigned>(size[0] * size[1] * 4 + 16) : static_cast<unsigned>(static_cast<int*>(bmp)[0x10]);
            for (unsigned k = 0; k < n; ++k) data[k] = static_cast<unsigned char>(r());
            void* name = makeName("export.texture");
            void* x = TestNew(0xa8, "X%d", made++);
            a.xCtor(x, nullptr, &name);
            DestroyNode(*cache, name);
            Log("op export %s %dx%d format %d data %s\n", Name(x).c_str(), size[0], size[1], bformat, Hex(data, 16).c_str());
            // (the base's GetAverageColor reads the info, Save calls it: the export gets a zeroed one)
            void* xi = TestNew(0x68, "XI%d", made++);
            std::memset(xi, 0, 0x68);
            for (int k = 0; k < 4; ++k) static_cast<unsigned*>(xi)[0x14 + k] = r();
            *reinterpret_cast<void**>(static_cast<char*>(x) + 0x5c) = xi;
            static const int ids[] = {1, 3, 5, 10, 11, 0x19, 0x1a, 2};
            a.xSetData(x, bmp, ids[r() % 8]);
            DumpGetters(x);  // (GetSize reads the bitmap)
            for (int k = 0; k < 14; ++k)
                if (r() % 2) {
                    unsigned v = r() % 4;
                    if (k == 0) v = 1 + r() % std::min(shift, 1 + static_cast<int>(r() % 6));  // mips within the size
                    if (k == 1) v = r() % 2;
                    if (k == 2 || k == 3) v = static_cast<unsigned>(ids[r() % 8]);
                    if (k == 4) v = r() % 3 ? 1 + r() % 2 : 6;
                    if (k == 13) v = r() % 12;
                    a.xSet[k](x, v);
                }
            if (r() % 2) a.xSetAniso(x, static_cast<float>(r() % 17) / 2.f);
            if (r() % 3 == 0) {
                void* an = TestNew(4, "AS%d", made++);
                static const char* comps[] = {"", "lzss", "zlib", "LZSS", "abcdefgh", "abcdefghij"};
                ansiCtor(an, nullptr, comps[r() % 6]);
                a.xSetCompression(x, an);
                ansiDtor(an, nullptr);
                RealDelete(an);
            }
            // every level must fit the bitmap
            if (static_cast<int*>(x)[0x70 / 4] > shift) static_cast<int*>(x)[0x70 / 4] = shift;
            if (bformat == 1) static_cast<int*>(x)[0x70 / 4] = 1;
            DumpGetters(x);
            if (r() % 2) {  // the copy and the assignment (the info pointer is shared: the test drops the copy's)
                void* y = TestNew(0xa8, "X%d", made++);
                a.xCopy(y, nullptr, x);
                Log("  copy info %s %s\n", Name(*reinterpret_cast<void**>(static_cast<char*>(y) + 0x5c)).c_str(),
                    (Hex(static_cast<char*>(y) + 0x60, 0xc) + " bitmap " +
                     Name(*reinterpret_cast<void**>(static_cast<char*>(y) + 0x6c)) + " " +
                     Hex(static_cast<char*>(y) + 0x70, 0x34)).c_str());
                a.xAssign(y, nullptr, x);
                *reinterpret_cast<void**>(static_cast<char*>(y) + 0x5c) = nullptr;
                DestroyResource(y);
            }
            res.push_back({x, true});
            (void)bmp;
            break;
        }
        case 12: {  // the copy constructor and the assignment of TextureResource
            if (rr.exportKind) break;
            void* y = TestNew(0x64, "R%d", made++);
            a.copy(y, nullptr, p);
            Log("op copy %s -> %s: info %s %s\n", Name(p).c_str(), Name(y).c_str(),
                Name(*reinterpret_cast<void**>(static_cast<char*>(y) + 0x5c)).c_str(), Hex(static_cast<char*>(y) + 0x60, 1).c_str());
            a.assign(y, nullptr, p);
            *reinterpret_cast<void**>(static_cast<char*>(y) + 0x5c) = nullptr;
            DestroyResource(y);
            break;
        }
        case 13: {  // a saved file loaded as a resource
            if (kept[s].empty()) break;
            const std::string path = kept[s][r() % kept[s].size()];
            void* q = TestNew(0x64, "R%d", made++);
            void* name = makeName(path.c_str());
            a.ctor(q, nullptr, &name, dbObj);
            DestroyNode(*cache, name);
            res.push_back({q, false});
            Log("op load saved %s: format %d\n", Name(q).c_str(), SC<int>(q, 32));
            int result = -7;
            SC<int*>(q, 10, &result, path.c_str());
            Log("  result %d\n", result);
            const Info* qi = *reinterpret_cast<Info* const*>(static_cast<char*>(q) + 0x5c);
            DumpInfo("saved", qi);
            if (!qi) break;
            DumpGetters(q);
            const unsigned levels = qi->w[2], depth = qi->w[3];
            for (int k = 0; k < 3; ++k) {
                const unsigned level = r() % levels, face = r() % depth;
                int size[2];
                a.sizeAt(q, nullptr, size, static_cast<int>(levels - 1 - level));
                const void* b = SC<const void*>(q, 43, level, face, size);
                Log("  mip %u %u: %s\n", level, face, BitmapText(b).c_str());
                FreeBitmap(b);
            }
            texObj[0x30] = static_cast<unsigned char>(r() % 3 == 0);
            SC<int*>(q, 28, &result, texObj);
            Log("  texture %d\n", result);
            break;
        }
        case 14: {  // a real file made older (an INFO version 0x102..0x108) or with a foreign block, statically loaded
            const std::string& f = pickFile();
            std::string bytes;
            if (FILE* fp = std::fopen(f.c_str(), "rb")) {
                char buf[65536];
                size_t n;
                while ((n = std::fread(buf, 1, sizeof buf, fp)) > 0) bytes.append(buf, n);
                std::fclose(fp);
            }
            if (bytes.size() < 0x74) break;
            const unsigned version = 0x102 + r() % 7;
            std::memcpy(&bytes[0x14], &version, 4);
            if (r() % 2) {  // dimension, depth
                const unsigned dim = r() % 2, depth = r() % 3 ? 1 : 1 + r() % 7;
                std::memcpy(&bytes[0x18], &dim, 4);
                std::memcpy(&bytes[0x20], &depth, 4);
            }
            if (r() % 2) {  // a foreign block before the levels (size 0 or 8)
                const unsigned size = r() % 3 ? 8 : 0;
                std::string block("XTRA", 4);
                block.append(reinterpret_cast<const char*>(&size), 4);
                block.append(size, '\x5a');
                bytes.insert(0x74, block);
            }
            char path[MAX_PATH];
            std::snprintf(path, sizeof path, "%s\\u6c_%d.texture", std::getenv("TEMP"), s);
            if (FILE* fp = std::fopen(path, "wb")) {
                std::fwrite(bytes.data(), 1, bytes.size(), fp);
                std::fclose(fp);
            }
            void* in = TestNew(0x68, "I%d", made++);
            std::memset(in, 0, 0x68);
            a.infoCtor(in, nullptr);
            int first = static_cast<int>(r() % 6) - 1, last = static_cast<int>(r() % 14), result = -7;
            void* key = TestNew(4, "K%d", made++);
            stringCtor(key, nullptr);
            Log("op crafted %s version %x\n", std::filesystem::path(f).filename().string().c_str(), version);
            a.loadStatic(&result, dbObj, path, key, &first, &last, in);
            Log("  result %d %d %d\n", result, first, last);
            DumpInfo("crafted", in);
            if (result == 0) {
                *reinterpret_cast<int*>(texObj + 0x20) = 1 + static_cast<int>(r() % 14);
                int res2 = -7;
                a.loadMips(&res2, in, texObj, static_cast<int>(r() % 6) - 1, static_cast<int>(r() % 14));
                Log("  mips %d\n", res2);
            }
            a.infoDtor(in, nullptr);
            stringDtor(key, nullptr);
            RealDelete(key);
            HookDelete(in);
            DeleteFileA(path);
            break;
        }
        case 15: {  // the defaults through this side's own accessors
            const int k = static_cast<int>(r() % 11);
            if (k < 9) {
                static const unsigned values[] = {0, 1, 2, 3, 5, 10, 0x1a};
                a.defSet[k](values[r() % 7]);
            } else if (k == 9) {
                a.defSetAniso(static_cast<float>(r() % 9) / 4.f);
            } else {
                char in[9] = "abcdefgh";
                in[r() % 9 % 8] = 0;
                a.defSetComp(in);
            }
            std::string t;
            for (int j = 0; j < 9; ++j) t += " " + std::to_string(a.defGet[j]());
            char out[9] = {};
            a.defGetComp(out);
            Log("op defaults %d:%s aniso %08x comp %s\n", k, t.c_str(), FloatBits(a.defGetAniso()), Hex(out, 8).c_str());
            break;
        }
        default: {  // the format getters of a resource that is not loaded (the image format answers the default)
            if (rr.exportKind || info) break;
            Log("op unloaded formats %s: %d %d\n", Name(p).c_str(), SC<int>(p, 32), SC<int>(p, 31));
            break;
        }
        }
    }
    // let everything go (the export's bitmap stays the test's)
    for (const std::string& f : kept[s]) DeleteFileA(f.c_str());
    for (const Resource& rr : res) {
        Log("op release %s\n", Name(rr.p).c_str());
        void* bmp = rr.exportKind ? *reinterpret_cast<void**>(static_cast<char*>(rr.p) + 0x6c) : nullptr;
        DestroyResource(rr.p);
        if (bmp) FreeBitmap(bmp);
    }
}

// ---- running

static void Compare(int round)
{
    ++checks;
    if (logText[0] == logText[1]) return;
    if (const char* dump = std::getenv("RS_U6_DUMP"); dump && !bad)
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
        std::printf("usage: diff_rs_u6 <bin dir> [rounds] [texture root]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 300;
    const std::string binDir = argv[1];
    const std::string root = argc > 3 ? argv[3] : binDir + "\\..\\UserData\\local";
    SetDllDirectoryA(argv[1]);
    mod[0] = LoadLibraryA((binDir + "\\RenderSystem_7c.dll").c_str());
    mod[1] = GetModuleHandleA("RenderSystem_7c_orig.dll");
    ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    // the inputs: the first .texture files of the root, in name order
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        const auto& p = it->path();
        if (p.extension() == ".texture" && std::filesystem::file_size(p, ec) < (8u << 20)) files.push_back(p.string());
        if (files.size() >= 400) break;
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        std::printf("no .texture files under %s\n", root.c_str());
        return 2;
    }
    std::printf("%u texture files\n", static_cast<unsigned>(files.size()));
    RealNew = static_cast<NewFn>(Proc(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    RealDelete = static_cast<DeleteFn>(Proc(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    CreateNode = As<decltype(CreateNode)>(Proc(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    DestroyNode = As<decltype(DestroyNode)>(Proc(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z"));
    cache = static_cast<void**>(Proc(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    ReleaseRef = As<decltype(ReleaseRef)>(Proc(ks, "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ"));
    bitmapCtor = As<decltype(bitmapCtor)>(Proc(ks, "??0Bitmap@Jet@@QAE@ABUSize@1@W4Format@01@PAE@Z"));
    bitmapDtor = As<decltype(bitmapDtor)>(Proc(ks, "??1Bitmap@Jet@@QAE@XZ"));
    ansiCtor = As<decltype(ansiCtor)>(Proc(ks, "??0AnsiString@Jet@@QAE@PBD@Z"));
    ansiDtor = As<decltype(ansiDtor)>(Proc(ks, "??1AnsiString@Jet@@QAE@XZ"));
    stringCtor = As<decltype(stringCtor)>(Proc(ks, "??0String@Jet@@QAE@XZ"));
    stringDtor = As<decltype(stringDtor)>(Proc(ks, "??1String@Jet@@QAE@XZ"));
    minStreamMips = static_cast<int*>(Proc(mod[1], "?MIN_STREAMING_TEXTURE_MIP_COUNT@TextureManager@Jet@@2HA"));
    maxStreamSize = static_cast<int*>(Proc(mod[1], "?MAX_STREAMING_TEXTURE_SIZE@TextureManager@Jet@@2HA"));
    setDefault[0] = As<void(__cdecl*)(int)>(Proc(mod[1], "?SetDefaultMinFilter@" TR "SAXW4TextureFilter@2@@Z"));
    setDefault[1] = As<void(__cdecl*)(int)>(Proc(mod[1], "?SetDefaultMagFilter@" TR "SAXW4TextureFilter@2@@Z"));
    setDefault[2] = As<void(__cdecl*)(int)>(Proc(mod[1], "?SetDefaultMipFilter@" TR "SAXW4TextureFilter@2@@Z"));
    setDefault[3] = As<void(__cdecl*)(int)>(Proc(mod[1], "?SetDefaultDisplayFormatId@" TR "SAXW4DFormatId@2@@Z"));
    setDefault[4] = As<void(__cdecl*)(int)>(Proc(mod[1], "?SetDefaultAlphaHint@" TR "SAXW4TextureAlpha@2@@Z"));
    setDefaultCompression = As<decltype(setDefaultCompression)>(Proc(mod[1], "?SetDefaultFileCompression@" TR "SAXPAD@Z"));
    getDefaultCompression = As<decltype(getDefaultCompression)>(Proc(mod[1], "?GetDefaultFileCompression@" TR "SAXQAD@Z"));
    // KernelResource's Resource reads the resource system's time: a zeroed stand-in (as in diff_rs_u3)
    static unsigned char fakeSystem[0x400];
    As<void(__cdecl*)(void*)>(Proc(GetModuleHandleA("KernelResource_7c.dll"), "?InitRS@Resource@Jet@@CAXPAVResourceSystem@2@@Z"))(fakeSystem);
    for (void*& p : dbVt) p = reinterpret_cast<void*>(&DbSlot);
    dbVt[3] = reinterpret_cast<void*>(&DbFileName);
    *reinterpret_cast<void**>(dbObj) = dbVt;
    fixedNames[dbObj] = "DB";
    for (void*& p : texVt) p = reinterpret_cast<void*>(&TexSlot);
    texVt[1] = reinterpret_cast<void*>(&TexMake);
    texVt[3] = reinterpret_cast<void*>(&TexLevel);
    *reinterpret_cast<void**>(texObj) = texVt;
    fixedNames[texObj] = "TEX";
    for (void*& p : displayVt) p = reinterpret_cast<void*>(&DisplaySlot);
    displayVt[0x148 / 4] = reinterpret_cast<void*>(&DisplayConvert);
    *reinterpret_cast<void**>(displayObj) = displayVt;
    fixedNames[displayObj] = "D";
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
    }
    for (int k = 0; k < 9; ++k) startDefaults[k] = api[1].defGet[k]();
    startAniso = api[1].defGetAniso();
    for (int round = 0; round < rounds; ++round) {
        const unsigned seed = rng();
        for (int s = 0; s < 2; ++s) {
            cur = s;
            logText[s].clear();
            names[s].clear();
            blockSize[s].clear();
            allocCount[s] = 0;
            RunTex(s, seed);
            cur = -1;
        }
        Compare(round);
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
