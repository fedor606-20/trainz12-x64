// Unit U1d1 of Render3DCore_7c: the line family (Line, LineSegment, Ray, Line2D, LineSegment2D, Circle2D,
// Polygon2D), transcribed from the TS12 assembly.
//
// Layouts are the TS12 ones, not the RC1 SDK header's: Line/Ray/LineSegment carry a vftable at +0, then the
// direction, the point (and, for a segment, the endpoint). The 2D shapes have no vftable; a Line2D is a direction
// followed by a point, a LineSegment2D two corners, a Circle2D a centre and a radius, and a Polygon2D a count, a
// vertex block and an ownership flag. Ordinary members of this module are __cdecl (this is pushed on the stack, as
// the original's callers do); constructors and the assignment are __thiscall. Comparison order is kept because NaN
// operands must produce the same result and the same payload as the original.
#pragma once

#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/FastDynamicArray.h>

namespace Jet {

class Matrix3;
class Plane;
class Sphere;
class LineSegment;
class Ray;
class Circle;
class Ellipse;
class Ellipsoid;

// WhichSide's answer. Only the values (1, 2, 3) are part of the ABI. Render3DCorePlane.h defines its own names
// for the same three values; the guard lets a tails translation unit see both headers.
#ifndef JET_R3D_CLIPRESULT_DEFINED
#define JET_R3D_CLIPRESULT_DEFINED
enum ClipResult {
    CLIP_SIDE_ONE = 1,
    CLIP_SIDE_TWO = 2,
    CLIP_ON_LINE = 3,
};
#endif

// 0x1c bytes: vftable +0, direction +0x04, point +0x10.
class Line {
public:
    Line();
    Line(const Vector3& direction, const Vector3& point);
    Line(const Vector3 points[2]);
    Line(const Line& line);
    Line(const LineSegment& line);
    Line(const Ray& line);
    Line& operator=(const Line& line);

    const Vector3& __cdecl GetDirection() const;
    const Vector3& __cdecl GetPoint() const;
    void __cdecl SetDirection(const Vector3& newDirection);
    void __cdecl SetPoint(const Vector3& newPoint);

    virtual Line& __cdecl Translate(const Vector3& delta);
    virtual Line& __cdecl Rotate(const Matrix3& m);
    Line& __cdecl SelfRotate(const Matrix3& m);
    Line& __cdecl Scale(float s);
    Line& __cdecl Normalize();
    bool __cdecl IsColinear(const Vector3& p, float threshold) const;

    Line& __cdecl operator+=(const Vector3& delta);
    Line& __cdecl operator-=(const Vector3& delta);
    Line& __cdecl operator*=(float s);
    Line& __cdecl operator/=(float s);

    float __cdecl Distance(const Vector3& point) const;
    float __cdecl Distance(const Line& line) const;
    float __cdecl DistanceSquared(const Vector3& testPoint) const;

    virtual bool __cdecl TestCollision(const Plane& plane) const;
    virtual bool __cdecl TestCollision(const Sphere& sphere) const;

    bool __cdecl TestCollision(Vector3 center, float radius2, Vector3& collisionPoint) const;
    bool __cdecl TestCollision(Vector3 center, float radius2, Vector3& collisionPoint1,
                               Vector3& collisionPoint2) const;
    bool __cdecl TestCollision(Vector3 center, float radius2, float& to1, float& to2) const;

    Vector3 direction; // +0x04
    Vector3 point;     // +0x10
};
JET_LAYOUT32(sizeof(Line) == 0x1c);
JET_LAYOUT32(offsetof(Line, direction) == 0x4);
JET_LAYOUT32(offsetof(Line, point) == 0x10);

// 0x28 bytes: the Line fields, then endPoint +0x1c. The direction is normalised (point -> endPoint).
class LineSegment {
public:
    LineSegment();
    LineSegment(const Vector3& point, const Vector3& endPoint);
    LineSegment(const Vector3 points[2]);
    LineSegment(const LineSegment& line);
    LineSegment& operator=(const LineSegment& line);

    const Vector3& __cdecl GetDirection() const;
    const Vector3& __cdecl GetPoint() const;
    const Vector3& __cdecl GetEndPoint() const;
    void __cdecl SetDirection(const Vector3& newDirection);
    void __cdecl SetPoint(const Vector3& newPoint);
    void __cdecl SetEndPoint(const Vector3& newEndPoint);

    Vector3 __cdecl GetClosestPoint(const Vector3& point) const;
    float __cdecl DistanceSquared(const Vector3& point) const;
    float __cdecl Distance(const Vector3& point) const;
    bool __cdecl InSegment(const Vector3& point) const;
    bool __cdecl IsColinear(const Vector3& p, float threshold) const;
    void __cdecl Normalize();
    void __cdecl Translate(const Vector3& delta);
    void __cdecl Rotate(const Matrix3& m);

    virtual bool __cdecl TestCollision(const Sphere& sphere) const;
    virtual bool __cdecl TestCollision(const Plane& plane) const;
    bool __cdecl TestCollision(Circle& circle) const;
    bool __cdecl TestCollision(Ellipse& ellipse) const;
    bool __cdecl TestCollision(Ellipsoid& ellipsoid) const;

    Vector3 direction; // +0x04
    Vector3 point;     // +0x10
    Vector3 endPoint;  // +0x1c
};
JET_LAYOUT32(sizeof(LineSegment) == 0x28);
JET_LAYOUT32(offsetof(LineSegment, direction) == 0x4);
JET_LAYOUT32(offsetof(LineSegment, point) == 0x10);
JET_LAYOUT32(offsetof(LineSegment, endPoint) == 0x1c);

// 0x1c bytes, same fields as Line but its own vftable (plane, sphere).
class Ray {
public:
    Ray();
    Ray(const Vector3& direction, const Vector3& point);
    Ray(const Vector3 points[2]);
    Ray(const Ray& ray);
    Ray(const LineSegment& line);
    Ray& operator=(const Ray& ray);

    const Vector3& __cdecl GetDirection() const;
    const Vector3& __cdecl GetPoint() const;
    void __cdecl SetDirection(const Vector3& newDirection);
    void __cdecl SetPoint(const Vector3& newPoint);

    void __cdecl Normalize();
    bool __cdecl IsColinear(const Vector3& p, float threshold) const;
    float __cdecl Distance(const Vector3& point) const;
    float __cdecl DistanceSquared(const Vector3& testPoint) const;

    virtual bool __cdecl TestCollision(const Plane& plane) const;
    virtual bool __cdecl TestCollision(const Sphere& sphere) const;

    bool __cdecl TestCollision(Vector3 center, float radius2, Vector3& collisionPoint) const;
    bool __cdecl TestCollision(Vector3 center, float radius2, Vector3& collisionPoint1,
                               Vector3& collisionPoint2) const;
    bool __cdecl TestCollision(Vector3 center, float radius2, float& to1, float& to2) const;

    Vector3 direction; // +0x04
    Vector3 point;     // +0x10
};
JET_LAYOUT32(sizeof(Ray) == 0x1c);
JET_LAYOUT32(offsetof(Ray, direction) == 0x4);
JET_LAYOUT32(offsetof(Ray, point) == 0x10);

// 0x10 bytes, no vftable: direction +0, point +8.
class Line2D {
public:
    Line2D();
    Line2D(const Vector2& direction, const Vector2& point);
    Line2D& operator=(const Line2D& line);

    const Vector2& __cdecl GetDirection() const;
    const Vector2& __cdecl GetPoint() const;
    void __cdecl SetDirection(const Vector2& newDirection);
    void __cdecl SetPoint(const Vector2& newPoint);

    ClipResult __cdecl WhichSide(const Vector2& point) const;
    ClipResult __cdecl WhichSide(const Vector2& point, float tolerance) const;
    bool __cdecl Intersects(const Line2D& line) const;
    Vector2 __cdecl Intercept(const Line2D& line) const;

    float __cdecl GetSeparation(const Vector2& point) const;
    float __cdecl GetSeparation(const Line2D& line) const;
    float __cdecl GetApproxSeparation(const Vector2& point) const;
    float __cdecl GetApproxSeparation(const Line2D& line) const;
    float __cdecl GetSeparationSquared(const Vector2& point) const;
    float __cdecl GetSeparationSquared(const Line2D& line) const;

    Vector2 direction; // +0x00
    Vector2 point;     // +0x08
};
JET_LAYOUT32(sizeof(Line2D) == 0x10);
JET_LAYOUT32(offsetof(Line2D, direction) == 0x0);
JET_LAYOUT32(offsetof(Line2D, point) == 0x8);

// 0x10 bytes, no vftable: two corners (v0 +0, v1 +8).
class LineSegment2D {
public:
    LineSegment2D();
    LineSegment2D(const Vector2& v0, const Vector2& v1);
    LineSegment2D& operator=(const LineSegment2D& line);

    const Vector2& __cdecl GetV0() const;
    const Vector2& __cdecl GetV1() const;
    void __cdecl SetExtents(const Vector2& v0, const Vector2& v1);

    ClipResult __cdecl WhichSide(const Vector2& point) const;
    ClipResult __cdecl WhichSide(const Vector2& point, float tolerance) const;
    bool __cdecl IsBeside(const Vector2& point) const;
    bool __cdecl Intersects(const Line2D& line) const;
    bool __cdecl Intersects(const LineSegment2D& line) const;
    bool __cdecl Intercept(const Line2D& line, Vector2& point) const;
    bool __cdecl Intercept(const Line2D& line, Vector2& point, float& t) const;

    float __cdecl GetSeparation(const Vector2& point) const;
    float __cdecl GetSeparation(const Line2D& line) const;
    float __cdecl GetSeparation(const LineSegment2D& line) const;
    float __cdecl GetApproxSeparation(const Vector2& point) const;
    float __cdecl GetApproxSeparation(const Line2D& line) const;
    float __cdecl GetApproxSeparation(const LineSegment2D& line) const;
    float __cdecl GetSeparationSquared(const Vector2& point) const;
    float __cdecl GetSeparationSquared(const Line2D& line) const;
    float __cdecl GetSeparationSquared(const LineSegment2D& line) const;

    Vector2 v0; // +0x00
    Vector2 v1; // +0x08
};
JET_LAYOUT32(sizeof(LineSegment2D) == 0x10);
JET_LAYOUT32(offsetof(LineSegment2D, v0) == 0x0);
JET_LAYOUT32(offsetof(LineSegment2D, v1) == 0x8);

// 0xc bytes, no vftable: centre +0, radius +8.
class Circle2D {
public:
    Circle2D();
    Circle2D(const Vector2& center, float radius);
    Circle2D(const Vector3& center, float radius);
    Circle2D& operator=(const Circle2D& circle);

    const Vector2& __cdecl GetCenter() const;
    void __cdecl SetCenter(const Vector2& newCenter);
    float __cdecl GetRadius() const;
    void __cdecl SetRadius(float radius);

    bool __cdecl Intersects(const Vector2& point) const;
    bool __cdecl Intersects(const Line2D& line) const;
    bool __cdecl Intersects(const Circle2D& circle) const;
    bool __cdecl Intersects(const LineSegment2D& line) const;

    float __cdecl GetSeparation(const Vector2& point) const;
    float __cdecl GetSeparation(const Line2D& line) const;
    float __cdecl GetSeparation(const Circle2D& circle) const;
    float __cdecl GetSeparation(const LineSegment2D& line) const;
    float __cdecl GetApproxSeparation(const Vector2& point) const;
    float __cdecl GetApproxSeparation(const Line2D& line) const;
    float __cdecl GetApproxSeparation(const Circle2D& circle) const;
    float __cdecl GetApproxSeparation(const LineSegment2D& line) const;
    float __cdecl GetSeparationSquared(const Vector2& point) const;
    float __cdecl GetSeparationSquared(const Line2D& line) const;
    float __cdecl GetSeparationSquared(const Circle2D& circle) const;
    float __cdecl GetSeparationSquared(const LineSegment2D& line) const;

    Vector2 center; // +0x00
    float radius;   // +0x08
};
JET_LAYOUT32(sizeof(Circle2D) == 0xc);
JET_LAYOUT32(offsetof(Circle2D, center) == 0x0);
JET_LAYOUT32(offsetof(Circle2D, radius) == 0x8);

// 0xc bytes, no vftable: count +0, vertices +4, ownsVertices +8.
class Polygon2D {
public:
    struct PointNode {
        Vector2 point; // +0x00
        float theta;   // +0x08
    };

    Polygon2D();
    Polygon2D(int numVerts, Vector2* vertices, bool ownsVertices);
    Polygon2D(const Polygon2D& polygon);
    ~Polygon2D();
    Polygon2D& operator=(const Polygon2D& polygon);

    void __cdecl SetNumVerts(int numVerts);
    int __cdecl GetNumVerts() const;
    Vector2* __cdecl GetVertices();
    const Vector2* __cdecl GetConstVertices() const;
    void __cdecl Set(int numVerts, Vector2* vertices, bool ownsVertices);
    bool __cdecl Set(int numVerts);
    void __cdecl Set(const FastDynamicArray<Vector2, 13, Less<Vector2, Vector2>>& vertices);

    Vector2 __cdecl GetCenter() const;
    void __cdecl GrahamScan();

    bool __cdecl Intersects(const Vector2& point) const;
    bool __cdecl Intersects(const Line2D& line) const;
    bool __cdecl Intersects(const LineSegment2D& line) const;
    bool __cdecl Intersects(const Circle2D& circle) const;
    bool __cdecl Intersects(const Polygon2D& polygon) const;

    float __cdecl GetSeparation(const Vector2& point) const;
    float __cdecl GetSeparation(const Line2D& line) const;
    float __cdecl GetSeparation(const LineSegment2D& line) const;
    float __cdecl GetSeparation(const Circle2D& circle) const;
    float __cdecl GetSeparation(const Polygon2D& polygon) const;
    float __cdecl GetApproxSeparation(const Vector2& point) const;
    float __cdecl GetApproxSeparation(const Line2D& line) const;
    float __cdecl GetApproxSeparation(const LineSegment2D& line) const;
    float __cdecl GetApproxSeparation(const Circle2D& circle) const;
    float __cdecl GetApproxSeparation(const Polygon2D& polygon) const;
    float __cdecl GetSeparationSquared(const Vector2& point) const;
    float __cdecl GetSeparationSquared(const Line2D& line) const;
    float __cdecl GetSeparationSquared(const LineSegment2D& line) const;
    float __cdecl GetSeparationSquared(const Circle2D& circle) const;
    float __cdecl GetSeparationSquared(const Polygon2D& polygon) const;

protected:
    int __cdecl CounterClockwise(const Vector2* a, const Vector2* b, const Vector2* c);
    float __cdecl Theta(const Vector2* a, const Vector2* b);
    void __cdecl ShellSortTheta(PointNode* nodes, int& count);

public:
    int numVerts;         // +0x00
    Vector2* vertices;    // +0x04
    bool ownsVertices;    // +0x08
};
JET_LAYOUT32(sizeof(Polygon2D) == 0xc);
JET_LAYOUT32(offsetof(Polygon2D, numVerts) == 0x0);
JET_LAYOUT32(offsetof(Polygon2D, vertices) == 0x4);
JET_LAYOUT32(offsetof(Polygon2D, ownsVertices) == 0x8);

}  // namespace Jet
