// WaterSystem_7c U8b: the FresWave factory's lifetime, resources and CreateInstance, plus OneTex's factory
// CreateInstance. The mirrored vtables U7a publishes get their local slots here.
#include "WaterMaterialFresWavePlugin.h"

#include <jet/Geometry.h>
#include <jet/RenderSystem.h>
#include <jet/VCall.h>

#include "WaterMaterialFresWave.h"
#include "WaterMaterialOneTex.h"

#include <cstring>

extern "C" void* WsPluginAddReferenceImport;
extern "C" void* WsPluginReleaseReferenceImport;
extern "C" void* WsPluginAssignImport;
#pragma comment(linker, "/alternatename:_WsPluginAddReferenceImport=__imp_?AddReference@PluginType@Jet@@UBAXXZ")
#pragma comment(linker, "/alternatename:_WsPluginReleaseReferenceImport=__imp_?ReleaseReference@PluginType@Jet@@UBAXXZ")
#pragma comment(linker, "/alternatename:_WsPluginAssignImport=__imp_?Assign@PluginInstance@Jet@@AAAXPBVPluginFactory@2@@Z")

namespace Jet {

namespace {

using BaseCtor = void (__thiscall WsMaterialPluginAbi::*)(const PString&, const UId&);
using BaseCopy = void (__thiscall WsMaterialPluginAbi::*)(const WsMaterialPluginAbi&);
using BaseAssign = WsMaterialPluginAbi& (__thiscall WsMaterialPluginAbi::*)(const WsMaterialPluginAbi&);
using BaseDtor = void (__thiscall WsMaterialPluginAbi::*)();
using DeleteFn = void(__cdecl*)(void*);
using PluginRefFn = void(__cdecl*)(const void*);
using AssignFn = void(__cdecl*)(void*, const void*);

const void* const EmptyPluginObjects[8] = {};

void SetVtable(void* self, void* table)
{
    WsEnsureMirrors();
    *static_cast<void**>(self) = table;
}

void BaseConstruct(WsMaterialPluginAbi* self, const PString& name, const UId& id)
{
    (self->*WsBind<BaseCtor>(WsMatPluginCtorImport))(name, id);
}

void BaseCopyConstruct(WsMaterialPluginAbi* self, const WsMaterialPluginAbi& other)
{
    (self->*WsBind<BaseCopy>(WsMatPluginCopyImport))(other);
}

void BaseAssignImpl(WsMaterialPluginAbi* self, const WsMaterialPluginAbi& other)
{
    (self->*WsBind<BaseAssign>(WsMatPluginAssignImport))(other);
}

void BaseDestroy(WsMaterialPluginAbi* self)
{
    (self->*WsBind<BaseDtor>(WsMatPluginDtorImport))();
}

void PluginAddReference(void* self)
{
    reinterpret_cast<PluginRefFn>(WsPluginAddReferenceImport)(self);
}

void PluginReleaseReference(void* self)
{
    reinterpret_cast<PluginRefFn>(WsPluginReleaseReferenceImport)(self);
}

// the destructor body shared by ~WaterMaterialFresWavePlugin and the vtable's vector destructor
void DestroyFresWavePlugin(WaterMaterialFresWavePlugin* self)
{
    SetVtable(self, WsFresWaveVftable);
    if (self->vertexType) self->vertexType->ReleaseReference();
    if (self->display) {
        for (int i = 0; i < 10; ++i)
            if (self->stageNodes[i]) VCall<void>(self->display, 0x124, self->stageNodes[i]);
    }
    BaseDestroy(reinterpret_cast<WsMaterialPluginAbi*>(self));
}

struct FresWaveThunks {
    void* __thiscall VectorDtor(unsigned flags);
};

void* __thiscall FresWaveThunks::VectorDtor(unsigned flags)
{
    auto* const self = reinterpret_cast<WaterMaterialFresWavePlugin*>(this);
    void* const base = self;
    if (flags & 2) {
        unsigned* const cookie = reinterpret_cast<unsigned*>(self);
        const unsigned count = cookie[-1];
        for (unsigned i = 0; i < count; ++i) DestroyFresWavePlugin(self + i);
        if (flags & 1) reinterpret_cast<DeleteFn>(WsAllocDeleteImport)(base);
        return reinterpret_cast<unsigned char*>(base) - sizeof(unsigned);
    }
    DestroyFresWavePlugin(self);
    if (flags & 1) reinterpret_cast<DeleteFn>(WsAllocDeleteImport)(base);
    return base;
}


template <class Fn>
void* MemberAddress(Fn fn)
{
    union {
        Fn member;
        void* code;
    } u;
    u.member = fn;
    return u.code;
}

bool tablesReady;

}  // namespace

// the local slots of U7a's mirrored factory tables (RTTI locator and the untouched slots stay the original's)
void EnsureWaterPluginTables()
{
    if (tablesReady) return;
    WsEnsureMirrors();
    void* const addRef = reinterpret_cast<void*>(static_cast<void(__cdecl*)(void*)>(&PluginAddReference));
    void* const releaseRef = reinterpret_cast<void*>(static_cast<void(__cdecl*)(void*)>(&PluginReleaseReference));
    WsFresWaveVftable[0] = addRef;
    WsFresWaveVftable[1] = releaseRef;
    WsFresWaveVftable[2] = MemberAddress(&FresWaveThunks::VectorDtor);
    WsFresWaveVftable[3] = MemberAddress(static_cast<Material* (__cdecl WaterMaterialPluginBase::*)() const>(&WaterMaterialPluginBase::CreateInstance));
    WsFresWaveVftable[5] = MemberAddress(&WaterMaterialFresWavePlugin::CreateInstance);
    WsOneTexVftable[0] = addRef;
    WsOneTexVftable[1] = releaseRef;
    WsOneTexVftable[3] = MemberAddress(static_cast<Material* (__cdecl WaterMaterialPluginBase::*)() const>(&WaterMaterialPluginBase::CreateInstance));
    WsOneTexVftable[5] = MemberAddress(&WaterMaterialOneTexPlugin::CreateInstance);
    tablesReady = true;
}

// ---------------------------------------------------------------------------------------------------------------
// WaterMaterialPluginBase

WaterMaterialPluginBase::WaterMaterialPluginBase()
{
    WsEnsureMirrors();
    BaseConstruct(reinterpret_cast<WsMaterialPluginAbi*>(this),
                  *static_cast<const PString*>(static_cast<const void*>(EmptyPluginObjects)),
                  *static_cast<const UId*>(static_cast<const void*>(EmptyPluginObjects + 4)));
}

WaterMaterialPluginBase::~WaterMaterialPluginBase() { BaseDestroy(reinterpret_cast<WsMaterialPluginAbi*>(this)); }

void __cdecl WaterMaterialPluginBase::AddReference() const { PluginAddReference(const_cast<WaterMaterialPluginBase*>(this)); }
void __cdecl WaterMaterialPluginBase::ReleaseReference() const { PluginReleaseReference(const_cast<WaterMaterialPluginBase*>(this)); }

// ---------------------------------------------------------------------------------------------------------------
// WaterMaterialFresWavePlugin

// 0x10012a00
WaterMaterialFresWavePlugin::WaterMaterialFresWavePlugin()
{
    EnsureWaterPluginTables();
    BaseConstruct(reinterpret_cast<WsMaterialPluginAbi*>(this), WaterMaterialFresWave::UNDECORATED_TYPE,
                  WaterMaterialFresWave::uid);
    SetVtable(this, WsFresWaveVftable);
    display = nullptr;
    vertexType = nullptr;
    std::memset(stageNodes, 0, sizeof stageNodes);
}

// 0x12a50 (protected: the factory the plugin system makes by name and id)
WaterMaterialFresWavePlugin::WaterMaterialFresWavePlugin(const PString& name, const UId& id)
{
    EnsureWaterPluginTables();
    BaseConstruct(reinterpret_cast<WsMaterialPluginAbi*>(this), name, id);
    SetVtable(this, WsFresWaveVftable);
    display = nullptr;
    vertexType = nullptr;
    std::memset(stageNodes, 0, sizeof stageNodes);
}

// 0x12900
WaterMaterialFresWavePlugin::WaterMaterialFresWavePlugin(const WaterMaterialFresWavePlugin& other)
{
    EnsureWaterPluginTables();
    BaseCopyConstruct(reinterpret_cast<WsMaterialPluginAbi*>(this), *reinterpret_cast<const WsMaterialPluginAbi*>(&other));
    SetVtable(this, WsFresWaveVftable);
    display = other.display;
    vertexType = other.vertexType;
    std::memcpy(stageNodes, other.stageNodes, sizeof stageNodes);
}

// 0x12a80
WaterMaterialFresWavePlugin::~WaterMaterialFresWavePlugin() { DestroyFresWavePlugin(this); }

// 0x12940
WaterMaterialFresWavePlugin& WaterMaterialFresWavePlugin::operator=(const WaterMaterialFresWavePlugin& other)
{
    BaseAssignImpl(reinterpret_cast<WsMaterialPluginAbi*>(this), *reinterpret_cast<const WsMaterialPluginAbi*>(&other));
    display = other.display;
    vertexType = other.vertexType;
    std::memcpy(stageNodes, other.stageNodes, sizeof stageNodes);
    return *this;
}

// 0x13f10 (the shared always-true body)
bool __cdecl WaterMaterialFresWavePlugin::IsSupported(Display*, PString&) { return true; }

// 0x12b20
void WaterMaterialFresWavePlugin::CreatePluginResources(RenderKernel* kernel) const
{
    if (vertexType || !kernel) return;
    auto* const self = const_cast<WaterMaterialFresWavePlugin*>(this);
    self->display = kernel->GetDisplay();
    if (!self->display) return;

    void* const memory = Alloc::New(0x24);
    auto* const type = memory ? ::new (memory) VertexType(static_cast<VertexTypeID>(99)) : nullptr;
    self->vertexType = type;
    if (!type) return;
    type->CreateStreams(1);
    type->CreateItems(0, 4);
    const PString empty;
    type->AddItem(0, static_cast<VertexItemType>(1), -1, static_cast<DVertexItemDataType>(0),
                  static_cast<VertexItemSemanticType>(1), empty, -1, static_cast<VertexType::BoolEnum>(-1));
    type->AddItem(0, static_cast<VertexItemType>(0x18), 2, static_cast<DVertexItemDataType>(7),
                  static_cast<VertexItemSemanticType>(1), empty, -1, static_cast<VertexType::BoolEnum>(-1));
    type->AddItem(0, static_cast<VertexItemType>(0x19), 4, static_cast<DVertexItemDataType>(7),
                  static_cast<VertexItemSemanticType>(1), empty, -1, static_cast<VertexType::BoolEnum>(-1));
    type->AddItem(0, static_cast<VertexItemType>(0xf), -1, static_cast<DVertexItemDataType>(0),
                  static_cast<VertexItemSemanticType>(1), empty, -1, static_cast<VertexType::BoolEnum>(-1));
    type->RegisterName(WaterMaterialFresWave::vertexName);
    VCall<void>(self->display, 0x38, type, 0, 0);

    // the ten stage nodes: {slot, flag, first TextureArgs, second TextureArgs}
    struct Stage {
        int slot, flag, first, second;
    };
    const Stage stages[10] = {{0x28, 0, 0, 1},  {0x2c, 1, 2, 3},  {0x30, 0, 10, 11}, {0x34, 1, 12, 13},
                              {0x40, 0, 14, 16}, {0x44, 0, 15, 16}, {0x48, 1, 17, 18}, {0x4c, 0, 4, 5},
                              {0x38, 0, 6, 7},   {0x3c, 1, 8, 9}};
    auto create = [self](int slot, int flag, int first, int second) {
        void* const node = CreateWaterStageNode(self->display, flag, first, second);
        self->stageNodes[(slot - 0x28) / 4] = node;
        return node != nullptr;
    };
    auto destroy = [self](int slot) {
        void*& node = self->stageNodes[(slot - 0x28) / 4];
        if (node) VCall<void>(self->display, 0x124, node);
        node = nullptr;
    };

    if (!create(0x28, 0, 0, 1)) return;
    if (!create(0x2c, 1, 2, 3)) destroy(0x28);
    if (!create(0x30, 0, 10, 11)) return;
    if (!create(0x34, 1, 12, 13)) destroy(0x30);
    if (create(0x40, 0, 14, 16)) {
        create(0x44, 0, 15, 16);
        create(0x48, 1, 17, 18);
        if (!stageNodes[6] || !stageNodes[7]) {
            destroy(0x30);
            destroy(0x44);
            destroy(0x48);
        }
    }
    create(0x4c, 0, 4, 5);
    if (!create(0x38, 0, 6, 7)) return;
    if (!create(0x3c, 1, 8, 9)) destroy(0x38);
}

// 0x12f00
Material* __cdecl WaterMaterialFresWavePlugin::CreateInstance(MaterialManager* manager, RenderKernel* kernel) const
{
    CreatePluginResources(kernel);
    void* const memory = Alloc::New(0x21c);
    WaterMaterialFresWave* const material =
        memory ? ::new (memory) WaterMaterialFresWave(manager, const_cast<void**>(stageNodes)) : nullptr;
    if (material) {
        reinterpret_cast<AssignFn>(WsPluginAssignImport)(reinterpret_cast<unsigned char*>(material) + 4, this);
    }
    return material;
}

// 0x13f20
Material* __cdecl WaterMaterialOneTexPlugin::CreateInstance(MaterialManager* manager, RenderKernel*) const
{
    void* const memory = Alloc::New(0xf4);
    WaterMaterialOneTex* const material = memory ? ::new (memory) WaterMaterialOneTex(manager) : nullptr;
    if (material)
        reinterpret_cast<AssignFn>(WsPluginAssignImport)(reinterpret_cast<unsigned char*>(material) + 4, this);
    return material;
}

bool __cdecl WaterMaterialOneTexPlugin::IsSupported(Display*, PString&) { return true; }

}  // namespace Jet




