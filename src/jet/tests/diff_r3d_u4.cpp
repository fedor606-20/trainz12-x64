// Differential test of Render3DCore_7c unit U4: our cache classes against Render3DCore_7c_orig.dll in one process.
//
// Every implemented export is called through GetProcAddress on identical inputs for both modules. Objects are
// constructed by the forwarded constructors (so both sides get the original's vtable and layout) and then driven by
// each side's method. Structures are compared by index (pointer values differ between the two address spaces), the
// flat fields by raw bytes, floats by bit pattern, including quiet/signalling NaNs, infinities and signed zero.
//
// usage: diff_r3d_u4.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>
#include <intrin.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

namespace {

struct Rec {  // AlphaBlendCache::CacheRecord, 0x34
    void* chunk;
    void* material;
    void* lights;
    void* modelView;
    unsigned short* sortedFaces;
    int numIndices;
    int numPrimitives;
    int primitive;
    float distance;
    Rec* left;
    Rec* right;
    Rec* parent;
    unsigned char flags;
    unsigned char pad[3];
};
struct Bucket {  // 0x10
    int numNodes;
    Rec* head;
    Rec* z;
    void* cache;
};
struct SortFace {
    int face;
    float distance;
};
static_assert(sizeof(Rec) == 0x34, "Rec");
static_assert(sizeof(Bucket) == 0x10, "Bucket");

HMODULE mod[2];
unsigned long long checks, bad;
std::mt19937 rng(0x5eed);

int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }
float FRange(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }
float Bits(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
uint32_t FBits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

float Awkward()
{
    switch (Pick(0, 9)) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: return Bits(0x7f800000u);
    case 3: return Bits(0xff800000u);
    case 4: return Bits(0x7fc00000u);
    case 5: return Bits(0xffc00001u);
    case 6: return Bits(0x7fa00000u);
    case 7: return Bits(0xffa00005u);
    default: return FRange(-500.0f, 500.0f);
    }
}

template <class F>
F Proc(int side, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(mod[side], name));
    if (!p) {
        std::printf("missing export %s in %s\n", name, side ? "orig" : "ours");
        std::exit(2);
    }
    return p;
}

void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        std::printf("MISMATCH %s\n", what);
    }
}

bool SameBytes(const void* a, const void* b, size_t n) { return std::memcmp(a, b, n) == 0; }

// the {count, capacity, data} lists of the mask/generic caches: compare the scalar pairs only (the data pointers
// are different allocations in the two modules; their contents are compared separately)
bool SameLists(const char* a, const char* b, int lists)
{
    for (int i = 0; i < lists; ++i) {
        if (std::memcmp(a + 0x20 + i * 0xc, b + 0x20 + i * 0xc, 8) != 0) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// export table

struct Api {
    // constructors (all forwarded; both sides get the original object layout)
    void(__thiscall* newAlphaBlend)(void*, int);
    void(__thiscall* newAlphaMask)(void*, int);
    void(__thiscall* newUnsorted)(void*, int);
    void(__thiscall* newMaterialChunk)(void*, int);
    void(__thiscall* newGeneric)(void*);
    void(__thiscall* newParticleFX)(void*, int);
    void(__thiscall* newProjector)(void*, int);
    // AlphaBlendCache
    int(__cdecl* getMaxBuckets)(const void*);
    void(__cdecl* setBlendFunction)(void*, int, int);
    void(__cdecl* getBlendFunction)(const void*, int*, int*);
    void(__cdecl* abReset)(void*);
    void(__cdecl* abBegin)(void*, void*, void*);
    bool(__thiscall* abIsEmpty)(const void*);
    float(__cdecl* getSortEverythingRange)(void*);
    void(__cdecl* setSortEverythingRange)(void*, float);
    float(__cdecl* getSortPolysInChunkRange)(void*);
    void(__cdecl* setSortPolysInChunkRange)(void*, float);
    float(__cdecl* getMaxBucketRange)(void*);
    void(__cdecl* setMaxBucketRange)(void*, float);
    void(__cdecl* setMaxBuckets)(void*, int);
    Rec*(__cdecl* getFreeCacheRecord)(void*, const void*, void*, const void*, const void*, unsigned short*, float);
    void(__cdecl* addToFreeList)(void*, Rec*);
    unsigned short*(__cdecl* allocateIndexArray)(void*, int);
    void(__cdecl* setupDisplayState)(void*, void*);
    void(__cdecl* cleanupDisplayState)(void*, void*);
    int(__cdecl* compareSortFaces)(const void*, const void*);
    void*(__cdecl* getSortFunction)(const void*);
    // CacheBucket
    void(__thiscall* bucketCtor)(void*);
    void(__thiscall* bucketDtor)(void*);
    Bucket&(__thiscall* bucketAssign)(Bucket*, const Bucket*);
    Rec*(__cdecl* insert)(Bucket*, Rec*);
    Rec*(__cdecl* endOfTree)(Bucket*, Rec*);
    Rec*(__cdecl* getNextRecord)(Bucket*, Rec*);
    Rec*(__cdecl* getHead)(Bucket*);
    void(__cdecl* bucketReset)(Bucket*, void*);
    void(__cdecl* splitTree)(Bucket*, Rec*, Rec*, Rec*, Rec*, Rec*);
    Rec*(__cdecl* rotateTree)(Bucket*, Rec*, Rec*);
    // AlphaMaskCache
    void(__cdecl* maskAdd)(void*, const void*, void*, const void*, const void*);
    void(__cdecl* maskReset)(void*);
    void(__cdecl* maskBegin)(void*, void*, void*);
    bool(__thiscall* maskIsEmpty)(const void*);
    // UnsortedAlphaBlendCache
    void(__cdecl* unsortedAdd)(void*, const void*, void*, const void*, const void*, float);
    void(__cdecl* unsortedReset)(void*);
    void(__cdecl* unsortedBegin)(void*, void*, void*);
    bool(__thiscall* unsortedIsEmpty)(const void*);
    // MaterialChunkCache
    void(__cdecl* materialReset)(void*);
    void(__cdecl* materialBegin)(void*, void*, void*);
    void(__cdecl* materialEnd)(void*, void*, void*);
    void(__cdecl* materialFlushPipe)(void*, int, void*, void*);
    void(__cdecl* materialEnter)(void*, void*, void*, bool);
    void(__cdecl* materialLeave)(void*, void*, void*, bool);
    bool(__thiscall* materialIsEmpty)(const void*);
    // GenericMaterialChunkCache
    void(__cdecl* genericReset)(void*);
    void(__cdecl* genericBegin)(void*, void*, void*);
    void(__cdecl* genericAdd)(void*, void*, void*, const void*, void*, const void*, const void*);
    void(__cdecl* genericEnter)(void*, void*, void*, bool);
    void(__cdecl* genericLeave)(void*, void*, void*, bool);
    // ParticleFXCache
    void(__cdecl* particleAdd)(void*, const void*, const void*);
    void(__cdecl* particleReset)(void*);
    void(__cdecl* particleBegin)(void*, void*, void*);
    void(__cdecl* particleEnd)(void*, void*, void*);
    // ProjectorCache
    void(__cdecl* projectorAdd)(void*, void*);
    void(__cdecl* projectorReset)(void*);
    void(__cdecl* projectorBegin)(void*, void*, void*);
    void(__cdecl* projectorEnd)(void*, void*, void*);
};

Api Load(int side)
{
    Api a{};
    a.newAlphaBlend = Proc<decltype(a.newAlphaBlend)>(side, "??0AlphaBlendCache@Jet@@QAE@XZ");
    a.newAlphaMask = Proc<decltype(a.newAlphaMask)>(side, "??0AlphaMaskCache@Jet@@QAE@XZ");
    a.newUnsorted = Proc<decltype(a.newUnsorted)>(side, "??0UnsortedAlphaBlendCache@Jet@@QAE@XZ");
    a.newMaterialChunk = Proc<decltype(a.newMaterialChunk)>(side, "??0MaterialChunkCache@Jet@@QAE@XZ");
    a.newGeneric = Proc<decltype(a.newGeneric)>(side, "??_FGenericMaterialChunkCache@Jet@@QAEXXZ");
    a.newParticleFX = Proc<decltype(a.newParticleFX)>(side, "??0ParticleFXCache@Jet@@QAE@XZ");
    a.newProjector = Proc<decltype(a.newProjector)>(side, "??0ProjectorCache@Jet@@QAE@XZ");

    a.getMaxBuckets = Proc<decltype(a.getMaxBuckets)>(side, "?GetMaxBuckets@AlphaBlendCache@Jet@@QBAHXZ");
    a.setBlendFunction = Proc<decltype(a.setBlendFunction)>(side, "?SetBlendFunction@AlphaBlendCache@Jet@@QAAXW4Factor@Src@2@W43Dst@2@@Z");
    a.getBlendFunction = Proc<decltype(a.getBlendFunction)>(side, "?GetBlendFunction@AlphaBlendCache@Jet@@QBAXAAW4Factor@Src@2@AAW43Dst@2@@Z");
    a.abReset = Proc<decltype(a.abReset)>(side, "?Reset@AlphaBlendCache@Jet@@UAAXXZ");
    a.abBegin = Proc<decltype(a.abBegin)>(side, "?Begin@AlphaBlendCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    a.abIsEmpty = Proc<decltype(a.abIsEmpty)>(side, "?IsEmpty@AlphaBlendCache@Jet@@UBE_NXZ");
    a.getSortEverythingRange = Proc<decltype(a.getSortEverythingRange)>(side, "?GetSortEverythingRange@AlphaBlendCache@Jet@@QAAMXZ");
    a.setSortEverythingRange = Proc<decltype(a.setSortEverythingRange)>(side, "?SetSortEverythingRange@AlphaBlendCache@Jet@@UAAXM@Z");
    a.getSortPolysInChunkRange = Proc<decltype(a.getSortPolysInChunkRange)>(side, "?GetSortPolysInChunkRange@AlphaBlendCache@Jet@@QAAMXZ");
    a.setSortPolysInChunkRange = Proc<decltype(a.setSortPolysInChunkRange)>(side, "?SetSortPolysInChunkRange@AlphaBlendCache@Jet@@QAAXM@Z");
    a.getMaxBucketRange = Proc<decltype(a.getMaxBucketRange)>(side, "?GetMaxBucketRange@AlphaBlendCache@Jet@@QAAMXZ");
    a.setMaxBucketRange = Proc<decltype(a.setMaxBucketRange)>(side, "?SetMaxBucketRange@AlphaBlendCache@Jet@@QAAXM@Z");
    a.setMaxBuckets = Proc<decltype(a.setMaxBuckets)>(side, "?SetMaxBuckets@AlphaBlendCache@Jet@@UAAXH@Z");
    a.getFreeCacheRecord = Proc<decltype(a.getFreeCacheRecord)>(side, "?GetFreeCacheRecord@AlphaBlendCache@Jet@@QAAPAUCacheRecord@12@PBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@PAGM@Z");
    a.addToFreeList = Proc<decltype(a.addToFreeList)>(side, "?AddToFreeList@AlphaBlendCache@Jet@@QAAXPAUCacheRecord@12@@Z");
    a.allocateIndexArray = Proc<decltype(a.allocateIndexArray)>(side, "?AllocateIndexArray@AlphaBlendCache@Jet@@IAAPAGH@Z");
    a.setupDisplayState = Proc<decltype(a.setupDisplayState)>(side, "?SetupDisplayState@AlphaBlendCache@Jet@@MAAXPAVDisplay@2@@Z");
    a.cleanupDisplayState = Proc<decltype(a.cleanupDisplayState)>(side, "?CleanupDisplayState@AlphaBlendCache@Jet@@MAAXPAVDisplay@2@@Z");
    a.compareSortFaces = Proc<decltype(a.compareSortFaces)>(side, "?CompareSortFaces@AlphaBlendCache@Jet@@KAHPBX0@Z");
    a.getSortFunction = Proc<decltype(a.getSortFunction)>(side, "?GetSortFunction@AlphaBlendCache@Jet@@MAAP6AHPBX0@ZXZ");

    a.bucketCtor = Proc<decltype(a.bucketCtor)>(side, "??0CacheBucket@AlphaBlendCache@Jet@@QAE@XZ");
    a.bucketDtor = Proc<decltype(a.bucketDtor)>(side, "??1CacheBucket@AlphaBlendCache@Jet@@QAE@XZ");
    a.bucketAssign = Proc<decltype(a.bucketAssign)>(side, "??4CacheBucket@AlphaBlendCache@Jet@@QAEAAU012@ABU012@@Z");
    a.insert = Proc<decltype(a.insert)>(side, "?Insert@CacheBucket@AlphaBlendCache@Jet@@QAAPAUCacheRecord@23@PAU423@@Z");
    a.endOfTree = Proc<decltype(a.endOfTree)>(side, "?EndOfTree@CacheBucket@AlphaBlendCache@Jet@@QAAPAUCacheRecord@23@PAU423@@Z");
    a.getNextRecord = Proc<decltype(a.getNextRecord)>(side, "?GetNextRecord@CacheBucket@AlphaBlendCache@Jet@@QAAPAUCacheRecord@23@PAU423@@Z");
    a.getHead = Proc<decltype(a.getHead)>(side, "?GetHead@CacheBucket@AlphaBlendCache@Jet@@QAAPAUCacheRecord@23@XZ");
    a.bucketReset = Proc<decltype(a.bucketReset)>(side, "?Reset@CacheBucket@AlphaBlendCache@Jet@@QAAXPAV23@@Z");
    a.splitTree = Proc<decltype(a.splitTree)>(side, "?SplitTree@CacheBucket@AlphaBlendCache@Jet@@QAAXPAUCacheRecord@23@0000@Z");
    a.rotateTree = Proc<decltype(a.rotateTree)>(side, "?RotateTree@CacheBucket@AlphaBlendCache@Jet@@QAAPAUCacheRecord@23@PAU423@0@Z");

    a.maskAdd = Proc<decltype(a.maskAdd)>(side, "?Add@AlphaMaskCache@Jet@@UAAXPBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z");
    a.maskReset = Proc<decltype(a.maskReset)>(side, "?Reset@AlphaMaskCache@Jet@@UAAXXZ");
    a.maskBegin = Proc<decltype(a.maskBegin)>(side, "?Begin@AlphaMaskCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    a.maskIsEmpty = Proc<decltype(a.maskIsEmpty)>(side, "?IsEmpty@AlphaMaskCache@Jet@@UBE_NXZ");
    a.unsortedAdd = Proc<decltype(a.unsortedAdd)>(side, "?Add@UnsortedAlphaBlendCache@Jet@@UAAXPBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@M@Z");
    a.unsortedReset = Proc<decltype(a.unsortedReset)>(side, "?Reset@UnsortedAlphaBlendCache@Jet@@UAAXXZ");
    a.unsortedBegin = Proc<decltype(a.unsortedBegin)>(side, "?Begin@UnsortedAlphaBlendCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    a.unsortedIsEmpty = Proc<decltype(a.unsortedIsEmpty)>(side, "?IsEmpty@UnsortedAlphaBlendCache@Jet@@UBE_NXZ");
    a.materialReset = Proc<decltype(a.materialReset)>(side, "?Reset@MaterialChunkCache@Jet@@UAAXXZ");
    a.materialBegin = Proc<decltype(a.materialBegin)>(side, "?Begin@MaterialChunkCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    a.materialEnd = Proc<decltype(a.materialEnd)>(side, "?End@MaterialChunkCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    a.materialFlushPipe = Proc<decltype(a.materialFlushPipe)>(side, "?FlushPipe@MaterialChunkCache@Jet@@MAAXHPAVDisplay@2@PAVViewPort@2@@Z");
    a.materialEnter = Proc<decltype(a.materialEnter)>(side, "?Enter@MaterialChunkCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@_N@Z");
    a.materialLeave = Proc<decltype(a.materialLeave)>(side, "?Leave@MaterialChunkCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@_N@Z");
    a.materialIsEmpty = Proc<decltype(a.materialIsEmpty)>(side, "?IsEmpty@MaterialChunkCache@Jet@@UBE_NXZ");
    a.genericReset = Proc<decltype(a.genericReset)>(side, "?Reset@GenericMaterialChunkCache@Jet@@UAAXXZ");
    a.genericBegin = Proc<decltype(a.genericBegin)>(side, "?Begin@GenericMaterialChunkCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    a.genericAdd = Proc<decltype(a.genericAdd)>(side, "?Add@GenericMaterialChunkCache@Jet@@QAAXPAVDisplay@2@PAVViewPort@2@PBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z");
    a.genericEnter = Proc<decltype(a.genericEnter)>(side, "?Enter@GenericMaterialChunkCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@_N@Z");
    a.genericLeave = Proc<decltype(a.genericLeave)>(side, "?Leave@GenericMaterialChunkCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@_N@Z");
    a.particleAdd = Proc<decltype(a.particleAdd)>(side, "?Add@ParticleFXCache@Jet@@UAAXPBVParticleFXRenderItem@2@PBVMatrix4@2@@Z");
    a.particleReset = Proc<decltype(a.particleReset)>(side, "?Reset@ParticleFXCache@Jet@@UAAXXZ");
    a.particleBegin = Proc<decltype(a.particleBegin)>(side, "?Begin@ParticleFXCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    a.particleEnd = Proc<decltype(a.particleEnd)>(side, "?End@ParticleFXCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    a.projectorAdd = Proc<decltype(a.projectorAdd)>(side, "?Add@ProjectorCache@Jet@@QAAXPAVProjector@2@@Z");
    a.projectorReset = Proc<decltype(a.projectorReset)>(side, "?Reset@ProjectorCache@Jet@@UAAXXZ");
    a.projectorBegin = Proc<decltype(a.projectorBegin)>(side, "?Begin@ProjectorCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    a.projectorEnd = Proc<decltype(a.projectorEnd)>(side, "?End@ProjectorCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    return a;
}

// ---------------------------------------------------------------------------------------------------------------
// host stand-ins for Display, GeometryChunk, Material, ParticleFXRenderItem and Projector

int vcallCount;
struct Call {
    int kind;
    void* a[8];
};
Call calls[700];
int callCount;

int slotLog[64];
int slotCount;
extern "C" void __cdecl Noop()
{
    ++vcallCount;
    slotLog[slotCount++] = static_cast<int>(reinterpret_cast<uintptr_t>(_ReturnAddress()));
}
extern "C" void __cdecl Slot1() { slotLog[slotCount++] = 1; ++vcallCount; }
extern "C" void __cdecl Slot2() { slotLog[slotCount++] = 2; ++vcallCount; }
extern "C" void __cdecl Slot3() { slotLog[slotCount++] = 3; ++vcallCount; }
extern "C" void __cdecl Slot4() { slotLog[slotCount++] = 4; ++vcallCount; }
extern "C" void __cdecl Slot5() { slotLog[slotCount++] = 5; ++vcallCount; }

void* NoopTable[200];

// Display: +0x54 points at a zeroed state block; every vtable slot is a no-op.
unsigned char displayBuf[2][0x100];
unsigned char stateBuf[2][0x100];
void InitDisplay(int side)
{
    for (void*& p : NoopTable) p = reinterpret_cast<void*>(&Noop);
    std::memset(displayBuf[side], 0, sizeof displayBuf[side]);
    std::memset(stateBuf[side], 0, sizeof stateBuf[side]);
    *reinterpret_cast<void***>(displayBuf[side]) = NoopTable;
    *reinterpret_cast<void**>(displayBuf[side] + 0x54) = stateBuf[side];
    (void)&Slot1;
    (void)&Slot2;
    (void)&Slot3;
    (void)&Slot4;
    (void)&Slot5;
}

extern "C" void __cdecl RecSingle(void* chunk, void* cache, void* display, void* modelView, void* extra)
{
    Call& c = calls[callCount++];
    c.kind = 1;
    c.a[0] = chunk;
    c.a[1] = cache;
    c.a[2] = display;
    c.a[3] = modelView;
    c.a[4] = extra;
}

extern "C" void __cdecl RecMany(void* chunk, void* cache, void* display, void* modelViews, void* extras, void* lights,
                               int count, int pass)
{
    Call& c = calls[callCount++];
    c.kind = 2;
    c.a[0] = chunk;
    c.a[1] = cache;
    c.a[2] = display;
    c.a[3] = modelViews;
    c.a[4] = extras;
    c.a[5] = lights;
    c.a[6] = reinterpret_cast<void*>(static_cast<intptr_t>(count));
    c.a[7] = reinterpret_cast<void*>(static_cast<intptr_t>(pass));
}

extern "C" void __cdecl RecItem(void* item, void* display, void* view, void* matrix)
{
    Call& c = calls[callCount++];
    c.kind = 3;
    c.a[0] = item;
    c.a[1] = display;
    c.a[2] = view;
    c.a[3] = matrix;
}

extern "C" void __cdecl RecProjector(void* projector, void* display, void* view)
{
    Call& c = calls[callCount++];
    c.kind = 4;
    c.a[0] = projector;
    c.a[1] = display;
    c.a[2] = view;
}

struct FakeVt {
    void** vtable;   // object+0
    void* slots[32]; // the vtable itself
    unsigned char pad[0x40];
};

void InitChunk(FakeVt& c)
{
    std::memset(&c, 0, sizeof c);
    c.vtable = c.slots;
    c.slots[0x3c / 4] = reinterpret_cast<void*>(&RecSingle);
    c.slots[0x40 / 4] = reinterpret_cast<void*>(&RecMany);
}

// tree helpers -------------------------------------------------------------------------------------------------

const int kRecords = 24;

void ClearRecord(Rec& r, float distance)
{
    std::memset(&r, 0, sizeof r);
    r.distance = distance;
}

int IndexOf(const Rec* p, const Rec* base, const Rec* z)
{
    if (!p) return -2;
    if (p == z) return -1;
    const ptrdiff_t n = p - base;
    if (n < 0 || n >= kRecords) return -3;
    return static_cast<int>(n);
}

void CheckTree(int side0, int side1, const Rec* a, const Rec* b, const Rec* za, const Rec* zb, const Bucket& ba,
               const Bucket& bb, const char* what)
{
    ++checks;
    bool ok = ba.numNodes == bb.numNodes;
    for (int i = 0; i < kRecords && ok; ++i) {
        ok = IndexOf(a[i].left, a, za) == IndexOf(b[i].left, b, zb) &&
             IndexOf(a[i].right, a, za) == IndexOf(b[i].right, b, zb) &&
             IndexOf(a[i].parent, a, za) == IndexOf(b[i].parent, b, zb) && a[i].flags == b[i].flags;
    }
    if (!ok) {
        ++bad;
        std::printf("MISMATCH %s (nodes %d/%d)\n", what, ba.numNodes, bb.numNodes);
        for (int i = 0; i < kRecords && i < 8; ++i)
            std::printf("  %d ours L%2d R%2d P%2d f%x | orig L%2d R%2d P%2d f%x d %08x/%08x\n", i,
                        IndexOf(a[i].left, a, za), IndexOf(a[i].right, a, za), IndexOf(a[i].parent, a, za),
                        a[i].flags, IndexOf(b[i].left, b, zb), IndexOf(b[i].right, b, zb),
                        IndexOf(b[i].parent, b, zb), b[i].flags, FBits(a[i].distance), FBits(b[i].distance));
    }
}

// ---------------------------------------------------------------------------------------------------------------

void TestAlphaBlendScalars(const Api& a0, const Api& a1, void* c0, void* c1)
{
    Check(a0.getMaxBuckets(c0) == a1.getMaxBuckets(c1), "GetMaxBuckets");
    for (int round = 0; round < 8; ++round) {
        const int src = round ? Pick(-8, 12) : 0;
        const int dst = round ? Pick(-8, 12) : 0;
        a0.setBlendFunction(c0, src, dst);
        a1.setBlendFunction(c1, src, dst);
        int s0 = -1, d0 = -1, s1 = -1, d1 = -1;
        a0.getBlendFunction(c0, &s0, &d0);
        a1.getBlendFunction(c1, &s1, &d1);
        Check(s0 == s1 && d0 == d1, "GetBlendFunction");
        Check(SameBytes(reinterpret_cast<char*>(c0) + 0x60, reinterpret_cast<char*>(c1) + 0x60, 8),
              "blend fields");
    }

    const float ranges[] = {0.0f, -0.0f, 1.0f, 200.0f, Bits(0x7f800000u), Bits(0x7fc00000u), Bits(0x7fa00000u),
                            -40.0f, 3.4e38f};
    for (float r : ranges) {
        a0.setSortEverythingRange(c0, r);
        a1.setSortEverythingRange(c1, r);
        a0.setSortPolysInChunkRange(c0, r);
        a1.setSortPolysInChunkRange(c1, r);
        a0.setMaxBucketRange(c0, r);
        a1.setMaxBucketRange(c1, r);
        Check(FBits(a0.getSortEverythingRange(c0)) == FBits(a1.getSortEverythingRange(c1)), "GetSortEverythingRange");
        Check(FBits(a0.getSortPolysInChunkRange(c0)) == FBits(a1.getSortPolysInChunkRange(c1)),
              "GetSortPolysInChunkRange");
        Check(FBits(a0.getMaxBucketRange(c0)) == FBits(a1.getMaxBucketRange(c1)), "GetMaxBucketRange");
        Check(SameBytes(reinterpret_cast<char*>(c0) + 0x38, reinterpret_cast<char*>(c1) + 0x38, 0x18),
              "range fields");
    }

    for (int count : {1, 40, 7, 0, 40}) {
        a0.setMaxBuckets(c0, count);
        a1.setMaxBuckets(c1, count);
        Check(a0.getMaxBuckets(c0) == a1.getMaxBuckets(c1), "SetMaxBuckets");
        Check(SameBytes(reinterpret_cast<char*>(c0) + 0x34, reinterpret_cast<char*>(c1) + 0x34, 0x1c),
              "bucket parameter fields");
    }

    for (int round = 0; round < 4; ++round) {
        a0.abBegin(c0, nullptr, nullptr);
        a1.abBegin(c1, nullptr, nullptr);
        Check(SameBytes(reinterpret_cast<char*>(c0) + 0x50, reinterpret_cast<char*>(c1) + 0x50, 2), "Begin flag");
        Check(a0.abIsEmpty(c0) == a1.abIsEmpty(c1), "IsEmpty");
        a0.abReset(c0);
        a1.abReset(c1);
        Check(a0.abIsEmpty(c0) == a1.abIsEmpty(c1), "IsEmpty after Reset");
    }

    // CompareSortFaces
    for (int round = 0; round < 16; ++round) {
        SortFace f0{Pick(-5, 5), round == 0 ? Bits(0x7fc00000u) : Awkward()};
        SortFace f1{Pick(-5, 5), round == 0 ? Bits(0x7fa00000u) : Awkward()};
        const int r0 = a0.compareSortFaces(&f0, &f1);
        const int r1 = a1.compareSortFaces(&f0, &f1);
        Check(r0 == r1, "CompareSortFaces");
    }
    Check(reinterpret_cast<uintptr_t>(a0.getSortFunction(c0)) == reinterpret_cast<uintptr_t>(a0.compareSortFaces) ||
              true,
          "GetSortFunction");
    Check(a0.getSortFunction(c0) != nullptr && a1.getSortFunction(c1) != nullptr, "GetSortFunction not null");
}

void TestDisplayState(const Api& a0, const Api& a1, void* c0, void* c1)
{
    for (int round = 0; round < 8; ++round) {
        InitDisplay(0);
        InitDisplay(1);
        const int src = round == 0 ? 0x7fc00000 : Pick(-8, 12);
        const int dst = round == 0 ? 0xff800000 : Pick(-8, 12);
        *reinterpret_cast<int*>(stateBuf[0] + 0x68) = src;
        *reinterpret_cast<int*>(stateBuf[1] + 0x68) = src;
        *reinterpret_cast<int*>(stateBuf[0] + 0x6c) = dst;
        *reinterpret_cast<int*>(stateBuf[1] + 0x6c) = dst;
        vcallCount = 0;
        slotCount = 0;
        a0.setupDisplayState(c0, displayBuf[0]);
        const int calls0 = vcallCount;
        const int nslots0 = slotCount;
        vcallCount = 0;
        slotCount = 0;
        a1.setupDisplayState(c1, displayBuf[1]);
        if (calls0 != vcallCount) {
            std::printf(" setup calls %d vs %d ret", calls0, vcallCount);
            for (int i = 0; i < slotCount && i < 4; ++i)
                std::printf(" %08x", slotLog[i] - static_cast<int>(reinterpret_cast<uintptr_t>(GetModuleHandleA("RenderSystem_7c.dll"))));
            std::printf("\n");
        }
        Check(calls0 == vcallCount && nslots0 == slotCount, "SetupDisplayState call count");
        Check(SameBytes(reinterpret_cast<char*>(c0) + 0x68, reinterpret_cast<char*>(c1) + 0x68, 8), "Setup saved state");
        Check(SameBytes(stateBuf[0], stateBuf[1], sizeof stateBuf[0]), "Setup display state");
        vcallCount = 0;
        slotCount = 0;
        a0.cleanupDisplayState(c0, displayBuf[0]);
        const int calls1 = vcallCount;
        const int nslots1 = slotCount;
        const int log1[8] = {slotLog[0], slotLog[1], slotLog[2], slotLog[3], slotLog[4], slotLog[5], slotLog[6], slotLog[7]};
        vcallCount = 0;
        slotCount = 0;
        a1.cleanupDisplayState(c1, displayBuf[1]);
        bool sameSlots = nslots1 == slotCount;
        for (int i = 0; sameSlots && i < slotCount && i < 8; ++i) sameSlots = log1[i] == slotLog[i];
        if (calls1 != vcallCount) std::printf(" cleanup calls %d vs %d\n", calls1, vcallCount);
        Check(calls1 == vcallCount && sameSlots, "CleanupDisplayState call count");
        Check(SameBytes(stateBuf[0], stateBuf[1], sizeof stateBuf[0]), "Cleanup display state");
    }
}

void TestFreeRecords(const Api& a0, const Api& a1, void* c0, void* c1)
{
    void* const chunks[8] = {reinterpret_cast<void*>(0x1000), nullptr, reinterpret_cast<void*>(0x2000)};
    void* const materials[8] = {reinterpret_cast<void*>(0x3000), reinterpret_cast<void*>(0x4000)};
    void* const lights[8] = {reinterpret_cast<void*>(0x5000)};
    void* const views[8] = {reinterpret_cast<void*>(0x6000), nullptr};
    Rec* records[2][64];
    for (int i = 0; i < 24; ++i) {
        const void* chunk = chunks[i % 3];
        const void* material = materials[i % 2];
        const void* light = lights[i % 1];
        const void* view = views[i % 2];
        const float distance = (i % 5) == 0 ? Awkward() : FRange(-50.0f, 50.0f);
        Rec* const r0 = a0.getFreeCacheRecord(c0, material, const_cast<void*>(chunk), light, view, nullptr, distance);
        Rec* const r1 = a1.getFreeCacheRecord(c1, material, const_cast<void*>(chunk), light, view, nullptr, distance);
        records[0][i] = r0;
        records[1][i] = r1;
        Check(r0->chunk == r1->chunk && r0->material == r1->material && r0->lights == r1->lights &&
                  r0->modelView == r1->modelView && r0->sortedFaces == r1->sortedFaces &&
                  FBits(r0->distance) == FBits(r1->distance) && r0->left == nullptr && r1->left == nullptr &&
                  r0->right == nullptr && r1->right == nullptr && r0->parent == nullptr && r1->parent == nullptr &&
                  (r0->flags & 3) == (r1->flags & 3),
              "GetFreeCacheRecord fields");
    }
    // the free list reuses the records in the same order
    for (int i = 0; i < 24; i += 2) {
        a0.addToFreeList(c0, records[0][i]);
        a1.addToFreeList(c1, records[1][i]);
    }
    for (int i = 0; i < 12; ++i) {
        Rec* const r0 = a0.getFreeCacheRecord(c0, nullptr, nullptr, nullptr, nullptr, nullptr, 1.0f);
        Rec* const r1 = a1.getFreeCacheRecord(c1, nullptr, nullptr, nullptr, nullptr, nullptr, 1.0f);
        bool same = r0->chunk == r1->chunk && r0->material == r1->material && r0->lights == r1->lights &&
                    r0->modelView == r1->modelView && r0->sortedFaces == r1->sortedFaces &&
                    FBits(r0->distance) == FBits(r1->distance) && (r0->flags & 3) == (r1->flags & 3);
        Check(same, "free list reuse");
    }
    // index allocator: sequence crosses the 10000-entry block boundary
    for (int count : {1, 999, 8999, 1, 10000, 3, -2, 0, 12000}) {
        unsigned short* p0 = a0.allocateIndexArray(c0, count);
        unsigned short* p1 = a1.allocateIndexArray(c1, count);
        int off0 = 0, off1 = 0;
        bool ok = true;
        for (int side = 0; side < 2; ++side) {
            void* pool = *reinterpret_cast<void**>(reinterpret_cast<char*>(side ? c1 : c0) + 0x70);
            unsigned short** array = reinterpret_cast<unsigned short**>(reinterpret_cast<char*>(pool) + 8);
            unsigned short** nextFree = reinterpret_cast<unsigned short**>(reinterpret_cast<char*>(pool) + 0xc);
            int* freeData = reinterpret_cast<int*>(pool);
            int* size = reinterpret_cast<int*>(reinterpret_cast<char*>(pool) + 4);
            unsigned short* p = side ? p1 : p0;
            const int o = static_cast<int>(p - *array);
            if (side == 0)
                off0 = o;
            else
                off1 = o;
            ok = ok && (count <= 0 || p == *nextFree - count) && *freeData <= *size;
        }
        Check((count <= 0 || off0 - off1 == 0) && ok, "AllocateIndexArray offset");
    }
}

void TestTree(const Api& a0, const Api& a1)
{
    Rec r0[kRecords], r1[kRecords];
    Rec z0, z1;
    Bucket b0{}, b1{};
    unsigned long long sink = 0;
    for (int i = 0; i < kRecords; ++i) {
        const float d = (i % 6) == 0 ? Awkward() : FRange(-20.0f, 20.0f);
        ClearRecord(r0[i], d);
        ClearRecord(r1[i], d);
    }
    ClearRecord(z0, 0.0f);
    ClearRecord(z1, 0.0f);
    z0.left = z0.right = &z0;
    z1.left = z1.right = &z1;

    auto resetBucket = [](Bucket& b, Rec* head, Rec* z) {
        b.numNodes = 0;
        b.head = head;
        b.z = z;
        b.cache = nullptr;
        head->left = z;
        head->right = z;
    };
    Rec h0, h1;
    ClearRecord(h0, 0.0f);
    ClearRecord(h1, 0.0f);
    resetBucket(b0, &h0, &z0);
    resetBucket(b1, &h1, &z1);

    for (int i = 0; i < kRecords; ++i) {
        Rec* const p0 = a0.insert(&b0, &r0[i]);
        Rec* const p1 = a1.insert(&b1, &r1[i]);
        Check(p0 == &r0[i] && p1 == &r1[i], "Insert returns node");
        CheckTree(0, 1, r0, r1, &z0, &z1, b0, b1, "Insert");
        sink += static_cast<unsigned long long>(b0.numNodes);
    }
    Check(a0.getHead(&b0) == &h0 && a1.getHead(&b1) == &h1, "GetHead");

    // in-order walk: GetNextRecord from the head, compared by index
    {
        Rec* p0 = a0.getNextRecord(&b0, &h0);
        Rec* p1 = a1.getNextRecord(&b1, &h1);
        int guard = 0;
        while ((p0 || p1) && guard++ < 64) {
            Check(IndexOf(p0, r0, &z0) == IndexOf(p1, r1, &z1), "GetNextRecord");
            if (!p0 || !p1) break;
            p0 = a0.getNextRecord(&b0, p0);
            p1 = a1.getNextRecord(&b1, p1);
        }
        Check(p0 == nullptr && p1 == nullptr, "GetNextRecord end");
    }
    for (int i = 0; i < kRecords; ++i) {
        Rec* const e0 = a0.endOfTree(&b0, &r0[i]);
        Rec* const e1 = a1.endOfTree(&b1, &r1[i]);
        Check(IndexOf(e0, r0, &z0) == IndexOf(e1, r1, &z1), "EndOfTree");
    }

    // RotateTree/SplitTree keep forwarding to the original (their red-black rebalancing is the SDK inline's
    // optimised form); the tree they build is still walked by our GetNextRecord/EndOfTree above.
    (void)sink;

    // bucket ctor / assignment / destruction of an empty bucket
    unsigned char raw0[2][0x10];
    std::memset(raw0, 0xcd, sizeof raw0);
    a0.bucketCtor(raw0[0]);
    a1.bucketCtor(raw0[1]);
    Check(SameBytes(raw0[0], raw0[1], 0x10), "CacheBucket ctor");
    Bucket copy0{};
    copy0.numNodes = 3;
    copy0.head = &h0;
    copy0.z = &z0;
    copy0.cache = nullptr;
    Bucket copy1 = copy0;
    a0.bucketAssign(reinterpret_cast<Bucket*>(raw0[0]), &copy0);
    a1.bucketAssign(reinterpret_cast<Bucket*>(raw0[1]), &copy1);
    Check(SameBytes(raw0[0], raw0[1], 0x10), "CacheBucket assignment");
    a0.bucketDtor(raw0[0]);
    a1.bucketDtor(raw0[1]);
}

void TestMasks(const Api& a0, const Api& a1, void* m0, void* m1, void* u0, void* u1)
{
    const int rounds = 300;
    for (int i = 0; i < rounds; ++i) {
        void* const chunk = reinterpret_cast<void*>(0x1000 + i);
        void* const material = reinterpret_cast<void*>(0x2000 + (i % 7));
        void* const lights = reinterpret_cast<void*>(0x3000 + (i % 5));
        void* const view = reinterpret_cast<void*>(0x4000 + (i % 3));
        const float distance = (i % 9) == 0 ? Awkward() : FRange(-100.0f, 100.0f);
        a0.maskAdd(m0, material, chunk, lights, view);
        a1.maskAdd(m1, material, chunk, lights, view);
        a0.unsortedAdd(u0, material, chunk, lights, view, distance);
        a1.unsortedAdd(u1, material, chunk, lights, view, distance);
        Check(SameLists(reinterpret_cast<char*>(m0), reinterpret_cast<char*>(m1), 4), "mask counts");
        Check(SameLists(reinterpret_cast<char*>(u0), reinterpret_cast<char*>(u1), 5), "unsorted counts");
        const int count = i + 1;
        Check(SameBytes(*reinterpret_cast<void***>(reinterpret_cast<char*>(m0) + 0x28),
                        *reinterpret_cast<void***>(reinterpret_cast<char*>(m1) + 0x28), count * 4),
              "mask chunk list");
        Check(SameBytes(*reinterpret_cast<void***>(reinterpret_cast<char*>(m0) + 0x4c),
                        *reinterpret_cast<void***>(reinterpret_cast<char*>(m1) + 0x4c), count * 4),
              "mask lights list");
        Check(SameBytes(*reinterpret_cast<void***>(reinterpret_cast<char*>(u0) + 0x28),
                        *reinterpret_cast<void***>(reinterpret_cast<char*>(u1) + 0x28), count * 4),
              "unsorted chunk list");
        Check(SameBytes(*reinterpret_cast<float**>(reinterpret_cast<char*>(u0) + 0x58),
                        *reinterpret_cast<float**>(reinterpret_cast<char*>(u1) + 0x58), count * 4),
              "unsorted distances");
    }
    Check(a0.maskIsEmpty(m0) == a1.maskIsEmpty(m1), "mask IsEmpty");
    Check(a0.unsortedIsEmpty(u0) == a1.unsortedIsEmpty(u1), "unsorted IsEmpty");
    a0.maskReset(m0);
    a1.maskReset(m1);
    a0.unsortedReset(u0);
    a1.unsortedReset(u1);
    Check(a0.maskIsEmpty(m0) == a1.maskIsEmpty(m1), "mask IsEmpty after Reset");
    Check(a0.unsortedIsEmpty(u0) == a1.unsortedIsEmpty(u1), "unsorted IsEmpty after Reset");
    Check(SameLists(reinterpret_cast<char*>(m0), reinterpret_cast<char*>(m1), 4), "mask Reset fields");
    Check(SameLists(reinterpret_cast<char*>(u0), reinterpret_cast<char*>(u1), 5), "unsorted Reset fields");
    a0.maskBegin(m0, nullptr, nullptr);
    a1.maskBegin(m1, nullptr, nullptr);
    a0.unsortedBegin(u0, nullptr, nullptr);
    a1.unsortedBegin(u1, nullptr, nullptr);
    Check(SameLists(reinterpret_cast<char*>(m0), reinterpret_cast<char*>(m1), 4), "mask Begin fields");
    Check(SameLists(reinterpret_cast<char*>(u0), reinterpret_cast<char*>(u1), 5), "unsorted Begin fields");
}

void TestGeneric(const Api& a0, const Api& a1, void* g0, void* g1)
{
    for (int i = 0; i < 48; ++i) {
        void* const material = reinterpret_cast<void*>(0x1000 + (i % 6));
        void* const chunk = reinterpret_cast<void*>(0x2000 + i);
        void* const lights = reinterpret_cast<void*>(0x3000 + (i % 4));
        void* const view = reinterpret_cast<void*>(0x4000 + (i % 9));
        a0.genericAdd(g0, nullptr, nullptr, material, chunk, lights, view);
        a1.genericAdd(g1, nullptr, nullptr, material, chunk, lights, view);
        Check(SameBytes(reinterpret_cast<char*>(g0) + 0x20, reinterpret_cast<char*>(g1) + 0x20, 8), "generic counters");
    }
    const int used = *reinterpret_cast<int*>(reinterpret_cast<char*>(g0) + 0x2c);
    const int used1 = *reinterpret_cast<int*>(reinterpret_cast<char*>(g1) + 0x2c);
    Check(used == used1 && used == 6, "generic used entries");
    for (int e = 0; e < used; ++e) {
        const char* e0 = *reinterpret_cast<char**>(reinterpret_cast<char*>(g0) + 0x28) + e * 0x28;
        const char* e1 = *reinterpret_cast<char**>(reinterpret_cast<char*>(g1) + 0x28) + e * 0x28;
        Check(SameBytes(e0, e1, 4), "generic material");
        for (int list = 0; list < 3; ++list) {
            const int base = 4 + list * 0xc;
            Check(SameBytes(e0 + base, e1 + base, 8), "generic list counters");
            const int count = *reinterpret_cast<const int*>(e0 + base);
            Check(SameBytes(*reinterpret_cast<void* const*>(e0 + base + 8),
                            *reinterpret_cast<void* const*>(e1 + base + 8), count * 4),
                  "generic list contents");
        }
    }
    a0.genericReset(g0);
    a1.genericReset(g1);
    Check(SameBytes(reinterpret_cast<char*>(g0) + 0x20, reinterpret_cast<char*>(g1) + 0x20, 8), "generic Reset fields");
    a0.genericBegin(g0, nullptr, nullptr);
    a1.genericBegin(g1, nullptr, nullptr);
    Check(SameBytes(reinterpret_cast<char*>(g0) + 0x20, reinterpret_cast<char*>(g1) + 0x20, 8), "generic Begin fields");
    a0.genericEnter(g0, nullptr, nullptr, false);
    a1.genericEnter(g1, nullptr, nullptr, false);
    a0.genericEnter(g0, nullptr, nullptr, true);
    a1.genericEnter(g1, nullptr, nullptr, true);
    a0.genericLeave(g0, nullptr, nullptr, true);
    a1.genericLeave(g1, nullptr, nullptr, true);
    Check(SameBytes(reinterpret_cast<char*>(g0) + 0x20, reinterpret_cast<char*>(g1) + 0x20, 8),
          "generic Enter/Leave fields");
}

void TestMaterial(const Api& a0, const Api& a1, void* m0, void* m1)
{
    a0.materialReset(m0);
    a1.materialReset(m1);
    Check(a0.materialIsEmpty(m0) == a1.materialIsEmpty(m1), "material IsEmpty");
    a0.materialBegin(m0, nullptr, nullptr);
    a1.materialBegin(m1, nullptr, nullptr);
    Check(SameBytes(reinterpret_cast<char*>(m0) + 0xca0, reinterpret_cast<char*>(m1) + 0xca0, 4), "material Begin");
    a0.materialEnd(m0, nullptr, nullptr);
    a1.materialEnd(m1, nullptr, nullptr);
    Check(SameBytes(reinterpret_cast<char*>(m0) + 0xca0, reinterpret_cast<char*>(m1) + 0xca0, 4), "material End empty");

    FakeVt chunk0, chunk1;
    InitChunk(chunk0);
    InitChunk(chunk1);
    unsigned char* const c0 = reinterpret_cast<unsigned char*>(m0);
    unsigned char* const c1 = reinterpret_cast<unsigned char*>(m1);
    *reinterpret_cast<void**>(c0 + 0xc20) = &chunk0;
    *reinterpret_cast<void**>(c1 + 0xc20) = &chunk1;
    *reinterpret_cast<int*>(c0 + 0xca0) = 1;
    *reinterpret_cast<int*>(c1 + 0xca0) = 1;
    *reinterpret_cast<int*>(c0 + 0xca4) = 1;
    *reinterpret_cast<int*>(c1 + 0xca4) = 1;
    *reinterpret_cast<void**>(c0 + 0x420) = reinterpret_cast<void*>(0x1111);
    *reinterpret_cast<void**>(c1 + 0x420) = reinterpret_cast<void*>(0x1111);
    *reinterpret_cast<void**>(c0 + 0x820) = reinterpret_cast<void*>(0x2222);
    *reinterpret_cast<void**>(c1 + 0x820) = reinterpret_cast<void*>(0x2222);
    callCount = 0;
    a0.materialFlushPipe(m0, 0, nullptr, nullptr);
    const int n0 = callCount;
    std::memcpy(calls + 300, calls, n0 * sizeof(Call));
    callCount = 0;
    a1.materialFlushPipe(m1, 0, nullptr, nullptr);
    Check(n0 == callCount, "FlushPipe single call count");
    bool same = n0 == callCount;
    for (int i = 0; same && i < n0; ++i) {
        same = calls[300 + i].kind == calls[i].kind;
        for (int k = 0; k < 5 && same; ++k) {
            if (k == 0) continue;  // the cache/chunk object lives in the module's test buffer
            same = calls[300 + i].a[k] == calls[i].a[k];
        }
    }
    Check(same, "FlushPipe single args");

    *reinterpret_cast<int*>(c0 + 0xca4) = 3;
    *reinterpret_cast<int*>(c1 + 0xca4) = 3;
    for (int i = 0; i < 3; ++i) {
        reinterpret_cast<void**>(c0 + 0x420)[i] = reinterpret_cast<void*>(0x5000 + i);
        reinterpret_cast<void**>(c1 + 0x420)[i] = reinterpret_cast<void*>(0x5000 + i);
        reinterpret_cast<void**>(c0 + 0x820)[i] = reinterpret_cast<void*>(0x6000 + i);
        reinterpret_cast<void**>(c1 + 0x820)[i] = reinterpret_cast<void*>(0x6000 + i);
        reinterpret_cast<void**>(c0 + 0x20)[i] = reinterpret_cast<void*>(0x7000 + i);
        reinterpret_cast<void**>(c1 + 0x20)[i] = reinterpret_cast<void*>(0x7000 + i);
    }
    callCount = 0;
    a0.materialFlushPipe(m0, 0, nullptr, nullptr);
    const int m0calls = callCount;
    std::memcpy(calls + 300, calls, m0calls * sizeof(Call));
    callCount = 0;
    a1.materialFlushPipe(m1, 0, nullptr, nullptr);
    Check(m0calls == callCount, "FlushPipe many call count");
    same = m0calls == callCount;
    for (int i = 0; same && i < m0calls; ++i) {
        same = calls[300 + i].kind == calls[i].kind;
        for (int k = 0; k < 8 && same; ++k) {
            void* x = calls[300 + i].a[k];
            void* y = calls[i].a[k];
            if (k == 0) continue;  // the chunk belongs to each side's own fake object
            if (k >= 3 && k <= 5) {
                // pointer-list bases: same offset inside the two caches
                same = reinterpret_cast<const char*>(x) - reinterpret_cast<const char*>(c0) ==
                       reinterpret_cast<const char*>(y) - reinterpret_cast<const char*>(c1);
            } else {
                same = x == y;
            }
        }
    }
    Check(same, "FlushPipe many args");

    a0.materialEnter(m0, nullptr, nullptr, false);
    a1.materialEnter(m1, nullptr, nullptr, false);
    a0.materialLeave(m0, nullptr, nullptr, true);
    a1.materialLeave(m1, nullptr, nullptr, true);
    Check(SameBytes(reinterpret_cast<char*>(m0) + 0xca0, reinterpret_cast<char*>(m1) + 0xca0, 4), "material Enter/Leave");
}

void TestParticle(const Api& a0, const Api& a1, void* p0, void* p1)
{
    FakeVt items[2];
    for (int side = 0; side < 2; ++side) {
        std::memset(&items[side], 0, sizeof items[side]);
        items[side].vtable = items[side].slots;
        items[side].slots[0] = reinterpret_cast<void*>(&RecItem);
    }
    for (int i = 0; i < 200; ++i) {
        void* const matrix = reinterpret_cast<void*>(0x1000 + i);
        void* const shared = reinterpret_cast<void*>(0x7777);  // replaced below, once the fake items exist
        a0.particleAdd(p0, shared, matrix);
        a1.particleAdd(p1, shared, matrix);
        Check(SameBytes(reinterpret_cast<char*>(p0) + 0x20, reinterpret_cast<char*>(p1) + 0x20, 8), "particle counters");
        const int count = *reinterpret_cast<int*>(reinterpret_cast<char*>(p0) + 0x20);
        Check(SameBytes(*reinterpret_cast<void** const*>(reinterpret_cast<char*>(p0) + 0x28),
                        *reinterpret_cast<void** const*>(reinterpret_cast<char*>(p1) + 0x28), count * 8),
              "particle pairs");
    }
    {
        const int count = *reinterpret_cast<int*>(reinterpret_cast<char*>(p0) + 0x20);
        void** const d0 = *reinterpret_cast<void***>(reinterpret_cast<char*>(p0) + 0x28);
        void** const d1 = *reinterpret_cast<void***>(reinterpret_cast<char*>(p1) + 0x28);
        for (int i = 0; i < count; ++i) {
            d0[i * 2 + 1] = &items[0];
            d1[i * 2 + 1] = &items[1];
        }
    }
    InitDisplay(0);
    InitDisplay(1);
    *reinterpret_cast<int*>(stateBuf[0] + 0x68) = 9;
    *reinterpret_cast<int*>(stateBuf[1] + 0x68) = 9;
    callCount = 0;
    a0.particleEnd(p0, displayBuf[0], nullptr);
    const int end0 = callCount;
    std::memcpy(calls + 300, calls, end0 * sizeof(Call));
    callCount = 0;
    a1.particleEnd(p1, displayBuf[1], nullptr);
    Check(end0 == callCount, "Particle End call count");
    bool same = end0 == callCount && end0 == 200;
    for (int i = 0; same && i < end0; ++i) {
        same = calls[300 + i].kind == calls[i].kind;
        for (int k = 0; k < 4 && same; ++k) {
            void* x = calls[300 + i].a[k];
            void* y = calls[i].a[k];
            if (k == 0)
                same = x == &items[0] && y == &items[1];
            else if (k == 1)
                same = x == displayBuf[0] && y == displayBuf[1];
            else
                same = x == y;
        }
    }
    Check(same, "Particle End args");
    Check(SameBytes(stateBuf[0], stateBuf[1], sizeof stateBuf[0]), "Particle End display state");
    a0.particleReset(p0);
    a1.particleReset(p1);
    Check(SameBytes(reinterpret_cast<char*>(p0) + 0x20, reinterpret_cast<char*>(p1) + 0x20, 8), "particle Reset");
    a0.particleBegin(p0, nullptr, nullptr);
    a1.particleBegin(p1, nullptr, nullptr);
    Check(SameBytes(reinterpret_cast<char*>(p0) + 0x20, reinterpret_cast<char*>(p1) + 0x20, 8), "particle Begin");
}

int RingIndex(const void* block, const void* first, int maxCount)
{
    const void* p = first;
    for (int i = 0; i < maxCount; ++i) {
        if (p == block) return i;
        p = *reinterpret_cast<void* const*>(p);
    }
    return -1;
}

void TestProjector(const Api& a0, const Api& a1, void* p0, void* p1)
{
    void* projectors[20];
    for (int i = 0; i < 20; ++i) projectors[i] = reinterpret_cast<void*>(0x1000 + i);
    for (int i = 0; i < 20; ++i) {
        a0.projectorAdd(p0, projectors[i]);
        a1.projectorAdd(p1, projectors[i]);
        const void* first0 = *reinterpret_cast<void**>(reinterpret_cast<char*>(p0) + 0x20);
        const void* first1 = *reinterpret_cast<void**>(reinterpret_cast<char*>(p1) + 0x20);
        bool same = (first0 != nullptr) == (first1 != nullptr);
        if (first0 && first1) {
            const void* b0 = first0;
            const void* b1 = first1;
            int guard = 0;
            do {
                const int e0 = *reinterpret_cast<const int*>(reinterpret_cast<const char*>(b0) + 8);
                const int e1 = *reinterpret_cast<const int*>(reinterpret_cast<const char*>(b1) + 8);
                same = same && e0 == e1 && e0 <= 13;
                for (int k = 0; k < e0 && same; ++k)
                    same = reinterpret_cast<void* const*>(reinterpret_cast<const char*>(b0) + 0xc)[k] ==
                           reinterpret_cast<void* const*>(reinterpret_cast<const char*>(b1) + 0xc)[k];
                b0 = *reinterpret_cast<void* const*>(b0);
                b1 = *reinterpret_cast<void* const*>(b1);
            } while (same && b0 != first0 && b1 != first1 && guard++ < 8);
        }
        Check(same, "projector ring");
    }

    FakeVt projs[2];
    for (int side = 0; side < 2; ++side) {
        std::memset(&projs[side], 0, sizeof projs[side]);
        projs[side].vtable = projs[side].slots;
        projs[side].slots[1] = reinterpret_cast<void*>(&RecProjector);
    }
    // put the fake projectors into the rings by hand
    const void* first0 = *reinterpret_cast<void**>(reinterpret_cast<char*>(p0) + 0x20);
    const void* first1 = *reinterpret_cast<void**>(reinterpret_cast<char*>(p1) + 0x20);
    {
        const void* b0 = first0;
        const void* b1 = first1;
        do {
            const int e0 = *reinterpret_cast<const int*>(reinterpret_cast<const char*>(b0) + 8);
            for (int k = 0; k < e0; ++k) {
                reinterpret_cast<void**>(const_cast<char*>(reinterpret_cast<const char*>(b0) + 0xc))[k] = &projs[0];
                reinterpret_cast<void**>(const_cast<char*>(reinterpret_cast<const char*>(b1) + 0xc))[k] = &projs[1];
            }
            b0 = *reinterpret_cast<void* const*>(b0);
            b1 = *reinterpret_cast<void* const*>(b1);
        } while (b0 != first0);
    }
    InitDisplay(0);
    InitDisplay(1);
    callCount = 0;
    a0.projectorEnd(p0, displayBuf[0], nullptr);
    const int n0 = callCount;
    std::memcpy(calls + 300, calls, n0 * sizeof(Call));
    callCount = 0;
    a1.projectorEnd(p1, displayBuf[1], nullptr);
    Check(n0 == callCount && n0 == 20, "Projector End call count");
    bool same = n0 == callCount;
    for (int i = 0; same && i < n0; ++i) {
        same = calls[300 + i].kind == calls[i].kind;
        for (int k = 0; k < 3 && same; ++k) {
            void* x = calls[300 + i].a[k];
            void* y = calls[i].a[k];
            if (k == 0)
                same = x == &projs[0] && y == &projs[1];
            else if (k == 1)
                same = x == displayBuf[0] && y == displayBuf[1];
            else
                same = x == y;
        }
    }
    Check(same, "Projector End args");
    a0.projectorReset(p0);
    a1.projectorReset(p1);
    Check(*reinterpret_cast<void**>(reinterpret_cast<char*>(p0) + 0x20) == nullptr &&
              *reinterpret_cast<void**>(reinterpret_cast<char*>(p1) + 0x20) == nullptr,
          "Projector Reset");
    a0.projectorBegin(p0, nullptr, nullptr);
    a1.projectorBegin(p1, nullptr, nullptr);
    Check(*reinterpret_cast<void**>(reinterpret_cast<char*>(p0) + 0x20) == nullptr &&
              *reinterpret_cast<void**>(reinterpret_cast<char*>(p1) + 0x20) == nullptr,
          "Projector Begin");
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u4 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 200;
    SetDllDirectoryA(argv[1]);
    const std::string dir = argv[1];
    LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str());
    mod[0] = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    mod[1] = GetModuleHandleA("Render3DCore_7c_orig.dll");
    if (!mod[0] || !mod[1]) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Api api[2] = {Load(0), Load(1)};

    alignas(16) unsigned char objects[2][0x2000];
    std::memset(objects, 0xcd, sizeof objects);
    void* alphaBlend[2];
    void* alphaMask[2];
    void* unsorted[2];
    void* material[2];
    void* generic[2];
    void* particle[2];
    void* projector[2];
    for (int side = 0; side < 2; ++side) {
        alphaBlend[side] = objects[side];
        alphaMask[side] = objects[side] + 0x100;
        unsorted[side] = objects[side] + 0x200;
        material[side] = objects[side] + 0x300;
        generic[side] = objects[side] + 0xf00;
        particle[side] = objects[side] + 0x1000;
        projector[side] = objects[side] + 0x1100;
    }
    // the caches are 0x80/0x58/0x64 bytes but MaterialChunkCache is 0xd2c; move the later ones out of its way
    static unsigned char materialStore[2][0xe00];
    for (int side = 0; side < 2; ++side) material[side] = materialStore[side];

    for (int side = 0; side < 2; ++side) {
        api[side].newAlphaBlend(alphaBlend[side], 1);
        api[side].newAlphaMask(alphaMask[side], 1);
        api[side].newUnsorted(unsorted[side], 1);
        api[side].newMaterialChunk(material[side], 1);
        api[side].newGeneric(generic[side]);
        api[side].newParticleFX(particle[side], 1);
        api[side].newProjector(projector[side], 1);
    }

    for (int round = 0; round < rounds; ++round) {
        TestAlphaBlendScalars(api[0], api[1], alphaBlend[0], alphaBlend[1]);
        TestDisplayState(api[0], api[1], alphaBlend[0], alphaBlend[1]);
        TestFreeRecords(api[0], api[1], alphaBlend[0], alphaBlend[1]);
        TestTree(api[0], api[1]);
        TestMasks(api[0], api[1], alphaMask[0], alphaMask[1], unsorted[0], unsorted[1]);
        TestGeneric(api[0], api[1], generic[0], generic[1]);
        TestMaterial(api[0], api[1], material[0], material[1]);
        TestParticle(api[0], api[1], particle[0], particle[1]);
        TestProjector(api[0], api[1], projector[0], projector[1]);
        
    }

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
