// Unit U1d1 of Render3DCore_7c: the 2D line shapes (Line2D, LineSegment2D, Circle2D), from the TS12 assembly.
//
// Layouts: Line2D {direction, point}, LineSegment2D {v0, v1}, Circle2D {center, radius}; no vftables. Members are
// __cdecl, constructors/assignment __thiscall. The original computes most of these in x87 at double precision and
// rounds once on return; the functions that call each other do not round in between, so the workers below return
// double and the exported wrappers cast.
#include <jet/Render3DCoreLines.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace Jet {

namespace {

float FromBits(uint32_t u)
{
    float f;
    std::memcpy(&f, &u, sizeof f);
    return f;
}

uint32_t ToBits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

// The bit-hack reciprocal square root seed of the 2D code (0x5f3759df - bits/2), as a double.
double ApproxInv(float s) { return static_cast<double>(FromBits(0x5f3759dfu - (ToBits(s) >> 1))); }

// s * inv * (1.5 - (s*0.5)*inv*inv): the approximate magnitude, in x87 double.
double ApproxRootD(float s)
{
    const double inv = ApproxInv(s);
    return (inv * (1.5 - ((0.5 * static_cast<double>(s)) * inv) * inv)) * static_cast<double>(s);
}

// The same, dividing a numerator by it (the line separations), with |numerator| first.
double ApproxDivD(double numerator, float s)
{
    const double inv = ApproxInv(s);
    return std::fabs(numerator) /
           ((inv * (1.5 - ((0.5 * static_cast<double>(s)) * inv) * inv)) * static_cast<double>(s));
}

// Line2D::WhichSide's helper and LineSegment2D's side comparisons: cross(q,d) against cross(p,d), p = the line point.
int SideCompare(const Vector2& q, const Vector2& point, const Vector2& direction)
{
    const float lhs = q.x * direction.y - q.y * direction.x;
    const float rhs = point.x * direction.y - point.y * direction.x;
    return lhs > rhs ? 1 : 2;
}

// Same, with the line given by a point `p` and a direction `d` (the segment's side test uses its own point).
int SideCompareDir(const Vector2& q, const Vector2& p, const Vector2& d)
{
    const float lhs = q.x * d.y - q.y * d.x;
    const float rhs = p.x * d.y - p.y * d.x;
    return lhs > rhs ? 1 : 2;
}

// Distance from a point to an infinite line through p with direction d (the Line2D separation, double).
double PointLineDistanceD(const Vector2& p, const Vector2& d, const Vector2& q)
{
    // Products first (SSE float), then the two sides subtracted: the original's order, which lets an overflow
    // turn into inf - inf = NaN instead of staying finite.
    const float lhs = q.x * d.y - q.y * d.x;
    const float rhs = p.x * d.y - p.y * d.x;
    const float cross = lhs - rhs;
    return std::fabs(static_cast<double>(cross)) /
           std::sqrt((static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y));
}

// Line2D::GetSeparation(Vector2), unrounded.
double Line2DVectorSeparationD(const Line2D& line, const Vector2& q)
{
    const float dx = q.x - line.point.x;
    const float dy = q.y - line.point.y;
    const float lx = q.x * line.direction.y - q.y * line.direction.x;
    const float rx = line.point.x * line.direction.y - line.point.y * line.direction.x;
    const float d = lx - rx;
    (void)dx;
    (void)dy;
    return std::fabs(static_cast<double>(d)) /
           std::sqrt(static_cast<double>(line.direction.x) * line.direction.x +
                     static_cast<double>(line.direction.y) * line.direction.y);
}

// Line2D::GetApproxSeparation(Vector2), unrounded.
double Line2DVectorApproxD(const Line2D& line, const Vector2& q)
{
    const float s = line.direction.x * line.direction.x + line.direction.y * line.direction.y;
    const float lx = q.x * line.direction.y - q.y * line.direction.x;
    const float rx = line.point.x * line.direction.y - line.point.y * line.direction.x;
    return ApproxDivD(static_cast<double>(lx - rx), s);
}

// Circle2D's approximate separation from a distance squared, unrounded.
double CircleApproxD(float s, float radius)
{
    return ApproxRootD(s) - radius;
}

// LineSegment2D nearest-point distance to q, unrounded (all of its branches end in an x87 sqrt).
double Segment2DSeparationD(const LineSegment2D& seg, const Vector2& q)
{
    const float dx = seg.v1.x - seg.v0.x;
    const float dy = seg.v1.y - seg.v0.y;
    const float ax = q.x - seg.v0.x;
    const float ay = q.y - seg.v0.y;
    const float dot1 = dx * ax + dy * ay;
    if (dot1 < 0.0f)
        return std::sqrt(static_cast<double>(ay) * ay + static_cast<double>(ax) * ax);
    const float bx = q.x - seg.v1.x;
    const float by = q.y - seg.v1.y;
    const float dot2 = dx * bx + dy * by;
    if (dot2 > 0.0f)
        return std::sqrt(static_cast<double>(by) * by + static_cast<double>(bx) * bx);
    const float c1 = q.x * dy;
    const float c2 = q.y * dx;
    const float c3 = seg.v0.x * dy;
    const float c4 = seg.v0.y * dx;
    const float cross = (c1 - c2) - (c3 - c4);
    return std::fabs(static_cast<double>(cross)) /
           std::sqrt(static_cast<double>(dy) * dy + static_cast<double>(dx) * dx);
}

// The original's x87 sequences for the circle separations: the square root and the radius subtraction are done
// in extended precision, then rounded to float once (a double sqrt here would double-round by one ULP).
#if defined(_M_IX86)
static float X87CircleSeparationV(const Circle2D* circle, const Vector2* point)
{
    float sep;
    __asm {
        mov eax, circle
        mov ecx, point
        fld dword ptr [ecx + 4]
        fsub dword ptr [eax + 4]
        fld dword ptr [ecx]
        fsub dword ptr [eax]
        fld st(1)
        fmulp st(2), st(0)
        fmul st(0), st(0)
        faddp st(1), st(0)
        fsqrt
        fsub dword ptr [eax + 8]
        fstp sep
    }
    return sep;
}

static float X87CircleSeparationC(const Circle2D* circle, const Circle2D* other)
{
    float sep;
    __asm {
        mov eax, circle
        mov ecx, other
        fld dword ptr [ecx + 4]
        fsub dword ptr [eax + 4]
        fld dword ptr [ecx]
        fsub dword ptr [eax]
        fld st(1)
        fmulp st(2), st(0)
        fmul st(0), st(0)
        faddp st(1), st(0)
        fsqrt
        fsub dword ptr [eax + 8]
        fsub dword ptr [ecx + 8]
        fstp sep
    }
    return sep;
}
#else
static float X87CircleSeparationV(const Circle2D* circle, const Vector2* point)
{
    const double dx = static_cast<double>(point->x) - circle->center.x;
    const double dy = static_cast<double>(point->y) - circle->center.y;
    return static_cast<float>(std::sqrt(dy * dy + dx * dx) - circle->radius);
}
static float X87CircleSeparationC(const Circle2D* circle, const Circle2D* other)
{
    const double dx = static_cast<double>(other->center.x) - circle->center.x;
    const double dy = static_cast<double>(other->center.y) - circle->center.y;
    return static_cast<float>((std::sqrt(dy * dy + dx * dx) - circle->radius) - other->radius);
}
#endif

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Line2D

// 0x1004fd0: the shared empty constructor.
Line2D::Line2D()
{
}

// 0x1001e6c0: (direction, point).
Line2D::Line2D(const Vector2& d, const Vector2& p) : direction(d), point(p)
{
}

// 0x1001ec50 (shared with LineSegment2D)
Line2D& Line2D::operator=(const Line2D& line)
{
    direction = line.direction;
    point = line.point;
    return *this;
}

const Vector2& __cdecl Line2D::GetDirection() const
{
    return direction;
}

// 0x1001e720
const Vector2& __cdecl Line2D::GetPoint() const
{
    return point;
}

void __cdecl Line2D::SetDirection(const Vector2& newDirection)
{
    direction = newDirection;
}

// 0x1001e180
void __cdecl Line2D::SetPoint(const Vector2& newPoint)
{
    point = newPoint;
}

// 0x1001e1a0
ClipResult __cdecl Line2D::WhichSide(const Vector2& q) const
{
    const float lhs = q.x * direction.y - q.y * direction.x;
    const float rhs = point.x * direction.y - point.y * direction.x;
    return lhs > rhs ? CLIP_SIDE_ONE : CLIP_SIDE_TWO;
}

// 0x1001e1f0: outside within the tolerance on one side, or on the line.
ClipResult __cdecl Line2D::WhichSide(const Vector2& q, float tolerance) const
{
    const float lhs = q.x * direction.y - q.y * direction.x;
    const float rhs = point.x * direction.y - point.y * direction.x;
    const float diff = lhs - rhs;
    if (diff > tolerance)
        return CLIP_SIDE_ONE;
    if (-diff > tolerance)
        return CLIP_SIDE_TWO;
    return CLIP_ON_LINE;
}

// 0x1001e260: non-parallel lines; NaN counts as intersecting.
bool __cdecl Line2D::Intersects(const Line2D& line) const
{
    const float v = direction.y * line.direction.x - line.direction.y * direction.x;
    return !(std::fabs(v) < 0.0001f);
}

// 0x1001e2a0
Vector2 __cdecl Line2D::Intercept(const Line2D& line) const
{
    const float num = (point.x - line.point.x) * direction.y + (point.y - line.point.y) * (-direction.x);
    const float den = line.direction.y * (-direction.x) + line.direction.x * direction.y;
    const float t = num / den;
    return Vector2{line.direction.x * t + line.point.x, line.point.y + line.direction.y * t};
}

// 0x1001e360
float __cdecl Line2D::GetSeparation(const Vector2& q) const
{
    return static_cast<float>(Line2DVectorSeparationD(*this, q));
}

// 0x1001e3e0: 0 when parallel, else the distance between the two lines.
float __cdecl Line2D::GetSeparation(const Line2D& line) const
{
    const float cross = line.direction.x * direction.y - line.direction.y * direction.x;
    if (!(std::fabs(cross) < 0.0001f))
        return 0.0f;
    const float lx = line.point.x * direction.y - line.point.y * direction.x;
    const float rx = point.x * direction.y - point.y * direction.x;
    const float d = lx - rx;
    return static_cast<float>(std::fabs(static_cast<double>(d)) /
                              std::sqrt(static_cast<double>(direction.x) * direction.x +
                                        static_cast<double>(direction.y) * direction.y));
}

// 0x1001e4a0
float __cdecl Line2D::GetApproxSeparation(const Vector2& q) const
{
    return static_cast<float>(Line2DVectorApproxD(*this, q));
}

// 0x1001e550: 0 when parallel, else the approximate separation of line.point.
float __cdecl Line2D::GetApproxSeparation(const Line2D& line) const
{
    const float cross = direction.y * line.direction.x - line.direction.y * direction.x;
    if (!(std::fabs(cross) < 0.0001f))
        return 0.0f;
    return GetApproxSeparation(line.point);
}

// 0x1001e5a0
float __cdecl Line2D::GetSeparationSquared(const Vector2& q) const
{
    const float lx = q.x * direction.y - q.y * direction.x;
    const float rx = point.x * direction.y - point.y * direction.x;
    const float d = lx - rx;
    return static_cast<float>((static_cast<double>(d) * d) /
                              (static_cast<double>(direction.x) * direction.x +
                               static_cast<double>(direction.y) * direction.y));
}

// 0x1001e610: 0 when parallel.
float __cdecl Line2D::GetSeparationSquared(const Line2D& line) const
{
    const float cross = line.direction.x * direction.y - line.direction.y * direction.x;
    if (!(std::fabs(cross) < 0.0001f))
        return 0.0f;
    const float lx = line.point.x * direction.y - line.point.y * direction.x;
    const float rx = point.x * direction.y - point.y * direction.x;
    const float d = lx - rx;
    return static_cast<float>((static_cast<double>(d) * d) /
                              (static_cast<double>(direction.x) * direction.x +
                               static_cast<double>(direction.y) * direction.y));
}

// ---------------------------------------------------------------------------------------------------------------
// LineSegment2D

LineSegment2D::LineSegment2D()
{
}

// 0x1001e6c0
LineSegment2D::LineSegment2D(const Vector2& a, const Vector2& b) : v0(a), v1(b)
{
}

// 0x1001ec50
LineSegment2D& LineSegment2D::operator=(const LineSegment2D& line)
{
    v0 = line.v0;
    v1 = line.v1;
    return *this;
}

// 0x1000b210
const Vector2& __cdecl LineSegment2D::GetV0() const
{
    return v0;
}

// 0x1001e720
const Vector2& __cdecl LineSegment2D::GetV1() const
{
    return v1;
}

// 0x1001e6f0
void __cdecl LineSegment2D::SetExtents(const Vector2& a, const Vector2& b)
{
    v0 = a;
    v1 = b;
}

// 0x1001e730
ClipResult __cdecl LineSegment2D::WhichSide(const Vector2& q) const
{
    const float dx = v0.x - v1.x;
    const float dy = v0.y - v1.y;
    const float lhs = q.x * dy - q.y * dx;
    const float rhs = v0.x * dy - v0.y * dx;
    return lhs > rhs ? CLIP_SIDE_ONE : CLIP_SIDE_TWO;
}

// 0x1001e790
ClipResult __cdecl LineSegment2D::WhichSide(const Vector2& q, float tolerance) const
{
    const float dx = v0.x - v1.x;
    const float dy = v0.y - v1.y;
    const float lhs = q.x * dy - q.y * dx;
    const float rhs = v0.x * dy - v0.y * dx;
    const float diff = lhs - rhs;
    if (diff > tolerance)
        return CLIP_SIDE_ONE;
    if (-diff > tolerance)
        return CLIP_SIDE_TWO;
    return CLIP_ON_LINE;
}

// 0x1001e810: between the two planes perpendicular to the segment.
bool __cdecl LineSegment2D::IsBeside(const Vector2& q) const
{
    const float dirx = v1.x - v0.x;
    const float diry = v1.y - v0.y;
    const float ax = q.x - v0.x;
    const float ay = q.y - v0.y;
    const float first = ax * dirx + diry * ay;
    if (first < 0.0f)
        return false;
    const float bx = q.x - v1.x;
    const float by = q.y - v1.y;
    const float second = bx * dirx + by * diry;
    if (second > 0.0f)
        return false;
    return true;
}

// 0x1001e8c0: the two corners of this segment on different sides of the line.
bool __cdecl LineSegment2D::Intersects(const Line2D& line) const
{
    const int a = SideCompareDir(v0, line.point, line.direction);
    const int b = SideCompareDir(v1, line.point, line.direction);
    return a != b;
}

// 0x1001e960: either segment crosses the other's supporting line.
bool __cdecl LineSegment2D::Intersects(const LineSegment2D& line) const
{
    const Vector2 d0{v0.x - v1.x, v0.y - v1.y};
    const Vector2 d1{line.v0.x - line.v1.x, line.v0.y - line.v1.y};
    const int a = SideCompareDir(line.v0, v0, d0);
    const int b = SideCompareDir(line.v1, v0, d0);
    if (a == b)
        return false;
    const int c = SideCompareDir(v0, line.v0, d1);
    const int d = SideCompareDir(v1, line.v0, d1);
    return c != d;
}

// 0x1001eaa0
bool __cdecl LineSegment2D::Intercept(const Line2D& line, Vector2& point_) const
{
    const float dx = v0.x - v1.x;
    const float dy = v1.y - v0.y;
    const float den = line.direction.y * dx + line.direction.x * dy;
    if (den == 0.0f)
        return false;
    const float num = dx * (v0.y - line.point.y) + dy * (v0.x - line.point.x);
    point_.x = line.direction.x * (num / den) + line.point.x;
    point_.y = line.point.y + line.direction.y * (num / den);
    return true;
}

// 0x1001eb70
bool __cdecl LineSegment2D::Intercept(const Line2D& line, Vector2& point_, float& t) const
{
    const float dx = v0.x - v1.x;
    const float dy = v1.y - v0.y;
    const float den = line.direction.y * dx + line.direction.x * dy;
    if (den == 0.0f)
        return false;
    const float num = dx * (v0.y - line.point.y) + dy * (v0.x - line.point.x);
    t = num / den;
    point_.x = line.direction.x * t + line.point.x;
    point_.y = line.point.y + line.direction.y * t;
    return true;
}

// 0x1002a9e0
float __cdecl LineSegment2D::GetSeparation(const Vector2& q) const
{
    return static_cast<float>(Segment2DSeparationD(*this, q));
}

// 0x1002ab00: 0 if the line crosses the segment, else the nearer corner's distance.

// 0x1002ac80: 0 when they cross, else the smallest of the four corner-to-segment distances.
float __cdecl LineSegment2D::GetSeparation(const LineSegment2D& line) const
{
    if (Intersects(line))
        return 0.0f;
    float best = 3.402823466e+38f;
    float s = GetSeparation(line.v0);
    if (s < best)
        best = s;
    s = GetSeparation(line.v1);
    if (s < best)
        best = s;
    s = line.GetSeparation(v0);
    if (s < best)
        best = s;
    s = line.GetSeparation(v1);
    if (s < best)
        best = s;
    return best;
}

// 0x1002ad60
float __cdecl LineSegment2D::GetApproxSeparation(const Vector2& q) const
{
    const float dx = v1.x - v0.x;
    const float dy = v1.y - v0.y;
    const float ax = q.x - v0.x;
    const float ay = q.y - v0.y;
    const float dot1 = ay * dy + ax * dx;
    if (dot1 < 0.0f)
        return static_cast<float>(ApproxRootD(ay * ay + ax * ax));
    const float bx = q.x - v1.x;
    const float by = q.y - v1.y;
    const float dot2 = by * dy + bx * dx;
    if (0.0f < dot2)
        return static_cast<float>(ApproxRootD(by * by + bx * bx));
    const float s = dy * dy + dx * dx;
    const float c1 = q.x * dy;
    const float c2 = q.y * dx;
    const float c3 = v0.x * dy;
    const float c4 = v0.y * dx;
    const float cross = std::fabs((c1 - c2) - (c3 - c4));
    const double inv = ApproxInv(s);
    return static_cast<float>(static_cast<double>(cross) /
                              ((inv * (1.5 - ((0.5 * static_cast<double>(s)) * inv) * inv)) *
                               static_cast<double>(s)));
}

// 0x1002af50

// 0x1002b030
float __cdecl LineSegment2D::GetApproxSeparation(const LineSegment2D& line) const
{
    if (Intersects(line))
        return 0.0f;
    float best = 3.402823466e+38f;
    float s = GetApproxSeparation(line.v0);
    if (s < best)
        best = s;
    s = GetApproxSeparation(line.v1);
    if (s < best)
        best = s;
    s = line.GetApproxSeparation(v0);
    if (s < best)
        best = s;
    s = line.GetApproxSeparation(v1);
    if (s < best)
        best = s;
    return best;
}

// 0x1002b110
float __cdecl LineSegment2D::GetSeparationSquared(const Vector2& q) const
{
    const float dx = v1.x - v0.x;
    const float dy = v1.y - v0.y;
    const float ax = q.x - v0.x;
    const float ay = q.y - v0.y;
    const float dot1 = ax * dx + dy * ay;
    if (dot1 < 0.0f)
        return static_cast<float>(static_cast<double>(ax) * ax + static_cast<double>(ay) * ay);
    const float bx = q.x - v1.x;
    const float by = q.y - v1.y;
    const float dot2 = by * dy + bx * dx;
    if (0.0f < dot2)
        return static_cast<float>(static_cast<double>(by) * by + static_cast<double>(bx) * bx);
    const double c1 = static_cast<double>(q.x) * dy;
    const double c2 = static_cast<double>(q.y) * dx;
    const double c3 = static_cast<double>(v0.x) * dy;
    const double c4 = static_cast<double>(v0.y) * dx;
    const double cross = (c1 - c2) - (c3 - c4);
    const double len = static_cast<double>(dy) * dy + static_cast<double>(dx) * dx;
    return static_cast<float>((cross * cross) / len);
}

// 0x1002b250: 0 when the line crosses the segment.
float __cdecl LineSegment2D::GetSeparationSquared(const Line2D& line) const
{
    if (SideCompareDir(v0, line.point, line.direction) != SideCompareDir(v1, line.point, line.direction))
        return 0.0f;
    const float s = line.direction.x * line.direction.x + line.direction.y * line.direction.y;
    const float lx = line.point.x * line.direction.y - line.point.y * line.direction.x;
    const float a = (line.direction.y * v0.x - line.direction.x * v0.y) - lx;
    const float b = (line.direction.y * v1.x - line.direction.x * v1.y) - lx;
    const float da = (a * a) / s;
    const float db = (b * b) / s;
    return da < db ? da : db;
}

// 0x1002b360
float __cdecl LineSegment2D::GetSeparationSquared(const LineSegment2D& line) const
{
    if (Intersects(line))
        return 0.0f;
    float best = 3.402823466e+38f;
    float s = GetSeparationSquared(line.v0);
    if (s < best)
        best = s;
    s = GetSeparationSquared(line.v1);
    if (s < best)
        best = s;
    s = line.GetSeparationSquared(v0);
    if (s < best)
        best = s;
    s = line.GetSeparationSquared(v0);  // the original repeats this call; v1 is never used
    if (s < best)
        best = s;
    return best;
}

// ---------------------------------------------------------------------------------------------------------------
// Circle2D

// 0x1004fd0
Circle2D::Circle2D()
{
}

// 0x1001ec70 (shared with the Vector3 overload, which reads the first two floats)
Circle2D::Circle2D(const Vector2& c, float r) : center(c), radius(r)
{
}

Circle2D::Circle2D(const Vector3& c, float r) : center(Vector2{c.x, c.y}), radius(r)
{
}

// 0x1001f180
Circle2D& Circle2D::operator=(const Circle2D& circle)
{
    center = circle.center;
    radius = circle.radius;
    return *this;
}

// 0x1000b210
const Vector2& __cdecl Circle2D::GetCenter() const
{
    return center;
}

// 0x1001ec90
void __cdecl Circle2D::SetCenter(const Vector2& newCenter)
{
    center = newCenter;
}

// 0x1001ecb0
float __cdecl Circle2D::GetRadius() const
{
    return radius;
}

// 0x1001ecc0
void __cdecl Circle2D::SetRadius(float r)
{
    radius = r;
}

// 0x1001ecd0
bool __cdecl Circle2D::Intersects(const Vector2& q) const
{
    const float dx = q.x - center.x;
    const float dy = q.y - center.y;
    return radius * radius > (dy * dy + dx * dx);
}

// 0x1001ed20: squared distance from the line to the centre against radius squared.
bool __cdecl Circle2D::Intersects(const Line2D& line) const
{
    const float dx = center.x - line.point.x;
    const float dy = center.y - line.point.y;
    const float d3 = line.direction.x;
    const float d4 = line.direction.y;
    const float cross = dx * d4 - dy * d3;
    const float len2 = d3 * d3 + d4 * d4;
    return radius * radius > (cross / len2) * cross;
}

// 0x1001edb0
bool __cdecl Circle2D::Intersects(const Circle2D& circle) const
{
    const float dx = circle.center.x - center.x;
    const float dy = circle.center.y - center.y;
    const float sum = circle.radius + radius;
    return sum * sum > (dy * dy + dx * dx);
}

// 0x1001f1a0
bool __cdecl Circle2D::Intersects(const LineSegment2D& line) const
{
    const float dx = line.v1.x - line.v0.x;
    const float dy = line.v1.y - line.v0.y;
    const float ax = center.x - line.v0.x;
    const float ay = center.y - line.v0.y;
    const float dot = ax * dx + dy * ay;
    if (dot < 0.0f)
        return (ax * ax + ay * ay) < radius * radius;
    const float bx = center.x - line.v1.x;
    const float by = center.y - line.v1.y;
    const float dot2 = bx * dx + by * dy;
    if (0.0f < dot2)
        return (bx * bx + by * by) < radius * radius;
    const float cross = ax * dy - ay * dx;
    return (cross * cross) / (dx * dx + dy * dy) < radius * radius;
}

// 0x1001ee00
float __cdecl Circle2D::GetSeparation(const Vector2& q) const
{
    return X87CircleSeparationV(this, &q);
}

// 0x1001ee30
float __cdecl Circle2D::GetSeparation(const Line2D& line) const
{
    const float dx = line.direction.x;
    const float dy = line.direction.y;
    const float cross = (center.x - line.point.x) * dy - (center.y - line.point.y) * dx;
    return static_cast<float>(static_cast<double>(cross) /
                                  std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy) -
                              radius);
}

// 0x1001ee90
float __cdecl Circle2D::GetSeparation(const Circle2D& circle) const
{
    return X87CircleSeparationC(this, &circle);
}

// 0x1001f2d0
float __cdecl Circle2D::GetSeparation(const LineSegment2D& line) const
{
    const float dx = line.v1.x - line.v0.x;
    const float dy = line.v1.y - line.v0.y;
    const float ax = center.x - line.v0.x;
    const float ay = center.y - line.v0.y;
    const float dot1 = ay * dy + ax * dx;
    if (dot1 < 0.0f)
        return static_cast<float>(std::sqrt(static_cast<double>(ay) * ay + static_cast<double>(ax) * ax) - radius);
    const float bx = center.x - line.v1.x;
    const float by = center.y - line.v1.y;
    const float dot2 = by * dy + bx * dx;
    if (!(dot2 <= 0.0f))
        return static_cast<float>(std::sqrt(static_cast<double>(by) * by + static_cast<double>(bx) * bx) - radius);
    const float cross = (center.x - line.v0.x) * dy - (center.y - line.v0.y) * dx;
    return static_cast<float>(static_cast<double>(cross) /
                                  std::sqrt(static_cast<double>(dy) * dy + static_cast<double>(dx) * dx) -
                              radius);
}

// 0x1001eed0
float __cdecl Circle2D::GetApproxSeparation(const Vector2& q) const
{
    const float dx = q.x - center.x;
    const float dy = q.y - center.y;
    return static_cast<float>(CircleApproxD(dx * dx + dy * dy, radius));
}

// 0x1001ef50
float __cdecl Circle2D::GetApproxSeparation(const Line2D& line) const
{
    const float s = line.direction.x * line.direction.x + line.direction.y * line.direction.y;
    const float cross = (center.x - line.point.x) * line.direction.y - (center.y - line.point.y) * line.direction.x;
    const double inv = ApproxInv(s);
    const double root = (inv * (1.5 - ((0.5 * static_cast<double>(s)) * inv) * inv)) * static_cast<double>(s);
    return static_cast<float>(static_cast<double>(cross) / root - radius);
}

// 0x1001eff0
float __cdecl Circle2D::GetApproxSeparation(const Circle2D& circle) const
{
    const float dx = circle.center.x - center.x;
    const float dy = circle.center.y - center.y;
    return static_cast<float>((ApproxRootD(dy * dy + dx * dx) - radius) - circle.radius);
}

// 0x1001f3f0
float __cdecl Circle2D::GetApproxSeparation(const LineSegment2D& line) const
{
    const float dx = line.v1.x - line.v0.x;
    const float dy = line.v1.y - line.v0.y;
    const float ax = center.x - line.v0.x;
    const float ay = center.y - line.v0.y;
    const float dot1 = ay * dy + ax * dx;
    if (dot1 < 0.0f)
        return static_cast<float>(ApproxRootD(ay * ay + ax * ax) - radius);
    const float bx = center.x - line.v1.x;
    const float by = center.y - line.v1.y;
    const float dot2 = by * dy + bx * dx;
    if (dot2 > 0.0f)
        return static_cast<float>(ApproxRootD(by * by + bx * bx) - radius);
    const float cross = (center.x - line.v0.x) * dy - (center.y - line.v0.y) * dx;
    const float s = dy * dy + dx * dx;
    const double inv = ApproxInv(s);
    const double root = (inv * (1.5 - ((0.5 * static_cast<double>(s)) * inv) * inv)) * static_cast<double>(s);
    return static_cast<float>(static_cast<double>(cross) / root - radius);
}

// 0x1001f080: the whole expression in SSE float, the square in x87.
float __cdecl Circle2D::GetSeparationSquared(const Vector2& q) const
{
    const float dx = q.x - center.x;
    const float dy = q.y - center.y;
    const float s = dx * dx + dy * dy;
    const float inv = FromBits(0x5f3759dfu - (ToBits(s) >> 1));
    float v = 1.5f - ((s * 0.5f) * inv) * inv;
    v = v * inv;
    v = v * s;
    v = v - radius;
    return static_cast<float>(static_cast<double>(v) * v);
}

// 0x1001f120 (approximate separation, squared without an intermediate rounding)
float __cdecl Circle2D::GetSeparationSquared(const Line2D& line) const
{
    const float dx = line.direction.x;
    const float dy = line.direction.y;
    const float s = dx * dx + dy * dy;
    const double inv = ApproxInv(s);
    const float cross = (center.x - line.point.x) * dy - (center.y - line.point.y) * dx;
    const double root = (inv * (1.5 - ((0.5 * static_cast<double>(s)) * inv) * inv)) * static_cast<double>(s);
    const double v = static_cast<double>(cross) / root - radius;
    return static_cast<float>(v * v);
}

// 0x1001f160
float __cdecl Circle2D::GetSeparationSquared(const Circle2D& circle) const
{
    const float dx = circle.center.x - center.x;
    const float dy = circle.center.y - center.y;
    const double v = (ApproxRootD(dy * dy + dx * dx) - radius) - circle.radius;
    return static_cast<float>(v * v);
}

// 0x1001f140
float __cdecl Circle2D::GetSeparationSquared(const LineSegment2D& line) const
{
    const float dx = line.v1.x - line.v0.x;
    const float dy = line.v1.y - line.v0.y;
    const float ax = center.x - line.v0.x;
    const float ay = center.y - line.v0.y;
    const float dot1 = ay * dy + ax * dx;
    if (dot1 < 0.0f) {
        const double v = ApproxRootD(ay * ay + ax * ax) - radius;
        return static_cast<float>(v * v);
    }
    const float bx = center.x - line.v1.x;
    const float by = center.y - line.v1.y;
    const float dot2 = by * dy + bx * dx;
    if (dot2 > 0.0f) {
        const double v = ApproxRootD(by * by + bx * bx) - radius;
        return static_cast<float>(v * v);
    }
    const float cross = (center.x - line.v0.x) * dy - (center.y - line.v0.y) * dx;
    const float s = dy * dy + dx * dx;
    const double inv = ApproxInv(s);
    const double root = (inv * (1.5 - ((0.5 * static_cast<double>(s)) * inv) * inv)) * static_cast<double>(s);
    const double v = static_cast<double>(cross) / root - radius;
    return static_cast<float>(v * v);
}

}  // namespace Jet
