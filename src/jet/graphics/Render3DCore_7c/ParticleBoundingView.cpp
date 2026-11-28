// Unit U3b of Render3DCore_7c: the twelve forwarded ParticleBoundingVolume methods U3 left to the original, now that
// U8 (Camera/ViewPoint) and U10 (RenderKernel/RenderBox/RenderSphere) are local:
//   ParticleAxisAlignedBoundingBox / ParticleBoundingSphere / ParticleOrientedBoundingBox
//     IsWithinView(const ViewPoint&), IsWithinView(const Camera*),
//     CreateRenderableObject(RenderKernel*, const Color&), CheckRenderableObjectPosition(RenderKernel*,
//     Render3DPrimitive*)
// Behaviour and addresses follow the TS12 assembly (0x100505f0, 0x10050650, 0x10050af0, 0x10050c00, 0x100513f0,
// 0x10051c30, 0x10051d20, 0x10053210, 0x100535f0, 0x10053700; notes/agents/r3d_workstreams/u3_bounds_decomp.c and
// u3_bounds_asm.txt).
//
// The OBB class shares the Sphere bodies in the original (the ViewPoint/Camera IsWithinView exports have the same RVA);
// its copies below keep the values identical, including the virtual RecalculateSphereVolume dispatch.
//
// ViewPoint is still a U9 class: only its camera pointer at +0x04 is read. The AABB frustum comes from the local
// Camera::GetFrustum() (which runs UpdateViewCone when the camera's +0x25e flag is set); the sphere/OBB radius test
// calls the original Camera::Clip3D until U8's local copy reproduces its unordered-comparison path (see Clip3D below).
//
// The renderable methods call RenderPrimitive::Init and RenderSphere::BuildGeometry through the original module (they
// are still forwarded by U10), and the protected virtual CalculatePrimitiveVolume through the vtable slot the original
// calls. Shape creation goes through Alloc::New and the local U10 constructors.
#include <jet/Render3DCoreBounds.h>
#include <jet/Render3DCoreCamera.h>
#include <jet/Render3DCorePrimitives.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

using BoundsDetail::MulSS;
using BoundsDetail::SubSS;

// The camera pointer is the only ViewPoint field the original reads here (its vftable is never used in these paths).
const Camera* ViewCamera(const ViewPoint& view)
{
    return *reinterpret_cast<const Camera* const*>(reinterpret_cast<const char*>(&view) + 4);
}

// U8's Camera::Clip3D is local and is the reference; call it directly.
ClipResult Clip3D(const Camera* camera, const Vector3& center, float radius)
{
    return camera->Clip3D(center, radius);
}

// The original's AABB CreateRenderableObject computes the sizes with x87 (`fld`/`fsub`), not SSE. Finite results are
// the same; the double expression keeps the original NaN handling through the x87 unit.
float X87Subtract(float a, float b)
{
    const double result = static_cast<double>(a) - static_cast<double>(b);
    return static_cast<float>(result);
}

void DestroyPrimitive(Render3DPrimitive* primitive)
{
    if (!primitive) return;
    using Fn = void(__thiscall*)(Render3DPrimitive*, int);
    reinterpret_cast<Fn>((*reinterpret_cast<void***>(primitive))[0])(primitive, 1);
}

// RenderPrimitive::Init and RenderSphere::BuildGeometry are U10's forwarded exports; the original calls the first
// directly and the second through the shape's protected member. The accessor keeps the calls non-virtual like the
// original's (the scalar `Init` in RenderSphere is a different, public overload).
struct SphereAccess : RenderSphere {
    bool InitPrimitive(RenderKernel* kernel, const Color& color, int creationFlags)
    {
        return RenderPrimitive::Init(kernel, color, creationFlags);
    }
    bool Build(RenderKernel* kernel, const Color& color, int segments, int creationFlags)
    {
        return BuildGeometry(kernel, color, segments, creationFlags);
    }
    void CalculateVolume()
    {
        CalculatePrimitiveVolume();
    }
    void SetRadius(float value)
    {
        radius = value;
    }
};

// The original's CreateRenderableObject/CheckRenderableObjectPosition only ever see RenderBox/RenderSphere objects and
// treat the primitive as that type (they read its extents/radius at +0x188).
RenderBox* AsBox(Render3DPrimitive* primitive)
{
    return reinterpret_cast<RenderBox*>(primitive);
}

RenderSphere* AsSphere(Render3DPrimitive* primitive)
{
    return reinterpret_cast<RenderSphere*>(primitive);
}

void ClearDebugFlag(RenderPrimitive* primitive)
{
    primitive->SetDebugRender(false);
}

}  // namespace

// ============================================================== ParticleAxisAlignedBoundingBox

// ==== 0x100505f0 ParticleAxisAlignedBoundingBox::IsWithinView(const ViewPoint&) const
bool __cdecl ParticleAxisAlignedBoundingBox::IsWithinView(const ViewPoint& view) const
{
    const Camera* const camera = ViewCamera(view);
    if (!camera) return false;
    if (dirtyFlags & 8) RecalculateAxisAlignedBoxVolume();
    return axisAlignedBox.IsWithin(camera->GetFrustum());
}

// ==== 0x10050650 ParticleAxisAlignedBoundingBox::IsWithinView(const Camera*) const
bool __cdecl ParticleAxisAlignedBoundingBox::IsWithinView(const Camera* camera) const
{
    if (dirtyFlags & 8) RecalculateAxisAlignedBoxVolume();
    return axisAlignedBox.IsWithin(camera->GetFrustum());
}

// ==== 0x10050af0 ParticleAxisAlignedBoundingBox::CreateRenderableObject
Render3DPrimitive* __cdecl ParticleAxisAlignedBoundingBox::CreateRenderableObject(RenderKernel* kernel,
                                                                                  const Color& color) const
{
    void* const memory = Alloc::New(sizeof(RenderBox));
    RenderBox* const box = ::new (memory) RenderBox();
    if (dirtyFlags & 8) RecalculateAxisAlignedBoxVolume();
    const float width = X87Subtract(axisAlignedBox.max.x, axisAlignedBox.min.x);
    const float height = X87Subtract(axisAlignedBox.max.y, axisAlignedBox.min.y);
    const float depth = X87Subtract(axisAlignedBox.max.z, axisAlignedBox.min.z);
    box->Init(kernel, color, width, height, depth, 1u, 1u, 1u, 4);
    ClearDebugFlag(box);
    return CheckRenderableObjectPosition(kernel, box);
}

// ==== 0x10050c00 ParticleAxisAlignedBoundingBox::CheckRenderableObjectPosition
Render3DPrimitive* __cdecl ParticleAxisAlignedBoundingBox::CheckRenderableObjectPosition(
    RenderKernel* kernel, Render3DPrimitive* primitive) const
{
    if (dirtyFlags & 8) RecalculateAxisAlignedBoxVolume();
    const float width = SubSS(axisAlignedBox.max.x, axisAlignedBox.min.x);
    const float height = SubSS(axisAlignedBox.max.y, axisAlignedBox.min.y);
    const float depth = SubSS(axisAlignedBox.max.z, axisAlignedBox.min.z);
    RenderBox* box = AsBox(primitive);
    if (MulSS(width, 0.5f) != box->GetExtents().x || MulSS(height, 0.5f) != box->GetExtents().y ||
        MulSS(depth, 0.5f) != box->GetExtents().z) {
        const Color oldColor = box->GetColor();
        DestroyPrimitive(box);
        box = ::new (Alloc::New(sizeof(RenderBox))) RenderBox();
        box->Init(kernel, oldColor, width, height, depth, 1u, 1u, 1u, 4);
        ClearDebugFlag(box);
    }
    const Vector3 center = axisAlignedBox.GetCenter();
    box->SetPosition(center);
    return box;
}

// ============================================================== ParticleBoundingSphere

// ==== 0x10053210 ParticleBoundingSphere::IsWithinView(const ViewPoint&) const (also the OBB's export)
bool __cdecl ParticleBoundingSphere::IsWithinView(const ViewPoint& view) const
{
    const Camera* const camera = ViewCamera(view);
    if (!camera) return false;
    if (dirtyFlags & 3) RecalculateSphereGlobalCenter();
    if (dirtyFlags & 2) RecalculateSphereVolume();
    return Clip3D(camera, globalSphereCenter, sphere.radius) != BACK;
}

// ==== 0x100513f0 ParticleBoundingSphere::IsWithinView(const Camera*) const (also the OBB's export)
bool __cdecl ParticleBoundingSphere::IsWithinView(const Camera* camera) const
{
    if (dirtyFlags & 3) RecalculateSphereGlobalCenter();
    if (dirtyFlags & 2) RecalculateSphereVolume();
    return Clip3D(camera, globalSphereCenter, sphere.radius) != BACK;
}

// ==== 0x10051c30 ParticleBoundingSphere::CreateRenderableObject
Render3DPrimitive* __cdecl ParticleBoundingSphere::CreateRenderableObject(RenderKernel* kernel,
                                                                          const Color& color) const
{
    RenderSphere* const sphereObject = ::new (Alloc::New(sizeof(RenderSphere))) RenderSphere();
    SphereAccess* const access = static_cast<SphereAccess*>(sphereObject);
    if (dirtyFlags & 2) RecalculateSphereVolume();
    access->SetRadius(sphere.radius);
    if (access->InitPrimitive(kernel, color, 4)) {
        if (access->Build(kernel, color, 0x10, 4)) access->CalculateVolume();
    }
    ClearDebugFlag(sphereObject);
    return CheckRenderableObjectPosition(kernel, sphereObject);
}

// ==== 0x10051d20 ParticleBoundingSphere::CheckRenderableObjectPosition
Render3DPrimitive* __cdecl ParticleBoundingSphere::CheckRenderableObjectPosition(
    RenderKernel* kernel, Render3DPrimitive* primitive) const
{
    if (dirtyFlags & 2) RecalculateSphereVolume();
    const float radius = sphere.radius;
    RenderSphere* sphereObject = AsSphere(primitive);
    if (radius != sphereObject->GetRadius()) {
        const Color oldColor = sphereObject->GetColor();
        DestroyPrimitive(sphereObject);
        sphereObject = ::new (Alloc::New(sizeof(RenderSphere))) RenderSphere();
        sphereObject->Init(kernel, oldColor, radius, 0x10, 4);
        ClearDebugFlag(sphereObject);
    }
    if (dirtyFlags & 3) RecalculateSphereGlobalCenter();
    sphereObject->SetPosition(globalSphereCenter);
    return sphereObject;
}

// ============================================================== ParticleOrientedBoundingBox

// ==== 0x10053210 ParticleOrientedBoundingBox::IsWithinView(const ViewPoint&) const (the original aliases the Sphere)
bool __cdecl ParticleOrientedBoundingBox::IsWithinView(const ViewPoint& view) const
{
    const Camera* const camera = ViewCamera(view);
    if (!camera) return false;
    if (dirtyFlags & 3) RecalculateSphereGlobalCenter();
    if (dirtyFlags & 2) RecalculateSphereVolume();
    return Clip3D(camera, globalSphereCenter, sphere.radius) != BACK;
}

// ==== 0x100513f0 ParticleOrientedBoundingBox::IsWithinView(const Camera*) const
bool __cdecl ParticleOrientedBoundingBox::IsWithinView(const Camera* camera) const
{
    if (dirtyFlags & 3) RecalculateSphereGlobalCenter();
    if (dirtyFlags & 2) RecalculateSphereVolume();
    return Clip3D(camera, globalSphereCenter, sphere.radius) != BACK;
}

// ==== 0x100535f0 ParticleOrientedBoundingBox::CreateRenderableObject
Render3DPrimitive* __cdecl ParticleOrientedBoundingBox::CreateRenderableObject(RenderKernel* kernel,
                                                                               const Color& color) const
{
    RenderBox* const box = ::new (Alloc::New(sizeof(RenderBox))) RenderBox();
    if (dirtyFlags & 4) RecalculateOrientedBoxVolume();
    box->Init(kernel, color, MulSS(orientedBox.extents.x, 2.0f), MulSS(orientedBox.extents.y, 2.0f),
              MulSS(orientedBox.extents.z, 2.0f), 1u, 1u, 1u, 4);
    ClearDebugFlag(box);
    return CheckRenderableObjectPosition(kernel, box);
}

// ==== 0x10053700 ParticleOrientedBoundingBox::CheckRenderableObjectPosition
Render3DPrimitive* __cdecl ParticleOrientedBoundingBox::CheckRenderableObjectPosition(
    RenderKernel* kernel, Render3DPrimitive* primitive) const
{
    if (dirtyFlags & 4) RecalculateOrientedBoxVolume();
    const float width = orientedBox.extents.x;
    const float height = orientedBox.extents.y;
    const float depth = orientedBox.extents.z;
    RenderBox* box = AsBox(primitive);
    if (width != box->GetExtents().x || height != box->GetExtents().y || depth != box->GetExtents().z) {
        const Color oldColor = box->GetColor();
        DestroyPrimitive(box);
        box = ::new (Alloc::New(sizeof(RenderBox))) RenderBox();
        box->Init(kernel, oldColor, MulSS(width, 2.0f), MulSS(height, 2.0f), MulSS(depth, 2.0f), 1u, 1u, 1u, 4);
        ClearDebugFlag(box);
    }
    if (dirtyFlags & 3) RecalculateSphereGlobalCenter();
    Matrix3 axes;
    std::memcpy(axes.m, &orientedBox.axis[0].x, sizeof axes.m);
    Quaternion rotation;
    rotation.Set(axes);
    box->SetPosition(globalSphereCenter);
    if (dirtyFlags & 0x20) RecalculateGlobalOrientation();
    Quaternion orientation;
    BoundsDetail::MultiplyQuaternions(&orientation.x, &rotation.x, &globalOrientation.x);
    box->SetOrientation(orientation);
    return box;
}

}  // namespace Jet
