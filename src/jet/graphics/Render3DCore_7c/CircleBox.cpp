// Jet::Circle (U1d2): 0x20 bytes, Plane base (normal +0x00, d +0x0c), centre +0x10, radius +0x1c. Not
// polymorphic in TS12. Local: constructors, assignment, accessors and TestCollision(Sphere), which only reads the
// circle's plane/centre/radius and the sphere's centre/radius. The Line/Plane/Circle overloads and Overlap call
// Plane::Intersect, Line::Distance/TestCollision and stay forwarded (U1b/U1d1).
#include <jet/Render3DCoreSolids.h>

namespace Jet {

namespace {
constexpr float kTolerance = 9.9999997e-05f;  // 0x38d1b717
}  // namespace

namespace {

struct PlaneFields {
    Vector3 normal;
    float d;
};

}  // namespace

Circle::Circle()
{
    normal = Vector3{0.0f, 1.0f, 0.0f};
    d = 0.0f;
    center = Vector3{0.0f, 0.0f, 0.0f};
    radius = 0.0f;
}

Circle::Circle(const Circle& circle)
{
    SolidsDetail::X87Copy(normal.x, circle.normal.x);
    SolidsDetail::X87Copy(normal.y, circle.normal.y);
    SolidsDetail::X87Copy(normal.z, circle.normal.z);
    SolidsDetail::X87Copy(d, circle.d);
    center.x = circle.center.x;
    center.y = circle.center.y;
    center.z = circle.center.z;
    SolidsDetail::X87Copy(radius, circle.radius);
}

Circle::Circle(const Vector3& center_, float radius_, const Plane& plane)
{
    const PlaneFields* p = reinterpret_cast<const PlaneFields*>(&plane);
    SolidsDetail::X87Copy(normal.x, p->normal.x);
    SolidsDetail::X87Copy(normal.y, p->normal.y);
    SolidsDetail::X87Copy(normal.z, p->normal.z);
    SolidsDetail::X87Copy(d, p->d);
    center.x = center_.x;
    center.y = center_.y;
    center.z = center_.z;
    radius = radius_;
}

Circle& Circle::operator=(const Circle& circle)
{
    SolidsDetail::X87Copy(normal.x, circle.normal.x);
    SolidsDetail::X87Copy(normal.y, circle.normal.y);
    SolidsDetail::X87Copy(normal.z, circle.normal.z);
    SolidsDetail::X87Copy(d, circle.d);
    SolidsDetail::X87Copy(center.x, circle.center.x);
    SolidsDetail::X87Copy(center.y, circle.center.y);
    SolidsDetail::X87Copy(center.z, circle.center.z);
    SolidsDetail::X87Copy(radius, circle.radius);
    return *this;
}

const Vector3& __cdecl Circle::GetCenter() const
{
    return center;
}

float __cdecl Circle::GetRadius() const
{
    return radius;
}

void __cdecl Circle::SetCenter(Vector3 center_)
{
    center.x = center_.x;
    center.y = center_.y;
    center.z = center_.z;
}

void __cdecl Circle::SetRadius(float radius_)
{
    radius = radius_;
}

// 0x1001dd60: project the sphere centre onto the circle plane, reject when the sphere does not reach the plane
// (t^2 > r^2), then compare the squared in-plane distance with the two radii.
bool __cdecl Circle::TestCollision(const Sphere& sphere)
{
    const float t = ((normal.z * sphere.center.z + normal.y * sphere.center.y) + normal.x * sphere.center.x) - d;
    const float px = sphere.center.x - normal.x * t;
    const float py = sphere.center.y - normal.y * t;
    const float pz = sphere.center.z - normal.z * t;
    const float t2 = t * t;
    const float radius2 = sphere.radius * sphere.radius;
    if (0.0f > radius2 - t2)
        return false;
    const float dx = px - center.x;
    const float dy = py - center.y;
    const float dz = pz - center.z;
    const float dist2 = ((dz * dz + dy * dy) + dx * dx);
    const float total = radius * radius + (radius2 - t2);
    return kTolerance > (dist2 - total);
}

}  // namespace Jet
