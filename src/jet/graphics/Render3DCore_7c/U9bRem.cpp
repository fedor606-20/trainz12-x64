// Unit U9b (wave 4): the remaining scene exports. ReferencePoint members, the Cell statistics block and the
// CellNode particle-tree operations, Portal Enter/Leave, the PhysicalPortal/Mirror cull and camera state.
//
// The octree split/merge bodies of Cell::Cull/AddAllToVisible and the display-stencil bodies of
// PhysicalPortal::Enter/Leave/Traverse plus the two virtual-portal camera builders still call the original's
// private implementation by RVA: they depend on the unexported CellNode split helpers
// 0x1ce50/0x1cef0/0x1d290/0x1d350/0x1d850 and on the display state block save/restore. Those seven exports are
// local definitions that bridge to the original body; everything else here is a transcription.
#include <jet/Render3DCoreScene.h>

#include <jet/FastDynamicArray.h>
#include <jet/Geometry.h>
#include <jet/Render3DCoreBounds.h>
#include <jet/Render3DCoreCamera.h>
#include <jet/RenderSystem.h>

#include <windows.h>

#include <cstring>

// The exported `stats` pointer is the first word of a 19-word block the original walks as an array; the static
// bounds checker sees only the 4-byte pointer and warns on the deliberate indexed writes.
#pragma warning(disable : 4789)

namespace Jet {

// The scene code only needs the camera pointer and the two calls it makes on a view point.
class ViewPoint {
public:
    void __cdecl AddPortal(Portal* portal, const Matrix4& m, unsigned int flags);
    void __cdecl Update(Camera* camera);

    unsigned char unknown0[4];  // +0x00
    Camera* camera;             // +0x04
};

// ---- CellNode --------------------------------------------------------------------------------------------
//
// 0x4c bytes: parent, two children, the owning cell, the split axis/value, the particle chunk list
// (FastDynamicArray<Particle*,13> at +0x18), subtree counters, cull state and the node's AABB.
class CellNode {
public:
    CellNode* parent;      // +0x00
    CellNode* child[2];    // +0x04, +0x08
    Cell* cell;            // +0x0c
    float splitValue;      // +0x10
    int axis;              // +0x14  -1 for a leaf
    FastDynamicArray<Particle*, 13> particles;  // +0x18
    unsigned int depth;    // +0x1c
    unsigned int subCount; // +0x20
    unsigned int count;    // +0x24
    int cullState;         // +0x28
    float unknown2c;       // +0x2c
    float min[3];          // +0x30
    float max[3];          // +0x3c
    unsigned char flags;   // +0x48
    unsigned char padding[3];
};
JET_LAYOUT32(sizeof(CellNode) == 0x4c);

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

void* OrigBase()
{
    static void* base = static_cast<void*>(OrigModule());
    return base;
}

template <class F>
F OrigRva(unsigned int rva)
{
    return reinterpret_cast<F>(static_cast<char*>(OrigBase()) + rva);
}

float X87Copy(float f)
{
    __asm {
        fld dword ptr f
        fstp dword ptr f
    }
    return f;
}

CellNode*& NodeOf(Particle* p)
{
    return *reinterpret_cast<CellNode**>(reinterpret_cast<char*>(p) + 0x24);
}

ParticleBoundingVolume* VolumeOf(Particle* p)
{
    return *reinterpret_cast<ParticleBoundingVolume**>(reinterpret_cast<char*>(p) + 0x4c);
}

// 0x1d1c0: CellNode::RemoveParticle(node in ESI, particle on the stack, body address as the third argument).
void __declspec(naked) R3DCellNodeRemove(CellNode*, Particle*, void*)
{
    __asm {
        push esi
        mov esi, dword ptr [esp + 8]
        push dword ptr [esp + 0xc]
        mov eax, dword ptr [esp + 0x14]
        call eax
        add esp, 4
        pop esi
        ret
    }
}

// 0x1d760: widen a leaf node's AABB with the particle's current axis-aligned bounding box.
void GrowNodeBounds(CellNode* node, Particle* particle)
{
    ParticleBoundingVolume* const volume = VolumeOf(particle);
    if (volume->dirtyFlags & 8) volume->RecalculateAxisAlignedBoxVolume();
    const AxisAlignedBox& box = volume->axisAlignedBox;
    if (node->count != 0) {
        if (node->min[0] > box.min.x) node->min[0] = box.min.x;
        if (node->min[1] > box.min.y) node->min[1] = box.min.y;
        if (node->min[2] > box.min.z) node->min[2] = box.min.z;
        if (box.max.x > node->max[0]) node->max[0] = box.max.x;
        if (box.max.y > node->max[1]) node->max[1] = box.max.y;
        if (box.max.z > node->max[2]) node->max[2] = box.max.z;
    } else {
        node->min[0] = X87Copy(box.min.x);
        node->min[1] = X87Copy(box.min.y);
        node->min[2] = X87Copy(box.min.z);
        node->max[0] = X87Copy(box.max.x);
        node->max[1] = X87Copy(box.max.y);
        node->max[2] = X87Copy(box.max.z);
    }
}

void CallOnChild(ParticleBoundingVolume* volume, Particle* particle, int slot)
{
    using Fn = void(__cdecl*)(ParticleBoundingVolume*, Particle*);
    Fn* const vtable = *reinterpret_cast<Fn**>(volume);
    vtable[slot](volume, particle);
}

void RemoveFromNode(CellNode* node, Particle* particle)
{
    R3DCellNodeRemove(node, particle, OrigRva<void*>(0x1d1c0));
}

// 0x1d850: rebuild a node's AABB from its particles (cdecl, stack argument).
void RebuildNodeBounds(CellNode* node)
{
    OrigRva<void(__cdecl*)(CellNode*)>(0x1d850)(node);
}

RenderingCacheManager* CacheManagerOf(ViewPort& port)
{
    RenderKernel* const kernel = *reinterpret_cast<RenderKernel**>(reinterpret_cast<char*>(&port) + 0x18);
    return reinterpret_cast<RenderingCacheManager*>(reinterpret_cast<char*>(kernel) + 0x6c);
}

void IdentityMatrix(Matrix4& m)
{
    for (int i = 0; i < 16; ++i) m.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

}  // namespace

// ---- Cell statistics -------------------------------------------------------------------------------------
//
// The original exports `stats` as a data symbol and treats the 19 words that follow it as one contiguous block;
// index 0 is the live-particle counter, 1/2 the cell counters, 3..18 the cull statistics. The block must be
// contiguous, so both parts live in one ordered section.
#pragma data_seg(".data$r3dcstats")
unsigned int* Cell::stats = nullptr;
unsigned int g_cellStats[18] = {};
#pragma data_seg()

unsigned int __cdecl Cell::GetStat(Stats stat)
{
    return reinterpret_cast<unsigned int*>(&stats)[stat];
}

const unsigned int* __cdecl Cell::GetStats()
{
    return reinterpret_cast<const unsigned int*>(&stats);
}

void __cdecl Cell::Reset()
{
    unsigned int* const block = reinterpret_cast<unsigned int*>(&stats);
    for (int i = 3; i < static_cast<int>(STAT_COUNT); ++i) block[i] = 0;
}

void __cdecl Cell::ResetAll()
{
    unsigned int* const block = reinterpret_cast<unsigned int*>(&stats);
    for (int i = 0; i < static_cast<int>(STAT_COUNT); ++i) block[i] = 0;
}

// ---- Cell particle-tree operations -----------------------------------------------------------------------

CellNode* __cdecl Cell::InsertParticle(Particle* particle)
{
    CellNode* const node = this->node;
    CallOnChild(VolumeOf(particle), particle, 6);  // OnChildAdded (slot 6)
    if (node->axis < 0) GrowNodeBounds(node, particle);
    node->particles.Add(particle);
    ++node->count;
    ++particleCount;
    ++reinterpret_cast<unsigned int*>(&stats)[0];
    return node;
}

void __cdecl Cell::RemoveParticle(Particle* particle, CellNode* node)
{
    RemoveFromNode(node, particle);
    CallOnChild(VolumeOf(particle), particle, 5);  // OnChildRemoved (slot 5)
    if (this->node->axis < 0) RebuildNodeBounds(this->node);
    --particleCount;
    --reinterpret_cast<unsigned int*>(&stats)[0];
}

void __cdecl Cell::Add(Particle* particle)
{
    if (NodeOf(particle)) {
        Cell* const oldCell = NodeOf(particle)->cell;
        if (oldCell == this) return;
        oldCell->RemoveParticle(particle, NodeOf(particle));
    }
    if (particle) NodeOf(particle) = InsertParticle(particle);
    else NodeOf(particle) = nullptr;
}

bool __cdecl Cell::Remove(Particle* particle)
{
    Cell* owner = nullptr;
    CellNode* const node = NodeOf(particle);
    if (node) {
        owner = node->cell;
        if (owner) RemoveParticle(particle, node);
    }
    NodeOf(particle) = nullptr;
    return owner == this;
}

CellNode* __cdecl Cell::ObjectMoved(Particle* particle, CellNode* oldNode)
{
    CallOnChild(VolumeOf(particle), particle, 5);  // OnChildRemoved (slot 5)
    if (this->node->axis < 0) GrowNodeBounds(this->node, particle);
    if (oldNode != this->node) {
        RemoveFromNode(oldNode, particle);
        this->node->particles.Add(particle);
        ++this->node->count;
    }
    return this->node;
}

// ---- Portal Enter/Leave ----------------------------------------------------------------------------------

void __cdecl Portal::Enter(Display* display, ViewPort& port, Matrix4&, unsigned int, unsigned int)
{
    ++enterCount;
    CacheManagerOf(port)->Enter(display, &port, GetStencil());
}

void __cdecl Portal::Leave(Display* display, ViewPort& port, Matrix4&, unsigned int, unsigned int)
{
    --enterCount;
    CacheManagerOf(port)->Leave(display, &port, GetStencil());
}

// ---- PhysicalPortal camera/cull state --------------------------------------------------------------------

// 0x182a0: Camera(const Camera& source, const Vector3* position, const Matrix4& warp), an exported constructor
// that U8 has not moved yet. Call it through the original so the import table stays free of names we export.
Camera* BuildWarpedCamera(void* memory, Camera* source, const Vector3* position, const Matrix4& warp)
{
    using Fn = Camera*(__thiscall*)(void*, const Camera*, const Vector3*, const Matrix4*);
    static Fn fn = OrigProc<Fn>("??0Camera@Jet@@QAE@ABV01@PBVVector3@1@ABVMatrix4@1@@Z");
    return fn(memory, source, position, &warp);
}

bool __cdecl PhysicalPortal::IsBackfacing(const Camera* camera) const
{
    Matrix4 view;
    std::memset(&view, 0, sizeof view);
    view.m[0] = view.m[5] = view.m[10] = view.m[15] = 1.0f;
    Matrix4 model;
    this->GetModelView(model, view);

    const Vector3& portalPos = GetPosition();
    const Vector3& cameraPos = *reinterpret_cast<const Vector3*>(reinterpret_cast<const char*>(camera) + 0x2c);

    const float f2 = BoundsDetail::SubSS(BoundsDetail::MulSS(BoundsDetail::AddSS(model.m[8], model.m[0]), 0.0f),
                                         BoundsDetail::MulSS(model.m[4], 1.0f));
    const float f3 = BoundsDetail::SubSS(BoundsDetail::MulSS(BoundsDetail::AddSS(model.m[9], model.m[1]), 0.0f),
                                         BoundsDetail::MulSS(model.m[5], 1.0f));
    const float f4 = BoundsDetail::SubSS(BoundsDetail::MulSS(BoundsDetail::AddSS(model.m[10], model.m[2]), 0.0f),
                                         BoundsDetail::MulSS(model.m[6], 1.0f));
    const float dz = BoundsDetail::MulSS(BoundsDetail::SubSS(cameraPos.z, portalPos.z), f4);
    const float dy = BoundsDetail::MulSS(BoundsDetail::SubSS(cameraPos.y, portalPos.y), f3);
    const float dx = BoundsDetail::MulSS(BoundsDetail::SubSS(cameraPos.x, portalPos.x), f2);
    const float dot = BoundsDetail::AddSS(BoundsDetail::AddSS(dz, dy), dx);
    return dot < 0.0f;
}

void __cdecl PhysicalPortal::Cull(ViewPoint& view, const Matrix4& m)
{
    if (nonReenterant) {
        const unsigned long stamp = ViewPort::GetTraversalNumber();
        if (traversalStamp == stamp) return;
        traversalStamp = stamp;
    }
    const Camera* const camera = *reinterpret_cast<Camera* const*>(reinterpret_cast<char*>(&view) + 4);
    if (!IsBackfacing(camera)) {
        view.AddPortal(this, m, 0);
        Particle::Cull(view, m);
    }
}

Camera* __cdecl PhysicalPortal::CreateCamera(Camera* camera, const Vector3* position, void* data,
                                             Matrix4& warp) const
{
    IdentityMatrix(warp);
    const CellNode* const ownNode = *reinterpret_cast<CellNode* const*>(reinterpret_cast<const char*>(this) + 0x24);
    const Cell* const source = ownNode ? ownNode->cell : nullptr;
    if (source) {
        const Vector3& p = *reinterpret_cast<const Vector3*>(reinterpret_cast<const char*>(source) + 0x2c);
        const float y0 = BoundsDetail::MulSS(p.y, 0.0f);
        const float x0 = BoundsDetail::MulSS(p.x, 0.0f);
        const float z0 = BoundsDetail::MulSS(p.z, 0.0f);
        warp.m[12] = BoundsDetail::AddSS(BoundsDetail::AddSS(p.x, z0), y0);
        warp.m[13] = BoundsDetail::AddSS(BoundsDetail::AddSS(p.y, x0), z0);
        warp.m[14] = BoundsDetail::AddSS(BoundsDetail::AddSS(p.z, x0), y0);
    }
    const Vector3& d = *reinterpret_cast<const Vector3*>(reinterpret_cast<const char*>(destination) + 0x2c);
    const float nx = BoundsDetail::SubSS(-0.0f, d.x);
    const float ny = BoundsDetail::SubSS(-0.0f, d.y);
    const float nz = BoundsDetail::SubSS(-0.0f, d.z);
    const float ny0 = BoundsDetail::MulSS(ny, 0.0f);
    const float nx0 = BoundsDetail::MulSS(nx, 0.0f);
    const float nz0 = BoundsDetail::MulSS(nz, 0.0f);
    // the original adds the z term before the y term
    warp.m[12] = BoundsDetail::AddSS(BoundsDetail::AddSS(BoundsDetail::AddSS(nx, nz0), ny0), warp.m[12]);
    warp.m[13] = BoundsDetail::AddSS(BoundsDetail::AddSS(BoundsDetail::AddSS(ny, nx0), nz0), warp.m[13]);
    warp.m[14] = BoundsDetail::AddSS(BoundsDetail::AddSS(BoundsDetail::AddSS(nz, nx0), ny0), warp.m[14]);
    if (!data) return nullptr;
    return BuildWarpedCamera(data, camera, position, warp);
}

// ---- VirtualPortal / Mirror state ------------------------------------------------------------------------

VirtualPortal::VirtualPortal(Cell* destinationCell, const Vector2& portalSize)
    : PhysicalPortal(destinationCell, portalSize), warpMatrix()
{
    IdentityMatrix(warpMatrix);
}

VirtualPortal::VirtualPortal(const VirtualPortal& o)
    : PhysicalPortal(o), warpMatrix(o.warpMatrix), reference(o.reference)
{
}

VirtualPortal::~VirtualPortal() {}

VirtualPortal& VirtualPortal::operator=(const VirtualPortal& o)
{
    PhysicalPortal::operator=(o);
    warpMatrix = o.warpMatrix;
    reference = o.reference;
    return *this;
}

int __cdecl VirtualPortal::GetID() const
{
    return 15;
}

bool __cdecl VirtualPortal::IsOfType(int id) const
{
    return id == 15 || id == 14 || id == 13 || id == 1;
}

Particle* __cdecl VirtualPortal::GetReference()
{
    return reference;
}

const Particle* __cdecl VirtualPortal::GetReference() const
{
    return reference;
}

void __cdecl VirtualPortal::SetReference(Particle* value)
{
    reference = value;
}

void __cdecl VirtualPortal::SetWarpMatrix(const Matrix4& warp)
{
    warpMatrix = warp;
}

Mirror::Mirror(Cell* destinationCell, const Vector2& portalSize) : VirtualPortal(destinationCell, portalSize) {}

Mirror::Mirror(const Mirror& o) : VirtualPortal(o) {}

Mirror::~Mirror() {}

Mirror& Mirror::operator=(const Mirror& o)
{
    VirtualPortal::operator=(o);
    return *this;
}

int __cdecl Mirror::GetID() const
{
    return 16;
}

bool __cdecl Mirror::IsOfType(int id) const
{
    return id == 16 || id == 15 || id == 14 || id == 13 || id == 1;
}

// 0x73920 / 0x79e0: the original's shared empty SetReference and "return 0" GetReference
void __cdecl Mirror::SetReference(Particle*) {}
Particle* __cdecl Mirror::GetReference()
{
    return nullptr;
}
const Particle* __cdecl Mirror::GetReference() const
{
    return nullptr;
}

void __cdecl Mirror::Cull(ViewPoint& view, const Matrix4& m)
{
    const Camera* const camera = *reinterpret_cast<Camera* const*>(reinterpret_cast<char*>(&view) + 4);
    if (!IsBackfacing(camera)) {
        view.AddPortal(this, m, 1);
        Particle::Cull(view, m);
    }
}

// ---- Corona's shared cache -------------------------------------------------------------------------------

ParticleFXCache* Corona::cache = nullptr;

}  // namespace Jet

// ---- ReferencePoint --------------------------------------------------------------------------------------
//
// The class body is U9a's header and its members are inline there. The exports are emitted as a helper class
// whose member names are unique (the .def aliases the original names onto them), so the original's
// uninitialised default constructor and its x87 member copies are preserved exactly.

namespace Jet {

namespace {

PStringCache* StringCache()
{
    return PString::Cache();
}

void DestroyName(PString& name)
{
    PStringCache* const cache = StringCache();
    if (name.node && cache) cache->Destroy(name.node);
}

void CopyName(PString& dst, const PString* src)
{
    DestroyName(dst);
    PStringCache* const cache = StringCache();
    if (src->node && cache) {
        cache->Lock();
        ++src->node->refs;
        ++cache->liveRefs;
        cache->Unlock();
    }
    dst.node = src->node;
}

}  // namespace

// Unique member names: the .def fragment maps the original ReferencePoint exports onto them.
struct U9bRemRefPoint {
    void __thiscall DefaultCtor();
    void __thiscall FullCtor(const PString& name, const Vector3& position, const Quaternion& orientation);
    void __thiscall CopyCtor(const ReferencePoint& source);
    ReferencePoint* __thiscall Assign(const ReferencePoint& source);
    void __thiscall Dtor();

    static PString* __cdecl GetName(ReferencePoint* self);
    static Vector3* __cdecl GetPosition(ReferencePoint* self);
    static Quaternion* __cdecl GetOrientation(ReferencePoint* self);
    static void __cdecl SetName(ReferencePoint* self, const PString& name);
    static void __cdecl SetPosition(ReferencePoint* self, const Vector3& position);
    static void __cdecl SetOrientation(ReferencePoint* self, const Quaternion& orientation);
};

void __thiscall U9bRemRefPoint::DefaultCtor()
{
    // the original clears only the name; position/orientation stay uninitialised
    reinterpret_cast<ReferencePoint*>(this)->name.node = nullptr;
}

void __thiscall U9bRemRefPoint::FullCtor(const PString& name, const Vector3& position,
                                         const Quaternion& orientation)
{
    ReferencePoint* const self = reinterpret_cast<ReferencePoint*>(this);
    self->name.node = nullptr;
    CopyName(self->name, &name);
    const float* const p = reinterpret_cast<const float*>(&position);
    const float* const q = reinterpret_cast<const float*>(&orientation);
    self->position.x = X87Copy(p[0]);
    self->position.y = X87Copy(p[1]);
    self->position.z = X87Copy(p[2]);
    self->orientation.x = X87Copy(q[0]);
    self->orientation.y = X87Copy(q[1]);
    self->orientation.z = X87Copy(q[2]);
    self->orientation.w = X87Copy(q[3]);
}

void __thiscall U9bRemRefPoint::CopyCtor(const ReferencePoint& source)
{
    ReferencePoint* const self = reinterpret_cast<ReferencePoint*>(this);
    // a constructor: the destination name is uninitialised and must not be released
    PStringCache* const cache = StringCache();
    if (source.name.node && cache) {
        cache->Lock();
        ++source.name.node->refs;
        ++cache->liveRefs;
        cache->Unlock();
    }
    self->name.node = source.name.node;
    self->position.x = source.position.x;
    self->position.y = source.position.y;
    self->position.z = source.position.z;
    self->orientation.x = X87Copy(source.orientation.x);
    self->orientation.y = X87Copy(source.orientation.y);
    self->orientation.z = X87Copy(source.orientation.z);
    self->orientation.w = X87Copy(source.orientation.w);
}

ReferencePoint* __thiscall U9bRemRefPoint::Assign(const ReferencePoint& source)
{
    ReferencePoint* const self = reinterpret_cast<ReferencePoint*>(this);
    DestroyName(self->name);
    PStringCache* const cache = StringCache();
    if (source.name.node && cache) {
        cache->Lock();
        ++source.name.node->refs;
        ++cache->liveRefs;
        cache->Unlock();
    }
    self->name.node = source.name.node;
    self->position.x = X87Copy(source.position.x);
    self->position.y = X87Copy(source.position.y);
    self->position.z = X87Copy(source.position.z);
    self->orientation.x = X87Copy(source.orientation.x);
    self->orientation.y = X87Copy(source.orientation.y);
    self->orientation.z = X87Copy(source.orientation.z);
    self->orientation.w = X87Copy(source.orientation.w);
    return self;
}

void __thiscall U9bRemRefPoint::Dtor()
{
    DestroyName(reinterpret_cast<ReferencePoint*>(this)->name);
}

PString* __cdecl U9bRemRefPoint::GetName(ReferencePoint* self)
{
    return &self->name;
}

Vector3* __cdecl U9bRemRefPoint::GetPosition(ReferencePoint* self)
{
    return &self->position;
}

Quaternion* __cdecl U9bRemRefPoint::GetOrientation(ReferencePoint* self)
{
    return &self->orientation;
}

void __cdecl U9bRemRefPoint::SetName(ReferencePoint* self, const PString& name)
{
    CopyName(self->name, &name);
}

void __cdecl U9bRemRefPoint::SetPosition(ReferencePoint* self, const Vector3& position)
{
    const float* const p = reinterpret_cast<const float*>(&position);
    self->position.x = X87Copy(p[0]);
    self->position.y = X87Copy(p[1]);
    self->position.z = X87Copy(p[2]);
}

void __cdecl U9bRemRefPoint::SetOrientation(ReferencePoint* self, const Quaternion& orientation)
{
    const float* const q = reinterpret_cast<const float*>(&orientation);
    self->orientation.x = X87Copy(q[0]);
    self->orientation.y = X87Copy(q[1]);
    self->orientation.z = X87Copy(q[2]);
    self->orientation.w = X87Copy(q[3]);
}

}  // namespace Jet
