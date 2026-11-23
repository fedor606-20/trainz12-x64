// U12/U16: resource factory registration performed by JetGetPluginsRenderSystem. The factories are our own
// ResourceType<> instances. When the renamed original is loaded, the registered objects carry local copies of
// its six factory vtables (with the RTTI locators in front of them), because a factory's vtable is observable
// and must stay byte-identical to the one the original installs; without it the objects keep the vtables the
// compiler built for the local factory classes.
#include <jet/GfxLanguage.h>
#include <jet/KernelStandard.h>
#include <jet/Material.h>
#include <jet/PString.h>
#include <jet/Resource.h>
#include <jet/Texture.h>
#include <jet/UId.h>
#include <windows.h>

#include <cstring>
#include <new>

#include "BitmapTextureResource.h"

namespace {

constexpr char kResourceGroup[] = "4C414E80-BBDA-11d4-9AD7-0001025364B8";

// The factory table each extension uses in the retail image: the RTTI locator at rva - 4, then the slots.
enum FactoryKind { Bitmap, Texture, AsciiTexture, VertexProgram, TextureCombiner, Material };
struct FactoryTableDef {
    unsigned int rva;
    unsigned int slots;
};
constexpr FactoryTableDef kFactoryTables[] = {
    {0x71d6c, 7},  // ResourceType<BitmapTextureResource>
    {0x71d34, 6},  // ResourceType<TextureResource>
    {0x71d50, 6},  // ResourceType<AsciiTextureResource>
    {0x71e64, 6},  // ResourceType<VertexProgramResource>
    {0x71df4, 6},  // ResourceType<TextureCombinerResource>
    {0x71d88, 6},  // ResourceType<MaterialResource>
};
constexpr unsigned kMaxFactorySlots = 7;

void* factoryTables[6][kMaxFactorySlots + 1];  // [-1] locator, then the slots
bool factoryTablesReady;
bool factoryTablesPresent;

// Build the local copies once, if the renamed original is loaded. The five resource factories get our own
// Create (slot 3); the bitmap factories keep the retail one so the installed table is the original's.
void InitFactoryTables()
{
    if (factoryTablesReady) return;
    factoryTablesReady = true;
    const auto* const original = reinterpret_cast<const unsigned char*>(
        GetModuleHandleA("RenderSystem_7c_orig.dll"));
    if (!original) return;
    for (unsigned i = 0; i < 6; ++i)
        std::memcpy(factoryTables[i], original + kFactoryTables[i].rva - sizeof(void*),
                    (kFactoryTables[i].slots + 1) * sizeof(void*));
    factoryTablesPresent = true;
}

// ---------------------------------------------------------------------------------------------------------------
// construction

template <class Factory>
void RegisterFactory(Jet::PStringCache* cache, FactoryKind kind, const char* extension, const char* guid)
{
    Jet::UId parent(kResourceGroup);
    void* const object = Jet::Alloc::New(sizeof(Factory));
    if (!object) return;
    auto* const created = cache->Create(reinterpret_cast<const unsigned char*>(extension));
    void* node = created;
    auto& name = *reinterpret_cast<Jet::PString*>(&node);  // borrowed: the caller releases the node
    Jet::UId id(guid);
    ::new (object) Factory(name, id, parent);
    InitFactoryTables();
    if (factoryTablesPresent && kind != Bitmap) {
        // keep the local Create for the reverse lookup other modules do, the rest of the table stays retail
        void* const localTable = *reinterpret_cast<void**>(object);
        factoryTables[kind][1 + 3] = static_cast<void**>(localTable)[3];
    }
    if (factoryTablesPresent) *reinterpret_cast<void**>(object) = factoryTables[kind] + 1;
    cache->Destroy(created);
}

template <class T>
void Register(Jet::PStringCache* cache, FactoryKind kind, const char* extension, const char* guid)
{
    RegisterFactory<Jet::ResourceType<T>>(cache, kind, extension, guid);
}

}  // namespace

void __cdecl RsJetGetPluginsRenderSystem()
{
    auto* const cache = Jet::PString::Cache();
    // U16: the five bitmap factories are local (BitmapTextureResourceType, retail 0x1003b7c0). While the
    // original is loaded, their installed table is the original's, so its Create makes the retail bitmap
    // resource; the local class runs when it is absent.
    RegisterFactory<Jet::BitmapTextureResourceType>(cache, Bitmap, "bmp-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8");
    RegisterFactory<Jet::BitmapTextureResourceType>(cache, Bitmap, "tga-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8");
    RegisterFactory<Jet::BitmapTextureResourceType>(cache, Bitmap, "bmframe-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8");
    RegisterFactory<Jet::BitmapTextureResourceType>(cache, Bitmap, "pcx-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8");
    RegisterFactory<Jet::BitmapTextureResourceType>(cache, Bitmap, "jpg-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8");
    Register<Jet::TextureResource>(cache, Texture, "texture", "07DACCA0-2424-11d5-9AD9-0001025364B8");
    Register<Jet::AsciiTextureResource>(cache, AsciiTexture, "texture.txt", "75E05E60-4C54-11d5-9AE6-0001025364B8");
    Register<Jet::VertexProgramResource>(cache, VertexProgram, "vtx.txt", "295B4F9C-48FE-4944-921F-CA56FE11A674");
    Register<Jet::TextureCombinerResource>(cache, TextureCombiner, "pxl.txt", "CBF1BD68-6D71-4189-8ED3-BCB82FC0920C");
    Register<Jet::MaterialResource>(cache, Material, "material.txt", "548C083E-CA85-48e2-B29A-CB8A0EC9F029");
}
