// Jet::Cylinder and Jet::Cone (U1d2).
//
// Cylinder is 0x20 bytes: vftable +0x00, centers[2] +0x04, radius +0x1c. Its vtable order in TS12 is
// AxisAlignedBox (the shared "return false" stub), Ellipsoid, Cylinder, Sphere, Line, Vector3. The five
// collision overloads that need Line/Ellipsoid maths are local wrappers that call the same export in
// Render3DCore_7c_orig.dll (the .def would otherwise forward the identical name; the vtable needs an address).
//
// Cone is 0x20 bytes, no vftable: centers[2] +0x00, radius +0x18, cosTheta +0x1c. Only the point test is local;
// the Line/Sphere overloads still forward.
#include <jet/Render3DCoreSolids.h>

#include <cmath>
#include <cstring>

namespace Jet {

namespace {

constexpr float kTolerance = 9.9999997e-05f;  // 0x38d1b717

template <class Fn>
Fn Orig(const char* name)
{
    return reinterpret_cast<Fn>(SolidsDetail::OrigProc(name));
}

using CylEllipsoidFn = bool(__cdecl*)(const Cylinder*, const Ellipsoid&);
using CylCylinderFn = bool(__cdecl*)(const Cylinder*, const Cylinder&);
using CylSphereFn = bool(__cdecl*)(const Cylinder*, const Sphere&);
using CylLineFn = bool(__cdecl*)(Cylinder*, const Line&);
using CylVectorFn = bool(__cdecl*)(Cylinder*, const Vector3&);

}  // namespace

Cylinder::Cylinder()
{
    centers[0] = Vector3{0.0f, 0.0f, 0.0f};
    centers[1] = Vector3{0.0f, 0.0f, 0.0f};
    radius = 0.0f;
}

Cylinder::Cylinder(const Vector3& center1, const Vector3& center2, float radius_)
{
    SolidsDetail::X87Copy(centers[0].x, center1.x);
    SolidsDetail::X87Copy(centers[0].y, center1.y);
    SolidsDetail::X87Copy(centers[0].z, center1.z);
    SolidsDetail::X87Copy(centers[1].x, center2.x);
    SolidsDetail::X87Copy(centers[1].y, center2.y);
    SolidsDetail::X87Copy(centers[1].z, center2.z);
    radius = radius_;
}

Cylinder::Cylinder(const Cylinder& cylinder)
{
    centers[0].x = cylinder.centers[0].x;
    centers[0].y = cylinder.centers[0].y;
    centers[0].z = cylinder.centers[0].z;
    centers[1].x = cylinder.centers[1].x;
    centers[1].y = cylinder.centers[1].y;
    centers[1].z = cylinder.centers[1].z;
    SolidsDetail::X87Copy(radius, cylinder.radius);
}

Cylinder& Cylinder::operator=(const Cylinder& cylinder)
{
    SolidsDetail::X87Copy(centers[0].x, cylinder.centers[0].x);
    SolidsDetail::X87Copy(centers[0].y, cylinder.centers[0].y);
    SolidsDetail::X87Copy(centers[0].z, cylinder.centers[0].z);
    SolidsDetail::X87Copy(centers[1].x, cylinder.centers[1].x);
    SolidsDetail::X87Copy(centers[1].y, cylinder.centers[1].y);
    SolidsDetail::X87Copy(centers[1].z, cylinder.centers[1].z);
    SolidsDetail::X87Copy(radius, cylinder.radius);
    return *this;
}

const Vector3* __cdecl Cylinder::GetCenters() const
{
    return centers;
}

float __cdecl Cylinder::GetRadius() const
{
    return radius;
}

void __cdecl Cylinder::SetCenters(const Vector3* const newCenters)
{
    SolidsDetail::X87Copy(centers[0].x, newCenters[0].x);
    SolidsDetail::X87Copy(centers[0].y, newCenters[0].y);
    SolidsDetail::X87Copy(centers[0].z, newCenters[0].z);
    SolidsDetail::X87Copy(centers[1].x, newCenters[1].x);
    SolidsDetail::X87Copy(centers[1].y, newCenters[1].y);
    SolidsDetail::X87Copy(centers[1].z, newCenters[1].z);
}

void __cdecl Cylinder::SetRadius(float radius_)
{
    radius = radius_;
}

// 0x1004f940: the original's shared stub (used by many unsupported pairs) returns false.
bool __cdecl Cylinder::TestCollision(const AxisAlignedBox&) const
{
    return false;
}

bool __cdecl Cylinder::TestCollision(const Ellipsoid& ellipsoid) const
{
    static CylEllipsoidFn fn = Orig<CylEllipsoidFn>("?TestCollision@Cylinder@Jet@@UBA_NABVEllipsoid@2@@Z");
    return fn != nullptr && fn(this, ellipsoid);
}

bool __cdecl Cylinder::TestCollision(const Cylinder& cylinder) const
{
    static CylCylinderFn fn = Orig<CylCylinderFn>("?TestCollision@Cylinder@Jet@@UBA_NABV12@@Z");
    return fn != nullptr && fn(this, cylinder);
}

bool __cdecl Cylinder::TestCollision(const Sphere& sphere) const
{
    static CylSphereFn fn = Orig<CylSphereFn>("?TestCollision@Cylinder@Jet@@UBA_NABVSphere@2@@Z");
    return fn != nullptr && fn(this, sphere);
}

bool __cdecl Cylinder::TestCollision(const Line& line)
{
    static CylLineFn fn = Orig<CylLineFn>("?TestCollision@Cylinder@Jet@@UAA_NABVLine@2@@Z");
    return fn != nullptr && fn(this, line);
}

bool __cdecl Cylinder::TestCollision(const Vector3& point)
{
    static CylVectorFn fn = Orig<CylVectorFn>("?TestCollision@Cylinder@Jet@@UAA_NABVVector3@2@@Z");
    return fn != nullptr && fn(this, point);
}

// ---------------------------------------------------------------------------------------------------------------
// Cone

Cone::Cone()
{
    centers[0] = Vector3{0.0f, 0.0f, 0.0f};
    centers[1] = Vector3{0.0f, 0.0f, 0.0f};
    radius = 0.0f;
    cosTheta = 0.0f;
}

Cone::Cone(const Vector3& center1, const Vector3& center2, float radius_)
{
    SolidsDetail::X87Copy(centers[0].x, center1.x);
    SolidsDetail::X87Copy(centers[0].y, center1.y);
    SolidsDetail::X87Copy(centers[0].z, center1.z);
    SolidsDetail::X87Copy(centers[1].x, center2.x);
    SolidsDetail::X87Copy(centers[1].y, center2.y);
    SolidsDetail::X87Copy(centers[1].z, center2.z);
    radius = radius_;
    CalculateCosTheta();
}

Cone& Cone::operator=(const Cone& cone)
{
    SolidsDetail::X87Copy(centers[0].x, cone.centers[0].x);
    SolidsDetail::X87Copy(centers[0].y, cone.centers[0].y);
    SolidsDetail::X87Copy(centers[0].z, cone.centers[0].z);
    SolidsDetail::X87Copy(centers[1].x, cone.centers[1].x);
    SolidsDetail::X87Copy(centers[1].y, cone.centers[1].y);
    SolidsDetail::X87Copy(centers[1].z, cone.centers[1].z);
    SolidsDetail::X87Copy(radius, cone.radius);
    SolidsDetail::X87Copy(cosTheta, cone.cosTheta);
    return *this;
}

const Vector3& __cdecl Cone::GetCenter(int i) const
{
    return centers[i];
}

Vector3 __cdecl Cone::GetCenter(int i)
{
    return centers[i];
}

void __cdecl Cone::GetCenter(Vector3& out, int i) const
{
    SolidsDetail::X87Copy(out.x, centers[i].x);
    SolidsDetail::X87Copy(out.y, centers[i].y);
    SolidsDetail::X87Copy(out.z, centers[i].z);
}

float __cdecl Cone::GetCosTheta() const
{
    return cosTheta;
}

float __cdecl Cone::GetRadius() const
{
    return radius;
}

void __cdecl Cone::SetCenter(const Vector3& center, int i)
{
    SolidsDetail::X87Copy(centers[i].x, center.x);
    SolidsDetail::X87Copy(centers[i].y, center.y);
    SolidsDetail::X87Copy(centers[i].z, center.z);
    CalculateCosTheta();
}

void __cdecl Cone::SetRadius(float radius_)
{
    radius = radius_;
    CalculateCosTheta();
}

// 0x10020bf0: cos = r / sqrt(h^2 + r^2), computed in x87 from the unrounded length; the guards use the float
// length (h > 0) and the float radius (> 0).
void __cdecl Cone::CalculateCosTheta()
{
    const float dx = centers[1].x - centers[0].x;
    const float dy = centers[1].y - centers[0].y;
    const float dz = centers[1].z - centers[0].z;
    Vector3 axis{dx, dy, dz};
    const double length = SolidsDetail::Length(axis);
    const float lengthF = static_cast<float>(length);
    if (!(radius > 0.0f) || !(lengthF > 0.0f)) {
        cosTheta = 0.0f;
        return;
    }
    const double rr = static_cast<double>(radius);
    cosTheta = static_cast<float>(rr / std::sqrt(length * length + rr * rr));
}

// 0x10020560: dot(point-apex, base-apex) / (|base-apex| * |point-apex|) > cosTheta. The dot product is summed
// x87 (x, z then y) and rounded to float; the first magnitude is rounded to float before the product.
bool __cdecl Cone::TestCollision(const Vector3& point)
{
    const float d1x = point.x - centers[0].x;
    const float d1y = point.y - centers[0].y;
    const float d1z = point.z - centers[0].z;
    const float ax = centers[1].x - centers[0].x;
    const float ay = centers[1].y - centers[0].y;
    const float az = centers[1].z - centers[0].z;
    const float dot = static_cast<float>(((static_cast<double>(ax) * d1x + static_cast<double>(az) * d1z) +
                                          static_cast<double>(ay) * d1y));
    Vector3 toPoint{d1x, d1y, d1z};
    Vector3 axis{ax, ay, az};
    const double length1 = SolidsDetail::Length(toPoint);
    const float length1F = static_cast<float>(length1);
    const double length2 = SolidsDetail::Length(axis);
    const double ratio = static_cast<double>(dot) / (length2 * static_cast<double>(length1F));
    return ratio > static_cast<double>(cosTheta);
}

}  // namespace Jet
