// Unit U3 of Render3DCore_7c: Jet::ParticleVisibleBoundingBox (0x10053dd0..0x10053e60 and 0x100277f0). It is an
// AxisAlignedBoundingBox with the AABB shape at +0xa0; only CreateInstance and the always-true IsWithinView differ.
#include <jet/Render3DCoreBounds.h>
#include <jet/Render3DCoreParticle.h>

#include <new>

namespace Jet {

// ==== 0x10053e60 ParticleVisibleBoundingBox::ParticleVisibleBoundingBox(Particle*)
ParticleVisibleBoundingBox::ParticleVisibleBoundingBox(Particle* host_)
    : ParticleAxisAlignedBoundingBox(host_)
{
    primaryVolumeType = PRIMARY_AXIS_ALIGNED_BOX;
}

// ==== 0x10053dd0 ParticleVisibleBoundingBox::ParticleVisibleBoundingBox(const ParticleVisibleBoundingBox&)
ParticleVisibleBoundingBox::ParticleVisibleBoundingBox(const ParticleVisibleBoundingBox& o)
    : ParticleAxisAlignedBoundingBox(o)
{
}

// ==== 0x10051f60 ParticleVisibleBoundingBox::~ParticleVisibleBoundingBox()
ParticleVisibleBoundingBox::~ParticleVisibleBoundingBox()
{
}

// ==== 0x10006ae0 ParticleVisibleBoundingBox::operator= (the original aliases the AABB body)
ParticleVisibleBoundingBox& ParticleVisibleBoundingBox::operator=(const ParticleVisibleBoundingBox& o)
{
    ParticleAxisAlignedBoundingBox::operator=(o);
    return *this;
}

// ==== 0x10053e10 ParticleVisibleBoundingBox::CreateInstance(Particle*) const
ParticleBoundingVolume* __cdecl ParticleVisibleBoundingBox::CreateInstance(Particle* host_) const
{
    void* const memory = Alloc::New(sizeof(ParticleVisibleBoundingBox));
    if (!memory) return nullptr;
    ParticleVisibleBoundingBox* const instance = ::new (memory) ParticleVisibleBoundingBox(host_);
    instance->dirtyFlags = 0xfffff;
    instance->localBox = localBox;
    return instance;
}

// ==== 0x100277f0 ParticleVisibleBoundingBox::IsWithinView: `mov al,1; ret` for both overloads
bool __cdecl ParticleVisibleBoundingBox::IsWithinView(const ViewPoint&) const
{
    return true;
}

bool __cdecl ParticleVisibleBoundingBox::IsWithinView(const Camera*) const
{
    return true;
}

}  // namespace Jet
