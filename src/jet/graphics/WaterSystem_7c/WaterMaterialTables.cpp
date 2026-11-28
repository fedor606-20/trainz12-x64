// U7b/U8a: local vtables in the original slot order. VS2026 places a new overload of an inherited virtual at
// the end of the base table (the TS12 compiler kept the declaration position), so the compiler tables of
// WaterMaterial/Bump/Ocean are rotated back by one for slots 23..46 before use and before export.
#include <windows.h>

#include <cstddef>
#include <cstring>

#include "WaterMaterialInternal.h"

#pragma warning(disable : 4073)

#pragma section("WsVtWM", read, write)
__declspec(allocate("WsVtWM")) void* WsWaterMaterialVtMeta;
__declspec(allocate("WsVtWM")) void* WsWaterMaterialVtFixed[48];

#pragma section("WsVtBumpM", read, write)
__declspec(allocate("WsVtBumpM")) void* WsBumpMaterialVtMeta;
__declspec(allocate("WsVtBumpM")) void* WsBumpMaterialVtFixed[52];

#pragma section("WsVtOceanM", read, write)
__declspec(allocate("WsVtOceanM")) void* WsOceanMaterialVtMeta;
__declspec(allocate("WsVtOceanM")) void* WsOceanMaterialVtFixed[52];

// the compiler's own tables (linked because the constructors write their vptr)
extern "C" void* WsWaterCompilerVt[];
extern "C" void* WsBumpCompilerVt[];
extern "C" void* WsOceanCompilerVt[];
#pragma comment(linker, "/include:??_7WaterMaterial@Jet@@6BMaterial@1@@")
#pragma comment(linker, "/include:??_7WaterBumpMaterial@Jet@@6BMaterial@1@@")
#pragma comment(linker, "/include:??_7WaterOceanMaterial@Jet@@6BMaterial@1@@")
#pragma comment(linker, "/alternatename:_WsWaterCompilerVt=??_7WaterMaterial@Jet@@6BMaterial@1@@")
#pragma comment(linker, "/alternatename:_WsBumpCompilerVt=??_7WaterBumpMaterial@Jet@@6BMaterial@1@@")
#pragma comment(linker, "/alternatename:_WsOceanCompilerVt=??_7WaterOceanMaterial@Jet@@6BMaterial@1@@")

namespace {
void RotateTable(void** destination, void** source, unsigned count, unsigned first, unsigned last)
{
    for (unsigned i = 0; i < count; ++i) destination[i] = source[i];
    for (unsigned i = first; i < last; ++i) destination[i] = source[i + 1];
    destination[last] = source[first];
}
}  // namespace

// Called from the constructors. The base ctor only records the compiler table (it must leave the vptr alone so
// the most-derived ctor can still see its own); the concrete ctors install their corrected table.
void WsInstallMaterialVtables(void* object, int kind)
{
    auto** const current = *static_cast<void***>(object);
    if (kind == 0) {
        if (!WsWaterMaterialVtFixed[0]) {
            WsWaterMaterialVtMeta = current[-1];
            RotateTable(WsWaterMaterialVtFixed, current, 48, 23, 46);
        }
    } else if (kind == 1) {
        if (!WsBumpMaterialVtFixed[0]) {
            WsBumpMaterialVtMeta = current[-1];
            RotateTable(WsBumpMaterialVtFixed, current, 52, 23, 46);
        }
        *static_cast<void***>(object) = WsBumpMaterialVtFixed;
    } else {
        if (!WsOceanMaterialVtFixed[0]) {
            WsOceanMaterialVtMeta = current[-1];
            RotateTable(WsOceanMaterialVtFixed, current, 52, 23, 46);
        }
        *static_cast<void***>(object) = WsOceanMaterialVtFixed;
    }
}

namespace {
struct VtablesAtLoad {
    VtablesAtLoad()
    {
        WsWaterMaterialVtMeta = WsWaterCompilerVt[-1];
        RotateTable(WsWaterMaterialVtFixed, WsWaterCompilerVt, 48, 23, 46);
        WsBumpMaterialVtMeta = WsBumpCompilerVt[-1];
        RotateTable(WsBumpMaterialVtFixed, WsBumpCompilerVt, 52, 23, 46);
        WsOceanMaterialVtMeta = WsOceanCompilerVt[-1];
        RotateTable(WsOceanMaterialVtFixed, WsOceanCompilerVt, 52, 23, 46);
    }
} vtablesAtLoad;
}  // namespace
