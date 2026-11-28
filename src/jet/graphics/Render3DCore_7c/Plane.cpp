// Unit U1b of Render3DCore_7c: Jet::Plane, transcribed from the TS12 assembly (addresses per member).
//
// The class is {Vector3 normal; float d;} = 0x10 bytes. Ordinary members are __cdecl (the this pointer is pushed on
// the stack as the original's callers do); constructors, the assignment and WhichSideAxisBox are __thiscall. Floats
// the original copies through x87 (fld/fstp) come out with a signalling NaN quieted, and its SSE arithmetic is kept
// in the same operand order (a helper pins destination and source; the compiler would otherwise commute them and
// pick a different NaN). The approximate vector normalize/length helpers (0x10008ab0/0x10008b50, the table-seeded
// two-step sqrt shared by the whole module) are reproduced with x87 exactly; the seed table is KernelStandard's
// exported StdMath::s_sqrtLUT, read at run time like the original's IAT entry.
#include <jet/Render3DCorePlane.h>

#include <windows.h>

#include <cstddef>
#include <cstring>

namespace Jet {

namespace {

using uint = unsigned int;

// addss/subss/mulss/divss with the original's destination (first operand) order.
float Add(float a, float b)
{
    __asm {
        movss xmm0, a
        addss xmm0, b
        movss a, xmm0
    }
    return a;
}

float Sub(float a, float b)
{
    __asm {
        movss xmm0, a
        subss xmm0, b
        movss a, xmm0
    }
    return a;
}

float Mul(float a, float b)
{
    __asm {
        movss xmm0, a
        mulss xmm0, b
        movss a, xmm0
    }
    return a;
}

float Div(float a, float b)
{
    __asm {
        movss xmm0, a
        divss xmm0, b
        movss a, xmm0
    }
    return a;
}

// a float through fld/fstp: a signalling NaN gets its quiet bit (no other bit changes)
float X87(float f)
{
    unsigned int u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) && !(u & 0x00400000u))
        u |= 0x00400000u;
    std::memcpy(&f, &u, 4);
    return f;
}

const float kHalf = 0.5f;
const double kThreeHalves = 1.5;

// StdMath::s_sqrtLUT (KernelStandard's exported table, read inline by the original) and the 1/sqrt seed of s
float Seed(float s)
{
    static const unsigned int* const lut = reinterpret_cast<const unsigned int*>([] {
        HMODULE m = GetModuleHandleA("KernelStandard_7c.dll");
        if (!m)
            m = LoadLibraryA("KernelStandard_7c.dll");
        return m ? GetProcAddress(m, "?s_sqrtLUT@StdMath@Jet@@0PAIA") : nullptr;
    }());
    unsigned int u;
    std::memcpy(&u, &s, 4);
    const unsigned int y =
        (((0x17cu - ((u >> 23) & 0xff)) << 22) & 0xff800000u) | (lut ? lut[(u >> 15) & 0x1ff] : 0u);
    float f;
    std::memcpy(&f, &y, 4);
    return f;
}

struct X87Value {  // an x87 register as fstp tbyte stores it
    unsigned char b[10];
};

// 0x10008ab0: |v| with two Newton steps, left unrounded (the caller divides 1 by it)
void LengthX87(const float* v, X87Value& out)
{
    const float s = Add(Add(Mul(v[0], v[0]), Mul(v[1], v[1])), Mul(v[2], v[2]));
    const float y = Seed(s);
    unsigned char* const o = out.b;
    __asm {
        fld     s
        fld     kHalf
        fmul    st, st(1)
        fld     y
        fld     st(0)
        fmul    st, st(1)
        fmul    st, st(2)
        fld     kThreeHalves
        fld     st(0)
        fsubrp  st(2), st
        fxch    st(1)
        fmulp   st(2), st
        fld     st(1)
        fmul    st, st(2)
        fmulp   st(3), st
        fsubrp  st(2), st
        fmulp   st(1), st
        fmulp   st(1), st
        mov     eax, o
        fstp    tbyte ptr [eax]
    }
}

// 0x10008b50: v scaled by its 1/length (two Newton steps), the factor rounded to float
void NormalizeX87(float* v)
{
    const float s = Add(Add(Mul(v[0], v[0]), Mul(v[1], v[1])), Mul(v[2], v[2]));
    const float y = Seed(s);
    float r;
    __asm {
        fld     s
        fmul    kHalf
        fld     y
        fld     st(0)
        fmul    st, st(1)
        fmul    st, st(2)
        fld     kThreeHalves
        fld     st(0)
        fsubrp  st(2), st
        fxch    st(1)
        fmulp   st(2), st
        fld     st(1)
        fmul    st, st(2)
        fmulp   st(3), st
        fsubrp  st(2), st
        fmulp   st(1), st
        fstp    r
    }
    v[0] = Mul(v[0], r);
    v[1] = Mul(v[1], r);
    v[2] = Mul(v[2], r);
}

// Line: vftable +0, direction +4, point +0x10 (0x1c bytes). Only read here.
struct LineFields {
    void* vftable;
    Vector3 direction;
    Vector3 point;
};
static_assert(offsetof(LineFields, direction) == 0x4, "Line direction");
static_assert(offsetof(LineFields, point) == 0x10, "Line point");
static_assert(sizeof(LineFields) == 0x1c, "Line size");

// LineSegment: a Line with the second point at +0x1c.
struct SegmentFields {
    void* vftable;
    Vector3 direction;
    Vector3 point;
    Vector3 end;
};
static_assert(offsetof(SegmentFields, end) == 0x1c, "LineSegment end");

// Sphere: vftable +0, centre +4, radius +0x10 (TS12 order, not the SDK header's).
struct SphereFields {
    void* vftable;
    Vector3 center;
    float radius;
    float radiusXY;
};
static_assert(offsetof(SphereFields, center) == 0x4, "Sphere centre");
static_assert(offsetof(SphereFields, radius) == 0x10, "Sphere radius");

const float kTolerance = 0.0001f;
const float kEps = 1.1920929e-07f;  // FLT_EPSILON, the intersection parallel test

// Common body of the four bool Intersect exports: kind 0 = line (infinite), 1 = ray (t >= 0), 2 = segment
// (t >= 0 and t*t <= |point - end|^2). The order and destination of every SSE operation is the original's.
bool IntersectLineCore(const Vector3& n, float d, bool twoSided, const Vector3& p, const Vector3& dir, Vector3& out,
                       float& t, int kind, const Vector3* end)
{
    const float f = Add(Add(Mul(dir.y, n.y), Mul(dir.z, n.z)), Mul(dir.x, n.x));
    if (!(f <= -kEps) && !(twoSided && f >= kEps))
        return false;
    const float dot = Add(Add(Mul(p.y, n.y), Mul(p.z, n.z)), Mul(n.x, p.x));
    t = Div(Sub(d, dot), f);
    if (kind != 0 && !(t >= 0.0f))
        return false;
    if (kind == 2) {
        const float dx = Sub(p.x, end->x);
        const float dy = Sub(p.y, end->y);
        const float dz = Sub(p.z, end->z);
        const float len2 = Add(Add(Mul(dy, dy), Mul(dx, dx)), Mul(dz, dz));
        if (!(len2 >= Mul(t, t)))
            return false;
    }
    out.x = Add(Mul(dir.x, t), p.x);
    out.y = Add(p.y, Mul(dir.y, t));
    out.z = Add(p.z, Mul(dir.z, t));
    return true;
}

}  // namespace

// 0x4fd0: the default constructor leaves the storage as it was (only `mov eax, ecx; ret`)
Plane::Plane()
{
}

// 0x1ec50: also the body of the assignment; the four copies go through x87
Plane::Plane(const Plane& o)
{
    normal.x = X87(o.normal.x);
    normal.y = X87(o.normal.y);
    normal.z = X87(o.normal.z);
    d = X87(o.d);
}

// 0x5030
Plane::Plane(const Vector3& n, float distance)
{
    normal.x = X87(n.x);
    normal.y = X87(n.y);
    normal.z = X87(n.z);
    d = distance;
}

// 0x4fe0: x and y through x87, z and the dot product in SSE
Plane::Plane(const Vector3& n, const Vector3& point)
{
    normal.x = X87(n.x);
    normal.y = X87(n.y);
    normal.z = n.z;
    d = Add(Add(Mul(point.z, normal.z), Mul(point.y, normal.y)), Mul(point.x, normal.x));
}

// 0x5060
Plane::Plane(const Vector3& p1, const Vector3& p2, const Vector3& p3)
{
    Set(p1, p2, p3);
}

// 0x1ec50
Plane& Plane::operator=(const Plane& o)
{
    normal.x = X87(o.normal.x);
    normal.y = X87(o.normal.y);
    normal.z = X87(o.normal.z);
    d = X87(o.d);
    return *this;
}

// 0x5090
void __cdecl Plane::Set(const Vector3& n, const Vector3& point)
{
    normal.x = X87(n.x);
    normal.y = X87(n.y);
    normal.z = n.z;
    d = Add(Add(Mul(point.z, normal.z), Mul(point.y, normal.y)), Mul(point.x, normal.x));
}

// 0x50e0
void __cdecl Plane::Set(const Vector3& n, float distance)
{
    normal.x = X87(n.x);
    normal.y = X87(n.y);
    normal.z = X87(n.z);
    d = distance;
}

// 0x5110: normal = (p1 - p2) x (p2 - p3), normalized, then d = p1 . normal. All differences and the cross are
// computed before the normal is written (the original loads p1..p3 into registers first), then the approximate
// normalize, then d reads p1 again -- which matters when p1 aliases this plane.
void __cdecl Plane::Set(const Vector3& p1, const Vector3& p2, const Vector3& p3)
{
    const float vx = Sub(p1.x, p2.x);
    const float vy = Sub(p1.y, p2.y);
    const float vz = Sub(p1.z, p2.z);
    const float ux = Sub(p2.x, p3.x);
    const float uy = Sub(p2.y, p3.y);
    const float uz = Sub(p2.z, p3.z);
    const float nx = Sub(Mul(vy, uz), Mul(vz, uy));
    const float ny = Sub(Mul(ux, vz), Mul(vx, uz));
    const float nz = Sub(Mul(vx, uy), Mul(vy, ux));
    normal.x = nx;
    normal.y = ny;
    normal.z = nz;
    NormalizeX87(&normal.x);
    d = Add(Add(Mul(p1.z, normal.z), Mul(p1.y, normal.y)), Mul(p1.x, normal.x));
}

// 0x5220
void __cdecl Plane::Set(const Vector3* threePoints)
{
    Set(threePoints[0], threePoints[1], threePoints[2]);
}

// 0x5200
void __cdecl Plane::Set(const Plane& plane)
{
    normal.x = X87(plane.normal.x);
    normal.y = X87(plane.normal.y);
    normal.z = X87(plane.normal.z);
    d = X87(plane.d);
}

// 0x5240
void __cdecl Plane::SetDistance(const Vector3& point)
{
    d = Add(Add(Mul(point.z, normal.z), Mul(point.y, normal.y)), Mul(point.x, normal.x));
}

// 0x5280
void __cdecl Plane::SetDistance(float distance)
{
    d = distance;
}

// 0x5290: 1 / approximate length, applied to all four fields in x87 (the factor stays unrounded)
Plane& __cdecl Plane::Normalize()
{
    X87Value len;
    LengthX87(&normal.x, len);
    const float* q = &normal.x;
    __asm {
        lea     eax, len
        fld     tbyte ptr [eax]
        fld1
        fdivrp  st(1), st
        mov     eax, q
        fld     dword ptr [eax]
        fmul    st, st(1)
        fstp    dword ptr [eax]
        fld     st(0)
        fmul    dword ptr [eax + 4]
        fstp    dword ptr [eax + 4]
        fld     st(0)
        fmul    dword ptr [eax + 8]
        fstp    dword ptr [eax + 8]
        fmul    dword ptr [eax + 0xc]
        fstp    dword ptr [eax + 0xc]
    }
    return *this;
}

// 0xb210: returns this (the normal is at +0)
const Vector3& __cdecl Plane::GetNormal() const
{
    return normal;
}

// 0x3480 (the shared component copy)
void __cdecl Plane::SetNormal(const Vector3& n)
{
    normal.x = X87(n.x);
    normal.y = X87(n.y);
    normal.z = X87(n.z);
}

// 0x5300
void __cdecl Plane::SetNormal(float x, float y, float z)
{
    normal.x = x;
    normal.y = y;
    normal.z = z;
}

// 0x5330
float __cdecl Plane::GetDistance() const
{
    return d;
}

// 0x5340: x and y as normal * d, z as d * normal (the original's operand order)
Vector3 __cdecl Plane::GetPoint() const
{
    return Vector3{Mul(normal.x, d), Mul(normal.y, d), Mul(d, normal.z)};
}

// 0x52c0
void __cdecl Plane::Flip()
{
    normal.x = Sub(-0.0f, normal.x);
    normal.y = Sub(-0.0f, normal.y);
    normal.z = Sub(-0.0f, normal.z);
    d = Sub(-0.0f, d);
}

// 0x53b0
ClipResult __cdecl Plane::WhichSide(const Vector3& p) const
{
    const float f = Sub(Add(Add(Mul(p.z, normal.z), Mul(p.y, normal.y)), Mul(p.x, normal.x)), d);
    if (f > kTolerance)
        return FRONT;
    if (f < -kTolerance)
        return BACK;
    return COINCIDENT;  // also a NaN
}

// 0x5410
ClipResult __cdecl Plane::WhichSide(const Vector3& p, float threshold) const
{
    const float f = Sub(Add(Add(Mul(p.z, normal.z), Mul(p.y, normal.y)), Mul(p.x, normal.x)), d);
    if (f > threshold)
        return FRONT;
    if (Sub(-0.0f, threshold) > f)
        return BACK;
    return COINCIDENT;  // also a NaN
}

// 0x5470: COINCIDENT when both sides occur, COPLANAR when all points are on the plane
ClipResult __cdecl Plane::WhichSide(int vertices, const Vector3* vertexArray) const
{
    bool front = false;
    bool back = false;
    for (int i = 0; i < vertices; ++i) {
        const Vector3& p = vertexArray[i];
        const float f = Sub(Add(Add(Mul(p.x, normal.x), Mul(p.z, normal.z)), Mul(p.y, normal.y)), d);
        if (f > kTolerance)
            front = true;
        else if (f < -kTolerance)
            back = true;
    }
    if (front)
        return back ? COINCIDENT : FRONT;
    return back ? BACK : COPLANAR;
}

// 0x5540
ClipResult __cdecl Plane::WhichSideSphere(const Vector3& center, float radius) const
{
    const float f = Sub(Add(Add(Mul(center.z, normal.z), Mul(center.y, normal.y)), Mul(center.x, normal.x)), d);
    if (!(f >= 0.0f))  // the NaN case takes this branch too (COMISS + JC)
        return (radius > Sub(-0.0f, f)) ? COINCIDENT : BACK;
    return (radius > f) ? COINCIDENT : FRONT;
}

// 0x551f0
ClipResult __cdecl Plane::WhichSide(const Sphere& sphere) const
{
    const SphereFields* s = reinterpret_cast<const SphereFields*>(&sphere);
    const float f = Sub(Add(Add(Mul(s->center.z, normal.z), Mul(s->center.y, normal.y)), Mul(s->center.x, normal.x)), d);
    if (!(f >= 0.0f))
        return (s->radius > Sub(-0.0f, f)) ? COINCIDENT : BACK;
    return (s->radius > f) ? COINCIDENT : FRONT;
}

// 0x5e60: distance of the centre, then the box radius |n.x|*e.x + |n.y|*e.y + |n.z|*e.z (x87, one rounding)
ClipResult __thiscall Plane::WhichSideAxisBox(const Vector3& center, const Vector3& extent, float& planeDistance,
                                              float& radius, float threshold) const
{
    planeDistance = Sub(Add(Add(Mul(center.z, normal.z), Mul(center.y, normal.y)), Mul(center.x, normal.x)), d);
    const float* n = &normal.x;
    const float* e = &extent.x;
    float r;
    __asm {
        mov     eax, n
        mov     ecx, e
        fld     dword ptr [eax + 8]
        fabs
        fmul    dword ptr [ecx + 8]
        fld     dword ptr [eax + 4]
        fabs
        fmul    dword ptr [ecx + 4]
        faddp   st(1), st
        fld     dword ptr [eax]
        fabs
        fmul    dword ptr [ecx]
        faddp   st(1), st
        fstp    r
    }
    radius = r;
    const float f = Add(r, threshold);
    if (f < planeDistance)
        return FRONT;
    if (Sub(-0.0f, f) > planeDistance)
        return BACK;
    return COINCIDENT;  // also a NaN
}

// 0x55b0: (d - normal . center) > radius
bool __cdecl Plane::IsBehind(const Vector3& center, float radius) const
{
    const float dot = Add(Add(Mul(center.z, normal.z), Mul(center.y, normal.y)), Mul(center.x, normal.x));
    return Sub(d, dot) > radius;
}

// 0x5600: (normal . center - d) > radius
bool __cdecl Plane::IsInFront(const Vector3& center, float radius) const
{
    const float f = Sub(Add(Add(Mul(center.z, normal.z), Mul(center.y, normal.y)), Mul(center.x, normal.x)), d);
    return f > radius;
}

// 0x5790: t = (d - point . normal) / (direction . normal), then point + t * direction
Vector3 __cdecl Plane::Intersect(const Line& line) const
{
    const LineFields* l = reinterpret_cast<const LineFields*>(&line);
    const float dot = Add(Add(Mul(l->point.y, normal.y), Mul(l->point.z, normal.z)), Mul(l->point.x, normal.x));
    const float den = Add(Add(Mul(l->direction.y, normal.y), Mul(l->direction.z, normal.z)), Mul(normal.x, l->direction.x));
    const float t = Div(Sub(d, dot), den);
    return Vector3{Add(Mul(l->direction.x, t), l->point.x), Add(Mul(l->direction.y, t), l->point.y),
                   Add(l->point.z, Mul(l->direction.z, t))};
}

// 0x5cc0: the same, with t returned; z is added the other way round in the original
Vector3 __cdecl Plane::Intersect(const Line& line, float& t) const
{
    const LineFields* l = reinterpret_cast<const LineFields*>(&line);
    const float dot = Add(Add(Mul(l->point.y, normal.y), Mul(l->point.z, normal.z)), Mul(l->point.x, normal.x));
    const float den = Add(Add(Mul(l->direction.y, normal.y), Mul(l->direction.z, normal.z)), Mul(normal.x, l->direction.x));
    t = Div(Sub(d, dot), den);
    return Vector3{Add(Mul(l->direction.x, t), l->point.x), Add(Mul(l->direction.y, t), l->point.y),
                   Add(Mul(l->direction.z, t), l->point.z)};
}

// 0x5de0: a line through point along the (approximate) normalized normal, intersected with this plane
Vector3 __cdecl Plane::Project(const Vector3& point) const
{
    Vector3 dir = normal;
    NormalizeX87(&dir.x);
    const float dot = Add(Add(Mul(point.y, normal.y), Mul(point.z, normal.z)), Mul(point.x, normal.x));
    const float den = Add(Add(Mul(dir.y, normal.y), Mul(dir.z, normal.z)), Mul(normal.x, dir.x));
    const float t = Div(Sub(d, dot), den);
    return Vector3{Add(Mul(dir.x, t), point.x), Add(Mul(dir.y, t), point.y), Add(point.z, Mul(dir.z, t))};
}

// 0x5dc0: (point . normal - d) in x87, returned unrounded
float __cdecl Plane::Distance(const Vector3& point) const
{
    const float* p = &point.x;
    const float* n = &normal.x;
    __asm {
        mov     ecx, p
        mov     eax, n
        fld     dword ptr [ecx + 8]
        fmul    dword ptr [eax + 8]
        fld     dword ptr [ecx + 4]
        fmul    dword ptr [eax + 4]
        faddp   st(1), st
        fld     dword ptr [ecx]
        fmul    dword ptr [eax]
        faddp   st(1), st
        fsub    dword ptr [eax + 0xc]
    }
}

// 0x5910 (the no-t variant at 0x5880 calls it with a local t)
bool __cdecl Plane::Intersect(bool twoSided, const Line& line, Vector3& out) const
{
    float t;
    return Intersect(twoSided, line, out, t);
}

bool __cdecl Plane::Intersect(bool twoSided, const Line& line, Vector3& out, float& t) const
{
    const LineFields* l = reinterpret_cast<const LineFields*>(&line);
    return IntersectLineCore(normal, d, twoSided, l->point, l->direction, out, t, 0, nullptr);
}

// 0x58b0 / 0x5a20
bool __cdecl Plane::Intersect(bool twoSided, const Ray& ray, Vector3& out) const
{
    float t;
    return Intersect(twoSided, ray, out, t);
}

bool __cdecl Plane::Intersect(bool twoSided, const Ray& ray, Vector3& out, float& t) const
{
    const LineFields* l = reinterpret_cast<const LineFields*>(&ray);
    return IntersectLineCore(normal, d, twoSided, l->point, l->direction, out, t, 1, nullptr);
}

// 0x58e0 / 0x5b40
bool __cdecl Plane::Intersect(bool twoSided, const LineSegment& segment, Vector3& out) const
{
    float t;
    return Intersect(twoSided, segment, out, t);
}

bool __cdecl Plane::Intersect(bool twoSided, const LineSegment& segment, Vector3& out, float& t) const
{
    const SegmentFields* s = reinterpret_cast<const SegmentFields*>(&segment);
    return IntersectLineCore(normal, d, twoSided, s->point, s->direction, out, t, 2, &s->end);
}

// 0x6260: TransformPlane with a local copy of the normal as the input vector and this plane as the output
void __cdecl Plane::Transform(const Matrix4& m, bool transform, float extra)
{
    const Vector3 n = normal;
    TransformPlane(m, transform, n, d, normal, d, extra);
}

// 0x62b0
void __cdecl Plane::SetFromTransformPlane(const Matrix4& m, bool transform, const Plane& src, float extra)
{
    TransformPlane(m, transform, src.normal, src.d, normal, d, extra);
}

// 0x5f10: transform=true takes the inverse transpose of the 3x3 part (cofactor matrix, then normalized) and
// recovers d from the transformed point normal*d; transform=false only rotates the normal and translates d.
// The out vector may alias the input vector: the original reads the input between the stores.
void __cdecl Plane::TransformPlane(const Matrix4& m, bool transform, const Vector3& v, float inD, Vector3& outN,
                                   float& outD, float extra)
{
    // row-major: row 0 at m[0..2], row 1 at m[4..6], row 2 at m[8..10], translation at m[12..14]
    const float m00 = m.m[0], m01 = m.m[1], m02 = m.m[2];
    const float m10 = m.m[4], m11 = m.m[5], m12 = m.m[6];
    const float m20 = m.m[8], m21 = m.m[9], m22 = m.m[10];
    const float m30 = m.m[12], m31 = m.m[13], m32 = m.m[14];
    if (transform) {
        const float c00 = Sub(Mul(m11, m22), Mul(m12, m21));
        const float c01 = Sub(Mul(m02, m21), Mul(m01, m22));
        const float c02 = Sub(Mul(m01, m12), Mul(m02, m11));
        const float c10 = Sub(Mul(m20, m12), Mul(m10, m22));
        const float c11 = Sub(Mul(m00, m22), Mul(m20, m02));
        const float c12 = Sub(Mul(m10, m02), Mul(m00, m12));
        const float c20 = Sub(Mul(m10, m21), Mul(m20, m11));
        const float c21 = Sub(Mul(m00, m11), Mul(m10, m01));
        const float c22 = Sub(Mul(m20, m01), Mul(m00, m21));
        outN.x = Add(Add(Mul(c02, v.z), Mul(v.x, c00)), Mul(v.y, c01));
        outN.y = Add(Add(Mul(c12, v.z), Mul(v.x, c10)), Mul(v.y, c11));
        outN.z = Add(Add(Mul(c21, v.z), Mul(v.x, c20)), Mul(v.y, c22));
        NormalizeX87(&outN.x);
        const float px = Mul(v.x, inD);
        const float py = Mul(v.y, inD);
        const float pz = Mul(v.z, inD);
        const float x = Add(Add(Add(Mul(m20, pz), Mul(m00, px)), Mul(py, m10)), m30);
        const float y = Add(Add(Add(Mul(m01, px), Mul(m21, pz)), Mul(m11, py)), m31);
        const float z = Add(Add(Add(Mul(m02, px), Mul(m22, pz)), Mul(py, m12)), m32);
        outD = Add(Add(Add(Mul(outN.x, x), Mul(outN.y, y)), Mul(outN.z, z)), extra);
        return;
    }
    outN.x = Add(Add(Mul(m20, v.z), Mul(m10, v.y)), Mul(m00, v.x));
    outN.y = Add(Add(Mul(m21, v.z), Mul(m01, v.x)), Mul(m11, v.y));
    outN.z = Add(Add(Mul(m22, v.z), Mul(m02, v.x)), Mul(m12, v.y));
    outD = Add(Add(Add(Add(Mul(outN.x, m30), Mul(outN.z, m32)), Mul(outN.y, m31)), inD), extra);
}

}  // namespace Jet
