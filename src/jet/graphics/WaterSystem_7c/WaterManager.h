// WaterSystem_7c unit U10 (notes/WaterSystem.md): the water manager plugin base. WaterManager owns the loaded
// WaterManagerConfig and is the plugin the game creates by name ("WaterManagerGeneric" / "WaterManagerOceanSim");
// its four-argument Initialize resolves the config file through the database and hands it to the five-argument
// virtual the derived managers implement. Layouts and behaviour follow the TS12 assembly (addresses per member in
// WaterManager.cpp). The compiler builds the vtables, vbtables, vtordisp and adjusting thunks from this shape:
// 0x178 bytes = {vftable, PluginInstance at +4, WaterManagerConfig +0xc, vtordisp +0x16c, ReferenceCount +0x170}.
#pragma once

#include <jet/Math.h>
#include <jet/Platform.h>
#include <jet/Plugin.h>
#include <jet/PString.h>
#include <jet/UId.h>
#include <jet/WaterManagerConfig.h>

namespace Jet {

class RenderKernel;
class ResourceDatabase;
class Texture;
class WaterMaterial;
class WaterTextureAnimation;

class WaterManager : public PluginInstance, protected PluginBase {
public:
    WaterManager();
    virtual ~WaterManager();

    // vtable slot 0: the derived managers' config-based initialization (not exported by the original).
    // MSVC lays same-named virtual overloads out in reverse declaration order, so the four-argument
    // Initialize is declared first and takes the second slot, as in the retail table.
    virtual bool __thiscall Initialize(RenderKernel* kernel, const ResourceDatabase* db, const PString& name,
                                       int flags);
    virtual bool __thiscall Initialize(RenderKernel* kernel, const ResourceDatabase* db,
                                       const WaterManagerConfig* config, const PString& name, int flags);
    // slots 2..5 (stubs in the original base table)
    virtual bool __thiscall SetReflectionType(int type);
    virtual int __thiscall GetReflectionType() const;
    virtual void __thiscall SetScaleNormXY(float value);
    virtual float __thiscall GetScaleNormXY() const;
    // slot 6
    virtual void __thiscall Update(float time);
    // slot 7
    virtual WaterMaterial* __thiscall GetMaterial();

    const WaterManagerConfig& __thiscall GetConfig() const;  // +0x0c
    bool __thiscall Init();                                   // a "return true" stub in the original

    static WaterManager* __cdecl CreateInstance(const PString& name);

    static const UId groupUid;  // 243CB347-3A8D-409e-9EA4-420CC0CCA611

protected:
    WaterManagerConfig config;  // +0x0c
};
JET_LAYOUT32(sizeof(WaterManager) == 0x178);

}  // namespace Jet
