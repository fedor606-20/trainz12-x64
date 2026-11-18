// Unit U7 of RenderSystem_7c: GeometryChunkTemplate<PackedVertexPNT, GeometryChunkSingleStream> (the chunk
// NullMaterial::CreateGeometryChunk hands out; 0x10037af0 constructs it, its vtable is 0x10071834 and its
// functions 0x10037bb0..0x100383a3) and NullMaterial::CreateGeometryChunk itself (0x10037a30). No exports: only
// the vtable's slots 1-5, 7-9, 12/13, 39, 42-44 and 46..55 are its own (the rest come from GeometryChunk and
// GeometryChunkSingleStream, compared slot by slot against the original's two vftables). Behaviour follows the
// TS12 assembly (addresses per function), including its quirks: the vertex block writes only its reference count
// and count (the item size and capacity keep whatever the heap held), DuplicateVertexArray copies maxVertices
// items whatever the old block held, and GetVertexArray on a chunk without a block answers 0x10.
#include <jet/Geometry.h>
#include <jet/RenderSystem.h>
#include <jet/VertexFormats.h>
#include <jet/VCall.h>

#include <cstring>
#include <new>

namespace Jet {

// MaterialFields.cpp: the geometry flags of a MakeGeometryChunkFlags call (the same table CreateGeometryChunk
// reads at 0x1006cd84)
unsigned __cdecl RsMatGeometryFlags(int type, const void* hint);

namespace {

// the vertex block as the template makes it (0x10037b6b, 0x10037faa, 0x1003810d): {references, count, item
// size, capacity, items}, but only the first two fields are written
struct VertexBlock {
    unsigned int refs;
    unsigned int count;
    unsigned int itemSize;
    unsigned int capacity;
    unsigned char items[1];
};

VertexBlock* MakeBlock(unsigned int items, unsigned int itemSize)
{
    auto* block = static_cast<VertexBlock*>(Alloc::New(items * itemSize + 0x10));
    block->count = 0;
    block->refs = 1;
    block->count += items;
    return block;
}

// the release the template inlines (0x10037c62, 0x10037ff9, 0x10038157, 0x100382d7): the block goes only with
// its last reference, and the slot is cleared only then
void ReleaseBlock(VertexBlock*& slot)
{
    if (!slot) return;
    if (--slot->refs == 0) {
        Alloc::Delete(slot);
        slot = nullptr;
    }
}

// the display's vertex buffer manager (+0x20c) and its two upload slots (0x20 vertices, 0x2c indices)
const unsigned char* Raw(const Display* d) { return reinterpret_cast<const unsigned char*>(d); }
void* BufferManager(const Display* d) { return *reinterpret_cast<void* const*>(Raw(d) + 0x20c); }

// an AutoReferenceArray's items, as the original computes them (0x10 past a null block is 0x10)
const unsigned short* IndexData(const void* block)
{
    return reinterpret_cast<const unsigned short*>(reinterpret_cast<std::uintptr_t>(block) + 0x10);
}

// stream 0's stride: the streams block at +0x10 starts with the first stream's stride
unsigned int Stride(const VertexType* type)
{
    return **reinterpret_cast<const unsigned int* const*>(reinterpret_cast<const unsigned char*>(type) + 0x10);
}

}  // namespace

// The chunk of the null material: one stream of PackedVertexPNT records behind a hand-made vertex block.
template <class Vertex, class Base>
class GeometryChunkTemplate : public Base {
public:
    GeometryChunkTemplate() : block(nullptr)
    {
        // (0x10038340: the base default construction, the template's vtable and no block)
    }
    GeometryChunkTemplate(unsigned int vertices, unsigned int primitives, unsigned int indices,
                          unsigned int faceNormals, unsigned int flags)
        : Base(vertices, primitives, indices, faceNormals, flags)
    {
        block = MakeBlock(vertices, sizeof(Vertex));  // 0x10037af0
    }
    ~GeometryChunkTemplate() override { ReleaseBlock(block); }  // 0x100382a0 (before the base destructor)

    GeometryChunk* __cdecl CreateInstance() const override  // 0x10037c30 (slot 3)
    {
        void* const memory = Alloc::New(0x60);
        if (!memory) return nullptr;
        auto* made = ::new (memory) GeometryChunkTemplate;
        this->InitInstance(*made);
        if (made->block != block) {
            ReleaseBlock(made->block);
            made->block = block;
            if (block) ++block->refs;
        }
        return made;
    }
    GeometryChunk* __cdecl CreateInstance(unsigned int vertices, unsigned int primitives, unsigned int indices,
                                          unsigned int faceNormals) const override  // 0x10037bb0 (slot 2)
    {
        void* const memory = Alloc::New(0x60);
        if (!memory) return nullptr;
        return ::new (memory) GeometryChunkTemplate(vertices, primitives, indices, faceNormals, this->flags);
    }

    bool __cdecl InitializeBuffers(Display* display, bool upload) override  // 0x10037ca0 (slot 4)
    {
        const unsigned long f = this->flags;
        const StaticBuffer* const had = this->GetStaticVertexBuffer(0, 0);
        const VertexType* const type = this->AssignVertexType(display);
        if (!(f & 0x10000) && !had) {
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
            const unsigned int count = this->indexArray.Count();
            auto* buffer = static_cast<StaticBuffer*>(VCall<void*>(display, 0x108, count, 0));
            if (!buffer) return false;
            if (upload) {
                unsigned int out = 0;
                const void* data = IndexData(this->indexArray.block);
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

    bool __cdecl LoadBuffers(Display* display) override  // 0x10037e80 (slot 5)
    {
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
            const void* data = IndexData(this->indexArray.block);
            if (VCall<int>(manager, 0x2c, buffer, data, this->numIndices, 1, &out) == -1) {
                buffer->ReleaseReference();
                return false;
            }
            this->SetIndexDataDirty(false);
        }
        return true;
    }

    void __cdecl DuplicateVertexArray(Display* display, int, int) override  // 0x10037f70 (slot 7)
    {
        if (!block) return;
        auto* copy = MakeBlock(this->maxVertices, sizeof(Vertex));
        std::memcpy(copy->items, block->items, this->maxVertices * sizeof(Vertex));
        if (block != copy) {
            ReleaseBlock(block);
            block = copy;
            ++copy->refs;
        }
        ReleaseBlock(copy);
        if (this->GetStaticVertexBuffer(0, 0)) {
            this->SetStaticVertexBuffer(nullptr, 0, 0);
            this->InitializeBuffers(display, false);
        }
    }

    void __cdecl DuplicateIndexArray(Display* display) override  // 0x10038080 (slot 8)
    {
        GeometryChunk::DuplicateIndexArray(display);
        if (this->GetStaticIndexBuffer(0)) {
            this->SetStaticIndexBuffer(nullptr, 0);
            this->InitializeBuffers(display, false);
        }
    }

    void __cdecl ResizeVertexArray(Display* display, unsigned int n, int, int) override  // 0x100380d0 (slot 9)
    {
        const unsigned int keep = this->maxVertices < n ? this->maxVertices : n;
        auto* copy = MakeBlock(n, sizeof(Vertex));
        if (keep) std::memcpy(copy->items, block->items, keep * sizeof(Vertex));
        if (block != copy) {
            ReleaseBlock(block);
            block = copy;
            ++copy->refs;
        }
        this->maxVertices = n;
        this->numVertices = n;
        if (block && this->GetStaticVertexBuffer(0, 0)) {
            this->SetStaticVertexBuffer(nullptr, 0, 0);
            this->InitializeBuffers(display, false);
        }
        ReleaseBlock(copy);
    }

    void* __cdecl GetVertexArray(int, int) override  // 0x100381f0 (slots 12/13): 0x10 with no block
    {
        return reinterpret_cast<unsigned char*>(block) + 0x10;
    }
    const void* __cdecl GetVertexArray(int, int) const override
    {
        return reinterpret_cast<const unsigned char*>(block) + 0x10;
    }

    const VertexType* __cdecl AssignVertexType(Display* display) const override  // 0x10038210 (slot 39)
    {
        if (!this->vertexType)
            this->SetVertexType(static_cast<VertexType*>(VCall<void*>(display, 0x10c, 18)), 0);
        return &this->GetVertexType(0);
    }

    VertexTypeID __cdecl GetVertexTypeID(int) const override  // 0x10038200 (slot 42)
    {
        return static_cast<VertexTypeID>(18);  // the PackedVertexPNT record
    }
    int __cdecl GetVertexSize(int) const override  // 0x1003e5f0 (slot 43)
    {
        return sizeof(Vertex);
    }
    int __cdecl GetItemOffset(VertexItemType type, int) const override  // 0x10038250 (slot 44)
    {
        return Vertex::vertexItemOffset[type].offset;
    }

    // 0x10038260 (slots 46..55): one body for every item view; the result goes through the hidden pointer a
    // member function returning this class gets (the stride of a missing item is 0)
    template <class P>
    P ItemView(VertexItemType type) const
    {
        P r;
        const int offset = Vertex::vertexItemOffset[type].offset;
        if (offset < 0) {
            r.stride = 0;
            r.data = nullptr;
            return r;
        }
        r.stride = sizeof(Vertex);
        r.data = reinterpret_cast<decltype(r.data)>(reinterpret_cast<std::uintptr_t>(block) + 0x10 + offset);
        return r;
    }

#define JET_TEMPLATE_ITEM(Getter, Ptr, ConstPtr)                                          \
    GeometryChunk::Ptr __cdecl Getter(VertexItemType type, int) override                  \
    {                                                                                     \
        return ItemView<GeometryChunk::Ptr>(type);                                        \
    }                                                                                     \
    GeometryChunk::ConstPtr __cdecl Getter(VertexItemType type, int) const override       \
    {                                                                                     \
        return ItemView<GeometryChunk::ConstPtr>(type);                                   \
    }
    JET_TEMPLATE_ITEM(GetVector2Array, Vector2Ptr, ConstVector2Ptr)
    JET_TEMPLATE_ITEM(GetVector3Array, Vector3Ptr, ConstVector3Ptr)
    JET_TEMPLATE_ITEM(GetVector4Array, Vector4Ptr, ConstVector4Ptr)
    JET_TEMPLATE_ITEM(GetFloatArray, FloatPtr, ConstFloatPtr)
    JET_TEMPLATE_ITEM(GetUDwordArray, UDwordPtr, ConstUDwordPtr)
#undef JET_TEMPLATE_ITEM

    // (public for the layout check and CreateGeometryChunk)
    VertexBlock* block;  // +0x5c
};

using PackedVertexChunk = GeometryChunkTemplate<PackedVertexPNT, GeometryChunkSingleStream>;
JET_LAYOUT32(sizeof(PackedVertexChunk) == 0x60);
JET_LAYOUT32(offsetof(PackedVertexChunk, block) == 0x5c);

// 0x10037a30: the chunk of the null material. `this` is unused (a const method that only builds a chunk).
GeometryChunk* __cdecl RsNullMaterialCreateGeometryChunk(void*, unsigned int vertices, unsigned int primitives,
                                                        unsigned int indices, unsigned int faceNormals,
                                                        int type, const GeometryChunkCreateHint* hint)
{
    const unsigned int flags = RsMatGeometryFlags(type, hint);
    void* const memory = Alloc::New(0x60);
    if (!memory) return nullptr;  // the original goes on and calls through null; ours answers null
    auto* chunk = ::new (memory) PackedVertexChunk(vertices, primitives, indices, faceNormals, flags);
    if (hint && hint->version == 1) chunk->SetVertexDataDynamic(0, -2, true);
    return chunk;
}

}  // namespace Jet
