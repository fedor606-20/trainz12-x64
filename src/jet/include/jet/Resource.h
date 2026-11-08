// The resource system of KernelResource_7c: Resource, RedirResource, ResourceVariant, ResourceInstance,
// ResourceTypeBase (and the ResourceType<T> factory template the modules inline), ResourceDatabase (+Find,
// ResourceNamelessDatabase), the bins (ResourceBin, ResourceBinCache, ResourceBinInstant) and ResourceSystem.
// Layouts, vtables, access and mangled names follow TS12 (notes/KernelResource.md).
//
// Most members are __cdecl (mangled QBA/QAA/UBA...), including the virtual ones: `this` goes on the stack. The
// internal classes (bins, the system's task and loader) use __thiscall. Keep virtual declarations in vtable order.
#pragma once

#include <jet/DynamicArray.h>
#include <jet/FastDynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/Plugin.h>
#include <jet/Result.h>

namespace Jet {

class Resource;
class ResourceDatabase;
class ResourceSystem;
class ResourceTypeBase;
class String;

// told when a resource has loaded (Resource::OnLoadNotify, or at once by AddObserver when it already has)
class ResourceObserver {
public:
    virtual void __cdecl OnLoad(const Resource* resource) const = 0;  // 0
    virtual ~ResourceObserver() {}                                    // 1 (AsciiTextureResource's thunk, U6b)
};

class Resource {
public:
    enum Results {};
    // None, Ready, Cached (queued for the loader), Loading, Loaded, Locking, Available, Inactive (unused, in the
    // bin), Invalid, Delete
    enum ResStatus {
        STATUS_NONE = 0,
        STATUS_READY = 1,
        STATUS_CACHED = 2,
        STATUS_LOADING = 3,
        STATUS_LOADED = 4,
        STATUS_LOCKING = 5,
        STATUS_AVAILABLE = 6,
        STATUS_INACTIVE = 7,
        STATUS_INVALID = 8,
        STATUS_DELETE = 9,
    };
    static const ResultTable* const results;
    using Result = ResultClass<&results, Results>;

    Resource(const PString& name, const ResourceDatabase* owner);
    Resource(const Resource& o);
    Resource& operator=(const Resource& o);

    // vtable slots 0-7
    virtual Result __cdecl Lock() const;
    virtual void __cdecl Unlock() const;
    virtual bool __cdecl LoadAll() const;
    virtual Result __cdecl IsAllLoaded() const;
    virtual void __cdecl SetPriority(float priority) const;
    virtual void __cdecl AddObserver(const ResourceObserver* o) const;
    virtual void __cdecl RemoveObserver(const ResourceObserver* o) const;
    virtual bool __cdecl GetMustResetWhenFinished() const;

    bool __cdecl operator==(const PString& name) const;
    bool __cdecl operator!=(const PString& name) const;
    void __cdecl AddReference() const;
    bool __cdecl ReleaseReference() const;
    void __cdecl Cache(bool force) const;
    void __cdecl ConvertResourceName(String& name) const;
    bool __cdecl DebugGetObserversNotified() const;
    const ResourceDatabase* __cdecl GetDB() const;
    unsigned int __cdecl GetDataSize() const;
    int __cdecl GetHitCount() const;
    const PString& __cdecl GetName() const;
    const ResourceDatabase* __cdecl GetOrig() const;
    const ResourceDatabase* __cdecl GetOwner() const;
    const PString& __cdecl GetOwnerName() const;
    float __cdecl GetPriority() const;
    int __cdecl GetReferenceCount() const;
    const String& __cdecl GetScrambleKey() const;
    unsigned int __cdecl GetSize() const;
    ResStatus __cdecl GetStatus() const;
    bool __cdecl IsAvailable() const;
    bool __cdecl IsLocked() const;
    void __cdecl SetScrambleKey(const String& key) const;
    void __cdecl Touch() const;

    // what the other modules inline
    void InlineAddReference() const { ++refs; }
    const String& ScrambleKey() const { return scrambleKey; }
    const ResourceDatabase* Owner() const { return owner; }
    int Status() const { return status; }

    JET_HEAP_OBJECT

protected:
    virtual ~Resource();                                                          // 8
    virtual const Resource* __cdecl CreateInstance(const PString& name) const;   // 9
    virtual Result __cdecl Load(const char* name);                               // 10
    virtual Result __cdecl LoadTxt(const char* name);                            // 11
    virtual Result __cdecl LoadComplete();                                       // 12
    virtual void __cdecl OnLoadNotify() const;                                   // 13
    virtual bool __cdecl Unload();                                               // 14
    virtual unsigned int __cdecl CalculateSize() const;                          // 15
    virtual unsigned int __cdecl CalculateDataSize() const;                      // 16

    int __cdecl GetInternalReferenceCount() const;
    void __cdecl InternalAddReference() const;
    bool __cdecl InternalReleaseReference() const;
    void __cdecl SetLoadName(const PString& name) const;
    void __cdecl SetOrig(const ResourceDatabase* db) const;
    void __cdecl SetRedir(bool on);

private:
    friend class RedirResource;
    friend class ResourceDatabase;
    friend class ResourceSystem;
    friend class ResourceBin;
    friend class ResourceBinCache;
    friend class ResourceBinInstant;
    friend class ResourceLoader;
    friend class ResourceTypeBase;
    friend class AsciiTextureResource;  // RenderSystem U6b reads the sub-resources' status and flags inline

    virtual const Resource* __cdecl GetRedirection() const;  // 17

    void __cdecl AddRedir(const Resource* redir) const;
    void __cdecl AttachType(const ResourceTypeBase* type) const;
    void __cdecl DeleteThis() const;
    bool __cdecl FinalizeLoad() const;
    float __cdecl GetCreationTime() const;
    float __cdecl GetHeapPriority() const;
    const ResourceTypeBase* __cdecl GetType() const;
    void __cdecl Inactive() const;
    bool __cdecl IsAliased() const;
    bool __cdecl IsInvalid() const;
    bool __cdecl IsRedir() const;
    bool __cdecl IsTxt() const;
    void __cdecl PreLoad() const;
    void __cdecl RemoveRedir(const Resource* redir) const;
    void __cdecl RemoveRedirs() const;
    void __cdecl ResetToReady() const;
    void __cdecl SetAliased(bool on) const;
    void __cdecl SetAvailableOnLoad() const;
    void __cdecl SetInvalid(bool on) const;
    void SetStatus(ResStatus s) const;  // __thiscall
    void __cdecl SetTxt(bool on) const;
    void __cdecl SetType(const ResourceTypeBase* type) const;
    bool __cdecl TemplateLoad(const char* name) const;
    void __cdecl UpdateHeapPriority(float time) const;
    static const char* __cdecl GetStatusString(ResStatus s);
    static void __cdecl InitRS(ResourceSystem* rs);
    static void __cdecl LogStats();
    // the cached sizes (GetSize / GetDataSize inlined by the original)
    unsigned int CachedSize() const;
    unsigned int CachedDataSize() const;

    using ObserverList = FastDynamicArray<const ResourceObserver*, 13>;
    using RedirList = FastDynamicArray<const Resource*, 13>;

    PString name;                                // +0x04
    mutable const ResourceDatabase* owner;       // +0x08 (ResourceDatabase::Remove clears it)
    mutable const ResourceDatabase* orig;        // +0x0c the database it was last asked for through
    float creationTime;                          // +0x10
    mutable float priority;                      // +0x14
    mutable float heapPriority;                  // +0x18
    mutable int refs;                            // +0x1c
    mutable float lastUse;                       // +0x20 the system's time when last used
    mutable int internalRefs;                    // +0x24
    mutable int status;                          // +0x28 ResStatus
    mutable ObserverList observers;              // +0x2c
    mutable bool observersNotified;              // +0x30
    mutable const ResourceTypeBase* type;        // +0x34 holds a reference
    mutable PString loadName;                    // +0x38
    mutable String scrambleKey;                  // +0x3c
    mutable int hits;                            // +0x40
    mutable int misses;                          // +0x44
    mutable unsigned int locks;                  // +0x48
    mutable RedirList redirs;                    // +0x4c redirections to this one
    mutable unsigned int size;                   // +0x50 cached CalculateSize, -1 until asked
    mutable unsigned int dataSize;               // +0x54 cached CalculateDataSize
    // +0x58: 1 counted in the system's loaded totals, 2 aliased, 4 a .txt resource, 8 a redirection
    mutable unsigned char flags;

    static unsigned int totalResources;
    static unsigned int totalActive;
    static unsigned int totalLoaded;
    static unsigned int totalHits;
    static unsigned int totalMisses;
    static unsigned int totalClashes;
    static ResourceSystem* resourceSystem;
};
JET_LAYOUT32(sizeof(Resource) == 0x5c);

// a name in one database standing for a resource of another
class RedirResource : public Resource {
public:
    RedirResource(const PString& name, const ResourceDatabase* owner, const Resource* target);
    RedirResource(const RedirResource& o);
    ~RedirResource() override;
    RedirResource& operator=(const RedirResource& o);
    const Resource* __cdecl GetRedirection() const override;

private:
    const Resource* target;  // +0x5c
};
JET_LAYOUT32(sizeof(RedirResource) == 0x60);

// a counted handle to a resource: {resource, the database it came from, locked}
class ResourceInstance {
public:
    using Result = Resource::Result;

    ResourceInstance();
    ResourceInstance(const ResourceInstance& o);
    ~ResourceInstance();
    ResourceInstance& __cdecl operator=(const ResourceInstance& o);

    void __cdecl Init(const ResourceDatabase* db, const PString& name);
    void __cdecl Init(const ResourceDatabase* db, const PString& name, bool cache);
    void __cdecl Reset();
    Result __cdecl Lock() const;
    void __cdecl Unlock() const;
    Result __cdecl IsAllLoaded() const;
    bool __cdecl IsAvailable() const;
    PString __cdecl GetName() const;
    __cdecl operator bool() const;
    bool __cdecl operator!() const;
    bool __cdecl operator==(const ResourceInstance& o) const;
    bool __cdecl operator!=(const ResourceInstance& o) const;
    bool __cdecl operator==(const PString& name) const;
    bool __cdecl operator!=(const PString& name) const;

protected:
    void __cdecl Inc() const;
    void __cdecl Dec() const;

    const Resource* resource;      // +0x00
    const ResourceDatabase* orig;  // +0x04
    mutable unsigned char flags;   // +0x08 bit 0 locked
};
JET_LAYOUT32(sizeof(ResourceInstance) == 0xc);

// a variant name of a database (a list node: {next, prev, name})
class ResourceVariant {
public:
    ResourceVariant(const PString& name);
    ResourceVariant(const ResourceVariant& o);
    ~ResourceVariant();
    ResourceVariant& operator=(const ResourceVariant& o);
    const PString& __cdecl GetName() const;
    JET_HEAP_OBJECT

    ResourceVariant* next;  // +0x00
    ResourceVariant* prev;  // +0x04

private:
    PString name;           // +0x08
};
JET_LAYOUT32(sizeof(ResourceVariant) == 0xc);

// a resource factory: a plugin type keyed by file extension
class ResourceTypeBase : public PluginType {
public:
    ResourceTypeBase(const PString& extension, const UId& id, const UId& parentId);
    ResourceTypeBase(const ResourceTypeBase& o);
    ResourceTypeBase& operator=(const ResourceTypeBase& o);
    ~ResourceTypeBase() override;                                                    // slot 2
    virtual Resource* __cdecl Create(const PString& name, const ResourceDatabase* owner) const = 0;  // 3
    virtual unsigned int __cdecl GetResourceSize() const = 0;                        // 4
    virtual const Resource* __cdecl CreateOnTheFly(const PString& name, const ResourceDatabase* owner) const;  // 5

protected:
    void __cdecl Init(Resource* resource) const;
};
JET_LAYOUT32(sizeof(ResourceTypeBase) == 0x20);

// the factory each resource class registers (inlined into the modules that define resources)
template <class T>
class ResourceType : public ResourceTypeBase {
public:
    ResourceType(const PString& extension, const UId& id, const UId& parentId)
        : ResourceTypeBase(extension, id, parentId)
    {
    }
    ~ResourceType() override {}
    Resource* __cdecl Create(const PString& name, const ResourceDatabase* owner) const override
    {
        return new T(name, owner);
    }
    unsigned int __cdecl GetResourceSize() const override { return sizeof(T); }
};

// a named set of resources: a folder (or a .ja archive mounted as one), searched with its variants and parents
class ResourceDatabase {
public:
    // a directory walk over a database (ResourceDatabase::GetSearchHandle)
    class SearchHandle {
    public:
        virtual ~SearchHandle() {}
        virtual bool __cdecl Open(const char* pattern, ResourceDatabase* db);  // false
        virtual void __cdecl Close();
        virtual bool __cdecl Next();
        virtual const File::Entry* __cdecl GetEntry();
        JET_HEAP_OBJECT
    };
    class Find {
    public:
        Find();
        Find(const Find& o);
        ~Find();
        Find& operator=(const Find& o);
        bool __cdecl Open(const char* pattern, ResourceDatabase* db);
        bool __cdecl Next();
        void __cdecl Close();
        const File::Entry& __cdecl GetEntry();

    private:
        File::Entry entry;     // +0x000
        SearchHandle* handle;  // +0x120
    };

    ResourceDatabase(const PString& name, const PString& path);
    virtual ~ResourceDatabase();                                                   // 0
    virtual const Resource* __cdecl Get(const PString& name, int flags) const;     // 1
    virtual bool __cdecl Exists(const PString& name) const;                        // 2
    virtual PString __cdecl GetFileName(const PString& name) const;                // 3

    void __cdecl AddParent(const PString& name);
    void __cdecl AddParent(ResourceDatabase* db);
    void __cdecl AddVariant(const PString& name);
    void DumpUnusedResourcesFromCache();  // __thiscall
    static ResourceDatabase* __cdecl FindDatabase(const PString& name);
    static ResourceSystem* __cdecl GetMainResourceSystem();
    const PString& __cdecl GetName() const;
    const Options* __cdecl GetOptions() const;
    void __cdecl GetParentResourceDatabases(DynamicArray<PString, 13, Less<PString, PString>>& out);
    void __cdecl Lock() const;
    void __cdecl Unlock() const;
    void __cdecl Remove(const PString& name) const;
    void __cdecl RemoveParent(ResourceDatabase* db);
    void __cdecl SetWarnings(bool on) const;
    bool __cdecl operator==(const PString& name) const;
    JET_HEAP_OBJECT

protected:
    ResourceDatabase(const PString& name);
    virtual SearchHandle* __cdecl GetSearchHandle();                              // 4

private:
    friend class Resource;
    friend class ResourceSystem;
    friend class ResourceBin;
    friend class ResourceBinCache;
    friend class ResourceBinInstant;
    friend class ResourceNamelessDatabase;

    // the resources by name node: 401 buckets of FastDynamicArray<Resource*, 13>
    struct ResourceTable {
        using Bucket = FastDynamicArray<const Resource*, 13>;
        // {table, bucket, the element, the bucket's first chunk, the element's chunk}
        struct Iterator {
            explicit Iterator(ResourceTable& t) : table(&t), bucket(0), node(nullptr), head(nullptr), chunk(nullptr) {}
            void Begin(unsigned long from);  // 0x10005cb0
            void Next();                     // 0x10005850
            ResourceTable* table;
            unsigned long bucket;
            const Resource** node;
            Bucket::Chunk* head;
            Bucket::Chunk* chunk;
        };
        ResourceTable();   // 0x100058d0
        ~ResourceTable();  // 0x10005990
        Iterator Find(const PStringCache::Node* name);  // 0x10005b60
        void Insert(const Resource* r);                  // 0x100059c0
        Iterator Remove(const Iterator& i);              // 0x10005a30
        void Clear();                                    // 0x10005af0
        Bucket* buckets;            // +0x00
        unsigned long count;        // +0x04
        unsigned long bucketCount;  // +0x08 401
    };
    // 0x1000a52c: a File::Find over the database's drive (0x444 bytes)
    class RDBSearchHandle : public SearchHandle {
    public:
        bool __cdecl Open(const char* pattern, ResourceDatabase* db) override;
        void __cdecl Close() override;
        bool __cdecl Next() override;
        const File::Entry* __cdecl GetEntry() override;

    private:
        File::Find find;  // +0x04
    };
    struct VariantList {
        ResourceVariant* next;
        ResourceVariant* prev;
        unsigned long count;
        ResourceVariant* Head() { return reinterpret_cast<ResourceVariant*>(this); }
    };
    using ParentList = FastDynamicArray<ResourceDatabase*, 5>;

    void __cdecl Dump() const;
    const Resource* __cdecl CreateInvalidResource(const PString& name, const PString& loadName) const;
    const Resource* __cdecl CreateResource(const PString& name, const PString& loadName, bool txt) const;
    void __cdecl CreateResource(const Resource* target) const;
    const Resource* __cdecl GetFromFile(const PString& name) const;
    const Resource* __cdecl GetFromOnTheFly(const PString& name) const;
    const Resource* __cdecl GetFromParents(const PString& name) const;
    const Resource* __cdecl GetFromPath(const PString& name, const PString& variant) const;
    const Resource* __cdecl GetFromThis(const PString& name) const;
    const Resource* __cdecl GetOneFromThis(const PString& name) const;
    void __cdecl Inactive(const Resource* r) const;
    void __cdecl Invalidate(ResourceDatabase* db);
    void __cdecl ProcessLRU(int& excess) const;
    void __cdecl Remove(const Resource* r) const;
    static void __cdecl InitRS(ResourceSystem* rs);
    static int __cdecl OrderResourceLRU(const void* a, const void* b);
    void Construct(const PString& name);

    void* unknown04;                     // +0x04
    void* unknown08;                     // +0x08
    PString name;                        // +0x0c
    mutable ResourceTable resources;     // +0x10
    ParentList parents;                  // +0x1c
    VariantList variants;                // +0x20
    mutable int lookups;                 // +0x2c GetOneFromThis calls
    mutable int requests;                // +0x30 Get calls not found here
    mutable int invalidRequests;         // +0x34
    mutable unsigned char flags;         // +0x38 bit 0 warnings
    CriticalSection lock;                // +0x3c

    static ResourceSystem* resourceSystem;
};
JET_LAYOUT32(sizeof(ResourceDatabase) == 0x48);

// a database whose resources sit directly in its folder (no name of its own in the paths)
class ResourceNamelessDatabase : public ResourceDatabase {
public:
    ResourceNamelessDatabase(const PString& path, const PString& name);  // (the folder first)
    ~ResourceNamelessDatabase() override;
};

// where unused resources wait to be unloaded (the system's bin). __thiscall virtuals.
class ResourceBin {
public:
    virtual ~ResourceBin() {}                                          // 0
    virtual void Init(const Options* options) {}                       // 1 (0x100085a0: nothing)
    virtual bool Reuse(const Resource* r);                             // 2 back to available
    virtual void Remove(const Resource* r);                            // 3 leaves the bin
    virtual void Add(const Resource* r);                               // 4 now unused
    virtual void Update(float dt) {}                                   // 5
    virtual void Dump() {}                                             // 6
    JET_HEAP_OBJECT
};

// unloads the resources that stayed unused longest, a few per pass
class ResourceBinCache : public ResourceBin {
public:
    ResourceBinCache();  // 0x10006a40
    ~ResourceBinCache() override;
    void Init(const Options* options) override;
    bool Reuse(const Resource* r) override;
    void Remove(const Resource* r) override;
    void Add(const Resource* r) override;
    void Update(float dt) override;
    void Dump() override;
    void DumpAll();  // 0x100035a0 (DumpUnusedResourcesFromCache)

private:
    void Push(const Resource* r);                // 0x10003740
    static void Evict(const Resource* r);        // 0x10002f70

    CriticalSection lock;          // +0x04
    unsigned long count;           // +0x10 Array<const Resource*>: count, capacity, data
    unsigned long capacity;        // +0x14
    const Resource** data;         // +0x18
    float time;                    // +0x1c
    float lastPass;                // +0x20
    float processTime;             // +0x24 "ResourceBinProcessTime" (0.5)
    float oldest;                  // +0x28 "ResourceBinOldest" (10)
    float spread;                  // +0x2c (2)
    float minAge;                  // +0x30 (5)
    float deleteRatio;             // +0x34 (0.1)
    unsigned int maxDeleted;       // +0x38 "ResourceBinMaxDeleted" (5)
    LCRand rand;                   // +0x3c
};
JET_LAYOUT32(sizeof(ResourceBinCache) == 0x40);

// unloads at once
class ResourceBinInstant : public ResourceBin {
public:
    void Init(const Options*) override {}
    bool Reuse(const Resource*) override { return false; }
    void Add(const Resource* r) override;
    void Update(float) override {}

    void* unused04 = nullptr;  // +0x04
};
JET_LAYOUT32(sizeof(ResourceBinInstant) == 8);

class ResourceLoader;
class ResourceSystemTask;

class ResourceSystem {
public:
    ResourceSystem();
    ~ResourceSystem();
    void __cdecl Init(const Options* options);
    void __cdecl InitBin(ResourceBin* bin);
    ResourceDatabase* __cdecl FindDatabase(const PString& name);
    void __cdecl Dump();
    void DumpUnusedResourcesFromCache();  // __thiscall
    const Options* __cdecl GetOptions() const;
    int __cdecl GetQueuedResourceCount();
    void __cdecl PauseProcessing();
    void __cdecl ResumeProcessing();
    void __cdecl Update(float dt);

private:
    friend class Resource;
    friend class ResourceDatabase;
    friend class ResourceBin;
    friend class ResourceBinCache;
    friend class ResourceBinInstant;
    friend class ResourceLoader;

    // a resource being loaded on the main thread, with a lock others wait on
    struct Slot {
        const Resource* resource;  // +0x00
        CriticalSection lock;      // +0x04
        bool used;                 // +0x10
    };

    void ProcessLoaded();                                      // 0x100072a0
    void LoaderClaim(const Resource* r);                       // 0x10008310 on the loader
    void LoaderQueue(const Resource* r);                       // 0x10008090 on the loader
    Mutex& LoaderMutex();                                      // the loader's +0x14
    void AddDatabase(ResourceDatabase* db);                    // 0x10007a60
    void RemoveDatabase(ResourceDatabase* db);                 // 0x10006c00
    ResourceDatabase** FindDatabaseSlot(const PString& name);  // 0x10007b60
    void Unqueue(const Resource* r);                           // 0x10007720
    void Unbin(const Resource* r);                             // 0x100077b0 (from the loaded queue)
    void CheckSize();                                          // 0x10007880
    void DumpUnused();                                         // 0x10007970
    static void QueueLoaded(const Resource* r);                // 0x100074a0
    static void AcquireSlot(const Resource* r);                // 0x10007530
    static void ReleaseSlot(const Resource* r);                // 0x10007610
    static const Resource* CreateFromType(const PString& name, const ResourceDatabase* owner, bool txt);  // 0x10006cf0
    static const Resource* CreateOnTheFly(const PString& name, const ResourceDatabase* owner);           // 0x10006f10

    FastDynamicArray<ResourceDatabase*, 0x1d> databases;  // +0x00
    const Options* options;                               // +0x04
    ResourceBin* bin;                                     // +0x08
    ResourceSystemTask* task;                             // +0x0c
    unsigned char flags;                                  // +0x10 bit 0 caching (a loader thread)
    ResourceLoader* loader;                               // +0x14
    FastDynamicArray<const Resource*, 13> loaded;         // +0x18 loaded by the thread, to be completed
    CriticalSection loadedLock;                           // +0x1c "LoadComplete"
    Slot slots[8];                                        // +0x28
    CriticalSection slotsLock;                            // +0xc8
    Semaphore slotsFree;                                  // +0xd4 (8)
    float time;                                           // +0xdc seconds of updates
    unsigned int resources;                               // +0xe0
    unsigned int loadedResources;                         // +0xe4
    unsigned int binned;                                  // +0xe8
    unsigned int size;                                    // +0xec
    unsigned int dataSize;                                // +0xf0
    unsigned int binSize;                                 // +0xf4
    unsigned int maxSize;                                 // +0xf8 (30,000,000)
    int paused;                                           // +0xfc
    bool dumpUnused;                                      // +0x100
};
JET_LAYOUT32(sizeof(ResourceSystem) == 0x104);

}  // namespace Jet
