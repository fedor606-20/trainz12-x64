// U9: access, initialization, and owned resources of the legacy RenderKernel.
#include <jet/Material.h>
#include <jet/Plugin.h>
#include <jet/RenderSystem.h>
#include <jet/Texture.h>
#include <windows.h>
#include <cstring>
#include <memory>
#include <new>

namespace Jet {
namespace {
template<class T>
T& Field(RenderKernel* kernel, unsigned offset)
{
    return *reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(kernel) + offset);
}

using ResourceList = FastDynamicArray<SystemResourceManager*>;

void ReleaseDisplay(Display*& display)
{
    auto* bytes = reinterpret_cast<unsigned char*>(display);
    const int offset = (*reinterpret_cast<const int**>(bytes + 4))[1];
    reinterpret_cast<ReferenceCount*>(bytes + 4 + offset)->ReleaseReference();
    display = nullptr;
}

struct DisplayPluginLookup : PluginBase {
    using PluginBase::LookupPluginType;
};

DisplayTexture* __cdecl CreateNullTexture(Display* display)
{
    std::unique_ptr<void, decltype(&Alloc::Delete)> memory(Alloc::New(sizeof(DisplayTextureNull)), &Alloc::Delete);
    if (!memory) return nullptr;
    auto* const texture = ::new (memory.get()) DisplayTextureNull(display);
    memory.release();
    return texture;
}

void InstallNullTextureFactory(Display* display)
{
    // Retail's primary DisplayNull table spans 0x6fea4..0x70103; the RTTI locator precedes it.
    struct Table {
        void* entries[1 + 0x260 / sizeof(void*)];
    };
    auto& table = *reinterpret_cast<void***>(display);
    static Table mirror = [&] {
        Table result;
        std::memcpy(result.entries, table - 1, sizeof result.entries);
        result.entries[1 + 0x98 / sizeof(void*)] = reinterpret_cast<void*>(&CreateNullTexture);
        return result;
    }();
    table = mirror.entries + 1;
}
}

void __cdecl RsDisplaySetSelector(Display*, void*);

bool __cdecl RsLoadRenderSystem(RenderKernel* kernel, DisplaySelector* selector)
{
    auto* const os = Field<OS*>(kernel, 0x8c);
    const auto* const options = os->GetOptions();
    auto& name = Field<String>(kernel, 0xac);
    auto& jpeg = Field<bool>(kernel, 0xb0);
    auto& start = Field<unsigned>(kernel, 0xa0);
    auto& step = Field<unsigned>(kernel, 0xa8);
    options->Get("ScreenDumpFile", name, name);
    options->Get("ScreenDumpJPG", jpeg, jpeg);
    if (options->Get("DumpStart", start, start)) {
        options->Get("DumpEnd", Field<unsigned>(kernel, 0xa4), ~0u);
        options->Get("DumpStep", step, step);
        Field<bool>(kernel, 0x99) = true;
    }

    auto& display = Field<Display*>(kernel, 0x7c);
    const auto open = [&] {
        return VCall<bool>(display, 0, os) && VCall<bool>(display, 0x34, options, false);
    };
    if (options->Get("NullRender")) {
        // DisplayNull is private. Keep its constructor and vtable in the original for now.
        const auto image = reinterpret_cast<const unsigned char*>(GetModuleHandleA("RenderSystem_7c_orig.dll"));
        const auto construct = reinterpret_cast<Display*(__stdcall*)(void*)>(image + 0x1e130);
        std::unique_ptr<void, decltype(&Alloc::Delete)> storage(Alloc::New(0xf28), &Alloc::Delete);
        display = storage ? construct(storage.get()) : nullptr;
        if (display) {
            storage.release();
            InstallNullTextureFactory(display);
            RsDisplaySetSelector(display, selector);
            if (open()) return true;
            ReleaseDisplay(display);
        }
    }

    options->SetSeparators("Render", ",");
    for (PString backend; options->GetNext("Render", backend); backend = PString()) {
        const PluginCollection candidates(UId("FA4A2250-6BCC-4642-ADE6-FCDA23A80123"), backend);
        for (auto it = candidates.First(); it != candidates.End(); ++it) {
            const auto* config = *it;
            PluginType* type = nullptr;
            if (DisplayPluginLookup::LookupPluginType(&type, config->GetParent()->GetId(), config->GetName()).code)
                continue;
            const auto release = [](PluginType* factory) { VCall<void>(factory, 4); };
            std::unique_ptr<PluginType, decltype(release)> factory(type, release);
            if (!factory) continue;
            display = VCall<Display*>(factory.get(), 0x0c);
            if (!display) continue;
            RsDisplaySetSelector(display, selector);
            if (!VCall<bool>(display, 4, os)) {
                ReleaseDisplay(display);
                continue;
            }
            factory.reset();
            if (open()) return true;
            ReleaseDisplay(display);
            break;
        }
    }
    Debugger::Output(static_cast<Debugger::Level>(1),
                     "Jet::RenderKernel::LoadRenderSystem> Unable to load any render device!\n");
    return false;
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
    Field<OS*>(kernel, 0x8c) = os;
    if (!RsLoadRenderSystem(kernel, selector)) return false;

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

    AsciiTextureResource::SetUsePath(os->GetOptions()->Get("NoAsciiTexPath"));
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

void RsUpdateLayers(void* layers, float dt);
void RsRenderLayers(void* layers);
void RsUpdateScene(SceneGraph* scene, float dt);
void RsPurgeScene(SceneGraph* scene);
Bitmap* __cdecl RsDisplayScreenshot(const Display*);
void RsQueueScreenDump(void*, Bitmap*, const AnsiString&);

namespace {
void TraceRenderStep(int& index, const char* name)
{
    if (index < 0) index = Profile::RegisterTracePoint(name);
    Profile::TracePoint(index);
}

}

int __cdecl RsRenderKernelUpdate(RenderKernel* kernel, float dt)
{
    static Profile::Info info{"RenderKernel::Update", "Jet/RenderSystem/RenderKernel.cpp", 0, 0, -1, 0x80000000u, 0, 0};
    static int app = -1, render = -1, swap = -1, messages = -1, end = -1;
    Profile scope(&info);
    TraceRenderStep(app, "RenderKernel App");
    auto& display = Field<Display*>(kernel, 0x7c);
    auto& scene = Field<SceneGraph*>(kernel, 0x80);
    auto& textures = Field<TextureManager*>(kernel, 0x94);
    auto* const layers = &Field<unsigned char>(kernel, 0x28);
    VCall<void>(display, 0x44, dt);
    RsUpdateScene(scene, dt);
    VCall<void>(textures, 0x20);
    auto& resources = Field<ResourceList>(kernel, 0x88);
    for (auto it = resources.First(); it != resources.End(); ++it) VCall<void>(*it, 0x0c);
    RsUpdateLayers(layers, dt);
    for (auto it = resources.First(); it != resources.End(); ++it) VCall<void>(*it, 0x10);
    VCall<void>(textures, 0x28, dt);
    TraceRenderStep(render, "RenderKernel Render");
    RsRenderLayers(layers);
    VCall<void>(layers + 4, 0x14);
    RsPurgeScene(scene);

    auto& pending = Field<bool>(kernel, 0x99);
    if (pending) {
        const unsigned frame = display->GetCurrentFrame();
        const unsigned single = Field<unsigned>(kernel, 0x9c);
        const unsigned first = Field<unsigned>(kernel, 0xa0);
        const unsigned last = Field<unsigned>(kernel, 0xa4);
        if (frame == single || (first <= frame && frame <= last &&
            (frame - first) % Field<unsigned>(kernel, 0xa8) == 0)) {
            AnsiString name(static_cast<const AnsiString&>(Field<String>(kernel, 0xac)));
            if (Field<bool>(kernel, 0x98)) name.Addf("_%08d", static_cast<int>(frame));
            name.Ins(Field<bool>(kernel, 0xb0) ? ".jpg" : ".tga", name.Length());
            if (auto* bitmap = RsDisplayScreenshot(display); bitmap && Field<bool>(kernel, 0x84))
                RsQueueScreenDump(kernel, bitmap, name);
        } else if (frame > single && frame > last) {
            pending = false;
        }
    }
    TraceRenderStep(swap, "RenderKernel swapBuffers");
    VCall<void>(display, 0x48, dt);
    TraceRenderStep(messages, "RenderKernel msgQueue");
    if (!VCall<bool>(display, 0x4c, dt)) return 2;
    TraceRenderStep(end, "RenderKernel END");
    return 0;
}

}  // namespace Jet
