// Unit U7 of RenderSystem_7c: the material state the manager and the config extractors build on. The leaf
// accessors (0x10004330 / 0x10004340, 0x1000c860, 0x1000cab0, 0x100106c0, 0x10023b90), SetTextureDefinition
// (0x1000c030), Equals (0x10033010 and its NullMaterial twin 0x10037910), InitInstance (0x100331b0 and its
// NullMaterial twin 0x10037a20), Rename (0x100328f0), VerifyDecoratedName (0x10032df0) and the NullMaterial
// SetTexture pair (0x100379c0 / 0x100379f0). Behaviour follows the TS12 assembly (addresses per function).
// The uniqueMatId counter and the DecoratedTypeToken name stay the original's shared statics (read through
// their forwarded exports), like the other module statics until U9.
#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/RenderTypes.h>
#include <jet/VCall.h>

#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

namespace Jet {
void __cdecl RsMatSetName(void* p, const PString* decorated, const PString* plain);

namespace {

template <class T>
T& Field(void* p, unsigned offset) { return *reinterpret_cast<T*>(static_cast<unsigned char*>(p) + offset); }

struct PropertyDef {
    PString key, value;
};
struct TextureDefItem {
    int type;
    PString name;
    float weight;
};

// the ring chunks of both lists: {next, prev, count, 7 items}
template <class T>
struct RingChunk {
    RingChunk* next;
    RingChunk* prev;
    unsigned count;
    alignas(T) unsigned char bytes[7 * sizeof(T)];
    T* item(unsigned i) const { return reinterpret_cast<T*>(const_cast<unsigned char*>(bytes)) + i; }
};
template <class T>
RingChunk<T>* Head(void* object, unsigned offset) { return Field<RingChunk<T>*>(object, offset); }

// 0x1000d8e0 / 0x1000d6b0: a new tail chunk when the last one is full (7 items), then a copy of `item`
template <class T>
void AppendItem(void* object, unsigned offset, const T& item)
{
    auto*& head = Field<RingChunk<T>*>(object, offset);
    auto* tail = head ? head->prev : nullptr;
    if (!tail || tail->count == 7) {
        auto* chunk = static_cast<RingChunk<T>*>(Alloc::New(sizeof(RingChunk<T>)));
        chunk->count = 0;
        if (!head) {
            head = chunk;
            chunk->next = chunk->prev = chunk;
        } else {
            chunk->next = head;
            chunk->prev = tail;
            tail->next = chunk;
            head->prev = chunk;
        }
        tail = chunk;
    }
    T* target = tail->item(tail->count++);
    ::new (target) T(item);
}

// 0x1000ddd0: the old node goes first
void Assign(PString& dst, const PString& src)
{
    if (dst.node) PString::Cache()->Destroy(dst.node);
    ::new (&dst) PString(src);
}

unsigned char* OrigBase()
{
    static unsigned char* const base = reinterpret_cast<unsigned char*>(GetModuleHandleA("RenderSystem_7c_orig.dll"));
    return base;
}

// the original's statics that stay shared until U9
unsigned long& UniqueMatId()
{
    static unsigned long* const p = reinterpret_cast<unsigned long*>(
        GetProcAddress(reinterpret_cast<HMODULE>(OrigBase()), "?uniqueMatId@Material@Jet@@0KA"));
    return *p;
}
const PString& DecoratedTypeToken()
{
    static const PString* const p = reinterpret_cast<const PString*>(
        GetProcAddress(reinterpret_cast<HMODULE>(OrigBase()), "?DecoratedTypeToken@Material@Jet@@2VPString@2@A"));
    return *p;
}

// the manager's vtable slots used here: 4 Add, 5 Remove, 6 Find (the virtuals are __cdecl with `this` first)
void ManagerAdd(void* manager, void* material) { VCall<void>(manager, 0x10, material); }
void ManagerRemove(void* manager, void* material)
{
    int result = -1;
    VCall<int*>(manager, 0x14, &result, material);
}
void* ManagerFind(void* manager, const void* name) { return VCall<void*>(manager, 0x18, name); }

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// the leaf accessors

// 0x10004330 (shared with Display::GetNormalLength)
float __cdecl RsMatShininess(void* p) { return Field<float>(p, 0x74); }

// 0x10004340 (shared with Display::SetNormalLength)
void __cdecl RsMatSetShininess(void* p, float value) { Field<float>(p, 0x74) = value; }

// 0x10023b90 (shared with the chunks' GetNumPasses/GetNumStreams)
unsigned __cdecl RsMatNumPasses(void*) { return 1; }

// 0x1000c860: always "loaded"
int* __cdecl RsMatIsLoaded(void*, int* result)
{
    *result = 0;
    return result;
}

// 0x1000cab0 (shared with the small getters)
void* __cdecl RsMatManagerKernel(void* p) { return Field<void*>(p, 8); }

// 0x100106c0 (shared with the small getters)
void* __cdecl RsMatManagerTextures(void* p) { return Field<void*>(p, 0xc); }

// ---------------------------------------------------------------------------------------------------------------
// Equals

// 0x10033010: the display properties and the scalar state, then both lists item by item. The property walk
// compares while both cursors run (a length difference falls through to the texture walk, which answers true
// as soon as one side ends).
bool __cdecl RsMatEquals(void* self, void* other)
{
    if (Field<DisplayMaterial>(self, 0x34).operator!=(Field<DisplayMaterial>(other, 0x34))) return false;
    if (Field<int>(self, 0x78) != Field<int>(other, 0x78)) return false;
    if (Field<int>(self, 0x7c) != Field<int>(other, 0x7c)) return false;
    if (Field<float>(self, 0x80) != Field<float>(other, 0x80)) return false;
    if ((Field<unsigned char>(self, 0x84) ^ Field<unsigned char>(other, 0x84)) & 1) return false;
    if (Field<float>(self, 0x94) != Field<float>(other, 0x94)) return false;
    if (Field<int>(self, 0x8c) != Field<int>(other, 0x8c)) return false;
    // the properties (8-byte {key, value} items), 0x1000d880 / 0x10033530
    const RingChunk<PropertyDef>* chunkA = Head<PropertyDef>(self, 0x88);
    const RingChunk<PropertyDef>* chunkB = Head<PropertyDef>(other, 0x88);
    const RingChunk<PropertyDef>* firstA = chunkA;
    const RingChunk<PropertyDef>* firstB = chunkB;
    const PropertyDef* itemA = chunkA ? chunkA->item(0) : nullptr;
    const PropertyDef* itemB = chunkB ? chunkB->item(0) : nullptr;
    while (itemA && itemB) {
        if (itemA->key.node != itemB->key.node) return false;
        if (itemA->value.node != itemB->value.node) return false;
        if (++itemA >= chunkA->item(chunkA->count)) {
            chunkA = chunkA->next;
            itemA = chunkA == firstA ? nullptr : chunkA->item(0);
        }
        if (++itemB >= chunkB->item(chunkB->count)) {
            chunkB = chunkB->next;
            itemB = chunkB == firstB ? nullptr : chunkB->item(0);
        }
    }
    // the texture definitions (12-byte {type, name, weight} items), 0x1000d390
    const RingChunk<TextureDefItem>* texA = Head<TextureDefItem>(self, 0x90);
    const RingChunk<TextureDefItem>* texB = Head<TextureDefItem>(other, 0x90);
    const RingChunk<TextureDefItem>* firstTexA = texA;
    const RingChunk<TextureDefItem>* firstTexB = texB;
    const TextureDefItem* iA = texA ? texA->item(0) : nullptr;
    const TextureDefItem* iB = texB ? texB->item(0) : nullptr;
    for (;;) {
        if (!iA || !iB) return true;
        if (iA->type != iB->type || iA->name.node != iB->name.node || iA->weight != iB->weight) return false;
        if (++iA >= texA->item(texA->count)) {
            texA = texA->next;
            iA = texA == firstTexA ? nullptr : texA->item(0);
        }
        if (++iB >= texB->item(texB->count)) {
            texB = texB->next;
            iB = texB == firstTexB ? nullptr : texB->item(0);
        }
    }
}

// 0x10037910: the same with the type name compared first
bool __cdecl RsNullMatEquals(void* self, void* other)
{
    if (Field<PString>(self, 0x9c).node != Field<PString>(other, 0x9c).node) return false;
    return RsMatEquals(self, other);
}

// ---------------------------------------------------------------------------------------------------------------
// SetTextureDefinition and the NullMaterial SetTexture pair

// 0x1000c030: a texture definition is replaced by its type when one is already there (the old name node goes
// first, as everywhere in the module), else appended; only the append bumps the count at +0x8c
void __cdecl RsMatSetTextureDefinition(void* self, int type, const void* name, float weight)
{
    const PString& newName = *static_cast<const PString*>(name);
    auto* chunk = Head<TextureDefItem>(self, 0x90);
    if (chunk) {
        auto* first = chunk;
        do {
            for (unsigned i = 0; i < chunk->count; ++i) {
                TextureDefItem* item = chunk->item(i);
                if (item->type == type) {
                    if (item->name.node) PString::Cache()->Destroy(item->name.node);
                    ::new (&item->name) PString(newName);
                    item->weight = weight;
                    return;
                }
            }
            chunk = chunk->next;
        } while (chunk != first);
    }
    TextureDefItem item{type, newName, weight};
    AppendItem<TextureDefItem>(self, 0x90, item);
    ++Field<int>(self, 0x8c);
}

// 0x100379c0: the name form keeps only the definition
int* __cdecl RsNullMatSetTextureNamed(void* self, int* result, int type, const void*, const void* name, float weight,
                                      const void*)
{
    RsMatSetTextureDefinition(self, type, name, weight);
    *result = 0;
    return result;
}

// 0x100379f0: the texture's name at +0x10
int* __cdecl RsNullMatSetTextureRef(void* self, int* result, int type, const void* texture, float weight)
{
    const void* name = static_cast<const char*>(*static_cast<void* const*>(texture)) + 0x10;
    RsMatSetTextureDefinition(self, type, name, weight);
    *result = 0;
    return result;
}

// ---------------------------------------------------------------------------------------------------------------
// InitInstance and Rename

// 0x100331b0 (NullMaterial twin at 0x10037a20): the instance takes the prototype's state and a fresh unique
// name (a name that already carries the "xx..%%" prefix keeps its tail; anything else gets "id%%%%name"),
// then joins the prototype's manager
void __cdecl RsMatInitInstance(void* self, void* dest)
{
    PString& sourceName = Field<PString>(self, 0x24);
    PString& destName = Field<PString>(dest, 0x24);
    const unsigned long id = UniqueMatId()++;
    const int marker = sourceName.node ? PString::Cache()->Find(sourceName.node, reinterpret_cast<const unsigned char*>("%%"), 8) : -1;
    if (marker == 8) {
        Assign(destName, sourceName);
        destName.node = PString::Cache()->Del(destName.node, 0, 8);
        char text[16];
        std::snprintf(text, sizeof text, "%08x", id);
        destName.node = PString::Cache()->Ins(destName.node, reinterpret_cast<const unsigned char*>(text), 0);
    } else {
        // the text of a name without a node goes to Setf as null, like the original's
        destName.Setf("%08x%%%%%s", id, sourceName.node ? sourceName.Text() : nullptr);
    }
    Assign(Field<PString>(dest, 0x28), Field<PString>(self, 0x28));
    Assign(Field<PString>(dest, 0x30), Field<PString>(self, 0x30));
    std::memcpy(static_cast<unsigned char*>(dest) + 0x34, static_cast<unsigned char*>(self) + 0x34, 0x44);
    Field<unsigned>(dest, 0x78) = Field<unsigned>(self, 0x78);
    Field<unsigned>(dest, 0x7c) = Field<unsigned>(self, 0x7c);
    Field<unsigned>(dest, 0x80) = Field<unsigned>(self, 0x80);
    Field<unsigned char>(dest, 0x84) ^= (Field<unsigned char>(dest, 0x84) ^ Field<unsigned char>(self, 0x84)) & 1;
    Field<float>(dest, 0x94) = Field<float>(self, 0x94);
    Assign(Field<PString>(dest, 0x2c), Field<PString>(self, 0x2c));
    Field<unsigned char>(dest, 0x84) ^= (Field<unsigned char>(dest, 0x84) ^ Field<unsigned char>(self, 0x84)) & 2;
    auto* properties = Head<PropertyDef>(self, 0x88);
    if (properties) {
        auto* first = properties;
        do {
            for (unsigned i = 0; i < properties->count; ++i) AppendItem<PropertyDef>(dest, 0x88, *properties->item(i));
            properties = properties->next;
        } while (properties != first);
    }
    Field<int>(dest, 0x8c) = Field<int>(self, 0x8c);
    auto* textures = Head<TextureDefItem>(self, 0x90);
    if (textures) {
        auto* first = textures;
        do {
            for (unsigned i = 0; i < textures->count; ++i) AppendItem<TextureDefItem>(dest, 0x90, *textures->item(i));
            textures = textures->next;
        } while (textures != first);
    }
    if (Field<void*>(self, 0x98)) ManagerAdd(Field<void*>(self, 0x98), dest);
}

// 0x100328f0: the new decorated name must be free in the manager; the material leaves the manager, takes the
// new names and joins again
bool __cdecl RsMatRename(void* self, const void* decorated, const void* plain)
{
    void* manager = Field<void*>(self, 0x98);
    if (!manager) return false;
    if (ManagerFind(manager, decorated)) return false;
    ManagerRemove(manager, self);
    RsMatSetName(self, static_cast<const PString*>(decorated), static_cast<const PString*>(plain));
    ManagerAdd(manager, self);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// VerifyDecoratedName

// 0x10032df0: a name that already carries the "m." token is left alone; else the material's tag is appended
// ("m.notex" / "m.onetex" from the hint alone, else by the texture list: reflect -> gloss/reflect by opacity,
// a diffuse map named "sb_*" / "tb_*" -> billboard tags, else the diffuse count)
void __cdecl RsMatVerifyDecoratedName(void* name, unsigned hint, const void* textures)
{
    AnsiString& text = *static_cast<AnsiString*>(name);
    text.ToLower();
    const PString& token = DecoratedTypeToken();
    const char* marker = token.node ? token.node->text : nullptr;
    if (text.Find(marker, 0) != -1) return;
    const char* tag;
    if (hint == 0) {
        tag = "m.notex";
    } else if (hint == 1) {
        tag = "m.onetex";
    } else {
        bool reflect = false, opacity = false, billboard = false, tribillboard = false, diffuse = false;
        const auto* chunk = *static_cast<RingChunk<TextureDefItem> const* const*>(textures);
        if (chunk) {
            const auto* first = chunk;
            do {
                for (unsigned i = 0; i < chunk->count; ++i) {
                    const TextureDefItem* item = chunk->item(i);
                    switch (item->type) {
                    case 1:
                        diffuse = true;
                        if (item->name.node && PString::Cache()->Find(item->name.node, reinterpret_cast<const unsigned char*>("sb_"), 0) != -1)
                            billboard = true;
                        else if (item->name.node && PString::Cache()->Find(item->name.node, reinterpret_cast<const unsigned char*>("tb_"), 0) != -1)
                            tribillboard = true;
                        break;
                    case 6:
                        opacity = true;
                        break;
                    case 9:
                        reflect = true;
                        break;
                    }
                }
                chunk = chunk->next;
            } while (chunk != first);
        }
        if (reflect)
            tag = opacity ? "m.gloss" : "m.reflect";
        else if (billboard)
            tag = "m.billboard";
        else if (tribillboard)
            tag = "m.tribillboard";
        else
            tag = diffuse ? "m.onetex" : "m.notex";
    }
    text.Ins(tag, text.Length());
}

// ---------------------------------------------------------------------------------------------------------------
// the manager's table (Add 0x10035ad0 / Remove 0x10035af0 / Find 0x10035b50, helpers 0x10035c80 / 0x10035cf0 /
// 0x10035e20 with the rings 0x1000d530 / 0x1000d5b0): the table at +0x14 is {buckets, count, size} over 97
// FastDynamicArray buckets of Material*, keyed by the decorated name node at +0x24; the ring chunks hold 13
// items (0x40). Add inserts as it finds it (duplicates stay), Remove takes the first entry with the same name
// node and compacts the chunk (a chunk of one goes, the head chunk moving on)

namespace {
struct MatRing {
    MatRing* next;
    MatRing* prev;
    unsigned count;
    void* items[13];
};
struct MatBucket {
    MatRing* head;
};
struct MatTable {
    MatBucket* buckets;
    unsigned count;
    unsigned size;
};
MatTable& Table(void* manager) { return Field<MatTable>(manager, 0x14); }

void BucketAppend(MatBucket& b, void* material)
{
    MatRing*& head = b.head;
    MatRing* tail = head ? head->prev : nullptr;
    if (!tail || tail->count == 13) {
        auto* ring = static_cast<MatRing*>(Alloc::New(sizeof(MatRing)));
        ring->count = 0;
        if (!head) {
            head = ring;
            ring->next = ring->prev = ring;
        } else {
            ring->next = head;
            ring->prev = tail;
            tail->next = ring;
            head->prev = ring;
        }
        tail = ring;
    }
    tail->items[tail->count++] = material;
}

// the first entry whose material's name node is `node`
bool FindItem(MatTable& t, void* node, MatRing*& outRing, unsigned& outIndex)
{
    MatRing* first = t.buckets[reinterpret_cast<unsigned int>(node) % t.size].head;
    if (!first) return false;
    MatRing* ring = first;
    do {
        for (unsigned i = 0; i < ring->count; ++i)
            if (Field<void*>(ring->items[i], 0x24) == node) {
                outRing = ring;
                outIndex = i;
                return true;
            }
        ring = ring->next;
    } while (ring != first);
    return false;
}

void BucketRemoveAt(MatBucket& b, MatRing* ring, unsigned index)
{
    void** at = &ring->items[index];
    void** last = &ring->items[ring->count - 1];
    if (at < last) std::memmove(at, at + 1, reinterpret_cast<char*>(last) - reinterpret_cast<char*>(at));
    if (ring->count == 1) {
        MatRing*& head = b.head;
        MatRing* next = ring->next;
        ring->prev->next = ring->next;
        ring->next->prev = ring->prev;
        const bool wasHead = ring == head;
        Alloc::Delete(ring);
        if (wasHead) head = next == ring ? nullptr : next;
    } else {
        --ring->count;
    }
}
}  // namespace

// 0x10035ad0
void __cdecl RsMatManagerAdd(void* self, void* material)
{
    MatTable& t = Table(self);
    ++t.count;
    BucketAppend(t.buckets[reinterpret_cast<unsigned int>(Field<void*>(material, 0x24)) % t.size], material);
}

// 0x10035af0: 0 when the entry went, 4 when the table has no such name
int* __cdecl RsMatManagerRemove(void* self, int* result, void* material)
{
    MatTable& t = Table(self);
    void* node = Field<void*>(material, 0x24);
    MatRing* ring;
    unsigned index;
    if (FindItem(t, node, ring, index)) {
        --t.count;
        BucketRemoveAt(t.buckets[reinterpret_cast<unsigned int>(node) % t.size], ring, index);
        *result = 0;
    } else {
        *result = 4;
    }
    return result;
}

// 0x10035b50
void* __cdecl RsMatManagerFind(void* self, const void* name)
{
    MatTable& t = Table(self);
    MatRing* ring;
    unsigned index;
    void* node = *static_cast<void* const*>(name);
    return FindItem(t, node, ring, index) ? ring->items[index] : nullptr;
}

}  // namespace Jet
