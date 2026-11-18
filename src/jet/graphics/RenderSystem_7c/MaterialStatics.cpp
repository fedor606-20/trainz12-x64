// U7's shared module statics and vftables. Until U9 the original DLL remains the owner of the objects: the
// mirrors below are filled from it at load and uniqueMatId is still advanced there first. They only exist so
// that the data exports belong to this module (the game keeps importing the same names and sees the same bytes).
// A few vtable slots are replaced by our exported functions; everything else keeps the original's slot value.
//
// Two details that matter to code compiled in trainz.exe / the other DLLs:
//   * an MSVC vftable has the RTTI complete-object locator one pointer BEFORE its first virtual slot;
//     __RTDynamicCast (and C++ exception handling) reads vptr[-1]. Every mirror therefore owns a private
//     word in front of its exported array, filled with the original's locator.
//   * the two replaced NullMaterial virtuals are exported as __cdecl free functions (that is the ABI the
//     differential test calls), but a vftable entry is __thiscall. The slots point to tiny member-function
//     thunks which forward 'this' from ECX to those exports.
#include <windows.h>

#include <cstddef>
#include <cstring>

// this initializer must run before any other unit in this DLL can publish one of these pointers
#pragma warning(disable : 4073)  // initializers put in library initialization area
#pragma init_seg(lib)

void* __cdecl RsU7NullCreateInstance(const void*);
bool __cdecl RsU7NullEquals(const void*, const void*);

// exported data symbols (aliased to the original mangled names in implemented.txt); declared before the
// initialiser below so its member bodies can see them. Their 4-byte storage mirrors the original statics,
// which are all pointers (PString*, UId, ResultTable*) or a 32-bit counter.

alignas(4) unsigned char RsMatDecoratedTypeToken[4];
alignas(4) unsigned char RsMatStringNotFound[4];
alignas(4) unsigned char RsMatPluginGroupName[4];
alignas(4) unsigned char RsMatPluginGroupUid[16];
alignas(4) unsigned char RsMaterialResults[4];            // ResultTable const* const
alignas(4) unsigned char RsMaterialManagerResults[4];     // ResultTable const* const
alignas(4) unsigned char RsMatUniqueMatId[4];

// One dedicated section per vftable: __declspec(allocate) packs variables of different sizes in declaration
// order only inside their own section, so the four bytes in front of each exported array are guaranteed to
// be ours (either 'Meta' or the alignment padding in front of an eight-byte-aligned 23-slot table). CopyVftable
// writes the locator to table[-1] -- the word the game actually reads -- and keeps 'Meta' referenced.
#pragma section("RsVtMat", read, write)
__declspec(allocate("RsVtMat")) void* RsMatVftableMaterialMeta;
__declspec(allocate("RsVtMat")) void* RsMatVftableMaterial[23];

#pragma section("RsVtMatI", read, write)
__declspec(allocate("RsVtMatI")) void* RsMatVftableMaterialInstanceMeta;
__declspec(allocate("RsVtMatI")) void* RsMatVftableMaterialInstance[2];

int RsMatVbtableMaterial[2];        // {0, 0x9c}

#pragma section("RsVtMP", read, write)
__declspec(allocate("RsVtMP")) void* RsMatVftableMaterialPluginMeta;
__declspec(allocate("RsVtMP")) void* RsMatVftableMaterialPlugin[6];

// the original's vftable at 0x100710ac has FOUR slots (0x1005729a, 0x100572a0, 0x10032480, 0x1001dec0);
// the differential test only dumps the first three, which is why the older mirror looked complete.
#pragma section("RsVtFac", read, write)
__declspec(allocate("RsVtFac")) void* RsMatVftablePluginFactoryMeta;
__declspec(allocate("RsVtFac")) void* RsMatVftablePluginFactory[4];

#pragma section("RsVtCfg", read, write)
__declspec(allocate("RsVtCfg")) void* RsMatVftableMaterialConfigMeta;
__declspec(allocate("RsVtCfg")) void* RsMatVftableMaterialConfig[1];

#pragma section("RsVtNull", read, write)
__declspec(allocate("RsVtNull")) void* RsMatVftableNullMaterialMeta;
__declspec(allocate("RsVtNull")) void* RsMatVftableNullMaterial[23];

#pragma section("RsVtNullI", read, write)
__declspec(allocate("RsVtNullI")) void* RsMatVftableNullMaterialInstanceMeta;
__declspec(allocate("RsVtNullI")) void* RsMatVftableNullMaterialInstance[2];

int RsMatVbtableNullMaterial[2];    // {0, 0xa0}

namespace {

HMODULE OrigRs()
{
    static HMODULE const module = GetModuleHandleA("RenderSystem_7c_orig.dll");
    return module;
}

void* OrigSym(const char* name)
{
    HMODULE const module = OrigRs();
    return module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
}

void CopyStatic(void* destination, const char* name, std::size_t size)
{
    if (void* source = OrigSym(name)) std::memcpy(destination, source, size);
}

void CopyVftable(void* table, void* meta, const char* name, std::size_t slots)
{
    void* const source = OrigSym(name);
    if (!source) return;

    auto* const src = static_cast<unsigned char*>(source);
    auto* const dst = static_cast<unsigned char*>(table);
    std::memcpy(dst, src, slots * sizeof(void*));

    void* const locator = *reinterpret_cast<void**>(src - sizeof(void*));
    *static_cast<void**>(meta) = locator;
    std::memcpy(dst - sizeof(void*), &locator, sizeof locator);
}

// A vftable entry has the __thiscall signature of the original virtual (this in ECX), while the exported
// functions above are __cdecl (this on the stack). MSVC emits these members as plain thiscall code addresses,
// so no inline assembler is needed and the x64 harness keeps working too.
struct NullVfThunks {
    void* CreateInstance();
    bool Equals(const void* other);
};

void* NullVfThunks::CreateInstance() { return RsU7NullCreateInstance(this); }
bool NullVfThunks::Equals(const void* other) { return RsU7NullEquals(this, other); }

template <class Fn>
void* SlotAddress(Fn fn)
{
    union { Fn member; void* code; } u;
    u.member = fn;
    return u.code;
}

// one initialiser builds every exported data object; it runs at module load, before any exported function can
// hand one of the pointers to the game
struct MirrorStatics {
    MirrorStatics()
    {
        CopyStatic(RsMatDecoratedTypeToken, "?DecoratedTypeToken@Material@Jet@@2VPString@2@A", 4);
        CopyStatic(RsMatStringNotFound, "?STRING_NOT_FOUND@Material@Jet@@2VPString@2@A", 4);
        CopyStatic(RsMatPluginGroupName, "?PluginGroupName@MaterialPlugin@Jet@@2VPString@2@A", 4);
        CopyStatic(RsMatPluginGroupUid, "?PluginGroupUID@MaterialPlugin@Jet@@2VUId@2@A", 16);
        CopyStatic(RsMaterialResults, "?results@Material@Jet@@2QBUResultTable@2@B", 4);
        CopyStatic(RsMaterialManagerResults, "?results@MaterialManager@Jet@@2QBUResultTable@2@B", 4);
        CopyStatic(RsMatUniqueMatId, "?uniqueMatId@Material@Jet@@0KA", 4);

        CopyVftable(RsMatVftableMaterial, &RsMatVftableMaterialMeta, "??_7Material@Jet@@6B01@@",
                    sizeof RsMatVftableMaterial / sizeof RsMatVftableMaterial[0]);
        CopyVftable(RsMatVftableMaterialInstance, &RsMatVftableMaterialInstanceMeta,
                    "??_7Material@Jet@@6BPluginInstance@1@@",
                    sizeof RsMatVftableMaterialInstance / sizeof RsMatVftableMaterialInstance[0]);
        CopyStatic(RsMatVbtableMaterial, "??_8Material@Jet@@7B@", sizeof RsMatVbtableMaterial);
        CopyVftable(RsMatVftableMaterialPlugin, &RsMatVftableMaterialPluginMeta,
                    "??_7MaterialPlugin@Jet@@6B@",
                    sizeof RsMatVftableMaterialPlugin / sizeof RsMatVftableMaterialPlugin[0]);
        CopyVftable(RsMatVftablePluginFactory, &RsMatVftablePluginFactoryMeta,
                    "??_7?$PluginFactoryBase@VMaterial@Jet@@@Jet@@6B@",
                    sizeof RsMatVftablePluginFactory / sizeof RsMatVftablePluginFactory[0]);
        CopyVftable(RsMatVftableMaterialConfig, &RsMatVftableMaterialConfigMeta,
                    "??_7MaterialConfig@Jet@@6B@",
                    sizeof RsMatVftableMaterialConfig / sizeof RsMatVftableMaterialConfig[0]);
        CopyVftable(RsMatVftableNullMaterial, &RsMatVftableNullMaterialMeta,
                    "??_7NullMaterial@Jet@@6BMaterial@1@@",
                    sizeof RsMatVftableNullMaterial / sizeof RsMatVftableNullMaterial[0]);
        CopyVftable(RsMatVftableNullMaterialInstance, &RsMatVftableNullMaterialInstanceMeta,
                    "??_7NullMaterial@Jet@@6BPluginInstance@1@@",
                    sizeof RsMatVftableNullMaterialInstance / sizeof RsMatVftableNullMaterialInstance[0]);
        CopyStatic(RsMatVbtableNullMaterial, "??_8NullMaterial@Jet@@7B@", sizeof RsMatVbtableNullMaterial);

        // the two NullMaterial virtuals of this unit now resolve into our code; compare_vtables still sees local
        // slots on both sides, and the rest of both tables is byte-for-byte the original's
        RsMatVftableNullMaterial[0] = SlotAddress(&NullVfThunks::CreateInstance);
        RsMatVftableNullMaterial[1] = SlotAddress(&NullVfThunks::Equals);
    }
} mirrorStatics;

}  // namespace

// the original's Material::InitInstance (0x100331b0) reads uniqueMatId and then increments it; NullMaterial's
// twin at 0x10037a20 does the same. Our replacement instance code therefore advances the SAME counter in the
// original module and keeps the exported 4-byte mirror in step.
extern "C" unsigned long RsMatNextUniqueId()
{
    auto* const counter = static_cast<unsigned long*>(OrigSym("?uniqueMatId@Material@Jet@@0KA"));
    if (!counter) return 0;
    const unsigned long id = static_cast<unsigned long>(
        InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(counter), 1));
    InterlockedExchange(reinterpret_cast<volatile LONG*>(RsMatUniqueMatId), static_cast<LONG>(id + 1));
    return id;
}
