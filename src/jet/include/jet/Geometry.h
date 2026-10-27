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
struct GeometryStreamType;

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
// 0); the stream chunks (U5c) fill them. The interpolation and copy slots (61..70) are U5b2 and call the original
// for now.
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
    using UDwordPtr = VertexItemPtr<unsigned long, VertexItemPtr<const unsigned long, VIPConstHelper>>;
    using ConstUDwordPtr = VertexItemPtr<const unsigned long, VIPConstHelper>;
    using ItemTypes = FastDynamicArray<VertexItemType, 13, Less<VertexItemType, VertexItemType>>;

    GeometryChunk();
    GeometryChunk(unsigned int vertices, unsigned int primitives, unsigned int indices, unsigned int faceNormals,
                  unsigned int flags);

    virtual int __cdecl Slot2() = 0;
    virtual int __cdecl Slot3() = 0;
    virtual bool __cdecl InitializeBuffers(Display* display, bool b) = 0;  // 4 (its body answers true)
    virtual int __cdecl Slot5() = 0;
    virtual void __cdecl DuplicateUnsharedArrays(Display* display);  // 6: every vertex array not shared
    virtual int __cdecl Slot7() = 0;                                 // DuplicateVertexArray(display, i, j)
    virtual void __cdecl DuplicateIndexArray(Display* display);
    virtual void __cdecl ResizeVertexArray(Display* display, unsigned int n, int i, int j);  // 9: nothing
    virtual void __cdecl ResizeIndexArray(Display* display, unsigned int n);
    virtual void __cdecl AppendChunk(Display* display, const GeometryChunk& o);  // 11
    virtual int __cdecl Slot12() = 0;
    virtual int __cdecl Slot13() = 0;
    virtual int __cdecl Slot14() = 0;
    virtual int __cdecl Slot15() = 0;
    virtual int __cdecl Slot16() = 0;
    virtual int __cdecl Slot17() = 0;
    virtual int __cdecl Slot18() = 0;
    virtual int __cdecl Slot19() = 0;
    virtual int __cdecl Slot20() = 0;
    virtual int __cdecl Slot21() = 0;
    virtual int __cdecl Slot22() = 0;
    virtual int __cdecl Slot23() = 0;
    virtual int __cdecl Slot24() = 0;
    virtual int __cdecl Slot25() = 0;
    virtual int __cdecl Slot26() = 0;
    virtual int __cdecl Slot27() = 0;
    virtual int __cdecl Slot28() = 0;
    virtual void __cdecl SetIndexDataDynamic();        // 29
    virtual bool __cdecl GetIndexDataDynamic() const;  // 30
    virtual int __cdecl Slot31() = 0;
    virtual int __cdecl Slot32() = 0;
    virtual void __cdecl SetIndexDataDirty(bool on) const;  // 33
    virtual bool __cdecl GetIndexDataDirty() const;         // 34
    virtual int __cdecl Slot35() = 0;
    virtual int __cdecl Slot36() = 0;
    virtual int __cdecl Slot37() = 0;
    virtual int __cdecl Slot38() = 0;
    virtual int __cdecl Slot39() = 0;
    virtual int __cdecl Slot40() = 0;
    virtual int __cdecl Slot41() = 0;
    virtual int __cdecl Slot42() = 0;
    virtual int __cdecl Slot43() = 0;
    virtual int __cdecl Slot44() = 0;
    virtual int __cdecl Slot45() = 0;
    virtual int __cdecl Slot46() = 0;
    virtual int __cdecl Slot47() = 0;
    virtual int __cdecl Slot48() = 0;
    virtual int __cdecl Slot49() = 0;
    virtual int __cdecl Slot50() = 0;
    virtual int __cdecl Slot51() = 0;
    virtual int __cdecl Slot52() = 0;
    virtual int __cdecl Slot53() = 0;
    virtual ConstUDwordPtr __cdecl GetUDwordArray(VertexItemType type, int index) const;  // 54/55: {4, null}
    virtual UDwordPtr __cdecl GetUDwordArray(VertexItemType type, int index);
    virtual void __cdecl UpdateLighting(Display* display, const DisplayLight* light, Material* material,
                                        const Matrix4& a, const Matrix4& b);  // 56
    virtual void __cdecl ResetToAmbient(Display* display, const Material* material, const Color& ambient);
    virtual void __cdecl RenderDebug(Display* display) const;  // 58
    virtual int __cdecl Slot59() = 0;
    virtual int __cdecl Slot60() = 0;
    // 61..70: InterpolateChunkData, the eight InterpolateAllChunkData, CopyChunkData (U5b2: the original's)
    virtual void __cdecl Interpolate61(const GeometryChunk*, unsigned int, unsigned int, unsigned int,
                                       const Vector3&, const ItemTypes&, int);
    virtual void __cdecl Interpolate62(const GeometryChunk*, unsigned int, unsigned int, unsigned int,
                                       const Vector3* const, unsigned int, unsigned int, const ItemTypes&, int);
    virtual void __cdecl Interpolate63(const GeometryChunk*, unsigned int, unsigned int, unsigned int,
                                       const Vector3* const, const ItemTypes&, int);
    virtual void __cdecl Interpolate64(const GeometryChunk*, unsigned int, unsigned int, unsigned int,
                                       const ItemTypes&, int);
    virtual void __cdecl Interpolate65(const GeometryChunk*, unsigned int, unsigned int, const Vector2* const,
                                       unsigned int, unsigned int, const ItemTypes&, int);
    virtual void __cdecl Interpolate66(const GeometryChunk*, unsigned int, unsigned int, const Vector2* const,
                                       const ItemTypes&, int);
    virtual void __cdecl Interpolate67(const GeometryChunk*, unsigned int, unsigned int, const ItemTypes&, int);
    virtual void __cdecl Interpolate68(const GeometryChunk*, unsigned int, const float* const, unsigned int,
                                       unsigned int, const ItemTypes&, int);
    virtual void __cdecl Interpolate69(const GeometryChunk*, unsigned int, const float* const, const ItemTypes&,
                                       int);
    virtual void __cdecl Copy70(const GeometryChunk*, unsigned int, unsigned int, unsigned int, const ItemTypes&,
                                int);
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

private:
    const GeometryChunk& operator=(const GeometryChunk& o);  // 0x10038c50: nothing
};
JET_LAYOUT32(sizeof(GeometryChunk) == 0x4c);

}  // namespace Jet
