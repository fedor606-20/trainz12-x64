// Unit U1d1 of Render3DCore_7c: Jet::Ray, transcribed from the TS12 assembly.
//
// Same fields as Line (vftable +0, direction +4, point +0x10) but its own vftable (plane then sphere) and a
// one-sided distance: the projection is only removed when it points forward.
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

// 0x10008b50
void NormalizeDirection(Vector3& v)
{
    const float s = (v.x * v.x + v.y * v.y) + v.z * v.z;
    const float r = StdMath::InvSqrt(s);
    v.x *= r;
    v.y *= r;
    v.z *= r;
}

// 0x10004ba0's body in double (the original leaves it unrounded in st(0)).
double __cdecl DistanceSquaredD(const Vector3& point, const Vector3& direction, const Vector3& testPoint)
{
    const float dx = testPoint.x - point.x;
    const float dy = testPoint.y - point.y;
    const float dz = testPoint.z - point.z;
    const float dot = (direction.z * dz + direction.y * dy) + direction.x * dx;
    const double sum = (static_cast<double>(dx) * dx + static_cast<double>(dz) * dz) + static_cast<double>(dy) * dy;
    if (dot > 0.0f)
        return sum - static_cast<double>(dot) * dot;
    return sum;
}

}  // namespace

// 0x10004b40
Ray::Ray()
{
}

// 0x10058b80: (direction, point), normalised direction.
Ray::Ray(const Vector3& d, const Vector3& p) : direction(d), point(p)
{
    NormalizeDirection(direction);
}

// 0x10058bc0: two points.
Ray::Ray(const Vector3 points[2])
    : direction(Vector3{points[1].x - points[0].x, points[1].y - points[0].y, points[1].z - points[0].z}),
      point(points[0])
{
    NormalizeDirection(direction);
}

// 0x10058c40 (shared with Ray(const LineSegment&)): copy, no normalisation.
Ray::Ray(const Ray& ray) : direction(ray.direction), point(ray.point)
{
}

Ray::Ray(const LineSegment& line) : direction(line.direction), point(line.point)
{
}

// 0x10004b70
Ray& Ray::operator=(const Ray& ray)
{
    direction = ray.direction;
    point = ray.point;
    return *this;
}

const Vector3& __cdecl Ray::GetPoint() const
{
    return point;
}

const Vector3& __cdecl Ray::GetDirection() const
{
    return direction;
}

void __cdecl Ray::SetPoint(const Vector3& newPoint)
{
    point = newPoint;
}

void __cdecl Ray::SetDirection(const Vector3& newDirection)
{
    direction = newDirection;
}

// 0x1002a2b0 (shared body)
void __cdecl Ray::Normalize()
{
    NormalizeDirection(direction);
}

// 0x10004ba0
float __cdecl Ray::DistanceSquared(const Vector3& testPoint) const
{
    return static_cast<float>(DistanceSquaredD(point, direction, testPoint));
}

// 0x58c80: compare the unrounded square.
bool __cdecl Ray::IsColinear(const Vector3& p, float threshold) const
{
    return static_cast<double>(threshold) > DistanceSquaredD(point, direction, p);
}

// 0x10058cb0
float __cdecl Ray::Distance(const Vector3& p) const
{
    return static_cast<float>(std::sqrt(DistanceSquaredD(point, direction, p)));
}

// 0x10058cd0: squared distance minus radius squared, against 1e-4 (NaN is false).
bool __cdecl Ray::TestCollision(const Sphere& sphere) const
{
    const SphereFields& s = reinterpret_cast<const SphereFields&>(sphere);
    const double d2 = DistanceSquaredD(point, direction, s.center);
    return (d2 - static_cast<double>(s.radius) * s.radius) < static_cast<double>(1e-4f);
}

// 0x10058d10: direction against the plane normal, then the point's height with the plane's sign rules.
bool __cdecl Ray::TestCollision(const Plane& plane_) const
{
    const PlaneFields& plane = reinterpret_cast<const PlaneFields&>(plane_);
    const float dot = (direction.y * plane.normal.y + direction.z * plane.normal.z) + direction.x * plane.normal.x;
    const float height =
        ((point.y * plane.normal.y + point.z * plane.normal.z) + point.x * plane.normal.x) - plane.d;
    if (dot >= 1e-6f)
        return !(height > 1e-4f);
    if (!(dot <= -1e-6f))
        return false;
    if (-1e-4f > height)
        return false;
    return true;
}

// 0x10059020: the ray/sphere quadratic, one-sided.
bool __cdecl Ray::TestCollision(Vector3 center, float radius2, float& to1, float& to2) const
{
    const float dx = center.x - point.x;
    const float dy = center.y - point.y;
    const float dz = center.z - point.z;
    const float dot = (direction.z * dz + direction.y * dy) + direction.x * dx;
    const float sum = (dx * dx + dz * dz) + dy * dy;
    const float rsq = radius2 * radius2;
    if (dot > 0.0f) {
        const float disc = rsq - (sum - dot * dot);
        if (disc < -1e-6f)
            return false;
        if (disc <= 1e-6f) {
            to1 = dot;
            to2 = dot;
            return true;
        }
        const float root = std::sqrt(disc);
        to1 = root + dot;
        float t2 = dot - root;
        if (t2 < 0.0f)
            t2 = to1;
        to2 = t2;
        return true;
    }
    const float disc = rsq - sum;
    if (-1e-6f > disc)
        return false;
    const float val = dot * dot + disc;
    if (!(val <= 0.0f)) {
        const float root = std::sqrt(val);
        to1 = root + dot;
        to2 = root + dot;
    } else {
        to1 = 0.0f;
        to2 = 0.0f;
    }
    return true;
}

// 0x10058de0: the smaller root.
bool __cdecl Ray::TestCollision(Vector3 center, float radius2, Vector3& collisionPoint) const
{
    float t1, t2;
    if (!TestCollision(center, radius2, t1, t2))
        return false;
    float t = t1;
    if (t2 < t1)
        t = t2;
    collisionPoint.z = point.z + direction.z * t;
    collisionPoint.y = point.y + direction.y * t;
    collisionPoint.x = point.x + direction.x * t;
    return true;
}

// 0x10058ed0: both roots.
bool __cdecl Ray::TestCollision(Vector3 center, float radius2, Vector3& collisionPoint1,
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
