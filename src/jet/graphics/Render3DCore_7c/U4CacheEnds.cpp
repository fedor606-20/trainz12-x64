// U4 wave 4: the render-cache entry points and the AlphaBlendCache bucket manager.
#include <jet/Render3DCoreCaches.h>

#include <cstddef>

namespace Jet {

namespace {

typedef void(__cdecl* MaterialRenderChunk)(void* material, void* display, void* view, void* chunk,
                                            const void* lights, const void* modelView, int pass);
typedef void(__cdecl* MaterialRenderPrimitives)(void* material, void* display, void* view, void* chunk,
                                                const void* lights, const void* modelView, int numIndices,
                                                unsigned short* indices, int numPrimitives, DPrimitive primitive,
                                                int pass);

inline void* Vtbl(void* object, int byteOffset)
{
    void** const table = *static_cast<void***>(object);
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(table) + byteOffset);
}

}  // namespace

// 0x10017b0 is already in AlphaCaches.cpp; the manager paths below are 0x1810..0x29a0.

// 0x10002690
void __cdecl AlphaBlendCache::RecalculateBucketParametrs(bool initial)
{
    const float scale = static_cast<float>(numBuckets) / maxBucketRange;
    sortEverythingId = static_cast<int>(sortEverythingRange * scale);
    sortPolysId = static_cast<int>(sortPolysInChunkRange * scale);
    bucketMultiplyer = scale;
    if (initial) {
        ResizeBuckets(bucketCount + bucketStep);
    } else {
        ResizeBuckets(numBuckets);
    }
    Reset();
}

// 0x10002a60-like helper: keep a [count, capacity, data] vector of CacheBuckets
void __cdecl AlphaBlendCache::ResizeBuckets(int count)
{
    if (count < 0) count = 0;
    if (count < bucketCount) {
        for (int i = count; i < bucketCount; ++i) buckets[i].~CacheBucket();
        bucketCount = count;
        return;
    }
    if (count == bucketCount) return;
    if (count > bucketCapacity) {
        int capacity = (count + 0x7f) & ~0x7f;
        if (capacity < 0x80) capacity = 0x80;
        CacheBucket* const data = static_cast<CacheBucket*>(Alloc::New(static_cast<unsigned>(capacity) * 0x10));
        if (buckets) {
            for (int i = 0; i < bucketCount; ++i) ::new (&data[i]) CacheBucket(buckets[i]);
            for (int i = 0; i < bucketCount; ++i) buckets[i].~CacheBucket();
            Alloc::Delete(buckets);
        }
        buckets = data;
        bucketCapacity = capacity;
    }
    for (int i = bucketCount; i < count; ++i) ::new (&buckets[i]) CacheBucket();
    bucketCount = count;
}

// 0x100029d0: append one bucketStep of empty buckets
void __cdecl AlphaBlendCache::ResetBuckets()
{
    ResizeBuckets(bucketCount + bucketStep);
}

// 0x10001810
void __cdecl AlphaBlendCache::End(Display* display, ViewPort* view)
{
    reinterpret_cast<void(__cdecl*)(AlphaBlendCache*, Display*)>(Vtbl(this, 0x38))(this, display);

    for (int i = numBuckets - 1; i >= 0; --i) {
        CacheBucket* const bucket =
            reinterpret_cast<CacheBucket*>(reinterpret_cast<char*>(buckets) + (bucketIndex + i) * 0x10);
        CacheRecord* record = bucket->GetNextRecord(bucket->head);
        while (record) {
            display->SetAlphaBlendFunction(blendSrc, blendDst);
            if (record->sortedFaces) {
                reinterpret_cast<MaterialRenderPrimitives>(Vtbl(const_cast<Material*>(record->material), 0x44))(
                    const_cast<Material*>(record->material), display, view, record->chunk, record->lights,
                    record->modelView, record->numIndices, record->sortedFaces, record->numPrimitives,
                    record->primitive, -1);
            } else {
                reinterpret_cast<MaterialRenderChunk>(Vtbl(const_cast<Material*>(record->material), 0x3c))(
                    const_cast<Material*>(record->material), display, view, record->chunk, record->lights,
                    record->modelView, -1);
            }
            record = bucket->GetNextRecord(record);
        }
        bucket->Reset(this);
    }

    currentIndexAllocator = headIndexAllocator;
    for (IndexCacheRecord* record = headIndexAllocator; record; record = record->next) record->Reset();

    reinterpret_cast<void(__cdecl*)(AlphaBlendCache*, Display*)>(Vtbl(this, 0x3c))(this, display);
    inFrame = false;
}

// 0x10002970: move to the next group of buckets, appending another group when the ring wraps, then Reset
void __cdecl AlphaBlendCache::Enter(Display* display, ViewPort* view, bool b)
{
    (void)display;
    (void)view;
    if (!b) return;
    bucketIndex += bucketStep;
    if (bucketIndex == bucketCount) ResetBuckets();
    Reset();
}

// 0x100029a0: End the current group and step back to the previous one
void __cdecl AlphaBlendCache::Leave(Display* display, ViewPort* view, bool b)
{
    if (!b) return;
    End(display, view);
    bucketIndex -= bucketStep;
}

// ---------------------------------------------------------------------------------------------------------------
// AlphaMaskCache

// 0x10002ed0
void __cdecl AlphaMaskCache::End(Display* display, ViewPort* view)
{
    display->SetAlphaTestMode(true);
    display->SetAlphaBlendMode(false);
    const unsigned char* const state =
        *reinterpret_cast<unsigned char* const*>(reinterpret_cast<char*>(display) + 0x54);
    const float savedReference = *reinterpret_cast<const float*>(state + 0xcc);
    display->SetAlphaTestFunction(static_cast<DFunction>(5), 0.5f);

    for (int i = 0; i < count0; ++i) {
        void* const material = data1[i];
        reinterpret_cast<MaterialRenderChunk>(Vtbl(material, 0x3c))(material, display, view, data0[i], data3[i],
                                                                    data2[i], -1);
    }

    display->SetAlphaTestMode(false);
    display->SetAlphaBlendMode(true);
    display->SetAlphaTestFunction(static_cast<DFunction>(5), savedReference);
    Reset();
}

// ---------------------------------------------------------------------------------------------------------------
// UnsortedAlphaBlendCache

// 0x10072840
void __cdecl UnsortedAlphaBlendCache::End(Display* display, ViewPort* view)
{
    if (m_ceebtrainz_delayed_alpha_render) return;

    const unsigned char* const state =
        *reinterpret_cast<unsigned char* const*>(reinterpret_cast<char*>(display) + 0x54);
    const float savedReference = *reinterpret_cast<const float*>(state + 0xcc);
    const int savedFunction = *reinterpret_cast<const int*>(state + 0xd0);
    const bool savedAlphaTest = ((state[0xa0] >> 5) & 1) != 0;
    const bool savedDepthWrite = ((state[0xa0] >> 1) & 1) != 0;

    display->SetAlphaTestMode(true);
    display->SetAlphaTestFunction(static_cast<DFunction>(5), 0.0f);

    for (int i = 0; i < count0; ++i) {
        void* const material = data1[i];
        reinterpret_cast<MaterialRenderChunk>(Vtbl(material, 0x3c))(material, display, view, data0[i], data3[i],
                                                                    data2[i], -1);
    }

    display->SetAlphaTestFunction(static_cast<DFunction>(savedFunction), savedReference);
    display->SetAlphaTestMode(savedAlphaTest);
    display->SetDepthWriteMode(savedDepthWrite);
    Reset();
}

// 0x10001ae0: decode the primitive counts used by the sort (vertex count in units of the primitive type)
int __cdecl AlphaBlendCache::SetupPrimitiveSort(GeometryChunk* chunk, int& verticesPerPrimitive, int& indicesPerPrimitive,
                                                float& reciprocal, DPrimitive& primitive)
{
    const unsigned type = *reinterpret_cast<unsigned*>(reinterpret_cast<char*>(chunk) + 0x18) & 0x1f;
    int count = *reinterpret_cast<int*>(reinterpret_cast<char*>(chunk) + 0x20);

    switch (type) {
    case 0:
        verticesPerPrimitive = 1;
        indicesPerPrimitive = 1;
        primitive = static_cast<DPrimitive>(0);
        break;
    case 1:
        verticesPerPrimitive = 2;
        indicesPerPrimitive = 2;
        count >>= 1;
        primitive = static_cast<DPrimitive>(1);
        break;
    case 2:
        verticesPerPrimitive = 2;
        indicesPerPrimitive = 1;
        --count;
        primitive = static_cast<DPrimitive>(1);
        break;
    case 4:
        count = *reinterpret_cast<int*>(reinterpret_cast<char*>(chunk) + 0x1c);
        verticesPerPrimitive = 3;
        indicesPerPrimitive = 3;
        primitive = static_cast<DPrimitive>(3);
        break;
    case 8:
    case 0x10:
        verticesPerPrimitive = 3;
        count -= 2;
        indicesPerPrimitive = 1;
        primitive = static_cast<DPrimitive>(3);
        break;
    default:
        verticesPerPrimitive = 1;
        indicesPerPrimitive = 1;
        reciprocal = 1.0f;
        primitive = static_cast<DPrimitive>(0);
        return 0;
    }

    reciprocal = 1.0f / static_cast<float>(verticesPerPrimitive);
    return count;
}

}  // namespace Jet
