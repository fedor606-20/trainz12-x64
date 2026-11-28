// Unit U2 of Render3DCore_7c: Jet::BSpline / Jet::BSplineSegment (0x10011fb0..0x10012bc6).
// One cubic B-spline segment per control interval; the scalar overloads weight every segment equally.
#include <jet/Render3DCoreSplines.h>

#include <new>

namespace Jet {

namespace {

BSplineSegment* NewBSplineSegment(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                                  float tension)
{
    void* mem = Alloc::New(sizeof(BSplineSegment));
    return mem ? new (mem) BSplineSegment(p0, p1, p2, p3, tension) : nullptr;
}

SplineSegment* NewPlainSegment(const Vector3& p)
{
    void* mem = Alloc::New(sizeof(SplineSegment));
    return mem ? new (mem) SplineSegment(p) : nullptr;
}

}  // namespace

// 0x10011fb0
BSplineSegment::BSplineSegment(const BSplineSegment& o) : SplineSegment(o)
{
}

// 0x100128c0: zero coefficients, then Set.
BSplineSegment::BSplineSegment(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                               float tension)
    : SplineSegment()
{
    Set(p0, p1, p2, p3, tension);
}

// 0x10071c20 (shared)
BSplineSegment::~BSplineSegment()
{
}

// 0x10028730 (shared)
BSplineSegment& BSplineSegment::operator=(const BSplineSegment& o)
{
    SplineSegment::operator=(o);
    return *this;
}

// 0x10012940: uniform cubic B-spline with tension f = tension/6. The original's per-axis operand order is kept
// (x adds the c2 term before the c1 term, y and z add them the other way round).
void __cdecl BSplineSegment::Set(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                                 float tension)
{
    using namespace SplinesDetail;
    const float sixth = 0.16666667f;
    const float f = Mul(tension, sixth);
    const float a = Sub(2.0f, Mul(f, 9.0f));
    const float b = Sub(Mul(f, 9.0f), 2.0f);
    const float c = Sub(Mul(f, 12.0f), 3.0f);
    const float d = Sub(3.0f, Mul(f, 15.0f));
    const float e = Sub(1.0f, Mul(f, 2.0f));

    // x: (p3-p0)*t/6 + p1*a + b*p2, then (p1*c + p0*t/2) + d*p2
    {
        const float a0 = Mul(Mul(Sub(p3.x, p0.x), tension), sixth);
        const float b0 = Mul(p1.x, a);
        const float c0v = Mul(b, p2.x);
        c0.x = Add(Add(a0, b0), c0v);
        const float a1 = Mul(p1.x, c);
        const float b1 = Mul(Mul(p0.x, tension), 0.5f);
        const float c1v = Mul(d, p2.x);
        c1.x = Add(Add(a1, b1), c1v);
        c2.x = Mul(Mul(Sub(Mul(p2.x, 3.0f), Mul(p0.x, 3.0f)), tension), sixth);
        c3.x = Add(Mul(Mul(Add(p0.x, p2.x), tension), sixth), Mul(e, p1.x));
    }
    // y, z: (p3-p0)*t/6 + b*p2 + p1*a, then (d*p2 + p1*c) + p0*t/2
    {
        const float a0 = Mul(Mul(Sub(p3.y, p0.y), tension), sixth);
        const float c0v = Mul(p2.y, b);
        const float b0 = Mul(p1.y, a);
        c0.y = Add(Add(a0, c0v), b0);
        const float a1 = Mul(p2.y, d);
        const float b1 = Mul(p1.y, c);
        const float c1v = Mul(Mul(p0.y, tension), 0.5f);
        c1.y = Add(Add(a1, b1), c1v);
        c2.y = Mul(Mul(Sub(Mul(p2.y, 3.0f), Mul(p0.y, 3.0f)), tension), sixth);
        c3.y = Add(Mul(Mul(Add(p2.y, p0.y), tension), sixth), Mul(p1.y, e));
    }
    {
        const float a0 = Mul(Mul(Sub(p3.z, p0.z), tension), sixth);
        const float c0v = Mul(p2.z, b);
        const float b0 = Mul(p1.z, a);
        c0.z = Add(Add(a0, c0v), b0);
        const float a1 = Mul(d, p2.z);
        const float b1 = Mul(c, p1.z);
        const float c1v = Mul(Mul(p0.z, tension), 0.5f);
        c1.z = Add(Add(a1, b1), c1v);
        c2.z = Mul(Mul(Sub(Mul(p2.z, 3.0f), Mul(p0.z, 3.0f)), tension), sixth);
        c3.z = Add(Mul(Mul(Add(p0.z, p2.z), tension), sixth), Mul(e, p1.z));
    }
}

// 0x10011fe0
BSpline::BSpline(const BSpline& o) : Spline(o)
{
}

// 0x10012070
BSpline::BSpline(unsigned int count, const Vector3* const points, float tension, bool closed) : Spline()
{
    Set(count, points, tension, closed);
}

// 0x100120e0
BSpline::BSpline(unsigned int count, const Vector3* const points, float* const tensions, bool closed) : Spline()
{
    Set(count, points, tensions, closed);
}

// 0x10012150
BSpline::~BSpline()
{
}

// 0x1000f930 (shared)
BSpline& BSpline::operator=(const BSpline& o)
{
    Spline::operator=(o);
    return *this;
}

// 0x10012160
void __cdecl BSpline::Set(unsigned int count, const Vector3* const points, float tension, bool closed)
{
    RemoveDeleteAll();
    if (points == nullptr || count == 0)
        return;

    if (count == 1) {
        Add(NewPlainSegment(points[0]));
        return;
    }

    if (closed) {
        Add(NewBSplineSegment(points[count - 1], points[0], points[1 % count], points[2 % count], tension));
        for (unsigned int i = 1; i < count; ++i)
            Add(NewBSplineSegment(points[i - 1], points[i], points[(i + 1) % count], points[(i + 2) % count],
                                  tension));
        return;
    }

    const unsigned int last = count > 2 ? 2u : count - 1;
    Add(NewBSplineSegment(points[0], points[0], points[1], points[last], tension));
    for (unsigned int i = 1; i + 2 < count; ++i)
        Add(NewBSplineSegment(points[i - 1], points[i], points[i + 1], points[i + 2], tension));
    if (count > 2)
        Add(NewBSplineSegment(points[count > 3 ? count - 3 : 0], points[count - 2], points[count - 1],
                              points[count - 1], tension));
}

// 0x100124f0
void __cdecl BSpline::Set(unsigned int count, const Vector3* const points, float* const tensions, bool closed)
{
    RemoveDeleteAll();
    if (points == nullptr || count == 0)
        return;

    if (count == 1) {
        Add(NewPlainSegment(points[0]));
        return;
    }

    if (closed) {
        Add(NewBSplineSegment(points[count - 1], points[0], points[1 % count], points[2 % count], tensions[0]));
        for (unsigned int i = 1; i < count; ++i)
            Add(NewBSplineSegment(points[i - 1], points[i], points[(i + 1) % count], points[(i + 2) % count],
                                  tensions[i]));
        return;
    }

    const unsigned int last = count > 2 ? 2u : count - 1;
    Add(NewBSplineSegment(points[0], points[0], points[1], points[last], tensions[0]));
    for (unsigned int i = 1; i + 2 < count; ++i)
        Add(NewBSplineSegment(points[i - 1], points[i], points[i + 1], points[i + 2], tensions[i]));
    if (count > 2)
        Add(NewBSplineSegment(points[count > 3 ? count - 3 : 0], points[count - 2], points[count - 1],
                              points[count - 1], tensions[count - 2]));
}

}  // namespace Jet
