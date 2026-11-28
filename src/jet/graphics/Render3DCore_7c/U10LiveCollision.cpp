// Unit U10 wave 5: collision entry points. TestCollisionGeometry on the 2D/3D bases is the original's (creation-flag
// gate, the shape test, then the ray parameter comparison). The cone/cylinder/tube ray tests are transcribed from
// their assembly (0x636d0 / 0x64470 / 0x6ab60).
#include <jet/Render3DCorePrimitives.h>
#include <jet/Render3DCoreLines.h>

#include <windows.h>

#include <cmath>
#include <cstring>
#include <xmmintrin.h>

namespace Jet {

namespace {

constexpr float kEps = 1.1920929e-7f;

float FAdd(float a, float b) { return _mm_cvtss_f32(_mm_add_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }
float FSub(float a, float b) { return _mm_cvtss_f32(_mm_sub_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }
float FMul(float a, float b) { return _mm_cvtss_f32(_mm_mul_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }
float FDiv(float a, float b) { return _mm_cvtss_f32(_mm_div_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }
float FSqrt(float a) { return _mm_cvtss_f32(_mm_sqrt_ss(_mm_load_ss(&a))); }

bool CmpGT(float a, float b) { return _mm_comigt_ss(_mm_load_ss(&a), _mm_load_ss(&b)) != 0; }
bool CmpGE(float a, float b) { return _mm_comige_ss(_mm_load_ss(&a), _mm_load_ss(&b)) != 0; }
bool CmpLT(float a, float b) { return _mm_comilt_ss(_mm_load_ss(&a), _mm_load_ss(&b)) != 0; }
bool CmpLE(float a, float b) { return _mm_comile_ss(_mm_load_ss(&a), _mm_load_ss(&b)) != 0; }

// 0x8e80 / 0x8e10: direction * t, then point + that
void SetPoint(Vector3& out, const Vector3& origin, const Vector3& dir, float t)
{
    const float x = FMul(dir.x, t);
    const float y = FMul(dir.y, t);
    const float z = FMul(dir.z, t);
    out.x = FAdd(origin.x, x);
    out.y = FAdd(origin.y, y);
    out.z = FAdd(origin.z, z);
}

}  // namespace

// 0x5fb90
bool __cdecl Render2DPrimitive::TestCollisionGeometry(const Ray& ray, float t, Vector3& point,
                                                      ConstFaceDescriptor&) const
{
    if ((creationFlags & 8) == 0) return false;
    const float dirZ = ray.direction.z;
    const float pointZ = ray.point.z;
    if (dirZ == 0.0f && pointZ == 0.0f) {
        point = ray.point;
    } else {
        if ((creationFlags & 4) == 0 && CmpGT(dirZ, -kEps)) return false;
        const float lhs = std::fabs(FMul(pointZ, kEps));
        const float rhs = std::fabs(dirZ);
        if (CmpGT(lhs, rhs)) return false;
        if ((pointZ > 0.0f) == (dirZ > 0.0f)) return false;
        const float tt = FMul(FDiv(-1.0f, dirZ), pointZ);
        SetPoint(point, ray.point, ray.direction, tt);
    }
    using InsideFn = bool(__cdecl*)(const Render2DPrimitive*, const Vector3*);
    void** const table2 = *reinterpret_cast<void** const*>(const_cast<Render2DPrimitive*>(this));
    const InsideFn inside = reinterpret_cast<InsideFn>(table2[0xec / 4]);
    if (!inside || !inside(this, &point)) return false;
    const float dx = FSub(point.x, ray.point.x);
    const float dy = FSub(point.y, ray.point.y);
    const float dz = FSub(point.z, ray.point.z);
    const float dot = FAdd(FAdd(FMul(ray.direction.z, dz), FMul(ray.direction.y, dy)),
                           FMul(ray.direction.x, dx));
    return CmpGT(t, dot);
}

// the base class answers no hit (the vtable's shared zero stub)
bool __cdecl Render3DPrimitive::GeometryTestCollision(const Ray&, Vector3&) const
{
    return false;
}

// 0x60bf0
bool __cdecl Render3DPrimitive::TestCollisionGeometry(const Ray& ray, float t, Vector3& point,
                                                      ConstFaceDescriptor&) const
{
    if ((creationFlags & 8) == 0) return false;
    using GeometryFn = bool(__cdecl*)(const Render3DPrimitive*, const Ray*, Vector3*);
    void** const table = *reinterpret_cast<void** const*>(const_cast<Render3DPrimitive*>(this));
    const GeometryFn geometry = reinterpret_cast<GeometryFn>(table[0xec / 4]);
    const bool g = geometry && geometry(this, &ray, &point);
    if (!g) return false;
    const float dz = FSub(point.z, ray.point.z);
    const float dy = FSub(point.y, ray.point.y);
    const float dx = FSub(point.x, ray.point.x);
    const float dot = FAdd(FAdd(FMul(ray.direction.z, dz), FMul(ray.direction.y, dy)),
                           FMul(ray.direction.x, dx));
    return CmpGT(t, dot);
}

// ---- cone / cylinder / tube: the original implementations by RVA (transcription follows) ------------------------

bool __cdecl RenderCone::GeometryTestCollision(const Ray& ray, Vector3& point) const
{
    const float sx = scale.x, sy = scale.y, sz = scale.z;
    const float dx = FDiv(ray.direction.x, sx);
    const float dy = FDiv(ray.direction.y, sy);
    const float dz = FDiv(ray.direction.z, sz);
    const float px = FDiv(ray.point.x, sx);
    const float py = FDiv(ray.point.y, sy);
    const float pz = FDiv(ray.point.z, sz);
    const float r = radius;
    const float h = height;
    if (CmpGT(pz, h) && CmpLE(dz, -kEps)) return false;
    if (CmpLT(pz, 0.0f) && CmpLE(dz, kEps)) return false;

    const float k = FDiv(FMul(r, r), FMul(h, h));
    const float a = FSub(FAdd(FMul(dx, dx), FMul(dy, dy)), FMul(k, FMul(dz, dz)));
    const float b = FMul(FSub(FAdd(FMul(dx, px), FMul(dy, py)), FMul(k, FMul(dz, pz))), 2.0f);
    const float c = FSub(FAdd(FMul(px, px), FMul(py, py)), FMul(k, FMul(pz, pz)));
    const bool inside = CmpLT(c, 0.0f) && CmpGE(pz, 0.0f) && CmpGE(h, pz);
    bool bHit = false;
    if (inside) {
        if ((creationFlags & 4) == 0) return false;
        bHit = true;
    }

    float best = -1.0f;
    float bestT = 0.0f;
    auto consider = [&](float t) {
        if (CmpLT(t, 0.0f)) return;
        if (best < 0.0f || (bHit ? CmpGT(t, bestT) : CmpLT(t, bestT))) {
            best = t;
            bestT = t;
        }
    };
    if (CmpGT(std::fabs(a), kEps)) {
        const float disc = FSub(FMul(b, b), FMul(FMul(a, c), 4.0f));
        if (CmpGE(disc, 0.0f)) {
            const float sq = FSqrt(disc);
            const float den = FMul(a, 2.0f);
            const float t0 = FDiv(FSub(FSub(-0.0f, b), sq), den);
            const float t1 = FDiv(FSub(sq, b), den);
            const float roots[2] = {t0, t1};
            for (float t : roots) {
                if (CmpLT(t, 0.0f)) continue;
                const float z = FAdd(pz, FMul(dz, t));
                if (CmpGE(z, 0.0f) && CmpGE(h, z)) consider(t);
            }
        }
    } else if (CmpGE(std::fabs(b), kEps)) {
        const float t = FDiv(FSub(-0.0f, c), b);
        if (CmpGE(t, 0.0f)) {
            const float z = FAdd(pz, FMul(dz, t));
            if (CmpGE(z, 0.0f) && CmpGE(h, z)) consider(t);
        }
    }
    // base disc at z = h
    if (CmpGT(std::fabs(dz), kEps)) {
        const float t = FDiv(FSub(h, pz), dz);
        if (CmpGE(t, 0.0f)) {
            const float x = FAdd(px, FMul(dx, t));
            const float y = FAdd(py, FMul(dy, t));
            if (CmpGE(FMul(r, r), FAdd(FMul(x, x), FMul(y, y)))) consider(t);
        }
    }
    if (best < 0.0f) return false;
    SetPoint(point, ray.point, ray.direction, bestT);
    return true;
}

bool __cdecl RenderCylinder::GeometryTestCollision(const Ray& ray, Vector3& point) const
{
    const float sx = scale.x, sy = scale.y, sz = scale.z;
    const float dx = FDiv(ray.direction.x, sx);
    const float dy = FDiv(ray.direction.y, sy);
    const float dz = FDiv(ray.direction.z, sz);
    const float px = FDiv(ray.point.x, sx);
    const float py = FDiv(ray.point.y, sy);
    const float pz = FDiv(ray.point.z, sz);
    const float r = radius;
    const float h = height;
    if (CmpLT(h, pz) && CmpGT(dz, -kEps)) return false;
    if (CmpLT(pz, 0.0f) && CmpLT(dz, kEps)) return false;

    const float a = FAdd(FMul(dx, dx), FMul(dy, dy));
    const float c = FSub(FAdd(FMul(px, px), FMul(py, py)), FMul(r, r));
    const bool inside = CmpGE(pz, 0.0f) && CmpGE(h, pz) && CmpLT(c, 0.0f);
    bool bHit = false;
    if (inside) {
        if ((creationFlags & 4) == 0) return false;
        bHit = true;
    }

    float best = -1.0f;
    float bestT = 0.0f;
    auto consider = [&](float t) {
        if (CmpLT(t, 0.0f)) return;
        if (best < 0.0f || (bHit ? CmpGT(t, bestT) : CmpLT(t, bestT))) {
            best = t;
            bestT = t;
        }
    };

    // side wall
    if (CmpGT(a, 0.0f)) {
        const float b = FMul(FAdd(FMul(dx, px), FMul(dy, py)), 2.0f);
        const float disc = FSub(FMul(b, b), FMul(FMul(a, c), 4.0f));
        if (CmpGE(disc, 0.0f)) {
            const float sq = FSqrt(disc);
            const float den = FMul(a, 2.0f);
            const float t0 = FDiv(FSub(FSub(-0.0f, b), sq), den);
            const float t1 = FDiv(FSub(sq, b), den);
            const float roots[2] = {t0, t1};
            for (float t : roots) {
                if (CmpLT(t, 0.0f)) continue;
                const float z = FAdd(pz, FMul(dz, t));
                if (CmpGE(z, 0.0f) && CmpGE(h, z)) consider(t);
            }
        }
    }
    // caps
    if (CmpGT(std::fabs(dz), kEps)) {
        const float t0 = FDiv(FSub(-0.0f, pz), dz);
        if (CmpGE(t0, 0.0f)) {
            const float x = FAdd(px, FMul(dx, t0));
            const float y = FAdd(py, FMul(dy, t0));
            if (CmpGE(FMul(r, r), FAdd(FMul(x, x), FMul(y, y)))) consider(t0);
        }
        const float t1 = FDiv(FSub(h, pz), dz);
        if (CmpGE(t1, 0.0f)) {
            const float x = FAdd(px, FMul(dx, t1));
            const float y = FAdd(py, FMul(dy, t1));
            if (CmpGE(FMul(r, r), FAdd(FMul(x, x), FMul(y, y)))) consider(t1);
        }
    }
    if (best < 0.0f) return false;
    SetPoint(point, ray.point, ray.direction, bestT);
    return true;
}

bool __cdecl RenderTube::GeometryTestCollision(const Ray& ray, Vector3& point) const
{
    const float sx = scale.x, sy = scale.y, sz = scale.z;
    const float dx = FDiv(ray.direction.x, sx);
    const float dy = FDiv(ray.direction.y, sy);
    const float dz = FDiv(ray.direction.z, sz);
    const float px = FDiv(ray.point.x, sx);
    const float py = FDiv(ray.point.y, sy);
    const float pz = FDiv(ray.point.z, sz);
    const float outer = radius;
    const float inner = (height > 0.0f) ? height : 0.0f;
    const float len = thickness;
    const float outer2 = FMul(outer, outer);
    const float inner2 = FMul(inner, inner);
    const float a = FAdd(FMul(dx, dx), FMul(dy, dy));
    const float cOuter = FSub(FAdd(FMul(px, px), FMul(py, py)), outer2);
    const bool inside = CmpGE(pz, 0.0f) && CmpGE(len, pz) && CmpLT(cOuter, 0.0f);
    bool bHit = false;
    if (inside) {
        if ((creationFlags & 4) == 0) return false;
        bHit = true;
    }

    float best = -1.0f;
    float bestT = 0.0f;
    auto consider = [&](float t) {
        if (CmpLT(t, 0.0f)) return;
        if (best < 0.0f || (bHit ? CmpGT(t, bestT) : CmpLT(t, bestT))) {
            best = t;
            bestT = t;
        }
    };
    auto wall = [&](float rr, float rr2, bool skipPositive) {
        if (!CmpGT(a, 0.0f)) return;
        const float b = FMul(FAdd(FMul(dx, px), FMul(dy, py)), 2.0f);
        const float c = FSub(FAdd(FMul(px, px), FMul(py, py)), rr2);
        const float disc = FSub(FMul(b, b), FMul(FMul(a, c), 4.0f));
        if (CmpLT(disc, 0.0f)) return;
        const float sq = FSqrt(disc);
        const float den = FMul(a, 2.0f);
        const float t0 = FDiv(FSub(FSub(-0.0f, b), sq), den);
        const float t1 = FDiv(FSub(sq, b), den);
        const float roots[2] = {t0, t1};
        for (float t : roots) {
            if (CmpLT(t, 0.0f)) continue;
            if (skipPositive && CmpGT(t, 0.0f) && CmpLT(rr, outer)) continue;
            const float z = FAdd(pz, FMul(dz, t));
            if (CmpGE(z, 0.0f) && CmpGE(len, z)) consider(t);
        }
    };
    wall(outer, outer2, false);
    if (CmpGT(inner, 0.0f)) wall(inner, inner2, false);
    // end caps: full disc for a solid tube, annulus for a hollow one
    if (CmpGT(std::fabs(dz), kEps)) {
        const float caps[2] = {0.0f, len};
        for (float zc : caps) {
            const float t = FDiv(FSub(zc, pz), dz);
            if (CmpLT(t, 0.0f)) continue;
            const float x = FAdd(px, FMul(dx, t));
            const float y = FAdd(py, FMul(dy, t));
            const float rr = FAdd(FMul(x, x), FMul(y, y));
            if (CmpGE(outer2, rr) && (CmpLE(inner, 0.0f) || CmpGE(rr, inner2))) consider(t);
        }
    }
    if (best < 0.0f) return false;
    SetPoint(point, ray.point, ray.direction, bestT);
    return true;
}

}  // namespace Jet

