// Unit U2 of Render3DCore_7c: Jet::HermiteSpline / Jet::HermiteSplineSegment (0x10028700..0x10028e98).
// The spline stores one Hermite segment per point interval: (point, tangent, next point, next tangent).
#include <jet/Render3DCoreSplines.h>

#include <new>

namespace Jet {

namespace {

HermiteSplineSegment* NewHermiteSegment(const Vector3& p0, const Vector3& t0, const Vector3& p1, const Vector3& t1)
{
    void* mem = Alloc::New(sizeof(HermiteSplineSegment));
    return mem ? new (mem) HermiteSplineSegment(p0, t0, p1, t1) : nullptr;
}

SplineSegment* NewPlainSegment(const Vector3& p)
{
    void* mem = Alloc::New(sizeof(SplineSegment));
    return mem ? new (mem) SplineSegment(p) : nullptr;
}

}  // namespace

// 0x10028700
HermiteSplineSegment::HermiteSplineSegment(const HermiteSplineSegment& o) : SplineSegment(o)
{
}

// 0x10028be0: zero coefficients, then the same polynomial body as SetHermiteSplineData.
HermiteSplineSegment::HermiteSplineSegment(const Vector3& p0, const Vector3& t0, const Vector3& p1,
                                           const Vector3& t1)
    : SplineSegment()
{
    Set(p0, t0, p1, t1);
}

// 0x10071c20 (shared)
HermiteSplineSegment::~HermiteSplineSegment()
{
}

// 0x10028730 (shared)
HermiteSplineSegment& HermiteSplineSegment::operator=(const HermiteSplineSegment& o)
{
    SplineSegment::operator=(o);
    return *this;
}

// 0x10028d60 (the exported name is shared with SplineSegment::SetHermiteSplineData)
void __cdecl HermiteSplineSegment::Set(const Vector3& p0, const Vector3& t0, const Vector3& p1, const Vector3& t1)
{
    SetHermiteSplineData(p0, t0, p1, t1);
}

// 0x10028790
HermiteSpline::HermiteSpline(const HermiteSpline& o) : Spline(o)
{
}

// 0x10028820
HermiteSpline::HermiteSpline(unsigned int count, const Vector3* const points, const Vector3* const tangents,
                             bool closed)
    : Spline()
{
    Set(count, points, tangents, closed);
}

// 0x10028890
HermiteSpline::HermiteSpline(unsigned int count, const Vector3* const points, bool closed) : Spline()
{
    Set(count, points, closed);
}

// 0x10028900
HermiteSpline::~HermiteSpline()
{
}

// 0x1000f930 (shared)
HermiteSpline& HermiteSpline::operator=(const HermiteSpline& o)
{
    Spline::operator=(o);
    return *this;
}

// 0x10028a90: separate point and tangent arrays, one segment per interval, optional closing segment.
void __cdecl HermiteSpline::Set(unsigned int count, const Vector3* const points, const Vector3* const tangents,
                                 bool closed)
{
    RemoveDeleteAll();
    if (points == nullptr || count == 0)
        return;

    if (count == 1) {
        Add(NewPlainSegment(points[0]));
        return;
    }

    for (unsigned int i = 0; i + 1 < count; ++i)
        Add(NewHermiteSegment(points[i], tangents[i], points[i + 1], tangents[i + 1]));
    if (closed)
        Add(NewHermiteSegment(points[count - 1], tangents[count - 1], points[0], tangents[0]));
}

// 0x10028910: the array interleaves points and tangents (p0, t0, p1, t1, ...); an odd count is rounded down.
void __cdecl HermiteSpline::Set(unsigned int count, const Vector3* const points, bool closed)
{
    RemoveDeleteAll();
    if ((count & 1u) != 0)
        --count;
    if (count == 0)
        return;

    if (count <= 2) {
        Add(NewPlainSegment(points[0]));
        return;
    }

    for (unsigned int i = 0; i + 3 < count; i += 2)
        Add(NewHermiteSegment(points[i], points[i + 1], points[i + 2], points[i + 3]));
    if (closed)
        Add(NewHermiteSegment(points[count - 2], points[count - 1], points[0], points[1]));
}

}  // namespace Jet
