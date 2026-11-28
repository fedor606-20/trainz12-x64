// Unit U1d1 of Render3DCore_7c: Jet::LineSegment, transcribed from the TS12 assembly.
//
// TS12 layout: vftable +0, direction +4, point +0x10, endPoint +0x1c. The direction is the normalised
// endPoint - point. Ordinary members are __cdecl, constructors and assignment __thiscall. The distance helpers
// leave their x87 double in st(0) in the original, so Distance() sees the unrounded square; the shared worker and
// the exported wrapper are split the same way here.
#include <jet/Render3DCoreLines.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <windows.h>

namespace Jet {

namespace {

struct PlaneFields {
    Vector3 normal; // +0x00
    float d;        // +0x0c
};
static_assert(sizeof(PlaneFields) == 0x10, "Plane");

struct SphereFields {
    void* vftable;  // +0x00
    Vector3 center; // +0x04
    float radius;   // +0x10
};
static_assert(offsetof(SphereFields, center) == 0x4, "Sphere centre");
static_assert(offsetof(SphereFields, radius) == 0x10, "Sphere radius");

// 0x10008b50: the same normalise as Line.cpp (s = (x*x + y*y) + z*z, StdMath::InvSqrt, scale).
void NormalizeDirection(Vector3& v)
{
    const float s = (v.x * v.x + v.y * v.y) + v.z * v.z;
    const float r = StdMath::InvSqrt(s);
    v.x *= r;
    v.y *= r;
    v.z *= r;
}

Vector3 RotatePoint(const Vector3& p, const Matrix3& m)
{
    return Vector3{(p.x * m.m[0] + m.m[3] * p.y) + p.z * m.m[6], (m.m[1] * p.x + m.m[4] * p.y) + m.m[7] * p.z,
                   (p.x * m.m[2] + p.y * m.m[5]) + p.z * m.m[8]};
}

// 0x10004e70's body, kept in double because the original leaves it unrounded in st(0).
double __cdecl DistanceSquaredD(const Vector3& point, const Vector3& endPoint, const Vector3& direction,
                                const Vector3& testPoint)
{
    const float dx = testPoint.x - point.x;
    const float dy = testPoint.y - point.y;
    const float dz = testPoint.z - point.z;
    const float dot = (direction.z * dz + direction.y * dy) + direction.x * dx;
    const double sum = (static_cast<double>(dz) * dz + static_cast<double>(dy) * dy) + static_cast<double>(dx) * dx;
    if (!(dot > 0.0f))
        return sum;
    const float ex = endPoint.x - point.x;
    const float ey = endPoint.y - point.y;
    const float ez = endPoint.z - point.z;
    const float dotEnd = (direction.z * ez + direction.y * ey) + direction.x * ex;
    if (dot > dotEnd) {
        const double qx = static_cast<double>(testPoint.x) - endPoint.x;
        const double qy = static_cast<double>(testPoint.y) - endPoint.y;
        const double qz = static_cast<double>(testPoint.z) - endPoint.z;
        return (qz * qz + qy * qy) + qx * qx;
    }
    return sum - static_cast<double>(dot) * dot;
}

}  // namespace

// 0x10004c50: only the vftable.
LineSegment::LineSegment()
{
}

// 0x10029fd0: point, endPoint, then direction = normalise(endPoint - point).
LineSegment::LineSegment(const Vector3& p, const Vector3& e) : point(p), endPoint(e)
{
    direction = Vector3{e.x - p.x, e.y - p.y, e.z - p.z};
    NormalizeDirection(direction);
}

// 0x1002a0e0: two points in an array.
LineSegment::LineSegment(const Vector3 points[2]) : point(points[0]), endPoint(points[1])
{
    direction = Vector3{points[1].x - points[0].x, points[1].y - points[0].y, points[1].z - points[0].z};
    NormalizeDirection(direction);
}

// 0x1002a090
LineSegment::LineSegment(const LineSegment& line)
    : direction(line.direction), point(line.point), endPoint(line.endPoint)
{
}

// 0x10004c70
LineSegment& LineSegment::operator=(const LineSegment& line)
{
    direction = line.direction;
    point = line.point;
    endPoint = line.endPoint;
    return *this;
}

const Vector3& __cdecl LineSegment::GetPoint() const
{
    return point;
}

const Vector3& __cdecl LineSegment::GetDirection() const
{
    return direction;
}

// 0x10004cb0
const Vector3& __cdecl LineSegment::GetEndPoint() const
{
    return endPoint;
}

void __cdecl LineSegment::SetPoint(const Vector3& newPoint)
{
    point = newPoint;
}

void __cdecl LineSegment::SetDirection(const Vector3& newDirection)
{
    direction = newDirection;
}

// 0x10004cc0
void __cdecl LineSegment::SetEndPoint(const Vector3& newEndPoint)
{
    endPoint = newEndPoint;
}

// 0x10004ce0: projection onto the direction, clamped to the two ends.
Vector3 __cdecl LineSegment::GetClosestPoint(const Vector3& p) const
{
    const float dx = p.x - point.x;
    const float dy = p.y - point.y;
    const float dz = p.z - point.z;
    float dot = (direction.y * dy + direction.z * dz) + direction.x * dx;
    if (!(dot > 0.0f))
        return point;
    const float ex = endPoint.x - point.x;
    const float ey = endPoint.y - point.y;
    const float ez = endPoint.z - point.z;
    const float dotEnd = (direction.z * ez + direction.y * ey) + direction.x * ex;
    if (dot > dotEnd)
        return endPoint;
    return Vector3{point.x + direction.x * dot, point.y + direction.y * dot, point.z + direction.z * dot};
}

// 0x10004e70: squared distance to the segment (the unrounded double behind the float export).
float __cdecl LineSegment::DistanceSquared(const Vector3& p) const
{
    return static_cast<float>(DistanceSquaredD(point, endPoint, direction, p));
}

// 0x1002a2c0: sqrt of the unrounded square.
float __cdecl LineSegment::Distance(const Vector3& p) const
{
    return static_cast<float>(std::sqrt(DistanceSquaredD(point, endPoint, direction, p)));
}

// 0x1002a1a0: the point lies between the two planes through point and endPoint; NaN is false.
bool __cdecl LineSegment::InSegment(const Vector3& p) const
{
    const float dx = point.x - p.x;
    const float dy = point.y - p.y;
    const float dz = point.z - p.z;
    const float ex = endPoint.x - p.x;
    const float ey = endPoint.y - p.y;
    const float ez = endPoint.z - p.z;
    const float dot1 = (direction.z * dz + direction.y * dy) + direction.x * dx;
    const float dot2 = (direction.z * ez + direction.y * ey) + direction.x * ex;
    return (dot1 * dot2) <= 0.0f;
}

// 0x1002a280: compare the unrounded squared distance against the threshold.
bool __cdecl LineSegment::IsColinear(const Vector3& p, float threshold) const
{
    return static_cast<double>(threshold) > DistanceSquaredD(point, endPoint, direction, p);
}

// 0x1002a2b0
void __cdecl LineSegment::Normalize()
{
    NormalizeDirection(direction);
}

// 0x1002a2e0: both ends move.
void __cdecl LineSegment::Translate(const Vector3& delta)
{
    point.x = delta.x + point.x;
    point.z = delta.z + point.z;
    point.y = delta.y + point.y;
    endPoint.x = endPoint.x + delta.x;
    endPoint.y = endPoint.y + delta.y;
    endPoint.z = endPoint.z + delta.z;
}

// 0x1002a360: direction, point and endPoint as row-vector times matrix.
void __cdecl LineSegment::Rotate(const Matrix3& m)
{
    direction = RotatePoint(direction, m);
    point = RotatePoint(point, m);
    endPoint = RotatePoint(endPoint, m);
}

// 0x1002a640: squared distance minus the squared radius, against 1e-4 (NaN is false).
bool __cdecl LineSegment::TestCollision(const Sphere& sphere) const
{
    const SphereFields& s = reinterpret_cast<const SphereFields&>(sphere);
    const double d2 = DistanceSquaredD(point, endPoint, direction, s.center);
    return (d2 - static_cast<double>(s.radius) * s.radius) < static_cast<double>(1e-4f);
}

// 0x1002a4e0: the two signed plane distances multiplied; a product above zero is no collision (NaN is false).
bool __cdecl LineSegment::TestCollision(const Plane& plane_) const
{
    const PlaneFields& plane = reinterpret_cast<const PlaneFields&>(plane_);
    const float d = plane.d;
    const float n0 = (plane.normal.x * d - point.x);
    const float n1 = (plane.normal.y * d - point.y);
    const float n2 = (plane.normal.z * d - point.z);
    const float first = (plane.normal.y * n1 + plane.normal.z * n2) + plane.normal.x * n0;
    const float e0 = (plane.normal.x * d - endPoint.x);
    const float e1 = (plane.normal.y * d - endPoint.y);
    const float e2 = (plane.normal.z * d - endPoint.z);
    const float second = (plane.normal.y * e1 + plane.normal.x * e0) + plane.normal.z * e2;
    return (first * second) <= 0.0f;
}

}  // namespace Jet
