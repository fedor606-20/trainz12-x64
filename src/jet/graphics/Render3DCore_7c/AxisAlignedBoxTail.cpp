// Unit U1c of Render3DCore_7c: the AxisAlignedBox methods that need the oriented box or the line/cylinder/ellipsoid
// types (U1a's AxisAlignedBox.cpp keeps the 25 box-only exports). Transcribed from the TS12 assembly; addresses per
// member. Ordinary members are __cdecl (as the original's callers push this); the constructor from an oriented box is
// __thiscall. 0x10004fd0 is U1a's default constructor (identical body shared by several classes).
#include <jet/Render3DCore.h>
#include <jet/Render3DCoreOrientedBox.h>

#include <cfloat>
#include <cmath>

namespace Jet {

namespace {

// Line-family objects are polymorphic in TS12 (a vftable at +0), so the fields start at +4: Ray and Line both have
// direction +4 and point +0x10; LineSegment adds endPoint +0x1c. Only the fields are read here.
struct LineFields {
    void* vftable;   // +0x00
    Vector3 dir;     // +0x04
    Vector3 point;   // +0x10
};
struct LineSegmentFields {
    void* vftable;   // +0x00
    Vector3 dir;     // +0x04
    Vector3 point;   // +0x10
    Vector3 end;     // +0x1c
};
static_assert(sizeof(LineFields) == 0x1c, "Line");
static_assert(sizeof(LineSegmentFields) == 0x28, "LineSegment");

}  // namespace

// 0x1000d9f0: world-axis extents are |axis.x|*extent.x + |axis.y|*extent.y + |axis.z|*extent.z per world axis,
// accumulated on the x87 stack (double intermediate here) and rounded once to float; min = center - e, max = center + e.
AxisAlignedBox::AxisAlignedBox(const OrientedBox& box)
{
    const double ex = box.extents.x, ey = box.extents.y, ez = box.extents.z;
    const double ax = box.axis[0].x, ay = box.axis[0].y, az = box.axis[0].z;
    const double bx = box.axis[1].x, by = box.axis[1].y, bz = box.axis[1].z;
    const double cx = box.axis[2].x, cy = box.axis[2].y, cz = box.axis[2].z;
    const float wx = static_cast<float>((fabs(bx) * ey + fabs(cx) * ez) + fabs(ax) * ex);
    const float wy = static_cast<float>((fabs(ay) * ex + fabs(by) * ey) + fabs(cy) * ez);
    const float wz = static_cast<float>((fabs(az) * ex + fabs(bz) * ey) + fabs(cz) * ez);
    min = Vector3{box.center.x - wx, box.center.y - wy, box.center.z - wz};
    max = Vector3{box.center.x + wx, box.center.y + wy, box.center.z + wz};
}

// 0x1000db00: build the box of the oriented box, then merge it into this one (the same COMISS/JBE order as U1a's
// Maximize(AxisAlignedBox): a NaN keeps this side).
void __cdecl AxisAlignedBox::Maximize(const OrientedBox& box)
{
    const AxisAlignedBox tmp(box);
    min.x = tmp.min.x < min.x ? tmp.min.x : min.x;
    min.y = tmp.min.y < min.y ? tmp.min.y : min.y;
    min.z = tmp.min.z < min.z ? tmp.min.z : min.z;
    max.x = max.x < tmp.max.x ? tmp.max.x : max.x;
    max.y = max.y < tmp.max.y ? tmp.max.y : max.y;
    max.z = max.z < tmp.max.z ? tmp.max.z : max.z;
}

// 0x1000dfc0: slab test between the box and the ray p + t*d. Along each axis a zero direction needs p between the
// slabs (0 < p-min and 0 < p-max must agree); otherwise the two plane crossings are ordered into near/far and merged
// with tMin = max, tMax = min. The comparisons keep the assembly's operand order, so NaN behaves as it does there.
bool __cdecl AxisAlignedBox::GetMinMaxCollision(const Vector3& point, const Vector3& dir, float& tMin,
                                               float& tMax) const
{
    const float px0 = min.x - point.x, px1 = max.x - point.x;
    const float py0 = min.y - point.y, py1 = max.y - point.y;
    const float pz0 = min.z - point.z, pz1 = max.z - point.z;

    if (dir.x == 0.0f) {
        if ((0.0f < px0) == (0.0f < px1)) return false;
        tMin = -FLT_MAX;
        tMax = FLT_MAX;
    } else {
        const float inv = 1.0f / dir.x;
        const float ta = inv * px0;
        const float tb = inv * px1;
        if (!(tb > ta)) {
            tMin = tb;
            tMax = ta;
        } else {
            tMin = ta;
            tMax = tb;
        }
    }

    if (dir.y == 0.0f) {
        if ((0.0f < py0) == (0.0f < py1)) return false;
    } else {
        const float inv = 1.0f / dir.y;
        const float ta = inv * py0;
        const float tb = inv * py1;
        float near, far;
        if (!(tb > ta)) {
            near = tb;
            far = ta;
        } else {
            near = ta;
            far = tb;
        }
        tMin = tMin > near ? tMin : near;
        tMax = far > tMax ? tMax : far;
    }

    if (dir.z == 0.0f) {
        if ((0.0f < pz0) == (0.0f < pz1)) return false;
    } else {
        const float inv = 1.0f / dir.z;
        const float ta = inv * pz0;
        const float tb = inv * pz1;
        float near, far;
        if (!(tb > ta)) {
            near = tb;
            far = ta;
        } else {
            near = ta;
            far = tb;
        }
        tMin = tMin > near ? tMin : near;
        tMax = far > tMax ? tMax : far;
    }

    if (tMin > tMax) return false;
    if (tMin == -FLT_MAX && tMax == FLT_MAX) {
        tMin = 0.0f;
        tMax = 0.0f;
    }
    return true;
}

// 0x1000de90: a line is infinite, so the raw slab result is the answer.
bool __cdecl AxisAlignedBox::TestCollision(const Line& line) const
{
    const LineFields* l = reinterpret_cast<const LineFields*>(&line);
    float tMin, tMax;
    return GetMinMaxCollision(l->point, l->dir, tMin, tMax);
}

// 0x1000dec0: a ray needs its entry point at t >= 0 (0 <= tMin, NaN passes as in the original's COMISS/JA).
bool __cdecl AxisAlignedBox::TestCollision(const Ray& ray) const
{
    const LineFields* l = reinterpret_cast<const LineFields*>(&ray);
    float tMin, tMax;
    if (!GetMinMaxCollision(l->point, l->dir, tMin, tMax)) return false;
    return !(0.0f > tMax);
}

// 0x1000df00: clamp the entry to the segment start (t >= 0), then the segment's own length (the direction dot
// end-point) into the far end; hit when the entry is still not past it.
bool __cdecl AxisAlignedBox::TestCollision(const LineSegment& segment) const
{
    const LineSegmentFields* s = reinterpret_cast<const LineSegmentFields*>(&segment);
    float tMin, tMax;
    if (!GetMinMaxCollision(s->point, s->dir, tMin, tMax)) return false;
    if (tMin < 0.0f) tMin = 0.0f;
    const float length = s->dir.z * (s->end.z - s->point.z) + s->dir.y * (s->end.y - s->point.y) +
                         s->dir.x * (s->end.x - s->point.x);
    if (length < tMax) tMax = length;
    return !(tMin > tMax);
}

// 0x1000de60: the box of the oriented box against the oriented box (builds the OBB from this box, as the original).
bool __cdecl AxisAlignedBox::TestCollision(const OrientedBox& box) const
{
    const OrientedBox mine(*this);
    return mine.TestCollision(box);
}

// 0x1004f940 (shared with several classes): no implementation in TS12, answers false.
bool __cdecl AxisAlignedBox::TestCollision(const Cylinder&) const
{
    return false;
}

// 0x1004f940
bool __cdecl AxisAlignedBox::TestCollision(const Ellipsoid&) const
{
    return false;
}

}  // namespace Jet
