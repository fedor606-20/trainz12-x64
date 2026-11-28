// Unit U11 of Render3DCore_7c: JetGetPluginsRender3DCore, the module's plugin entry (0x100608e0).
//
// The entry is a fixed registration order: fourteen material plugin factories, the SplineResource resource type
// and, last, ColorOneTexture. Each factory allocated its plugin with Alloc::New and installed the class vtable;
// the first four helpers (AutoTex, NoTexture, OneTexture, OneTextureNoAlpha) then registered the animatedLit
// vertex program through the shared helper at 0x10060050. Billboard (0x10060310) and the SplineResource
// ResourceType (0x10060770) have no exported constructor of their own.
//
// The plugin classes belong to U5a/U5b/U6, so this unit does not re-implement them. A factory whose default
// constructor is already local is called through its export (U5a2's aliases); every other export still forwards,
// and the two constructor-less helpers are called at their RVA in the original. That keeps every created object
// byte-identical to the original and keeps the registration order the retail one.
#include <jet/KernelStandard.h>

#include <windows.h>

namespace {

// the four factory helpers that call the shared animatedLit program registration at 0x10060050
constexpr unsigned kRegisterProgramRva = 0x60050;

struct Factory {
    unsigned int size;
    const char* constructor;  // mangled default constructor, or null when the original helper has to run
    unsigned int helperRva;   // the original's static registration helper
    bool registerProgram;
};

// The material plugin classes belong to U5a/U5b. The eight U5a2 classes already have a local constructor, so they
// are registered through their export; the MultiPass plugins (U5b), Billboard's template instance and the
// SplineResource resource type have no local class in this unit, so their original static helper runs. That keeps
// every created object byte-identical to the retail one without copying another unit's class.
const Factory kFactories[] = {
    {0x2c, "??0AutoTexMaterialPlugin@Jet@@QAE@XZ", 0x60290, true},
    {0x30, "??0NoTextureMaterialPlugin@Jet@@QAE@XZ", 0x600e0, true},
    {0x34, "??0OneTextureMaterialPlugin@Jet@@QAE@XZ", 0x60170, true},
    {0x34, "??0OneTextureNoAlphaMaterialPlugin@Jet@@QAE@XZ", 0x60200, true},
    {0, nullptr, 0x60310, false},  // StdMaterialPluginTemplate<BillboardMaterial>
    {0x5c, "??0GlossMaterialPlugin@Jet@@QAE@XZ", 0x60390, false},
    {0x28, "??0GenericShaderMaterialPlugin@Jet@@QAE@XZ", 0x60410, false},
    {0x5c, "??0ReflectiveMaterialPlugin@Jet@@QAE@XZ", 0x60490, false},
    {0, nullptr, 0x60510, false},  // MultiPassBumpMaterialPlugin
    {0, nullptr, 0x60570, false},  // MultiPassBump2MaterialPlugin
    {0, nullptr, 0x605d0, false},  // MultiPassBumpSpecMaterialPlugin
    {0, nullptr, 0x60630, false},  // MultiPassBumpSpec2MaterialPlugin
    {0, nullptr, 0x60690, false},  // MultiPassNoTextureMaterialPlugin
    {0, nullptr, 0x60710, false},  // MultiPassOneTextureMaterialPlugin
    {0, nullptr, 0x60770, false},  // ResourceType<SplineResource>
    {0x34, "??0ColorOneTextureMaterialPlugin@Jet@@QAE@XZ", 0x60860, false},
};

HMODULE SelfModule()
{
    static HMODULE module = [] {
        HMODULE found = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&SelfModule), &found);
        return found;
    }();
    return module;
}

HMODULE OrigModule()
{
    static HMODULE module = [] {
        HMODULE found = GetModuleHandleA("Render3DCore_7c_orig.dll");
        return found ? found : LoadLibraryA("Render3DCore_7c_orig.dll");
    }();
    return module;
}

void CallOriginal(unsigned int rva)
{
    HMODULE const module = OrigModule();
    if (!module) return;
    reinterpret_cast<void(__cdecl*)()>(reinterpret_cast<unsigned char*>(module) + rva)();
}

// allocate and default-construct a plugin whose constructor export exists (local or still forwarded)
bool CreatePlugin(const Factory& factory)
{
    HMODULE const self = SelfModule();
    if (!self) return false;
    FARPROC const raw = GetProcAddress(self, factory.constructor);
    if (!raw) return false;
    void* const memory = Jet::Alloc::New(factory.size);
    if (memory) reinterpret_cast<void(__thiscall*)(void*)>(raw)(memory);
    return true;
}

}  // namespace

extern "C" void __cdecl JetGetPluginsRender3DCore()
{
    for (const Factory& factory : kFactories) {
        if (!factory.constructor || !CreatePlugin(factory)) {
            // the class has no exported default constructor in this module, or the export is missing: run the
            // original's static helper, which allocates, constructs and (for its four simple plugins) registers
            // the animatedLit program exactly as before
            CallOriginal(factory.helperRva);
            continue;
        }
        if (factory.registerProgram) CallOriginal(kRegisterProgramRva);
    }
}
