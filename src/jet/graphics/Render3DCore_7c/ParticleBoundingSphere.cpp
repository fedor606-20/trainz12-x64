// Unit U3 of Render3DCore_7c: Jet::ParticleBoundingSphere, the volume whose local shape is a Sphere at +0xa0,
// transcribed from the TS12 assembly (0x10050e00..0x10051d20).
//
// The collision methods look at the host particle's current bounding volume (as the original does) and the ray /
// segment distance helpers come from U1d1. IsWithinView and the renderable overrides live in ParticleBoundingView.cpp
// (unit U3b, after U8/U10 became local).
#include <jet/Render3DCoreBounds.h>
#include <jet/Render3DCoreLines.h>

#include <cmath>
#include <new>

namespace Jet {

namespace {

using BoundsDetail::AddSS;
using BoundsDetail::CopyX87;
using BoundsDetail::DivSS;
using BoundsDetail::FirstChild;
using BoundsDetail::LocalToGlobal;
using BoundsDetail::MulSS;
using BoundsDetail::NextSibling;
using BoundsDetail::Orientation;
using BoundsDetail::Position;
using BoundsDetail::RotateByQuaternion;
using BoundsDetail::SubSS;
using BoundsDetail::Volume;

// the original compares `distanceSquared - radius*radius < 0.0001` in x87, where 0.0001 is the float at 0x1007d27c
bool CloserThanTolerance(float distanceSquared, float radius)
{
    bool result;
    const float tolerance = 9.9999997e-05f;
    __asm {
        fld dword ptr [distanceSquared]
        fld dword ptr [radius]
        fmul st, st(0)
        fsubp st(1), st
        fld dword ptr [tolerance]
        fcomip st, st(1)
        fstp st(0)
        seta result
    }
    return result;
}

constexpr float kTolerance = 9.9999997e-05f;  // 0x38d1b717

// 0x100518d0's rotation: (centre - host position) by the inverse of the host orientation. The grouping is the
// assembly's (vx,vy,vz term order per output); RotateByQuaternion groups them differently by one ULP.
Vector3 InverseRotate(const Vector3& v, const Quaternion& q)
{
    const float sum = AddSS(AddSS(AddSS(MulSS(q.w, q.w), MulSS(q.z, q.z)), MulSS(q.y, q.y)),
                            MulSS(q.x, q.x));
    const float inv = DivSS(1.0f, sum);
    const float ix = MulSS(inv, SubSS(-0.0f, q.x));
    const float iy = MulSS(inv, SubSS(-0.0f, q.y));
    const float iz = MulSS(inv, SubSS(-0.0f, q.z));
    const float iw = MulSS(inv, q.w);

    const float x2 = MulSS(ix, 2.0f);
    const float y2 = MulSS(iy, 2.0f);
    const float z2 = MulSS(iz, 2.0f);
    const float xx = MulSS(x2, ix);
    const float yy = MulSS(y2, iy);
    const float zz = MulSS(z2, iz);
    const float xy = MulSS(x2, iy);
    const float xz = MulSS(x2, iz);
    const float yz = MulSS(y2, iz);
    const float wx = MulSS(x2, iw);
    const float wy = MulSS(y2, iw);
    const float wz = MulSS(z2, iw);

    Vector3 out;
    out.x = AddSS(AddSS(MulSS(SubSS(1.0f, AddSS(yy, zz)), v.x), MulSS(SubSS(xy, wz), v.y)),
                  MulSS(AddSS(xz, wy), v.z));
    out.y = AddSS(AddSS(MulSS(SubSS(1.0f, AddSS(xx, zz)), v.y), MulSS(SubSS(yz, wx), v.z)),
                  MulSS(AddSS(xy, wz), v.x));
    out.z = AddSS(AddSS(MulSS(SubSS(1.0f, AddSS(xx, yy)), v.z), MulSS(AddSS(wx, yz), v.y)),
                  MulSS(SubSS(xz, wy), v.x));
    return out;
}

}  // namespace

// ==== 0x10051350 ParticleBoundingSphere::ParticleBoundingSphere(Particle*)
ParticleBoundingSphere::ParticleBoundingSphere(Particle* host_)
    : ParticleBoundingVolume(host_), localSphere()
{
    localSphere.center = Vector3{0.0f, 0.0f, 0.0f};
    localSphere.radius = 0.0f;
    primaryVolumeType = PRIMARY_SPHERE;
}

// ==== 0x10050e00 ParticleBoundingSphere::ParticleBoundingSphere(const ParticleBoundingSphere&)
ParticleBoundingSphere::ParticleBoundingSphere(const ParticleBoundingSphere& o)
    : ParticleBoundingVolume(o), localSphere()
{
    localSphere.center = o.localSphere.center;  // plain moves; radius through x87
    CopyX87(localSphere.radius, o.localSphere.radius);
}

// ==== 0x10051f60 ParticleBoundingSphere::~ParticleBoundingSphere()
ParticleBoundingSphere::~ParticleBoundingSphere()
{
}

// ==== 0x10050e60 ParticleBoundingSphere::operator=
ParticleBoundingSphere& ParticleBoundingSphere::operator=(const ParticleBoundingSphere& o)
{
    ParticleBoundingVolume::operator=(o);
    localSphere = o.localSphere;
    return *this;
}

// ==== 0x10050f10 ParticleBoundingSphere::CreateInstance(Particle*) const
ParticleBoundingVolume* __cdecl ParticleBoundingSphere::CreateInstance(Particle* host_) const
{
    void* const memory = Alloc::New(sizeof(ParticleBoundingSphere));
    if (!memory) return nullptr;
    ParticleBoundingSphere* const instance = ::new (memory) ParticleBoundingSphere(host_);
    instance->dirtyFlags = 0xfffff;
    instance->localSphere = localSphere;
    return instance;
}

// ==== 0x10050fc0 ParticleBoundingSphere::SetLocalVolume(const OrientedBox&)
void __cdecl ParticleBoundingSphere::SetLocalVolume(const OrientedBox& box)
{
    Sphere value;
    value.center = box.center;
    value.radius = static_cast<float>(SolidsDetail::Length(box.extents));
    SetLocalVolume(value);
}

// ==== 0x10051020 ParticleBoundingSphere::SetLocalVolume(const AxisAlignedBox&)
void __cdecl ParticleBoundingSphere::SetLocalVolume(const AxisAlignedBox& box)
{
    Vector3 delta{SubSS(box.max.x, box.min.x), SubSS(box.max.y, box.min.y), SubSS(box.max.z, box.min.z)};
    Sphere value;
    value.center = box.GetCenter();
    value.radius = static_cast<float>(SolidsDetail::Length(delta) * 0.5);
    SetLocalVolume(value);
}

// ==== 0x100513a0 ParticleBoundingSphere::SetLocalVolume(const Sphere&)
void __cdecl ParticleBoundingSphere::SetLocalVolume(const Sphere& sphere_)
{
    dirtyFlags |= 0xf;
    for (Particle* child = FirstChild(host); child; child = NextSibling(child))
        Volume(child)->dirtyFlags |= 0xf;
    CopyX87(localSphere.center.x, sphere_.center.x);
    CopyX87(localSphere.center.y, sphere_.center.y);
    CopyX87(localSphere.center.z, sphere_.center.z);
    CopyX87(localSphere.radius, sphere_.radius);
}

// ==== 0x100510f0 ParticleBoundingSphere::GetLocalVolume(OrientedBox&) const
void __cdecl ParticleBoundingSphere::GetLocalVolume(OrientedBox& box) const
{
    AxisAlignedBox aligned;
    GetLocalVolume(aligned);
    box = OrientedBox(aligned);
}

// ==== 0x10051130 ParticleBoundingSphere::GetLocalVolume(AxisAlignedBox&) const
void __cdecl ParticleBoundingSphere::GetLocalVolume(AxisAlignedBox& box) const
{
    const float radius = localSphere.radius;
    const Vector3& center = localSphere.center;
    box.min.x = SubSS(center.x, radius);
    box.min.y = SubSS(center.y, radius);
    box.min.z = SubSS(center.z, radius);
    box.max.x = AddSS(center.x, radius);
    box.max.y = AddSS(center.y, radius);
    box.max.z = AddSS(center.z, radius);
}

// ==== 0x100511e0 ParticleBoundingSphere::GetLocalVolume(Sphere&) const
void __cdecl ParticleBoundingSphere::GetLocalVolume(Sphere& sphere_) const
{
    CopyX87(sphere_.center.x, localSphere.center.x);
    CopyX87(sphere_.center.y, localSphere.center.y);
    CopyX87(sphere_.center.z, localSphere.center.z);
    CopyX87(sphere_.radius, localSphere.radius);
}

// ==== 0x10051440 ParticleBoundingSphere::TestCollision(const Ray&, bool&) const
bool __cdecl ParticleBoundingSphere::TestCollision(const Ray& ray, bool& hit) const
{
    hit = false;
    Particle* const owner = host;
    ParticleBoundingVolume* volume = Volume(owner);
    if (volume->dirtyFlags & 2) volume->RecalculateSphereVolume();

    Sphere sphere_;
    sphere_.center = volume->sphere.center;
    sphere_.radius = volume->sphere.radius;

    volume = Volume(owner);
    if (volume->dirtyFlags & 3) volume->RecalculateSphereGlobalCenter();
    sphere_.center = volume->globalSphereCenter;

    if (!CloserThanTolerance(ray.DistanceSquared(sphere_.center), sphere_.radius)) return false;

    const float ownRadius = localSphere.radius;
    if (0.0f < ownRadius) {
        if (ownRadius < sphere_.radius) {
            Sphere local;
            local.center = localSphere.center;
            local.radius = localSphere.radius;
            LocalToGlobal(owner, local.center);
            if (!local.TestCollision(ray)) return true;
        }
        hit = true;
    }
    return true;
}

// ==== 0x100515b0 ParticleBoundingSphere::TestCollision(const LineSegment&, bool&) const
bool __cdecl ParticleBoundingSphere::TestCollision(const LineSegment& segment, bool& hit) const
{
    hit = false;
    Particle* const owner = host;
    ParticleBoundingVolume* volume = Volume(owner);
    if (volume->dirtyFlags & 2) volume->RecalculateSphereVolume();

    Sphere sphere_;
    sphere_.center = volume->sphere.center;
    sphere_.radius = volume->sphere.radius;

    volume = Volume(owner);
    if (volume->dirtyFlags & 3) volume->RecalculateSphereGlobalCenter();
    sphere_.center = volume->globalSphereCenter;

    if (!CloserThanTolerance(segment.DistanceSquared(sphere_.center), sphere_.radius)) return false;

    const float ownRadius = localSphere.radius;
    if (0.0f < ownRadius) {
        if (ownRadius < sphere_.radius) {
            Sphere local;
            local.center = localSphere.center;
            local.radius = localSphere.radius;
            LocalToGlobal(owner, local.center);
            if (!local.TestCollision(segment)) return true;
        }
        hit = true;
    }
    return true;
}

// ==== 0x10051720 ParticleBoundingSphere::RecalculateSphereVolume() const
void __cdecl ParticleBoundingSphere::RecalculateSphereVolume() const
{
    ParticleBoundingSphere* const self = const_cast<ParticleBoundingSphere*>(this);
    GetChildBoundingSphere(self->sphere);

    const float ownRadius = localSphere.radius;
    if (0.0f < ownRadius) {
        Particle* const owner = host;
        float rotated[3];
        RotateByQuaternion(rotated, &localSphere.center.x, &Orientation(owner).x);
        const Vector3& position = Position(owner);
        Sphere value;
        value.center.x = AddSS(position.x, rotated[0]);
        value.center.y = AddSS(position.y, rotated[1]);
        value.center.z = AddSS(position.z, rotated[2]);
        value.radius = ownRadius;

        if (0.0f < sphere.radius)
            self->sphere.Maximize(value);
        else
            self->sphere = value;  // plain moves in the original
    }
    self->dirtyFlags &= ~2u;
}

// ==== 0x10051810 ParticleBoundingSphere::RecalculateAxisAlignedBoxVolume() const
void __cdecl ParticleBoundingSphere::RecalculateAxisAlignedBoxVolume() const
{
    ParticleBoundingSphere* const self = const_cast<ParticleBoundingSphere*>(this);
    if (dirtyFlags & 2) RecalculateSphereVolume();
    if (dirtyFlags & 3) RecalculateSphereGlobalCenter();

    const float radius = sphere.radius;
    self->axisAlignedBox.min.x = SubSS(globalSphereCenter.x, radius);
    self->axisAlignedBox.min.y = SubSS(globalSphereCenter.y, radius);
    self->axisAlignedBox.min.z = SubSS(globalSphereCenter.z, radius);
    self->axisAlignedBox.max.x = AddSS(globalSphereCenter.x, radius);
    self->axisAlignedBox.max.y = AddSS(globalSphereCenter.y, radius);
    self->axisAlignedBox.max.z = AddSS(globalSphereCenter.z, radius);
    self->dirtyFlags &= ~8u;
}

// ==== 0x100518d0 ParticleBoundingSphere::RecalculateOrientedBoxVolume() const
void __cdecl ParticleBoundingSphere::RecalculateOrientedBoxVolume() const
{
    ParticleBoundingSphere* const self = const_cast<ParticleBoundingSphere*>(this);
    if (dirtyFlags & 2) RecalculateSphereVolume();

    Particle* const owner = host;
    const Vector3& position = Position(owner);
    Vector3 delta{SubSS(sphere.center.x, position.x), SubSS(sphere.center.y, position.y),
                  SubSS(sphere.center.z, position.z)};
    const Vector3 rotated = InverseRotate(delta, Orientation(owner));

    const float radius = sphere.radius;
    AxisAlignedBox aligned;
    aligned.min = Vector3{SubSS(rotated.x, radius), SubSS(rotated.y, radius), SubSS(rotated.z, radius)};
    aligned.max = Vector3{AddSS(rotated.x, radius), AddSS(rotated.y, radius), AddSS(rotated.z, radius)};
    self->orientedBox = OrientedBox(aligned);
    self->dirtyFlags &= ~4u;
}

// ==== 0x10051bf0 ParticleBoundingSphere::InitInstance(ParticleBoundingSphere&) const
void __cdecl ParticleBoundingSphere::InitInstance(ParticleBoundingSphere& instance) const
{
    instance.dirtyFlags = 0xfffff;
    instance.localSphere = localSphere;
}

}  // namespace Jet
