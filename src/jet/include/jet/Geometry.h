// RenderSystem_7c unit U5 (notes/RenderSystem.md), geometry. U5a: the vertex type (the streams and items a vertex
// format is made of, and the registry of named types), the rendering caches and their manager, the vertex buffer
// manager base, StaticBuffer and the chunk creation hints. Layouts follow the TS12 assembly (addresses per member in
// RenderSystem_7c/VertexType.cpp and RenderCaches.cpp).
#pragma once

#include <jet/FastDynamicArray.h>
#include <jet/GfxLanguage.h>
#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/SmallTypes.h>
#include <jet/VertexFormats.h>

namespace Jet {

class Display;
class Options;
class ViewPort;

enum DVertexItemDataType {};     // 0..8 (sizes in VertexType::GetElementTypeSize); 9 "none"
enum VertexItemSemanticType {};  // 0 "none"

// A vertex format (0x24 bytes): streams of items, each item a vertex attribute at an offset of its stream's
// vertex. The API module keeps its own data for it (apiData, made and freed through the static `display`). Built
// by the Display (CreateStreams, CreateItems, AddItem), registered by name in a hash table of RegistrationData.
class VertexType : public ReferenceCount {
public:
    enum BoolEnum {};  // -1: the data type's default, 1 true, otherwise false

    struct Item {  // 0x20
        Item() {}  // 0x100567a0: only the name (the rest is whatever the heap held)
        VertexItemType type;  // +0
        PString name;         // +4
        int offset;           // +8 (-1: placed by ConfigureStream)
        int count;            // +0xc elements
        int dataType;         // +0x10 DVertexItemDataType
        int semantic;         // +0x14 VertexItemSemanticType
        int sharedWith;       // +0x18 the item type whose data this one shares (-1: none)
        bool flag;            // +0x1c (AddItem's BoolEnum)
    };
    struct Stream {  // 0x10
        unsigned int stride;    // +0 the vertex size (ConfigureStream, once the stream is full)
        unsigned int capacity;  // +4 items made by CreateItems
        unsigned int count;     // +8 items added
        Item* items;            // +0xc (an Alloc block: the count, then the items)
    };
    struct RegistrationData {  // 8
        VertexType* type;      // referenced
        PString name;
        JET_HEAP_OBJECT
    };
    struct RegistrationDataOp {};

    explicit VertexType(VertexTypeID id);
    VertexType(const VertexType& o);

    virtual bool __cdecl operator==(const VertexType& o) const;  // slot 2
    virtual bool __cdecl CompareVertex(const VertexType& o) const;  // slot 3: the streams, item by item

    void __cdecl CreateStreams(unsigned int count);
    void __cdecl CreateItems(unsigned int stream, unsigned int count);
    void __cdecl AddItem(unsigned int stream, VertexItemType type, int count, DVertexItemDataType dataType,
                         VertexItemSemanticType semantic, const PString& name, int offset, BoolEnum flag);
    void __cdecl AddItem(VertexItemType type, VertexItemType sharedWith, const PString& name);
    void __cdecl SetApiData(const Display* display, unsigned long data);
    unsigned long __cdecl GetApiData() const;
    unsigned long __cdecl GetApiFlags() const;
    void __cdecl SetApiFlags(unsigned long f);
    unsigned long __cdecl GetUserData() const;
    void __cdecl SetUserData(unsigned long d);
    VertexTypeID __cdecl GetVertexTypeID() const;
    unsigned int __cdecl GetNumStreams() const;
    const Stream* __cdecl GetStreamList() const;
    unsigned int __cdecl GetVertexSize(unsigned int stream) const;
    const Item* __cdecl GetItem(VertexItemType type) const;
    int __cdecl GetItemOffset(VertexItemType type) const;
    int __cdecl GetItemStream(VertexItemType type) const;
    DVertexItemDataType __cdecl GetItemDataType(VertexItemType type) const;
    VertexItemSemanticType __cdecl GetItemSemanticType(VertexItemType type) const;
    int __cdecl GetItemElementCount(VertexItemType type) const;
    bool __cdecl GetNamedItemData(const PString& name, unsigned int& stream, unsigned int& item, const Stream*& s,
                                  const Item*& i) const;
    void __cdecl GetVertexItemTypes(FastDynamicArray<VertexItemType, 13, Less<VertexItemType, VertexItemType>>& out)
        const;
    int __cdecl GetCompatibleVertexItemTypes(VertexItemSemanticType semantic, VertexItemType* out, int max) const;
    const PString& __cdecl GetRegisteredName() const;
    bool __cdecl RegisterName(const PString& name);  // false when the name is taken (the name is kept anyway)

    static unsigned int __cdecl GetElementTypeSize(DVertexItemDataType t);
    static const PString& __cdecl GetDefaultVertexItemName(VertexItemType t);
    static VertexType* __cdecl GetVertexType(const PString& name);  // referenced, or null
    static void __cdecl Shutdown();                                  // empties the registry

    JET_HEAP_OBJECT

protected:
    ~VertexType() override;
    void __cdecl ConfigureStream(Stream* s);  // offsets and stride, once every item of `s` is added

    VertexTypeID id;              // +8
    unsigned int streamCount;     // +0xc
    Stream* streams;              // +0x10 (an Alloc block)
    unsigned long apiData;        // +0x14
    unsigned long apiFlags;       // +0x18
    mutable unsigned long user;   // +0x1c
    PString registeredName;       // +0x20

    // the module statics stay the original's until U9 (the Display that sets them is the original's)
    static __declspec(dllimport) const Display* const display;
    static __declspec(dllimport) int nextFreeId;
    static __declspec(dllimport) HashTable<unsigned int, RegistrationData*, RegistrationDataOp, 13> registrations;
};
JET_LAYOUT32(sizeof(VertexType) == 0x24);

class RenderingCacheManager;

// A cache the 3D layers flush in priority order (0x28 bytes with its virtual ReferenceCount at +0x20): linked into
// its manager's list through its IListNode (+4).
class RenderingCache : public IListNode, public virtual ReferenceCount {
public:
    RenderingCache(const PString& name, float priority);
    ~RenderingCache() override;  // out of its manager

    virtual void __cdecl SetHost(RenderingCacheManager* host);        // 0
    virtual void __cdecl Activate(Display* display, ViewPort* view);  // 1: nothing
    virtual void __cdecl Reset();                                     // 2: nothing
    virtual void __cdecl Begin(Display* display, ViewPort* view);     // 3: nothing
    virtual void __cdecl End(Display* display, ViewPort* view);       // 4: nothing
    virtual void __cdecl Flush(Display* display, ViewPort* view);     // 5: End, then Begin
    virtual void __cdecl Enter(Display* display, ViewPort* view, bool b);  // 6: nothing
    virtual void __cdecl Leave(Display* display, ViewPort* view, bool b);  // 7: nothing

    RenderingCacheManager* __cdecl GetHost() const;
    const PString& __cdecl GetName() const;
    float __cdecl GetPriority() const;
    void __cdecl SetPriority(float p);  // moved to its place in the host's list
    bool __cdecl operator<(const RenderingCache& o);
    bool __cdecl IsEnabled() const;
    void __cdecl Enable(bool on);
    bool __cdecl GetRendersGeometry() const;
    void __cdecl SetRendersGeometry(bool on);

    JET_HEAP_OBJECT

protected:
    friend class RenderingCacheManager;
    PString name;                 // +0x10
    float priority;               // +0x14
    RenderingCacheManager* host;  // +0x18
    bool rendersGeometry;         // +0x1c
    bool enabled;                 // +0x1d
};
JET_LAYOUT32(sizeof(RenderingCache) == 0x28);

// The caches of a 3D scene (0x10 bytes): a ring through the caches' IListNode, by rising priority. Begin remembers
// whether only the caches that render geometry take part.
class RenderingCacheManager {
public:
    RenderingCacheManager();
    ~RenderingCacheManager();  // resets and lets go of every cache
    RenderingCacheManager& operator=(const RenderingCacheManager& o);  // a shallow copy (0x1003e930)

    void __cdecl AddCache(RenderingCache* cache);
    void __cdecl RemoveCache(RenderingCache* cache);
    RenderingCache* __cdecl FindCache(const PString& name);
    void __cdecl Begin(Display* display, ViewPort* view, bool geometryOnly);
    void __cdecl End(Display* display, ViewPort* view);
    void __cdecl Enter(Display* display, ViewPort* view, bool b);
    void __cdecl Leave(Display* display, ViewPort* view, bool b);

private:
    IListNode* next;     // +0 the first cache's node (the head is this object)
    IListNode* prev;     // +4
    unsigned int count;  // +8
    bool geometryOnly;   // +0xc
};
JET_LAYOUT32(sizeof(RenderingCacheManager) == 0x10);

// A buffer the API module keeps in video memory (8 bytes here; the API modules derive from it).
class StaticBuffer : public ReferenceCount {
public:
    StaticBuffer();
    JET_HEAP_OBJECT

protected:
    ~StaticBuffer() override;
};
JET_LAYOUT32(sizeof(StaticBuffer) == 8);

// The base of the API modules' vertex buffer managers (0x78 bytes): the dynamic buffer sizes (options
// "DynamicVertexBufferSize" / "DynamicIndexBufferSize") and the buffer statistics, guarded by a critical section.
// Every other slot does nothing here (the ones without a name answer 0).
class VertexBufferManager {
public:
    struct BufferStats {  // 0x48
        unsigned long v[0x12];
    };

    virtual ~VertexBufferManager();                                             // 0
    virtual bool __cdecl Initialize(Display* display, const Options* options);  // 1: keeps the display, true
    virtual int __cdecl Slot2();
    virtual int __cdecl Slot3();
    virtual void __cdecl Update();  // 4
    virtual int __cdecl Slot5();
    virtual int __cdecl Slot6();
    virtual int __cdecl Slot7();
    virtual int __cdecl Slot8();
    virtual int __cdecl Slot9();
    virtual int __cdecl Slot10();
    virtual int __cdecl Slot11();
    virtual int __cdecl Slot12();
    virtual int __cdecl Slot13();
    virtual void __cdecl FlushState();                         // 14
    virtual void __cdecl ResetDynamicBufferForReuse();         // 15
    virtual void __cdecl ResetDynamicBufferOffsets();          // 16
    virtual void __cdecl DrawComplete(const StaticBuffer* b);  // 17/18 (overloads, in some order)
    virtual void __cdecl DrawComplete(const VertexType& t);
    virtual int __cdecl Slot19();

protected:
    virtual void __cdecl GetConfiguration(const Options* options);  // 20

public:
    virtual int __cdecl Slot21();
    virtual int __cdecl Slot22();

    unsigned int __cdecl GetDefaultDynamicBufferSize() const;
    unsigned int __cdecl GetDefaultDynamicIndexSize() const;
    BufferStats __cdecl GetBufferStats() const;

    JET_HEAP_OBJECT

protected:
    VertexBufferManager();

    Display* display;                 // +4
    unsigned int dynamicBufferSize;   // +8 (0x108000)
    unsigned int dynamicIndexSize;    // +0xc (0xffff)
    BufferStats stats;                // +0x10
    unsigned long more[5];            // +0x58
    CriticalSection lock;             // +0x6c
};
JET_LAYOUT32(sizeof(VertexBufferManager) == 0x78);

// How a geometry chunk is to be made (0x10): system and user hint bits.
class GeometryChunkCreateHint {
public:
    GeometryChunkCreateHint();
    GeometryChunkCreateHint(unsigned long system, unsigned long user);
    GeometryChunkCreateHint& operator=(const GeometryChunkCreateHint& o);
    bool __cdecl SupportsSystemHint(unsigned long h) const;
    bool __cdecl SupportsUserHint(unsigned long h) const;

    unsigned long version;  // +0 (2)
    unsigned long size;     // +4 (2)
    unsigned long system;   // +8
    unsigned long user;     // +0xc
};
JET_LAYOUT32(sizeof(GeometryChunkCreateHint) == 0x10);

// system bit 1 (0x20)
class StandardChunkHint : public GeometryChunkCreateHint {
public:
    StandardChunkHint();
    StandardChunkHint(unsigned long system, unsigned long user);
    StandardChunkHint& operator=(const StandardChunkHint& o);

    unsigned char bits : 4;  // +0x10 (cleared)
    unsigned long a;         // +0x14
    unsigned long b;         // +0x18
    unsigned long c;         // +0x1c (1)
};
JET_LAYOUT32(sizeof(StandardChunkHint) == 0x20);

// system bits 3 (0x24)
class DatabaseChunkHint : public StandardChunkHint {
public:
    DatabaseChunkHint();
    DatabaseChunkHint(unsigned long system, unsigned long user);
    DatabaseChunkHint& operator=(const DatabaseChunkHint& o);

    unsigned long database;  // +0x20
};
JET_LAYOUT32(sizeof(DatabaseChunkHint) == 0x24);

// system bits 5 (0x24)
class TerrainMorphChunkHint : public StandardChunkHint {
public:
    TerrainMorphChunkHint();
    TerrainMorphChunkHint(unsigned long system, unsigned long user);
    TerrainMorphChunkHint& operator=(const TerrainMorphChunkHint& o);

    bool morph;  // +0x20
};
JET_LAYOUT32(sizeof(TerrainMorphChunkHint) == 0x24);

// ---- U5b (RenderSystem_7c/GeometryChunk.cpp)

class Material;
class DisplayLight;
class Vector2;
class Vector4;
struct GeometryStreamType;
enum DPrimitive {};  // the API primitive (GeometryChunk::primitiveLookup maps the chunk's primitive bits to it)

// A shared, reference-counted array: one pointer to an Alloc block {references, count, two unused words, items}.
// Passed by value, the callee lets go of its copy.
template <class T>
class AutoReferenceArray {
public:
    struct Block {
        int refs;
        unsigned int count;
        int unused[2];
        T data[1];
    };
    AutoReferenceArray() : block(nullptr) {}
    AutoReferenceArray(const AutoReferenceArray& o) : block(o.block)
    {
        if (block) ++block->refs;
    }
    ~AutoReferenceArray() { Release(); }
    AutoReferenceArray& operator=(const AutoReferenceArray&) = delete;

    // a new block of `n` items (one reference)
    static Block* Make(unsigned int n)
    {
        Block* const b = static_cast<Block*>(Alloc::New(n * sizeof(T) + 0x10));
        b->count = 0;
        b->refs = 1;
        b->count += n;
        return b;
    }
    // shares `b` (the inlined assignment: the old block goes first unless it is the same)
    void Share(Block* b)
    {
        if (block == b) return;
        Release();
        block = b;
        if (b) ++b->refs;
    }
    void Release()
    {
        if (block) {
            --block->refs;
            if (!block->refs) {
                if (block) Alloc::Delete(block);
                block = nullptr;
            }
        }
    }
    static void Drop(Block* b)  // a temporary's reference
    {
        if (!--b->refs) Alloc::Delete(b);
    }
    unsigned int Count() const { return block ? block->count : 0; }
    T* Data() const { return block ? block->data : nullptr; }

    Block* block;
};

// A geometry chunk, the base of the API modules' vertex/index buffers (0x4c bytes on ReferenceCount): the
// primitive type and state flags, the counts, the shared index / face neighbour / face normal arrays, named float
// properties, the bone matrices it is skinned by. Every slot answering 0 is pure (the module's _purecall answers
// 0); the stream chunks (U5c) fill them.
class GeometryChunk : public ReferenceCount {
public:
    struct FaceNeighbors {
        int face[3];
    };
    struct ChunkProperty {  // 8
        PStringCache::Node* name;
        float value;
    };
    struct PropertyChunk {  // 0x24: a FastDynamicArray<ChunkProperty, 3> chunk
        PropertyChunk* next;
        PropertyChunk* prev;
        unsigned int count;
        ChunkProperty data[3];
    };
    struct RegistrationData {  // 8
        PString name;
        GeometryChunk*(__cdecl* create)();
        JET_HEAP_OBJECT
    };
    struct RegistrationDataOp {};
    using FloatPtr = VertexItemPtr<float, VertexItemPtr<const float, VIPConstHelper>>;
    using ConstFloatPtr = VertexItemPtr<const float, VIPConstHelper>;
    using UDwordPtr = VertexItemPtr<unsigned long, VertexItemPtr<const unsigned long, VIPConstHelper>>;
    using ConstUDwordPtr = VertexItemPtr<const unsigned long, VIPConstHelper>;
    using Vector2Ptr = VertexItemPtr<Vector2, VertexItemPtr<const Vector2, VIPConstHelper>>;
    using ConstVector2Ptr = VertexItemPtr<const Vector2, VIPConstHelper>;
    using Vector3Ptr = VertexItemPtr<Vector3, VertexItemPtr<const Vector3, VIPConstHelper>>;
    using ConstVector3Ptr = VertexItemPtr<const Vector3, VIPConstHelper>;
    using Vector4Ptr = VertexItemPtr<Vector4, VertexItemPtr<const Vector4, VIPConstHelper>>;
    using ConstVector4Ptr = VertexItemPtr<const Vector4, VIPConstHelper>;
    using ItemTypes = FastDynamicArray<VertexItemType, 13, Less<VertexItemType, VertexItemType>>;

    GeometryChunk();
    GeometryChunk(unsigned int vertices, unsigned int primitives, unsigned int indices, unsigned int faceNormals,
                  unsigned int flags);

    // 2/3 (MSVC lays out overloaded virtuals in reverse order of declaration: 2 is the second one here)
    virtual GeometryChunk* __cdecl CreateInstance() const = 0;
    virtual GeometryChunk* __cdecl CreateInstance(unsigned int vertices, unsigned int primitives,
                                                  unsigned int indices, unsigned int faceNormals) const = 0;
    virtual bool __cdecl InitializeBuffers(Display* display, bool upload) = 0;  // 4 (its body answers true)
    virtual bool __cdecl LoadBuffers(Display* display) = 0;                     // 5
    virtual void __cdecl DuplicateUnsharedArrays(Display* display);  // 6: every vertex array not shared
    virtual void __cdecl DuplicateVertexArray(Display* display, int i, int stream) = 0;  // 7
    virtual void __cdecl DuplicateIndexArray(Display* display);
    virtual void __cdecl ResizeVertexArray(Display* display, unsigned int n, int i, int j);  // 9: nothing
    virtual void __cdecl ResizeIndexArray(Display* display, unsigned int n);
    virtual void __cdecl AppendChunk(Display* display, const GeometryChunk& o);  // 11
    virtual void* __cdecl GetVertexArray(int i, int stream) = 0;              // 13
    virtual const void* __cdecl GetVertexArray(int i, int stream) const = 0;  // 12
    virtual void __cdecl RenderGeometry(Display* display, ViewPort* view, const VertexProgram* program, bool bones,
                                        int stream) const = 0;  // 14
    virtual void __cdecl RenderPrimitives(Display* display, ViewPort* view, const VertexProgram* program,
                                          unsigned int numIndices, const unsigned short* indices,
                                          unsigned int numPrimitives, DPrimitive primitive, bool bones,
                                          int stream) const = 0;  // 15
    virtual bool __cdecl SetupMultipassRender(Display* display, ViewPort* view, const VertexProgram* program,
                                              const unsigned short* indices, unsigned int numIndices) const = 0;
    virtual bool __cdecl SetupMultipassRender(Display* display, ViewPort* view, const unsigned short* indices,
                                              unsigned int numIndices) const = 0;  // 16 (17 the one above)
    virtual void __cdecl BindVertexProgram(const VertexProgram* old, const VertexProgram* program,
                                           bool bones) const = 0;  // 19
    virtual void __cdecl BindVertexProgram(int pass, bool bones) const = 0;  // 18
    virtual void __cdecl RenderGeometryPass(Display* display, ViewPort* view, int pass) const = 0;  // 21
    virtual void __cdecl RenderGeometryPass(Display* display, ViewPort* view, int count, DPrimitive primitive,
                                            int pass) const = 0;  // 20
    virtual void __cdecl CleanupMultipassRender(Display* display, const VertexProgram* program) const = 0;  // 22
    virtual int __cdecl GetNumPasses() const = 0;   // 23
    virtual int __cdecl GetNumStreams() const = 0;  // 24
    virtual void __cdecl SetVertexDataDynamic(int i, int stream, bool on) = 0;  // 25
    virtual bool __cdecl GetVertexDataDynamic(int i, int stream) const = 0;
    virtual void __cdecl SetVertexDataShared(int i, int stream, bool on) = 0;
    virtual bool __cdecl GetVertexDataShared(int i, int stream) const = 0;  // 28
    virtual void __cdecl SetIndexDataDynamic();        // 29
    virtual bool __cdecl GetIndexDataDynamic() const;  // 30
    virtual void __cdecl SetVertexDataDirty(bool on, int i, int stream) const = 0;  // 31
    virtual bool __cdecl GetVertexDataDirty(int i, int stream) const = 0;
    virtual void __cdecl SetIndexDataDirty(bool on) const;  // 33
    virtual bool __cdecl GetIndexDataDirty() const;         // 34
    virtual const StaticBuffer* __cdecl GetStaticVertexBuffer(int i, int stream) const = 0;  // 35
    virtual void __cdecl SetStaticVertexBuffer(const StaticBuffer* b, int i, int stream) const = 0;
    virtual const StaticBuffer* __cdecl GetStaticIndexBuffer(int i) const = 0;
    virtual void __cdecl SetStaticIndexBuffer(const StaticBuffer* b, int i) const = 0;  // 38
    virtual const VertexType* __cdecl AssignVertexType(Display* display) const = 0;      // 39
    virtual void __cdecl SetVertexType(VertexType* type, int stream) const = 0;
    virtual const VertexType& __cdecl GetVertexType(int stream) const = 0;
    virtual VertexTypeID __cdecl GetVertexTypeID(int stream) const = 0;
    virtual int __cdecl GetVertexSize(int stream) const = 0;
    virtual int __cdecl GetItemOffset(VertexItemType type, int stream) const = 0;
    virtual int __cdecl GetItemStream(VertexItemType type, int stream) const = 0;  // 45
    // 46..55 by pairs, each the const one first: Vector2, Vector3, Vector4, float, udword (the base udword
    // views are {4, null})
    virtual Vector2Ptr __cdecl GetVector2Array(VertexItemType type, int stream) = 0;
    virtual ConstVector2Ptr __cdecl GetVector2Array(VertexItemType type, int stream) const = 0;
    virtual Vector3Ptr __cdecl GetVector3Array(VertexItemType type, int stream) = 0;
    virtual ConstVector3Ptr __cdecl GetVector3Array(VertexItemType type, int stream) const = 0;
    virtual Vector4Ptr __cdecl GetVector4Array(VertexItemType type, int stream) = 0;
    virtual ConstVector4Ptr __cdecl GetVector4Array(VertexItemType type, int stream) const = 0;
    virtual FloatPtr __cdecl GetFloatArray(VertexItemType type, int stream) = 0;
    virtual ConstFloatPtr __cdecl GetFloatArray(VertexItemType type, int stream) const = 0;
    virtual UDwordPtr __cdecl GetUDwordArray(VertexItemType type, int index);
    virtual ConstUDwordPtr __cdecl GetUDwordArray(VertexItemType type, int index) const;
    virtual void __cdecl UpdateLighting(Display* display, const DisplayLight* light, Material* material,
                                        const Matrix4& a, const Matrix4& b);  // 56
    virtual void __cdecl ResetToAmbient(Display* display, const Material* material, const Color& ambient);
    virtual void __cdecl RenderDebug(Display* display) const;  // 58
    virtual void __cdecl SetVertexProgram(VertexProgram* program, int stream) = 0;     // 59
    virtual const VertexProgram* __cdecl GetVertexProgram(int stream) const = 0;       // 60
    // 61..70: the items of `types` in vertex type `stream` of `src` into this chunk's vertices (every one with
    // the same data type and count on both sides: floats of 1..4 elements, udwords of 1), then this chunk's slot
    // 31 (vertex data dirty). 61: vertex `dst` between src vertices i0 and i1 at position `pos`.
    virtual void __cdecl InterpolateChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                              unsigned int dst, const Vector3& pos, const ItemTypes& types,
                                              int stream);
    // 62..69 (MSVC lays out overloaded virtuals in reverse order of declaration, hence 69 first). 68/69: one src
    // vertex scaled by w[k]
    virtual void __cdecl InterpolateAllChunkData(const GeometryChunk* src, unsigned int i, const float* const w,
                                                 const ItemTypes& types, int stream);
    virtual void __cdecl InterpolateAllChunkData(const GeometryChunk* src, unsigned int i, const float* const w,
                                                 unsigned int first, unsigned int count, const ItemTypes& types,
                                                 int stream);
    // 65..67: from two src vertices (67: by the weights of the vertex positions along the segment)
    virtual void __cdecl InterpolateAllChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                 const ItemTypes& types, int stream);
    virtual void __cdecl InterpolateAllChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                 const Vector2* const w, const ItemTypes& types, int stream);
    virtual void __cdecl InterpolateAllChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                 const Vector2* const w, unsigned int first, unsigned int count,
                                                 const ItemTypes& types, int stream);
    // 62..64: from three src vertices: 62 vertices first..first+count-1 weighted by w[k], 63 all vertices, 64 all
    // vertices by the weights of their positions in the triangle
    virtual void __cdecl InterpolateAllChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                 unsigned int i2, const ItemTypes& types, int stream);
    virtual void __cdecl InterpolateAllChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                 unsigned int i2, const Vector3* const w, const ItemTypes& types,
                                                 int stream);
    virtual void __cdecl InterpolateAllChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1,
                                                 unsigned int i2, const Vector3* const w, unsigned int first,
                                                 unsigned int count, const ItemTypes& types, int stream);
    // 70: src vertices from.. to this chunk's to..
    virtual void __cdecl CopyChunkData(const GeometryChunk* src, unsigned int from, unsigned int to,
                                       unsigned int count, const ItemTypes& types, int stream);
    virtual bool __cdecl GetLoadChunkDataType(const Array<Array<GeometryStreamType, 1>, 2>& formats,
                                              unsigned int& index, bool& exact) const;  // 71
    virtual bool __cdecl LoadChunkData(const Array<GeometryStreamType, 1>& streams, void** const data);  // 72

    unsigned long __cdecl GetFlags() const;
    void __cdecl SetFlags(unsigned long f);
    bool __cdecl ContainsUnsharedData() const;
    unsigned int __cdecl GetNumVertices() const;
    void __cdecl SetNumVertices(unsigned int n);
    unsigned int __cdecl GetMaxVertices() const;
    void __cdecl SetMaxVertices(unsigned int n);
    unsigned int __cdecl GetNumIndices() const;
    void __cdecl SetNumIndices(unsigned int n);
    unsigned int __cdecl GetMaxIndices() const;
    unsigned int __cdecl GetNumPrimitives() const;
    void __cdecl SetNumPrimitives(unsigned int n);
    unsigned int __cdecl GetNumFaceNormals() const;
    void __cdecl SetNumFaceNormals(unsigned int n);
    unsigned int __cdecl GetNumFaceNeighbors() const;
    float __cdecl GetOpacity() const;
    void __cdecl SetOpacity(Display* display, float opacity);
    float __cdecl GetSurfaceArea() const;
    void __cdecl SetSurfaceArea(float a);
    unsigned long __cdecl GetUserData() const;
    void __cdecl SetUserData(unsigned long d);
    ReferenceCount* __cdecl GetRefefencedUserData() const;  // (sic)
    void __cdecl SetReferencedUserData(ReferenceCount* d);
    unsigned short* __cdecl GetIndexArray();
    const unsigned short* __cdecl GetIndexArray() const;
    void __cdecl SetIndexArray(AutoReferenceArray<unsigned short> a);
    Vector3* __cdecl GetFaceNormalArray();
    const Vector3* __cdecl GetFaceNormalArray() const;
    void __cdecl SetFaceNormalArray(AutoReferenceArray<Vector3> a);
    FaceNeighbors* __cdecl GetFaceNeighborArray();
    const FaceNeighbors* __cdecl GetFaceNeighborArray() const;
    void __cdecl SetFaceNeighborArray(AutoReferenceArray<FaceNeighbors>& a);
    void __cdecl DuplicateFaceNormalArray();
    void __cdecl DuplicateFaceNeighborArray();
    void __cdecl ResizeFaceNormalArray(unsigned int n);
    void __cdecl ResizeFaceNeighborArray(unsigned int n);
    bool __cdecl GetVertexIndex(int primitive, int vertex, unsigned int& index) const;
    unsigned int __cdecl GetVertexIndex(int primitive, int vertex) const;
    void __cdecl GetTriangleIndexes(int primitive, unsigned long& a, unsigned long& b, unsigned long& c) const;
    // vertex `dst` from src triangle (i0, i1, i2) by the weights of `pos` in it (nothing for a degenerate one)
    void __cdecl InterpolateChunkData(const GeometryChunk* src, unsigned int i0, unsigned int i1, unsigned int i2,
                                      unsigned int dst, const Vector3& pos, const ItemTypes& types, int stream);
    // the weights of every vertex position in triangle (a, b, c) (nothing written for a degenerate one)
    void __cdecl GetAllChunkWeights(const Vector3& a, const Vector3& b, const Vector3& c, Vector3* const out) const;
    bool __cdecl AddChunkProperty(const PString& name);  // 0.0, unless it exists
    bool __cdecl SetChunkProperty(const PString& name, float value);
    bool __cdecl GetChunkProperty(const PString& name, float& value) const;
    bool __cdecl RemoveChunkProperty(const PString& name);
    bool __cdecl ChunkPropertyExists(const PString& name) const;
    int __cdecl AddInfluenceMatrix(int index);  // its slot, made when new
    bool __cdecl AssignInfluenceMatrix(int index, const Matrix4* m);
    void __cdecl ResetMatrixArray();
    Array<VertexProgram::MatrixMap*, 128>& __cdecl GetInfluenceArray();
    const Array<VertexProgram::MatrixMap*, 128>& __cdecl GetInfluenceArray() const;
    void __cdecl SetVertexType(Display* display, VertexTypeID id, int stream) const;
    void __cdecl RenderChunkColor(Display* display) const;  // nothing
    void __cdecl RenderOutlines(Display* display) const;
    void __cdecl RenderVertexNormals(Display* display) const;
    void __cdecl RenderTriangleNormals(Display* display) const;
    void __cdecl RenderAxes(Display* display) const;

    static bool __cdecl RegisterName(const PString& name, GeometryChunk*(__cdecl* create)());
    static GeometryChunk* __cdecl CreateRegisteredInstance(const PString& name);
    static void __cdecl ReleaseRegistrationData();

    JET_HEAP_OBJECT

protected:
    ~GeometryChunk() override;
    void __cdecl InitInstance(GeometryChunk& to) const;  // shares the arrays, copies the counts and bones
    void __cdecl SetConstFlags(unsigned long f) const;

    // the chunk properties: FastDynamicArray<ChunkProperty, 3> (0x1002ada0 lets go of them)
    struct Properties {
        PropertyChunk* head = nullptr;
        ~Properties();
    };

    Properties properties;                              // +8
    Array<VertexProgram::MatrixMap*, 128> influences;   // +0xc
    mutable unsigned long flags;                        // +0x18 (bits 0..4 primitive, 8 dynamic, 9 dirty, ...)
    unsigned int numPrimitives;                         // +0x1c
    unsigned int numIndices;                            // +0x20
    unsigned int numVertices;                           // +0x24
    unsigned int maxVertices;                           // +0x28
    AutoReferenceArray<unsigned short> indexArray;      // +0x2c
    AutoReferenceArray<FaceNeighbors> faceNeighbors;    // +0x30
    AutoReferenceArray<Vector3> faceNormals;            // +0x34
    unsigned int numFaceNormals;                        // +0x38
    float opacity;                                      // +0x3c
    float surfaceArea;                                  // +0x40
    unsigned long userData;                             // +0x44
    ReferenceCount* referencedUserData;                 // +0x48

    static __declspec(dllimport) HashTable<unsigned int, RegistrationData*, RegistrationDataOp, 13> registrations;

public:
    static __declspec(dllimport) const DPrimitive primitiveLookup[];  // by the primitive bits (flags & 0x1f)

private:
    const GeometryChunk& operator=(const GeometryChunk& o);  // 0x10038c50: nothing
};
JET_LAYOUT32(sizeof(GeometryChunk) == 0x4c);

// A reference-counted block of equal-sized items behind one pointer: {references, count, item size, capacity,
// items}. Copies share the block; Duplicate makes it this array's own.
class GenericReferenceArray {
public:
    struct Block {
        int refs;
        unsigned int count;
        unsigned int itemSize;
        unsigned int capacity;
        unsigned char data[1];
    };

    GenericReferenceArray();
    GenericReferenceArray(unsigned int capacity, unsigned int itemSize);
    GenericReferenceArray(const GenericReferenceArray& o);
    ~GenericReferenceArray();
    void __cdecl operator=(const GenericReferenceArray& o);

    void __cdecl Create(unsigned int capacity, unsigned int itemSize);  // a new block (the old one let go)
    void __cdecl AddReference() const;
    bool __cdecl ReleaseReference() const;  // true when the block went
    int __cdecl GetReferenceCount() const;
    void __cdecl Add();
    void __cdecl Allocate(unsigned int n);
    void __cdecl Remove();
    void __cdecl RemoveAll();
    unsigned int __cdecl Count() const;
    void __cdecl Duplicate(GenericReferenceArray& to) const;
    void __cdecl Duplicate();
    void* __cdecl operator[](unsigned int i);
    const void* __cdecl operator[](unsigned int i) const;
    __cdecl operator bool() const;
    __cdecl operator void*() const;  // the items, null when empty
    unsigned char* __cdecl GetByteOffset(unsigned int offset);
    const unsigned char* __cdecl GetByteOffset(unsigned int offset) const;

    mutable Block* block;
};
JET_LAYOUT32(sizeof(GenericReferenceArray) == 4);

// The chunk of an API module that keeps every vertex item in one stream (0x5c): the static buffers, the vertex
// type and the vertex program. The vertex state bits live in the chunk flags (0x10000 dynamic, 0x20000 dirty,
// 0x40000 unshared).
class GeometryChunkSingleStream : public GeometryChunk {
public:
    GeometryChunkSingleStream();
    GeometryChunkSingleStream(unsigned int vertices, unsigned int primitives, unsigned int indices,
                              unsigned int faceNormals, unsigned int flags);

    void __cdecl RenderGeometry(Display* display, ViewPort* view, const VertexProgram* program, bool bones,
                                int stream) const override;
    void __cdecl RenderPrimitives(Display* display, ViewPort* view, const VertexProgram* program,
                                  unsigned int numIndices, const unsigned short* indices, unsigned int numPrimitives,
                                  DPrimitive primitive, bool bones, int stream) const override;
    bool __cdecl SetupMultipassRender(Display* display, ViewPort* view, const VertexProgram* program,
                                      const unsigned short* indices, unsigned int numIndices) const override;
    bool __cdecl SetupMultipassRender(Display* display, ViewPort* view, const unsigned short* indices,
                                      unsigned int numIndices) const override;
    void __cdecl BindVertexProgram(const VertexProgram* old, const VertexProgram* program,
                                   bool bones) const override;
    void __cdecl BindVertexProgram(int pass, bool bones) const override;  // nothing
    void __cdecl RenderGeometryPass(Display* display, ViewPort* view, int pass) const override;
    void __cdecl RenderGeometryPass(Display* display, ViewPort* view, int count, DPrimitive primitive,
                                    int pass) const override;
    void __cdecl CleanupMultipassRender(Display* display, const VertexProgram* program) const override;
    int __cdecl GetNumPasses() const override;   // 1
    int __cdecl GetNumStreams() const override;  // 1
    void __cdecl SetVertexDataDynamic(int i, int stream, bool on) override;
    bool __cdecl GetVertexDataDynamic(int i, int stream) const override;
    void __cdecl SetVertexDataShared(int i, int stream, bool on) override;
    bool __cdecl GetVertexDataShared(int i, int stream) const override;
    void __cdecl SetVertexDataDirty(bool on, int i, int stream) const override;
    bool __cdecl GetVertexDataDirty(int i, int stream) const override;
    const StaticBuffer* __cdecl GetStaticVertexBuffer(int i, int stream) const override;
    void __cdecl SetStaticVertexBuffer(const StaticBuffer* b, int i, int stream) const override;
    const StaticBuffer* __cdecl GetStaticIndexBuffer(int i) const override;
    void __cdecl SetStaticIndexBuffer(const StaticBuffer* b, int i) const override;
    void __cdecl SetVertexType(VertexType* type, int stream) const override;
    const VertexType& __cdecl GetVertexType(int stream) const override;
    int __cdecl GetItemStream(VertexItemType type, int stream) const override;  // 0
    void __cdecl SetVertexProgram(VertexProgram* program, int stream) override;
    const VertexProgram* __cdecl GetVertexProgram(int stream) const override;

protected:
    ~GeometryChunkSingleStream() override;
    void __cdecl InitInstance(GeometryChunkSingleStream& to) const;
    bool __cdecl SetupMultipassRenderInternal(Display* display, ViewPort* view, const VertexProgram* program,
                                              const unsigned short* indices, unsigned int numIndices) const;

    mutable const StaticBuffer* vertexBuffer;  // +0x4c
    mutable const StaticBuffer* indexBuffer;   // +0x50
    mutable VertexType* vertexType;            // +0x54
    VertexProgram* program;                    // +0x58
};
JET_LAYOUT32(sizeof(GeometryChunkSingleStream) == 0x5c);

// The chunk of an API module that keeps its vertex items in several streams (0x60): a static buffer and state
// bits per stream (1 dynamic, 2 dirty, 4 unshared).
class GeometryChunkMultiStream : public GeometryChunk {
public:
    struct StreamState {  // 8
        const StaticBuffer* buffer;
        unsigned long flags;
    };

    explicit GeometryChunkMultiStream(int streams);
    GeometryChunkMultiStream(unsigned int vertices, unsigned int primitives, unsigned int indices,
                             unsigned int faceNormals, int streams, unsigned int flags);

    void __cdecl RenderGeometry(Display* display, ViewPort* view, const VertexProgram* program, bool bones,
                                int stream) const override;
    void __cdecl RenderPrimitives(Display* display, ViewPort* view, const VertexProgram* program,
                                  unsigned int numIndices, const unsigned short* indices, unsigned int numPrimitives,
                                  DPrimitive primitive, bool bones, int stream) const override;
    bool __cdecl SetupMultipassRender(Display* display, ViewPort* view, const VertexProgram* program,
                                      const unsigned short* indices, unsigned int numIndices) const override;
    bool __cdecl SetupMultipassRender(Display* display, ViewPort* view, const unsigned short* indices,
                                      unsigned int numIndices) const override;
    void __cdecl BindVertexProgram(const VertexProgram* old, const VertexProgram* program,
                                   bool bones) const override;
    void __cdecl BindVertexProgram(int pass, bool bones) const override;  // nothing
    void __cdecl RenderGeometryPass(Display* display, ViewPort* view, int pass) const override;
    void __cdecl RenderGeometryPass(Display* display, ViewPort* view, int count, DPrimitive primitive,
                                    int pass) const override;
    void __cdecl CleanupMultipassRender(Display* display, const VertexProgram* program) const override;
    int __cdecl GetNumPasses() const override;   // 1
    int __cdecl GetNumStreams() const override;
    void __cdecl SetVertexDataDynamic(int i, int stream, bool on) override;  // stream -2: every one
    bool __cdecl GetVertexDataDynamic(int i, int stream) const override;
    void __cdecl SetVertexDataShared(int i, int stream, bool on) override;  // -2 every one, -3 none
    bool __cdecl GetVertexDataShared(int i, int stream) const override;
    void __cdecl SetVertexDataDirty(bool on, int i, int stream) const override;  // -2 every one
    bool __cdecl GetVertexDataDirty(int i, int stream) const override;
    const StaticBuffer* __cdecl GetStaticVertexBuffer(int i, int stream) const override;
    void __cdecl SetStaticVertexBuffer(const StaticBuffer* b, int i, int stream) const override;
    const StaticBuffer* __cdecl GetStaticIndexBuffer(int i) const override;
    void __cdecl SetStaticIndexBuffer(const StaticBuffer* b, int i) const override;
    void __cdecl SetVertexType(VertexType* type, int stream) const override;
    const VertexType& __cdecl GetVertexType(int stream) const override;
    void __cdecl SetVertexProgram(VertexProgram* program, int stream) override;
    const VertexProgram* __cdecl GetVertexProgram(int stream) const override;

    unsigned long __cdecl GetStreamFlags(int stream) const;
    void __cdecl SetStreamFlags(int stream, unsigned long f);

protected:
    ~GeometryChunkMultiStream() override;
    void __cdecl InitInstance(GeometryChunkMultiStream& to) const;  // the bones go across a second time
    void __cdecl SetConstStreamFlags(int stream, unsigned long f) const;
    bool __cdecl SetupMultipassRenderInternal(Display* display, ViewPort* view, const VertexProgram* program,
                                              const unsigned short* indices, unsigned int numIndices) const;

    StreamState* streams;                     // +0x4c (numStreams of them)
    mutable const StaticBuffer* indexBuffer;  // +0x50
    mutable VertexType* vertexType;           // +0x54
    VertexProgram* program;                   // +0x58
    int numStreams;                           // +0x5c
};
JET_LAYOUT32(sizeof(GeometryChunkMultiStream) == 0x60);

// A single stream chunk with its vertices in memory (0x64): a GenericReferenceArray of vertices of the vertex
// type's stride and the item offsets of the vertex type (a shared ItemOffsetTable).
class GenericGeometryChunkSingleStream : public GeometryChunkSingleStream {
public:
    // 0xa8: the stride, then the offset of every item type (-1: not there)
    class ItemOffsetTable : public ReferenceCount {
    public:
        explicit ItemOffsetTable(unsigned int s) : stride(s)
        {
            for (int& o : offsets) o = -1;
        }
        JET_HEAP_OBJECT

        unsigned int stride;  // +8
        int offsets[39];      // +0xc
    };

    GenericGeometryChunkSingleStream(VertexType* type, unsigned int vertices, unsigned int primitives,
                                     unsigned int indices, unsigned int faceNormals, unsigned int flags);

    GeometryChunk* __cdecl CreateInstance() const override;
    GeometryChunk* __cdecl CreateInstance(unsigned int vertices, unsigned int primitives, unsigned int indices,
                                          unsigned int faceNormals) const override;
    bool __cdecl InitializeBuffers(Display* display, bool upload) override;
    bool __cdecl LoadBuffers(Display* display) override;
    void __cdecl DuplicateVertexArray(Display* display, int i, int stream) override;
    void __cdecl ResizeVertexArray(Display* display, unsigned int n, int i, int j) override;
    void* __cdecl GetVertexArray(int i, int stream) override;
    const void* __cdecl GetVertexArray(int i, int stream) const override;
    const VertexType* __cdecl AssignVertexType(Display* display) const override;
    VertexTypeID __cdecl GetVertexTypeID(int stream) const override;
    int __cdecl GetVertexSize(int stream) const override;
    int __cdecl GetItemOffset(VertexItemType type, int stream) const override;
    Vector2Ptr __cdecl GetVector2Array(VertexItemType type, int stream) override;
    ConstVector2Ptr __cdecl GetVector2Array(VertexItemType type, int stream) const override;
    Vector3Ptr __cdecl GetVector3Array(VertexItemType type, int stream) override;
    ConstVector3Ptr __cdecl GetVector3Array(VertexItemType type, int stream) const override;
    Vector4Ptr __cdecl GetVector4Array(VertexItemType type, int stream) override;
    ConstVector4Ptr __cdecl GetVector4Array(VertexItemType type, int stream) const override;
    FloatPtr __cdecl GetFloatArray(VertexItemType type, int stream) override;
    ConstFloatPtr __cdecl GetFloatArray(VertexItemType type, int stream) const override;
    UDwordPtr __cdecl GetUDwordArray(VertexItemType type, int stream) override;
    ConstUDwordPtr __cdecl GetUDwordArray(VertexItemType type, int stream) const override;

protected:
    GenericGeometryChunkSingleStream();
    ~GenericGeometryChunkSingleStream() override;
    void __cdecl InitInstance(GenericGeometryChunkSingleStream& to) const;
    void __cdecl CreateVertexArray(VertexType* type, int vertices);

    GenericReferenceArray vertices;  // +0x5c
    ItemOffsetTable* offsetTable;    // +0x60
};
JET_LAYOUT32(sizeof(GenericGeometryChunkSingleStream) == 0x64);

// A multi stream chunk with its vertices in memory (0x68): one GenericReferenceArray per stream in a shared block
// and the stream and offset of every item type (a shared ItemOffsetTable).
class GenericGeometryChunkMultiStream : public GeometryChunkMultiStream {
public:
    // 0x148: the stride per stream, the vertex size over every stream, then {stream, offset} per item type
    class ItemOffsetTable : public ReferenceCount {
    public:
        struct Entry {
            int stream;
            int offset;
        };
        explicit ItemOffsetTable(unsigned int streamCount);
        ~ItemOffsetTable() override;
        JET_HEAP_OBJECT

        unsigned int* strides;  // +8 (an Alloc block)
        unsigned int size;      // +0xc
        Entry entries[39];      // +0x10
    };
    // the arrays of the streams: {references, count, two words left as the heap had them, the arrays}
    struct Arrays {
        int refs;
        unsigned int count;
        int unused[2];
        GenericReferenceArray data[1];
    };

    GenericGeometryChunkMultiStream(VertexType* type, unsigned int vertices, unsigned int primitives,
                                    unsigned int indices, unsigned int faceNormals, unsigned int flags);

    GeometryChunk* __cdecl CreateInstance() const override;
    GeometryChunk* __cdecl CreateInstance(unsigned int vertices, unsigned int primitives, unsigned int indices,
                                          unsigned int faceNormals) const override;
    bool __cdecl InitializeBuffers(Display* display, bool upload) override;
    bool __cdecl LoadBuffers(Display* display) override;
    void __cdecl DuplicateVertexArray(Display* display, int i, int stream) override;  // stream -2: every one
    void* __cdecl GetVertexArray(int i, int stream) override;
    const void* __cdecl GetVertexArray(int i, int stream) const override;
    const VertexType* __cdecl AssignVertexType(Display* display) const override;
    VertexTypeID __cdecl GetVertexTypeID(int stream) const override;
    int __cdecl GetVertexSize(int stream) const override;
    int __cdecl GetItemOffset(VertexItemType type, int stream) const override;
    int __cdecl GetItemStream(VertexItemType type, int stream) const override;
    Vector2Ptr __cdecl GetVector2Array(VertexItemType type, int stream) override;
    ConstVector2Ptr __cdecl GetVector2Array(VertexItemType type, int stream) const override;
    Vector3Ptr __cdecl GetVector3Array(VertexItemType type, int stream) override;
    ConstVector3Ptr __cdecl GetVector3Array(VertexItemType type, int stream) const override;
    Vector4Ptr __cdecl GetVector4Array(VertexItemType type, int stream) override;
    ConstVector4Ptr __cdecl GetVector4Array(VertexItemType type, int stream) const override;
    FloatPtr __cdecl GetFloatArray(VertexItemType type, int stream) override;
    ConstFloatPtr __cdecl GetFloatArray(VertexItemType type, int stream) const override;
    UDwordPtr __cdecl GetUDwordArray(VertexItemType type, int stream) override;
    ConstUDwordPtr __cdecl GetUDwordArray(VertexItemType type, int stream) const override;

protected:
    explicit GenericGeometryChunkMultiStream(int streams);
    ~GenericGeometryChunkMultiStream() override;
    void __cdecl InitInstance(GenericGeometryChunkMultiStream& to) const;
    void __cdecl CreateVertexArray(VertexType* type, int vertices);

    Arrays* arrays;                // +0x60
    ItemOffsetTable* offsetTable;  // +0x64
};
JET_LAYOUT32(sizeof(GenericGeometryChunkMultiStream) == 0x68);

}  // namespace Jet
