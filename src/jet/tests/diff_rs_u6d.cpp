// Differential test of RenderSystem_7c unit U6d1a: TextureManager's containers and lifetime (TextureManager.cpp),
// ours against RenderSystem_7c_orig.dll in one process.
//
// As in diff_rs_u6c2: both modules' Jet::Alloc imports are hooked and logged, each scenario runs the same random
// script on both sides and the logs must be equal. Each side makes its own manager and drives it through its
// slots and exports with stand-in textures (copies of the original's Texture / TextureRes vftables, so that
// RemoveTexture's dynamic_cast sees their RTTI; the slots log), a stand-in RenderKernel (a Display answering slots
// 0x1c and 0x28, an OS whose options come from a random command line), a stand-in ResourceDatabase and resource.
// The streaming half that stays the original's (Update, UpSizeTexture, CleanupAsyncTextureLoad, the two Removed*
// functions) and SharedTextureCollection::GenerateSubstituteLoadTexture get jumps to logging stand-ins, so both
// sides reach the same stubs; Resource::SetScrambleKey / ReleaseReference are hooked in both import tables. The
// statics both sides share (the original's) are set alike before each side and logged after.
//
// usage: diff_rs_u6d.exe <bin dir> [rounds]
//   env RS_U6D_ECHO (echo the log), RS_U6D_DUMP=<prefix> (both logs of the first mismatch)
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

static HMODULE mod[2];  // 0 ours, 1 the original
static HMODULE ks;
static unsigned long long checks, bad;
static std::mt19937 rng(6614);

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
static void** cache;
static void* (*CreateNode)(void*, const unsigned char*);
static void (*DestroyNode)(void*, void*);

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
    static const bool echo = std::getenv("RS_U6D_ECHO") != nullptr;
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

// a jump at the start of an original function
static void PatchJump(void* at, void* to)
{
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    auto* b = static_cast<unsigned char*>(at);
    b[0] = 0xe9;
    const int rel = static_cast<int>(reinterpret_cast<intptr_t>(to) - (reinterpret_cast<intptr_t>(at) + 5));
    std::memcpy(b + 1, &rel, 4);
    VirtualProtect(at, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
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

template <class T>
static T& At(const void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(const_cast<void*>(p)) + off); }
template <class R, class... T>
static R SC(const void* c, int slot, T... args)
{
    using Fn = R(__cdecl*)(const void*, T...);
    return (*static_cast<Fn* const*>(c))[slot](c, args...);
}
static const char* Text(const void* node) { return node ? static_cast<const char*>(node) + 0x18 : "(null)"; }
static unsigned Bits(float f)
{
    unsigned u;
    std::memcpy(&u, &f, 4);
    return u;
}

// ---- per-side entry points

#define TM "TextureManager@Jet@@"

struct Api {
    void*(__fastcall* ctor)(void*, void*);
    void(__cdecl* setUpdate)(void*, void*, bool);
    void*(__cdecl* substitute)(void*);
    unsigned long(__cdecl* total)(const void*);
    unsigned long(__cdecl* activeMemory)(const void*);
    unsigned(__cdecl* frame)(const void*);
    void*(__cdecl* kernel)(void*);
    void(__cdecl* setMin)(int);
    void(__cdecl* setMax)(int);
    void(__cdecl* setEnable)(bool);
};
static Api api[2];

static void LoadApi(int s)
{
    Api& a = api[s];
    HMODULE m = mod[s];
#define A(field, n) a.field = As<decltype(a.field)>(Proc(m, n))
    A(ctor, "??0" TM "QAE@XZ");
    A(setUpdate, "?SetTextureUpdate@" TM "IAAXPAVTexture@2@_N@Z");
    A(substitute, "?GetSubstituteTexture@" TM "IAAPAVTexture@2@XZ");
    A(total, "?GetTotalTextureMemory@" TM "QBAKXZ");
    A(activeMemory, "?GetActiveTextureMemory@" TM "QBAKXZ");
    A(frame, "?GetActiveFrame@" TM "IBAIXZ");
    A(kernel, "?GetRenderKernel@" TM "QAAPAVRenderKernel@2@XZ");
    A(setMin, "?SetMinSizeTextureStreaming@" TM "SAXH@Z");
    A(setMax, "?SetMaxSizeTextureStreaming@" TM "SAXH@Z");
    A(setEnable, "?SetEnableTextureStreaming@" TM "SAX_N@Z");
#undef A
}

static void(__fastcall* addRef)(const void*, void*);
static bool(__fastcall* releaseRef)(const void*, void*);
static void*(__fastcall* optionsCtor)(void*, void*);
static void(__cdecl* optionsRaw)(void*, const char*);

// the original's statics (shared by both sides)
static unsigned* skipLevels;
static unsigned* skipThreshold;
static int* formatDefault;
static float* anisoDefault;
static int* minFilter;
static int* magFilter;
static int* mipFilter;
static int* autoMip;
static int* minMips;
static int* maxMips;
static int* minSize;
static int* maxSize;
static bool* streaming;
static int* bitmapMaxSize;  // KernelResources' BitmapResource streaming size (set by SetMaxSizeTextureStreaming)
static void** substituteCache;  // SharedTextureCollection::textureArray[2]
static void** substituteName;   // textureNameArray[2] (a PString)
static std::vector<std::string> formatNames;  // the 16 names of the original's table (0x100b8780)

// ---- the stand-ins

static std::mt19937 standRng;
static int made;

// textures: 0x60 bytes, {vptr, refs, +0xc flags, +0x10 name node, +0x14 active frame, +0x4c async info, +0x50 mips
// on disk, +0x54 API texture, +0x58 streaming info, +0x5c memory}
static void* texVt[2][1 + 32];  // [0] Texture, [1] TextureRes: the COL, then the slots
static void __cdecl TexDeleteThis(const void* t) { Log("  texture %s delete this\n", Name(t).c_str()); }
static unsigned long __cdecl TexMemory(const void* t)
{
    Log("  texture %s memory\n", Name(t).c_str());
    return At<unsigned long>(t, 0x5c);
}
static void __cdecl TexUpdate(void* t, float time) { Log("  texture %s update %08x\n", Name(t).c_str(), Bits(time)); }
static void* __cdecl TexDisplay(const void* t)
{
    Log("  texture %s display texture\n", Name(t).c_str());
    return At<void*>(t, 0x54);
}
static void* __cdecl TexInfo(void* t)
{
    Log("  texture %s info\n", Name(t).c_str());
    return At<void*>(t, 0x58);
}
static int __cdecl TexSlot(const void* t) { Log("  texture %s slot\n", Name(t).c_str()); return 0; }

// API textures (+0x20 mip levels)
static void* dtVt[20];
static void __cdecl DtSlot(void* d) { Log("  display texture %s slot\n", Name(d).c_str()); }

// the kernel: +0x7c display, +0x8c OS (+0xc its options)
static unsigned char kernelObj[0x100];
static unsigned char osObj[0x10];
static void* displayVt[0x60];
static unsigned char displayObj[0x40];
static bool displayAutoMip;
static float displayMaxAniso;
static int __cdecl DisplaySlot(void*) { Log("  display slot\n"); return 0; }
static bool __cdecl DisplayQuery(void*, int what)
{
    Log("  display query %d\n", what);
    return displayAutoMip;
}
static float __cdecl DisplayMaxAniso(void*)
{
    Log("  display max anisotropy\n");
    return displayMaxAniso;
}

// the database: slot 1 Get(name, flags), +0xc its name node; the resource: slot 29 CreateInstance(name, manager)
static void* dbVt[8];
static unsigned char dbObj[0x40];
static void* resVt[48];
static unsigned char resObj[0x64];
static int dbAnswer;  // 0 none, else the resource
static int resAnswer;  // 0 none, else a new TextureRes stand-in
static void* curManager;
static std::vector<void*>* curTextures;
static void* MakeTexture(bool res, const char* name, std::mt19937& r);
static const void* __cdecl DbGet(const void* db, void* const* name, int flags)
{
    Log("  db get %s %d -> %d\n", Text(*name), flags, dbAnswer);
    return dbAnswer ? resObj : nullptr;
}
static void* __cdecl ResCreateInstance(const void* res, void* const* name, void* manager)
{
    Log("  resource create instance %s %s -> %d\n", Text(*name), Name(manager).c_str(), resAnswer);
    if (!resAnswer) return nullptr;
    void* t = MakeTexture(true, Text(*name), standRng);
    SC<void>(manager, 4, t);  // as TextureRes's constructor (Texture's) does
    curTextures->push_back(t);
    return t;
}
static int __cdecl ResSlot(const void*) { Log("  resource slot\n"); return 0; }
static void __cdecl SetScrambleKey(const void* res, const void* key) { Log("  set scramble key %s %s\n", Name(res).c_str(), Name(key).c_str()); }
static bool __cdecl ResRelease(const void* res)
{
    Log("  resource release %s\n", Name(res).c_str());
    return false;
}
static unsigned char keyObj[4];

// the original's streaming half and the substitute generator
static void __cdecl UpdateStub(void* m) { Log("  (update %s)\n", Name(m).c_str()); }
static void __fastcall CleanupStub(void* m, void*) { Log("  (cleanup async %s)\n", Name(m).c_str()); }
static bool __cdecl RemovedAsyncStub(void* m, const void* t)
{
    Log("  (removed async %s %s)\n", Name(m).c_str(), Name(t).c_str());
    return standRng() % 2 != 0;
}
static bool __cdecl RemovedDownStub(void* m, const void* t)
{
    Log("  (removed down size %s %s)\n", Name(m).c_str(), Name(t).c_str());
    return standRng() % 2 != 0;
}
static void __cdecl UpSizeStub(void* m, const void* t, int size) { Log("  (up size %s %s %d)\n", Name(m).c_str(), Name(t).c_str(), size); }
static int genAnswer;
static void* __cdecl GenerateStub(void* display, unsigned a, unsigned b)
{
    Log("  (generate substitute %s %u %u -> %d)\n", Name(display).c_str(), a, b, genAnswer);
    if (!genAnswer) return nullptr;
    void* d = TestNew(0x38, "DT%d", made++);
    *static_cast<void**>(d) = dtVt;
    return d;
}

static const char* kNames[] = {"wheel", "cab.texture", "clip.movie", "x.TEXTURE", "dbA:wheel-tex", "dbA:cab.texture",
                               "wheel-tex", "", "a.b", "dbB:x.TEXTURE"};

static void* MakeTexture(bool res, const char* name, std::mt19937& r)
{
    void* t = TestNew(0x60, res ? "R%d" : "T%d", made++);
    *static_cast<void**>(t) = &texVt[res][1];
    At<int>(t, 4) = 1;
    At<unsigned char>(t, 0xc) = static_cast<unsigned char>(r() % 4 ? 0 : 1 | (r() % 2 ? 2 : 0));
    At<void*>(t, 0x10) = CreateNode(*cache, reinterpret_cast<const unsigned char*>(name));
    At<unsigned>(t, 0x14) = r() % 4;
    At<int>(t, 0x50) = static_cast<int>(r() % 12);
    if (r() % 3) {
        void* d = TestNew(0x38, "DT%d", made++);
        *static_cast<void**>(d) = dtVt;
        At<int>(d, 0x20) = static_cast<int>(r() % 12);
        At<void*>(t, 0x54) = d;
    }
    if (res) {
        void* info = TestNew(0x20, "I%d", made++);
        At<int>(info, 0) = static_cast<int>(r() % 12) - 1;
        void* node = static_cast<char*>(info) + 0xc;
        At<void*>(node, 0) = At<void*>(node, 4) = At<void*>(node, 8) = node;
        At<void*>(t, 0x58) = info;
        if (r() % 3 == 0) {
            void* async = TestNew(0x18, "Q%d", made++);
            At<int>(async, 0x14) = static_cast<int>(r() % 4);
            At<void*>(t, 0x4c) = async;
        }
    }
    At<unsigned long>(t, 0x5c) = r() % 5 ? (r() % 0x100000) : 0xfffffc00u;
    return t;
}

// ---- the dumps

// a FastDynamicArray's items: {next, prev, count, data[13]} chunks in a ring from the head
static std::vector<void*> Items(void* head)
{
    std::vector<void*> v;
    if (!head) return v;
    void* c = head;
    do {
        for (unsigned i = 0; i < At<unsigned>(c, 8); ++i) v.push_back(At<void*>(c, 0xc + 4 * i));
        c = At<void*>(c, 0);
    } while (c != head && v.size() < 10000);
    return v;
}
static std::string Chunks(void* head)
{
    std::string s;
    if (!head) return "-";
    void* c = head;
    char b[32];
    do {
        std::snprintf(b, sizeof b, "%u/", At<unsigned>(c, 8));
        s += b;
        if (At<void*>(At<void*>(c, 0), 4) != c) s += "!";
        c = At<void*>(c, 0);
    } while (c != head && s.size() < 1000);
    return s;
}

static void DumpManager(const char* tag, void* m)
{
    Log("  %s %s: vt %s refs %d kernel %s frame %u active KB %lu substitute %d +0x38 %d +0xa4 %d +0xa8 %d index %d "
        "+0xd4 %d\n",
        tag, Name(m).c_str(), *static_cast<void**>(m) == *static_cast<void**>(m) ? "-" : "?", At<int>(m, 4),
        Name(At<void*>(m, 0x14)).c_str(), At<unsigned>(m, 0x28), At<unsigned long>(m, 0x2c), At<bool>(m, 0x30),
        At<int>(m, 0x38), At<int>(m, 0xa4), At<int>(m, 0xa8), At<int>(m, 0xd0), At<int>(m, 0xd4));
    // the table: count, size, then the entries sorted by name (bucket order depends on node addresses)
    void** buckets = At<void**>(m, 8);
    const unsigned size = At<unsigned>(m, 0x10);
    std::multiset<std::string> entries;
    unsigned seen = 0;
    if (buckets)
        for (unsigned k = 0; k < size; ++k)
            for (void* t : Items(buckets[k])) {
                ++seen;
                const bool placed = reinterpret_cast<uintptr_t>(At<void*>(t, 0x10)) % size == k;
                entries.insert(Name(t) + ":" + Text(At<void*>(t, 0x10)) + (placed ? "" : "(misplaced)"));
            }
    std::string e;
    for (const auto& s : entries) e += " " + s;
    Log("    table count %u size %u block %s items %u:%s\n", At<unsigned>(m, 0xc), size,
        buckets ? Name(reinterpret_cast<char*>(buckets) - 4).c_str() : "null", seen, e.c_str());
    std::string u;
    for (void* t : Items(At<void*>(m, 0x18))) u += " " + Name(t);
    Log("    updates [%s]:%s\n", Chunks(At<void*>(m, 0x18)).c_str(), u.c_str());
    std::string a;
    void** data = At<void**>(m, 0x24);
    for (unsigned i = 0; i < At<unsigned>(m, 0x1c) && i < 2000; ++i) a += " " + Name(data[i]);
    Log("    active %u/%u %s:%s\n", At<unsigned>(m, 0x1c), At<unsigned>(m, 0x20), Name(data).c_str(), a.c_str());
    for (int l = 0; l < 2; ++l) {
        char* h = static_cast<char*>(m) + 0xb0 + 0x10 * l;
        std::string r;
        int n = 0;
        for (char* p = At<char*>(h, 8); p != h && n < 100; p = At<char*>(p, 8), ++n)
            r += " " + Name(p) + (At<char*>(p, 0) == h ? "" : "(owner " + Name(At<void*>(p, 0)) + ")") +
                 (At<char*>(At<char*>(p, 8), 4) == p ? "" : "(!)");
        Log("    list %d owner %s prev %s +0xc %d:%s\n", l, Name(At<void*>(h, 0)).c_str(), Name(At<void*>(h, 4)).c_str(),
            At<int>(h, 0xc), r.c_str());
    }
}

static void DumpTex(void* t)
{
    std::string node = "-";
    const bool standIn = *static_cast<void**>(t) == &texVt[0][1] || *static_cast<void**>(t) == &texVt[1][1];
    if (void* info = standIn ? At<void*>(t, 0x58) : nullptr) {
        void* n = static_cast<char*>(info) + 0xc;
        node = "wanted " + std::to_string(At<int>(info, 0)) + " node " + Name(At<void*>(n, 0)) + "/" +
               Name(At<void*>(n, 4)) + "/" + Name(At<void*>(n, 8));
    }
    Log("  tex %s: refs %d flags %d name %s frame %u %s\n", Name(t).c_str(), At<int>(t, 4), At<unsigned char>(t, 0xc),
        Text(At<void*>(t, 0x10)), At<unsigned>(t, 0x14), node.c_str());
}

static void DumpStatics()
{
    Log("  statics: skip %u threshold %u format %d aniso %08x filters %d %d %d automip %d mips %d..%d size %d..%d "
        "streaming %d bitmap max %d\n",
        *skipLevels, *skipThreshold, *formatDefault, Bits(*anisoDefault), *minFilter, *magFilter, *mipFilter,
        *autoMip, *minMips, *maxMips, *minSize, *maxSize, *streaming, *bitmapMaxSize);
}

// ---- the scenario

struct Statics {
    unsigned skip, threshold;
    int format;
    float aniso;
    int minF, magF, mipF, automip, minM, maxM, minS, maxS;
    bool streaming;
    int bitmapMax;
};
static Statics Save()
{
    return {*skipLevels, *skipThreshold, *formatDefault, *anisoDefault, *minFilter, *magFilter, *mipFilter, *autoMip,
            *minMips, *maxMips, *minSize, *maxSize, *streaming, *bitmapMaxSize};
}
static void Restore(const Statics& s)
{
    *skipLevels = s.skip;
    *skipThreshold = s.threshold;
    *formatDefault = s.format;
    *anisoDefault = s.aniso;
    *minFilter = s.minF;
    *magFilter = s.magF;
    *mipFilter = s.mipF;
    *autoMip = s.automip;
    *minMips = s.minM;
    *maxMips = s.maxM;
    *minSize = s.minS;
    *maxSize = s.maxS;
    *streaming = s.streaming;
    *bitmapMaxSize = s.bitmapMax;
}

// a random option line for Init
static std::string Options(std::mt19937& r)
{
    static const char* filters[] = {"none", "trilinear", "bilinear", "NONE", "Trilinear", "x"};
    static const char* minmax[] = {"nearest", "linear", "LINEAR", "cubic"};
    static const char* automips[] = {"none", "default", "fastest", "nicest", "NICEST", "fast"};
    static const char* anisos[] = {"0", "0.5", "1", "2", "4", "8", "16", "32", "-3", "nan", "abc"};
    std::string s;
    char b[64];
    if (r() % 2) { std::snprintf(b, sizeof b, " -GlobalTextureResolution=%u", r() % 4); s += b; }
    if (r() % 2) { std::snprintf(b, sizeof b, " -TextureResThreshold=%u", r() % 2048); s += b; }
    if (r() % 2) s += " -DefaultTextureFormat=" + (r() % 4 ? formatNames[r() % formatNames.size()] : std::string("bogus"));
    if (r() % 2) s += std::string(" -DefaultAnisotropy=") + anisos[r() % 11];
    if (r() % 3 == 0) s += std::string(" -DefaultTextureFilter=") + filters[r() % 6];
    if (r() % 2) s += std::string(" -DefaultMinFilter=") + minmax[r() % 4];
    if (r() % 2) s += std::string(" -DefaultMipFilter=") + minmax[r() % 4];
    if (r() % 2) s += std::string(" -DefaultMagFilter=") + minmax[r() % 4];
    if (r() % 2) s += std::string(" -DefaultAutoMip=") + automips[r() % 6];
    if (r() % 2) s += r() % 2 ? " -UseSubstituteTexture=1" : " -UseSubstituteTexture=0";
    return s;
}

static void* optionsObj[2];  // the options made for the round (shared by both sides)

static void Run(int s, unsigned seed, const Statics& start, void* seeded)
{
    std::mt19937 r(seed);
    standRng.seed(seed ^ 0x6d6d);
    const Api& a = api[s];
    made = 0;
    Restore(start);
    // the substitute cache: empty, or a texture of the round (one reference for the cache)
    *substituteCache = seeded;
    if (seeded) At<int>(seeded, 4) = 2;
    std::vector<void*> tex;
    curTextures = &tex;
    displayAutoMip = r() % 2 != 0;
    static const float anis[] = {0.5f, 1.0f, 2.0f, 4.0f, 16.0f};
    displayMaxAniso = anis[r() % 5];
    At<void*>(osObj, 0xc) = r() % 6 ? optionsObj[0] : nullptr;
    At<void*>(dbObj, 0xc) = r() % 4 ? CreateNode(*cache, reinterpret_cast<const unsigned char*>(r() % 2 ? "dbA" : "dbB")) : nullptr;

    void* m = TestNew(0xd8, "M%d", 0);
    std::memset(m, 0xcd, 0xd8);  // fields the constructor leaves
    Log("op construct\n");
    a.ctor(m, nullptr);
    curManager = m;
    DumpManager("made", m);
    if (r() % 4) {
        Log("op init\n");
        const bool ok = SC<bool>(m, 2, kernelObj);
        Log("  -> %d\n", ok);
        DumpStatics();
        DumpManager("init", m);
    }
    if (r() % 3 == 0) At<int>(m, 0xd0) = static_cast<int>(r() % 2);  // the list (U6d1b switches it)
    for (int step = 0, steps = 8 + static_cast<int>(r() % 40); step < steps; ++step) {
        void* t = tex.empty() ? nullptr : tex[r() % tex.size()];
        switch (r() % 20) {
        case 0: case 1: case 2: {  // a texture added
            void* n = MakeTexture(r() % 2 != 0, kNames[r() % 10], r);
            Log("op add %s\n", Name(n).c_str());
            DumpTex(n);
            SC<void>(m, 4, n);
            tex.push_back(n);
            break;
        }
        case 3: case 4: {  // removed
            if (!t) break;
            Log("op remove %s\n", Name(t).c_str());
            SC<void>(m, 6, t);
            DumpTex(t);
            if (r() % 2) tex.erase(std::find(tex.begin(), tex.end(), t));
            break;
        }
        case 5: {  // found by name
            void* node = CreateNode(*cache, reinterpret_cast<const unsigned char*>(kNames[r() % 10]));
            void* f = SC<void*>(m, 7, &node);
            Log("op find %s -> %s\n", Text(node), Name(f).c_str());
            DestroyNode(*cache, node);
            break;
        }
        case 6: {  // created
            void* node = r() % 8 ? CreateNode(*cache, reinterpret_cast<const unsigned char*>(kNames[r() % 10])) : nullptr;
            dbAnswer = static_cast<int>(r() % 4);
            resAnswer = static_cast<int>(r() % 3);
            genAnswer = static_cast<int>(r() % 3);
            const void* db = r() % 8 ? static_cast<const void*>(dbObj) : nullptr;
            Log("op create %s %s\n", Name(db).c_str(), Text(node));
            void* c = SC<void*>(m, 3, db, &node, keyObj);
            Log("  -> %s\n", Name(c).c_str());
            if (c) {
                DumpTex(c);
                if (std::find(tex.begin(), tex.end(), c) == tex.end() && c != *substituteCache && c != seeded)
                    tex.push_back(c);
            }
            if (node) DestroyNode(*cache, node);
            break;
        }
        case 7: {  // the substitute
            genAnswer = static_cast<int>(r() % 3);
            void* c = a.substitute(m);
            Log("op substitute -> %s cache %s\n", Name(c).c_str(), Name(*substituteCache).c_str());
            if (c) DumpTex(c);
            break;
        }
        case 8: case 9: {  // a frame
            Log("op begin\n");
            SC<void>(m, 8);
            for (int k = 0, n = static_cast<int>(r() % 6); k < n && !tex.empty(); ++k) {
                void* x = tex[r() % tex.size()];
                if (r() % 3 == 0) At<unsigned>(x, 0x14) = At<unsigned>(m, 0x28);
                const int mips = static_cast<int>(r() % 16) - 2;
                Log("op activate %s %d\n", Name(x).c_str(), mips);
                SC<void>(m, 9, x, mips);
                DumpTex(x);
            }
            const float time = static_cast<float>(r() % 100) / 7.0f;
            Log("op end\n");
            SC<void>(m, 10, time);
            break;
        }
        case 10: {  // the update flag
            if (!t) break;
            const bool on = r() % 2 != 0;
            Log("op set update %s %d\n", Name(t).c_str(), on);
            a.setUpdate(m, t, on);
            break;
        }
        case 11: {  // streaming request lists
            if (!t || *static_cast<void**>(t) != &texVt[1][1]) break;
            Log("op register streamable %s\n", Name(t).c_str());
            SC<void>(m, 5, t);
            DumpTex(t);
            break;
        }
        case 12: {  // the streaming statics
            const int which = static_cast<int>(r() % 3);
            const int v = r() % 4 ? 1 << (r() % 13) : static_cast<int>(r() % 3000) - 1000;
            Log("op static %d %d\n", which, v);
            if (which == 0) a.setMin(v);
            else if (which == 1) a.setMax(v);
            else a.setEnable(v > 64);
            DumpStatics();
            break;
        }
        case 13:  // memory figures
            Log("op memory: total %lu active %lu frame %u kernel %s\n", a.total(m), a.activeMemory(m), a.frame(m),
                Name(a.kernel(m)).c_str());
            break;
        case 14:  // a texture's flag changes behind the manager
            if (!t) break;
            At<unsigned char>(t, 0xc) ^= 1;
            Log("op flip flag %s\n", Name(t).c_str());
            break;
        default:
            break;
        }
        if (r() % 4 == 0) DumpManager("step", m);
    }
    DumpManager("end", m);
    for (void* t : tex) DumpTex(t);
    Log("op release manager\n");
    releaseRef(m, nullptr);  // the last reference: DeleteThis, the deleting destructor
    for (void* t : tex) DumpTex(t);  // the request nodes the lists' destructors took off
    Log("  substitute cache %s refs %d\n", Name(*substituteCache).c_str(),
        *substituteCache ? At<int>(*substituteCache, 4) : 0);
    DumpStatics();
    *substituteCache = nullptr;
    curTextures = nullptr;
}

// ---- running

static void Compare(int round)
{
    ++checks;
    if (logText[0] == logText[1]) return;
    if (const char* dump = std::getenv("RS_U6D_DUMP"); dump && !bad)
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
    std::printf("fault %08lx at %p, side %d; log end:\n", e->ExceptionRecord->ExceptionCode,
                e->ExceptionRecord->ExceptionAddress, cur);
    for (int s = 0; s < 2; ++s) {
        const auto b = reinterpret_cast<uintptr_t>(mod[s]);
        const auto at = reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress);
        if (at - b < 0x100000) std::printf("  in %s +0x%x\n", s ? "the original" : "ours", static_cast<unsigned>(at - b));
    }
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
        std::printf("usage: diff_rs_u6d <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 500;
    const std::string binDir = argv[1];
    SetDllDirectoryA(argv[1]);
    mod[0] = LoadLibraryA((binDir + "\\RenderSystem_7c.dll").c_str());
    mod[1] = GetModuleHandleA("RenderSystem_7c_orig.dll");
    ks = GetModuleHandleA("KernelStandard_7c.dll");
    HMODULE krs = GetModuleHandleA("KernelResources_7c.dll");
    if (!mod[0] || !mod[1] || !ks || !krs) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    RealNew = static_cast<NewFn>(Proc(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    RealDelete = static_cast<DeleteFn>(Proc(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    CreateNode = As<decltype(CreateNode)>(Proc(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    DestroyNode = As<decltype(DestroyNode)>(Proc(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z"));
    cache = static_cast<void**>(Proc(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    addRef = As<decltype(addRef)>(Proc(ks, "?AddReference@ReferenceCount@Jet@@QBEXXZ"));
    releaseRef = As<decltype(releaseRef)>(Proc(ks, "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ"));
    optionsCtor = As<decltype(optionsCtor)>(Proc(ks, "??0Options@Jet@@QAE@XZ"));
    optionsRaw = As<decltype(optionsRaw)>(Proc(ks, "?ProcessRaw@Options@Jet@@QAAXPBD@Z"));
    HMODULE o = mod[1];
    skipLevels = static_cast<unsigned*>(Proc(o, "?globalSkipLevels@" TM "1IA"));
    skipThreshold = static_cast<unsigned*>(Proc(o, "?globalSkipThreshold@" TM "1IA"));
    formatDefault = static_cast<int*>(Proc(o, "?DISPLAYFORMATID_DEFAULT@TextureResource@Jet@@0W4DFormatId@2@A"));
    anisoDefault = static_cast<float*>(Proc(o, "?ANISOTROPY_DEFAULT@TextureResource@Jet@@0MA"));
    minFilter = static_cast<int*>(Proc(o, "?MIN_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A"));
    magFilter = static_cast<int*>(Proc(o, "?MAG_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A"));
    mipFilter = static_cast<int*>(Proc(o, "?MIP_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A"));
    autoMip = static_cast<int*>(Proc(o, "?AUTO_MIP_DEFAULT@TextureResource@Jet@@0W4TextureAutoMip@2@A"));
    minMips = static_cast<int*>(Proc(o, "?MIN_STREAMING_TEXTURE_MIP_COUNT@" TM "2HA"));
    maxMips = static_cast<int*>(Proc(o, "?MAX_STREAMING_TEXTURE_MIP_COUNT@" TM "2HA"));
    minSize = static_cast<int*>(Proc(o, "?MIN_STREAMING_TEXTURE_SIZE@" TM "2HA"));
    maxSize = static_cast<int*>(Proc(o, "?MAX_STREAMING_TEXTURE_SIZE@" TM "2HA"));
    streaming = static_cast<bool*>(Proc(o, "?TEXTURE_STREAMING_ENABLED@" TM "2_NA"));
    bitmapMaxSize = static_cast<int*>(Proc(krs, "?MAX_STREAMING_TEXTURE_SIZE@BitmapResource@Jet@@1HA"));
    substituteCache = static_cast<void**>(Proc(o, "?textureArray@SharedTextureCollection@Jet@@1PAV?$AutoReference@VTexture@Jet@@@2@A")) + 2;
    substituteName = static_cast<void**>(Proc(o, "?textureNameArray@SharedTextureCollection@Jet@@1PAVPString@2@A")) + 2;
    for (int i = 0; i < 16; ++i) {
        void* const* name = *reinterpret_cast<void* const* const*>(reinterpret_cast<unsigned char*>(o) + 0xb8780 + 8 * i);
        formatNames.push_back(name && *name ? Text(*name) : "");
    }
    std::printf("substitute name %s; formats:", Text(*substituteName));
    for (auto& f : formatNames) std::printf(" %s", f.c_str());
    std::printf("\n");
    // the streaming half and the generator: jumps into logging stand-ins (both sides reach the original's)
    PatchJump(Proc(o, "?Update@" TM "IAAXXZ"), reinterpret_cast<void*>(&UpdateStub));
    PatchJump(Proc(o, "?CleanupAsyncTextureLoad@" TM "IAEXXZ"), reinterpret_cast<void*>(&CleanupStub));
    PatchJump(Proc(o, "?RemovedAsyncTextureLoad@" TM "IAA_NPBVTextureRes@2@@Z"), reinterpret_cast<void*>(&RemovedAsyncStub));
    PatchJump(Proc(o, "?RemovedTextureDownSize@" TM "IAA_NPBVTextureRes@2@@Z"), reinterpret_cast<void*>(&RemovedDownStub));
    PatchJump(Proc(o, "?UpSizeTexture@" TM "QAAXPBVTextureRes@2@H@Z"), reinterpret_cast<void*>(&UpSizeStub));
    PatchJump(Proc(o, "?GenerateSubstituteLoadTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@II@Z"),
              reinterpret_cast<void*>(&GenerateStub));
    // the texture stand-ins: the original's vftables (their RTTI), every slot logging
    const char* vts[2] = {"??_7Texture@Jet@@6B@", "??_7TextureRes@Jet@@6BTexture@1@@"};
    for (int k = 0; k < 2; ++k) {
        void* const* v = static_cast<void* const*>(Proc(o, vts[k]));
        texVt[k][0] = v[-1];
        for (int i = 1; i <= 32; ++i) texVt[k][i] = reinterpret_cast<void*>(&TexSlot);
        texVt[k][1 + 0] = reinterpret_cast<void*>(&TexDeleteThis);
        texVt[k][1 + 8] = reinterpret_cast<void*>(&TexMemory);
        texVt[k][1 + 16] = reinterpret_cast<void*>(&TexUpdate);
        texVt[k][1 + 21] = reinterpret_cast<void*>(&TexDisplay);
        texVt[k][1 + 22] = reinterpret_cast<void*>(&TexInfo);
    }
    for (void*& p : dtVt) p = reinterpret_cast<void*>(&DtSlot);
    for (void*& p : displayVt) p = reinterpret_cast<void*>(&DisplaySlot);
    displayVt[0x1c / 4] = reinterpret_cast<void*>(&DisplayQuery);
    displayVt[0x28 / 4] = reinterpret_cast<void*>(&DisplayMaxAniso);
    *reinterpret_cast<void**>(displayObj) = displayVt;
    fixedNames[displayObj] = "D";
    At<void*>(kernelObj, 0x7c) = displayObj;
    At<void*>(kernelObj, 0x8c) = osObj;
    fixedNames[kernelObj] = "K";
    for (void*& p : dbVt) p = reinterpret_cast<void*>(&ResSlot);
    dbVt[1] = reinterpret_cast<void*>(&DbGet);
    *reinterpret_cast<void**>(dbObj) = dbVt;
    fixedNames[dbObj] = "DB";
    for (void*& p : resVt) p = reinterpret_cast<void*>(&ResSlot);
    resVt[29] = reinterpret_cast<void*>(&ResCreateInstance);
    *reinterpret_cast<void**>(resObj) = resVt;
    fixedNames[resObj] = "RES";
    fixedNames[keyObj] = "KEY";
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
        PatchImport(mod[s], "?SetScrambleKey@Resource@Jet@@QBAXABVString@2@@Z", reinterpret_cast<void*>(&SetScrambleKey));
        PatchImport(mod[s], "?ReleaseReference@Resource@Jet@@QBA_NXZ", reinterpret_cast<void*>(&ResRelease));
    }
    const Statics initial = Save();
    for (int round = 0; round < rounds; ++round) {
        const unsigned seed = rng();
        // the round's options (KernelStandard's heap; made once, shared by both sides)
        std::mt19937 orng(seed ^ 0x5151);
        optionsObj[0] = RealNew(0x10);
        optionsCtor(optionsObj[0], nullptr);
        const std::string line = Options(orng);
        optionsRaw(optionsObj[0], line.c_str());
        Statics start = initial;
        start.streaming = orng() % 2 != 0;
        start.minM = 1 + static_cast<int>(orng() % 4);
        start.maxM = start.minM + static_cast<int>(orng() % 10);
        start.aniso = orng() % 2 ? 1.0f : 2.0f;
        // a substitute texture already cached (made once per round on side 0's heap names)
        void* seeded = nullptr;
        if (orng() % 4 == 0) {
            cur = 0;
            seeded = RealNew(0x60);
            std::memset(seeded, 0, 0x60);
            *static_cast<void**>(seeded) = &texVt[0][1];
            At<void*>(seeded, 0x10) = CreateNode(*cache, reinterpret_cast<const unsigned char*>("seeded"));
            fixedNames[seeded] = "SEEDED";
            cur = -1;
        }
        for (int s = 0; s < 2; ++s) {
            cur = s;
            logText[s].clear();
            names[s].clear();
            blockSize[s].clear();
            allocCount[s] = 0;
            Log("options:%s\n", line.c_str());
            Run(s, seed, start, seeded);
            cur = -1;
        }
        Compare(round);
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
