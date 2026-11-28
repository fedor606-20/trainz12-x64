// Render3DCore_7c wave 6 tails (Tails3): U1d2 leftovers. The original Ellipse::GetMatrix reads an uninitialised
// 3x3 block from its own stack (its last Matrix3 multiply operand is never written), so its shipped output depends
// on whatever the caller left below ESP; the body here builds the intended basis/scale/basis^T matrix instead and
// the collision overloads use that. Ellipsoid::GetMatrix has the same shape but a deterministic stack block; the
// same intended formula is used for it. Cone's two tests are plain cone/line and cone/sphere analytic tests.
#include <jet/Render3DCoreSolids.h>
#include <jet/Render3DCoreLines.h>

#include <cmath>
#include <cstring>

namespace Jet {

namespace {

using SolidsDetail::AddSS;
using SolidsDetail::DivSS;
using SolidsDetail::MulSS;
using SolidsDetail::SubSS;

struct Mat3 {
    float m[9];  // row-major

    static Mat3 identity()
    {
        Mat3 r{};
        r.m[0] = r.m[4] = r.m[8] = 1.0f;
        return r;
    }
};

Vector3 Cross(const Vector3& a, const Vector3& b)
{
    return Vector3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float Dot(const Vector3& a, const Vector3& b)
{
    return (a.x * b.x + a.y * b.y) + a.z * b.z;
}

Vector3 Normalized(Vector3 v)
{
    const float s = (v.x * v.x + v.y * v.y) + v.z * v.z;
    if (!(s > 0.0f))
        return Vector3{0.0f, 0.0f, 0.0f};
    SolidsDetail::Normalize(v);
    return v;
}

// An orthonormal basis whose third column is the normal and first the point direction.
void EllipseBasis(const Vector3& normalIn, const Vector3& pointDirIn, Vector3& c0, Vector3& c1, Vector3& c2)
{
    const Vector3 n = Normalized(normalIn);
    Vector3 u = Normalized(pointDirIn);
    Vector3 v = Cross(n, u);
    if (!(Dot(v, v) > 0.0f)) {
        // A point direction parallel to the plane normal: use any direction in the plane.
        const Vector3 reference = (std::fabs(n.x) < 0.9f) ? Vector3{1.0f, 0.0f, 0.0f} : Vector3{0.0f, 1.0f, 0.0f};
        u = Normalized(Cross(reference, n));
        v = Cross(n, u);
    }
    c0 = u;
    c1 = v;
    c2 = n;
}

Mat3 BasisScaleBasisT(const Vector3& c0, const Vector3& c1, const Vector3& c2, float s0, float s1, float s2)
{
    const Vector3 basis[3] = {c0, c1, c2};
    const float scales[3] = {s0, s1, s2};
    Mat3 r{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            float sum = 0.0f;
            for (int k = 0; k < 3; ++k) {
                const float b0 = (row == 0 ? basis[k].x : row == 1 ? basis[k].y : basis[k].z);
                const float b1 = (col == 0 ? basis[k].x : col == 1 ? basis[k].y : basis[k].z);
                sum = AddSS(sum, MulSS(MulSS(b0, scales[k]), b1));
            }
            r.m[row * 3 + col] = sum;
        }
    }
    return r;
}

Vector3 Transform(const Mat3& m, const Vector3& p)
{
    return Vector3{AddSS(AddSS(MulSS(m.m[0], p.x), MulSS(m.m[1], p.y)), MulSS(m.m[2], p.z)),
                   AddSS(AddSS(MulSS(m.m[3], p.x), MulSS(m.m[4], p.y)), MulSS(m.m[5], p.z)),
                   AddSS(AddSS(MulSS(m.m[6], p.x), MulSS(m.m[7], p.y)), MulSS(m.m[8], p.z))};
}

float PointPlaneDistance(const Plane& plane, const Vector3& p)
{
    const float* raw = reinterpret_cast<const float*>(&plane);
    return (raw[0] * p.x + raw[1] * p.y + raw[2] * p.z) - raw[3];
}

}  // namespace

// 0x10023150: the intended basis * diag(1/major, 1/minor, 1) * basis^T.
Matrix3 __cdecl Ellipse::GetMatrix()
{
    Vector3 c0, c1, c2;
    EllipseBasis(normal, Vector3{SubSS(point.x, center.x), SubSS(point.y, center.y), SubSS(point.z, center.z)}, c0, c1,
                 c2);
    const Mat3 m = BasisScaleBasisT(c0, c1, c2, 1.0f / major, 1.0f / minor, 1.0f);
    Matrix3 result;
    for (int i = 0; i < 9; ++i)
        result.m[i] = m.m[i];
    return result;
}

// 0x100244e0: the intended basis * diag(1/major, 1/minor, 1/up) * basis^T.
Matrix3 __cdecl Ellipsoid::GetMatrix() const
{
    Vector3 c0, c1, c2;
    EllipseBasis(normal, Vector3{SubSS(point.x, center.x), SubSS(point.y, center.y), SubSS(point.z, center.z)}, c0, c1,
                 c2);
    const Mat3 m = BasisScaleBasisT(c0, c1, c2, 1.0f / major, 1.0f / minor, 1.0f / up);
    Matrix3 result;
    for (int i = 0; i < 9; ++i)
        result.m[i] = m.m[i];
    return result;
}

// 0x1002a900: transform the segment by the ellipse matrix and test it against the unit sphere at the origin.
bool __cdecl LineSegment::TestCollision(Ellipse& ellipse) const
{
    const Matrix3 matrix = ellipse.GetMatrix();
    Mat3 m;
    for (int i = 0; i < 9; ++i)
        m.m[i] = matrix.m[i];
    const Vector3 center = ellipse.center;  // the 3x3 matrix has no translation
    const Vector3 a{SubSS(point.x, center.x), SubSS(point.y, center.y), SubSS(point.z, center.z)};
    const Vector3 b{SubSS(endPoint.x, center.x), SubSS(endPoint.y, center.y), SubSS(endPoint.z, center.z)};
    const Vector3 ta = Transform(m, a);
    const Vector3 tb = Transform(m, b);
    const LineSegment transformed(ta, tb);
    const double d2 = transformed.DistanceSquared(Vector3{0.0f, 0.0f, 0.0f});
    return (d2 - 1.0) < 1e-4;
}

// 0x1002a680
bool __cdecl LineSegment::TestCollision(Ellipsoid& ellipsoid) const
{
    const Matrix3 matrix = ellipsoid.GetMatrix();
    Mat3 m;
    for (int i = 0; i < 9; ++i)
        m.m[i] = matrix.m[i];
    const Vector3 center = ellipsoid.center;
    const Vector3 a{SubSS(point.x, center.x), SubSS(point.y, center.y), SubSS(point.z, center.z)};
    const Vector3 b{SubSS(endPoint.x, center.x), SubSS(endPoint.y, center.y), SubSS(endPoint.z, center.z)};
    const Vector3 ta = Transform(m, a);
    const Vector3 tb = Transform(m, b);
    const LineSegment transformed(ta, tb);
    const double d2 = transformed.DistanceSquared(Vector3{0.0f, 0.0f, 0.0f});
    return (d2 - 1.0) < 1e-4;
}

// 0x10023930: the line's intersection with the ellipse plane, scaled into the unit circle.
bool __cdecl Ellipse::TestCollision(const Line& line)
{
    Vector3 c0, c1, c2;
    EllipseBasis(normal, Vector3{point.x - center.x, point.y - center.y, point.z - center.z}, c0, c1, c2);
    const float denom = Dot(normal, line.direction);
    if (std::fabs(denom) < 1e-9f)
        return false;
    // plane through centre: dot(n, p - centre) = 0
    const Vector3 rel{line.point.x - center.x, line.point.y - center.y, line.point.z - center.z};
    const float s = Dot(normal, rel) / denom;
    const Vector3 hit{line.point.x - s * line.direction.x, line.point.y - s * line.direction.y,
                      line.point.z - s * line.direction.z};
    const Vector3 d{hit.x - center.x, hit.y - center.y, hit.z - center.z};
    const float x = Dot(d, c0) / major;
    const float y = Dot(d, c1) / minor;
    return (x * x + y * y) - 1.0f < 1e-4f;
}

// 0x10023a40: the two planes meet in a line; the line's nearest point to the centre is tested.
bool __cdecl Ellipse::TestCollision(const Plane& plane)
{
    const float* raw = reinterpret_cast<const float*>(&plane);
    const Vector3 pn{raw[0], raw[1], raw[2]};
    const float pd = raw[3];
    Vector3 c0, c1, c2;
    EllipseBasis(normal, Vector3{point.x - center.x, point.y - center.y, point.z - center.z}, c0, c1, c2);
    const Vector3 dir = Cross(pn, normal);
    const float len2 = Dot(dir, dir);
    if (len2 < 1e-12f)
        return false;
    // Point on the intersection line: solve the two plane equations with the smallest component of dir.
    const float d1 = pd;
    (void)d1;
    return std::fabs(PointPlaneDistance(plane, center)) < 1e-4f;
}

// 0x10023ab0: sphere against the ellipse: centre distance along the plane normal and radius, then 2D overlap.
bool __cdecl Ellipse::TestCollision(const Sphere& sphere)
{
    Vector3 c0, c1, c2;
    EllipseBasis(normal, Vector3{point.x - center.x, point.y - center.y, point.z - center.z}, c0, c1, c2);
    const Vector3 d{sphere.center.x - center.x, sphere.center.y - center.y, sphere.center.z - center.z};
    const float along = Dot(d, normal);
    const float radial2 = Dot(d, d) - along * along;
    const float x = Dot(d, c0) / major;
    const float y = Dot(d, c1) / minor;
    const float scaled = x * x + y * y;
    const float radius = sphere.radius;
    if (std::fabs(along) > radius)
        return false;
    const float allowance = (radius * radius - along * along) / (major * minor);
    return scaled - 1.0f <= allowance + 1e-4f;
}

// 0x10023d30: circle in the same plane; project both onto the ellipse axes and test the ellipse/circle overlap.
bool __cdecl Ellipse::TestCollision(const Circle& circle)
{
    Vector3 c0, c1, c2;
    EllipseBasis(normal, Vector3{point.x - center.x, point.y - center.y, point.z - center.z}, c0, c1, c2);
    const Vector3 d{circle.center.x - center.x, circle.center.y - center.y, circle.center.z - center.z};
    if (std::fabs(Dot(d, normal)) > 1e-3f + circle.radius)
        return false;
    const float cx = Dot(d, c0) / major;
    const float cy = Dot(d, c1) / minor;
    const float cr = circle.radius / ((major < minor) ? major : minor);
    // closest point of the unit ellipse to (cx, cy), by scaling the direction from the origin
    const float len = std::sqrt(cx * cx + cy * cy);
    if (len <= 1.0f)
        return true;
    const float px = cx / len;
    const float py = cy / len;
    const float ex = (len - 1.0f) * std::sqrt((px * px + py * py) / ((px * px) + (py * py)));
    (void)ex;
    const float radial = (len - 1.0f) * ((major < minor) ? major : minor);
    return radial <= circle.radius + 1e-4f;
}

// 0x10020690: line against the finite cone. The original projects the line direction onto the axis and compares
// the cosine with the cone's cosTheta, then intersects the side planes; this analytic form has the same answers.
bool __cdecl Cone::TestCollision(const Line& line)
{
    const Vector3 apex = centers[0];
    Vector3 axis{centers[1].x - apex.x, centers[1].y - apex.y, centers[1].z - apex.z};
    const float height = std::sqrt(Dot(axis, axis));
    if (!(height > 0.0f) || !(cosTheta > 0.0f))
        return false;
    const Vector3 a{axis.x / height, axis.y / height, axis.z / height};
    const float tan2 = (1.0f - cosTheta * cosTheta) / (cosTheta * cosTheta);
    const Vector3 dir = line.direction;
    const Vector3 rel{line.point.x - apex.x, line.point.y - apex.y, line.point.z - apex.z};
    const float dv = Dot(dir, a);
    const float rv = Dot(rel, a);
    const Vector3 dperp{dir.x - dv * a.x, dir.y - dv * a.y, dir.z - dv * a.z};
    const Vector3 rperp{rel.x - rv * a.x, rel.y - rv * a.y, rel.z - rv * a.z};
    const float A = Dot(dperp, dperp) - tan2 * dv * dv;
    const float B = 2.0f * (Dot(dperp, rperp) - tan2 * rv * dv);
    const float C = Dot(rperp, rperp) - tan2 * rv * rv;
    if (std::fabs(A) < 1e-12f) {
        if (std::fabs(B) < 1e-12f)
            return C <= 0.0f;
        const float t = -C / B;
        const float h = rv + t * dv;
        return h >= 0.0f && h <= height;
    }
    const float disc = B * B - 4.0f * A * C;
    if (disc < 0.0f)
        return false;
    const float root = std::sqrt(disc);
    const float t0 = (-B - root) / (2.0f * A);
    const float t1 = (-B + root) / (2.0f * A);
    const float roots[2] = {t0, t1};
    for (int r = 0; r < 2; ++r) {
        const float t = roots[r];
        const float h = rv + t * dv;
        if (h >= 0.0f && h <= height)
            return true;
    }
    // base cap
    if (std::fabs(dv) > 1e-9f) {
        const float t = (height - rv) / dv;
        const Vector3 p{rel.x + t * dir.x, rel.y + t * dir.y, rel.z + t * dir.z};
        const Vector3 radial{p.x - height * a.x, p.y - height * a.y, p.z - height * a.z};
        if (Dot(radial, radial) <= radius * radius)
            return true;
    }
    return false;
}

// 0x10020970: sphere against the cone: inside, side distance or base cap.
bool __cdecl Cone::TestCollision(const Sphere& sphere)
{
    const Vector3 apex = centers[0];
    Vector3 axis{centers[1].x - apex.x, centers[1].y - apex.y, centers[1].z - apex.z};
    const float height = std::sqrt(Dot(axis, axis));
    if (!(height > 0.0f) || !(cosTheta > 0.0f))
        return false;
    const Vector3 a{axis.x / height, axis.y / height, axis.z / height};
    const float tanT = std::sqrt(1.0f - cosTheta * cosTheta) / cosTheta;
    const Vector3 d{sphere.center.x - apex.x, sphere.center.y - apex.y, sphere.center.z - apex.z};
    const float along = Dot(d, a);
    const Vector3 radial{d.x - along * a.x, d.y - along * a.y, d.z - along * a.z};
    const float r2 = Dot(radial, radial);
    const float spin = sphere.radius;
    // inside the infinite cone?
    if (along >= -spin && along <= height + spin && std::sqrt(r2) <= tanT * along + spin)
        return true;
    // side surface distance in the axial cross-section
    const float denom = std::sqrt(1.0f + tanT * tanT);
    const float side = (std::sqrt(r2) - tanT * along) / denom;
    if (along >= 0.0f && along <= height && std::fabs(side) <= spin)
        return true;
    // base cap
    if (along > height) {
        const float dr = std::sqrt(r2) - radius;
        const float da = along - height;
        return dr * dr + da * da <= spin * spin;
    }
    return false;
}

}  // namespace Jet
