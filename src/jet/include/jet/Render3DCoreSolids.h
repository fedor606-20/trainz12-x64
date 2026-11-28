// Render3DCore_7c workstream U1d2: Polygon, Sphere, Ellipsoid, Ellipse, Cylinder, Cone and Circle. Layouts and
// calling conventions follow the TS12 assembly (addresses per member in graphics/Render3DCore_7c/*.cpp). The RC1
// SDK headers are a naming reference only: TS12 moved fields (Sphere's radius is at +0x10 and there is no
// radiusXY; Ellipsoid derives Plane at +0x04, etc.).
//
// Ordinary members are __cdecl (this is pushed on the stack, mangled QAA/UAA); only constructors and the
// compiler-generated assignments are __thiscall (QAE). MSVC emits same-name virtual overloads in REVERSE
// declaration order, so the TestCollision lists below are written backwards to produce the original's slot order.
#pragma once

#include <jet/Math.h>
#include <jet/Render3DCore.h>

namespace Jet {

// Internal helpers shared by the solids units. They reproduce the original's unnamed float helpers: the
// table-driven magnitude/normalise (0x10008ab0 / 0x10008b50, which read StdMath::s_sqrtLUT from
// KernelStandard_7c) and the x87 float copy that quiets a signalling NaN as the original's fld/fstp does.
namespace SolidsDetail {
double __cdecl Length(const Vector3& v);      // 0x10008ab0: |v| as the x87 value the original returns
void __cdecl Normalize(Vector3& v);           // 0x10008b50: v /= |v| through the same seed/Newton steps
void __cdecl X87Copy(float& dst, const float& src);
float __cdecl FromBits(unsigned int bits);
float __cdecl MulSS(float a, float b);        // scalar SSE with a fixed operand order (NaN payload parity)
float __cdecl AddSS(float a, float b);
float __cdecl SubSS(float a, float b);
float __cdecl DivSS(float a, float b);
float __cdecl SqrtF(float a);                 // x87 fsqrt + fstp float, as the original's float square roots
void* __cdecl OrigProc(const char* mangled);  // an export of Render3DCore_7c_orig.dll, or null
}  // namespace SolidsDetail

class Line;
class LineSegment;
class Ray;
class Plane;
class Polygon2D;
class Circle;

// POLYGON (0x1c bytes): vftable +0x00, normal +0x04, d +0x10, numVerts +0x14, verts +0x18.
class Polygon {
public:
    Polygon();
    Polygon(const Polygon& poly);
    Polygon(const Polygon2D& poly);
    Polygon(int numVerts, const Vector3* verts);
    virtual ~Polygon();
    Polygon& __cdecl operator=(const Polygon& other);
    Polygon& __cdecl operator=(const Polygon2D& other);

    int __cdecl GetNumVerts() const;
    bool __cdecl Intersect(const Line& line, Vector3& intersection) const;
    void __cdecl Reset();
    void __cdecl Set(int n);
    void __cdecl Set(int n, const Vector3* vList);
    void __cdecl Set(int n, const Vector2* vList);
    const Vector3& __cdecl GetVertex(int i) const;
    Vector3 __cdecl GetVertex(int i);
    void __cdecl GetVertex(Vector3& out, int i) const;
    void __cdecl GetVertex(Vector2& out, int i) const;
    const Vector3* __cdecl GetVertexArray() const;
    void __cdecl SetVertex(const Vector3& v, int i);

    bool __cdecl IsInsideXY(const Vector3& point) const;
    bool __cdecl IsInsideYZ(const Vector3& point) const;
    bool __cdecl IsInsideZX(const Vector3& point) const;
    bool __cdecl IsInsideXYNegative(const Vector3& point) const;
    bool __cdecl IsInsideYZNegative(const Vector3& point) const;
    bool __cdecl IsInsideZXNegative(const Vector3& point) const;
    bool __cdecl IsInside(const Vector3& point) const;
    bool __cdecl IsInsideXY(const Vector3& point, float& distSq) const;

    bool __cdecl CalculateCenter(Vector3& center) const;
    bool __cdecl CalculateBoundingSphere(Sphere& sphere) const;
    void __cdecl Flip();
    void __cdecl RemoveDuplicates();
    void __cdecl ReduceWithTolerance(float tolerance);
    void __cdecl Offset(const Vector3& offset);

    Vector3 normal;  // +0x04
    float d;         // +0x10
    int numVerts;    // +0x14
    Vector3* verts;  // +0x18
};
JET_LAYOUT32(sizeof(Polygon) == 0x1c);

// SPHERE (0x14 bytes): vftable +0x00, center +0x04, radius +0x10. No radiusXY in TS12.
class Sphere {
public:
    Sphere();
    Sphere(const Vector3& center, float radius);
    Sphere(const Sphere& sphere);
    Sphere& operator=(const Sphere& sphere);

    Vector3& __cdecl GetCenter();
    const Vector3& __cdecl GetCenter() const;
    float __cdecl GetRadius() const;
    void __cdecl SetCenter(const Vector3& center);
    void __cdecl SetRadius(float radius);
    virtual void __cdecl Maximize(const Sphere& sphere);
    bool __cdecl CheckVisibility(const Plane* frustum);

    bool __cdecl TestCollision(const Vector3& point) const;
    bool __cdecl TestCollision(const Vector3& center, float radius) const;
    bool __cdecl TestCollision(const Sphere& sphere) const;
    bool __cdecl TestCollision(const AxisAlignedBox& box) const;
    bool __cdecl TestCollision(const Cylinder& cylinder) const;
    bool __cdecl TestCollision(const Ellipsoid& ellipsoid) const;
    bool __cdecl TestCollision(const Line& line) const;
    bool __cdecl TestCollision(const LineSegment& segment) const;
    bool __cdecl TestCollision(const Plane& plane) const;
    bool __cdecl TestCollision(const Ray& ray) const;

    Vector3 center;  // +0x04
    float radius;    // +0x10
};
JET_LAYOUT32(sizeof(Sphere) == 0x14);

// ELLIPSOID (0x38 bytes): vftable +0x00, Plane base normal +0x04/d +0x10, center +0x14, major +0x20,
// minor +0x24, up +0x28, point +0x2c.
class Ellipsoid {
public:
    Ellipsoid();
    Ellipsoid(const Vector3& center, float major, float minor, float up, const Plane& plane, const Vector3& point);
    Ellipsoid(const Ellipsoid& ellipsoid);
    Ellipsoid& operator=(const Ellipsoid& ellipsoid);

    const Vector3& __cdecl GetCenter() const;
    float __cdecl GetMajor() const;
    float __cdecl GetMinor() const;
    float __cdecl GetUp() const;
    Matrix3 __cdecl GetMatrix() const;
    void __cdecl SetCenter(const Vector3& center);
    void __cdecl SetMajor(float major);
    void __cdecl SetMinor(float minor);
    void __cdecl SetUp(float up);

    // The original's vtable (0x1007eb90) is AxisAlignedBox, Ellipsoid, Cylinder, Sphere, Plane, Line, Vector3.
    // MSVC emits overloads of one name in REVERSE declaration order, so declare them the other way round.
    virtual bool __cdecl TestCollision(const Vector3& point);
    virtual bool __cdecl TestCollision(const Line& line);
    virtual bool __cdecl TestCollision(const Plane& plane);
    virtual bool __cdecl TestCollision(const Sphere& sphere) const;
    virtual bool __cdecl TestCollision(const Cylinder& cylinder) const;
    virtual bool __cdecl TestCollision(const Ellipsoid& ellipsoid) const;
    virtual bool __cdecl TestCollision(const AxisAlignedBox& box) const;

    Vector3 normal;  // +0x04 (Plane base)
    float d;         // +0x10
    Vector3 center;  // +0x14
    float major;     // +0x20
    float minor;     // +0x24
    float up;        // +0x28
    Vector3 point;   // +0x2c
};
JET_LAYOUT32(sizeof(Ellipsoid) == 0x38);

// ELLIPSE (0x30 bytes): Plane base normal +0x00/d +0x0c, center +0x10, major +0x1c, minor +0x20,
// point +0x24. Not polymorphic in TS12.
class Ellipse {
public:
    Ellipse();
    Ellipse(const Ellipse& ellipse);
    Ellipse(const Vector3& center, float major, float minor, const Vector3& point, const Plane& plane);
    Ellipse& operator=(const Ellipse& ellipse);

    Matrix3 __cdecl GetMatrix();
    bool __cdecl Overlap(Vector3& a, Vector3& b, Vector3& c, Vector3& d);
    bool __cdecl TestCollision(const Circle& circle);
    bool __cdecl TestCollision(const Line& line);
    bool __cdecl TestCollision(const Plane& plane);
    bool __cdecl TestCollision(const Sphere& sphere);

    Vector3 normal;  // +0x00 (Plane base)
    float d;         // +0x0c
    Vector3 center;  // +0x10
    float major;     // +0x1c
    float minor;     // +0x20
    Vector3 point;   // +0x24
};
JET_LAYOUT32(sizeof(Ellipse) == 0x30);

// CYLINDER (0x20 bytes): vftable +0x00, centers[2] +0x04, radius +0x1c.
class Cylinder {
public:
    Cylinder();
    Cylinder(const Vector3& center1, const Vector3& center2, float radius);
    Cylinder(const Cylinder& cylinder);
    Cylinder& operator=(const Cylinder& cylinder);

    const Vector3* __cdecl GetCenters() const;
    float __cdecl GetRadius() const;
    void __cdecl SetCenters(const Vector3* const centers);
    void __cdecl SetRadius(float radius);

    // The original's vtable (0x1007e7c8) is AxisAlignedBox, Ellipsoid, Cylinder, Sphere, Line, Vector3. MSVC emits
    // overloads of one name in reverse declaration order, so declare them the other way round.
    virtual bool __cdecl TestCollision(const Vector3& point);
    virtual bool __cdecl TestCollision(const Line& line);
    virtual bool __cdecl TestCollision(const Sphere& sphere) const;
    virtual bool __cdecl TestCollision(const Cylinder& cylinder) const;
    virtual bool __cdecl TestCollision(const Ellipsoid& ellipsoid) const;
    virtual bool __cdecl TestCollision(const AxisAlignedBox& box) const;

    Vector3 centers[2];  // +0x04
    float radius;        // +0x1c
};
JET_LAYOUT32(sizeof(Cylinder) == 0x20);

// CONE (0x20 bytes): centers[2] +0x00, radius +0x18, cosTheta +0x1c. Not polymorphic.
class Cone {
public:
    Cone();
    Cone(const Vector3& center1, const Vector3& center2, float radius);
    Cone& operator=(const Cone& cone);

    const Vector3& __cdecl GetCenter(int i) const;
    Vector3 __cdecl GetCenter(int i);
    void __cdecl GetCenter(Vector3& out, int i) const;
    float __cdecl GetCosTheta() const;
    float __cdecl GetRadius() const;
    void __cdecl SetCenter(const Vector3& center, int i);
    void __cdecl SetRadius(float radius);
    bool __cdecl TestCollision(const Line& line);
    bool __cdecl TestCollision(const Sphere& sphere);
    bool __cdecl TestCollision(const Vector3& point);

    Vector3 centers[2];  // +0x00
    float radius;        // +0x18
    float cosTheta;      // +0x1c

protected:
    void __cdecl CalculateCosTheta();
};
JET_LAYOUT32(sizeof(Cone) == 0x20);

// CIRCLE (0x20 bytes): Plane base normal +0x00/d +0x0c, center +0x10, radius +0x1c. Not polymorphic.
class Circle {
public:
    Circle();
    Circle(const Circle& circle);
    Circle(const Vector3& center, float radius, const Plane& plane);
    Circle& operator=(const Circle& circle);

    const Vector3& __cdecl GetCenter() const;
    float __cdecl GetRadius() const;
    void __cdecl SetCenter(Vector3 center);
    void __cdecl SetRadius(float radius);
    bool __cdecl TestCollision(const Circle& circle);
    bool __cdecl TestCollision(const Line& line);
    bool __cdecl TestCollision(const Plane& plane);
    bool __cdecl TestCollision(const Sphere& sphere);
    bool __cdecl Overlap(Vector3& a, Vector3& b, Vector3& c, Vector3& d);

    Vector3 normal;  // +0x00 (Plane base)
    float d;         // +0x0c
    Vector3 center;  // +0x10
    float radius;    // +0x1c
};
JET_LAYOUT32(sizeof(Circle) == 0x20);

}  // namespace Jet
