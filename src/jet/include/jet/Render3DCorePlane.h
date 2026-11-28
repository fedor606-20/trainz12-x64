// Render3DCore_7c unit U1b (notes/Render3DCore.md): Jet::Plane. Layout and behaviour follow the TS12 assembly
// (addresses per member in Render3DCore_7c/Plane.cpp). The class has no vftable and is 0x10 bytes: {normal, d}.
// Ordinary members are __cdecl (this is pushed on the stack, as the original's callers do); constructors, the
// compiler-generated assignment and WhichSideAxisBox are __thiscall. Clip, Split and Intersect(const Plane&) need
// Polygon and the Line vftable from other units, so they keep forwarding to Render3DCore_7c_orig.dll.
#pragma once

#include <jet/Math.h>
#include <jet/RenderTypes.h>

namespace Jet {

class Line;
class LineSegment;
class Ray;
class Sphere;
class Polygon;

// ClipResult.h of the Jet SDK: FRONT 1, BACK 2, COINCIDENT 3, COPLANAR 4. Other units that need the same enum can
// share this guard until the type has one home in Render3DCore.h.
#ifndef JET_R3D_CLIPRESULT_DEFINED
#define JET_R3D_CLIPRESULT_DEFINED
enum ClipResult {
    FRONT = 1,      // only in the front space
    BACK = 2,       // only in the back space
    COINCIDENT = 3, // parts in both spaces
    COPLANAR = 4    // on the plane
};
#endif

class Plane {
public:
    Plane();
    Plane(const Plane& o);
    Plane(const Vector3& n, float d);
    Plane(const Vector3& n, const Vector3& p);
    Plane(const Vector3& p1, const Vector3& p2, const Vector3& p3);
    Plane& operator=(const Plane& o);

    // creation
    void __cdecl Set(const Vector3& n, const Vector3& p);
    void __cdecl Set(const Vector3& n, float d);
    void __cdecl Set(const Vector3& p1, const Vector3& p2, const Vector3& p3);
    void __cdecl Set(const Vector3* threePoints);
    void __cdecl Set(const Plane& plane);
    void __cdecl SetDistance(const Vector3& point);
    void __cdecl SetDistance(float d);
    Plane& __cdecl Normalize();

    // data access
    const Vector3& __cdecl GetNormal() const;
    void __cdecl SetNormal(const Vector3& n);
    void __cdecl SetNormal(float x, float y, float z);
    float __cdecl GetDistance() const;
    Vector3 __cdecl GetPoint() const;

    // clip tests
    ClipResult __cdecl WhichSide(const Vector3& p) const;
    ClipResult __cdecl WhichSide(const Vector3& p, float threshold) const;
    ClipResult __cdecl WhichSide(int vertices, const Vector3* vertexArray) const;
    ClipResult __cdecl WhichSide(const Sphere& sphere) const;
    ClipResult __cdecl WhichSideSphere(const Vector3& center, float radius) const;
    ClipResult __thiscall WhichSideAxisBox(const Vector3& center, const Vector3& extent, float& planeDistance,
                                           float& radius, float threshold) const;
    bool __cdecl IsBehind(const Vector3& center, float radius) const;
    bool __cdecl IsInFront(const Vector3& center, float radius) const;

    // geometry
    ClipResult __cdecl Clip(const Polygon& poly, Polygon*& clipped) const;
    void __cdecl Split(const Polygon& poly, Polygon& front, Polygon& back) const;
    Vector3 __cdecl Intersect(const Line& line) const;
    Vector3 __cdecl Intersect(const Line& line, float& t) const;
    Line __cdecl Intersect(const Plane& plane) const;
    bool __cdecl Intersect(bool twoSided, const Line& line, Vector3& out) const;
    bool __cdecl Intersect(bool twoSided, const Line& line, Vector3& out, float& t) const;
    bool __cdecl Intersect(bool twoSided, const Ray& ray, Vector3& out) const;
    bool __cdecl Intersect(bool twoSided, const Ray& ray, Vector3& out, float& t) const;
    bool __cdecl Intersect(bool twoSided, const LineSegment& segment, Vector3& out) const;
    bool __cdecl Intersect(bool twoSided, const LineSegment& segment, Vector3& out, float& t) const;
    float __cdecl Distance(const Vector3& point) const;
    Vector3 __cdecl Project(const Vector3& point) const;

    // transforms
    void __cdecl Transform(const Matrix4& m, bool transform, float extra);
    void __cdecl SetFromTransformPlane(const Matrix4& m, bool transform, const Plane& src, float extra);
    static void __cdecl TransformPlane(const Matrix4& m, bool transform, const Vector3& v, float inD, Vector3& outN,
                                       float& outD, float extra);
    void __cdecl Flip();

public:
    Vector3 normal; // +0x00
    float d;        // +0x0c
};
JET_LAYOUT32(sizeof(Plane) == 0x10);

}  // namespace Jet
