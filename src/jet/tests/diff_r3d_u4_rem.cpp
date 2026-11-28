// Differential test of the U4 wave-4 remainder: lifetimes, vtables/data, the cache entry points and the tree
// rebalancer against Render3DCore_7c_orig.dll. Objects are built by each side's constructor into their own buffer;
// shared fake Display/Material/GeometryChunk objects let both sides log identical pointer arguments.
//
// usage: diff_r3d_u4_rem.exe <dir with Render3DCore_7c.dll and Render3DCore_7c_orig.dll> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

namespace {

struct Rec {  // AlphaBlendCache::CacheRecord
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
struct Bucket {
    int numNodes;
    Rec* head;
    Rec* z;
    void* cache;
};
static_assert(sizeof(Rec) == 0x34, "Rec");
static_assert(sizeof(Bucket) == 0x10, "Bucket");

HMODULE mod[2];
unsigned long long checks, bad;
std::mt19937 rng(0x4e55);

int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }
float FRange(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }
uint32_t FBits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
float Bits(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }

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

void* Data(int side, const char* name)
{
    void* p = GetProcAddress(mod[side], name);
    if (!p) {
        std::printf("missing data export %s in %s\n", name, side ? "orig" : "ours");
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

// ---------------------------------------------------------------------------------------------------------------
// host stand-ins

struct Call {
    int kind;
    void* a[12];
};
Call calls[4000];
int callCount;

extern "C" void __cdecl Noop() {}

extern "C" void __cdecl RecChunk(void* material, void* display, void* view, void* chunk, void* lights, void* modelView,
                                 void* pass)
{
    Call& c = calls[callCount++];
    c.kind = 1;
    c.a[0] = material;
    c.a[1] = display;
    c.a[2] = view;
    c.a[3] = chunk;
    c.a[4] = lights;
    c.a[5] = modelView;
    c.a[6] = pass;
}

extern "C" void __cdecl RecNumChunks(void* material, void* display, void* view, void* chunks, void* lights,
                                     void* modelViews, void* count, void* pass)
{
    Call& c = calls[callCount++];
    c.kind = 2;
    c.a[0] = material;
    c.a[1] = display;
    c.a[2] = view;
    c.a[3] = chunks;
    c.a[4] = lights;
    c.a[5] = modelViews;
    c.a[6] = count;
    c.a[7] = pass;
    const int n = static_cast<int>(reinterpret_cast<intptr_t>(count));
    auto** const chunkList = static_cast<void**>(chunks);
    auto** const lightList = static_cast<void**>(lights);
    auto** const matrixList = static_cast<void**>(modelViews);
    for (int i = 0; i < n && i < 4; ++i) {
        c.a[8 + i] = chunkList ? chunkList[i] : nullptr;
        (void)lightList;
        (void)matrixList;
    }
}

extern "C" void __cdecl RecPrimitives(void* material, void* display, void* view, void* chunk, void* lights,
                                      void* modelView, void* numIndices, void* indices, void* numPrimitives,
                                      void* primitive, void* pass)
{
    Call& c = calls[callCount++];
    c.kind = 3;
    c.a[0] = material;
    c.a[1] = display;
    c.a[2] = view;
    c.a[3] = chunk;
    c.a[4] = lights;
    c.a[5] = modelView;
    c.a[6] = numIndices;
    c.a[7] = indices;
    c.a[8] = numPrimitives;
    c.a[9] = primitive;
    c.a[10] = pass;
}

void* NoopTable[256];

struct FakeMaterial {
    void** vtable;
    void* slots[64];
};

void InitMaterial(FakeMaterial& m)
{
    std::memset(&m, 0, sizeof m);
    for (void*& p : m.slots) p = reinterpret_cast<void*>(&Noop);
    m.vtable = m.slots;
    m.slots[0x3c / 4] = reinterpret_cast<void*>(&RecChunk);
    m.slots[0x40 / 4] = reinterpret_cast<void*>(&RecNumChunks);
    m.slots[0x44 / 4] = reinterpret_cast<void*>(&RecPrimitives);
}

struct FakeChunk {
    void** vtable;
    void* slots[128];
    unsigned char data[0x40];
};

void InitChunk(FakeChunk& c, unsigned flags, int numPrimitives)
{
    std::memset(&c, 0, sizeof c);
    c.vtable = c.slots;
    for (void*& p : c.slots) p = reinterpret_cast<void*>(&Noop);
    *reinterpret_cast<unsigned*>(c.data + 0x18) = flags;
    *reinterpret_cast<int*>(c.data + 0x1c) = numPrimitives;
}

unsigned char displayBuf[2][0x100];
unsigned char stateBuf[2][0x100];
void InitDisplay(int side)
{
    for (void*& p : NoopTable) p = reinterpret_cast<void*>(&Noop);
    std::memset(displayBuf[side], 0, sizeof displayBuf[side]);
    std::memset(stateBuf[side], 0, sizeof stateBuf[side]);
    *reinterpret_cast<void***>(displayBuf[side]) = NoopTable;
    *reinterpret_cast<void**>(displayBuf[side] + 0x54) = stateBuf[side];
}

struct Matrix {
    float m[16];
};

// ---------------------------------------------------------------------------------------------------------------
// API

struct Api {
    void(__thiscall* abCtor)(void*, int);
    void(__thiscall* maskCtor)(void*, int);
    void(__thiscall* unsortedCtor)(void*, int);
    void(__thiscall* materialCtor)(void*, int);
    void(__thiscall* genericCtor)(void*);
    void(__thiscall* particleCtor)(void*, int);
    void(__thiscall* projectorCtor)(void*, int);

    void(__cdecl* abAdd)(void*, void*, void*, const void*, void*, const void*, const void*);
    void(__cdecl* abEnd)(void*, void*, void*);
    void(__cdecl* abEnter)(void*, void*, void*, bool);
    void(__cdecl* abLeave)(void*, void*, void*, bool);
    void(__cdecl* abRecalculate)(void*, bool);
    int(__cdecl* abSetupPrimitiveSort)(void*, void*, int*, int*, float*, int*);
    void(__cdecl* abSortChunk)(void*, Rec*);
    void(__cdecl* abSortChunkIntoBuckets)(void*, const void*, void*, const void*, const void*);
    Rec*(__cdecl* abInsert)(Bucket*, Rec*);
    void(__cdecl* abSplitTree)(Bucket*, Rec*, Rec*, Rec*, Rec*, Rec*);
    Rec*(__cdecl* abRotateTree)(Bucket*, Rec*, Rec*);
    Rec*(__cdecl* abGetFree)(void*, const void*, void*, const void*, const void*, unsigned short*, float);

    void(__cdecl* maskAdd)(void*, const void*, void*, const void*, const void*);
    void(__cdecl* maskEnd)(void*, void*, void*);
    void(__cdecl* unsortedAdd)(void*, const void*, void*, const void*, const void*, float);
    void(__cdecl* unsortedEnd)(void*, void*, void*);
    void(__cdecl* unsortedSetDelayed)(bool);
    void(__cdecl* materialAdd)(void*, void*, void*, const void*, void*, const void*, const void*);
    void(__cdecl* materialFlush)(void*, int, void*, void*);
    void(__cdecl* genericAdd)(void*, void*, void*, const void*, void*, const void*, const void*);
    void(__cdecl* genericEnd)(void*, void*, void*);
};

Api Load(int side)
{
    Api a{};
    a.abCtor = Proc<decltype(a.abCtor)>(side, "??0AlphaBlendCache@Jet@@QAE@XZ");
    a.maskCtor = Proc<decltype(a.maskCtor)>(side, "??0AlphaMaskCache@Jet@@QAE@XZ");
    a.unsortedCtor = Proc<decltype(a.unsortedCtor)>(side, "??0UnsortedAlphaBlendCache@Jet@@QAE@XZ");
    a.materialCtor = Proc<decltype(a.materialCtor)>(side, "??0MaterialChunkCache@Jet@@QAE@XZ");
    a.genericCtor = Proc<decltype(a.genericCtor)>(side, "??_FGenericMaterialChunkCache@Jet@@QAEXXZ");
    a.particleCtor = Proc<decltype(a.particleCtor)>(side, "??0ParticleFXCache@Jet@@QAE@XZ");
    a.projectorCtor = Proc<decltype(a.projectorCtor)>(side, "??0ProjectorCache@Jet@@QAE@XZ");

    a.abAdd = Proc<decltype(a.abAdd)>(side, "?Add@AlphaBlendCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@PBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z");
    a.abEnd = Proc<decltype(a.abEnd)>(side, "?End@AlphaBlendCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    a.abEnter = Proc<decltype(a.abEnter)>(side, "?Enter@AlphaBlendCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@_N@Z");
    a.abLeave = Proc<decltype(a.abLeave)>(side, "?Leave@AlphaBlendCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@_N@Z");
    a.abRecalculate = Proc<decltype(a.abRecalculate)>(side, "?RecalculateBucketParametrs@AlphaBlendCache@Jet@@MAAX_N@Z");
    a.abSetupPrimitiveSort = Proc<decltype(a.abSetupPrimitiveSort)>(side, "?SetupPrimitiveSort@AlphaBlendCache@Jet@@IAAHPAVGeometryChunk@2@AAH1AAMAAW4DPrimitive@2@@Z");
    a.abSortChunk = Proc<decltype(a.abSortChunk)>(side, "?SortChunk@AlphaBlendCache@Jet@@IAAXAAUCacheRecord@12@@Z");
    a.abSortChunkIntoBuckets = Proc<decltype(a.abSortChunkIntoBuckets)>(side, "?SortChunkIntoBuckets@AlphaBlendCache@Jet@@IAAXPBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z");
    a.abInsert = Proc<decltype(a.abInsert)>(side, "?Insert@CacheBucket@AlphaBlendCache@Jet@@QAAPAUCacheRecord@23@PAU423@@Z");
    a.abSplitTree = Proc<decltype(a.abSplitTree)>(side, "?SplitTree@CacheBucket@AlphaBlendCache@Jet@@QAAXPAUCacheRecord@23@0000@Z");
    a.abRotateTree = Proc<decltype(a.abRotateTree)>(side, "?RotateTree@CacheBucket@AlphaBlendCache@Jet@@QAAPAUCacheRecord@23@PAU423@0@Z");
    a.abGetFree = Proc<decltype(a.abGetFree)>(side, "?GetFreeCacheRecord@AlphaBlendCache@Jet@@QAAPAUCacheRecord@12@PBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@PAGM@Z");

    a.maskAdd = Proc<decltype(a.maskAdd)>(side, "?Add@AlphaMaskCache@Jet@@UAAXPBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z");
    a.maskEnd = Proc<decltype(a.maskEnd)>(side, "?End@AlphaMaskCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    a.unsortedAdd = Proc<decltype(a.unsortedAdd)>(side, "?Add@UnsortedAlphaBlendCache@Jet@@UAAXPBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@M@Z");
    a.unsortedEnd = Proc<decltype(a.unsortedEnd)>(side, "?End@UnsortedAlphaBlendCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    a.unsortedSetDelayed = Proc<decltype(a.unsortedSetDelayed)>(side, "?SetTrainzDelayedRender@UnsortedAlphaBlendCache@Jet@@SAX_N@Z");
    a.materialAdd = Proc<decltype(a.materialAdd)>(side, "?Add@MaterialChunkCache@Jet@@QAAXPAVDisplay@2@PAVViewPort@2@PBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z");
    a.materialFlush = Proc<decltype(a.materialFlush)>(side, "?FlushPipe@MaterialChunkCache@Jet@@MAAXHPAVDisplay@2@PAVViewPort@2@@Z");
    a.genericAdd = Proc<decltype(a.genericAdd)>(side, "?Add@GenericMaterialChunkCache@Jet@@QAAXPAVDisplay@2@PAVViewPort@2@PBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z");
    a.genericEnd = Proc<decltype(a.genericEnd)>(side, "?End@GenericMaterialChunkCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z");
    return a;
}

// check one vtable slot against the local export of the same name
void CheckSlot(const Api*, int side, void* object, int byteOffset, const char* name, const char* what)
{
    void** const table = *reinterpret_cast<void***>(object);
    void* const actual = *reinterpret_cast<void**>(reinterpret_cast<char*>(table) + byteOffset);
    void* const expected = GetProcAddress(mod[side], name);
    Check(actual == expected, what);
}

// the virtual destructor lives in the ReferenceCount vtable (slot 1: scalar deleting destructor); the
// RenderingCache vtable starts with SetHost, as in the original
void DestroyObject(void* object, int referenceCountOffset)
{
    void** const table = *reinterpret_cast<void***>(static_cast<char*>(object) + referenceCountOffset);
    typedef void*(__thiscall* Vdtor)(void*, int);
    (*reinterpret_cast<Vdtor>(table[1]))(static_cast<char*>(object) + referenceCountOffset, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// tree comparisons

const int kMaxRecs = 96;

int IndexOf(const Rec* p, Rec* const* recs, int n, const Rec* z)
{
    if (!p) return -2;
    if (p == z) return -1;
    for (int i = 0; i < n; ++i)
        if (p == recs[i]) return i;
    return -3;
}

void CompareTreeShape(const Rec* a, const Rec* b, Rec* const* ra, int na, const Rec* za, Rec* const* rb, int nb,
                      const Rec* zb, const char* what, int depth)
{
    if (depth > 200) {
        Check(false, what);
        return;
    }
    Check(IndexOf(a, ra, na, za) == IndexOf(b, rb, nb, zb), what);
    if (a == za || b == zb || !a || !b) return;
    Check(FBits(a->distance) == FBits(b->distance), what);
    CompareTreeShape(a->left, b->left, ra, na, za, rb, nb, zb, what, depth + 1);
    CompareTreeShape(a->right, b->right, ra, na, za, rb, nb, zb, what, depth + 1);
}

int CollectTree(const Rec* node, const Rec* z, Rec** out, int count, int max)
{
    if (!node || node == z || count >= max) return count;
    out[count++] = const_cast<Rec*>(node);
    count = CollectTree(node->left, z, out, count, max);
    count = CollectTree(node->right, z, out, count, max);
    return count;
}

bool TreeEqual(const Rec* a, const Rec* b, Rec* const* ra, int na, const Rec* za, Rec* const* rb, int nb,
               const Rec* zb)
{
    if (IndexOf(a, ra, na, za) != IndexOf(b, rb, nb, zb)) return false;
    if (a == za || b == zb || !a || !b) return true;
    return FBits(a->distance) == FBits(b->distance) && TreeEqual(a->left, b->left, ra, na, za, rb, nb, zb) &&
           TreeEqual(a->right, b->right, ra, na, za, rb, nb, zb);
}

void DumpNode(const Rec* node, Rec* const* recs, int n, const Rec* z, int depth)
{
    if (!node || node == z || depth > 6) return;
    std::printf(" %d(d%08x,f%x,P%d,L%d,R%d)", IndexOf(node, recs, n, z), FBits(node->distance), node->flags,
                IndexOf(node->parent, recs, n, z), IndexOf(node->left, recs, n, z),
                IndexOf(node->right, recs, n, z));
    DumpNode(node->left, recs, n, z, depth + 1);
    DumpNode(node->right, recs, n, z, depth + 1);
}

// ---------------------------------------------------------------------------------------------------------------
// tests

void TestLifetimes(Api api[2])
{
    alignas(16) unsigned char a[2][0x1000];
    std::memset(a, 0xcd, sizeof a);
    api[0].abCtor(a[0], 1);
    api[1].abCtor(a[1], 1);

    // flat fields, pointers only by null-ness
    Check(SameBytes(a[0] + 0x20, a[1] + 0x20, 8), "AB bucket vector scalars");
    if (!SameBytes(a[0] + 0x20, a[1] + 0x20, 8))
        std::printf("  bucket count/cap %d/%d vs %d/%d\n", *reinterpret_cast<int*>(a[0] + 0x20),
                    *reinterpret_cast<int*>(a[0] + 0x24), *reinterpret_cast<int*>(a[1] + 0x20),
                    *reinterpret_cast<int*>(a[1] + 0x24));
    Check(*reinterpret_cast<void**>(a[0] + 0x28) != nullptr && *reinterpret_cast<void**>(a[1] + 0x28) != nullptr,
          "AB bucket data");
    Check(SameBytes(a[0] + 0x2c, a[1] + 0x2c, 0x40), "AB ranges/bools");
    Check((*reinterpret_cast<void**>(a[0] + 0x70) != nullptr) == (*reinterpret_cast<void**>(a[1] + 0x70) != nullptr),
          "AB index pointers");
    Check(*reinterpret_cast<int*>(a[0] + 0x34) == *reinterpret_cast<int*>(a[1] + 0x34), "AB numBuckets");
    Check(*reinterpret_cast<int*>(a[0] + 0x20) == *reinterpret_cast<int*>(a[1] + 0x20), "AB bucket count");
    Check(static_cast<double>(*reinterpret_cast<float*>(a[0] + 0x38)) ==
              static_cast<double>(*reinterpret_cast<float*>(a[1] + 0x38)),
          "AB max range");

    void* const singleton[2] = {*static_cast<void**>(Data(0, "?singleton@AlphaBlendCache@Jet@@2PAV12@A")),
                                *static_cast<void**>(Data(1, "?singleton@AlphaBlendCache@Jet@@2PAV12@A"))};
    Check(singleton[0] == a[0], "AB singleton ours");
    Check(singleton[1] == a[1], "AB singleton orig");

    CheckSlot(nullptr, 0, a[0], 0x20, "?IsEmpty@AlphaBlendCache@Jet@@UBE_NXZ", "AB vtable IsEmpty");
    CheckSlot(nullptr, 1, a[1], 0x20, "?IsEmpty@AlphaBlendCache@Jet@@UBE_NXZ", "AB orig vtable IsEmpty");
    CheckSlot(nullptr, 0, a[0], 0x24, "?Add@AlphaBlendCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@PBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z", "AB vtable Add");
    CheckSlot(nullptr, 0, a[0], 0x30, "?GetSortFunction@AlphaBlendCache@Jet@@MAAP6AHPBX0@ZXZ", "AB vtable GetSortFunction");
    CheckSlot(nullptr, 0, a[0], 0x34, "?RecalculateBucketParametrs@AlphaBlendCache@Jet@@MAAX_N@Z", "AB vtable Recalculate");

    const int* const vt[2] = {static_cast<const int*>(Data(0, "??_8AlphaBlendCache@Jet@@7B@")),
                              static_cast<const int*>(Data(1, "??_8AlphaBlendCache@Jet@@7B@"))};
    Check(vt[0][0] == vt[1][0] && vt[0][1] == vt[1][1], "AB vbtable");

    // construct the other classes and check their vtables/data
    alignas(16) unsigned char o[2][7][0x1000];
    std::memset(o, 0, sizeof o);
    api[0].maskCtor(o[0][0], 1);
    api[1].maskCtor(o[1][0], 1);
    api[0].unsortedCtor(o[0][1], 1);
    api[1].unsortedCtor(o[1][1], 1);
    api[0].materialCtor(o[0][2], 1);
    api[1].materialCtor(o[1][2], 1);
    api[0].genericCtor(o[0][3]);
    api[1].genericCtor(o[1][3]);
    api[0].particleCtor(o[0][4], 1);
    api[1].particleCtor(o[1][4], 1);
    api[0].projectorCtor(o[0][5], 1);
    api[1].projectorCtor(o[1][5], 1);

    for (int s = 0; s < 2; ++s) {
        CheckSlot(nullptr, s, o[s][0], 0x20, "?IsEmpty@AlphaMaskCache@Jet@@UBE_NXZ", "mask vtable IsEmpty");
        CheckSlot(nullptr, s, o[s][0], 0x24, "?Add@AlphaMaskCache@Jet@@UAAXPBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z", "mask vtable Add");
        CheckSlot(nullptr, s, o[s][1], 0x20, "?IsEmpty@UnsortedAlphaBlendCache@Jet@@UBE_NXZ", "unsorted vtable IsEmpty");
        CheckSlot(nullptr, s, o[s][2], 0x20, "?IsEmpty@MaterialChunkCache@Jet@@UBE_NXZ", "material vtable IsEmpty");
        CheckSlot(nullptr, s, o[s][2], 0x24, "?FlushPipe@MaterialChunkCache@Jet@@MAAXHPAVDisplay@2@PAVViewPort@2@@Z", "material vtable FlushPipe");
        CheckSlot(nullptr, s, o[s][4], 0x20, "?Add@ParticleFXCache@Jet@@UAAXPBVParticleFXRenderItem@2@PBVMatrix4@2@@Z", "particle vtable Add");
        CheckSlot(nullptr, s, o[s][3], 8, "?Reset@GenericMaterialChunkCache@Jet@@UAAXXZ", "generic vtable Reset");
        CheckSlot(nullptr, s, o[s][3], 12, "?Begin@GenericMaterialChunkCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z", "generic vtable Begin");
        CheckSlot(nullptr, s, o[s][3], 16, "?End@GenericMaterialChunkCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z", "generic vtable End");
        CheckSlot(nullptr, s, o[s][5], 8, "?Reset@ProjectorCache@Jet@@UAAXXZ", "projector vtable Reset");
        CheckSlot(nullptr, s, o[s][5], 16, "?End@ProjectorCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@@Z", "projector vtable End");
    }

    const char* const ids[7] = {"?AlphaBlendCacheID@AlphaBlendCache@Jet@@2VPString@2@A",
                                "?AlphaMaskCacheID@AlphaMaskCache@Jet@@2VPString@2@A",
                                "?MaterialChunkCacheID@MaterialChunkCache@Jet@@2VPString@2@A",
                                "?GenericMaterialChunkCacheID@GenericMaterialChunkCache@Jet@@2VPString@2@A",
                                "?ParticleFXCacheID@ParticleFXCache@Jet@@2VPString@2@A",
                                "?ProjectorCacheID@ProjectorCache@Jet@@2VPString@2@A",
                                "?UnsortedAlphaBlendCacheID@UnsortedAlphaBlendCache@Jet@@2VPString@2@A"};
    const char* const texts[7] = {"AlphaBlendCache", "AlphaMaskCache", "ChunkCache", "GenericChunkCache",
                                  "ParticleFXCache", "ProjectorCache", "UnsortedAlphaBlendCache"};
    for (int i = 0; i < 7; ++i) {
        const char* const a0 = *static_cast<char**>(Data(0, ids[i]));
        const char* const a1 = *static_cast<char**>(Data(1, ids[i]));
        Check(a0 && a1 && std::strcmp(a0 + 0x18, texts[i]) == 0 && std::strcmp(a1 + 0x18, texts[i]) == 0,
              "CacheID text");
    }

    Check(o[0][0][0x20] == 0 && o[1][0][0x20] == 0, "mask empty");
    Check(o[0][1][0x20] == 0 && o[1][1][0x20] == 0, "unsorted empty");
    Check(*reinterpret_cast<int*>(o[0][2] + 0xca0) == 0, "material empty");
    Check(*reinterpret_cast<int*>(o[0][3] + 0x20) == 0, "generic empty");
    Check(*reinterpret_cast<int*>(o[0][4] + 0x20) == 0, "particle empty");
    Check(*reinterpret_cast<void**>(o[0][5] + 0x20) == nullptr, "projector empty");
    Check(*reinterpret_cast<unsigned char*>(o[0][5] + 0x1c) == 0, "projector rendersGeometry");

    // destruct through the ReferenceCount vtable's scalar deleting destructor, then the singleton must be cleared
    for (int s = 0; s < 2; ++s) {
        DestroyObject(a[s], 0x78);
        DestroyObject(o[s][0], 0x50);
        DestroyObject(o[s][1], 0x5c);
        DestroyObject(o[s][2], 0xd24);
        DestroyObject(o[s][3], 0x30);
        DestroyObject(o[s][4], 0x2c);
        DestroyObject(o[s][5], 0x24);
    }
    Check(*static_cast<void**>(Data(0, "?singleton@AlphaBlendCache@Jet@@2PAV12@A")) == nullptr, "AB singleton cleared");
    Check(*static_cast<void**>(Data(1, "?singleton@AlphaBlendCache@Jet@@2PAV12@A")) == nullptr, "AB orig singleton cleared");
}

void TestMaskAndUnsorted(Api api[2], FakeMaterial& material, FakeChunk& chunk, Matrix& matrix)
{
    alignas(16) unsigned char mask[2][0x100], unsorted[2][0x100];
    std::memset(mask, 0, sizeof mask);
    std::memset(unsorted, 0, sizeof unsorted);
    api[0].maskCtor(mask[0], 1);
    api[1].maskCtor(mask[1], 1);
    api[0].unsortedCtor(unsorted[0], 1);
    api[1].unsortedCtor(unsorted[1], 1);

    for (int i = 0; i < 3; ++i) {
        api[0].maskAdd(mask[0], &material, &chunk, nullptr, &matrix);
        api[1].maskAdd(mask[1], &material, &chunk, nullptr, &matrix);
        api[0].unsortedAdd(unsorted[0], &material, &chunk, nullptr, &matrix, 1.0f + static_cast<float>(i));
        api[1].unsortedAdd(unsorted[1], &material, &chunk, nullptr, &matrix, 1.0f + static_cast<float>(i));
    }

    InitDisplay(0);
    *reinterpret_cast<float*>(stateBuf[0] + 0xcc) = Bits(0x3f000000u);
    stateBuf[0][0xa0] = 0x22;
    unsigned char baseline[0x100];
    std::memcpy(baseline, stateBuf[0], sizeof baseline);

    callCount = 0;
    api[0].maskEnd(mask[0], displayBuf[0], nullptr);
    const int n0 = callCount;
    Call c0[16];
    std::memcpy(c0, calls, sizeof(Call) * static_cast<size_t>(n0));
    unsigned char after0[0x100];
    std::memcpy(after0, stateBuf[0], sizeof after0);
    std::memcpy(stateBuf[0], baseline, sizeof baseline);
    callCount = 0;
    api[1].maskEnd(mask[1], displayBuf[0], nullptr);
    Check(n0 == callCount, "mask End call count");
    for (int i = 0; i < n0 && i < callCount; ++i) {
        if (!SameBytes(&c0[i], &calls[i], sizeof(Call))) {
            std::printf("  mask call %d ours:", i);
            for (void* p : c0[i].a) std::printf(" %p", p);
            std::printf("\n  orig:");
            for (void* p : calls[i].a) std::printf(" %p", p);
            std::printf("\n");
            break;
        }
    }
    Check(SameBytes(after0, stateBuf[0], sizeof after0), "mask End display state");
    Check(*reinterpret_cast<int*>(mask[0] + 0x20) == 0 && *reinterpret_cast<int*>(mask[1] + 0x20) == 0,
          "mask End reset");

    // the delayed-render flag suppresses UnsortedAlphaBlendCache::End
    api[0].unsortedSetDelayed(true);
    api[1].unsortedSetDelayed(true);
    callCount = 0;
    api[0].unsortedEnd(unsorted[0], displayBuf[0], nullptr);
    api[1].unsortedEnd(unsorted[1], displayBuf[0], nullptr);
    Check(callCount == 0, "unsorted delayed End suppressed");
    api[0].unsortedSetDelayed(false);
    api[1].unsortedSetDelayed(false);

    std::memcpy(stateBuf[0], baseline, sizeof baseline);
    callCount = 0;
    api[0].unsortedEnd(unsorted[0], displayBuf[0], nullptr);
    const int u0 = callCount;
    Call cu[16];
    std::memcpy(cu, calls, sizeof(Call) * static_cast<size_t>(u0));
    unsigned char afterU[0x100];
    std::memcpy(afterU, stateBuf[0], sizeof afterU);
    std::memcpy(stateBuf[0], baseline, sizeof baseline);
    callCount = 0;
    api[1].unsortedEnd(unsorted[1], displayBuf[0], nullptr);
    Check(u0 == callCount, "unsorted End call count");
    for (int i = 0; i < u0 && i < callCount; ++i) {
        if (!SameBytes(&cu[i], &calls[i], sizeof(Call))) {
            std::printf("  unsorted call %d ours:", i);
            for (void* p : cu[i].a) std::printf(" %p", p);
            std::printf("\n  orig:");
            for (void* p : calls[i].a) std::printf(" %p", p);
            std::printf("\n");
            break;
        }
    }
    Check(SameBytes(afterU, stateBuf[0], sizeof afterU), "unsorted End display state");
    if (!SameBytes(afterU, stateBuf[0], sizeof afterU)) {
        for (int i = 0; i < 0x100; ++i)
            if (afterU[i] != stateBuf[0][i])
                std::printf("  unsorted state +%02x ours %02x orig %02x\n", i, afterU[i], stateBuf[0][i]);
    }
    Check(*reinterpret_cast<int*>(unsorted[0] + 0x20) == 0, "unsorted End reset");

    DestroyObject(mask[0], 0x50);
    DestroyObject(mask[1], 0x50);
    DestroyObject(unsorted[0], 0x5c);
    DestroyObject(unsorted[1], 0x5c);
}

void TestMaterialCache(Api api[2], FakeMaterial& material)
{
    alignas(16) unsigned char cache[2][0x1000];
    std::memset(cache, 0, sizeof cache);
    api[0].materialCtor(cache[0], 1);
    api[1].materialCtor(cache[1], 1);

    FakeChunk chunks[12];
    for (int i = 0; i < 12; ++i) InitChunk(chunks[i], 0, 1);

    InitDisplay(0);
    InitDisplay(1);
    callCount = 0;
    for (int i = 0; i < 10; ++i) {
        api[0].materialAdd(cache[0], displayBuf[0], nullptr, &material, &chunks[i], nullptr, nullptr);
        api[1].materialAdd(cache[1], displayBuf[0], nullptr, &material, &chunks[i], nullptr, nullptr);
    }
    Check(*reinterpret_cast<int*>(cache[0] + 0xca0) == *reinterpret_cast<int*>(cache[1] + 0xca0), "material pipe count");
    Check(SameBytes(cache[0] + 0xca4, cache[1] + 0xca4, 0x80), "material pipe sizes");
    for (int i = 0; i < 32; ++i) {
        Check(reinterpret_cast<void**>(cache[0] + 0xc20)[i] == reinterpret_cast<void**>(cache[1] + 0xc20)[i],
              "material pipe material");
        for (int j = 0; j < 8; ++j) {
            const int slot = (i + 1) * 8 + j;
            Check(reinterpret_cast<void**>(cache[0] + 0x20)[slot] == reinterpret_cast<void**>(cache[1] + 0x20)[slot],
                  "material pipe chunk");
            if (reinterpret_cast<void**>(cache[0] + 0x20)[slot] != reinterpret_cast<void**>(cache[1] + 0x20)[slot])
                std::printf("  pipe %d slot %d chunk %p vs %p\n", i, j,
                            reinterpret_cast<void**>(cache[0] + 0x20)[slot],
                            reinterpret_cast<void**>(cache[1] + 0x20)[slot]);
            Check(reinterpret_cast<void**>(cache[0] + 0x420)[slot] == reinterpret_cast<void**>(cache[1] + 0x420)[slot],
                  "material pipe lights");
            Check(reinterpret_cast<void**>(cache[0] + 0x820)[slot] == reinterpret_cast<void**>(cache[1] + 0x820)[slot],
                  "material pipe matrix");
        }
    }

    // a full pipe is flushed by Add; the call must be the material's RenderNumChunks
    callCount = 0;
    api[0].materialAdd(cache[0], displayBuf[0], nullptr, &material, &chunks[10], nullptr, nullptr);
    const int n0 = callCount;
    Call c0[8];
    std::memcpy(c0, calls, sizeof(Call) * static_cast<size_t>(n0));
    callCount = 0;
    api[1].materialAdd(cache[1], displayBuf[0], nullptr, &material, &chunks[10], nullptr, nullptr);
    Check(n0 == callCount, "material flush call count");
    for (int i = 0; i < n0 && i < callCount; ++i) Check(SameBytes(&c0[i], &calls[i], sizeof(Call)), "material flush args");
    Check(SameBytes(cache[0] + 0xca4, cache[1] + 0xca4, 0x80), "material sizes after flush");

    DestroyObject(cache[0], 0xd24);
    DestroyObject(cache[1], 0xd24);
}

void TestGenericCache(Api api[2], FakeMaterial& material)
{
    alignas(16) unsigned char cache[2][0x1000];
    std::memset(cache, 0, sizeof cache);
    api[0].genericCtor(cache[0]);
    api[1].genericCtor(cache[1]);

    FakeChunk chunks[6];
    for (int i = 0; i < 6; ++i) InitChunk(chunks[i], 0, 1);
    FakeMaterial material2;
    InitMaterial(material2);

    for (int i = 0; i < 6; ++i) {
        FakeMaterial& m = (i < 4) ? material : material2;
        api[0].genericAdd(cache[0], displayBuf[0], nullptr, &m, &chunks[i], nullptr, nullptr);
        api[1].genericAdd(cache[1], displayBuf[0], nullptr, &m, &chunks[i], nullptr, nullptr);
    }
    Check(*reinterpret_cast<int*>(cache[0] + 0x20) == *reinterpret_cast<int*>(cache[1] + 0x20), "generic constructed");
    Check(*reinterpret_cast<int*>(cache[0] + 0x2c) == *reinterpret_cast<int*>(cache[1] + 0x2c), "generic used");

    InitDisplay(0);
    InitDisplay(1);
    callCount = 0;
    api[0].genericEnd(cache[0], displayBuf[0], nullptr);
    const int n0 = callCount;
    Call c0[16];
    std::memcpy(c0, calls, sizeof(Call) * static_cast<size_t>(n0));
    callCount = 0;
    api[1].genericEnd(cache[1], displayBuf[0], nullptr);
    Check(n0 == callCount, "generic End call count");
    for (int i = 0; i < n0 && i < callCount; ++i) {
        bool same = c0[i].kind == calls[i].kind;
        if (same && c0[i].kind == 2) {
            // RenderNumChunks logs the arrays themselves; compare the element pointers logged in a[8..11]
            same = c0[i].a[0] == calls[i].a[0] && c0[i].a[1] == calls[i].a[1] && c0[i].a[2] == calls[i].a[2] &&
                   c0[i].a[6] == calls[i].a[6] && c0[i].a[7] == calls[i].a[7];
            for (int k = 8; k < 12 && same; ++k) same = c0[i].a[k] == calls[i].a[k];
        } else if (same) {
            same = SameBytes(&c0[i], &calls[i], sizeof(Call));
        }
        if (!same) {
            std::printf("  generic call %d kind %d ours:", i, c0[i].kind);
            for (void* p : c0[i].a) std::printf(" %p", p);
            std::printf("\n  orig:");
            for (void* p : calls[i].a) std::printf(" %p", p);
            std::printf("\n");
            break;
        }
    }

    DestroyObject(cache[0], 0x30);
    DestroyObject(cache[1], 0x30);
}

void TestTree(Api api[2])
{
    alignas(16) unsigned char cache[2][0x100];
    std::memset(cache, 0, sizeof cache);
    api[0].abCtor(cache[0], 1);
    api[1].abCtor(cache[1], 1);

    // reset builds the two sentinels of every bucket
    typedef void(__cdecl* ResetFn)(void*);
    ResetFn const reset[2] = {Proc<ResetFn>(0, "?Reset@AlphaBlendCache@Jet@@UAAXXZ"),
                              Proc<ResetFn>(1, "?Reset@AlphaBlendCache@Jet@@UAAXXZ")};
    reset[0](cache[0]);
    reset[1](cache[1]);

    void** const buckets[2] = {*reinterpret_cast<void***>(cache[0] + 0x28), *reinterpret_cast<void***>(cache[1] + 0x28)};
    Bucket* const bucket[2] = {reinterpret_cast<Bucket*>(buckets[0]), reinterpret_cast<Bucket*>(buckets[1])};

    Rec* records[2][kMaxRecs];
    std::memset(records, 0, sizeof records);
    for (int round = 0; round < 24; ++round) {
        const float distance = (round % 9 == 0) ? Bits(0x7fc00000u) : FRange(-100.0f, 100.0f);
        for (int s = 0; s < 2; ++s) {
            records[s][round] = api[s].abGetFree(cache[s], nullptr, nullptr, nullptr, nullptr, nullptr, 0.0f);
            records[s][round]->distance = distance;
            api[s].abInsert(bucket[s], records[s][round]);
        }
        const bool equal = TreeEqual(bucket[0]->head->right, bucket[1]->head->right, records[0], round + 1,
                                     bucket[0]->z, records[1], round + 1, bucket[1]->z);
        Check(equal, "tree after insert");
        if (!equal) {
            std::printf("  first mismatch round %d d=%08x ours:", round, FBits(distance));
            DumpNode(bucket[0]->head->right, records[0], round + 1, bucket[0]->z, 0);
            std::printf(" head L%d R%d\n", IndexOf(bucket[0]->head->left, records[0], round + 1, bucket[0]->z),
                        IndexOf(bucket[0]->head->right, records[0], round + 1, bucket[0]->z));
            std::printf("  orig:");
            DumpNode(bucket[1]->head->right, records[1], round + 1, bucket[1]->z, 0);
            std::printf(" head L%d R%d\n", IndexOf(bucket[1]->head->left, records[1], round + 1, bucket[1]->z),
                        IndexOf(bucket[1]->head->right, records[1], round + 1, bucket[1]->z));
            break;
        }
        Check(bucket[0]->numNodes == bucket[1]->numNodes, "tree node count");
    }

    DestroyObject(cache[0], 0x78);
    DestroyObject(cache[1], 0x78);
}

void TestAddAndEnd(Api api[2], FakeMaterial& material, FakeChunk& chunk)
{
    alignas(16) unsigned char cache[2][0x100];
    std::memset(cache, 0, sizeof cache);
    api[0].abCtor(cache[0], 1);
    api[1].abCtor(cache[1], 1);

    typedef void(__cdecl* SetRangeFn)(void*, float);
    SetRangeFn const setEverything[2] = {Proc<SetRangeFn>(0, "?SetSortEverythingRange@AlphaBlendCache@Jet@@UAAXM@Z"),
                                         Proc<SetRangeFn>(1, "?SetSortEverythingRange@AlphaBlendCache@Jet@@UAAXM@Z")};
    SetRangeFn const setPolys[2] = {Proc<SetRangeFn>(0, "?SetSortPolysInChunkRange@AlphaBlendCache@Jet@@QAAXM@Z"),
                                    Proc<SetRangeFn>(1, "?SetSortPolysInChunkRange@AlphaBlendCache@Jet@@QAAXM@Z")};
    InitDisplay(0);
    InitDisplay(1);
    for (int s = 0; s < 2; ++s) {
        setEverything[s](cache[s], 0.0f);
        setPolys[s](cache[s], 0.0f);
    }

    Matrix matrices[12];
    for (int i = 0; i < 12; ++i) {
        std::memset(&matrices[i], 0, sizeof matrices[i]);
        matrices[i].m[0] = matrices[i].m[5] = matrices[i].m[10] = 1.0f;
        matrices[i].m[12] = FRange(-100.0f, 100.0f);
        matrices[i].m[13] = FRange(-100.0f, 100.0f);
        matrices[i].m[14] = FRange(-100.0f, 100.0f);
    }
    InitChunk(chunk, 0, 4);

    for (int i = 0; i < 12; ++i) {
        api[0].abAdd(cache[0], displayBuf[0], nullptr, &material, &chunk, nullptr, &matrices[i]);
        api[1].abAdd(cache[1], displayBuf[1], nullptr, &material, &chunk, nullptr, &matrices[i]);
    }

    InitChunk(chunk, 0, 4);

    // compare the buckets' trees and records by walking each of the 40 buckets
    for (int b = 0; b < 40; ++b) {
        Bucket* const bucket0 = reinterpret_cast<Bucket*>(
            reinterpret_cast<char*>(*reinterpret_cast<void**>(cache[0] + 0x28)) + b * 0x10);
        Bucket* const bucket1 = reinterpret_cast<Bucket*>(
            reinterpret_cast<char*>(*reinterpret_cast<void**>(cache[1] + 0x28)) + b * 0x10);
        Rec* list0[kMaxRecs];
        Rec* list1[kMaxRecs];
        const int n0 = CollectTree(bucket0->head->right, bucket0->z, list0, 0, kMaxRecs);
        const int n1 = CollectTree(bucket1->head->right, bucket1->z, list1, 0, kMaxRecs);
        Check(n0 == n1, "Add bucket record count");
        CompareTreeShape(bucket0->head->right, bucket1->head->right, list0, n0, bucket0->z, list1, n1, bucket1->z,
                         "Add bucket shape", 0);
        if (n0 == n1) {
            for (int i = 0; i < n0; ++i) {
                Check(FBits(list0[i]->distance) == FBits(list1[i]->distance), "Add distance");
                if (FBits(list0[i]->distance) != FBits(list1[i]->distance))
                    std::printf("  add distance %08x vs %08x\n", FBits(list0[i]->distance),
                                FBits(list1[i]->distance));
                Check(list0[i]->numPrimitives == list1[i]->numPrimitives, "Add numPrimitives");
                Check(list0[i]->primitive == list1[i]->primitive, "Add primitive");
                Check(list0[i]->sortedFaces == nullptr && list1[i]->sortedFaces == nullptr, "Add sortedFaces null");
            }
        }
    }

    callCount = 0;
    api[0].abEnd(cache[0], displayBuf[0], nullptr);
    const int n0 = callCount;
    Call c0[80];
    std::memcpy(c0, calls, sizeof(Call) * static_cast<size_t>(n0 < 80 ? n0 : 80));
    unsigned char afterEnd[0x100];
    std::memcpy(afterEnd, stateBuf[0], sizeof afterEnd);
    callCount = 0;
    api[1].abEnd(cache[1], displayBuf[0], nullptr);
    Check(n0 == callCount, "AB End call count");
    for (int i = 0; i < n0 && i < callCount && i < 80; ++i)
        Check(SameBytes(&c0[i], &calls[i], sizeof(Call)), "AB End args");
    Check(SameBytes(afterEnd, stateBuf[0], sizeof afterEnd), "AB End display state");
    if (!SameBytes(afterEnd, stateBuf[0], sizeof afterEnd))
        for (int i = 0; i < 0x100; ++i)
            if (afterEnd[i] != stateBuf[0][i])
                std::printf("  AB state +%02x ours %02x orig %02x\n", i, afterEnd[i], stateBuf[0][i]);
    Check(*reinterpret_cast<int*>(cache[0] + 0x34) == *reinterpret_cast<int*>(cache[1] + 0x34), "AB End buckets");

    DestroyObject(cache[0], 0x78);
    DestroyObject(cache[1], 0x78);
}

void TestPrimitiveSort(Api api[2])
{
    FakeChunk chunk;
    for (unsigned type : {0u, 1u, 2u, 4u, 8u, 16u, 3u, 7u}) {
        InitChunk(chunk, type, 17);
        int a0 = 0, b0 = 0, a1 = 0, b1 = 0;
        float r0 = 0.0f, r1 = 0.0f;
        int p0 = -1, p1 = -1;
        const int n0 = api[0].abSetupPrimitiveSort(nullptr, &chunk, &a0, &b0, &r0, &p0);
        const int n1 = api[1].abSetupPrimitiveSort(nullptr, &chunk, &a1, &b1, &r1, &p1);
        Check(n0 == n1 && a0 == a1 && b0 == b1 && FBits(r0) == FBits(r1) && p0 == p1, "SetupPrimitiveSort");
    }
}

}  // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 2) {
        std::printf("usage: %s <dir>\n", argv[0]);
        return 2;
    }
    char path[2][MAX_PATH];
    std::snprintf(path[0], sizeof path[0], "%s\\Render3DCore_7c.dll", argv[1]);
    std::snprintf(path[1], sizeof path[1], "%s\\Render3DCore_7c_orig.dll", argv[1]);
    for (int i = 0; i < 2; ++i) {
        mod[i] = LoadLibraryA(path[i]);
        if (!mod[i]) {
            std::printf("cannot load %s\n", path[i]);
            return 2;
        }
    }

    Api api[2] = {Load(0), Load(1)};
    FakeMaterial material;
    InitMaterial(material);
    FakeChunk chunk;
    InitChunk(chunk, 0, 4);
    Matrix matrix;
    std::memset(&matrix, 0, sizeof matrix);
    matrix.m[0] = matrix.m[5] = matrix.m[10] = 1.0f;

    std::printf("lifetimes\n");
    TestLifetimes(api);
    std::printf("mask\n");
    TestMaskAndUnsorted(api, material, chunk, matrix);
    std::printf("material\n");
    TestMaterialCache(api, material);
    std::printf("generic\n");
    TestGenericCache(api, material);
    std::printf("tree\n");
    TestTree(api);
    std::printf("add\n");
    TestAddAndEnd(api, material, chunk);
    std::printf("sort\n");
    TestPrimitiveSort(api);

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
