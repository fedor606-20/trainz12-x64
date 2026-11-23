// Win32 Display lifetime. A derived renderer supplies its own virtual-base offset.
#include <jet/RenderSystem.h>
#include <jet/Window.h>
#include <windows.h>

#include <cstring>
#include <initializer_list>

namespace Jet {

extern "C" void* __fastcall RsDisplayContextCtor(unsigned char*, void*);

namespace {
constexpr int DisplayReferenceBase = 0xf04;

template<class T>
T& At(unsigned char* object, int offset)
{
    return *reinterpret_cast<T*>(object + offset);
}

template<class T>
T Symbol(const char* module, const char* name)
{
    return reinterpret_cast<T>(GetProcAddress(GetModuleHandleA(module), name));
}

void InstallDisplayTables(unsigned char* display)
{
    // Derived backends still use retail RTTI and virtual dispatch.
    At<void*>(display, 0) = Symbol<void*>("RenderSystem_7c_orig.dll", "??_7Display@Jet@@6B01@@");
    const int offset = At<const int*>(display, 4)[1];
    At<void*>(display, offset + 4) =
        Symbol<void*>("RenderSystem_7c_orig.dll", "??_7Display@Jet@@6BPluginInstance@1@@");
    At<int>(display, offset) = offset - (DisplayReferenceBase - 4);
}

void DestroyReferenceBase(void* base)
{
    Symbol<void(__thiscall*)(void*)>("KernelStandard_7c.dll", "??1ReferenceCount@Jet@@MAE@XZ")(base);
}
}

void* __fastcall RsDisplayCtor(unsigned char* display, void*, bool createContext, int mostDerived)
{
    if (mostDerived) {
        At<const int*>(display, 4) = Symbol<const int*>("RenderSystem_7c_orig.dll", "??_8Display@Jet@@7B@");
        At<void*>(display, DisplayReferenceBase) =
            Symbol<void*>("KernelStandard_7c.dll", "??_7ReferenceCount@Jet@@6B@");
        At<unsigned>(display, DisplayReferenceBase + 4) = 1;
    }

    try {
        Symbol<void*(__thiscall*)(void*, int)>("Kernel_7c.dll", "??0PluginInstance@Jet@@QAE@XZ")(display + 4, 0);
        InstallDisplayTables(display);

        // Do not initialize padding or backend-owned bytes.
        std::memset(display + 0x14, 0, 0x34);
        std::memset(display + 0x4c, 0, 0x14);
        std::memset(display + 0x80, 0, 99 * sizeof(void*));
        std::memset(display + 0x20c, 0, 0x14);
        std::memset(display + 0x224, 0, 0x70);
        std::memset(display + 0x298, 0, 0x60);
        for (int offset : {0x0c, 0x10, 0x2f4}) At<float>(display, offset) = 1.0f;
        for (int offset : {0x2a8, 0x2d4}) At<int>(display, offset) = -1;
        for (int offset : {0x49, 0x4a, 0x7a, 0x7b, 0x213, 0x294}) display[offset] = 1;
        for (int offset : {0x48, 0x68, 0x70, 0x78, 0x79, 0x7c, 0x220}) display[offset] = 0;
        At<unsigned>(display, 0x60) = 0x3a83126f;
        At<unsigned>(display, 0x64) = 0;
        At<unsigned>(display, 0x6c) = 0x3faaaaab;
        display[0x71] &= 0xf0;
        At<float>(display, 0x74) = 0.5f;
        At<void*>(display, 0xef8) = nullptr;
        At<void*>(display, 0xefc) = nullptr;
        if (createContext) {
            auto* storage = static_cast<unsigned char*>(Alloc::New(0x158));
            auto* context = storage ? RsDisplayContextCtor(storage, nullptr) : nullptr;
            At<void*>(display, 0x54) = At<void*>(display, 0x58) = context;
        }
        return display;
    } catch (...) {
        if (mostDerived) DestroyReferenceBase(display + DisplayReferenceBase);
        throw;
    }
}

extern "C" void __fastcall RsDisplayDtor(unsigned char* adjusted)
{
    auto* display = adjusted - DisplayReferenceBase;
    InstallDisplayTables(display);
    if (auto& shutdown = At<void*>(display, 0xef8); shutdown) {
        ExceptionHandler::RemoveCriticalShutdown(shutdown);
        shutdown = nullptr;
    }
    if (auto& startup = At<void*>(display, 0xefc); startup) {
        ExceptionHandler::RemoveCriticalStartup(startup);
        startup = nullptr;
    }
    if (auto* os = At<OS*>(display, 0x44)) os->SetWindow(nullptr);
    auto& window = At<Window*>(display, 0x40);
    if (window) {
        window->~Window();
        Alloc::Delete(window);
    }
    window = nullptr;
    if (auto* selector = At<ReferenceCount*>(display, 0x288)) selector->ReleaseReference();
    auto& context = At<void*>(display, 0x58);
    if (context) VDelete(context);
    context = nullptr;
    for (int offset : {0x25c, 0x230}) {
        At<unsigned>(display, offset) = 0;
        if (auto* entries = At<void*>(display, offset + 8)) Alloc::Delete(entries);
    }
}

extern "C" void __fastcall RsDisplayVbaseDtor(unsigned char* display)
{
    auto* base = display + DisplayReferenceBase;
    RsDisplayDtor(base);
    DestroyReferenceBase(base);
}

}  // namespace Jet
