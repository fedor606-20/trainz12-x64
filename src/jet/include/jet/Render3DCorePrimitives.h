// Render3DCore_7c unit U10: the render-primitive family (RenderPrimitive, the 2D/3D bases, the debug shapes and
// RenderLines). Layouts and behaviour follow the TS12 assembly (addresses per member in the .cpp files).
//
// Hierarchy and object sizes:
//   Particle (0x12c)
//     RenderPrimitive (0x188)              +0x13c kernel, +0x140 chunk array, +0x144 material array,
//                                         +0x148 orientation, +0x158 position, +0x164 scale, +0x170 color,
//                                         +0x180 creation flags, +0x184 flags byte
//       Render2DPrimitive (0x188)         adds TestCollisionGeometry and IsWithin
//       Render3DPrimitive (0x188)         adds TestCollisionGeometry and GeometryTestCollision
//       RenderBox (0x194)                 +0x188 extents
//       RenderSphere (0x18c), RenderCircle (0x18c), RenderEllipse (0x190), RenderCylinder (0x190),
//       RenderCone (0x190), RenderEllipsoid (0x194), RenderPlane (0x190), RenderTube (0x194)
//       RenderPolyline (0x198)            +0x188 radius, +0x18c center
//         RenderLine (0x198), RenderSpline (0x198)
//     RenderLines (0x188)                 five pointer pools and their live counts
//   RenderPolygon : Render2DPrimitive, Polygon (0x1a4; Polygon at +0x188)
//
// Ordinary members are __cdecl (this on the stack as the first argument), constructors, the assignment and the
// virtuals are __thiscall, as their mangled names say. The virtual order below is the original's vtable order:
// slots 0x00..0xb8 Particle, 0xbc SetScale.. 0xd8 Init, 0xdc GetGeometryRadius, 0xe0 GetGeometryCenter,
// 0xe4 CalculatePrimitiveVolume; the 2D/3D bases add TestCollisionGeometry at 0xe8 and their own test at 0xec.
#pragma once

#include <jet/DynamicArray.h>
#include <jet/FastDynamicArray.h>
#include <jet/Geometry.h>
#include <jet/InputSystem.h>
#include <jet/Math.h>
#include <jet/Render3DCoreMaterials.h>
#include <jet/Render3DCoreParticle.h>
#include <jet/Render3DCoreSolids.h>
#include <jet/Render3DCoreSplines.h>
#include <jet/RenderSystem.h>
#include <jet/RenderTypes.h>
#include <jet/SmallTypes.h>

namespace Jet {

class Display;
class GeometryChunk;
class Material;
class RenderKernel;
class ViewPort;
class ViewPoint;

// The debug shapes' geometry interleaves a position with a packed colour; the item type numbers are the original's.
enum : int {
    R3D_ITEM_COLOR3 = 4,
    R3D_ITEM_COLOR = 2,
    R3D_ITEM_POSITION = 1,
};

// One reference-counted block of AutoReference<T> objects: {refs, count, unused[2], items}. The block's last
// reference releases each item and frees the block (the original's inlined helpers at 0x22540/0x225b0).
template <class T>
using AutoReferenceBlock = typename AutoReferenceArray<AutoReference<T>>::Block;

class RenderPrimitive : public ViewPortParticle {
public:
    RenderPrimitive();
    explicit RenderPrimitive(int allocation);
    RenderPrimitive(const RenderPrimitive& o);
    virtual ~RenderPrimitive();
    RenderPrimitive& operator=(const RenderPrimitive& o);

    // Particle overrides
    virtual void __cdecl Cull(ViewPoint& view, const Matrix4& m) override;
    virtual float __cdecl GetOpacity() const override;
    virtual void __cdecl SetOpacity(float opacity) override;

    // new virtuals, in the original's slot order
    virtual void __cdecl SetScale(const Vector3& scale);
    virtual const Vector3& __cdecl GetScale() const;
    virtual void __cdecl SetGeometryOrientation(const Quaternion& orientation);
    virtual const Quaternion& __cdecl GetGeometryOrientation() const;
    virtual void __cdecl SetGeometryPosition(const Vector3& position);
    virtual const Vector3& __cdecl GetGeometryPosition() const;

    // the original's slots 0xd8..0xe4, all protected; declaration order is the vtable order
protected:
    virtual bool __cdecl Init(RenderKernel* kernel, const Color& color, int creationFlags);
    virtual float __cdecl GetGeometryRadius() const;
    virtual Vector3 __cdecl GetGeometryCenter() const;
    virtual void __cdecl CalculatePrimitiveVolume();

public:
    int __cdecl GetCreationFlags() const;
    unsigned int __cdecl GetNumChunks() const;
    AutoReferenceArray<AutoReference<GeometryChunk>> __cdecl GetChunkArray();
    const AutoReferenceArray<AutoReference<GeometryChunk>> __cdecl GetChunkArray() const;
    AutoReferenceArray<AutoReference<Material>> __cdecl GetMaterialArray();
    const AutoReferenceArray<AutoReference<Material>> __cdecl GetMaterialArray() const;
    const Color& __cdecl GetColor() const;
    void __cdecl SetColor(const Color& color);
    void __cdecl SetTestCollision(bool state);
    bool __cdecl GetTestCollision() const;
    void __cdecl SetDebugRender(bool state);
    bool __cdecl GetDebugRender() const;

protected:
    static FastDynamicArray<DisplayLight*, 13, Less<DisplayLight*, DisplayLight*>> lights;

protected:
    virtual void __cdecl Render(Display* display, ViewPort& port, Matrix4& m) override;

    void __cdecl CalculateBoxVolume();
    GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                               unsigned int numIndices, unsigned int numFaceNormals,
                                               Material::GeometryType type, GeometryChunkCreateHint* hint);
    void __cdecl InitInstance(RenderPrimitive& instance) const;

    RenderKernel* kernel;                              // +0x13c
    AutoReferenceArray<AutoReference<GeometryChunk>> chunkArray;   // +0x140
    AutoReferenceArray<AutoReference<Material>> materialArray;     // +0x144
    Quaternion geometryOrientation;                    // +0x148
    Vector3 geometryPosition;                          // +0x158
    Vector3 scale;                                     // +0x164
    Color color;                                       // +0x170
    int creationFlags;                                 // +0x180
    unsigned char flags;                               // +0x184 bit 0 orientation, 1 position, 2 scale, 3 debug
    unsigned char padding185[3];
};
JET_LAYOUT32(sizeof(RenderPrimitive) == 0x188);



class Render2DPrimitive : public RenderPrimitive {
public:
    Render2DPrimitive();
    explicit Render2DPrimitive(int allocation);
    Render2DPrimitive(const Render2DPrimitive& o);
    virtual ~Render2DPrimitive();
    Render2DPrimitive& operator=(const Render2DPrimitive& o);

    virtual bool __cdecl TestCollisionGeometry(const Ray& ray, float t, Vector3& point,
                                               ConstFaceDescriptor& face) const;
    virtual bool __cdecl IsWithin(const Vector3& point) const;

};
JET_LAYOUT32(sizeof(Render2DPrimitive) == 0x188);


class Render3DPrimitive : public RenderPrimitive {
public:
    Render3DPrimitive();
    explicit Render3DPrimitive(int allocation);
    Render3DPrimitive(const Render3DPrimitive& o);
    virtual ~Render3DPrimitive();
    Render3DPrimitive& operator=(const Render3DPrimitive& o);

    virtual bool __cdecl TestCollisionGeometry(const Ray& ray, float t, Vector3& point,
                                               ConstFaceDescriptor& face) const;
    virtual bool __cdecl GeometryTestCollision(const Ray& ray, Vector3& point) const;

};
JET_LAYOUT32(sizeof(Render3DPrimitive) == 0x188);


class RenderBox : public Render3DPrimitive {
public:
    RenderBox();
    RenderBox(const RenderBox& o);
    virtual ~RenderBox();
    RenderBox& operator=(const RenderBox& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, float width, float height, float depth,
                      unsigned int xSegments, unsigned int ySegments, unsigned int zSegments, int creationFlags);
    const Vector3& __cdecl GetExtents() const;


protected:
    bool __cdecl BuildGeometry(RenderKernel* kernel, const Color& color, int creationFlags, int xSegments,
                                       int ySegments, int zSegments);
    void __cdecl BuildIndicesSolid(int xSegments, int ySegments, int zSegments, int xCells, unsigned short* indices,
                                   int vertexCount);
    void __cdecl BuildIndicesWire(int xSegments, int ySegments, int zSegments, int xCells, unsigned short* indices,
                                  int vertexCount);
    virtual float __cdecl GetGeometryRadius() const override;
    virtual bool __cdecl GeometryTestCollision(const Ray& ray, Vector3& point) const;
    virtual void __cdecl CalculatePrimitiveVolume() override;
    void __cdecl InitInstance(RenderBox& instance) const;

    Vector3 extents;  // +0x188 (half the size)
};
JET_LAYOUT32(sizeof(RenderBox) == 0x194);


class RenderSphere : public Render3DPrimitive {
public:
    RenderSphere();
    RenderSphere(const RenderSphere& o);
    virtual ~RenderSphere();
    RenderSphere& operator=(const RenderSphere& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, float radius, unsigned int segments,
                      int creationFlags);
    float __cdecl GetRadius() const;


protected:
    bool __cdecl BuildGeometry(RenderKernel* kernel, const Color& color, int creationFlags, int segments);
    void __cdecl BuildIndicesSolid(int segments, int rings, unsigned short* indices, int vertexCount);
    void __cdecl BuildIndicesWire(int segments, int rings, unsigned short* indices, int vertexCount);
    virtual float __cdecl GetGeometryRadius() const override;
    virtual bool __cdecl GeometryTestCollision(const Ray& ray, Vector3& point) const;
    void __cdecl InitInstance(RenderSphere& instance) const;

    float radius;  // +0x188
};
JET_LAYOUT32(sizeof(RenderSphere) == 0x18c);


class RenderCircle : public Render2DPrimitive {
public:
    RenderCircle();
    RenderCircle(const RenderCircle& o);
    virtual ~RenderCircle();
    RenderCircle& operator=(const RenderCircle& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, float radius, unsigned int segments,
                      int creationFlags);


protected:
    bool __cdecl BuildGeometry(RenderKernel* kernel, const Color& color, int creationFlags, int segments);
    virtual float __cdecl GetGeometryRadius() const override;
    virtual bool __cdecl IsWithin(const Vector3& point) const;
    void __cdecl InitInstance(RenderCircle& instance) const;

    float radius;  // +0x188
};
JET_LAYOUT32(sizeof(RenderCircle) == 0x18c);


class RenderEllipse : public Render2DPrimitive {
public:
    RenderEllipse();
    RenderEllipse(const RenderEllipse& o);
    virtual ~RenderEllipse();
    RenderEllipse& operator=(const RenderEllipse& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, float radiusX, float radiusY,
                      unsigned int segments, int creationFlags);


protected:
    bool __cdecl BuildGeometry(RenderKernel* kernel, const Color& color, int creationFlags, int segments);
    virtual float __cdecl GetGeometryRadius() const override;
    virtual bool __cdecl IsWithin(const Vector3& point) const;
    void __cdecl InitInstance(RenderEllipse& instance) const;

    float radiusX;  // +0x188
    float radiusY;  // +0x18c
};
JET_LAYOUT32(sizeof(RenderEllipse) == 0x190);


class RenderCone : public Render3DPrimitive {
public:
    RenderCone();
    RenderCone(const RenderCone& o);
    virtual ~RenderCone();
    RenderCone& operator=(const RenderCone& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, float radius, float height, unsigned int segments,
                      int creationFlags);


protected:
    bool __cdecl BuildGeometry(RenderKernel* kernel, const Color& color, int creationFlags, int segments);
    void __cdecl BuildIndicesSolid(int segments, int rows, unsigned short* indices, int vertexCount);
    void __cdecl BuildIndicesWire(int segments, int rows, unsigned short* indices, int vertexCount);
    virtual float __cdecl GetGeometryRadius() const override;
    virtual Vector3 __cdecl GetGeometryCenter() const override;
    virtual bool __cdecl GeometryTestCollision(const Ray& ray, Vector3& point) const;
    void __cdecl InitInstance(RenderCone& instance) const;

    float radius;  // +0x188
    float height;  // +0x18c
};
JET_LAYOUT32(sizeof(RenderCone) == 0x190);


class RenderCylinder : public Render3DPrimitive {
public:
    RenderCylinder();
    RenderCylinder(const RenderCylinder& o);
    virtual ~RenderCylinder();
    RenderCylinder& operator=(const RenderCylinder& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, float radius, float height, unsigned int segments,
                      int creationFlags);


protected:
    bool __cdecl BuildGeometry(RenderKernel* kernel, const Color& color, int creationFlags, int segments);
    void __cdecl BuildIndicesSolid(int segments, int rows, unsigned short* indices, int vertexCount);
    void __cdecl BuildIndicesWire(int segments, int rows, unsigned short* indices, int vertexCount);
    virtual float __cdecl GetGeometryRadius() const override;
    virtual Vector3 __cdecl GetGeometryCenter() const override;
    virtual bool __cdecl GeometryTestCollision(const Ray& ray, Vector3& point) const;
    void __cdecl InitInstance(RenderCylinder& instance) const;

    float radius;  // +0x188
    float height;  // +0x18c
};
JET_LAYOUT32(sizeof(RenderCylinder) == 0x190);


class RenderEllipsoid : public Render3DPrimitive {
public:
    RenderEllipsoid();
    RenderEllipsoid(const RenderEllipsoid& o);
    virtual ~RenderEllipsoid();
    RenderEllipsoid& operator=(const RenderEllipsoid& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, float radiusX, float radiusY, float radiusZ,
                      unsigned int segments, int creationFlags);


protected:
    bool __cdecl BuildGeometry(RenderKernel* kernel, const Color& color, int creationFlags, int segments);
    void __cdecl BuildIndicesSolid(int segments, int rings, unsigned short* indices, int vertexCount);
    void __cdecl BuildIndicesWire(int segments, int rings, unsigned short* indices, int vertexCount);
    virtual float __cdecl GetGeometryRadius() const override;
    virtual bool __cdecl GeometryTestCollision(const Ray& ray, Vector3& point) const;
    void __cdecl InitInstance(RenderEllipsoid& instance) const;

    float radiusX;  // +0x188
    float radiusY;  // +0x18c
    float radiusZ;  // +0x190
};
JET_LAYOUT32(sizeof(RenderEllipsoid) == 0x194);


class RenderPlane : public Render2DPrimitive {
public:
    RenderPlane();
    RenderPlane(const RenderPlane& o);
    virtual ~RenderPlane();
    RenderPlane& operator=(const RenderPlane& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, float width, float height, unsigned int xSegments,
                      unsigned int ySegments, int creationFlags);


protected:
    virtual bool __cdecl BuildGeometry(RenderKernel* kernel, const Color& color, float width, float height,
                                       int xSegments, int ySegments, int creationFlags);
    void __cdecl BuildIndicesSolid(int xSegments, int ySegments, int flags, unsigned short* indices,
                                   int vertexCount);
    void __cdecl BuildIndicesWire(int xSegments, int ySegments, int flags, unsigned short* indices,
                                  int vertexCount);
    virtual float __cdecl GetGeometryRadius() const override;
    virtual bool __cdecl IsWithin(const Vector3& point) const;
    virtual void __cdecl CalculatePrimitiveVolume() override;
    void __cdecl InitInstance(RenderPlane& instance) const;

    float width;   // +0x188
    float height;  // +0x18c
};
JET_LAYOUT32(sizeof(RenderPlane) == 0x190);


class RenderTube : public Render3DPrimitive {
public:
    RenderTube();
    RenderTube(const RenderTube& o);
    virtual ~RenderTube();
    RenderTube& operator=(const RenderTube& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, float radius, float height, float thickness,
                      unsigned int segments, int creationFlags);


protected:
    bool __cdecl BuildGeometry(RenderKernel* kernel, const Color& color, int creationFlags, int segments);
    void __cdecl BuildIndicesSolid(int segments, int rows, unsigned short* indices, int vertexCount);
    void __cdecl BuildIndicesWire(int segments, int rows, unsigned short* indices, int vertexCount);
    virtual float __cdecl GetGeometryRadius() const override;
    virtual Vector3 __cdecl GetGeometryCenter() const override;
    virtual bool __cdecl GeometryTestCollision(const Ray& ray, Vector3& point) const;
    virtual void __cdecl CalculatePrimitiveVolume() override;
    void __cdecl InitInstance(RenderTube& instance) const;

    float radius;     // +0x188
    float height;     // +0x18c
    float thickness;  // +0x190
};
JET_LAYOUT32(sizeof(RenderTube) == 0x194);


class RenderPolyline : public RenderPrimitive {
public:
    RenderPolyline();
    RenderPolyline(const RenderPolyline& o);
    virtual ~RenderPolyline();
    RenderPolyline& operator=(const RenderPolyline& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, const Vector3* points, int count, int creationFlags);


protected:
    virtual bool __cdecl BuildGeometry(RenderKernel* kernel, const Color& color, const Vector3* points, int count,
                                       int creationFlags);
    virtual float __cdecl GetGeometryRadius() const override;
    virtual Vector3 __cdecl GetGeometryCenter() const override;
    virtual void __cdecl CalculatePrimitiveVolume() override;
    void __cdecl InitInstance(RenderPolyline& instance) const;

    float radius;      // +0x188
    Vector3 center;    // +0x18c
};
JET_LAYOUT32(sizeof(RenderPolyline) == 0x198);


class RenderLine : public RenderPolyline {
public:
    RenderLine();
    RenderLine(const RenderLine& o);
    virtual ~RenderLine();
    RenderLine& operator=(const RenderLine& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, const Vector3& from, const Vector3& to,
                      int creationFlags);


protected:
    void __cdecl InitInstance(RenderLine& instance) const;
    virtual void __cdecl CalculatePrimitiveVolume() override;
};
JET_LAYOUT32(sizeof(RenderLine) == 0x198);


class RenderSpline : public RenderPolyline {
public:
    RenderSpline();
    RenderSpline(const RenderSpline& o);
    virtual ~RenderSpline();
    RenderSpline& operator=(const RenderSpline& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, const Spline& spline, unsigned int steps,
                      int creationFlags);


protected:
    void __cdecl InitInstance(RenderSpline& instance) const;
    virtual void __cdecl CalculatePrimitiveVolume() override;
};
JET_LAYOUT32(sizeof(RenderSpline) == 0x198);


class RenderPolygon : public Render2DPrimitive, public Polygon {
public:
    RenderPolygon();
    RenderPolygon(const RenderPolygon& o);
    virtual ~RenderPolygon();
    RenderPolygon& operator=(const RenderPolygon& o);

    virtual Particle* __cdecl CreateInstance() const override;
    bool __cdecl Init(RenderKernel* kernel, const Color& color, const Polygon& polygon, int creationFlags);
    bool __cdecl Init(RenderKernel* kernel, const Color& color, int count, const Vector3* points, int creationFlags);


protected:
    virtual bool __cdecl BuildGeometry(RenderKernel* kernel, const Color& color, const Polygon& polygon,
                                       int creationFlags);
    virtual float __cdecl GetGeometryRadius() const override;
    virtual bool __cdecl IsWithin(const Vector3& point) const;
    virtual void __cdecl CalculatePrimitiveVolume() override;
    void __cdecl InitInstance(RenderPolygon& instance) const;
};
JET_LAYOUT32(sizeof(RenderPolygon) == 0x1a4);


// A pool of debug primitives: five {count, capacity, data} blocks (spheres added by Add(Vector3), lines by
// Add(Vector3,Vector3), polygons by Add(Vector3*,count)), a live count and a frame stamp beside each, then the
// kernel the objects were made with.
class RenderLines : public Particle {
public:
    struct Pool {
        unsigned int count;      // +0
        unsigned int capacity;   // +4
        void** data;             // +8
    };

    RenderLines();
    RenderLines(const RenderLines& o);
    virtual ~RenderLines();
    RenderLines& operator=(const RenderLines& o);

    virtual void __cdecl Cull(ViewPoint& view, const Matrix4& m) override;
    virtual void __cdecl RemoveAll() override;
    virtual void __cdecl DeleteAll() override;

    void __cdecl Add(RenderKernel* kernel, const Vector3& a, const Vector3& b, const Color& color);
    void __cdecl Add(RenderKernel* kernel, const Vector3& point, const Color& color);
    void __cdecl Add(RenderKernel* kernel, const Vector3* points, int count, const Color& color);
    void __cdecl AddPolygon(RenderKernel* kernel, const Vector3& a, const Vector3& b, const Vector3& c,
                            const Vector3& d, const Color& color);
    static void __cdecl SetState(bool state);
    static bool __cdecl GetState();
    static bool __cdecl Switch();


private:
    static bool enabled;

    Pool spheres;             // +0x12c
    unsigned int sphereCount; // +0x138
    unsigned int sphereCap;   // +0x13c
    Pool lines;               // +0x140
    Pool linePool;            // +0x14c
    unsigned int lineCount;   // +0x158
    unsigned int lineCap;     // +0x15c
    Pool polygons;            // +0x160
    Pool polygonPool;         // +0x16c
    unsigned int polygonCount;  // +0x178
    unsigned int polygonCap;    // +0x17c
    unsigned int lastFrame;     // +0x180
    RenderKernel* kernel;       // +0x184
};
JET_LAYOUT32(sizeof(RenderLines) == 0x188);


}  // namespace Jet
