// Render3DCore_7c unit U3: the particle bounding volumes. Layouts, virtual slot order and behaviour follow the TS12
// assembly (addresses per member in graphics/Render3DCore_7c/ParticleBounding*.cpp and the decompiled listing
// notes/agents/r3d_workstreams/u3_bounds_decomp.c).
//
// ParticleBoundingVolume is 0xa0 bytes:
//   +0x00 vftable, +0x04 dirty flags, +0x08 embedded Sphere {vftable, centre +0xc, radius +0x18},
//   +0x1c global sphere centre, +0x28 embedded AxisAlignedBox, +0x40 embedded OrientedBox,
//   +0x7c global position, +0x88 global orientation, +0x98 host Particle*, +0x9c primary volume type.
// ParticleAxisAlignedBoundingBox / ParticleOrientedBoundingBox / ParticleVisibleBoundingBox add an OrientedBox at
// +0xa0 (0xdc bytes); ParticleBoundingSphere adds a Sphere at +0xa0 (0xb4 bytes).
//
// Ordinary members are __cdecl (this is on the stack, mangled QAA/IBA), constructors and assignments are __thiscall
// (QAE); the virtuals are __cdecl (UAA/UBA). MSVC emits same-name virtual overloads in REVERSE declaration order, so
// the overload groups below are written backwards to produce the original's slots:
//   1 IsWithinView(Camera*)   2 IsWithinView(ViewPoint&)   3 TestCollision(LineSegment)  4 TestCollision(Ray)
//   5 OnChildAdded            6 OnChildRemoved             7 SetLocalVolume(Sphere)       8 SetLocalVolume(AABB)
//   9 SetLocalVolume(OBB)    10 GetLocalVolume(Sphere)    11 GetLocalVolume(AABB)       12 GetLocalVolume(OBB)
//  13 CreateInstance         14 CreateRenderableObject    15 CheckRenderableObjectPosition
//  16 RecalculateSphereVolume 17 RecalculateAxisAlignedBoxVolume 18 RecalculateOrientedBoxVolume
//
// Methods that need Camera/ViewPoint or the U10 render primitives keep a local override only so the vtable is right;
// the body calls the original export by name (those exports themselves stay forwarded in implemented.d).
#pragma once

#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/Render3DCore.h>
#include <jet/Render3DCoreOrientedBox.h>
#include <jet/Render3DCoreSolids.h>

namespace Jet {

class Camera;
class Color;
class Particle;
class ParticleBoundingVolume;
class Render3DPrimitive;
class RenderKernel;
class ViewPoint;

// Shared float helpers used by more than one class file. They reproduce the original's SSE operand order and the
// x87 (0x1004be00 / 0x1004fe60) quaternion maths of the Particle unit.
namespace BoundsDetail {
float __cdecl AddSS(float a, float b);
float __cdecl SubSS(float a, float b);
float __cdecl MulSS(float a, float b);
float __cdecl DivSS(float a, float b);
void __cdecl RotateByQuaternion(float* out, const float* v, const float* q);
void __cdecl MultiplyQuaternions(float* out, const float* a, const float* b);
void __cdecl QuaternionToMatrix3(float* m, const float* q);
void __cdecl CopyX87(float& dst, const float& src);

// The Particle scene node is U9a; the helpers keep its header out of the class files that also need the line
// classes (both define ClipResult in the SDK-shaped headers). Layout and behaviour are the U9a ones.
Particle* __cdecl Parent(Particle* particle);
Particle* __cdecl FirstChild(Particle* particle);
Particle* __cdecl NextSibling(Particle* particle);
ParticleBoundingVolume* __cdecl Volume(Particle* particle);
const Vector3& __cdecl Position(Particle* particle);
const Quaternion& __cdecl Orientation(Particle* particle);
const Vector3& __cdecl GlobalPosition(Particle* particle);
const Quaternion& __cdecl GlobalOrientation(Particle* particle);
void __cdecl LocalToGlobal(Particle* particle, Vector3& value);
void __cdecl LocalToGlobal(Particle* particle, OrientedBox& box);
void __cdecl GlobalToLocal(Particle* particle, Vector3& value);
void __cdecl GlobalToLocal(Particle* particle, OrientedBox& box);
}  // namespace BoundsDetail

class ParticleBoundingVolume {
public:
    enum PrimaryVolumeType {
        PRIMARY_SPHERE = 0,
        PRIMARY_AXIS_ALIGNED_BOX = 1,
        PRIMARY_ORIENTED_BOX = 2,
        PRIMARY_BOUNDING_VOLUME = 3,
    };

    ParticleBoundingVolume(Particle* host);
    ParticleBoundingVolume(const ParticleBoundingVolume& o);
    virtual ~ParticleBoundingVolume();
    ParticleBoundingVolume& operator=(const ParticleBoundingVolume& o);

    // virtual slots 2 and 1 (reverse declaration order)
    virtual bool __cdecl IsWithinView(const ViewPoint& view) const;
    virtual bool __cdecl IsWithinView(const Camera* camera) const;
    // slots 4 and 3
    virtual bool __cdecl TestCollision(const Ray& ray, bool& hit) const;
    virtual bool __cdecl TestCollision(const LineSegment& segment, bool& hit) const;
    // slots 5 and 6
    virtual void __cdecl OnChildAdded(Particle* child);
    virtual void __cdecl OnChildRemoved(Particle* child);
    // slots 9, 8, 7
    virtual void __cdecl SetLocalVolume(const OrientedBox& box);
    virtual void __cdecl SetLocalVolume(const AxisAlignedBox& box);
    virtual void __cdecl SetLocalVolume(const Sphere& sphere);
    // slots 12, 11, 10
    virtual void __cdecl GetLocalVolume(OrientedBox& box) const;
    virtual void __cdecl GetLocalVolume(AxisAlignedBox& box) const;
    virtual void __cdecl GetLocalVolume(Sphere& sphere) const;
    // slots 13, 14, 15
    virtual ParticleBoundingVolume* __cdecl CreateInstance(Particle* host) const;
    virtual Render3DPrimitive* __cdecl CreateRenderableObject(RenderKernel* kernel, const Color& color) const;
    virtual Render3DPrimitive* __cdecl CheckRenderableObjectPosition(RenderKernel* kernel,
                                                                   Render3DPrimitive* primitive) const;
    // slots 16, 17, 18
    virtual void __cdecl RecalculateSphereVolume() const;
    virtual void __cdecl RecalculateAxisAlignedBoxVolume() const;
    virtual void __cdecl RecalculateOrientedBoxVolume() const;

    Particle* __cdecl GetHost();
    const Particle* __cdecl GetHost() const;
    bool __cdecl IsFlagDirty(unsigned long flag) const;
    unsigned long __cdecl GetDirtyFlags() const;
    void __cdecl SetDirtyFlags(unsigned long flags);
    void __cdecl SetParentDirtyFlags(unsigned long flags);
    void __cdecl SetChildDirtyFlags(unsigned long flags);
    void __cdecl SetPositionChanged(const Vector3& oldPosition, const Vector3& newPosition);
    void __cdecl SetOrientationChanged(const Quaternion& oldOrientation, const Quaternion& newOrientation);
    void __cdecl SetPositionOrientationChanged(const Vector3& oldPosition, const Vector3& newPosition,
                                               const Quaternion& oldOrientation,
                                               const Quaternion& newOrientation);

    const Sphere& __cdecl GetBoundingSphere(bool recalculate) const;
    const Vector3& __cdecl GetGlobalBoundingSphereCenter(bool recalculate) const;
    const OrientedBox& __cdecl GetOrientedBoundingBox(bool recalculate) const;
    const AxisAlignedBox& __cdecl GetAxisAlignedBoundingBox(bool recalculate) const;
    const Vector3& __cdecl GetGlobalPosition() const;
    const Quaternion& __cdecl GetGlobalOrientation() const;
    PrimaryVolumeType __cdecl GetPrimaryVolumeType() const;

protected:
    void __cdecl SetRawDirtyFlags(bool set, unsigned long flags) const;
    void __cdecl SetRawDirtyFlags(unsigned long flags) const;
    void __cdecl GetChildBoundingSphere(Sphere& sphere) const;
    void __cdecl GetChildBoundingOrientedBox(OrientedBox& box) const;
    void __cdecl GetChildBoundingAxisAlignBox(AxisAlignedBox& box) const;
    void __cdecl RecalculateGlobalPosition() const;
    void __cdecl RecalculateGlobalOrientation() const;
    void __cdecl RecalculateSphereGlobalCenter() const;
    void __cdecl InitInstance(ParticleBoundingVolume& instance) const;

    friend class ParticleAxisAlignedBoundingBox;
    friend class ParticleBoundingSphere;
    friend class ParticleOrientedBoundingBox;
    friend class ParticleVisibleBoundingBox;
    friend struct BoundsAccess;

public:
    JET_HEAP_OBJECT

    unsigned long dirtyFlags;             // +0x04
    Sphere sphere;                        // +0x08
    Vector3 globalSphereCenter;           // +0x1c
    AxisAlignedBox axisAlignedBox;        // +0x28
    OrientedBox orientedBox;              // +0x40
    Vector3 globalPosition;               // +0x7c
    Quaternion globalOrientation;         // +0x88
    Particle* host;                       // +0x98
    PrimaryVolumeType primaryVolumeType;  // +0x9c
};
JET_LAYOUT32(sizeof(ParticleBoundingVolume) == 0xa0);

// The three global notifications are protected, because their decorated names must keep spelling them that way.
// The other units of this module call them through here instead of through the original module.
struct BoundsAccess
{
    static void RecalculateGlobalPosition(const ParticleBoundingVolume* volume)
    {
        volume->RecalculateGlobalPosition();
    }
    static void RecalculateGlobalOrientation(const ParticleBoundingVolume* volume)
    {
        volume->RecalculateGlobalOrientation();
    }
    static void RecalculateSphereGlobalCenter(const ParticleBoundingVolume* volume)
    {
        volume->RecalculateSphereGlobalCenter();
    }
};

class ParticleAxisAlignedBoundingBox : public ParticleBoundingVolume {
public:
    ParticleAxisAlignedBoundingBox(Particle* host);
    ParticleAxisAlignedBoundingBox(const ParticleAxisAlignedBoundingBox& o);
    virtual ~ParticleAxisAlignedBoundingBox();
    ParticleAxisAlignedBoundingBox& operator=(const ParticleAxisAlignedBoundingBox& o);

    virtual bool __cdecl IsWithinView(const ViewPoint& view) const;
    virtual bool __cdecl IsWithinView(const Camera* camera) const;
    virtual bool __cdecl TestCollision(const Ray& ray, bool& hit) const;
    virtual bool __cdecl TestCollision(const LineSegment& segment, bool& hit) const;
    virtual void __cdecl SetLocalVolume(const OrientedBox& box);
    virtual void __cdecl SetLocalVolume(const AxisAlignedBox& box);
    virtual void __cdecl SetLocalVolume(const Sphere& sphere);
    virtual void __cdecl GetLocalVolume(OrientedBox& box) const;
    virtual void __cdecl GetLocalVolume(AxisAlignedBox& box) const;
    virtual void __cdecl GetLocalVolume(Sphere& sphere) const;
    virtual ParticleBoundingVolume* __cdecl CreateInstance(Particle* host) const;
    virtual Render3DPrimitive* __cdecl CreateRenderableObject(RenderKernel* kernel, const Color& color) const;
    virtual Render3DPrimitive* __cdecl CheckRenderableObjectPosition(RenderKernel* kernel,
                                                                   Render3DPrimitive* primitive) const;

protected:
    virtual void __cdecl RecalculateSphereVolume() const;
    virtual void __cdecl RecalculateAxisAlignedBoxVolume() const;
    virtual void __cdecl RecalculateOrientedBoxVolume() const;
    void __cdecl InitInstance(ParticleAxisAlignedBoundingBox& instance) const;

public:
    OrientedBox localBox;  // +0xa0
};
JET_LAYOUT32(sizeof(ParticleAxisAlignedBoundingBox) == 0xdc);

class ParticleBoundingSphere : public ParticleBoundingVolume {
public:
    ParticleBoundingSphere(Particle* host);
    ParticleBoundingSphere(const ParticleBoundingSphere& o);
    virtual ~ParticleBoundingSphere();
    ParticleBoundingSphere& operator=(const ParticleBoundingSphere& o);

    virtual bool __cdecl IsWithinView(const ViewPoint& view) const;
    virtual bool __cdecl IsWithinView(const Camera* camera) const;
    virtual bool __cdecl TestCollision(const Ray& ray, bool& hit) const;
    virtual bool __cdecl TestCollision(const LineSegment& segment, bool& hit) const;
    virtual void __cdecl SetLocalVolume(const OrientedBox& box);
    virtual void __cdecl SetLocalVolume(const AxisAlignedBox& box);
    virtual void __cdecl SetLocalVolume(const Sphere& sphere);
    virtual void __cdecl GetLocalVolume(OrientedBox& box) const;
    virtual void __cdecl GetLocalVolume(AxisAlignedBox& box) const;
    virtual void __cdecl GetLocalVolume(Sphere& sphere) const;
    virtual ParticleBoundingVolume* __cdecl CreateInstance(Particle* host) const;
    virtual Render3DPrimitive* __cdecl CreateRenderableObject(RenderKernel* kernel, const Color& color) const;
    virtual Render3DPrimitive* __cdecl CheckRenderableObjectPosition(RenderKernel* kernel,
                                                                   Render3DPrimitive* primitive) const;

protected:
    virtual void __cdecl RecalculateSphereVolume() const;
    virtual void __cdecl RecalculateAxisAlignedBoxVolume() const;
    virtual void __cdecl RecalculateOrientedBoxVolume() const;
    void __cdecl InitInstance(ParticleBoundingSphere& instance) const;

public:
    Sphere localSphere;  // +0xa0
};
JET_LAYOUT32(sizeof(ParticleBoundingSphere) == 0xb4);

class ParticleOrientedBoundingBox : public ParticleAxisAlignedBoundingBox {
public:
    ParticleOrientedBoundingBox(Particle* host);
    ParticleOrientedBoundingBox(const ParticleOrientedBoundingBox& o);
    virtual ~ParticleOrientedBoundingBox();
    ParticleOrientedBoundingBox& operator=(const ParticleOrientedBoundingBox& o);

    virtual bool __cdecl IsWithinView(const ViewPoint& view) const;
    virtual bool __cdecl IsWithinView(const Camera* camera) const;
    virtual bool __cdecl TestCollision(const Ray& ray, bool& hit) const;
    virtual bool __cdecl TestCollision(const LineSegment& segment, bool& hit) const;
    virtual ParticleBoundingVolume* __cdecl CreateInstance(Particle* host) const;
    virtual Render3DPrimitive* __cdecl CreateRenderableObject(RenderKernel* kernel, const Color& color) const;
    virtual Render3DPrimitive* __cdecl CheckRenderableObjectPosition(RenderKernel* kernel,
                                                                   Render3DPrimitive* primitive) const;

protected:
    virtual void __cdecl RecalculateSphereVolume() const;
    virtual void __cdecl RecalculateAxisAlignedBoxVolume() const;
    virtual void __cdecl RecalculateOrientedBoxVolume() const;
    void __cdecl InitInstance(ParticleOrientedBoundingBox& instance) const;
};
JET_LAYOUT32(sizeof(ParticleOrientedBoundingBox) == 0xdc);

class ParticleVisibleBoundingBox : public ParticleAxisAlignedBoundingBox {
public:
    ParticleVisibleBoundingBox(Particle* host);
    ParticleVisibleBoundingBox(const ParticleVisibleBoundingBox& o);
    virtual ~ParticleVisibleBoundingBox();
    ParticleVisibleBoundingBox& operator=(const ParticleVisibleBoundingBox& o);

    virtual bool __cdecl IsWithinView(const ViewPoint& view) const;
    virtual bool __cdecl IsWithinView(const Camera* camera) const;
    virtual ParticleBoundingVolume* __cdecl CreateInstance(Particle* host) const;
};
JET_LAYOUT32(sizeof(ParticleVisibleBoundingBox) == 0xdc);

}  // namespace Jet
