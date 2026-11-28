// Unit U2 of Render3DCore_7c: Jet::SplineDefinition (0x1005fdc0..0x1006e7d0). The point array and parameters are
// local; ReadConfigData/WriteConfigData/ReadTextFile/WriteTextFile need KernelStandard's ConfigData/ConfigParser and
// KernelResource's ResourceDatabase and stay forwarded to Render3DCore_7c_orig.dll.
#include <jet/Render3DCoreSplines.h>

#include <new>

namespace Jet {

namespace {

void GrowPoints(Vector3*& points, unsigned int& capacity, unsigned int required)
{
    if (capacity >= required)
        return;
    const unsigned int chunks = (required + 0x7f) >> 7;
    const unsigned int newCapacity = chunks * 0x80;
    Vector3* grown = static_cast<Vector3*>(Alloc::New(newCapacity * 0xc));
    if (points != nullptr) {
        for (unsigned int i = 0; i < capacity && grown != nullptr; ++i)
            grown[i] = points[i];
        Alloc::Delete(points);
    }
    capacity = newCapacity;
    points = grown;
}

template <class T, class... A>
Spline* NewSpline(A&&... args)
{
    void* mem = Alloc::New(sizeof(T));
    return mem ? new (mem) T(static_cast<A&&>(args)...) : nullptr;
}

}  // namespace

// 0x1006d760
SplineDefinition::SplineDefinition()
    : type(Unknown), closed(true), tension(0.0f), bias(0.0f), continuity(0.0f), count(0), capacity(0), points(nullptr)
{
}

// 0x1005fdc0
SplineDefinition::SplineDefinition(const SplineDefinition& o)
    : type(o.type), closed(o.closed), tension(o.tension), bias(o.bias), continuity(o.continuity), count(0),
      capacity(0), points(nullptr)
{
    CopyPointsFrom(o);
}

// 0x1005fe10
SplineDefinition& SplineDefinition::operator=(const SplineDefinition& o)
{
    if (this != &o) {
        type = o.type;
        closed = o.closed;
        tension = o.tension;
        bias = o.bias;
        continuity = o.continuity;
        CopyPointsFrom(o);
    }
    return *this;
}

// 0x1006d7a0
SplineDefinition::~SplineDefinition()
{
    count = 0;
    if (points != nullptr)
        Alloc::Delete(points);
}

// 0x100609e0: keeps the destination capacity, grows to the source capacity if needed, then copies the points.
void SplineDefinition::CopyPointsFrom(const SplineDefinition& o)
{
    count = 0;
    if (o.count == 0)
        return;
    if (capacity < o.capacity) {
        if (points != nullptr)
            Alloc::Delete(points);
        points = static_cast<Vector3*>(Alloc::New(o.capacity * 0xc));
        capacity = o.capacity;
    }
    count = o.count;
    for (unsigned int i = 0; i < count && points != nullptr; ++i)
        points[i] = o.points[i];
}

// 0x1005fec0
void __cdecl SplineDefinition::SetType(SplineType t)
{
    type = t;
}

// 0x10006970
SplineDefinition::SplineType __cdecl SplineDefinition::GetType() const
{
    return static_cast<SplineType>(type);
}

// 0x1005fed0
void __cdecl SplineDefinition::SetParameters(bool closed_, float tension_, float bias_, float continuity_)
{
    closed = closed_;
    tension = tension_;
    bias = bias_;
    continuity = continuity_;
}

// 0x1005ff00
void __cdecl SplineDefinition::GetParameters(bool& closed_, float& tension_, float& bias_, float& continuity_) const
{
    closed_ = closed;
    tension_ = tension;
    bias_ = bias;
    continuity_ = continuity;
}

// 0x1005ff30: grows the array to count+1, stores the point at the end, then bumps the count.
void __cdecl SplineDefinition::AddPoint(const Vector3& point)
{
    const unsigned int required = count + 1;
    GrowPoints(points, capacity, required);
    if (points != nullptr) {
        Vector3* slot = points + count;
        slot->x = point.x;
        slot->y = point.y;
        slot->z = point.z;
    }
    ++count;
}

// 0x1005ff70
unsigned int __cdecl SplineDefinition::GetNumPoints() const
{
    return count;
}

// 0x1005ff90
void __cdecl SplineDefinition::RemoveAllPoints()
{
    count = 0;
}

// 0x1005ff80 (both the const and non-const export share this body)
Vector3* __cdecl SplineDefinition::GetPointArray()
{
    return points;
}

const Vector3* __cdecl SplineDefinition::GetPointArray() const
{
    return points;
}

// 0x1006e7d0: the definition's type selects one of the spline classes; type 5 is treated as TCB, unknown as none.
Spline* __cdecl SplineDefinition::CreateSpline()
{
    if (count == 0)
        return nullptr;

    switch (type) {
    case Hermite:
        return NewSpline<HermiteSpline>(count, points, closed);
    case Cardinal:
        return NewSpline<CardinalSpline>(count, points, tension, closed);
    case TCB:
    case Unknown:
        return NewSpline<TCBSpline>(count, points, tension, bias, continuity, closed);
    case Bezier:
        return NewSpline<BezierSpline>(count, points);
    case BSpline:
        return NewSpline<Jet::BSpline>(count, points, tension, closed);
    default:
        return nullptr;
    }
}

}  // namespace Jet
