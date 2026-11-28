// Unit U10 of Render3DCore_7c: the debug shapes' constructors, instance helpers and the pure geometry queries
// (radius, centre, point test). BuildGeometry, Init and the ray tests are still the original's: they need the
// material manager (Init) or the material-created vertex chunk (BuildGeometry), which belong to U5/U6.
//
// Object layouts are in Render3DCorePrimitives.h. Ordinary members are __cdecl (this on the stack), constructors and
// the assignment __thiscall, the virtuals __cdecl.
#include <jet/Render3DCorePrimitives.h>

#include <windows.h>

#include <cmath>
#include <xmmintrin.h>
#include <cstring>
#include <new>

namespace Jet {

namespace {

float MaxScaleXY(const Vector3& s)
{
    float m = s.y;
    if (s.z > m) m = s.z;
    if (m > s.x) return m;
    return s.x;
}

// the original's fast square root: an inverse square root seed from the bit pattern and one Newton step, all in
// x87 extended precision, times the squared length. The instruction order is the original's (0x626d0), so the NaN
// payload selection matches too.
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

float Sq(float v)
{
    return v * v;
}

float MulSS(float a, float b)
{
    __m128 v = _mm_mul_ss(_mm_load_ss(&a), _mm_load_ss(&b));
    float r;
    _mm_store_ss(&r, v);
    return r;
}

// the original's two- and three-term squared sums, with the SSE destination left operand order
float SqSum2(float a, float b)
{
    __m128 va = _mm_mul_ss(_mm_load_ss(&a), _mm_load_ss(&a));
    __m128 vb = _mm_mul_ss(_mm_load_ss(&b), _mm_load_ss(&b));
    va = _mm_add_ss(va, vb);
    float r;
    _mm_store_ss(&r, va);
    return r;
}

float SqSum3(float a, float b, float c)
{
    __m128 va = _mm_mul_ss(_mm_load_ss(&a), _mm_load_ss(&a));
    __m128 vb = _mm_mul_ss(_mm_load_ss(&b), _mm_load_ss(&b));
    __m128 vc = _mm_mul_ss(_mm_load_ss(&c), _mm_load_ss(&c));
    va = _mm_add_ss(va, vb);
    va = _mm_add_ss(va, vc);
    float r;
    _mm_store_ss(&r, va);
    return r;
}

template <class T>
T* AllocateInstance(unsigned int size)
{
    void* const raw = Alloc::New(size);
    return raw ? ::new (raw) T() : nullptr;
}

}  // namespace

// ---- RenderBox --------------------------------------------------------------------------------------------------

RenderBox::RenderBox() : Render3DPrimitive(1), extents()
{
    extents.x = 0.0f;
    extents.y = 0.0f;
    extents.z = 0.0f;
}

RenderBox::RenderBox(const RenderBox& o) : Render3DPrimitive(o), extents()
{
    std::memcpy(&extents, &o.extents, sizeof extents);
}

RenderBox::~RenderBox() {}

RenderBox& RenderBox::operator=(const RenderBox& o)
{
    RenderPrimitive::operator=(o);
    std::memcpy(&extents, &o.extents, sizeof extents);
    return *this;
}

// 0x50480
Particle* RenderBox::CreateInstance() const
{
    RenderBox* const instance = AllocateInstance<RenderBox>(0x194);
    if (!instance) return nullptr;
    RenderPrimitive::InitInstance(*instance);
    std::memcpy(&instance->extents, &extents, sizeof extents);
    return instance;
}

// 0x50540
const Vector3& RenderBox::GetExtents() const
{
    return extents;
}

// 0x626d0
float RenderBox::GetGeometryRadius() const
{
    const float x = MulSS(extents.x, scale.x);
    const float y = MulSS(extents.y, scale.y);
    const float z = MulSS(extents.z, scale.z);
    return ApproxLengthSquared(SqSum3(z, x, y));
}

// 0x62b30
void RenderBox::InitInstance(RenderBox& instance) const
{
    RenderPrimitive::InitInstance(instance);
    std::memcpy(&instance.extents, &extents, sizeof extents);
}

// ---- RenderSphere -----------------------------------------------------------------------------------------------

RenderSphere::RenderSphere() : Render3DPrimitive(1), radius(0.0f) {}
RenderSphere::RenderSphere(const RenderSphere& o) : Render3DPrimitive(o), radius(o.radius) {}
RenderSphere::~RenderSphere() {}

RenderSphere& RenderSphere::operator=(const RenderSphere& o)
{
    RenderPrimitive::operator=(o);
    radius = o.radius;
    return *this;
}

// 0x512a0
Particle* RenderSphere::CreateInstance() const
{
    RenderSphere* const instance = AllocateInstance<RenderSphere>(0x18c);
    if (!instance) return nullptr;
    RenderPrimitive::InitInstance(*instance);
    instance->radius = radius;
    return instance;
}

// 0x51340
float RenderSphere::GetRadius() const
{
    return radius;
}

// 0x65960 (shared with RenderPolyline)
float RenderSphere::GetGeometryRadius() const
{
    float m = scale.y;
    if (scale.z > m) m = scale.z;
    if (m > scale.x) return radius * m;
    return radius * scale.x;
}

// 0x63050 (shared with RenderCircle)
void RenderSphere::InitInstance(RenderSphere& instance) const
{
    RenderPrimitive::InitInstance(instance);
    instance.radius = radius;
}

// ---- RenderCircle -----------------------------------------------------------------------------------------------

RenderCircle::RenderCircle() : Render2DPrimitive(1), radius(0.0f) {}
RenderCircle::RenderCircle(const RenderCircle& o) : Render2DPrimitive(o), radius(o.radius) {}
RenderCircle::~RenderCircle() {}

RenderCircle& RenderCircle::operator=(const RenderCircle& o)
{
    RenderPrimitive::operator=(o);
    radius = o.radius;
    return *this;
}

// 0x62c10
Particle* RenderCircle::CreateInstance() const
{
    RenderCircle* const instance = AllocateInstance<RenderCircle>(0x18c);
    if (!instance) return nullptr;
    RenderPrimitive::InitInstance(*instance);
    instance->radius = radius;
    return instance;
}

// 0x62bd0
float RenderCircle::GetGeometryRadius() const
{
    if (scale.y <= scale.x) return radius * scale.x;
    return radius * scale.y;
}

// 0x62fe0
bool RenderCircle::IsWithin(const Vector3& point) const
{
    const float rx = scale.x * radius;
    const float ry = scale.y * radius;
    return Sq(point.x) / Sq(rx) + Sq(point.y) / Sq(ry) <= 1.0f;
}

// 0x63050
void RenderCircle::InitInstance(RenderCircle& instance) const
{
    RenderPrimitive::InitInstance(instance);
    instance.radius = radius;
}

// ---- RenderEllipse ----------------------------------------------------------------------------------------------

RenderEllipse::RenderEllipse() : Render2DPrimitive(1), radiusX(0.0f), radiusY(0.0f) {}
RenderEllipse::RenderEllipse(const RenderEllipse& o)
    : Render2DPrimitive(o), radiusX(o.radiusX), radiusY(o.radiusY)
{
}
RenderEllipse::~RenderEllipse() {}

RenderEllipse& RenderEllipse::operator=(const RenderEllipse& o)
{
    RenderPrimitive::operator=(o);
    radiusX = o.radiusX;
    radiusY = o.radiusY;
    return *this;
}

// 0x64ad0
Particle* RenderEllipse::CreateInstance() const
{
    RenderEllipse* const instance = AllocateInstance<RenderEllipse>(0x190);
    if (!instance) return nullptr;
    RenderPrimitive::InitInstance(*instance);
    instance->radiusX = radiusX;
    instance->radiusY = radiusY;
    return instance;
}

// 0x64a90
float RenderEllipse::GetGeometryRadius() const
{
    const float x = radiusX * scale.x;
    const float y = radiusY * scale.y;
    if (y < x) return x;
    return y;
}

// 0x64ee0
bool RenderEllipse::IsWithin(const Vector3& point) const
{
    return Sq(point.x) / (radiusX * scale.x * radiusX * scale.x) +
               Sq(point.y) / (radiusY * scale.y * radiusY * scale.y) <=
           1.0f;
}

// 0x64f50 (shared with RenderPlane)
void RenderEllipse::InitInstance(RenderEllipse& instance) const
{
    RenderPrimitive::InitInstance(instance);
    instance.radiusX = radiusX;
    instance.radiusY = radiusY;
}

// ---- RenderCone -------------------------------------------------------------------------------------------------

RenderCone::RenderCone() : Render3DPrimitive(1), radius(0.0f), height(0.0f) {}
RenderCone::RenderCone(const RenderCone& o) : Render3DPrimitive(o), radius(o.radius), height(o.height) {}
RenderCone::~RenderCone() {}

RenderCone& RenderCone::operator=(const RenderCone& o)
{
    RenderPrimitive::operator=(o);
    radius = o.radius;
    height = o.height;
    return *this;
}

// 0x63110
Particle* RenderCone::CreateInstance() const
{
    RenderCone* const instance = AllocateInstance<RenderCone>(0x190);
    if (!instance) return nullptr;
    RenderPrimitive::InitInstance(*instance);
    instance->radius = radius;
    instance->height = height;
    return instance;
}

// 0x635c0
float RenderCone::GetGeometryRadius() const
{
    float m = scale.y;
    if (_mm_comige_ss(_mm_load_ss(&scale.x), _mm_load_ss(&scale.y))) m = scale.x;
    const float h = MulSS(height, scale.z);
    const float r = MulSS(radius, m);
    const __m128 vsh = _mm_mul_ss(_mm_load_ss(&scale.z), _mm_load_ss(&h));
    if (!_mm_comige_ss(_mm_load_ss(&r), vsh)) {
        // fall through to the division below
    } else {
        return r;
    }
    __m128 vr = _mm_load_ss(&r);
    vr = _mm_mul_ss(vr, vr);
    __m128 vh = _mm_load_ss(&h);
    vh = _mm_mul_ss(vh, vh);
    vr = _mm_add_ss(vr, vh);
    __m128 vd = _mm_mul_ss(_mm_load_ss(&h), _mm_set_ss(2.0f));
    vr = _mm_div_ss(vr, vd);
    float result;
    _mm_store_ss(&result, vr);
    return result;
}

// 0x63640
Vector3 RenderCone::GetGeometryCenter() const
{
    float m = scale.y;
    if (_mm_comige_ss(_mm_load_ss(&scale.x), _mm_load_ss(&scale.y))) m = scale.x;
    const float h = MulSS(height, scale.z);
    const float r = MulSS(radius, m);
    if (_mm_comilt_ss(_mm_load_ss(&r), _mm_load_ss(&h)))
        return Vector3{0.0f, 0.0f, (Sq(h) - Sq(r)) / (h * 2.0f)};
    return Vector3{0.0f, 0.0f, 0.0f};
}

// 0x63d80 (shared with RenderCylinder)
void RenderCone::InitInstance(RenderCone& instance) const
{
    RenderPrimitive::InitInstance(instance);
    instance.radius = radius;
    instance.height = height;
}

// ---- RenderCylinder ---------------------------------------------------------------------------------------------

RenderCylinder::RenderCylinder() : Render3DPrimitive(1), radius(0.0f), height(0.0f) {}
RenderCylinder::RenderCylinder(const RenderCylinder& o)
    : Render3DPrimitive(o), radius(o.radius), height(o.height)
{
}
RenderCylinder::~RenderCylinder() {}

RenderCylinder& RenderCylinder::operator=(const RenderCylinder& o)
{
    RenderPrimitive::operator=(o);
    radius = o.radius;
    height = o.height;
    return *this;
}

// 0x63df0
Particle* RenderCylinder::CreateInstance() const
{
    RenderCylinder* const instance = AllocateInstance<RenderCylinder>(0x190);
    if (!instance) return nullptr;
    RenderPrimitive::InitInstance(*instance);
    instance->radius = radius;
    instance->height = height;
    return instance;
}

// 0x643a0
float RenderCylinder::GetGeometryRadius() const
{
    float m = scale.y;
    if (_mm_comige_ss(_mm_load_ss(&scale.x), _mm_load_ss(&scale.y))) m = scale.x;
    const float h = MulSS(MulSS(height, scale.z), 0.5f);
    const float r = MulSS(radius, m);
    return ApproxLengthSquared(SqSum2(h, r));
}

// 0x64430
Vector3 RenderCylinder::GetGeometryCenter() const
{
    return Vector3{0.0f, 0.0f, MulSS(MulSS(height, scale.z), 0.5f)};
}

// 0x63d80
void RenderCylinder::InitInstance(RenderCylinder& instance) const
{
    RenderPrimitive::InitInstance(instance);
    instance.radius = radius;
    instance.height = height;
}

// ---- RenderEllipsoid --------------------------------------------------------------------------------------------

RenderEllipsoid::RenderEllipsoid() : Render3DPrimitive(1), radiusX(0.0f), radiusY(0.0f), radiusZ(0.0f) {}
RenderEllipsoid::RenderEllipsoid(const RenderEllipsoid& o)
    : Render3DPrimitive(o), radiusX(o.radiusX), radiusY(o.radiusY), radiusZ(o.radiusZ)
{
}
RenderEllipsoid::~RenderEllipsoid() {}

RenderEllipsoid& RenderEllipsoid::operator=(const RenderEllipsoid& o)
{
    RenderPrimitive::operator=(o);
    radiusX = o.radiusX;
    radiusY = o.radiusY;
    radiusZ = o.radiusZ;
    return *this;
}

// 0x65080
Particle* RenderEllipsoid::CreateInstance() const
{
    RenderEllipsoid* const instance = AllocateInstance<RenderEllipsoid>(0x194);
    if (!instance) return nullptr;
    RenderPrimitive::InitInstance(*instance);
    instance->radiusX = radiusX;
    instance->radiusY = radiusY;
    instance->radiusZ = radiusZ;
    return instance;
}

// 0x65020
float RenderEllipsoid::GetGeometryRadius() const
{
    float m = radiusY * scale.y;
    const float z = radiusZ * scale.z;
    const float x = radiusX * scale.x;
    if (m < z) m = z;
    if (x < m) return m;
    return x;
}

// 0x62b30 (shared with RenderBox)
void RenderEllipsoid::InitInstance(RenderEllipsoid& instance) const
{
    RenderPrimitive::InitInstance(instance);
    instance.radiusX = radiusX;
    instance.radiusY = radiusY;
    instance.radiusZ = radiusZ;
}

// ---- RenderPlane ------------------------------------------------------------------------------------------------

RenderPlane::RenderPlane() : Render2DPrimitive(1), width(0.0f), height(0.0f) {}
RenderPlane::RenderPlane(const RenderPlane& o) : Render2DPrimitive(o), width(o.width), height(o.height) {}
RenderPlane::~RenderPlane() {}

RenderPlane& RenderPlane::operator=(const RenderPlane& o)
{
    RenderPrimitive::operator=(o);
    width = o.width;
    height = o.height;
    return *this;
}

// 0x67070
Particle* RenderPlane::CreateInstance() const
{
    RenderPlane* const instance = AllocateInstance<RenderPlane>(0x190);
    if (!instance) return nullptr;
    RenderPrimitive::InitInstance(*instance);
    instance->width = width;
    instance->height = height;
    return instance;
}

// 0x67af0
float RenderPlane::GetGeometryRadius() const
{
    const float x = MulSS(width, scale.x);
    const float y = MulSS(height, scale.y);
    return ApproxLengthSquared(SqSum2(x, y));
}

// 0x67b70
bool RenderPlane::IsWithin(const Vector3& point) const
{
    if (!(std::fabs(point.x) <= width * scale.x)) return false;
    if (!(std::fabs(point.y) <= height * scale.y)) return false;
    return true;
}

// 0x64f50
void RenderPlane::InitInstance(RenderPlane& instance) const
{
    RenderPrimitive::InitInstance(instance);
    instance.width = width;
    instance.height = height;
}

// ---- RenderTube -------------------------------------------------------------------------------------------------

RenderTube::RenderTube() : Render3DPrimitive(1), radius(0.0f), height(0.0f), thickness(0.0f) {}
RenderTube::RenderTube(const RenderTube& o)
    : Render3DPrimitive(o), radius(o.radius), height(o.height), thickness(o.thickness)
{
}
RenderTube::~RenderTube() {}

RenderTube& RenderTube::operator=(const RenderTube& o)
{
    RenderPrimitive::operator=(o);
    radius = o.radius;
    height = o.height;
    thickness = o.thickness;
    return *this;
}

// 0x6a060
Particle* RenderTube::CreateInstance() const
{
    RenderTube* const instance = AllocateInstance<RenderTube>(0x194);
    if (!instance) return nullptr;
    RenderPrimitive::InitInstance(*instance);
    instance->radius = radius;
    instance->height = height;
    instance->thickness = thickness;
    return instance;
}

// 0x6aa90
float RenderTube::GetGeometryRadius() const
{
    float m = scale.y;
    if (_mm_comige_ss(_mm_load_ss(&scale.x), _mm_load_ss(&scale.y))) m = scale.x;
    const float h = MulSS(MulSS(thickness, scale.z), 0.5f);
    const float r = MulSS(radius, m);
    return ApproxLengthSquared(SqSum2(h, r));
}

// 0x6ab20
Vector3 RenderTube::GetGeometryCenter() const
{
    return Vector3{0.0f, 0.0f, MulSS(MulSS(thickness, scale.z), 0.5f)};
}

// 0x6b590
void RenderTube::InitInstance(RenderTube& instance) const
{
    RenderPrimitive::InitInstance(instance);
    instance.radius = radius;
    instance.height = height;
    instance.thickness = thickness;
}

// the plane builder and the cone/cylinder/tube ray tests live in U10LiveGeometry.cpp / U10LiveCollision.cpp

}  // namespace Jet
