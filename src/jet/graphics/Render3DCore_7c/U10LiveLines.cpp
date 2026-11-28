// Unit U10 wave 5: the RenderLines collectors (0x66540, 0x66670, 0x667c0, 0x65fa0, 0x66b20). Each Add makes a
// debug primitive through the local constructor and its Init, then sets its opacity (slot 0x40) to 0.3. Cull walks
// the line, polygon and sphere pools; spheres are culled with a model-view built from the particle's transform.
#include <jet/Render3DCorePrimitives.h>

#include <jet/Render3DCoreParticle.h>

#include <cstring>
#include <new>
#include <xmmintrin.h>

namespace Jet {

#ifdef LIVEPATCH_BUILD
bool RenderLines::enabled = false;
#endif

namespace {

float FAdd(float a, float b) { return _mm_cvtss_f32(_mm_add_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }
float FSub(float a, float b) { return _mm_cvtss_f32(_mm_sub_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }
float FMul(float a, float b) { return _mm_cvtss_f32(_mm_mul_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }

void EnsureSlots(RenderLines::Pool& pool, unsigned int needed)
{
    if (needed <= pool.capacity) return;
    const unsigned int items = (needed + 127u) / 128u * 128u;
    void** const block = static_cast<void**>(Alloc::New(items * 4));
    if (pool.data) {
        std::memcpy(block, pool.data, pool.count * 4);
        Alloc::Delete(pool.data);
    }
    pool.capacity = items;
    pool.data = block;
}

// 0x1ab30: row-major multiply, result = a * b. The add order is the original's SSE sequence.
void Multiply44(const float* a, const float* b, float* out)
{
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float v = FMul(a[i * 4 + 1], b[4 + j]);
            v = FAdd(v, FMul(a[i * 4 + 3], b[12 + j]));
            v = FAdd(v, FMul(a[i * 4 + 2], b[8 + j]));
            v = FAdd(v, FMul(a[i * 4 + 0], b[j]));
            out[i * 4 + j] = v;
        }
    }
}

void SphereModelView(Particle* particle, const Matrix4& m, Matrix4& out)
{
    const char* const p = reinterpret_cast<const char*>(particle);
    const float qx = *reinterpret_cast<const float*>(p + 0x38);
    const float qy = *reinterpret_cast<const float*>(p + 0x3c);
    const float qz = *reinterpret_cast<const float*>(p + 0x40);
    const float qw = *reinterpret_cast<const float*>(p + 0x44);
    const float two = 2.0f;
    float model[16];
    std::memset(model, 0, sizeof model);
    const float qx2 = FMul(FMul(qx, two), qx);
    const float qy2 = FMul(FMul(qy, two), qy);
    const float qz2 = FMul(FMul(qz, two), qz);
    const float qwq = FMul(qw, two);
    const float xy = FMul(qx, FMul(qy, two));
    const float xz = FMul(qx, FMul(qz, two));
    const float yz = FMul(qy, FMul(qz, two));
    const float wx = FMul(qwq, qx);
    const float wy = FMul(qwq, qy);
    const float wz = FMul(qwq, qz);
    model[0] = FSub(1.0f, FAdd(qx2, qy2));
    model[1] = FAdd(wz, xy);
    model[2] = FSub(xz, wy);
    model[4] = FSub(xy, wz);
    model[5] = FSub(1.0f, FAdd(qx2, qz2));
    model[6] = FAdd(wx, yz);
    model[8] = FAdd(wy, xz);
    model[9] = FSub(yz, wx);
    model[10] = FSub(1.0f, FAdd(qy2, qz2));
    model[15] = 1.0f;
    model[12] = *reinterpret_cast<const float*>(p + 0x2c);
    model[13] = *reinterpret_cast<const float*>(p + 0x30);
    model[14] = *reinterpret_cast<const float*>(p + 0x34);
    Multiply44(model, m.m, out.m);
}

template <class T>
T* Make()
{
    void* const raw = Alloc::New(sizeof(T));
    return raw ? ::new (raw) T() : nullptr;
}

void Grow(RenderLines::Pool& pool, unsigned int& liveCount)
{
    EnsureSlots(pool, pool.count + 1);
    pool.data[pool.count++] = nullptr;
    ++liveCount;
}

}  // namespace

// 0x66540
void __cdecl RenderLines::Add(RenderKernel* kernel_, const Vector3& a, const Vector3& b, const Color& color)
{
    if (!enabled) return;
    kernel = kernel_;
    if (lineCount >= lineCap) {
        RenderLine* const line = Make<RenderLine>();
        EnsureSlots(lines, lines.count + 1);
        lines.data[lines.count++] = line;
        ++lineCap;
    }
    RenderLine* const item = static_cast<RenderLine*>(lines.data[lineCount]);
    item->Init(kernel_, color, a, b, 0);
    item->SetOpacity(0.3f);
    ++lineCount;
}

// 0x66670
void __cdecl RenderLines::Add(RenderKernel* kernel_, const Vector3& point, const Color& color)
{
    if (!enabled) return;
    kernel = kernel_;
    if (sphereCount >= sphereCap) {
        RenderSphere* const sphere = Make<RenderSphere>();
        EnsureSlots(spheres, spheres.count + 1);
        spheres.data[spheres.count++] = sphere;
        ++sphereCap;
    }
    RenderSphere* const item = static_cast<RenderSphere*>(spheres.data[sphereCount]);
    item->SetColor(color);
    item->Init(kernel_, color, 0.4f, 16, 8);
    item->SetOpacity(0.3f);
    ++sphereCount;
}

// 0x667c0
void __cdecl RenderLines::Add(RenderKernel* kernel_, const Vector3* points, int count, const Color& color)
{
    if (!enabled) return;
    kernel = kernel_;
    if (polygonCount >= polygonCap) {
        RenderPolygon* const polygon = Make<RenderPolygon>();
        EnsureSlots(polygons, polygons.count + 1);
        polygons.data[polygons.count++] = polygon;
        ++polygonCap;
    }
    RenderPolygon* const item = static_cast<RenderPolygon*>(polygons.data[polygonCount]);
    item->Init(kernel_, color, count, points, 0xc);
    item->SetOpacity(0.3f);
    ++polygonCount;
}

// 0x65fa0
void __cdecl RenderLines::AddPolygon(RenderKernel* kernel_, const Vector3& a, const Vector3& b, const Vector3& c,
                                     const Vector3& d, const Color& color)
{
    const Vector3 points[4] = {a, b, c, d};
    Add(kernel_, points, 4, color);
}

// 0x66b20
void __cdecl RenderLines::Cull(ViewPoint& view, const Matrix4& m)
{
    if (!enabled) return;
    for (unsigned int i = 0; i < lines.count; ++i) {
        Particle* const item = static_cast<Particle*>(lines.data[i]);
        if (item) item->Cull(view, m);
    }
    for (unsigned int i = 0; i < polygons.count; ++i) {
        Particle* const item = static_cast<Particle*>(polygons.data[i]);
        if (item) item->Cull(view, m);
    }
    for (unsigned int i = 0; i < spheres.count; ++i) {
        Particle* const item = static_cast<Particle*>(spheres.data[i]);
        if (!item) continue;
        Matrix4 modelView;
        SphereModelView(item, m, modelView);
        item->Cull(view, modelView);
    }
}

}  // namespace Jet
