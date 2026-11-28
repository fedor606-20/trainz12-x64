// Render3DCore_7c unit U9b: the scene-graph classes around the particle tree.
//
// Layouts and behaviour follow the TS12 assembly (notes/agents/r3d_workstreams/u9b_scene_asm.txt). Ordinary members are
// __cdecl (this on the stack, as in the rest of this module); constructors, assignments and destructors are
// __thiscall. The classes extend U9a's Particle with the original's own fields, so the exported vtables keep the
// original's slot numbering: Portal's Traverse/Enter/Leave/CreateCamera start at slot 48, and Cell's ObjectMoved is
// its only new slot.
//
// The private pieces that have no exported entry point still run the original's implementation by RVA:
//   - Cell's constructor allocates a Cell::CellAxisAlignedBoundingBox, a U3 particle-bounding-volume subclass that
//     has no exported constructor; the Cell constructor therefore runs the original constructor on `this` and only
//     restores our vftable afterwards.
//   - Cell::Cull/AddAllToVisible (the octree split/rebalance helpers) and PhysicalPortal::Enter/Leave/Traverse plus
//     the VirtualPortal/Mirror camera builders; these are local exports that bridge to the original bodies.
// Cell's stats block and its Add/Remove/InsertParticle/RemoveParticle/ObjectMoved operations are local (U9bRem.cpp).
#pragma once

#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/Render3DCoreParticle.h>
#include <jet/RenderTypes.h>

namespace Jet {

class AttachmentObserver;
class Camera;
class CellNode;
class DebugViewPort;
class Display;
class Matrix4;
class Particle;
class ParticleFXCache;
class ViewPoint;
class ViewPort;

// ---- AttachmentPoint / AttachmentObserver (0x134 / 8 bytes) ------------------------------------------------
//
// AttachmentPoint = Particle + host-name PString (+0x12c) + FastDynamicArray<AttachmentObserver*,13> (+0x130).
// The observer callbacks are the original's four new slots after Particle's 48: AddObserver, RemoveObserver,
// ClearObservers and NotifyObservers. The Stencil byte of a Particle is shared with a ViewPortParticle alias count in
// the original headers; the scene code never reads it.

class AttachmentObserver {
public:
    AttachmentObserver();
    explicit AttachmentObserver(AttachmentPoint* point);
    AttachmentObserver(const AttachmentObserver& o);
    AttachmentObserver& operator=(const AttachmentObserver& o);
    virtual ~AttachmentObserver();

    // The original's slots 1..3 are one shared "xor eax,eax; ret" stub; derived observers override them.
    virtual void __cdecl Added(Particle* particle);
    virtual void __cdecl Removed(Particle* particle);
    virtual void __cdecl VirtualShutDown();

    AttachmentPoint* point;  // +0x04

    JET_HEAP_OBJECT

protected:
    void __cdecl ShutDown();  // exported: clears point, then calls slot 3
};
JET_LAYOUT32(sizeof(AttachmentObserver) == 8);

class AttachmentPoint : public Particle {
public:
    AttachmentPoint();
    AttachmentPoint(const AttachmentPoint& o);
    AttachmentPoint(const PString& name, const PString& hostName);
    AttachmentPoint(const PString& name, const PString& hostName, const Vector3& position,
                    const Quaternion& orientation);
    ~AttachmentPoint() override;
    AttachmentPoint& operator=(const AttachmentPoint& o);

    virtual void __cdecl AddObserver(AttachmentObserver* observer);      // slot 48
    virtual bool __cdecl RemoveObserver(AttachmentObserver* observer);   // slot 49
    virtual void __cdecl ClearObservers();                               // slot 50
protected:
    virtual void __cdecl NotifyObservers(bool added, Particle* particle);  // slot 51

public:
    // Particle overrides
    virtual Particle* __cdecl CreateInstance() const override;
    virtual int __cdecl GetID() const override;
    virtual bool __cdecl IsOfType(int id) const override;
    virtual void __cdecl Add(Particle* child) override;
    virtual bool __cdecl Add(const PString& containerName, Particle* child) override;
    virtual void __cdecl Push(Particle* child) override;
    virtual Iterator __cdecl InsertBefore(Iterator& target, Particle* child) override;
    virtual Iterator __cdecl InsertAfter(Iterator& target, Particle* child) override;
    virtual Particle* __cdecl Remove() override;
    virtual bool __cdecl Remove(Particle* child) override;
    virtual Iterator __cdecl Remove(Iterator& target) override;
    virtual Particle* __cdecl Pop() override;

    const PString& __cdecl GetHostName() const;
    void __cdecl SetHostName(const PString& hostName);
    static void __cdecl GetSubNames(const PString& name, PString& first, PString& second);

protected:
    void __cdecl InitInstance(AttachmentPoint& instance) const;

private:
    PString hostName;                                  // +0x12c
    FastDynamicArray<AttachmentObserver*, 13> observers;  // +0x130
};
JET_LAYOUT32(sizeof(AttachmentPoint) == 0x134);

// ---- Cell (0x158 bytes) --------------------------------------------------------------------------------------
//
// Cell = Particle + the cell's own CellNode* (+0x12c), the particle count (+0x130), a traversal ring buffer
// {count, capacity, data, read, write} at +0x138..+0x148 and Array<CellNode*> at +0x14c..+0x154. The CellNode and
// the bounding volume are opaque here: the constructor is the original's, the destructor frees the node tree
// itself. The class only adds ObjectMoved as a new virtual slot (48).
class Cell : public Particle {
public:
    Cell();
    Cell(const Cell& o);
    ~Cell() override;
    Cell& operator=(const Cell& o);

    enum Stats : int {
        STAT_LIVE_PARTICLES = 0,
        STAT_TOTAL_CELLS = 1,
        STAT_TOTAL_PARTICLES = 2,
        STAT_CULL_NODES = 4,
        STAT_CULL_PLANE_TESTS = 11,
        STAT_CULL_PARTIAL = 12,
        STAT_COUNT = 19,
    };

    virtual void __cdecl Cull(ViewPoint& view, const Matrix4& m) override;
    virtual void __cdecl RenderDebug(Display* display, DebugViewPort& port, Matrix4& m, unsigned long flags) override;
    virtual void __cdecl Add(Particle* particle) override;         // slot 18
    virtual bool __cdecl Remove(Particle* particle) override;      // slot 24
    virtual int __cdecl GetID() const override;
    virtual bool __cdecl IsOfType(int id) const override;

    AxisAlignedBox __cdecl GetExtents() const;
    static unsigned int __cdecl GetStat(Stats stat);
    static const unsigned int* __cdecl GetStats();
    static void __cdecl Reset();
    static void __cdecl ResetAll();

private:
    virtual CellNode* __cdecl ObjectMoved(Particle* particle, CellNode* node);  // slot 48
    void __cdecl AddAllToVisible(CellNode* node, ViewPoint& view, const Matrix4& m, int flags);
    CellNode* __cdecl InsertParticle(Particle* particle);
    void __cdecl RemoveParticle(Particle* particle, CellNode* node);

    CellNode* node;             // +0x12c  the cell's own node
    unsigned int particleCount; // +0x130
    unsigned int unknown134;    // +0x134  copied, not touched by the constructor
    unsigned int ringCount;     // +0x138
    unsigned int ringCapacity;  // +0x13c
    void** ringData;            // +0x140
    unsigned int ringRead;      // +0x144
    unsigned int ringWrite;     // +0x148
    unsigned int arrayCount;    // +0x14c
    unsigned int arrayCapacity; // +0x150
    void** arrayData;           // +0x154

    static unsigned int* stats;
};
JET_LAYOUT32(sizeof(Cell) == 0x158);

// ---- Portal family -------------------------------------------------------------------------------------------
//
// Portal = Particle + the enter counter (+0x12c) + the stencil byte (+0x130), and four new virtual slots:
// Traverse, Enter, Leave, CreateCamera. PhysicalPortal adds the size (+0x134), the destination cell (+0x13c),
// a traversal stamp (+0x140) and the non-reenterant flag (+0x144). VirtualPortal adds the warp matrix (+0x148) and
// a reference particle (+0x188); Mirror is a VirtualPortal with its own vtable.

class Portal : public Particle {
public:
    Portal();
    Portal(const Portal& o);
    ~Portal() override;
    Portal& operator=(const Portal& o);

    virtual void __cdecl Traverse(ViewPoint& view, const Matrix4& m, Camera* camera);          // slot 48
    virtual void __cdecl Enter(Display* display, ViewPort& port, Matrix4& m, unsigned int a, unsigned int b);  // 49
    virtual void __cdecl Leave(Display* display, ViewPort& port, Matrix4& m, unsigned int a, unsigned int b);  // 50
protected:
    virtual Camera* __cdecl CreateCamera(Camera* camera, const Vector3* position, void* data,
                                         Matrix4& warp) const;  // slot 51
public:
    virtual int __cdecl GetID() const override;
    virtual bool __cdecl IsOfType(int id) const override;

    bool __cdecl GetStencil() const;
    static bool __cdecl GetEnableGlobalStencil();
    static void __cdecl SetEnableGlobalStencil(bool enable);
    static unsigned int __cdecl GetTraversed();
    static unsigned int __cdecl GetTotal();
    static void __cdecl Reset();

protected:
    void __cdecl SetStencil(bool stencil);

private:
    int enterCount;          // +0x12c
    unsigned char stencil;   // +0x130
    unsigned char padding131[3];

    static unsigned int total;
    static unsigned int traversed;
    static bool enableStencilMode;
    friend struct PortalStateAccess;
};
JET_LAYOUT32(sizeof(Portal) == 0x134);

// The counters are private because their decorated names must keep spelling them that way; U9b's bridges that
// still run the original body read them through here.
struct PortalStateAccess
{
    static unsigned int& Traversed() { return Portal::traversed; }
    static unsigned int& Total() { return Portal::total; }
    static bool& EnableStencilMode() { return Portal::enableStencilMode; }
};

class PhysicalPortal : public Portal {
public:
    PhysicalPortal(Cell* destination, const Vector2& size);
    PhysicalPortal(const PhysicalPortal& o);
    ~PhysicalPortal() override;
    PhysicalPortal& operator=(const PhysicalPortal& o);

    virtual void __cdecl Traverse(ViewPoint& view, const Matrix4& m, Camera* camera) override;
    virtual void __cdecl Cull(ViewPoint& view, const Matrix4& m);
    virtual void __cdecl Enter(Display* display, ViewPort& port, Matrix4& m, unsigned int a, unsigned int b) override;
    virtual void __cdecl Leave(Display* display, ViewPort& port, Matrix4& m, unsigned int a, unsigned int b) override;
    virtual int __cdecl GetID() const override;
    virtual bool __cdecl IsOfType(int id) const override;

    const Vector2& __cdecl GetSize() const;
    Cell* __cdecl GetDestination();
    const Cell* __cdecl GetDestination() const;
    bool __cdecl GetNonReenterant() const;
    void __cdecl SetNonReenterant(bool value);

protected:
    virtual Camera* __cdecl CreateCamera(Camera* camera, const Vector3* position, void* data,
                                         Matrix4& result) const override;
    bool __cdecl IsBackfacing(const Camera* camera) const;

private:
    Vector2 size;             // +0x134
    Cell* destination;        // +0x13c
    unsigned int traversalStamp;  // +0x140
    bool nonReenterant;       // +0x144
    unsigned char padding145[3];
};
JET_LAYOUT32(sizeof(PhysicalPortal) == 0x148);

class VirtualPortal : public PhysicalPortal {
public:
    VirtualPortal(Cell* destination, const Vector2& size);
    VirtualPortal(const VirtualPortal& o);
    ~VirtualPortal() override;
    VirtualPortal& operator=(const VirtualPortal& o);

    virtual int __cdecl GetID() const override;
    virtual bool __cdecl IsOfType(int id) const override;

    Particle* __cdecl GetReference();
    const Particle* __cdecl GetReference() const;
    void __cdecl SetReference(Particle* reference);

private:
    void __cdecl SetWarpMatrix(const Matrix4& warp);

protected:
    virtual Camera* __cdecl CreateCamera(Camera* camera, const Vector3* position, void* data,
                                         Matrix4& result) const override;

private:
    Matrix4 warpMatrix;       // +0x148
    Particle* reference;      // +0x188
};
JET_LAYOUT32(sizeof(VirtualPortal) == 0x18c);

class Mirror : public VirtualPortal {
public:
    Mirror(Cell* destination, const Vector2& size);
    Mirror(const Mirror& o);
    ~Mirror() override;
    Mirror& operator=(const Mirror& o);

    virtual void __cdecl Cull(ViewPoint& view, const Matrix4& m) override;
    virtual int __cdecl GetID() const override;
    virtual bool __cdecl IsOfType(int id) const override;

private:
    void __cdecl SetReference(Particle* reference);
    Particle* __cdecl GetReference();
    const Particle* __cdecl GetReference() const;

protected:
    virtual Camera* __cdecl CreateCamera(Camera* camera, const Vector3* position, void* data,
                                         Matrix4& result) const override;
};
JET_LAYOUT32(sizeof(Mirror) == 0x18c);

// ---- Location, Corona, Function ------------------------------------------------------------------------------

// 0x10 bytes: a local position and the particle that owns the frame (null means the position is already global).
class Location {
public:
    Location();
    Location(const Vector3& position, Particle* particle);
    Location(const Location& o);
    Location& operator=(const Location& o);

    void __cdecl Set(const Vector3& position, Particle* particle);
    Vector3 __cdecl GetGlobalLocation() const;

    Vector3 position;   // +0x00
    Particle* particle; // +0x0c
};
JET_LAYOUT32(sizeof(Location) == 0x10);

// 0x18 bytes, six float parameters; no virtual functions.
class Corona {
public:
    Corona();
    ~Corona();
    Corona& operator=(const Corona& o);

    void __cdecl Cull(ViewPoint& view, const Matrix4& m, Vector3& position);
    void __cdecl Render(Matrix4& m);

    float a;  // +0x00  0.5
    float b;  // +0x04  10000
    float c;  // +0x08  100
    float d;  // +0x0c  0.5
    float e;  // +0x10  0.1
    float f;  // +0x14  0

protected:
    static ParticleFXCache* cache;
};
JET_LAYOUT32(sizeof(Corona) == 0x18);

// 4 bytes: one virtual slot, the original's shared "return 0" stub; no virtual destructor.
class Function {
public:
    Function();
    Function(const Function& o);
    Function& operator=(const Function& o);

    virtual unsigned long __cdecl GetType() const;
};
JET_LAYOUT32(sizeof(Function) == 4);

}  // namespace Jet
