// Render3DCore_7c unit U6: the geometry interface (Geometry, TriangleGeometry) and the concrete geometry chunk
// templates this module instantiates. Layouts and behaviour follow the TS12 assembly (addresses per member in
// Render3DCore_7c/Geometry.cpp and notes/agents/u6_geometry.md).
//
// GeometryChunkTemplate<Vertex, Base> is the vertex-block chunk of the module: Base is a RenderSystem geometry chunk
// class (GeometryChunkSingleStream for every TS12 instantiation) and Vertex one of the PackedVertex records of
// VertexFormats.h. The template keeps its vertices in one Alloc block at +0x5c: {references, count, then the records
// at +0x10}. GeometryChunkNmlTemplate adds a second block of Vector3 normals at +0x60 and serves the normal item
// (VertexItemType 4) from it.
//
// These templates are not exported by the original (only the two instantiations the multipass chunks derive from have
// export entries, and those stay forwarded). They are header-only here so the materials, the multipass chunks and the
// render primitives can instantiate the same classes; the bodies reproduce every local template function in the
// original's range (0x1000ca00..0x10076078).
#pragma once

#include <jet/Geometry.h>
#include <jet/Math.h>
#include <jet/VertexFormats.h>
#include <jet/VCall.h>

#include <cstring>
#include <cstdint>
#include <new>

namespace Jet {

class Display;
class ViewPort;

// ---- the geometry interface (0x44 bytes: vftable, then the stored matrix) ---------------------------------------

// TS12 Geometry: vftable at +0, the model-view matrix at +4, 0x44 bytes in all. The exported ordinary members are
// __cdecl (this on the stack, ahead of the hidden by-value return slot) and the constructors, the destructor and the
// assignment are __thiscall. The vtable is six words: 0 is the vector deleting destructor, slots 1..4 are the module's
// shared `xor eax,eax; ret` stub (0x100079e0) and slot 5 is a null word. TS12 does not export those methods, so their
// signatures are not recoverable; the placeholders below keep the slot count and the answer-0 behaviour, which is all a
// class derived outside this module sees. (C++ cannot emit the null fifth entry, so our vtable stops after slot 4.)
class Geometry {
public:
    Geometry();
    Geometry(const Geometry& o);
    virtual ~Geometry();
    Geometry& operator=(const Geometry& o);

    // Stores the matrix only for the main viewport (ViewPort::viewportNumber at +0x58 equals 0); otherwise the stored
    // matrix is left alone.
    void __cdecl StoreMatrix(const ViewPort& view, const Matrix4& matrix);
    // The stored matrix, copied out. The viewport argument is not read (the original ignores it).
    Matrix4 __cdecl GetRenderMatrix(const ViewPort& view) const;

    virtual int vf1() { return 0; }
    virtual int vf2() const { return 0; }
    virtual int vf3() { return 0; }
    virtual int vf4() const { return 0; }

protected:
    // copies the stored matrix (the original's exported helpers share this body with operator=)
    void __cdecl InitInstance(Geometry& to) const;

    Matrix4 modelViewMatrix;  // +0x04
};
JET_LAYOUT32(sizeof(Geometry) == 0x44);

// TS12 TriangleGeometry: no fields of its own, 0x44 bytes. Its vtable is 17 slots: the Geometry destructor followed by
// sixteen stubs, all the same answer-0 function (0x100079e0). The extra methods are not exported either; the
// placeholders keep the length.
class TriangleGeometry : public Geometry {
public:
    TriangleGeometry();
    TriangleGeometry(const TriangleGeometry& o);
    ~TriangleGeometry() override;
    TriangleGeometry& operator=(const TriangleGeometry& o);

    virtual int vf5() { return 0; }
    virtual int vf6() const { return 0; }
    virtual int vf7() { return 0; }
    virtual int vf8() const { return 0; }
    virtual int vf9() { return 0; }
    virtual int vf10() const { return 0; }
    virtual int vf11() { return 0; }
    virtual int vf12() const { return 0; }
    virtual int vf13() { return 0; }
    virtual int vf14() const { return 0; }
    virtual int vf15() { return 0; }
    virtual int vf16() const { return 0; }
};
JET_LAYOUT32(sizeof(TriangleGeometry) == 0x44);
JET_LAYOUT32(sizeof(Matrix4) == 0x40);

// ---- the vertex blocks of the chunk templates -------------------------------------------------------------------

// Every template block is one Alloc block: {references, count, two words left as the heap had them, items at +0x10}.
// Only the first two words are ever written (the original leaves the item size and capacity fields uninitialised).
struct GeometryChunkBlock {
    unsigned int references;
    unsigned int count;
};

namespace GeometryChunkDetail {

inline GeometryChunkBlock* MakeBlock(unsigned int count, unsigned int itemSize)
{
    auto* block = static_cast<GeometryChunkBlock*>(Alloc::New(count * itemSize + 0x10));
    block->count = 0;
    block->references = 1;
    block->count += count;
    return block;
}

// the release the templates inline: the block goes only with its last reference, and the slot is cleared only then
inline void ReleaseBlock(GeometryChunkBlock*& slot)
{
    if (!slot) return;
    if (--slot->references == 0) {
        Alloc::Delete(slot);
        slot = nullptr;
    }
}

// the inlined assignment: the destination's old block goes first unless it is the same one
inline void ShareBlock(GeometryChunkBlock*& to, GeometryChunkBlock* from)
{
    if (to == from) return;
    ReleaseBlock(to);
    to = from;
    if (from) ++from->references;
}

inline void* BlockData(const GeometryChunkBlock* block)
{
    return reinterpret_cast<unsigned char*>(const_cast<GeometryChunkBlock*>(block)) + 0x10;
}

// the display's vertex buffer manager (+0x20c)
inline void* BufferManager(const Display* display)
{
    return *reinterpret_cast<void* const*>(reinterpret_cast<const unsigned char*>(display) + 0x20c);
}

// an AutoReferenceArray's items, as the original computes them (0x10 past a null block is 0x10)
inline const unsigned short* IndexData(const void* block)
{
    return reinterpret_cast<const unsigned short*>(reinterpret_cast<std::uintptr_t>(block) + 0x10);
}

// stream 0's stride: the streams block at +0x10 of the vertex type starts with the first stream's stride
inline unsigned int Stride(const VertexType* type)
{
    return **reinterpret_cast<const unsigned int* const*>(reinterpret_cast<const unsigned char*>(type) + 0x10);
}

}  // namespace GeometryChunkDetail

// ---- GeometryChunkTemplate --------------------------------------------------------------------------------------

// One stream of Vertex records in memory. Layout over Base: +0x00 the chunk, +0x5c the vertex block (0x60 bytes over
// GeometryChunkSingleStream). Every virtual the original's template overrides is here; the rest come from Base.
template <class Vertex, class Base>
class GeometryChunkTemplate : public Base {
public:
    GeometryChunkTemplate() : block(nullptr) {}

    GeometryChunkTemplate(unsigned int vertices, unsigned int primitives, unsigned int indices,
                          unsigned int faceNormals, unsigned int flags)
        : Base(vertices, primitives, indices, faceNormals, flags)
    {
        block = GeometryChunkDetail::MakeBlock(vertices, sizeof(Vertex));
    }

protected:
    // The original's exported destructor is protected (??1...MAE@XZ); the access is part of the mangled name.
    ~GeometryChunkTemplate() override { GeometryChunkDetail::ReleaseBlock(block); }

public:
    // 0x2c9d0 (slot 2): sizes the copy from the source chunk's flags.
    GeometryChunk* __cdecl CreateInstance(unsigned int vertices, unsigned int primitives, unsigned int indices,
                                          unsigned int faceNormals) const override
    {
        void* const memory = Alloc::New(sizeof(GeometryChunkTemplate));
        if (!memory) return nullptr;
        return ::new (memory) GeometryChunkTemplate(vertices, primitives, indices, faceNormals, this->flags);
    }

    // 0x2ca60 (slot 3): a default copy that shares the vertex block (and only that block).
    GeometryChunk* __cdecl CreateInstance() const override
    {
        void* const memory = Alloc::New(sizeof(GeometryChunkTemplate));
        if (!memory) return nullptr;
        auto* const made = ::new (memory) GeometryChunkTemplate;
        Base::InitInstance(*made);
        GeometryChunkDetail::ShareBlock(made->block, block);
        return made;
    }

    // 0x3e2a0 (slot 4): the vertex and index buffers, unless dynamic (0x10000) or already there.
    bool __cdecl InitializeBuffers(Display* display, bool upload) override
    {
        using namespace GeometryChunkDetail;
        const unsigned long f = this->flags;
        const StaticBuffer* const hadVertex = this->GetStaticVertexBuffer(0, 0);
        const VertexType* const type = this->AssignVertexType(display);
        if (!(f & 0x10000) && !hadVertex) {
            if (!type) return false;
            auto* buffer = static_cast<StaticBuffer*>(VCall<void*>(display, 0x104, this->maxVertices, Stride(type)));
            if (!buffer) return false;
            if (upload) {
                unsigned int out = 0;
                if (!VCall<unsigned char>(BufferManager(display), 0x20, type, buffer,
                                          this->GetVertexArray(0, 0), this->numVertices, 1, &out, 0)) {
                    buffer->ReleaseReference();
                    return false;
                }
                this->SetVertexDataDirty(false, 0, 0);
            } else {
                this->SetVertexDataDirty(true, 0, 0);
            }
            this->SetStaticVertexBuffer(buffer, 0, 0);
            buffer->ReleaseReference();
        }
        const StaticBuffer* const hadIndex = this->GetStaticIndexBuffer(0);
        if (!(f & 0x100) && !hadIndex) {
            auto* buffer = static_cast<StaticBuffer*>(VCall<void*>(display, 0x108, this->GetMaxIndices(), 0));
            if (!buffer) return false;
            if (upload) {
                unsigned int out = 0;
                const void* const data = IndexData(this->indexArray.block);
                if (VCall<int>(BufferManager(display), 0x2c, buffer, data, this->numIndices, 1, &out) == -1) {
                    buffer->ReleaseReference();
                    return false;
                }
                this->SetIndexDataDirty(false);
            } else {
                this->SetIndexDataDirty(true);
            }
            this->SetStaticIndexBuffer(buffer, 0);
            buffer->ReleaseReference();
        }
        return true;
    }

    // 0x5c500 (slot 5): uploads into the static buffers that must already exist.
    bool __cdecl LoadBuffers(Display* display) override
    {
        using namespace GeometryChunkDetail;
        const unsigned long f = this->flags;
        void* const manager = BufferManager(display);
        if (!(f & 0x10000)) {
            const StaticBuffer* const buffer = this->GetStaticVertexBuffer(0, 0);
            if (!buffer) return false;
            unsigned int out = 0;
            if (!VCall<unsigned char>(manager, 0x20, this->vertexType, buffer, this->GetVertexArray(0, 0),
                                      this->numVertices, 1, &out, 0))
                return false;
            this->SetVertexDataDirty(false, 0, 0);
        }
        if (!(f & 0x100)) {
            const StaticBuffer* const buffer = this->GetStaticIndexBuffer(0);
            if (!buffer) return false;
            unsigned int out = 0;
            const void* const data = IndexData(this->indexArray.block);
            if (VCall<int>(manager, 0x2c, buffer, data, this->numIndices, 1, &out) == -1) {
                buffer->ReleaseReference();
                return false;
            }
            this->SetIndexDataDirty(false);
        }
        return true;
    }

    // 0x46720 (slot 7): copies maxVertices records whatever the old block holds, then rebinds the static buffer.
    void __cdecl DuplicateVertexArray(Display* display, int, int) override
    {
        using namespace GeometryChunkDetail;
        if (!block) return;
        GeometryChunkBlock* copy = MakeBlock(this->maxVertices, sizeof(Vertex));
        std::memcpy(BlockData(copy), BlockData(block), this->maxVertices * sizeof(Vertex));
        ShareBlock(block, copy);
        if (this->GetStaticVertexBuffer(0, 0)) {
            this->SetStaticVertexBuffer(nullptr, 0, 0);
            this->InitializeBuffers(display, false);
        }
        ReleaseBlock(copy);
    }

    // 0x3f1c0 (slot 8)
    void __cdecl DuplicateIndexArray(Display* display) override
    {
        GeometryChunk::DuplicateIndexArray(display);
        if (this->GetStaticIndexBuffer(0)) {
            this->SetStaticIndexBuffer(nullptr, 0);
            this->InitializeBuffers(display, false);
        }
    }

    // 0x40880 (slot 9): keeps min(maxVertices, n) records, then max and count become n.
    void __cdecl ResizeVertexArray(Display* display, unsigned int n, int, int) override
    {
        using namespace GeometryChunkDetail;
        const unsigned int keep = this->maxVertices < n ? this->maxVertices : n;
        GeometryChunkBlock* copy = MakeBlock(n, sizeof(Vertex));
        if (keep) std::memcpy(BlockData(copy), BlockData(block), keep * sizeof(Vertex));
        ShareBlock(block, copy);
        this->maxVertices = n;
        this->numVertices = n;
        if (block && this->GetStaticVertexBuffer(0, 0)) {
            this->SetStaticVertexBuffer(nullptr, 0, 0);
            this->InitializeBuffers(display, false);
        }
        ReleaseBlock(copy);
    }

    // 0x3ee20 (slots 12/13): 0x10 with no block
    void* __cdecl GetVertexArray(int, int) override { return BlockAddress(0x10); }
    const void* __cdecl GetVertexArray(int, int) const override { return BlockAddress(0x10); }

    // 0x2cc50 (slot 39): the vertex type of the record, asked of the display the first time
    const VertexType* __cdecl AssignVertexType(Display* display) const override
    {
        if (!this->vertexType) GeometryChunk::SetVertexType(display, Vertex::GetVertexTypeID(), 0);
        return &this->GetVertexType(0);
    }

    // 0x27d00 (slot 42)
    VertexTypeID __cdecl GetVertexTypeID(int) const override { return Vertex::GetVertexTypeID(); }

    // 0x5b110 (slot 43)
    int __cdecl GetVertexSize(int) const override { return sizeof(Vertex); }

    // 0x2cc80 (slot 44): the offset table is the record's own static array in RenderSystem
    int __cdecl GetItemOffset(VertexItemType type, int) const override { return Vertex::GetItemOffset(type); }

    // 0x2cc90 (slots 46..55): one body for every item view; the stride of a missing item is 0
    template <class P>
    P ItemView(VertexItemType type) const
    {
        P r;
        const int offset = Vertex::GetItemOffset(type);
        if (offset < 0) {
            r.stride = 0;
            r.data = nullptr;
            return r;
        }
        r.stride = sizeof(Vertex);
        r.data = reinterpret_cast<decltype(r.data)>(BlockAddress(0x10 + offset));
        return r;
    }

#define JET_GEOMETRY_ITEM(Getter, Ptr, ConstPtr)                                          \
    GeometryChunk::Ptr __cdecl Getter(VertexItemType type, int) override                  \
    {                                                                                     \
        return ItemView<GeometryChunk::Ptr>(type);                                        \
    }                                                                                     \
    GeometryChunk::ConstPtr __cdecl Getter(VertexItemType type, int) const override       \
    {                                                                                     \
        return ItemView<GeometryChunk::ConstPtr>(type);                                   \
    }
    JET_GEOMETRY_ITEM(GetVector2Array, Vector2Ptr, ConstVector2Ptr)
    JET_GEOMETRY_ITEM(GetVector3Array, Vector3Ptr, ConstVector3Ptr)
    JET_GEOMETRY_ITEM(GetVector4Array, Vector4Ptr, ConstVector4Ptr)
    JET_GEOMETRY_ITEM(GetFloatArray, FloatPtr, ConstFloatPtr)
    JET_GEOMETRY_ITEM(GetUDwordArray, UDwordPtr, ConstUDwordPtr)
#undef JET_GEOMETRY_ITEM

protected:
    // 0x2d130: Base::InitInstance, then the vertex block is shared
    void __cdecl InitInstance(GeometryChunkTemplate& to) const
    {
        Base::InitInstance(to);
        GeometryChunkDetail::ShareBlock(to.block, block);
    }

    void* BlockAddress(std::uintptr_t offset) const
    {
        return reinterpret_cast<unsigned char*>(block) + offset;
    }

public:
    GeometryChunkBlock* block;  // +0x5c
};

// ---- GeometryChunkNmlTemplate -----------------------------------------------------------------------------------

// The template with a separate per-vertex normal array: a Vector3 block at +0x60 (0x64 bytes over
// GeometryChunkSingleStream). GetVector3Array(VIT_NORMAL = 4) reads that block instead of the vertex records.
template <class Vertex, class Base>
class GeometryChunkNmlTemplate : public GeometryChunkTemplate<Vertex, Base> {
public:
    GeometryChunkNmlTemplate() : normals(nullptr) {}

    GeometryChunkNmlTemplate(unsigned int vertices, unsigned int primitives, unsigned int indices,
                             unsigned int faceNormals, unsigned int flags)
        : GeometryChunkTemplate<Vertex, Base>(vertices, primitives, indices, faceNormals, flags)
    {
        normals = GeometryChunkDetail::MakeBlock(vertices, sizeof(Vector3));
    }

    ~GeometryChunkNmlTemplate() override { GeometryChunkDetail::ReleaseBlock(normals); }

    // 0xcd20 (slot 2)
    GeometryChunk* __cdecl CreateInstance(unsigned int vertices, unsigned int primitives, unsigned int indices,
                                          unsigned int faceNormals) const override
    {
        void* const memory = Alloc::New(sizeof(GeometryChunkNmlTemplate));
        if (!memory) return nullptr;
        return ::new (memory) GeometryChunkNmlTemplate(vertices, primitives, indices, faceNormals, this->flags);
    }

    // 0xcda0 (slot 3): the original's shared InitInstance helper shares both blocks, so a default copy shares the
    // vertex block and the normal block.
    GeometryChunk* __cdecl CreateInstance() const override
    {
        void* const memory = Alloc::New(sizeof(GeometryChunkNmlTemplate));
        if (!memory) return nullptr;
        auto* const made = ::new (memory) GeometryChunkNmlTemplate;
        InitInstance(*made);
        return made;
    }

    // 0xce20 (slot 7): the base's copy, then the normal block is copied (maxVertices records) if it exists
    void __cdecl DuplicateVertexArray(Display* display, int i, int stream) override
    {
        using namespace GeometryChunkDetail;
        GeometryChunkTemplate<Vertex, Base>::DuplicateVertexArray(display, i, stream);
        if (!normals) return;
        GeometryChunkBlock* copy = MakeBlock(this->maxVertices, sizeof(Vector3));
        std::memcpy(BlockData(copy), BlockData(normals), this->maxVertices * sizeof(Vector3));
        ShareBlock(normals, copy);
        ReleaseBlock(copy);
    }

    // 0xcee0 (slot 9): the normal block first (min(old count, n) records), then the base resize
    void __cdecl ResizeVertexArray(Display* display, unsigned int n, int i, int stream) override
    {
        using namespace GeometryChunkDetail;
        if (normals) {
            const unsigned int keep = normals->count < n ? normals->count : n;
            GeometryChunkBlock* copy = MakeBlock(n, sizeof(Vector3));
            if (keep) std::memcpy(BlockData(copy), BlockData(normals), keep * sizeof(Vector3));
            ShareBlock(normals, copy);
            ReleaseBlock(copy);
        }
        GeometryChunkTemplate<Vertex, Base>::ResizeVertexArray(display, n, i, stream);
    }

    // 0xcfb0 (slots 48/49): the normal item comes from the normal block (stride 12), everything else from the
    // vertex records (stride sizeof(Vertex)). A null normal block gives 0x10, as the original does.
    GeometryChunk::Vector3Ptr __cdecl GetVector3Array(VertexItemType type, int stream) override
    {
        if (static_cast<int>(type) == 4) {
            GeometryChunk::Vector3Ptr r;
            r.stride = sizeof(Vector3);
            r.data = reinterpret_cast<Vector3*>(GeometryChunkDetail::BlockData(normals));
            return r;
        }
        return GeometryChunkTemplate<Vertex, Base>::GetVector3Array(type, stream);
    }
    GeometryChunk::ConstVector3Ptr __cdecl GetVector3Array(VertexItemType type, int stream) const override
    {
        if (static_cast<int>(type) == 4) {
            GeometryChunk::ConstVector3Ptr r;
            r.stride = sizeof(Vector3);
            r.data = reinterpret_cast<const Vector3*>(GeometryChunkDetail::BlockData(normals));
            return r;
        }
        return GeometryChunkTemplate<Vertex, Base>::GetVector3Array(type, stream);
    }

    GeometryChunkBlock* normals;  // +0x60

protected:
    // 0x5f420: the shared helper the Nml CreateInstance calls: Base::InitInstance, then both blocks are shared
    void __cdecl InitInstance(GeometryChunkNmlTemplate& to) const
    {
        GeometryChunkTemplate<Vertex, Base>::InitInstance(to);
        GeometryChunkDetail::ShareBlock(to.normals, normals);
    }
};

// ---- the TS12 instantiations ------------------------------------------------------------------------------------
//
// Every class below exists in the original with GeometryChunkSingleStream as the base; the multipass bump chunks
// derive from the PackedVertexPNTA and PackedVertexPNTTA ones (their 58 exports stay forwarded). The aliases are a
// compile-time catalogue: the differential test instantiates all of them and asks the original by RVA.

#define JET_GEOMETRY_CHUNK_ALIAS(Vertex) \
    using PackedVertex##Vertex##GeometryChunk = GeometryChunkTemplate<PackedVertex##Vertex, GeometryChunkSingleStream>;

// 40 plain template instantiations
JET_GEOMETRY_CHUNK_ALIAS(PC)
JET_GEOMETRY_CHUNK_ALIAS(PCNT)
JET_GEOMETRY_CHUNK_ALIAS(PCT)
JET_GEOMETRY_CHUNK_ALIAS(PCTT)
JET_GEOMETRY_CHUNK_ALIAS(PN)
JET_GEOMETRY_CHUNK_ALIAS(PNT)
JET_GEOMETRY_CHUNK_ALIAS(PNTA)
JET_GEOMETRY_CHUNK_ALIAS(PNTT)
JET_GEOMETRY_CHUNK_ALIAS(PNTTA)
JET_GEOMETRY_CHUNK_ALIAS(PW1IC)
JET_GEOMETRY_CHUNK_ALIAS(PW1ICT)
JET_GEOMETRY_CHUNK_ALIAS(PW1ICTT)
JET_GEOMETRY_CHUNK_ALIAS(PW1IN)
JET_GEOMETRY_CHUNK_ALIAS(PW1INT)
JET_GEOMETRY_CHUNK_ALIAS(PW1INTT)
JET_GEOMETRY_CHUNK_ALIAS(PW2IC)
JET_GEOMETRY_CHUNK_ALIAS(PW2ICT)
JET_GEOMETRY_CHUNK_ALIAS(PW2ICTT)
JET_GEOMETRY_CHUNK_ALIAS(PW2IN)
JET_GEOMETRY_CHUNK_ALIAS(PW2INT)
JET_GEOMETRY_CHUNK_ALIAS(PW2INTT)
JET_GEOMETRY_CHUNK_ALIAS(PW3IC)
JET_GEOMETRY_CHUNK_ALIAS(PW3ICT)
JET_GEOMETRY_CHUNK_ALIAS(PW3ICTT)
JET_GEOMETRY_CHUNK_ALIAS(PW3IN)
JET_GEOMETRY_CHUNK_ALIAS(PW3INT)
JET_GEOMETRY_CHUNK_ALIAS(PW3INTT)
JET_GEOMETRY_CHUNK_ALIAS(PW4IC)
JET_GEOMETRY_CHUNK_ALIAS(PW4ICT)
JET_GEOMETRY_CHUNK_ALIAS(PW4ICTT)
JET_GEOMETRY_CHUNK_ALIAS(PW4IN)
JET_GEOMETRY_CHUNK_ALIAS(PW4INT)
JET_GEOMETRY_CHUNK_ALIAS(PW4INTT)
JET_GEOMETRY_CHUNK_ALIAS(PWb4IC)
JET_GEOMETRY_CHUNK_ALIAS(PWb4ICNT)
JET_GEOMETRY_CHUNK_ALIAS(PWb4ICT)
JET_GEOMETRY_CHUNK_ALIAS(PWb4ICTT)
JET_GEOMETRY_CHUNK_ALIAS(PWb4IN)
JET_GEOMETRY_CHUNK_ALIAS(PWb4INT)
JET_GEOMETRY_CHUNK_ALIAS(PWb4INTT)
#undef JET_GEOMETRY_CHUNK_ALIAS

// 18 normal-template instantiations
#define JET_GEOMETRY_NML_ALIAS(Vertex) \
    using PackedVertex##Vertex##GeometryNmlChunk = GeometryChunkNmlTemplate<PackedVertex##Vertex, GeometryChunkSingleStream>;
JET_GEOMETRY_NML_ALIAS(PC)
JET_GEOMETRY_NML_ALIAS(PCT)
JET_GEOMETRY_NML_ALIAS(PCTT)
JET_GEOMETRY_NML_ALIAS(PW1IC)
JET_GEOMETRY_NML_ALIAS(PW1ICT)
JET_GEOMETRY_NML_ALIAS(PW1ICTT)
JET_GEOMETRY_NML_ALIAS(PW2IC)
JET_GEOMETRY_NML_ALIAS(PW2ICT)
JET_GEOMETRY_NML_ALIAS(PW2ICTT)
JET_GEOMETRY_NML_ALIAS(PW3IC)
JET_GEOMETRY_NML_ALIAS(PW3ICT)
JET_GEOMETRY_NML_ALIAS(PW3ICTT)
JET_GEOMETRY_NML_ALIAS(PW4IC)
JET_GEOMETRY_NML_ALIAS(PW4ICT)
JET_GEOMETRY_NML_ALIAS(PW4ICTT)
JET_GEOMETRY_NML_ALIAS(PWb4IC)
JET_GEOMETRY_NML_ALIAS(PWb4ICT)
JET_GEOMETRY_NML_ALIAS(PWb4ICTT)
#undef JET_GEOMETRY_NML_ALIAS

// one stream of records over GeometryChunkSingleStream: the block pointer is the only field the template adds
JET_LAYOUT32(sizeof(PackedVertexPNTAGeometryChunk) == 0x60);
JET_LAYOUT32(sizeof(PackedVertexPCGeometryNmlChunk) == 0x64);

}  // namespace Jet
