// Render3DCore_7c tails: the U1d2 solids exports that only needed the now-local Line/Plane/Polygon paths.
// Sphere's Ray/LineSegment tests, Polygon::Intersect, and the Circle test/overlap family. The arithmetic keeps the
// original's operand order through SolidsDetail's scalar SSE helpers (NaN payloads) and its x87 comparison habits.
#include <jet/Render3DCoreLines.h>
#include <jet/Render3DCorePlane.h>
#include <jet/Render3DCoreSolids.h>

namespace Jet {

namespace {

constexpr float kTolerance = 0.0001f;  // 0x1007d27c

inline Plane AsPlane(const Vector3& normal, float d)
{
    Plane plane;
    plane.normal = normal;
    plane.d = d;
    return plane;
}

// Circle-side plane test helper: both distances in the original are built from (normal . v) - d with the same
// component order the non-virtual Plane code uses.
inline float PlaneDistance(const Vector3& normal, float d, const Vector3& v)
{
    return SolidsDetail::SubSS(
        SolidsDetail::AddSS(SolidsDetail::AddSS(SolidsDetail::MulSS(normal.x, v.x),
                                                SolidsDetail::MulSS(normal.y, v.y)),
                            SolidsDetail::MulSS(normal.z, v.z)),
        d);
}

}  // namespace

// 0x10006570 / 0x100065b0: distanceSquared - radius*radius < 0.0001. The original keeps the whole comparison in
// x87 (the radius product and the subtraction are extended), and its JBE also rejects an unordered compare. Inline
// x87 reproduces both the last-bit behaviour and the NaN answer.
bool __cdecl Sphere::TestCollision(const Ray& ray) const
{
    volatile float distanceSquared = ray.DistanceSquared(center);
    volatile float radiusValue = radius;
    volatile float tolerance = kTolerance;
    unsigned char hit;
    __asm {
        fld dword ptr [distanceSquared]
        fld dword ptr [radiusValue]
        fmul st, st
        fsubp st(1), st
        fld dword ptr [tolerance]
        fcomip st, st(1)
        fstp st(0)
        seta hit
    }
    return hit != 0;
}

bool __cdecl Sphere::TestCollision(const LineSegment& segment) const
{
    volatile float distanceSquared = segment.DistanceSquared(center);
    volatile float radiusValue = radius;
    volatile float tolerance = kTolerance;
    unsigned char hit;
    __asm {
        fld dword ptr [distanceSquared]
        fld dword ptr [radiusValue]
        fmul st, st
        fsubp st(1), st
        fld dword ptr [tolerance]
        fcomip st, st(1)
        fstp st(0)
        seta hit
    }
    return hit != 0;
}

// 0x10054e70: the polygon is its own plane; intersect the line with it and test the point in 3D.
bool __cdecl Polygon::Intersect(const Line& line, Vector3& intersection) const
{
    const Plane plane = AsPlane(normal, d);
    intersection = plane.Intersect(line);
    return IsInside(intersection);
}

// 0x1001dc50: point-on-plane distance to the circle centre, then subtract the radius squared.
bool __cdecl Circle::TestCollision(const Line& line)
{
    const Plane plane = AsPlane(normal, d);
    const Vector3 point = plane.Intersect(line);

    const float dx = SolidsDetail::SubSS(point.x, center.x);
    const float dz = SolidsDetail::SubSS(point.z, center.z);
    const float dy = SolidsDetail::SubSS(point.y, center.y);
    const float sum = SolidsDetail::AddSS(SolidsDetail::AddSS(SolidsDetail::MulSS(dx, dx),
                                                             SolidsDetail::MulSS(dz, dz)),
                                          SolidsDetail::MulSS(dy, dy));
    const float difference = SolidsDetail::SubSS(sum, SolidsDetail::MulSS(radius, radius));
    return difference < kTolerance;
}

// 0x1001dcf0: the compiled body intersects the *argument* plane with itself, which leaves a degenerate line; the
// distance is then never within 0.0001. Keep the same computation so the export stays bit-identical (always false,
// including NaN inputs).
bool __cdecl Circle::TestCollision(const Plane& plane)
{
    Plane argument = plane;
    const Line line = argument.Intersect(plane);
    float difference = SolidsDetail::SubSS(line.Distance(center), radius);
    if (difference < 0.0f)
        difference = SolidsDetail::SubSS(-0.0f, difference);
    return difference < kTolerance;
}

// 0x1001deb0: intersect the two circle planes, clip the line against both circles, then test the two chords.
bool __cdecl Circle::TestCollision(const Circle& circle)
{
    const Plane mine = AsPlane(normal, d);
    const Plane theirs = AsPlane(circle.normal, circle.d);
    const Line line = mine.Intersect(theirs);

    Vector3 myNear, myFar, theirNear, theirFar;
    const bool mineHit = line.TestCollision(center, radius, myNear, myFar);
    const bool theirsHit = line.TestCollision(circle.center, circle.radius, theirNear, theirFar);
    if (mineHit && theirsHit)
        return Overlap(myNear, myFar, theirNear, theirFar);
    return false;
}

// 0x1001df70: the original's plain side tests on the four chord points; the formulas and their order are the
// decompiled ones, including the unordered (NaN) branch behaviour.
bool __cdecl Circle::Overlap(Vector3& a, Vector3& b, Vector3& c, Vector3& d)
{
    const float ac = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(a.y, SolidsDetail::SubSS(c.y, a.y)),
                            SolidsDetail::MulSS(a.z, SolidsDetail::SubSS(c.z, a.z))),
        SolidsDetail::MulSS(a.x, SolidsDetail::SubSS(c.x, a.x)));
    const float ad = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(a.z, SolidsDetail::SubSS(d.z, a.z)),
                            SolidsDetail::MulSS(a.y, SolidsDetail::SubSS(d.y, a.y))),
        SolidsDetail::MulSS(a.x, SolidsDetail::SubSS(d.x, a.x)));
    const float bc = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(b.z, SolidsDetail::SubSS(c.z, b.z)),
                            SolidsDetail::MulSS(b.y, SolidsDetail::SubSS(c.y, b.y))),
        SolidsDetail::MulSS(b.x, SolidsDetail::SubSS(c.x, b.x)));
    const float bd = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(b.z, SolidsDetail::SubSS(d.z, b.z)),
                            SolidsDetail::MulSS(b.y, SolidsDetail::SubSS(d.y, b.y))),
        SolidsDetail::MulSS(b.x, SolidsDetail::SubSS(d.x, b.x)));

    if (ac < 0.0f && ((bd < 0.0f && bc < 0.0f) && ad < 0.0f))
        return false;
    if (0.0f < bd)
        return ac <= 0.0f || (bc <= 0.0f || ad <= 0.0f);
    return true;
}

// 0x10024100: the ellipse's version of the chord side test. It projects the four points on the ellipse's
// in-plane basis (b - a) and uses the same branch shape as Circle::Overlap, with the original's z,y,x order.
bool __cdecl Ellipse::Overlap(Vector3& a, Vector3& b, Vector3& c, Vector3& d)
{
    const float bx = SolidsDetail::SubSS(b.x, a.x);
    const float by = SolidsDetail::SubSS(b.y, a.y);
    const float bz = SolidsDetail::SubSS(b.z, a.z);

    const float v1 = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(bz, SolidsDetail::SubSS(c.z, a.z)),
                            SolidsDetail::MulSS(by, SolidsDetail::SubSS(c.y, a.y))),
        SolidsDetail::MulSS(bx, SolidsDetail::SubSS(c.x, a.x)));
    const float v2 = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(bz, SolidsDetail::SubSS(d.z, a.z)),
                            SolidsDetail::MulSS(by, SolidsDetail::SubSS(d.y, a.y))),
        SolidsDetail::MulSS(bx, SolidsDetail::SubSS(d.x, a.x)));
    const float v3 = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(bz, SolidsDetail::SubSS(c.z, b.z)),
                            SolidsDetail::MulSS(by, SolidsDetail::SubSS(c.y, b.y))),
        SolidsDetail::MulSS(bx, SolidsDetail::SubSS(c.x, b.x)));
    const float v4 = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(bz, SolidsDetail::SubSS(d.z, b.z)),
                            SolidsDetail::MulSS(by, SolidsDetail::SubSS(d.y, b.y))),
        SolidsDetail::MulSS(bx, SolidsDetail::SubSS(d.x, b.x)));

    if (v1 < 0.0f && ((v4 < 0.0f && v3 < 0.0f) && v2 < 0.0f))
        return false;
    if (0.0f < v4)
        return v1 <= 0.0f || (v3 <= 0.0f || v2 <= 0.0f);
    return true;
}

}  // namespace Jet
