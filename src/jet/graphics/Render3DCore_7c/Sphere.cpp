// Jet::Sphere (U1d2): 0x14 bytes, vftable +0x00, centre +0x04, radius +0x10. TS12 has no radiusXY.
// Ordinary members are __cdecl; the class has one virtual slot (Maximize). Float operation order follows the
// assembly, including the (x^2 + z^2) + y^2 sums and the 0.0001 / 1e-8 tolerances.
#include <jet/Render3DCoreSolids.h>

#include <cmath>
#include <cstring>

namespace Jet {

namespace {

struct PlaneFields {
    Vector3 normal;
    float d;
};

struct LineFields {
    void* vftable;  // +0x00
    Vector3 direction;  // +0x04
    Vector3 point;      // +0x10
};
static_assert(sizeof(LineFields) == 0x1c, "Line");

constexpr float kTolerance = 9.9999997e-05f;  // 0x38d1b717, the 0.0001 the assembly loads
constexpr float kTiny = 9.9999991e-09f;       // 0x322bcc76, Sphere vs point

float AbsNeg(float v)
{
    if (0.0f > v)
        v = -0.0f - v;
    return v;
}

}  // namespace

Sphere::Sphere()
{
}

Sphere::Sphere(const Vector3& center_, float radius_)
{
    SolidsDetail::X87Copy(center.x, center_.x);
    SolidsDetail::X87Copy(center.y, center_.y);
    SolidsDetail::X87Copy(center.z, center_.z);
    radius = radius_;
}

Sphere::Sphere(const Sphere& sphere)
{
    center.x = sphere.center.x;
    center.y = sphere.center.y;
    center.z = sphere.center.z;
    SolidsDetail::X87Copy(radius, sphere.radius);
}

Sphere& Sphere::operator=(const Sphere& sphere)
{
    SolidsDetail::X87Copy(center.x, sphere.center.x);
    SolidsDetail::X87Copy(center.y, sphere.center.y);
    SolidsDetail::X87Copy(center.z, sphere.center.z);
    SolidsDetail::X87Copy(radius, sphere.radius);
    return *this;
}

Vector3& __cdecl Sphere::GetCenter()
{
    return center;
}

const Vector3& __cdecl Sphere::GetCenter() const
{
    return center;
}

float __cdecl Sphere::GetRadius() const
{
    return radius;
}

void __cdecl Sphere::SetCenter(const Vector3& center_)
{
    SolidsDetail::X87Copy(center.x, center_.x);
    SolidsDetail::X87Copy(center.y, center_.y);
    SolidsDetail::X87Copy(center.z, center_.z);
}

void __cdecl Sphere::SetRadius(float radius_)
{
    radius = radius_;
}

// 0x1006cd50: union of this and the argument. COMISS(diff2, dist2)/JC merges when diff2 < dist2 or either is NaN;
// the growth branch copies the other sphere when its radius is strictly larger.
void __cdecl Sphere::Maximize(const Sphere& sphere)
{
    const float dx = SolidsDetail::SubSS(sphere.center.x, center.x);
    const float dy = SolidsDetail::SubSS(sphere.center.y, center.y);
    const float dz = SolidsDetail::SubSS(sphere.center.z, center.z);
    const float dist2 = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(dz, dz), SolidsDetail::MulSS(dx, dx)), SolidsDetail::MulSS(dy, dy));
    const float radiusDiff = SolidsDetail::SubSS(sphere.radius, radius);
    const float diff2 = SolidsDetail::MulSS(radiusDiff, radiusDiff);
    if (!(diff2 >= dist2)) {
        const float dist = SolidsDetail::SqrtF(dist2);
        const float outer = SolidsDetail::AddSS(dist, sphere.radius);
        const float t = SolidsDetail::DivSS(SolidsDetail::SubSS(outer, radius),
                                            SolidsDetail::MulSS(dist, 2.0f));
        center.x = SolidsDetail::AddSS(SolidsDetail::MulSS(dx, t), center.x);
        center.y = SolidsDetail::AddSS(center.y, SolidsDetail::MulSS(dy, t));
        center.z = SolidsDetail::AddSS(center.z, SolidsDetail::MulSS(dz, t));
        radius = SolidsDetail::MulSS(SolidsDetail::AddSS(outer, radius), 0.5f);
    } else if (radiusDiff > 0.0f) {
        center.x = sphere.center.x;
        center.y = sphere.center.y;
        center.z = sphere.center.z;
        radius = sphere.radius;
    }
}

// 0x10006360: five planes hide the sphere when d - dot(normal, centre) > radius; the sixth uses the strict
// Plane::IsBehind form (radius < d - dot), exactly as the original's tail call does.
bool __cdecl Sphere::CheckVisibility(const Plane* frustum)
{
    const PlaneFields* planes = reinterpret_cast<const PlaneFields*>(frustum);
    for (int i = 0; i < 5; ++i) {
        const PlaneFields& p = planes[i];
        const float dot = (p.normal.z * center.z + p.normal.y * center.y) + p.normal.x * center.x;
        const float behind = p.d - dot;
        if (behind > radius)
            return false;
    }
    const PlaneFields& p = planes[5];
    const float dot = (p.normal.z * center.z + p.normal.y * center.y) + p.normal.x * center.x;
    return !(radius < p.d - dot);
}

bool __cdecl Sphere::TestCollision(const Vector3& point) const
{
    const float dx = point.x - center.x;
    const float dy = point.y - center.y;
    const float dz = point.z - center.z;
    const float dist2 = ((dx * dx) + (dz * dz)) + (dy * dy);
    return kTiny > (dist2 - radius * radius);
}

bool __cdecl Sphere::TestCollision(const Vector3& point, float otherRadius) const
{
    const float dx = center.x - point.x;
    const float dy = center.y - point.y;
    const float dz = center.z - point.z;
    const float dist2 = ((dx * dx) + (dz * dz)) + (dy * dy);
    const float sum = radius + otherRadius;
    return kTiny > (dist2 - sum * sum);
}

bool __cdecl Sphere::TestCollision(const Sphere& sphere) const
{
    const float dx = center.x - sphere.center.x;
    const float dz = center.z - sphere.center.z;
    const float dy = center.y - sphere.center.y;
    const float dist2 = ((dx * dx) + (dz * dz)) + (dy * dy);
    const float sum = radius + sphere.radius;
    return kTiny > (dist2 - sum * sum);
}

// 0x1006cd30: box.TestCollision(sphere), which is local in this module (U1a).
bool __cdecl Sphere::TestCollision(const AxisAlignedBox& box) const
{
    return box.TestCollision(*this);
}

// 0x1006cd10: virtual dispatch to the cylinder's Sphere overload (vtable slot 3).
bool __cdecl Sphere::TestCollision(const Cylinder& cylinder) const
{
    return cylinder.TestCollision(*this);
}

// Same helper shared with the Cylinder overload in the original (0x1006cd10).
bool __cdecl Sphere::TestCollision(const Ellipsoid& ellipsoid) const
{
    return ellipsoid.TestCollision(*this);
}

// 0x100064b0: raw direction/point layout of Line (direction +0x04, point +0x10), no normalisation.
bool __cdecl Sphere::TestCollision(const Line& line) const
{
    const LineFields* l = reinterpret_cast<const LineFields*>(&line);
    const float dx = center.x - l->point.x;
    const float dy = center.y - l->point.y;
    const float dz = center.z - l->point.z;
    const float dot = (l->direction.y * dy + l->direction.z * dz) + l->direction.x * dx;
    const float dist2 = ((dx * dx) + (dy * dy)) + (dz * dz);
    const float result = (dist2 - dot * dot) - radius * radius;
    return kTolerance > result;
}

bool __cdecl Sphere::TestCollision(const Plane& plane) const
{
    const PlaneFields* p = reinterpret_cast<const PlaneFields*>(&plane);
    const float dot = (p->normal.z * center.z + p->normal.y * center.y) + p->normal.x * center.x;
    const float distance = AbsNeg(dot - p->d);
    return kTolerance > (distance - radius);
}

}  // namespace Jet
