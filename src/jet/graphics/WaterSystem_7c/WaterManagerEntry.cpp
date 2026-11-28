// Unit U10: JetGetPluginsWaterSystem, the module's plugin entry. Registration order follows the original at
// 0x100162f0: the four material factories, the WaterAnimationPN factory, then the ocean-simulation and generic
// water manager factories under WaterManager::groupUid. The material factory objects are the shared 0x20/0x50
// MaterialPlugin bases with U7a's mirrored vtables; the manager and animation factories are local
// PluginFactoryOf<>s.
#include "WaterMaterialInternal.h"

#include "WaterManager.h"
#include "WaterManagerGeneric.h"
#include "WaterManagerOceanSim.h"

#include <jet/KernelStandard.h>  // Alloc
#include <jet/Plugin.h>
#include <jet/PString.h>
#include <jet/UId.h>
#include <jet/WaterAnimation.h>

#include <cstring>

namespace {

using MaterialPluginCtor = void (__thiscall WsMaterialPluginAbi::*)(const Jet::PString&, const Jet::UId&);

void RegisterMaterialFactory(void* vtable, unsigned int size, const char* name, const char* id, bool fresWave)
{
    WsEnsureMirrors();
    void* const memory = Jet::Alloc::New(size);
    if (!memory)
        return;
    Jet::PString pluginName(name);
    Jet::UId pluginId(id);
    auto* const base = static_cast<WsMaterialPluginAbi*>(memory);
    (base->*WsBind<MaterialPluginCtor>(WsMatPluginCtorImport))(pluginName, pluginId);
    if (fresWave)
        std::memset(static_cast<unsigned char*>(memory) + 0x20, 0, 0x30);
    *static_cast<void**>(memory) = vtable;
}

}  // namespace

extern "C" void __cdecl WsJetGetPluginsWaterSystem()
{
    // 0x10015f10, 0x10015f90, 0x10016010, 0x10016270
    RegisterMaterialFactory(WsBumpVftable, 0x20, "WaterBumpMaterial",
                            "5EA5C88C-A2F1-423a-B5AB-FFB935941E25", false);
    RegisterMaterialFactory(WsOceanVftable, 0x20, "WaterOceanMaterial",
                            "EF9795F6-F515-4aee-A89F-6FBD9CF42FF6", false);
    RegisterMaterialFactory(WsFresWaveVftable, 0x50, "WaterMaterialFresWave",
                            "4F7EA82E-1CB4-47b4-B1C2-2FCEA5A7D7B9", true);
    RegisterMaterialFactory(WsOneTexVftable, 0x20, "WaterMaterialOneTex",
                            "4F7EA82E-1CB4-47b4-B1C2-2FCEA5A7D7CA", false);

    // 0x100160c0
    Jet::RegisterWaterAnimationFactory();

    // 0x10016150, 0x100161e0
    new Jet::PluginFactoryOf<Jet::WaterManagerOceanSim, Jet::WaterManager>(
        Jet::WaterManagerOceanSim::MANAGER_NAME, Jet::UId("FBE986AD-89CD-4298-84C3-7B2D2F347899"),
        Jet::WaterManager::groupUid);
    new Jet::PluginFactoryOf<Jet::WaterManagerGeneric, Jet::WaterManager>(
        Jet::WaterManagerGeneric::MANAGER_NAME, Jet::UId("00A106DC-1BBC-4078-AE59-99EA0AF8042A"),
        Jet::WaterManager::groupUid);
}
