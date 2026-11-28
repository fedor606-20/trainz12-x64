// Unit U1d1 of Render3DCore_7c: Jet::Line and the shared vector helpers, transcribed from the TS12 assembly.
//
// The class keeps the TS12 layout (vftable +0, direction +4, point +0x10). Ordinary members are __cdecl, the
// constructors and the assignment __thiscall. Several bodies use x87 double arithmetic in the original (the
// float values are promoted for the whole expression and rounded once at the end); that is kept here.
#include <jet/Render3DCoreLines.h>

#include <cmath>
#include <cstdint>
#include <cstring>

namespace Jet {

// KernelStandard_7c's exported s_sqrtLUT (a pointer variable); the import is bound by the loader exactly as the
// original module's is. The alternatename maps our C name onto the import.
extern "C" unsigned int* JetSqrtLutPtr;
#if defined(_M_IX86)
#pragma comment(linker, "/alternatename:_JetSqrtLutPtr=__imp_?s_sqrtLUT@StdMath@Jet@@0PAIA")
#endif

namespace {

// The other stream's class layouts: only the fields read here.
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

// TS12's normalise (0x10008b50): s = (x*x + y*y) + z*z; r = 1/sqrt(s) from s_sqrtLUT and two Newton steps, all
// in x87 double, rounded to float once, then the components are scaled. StdMath::InvSqrt is the same worker
// (KernelStandard 0x10014500) and its float-typed result is that rounded r.
void NormalizeDirection(Vector3& v)
{
    const float s = (v.x * v.x + v.y * v.y) + v.z * v.z;
    const float r = StdMath::InvSqrt(s);
    v.x *= r;
    v.y *= r;
    v.z *= r;
}

// TS12's approximate magnitude of a three-float vector (0x10008ab0), used by Distance(Line) for parallel lines.
// One seed from the same table, two Newton steps in x87 double, times the squared length, rounded to float.
float ApproxMagnitude(const Vector3& v)
{
    const float s = (v.x * v.x + v.y * v.y) + v.z * v.z;
    const unsigned int* lut = JetSqrtLutPtr;
    const uint32_t bits = [&] {
        uint32_t u;
        std::memcpy(&u, &s, sizeof u);
        return u;
    }();
    const uint32_t e = (bits >> 23) & 0xff;
    const uint32_t seedBits = (((0x17cu - e) << 22) & 0xff800000u) | (lut ? lut[(bits >> 15) & 0x1ff] : 0u);
    const double h = 0.5 * static_cast<double>(s);
    double y = [&] {
        float f;
        std::memcpy(&f, &seedBits, sizeof f);
        return static_cast<double>(f);
    }();
    y = y * (1.5 - (y * y) * h);
    y = y * (1.5 - (y * y) * h);
    return static_cast<float>(y * static_cast<double>(s));
}

// Row-vector times the row-major matrix, the grouping of Line::Rotate's direction (0x10029200).
Vector3 RotateDirection(const Vector3& d, const Matrix3& m)
{
    return Vector3{(m.m[0] * d.x + d.z * m.m[6]) + m.m[3] * d.y, (m.m[4] * d.y + m.m[7] * d.z) + m.m[1] * d.x,
                   (m.m[2] * d.x + d.y * m.m[5]) + d.z * m.m[8]};
}

// The same for a point (0x10029285 and LineSegment::Rotate 0x1002a360).
Vector3 RotatePoint(const Vector3& p, const Matrix3& m)
{
    return Vector3{(p.x * m.m[0] + m.m[3] * p.y) + p.z * m.m[6], (m.m[1] * p.x + m.m[4] * p.y) + m.m[7] * p.z,
                   (p.x * m.m[2] + p.y * m.m[5]) + p.z * m.m[8]};
}

}  // namespace

// 0x10004a70: the default constructor only writes the vftable.
Line::Line()
{
}

// 0x10029080: (direction, point); the direction is normalised.
Line::Line(const Vector3& d, const Vector3& p) : direction(d), point(p)
{
    NormalizeDirection(direction);
}

// 0x100290c0: two points; direction = p1 - p0 (SSE subtraction, no double promotion), point = p0.
Line::Line(const Vector3 points[2])
    : direction(Vector3{points[1].x - points[0].x, points[1].y - points[0].y, points[1].z - points[0].z}),
      point(points[0])
{
    NormalizeDirection(direction);
}

// 0x10029140
Line::Line(const Line& line) : direction(line.direction), point(line.point)
{
}

// 0x10029180 (shared with the Ray overload): copy, no normalisation.
Line::Line(const LineSegment& line) : direction(line.direction), point(line.point)
{
}

Line::Line(const Ray& line) : direction(line.direction), point(line.point)
{
}

// 0x10004b70 (shared with Ray::operator=)
Line& Line::operator=(const Line& line)
{
    direction = line.direction;
    point = line.point;
    return *this;
}

// 0x10004b50 (shared with LineSegment/Ray SetPoint): only the point.
void __cdecl Line::SetPoint(const Vector3& newPoint)
{
    point = newPoint;
}

void __cdecl Line::SetDirection(const Vector3& newDirection)
{
    direction = newDirection;
}

// 0x10004c60 / 0x1000b210-like leaf, shared by several classes.
const Vector3& __cdecl Line::GetPoint() const
{
    return point;
}

const Vector3& __cdecl Line::GetDirection() const
{
    return direction;
}

// 0x100291c0 (also Line::operator+=): only the point moves.
Line& __cdecl Line::Translate(const Vector3& delta)
{
    point.x = point.x + delta.x;
    point.y = delta.y + point.y;
    point.z = delta.z + point.z;
    return *this;
}

Line& __cdecl Line::operator+=(const Vector3& delta)
{
    point.x = point.x + delta.x;
    point.y = delta.y + point.y;
    point.z = delta.z + point.z;
    return *this;
}

// 0x10029470
Line& __cdecl Line::operator-=(const Vector3& delta)
{
    point.x = point.x - delta.x;
    point.y = point.y - delta.y;
    point.z = point.z - delta.z;
    return *this;
}

// 0x100294b0 (also Scale)
Line& __cdecl Line::operator*=(float s)
{
    point.x = point.x * s;
    point.y = s * point.y;
    point.z = s * point.z;
    return *this;
}

Line& __cdecl Line::Scale(float s)
{
    point.x = point.x * s;
    point.y = s * point.y;
    point.z = s * point.z;
    return *this;
}

// 0x100294e0: the reciprocal is formed once, then multiplied.
Line& __cdecl Line::operator/=(float s)
{
    const float inv = 1.0f / s;
    point.x = point.x * inv;
    point.y = point.y * inv;
    point.z = point.z * inv;
    return *this;
}

// 0x10029200: direction and point are transformed as a row vector times the matrix.
Line& __cdecl Line::Rotate(const Matrix3& m)
{
    direction = RotateDirection(direction, m);
    point = RotatePoint(point, m);
    return *this;
}

// 0x10029300: only the direction.
Line& __cdecl Line::SelfRotate(const Matrix3& m)
{
    const Vector3 d = direction;
    direction.x = (m.m[3] * d.y + m.m[6] * d.z) + m.m[0] * d.x;
    direction.y = (m.m[1] * d.x + m.m[4] * d.y) + m.m[7] * d.z;
    direction.z = (m.m[2] * d.x + m.m[5] * d.y) + m.m[8] * d.z;
    return *this;
}

// 0x10029390
Line& __cdecl Line::Normalize()
{
    NormalizeDirection(direction);
    return *this;
}

// 0x100293b0: squared length of (p - point) x direction against the threshold (the original compares the square).
bool __cdecl Line::IsColinear(const Vector3& p, float threshold) const
{
    const float dx = p.x - point.x;
    const float dy = p.y - point.y;
    const float dz = p.z - point.z;
    float cx = direction.z * dy - direction.y * dz;
    const float cy = direction.x * dz - direction.z * dx;
    const float cz = direction.y * dx - direction.x * dy;
    const float len2 = (cz * cz + cy * cy) + cx * cx;
    return len2 < threshold;
}

// 0x10004aa0: sum of the displacement squares in dx, dz, dy order minus the projection squared.
float __cdecl Line::DistanceSquared(const Vector3& testPoint) const
{
    const float dx = testPoint.x - point.x;
    const float dy = testPoint.y - point.y;
    const float dz = testPoint.z - point.z;
    const float dot = (direction.z * dz + direction.y * dy) + direction.x * dx;
    const double sum = (static_cast<double>(dx) * dx + static_cast<double>(dz) * dz) + static_cast<double>(dy) * dy;
    return static_cast<float>(sum - static_cast<double>(dot) * dot);
}

// 0x10029520: sqrt(sum - projection^2 / |direction|^2), all but the projection in x87 double.
float __cdecl Line::Distance(const Vector3& p) const
{
    const float dx = p.x - point.x;
    const float dy = p.y - point.y;
    const float dz = p.z - point.z;
    const float dot = (direction.z * dz + direction.y * dy) + direction.x * dx;
    const double sum = (static_cast<double>(dz) * dz + static_cast<double>(dy) * dy) + static_cast<double>(dx) * dx;
    const double dirsum =
        (static_cast<double>(direction.x) * direction.x + static_cast<double>(direction.y) * direction.y) +
        static_cast<double>(direction.z) * direction.z;
    return static_cast<float>(std::sqrt(sum - static_cast<double>(dot) * dot / dirsum));
}

// 0x100295e0: |(p1 - p2) . normalise(d1 x d2)|; for parallel lines the approximate |p1 - p2|.
float __cdecl Line::Distance(const Line& line) const
{
    const float cx = direction.y * line.direction.z - direction.z * line.direction.y;
    const float cy = direction.z * line.direction.x - direction.x * line.direction.z;
    const float cz = direction.x * line.direction.y - direction.y * line.direction.x;
    const float len2 = (cz * cz + cy * cy) + cx * cx;
    const Vector3 diff{point.x - line.point.x, point.y - line.point.y, point.z - line.point.z};
    if (len2 < 1e-4f)
        return ApproxMagnitude(diff);
    const float inv = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(len2)));
    const float nx = cx * inv;
    const float ny = cy * inv;
    const float nz = cz * inv;
    const float dot = (diff.z * nz + diff.y * ny) + diff.x * nx;
    return std::fabs(dot);
}

// 0x10029750: distance from the centre line minus the radius, against 1e-4.
bool __cdecl Line::TestCollision(const Sphere& sphere) const
{
    const SphereFields& s = reinterpret_cast<const SphereFields&>(sphere);
    const float dx = s.center.x - point.x;
    const float dy = s.center.y - point.y;
    const float dz = s.center.z - point.z;
    const float dot = (direction.y * dy + direction.z * dz) + direction.x * dx;
    const float sum = (dx * dx + dy * dy) + dz * dz;
    return ((sum - dot * dot) - s.radius * s.radius) < 1e-4f;
}

// 0x10029810: parallel to the plane (|n . direction| < 1e-6) is not a collision.
bool __cdecl Line::TestCollision(const Plane& plane_) const
{
    const PlaneFields& plane = reinterpret_cast<const PlaneFields&>(plane_);
    const float dot = (direction.z * plane.normal.z + direction.y * plane.normal.y) + direction.x * plane.normal.x;
    return dot >= 1e-6f || dot <= -1e-6f;
}

// 0x10029aa0: the quadratic (b^2 - 4ac) of a line against a sphere; t1/t2 are the two roots.
bool __cdecl Line::TestCollision(Vector3 center, float radius2, float& to1, float& to2) const
{
    const float bz = (point.z * direction.z) * 2.0f - (direction.z * center.z) * 2.0f;
    const float by = (point.y * direction.y) * 2.0f - (direction.y * center.y) * 2.0f;
    const float bx = (point.x * direction.x) * 2.0f - (direction.x * center.x) * 2.0f;
    const float b = (bz + by) + bx;
    const float a = (direction.z * direction.z + direction.y * direction.y) + direction.x * direction.x;
    const float cz = (point.z * point.z - point.z * center.z * 2.0f) + center.z * center.z;
    const float cy = (point.y * point.y - point.y * center.y * 2.0f) + center.y * center.y;
    const float cx = (point.x * point.x - point.x * center.x * 2.0f) + center.x * center.x;
    const float c = ((cz + cy) + cx) - radius2 * radius2;
    const float disc = b * b - ((c * a) * 4.0f);
    if (disc < 0.0f)
        return false;
    const float denom = a * 2.0f;
    if (disc == 0.0f) {
        const float t = (-0.0f - b) / denom;
        to1 = t;
        to2 = t;
        return true;
    }
    const float root = std::sqrt(disc);
    to1 = (root - b) / denom;
    to2 = ((-0.0f - b) - root) / denom;
    return true;
}

// 0x10029860: the nearer root.
bool __cdecl Line::TestCollision(Vector3 center, float radius2, Vector3& collisionPoint) const
{
    float t1, t2;
    if (!TestCollision(center, radius2, t1, t2))
        return false;
    float t = t2;
    if (std::fabs(t1) < std::fabs(t2))
        t = t1;
    collisionPoint.z = point.z + direction.z * t;
    collisionPoint.y = point.y + direction.y * t;
    collisionPoint.x = point.x + direction.x * t;
    return true;
}

// 0x10029950: both roots, to1 first.
bool __cdecl Line::TestCollision(Vector3 center, float radius2, Vector3& collisionPoint1,
                                 Vector3& collisionPoint2) const
{
    float t1, t2;
    if (!TestCollision(center, radius2, t1, t2))
        return false;
    collisionPoint1.x = point.x + direction.x * t1;
    collisionPoint1.y = point.y + direction.y * t1;
    collisionPoint1.z = point.z + direction.z * t1;
    collisionPoint2.x = point.x + direction.x * t2;
    collisionPoint2.y = point.y + direction.y * t2;
    collisionPoint2.z = point.z + direction.z * t2;
    return true;
}

}  // namespace Jet
