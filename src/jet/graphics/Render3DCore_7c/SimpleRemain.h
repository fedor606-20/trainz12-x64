// Render3DCore_7c wave 6: the simple remaining classes of the Camera/DebugRender/Tube/Transformation slice.
//
// Layouts and calling conventions are taken from the TS12 assembly (addresses per member in the .cpp files of this
// unit). Ordinary members are __cdecl (this on the stack, mangled QAA/UAA); constructors, destructors and the
// compiler-generated assignments are __thiscall (QAE/UAE). Classes that have no data beyond a vftable keep exactly
// that: Primitive3D and ParticleFXRenderItem are 4 bytes with a 12-slot vtable (single inheritance, no virtual
// destructor), DebugRender and LightLoader are empty.
#pragma once

#include <jet/DynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/Render3DCoreBounds.h>
#include <jet/Render3DCoreCamera.h>
#include <jet/Render3DCoreLines.h>
#include <jet/Render3DCoreScene.h>
#include <jet/Render3DCoreSolids.h>
#include <jet/RenderSystem.h>
#include <jet/RenderTypes.h>
#include <jet/SmallTypes.h>

#include <cstddef>

namespace Jet {

class CollisionResult;
class IndexedMeshInstance;
class Light;

// ---- Transformation (0x30 bytes): a row-vector 3x3 rotation then the translation ---------------------------------
// +0x00..+0x24 Matrix3 (row-major), +0x24..+0x30 Vector3. Transform computes v * M + t; InverseTransform
// computes (v - t) * M^T (dot with the rows).
class Transformation {
public:
    Transformation();
    Transformation(const Matrix3& rotation, const Vector3& translation);
    Transformation& operator=(const Transformation& o);

    void __cdecl MultiplyTransform(const Matrix3& rotation, const Vector3& translation,
                                   Transformation& result) const;
    Vector3 __cdecl Transform(const Vector3& v) const;
    Vector3 __cdecl InverseTransform(const Vector3& v) const;
    void __cdecl TransformArray(int count, const Vector3* in, Vector3* out);
    Transformation __cdecl operator*(const Transformation& o);
    Transformation& __cdecl operator*=(const Transformation& o);

    Matrix3 rotation;     // +0x00
    Vector3 translation;  // +0x24
};
JET_LAYOUT32(sizeof(Transformation) == 0x30);
JET_LAYOUT32(offsetof(Transformation, translation) == 0x24);

// ---- Tube (0x1c bytes): two centres and a radius ------------------------------------------------------------------
class Tube {
public:
    Tube();
    Tube(const Vector3& start, const Vector3& end, float radius);
    Tube& operator=(const Tube& o);

    const Vector3& __cdecl GetCenter(int index) const;
    Vector3 __cdecl GetCenter(int index);
    void __cdecl GetCenter(Vector3& out, int index) const;
    Vector3* __cdecl GetCenters() const;
    float __cdecl GetRadius() const;
    void __cdecl SetCenter(const Vector3& center, int index);
    void __cdecl SetCenters(const Vector3* const centers);
    void __cdecl SetRadius(float radius);

    bool __cdecl TestCollision(const Vector3& point);
    bool __cdecl TestCollision(const Line& line);
    bool __cdecl TestCollision(const Sphere& sphere);
    bool __cdecl TestCollision(const Tube& tube);

    Vector3 centers[2];  // +0x00
    float radius;        // +0x18
};
JET_LAYOUT32(sizeof(Tube) == 0x1c);

// ---- Primitive3D (4 bytes): the vtable-only primitive interface --------------------------------------------------
// Slot 2 and 3 receive (Transformation const&, Vector3 const&) and are what Transform/InverseTransform call; slot 4
// is Intersects. The original's remaining slots are the shared "return 0" stubs, so the declarations only have to
// keep the slot count and order.
class Primitive3D {
public:
    Primitive3D();
    Primitive3D(const Primitive3D& o);
    Primitive3D& operator=(const Primitive3D& o);

    virtual void* __cdecl vf0();
    virtual void* __cdecl vf1();
    virtual void __cdecl TransformInstance(const Transformation& transformation, const Vector3& translation);
    virtual void __cdecl InverseTransformInstance(const Transformation& transformation, const Vector3& translation);
    virtual bool __cdecl Intersects(const IndexedMeshInstance& mesh, const Transformation& transformation,
                                    CollisionResult& result) const;
    virtual void* __cdecl vf5();
    virtual void* __cdecl vf6();
    virtual void* __cdecl vf7();
    virtual void* __cdecl vf8();
    virtual void* __cdecl vf9();
    virtual void* __cdecl vf10();
    virtual void* __cdecl vf11();

    void __cdecl Transform(const Transformation& transformation);
    void __cdecl InverseTransform(const Transformation& transformation);
};
JET_LAYOUT32(sizeof(Primitive3D) == 4);

// ---- ParticleFXRenderItem (4 bytes): the vtable-only render item -------------------------------------------------
class ParticleFXRenderItem {
public:
    ParticleFXRenderItem();
    ParticleFXRenderItem(const ParticleFXRenderItem& o);
    ParticleFXRenderItem& operator=(const ParticleFXRenderItem& o);

    virtual void* __cdecl vf0();
    virtual void* __cdecl vf1();
    virtual void* __cdecl vf2();
    virtual void* __cdecl vf3();
    virtual void* __cdecl vf4();
    virtual void* __cdecl vf5();
    virtual void* __cdecl vf6();
    virtual void* __cdecl vf7();
    virtual void* __cdecl vf8();
    virtual void* __cdecl vf9();
    virtual void* __cdecl vf10();
    virtual void* __cdecl vf11();
};
JET_LAYOUT32(sizeof(ParticleFXRenderItem) == 4);

// ---- DebugRender (empty): debug flags and static guards ----------------------------------------------------------
class DebugRender {
public:
    DebugRender(const Color& color, unsigned long mask);
    DebugRender& operator=(const DebugRender& o);

    void __cdecl Cull(RenderKernel* kernel, ViewPoint& view, Particle* particle);
    static bool __cdecl GetShowAllBoundingVolumes();
    static void __cdecl SetShowAllBoundingVolumes(bool show);
    static unsigned long __cdecl GetShowAllBoundingTestMask();
    static void __cdecl SetBoundingVolumesTestMask(unsigned long mask);
    bool __cdecl GetShowBoundingVolume() const;
    void __cdecl SetShowBoundingVolume(bool show);
    unsigned long __cdecl GetBoundingVolumeMask() const;
    void __cdecl SetBoundingVolumeMask(unsigned long mask);
    const Color& __cdecl GetBoundingVolumeColor() const;
    void __cdecl SetBoundingVolumeColor(const Color& color);
};
JET_LAYOUT32(sizeof(DebugRender) == 1);

// ---- LightLoader (empty): the two loader entry points ------------------------------------------------------------
class LightLoader {
public:
    LightLoader& operator=(const LightLoader& o);

    Light* __cdecl Load(const PString& base, const PString& path);
    bool __cdecl Save(const PString& base, const PString& path, Light* light);
};
JET_LAYOUT32(sizeof(LightLoader) == 1);

// ---- ViewPoint (0x70 bytes) --------------------------------------------------------------------------------------
// +0x00 RenderKernel*, +0x04 Camera*, +0x08 Array<VisibleElement,128>*, +0x0c next sibling of a child chain,
// +0x10 first child, +0x14 Portal*, +0x18 ViewPort const*, +0x1c Matrix4, +0x5c depth argument, +0x60 first
// visible index, +0x64 visible count, +0x68 portal depth, +0x6c pixel size at the near plane.
class ViewPoint {
public:
    ViewPoint(Array<VisibleElement, 128>* visibleList, RenderKernel* kernel, const ViewPort* viewPort);
    ViewPoint& operator=(const ViewPoint& o);
    ~ViewPoint();

    void __cdecl Add(Element* element, const Matrix4& matrix);
    void __cdecl Add(Element* element, const Matrix4& matrix, unsigned long key);
    void __cdecl AddPortal(Portal* portal, const Matrix4& matrix, unsigned int depth);
    static int __cdecl CalcWantedMipLevelsForCull(ViewPoint& view, Particle& particle, float f);
    const Camera* __cdecl GetCamera() const;
    const Portal* __cdecl GetPortal() const;
    RenderKernel* __cdecl GetRenderKernel() const;
    const ViewPort* __cdecl GetViewPort() const;
    void __cdecl RemoveAll();
    void __cdecl Render(ViewPort& port);
    void __cdecl Update(Camera* camera);

private:
    ViewPoint(ViewPoint& parent, Portal* portal, const Matrix4& matrix, unsigned int depth, ViewPoint* next);
    void __cdecl TraversePortal(Camera* camera);

public:
    RenderKernel* renderKernel;                  // +0x00
    Camera* camera;                              // +0x04
    Array<VisibleElement, 128>* visibleList;     // +0x08
    ViewPoint* next;                             // +0x0c
    ViewPoint* child;                            // +0x10
    Portal* portal;                              // +0x14
    const ViewPort* viewPort;                    // +0x18
    Matrix4 matrix;                              // +0x1c
    unsigned int depthArgument;                  // +0x5c
    unsigned int visibleFirst;                   // +0x60
    unsigned int visibleCount;                   // +0x64
    unsigned int depth;                          // +0x68
    float pixelSize;                             // +0x6c
};
JET_LAYOUT32(sizeof(ViewPoint) == 0x70);
JET_LAYOUT32(offsetof(ViewPoint, matrix) == 0x1c);
JET_LAYOUT32(offsetof(ViewPoint, pixelSize) == 0x6c);

}  // namespace Jet
