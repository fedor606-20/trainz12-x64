// Render3DCore_7c wave 6: Jet::Tube (0x1c bytes: two centres then the radius).
//
// Addresses: 0x1000721d0 default ctor, 0x100072200 (Vector3, Vector3, float) ctor, 0x100072160 assignment,
// 0x100072110 GetRadius, 0x100072150 SetRadius, 0x100072120 SetCenters, 0x1000721a0 SetCenter,
// 0x100020360/0x100020370/0x1000203a0 GetCenter, 0x10000b210 GetCenters, and the four TestCollision methods
// (0x100072240 point, 0x1000722f0 line, 0x1000723a0 sphere, 0x100072460 tube).
//
// The tests build the centre line (normalised with 0x10008b50) and compare Line::Distance minus the radius (and the
// other radius) against the constant 1e-4 at 0x10007d27c with x87's `jbe`, so unordered also answers false.
#include "SimpleRemain.h"

namespace Jet {

namespace {

using namespace SolidsDetail;

constexpr float kTubeTolerance = 1.0e-4f;  // 0x10007d27c

// The original leaves Line::Distance's value in st(0) (extended) and subtracts the radius before comparing. A
// float here only differs in the last bits; the answer changes only exactly at the tolerance.
bool BelowTolerance(float lineDistance, float radii)
{
    return (lineDistance - radii) < kTubeTolerance;
}

Line CenterLine(const Tube& tube)
{
    Line line;
    line.direction.x = SubSS(tube.centers[1].x, tube.centers[0].x);
    line.direction.y = SubSS(tube.centers[1].y, tube.centers[0].y);
    line.direction.z = SubSS(tube.centers[1].z, tube.centers[0].z);
    line.point = tube.centers[0];
    SolidsDetail::Normalize(line.direction);
    return line;
}

}  // namespace

Tube::Tube()
{
    centers[0].x = 0.0f;
    centers[0].y = 0.0f;
    centers[0].z = 0.0f;
    centers[1].x = 0.0f;
    centers[1].y = 0.0f;
    centers[1].z = 0.0f;
    radius = 0.0f;
}

Tube::Tube(const Vector3& start, const Vector3& end, float radius_)
{
    SolidsDetail::X87Copy(centers[0].x, start.x);
    SolidsDetail::X87Copy(centers[0].y, start.y);
    SolidsDetail::X87Copy(centers[0].z, start.z);
    SolidsDetail::X87Copy(centers[1].x, end.x);
    SolidsDetail::X87Copy(centers[1].y, end.y);
    SolidsDetail::X87Copy(centers[1].z, end.z);
    radius = radius_;
}

Tube& Tube::operator=(const Tube& o)
{
    for (int i = 0; i < 6; ++i) {
        SolidsDetail::X87Copy(reinterpret_cast<float*>(centers)[i], reinterpret_cast<const float*>(o.centers)[i]);
    }
    SolidsDetail::X87Copy(radius, o.radius);
    return *this;
}

const Vector3& __cdecl Tube::GetCenter(int index) const
{
    return centers[index];
}

Vector3 __cdecl Tube::GetCenter(int index)
{
    return centers[index];
}

void __cdecl Tube::GetCenter(Vector3& out, int index) const
{
    out = centers[index];
}

Vector3* __cdecl Tube::GetCenters() const
{
    return const_cast<Vector3*>(centers);
}

float __cdecl Tube::GetRadius() const
{
    return radius;
}

void __cdecl Tube::SetCenter(const Vector3& center, int index)
{
    centers[index] = center;
}

void __cdecl Tube::SetCenters(const Vector3* const centers_)
{
    centers[0] = centers_[0];
    centers[1] = centers_[1];
}

void __cdecl Tube::SetRadius(float radius_)
{
    radius = radius_;
}

bool __cdecl Tube::TestCollision(const Vector3& point)
{
    const Line line = CenterLine(*this);
    return BelowTolerance(line.Distance(point), radius);
}

bool __cdecl Tube::TestCollision(const Line& line)
{
    const Line center = CenterLine(*this);
    return BelowTolerance(center.Distance(line), radius);
}

bool __cdecl Tube::TestCollision(const Sphere& sphere)
{
    const Line center = CenterLine(*this);
    const float sphereRadius = *reinterpret_cast<const float*>(reinterpret_cast<const char*>(&sphere) + 0x10);
    return BelowTolerance(center.Distance(sphere.GetCenter()), radius + sphereRadius);
}

bool __cdecl Tube::TestCollision(const Tube& tube)
{
    const Line self = CenterLine(*this);
    const Line other = CenterLine(tube);
    return BelowTolerance(self.Distance(other), radius + tube.radius);
}

}  // namespace Jet
