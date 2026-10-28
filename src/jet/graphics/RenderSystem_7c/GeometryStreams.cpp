// Unit U5c of RenderSystem_7c: the stream chunks GeometryChunkSingleStream / GeometryChunkMultiStream
// (0x100222c0..0x10022a80, 0x10023b90..0x10024160, 0x1002b2e0..0x1002b9c4), the in-memory chunks
// GenericGeometryChunkSingleStream / GenericGeometryChunkMultiStream (0x10022e40..0x10023880, 0x10024190..
// 0x10024a90) and GenericReferenceArray (0x10022ab0..0x10022d9e). Behaviour follows the TS12 assembly (addresses
// per function), including its quirks: the multi stream render calls ignore the primitive and count they are given,
// GenericGeometryChunkMultiStream's constructor passes its flags as the stream count and 4 as the flags, the multi
// stream InitInstance copies the bones a second time. GeometryChunk::primitiveLookup stays the original's.
#include <jet/Geometry.h>
#include <jet/VCall.h>

#include <malloc.h>

#include <cstdint>
#include <cstring>

namespace Jet {

void AppendMatrixMap(Array<VertexProgram::MatrixMap*, 128>& a, VertexProgram::MatrixMap* m);  // GeometryChunk.cpp

namespace {

const ReferenceCount* Ref(const VertexProgram* p) { return p; }

// an AutoReferenceArray's items, as the original computes them (0x10 past a null block is 0x10)
const unsigned short* IndexData(const void* block)
{
    return reinterpret_cast<const unsigned short*>(reinterpret_cast<std::uintptr_t>(block) + 0x10);
}

// the vertex program's API handle (with the chunk's bones bound when asked), or without a program the chunk's
// vertex type (slot 41)
const void* Handle(const GeometryChunk* c, const VertexProgram* p, bool bones, int stream, const void* influences)
{
    if (!p) return VCall<const void*>(c, 0xa4, stream);
    const void* const h = VCall<const void*>(p, 0x74);
    if (bones) {
        VCall<void>(p, 0x3c, 1);
        VCall<void>(p, 0x48, influences);
    }
    return h;
}

// the display's vertex buffer manager (+0x20c); before a multi stream draw the original lets it catch up (slot
// 0x10) unless the display's +0x213 says it need not
const unsigned char* Raw(const Display* d) { return reinterpret_cast<const unsigned char*>(d); }
void* BufferManager(const Display* d) { return *reinterpret_cast<void* const*>(Raw(d) + 0x20c); }
void SyncBuffers(const Display* d)
{
    if (!Raw(d)[0x213]) VCall<void>(BufferManager(d), 0x40);
}

// a stream as the display's draw calls take it (12 bytes; only the low byte of the last word is written)
struct StreamEntry {
    const void* data;
    const StaticBuffer* buffer;
    unsigned char dirty;
};

// the bits the original passes for a "dirty" bool: the bit shifted down with everything above it
unsigned long DirtyBits(unsigned long f, int bit) { return f >> bit & 0xffffff01; }

// a multiplication with the original's overflow check (operator new[] style: all bits set on overflow)
unsigned int Times(unsigned int n, unsigned int size)
{
    const unsigned long long b = static_cast<unsigned long long>(n) * size;
    return b >> 32 ? 0xffffffffu : static_cast<unsigned int>(b);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// GenericReferenceArray

GenericReferenceArray::GenericReferenceArray() : block(nullptr) {}  // 0x10042f60

// 0x10022ab0
GenericReferenceArray::GenericReferenceArray(unsigned int capacity, unsigned int itemSize) : block(nullptr)
{
    Create(capacity, itemSize);
}

// 0x10056050
GenericReferenceArray::GenericReferenceArray(const GenericReferenceArray& o) : block(nullptr)
{
    if (o.block) {
        block = o.block;
        ++block->refs;
    }
}

GenericReferenceArray::~GenericReferenceArray() { ReleaseReference(); }  // 0x10056170

// 0x10022c20
void __cdecl GenericReferenceArray::operator=(const GenericReferenceArray& o)
{
    if (block == o.block) return;
    ReleaseReference();
    block = o.block;
    if (block) ++block->refs;
}

// 0x10022ae0
void __cdecl GenericReferenceArray::Create(unsigned int capacity, unsigned int itemSize)
{
    ReleaseReference();
    block = static_cast<Block*>(Alloc::New(capacity * itemSize + 0x10));
    block->count = 0;
    block->itemSize = itemSize;
    block->refs = 1;
    block->capacity = capacity;
}

// 0x10022b50
void __cdecl GenericReferenceArray::AddReference() const
{
    if (block) ++block->refs;
}

// 0x10022b60
bool __cdecl GenericReferenceArray::ReleaseReference() const
{
    if (!block) return false;
    if (--block->refs) return false;
    Alloc::Delete(block);
    block = nullptr;
    return true;
}

int __cdecl GenericReferenceArray::GetReferenceCount() const { return block->refs; }  // 0x10022ba0
void __cdecl GenericReferenceArray::Add() { ++block->count; }                        // 0x10022bb0
void __cdecl GenericReferenceArray::Allocate(unsigned int n) { block->count += n; }  // 0x10022bc0

// 0x10022bd0
void __cdecl GenericReferenceArray::Remove()
{
    if (block->count) --block->count;
}

void __cdecl GenericReferenceArray::RemoveAll() { block->count = 0; }                     // 0x10022bf0
unsigned int __cdecl GenericReferenceArray::Count() const { return block ? block->count : 0; }  // 0x10022c00

// 0x10022c60
void __cdecl GenericReferenceArray::Duplicate(GenericReferenceArray& to) const
{
    if (!block) return;
    to.Create(block->capacity, block->itemSize);
    to.block->count += block->count;
    std::memcpy(to.block->data, block->data, block->itemSize * block->count);
}

// 0x10022cb0
void __cdecl GenericReferenceArray::Duplicate()
{
    if (!block) return;
    GenericReferenceArray copy;
    copy.Create(block->capacity, block->itemSize);
    copy.block->count += block->count;
    std::memcpy(copy.block->data, block->data, block->itemSize * block->count);
    *this = copy;
}

// 0x10022d40
void* __cdecl GenericReferenceArray::operator[](unsigned int i) { return block->data + block->itemSize * i; }
const void* __cdecl GenericReferenceArray::operator[](unsigned int i) const
{
    return block->data + block->itemSize * i;
}

__cdecl GenericReferenceArray::operator bool() const { return block != nullptr; }  // 0x10022d60

// 0x10022d70
__cdecl GenericReferenceArray::operator void*() const { return block && block->count ? block->data : nullptr; }

// 0x10022d90
unsigned char* __cdecl GenericReferenceArray::GetByteOffset(unsigned int offset) { return block->data + offset; }
const unsigned char* __cdecl GenericReferenceArray::GetByteOffset(unsigned int offset) const
{
    return block->data + offset;
}

// ---------------------------------------------------------------------------------------------------------------
// GeometryChunkSingleStream

// 0x1002b800
GeometryChunkSingleStream::GeometryChunkSingleStream()
    : vertexBuffer(nullptr), indexBuffer(nullptr), vertexType(nullptr), program(nullptr)
{
}

// 0x1002b870
GeometryChunkSingleStream::GeometryChunkSingleStream(unsigned int vertices, unsigned int primitives,
                                                     unsigned int indices, unsigned int faceNormals,
                                                     unsigned int f)
    : GeometryChunk(vertices, primitives, indices, faceNormals, f), vertexBuffer(nullptr), indexBuffer(nullptr),
      vertexType(nullptr), program(nullptr)
{
}

// 0x1002b8b0
GeometryChunkSingleStream::~GeometryChunkSingleStream()
{
    if (vertexType) vertexType->ReleaseReference();
    if (vertexBuffer) {
        vertexBuffer->ReleaseReference();
        vertexBuffer = nullptr;
    }
    if (indexBuffer) {
        indexBuffer->ReleaseReference();
        indexBuffer = nullptr;
    }
    if (program) Ref(program)->ReleaseReference();
}

// 0x1002b950: the buffers, the vertex type and the program shared (the target's own are not let go, but its
// program)
void __cdecl GeometryChunkSingleStream::InitInstance(GeometryChunkSingleStream& to) const
{
    GeometryChunk::InitInstance(to);
    to.vertexBuffer = vertexBuffer;
    to.indexBuffer = indexBuffer;
    to.vertexType = vertexType;
    if (vertexType) vertexType->AddReference();
    if (to.vertexBuffer) to.vertexBuffer->AddReference();
    if (to.indexBuffer) to.indexBuffer->AddReference();
    VertexProgram* const old = to.program;
    to.program = program;
    if (program) Ref(program)->AddReference();
    if (old) Ref(old)->ReleaseReference();
}

// 0x10023cf0: dynamic vertices with dynamic indices go through display slot 0xf0, anything else through the
// static buffers (slot 0xec), after which both are clean
void __cdecl GeometryChunkSingleStream::RenderGeometry(Display* display, ViewPort*, const VertexProgram* p,
                                                       bool bones, int stream) const
{
    const unsigned long f = flags;
    const void* const h = Handle(this, p, bones, stream, &influences);
    if (f & 0x10000 && f & 0x100) {
        const void* const v = VCall<const void*>(this, 0x30, 0, 0);
        VCall<void>(display, 0xf0, h, primitiveLookup[f & 0x1f], numPrimitives, v, numVertices,
                    IndexData(indexArray.block), numIndices);
    } else {
        const StaticBuffer* const vb = VCall<const StaticBuffer*>(this, 0x8c, 0, 0);
        const StaticBuffer* const ib = VCall<const StaticBuffer*>(this, 0x94, 0);
        const void* const v = VCall<const void*>(this, 0x30, 0, 0);
        VCall<void>(display, 0xec, h, primitiveLookup[f & 0x1f], numPrimitives, v, maxVertices,
                    IndexData(indexArray.block), numIndices, vb, DirtyBits(f, 17), ib, DirtyBits(f, 9));
        VCall<void>(this, 0x84, 0);
        VCall<void>(this, 0x7c, 0, 0, -2);
    }
    if (p && bones) VCall<void>(p, 0x18);
}

// 0x10023e80
void __cdecl GeometryChunkSingleStream::RenderPrimitives(Display* display, ViewPort*, const VertexProgram* p,
                                                         unsigned int count, const unsigned short* indices,
                                                         unsigned int prims, DPrimitive primitive, bool bones,
                                                         int stream) const
{
    const unsigned long f = flags;
    const void* const h = Handle(this, p, bones, stream, &influences);
    if (!(f & 0x10000)) {
        const StaticBuffer* const vb = VCall<const StaticBuffer*>(this, 0x8c, 0, 0);
        const void* const v = VCall<const void*>(this, 0x30, 0, 0);
        VCall<void>(display, 0xec, h, primitive, prims, v, maxVertices, indices, count, vb, DirtyBits(f, 17), 0, 0);
        VCall<void>(this, 0x7c, 0, 0, -2);
    } else {
        const void* const v = VCall<const void*>(this, 0x30, 0, 0);
        VCall<void>(display, 0xf0, h, primitive, prims, v, numVertices, indices, count);
    }
    if (p && bones) VCall<void>(p, 0x18);
}

// 0x10023fc0 (also slot 17, 0x100240f0): the program stays bound for the passes (CleanupMultipassRender lets go)
bool __cdecl GeometryChunkSingleStream::SetupMultipassRenderInternal(Display* display, ViewPort*,
                                                                     const VertexProgram* p,
                                                                     const unsigned short* indices,
                                                                     unsigned int count) const
{
    const void* const h = Handle(this, p, true, 0, &influences);
    const unsigned long f = flags;
    StreamEntry s;
    s.data = VCall<const void*>(this, 0x30, 0, 0);
    s.buffer = VCall<const StaticBuffer*>(this, 0x8c, 0, 0);
    s.dirty = static_cast<unsigned char>(f >> 17 & 1);
    const StaticBuffer* ib = nullptr;
    if (!indices) {
        indices = IndexData(indexArray.block);
        count = numIndices;
        ib = indexBuffer;
    }
    if (!VCall<bool>(display, 0xfc, h, &s, maxVertices, ib, indices, count, DirtyBits(f, 9))) return false;
    if (!(f & 0x10000)) VCall<void>(this, 0x7c, 0, 0, -2);
    if (!(f & 0x100)) VCall<void>(this, 0x84, 0);
    return true;
}

bool __cdecl GeometryChunkSingleStream::SetupMultipassRender(Display* display, ViewPort* view,
                                                             const VertexProgram* p, const unsigned short* indices,
                                                             unsigned int count) const
{
    return SetupMultipassRenderInternal(display, view, p, indices, count);
}

// 0x10024100: with the chunk's own program (slot 60)
bool __cdecl GeometryChunkSingleStream::SetupMultipassRender(Display* display, ViewPort* view,
                                                             const unsigned short* indices,
                                                             unsigned int count) const
{
    const VertexProgram* const p = VCall<const VertexProgram*>(this, 0xf0, 0);
    return SetupMultipassRenderInternal(display, view, p, indices, count);
}

// 0x10022a30
void __cdecl GeometryChunkSingleStream::BindVertexProgram(const VertexProgram* old, const VertexProgram* p,
                                                          bool bones) const
{
    if (old) VCall<void>(old, 0x18);
    if (p) {
        VCall<void>(p, 0x3c, 1);
        if (bones) VCall<void>(p, 0x48, static_cast<const void*>(&influences));
    }
}

void __cdecl GeometryChunkSingleStream::BindVertexProgram(int, bool) const {}  // 0x10021870

// 0x10022a80
void __cdecl GeometryChunkSingleStream::RenderGeometryPass(Display* display, ViewPort*, int) const
{
    VCall<void>(display, 0xf8, primitiveLookup[flags & 0x1f], numPrimitives);
}

// 0x10024140
void __cdecl GeometryChunkSingleStream::RenderGeometryPass(Display* display, ViewPort*, int count,
                                                           DPrimitive primitive, int) const
{
    VCall<void>(display, 0xf8, primitive, count);
}

// 0x10024160
void __cdecl GeometryChunkSingleStream::CleanupMultipassRender(Display* display, const VertexProgram* p) const
{
    if (p) VCall<void>(p, 0x18);
    VCall<void>(display, 0x100);
}

int __cdecl GeometryChunkSingleStream::GetNumPasses() const { return 1; }   // 0x10023b90
int __cdecl GeometryChunkSingleStream::GetNumStreams() const { return 1; }  // 0x10023b90

// 0x10023c40: on sets the dynamic bit; off sets it too, with the unshared bits
void __cdecl GeometryChunkSingleStream::SetVertexDataDynamic(int, int, bool on)
{
    flags |= on ? 0x10000 : 0x51000;
}

bool __cdecl GeometryChunkSingleStream::GetVertexDataDynamic(int, int) const { return (flags >> 16 & 1) != 0; }

// 0x10023c80
void __cdecl GeometryChunkSingleStream::SetVertexDataShared(int, int stream, bool on)
{
    if (on)
        flags &= ~0x40000ul;
    else
        flags |= (stream != -3 ? 0x40000 : 0) | 0x1000;
}

bool __cdecl GeometryChunkSingleStream::GetVertexDataShared(int, int) const { return !(flags >> 18 & 1); }

// 0x10023cc0
void __cdecl GeometryChunkSingleStream::SetVertexDataDirty(bool on, int, int) const
{
    if (on)
        flags |= 0x20000;
    else
        flags &= ~0x20000ul;
}

bool __cdecl GeometryChunkSingleStream::GetVertexDataDirty(int, int) const { return (flags >> 17 & 1) != 0; }

const StaticBuffer* __cdecl GeometryChunkSingleStream::GetStaticVertexBuffer(int, int) const  // 0x10003530
{
    return vertexBuffer;
}

// 0x10023be0
void __cdecl GeometryChunkSingleStream::SetStaticVertexBuffer(const StaticBuffer* b, int, int) const
{
    if (b) b->AddReference();
    if (vertexBuffer) vertexBuffer->ReleaseReference();
    vertexBuffer = b;
}

const StaticBuffer* __cdecl GeometryChunkSingleStream::GetStaticIndexBuffer(int) const { return indexBuffer; }

// 0x10023c10
void __cdecl GeometryChunkSingleStream::SetStaticIndexBuffer(const StaticBuffer* b, int) const
{
    if (b) b->AddReference();
    if (indexBuffer) indexBuffer->ReleaseReference();
    indexBuffer = b;
}

// 0x10023bb0: the old type goes first (the same type with one reference is freed)
void __cdecl GeometryChunkSingleStream::SetVertexType(VertexType* type, int) const
{
    if (vertexType) vertexType->ReleaseReference();
    type->AddReference();
    vertexType = type;
}

const VertexType& __cdecl GeometryChunkSingleStream::GetVertexType(int) const { return *vertexType; }  // 0x10022f60
int __cdecl GeometryChunkSingleStream::GetItemStream(VertexItemType, int) const { return 0; }        // 0x1001dec0

// 0x10022510
void __cdecl GeometryChunkSingleStream::SetVertexProgram(VertexProgram* p, int)
{
    if (p) Ref(p)->AddReference();
    if (program) Ref(program)->ReleaseReference();
    program = p;
}

const VertexProgram* __cdecl GeometryChunkSingleStream::GetVertexProgram(int) const { return program; }  // 0x10023ba0

// ---------------------------------------------------------------------------------------------------------------
// GeometryChunkMultiStream

namespace {

GeometryChunkMultiStream::StreamState* MakeStreams(int n)
{
    auto* const s = static_cast<GeometryChunkMultiStream::StreamState*>(Alloc::New(Times(n, 8)));
    if (!s) return nullptr;
    for (int k = 0; k < n; ++k) s[k] = {nullptr, 0};
    return s;
}

}  // namespace

// 0x1002b340
GeometryChunkMultiStream::GeometryChunkMultiStream(int n)
    : streams(nullptr), indexBuffer(nullptr), vertexType(nullptr), program(nullptr), numStreams(n)
{
    streams = MakeStreams(n);
}

// 0x1002b430
GeometryChunkMultiStream::GeometryChunkMultiStream(unsigned int vertices, unsigned int primitives,
                                                   unsigned int indices, unsigned int faceNormals, int n,
                                                   unsigned int f)
    : GeometryChunk(vertices, primitives, indices, faceNormals, f), streams(nullptr), indexBuffer(nullptr),
      vertexType(nullptr), program(nullptr), numStreams(n)
{
    streams = MakeStreams(n);
}

// 0x1002b500
GeometryChunkMultiStream::~GeometryChunkMultiStream()
{
    if (vertexType) vertexType->ReleaseReference();
    for (int k = 0; k < numStreams; ++k) {
        if (streams[k].buffer) {
            streams[k].buffer->ReleaseReference();
            streams[k].buffer = nullptr;
        }
    }
    if (indexBuffer) {
        indexBuffer->ReleaseReference();
        indexBuffer = nullptr;
    }
    if (streams) Alloc::Delete(streams);
    streams = nullptr;
    if (program) Ref(program)->ReleaseReference();
}

// 0x1002b5d0
void __cdecl GeometryChunkMultiStream::InitInstance(GeometryChunkMultiStream& to) const
{
    GeometryChunk::InitInstance(to);
    for (int k = 0; k < numStreams; ++k) {
        to.streams[k].flags = streams[k].flags;
        to.streams[k].buffer = streams[k].buffer;
        if (streams[k].buffer) streams[k].buffer->AddReference();
    }
    to.indexBuffer = indexBuffer;
    if (indexBuffer) indexBuffer->AddReference();
    to.vertexType = vertexType;
    if (vertexType) vertexType->AddReference();
    VertexProgram* const old = to.program;
    to.program = program;
    if (program) Ref(program)->AddReference();
    if (old) Ref(old)->ReleaseReference();
    for (unsigned int k = 0; k < influences.count; ++k) {
        auto* const m = static_cast<VertexProgram::MatrixMap*>(Alloc::New(8));
        if (m) {
            m->matrix = nullptr;
            m->index = influences.data[k]->index;
        }
        AppendMatrixMap(to.influences, m);
    }
}

// the streams of a multi stream chunk as the display takes them: data (slot 12), static buffer (35) and dirty
// (32) of each, on the stack
#define JET_MULTI_STREAMS(c, entries)                                                                        \
    const int streamCount = VCall<int>(c, 0x60);                                                           \
    StreamEntry* const entries = static_cast<StreamEntry*>(_alloca((streamCount > 0 ? streamCount : 0) * 12)); \
    for (int s = 0; s < streamCount; ++s) {                                                                 \
        entries[s].data = VCall<const void*>(c, 0x30, 0, s);                                               \
        entries[s].buffer = VCall<const StaticBuffer*>(c, 0x8c, 0, s);                                     \
        entries[s].dirty = static_cast<unsigned char>(VCall<unsigned long>(c, 0x80, 0, s));                \
    }

// 0x10022550
void __cdecl GeometryChunkMultiStream::RenderGeometry(Display* display, ViewPort*, const VertexProgram* p,
                                                      bool bones, int stream) const
{
    const unsigned long f = flags;
    const void* const h = Handle(this, p, bones, stream, &influences);
    JET_MULTI_STREAMS(this, entries)
    SyncBuffers(display);
    const unsigned int count = numIndices;
    const unsigned int max = maxVertices;
    const unsigned short* const indices = IndexData(indexArray.block);
    const unsigned int prims = numPrimitives;
    const unsigned long dirty = VCall<unsigned long>(this, 0x88);
    const StaticBuffer* const ib = VCall<const StaticBuffer*>(this, 0x94, 0);
    VCall<void>(display, 0xf4, h, primitiveLookup[f & 0x1f], prims, entries, max, indices, ib, count, dirty);
    VCall<void>(this, 0x84, 0);
    VCall<void>(this, 0x7c, 0, 0, -2);
    if (p && bones) VCall<void>(p, 0x18);
}

// 0x100226f0: the chunk's own primitive and primitive count are drawn, not the ones given
void __cdecl GeometryChunkMultiStream::RenderPrimitives(Display* display, ViewPort*, const VertexProgram* p,
                                                        unsigned int count, const unsigned short* indices,
                                                        unsigned int, DPrimitive, bool bones, int stream) const
{
    const unsigned long f = flags;
    const void* const h = Handle(this, p, bones, stream, &influences);
    JET_MULTI_STREAMS(this, entries)
    SyncBuffers(display);
    VCall<void>(display, 0xf4, h, primitiveLookup[f & 0x1f], numPrimitives, entries, maxVertices, indices, 0,
                count, 0);
    VCall<void>(this, 0x7c, 0, 0, -2);
    if (p && bones) VCall<void>(p, 0x18);
}

// 0x10022850 (also slot 17, 0x100229e0)
bool __cdecl GeometryChunkMultiStream::SetupMultipassRenderInternal(Display* display, ViewPort*,
                                                                    const VertexProgram* p,
                                                                    const unsigned short* indices,
                                                                    unsigned int count) const
{
    const void* const h = Handle(this, p, true, 0, &influences);
    const unsigned long f = flags;
    JET_MULTI_STREAMS(this, entries)
    const StaticBuffer* ib = nullptr;
    if (!indices) {
        indices = IndexData(indexArray.block);
        count = numIndices;
        ib = indexBuffer;
    }
    SyncBuffers(display);
    if (!VCall<bool>(display, 0xfc, h, entries, maxVertices, ib, indices, count, DirtyBits(f, 9))) return false;
    VCall<void>(this, 0x7c, 0, 0, -2);
    VCall<void>(this, 0x84, 0);
    return true;
}

#undef JET_MULTI_STREAMS

bool __cdecl GeometryChunkMultiStream::SetupMultipassRender(Display* display, ViewPort* view,
                                                            const VertexProgram* p, const unsigned short* indices,
                                                            unsigned int count) const
{
    return SetupMultipassRenderInternal(display, view, p, indices, count);
}

// 0x100229f0
bool __cdecl GeometryChunkMultiStream::SetupMultipassRender(Display* display, ViewPort* view,
                                                            const unsigned short* indices,
                                                            unsigned int count) const
{
    const VertexProgram* const p = VCall<const VertexProgram*>(this, 0xf0, 0);
    return SetupMultipassRenderInternal(display, view, p, indices, count);
}

// 0x10022a30
void __cdecl GeometryChunkMultiStream::BindVertexProgram(const VertexProgram* old, const VertexProgram* p,
                                                         bool bones) const
{
    if (old) VCall<void>(old, 0x18);
    if (p) {
        VCall<void>(p, 0x3c, 1);
        if (bones) VCall<void>(p, 0x48, static_cast<const void*>(&influences));
    }
}

void __cdecl GeometryChunkMultiStream::BindVertexProgram(int, bool) const {}  // 0x10021870

// 0x10022a80
void __cdecl GeometryChunkMultiStream::RenderGeometryPass(Display* display, ViewPort*, int) const
{
    VCall<void>(display, 0xf8, primitiveLookup[flags & 0x1f], numPrimitives);
}

// 0x10024140
void __cdecl GeometryChunkMultiStream::RenderGeometryPass(Display* display, ViewPort*, int count,
                                                          DPrimitive primitive, int) const
{
    VCall<void>(display, 0xf8, primitive, count);
}

// 0x10024160
void __cdecl GeometryChunkMultiStream::CleanupMultipassRender(Display* display, const VertexProgram* p) const
{
    if (p) VCall<void>(p, 0x18);
    VCall<void>(display, 0x100);
}

int __cdecl GeometryChunkMultiStream::GetNumPasses() const { return 1; }             // 0x10023b90
int __cdecl GeometryChunkMultiStream::GetNumStreams() const { return numStreams; }  // 0x1003fd40

// 0x10022360: on sets the dynamic bit; off sets it too, with the unshared bit
void __cdecl GeometryChunkMultiStream::SetVertexDataDynamic(int, int stream, bool on)
{
    const unsigned long bits = on ? 1 : 5;
    if (stream == -2) {
        for (int k = 0; k < numStreams; ++k) streams[k].flags |= bits;
    } else {
        streams[stream].flags |= bits;
    }
    if (!on) flags |= 0x1000;
}

bool __cdecl GeometryChunkMultiStream::GetVertexDataDynamic(int, int stream) const
{
    return (streams[stream].flags & 1) != 0;
}

// 0x100223f0
void __cdecl GeometryChunkMultiStream::SetVertexDataShared(int, int stream, bool on)
{
    if (stream == -2) {
        for (int k = 0; k < numStreams; ++k) {
            if (on)
                streams[k].flags &= ~4ul;
            else
                streams[k].flags |= 4;
        }
    } else if (stream != -3) {
        if (on)
            streams[stream].flags &= ~4ul;
        else
            streams[stream].flags |= 4;
    }
    if (!on) flags |= 0x1000;
}

bool __cdecl GeometryChunkMultiStream::GetVertexDataShared(int, int stream) const
{
    return !(streams[stream].flags >> 2 & 1);
}

// 0x100224a0
void __cdecl GeometryChunkMultiStream::SetVertexDataDirty(bool on, int, int stream) const
{
    const int first = stream == -2 ? 0 : stream;
    const int end = stream == -2 ? numStreams : stream + 1;
    for (int k = first; k < end; ++k) {
        if (on)
            streams[k].flags |= 2;
        else
            streams[k].flags &= ~2ul;
    }
}

bool __cdecl GeometryChunkMultiStream::GetVertexDataDirty(int, int stream) const
{
    return (streams[stream].flags >> 1 & 1) != 0;
}

const StaticBuffer* __cdecl GeometryChunkMultiStream::GetStaticVertexBuffer(int, int stream) const  // 0x10022310
{
    return streams[stream].buffer;
}

// 0x10022320
void __cdecl GeometryChunkMultiStream::SetStaticVertexBuffer(const StaticBuffer* b, int, int stream) const
{
    if (b) b->AddReference();
    if (streams[stream].buffer) streams[stream].buffer->ReleaseReference();
    streams[stream].buffer = b;
}

const StaticBuffer* __cdecl GeometryChunkMultiStream::GetStaticIndexBuffer(int) const { return indexBuffer; }

// 0x10023c10
void __cdecl GeometryChunkMultiStream::SetStaticIndexBuffer(const StaticBuffer* b, int) const
{
    if (b) b->AddReference();
    if (indexBuffer) indexBuffer->ReleaseReference();
    indexBuffer = b;
}

// 0x10023bb0
void __cdecl GeometryChunkMultiStream::SetVertexType(VertexType* type, int) const
{
    if (vertexType) vertexType->ReleaseReference();
    type->AddReference();
    vertexType = type;
}

const VertexType& __cdecl GeometryChunkMultiStream::GetVertexType(int) const { return *vertexType; }  // 0x10022f60

// 0x10022510
void __cdecl GeometryChunkMultiStream::SetVertexProgram(VertexProgram* p, int)
{
    if (p) Ref(p)->AddReference();
    if (program) Ref(program)->ReleaseReference();
    program = p;
}

const VertexProgram* __cdecl GeometryChunkMultiStream::GetVertexProgram(int) const { return program; }  // 0x10023ba0

// 0x100222c0 / 0x100222d0
unsigned long __cdecl GeometryChunkMultiStream::GetStreamFlags(int stream) const { return streams[stream].flags; }
void __cdecl GeometryChunkMultiStream::SetStreamFlags(int stream, unsigned long f) { streams[stream].flags = f; }
void __cdecl GeometryChunkMultiStream::SetConstStreamFlags(int stream, unsigned long f) const
{
    streams[stream].flags = f;
}

// ---------------------------------------------------------------------------------------------------------------
// GenericGeometryChunkSingleStream

// 0x100242a0
GenericGeometryChunkSingleStream::GenericGeometryChunkSingleStream() : offsetTable(nullptr) {}

// 0x10024310
GenericGeometryChunkSingleStream::GenericGeometryChunkSingleStream(VertexType* type, unsigned int v,
                                                                   unsigned int primitives, unsigned int indices,
                                                                   unsigned int faceNormals, unsigned int f)
    : GeometryChunkSingleStream(v, primitives, indices, faceNormals, f), offsetTable(nullptr)
{
    vertexType = type;
    if (type) {
        type->AddReference();
        CreateVertexArray(type, v);
    }
}

// 0x100243b0
GenericGeometryChunkSingleStream::~GenericGeometryChunkSingleStream()
{
    if (offsetTable) offsetTable->ReleaseReference();
}

// 0x10024440: `n` vertices of the type's stride; the offset table only when there is none yet
void __cdecl GenericGeometryChunkSingleStream::CreateVertexArray(VertexType* type, int n)
{
    const unsigned int stride = type->GetStreamList()[0].stride;
    vertices.Create(n, stride);
    vertices.block->count += n;
    if (offsetTable) return;
    offsetTable = new ItemOffsetTable(stride);
    for (int t = 0; t < 39; ++t) offsetTable->offsets[t] = type->GetItemOffset(static_cast<VertexItemType>(t));
}

// 0x100244e0: the first min(n, max) vertices kept, the static buffer remade when there was one
void __cdecl GenericGeometryChunkSingleStream::ResizeVertexArray(Display* display, unsigned int n, int, int)
{
    const unsigned int keep = maxVertices > n ? n : maxVertices;
    const int size = VCall<int>(this, 0xac, 0);
    GenericReferenceArray grown;
    grown.Create(n, size);
    grown.block->count += n;
    const void* const src = vertices.block && vertices.block->count ? vertices.block->data : nullptr;
    void* const dst = grown.block->count ? grown.block->data : nullptr;
    std::memcpy(dst, src, size * keep);
    vertices = grown;
    maxVertices = n;
    numVertices = n;
    if (vertices.block && VCall<const StaticBuffer*>(this, 0x8c, 0, 0)) {
        VCall<void>(this, 0x90, nullptr, 0, 0);
        VCall<void>(this, 0x10, display, 0);
    }
}

// 0x10024630
GeometryChunk* __cdecl GenericGeometryChunkSingleStream::CreateInstance() const
{
    auto* const c = new GenericGeometryChunkSingleStream();
    InitInstance(*c);
    return c;
}

// 0x10024670: the vertex type and offset table shared, new vertices
GeometryChunk* __cdecl GenericGeometryChunkSingleStream::CreateInstance(unsigned int v, unsigned int primitives,
                                                                         unsigned int indices,
                                                                         unsigned int faceNormals) const
{
    auto* const c = new GenericGeometryChunkSingleStream(nullptr, v, primitives, indices, faceNormals, flags);
    if (offsetTable) {
        offsetTable->AddReference();
        c->offsetTable = offsetTable;
    }
    if (vertexType) {
        c->vertexType = vertexType;
        vertexType->AddReference();
        c->CreateVertexArray(vertexType, v);
    }
    return c;
}

// 0x10024730
void __cdecl GenericGeometryChunkSingleStream::InitInstance(GenericGeometryChunkSingleStream& to) const
{
    GeometryChunkSingleStream::InitInstance(to);
    to.vertices = vertices;
    to.offsetTable = offsetTable;
    if (offsetTable) offsetTable->AddReference();
}

// 0x100247a0: a static vertex buffer (display slot 0x104) unless dynamic or made, filled through the display's
// buffer manager when `upload` (else left dirty); the same for the indices (slot 0x108)
bool __cdecl GenericGeometryChunkSingleStream::InitializeBuffers(Display* display, bool upload)
{
    const unsigned long f = flags;
    const StaticBuffer* const vb = VCall<const StaticBuffer*>(this, 0x8c, 0, 0);
    if (!(f & 0x10000) && !vb) {
        if (!vertexType) return false;
        StaticBuffer* const b =
            VCall<StaticBuffer*>(display, 0x104, maxVertices, vertexType->GetStreamList()[0].stride);
        if (!b) return false;
        if (upload) {
            void* const manager = BufferManager(display);
            unsigned long out;
            void* const v = VCall<void*>(this, 0x34, 0, 0);
            if (!VCall<bool>(manager, 0x20, vertexType, b, v, numVertices, 1, &out, 0)) {
                b->ReleaseReference();
                return false;
            }
            VCall<void>(this, 0x7c, 0, 0, 0);
        } else {
            VCall<void>(this, 0x7c, 1, 0, 0);
        }
        VCall<void>(this, 0x90, b, 0, 0);
        b->ReleaseReference();
    }
    const StaticBuffer* const ib = VCall<const StaticBuffer*>(this, 0x94, 0);
    if (f & 0x100 || ib) return true;
    StaticBuffer* const b = VCall<StaticBuffer*>(display, 0x108, indexArray.Count(), 0);
    if (!b) return false;
    if (upload) {
        unsigned long out;
        if (VCall<int>(BufferManager(display), 0x2c, b, IndexData(indexArray.block), numIndices, 1, &out) == -1) {
            b->ReleaseReference();
            return false;
        }
        VCall<void>(this, 0x84, 0);
    } else {
        VCall<void>(this, 0x84, 1);
    }
    VCall<void>(this, 0x98, b, 0);
    b->ReleaseReference();
    return true;
}

// 0x10024960: the static buffers filled again (false when one is missing)
bool __cdecl GenericGeometryChunkSingleStream::LoadBuffers(Display* display)
{
    void* const manager = BufferManager(display);
    const unsigned long f = flags;
    if (!(f & 0x10000)) {
        const StaticBuffer* const vb = VCall<const StaticBuffer*>(this, 0x8c, 0, 0);
        if (!vb) return false;
        unsigned long out;
        void* const v = VCall<void*>(this, 0x34, 0, 0);
        if (!VCall<bool>(manager, 0x20, vertexType, vb, v, numVertices, 1, &out, 0)) return false;
        VCall<void>(this, 0x7c, 0, 0, 0);
    }
    if (f & 0x100) return true;
    const StaticBuffer* const ib = VCall<const StaticBuffer*>(this, 0x94, 0);
    if (!ib) return false;
    unsigned long out;
    if (VCall<int>(manager, 0x2c, ib, IndexData(indexArray.block), numIndices, 1, &out) == -1) return false;
    VCall<void>(this, 0x84, 0);
    return true;
}

// 0x10024a40
void __cdecl GenericGeometryChunkSingleStream::DuplicateVertexArray(Display* display, int, int)
{
    if (!vertices.block) return;
    vertices.Duplicate();
    if (VCall<const StaticBuffer*>(this, 0x8c, 0, 0)) {
        VCall<void>(this, 0x90, nullptr, 0, 0);
        VCall<void>(this, 0x10, display, 0);
    }
}

// 0x10024220
void* __cdecl GenericGeometryChunkSingleStream::GetVertexArray(int, int)
{
    return vertices.block && vertices.block->count ? vertices.block->data : nullptr;
}
const void* __cdecl GenericGeometryChunkSingleStream::GetVertexArray(int, int) const
{
    return vertices.block && vertices.block->count ? vertices.block->data : nullptr;
}

const VertexType* __cdecl GenericGeometryChunkSingleStream::AssignVertexType(Display*) const  // 0x10022f60
{
    return vertexType;
}

VertexTypeID __cdecl GenericGeometryChunkSingleStream::GetVertexTypeID(int) const  // 0x10022f80
{
    return vertexType->GetVertexTypeID();
}

int __cdecl GenericGeometryChunkSingleStream::GetVertexSize(int) const  // 0x10024240
{
    return vertexType->GetStreamList()[0].stride;
}

int __cdecl GenericGeometryChunkSingleStream::GetItemOffset(VertexItemType type, int) const  // 0x10024250
{
    return offsetTable->offsets[type];
}

namespace {

// 0x10024260: every item view of a single stream chunk
template <class P>
P SingleItem(const GenericGeometryChunkSingleStream::ItemOffsetTable* t, const GenericReferenceArray& v,
             VertexItemType type)
{
    P r;
    const int offset = t->offsets[type];
    if (offset < 0) {
        r.stride = 0;
        r.data = nullptr;
        return r;
    }
    r.stride = t->stride;
    r.data = reinterpret_cast<decltype(r.data)>(reinterpret_cast<std::uintptr_t>(v.block) + 0x10 + offset);
    return r;
}

}  // namespace

#define JET_SINGLE_ITEM(Getter, Ptr, ConstPtr)                                                                  \
    GeometryChunk::Ptr __cdecl GenericGeometryChunkSingleStream::Getter(VertexItemType type, int)               \
    {                                                                                                           \
        return SingleItem<Ptr>(offsetTable, vertices, type);                                                    \
    }                                                                                                           \
    GeometryChunk::ConstPtr __cdecl GenericGeometryChunkSingleStream::Getter(VertexItemType type, int) const    \
    {                                                                                                           \
        return SingleItem<ConstPtr>(offsetTable, vertices, type);                                               \
    }
JET_SINGLE_ITEM(GetVector2Array, Vector2Ptr, ConstVector2Ptr)
JET_SINGLE_ITEM(GetVector3Array, Vector3Ptr, ConstVector3Ptr)
JET_SINGLE_ITEM(GetVector4Array, Vector4Ptr, ConstVector4Ptr)
JET_SINGLE_ITEM(GetFloatArray, FloatPtr, ConstFloatPtr)
JET_SINGLE_ITEM(GetUDwordArray, UDwordPtr, ConstUDwordPtr)
#undef JET_SINGLE_ITEM

// ---------------------------------------------------------------------------------------------------------------
// GenericGeometryChunkMultiStream

// 0x10022da0
GenericGeometryChunkMultiStream::ItemOffsetTable::ItemOffsetTable(unsigned int streamCount) : size(0)
{
    strides = static_cast<unsigned int*>(Alloc::New(Times(streamCount, 4)));
    for (Entry& e : entries) e = {0, -1};
}

// 0x10022ea0
GenericGeometryChunkMultiStream::ItemOffsetTable::~ItemOffsetTable()
{
    if (strides) Alloc::Delete(strides);
    strides = nullptr;
}

namespace {

// 0x10023b10: the stream arrays let go (each one as well once the block goes; a still shared block stays put)
void ReleaseArrays(GenericGeometryChunkMultiStream::Arrays*& a)
{
    if (!a || --a->refs) return;
    for (unsigned int k = 0; k < a->count; ++k) a->data[k].ReleaseReference();
    Alloc::Delete(a);
    a = nullptr;
}

}  // namespace

// 0x10023000
GenericGeometryChunkMultiStream::GenericGeometryChunkMultiStream(int n)
    : GeometryChunkMultiStream(n), arrays(nullptr), offsetTable(nullptr)
{
}

// 0x10023030: `f` goes to the stream count and the chunk gets flags 4 (as in TS12)
GenericGeometryChunkMultiStream::GenericGeometryChunkMultiStream(VertexType* type, unsigned int v,
                                                                 unsigned int primitives, unsigned int indices,
                                                                 unsigned int faceNormals, unsigned int f)
    : GeometryChunkMultiStream(v, primitives, indices, faceNormals, static_cast<int>(f), 4), arrays(nullptr),
      offsetTable(nullptr)
{
    vertexType = type;
    if (type) {
        type->AddReference();
        CreateVertexArray(type, v);
    }
}

// 0x100230d0
GenericGeometryChunkMultiStream::~GenericGeometryChunkMultiStream()
{
    if (offsetTable) offsetTable->ReleaseReference();
    ReleaseArrays(arrays);
}

// 0x10023140: an array of `n` vertices per stream of the type; the offset table only when there is none yet
void __cdecl GenericGeometryChunkMultiStream::CreateVertexArray(VertexType* type, int n)
{
    const int count = static_cast<int>(type->GetNumStreams());
    numStreams = count;
    ReleaseArrays(arrays);
    arrays = static_cast<Arrays*>(Alloc::New(count * 4 + 0x10));
    arrays->count = 0;
    arrays->refs = 1;
    for (unsigned int k = 0; k < static_cast<unsigned int>(numStreams); ++k) {
        GenericReferenceArray* const at = &arrays->data[arrays->count + k];
        if (at) at->block = nullptr;
    }
    arrays->count += numStreams;
    if (!offsetTable) {
        offsetTable = new ItemOffsetTable(numStreams);
        for (int t = 0; t < 39; ++t) {
            offsetTable->entries[t].offset = type->GetItemOffset(static_cast<VertexItemType>(t));
            offsetTable->entries[t].stream = type->GetItemStream(static_cast<VertexItemType>(t));
        }
    }
    for (int s = 0; s < numStreams; ++s) {
        const unsigned int stride = type->GetStreamList()[s].stride;
        GenericReferenceArray& a = arrays->data[s];
        a.Create(n, stride);
        a.block->count += n;
        offsetTable->strides[s] = stride;
        offsetTable->size += stride;
    }
}

// 0x10023300
GeometryChunk* __cdecl GenericGeometryChunkMultiStream::CreateInstance() const
{
    auto* const c = new GenericGeometryChunkMultiStream(numStreams);
    InitInstance(*c);
    return c;
}

// 0x100233c0: made with the vertex type (so with arrays and a table), then the table and arrays made again
// from this chunk's (the first ones are leaked, as in TS12)
GeometryChunk* __cdecl GenericGeometryChunkMultiStream::CreateInstance(unsigned int v, unsigned int primitives,
                                                                        unsigned int indices,
                                                                        unsigned int faceNormals) const
{
    auto* const c = new GenericGeometryChunkMultiStream(vertexType, v, primitives, indices, faceNormals, flags);
    if (offsetTable) {
        offsetTable->AddReference();
        c->offsetTable = offsetTable;
    }
    if (vertexType) {
        c->vertexType = vertexType;
        vertexType->AddReference();
        c->CreateVertexArray(vertexType, v);
    }
    return c;
}

// 0x10023480
void __cdecl GenericGeometryChunkMultiStream::InitInstance(GenericGeometryChunkMultiStream& to) const
{
    GeometryChunkMultiStream::InitInstance(to);
    if (to.arrays != arrays) {
        ReleaseArrays(to.arrays);
        to.arrays = arrays;
        if (arrays) ++arrays->refs;
    }
    to.offsetTable = offsetTable;
    if (offsetTable) offsetTable->AddReference();
}

// 0x100234d0: per stream as GenericGeometryChunkSingleStream does (a stream counts as made when dynamic or with a
// buffer), then the indices
bool __cdecl GenericGeometryChunkMultiStream::InitializeBuffers(Display* display, bool upload)
{
    for (int s = 0; s < numStreams; ++s) {
        const StaticBuffer* const vb = VCall<const StaticBuffer*>(this, 0x8c, 0, s);
        if (VCall<bool>(this, 0x68, 0, s) || vb) continue;
        if (!vertexType) return false;
        StaticBuffer* const b =
            VCall<StaticBuffer*>(display, 0x104, maxVertices, vertexType->GetStreamList()[s].stride);
        if (!b) return false;
        if (upload) {
            void* const manager = BufferManager(display);
            unsigned long out;
            void* const v = VCall<void*>(this, 0x34, 0, s);
            if (!VCall<bool>(manager, 0x20, vertexType, b, v, numVertices, 1, &out, 0)) {
                b->ReleaseReference();
                return false;
            }
            VCall<void>(this, 0x7c, 0, 0, s);
        } else {
            VCall<void>(this, 0x7c, 1, 0, s);
        }
        VCall<void>(this, 0x90, b, 0, s);
        b->ReleaseReference();
    }
    const StaticBuffer* const ib = VCall<const StaticBuffer*>(this, 0x94, 0);
    if (VCall<bool>(this, 0x78) || ib) return true;
    StaticBuffer* const b = VCall<StaticBuffer*>(display, 0x108, indexArray.Count(), 0);
    if (!b) return false;
    if (upload) {
        unsigned long out;
        if (VCall<int>(BufferManager(display), 0x2c, b, IndexData(indexArray.block), numIndices, 1, &out) == -1) {
            b->ReleaseReference();
            return false;
        }
        VCall<void>(this, 0x84, 0);
    } else {
        VCall<void>(this, 0x84, 1);
    }
    VCall<void>(this, 0x98, b, 0);
    b->ReleaseReference();
    return true;
}

// 0x100236e0
bool __cdecl GenericGeometryChunkMultiStream::LoadBuffers(Display* display)
{
    void* const manager = BufferManager(display);
    for (int s = 0; s < numStreams; ++s) {
        if (VCall<bool>(this, 0x68, 0, s)) continue;
        const StaticBuffer* const vb = VCall<const StaticBuffer*>(this, 0x8c, 0, s);
        if (!vb) return false;
        unsigned long out;
        void* const v = VCall<void*>(this, 0x34, 0, s);
        if (!VCall<bool>(manager, 0x20, vertexType, vb, v, numVertices, 1, &out, 0)) return false;
        VCall<void>(this, 0x7c, 0, 0, s);
    }
    if (VCall<bool>(this, 0x78)) return true;
    const StaticBuffer* const ib = VCall<const StaticBuffer*>(this, 0x94, 0);
    if (!ib) return false;
    unsigned long out;
    if (VCall<int>(manager, 0x2c, ib, IndexData(indexArray.block), numIndices, 1, &out) == -1) return false;
    VCall<void>(this, 0x84, 0);
    return true;
}

// 0x100237f0
void __cdecl GenericGeometryChunkMultiStream::DuplicateVertexArray(Display* display, int i, int stream)
{
    int s = stream;
    int end = stream + 1;
    if (stream == -2) {
        s = 0;
        end = numStreams;
    }
    for (; s < end; ++s) {
        arrays->data[s].Duplicate();
        if (VCall<const StaticBuffer*>(this, 0x8c, i, s)) {
            VCall<void>(this, 0x90, nullptr, i, s);
            VCall<void>(this, 0x10, display, 0);
        }
    }
}

// 0x10022f30
void* __cdecl GenericGeometryChunkMultiStream::GetVertexArray(int, int stream)
{
    const GenericReferenceArray::Block* const b = arrays->data[stream].block;
    return b && b->count ? const_cast<unsigned char*>(b->data) : nullptr;
}
const void* __cdecl GenericGeometryChunkMultiStream::GetVertexArray(int, int stream) const
{
    const GenericReferenceArray::Block* const b = arrays->data[stream].block;
    return b && b->count ? b->data : nullptr;
}

const VertexType* __cdecl GenericGeometryChunkMultiStream::AssignVertexType(Display*) const  // 0x10022f60
{
    return vertexType;
}

VertexTypeID __cdecl GenericGeometryChunkMultiStream::GetVertexTypeID(int) const  // 0x10022f80
{
    return vertexType->GetVertexTypeID();
}

int __cdecl GenericGeometryChunkMultiStream::GetVertexSize(int) const { return offsetTable->size; }  // 0x10022f70

int __cdecl GenericGeometryChunkMultiStream::GetItemOffset(VertexItemType type, int) const  // 0x10022f90
{
    return offsetTable->entries[type].offset;
}

int __cdecl GenericGeometryChunkMultiStream::GetItemStream(VertexItemType type, int) const  // 0x10022fa0
{
    return offsetTable->entries[type].stream;
}

namespace {

// 0x10022fb0: every item view of a multi stream chunk
template <class P>
P MultiItem(const GenericGeometryChunkMultiStream::ItemOffsetTable* t,
            const GenericGeometryChunkMultiStream::Arrays* a, VertexItemType type)
{
    P r;
    const int offset = t->entries[type].offset;
    if (offset < 0) {
        r.stride = 0;
        r.data = nullptr;
        return r;
    }
    const int s = t->entries[type].stream;
    r.stride = t->strides[s];
    r.data = reinterpret_cast<decltype(r.data)>(reinterpret_cast<std::uintptr_t>(a->data[s].block) + 0x10 + offset);
    return r;
}

}  // namespace

#define JET_MULTI_ITEM(Getter, Ptr, ConstPtr)                                                                   \
    GeometryChunk::Ptr __cdecl GenericGeometryChunkMultiStream::Getter(VertexItemType type, int)                \
    {                                                                                                           \
        return MultiItem<Ptr>(offsetTable, arrays, type);                                                       \
    }                                                                                                           \
    GeometryChunk::ConstPtr __cdecl GenericGeometryChunkMultiStream::Getter(VertexItemType type, int) const     \
    {                                                                                                           \
        return MultiItem<ConstPtr>(offsetTable, arrays, type);                                                  \
    }
JET_MULTI_ITEM(GetVector2Array, Vector2Ptr, ConstVector2Ptr)
JET_MULTI_ITEM(GetVector3Array, Vector3Ptr, ConstVector3Ptr)
JET_MULTI_ITEM(GetVector4Array, Vector4Ptr, ConstVector4Ptr)
JET_MULTI_ITEM(GetFloatArray, FloatPtr, ConstFloatPtr)
JET_MULTI_ITEM(GetUDwordArray, UDwordPtr, ConstUDwordPtr)
#undef JET_MULTI_ITEM

}  // namespace Jet
