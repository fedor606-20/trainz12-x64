// Render3DCore_7c wave 5: the remaining template/free-operator exports.
//
// 108 exports that no earlier unit owned: the two exported GeometryChunkTemplate instantiations, ITree<Particle>,
// StdMaterialPluginTemplate<AutoTexMaterial>, the six Line free operators and DefaultInterpolator<Quaternion>.
// The class bodies come from the existing unit headers; this file provides the out-of-line template members, the
// explicit instantiations (which emit the exported symbols and vtables) and the two small templates that had no
// local declaration yet.
#include <jet/Templates.h>

#include <jet/Render3DCoreCaches.h>
#include <jet/Render3DCoreGeometry.h>
#include <jet/Render3DCoreParticle.h>
#include <jet/Render3DCoreSolids.h>
#include <jet/KernelStandard.h>

#include <new>

namespace Jet {

namespace {

// fld/fstp on a float quiets a signalling NaN; every Line operator copies the direction that way (0x29c80..).
inline void CopyDirectionX87(Vector3& dst, const Vector3& src)
{
    SolidsDetail::X87Copy(dst.x, src.x);
    SolidsDetail::X87Copy(dst.y, src.y);
    SolidsDetail::X87Copy(dst.z, src.z);
}

// The three rendering caches StdMaterialPlugin owns. FindCache/AddCache are RenderSystem_7c exports; the cache
// constructors are U4's local ones (same sizes and priorities as the original).
template <class Cache>
void EnsureCache(RenderingCache*& slot, RenderingCacheManager* manager, const PString& id)
{
    if (slot) return;
    RenderingCache* const found = manager->FindCache(id);
    slot = found;
    if (found) {
        found->AddReference();
        return;
    }
    Cache* const made = new Cache();
    slot = made;
    manager->AddCache(made);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// ITree<T> (0x10006d50..0x100089a0). `this` is the ITree subobject, which sits 0xc bytes into T.

template <class T>
ITree<T>& ITree<T>::operator=(const ITree& o)
{
    _count = o._count;
    _parent = o._parent;
    _head = o._head;
    _tail = o._tail;
    _next = o._next;
    _prev = o._prev;
    return *this;
}

template <class T>
ITree<T>::~ITree()
{
    DeleteAll();
}

// 0x84d0: append at the tail.
template <class T>
typename ITree<T>::Iterator ITree<T>::Add(T* node)
{
    T* const owner = reinterpret_cast<T*>(reinterpret_cast<char*>(this) - 0xc);
    node->_parent = owner;
    node->_next = nullptr;
    node->_prev = _tail;
    if (_count == 0) {
        ++_count;
        _head = node;
        _tail = node;
        return Iterator(node);
    }
    _tail->_next = node;
    ++_count;
    _tail = node;
    return Iterator(node);
}

// 0x8590: push at the head.
template <class T>
typename ITree<T>::Iterator ITree<T>::Push(T* node)
{
    T* const owner = reinterpret_cast<T*>(reinterpret_cast<char*>(this) - 0xc);
    node->_parent = owner;
    node->_next = _head;
    node->_prev = nullptr;
    if (_count == 0) {
        ++_count;
        _tail = node;
        _head = node;
        return Iterator(node);
    }
    _head->_prev = node;
    ++_count;
    _head = node;
    return Iterator(node);
}

// 0x8600
template <class T>
typename ITree<T>::Iterator ITree<T>::InsertBefore(Iterator& target, T* node)
{
    if (_head == target.node) _head = node;
    node->_parent = reinterpret_cast<T*>(reinterpret_cast<char*>(this) - 0xc);
    node->_next = target.node;
    node->_prev = target.node->_prev;
    if (target.node->_prev) target.node->_prev->_next = node;
    target.node->_prev = node;
    ++_count;
    return Iterator(node);
}

// 0x8650
template <class T>
typename ITree<T>::Iterator ITree<T>::InsertAfter(Iterator& target, T* node)
{
    if (_tail == target.node) _tail = node;
    node->_parent = reinterpret_cast<T*>(reinterpret_cast<char*>(this) - 0xc);
    node->_prev = target.node;
    node->_next = target.node->_next;
    if (target.node->_next) target.node->_next->_prev = node;
    target.node->_next = node;
    ++_count;
    return Iterator(node);
}

// 0x8520
template <class T>
typename ITree<T>::Iterator ITree<T>::Remove(Iterator& target)
{
    T* const node = target.node;
    Iterator next(node->_next);
    IRemove(target);
    return next;
}

// 0x8540
template <class T>
bool ITree<T>::Remove(T* node)
{
    T* const owner = reinterpret_cast<T*>(reinterpret_cast<char*>(this) - 0xc);
    if (node->_parent != owner) return false;
    IRemove(Iterator(node));
    return true;
}

// 0x8570: the tail
template <class T>
T* ITree<T>::Remove()
{
    if (_count == 0) return nullptr;
    T* const node = _tail;
    IRemove(Iterator(node));
    return node;
}

// 0x85e0: the head
template <class T>
T* ITree<T>::Pop()
{
    if (_count == 0) return nullptr;
    T* const node = _head;
    IRemove(Iterator(node));
    return node;
}

// 0x86a0
template <class T>
void ITree<T>::RemoveAll()
{
    while (_count) IRemove(Iterator(_head));
}

// 0x8700
template <class T>
void ITree<T>::RemoveAllChildren()
{
    while (_head) {
        T* const child = _head;
        child->RemoveAllChildren();
        IRemove(Iterator(child));
    }
}

// 0x8770 (identical to the destructor: the original's ~ITree and DeleteAll are the same body)
template <class T>
void ITree<T>::DeleteAll()
{
    while (_count) {
        T* const child = _head;
        child->DeleteAll();
        IRemove(Iterator(child));
        delete child;
    }
}

// 0x8490: reparent through the parents' virtual Add/Remove (slots 18/24 of Particle)
template <class T>
void ITree<T>::SetParent(T* node)
{
    T* const owner = reinterpret_cast<T*>(reinterpret_cast<char*>(this) - 0xc);
    if (_parent == node) return;
    if (_parent) _parent->Remove(owner);
    if (node) node->Add(owner);
}

// 0x88d0
template <class T>
typename ITree<T>::Iterator ITree<T>::Index(unsigned long index)
{
    T* node = _head;
    while (index--) node = node->_next;
    return Iterator(node);
}

template <class T>
typename ITree<T>::ConstIterator ITree<T>::Index(unsigned long index) const
{
    const T* node = _head;
    while (index--) node = node->_next;
    return ConstIterator(node);
}

// 0x8430: this count plus every descendant's own count
template <class T>
unsigned int ITree<T>::CountAll() const
{
    unsigned int total = _count;
    for (ConstIterator it = First(); it != End(); ++it) total += (*it)->CountAll();
    return total;
}

// 0x8900
template <class T>
bool ITree<T>::Exists(ConstIterator& it) const
{
    for (const T* node = _head; node; node = node->_next)
        if (node == it.node) return true;
    return false;
}

template <class T>
bool ITree<T>::Exists(const Iterator& it) const
{
    for (const T* node = _head; node; node = node->_next)
        if (node == it.node) return true;
    return false;
}

// 0x8810: pre-order over the owner (this-0xc) and its descendants
template <class T>
void ITree<T>::Traverse(void (__cdecl* fn)(T*))
{
    T* const owner = reinterpret_cast<T*>(reinterpret_cast<char*>(this) - 0xc);
    fn(owner);
    for (Iterator it = First(); it != End(); ++it) (*it)->Traverse(fn);
}

template <class T>
void ITree<T>::Traverse(void (__cdecl* fn)(const T*)) const
{
    const T* const owner = reinterpret_cast<const T*>(reinterpret_cast<const char*>(this) - 0xc);
    fn(owner);
    for (ConstIterator it = First(); it != End(); ++it) (*it)->Traverse(fn);
}

// 0x8930
template <class T>
void ITree<T>::IRemove(Iterator it)
{
    T* const node = it.node;
    if (_head == node) {
        T* const next = node->_next;
        _head = next;
        if (next) next->_prev = nullptr;
    } else {
        node->_prev->_next = node->_next;
    }
    if (_tail == node) {
        T* const prev = node->_prev;
        _tail = prev;
        if (prev) {
            prev->_next = nullptr;
            node->_prev = nullptr;
            node->_next = nullptr;
            node->_parent = nullptr;
            --_count;
            return;
        }
    } else {
        node->_next->_prev = node->_prev;
    }
    node->_prev = nullptr;
    node->_next = nullptr;
    node->_parent = nullptr;
    --_count;
}

// 0x89a0
template <class T>
void ITree<T>::IDelete(Iterator it)
{
    it.node->DeleteAll();
    IRemove(it);
    delete it.node;
}

template class ITree<Particle>;

// ---------------------------------------------------------------------------------------------------------------
// The two exported GeometryChunkTemplate instantiations (0x2c8f0..0x5e6f0, vtables 0x7f554 / 0x7f8e4). The
// explicit instantiation emits every member and the vtable even though no local code uses them.

template class GeometryChunkTemplate<PackedVertexPNTA, GeometryChunkSingleStream>;
template class GeometryChunkTemplate<PackedVertexPNTTA, GeometryChunkSingleStream>;

// ---------------------------------------------------------------------------------------------------------------
// StdMaterialPluginTemplate<M> (0xd6d0..0xd9e2, vftable 0x7df4c)

// 0xd910 / 0xd8e0: StdMaterialPlugin's constructors already zero the three cache pointers; the template only
// installs its own vtable afterwards (the compiler does that).
template <class M>
StdMaterialPluginTemplate<M>::StdMaterialPluginTemplate()
    : StdMaterialPlugin(M::UNDECORATED_TYPE, M::UID), cache0(nullptr), cache1(nullptr), cache2(nullptr)
{
}

template <class M>
StdMaterialPluginTemplate<M>::StdMaterialPluginTemplate(const PString& name, const UId& id)
    : StdMaterialPlugin(name, id), cache0(nullptr), cache1(nullptr), cache2(nullptr)
{
}

template <class M>
StdMaterialPluginTemplate<M>::StdMaterialPluginTemplate(const StdMaterialPluginTemplate& o)
    : StdMaterialPlugin(o), cache0(o.cache0), cache1(o.cache1), cache2(o.cache2)
{
}

// 0xd7a0: the original's template destructor is StdMaterialPlugin's destructor (a jump at 0x1000d7a0). The base
// destructor releases +0x20..+0x28 and destroys MaterialPlugin; nothing template-specific is left.
template <class M>
StdMaterialPluginTemplate<M>::~StdMaterialPluginTemplate()
{
}

// 0xd6d0
template <class M>
StdMaterialPluginTemplate<M>& StdMaterialPluginTemplate<M>::operator=(const StdMaterialPluginTemplate& o)
{
    MaterialPlugin::operator=(o);
    cache0 = o.cache0;
    cache1 = o.cache1;
    cache2 = o.cache2;
    return *this;
}

// 0x711f0: the same body StdMaterialPlugin uses; the three caches are +0x20/+0x24/+0x28.
template <class M>
void __cdecl StdMaterialPluginTemplate<M>::CreatePluginResources(RenderKernel* kernel) const
{
    if (!kernel) return;
    RenderingCacheManager* const manager =
        reinterpret_cast<RenderingCacheManager*>(reinterpret_cast<unsigned char*>(kernel) + 0x6c);
    StdMaterialPluginTemplate* const self = const_cast<StdMaterialPluginTemplate*>(this);
    EnsureCache<MaterialChunkCache>(self->cache0, manager, MaterialChunkCache::MaterialChunkCacheID);
    EnsureCache<AlphaBlendCache>(self->cache1, manager, AlphaBlendCache::AlphaBlendCacheID);
    EnsureCache<AlphaMaskCache>(self->cache2, manager, AlphaMaskCache::AlphaMaskCacheID);
}

// 0xd940: resources first (slot 6), then the concrete material from the three caches, then the factory link.
template <class M>
Material* __cdecl StdMaterialPluginTemplate<M>::CreateInstance(MaterialManager* manager, RenderKernel* kernel) const
{
    this->CreatePluginResources(kernel);
    void* const memory = Alloc::New(sizeof(M));
    if (!memory) return nullptr;
    M* const material = ::new (memory) M(manager, static_cast<MaterialChunkCache*>(cache0),
                                         static_cast<AlphaMaskCache*>(cache2),
                                         static_cast<AlphaBlendCache*>(cache1));
    this->AssignFactory(material);
    return material;
}

static_assert(sizeof(AutoTexMaterial) == 0xc4, "CreateInstance allocates the original's 0xc4 bytes");
template class StdMaterialPluginTemplate<AutoTexMaterial>;

// ---------------------------------------------------------------------------------------------------------------
// DefaultInterpolator<Quaternion> (0x21cb0 / 0x28ea0)

template <class T>
T __cdecl DefaultInterpolator<T>::Interpolate(const T& a, const T& b, float t)
{
    T result;
    result.Slerp(a, b, t);
    return result;
}

template class DefaultInterpolator<Quaternion>;

// ---------------------------------------------------------------------------------------------------------------
// Line free operators (0x10029c80..0x10029fce). Operand order and the x87 direction copy follow the assembly.

// 0x29c80: the point moves, the direction is copied.
Line __cdecl operator+(const Line& line, const Vector3& delta)
{
    Line result;
    result.point.x = SolidsDetail::AddSS(delta.x, line.point.x);
    result.point.y = SolidsDetail::AddSS(delta.y, line.point.y);
    result.point.z = SolidsDetail::AddSS(delta.z, line.point.z);
    CopyDirectionX87(result.direction, line.direction);
    return result;
}

// 0x29cf0
Line __cdecl operator-(const Line& line, const Vector3& delta)
{
    Line result;
    result.point.x = SolidsDetail::SubSS(line.point.x, delta.x);
    result.point.y = SolidsDetail::SubSS(line.point.y, delta.y);
    result.point.z = SolidsDetail::SubSS(line.point.z, delta.z);
    CopyDirectionX87(result.direction, line.direction);
    return result;
}

// 0x29d60: row-major matrix times the line (direction and point rotate the same way)
Line __cdecl operator*(const Matrix3& m, const Line& line)
{
    Line result;
    using namespace SolidsDetail;
    result.point.x = AddSS(AddSS(MulSS(m.m[0], line.point.x), MulSS(m.m[6], line.point.z)),
                           MulSS(line.point.y, m.m[3]));
    result.point.y = AddSS(AddSS(MulSS(line.point.x, m.m[1]), MulSS(m.m[4], line.point.y)),
                           MulSS(m.m[7], line.point.z));
    result.point.z = AddSS(AddSS(MulSS(m.m[2], line.point.x), MulSS(m.m[5], line.point.y)),
                           MulSS(m.m[8], line.point.z));
    result.direction.x = AddSS(AddSS(MulSS(line.direction.x, m.m[0]), MulSS(line.direction.z, m.m[6])),
                               MulSS(line.direction.y, m.m[3]));
    result.direction.y = AddSS(AddSS(MulSS(m.m[4], line.direction.y), MulSS(m.m[7], line.direction.z)),
                               MulSS(line.direction.x, m.m[1]));
    result.direction.z = AddSS(AddSS(MulSS(line.direction.y, m.m[5]), MulSS(line.direction.z, m.m[8])),
                               MulSS(m.m[2], line.direction.x));
    return result;
}

// 0x29e80
Line __cdecl operator*(const Line& line, float scale)
{
    Line result;
    result.point.x = SolidsDetail::MulSS(line.point.x, scale);
    result.point.y = SolidsDetail::MulSS(line.point.y, scale);
    result.point.z = SolidsDetail::MulSS(line.point.z, scale);
    CopyDirectionX87(result.direction, line.direction);
    return result;
}

// 0x29ef0
Line __cdecl operator*(float scale, const Line& line)
{
    Line result;
    result.point.x = SolidsDetail::MulSS(line.point.x, scale);
    result.point.y = SolidsDetail::MulSS(line.point.y, scale);
    result.point.z = SolidsDetail::MulSS(line.point.z, scale);
    CopyDirectionX87(result.direction, line.direction);
    return result;
}

// 0x29f60: one reciprocal, then multiplies
Line __cdecl operator/(const Line& line, float scale)
{
    const float inv = SolidsDetail::DivSS(1.0f, scale);
    Line result;
    result.point.x = SolidsDetail::MulSS(inv, line.point.x);
    result.point.y = SolidsDetail::MulSS(inv, line.point.y);
    result.point.z = SolidsDetail::MulSS(inv, line.point.z);
    CopyDirectionX87(result.direction, line.direction);
    return result;
}

}  // namespace Jet
