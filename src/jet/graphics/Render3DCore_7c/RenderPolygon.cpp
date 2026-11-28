// Unit U10 of Render3DCore_7c: RenderPolygon, a Render2DPrimitive with a Polygon subobject at +0x188 (0x1a4 bytes).
// The primary vtable is Render2DPrimitive's group and the secondary one is Polygon's; the compiler emits both from
// the base list. BuildGeometry/Init stay with the original (they drive the material chunk).
#include <jet/Render3DCorePrimitives.h>

#include <windows.h>

#include <new>

namespace Jet {

namespace {

}  // namespace

// 0x67bd0
RenderPolygon::RenderPolygon() : Render2DPrimitive(1), Polygon() {}

// 0x660a0
RenderPolygon::RenderPolygon(const RenderPolygon& o)
    : Render2DPrimitive(o), Polygon(static_cast<const Polygon&>(o))
{
}

// 0x66060
RenderPolygon::~RenderPolygon() {}

// 0x66120
RenderPolygon& RenderPolygon::operator=(const RenderPolygon& o)
{
    Render2DPrimitive::operator=(o);
    Polygon::operator=(static_cast<const Polygon&>(o));
    return *this;
}

// 0x66230
Particle* RenderPolygon::CreateInstance() const
{
    void* const raw = Alloc::New(0x1a4);
    if (!raw) return nullptr;
    RenderPolygon* const instance = ::new (raw) RenderPolygon();
    RenderPrimitive::InitInstance(*instance);
    instance->Polygon::operator=(static_cast<const Polygon&>(*this));
    return instance;
}

// 0x67fd0
float RenderPolygon::GetGeometryRadius() const
{
    Sphere sphere;
    if (static_cast<const Polygon&>(*this).CalculateBoundingSphere(sphere)) return sphere.radius;
    return 0.0f;
}

// 0x66300
bool RenderPolygon::IsWithin(const Vector3& point) const
{
    return static_cast<const Polygon&>(*this).IsInside(point);
}

// 0x68010
void RenderPolygon::InitInstance(RenderPolygon& instance) const
{
    RenderPrimitive::InitInstance(instance);
    instance.Polygon::operator=(static_cast<const Polygon&>(*this));
}

// 0x67d20: the polygon fan/triangulation is the original's
}  // namespace Jet
