// Unit U9a of Render3DCore_7c: Jet::ViewPortParticle. 0x13c bytes: Particle plus render flags (+0x12c) and the
// alias pointer array {count, capacity, data} (+0x130..+0x138). The array is a plain Jet::Alloc block of pointers,
// grown to exactly count+1 (0x1002ef90 copies it, 0x10075af0 appends, 0x10075aa0 removes one, 0x10075a40 deletes the
// aliases but leaves the array alone).
#include <jet/Render3DCoreParticle.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

struct AliasArray {
    unsigned int count;
    unsigned int capacity;
    ParticleAlias** data;
};

AliasArray& Aliases(ViewPortParticle* p)
{
    return *reinterpret_cast<AliasArray*>(reinterpret_cast<char*>(p) + 0x130);
}

const AliasArray& Aliases(const ViewPortParticle* p)
{
    return *reinterpret_cast<const AliasArray*>(reinterpret_cast<char const*>(p) + 0x130);
}

// 0x10075af0: append one alias, growing the block to exactly count+1
void ArrayAdd(AliasArray& a, ParticleAlias* alias)
{
    const unsigned int next = a.count + 1;
    if (a.capacity < next) {
        void* const data = Alloc::New(next * sizeof(ParticleAlias*));
        if (a.data) {
            std::memcpy(data, a.data, a.count * sizeof(ParticleAlias*));
            Alloc::Delete(a.data);
        }
        a.data = static_cast<ParticleAlias**>(data);
        a.capacity = next;
    }
    a.data[a.count++] = alias;
}

// 0x10075aa0: remove one alias (return 1 when found, 0 otherwise, as the original)
bool ArrayRemove(AliasArray& a, ParticleAlias* alias)
{
    for (unsigned int i = 0; i < a.count; ++i) {
        if (a.data[i] == alias) {
            if (i < a.count) {
                --a.count;
                std::memmove(a.data + i, a.data + i + 1, (a.count - i) * sizeof(ParticleAlias*));
            }
            return true;
        }
    }
    return false;
}

// 0x1002ef90: copy the alias array, keeping the source capacity when it is larger
void ArrayCopy(AliasArray& dst, const AliasArray& src)
{
    dst.count = 0;
    if (!src.data) return;
    if (dst.capacity < src.capacity) {
        if (dst.data) Alloc::Delete(dst.data);
        dst.data = static_cast<ParticleAlias**>(Alloc::New(src.capacity * sizeof(ParticleAlias*)));
        dst.capacity = src.capacity;
    }
    dst.count = src.count;
    for (unsigned int i = 0; i < src.count; ++i) dst.data[i] = src.data[i];
}

}  // namespace

// 0xaf10 ViewPortParticle::ViewPortParticle()
ViewPortParticle::ViewPortParticle() : Particle()
{
    renderFlags = 1;
    aliasCount = 0;
    aliasCapacity = 0;
    aliases = nullptr;
}

// 0xadf0 ViewPortParticle::ViewPortParticle(const ViewPortParticle&)
ViewPortParticle::ViewPortParticle(const ViewPortParticle& o) : Particle(o)
{
    renderFlags = o.renderFlags;
    aliasCount = 0;
    aliasCapacity = 0;
    aliases = nullptr;
    CopyAliasesFrom(o);
}

// 0x75980 ViewPortParticle::~ViewPortParticle()
ViewPortParticle::~ViewPortParticle()
{
    DeleteAllAliases();
    aliasCount = 0;
    if (aliases) Alloc::Delete(aliases);
}

// 0xae70 ViewPortParticle::operator=(const ViewPortParticle&)
ViewPortParticle& ViewPortParticle::operator=(const ViewPortParticle& o)
{
    Particle::operator=(o);
    renderFlags = o.renderFlags;
    CopyAliasesFrom(o);
    return *this;
}

// 0xaeb0 scalar deleting destructor is compiler generated from the virtual destructor.

// 0xaf40 ViewPortParticle::SetRenderFlags(unsigned long)
void ViewPortParticle::SetRenderFlags(unsigned long flags)
{
    renderFlags = flags;
}

// 0xaf50 ViewPortParticle::GetRenderFlags() const
unsigned long ViewPortParticle::GetRenderFlags() const
{
    return renderFlags;
}

// 0xaf60 ViewPortParticle::CheckVisible(const ViewPoint&)
bool ViewPortParticle::CheckVisible(const ViewPoint& view)
{
    const unsigned int mask = *reinterpret_cast<unsigned int*>(
        reinterpret_cast<char*>(*reinterpret_cast<void* const*>(reinterpret_cast<char const*>(&view) + 0x18)) + 0x50);
    if ((renderFlags & mask) != 0 && (flags & 2) != 0) {
        void* const bv = boundingVolume;
        using FnIsWithin = bool(__cdecl*)(void*, const ViewPoint*);
        if (reinterpret_cast<FnIsWithin>((*reinterpret_cast<void***>(bv))[8 / 4])(bv, &view)) {
            SetVisited();
            const unsigned int frame = *reinterpret_cast<unsigned int*>(
                reinterpret_cast<char*>(*reinterpret_cast<void* const*>(reinterpret_cast<char const*>(&view) + 0x7c)) + 0x3c);
            SetLastCulledFrame(frame);
            return true;
        }
    }
    return false;
}

// 0xafc0 ViewPortParticle::CreateInstance() const
Particle* ViewPortParticle::CreateInstance() const
{
    ViewPortParticle* instance = static_cast<ViewPortParticle*>(Alloc::New(sizeof(ViewPortParticle)));
    if (instance) {
        ::new (instance) ViewPortParticle();
        Particle::InitInstance(*instance);
        instance->renderFlags = renderFlags;
    }
    return instance;
}

// 0x75a00 ViewPortParticle::AddAlias(ParticleAlias*)
void ViewPortParticle::AddAlias(ParticleAlias* alias)
{
    ArrayAdd(Aliases(this), alias);
}

// 0x75a20 ViewPortParticle::RemoveAlias(ParticleAlias*)
void ViewPortParticle::RemoveAlias(ParticleAlias* alias)
{
    ArrayRemove(Aliases(this), alias);
}

// 0x75a40 ViewPortParticle::DeleteAllAliases()
void ViewPortParticle::DeleteAllAliases()
{
    AliasArray& a = Aliases(this);
    for (unsigned int i = 0; i < a.count; ++i) {
        ParticleAlias* const alias = a.data[i];
        *reinterpret_cast<ViewPortParticle**>(reinterpret_cast<char*>(alias) + 0x13c) = nullptr;
        if (alias) {
            using Fn = void(__thiscall*)(ParticleAlias*, int);
            reinterpret_cast<Fn>((*reinterpret_cast<void***>(alias))[0])(alias, 1);
        }
    }
}

// 0x502a0 ViewPortParticle::InitInstance(ViewPortParticle&) const (same code as ParticleAlias::InitInstance)
void ViewPortParticle::InitInstance(ViewPortParticle& instance) const
{
    Particle::InitInstance(instance);
    instance.renderFlags = renderFlags;
}

void ViewPortParticle::CopyAliasesFrom(const ViewPortParticle& o)
{
    ArrayCopy(Aliases(this), Aliases(&o));
}

}  // namespace Jet
