// Unit U10 remainder: the 3D shapes' ray tests and the 2D/3D bases' default-constructor closures.
//
// The ray tests follow the original's scaled-space algebra: the ray is divided by the shape's scale, tested against a
// unit sphere/box/cylinder, and the hit point is rebuilt as `ray.point + ray.direction * t`. Operand order and the
// SSE destination are kept from the assembly (NaN payloads and the `0 x` comparison directions matter); `t` is
// compared and chosen exactly as the original does.
#include <jet/Render3DCorePrimitives.h>

#include <jet/Render3DCoreLines.h>

#include <cmath>
#include <cstring>
#include <new>

#include <xmmintrin.h>

namespace Jet {

namespace {

inline float FAdd(float a, float b)
{
    __m128 v = _mm_add_ss(_mm_load_ss(&a), _mm_load_ss(&b));
    float r;
    _mm_store_ss(&r, v);
    return r;
}

inline float FSub(float a, float b)
{
    __m128 v = _mm_sub_ss(_mm_load_ss(&a), _mm_load_ss(&b));
    float r;
    _mm_store_ss(&r, v);
    return r;
}

inline float FMul(float a, float b)
{
    __m128 v = _mm_mul_ss(_mm_load_ss(&a), _mm_load_ss(&b));
    float r;
    _mm_store_ss(&r, v);
    return r;
}

inline float FDiv(float a, float b)
{
    __m128 v = _mm_div_ss(_mm_load_ss(&a), _mm_load_ss(&b));
    float r;
    _mm_store_ss(&r, v);
    return r;
}

// scaled direction/point: (1 / scale) * value, as the original's two instructions
inline float ScaleInverse(float scale, float value)
{
    const float one = 1.0f;
    return FMul(FDiv(one, scale), value);
}

inline void SetPoint(Vector3& out, float px, float py, float pz, float dx, float dy, float dz, float t)
{
    // FUN_10008e80 = direction * t, FUN_10008e10 = that + point (destination is the product)
    const float ax = FMul(dx, t);
    const float ay = FMul(dy, t);
    const float az = FMul(dz, t);
    out.x = FAdd(ax, px);
    out.y = FAdd(ay, py);
    out.z = FAdd(az, pz);
}

// the near positive root of the scaled quadratic, as RenderSphere/RenderEllipsoid choose it
inline bool CmpGT(float a, float b) { return _mm_comigt_ss(_mm_load_ss(&a), _mm_load_ss(&b)) != 0; }
inline bool CmpGE(float a, float b) { return _mm_comige_ss(_mm_load_ss(&a), _mm_load_ss(&b)) != 0; }

bool PickRoot(float a, float b, float c, float disc, float& t)
{
    if (CmpGT(0.0f, disc)) return false;
    const float den = FMul(a, 2.0f);
    if (disc == 0.0f) {
        t = FSub(-0.0f, FDiv(b, den));
        return !CmpGE(0.0f, t);
    }
    const float sq = std::sqrt(disc);
    const float t1 = FDiv(FSub(FSub(-0.0f, b), sq), den);
    const float t2 = FDiv(FSub(sq, b), den);
    if (CmpGT(0.0f, t2)) return false;
    t = CmpGT(0.0f, t1) ? t2 : t1;
    return true;
}

}  // namespace

// 0x62760
bool RenderBox::GeometryTestCollision(const Ray& ray, Vector3& point) const
{
    const float hx = FMul(extents.x, scale.x);
    const float hy = FMul(extents.y, scale.y);
    const float hz = FMul(extents.z, scale.z);
    const float px = ray.point.x;
    const float py = ray.point.y;
    const float pz = ray.point.z;
    const float dx = ray.direction.x;
    const float dy = ray.direction.y;
    const float dz = ray.direction.z;
    const float eps = FMul(FAdd(FAdd(std::fabs(hz), std::fabs(hx)), std::fabs(hy)), 1.0e-5f);
    const float nx = FSub(-0.0f, hx);
    const float ny = FSub(-0.0f, hy);
    const float nz = FSub(-0.0f, hz);
    float t = -1.0f;

    if (px <= nx || hx <= px || py <= ny || hy <= py || pz <= nz || hz <= pz) {
        float ad = std::fabs(dx);
        if (eps < ad) {
            if (nx < px) {
                if (hx <= px) t = FDiv(FSub(px, hx), ad);
            } else {
                t = FDiv(FSub(nx, px), ad);
            }
        }
        ad = std::fabs(dy);
        if (eps < ad) {
            if (ny < py) {
                if (hy <= py) {
                    const float v = FDiv(FSub(py, hy), ad);
                    if (t < v) t = v;
                }
            } else {
                const float v = FDiv(FSub(ny, py), ad);
                if (t < v) t = v;
            }
        }
        ad = std::fabs(dz);
        if (eps < ad) {
            if (nz < pz) {
                if (hz <= pz) {
                    const float v = FDiv(FSub(pz, hz), ad);
                    if (t < v) t = v;
                }
            } else {
                const float v = FDiv(FSub(nz, pz), ad);
                if (t < v) t = v;
            }
        }
    } else {
        if ((creationFlags & 4) == 0) return false;
        float ad = std::fabs(dx);
        if (eps < ad) {
            if (dx <= 0.0f)
                t = FDiv(FAdd(px, hx), ad);
            else
                t = FDiv(FSub(hx, px), ad);
        }
        ad = std::fabs(dy);
        if (eps < ad) {
            if (dy <= 0.0f) {
                const float v = FDiv(FAdd(py, hy), ad);
                if (t < 0.0f || v < t) t = v;
            } else {
                const float v = FDiv(FSub(hy, py), ad);
                if (t < 0.0f || v < t) t = v;
            }
        }
        ad = std::fabs(dz);
        if (eps < ad) {
            if (dz <= 0.0f) {
                const float v = FDiv(FAdd(pz, hz), ad);
                if (t < 0.0f || v < t) t = v;
            } else {
                const float v = FDiv(FSub(hz, pz), ad);
                if (t < 0.0f || v < t) t = v;
            }
        }
    }
    if (0.0f <= t) {
        const float ox = FAdd(px, FMul(dx, t));
        const float oy = FAdd(py, FMul(dy, t));
        const float oz = FAdd(pz, FMul(dz, t));
        point.x = ox;
        point.y = oy;
        point.z = oz;
        if (FSub(nx, eps) <= ox && ox <= FAdd(hx, eps) && FSub(ny, eps) <= oy && oy <= FAdd(hy, eps) &&
            FSub(nz, eps) <= oz && oz <= FAdd(hz, eps))
            return true;
    }
    return false;
}

// 0x697e0
bool RenderSphere::GeometryTestCollision(const Ray& ray, Vector3& point) const
{
    const float dx = ScaleInverse(scale.x, ray.direction.x);
    const float dy = ScaleInverse(scale.y, ray.direction.y);
    const float dz = ScaleInverse(scale.z, ray.direction.z);
    const float px = ScaleInverse(scale.x, ray.point.x);
    const float py = ScaleInverse(scale.y, ray.point.y);
    const float pz = ScaleInverse(scale.z, ray.point.z);

    const float a = FAdd(FAdd(FMul(dz, dz), FMul(dy, dy)), FMul(dx, dx));
    const float dot = FAdd(FAdd(FMul(dz, pz), FMul(dy, py)), FMul(dx, px));
    const float b = FMul(dot, 2.0f);
    const float c = FSub(FAdd(FAdd(FMul(pz, pz), FMul(py, py)), FMul(px, px)), FMul(radius, radius));
    const float disc = FSub(FMul(b, b), FMul(FMul(c, a), 4.0f));
    if (CmpGT(0.0f, disc)) return false;
    if ((creationFlags & 4) == 0 && CmpGE(0.0f, c)) return false;
    float t;
    if (!PickRoot(a, b, c, disc, t)) return false;
    SetPoint(point, ray.point.x, ray.point.y, ray.point.z, ray.direction.x, ray.direction.y, ray.direction.z, t);
    return true;
}

// 0x656d0
bool RenderEllipsoid::GeometryTestCollision(const Ray& ray, Vector3& point) const
{
    const float ix = FDiv(1.0f, FMul(radiusX, scale.x));
    const float iy = FDiv(1.0f, FMul(radiusY, scale.y));
    const float iz = FDiv(1.0f, FMul(radiusZ, scale.z));
    const float dx = FMul(ix, ray.direction.x);
    const float dy = FMul(iy, ray.direction.y);
    const float dz = FMul(iz, ray.direction.z);
    const float px = FMul(ix, ray.point.x);
    const float py = FMul(iy, ray.point.y);
    const float pz = FMul(iz, ray.point.z);

    const float c = FSub(FAdd(FAdd(FMul(pz, pz), FMul(py, py)), FMul(px, px)), 1.0f);
    const float dot = FAdd(FAdd(FMul(dz, pz), FMul(dy, py)), FMul(dx, px));
    const float b = FMul(dot, 2.0f);
    const float a = FAdd(FAdd(FMul(dz, dz), FMul(dy, dy)), FMul(dx, dx));
    const float disc = FSub(FMul(b, b), FMul(FMul(c, a), 4.0f));
    if (CmpGT(0.0f, disc)) return false;
    if ((creationFlags & 4) == 0 && CmpGE(0.0f, c)) return false;
    float t;
    if (!PickRoot(a, b, c, disc, t)) return false;
    SetPoint(point, ray.point.x, ray.point.y, ray.point.z, ray.direction.x, ray.direction.y, ray.direction.z, t);
    return true;
}

// ---- default constructor closures ------------------------------------------------------------------------------
// ??_F<Class> pushes the chunk-slot count (1) and calls the base constructor plus the class vftable, exactly like the
// exported int constructor. A __fastcall helper with an ignored second register matches `ret`-only thiscall.

extern "C" void __fastcall JetRender2DPrimitiveDefaultCtor(void* self, void*)
{
    ::new (self) Render2DPrimitive(1);
}
#pragma comment(linker, "/alternatename:??_FRender2DPrimitive@Jet@@QAEXXZ=_JetRender2DPrimitiveDefaultCtor@8")

extern "C" void __fastcall JetRender3DPrimitiveDefaultCtor(void* self, void*)
{
    ::new (self) Render3DPrimitive(1);
}
#pragma comment(linker, "/alternatename:??_FRender3DPrimitive@Jet@@QAEXXZ=_JetRender3DPrimitiveDefaultCtor@8")

}  // namespace Jet
