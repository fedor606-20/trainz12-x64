// Differential test of RenderSystem_7c unit U6c2: Texture (TextureObject.cpp), ours against RenderSystem_7c_orig.dll
// in one process.
//
// As in diff_rs_u6c: both modules' Jet::Alloc imports are hooked and logged, each scenario runs the same random
// script on both sides and the logs must be equal. A stand-in TextureManager logs its slots (4 add, 6 remove,
// 9 activate; DeleteThis) and carries the current frame at +0x28; the original's TextureManager::SetTextureUpdate
// (U6d, called by both sides) gets a jump to a logging stand-in. The API textures are DisplayTextureNull objects
// of the side under test on a stand-in Display (the null format table filled with supported formats). The
// original's uniqueTexId is the same at the start of each side.
//
// usage: diff_rs_u6c2.exe <bin dir> [rounds]
//   env RS_U6C2_ECHO (echo the log), RS_U6C2_DUMP=<prefix> (both logs of the first mismatch)
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
#include <string>
#include <vector>

static HMODULE mod[2];  // 0 ours, 1 the original
static HMODULE ks;
static unsigned long long checks, bad;
static std::mt19937 rng(6604);

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
    static const bool echo = std::getenv("RS_U6C2_ECHO") != nullptr;
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
template <class T>
static T& At(const void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(const_cast<void*>(p)) + off); }
template <class R, class... T>
static R SC(const void* c, int slot, T... args)
{
    using Fn = R(__cdecl*)(const void*, T...);
    return (*static_cast<Fn* const*>(c))[slot](c, args...);
}
static const char* Text(const void* node) { return node ? static_cast<const char*>(node) + 0x18 : "(null)"; }

// ---- per-side entry points

#define TX "Texture@Jet@@"
#define DN "DisplayTextureNull@Jet@@"

struct Api {
    void*(__fastcall* ctor)(void*, void*, void* const* name, void* manager);
    void*(__fastcall* ctor2)(void*, void*, void* const* name, void* manager, void* texture, int alpha);
    void**(__cdecl* unique)(const void*, void**, void* const*);
    void**(__cdecl* nonUnique)(const void*, void**);
    void(__cdecl* init)(const void*, void*);
    void(__cdecl* setUpdate)(void*, bool);
    bool(__cdecl* getUpdate)(const void*);
    bool(__cdecl* active)(const void*);
    unsigned(__cdecl* frame)(const void*);
    void(__cdecl* setFrame)(void*, unsigned);
    void(__fastcall* setAlpha)(void*, void*, int);
    void* const*(__cdecl* name)(const void*);
    void*(__cdecl* manager)(const void*);
    void*(__fastcall* nullCtor)(void*, void*, void* display);
};
static Api api[2];

static void LoadApi(int s)
{
    Api& a = api[s];
    HMODULE m = mod[s];
#define A(field, n) a.field = As<decltype(a.field)>(Proc(m, n))
    A(ctor, "??0" TX "QAE@ABVPString@1@PAVTextureManager@1@@Z");
    A(ctor2, "??0" TX "QAE@ABVPString@1@PAVTextureManager@1@PAVDisplayTexture@1@W4TextureAlpha@1@@Z");
    A(unique, "?GenerateUniqueName@" TX "IBA?AVPString@2@ABV32@@Z");
    A(nonUnique, "?GetNonUniqueName@" TX "QBA?AVPString@2@XZ");
    A(init, "?InitInstance@" TX "IBAXAAV12@@Z");
    A(setUpdate, "?SetRequiresUpdate@" TX "IAAX_N@Z");
    A(getUpdate, "?GetRequiresUpdate@" TX "QBA_NXZ");
    A(active, "?GetActive@" TX "QBA_NXZ");
    A(frame, "?GetActiveFrame@" TX "QBAIXZ");
    A(setFrame, "?SetActiveFrame@" TX "QAAXI@Z");
    A(setAlpha, "?SetAlphaHint@" TX "QAEXW4TextureAlpha@2@@Z");
    A(name, "?GetName@" TX "QBAABVPString@2@XZ");
    A(manager, "?GetTextureManager@" TX "IBAPAVTextureManager@2@XZ");
    A(nullCtor, "??0" DN "QAE@PAVDisplay@1@@Z");
#undef A
}

static unsigned long* uniqueTexId;
static void*(__fastcall* formatCtor)(void*, void*, int id);
static void*(__fastcall* rtcCtor)(void*, void*, int target, bool, bool, bool, const void* shared);
static void(__fastcall* rtcDtor)(void*, void*);
static void*(__fastcall* bitmapCtor)(void*, void*, const int* size, int format, unsigned char* data);
static void(__fastcall* bitmapDtor)(void*, void*);
static bool(__fastcall* releaseRef)(const void*, void*);

// ---- the stand-ins

static std::mt19937 standRng;
static int made;

// the display (for the null API textures)
static void* displayVt[0x60];
static unsigned char displayObj[0x40];
static int __cdecl DisplaySlot(void*) { Log("  display slot\n"); return 0; }
static bool __cdecl DisplayQuery(void*, int) { return true; }
static float __cdecl DisplayMaxAniso(void*) { return 4.0f; }
static void* __cdecl DisplayCreateTexture(void* d)
{
    void* t = TestNew(0x70, "DT%d", made++);
    Log("  display create texture %s\n", Name(t).c_str());
    api[cur].nullCtor(t, nullptr, d);
    return t;
}
static void __cdecl DisplaySetTexture(void*, unsigned unit) { Log("  display set texture %u\n", unit); }

// the texture manager: {vptr, refs, ..., +0x28 current frame}
static void* managerVt[16];
static unsigned char managerObj[0x40];
static void __cdecl ManagerAdd(void* m, void* t) { Log("  manager add %s %s\n", Name(t).c_str(), Text(At<void*>(t, 0x10))); }
static void __cdecl ManagerRemove(void* m, void* t) { Log("  manager remove %s %s\n", Name(t).c_str(), Text(At<void*>(t, 0x10))); }
static void __cdecl ManagerActivate(void* m, void* t, int how) { Log("  manager activate %s %d\n", Name(t).c_str(), how); }
static void __cdecl ManagerDeleteThis(void*) { Log("  manager delete this\n"); }
static int __cdecl ManagerSlot(void*) { Log("  manager slot\n"); return 0; }
static void __cdecl SetTextureUpdate(void* m, void* t, bool on) { Log("  manager update %s %d\n", Name(t).c_str(), on); }

static unsigned char nullTable[29 * 36];

// a null API texture made (Create) by the side under test
static void* MakeApiTexture(std::mt19937& r)
{
    void* t = TestNew(0x70, "DT%d", made++);
    api[cur].nullCtor(t, nullptr, displayObj);
    unsigned char f[12];
    formatCtor(f, nullptr, r() % 2 ? 2 : 3);
    const int size[2] = {1 << (r() % 7), 1 << (r() % 7)};
    unsigned char rtc[0x10];
    rtcCtor(rtc, nullptr, 0, false, false, false, nullptr);
    using CreateFn = bool(__cdecl*)(void*, const int*, const void*, int, unsigned, int, unsigned, float, int, int, int,
                                   int, int, int, const void*);
    const int dim = r() % 4 ? 0 : 1;
    (*static_cast<CreateFn* const*>(t))[1](t, size, f, dim, 1 + r() % 4, 0, dim ? 6 : 1, 1.0f, 0, 1, 1, 1, 0, 0, rtc);
    rtcDtor(rtc, nullptr);
    return t;
}

// ---- the dumps

static void DumpTex(const char* tag, const void* t)
{
    Log("  %s %s: refs %d unused %d flags %d name %s frame %u texture %s alpha %d color %s manager %s\n", tag,
        Name(t).c_str(), At<int>(t, 4), At<int>(t, 8), At<unsigned char>(t, 0xc), Text(At<void*>(t, 0x10)),
        At<unsigned>(t, 0x14), Name(At<void*>(t, 0x18)).c_str(), At<int>(t, 0x1c),
        Hex(static_cast<const char*>(t) + 0x20, 16).c_str(), Name(At<void*>(t, 0x30)).c_str());
}

static void DumpGetters(const Api& a, const void* t)
{
    int size[2] = {-1, -1}, abs[2] = {-1, -1};
    float color[4];
    SC<void*>(t, 13, size);
    SC<void*>(t, 14, abs);
    SC<void*>(t, 10, color);
    Log("  getters %s: avail %d failure %d resname %s resource %s memory %lu alpha %d color %s dim %d depth %u size "
        "%dx%d abs %dx%d texture %s info %s name %s update %d active %d frame %u manager %s\n",
        Name(t).c_str(), SC<bool>(t, 3), SC<bool>(t, 4), Text(*SC<void* const*>(t, 6)), Name(SC<void*>(t, 7)).c_str(),
        SC<unsigned long>(t, 8), SC<int>(t, 9), Hex(color, 16).c_str(), SC<int>(t, 11), SC<unsigned>(t, 12), size[0],
        size[1], abs[0], abs[1], Name(SC<void*>(t, 21)).c_str(), Name(SC<void*>(t, 22)).c_str(), Text(*a.name(t)),
        a.getUpdate(t), a.active(t), a.frame(t), Name(a.manager(t)).c_str());
}

static void Release(void* t) { releaseRef(t, nullptr); }

// ---- the scenario

static const char* kNames[] = {"wheel", "cab.texture", "00000001%%wheel", "abcdef12%%x", "1234567%%short", "",
                               "%%", "deadbeef%%deadbeef%%twice", "ABCDEFGH%%up", "%%345678%%early"};

static void RunTex(int s, unsigned seed, unsigned long startId)
{
    std::mt19937 r(seed);
    standRng.seed(seed ^ 0x6d6d);
    const Api& a = api[s];
    made = 0;
    *uniqueTexId = startId;
    At<int>(managerObj, 4) = 1000;
    At<unsigned>(managerObj, 0x28) = r() % 4;
    std::vector<void*> tex;
    auto makeName = [&](const char* text) {
        return CreateNode(*cache, reinterpret_cast<const unsigned char*>(text));
    };
    auto newTex = [&]() {
        void* t = TestNew(0x34, "T%d", made++);
        void* name = makeName(kNames[r() % 10]);
        if (r() % 2) {
            Log("op new %s\n", Name(t).c_str());
            a.ctor(t, nullptr, &name, managerObj);
        } else {
            void* d = r() % 4 ? MakeApiTexture(r) : nullptr;
            const int alpha = static_cast<int>(r() % 5) - 1;
            Log("op new %s with %s alpha %d\n", Name(t).c_str(), Name(d).c_str(), alpha);
            a.ctor2(t, nullptr, &name, managerObj, d, alpha);
        }
        DestroyNode(*cache, name);
        DumpTex("made", t);
        tex.push_back(t);
    };
    newTex();
    for (int step = 0, steps = 6 + static_cast<int>(r() % 16); step < steps; ++step) {
        void* t = tex[r() % tex.size()];
        switch (r() % 16) {
        case 0:
            newTex();
            break;
        case 1: case 2:
            Log("op getters %s\n", Name(t).c_str());
            DumpGetters(a, t);
            break;
        case 3: {  // CreateInstance
            Log("op instance %s\n", Name(t).c_str());
            void* n = SC<void*>(t, 2);
            DumpTex("instance", n);
            tex.push_back(n);
            break;
        }
        case 4: {  // the names
            void* n = makeName(kNames[r() % 10]);
            void* out = nullptr;
            a.unique(t, &out, &n);
            void* out2 = nullptr;
            a.nonUnique(t, &out2);
            Log("op names %s: unique of %s %s, non-unique %s, id %lu\n", Name(t).c_str(), Text(n), Text(out),
                Text(out2), *uniqueTexId - startId);
            DestroyNode(*cache, n);
            if (out) DestroyNode(*cache, out);
            if (out2) DestroyNode(*cache, out2);
            break;
        }
        case 5: {  // SetName
            void* n = makeName(kNames[r() % 10]);
            Log("op set name %s %s\n", Name(t).c_str(), Text(n));
            SC<void>(t, 5, &n);
            DestroyNode(*cache, n);
            DumpTex("renamed", t);
            break;
        }
        case 6:  // Activate, Bind
            SC<void>(t, 15, static_cast<int>(r() % 3));
            if (At<void*>(t, 0x18)) SC<void>(t, 17, r() % 4);
            Log("op activate bind %s\n", Name(t).c_str());
            break;
        case 7: {  // DynamicSubload
            void* b = TestNew(0x44, "BMP%d", made++);
            const int size[2] = {1 << (r() % 4), 1 << (r() % 4)};
            bitmapCtor(b, nullptr, size, r() % 2 ? 2 : 3, nullptr);
            unsigned char f[12];
            formatCtor(f, nullptr, 3);
            const bool ok = SC<bool>(t, 19, static_cast<int>(r() % 4), static_cast<int>(r() % 4), b, f);
            Log("op dynamic subload %s: %d\n", Name(t).c_str(), ok);
            bitmapDtor(b, nullptr);
            HookDelete(b);
            break;
        }
        case 8: {  // SetDisplayTexture (not the one it has: that one would be freed and kept)
            void* d = r() % 4 ? MakeApiTexture(r) : nullptr;
            Log("op set display texture %s %s\n", Name(t).c_str(), Name(d).c_str());
            SC<void>(t, 20, d);
            DumpTex("set", t);
            break;
        }
        case 9: {  // frames
            a.setFrame(t, r() % 4);
            At<unsigned>(managerObj, 0x28) = r() % 4;
            Log("op frame %s: %u active %d\n", Name(t).c_str(), a.frame(t), a.active(t));
            break;
        }
        case 10:  // the update flag
            a.setUpdate(t, r() % 2 != 0);
            Log("op update %s: %d\n", Name(t).c_str(), a.getUpdate(t));
            break;
        case 11: {  // InitInstance into another
            void* o = tex[r() % tex.size()];
            if (o == t) break;
            if (At<void*>(t, 0x18) && !At<void*>(o, 0x18)) Log("  (copies the API texture)\n");
            Log("op init instance %s -> %s\n", Name(t).c_str(), Name(o).c_str());
            a.init(t, o);
            DumpTex("to", o);
            break;
        }
        case 12:  // the alpha hint, the colour (written by the test)
            a.setAlpha(t, nullptr, static_cast<int>(r() % 4));
            for (int k = 0; k < 4; ++k) At<float>(t, 0x20 + 4 * k) = static_cast<float>(r() % 256) / 255.0f;
            Log("op alpha %s\n", Name(t).c_str());
            DumpTex("alpha", t);
            break;
        case 13: {  // a reference more, then let one go
            if (tex.size() < 2) break;
            const size_t k = r() % tex.size();
            Log("op release %s\n", Name(tex[k]).c_str());
            Release(tex[k]);
            tex.erase(tex.begin() + static_cast<long>(k));
            break;
        }
        default:
            Log("op unused slots %s\n", Name(t).c_str());
            SC<void>(t, 16, 1.0f);
            SC<void>(t, 18, 2.0f);
            break;
        }
    }
    for (void* t : tex) {
        Log("op release %s\n", Name(t).c_str());
        Release(t);
    }
    Log("manager refs %d, id %lu\n", At<int>(managerObj, 4), *uniqueTexId - startId);
}

// ---- running

static void Compare(int round)
{
    ++checks;
    if (logText[0] == logText[1]) return;
    if (const char* dump = std::getenv("RS_U6C2_DUMP"); dump && !bad)
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
        std::printf("usage: diff_rs_u6c2 <bin dir> [rounds]\n");
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
    CreateNode = As<decltype(CreateNode)>(Proc(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    DestroyNode = As<decltype(DestroyNode)>(Proc(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z"));
    cache = static_cast<void**>(Proc(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    releaseRef = As<decltype(releaseRef)>(Proc(ks, "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ"));
    bitmapCtor = As<decltype(bitmapCtor)>(Proc(ks, "??0Bitmap@Jet@@QAE@ABUSize@1@W4Format@01@PAE@Z"));
    bitmapDtor = As<decltype(bitmapDtor)>(Proc(ks, "??1Bitmap@Jet@@QAE@XZ"));
    uniqueTexId = static_cast<unsigned long*>(Proc(mod[1], "?uniqueTexId@" TX "1KA"));
    formatCtor = As<decltype(formatCtor)>(Proc(mod[1], "??0DisplayFormat@Jet@@QAE@W4DFormatId@1@@Z"));
    rtcCtor = As<decltype(rtcCtor)>(Proc(mod[1], "??0RenderTextureConstruct@Jet@@QAE@W4TextureTarget@1@_N11PBVSharedContext@1@@Z"));
    rtcDtor = As<decltype(rtcDtor)>(Proc(mod[1], "??1RenderTextureConstruct@Jet@@QAE@XZ"));
    PatchJump(Proc(mod[1], "?SetTextureUpdate@TextureManager@Jet@@IAAXPAVTexture@2@_N@Z"), reinterpret_cast<void*>(&SetTextureUpdate));
    // the null format table: every format supported as it is
    unsigned char* table = reinterpret_cast<unsigned char*>(mod[1]) + 0xb8228;
    for (int id = 0; id < 29; ++id) {
        *reinterpret_cast<unsigned*>(table + id * 36) = 1;
        formatCtor(table + id * 36 + 0x18, nullptr, id);
    }
    for (void*& p : displayVt) p = reinterpret_cast<void*>(&DisplaySlot);
    displayVt[0x1c / 4] = reinterpret_cast<void*>(&DisplayQuery);
    displayVt[0x28 / 4] = reinterpret_cast<void*>(&DisplayMaxAniso);
    displayVt[0x98 / 4] = reinterpret_cast<void*>(&DisplayCreateTexture);
    displayVt[0xa0 / 4] = reinterpret_cast<void*>(&DisplaySetTexture);
    *reinterpret_cast<void**>(displayObj) = displayVt;
    fixedNames[displayObj] = "D";
    for (void*& p : managerVt) p = reinterpret_cast<void*>(&ManagerSlot);
    managerVt[0] = reinterpret_cast<void*>(&ManagerDeleteThis);
    managerVt[4] = reinterpret_cast<void*>(&ManagerAdd);
    managerVt[6] = reinterpret_cast<void*>(&ManagerRemove);
    managerVt[9] = reinterpret_cast<void*>(&ManagerActivate);
    *reinterpret_cast<void**>(managerObj) = managerVt;
    fixedNames[managerObj] = "M";
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
    }
    for (int round = 0; round < rounds; ++round) {
        const unsigned seed = rng();
        const unsigned long startId = rng();
        for (int s = 0; s < 2; ++s) {
            cur = s;
            logText[s].clear();
            names[s].clear();
            blockSize[s].clear();
            allocCount[s] = 0;
            RunTex(s, seed, startId);
            cur = -1;
        }
        Compare(round);
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
