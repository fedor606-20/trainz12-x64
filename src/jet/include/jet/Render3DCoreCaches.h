// Render3DCore_7c unit U4 (notes/Render3DCore.md): the rendering caches. Layouts follow the TS12 assembly
// (addresses per member in Render3DCore_7c/AlphaCaches.cpp, MaterialChunkCaches.cpp, ParticleFXCache.cpp,
// ProjectorCache.cpp).
//
// Every cache derives from RenderSystem's RenderingCache (IListNode + a virtual ReferenceCount at the end of the
// complete object). Ordinary members are __cdecl (this is pushed on the stack, as in the original); IsEmpty is
// __thiscall because RenderingCache has no such virtual and the original's mangling says so. The virtual slots are
// declared in the original's order (see the vftable dumps in the report). Singleton and vftable data stay the
// original's, so constructors that publish a singleton and all destructors keep forwarding.
#pragma once

#include <jet/FastDynamicArray.h>
#include <jet/Geometry.h>
#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/RenderSystem.h>
#include <jet/RenderTypes.h>

namespace Jet {

class Display;
class GeometryChunk;
class Material;
class Matrix4;
class ParticleFXRenderItem;
class Projector;
class ViewPort;

// FastDynamicArray<DisplayLight *, 13> as the export mangling spells it.
using DisplayLightPtrList = FastDynamicArray<DisplayLight*, 13, Less<DisplayLight*, DisplayLight*>>;

// 0x80 bytes with the virtual ReferenceCount at +0x78; all of the original's buckets are 0x10-byte CacheBuckets
// reached through (bucketIndex + i) * 0x10 + buckets. The blend functions are 4 = SRC_SRC_ALPHA and 5 =
// DST_ONE_MINUS_SRC_ALPHA.
class AlphaBlendCache : public RenderingCache {
public:
    struct CacheRecord {  // 0x34, a red-black tree node and a free-list node at the same time (right = next free)
        GeometryChunk* chunk;              // +0x00
        const Material* material;          // +0x04
        const DisplayLightPtrList* lights; // +0x08
        const Matrix4* modelView;          // +0x0c
        unsigned short* sortedFaces;       // +0x10
        int numIndices;                    // +0x14
        int numPrimitives;                 // +0x18
        DPrimitive primitive;              // +0x1c
        float distance;                    // +0x20
        CacheRecord* left;                 // +0x24
        CacheRecord* right;                // +0x28
        CacheRecord* parent;               // +0x2c
        bool red : 1;                      // +0x30
        bool userAllocated : 1;

        bool __cdecl operator<(const CacheRecord& other) { return distance < other.distance; }
    };
    struct CacheBucket {  // 0x10; z is the sentinel every leaf points at
        CacheBucket();
        ~CacheBucket();
        CacheBucket& operator=(const CacheBucket& o);

        CacheRecord* __cdecl GetHead();
        CacheRecord* __cdecl GetNextRecord(CacheRecord* record);
        CacheRecord* __cdecl EndOfTree(CacheRecord* record);
        CacheRecord* __cdecl Insert(CacheRecord* record);
        void __cdecl SplitTree(CacheRecord* root, CacheRecord* parent, CacheRecord* node, CacheRecord* grand,
                               CacheRecord* great);
        CacheRecord* __cdecl RotateTree(CacheRecord* parent, CacheRecord* node);
        void __cdecl Reset(AlphaBlendCache* cache);

        int numNodes = 0;                            // +0x00
        CacheRecord* head = nullptr;                 // +0x04
        CacheRecord* z = nullptr;                    // +0x08
        AlphaBlendCache* alphaBlendCache = nullptr;  // +0x0c
    };

    struct SortFace {  // 8
        int face;        // +0
        float distance;  // +4
    };
    struct IndexCacheRecord {  // 0x14; a 10000-index block per record
        IndexCacheRecord(int count);
        void __cdecl Reset();

        int freeData;                  // +0x00
        int size;                      // +0x04
        unsigned short* array;         // +0x08
        unsigned short* nextFree;      // +0x0c
        IndexCacheRecord* next;        // +0x10
    };

    AlphaBlendCache();
    ~AlphaBlendCache() override;

protected:
    AlphaBlendCache(bool);  // 0x1570: the partial constructor the original's derived caches use

public:
    virtual bool __thiscall IsEmpty() const;                                        // slot 8
    virtual void __cdecl Add(Display*, ViewPort*, const Material*, GeometryChunk*,
                             const DisplayLightPtrList*, const Matrix4*);           // slot 9
    virtual void __cdecl SetSortEverythingRange(float range);                       // slot 10
    virtual void __cdecl SetMaxBuckets(int count);                                  // slot 11

    int __cdecl GetMaxBuckets() const;
    void __cdecl SetBlendFunction(Src::Factor src, Dst::Factor dst);
    void __cdecl GetBlendFunction(Src::Factor& src, Dst::Factor& dst) const;
    void __cdecl Reset();
    void __cdecl Begin(Display* display, ViewPort* view);
    void __cdecl End(Display* display, ViewPort* view);
    void __cdecl Enter(Display* display, ViewPort* view, bool b);
    void __cdecl Leave(Display* display, ViewPort* view, bool b);
    CacheRecord* __cdecl GetFreeCacheRecord(const Material* material, GeometryChunk* chunk,
                                            const DisplayLightPtrList* lights, const Matrix4* modelView,
                                            unsigned short* sortedFaces, float distance);
    void __cdecl AddToFreeList(CacheRecord* record);
    float __cdecl GetSortEverythingRange();
    void __cdecl SetSortPolysInChunkRange(float range);
    float __cdecl GetSortPolysInChunkRange();
    void __cdecl SetMaxBucketRange(float range);
    float __cdecl GetMaxBucketRange();

protected:
    virtual int(__cdecl* __cdecl GetSortFunction())(const void*, const void*);      // slot 12
    virtual void __cdecl RecalculateBucketParametrs(bool initial);                  // slot 13
    virtual void __cdecl SetupDisplayState(Display* display);                       // slot 14
    virtual void __cdecl CleanupDisplayState(Display* display);                     // slot 15

    unsigned short* __cdecl AllocateIndexArray(int count);
    static int __cdecl CompareSortFaces(const void* a, const void* b);
    void __cdecl SortChunk(CacheRecord& record);
    void __cdecl SortChunkIntoBuckets(const Material*, GeometryChunk*, const DisplayLightPtrList*,
                                      const Matrix4*);
    int __cdecl SetupPrimitiveSort(GeometryChunk*, int&, int&, float&, DPrimitive&);
    void __cdecl ResizeBuckets(int count);
    void __cdecl ResetBuckets();

private:
    const AlphaBlendCache& __cdecl operator=(const AlphaBlendCache& o);

public:
    static AlphaBlendCache* singleton;
    static PString AlphaBlendCacheID;

    int bucketCount = 0;                    // +0x20 constructed buckets
    int bucketCapacity = 0;                 // +0x24
    CacheBucket* buckets = nullptr;         // +0x28
    int bucketIndex = 0;                    // +0x2c added to the element index by Reset/End/Add
    int bucketStep = 0;                     // +0x30 (40)
    int numBuckets = 0;                     // +0x34 GetMaxBuckets
    float maxBucketRange = 0.0f;            // +0x38
    float bucketMultiplyer = 0.0f;          // +0x3c numBuckets / maxBucketRange
    float sortEverythingRange = 0.0f;       // +0x40
    float sortPolysInChunkRange = 0.0f;     // +0x44
    int sortEverythingId;                   // +0x48
    int sortPolysId;                        // +0x4c
    bool overlappingChunks = true;          // +0x50
    bool inFrame = false;                   // +0x51
    CacheRecord* freelist = nullptr;        // +0x54
    SortFace* sortedFaces = nullptr;        // +0x58
    int sortedFaceSize = 0;                 // +0x5c
    Src::Factor blendSrc = static_cast<Src::Factor>(4);  // +0x60
    Dst::Factor blendDst = static_cast<Dst::Factor>(5);  // +0x64
    int savedBlendSrc;                      // +0x68 Display state saved by SetupDisplayState
    int savedBlendDst;                      // +0x6c
    IndexCacheRecord* currentIndexAllocator = nullptr;  // +0x70
    IndexCacheRecord* headIndexAllocator = nullptr;     // +0x74
};
JET_LAYOUT32(sizeof(AlphaBlendCache::CacheRecord) == 0x34);
JET_LAYOUT32(sizeof(AlphaBlendCache::CacheBucket) == 0x10);
JET_LAYOUT32(sizeof(AlphaBlendCache) == 0x80);

// 0x58 bytes, four parallel pointer arrays (chunk, material, modelView, lights), counts at +0x20/+0x2c/+0x38/+0x44.
class AlphaMaskCache : public RenderingCache {
public:
    AlphaMaskCache();
    ~AlphaMaskCache() override;

    virtual bool __thiscall IsEmpty() const;                                   // slot 8
    virtual void __cdecl Add(const Material*, GeometryChunk*, const DisplayLightPtrList*, const Matrix4*);  // 9
    void __cdecl Reset();
    void __cdecl Begin(Display* display, ViewPort* view);
    void __cdecl End(Display* display, ViewPort* view);

    static AlphaMaskCache* singleton;
    static PString AlphaMaskCacheID;

    int count0 = 0;             // +0x20
    int capacity0 = 0;          // +0x24
    void** data0 = nullptr;     // +0x28 chunk
    int count1 = 0;             // +0x2c
    int capacity1 = 0;          // +0x30
    void** data1 = nullptr;     // +0x34 material
    int count2 = 0;             // +0x38
    int capacity2 = 0;          // +0x3c
    void** data2 = nullptr;     // +0x40 modelView
    int count3 = 0;             // +0x44
    int capacity3 = 0;          // +0x48
    void** data3 = nullptr;     // +0x4c lights
};
JET_LAYOUT32(sizeof(AlphaMaskCache) == 0x58);

// 0x64 bytes, five parallel arrays: chunk, material, modelView, lights, distances (floats).
class UnsortedAlphaBlendCache : public RenderingCache {
public:
    UnsortedAlphaBlendCache();
    ~UnsortedAlphaBlendCache() override;

    virtual bool __thiscall IsEmpty() const;                                   // slot 8
    virtual void __cdecl Add(const Material*, GeometryChunk*, const DisplayLightPtrList*, const Matrix4*,
                             float distance);                                  // slot 9
    void __cdecl Reset();
    void __cdecl Begin(Display* display, ViewPort* view);
    void __cdecl End(Display* display, ViewPort* view);
    static void __cdecl SetTrainzDelayedRender(bool on);

    static UnsortedAlphaBlendCache* singleton;
    static PString UnsortedAlphaBlendCacheID;

protected:
    static bool m_ceebtrainz_delayed_alpha_render;

public:

    int count0 = 0;             // +0x20
    int capacity0 = 0;          // +0x24
    void** data0 = nullptr;     // +0x28 chunk
    int count1 = 0;             // +0x2c
    int capacity1 = 0;          // +0x30
    void** data1 = nullptr;     // +0x34 material
    int count2 = 0;             // +0x38
    int capacity2 = 0;          // +0x3c
    void** data2 = nullptr;     // +0x40 modelView
    int count3 = 0;             // +0x44
    int capacity3 = 0;          // +0x48
    void** data3 = nullptr;     // +0x4c lights
    int count4 = 0;             // +0x50
    int capacity4 = 0;          // +0x54
    float* data4 = nullptr;     // +0x58 distances
};
JET_LAYOUT32(sizeof(UnsortedAlphaBlendCache) == 0x64);

// 0xd2c bytes, 32 pipes: chunk pointers at +0xc20, per-pipe counts at +0xca4, and 8 lights / model views / the
// third pointer list per pipe in 0x20-byte blocks at +0x20 / +0x420 / +0x820.
class MaterialChunkCache : public RenderingCache {
public:
    MaterialChunkCache();
    ~MaterialChunkCache() override;

    virtual bool __thiscall IsEmpty() const;                                    // slot 8
    void __cdecl Reset();
    void __cdecl Begin(Display* display, ViewPort* view);
    void __cdecl End(Display* display, ViewPort* view);
    void __cdecl Enter(Display* display, ViewPort* view, bool b);
    void __cdecl Leave(Display* display, ViewPort* view, bool b);
    void __cdecl Add(Display*, ViewPort*, const Material*, GeometryChunk*, const DisplayLightPtrList*,
                     const Matrix4*);

protected:
    virtual void __cdecl FlushPipe(int pipe, Display* display, ViewPort* view);  // slot 9

public:

    static MaterialChunkCache* singleton;
    static PString MaterialChunkCacheID;

protected:
    MaterialChunkCache(const PString& name, float priority);

public:
    void* pipeChunks[32 * 8];       // +0x20 blocks of 8 pointers: geometry chunks
    void* pipeLights[32 * 8];       // +0x420
    void* pipeModelViews[32 * 8];   // +0x820
    Material* pipeMaterials[32];    // +0xc20
    int pipeCount = 0;              // +0xca0
    int pipeSizes[32];              // +0xca4
};
JET_LAYOUT32(sizeof(MaterialChunkCache) == 0xd2c);

// 0x38 bytes: a vector of 0x28-byte per-chunk pointer lists.
class GenericMaterialChunkCache : public RenderingCache {
public:
    explicit GenericMaterialChunkCache(const PString& name, float priority);
    ~GenericMaterialChunkCache() override;

    struct Entry {  // 0x28
        Material* material;     // +0x00
        int count0 = 0;         // +0x04
        int capacity0 = 0;      // +0x08
        void** data0 = nullptr; // +0x0c chunks
        int count1 = 0;         // +0x10
        int capacity1 = 0;      // +0x14
        void** data1 = nullptr; // +0x18 lights
        int count2 = 0;         // +0x1c
        int capacity2 = 0;      // +0x20
        void** data2 = nullptr; // +0x24 modelView
    };

    void __cdecl Reset();
    void __cdecl Begin(Display* display, ViewPort* view);
    void __cdecl End(Display* display, ViewPort* view);
    void __cdecl Enter(Display* display, ViewPort* view, bool b);
    void __cdecl Leave(Display* display, ViewPort* view, bool b);
    void __cdecl Add(Display*, ViewPort*, const Material*, GeometryChunk*, const DisplayLightPtrList*,
                     const Matrix4*);

    static PString GenericMaterialChunkCacheID;

    int constructed = 0;  // +0x20 entries built
    int capacity = 0;     // +0x24 entries allocated
    Entry* entries = nullptr;  // +0x28
    int used = 0;         // +0x2c live entries

private:
    void __cdecl FreeEntries();
};
JET_LAYOUT32(sizeof(GenericMaterialChunkCache::Entry) == 0x28);
JET_LAYOUT32(sizeof(GenericMaterialChunkCache) == 0x38);

// 0x34 bytes: {Matrix4*, ParticleFXRenderItem*} pairs.
class ParticleFXCache : public RenderingCache {
public:
    ParticleFXCache();
    ~ParticleFXCache() override;

    virtual void __cdecl Add(const ParticleFXRenderItem* item, const Matrix4* matrix);  // slot 8
    void __cdecl Reset();
    void __cdecl Begin(Display* display, ViewPort* view);
    void __cdecl End(Display* display, ViewPort* view);

    static PString ParticleFXCacheID;

    int count = 0;          // +0x20
    int capacity = 0;       // +0x24
    void** data = nullptr;  // +0x28
};
JET_LAYOUT32(sizeof(ParticleFXCache) == 0x34);

// 0x2c bytes: a ring of 0x40-byte blocks, each {next, prev, count, projectors[13]}.
class ProjectorCache : public RenderingCache {
public:
    ProjectorCache();
    ~ProjectorCache() override;

    struct Block {  // 0x40
        Block* next;            // +0x00
        Block* prev;            // +0x04
        int count;              // +0x08
        Projector* items[13];   // +0x0c
    };

    void __cdecl Reset();
    void __cdecl Begin(Display* display, ViewPort* view);
    void __cdecl End(Display* display, ViewPort* view);
    void __cdecl Add(Projector* projector);

    static PString ProjectorCacheID;

    Block* blocks = nullptr;  // +0x20
};
JET_LAYOUT32(sizeof(ProjectorCache::Block) == 0x40);
JET_LAYOUT32(sizeof(ProjectorCache) == 0x2c);

}  // namespace Jet
