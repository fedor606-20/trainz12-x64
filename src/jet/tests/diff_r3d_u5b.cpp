// Differential test of Render3DCore_7c unit U5b: the reconstructed multipass material leaves, the two light-render
// containers and the MultiPassBumpChunkBase constructors, against Render3DCore_7c_orig.dll in one process.
//
// The entry points are called through GetProcAddress by their original mangled names on identical inputs and the
// object bytes, return values and side effects (reference counts, freed arrays, virtual calls, interned strings) are
// compared raw. No stand-in derives from a game class: the objects are flat buffers laid out as the TS12 assembly
// reads them, and the few vtables the methods call are local arrays with recording hooks. `this`-taking members are
// called through function pointers with the member ABI the exports use (__thiscall for constructors and the
// appenders, __cdecl for everything else, including the virtual methods).
//
// usage: diff_r3d_u5b.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr unsigned kLightSize = 0x38;
constexpr unsigned kMPLightSize = 0x50;
constexpr unsigned kPassSize = 0x104;
constexpr unsigned kMaterialSize = 0x100;

// ---- byte-level comparison ------------------------------------------------------------------------------------

HMODULE mod[2];
unsigned long long checks, bad;
std::mt19937 rng(0x5eed5b5b);
int printed;

int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }


uint32_t RandBits(bool awkward)
{
    if (!awkward) return static_cast<uint32_t>(rng());
    switch (Pick(0, 9)) {
    case 0: return 0x00000000u;
    case 1: return 0x80000000u;  // -0.0f
    case 2: return 0x7f800000u;  // +inf
    case 3: return 0xff800000u;  // -inf
    case 4: return 0x7fc00000u;  // qNaN
    case 5: return 0xffc00001u;  // qNaN, payload
    case 6: return 0x7fa00000u;  // sNaN
    case 7: return 0xffffffffu;
    case 8: return 0x7fffff7fu;
    default: return static_cast<uint32_t>(rng());
    }
}


bool SameBytes(const void* a, const void* b, size_t n) { return std::memcmp(a, b, n) == 0; }

void DumpFirst(const unsigned char* a, const unsigned char* b, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            std::printf("  first difference at +0x%zx: ours %02x orig %02x\n", i, a[i], b[i]);
            return;
        }
    }
}

void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        if (printed++ < 40) std::printf("MISMATCH %s\n", what);
    }
}

void CheckBytes(const void* ours, const void* orig, size_t n, const char* what)
{
    ++checks;
    if (!SameBytes(ours, orig, n)) {
        ++bad;
        if (printed++ < 40) {
            std::printf("MISMATCH %s\n", what);
            DumpFirst(static_cast<const unsigned char*>(ours), static_cast<const unsigned char*>(orig), n);
        }
    }
}

void CheckValue(unsigned long long ours, unsigned long long orig, const char* what)
{
    ++checks;
    if (ours != orig) {
        ++bad;
        if (printed++ < 40) std::printf("MISMATCH %s: ours %llu (0x%llx) orig %llu (0x%llx)\n", what, ours, ours, orig,
                                       orig);
    }
}

// ---- the original module's exports ---------------------------------------------------------------------------

template <class T>
T Proc(int side, const char* name)
{
    auto p = reinterpret_cast<T>(GetProcAddress(mod[side], name));
    if (!p) {
        std::printf("missing export %s in %s\n", name, side ? "orig" : "ours");
        std::exit(2);
    }
    return p;
}

// KernelStandard's allocator, shared with both sides (their inlined Array growth calls the same entry).
using NewFn = void* (__cdecl*)(unsigned int);
using DeleteFn = void(__cdecl*)(void*);
NewFn ksNew;
DeleteFn ksDelete;

void* KsNew(unsigned bytes) { return ksNew ? ksNew(bytes) : std::malloc(bytes); }
void KsDelete(void* p)
{
    if (!p) return;
    if (ksDelete)
        ksDelete(p);
    else
        std::free(p);
}

// ---- PString cache access for IsSupported (flat view; the cache itself belongs to KernelStandard) -------------

struct PStr {
    void* node;
};
struct PNode {
    void* next;
    void* prev;
    unsigned int length;
    unsigned int capacity;
    int refs;
    unsigned int id;
    char text[1];
};
void** cacheSlot;
using NodeCreateFn = PNode* (__cdecl*)(void*, const unsigned char*);
using NodeDestroyFn = void(__cdecl*)(void*, PNode*);
using CacheLockFn = void(__cdecl*)(void*);
using CacheUnlockFn = void(__cdecl*)(void*);
NodeCreateFn nodeCreate;
NodeDestroyFn nodeDestroy;
CacheLockFn cacheLock;
CacheUnlockFn cacheUnlock;

void AddCacheRef(PNode* node)
{
    if (!node) return;
    cacheLock(*cacheSlot);
    ++node->refs;
    ++*reinterpret_cast<int*>(static_cast<unsigned char*>(*cacheSlot) + 4);
    cacheUnlock(*cacheSlot);
}

// ---- flat stand-ins used by Activate and IsSupported ---------------------------------------------------------

struct FakeTexture {
    void** vtable;
};
struct FakeDisplay {
    void** vtable;
};

std::vector<std::pair<void*, int>> activateLog;
void __cdecl ActivateHook(void* self, int value) { activateLog.emplace_back(self, value); }

int capabilityResult[3];
int capabilityCode[3];
int capabilitySide;
bool __cdecl CapabilityHook(void* self, int code)
{
    (void)self;
    for (int i = 0; i < 3; ++i) {
        if (capabilityCode[i] == code && capabilityResult[i]) return true;
    }
    return false;
}

struct DummyRef {
    void* vtable;
    int refs;
};
int dummyDeletes[2];
int dummySide;
void __cdecl DummyDelete(void* self)
{
    (void)self;
    ++dummyDeletes[dummySide];
}
void* dummyVtable[1] = {reinterpret_cast<void*>(&DummyDelete)};

// ---- exported entry points, one set per side ----------------------------------------------------------------

struct Api {
    void* (__thiscall* mplCtor)(void*);
    void(__thiscall* mplDtor)(void*);
    void* (__thiscall* lightCtor)(void*, unsigned, unsigned, unsigned, unsigned, unsigned);
    unsigned(__cdecl* numLights)(const void*, int);
    unsigned(__cdecl* numPasses)(const void*);
    unsigned(__cdecl* totalLights)(const void*);
    void(__thiscall* addTexture)(void*, int, unsigned);
    void(__thiscall* addVPProperty)(void*, unsigned, unsigned, unsigned);
    void(__thiscall* addTCProperty)(void*, unsigned, unsigned, unsigned);
    void(__thiscall* addScissor)(void*, unsigned);
    void* (__thiscall* chunkCtor)(void*);
    void* (__thiscall* chunkCopyCtor)(void*, const void*);

    unsigned char(__cdecl* getSmooth)(const void*);
    void(__cdecl* setSmooth)(void*, unsigned char);
    unsigned char(__cdecl* getSingle)(const void*);
    void(__cdecl* setSingle)(void*, unsigned char);
    unsigned char(__cdecl* getIntensity)(const void*);
    void(__cdecl* setIntensity)(void*, unsigned char);
    unsigned char(__cdecl* getDebug)(const void*);
    void(__cdecl* setDebug)(void*, unsigned char);

    const void* (__cdecl* typeNoTexture)(const void*);
    const void* (__cdecl* typeOneTexture)(const void*);
    const void* (__cdecl* typeBump)(const void*);
    const void* (__cdecl* typeBump2)(const void*);
    const void* (__cdecl* typeBumpSpec)(const void*);
    const void* (__cdecl* typeBumpSpec2)(const void*);

    void* (__cdecl* texNoTexture)(void*, int);
    void* (__cdecl* texNoTextureC)(const void*, int);
    void* (__cdecl* texOneTexture)(void*, int);
    void* (__cdecl* texOneTextureC)(const void*, int);
    void* (__cdecl* texBump)(void*, int);
    void* (__cdecl* texBumpC)(const void*, int);

    void(__cdecl* setAlpha)(void*, unsigned);
    unsigned(__cdecl* getAlpha)(const void*);

    void(__cdecl* activateNoTexture)(void*, int);
    void(__cdecl* activateOneTexture)(void*, int);
    void(__cdecl* activateBump)(void*, int);

    void(__cdecl* updateNoTexture)(void*);
    void(__cdecl* updateOneTexture)(void*);
    void(__cdecl* updateBump)(void*);

    bool(__cdecl* supportedNoTexture)(void*, void*, PStr*);
    bool(__cdecl* supportedOneTexture)(void*, void*, PStr*);
    bool(__cdecl* supportedBump)(void*, void*, PStr*);
    bool(__cdecl* supportedBumpSpec)(void*, void*, PStr*);
};

Api Load(int side)
{
    Api a{};
    a.mplCtor = Proc<decltype(a.mplCtor)>(side, "??0MPLightRender@MultiPassRenderState@Jet@@QAE@XZ");
    a.mplDtor = Proc<decltype(a.mplDtor)>(side, "??1MPLightRender@MultiPassRenderState@Jet@@MAE@XZ");
    a.lightCtor = Proc<decltype(a.lightCtor)>(side, "??0MultiPassLightRender@Jet@@QAE@IIIII@Z");
    a.numLights = Proc<decltype(a.numLights)>(side, "?GetNumLights@MultiPassLightRender@Jet@@QBAIW4DLightTypes@2@@Z");
    a.numPasses = Proc<decltype(a.numPasses)>(side, "?GetNumPasses@MultiPassLightRender@Jet@@QBAIXZ");
    a.totalLights = Proc<decltype(a.totalLights)>(side, "?GetTotalLights@MultiPassLightRender@Jet@@QBAIXZ");
    a.addTexture = Proc<decltype(a.addTexture)>(side, "?AddTexture@MultiPassLightRender@Jet@@QAEXHI@Z");
    a.addVPProperty =
        Proc<decltype(a.addVPProperty)>(side, "?AddVPProperty@MultiPassLightRender@Jet@@QAEXW4MP_RenderProperties@2@II@Z");
    a.addTCProperty =
        Proc<decltype(a.addTCProperty)>(side, "?AddTCProperty@MultiPassLightRender@Jet@@QAEXW4MP_RenderProperties@2@II@Z");
    a.addScissor = Proc<decltype(a.addScissor)>(side, "?AddScissorLight@MultiPassLightRender@Jet@@QAEXI@Z");
    a.chunkCtor = Proc<decltype(a.chunkCtor)>(side, "??0MultiPassBumpChunkBase@Jet@@QAE@XZ");
    a.chunkCopyCtor = Proc<decltype(a.chunkCopyCtor)>(side, "??0MultiPassBumpChunkBase@Jet@@QAE@ABV01@@Z");

    a.getSmooth = Proc<decltype(a.getSmooth)>(side, "?GetSmoothBlendMode@MultiPassNoTextureMaterial@Jet@@QBA_NXZ");
    a.setSmooth = Proc<decltype(a.setSmooth)>(side, "?SetSmoothBlendMode@MultiPassNoTextureMaterial@Jet@@QAAX_N@Z");
    a.getSingle = Proc<decltype(a.getSingle)>(side, "?GetSinglePassMode@MultiPassNoTextureMaterial@Jet@@QBA_NXZ");
    a.setSingle = Proc<decltype(a.setSingle)>(side, "?SetSinglePassMode@MultiPassNoTextureMaterial@Jet@@QAAX_N@Z");
    a.getIntensity = Proc<decltype(a.getIntensity)>(side, "?GetIntensityMode@MultiPassNoTextureMaterial@Jet@@QBA_NXZ");
    a.setIntensity = Proc<decltype(a.setIntensity)>(side, "?SetIntensityMode@MultiPassNoTextureMaterial@Jet@@QAAX_N@Z");
    a.getDebug = Proc<decltype(a.getDebug)>(side, "?GetDebugPassMode@MultiPassNoTextureMaterial@Jet@@QBA_NXZ");
    a.setDebug = Proc<decltype(a.setDebug)>(side, "?SetDebugPassMode@MultiPassNoTextureMaterial@Jet@@QAAX_N@Z");

    a.typeNoTexture = Proc<decltype(a.typeNoTexture)>(side, "?GetType@MultiPassNoTextureMaterial@Jet@@UBAABVPString@2@XZ");
    a.typeOneTexture = Proc<decltype(a.typeOneTexture)>(side, "?GetType@MultiPassOneTextureMaterial@Jet@@UBAABVPString@2@XZ");
    a.typeBump = Proc<decltype(a.typeBump)>(side, "?GetType@MultiPassBumpMaterial@Jet@@UBAABVPString@2@XZ");
    a.typeBump2 = Proc<decltype(a.typeBump2)>(side, "?GetType@MultiPassBump2Material@Jet@@UBAABVPString@2@XZ");
    a.typeBumpSpec = Proc<decltype(a.typeBumpSpec)>(side, "?GetType@MultiPassBumpSpecMaterial@Jet@@UBAABVPString@2@XZ");
    a.typeBumpSpec2 = Proc<decltype(a.typeBumpSpec2)>(side, "?GetType@MultiPassBumpSpec2Material@Jet@@UBAABVPString@2@XZ");

    a.texNoTexture = Proc<decltype(a.texNoTexture)>(side, "?GetTexture@MultiPassNoTextureMaterial@Jet@@UAAPAVTexture@2@H@Z");
    a.texNoTextureC = Proc<decltype(a.texNoTextureC)>(side, "?GetTexture@MultiPassNoTextureMaterial@Jet@@UBAPBVTexture@2@H@Z");
    a.texOneTexture = Proc<decltype(a.texOneTexture)>(side, "?GetTexture@MultiPassOneTextureMaterial@Jet@@UAAPAVTexture@2@H@Z");
    a.texOneTextureC =
        Proc<decltype(a.texOneTextureC)>(side, "?GetTexture@MultiPassOneTextureMaterial@Jet@@UBAPBVTexture@2@H@Z");
    a.texBump = Proc<decltype(a.texBump)>(side, "?GetTexture@MultiPassBumpMaterial@Jet@@UAAPAVTexture@2@H@Z");
    a.texBumpC = Proc<decltype(a.texBumpC)>(side, "?GetTexture@MultiPassBumpMaterial@Jet@@UBAPBVTexture@2@H@Z");

    a.setAlpha = Proc<decltype(a.setAlpha)>(side, "?SetAlphaHint@MultiPassOneTextureMaterial@Jet@@QAAXW4TextureAlpha@2@@Z");
    a.getAlpha = Proc<decltype(a.getAlpha)>(side, "?GetAlphaHint@MultiPassOneTextureMaterial@Jet@@QAA?AW4TextureAlpha@2@XZ");

    a.activateNoTexture = Proc<decltype(a.activateNoTexture)>(side, "?Activate@MultiPassNoTextureMaterial@Jet@@UAAXH@Z");
    a.activateOneTexture = Proc<decltype(a.activateOneTexture)>(side, "?Activate@MultiPassOneTextureMaterial@Jet@@UAAXH@Z");
    a.activateBump = Proc<decltype(a.activateBump)>(side, "?Activate@MultiPassBumpMaterial@Jet@@UAAXH@Z");

    a.updateNoTexture = Proc<decltype(a.updateNoTexture)>(side, "?UpdateTextureArray@MultiPassNoTextureMaterial@Jet@@MAAXXZ");
    a.updateOneTexture = Proc<decltype(a.updateOneTexture)>(side, "?UpdateTextureArray@MultiPassOneTextureMaterial@Jet@@MAAXXZ");
    a.updateBump = Proc<decltype(a.updateBump)>(side, "?UpdateTextureArray@MultiPassBumpMaterial@Jet@@MAAXXZ");

    a.supportedNoTexture = Proc<decltype(a.supportedNoTexture)>(
        side, "?IsSupported@MultiPassNoTextureMaterialPlugin@Jet@@UAA_NPAVDisplay@2@AAVPString@2@@Z");
    a.supportedOneTexture = Proc<decltype(a.supportedOneTexture)>(
        side, "?IsSupported@MultiPassOneTextureMaterialPlugin@Jet@@UAA_NPAVDisplay@2@AAVPString@2@@Z");
    a.supportedBump = Proc<decltype(a.supportedBump)>(
        side, "?IsSupported@MultiPassBumpMaterialPlugin@Jet@@UAA_NPAVDisplay@2@AAVPString@2@@Z");
    a.supportedBumpSpec = Proc<decltype(a.supportedBumpSpec)>(
        side, "?IsSupported@MultiPassBumpSpecMaterialPlugin@Jet@@UAA_NPAVDisplay@2@AAVPString@2@@Z");
    return a;
}

// ---- the scenarios ------------------------------------------------------------------------------------------

void TestMPLightCtor(const Api api[2], bool awkward)
{
    unsigned char ours[kMPLightSize], orig[kMPLightSize];
    std::memset(ours, 0xa5, sizeof ours);
    std::memset(orig, 0xa5, sizeof orig);
    api[0].mplCtor(ours);
    api[1].mplCtor(orig);
    CheckBytes(ours, orig, sizeof ours, "MPLightRender ctor");
    (void)awkward;
}

// Reference-counted dummy elements: ReleaseReference calls vtable slot 0 (DeleteThis) when the count reaches zero.
struct DummyLayout {
    int count[6];
    int refs[6][3];
    bool bufferWhenEmpty[6];
};

DummyLayout MakeDummyLayout(bool awkward)
{
    DummyLayout l{};
    for (int a = 0; a < 6; ++a) {
        l.count[a] = awkward && a == 2 ? 0 : Pick(0, 3);
        l.bufferWhenEmpty[a] = awkward && a == 4;
        for (int i = 0; i < 3; ++i) l.refs[a][i] = 1 + Pick(0, 3);
    }
    return l;
}

void BuildMPLightArrays(unsigned char* object, const DummyLayout& layout, DummyRef* pool)
{
    unsigned int* const counts = reinterpret_cast<unsigned int*>(object + 0x08);
    void** const data = reinterpret_cast<void**>(object + 0x10);
    int used = 0;
    for (int a = 0; a < 6; ++a) {
        const int count = layout.count[a];
        counts[a * 3] = static_cast<unsigned>(count);      // count
        counts[a * 3 + 1] = static_cast<unsigned>(count);  // capacity (not read by the destructor)
        void** const block = count || layout.bufferWhenEmpty[a]
                                 ? static_cast<void**>(KsNew(static_cast<unsigned>(count ? count : 1) * 4))
                                 : nullptr;
        data[a * 3] = block;
        if (block) {
            for (int i = 0; i < count; ++i) {
                DummyRef* const dummy = &pool[used++];
                dummy->vtable = dummyVtable;
                dummy->refs = layout.refs[a][i];
                block[i] = dummy;
            }
        }
    }
}

void TestMPLightDtor(const Api api[2], bool awkward)
{
    const DummyLayout layout = MakeDummyLayout(awkward);
    // Both objects start from the original's constructor so the only variable is the destructor.
    alignas(8) unsigned char ours[kMPLightSize], orig[kMPLightSize];
    std::memset(ours, 0, sizeof ours);
    std::memset(orig, 0, sizeof orig);
    DummyRef pool[2][24];
    std::memset(pool, 0, sizeof pool);
    BuildMPLightArrays(ours, layout, pool[0]);

    BuildMPLightArrays(orig, layout, pool[1]);


    dummySide = 0;
    dummyDeletes[0] = dummyDeletes[1] = 0;

    api[0].mplDtor(ours);

    dummySide = 1;
    api[1].mplDtor(orig);


    // The six data pointers are left dangling by both destructors, and the two sides' blocks are at different
    // addresses: compare every other word.
    for (unsigned offset = 0; offset < kMPLightSize; offset += 4) {
        if (offset == 0x10 || offset == 0x1c || offset == 0x28 || offset == 0x34 || offset == 0x40 || offset == 0x4c)
            continue;
        CheckValue(*reinterpret_cast<unsigned*>(ours + offset), *reinterpret_cast<unsigned*>(orig + offset),
                   "MPLightRender dtor object");
    }
    CheckValue(static_cast<unsigned long long>(dummyDeletes[0]), static_cast<unsigned long long>(dummyDeletes[1]),
               "MPLightRender dtor DeleteThis count");
    int used = 0;
    for (int a = 0; a < 6; ++a) {
        for (int i = 0; i < layout.count[a]; ++i) {
            CheckValue(static_cast<unsigned>(pool[0][used].refs), static_cast<unsigned>(pool[1][used].refs),
                       "MPLightRender dtor element refs");
            ++used;
        }
    }
}

void TestLightRender(const Api api[2], bool awkward)
{
    unsigned char ours[kLightSize + 0x40], orig[kLightSize + 0x40];
    std::memset(ours, 0xa5, sizeof ours);
    std::memset(orig, 0xa5, sizeof orig);
    const unsigned args[5] = {RandBits(awkward), RandBits(awkward), RandBits(awkward), RandBits(awkward),
                               RandBits(awkward)};
    api[0].lightCtor(ours, args[0], args[1], args[2], args[3], args[4]);
    api[1].lightCtor(orig, args[0], args[1], args[2], args[3], args[4]);
    CheckBytes(ours, orig, kLightSize, "MultiPassLightRender ctor");

    for (int type = -1; type <= 6; ++type) {
        CheckValue(api[0].numLights(ours, type), api[1].numLights(orig, type), "GetNumLights");
    }
    CheckValue(api[0].numPasses(ours), api[1].numPasses(orig), "GetNumPasses");
    CheckValue(api[0].totalLights(ours), api[1].totalLights(orig), "GetTotalLights");

    // constructor parameters map to specific counters
    CheckValue(*reinterpret_cast<unsigned*>(ours + 0x20), *reinterpret_cast<unsigned*>(orig + 0x20), "light arg1");
    CheckValue(*reinterpret_cast<unsigned*>(ours + 0x24), *reinterpret_cast<unsigned*>(orig + 0x24), "light arg3");
    CheckValue(*reinterpret_cast<unsigned*>(ours + 0x28), *reinterpret_cast<unsigned*>(orig + 0x28), "light arg4");
    CheckValue(*reinterpret_cast<unsigned*>(ours + 0x2c), *reinterpret_cast<unsigned*>(orig + 0x2c), "light arg2");
    CheckValue(*reinterpret_cast<unsigned*>(ours + 0x30), *reinterpret_cast<unsigned*>(orig + 0x30), "light arg5");
}

struct UIntArray {
    unsigned* data;
};

UIntArray PrepareUIntArray(unsigned char* object, unsigned offset, unsigned count, unsigned capacity, bool awkward)
{
    UIntArray a{nullptr};
    unsigned* const header = reinterpret_cast<unsigned*>(object + offset);
    header[0] = count;
    header[1] = capacity;
    if (capacity) a.data = static_cast<unsigned*>(KsNew(capacity * 4));
    *reinterpret_cast<unsigned**>(object + offset + 8) = a.data;
    for (unsigned i = 0; i < capacity && a.data; ++i) a.data[i] = RandBits(awkward);
    return a;
}

void TestAddScissor(const Api api[2], bool awkward)
{
    const unsigned cases[][2] = {{0, 0}, {0, 4}, {2, 4}, {4, 4}, {5, 10}, {1, 1}};
    for (const auto& c : cases) {
        unsigned char ours[0x40], orig[0x40];
        std::memset(ours, 0xa5, sizeof ours);
        std::memset(orig, 0xa5, sizeof orig);
        UIntArray ua = PrepareUIntArray(ours, 0x14, c[0], c[1], awkward);
        UIntArray ub = PrepareUIntArray(orig, 0x14, c[0], c[1], awkward);
        if (ua.data && ub.data && c[1]) std::memcpy(ub.data, ua.data, c[1] * 4);
        const unsigned value = RandBits(awkward);
        api[0].addScissor(ours, value);
        api[1].addScissor(orig, value);
        const unsigned* ha = reinterpret_cast<const unsigned*>(ours + 0x14);
        const unsigned* hb = reinterpret_cast<const unsigned*>(orig + 0x14);
        CheckValue(ha[0], hb[0], "AddScissorLight count");
        CheckValue(ha[1], hb[1], "AddScissorLight capacity");

        const unsigned* da = *reinterpret_cast<unsigned**>(ours + 0x1c);
        const unsigned* db = *reinterpret_cast<unsigned**>(orig + 0x1c);
        for (unsigned i = 0; i < ha[0] && i < hb[0]; ++i) CheckValue(da[i], db[i], "AddScissorLight item");
        KsDelete(*reinterpret_cast<void**>(ours + 0x1c));
        KsDelete(*reinterpret_cast<void**>(orig + 0x1c));
    }
}

void TestPropertyAppenders(const Api api[2], bool awkward, int which)
{
    // which: 0 = texture list; 1..3 = vertex-property list 0..2; 4..6 = texture-combiner list 0..2.
    const unsigned base = which == 0 ? 0x00u : which <= 3 ? 0x0c + (which - 1) * 0x0c : 0x84 + (which - 4) * 0x0c;
    for (int initial = 0; initial <= 6; ++initial) {
        for (int cap : {0, 4, 8}) {
            if (cap < initial) continue;
            unsigned char ours[0x50], orig[0x50];
            std::memset(ours, 0xa5, sizeof ours);
            std::memset(orig, 0xa5, sizeof orig);
            *reinterpret_cast<unsigned*>(ours + 0x08) = 1;
            *reinterpret_cast<unsigned*>(orig + 0x08) = 1;
            unsigned char passA[kPassSize], passB[kPassSize];
            std::memset(passA, 0x5a, sizeof passA);
            std::memset(passB, 0x5a, sizeof passB);
            *reinterpret_cast<void**>(ours + 0x10) = passA;
            *reinterpret_cast<void**>(orig + 0x10) = passB;

            unsigned* const ha = reinterpret_cast<unsigned*>(passA + base);
            unsigned* const hb = reinterpret_cast<unsigned*>(passB + base);
            ha[0] = hb[0] = static_cast<unsigned>(initial);
            ha[1] = hb[1] = static_cast<unsigned>(cap);
            unsigned* const da = cap ? static_cast<unsigned*>(KsNew(static_cast<unsigned>(cap) * 8)) : nullptr;
            unsigned* const db = cap ? static_cast<unsigned*>(KsNew(static_cast<unsigned>(cap) * 8)) : nullptr;
            for (int i = 0; i < cap * 2 && da; ++i) da[i] = db[i] = RandBits(awkward);
            ha[2] = reinterpret_cast<unsigned>(da);
            hb[2] = reinterpret_cast<unsigned>(db);

            const unsigned property = RandBits(awkward);
            const unsigned value = RandBits(awkward);
            if (which == 0) {
                api[0].addTexture(ours, static_cast<int>(property), value);
                api[1].addTexture(orig, static_cast<int>(property), value);
            } else if (which <= 3) {
                api[0].addVPProperty(ours, property, value, static_cast<unsigned>(which - 1));
                api[1].addVPProperty(orig, property, value, static_cast<unsigned>(which - 1));
            } else {
                api[0].addTCProperty(ours, property, value, static_cast<unsigned>(which - 4));
                api[1].addTCProperty(orig, property, value, static_cast<unsigned>(which - 4));
            }
            CheckValue(ha[0], hb[0], "property append count");
            CheckValue(ha[1], hb[1], "property append capacity");
            const unsigned* const oa = reinterpret_cast<const unsigned*>(ha[2]);
            const unsigned* const ob = reinterpret_cast<const unsigned*>(hb[2]);
            for (unsigned i = 0; i < ha[0] * 2 && oa && ob; ++i) CheckValue(oa[i], ob[i], "property append item");

            // every untouched byte outside the chosen list stays as it was, identically on both sides
            static const unsigned probes[] = {0x00, 0x0b, 0x0c, 0x2f, 0x30, 0x54, 0x78, 0x84, 0x9f, 0x100, 0x103};
            for (unsigned i : probes) {
                if (i >= base && i < base + 0x0c) continue;
                CheckValue(passA[i], passB[i], "property append untouched pass");
            }
            KsDelete(reinterpret_cast<void*>(ha[2]));
            KsDelete(reinterpret_cast<void*>(hb[2]));
        }
    }
}

void TestChunkBase(const Api api[2], bool awkward)
{
    unsigned char ours[8], orig[8];
    std::memset(ours, 0xa5, sizeof ours);
    std::memset(orig, 0xa5, sizeof orig);
    api[0].chunkCtor(ours);
    api[1].chunkCtor(orig);
    CheckBytes(ours, orig, sizeof ours, "MultiPassBumpChunkBase ctor");

    std::memset(ours, 0xa5, sizeof ours);
    std::memset(orig, 0xa5, sizeof orig);
    unsigned char source[8];
    for (unsigned char& c : source) c = static_cast<unsigned char>(RandBits(awkward));
    api[0].chunkCopyCtor(ours, source);
    api[1].chunkCopyCtor(orig, source);
    CheckBytes(ours, orig, sizeof ours, "MultiPassBumpChunkBase copy ctor");
}

void TestModes(const Api api[2], bool awkward)
{
    unsigned char ours[kMaterialSize], orig[kMaterialSize];
    std::memset(ours, 0x5a, sizeof ours);
    std::memset(orig, 0x5a, sizeof orig);
    for (unsigned char value : {static_cast<unsigned char>(0),
                                static_cast<unsigned char>(1),
                                static_cast<unsigned char>(0x7f),
                                static_cast<unsigned char>(0x80),
                                static_cast<unsigned char>(RandBits(awkward))}) {
        api[0].setSmooth(ours, value);
        api[1].setSmooth(orig, value);
        api[0].setSingle(ours, value);
        api[1].setSingle(orig, value);
        api[0].setIntensity(ours, value);
        api[1].setIntensity(orig, value);
        api[0].setDebug(ours, value);
        api[1].setDebug(orig, value);
        CheckBytes(ours, orig, sizeof ours, "mode setters");
        CheckValue(api[0].getSmooth(ours), api[1].getSmooth(orig), "GetSmoothBlendMode");
        CheckValue(api[0].getSingle(ours), api[1].getSingle(orig), "GetSinglePassMode");
        CheckValue(api[0].getIntensity(ours), api[1].getIntensity(orig), "GetIntensityMode");
        CheckValue(api[0].getDebug(ours), api[1].getDebug(orig), "GetDebugPassMode");
    }
    // raw initial bytes, including values that are neither 0 nor 1
    for (unsigned char& c : ours) c = static_cast<unsigned char>(RandBits(awkward));
    std::memcpy(orig, ours, sizeof ours);
    CheckValue(api[0].getSmooth(ours), api[1].getSmooth(orig), "GetSmoothBlendMode raw");
    CheckValue(api[0].getSingle(ours), api[1].getSingle(orig), "GetSinglePassMode raw");
    CheckValue(api[0].getIntensity(ours), api[1].getIntensity(orig), "GetIntensityMode raw");
    CheckValue(api[0].getDebug(ours), api[1].getDebug(orig), "GetDebugPassMode raw");
}

void TestTypes(const Api api[2])
{
    const decltype(Api::typeNoTexture) getters[2][6] = {
        {api[0].typeNoTexture, api[0].typeOneTexture, api[0].typeBump, api[0].typeBump2, api[0].typeBumpSpec,
         api[0].typeBumpSpec2},
        {api[1].typeNoTexture, api[1].typeOneTexture, api[1].typeBump, api[1].typeBump2, api[1].typeBumpSpec,
         api[1].typeBumpSpec2}};
    static const char* const names[6] = {"GetType NoTexture", "GetType OneTexture", "GetType Bump",
                                         "GetType Bump2", "GetType BumpSpec", "GetType BumpSpec2"};
    alignas(16) unsigned char object[0x10] = {};
    for (int i = 0; i < 6; ++i) {
        const void* const a = getters[0][i](object);
        const void* const b = getters[1][i](object);
        CheckValue(reinterpret_cast<uintptr_t>(a), reinterpret_cast<uintptr_t>(b), names[i]);
        if (a && b) {
            CheckValue(*reinterpret_cast<const unsigned*>(a), *reinterpret_cast<const unsigned*>(b), names[i]);
            const PNode* const na = *reinterpret_cast<const PNode* const*>(a);
            const PNode* const nb = *reinterpret_cast<const PNode* const*>(b);
            if (na && nb) CheckBytes(na->text, nb->text, na->length + 1, names[i]);
        }
    }
}

void TestTextures(const Api api[2], bool awkward)
{
    unsigned char ours[kMaterialSize], orig[kMaterialSize];
    std::memset(ours, 0xa5, sizeof ours);
    std::memset(orig, 0xa5, sizeof orig);
    for (unsigned offset : {0xa4u, 0xa8u, 0xc4u, 0xd0u}) {
        const unsigned sentinel = (offset == 0xa4 || offset == 0xc4) && awkward ? 0u : RandBits(awkward) | 1u;
        *reinterpret_cast<unsigned*>(ours + offset) = sentinel;
        *reinterpret_cast<unsigned*>(orig + offset) = sentinel;
    }
    for (int slot = -2; slot <= 0x14; ++slot) {
        CheckValue(reinterpret_cast<uintptr_t>(api[0].texNoTexture(ours, slot)),
                   reinterpret_cast<uintptr_t>(api[1].texNoTexture(orig, slot)), "GetTexture NoTexture");
        CheckValue(reinterpret_cast<uintptr_t>(api[0].texNoTextureC(ours, slot)),
                   reinterpret_cast<uintptr_t>(api[1].texNoTextureC(orig, slot)), "GetTexture NoTexture const");
        CheckValue(reinterpret_cast<uintptr_t>(api[0].texOneTexture(ours, slot)),
                   reinterpret_cast<uintptr_t>(api[1].texOneTexture(orig, slot)), "GetTexture OneTexture");
        CheckValue(reinterpret_cast<uintptr_t>(api[0].texOneTextureC(ours, slot)),
                   reinterpret_cast<uintptr_t>(api[1].texOneTextureC(orig, slot)), "GetTexture OneTexture const");
        CheckValue(reinterpret_cast<uintptr_t>(api[0].texBump(ours, slot)),
                   reinterpret_cast<uintptr_t>(api[1].texBump(orig, slot)), "GetTexture Bump");
        CheckValue(reinterpret_cast<uintptr_t>(api[0].texBumpC(ours, slot)),
                   reinterpret_cast<uintptr_t>(api[1].texBumpC(orig, slot)), "GetTexture Bump const");
    }
}

void TestAlpha(const Api api[2], bool awkward)
{
    unsigned char ours[kMaterialSize], orig[kMaterialSize];
    std::memset(ours, 0xa5, sizeof ours);
    std::memset(orig, 0xa5, sizeof orig);
    for (unsigned value : {0u, 1u, 0xffffffffu, RandBits(awkward), RandBits(awkward)}) {
        api[0].setAlpha(ours, value);
        api[1].setAlpha(orig, value);
        CheckBytes(ours, orig, sizeof ours, "SetAlphaHint");
        CheckValue(api[0].getAlpha(ours), api[1].getAlpha(orig), "GetAlphaHint");
    }
}

void TestActivate(const Api api[2], bool awkward)
{
    FakeTexture textures[4];
    void* vtable[0x40 / 4] = {};
    vtable[0x3c / 4] = reinterpret_cast<void*>(&ActivateHook);
    for (FakeTexture& t : textures) t.vtable = vtable;

    unsigned char ours[kMaterialSize], orig[kMaterialSize];
    std::memset(ours, 0xa5, sizeof ours);
    std::memset(orig, 0xa5, sizeof orig);
    const unsigned offsets[4] = {0xa4, 0xa8, 0xc4, 0xd0};
    for (int i = 0; i < 4; ++i) {
        const unsigned value = (i == 1 && awkward) ? 0u : reinterpret_cast<unsigned>(&textures[i]);
        *reinterpret_cast<unsigned*>(ours + offsets[i]) = value;
        *reinterpret_cast<unsigned*>(orig + offsets[i]) = value;
    }
    const int argument = static_cast<int>(RandBits(awkward));

    activateLog.clear();
    api[0].activateNoTexture(ours, argument);
    std::vector<std::pair<void*, int>> log0 = activateLog;
    activateLog.clear();
    api[1].activateNoTexture(orig, argument);
    CheckValue(log0.size(), activateLog.size(), "Activate NoTexture calls");
    Check(log0 == activateLog, "Activate NoTexture log");

    activateLog.clear();
    api[0].activateOneTexture(ours, argument);
    log0 = activateLog;
    activateLog.clear();
    api[1].activateOneTexture(orig, argument);
    CheckValue(log0.size(), activateLog.size(), "Activate OneTexture calls");
    Check(log0 == activateLog, "Activate OneTexture log");

    activateLog.clear();
    api[0].activateBump(ours, argument);
    log0 = activateLog;
    activateLog.clear();
    api[1].activateBump(orig, argument);
    CheckValue(log0.size(), activateLog.size(), "Activate Bump calls");
    Check(log0 == activateLog, "Activate Bump log");
}

void TestUpdate(const Api api[2], const char* name, void(__cdecl* update[2])(void*), unsigned textureCount,
                unsigned preCapacity, bool awkward)
{
    unsigned char ours[kMaterialSize], orig[kMaterialSize];
    std::memset(ours, 0xa5, sizeof ours);
    std::memset(orig, 0xa5, sizeof orig);
    const unsigned offsets[4] = {0xa4, 0xa8, 0xc4, 0xd0};
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned value = i < textureCount ? RandBits(awkward) | 1u : 0u;
        *reinterpret_cast<unsigned*>(ours + offsets[i]) = value;
        *reinterpret_cast<unsigned*>(orig + offsets[i]) = value;
    }
    unsigned* da = preCapacity ? static_cast<unsigned*>(KsNew(preCapacity * 4)) : nullptr;
    unsigned* db = preCapacity ? static_cast<unsigned*>(KsNew(preCapacity * 4)) : nullptr;
    for (unsigned i = 0; i < preCapacity; ++i) da[i] = db[i] = RandBits(awkward);
    for (int s = 0; s < 2; ++s) {
        unsigned char* const o = s ? orig : ours;
        unsigned* const header = reinterpret_cast<unsigned*>(o + 0xb8);
        header[0] = 0;
        header[1] = preCapacity;
        *reinterpret_cast<unsigned**>(o + 0xc0) = s ? db : da;
    }

    update[0](ours);
    update[1](orig);
    const unsigned* ha = reinterpret_cast<const unsigned*>(ours + 0xb8);
    const unsigned* hb = reinterpret_cast<const unsigned*>(orig + 0xb8);
    CheckValue(ha[0], hb[0], name);
    CheckValue(ha[1], hb[1], name);
    const unsigned* oa = *reinterpret_cast<unsigned**>(ours + 0xc0);
    const unsigned* ob = *reinterpret_cast<unsigned**>(orig + 0xc0);
    for (unsigned i = 0; i < ha[0] && i < hb[0]; ++i) CheckValue(oa[i], ob[i], name);
    KsDelete(*reinterpret_cast<void**>(ours + 0xc0));
    KsDelete(*reinterpret_cast<void**>(orig + 0xc0));
}

void TestUpdateArrays(const Api api[2], bool awkward)
{
    void(__cdecl* no[2])(void*) = {api[0].updateNoTexture, api[1].updateNoTexture};
    void(__cdecl* one[2])(void*) = {api[0].updateOneTexture, api[1].updateOneTexture};
    void(__cdecl* bump[2])(void*) = {api[0].updateBump, api[1].updateBump};
    for (unsigned pre : {0u, 10u}) {
        TestUpdate(api, "UpdateTextureArray NoTexture", no, 2, pre, awkward);
        TestUpdate(api, "UpdateTextureArray OneTexture", one, 3, pre, awkward);
        TestUpdate(api, "UpdateTextureArray Bump", bump, 4, pre, awkward);
    }
}

void TestIsSupported(const Api api[2], bool awkward)
{
    FakeDisplay display;
    void* vtable[0x20 / 4 + 1] = {};
    vtable[0x1c / 4] = reinterpret_cast<void*>(&CapabilityHook);
    display.vtable = vtable;

    using Fn = bool(__cdecl*)(void*, void*, PStr*);
    const Fn functions[2][4] = {{api[0].supportedNoTexture, api[0].supportedOneTexture, api[0].supportedBump,
                                  api[0].supportedBumpSpec},
                                 {api[1].supportedNoTexture, api[1].supportedOneTexture, api[1].supportedBump,
                                  api[1].supportedBumpSpec}};
    static const char* const names[4] = {"IsSupported NoTexture", "IsSupported OneTexture", "IsSupported Bump",
                                         "IsSupported BumpSpec"};

    capabilityCode[0] = 0x15;
    capabilityCode[1] = 0x11;
    capabilityCode[2] = 0x1b;
    for (int round = 0; round < 5; ++round) {
        for (int i = 0; i < 3; ++i) {
            capabilityResult[i] = awkward ? (Pick(0, 1) != 0) : (round >= i);
        }
        for (int f = 0; f < 4; ++f) {
            PStr a{nullptr}, b{nullptr};
            const bool ra = functions[0][f](nullptr, &display, &a);
            const bool rb = functions[1][f](nullptr, &display, &b);
            CheckValue(ra, rb, names[f]);
            CheckValue(reinterpret_cast<uintptr_t>(a.node), reinterpret_cast<uintptr_t>(b.node), names[f]);
            if (a.node && b.node) {
                const PNode* const na = static_cast<const PNode*>(a.node);
                const PNode* const nb = static_cast<const PNode*>(b.node);
                CheckBytes(na->text, nb->text, na->length + 1, names[f]);
            }
        }
    }

    // An out string that already holds a node: the original destroys it (one of two references) and installs the
    // fallback. Compare the surviving node's reference count and the text.
    if (nodeCreate && nodeDestroy && cacheLock && cacheUnlock && cacheSlot && *cacheSlot) {
        for (int f = 0; f < 4; ++f) {
            PNode* const preA = nodeCreate(*cacheSlot, reinterpret_cast<const unsigned char*>("u5b-prefix-a"));
            PNode* const preB = nodeCreate(*cacheSlot, reinterpret_cast<const unsigned char*>("u5b-prefix-b"));
            if (!preA || !preB) continue;
            AddCacheRef(preA);
            AddCacheRef(preB);
            PStr a{preA}, b{preB};
            capabilityResult[0] = capabilityResult[1] = capabilityResult[2] = 0;
            const bool ra = functions[0][f](nullptr, &display, &a);
            const bool rb = functions[1][f](nullptr, &display, &b);
            CheckValue(ra, rb, names[f]);
            CheckValue(reinterpret_cast<uintptr_t>(a.node), reinterpret_cast<uintptr_t>(b.node), names[f]);
            CheckValue(static_cast<unsigned>(preA->refs), static_cast<unsigned>(preB->refs), names[f]);
            nodeDestroy(*cacheSlot, preA);
            nodeDestroy(*cacheSlot, preB);
        }
    }
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u5b <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 1000;
    const std::string dir = argv[1];
    SetDllDirectoryA(dir.c_str());
    LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str());
    mod[0] = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    mod[1] = GetModuleHandleA("Render3DCore_7c_orig.dll");
    HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load the modules from %s (%lu)\n", dir.c_str(), GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    ksNew = reinterpret_cast<NewFn>(GetProcAddress(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    ksDelete = reinterpret_cast<DeleteFn>(GetProcAddress(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    cacheSlot = reinterpret_cast<void**>(GetProcAddress(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    if (cacheSlot) {
        nodeCreate = reinterpret_cast<NodeCreateFn>(GetProcAddress(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
        nodeDestroy = reinterpret_cast<NodeDestroyFn>(GetProcAddress(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z"));
        cacheLock = reinterpret_cast<CacheLockFn>(GetProcAddress(ks, "?Lock@PStringCache@Jet@@QAAXXZ"));
        cacheUnlock = reinterpret_cast<CacheUnlockFn>(GetProcAddress(ks, "?Unlock@PStringCache@Jet@@QAAXXZ"));
    }

    Api api[2] = {Load(0), Load(1)};
    for (int round = 0; round < rounds; ++round) {
        const bool awkward = (round % 7) == 0;
        TestMPLightCtor(api, awkward);
        TestMPLightDtor(api, awkward);
        TestLightRender(api, awkward);
        TestAddScissor(api, awkward);
        for (int which = 0; which <= 6; ++which) TestPropertyAppenders(api, awkward, which);
        TestChunkBase(api, awkward);
        TestModes(api, awkward);
        TestTypes(api);
        TestTextures(api, awkward);
        TestAlpha(api, awkward);
        TestActivate(api, awkward);
        TestUpdateArrays(api, awkward);
        TestIsSupported(api, awkward);
    }

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
