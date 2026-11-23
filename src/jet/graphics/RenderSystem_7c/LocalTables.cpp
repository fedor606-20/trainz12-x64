// U16: local tables for the base objects our own code constructs. The exported vftables still mirror the
// retail ones when the renamed original is present; without it, the families we can build locally are filled
// here so a placeholder never carries a null vptr.
#include <jet/KernelStandard.h>
#include <jet/RenderSystem.h>
#include <windows.h>

#include <cstring>

namespace Jet {
void __cdecl RsSelectResolution(void*, const Options*, unsigned&, unsigned&);
void __cdecl RsSelectScreenMode(void*, const Options*, Window*, const void*, int&);
void __cdecl RsSelectWindowArea(void*, const Options*, Window*, const void*, Rect&);
bool __cdecl RsSelectVideoMode(void*, const Options*, const void*, void*);
bool __cdecl RsSelectVideoFormat(void*, const Options*, const void*, void*);
class RenderKernel;
class OS;
class DisplaySelector;
class LayerManager;
int __cdecl RsRenderKernelUpdate(RenderKernel*, float);
bool __cdecl RsRenderKernelInit(RenderKernel*, OS*, DisplaySelector*);
RenderKernel* __cdecl RsKernelFromLayerManager(void*);

namespace {

struct LocalTypeDescriptor {
    const void* vftable;
    void* spare;
    char name[40];
};

struct LocalCompleteObjectLocator {
    unsigned signature;
    unsigned offset;
    unsigned cdOffset;
    const void* typeDescriptor;
    const void* classDescriptor;
};

// VC8 32-bit RTTI. One type descriptor per class is enough for typeid; the hierarchy stays empty until a
// dynamic_cast needs it (the prototype hit heap corruption with a fuller synthetic hierarchy).
const LocalTypeDescriptor selectorType = {nullptr, nullptr, ".?AVDisplaySelector@Jet@@"};
const LocalCompleteObjectLocator selectorLocator = {0, 0, 0, &selectorType, nullptr};

void* KernelExport(const char* name)
{
    HMODULE const module = GetModuleHandleA("KernelStandard_7c.dll");
    return module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
}

// Our own dispatch goes through VCall (__cdecl with the object as the first stack argument), so the
// ported bodies are installed as-is. The base selector is only constructed and dispatched by this module;
// a renderer that supplies its own selector has its own derived vtable.

void* __fastcall SelectorDeletingDtor(void* self, void*, unsigned flags)
{
    if (flags & 1) Alloc::Delete(self);
    return self;
}

const LocalTypeDescriptor renderKernelType = {nullptr, nullptr, ".?AVRenderKernel@Jet@@"};
const LocalCompleteObjectLocator renderKernelLocator = {0, 0, 0, &renderKernelType, nullptr};

void FillRenderKernelTask(unsigned char* table, void** meta)
{
    HMODULE const kernel = GetModuleHandleA("Kernel_7c.dll");
    auto** const task = reinterpret_cast<void**>(GetProcAddress(kernel, "??_7Task@Jet@@6B@"));
    if (!task) return;
    auto** const slots = reinterpret_cast<void**>(table);
    for (int i = 0; i < 5; ++i) slots[i] = task[i];
    slots[4] = reinterpret_cast<void*>(&RsRenderKernelUpdate);
    slots[5] = reinterpret_cast<void*>(&RsRenderKernelInit);
    void* const locator = const_cast<LocalCompleteObjectLocator*>(&renderKernelLocator);
    *meta = locator;
    std::memcpy(table - 4, &locator, sizeof locator);
}

void FillRenderKernelLayer(unsigned char* table, void** meta)
{
    auto** const slots = reinterpret_cast<void**>(table);
    // The deleting destructor is not reachable in our code; the kernel calls RsLayerManagerDrop directly.
    slots[0] = nullptr;
    slots[1] = reinterpret_cast<void*>(&RsKernelFromLayerManager);
    void* const locator = const_cast<LocalCompleteObjectLocator*>(&renderKernelLocator);
    *meta = locator;
    std::memcpy(table - 4, &locator, sizeof locator);
}

void FillProfileInfo(unsigned char* table, const char* name, const char* file)
{
    struct Entry { const char* name; const char* file; unsigned count, time; int id; unsigned mask, peek, unknown; };
    const Entry value{name, file, 0, 0, -1, 0x80000000u, 0, 0};
    std::memcpy(table, &value, sizeof value);
}

void FillSelector(unsigned char* table, void** meta)
{
    void* const deleteThis = KernelExport("?DeleteThis@ReferenceCount@Jet@@MBAXXZ");
    if (!deleteThis) return;
    void** const slots = reinterpret_cast<void**>(table);
    slots[0] = deleteThis;
    slots[1] = reinterpret_cast<void*>(&SelectorDeletingDtor);
    slots[2] = reinterpret_cast<void*>(&RsSelectScreenMode);
    slots[3] = reinterpret_cast<void*>(&RsSelectResolution);
    slots[4] = reinterpret_cast<void*>(&RsSelectWindowArea);
    slots[5] = reinterpret_cast<void*>(&RsSelectVideoMode);
    slots[6] = reinterpret_cast<void*>(&RsSelectVideoFormat);
    void* const locator = const_cast<LocalCompleteObjectLocator*>(&selectorLocator);
    *meta = locator;
    std::memcpy(table - 4, &locator, sizeof locator);
}

}  // namespace

// Returns true when this export has a local definition, so the mirror keeps it instead of zeroes.
extern "C" bool RsFillLocalVtable(unsigned char* table, void** meta, const char* name)
{
    if (std::strcmp(name, "??_7RenderKernel@Jet@@6BTask@1@@") == 0) {
        FillRenderKernelTask(table, meta);
        return true;
    }
    if (std::strcmp(name, "??_7RenderKernel@Jet@@6BLayerManager@1@@") == 0) {
        FillRenderKernelLayer(table, meta);
        return true;
    }
    if (std::strcmp(name, "??_7DisplaySelector@Jet@@6B@") == 0) {
        FillSelector(table, meta);
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

}  // namespace Jet
