// U7a/U7b/U8b: the four water material factories' published vtables. Slots 0, 1 and 3 are the imported
// PluginType/MaterialPlugin base methods (AddReference, ReleaseReference, bare CreateInstance), slot 2 the
// local deleting destructor, slot 4 the local IsSupported and slot 5 the local factory CreateInstance.
// The tables are built locally; no WaterSystem_7c_orig.dll is read.
#include <windows.h>

#include <cstddef>
#include <cstring>

#include "WaterMaterialInternal.h"

#pragma warning(disable : 4073)
#pragma init_seg(lib)

#pragma section("WsVtBump", read, write)
__declspec(allocate("WsVtBump")) void* WsBumpVftableMeta;
__declspec(allocate("WsVtBump")) void* WsBumpVftable[6];

#pragma section("WsVtFres", read, write)
__declspec(allocate("WsVtFres")) void* WsFresWaveVftableMeta;
__declspec(allocate("WsVtFres")) void* WsFresWaveVftable[6];

#pragma section("WsVtOne", read, write)
__declspec(allocate("WsVtOne")) void* WsOneTexVftableMeta;
__declspec(allocate("WsVtOne")) void* WsOneTexVftable[6];

#pragma section("WsVtOcean", read, write)
__declspec(allocate("WsVtOcean")) void* WsOceanVftableMeta;
__declspec(allocate("WsVtOcean")) void* WsOceanVftable[6];

// the three base methods the original's vtable slots 0, 1 and 3 jump to; each import slot holds the address
extern "C" void* WsPluginTypeAddImport;
extern "C" void* WsPluginTypeReleaseImport;
extern "C" void* WsMaterialPluginBareCreateImport;
#pragma comment(linker, "/alternatename:_WsPluginTypeAddImport=__imp_?AddReference@PluginType@Jet@@UBAXXZ")
#pragma comment(linker, "/alternatename:_WsPluginTypeReleaseImport=__imp_?ReleaseReference@PluginType@Jet@@UBAXXZ")
#pragma comment(linker, \
                "/alternatename:_WsMaterialPluginBareCreateImport=__imp_?CreateInstance@MaterialPlugin@Jet@@MBAPAVMaterial@2@XZ")

namespace {

template <class Fn>
void* SlotAddress(Fn fn)
{
    union { Fn member; void* code; } u;
    u.member = fn;
    return u.code;
}

bool mirrorsReady;

void FillMirrors()
{
    if (mirrorsReady) return;

    void* const addRef = *reinterpret_cast<void**>(WsPluginTypeAddImport);
    void* const releaseRef = *reinterpret_cast<void**>(WsPluginTypeReleaseImport);
    void* const bareCreate = *reinterpret_cast<void**>(WsMaterialPluginBareCreateImport);

    // The hand-written factory tables have no generated RTTI. Point the locator word at RenderSystem's
    // MaterialPlugin locator so a dynamic_cast to the plugin base still finds a valid hierarchy; the concrete
    // factory type has no local RTTI by design.
    void* locator = nullptr;
    if (HMODULE const rs = GetModuleHandleA("RenderSystem_7c.dll")) {
        if (void* const pluginVtable = reinterpret_cast<void*>(
                GetProcAddress(rs, "??_7MaterialPlugin@Jet@@6B@"))) {
            locator = static_cast<void**>(pluginVtable)[-1];
        }
    }

    void* const tables[] = {WsBumpVftable, WsFresWaveVftable, WsOneTexVftable, WsOceanVftable};
    for (void* const table : tables) {
        auto* const slots = static_cast<void**>(table);
        slots[-1] = locator;
        slots[0] = addRef;
        slots[1] = releaseRef;
        slots[3] = bareCreate;
    }

    WsBumpVftable[2] = SlotAddress(&WsVfThunks::BumpVectorDtor);
    WsBumpVftable[4] = SlotAddress(&WsVfThunks::SupportedByDisplay);
    WsOceanVftable[2] = SlotAddress(&WsVfThunks::OceanVectorDtor);
    WsOceanVftable[4] = SlotAddress(&WsVfThunks::SupportedByDisplay);
    WsOneTexVftable[2] = SlotAddress(&WsVfThunks::OneTexVectorDtor);
    WsOneTexVftable[4] = SlotAddress(&WsVfThunks::SupportedAlways);
    // FresWave's deleting destructor is installed by WaterMaterialFresWavePlugin.cpp
    WsFresWaveVftable[4] = SlotAddress(&WsVfThunks::SupportedAlways);

    WsInstallFactoryCreate();
    mirrorsReady = true;
}

struct MirrorStatics {
    MirrorStatics() { FillMirrors(); }
} mirrorStatics;

}  // namespace

void WsEnsureMirrors()
{
    FillMirrors();
}
