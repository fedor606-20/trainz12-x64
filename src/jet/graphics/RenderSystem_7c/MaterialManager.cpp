// Unit U7 of RenderSystem_7c: MaterialManager's lifetime, table and Create (0x10035510 ctor, 0x100355b0 dtor,
// 0x1000c920 Init, 0x10035670 Create, Add 0x10035ad0 / Remove 0x10035af0 / Find 0x10035b50 with the helpers
// 0x10035c80 / 0x10035e20 and the rings 0x1000d530 / 0x1000d5b0, and the static
// SplitDecoratedMaterialName 0x10035b80). Behaviour follows the TS12 assembly (addresses per function):
// Create looks the decorated name up in the table, splits it into type and name, then walks the material
// plugin types of that name (their IsSupported / CreateInstance) until one makes a material.
#include <jet/Material.h>
#include <jet/Plugin.h>
#include <jet/RenderSystem.h>
#include <jet/Texture.h>
#include <jet/UId.h>
#include <jet/VCall.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

MaterialManager* __cdecl RenderKernel::GetMaterialManager()
{
    return *reinterpret_cast<MaterialManager**>(reinterpret_cast<unsigned char*>(this) + 0x90);
}

void __cdecl RenderKernel::SetMaterialManager(MaterialManager* manager)
{
    auto& current = *reinterpret_cast<MaterialManager**>(reinterpret_cast<unsigned char*>(this) + 0x90);
    if (current) current->ReleaseReference();
    current = manager;
    if (manager) {
        manager->AddReference();
        VCall<bool>(manager, 0x08, this);
    }
}

// the material name state (MaterialFields.cpp, their own exports)
void __cdecl RsMatSetName(void* p, const PString* decorated, const PString* plain);
void __cdecl RsMatSetCreationType(void* p, PString* v);
void __cdecl RsMatSetRequestName(void* p, PString* v);

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

// a material's ReferenceCount is a virtual base (the original adjusts the same way: vbptr +4, the offset of
// the base at vbtable[1])
ReferenceCount* MaterialRefs(void* material)
{
    unsigned char* p = static_cast<unsigned char*>(material);
    void* const vbtable = *reinterpret_cast<void* const*>(p + 4);
    const int offset = *reinterpret_cast<const int*>(static_cast<unsigned char*>(vbtable) + 4);
    return reinterpret_cast<ReferenceCount*>(p + 4 + offset);
}

// the material plugin group (the original's shared static until U9)
const UId& MaterialPluginGroupUID()
{
    static const UId* const uid = reinterpret_cast<const UId*>(
        GetProcAddress(GetModuleHandleA("RenderSystem_7c_orig.dll"),
                       "?PluginGroupUID@MaterialPlugin@Jet@@2VUId@2@A"));
    return *uid;
}

struct PluginAccess : PluginBase {
    using PluginBase::LookupPluginType;
};

// 0x100335d0: the material plugin type named `name` (nothing when the lookup fails)
PluginType* LookupMaterialPlugin(const PString& name)
{
    PluginType* found = nullptr;
    if (PluginAccess::LookupPluginType(&found, MaterialPluginGroupUID(), name).code == 0) return found;
    return nullptr;
}

// 0x10035eb0: the first `length` characters of `text` (clipped to the text), the old node going first
void SetPrefix(PString& out, const char* text, unsigned length)
{
    if (out.node) PString::Cache()->Destroy(out.node);
    const unsigned textLength = static_cast<unsigned>(std::strlen(text));
    if (length > textLength) length = textLength;
    out.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(text), length);
}

// the shared static block (the display format names; the split only reads its first slot, an empty PString
// standing for "no type"). The original makes it from its module's CRT initialisers (FUN_1003e060 runs off
// FUN_100351d0's guard, which the startup table already tripped), so it is a namespace-scope static here too:
// the cache nodes exist before any caller, not on the first split.
struct SplitBlock {
    PString names[19];
    SplitBlock()
    {
        static const char* const texts[18] = {
            "Materials", "ARGB8888", "ABGR8888", "RGB0888", "BGR0888", "RGB888", "BGR888", "ARGB4444",
            "RGB565",    "RGB555",   "DXT1",     "DXT3",   "DXT5",   "ATC",   "ATCE",   "ATCI",
            "ETC1",      "NULL"};
        for (unsigned i = 0; i < 18; ++i)
            names[i + 1].node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(texts[i]));
    }
};
const SplitBlock splitStatics;
const SplitBlock& SplitStatics() { return splitStatics; }

// the manager's virtuals used here: 4 Add (the virtuals are __cdecl with `this` first)
void ManagerAdd(void* manager, void* material) { VCall<void>(manager, 0x10, material); }

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

// ---------------------------------------------------------------------------------------------------------------
// Create

// 0x10035b80: the type is the text after the last dot, the name the lowercased prefix before it minus the
// "m" of the "m." tag ("roofm.onetex" gives type "onetex", name "roof")
MaterialManager::Result MaterialManager::SplitDecoratedMaterialName(const PString& decMaterialName,
                                                                    PString& materialType,
                                                                    PString& materialName)
{
    const char* text = decMaterialName.node ? decMaterialName.node->text : nullptr;
    if (!text) return Result(5);
    const char* dot = std::strrchr(text, '.');
    if (!dot) return Result(6);
    if (dot == text) return Result(6);
    SetPrefix(materialName, text, static_cast<unsigned>(dot - text - 1));
    materialName.node = PString::Cache()->ToLower(materialName.node);
    if (materialType.node) PString::Cache()->Destroy(materialType.node);
    materialType.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(dot + 1));
    SplitStatics();
    return materialType.node ? Result(0) : Result(6);
}

// 0x10035670: a material of the decorated name is handed back when the table has one, else the split type
// walks its plugin types (IsSupported per display, the fallback name retried) until one creates a material
MaterialManager::Result MaterialManager::Create(Material*& material, const PString& decName)
{
    MatRing* ring;
    unsigned index;
    if (FindItem(Table(this), decName.node, ring, index)) {
        material = static_cast<Material*>(ring->items[index]);
        MaterialRefs(material)->AddReference();
        return Result(0);
    }
    PString type, plain, lookup;
    const Result split = SplitDecoratedMaterialName(decName, type, plain);
    if (split.code != 0) return split;
    material = nullptr;
    lookup = type;
    int skipped = 0;
    bool found = false;
    for (;;) {
        PluginType* plugin = LookupMaterialPlugin(lookup);
        if (!plugin) break;
        bool stop = false;
        {
            PString current = lookup;
            bool supported;
            if (!kernel) {
                supported = true;
            } else {
                void* const display = Field<void*>(kernel, 0x7c);
                supported = VCall<unsigned char>(plugin, 0x10, display, &lookup) != 0;
            }
            if (supported) {
                material = static_cast<Material*>(VCall<void*>(plugin, 0x14, this, kernel));
                found = true;
            } else {
                ++skipped;
                // a fallback name that is new keeps the walk going; the same one (or none) stops it
                stop = !lookup.node || !lookup.node->text || lookup.node == current.node;
            }
        }
        plugin->ReleaseReference();
        if (found || stop) break;
    }
    if (material) {
        if (skipped > 0) Field<unsigned char>(material, 0x84) |= 8;
        RsMatSetCreationType(material, &type);
        RsMatSetName(material, &decName, &plain);
        RsMatSetRequestName(material, const_cast<PString*>(&decName));
        MaterialManager::Add(material);
        return Result(0);
    }
    return Result(lookup.node == type.node ? 2 : 3);
}

}  // namespace Jet
