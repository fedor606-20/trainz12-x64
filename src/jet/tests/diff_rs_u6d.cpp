// Differential test of TextureManager's containers, lifetime and downsize queue (U6d1a and part of U6d1b),
// ours against RenderSystem_7c_orig.dll in one process.
//
// As in diff_rs_u6c2: both modules' Jet::Alloc imports are hooked and logged, each scenario runs the same random
// script on both sides and the logs must be equal. Each side makes its own manager and drives it through its
// slots and exports with stand-in textures (copies of the original's Texture / TextureRes vftables, so that
// RemoveTexture's dynamic_cast sees their RTTI; the slots log), a stand-in RenderKernel (a Display answering slots
// 0x1c and 0x28, an OS whose options come from a random command line), a stand-in ResourceDatabase and resource.
// Update and SharedTextureCollection::GenerateSubstituteLoadTexture get jumps to stand-ins, so both
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
    void(__cdecl* downsize)(void*, const void*, int);
    void(__cdecl* cancelResize)(void*, const void*);
    bool(__cdecl* isResizing)(void*, const void*);
    void(__cdecl* addAsync)(void*, const void*, const void*, int, int);
    void(__cdecl* addDownsize)(void*, const void*, int);
    void(__cdecl* upsize)(void*, const void*, int);
    void(__cdecl* checkDownsize)(void*, void*);
    void(__cdecl* checkTextures)(void*);
    void(__cdecl* refresh)(void*);
    void(__cdecl* update)(void*);
    void(__fastcall* append)(void*, void*, void*, void*, int);
    void*(__cdecl* getAsync)(const void*);
    void(__cdecl* setAsync)(const void*, void*);
    const void*(__cdecl* maxSize)(const void*);
    int(__cdecl* diskMips)(const void*);
    const void*(__cdecl* resourceDatabase)(const void*);
    void(__cdecl* setDelayed)(bool);
    void(__fastcall* cleanup)(void*, void*);
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
    A(downsize, "?DownSizeTexture@" TM "QAAXPBVTextureRes@2@H@Z");
    A(cancelResize, "?CancelResizeTexture@" TM "QAAXPBVTextureRes@2@@Z");
    A(isResizing, "?IsTextureBeingResized@" TM "QAA_NPBVTextureRes@2@@Z");
    A(addAsync, "?AddAsyncTextureLoad@" TM "IAAXPBVTextureRes@2@PBVResourceDatabase@2@HH@Z");
    A(addDownsize, "?AddTextureDownSize@" TM "IAAXPBVTextureRes@2@H@Z");
    A(upsize, "?UpSizeTexture@" TM "QAAXPBVTextureRes@2@H@Z");
    A(checkDownsize, "?CheckNeedDownSize@" TM "IAAXAAVTextureManagerInfo@2@@Z");
    A(checkTextures, "?CheckTexturesNeedDownSize@" TM "IAAXXZ");
    A(refresh, "?RefreshFromChangedSettings@" TM "QAAXXZ");
    A(update, "?Update@" TM "IAAXXZ");
    A(append, "?AppendMips@" TM "IAEXPAVTextureRes@2@PAUTextureInfo@TextureResource@2@H@Z");
    A(getAsync, "?GetAsyncLoadInfo@TextureRes@Jet@@QBAPAVTextureAsyncLoadInfo@2@XZ");
    A(setAsync, "?SetAsyncLoadInfo@TextureRes@Jet@@QBAXPAVTextureAsyncLoadInfo@2@@Z");
    A(maxSize, "?GetMaxSizeOnDisk@TextureRes@Jet@@QBAABUSize@2@XZ");
    A(diskMips, "?GetMipLevelCountOnDisk@TextureRes@Jet@@QBAHXZ");
    A(resourceDatabase, "?GetResourceDatabase@TextureRes@Jet@@QBAPBVResourceDatabase@2@XZ");
    A(setDelayed, "?SetDelayedLoading@TextureRes@Jet@@SAX_N@Z");
    A(cleanup, "?CleanupAsyncTextureLoad@" TM "IAEXXZ");
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
static int genAnswer;
static void* originalSubstitute;
static unsigned char originalSubstituteBytes[5];
static void* ownSubstitute;
static unsigned char ownSubstituteBytes[5];
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

static void ExerciseDownsize(const Api& api, void* manager)
{
    static const int sizes[] = {-1, 127, 128, 255, 256, 512, 1024};
    unsigned char textures[24][0x60]{};
    unsigned char displays[24][0x38]{};
    Log("op downsize queue\n");
    for (int i = 0; i < 24; ++i) {
        void* const t = textures[i];
        names[cur][t] = "down" + std::to_string(i);
        At<void*>(t, 0) = &texVt[1][1];
        At<int>(t, 0x50) = i == 0 ? 1 : 12;
        At<void*>(t, 0x54) = i == 1 ? nullptr : displays[i];
        At<int>(displays[i], 0x20) = i == 2 ? 8 : i == 3 ? 6 : 12;
        api.downsize(manager, t, sizes[i % 7]);
        const bool resizing = api.isResizing(manager, t);
        Log("  %d resizing %d\n", i, resizing);
        if (resizing) api.downsize(manager, t, 128);  // a pending request is left alone
    }
    for (void* q : Items(At<void*>(manager, 0xac)))
        Log("  request %s db %s cancel %d level %d first %d state %d\n",
            Name(At<void*>(q, 0)).c_str(), Name(At<void*>(q, 4)).c_str(), At<bool>(q, 8),
            At<int>(q, 0xc), At<int>(q, 0x10), At<int>(q, 0x14));
    for (int parity = 1; parity >= 0; --parity)
        for (int i = parity; i < 24; i += 2) {
            api.cancelResize(manager, textures[i]);
            api.cancelResize(manager, textures[i]);
            Log("  %d after cancel %d\n", i, api.isResizing(manager, textures[i]));
        }
    Log("  queue remaining %zu\n", Items(At<void*>(manager, 0xac)).size());
    for (const auto& texture : textures) names[cur].erase(texture);
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

static void ExerciseAsyncQueue(const Api& api, void* manager)
{
    unsigned char textures[31][0x60]{};
    Log("op async queue\n");
    for (int i = 0; i < 31; ++i) {
        void* const t = textures[i];
        names[cur][t] = "async" + std::to_string(i);
        api.addAsync(manager, t, i % 3 ? dbObj : nullptr, i - 2, 30 - i);
        api.addAsync(manager, t, nullptr, 99, 99);
        Log("  %d pending %d\n", i, api.isResizing(manager, t));
    }
    Log("  chunks %s\n", Chunks(At<void*>(manager, 0x34)).c_str());
    for (void* q : Items(At<void*>(manager, 0x34))) {
        Log("  load %s db %s cancelled %d level %d first %d state %d\n",
            Name(At<void*>(q, 0)).c_str(), Name(At<void*>(q, 4)).c_str(), At<bool>(q, 8),
            At<int>(q, 0xc), At<int>(q, 0x10), At<int>(q, 0x14));
    }
    for (int parity = 1; parity >= 0; --parity)
        for (int i = parity; i < 31; i += 2) {
            api.cancelResize(manager, textures[i]);
            Log("  cancelled %d pending %d\n", i, api.isResizing(manager, textures[i]));
        }
    Log("  remaining %zu\n", Items(At<void*>(manager, 0x34)).size());
    for (const auto& texture : textures) names[cur].erase(texture);
}

static void ExerciseCleanup(const Api& api, void* manager)
{
    unsigned char textures[32][0x60]{};
    Log("op cleanup queues\n");
    for (int i = 0; i < 16; ++i) {
        api.addAsync(manager, textures[i], dbObj, i, i + 1);
        api.addDownsize(manager, textures[i + 16], i);
    }
    Log("  before async %s downsize %s\n", Chunks(At<void*>(manager, 0x34)).c_str(),
        Chunks(At<void*>(manager, 0xac)).c_str());
    api.cleanup(manager, nullptr);
    Log("  after async %s downsize %s\n", Chunks(At<void*>(manager, 0x34)).c_str(),
        Chunks(At<void*>(manager, 0xac)).c_str());
}

struct RunningLoad {
    volatile unsigned char* cancelled;
    volatile int* state;
};

static DWORD WINAPI FinishCancelledLoad(void* context)
{
    auto* load = static_cast<RunningLoad*>(context);
    while (!*load->cancelled) Sleep(1);
    *load->state = 4;
    return 0;
}

static void ExerciseActiveCancellation(const Api& api, void* manager)
{
    unsigned char texture[0x60]{};
    names[cur][texture] = "active";
    for (int state = 1; state <= 4; ++state) {
        Log("op cancel active state %d\n", state);
        void* request = TestNew(0x18, "activeRequest%d", state);
        At<void*>(request, 0) = texture;
        At<int>(request, 0x14) = 2;
        At<void*>(texture, 0x4c) = request;
        At<void*>(manager, 0x38) = request;
        At<int>(manager, 0xa4) = state;
        RunningLoad load{reinterpret_cast<volatile unsigned char*>(static_cast<char*>(request) + 8),
                         reinterpret_cast<volatile int*>(static_cast<char*>(manager) + 0xa4)};
        HANDLE worker = state == 1 ? CreateThread(nullptr, 0, FinishCancelledLoad, &load, 0, nullptr) : nullptr;
        api.cancelResize(manager, texture);
        if (worker) {
            WaitForSingleObject(worker, INFINITE);
            CloseHandle(worker);
        }
        Log("  request %d state %d pending %d\n", At<void*>(manager, 0x38) != nullptr,
            At<int>(manager, 0xa4), api.isResizing(manager, texture));
    }
    names[cur].erase(texture);
}

static const void* __cdecl UpsizeResource(const void*) { return resObj; }

static void ExerciseTextureResAccessors(const Api& api)
{
    unsigned char texture[0x60]{};
    void* vtable[32]{};
    vtable[7] = reinterpret_cast<void*>(&UpsizeResource);
    At<void*>(texture, 0) = vtable;
    At<int>(texture, 0x50) = 9;
    At<int>(texture, 0x54) = 1024;
    At<int>(texture, 0x58) = 512;
    At<void*>(resObj, 8) = dbObj;
    Log("op texture resource accessors\n");
    api.setAsync(texture, dbObj);
    const void* size = api.maxSize(texture);
    Log("  async %s size %d %d mips %d owner %s\n", Name(api.getAsync(texture)).c_str(),
        At<int>(size, 0), At<int>(size, 4), api.diskMips(texture), Name(api.resourceDatabase(texture)).c_str());
    api.setAsync(texture, nullptr);
    At<void*>(resObj, 8) = nullptr;
    Log("  cleared %d owner %s\n", api.getAsync(texture) == nullptr, Name(api.resourceDatabase(texture)).c_str());
    bool* delayed = static_cast<bool*>(Proc(mod[1], "?delayedLoading@TextureRes@Jet@@1_NA"));
    bool const old = *delayed;
    api.setDelayed(true);
    Log("  delayed %d\n", *delayed);
    api.setDelayed(false);
    Log("  delayed %d\n", *delayed);
    *delayed = old;
}

static bool __cdecl TextureResSubload(void*, unsigned level, const void* rect, void*, const void*, unsigned face)
{
    Log("  subload %u rect %d %d %d %d face %u\n", level, At<int>(rect, 0), At<int>(rect, 4),
        At<int>(rect, 8), At<int>(rect, 12), face);
    return true;
}

static bool __cdecl TextureResBind(void*, unsigned unit)
{
    Log("  texture bind unit %u\n", unit);
    return true;
}

static void __cdecl TextureResBindEmpty(void*, int value) { Log("  bind empty %d\n", value); }

static int forceLockResult, forceLoadResult;
static void* forceDisplay;
static void* __cdecl ForceLock(void*, void* result) { At<int>(result, 0) = forceLockResult; Log("  lock %d\n", forceLockResult); return result; }
static void __cdecl ForceUnlock(void*) { Log("  unlock\n"); }
static bool __cdecl ForceUnload(void*) { Log("  unload resource data\n"); return true; }
static void __cdecl ForceRemoveObserver(void*, void*) { Log("  remove observer\n"); }
static void* __cdecl ForceSize(void*, void* size) { At<int>(size, 0) = 1024; At<int>(size, 4) = 512; Log("  resource size\n"); return size; }
static int __cdecl ForceAlpha(void*) { Log("  resource alpha\n"); return 2; }
static void* __cdecl ForceColor(void*, void* color) { for (int i = 0; i < 4; ++i) At<int>(color, 4 * i) = 0x3f000000 + i; Log("  resource color\n"); return color; }
static void* __cdecl ForceLoad(void*, void* result, void*) { At<int>(result, 0) = forceLoadResult; Log("  load texture %d\n", forceLoadResult); return result; }
static void* __cdecl ForceCreateDisplay(void*) { Log("  create display %d\n", forceDisplay != nullptr); return forceDisplay; }
static void* __fastcall ForceDeleteDisplay(void* display, void*, unsigned) { Log("  delete display\n"); return display; }
static bool changeCreateResult;
static int __cdecl ChangeFormat(void*) { return 1; }
static bool __cdecl ChangeCreate(void*, const void* size, const void*, int dimension, unsigned levels, int automip,
                                 unsigned depth, float, int, int, int, int, int wrapS, int wrapT, const void*)
{
    Log("  create %d %d dimension %d levels %u automip %d depth %u wrap %d %d -> %d\n",
        At<int>(size, 0), At<int>(size, 4), dimension, levels, automip, depth, wrapS, wrapT, changeCreateResult);
    return changeCreateResult;
}
static void __cdecl ChangeSampler(void*, void*) { Log("  copy sampler\n"); }
static void __cdecl ChangeCopy(void*, void*, int from, void*, int to) { Log("  copy mip %d -> %d\n", from, to); }
static void __cdecl ChangeSet(void* texture, void* display) { Log("  set resized display\n"); At<void*>(texture, 0x18) = display; }

static void ExerciseChangeMipCount()
{
    auto change = As<void(__fastcall*)(void*, void*, const void*, int, bool)>(
        Proc(mod[cur], "?ChangeMipCount@TextureRes@Jet@@QAEXABUSize@2@H_N@Z"));
    unsigned char texture[0x60]{}, old[0x38]{}, replacement[0x38]{}, manager[0x20]{}, kernel[0x90]{}, device[4]{};
    void* textureVtable[32]{};
    void* oldVtable[20]{};
    void* newVtable[20]{};
    void* deviceVtable[0x30]{};
    textureVtable[20] = reinterpret_cast<void*>(&ChangeSet);
    oldVtable[9] = reinterpret_cast<void*>(&ChangeCopy);
    oldVtable[13] = reinterpret_cast<void*>(&ChangeFormat);
    newVtable[0] = reinterpret_cast<void*>(&ForceDeleteDisplay);
    newVtable[1] = reinterpret_cast<void*>(&ChangeCreate);
    newVtable[14] = reinterpret_cast<void*>(&ChangeSampler);
    deviceVtable[0x98 / 4] = reinterpret_cast<void*>(&ForceCreateDisplay);
    At<void*>(texture, 0) = textureVtable;
    At<void*>(texture, 0x30) = manager;
    At<void*>(manager, 0x14) = kernel;
    At<void*>(kernel, 0x7c) = device;
    At<void*>(device, 0) = deviceVtable;
    At<void*>(old, 0) = oldVtable;
    At<int>(old, 0x18) = 2;
    At<int>(old, 0x1c) = 6;
    At<float>(old, 0x28) = 2.0f;
    At<void*>(replacement, 0) = newVtable;
    forceDisplay = replacement;
    const int size[] = {256, 128};
    for (int mode = 0; mode < 5; ++mode) {
        const int oldLevels = mode == 0 ? 4 : mode == 1 ? 8 : 4;
        const int levels = mode == 0 ? 8 : mode == 1 ? 3 : mode == 2 ? 4 : 7;
        const bool copy = mode != 3;
        changeCreateResult = mode != 4;
        At<void*>(texture, 0x18) = old;
        At<int>(old, 0x20) = oldLevels;
        Log("op change mips mode %d\n", mode);
        change(texture, nullptr, size, levels, copy);
        Log("  replaced %d\n", At<void*>(texture, 0x18) == replacement);
    }
}

static void ExerciseForceTextureLoad()
{
    auto force = As<void(__cdecl*)(const void*)>(Proc(mod[cur], "?ForceTextureLoad@TextureRes@Jet@@IBAXXZ"));
    unsigned char texture[0x60]{}, resource[0x64]{}, manager[0x20]{}, kernel[0x90]{}, device[4]{}, display[0x38]{}, info[0x68]{};
    void* resourceVtable[48]{};
    void* deviceVtable[0x30]{};
    void* displayVtable[20]{};
    resourceVtable[0] = reinterpret_cast<void*>(&ForceLock);
    resourceVtable[1] = reinterpret_cast<void*>(&ForceUnlock);
    resourceVtable[6] = reinterpret_cast<void*>(&ForceRemoveObserver);
    resourceVtable[14] = reinterpret_cast<void*>(&ForceUnload);
    resourceVtable[20] = reinterpret_cast<void*>(&ForceAlpha);
    resourceVtable[21] = reinterpret_cast<void*>(&ForceColor);
    resourceVtable[24] = reinterpret_cast<void*>(&ForceSize);
    resourceVtable[28] = reinterpret_cast<void*>(&ForceLoad);
    deviceVtable[0x98 / 4] = reinterpret_cast<void*>(&ForceCreateDisplay);
    displayVtable[0] = reinterpret_cast<void*>(&ForceDeleteDisplay);
    At<void*>(resource, 0) = resourceVtable;
    At<void*>(device, 0) = deviceVtable;
    At<void*>(display, 0) = displayVtable;
    At<void*>(manager, 0x14) = kernel;
    At<void*>(kernel, 0x7c) = device;
    At<void*>(texture, 0x30) = manager;
    names[cur][resource] = "forceResource";
    At<int>(info, 0x60) = 4;
    for (int mode = 0; mode < 6; ++mode) {
        At<int>(texture, 0x38) = 0;
        At<void*>(texture, 0x3c) = mode == 0 ? nullptr : resource;
        At<void*>(texture, 0x18) = nullptr;
        forceLockResult = mode == 1;
        forceLoadResult = mode == 4;
        forceDisplay = mode >= 3 ? display : nullptr;
        At<void*>(resource, 0x5c) = mode == 5 ? info : nullptr;
        At<unsigned char>(resource, 0x60) = 0;
        Log("op force texture %d\n", mode);
        force(texture);
        Log("  state %d resource %d display %d size %d %d alpha %d mips %d unloaded %d\n", At<int>(texture, 0x38),
            At<void*>(texture, 0x3c) != nullptr, At<void*>(texture, 0x18) != nullptr,
            At<int>(texture, 0x40), At<int>(texture, 0x44), At<int>(texture, 0x1c),
            At<int>(texture, 0x50), At<unsigned char>(resource, 0x60));
    }
    names[cur].erase(resource);
}

static void __cdecl LifetimeAdd(void*, void*) { Log("  manager add\n"); }
static void __cdecl LifetimeRegister(void*, void*) { Log("  manager register\n"); }
static void __cdecl LifetimeRemove(void*, void*) { Log("  manager remove\n"); }

static void ExerciseTextureResLifetime(const Api&)
{
    using Ctor = void*(__fastcall*)(void*, void*, const void*, void*, const void*);
    auto ctor = As<Ctor>(Proc(mod[cur], "??0TextureRes@Jet@@QAE@ABVPString@1@PAVTextureManager@1@PBVTextureResource@1@@Z"));
    auto instance = As<void*(__cdecl*)(const void*)>(Proc(mod[cur], "?CreateInstance@TextureRes@Jet@@UBAPAVTexture@2@XZ"));
    auto* uniqueId = static_cast<unsigned*>(Proc(mod[1], "?uniqueTexId@Texture@Jet@@1KA"));
    const unsigned savedId = *uniqueId;
    *uniqueId = 1;
    unsigned char managerData[0xd8]{};
    void* managerVtable[12]{};
    managerVtable[4] = reinterpret_cast<void*>(&LifetimeAdd);
    managerVtable[5] = reinterpret_cast<void*>(&LifetimeRegister);
    managerVtable[6] = reinterpret_cast<void*>(&LifetimeRemove);
    At<void*>(managerData, 0) = managerVtable;
    At<int>(managerData, 4) = 20;
    void* manager = managerData;
    void* node = CreateNode(*cache, reinterpret_cast<const unsigned char*>("texture-resource"));
    void* resourceNode = CreateNode(*cache, reinterpret_cast<const unsigned char*>("source-resource"));
    At<void*>(resObj, 4) = resourceNode;
    At<int>(resObj, 0x1c) = 10;
    void* name = node;
    for (int withResource = 0; withResource < 2; ++withResource) {
        void* texture = TestNew(0x60, "realTexture%d", withResource);
        Log("op texture lifetime resource %d\n", withResource);
        ctor(texture, nullptr, &name, manager, withResource ? resObj : nullptr);
        void* info = SC<void*>(texture, 22);
        Log("  state %d refs %d source %s info %d owner %d\n", At<int>(texture, 0x38), At<int>(resObj, 0x1c),
            Text(At<void*>(texture, 0x48)), info != nullptr, info && At<void*>(info, 8) == texture);
        void* child = instance(texture);
        Log("  instance state %d size %d %d\n", At<int>(child, 0x38), At<int>(child, 0x40), At<int>(child, 0x44));
        releaseRef(child, nullptr);
        releaseRef(texture, nullptr);
    }
    DestroyNode(*cache, resourceNode);
    DestroyNode(*cache, node);
    At<void*>(resObj, 4) = nullptr;
    *uniqueId = savedId;
}

static void ExerciseSignedColor()
{
    using Convert = void*(__cdecl*)(void*, const float*);
    auto convert = As<Convert>(Proc(mod[cur], "?GetSignedColor@SharedTextureCollection@Jet@@KA?AUPackedColor@2@ABVVector3@2@@Z"));
    std::mt19937 random(0x425a);
    unsigned hash = 2166136261u;
    for (int i = 0; i < 2000; ++i) {
        float vector[3];
        for (float& component : vector) {
            const unsigned bits = random();
            std::memcpy(&component, &bits, sizeof component);
        }
        unsigned result = 0;
        convert(&result, vector);
        hash = (hash ^ result) * 16777619u;
    }
    Log("op signed colors hash %08x\n", hash);
}

static void* sharedFindResult;
static void* __cdecl SharedFind(void*, const void*) { Log("  find shared\n"); return sharedFindResult; }

static void ExerciseSharedTextures()
{
    const char* getName = "?GetTexture@SharedTextureCollection@Jet@@SA_NW4SharedTextureID@2@PAVTextureManager@2@AAV?$AutoReference@VTexture@Jet@@@2@@Z";
    const char* setName = "?SetTexture@SharedTextureCollection@Jet@@SA_NW4SharedTextureID@2@PAVTextureManager@2@V?$AutoReference@VTexture@Jet@@@2@@Z";
    auto get = As<bool(__cdecl*)(int, void*, void**)>(Proc(mod[cur], getName));
    auto set = As<bool(__cdecl*)(int, void*, void*)>(Proc(mod[cur], setName));
    auto clear = As<void(__cdecl*)()>(Proc(mod[cur], "?ClearTextureArray@SharedTextureCollection@Jet@@SAXXZ"));
    auto assign = As<void*(__fastcall*)(void*, void*, const void*)>(Proc(mod[cur], "??4SharedTextureCollection@Jet@@QAEAAV01@ABV01@@Z"));
    auto** cacheArray = static_cast<void**>(Proc(mod[1], "?textureArray@SharedTextureCollection@Jet@@1PAV?$AutoReference@VTexture@Jet@@@2@A"));
    void* saved[11];
    std::memcpy(saved, cacheArray, sizeof saved);
    std::memset(cacheArray, 0, sizeof saved);
    unsigned char textureA[0x60]{}, textureB[0x60]{}, manager[4]{};
    unsigned char collection = 42, other = 7;
    Log("op shared assignment %d value %d\n", assign(&collection, nullptr, &other) == &collection, collection);
    void* managerVtable[8]{};
    managerVtable[7] = reinterpret_cast<void*>(&SharedFind);
    At<void*>(manager, 0) = managerVtable;
    At<int>(textureA, 4) = 20;
    At<int>(textureB, 4) = 20;
    sharedFindResult = textureA;
    void* result = textureB;
    Log("op shared find and cache\n");
    bool ok = get(0, manager, &result);
    Log("  ok %d resultA %d cacheA %d refs %d %d\n", ok, result == textureA,
        cacheArray[0] == textureA, At<int>(textureA, 4), At<int>(textureB, 4));
    ok = get(0, manager, &result);
    Log("  hit %d resultA %d refs %d %d\n", ok, result == textureA, At<int>(textureA, 4), At<int>(textureB, 4));
    ok = get(4, manager, &result);
    Log("  other %d cache %d refs %d\n", ok, cacheArray[4] != nullptr, At<int>(textureA, 4));
    addRef(textureB, nullptr);
    ok = set(1, manager, textureB);
    Log("  set %d cacheB %d refs %d\n", ok, cacheArray[1] == textureB, At<int>(textureB, 4));
    addRef(textureB, nullptr);
    ok = set(11, manager, textureB);
    Log("  invalid %d refs %d\n", ok, At<int>(textureB, 4));
    clear();
    Log("  clear %d %d refs %d %d\n", cacheArray[0] == nullptr, cacheArray[1] == nullptr,
        At<int>(textureA, 4), At<int>(textureB, 4));
    std::memcpy(cacheArray, saved, sizeof saved);
}

static void* __cdecl GenerateDefaultStub(void*) { Log("  generate default\n"); return forceDisplay; }
static void* __cdecl GenerateBumpStub(void*) { Log("  generate bump\n"); return forceDisplay; }
static void* __cdecl GenerateD2Stub(void*, unsigned w, unsigned h) { Log("  generate attenuation %u %u\n", w, h); return forceDisplay; }
static void* __cdecl GenerateSpotStub(void*, unsigned w) { Log("  generate spot %u\n", w); return forceDisplay; }
static void* __cdecl GenerateNormalStub(void*, unsigned w) { Log("  generate normal %u\n", w); return forceDisplay; }
static void* __cdecl GeneratePowerStub(void*, unsigned w, float p) { Log("  generate power %u %.0f\n", w, p); return forceDisplay; }
static void* __cdecl GenerateSpecStub(void*, unsigned w, float p) { Log("  generate spec %u %.0f\n", w, p); return forceDisplay; }

static void ExerciseSharedGeneration(const Api& api)
{
    const char* symbols[] = {
        "?GenerateDefaultTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@@Z",
        "?GenerateDefaultBumpTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@@Z",
        "?GenerateD2AttenuationTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@II@Z",
        "?GenerateSpotCubeMapTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@I@Z",
        "?GenerateNormalCubeMapTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@I@Z",
        "?GeneratePowerTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@IM@Z",
        "?GenerateSpecPowerTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@IM@Z"};
    void* replacements[] = {reinterpret_cast<void*>(&GenerateDefaultStub), reinterpret_cast<void*>(&GenerateBumpStub),
        reinterpret_cast<void*>(&GenerateD2Stub), reinterpret_cast<void*>(&GenerateSpotStub),
        reinterpret_cast<void*>(&GenerateNormalStub), reinterpret_cast<void*>(&GeneratePowerStub),
        reinterpret_cast<void*>(&GenerateSpecStub)};
    void* functions[14];
    unsigned char saved[14][5];
    for (int i = 0; i < 7; ++i) {
        functions[i] = Proc(mod[1], symbols[i]);
        std::memcpy(saved[i], functions[i], 5);
        PatchJump(functions[i], replacements[i]);
    }
    for (int i = 0; i < 3; ++i) {
        functions[7 + i] = Proc(mod[0], symbols[i]);
        std::memcpy(saved[7 + i], functions[7 + i], 5);
        PatchJump(functions[7 + i], replacements[i]);
    }
    for (int i = 0; i < 2; ++i) {
        functions[10 + i] = Proc(mod[0], symbols[5 + i]);
        std::memcpy(saved[10 + i], functions[10 + i], 5);
        PatchJump(functions[10 + i], replacements[5 + i]);
    }
    functions[12] = Proc(mod[0], symbols[3]);
    std::memcpy(saved[12], functions[12], 5);
    PatchJump(functions[12], replacements[3]);
    functions[13] = Proc(mod[0], symbols[4]);
    std::memcpy(saved[13], functions[13], 5);
    PatchJump(functions[13], replacements[4]);
    using Generate = bool(__cdecl*)(int, const void*, void*, void**, unsigned, unsigned);
    auto generate = As<Generate>(Proc(mod[cur], "?GenerateBuiltInTexture@SharedTextureCollection@Jet@@SA_NW4SharedTextureID@2@ABVPString@2@PAVTextureManager@2@AAV?$AutoReference@VTexture@Jet@@@2@II@Z"));
    void* manager = TestNew(0xd8, "sharedManager%d", 0);
    api.ctor(manager, nullptr);
    unsigned char kernel[0x90]{}, display[0x38]{};
    void* displayVtable[20]{};
    displayVtable[0] = reinterpret_cast<void*>(&ForceDeleteDisplay);
    At<void*>(display, 0) = displayVtable;
    At<void*>(manager, 0x14) = kernel;
    At<void*>(kernel, 0x7c) = displayObj;
    void* node = CreateNode(*cache, reinterpret_cast<const unsigned char*>("shared-generated"));
    void* name = node;
    for (int id = 0; id <= 11; ++id) {
        void* result = nullptr;
        forceDisplay = display;
        genAnswer = id == 2 ? 0 : 1;
        Log("op generate shared %d\n", id);
        bool ok = generate(id, &name, manager, &result, 64, 32);
        Log("  generated %d result %d\n", ok, result != nullptr);
        if (result) releaseRef(result, nullptr);
    }
    releaseRef(manager, nullptr);
    DestroyNode(*cache, node);
    for (int i = 0; i < 14; ++i) {
        DWORD old;
        VirtualProtect(functions[i], 5, PAGE_EXECUTE_READWRITE, &old);
        std::memcpy(functions[i], saved[i], 5);
        VirtualProtect(functions[i], 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), functions[i], 5);
    }
}

static bool __cdecl DefaultTextureLoad(void*, unsigned level, void* format, const void* bitmap, unsigned face)
{
    const auto* pixels = static_cast<const unsigned char*>(At<void*>(bitmap, 0xc));
    Log("  upload %u format %u size %d %d pixel %02x %02x %02x face %u\n", level,
        At<unsigned char>(format, 0), At<int>(bitmap, 0), At<int>(bitmap, 4),
        pixels[0], pixels[1], pixels[2], face);
    return true;
}

static void ExerciseDefaultSharedTextures()
{
    using Generate = void*(__cdecl*)(void*);
    auto normal = As<Generate>(Proc(mod[cur], "?GenerateDefaultTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@@Z"));
    auto bump = As<Generate>(Proc(mod[cur], "?GenerateDefaultBumpTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@@Z"));
    unsigned char device[4]{}, display[0x38]{};
    void* deviceVtable[0x30]{};
    void* displayVtable[20]{};
    deviceVtable[0x98 / 4] = reinterpret_cast<void*>(&ForceCreateDisplay);
    displayVtable[1] = reinterpret_cast<void*>(&ChangeCreate);
    displayVtable[3] = reinterpret_cast<void*>(&DefaultTextureLoad);
    At<void*>(device, 0) = deviceVtable;
    At<void*>(display, 0) = displayVtable;
    for (int mode = 0; mode < 2; ++mode) {
        for (int created = 0; created < 2; ++created) {
            forceDisplay = display;
            changeCreateResult = created != 0;
            Log("op generate fixed texture %d create %d\n", mode, created);
            void* result = mode ? bump(device) : normal(device);
            Log("  result %d\n", result == display);
        }
    }
    forceDisplay = nullptr;
    Log("op generate fixed texture without display\n");
    Log("  result %d\n", normal(device) != nullptr);
}

static int failSharedLevel;
static bool __cdecl SubstituteTextureLoad(void*, unsigned level, void*, const void* bitmap, unsigned)
{
    unsigned hash = 2166136261u;
    const int width = At<int>(bitmap, 0);
    const int height = At<int>(bitmap, 4);
    const auto* pixels = static_cast<const unsigned char*>(At<void*>(bitmap, 0xc));
    for (int i = 0; i < width * height * 4; ++i) hash = (hash ^ pixels[i]) * 16777619u;
    Log("  upload mip %u %d %d %08x\n", level, width, height, hash);
    return static_cast<int>(level) != failSharedLevel;
}

static void ExerciseSubstituteTexture()
{
    using Generate = void*(__cdecl*)(void*, unsigned, unsigned);
    auto generate = As<Generate>(Proc(mod[cur], "?GenerateSubstituteLoadTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@II@Z"));
    unsigned char device[4]{}, display[0x38]{};
    void* deviceVtable[0x30]{};
    void* displayVtable[20]{};
    deviceVtable[0x98 / 4] = reinterpret_cast<void*>(&ForceCreateDisplay);
    displayVtable[0] = reinterpret_cast<void*>(&ForceDeleteDisplay);
    displayVtable[1] = reinterpret_cast<void*>(&ChangeCreate);
    displayVtable[3] = reinterpret_cast<void*>(&SubstituteTextureLoad);
    At<void*>(device, 0) = deviceVtable;
    At<void*>(display, 0) = displayVtable;
    DWORD protection;
    VirtualProtect(originalSubstitute, 5, PAGE_EXECUTE_READWRITE, &protection);
    std::memcpy(originalSubstitute, originalSubstituteBytes, 5);
    VirtualProtect(originalSubstitute, 5, protection, &protection);
    FlushInstructionCache(GetCurrentProcess(), originalSubstitute, 5);
    VirtualProtect(ownSubstitute, 5, PAGE_EXECUTE_READWRITE, &protection);
    std::memcpy(ownSubstitute, ownSubstituteBytes, 5);
    VirtualProtect(ownSubstitute, 5, protection, &protection);
    FlushInstructionCache(GetCurrentProcess(), ownSubstitute, 5);
    for (int mode = 0; mode < 4; ++mode) {
        forceDisplay = display;
        changeCreateResult = mode != 1;
        failSharedLevel = mode == 2 ? 1 : -1;
        const unsigned width = mode == 3 ? 8 : 4;
        const unsigned height = 4;
        Log("op substitute texture mode %d\n", mode);
        void* result = generate(device, width, height);
        Log("  result %d\n", result == display);
    }
    forceDisplay = nullptr;
    Log("op substitute without display\n");
    Log("  result %d\n", generate(device, 4, 4) != nullptr);
    PatchJump(originalSubstitute, reinterpret_cast<void*>(&GenerateStub));
    PatchJump(ownSubstitute, reinterpret_cast<void*>(&GenerateStub));
}

static bool d2LoadResult;
static bool __cdecl D2TextureLoad(void*, unsigned, void*, const void* bitmap, unsigned)
{
    unsigned hash = 2166136261u;
    const int width = At<int>(bitmap, 0), height = At<int>(bitmap, 4), pitch = At<int>(bitmap, 0x10);
    const auto* pixels = static_cast<const unsigned char*>(At<void*>(bitmap, 0xc));
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width * 3; ++x) hash = (hash ^ pixels[y * pitch + x]) * 16777619u;
    Log("  upload d2 %d %d %08x -> %d\n", width, height, hash, d2LoadResult);
    return d2LoadResult;
}

static void ExerciseD2Texture()
{
    using Generate = void*(__cdecl*)(void*, unsigned, unsigned);
    auto generate = As<Generate>(Proc(mod[cur], "?GenerateD2AttenuationTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@II@Z"));
    unsigned char device[4]{}, display[0x38]{};
    void* deviceVtable[0x30]{};
    void* displayVtable[20]{};
    deviceVtable[0x98 / 4] = reinterpret_cast<void*>(&ForceCreateDisplay);
    displayVtable[0] = reinterpret_cast<void*>(&ForceDeleteDisplay);
    displayVtable[1] = reinterpret_cast<void*>(&ChangeCreate);
    displayVtable[3] = reinterpret_cast<void*>(&D2TextureLoad);
    At<void*>(device, 0) = deviceVtable;
    At<void*>(display, 0) = displayVtable;
    const unsigned dimensions[][2] = {{0, 0}, {4, 4}, {17, 9}, {64, 32}, {1, 3}};
    for (const auto& dimensionsPair : dimensions) {
        for (int mode = 0; mode < 4; ++mode) {
            forceDisplay = mode == 3 ? nullptr : display;
            changeCreateResult = mode != 1;
            d2LoadResult = mode != 2;
            Log("op d2 texture %u %u mode %d\n", dimensionsPair[0], dimensionsPair[1], mode);
            void* result = generate(device, dimensionsPair[0], dimensionsPair[1]);
            Log("  result %d\n", result != nullptr);
        }
    }
}

static int failCubeFace;
static bool __cdecl SpotTextureLoad(void*, unsigned, void*, const void* bitmap, unsigned face)
{
    unsigned hash = 2166136261u;
    const int width = At<int>(bitmap, 0), height = At<int>(bitmap, 4), pitch = At<int>(bitmap, 0x10);
    const auto* pixels = static_cast<const unsigned char*>(At<void*>(bitmap, 0xc));
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width * 3; ++x) hash = (hash ^ pixels[y * pitch + x]) * 16777619u;
    Log("  upload spot face %u %d %d %08x\n", face, width, height, hash);
    return static_cast<int>(face) != failCubeFace;
}

static void ExerciseSpotTexture()
{
    using Generate = void*(__cdecl*)(void*, unsigned);
    auto generate = As<Generate>(Proc(mod[cur], "?GenerateSpotCubeMapTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@I@Z"));
    unsigned char device[4]{}, display[0x38]{};
    void* deviceVtable[0x30]{};
    void* displayVtable[20]{};
    deviceVtable[0x98 / 4] = reinterpret_cast<void*>(&ForceCreateDisplay);
    displayVtable[0] = reinterpret_cast<void*>(&ForceDeleteDisplay);
    displayVtable[1] = reinterpret_cast<void*>(&ChangeCreate);
    displayVtable[3] = reinterpret_cast<void*>(&SpotTextureLoad);
    At<void*>(device, 0) = deviceVtable;
    At<void*>(display, 0) = displayVtable;
    for (unsigned width : {0u, 1u, 4u, 7u, 32u, 63u}) {
        for (int mode = 0; mode < 4; ++mode) {
            forceDisplay = mode == 3 ? nullptr : display;
            changeCreateResult = mode != 1;
            failCubeFace = mode == 2 ? 3 : -1;
            Log("op spot texture %u mode %d\n", width, mode);
            void* result = generate(device, width);
            Log("  result %d\n", result != nullptr);
        }
    }
}

static bool __cdecl NormalTextureLoad(void*, unsigned, void*, const void* bitmap, unsigned face)
{
    unsigned hash = 2166136261u;
    const int width = At<int>(bitmap, 0), height = At<int>(bitmap, 4), pitch = At<int>(bitmap, 0x10);
    const auto* pixels = static_cast<const unsigned char*>(At<void*>(bitmap, 0xc));
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width * 4; ++x) hash = (hash ^ pixels[y * pitch + x]) * 16777619u;
    Log("  upload normal face %u %d %d %08x\n", face, width, height, hash);
    return static_cast<int>(face) != failCubeFace;
}

static void ExerciseNormalTexture()
{
    using Generate = void*(__cdecl*)(void*, unsigned);
    auto generate = As<Generate>(Proc(mod[cur], "?GenerateNormalCubeMapTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@I@Z"));
    unsigned char device[4]{}, display[0x38]{};
    void* deviceVtable[0x30]{}, *displayVtable[20]{};
    deviceVtable[0x98 / 4] = reinterpret_cast<void*>(&ForceCreateDisplay);
    displayVtable[0] = reinterpret_cast<void*>(&ForceDeleteDisplay);
    displayVtable[1] = reinterpret_cast<void*>(&ChangeCreate);
    displayVtable[3] = reinterpret_cast<void*>(&NormalTextureLoad);
    At<void*>(device, 0) = deviceVtable;
    At<void*>(display, 0) = displayVtable;
    for (unsigned width : {1u, 4u, 7u, 16u, 32u, 0u}) {
        for (int mode = 0; mode < 3; ++mode) {
            forceDisplay = display;
            changeCreateResult = mode != 1;
            failCubeFace = mode == 2 ? 3 : -1;
            Log("op normal texture %u mode %d\n", width, mode);
            void* result = generate(device, width);
            Log("  result %d\n", result != nullptr);
        }
    }
}

static bool __cdecl PowerTextureLoad(void*, unsigned, void* format, const void* bitmap, unsigned)
{
    const unsigned formatId = At<unsigned char>(format, 0);
    const int width = At<int>(bitmap, 0);
    const auto* pixels = static_cast<const unsigned char*>(At<void*>(bitmap, 0xc));
    const int count = width * (formatId == 14 ? 2 : 1);
    unsigned hash = 2166136261u;
    for (int i = 0; i < count; ++i) hash = (hash ^ pixels[i]) * 16777619u;
    Log("  upload power format %u width %d hash %08x\n", formatId, width, hash);
    return true;
}

static void ExercisePowerTextures()
{
    using Generate = void*(__cdecl*)(void*, unsigned, float);
    auto power = As<Generate>(Proc(mod[cur], "?GeneratePowerTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@IM@Z"));
    auto spec = As<Generate>(Proc(mod[cur], "?GenerateSpecPowerTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@IM@Z"));
    unsigned char device[4]{}, display[0x38]{};
    void* deviceVtable[0x30]{};
    void* displayVtable[20]{};
    deviceVtable[0x98 / 4] = reinterpret_cast<void*>(&ForceCreateDisplay);
    displayVtable[1] = reinterpret_cast<void*>(&ChangeCreate);
    displayVtable[3] = reinterpret_cast<void*>(&PowerTextureLoad);
    At<void*>(device, 0) = deviceVtable;
    At<void*>(display, 0) = displayVtable;
    forceDisplay = display;
    for (int mode = 0; mode < 2; ++mode) {
        for (unsigned width : {0u, 1u, 2u, 17u, 64u, 73u, 512u}) {
            for (float exponent : {2.5f, 8.0f, 16.0f, 32.0f}) {
                changeCreateResult = true;
                Log("op power texture %d %u %.1f\n", mode, width, exponent);
                void* result = (mode ? spec : power)(device, width, exponent);
                Log("  result %d\n", result == display);
            }
        }
    }
}

static void MaterialChunk(void* object, unsigned offset, unsigned entrySize, int seed)
{
    unsigned char* previous = nullptr;
    unsigned char* first = nullptr;
    for (int block = 0; block < 2; ++block) {
        auto* chunk = static_cast<unsigned char*>(RealNew(12 + entrySize * 7));
        std::memset(chunk, 0, 12 + entrySize * 7);
        if (!first) first = chunk;
        if (previous) At<void*>(previous, 0) = chunk;
        At<void*>(chunk, 4) = previous;
        At<unsigned>(chunk, 8) = block + 1;
        for (int i = 0; i <= block; ++i) {
            auto* item = chunk + 12 + i * entrySize;
            char value[32];
            std::snprintf(value, sizeof value, "mat%d-%d-%d", seed, block, i);
            if (entrySize == 8) {
                At<void*>(item, 0) = CreateNode(*cache, reinterpret_cast<const unsigned char*>(value));
                At<void*>(item, 4) = CreateNode(*cache, reinterpret_cast<const unsigned char*>("texture"));
            } else {
                At<int>(item, 0) = seed + block;
                At<void*>(item, 4) = CreateNode(*cache, reinterpret_cast<const unsigned char*>(value));
                At<int>(item, 8) = 1234 + i;
            }
        }
        previous = chunk;
    }
    At<void*>(previous, 0) = first;
    At<void*>(first, 4) = previous;
    At<void*>(object, offset) = first;
}

static void LogMaterial(const unsigned char* object)
{
    Log("  strings %s %s bytes", Text(At<void*>(object, 0)), Text(At<void*>(object, 4)));
    for (unsigned i = 8; i < 0x6c; ++i)
        if (i < 0x5c || i >= 0x64) Log(" %02x", object[i]);
    Log("\n");
    for (unsigned offset : {0x5cu, 0x60u}) {
        auto* first = At<unsigned char*>(object, offset);
        auto* chunk = first;
        if (!chunk) continue;
        do {
            const unsigned count = At<unsigned>(chunk, 8);
            Log("  list %x count %u", offset, count);
            for (unsigned i = 0; i < count; ++i) {
                auto* item = chunk + 12 + i * (offset == 0x5c ? 8 : 12);
                if (offset == 0x5c) Log(" %s/%s", Text(At<void*>(item, 0)), Text(At<void*>(item, 4)));
                else Log(" %d/%s/%d", At<int>(item, 0), Text(At<void*>(item, 4)), At<int>(item, 8));
            }
            Log("\n");
            chunk = At<unsigned char*>(chunk, 0);
        } while (chunk != first);
    }
}

static void ExerciseMaterialSpec()
{
    using Ctor0 = void*(__fastcall*)(void*, void*);
    using Ctor1 = void*(__fastcall*)(void*, void*, const void*);
    using Dtor = void(__fastcall*)(void*, void*);
    auto ctor = As<Ctor0>(Proc(mod[cur], "??0MaterialSpec@Jet@@QAE@XZ"));
    auto named = As<Ctor1>(Proc(mod[cur], "??0MaterialSpec@Jet@@QAE@ABVPString@1@@Z"));
    auto copy = As<Ctor1>(Proc(mod[cur], "??0MaterialSpec@Jet@@QAE@ABU01@@Z"));
    auto assign = As<Ctor1>(Proc(mod[cur], "??4MaterialSpec@Jet@@QAEAAU01@ABU01@@Z"));
    auto dtor = As<Dtor>(Proc(mod[cur], "??1MaterialSpec@Jet@@QAE@XZ"));
    alignas(4) unsigned char source[0x6c], destination[0x6c], copied[0x6c];
    std::memset(source, 0xa5, sizeof source);
    std::memset(destination, 0xa5, sizeof destination);
    std::memset(copied, 0xa5, sizeof copied);
    void* node = CreateNode(*cache, reinterpret_cast<const unsigned char*>("named material"));
    named(source, nullptr, &node);
    DestroyNode(*cache, node);
    ctor(destination, nullptr);
    Log("op material constructors\n");
    LogMaterial(source);
    LogMaterial(destination);
    At<void*>(source, 4) = CreateNode(*cache, reinterpret_cast<const unsigned char*>("shader name"));
    At<int>(source, 0x4c) = 17;
    At<int>(source, 0x50) = 29;
    At<int>(source, 0x54) = 35;
    source[0x58] = 0x27;
    source[0x67] = 1;
    MaterialChunk(source, 0x5c, 8, 10);
    MaterialChunk(source, 0x60, 12, 20);
    MaterialChunk(destination, 0x5c, 8, 30);
    MaterialChunk(destination, 0x60, 12, 40);
    Log("op material copy\n");
    copy(copied, nullptr, source);
    LogMaterial(copied);
    Log("op material assignment\n");
    assign(destination, nullptr, source);
    LogMaterial(destination);
    Log("op material destruction\n");
    dtor(copied, nullptr);
    dtor(destination, nullptr);
    dtor(source, nullptr);
}

static void ExerciseMaterialFields()
{
    using Ptr = void*(__cdecl*)(void*);
    using Getter = unsigned(__cdecl*)(void*);
    using FloatGetter = float(__cdecl*)(void*);
    using FlagGetter = bool(__cdecl*)(void*);
    using Copy = void(__cdecl*)(void*, void*);
    using SetFloat = void(__cdecl*)(void*, float);
    using SetFlag = void(__cdecl*)(void*, bool);
    using SetBlend = void(__cdecl*)(void*, int, int);
    using GetBlend = void(__cdecl*)(void*, int*, int*);
    alignas(4) unsigned char object[0xa0], input[0x44], chunkA[12]{}, chunkB[12]{};
    for (unsigned i = 0; i < sizeof object; ++i) object[i] = static_cast<unsigned char>(i * 7);
    for (unsigned i = 0; i < sizeof input; ++i) input[i] = static_cast<unsigned char>(255 - i);
    At<void*>(chunkA, 0) = chunkB; At<void*>(chunkB, 0) = chunkA;
    At<unsigned>(chunkA, 8) = 3; At<unsigned>(chunkB, 8) = 5;
    At<void*>(object, 0x88) = chunkA; At<void*>(object, 0x98) = input;
    const char* pointers[] = {
        "??BMaterial@Jet@@QAAAAUDisplayMaterial@1@XZ", "??BMaterial@Jet@@QAAPAUDisplayMaterial@1@XZ",
        "??BMaterial@Jet@@QBAABUDisplayMaterial@1@XZ", "??BMaterial@Jet@@QBAPBUDisplayMaterial@1@XZ",
        "?GetAmbientColor@Material@Jet@@QAAAAVColor@2@XZ", "?GetAmbientColor@Material@Jet@@QBAABVColor@2@XZ",
        "?GetDiffuseColor@Material@Jet@@QAAAAVColor@2@XZ", "?GetDiffuseColor@Material@Jet@@QBAABVColor@2@XZ",
        "?GetDisplayMaterialProperties@Material@Jet@@QAAAAUDisplayMaterial@2@XZ", "?GetDisplayMaterialProperties@Material@Jet@@QBAABUDisplayMaterial@2@XZ",
        "?GetEmissiveColor@Material@Jet@@QAAAAVColor@2@XZ", "?GetEmissiveColor@Material@Jet@@QBAABVColor@2@XZ",
        "?GetOpacity@Material@Jet@@QAAAAMXZ", "?GetShininess@Material@Jet@@QAAAAMXZ",
        "?GetSpecularColor@Material@Jet@@QAAAAVColor@2@XZ", "?GetSpecularColor@Material@Jet@@QBAABVColor@2@XZ",
        "?GetPropertyList@Material@Jet@@QAAAAV?$DynamicArray@UPropertyDef@Material@Jet@@$06U?$Less@UPropertyDef@Material@Jet@@U123@@3@@2@XZ",
        "?GetPropertyList@Material@Jet@@QBAABV?$DynamicArray@UPropertyDef@Material@Jet@@$06U?$Less@UPropertyDef@Material@Jet@@U123@@3@@2@XZ",
        "?GetTextureList@Material@Jet@@QAAAAV?$DynamicArray@UTextureDef@Material@Jet@@$06U?$Less@UTextureDef@Material@Jet@@U123@@3@@2@XZ",
        "?GetTextureList@Material@Jet@@QBAABV?$DynamicArray@UTextureDef@Material@Jet@@$06U?$Less@UTextureDef@Material@Jet@@U123@@3@@2@XZ",
        "?GetDecoratedName@Material@Jet@@QAAAAVPString@2@XZ", "?GetDecoratedName@Material@Jet@@QBAABVPString@2@XZ",
        "?GetUndecoratedName@Material@Jet@@QAAAAVPString@2@XZ", "?GetUndecoratedName@Material@Jet@@QBAABVPString@2@XZ",
        "?GetCreationType@Material@Jet@@QAAAAVPString@2@XZ", "?GetCreationType@Material@Jet@@QBAABVPString@2@XZ",
        "?GetRequestName@Material@Jet@@QBAABVPString@2@XZ",
        "?MaterialProperties@Material@Jet@@IBAAAUDisplayMaterial@2@XZ"};
    Log("op material field references");
    for (auto name : pointers) Log(" %d", static_cast<unsigned char*>(As<Ptr>(Proc(mod[cur], name))(object)) - object);
    Log(" manager %d\n", As<Ptr>(Proc(mod[cur], "?GetMaterialManager@Material@Jet@@QBAPAVMaterialManager@2@XZ"))(object) == input);
    Log("  counts %u %u\n", As<Getter>(Proc(mod[cur], "?GetNumProperties@Material@Jet@@QBAIXZ"))(object),
        As<Getter>(Proc(mod[cur], "?GetNumTextures@Material@Jet@@QBAIXZ"))(object));
    At<void*>(object, 0x88) = nullptr;
    Log("  empty count %u\n", As<Getter>(Proc(mod[cur], "?GetNumProperties@Material@Jet@@QBAIXZ"))(object));
    const char* copies[] = {"?SetAmbientColor@Material@Jet@@QAAXABVColor@2@@Z", "?SetDiffuseColor@Material@Jet@@QAAXABVColor@2@@Z",
        "?SetSpecularColor@Material@Jet@@QAAXABVColor@2@@Z", "?SetEmissiveColor@Material@Jet@@QAAXABVColor@2@@Z",
        "?SetDisplayMaterialProperties@Material@Jet@@QAAXABUDisplayMaterial@2@@Z"};
    for (auto name : copies) {
        As<Copy>(Proc(mod[cur], name))(object, input);
        unsigned hash = 2166136261u;
        for (unsigned i = 0x34; i < 0x78; ++i) hash = (hash ^ object[i]) * 16777619u;
        Log("  copy hash %08x\n", hash);
    }
    As<SetFloat>(Proc(mod[cur], "?SetOpacity@Material@Jet@@QAAXM@Z"))(object, 0.375f);
    As<SetFloat>(Proc(mod[cur], "?SetResourcePriority@Material@Jet@@UBAXM@Z"))(object, -5.25f);
    Log("  floats %.3f %.3f\n", As<FloatGetter>(Proc(mod[cur], "?GetOpacity@Material@Jet@@QBAMXZ"))(object),
        As<FloatGetter>(Proc(mod[cur], "?GetResourcePriority@Material@Jet@@QBAMXZ"))(object));
    auto setBlend = As<SetBlend>(Proc(mod[cur], "?SetBlendMode@Material@Jet@@QAAXW4Factor@Src@2@W43Dst@2@@Z"));
    auto getBlend = As<GetBlend>(Proc(mod[cur], "?GetBlendMode@Material@Jet@@QBAXAAW4Factor@Src@2@AAW43Dst@2@@Z"));
    auto two = As<SetFlag>(Proc(mod[cur], "?SetTwoSided@Material@Jet@@QAAX_N@Z"));
    auto z = As<SetFlag>(Proc(mod[cur], "?SetZBiased@Material@Jet@@QAAX_N@Z"));
    const char* flags[] = {"?GetTwoSided@Material@Jet@@QBA_NXZ", "?GetZBiased@Material@Jet@@QBA_NXZ",
        "?IsFallback@Material@Jet@@QBA_NXZ", "?IsStandardBlendMode@Material@Jet@@QBA_NXZ"};
    for (int mode = 0; mode < 4; ++mode) {
        At<unsigned char>(object, 0x84) = mode & 1 ? 0xf0 : 0x0f;
        two(object, (mode & 1) != 0); z(object, (mode & 2) != 0);
        setBlend(object, mode == 0 ? 1 : mode, mode == 0 ? 0 : 2);
        As<void(__cdecl*)(void*)>(Proc(mod[cur], "?SetIsFallback@Material@Jet@@IAAXXZ"))(object);
        int src = -1, dst = -1; getBlend(object, &src, &dst);
        const unsigned dstGetter = As<Getter>(Proc(mod[cur], "?GetDstBlendMode@Material@Jet@@QBA?AW4Factor@Dst@2@XZ"))(object);
        if (dstGetter != static_cast<unsigned>(dst)) Log(" dst-mismatch %u %d", dstGetter, dst);
        Log("  flags %02x blend %d %d src %u", object[0x84], src, dst,
            As<Getter>(Proc(mod[cur], "?GetSrcBlendMode@Material@Jet@@QBA?AW4Factor@Src@2@XZ"))(object));
        for (auto name : flags) Log(" %d", As<FlagGetter>(Proc(mod[cur], name))(object));
        Log("\n");
    }
    unsigned char manager[0x20]{};
    using Init = bool(__cdecl*)(void*, void*);
    auto init = As<Init>(Proc(mod[cur], "?Init@MaterialManager@Jet@@UAA_NPAVRenderKernel@2@@Z"));
    auto defaultName = As<Ptr>(Proc(mod[cur], "?GetDefaultMaterialType@MaterialManager@Jet@@QBAABVPString@2@XZ"));
    Log("  manager init %d ", init(manager, input));
    Log("kernel %d name-offset %d\n", At<void*>(manager, 8) == input, static_cast<unsigned char*>(defaultName(manager)) - manager);
    void* string = CreateNode(*cache, reinterpret_cast<const unsigned char*>("next name"));
    At<void*>(object, 0x2c) = CreateNode(*cache, reinterpret_cast<const unsigned char*>("old type"));
    At<void*>(object, 0x30) = CreateNode(*cache, reinterpret_cast<const unsigned char*>("old request"));
    At<void*>(manager, 0x10) = CreateNode(*cache, reinterpret_cast<const unsigned char*>("old default"));
    As<Copy>(Proc(mod[cur], "?SetCreationType@Material@Jet@@QAAXABVPString@2@@Z"))(object, &string);
    As<Copy>(Proc(mod[cur], "?SetRequestName@Material@Jet@@QAAXABVPString@2@@Z"))(object, &string);
    As<Copy>(Proc(mod[cur], "?SetDefaultMaterialType@MaterialManager@Jet@@QAAXABVPString@2@@Z"))(manager, &string);
    Log("  names %s %s %s\n", Text(At<void*>(object, 0x2c)), Text(At<void*>(object, 0x30)), Text(At<void*>(manager, 0x10)));
    DestroyNode(*cache, At<void*>(object, 0x2c));
    DestroyNode(*cache, At<void*>(object, 0x30));
    DestroyNode(*cache, At<void*>(manager, 0x10));
    DestroyNode(*cache, string);
    using Property = void*(__cdecl*)(void*, const void*);
    using PropertyFound = void*(__cdecl*)(void*, const void*, bool*);
    auto property = As<Property>(Proc(mod[cur], "?GetPropertyString@Material@Jet@@UBAABVPString@2@ABV32@@Z"));
    auto propertyFound = As<PropertyFound>(Proc(mod[cur], "?GetPropertyString@Material@Jet@@UBAABVPString@2@ABV32@AA_N@Z"));
    alignas(4) unsigned char propA[28]{}, propB[20]{};
    At<void*>(propA, 0) = propB; At<void*>(propB, 0) = propA;
    At<unsigned>(propA, 8) = 2; At<unsigned>(propB, 8) = 1;
    void* keys[4]{};
    const char* words[] = {"first", "second", "third", "absent"};
    for (int i = 0; i < 4; ++i) keys[i] = CreateNode(*cache, reinterpret_cast<const unsigned char*>(words[i]));
    At<void*>(propA, 12) = keys[0]; At<void*>(propA, 16) = keys[1];
    At<void*>(propA, 20) = keys[1]; At<void*>(propA, 24) = keys[2];
    At<void*>(propB, 12) = keys[2]; At<void*>(propB, 16) = keys[0];
    At<void*>(object, 0x88) = propA;
    for (int i = 0; i < 4; ++i) {
        bool found = false;
        auto* result = static_cast<void**>(propertyFound(object, &keys[i], &found));
        auto* simple = static_cast<void**>(property(object, &keys[i]));
        Log("  property %s found %d matches %d value %s\n", words[i], found, result == simple, Text(*result));
    }
    At<void*>(object, 0x88) = nullptr;
    bool found = true;
    propertyFound(object, &keys[0], &found);
    Log("  empty property found %d\n", found);
    using SetNames = void(__cdecl*)(void*, const void*, const void*);
    At<void*>(object, 0x24) = nullptr; At<void*>(object, 0x28) = nullptr;
    auto sortKey = As<unsigned long(__cdecl*)(void*)>(Proc(mod[cur], "?GetSortKey@Material@Jet@@UBAKXZ"));
    Log("  empty sort %08lx\n", sortKey(object));
    As<SetNames>(Proc(mod[cur], "?SetName@Material@Jet@@IAAXABVPString@2@0@Z"))(object, &keys[0], &keys[1]);
    Log("  material names %s %s sort %08lx\n", Text(At<void*>(object, 0x24)), Text(At<void*>(object, 0x28)), sortKey(object));
    DestroyNode(*cache, At<void*>(object, 0x24)); DestroyNode(*cache, At<void*>(object, 0x28));
    auto versionSet = As<void(__fastcall*)(void*, void*, float)>(Proc(mod[cur], "?SetMaterialAssetVersion@Material@Jet@@QAEXM@Z"));
    auto versionGet = As<float(__fastcall*)(void*, void*)>(Proc(mod[cur], "?GetMaterialAssetVersion@Material@Jet@@QBEMXZ"));
    versionSet(object, nullptr, 3.45f);
    Log("  asset version %.3f\n", versionGet(object, nullptr));
    auto geometry = As<unsigned(__cdecl*)(int, const void*)>(Proc(mod[cur], "?MakeGeometryChunkFlags@Material@Jet@@SAIW4GeometryType@12@PBVGeometryChunkCreateHint@2@@Z"));
    unsigned char hint[0x20]{};
    for (int type = 0; type < 6; ++type) {
        Log("  geometry %d %u", type, geometry(type, nullptr));
        for (int bits : {0, 1, 4, 5}) {
            hint[8] = 1; hint[0x10] = static_cast<unsigned char>(bits);
            Log(" %u", geometry(type, hint));
        }
        hint[8] = 0;
        Log(" %u\n", geometry(type, hint));
    }
    using NameValue = void*(__cdecl*)(void*, void*);
    auto nonUnique = As<NameValue>(Proc(mod[cur], "?GetNonUniqueDecoratedName@Material@Jet@@QBA?AVPString@2@XZ"));
    for (const char* inputName : {"", "abcd", "abcdefgh%%", "abcdefgh%%wood.texture", "a%%bcdef%%file", "abcdefghi%%suffix"}) {
        void* source = CreateNode(*cache, reinterpret_cast<const unsigned char*>(inputName));
        At<void*>(object, 0x24) = source;
        void* result = nullptr;
        nonUnique(object, &result);
        Log("  nonunique %s -> %s\n", inputName, Text(result));
        if (result) DestroyNode(*cache, result);
        if (source) DestroyNode(*cache, source);
    }
    At<void*>(object, 0x24) = nullptr;
    void* nullResult = nullptr;
    nonUnique(object, &nullResult);
    Log("  nonunique null -> %s\n", Text(nullResult));
    for (void* key : keys) DestroyNode(*cache, key);
}

static void ExerciseMaterialConfigExtract()
{
    using Constructor = void*(__fastcall*)(void*, void*);
    using Destructor = void(__fastcall*)(void*, void*);
    using NewChild = void*(__cdecl*)(void*, const void*);
    using SetFloat = void(__fastcall*)(void*, void*, float);
    using SetColor = void(__fastcall*)(void*, void*, const void*);
    using Extract = bool(__cdecl*)(void*, void*, const void*, const char*);
    alignas(4) unsigned char config[16]{}, helper[4]{};
    unsigned char helperCopy[4]{};
    using Helper0 = void*(__fastcall*)(void*, void*);
    using Helper1 = void*(__fastcall*)(void*, void*, const void*);
    auto helperCtor = As<Helper0>(Proc(mod[cur], "??0MaterialConfig@Jet@@QAE@XZ"));
    auto helperCopyCtor = As<Helper1>(Proc(mod[cur], "??0MaterialConfig@Jet@@QAE@ABV01@@Z"));
    auto helperAssign = As<Helper1>(Proc(mod[cur], "??4MaterialConfig@Jet@@QAEAAV01@ABV01@@Z"));
    auto helperDtor = As<Destructor>(Proc(mod[cur], "??1MaterialConfig@Jet@@UAE@XZ"));
    auto* table = Proc(mod[1], "??_7MaterialConfig@Jet@@6B@");
    Log("op material config lifetime %d", helperCtor(helper, nullptr) == helper);
    Log(" %d %d", At<void*>(helper, 0) == table, helperCopyCtor(helperCopy, nullptr, helper) == helperCopy);
    Log(" %d %d", At<void*>(helperCopy, 0) == table, helperAssign(helper, nullptr, helperCopy) == helper);
    helperDtor(helperCopy, nullptr);
    Log(" %d\n", At<void*>(helperCopy, 0) == table);
    auto ctor = As<Constructor>(Proc(ks, "??0ConfigData@Jet@@QAE@XZ"));
    auto dtor = As<Destructor>(Proc(ks, "??1ConfigData@Jet@@QAE@XZ"));
    auto add = As<NewChild>(Proc(ks, "?New@ConfigData@Jet@@QAAAAV12@ABVPString@2@@Z"));
    auto setFloat = As<SetFloat>(Proc(ks, "??6ConfigData@Jet@@QAEXM@Z"));
    auto setColor = As<SetColor>(Proc(ks, "??6ConfigData@Jet@@QAEXABVColor@1@@Z"));
    auto color = As<Extract>(Proc(mod[cur], "?ExtractColor@MaterialConfig@Jet@@IBA_NAAVColor@2@ABVConfigData@2@PBD@Z"));
    auto real = As<Extract>(Proc(mod[cur], "?ExtractFloat@MaterialConfig@Jet@@IBA_NAAMABVConfigData@2@PBD@Z"));
    auto flag = As<Extract>(Proc(mod[cur], "?ExtractBool@MaterialConfig@Jet@@IBA_NAA_NABVConfigData@2@PBD@Z"));
    ctor(config, nullptr);
    void* keyFloat = CreateNode(*cache, reinterpret_cast<const unsigned char*>("gloss"));
    void* keyColor = CreateNode(*cache, reinterpret_cast<const unsigned char*>("colour"));
    void* keyZero = CreateNode(*cache, reinterpret_cast<const unsigned char*>("disabled"));
    void* floatNode = add(config, &keyFloat);
    void* colorNode = add(config, &keyColor);
    void* zeroNode = add(config, &keyZero);
    float rgba[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    setFloat(floatNode, nullptr, 3.75f);
    setColor(colorNode, nullptr, rgba);
    setFloat(zeroNode, nullptr, 0.0f);
    for (const char* name : {"gloss", "colour", "disabled", "missing"}) {
        float result = -123.0f, resultColor[4] = {-1, -2, -3, -4};
        bool enabled = false;
        const bool hasFloat = std::strcmp(name, "colour") && real(helper, &result, config, name);
        const bool hasColor = (!std::strcmp(name, "colour") || !std::strcmp(name, "missing")) &&
                              color(helper, resultColor, config, name);
        const bool hasFlag = flag(helper, &enabled, config, name);
        Log("op material config %s float %d %.3f color %d %.3f %.3f %.3f %.3f flag %d %d\n", name,
            hasFloat, result, hasColor, resultColor[0], resultColor[1], resultColor[2], resultColor[3], hasFlag, enabled);
    }
    dtor(config, nullptr);
    helperDtor(helper, nullptr);
    DestroyNode(*cache, keyFloat); DestroyNode(*cache, keyColor); DestroyNode(*cache, keyZero);
}

static void ExerciseMaterialNameSplit()
{
    using Split = void*(__cdecl*)(int*, const void*, void*, void*);
    auto split = As<Split>(Proc(mod[cur], "?SplitDecoratedMaterialName@MaterialManager@Jet@@SA?AV?$ResultClass@$1?results@MaterialManager@Jet@@2QBUResultTable@2@BW4Results@MaterialManager@Jet@@@2@ABVPString@2@AAV42@1@Z"));
    for (const char* input : {"", ".", "a.", "plain", ".tga", "wood.", "WOOD.TGA", "object.diffuse.texture", "a.B", "foo.m.notex", "12345678%%thing.texture"}) {
        void* source = CreateNode(*cache, reinterpret_cast<const unsigned char*>(input));
        void* a = CreateNode(*cache, reinterpret_cast<const unsigned char*>("old-a"));
        void* b = CreateNode(*cache, reinterpret_cast<const unsigned char*>("old-b"));
        int status = -1;
        split(&status, &source, &a, &b);
        Log("op material split %s status %d outputs %s | %s\n", input, status, Text(a), Text(b));
        if (source) DestroyNode(*cache, source);
        if (a) DestroyNode(*cache, a);
        if (b) DestroyNode(*cache, b);
    }
    void* empty = nullptr;
    void* a = nullptr;
    void* b = nullptr;
    int status = -1;
    split(&status, &empty, &a, &b);
    Log("op material split null status %d outputs %s | %s\n", status, Text(a), Text(b));
}

static void ExerciseNullMaterial()
{
    using Ptr = void*(__cdecl*)(void*, int);
    using Set = void(__cdecl*)(void*, const void*);
    using SetNames = void(__cdecl*)(void*, const void*, const void*);
    using Noop = void(__cdecl*)(void*, void*, void*, void*, void*, void*, void*, void*, void*);
    using Unsupported = void*(__cdecl*)(void*, int*, int, void*, void*, float, void*);
    alignas(4) unsigned char object[0xa0]{};
    void* oldType = CreateNode(*cache, reinterpret_cast<const unsigned char*>("old type"));
    void* newType = CreateNode(*cache, reinterpret_cast<const unsigned char*>("new type"));
    At<void*>(object, 0x9c) = oldType;
    auto get = As<Ptr>(Proc(mod[cur], "?GetType@NullMaterial@Jet@@UBAABVPString@2@XZ"));
    auto set = As<Set>(Proc(mod[cur], "?SetType@NullMaterial@Jet@@QAAXABVPString@2@@Z"));
    set(object, &newType);
    Log("op null material type %d %s refs %d\n", get(object, 0) == object + 0x9c,
        Text(At<void*>(object, 0x9c)), At<int>(oldType, 0x10));
    auto names = As<SetNames>(Proc(mod[cur], "?SetName@NullMaterial@Jet@@QAAXABVPString@2@0@Z"));
    names(object, &oldType, &newType);
    Log("  names %s %s\n", Text(At<void*>(object, 0x24)), Text(At<void*>(object, 0x28)));
    const char* nullTextures[] = {"?GetTexture@NullMaterial@Jet@@UAAPAVTexture@2@H@Z",
        "?GetTexture@NullMaterial@Jet@@UBAPBVTexture@2@H@Z", "?CreateInstance@MaterialPlugin@Jet@@MBAPAVMaterial@2@XZ"};
    Log("  null textures");
    for (auto name : nullTextures) Log(" %d", As<Ptr>(Proc(mod[cur], name))(object, 42) == nullptr);
    Log("\n");
    const char* unsupported[] = {
        "?DuplicateTexture@NullMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@H@Z",
        "?SetTexture@Material@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HABV?$AutoReference@VTexture@Jet@@@2@M@Z",
        "?SetTexture@Material@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HPBVResourceDatabase@2@ABVPString@2@MABVString@2@@Z"};
    for (auto name : unsupported) {
        int result = -1;
        void* returned = As<Unsupported>(Proc(mod[cur], name))(object, &result, 5, nullptr, nullptr, 0.5f, nullptr);
        Log("  unsupported %d result %d\n", returned == &result, result);
    }
    const char* noops[] = {"?Activate@NullMaterial@Jet@@UAAXH@Z", "?Bind@NullMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
        "?Unbind@NullMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
        "?CacheChunk@NullMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z",
        "?RenderChunk@NullMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@H@Z",
        "?RenderNumChunks@NullMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAPAVGeometryChunk@2@PAPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PAPBVMatrix4@2@IH@Z",
        "?RenderPrimitives@NullMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@IPBGIW4DPrimitive@2@H@Z"};
    for (auto name : noops) As<Noop>(Proc(mod[cur], name))(object, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    Log("  noops complete\n");
    DestroyNode(*cache, At<void*>(object, 0x24)); DestroyNode(*cache, At<void*>(object, 0x28));
    DestroyNode(*cache, At<void*>(object, 0x9c)); DestroyNode(*cache, newType);
}

static void ExerciseMaterialPropertyMutation()
{
    using Init = void(__cdecl*)(void*);
    using Add = void(__cdecl*)(void*, const void*, const void*);
    using Count = unsigned(__cdecl*)(void*);
    using Property = void*(__cdecl*)(void*, const void*, bool*);
    auto init = As<Init>(Proc(mod[cur], "?Init@Material@Jet@@QAAXXZ"));
    auto add = As<Add>(Proc(mod[cur], "?AddProperty@Material@Jet@@UAAXABVPString@2@0@Z"));
    auto count = As<Count>(Proc(mod[cur], "?GetNumProperties@Material@Jet@@QBAIXZ"));
    auto find = As<Property>(Proc(mod[cur], "?GetPropertyString@Material@Jet@@UBAABVPString@2@ABV32@AA_N@Z"));
    alignas(4) unsigned char object[0xa0]{};
    At<unsigned char>(object, 0x84) = 0xff;
    Log("op material initialize empty\n");
    init(object);
    void* keys[17]{}, *values[17]{};
    for (int i = 0; i < 17; ++i) {
        char key[32], value[32];
        std::snprintf(key, sizeof key, "property-%d", i);
        std::snprintf(value, sizeof value, "value-%d", i);
        keys[i] = CreateNode(*cache, reinterpret_cast<const unsigned char*>(key));
        values[i] = CreateNode(*cache, reinterpret_cast<const unsigned char*>(value));
        add(object, &keys[i], &values[i]);
    }
    add(object, &keys[2], &values[16]);
    add(object, &keys[14], &values[0]);
    Log("  count %u", count(object));
    for (int i : {0, 2, 7, 14, 16}) {
        bool found = false;
        auto* value = static_cast<void**>(find(object, &keys[i], &found));
        Log(" %d:%s", found, Text(*value));
    }
    Log("\n");
    MaterialChunk(object, 0x90, 12, 60);
    At<unsigned>(object, 0x8c) = 3;
    Log("op material reinitialize populated\n");
    init(object);
    unsigned hash = 2166136261u;
    for (unsigned i = 0x34; i < 0x88; ++i) hash = (hash ^ object[i]) * 16777619u;
    Log("  state %08x lists %d %d texture-count %u\n", hash, At<void*>(object, 0x88) != nullptr,
        At<void*>(object, 0x90) != nullptr, At<unsigned>(object, 0x8c));
    for (int i = 0; i < 17; ++i) { DestroyNode(*cache, keys[i]); DestroyNode(*cache, values[i]); }
}

// ---- U7: the material instance state (MaterialInstance.cpp)

static void* matManagerVt[8];
static int matFindTurn;
static void* __cdecl MatManagerAdd(void*, void* material)
{
    Log("  mgr add %s\n", Text(At<void*>(material, 0x24)));
    return nullptr;
}
static void* __cdecl MatManagerRemove(void*, void* result, void* material)
{
    Log("  mgr remove %s\n", Text(At<void*>(material, 0x24)));
    *static_cast<int*>(result) = 0;
    return result;
}
static void* __cdecl MatManagerFind(void* self, const void* name)
{
    Log("  mgr find %s\n", Text(*static_cast<void* const*>(name)));
    return (matFindTurn++ & 1) ? self : nullptr;
}

static void LogMatState(const unsigned char* object)
{
    Log("  names %s %s %s %s", Text(At<void*>(object, 0x24)), Text(At<void*>(object, 0x28)),
        Text(At<void*>(object, 0x2c)), Text(At<void*>(object, 0x30)));
    unsigned hash = 2166136261u;
    for (unsigned i = 0x34; i < 0x88; ++i) hash = (hash ^ object[i]) * 16777619u;
    Log(" state %08x flags %02x count %u manager %d\n", hash, object[0x84], At<unsigned>(object, 0x8c),
        At<void*>(object, 0x98) != nullptr);
    for (unsigned offset : {0x88u, 0x90u}) {
        auto* first = At<unsigned char*>(object, offset);
        auto* chunk = first;
        if (!chunk) continue;
        do {
            const unsigned count = At<unsigned>(chunk, 8);
            Log("  list %x count %u", offset, count);
            for (unsigned i = 0; i < count; ++i) {
                auto* item = chunk + 12 + i * (offset == 0x88 ? 8 : 12);
                if (offset == 0x88) Log(" %s/%s", Text(At<void*>(item, 0)), Text(At<void*>(item, 4)));
                else Log(" %d/%s/%d", At<int>(item, 0), Text(At<void*>(item, 4)), At<int>(item, 8));
            }
            Log("\n");
            chunk = At<unsigned char*>(chunk, 0);
        } while (chunk != first);
    }
}

static void* MakeName(const char* text) { return CreateNode(*cache, reinterpret_cast<const unsigned char*>(text)); }

static void LogString(const void* text)
{
    const char* data = At<const char*>(text, 0);
    const unsigned length = data ? At<unsigned>(data, 4) : 0;
    Log("%.*s", static_cast<int>(length), data + 8);
}

// one chunk of up to seven texture definitions (leaked like MaterialChunk's)
static void* BuildTexRing(const int* types, const char* const* names, int count, std::vector<void*>& nodes)
{
    auto* chunk = static_cast<unsigned char*>(RealNew(12 + 12 * 7));
    std::memset(chunk, 0, 12 + 12 * 7);
    At<void*>(chunk, 0) = chunk;
    At<void*>(chunk, 4) = chunk;
    At<unsigned>(chunk, 8) = count;
    for (int i = 0; i < count; ++i) {
        void* node = MakeName(names[i]);
        nodes.push_back(node);
        At<int>(chunk, 12 + i * 12) = types[i];
        At<void*>(chunk, 12 + i * 12 + 4) = node;
        At<int>(chunk, 12 + i * 12 + 8) = 1234 + i;
    }
    return chunk;
}

static void ExerciseMaterialInstance()
{
    using FloatGet = float(__cdecl*)(void*);
    using FloatSet = void(__cdecl*)(void*, float);
    using UnsignedGet = unsigned(__cdecl*)(void*);
    using ResultGet = int*(__cdecl*)(void*, int*);
    using PtrGet = void*(__cdecl*)(void*);
    using Equals = bool(__cdecl*)(void*, void*);
    using SetTexDef = void(__cdecl*)(void*, int, const void*, float);
    using SetTexNamed = int*(__cdecl*)(void*, int*, int, const void*, const void*, float, const void*);
    using SetTexRef = int*(__cdecl*)(void*, int*, int, const void*, float);
    using InitInst = void(__cdecl*)(void*, void*);
    using RenameFn = bool(__cdecl*)(void*, const void*, const void*);
    using Verify = void(__cdecl*)(void*, unsigned, const void*);
    // the shared counter of the original's side starts alike for both
    static unsigned long* const idCounter = static_cast<unsigned long*>(Proc(mod[1], "?uniqueMatId@Material@Jet@@0KA"));
    *idCounter = 7;
    matFindTurn = 0;
    matManagerVt[4] = reinterpret_cast<void*>(&MatManagerAdd);
    matManagerVt[5] = reinterpret_cast<void*>(&MatManagerRemove);
    matManagerVt[6] = reinterpret_cast<void*>(&MatManagerFind);
    alignas(4) unsigned char manager[0x20]{};
    *reinterpret_cast<void**>(manager) = matManagerVt;

    alignas(4) unsigned char object[0xac], other[0xac];
    for (unsigned i = 0; i < 0xac; ++i) object[i] = static_cast<unsigned char>(i * 5);

    Log("op material leaf\n");
    auto shininess = As<FloatGet>(Proc(mod[cur], "?GetShininess@Material@Jet@@QBAMXZ"));
    auto setShininess = As<FloatSet>(Proc(mod[cur], "?SetShininess@Material@Jet@@QAAXM@Z"));
    Log("  shininess %.3f", shininess(object));
    setShininess(object, 12.5f);
    Log(" -> %.3f\n", shininess(object));
    Log("  passes %u\n", As<UnsignedGet>(Proc(mod[cur], "?GetNumPasses@Material@Jet@@UBAIXZ"))(object));
    int result = -1;
    auto loaded = As<ResultGet>(Proc(mod[cur],
        "?IsLoaded@Material@Jet@@UBA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@XZ"));
    void* loadedResult = loaded(object, &result);
    Log("  loaded %d result %d\n", loadedResult == &result, result);
    At<void*>(manager, 8) = object;
    At<void*>(manager, 0xc) = other;
    Log("  manager links %d %d\n",
        As<PtrGet>(Proc(mod[cur], "?GetRenderKernel@MaterialManager@Jet@@QBAPAVRenderKernel@2@XZ"))(manager) == object,
        As<PtrGet>(Proc(mod[cur], "?GetTextureManager@MaterialManager@Jet@@QBAPAVTextureManager@2@XZ"))(manager) == other);
    At<void*>(manager, 8) = nullptr;
    At<void*>(manager, 0xc) = nullptr;

    Log("op material equals\n");
    auto equals = As<Equals>(Proc(mod[cur], "?Equals@Material@Jet@@IBA_NABV12@@Z"));
    for (unsigned i = 0; i < 0xac; ++i) object[i] = other[i] = static_cast<unsigned char>(i * 5);
    At<void*>(object, 0x88) = At<void*>(object, 0x90) = nullptr;
    At<void*>(other, 0x88) = At<void*>(other, 0x90) = nullptr;
    Log("  plain %d\n", equals(object, other));
    for (unsigned offset : {0x34u, 0x35u, 0x78u, 0x7cu, 0x80u, 0x94u, 0x8cu}) {
        other[offset] ^= 0x40;
        Log("  byte %x %d\n", offset, equals(object, other));
        other[offset] ^= 0x40;
    }
    other[0x84] ^= 1;
    Log("  bit0 %d\n", equals(object, other));
    other[0x84] ^= 1;
    other[0x84] ^= 2;
    Log("  bit1 %d\n", equals(object, other));
    other[0x84] ^= 2;
    MaterialChunk(object, 0x88, 8, 10);
    MaterialChunk(object, 0x90, 12, 20);
    MaterialChunk(other, 0x88, 8, 10);
    MaterialChunk(other, 0x90, 12, 20);
    Log("  lists equal %d\n", equals(object, other));
    MaterialChunk(other, 0x90, 12, 30);
    Log("  textures differ %d\n", equals(object, other));
    MaterialChunk(other, 0x88, 8, 40);
    Log("  properties differ %d\n", equals(object, other));
    At<void*>(object, 0x88) = nullptr;
    Log("  one-sided properties %d\n", equals(object, other));
    At<void*>(object, 0x90) = nullptr;
    Log("  one-sided textures %d\n", equals(object, other));
    alignas(4) unsigned char nullA[0xac], nullB[0xac];
    auto nullEquals = As<Equals>(Proc(mod[cur], "?Equals@NullMaterial@Jet@@IBA_NABV12@@Z"));
    for (unsigned i = 0; i < 0xac; ++i) nullA[i] = nullB[i] = static_cast<unsigned char>(i * 9);
    At<void*>(nullA, 0x88) = At<void*>(nullA, 0x90) = nullptr;
    At<void*>(nullB, 0x88) = At<void*>(nullB, 0x90) = nullptr;
    Log("  null equal %d\n", nullEquals(nullA, nullB));
    At<void*>(nullB, 0x9c) = MakeName("u7-type-b");
    At<void*>(nullA, 0x9c) = MakeName("u7-type-a");
    Log("  null types %d\n", nullEquals(nullA, nullB));
    At<void*>(nullA, 0x9c) = At<void*>(nullB, 0x9c);
    Log("  null same type %d\n", nullEquals(nullA, nullB));

    Log("op material texture definitions\n");
    auto setDef = As<SetTexDef>(Proc(mod[cur], "?SetTextureDefinition@Material@Jet@@IAAXHABVPString@2@M@Z"));
    for (unsigned i = 0; i < 0xac; ++i) object[i] = 0;
    struct Def {
        int type;
        const char* name;
        float weight;
    };
    const Def defs[] = {{1, "u7-diffuse", 1.0f}, {8, "u7-bump", 0.5f}, {1, "u7-second", 2.0f},
                        {8, "u7-bump", -1.0f},   {6, "u7-opacity", 3.25f}, {1, "u7-replacement", 4.5f}};
    for (const Def& def : defs) {
        void* name = MakeName(def.name);
        setDef(object, def.type, &name, def.weight);
        Log("  set %d %s %.2f ->", def.type, def.name, def.weight);
        LogMatState(object);
    }

    Log("op null material set texture\n");
    auto setNamed = As<SetTexNamed>(Proc(mod[cur],
        "?SetTexture@NullMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HPBVResourceDatabase@2@ABVPString@2@MABVString@2@@Z"));
    auto setRef = As<SetTexRef>(Proc(mod[cur],
        "?SetTexture@NullMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HABV?$AutoReference@VTexture@Jet@@@2@M@Z"));
    for (unsigned i = 0; i < 0xac; ++i) nullA[i] = 0;
    result = -1;
    void* named = MakeName("u7-named");
    Log("  named %d", setNamed(nullA, &result, 3, nullptr, &named, 0.25f, nullptr) == &result);
    Log(" result %d\n", result);
    alignas(4) unsigned char texture[0x20]{};
    At<void*>(texture, 0x10) = MakeName("u7-from-texture");
    void* texturePtr = texture;
    result = -1;
    Log("  ref %d", setRef(nullA, &result, 6, &texturePtr, 0.75f) == &result);
    Log(" result %d\n", result);
    LogMatState(nullA);

    Log("op material init instance\n");
    auto initInstance = As<InitInst>(Proc(mod[cur], "?InitInstance@Material@Jet@@IBAXAAV12@@Z"));
    for (int scenario = 0; scenario < 3; ++scenario) {
        for (unsigned i = 0; i < 0xac; ++i) object[i] = static_cast<unsigned char>(i * 3 + scenario);
        for (unsigned i = 0; i < 0xac; ++i) other[i] = 0xa5;
        At<void*>(object, 0x88) = At<void*>(object, 0x90) = nullptr;
        At<void*>(other, 0x88) = At<void*>(other, 0x90) = nullptr;
        At<void*>(other, 0x24) = MakeName("u7-oldd");
        At<void*>(other, 0x28) = MakeName("u7-oldu");
        At<void*>(other, 0x2c) = MakeName("u7-oldc");
        At<void*>(other, 0x30) = MakeName("u7-oldr");
        At<void*>(object, 0x24) = scenario == 0 ? MakeName("0000000a%%u7-wood") : (scenario == 1 ? MakeName("u7-plain") : nullptr);
        At<void*>(object, 0x28) = MakeName("u7-under");
        At<void*>(object, 0x2c) = MakeName("u7-creation");
        At<void*>(object, 0x30) = MakeName("u7-request");
        At<unsigned>(object, 0x8c) = 2;
        At<void*>(object, 0x98) = scenario == 2 ? nullptr : manager;
        if (scenario < 2) {
            MaterialChunk(object, 0x88, 8, 50 + scenario);
            MaterialChunk(object, 0x90, 12, 60 + scenario);
        }
        Log("op material init %d\n", scenario);
        initInstance(object, other);
        LogMatState(other);
    }

    Log("op material rename\n");
    auto rename = As<RenameFn>(Proc(mod[cur], "?Rename@Material@Jet@@QAA_NABVPString@2@0@Z"));
    At<void*>(object, 0x88) = At<void*>(object, 0x90) = nullptr;
    At<void*>(object, 0x24) = MakeName("u7-r1");
    At<void*>(object, 0x28) = MakeName("u7-rp1");
    At<void*>(object, 0x98) = nullptr;
    Log("  no manager %d\n", rename(object, nullptr, nullptr));
    At<void*>(object, 0x98) = manager;
    for (int i = 0; i < 3; ++i) {
        char text[32];
        std::snprintf(text, sizeof text, "u7-rd%d", i);
        void* decorated = MakeName(text);
        std::snprintf(text, sizeof text, "u7-rp%d", i);
        void* plain = MakeName(text);
        Log("  rename %d\n", rename(object, &decorated, &plain));
        LogMatState(object);
    }

    Log("op material verify name\n");
    auto verify = As<Verify>(Proc(mod[cur],
        "?VerifyDecoratedName@Material@Jet@@SAXAAVString@2@IABV?$DynamicArray@UTextureDef@Material@Jet@@$06U?$Less@UTextureDef@Material@Jet@@U123@@3@@2@@Z"));
    using StrCtor = void*(__fastcall*)(void*, void*, const char*);
    using StrDtor = void(__fastcall*)(void*, void*);
    auto strCtor = As<StrCtor>(Proc(ks, "??0String@Jet@@QAE@PBD@Z"));
    auto strDtor = As<StrDtor>(Proc(ks, "??1String@Jet@@QAE@XZ"));
    static const char* const starts[] = {"", "Wood", "M.WOODED", "plain+tag", "m.onetexstuff", "u7.name"};
    static const char* const listNames[][3] = {{"", "", ""},        {"abc", "", ""},          {"sb_tree", "", ""},
                                               {"tb_tree", "", ""}, {"SB_x", "", ""},        {"r", "", ""},
                                               {"r", "o", ""},      {"u7-diffuse", "u7-diffuse2", "u7-diffuse3"}};
    static const int listTypes[][3] = {{0, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {9, 0, 0}, {9, 6, 0}, {1, 1, 1}};
    static const int listCounts[] = {0, 1, 1, 1, 1, 1, 2, 3};
    for (const char* start : starts) {
        for (unsigned hint : {0u, 1u, 2u, 5u}) {
            for (int list = 0; list < 8; ++list) {
                std::vector<void*> nodes;
                alignas(4) unsigned char holder[4]{};
                if (listCounts[list]) At<void*>(holder, 0) = BuildTexRing(listTypes[list], listNames[list], listCounts[list], nodes);
                alignas(4) unsigned char text[16]{};
                strCtor(text, nullptr, start);
                Log("  verify '%s' hint %u list %d -> ", start, hint, list);
                verify(text, hint, holder);
                LogString(text);
                Log("\n");
                strDtor(text, nullptr);
            }
        }
    }
}

static void LogMatTable(void* manager)
{
    void* buckets = At<void*>(manager, 0x14);
    const unsigned size = At<unsigned>(manager, 0x1c);
    Log("  table count %u\n", At<unsigned>(manager, 0x18));
    for (unsigned k = 0; k < size; ++k) {
        auto* head = static_cast<unsigned char*>(At<void*>(buckets, k * 4));
        if (!head) continue;
        Log("  bucket %u", k);
        unsigned char* ring = head;
        do {
            const unsigned count = At<unsigned>(ring, 8);
            for (unsigned i = 0; i < count; ++i) Log(" %s", Text(At<void*>(At<void*>(ring, 12 + i * 4), 0x24)));
            ring = At<unsigned char*>(ring, 0);
        } while (ring != head);
        Log("\n");
    }
}

static void ExerciseMaterialManager()
{
    using Add = void(__cdecl*)(void*, void*);
    using Remove = int*(__cdecl*)(void*, int*, void*);
    using Find = void*(__cdecl*)(void*, const void*);
    auto add = As<Add>(Proc(mod[cur], "?Add@MaterialManager@Jet@@UAAXPAVMaterial@2@@Z"));
    auto remove = As<Remove>(Proc(mod[cur],
        "?Remove@MaterialManager@Jet@@UAA?AV?$ResultClass@$1?results@MaterialManager@Jet@@2QBUResultTable@2@BW4Results@MaterialManager@Jet@@@2@PAVMaterial@2@@Z"));
    auto find = As<Find>(Proc(mod[cur], "?Find@MaterialManager@Jet@@UAAPAVMaterial@2@ABVPString@2@@Z"));
    Log("op material manager table\n");
    alignas(4) unsigned char manager[0x28]{};
    auto* block = static_cast<unsigned char*>(RealNew(4 + 97 * 4));
    std::memset(block, 0, 4 + 97 * 4);
    At<unsigned>(block, 0) = 97;
    At<void*>(manager, 0x14) = block + 4;
    At<unsigned>(manager, 0x1c) = 97;
    // names grouped by bucket: the fullest bucket gets the two-chunk treatment
    std::vector<void*> names;
    std::vector<int> groups[97];
    for (int i = 0; i < 48; ++i) {
        char text[32];
        std::snprintf(text, sizeof text, "u7-mgr-%d", i);
        void* node = MakeName(text);
        names.push_back(node);
        groups[reinterpret_cast<unsigned>(node) % 97].push_back(i);
    }
    const std::vector<int>* best = &groups[0];
    for (auto& g : groups)
        if (g.size() > best->size()) best = &g;
    alignas(4) unsigned char mats[20][0x28];
    std::vector<void*> materials;
    for (unsigned i = 0; i < best->size() && i < 20; ++i) {
        std::memset(mats[i], 0, 0x28);
        At<void*>(mats[i], 0x24) = names[(*best)[i]];
        materials.push_back(mats[i]);
    }
    for (void* m : materials) add(manager, m);
    LogMatTable(manager);
    for (void* m : materials) Log("  find %s %d\n", Text(At<void*>(m, 0x24)), find(manager, static_cast<unsigned char*>(m) + 0x24) == m);
    void* missing = MakeName("u7-mgr-missing");
    Log("  find missing %d\n", find(manager, &missing) == nullptr);
    // a name added twice: Remove takes the first entry with that node
    if (materials.size() >= 2) {
        alignas(4) unsigned char twin[0x28]{};
        At<void*>(twin, 0x24) = At<void*>(materials[0], 0x24);
        add(manager, twin);
        Log("  twin find %d\n", find(manager, static_cast<unsigned char*>(twin) + 0x24) == materials[0]);
        int result = -1;
        remove(manager, &result, twin);
        Log("  twin remove %d first-gone %d\n", result, find(manager, static_cast<unsigned char*>(twin) + 0x24) != materials[0]);
    }
    // every other one goes, then the rest
    for (unsigned i = 0; i < materials.size(); i += 2) {
        int result = -1;
        remove(manager, &result, materials[i]);
        Log("  remove %u %d\n", i, result);
    }
    LogMatTable(manager);
    for (unsigned i = 1; i < materials.size(); i += 2) {
        int result = -1;
        remove(manager, &result, materials[i]);
        Log("  remove %u %d\n", i, result);
    }
    int result = -1;
    remove(manager, &result, materials[materials.size() - 1]);
    Log("  remove again %d\n", result);
    LogMatTable(manager);
}

static void ExerciseMaterialConversions()
{
    using Ctor = void*(__fastcall*)(void*, void*);
    using Assign = void*(__fastcall*)(void*, void*, const void*);
    unsigned char object[4] = {0x12, 0x34, 0x56, 0x78};
    unsigned char other[4] = {1, 2, 3, 4};
    auto ctor = As<Ctor>(Proc(mod[cur], "??0MaterialFileSupport@Jet@@AAE@XZ"));
    auto assign = As<Assign>(Proc(mod[cur], "??4MaterialFileSupport@Jet@@QAEAAV01@ABV01@@Z"));
    Log("op material support ctor %d ", ctor(object, nullptr) == object);
    Log("assign %d bytes %02x %02x %02x %02x\n", assign(object, nullptr, other) == object,
        object[0], object[1], object[2], object[3]);
    using Parse = int(__cdecl*)(const void*);
    using Format = void*(__cdecl*)(void*, int);
    const char* parseNames[] = {
        "?GetTextureType@MaterialFileSupport@Jet@@SAHABVPString@2@@Z",
        "?ConvertSrc@MaterialFileSupport@Jet@@SA?AW4Factor@Src@2@ABVPString@2@@Z",
        "?ConvertDst@MaterialFileSupport@Jet@@SA?AW4Factor@Dst@2@ABVPString@2@@Z",
        "?ConvertDFunction@MaterialFileSupport@Jet@@SA?AW4DFunction@2@ABVPString@2@@Z"};
    const char* formatNames[] = {
        "?ConvertSrc@MaterialFileSupport@Jet@@SA?AVPString@2@W4Factor@Src@2@@Z",
        "?ConvertDst@MaterialFileSupport@Jet@@SA?AVPString@2@W4Factor@Dst@2@@Z",
        "?ConvertDFunction@MaterialFileSupport@Jet@@SA?AVPString@2@W4DFunction@2@@Z"};
    const char* inputs[] = {"", "ambient", "DIFFUSE", "specular", "shine", "shinstrength", "selfillum",
        "opacity", "filtercolor", "bump", "reflect", "refract", "displacement", "lookup0", "lookup1",
        "lookup2", "lookup3", "zero", "ONE", "dstColor", "oneMinusDstColor", "srcAlpha", "oneMinusSrcAlpha",
        "dstAlpha", "oneMinusDstAlpha", "srcAlphaSaturate", "dst_Color", "one_Minus_Dst_Color", "src_Alpha",
        "one_Minus_Src_Alpha", "dst_Alpha", "one_Minus_Dst_Alpha", "src_Alpha_Saturate", "srcColor",
        "oneMinusSrcColor", "src_Color", "one_Minus_Src_Color", "Never", "Less", "LessEqual", "Equal",
        "GreaterEqual", "Greater", "NotEqual", "Always", "unknown"};
    for (const char* input : inputs) {
        void* node = CreateNode(*cache, reinterpret_cast<const unsigned char*>(input));
        Log("op material parse %s", input);
        for (const char* symbol : parseNames) Log(" %d", As<Parse>(Proc(mod[cur], symbol))(&node));
        Log("\n");
        if (node) DestroyNode(*cache, node);
    }
    for (int value = -2; value <= 18; ++value) {
        Log("op material format %d", value);
        for (const char* symbol : formatNames) {
            void* result = nullptr;
            As<Format>(Proc(mod[cur], symbol))(&result, value);
            Log(" %s", Text(result));
            if (result) DestroyNode(*cache, result);
        }
        Log("\n");
    }
}

static void ExerciseTextureResVirtuals()
{
    HMODULE module = mod[cur];
    auto available = As<bool(__cdecl*)(const void*)>(Proc(module, "?IsAvailable@TextureRes@Jet@@UBA_NXZ"));
    auto failure = As<bool(__cdecl*)(const void*)>(Proc(module, "?IsLoadFailure@TextureRes@Jet@@UBA_NXZ"));
    auto alpha = As<int(__cdecl*)(const void*)>(Proc(module, "?GetAlphaHint@TextureRes@Jet@@UBA?AW4TextureAlpha@2@XZ"));
    auto dimension = As<int(__cdecl*)(const void*)>(Proc(module, "?GetDimension@TextureRes@Jet@@UBA?AW4TextureDimension@2@XZ"));
    auto depth = As<unsigned(__cdecl*)(const void*)>(Proc(module, "?GetDepth@TextureRes@Jet@@UBAIXZ"));
    auto getSize = As<void(__cdecl*)(const void*, void*)>(Proc(module, "?GetSize@TextureRes@Jet@@UBA?AUSize@2@XZ"));
    auto absolute = As<void(__cdecl*)(const void*, void*)>(Proc(module, "?GetAbsoluteSize@TextureRes@Jet@@UBA?AUSize@2@XZ"));
    auto average = As<void(__cdecl*)(const void*, void*)>(Proc(module, "?GetAverageColor@TextureRes@Jet@@UBA?AVColor@2@XZ"));
    auto getDisplay = As<void*(__cdecl*)(const void*)>(Proc(module, "?GetDisplayTexture@TextureRes@Jet@@UBAPAVDisplayTexture@2@XZ"));
    auto getResource = As<void*(__cdecl*)(const void*)>(Proc(module, "?GetResource@TextureRes@Jet@@MBAPBVTextureResource@2@XZ"));
    auto getName = As<void*(__cdecl*)(const void*)>(Proc(module, "?GetResourceName@TextureRes@Jet@@UBAABVPString@2@XZ"));
    auto getInfo = As<void*(__cdecl*)(void*)>(Proc(module, "?GetTextureManagerInfo@TextureRes@Jet@@UAAPAVTextureManagerInfo@2@XZ"));
    auto subload = As<bool(__cdecl*)(void*, int, int, const void*, void*)>(Proc(module, "?DynamicSubload@TextureRes@Jet@@UAA_NHHABVBitmap@2@AAVDisplayFormat@2@@Z"));
    auto setDisplay = As<void(__cdecl*)(void*, void*)>(Proc(module, "?SetDisplayTexture@TextureRes@Jet@@UAAXPAVDisplayTexture@2@@Z"));
    auto bind = As<void(__cdecl*)(const void*, unsigned)>(Proc(module, "?Bind@TextureRes@Jet@@UBAXI@Z"));
    auto onLoad = As<void(__cdecl*)(const void*, const void*)>(Proc(module, "?OnLoad@TextureRes@Jet@@EBAXPBVResource@2@@Z"));
    auto initInstance = As<void(__cdecl*)(const void*, void*)>(Proc(module, "?InitInstance@TextureRes@Jet@@IBAXAAV12@@Z"));
    unsigned char texture[0x60]{};
    unsigned char display[0x38]{};
    unsigned char info[0x1c]{};
    unsigned char bitmap[0x44]{};
    unsigned char format[0x20]{};
    void* displayVtable[20]{};
    displayVtable[4] = reinterpret_cast<void*>(&TextureResSubload);
    displayVtable[8] = reinterpret_cast<void*>(&TextureResBind);
    At<void*>(display, 0) = displayVtable;
    At<int>(bitmap, 0) = 32;
    At<int>(bitmap, 4) = 16;
    At<void*>(texture, 0x18) = display;
    At<void*>(texture, 0x3c) = resObj;
    At<void*>(texture, 0x5c) = info;
    At<int>(texture, 0x1c) = 7;
    At<int>(texture, 0x40) = 2048;
    At<int>(texture, 0x44) = 1024;
    At<int>(display, 0x10) = 512;
    At<int>(display, 0x14) = 256;
    At<int>(display, 0x18) = 2;
    At<int>(display, 0x1c) = 6;
    for (int i = 0; i < 4; ++i) At<int>(texture, 0x20 + i * 4) = 0x3f000000 + i;
    for (int state : {1, 2}) {
        At<int>(texture, 0x38) = state;
        int size[2]{}, absoluteSize[2]{}, color[4]{};
        getSize(texture, size);
        absolute(texture, absoluteSize);
        average(texture, color);
        Log("op texture virtuals state %d available %d failure %d alpha %d dimension %d depth %u\n",
            state, available(texture), failure(texture), alpha(texture), dimension(texture), depth(texture));
        Log("  size %d %d absolute %d %d color %08x %08x %08x %08x refs %d %d %d %d\n",
            size[0], size[1], absoluteSize[0], absoluteSize[1], color[0], color[1], color[2], color[3],
            getDisplay(texture) == display, getResource(texture) == resObj,
            getName(texture) == texture + 0x48, getInfo(texture) == info);
        Log("  dynamic %d\n", subload(texture, 2, 3, bitmap, format));
        bind(texture, 3);
    }
    At<void*>(texture, 0x18) = nullptr;
    At<int>(texture, 0x38) = 0;
    Log("op texture set display before load\n");
    setDisplay(texture, display);
    Log("  state %d resource %d display %d\n", At<int>(texture, 0x38),
        At<void*>(texture, 0x3c) != nullptr, At<void*>(texture, 0x18) == display);
    unsigned char manager[0x20]{}, kernel[0x90]{}, device[4]{};
    void* deviceVtable[0x30]{};
    deviceVtable[0xa0 / 4] = reinterpret_cast<void*>(&TextureResBindEmpty);
    At<void*>(device, 0) = deviceVtable;
    At<void*>(kernel, 0x7c) = device;
    At<void*>(manager, 0x14) = kernel;
    At<void*>(texture, 0x30) = manager;
    At<void*>(texture, 0x18) = nullptr;
    Log("op texture bind without display\n");
    bind(texture, 4);
    At<int>(texture, 0x38) = 0;
    At<void*>(texture, 0x3c) = nullptr;
    Log("op texture observer mismatch\n");
    onLoad(texture + 0x34, resObj);
    Log("  state %d\n", At<int>(texture, 0x38));
    Log("op texture observer match\n");
    onLoad(texture + 0x34, nullptr);
    Log("  state %d\n", At<int>(texture, 0x38));
    unsigned char target[0x60]{};
    At<int>(texture, 0x38) = 2;
    Log("op texture init instance\n");
    initInstance(texture, target);
    Log("  state %d size %d %d alpha %d color %08x %08x %08x %08x\n", At<int>(target, 0x38),
        At<int>(target, 0x40), At<int>(target, 0x44), At<int>(target, 0x1c), At<int>(target, 0x20),
        At<int>(target, 0x24), At<int>(target, 0x28), At<int>(target, 0x2c));
}

static void ExerciseUpsize(const Api& api, void* manager)
{
    unsigned char texture[0x60]{};
    unsigned char display[0x38]{};
    void* vtable[32]{};
    vtable[7] = reinterpret_cast<void*>(&UpsizeResource);
    vtable[21] = reinterpret_cast<void*>(&TexDisplay);
    At<void*>(texture, 0) = vtable;
    At<void*>(texture, 0x54) = display;
    At<void*>(resObj, 8) = dbObj;
    At<int>(texture, 0x50) = 9;
    At<int>(display, 0x20) = 3;
    names[cur][texture] = "upsize";
    api.setMax(256);
    for (int size : {4, 8, 16, 128, 512}) {
        Log("op upsize %d\n", size);
        api.upsize(manager, texture, size);
        if (void* request = At<void*>(texture, 0x4c)) {
            Log("  db %s reverse %d first %d\n", Name(At<void*>(request, 4)).c_str(),
                At<int>(request, 0xc), At<int>(request, 0x10));
            api.cancelResize(manager, texture);
        }
    }
    Log("op upsize without database\n");
    At<void*>(resObj, 8) = nullptr;
    api.upsize(manager, texture, 512);
    Log("  pending %d\n", api.isResizing(manager, texture));
    At<void*>(resObj, 8) = dbObj;
    Log("op upsize without display\n");
    At<void*>(texture, 0x54) = nullptr;
    api.upsize(manager, texture, 512);
    Log("  pending %d\n", api.isResizing(manager, texture));
    At<void*>(resObj, 8) = nullptr;
    names[cur].erase(texture);
}

static void ExerciseCheckDownsize(const Api& api, void* manager)
{
    unsigned char texture[0x60]{};
    unsigned char display[0x38]{};
    unsigned char info[0x1c]{};
    void* vtable[32]{};
    vtable[21] = reinterpret_cast<void*>(&TexDisplay);
    At<void*>(texture, 0) = vtable;
    At<void*>(texture, 0x54) = display;
    At<int>(texture, 0x50) = 12;
    At<int>(display, 0x20) = 10;
    At<void*>(info, 8) = texture;
    names[cur][texture] = "checked";
    api.setMin(32);
    for (int age : {0, 100, 101, 400}) {
        for (int wanted : {2, 7, 11}) {
            At<unsigned>(manager, 0x28) = 1000;
            At<int>(info, 0) = wanted;
            At<unsigned>(info, 4) = 1000 - age;
            Log("op check age %d wanted %d\n", age, wanted);
            api.checkDownsize(manager, info);
            Log("  wanted %d frame %u pending %d\n", At<int>(info, 0), At<unsigned>(info, 4),
                api.isResizing(manager, texture));
            if (void* request = At<void*>(texture, 0x4c)) {
                Log("  reverse %d\n", At<int>(request, 0xc));
                api.cancelResize(manager, texture);
            }
        }
    }
    names[cur].erase(texture);
}

static void ExerciseCheckTextures(const Api& api)
{
    void* manager = TestNew(0xd8, "checkManager%d", 0);
    api.ctor(manager, nullptr);
    unsigned char textures[14][0x60]{};
    unsigned char infos[14][0x20]{};
    unsigned char displays[14][0x38]{};
    void* vtable[32]{};
    vtable[21] = reinterpret_cast<void*>(&TexDisplay);
    vtable[22] = reinterpret_cast<void*>(&TexInfo);
    At<unsigned>(manager, 0x28) = 500;
    for (int i = 0; i < 14; ++i) {
        void* texture = textures[i];
        void* info = infos[i];
        void* node = static_cast<char*>(info) + 0xc;
        names[cur][texture] = "checkTex" + std::to_string(i);
        names[cur][info] = "checkInfo" + std::to_string(i);
        At<void*>(texture, 0) = vtable;
        At<void*>(texture, 0x54) = displays[i];
        At<void*>(texture, 0x58) = info;
        At<int>(texture, 0x50) = 12;
        At<int>(displays[i], 0x20) = 10;
        At<int>(info, 0) = i % 9;
        At<unsigned>(info, 4) = i * 30;
        At<void*>(info, 8) = texture;
        At<void*>(node, 0) = At<void*>(node, 4) = At<void*>(node, 8) = node;
        At<void*>(node, 0xc) = info;
        SC<void>(manager, 5, texture);
    }
    for (int pass = 0; pass < 4; ++pass) {
        Log("op check textures %d\n", pass);
        api.checkTextures(manager);
        Log("  lists %d %d\n", At<int>(manager, 0xd0), At<int>(manager, 0xd4));
        for (int i = 0; i < 14; ++i)
            Log("  %d wanted %d frame %u pending %d\n", i, At<int>(infos[i], 0),
                At<unsigned>(infos[i], 4), api.isResizing(manager, textures[i]));
    }
    releaseRef(manager, nullptr);
    for (int i = 0; i < 14; ++i) {
        names[cur].erase(textures[i]);
        names[cur].erase(infos[i]);
    }
}

static void ExerciseBulkResizing(const Api& api)
{
    void* manager = TestNew(0xd8, "bulkManager%d", 0);
    api.ctor(manager, nullptr);
    unsigned char textures[3][0x60]{};
    unsigned char displays[3][0x38]{};
    void* nodes[3];
    for (int i = 0; i < 3; ++i) {
        nodes[i] = CreateNode(*cache, reinterpret_cast<const unsigned char*>(i == 0 ? "bulk-a" : i == 1 ? "bulk-b" : "bulk-c"));
        At<void*>(textures[i], 0) = &texVt[i == 2 ? 0 : 1][1];
        At<void*>(textures[i], 0x10) = nodes[i];
        At<void*>(textures[i], 0x54) = displays[i];
        At<int>(textures[i], 0x50) = 12;
        At<int>(displays[i], 0x20) = i == 0 ? 12 : 7;
        names[cur][textures[i]] = "bulk" + std::to_string(i);
        SC<void>(manager, 4, textures[i]);
    }
    auto up = As<void(__cdecl*)(void*)>(Proc(mod[cur], "?TestUpSizeStreamableTextures@TextureManager@Jet@@QAAXXZ"));
    auto down = As<void(__cdecl*)(void*)>(Proc(mod[cur], "?TestDownSizeStreamableTextures@TextureManager@Jet@@QAAXXZ"));
    Log("op bulk upsize\n");
    up(manager);
    Log("op bulk downsize\n");
    down(manager);
    for (int i = 0; i < 3; ++i) {
        Log("  %d pending %d\n", i, api.isResizing(manager, textures[i]));
        if (i < 2) api.cancelResize(manager, textures[i]);
    }
    releaseRef(manager, nullptr);
    for (int i = 0; i < 3; ++i) {
        names[cur].erase(textures[i]);
        DestroyNode(*cache, nodes[i]);
    }
}

static void __fastcall ChangeMipStub(void* texture, void*, const void* size, int levels, bool copy)
{
    Log("  change %s %d x %d levels %d copy %d\n", Name(texture).c_str(), At<int>(size, 0),
        At<int>(size, 4), levels, copy);
}

static void* updateDisplay;
static void* __cdecl UpdateDisplay(const void* texture)
{
    Log("  texture %s display texture\n", Name(texture).c_str());
    return updateDisplay;
}

static int testLoadResult = 1;
static int testLoadLast = 9;
static void* __cdecl FailedLoadStub(void* result, const void*, const char*, const void*, int*, int* last, void* info)
{
    *static_cast<int*>(result) = testLoadResult;
    if (!testLoadResult) {
        *last = testLoadLast;
        At<int>(info, 0x14) = 1024;
        At<int>(info, 0x18) = 512;
    }
    return result;
}

static void* __cdecl LoadMipStub(void* result, void*, void*, int first, int last)
{
    Log("  load mip levels %d through %d\n", first, last);
    *static_cast<int*>(result) = 0;
    return result;
}

static const void* __cdecl UpdateResourceName(const void* texture)
{
    return static_cast<const char*>(texture) + 0x10;
}

static void ExerciseUpdate(const Api& api)
{
    void* manager = TestNew(0xd8, "updateManager%d", 0);
    api.ctor(manager, nullptr);
    unsigned char textures[9][0x60]{};
    void* vtable[32]{};
    unsigned char display[0x38]{};
    vtable[21] = reinterpret_cast<void*>(&TexDisplay);
    At<int>(display, 0x10) = 1024;
    At<int>(display, 0x14) = 512;
    At<int>(display, 0x20) = 11;
    for (int i = 0; i < 9; ++i) {
        At<void*>(textures[i], 0) = vtable;
        names[cur][textures[i]] = "updateTex" + std::to_string(i);
        api.addDownsize(manager, textures[i], i);
    }
    void* activeVtable[32]{};
    activeVtable[21] = reinterpret_cast<void*>(&UpdateDisplay);
    At<void*>(textures[2], 0) = activeVtable;
    At<int>(textures[2], 0x54) = 1024;
    At<int>(textures[2], 0x58) = 512;
    At<void*>(textures[2], 0x18) = display;
    updateDisplay = display;
    void* changes[] = {Proc(mod[0], "?ChangeMipCount@TextureRes@Jet@@QAEXABUSize@2@H_N@Z"),
                       Proc(mod[1], "?ChangeMipCount@TextureRes@Jet@@QAEXABUSize@2@H_N@Z")};
    unsigned char saved[2][5];
    for (int i = 0; i < 2; ++i) {
        std::memcpy(saved[i], changes[i], 5);
        PatchJump(changes[i], reinterpret_cast<void*>(&ChangeMipStub));
    }
    api.setEnable(false);
    Log("op update disabled\n");
    api.update(manager);
    Log("  queue %zu\n", Items(At<void*>(manager, 0xac)).size());
    api.setEnable(true);
    for (int pass = 0; pass < 3; ++pass) {
        Log("op update pass %d\n", pass);
        api.update(manager);
        Log("  queue %zu\n", Items(At<void*>(manager, 0xac)).size());
    }
    void* request = TestNew(0x18, "failedRequest%d", 0);
    At<void*>(request, 0) = textures[0];
    At<int>(request, 0x14) = 2;
    At<void*>(textures[0], 0x4c) = request;
    At<void*>(manager, 0x38) = request;
    At<int>(manager, 0xa4) = 3;
    Log("op update failed request\n");
    api.update(manager);
    Log("  state %d pending %d\n", At<int>(manager, 0xa4), api.isResizing(manager, textures[0]));
    auto** resourceSystem = static_cast<void**>(Proc(GetModuleHandleA("KernelResource_7c.dll"),
        "?resourceSystem@ResourceDatabase@Jet@@0PAVResourceSystem@2@A"));
    void* const savedSystem = *resourceSystem;
    unsigned char fakeSystem[0x40]{};
    unsigned char fakeLoader[0x20]{};
    At<void*>(fakeSystem, 0x14) = fakeLoader;
    *resourceSystem = fakeSystem;
    api.addAsync(manager, textures[0], nullptr, 0, 1);
    At<int>(fakeLoader, 8) = 1;
    Log("op update resource loader busy\n");
    api.update(manager);
    Log("  queue %zu pending %d\n", Items(At<void*>(manager, 0x34)).size(), api.isResizing(manager, textures[0]));
    At<int>(fakeLoader, 8) = 0;
    Log("op update invalid async request\n");
    api.update(manager);
    Log("  queue %zu pending %d state %d\n", Items(At<void*>(manager, 0x34)).size(),
        api.isResizing(manager, textures[0]), At<int>(manager, 0xa4));
    const char* loadName = "?Load@TextureResource@Jet@@SA?AV?$ResultClass@$1?results@Resource@Jet@@2QBUResultTable@2@BW4Results@Resource@Jet@@@2@PBVResourceDatabase@2@PBDABVString@2@AAH3PAUTextureInfo@12@@Z";
    void* loads[] = {Proc(mod[0], loadName), Proc(mod[1], loadName)};
    unsigned char loadBytes[2][5];
    for (int i = 0; i < 2; ++i) {
        std::memcpy(loadBytes[i], loads[i], 5);
        PatchJump(loads[i], reinterpret_cast<void*>(&FailedLoadStub));
    }
    void* workerVtable[32]{};
    workerVtable[6] = reinterpret_cast<void*>(&UpdateResourceName);
    workerVtable[7] = reinterpret_cast<void*>(&UpsizeResource);
    At<void*>(textures[0], 0) = workerVtable;
    api.addAsync(manager, textures[0], dbObj, 0, 1);
    Log("op update starts worker\n");
    api.update(manager);
    for (int wait = 0; wait < 5000 && At<volatile int>(manager, 0xa4) == 1; ++wait) Sleep(1);
    Log("  worker state %d\n", At<int>(manager, 0xa4));
    api.update(manager);
    Log("  final state %d pending %d\n", At<int>(manager, 0xa4), api.isResizing(manager, textures[0]));
    for (int i = 0; i < 2; ++i) {
        DWORD protection;
        VirtualProtect(loads[i], 5, PAGE_EXECUTE_READWRITE, &protection);
        std::memcpy(loads[i], loadBytes[i], 5);
        VirtualProtect(loads[i], 5, protection, &protection);
        FlushInstructionCache(GetCurrentProcess(), loads[i], 5);
    }
    const char* mipName = "?LoadMipLevels@TextureResource@Jet@@SA?AV?$ResultClass@$1?results@Resource@Jet@@2QBUResultTable@2@BW4Results@Resource@Jet@@@2@PAUTextureInfo@12@PAVDisplayTexture@2@HH@Z";
    void* mipLoads[] = {Proc(mod[0], mipName), Proc(mod[1], mipName)};
    unsigned char mipBytes[2][5];
    for (int i = 0; i < 2; ++i) {
        std::memcpy(mipBytes[i], mipLoads[i], 5);
        PatchJump(mipLoads[i], reinterpret_cast<void*>(&LoadMipStub));
    }
    At<int>(manager, 0x3c + 0x14) = 1024;
    At<int>(manager, 0x3c + 0x18) = 512;
    for (int oldLevels : {1, 4, 8}) {
        At<int>(display, 0x20) = oldLevels;
        Log("op append mips old %d\n", oldLevels);
        api.append(manager, nullptr, textures[2], static_cast<char*>(manager) + 0x3c, 9);
    }
    void* completed = TestNew(0x18, "completedRequest%d", 0);
    At<void*>(completed, 0) = textures[2];
    At<int>(completed, 0xc) = 9;
    At<int>(completed, 0x14) = 2;
    At<void*>(textures[2], 0x4c) = completed;
    At<void*>(manager, 0x38) = completed;
    At<int>(manager, 0xa4) = 2;
    Log("op update completed request\n");
    api.update(manager);
    Log("  state %d pending %d\n", At<int>(manager, 0xa4), api.isResizing(manager, textures[2]));
    auto testUnload = As<void(__cdecl*)(void*, int)>(Proc(mod[cur], "?TestUnloadingMips@TextureRes@Jet@@QAAXH@Z"));
    auto removeMips = As<void(__cdecl*)(void*, void*, int)>(Proc(mod[cur], "?DownSizeRemoveMips@TextureManager@Jet@@IAAXPAVTextureRes@2@H@Z"));
    for (int size : {1, 64, 1024}) {
        Log("op test unload mips size %d\n", size);
        testUnload(textures[2], size);
    }
    for (int levels : {3, 8, 12}) {
        Log("op remove mips levels %d\n", levels);
        removeMips(manager, textures[2], levels);
    }
    for (int i = 0; i < 2; ++i) PatchJump(loads[i], reinterpret_cast<void*>(&FailedLoadStub));
    auto remaining = As<void(__cdecl*)(void*, void*)>(Proc(mod[cur], "?TestLoadRemainingMips@TextureRes@Jet@@QAAXPAVResourceDatabase@2@@Z"));
    At<void*>(textures[2], 0x3c) = resObj;
    At<int>(display, 0x20) = 8;
    for (int mode = 0; mode < 3; ++mode) {
        testLoadResult = mode == 0;
        testLoadLast = mode == 1 ? 6 : 9;
        Log("op test remaining mips mode %d\n", mode);
        remaining(textures[2], dbObj);
    }
    testLoadResult = 1;
    for (int i = 0; i < 2; ++i) {
        DWORD protection;
        VirtualProtect(loads[i], 5, PAGE_EXECUTE_READWRITE, &protection);
        std::memcpy(loads[i], loadBytes[i], 5);
        VirtualProtect(loads[i], 5, protection, &protection);
        FlushInstructionCache(GetCurrentProcess(), loads[i], 5);
    }
    for (int i = 0; i < 2; ++i) {
        DWORD protection;
        VirtualProtect(mipLoads[i], 5, PAGE_EXECUTE_READWRITE, &protection);
        std::memcpy(mipLoads[i], mipBytes[i], 5);
        VirtualProtect(mipLoads[i], 5, protection, &protection);
        FlushInstructionCache(GetCurrentProcess(), mipLoads[i], 5);
    }
    *resourceSystem = savedSystem;
    for (int i = 0; i < 2; ++i) {
        DWORD old;
        VirtualProtect(changes[i], 5, PAGE_EXECUTE_READWRITE, &old);
        std::memcpy(changes[i], saved[i], 5);
        VirtualProtect(changes[i], 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), changes[i], 5);
    }
    releaseRef(manager, nullptr);
    for (const auto& texture : textures) names[cur].erase(texture);
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
    ExerciseDownsize(a, m);
    ExerciseAsyncQueue(a, m);
    ExerciseCleanup(a, m);
    ExerciseActiveCancellation(a, m);
    ExerciseUpsize(a, m);
    ExerciseTextureResAccessors(a);
    ExerciseTextureResVirtuals();
    ExerciseForceTextureLoad();
    ExerciseChangeMipCount();
    ExerciseTextureResLifetime(a);
    ExerciseSignedColor();
    ExerciseSharedTextures();
    ExerciseSharedGeneration(a);
    ExerciseDefaultSharedTextures();
    ExerciseSubstituteTexture();
    ExerciseD2Texture();
    ExerciseSpotTexture();
    ExerciseNormalTexture();
    ExercisePowerTextures();
    ExerciseMaterialConversions();
    ExerciseMaterialSpec();
    ExerciseMaterialFields();
    ExerciseMaterialConfigExtract();
    ExerciseMaterialNameSplit();
    ExerciseNullMaterial();
    ExerciseMaterialPropertyMutation();
    ExerciseMaterialInstance();
    ExerciseMaterialManager();
    ExerciseCheckDownsize(a, m);
    Log("op refresh settings\n");
    a.refresh(m);
    for (void* t : tex) DumpTex(t);
    Log("op release manager\n");
    releaseRef(m, nullptr);  // the last reference: DeleteThis, the deleting destructor
    ExerciseCheckTextures(a);
    ExerciseBulkResizing(a);
    ExerciseUpdate(a);
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
    originalSubstitute = Proc(o, "?GenerateSubstituteLoadTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@II@Z");
    std::memcpy(originalSubstituteBytes, originalSubstitute, 5);
    PatchJump(originalSubstitute, reinterpret_cast<void*>(&GenerateStub));
    ownSubstitute = Proc(mod[0], "?GenerateSubstituteLoadTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@II@Z");
    std::memcpy(ownSubstituteBytes, ownSubstitute, 5);
    PatchJump(ownSubstitute, reinterpret_cast<void*>(&GenerateStub));
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
