// Jet::Ellipsoid (U1d2): 0x38 bytes, vftable +0x00, Plane base (normal +0x04, d +0x10), centre +0x14,
// major +0x20, minor +0x24, up +0x28, point +0x2c. Local: constructors, assignment and the scalar accessors.
// The vtable has seven slots (AxisAlignedBox, Ellipsoid, Cylinder, Sphere, Plane, Line, Vector3); the two
// unsupported pairs use the original's shared "return false" body, the rest wrap the same export of
// Render3DCore_7c_orig.dll because GetMatrix (and through it the collision maths) still belongs to the original.
#include <jet/Render3DCoreSolids.h>

namespace Jet {

namespace {

template <class Fn>
Fn Orig(const char* name)
{
    return reinterpret_cast<Fn>(SolidsDetail::OrigProc(name));
}

using EllEllipsoidFn = bool(__cdecl*)(const Ellipsoid*, const Ellipsoid&);
using EllSphereFn = bool(__cdecl*)(const Ellipsoid*, const Sphere&);
using EllPlaneFn = bool(__cdecl*)(Ellipsoid*, const Plane&);
using EllLineFn = bool(__cdecl*)(Ellipsoid*, const Line&);
using EllVectorFn = bool(__cdecl*)(Ellipsoid*, const Vector3&);

}  // namespace

namespace {

struct PlaneFields {
    Vector3 normal;
    float d;
};

}  // namespace

Ellipsoid::Ellipsoid()
{
    normal = Vector3{0.0f, 1.0f, 0.0f};
    d = 1.0f;
    center = Vector3{0.0f, 0.0f, 0.0f};
    major = 0.0f;
    minor = 0.0f;
    up = 0.0f;
    point = Vector3{0.0f, 0.0f, 0.0f};
}

Ellipsoid::Ellipsoid(const Vector3& center_, float major_, float minor_, float up_, const Plane& plane,
                     const Vector3& point_)
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
    up = up_;
}

Ellipsoid::Ellipsoid(const Ellipsoid& ellipsoid)
{
    SolidsDetail::X87Copy(normal.x, ellipsoid.normal.x);
    SolidsDetail::X87Copy(normal.y, ellipsoid.normal.y);
    SolidsDetail::X87Copy(normal.z, ellipsoid.normal.z);
    SolidsDetail::X87Copy(d, ellipsoid.d);
    center.x = ellipsoid.center.x;
    center.y = ellipsoid.center.y;
    center.z = ellipsoid.center.z;
    SolidsDetail::X87Copy(major, ellipsoid.major);
    SolidsDetail::X87Copy(minor, ellipsoid.minor);
    SolidsDetail::X87Copy(up, ellipsoid.up);
    point.x = ellipsoid.point.x;
    point.y = ellipsoid.point.y;
    point.z = ellipsoid.point.z;
}

Ellipsoid& Ellipsoid::operator=(const Ellipsoid& ellipsoid)
{
    SolidsDetail::X87Copy(normal.x, ellipsoid.normal.x);
    SolidsDetail::X87Copy(normal.y, ellipsoid.normal.y);
    SolidsDetail::X87Copy(normal.z, ellipsoid.normal.z);
    SolidsDetail::X87Copy(d, ellipsoid.d);
    SolidsDetail::X87Copy(center.x, ellipsoid.center.x);
    SolidsDetail::X87Copy(center.y, ellipsoid.center.y);
    SolidsDetail::X87Copy(center.z, ellipsoid.center.z);
    SolidsDetail::X87Copy(major, ellipsoid.major);
    SolidsDetail::X87Copy(minor, ellipsoid.minor);
    SolidsDetail::X87Copy(up, ellipsoid.up);
    SolidsDetail::X87Copy(point.x, ellipsoid.point.x);
    SolidsDetail::X87Copy(point.y, ellipsoid.point.y);
    SolidsDetail::X87Copy(point.z, ellipsoid.point.z);
    return *this;
}

const Vector3& __cdecl Ellipsoid::GetCenter() const
{
    return center;
}

float __cdecl Ellipsoid::GetMajor() const
{
    return major;
}

float __cdecl Ellipsoid::GetMinor() const
{
    return minor;
}

float __cdecl Ellipsoid::GetUp() const
{
    return up;
}

void __cdecl Ellipsoid::SetCenter(const Vector3& center_)
{
    SolidsDetail::X87Copy(center.x, center_.x);
    SolidsDetail::X87Copy(center.y, center_.y);
    SolidsDetail::X87Copy(center.z, center_.z);
}

void __cdecl Ellipsoid::SetMajor(float major_)
{
    major = major_;
}

void __cdecl Ellipsoid::SetMinor(float minor_)
{
    minor = minor_;
}

void __cdecl Ellipsoid::SetUp(float up_)
{
    up = up_;
}

bool __cdecl Ellipsoid::TestCollision(const AxisAlignedBox&) const
{
    return false;
}

bool __cdecl Ellipsoid::TestCollision(const Ellipsoid& ellipsoid) const
{
    static EllEllipsoidFn fn = Orig<EllEllipsoidFn>("?TestCollision@Ellipsoid@Jet@@UBA_NABV12@@Z");
    return fn != nullptr && fn(this, ellipsoid);
}

bool __cdecl Ellipsoid::TestCollision(const Cylinder&) const
{
    return false;
}

bool __cdecl Ellipsoid::TestCollision(const Sphere& sphere) const
{
    static EllSphereFn fn = Orig<EllSphereFn>("?TestCollision@Ellipsoid@Jet@@UBA_NABVSphere@2@@Z");
    return fn != nullptr && fn(this, sphere);
}

bool __cdecl Ellipsoid::TestCollision(const Plane& plane)
{
    static EllPlaneFn fn = Orig<EllPlaneFn>("?TestCollision@Ellipsoid@Jet@@UAA_NABVPlane@2@@Z");
    return fn != nullptr && fn(this, plane);
}

bool __cdecl Ellipsoid::TestCollision(const Line& line)
{
    static EllLineFn fn = Orig<EllLineFn>("?TestCollision@Ellipsoid@Jet@@UAA_NABVLine@2@@Z");
    return fn != nullptr && fn(this, line);
}

bool __cdecl Ellipsoid::TestCollision(const Vector3& point)
{
    static EllVectorFn fn = Orig<EllVectorFn>("?TestCollision@Ellipsoid@Jet@@UAA_NABVVector3@2@@Z");
    return fn != nullptr && fn(this, point);
}

}  // namespace Jet
