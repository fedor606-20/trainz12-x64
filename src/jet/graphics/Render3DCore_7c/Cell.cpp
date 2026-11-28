// Unit U9b of Render3DCore_7c: Cell.
//
// Cell = Particle + own CellNode* + particle count + a traversal ring + an Array<CellNode*>. See
// Render3DCoreScene.h. This file owns construction, copying, IDs, extents and the node-tree destructor; the
// statistics block and the particle-tree operations (Add/Remove/ObjectMoved/InsertParticle/RemoveParticle) are in
// U9bRem.cpp, and Cell::Cull/AddAllToVisible there still run the original's octree body by RVA. The destructor
// frees the node tree itself so the Particle destructor still runs exactly once.
#include <jet/Render3DCoreBounds.h>
#include <jet/Render3DCoreScene.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

float X87Copy(float f)
{
    __asm {
        fld dword ptr f
        fstp dword ptr f
    }
    return f;
}

// 0x1001bc70: the CellNode tree's destructor. The two child nodes at +4/+8 are the cell tree; the chunk ring at
// +0x18 is the node's FastDynamicArray<Particle*,13> (the particles themselves belong to the scene, not the node).
void DestroyNode(void* raw)
{
    char* const node = static_cast<char*>(raw);
    void* const left = *reinterpret_cast<void**>(node + 4);
    if (left) {
        DestroyNode(left);
        Alloc::Delete(left);
    }
    void* const right = *reinterpret_cast<void**>(node + 8);
    if (right) {
        DestroyNode(right);
        Alloc::Delete(right);
    }
    void* const head = *reinterpret_cast<void**>(node + 0x18);
    if (head) {
        void* c = *reinterpret_cast<void**>(head);
        while (c != head) {
            void* const next = *reinterpret_cast<void**>(c);
            Alloc::Delete(c);
            c = next;
        }
        Alloc::Delete(head);
        *reinterpret_cast<void**>(node + 0x18) = nullptr;
    }
}

// 0x1002ef90: Array<T,SIZ>::operator=, the three-word {count, capacity, data} block. It keeps our block when it is
// large enough and copies only the live items.
void CopyArray(void* destRaw, const void* srcRaw)
{
    char* const dest = static_cast<char*>(destRaw);
    const char* const src = static_cast<const char*>(srcRaw);
    *reinterpret_cast<unsigned int*>(dest) = 0;
    const void* const srcData = *reinterpret_cast<void* const*>(src + 8);
    if (!srcData) return;
    const unsigned int srcCapacity = *reinterpret_cast<const unsigned int*>(src + 4);
    if (srcCapacity > *reinterpret_cast<unsigned int*>(dest + 4)) {
        void* const old = *reinterpret_cast<void**>(dest + 8);
        if (old) Alloc::Delete(old);
        *reinterpret_cast<void**>(dest + 8) = Alloc::New(srcCapacity * 4);
        *reinterpret_cast<unsigned int*>(dest + 4) = srcCapacity;
    }
    const unsigned int srcCount = *reinterpret_cast<const unsigned int*>(src);
    *reinterpret_cast<unsigned int*>(dest) = srcCount;
    if (!srcCount) return;
    std::memcpy(*reinterpret_cast<void**>(dest + 8), srcData, srcCount * 4);
}

}  // namespace

namespace {

// 0x1001bc50: the cell's private bounding volume. It overrides only RecalculateAxisAlignedBoxVolume: the cell's
// own node holds the extents, so the local box is copied from node +0x30..0x44 and dirty bit 3 is cleared.
class CellAxisAlignedBoundingBox : public ParticleAxisAlignedBoundingBox {
public:
    explicit CellAxisAlignedBoundingBox(Particle* host_) : ParticleAxisAlignedBoundingBox(host_) {}

    void __cdecl RecalculateAxisAlignedBoxVolume() const override;
};

void CellAxisAlignedBoundingBox::RecalculateAxisAlignedBoxVolume() const
{
    CellAxisAlignedBoundingBox* const self = const_cast<CellAxisAlignedBoundingBox*>(this);
    const char* const cell = reinterpret_cast<const char*>(host);
    const char* const node = *reinterpret_cast<char* const*>(cell + 0x12c);
    if (node) std::memcpy(&self->axisAlignedBox.min.x, node + 0x30, 6 * sizeof(float));
    self->dirtyFlags &= ~8u;
}

}  // namespace

Cell::Cell() : Particle()
{
    char* const n = static_cast<char*>(Alloc::New(0x4c));
    if (n) {
        *reinterpret_cast<void**>(n + 0x04) = nullptr;
        *reinterpret_cast<void**>(n + 0x08) = nullptr;
        *reinterpret_cast<Cell**>(n + 0x0c) = this;
        *reinterpret_cast<unsigned int*>(n + 0x14) = 0xffffffffu;
        *reinterpret_cast<void**>(n + 0x18) = nullptr;
        n[0x48] = static_cast<char>(n[0x48] & 0xfe);
        *reinterpret_cast<void**>(n + 0x1c) = nullptr;
        *reinterpret_cast<void**>(n + 0x24) = nullptr;
        *reinterpret_cast<unsigned int*>(n + 0x28) = 3;
    }
    node = reinterpret_cast<CellNode*>(n);
    particleCount = 0;
    ringCount = 0;
    ringCapacity = 0;
    ringData = nullptr;
    ringRead = 0;
    ringWrite = 0;
    arrayCount = 0;
    arrayCapacity = 0;
    arrayData = nullptr;

    // the original counts the cell in its stats block here
    ++reinterpret_cast<unsigned int*>(&stats)[STAT_TOTAL_CELLS];
    ++reinterpret_cast<unsigned int*>(&stats)[STAT_TOTAL_PARTICLES];

    if (n) {
        *reinterpret_cast<void**>(n + 0x20) = nullptr;
        std::memset(n + 0x30, 0, 6 * sizeof(float));
        *reinterpret_cast<void**>(n) = nullptr;
    }

    void* const memory = Alloc::New(sizeof(CellAxisAlignedBoundingBox));
    if (memory) {
        CellAxisAlignedBoundingBox* const volume = ::new (memory) CellAxisAlignedBoundingBox(this);
        SetBoundingVolume(volume);
    }
}

Cell::Cell(const Cell& o) : Particle(o)
{
    node = o.node;
    particleCount = o.particleCount;
    unknown134 = o.unknown134;
    ringCount = o.ringCount;
    ringCapacity = o.ringCapacity;
    ringData = o.ringData;
    ringRead = o.ringRead;
    ringWrite = o.ringWrite;
    arrayCount = 0;
    arrayCapacity = 0;
    arrayData = nullptr;
    CopyArray(&arrayCount, &o.arrayCount);
}

Cell& Cell::operator=(const Cell& o)
{
    Particle::operator=(o);
    node = o.node;
    particleCount = o.particleCount;
    unknown134 = o.unknown134;
    ringCount = o.ringCount;
    ringCapacity = o.ringCapacity;
    ringData = o.ringData;
    ringRead = o.ringRead;
    ringWrite = o.ringWrite;
    CopyArray(&arrayCount, &o.arrayCount);
    return *this;
}

Cell::~Cell()
{
    if (node) {
        DestroyNode(node);
        Alloc::Delete(node);
        node = nullptr;
    }
    arrayCount = 0;
    if (arrayData) {
        Alloc::Delete(arrayData);
        arrayData = nullptr;
    }
    if (ringData) {
        Alloc::Delete(ringData);
        ringData = nullptr;
    }
}

// 0x1bf70: the extents live in the cell's own node, six fld/fstp copies
AxisAlignedBox __cdecl Cell::GetExtents() const
{
    AxisAlignedBox box;
    const char* const n = reinterpret_cast<const char*>(node);
    box.min.x = X87Copy(*reinterpret_cast<const float*>(n + 0x30));
    box.min.y = X87Copy(*reinterpret_cast<const float*>(n + 0x34));
    box.min.z = X87Copy(*reinterpret_cast<const float*>(n + 0x38));
    box.max.x = X87Copy(*reinterpret_cast<const float*>(n + 0x3c));
    box.max.y = X87Copy(*reinterpret_cast<const float*>(n + 0x40));
    box.max.z = X87Copy(*reinterpret_cast<const float*>(n + 0x44));
    return box;
}

int __cdecl Cell::GetID() const
{
    return 12;
}

bool __cdecl Cell::IsOfType(int id) const
{
    return id == 12 || id == 1;
}

// 0x73920: the original's shared empty RenderDebug
void __cdecl Cell::RenderDebug(Display*, DebugViewPort&, Matrix4&, unsigned long) {}


}  // namespace Jet

