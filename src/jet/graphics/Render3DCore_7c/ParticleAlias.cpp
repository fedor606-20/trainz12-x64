// Unit U9a of Render3DCore_7c: Jet::ParticleAlias. 0x140 bytes: ViewPortParticle plus the aliased view-port particle
// at +0x13c. An alias is registered in its target's alias array; setting a new target unregisters the old one, creates
// a private bounding volume bound to the alias and copies the target's render flags.
#include <jet/Render3DCoreParticle.h>

#include <new>

namespace Jet {



// 0x50150 ParticleAlias::ParticleAlias(ViewPortParticle*)
ParticleAlias::ParticleAlias(ViewPortParticle* target) : ViewPortParticle()
{
    aliasTarget = target;
    AssumeIdentity(target);
}

// 0x50020 ParticleAlias::ParticleAlias(const ParticleAlias&)
ParticleAlias::ParticleAlias(const ParticleAlias& o) : ViewPortParticle(o)
{
    aliasTarget = o.aliasTarget;
}

// 0x501d0 ParticleAlias::~ParticleAlias()
ParticleAlias::~ParticleAlias()
{
    if (aliasTarget) aliasTarget->RemoveAlias(this);
}

// 0x50050 ParticleAlias::operator=(const ParticleAlias&)
ParticleAlias& ParticleAlias::operator=(const ParticleAlias& o)
{
    Particle::operator=(o);
    renderFlags = o.renderFlags;
    CopyAliasesFrom(o);
    aliasTarget = o.aliasTarget;
    return *this;
}

// 0x50210 ParticleAlias::CreateInstance() const
Particle* ParticleAlias::CreateInstance() const
{
    ParticleAlias* instance = static_cast<ParticleAlias*>(Alloc::New(sizeof(ParticleAlias)));
    if (instance) {
        ::new (instance) ParticleAlias(aliasTarget);
        Particle::InitInstance(*instance);
        instance->renderFlags = renderFlags;
    }
    return instance;
}

// 0xd380 ParticleAlias::GetID() const
int ParticleAlias::GetID() const
{
    return 3;
}

// 0x50120 ParticleAlias::IsOfType(int) const
bool ParticleAlias::IsOfType(int id) const
{
    return id == 3 || id == 1;
}

// 0x2ba60 ParticleAlias::GetAliasTo() const
ViewPortParticle* ParticleAlias::GetAliasTo() const
{
    return aliasTarget;
}

// 0x502d0 ParticleAlias::SetAliasTo(ViewPortParticle*)
void ParticleAlias::SetAliasTo(ViewPortParticle* target)
{
    if (aliasTarget) aliasTarget->RemoveAlias(this);
    AssumeIdentity(target);
}

// 0x50140 ParticleAlias::Unalias()
void ParticleAlias::Unalias()
{
    aliasTarget = nullptr;
}

// 0x50310 ParticleAlias::AssumeIdentity(ViewPortParticle*)
void ParticleAlias::AssumeIdentity(ViewPortParticle* target)
{
    aliasTarget = target;
    if (!target) return;

    renderFlags = target->GetRenderFlags();

    void* const bv = *reinterpret_cast<void**>(reinterpret_cast<char*>(target) + 0x4c);
    using FnCreate = void*(__cdecl*)(void*, Particle*);
    void* const volume = reinterpret_cast<FnCreate>((*reinterpret_cast<void***>(bv))[0x34 / 4])(bv, this);
    SetBoundingVolume(reinterpret_cast<ParticleBoundingVolume*>(volume));

    target->AddAlias(this);
}

// 0x502a0 ParticleAlias::InitInstance(ParticleAlias&) const
void ParticleAlias::InitInstance(ParticleAlias& instance) const
{
    Particle::InitInstance(instance);
    instance.renderFlags = renderFlags;
}

// 0x50370 ParticleAlias::CheckVisible(const ViewPoint&)
bool ParticleAlias::CheckVisible(const ViewPoint& view)
{
    if (aliasTarget && !aliasTarget->IsVisited() && (flags & 2) != 0) {
        void* const bv = boundingVolume;
        using FnIsWithin = bool(__cdecl*)(void*, const ViewPoint*);
        if (reinterpret_cast<FnIsWithin>((*reinterpret_cast<void***>(bv))[8 / 4])(bv, &view)) {
            const unsigned int frame = *reinterpret_cast<unsigned int*>(
                reinterpret_cast<char*>(*reinterpret_cast<void* const*>(reinterpret_cast<char const*>(&view) + 0x7c)) + 0x3c);
            SetVisited();
            SetLastCulledFrame(frame);
            aliasTarget->SetVisited();
            aliasTarget->SetLastCulledFrame(frame);
            return true;
        }
    }
    return false;
}

// 0x50400 ParticleAlias::Cull(ViewPoint&, const Matrix4&)
void ParticleAlias::Cull(ViewPoint& view, const Matrix4& m)
{
    Particle::Cull(view, m);
    if (aliasTarget) aliasTarget->Cull(view, m);
}

}  // namespace Jet
