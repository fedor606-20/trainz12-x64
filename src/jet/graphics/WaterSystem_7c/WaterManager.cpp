// Unit U10: WaterManager, the water manager plugin base. Behaviour follows the TS12 assembly (addresses per
// function).
#include "WaterManager.h"

#include <jet/KernelStandard.h>  // Alloc, File::Path
#include <jet/Resource.h>        // ResourceDatabase
#include <jet/VCall.h>

namespace Jet {

// 0x10038ff8 (the id the two manager factories are registered under)
const UId WaterManager::groupUid("243CB347-3A8D-409e-9EA4-420CC0CCA611");

// 0x100092a0
WaterManager::WaterManager()
{
}

// 0x10009360
WaterManager::~WaterManager() = default;

// 0x100056e0
const WaterManagerConfig& WaterManager::GetConfig() const
{
    return config;
}

// 0x10013f10 (shared body: returns true)
bool WaterManager::Init()
{
    return true;
}

// 0x100093e0: the factory registered under groupUid, its slot-3 CreateInstance called raw, then released
WaterManager* WaterManager::CreateInstance(const PString& name)
{
    PluginType* plugin = nullptr;
    const Result result = PluginBase::LookupPluginType(&plugin, groupUid, name);
    if (result.code != 0)
        plugin = nullptr;
    WaterManager* instance = nullptr;
    if (plugin)
        instance = VCall<WaterManager*>(plugin, 0xC);
    if (plugin)
        plugin->ReleaseReference();
    return instance;
}

// 0x10009460: resolve the config file through the database, load it, then run the five-argument virtual with
// an interned copy of the resolved file name (the fourth argument the derived managers receive)
bool WaterManager::Initialize(RenderKernel* kernel, const ResourceDatabase* db, const PString& name, int flags)
{
    File::Path path(name.Text());
    path.DelName(false);

    PString fileName = db->GetFileName(name);
    if (!config.Load(fileName, nullptr))
        return false;
    PString configName(fileName);
    return Initialize(kernel, db, &config, configName, flags);
}

// 0x10015db0: the shared "return 0" stubs of the base vtable
bool WaterManager::Initialize(RenderKernel*, const ResourceDatabase*, const WaterManagerConfig*, const PString&, int)
{
    return false;
}

bool WaterManager::SetReflectionType(int)
{
    return false;
}

int WaterManager::GetReflectionType() const
{
    return 0;
}

void WaterManager::SetScaleNormXY(float)
{
}

float WaterManager::GetScaleNormXY() const
{
    return 0.0f;
}

void WaterManager::Update(float)
{
}

WaterMaterial* WaterManager::GetMaterial()
{
    return nullptr;
}

}  // namespace Jet
