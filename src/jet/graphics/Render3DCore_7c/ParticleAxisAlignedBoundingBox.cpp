// Unit U3 of Render3DCore_7c: Jet::ParticleAxisAlignedBoundingBox, the volume whose local shape is an
// AxisAlignedBox (stored as an OrientedBox at +0xa0), transcribed from the TS12 assembly
// (0x10006ab0..0x10006d10, 0x10050550..0x10050c00).
//
// IsWithinView and the renderable methods now live in ParticleBoundingView.cpp (unit U3b, after U8/U10 became local);
// this file keeps the shape, collision, local-volume and recalculation methods.
#include <jet/Render3DCoreBounds.h>
#include <jet/Render3DCoreLines.h>

#include <windows.h>

#include <cmath>
#include <new>

namespace Jet {

namespace {

using BoundsDetail::AddSS;
using BoundsDetail::CopyX87;
using BoundsDetail::DivSS;
using BoundsDetail::FirstChild;
using BoundsDetail::GlobalToLocal;
using BoundsDetail::LocalToGlobal;
using BoundsDetail::MulSS;
using BoundsDetail::NextSibling;
using BoundsDetail::Orientation;
using BoundsDetail::Parent;
using BoundsDetail::Position;
using BoundsDetail::RotateByQuaternion;
using BoundsDetail::SubSS;
using BoundsDetail::Volume;

bool ExtentsNonZero(const OrientedBox& box)
{
    return box.extents.x != 0.0f || box.extents.y != 0.0f || box.extents.z != 0.0f;  // NaN counts as non-zero
}

// U1c's OrientedBox owns the aligned-box conversion.
void BuildAlignedBox(AxisAlignedBox& out, const OrientedBox& box)
{
    out = AxisAlignedBox(box);
}

}  // namespace

// ==== 0x10050550 ParticleAxisAlignedBoundingBox::ParticleAxisAlignedBoundingBox(Particle*)
ParticleAxisAlignedBoundingBox::ParticleAxisAlignedBoundingBox(Particle* host_)
    : ParticleBoundingVolume(host_), localBox()
{
    localBox = OrientedBox(axisAlignedBox);
    primaryVolumeType = PRIMARY_AXIS_ALIGNED_BOX;
}

// ==== 0x10006ab0 ParticleAxisAlignedBoundingBox::ParticleAxisAlignedBoundingBox(const ParticleAxisAlignedBoundingBox&)
ParticleAxisAlignedBoundingBox::ParticleAxisAlignedBoundingBox(const ParticleAxisAlignedBoundingBox& o)
    : ParticleBoundingVolume(o), localBox(o.localBox)
{
}

// ==== 0x10051f60 ParticleAxisAlignedBoundingBox::~ParticleAxisAlignedBoundingBox()
ParticleAxisAlignedBoundingBox::~ParticleAxisAlignedBoundingBox()
{
}

// ==== 0x10006ae0 ParticleAxisAlignedBoundingBox::operator=
ParticleAxisAlignedBoundingBox& ParticleAxisAlignedBoundingBox::operator=(const ParticleAxisAlignedBoundingBox& o)
{
    ParticleBoundingVolume::operator=(o);
    localBox = o.localBox;
    return *this;
}

// ==== 0x10006b10 ParticleAxisAlignedBoundingBox::CreateInstance(Particle*) const
ParticleBoundingVolume* __cdecl ParticleAxisAlignedBoundingBox::CreateInstance(Particle* host_) const
{
    void* const memory = Alloc::New(sizeof(ParticleAxisAlignedBoundingBox));
    if (!memory) return nullptr;
    ParticleAxisAlignedBoundingBox* const instance =
        ::new (memory) ParticleAxisAlignedBoundingBox(host_);
    instance->dirtyFlags = 0xfffff;
    instance->localBox = localBox;
    return instance;
}

// ==== 0x100505b0 ParticleAxisAlignedBoundingBox::SetLocalVolume(const OrientedBox&)
void __cdecl ParticleAxisAlignedBoundingBox::SetLocalVolume(const OrientedBox& box)
{
    dirtyFlags |= 0xf;
    for (Particle* child = FirstChild(host); child; child = NextSibling(child))
        Volume(child)->dirtyFlags |= 0xf;
    localBox = box;
}

// ==== 0x10006bb0 ParticleAxisAlignedBoundingBox::SetLocalVolume(const AxisAlignedBox&)
void __cdecl ParticleAxisAlignedBoundingBox::SetLocalVolume(const AxisAlignedBox& box)
{
    const OrientedBox oriented(box);
    SetLocalVolume(oriented);
}

// ==== 0x10006be0 ParticleAxisAlignedBoundingBox::SetLocalVolume(const Sphere&)
void __cdecl ParticleAxisAlignedBoundingBox::SetLocalVolume(const Sphere& sphere_)
{
    AxisAlignedBox box;
    box.min.x = SubSS(sphere_.center.x, sphere_.radius);
    box.min.y = SubSS(sphere_.center.y, sphere_.radius);
    box.min.z = SubSS(sphere_.center.z, sphere_.radius);
    box.max.x = AddSS(sphere_.center.x, sphere_.radius);
    box.max.y = AddSS(sphere_.center.y, sphere_.radius);
    box.max.z = AddSS(sphere_.center.z, sphere_.radius);
    const OrientedBox oriented(box);
    SetLocalVolume(oriented);
}

// ==== 0x10006ca0 ParticleAxisAlignedBoundingBox::GetLocalVolume(OrientedBox&) const
void __cdecl ParticleAxisAlignedBoundingBox::GetLocalVolume(OrientedBox& box) const
{
    box = localBox;
}

// ==== 0x10006cc0 ParticleAxisAlignedBoundingBox::GetLocalVolume(AxisAlignedBox&) const
void __cdecl ParticleAxisAlignedBoundingBox::GetLocalVolume(AxisAlignedBox& box) const
{
    AxisAlignedBox oriented;
    BuildAlignedBox(oriented, localBox);
    CopyX87(box.min.x, oriented.min.x);
    CopyX87(box.min.y, oriented.min.y);
    CopyX87(box.min.z, oriented.min.z);
    CopyX87(box.max.x, oriented.max.x);
    CopyX87(box.max.y, oriented.max.y);
    CopyX87(box.max.z, oriented.max.z);
}

// ==== 0x10006d10 ParticleAxisAlignedBoundingBox::GetLocalVolume(Sphere&) const
void __cdecl ParticleAxisAlignedBoundingBox::GetLocalVolume(Sphere& sphere_) const
{
    CopyX87(sphere_.center.x, localBox.center.x);
    CopyX87(sphere_.center.y, localBox.center.y);
    CopyX87(sphere_.center.z, localBox.center.z);
    sphere_.radius = static_cast<float>(SolidsDetail::Length(localBox.extents));
}

// ==== 0x100506a0 ParticleAxisAlignedBoundingBox::TestCollision(const Ray&, bool&) const
bool __cdecl ParticleAxisAlignedBoundingBox::TestCollision(const Ray& ray, bool& hit) const
{
    hit = false;
    if (dirtyFlags & 8) RecalculateAxisAlignedBoxVolume();
    float tMin, tMax;
    if (!axisAlignedBox.GetMinMaxCollision(ray.point, ray.direction, tMin, tMax)) return false;
    if (0.0f > tMax) return false;  // NaN passes, like the original's COMISS/JA
    if (ExtentsNonZero(localBox)) hit = true;
    return true;
}

// ==== 0x10050730 ParticleAxisAlignedBoundingBox::TestCollision(const LineSegment&, bool&) const
bool __cdecl ParticleAxisAlignedBoundingBox::TestCollision(const LineSegment& segment, bool& hit) const
{
    hit = false;
    if (dirtyFlags & 8) RecalculateAxisAlignedBoxVolume();
    if (!axisAlignedBox.TestCollision(segment)) return false;
    if (ExtentsNonZero(localBox)) hit = true;
    return true;
}

// ==== 0x100507b0 ParticleAxisAlignedBoundingBox::RecalculateSphereVolume() const
void __cdecl ParticleAxisAlignedBoundingBox::RecalculateSphereVolume() const
{
    ParticleAxisAlignedBoundingBox* const self = const_cast<ParticleAxisAlignedBoundingBox*>(this);
    if (dirtyFlags & 8) RecalculateAxisAlignedBoxVolume();

    Vector3 delta{SubSS(axisAlignedBox.max.x, axisAlignedBox.min.x),
                  SubSS(axisAlignedBox.max.y, axisAlignedBox.min.y),
                  SubSS(axisAlignedBox.max.z, axisAlignedBox.min.z)};
    const float radius = static_cast<float>(SolidsDetail::Length(delta) * 0.5);

    Vector3 center = axisAlignedBox.GetCenter();
    Particle* const owner = host;
    if (Parent(owner)) {
        GlobalToLocal(Parent(owner), center);
    } else {
        GlobalToLocal(owner, center);
        float rotated[3];
        RotateByQuaternion(rotated, &center.x, &Orientation(owner).x);
        const Vector3& position = Position(owner);
        center.x = AddSS(position.x, rotated[0]);
        center.y = AddSS(position.y, rotated[1]);
        center.z = AddSS(position.z, rotated[2]);
    }
    self->sphere.center = center;
    self->sphere.radius = radius;
    self->dirtyFlags &= ~2u;
}

// ==== 0x10050900 ParticleAxisAlignedBoundingBox::RecalculateAxisAlignedBoxVolume() const
void __cdecl ParticleAxisAlignedBoundingBox::RecalculateAxisAlignedBoxVolume() const
{
    ParticleAxisAlignedBoundingBox* const self = const_cast<ParticleAxisAlignedBoundingBox*>(this);
    GetChildBoundingAxisAlignBox(self->axisAlignedBox);

    if (ExtentsNonZero(localBox)) {
        OrientedBox transformed = localBox;
        LocalToGlobal(host, transformed);
        AxisAlignedBox box;
        BuildAlignedBox(box, transformed);

        if (axisAlignedBox.min.x == axisAlignedBox.max.x && axisAlignedBox.min.y == axisAlignedBox.max.y &&
            axisAlignedBox.min.z == axisAlignedBox.max.z) {
            self->axisAlignedBox = box;
        } else {
            if (axisAlignedBox.min.x > box.min.x) self->axisAlignedBox.min.x = box.min.x;
            if (axisAlignedBox.min.y > box.min.y) self->axisAlignedBox.min.y = box.min.y;
            if (axisAlignedBox.min.z > box.min.z) self->axisAlignedBox.min.z = box.min.z;
            if (box.max.x > axisAlignedBox.max.x) self->axisAlignedBox.max.x = box.max.x;
            if (box.max.y > axisAlignedBox.max.y) self->axisAlignedBox.max.y = box.max.y;
            if (box.max.z > axisAlignedBox.max.z) self->axisAlignedBox.max.z = box.max.z;
        }
    }
    self->dirtyFlags &= ~8u;
}

// ==== 0x10050aa0 ParticleAxisAlignedBoundingBox::RecalculateOrientedBoxVolume() const
void __cdecl ParticleAxisAlignedBoundingBox::RecalculateOrientedBoxVolume() const
{
    ParticleAxisAlignedBoundingBox* const self = const_cast<ParticleAxisAlignedBoundingBox*>(this);
    if (dirtyFlags & 8) RecalculateAxisAlignedBoxVolume();
    OrientedBox transformed(axisAlignedBox);
    GlobalToLocal(host, transformed);
    self->orientedBox = transformed;
    self->dirtyFlags &= ~4u;
}

// ==== 0x100535c0 ParticleAxisAlignedBoundingBox::InitInstance(ParticleAxisAlignedBoundingBox&) const
void __cdecl ParticleAxisAlignedBoundingBox::InitInstance(ParticleAxisAlignedBoundingBox& instance) const
{
    instance.dirtyFlags = 0xfffff;
    instance.localBox = localBox;
}

}  // namespace Jet
