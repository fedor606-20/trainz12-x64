// Unit U9b2 of Render3DCore_7c: BoneModifier, BoneBlender and SkeletonModifier.
//
// Layouts per Render3DCoreBones.h. What is local is the object state, the priority-sorted BoneModifier list of a
// BoneBlender, the skeleton's BoneBlender array (built from the "b." children of a particle) and every wrapper that
// walks those arrays. The pieces that depend on the derived BoneModifier subclasses in other modules
// (EnumerateBones -> RTTI -> AddToBoneBlenders), the influence blend loop and the bone-tree switch stay the
// original's through a vtable thunk that resolves the original export in Render3DCore_7c_orig.dll; their exports
// forward.
#include <jet/Render3DCoreBones.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

HMODULE OrigModule()
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("Render3DCore_7c_orig.dll");
        return m ? m : LoadLibraryA("Render3DCore_7c_orig.dll");
    }();
    return module;
}

template <class F>
F OrigProc(const char* name)
{
    HMODULE m = OrigModule();
    F fn{};
    FARPROC raw = m ? GetProcAddress(m, name) : nullptr;
    std::memcpy(&fn, &raw, sizeof fn);
    return fn;
}

// InsertBefore/Add stops before the first entry of equal or higher priority (FCOMIP: insert when existing >= new);
// InsertAfter stops before the first entry of strictly higher priority (JA: existing > new), so it goes after its
// equals. A NaN on either side makes the comparison unordered and the scan continues to the end (FCOMIP leaves CF=1
// and the JNC/JA branches fall through), so the loop is written as the original's negated condition.
void InsertSorted(FastDynamicArray<BoneModifier*>& modifiers, BoneModifier* modifier)
{
    FastDynamicArray<BoneModifier*>::Iterator it = modifiers.First();
    const float priority = modifier->GetPriority();
    while (it != modifiers.End() && !((*it)->GetPriority() >= priority)) ++it;
    modifiers.InsertBefore(it, modifier);
}

void InsertAfterEqual(FastDynamicArray<BoneModifier*>& modifiers, BoneModifier* modifier)
{
    FastDynamicArray<BoneModifier*>::Iterator it = modifiers.First();
    const float priority = modifier->GetPriority();
    while (it != modifiers.End() && !((*it)->GetPriority() > priority)) ++it;
    modifiers.InsertBefore(it, modifier);
}

void CollectBones(Particle* parent, SkeletonModifier* host, FastDynamicArray<BoneBlender*>& out)
{
    for (Particle::Iterator it = parent->First(); it != parent->End(); ++it) {
        Particle* child = *it;
        const PString& name = child->GetName();
        const char* text = name.Text();
        if (text && text[0] == 'b' && text[1] == '.') out.Add(new BoneBlender(host, name, child));
        CollectBones(child, host, out);
    }
}

// ModifierList::modifierListLock is a data export of RenderSystem_7c; the shared header cannot mark it dllimport
// (RenderSystem defines it itself), so the address comes from the loaded module as the project does for other data.
CriticalSection* ModifierLock()
{
    static CriticalSection* lock = [] {
        HMODULE rs = GetModuleHandleA("RenderSystem_7c.dll");
        return rs ? reinterpret_cast<CriticalSection*>(
                        GetProcAddress(rs, "?modifierListLock@ModifierList@Jet@@1VCriticalSection@2@A"))
                  : nullptr;
    }();
    return lock;
}

PString g_nullBoneString;

}  // namespace

// ---- BoneModifier ---------------------------------------------------------------------------------------------

PString& BoneModifier::nullString = g_nullBoneString;

BoneModifier::BoneModifier() : Modifier(), updateFlags(0) {}

BoneModifier::BoneModifier(const PString& name, float priority, const PString& rootBone_, unsigned long flags)
    : Modifier(name, priority), rootBone(rootBone_), updateFlags(flags)
{
}

BoneModifier::BoneModifier(ModifierHost* host, const PString& name, float priority, const PString& rootBone_,
                           unsigned long flags)
    : Modifier(host, name, priority), rootBone(rootBone_), updateFlags(flags)
{
}

BoneModifier::BoneModifier(RenderKernel* kernel, const PString& name, float priority, const PString& rootBone_,
                           unsigned long flags)
    : Modifier(kernel, name, priority), rootBone(rootBone_), updateFlags(flags)
{
}

BoneModifier::BoneModifier(const BoneModifier& o) : Modifier(o), rootBone(o.rootBone), updateFlags(o.updateFlags) {}

BoneModifier::~BoneModifier() {}

BoneModifier& BoneModifier::operator=(const BoneModifier& o)
{
    Modifier::operator=(o);
    rootBone = o.rootBone;
    updateFlags = o.updateFlags;
    return *this;
}

Modifier::Result __cdecl BoneModifier::Update(float) { return 0; }

// The base vtable slot is the original's shared stub: it leaves the (caller-provided) list untouched.
void __cdecl BoneModifier::EnumerateBones(DynamicArray<PString, 13, Less<PString, PString>>&) const {}

SkeletonModifier* __cdecl BoneModifier::GetSkeleton() { return dynamic_cast<SkeletonModifier*>(GetHost()); }

PString __cdecl BoneModifier::GetRootBone() const { return rootBone; }

void __cdecl BoneModifier::SetRootBone(const PString& rootBone_) { rootBone = rootBone_; }

float __cdecl BoneModifier::GetInfluence(const PString&) const { return 0.0f; }

float __cdecl BoneModifier::GetInfluenceLimit(const PString&) const { return 0.0f; }

float __cdecl BoneModifier::GetInfluenceInTime(const PString&) const { return 0.0f; }

float __cdecl BoneModifier::GetInfluenceOutTime(const PString&) const { return 0.0f; }

void __cdecl BoneModifier::SetInfluence(const PString&, float) {}

void __cdecl BoneModifier::SetInfluenceLimit(const PString&, float) {}

void __cdecl BoneModifier::SetInfluenceInTime(const PString&, float) {}

void __cdecl BoneModifier::SetInfluenceOutTime(const PString&, float) {}

bool __cdecl BoneModifier::GetUpdateFlag(unsigned long flag) const { return (updateFlags & flag) == flag; }

void __cdecl BoneModifier::SetUpdateFlag(unsigned long flag, bool set)
{
    if (set)
        updateFlags |= flag;
    else
        updateFlags &= ~flag;
}

void __cdecl BoneModifier::UpdateBone(const PString&, Particle*, float, bool, float) {}

// ---- BoneBlender ----------------------------------------------------------------------------------------------

BoneBlender::BoneBlender(SkeletonModifier* host_, const PString& boneName_, Particle* bone_)
    : host(host_), boneName(boneName_), bone(bone_)
{
}

BoneBlender::BoneBlender(const BoneBlender& o) : host(o.host), boneName(o.boneName), bone(o.bone)
{
    modifiers.AppendChunks(o.modifiers);
}

BoneBlender::~BoneBlender() {}

BoneBlender& BoneBlender::operator=(const BoneBlender& o)
{
    host = o.host;
    boneName = o.boneName;
    bone = o.bone;
    modifiers.RemoveAll();
    modifiers.AppendChunks(o.modifiers);
    return *this;
}

bool __cdecl BoneBlender::Add(BoneModifier* modifier) { return InsertBefore(modifier); }

bool __cdecl BoneBlender::InsertBefore(BoneModifier* modifier)
{
    InsertSorted(modifiers, modifier);
    return true;
}

bool __cdecl BoneBlender::InsertBefore(BoneModifier* modifier, const BoneModifier* before)
{
    const float priority = modifier->GetPriority();
    if (priority != before->GetPriority()) return false;
    for (FastDynamicArray<BoneModifier*>::Iterator it = modifiers.First(); it != modifiers.End(); ++it) {
        if (*it == before) {
            modifiers.InsertBefore(it, modifier);
            return true;
        }
    }
    modifiers.Add(modifier);
    return true;
}

bool __cdecl BoneBlender::InsertAfter(BoneModifier* modifier)
{
    InsertAfterEqual(modifiers, modifier);
    return true;
}

bool __cdecl BoneBlender::InsertAfter(BoneModifier* modifier, const BoneModifier* after)
{
    const float priority = modifier->GetPriority();
    if (priority != after->GetPriority()) return false;
    for (FastDynamicArray<BoneModifier*>::Iterator it = modifiers.First(); it != modifiers.End(); ++it) {
        if (*it == after) {
            ++it;
            modifiers.InsertBefore(it, modifier);
            return true;
        }
    }
    modifiers.Add(modifier);
    return true;
}

bool __cdecl BoneBlender::Remove(const PString& name)
{
    for (FastDynamicArray<BoneModifier*>::Iterator it = modifiers.First(); it != modifiers.End(); ++it) {
        if ((*it)->GetName() == name) {
            modifiers.Remove(it);
            return true;
        }
    }
    return false;
}

bool __cdecl BoneBlender::Remove(const Modifier* modifier)
{
    for (FastDynamicArray<BoneModifier*>::Iterator it = modifiers.First(); it != modifiers.End(); ++it) {
        if (static_cast<const Modifier*>(*it) == modifier) {
            modifiers.Remove(it);
            return true;
        }
    }
    return false;
}

void __thiscall BoneBlender::RemoveAll() { modifiers.RemoveAll(); }

bool __cdecl BoneBlender::UpdateBoneModifiers(float dt)
{
    using Fn = bool(__cdecl*)(BoneBlender*, float);
    Fn fn = OrigProc<Fn>("?UpdateBoneModifiers@BoneBlender@Jet@@UAA_NM@Z");
    return fn ? fn(this, dt) : false;
}

const PString& __cdecl BoneBlender::GetBoneName() const { return boneName; }

Particle* __cdecl BoneBlender::GetBone() { return bone; }

const Particle* __cdecl BoneBlender::GetBone() const { return bone; }

void __cdecl BoneBlender::SetBone(Particle* bone_) { bone = bone_; }

BoneModifier* __cdecl BoneBlender::GetFirstReplaceable(float priority)
{
    for (FastDynamicArray<BoneModifier*>::Iterator it = modifiers.First(); it != modifiers.End(); ++it) {
        BoneModifier* modifier = *it;
        if (modifier->GetPriority() == priority && !modifier->GetUpdateFlag(BoneModifier::FIXED)) return modifier;
    }
    return nullptr;
}

// ---- SkeletonModifier -----------------------------------------------------------------------------------------

SkeletonModifier::SkeletonModifier(Particle* particle, float priority)
    : ModifierList(), Modifier(PString("JETSkeletonModifier"), priority)
{
    CreateBlenders(particle);
}

SkeletonModifier::SkeletonModifier(Particle* particle, ModifierHost* host, float priority)
    : ModifierList(), Modifier(host, PString("JETSkeletonModifier"), priority)
{
    CreateBlenders(particle);
}

SkeletonModifier::SkeletonModifier(Particle* particle, RenderKernel* kernel, float priority)
    : ModifierList(), Modifier(kernel, PString("JETSkeletonModifier"), priority)
{
    CreateBlenders(particle);
}

SkeletonModifier::SkeletonModifier(const SkeletonModifier& o)
    : ModifierList(o), Modifier(static_cast<const Modifier&>(o))
{
    blenders.AppendChunks(o.blenders);
}

SkeletonModifier::~SkeletonModifier()
{
    if (ModifierHost* host = Modifier::GetHost()) host->Remove(static_cast<Modifier*>(this));
    {
        SingleLock lock(ModifierLock(), true);
        while (!blenders.IsEmpty()) {
            BoneBlender* blender = *blenders.First();
            blenders.Remove(blenders.First());
            delete blender;
        }
    }
}

SkeletonModifier& SkeletonModifier::operator=(const SkeletonModifier& o)
{
    ModifierList::operator=(o);
    Modifier::operator=(static_cast<const Modifier&>(o));
    blenders.RemoveAll();
    blenders.AppendChunks(o.blenders);
    return *this;
}

ModifierHost::Result __cdecl SkeletonModifier::Add(Modifier* modifier)
{
    using Fn = void(__cdecl*)(SkeletonModifier*, void*, Modifier*);
    Fn fn = OrigProc<Fn>("?Add@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResultTable@2@"
                          "BW4Results@ModifierHost@Jet@@@2@PAVModifier@2@@Z");
    ModifierHost::Result result;
    if (fn) fn(this, &result, modifier);
    return result;
}

ModifierHost::Result __cdecl SkeletonModifier::InsertBefore(Modifier* modifier)
{
    using Fn = void(__cdecl*)(SkeletonModifier*, void*, Modifier*);
    Fn fn = OrigProc<Fn>("?InsertBefore@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResult"
                          "Table@2@BW4Results@ModifierHost@Jet@@@2@PAVModifier@2@@Z");
    ModifierHost::Result result;
    if (fn) fn(this, &result, modifier);
    return result;
}

ModifierHost::Result __cdecl SkeletonModifier::InsertBefore(Modifier* modifier, const Modifier* before)
{
    using Fn = void(__cdecl*)(SkeletonModifier*, void*, Modifier*, const Modifier*);
    Fn fn = OrigProc<Fn>("?InsertBefore@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBURe"
                          "sultTable@2@BW4Results@ModifierHost@Jet@@@2@PAVModifier@2@PBV42@@Z");
    ModifierHost::Result result;
    if (fn) fn(this, &result, modifier, before);
    return result;
}

ModifierHost::Result __cdecl SkeletonModifier::InsertAfter(Modifier* modifier)
{
    using Fn = void(__cdecl*)(SkeletonModifier*, void*, Modifier*);
    Fn fn = OrigProc<Fn>("?InsertAfter@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResult"
                          "Table@2@BW4Results@ModifierHost@Jet@@@2@PAVModifier@2@@Z");
    ModifierHost::Result result;
    if (fn) fn(this, &result, modifier);
    return result;
}

ModifierHost::Result __cdecl SkeletonModifier::InsertAfter(Modifier* modifier, const Modifier* after)
{
    using Fn = void(__cdecl*)(SkeletonModifier*, void*, Modifier*, const Modifier*);
    Fn fn = OrigProc<Fn>("?InsertAfter@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUR"
                          "esultTable@2@BW4Results@ModifierHost@Jet@@@2@PAVModifier@2@PBV42@@Z");
    ModifierHost::Result result;
    if (fn) fn(this, &result, modifier, after);
    return result;
}

ModifierHost::Result __cdecl SkeletonModifier::Remove(const Modifier* modifier)
{
    SingleLock lock(ModifierLock(), true);
    bool removed = true;
    for (FastDynamicArray<BoneBlender*>::Iterator it = blenders.First(); it != blenders.End(); ++it)
        if (!(*it)->Remove(modifier)) removed = false;
    const ModifierHost::Result result = ModifierList::Remove(modifier);
    return removed ? result : ModifierHost::Result(5);
}

ModifierHost::Result __cdecl SkeletonModifier::Remove(const PString& name)
{
    SingleLock lock(ModifierLock(), true);
    bool removed = true;
    for (FastDynamicArray<BoneBlender*>::Iterator it = blenders.First(); it != blenders.End(); ++it)
        if (!(*it)->Remove(name)) removed = false;
    const ModifierHost::Result result = ModifierList::Remove(name);
    return removed ? result : ModifierHost::Result(5);
}

void __cdecl SkeletonModifier::RemoveAll()
{
    SingleLock lock(ModifierLock(), true);
    for (FastDynamicArray<BoneBlender*>::Iterator it = blenders.First(); it != blenders.End(); ++it)
        (*it)->RemoveAll();
    ModifierList::RemoveAll();
}

ModifierHost::Result __cdecl SkeletonModifier::UpdateModifiers(float dt)
{
    return ModifierList::UpdateModifiers(dt);
}

Modifier::Result __cdecl SkeletonModifier::Update(float dt)
{
    // Modifier::complete is private; the original reads the same bit (+0x28 here is Modifier +0x10).
    if (reinterpret_cast<unsigned char*>(static_cast<Modifier*>(this))[0x10] & 1) return 2;
    SingleLock lock(ModifierLock(), true);
    ModifierList::UpdateModifiers(dt);
    for (FastDynamicArray<BoneBlender*>::Iterator it = blenders.First(); it != blenders.End(); ++it)
        (*it)->UpdateBoneModifiers(dt);
    return 0;
}

PString __cdecl SkeletonModifier::GetRootBone() const
{
    SingleLock lock(ModifierLock(), true);
    if (!blenders.IsEmpty()) return (*blenders.First())->GetBoneName();
    return PString();
}

int __cdecl SkeletonModifier::GetNumBones() const
{
    SingleLock lock(ModifierLock(), true);
    return static_cast<int>(blenders.Count());
}

bool __cdecl SkeletonModifier::RemoveBlender(BoneBlender* blender, bool deleteBlender)
{
    SingleLock lock(ModifierLock(), true);
    for (FastDynamicArray<BoneBlender*>::Iterator it = blenders.First(); it != blenders.End(); ++it) {
        if (*it == blender) {
            blenders.Remove(it);
            if (deleteBlender) delete blender;
            return true;
        }
    }
    return false;
}

unsigned int __cdecl SkeletonModifier::RemoveBlenders(const FastDynamicArray<BoneBlender*>& list, bool deleteBlenders)
{
    SingleLock lock(ModifierLock(), true);
    unsigned int removed = 0;
    for (FastDynamicArray<BoneBlender*>::ConstIterator it = list.First(); it != list.End(); ++it)
        if (RemoveBlender(*it, deleteBlenders)) ++removed;
    return removed;
}

void __cdecl SkeletonModifier::SwitchParticles(Particle* particle, FastDynamicArray<BoneBlender*>* old,
                                               FastDynamicArray<BoneBlender*>* current)
{
    using Fn = void(__cdecl*)(SkeletonModifier*, Particle*, FastDynamicArray<BoneBlender*>*,
                              FastDynamicArray<BoneBlender*>*);
    Fn fn = OrigProc<Fn>("?SwitchParticles@SkeletonModifier@Jet@@QAAXPAVParticle@2@PAV?$FastDynamicArray@PAVBoneBlender@"
                         "Jet@@$0N@U?$Less@PAVBoneBlender@Jet@@PAV12@@2@@2@1@Z");
    if (fn) fn(this, particle, old, current);
}

ModifierHost::Result __cdecl SkeletonModifier::AddToBoneBlenders(Modifier* modifier, const Modifier* other, bool after)
{
    using Fn = void(__cdecl*)(SkeletonModifier*, void*, Modifier*, const Modifier*, bool);
    Fn fn = OrigProc<Fn>("?AddToBoneBlenders@SkeletonModifier@Jet@@IAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBU"
                         "ResultTable@2@BW4Results@ModifierHost@Jet@@@2@PAVModifier@2@PBV42@_N@Z");
    ModifierHost::Result result;
    if (fn) fn(this, &result, modifier, other, after);
    return result;
}

void __cdecl SkeletonModifier::CreateBlenders(Particle* particle)
{
    if (!particle) return;
    SingleLock lock(ModifierLock(), true);
    CollectBones(particle, this, blenders);
}

BoneBlender* __cdecl SkeletonModifier::GetBoneBlender(const PString& name)
{
    SingleLock lock(ModifierLock(), true);
    for (FastDynamicArray<BoneBlender*>::Iterator it = blenders.First(); it != blenders.End(); ++it)
        if ((*it)->GetBoneName() == name) return *it;
    return nullptr;
}

void __cdecl SkeletonModifier::UnlinkBlenders()
{
    SingleLock lock(ModifierLock(), true);
    for (FastDynamicArray<BoneBlender*>::Iterator it = blenders.First(); it != blenders.End(); ++it)
        (*it)->SetBone(nullptr);
}

}  // namespace Jet
