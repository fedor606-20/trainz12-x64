// U4 wave 4: cache lifetimes and the managed statics. The constructors, destructors, vtables and vbtables are the
// original's code shape: every class derives from RenderingSystem's RenderingCache with a virtual ReferenceCount
// base, so the compiler emits the same two vftables, the vbtable, the scalar deleting destructor and the vbase
// destructor. The CacheID PStrings and the singletons are the module's data exports.
#include <jet/Render3DCoreCaches.h>

#include <new>

namespace Jet {

PString AlphaBlendCache::AlphaBlendCacheID("AlphaBlendCache");
PString AlphaMaskCache::AlphaMaskCacheID("AlphaMaskCache");
PString MaterialChunkCache::MaterialChunkCacheID("ChunkCache");
PString GenericMaterialChunkCache::GenericMaterialChunkCacheID("GenericChunkCache");
PString ParticleFXCache::ParticleFXCacheID("ParticleFXCache");
PString ProjectorCache::ProjectorCacheID("ProjectorCache");
PString UnsortedAlphaBlendCache::UnsortedAlphaBlendCacheID("UnsortedAlphaBlendCache");

AlphaBlendCache* AlphaBlendCache::singleton = nullptr;
AlphaMaskCache* AlphaMaskCache::singleton = nullptr;
MaterialChunkCache* MaterialChunkCache::singleton = nullptr;
UnsortedAlphaBlendCache* UnsortedAlphaBlendCache::singleton = nullptr;
bool UnsortedAlphaBlendCache::m_ceebtrainz_delayed_alpha_render = false;

// 0x10001400: default ctor. 40 initial buckets (the vector helper appends bucketStep of them), 200/80 ranges and
// the two 10000-index blocks. savedBlendSrc/Dst stay uninitialised exactly like the original.
AlphaBlendCache::AlphaBlendCache() : RenderingCache(AlphaBlendCacheID, 50.0f)
{
    bucketStep = 40;
    numBuckets = 40;
    maxBucketRange = 200.0f;
    sortPolysInChunkRange = 80.0f;
    overlappingChunks = true;
    blendSrc = static_cast<Src::Factor>(4);
    blendDst = static_cast<Dst::Factor>(5);
    singleton = this;
    headIndexAllocator = static_cast<IndexCacheRecord*>(Alloc::New(sizeof(IndexCacheRecord)));
    if (headIndexAllocator) ::new (headIndexAllocator) IndexCacheRecord(10000);
    currentIndexAllocator = headIndexAllocator;
    RecalculateBucketParametrs(true);
}

// 0x10001570: the partial constructor (no index allocator, no bucket recalculation). The default member
// initialisers already give the original's values; the hidden virtual-base flag is compiler-generated.
AlphaBlendCache::AlphaBlendCache(bool) : RenderingCache(AlphaBlendCacheID, 50.0f)
{
    singleton = this;
}

// 0x10001660
AlphaBlendCache::~AlphaBlendCache()
{
    if (singleton == this) singleton = nullptr;

    if (sortedFaces) Alloc::Delete(sortedFaces);
    sortedFaces = nullptr;

    for (int i = 0; i < bucketCount; ++i) buckets[i].~CacheBucket();
    bucketCount = 0;
    if (buckets) Alloc::Delete(buckets);
    buckets = nullptr;

    while (freelist) {
        CacheRecord* const record = freelist;
        freelist = record->right;
        if (record->userAllocated) {
            if (record->sortedFaces) Alloc::Delete(record->sortedFaces);
            record->sortedFaces = nullptr;
        }
        Alloc::Delete(record);
    }

    while (headIndexAllocator) {
        IndexCacheRecord* const record = headIndexAllocator;
        headIndexAllocator = record->next;
        if (record->array) Alloc::Delete(record->array);
        record->array = nullptr;
        Alloc::Delete(record);
    }
    currentIndexAllocator = nullptr;
}

// 0x10002f90
AlphaMaskCache::AlphaMaskCache() : RenderingCache(AlphaMaskCacheID, 30.0f)
{
    singleton = this;
}

// 0x10003060
AlphaMaskCache::~AlphaMaskCache()
{
    if (singleton == this) singleton = nullptr;
    void*** const lists[4] = {&data3, &data2, &data1, &data0};
    for (void*** const list : lists) {
        if (*list) Alloc::Delete(*list);
        *list = nullptr;
    }
    count3 = count2 = count1 = count0 = 0;
}

// 0x10072600
UnsortedAlphaBlendCache::UnsortedAlphaBlendCache() : RenderingCache(UnsortedAlphaBlendCacheID, 35.0f)
{
    singleton = this;
}

// 0x100726d0
UnsortedAlphaBlendCache::~UnsortedAlphaBlendCache()
{
    if (singleton == this) singleton = nullptr;
    void*** const lists[4] = {&data3, &data2, &data1, &data0};
    for (void*** const list : lists) {
        if (*list) Alloc::Delete(*list);
        *list = nullptr;
    }
    if (data4) Alloc::Delete(data4);
    data4 = nullptr;
    count4 = count3 = count2 = count1 = count0 = 0;
}

void __cdecl UnsortedAlphaBlendCache::SetTrainzDelayedRender(bool on)
{
    m_ceebtrainz_delayed_alpha_render = on;
}

// 0x1002b540
MaterialChunkCache::MaterialChunkCache() : RenderingCache(MaterialChunkCacheID, 10.0f)
{
    pipeCount = 0;
    singleton = this;
}

// 0x1002b600
MaterialChunkCache::MaterialChunkCache(const PString& name, float priority) : RenderingCache(name, priority)
{
    pipeCount = 0;
    singleton = this;
}

// 0x1002b6c0
MaterialChunkCache::~MaterialChunkCache()
{
    if (singleton == this) singleton = nullptr;
}

// 0x10025b80
GenericMaterialChunkCache::GenericMaterialChunkCache(const PString& name, float priority)
    : RenderingCache(name, priority)
{
    constructed = 0;
    capacity = 0;
    entries = nullptr;
    used = 0;
}

// 0x10026060: release the three per-entry lists of every constructed entry and reset the count
void __cdecl GenericMaterialChunkCache::FreeEntries()
{
    for (int i = 0; i < constructed; ++i) {
        Entry& entry = entries[i];
        if (entry.data2) Alloc::Delete(entry.data2);
        entry.data2 = nullptr;
        entry.count2 = 0;
        if (entry.data1) Alloc::Delete(entry.data1);
        entry.data1 = nullptr;
        entry.count1 = 0;
        if (entry.data0) Alloc::Delete(entry.data0);
        entry.data0 = nullptr;
        entry.count0 = 0;
    }
    constructed = 0;
}

// 0x10025a50
GenericMaterialChunkCache::~GenericMaterialChunkCache()
{
    FreeEntries();
    if (entries) Alloc::Delete(entries);
    entries = nullptr;
}

// 0x10052ed0
ParticleFXCache::ParticleFXCache() : RenderingCache(ParticleFXCacheID, 70.0f) {}

// 0x10052db0
ParticleFXCache::~ParticleFXCache()
{
    count = 0;
    if (data) Alloc::Delete(data);
    data = nullptr;
}

// 0x10058a10: this cache does not render scene geometry
ProjectorCache::ProjectorCache() : RenderingCache(ProjectorCacheID, 60.0f)
{
    blocks = nullptr;
    rendersGeometry = false;
}

// 0x10058930
ProjectorCache::~ProjectorCache()
{
    Reset();
}

}  // namespace Jet

// 0x10025a80: the default constructor closure has no C++ spelling; the .def aliases the original name to this
// __fastcall stub so 'this' arrives in ECX like the __thiscall ctor.
extern "C" void __fastcall U4GenericMaterialChunkCacheDefaultCtor(void* self, void*)
{
    ::new (self) Jet::GenericMaterialChunkCache(Jet::GenericMaterialChunkCache::GenericMaterialChunkCacheID, 10.0f);
}
