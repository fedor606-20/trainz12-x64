// Jet::Ellipse (U1d2): 0x30 bytes, Plane base (normal +0x00, d +0x0c), centre +0x10, major +0x1c,
// minor +0x20, point +0x24. Not polymorphic. Local: constructors and assignment; GetMatrix and the collision
// overloads still forward to the original (they need the plane/line methods of U1b/U1d1).
#include <jet/Render3DCoreSolids.h>

namespace Jet {

namespace {

struct PlaneFields {
    Vector3 normal;
    float d;
};

}  // namespace

Ellipse::Ellipse()
{
    normal = Vector3{0.0f, 1.0f, 0.0f};
    d = 0.0f;
    center = Vector3{0.0f, 0.0f, 0.0f};
    point = Vector3{0.0f, 0.0f, 0.0f};
    major = 0.0f;
    minor = 0.0f;
}

Ellipse::Ellipse(const Ellipse& ellipse)
{
    SolidsDetail::X87Copy(normal.x, ellipse.normal.x);
    SolidsDetail::X87Copy(normal.y, ellipse.normal.y);
    SolidsDetail::X87Copy(normal.z, ellipse.normal.z);
    SolidsDetail::X87Copy(d, ellipse.d);
    center.x = ellipse.center.x;
    center.y = ellipse.center.y;
    center.z = ellipse.center.z;
    SolidsDetail::X87Copy(major, ellipse.major);
    SolidsDetail::X87Copy(minor, ellipse.minor);
    point.x = ellipse.point.x;
    point.y = ellipse.point.y;
    point.z = ellipse.point.z;
}

Ellipse::Ellipse(const Vector3& center_, float major_, float minor_, const Vector3& point_, const Plane& plane)
{
    const PlaneFields* p = reinterpret_cast<const PlaneFields*>(&plane);
    SolidsDetail::X87Copy(normal.x, p->normal.x);
    SolidsDetail::X87Copy(normal.y, p->normal.y);
    SolidsDetail::X87Copy(normal.z, p->normal.z);
    SolidsDetail::X87Copy(d, p->d);
    center.x = center_.x;
    center.y = center_.y;
    center.z = center_.z;
    point.x = point_.x;
    point.y = point_.y;
    point.z = point_.z;
    major = major_;
    minor = minor_;
}

Ellipse& Ellipse::operator=(const Ellipse& ellipse)
{
    SolidsDetail::X87Copy(normal.x, ellipse.normal.x);
    SolidsDetail::X87Copy(normal.y, ellipse.normal.y);
    SolidsDetail::X87Copy(normal.z, ellipse.normal.z);
    SolidsDetail::X87Copy(d, ellipse.d);
    SolidsDetail::X87Copy(center.x, ellipse.center.x);
    SolidsDetail::X87Copy(center.y, ellipse.center.y);
    SolidsDetail::X87Copy(center.z, ellipse.center.z);
    SolidsDetail::X87Copy(major, ellipse.major);
    SolidsDetail::X87Copy(minor, ellipse.minor);
    SolidsDetail::X87Copy(point.x, ellipse.point.x);
    SolidsDetail::X87Copy(point.y, ellipse.point.y);
    SolidsDetail::X87Copy(point.z, ellipse.point.z);
    return *this;
}

}  // namespace Jet
