// Unit U2 of Render3DCore_7c: Jet::TCBSpline / Jet::TCBSplineSegment (0x1006d6a0..0x10072104 from the separate
// TCBSpline dump). Tension/bias/continuity blend the neighbouring differences into Hermite tangents.
#include <jet/Render3DCoreSplines.h>

#include <new>

namespace Jet {

namespace {

TCBSplineSegment* NewTCBSegment(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                                float tension, float bias, float continuity)
{
    void* mem = Alloc::New(sizeof(TCBSplineSegment));
    return mem ? new (mem) TCBSplineSegment(p0, p1, p2, p3, tension, bias, continuity) : nullptr;
}

SplineSegment* NewPlainSegment(const Vector3& p)
{
    void* mem = Alloc::New(sizeof(SplineSegment));
    return mem ? new (mem) SplineSegment(p) : nullptr;
}

}  // namespace

// 0x1006d6a0
TCBSplineSegment::TCBSplineSegment(const TCBSplineSegment& o) : SplineSegment(o)
{
}

// 0x10071b90
TCBSplineSegment::TCBSplineSegment(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                                   float tension, float bias, float continuity)
    : SplineSegment()
{
    Set(p0, p1, p2, p3, tension, bias, continuity);
}

// 0x10071c20 (shared)
TCBSplineSegment::~TCBSplineSegment()
{
}

// 0x10028730 (shared)
TCBSplineSegment& TCBSplineSegment::operator=(const TCBSplineSegment& o)
{
    SplineSegment::operator=(o);
    return *this;
}

// 0x10071c30: TCB tangents in Hermite form. Order of operations follows the assembly per axis.
void __cdecl TCBSplineSegment::Set(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                                   float tension, float bias, float continuity)
{
    using namespace SplinesDetail;
    const float f = Mul(Sub(1.0f, tension), 0.5f);
    const float a = Mul(Add(continuity, 1.0f), Sub(1.0f, bias));
    const float b = Mul(Add(bias, 1.0f), Sub(1.0f, continuity));
    const float c = Mul(Add(continuity, 1.0f), Add(bias, 1.0f));
    const float d = Mul(Sub(1.0f, bias), Sub(1.0f, continuity));

    const float t2x = Mul(Add(Mul(Sub(p1.x, p0.x), a), Mul(Sub(p2.x, p1.x), b)), f);
    const float t3x = Mul(Add(Mul(Sub(p2.x, p1.x), c), Mul(Sub(p3.x, p2.x), d)), f);
    c0.x = Add(Add(Sub(Mul(p1.x, 2.0f), Mul(p2.x, 2.0f)), t3x), t2x);
    c1.x = Sub(Sub(Sub(Mul(p2.x, 3.0f), Mul(p1.x, 3.0f)), Mul(t2x, 2.0f)), t3x);
    c2.x = t2x;
    c3.x = p1.x;

    const float t2y = Mul(Add(Mul(Sub(p1.y, p0.y), a), Mul(Sub(p2.y, p1.y), b)), f);
    const float t3y = Mul(Add(Mul(Sub(p2.y, p1.y), c), Mul(Sub(p3.y, p2.y), d)), f);
    c0.y = Add(Add(Sub(Mul(p1.y, 2.0f), Mul(p2.y, 2.0f)), t3y), t2y);
    c1.y = Sub(Sub(Sub(Mul(p2.y, 3.0f), Mul(p1.y, 3.0f)), Mul(t2y, 2.0f)), t3y);
    c2.y = t2y;
    c3.y = p1.y;

    const float t2z = Mul(Add(Mul(Sub(p1.z, p0.z), a), Mul(Sub(p2.z, p1.z), b)), f);
    const float t3z = Mul(Add(Mul(Sub(p2.z, p1.z), c), Mul(Sub(p3.z, p2.z), d)), f);
    c0.z = Add(Add(Sub(Mul(p1.z, 2.0f), Mul(p2.z, 2.0f)), t3z), t2z);
    c1.z = Sub(Sub(Sub(Mul(p2.z, 3.0f), Mul(p1.z, 3.0f)), Mul(t2z, 2.0f)), t3z);
    c2.z = t2z;
    c3.z = p1.z;
}

// 0x1006d6d0
TCBSpline::TCBSpline(const TCBSpline& o) : Spline(o)
{
}

// 0x100713a0
TCBSpline::TCBSpline(unsigned int count, const Vector3* const points, float tension, float bias, float continuity,
                     bool closed)
    : Spline()
{
    Set(count, points, tension, bias, continuity, closed);
}

// 0x10071420
TCBSpline::TCBSpline(unsigned int count, const Vector3* const points, float* const tensions, float* const biases,
                     float* const continuities, bool closed)
    : Spline()
{
    Set(count, points, tensions, biases, continuities, closed);
}

// 0x100714a0
TCBSpline::~TCBSpline()
{
}

// 0x1000f930 (shared)
TCBSpline& TCBSpline::operator=(const TCBSpline& o)
{
    Spline::operator=(o);
    return *this;
}

// 0x100714b0
void __cdecl TCBSpline::Set(unsigned int count, const Vector3* const points, float tension, float bias,
                            float continuity, bool closed)
{
    RemoveDeleteAll();
    if (points == nullptr || count == 0)
        return;

    if (count == 1) {
        Add(NewPlainSegment(points[0]));
        return;
    }

    if (closed) {
        Add(NewTCBSegment(points[count - 1], points[0], points[1 % count], points[2 % count], tension, bias,
                          continuity));
        for (unsigned int i = 1; i < count; ++i)
            Add(NewTCBSegment(points[i - 1], points[i], points[(i + 1) % count], points[(i + 2) % count], tension,
                              bias, continuity));
        return;
    }

    const unsigned int last = count > 2 ? 2u : count - 1;
    Add(NewTCBSegment(points[0], points[0], points[1], points[last], tension, bias, continuity));
    for (unsigned int i = 1; i + 2 < count; ++i)
        Add(NewTCBSegment(points[i - 1], points[i], points[i + 1], points[i + 2], tension, bias, continuity));
    if (count > 2)
        Add(NewTCBSegment(points[count > 3 ? count - 3 : 0], points[count - 2], points[count - 1],
                          points[count - 1], tension, bias, continuity));
}

// 0x100717e0
void __cdecl TCBSpline::Set(unsigned int count, const Vector3* const points, float* const tensions, float* const biases,
                            float* const continuities, bool closed)
{
    RemoveDeleteAll();
    if (points == nullptr || count == 0)
        return;

    if (count == 1) {
        Add(NewPlainSegment(points[0]));
        return;
    }

    if (closed) {
        Add(NewTCBSegment(points[count - 1], points[0], points[1 % count], points[2 % count], tensions[0],
                          biases[0], continuities[0]));
        for (unsigned int i = 1; i < count; ++i)
            Add(NewTCBSegment(points[i - 1], points[i], points[(i + 1) % count], points[(i + 2) % count],
                              tensions[i], biases[i], continuities[i]));
        return;
    }

    const unsigned int last = count > 2 ? 2u : count - 1;
    Add(NewTCBSegment(points[0], points[0], points[1], points[last], tensions[0], biases[0], continuities[0]));
    for (unsigned int i = 1; i + 2 < count; ++i)
        Add(NewTCBSegment(points[i - 1], points[i], points[i + 1], points[i + 2], tensions[i], biases[i],
                          continuities[i]));
    if (count > 2)
        Add(NewTCBSegment(points[count > 3 ? count - 3 : 0], points[count - 2], points[count - 1],
                          points[count - 1], tensions[count - 2], biases[count - 2], continuities[count - 2]));
}

}  // namespace Jet
