// Render3DCore_7c unit U1c (notes/agents/Render3DCore_workstreams.md): the oriented box. Layout and behaviour follow
// the TS12 assembly (addresses per member in Render3DCore_7c/OrientedBox.cpp). 0x3c bytes: centre +0, three axes
// +0xc..+0x2c (each a Vector3), extents +0x30. No vftable. Ordinary members of this module are __cdecl (this is on
// the stack, as the original's callers do); constructors and the assignment are __thiscall, CalcLocalDir/Pos are
// __thiscall as their mangled names say (QBEX). Methods that need classes from other streams read their fields only
// and do not call into them.
#pragma once

#include <jet/Render3DCore.h>

namespace Jet {

class OrientedBox {
public:
    OrientedBox();
    OrientedBox(unsigned int count, const Vector3* points);
    OrientedBox(unsigned int count, VertexItemPtr<const Vector3, VIPConstHelper> items);
    OrientedBox(const OrientedBox& o);
    OrientedBox(const AxisAlignedBox& box);
    OrientedBox& operator=(const OrientedBox& o);

    void __cdecl SetCenter(const Vector3& center);
    const Vector3& __cdecl GetCenter() const;
    void __cdecl SetExtents(const Vector3& extents);
    const Vector3& __cdecl GetExtents() const;
    const Vector3* __cdecl GetAxis() const;
    void __cdecl SetAxis(const Vector3* axes);
    void __cdecl SetAxis(const Vector3& x, const Vector3& y, const Vector3& z);
    void __cdecl Zero();
    bool __cdecl IsEmpty() const;

    void __cdecl Transform(const Matrix3& m, const Vector3& t);
    void __cdecl Transform(const Matrix4& m);
    void __cdecl InverseTransform(const Matrix3& m, const Vector3& t);
    void __thiscall CalcLocalDir(const Vector3& dir, Vector3& out) const;
    void __thiscall CalcLocalPos(const Vector3& pos, Vector3& out) const;
    void __cdecl CalcCorners(Vector3* corners) const;

    void __cdecl Maximize(const OrientedBox& box);
    void __cdecl Maximize(const AxisAlignedBox& box);
    void __cdecl MakeBestFit(unsigned int count, const Vector3* points);
    void __cdecl MakeBestFit(unsigned int count, VertexItemPtr<const Vector3, VIPConstHelper> items);

    bool __cdecl TestCollision(const OrientedBox& box) const;
    bool __cdecl TestCollision(const AxisAlignedBox& box) const;
    bool __cdecl TestCollision(const Sphere& sphere) const;
    bool __cdecl TestCollision(const Line& line) const;
    bool __cdecl TestCollision(const Ray& ray) const;
    bool __cdecl TestCollision(const LineSegment& segment) const;
    bool __cdecl TestCollision(const Cylinder& cylinder) const;
    bool __cdecl TestCollision(const Ellipsoid& ellipsoid) const;
    bool __cdecl GetMinMaxCollision(const Vector3& point, const Vector3& dir, float& tMin, float& tMax) const;

protected:
    void __cdecl Init(int count, VertexItemPtr<const Vector3, VIPConstHelper> items);
    void __cdecl MinimalBoxForAngles(int count, VertexItemPtr<const Vector3, VIPConstHelper> items,
                                     float* const angles);
    static float __cdecl Volume(const float* angles, unsigned int count,
                                VertexItemPtr<const Vector3, VIPConstHelper> items);

public:
    Vector3 center;   // +0x00
    Vector3 axis[3];  // +0x0c
    Vector3 extents;  // +0x30

private:
    friend struct OrientedBoxAccess;
};
JET_LAYOUT32(sizeof(OrientedBox) == 0x3c);

// Other units of this module build a box from a vertex view through the protected Init.
struct OrientedBoxAccess
{
    static void Init(OrientedBox& box, int count, VertexItemPtr<const Vector3, VIPConstHelper> items)
    {
        box.Init(count, items);
    }
};

}  // namespace Jet
