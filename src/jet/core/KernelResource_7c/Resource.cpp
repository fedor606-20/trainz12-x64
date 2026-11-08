// KernelResource_7c: Resource (0x10001130..0x10002bbd), RedirResource, ResourceVariant, ResourceInstance
// (0x10005fd0..0x1000639d), ResourceTypeBase and the bins (0x10002e00..0x10003810). Transcribed from the assembly;
// notes/KernelResource.md has the layouts.
#include <jet/Resource.h>

#include <cstdlib>
#include <cstring>
#include <new>

namespace Jet {

// the original's table is only the text "UNKNOWN"
struct ResultTable {
    char text[8];
};
static const ResultTable kResourceResults = {"UNKNOWN"};
const ResultTable* const Resource::results = &kResourceResults;

unsigned int Resource::totalResources = 0;
unsigned int Resource::totalActive = 0;
unsigned int Resource::totalLoaded = 0;
unsigned int Resource::totalHits = 0;
unsigned int Resource::totalMisses = 0;
unsigned int Resource::totalClashes = 0;
ResourceSystem* Resource::resourceSystem = nullptr;

namespace {

const char* const kStatusNames[] = {"None", "Ready", "Cached", "Loading", "Loaded", "Locking", "Available",
                                    "Inactive", "Invalid", "Delete"};

// what the original does for `pstring = other`: drop the old node, then take the new one
void Assign(PString& to, const PString& from)
{
    to.~PString();
    new (&to) PString(from);
}

}  // namespace

// ---- construction
Resource::Resource(const PString& n, const ResourceDatabase* o)  // 0x10001ef0
    : name(n), owner(o), orig(nullptr), creationTime(static_cast<float>(Time::GetSeconds())), priority(0.0f),
      heapPriority(0.0f), refs(0), lastUse(resourceSystem->time), internalRefs(1), observersNotified(false),
      type(nullptr), loadName(n), hits(0), misses(0), locks(0), size(~0u), dataSize(~0u)
{
    flags &= 0xf0;
    status = o ? STATUS_NONE : STATUS_READY;
    ++totalResources;
}

// 0x10001130: a field copy; the type is copied without a reference, flag bits 0..3 one by one
Resource::Resource(const Resource& o)
    : name(o.name), owner(o.owner), orig(o.orig), creationTime(o.creationTime), priority(o.priority),
      heapPriority(o.heapPriority), refs(o.refs), lastUse(o.lastUse), internalRefs(o.internalRefs), status(o.status),
      observersNotified(o.observersNotified), type(o.type), loadName(o.loadName), scrambleKey(o.scrambleKey),
      hits(o.hits), misses(o.misses), locks(o.locks), size(o.size), dataSize(o.dataSize)
{
    observers.AppendChunks(o.observers);
    redirs.AppendChunks(o.redirs);
    flags = static_cast<unsigned char>((flags & ~0xf) | (o.flags & 0xf));
}

Resource& Resource::operator=(const Resource& o)  // 0x100012c0 (no self check)
{
    Assign(name, o.name);
    owner = o.owner;
    orig = o.orig;
    creationTime = o.creationTime;
    priority = o.priority;
    heapPriority = o.heapPriority;
    refs = o.refs;
    lastUse = o.lastUse;
    internalRefs = o.internalRefs;
    status = o.status;
    observers.RemoveAll();
    observers.AppendChunks(o.observers);
    observersNotified = o.observersNotified;
    type = o.type;
    Assign(loadName, o.loadName);
    scrambleKey = o.scrambleKey;
    hits = o.hits;
    misses = o.misses;
    locks = o.locks;
    redirs.RemoveAll();
    redirs.AppendChunks(o.redirs);
    size = o.size;
    dataSize = o.dataSize;
    flags = static_cast<unsigned char>((flags & ~0xf) | (o.flags & 0xf));
    return *this;
}

Resource::~Resource()  // 0x10002030 (the members go in reverse order: redirs, key, load name, observers, name)
{
    --totalResources;
    if (status == STATUS_AVAILABLE || status == STATUS_INACTIVE) --totalLoaded;
    if (status == STATUS_AVAILABLE) --totalActive;
}

// ---- accessors
bool Resource::IsAvailable() const { return status == STATUS_AVAILABLE; }            // 0x100014f0
void Resource::InitRS(ResourceSystem* rs) { resourceSystem = rs; }                    // 0x10001500
const PString& Resource::GetName() const { return name; }                             // 0x10001510
const ResourceDatabase* Resource::GetOwner() const { return owner; }                  // 0x10001520
const ResourceDatabase* Resource::GetOrig() const { return orig; }                    // 0x10001530
const ResourceDatabase* Resource::GetDB() const { return orig ? orig : owner; }       // 0x10001540
bool Resource::operator==(const PString& n) const { return name.node == n.node; }     // 0x10001550
bool Resource::operator!=(const PString& n) const { return name.node != n.node; }     // 0x10001660
void Resource::SetLoadName(const PString& n) const { Assign(loadName, n); }           // 0x10001570
bool Resource::IsRedir() const { return (flags >> 3) & 1; }                           // 0x100015d0
void Resource::SetRedir(bool on) { flags = static_cast<unsigned char>((flags & ~8) | (on ? 8 : 0)); }  // 0x100015e0
const ResourceTypeBase* Resource::GetType() const { return type; }                    // 0x10001600
void Resource::SetAliased(bool on) const { flags = static_cast<unsigned char>((flags & ~2) | (on ? 2 : 0)); }
bool Resource::IsAliased() const { return (flags >> 1) & 1; }                         // 0x10001630
void Resource::SetInvalid(bool) const { status = STATUS_INVALID; }                    // 0x10001640 (the flag is ignored)
bool Resource::IsInvalid() const { return status == STATUS_INVALID; }                 // 0x10001650
void Resource::AddReference() const { ++refs; }                                       // 0x10001680
void Resource::InternalAddReference() const { ++internalRefs; }                       // 0x100016f0
int Resource::GetInternalReferenceCount() const { return internalRefs; }              // 0x10001720
int Resource::GetReferenceCount() const { return refs; }                              // 0x10001730
float Resource::GetPriority() const { return priority; }                              // 0x10001740
float Resource::GetHeapPriority() const { return heapPriority; }                      // 0x10001750
void Resource::UpdateHeapPriority(float t) const { heapPriority = priority + t; }     // 0x10001760
float Resource::GetCreationTime() const { return creationTime; }                      // 0x10001780
void Resource::SetTxt(bool on) const { flags = static_cast<unsigned char>((flags & ~4) | (on ? 4 : 0)); }  // 0x10001790
bool Resource::IsTxt() const { return (flags >> 2) & 1; }                             // 0x100017b0
void Resource::SetStatus(ResStatus s) const { status = s; }                           // 0x100017c0
bool Resource::GetMustResetWhenFinished() const { return false; }                     // 0x100017d0
bool Resource::Unload() { return false; }                                             // 0x100017d0
const String& Resource::GetScrambleKey() const { return scrambleKey; }                // 0x100017e0
Resource::ResStatus Resource::GetStatus() const { return static_cast<ResStatus>(status); }  // 0x100017f0
bool Resource::IsLocked() const { return locks > 0; }                                 // 0x10001800
int Resource::GetHitCount() const { return hits; }                                    // 0x10001810
void Resource::AddRedir(const Resource* r) const { redirs.Add(r); }                   // 0x10001820
unsigned int Resource::GetSize() const { return CachedSize(); }                             // 0x10001840
unsigned int Resource::GetDataSize() const { return CachedDataSize(); }                     // 0x10001860
unsigned int Resource::CalculateSize() const { return ~0u; }                          // 0x10001880
unsigned int Resource::CalculateDataSize() const { return 0; }                        // 0x100085a0
const Resource* Resource::GetRedirection() const { return nullptr; }                  // 0x100085a0
void Resource::SetOrig(const ResourceDatabase* db) const { orig = db; }               // 0x10001890
bool Resource::DebugGetObserversNotified() const { return observersNotified; }        // 0x100018a0
void Resource::SetPriority(float p) const { priority = p; }                           // 0x10002860
void Resource::SetScrambleKey(const String& key) const { scrambleKey = key; }         // 0x10002930
const char* Resource::GetStatusString(ResStatus s) { return kStatusNames[s]; }        // 0x100028f0
void Resource::LogStats() {}                                                          // 0x10004b60
void Resource::Touch() const { lastUse = resourceSystem->time; }                      // 0x10002b40
Resource::Result Resource::Load(const char*) { return 6; }                            // 0x10002180
Resource::Result Resource::LoadTxt(const char*) { return 6; }                         // 0x10002180
Resource::Result Resource::LoadComplete() { return 0; }                               // 0x100028e0

unsigned int Resource::CachedSize() const
{
    if (size == ~0u) size = CalculateSize();
    return size;
}

unsigned int Resource::CachedDataSize() const
{
    if (dataSize == ~0u) dataSize = CalculateDataSize();
    return dataSize;
}

// ---- references
// 0x10001690: the last reference of an owned resource leaves it to the bin (through Inactive when it is made
// available); one with no owner goes with its last internal reference
bool Resource::ReleaseReference() const
{
    if (--refs != 0) return false;
    if (owner) {
        lastUse = resourceSystem->time;
        if (GetMustResetWhenFinished()) ResetToReady();
        return true;
    }
    if (--internalRefs != 0) return false;
    DeleteThis();
    return true;
}

bool Resource::InternalReleaseReference() const  // 0x10001700
{
    if (--internalRefs != 0) return false;
    DeleteThis();
    return true;
}

// ---- loading
// 0x10002190: waits out the loader thread, loads on this thread when needed (under a slot so that two threads
// do not load one resource) and counts the hit. Results: 0 locked, 1 failed to load, 7 invalid.
Resource::Result Resource::Lock() const
{
    lastUse = resourceSystem->time;
    for (;;) {
        if ((status == STATUS_CACHED || status == STATUS_LOADING) && (resourceSystem->flags & 1))
            resourceSystem->LoaderClaim(this);
        if (status == STATUS_AVAILABLE) {
            if (!hits) ++totalHits;
            ++hits;
            ++locks;
            return 0;
        }
        ResourceSystem::AcquireSlot(this);
        if (status != STATUS_CACHED && status != STATUS_LOADING) break;
        ResourceSystem::ReleaseSlot(this);
    }
    if (status == STATUS_LOADED) resourceSystem->Unqueue(this);
    if (status == STATUS_LOADED) {
        if (const_cast<Resource*>(this)->LoadComplete().code == 0) {
            SetAvailableOnLoad();
            OnLoadNotify();
        } else {
            status = STATUS_INVALID;
        }
    }
    if (status == STATUS_AVAILABLE) {
        if (!hits) ++totalHits;
        ++hits;
        ++locks;
        ResourceSystem::ReleaseSlot(this);
        return 0;
    }
    if (status == STATUS_INVALID) {
        ResourceSystem::ReleaseSlot(this);
        return 7;
    }
    ++misses;
    ++totalMisses;
    if (!TemplateLoad(loadName.Text())) {
        status = STATUS_INVALID;
        ResourceSystem::ReleaseSlot(this);
        return 1;
    }
    status = STATUS_LOADED;
    ++totalLoaded;
    if (FinalizeLoad()) {  // (a fresh load counts the lock, not a hit)
        ++locks;
        ResourceSystem::ReleaseSlot(this);
        return 0;
    }
    ResourceSystem::ReleaseSlot(this);
    return 1;
}

void Resource::Unlock() const { --locks; }  // 0x10002320

void Resource::PreLoad() const  // 0x10002330 (the loader thread's)
{
    if (!TemplateLoad(loadName.Text())) {
        status = STATUS_INVALID;
        return;
    }
    ResourceSystem::QueueLoaded(this);
    ++totalLoaded;
}

// 0x10002370: a .txt resource loads "<name>.txt" through LoadTxt
bool Resource::TemplateLoad(const char* file) const
{
    Resource* const self = const_cast<Resource*>(this);
    if (flags & 4) {
        String full(file);
        static_cast<AnsiString&>(full) += String(".txt");
        return self->LoadTxt(full).code == 0;
    }
    return self->Load(file).code == 0;
}

bool Resource::FinalizeLoad() const  // 0x100024b0
{
    if (const_cast<Resource*>(this)->LoadComplete().code == 0) {
        SetAvailableOnLoad();
        OnLoadNotify();
        return true;
    }
    status = STATUS_INVALID;
    return false;
}

// 0x100024f0: a name relative to this resource's file (lowered and normalised)
void Resource::ConvertResourceName(String& s) const
{
    if (!s.Size()) return;
    s.ToLower();
    File::Path relative(s);
    relative.Normalize();
    File::Path path(loadName.Text());
    path.DelName(false);
    path.AddPath(relative.text);
    s = path.text;
}

void Resource::SetType(const ResourceTypeBase* t) const  // 0x100025b0
{
    if (type) type->ReleaseReference();
    type = t;
    if (t) t->AddReference();
}

void Resource::AttachType(const ResourceTypeBase* t) const  // 0x100025e0 (takes over the caller's reference)
{
    if (type) type->ReleaseReference();
    type = t;
}

const PString& Resource::GetOwnerName() const  // 0x10002610
{
    static PString none("");
    return owner ? owner->name : none;
}

// 0x100026b0: a resource of the same type and file under another name
const Resource* Resource::CreateInstance(const PString& n) const
{
    Resource* const r = type->Create(n, owner);
    Assign(r->loadName, loadName);
    if (r->type) r->type->ReleaseReference();
    r->type = type;
    if (type) type->AddReference();
    r->flags = static_cast<unsigned char>((r->flags & ~4) | (flags & 4));
    return r;
}

// 0x10002760: out of the system's totals and its database, then deleted
void Resource::DeleteThis() const
{
    const ResourceTypeBase* const t = type;
    RemoveRedirs();
    if (flags & 1) {
        ResourceSystem* const rs = resourceSystem;
        --rs->loadedResources;
        if (CachedDataSize() != ~0u) rs->dataSize -= CachedDataSize();
    }
    if (owner) owner->Remove(this);
    ResourceSystem* const rs = resourceSystem;
    --rs->resources;
    if (CachedSize() != ~0u) rs->size -= CachedSize();
    delete this;
    if (t) t->ReleaseReference();
}

// 0x10002810: an available resource nobody holds goes to the bin
void Resource::Inactive() const
{
    if (status != STATUS_AVAILABLE) return;
    --totalActive;
    const ResourceDatabase* const db = owner;
    db->lock.Lock(~0ul);
    if (refs == 0) ResourceDatabase::resourceSystem->bin->Add(this);
    db->lock.Unlock();
}

// 0x10002870: told at once when already loaded
void Resource::AddObserver(const ResourceObserver* o) const
{
    if (observersNotified) {
        o->OnLoad(this);
        return;
    }
    observers.Add(o);
}

void Resource::RemoveObserver(const ResourceObserver* o) const  // 0x100028a0
{
    ObserverList::Iterator i = observers.Find(o);
    if (i.node) observers.Remove(i);
}

// 0x10002900: queued for the loader thread (when there is one)
void Resource::Cache(bool force) const
{
    if (force) status = STATUS_READY;
    if (status == STATUS_READY && (resourceSystem->flags & 1)) resourceSystem->LoaderQueue(this);
}

void Resource::RemoveRedir(const Resource* r) const  // 0x10002950
{
    RedirList::Iterator i = redirs.Find(r);
    redirs.Remove(i);
}

// 0x10002980: each redirection (the last one, until it removes itself) loses its internal references
void Resource::RemoveRedirs() const
{
    while (redirs.head) {
        RedirList::Chunk* const last = redirs.head->prev;
        const Resource* const r = last->data[last->count - 1];
        if (--r->internalRefs == 0) r->DeleteThis();
    }
    redirs.RemoveAll();
}

// 0x10002a10: counted as loaded once; a loaded resource becomes available (and inactive when nobody holds it)
void Resource::SetAvailableOnLoad() const
{
    if (!(flags & 1)) {
        flags |= 1;
        ResourceSystem* const rs = resourceSystem;
        ++rs->loadedResources;
        if (CachedDataSize() != ~0u) rs->dataSize += CachedDataSize();
    }
    if (status == STATUS_LOADED) {
        status = STATUS_AVAILABLE;
        ++totalActive;
        if (refs == 0) Inactive();
    }
}

// 0x10002a80: the observers from the last
void Resource::OnLoadNotify() const
{
    if (ObserverList::Chunk* const head = observers.head) {
        ObserverList::Chunk* chunk = head->prev;
        const ResourceObserver* const* item = &chunk->data[chunk->count - 1];
        while (item) {
            const ResourceObserver* const o = *item;
            if (--item < &chunk->data[0]) {
                if (chunk == head) {
                    item = nullptr;
                } else {
                    chunk = chunk->prev;
                    item = &chunk->data[chunk->count - 1];
                }
            }
            o->OnLoad(this);
        }
    }
    observersNotified = true;
}

void Resource::ResetToReady() const  // 0x10002b00
{
    if (status != STATUS_AVAILABLE && status != STATUS_INACTIVE) return;
    if (resourceSystem->flags & 1) resourceSystem->LoaderClaim(this);
    resourceSystem->Unqueue(this);
    status = STATUS_READY;
}

bool Resource::LoadAll() const  // 0x10002b60
{
    if (Lock().code != 0) return false;
    Unlock();
    return true;
}

Resource::Result Resource::IsAllLoaded() const  // 0x10002b90: 0 available, 1 invalid, 5 not yet
{
    if (status == STATUS_AVAILABLE) return 0;
    if (status == STATUS_INVALID) return 1;
    return 5;
}

// ---- RedirResource
RedirResource::RedirResource(const PString& n, const ResourceDatabase* o, const Resource* t)  // 0x10001960
    : Resource(n, o), target(t)
{
    flags |= 8;
}

RedirResource::RedirResource(const RedirResource& o) : Resource(o), target(o.target) {}  // 0x100018b0

RedirResource& RedirResource::operator=(const RedirResource& o)  // 0x100018e0
{
    Resource::operator=(o);
    target = o.target;
    return *this;
}

RedirResource::~RedirResource()  // 0x100019a0
{
    if (target) target->RemoveRedir(this);
}

const Resource* RedirResource::GetRedirection() const { return target; }  // 0x10001990

// ---- ResourceVariant
ResourceVariant::ResourceVariant(const PString& n) : next(nullptr), prev(nullptr), name(n) {}  // 0x10001cc0
ResourceVariant::ResourceVariant(const ResourceVariant& o) : next(o.next), prev(o.prev), name(o.name) {}  // 0x10001c00
ResourceVariant::~ResourceVariant() {}  // 0x10001be0

ResourceVariant& ResourceVariant::operator=(const ResourceVariant& o)  // 0x10001c50
{
    next = o.next;
    prev = o.prev;
    Assign(name, o.name);
    return *this;
}

const PString& ResourceVariant::GetName() const { return name; }  // 0x10001d70

// ---- ResourceTypeBase
ResourceTypeBase::ResourceTypeBase(const PString& extension, const UId& id, const UId& parentId)  // 0x10008570
    : PluginType(extension, id, parentId)
{
}

ResourceTypeBase::ResourceTypeBase(const ResourceTypeBase& o) : PluginType(o) {}  // 0x10001090

ResourceTypeBase& ResourceTypeBase::operator=(const ResourceTypeBase& o)  // 0x100010b0
{
    PluginType::operator=(o);
    return *this;
}

ResourceTypeBase::~ResourceTypeBase() {}  // 0x10001080

const Resource* ResourceTypeBase::CreateOnTheFly(const PString&, const ResourceDatabase*) const { return nullptr; }

void ResourceTypeBase::Init(Resource* r) const { r->SetType(this); }  // 0x100085b0

// ---- ResourceInstance
ResourceInstance::ResourceInstance() : resource(nullptr), orig(nullptr) { flags &= ~1; }  // 0x10005fd0

// 0x10005ff0: a copy of a locked instance locks again
ResourceInstance::ResourceInstance(const ResourceInstance& o) : resource(o.resource), orig(o.orig)
{
    flags &= ~1;
    if (resource) resource->InlineAddReference();
    if ((o.flags & 1) && resource && !(flags & 1)) {
        if (resource->Lock().code == 0) flags |= 1;
    }
}

ResourceInstance::~ResourceInstance()  // 0x10006050
{
    if (resource) resource->ReleaseReference();
}

ResourceInstance& ResourceInstance::operator=(const ResourceInstance& o)  // 0x10006060 (the lock is not taken)
{
    if (this == &o) return *this;
    if (resource) resource->ReleaseReference();
    orig = o.orig;
    flags &= ~1;
    resource = o.resource;
    if (resource) resource->InlineAddReference();
    return *this;
}

ResourceInstance::Result ResourceInstance::Lock() const  // 0x100060a0
{
    if (resource && !(flags & 1)) {
        const Resource::Result r = resource->Lock();
        if (r.code != 0) return r;
        flags |= 1;
    }
    return (flags & 1) ? 0 : 4;
}

void ResourceInstance::Unlock() const  // 0x10006100
{
    if (resource && (flags & 1)) {
        resource->Unlock();
        flags &= ~1;
    }
}

ResourceInstance::Result ResourceInstance::IsAllLoaded() const  // 0x10006130
{
    return resource ? resource->IsAllLoaded() : Result(0);
}

bool ResourceInstance::IsAvailable() const { return resource ? resource->Status() == Resource::STATUS_AVAILABLE : true; }

void ResourceInstance::Inc() const  // 0x10006180
{
    if (resource) resource->InlineAddReference();
}

void ResourceInstance::Dec() const  // 0x10006190
{
    if (resource) resource->ReleaseReference();
}

PString ResourceInstance::GetName() const { return resource ? resource->GetName() : PString(); }  // 0x100061b0
ResourceInstance::operator bool() const { return resource != nullptr; }                         // 0x10006210
bool ResourceInstance::operator!() const { return resource == nullptr; }                        // 0x10006220
bool ResourceInstance::operator==(const ResourceInstance& o) const { return resource == o.resource; }
bool ResourceInstance::operator!=(const ResourceInstance& o) const { return resource != o.resource; }

bool ResourceInstance::operator==(const PString& n) const  // 0x10006270
{
    return resource ? resource->GetName().node == n.node : false;
}

bool ResourceInstance::operator!=(const PString& n) const  // 0x10006290
{
    return !(resource ? resource->GetName().node == n.node : false);
}

void ResourceInstance::Reset()  // 0x100062c0 (orig and the lock flag stay)
{
    if (resource) resource->ReleaseReference();
    resource = nullptr;
}

// 0x100062e0: the resource of that name from db (no name: none); the old one is released after
void ResourceInstance::Init(const ResourceDatabase* db, const PString& n)
{
    const Resource* const old = resource;
    if (n.node && n.node->length) {
        resource = db->Get(n, 0);
        orig = resource ? db : nullptr;
    } else {
        resource = nullptr;
        orig = nullptr;
    }
    if (old) old->ReleaseReference();
}

void ResourceInstance::Init(const ResourceDatabase* db, const PString& n, bool cache)  // 0x10006340
{
    const Resource* const old = resource;
    if (n.node && n.node->length) {
        resource = db->Get(n, cache ? 0 : 1);
        orig = resource ? db : nullptr;
    } else {
        resource = nullptr;
        orig = nullptr;
    }
    if (old) old->ReleaseReference();
}

// ---- the bins
void ResourceBin::Add(const Resource* r)  // 0x10002e20
{
    r->status = Resource::STATUS_INACTIVE;
    ResourceSystem* const rs = Resource::resourceSystem;
    ++rs->binned;
    const unsigned int s = r->CachedSize(), d = r->CachedDataSize();
    if (s != ~0u) rs->binSize += s;
    if (d != ~0u) rs->binSize += d;
}

bool ResourceBin::Reuse(const Resource* r)  // 0x10002e90
{
    ResourceSystem* const rs = Resource::resourceSystem;
    --rs->binned;
    const unsigned int s = r->CachedSize(), d = r->CachedDataSize();
    if (s != ~0u) rs->binSize -= s;
    if (d != ~0u) rs->binSize -= d;
    r->status = Resource::STATUS_AVAILABLE;
    ++Resource::totalActive;
    return true;
}

void ResourceBin::Remove(const Resource* r)  // 0x10002f10
{
    ResourceSystem* const rs = Resource::resourceSystem;
    --rs->binned;
    const unsigned int s = r->CachedSize(), d = r->CachedDataSize();
    if (s != ~0u) rs->binSize -= s;
    if (d != ~0u) rs->binSize -= d;
}

// 0x10002f70: out of the bin for deletion
void ResourceBinCache::Evict(const Resource* r)
{
    ResourceSystem* const rs = Resource::resourceSystem;
    --rs->binned;
    const unsigned int s = r->CachedSize(), d = r->CachedDataSize();
    if (s != ~0u) rs->binSize -= s;
    if (d != ~0u) rs->binSize -= d;
    r->status = Resource::STATUS_DELETE;
    r->RemoveRedirs();
}

namespace {

// 0x100034b0: by priority, highest first
int ByPriority(const void* a, const void* b)
{
    const float pa = (*static_cast<const Resource* const*>(a))->GetPriority();
    const float pb = (*static_cast<const Resource* const*>(b))->GetPriority();
    if (pb > pa) return 1;
    if (pa > pb) return -1;
    return 0;
}

}  // namespace

ResourceBinCache::ResourceBinCache() : count(0), capacity(0), data(nullptr) {}  // 0x10006a40

ResourceBinCache::~ResourceBinCache()  // 0x10006b90
{
    count = 0;
    if (data) Alloc::Delete(data);
}

void ResourceBinCache::Init(const Options* o)  // 0x10002fe0
{
    time = 0.0f;
    lastPass = 0.0f;
    processTime = 0.5f;
    oldest = 10.0f;
    minAge = 5.0f;
    spread = 2.0f;
    maxDeleted = 5;
    deleteRatio = 0.1f;
    if (!o) return;
    o->Get("ResourceBinProcessTime", processTime, 0.5f);
    o->Get("ResourceBinOldest", oldest, oldest);
    o->Get("ResourceBinMaxDeleted", maxDeleted, maxDeleted);
}

// 0x10003090: back from the bin. A resource marked for deletion stays out. (Not found, the first one goes.)
bool ResourceBinCache::Reuse(const Resource* r)
{
    SingleLock guard(&lock, true);
    if (r->status == Resource::STATUS_DELETE) return false;
    unsigned long i = 0;
    for (; i < count; ++i)
        if (data[i] == r && data[i]->owner == r->owner) break;
    if (i == count) i = 0;
    if (i < count) {
        --count;
        std::memmove(data + i, data + i + 1, (count - i) * sizeof(*data));
    }
    return ResourceBin::Reuse(r);
}

void ResourceBinCache::Remove(const Resource* r)  // 0x100034e0 (not found, the first one goes)
{
    SingleLock guard(&lock, true);
    unsigned long i = 0;
    for (; i < count; ++i)
        if (data[i] == r) break;
    if (i == count) i = 0;
    if (i < count) {
        --count;
        std::memmove(data + i, data + i + 1, (count - i) * sizeof(*data));
    }
    ResourceBin::Remove(r);
}

// 0x100031f0: kept until the bin's clock passes its priority: now + oldest + up to `spread` seconds
void ResourceBinCache::Add(const Resource* r)
{
    SingleLock guard(&lock, true);
    ResourceBin::Add(r);
    // (the sum in double precision, rounded once: the original adds on the x87 stack, starting from the draw as
    // NextFloat leaves it there, unrounded: KernelStandard exports a double function under that name)
    float (LCRand::*nextFloat)(float) = &LCRand::NextFloat;
    void* code;
    std::memcpy(&code, &nextFloat, sizeof(code));
    const double draw = reinterpret_cast<double(__fastcall*)(LCRand*, int, float)>(code)(&rand, 0, spread);
    r->SetPriority(static_cast<float>(draw + oldest + time));
    Push(r);
}

void ResourceBinCache::Push(const Resource* r)  // 0x10003740: grows by multiples of 8
{
    if (count + 1 > capacity) {
        const unsigned long items = (count + 1 + 7) / 8 * 8;
        auto* block = static_cast<const Resource**>(Alloc::New(static_cast<unsigned int>(items * sizeof(*data))));
        if (data) {
            std::memcpy(block, data, count * sizeof(*data));
            if (data) Alloc::Delete(data);
        }
        capacity = items;
        data = block;
    }
    data[count] = r;
    ++count;
}

// 0x100032a0: every processTime seconds, the resources due (lowest priority first) are deleted, at most
// max(count * deleteRatio, maxDeleted) of them
void ResourceBinCache::Update(float dt)
{
    time += dt;
    if (!(time - lastPass > processTime)) return;
    lastPass = time;
    const Resource** due;
    int evicted = 0;
    {
        SingleLock guard(&lock, true);
        const unsigned long n = count;
        if (!n) return;
        std::qsort(data, n, sizeof(*data), ByPriority);
        // (truncated toward zero, as the original's fistp under a chopping control word)
        const unsigned long byRatio = static_cast<unsigned long>(static_cast<long long>(static_cast<float>(n) * deleteRatio));
        unsigned long limit;
        float slack = 1.0f;
        if (byRatio > maxDeleted) {
            slack = static_cast<float>(byRatio) / static_cast<float>(maxDeleted);
            limit = byRatio;
        } else {
            limit = maxDeleted;
        }
        const unsigned long most = n > limit ? limit : n;
        due = static_cast<const Resource**>(_alloca(most * sizeof(*due)));
        for (unsigned long k = n; evicted < static_cast<int>(most); --k) {
            if (data[k - 1]->GetPriority() - slack > time) break;
            --count;
            due[evicted] = data[count];
            Evict(data[count]);
            ++evicted;
        }
    }
    for (int i = 0; i < evicted; ++i) {
        const Resource* const r = due[i];
        if (r->refs != 0) continue;
        r->owner->Remove(r);
        if (--r->internalRefs == 0) r->DeleteThis();
    }
}

void ResourceBinCache::Dump() {}  // 0x10003480

// 0x100035a0: everything in the bin is deleted (the ones still unused)
void ResourceBinCache::DumpAll()
{
    const Resource** all;
    unsigned long n;
    {
        SingleLock guard(&lock, true);
        n = count;
        if (!n) return;
        all = static_cast<const Resource**>(_alloca(n * sizeof(*all)));
        for (unsigned long i = 0; i < n; ++i) {
            --count;
            all[i] = data[count];
            Evict(all[i]);
        }
    }
    for (unsigned long i = 0; i < n; ++i) {
        const Resource* const r = all[i];
        const ResourceDatabase* const db = r->owner;
        db->lock.Lock(~0ul);
        if (r->refs == 0) {
            db->Remove(r);
            if (--r->internalRefs == 0) r->DeleteThis();
        }
        db->lock.Unlock();
    }
}

// 0x100037d0: a resource nobody holds leaves the bin at once (the base Remove: the counts go below zero, kept)
void ResourceBinInstant::Add(const Resource* r)
{
    const ResourceDatabase* const db = r->owner;
    db->lock.Lock(~0ul);
    if (r->refs == 0) Remove(r);
    db->lock.Unlock();
}

}  // namespace Jet
