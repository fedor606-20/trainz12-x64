// U9: access, initialization, and owned resources of the legacy RenderKernel.
#include <jet/Material.h>
#include <jet/RenderSystem.h>
#include <jet/Texture.h>
#include <windows.h>

namespace Jet {
namespace {
template<class T>
T& Field(RenderKernel* kernel, unsigned offset)
{
    return *reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(kernel) + offset);
}

using ResourceList = FastDynamicArray<SystemResourceManager*>;
}

Display* __cdecl RenderKernel::GetDisplay() { return Field<Display*>(this, 0x7c); }
OS* __cdecl RenderKernel::GetOS() { return Field<OS*>(this, 0x8c); }
SceneGraph* __cdecl RenderKernel::GetSceneGraph() { return Field<SceneGraph*>(this, 0x80); }
Jet::TextureManager* __cdecl RenderKernel::GetTextureManager() { return Field<Jet::TextureManager*>(this, 0x94); }

void __cdecl RenderKernel::SetTextureManager(Jet::TextureManager* manager)
{
    auto& current = Field<Jet::TextureManager*>(this, 0x94);
    if (current)
        current->ReleaseReference();
    current = manager;
    if (manager) {
        manager->AddReference();
        VCall<bool>(manager, 0x08, this);
    }
}

void __cdecl RenderKernel::AddSystemResource(SystemResourceManager* resource)
{
    Field<ResourceList>(this, 0x88).Add(resource);
}

void __cdecl RenderKernel::RemoveSystemResource(SystemResourceManager* resource)
{
    auto& list = Field<ResourceList>(this, 0x88);
    list.Remove(list.Find(resource));
}

// This virtual's stack-this pointer is to the LayerManager base, not RenderKernel.
RenderKernel* __cdecl RsKernelFromLayerManager(void* base)
{
    return reinterpret_cast<RenderKernel*>(static_cast<unsigned char*>(base) - 0x28);
}

bool __cdecl RsRenderKernelInit(RenderKernel* kernel, OS* os, DisplaySelector* selector)
{
    const auto retail = GetModuleHandleA("RenderSystem_7c_orig.dll");
    using Load = bool(__cdecl*)(RenderKernel*, DisplaySelector*);
    const auto load = reinterpret_cast<Load>(GetProcAddress(retail,
        "?LoadRenderSystem@RenderKernel@Jet@@IAA_NPAVDisplaySelector@2@@Z"));
    Field<OS*>(kernel, 0x8c) = os;
    if (!load(kernel, selector)) return false;

    auto& textures = Field<TextureManager*>(kernel, 0x94);
    if (!textures) {
        textures = new TextureManager;
        VCall<bool>(textures, 0x08, kernel);
    }
    auto& materials = Field<MaterialManager*>(kernel, 0x90);
    if (!materials) {
        materials = new MaterialManager(textures);
        VCall<bool>(materials, 0x08, kernel);
    }

    // The original and reconstructed texture loaders still share this option.
    auto* noPath = reinterpret_cast<bool*>(GetProcAddress(retail, "?noPath@AsciiTextureResource@Jet@@1_NA"));
    *noPath = os->GetOptions()->Get("NoAsciiTexPath");
    using Init = bool(__cdecl*)(RenderKernel*, OS*);
    const auto init = reinterpret_cast<Init>(GetProcAddress(GetModuleHandleA("Kernel_7c.dll"),
        "?Init@Task@Jet@@MAA_NPAVOS@2@@Z"));
    return init(kernel, os);
}

void __cdecl RsKernelDumpScreen(RenderKernel* kernel, unsigned frame)
{
    Field<unsigned>(kernel, 0x9c) = frame ? frame : kernel->GetDisplay()->GetCurrentFrame() + 1;
    Field<bool>(kernel, 0x99) = true;
}
void __cdecl RsKernelMultipleScreenDump(RenderKernel* kernel, unsigned first, unsigned last, unsigned step)
{
    Field<unsigned>(kernel, 0xa0) = first ? first : kernel->GetDisplay()->GetCurrentFrame();
    Field<unsigned>(kernel, 0xa4) = last;
    Field<unsigned>(kernel, 0xa8) = step;
    Field<bool>(kernel, 0x99) = true;
}
void __cdecl RsKernelSetScreenDumpFilename(RenderKernel* kernel, const String& name, bool sequence, bool jpeg)
{
    Field<bool>(kernel, 0x98) = sequence;
    static_cast<AnsiString&>(Field<String>(kernel, 0xac)) = name;
    Field<bool>(kernel, 0xb0) = jpeg;
}

}  // namespace Jet
