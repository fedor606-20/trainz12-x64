// Unit U7 of RenderSystem_7c: the MaterialConfig extractors. The leaf extractors (ExtractColor 0x10033ad0,
// ExtractFloat 0x10033bc0, ExtractBool 0x10033c80) and the object's lifetime run as free functions under
// their exported names (implemented.txt aliases); the tree extractors are MaterialConfig members so their
// mangled names come out exactly as exported. Behaviour follows the TS12 assembly (addresses per function):
// ExtractMaterialSpec (0x100343a0) fills a MaterialSpec from a config item, GetMaterial (0x10033d40) makes or
// reuses a material for it, CreateNewMaterial (0x10034120) clones a material under a new name through the
// manager, and the two list extractors (0x10034960 / 0x10034a30) walk the "Material" children.
#include <jet/Config.h>
#include <jet/Material.h>
#include <jet/PString.h>
#include <jet/RenderTypes.h>
#include <jet/VCall.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

// the leaf extractors and MaterialFileSupport's conversions (their own exports / other TUs)
bool __cdecl RsExtractMaterialColor(void* self, Color* out, const ConfigData* config, const char* name);
bool __cdecl RsExtractMaterialFloat(void* self, float* out, const ConfigData* config, const char* name);
bool __cdecl RsExtractMaterialBool(void* self, bool* out, const ConfigData* config, const char* name);
int __cdecl RsMaterialSourceValue(const PString& name);
int __cdecl RsMaterialDestinationValue(const PString& name);

namespace {

template <class T>
T& Field(void* p, unsigned offset) { return *reinterpret_cast<T*>(static_cast<unsigned char*>(p) + offset); }

// 0x1000ddd0: the old node goes first (the assignment the original inlines everywhere)
void Assign(PString& dst, const PString& src)
{
    if (dst.node) PString::Cache()->Destroy(dst.node);
    dst.node = src.node;
    if (dst.node) {
        PString::Cache()->Lock();
        ++dst.node->refs;
        ++PString::Cache()->liveRefs;
        PString::Cache()->Unlock();
    }
}

// 0x1000be60's halves: a new reference to a raw node, like a PString copy
void AdoptNode(PString& dst, PStringCache::Node* node)
{
    dst.node = node;
    if (node) {
        PString::Cache()->Lock();
        ++node->refs;
        ++PString::Cache()->liveRefs;
        PString::Cache()->Unlock();
    }
}

// the material's own lists (the same seven-item ring chunks as the spec's)
struct MaterialPropertyDef {
    PString key, value;
};
struct MaterialTextureDefItem {
    int type;
    PString name;
    float weight;
};
template <class T>
struct RingChunk {
    RingChunk* next;
    RingChunk* prev;
    unsigned count;
    T items[7];
};
// the first entry whose key node is `key`
MaterialPropertyDef* FindProperty(void* material, const PString& key)
{
    auto* first = Field<RingChunk<MaterialPropertyDef>*>(material, 0x88);
    if (!first) return nullptr;
    auto* chunk = first;
    do {
        for (unsigned i = 0; i < chunk->count; ++i)
            if (chunk->items[i].key.node == key.node) return &chunk->items[i];
        chunk = chunk->next;
    } while (chunk != first);
    return nullptr;
}

// the texture type names the definition walk matches (the original's {name, type} table at 0x1006cb98)
struct TextureTypeName {
    const char* name;
    int type;
};
const TextureTypeName textureTypes[] = {
    {"ambient", 0}, {"diffuse", 1}, {"specular", 2}, {"shine", 3}, {"shinstrength", 4},
    {"selfillum", 5}, {"opacity", 6}, {"filtercolor", 7}, {"bump", 8}, {"reflect", 9},
    {"refract", 10}, {"displacement", 11}, {"lookup0", 12}, {"lookup1", 13}, {"lookup2", 14}, {"lookup3", 15}};

// The key names the original interns once, at module load, in its own file-scope PStrings (its static
// initialisers call PStringCache::Create off the .CRT table, so a lookup reuses the node rather than making
// one). Keeping them here as module-scope statics gives the extractors the same node traffic: no Create /
// Destroy pair per key, only the "Material" walk and the runtime-name leaf lookups make nodes.
const PString keyMaterial("Material");
const PString keyRename("Rename");
const PString keySrcBlend("SrcBlend");
const PString keyDstBlend("DstBlend");
const PString keyProperties("Properties");
const PString keyTexture("Texture");
const PString keyBitmap("Bitmap");
const PString keyName("Name");

// the manager's and the material's virtuals used here (the virtuals are __cdecl with `this` first):
// manager 3 Create; material 8 GetTexture, 9 SetTexture (the texture form), 10 SetTexture (the name form),
// 18 AddProperty
MaterialManager::Result ManagerCreate(void* manager, Material*& material, const PString& name)
{
    MaterialManager::Result result;
    VCall<int*>(manager, 0xc, &result, &material, &name);
    return result;
}
void* MaterialGetTexture(void* material, int type) { return VCall<void*>(material, 0x20, type); }
void MaterialSetTextureRef(void* material, int type, void* texture, float amount)
{
    int result = 0;
    VCall<int*>(material, 0x24, &result, type, &texture, amount);
}
void MaterialSetTextureName(void* material, int type, const ResourceDatabase* db, const PString& name,
                            float amount, const String& fallback)
{
    int result = 0;
    VCall<int*>(material, 0x28, &result, type, db, &name, amount, &fallback);
}
void MaterialAddProperty(void* material, const PString& key, const PString& value)
{
    VCall<void>(material, 0x48, &key, &value);
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

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// the leaf extractors and the object's lifetime (free functions under the exported names)

namespace {
void SetMaterialConfigVtable(void* object)
{
    static void* const table = reinterpret_cast<void*>(GetProcAddress(
        GetModuleHandleA("RenderSystem_7c_orig.dll"), "??_7MaterialConfig@Jet@@6B@"));
    *static_cast<void**>(object) = table;
}
}  // namespace

void* __fastcall RsMaterialConfigDefault(void* object, void*) { SetMaterialConfigVtable(object); return object; }
void* __fastcall RsMaterialConfigCopy(void* object, void*, const void*) { SetMaterialConfigVtable(object); return object; }
void __fastcall RsMaterialConfigDestroy(void* object, void*) { SetMaterialConfigVtable(object); }
void* __fastcall RsMaterialConfigAssign(void* object, void*, const void*) { return object; }

namespace {
const ConfigData* FindMaterialKey(const ConfigData& config, const char* name)
{
    const PString key(name);
    return &config.Find(key);
}
}  // namespace

bool __cdecl RsExtractMaterialColor(void*, Color* out, const ConfigData* config, const char* name)
{
    const ConfigData* item = FindMaterialKey(*config, name);
    if (item == &ConfigData::unknownItem) return false;
    *out = static_cast<Color>(*item);
    return true;
}
bool __cdecl RsExtractMaterialFloat(void*, float* out, const ConfigData* config, const char* name)
{
    const ConfigData* item = FindMaterialKey(*config, name);
    if (item == &ConfigData::unknownItem) return false;
    *out = static_cast<float>(*item);
    return true;
}
bool __cdecl RsExtractMaterialBool(void*, bool* out, const ConfigData* config, const char* name)
{
    const ConfigData* item = FindMaterialKey(*config, name);
    if (item == &ConfigData::unknownItem) return false;
    *out = true;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// the config extractors

// 0x100343a0: the scalars under their keys, the "Rename" value, the blend names, then the property and
// texture definition lists
MaterialSpec* MaterialConfig::ExtractMaterialSpec(const ConfigData& config) const
{
    if (&config == &ConfigData::unknownItem) return nullptr;
    PString name;
    config >> name;
    void* block = Alloc::New(0x6c);
    MaterialSpec* spec = block ? new (block) MaterialSpec(name) : nullptr;
    void* const self = const_cast<MaterialConfig*>(this);
    spec->flags[0] = RsExtractMaterialColor(self, &spec->material.ambient, &config, "Ambient");
    spec->flags[1] = RsExtractMaterialColor(self, &spec->material.diffuse, &config, "Diffuse");
    spec->flags[2] = RsExtractMaterialColor(self, &spec->material.specular, &config, "Specilar");
    spec->flags[3] = RsExtractMaterialColor(self, &spec->material.emissive, &config, "Emissive");
    spec->flags[4] = RsExtractMaterialFloat(self, &spec->material.power, &config, "Shininess");
    spec->flags[6] = RsExtractMaterialFloat(self, reinterpret_cast<float*>(&spec->values[2]), &config, "Opacity");
    spec->flags[7] = RsExtractMaterialBool(self, reinterpret_cast<bool*>(&spec->flag), &config, "TwoSided");
    const ConfigData& rename = config.Find(keyRename);
    if (&rename != &ConfigData::unknownItem) Assign(spec->rename, rename.GetPStringData());
    PString srcName, dstName;
    config.Find(keySrcBlend) >> srcName;
    config.Find(keyDstBlend) >> dstName;
    spec->values[0] = RsMaterialSourceValue(srcName);
    spec->values[1] = RsMaterialDestinationValue(dstName);
    if ((srcName.node && srcName.node->length) || (dstName.node && dstName.node->length)) spec->flags[5] = 1;
    // the properties: the {key, value} of every "Properties" child
    const ConfigData& properties = config.Find(keyProperties);
    for (int i = 0;; ++i) {
        const ConfigData& child = properties[i];
        if (&child == &ConfigData::unknownItem) break;
        PString value;
        child >> value;
        MaterialProperty entry;
        AdoptNode(entry.key, child.GetName().node);
        AdoptNode(entry.value, child.GetPStringData().node);
        spec->properties.Append(entry);
    }
    // the texture definitions: the value names the type, "Bitmap" or "Name" the file, "Amount" the weight
    for (int i = 0;; ++i) {
        const ConfigData& child = config(keyTexture, i);
        if (&child == &ConfigData::unknownItem) break;
        PString value;
        child >> value;
        int type = -1;
        for (unsigned t = 0; t < sizeof textureTypes / sizeof textureTypes[0]; ++t)
            if (!value.CompareIgnoreCase(textureTypes[t].name)) {
                type = textureTypes[t].type;
                break;
            }
        PString fileName;
        child.Find(keyBitmap) >> fileName;
        child.Find(keyName) >> fileName;
        float weight;
        if (!RsExtractMaterialFloat(self, &weight, &child, "Amount")) weight = 1.0f;
        MaterialTextureDef entry;
        entry.type = type;
        AdoptNode(entry.name, fileName.node);
        entry.weight = weight;
        spec->textures.Append(entry);
    }
    return spec;
}

// 0x10034a30: every "Material" child that still extracts
MaterialSpecList MaterialConfig::ExtractMaterialSpecs(const ConfigData& config) const
{
    MaterialSpecList list;
    for (int i = 0;; ++i) {
        MaterialSpec* spec = ExtractMaterialSpec(config(keyMaterial, i));
        if (!spec) break;
        list.Add(spec);
    }
    return list;
}

// 0x10034350: one spec in, one material out (the spec goes afterwards)
Material* MaterialConfig::ExtractMaterial(MaterialManager* manager, const ResourceDatabase* db,
                                          const ConfigData& config) const
{
    MaterialSpec* spec = ExtractMaterialSpec(config);
    if (!spec) return nullptr;
    Material* material = GetMaterial(manager, db, *spec);
    spec->~MaterialSpec();
    Alloc::Delete(spec);
    return material;
}

// 0x10034960: every "Material" child that produces a material
MaterialList MaterialConfig::ExtractMaterials(MaterialManager* manager, const ResourceDatabase* db,
                                             const ConfigData& config) const
{
    MaterialList list;
    for (int i = 0;; ++i) {
        Material* material = ExtractMaterial(manager, db, config(keyMaterial, i));
        if (!material) break;
        list.Add(material);
    }
    return list;
}

// 0x10033d40: the manager makes the material under the spec's name (the "Rename" name makes a second one from
// it), then the spec's state goes in field by field and its lists walk the material's
Material* MaterialConfig::GetMaterial(MaterialManager* manager, const ResourceDatabase* db,
                                      const MaterialSpec& spec) const
{
    PString temp;
    temp.node = PString::Cache()->Create(spec.name.node
                                             ? reinterpret_cast<const unsigned char*>(spec.name.node->text)
                                             : nullptr);
    Material* material = nullptr;
    const MaterialManager::Result created = ManagerCreate(manager, material, temp);
    if (temp.node) {
        PString::Cache()->Destroy(temp.node);
        temp.node = nullptr;  // the original hands the manager a raw node: no second destroy on the way out
    }
    if (created.code != 0) return nullptr;
    if (spec.rename.node && spec.rename.node->length &&
        Field<PString>(material, 0x24).CompareIgnoreCase(spec.rename) != 0) {
        Material* renamed = CreateNewMaterial(manager, material, spec.rename);
        MaterialRefs(material)->ReleaseReference();
        material = renamed;
        if (!material) return nullptr;
    }
    unsigned char* const m = reinterpret_cast<unsigned char*>(material);
    if (spec.flags[0]) std::memcpy(m + 0x34, &spec.material.ambient, 16);
    if (spec.flags[1]) std::memcpy(m + 0x44, &spec.material.diffuse, 16);
    if (spec.flags[2]) std::memcpy(m + 0x54, &spec.material.specular, 16);
    if (spec.flags[3]) std::memcpy(m + 0x64, &spec.material.emissive, 16);
    if (spec.flags[4]) *reinterpret_cast<float*>(m + 0x74) = spec.material.power;
    if (spec.flags[5]) {
        *reinterpret_cast<int*>(m + 0x78) = static_cast<int>(spec.values[0]);
        *reinterpret_cast<int*>(m + 0x7c) = static_cast<int>(spec.values[1]);
        unsigned char& flags = m[0x84];
        if (spec.values[0] == 1 && spec.values[1] == 0)
            flags |= 2;
        else
            flags &= ~2;
    }
    if (spec.flags[7]) m[0x84] ^= (m[0x84] ^ spec.flag) & 1;
    if (spec.flags[6]) *reinterpret_cast<float*>(m + 0x80) = *reinterpret_cast<const float*>(&spec.values[2]);
    // the properties: a matching key replaces its value, anything else goes through AddProperty
    for (unsigned i = 0; i < spec.properties.Count(); ++i) {
        const MaterialProperty& entry = *spec.properties.At(i);
        if (MaterialPropertyDef* found = FindProperty(material, entry.key))
            Assign(found->value, entry.value);
        else
            MaterialAddProperty(material, entry.key, entry.value);
    }
    // the texture definitions, always through the name form of SetTexture
    for (unsigned i = 0; i < spec.textures.Count(); ++i) {
        const MaterialTextureDef& entry = *spec.textures.At(i);
        String fallback;
        MaterialSetTextureName(material, entry.type, db, entry.name, entry.weight, fallback);
    }
    return material;
}

// 0x10034120: the manager makes a material under `name`, the base's state is copied into it and its
// definitions follow (the textures through GetTexture / SetTexture)
Material* MaterialConfig::CreateNewMaterial(MaterialManager* manager, Material* material,
                                            const PString& name) const
{
    PString temp;
    temp.node = PString::Cache()->Create(name.node ? reinterpret_cast<const unsigned char*>(name.node->text)
                                                   : nullptr);
    Material* created = nullptr;
    const MaterialManager::Result result = ManagerCreate(manager, created, temp);
    if (temp.node) {
        PString::Cache()->Destroy(temp.node);
        temp.node = nullptr;  // the original hands the manager a raw node: no second destroy on the way out
    }
    if (result.code != 0) return nullptr;
    unsigned char* const dst = reinterpret_cast<unsigned char*>(created);
    const unsigned char* const src = reinterpret_cast<const unsigned char*>(material);
    const int source = *reinterpret_cast<const int*>(src + 0x78);
    const int destination = *reinterpret_cast<const int*>(src + 0x7c);
    *reinterpret_cast<int*>(dst + 0x78) = source;
    *reinterpret_cast<int*>(dst + 0x7c) = destination;
    unsigned char& flags = dst[0x84];
    if (source == 1 && destination == 0)
        flags |= 2;
    else
        flags &= ~2;
    *reinterpret_cast<float*>(dst + 0x80) = *reinterpret_cast<const float*>(src + 0x80);
    flags ^= (flags ^ src[0x84]) & 1;
    ::new (dst + 0x34) DisplayMaterial(*reinterpret_cast<const DisplayMaterial*>(src + 0x34));
    // the properties go in through AddProperty
    auto* firstProperty = Field<RingChunk<MaterialPropertyDef>*>(material, 0x88);
    if (firstProperty) {
        auto* chunk = firstProperty;
        do {
            for (unsigned i = 0; i < chunk->count; ++i)
                MaterialAddProperty(created, chunk->items[i].key, chunk->items[i].value);
            chunk = chunk->next;
        } while (chunk != firstProperty);
    }
    // the texture definitions: the texture of each type rides in through the SetTexture form
    auto* firstTexture = Field<RingChunk<MaterialTextureDefItem>*>(material, 0x90);
    if (firstTexture) {
        auto* chunk = firstTexture;
        do {
            for (unsigned i = 0; i < chunk->count; ++i) {
                const MaterialTextureDefItem& entry = chunk->items[i];
                if (void* object = MaterialGetTexture(material, entry.type)) {
                    static_cast<ReferenceCount*>(object)->AddReference();
                    MaterialSetTextureRef(created, entry.type, object, entry.weight);
                    if (object) static_cast<ReferenceCount*>(object)->ReleaseReference();
                }
            }
            chunk = chunk->next;
        } while (chunk != firstTexture);
    }
    return created;
}

}  // namespace Jet
