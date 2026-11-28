// Unit U1a of Render3DCore_7c: Jet::AxisAlignedBox, transcribed from the TS12 assembly.
//
// Ordinary members of this module are __cdecl (the this pointer is pushed on the stack, as the original's callers
// do); constructors and the compiler-generated assignment are __thiscall. The comparison order is kept because NaN
// operands must pick the same side as the original: the assembly compares with COMISS/JBE, so a NaN keeps the left
// operand. The box is two Vector3 corners; nothing here calls another module.
#include <jet/Render3DCore.h>

#include <cstddef>

namespace Jet {

namespace {

// Sphere is polymorphic (virtual Maximize); only the fields are read here. TS12 layout: vftable +0, centre +4,
// radius +0x10, radiusXY +0x14.
struct SphereFields {
    void* vftable;  // +0x00
    Vector3 center; // +0x04
    float radius;   // +0x10
    float radiusXY; // +0x14
};
static_assert(offsetof(SphereFields, center) == 0x4, "Sphere centre");
static_assert(offsetof(SphereFields, radius) == 0x10, "Sphere radius");
static_assert(sizeof(SphereFields) == 0x18, "Sphere size");

// Plane has six instances in a frustum array; the box only reads normal and d.
struct PlaneFields {
    Vector3 normal;  // +0x00
    float d;         // +0x0c
};
static_assert(sizeof(PlaneFields) == 0x10, "Plane size");

}  // namespace

// 0x10004fd0: the default constructor leaves every corner as whatever the storage held (the original is `ret` only).
AxisAlignedBox::AxisAlignedBox()
{
}

// 0x10003170
AxisAlignedBox::AxisAlignedBox(const Vector3& min_, const Vector3& max_) : min(min_), max(max_)
{
}

// 0x100031a0: a contiguous array of Vector3 (stride 0xc).
AxisAlignedBox::AxisAlignedBox(unsigned int count, const Vector3* points)
{
    CalcBoxLimits(count, VertexItemPtr<const Vector3, VIPConstHelper>{0xc, points});
}

// 0x100031d0: a strided view of Vector3.
AxisAlignedBox::AxisAlignedBox(unsigned int count, VertexItemPtr<const Vector3, VIPConstHelper> items)
{
    CalcBoxLimits(count, items);
}

// 0x10003200 (also the copy assignment's body)
AxisAlignedBox::AxisAlignedBox(const AxisAlignedBox& o) : min(o.min), max(o.max)
{
}

AxisAlignedBox& AxisAlignedBox::operator=(const AxisAlignedBox& o)
{
    min = o.min;
    max = o.max;
    return *this;
}

// 0x10003480 / 0x100034a0
void __cdecl AxisAlignedBox::SetMin(const Vector3& v)
{
    min = v;
}

void __cdecl AxisAlignedBox::SetMax(const Vector3& v)
{
    max = v;
}

// 0x1000b210 / 0x10003760 (identical bodies shared with other classes)
const Vector3& __cdecl AxisAlignedBox::GetMin() const
{
    return min;
}

const Vector3& __cdecl AxisAlignedBox::GetMax() const
{
    return max;
}

// 0x100034c0
void __cdecl AxisAlignedBox::Zero()
{
    min = Vector3{0.0f, 0.0f, 0.0f};
    max = Vector3{0.0f, 0.0f, 0.0f};
}

// 0x100034f0: exact equality; NaN compares unordered, so it is not empty (the original's UCOMISS/JP).
bool __cdecl AxisAlignedBox::IsEmpty() const
{
    return min.x == max.x && min.y == max.y && min.z == max.z;
}

// 0x10003230: COMISS(this, other)/JBE keep this, i.e. other < this wins; a NaN keeps this.
void __cdecl AxisAlignedBox::Maximize(const AxisAlignedBox& box)
{
    min.x = box.min.x < min.x ? box.min.x : min.x;
    min.y = box.min.y < min.y ? box.min.y : min.y;
    min.z = box.min.z < min.z ? box.min.z : min.z;
    max.x = max.x < box.max.x ? box.max.x : max.x;
    max.y = max.y < box.max.y ? box.max.y : max.y;
    max.z = max.z < box.max.z ? box.max.z : max.z;
}

// 0x10003530
void __cdecl AxisAlignedBox::Minimize(const AxisAlignedBox& box)
{
    min.x = min.x < box.min.x ? box.min.x : min.x;
    min.y = min.y < box.min.y ? box.min.y : min.y;
    min.z = min.z < box.min.z ? box.min.z : min.z;
    max.x = box.max.x < max.x ? box.max.x : max.x;
    max.y = box.max.y < max.y ? box.max.y : max.y;
    max.z = box.max.z < max.z ? box.max.z : max.z;
}

// 0x100033d0: the original adds max + min (in that operand order) and multiplies by 0.5f.
Vector3 __cdecl AxisAlignedBox::GetCenter() const
{
    return Vector3{(max.x + min.x) * 0.5f, (max.y + min.y) * 0.5f, (max.z + min.z) * 0.5f};
}

// 0x1000dbb0: eight corners, bottom face in min-z order, then the max-z face.
void __cdecl AxisAlignedBox::CalcCorners(Vector3* corners) const
{
    corners[0] = Vector3{min.x, min.y, min.z};
    corners[1] = Vector3{max.x, min.y, min.z};
    corners[2] = Vector3{max.x, max.y, min.z};
    corners[3] = Vector3{min.x, max.y, min.z};
    corners[4] = Vector3{min.x, min.y, max.z};
    corners[5] = Vector3{max.x, min.y, max.z};
    corners[6] = Vector3{max.x, max.y, max.z};
    corners[7] = Vector3{min.x, max.y, max.z};
}

// 0x1000e240: clamp each component to [min, max]; a NaN stays NaN (both comparisons are false).
Vector3 __cdecl AxisAlignedBox::Clamp(const Vector3& point) const
{
    float x = point.x > min.x ? point.x : min.x;
    if (x > max.x)
        x = max.x;
    float y = point.y > min.y ? point.y : min.y;
    if (y > max.y)
        y = max.y;
    float z = point.z > min.z ? point.z : min.z;
    if (z > max.z)
        z = max.z;
    return Vector3{x, y, z};
}

// 0x100032d0: inside when no component is strictly outside; equal bounds are inside, NaN is inside (all six COMISS
// tests compare with > only).
bool __cdecl AxisAlignedBox::IsWithin(const Vector3& point) const
{
    return !(min.x > point.x || min.y > point.y || min.z > point.z || point.x > max.x || point.y > max.y ||
             point.z > max.z);
}

// 0x10003370 (also the body of TestCollision): strict overlap on every axis; touching faces do not collide.
bool __cdecl AxisAlignedBox::IsWithin(const AxisAlignedBox& box) const
{
    return max.x > box.min.x && max.y > box.min.y && max.z > box.min.z && box.max.x > min.x &&
           box.max.y > min.y && box.max.z > min.z;
}

// 0x1000e2c0: for each of six planes, the corner farthest along the normal must not be behind it.
bool __cdecl AxisAlignedBox::IsWithin(const Plane* const planes) const
{
    const PlaneFields* base = reinterpret_cast<const PlaneFields*>(planes);
    for (int i = 0; i < 6; ++i) {
        const PlaneFields* plane = base + i;
        const float x = plane->normal.x < 0.0f ? min.x : max.x;
        const float y = plane->normal.y < 0.0f ? min.y : max.y;
        const float z = plane->normal.z < 0.0f ? min.z : max.z;
        const float dot = plane->normal.x * x + plane->normal.y * y + plane->normal.z * z;
        if (dot < plane->d)
            return false;
    }
    return true;
}

// 0x10003370
bool __cdecl AxisAlignedBox::TestCollision(const AxisAlignedBox& box) const
{
    return IsWithin(box);
}

// 0x1000dda0: squared distance from the sphere centre to the nearest point of the box, against radius squared.
bool __cdecl AxisAlignedBox::TestCollision(const Sphere& sphere) const
{
    const SphereFields* s = reinterpret_cast<const SphereFields*>(&sphere);
    float dx = 0.0f;
    if (min.x > s->center.x)
        dx = s->center.x - min.x;
    else if (s->center.x > max.x)
        dx = s->center.x - max.x;
    float dy = 0.0f;
    if (min.y > s->center.y)
        dy = s->center.y - min.y;
    else if (s->center.y > max.y)
        dy = s->center.y - max.y;
    float dz = 0.0f;
    if (min.z > s->center.z)
        dz = s->center.z - min.z;
    else if (s->center.z > max.z)
        dz = s->center.z - max.z;
    const float distance2 = dx * dx + dy * dy + dz * dz;
    return s->radius * s->radius >= distance2;
}

// 0x100035d0 / 0x10003640: component-wise equality, NaN makes both false/true as in the original's UCOMISS/JP.
bool __cdecl AxisAlignedBox::operator==(const AxisAlignedBox& box) const
{
    return min.x == box.min.x && min.y == box.min.y && min.z == box.min.z && max.x == box.max.x &&
           max.y == box.max.y && max.z == box.max.z;
}

bool __cdecl AxisAlignedBox::operator!=(const AxisAlignedBox& box) const
{
    return !(*this == box);
}

// 0x1000dc90: first point seeds both corners, then each stride step lowers min and raises max. The comparison order
// is the original's (a NaN keeps the accumulator), and an empty array zeroes the box.
void __cdecl AxisAlignedBox::CalcBoxLimits(unsigned int count, VertexItemPtr<const Vector3, VIPConstHelper> items)
{
    if (count == 0) {
        min = Vector3{0.0f, 0.0f, 0.0f};
        max = Vector3{0.0f, 0.0f, 0.0f};
        return;
    }
    min = *items.data;
    max = *items.data;
    const char* p = reinterpret_cast<const char*>(items.data) + items.stride;
    for (unsigned int i = 1; i < count; ++i, p += items.stride) {
        const Vector3& v = *reinterpret_cast<const Vector3*>(p);
        min.x = v.x < min.x ? v.x : min.x;
        min.y = v.y < min.y ? v.y : min.y;
        min.z = v.z < min.z ? v.z : min.z;
        max.x = max.x < v.x ? v.x : max.x;
        max.y = max.y < v.y ? v.y : max.y;
        max.z = max.z < v.z ? v.z : max.z;
    }
}

}  // namespace Jet
