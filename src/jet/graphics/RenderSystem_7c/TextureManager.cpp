// Unit U6d1a of RenderSystem_7c: TextureManager's containers and lifetime (0x10041420..0x10042361, the helpers
// 0x10042d00..0x10043187 and the folded getters 0x10004aa0 / 0x10004eb0 / 0x10004ec0). Behaviour follows the
// TS12 assembly (addresses per function). The streaming half (Update, UpSizeTexture, the async and down-size
// queues, CleanupAsyncTextureLoad) is U6d1b: until then those calls go to the original's exports. The manager's
// statics, the TextureResource defaults Init sets, the format-name table (0x100b8780, built by the original's
// initializer) and SharedTextureCollection's substitute texture and name (0x100b8708 / 0x100b8734) stay the
// original's.
#include <jet/KernelResources.h>
#include <jet/Texture.h>
#include <jet/VCall.h>

#include <windows.h>

#include <new>

namespace Jet {

namespace {

using Node = PStringCache::Node;

unsigned char* OrigBase()
{
    static unsigned char* const base = reinterpret_cast<unsigned char*>(GetModuleHandleA("RenderSystem_7c_orig.dll"));
    return base;
}

template <class F>
F Orig(const char* name)
{
    return reinterpret_cast<F>(GetProcAddress(reinterpret_cast<HMODULE>(OrigBase()), name));
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

// the original's 16 {name, format} pairs (0x100b8780) and the PString holding the default format's name
// (0x100351d0 builds it once; the name at +8)
struct FormatName {
    const PString* name;
    DFormatId id;
};
const FormatName* FormatNames() { return reinterpret_cast<const FormatName*>(OrigBase() + 0xb8780); }
const PString& DefaultFormatName()
{
    using F = unsigned char*(__cdecl*)();
    return *reinterpret_cast<const PString*>(reinterpret_cast<F>(OrigBase() + 0x351d0)() + 8);
}

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

void __cdecl TextureManager::SetEnableTextureStreaming(bool on) { TEXTURE_STREAMING_ENABLED = on; }  // 0x10041490

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
// the forwarded streaming half (U6d1b)

void __cdecl TextureManager::Update()  // 0x10042480
{
    static auto const f = Orig<void(__cdecl*)(TextureManager*)>("?Update@TextureManager@Jet@@IAAXXZ");
    f(this);
}

bool __cdecl TextureManager::RemovedAsyncTextureLoad(const TextureRes* t)  // 0x100423e0
{
    static auto const f = Orig<bool(__cdecl*)(TextureManager*, const TextureRes*)>(
        "?RemovedAsyncTextureLoad@TextureManager@Jet@@IAA_NPBVTextureRes@2@@Z");
    return f(this, t);
}

bool __cdecl TextureManager::RemovedTextureDownSize(const TextureRes* t)  // 0x10042980
{
    static auto const f = Orig<bool(__cdecl*)(TextureManager*, const TextureRes*)>(
        "?RemovedTextureDownSize@TextureManager@Jet@@IAA_NPBVTextureRes@2@@Z");
    return f(this, t);
}

void __cdecl TextureManager::UpSizeTexture(const TextureRes* t, int size)  // 0x10042720
{
    static auto const f = Orig<void(__cdecl*)(TextureManager*, const TextureRes*, int)>(
        "?UpSizeTexture@TextureManager@Jet@@QAAXPBVTextureRes@2@H@Z");
    f(this, t, size);
}

void TextureManager::CleanupAsyncTextureLoad()  // 0x10042690
{
    static auto const f =
        Orig<void(__fastcall*)(TextureManager*, void*)>("?CleanupAsyncTextureLoad@TextureManager@Jet@@IAEXXZ");
    f(this, nullptr);
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
        if (r->asyncInfo) {
            const int state = *reinterpret_cast<const int*>(static_cast<char*>(r->asyncInfo) + 0x14);
            if (state == 2)
                RemovedAsyncTextureLoad(r);
            else if (state == 1)
                RemovedTextureDownSize(r);
        }
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
            static auto const gen = Orig<DisplayTexture*(__cdecl*)(Display*, unsigned int, unsigned int)>(
                "?GenerateSubstituteLoadTexture@SharedTextureCollection@Jet@@KAPAVDisplayTexture@2@PAVDisplay@2@II@Z");
            DisplayTexture* const d = gen(KernelDisplay(kernel), 0, 0);
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

// 0x10041db0: the texture defaults from the OS options (always true)
bool __cdecl TextureManager::Init(RenderKernel* k)
{
    kernel = k;
    const Options* const o = KernelOS(k)->GetOptions();
    if (!o) return true;
    o->Get("GlobalTextureResolution", globalSkipLevels, globalSkipLevels);
    o->Get("TextureResThreshold", globalSkipThreshold, globalSkipThreshold);
    PString format(DefaultFormatName());
    o->Get("DefaultTextureFormat", format, format);
    for (int i = 0; i < 16; ++i)
        if (!FormatNames()[i].name->CompareIgnoreCase(format)) {
            TextureResource::DISPLAYFORMATID_DEFAULT = FormatNames()[i].id;
            break;
        }
    float aniso;
    if (o->Get("DefaultAnisotropy", aniso, TextureResource::ANISOTROPY_DEFAULT)) {
        Display* const d = KernelDisplay(k);
        if (aniso > MaxAnisotropy(d)) aniso = MaxAnisotropy(d);
        if (aniso >= 1.0f) TextureResource::ANISOTROPY_DEFAULT = aniso;
    }
    PString filter;
    if (o->Get("DefaultTextureFilter", filter, filter)) {
        if (!filter.CompareIgnoreCase("none")) {
            TextureResource::MIN_FILTER_DEFAULT = static_cast<TextureFilter>(0);
            TextureResource::MIP_FILTER_DEFAULT = static_cast<TextureFilter>(0);
            TextureResource::MAG_FILTER_DEFAULT = static_cast<TextureFilter>(0);
        } else {
            const bool trilinear = !filter.CompareIgnoreCase("trilinear");
            TextureResource::MIN_FILTER_DEFAULT = static_cast<TextureFilter>(1);
            TextureResource::MAG_FILTER_DEFAULT = static_cast<TextureFilter>(1);
            TextureResource::MIP_FILTER_DEFAULT = static_cast<TextureFilter>(trilinear ? 1 : 0);
        }
    } else {
        if (o->Get("DefaultMinFilter", filter, filter))
            for (int i = 0; i < 2; ++i)
                if (!filter.CompareIgnoreCase(kFilterNames[i])) {
                    TextureResource::MIN_FILTER_DEFAULT = static_cast<TextureFilter>(i);
                    break;
                }
        if (o->Get("DefaultMipFilter", filter, filter))
            for (int i = 0; i < 2; ++i)
                if (!filter.CompareIgnoreCase(kFilterNames[i])) {
                    TextureResource::MIP_FILTER_DEFAULT = static_cast<TextureFilter>(i);
                    break;
                }
        if (o->Get("DefaultMagFilter", filter, filter))
            for (int i = 0; i < 2; ++i)
                if (!filter.CompareIgnoreCase(kFilterNames[i])) {
                    TextureResource::MAG_FILTER_DEFAULT = static_cast<TextureFilter>(i);
                    break;
                }
    }
    if (Query(KernelDisplay(k), 5)) {
        PString mip;
        if (o->Get("DefaultAutoMip", mip, mip))
            for (int i = 0; i < 4; ++i)
                if (!mip.CompareIgnoreCase(kAutoMipNames[i])) {
                    TextureResource::AUTO_MIP_DEFAULT = static_cast<TextureAutoMip>(i);
                    break;
                }
    } else {
        TextureResource::AUTO_MIP_DEFAULT = static_cast<TextureAutoMip>(0);
    }
    o->Get("UseSubstituteTexture", useSubstitute, useSubstitute);
    return true;
}

}  // namespace Jet
