// U12: resource factory registration performed by JetGetPluginsRenderSystem.
// The factories' virtual methods still belong to the original module; their vtables are copied into this module
// so the registered objects point at local data. All calls to other Jet modules stay ordinary imports.
#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/Resource.h>
#include <jet/UId.h>
#include <windows.h>

#include <cstdlib>
#include <cstring>
#include <new>

namespace {

// Offsets and slot counts read from the original RenderSystem_7c image.
enum FactoryTable { Bitmap, Texture, AsciiTexture, VertexProgram, TextureCombiner, Material };
struct TableDef {
    unsigned int rva;
    unsigned int slots;
};
constexpr TableDef kTables[] = {
    {0x71d6c, 7}, {0x71d34, 6}, {0x71d50, 6}, {0x71e64, 6}, {0x71df4, 6}, {0x71d88, 6},
};
void* tables[6][8];  // [-1] RTTI locator, then up to seven slots; the rest stays zero.
bool tablesReady;

struct ConcreteResourceType : Jet::ResourceTypeBase {
    ConcreteResourceType(const Jet::PString& name, const Jet::UId& id, const Jet::UId& parent)
        : ResourceTypeBase(name, id, parent) {}
    Jet::Resource* __cdecl Create(const Jet::PString&, const Jet::ResourceDatabase*) const override { return nullptr; }
    unsigned int __cdecl GetResourceSize() const override { return 0; }
};

void InitTables()
{
    if (tablesReady) return;
    const auto* original = reinterpret_cast<const unsigned char*>(GetModuleHandleA("RenderSystem_7c_orig.dll"));
    if (!original) std::abort();
    for (unsigned i = 0; i < 6; ++i)
        std::memcpy(tables[i], original + kTables[i].rva - 4, (kTables[i].slots + 1) * sizeof(void*));
    tablesReady = true;
}

void Register(Jet::PStringCache* cache, const char* extension, const char* guid, FactoryTable table)
{
    InitTables();
    Jet::UId parent("4C414E80-BBDA-11d4-9AD7-0001025364B8");
    void* object = Jet::Alloc::New(sizeof(Jet::ResourceTypeBase));
    if (!object) return;
    auto* node = cache->Create(reinterpret_cast<const unsigned char*>(extension));
    auto& name = *reinterpret_cast<Jet::PString*>(&node);  // borrowed: the caller releases the node
    Jet::UId id(guid);
    ::new (object) ConcreteResourceType(name, id, parent);
    *reinterpret_cast<void**>(object) = tables[table] + 1;
    cache->Destroy(node);
}

}  // namespace

void __cdecl RsJetGetPluginsRenderSystem()
{
    auto* cache = Jet::PString::Cache();
    Register(cache, "bmp-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8", Bitmap);
    Register(cache, "tga-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8", Bitmap);
    Register(cache, "bmframe-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8", Bitmap);
    Register(cache, "pcx-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8", Bitmap);
    Register(cache, "jpg-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8", Bitmap);
    Register(cache, "texture", "07DACCA0-2424-11d5-9AD9-0001025364B8", Texture);
    Register(cache, "texture.txt", "75E05E60-4C54-11d5-9AE6-0001025364B8", AsciiTexture);
    Register(cache, "vtx.txt", "295B4F9C-48FE-4944-921F-CA56FE11A674", VertexProgram);
    Register(cache, "pxl.txt", "CBF1BD68-6D71-4189-8ED3-BCB82FC0920C", TextureCombiner);
    Register(cache, "material.txt", "548C083E-CA85-48e2-B29A-CB8A0EC9F029", Material);
}
