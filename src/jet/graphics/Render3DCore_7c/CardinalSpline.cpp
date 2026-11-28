// Unit U2 of Render3DCore_7c: Jet::CardinalSpline / Jet::CardinalSplineSegment (0x1001b070..0x1001bc43).
// Cardinal tangents are (next - previous) * (1 - tension) / 2 around each inner point.
#include <jet/Render3DCoreSplines.h>

#include <new>

namespace Jet {

namespace {

CardinalSplineSegment* NewCardinalSegment(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                                          const Vector3& p3, float tension)
{
    void* mem = Alloc::New(sizeof(CardinalSplineSegment));
    return mem ? new (mem) CardinalSplineSegment(p0, p1, p2, p3, tension) : nullptr;
}

SplineSegment* NewPlainSegment(const Vector3& p)
{
    void* mem = Alloc::New(sizeof(SplineSegment));
    return mem ? new (mem) SplineSegment(p) : nullptr;
}

}  // namespace

// 0x1001b070
CardinalSplineSegment::CardinalSplineSegment(const CardinalSplineSegment& o) : SplineSegment(o)
{
}

// 0x1001b980
CardinalSplineSegment::CardinalSplineSegment(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                                             const Vector3& p3, float tension)
    : SplineSegment()
{
    Set(p0, p1, p2, p3, tension);
}

// 0x10071c20 (shared)
CardinalSplineSegment::~CardinalSplineSegment()
{
}

// 0x10028730 (shared)
CardinalSplineSegment& CardinalSplineSegment::operator=(const CardinalSplineSegment& o)
{
    SplineSegment::operator=(o);
    return *this;
}

// 0x1001ba00: Hermite form with the two cardinal tangents.
void __cdecl CardinalSplineSegment::Set(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                                        float tension)
{
    using namespace SplinesDetail;
    const float f = Mul(Sub(1.0f, tension), 0.5f);
    for (int axis = 0; axis < 3; ++axis) {
        const float a = axis == 0 ? p0.x : axis == 1 ? p0.y : p0.z;
        const float b = axis == 0 ? p1.x : axis == 1 ? p1.y : p1.z;
        const float c = axis == 0 ? p2.x : axis == 1 ? p2.y : p2.z;
        const float d = axis == 0 ? p3.x : axis == 1 ? p3.y : p3.z;

        const float t2 = Mul(Sub(c, a), f);
        const float t3 = Mul(Sub(d, b), f);
        const float v0 = Add(Add(Sub(Mul(b, 2.0f), Mul(c, 2.0f)), t3), t2);
        const float v1 = Sub(Sub(Sub(Mul(c, 3.0f), Mul(b, 3.0f)), Mul(t2, 2.0f)), t3);
        if (axis == 0) {
            c0.x = v0;
            c1.x = v1;
            c2.x = t2;
            c3.x = b;
        } else if (axis == 1) {
            c0.y = v0;
            c1.y = v1;
            c2.y = t2;
            c3.y = b;
        } else {
            c0.z = v0;
            c1.z = v1;
            c2.z = t2;
            c3.z = b;
        }
    }
}

// 0x1001b0a0
CardinalSpline::CardinalSpline(const CardinalSpline& o) : Spline(o)
{
}

// 0x1001b130
CardinalSpline::CardinalSpline(unsigned int count, const Vector3* const points, float tension, bool closed)
    : Spline()
{
    Set(count, points, tension, closed);
}

// 0x1001b1a0
CardinalSpline::CardinalSpline(unsigned int count, const Vector3* const points, float* const tensions, bool closed)
    : Spline()
{
    Set(count, points, tensions, closed);
}

// 0x1001b210
CardinalSpline::~CardinalSpline()
{
}

// 0x1000f930 (shared)
CardinalSpline& CardinalSpline::operator=(const CardinalSpline& o)
{
    Spline::operator=(o);
    return *this;
}

// 0x1001b220
void __cdecl CardinalSpline::Set(unsigned int count, const Vector3* const points, float tension, bool closed)
{
    RemoveDeleteAll();
    if (points == nullptr || count == 0)
        return;

    if (count == 1) {
        Add(NewPlainSegment(points[0]));
        return;
    }

    if (closed) {
        Add(NewCardinalSegment(points[count - 1], points[0], points[1 % count], points[2 % count], tension));
        for (unsigned int i = 1; i < count; ++i)
            Add(NewCardinalSegment(points[i - 1], points[i], points[(i + 1) % count], points[(i + 2) % count],
                                   tension));
        return;
    }

    const unsigned int last = count > 2 ? 2u : count - 1;
    Add(NewCardinalSegment(points[0], points[0], points[1], points[last], tension));
    for (unsigned int i = 1; i + 2 < count; ++i)
        Add(NewCardinalSegment(points[i - 1], points[i], points[i + 1], points[i + 2], tension));
    if (count > 2)
        Add(NewCardinalSegment(points[count > 3 ? count - 3 : 0], points[count - 2], points[count - 1],
                               points[count - 1], tension));
}

// 0x1001b5b0
void __cdecl CardinalSpline::Set(unsigned int count, const Vector3* const points, float* const tensions, bool closed)
{
    RemoveDeleteAll();
    if (points == nullptr || count == 0)
        return;

    if (count == 1) {
        Add(NewPlainSegment(points[0]));
        return;
    }

    if (closed) {
        Add(NewCardinalSegment(points[count - 1], points[0], points[1 % count], points[2 % count], tensions[0]));
        for (unsigned int i = 1; i < count; ++i)
            Add(NewCardinalSegment(points[i - 1], points[i], points[(i + 1) % count], points[(i + 2) % count],
                                   tensions[i]));
        return;
    }

    const unsigned int last = count > 2 ? 2u : count - 1;
    Add(NewCardinalSegment(points[0], points[0], points[1], points[last], tensions[0]));
    for (unsigned int i = 1; i + 2 < count; ++i)
        Add(NewCardinalSegment(points[i - 1], points[i], points[i + 1], points[i + 2], tensions[i]));
    if (count > 2)
        Add(NewCardinalSegment(points[count > 3 ? count - 3 : 0], points[count - 2], points[count - 1],
                               points[count - 1], tensions[count - 2]));
}

}  // namespace Jet
