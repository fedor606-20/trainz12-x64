// RenderSystem_7c unit U7 (notes/RenderSystem.md), the material manager and the material config extractors.
// TS12 layout and slot order (vftable 0x100714fc): the hash table of materials keyed by their decorated name
// node, the texture manager it hands out and the default material type ("notex" until set).
#pragma once

#include <jet/FastDynamicArray.h>
#include <jet/GfxLanguage.h>
#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/RenderTypes.h>
#include <jet/Result.h>

#include <cstdint>
#include <new>

namespace Jet {

class ConfigData;
class Material;
class MaterialManager;
class RenderKernel;
class ResourceDatabase;
class TextureManager;

class MaterialManager : public ReferenceCount {
public:
    enum Results {};
    static __declspec(dllimport) const ResultTable* const results;
    using Result = ResultClass<&results, Results>;

    explicit MaterialManager(TextureManager* textures);               // ??0MaterialManager@Jet@@QAE@PAVTextureManager@1@@Z
    ~MaterialManager() override;                                      // 1 (0 is ReferenceCount::DeleteThis)

    virtual bool __cdecl Init(RenderKernel* kernel);                  // 2 (0x1000c920)
    virtual Result __cdecl Create(Material*& material, const PString& name);  // 3 (0x10035670)
    virtual void __cdecl Add(Material* material);                     // 4 (0x10035ad0)
    virtual Result __cdecl Remove(Material* material);                // 5 (0x10035af0)
    virtual Material* __cdecl Find(const PString& name);              // 6 (0x10035b50)

    // 0x10035b80: the type after the last dot, the lowercased prefix before it minus the "m" of the "m." tag
    static Result __cdecl SplitDecoratedMaterialName(const PString& decMaterialName, PString& materialType,
                                                     PString& materialName);

    JET_HEAP_OBJECT

    struct MaterialHash {};
    // the table by the decorated name node (0x10042d80 init, 0x10035db0 clear, the buckets freed last):
    // 97 FastDynamicArray buckets of Material*, chunks of 13
    struct Materials : HashTable<void, Material*, MaterialHash, 13> {
        Materials() { Init(); }
        ~Materials() { Destroy(); }
    };

    // (public for the layout checks and the module's own field access, like PStringCache)
    RenderKernel* kernel;      // +0x08
    TextureManager* textures;  // +0x0c (referenced)
    PString defaultType;       // +0x10 "notex" until SetDefaultMaterialType
    Materials materials;       // +0x14
};

JET_LAYOUT32(sizeof(MaterialManager) == 0x20);
JET_LAYOUT32(offsetof(MaterialManager, kernel) == 0x08 && offsetof(MaterialManager, textures) == 0x0c);
JET_LAYOUT32(offsetof(MaterialManager, defaultType) == 0x10 && offsetof(MaterialManager, materials) == 0x14);

// ---- MaterialSpec: the description a material config extracts (0x10034af0 / 0x10034b80 / 0x10033730 /
// 0x100337d0 / 0x10033910). Both lists are seven-item ring chunks: properties {key, value} and texture
// definitions {type, name, weight}.

struct MaterialProperty {
    PString key, value;
};
struct MaterialTextureDef {
    int type;
    PString name;
    float weight;
};

// the ring chunks of both lists ({next, prev, count, 7 items}); 0x1000d8e0 copies a property into a slot and
// 0x1000d6b0 a texture definition
template <class T>
class MaterialEntries {
    struct Chunk {
        Chunk* next;
        Chunk* prev;
        unsigned count;
        alignas(T) unsigned char storage[7 * sizeof(T)];
        T* item(unsigned i) { return reinterpret_cast<T*>(storage) + i; }
        const T* item(unsigned i) const { return reinterpret_cast<const T*>(storage) + i; }
    };

public:
    MaterialEntries() : head(nullptr) {}
    MaterialEntries(const MaterialEntries& other) : head(nullptr) { Append(other); }
    ~MaterialEntries() { Clear(); }
    MaterialEntries& operator=(const MaterialEntries& other)
    {
        Clear();
        Append(other);
        return *this;
    }

    void Append(const T& item)
    {
        Chunk* tail = head ? head->prev : nullptr;
        if (!tail || tail->count == 7) {
            auto* chunk = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
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
        ::new (tail->item(tail->count++)) T(item);
    }

    void Clear()
    {
        Chunk* first = head;
        if (!first) return;
        head = nullptr;
        Chunk* chunk = first->next;
        while (chunk != first) {
            Chunk* next = chunk->next;
            for (unsigned i = 0; i < chunk->count; ++i) chunk->item(i)->~T();
            Alloc::Delete(chunk);
            chunk = next;
        }
        for (unsigned i = 0; i < first->count; ++i) first->item(i)->~T();
        Alloc::Delete(first);
    }

    // the walk the extractors use: item by item, chunk by chunk from the head
    unsigned Count() const
    {
        unsigned n = 0;
        if (const Chunk* c = head) {
            do {
                n += c->count;
                c = c->next;
            } while (c != head);
        }
        return n;
    }
    const T* At(unsigned index) const
    {
        const Chunk* c = head;
        while (c) {
            if (index < c->count) return c->item(index);
            index -= c->count;
            c = c->next;
            if (c == head) break;
        }
        return nullptr;
    }

    Chunk* head;

private:
    void Append(const MaterialEntries& other)
    {
        const Chunk* source = other.head;
        if (!source) return;
        do {
            Chunk* chunk = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
            chunk->count = source->count;
            for (unsigned i = 0; i < source->count; ++i) ::new (chunk->item(i)) T(*source->item(i));
            if (!head) {
                head = chunk;
                chunk->prev = chunk->next = chunk;
            } else {
                chunk->next = head;
                chunk->prev = head->prev;
                head->prev->next = chunk;
                head->prev = chunk;
            }
            source = source->next;
        } while (source != other.head);
    }
};

struct MaterialSpec {
    MaterialSpec();
    explicit MaterialSpec(const PString& name);
    MaterialSpec(const MaterialSpec& other);
    MaterialSpec& operator=(const MaterialSpec& other);
    ~MaterialSpec();

    PString name;                                     // +0x00 the config item's value
    PString rename;                                   // +0x04 the "Rename" key's value
    DisplayMaterial material;                         // +0x08 ambient, diffuse, specular, emissive, power
    std::uint32_t values[3];                          // +0x4c source blend, +0x50 destination blend, +0x54 opacity
    unsigned char flag;                               // +0x58 bit 0 = two-sided
    MaterialEntries<MaterialProperty> properties;     // +0x5c
    MaterialEntries<MaterialTextureDef> textures;     // +0x60
    // keys present: ambient, diffuse, specular, emissive, shininess, blend, opacity, two-sided
    unsigned char flags[8];                           // +0x64
};
JET_LAYOUT32(sizeof(MaterialSpec) == 0x6c);
JET_LAYOUT32(offsetof(MaterialSpec, rename) == 0x04 && offsetof(MaterialSpec, material) == 0x08);
JET_LAYOUT32(offsetof(MaterialSpec, values) == 0x4c && offsetof(MaterialSpec, flag) == 0x58);
JET_LAYOUT32(offsetof(MaterialSpec, properties) == 0x5c && offsetof(MaterialSpec, textures) == 0x60);

using MaterialSpecList = FastDynamicArray<MaterialSpec*, 13, Less<MaterialSpec*, MaterialSpec*>>;
using MaterialList = FastDynamicArray<Material*, 13, Less<Material*, Material*>>;

// ---- MaterialConfig: the extractors over a config tree (the vftable only holds the destructor)

class MaterialConfig {
public:
    MaterialSpec* __cdecl ExtractMaterialSpec(const ConfigData& config) const;             // 0x100343a0
    MaterialSpecList __cdecl ExtractMaterialSpecs(const ConfigData& config) const;        // 0x10034a30
    Material* __cdecl ExtractMaterial(MaterialManager* manager, const ResourceDatabase* db,
                                      const ConfigData& config) const;                    // 0x10034350
    MaterialList __cdecl ExtractMaterials(MaterialManager* manager, const ResourceDatabase* db,
                                          const ConfigData& config) const;                // 0x10034960
    Material* __cdecl GetMaterial(MaterialManager* manager, const ResourceDatabase* db,
                                  const MaterialSpec& spec) const;                        // 0x10033d40

protected:
    Material* __cdecl CreateNewMaterial(MaterialManager* manager, Material* material,
                                        const PString& name) const;                       // 0x10034120
};

}  // namespace Jet
