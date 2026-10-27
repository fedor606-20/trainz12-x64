// Units U5b1 and U5b2 of RenderSystem_7c: GeometryChunk (0x10021a70..0x100222c0, 0x10024a90, 0x100254a0..
// 0x1002abe0, with the property, bone and registry helpers; U5b2 is the interpolation and copy). Behaviour follows
// the TS12 assembly (addresses per function); the registry stays the original's static. Floats the original copies through the x87 stack are copied as it does (a
// signalling NaN comes out quiet).
#include <jet/Geometry.h>
#include <jet/Math.h>
#include <jet/RenderSystem.h>
#include <jet/VCall.h>

#include <malloc.h>
#include <windows.h>

#include <cstring>
#include <new>

// The original's _purecall answers 0 (its pure slots are `xor eax, eax; ret`); so does ours
extern "C" int __cdecl _purecall() { return 0; }

namespace Jet {

namespace {

using Node = PStringCache::Node;
using IndexArray = AutoReferenceArray<unsigned short>;

// addss / mulss with the original's operand order (with two NaNs the first one comes out)
// (inline assembly: the compiler commutes even the intrinsics)
float Add(float a, float b)
{
    __asm {
        movss xmm0, a
        addss xmm0, b
        movss a, xmm0
    }
    return a;
}
float Mul(float a, float b)
{
    __asm {
        movss xmm0, a
        mulss xmm0, b
        movss a, xmm0
    }
    return a;
}

// a float through fld/fstp: a signalling NaN gets its quiet bit
float X87(float f)
{
    unsigned int u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7f800000) == 0x7f800000 && (u & 0x7fffff) && !(u & 0x400000)) u |= 0x400000;
    std::memcpy(&f, &u, 4);
    return f;
}

void Destroy(Node* n)
{
    if (n && PString::Cache()) PString::Cache()->Destroy(n);
}

// the element sizes per vertex item data type (VertexType.cpp's table)
unsigned int ElementSize(int dataType) { return VertexType::GetElementTypeSize(static_cast<DVertexItemDataType>(dataType)); }

// a slot returning a VertexItemPtr (hidden result pointer after `this`)
template <class P>
P ItemArray(const GeometryChunk* c, unsigned int offset, int type, int index)
{
    P r;
    VCall<void>(c, offset, &r, type, index);
    return r;
}

// the display's debug settings: +0x54 the render state block (+0xa1 bit 2), +0x71 the debug flags, +0x74 the
// normal length, +0x78 / +0x79
const unsigned char* Raw(const Display* d) { return reinterpret_cast<const unsigned char*>(d); }
unsigned char* StateFlags(Display* d) { return *reinterpret_cast<unsigned char* const*>(Raw(d) + 0x54) + 0xa1; }
float NormalLength(const Display* d) { return *reinterpret_cast<const float*>(Raw(d) + 0x74); }

struct LineVertex {  // 0x10
    float x, y, z;
    unsigned long color;
};

unsigned long Convert(float r, float g, float b, float a)
{
    const Color c{r, g, b, a};
    return Display::ConvertColor(c);
}

// the original's MatrixMap array growth (0x10025e04 / 0x10026cc0): room for count + 1 rounded up to 128
void AppendMap(Array<VertexProgram::MatrixMap*, 128>& a, VertexProgram::MatrixMap* m)
{
    if (a.capacity < a.count + 1) {
        const unsigned int items = (a.count + 0x80) >> 7 << 7;
        auto* const block = static_cast<VertexProgram::MatrixMap**>(Alloc::New(items * 4));
        if (a.data) {
            std::memcpy(block, a.data, a.count * 4);
            if (a.data) Alloc::Delete(a.data);
        }
        a.data = block;
        a.capacity = items;
    }
    VertexProgram::MatrixMap** const at = a.data + a.count;
    if (at) *at = m;
    ++a.count;
}

// the original's U5b2 entry points (looked up once)
template <class F>
F Orig(const char* name)
{
    return reinterpret_cast<F>(GetProcAddress(GetModuleHandleA("RenderSystem_7c_orig.dll"), name));
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// construction

// 0x100254a0 (the vertex count is left as it is)
GeometryChunk::GeometryChunk()
    : flags(0), numPrimitives(0), numIndices(0), maxVertices(0), numFaceNormals(0), opacity(1.0f), surfaceArea(0),
      userData(0), referencedUserData(nullptr)
{
}

// 0x10025500
GeometryChunk::GeometryChunk(unsigned int vertices, unsigned int primitives, unsigned int indices,
                             unsigned int faceNormalCount, unsigned int f)
    : flags(f), numPrimitives(primitives), numIndices(indices), numVertices(vertices), maxVertices(vertices),
      numFaceNormals(faceNormalCount), opacity(1.0f), surfaceArea(0), userData(0), referencedUserData(nullptr)
{
    if (indices) {
        indexArray.Release();
        indexArray.block = IndexArray::Make(indices);
    }
    if (faceNormalCount) {
        faceNormals.Release();
        faceNormals.block = AutoReferenceArray<Vector3>::Make(faceNormalCount);
    }
}

// 0x10025650 (the arrays go 0x34, 0x30, 0x2c, then the bone array, then the properties)
GeometryChunk::~GeometryChunk()
{
    if (referencedUserData) referencedUserData->ReleaseReference();
    ResetMatrixArray();
}

// 0x1002ada0: the chunks after the head first, the head last
GeometryChunk::Properties::~Properties()
{
    PropertyChunk* const h = head;
    if (!h) return;
    head = nullptr;
    PropertyChunk* c = h->next;
    while (c != h) {
        PropertyChunk* const n = c->next;
        for (unsigned int k = 0; k < c->count; ++k) Destroy(c->data[k].name);
        Alloc::Delete(c);
        c = n;
    }
    for (unsigned int k = 0; k < c->count; ++k) Destroy(c->data[k].name);
    Alloc::Delete(c);
}

const GeometryChunk& GeometryChunk::operator=(const GeometryChunk&) { return *this; }  // 0x10038c50

bool __cdecl GeometryChunk::InitializeBuffers(Display*, bool) { return true; }  // 0x100015d0
void __cdecl GeometryChunk::ResizeVertexArray(Display*, unsigned int, int, int) {}  // 0x10021870
void __cdecl GeometryChunk::RenderChunkColor(Display*) const {}  // 0x10021870

// 0x10025ca0: the arrays are shared, the user data referenced once more (the target's own is not let go), the
// bones copied without their matrices
void __cdecl GeometryChunk::InitInstance(GeometryChunk& to) const
{
    to.surfaceArea = X87(surfaceArea);
    to.opacity = X87(opacity);
    to.flags = flags;
    to.userData = userData;
    to.referencedUserData = referencedUserData;
    to.numVertices = numVertices;
    to.maxVertices = maxVertices;
    to.numPrimitives = numPrimitives;
    to.numIndices = numIndices;
    to.numFaceNormals = numFaceNormals;
    to.indexArray.Share(indexArray.block);
    to.faceNormals.Share(faceNormals.block);
    to.faceNeighbors.Share(faceNeighbors.block);
    if (referencedUserData) referencedUserData->AddReference();
    for (unsigned int k = 0; k < influences.count; ++k) {
        auto* const m = static_cast<VertexProgram::MatrixMap*>(Alloc::New(8));
        if (m) {
            m->matrix = nullptr;
            m->index = influences.data[k]->index;
        }
        AppendMap(to.influences, m);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// plain members

unsigned long __cdecl GeometryChunk::GetFlags() const { return flags; }                       // 0x1000ae00
void __cdecl GeometryChunk::SetFlags(unsigned long f) { flags = f; }                          // 0x10021a60
void __cdecl GeometryChunk::SetConstFlags(unsigned long f) const { flags = f; }               // 0x10021a60
void __cdecl GeometryChunk::SetIndexDataDynamic() { flags |= 0x100; }                         // 0x10021ab0
bool __cdecl GeometryChunk::GetIndexDataDynamic() const { return (flags >> 8 & 1) != 0; }     // 0x10021ac0
bool __cdecl GeometryChunk::GetIndexDataDirty() const { return (flags >> 9 & 1) != 0; }       // 0x10021af0
bool __cdecl GeometryChunk::ContainsUnsharedData() const { return (flags >> 12 & 1) != 0; }   // 0x10021b00
unsigned int __cdecl GeometryChunk::GetNumVertices() const { return numVertices; }            // 0x10021b20
void __cdecl GeometryChunk::SetNumVertices(unsigned int n) { numVertices = n; }               // 0x10021b30
unsigned int __cdecl GeometryChunk::GetMaxVertices() const { return maxVertices; }            // 0x10004ec0
void __cdecl GeometryChunk::SetMaxVertices(unsigned int n) { maxVertices = n; }               // 0x10021b40
unsigned int __cdecl GeometryChunk::GetNumIndices() const { return numIndices; }              // 0x10021b50
void __cdecl GeometryChunk::SetNumIndices(unsigned int n) { numIndices = n; }                 // 0x10021b60
unsigned int __cdecl GeometryChunk::GetMaxIndices() const { return indexArray.Count(); }      // 0x10021b70
unsigned int __cdecl GeometryChunk::GetNumPrimitives() const { return numPrimitives; }        // 0x10010940
void __cdecl GeometryChunk::SetNumPrimitives(unsigned int n) { numPrimitives = n; }           // 0x10021b90
unsigned int __cdecl GeometryChunk::GetNumFaceNormals() const { return numFaceNormals; }      // 0x100219a0
void __cdecl GeometryChunk::SetNumFaceNormals(unsigned int n) { numFaceNormals = n; }         // 0x10021ba0
unsigned int __cdecl GeometryChunk::GetNumFaceNeighbors() const { return faceNeighbors.Count(); }  // 0x10021bb0
float __cdecl GeometryChunk::GetOpacity() const { return opacity; }                           // 0x1000bc00
float __cdecl GeometryChunk::GetSurfaceArea() const { return surfaceArea; }                   // 0x10021b10
void __cdecl GeometryChunk::SetSurfaceArea(float a) { surfaceArea = a; }                      // 0x1000bc20
unsigned long __cdecl GeometryChunk::GetUserData() const { return userData; }                 // 0x1000fc40
void __cdecl GeometryChunk::SetUserData(unsigned long d) { userData = d; }                    // 0x1000fc30
ReferenceCount* __cdecl GeometryChunk::GetRefefencedUserData() const { return referencedUserData; }  // 0x10021aa0
Array<VertexProgram::MatrixMap*, 128>& __cdecl GeometryChunk::GetInfluenceArray() { return influences; }  // 0x1003ea80
const Array<VertexProgram::MatrixMap*, 128>& __cdecl GeometryChunk::GetInfluenceArray() const { return influences; }

// 0x10021ad0
void __cdecl GeometryChunk::SetIndexDataDirty(bool on) const
{
    if (on)
        flags |= 0x200;
    else
        flags &= ~0x200ul;
}

// 0x10021a70
void __cdecl GeometryChunk::SetReferencedUserData(ReferenceCount* d)
{
    if (referencedUserData) referencedUserData->ReleaseReference();
    referencedUserData = d;
    if (d) d->AddReference();
}

// 0x10021bd0: the block's items (no null test)
unsigned short* __cdecl GeometryChunk::GetIndexArray()
{
    return reinterpret_cast<unsigned short*>(reinterpret_cast<uintptr_t>(indexArray.block) + 0x10);
}
const unsigned short* __cdecl GeometryChunk::GetIndexArray() const
{
    return reinterpret_cast<const unsigned short*>(reinterpret_cast<uintptr_t>(indexArray.block) + 0x10);
}
// 0x10021c50 / 0x10021ce0
Vector3* __cdecl GeometryChunk::GetFaceNormalArray() { return faceNormals.Data(); }
const Vector3* __cdecl GeometryChunk::GetFaceNormalArray() const { return faceNormals.Data(); }
GeometryChunk::FaceNeighbors* __cdecl GeometryChunk::GetFaceNeighborArray() { return faceNeighbors.Data(); }
const GeometryChunk::FaceNeighbors* __cdecl GeometryChunk::GetFaceNeighborArray() const { return faceNeighbors.Data(); }

// 0x10021be0: the count follows the array
void __cdecl GeometryChunk::SetIndexArray(AutoReferenceArray<unsigned short> a)
{
    indexArray.Share(a.block);
    numIndices = indexArray.Count();
}
// 0x10021c70
void __cdecl GeometryChunk::SetFaceNormalArray(AutoReferenceArray<Vector3> a)
{
    faceNormals.Share(a.block);
    numFaceNormals = faceNormals.Count();
}
// 0x10021d00
void __cdecl GeometryChunk::SetFaceNeighborArray(AutoReferenceArray<FaceNeighbors>& a) { faceNeighbors.Share(a.block); }

// 0x100222a0: no data here
GeometryChunk::ConstUDwordPtr __cdecl GeometryChunk::GetUDwordArray(VertexItemType, int) const
{
    ConstUDwordPtr r;
    r.stride = 4;
    r.data = nullptr;
    return r;
}
GeometryChunk::UDwordPtr __cdecl GeometryChunk::GetUDwordArray(VertexItemType, int)
{
    UDwordPtr r;
    r.stride = 4;
    r.data = nullptr;
    return r;
}

// ---------------------------------------------------------------------------------------------------------------
// the arrays

namespace {

// a new block replaces the field (its temporary reference let go afterwards)
template <class T>
void Replace(AutoReferenceArray<T>& field, typename AutoReferenceArray<T>::Block* b)
{
    if (field.block != b) {
        field.Release();
        field.block = b;
        ++b->refs;
    }
    AutoReferenceArray<T>::Drop(b);
}

}  // namespace

// 0x100257d0
void __cdecl GeometryChunk::DuplicateIndexArray(Display*)
{
    if (!indexArray.block) return;
    IndexArray::Block* const b = IndexArray::Make(indexArray.Count());
    if (indexArray.block && indexArray.block->count)
        std::memcpy(b->data, indexArray.block->data, indexArray.Count() * 2);
    Replace(indexArray, b);
}

// 0x10025890: as many as numFaceNormals
void __cdecl GeometryChunk::DuplicateFaceNormalArray()
{
    if (!faceNormals.block) return;
    auto* const b = AutoReferenceArray<Vector3>::Make(numFaceNormals);
    if (numFaceNormals) std::memcpy(b->data, faceNormals.block->data, numFaceNormals * 0xc);
    Replace(faceNormals, b);
}

// 0x10025940: as many as there are primitives
void __cdecl GeometryChunk::DuplicateFaceNeighborArray()
{
    if (!faceNeighbors.block) return;
    auto* const b = AutoReferenceArray<FaceNeighbors>::Make(numPrimitives);
    if (numPrimitives) std::memcpy(b->data, faceNeighbors.block->data, numPrimitives * 0xc);
    Replace(faceNeighbors, b);
}

// 0x10025760: every vertex array the chunk does not share (slot 0x70) is duplicated (slot 0x1c)
void __cdecl GeometryChunk::DuplicateUnsharedArrays(Display* display)
{
    const int a = VCall<int>(this, 0x5c);
    const int b = VCall<int>(this, 0x60);
    for (int i = 0; i < a; ++i)
        for (int j = 0; j < b; ++j)
            if (!VCall<bool>(this, 0x70, i, j)) VCall<void>(this, 0x1c, display, i, j);
}

// 0x100259f0: a static index buffer (slot 0x94) is dropped (0x98) and the buffers made again (slot 4)
void __cdecl GeometryChunk::ResizeIndexArray(Display* display, unsigned int n)
{
    unsigned int keep = indexArray.Count();
    if (n <= keep) keep = n;
    IndexArray::Block* const b = IndexArray::Make(n);
    if (keep) std::memcpy(b->data, indexArray.block->data, keep * 2);
    if (indexArray.block != b) {
        indexArray.Release();
        indexArray.block = b;
        ++b->refs;
    }
    numIndices = n;
    if (indexArray.block && VCall<int>(this, 0x94, 0)) {
        VCall<void>(this, 0x98, 0, 0);
        VCall<void>(this, 0x10, display, false);
    }
    IndexArray::Drop(b);
}

// 0x10025b10: at least one item, the count as asked
void __cdecl GeometryChunk::ResizeFaceNormalArray(unsigned int n)
{
    unsigned int keep = n;
    if (numFaceNormals <= n) keep = numFaceNormals;
    auto* const b = AutoReferenceArray<Vector3>::Make(n ? n : 1);
    if (keep) std::memcpy(b->data, faceNormals.block->data, keep * 0xc);
    if (faceNormals.block != b) {
        faceNormals.Release();
        faceNormals.block = b;
        ++b->refs;
    }
    numFaceNormals = n;
    AutoReferenceArray<Vector3>::Drop(b);
}

// 0x10025bd0
void __cdecl GeometryChunk::ResizeFaceNeighborArray(unsigned int n)
{
    unsigned int keep = faceNeighbors.Count();
    if (faceNeighbors.block && n < keep) keep = n;
    auto* const b = AutoReferenceArray<FaceNeighbors>::Make(n ? n : 1);
    if (keep) std::memcpy(b->data, faceNeighbors.block->data, keep * 0xc);
    Replace(faceNeighbors, b);
}

// ---------------------------------------------------------------------------------------------------------------
// primitives (flags & 0x1f: 0 points, 1 line list, 2 line strip, 4 triangle list, 8 triangle strip, 0x10 fan)

namespace {

bool HasIndices(const GeometryChunk* c, const unsigned short*& idx)
{
    idx = c->GetIndexArray();
    return reinterpret_cast<const volatile uintptr_t&>(idx) != 0;
}

}  // namespace

// 0x10021d50
bool __cdecl GeometryChunk::GetVertexIndex(int primitive, int vertex, unsigned int& index) const
{
    if (primitive < 0 || numPrimitives <= static_cast<unsigned int>(primitive)) return false;
    const unsigned int v = static_cast<unsigned int>(vertex);
    switch (flags & 0x1f) {
    case 0:
        if (v != 0) return false;
        index = primitive;
        break;
    case 1:
        if (v > 1) return false;
        index = v + primitive * 2;
        break;
    case 2:
        if (v > 1) return false;
        index = primitive + v;
        break;
    case 4:
        if (v > 2) return false;
        index = v + primitive * 3;
        break;
    case 8:
        if (v > 2) return false;
        if ((primitive & 1) == 0)
            index = primitive + v;
        else if (v != 0)
            index = (primitive - v) + 3;
        else
            index = primitive;
        break;
    case 0x10:
        if (v > 2) return false;
        index = v == 0 ? 0 : primitive + v;
        break;
    default:
        return false;
    }
    const unsigned short* idx;
    if (HasIndices(this, idx)) index = idx[index];
    return true;
}

// 0x10021e70: no range test; an unknown type answers 0
unsigned int __cdecl GeometryChunk::GetVertexIndex(int primitive, int vertex) const
{
    unsigned int i;
    switch (flags & 0x1f) {
    case 0:
        i = primitive;
        break;
    case 1:
        i = vertex + primitive * 2;
        break;
    case 2:
        i = vertex + primitive;
        break;
    case 4:
        i = vertex + primitive * 2 + primitive;
        break;
    case 8:
        if ((primitive & 1) == 0 || vertex == 0)
            i = primitive + vertex;
        else
            i = (primitive - vertex) + 3;
        break;
    case 0x10:
        if (vertex == 0) {
            const unsigned short* idx;
            return HasIndices(this, idx) ? idx[0] : 0;
        }
        i = vertex + primitive;
        break;
    default:
        return 0;
    }
    const unsigned short* idx;
    return HasIndices(this, idx) ? idx[i] : i;
}

// 0x10021f50: a fan's triangle n is (0, n, n + 1); other types give (0, 0, 0)
void __cdecl GeometryChunk::GetTriangleIndexes(int p, unsigned long& a, unsigned long& b, unsigned long& c) const
{
    const unsigned int t = flags & 0x1f;
    if (t == 4) {
        a = p * 3;
        b = p * 3 + 1;
        c = p * 3 + 2;
    } else if (t == 8) {
        a = p;
        if (p & 1) {
            b = p + 2;
            c = p + 1;
        } else {
            b = p + 1;
            c = p + 2;
        }
    } else {
        a = 0;
        if (t == 0x10) {
            b = p;
            c = p + 1;
        } else {
            b = 0;
            c = 0;
        }
    }
    const unsigned short* idx;
    if (HasIndices(this, idx)) {
        a = idx[a];
        b = idx[b];
        c = idx[c];
    }
}

// ---------------------------------------------------------------------------------------------------------------
// chunk properties (by name node, in insertion order)

namespace {

GeometryChunk::ChunkProperty* FindProperty(GeometryChunk::PropertyChunk* head, const Node* name,
                                           GeometryChunk::PropertyChunk** chunk = nullptr)
{
    if (!head) return nullptr;
    GeometryChunk::PropertyChunk* c = head;
    GeometryChunk::ChunkProperty* p = c->data;
    for (;;) {
        if (p->name == name) {
            if (chunk) *chunk = c;
            return p;
        }
        ++p;
        if (c->data + c->count <= p) {
            c = c->next;
            if (c == head) return nullptr;
            p = c->data;
        }
    }
}

}  // namespace

// 0x10022250
bool __cdecl GeometryChunk::ChunkPropertyExists(const PString& name) const
{
    return FindProperty(properties.head, name.node) != nullptr;
}

// 0x10022000 / 0x100238b0: at the end, 0.0
bool __cdecl GeometryChunk::AddChunkProperty(const PString& name)
{
    if (ChunkPropertyExists(name)) return false;
    PropertyChunk* tail;
    if (!properties.head) {
        tail = static_cast<PropertyChunk*>(Alloc::New(0x24));
        tail->count = 0;
        properties.head = tail;
        tail->prev = tail;
        tail->next = tail;
    } else {
        tail = properties.head->prev;
        if (tail->count == 3) {
            PropertyChunk* const c = static_cast<PropertyChunk*>(Alloc::New(0x24));
            c->count = 0;
            c->next = properties.head;
            c->prev = tail;
            properties.head->prev = c;
            tail->next = c;
            tail = c;
        }
    }
    ChunkProperty& p = tail->data[tail->count++];
    ::new (&p.name) PString(name);
    p.value = 0.0f;
    return true;
}

// 0x10022110
bool __cdecl GeometryChunk::SetChunkProperty(const PString& name, float value)
{
    ChunkProperty* const p = FindProperty(properties.head, name.node);
    if (!p) return false;
    p->value = value;
    return true;
}

// 0x10022170
bool __cdecl GeometryChunk::GetChunkProperty(const PString& name, float& value) const
{
    const ChunkProperty* const p = FindProperty(properties.head, name.node);
    if (!p) return false;
    value = X87(p->value);
    return true;
}

// 0x100221d0 / 0x10023950: the later ones of its chunk move down (through the x87 stack); an emptied chunk goes
bool __cdecl GeometryChunk::RemoveChunkProperty(const PString& name)
{
    PropertyChunk* c;
    ChunkProperty* const p = FindProperty(properties.head, name.node, &c);
    if (!p) return false;
    Destroy(p->name);
    ChunkProperty* const end = c->data + c->count;
    for (ChunkProperty* q = p; q + 1 < end; ++q) {
        q->name = q[1].name;
        q->value = X87(q[1].value);
    }
    if (c->count == 1) {
        if (c == properties.head && c->next == c) {
            Alloc::Delete(c);
            properties.head = nullptr;
        } else {
            c->prev->next = c->next;
            c->next->prev = c->prev;
            PropertyChunk* const n = c->next;
            if (c) Alloc::Delete(c);
            if (c == properties.head) properties.head = n;
        }
    } else {
        --c->count;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// bones

// 0x10026cc0
int __cdecl GeometryChunk::AddInfluenceMatrix(int index)
{
    for (unsigned int k = 0; k < influences.count; ++k)
        if (influences.data[k]->index == index) return static_cast<int>(k);
    auto* const m = static_cast<VertexProgram::MatrixMap*>(Alloc::New(8));
    if (m) {
        m->matrix = nullptr;
        m->index = index;
    }
    const int at = static_cast<int>(influences.count);
    AppendMap(influences, m);
    return at;
}

// 0x10026db0
bool __cdecl GeometryChunk::AssignInfluenceMatrix(int index, const Matrix4* m)
{
    for (unsigned int k = 0; k < influences.count; ++k)
        if (influences.data[k]->index == index) {
            influences.data[k]->matrix = m;
            return true;
        }
    return false;
}

// 0x10026e10: the block stays
void __cdecl GeometryChunk::ResetMatrixArray()
{
    for (unsigned int k = 0; k < influences.count; ++k) {
        if (influences.data[k]) Alloc::Delete(influences.data[k]);
        influences.data[k] = nullptr;
    }
    influences.count = 0;
}

// ---------------------------------------------------------------------------------------------------------------
// the display

// 0x10026c90: the display's vertex type for the id (slot 0x10c) given to slot 0xa0
void __cdecl GeometryChunk::SetVertexType(Display* display, VertexTypeID id, int stream) const
{
    void* const t = VCall<void*>(display, 0x10c, id);
    VCall<void>(this, 0xa0, t, stream);
}

// 0x100269a0: the alpha of every colour (slot 0xdc) replaced, the stream marked dirty
void __cdecl GeometryChunk::SetOpacity(Display*, float o)
{
    opacity = o;
    const UDwordPtr colors = ItemArray<UDwordPtr>(this, 0xdc, 2, 0);
    if (!colors.data) return;
    const unsigned long keep = ~Convert(0, 0, 0, 1.0f);
    const unsigned long alpha = Convert(0, 0, 0, o);
    unsigned long* p = colors.data;
    for (unsigned int k = 0; k < numVertices; ++k) {
        *p &= keep;
        *p |= alpha;
        p = reinterpret_cast<unsigned long*>(reinterpret_cast<char*>(p) + colors.stride);
    }
    const int stream = VCall<int>(this, 0xb4, 2, 0);
    VCall<void>(this, 0x7c, true, 0, stream);
}

// 0x10026a90: the light's slot 1 lights the positions, normals and colours
void __cdecl GeometryChunk::UpdateLighting(Display* display, const DisplayLight* light, Material* material,
                                           const Matrix4& a, const Matrix4& b)
{
    if (!(flags & 0x20)) return;
    const unsigned int n = numVertices;
    const FloatPtr pos = ItemArray<FloatPtr>(this, 0xc4, 1, 0);
    const FloatPtr nor = ItemArray<FloatPtr>(this, 0xc4, 4, 0);
    const UDwordPtr col = ItemArray<UDwordPtr>(this, 0xdc, 2, 0);
    if (n && pos.data && nor.data && col.data)
        VCall<void>(light, 4, display, n, pos.stride, pos.data, nor.stride, nor.data, col.stride, col.data, &a, &b,
                    material);
}

// 0x10026b50: every colour's RGB becomes the material's emissive + ambient * its ambient colour, alpha kept
void __cdecl GeometryChunk::ResetToAmbient(Display*, const Material* material, const Color& ambient)
{
    const UDwordPtr colors = ItemArray<UDwordPtr>(this, 0xdc, 2, 0);
    if (!colors.data || !(flags & 0x20)) return;
    const float* const m = reinterpret_cast<const float*>(material);
    const float r = Add(m[0x64 / 4], Mul(ambient.r, m[0x34 / 4]));
    const float g = Add(m[0x68 / 4], Mul(ambient.g, m[0x38 / 4]));
    const float bl = Add(m[0x6c / 4], Mul(ambient.b, m[0x3c / 4]));
    const unsigned long mask = Convert(1.0f, 1.0f, 1.0f, 0);
    const unsigned long value = Convert(r, g, bl, m[0x70 / 4]) & mask;
    unsigned long* p = colors.data;
    for (unsigned int k = 0; k < numVertices; ++k) {
        *p &= ~mask;
        *p |= value;
        p = reinterpret_cast<unsigned long*>(reinterpret_cast<char*>(p) + colors.stride);
    }
    const int stream = VCall<int>(this, 0xb4, 2, 0);
    VCall<void>(this, 0x7c, true, 0, stream);
}

// 0x10026030: the display's debug flags (+0x71 bits 0..3 outlines, vertex normals, triangle normals, axes) with
// its state bit 2 cleared meanwhile (slots 0xa8 / 0xac around, 0x1f0 to set the bit)
void __cdecl GeometryChunk::RenderDebug(Display* display) const
{
    const unsigned char* const d = Raw(display);
    if (!(d[0x71] & 0xf) && !d[0x78] && !d[0x79]) return;
    const bool wasSet = (*StateFlags(display) >> 2 & 1) != 0;
    const bool saved = VCall<bool>(display, 0xa8);
    if (*StateFlags(display) & 4) {
        VCall<void>(display, 0x1f0, false);
        *StateFlags(display) &= 0xfb;
    }
    VCall<void>(display, 0xac, false);
    if (d[0x71] & 1) RenderOutlines(display);
    if (d[0x71] & 2) RenderVertexNormals(display);
    if (d[0x71] & 4) RenderTriangleNormals(display);
    if (d[0x71] & 8) RenderAxes(display);
    if ((*StateFlags(display) >> 2 & 1) != static_cast<unsigned char>(wasSet)) {
        VCall<void>(display, 0x1f0, wasSet);
        *StateFlags(display) ^= (static_cast<unsigned char>(wasSet) * 4 ^ *StateFlags(display)) & 4;
    }
    VCall<void>(display, 0xac, saved);
}

// 0x10026150: a triangle list's edges as lines (slot 0xe0). The original moves on two vertices per triangle while
// writing six, so each triangle keeps only its first edge and the draw reads unwritten stack after the last one.
void __cdecl GeometryChunk::RenderOutlines(Display* display) const
{
    if (!(flags & 4) || !numPrimitives) return;
    LineVertex* const v = static_cast<LineVertex*>(_alloca(numPrimitives * 0x60));
    const unsigned long color = Convert(0.8f, 0.8f, 0.8f, 1.0f);
    const FloatPtr pos = ItemArray<FloatPtr>(this, 0xc0, 1, 0);
    if (!pos.data) return;
    const unsigned short* const idx = GetIndexArray();
    auto at = [&](unsigned short i) { return reinterpret_cast<const float*>(reinterpret_cast<const char*>(pos.data) + i * pos.stride); };
    auto put = [&](LineVertex& o, unsigned short i) {
        const float* const p = at(i);
        o.x = X87(p[0]);
        o.y = X87(p[1]);
        o.z = X87(p[2]);
        o.color = color;
    };
    for (unsigned int k = 0; k < numPrimitives; ++k) {
        const unsigned short* const t = idx + k * 3;
        LineVertex* const o = v + k * 2;
        put(o[0], t[0]);
        put(o[1], t[1]);
        put(o[2], t[1]);
        put(o[3], t[2]);
        put(o[4], t[2]);
        put(o[5], t[0]);
    }
    VCall<void>(display, 0xe0, 3, 1, numPrimitives * 3, v, numPrimitives * 6);
}

// 0x10026300: a line from each position along its normal, the display's normal length long
void __cdecl GeometryChunk::RenderVertexNormals(Display* display) const
{
    if (!numVertices) return;
    const float len = NormalLength(display);
    LineVertex* const v = static_cast<LineVertex*>(_alloca(numVertices * 0x20));
    const unsigned long color = Convert(0.4f, 0.4f, 1.0f, 1.0f);
    const FloatPtr pos = ItemArray<FloatPtr>(this, 0xc0, 1, 0);
    const FloatPtr nor = ItemArray<FloatPtr>(this, 0xc0, 4, 0);
    if (!pos.data || !nor.data) return;
    const char* p = reinterpret_cast<const char*>(pos.data);
    const char* n = reinterpret_cast<const char*>(nor.data);
    for (unsigned int k = 0; k < numVertices; ++k) {
        const float* const pf = reinterpret_cast<const float*>(p);
        const float* const nf = reinterpret_cast<const float*>(n);
        LineVertex* const o = v + k * 2;
        o[0].x = X87(pf[0]);
        o[0].y = X87(pf[1]);
        o[0].z = X87(pf[2]);
        o[0].color = color;
        const float nx = nf[0], ny = nf[1], nz = nf[2];
        o[1].x = Add(pf[0], Mul(nx, len));
        o[1].y = Add(pf[1], Mul(ny, len));
        o[1].z = Add(pf[2], Mul(nz, len));
        o[1].color = color;
        p += pos.stride;
        n += nor.stride;
    }
    VCall<void>(display, 0xe0, 3, 1, numVertices, v, numVertices * 2);
}

// 0x100264a0: a line from each triangle's centre along its face normal (the buffer and the line count go by the
// primitive count, the loop by the face normal count)
void __cdecl GeometryChunk::RenderTriangleNormals(Display* display) const
{
    if (!(flags & 4) || !numFaceNormals || !faceNormals.block) return;
    const float len = NormalLength(display);
    LineVertex* const v = static_cast<LineVertex*>(_alloca(numPrimitives * 0x20));
    const unsigned long color = Convert(0.6f, 0.6f, 0.6f, 1.0f);
    const FloatPtr pos = ItemArray<FloatPtr>(this, 0xc0, 1, 0);
    if (!pos.data) return;
    const unsigned short* const idx = GetIndexArray();
    const Vector3* const fn = faceNormals.Data();
    auto at = [&](unsigned short i) { return reinterpret_cast<const float*>(reinterpret_cast<const char*>(pos.data) + i * pos.stride); };
    const float third = 0.33333334f;
    for (unsigned int k = 0; k < numFaceNormals; ++k) {
        const float* const a = at(idx[k * 3]);
        const float* const b = at(idx[k * 3 + 1]);
        const float* const c = at(idx[k * 3 + 2]);
        const float cx = Mul(Add(c[0], Add(b[0], a[0])), third);
        const float cy = Mul(Add(c[1], Add(b[1], a[1])), third);
        const float cz = Mul(Add(c[2], Add(b[2], a[2])), third);
        LineVertex* const o = v + k * 2;
        o[0] = {cx, cy, cz, color};
        const float* const nf = reinterpret_cast<const float*>(&fn[k]);
        o[1] = {Add(Mul(nf[0], len), cx), Add(cy, Mul(nf[1], len)), Add(cz, Mul(nf[2], len)), color};
    }
    VCall<void>(display, 0xe0, 3, 1, numPrimitives, v, numPrimitives * 2);
}

// 0x100266e0: the three axes, 50 normal lengths each way (the negative halves dark)
void __cdecl GeometryChunk::RenderAxes(Display* display) const
{
    const float len = NormalLength(display);
    const float pos = Mul(len, 50.0f);
    const float neg = Mul(len, -50.0f);
    LineVertex v[12] = {};
    const unsigned long cx = Convert(1.0f, 0, 0, 1.0f);
    const unsigned long dx = Convert(0.2f, 0, 0, 1.0f);
    const unsigned long cy = Convert(0, 1.0f, 0, 1.0f);
    const unsigned long dy = Convert(0, 0.2f, 0, 1.0f);
    const unsigned long cz = Convert(0, 0, 1.0f, 1.0f);
    const unsigned long dz = Convert(0, 0, 0.2f, 1.0f);
    v[0] = {0, 0, 0, cx};
    v[1] = {pos, 0, 0, cx};
    v[2] = {0, 0, 0, dx};
    v[3] = {neg, 0, 0, dx};
    v[4] = {0, 0, 0, cy};
    v[5] = {0, pos, 0, cy};
    v[6] = {0, 0, 0, dy};
    v[7] = {0, neg, 0, dy};
    v[8] = {0, 0, 0, cz};
    v[9] = {0, 0, pos, cz};
    v[10] = {0, 0, 0, dz};
    v[11] = {0, 0, neg, dz};
    VCall<void>(display, 0xe0, 3, 1, 6, v, 12);
}

// ---------------------------------------------------------------------------------------------------------------
// appending and loading

// 0x1002a720: the other chunk's vertices (stream by stream: slots 0x5c / 0x60 count them, 0xa4 the vertex type,
// 0x24 resizes, 0x30 / 0x34 the arrays, 0x7c marks dirty), indices shifted by our vertex count (0x28 resizes, 0x84
// marks dirty), primitives and face normals after ours
void __cdecl GeometryChunk::AppendChunk(Display* display, const GeometryChunk& o)
{
    const int streams = VCall<int>(this, 0x5c);
    const int buffers = VCall<int>(this, 0x60);
    const unsigned int theirs = o.numVertices;
    const unsigned int ours = numVertices;
    const unsigned int total = theirs + ours;
    for (int i = 0; i < streams; ++i) {
        const char* const type = VCall<const char*>(this, 0xa4, i);
        for (int j = 0; j < buffers; ++j) {
            VCall<void>(this, 0x24, display, total, i, j);
            const void* const src = VCall<const void*>(&o, 0x30, i, j);
            char* const dst = VCall<char*>(this, 0x34, i, j);
            const unsigned int stride =
                *reinterpret_cast<const unsigned int*>(*reinterpret_cast<const char* const*>(type + 0x10) + j * 0x10);
            std::memcpy(dst + stride * ours, src, stride * theirs);
            VCall<void>(this, 0x7c, true, i, j);
        }
    }
    const unsigned int myIndices = numIndices;
    const int count = static_cast<int>(o.numIndices);
    VCall<void>(this, 0x28, display, count + myIndices);
    const unsigned short* s = o.GetIndexArray();
    unsigned short* d = GetIndexArray() + myIndices;
    for (int k = count; k > 0; --k) *d++ = static_cast<unsigned short>(*s++ + static_cast<unsigned short>(ours));
    VCall<void>(this, 0x84, true);
    numPrimitives += o.numPrimitives;
    const unsigned int myFaces = numFaceNormals;
    const unsigned int faces = o.numFaceNormals;
    if (faces + myFaces) {
        ResizeFaceNormalArray(faces + myFaces);
        const Vector3* const from = o.faceNormals.Data();
        Vector3* const to = faceNormals.Data();
        std::memcpy(reinterpret_cast<char*>(to) + myFaces * 0xc, from, faces * 0xc);
    }
}

// GeometryStreamType (0x10): the source stride and its items {name, data type, element count, offset}
struct GeometryStreamItem {
    PStringCache::Node* name;
    unsigned int dataType;
    unsigned int count;
    unsigned int offset;
};
struct GeometryStreamType {
    unsigned int stride;
    unsigned int itemCount;
    unsigned int itemCapacity;
    GeometryStreamItem* items;
};

// 0x1002a8c0: the format matching our vertex type (slot 0xa4) exactly, else the one with most items in common
bool __cdecl GeometryChunk::GetLoadChunkDataType(const Array<Array<GeometryStreamType, 1>, 2>& formats,
                                                 unsigned int& index, bool& exact) const
{
    exact = false;
    unsigned int best = 0xffffffff;
    unsigned int bestCount = 0;
    const VertexType* const vt = VCall<const VertexType*>(this, 0xa4, 0);
    const unsigned int n = formats.count;
    if (!n) return false;
    const unsigned int typeStreams = vt->GetNumStreams();
    for (unsigned int f = 0; f < n; ++f) {
        const Array<GeometryStreamType, 1>& fmt = formats.data[f];
        bool ok = fmt.count == typeStreams;
        unsigned int matched = 0;
        for (unsigned int s = 0; s < fmt.count; ++s) {
            const GeometryStreamType& st = fmt.data[s];
            if (s < typeStreams && st.stride != vt->GetStreamList()[s].stride) ok = false;
            for (unsigned int k = 0; k < st.itemCount; ++k) {
                const GeometryStreamItem& it = st.items[k];
                unsigned int si, ii;
                const VertexType::Stream* sp;
                const VertexType::Item* ip;
                if (!vt->GetNamedItemData(*reinterpret_cast<const PString*>(&it.name), si, ii, sp, ip) ||
                    static_cast<unsigned int>(ip->dataType) != it.dataType ||
                    static_cast<unsigned int>(ip->count) != it.count) {
                    ok = false;
                    continue;
                }
                ++matched;
                if (si != s || static_cast<unsigned int>(ip->offset) != it.offset) ok = false;
            }
        }
        if (ok) {
            index = f;
            exact = true;
            return true;
        }
        if (matched && matched > bestCount) {
            best = f;
            bestCount = matched;
        }
    }
    if (best == 0xffffffff) return false;
    index = best;
    return true;
}

// 0x1002aa80: each item the vertex type has with the same format, copied vertex by vertex from data[stream]
bool __cdecl GeometryChunk::LoadChunkData(const Array<GeometryStreamType, 1>& streams, void** const data)
{
    const unsigned int n = numVertices;
    const VertexType* const vt = VCall<const VertexType*>(this, 0xa4, 0);
    for (unsigned int s = 0; s < streams.count; ++s) {
        const GeometryStreamType& st = streams.data[s];
        for (unsigned int k = 0; k < st.itemCount; ++k) {
            const GeometryStreamItem& it = st.items[k];
            unsigned int si, ii;
            const VertexType::Stream* sp;
            const VertexType::Item* ip;
            if (!vt->GetNamedItemData(*reinterpret_cast<const PString*>(&it.name), si, ii, sp, ip) ||
                static_cast<unsigned int>(ip->dataType) != it.dataType ||
                static_cast<unsigned int>(ip->count) != it.count)
                continue;
            const char* src = static_cast<const char*>(data[s]) + it.offset;
            char* dst = VCall<char*>(this, 0x34, 0, si) + ip->offset;
            const unsigned int stride = sp->stride;
            const unsigned int size = ElementSize(it.dataType) * it.count;
            for (unsigned int v = n; v; --v) {
                std::memcpy(dst, src, size);
                dst += stride;
                src += st.stride;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// the registry of chunk classes (the original's static)

namespace {

const void* RegistrationKey(GeometryChunk::RegistrationData* r) { return r->name.node; }

}  // namespace

// 0x10025ea0
bool __cdecl GeometryChunk::RegisterName(const PString& name, GeometryChunk*(__cdecl* create)())
{
    if (registrations.Find(name.node, RegistrationKey)) return false;
    auto* const r = static_cast<RegistrationData*>(Alloc::New(8));
    if (r) {
        ::new (&r->name) PString(name);
        r->create = create;
    }
    registrations.Insert(r, r->name.node);
    return true;
}

// 0x10025f70
GeometryChunk* __cdecl GeometryChunk::CreateRegisteredInstance(const PString& name)
{
    RegistrationData** const r = registrations.Find(name.node, RegistrationKey);
    if (r && (*r)->create) return (*r)->create();
    return nullptr;
}

// 0x10025fb0: the entries go, their slots are cleared (the buckets keep them)
void __cdecl GeometryChunk::ReleaseRegistrationData()
{
    registrations.ForEach([](RegistrationData*& r) {
        if (r) {
            Destroy(r->name.node);
            Alloc::Delete(r);
        }
        r = nullptr;
    });
}

// ---------------------------------------------------------------------------------------------------------------
// U5b2: interpolation and copy (0x10026e70..0x1002a720). The compiler unrolled the original's loops by four and
// picked operand orders per unrolled step; with two NaN operands SSE returns the first one, so the orders are
// kept per step (Add/Mul above). Lengths and the udword blends run on the x87 stack as in the original (at the
// caller's precision control).

namespace {

using TypeChunk = GeometryChunk::ItemTypes::Chunk;
using Item = VertexType::Item;

float* At(const GeometryChunk::FloatPtr& p, unsigned int i)
{
    return reinterpret_cast<float*>(reinterpret_cast<char*>(p.data) + p.stride * i);
}
unsigned long* At(const GeometryChunk::UDwordPtr& p, unsigned int i)
{
    return reinterpret_cast<unsigned long*>(reinterpret_cast<char*>(p.data) + p.stride * i);
}

// the vertices the compiler's four-step loop covers (it is entered for a count of at least 4, signed)
unsigned int Unrolled(unsigned int n) { return static_cast<int>(n) >= 4 ? (((n - 4) >> 2) + 1) * 4 : 0; }

// every item type of the list, as the original walks it (an item is used before the chunk end is checked)
template <class F>
void ForEachType(const GeometryChunk::ItemTypes& types, F f)
{
    TypeChunk* const head = *reinterpret_cast<TypeChunk* const*>(&types);
    TypeChunk* c = head;
    const VertexItemType* it = head ? head->data : nullptr;
    while (it) {
        f(*it);
        if (++it >= c->data + c->count) {
            c = c->next;
            if (c == head) break;
            it = c->data;
        }
    }
}

// the element count of an item both chunks have with the same data type and count (1..4), else 0; the copy
// also skips a missing item
int Matching(const GeometryChunk* src, const VertexType* st, const GeometryChunk* dst, const VertexType* dt,
             VertexItemType type, int& dataType, bool nullCheck)
{
    if (VCall<int>(src, 0xb0, type, 0) == -1) return 0;
    if (VCall<int>(dst, 0xb0, type, 0) == -1) return 0;
    const Item* const a = st->GetItem(type);
    const Item* const b = dt->GetItem(type);
    if (nullCheck && (!a || !b)) return 0;
    if (a->dataType != b->dataType || a->count != b->count) return 0;
    dataType = a->dataType;
    return a->count >= 1 && a->count <= 4 ? a->count : 0;
}

const VertexType* TypeOf(const GeometryChunk* c, int stream) { return VCall<const VertexType*>(c, 0xa4, stream); }

void SetDirty(GeometryChunk* c) { VCall<void>(c, 0x7c, 1, 0, -2); }  // slot 31

// StdMath::s_sqrtLUT (KernelStandard's exported table, read inline like the original) and the 1/sqrt seed of s
float Seed(float s)
{
    static const unsigned int* const lut = reinterpret_cast<const unsigned int*>(
        GetProcAddress(GetModuleHandleA("KernelStandard_7c.dll"), "?s_sqrtLUT@StdMath@Jet@@0PAIA"));
    unsigned int u;
    std::memcpy(&u, &s, 4);
    const unsigned int y = (((0x17cu - ((u >> 23) & 0xff)) << 22) & 0xff800000u) | lut[(u >> 15) & 0x1ff];
    float f;
    std::memcpy(&f, &y, 4);
    return f;
}

const float kHalf = 0.5f;
const double kThreeHalves = 1.5;
const float kTwo32 = 4294967296.0f;

struct X87Value {  // an x87 register as fstp tbyte stores it
    unsigned char b[10];
};

// 0x1002b170: |v| with two Newton steps, left unrounded
void Length(const float* v, X87Value& out)
{
    const float s = Add(Add(Mul(v[0], v[0]), Mul(v[1], v[1])), Mul(v[2], v[2]));
    const float y = Seed(s);
    unsigned char* const o = out.b;
    __asm {
        fld     s
        fld     kHalf
        fmul    st, st(1)
        fld     y
        fld     st(0)
        fmul    st, st(1)
        fmul    st, st(2)
        fld     kThreeHalves
        fld     st(0)
        fsubrp  st(2), st
        fxch    st(1)
        fmulp   st(2), st
        fld     st(1)
        fmul    st, st(2)
        fmulp   st(3), st
        fsubrp  st(2), st
        fmulp   st(1), st
        fmulp   st(1), st
        mov     eax, o
        fstp    tbyte ptr [eax]
    }
}

// |a| (rounded to float) / |b| (unrounded), rounded to float
float LengthRatio(const float* a, const float* b)
{
    X87Value la, lb;
    float l1, r;
    Length(a, la);
    __asm {
        lea     eax, la
        fld     tbyte ptr [eax]
        fstp    l1
    }
    Length(b, lb);
    __asm {
        lea     eax, lb
        fld     tbyte ptr [eax]
        fdivr   l1
        fstp    r
    }
    return r;
}

// 0x1002ae50: v scaled by its 1/length (two Newton steps)
void Normalize(float* v)
{
    const float s = Add(Add(Mul(v[0], v[0]), Mul(v[1], v[1])), Mul(v[2], v[2]));
    const float y = Seed(s);
    float r;
    __asm {
        fld     s
        fmul    kHalf
        fld     y
        fld     st(0)
        fmul    st, st(1)
        fmul    st, st(2)
        fld     kThreeHalves
        fld     st(0)
        fsubrp  st(2), st
        fxch    st(1)
        fmulp   st(2), st
        fld     st(1)
        fmul    st, st(2)
        fmulp   st(3), st
        fsubrp  st(2), st
        fmulp   st(1), st
        fstp    r
    }
    v[0] = Mul(v[0], r);
    v[1] = Mul(v[1], r);
    v[2] = Mul(v[2], r);
}

// udwords through the x87 stack: the sum of u[i] (as unsigned) * w[i], truncated to 64 bits, the low half
unsigned long UBlend(const unsigned long* u, const float* w, int n)
{
    unsigned short cw, cwt;
    __int64 q;
    __asm {
        mov     esi, u
        mov     edi, w
        mov     ecx, n
        fild    dword ptr [esi]
        cmp     dword ptr [esi], 0
        jge     ub_first
        fadd    kTwo32
    ub_first:
        fmul    dword ptr [edi]
        dec     ecx
        jz      ub_done
    ub_next:
        add     esi, 4
        add     edi, 4
        fild    dword ptr [esi]
        cmp     dword ptr [esi], 0
        jge     ub_positive
        fadd    kTwo32
    ub_positive:
        fmul    dword ptr [edi]
        faddp   st(1), st
        dec     ecx
        jnz     ub_next
    ub_done:
        fnstcw  cw
        movzx   eax, cw
        or      eax, 0C00h
        mov     cwt, ax
        fldcw   cwt
        fistp   q
        fldcw   cw
    }
    return static_cast<unsigned long>(q);
}

float CopyX87(const float* s) { return X87(*s); }

}  // namespace

// 0x10026e70: t from where pos lies between src positions i0 and i1 (0 / 1 when it is one of them)
void __cdecl GeometryChunk::InterpolateChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                 unsigned int dst, const Vector3& pos, const ItemTypes& types,
                                                 int stream)
{
    const VertexType* const st = TypeOf(src, stream);
    const VertexType* const dt = TypeOf(this, stream);
    const FloatPtr sp = ItemArray<FloatPtr>(src, 0xc0, 1, 0);
    const float* const p0 = At(sp, i0);
    const float* const q = &pos.x;
    float t;
    if (q[0] == p0[0] && q[1] == p0[1] && q[2] == p0[2]) {
        t = 0.0f;
    } else {
        const float* const p1 = At(sp, i1);
        if (q[0] == p1[0] && q[1] == p1[1] && q[2] == p1[2]) {
            t = 1.0f;
        } else {
            const float a[3] = {q[0] - p0[0], q[1] - p0[1], q[2] - p0[2]};
            const float b[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
            t = LengthRatio(a, b);
        }
    }
    ForEachType(types, [&](VertexItemType type) {
        int dataType;
        const int count = Matching(src, st, this, dt, type, dataType, false);
        if (count == 1 && dataType == 8) {
            const UDwordPtr s = ItemArray<UDwordPtr>(src, 0xd8, type, 0);
            const UDwordPtr d = ItemArray<UDwordPtr>(this, 0xdc, type, 0);
            const unsigned long a = *At(s, i0);
            const unsigned long diff = *At(s, i1) - a;
            *At(d, dst) = UBlend(&diff, &t, 1) + a;
            return;
        }
        if (!count || dataType != 7) return;
        static const unsigned int getters[5] = {0, 0xd0, 0xb8, 0xc0, 0xc8};
        const FloatPtr s = ItemArray<FloatPtr>(src, getters[count], type, 0);
        const FloatPtr d = ItemArray<FloatPtr>(this, getters[count] + 4, type, 0);
        const float* const a = At(s, i0);
        const float* const b = At(s, i1);
        float r[4];
        for (int c = 0; c < count; ++c) r[c] = Add(Mul(b[c] - a[c], t), a[c]);
        float* const o = At(d, dst);
        for (int c = 0; c < count; ++c) o[c] = r[c];  // (copied through x87 for 2..4: the sums are never signalling)
    });
    SetDirty(this);
}

// 0x100274c0: the weights of pos in the triangle (its normal the third input), then slot 62 for one vertex
void __cdecl GeometryChunk::InterpolateChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                 unsigned int i2, unsigned int dst, const Vector3& pos,
                                                 const ItemTypes& types, int stream)
{
    const FloatPtr sp = ItemArray<FloatPtr>(src, 0xc0, 1, 0);
    const float* const c = At(sp, i2);
    const float* const b = At(sp, i1);
    const float* const a = At(sp, i0);
    const float e[3] = {b[0] - c[0], b[1] - c[1], b[2] - c[2]};
    const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    float n[3];
    n[0] = Mul(d[1], e[2]) - Mul(d[2], e[1]);
    n[2] = Mul(d[0], e[1]) - Mul(d[1], e[0]);
    n[1] = Mul(d[2], e[0]) - Mul(d[0], e[2]);
    if (Add(Add(Mul(n[2], n[2]), Mul(n[1], n[1])), Mul(n[0], n[0])) != 0.0f) {
        Normalize(n);
        const Vector3 w = GetBaryCentricWeights(*reinterpret_cast<const Vector3*>(a),
                                                *reinterpret_cast<const Vector3*>(b),
                                                *reinterpret_cast<const Vector3*>(c), pos,
                                                *reinterpret_cast<const Vector3*>(n));
        VCall<void>(this, 0xf8, src, i0, i1, i2, &w, dst, 1u, &types, stream);
    }
}

// 0x10028d40
void __cdecl GeometryChunk::InterpolateAllChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                    unsigned int i2, const Vector3* const w, unsigned int first,
                                                    unsigned int count, const ItemTypes& types, int stream)
{
    const VertexType* const st = TypeOf(src, stream);
    const VertexType* const dt = TypeOf(this, stream);
    const unsigned int unrolled = Unrolled(count);
    ForEachType(types, [&](VertexItemType type) {
        int dataType;
        switch (Matching(src, st, this, dt, type, dataType, false)) {
        case 1:
            if (dataType == 7) {
                const FloatPtr s = ItemArray<FloatPtr>(src, 0xd0, type, 0);
                const FloatPtr d = ItemArray<FloatPtr>(this, 0xd4, type, 0);
                for (unsigned int k = 0; k < count; ++k) {
                    const float r = Add(Add(Mul(*At(s, i0), w[k].x), Mul(*At(s, i1), w[k].y)), Mul(*At(s, i2), w[k].z));
                    *At(d, first + k) = r;
                }
            } else if (dataType == 8) {
                const UDwordPtr s = ItemArray<UDwordPtr>(src, 0xd8, type, 0);
                const UDwordPtr d = ItemArray<UDwordPtr>(this, 0xdc, type, 0);
                for (unsigned int k = 0; k < count; ++k) {
                    const unsigned long u[3] = {*At(s, i0), *At(s, i1), *At(s, i2)};
                    *At(d, first + k) = UBlend(u, &w[k].x, 3);
                }
            }
            break;
        case 2:
            if (dataType == 7) {
                const FloatPtr s = ItemArray<FloatPtr>(src, 0xb8, type, 0);
                const FloatPtr d = ItemArray<FloatPtr>(this, 0xbc, type, 0);
                for (unsigned int k = 0; k < count; ++k) {
                    const float* const c = At(s, i2);
                    const float c0 = c[0], c1 = c[1];
                    const float* const b = At(s, i1);
                    const float b0 = b[0], b1 = b[1];
                    const float* const a = At(s, i0);
                    const float a0 = a[0], a1 = a[1];
                    const float x = Add(Add(Mul(a0, w[k].x), Mul(b0, w[k].y)), Mul(c0, w[k].z));
                    const float y = Add(Add(Mul(a1, w[k].x), Mul(b1, w[k].y)), Mul(c1, w[k].z));
                    float* const o = At(d, first + k);
                    o[0] = x;
                    o[1] = y;
                }
            }
            break;
        case 3:
            if (dataType == 7) {
                const FloatPtr s = ItemArray<FloatPtr>(src, 0xc0, type, 0);
                const FloatPtr d = ItemArray<FloatPtr>(this, 0xc4, type, 0);
                const float* const pa = At(s, i0);
                const float* const pb = At(s, i1);
                const float* const pc = At(s, i2);
                const float a[3] = {pa[0], pa[1], pa[2]};  // read once, before the loop
                const float b[3] = {pb[0], pb[1], pb[2]};
                const float c[3] = {pc[0], pc[1], pc[2]};
                for (unsigned int k = 0; k < count; ++k) {
                    const float wx = w[k].x, wy = w[k].y, wz = w[k].z;
                    float* const o = At(d, first + k);
                    if (k < unrolled) {
                        if ((k & 3) == 0) {
                            for (int j = 0; j < 3; ++j) o[j] = Add(Add(Mul(wz, c[j]), Mul(wx, a[j])), Mul(wy, b[j]));
                        } else {
                            for (int j = 0; j < 3; ++j) o[j] = Add(Add(Mul(wz, c[j]), Mul(wy, b[j])), Mul(wx, a[j]));
                        }
                    } else {
                        o[0] = Add(Add(Mul(wz, c[0]), Mul(wx, a[0])), Mul(wy, b[0]));
                        o[1] = Add(Add(Mul(wz, c[1]), Mul(wx, a[1])), Mul(wy, b[1]));
                        o[2] = Add(Add(Mul(wy, b[2]), Mul(wz, c[2])), Mul(wx, a[2]));
                    }
                }
            }
            break;
        case 4:
            if (dataType == 7) {
                const FloatPtr s = ItemArray<FloatPtr>(src, 0xc8, type, 0);
                const FloatPtr d = ItemArray<FloatPtr>(this, 0xcc, type, 0);
                for (unsigned int k = 0; k < count; ++k) {
                    float* const o = At(d, first + k);
                    float vc[4], vb[4], va[4];
                    const float* const c = At(s, i2);
                    for (int j = 0; j < 4; ++j) vc[j] = Mul(c[j], w[k].z);
                    const float* const b = At(s, i1);
                    for (int j = 0; j < 4; ++j) vb[j] = Mul(b[j], w[k].y);
                    const float* const a = At(s, i0);
                    for (int j = 0; j < 4; ++j) va[j] = Mul(a[j], w[k].x);
                    float r[4];
                    for (int j = 0; j < 4; ++j) r[j] = Add(vc[j], Add(vb[j], va[j]));
                    for (int j = 0; j < 4; ++j) o[j] = r[j];
                }
            }
            break;
        }
    });
    SetDirty(this);
}

// 0x10028d00
void __cdecl GeometryChunk::InterpolateAllChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                    unsigned int i2, const Vector3* const w, const ItemTypes& types,
                                                    int stream)
{
    VCall<void>(this, 0xf8, src, i0, i1, i2, w, 0u, numVertices, &types, stream);
}

// 0x10028c90: each vertex by its own position
void __cdecl GeometryChunk::InterpolateAllChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                    unsigned int i2, const ItemTypes& types, int stream)
{
    const unsigned int n = numVertices;
    const FloatPtr dp = ItemArray<FloatPtr>(this, 0xc4, 1, 0);
    for (unsigned int k = 0; k < n; ++k)
        InterpolateChunkData(src, i0, i1, i2, k, *reinterpret_cast<const Vector3*>(At(dp, k)), types, stream);
}

// 0x10027fe0
void __cdecl GeometryChunk::InterpolateAllChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                    const Vector2* const w, unsigned int first, unsigned int count,
                                                    const ItemTypes& types, int stream)
{
    const VertexType* const st = TypeOf(src, stream);
    const VertexType* const dt = TypeOf(this, stream);
    const unsigned int unrolled = Unrolled(count);
    ForEachType(types, [&](VertexItemType type) {
        int dataType;
        switch (Matching(src, st, this, dt, type, dataType, false)) {
        case 1:
            if (dataType == 7) {
                const FloatPtr s = ItemArray<FloatPtr>(src, 0xd0, type, 0);
                const FloatPtr d = ItemArray<FloatPtr>(this, 0xd4, type, 0);
                for (unsigned int k = 0; k < count; ++k) {
                    const float a = *At(s, i0), b = *At(s, i1);
                    const float r = k < unrolled && (k & 3) == 1 ? Add(Mul(b, w[k].y), Mul(a, w[k].x))
                                                                 : Add(Mul(a, w[k].x), Mul(b, w[k].y));
                    *At(d, first + k) = r;
                }
            } else if (dataType == 8) {
                const UDwordPtr s = ItemArray<UDwordPtr>(src, 0xd8, type, 0);
                const UDwordPtr d = ItemArray<UDwordPtr>(this, 0xdc, type, 0);
                for (unsigned int k = 0; k < count; ++k) {
                    const unsigned long u[2] = {*At(s, i0), *At(s, i1)};
                    *At(d, first + k) = UBlend(u, &w[k].x, 2);
                }
            }
            break;
        case 2:
            if (dataType == 7) {
                const FloatPtr s = ItemArray<FloatPtr>(src, 0xb8, type, 0);
                const FloatPtr d = ItemArray<FloatPtr>(this, 0xbc, type, 0);
                for (unsigned int k = 0; k < count; ++k) {
                    const float* const b = At(s, i1);
                    const float b0 = b[0], b1 = b[1];
                    const float* const a = At(s, i0);
                    const float a0 = a[0], a1 = a[1];
                    const float x = Add(Mul(a0, w[k].x), Mul(b0, w[k].y));
                    const float y = Add(Mul(a1, w[k].x), Mul(b1, w[k].y));
                    float* const o = At(d, first + k);
                    o[0] = x;
                    o[1] = y;
                }
            }
            break;
        case 3:
            if (dataType == 7) {
                const FloatPtr s = ItemArray<FloatPtr>(src, 0xc0, type, 0);
                const FloatPtr d = ItemArray<FloatPtr>(this, 0xc4, type, 0);
                const float* const pa = At(s, i0);
                const float* const pb = At(s, i1);
                const float a[3] = {pa[0], pa[1], pa[2]};  // read once, before the loop
                const float b[3] = {pb[0], pb[1], pb[2]};
                for (unsigned int k = 0; k < count; ++k) {
                    const float wx = w[k].x, wy = w[k].y;
                    float* const o = At(d, first + k);
                    if (k < unrolled && (k & 3) != 1) {
                        for (int j = 0; j < 3; ++j) o[j] = Add(Mul(wy, b[j]), Mul(wx, a[j]));
                    } else {
                        for (int j = 0; j < 3; ++j) o[j] = Add(Mul(wx, a[j]), Mul(wy, b[j]));
                    }
                }
            }
            break;
        case 4:
            if (dataType == 7) {
                const FloatPtr s = ItemArray<FloatPtr>(src, 0xc8, type, 0);
                const FloatPtr d = ItemArray<FloatPtr>(this, 0xcc, type, 0);
                for (unsigned int k = 0; k < count; ++k) {
                    float* const o = At(d, first + k);
                    float vb[4], va[4];
                    const float* const b = At(s, i1);
                    for (int j = 0; j < 4; ++j) vb[j] = Mul(b[j], w[k].y);
                    const float* const a = At(s, i0);
                    for (int j = 0; j < 4; ++j) va[j] = Mul(a[j], w[k].x);
                    float r[4];
                    for (int j = 0; j < 4; ++j) r[j] = Add(vb[j], va[j]);
                    for (int j = 0; j < 4; ++j) o[j] = r[j];
                }
            }
            break;
        }
    });
    SetDirty(this);
}

// 0x10027fa0
void __cdecl GeometryChunk::InterpolateAllChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                    const Vector2* const w, const ItemTypes& types, int stream)
{
    VCall<void>(this, 0x104, src, i0, i1, w, 0u, numVertices, &types, stream);
}

// 0x10027d40: the weights (1 - t, t) of each vertex position along src i0..i1, then slot 65
void __cdecl GeometryChunk::InterpolateAllChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                    const ItemTypes& types, int stream)
{
    const unsigned int n = numVertices;
    const FloatPtr sp = ItemArray<FloatPtr>(src, 0xc0, 1, 0);
    const FloatPtr dp = ItemArray<FloatPtr>(this, 0xc4, 1, 0);
    const unsigned long long bytes = static_cast<unsigned long long>(n) * 8;
    float* const w = static_cast<float*>(Alloc::New(bytes >> 32 ? 0xffffffffu : static_cast<unsigned int>(bytes)));
    for (unsigned int k = 0; k < n; ++k) {
        const float* const p0 = At(sp, i0);
        const float* const q = At(dp, k);
        float t;
        if (q[0] == p0[0] && q[1] == p0[1] && q[2] == p0[2]) {
            t = 0.0f;
        } else {
            const float* const p1 = At(sp, i1);
            if (q[0] == p1[0] && q[1] == p1[1] && q[2] == p1[2]) {
                t = 1.0f;
            } else {
                const float a[3] = {q[0] - p0[0], q[1] - p0[1], q[2] - p0[2]};
                const float b[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
                t = LengthRatio(a, b);
            }
        }
        w[k * 2] = 1.0f - t;
        w[k * 2 + 1] = t;
    }
    VCall<void>(this, 0x104, src, i0, i1, reinterpret_cast<const Vector2*>(w), 0u, n, &types, stream);
    if (w) Alloc::Delete(w);
}

// 0x10027680
void __cdecl GeometryChunk::InterpolateAllChunkData(const GeometryChunk* src, unsigned int i, const float* const w,
                                                    unsigned int first, unsigned int count, const ItemTypes& types,
                                                    int stream)
{
    const VertexType* const st = TypeOf(src, stream);
    const VertexType* const dt = TypeOf(this, stream);
    ForEachType(types, [&](VertexItemType type) {
        int dataType;
        const int n = Matching(src, st, this, dt, type, dataType, false);
        if (n == 1 && dataType == 8) {
            const UDwordPtr s = ItemArray<UDwordPtr>(src, 0xd8, type, 0);
            const UDwordPtr d = ItemArray<UDwordPtr>(this, 0xdc, type, 0);
            for (unsigned int k = 0; k < count; ++k) *At(d, first + k) = UBlend(At(s, i), &w[k], 1);
            return;
        }
        if (!n || dataType != 7) return;
        static const unsigned int getters[5] = {0, 0xd0, 0xb8, 0xc0, 0xc8};
        const FloatPtr s = ItemArray<FloatPtr>(src, getters[n], type, 0);
        const FloatPtr d = ItemArray<FloatPtr>(this, getters[n] + 4, type, 0);
        for (unsigned int k = 0; k < count; ++k) {
            const float* const a = At(s, i);
            float r[4];
            for (int j = 0; j < n; ++j) r[j] = Mul(a[j], w[k]);
            float* const o = At(d, first + k);
            for (int j = 0; j < n; ++j) o[j] = r[j];
        }
    });
    SetDirty(this);
}

// 0x10027640
void __cdecl GeometryChunk::InterpolateAllChunkData(const GeometryChunk* src, unsigned int i, const float* const w,
                                                    const ItemTypes& types, int stream)
{
    VCall<void>(this, 0x110, src, i, w, 0u, numVertices, &types, stream);
}

// 0x10029fc0: the floats through the x87 stack (a signalling NaN comes out quiet)
void __cdecl GeometryChunk::CopyChunkData(const GeometryChunk* src, unsigned int from, unsigned int to,
                                          unsigned int count, const ItemTypes& types, int stream)
{
    const VertexType* const st = TypeOf(src, stream);
    const VertexType* const dt = TypeOf(this, stream);
    ForEachType(types, [&](VertexItemType type) {
        int dataType;
        const int n = Matching(src, st, this, dt, type, dataType, true);
        if (n == 1 && dataType == 8) {
            const UDwordPtr s = ItemArray<UDwordPtr>(src, 0xd8, type, 0);
            const UDwordPtr d = ItemArray<UDwordPtr>(this, 0xdc, type, 0);
            for (unsigned int k = 0; k < count; ++k) *At(d, to + k) = *At(s, from + k);
            return;
        }
        if (!n || dataType != 7) return;
        static const unsigned int getters[5] = {0, 0xd0, 0xb8, 0xc0, 0xc8};
        const FloatPtr s = ItemArray<FloatPtr>(src, getters[n], type, 0);
        const FloatPtr d = ItemArray<FloatPtr>(this, getters[n] + 4, type, 0);
        for (unsigned int k = 0; k < count; ++k) {
            const float* const a = At(s, from + k);
            float* const o = At(d, to + k);
            for (int j = 0; j < n; ++j) o[j] = CopyX87(a + j);
        }
    });
    SetDirty(this);
}

// 0x10029e50
void __cdecl GeometryChunk::GetAllChunkWeights(const Vector3& a, const Vector3& b, const Vector3& c,
                                               Vector3* const out) const
{
    const unsigned int n = numVertices;
    const FloatPtr pp = ItemArray<FloatPtr>(this, 0xc0, 1, 0);
    const float e[3] = {b.x - c.x, b.y - c.y, b.z - c.z};
    const float d[3] = {a.x - b.x, a.y - b.y, a.z - b.z};
    float nv[3];
    nv[1] = Mul(d[2], e[0]) - Mul(e[2], d[0]);
    nv[2] = Mul(e[1], d[0]) - Mul(d[1], e[0]);
    nv[0] = Mul(d[1], e[2]) - Mul(d[2], e[1]);
    if (Add(Add(Mul(nv[2], nv[2]), Mul(nv[1], nv[1])), Mul(nv[0], nv[0])) == 0.0f) return;
    Normalize(nv);
    for (unsigned int k = 0; k < n; ++k) {
        const Vector3 r = GetBaryCentricWeights(a, b, c, *reinterpret_cast<const Vector3*>(At(pp, k)),
                                                *reinterpret_cast<const Vector3*>(nv));
        out[k].x = CopyX87(&r.x);
        out[k].y = CopyX87(&r.y);
        out[k].z = CopyX87(&r.z);
    }
}

}  // namespace Jet
