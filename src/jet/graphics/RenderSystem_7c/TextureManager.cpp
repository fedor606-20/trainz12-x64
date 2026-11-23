// Unit U6d1a of RenderSystem_7c: TextureManager's containers and lifetime (0x10041420..0x10042361, the helpers
// 0x10042d00..0x10043187 and the folded getters 0x10004aa0 / 0x10004eb0 / 0x10004ec0). Behaviour follows the
// TS12 assembly (addresses per function). U6d1b starts with queueing and cancelling texture downsizes.
// Update and the async loader still call the original's exports. The manager's
// statics, the TextureResource defaults Init sets, the format-name table (0x100b8780, built by the original's
// initializer) and SharedTextureCollection's substitute texture and name (0x100b8708 / 0x100b8734) stay the
// original's.
#include <jet/KernelResources.h>
#include <jet/Texture.h>
#include <jet/VCall.h>

#include <windows.h>

#include <new>

extern "C" void RsMirrorSync(const char* name);  // U11: keep exported data mirrors in step
namespace Jet {

DisplayTexture* __cdecl RsGenerateSubstituteTexture(void* display, unsigned width, unsigned height);

// Shared with the original Update and the background loader while they still own queue processing.
struct TextureResizeRequest {
    enum State { Downsize = 1, AsyncLoad = 2 };

    const TextureRes* texture;
    const ResourceDatabase* database;
    bool cancelled;
    int reverseLevel;
    int firstMip;
    State state;
};
JET_LAYOUT32(sizeof(TextureResizeRequest) == 0x18);
JET_LAYOUT32(offsetof(TextureResizeRequest, reverseLevel) == 0xc);
JET_LAYOUT32(offsetof(TextureResizeRequest, state) == 0x14);

namespace {

using Node = PStringCache::Node;

unsigned char* OrigBase()
{
    static unsigned char* const base = reinterpret_cast<unsigned char*>(GetModuleHandleA("RenderSystem_7c_orig.dll"));
    return base;
}

// 0x100411a0: a copy of `a` with `text` appended
PString Concat(const PString& a, const char* text)
{
    PString r(a);
    r.node = PString::Cache()->Ins(r.node, reinterpret_cast<const unsigned char*>(text), r.node ? r.node->length : 0);
    return r;
}

// 0x1000ddd0: the old node goes first
void Assign(PString& dst, const PString& src)
{
    if (dst.node) PString::Cache()->Destroy(dst.node);
    ::new (&dst) PString(src);
}

const void* NameKey(const Texture* t) { return t->GetName().node; }

Display* KernelDisplay(const RenderKernel* k) { return *reinterpret_cast<Display* const*>(reinterpret_cast<const char*>(k) + 0x7c); }
OS* KernelOS(const RenderKernel* k) { return *reinterpret_cast<OS* const*>(reinterpret_cast<const char*>(k) + 0x8c); }

// Retail initialization at 0x1005e570 builds these 16 pairs, in this order.
struct FormatName {
    const char* name;
    int id;
};
constexpr char kDefaultFormatName[] = "ARGB8888";
constexpr FormatName kFormatNames[] = {
    {"DXT1", 10}, {"DXT3", 11}, {"DXT5", 12}, {"ATC", 25},
    {"ATCE", 26}, {"ATCI", 27}, {"ETC1", 28}, {kDefaultFormatName, 1},
    {"ABGR8888", 2}, {"RGB0888", 3}, {"BGR0888", 4}, {"RGB888", 5},
    {"BGR888", 6}, {"ARGB4444", 7}, {"RGB565", 8}, {"RGB555", 9},
};

// SharedTextureCollection::textureArray[2] (an AutoReference) and textureNameArray[2]
Texture*& SubstituteTexture() { return *reinterpret_cast<Texture**>(OrigBase() + 0xb8708); }
const PString& SubstituteName() { return *reinterpret_cast<const PString*>(OrigBase() + 0xb8734); }

// 0x1006b110 / 0x100b1bec
const char* const kAutoMipNames[4] = {"none", "default", "fastest", "nicest"};
const char* const kFilterNames[2] = {"nearest", "linear"};

// the display's slots used here
bool Query(Display* d, int what) { return VCall<bool>(d, 0x1c, what); }
float MaxAnisotropy(Display* d) { return VCall<float>(d, 0x28); }

// a streaming request's list node (TextureManagerInfo +0xc)
struct InfoNode {
    InfoNode* owner;
    InfoNode* prev;
    InfoNode* next;
    TextureManagerInfo* info;
};
InfoNode* NodeOf(void* info) { return reinterpret_cast<InfoNode*>(static_cast<char*>(info) + 0xc); }

// the number of halvings from `size` down to 1, plus one (0x10041420 / 0x10041450)
int MipCount(int size)
{
    int c = 0;
    for (int n = size / 2; n; n /= 2) ++c;
    return c + 1;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// the streaming-request rings

// 0x10042d00
TextureManager::ListHead::ListHead() : owner(this), prev(this), next(this), unused(0) {}

// 0x10042d30: a head that is someone's node unlinks itself; else every node is taken off from the tail
TextureManager::ListHead::~ListHead()
{
    if (owner != this) {
        next->prev = prev;
        prev->next = next;
        prev = this;
        next = this;
        owner = this;
        return;
    }
    while (prev != this) {
        ListHead* n = prev;
        n->next->prev = n->prev;
        n->prev->next = n->next;
        n->prev = n;
        n->next = n;
        n->owner = n;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the static setters

int TextureManager::MIN_STREAMING_TEXTURE_SIZE = 128;
int TextureManager::MIN_STREAMING_TEXTURE_MIP_COUNT = 8;   // MipCount(128)
int TextureManager::MAX_STREAMING_TEXTURE_MIP_COUNT = 12;  // MipCount(2048)
int TextureManager::MAX_STREAMING_TEXTURE_SIZE = 2048;
bool TextureManager::TEXTURE_STREAMING_ENABLED = true;

void __cdecl TextureManager::SetMinSizeTextureStreaming(int size)  // 0x10041420
{
    MIN_STREAMING_TEXTURE_SIZE = size;
    MIN_STREAMING_TEXTURE_MIP_COUNT = MipCount(size);
}

void __cdecl TextureManager::SetMaxSizeTextureStreaming(int size)  // 0x10041450
{
    MAX_STREAMING_TEXTURE_SIZE = size;
    MAX_STREAMING_TEXTURE_MIP_COUNT = MipCount(size);
    BitmapResource::SetMaxSizeTextureStreaming(size);
}

void __cdecl TextureManager::SetEnableTextureStreaming(bool on)  // 0x10041490
{
    TEXTURE_STREAMING_ENABLED = on;
}

// ---------------------------------------------------------------------------------------------------------------
// lifetime

// 0x100414a0
TextureManager::TextureManager()
    : kernel(nullptr), active(), frame(0), activeMemory(0), useSubstitute(false), field38(0), fieldA4(0),
      fieldA8(0), listIndex(0), fieldD4(1)
{
}

// 0x10041580: the queues first, then the update list and the table emptied; the members go in reverse order,
// the table's buckets last
TextureManager::~TextureManager()
{
    CleanupAsyncTextureLoad();
    updates.RemoveAll();
    textures.Clear();
}

// ---------------------------------------------------------------------------------------------------------------
// forwarded streaming operations

// 0x10042370: queue a load only while the texture has no resize request.
// Update and its worker retain ownership of consuming and releasing the request.
void __cdecl TextureManager::AddAsyncTextureLoad(const TextureRes* t, const ResourceDatabase* database,
                                                int firstMip, int reverseLevel)
{
    if (t->asyncInfo) return;
    void* const mem = Alloc::New(sizeof(TextureResizeRequest));
    if (!mem) return;
    auto* const request = ::new (mem) TextureResizeRequest{
        t, database, false, reverseLevel, firstMip, TextureResizeRequest::AsyncLoad};
    asyncQueue.Add(request);
    t->asyncInfo = request;
}

void __cdecl TextureManager::Update()  // 0x10042480
{
    if (!TEXTURE_STREAMING_ENABLED) return;
    if (fieldA4 == 2) {
        auto* request = reinterpret_cast<TextureResizeRequest*>(field38);
        AppendMips(const_cast<TextureRes*>(request->texture), &info, request->reverseLevel);
        ReleaseActiveLoad();
    }
    if (fieldA4 != 1) {
        if (fieldA4 != 0) ReleaseActiveLoad();
        static auto const system = reinterpret_cast<ResourceSystem**>(GetProcAddress(
            GetModuleHandleA("KernelResource_7c.dll"), "?resourceSystem@ResourceDatabase@Jet@@0PAVResourceSystem@2@A"));
        if (asyncQueue && *system && (*system)->GetQueuedResourceCount() < 1) {
            field38 = reinterpret_cast<int>(asyncQueue.Pop());
            if (!field38 || !StartActiveLoad()) ReleaseActiveLoad();
        }
    }
    for (int count = 0; count < 4 && downsizeQueue; ++count) {
        TextureResizeRequest* const request = downsizeQueue.Pop();
        auto* const texture = const_cast<TextureRes*>(request->texture);
        DownSizeRemoveMips(texture, request->reverseLevel + 1);
        texture->asyncInfo = nullptr;
        Alloc::Delete(request);
    }
    CheckTexturesNeedDownSize();
}

void TextureManager::AppendMips(TextureRes* texture, TextureResource::TextureInfo* loaded, int reverseLevel)
{
    const int oldLevels = static_cast<int>(texture->GetDisplayTexture()->GetNumMipLevels());
    const int first = oldLevels > 1 ? oldLevels - 1 : 0;
    const Size size = TextureResource::GetMipSizeAtReverseLevel(reverseLevel, loaded->size);
    texture->ChangeMipCount(size, reverseLevel + 1, true);
    TextureResource::LoadMipLevels(loaded, texture->GetDisplayTexture(), first, reverseLevel);
}

bool __cdecl TextureManager::RemovedAsyncTextureLoad(const TextureRes* t)  // 0x100423e0
{
    const auto* const active = reinterpret_cast<const TextureResizeRequest*>(field38);
    if (active && active->texture == t) {
        if (active->state != TextureResizeRequest::AsyncLoad) return false;
        CancelActiveLoad();
        ReleaseActiveLoad();
        return true;
    }
    TextureResizeRequest* const request = t->asyncInfo;
    if (!request || request->state != TextureResizeRequest::AsyncLoad) return false;
    const auto i = asyncQueue.Find(request);
    if (i != asyncQueue.End()) asyncQueue.Remove(i);
    t->asyncInfo = nullptr;
    Alloc::Delete(request);
    return true;
}

void TextureManager::CancelActiveLoad()
{
    volatile int& state = fieldA4;
    if (state != 1) return;
    auto* const request = reinterpret_cast<TextureResizeRequest*>(field38);
    if (request) request->cancelled = true;
    while (state == 1) Time::Delay(16);
}

void TextureManager::ReleaseActiveLoad()
{
    if (fieldA8) {
        auto* const worker = reinterpret_cast<Thread*>(fieldA8);
        worker->~Thread();
        Alloc::Delete(worker);
    }
    fieldA8 = 0;
    auto* const request = reinterpret_cast<TextureResizeRequest*>(field38);
    if (request && request->texture) request->texture->asyncInfo = nullptr;
    if (request) Alloc::Delete(request);
    field38 = 0;
    info.FreeBlocks();
    fieldA4 = 0;
}

unsigned int __stdcall TextureManager::RunActiveLoad(void* manager)
{
    auto* const self = static_cast<TextureManager*>(manager);
    auto* const request = reinterpret_cast<TextureResizeRequest*>(self->field38);
    self->info.FreeBlocks();
    const TextureResource* const resource = request->texture->GetResource();
    const PString& name = request->texture->GetResourceName();
    const char* const text = name.node ? name.node->text : nullptr;
    const auto result = TextureResource::Load(request->database, text, resource->ScrambleKey(),
                                               request->firstMip, request->reverseLevel, &self->info);
    volatile int& state = self->fieldA4;
    state = request->cancelled ? 4 : (result.code ? 3 : 2);
    return 0;
}

bool TextureManager::StartActiveLoad()
{
    auto* const request = reinterpret_cast<TextureResizeRequest*>(field38);
    if (!request->texture || !request->database || request->cancelled) {
        fieldA4 = 3;
        return false;
    }
    fieldA4 = 1;
    void* const memory = Alloc::New(sizeof(Thread));
    Thread* const worker = memory ? ::new (memory) Thread(&RunActiveLoad, this, true) : nullptr;
    fieldA8 = reinterpret_cast<int>(worker);
    if (!worker || !worker->StartedOk()) {
        if (worker) {
            worker->~Thread();
            Alloc::Delete(worker);
        }
        fieldA8 = 0;
        return false;
    }
    return true;
}

void __cdecl TextureManager::UpSizeTexture(const TextureRes* t, int size)  // 0x10042720
{
    if (t->asyncInfo || t->mipLevelsOnDisk <= 1) return;
    const TextureResource* const resource = t->GetResource();
    const ResourceDatabase* const database = resource ? resource->Owner() : nullptr;
    const DisplayTexture* const display = t->GetDisplayTexture();
    if (!database || !display) return;
    const int current = static_cast<int>(display->GetNumMipLevels());
    int limit = t->mipLevelsOnDisk;
    if (MAX_STREAMING_TEXTURE_MIP_COUNT < limit) limit = MAX_STREAMING_TEXTURE_MIP_COUNT;
    const int requested = MipCount(size);
    if (requested < limit) limit = requested;
    if (current < limit) AddAsyncTextureLoad(t, database, current, limit - 1);
}

void TextureManager::CleanupAsyncTextureLoad()  // 0x10042690
{
    CancelActiveLoad();
    ReleaseActiveLoad();
    for (auto i = asyncQueue.First(); i != asyncQueue.End(); ++i) Alloc::Delete(*i);
    asyncQueue.RemoveAll();
    for (auto i = downsizeQueue.First(); i != downsizeQueue.End(); ++i) Alloc::Delete(*i);
    downsizeQueue.RemoveAll();
}

// 0x100427a0: the explicit downsize API has a 128-pixel floor, independent of the streaming settings.
void __cdecl TextureManager::DownSizeTexture(const TextureRes* t, int size)
{
    if (t->asyncInfo || t->mipLevelsOnDisk <= 1) return;
    const DisplayTexture* const display = t->GetDisplayTexture();
    if (!display) return;
    const int mips = MipCount(size < 128 ? 128 : size);
    if (mips < static_cast<int>(display->GetNumMipLevels())) AddTextureDownSize(t, mips - 1);
}

void __cdecl TextureManager::DownSizeRemoveMips(TextureRes* texture, int levels)
{
    DisplayTexture* const display = texture->GetDisplayTexture();
    if (!display || levels >= static_cast<int>(display->GetNumMipLevels())) return;
    const Size size = TextureResource::GetMipSizeAtReverseLevel(levels - 1, texture->maxSizeOnDisk);
    texture->ChangeMipCount(size, levels, true);
}

void __cdecl TextureManager::TestUpSizeStreamableTextures()
{
    textures.ForEach([this](Texture* texture) {
        if (auto* res = dynamic_cast<TextureRes*>(texture)) UpSizeTexture(res, 65535);
    });
}

void __cdecl TextureManager::TestDownSizeStreamableTextures()
{
    textures.ForEach([this](Texture* texture) {
        auto* res = dynamic_cast<TextureRes*>(texture);
        if (!res || res->asyncInfo || res->mipLevelsOnDisk <= 1) return;
        const DisplayTexture* const display = res->GetDisplayTexture();
        if (display && static_cast<int>(display->GetNumMipLevels()) > 8) AddTextureDownSize(res, 7);
    });
}

// 0x10042940
bool __cdecl TextureManager::IsTextureBeingResized(const TextureRes* t) { return t->asyncInfo != nullptr; }

// 0x10042950
void __cdecl TextureManager::CancelResizeTexture(const TextureRes* t)
{
    if (!t->asyncInfo) return;
    if (t->asyncInfo->state == TextureResizeRequest::AsyncLoad)
        RemovedAsyncTextureLoad(t);
    else if (t->asyncInfo->state == TextureResizeRequest::Downsize)
        RemovedTextureDownSize(t);
}

// 0x10042980
bool __cdecl TextureManager::RemovedTextureDownSize(const TextureRes* t)
{
    TextureResizeRequest* const request = t->asyncInfo;
    if (!request || request->state != TextureResizeRequest::Downsize) return false;
    const auto i = downsizeQueue.Find(request);
    if (i != downsizeQueue.End()) downsizeQueue.Remove(i);
    t->asyncInfo = nullptr;
    Alloc::Delete(request);
    return true;
}

// 0x100429f0: the original Update consumes this request in reverse mip-level coordinates.
void __cdecl TextureManager::AddTextureDownSize(const TextureRes* t, int reverseLevel)
{
    if (t->asyncInfo) return;
    void* const mem = Alloc::New(sizeof(TextureResizeRequest));
    if (!mem) return;
    auto* const request = ::new (mem) TextureResizeRequest{
        t, nullptr, false, reverseLevel, 0, TextureResizeRequest::Downsize};
    downsizeQueue.Add(request);
    t->asyncInfo = request;
}

// 0x10042c20: revisit textures that have not been considered for 100 frames.
void __cdecl TextureManager::CheckNeedDownSize(TextureManagerInfo& info)
{
    auto* const fields = reinterpret_cast<unsigned int*>(&info);
    auto* const texture = reinterpret_cast<TextureRes*>(fields[2]);
    if (texture->mipLevelsOnDisk <= 1) return;
    const DisplayTexture* const display = texture->GetDisplayTexture();
    if (!display || static_cast<int>(frame - fields[1]) <= 100) return;
    int wanted = static_cast<int>(fields[0]);
    if (wanted < MIN_STREAMING_TEXTURE_MIP_COUNT) wanted = MIN_STREAMING_TEXTURE_MIP_COUNT;
    fields[0] = static_cast<unsigned int>(wanted);
    if (wanted < static_cast<int>(display->GetNumMipLevels()))
        DownSizeTexture(texture, static_cast<int>(1u << ((wanted - 1) & 31)));
    fields[1] = frame;
    fields[0] = 0;
}

void __cdecl TextureManager::CheckTexturesNeedDownSize()
{
    int switches = 0;
    for (int remaining = 10; remaining > 0; --remaining) {
        bool switched = false;
        auto* head = reinterpret_cast<InfoNode*>(lists[listIndex].owner);
        InfoNode* node = head->prev;
        if (node == head) {
            const int previous = listIndex;
            listIndex = fieldD4;
            fieldD4 = previous;
            if (++switches > 1) return;
            switched = true;
            head = reinterpret_cast<InfoNode*>(lists[listIndex].owner);
            node = head->prev;
            if (node == head) continue;
        }
        node->next->prev = node->prev;
        node->prev->next = node->next;
        node->owner = node->prev = node->next = node;
        if (!node->info) {
            if (switched) continue;
            const int previous = listIndex;
            listIndex = fieldD4;
            fieldD4 = previous;
            if (++switches > 1) return;
            head = reinterpret_cast<InfoNode*>(lists[listIndex].owner);
            node = head->prev;
            if (node == head) continue;
            node->next->prev = node->prev;
            node->prev->next = node->next;
            node->owner = node->prev = node->next = node;
            if (!node->info) continue;
        }
        CheckNeedDownSize(*node->info);
        auto* destination = reinterpret_cast<InfoNode*>(lists[fieldD4].owner);
        node->prev = destination;
        node->next = destination->next;
        destination->next->prev = node;
        destination->next = node;
        node->owner = destination->owner;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the textures

// 0x10041a50: on appends (again if already there), off takes out the first entry
void __cdecl TextureManager::SetTextureUpdate(Texture* t, bool on)
{
    if (on) {
        updates.Add(t);
        return;
    }
    for (auto i = updates.First(); i != updates.End(); ++i)
        if (*i == t) {
            updates.Remove(i);
            return;
        }
}

// 0x10041ab0: the request's node moves to the end of the current list
void __cdecl TextureManager::RegisterStreamableTexture(TextureRes* t)
{
    InfoNode* const n = NodeOf(t->GetTextureManagerInfo());
    InfoNode* const head = reinterpret_cast<InfoNode*>(lists[listIndex].owner);
    n->next->prev = n->prev;
    n->prev->next = n->next;
    n->prev = n;
    n->owner = n;
    n->next = head;
    n->prev = head->prev;
    head->prev = n;
    n->prev->next = n;
    n->owner = head->owner;
}

// 0x10041b10
void __cdecl TextureManager::Add(Texture* t)
{
    textures.Insert(t, NameKey(t));
    if (t->flags & 1) updates.Add(t);
}

// 0x10041b50: a TextureRes leaves its streaming queue and request list first
void __cdecl TextureManager::RemoveTexture(Texture* t)
{
    if (TextureRes* const r = dynamic_cast<TextureRes*>(t)) {
        CancelResizeTexture(r);
        InfoNode* const n = NodeOf(r->GetTextureManagerInfo());
        n->next->prev = n->prev;
        n->prev->next = n->next;
        n->prev = n;
        n->next = n;
        n->owner = n;
    }
    const void* const key = NameKey(t);
    auto& bucket = textures.buckets[reinterpret_cast<unsigned int>(key) % textures.size];
    for (auto i = bucket.First(); i != bucket.End(); ++i)
        if (NameKey(*i) == key) {
            --textures.count;
            bucket.Remove(i);
            if (t->flags & 1) SetTextureUpdate(t, false);
            return;
        }
}

// 0x10041c40
Texture* __cdecl TextureManager::Find(const PString& name)
{
    Texture* const* const slot = textures.Find(name.node, NameKey);
    return slot ? *slot : nullptr;
}

// 0x10041c70
void __cdecl TextureManager::Begin()
{
    activeMemory = 0;
    active.count = 0;
    ++frame;
    Update();
}

// 0x10041c90: counted once per frame; with streaming on, a TextureRes's wanted mip count only grows (clamped to
// the streaming limits) and a texture loaded with fewer levels is sized up
void __cdecl TextureManager::Activate(Texture* t, int mips)
{
    if (t->activeFrame != frame) {
        active.Reserve(active.count + 1);
        active.data[active.count] = t;
        ++active.count;
        activeMemory += t->GetMemorySize() >> 10;
    }
    if (!TEXTURE_STREAMING_ENABLED) return;
    int* const wanted = reinterpret_cast<int*>(t->GetTextureManagerInfo());
    if (!wanted) return;
    int limit = *reinterpret_cast<const int*>(reinterpret_cast<const char*>(t) + 0x50);
    if (limit <= 1) return;
    if (MAX_STREAMING_TEXTURE_MIP_COUNT <= limit) limit = MAX_STREAMING_TEXTURE_MIP_COUNT;
    int m = mips;
    if (m < 0 || m > limit)
        m = limit;
    else if (m < MIN_STREAMING_TEXTURE_MIP_COUNT)
        m = MIN_STREAMING_TEXTURE_MIP_COUNT;
    DisplayTexture* const d = t->GetDisplayTexture();
    if (m < *wanted) m = *wanted;
    *wanted = m;
    if (d && m > *reinterpret_cast<const int*>(reinterpret_cast<const char*>(d) + 0x20))
        UpSizeTexture(static_cast<const TextureRes*>(t), 1 << (m - 1));
}

// 0x10041d60
void __cdecl TextureManager::End(float time)
{
    for (auto i = updates.First(); i != updates.End(); ++i) (*i)->Update(time);
}

// 0x10042300
unsigned long __cdecl TextureManager::GetTotalTextureMemory() const
{
    unsigned long sum = 0;
    const_cast<Textures&>(textures).ForEach([&sum](Texture* t) { sum += t->GetMemorySize() >> 10; });
    return sum;
}

unsigned long __cdecl TextureManager::GetActiveTextureMemory() const { return activeMemory; }  // 0x10004eb0
unsigned int __cdecl TextureManager::GetActiveFrame() const { return frame; }                 // 0x10004ec0
RenderKernel* __cdecl TextureManager::GetRenderKernel() { return kernel; }                    // 0x10004aa0

void __cdecl TextureManager::RefreshFromChangedSettings()  // 0x10042c90
{
    textures.ForEach([](Texture* texture) {
        if (DisplayTexture* display = texture->GetDisplayTexture()) display->RefreshFromChangedSettings();
    });
}

// 0x10041730: names without a .texture/.movie extension get "-tex"; the table is keyed by "database:name"
Texture* __cdecl TextureManager::Create(const ResourceDatabase* db, const PString& name, const String& key)
{
    if (!db || !name.node || !name.node->length) return nullptr;
    PString n;
    File::Path path(name.node->text);
    if (path.IsExt("texture") || path.IsExt("movie")) {
        Assign(n, name);
    } else {
        PString t = Concat(name, "-tex");
        Assign(n, t);
    }
    PString k(n);
    k.node = PString::Cache()->Ins(k.node, ':', 0, 1);
    const Node* const dbName = *reinterpret_cast<Node* const*>(reinterpret_cast<const char*>(db) + 0xc);
    k.node = PString::Cache()->Ins(k.node, dbName ? reinterpret_cast<const unsigned char*>(dbName->text) : nullptr, 0);
    if (Texture* const* const slot = textures.Find(k.node, NameKey)) {
        (*slot)->AddReference();
        return *slot;
    }
    const Resource* const res = VCall<const Resource*>(db, 4, &n, 0);
    if (!res) return GetSubstituteTexture();
    res->SetScrambleKey(key);
    Texture* t = static_cast<const TextureResource*>(res)->CreateInstance(k, this);
    if (!t) t = GetSubstituteTexture();
    res->ReleaseReference();
    return t;
}

// 0x100421a0: the shared substitute texture, cached in SharedTextureCollection's slot (one reference there, one
// for the caller); a texture of that name already registered is taken, and found on the second look it is none
Texture* __cdecl TextureManager::GetSubstituteTexture()
{
    if (!useSubstitute) return nullptr;
    Texture*& cache = SubstituteTexture();
    Texture* r = cache;
    if (r) {
        r->AddReference();
    } else {
        r = Find(SubstituteName());
        if (r) {
            r->AddReference();
        } else {
            if (Find(SubstituteName())) return nullptr;
            DisplayTexture* const d = RsGenerateSubstituteTexture(KernelDisplay(kernel), 0, 0);
            if (!d) return nullptr;
            void* const mem = Alloc::New(sizeof(Texture));
            r = mem ? ::new (mem) Texture(SubstituteName(), this, d, static_cast<TextureAlpha>(0)) : nullptr;
        }
        Texture* const old = cache;
        cache = r;
        if (r) r->AddReference();
        if (old) old->ReleaseReference();
    }
    r->AddReference();
    if (r) r->ReleaseReference();
    return r;
}

// Retail's only accesses to these two fields are in TextureManager::Init.
unsigned int TextureManager::globalSkipLevels = 0;       // 0x100b808c
unsigned int TextureManager::globalSkipThreshold = 64;   // 0x100b1be8

// 0x10041db0: the texture defaults from the OS options (always true)
bool __cdecl TextureManager::Init(RenderKernel* k)
{
    kernel = k;
    const Options* const o = KernelOS(k)->GetOptions();
    if (!o) return true;
    o->Get("GlobalTextureResolution", globalSkipLevels, globalSkipLevels);
    o->Get("TextureResThreshold", globalSkipThreshold, globalSkipThreshold);
    PString format(kDefaultFormatName);
    o->Get("DefaultTextureFormat", format, format);
    for (const auto& entry : kFormatNames)
        if (!format.CompareIgnoreCase(entry.name)) {
            TextureResource::DISPLAYFORMATID_DEFAULT = static_cast<DFormatId>(entry.id);
            RsMirrorSync("?DISPLAYFORMATID_DEFAULT@TextureResource@Jet@@0W4DFormatId@2@A");
            break;
        }
    float aniso;
    if (o->Get("DefaultAnisotropy", aniso, TextureResource::ANISOTROPY_DEFAULT)) {
        Display* const d = KernelDisplay(k);
        if (aniso > MaxAnisotropy(d)) aniso = MaxAnisotropy(d);
        if (aniso >= 1.0f) {
            TextureResource::ANISOTROPY_DEFAULT = aniso;
            RsMirrorSync("?ANISOTROPY_DEFAULT@TextureResource@Jet@@0MA");
        }
    }
    PString filter;
    if (o->Get("DefaultTextureFilter", filter, filter)) {
        if (!filter.CompareIgnoreCase("none")) {
            TextureResource::MIN_FILTER_DEFAULT = static_cast<TextureFilter>(0);
            TextureResource::MIP_FILTER_DEFAULT = static_cast<TextureFilter>(0);
            TextureResource::MAG_FILTER_DEFAULT = static_cast<TextureFilter>(0);
            RsMirrorSync("?MIN_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A");
            RsMirrorSync("?MIP_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A");
            RsMirrorSync("?MAG_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A");
        } else {
            const bool trilinear = !filter.CompareIgnoreCase("trilinear");
            TextureResource::MIN_FILTER_DEFAULT = static_cast<TextureFilter>(1);
            TextureResource::MAG_FILTER_DEFAULT = static_cast<TextureFilter>(1);
            TextureResource::MIP_FILTER_DEFAULT = static_cast<TextureFilter>(trilinear ? 1 : 0);
            RsMirrorSync("?MIN_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A");
            RsMirrorSync("?MIP_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A");
            RsMirrorSync("?MAG_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A");
        }
    } else {
        if (o->Get("DefaultMinFilter", filter, filter))
            for (int i = 0; i < 2; ++i)
                if (!filter.CompareIgnoreCase(kFilterNames[i])) {
                    TextureResource::MIN_FILTER_DEFAULT = static_cast<TextureFilter>(i);
                    RsMirrorSync("?MIN_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A");
                    break;
                }
        if (o->Get("DefaultMipFilter", filter, filter))
            for (int i = 0; i < 2; ++i)
                if (!filter.CompareIgnoreCase(kFilterNames[i])) {
                    TextureResource::MIP_FILTER_DEFAULT = static_cast<TextureFilter>(i);
                    RsMirrorSync("?MIP_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A");
                    break;
                }
        if (o->Get("DefaultMagFilter", filter, filter))
            for (int i = 0; i < 2; ++i)
                if (!filter.CompareIgnoreCase(kFilterNames[i])) {
                    TextureResource::MAG_FILTER_DEFAULT = static_cast<TextureFilter>(i);
                    RsMirrorSync("?MAG_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A");
                    break;
                }
    }
    if (Query(KernelDisplay(k), 5)) {
        PString mip;
        if (o->Get("DefaultAutoMip", mip, mip))
            for (int i = 0; i < 4; ++i)
                if (!mip.CompareIgnoreCase(kAutoMipNames[i])) {
                    TextureResource::AUTO_MIP_DEFAULT = static_cast<TextureAutoMip>(i);
                    RsMirrorSync("?AUTO_MIP_DEFAULT@TextureResource@Jet@@0W4TextureAutoMip@2@A");
                    break;
                }
    } else {
        TextureResource::AUTO_MIP_DEFAULT = static_cast<TextureAutoMip>(0);
        RsMirrorSync("?AUTO_MIP_DEFAULT@TextureResource@Jet@@0W4TextureAutoMip@2@A");
    }
    o->Get("UseSubstituteTexture", useSubstitute, useSubstitute);
    return true;
}

}  // namespace Jet
