// Unit U4 of Render3DCore_7c: AlphaBlendCache (and its CacheBucket red-black tree), AlphaMaskCache and
// UnsortedAlphaBlendCache. Transcribed from the TS12 assembly; addresses per member in the comments.
//
// The three classes share the 4/5 parallel pointer lists whose growth helper is at 0x100030f0 (round the requested
// count up to 128, copy the old items, free the old block). The tree comparisons use the original's COMISS + JBE:
// The tree rebalancer (Insert/SplitTree/RotateTree) is forwarded; only the walks and Reset are local.
#include <jet/Render3DCoreCaches.h>

#include <cstddef>
#include <cstring>
#include <new>

namespace Jet {

namespace {

typedef AlphaBlendCache::CacheRecord CacheRecord;
typedef AlphaBlendCache::CacheBucket CacheBucket;

// 0x100030f0: grow a {count, capacity, data} list to hold need items
void GrowList(int* list, unsigned need)
{
    if (need <= static_cast<unsigned>(list[1])) return;
    const unsigned capacity = (need + 0x7f) & ~0x7fu;
    void** const data = static_cast<void**>(Alloc::New(capacity * 4));
    if (list[2]) {
        std::memcpy(data, reinterpret_cast<void*>(list[2]), static_cast<size_t>(list[0]) * 4);
        Alloc::Delete(reinterpret_cast<void*>(list[2]));
    }
    list[1] = static_cast<int>(capacity);
    list[2] = reinterpret_cast<int>(data);
}

// 0x10053050: the same for 8-byte pairs
void GrowPairs(int* list, unsigned need)
{
    if (need <= static_cast<unsigned>(list[1])) return;
    const unsigned capacity = (need + 0x7f) & ~0x7fu;
    void** const data = static_cast<void**>(Alloc::New(capacity * 8));
    if (list[2]) {
        std::memcpy(data, reinterpret_cast<void*>(list[2]), static_cast<size_t>(list[0]) * 8);
        Alloc::Delete(reinterpret_cast<void*>(list[2]));
    }
    list[1] = static_cast<int>(capacity);
    list[2] = reinterpret_cast<int>(data);
}

// a plain pointer list append: grow to count + 1, store, count++
inline void AppendPointer(int* list, void* value)
{
    GrowList(list, static_cast<unsigned>(list[0]) + 1);
    if (list[2]) reinterpret_cast<void**>(list[2])[list[0]] = value;
    ++list[0];
}

inline void AppendFloat(int* list, float value)
{
    GrowList(list, static_cast<unsigned>(list[0]) + 1);
    if (list[2]) reinterpret_cast<float*>(list[2])[list[0]] = value;
    ++list[0];
}

// 0x10001090
}

AlphaBlendCache::CacheBucket& __thiscall AlphaBlendCache::CacheBucket::operator=(const CacheBucket& o)
{
    numNodes = o.numNodes;
    head = o.head;
    z = o.z;
    alphaBlendCache = o.alphaBlendCache;
    return *this;
}

// 0x10001140
int __cdecl AlphaBlendCache::GetMaxBuckets() const
{
    return numBuckets;
}

// 0x10001150
void __cdecl AlphaBlendCache::SetBlendFunction(Src::Factor src, Dst::Factor dst)
{
    blendSrc = src;
    blendDst = dst;
}

// 0x10001170
void __cdecl AlphaBlendCache::GetBlendFunction(Src::Factor& src, Dst::Factor& dst) const
{
    src = blendSrc;
    dst = blendDst;
}

// 0x10001350
CacheRecord* __cdecl AlphaBlendCache::CacheBucket::EndOfTree(CacheRecord* record)
{
    CacheRecord* node = record;
    while (node->right != z) node = node->right;
    return node;
}

// 0x10001370
CacheRecord* __cdecl AlphaBlendCache::CacheBucket::GetNextRecord(CacheRecord* record)
{
    CacheRecord* node = record;
    if (node == head) {
        if (head->right != z)
            node = head->right;
        else if (head->left != z)
            node = head->left;
        else
            return nullptr;
        return EndOfTree(node);
    }
    if (node->left != z) return EndOfTree(node->left);
    CacheRecord* parent = node->parent;
    while (parent && parent->right != node) {
        node = parent;
        parent = parent->parent;
    }
    if (parent == head) {
        if (head->left == z) return nullptr;
        return EndOfTree(head->left);
    }
    return parent;
}

// 0x1006970
CacheRecord* __cdecl AlphaBlendCache::CacheBucket::GetHead()
{
    return head;
}

// 0x100017a0
bool __thiscall AlphaBlendCache::IsEmpty() const
{
    return !inFrame;
}

// 0x100017b0
void __cdecl AlphaBlendCache::Reset()
{
    for (int i = 0; i < numBuckets; ++i)
        reinterpret_cast<CacheBucket*>(reinterpret_cast<char*>(buckets) + (bucketIndex + i) * 0x10)->Reset(this);

    IndexCacheRecord* record = headIndexAllocator;
    currentIndexAllocator = record;
    for (; record; record = record->next) {
        record->freeData = record->size;
        record->nextFree = record->array;
    }
}

// 0x10001800
void __cdecl AlphaBlendCache::Begin(Display*, ViewPort*)
{
    inFrame = true;
}

// 0x10001a60: save the display's alpha blend factors, then alpha test off / alpha blend on / depth write off
void __cdecl AlphaBlendCache::SetupDisplayState(Display* display)
{
    const unsigned char* const state = *reinterpret_cast<unsigned char* const*>(reinterpret_cast<char*>(display) + 0x54);
    savedBlendSrc = *reinterpret_cast<const int*>(state + 0x68);
    savedBlendDst = *reinterpret_cast<const int*>(state + 0x6c);
    display->SetAlphaTestMode(false);
    display->SetAlphaBlendMode(true);
    display->SetDepthWriteMode(false);
}

// 0x10001aa0
void __cdecl AlphaBlendCache::CleanupDisplayState(Display* display)
{
    display->SetAlphaBlendFunction(static_cast<Src::Factor>(savedBlendSrc), static_cast<Dst::Factor>(savedBlendDst));
    display->SetAlphaBlendMode(false);
    display->SetAlphaTestMode(false);
    display->SetDepthWriteMode(true);
}

// 0x10001c20
int __cdecl AlphaBlendCache::CompareSortFaces(const void* a, const void* b)
{
    const SortFace* const fa = static_cast<const SortFace*>(a);
    const SortFace* const fb = static_cast<const SortFace*>(b);
    if (fa->distance < fb->distance) return 1;
    if (fb->distance < fa->distance) return -1;
    return 0;
}

// 0x10001c50
int(__cdecl* __cdecl AlphaBlendCache::GetSortFunction())(const void*, const void*)
{
    return CompareSortFaces;
}

// 0x10002430
CacheRecord* __cdecl AlphaBlendCache::GetFreeCacheRecord(const Material* material, GeometryChunk* chunk,
                                                         const DisplayLightPtrList* lights,
                                                         const Matrix4* modelView, unsigned short* sortedFaces,
                                                         float distance)
{
    if (!freelist) {
        CacheRecord* const record = static_cast<CacheRecord*>(Alloc::New(0x34));
        if (record) {
            record->red = false;
            record->chunk = nullptr;
            record->material = nullptr;
            record->lights = nullptr;
            record->modelView = nullptr;
            record->sortedFaces = nullptr;
            record->distance = 0.0f;
            record->left = nullptr;
            record->right = nullptr;
            record->parent = nullptr;
        }
        if (record && (record->red || record->userAllocated)) {
            if (record->sortedFaces) Alloc::Delete(record->sortedFaces);
            record->sortedFaces = nullptr;
        }
        record->right = freelist;
        freelist = record;
    }
    CacheRecord* const record = freelist;
    freelist = record->right;
    record->red = false;
    record->userAllocated = false;
    record->chunk = chunk;
    record->material = material;
    record->lights = lights;
    record->modelView = modelView;
    record->sortedFaces = sortedFaces;
    record->distance = distance;
    record->left = nullptr;
    record->right = nullptr;
    record->parent = nullptr;
    return record;
}

// 0x100024f0
void __cdecl AlphaBlendCache::AddToFreeList(CacheRecord* record)
{
    if (record->userAllocated) {
        if (record->sortedFaces) Alloc::Delete(record->sortedFaces);
        record->sortedFaces = nullptr;
    }
    record->right = freelist;
    freelist = record;
}

// 0x10002530
unsigned short* __cdecl AlphaBlendCache::AllocateIndexArray(int count)
{
    if (currentIndexAllocator->freeData < count) {
        if (!currentIndexAllocator->next) {
            IndexCacheRecord* const record = static_cast<IndexCacheRecord*>(Alloc::New(0x14));
            if (record) ::new (record) IndexCacheRecord(10000);
            currentIndexAllocator->next = record;
        }
        currentIndexAllocator = currentIndexAllocator->next;
    }
    unsigned short* const data = currentIndexAllocator->nextFree;
    currentIndexAllocator->freeData -= count;
    currentIndexAllocator->nextFree += count;
    return data;
}

// 0x100025d0
void __cdecl AlphaBlendCache::SetSortEverythingRange(float range)
{
    const float scaled = bucketMultiplyer * range;
    sortEverythingRange = range;
    sortEverythingId = static_cast<int>(scaled);
}

// 0x100025f0
float __cdecl AlphaBlendCache::GetSortEverythingRange()
{
    return sortEverythingRange;
}

// 0x10002600
void __cdecl AlphaBlendCache::SetSortPolysInChunkRange(float range)
{
    const float scaled = bucketMultiplyer * range;
    sortPolysInChunkRange = range;
    sortPolysId = static_cast<int>(scaled);
}

// 0x10002620
void __cdecl AlphaBlendCache::SetMaxBucketRange(float range)
{
    const float scale = static_cast<float>(numBuckets) / range;
    maxBucketRange = range;
    bucketMultiplyer = scale;
    sortEverythingId = static_cast<int>(sortEverythingRange * scale);
    sortPolysId = static_cast<int>(sortPolysInChunkRange * scale);
}

// 0x10002660
float __cdecl AlphaBlendCache::GetMaxBucketRange()
{
    return maxBucketRange;
}

// 0x1000a970
float __cdecl AlphaBlendCache::GetSortPolysInChunkRange()
{
    return sortPolysInChunkRange;
}

// 0x10002670: store the count, then RecalculateBucketParametrs(false) through the vtable (slot 13)
void __cdecl AlphaBlendCache::SetMaxBuckets(int count)
{
    numBuckets = count;
    RecalculateBucketParametrs(false);
}

// 0x10002710
AlphaBlendCache::CacheBucket::CacheBucket()
{
    numNodes = 0;
    head = nullptr;
    z = nullptr;
    alphaBlendCache = nullptr;
}

// 0x10002720
AlphaBlendCache::CacheBucket::~CacheBucket()
{
    if (alphaBlendCache) {
        Reset(alphaBlendCache);
        if (head) alphaBlendCache->AddToFreeList(head);
        if (z) alphaBlendCache->AddToFreeList(z);
    }
}

// 0x100027a0: release every record of the tree, then build the two sentinels again
void __cdecl AlphaBlendCache::CacheBucket::Reset(AlphaBlendCache* cache)
{
    alphaBlendCache = cache;
    CacheRecord* node = head;
    while (node) {
        CacheRecord* next = node->right;
        if (next == z) {
            next = node->left;
            if (next == z) {
                CacheRecord* const parent = node->parent;
                if (parent) {
                    if (parent->right == node)
                        parent->right = z;
                    else
                        parent->left = z;
                }
                if (node->userAllocated) {
                    if (node->sortedFaces) Alloc::Delete(node->sortedFaces);
                    node->sortedFaces = nullptr;
                }
                node->right = cache->freelist;
                cache->freelist = node;
                next = parent;
            }
        }
        node = next;
    }
    if (!z) z = cache->GetFreeCacheRecord(nullptr, nullptr, nullptr, nullptr, nullptr, 0.0f);
    z->left = z;
    z->right = z;
    head = cache->GetFreeCacheRecord(nullptr, nullptr, nullptr, nullptr, nullptr, 0.0f);
    head->right = z;
    head->left = z;
    numNodes = 0;
}

// 0x10001c10 style: the index cache record constructor (0x10001020) is 10000 indices
AlphaBlendCache::IndexCacheRecord::IndexCacheRecord(int count)
{
    freeData = count;
    size = count;
    array = static_cast<unsigned short*>(Alloc::New(count * 2));
    nextFree = array;
    next = nullptr;
}

void __cdecl AlphaBlendCache::IndexCacheRecord::Reset()
{
    freeData = size;
    nextFree = array;
}

// 0x100b210: the private assignment is a no-op that returns this
const AlphaBlendCache& __cdecl AlphaBlendCache::operator=(const AlphaBlendCache&)
{
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------
// AlphaMaskCache

// 0x100030e0
bool __thiscall AlphaMaskCache::IsEmpty() const
{
    return count0 == 0;
}

// 0x10002eb0
void __cdecl AlphaMaskCache::Reset()
{
    count0 = 0;
    count1 = 0;
    count2 = 0;
    count3 = 0;
}

// 0x10073920
void __cdecl AlphaMaskCache::Begin(Display*, ViewPort*)
{
}

// 0x10002e10
void __cdecl AlphaMaskCache::Add(const Material* material, GeometryChunk* chunk, const DisplayLightPtrList* lights,
                                 const Matrix4* modelView)
{
    AppendPointer(&count0, chunk);
    AppendPointer(&count1, const_cast<Material*>(material));
    AppendPointer(&count2, const_cast<Matrix4*>(modelView));
    AppendPointer(&count3, const_cast<DisplayLightPtrList*>(lights));
}

// ---------------------------------------------------------------------------------------------------------------
// UnsortedAlphaBlendCache

// 0x100030e0 (shared)
bool __thiscall UnsortedAlphaBlendCache::IsEmpty() const
{
    return count0 == 0;
}

// 0x10072820
void __cdecl UnsortedAlphaBlendCache::Reset()
{
    count0 = 0;
    count1 = 0;
    count2 = 0;
    count3 = 0;
    count4 = 0;
}

// 0x10073920 (shared)
void __cdecl UnsortedAlphaBlendCache::Begin(Display*, ViewPort*)
{
}

// 0x10072760
void __cdecl UnsortedAlphaBlendCache::Add(const Material* material, GeometryChunk* chunk,
                                          const DisplayLightPtrList* lights, const Matrix4* modelView,
                                          float distance)
{
    AppendPointer(&count0, chunk);
    AppendPointer(&count1, const_cast<Material*>(material));
    AppendPointer(&count2, const_cast<Matrix4*>(modelView));
    AppendPointer(&count3, const_cast<DisplayLightPtrList*>(lights));
    AppendFloat(&count4, distance);
}

}  // namespace Jet
