// Render3DCore_7c unit U1a (notes/Render3DCore.md): the axis-aligned box. Layout and behaviour follow the TS12
// assembly (addresses per member in Render3DCore_7c/AxisAlignedBox.cpp). The class has no vftable; an ordinary member
// function of this module is __cdecl (this is pushed on the stack, as in the original), while constructors and the
// compiler-generated assignment use __thiscall. Methods that need classes still to be reconstructed are not declared
// here yet; they keep forwarding to Render3DCore_7c_orig.dll through the generated .def.
#pragma once

#include <jet/Math.h>
#include <jet/RenderTypes.h>

namespace Jet {

class OrientedBox;
class Cylinder;
class Ellipsoid;
class Line;
class Ray;
class LineSegment;
class Plane;
class Sphere;

// 0x18 bytes: the two corners. min is compared with <= in the assembly (NaN keeps the left operand).
class AxisAlignedBox {
public:
    AxisAlignedBox();
    AxisAlignedBox(const Vector3& min, const Vector3& max);
    AxisAlignedBox(unsigned int count, const Vector3* points);
    AxisAlignedBox(unsigned int count, VertexItemPtr<const Vector3, VIPConstHelper> items);
    AxisAlignedBox(const AxisAlignedBox& o);
    AxisAlignedBox(const OrientedBox& box);  // U1c: center +/- the world-axis extents
    AxisAlignedBox& operator=(const AxisAlignedBox& o);

    void __cdecl SetMin(const Vector3& v);
    void __cdecl SetMax(const Vector3& v);
    const Vector3& __cdecl GetMin() const;
    const Vector3& __cdecl GetMax() const;
    void __cdecl Zero();
    bool __cdecl IsEmpty() const;
    void __cdecl Maximize(const AxisAlignedBox& box);
    void __cdecl Maximize(const OrientedBox& box);
    void __cdecl Minimize(const AxisAlignedBox& box);
    Vector3 __cdecl GetCenter() const;
    void __cdecl CalcCorners(Vector3* corners) const;
    Vector3 __cdecl Clamp(const Vector3& point) const;

    bool __cdecl IsWithin(const Vector3& point) const;
    bool __cdecl IsWithin(const AxisAlignedBox& box) const;
    bool __cdecl IsWithin(const Plane* const planes) const;  // six frustum planes, each {normal, d}
    bool __cdecl TestCollision(const AxisAlignedBox& box) const;
    bool __cdecl TestCollision(const Sphere& sphere) const;
    bool __cdecl TestCollision(const Line& line) const;
    bool __cdecl TestCollision(const Ray& ray) const;
    bool __cdecl TestCollision(const LineSegment& segment) const;
    bool __cdecl TestCollision(const OrientedBox& box) const;
    bool __cdecl TestCollision(const Cylinder& cylinder) const;
    bool __cdecl TestCollision(const Ellipsoid& ellipsoid) const;
    bool __cdecl GetMinMaxCollision(const Vector3& point, const Vector3& dir, float& tMin, float& tMax) const;

    bool __cdecl operator==(const AxisAlignedBox& box) const;
    bool __cdecl operator!=(const AxisAlignedBox& box) const;

protected:
    void __cdecl CalcBoxLimits(unsigned int count, VertexItemPtr<const Vector3, VIPConstHelper> items);

public:
    Vector3 min;  // +0x00
    Vector3 max;  // +0x0c
};
JET_LAYOUT32(sizeof(AxisAlignedBox) == 0x18);

}  // namespace Jet
