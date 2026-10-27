// Unit U5a of RenderSystem_7c: RenderingCache (0x1000b6c0..0x1000b8e0, 0x10038ce0..0x10038db0),
// RenderingCacheManager (0x10038db0..0x100391e0), VertexBufferManager (0x100490c0..0x10049250, 0x1000cab0),
// StaticBuffer (0x1001df40..0x1001dfd0) and the chunk creation hints (0x10032220..0x10032400, 0x1003e770).
// Behaviour follows the TS12 assembly (addresses per function).
#include <jet/Geometry.h>

#include <cstring>

namespace Jet {

namespace {

// the cache a list node belongs to (its IListNode is at +4); null for null
RenderingCache* CacheOf(IListNode* n) { return static_cast<RenderingCache*>(n); }
IListNode* NextOf(const RenderingCache* c) { return static_cast<IListNode*>(static_cast<const IListNode*>(c)->first); }

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// RenderingCache

// 0x1000b750
RenderingCache::RenderingCache(const PString& n, float p) : rendersGeometry(true), enabled(true)
{
    name = n;
    priority = p;
    host = nullptr;
}

// 0x10038ce0
RenderingCache::~RenderingCache()
{
    if (host) host->RemoveCache(this);
}

void __cdecl RenderingCache::SetHost(RenderingCacheManager* h) { host = h; }  // 0x10021a60
void __cdecl RenderingCache::Activate(Display*, ViewPort*) {}                // 0x10021870
void __cdecl RenderingCache::Reset() {}                                      // 0x1001dec0
void __cdecl RenderingCache::Begin(Display*, ViewPort*) {}                   // 0x1001dec0
void __cdecl RenderingCache::End(Display*, ViewPort*) {}                     // 0x1001dec0
void __cdecl RenderingCache::Enter(Display*, ViewPort*, bool) {}             // 0x10021870
void __cdecl RenderingCache::Leave(Display*, ViewPort*, bool) {}             // 0x10021870

// 0x1000b870
void __cdecl RenderingCache::Flush(Display* d, ViewPort* v)
{
    End(d, v);
    Begin(d, v);
}

RenderingCacheManager* __cdecl RenderingCache::GetHost() const { return host; }     // 0x1000ae00
const PString& __cdecl RenderingCache::GetName() const { return name; }             // 0x1000c990
float __cdecl RenderingCache::GetPriority() const { return priority; }              // 0x1000b840
bool __cdecl RenderingCache::IsEnabled() const { return enabled; }                  // 0x1000b8a0
void __cdecl RenderingCache::Enable(bool on) { enabled = on; }                      // 0x1000b8b0
bool __cdecl RenderingCache::GetRendersGeometry() const { return rendersGeometry; }  // 0x1000b8d0
void __cdecl RenderingCache::SetRendersGeometry(bool on) { rendersGeometry = on; }  // 0x1000b8c0

// 0x1000b850
bool __cdecl RenderingCache::operator<(const RenderingCache& o) { return o.priority > priority; }

// 0x10038d70
void __cdecl RenderingCache::SetPriority(float p)
{
    priority = p;
    RenderingCacheManager* const h = host;
    if (h) {
        h->RemoveCache(this);
        h->AddCache(this);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// RenderingCacheManager: the manager is the ring's head (as a node at -4 of a cache)

namespace {

RenderingCache* EndOf(RenderingCacheManager* m)
{
    return reinterpret_cast<RenderingCache*>(reinterpret_cast<char*>(m) - 4);
}

}  // namespace

// 0x10038db0
RenderingCacheManager::RenderingCacheManager()
    : next(reinterpret_cast<IListNode*>(this)), prev(reinterpret_cast<IListNode*>(this)), count(0),
      geometryOnly(false)
{
}

// 0x10038dc0: each cache, from the first, out of the ring, reset and told it has no host; then the ring's own
// clean-up (0x10039190, nothing left by then)
RenderingCacheManager::~RenderingCacheManager()
{
    while (count) {
        RenderingCache* const c = CacheOf(next);
        IListNode* const n = c;
        static_cast<IListNode*>(n->second)->first = n->first;
        static_cast<IListNode*>(n->first)->second = n->second;
        n->second = nullptr;
        n->first = nullptr;
        --count;
        c->Reset();
        c->SetHost(nullptr);
    }
}

// 0x1003e930
RenderingCacheManager& RenderingCacheManager::operator=(const RenderingCacheManager& o)
{
    std::memcpy(this, &o, sizeof(*this));
    return *this;
}

// 0x10038e80 / 0x10039130: before the first cache of a priority not below its own
void __cdecl RenderingCacheManager::AddCache(RenderingCache* cache)
{
    if (!cache) return;
    RenderingCache* const end = EndOf(this);
    RenderingCache* at = CacheOf(next);
    while (at != end && cache->priority > at->priority) at = CacheOf(NextOf(at));
    IListNode* const n = cache;
    IListNode* const before = at;
    n->first = before;
    n->second = before->second;
    static_cast<IListNode*>(before->second)->first = n;
    before->second = n;
    ++count;
    cache->SetHost(this);
}

// 0x10038eb0: only a cache in the ring
void __cdecl RenderingCacheManager::RemoveCache(RenderingCache* cache)
{
    RenderingCache* const end = EndOf(this);
    for (RenderingCache* at = CacheOf(next); at != end; at = CacheOf(NextOf(at))) {
        if (at != cache) continue;
        IListNode* const n = cache;
        static_cast<IListNode*>(n->second)->first = n->first;
        static_cast<IListNode*>(n->first)->second = n->second;
        n->second = nullptr;
        n->first = nullptr;
        --count;
        cache->SetHost(nullptr);
        return;
    }
}

// 0x10038f20
RenderingCache* __cdecl RenderingCacheManager::FindCache(const PString& name)
{
    RenderingCache* const end = EndOf(this);
    for (RenderingCache* at = CacheOf(next); at != end; at = CacheOf(NextOf(at)))
        if (at->name.node == name.node) return at;
    return nullptr;
}

// 0x10038f60
void __cdecl RenderingCacheManager::Begin(Display* d, ViewPort* v, bool geometry)
{
    geometryOnly = geometry;
    RenderingCache* const end = EndOf(this);
    for (RenderingCache* at = CacheOf(next); at != end; at = CacheOf(NextOf(at)))
        if (!geometryOnly || at->rendersGeometry) at->Begin(d, v);
}

// 0x10038fc0: every cache activated, then every cache ended
void __cdecl RenderingCacheManager::End(Display* d, ViewPort* v)
{
    RenderingCache* const end = EndOf(this);
    for (RenderingCache* at = CacheOf(next); at != end; at = CacheOf(NextOf(at)))
        if (!geometryOnly || at->rendersGeometry) at->Activate(d, v);
    for (RenderingCache* at = CacheOf(next); at != end; at = CacheOf(NextOf(at)))
        if (!geometryOnly || at->rendersGeometry) at->End(d, v);
}

// 0x10039060
void __cdecl RenderingCacheManager::Enter(Display* d, ViewPort* v, bool b)
{
    RenderingCache* const end = EndOf(this);
    for (RenderingCache* at = CacheOf(next); at != end; at = CacheOf(NextOf(at)))
        if (!geometryOnly || at->rendersGeometry) at->Enter(d, v, b);
}

// 0x100390c0
void __cdecl RenderingCacheManager::Leave(Display* d, ViewPort* v, bool b)
{
    RenderingCache* const end = EndOf(this);
    for (RenderingCache* at = CacheOf(next); at != end; at = CacheOf(NextOf(at)))
        if (!geometryOnly || at->rendersGeometry) at->Leave(d, v, b);
}

// ---------------------------------------------------------------------------------------------------------------
// StaticBuffer (0x1001df50 / 0x1001df40)

StaticBuffer::StaticBuffer() {}
StaticBuffer::~StaticBuffer() {}

// ---------------------------------------------------------------------------------------------------------------
// VertexBufferManager

// 0x10049130
VertexBufferManager::VertexBufferManager()
    : display(nullptr), dynamicBufferSize(0x108000), dynamicIndexSize(0xffff), lock(nullptr)
{
    std::memset(&stats, 0, sizeof(stats));
    std::memset(more, 0, sizeof(more));
    stats.v[4] = 0xffffffff;  // +0x20
    stats.v[6] = 0xffffffff;  // +0x28
}

VertexBufferManager::~VertexBufferManager() {}  // 0x100491b0

// 0x100491c0
bool __cdecl VertexBufferManager::Initialize(Display* d, const Options*)
{
    display = d;
    return true;
}

// 0x100491d0
void __cdecl VertexBufferManager::GetConfiguration(const Options* options)
{
    unsigned long v = dynamicBufferSize;
    options->Get("DynamicVertexBufferSize", v, v);
    dynamicBufferSize = v;
    v = dynamicIndexSize;
    options->Get("DynamicIndexBufferSize", v, v);
    dynamicIndexSize = v;
}

// 0x10049210: a copy taken under the lock
VertexBufferManager::BufferStats __cdecl VertexBufferManager::GetBufferStats() const
{
    const bool locked = lock.Lock(0xffffffff);
    const BufferStats s = stats;
    if (locked) lock.Unlock();
    return s;
}

unsigned int __cdecl VertexBufferManager::GetDefaultDynamicBufferSize() const { return dynamicBufferSize; }  // 0x1000cab0
unsigned int __cdecl VertexBufferManager::GetDefaultDynamicIndexSize() const { return dynamicIndexSize; }  // 0x100106c0

// the empty slots (0x10021870) and the ones answering 0 (0x1001dec0)
void __cdecl VertexBufferManager::Update() {}
void __cdecl VertexBufferManager::FlushState() {}
void __cdecl VertexBufferManager::ResetDynamicBufferForReuse() {}
void __cdecl VertexBufferManager::ResetDynamicBufferOffsets() {}
void __cdecl VertexBufferManager::DrawComplete(const StaticBuffer*) {}
void __cdecl VertexBufferManager::DrawComplete(const VertexType&) {}
int __cdecl VertexBufferManager::Slot2() { return 0; }
int __cdecl VertexBufferManager::Slot3() { return 0; }
int __cdecl VertexBufferManager::Slot5() { return 0; }
int __cdecl VertexBufferManager::Slot6() { return 0; }
int __cdecl VertexBufferManager::Slot7() { return 0; }
int __cdecl VertexBufferManager::Slot8() { return 0; }
int __cdecl VertexBufferManager::Slot9() { return 0; }
int __cdecl VertexBufferManager::Slot10() { return 0; }
int __cdecl VertexBufferManager::Slot11() { return 0; }
int __cdecl VertexBufferManager::Slot12() { return 0; }
int __cdecl VertexBufferManager::Slot13() { return 0; }
int __cdecl VertexBufferManager::Slot19() { return 0; }
int __cdecl VertexBufferManager::Slot21() { return 0; }
int __cdecl VertexBufferManager::Slot22() { return 0; }

// ---------------------------------------------------------------------------------------------------------------
// chunk creation hints

GeometryChunkCreateHint::GeometryChunkCreateHint() : version(2), size(2), system(0), user(0) {}  // 0x10032240
GeometryChunkCreateHint::GeometryChunkCreateHint(unsigned long s, unsigned long u)               // 0x10032260
    : version(2), size(2), system(s), user(u)
{
}
GeometryChunkCreateHint& GeometryChunkCreateHint::operator=(const GeometryChunkCreateHint& o)  // 0x1003e930
{
    std::memcpy(this, &o, sizeof(*this));
    return *this;
}
bool __cdecl GeometryChunkCreateHint::SupportsSystemHint(unsigned long h) const { return (system & h) != 0; }  // 0x10032280
bool __cdecl GeometryChunkCreateHint::SupportsUserHint(unsigned long h) const { return (user & h) != 0; }  // 0x100322a0

// 0x100322c0 / 0x100322f0
StandardChunkHint::StandardChunkHint() : GeometryChunkCreateHint(1, 0), bits(0), a(0), b(0), c(1) {}
StandardChunkHint::StandardChunkHint(unsigned long s, unsigned long u)
    : GeometryChunkCreateHint(s | 1, u), bits(0), a(0), b(0), c(1)
{
}
StandardChunkHint& StandardChunkHint::operator=(const StandardChunkHint& o)  // 0x10032220
{
    std::memcpy(this, &o, sizeof(*this));
    return *this;
}

// 0x10032330 / 0x10032360
DatabaseChunkHint::DatabaseChunkHint() : StandardChunkHint(), database(0) { system = 3; }
DatabaseChunkHint::DatabaseChunkHint(unsigned long s, unsigned long u) : StandardChunkHint(s, u), database(0)
{
    system = s | 3;
}
DatabaseChunkHint& DatabaseChunkHint::operator=(const DatabaseChunkHint& o)  // 0x1003e770
{
    std::memcpy(this, &o, sizeof(*this));
    return *this;
}

// 0x100323a0 / 0x100323d0
TerrainMorphChunkHint::TerrainMorphChunkHint() : StandardChunkHint(), morph(false) { system = 5; }
TerrainMorphChunkHint::TerrainMorphChunkHint(unsigned long s, unsigned long u) : StandardChunkHint(s, u), morph(false)
{
    system = s | 5;
}
TerrainMorphChunkHint& TerrainMorphChunkHint::operator=(const TerrainMorphChunkHint& o)  // 0x1003e770
{
    std::memcpy(this, &o, sizeof(*this));
    return *this;
}

}  // namespace Jet
