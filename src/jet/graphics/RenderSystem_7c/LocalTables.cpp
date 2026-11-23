// U16: local vtables and RTTI for the objects this module constructs itself.  The exported data
// objects still mirror the retail tables while the renamed original is present (DataStatics and
// MaterialStatics copy it at load); without it these builders supply every slot plus a real
// MSVC RTTI locator, so dynamic_cast and exception handling see the retail class hierarchy.
#include <jet/KernelStandard.h>
#include <jet/RenderSystem.h>
#include <windows.h>

#include <cstring>
#include <typeinfo>

#include "rs_local_tables.h"

namespace Jet {

class RenderKernel;
class OS;
class DisplaySelector;
class LayerManager;
int __cdecl RsRenderKernelUpdate(RenderKernel*, float);
bool __cdecl RsRenderKernelInit(RenderKernel*, OS*, DisplaySelector*);
RenderKernel* __cdecl RsKernelFromLayerManager(void*);

// Destructor bodies from the other units of this module (exports under their retail names).
extern "C" void __fastcall RsDisplayContextDtor(unsigned char*, void*);
extern "C" void __fastcall RsDisplayVbaseDtor(unsigned char*);
extern "C" void __fastcall RsSharedContextDtor(void*, void*);
extern "C" void __fastcall RsNonRenderContextDtor(void*, void*);
extern "C" void __fastcall RsSaveBufferDtor(void*, void*);
extern "C" void __fastcall RsSelectorDtor(void*, void*);
extern "C" void __fastcall RsElementLoaderDtor(FARPROC*, void*);
extern "C" void __fastcall RsU7MaterialVbaseDtor(void*, void*);
extern "C" void __fastcall RsU7NullMaterialVbaseDtor(void*, void*);
extern "C" void __fastcall RsU7FactoryDtor(void*, void*);
void __fastcall RsMaterialConfigDestroy(void*, void*);

extern "C" int __cdecl RsLocalStubZero() { return 0; }
extern "C" const void* RsLocalRttiLocator(const char* tableName);
extern "C" void* __fastcall RsRenderKernelDeletingDtor(void*, void*, unsigned);
extern "C" void* __fastcall RsRenderKernelLayerDeletingDtor(void*, void*, unsigned);

namespace {

constexpr unsigned DisplayReferenceBase = 0xf04;
constexpr unsigned DisplayObjectSize = 0xf0c;

// The one local word every stub slot resolves to.  It matches the shared retail no-op bodies
// (`xor eax,eax; ret`): those slots are pure virtuals, never reached by a concrete backend.
HMODULE SelfModule()
{
    HMODULE module = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&RsLocalStubZero), &module);
    return module;
}

HMODULE ImportModule(const char* symbol)
{
    if (std::strncmp(symbol, "KernelStandard_7c.dll!", 22) == 0)
        return GetModuleHandleA("KernelStandard_7c.dll");
    if (std::strncmp(symbol, "Kernel_7c.dll!", 14) == 0)
        return GetModuleHandleA("Kernel_7c.dll");
    return nullptr;
}

void* ResolveSymbol(const char* symbol)
{
    if (!symbol) return nullptr;
    if (const char* const bang = std::strchr(symbol, '!')) {
        HMODULE const module = ImportModule(symbol);
        return module ? reinterpret_cast<void*>(GetProcAddress(module, bang + 1)) : nullptr;
    }
    HMODULE const self = SelfModule();
    return self ? reinterpret_cast<void*>(GetProcAddress(self, symbol)) : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// MSVC 32-bit RTTI objects, built from the generated layout numbers.  The type descriptors carry
// the retail decorated names, so casts and catch clauses match objects built by the original and
// by this module alike.  Their vftable is this CRT's type_info table (not a retail pointer).

struct LocalTypeDescriptor {
    const void* vftable;
    void* spare;
    char name[RS_LOCAL_RTTI_MAX_NAME];
};

struct LocalBaseClassDescriptor {
    const LocalTypeDescriptor* type;
    unsigned numContainedBases;
    int mdisp;
    int pdisp;
    int vdisp;
    unsigned attributes;
};

struct LocalClassHierarchyDescriptor {
    unsigned signature;
    unsigned attributes;
    unsigned numBaseClasses;
    const LocalBaseClassDescriptor* const* baseArray;
};

struct LocalCompleteObjectLocator {
    unsigned signature;
    unsigned offset;
    unsigned cdOffset;
    const LocalTypeDescriptor* type;
    const LocalClassHierarchyDescriptor* hierarchy;
};

const void* TypeInfoVtable()
{
    static const void* const table = *reinterpret_cast<void* const*>(&typeid(int));
    return table;
}

struct LocalRttiSet {
    LocalTypeDescriptor types[RS_LOCAL_RTTI_TYPE_COUNT];
    LocalBaseClassDescriptor bases[RS_LOCAL_RTTI_BASE_COUNT];
    const LocalBaseClassDescriptor* basePointers[RS_LOCAL_RTTI_BASE_COUNT];
    LocalClassHierarchyDescriptor classes[RS_LOCAL_RTTI_CLASS_COUNT];
    LocalCompleteObjectLocator locators[RS_LOCAL_RTTI_TABLE_COUNT];

    LocalRttiSet()
    {
        const void* const typeInfo = TypeInfoVtable();
        for (unsigned i = 0; i < RS_LOCAL_RTTI_TYPE_COUNT; ++i) {
            std::memset(&types[i], 0, sizeof types[i]);
            types[i].vftable = typeInfo;
            std::strncpy(types[i].name, RsLocalRttiTypeNames[i], RS_LOCAL_RTTI_MAX_NAME - 1);
        }
        for (unsigned i = 0; i < RS_LOCAL_RTTI_BASE_COUNT; ++i) {
            LocalBaseClassDescriptor& base = bases[i];
            base.type = &types[RsLocalRttiBases[i][0]];
            base.numContainedBases = static_cast<unsigned>(RsLocalRttiBases[i][1]);
            base.mdisp = RsLocalRttiBases[i][2];
            base.pdisp = RsLocalRttiBases[i][3];
            base.vdisp = RsLocalRttiBases[i][4];
            base.attributes = static_cast<unsigned>(RsLocalRttiBases[i][5]);
            basePointers[i] = &base;
        }
        for (unsigned i = 0; i < RS_LOCAL_RTTI_CLASS_COUNT; ++i) {
            LocalClassHierarchyDescriptor& hierarchy = classes[i];
            hierarchy.signature = RsLocalRttiClassSignature[i];
            hierarchy.attributes = RsLocalRttiClassAttributes[i];
            hierarchy.numBaseClasses = RsLocalRttiClassBaseCount[i];
            hierarchy.baseArray = &basePointers[RsLocalRttiClassFirstBase[i]];
        }
        for (unsigned i = 0; i < RS_LOCAL_RTTI_TABLE_COUNT; ++i) {
            LocalCompleteObjectLocator& locator = locators[i];
            locator.signature = RsLocalRttiTableSignature[i];
            locator.offset = RsLocalRttiTableOffset[i];
            locator.cdOffset = RsLocalRttiTableCdOffset[i];
            locator.type = &types[RsLocalRttiClassType[RsLocalRttiTableClass[i]]];
            locator.hierarchy = &classes[RsLocalRttiTableClass[i]];
        }
    }
};

const LocalRttiSet& Rtti()
{
    static const LocalRttiSet set;
    return set;
}

void SetLocator(unsigned char* table, void** meta, const char* tableName)
{
    const void* const locator = RsLocalRttiLocator(tableName);
    if (!locator) return;
    if (meta) *meta = const_cast<void*>(locator);
    std::memcpy(table - sizeof(void*), &locator, sizeof locator);
}

void FillGenerated(unsigned char* table, const char* const* symbols, unsigned slots, void** meta,
                   const char* tableName)
{
    auto** const entries = reinterpret_cast<void**>(table);
    for (unsigned i = 0; i < slots; ++i) {
        void* const symbol = ResolveSymbol(symbols[i]);
        entries[i] = symbol ? symbol : reinterpret_cast<void*>(&RsLocalStubZero);
    }
    SetLocator(table, meta, tableName);
}

void FillRenderKernelTask(unsigned char* table, void** meta)
{
    auto** const slots = reinterpret_cast<void**>(table);
    HMODULE const kernel = GetModuleHandleA("Kernel_7c.dll");
    auto** const task = kernel
        ? reinterpret_cast<void**>(GetProcAddress(kernel, "??_7Task@Jet@@6B@")) : nullptr;
    if (task) {
        for (int i = 0; i < 5; ++i) slots[i] = task[i];
    } else {
        for (int i = 0; i < 5; ++i) slots[i] = reinterpret_cast<void*>(&RsLocalStubZero);
    }
    // The base table's slot 0 destroys a Task; a RenderKernel needs its own deleting destructor.
    slots[0] = reinterpret_cast<void*>(&RsRenderKernelDeletingDtor);
    slots[4] = reinterpret_cast<void*>(&RsRenderKernelUpdate);
    slots[5] = reinterpret_cast<void*>(&RsRenderKernelInit);
    SetLocator(table, meta, "??_7RenderKernel@Jet@@6BTask@1@@");
}

void FillRenderKernelLayer(unsigned char* table, void** meta)
{
    auto** const slots = reinterpret_cast<void**>(table);
    slots[0] = reinterpret_cast<void*>(&RsRenderKernelLayerDeletingDtor);
    slots[1] = reinterpret_cast<void*>(&RsKernelFromLayerManager);
    SetLocator(table, meta, "??_7RenderKernel@Jet@@6BLayerManager@1@@");
}

void FillProfileInfo(unsigned char* table, const char* name, const char* file)
{
    struct Entry { const char* name; const char* file; unsigned count, time; int id; unsigned mask, peek, unknown; };
    const Entry value{name, file, 0, 0, -1, 0x80000000u, 0, 0};
    std::memcpy(table, &value, sizeof value);
}

// ---------------------------------------------------------------------------------------------------------------
// Deleting destructors and adjustors that the compiler generated in the retail image and therefore
// have no export.  Slot 0 is a __cdecl DeleteThis (this on the stack); slot 1 is a __thiscall
// scalar/vector deleting destructor (flags on the stack, callee pops).

void __cdecl DeletePluginInstance(void* selfAtTwelve)
{
    using Fn = void(__cdecl*)(void*);
    static Fn const fn = reinterpret_cast<Fn>(GetProcAddress(
        GetModuleHandleA("Kernel_7c.dll"), "?DeleteThis@PluginInstance@Jet@@MBAXXZ"));
    if (fn) fn(selfAtTwelve);
}

// ??1RenderKernel is exported by RenderKernelLifetime.cpp as a __thiscall ABI shim.
void DestroyRenderKernel(void* kernel)
{
    using Fn = void(__thiscall*)(void*);
    static Fn const fn = reinterpret_cast<Fn>(ResolveSymbol("??1RenderKernel@Jet@@UAE@XZ"));
    if (fn) fn(kernel);
}

template <unsigned Stride, class Body>
void* DeleteArray(unsigned char* first, unsigned flags, Body body)
{
    unsigned count = *reinterpret_cast<unsigned*>(first - sizeof(unsigned));
    while (count) body(first + Stride * --count);
    if (flags & 1) Alloc::Delete(first - sizeof(unsigned));
    return first - sizeof(unsigned);
}

}  // namespace

// The Display virtual base: retail's two-slot PluginInstance table adjustors.  DeleteThis gets
// PluginInstance+0xc (the vtordisp-adjusted pointer the Kernel implementation expects); the
// deleting destructor gets the ReferenceCount subobject and recovers the object from its vtordisp.
extern "C" void __cdecl RsDisplayPluginDeleteThis(void* subobject)
{
    auto* const base = static_cast<unsigned char*>(subobject);
    int const displacement = *reinterpret_cast<int*>(base - sizeof(int));
    DeletePluginInstance(base - displacement - 0xef4);
}

extern "C" void* __fastcall RsDisplayPluginDeletingDtor(void* subobject, void*, unsigned flags)
{
    auto* const base = static_cast<unsigned char*>(subobject);
    int const displacement = *reinterpret_cast<int*>(base - sizeof(int));
    auto* const display = base - displacement - DisplayReferenceBase;
    if (flags & 2) {
        return DeleteArray<DisplayObjectSize>(display, flags, [](unsigned char* element) {
            RsDisplayVbaseDtor(element);
        });
    }
    RsDisplayVbaseDtor(display);
    if (flags & 1) Alloc::Delete(display);
    return display;
}

extern "C" void* __fastcall RsDisplayContextDeletingDtor(void* state, void*, unsigned flags)
{
    auto* const p = static_cast<unsigned char*>(state);
    if (flags & 2) {
        return DeleteArray<0x158>(p, flags, [](unsigned char* element) {
            RsDisplayContextDtor(element, nullptr);
        });
    }
    RsDisplayContextDtor(p, nullptr);
    if (flags & 1) Alloc::Delete(p);
    return p;
}

extern "C" void* __fastcall RsSaveBufferDeletingDtor(void* storage, void*, unsigned flags)
{
    auto* const p = static_cast<unsigned char*>(storage);
    if (flags & 2) {
        return DeleteArray<0x0c>(p, flags, [](unsigned char* element) {
            RsSaveBufferDtor(element, nullptr);
        });
    }
    RsSaveBufferDtor(p, nullptr);
    if (flags & 1) Alloc::Delete(p);
    return p;
}

extern "C" void* __fastcall RsElementLoaderDeletingDtor(void* storage, void*, unsigned flags)
{
    auto* const p = static_cast<unsigned char*>(storage);
    if (flags & 2) {
        return DeleteArray<0x04>(p, flags, [](unsigned char* element) {
            RsElementLoaderDtor(reinterpret_cast<FARPROC*>(element), nullptr);
        });
    }
    RsElementLoaderDtor(reinterpret_cast<FARPROC*>(p), nullptr);
    if (flags & 1) Alloc::Delete(p);
    return p;
}

extern "C" void* __fastcall RsSharedContextDeletingDtor(void* storage, void*, unsigned flags)
{
    auto* const p = static_cast<unsigned char*>(storage);
    if (flags & 2) {
        return DeleteArray<0x08>(p, flags, [](unsigned char* element) {
            RsSharedContextDtor(element, nullptr);
        });
    }
    RsSharedContextDtor(p, nullptr);
    if (flags & 1) Alloc::Delete(p);
    return p;
}

extern "C" void* __fastcall RsNonRenderContextDeletingDtor(void* storage, void*, unsigned flags)
{
    auto* const p = static_cast<unsigned char*>(storage);
    if (flags & 2) {
        return DeleteArray<0x08>(p, flags, [](unsigned char* element) {
            RsNonRenderContextDtor(element, nullptr);
        });
    }
    RsNonRenderContextDtor(p, nullptr);
    if (flags & 1) Alloc::Delete(p);
    return p;
}

extern "C" void* __fastcall RsSelectorDeletingDtor(void* storage, void*, unsigned flags)
{
    auto* const p = static_cast<unsigned char*>(storage);
    if (flags & 2) {
        return DeleteArray<0x08>(p, flags, [](unsigned char* element) {
            RsSelectorDtor(element, nullptr);
        });
    }
    RsSelectorDtor(p, nullptr);
    if (flags & 1) Alloc::Delete(p);
    return p;
}

extern "C" void* __fastcall RsFactoryBaseDeletingDtor(void* storage, void*, unsigned flags)
{
    auto* const p = static_cast<unsigned char*>(storage);
    if (flags & 2) {
        return DeleteArray<0x20>(p, flags, [](unsigned char* element) {
            RsU7FactoryDtor(element, nullptr);
        });
    }
    RsU7FactoryDtor(p, nullptr);
    if (flags & 1) Alloc::Delete(p);
    return p;
}

extern "C" void* __fastcall RsMaterialConfigDeletingDtor(void* storage, void*, unsigned flags)
{
    auto* const p = static_cast<unsigned char*>(storage);
    if (flags & 2) {
        return DeleteArray<0x04>(p, flags, [](unsigned char* element) {
            RsMaterialConfigDestroy(element, nullptr);
        });
    }
    RsMaterialConfigDestroy(p, nullptr);
    if (flags & 1) Alloc::Delete(p);
    return p;
}

extern "C" void __cdecl RsMaterialPluginDeleteThis(void* subobject)
{
    auto* const base = static_cast<unsigned char*>(subobject);
    int const displacement = *reinterpret_cast<int*>(base - sizeof(int));
    DeletePluginInstance(base - displacement - 0x90);
}

extern "C" void* __fastcall RsMaterialDeletingDtor(void* subobject, void*, unsigned flags)
{
    auto* const base = static_cast<unsigned char*>(subobject);
    int const displacement = *reinterpret_cast<int*>(base - sizeof(int));
    auto* const material = base - displacement - 0xa0;
    if (flags & 2) {
        return DeleteArray<0xa8>(material, flags, [](unsigned char* element) {
            RsU7MaterialVbaseDtor(element, nullptr);
        });
    }
    RsU7MaterialVbaseDtor(material, nullptr);
    if (flags & 1) Alloc::Delete(material);
    return material;
}

extern "C" void __cdecl RsNullMaterialPluginDeleteThis(void* subobject)
{
    auto* const base = static_cast<unsigned char*>(subobject);
    int const displacement = *reinterpret_cast<int*>(base - sizeof(int));
    DeletePluginInstance(base - displacement - 0x94);
}

extern "C" void* __fastcall RsNullMaterialDeletingDtor(void* subobject, void*, unsigned flags)
{
    auto* const base = static_cast<unsigned char*>(subobject);
    int const displacement = *reinterpret_cast<int*>(base - sizeof(int));
    auto* const material = base - displacement - 0xa4;
    if (flags & 2) {
        return DeleteArray<0xac>(material, flags, [](unsigned char* element) {
            RsU7NullMaterialVbaseDtor(element, nullptr);
        });
    }
    RsU7NullMaterialVbaseDtor(material, nullptr);
    if (flags & 1) Alloc::Delete(material);
    return material;
}

// RenderKernel's Task table and the LayerManager subobject table both lead to the kernel's own
// destructor; the LayerManager table sits 0x28 bytes into the object.
extern "C" void* __fastcall RsRenderKernelDeletingDtor(void* kernel, void*, unsigned flags)
{
    auto* const p = static_cast<unsigned char*>(kernel);
    if (flags & 2) {
        return DeleteArray<0xe8>(p, flags, [](unsigned char* element) {
            DestroyRenderKernel(element);
        });
    }
    DestroyRenderKernel(p);
    if (flags & 1) Alloc::Delete(p);
    return p;
}

extern "C" void* __fastcall RsRenderKernelLayerDeletingDtor(void* layerManager, void*, unsigned flags)
{
    return RsRenderKernelDeletingDtor(static_cast<unsigned char*>(layerManager) - 0x28, nullptr, flags);
}

// Returns true when this export has a local definition, so the mirror keeps it instead of zeroes.
extern "C" bool RsFillLocalVtable(unsigned char* table, void** meta, const char* name)
{
    struct SlotMap {
        const char* name;
        const char* const* symbols;
        unsigned slots;
    };
    static const SlotMap maps[] = {
        {"??_7Display@Jet@@6B01@@", RsLocalVt___7Display_Jet__6B01__, 152},
        {"??_7Display@Jet@@6BPluginInstance@1@@", RsLocalVt___7Display_Jet__6BPluginInstance_1__, 2},
        {"??_7DisplayContext@Display@Jet@@6B@", RsLocalVt___7DisplayContext_Display_Jet__6B_, 1},
        {"??_7DisplaySaveBuffer@Jet@@6B@", RsLocalVt___7DisplaySaveBuffer_Jet__6B_, 4},
        {"??_7DisplaySelector@Jet@@6B@", RsLocalVt___7DisplaySelector_Jet__6B_, 7},
        {"??_7ElementLoader@Jet@@6B@", RsLocalVt___7ElementLoader_Jet__6B_, 2},
        {"??_7NonRenderContext@Jet@@6B@", RsLocalVt___7NonRenderContext_Jet__6B_, 5},
        {"??_7SharedContext@Jet@@6B@", RsLocalVt___7SharedContext_Jet__6B_, 2},
        {"??_7Material@Jet@@6B01@@", RsLocalVt___7Material_Jet__6B01__, 23},
        {"??_7Material@Jet@@6BPluginInstance@1@@", RsLocalVt___7Material_Jet__6BPluginInstance_1__, 2},
        {"??_7MaterialPlugin@Jet@@6B@", RsLocalVt___7MaterialPlugin_Jet__6B_, 6},
        {"??_7?$PluginFactoryBase@VMaterial@Jet@@@Jet@@6B@",
         RsLocalVt___7__PluginFactoryBase_VMaterial_Jet___Jet__6B_, 4},
        {"??_7MaterialConfig@Jet@@6B@", RsLocalVt___7MaterialConfig_Jet__6B_, 1},
        {"??_7NullMaterial@Jet@@6BMaterial@1@@", RsLocalVt___7NullMaterial_Jet__6BMaterial_1__, 23},
        {"??_7NullMaterial@Jet@@6BPluginInstance@1@@",
         RsLocalVt___7NullMaterial_Jet__6BPluginInstance_1__, 2},
    };
    for (const SlotMap& map : maps) {
        if (std::strcmp(name, map.name) != 0) continue;
        FillGenerated(table, map.symbols, map.slots, meta, name);
        void** const slots = reinterpret_cast<void**>(table);
        if (std::strcmp(name, "??_7Display@Jet@@6BPluginInstance@1@@") == 0) {
            slots[0] = reinterpret_cast<void*>(&RsDisplayPluginDeleteThis);
            slots[1] = reinterpret_cast<void*>(&RsDisplayPluginDeletingDtor);
        } else if (std::strcmp(name, "??_7DisplayContext@Display@Jet@@6B@") == 0) {
            slots[0] = reinterpret_cast<void*>(&RsDisplayContextDeletingDtor);
        } else if (std::strcmp(name, "??_7DisplaySelector@Jet@@6B@") == 0) {
            slots[1] = reinterpret_cast<void*>(&RsSelectorDeletingDtor);
        } else if (std::strcmp(name, "??_7DisplaySaveBuffer@Jet@@6B@") == 0) {
            slots[1] = reinterpret_cast<void*>(&RsSaveBufferDeletingDtor);
        } else if (std::strcmp(name, "??_7ElementLoader@Jet@@6B@") == 0) {
            slots[0] = reinterpret_cast<void*>(&RsElementLoaderDeletingDtor);
        } else if (std::strcmp(name, "??_7SharedContext@Jet@@6B@") == 0) {
            slots[1] = reinterpret_cast<void*>(&RsSharedContextDeletingDtor);
        } else if (std::strcmp(name, "??_7NonRenderContext@Jet@@6B@") == 0) {
            slots[1] = reinterpret_cast<void*>(&RsNonRenderContextDeletingDtor);
        } else if (std::strcmp(name, "??_7Material@Jet@@6BPluginInstance@1@@") == 0) {
            slots[0] = reinterpret_cast<void*>(&RsMaterialPluginDeleteThis);
            slots[1] = reinterpret_cast<void*>(&RsMaterialDeletingDtor);
        } else if (std::strcmp(name, "??_7NullMaterial@Jet@@6BPluginInstance@1@@") == 0) {
            slots[0] = reinterpret_cast<void*>(&RsNullMaterialPluginDeleteThis);
            slots[1] = reinterpret_cast<void*>(&RsNullMaterialDeletingDtor);
        } else if (std::strcmp(name, "??_7MaterialPlugin@Jet@@6B@") == 0) {
            slots[2] = reinterpret_cast<void*>(&RsFactoryBaseDeletingDtor);
        } else if (std::strcmp(name, "??_7?$PluginFactoryBase@VMaterial@Jet@@@Jet@@6B@") == 0) {
            slots[2] = reinterpret_cast<void*>(&RsFactoryBaseDeletingDtor);
        } else if (std::strcmp(name, "??_7MaterialConfig@Jet@@6B@") == 0) {
            slots[0] = reinterpret_cast<void*>(&RsMaterialConfigDeletingDtor);
        }
        return true;
    }
    if (std::strcmp(name, "??_8Display@Jet@@7B@") == 0) {
        // { primary offset 0, virtual-base offset; } the Display subobject follows +4.
        auto* const words = reinterpret_cast<int*>(table);
        words[0] = 0;
        words[1] = 0xf00;
        return true;
    }
    if (std::strcmp(name, "??_7RenderKernel@Jet@@6BTask@1@@") == 0) {
        FillRenderKernelTask(table, meta);
        return true;
    }
    if (std::strcmp(name, "??_7RenderKernel@Jet@@6BLayerManager@1@@") == 0) {
        FillRenderKernelLayer(table, meta);
        return true;
    }
    if (std::strcmp(name, "?__info@?1??Update@LayerManager@Jet@@QAAXM@Z@4UInfo@Profile@3@A") == 0) {
        FillProfileInfo(table, "LayerManager::Update",
                        "c:\\src\\trainz\\candidate\\ts12_sp1\\jet\\src\\jet\\rendersystem\\LayerManager.h");
        return true;
    }
    if (std::strcmp(name, "?__info@?1??Render@LayerManager@Jet@@QAAXXZ@4UInfo@Profile@3@A") == 0) {
        FillProfileInfo(table, "LayerManager::Render",
                        "c:\\src\\trainz\\candidate\\ts12_sp1\\jet\\src\\jet\\rendersystem\\LayerManager.h");
        return true;
    }
    if (std::strcmp(name, "?internalColorFormat@Display@Jet@@1W4DFormatId@2@A") == 0) {
        // Constant in the retail image: DFormatId 2, never assigned at run time.
        const unsigned format = 2;
        std::memcpy(table, &format, sizeof format);
        return true;
    }
    return false;
}

// The complete-object locator of one of the generated tables, for the units that build their own
// slot arrays (LayerLifetime, RenderKernel).  Null when the generated data has no such table.
extern "C" const void* RsLocalRttiLocator(const char* tableName)
{
    const LocalRttiSet& set = Rtti();
    for (unsigned i = 0; i < RS_LOCAL_RTTI_TABLE_COUNT; ++i) {
        if (std::strcmp(tableName, RsLocalRttiTableName[i]) == 0) return &set.locators[i];
    }
    return nullptr;
}

}  // namespace Jet
