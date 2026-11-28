// Unit U9a of Render3DCore_7c: Jet::ParticleStructure. 0x140 bytes: ViewPortParticle plus the reference point list
// at +0x13c (DynamicArray<ReferencePoint, 3>). Attachment points and Cell are U9 scene classes; an AttachmentPoint is
// a Particle with the host name PString at +0x12c, which is all AddAttachmentPoint needs. The RTTI-based enumeration
// and the reference-point pose maths belong to the scene unit and stay forwarded.
#include <jet/Render3DCoreParticle.h>

#include <windows.h>

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
    return m ? reinterpret_cast<F>(GetProcAddress(m, name)) : nullptr;
}

// ITree<Particle>::Iterator::WalkNext with the sub-tree bound, as 0x100089d0
Particle* WalkNext(Particle* node, Particle* endNode)
{
    if (node->_count) return node->_head;
    if (node->_next) return node->_next;
    Particle* parent = node->_parent;
    while (parent) {
        if (endNode && (parent == endNode || parent->_next == endNode)) {
            parent = nullptr;
        } else {
            if (parent->_next) return parent->_next;
            parent = parent->_parent;
        }
    }
    return nullptr;
}

}  // namespace

// 0xb420 ParticleStructure::ParticleStructure()
ParticleStructure::ParticleStructure() : ViewPortParticle()
{
}

// 0xb300 ParticleStructure::ParticleStructure(const ParticleStructure&)
ParticleStructure::ParticleStructure(const ParticleStructure& o) : ViewPortParticle(o)
{
    referencePoints.AppendChunks(o.referencePoints);
}

// 0xb2a0 ParticleStructure::~ParticleStructure()
ParticleStructure::~ParticleStructure()
{
    referencePoints.RemoveAll();
}

// 0xb370 ParticleStructure::operator=(const ParticleStructure&)
ParticleStructure& ParticleStructure::operator=(const ParticleStructure& o)
{
    Particle::operator=(o);
    renderFlags = o.renderFlags;
    CopyAliasesFrom(o);
    referencePoints = o.referencePoints;
    return *this;
}

// 0xb460 ParticleStructure::CreateInstance() const
Particle* ParticleStructure::CreateInstance() const
{
    ParticleStructure* instance = static_cast<ParticleStructure*>(Alloc::New(sizeof(ParticleStructure)));
    if (instance) {
        ::new (instance) ParticleStructure();
        InitInstance(*instance);
        instance->renderFlags = renderFlags;
    }
    return instance;
}

// 0x27d00 ParticleStructure::GetID() const
int ParticleStructure::GetID() const
{
    return 2;
}

// 0xb4d0 ParticleStructure::IsOfType(int) const
bool ParticleStructure::IsOfType(int id) const
{
    return id == 2 || id == 1;
}

// 0x53950 ParticleStructure::AddAttachmentPoint(AttachmentPoint*, bool)
bool ParticleStructure::AddAttachmentPoint(AttachmentPoint* point, bool matchHost)
{
    Particle* const attachment = reinterpret_cast<Particle*>(point);
    void* const hostName = *reinterpret_cast<void**>(reinterpret_cast<char*>(point) + 0x12c);
    if (matchHost && hostName != name.node) {
        Particle* node = _head;
        while (node) {
            if (node->name.node == hostName) {
                node->Add(attachment);
                return true;
            }
            node = WalkNext(node, this);
        }
        return false;
    }
    Add(attachment);
    return true;
}

// 0x539c0 ParticleStructure::EnumerateAttachmentPoints (RTTI dynamic_cast to AttachmentPoint: U9 scene)
bool ParticleStructure::EnumerateAttachmentPoints(FastDynamicArray<AttachmentPoint*, 13>& points)
{
    using Fn = bool(__cdecl*)(ParticleStructure*, FastDynamicArray<AttachmentPoint*, 13>&);
    static Fn fn = OrigProc<Fn>(
        "?EnumerateAttachmentPoints@ParticleStructure@Jet@@QAA_NAAV?$FastDynamicArray@PAVAttachmentPoint@Jet@@$0N@U?$"
        "Less@PAVAttachmentPoint@Jet@@PAV12@@2@@2@@Z");
    return fn ? fn(this, points) : false;
}

bool ParticleStructure::EnumerateAttachmentPoints(FastDynamicArray<const AttachmentPoint*, 13>& points) const
{
    using Fn = bool(__cdecl*)(const ParticleStructure*, FastDynamicArray<const AttachmentPoint*, 13>&);
    static Fn fn = OrigProc<Fn>(
        "?EnumerateAttachmentPoints@ParticleStructure@Jet@@QBA_NAAV?$FastDynamicArray@PBVAttachmentPoint@Jet@@$0N@U?$"
        "Less@PBVAttachmentPoint@Jet@@PBV12@@2@@2@@Z");
    return fn ? fn(this, points) : false;
}

// 0xb4f0 ParticleStructure::AddReferencePoint(const ReferencePoint&)
void ParticleStructure::AddReferencePoint(const ReferencePoint& point)
{
    referencePoints.Add(point);
}

// 0xb510 ParticleStructure::GetReferencePointList()
ReferencePointList& ParticleStructure::GetReferencePointList()
{
    return referencePoints;
}

const ReferencePointList& ParticleStructure::GetReferencePointList() const
{
    return referencePoints;
}

// 0xb520 ParticleStructure::SetReferencePointList(const ReferencePointList&)
void ParticleStructure::SetReferencePointList(const ReferencePointList& list)
{
    referencePoints = list;
}

// 0x53a30 ParticleStructure::SetCurrentReferencePoint(const PString&) (needs the scene unit's pose maths)
bool ParticleStructure::SetCurrentReferencePoint(const PString& name_)
{
    using Fn = bool(__cdecl*)(ParticleStructure*, const PString&);
    static Fn fn = OrigProc<Fn>("?SetCurrentReferencePoint@ParticleStructure@Jet@@QAA_NABVPString@2@@Z");
    return fn ? fn(this, name_) : false;
}

// 0x53cf0 ParticleStructure::InitInstance(ParticleStructure&) const
void ParticleStructure::InitInstance(ParticleStructure& instance) const
{
    Particle::InitInstance(instance);
    instance.renderFlags = renderFlags;
    instance.referencePoints.AppendChunks(referencePoints);
}

}  // namespace Jet
