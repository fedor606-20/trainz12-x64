// U12/U16: resource factory registration performed by JetGetPluginsRenderSystem. The factories are our own
// ResourceType<> instances now, so registration needs no table copied from the renamed original.
#include <jet/GfxLanguage.h>
#include <jet/KernelStandard.h>
#include <jet/Material.h>
#include <jet/PString.h>
#include <jet/Resource.h>
#include <jet/Texture.h>
#include <jet/UId.h>

#include <new>

namespace {

constexpr char kResourceGroup[] = "4C414E80-BBDA-11d4-9AD7-0001025364B8";

template <class T>
void Register(Jet::PStringCache* cache, const char* extension, const char* guid)
{
    Jet::UId parent(kResourceGroup);
    void* const object = Jet::Alloc::New(sizeof(Jet::ResourceType<T>));
    if (!object) return;
    auto* const created = cache->Create(reinterpret_cast<const unsigned char*>(extension));
    void* node = created;
    auto& name = *reinterpret_cast<Jet::PString*>(&node);  // borrowed: the caller releases the node
    Jet::UId id(guid);
    ::new (object) Jet::ResourceType<T>(name, id, parent);
    cache->Destroy(created);
}

}  // namespace

void __cdecl RsJetGetPluginsRenderSystem()
{
    auto* const cache = Jet::PString::Cache();
    // bmp-tex and friends still lack their own BitmapTextureResource; the base loader is a local placeholder
    // until that unit is ported (it never reaches for the original DLL).
    Register<Jet::TextureResource>(cache, "bmp-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8");
    Register<Jet::TextureResource>(cache, "tga-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8");
    Register<Jet::TextureResource>(cache, "bmframe-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8");
    Register<Jet::TextureResource>(cache, "pcx-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8");
    Register<Jet::TextureResource>(cache, "jpg-tex", "10289D60-1FB0-11d5-9AD9-0001025364B8");
    Register<Jet::TextureResource>(cache, "texture", "07DACCA0-2424-11d5-9AD9-0001025364B8");
    Register<Jet::AsciiTextureResource>(cache, "texture.txt", "75E05E60-4C54-11d5-9AE6-0001025364B8");
    Register<Jet::VertexProgramResource>(cache, "vtx.txt", "295B4F9C-48FE-4944-921F-CA56FE11A674");
    Register<Jet::TextureCombinerResource>(cache, "pxl.txt", "CBF1BD68-6D71-4189-8ED3-BCB82FC0920C");
    Register<Jet::MaterialResource>(cache, "material.txt", "548C083E-CA85-48e2-B29A-CB8A0EC9F029");
}
