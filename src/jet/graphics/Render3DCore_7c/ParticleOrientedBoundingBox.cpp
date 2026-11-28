// Unit U3 of Render3DCore_7c: Jet::ParticleOrientedBoundingBox, the volume whose local shape is the OrientedBox at
// +0xa0, transcribed from the TS12 assembly (0x100530b0..0x10053700).
//
// It reuses the AxisAlignedBoundingBox implementations of Set/GetLocalVolume and the child-flag helpers (its vtable
// slots 7..12 point at those functions); the oriented box itself is the primary volume. IsWithinView and the
// renderable overrides live in ParticleBoundingView.cpp (unit U3b); the sphere view bodies are shared there, as the
// original's vtable aliases them.
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
using BoundsDetail::LocalToGlobal;
using BoundsDetail::MulSS;
using BoundsDetail::Orientation;
using BoundsDetail::Position;
using BoundsDetail::RotateByQuaternion;
using BoundsDetail::SubSS;

void MaximizeOrig(OrientedBox& box, const OrientedBox& other)
{
    box.Maximize(other);
}

bool ExtentsNonZero(const OrientedBox& box)
{
    return box.extents.x != 0.0f || box.extents.y != 0.0f || box.extents.z != 0.0f;  // NaN counts as non-zero
}

void BuildAlignedBox(AxisAlignedBox& out, const OrientedBox& box)
{
    out = AxisAlignedBox(box);
}

}  // namespace

// ==== 0x100531a0 ParticleOrientedBoundingBox::ParticleOrientedBoundingBox(Particle*)
ParticleOrientedBoundingBox::ParticleOrientedBoundingBox(Particle* host_)
    : ParticleAxisAlignedBoundingBox(host_)
{
    primaryVolumeType = PRIMARY_ORIENTED_BOX;
}

// ==== 0x100530b0 ParticleOrientedBoundingBox::ParticleOrientedBoundingBox(const ParticleOrientedBoundingBox&)
ParticleOrientedBoundingBox::ParticleOrientedBoundingBox(const ParticleOrientedBoundingBox& o)
    : ParticleAxisAlignedBoundingBox(o)
{
}

// ==== 0x10051f60 ParticleOrientedBoundingBox::~ParticleOrientedBoundingBox()
ParticleOrientedBoundingBox::~ParticleOrientedBoundingBox()
{
}

// ==== 0x10006ae0 ParticleOrientedBoundingBox::operator= (the original aliases the AABB body)
ParticleOrientedBoundingBox& ParticleOrientedBoundingBox::operator=(const ParticleOrientedBoundingBox& o)
{
    ParticleAxisAlignedBoundingBox::operator=(o);
    return *this;
}

// ==== 0x10053150 ParticleOrientedBoundingBox::CreateInstance(Particle*) const
ParticleBoundingVolume* __cdecl ParticleOrientedBoundingBox::CreateInstance(Particle* host_) const
{
    void* const memory = Alloc::New(sizeof(ParticleOrientedBoundingBox));
    if (!memory) return nullptr;
    ParticleOrientedBoundingBox* const instance = ::new (memory) ParticleOrientedBoundingBox(host_);
    instance->dirtyFlags = 0xfffff;
    instance->localBox = localBox;
    return instance;
}

// ==== 0x10053270 ParticleOrientedBoundingBox::TestCollision(const Ray&, bool&) const
bool __cdecl ParticleOrientedBoundingBox::TestCollision(const Ray& ray, bool& hit) const
{
    hit = false;
    if (dirtyFlags & 4) RecalculateOrientedBoxVolume();
    OrientedBox transformed = orientedBox;
    LocalToGlobal(host, transformed);

    float tMin, tMax;
    if (!transformed.GetMinMaxCollision(ray.point, ray.direction, tMin, tMax)) return false;
    if (0.0f > tMax) return false;  // NaN passes, like the original's COMISS/JA
    if (ExtentsNonZero(localBox)) hit = true;
    return true;
}

// ==== 0x10053330 ParticleOrientedBoundingBox::TestCollision(const LineSegment&, bool&) const
bool __cdecl ParticleOrientedBoundingBox::TestCollision(const LineSegment& segment, bool& hit) const
{
    hit = false;
    if (dirtyFlags & 4) RecalculateOrientedBoxVolume();
    OrientedBox transformed = orientedBox;
    LocalToGlobal(host, transformed);
    if (!transformed.TestCollision(segment)) return false;
    if (ExtentsNonZero(localBox)) hit = true;
    return true;
}

// ==== 0x100533d0 ParticleOrientedBoundingBox::RecalculateSphereVolume() const
void __cdecl ParticleOrientedBoundingBox::RecalculateSphereVolume() const
{
    ParticleOrientedBoundingBox* const self = const_cast<ParticleOrientedBoundingBox*>(this);
    if (dirtyFlags & 4) RecalculateOrientedBoxVolume();

    Particle* const owner = host;
    float rotated[3];
    RotateByQuaternion(rotated, &orientedBox.center.x, &Orientation(owner).x);
    const Vector3& position = Position(owner);
    self->sphere.center.x = AddSS(position.x, rotated[0]);
    self->sphere.center.y = AddSS(position.y, rotated[1]);
    self->sphere.center.z = AddSS(position.z, rotated[2]);
    self->sphere.radius = static_cast<float>(SolidsDetail::Length(orientedBox.extents));
    self->dirtyFlags &= ~2u;
}

// ==== 0x10053480 ParticleOrientedBoundingBox::RecalculateAxisAlignedBoxVolume() const
void __cdecl ParticleOrientedBoundingBox::RecalculateAxisAlignedBoxVolume() const
{
    ParticleOrientedBoundingBox* const self = const_cast<ParticleOrientedBoundingBox*>(this);
    if (dirtyFlags & 4) RecalculateOrientedBoxVolume();
    OrientedBox transformed = orientedBox;
    LocalToGlobal(host, transformed);
    AxisAlignedBox box;
    BuildAlignedBox(box, transformed);
    self->axisAlignedBox = box;
    self->dirtyFlags &= ~8u;
}

// ==== 0x10053520 ParticleOrientedBoundingBox::RecalculateOrientedBoxVolume() const
void __cdecl ParticleOrientedBoundingBox::RecalculateOrientedBoxVolume() const
{
    ParticleOrientedBoundingBox* const self = const_cast<ParticleOrientedBoundingBox*>(this);
    GetChildBoundingOrientedBox(self->orientedBox);
    if (ExtentsNonZero(localBox)) {
        if (ExtentsNonZero(orientedBox)) {
            MaximizeOrig(self->orientedBox, localBox);
            self->dirtyFlags &= ~4u;
            return;
        }
        self->orientedBox = localBox;
    }
    self->dirtyFlags &= ~4u;
}

// ==== 0x100535c0 ParticleOrientedBoundingBox::InitInstance(ParticleOrientedBoundingBox&) const
void __cdecl ParticleOrientedBoundingBox::InitInstance(ParticleOrientedBoundingBox& instance) const
{
    instance.dirtyFlags = 0xfffff;
    instance.localBox = localBox;
}

}  // namespace Jet
