// Unit U5b, wave 4: the multipass material, plugin and geometry-chunk lifetimes, transcribed from the TS12
// assembly.
//
// Every constructor and destructor here is a member of a local thunk class, so the thiscall entry ABI and the
// hidden virtual-base flag argument match the original exactly. The objects are built on the original class
// vtables (looked up in Render3DCore_7c_orig.dll) and on the RenderSystem_7c Material/MaterialPlugin base
// constructors, which are local in this tree; the reference-counted cache pointers use the same vbptr arithmetic
// the original does. Nothing here defines a Material-derived class, so U5a's header stays the only owner of the
// class bodies.
#include <jet/Render3DCoreMultiPass.h>

#include <jet/DynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/PString.h>

#include <cstring>

namespace Jet {
namespace MultiPass {

namespace {

using MaterialCtorFn = void(__thiscall*)(void*, void*, bool);
using MaterialDtorFn = void(__thiscall*)(void*);
using MaterialInitFn = void(__cdecl*)(const void*, void*);
using MaterialEqualsFn = bool(__cdecl*)(const void*, const void*);
using MaterialCreateFn = void*(__cdecl*)(const void*, void*);
using SharedTextureFn = bool(__cdecl*)(int, void*, void*);

using PluginNamedCtorFn = void(__thiscall*)(void*, const void*, const void*);
using PluginCopyCtorFn = void(__thiscall*)(void*, const void*);
using PluginAssignFn = void*(__thiscall*)(void*, const void*);
using PluginDtorFn = void(__thiscall*)(void*);
using ReferenceCountDtorFn = void(__thiscall*)(void*);

using GeometryChunkCtorFn = void(__thiscall*)(void*, unsigned, unsigned, unsigned, unsigned, unsigned);
using GeometryChunkDtorFn = void(__thiscall*)(void*);
using GeometryChunkInitializeFn = bool(__thiscall*)(void*, void*, bool);

HMODULE RenderSystem()
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("RenderSystem_7c.dll");
        return m ? m : LoadLibraryA("RenderSystem_7c.dll");
    }();
    return module;
}

template <class F>
F Import(const char* name)
{
    return reinterpret_cast<F>(GetProcAddress(RenderSystem(), name));
}

MaterialCtorFn MaterialCtor() { static MaterialCtorFn f = Import<MaterialCtorFn>("??0Material@Jet@@QAE@PAVMaterialManager@1@@Z"); return f; }
MaterialDtorFn MaterialDtor() { static MaterialDtorFn f = Import<MaterialDtorFn>("??1Material@Jet@@MAE@XZ"); return f; }
MaterialInitFn MaterialInit() { static MaterialInitFn f = Import<MaterialInitFn>("?InitInstance@Material@Jet@@IBAXAAV12@@Z"); return f; }
MaterialEqualsFn MaterialEquals() { static MaterialEqualsFn f = Import<MaterialEqualsFn>("?Equals@Material@Jet@@IBA_NABV12@@Z"); return f; }
MaterialCreateFn MaterialCreate() { static MaterialCreateFn f = Import<MaterialCreateFn>("?CreateInstance@Material@Jet@@SAPAV12@ABVPString@2@PAVMaterialManager@2@@Z"); return f; }
SharedTextureFn SharedTexture() { static SharedTextureFn f = Import<SharedTextureFn>("?GetTexture@SharedTextureCollection@Jet@@SA_NW4SharedTextureID@2@PAVTextureManager@2@AAV?$AutoReference@VTexture@Jet@@@2@@Z"); return f; }

PluginNamedCtorFn PluginNamedCtor() { static PluginNamedCtorFn f = Import<PluginNamedCtorFn>("??0MaterialPlugin@Jet@@QAE@ABVPString@1@ABVUId@1@@Z"); return f; }
PluginCopyCtorFn PluginCopyCtor() { static PluginCopyCtorFn f = Import<PluginCopyCtorFn>("??0MaterialPlugin@Jet@@QAE@ABV01@@Z"); return f; }
PluginAssignFn PluginAssign() { static PluginAssignFn f = Import<PluginAssignFn>("??4MaterialPlugin@Jet@@QAEAAV01@ABV01@@Z"); return f; }
PluginDtorFn PluginDtor() { static PluginDtorFn f = Import<PluginDtorFn>("??1MaterialPlugin@Jet@@UAE@XZ"); return f; }

GeometryChunkCtorFn ChunkCtorPNTA()
{
    static GeometryChunkCtorFn f = reinterpret_cast<GeometryChunkCtorFn>(OriginalExport(
        "??0?$GeometryChunkTemplate@UPackedVertexPNTA@Jet@@VGeometryChunkSingleStream@2@@Jet@@QAE@IIIII@Z"));
    return f;
}
GeometryChunkCtorFn ChunkCtorPNTTA()
{
    static GeometryChunkCtorFn f = reinterpret_cast<GeometryChunkCtorFn>(OriginalExport(
        "??0?$GeometryChunkTemplate@UPackedVertexPNTTA@Jet@@VGeometryChunkSingleStream@2@@Jet@@QAE@IIIII@Z"));
    return f;
}
GeometryChunkDtorFn ChunkStreamDtor()
{
    static GeometryChunkDtorFn f = Import<GeometryChunkDtorFn>("??1GeometryChunkSingleStream@Jet@@MAE@XZ");
    return f;
}
GeometryChunkInitializeFn ChunkInitializePNTA()
{
    static GeometryChunkInitializeFn f = reinterpret_cast<GeometryChunkInitializeFn>(OriginalExport(
        "?InitializeBuffers@?$GeometryChunkTemplate@UPackedVertexPNTA@Jet@@VGeometryChunkSingleStream@2@@Jet@@UAA_NPAVDisplay@2@_N@Z"));
    return f;
}

// KernelStandard_7c's ReferenceCount destructor writes the virtual base's vtable in its prologue; the vbase
// destructors of the material classes end with that call.
ReferenceCountDtorFn ReferenceCountDtor()
{
    static ReferenceCountDtorFn f = [] {
        HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
        if (!ks) ks = LoadLibraryA("KernelStandard_7c.dll");
        return ks ? reinterpret_cast<ReferenceCountDtorFn>(GetProcAddress(ks, "??1ReferenceCount@Jet@@MAE@XZ"))
                  : nullptr;
    }();
    return f;
}

void* Orig(const char* name) { return OriginalExport(name); }

// ---- reference-count helpers ----------------------------------------------------------------------------------

void AddDirect(void* object)
{
    if (object) reinterpret_cast<const ReferenceCount*>(object)->AddReference();
}
void AddDirectUnchecked(void* object)
{
    reinterpret_cast<const ReferenceCount*>(object)->AddReference();
}
void ReleaseDirect(void* object)
{
    if (object) reinterpret_cast<const ReferenceCount*>(object)->ReleaseReference();
}
void ReleaseDirectUnchecked(void* object)
{
    reinterpret_cast<const ReferenceCount*>(object)->ReleaseReference();
}

// A rendering cache derives from ReferenceCount through a virtual base: cache+0x0c is the vbptr and
// vbptr[1] is the offset of the ReferenceCount subobject from cache+0x0c (the original's inline arithmetic).
void AddCache(void* cache)
{
    if (!cache) return;
    void* const vbptr = *reinterpret_cast<void**>(static_cast<unsigned char*>(cache) + 0x0c);
    const int delta = *reinterpret_cast<int*>(static_cast<unsigned char*>(vbptr) + 4);
    AddDirect(static_cast<unsigned char*>(cache) + 0x0c + delta);
}
void ReleaseCache(void* cache)
{
    if (!cache) return;
    void* const vbptr = *reinterpret_cast<void**>(static_cast<unsigned char*>(cache) + 0x0c);
    const int delta = *reinterpret_cast<int*>(static_cast<unsigned char*>(vbptr) + 4);
    ReleaseDirect(static_cast<unsigned char*>(cache) + 0x0c + delta);
}

void* ManagerTextures(void* manager)
{
    void* const kernel = *reinterpret_cast<void**>(static_cast<unsigned char*>(manager) + 8);
    return *reinterpret_cast<void**>(static_cast<unsigned char*>(kernel) + 0x94);
}

bool FetchTexture(int id, void* manager, void* out)
{
    return SharedTexture()(id, ManagerTextures(manager), out);
}

// The material tail array is Array<const Texture*, 10> at +0xb8; the growth step only matters to Reserve.
void ResetTextureArray(void* self)
{
    *reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(self) + MaterialLayout::textureArray) = 0;
}
void AppendTexture(void* self, void* texture)
{
    Array<const void*, 10>* const array =
        reinterpret_cast<Array<const void*, 10>*>(static_cast<unsigned char*>(self) + MaterialLayout::textureArray);
    array->Reserve(array->count + 1);
    array->data[array->count] = texture;
    ++array->count;
}

// ---- material constructor/destructor preludes -----------------------------------------------------------------

int VbDelta(void* self)
{
    const int* const table = *reinterpret_cast<const int* const*>(static_cast<unsigned char*>(self) + 4);
    return table[1];
}

void Begin(void* self, bool flag, const char* vbtable)
{
    if (!flag) return;
    void* const table = Orig(vbtable);
    *reinterpret_cast<void**>(static_cast<unsigned char*>(self) + 4) = table;
    const int delta = *reinterpret_cast<const int*>(static_cast<const unsigned char*>(table) + 4);
    // the virtual ReferenceCount base's count; the pre-constructor vftable of the original is overwritten by
    // the Material constructor and then by the class vftable, so only the count is observable.
    *reinterpret_cast<int*>(static_cast<unsigned char*>(self) + delta + 8) = 1;
}

void Finish(void* self, const char* materialTable, const char* instanceTable)
{
    const int delta = VbDelta(self);
    *reinterpret_cast<void**>(self) = Orig(materialTable);
    *reinterpret_cast<void**>(static_cast<unsigned char*>(self) + delta + 4) = Orig(instanceTable);
    *reinterpret_cast<int*>(static_cast<unsigned char*>(self) + delta) = 0;
}

// ---- the material vtable / vbtable names ----------------------------------------------------------------------

struct MaterialTables {
    const char* materialVtable;
    const char* instanceVtable;
    const char* vbtable;
    unsigned int size;
    unsigned int vbaseOffset;
};

// Object sizes and virtual-base offsets read from the assembly (the vbtable's [1] value).
const MaterialTables kNoTexture = {
    "??_7MultiPassNoTextureMaterial@Jet@@6BMaterial@1@@",
    "??_7MultiPassNoTextureMaterial@Jet@@6BPluginInstance@1@@",
    "??_8MultiPassNoTextureMaterial@Jet@@7B@", 0xd0, 0xc4};
const MaterialTables kOneTexture = {
    "??_7MultiPassOneTextureMaterial@Jet@@6BMaterial@1@@",
    "??_7MultiPassOneTextureMaterial@Jet@@6BPluginInstance@1@@",
    "??_8MultiPassOneTextureMaterial@Jet@@7B@", 0xdc, 0xd0};
const MaterialTables kBump = {
    "??_7MultiPassBumpMaterial@Jet@@6BMaterial@1@@",
    "??_7MultiPassBumpMaterial@Jet@@6BPluginInstance@1@@",
    "??_8MultiPassBumpMaterial@Jet@@7B@", 0xe0, 0xd4};
const MaterialTables kBump2 = {
    "??_7MultiPassBump2Material@Jet@@6BMaterial@1@@",
    "??_7MultiPassBump2Material@Jet@@6BPluginInstance@1@@",
    "??_8MultiPassBump2Material@Jet@@7B@", 0xe0, 0xd4};
const MaterialTables kBumpSpec = {
    "??_7MultiPassBumpSpecMaterial@Jet@@6BMaterial@1@@",
    "??_7MultiPassBumpSpecMaterial@Jet@@6BPluginInstance@1@@",
    "??_8MultiPassBumpSpecMaterial@Jet@@7B@", 0xe0, 0xd4};
const MaterialTables kBumpSpec2 = {
    "??_7MultiPassBumpSpec2Material@Jet@@6BMaterial@1@@",
    "??_7MultiPassBumpSpec2Material@Jet@@6BPluginInstance@1@@",
    "??_8MultiPassBumpSpec2Material@Jet@@7B@", 0xe0, 0xd4};

// the per-class type/uid objects, always the renamed original's (data exports stay original-owned)
const char* const kTypeNoTexture = "?UNDECORATED_TYPE@MultiPassNoTextureMaterial@Jet@@2VPString@2@B";
const char* const kUidNoTexture = "?UID@MultiPassNoTextureMaterial@Jet@@2VUId@2@B";
const char* const kTypeOneTexture = "?UNDECORATED_TYPE@MultiPassOneTextureMaterial@Jet@@2VPString@2@B";
const char* const kUidOneTexture = "?UID@MultiPassOneTextureMaterial@Jet@@2VUId@2@B";
const char* const kTypeBump = "?UNDECORATED_TYPE@MultiPassBumpMaterial@Jet@@2VPString@2@B";
const char* const kUidBump = "?UID@MultiPassBumpMaterial@Jet@@2VUId@2@B";
const char* const kTypeBump2 = "?UNDECORATED_TYPE@MultiPassBump2Material@Jet@@2VPString@2@B";
const char* const kUidBump2 = "?UID@MultiPassBump2Material@Jet@@2VUId@2@B";
const char* const kTypeBumpSpec = "?UNDECORATED_TYPE@MultiPassBumpSpecMaterial@Jet@@2VPString@2@B";
const char* const kUidBumpSpec = "?UID@MultiPassBumpSpecMaterial@Jet@@2VUId@2@B";
const char* const kTypeBumpSpec2 = "?UNDECORATED_TYPE@MultiPassBumpSpec2Material@Jet@@2VPString@2@B";
const char* const kUidBumpSpec2 = "?UID@MultiPassBumpSpec2Material@Jet@@2VUId@2@B";

// ---- common material bodies -----------------------------------------------------------------------------------

void* NoTextureBody(void* self, bool flag, void* manager, void* chunk, void* blend, void* light0, void* light1)
{
    Begin(self, flag, kNoTexture.vbtable);
    MaterialCtor()(self, manager, false);
    Finish(self, kNoTexture.materialVtable, kNoTexture.instanceVtable);

    unsigned char* const p = static_cast<unsigned char*>(self);
    *reinterpret_cast<void**>(p + 0x9c) = chunk;
    *reinterpret_cast<void**>(p + 0xa0) = blend;
    *reinterpret_cast<void**>(p + 0xa4) = nullptr;  // texture A
    *reinterpret_cast<void**>(p + 0xa8) = nullptr;  // texture B
    p[0xac] = p[0xad] = p[0xae] = p[0xaf] = 0;
    *reinterpret_cast<void**>(p + 0xb0) = light0;
    *reinterpret_cast<void**>(p + 0xb4) = light1;
    *reinterpret_cast<unsigned int*>(p + 0xb8) = 0;
    *reinterpret_cast<unsigned int*>(p + 0xbc) = 0;
    *reinterpret_cast<void**>(p + 0xc0) = nullptr;

    AddCache(chunk);
    AddCache(blend);
    AddDirectUnchecked(light0);
    AddDirectUnchecked(light1);

    if (!FetchTexture(5, manager, p + 0xa4)) FetchTexture(0, manager, p + 0xa4);
    if (!FetchTexture(4, manager, p + 0xa8)) FetchTexture(0, manager, p + 0xa8);

    ResetTextureArray(self);
    AppendTexture(self, *reinterpret_cast<void**>(p + 0xa4));
    AppendTexture(self, *reinterpret_cast<void**>(p + 0xa8));
    return self;
}

void* OneTextureBody(void* self, bool flag, void* manager, void* chunk, void* blend, void* light0, void* light1,
                     void* vertexType)
{
    Begin(self, flag, kOneTexture.vbtable);
    NoTextureBody(self, false, manager, chunk, blend, light0, light1);
    Finish(self, kOneTexture.materialVtable, kOneTexture.instanceVtable);

    unsigned char* const p = static_cast<unsigned char*>(self);
    *reinterpret_cast<void**>(p + 0xc4) = nullptr;   // primary texture
    *reinterpret_cast<int*>(p + 0xc8) = -1;          // alpha hint
    *reinterpret_cast<void**>(p + 0xcc) = vertexType;

    FetchTexture(0, manager, p + 0xc4);
    AddDirect(vertexType);

    ResetTextureArray(self);
    AppendTexture(self, *reinterpret_cast<void**>(p + 0xc4));
    AppendTexture(self, *reinterpret_cast<void**>(p + 0xa4));
    AppendTexture(self, *reinterpret_cast<void**>(p + 0xa8));
    return self;
}

void* BumpBody(void* self, bool flag, void* manager, void* chunk, void* blend, void* light0, void* light1,
               void* vertexType)
{
    Begin(self, flag, kBump.vbtable);
    OneTextureBody(self, false, manager, chunk, blend, light0, light1, vertexType);
    Finish(self, kBump.materialVtable, kBump.instanceVtable);

    unsigned char* const p = static_cast<unsigned char*>(self);
    *reinterpret_cast<void**>(p + 0xd0) = nullptr;   // bump texture
    FetchTexture(1, manager, p + 0xd0);

    ResetTextureArray(self);
    AppendTexture(self, *reinterpret_cast<void**>(p + 0xc4));
    AppendTexture(self, *reinterpret_cast<void**>(p + 0xd0));
    AppendTexture(self, *reinterpret_cast<void**>(p + 0xa4));
    AppendTexture(self, *reinterpret_cast<void**>(p + 0xa8));
    return self;
}

// ---- dtors: the exported virtual destructor receives the virtual-base subobject (object + 0xc8/0xd4/0xd8) ----

void NoTextureDtorBody(unsigned char* self)
{
    unsigned char* const obj = self - 0xc8;
    const int delta = VbDelta(obj);
    *reinterpret_cast<void**>(obj) = Orig(kNoTexture.materialVtable);
    *reinterpret_cast<void**>(obj + delta + 4) = Orig(kNoTexture.instanceVtable);
    *reinterpret_cast<int*>(obj + delta) = 0;

    ReleaseCache(*reinterpret_cast<void**>(obj + 0x9c));
    ReleaseCache(*reinterpret_cast<void**>(obj + 0xa0));
    ReleaseDirectUnchecked(*reinterpret_cast<void**>(obj + 0xb0));
    ReleaseDirectUnchecked(*reinterpret_cast<void**>(obj + 0xb4));

    *reinterpret_cast<unsigned int*>(obj + 0xb8) = 0;
    if (void* const data = *reinterpret_cast<void**>(obj + 0xc0)) Alloc::Delete(data);
    ReleaseDirect(*reinterpret_cast<void**>(obj + 0xa8));
    ReleaseDirect(*reinterpret_cast<void**>(obj + 0xa4));

    MaterialDtor()(obj + 0xa0);
}

void OneTextureDtorBody(unsigned char* self)
{
    unsigned char* const obj = self - 0xd4;
    const int delta = VbDelta(obj);
    *reinterpret_cast<void**>(obj) = Orig(kOneTexture.materialVtable);
    *reinterpret_cast<void**>(obj + delta + 4) = Orig(kOneTexture.instanceVtable);
    *reinterpret_cast<int*>(obj + delta) = 0;

    ReleaseDirect(*reinterpret_cast<void**>(obj + 0xcc));
    ReleaseDirect(*reinterpret_cast<void**>(obj + 0xc4));
    NoTextureDtorBody(obj + 0xc8);
}

void BumpDtorBody(unsigned char* self)
{
    unsigned char* const obj = self - 0xd8;
    const int delta = VbDelta(obj);
    *reinterpret_cast<void**>(obj) = Orig(kBump.materialVtable);
    *reinterpret_cast<void**>(obj + delta + 4) = Orig(kBump.instanceVtable);
    *reinterpret_cast<int*>(obj + delta) = 0;

    ReleaseDirect(*reinterpret_cast<void**>(obj + 0xd0));
    OneTextureDtorBody(obj + 0xd4);
}

}  // namespace

// ----------------------------------------------------------------------------------------------------------------
// the exported thiscall entry points

class LifeThunks {
public:
    // MultiPass plugins
    void* __thiscall BumpPluginCopy(const void* source);
    void* __thiscall BumpPluginDefault();
    void* __thiscall BumpPluginNamed(const void* name, const void* id, const char* a1, const char* a2,
                                     const char* a3, const char* a4, const char* a5, const char* a6,
                                     const char* a7, const char* a8, const char* a9, const char* a10,
                                     const char* a11, const char* a12, const char* a13, const char* a14,
                                     const char* a15);
    void __thiscall BumpPluginDtor();
    void* __thiscall BumpPluginAssign(const void* source);

    void* __thiscall Bump2PluginCopy(const void* source);
    void* __thiscall Bump2PluginDefault();
    void __thiscall Bump2PluginDtor();

    void* __thiscall BumpSpecPluginCopy(const void* source);
    void* __thiscall BumpSpecPluginDefault();
    void* __thiscall BumpSpecPluginNamed(const void* name, const void* id, const char* a1, const char* a2,
                                         const char* a3, const char* a4, const char* a5, const char* a6,
                                         const char* a7, const char* a8, const char* a9, const char* a10,
                                         const char* a11, const char* a12, const char* a13, const char* a14,
                                         const char* a15);
    void __thiscall BumpSpecPluginDtor();
    void* __thiscall BumpSpecPluginAssign(const void* source);

    void* __thiscall BumpSpec2PluginCopy(const void* source);
    void* __thiscall BumpSpec2PluginDefault();
    void __thiscall BumpSpec2PluginDtor();
    void* __thiscall BumpSpec2PluginAssign(const void* source);

    void* __thiscall NoTexturePluginCopy(const void* source);
    void* __thiscall NoTexturePluginDefault();
    void __thiscall NoTexturePluginDtor();
    void* __thiscall NoTexturePluginAssign(const void* source);

    void* __thiscall OneTexturePluginCopy(const void* source);
    void* __thiscall OneTexturePluginDefault();
    void* __thiscall OneTexturePluginNamed(const void* name, const void* id, const char* a1, const char* a2,
                                           const char* a3, const char* a4, const char* a5, const char* a6,
                                           const char* a7, const char* a8, const char* a9, const char* a10,
                                           const char* a11, const char* a12, const char* a13, const char* a14,
                                           const char* a15);
    void __thiscall OneTexturePluginDtor();
    void* __thiscall OneTexturePluginAssign(const void* source);

    // MultiPass materials
    void* __thiscall NoTextureCtor(void* manager, void* chunk, void* blend, void* light0, void* light1, bool flag);
    void* __thiscall OneTextureCtor(void* manager, void* chunk, void* blend, void* light0, void* light1,
                                    void* vertexType, bool flag);
    void* __thiscall BumpCtor(void* manager, void* chunk, void* blend, void* light0, void* light1, void* vertexType,
                              bool flag);
    void* __thiscall Bump2Ctor(void* manager, void* chunk, void* blend, void* light0, void* light1, void* vertexType,
                               bool flag);
    void* __thiscall BumpSpecCtor(void* manager, void* chunk, void* blend, void* light0, void* light1,
                                  void* vertexType, bool flag);
    void* __thiscall BumpSpec2Ctor(void* manager, void* chunk, void* blend, void* light0, void* light1,
                                   void* vertexType, bool flag);

    void __thiscall NoTextureDtor();
    void __thiscall OneTextureDtor();
    void __thiscall BumpDtor();
    void __thiscall Bump2Dtor() { BumpDtor(); }

    void __thiscall NoTextureVbaseDtor();
    void __thiscall OneTextureVbaseDtor();
    void __thiscall BumpVbaseDtor();

    // geometry chunks
    void* __thiscall BumpChunkTCtor(unsigned, unsigned, unsigned, unsigned, unsigned);
    void* __thiscall BumpChunkT2Ctor(unsigned, unsigned, unsigned, unsigned, unsigned);
    void __thiscall BumpChunkTDtor();
    void __thiscall BumpChunkT2Dtor();
    bool __thiscall BumpChunkTInitialize(void* display, bool load);
};

// ---- plugins --------------------------------------------------------------------------------------------------

namespace {

void FillNames(unsigned char* p, const char* const* names)
{
    for (unsigned i = 0; i < 15; ++i) *reinterpret_cast<const char**>(p + 0x34 + i * 4) = names[i];
}

void ReleasePluginCaches(void* self)
{
    unsigned char* const p = static_cast<unsigned char*>(self);
    ReleaseCache(*reinterpret_cast<void**>(p + 0x20));
    ReleaseCache(*reinterpret_cast<void**>(p + 0x24));
    ReleaseDirect(*reinterpret_cast<void**>(p + 0x28));
    ReleaseDirect(*reinterpret_cast<void**>(p + 0x2c));
    ReleaseDirect(*reinterpret_cast<void**>(p + 0x30));
}

void PluginDefaultBase(void* self, const char* type, const char* uid, const char* vtable,
                       const char* const* names, unsigned caches)
{
    PluginNamedCtor()(self, Orig(type), Orig(uid));
    unsigned char* const p = static_cast<unsigned char*>(self);
    for (unsigned i = 0; i < caches; ++i) *reinterpret_cast<void**>(p + 0x20 + i * 4) = nullptr;
    *reinterpret_cast<void**>(self) = Orig(vtable);
    if (names) FillNames(p, names);
}

void PluginNamedBody(void* self, const void* name, const void* id, const char* vtable, unsigned caches,
                     const char* const* defaults, const char* const* args)
{
    PluginNamedCtor()(self, name, id);
    unsigned char* const p = static_cast<unsigned char*>(self);
    for (unsigned i = 0; i < caches; ++i) *reinterpret_cast<void**>(p + 0x20 + i * 4) = nullptr;
    *reinterpret_cast<void**>(self) = Orig(vtable);
    for (unsigned i = 0; i < 15; ++i)
        *reinterpret_cast<const char**>(p + 0x34 + i * 4) = args[i] ? args[i] : defaults[i];
}

void PluginCopyBody(void* self, const void* source, const char* vtable, unsigned last)
{
    PluginCopyCtor()(self, source);
    *reinterpret_cast<void**>(self) = Orig(vtable);
    for (unsigned offset = 0x20; offset <= last; offset += 4)
        *reinterpret_cast<void**>(static_cast<unsigned char*>(self) + offset) =
            *reinterpret_cast<void* const*>(static_cast<const unsigned char*>(source) + offset);
}

void PluginAssignBody(void* self, const void* source, unsigned last)
{
    PluginAssign()(self, source);
    for (unsigned offset = 0x20; offset <= last; offset += 4)
        *reinterpret_cast<void**>(static_cast<unsigned char*>(self) + offset) =
            *reinterpret_cast<void* const*>(static_cast<const unsigned char*>(source) + offset);
}

const char* const kPluginBumpVtable = "??_7MultiPassBumpMaterialPlugin@Jet@@6B@";
const char* const kPluginBump2Vtable = "??_7MultiPassBump2MaterialPlugin@Jet@@6B@";
const char* const kPluginBumpSpecVtable = "??_7MultiPassBumpSpecMaterialPlugin@Jet@@6B@";
const char* const kPluginBumpSpec2Vtable = "??_7MultiPassBumpSpec2MaterialPlugin@Jet@@6B@";
const char* const kPluginNoTextureVtable = "??_7MultiPassNoTextureMaterialPlugin@Jet@@6B@";
const char* const kPluginOneTextureVtable = "??_7MultiPassOneTextureMaterialPlugin@Jet@@6B@";

const char* const kBumpNames[15] = {
    "bumpTex_AmbientLight", "bumpTex_ParallelLight", "bumpTex_PointLight", "bumpTex_SpotLight",
    "bumpTex_CubeMapLight", "MPBumpTexMat_AmbientLight", "MPBumpTexMat_AmbientLightx2",
    "MPBumpTexMat_ParallelLight", "MPBumpTexMat_ParallelLightx2", "MPBumpTexMat_PointLight",
    "MPBumpTexMat_PointLightx2", "MPBumpTexMat_SpotLight", "MPBumpTexMat_SpotLightx2",
    "MPBumpTexMat_CubeMapLight", "MPBumpTexMat_CubeMapLightx2"};
const char* const kBump2Names[15] = {
    "bumpTex_AmbientLight2T", "bumpTex_ParallelLight2T", "bumpTex_PointLight2T", "bumpTex_SpotLight2T",
    "bumpTex_CubeMapLight2T", "MPBumpTexMat_AmbientLight", "MPBumpTexMat_AmbientLightx2",
    "MPBumpTexMat_ParallelLight", "MPBumpTexMat_ParallelLightx2", "MPBumpTexMat_PointLight",
    "MPBumpTexMat_PointLightx2", "MPBumpTexMat_SpotLight", "MPBumpTexMat_SpotLightx2",
    "MPBumpTexMat_CubeMapLight", "MPBumpTexMat_CubeMapLightx2"};
const char* const kBumpSpecNames[15] = {
    "bumpSpec_AmbientLight", "bumpSpec_ParallelLight", "bumpSpecPV_PointLight", "bumpSpec_SpotLight",
    "bumpSpec_CubeMapLight", "MPBumpSpecMat_AmbientLight", "MPBumpSpecMat_AmbientLightx2",
    "MPBumpSpecMat_ParallelLight", "MPBumpSpecMat_ParallelLightx2", "MPBumpSpecMat_PointLight",
    "MPBumpSpecMat_PointLightx2", "MPBumpSpecMat_SpotLight", "MPBumpSpecMat_SpotLightx2",
    "MPBumpSpecMat_CubeMapLight", "MPBumpSpecMat_CubeMapLightx2"};
const char* const kBumpSpec2Names[15] = {
    "bumpSpec_AmbientLight2T", "bumpSpec_ParallelLight2T", "bumpSpecPV_PointLight2T", "bumpSpec_SpotLight2T",
    "bumpSpec_CubeMapLight2T", "MPBumpSpecMat_AmbientLight", "MPBumpSpecMat_AmbientLightx2",
    "MPBumpSpecMat_ParallelLight", "MPBumpSpecMat_ParallelLightx2", "MPBumpSpecMat_PointLight",
    "MPBumpSpecMat_PointLightx2", "MPBumpSpecMat_SpotLight", "MPBumpSpecMat_SpotLightx2",
    "MPBumpSpecMat_CubeMapLight", "MPBumpSpecMat_CubeMapLightx2"};
const char* const kOneTextureNames[15] = {
    "oneTex_AmbientLight", "oneTex_ParallelLight", "oneTex_PointLight", "oneTex_SpotLight", "oneTex_CubeMapLight",
    "MPOneTexMat_AmbientLight", "MPOneTexMat_AmbientLightx2", "MPOneTexMat_AmbientLight",
    "MPOneTexMat_AmbientLightx2", "MPOneTexMat_PointLight", "MPOneTexMat_PointLightx2",
    "MPOneTexMat_SpotLight", "MPOneTexMat_SpotLightx2", "MPOneTexMat_CubeMapLight",
    "MPOneTexMat_CubeMapLightx2"};

}  // namespace

void* __thiscall LifeThunks::BumpPluginCopy(const void* source)
{
    PluginCopyBody(this, source, kPluginBumpVtable, 0x6c);
    return this;
}
void* __thiscall LifeThunks::BumpPluginDefault()
{
    PluginDefaultBase(this, kTypeBump, kUidBump, kPluginBumpVtable, kBumpNames, 5);
    return this;
}
void* __thiscall LifeThunks::BumpPluginNamed(const void* name, const void* id, const char* a1, const char* a2,
                                             const char* a3, const char* a4, const char* a5, const char* a6,
                                             const char* a7, const char* a8, const char* a9, const char* a10,
                                             const char* a11, const char* a12, const char* a13, const char* a14,
                                             const char* a15)
{
    const char* const args[15] = {a1,  a2,  a3,  a4,  a5,  a6,  a7,  a8,
                                  a9,  a10, a11, a12, a13, a14, a15};
    PluginNamedBody(this, name, id, kPluginBumpVtable, 5, kBumpNames, args);
    return this;
}
void __thiscall LifeThunks::BumpPluginDtor()
{
    *reinterpret_cast<void**>(this) = Orig(kPluginBumpVtable);
    ReleasePluginCaches(this);
    PluginDtor()(this);
}
void* __thiscall LifeThunks::BumpPluginAssign(const void* source)
{
    PluginAssignBody(this, source, 0x6c);
    return this;
}

void* __thiscall LifeThunks::Bump2PluginCopy(const void* source)
{
    BumpPluginCopy(source);
    *reinterpret_cast<void**>(this) = Orig(kPluginBump2Vtable);
    return this;
}
void* __thiscall LifeThunks::Bump2PluginDefault()
{
    PluginDefaultBase(this, kTypeBump2, kUidBump2, kPluginBump2Vtable, kBump2Names, 5);
    return this;
}
void __thiscall LifeThunks::Bump2PluginDtor()
{
    *reinterpret_cast<void**>(this) = Orig(kPluginBumpVtable);
    ReleasePluginCaches(this);
    PluginDtor()(this);
}

void* __thiscall LifeThunks::BumpSpecPluginCopy(const void* source)
{
    BumpPluginCopy(source);
    *reinterpret_cast<void**>(this) = Orig(kPluginBumpSpecVtable);
    return this;
}
void* __thiscall LifeThunks::BumpSpecPluginDefault()
{
    const char* const zeros[15] = {};
    PluginNamedBody(this, Orig(kTypeBumpSpec), Orig(kUidBumpSpec), kPluginBumpSpecVtable, 5, kBumpSpecNames, zeros);
    return this;
}
void* __thiscall LifeThunks::BumpSpecPluginNamed(const void* name, const void* id, const char* a1, const char* a2,
                                                 const char* a3, const char* a4, const char* a5, const char* a6,
                                                 const char* a7, const char* a8, const char* a9, const char* a10,
                                                 const char* a11, const char* a12, const char* a13, const char* a14,
                                                 const char* a15)
{
    const char* const args[15] = {a1,  a2,  a3,  a4,  a5,  a6,  a7,  a8,
                                  a9,  a10, a11, a12, a13, a14, a15};
    PluginNamedBody(this, name, id, kPluginBumpSpecVtable, 5, kBumpSpecNames, args);
    return this;
}
void __thiscall LifeThunks::BumpSpecPluginDtor()
{
    *reinterpret_cast<void**>(this) = Orig(kPluginBumpSpecVtable);
    BumpPluginDtor();
}
void* __thiscall LifeThunks::BumpSpecPluginAssign(const void* source)
{
    BumpPluginAssign(source);
    return this;
}

void* __thiscall LifeThunks::BumpSpec2PluginCopy(const void* source)
{
    BumpSpecPluginCopy(source);
    *reinterpret_cast<void**>(this) = Orig(kPluginBumpSpec2Vtable);
    return this;
}
void* __thiscall LifeThunks::BumpSpec2PluginDefault()
{
    const char* const zeros[15] = {};
    PluginNamedBody(this, Orig(kTypeBumpSpec2), Orig(kUidBumpSpec2), kPluginBumpSpec2Vtable, 5, kBumpSpec2Names,
                    zeros);
    return this;
}
void __thiscall LifeThunks::BumpSpec2PluginDtor()
{
    *reinterpret_cast<void**>(this) = Orig(kPluginBumpSpecVtable);
    BumpPluginDtor();
}
void* __thiscall LifeThunks::BumpSpec2PluginAssign(const void* source)
{
    BumpPluginAssign(source);
    return this;
}

void* __thiscall LifeThunks::NoTexturePluginCopy(const void* source)
{
    PluginCopyBody(this, source, kPluginNoTextureVtable, 0x2c);
    return this;
}
void* __thiscall LifeThunks::NoTexturePluginDefault()
{
    PluginDefaultBase(this, kTypeNoTexture, kUidNoTexture, kPluginNoTextureVtable, nullptr, 4);
    return this;
}
void __thiscall LifeThunks::NoTexturePluginDtor()
{
    *reinterpret_cast<void**>(this) = Orig(kPluginNoTextureVtable);
    unsigned char* const p = reinterpret_cast<unsigned char*>(this);
    ReleaseCache(*reinterpret_cast<void**>(p + 0x20));
    ReleaseCache(*reinterpret_cast<void**>(p + 0x24));
    ReleaseDirect(*reinterpret_cast<void**>(p + 0x28));
    ReleaseDirect(*reinterpret_cast<void**>(p + 0x2c));
    PluginDtor()(this);
}
void* __thiscall LifeThunks::NoTexturePluginAssign(const void* source)
{
    PluginAssignBody(this, source, 0x2c);
    return this;
}

void* __thiscall LifeThunks::OneTexturePluginCopy(const void* source)
{
    PluginCopyBody(this, source, kPluginOneTextureVtable, 0x6c);
    return this;
}
void* __thiscall LifeThunks::OneTexturePluginDefault()
{
    PluginDefaultBase(this, kTypeOneTexture, kUidOneTexture, kPluginOneTextureVtable, kOneTextureNames, 5);
    return this;
}
void* __thiscall LifeThunks::OneTexturePluginNamed(const void* name, const void* id, const char* a1,
                                                   const char* a2, const char* a3, const char* a4, const char* a5,
                                                   const char* a6, const char* a7, const char* a8, const char* a9,
                                                   const char* a10, const char* a11, const char* a12, const char* a13,
                                                   const char* a14, const char* a15)
{
    const char* const args[15] = {a1,  a2,  a3,  a4,  a5,  a6,  a7,  a8,
                                  a9,  a10, a11, a12, a13, a14, a15};
    PluginNamedBody(this, name, id, kPluginOneTextureVtable, 5, kOneTextureNames, args);
    return this;
}
void __thiscall LifeThunks::OneTexturePluginDtor()
{
    *reinterpret_cast<void**>(this) = Orig(kPluginOneTextureVtable);
    ReleasePluginCaches(this);
    PluginDtor()(this);
}
void* __thiscall LifeThunks::OneTexturePluginAssign(const void* source)
{
    PluginAssignBody(this, source, 0x6c);
    return this;
}

// ---- material ctors -------------------------------------------------------------------------------------------

void* __thiscall LifeThunks::NoTextureCtor(void* manager, void* chunk, void* blend, void* light0, void* light1,
                                           bool flag)
{
    return NoTextureBody(this, flag, manager, chunk, blend, light0, light1);
}
void* __thiscall LifeThunks::OneTextureCtor(void* manager, void* chunk, void* blend, void* light0, void* light1,
                                            void* vertexType, bool flag)
{
    return OneTextureBody(this, flag, manager, chunk, blend, light0, light1, vertexType);
}
void* __thiscall LifeThunks::BumpCtor(void* manager, void* chunk, void* blend, void* light0, void* light1,
                                      void* vertexType, bool flag)
{
    return BumpBody(this, flag, manager, chunk, blend, light0, light1, vertexType);
}
void* __thiscall LifeThunks::Bump2Ctor(void* manager, void* chunk, void* blend, void* light0, void* light1,
                                       void* vertexType, bool flag)
{
    Begin(this, flag, kBump2.vbtable);
    BumpBody(this, false, manager, chunk, blend, light0, light1, vertexType);
    Finish(this, kBump2.materialVtable, kBump2.instanceVtable);
    return this;
}
void* __thiscall LifeThunks::BumpSpecCtor(void* manager, void* chunk, void* blend, void* light0, void* light1,
                                          void* vertexType, bool flag)
{
    Begin(this, flag, kBumpSpec.vbtable);
    BumpBody(this, false, manager, chunk, blend, light0, light1, vertexType);
    Finish(this, kBumpSpec.materialVtable, kBumpSpec.instanceVtable);
    return this;
}
void* __thiscall LifeThunks::BumpSpec2Ctor(void* manager, void* chunk, void* blend, void* light0, void* light1,
                                           void* vertexType, bool flag)
{
    Begin(this, flag, kBumpSpec2.vbtable);
    BumpBody(this, false, manager, chunk, blend, light0, light1, vertexType);
    Finish(this, kBumpSpec2.materialVtable, kBumpSpec2.instanceVtable);
    return this;
}

void __thiscall LifeThunks::NoTextureDtor() { NoTextureDtorBody(reinterpret_cast<unsigned char*>(this)); }
void __thiscall LifeThunks::OneTextureDtor() { OneTextureDtorBody(reinterpret_cast<unsigned char*>(this)); }
void __thiscall LifeThunks::BumpDtor() { BumpDtorBody(reinterpret_cast<unsigned char*>(this)); }

void __thiscall LifeThunks::NoTextureVbaseDtor()
{
    unsigned char* const base = reinterpret_cast<unsigned char*>(this);
    NoTextureDtorBody(base + 0xc8);
    if (ReferenceCountDtorFn fn = ReferenceCountDtor()) fn(base + 0xc8);
}
void __thiscall LifeThunks::OneTextureVbaseDtor()
{
    unsigned char* const base = reinterpret_cast<unsigned char*>(this);
    OneTextureDtorBody(base + 0xd4);
    if (ReferenceCountDtorFn fn = ReferenceCountDtor()) fn(base + 0xd4);
}
void __thiscall LifeThunks::BumpVbaseDtor()
{
    unsigned char* const base = reinterpret_cast<unsigned char*>(this);
    BumpDtorBody(base + 0xd8);
    if (ReferenceCountDtorFn fn = ReferenceCountDtor()) fn(base + 0xd8);
}

// ---- geometry chunks ------------------------------------------------------------------------------------------

void* __thiscall LifeThunks::BumpChunkTCtor(unsigned a, unsigned b, unsigned c, unsigned d, unsigned e)
{
    ChunkCtorPNTA()(this, a, b, c, d, e);
    *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(this) + 0x60) =
        Orig("??_7MultiPassBumpChunkT@Jet@@6BMultiPassBumpChunkBase@1@@");
    *reinterpret_cast<void**>(this) =
        Orig("??_7MultiPassBumpChunkT@Jet@@6B?$GeometryChunkTemplate@UPackedVertexPNTA@Jet@@VGeometryChunkSingleStream@2@@1@@");
    *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(this) + 0x60) =
        Orig("??_7MultiPassBumpChunkT@Jet@@6BMultiPassBumpChunkBase@1@@");
    return this;
}
void* __thiscall LifeThunks::BumpChunkT2Ctor(unsigned a, unsigned b, unsigned c, unsigned d, unsigned e)
{
    ChunkCtorPNTTA()(this, a, b, c, d, e);
    *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(this) + 0x60) =
        Orig("??_7MultiPassBumpChunkT2@Jet@@6BMultiPassBumpChunkBase@1@@");
    *reinterpret_cast<void**>(this) =
        Orig("??_7MultiPassBumpChunkT2@Jet@@6B?$GeometryChunkTemplate@UPackedVertexPNTTA@Jet@@VGeometryChunkSingleStream@2@@1@@");
    *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(this) + 0x60) =
        Orig("??_7MultiPassBumpChunkT2@Jet@@6BMultiPassBumpChunkBase@1@@");
    return this;
}
void __thiscall LifeThunks::BumpChunkTDtor()
{
    *reinterpret_cast<void**>(this) =
        Orig("??_7?$GeometryChunkTemplate@UPackedVertexPNTA@Jet@@VGeometryChunkSingleStream@2@@Jet@@6B@");
    int* const count = *reinterpret_cast<int**>(reinterpret_cast<unsigned char*>(this) + 0x5c);
    if (count) {
        --*count;
        if (*count == 0) {
            Alloc::Delete(count);
            *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(this) + 0x5c) = nullptr;
        }
    }
    ChunkStreamDtor()(this);
}
void __thiscall LifeThunks::BumpChunkT2Dtor()
{
    *reinterpret_cast<void**>(this) =
        Orig("??_7?$GeometryChunkTemplate@UPackedVertexPNTTA@Jet@@VGeometryChunkSingleStream@2@@Jet@@6B@");
    int* const count = *reinterpret_cast<int**>(reinterpret_cast<unsigned char*>(this) + 0x5c);
    if (count) {
        --*count;
        if (*count == 0) {
            Alloc::Delete(count);
            *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(this) + 0x5c) = nullptr;
        }
    }
    ChunkStreamDtor()(this);
}
bool __thiscall LifeThunks::BumpChunkTInitialize(void* display, bool load)
{
    // Display is a virtual object at display+0x60; the original calls slot 0 there with the display pointer,
    // then the PNTA template's InitializeBuffers (the T2 body is the same and calls the PNTA instantiation).
    unsigned char* const sub = static_cast<unsigned char*>(display) + 0x60;
    void** const vtable = *reinterpret_cast<void***>(sub);
    reinterpret_cast<void(__thiscall*)(void*, void*)>(vtable[0])(sub, display);
    return ChunkInitializePNTA()(this, display, load);
}

}  // namespace MultiPass
}  // namespace Jet

// ----------------------------------------------------------------------------------------------------------------
// the cdecl leaves: equality, InitInstance, CreateInstance

namespace Jet {
namespace MultiPass {
namespace Leaves {

namespace {

void CopyRef(const void* self, unsigned offset, void* target)
{
    void* const next = *reinterpret_cast<void* const*>(static_cast<const unsigned char*>(self) + offset);
    void*& slot = *reinterpret_cast<void**>(static_cast<unsigned char*>(target) + offset);
    void* const previous = slot;
    slot = next;
    AddDirect(next);
    if (previous) reinterpret_cast<const ReferenceCount*>(previous)->ReleaseReference();
}

bool SameBumpTexture(const void* self, const void* other)
{
    return *reinterpret_cast<void* const*>(static_cast<const unsigned char*>(self) + 0xd0) ==
           *reinterpret_cast<void* const*>(static_cast<const unsigned char*>(other) + 0xd0);
}

bool SamePrimaryTexture(const void* self, const void* other)
{
    return *reinterpret_cast<void* const*>(static_cast<const unsigned char*>(self) + 0xc4) ==
           *reinterpret_cast<void* const*>(static_cast<const unsigned char*>(other) + 0xc4);
}

bool SameMainTextures(const void* self, const void* other)
{
    const unsigned char* const a = static_cast<const unsigned char*>(self);
    const unsigned char* const b = static_cast<const unsigned char*>(other);
    return *reinterpret_cast<void* const*>(a + 0xa4) == *reinterpret_cast<void* const*>(b + 0xa4) &&
           *reinterpret_cast<void* const*>(a + 0xa8) == *reinterpret_cast<void* const*>(b + 0xa8) &&
           a[0xac] == b[0xac] && a[0xad] == b[0xad] && a[0xae] == b[0xae] && a[0xaf] == b[0xaf];
}

// The original's operator== starts with __RTDynamicCast(&other, Material, <class>); comparing the vftable's
// Complete Object Locator type descriptor is the same test for a non-derived runtime type and needs no RTTI
// object of our own. A cast to a base class also succeeds for the derived multipass classes, so the check models
// the hierarchy (NoTexture <- OneTexture <- Bump <- Bump2/BumpSpec <- BumpSpec2).
const void* TypeDescriptor(const void* object)
{
    void** const vtable = *reinterpret_cast<void***>(const_cast<void*>(object));
    const unsigned char* const locator = static_cast<const unsigned char*>(vtable[-1]);
    return *reinterpret_cast<const void* const*>(locator + 0x0c);
}

const void* DescriptorOfVtable(const char* name)
{
    void** const vtable = static_cast<void**>(Orig(name));
    const unsigned char* const locator = static_cast<const unsigned char*>(vtable[-1]);
    return *reinterpret_cast<const void* const*>(locator + 0x0c);
}

enum ClassRank { kNoTexture = 0, kOneTexture = 1, kBumpRank = 2, kBump2Rank = 3, kBumpSpecRank = 4,
                 kBumpSpec2Rank = 5 };

int RankOf(const void* descriptor)
{
    static const void* const descriptors[6] = {
        DescriptorOfVtable("??_7MultiPassNoTextureMaterial@Jet@@6BMaterial@1@@"),
        DescriptorOfVtable("??_7MultiPassOneTextureMaterial@Jet@@6BMaterial@1@@"),
        DescriptorOfVtable("??_7MultiPassBumpMaterial@Jet@@6BMaterial@1@@"),
        DescriptorOfVtable("??_7MultiPassBump2Material@Jet@@6BMaterial@1@@"),
        DescriptorOfVtable("??_7MultiPassBumpSpecMaterial@Jet@@6BMaterial@1@@"),
        DescriptorOfVtable("??_7MultiPassBumpSpec2Material@Jet@@6BMaterial@1@@")};
    for (int i = 0; i < 6; ++i)
        if (descriptors[i] == descriptor) return i;
    return -1;
}

bool SameClass(const void* self, const void* other, int selfRank)
{
    if (!other) return false;
    const int otherRank = RankOf(TypeDescriptor(other));
    if (otherRank < 0) return false;
    switch (selfRank) {
    case kNoTexture: return true;
    case kOneTexture: return otherRank != kNoTexture;
    case kBumpRank: return otherRank == kBumpRank || otherRank == kBump2Rank || otherRank == kBumpSpecRank ||
                            otherRank == kBumpSpec2Rank;
    case kBump2Rank: return otherRank == kBump2Rank;
    case kBumpSpecRank: return otherRank == kBumpSpecRank || otherRank == kBumpSpec2Rank;
    default: return otherRank == kBumpSpec2Rank;
    }
}

}  // namespace

bool r3d_u5b_eq_notexture(const void* self, const void* other)
{
    if (!SameClass(self, other, kNoTexture)) return false;
    return SameMainTextures(self, other) && MaterialEquals()(self, other);
}
bool r3d_u5b_eq_onetexture(const void* self, const void* other)
{
    if (!SameClass(self, other, kOneTexture)) return false;
    return SamePrimaryTexture(self, other) &&
           *reinterpret_cast<int const*>(static_cast<const unsigned char*>(self) + 0xc8) ==
               *reinterpret_cast<int const*>(static_cast<const unsigned char*>(other) + 0xc8) &&
           SameMainTextures(self, other) && MaterialEquals()(self, other);
}
bool r3d_u5b_eq_bump(const void* self, const void* other)
{
    if (!SameClass(self, other, kBumpRank)) return false;
    return SameBumpTexture(self, other) && SamePrimaryTexture(self, other) &&
           *reinterpret_cast<int const*>(static_cast<const unsigned char*>(self) + 0xc8) ==
               *reinterpret_cast<int const*>(static_cast<const unsigned char*>(other) + 0xc8) &&
           SameMainTextures(self, other) && MaterialEquals()(self, other);
}
bool r3d_u5b_eq_bump2(const void* self, const void* other)
{
    if (!SameClass(self, other, kBump2Rank)) return false;
    return SameBumpTexture(self, other) && SamePrimaryTexture(self, other) &&
           *reinterpret_cast<int const*>(static_cast<const unsigned char*>(self) + 0xc8) ==
               *reinterpret_cast<int const*>(static_cast<const unsigned char*>(other) + 0xc8) &&
           SameMainTextures(self, other) && MaterialEquals()(self, other);
}
bool r3d_u5b_eq_bumpspec(const void* self, const void* other)
{
    if (!SameClass(self, other, kBumpSpecRank)) return false;
    return SameBumpTexture(self, other) && SamePrimaryTexture(self, other) &&
           *reinterpret_cast<int const*>(static_cast<const unsigned char*>(self) + 0xc8) ==
               *reinterpret_cast<int const*>(static_cast<const unsigned char*>(other) + 0xc8) &&
           SameMainTextures(self, other) && MaterialEquals()(self, other);
}
bool r3d_u5b_eq_bumpspec2(const void* self, const void* other)
{
    if (!SameClass(self, other, kBumpSpec2Rank)) return false;
    return SameBumpTexture(self, other) && SamePrimaryTexture(self, other) &&
           *reinterpret_cast<int const*>(static_cast<const unsigned char*>(self) + 0xc8) ==
               *reinterpret_cast<int const*>(static_cast<const unsigned char*>(other) + 0xc8) &&
           SameMainTextures(self, other) && MaterialEquals()(self, other);
}

bool r3d_u5b_equals_notexture(const void* self, const void* other)
{
    return SameMainTextures(self, other) && MaterialEquals()(self, other);
}
bool r3d_u5b_equals_onetexture(const void* self, const void* other)
{
    return SamePrimaryTexture(self, other) &&
           *reinterpret_cast<int const*>(static_cast<const unsigned char*>(self) + 0xc8) ==
               *reinterpret_cast<int const*>(static_cast<const unsigned char*>(other) + 0xc8) &&
           SameMainTextures(self, other) && MaterialEquals()(self, other);
}
bool r3d_u5b_equals_bump(const void* self, const void* other)
{
    return SameBumpTexture(self, other) && SamePrimaryTexture(self, other) &&
           *reinterpret_cast<int const*>(static_cast<const unsigned char*>(self) + 0xc8) ==
               *reinterpret_cast<int const*>(static_cast<const unsigned char*>(other) + 0xc8) &&
           SameMainTextures(self, other) && MaterialEquals()(self, other);
}

void r3d_u5b_init_notexture(const void* self, void* target)
{
    MaterialInit()(self, target);
    CopyRef(self, 0xa4, target);
    CopyRef(self, 0xa8, target);
    unsigned char* const a = static_cast<unsigned char*>(const_cast<void*>(self));
    unsigned char* const b = static_cast<unsigned char*>(target);
    b[0xac] = a[0xac];
    b[0xad] = a[0xad];
    b[0xae] = a[0xae];
    b[0xaf] = a[0xaf];
}
void r3d_u5b_init_onetexture(const void* self, void* target)
{
    r3d_u5b_init_notexture(self, target);
    CopyRef(self, 0xc4, target);
    *reinterpret_cast<int*>(static_cast<unsigned char*>(target) + 0xc8) =
        *reinterpret_cast<int const*>(static_cast<const unsigned char*>(self) + 0xc8);
}
void r3d_u5b_init_bump(const void* self, void* target)
{
    r3d_u5b_init_onetexture(self, target);
    CopyRef(self, 0xd0, target);
}

void* r3d_u5b_create_notexture(const void* self)
{
    void* const type = OriginalExport(kTypeNoTexture);
    void* const created = MaterialCreate()(type, *reinterpret_cast<void* const*>(
                                                     static_cast<const unsigned char*>(self) + 0x98));
    r3d_u5b_init_notexture(self, created);
    return created;
}
void* r3d_u5b_create_onetexture(const void* self)
{
    void* const type = OriginalExport(kTypeOneTexture);
    void* const created = MaterialCreate()(type, *reinterpret_cast<void* const*>(
                                                     static_cast<const unsigned char*>(self) + 0x98));
    r3d_u5b_init_onetexture(self, created);
    return created;
}
void* r3d_u5b_create_bump(const void* self, const char* typeName)
{
    void* const type = OriginalExport(typeName);
    void* const created = MaterialCreate()(type, *reinterpret_cast<void* const*>(
                                                     static_cast<const unsigned char*>(self) + 0x98));
    r3d_u5b_init_onetexture(self, created);
    CopyRef(self, 0xd0, created);
    return created;
}

void* r3d_u5b_create_bumpmaterial(const void* self)
{
    return r3d_u5b_create_bump(self, kTypeBump);
}
void* r3d_u5b_create_bump2material(const void* self)
{
    return r3d_u5b_create_bump(self, kTypeBump2);
}
void* r3d_u5b_create_bumpspecmaterial(const void* self)
{
    return r3d_u5b_create_bump(self, kTypeBumpSpec);
}
void* r3d_u5b_create_bumpspec2material(const void* self)
{
    return r3d_u5b_create_bump(self, kTypeBumpSpec2);
}

}  // namespace Leaves
}  // namespace MultiPass
}  // namespace Jet
