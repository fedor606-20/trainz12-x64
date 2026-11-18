// Unit U7 of RenderSystem_7c: MaterialManager's lifetime and table (0x10035510 ctor, 0x100355b0 dtor,
// 0x1000c920 Init, Add 0x10035ad0 / Remove 0x10035af0 / Find 0x10035b50 with the helpers 0x10035c80 /
// 0x10035cf0 / 0x10035e20 and the rings 0x1000d530 / 0x1000d5b0). Behaviour follows the TS12 assembly
// (addresses per function). Create (0x10035670) still runs the original: its plugin-type walk is not
// reconstructed yet.
#include <jet/Material.h>
#include <jet/Texture.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {
namespace {

template <class T>
T& Field(void* p, unsigned offset) { return *reinterpret_cast<T*>(static_cast<unsigned char*>(p) + offset); }

// the manager's table: {buckets, count, size} over 97 FastDynamicArray buckets of Material*, keyed by the
// decorated name node at +0x24; the ring chunks hold 13 items (0x40). A view of MaterialManager::Materials.
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
static_assert(sizeof(MatRing) == 0x40 && sizeof(MatTable) == sizeof(MaterialManager::Materials));

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

// ---------------------------------------------------------------------------------------------------------------
// lifetime

// 0x10035510: the default type's "notex" node (0x100714f0), an empty table, then the texture manager
// referenced and no kernel yet
MaterialManager::MaterialManager(TextureManager* t) : defaultType("notex")
{
    textures = t;
    t->AddReference();  // (the original does not test for null)
    kernel = nullptr;
}

// 0x100355b0: the table emptied, the texture manager released; the members go in reverse order after that
// (the table's buckets last), then the reference count base
MaterialManager::~MaterialManager()
{
    materials.Clear();
    if (textures) textures->ReleaseReference();
}

// 0x1000c920
bool __cdecl MaterialManager::Init(RenderKernel* k)
{
    kernel = k;
    return true;
}

// 0x10035670: still the original's (the plugin-type walk and the factories' CreateInstance are not
// reconstructed yet); this slot reaches the same code the forwarded export does
MaterialManager::Result MaterialManager::Create(Material*& material, const PString& name)
{
    using F = int*(__cdecl*)(void*, int*, void*, const void*);
    static F const f = reinterpret_cast<F>(GetProcAddress(
        GetModuleHandleA("RenderSystem_7c_orig.dll"),
        "?Create@MaterialManager@Jet@@UAA?AV?$ResultClass@$1?results@MaterialManager@Jet@@2QBUResultTable@2@"
        "BW4Results@MaterialManager@Jet@@@2@AAPAVMaterial@2@ABVPString@2@@Z"));
    Result result;
    f(this, &result.code, &material, &name);
    return result;
}

// ---------------------------------------------------------------------------------------------------------------
// the table

// 0x10035ad0
void __cdecl MaterialManager::Add(Material* material)
{
    MatTable& t = Table(this);
    ++t.count;
    BucketAppend(t.buckets[reinterpret_cast<unsigned int>(Field<void*>(material, 0x24)) % t.size], material);
}

// 0x10035af0: 0 when the entry went, 4 when the table has no such name
MaterialManager::Result MaterialManager::Remove(Material* material)
{
    MatTable& t = Table(this);
    void* node = Field<void*>(material, 0x24);
    MatRing* ring;
    unsigned index;
    if (FindItem(t, node, ring, index)) {
        --t.count;
        BucketRemoveAt(t.buckets[reinterpret_cast<unsigned int>(node) % t.size], ring, index);
        return Result(0);
    }
    return Result(4);
}

// 0x10035b50
Material* __cdecl MaterialManager::Find(const PString& name)
{
    MatTable& t = Table(this);
    MatRing* ring;
    unsigned index;
    void* node = name.node;
    return FindItem(t, node, ring, index) ? static_cast<Material*>(ring->items[index]) : nullptr;
}

}  // namespace Jet
