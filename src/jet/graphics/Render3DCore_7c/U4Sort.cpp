// U4 wave 4: AlphaBlendCache::Add and the two sort paths. The distance uses the original's bit-trick square root
// (0x5f3759df, one Newton step) and SSE scalar operation order; the centre is the model-view translation pushed
// through the matrix's upper 3x3.
#include <jet/Render3DCoreCaches.h>

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace Jet {

namespace {

inline float FastSqrt(float value)
{
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    const uint32_t estimateBits = 0x5f3759dfu - (bits >> 1);
    float estimate;
    std::memcpy(&estimate, &estimateBits, sizeof estimate);
    volatile float x = value;
    const float half = x * 0.5f;
    const float curvature = half * estimate * estimate;
    const float corrected = (1.5f - curvature) * estimate;
    return corrected * x;
}

inline float DistanceSquared(float dz, float dy, float dx)
{
    const float z2 = dz * dz;
    const float y2 = dy * dy;
    const float x2 = dx * dx;
    return z2 + y2 + x2;
}

// GeometryChunk::primitiveLookup is RenderSystem's data export; the project header does not mark it dllimport, so
// resolve it at load time instead of emitting a local unresolved symbol.
const DPrimitive* PrimitiveLookup()
{
    static const DPrimitive* const table = []() -> const DPrimitive* {
        HMODULE const module = GetModuleHandleA("RenderSystem_7c.dll");
        if (!module) return nullptr;
        return reinterpret_cast<const DPrimitive*>(
            GetProcAddress(module, "?primitiveLookup@GeometryChunk@Jet@@2QBW4DPrimitive@2@B"));
    }();
    return table;
}

}  // namespace

// 0x10001930
void __cdecl AlphaBlendCache::Add(Display*, ViewPort*, const Material* material, GeometryChunk* chunk,
                                  const DisplayLightPtrList* lights, const Matrix4* modelView)
{
    const float* const m = reinterpret_cast<const float*>(modelView);
    const float distance = FastSqrt(DistanceSquared(m[0xe], m[0xd], m[0xc]));
    const float scaled = distance * bucketMultiplyer;

    int bucket = numBuckets - 1;
    if (!(static_cast<float>(bucket) <= scaled)) bucket = static_cast<int>(scaled);

    if (bucket < sortEverythingId) {
        SortChunkIntoBuckets(material, chunk, lights, modelView);
        return;
    }

    CacheRecord* const record =
        GetFreeCacheRecord(material, chunk, lights, modelView, nullptr, distance);
    const unsigned flags = *reinterpret_cast<const unsigned*>(reinterpret_cast<const char*>(chunk) + 0x18);
    record->numPrimitives = *reinterpret_cast<const int*>(reinterpret_cast<const char*>(chunk) + 0x1c);
    const DPrimitive* const lookup = PrimitiveLookup();
    record->primitive = lookup ? lookup[flags & 0x1f] : static_cast<DPrimitive>(0);

    if (bucket < sortPolysId) SortChunk(*record);

    CacheBucket* const target =
        reinterpret_cast<CacheBucket*>(reinterpret_cast<char*>(buckets) + (bucketIndex + bucket) * 0x10);
    target->Insert(record);
}

// 0x10001c60
void __cdecl AlphaBlendCache::SortChunk(CacheRecord& record)
{
    int verticesPerPrimitive = 0;
    int indicesPerPrimitive = 0;
    float reciprocal = 0.0f;
    DPrimitive primitive = static_cast<DPrimitive>(0);
    const int count = SetupPrimitiveSort(record.chunk, verticesPerPrimitive, indicesPerPrimitive, reciprocal, primitive);
    record.primitive = primitive;

    if (sortedFaceSize < count) {
        if (sortedFaces) Alloc::Delete(sortedFaces);
        sortedFaces = static_cast<SortFace*>(Alloc::New(static_cast<unsigned>(count) * sizeof(SortFace)));
        sortedFaceSize = count;
    }

    const float* const m = reinterpret_cast<const float*>(record.modelView);
    const float centreX = m[2] * -m[0xe] + m[1] * -m[0xd] + m[0] * -m[0xc];
    const float centreY = m[6] * -m[0xe] + m[5] * -m[0xd] + m[4] * -m[0xc];
    const float centreZ = m[10] * -m[0xe] + m[9] * -m[0xd] + m[8] * -m[0xc];

    const GeometryChunk::Vector3Ptr vertices =
        record.chunk->GetVector3Array(VIT_POSITION, 0);
    unsigned short* const indices = record.chunk->GetIndexArray();
    const unsigned type = *reinterpret_cast<const unsigned*>(reinterpret_cast<const char*>(record.chunk) + 0x18) & 0x1f;
    const int stride = static_cast<int>(vertices.stride);
    const char* const vertexBase = reinterpret_cast<const char*>(vertices.data);

    for (int i = 0; i < count; ++i) {
        const unsigned short* ip = indices + i * indicesPerPrimitive;
        const float* p = reinterpret_cast<const float*>(vertexBase + static_cast<unsigned>(*ip) * stride);
        float sx = p[0], sy = p[1], sz = p[2];
        if (verticesPerPrimitive > 1) {
            int remaining = verticesPerPrimitive - 1;
            ++ip;
            if (type == 0x10) {
                do {
                    p = reinterpret_cast<const float*>(vertexBase + static_cast<unsigned>(*ip++) * stride);
                    sx = p[0] + sx;
                    sy = p[1] + sy;
                    sz = p[2] + sz;
                } while (--remaining);
            } else {
                do {
                    p = reinterpret_cast<const float*>(vertexBase + static_cast<unsigned>(*ip++) * stride);
                    sx = sx + p[0];
                    sy = sy + p[1];
                    sz = sz + p[2];
                } while (--remaining);
            }
        }
        sx *= reciprocal;
        sy *= reciprocal;
        sz *= reciprocal;
        sortedFaces[i].face = i;
        sortedFaces[i].distance = DistanceSquared(sz - centreZ, sy - centreY, sx - centreX);
    }

    qsort(sortedFaces, static_cast<size_t>(count), sizeof(SortFace), GetSortFunction());
    if (count > 0)
        record.distance = (sortedFaces[count - 1].distance + sortedFaces[0].distance) * 0.5f;

    unsigned short* const out =
        static_cast<unsigned short*>(Alloc::New(static_cast<unsigned>(verticesPerPrimitive * count) * 2));
    record.userAllocated = true;
    record.sortedFaces = out;

    int written = 0;
    for (int i = 0; i < count; ++i) {
        const int base = sortedFaces[i].face * indicesPerPrimitive;
        if (type == 0x10) {
            out[written++] = indices[base];
            out[written++] = indices[base + 1];
            out[written++] = indices[base + 2];
        } else if (verticesPerPrimitive > 0) {
            const int forward = base;
            const int reverse = verticesPerPrimitive - base - 1;
            for (int j = 0; j < verticesPerPrimitive; ++j) {
                if (type == 8)
                    out[written++] = indices[((i & 1) == 0 ? forward : reverse) + j];
                else
                    out[written++] = indices[forward + j];
            }
        }
    }
    record.numPrimitives = count;
    record.numIndices = written;
}

// 0x100020d0
void __cdecl AlphaBlendCache::SortChunkIntoBuckets(const Material* material, GeometryChunk* chunk,
                                                   const DisplayLightPtrList* lights, const Matrix4* modelView)
{
    int verticesPerPrimitive = 0;
    int indicesPerPrimitive = 0;
    float reciprocal = 0.0f;
    DPrimitive primitive = static_cast<DPrimitive>(0);
    const int count = SetupPrimitiveSort(chunk, verticesPerPrimitive, indicesPerPrimitive, reciprocal, primitive);

    const float* const m = reinterpret_cast<const float*>(modelView);
    const float centreX = m[2] * -m[0xe] + m[1] * -m[0xd] + m[0] * -m[0xc];
    const float centreY = m[6] * -m[0xe] + m[5] * -m[0xd] + m[4] * -m[0xc];
    const float centreZ = m[10] * -m[0xe] + m[9] * -m[0xd] + m[8] * -m[0xc];

    const GeometryChunk::Vector3Ptr vertices = chunk->GetVector3Array(VIT_POSITION, 0);
    unsigned short* const indices = chunk->GetIndexArray();
    const unsigned type = *reinterpret_cast<const unsigned*>(reinterpret_cast<const char*>(chunk) + 0x18) & 0x1f;
    const int stride = static_cast<int>(vertices.stride);
    const char* const vertexBase = reinterpret_cast<const char*>(vertices.data);

    for (int i = 0; i < count; ++i) {
        const unsigned short* ip = indices + i * indicesPerPrimitive;
        unsigned short* const out = AllocateIndexArray(verticesPerPrimitive);
        const float* p = reinterpret_cast<const float*>(vertexBase + static_cast<unsigned>(*ip) * stride);
        unsigned short firstIndex = *ip;
        float sx = p[0], sy = p[1], sz = p[2];
        out[0] = firstIndex;
        for (int j = 1; j < verticesPerPrimitive; ++j) {
            const unsigned short index = ip[j];
            p = reinterpret_cast<const float*>(vertexBase + static_cast<unsigned>(index) * stride);
            sx = p[0] + sx;
            sy = p[1] + sy;
            sz = p[2] + sz;
            out[j] = index;
        }
        if (type == 8 && (i & 1)) {
            const unsigned short first = out[0];
            out[0] = out[2];
            out[2] = first;
        }
        sx *= reciprocal;
        sy *= reciprocal;
        sz *= reciprocal;
        const float distance = FastSqrt(DistanceSquared(sz - centreZ, sy - centreY, sx - centreX));

        CacheRecord* const record = GetFreeCacheRecord(material, chunk, lights, modelView, out, distance);
        record->numIndices = verticesPerPrimitive;
        record->numPrimitives = 1;
        record->primitive = primitive;

        int bucket = numBuckets - 1;
        const float scaled = bucketMultiplyer * distance;
        if (!(static_cast<float>(bucket) <= scaled)) bucket = static_cast<int>(scaled);
        CacheBucket* const target =
            reinterpret_cast<CacheBucket*>(reinterpret_cast<char*>(buckets) + (bucketIndex + bucket) * 0x10);
        target->Insert(record);
    }
}

}  // namespace Jet
