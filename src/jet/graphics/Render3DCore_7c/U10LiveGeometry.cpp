// Unit U10 wave 5: the shape geometry builders. Each creates the material's GeometryChunk through
// RenderPrimitive::CreateGeometryChunk, fills the position and packed-colour streams, calls the shape's index
// builder and initialises the buffers. Loops, vertex order, counts and creation type values are the original's
// (notes/agents/r3d_workstreams/u10_primitives_asm.txt).
#include <jet/Render3DCorePrimitives.h>

#include <jet/Geometry.h>
#include <jet/RenderSystem.h>

#include <cmath>
#include <cstring>
#include <xmmintrin.h>

namespace Jet {

namespace {

float FAdd(float a, float b) { return _mm_cvtss_f32(_mm_add_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }
float FSub(float a, float b) { return _mm_cvtss_f32(_mm_sub_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }
float FMul(float a, float b) { return _mm_cvtss_f32(_mm_mul_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }
float FDiv(float a, float b) { return _mm_cvtss_f32(_mm_div_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }

// 0x63080 / 0x63090: x87 fcos / fsin, the result stored as a float
float FCos(float a)
{
    float r;
    __asm {
        fld a
        fcos
        fstp r
    }
    return r;
}

float FSin(float a)
{
    float r;
    __asm {
        fld a
        fsin
        fstp r
    }
    return r;
}

// 0x1f5c0: x87 fsqrt, the result stored as a float
float FSqrt(float a)
{
    float r;
    __asm {
        fld a
        fsqrt
        fstp r
    }
    return r;
}

void MakeHint(StandardChunkHint& hint)
{
    ::new (&hint) StandardChunkHint();
    hint.bits |= 1;
}

void Finish(GeometryChunk* chunk, RenderKernel* kernel)
{
    chunk->InitializeBuffers(kernel->GetDisplay(), true);
}

void FillColors(GeometryChunk* chunk, unsigned int count)
{
    GeometryChunk::UDwordPtr colors = chunk->GetUDwordArray(static_cast<VertexItemType>(R3D_ITEM_COLOR), 0);
    const unsigned long packed = 0;  // replaced by the caller's colour when needed
    (void)packed;
    for (unsigned int i = 0; i < count; ++i) {
        // the caller stores the packed colour through the same view; see the per-shape code
        colors.data = reinterpret_cast<unsigned long*>(reinterpret_cast<char*>(colors.data) + 0);
    }
}

// the original's fast square root (0x626d0): inverse-square-root seed, one Newton step in x87, times len2
float ApproxLengthSquared(float len2)
{
    unsigned int bits;
    std::memcpy(&bits, &len2, 4);
    const unsigned int seedBits = 0x5f3759dfu - (bits >> 1);
    float seed;
    std::memcpy(&seed, &seedBits, 4);
    const float half = 0.5f;
    const float oneFive = 1.5f;
    float result;
    __asm {
        fld len2
        fld half
        fmul st, st(1)
        fld seed
        fmul st(1), st
        fmul st(1), st
        fld oneFive
        fsubrp st(2), st
        fmulp st(1), st
        fmulp st(1), st
        fstp result
    }
    return result;
}

char* PositionView(GeometryChunk* chunk, unsigned int& stride)
{
    GeometryChunk::Vector3Ptr view = chunk->GetVector3Array(static_cast<VertexItemType>(R3D_ITEM_POSITION), 0);
    stride = view.stride;
    return reinterpret_cast<char*>(view.data);
}

void WriteColor(GeometryChunk* chunk, unsigned int count, unsigned long packed)
{
    GeometryChunk::UDwordPtr view = chunk->GetUDwordArray(static_cast<VertexItemType>(R3D_ITEM_COLOR), 0);
    char* p = reinterpret_cast<char*>(view.data);
    for (unsigned int i = 0; i < count; ++i) {
        *reinterpret_cast<unsigned long*>(p) = packed;
        p += view.stride;
    }
}

}  // namespace

// 0x60d70
bool __cdecl RenderBox::BuildGeometry(RenderKernel* kernel_, const Color& value, int xSegments_, int ySegments_,
                                      int zSegments_, int creationFlags_)
{
    if (!(0.0f < extents.x) || !(0.0f < extents.y) || !(0.0f < extents.z)) return false;
    if (xSegments_ == 0) xSegments_ = 1;
    if (ySegments_ == 0) ySegments_ = 1;
    if (zSegments_ == 0) zSegments_ = 1;

    const unsigned long packed = Display::ConvertColor(value);
    StandardChunkHint hint;
    MakeHint(hint);
    const float dx = FDiv(FMul(extents.x, 2.0f), static_cast<float>(xSegments_));
    const float dy = FDiv(FMul(extents.y, 2.0f), static_cast<float>(ySegments_));
    const float dz = FDiv(FMul(extents.z, 2.0f), static_cast<float>(zSegments_));
    if (creationFlags_ & 1) {
        xSegments_ = 1;
        ySegments_ = 1;
        zSegments_ = 1;
    }

    GeometryChunk* chunk;
    unsigned int vertexCount;
    if ((creationFlags_ & 3) == 0) {
        const int x = xSegments_;
        const int y = ySegments_;
        const int z = zSegments_;
        vertexCount = static_cast<unsigned int>(2 * ((x + y) * (z - 1) + (x + 1) * (y + 1)));
        const int faces = x * y + x * z + y * z;
        chunk = CreateGeometryChunk(vertexCount, static_cast<unsigned int>(faces * 4),
                                    static_cast<unsigned int>(faces * 12), 0,
                                    static_cast<Material::GeometryType>(0), &hint);
        unsigned int stride = 0;
        char* p = PositionView(chunk, stride);
        const float hx = extents.x;
        const float hy = extents.y;
        const float hz = extents.z;
        // +z face
        for (int row = 0; row <= y; ++row) {
            const float yy = FSub(hy, FMul(dy, static_cast<float>(row)));
            float xx = FSub(-0.0f, hx);
            for (int col = 0; col <= x; ++col) {
                *reinterpret_cast<float*>(p) = xx;
                *reinterpret_cast<float*>(p + 4) = yy;
                *reinterpret_cast<float*>(p + 8) = hz;
                p += stride;
                xx = FAdd(xx, dx);
            }
        }
        // side rings
        for (int ring = 1; ring < z; ++ring) {
            const float zz = FSub(hz, FMul(dz, static_cast<float>(ring)));
            float xx = FSub(-0.0f, hx);
            for (int col = 0; col < x; ++col) {
                *reinterpret_cast<float*>(p) = xx;
                *reinterpret_cast<float*>(p + 4) = hy;
                *reinterpret_cast<float*>(p + 8) = zz;
                p += stride;
                xx = FAdd(xx, dx);
            }
            float yy = hy;
            for (int row = 0; row <= y; ++row) {
                *reinterpret_cast<float*>(p) = hx;
                *reinterpret_cast<float*>(p + 4) = yy;
                *reinterpret_cast<float*>(p + 8) = zz;
                p += stride;
                yy = FSub(yy, dy);
            }
            xx = hx;
            for (int col = 0; col < x; ++col) {
                *reinterpret_cast<float*>(p) = xx;
                *reinterpret_cast<float*>(p + 4) = FSub(-0.0f, hy);
                *reinterpret_cast<float*>(p + 8) = zz;
                p += stride;
                xx = FSub(xx, dx);
            }
            yy = FSub(-0.0f, hy);
            for (int row = 1; row < y; ++row) {
                yy = FAdd(yy, dy);
                *reinterpret_cast<float*>(p) = FSub(-0.0f, hx);
                *reinterpret_cast<float*>(p + 4) = yy;
                *reinterpret_cast<float*>(p + 8) = zz;
                p += stride;
            }
        }
        // -z face
        for (int row = 0; row <= y; ++row) {
            const float yy = FSub(hy, FMul(dy, static_cast<float>(row)));
            float xx = FSub(-0.0f, hx);
            for (int col = 0; col <= x; ++col) {
                *reinterpret_cast<float*>(p) = xx;
                *reinterpret_cast<float*>(p + 4) = yy;
                *reinterpret_cast<float*>(p + 8) = FSub(-0.0f, hz);
                p += stride;
                xx = FAdd(xx, dx);
            }
        }
        BuildIndicesSolid(x, y, z, 0, chunk->GetIndexArray(), static_cast<int>(vertexCount));
    } else {
        const int x = xSegments_;
        const int y = ySegments_;
        const int z = zSegments_;
        vertexCount = static_cast<unsigned int>((z + 1) * (x + y) * 2);
        const int levels = x + y + z;
        chunk = CreateGeometryChunk(vertexCount, static_cast<unsigned int>(levels * 4),
                                    static_cast<unsigned int>(levels * 8), 0,
                                    static_cast<Material::GeometryType>(3), &hint);
        unsigned int stride = 0;
        char* p = PositionView(chunk, stride);
        const float hx = extents.x;
        const float hy = extents.y;
        const float hz = extents.z;
        float zz = hz;
        for (int level = 0; level <= z; ++level) {
            float xx = FSub(-0.0f, hx);
            for (int col = 0; col < x; ++col) {
                *reinterpret_cast<float*>(p) = xx;
                *reinterpret_cast<float*>(p + 4) = hy;
                *reinterpret_cast<float*>(p + 8) = zz;
                p += stride;
                xx = FAdd(xx, dx);
            }
            float yy = hy;
            for (int row = 0; row < y; ++row) {
                *reinterpret_cast<float*>(p) = hx;
                *reinterpret_cast<float*>(p + 4) = yy;
                *reinterpret_cast<float*>(p + 8) = zz;
                p += stride;
                yy = FSub(yy, dy);
            }
            xx = hx;
            for (int col = 0; col < x; ++col) {
                *reinterpret_cast<float*>(p) = xx;
                *reinterpret_cast<float*>(p + 4) = FSub(-0.0f, hy);
                *reinterpret_cast<float*>(p + 8) = zz;
                p += stride;
                xx = FSub(xx, dx);
            }
            yy = FSub(-0.0f, hy);
            for (int row = 0; row < y; ++row) {
                *reinterpret_cast<float*>(p) = FSub(-0.0f, hx);
                *reinterpret_cast<float*>(p + 4) = yy;
                *reinterpret_cast<float*>(p + 8) = zz;
                p += stride;
                yy = FAdd(yy, dy);
            }
            zz = FSub(zz, dz);
        }
        BuildIndicesWire(x, y, z, 0, chunk->GetIndexArray(), static_cast<int>(vertexCount));
    }

    WriteColor(chunk, vertexCount, packed);
    Finish(chunk, kernel_);
    return true;
}

// 0x690c0
bool __cdecl RenderSphere::BuildGeometry(RenderKernel* kernel_, const Color& value, int segments,
                                         int creationFlags_)
{
    if (!(0.0f < radius)) return false;
    if (segments < 4) segments = 4;
    const int rings = segments / 2;
    const unsigned int vertexCount = static_cast<unsigned int>((rings - 1) * segments + 2);
    const unsigned long packed = Display::ConvertColor(value);
    StandardChunkHint hint;
    MakeHint(hint);
    const int wire = (creationFlags_ & 3) != 0;

    GeometryChunk* chunk;
    if (!wire) {
        const unsigned int primitives = static_cast<unsigned int>((rings * 2 - 2) * segments);
        chunk = CreateGeometryChunk(vertexCount, primitives, primitives * 3, 0,
                                    static_cast<Material::GeometryType>(0), &hint);
    } else {
        const int lines = (segments + 1) * (rings - 1) + 2 + (segments - 1) * rings;
        chunk = CreateGeometryChunk(vertexCount, static_cast<unsigned int>(lines - 1),
                                    static_cast<unsigned int>(lines), 0,
                                    static_cast<Material::GeometryType>(4), &hint);
    }

    WriteColor(chunk, vertexCount, packed);
    unsigned int stride = 0;
    char* p = PositionView(chunk, stride);
    const float r = radius;
    *reinterpret_cast<float*>(p) = 0.0f;
    *reinterpret_cast<float*>(p + 4) = 0.0f;
    *reinterpret_cast<float*>(p + 8) = r;
    p += stride;

    const float pi = 3.1415927f;
    const float step = FDiv(pi, static_cast<float>(segments));
    for (int ring = 1; ring < rings; ++ring) {
        const float angle = FMul(static_cast<float>(ring), FDiv(pi, static_cast<float>(rings)));
        const float sinA = FMul(FSin(angle), r);
        const float cosA = FMul(FCos(angle), r);
        for (int i = 0; i < segments; ++i) {
            const float a = FMul(static_cast<float>(i), step);
            *reinterpret_cast<float*>(p) = FSub(-0.0f, FMul(FCos(a), sinA));
            *reinterpret_cast<float*>(p + 4) = FMul(FSin(a), sinA);
            *reinterpret_cast<float*>(p + 8) = cosA;
            p += stride;
        }
    }
    *reinterpret_cast<float*>(p) = 0.0f;
    *reinterpret_cast<float*>(p + 4) = 0.0f;
    *reinterpret_cast<float*>(p + 8) = FSub(-0.0f, r);

    if (!wire)
        BuildIndicesSolid(segments, static_cast<int>(vertexCount), chunk->GetIndexArray(), 0);
    else
        BuildIndicesWire(segments, static_cast<int>(vertexCount), chunk->GetIndexArray(), 0);
    Finish(chunk, kernel_);
    return true;
}

// 0x651f0
bool __cdecl RenderEllipsoid::BuildGeometry(RenderKernel* kernel_, const Color& value, int segments,
                                            int creationFlags_)
{
    if (!(0.0f < radiusX) || !(0.0f < radiusZ) || !(0.0f < radiusZ)) return false;
    if (segments < 4) segments = 4;
    const int rings = segments / 2;
    const unsigned int vertexCount = static_cast<unsigned int>((rings - 1) * segments + 2);
    const unsigned long packed = Display::ConvertColor(value);
    StandardChunkHint hint;
    MakeHint(hint);
    const bool wire = (creationFlags_ & 2) != 0;

    GeometryChunk* chunk;
    if (!wire) {
        const unsigned int primitives = static_cast<unsigned int>((rings * 2 - 2) * segments);
        chunk = CreateGeometryChunk(vertexCount, primitives, primitives * 3, 0,
                                    static_cast<Material::GeometryType>(0), &hint);
    } else {
        const int lines = (segments + 1) * (rings - 1) + 2 + (segments - 1) * rings;
        chunk = CreateGeometryChunk(vertexCount, static_cast<unsigned int>(lines - 1),
                                    static_cast<unsigned int>(lines), 0,
                                    static_cast<Material::GeometryType>(4), &hint);
    }

    WriteColor(chunk, vertexCount, packed);
    unsigned int stride = 0;
    char* p = PositionView(chunk, stride);
    *reinterpret_cast<float*>(p) = 0.0f;
    *reinterpret_cast<float*>(p + 4) = 0.0f;
    *reinterpret_cast<float*>(p + 8) = radiusZ;
    p += stride;

    const float pi = 3.1415927f;
    const float step = FDiv(pi, static_cast<float>(segments));
    for (int ring = 1; ring < rings; ++ring) {
        const float angle = FMul(static_cast<float>(ring), FDiv(pi, static_cast<float>(rings)));
        const float z = FMul(FCos(angle), radiusZ);
        const float t = FSub(1.0f, FDiv(FMul(z, z), FMul(radiusZ, radiusZ)));
        const float f = FSqrt(t);
        const float rx = FMul(f, radiusX);
        const float ry = FMul(f, radiusY);
        for (int i = 0; i < segments; ++i) {
            const float a = FMul(static_cast<float>(i), step);
            *reinterpret_cast<float*>(p) = FSub(-0.0f, FMul(FCos(a), rx));
            *reinterpret_cast<float*>(p + 4) = FMul(FSin(a), ry);
            *reinterpret_cast<float*>(p + 8) = z;
            p += stride;
        }
    }
    *reinterpret_cast<float*>(p) = 0.0f;
    *reinterpret_cast<float*>(p + 4) = 0.0f;
    *reinterpret_cast<float*>(p + 8) = FSub(-0.0f, radiusZ);

    if (!wire)
        BuildIndicesSolid(segments, static_cast<int>(vertexCount), chunk->GetIndexArray(), 0);
    else
        BuildIndicesWire(segments, static_cast<int>(vertexCount), chunk->GetIndexArray(), 0);
    Finish(chunk, kernel_);
    return true;
}

// 0x63270
bool __cdecl RenderCone::BuildGeometry(RenderKernel* kernel_, const Color& value, int segments,
                                       int creationFlags_)
{
    if (!(0.0f < radius) || !(0.0f < height)) return false;
    if (segments < 4) segments = 4;
    const unsigned int vertexCount = static_cast<unsigned int>(segments + 2);
    const unsigned long packed = Display::ConvertColor(value);
    StandardChunkHint hint;
    MakeHint(hint);
    const bool wire = (creationFlags_ & 3) != 0;

    GeometryChunk* chunk;
    if (!wire)
        chunk = CreateGeometryChunk(vertexCount, static_cast<unsigned int>(segments * 2),
                                    static_cast<unsigned int>(segments * 6), 0,
                                    static_cast<Material::GeometryType>(0), &hint);
    else
        chunk = CreateGeometryChunk(vertexCount, static_cast<unsigned int>(segments * 3),
                                    static_cast<unsigned int>(segments * 6), 0,
                                    static_cast<Material::GeometryType>(3), &hint);

    unsigned int stride = 0;
    char* p = PositionView(chunk, stride);
    const float pi = 3.1415927f;
    const float step = FDiv(pi + pi, static_cast<float>(segments));
    for (int i = 0; i < segments; ++i) {
        const float a = FMul(static_cast<float>(i), step);
        *reinterpret_cast<float*>(p) = FMul(FSin(a), radius);
        *reinterpret_cast<float*>(p + 4) = FMul(FCos(a), radius);
        *reinterpret_cast<float*>(p + 8) = 0.0f;
        p += stride;
    }
    *reinterpret_cast<float*>(p) = 0.0f;
    *reinterpret_cast<float*>(p + 4) = 0.0f;
    *reinterpret_cast<float*>(p + 8) = 0.0f;
    p += stride;
    *reinterpret_cast<float*>(p) = 0.0f;
    *reinterpret_cast<float*>(p + 4) = 0.0f;
    *reinterpret_cast<float*>(p + 8) = height;

    if (!wire)
        BuildIndicesSolid(segments, static_cast<int>(vertexCount), chunk->GetIndexArray(), 0);
    else
        BuildIndicesWire(segments, static_cast<int>(vertexCount), chunk->GetIndexArray(), 0);
    WriteColor(chunk, vertexCount, packed);
    Finish(chunk, kernel_);
    return true;
}

// 0x63f50
bool __cdecl RenderCylinder::BuildGeometry(RenderKernel* kernel_, const Color& value, int segments,
                                           int creationFlags_)
{
    if (!(0.0f < radius) || !(0.0f < height)) return false;
    if (segments < 4) segments = 4;
    const unsigned int vertexCount = static_cast<unsigned int>(segments * 2 + 2);
    const unsigned long packed = Display::ConvertColor(value);
    StandardChunkHint hint;
    MakeHint(hint);
    const bool wire = (creationFlags_ & 3) != 0;

    GeometryChunk* chunk;
    if (!wire)
        chunk = CreateGeometryChunk(vertexCount, static_cast<unsigned int>(segments * 4),
                                    static_cast<unsigned int>(segments * 12), 0,
                                    static_cast<Material::GeometryType>(0), &hint);
    else
        chunk = CreateGeometryChunk(vertexCount, static_cast<unsigned int>(segments * 5),
                                    static_cast<unsigned int>(segments * 10), 0,
                                    static_cast<Material::GeometryType>(3), &hint);

    unsigned int stride = 0;
    char* p = PositionView(chunk, stride);
    const float pi = 3.1415927f;
    const float step = FDiv(pi + pi, static_cast<float>(segments));
    for (int i = 0; i < segments; ++i) {
        const float a = FMul(static_cast<float>(i), step);
        const float x = FMul(FSin(a), radius);
        const float y = FMul(FCos(a), radius);
        *reinterpret_cast<float*>(p) = x;
        *reinterpret_cast<float*>(p + 4) = y;
        *reinterpret_cast<float*>(p + 8) = 0.0f;
        p += stride;
        *reinterpret_cast<float*>(p) = x;
        *reinterpret_cast<float*>(p + 4) = y;
        *reinterpret_cast<float*>(p + 8) = height;
        p += stride;
    }
    *reinterpret_cast<float*>(p) = 0.0f;
    *reinterpret_cast<float*>(p + 4) = 0.0f;
    *reinterpret_cast<float*>(p + 8) = 0.0f;
    p += stride;
    *reinterpret_cast<float*>(p) = 0.0f;
    *reinterpret_cast<float*>(p + 4) = 0.0f;
    *reinterpret_cast<float*>(p + 8) = height;

    if (!wire)
        BuildIndicesSolid(segments, static_cast<int>(vertexCount), chunk->GetIndexArray(), 0);
    else
        BuildIndicesWire(segments, static_cast<int>(vertexCount), chunk->GetIndexArray(), 0);
    WriteColor(chunk, vertexCount, packed);
    Finish(chunk, kernel_);
    return true;
}

// 0x62d40
bool __cdecl RenderCircle::BuildGeometry(RenderKernel* kernel_, const Color& value, int segments,
                                         int creationFlags_)
{
    if (!(0.0f < radius)) return false;
    if (segments < 4) segments = 4;
    const unsigned long packed = Display::ConvertColor(value);
    StandardChunkHint hint;
    MakeHint(hint);

    unsigned int vertexCount;
    unsigned int primitives;
    unsigned int indices;
    int type;
    if ((creationFlags_ & 1) == 0) {
        if ((creationFlags_ & 2) == 0) {
            vertexCount = static_cast<unsigned int>(segments + 1);
            primitives = static_cast<unsigned int>(segments);
            indices = static_cast<unsigned int>(segments + 2);
            type = 2;
        } else {
            vertexCount = static_cast<unsigned int>(segments + 1);
            primitives = static_cast<unsigned int>(segments * 2);
            indices = static_cast<unsigned int>(segments * 4);
            type = 3;
        }
    } else {
        vertexCount = static_cast<unsigned int>(segments);
        primitives = static_cast<unsigned int>(segments);
        indices = static_cast<unsigned int>(segments + 1);
        type = 4;
    }
    GeometryChunk* const chunk = CreateGeometryChunk(vertexCount, primitives, indices, 0,
                                                     static_cast<Material::GeometryType>(type), &hint);
    unsigned short* const idx = chunk->GetIndexArray();
    unsigned int stride = 0;
    char* p = PositionView(chunk, stride);
    const float pi = 3.1415927f;
    const float step = FDiv(pi + pi, static_cast<float>(segments));
    for (int i = 0; i < segments; ++i) {
        const float a = FMul(static_cast<float>(i), step);
        *reinterpret_cast<float*>(p) = FSub(-0.0f, FMul(FSin(a), radius));
        *reinterpret_cast<float*>(p + 4) = FMul(FCos(a), radius);
        *reinterpret_cast<float*>(p + 8) = 0.0f;
        p += stride;
    }
    if ((creationFlags_ & 1) == 0) {
        *reinterpret_cast<float*>(p) = 0.0f;
        *reinterpret_cast<float*>(p + 4) = 0.0f;
        *reinterpret_cast<float*>(p + 8) = 0.0f;
        if ((creationFlags_ & 2) == 0) {
            idx[0] = static_cast<unsigned short>(segments);
            for (int i = 0; i < segments; ++i) idx[1 + i] = static_cast<unsigned short>(i);
            idx[segments + 1] = 0;
        } else {
            int c = 0;
            for (int i = 0; i < segments - 1; ++i) {
                idx[c++] = static_cast<unsigned short>(i);
                idx[c++] = static_cast<unsigned short>(i + 1);
            }
            idx[c++] = static_cast<unsigned short>(segments - 1);
            idx[c++] = 0;
            for (int i = 0; i < segments; ++i) {
                idx[c++] = static_cast<unsigned short>(segments);
                idx[c++] = static_cast<unsigned short>(i);
            }
        }
    } else {
        for (int i = 0; i < segments; ++i) idx[i] = static_cast<unsigned short>(i);
        idx[segments] = 0;
    }
    WriteColor(chunk, vertexCount, packed);
    Finish(chunk, kernel_);
    return true;
}

// 0x64c30
bool __cdecl RenderEllipse::BuildGeometry(RenderKernel* kernel_, const Color& value, int segments,
                                          int creationFlags_)
{
    if (!(0.0f < radiusX) || !(0.0f < radiusY)) return false;
    if (segments < 4) segments = 4;
    const unsigned long packed = Display::ConvertColor(value);
    StandardChunkHint hint;
    MakeHint(hint);

    unsigned int vertexCount;
    unsigned int primitives;
    unsigned int indices;
    int type;
    if ((creationFlags_ & 1) == 0) {
        if ((creationFlags_ & 2) == 0) {
            vertexCount = static_cast<unsigned int>(segments + 1);
            primitives = static_cast<unsigned int>(segments);
            indices = static_cast<unsigned int>(segments + 2);
            type = 2;
        } else {
            vertexCount = static_cast<unsigned int>(segments + 1);
            primitives = static_cast<unsigned int>(segments * 2);
            indices = static_cast<unsigned int>(segments * 4);
            type = 3;
        }
    } else {
        vertexCount = static_cast<unsigned int>(segments);
        primitives = static_cast<unsigned int>(segments);
        indices = static_cast<unsigned int>(segments + 1);
        type = 4;
    }
    GeometryChunk* const chunk = CreateGeometryChunk(vertexCount, primitives, indices, 0,
                                                     static_cast<Material::GeometryType>(type), &hint);
    unsigned short* const idx = chunk->GetIndexArray();
    unsigned int stride = 0;
    char* p = PositionView(chunk, stride);
    const float pi = 3.1415927f;
    const float step = FDiv(pi + pi, static_cast<float>(segments));
    for (int i = 0; i < segments; ++i) {
        const float a = FMul(static_cast<float>(i), step);
        *reinterpret_cast<float*>(p) = FSub(-0.0f, FMul(FSin(a), radiusX));
        *reinterpret_cast<float*>(p + 4) = FMul(FCos(a), radiusY);
        *reinterpret_cast<float*>(p + 8) = 0.0f;
        p += stride;
    }
    if ((creationFlags_ & 1) == 0) {
        *reinterpret_cast<float*>(p) = 0.0f;
        *reinterpret_cast<float*>(p + 4) = 0.0f;
        *reinterpret_cast<float*>(p + 8) = 0.0f;
        if ((creationFlags_ & 2) == 0) {
            idx[0] = static_cast<unsigned short>(segments);
            for (int i = 0; i < segments; ++i) idx[1 + i] = static_cast<unsigned short>(i);
            idx[segments + 1] = 0;
        } else {
            int c = 0;
            for (int i = 0; i < segments - 1; ++i) {
                idx[c++] = static_cast<unsigned short>(i);
                idx[c++] = static_cast<unsigned short>(i + 1);
            }
            idx[c++] = static_cast<unsigned short>(segments - 1);
            idx[c++] = 0;
            for (int i = 0; i < segments; ++i) {
                idx[c++] = static_cast<unsigned short>(segments);
                idx[c++] = static_cast<unsigned short>(i);
            }
        }
    } else {
        for (int i = 0; i < segments; ++i) idx[i] = static_cast<unsigned short>(i);
        idx[segments] = 0;
    }
    WriteColor(chunk, vertexCount, packed);
    Finish(chunk, kernel_);
    return true;
}

// 0x671d0
bool __cdecl RenderPlane::BuildGeometry(RenderKernel* kernel_, const Color& value, float width_, float height_,
                                        int xSegments, int ySegments, int creationFlags_)
{
    if (!(0.0f < width_) || !(0.0f < height_)) return false;
    this->width = FMul(width_, 0.5f);
    this->height = FMul(height_, 0.5f);
    if (xSegments == 0) xSegments = 1;
    if (ySegments == 0) ySegments = 1;
    const unsigned long packed = Display::ConvertColor(value);
    StandardChunkHint hint;
    MakeHint(hint);
    const float dx = FDiv(width_, static_cast<float>(xSegments));
    const float dy = FDiv(height_, static_cast<float>(ySegments));

    GeometryChunk* chunk;
    unsigned int vertexCount;
    if ((creationFlags_ & 1) == 0) {
        if ((creationFlags_ & 2) == 0) {
            vertexCount = static_cast<unsigned int>((xSegments + 1) * (ySegments + 1));
            chunk = CreateGeometryChunk(vertexCount, static_cast<unsigned int>(xSegments * ySegments * 2),
                                        static_cast<unsigned int>(xSegments * ySegments * 6), 0,
                                        static_cast<Material::GeometryType>(0), &hint);
            unsigned int stride = 0;
            char* p = PositionView(chunk, stride);
            float yy = this->height;
            for (int row = 0; row <= ySegments; ++row) {
                float xx = FSub(-0.0f, this->width);
                for (int col = 0; col <= xSegments; ++col) {
                    *reinterpret_cast<float*>(p) = xx;
                    *reinterpret_cast<float*>(p + 4) = yy;
                    *reinterpret_cast<float*>(p + 8) = 0.0f;
                    p += stride;
                    xx = FAdd(xx, dx);
                }
                yy = FSub(yy, dy);
            }
            BuildIndicesSolid(xSegments, ySegments, creationFlags_, chunk->GetIndexArray(),
                              static_cast<int>(vertexCount));
        } else {
            vertexCount = static_cast<unsigned int>((xSegments + ySegments) * 2);
            chunk = CreateGeometryChunk(vertexCount, static_cast<unsigned int>(xSegments + ySegments + 2),
                                        static_cast<unsigned int>((xSegments + ySegments + 2) * 2), 0,
                                        static_cast<Material::GeometryType>(3), &hint);
            unsigned int stride = 0;
            char* p = PositionView(chunk, stride);
            float xx = FSub(-0.0f, this->width);
            for (int col = 0; col <= xSegments; ++col) {
                *reinterpret_cast<float*>(p) = xx;
                *reinterpret_cast<float*>(p + 4) = this->height;
                *reinterpret_cast<float*>(p + 8) = 0.0f;
                p += stride;
                *reinterpret_cast<float*>(p) = xx;
                *reinterpret_cast<float*>(p + 4) = FSub(-0.0f, this->height);
                *reinterpret_cast<float*>(p + 8) = 0.0f;
                p += stride;
                xx = FAdd(xx, dx);
            }
            float yy = FSub(this->height, dy);
            for (int row = 1; row < ySegments; ++row) {
                *reinterpret_cast<float*>(p) = FSub(-0.0f, this->width);
                *reinterpret_cast<float*>(p + 4) = yy;
                *reinterpret_cast<float*>(p + 8) = 0.0f;
                p += stride;
                *reinterpret_cast<float*>(p) = this->width;
                *reinterpret_cast<float*>(p + 4) = yy;
                *reinterpret_cast<float*>(p + 8) = 0.0f;
                p += stride;
                yy = FSub(yy, dy);
            }
            BuildIndicesWire(xSegments, ySegments, creationFlags_, chunk->GetIndexArray(),
                             static_cast<int>(vertexCount));
        }
    } else {
        vertexCount = 4;
        chunk = CreateGeometryChunk(4, 4, 5, 0, static_cast<Material::GeometryType>(4), &hint);
        unsigned int stride = 0;
        char* p = PositionView(chunk, stride);
        *reinterpret_cast<float*>(p) = FSub(-0.0f, this->width);
        *reinterpret_cast<float*>(p + 4) = this->height;
        *reinterpret_cast<float*>(p + 8) = 0.0f;
        p += stride;
        *reinterpret_cast<float*>(p) = this->width;
        *reinterpret_cast<float*>(p + 4) = this->height;
        *reinterpret_cast<float*>(p + 8) = 0.0f;
        p += stride;
        *reinterpret_cast<float*>(p) = this->width;
        *reinterpret_cast<float*>(p + 4) = FSub(-0.0f, this->height);
        *reinterpret_cast<float*>(p + 8) = 0.0f;
        p += stride;
        *reinterpret_cast<float*>(p) = FSub(-0.0f, this->width);
        *reinterpret_cast<float*>(p + 4) = FSub(-0.0f, this->height);
        *reinterpret_cast<float*>(p + 8) = 0.0f;
        unsigned short* const idx = chunk->GetIndexArray();
        idx[0] = 0;
        idx[1] = 1;
        idx[2] = 2;
        idx[3] = 3;
        idx[4] = 0;
    }
    WriteColor(chunk, vertexCount, packed);
    Finish(chunk, kernel_);
    return true;
}

// 0x6a1d0
bool __cdecl RenderTube::BuildGeometry(RenderKernel* kernel_, const Color& value, int segments,
                                       int creationFlags_)
{
    if (!(0.0f < radius) || !(0.0f < thickness)) return false;
    if (height <= 0.0f && height != 0.0f) height = 0.0f;
    if (segments < 4) segments = 4;
    const unsigned long packed = Display::ConvertColor(value);
    StandardChunkHint hint;
    MakeHint(hint);
    const bool hasInner = 0.0f < height;
    const unsigned int vertexCount = hasInner ? static_cast<unsigned int>(segments * 4)
                                              : static_cast<unsigned int>(segments * 2 + 2);
    const bool wire = (creationFlags_ & 3) != 0;

    GeometryChunk* chunk;
    if (!wire) {
        const unsigned int primitives = static_cast<unsigned int>(segments * (hasInner ? 8 : 4));
        chunk = CreateGeometryChunk(vertexCount, primitives, primitives * 3, 0,
                                    static_cast<Material::GeometryType>(0), &hint);
    } else {
        const unsigned int primitives = static_cast<unsigned int>(segments * (hasInner ? 8 : 5));
        chunk = CreateGeometryChunk(vertexCount, primitives, primitives * 2, 0,
                                    static_cast<Material::GeometryType>(3), &hint);
    }

    unsigned int stride = 0;
    char* p = PositionView(chunk, stride);
    const float pi = 3.1415927f;
    const float step = FDiv(pi + pi, static_cast<float>(segments));
    for (int i = 0; i < segments; ++i) {
        const float a = FMul(static_cast<float>(i), step);
        const float x = FMul(FSin(a), radius);
        const float y = FMul(FCos(a), radius);
        *reinterpret_cast<float*>(p) = x;
        *reinterpret_cast<float*>(p + 4) = y;
        *reinterpret_cast<float*>(p + 8) = 0.0f;
        p += stride;
        *reinterpret_cast<float*>(p) = x;
        *reinterpret_cast<float*>(p + 4) = y;
        *reinterpret_cast<float*>(p + 8) = thickness;
        p += stride;
    }
    if (!hasInner) {
        *reinterpret_cast<float*>(p) = 0.0f;
        *reinterpret_cast<float*>(p + 4) = 0.0f;
        *reinterpret_cast<float*>(p + 8) = 0.0f;
        p += stride;
        *reinterpret_cast<float*>(p) = 0.0f;
        *reinterpret_cast<float*>(p + 4) = 0.0f;
        *reinterpret_cast<float*>(p + 8) = thickness;
    } else {
        for (int i = 0; i < segments; ++i) {
            const float a = FMul(static_cast<float>(i), step);
            const float x = FMul(FSin(a), height);
            const float y = FMul(FCos(a), height);
            *reinterpret_cast<float*>(p) = x;
            *reinterpret_cast<float*>(p + 4) = y;
            *reinterpret_cast<float*>(p + 8) = 0.0f;
            p += stride;
            *reinterpret_cast<float*>(p) = x;
            *reinterpret_cast<float*>(p + 4) = y;
            *reinterpret_cast<float*>(p + 8) = thickness;
            p += stride;
        }
    }

    if (!wire)
        BuildIndicesSolid(segments, static_cast<int>(vertexCount), chunk->GetIndexArray(), 0);
    else
        BuildIndicesWire(segments, static_cast<int>(vertexCount), chunk->GetIndexArray(), 0);
    WriteColor(chunk, vertexCount, packed);
    Finish(chunk, kernel_);
    return true;
}

// 0x680e0
bool __cdecl RenderPolyline::BuildGeometry(RenderKernel* kernel_, const Color& value, const Vector3* points,
                                           int count, int creationFlags_)
{
    const unsigned long packed = Display::ConvertColor(value);
    float minX = 3.40282347e+38f, minY = minX, minZ = minX;
    float maxX = -3.40282347e+38f, maxY = maxX, maxZ = maxX;
    for (int i = 0; i < count; ++i) {
        const Vector3& v = points[i];
        if (v.x < minX) minX = v.x;
        if (maxX < v.x) maxX = v.x;
        if (v.y < minY) minY = v.y;
        if (maxY < v.y) maxY = v.y;
        if (v.z < minZ) minZ = v.z;
        if (maxZ < v.z) maxZ = v.z;
    }
    center.y = FMul(FAdd(maxY, minY), 0.5f);
    center.z = FMul(FAdd(maxZ, minZ), 0.5f);
    center.x = FMul(FAdd(maxX, minX), 0.5f);
    const float dx = FSub(maxX, center.x);
    const float dy = FSub(maxY, center.y);
    const float dz = FSub(maxZ, center.z);
    radius = ApproxLengthSquared(FAdd(FAdd(FMul(dx, dx), FMul(dy, dy)), FMul(dz, dz)));

    StandardChunkHint hint;
    MakeHint(hint);
    GeometryChunk* const chunk = CreateGeometryChunk(static_cast<unsigned int>(count),
                                                     static_cast<unsigned int>(count - 1),
                                                     static_cast<unsigned int>(count), 0,
                                                     static_cast<Material::GeometryType>(4), &hint);
    unsigned short* const idx = chunk->GetIndexArray();
    for (int i = 0; i < count; ++i) idx[i] = static_cast<unsigned short>(i);
    WriteColor(chunk, static_cast<unsigned int>(count), packed);
    unsigned int stride = 0;
    char* p = PositionView(chunk, stride);
    for (int i = 0; i < count; ++i) {
        std::memcpy(p, &points[i], sizeof(Vector3));
        p += stride;
    }
    Finish(chunk, kernel_);
    return true;
}

// 0x67d20
bool __cdecl RenderPolygon::BuildGeometry(RenderKernel* kernel_, const Color& value, const Polygon& polygon,
                                          int creationFlags_)
{
    if (polygon.numVerts <= 2) return false;
    Polygon::operator=(polygon);
    const unsigned int n = static_cast<unsigned int>(this->numVerts);
    const unsigned long packed = Display::ConvertColor(value);
    StandardChunkHint hint;
    MakeHint(hint);

    GeometryChunk* chunk;
    if (creationFlags_ & 1) {
        chunk = CreateGeometryChunk(n + 1, n, n + 1, 0, static_cast<Material::GeometryType>(4), &hint);
        unsigned short* const idx = chunk->GetIndexArray();
        unsigned int stride = 0;
        char* p = PositionView(chunk, stride);
        for (unsigned int i = 0; i < n; ++i) {
            std::memcpy(p, &this->verts[i], sizeof(Vector3));
            idx[i] = static_cast<unsigned short>(i);
            p += stride;
        }
        idx[n] = 0;
    } else if (creationFlags_ & 2) {
        const unsigned int primitives = n * 2 - 3;
        chunk = CreateGeometryChunk(n, primitives, primitives * 2, 0,
                                    static_cast<Material::GeometryType>(3), &hint);
        unsigned short* idx = chunk->GetIndexArray();
        unsigned int stride = 0;
        char* p = PositionView(chunk, stride);
        for (unsigned int i = 0; i < n; ++i) {
            std::memcpy(p, &this->verts[i], sizeof(Vector3));
            idx[0] = static_cast<unsigned short>(i);
            idx[1] = static_cast<unsigned short>(i < n - 1 ? i + 1 : 0);
            idx += 2;
            p += stride;
        }
        for (unsigned int i = 2; i < n - 1; ++i) {
            idx[0] = 0;
            idx[1] = static_cast<unsigned short>(i);
            idx += 2;
        }
    } else {
        chunk = CreateGeometryChunk(n, n - 2, n, 0, static_cast<Material::GeometryType>(2), &hint);
        unsigned short* const idx = chunk->GetIndexArray();
        unsigned int stride = 0;
        char* p = PositionView(chunk, stride);
        for (unsigned int i = 0; i < n; ++i) {
            std::memcpy(p, &this->verts[i], sizeof(Vector3));
            idx[i] = static_cast<unsigned short>(i);
            p += stride;
        }
    }
    WriteColor(chunk, n, packed);
    Finish(chunk, kernel_);
    return true;
}

}  // namespace Jet
