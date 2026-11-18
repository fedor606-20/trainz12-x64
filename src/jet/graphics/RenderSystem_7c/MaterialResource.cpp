// Unit U7 of RenderSystem_7c: MaterialResource, the material text resource (0x10035f00..0x10036300; the
// vftable 0x10071554 has the 18 Resource slots plus CreateInstance at 18). Behaviour follows the TS12
// assembly (addresses per function): the ctors keep the extracted spec pointer and the MaterialConfig
// subobject's vtable, LoadTxt (0x10036080) parses the "version 2.0" text form and extracts its "Material"
// item into a MaterialSpec (renaming every texture definition through ConvertResourceName), and CreateInstance
// (0x10036060) makes the material of that spec through the manager.
#include <jet/Config.h>
#include <jet/Material.h>
#include <jet/PString.h>
#include <jet/Resource.h>

namespace Jet {

void __cdecl RsMaterialConfigSetVtable(void* object);  // MaterialConfigExtract.cpp (the shared original's)

namespace {

// the "Material" item LoadTxt extracts (the original's static PString at 0x100b86ec, interned at load like
// the extractors' literal keys: no Create / Destroy pair per parse)
const PString keyMaterialItem("Material");

// the spec's texture definition ring chunks (the same seven-item {next, prev, count, items} as MaterialSpec's
// MaterialEntries; the walk is FUN_1000d390's state machine, item by item and chunk by chunk)
struct TextureChunk {
    TextureChunk* next;
    TextureChunk* prev;
    unsigned int count;
    MaterialTextureDef items[7];
};

}  // namespace

// 0x10035fb0
MaterialResource::MaterialResource(const PString& name, const ResourceDatabase* owner)
    : Resource(name, owner), spec(nullptr)
{
    RsMaterialConfigSetVtable(&config);
}

// 0x10035f00
MaterialResource::MaterialResource(const MaterialResource& o) : Resource(o), spec(o.spec)
{
    RsMaterialConfigSetVtable(&config);
}

// 0x10035f30
MaterialResource& MaterialResource::operator=(const MaterialResource& o)
{
    Resource::operator=(o);
    spec = o.spec;
    return *this;
}

// 0x10035fe0: the spec goes, then the members in reverse order (the config subobject's vtable is restored
// before the Resource base takes its own down)
MaterialResource::~MaterialResource()
{
    if (spec) {
        spec->~MaterialSpec();
        Alloc::Delete(spec);
    }
    spec = nullptr;
    RsMaterialConfigSetVtable(&config);
}

// 0x10036060: the material of the extracted spec, through the manager
Material* __cdecl MaterialResource::CreateInstance(MaterialManager* manager, const ResourceDatabase* db) const
{
    return config.GetMaterial(manager, db, *spec);
}

// 0x10036080: the text form is "version 2.0" and one "Material" item; its texture definitions keep their
// names converted for this resource's database. The parse goes through the owner's GetFileName when there is
// one, else the name is the path
Resource::Result __cdecl MaterialResource::LoadTxt(const char* name)
{
    ConfigParser parser;
    bool failed;
    if (Owner()) {
        const PString fileName(name);
        const PString path = Owner()->GetFileName(fileName);
        failed = parser.Parse(path.Text()).code != 0;
    } else {
        failed = parser.Parse(name).code != 0;
    }
    if (failed) return Result(1);
    if (!parser.RootPtr() || parser.GetVersion() != 2.0f) return Result(1);
    if (spec) {
        spec->~MaterialSpec();
        Alloc::Delete(spec);
    }
    spec = nullptr;
    spec = config.ExtractMaterialSpec(parser.Root().Find(keyMaterialItem));
    if (!spec) return Result(1);
    TextureChunk* const head = reinterpret_cast<TextureChunk*>(spec->textures.head);
    if (head) {
        unsigned char* item = reinterpret_cast<unsigned char*>(head) + 0xc;
        TextureChunk* chunk = head;
        while (item) {
            MaterialTextureDef* const def = reinterpret_cast<MaterialTextureDef*>(item);
            String text(def->name.node ? def->name.node->text : nullptr);
            ConvertResourceName(text);
            const char* converted = text;
            if (def->name.node) PString::Cache()->Destroy(def->name.node);
            def->name.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(converted));
            item += 0xc;
            if (reinterpret_cast<unsigned char*>(chunk) + (chunk->count + 1) * 0xc <= item) {
                chunk = chunk->next;
                item = chunk != head ? reinterpret_cast<unsigned char*>(chunk) + 0xc : nullptr;
            }
        }
    }
    return Result(0);
}

}  // namespace Jet
