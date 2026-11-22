// U7a's published factory vtables. The original module stays the owner of the shared statics; these arrays are
// filled from its exported tables at load, keep its RTTI locator in the word before slot 0, and replace only
// the virtuals that are local in this unit (the deleting destructor and IsSupported). CreateInstance stays the
// original slot until U7b.
#include <windows.h>

#include <cstddef>
#include <cstring>

#include "WaterMaterialInternal.h"

#pragma warning(disable : 4073)
#pragma init_seg(lib)

#pragma section("WsVtBump", read, write)
__declspec(allocate("WsVtBump")) void* WsBumpVftableMeta;
__declspec(allocate("WsVtBump")) void* WsBumpVftable[6];

#pragma section("WsVtFres", read, write)
__declspec(allocate("WsVtFres")) void* WsFresWaveVftableMeta;
__declspec(allocate("WsVtFres")) void* WsFresWaveVftable[6];

#pragma section("WsVtOne", read, write)
__declspec(allocate("WsVtOne")) void* WsOneTexVftableMeta;
__declspec(allocate("WsVtOne")) void* WsOneTexVftable[6];

#pragma section("WsVtOcean", read, write)
__declspec(allocate("WsVtOcean")) void* WsOceanVftableMeta;
__declspec(allocate("WsVtOcean")) void* WsOceanVftable[6];

namespace {

HMODULE OrigWs()
{
    return GetModuleHandleA("WaterSystem_7c_orig.dll");
}

void* OrigSym(const char* name)
{
    HMODULE const module = OrigWs();
    return module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
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

template <class Fn>
void* SlotAddress(Fn fn)
{
    union { Fn member; void* code; } u;
    u.member = fn;
    return u.code;
}

bool mirrorsReady;

void FillMirrors()
{
    if (mirrorsReady) return;
    CopyVftable(WsBumpVftable, &WsBumpVftableMeta, "??_7WaterBumpMaterialPlugin@Jet@@6B@", 6);
    CopyVftable(WsFresWaveVftable, &WsFresWaveVftableMeta, "??_7WaterMaterialFresWavePlugin@Jet@@6B@", 6);
    CopyVftable(WsOneTexVftable, &WsOneTexVftableMeta, "??_7WaterMaterialOneTexPlugin@Jet@@6B@", 6);
    CopyVftable(WsOceanVftable, &WsOceanVftableMeta, "??_7WaterOceanMaterialPlugin@Jet@@6B@", 6);

    WsBumpVftable[2] = SlotAddress(&WsVfThunks::BumpVectorDtor);
    // FresWave's lifetime stays the original's in this batch; only its IsSupported slot is local.
    WsOneTexVftable[2] = SlotAddress(&WsVfThunks::OneTexVectorDtor);
    WsOceanVftable[2] = SlotAddress(&WsVfThunks::OceanVectorDtor);
    WsBumpVftable[4] = WsOceanVftable[4] = SlotAddress(&WsVfThunks::SupportedByDisplay);
    WsFresWaveVftable[4] = WsOneTexVftable[4] = SlotAddress(&WsVfThunks::SupportedAlways);
    mirrorsReady = true;
}

struct MirrorStatics {
    MirrorStatics()
    {
        if (OrigWs()) FillMirrors();
    }
} mirrorStatics;

}  // namespace

void WsEnsureMirrors()
{
    if (mirrorsReady) return;
    if (!OrigWs()) LoadLibraryA("WaterSystem_7c_orig.dll");
    FillMirrors();
}
