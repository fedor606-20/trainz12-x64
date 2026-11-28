// Render3DCore_7c U5b last wave: the six multipass material plugins' factory, cache, light-render and built-in
// program entries. The bodies follow 0x2f4c0..0x39620 of the original; the RenderSystem managers are reached
// through their exported entry points.
#include "U5bLast.h"

#include <jet/DynamicArray.h>
#include <jet/GfxLanguage.h>
#include <jet/PString.h>

#include <cstring>

#include <windows.h>

namespace Jet {
namespace MultiPass {
namespace Last {

using Final::AddVBaseRef;
using Final::Bytes;
using Final::Local;
using Final::Original;
using Final::ReleaseVBaseRef;
using Final::Thunks;

namespace {

constexpr unsigned int kPluginBlendCache = 0x20;
constexpr unsigned int kPluginChunkCache = 0x24;
constexpr unsigned int kPluginLightRender0 = 0x28;
constexpr unsigned int kPluginLightRender1 = 0x2c;
constexpr unsigned int kPluginVertexType = 0x30;
constexpr unsigned int kMlrAmbient = 0x08;
constexpr unsigned int kMlrParallel = 0x14;
constexpr unsigned int kMlrPoint = 0x20;
constexpr unsigned int kMlrSpot = 0x2c;
constexpr unsigned int kMlrCube = 0x38;
constexpr unsigned int kMlrCubeSpot = 0x44;

void* RenderSystemProc(const char* name)
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("RenderSystem_7c.dll");
        if (!m) m = LoadLibraryA("RenderSystem_7c.dll");
        return m;
    }();
    return module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
}
void* KernelStandardProc(const char* name)
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("KernelStandard_7c.dll");
        if (!m) m = LoadLibraryA("KernelStandard_7c.dll");
        return m;
    }();
    return module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
}

struct LightInitSpec {
    unsigned int arrayOffset;
    unsigned int ctorCounts[5];
    int vpIndex;
    int tc0Index;
    int tc1Index;
    const char* vpLiteral0;
    const char* tcLiteral0;
    const char* vpLiteral1;
    const char* tcLiteral1;
    unsigned int properties[4][3];
    unsigned int propertyCount;
};

const LightInitSpec kAmbientSpec = {kMlrAmbient, {1, 0, 0, 0, 0}, 0, 5, 6, nullptr, nullptr, nullptr, nullptr,
                                    {{3, 4, 0}}, 1};
const LightInitSpec kParallelSpec = {kMlrParallel, {0, 1, 0, 0, 0}, 1, 7, 8, nullptr, nullptr, nullptr, nullptr,
                                     {{0x12, 7, 0}}, 1};
const LightInitSpec kPointSpec = {kMlrPoint, {0, 0, 1, 0, 0}, 2, 9, 10, nullptr, nullptr, nullptr, nullptr,
                                  {{0x13, 5, 0}, {0x15, 6, 0}}, 2};
const LightInitSpec kSpotSpec = {kMlrSpot, {0, 0, 0, 1, 0}, 3, 11, 12, nullptr, nullptr, nullptr, nullptr,
                                 {{0x13, 5, 0}, {0x15, 6, 0}, {0x16, 9, 0}}, 3};
const LightInitSpec kCubeSpec = {kMlrCube, {0, 0, 0, 0, 1}, 4, 13, 14, nullptr, nullptr, nullptr, nullptr,
                                 {{0x13, 5, 1}, {0x15, 6, 1}, {0x17, 9, 1}}, 3};
const LightInitSpec kCubeSpotSpec = {kMlrCubeSpot, {0, 0, 0, 0, 1}, 3, 11, 12, nullptr, nullptr, nullptr, nullptr,
                                     {{0x13, 5, 1}, {0x15, 6, 1}, {0x17, 9, 1}}, 3};

const LightInitSpec kNoTexAmbient = {kMlrAmbient, {1, 0, 0, 0, 0}, -1, -1, -1,
                                     "MPNoTexMat_AmbientLight", "noTex_AmbientLight",
                                     "MPNoTexMat_AmbientLight", "noTex_AmbientLightx2", {{3, 4, 0}}, 1};
const LightInitSpec kNoTexParallel = {kMlrParallel, {0, 1, 0, 0, 0}, -1, -1, -1,
                                      "MPNoTexMat_AmbientLight", "noTex_ParallelLight",
                                      "MPNoTexMat_AmbientLight", "noTex_ParallelLightx2",
                                      {{0xe, 4, 1}, {0x12, 7, 0}, {0xd, 8, 0}}, 3};
const LightInitSpec kNoTexPoint = {kMlrPoint, {0, 0, 1, 0, 0}, -1, -1, -1,
                                   "MPNoTexMat_PointLight", "noTex_PointLight", "MPNoTexMat_PointLightx2",
                                   "noTex_PointLight", {{0x13, 5, 0}, {0x15, 6, 0}}, 2};
const LightInitSpec kNoTexSpot = {kMlrSpot, {0, 0, 0, 1, 0}, -1, -1, -1,
                                  "MPNoTexMat_SpotLight", "noTex_SpotLight", "MPNoTexMat_SpotLightx2",
                                  "noTex_SpotLight", {{0x13, 5, 0}, {0x15, 6, 0}, {0x16, 9, 0}}, 3};
const LightInitSpec kNoTexCube = {kMlrCube, {0, 0, 0, 0, 1}, -1, -1, -1,
                                  "MPNoTexMat_CubeMapLight", "noTex_CubeMapLight", "MPNoTexMat_CubeMapLightx2",
                                  "noTex_CubeMapLight", {{0x13, 5, 1}, {0x15, 6, 1}, {0x17, 9, 1}}, 3};
const LightInitSpec kNoTexCubeSpot = {kMlrCubeSpot, {0, 0, 0, 0, 1}, -1, -1, -1,
                                      "MPNoTexMat_SpotLight", "noTex_SpotLight", "MPNoTexMat_SpotLightx2",
                                      "noTex_SpotLight", {{0x13, 5, 1}, {0x15, 6, 1}, {0x17, 9, 1}}, 3};

const LightInitSpec kOneTexParallel = {kMlrParallel, {0, 1, 0, 0, 0}, 1, 7, 8, nullptr, nullptr, nullptr, nullptr,
                                       {{0xe, 4, 1}, {0x12, 7, 0}, {0xd, 8, 0}}, 3};
const LightInitSpec kBumpSpecParallel = {kMlrParallel, {0, 1, 0, 0, 0}, 1, 7, 8, nullptr, nullptr, nullptr, nullptr,
                                         {{0x12, 7, 0}, {0x14, 0xd, 0}}, 2};
const LightInitSpec kBumpSpecPoint = {kMlrPoint, {0, 0, 1, 0, 0}, 2, 9, 10, nullptr, nullptr, nullptr, nullptr,
                                      {{0x13, 5, 0}, {0x15, 6, 0}, {0x14, 0xd, 0}}, 3};

void AppendMPLightRender(void* arrayAddress, void* render)
{
    unsigned char* const a = static_cast<unsigned char*>(arrayAddress);
    unsigned int& count = *reinterpret_cast<unsigned int*>(a);
    unsigned int& capacity = *reinterpret_cast<unsigned int*>(a + 4);
    void** const dataSlot = reinterpret_cast<void**>(a + 8);
    if (count + 1 > capacity) {
        const unsigned int next = (count + 1 + 2) / 3 * 3;
        void* const block = Alloc::New(next * 4);
        if (*dataSlot && count) std::memcpy(block, *dataSlot, count * 4);
        if (*dataSlot) Alloc::Delete(*dataSlot);
        *dataSlot = block;
        capacity = next;
    }
    static_cast<void**>(*dataSlot)[count] = render;
    ++count;
}

bool CallLoadLightPrograms(void* plugin, void* render, void* kernel, const char* vp, const char* tc, bool swap)
{
    using Fn = bool(__cdecl*)(void*, void*, void*, const char*, const char*);
    void** const vtable = *reinterpret_cast<void***>(plugin);
    // NoTexture's body passes the two names in the opposite slots (its leaf puts the combiner fourth).
    return swap ? reinterpret_cast<Fn>(vtable[0x40 / 4])(plugin, render, kernel, tc, vp)
                : reinterpret_cast<Fn>(vtable[0x40 / 4])(plugin, render, kernel, vp, tc);
}

const char* NameFor(void* plugin, int index, const char* literal)
{
    if (index < 0) return literal;
    return *reinterpret_cast<const char* const*>(Bytes(plugin) + 0x34 + index * 4);
}

bool InitializeLightType(void* plugin, void* kernel, const LightInitSpec& spec)
{
    void* const firstArray = *reinterpret_cast<void* const*>(Bytes(plugin) + kPluginLightRender0);
    void* const secondArray = *reinterpret_cast<void* const*>(Bytes(plugin) + kPluginLightRender1);
    if (!firstArray || !secondArray) return false;

    void* const first = NewMultiPassLightRender(spec.ctorCounts[0], spec.ctorCounts[1], spec.ctorCounts[2],
                                                spec.ctorCounts[3], spec.ctorCounts[4]);
    if (!first) return false;
    AppendMPLightRender(Bytes(firstArray) + spec.arrayOffset, first);
    if (!CallLoadLightPrograms(plugin, first, kernel, NameFor(plugin, spec.vpIndex, spec.vpLiteral0),
                               NameFor(plugin, spec.tc0Index, spec.tcLiteral0), spec.vpIndex < 0))
        return false;
    for (unsigned int i = 0; i < spec.propertyCount; ++i)
        AddVPProperty(first, spec.properties[i][0], spec.properties[i][1], spec.properties[i][2]);

    void* const second = NewMultiPassLightRender(spec.ctorCounts[0], spec.ctorCounts[1], spec.ctorCounts[2],
                                                 spec.ctorCounts[3], spec.ctorCounts[4]);
    if (!second) return false;
    AppendMPLightRender(Bytes(secondArray) + spec.arrayOffset, second);
    if (!CallLoadLightPrograms(plugin, second, kernel, NameFor(plugin, spec.vpIndex, spec.vpLiteral1),
                               NameFor(plugin, spec.tc1Index, spec.tcLiteral1), spec.vpIndex < 0))
        return false;
    for (unsigned int i = 0; i < spec.propertyCount; ++i)
        AddVPProperty(second, spec.properties[i][0], spec.properties[i][1], spec.properties[i][2]);
    return true;
}

void* EnsureCache(void* manager, const char* idName, unsigned int size, const char* ctorName, void** slot)
{
    using FindFn = void*(__cdecl*)(void*, const void*);
    using AddFn = void(__cdecl*)(void*, void*);
    using CtorFn = void*(__thiscall*)(void*);
    static FindFn find = reinterpret_cast<FindFn>(
        RenderSystemProc("?FindCache@RenderingCacheManager@Jet@@QAAPAVRenderingCache@2@ABVPString@2@@Z"));
    static AddFn add =
        reinterpret_cast<AddFn>(RenderSystemProc("?AddCache@RenderingCacheManager@Jet@@QAAXPAVRenderingCache@2@@Z"));
    if (!find || !add) return *slot;
    void* const id = Local(idName);
    if (!id) return *slot;
    void* cache = find(manager, id);
    if (cache) {
        AddVBaseRef(cache, 0xc);
    } else {
        void* const memory = Alloc::New(size);
        CtorFn ctor = reinterpret_cast<CtorFn>(Local(ctorName));
        if (!ctor) ctor = reinterpret_cast<CtorFn>(Original(ctorName));
        cache = (memory && ctor) ? ctor(memory) : nullptr;
        if (!cache) return nullptr;
        add(manager, cache);
    }
    *slot = cache;
    return cache;
}

void* CreateMaterial(PluginFamilyId familyId, void* manager, void* plugin)
{
    const PluginFamily& family = Family(familyId);
    void* const memory = Alloc::New(family.materialSize);
    if (!memory) return nullptr;
    void* const chunk = *reinterpret_cast<void* const*>(Bytes(manager) + 0x24);
    void* const blend = *reinterpret_cast<void* const*>(Bytes(manager) + 0x20);
    void* const light0 = *reinterpret_cast<void* const*>(Bytes(manager) + 0x28);
    void* const light1 = *reinterpret_cast<void* const*>(Bytes(manager) + 0x2c);
    void* const vertexType = *reinterpret_cast<void* const*>(Bytes(manager) + 0x30);

    using Ctor5 = void*(__thiscall*)(void*, void*, void*, void*, void*, void*, int);
    using Ctor6 = void*(__thiscall*)(void*, void*, void*, void*, void*, void*, void*, int);
    void* material = nullptr;
    if (family.materialHasVertexType) {
        Ctor6 ctor = reinterpret_cast<Ctor6>(Local(family.materialCtor));
        if (!ctor) ctor = reinterpret_cast<Ctor6>(Original(family.materialCtor));
        if (ctor) material = ctor(memory, manager, chunk, blend, light0, light1, vertexType, 1);
    } else {
        Ctor5 ctor = reinterpret_cast<Ctor5>(Local(family.materialCtor));
        if (!ctor) ctor = reinterpret_cast<Ctor5>(Original(family.materialCtor));
        if (ctor) material = ctor(memory, manager, chunk, blend, light0, light1, 1);
    }
    if (!material) return nullptr;

    using AssignFn = void(__cdecl*)(void*, const void*);
    static AssignFn assign = reinterpret_cast<AssignFn>(
        KernelStandardProc("?Assign@PluginInstance@Jet@@AAEXPBVPluginFactory@2@@Z"));
    if (!assign) {
        HMODULE kernel = GetModuleHandleA("Kernel_7c.dll");
        if (!kernel) kernel = LoadLibraryA("Kernel_7c.dll");
        if (kernel)
            assign = reinterpret_cast<AssignFn>(
                GetProcAddress(kernel, "?Assign@PluginInstance@Jet@@AAEXPBVPluginFactory@2@@Z"));
    }
    if (assign) assign(Bytes(material) + 4, plugin);
    return material;
}

void* CreateInstance(PluginFamilyId familyId, void* self, void* manager, void* kernel)
{
    using InitBoolFn = bool(__cdecl*)(void*, void*);
    using InitVoidFn = void(__cdecl*)(void*, void*);
    void** const vtable = *reinterpret_cast<void***>(self);
    if (!reinterpret_cast<InitBoolFn>(vtable[0x18 / 4])(self, kernel)) return nullptr;
    reinterpret_cast<InitVoidFn>(vtable[0x20 / 4])(self, kernel);
    if (!reinterpret_cast<InitBoolFn>(vtable[0x1c / 4])(self, kernel)) return nullptr;
    return CreateMaterial(familyId, manager, self);
}

void DelegateFixedVertexType(void* self, void* kernel, const char* name)
{
    using Fn = void(__cdecl*)(void*, void*);
    Fn fn = reinterpret_cast<Fn>(Original(name));
    if (fn) fn(self, kernel);
}

void ReplayPrograms(void* self, const BuiltInProgram* programs, unsigned int count, bool nameTable)
{
    using RegisterFn = void(__cdecl*)(const PString*, const char*);
    static RegisterFn registerVertex = reinterpret_cast<RegisterFn>(
        RenderSystemProc("?RegisterBuiltInPrograms@VertexProgramManager@Jet@@SAXABVPString@2@PBD@Z"));
    static RegisterFn registerCombiner = reinterpret_cast<RegisterFn>(
        RenderSystemProc("?RegisterBuiltInPrograms@TextureCombinerManager@Jet@@SAXABVPString@2@PBD@Z"));
    unsigned int tcIndex = 0;
    for (unsigned int i = 0; i < count; ++i) {
        const BuiltInProgram& program = programs[i];
        const char* name = program.name;
        if (program.vertex) {
            if (registerVertex) {
                PString text(name);
                registerVertex(&text, program.text);
            }
        } else {
            if (!name && nameTable) name = *reinterpret_cast<const char* const*>(Bytes(self) + 0x48 + tcIndex * 4);
            ++tcIndex;
            if (registerCombiner && name) {
                PString text(name);
                registerCombiner(&text, program.text);
            }
        }
    }
}

}  // namespace

const PluginFamily& Family(PluginFamilyId id)
{
    static const PluginFamily families[kFamilyCount] = {
        {0xd0,
         "??0MultiPassNoTextureMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@"
         "PAVAlphaBlendCache@1@PAUMPLightRender@MultiPassRenderState@1@3@Z",
         false},
        {0xdc,
         "??0MultiPassOneTextureMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@"
         "PAVAlphaBlendCache@1@PAUMPLightRender@MultiPassRenderState@1@3PAVVertexType@1@@Z",
         true},
        {0xe0,
         "??0MultiPassBumpMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@"
         "PAVAlphaBlendCache@1@PAUMPLightRender@MultiPassRenderState@1@3PAVVertexType@1@@Z",
         true},
        {0xe0,
         "??0MultiPassBumpSpecMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@"
         "PAVAlphaBlendCache@1@PAUMPLightRender@MultiPassRenderState@1@3PAVVertexType@1@@Z",
         true},
        {0xe0,
         "??0MultiPassBump2Material@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@"
         "PAVAlphaBlendCache@1@PAUMPLightRender@MultiPassRenderState@1@3PAVVertexType@1@@Z",
         true},
        {0xe0,
         "??0MultiPassBumpSpec2Material@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@"
         "PAVAlphaBlendCache@1@PAUMPLightRender@MultiPassRenderState@1@3PAVVertexType@1@@Z",
         true},
    };
    return families[id];
}

void* NewMPLightRender()
{
    unsigned char* const self = static_cast<unsigned char*>(Alloc::New(0x50));
    if (!self) return nullptr;
    *reinterpret_cast<void**>(self) = Original("??_7MPLightRender@MultiPassRenderState@Jet@@6B@");
    *reinterpret_cast<int*>(self + 4) = 1;
    std::memset(self + 8, 0, 0x48);
    return self;
}

void* NewMultiPassLightRender(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e)
{
    unsigned char* const self = static_cast<unsigned char*>(Alloc::New(0x38));
    if (!self) return nullptr;
    *reinterpret_cast<void**>(self) = Original("??_7MultiPassLightRender@Jet@@6B@");
    *reinterpret_cast<int*>(self + 4) = 1;
    std::memset(self + 8, 0, 0x18);
    *reinterpret_cast<unsigned int*>(self + 0x20) = a;
    *reinterpret_cast<unsigned int*>(self + 0x24) = c;
    *reinterpret_cast<unsigned int*>(self + 0x28) = d;
    *reinterpret_cast<unsigned int*>(self + 0x2c) = b;
    *reinterpret_cast<unsigned int*>(self + 0x30) = e;
    *reinterpret_cast<unsigned int*>(self + 0x34) = a + b + c + d + e;
    return self;
}

void AddVPProperty(void* lightRender, unsigned int property, unsigned int value, unsigned int which)
{
    unsigned char* const self = static_cast<unsigned char*>(lightRender);
    const unsigned int passes = *reinterpret_cast<unsigned int*>(self + 8);
    unsigned char* const pass =
        static_cast<unsigned char*>(*reinterpret_cast<void**>(self + 0x10)) + (passes - 1) * 0x104;
    Array<PropertyPair, 5>* const array =
        reinterpret_cast<Array<PropertyPair, 5>*>(pass + 0x0c + which * 0x0c);
    array->Reserve(array->count + 1);
    array->data[array->count].property = property;
    array->data[array->count].value = value;
    ++array->count;
}

void AddTCProperty(void* lightRender, unsigned int property, unsigned int value, unsigned int which)
{
    unsigned char* const self = static_cast<unsigned char*>(lightRender);
    const unsigned int passes = *reinterpret_cast<unsigned int*>(self + 8);
    unsigned char* const pass =
        static_cast<unsigned char*>(*reinterpret_cast<void**>(self + 0x10)) + (passes - 1) * 0x104;
    Array<PropertyPair, 5>* const array =
        reinterpret_cast<Array<PropertyPair, 5>*>(pass + 0x84 + which * 0x0c);
    array->Reserve(array->count + 1);
    array->data[array->count].property = property;
    array->data[array->count].value = value;
    ++array->count;
}

void AppendLightRenderItem(void* arrayAddress, void* render) { AppendMPLightRender(arrayAddress, render); }

void* VtableCall(void* object, unsigned int slot)
{
    using Fn = void* (__cdecl*)(void*);
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[slot / 4])(object);
}

// ---------------------------------------------------------------------------------------------------------------
// The exported entries.

void* __cdecl PluginCreateNoTexture(void* self, void* manager, void* kernel)
{
    return CreateInstance(kNoTextureFamily, self, manager, kernel);
}
void* __cdecl PluginCreateOneTexture(void* self, void* manager, void* kernel)
{
    return CreateInstance(kOneTextureFamily, self, manager, kernel);
}
void* __cdecl PluginCreateBump(void* self, void* manager, void* kernel)
{
    return CreateInstance(kBumpFamily, self, manager, kernel);
}
void* __cdecl PluginCreateBump2(void* self, void* manager, void* kernel)
{
    return CreateInstance(kBump2Family, self, manager, kernel);
}
void* __cdecl PluginCreateBumpSpec(void* self, void* manager, void* kernel)
{
    return CreateInstance(kBumpSpecFamily, self, manager, kernel);
}
void* __cdecl PluginCreateBumpSpec2(void* self, void* manager, void* kernel)
{
    return CreateInstance(kBumpSpec2Family, self, manager, kernel);
}

bool __cdecl PluginInitializeCaches(void* self, void* kernel)
{
    if (!kernel) return false;
    void* const manager = *reinterpret_cast<void* const*>(Bytes(kernel) + 0x6c);
    unsigned char* const plugin = Bytes(self);
    if (!EnsureCache(manager, "?AlphaBlendCacheID@AlphaBlendCache@Jet@@2VPString@2@A", 0x80,
                     "??0AlphaBlendCache@Jet@@QAE@XZ", reinterpret_cast<void**>(plugin + kPluginBlendCache)))
        return false;
    if (!EnsureCache(manager, "?MaterialChunkCacheID@MaterialChunkCache@Jet@@2VPString@2@A", 0xd2c,
                     "??0MaterialChunkCache@Jet@@QAE@XZ", reinterpret_cast<void**>(plugin + kPluginChunkCache)))
        return false;
    return true;
}

bool __cdecl PluginInitializeLightRenders(void* self, void* kernel)
{
    unsigned char* const plugin = Bytes(self);
    if (*reinterpret_cast<void**>(plugin + kPluginLightRender0) &&
        *reinterpret_cast<void**>(plugin + kPluginLightRender1))
        return true;

    using InitBoolFn = bool(__cdecl*)(void*, void*);
    using InitVoidFn = void(__cdecl*)(void*, void*);
    void** const vtable = *reinterpret_cast<void***>(self);
    reinterpret_cast<InitVoidFn>(vtable[0x24 / 4])(self, kernel);

    *reinterpret_cast<void**>(plugin + kPluginLightRender0) = NewMPLightRender();
    *reinterpret_cast<void**>(plugin + kPluginLightRender1) = NewMPLightRender();

    static const unsigned int kSlots[6] = {0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c};
    bool ok = true;
    for (unsigned int i = 0; i < 6 && ok; ++i)
        ok = reinterpret_cast<InitBoolFn>(vtable[kSlots[i] / 4])(self, kernel);
    if (ok) return true;

    ReleaseVBaseRef(*reinterpret_cast<void**>(plugin + kPluginLightRender0), 4);
    ReleaseVBaseRef(*reinterpret_cast<void**>(plugin + kPluginLightRender1), 4);
    *reinterpret_cast<void**>(plugin + kPluginLightRender0) = nullptr;
    *reinterpret_cast<void**>(plugin + kPluginLightRender1) = nullptr;
    return false;
}

void __cdecl PluginInitializeFixedVertexType(void* self, void* kernel)
{
    DelegateFixedVertexType(self, kernel,
                            "?InitializeFixedVertexType@MultiPassBumpMaterialPlugin@Jet@@MBAXPAVRenderKernel@2@@Z");
}

bool __cdecl PluginInitAmbientNoTexture(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kNoTexAmbient);
}
bool __cdecl PluginInitAmbient(void* self, void* kernel) { return InitializeLightType(self, kernel, kAmbientSpec); }
bool __cdecl PluginInitParallelNoTexture(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kNoTexParallel);
}
bool __cdecl PluginInitParallelOneTexture(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kOneTexParallel);
}
bool __cdecl PluginInitParallelBump(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kParallelSpec);
}
bool __cdecl PluginInitParallelBumpSpec(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kBumpSpecParallel);
}
bool __cdecl PluginInitPointNoTexture(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kNoTexPoint);
}
bool __cdecl PluginInitPointOneTexture(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kPointSpec);
}
bool __cdecl PluginInitPointBump(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kPointSpec);
}
bool __cdecl PluginInitPointBumpSpec(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kBumpSpecPoint);
}
bool __cdecl PluginInitSpotNoTexture(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kNoTexSpot);
}
bool __cdecl PluginInitSpot(void* self, void* kernel) { return InitializeLightType(self, kernel, kSpotSpec); }
bool __cdecl PluginInitCubeNoTexture(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kNoTexCube);
}
bool __cdecl PluginInitCube(void* self, void* kernel) { return InitializeLightType(self, kernel, kCubeSpec); }
bool __cdecl PluginInitCubeSpotNoTexture(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kNoTexCubeSpot);
}
bool __cdecl PluginInitCubeSpot(void* self, void* kernel)
{
    return InitializeLightType(self, kernel, kCubeSpotSpec);
}

bool __cdecl PluginLoadLightPrograms(void* self, void* lightRender, void* kernel, const char* name,
                                     const char* combinerName)
{
    if (!name && !combinerName) {
        reinterpret_cast<Thunks*>(lightRender)->CreateNewLightPass(nullptr, nullptr);
        return true;
    }
    using Fn = bool(__cdecl*)(void*, void*, void*, const char*, const char*);
    Fn fn = reinterpret_cast<Fn>(
        Original("?LoadLightPrograms@MultiPassBumpMaterialPlugin@Jet@@MBA_NPAVMultiPassLightRender@2@"
                 "PAVRenderKernel@2@PBD2@Z"));
    if (fn) return fn(self, lightRender, kernel, name, combinerName);
    fn = reinterpret_cast<Fn>(
        Original("?LoadLightPrograms@MultiPassNoTextureMaterialPlugin@Jet@@IBA_NPAVMultiPassLightRender@2@"
                 "PAVRenderKernel@2@PBD2@Z"));
    if (fn) return fn(self, lightRender, kernel, name, combinerName);
    fn = reinterpret_cast<Fn>(
        Original("?LoadLightPrograms@MultiPassOneTextureMaterialPlugin@Jet@@IBA_NPAVMultiPassLightRender@2@"
                 "PAVRenderKernel@2@PBD2@Z"));
    return fn ? fn(self, lightRender, kernel, name, combinerName) : false;
}

void __cdecl PluginRegisterProgramsNoTexture(void* self)
{
    ReplayPrograms(self, kNotexturePrograms, kNotextureProgramCount, false);
}
void __cdecl PluginRegisterProgramsOneTexture(void* self)
{
    ReplayPrograms(self, kOnetexturePrograms, kOnetextureProgramCount, false);
}
void __cdecl PluginRegisterProgramsBump(void* self)
{
    ReplayPrograms(self, kBumpPrograms, kBumpProgramCount, true);
}
void __cdecl PluginRegisterProgramsBumpSpec(void* self)
{
    ReplayPrograms(self, kBumpspecPrograms, kBumpspecProgramCount, true);
}

}  // namespace Last
}  // namespace MultiPass
}  // namespace Jet
