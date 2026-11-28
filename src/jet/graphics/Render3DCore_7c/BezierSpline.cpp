// Unit U2 of Render3DCore_7c: Jet::BezierSpline / Jet::BezierSplineSegment (0x1000fa10..0x1000fe85).
// One segment per group of four control points; groups advance by three points.
#include <jet/Render3DCoreSplines.h>

#include <new>

namespace Jet {

namespace {

BezierSplineSegment* NewBezierSegment(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3)
{
    void* mem = Alloc::New(sizeof(BezierSplineSegment));
    return mem ? new (mem) BezierSplineSegment(p0, p1, p2, p3) : nullptr;
}

}  // namespace

// 0x1000fa10
BezierSplineSegment::BezierSplineSegment(const BezierSplineSegment& o) : SplineSegment(o)
{
}

// 0x1000fda0: zero coefficients, then Set.
BezierSplineSegment::BezierSplineSegment(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3)
    : SplineSegment()
{
    Set(p0, p1, p2, p3);
}

// 0x10071c20 (shared)
BezierSplineSegment::~BezierSplineSegment()
{
}

// 0x10028730 (shared)
BezierSplineSegment& BezierSplineSegment::operator=(const BezierSplineSegment& o)
{
    SplineSegment::operator=(o);
    return *this;
}

// 0x1000fe10: cubic Bezier polynomial in the four control points p0..p3. The original's operand order is kept.
void __cdecl BezierSplineSegment::Set(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3)
{
    using namespace SplinesDetail;
    for (int axis = 0; axis < 3; ++axis) {
        const float a = axis == 0 ? p0.x : axis == 1 ? p0.y : p0.z;
        const float b = axis == 0 ? p1.x : axis == 1 ? p1.y : p1.z;
        const float c = axis == 0 ? p2.x : axis == 1 ? p2.y : p2.z;
        const float d = axis == 0 ? p3.x : axis == 1 ? p3.y : p3.z;

        const float d21 = Sub(b, a);
        const float d43 = Sub(d, c);
        const float v0 = Add(Add(Sub(Mul(a, 2.0f), Mul(d, 2.0f)), Mul(d43, 3.0f)), Mul(d21, 3.0f));
        const float v1 = Sub(Sub(Sub(Mul(d, 3.0f), Mul(a, 3.0f)), Mul(Mul(d21, 3.0f), 2.0f)), Mul(d43, 3.0f));
        const float v2 = Mul(d21, 3.0f);
        if (axis == 0) {
            c0.x = v0;
            c1.x = v1;
            c2.x = v2;
            c3.x = a;
        } else if (axis == 1) {
            c0.y = v0;
            c1.y = v1;
            c2.y = v2;
            c3.y = a;
        } else {
            c0.z = v0;
            c1.z = v1;
            c2.z = v2;
            c3.z = a;
        }
    }
}

// 0x1000fa40
BezierSpline::BezierSpline(const BezierSpline& o) : Spline(o)
{
}

// 0x1000fad0: empty array, then Set.
BezierSpline::BezierSpline(unsigned int count, const Vector3* const points) : Spline()
{
    Set(count, points);
}

// 0x1000fb30
BezierSpline::~BezierSpline()
{
}

// 0x1000f930 (shared)
BezierSpline& BezierSpline::operator=(const BezierSpline& o)
{
    Spline::operator=(o);
    return *this;
}

// 0x1000fb40: fewer than four points gives one plain SplineSegment at the first point; otherwise
// (count-1)/3 segments of four consecutive control points, each starting three points after the previous.
void __cdecl BezierSpline::Set(unsigned int count, const Vector3* const points)
{
    RemoveDeleteAll();
    if (points == nullptr || count == 0)
        return;

    if (count < 4) {
        void* mem = Alloc::New(sizeof(SplineSegment));
        SplineSegment* segment = mem ? new (mem) SplineSegment(points[0]) : nullptr;
        Add(segment);
        return;
    }

    const unsigned int groups = (count - 1) / 3;
    const Vector3* p = points;
    for (unsigned int i = 0; i < groups; ++i, p += 3)
        Add(NewBezierSegment(p[0], p[1], p[2], p[3]));
}

}  // namespace Jet
