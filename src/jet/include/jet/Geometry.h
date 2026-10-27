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

}  // namespace Jet
