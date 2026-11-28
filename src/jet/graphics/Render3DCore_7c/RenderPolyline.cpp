// Unit U10 of Render3DCore_7c: the polyline family (RenderPolyline, RenderLine, RenderSpline). RenderPrimitive keeps
// the radius in +0x188 and the centre in +0x18c; RenderPolyline's BuildGeometry is the family's first new virtual
// (vtable slot 0xe8), so it is a virtual member here. BuildGeometry and Init stay with the original (the material's
// chunk path); the vtable entry is a call into Render3DCore_7c_orig.dll.
#include <jet/Render3DCorePrimitives.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

template <class T>
T* AllocateInstance(unsigned int size)
{
    void* const raw = Alloc::New(size);
    return raw ? ::new (raw) T() : nullptr;
}

}  // namespace

// 0x68060
RenderPolyline::RenderPolyline() : RenderPrimitive(1), radius(0.0f) {}

// 0x658c0
RenderPolyline::RenderPolyline(const RenderPolyline& o)
    : RenderPrimitive(o), radius(o.radius), center(o.center)
{
}

RenderPolyline::~RenderPolyline() {}

// 0x65910 (shared with RenderLine and RenderSpline)
RenderPolyline& RenderPolyline::operator=(const RenderPolyline& o)
{
    RenderPrimitive::operator=(o);
    radius = o.radius;
    std::memcpy(&center, &o.center, sizeof center);
    return *this;
}

// 0x65a00
Particle* RenderPolyline::CreateInstance() const
{
    RenderPolyline* const instance = AllocateInstance<RenderPolyline>(0x198);
    if (!instance) return nullptr;
    RenderPrimitive::InitInstance(*instance);
    instance->radius = radius;
    std::memcpy(&instance->center, &center, sizeof center);
    return instance;
}

// 0x65960
float RenderPolyline::GetGeometryRadius() const
{
    float m = scale.y;
    if (scale.z > m) m = scale.z;
    if (m > scale.x) return radius * m;
    return radius * scale.x;
}

// 0x659b0
Vector3 RenderPolyline::GetGeometryCenter() const
{
    return Vector3{center.x * scale.x, center.y * scale.y, center.z * scale.z};
}

// 0x68010
void RenderPolyline::InitInstance(RenderPolyline& instance) const
{
    RenderPrimitive::InitInstance(instance);
    instance.radius = radius;
    std::memcpy(&instance.center, &center, sizeof center);
}

// ---- RenderLine -------------------------------------------------------------------------------------------------

// 0x65ac0
RenderLine::RenderLine() : RenderPolyline() {}

// 0x65ae0
RenderLine::RenderLine(const RenderLine& o) : RenderPolyline(o) {}

RenderLine::~RenderLine() {}

// 0x65910
RenderLine& RenderLine::operator=(const RenderLine& o)
{
    RenderPolyline::operator=(o);
    return *this;
}

// 0x65b90
Particle* RenderLine::CreateInstance() const
{
    RenderLine* const instance = AllocateInstance<RenderLine>(0x198);
    if (!instance) return nullptr;
    RenderPrimitive::InitInstance(*instance);
    instance->radius = radius;
    std::memcpy(&instance->center, &center, sizeof center);
    return instance;
}

// 0x68370 (shared with RenderPolyline and RenderSpline)
void RenderLine::InitInstance(RenderLine& instance) const
{
    RenderPrimitive::InitInstance(instance);
    instance.radius = radius;
    std::memcpy(&instance.center, &center, sizeof center);
}

// ---- RenderSpline -----------------------------------------------------------------------------------------------

// 0x699b0
RenderSpline::RenderSpline() : RenderPolyline() {}

// 0x699d0
RenderSpline::RenderSpline(const RenderSpline& o) : RenderPolyline(o) {}

RenderSpline::~RenderSpline() {}

// 0x65910
RenderSpline& RenderSpline::operator=(const RenderSpline& o)
{
    RenderPolyline::operator=(o);
    return *this;
}

// 0x69a20
Particle* RenderSpline::CreateInstance() const
{
    RenderSpline* const instance = AllocateInstance<RenderSpline>(0x198);
    if (!instance) return nullptr;
    RenderPrimitive::InitInstance(*instance);
    instance->radius = radius;
    std::memcpy(&instance->center, &center, sizeof center);
    return instance;
}

// 0x68370
void RenderSpline::InitInstance(RenderSpline& instance) const
{
    RenderPrimitive::InitInstance(instance);
    instance.radius = radius;
    std::memcpy(&instance.center, &center, sizeof center);
}

}  // namespace Jet
