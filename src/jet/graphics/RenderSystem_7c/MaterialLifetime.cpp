// U7 material/plugin lifetime and factory exports. Addresses and the hidden __thiscall virtual-base flags follow
// the TS12 assembly: Material 0x32650/0x327b0/0xbf50/0xc790/0x32510/0x325b0, PluginFactoryBase and
// MaterialPlugin 0x32410/0x32430/0x32440/0x32460/0x324e0/0x335a0, and NullMaterial
// 0x37760/0x37650/0x37740/0x37840/0x378c0. All exports keep the original names/ordinals through implemented.txt;
// the helpers below are plain C symbols aliased there.
//
// Every operation the original imports (the allocator, the PString cache, ReferenceCount, PluginType and
// PluginBase::LookupPluginType) is expressed here through the project headers so it goes through this module's
// import table. The one exception is PluginInstance's ctor: the hidden virtual-base flag cannot be written with
// the C++ declaration, so that single import is still resolved by name.
#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/Plugin.h>

#include <windows.h>

#include <cstddef>
#include <new>

namespace Jet {
void __cdecl RsMaterialInit(void*);
void __cdecl RsMatInitInstance(void*, void*);
bool __cdecl RsMatEquals(void*, void*);
}  // namespace Jet

// C++ linkage gives these data symbols '?' names; a .def alias to an undecorated C name makes LINK truncate the
// mangled export name at its first '@'.
void* RsMatVftableMaterial[];
void* RsMatVftableMaterialInstance[];
int RsMatVbtableMaterial[];
void* RsMatVftableMaterialPlugin[];
void* RsMatVftablePluginFactory[];
void* RsMatVftableNullMaterial[];
void* RsMatVftableNullMaterialInstance[];
int RsMatVbtableNullMaterial[];
unsigned char RsMatPluginGroupUid[];

namespace {

HMODULE Module(const char* name) { return GetModuleHandleA(name); }

template <class F>
F Sym(HMODULE module, const char* name)
{
    return reinterpret_cast<F>(GetProcAddress(module, name));
}

// The ReferenceCount subobject exists on both sides; its vftable is exported by KernelStandard.
void* ReferenceCountVtable()
{
    if (HMODULE const ks = GetModuleHandleA("KernelStandard_7c.dll"))
        if (void* const local = reinterpret_cast<void*>(GetProcAddress(ks, "??_7ReferenceCount@Jet@@6B@")))
            return local;
    return nullptr;
}

// ReferenceCount's destructor is protected, and the original calls it non-virtually on the base subobject. A small
// derived access struct makes the imported symbol reachable without changing that layout.
struct ReferenceAccess : Jet::ReferenceCount {
    static void Add(void* p) { static_cast<ReferenceAccess*>(p)->AddReference(); }
    static bool Release(void* p) { return static_cast<ReferenceAccess*>(p)->ReleaseReference(); }
    static void Destroy(void* p) { static_cast<ReferenceAccess*>(p)->~ReferenceAccess(); }
};

// PluginInstance's ctor takes a hidden stack flag to construct its virtual ReferenceCount base. The exported C++
// declaration has no such argument, so it is the only function still fetched by name.
using CtorHidden = void(__thiscall*)(void*, int);
CtorHidden PluginInstanceCtor()
{
    static CtorHidden const f = Sym<CtorHidden>(Module("Kernel_7c.dll"), "??0PluginInstance@Jet@@QAE@XZ");
    return f;
}

void DestroyStringNode(void* node)
{
    if (node && Jet::PString::Cache()) Jet::PString::Cache()->Destroy(static_cast<Jet::PStringCache::Node*>(node));
}

// the incremental tables the virtuals are called through (__cdecl: `this` is the first stack argument)
template <class R, class... A>
R VCall(const void* obj, unsigned offset, A... args)
{
    using F = R(__cdecl*)(const void*, A...);
    return (*reinterpret_cast<F* const*>(obj))[offset / sizeof(void*)](obj, args...);
}
void PluginRelease(void* plugin)
{
    // PluginType::ReleaseReference is __cdecl in Jet (the vftable slot takes `this` on the stack)
    using F = void(__cdecl*)(void*);
    (*reinterpret_cast<F* const*>(plugin))[1](plugin);
}

// 0x1000d750 / 0x1000d800: both rings are freed chunk by chunk; properties destroy value before key
using Chunk = unsigned char*;
void ClearRing(void** headAddress, unsigned itemSize, bool properties)
{
    Chunk first = static_cast<Chunk>(*headAddress);
    if (!first) return;
    *headAddress = nullptr;
    auto freeChunk = [&](Chunk chunk) {
        const unsigned count = *reinterpret_cast<unsigned*>(chunk + 8);
        unsigned char* item = chunk + 12;
        for (unsigned i = 0; i < count; ++i, item += itemSize) {
            if (properties) {
                DestroyStringNode(*reinterpret_cast<void**>(item + 4));
                DestroyStringNode(*reinterpret_cast<void**>(item));
            } else {
                DestroyStringNode(*reinterpret_cast<void**>(item + 4));
            }
        }
        Jet::Alloc::Delete(chunk);
    };
    Chunk chunk = *reinterpret_cast<Chunk*>(first);
    while (chunk != first) {
        Chunk next = *reinterpret_cast<Chunk*>(chunk);
        freeChunk(chunk);
        chunk = next;
    }
    freeChunk(first);
}

void ClearMaterialStrings(unsigned char* material)
{
    for (unsigned offset = 0x30;; offset -= 4) {
        DestroyStringNode(*reinterpret_cast<void**>(material + offset));
        if (offset == 0x24) break;
    }
}

// 0x100335d0 / 0x10033660: the by-name lookup uses the material plugin group as the parent UId; by-id uses only
// the key's UId. The import returns a Result by value; only code 0 yields the found plugin.
struct PluginAccess : Jet::PluginBase {
    using Jet::PluginBase::LookupPluginType;
};

void* LookupMaterialPlugin(const void* key, bool byId)
{
    Jet::PluginType* found = nullptr;
    if (byId) {
        const Jet::Result result = PluginAccess::LookupPluginType(&found, *static_cast<const Jet::UId*>(key));
        return result.code == 0 ? found : nullptr;
    }
    const Jet::Result result = PluginAccess::LookupPluginType(
        &found, *reinterpret_cast<const Jet::UId*>(RsMatPluginGroupUid), *static_cast<const Jet::PString*>(key));
    return result.code == 0 ? found : nullptr;
}

void* CreateFromPlugin(const void* key, void* manager, bool byId)
{
    void* plugin = LookupMaterialPlugin(key, byId);
    if (!plugin) return nullptr;
    void* kernel = *reinterpret_cast<void**>(static_cast<unsigned char*>(manager) + 8);
    void* material = VCall<void*>(plugin, 0x14, manager, kernel, 0);
    PluginRelease(plugin);
    return material;
}

// the complete Material vftable/vbtable of a derived object are read through its own vbptr
int VbOffset(const unsigned char* materialBase)
{
    const void* const vbtable = *reinterpret_cast<void* const*>(materialBase + 4);
    return static_cast<const int*>(vbtable)[1];
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// PluginFactoryBase<Material> and MaterialPlugin

extern "C" void* __fastcall RsU7FactoryCopyCtor(void* self, void*, const void* source)
{
    ::new (self) Jet::PluginType(*static_cast<const Jet::PluginType*>(source));
    *reinterpret_cast<void**>(self) = RsMatVftablePluginFactory;
    return self;
}
extern "C" void* __fastcall RsU7FactoryCtor(void* self, void*, const void* name, const void* id, const void* parent)
{
    ::new (self) Jet::PluginType(*static_cast<const Jet::PString*>(name), *static_cast<const Jet::UId*>(id),
                                 *static_cast<const Jet::UId*>(parent));
    *reinterpret_cast<void**>(self) = RsMatVftablePluginFactory;
    return self;
}
extern "C" void __fastcall RsU7FactoryDtor(void* self, void*)
{
    // Must be non-virtual. An unqualified `->~PluginType()` compiles to `call [vptr+8]` (vftable slot 2);
    // on the crashing objects that slot leads back to this export, so the destructor called itself forever.
    // The original calls ~PluginType non-virtually (0x32430/0x32480 use the imported symbol directly).
    static_cast<Jet::PluginType*>(self)->Jet::PluginType::~PluginType();
}
extern "C" void* __fastcall RsU7FactoryAssign(void* self, void*, const void* source)
{
    *static_cast<Jet::PluginType*>(self) = *static_cast<const Jet::PluginType*>(source);
    return self;
}
extern "C" void* __fastcall RsU7MaterialPluginCopyCtor(void* self, void*, const void* source)
{
    ::new (self) Jet::PluginType(*static_cast<const Jet::PluginType*>(source));
    *reinterpret_cast<void**>(self) = RsMatVftableMaterialPlugin;
    return self;
}
extern "C" void* __fastcall RsU7MaterialPluginCtor(void* self, void*, const void* name, const void* id)
{
    ::new (self) Jet::PluginType(*static_cast<const Jet::PString*>(name), *static_cast<const Jet::UId*>(id),
                                 *reinterpret_cast<const Jet::UId*>(RsMatPluginGroupUid));
    *reinterpret_cast<void**>(self) = RsMatVftableMaterialPlugin;
    return self;
}

// ---------------------------------------------------------------------------------------------------------------
// Material

extern "C" void* __fastcall RsU7MaterialCtor(void* self, void*, void* manager, int constructVirtualBases)
{
    auto* p = static_cast<unsigned char*>(self);
    if (constructVirtualBases) {
        *reinterpret_cast<void**>(p + 4) = RsMatVbtableMaterial;
        const int offset = VbOffset(p);
        *reinterpret_cast<void**>(p + 4 + offset) = ReferenceCountVtable();  // ReferenceCount's vftable
        *reinterpret_cast<int*>(p + 8 + offset) = 1;
    }
    PluginInstanceCtor()(p + 4, 0);
    *reinterpret_cast<int*>(p + 0x10) = 0;
    *reinterpret_cast<int*>(p + 0xc) = 0;
    *reinterpret_cast<void**>(p) = RsMatVftableMaterial;
    const int offset = VbOffset(p);
    *reinterpret_cast<void**>(p + 4 + offset) = RsMatVftableMaterialInstance;
    *reinterpret_cast<int*>(p + offset) = offset - 0x9c;  // vtordisp

    // the DisplayMaterial defaults (the original writes +0x20..+0x74, nothing beyond), then the empty rings
    *reinterpret_cast<float*>(p + 0x20) = 2.5f;
    for (unsigned offset = 0x24; offset <= 0x74; offset += 4)
        *reinterpret_cast<unsigned*>(p + offset) = 0;
    *reinterpret_cast<float*>(p + 0x40) = 1.0f;
    *reinterpret_cast<float*>(p + 0x50) = 1.0f;
    *reinterpret_cast<float*>(p + 0x60) = 1.0f;
    *reinterpret_cast<float*>(p + 0x70) = 1.0f;
    *reinterpret_cast<int*>(p + 0x88) = 0;
    *reinterpret_cast<int*>(p + 0x90) = 0;

    *reinterpret_cast<void**>(p + 0x98) = manager;
    if (manager) ReferenceAccess::Add(manager);
    Jet::RsMaterialInit(p);  // the original calls Init last, after the manager is stored and referenced
    return self;
}

extern "C" void __fastcall RsU7MaterialDtor(void* self, void*)
{
    // the exported destructor receives Material+0xa0 (the virtual ReferenceCount base)
    auto* base = static_cast<unsigned char*>(self);
    auto* p = base - 0xa0;
    const int offset = VbOffset(p);
    *reinterpret_cast<void**>(p) = RsMatVftableMaterial;
    *reinterpret_cast<void**>(p + 4 + offset) = RsMatVftableMaterialInstance;
    *reinterpret_cast<int*>(p + offset) = offset - 0x9c;  // vtordisp

    void* manager = *reinterpret_cast<void**>(p + 0x98);
    if (manager) {
        int result = 0;
        VCall<int*>(manager, 0x14, &result, p, 0);  // MaterialManager::Remove
        ReferenceAccess::Release(manager);
    }
    ClearRing(reinterpret_cast<void**>(p + 0x90), 12, false);
    ClearRing(reinterpret_cast<void**>(p + 0x88), 8, true);
    ClearMaterialStrings(p);
}

extern "C" void __fastcall RsU7MaterialVbaseDtor(void* self, void*)
{
    auto* p = static_cast<unsigned char*>(self);
    RsU7MaterialDtor(p + 0xa0, nullptr);
    // the original's vbase destructor calls ReferenceCount's (non-virtual) destructor, which only replaces the
    // virtual base's vptr. Doing the write directly avoids a second virtual dispatch through the just-replaced
    // vftable, and keeps the primary vptr pointing at this module's mirror.
    *reinterpret_cast<void**>(p + 0xa0) = ReferenceCountVtable();
    *reinterpret_cast<void**>(p) = RsMatVftableMaterial;
}

bool __cdecl RsU7MaterialLess(const void* self, const void* other)
{
    // 0x100c790: the slot is called on `other` first, then on `this`; the result is selfKey < otherKey
    const unsigned otherKey = VCall<unsigned>(other, 0x54);
    const unsigned selfKey = VCall<unsigned>(self, 0x54);
    return selfKey < otherKey;
}

void* __cdecl RsU7MaterialCreateInstanceId(const void* id, void* manager)
{
    return CreateFromPlugin(id, manager, true);
}
void* __cdecl RsU7MaterialCreateInstanceName(const void* name, void* manager)
{
    return CreateFromPlugin(name, manager, false);
}

// ---------------------------------------------------------------------------------------------------------------
// NullMaterial

extern "C" void* __fastcall RsU7NullMaterialCtor(void* self, void*, const void* type, int constructVirtualBases)
{
    auto* p = static_cast<unsigned char*>(self);
    if (constructVirtualBases) {
        *reinterpret_cast<void**>(p + 4) = RsMatVbtableNullMaterial;
        const int offset = VbOffset(p);
        *reinterpret_cast<void**>(p + 4 + offset) = ReferenceCountVtable();
        *reinterpret_cast<int*>(p + 8 + offset) = 1;
    }
    RsU7MaterialCtor(p, nullptr, nullptr, 0);  // the original pushes 0 for Material's hidden flag
    *reinterpret_cast<void**>(p) = RsMatVftableNullMaterial;
    const int offset = VbOffset(p);
    *reinterpret_cast<void**>(p + 4 + offset) = RsMatVftableNullMaterialInstance;
    *reinterpret_cast<int*>(p + offset) = offset - 0xa0;

    auto* node = static_cast<Jet::PStringCache::Node*>(*static_cast<void* const*>(type));
    if (Jet::PStringCache* cache = Jet::PString::Cache()) {
        if (node) {
            cache->Lock();
            ++node->refs;
            ++cache->liveRefs;
            cache->Unlock();
        }
    }
    *reinterpret_cast<Jet::PStringCache::Node**>(p + 0x9c) = node;
    return self;
}

extern "C" void __fastcall RsU7NullMaterialDtor(void* self, void*)
{
    // the exported destructor receives NullMaterial+0xa4
    auto* base = static_cast<unsigned char*>(self);
    DestroyStringNode(*reinterpret_cast<void**>(base - 8));
    RsU7MaterialDtor(base - 4, nullptr);
}

extern "C" void __fastcall RsU7NullMaterialVbaseDtor(void* self, void*)
{
    auto* p = static_cast<unsigned char*>(self);
    RsU7NullMaterialDtor(p + 0xa4, nullptr);
    // as in Material's vbase destructor, write ReferenceCount's vptr directly: the imported virtual destructor
    // would dispatch through the vftable we have just replaced and leave an original-module pointer here
    *reinterpret_cast<void**>(p + 0xa4) = ReferenceCountVtable();
    *reinterpret_cast<void**>(p) = RsMatVftableNullMaterial;
}

void* __cdecl RsU7NullCreateInstance(const void* self)
{
    auto* source = static_cast<const unsigned char*>(self);
    void* result = Jet::Alloc::New(0xac);
    if (result) RsU7NullMaterialCtor(result, nullptr, source + 0x9c, 1);
    Jet::RsMatInitInstance(const_cast<void*>(self), result);
    return result;
}

bool __cdecl RsU7NullEquals(const void* self, const void* other)
{
    if (!other) return false;
    // __RTDynamicCast in the original accepts any NullMaterial; only local objects exist now.
    const void* vptr = *static_cast<void* const*>(other);
    if (vptr != RsMatVftableNullMaterial) return false;
    const auto* a = static_cast<const unsigned char*>(self);
    const auto* b = static_cast<const unsigned char*>(other);
    if (*reinterpret_cast<void* const*>(a + 0x9c) != *reinterpret_cast<void* const*>(b + 0x9c)) return false;
    return Jet::RsMatEquals(const_cast<void*>(self), const_cast<void*>(other));
}
