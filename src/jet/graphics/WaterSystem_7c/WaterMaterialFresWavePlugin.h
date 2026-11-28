// WaterSystem_7c U8b: the two remaining water material factories. The shared RenderSystem MaterialPlugin base is
// imported by name; this local ABI mirror gives the six virtual slots in the original order (0/1 the plugin-type
// reference thunks, 2 the deleting destructor, 3 the bare CreateInstance, 4 IsSupported, 5
// CreateInstance(manager, kernel)). The published vtables are U7a's mirrored arrays; this unit fills their local
// slots and constructs with the derived vptr.
#pragma once

#include <jet/Render3DCoreMaterials.h>
#include <jet/Plugin.h>

#include "WaterMaterialInternal.h"

namespace Jet {

class Display;
class MaterialManager;
class RenderKernel;
class VertexType;

// OneTex's factory has no state of its own (0x20 bytes).
class __declspec(novtable) WaterMaterialOneTexPlugin;

// The FresWave factory's state: the display, the water vertex type and ten texture-stage nodes (0x50 bytes).
class __declspec(novtable) WaterMaterialFresWavePlugin;

// the six-slot ABI every water factory publishes
class __declspec(novtable) WaterMaterialPluginBase {
public:
    WaterMaterialPluginBase();
    virtual ~WaterMaterialPluginBase();

    virtual void __cdecl AddReference() const;
    virtual void __cdecl ReleaseReference() const;
    virtual Material* __cdecl CreateInstance() const { return nullptr; }  // slot 3: the bare form
    virtual bool __cdecl IsSupported(Display* display, PString& reason) = 0;  // slot 4
    virtual Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const = 0;  // slot 5

    UId id;          // +0x04
    PString name;    // +0x14
    void* lib;       // +0x18
    void* parent;    // +0x1c
};
JET_LAYOUT32(sizeof(WaterMaterialPluginBase) == 0x20);

// 0x50 bytes
class __declspec(novtable) WaterMaterialFresWavePlugin : public WaterMaterialPluginBase {
public:
    WaterMaterialFresWavePlugin();
    WaterMaterialFresWavePlugin(const WaterMaterialFresWavePlugin& other);
    ~WaterMaterialFresWavePlugin() override;
    WaterMaterialFresWavePlugin& operator=(const WaterMaterialFresWavePlugin& other);

    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;
    bool __cdecl IsSupported(Display* display, PString& reason) override;
protected:
    WaterMaterialFresWavePlugin(const PString& name, const UId& id);
    void CreatePluginResources(RenderKernel* kernel) const;

public:
    Display* display;        // +0x20
    VertexType* vertexType;  // +0x24
    void* stageNodes[10];    // +0x28 .. +0x4c
};

JET_LAYOUT32(sizeof(WaterMaterialFresWavePlugin) == 0x50);
JET_LAYOUT32(offsetof(WaterMaterialFresWavePlugin, display) == 0x20);
JET_LAYOUT32(offsetof(WaterMaterialFresWavePlugin, vertexType) == 0x24);
JET_LAYOUT32(offsetof(WaterMaterialFresWavePlugin, stageNodes) == 0x28);

// 0x20 bytes
class __declspec(novtable) WaterMaterialOneTexPlugin : public WaterMaterialPluginBase {
public:
    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;
    bool __cdecl IsSupported(Display* display, PString& reason) override;
};
JET_LAYOUT32(sizeof(WaterMaterialOneTexPlugin) == 0x20);

// fills the local slots (0/1/2/3/5 for FresWave; 0/1/3/5 for OneTex) of U7a's mirrored tables
void EnsureWaterPluginTables();

}  // namespace Jet


