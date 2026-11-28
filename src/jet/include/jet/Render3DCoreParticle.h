// Render3DCore_7c unit U9a: the particle scene node and its core tree/attachment helpers.
//
// Layout and behaviour follow the TS12 assembly (addresses per member in Particle.cpp, ParticleAlias.cpp,
// ViewPortParticle.cpp, ParticleStructure.cpp). 0x12c bytes for Particle:
//   +0x00 vftable (Element), +0x04 visited, +0x08 lastCulledFrame,
//   +0x0c ITree<Particle> {count, parent, head, tail, next, prev},
//   +0x24 cell node, +0x28 name, +0x2c position, +0x38 orientation, +0x48 flag byte,
//   +0x4c bounding volume pointer, +0x50 embedded ParticleAxisAlignedBoundingBox (0xa0),
//   +0xf0 OrientedBox (0x3c).
// Virtual member functions of the original are __cdecl (this is on the stack as the first argument); constructors,
// the assignment and the private helpers are __thiscall, as their mangled names say.
//
// The bounding volume and cell are other units (U3, U9 scene); their objects are addressed through the local
// classes now: the embedded volume is constructed by U3's ParticleBoundingVolume constructors and the cell by the
// private CellAxisAlignedBoundingBox in Cell.cpp. Everything marked "U9 scene" moves with that unit.
#pragma once

#include <jet/DynamicArray.h>
#include <jet/FastDynamicArray.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/Render3DCoreOrientedBox.h>
#include <jet/Render3DCorePlane.h>
#include <jet/Render3DCoreSolids.h>
#include <jet/RenderTypes.h>

namespace Jet {

class Facility;
class Particle;
class ParticleAlias;
class ParticleBoundingVolume;
class ParticleStructure;
class ViewPortParticle;
class AttachmentPoint;
class Cell;
class CellNode;
class CollisionResult;
class ConstFaceDescriptor;
class Ray;
class ViewPoint;

using ParticlePtrList = FastDynamicArray<Particle*, 21>;
using ConstParticlePtrList = FastDynamicArray<const Particle*, 21>;
using PlaneList = DynamicArray<Plane, 16>;
using FacilityPtrList = FastDynamicArray<Facility*, 13>;
using ConstFacilityPtrList = FastDynamicArray<const Facility*, 13>;

// Jet::ITree<T>: a node of a linked tree. The original's SDK class has no virtual functions; the six links are
// read and written directly by the Particle code. Iterator is one node pointer.
template <class T>
class ITree {
public:
    class ConstIterator;

    class Iterator {
    public:
        Iterator(T* n = nullptr) : node(n) {}
        bool operator==(const Iterator& o) const { return node == o.node; }
        bool operator!=(const Iterator& o) const { return node != o.node; }
        T* operator*() const { return node; }
        T* operator->() const { return node; }
        Iterator& operator++()
        {
            node = node->_next;
            return *this;
        }
        T* node;
    };

    class ConstIterator {
    public:
        ConstIterator(const T* n = nullptr) : node(n) {}
        ConstIterator(const Iterator& o) : node(o.node) {}
        bool operator==(const ConstIterator& o) const { return node == o.node; }
        bool operator!=(const ConstIterator& o) const { return node != o.node; }
        const T* operator*() const { return node; }
        const T* operator->() const { return node; }
        ConstIterator& operator++()
        {
            node = node->_next;
            return *this;
        }
        const T* node;
    };

    ITree() : _count(0), _parent(nullptr), _head(nullptr), _tail(nullptr), _next(nullptr), _prev(nullptr) {}
    ~ITree();

    ITree& operator=(const ITree& o);

    unsigned int Count() const { return _count; }
    Iterator First() { return Iterator(_head); }
    ConstIterator First() const { return ConstIterator(_head); }
    Iterator Last() { return Iterator(_tail); }
    ConstIterator Last() const { return ConstIterator(_tail); }
    Iterator End() { return Iterator(nullptr); }
    ConstIterator End() const { return ConstIterator(nullptr); }
    Iterator Parent() { return Iterator(_parent); }
    ConstIterator Parent() const { return ConstIterator(_parent); }
    Iterator Next() { return Iterator(_next); }
    ConstIterator Next() const { return ConstIterator(_next); }
    Iterator Prev() { return Iterator(_prev); }
    ConstIterator Prev() const { return ConstIterator(_prev); }

    Iterator Add(T* node);
    Iterator Push(T* node);
    Iterator InsertBefore(Iterator& target, T* node);
    Iterator InsertAfter(Iterator& target, T* node);
    Iterator Remove(Iterator& target);
    bool Remove(T* node);
    T* Remove();
    T* Pop();
    void RemoveAll();
    void RemoveAllChildren();
    void DeleteAll();
    void SetParent(T* node);
    Iterator Index(unsigned long index);
    ConstIterator Index(unsigned long index) const;
    unsigned int CountAll() const;
    bool Exists(ConstIterator& it) const;
    bool Exists(const Iterator& it) const;
    void Traverse(void (__cdecl* fn)(T*));
    void Traverse(void (__cdecl* fn)(const T*)) const;

private:
    void IRemove(Iterator it);
    void IDelete(Iterator it);

public:
    unsigned int _count;  // +0x00
    T* _parent;           // +0x04
    T* _head;             // +0x08
    T* _tail;             // +0x0c
    T* _next;             // +0x10
    T* _prev;             // +0x14
};

// A passive attachment point of a ParticleStructure (U9 scene class, declared here with the original's layout:
// PString name +0, Vector3 position +4, Quaternion orientation +0x10; 0x20 bytes).
class ReferencePoint {
public:
    ReferencePoint() : name(), position(), orientation() {}
    ReferencePoint(const ReferencePoint& o) : name(o.name), position(o.position), orientation(o.orientation) {}
    ReferencePoint& operator=(const ReferencePoint& o)
    {
        name = o.name;
        position = o.position;
        orientation = o.orientation;
        return *this;
    }
    ~ReferencePoint() {}

    const PString& __cdecl GetName() const { return name; }
    void __cdecl SetName(const PString& v) { name = v; }
    const Vector3& __cdecl GetPosition() const { return position; }
    void __cdecl SetPosition(const Vector3& v) { position = v; }
    const Quaternion& __cdecl GetOrientation() const { return orientation; }
    void __cdecl SetOrientation(const Quaternion& v) { orientation = v; }

    PString name;             // +0x00
    Vector3 position;         // +0x04
    Quaternion orientation;   // +0x10
};
JET_LAYOUT32(sizeof(ReferencePoint) == 0x20);

using ReferencePointList = DynamicArray<ReferencePoint, 3>;

class Particle : public Element, public ITree<Particle> {
public:
    Particle();
    Particle(const Particle& o);
    virtual ~Particle();
    Particle& operator=(const Particle& o);

    // overrides of Element's virtuals
    virtual void __cdecl Cull(ViewPoint& view, const Matrix4& m);
    virtual void __cdecl RenderDebug(Display* display, DebugViewPort& port, Matrix4& m, unsigned long flags);

    virtual Particle* __cdecl CreateInstance() const;
    virtual int __cdecl GetID() const;
    virtual bool __cdecl IsOfType(int id) const;
    virtual void __cdecl SetName(const PString& name);
    virtual void __cdecl SetParent(Particle* parent);
    virtual void __cdecl SetPosition(const Vector3& position);
    virtual void __cdecl SetPositionXY(const Vector2& position);
    virtual void __cdecl SetOrientation(const Quaternion& orientation);
    virtual void __cdecl SetReferenceFrame(const Vector3& position, const Quaternion& orientation);
    virtual float __cdecl GetOpacity() const;
    virtual void __cdecl SetOpacity(float opacity);
    // overloaded virtuals: MSVC puts each name's overloads in reverse declaration order
    virtual void __cdecl Add(Particle* particle);                       // slot 18
    virtual bool __cdecl Add(const PString& hostName, Particle* particle);  // slot 17
    virtual void __cdecl Push(Particle* particle);
    virtual Iterator __cdecl InsertBefore(Iterator& target, Particle* particle);
    virtual Iterator __cdecl InsertAfter(Iterator& target, Particle* particle);
    virtual Particle* __cdecl Remove();                                 // slot 25
    virtual bool __cdecl Remove(Particle* particle);                    // slot 24
    virtual Particle* __cdecl Remove(const PString& name);              // slot 23
    virtual Iterator __cdecl Remove(Iterator& target);                  // slot 22
    virtual Particle* __cdecl Pop();
    virtual void __cdecl RemoveAll();
    virtual void __cdecl RemoveAllChildren();
    virtual void __cdecl DeleteAll();
    virtual bool __cdecl CheckVisible(const ViewPoint& view);
    virtual bool __cdecl IsContainer() const;
    virtual const Particle* __cdecl FindParentContainer() const;  // slot 33
    virtual Particle* __cdecl FindParentContainer();              // slot 32
    virtual bool __cdecl TestCollision(const Ray& ray, CollisionResult& result, const Particle* ignoreThis);
    virtual bool __cdecl TestCollisionGeometry(const Ray& ray, float& t, ConstFaceDescriptor& face) const;
    virtual bool __cdecl IsWithin(const PlaneList& planes) const;
    virtual bool __cdecl GetAllWithin(const PlaneList& planes, ParticlePtrList& found);            // slot 40
    virtual bool __cdecl GetAllWithin(const PlaneList& planes, ConstParticlePtrList& found) const;  // slot 39
    virtual bool __cdecl GetAllWithin(const Sphere& sphere, ParticlePtrList& found);               // slot 38
    virtual bool __cdecl GetAllWithin(const Sphere& sphere, ConstParticlePtrList& found) const;    // slot 37
    virtual bool __cdecl GetFacilities(FacilityPtrList& facilities);                                // slot 42
    virtual bool __cdecl GetFacilities(ConstFacilityPtrList& facilities) const;                     // slot 41
    virtual const Facility* __cdecl GetFacility(const PString& name) const;                         // slot 44
    virtual Facility* __cdecl GetFacility(const PString& name);                                     // slot 43
    virtual bool __cdecl SetBoundingVolume(ParticleBoundingVolume* volume);

    const PString& __cdecl GetName() const;
    Particle* __cdecl GetParent();
    const Particle* __cdecl GetParent() const;
    const Vector3& __cdecl GetGlobalPosition() const;
    const Quaternion& __cdecl GetGlobalOrientation() const;
    const Vector3& __cdecl GetPosition() const;
    Vector2 __cdecl GetPositionXY() const;
    const Quaternion& __cdecl GetOrientation() const;
    bool __cdecl GetEnabled() const;
    void __cdecl SetEnabled(bool enabled);
    const Sphere& __cdecl GetBoundingSphere(bool recalculate) const;
    const AxisAlignedBox& __cdecl GetAxisAlignedBoundingBox(bool recalculate) const;
    const OrientedBox& __cdecl GetOrientedBoundingBox(bool recalculate) const;
    Sphere __cdecl GetGlobalBoundingSphere(bool recalculate) const;
    void __cdecl GetGlobalBoundingSphere(Sphere& sphere, bool recalculate) const;
    const Vector3& __cdecl GetGlobalBoundingSphereCenter(bool recalculate) const;
    void __cdecl PointXAt(const Vector3& target, const Vector3& up);
    void __cdecl PointYAt(const Vector3& target, const Vector3& up);
    void __cdecl PointZAt(const Vector3& target, const Vector3& up);
    void __cdecl SetCullSizeCheck(bool state);
    bool __cdecl GetCullSizeCheck() const;
    ParticleBoundingVolume* __cdecl GetBoundingVolume();
    const ParticleBoundingVolume* __cdecl GetBoundingVolume() const;
    Matrix4 __cdecl GetModelView(const Matrix4& view) const;
    void __cdecl GetModelView(Matrix4& result, const Matrix4& view) const;
    void __cdecl Translate(const Vector3& delta);
    void __cdecl Rotate(const Matrix3& rot);
    void __cdecl Rotate(const Quaternion& rot);
    void __cdecl SelfRotate(const Matrix3& rot);
    void __cdecl SelfRotate(const Quaternion& rot);
    void __cdecl LocalToGlobal(OrientedBox& box) const;
    void __cdecl LocalToGlobal(Plane& plane) const;
    void __cdecl LocalToGlobal(Quaternion& orientation) const;
    void __cdecl LocalToGlobal(Vector3& position) const;
    void __cdecl LocalToGlobal(Vector3& position, Quaternion& orientation) const;
    void __cdecl GlobalToLocal(OrientedBox& box) const;
    void __cdecl GlobalToLocal(Plane& plane) const;
    void __cdecl GlobalToLocal(Quaternion& orientation) const;
    void __cdecl GlobalToLocal(Vector3& position) const;
    void __cdecl GlobalToLocal(Vector3& position, Quaternion& orientation) const;
    void __cdecl LocalToLocal(const Particle* target, OrientedBox& box) const;
    void __cdecl LocalToLocal(const Particle* target, Plane& plane) const;
    void __cdecl LocalToLocal(const Particle* target, Quaternion& orientation) const;
    void __cdecl LocalToLocal(const Particle* target, Vector3& position) const;
    void __cdecl LocalToLocal(const Particle* target, Vector3& position, Quaternion& orientation) const;
    Particle* __cdecl FindByName(const PString& name);
    const Particle* __cdecl FindByName(const PString& name) const;
    bool __cdecl EnumerateParticles(ParticlePtrList& list);
    bool __cdecl EnumerateParticles(ConstParticlePtrList& list) const;
    Cell* __cdecl GetCell();
    const Cell* __cdecl GetCell() const;
    void __cdecl SetCell(Cell* cell);
    void __cdecl PushToCellRoot();

protected:
    void __cdecl InitInstance(Particle& instance) const;
    virtual void __cdecl SetRawPosition(const Vector3& position);
    virtual void __cdecl SetRawOrientation(const Quaternion& orientation);

private:
    void __cdecl SetCellNode(CellNode* node);

    friend class ViewPortParticle;
    friend class ParticleStructure;
    friend class ParticleAlias;

    CellNode* cellNode;                       // +0x24
    PString name;                             // +0x28
    Vector3 position;                         // +0x2c
    Quaternion orientation;                   // +0x38
    unsigned char flags;                      // +0x48: bit 0 cull size check, bit 1 enabled
    unsigned char padding49[3];
    ParticleBoundingVolume* boundingVolume;   // +0x4c
    unsigned char volumeStore[0xa0];          // +0x50: embedded ParticleAxisAlignedBoundingBox
    OrientedBox orientedBox;                  // +0xf0
};
JET_LAYOUT32(sizeof(Particle) == 0x12c);

class ViewPortParticle : public Particle {
public:
    ViewPortParticle();
    ViewPortParticle(const ViewPortParticle& o);
    virtual ~ViewPortParticle();
    ViewPortParticle& operator=(const ViewPortParticle& o);

    virtual Particle* __cdecl CreateInstance() const;
    virtual bool __cdecl CheckVisible(const ViewPoint& view);

    void __cdecl AddAlias(ParticleAlias* alias);
    void __cdecl RemoveAlias(ParticleAlias* alias);
    void __cdecl DeleteAllAliases();
    void __cdecl SetRenderFlags(unsigned long flags);
    unsigned long __cdecl GetRenderFlags() const;

protected:
    void __cdecl InitInstance(ViewPortParticle& instance) const;
    void __cdecl CopyAliasesFrom(const ViewPortParticle& o);

    unsigned long renderFlags;   // +0x12c
    unsigned int aliasCount;     // +0x130
    unsigned int aliasCapacity;  // +0x134
    ParticleAlias** aliases;     // +0x138
};
JET_LAYOUT32(sizeof(ViewPortParticle) == 0x13c);

class ParticleAlias : public ViewPortParticle {
public:
    ParticleAlias();
    ParticleAlias(ViewPortParticle* aliasTo);
    ParticleAlias(const ParticleAlias& o);
    virtual ~ParticleAlias();
    ParticleAlias& operator=(const ParticleAlias& o);

    virtual Particle* __cdecl CreateInstance() const;
    virtual int __cdecl GetID() const;
    virtual bool __cdecl IsOfType(int id) const;
    virtual bool __cdecl CheckVisible(const ViewPoint& view);
    virtual void __cdecl Cull(ViewPoint& view, const Matrix4& m);

    ViewPortParticle* __cdecl GetAliasTo() const;
    void __cdecl SetAliasTo(ViewPortParticle* aliasTo);

protected:
    void __cdecl InitInstance(ParticleAlias& instance) const;
    void __cdecl AssumeIdentity(ViewPortParticle* aliasTo);
    void __cdecl Unalias();

    ViewPortParticle* aliasTarget;  // +0x13c
};
JET_LAYOUT32(sizeof(ParticleAlias) == 0x140);

class ParticleStructure : public ViewPortParticle {
public:
    ParticleStructure();
    ParticleStructure(const ParticleStructure& o);
    virtual ~ParticleStructure();
    ParticleStructure& operator=(const ParticleStructure& o);

    virtual Particle* __cdecl CreateInstance() const;
    virtual int __cdecl GetID() const;
    virtual bool __cdecl IsOfType(int id) const;

    bool __cdecl AddAttachmentPoint(AttachmentPoint* point, bool matchHost);
    bool __cdecl EnumerateAttachmentPoints(FastDynamicArray<AttachmentPoint*, 13>& points);
    bool __cdecl EnumerateAttachmentPoints(FastDynamicArray<const AttachmentPoint*, 13>& points) const;
    void __cdecl AddReferencePoint(const ReferencePoint& point);
    ReferencePointList& __cdecl GetReferencePointList();
    const ReferencePointList& __cdecl GetReferencePointList() const;
    void __cdecl SetReferencePointList(const ReferencePointList& list);
    bool __cdecl SetCurrentReferencePoint(const PString& name);

protected:
    void __cdecl InitInstance(ParticleStructure& instance) const;

    ReferencePointList referencePoints;  // +0x13c
};
JET_LAYOUT32(sizeof(ParticleStructure) == 0x140);

}  // namespace Jet
