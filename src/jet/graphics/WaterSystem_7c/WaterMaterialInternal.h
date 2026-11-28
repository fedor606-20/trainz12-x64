// U7 internals: the material factories' raw RenderSystem base and the mirrored vtables they publish.
#pragma once

#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/UId.h>

#include <cstring>

namespace Jet {
class Display;
class Material;
class MaterialManager;
class RenderKernel;
}

// RenderSystem_7c's MaterialPlugin: vptr, then PluginType's 0x1c bytes. Imported by name; no local vtable.
class WsMaterialPluginAbi {
public:
    void __thiscall Ctor(const Jet::PString& name, const Jet::UId& id);
    void __thiscall Copy(const WsMaterialPluginAbi& other);
    WsMaterialPluginAbi& __thiscall Assign(const WsMaterialPluginAbi& other);
    void __thiscall Dtor();

    void* vptr;          // +0x00
    Jet::UId id;         // +0x04
    Jet::PString name;   // +0x14
    void* lib;           // +0x18
    void* parent;        // +0x1c
};
static_assert(sizeof(WsMaterialPluginAbi) == 0x20);

// filled from RenderSystem_7c.dll's import table; each is the address of the imported function
extern "C" void* WsMatPluginCtorImport;
extern "C" void* WsMatPluginCopyImport;
extern "C" void* WsMatPluginAssignImport;
extern "C" void* WsMatPluginDtorImport;
extern "C" void* WsAllocNewImport;
extern "C" void* WsAllocDeleteImport;
#pragma comment(linker, "/alternatename:_WsMatPluginCtorImport=__imp_??0MaterialPlugin@Jet@@QAE@ABVPString@1@ABVUId@1@@Z")
#pragma comment(linker, "/alternatename:_WsMatPluginCopyImport=__imp_??0MaterialPlugin@Jet@@QAE@ABV01@@Z")
#pragma comment(linker, "/alternatename:_WsMatPluginAssignImport=__imp_??4MaterialPlugin@Jet@@QAEAAV01@ABV01@@Z")
#pragma comment(linker, "/alternatename:_WsMatPluginDtorImport=__imp_??1MaterialPlugin@Jet@@UAE@XZ")
#pragma comment(linker, "/alternatename:_WsAllocNewImport=__imp_?New@Alloc@Jet@@SAPAXI@Z")
#pragma comment(linker, "/alternatename:_WsAllocDeleteImport=__imp_?Delete@Alloc@Jet@@SAXPAX@Z")

template <class Fn>
static Fn WsBind(void* address)
{
    static_assert(sizeof(Fn) == sizeof(void*), "single-inheritance member function pointer must be one slot");
    Fn fn;
    std::memcpy(&fn, &address, sizeof fn);
    return fn;
}

// 0x10005180: the module's Matrix4 product (a * b, stored in `out`). The original adds each output's
// products in the compiler's order ((a[r+1]*b[4+j] + a[r+3]*b[12+j]) + a[r+2]*b[8+j]) + a[r]*b[j],
// so the single-precision rounding matches the retail build's.
inline void WsMultiplyMatrix(Jet::Matrix4& out, const Jet::Matrix4& a, const Jet::Matrix4& b)
{
    Jet::Matrix4 result{};
    for (int row = 0; row < 4; ++row) {
        const int r = row * 4;
        for (int col = 0; col < 4; ++col) {
            float value = a.m[r + 1] * b.m[4 + col] + a.m[r + 3] * b.m[12 + col];
            value += a.m[r + 2] * b.m[8 + col];
            value += a.m[r] * b.m[col];
            result.m[r + col] = value;
        }
    }
    out = result;
}

// fills the mirrors on first use when the original DLL was loaded after this one
void WsEnsureMirrors();

// install the original-order vtable on a freshly constructed material (kind 0 base, 1 bump, 2 ocean)
void WsInstallMaterialVtables(void* object, int kind);

// set the factories' CreateInstance slot to the local implementations (Bump/Ocean)
void WsInstallFactoryCreate();

// the four published factory vtables; their first, second and fourth slots stay the original's import thunks
extern void* WsBumpVftable[6];
extern void* WsFresWaveVftable[6];
extern void* WsOneTexVftable[6];
extern void* WsOceanVftable[6];

// vtable entries: __thiscall for slot 2, __cdecl for slot 4 (the exported ABI, not a local wrapper)
struct WsVfThunks {
    void* __thiscall BumpVectorDtor(unsigned flags);
    void* __thiscall OneTexVectorDtor(unsigned flags);
    void* __thiscall OceanVectorDtor(unsigned flags);
    bool __cdecl SupportedByDisplay(Jet::Display* display, Jet::PString& fallback);
    bool __cdecl SupportedAlways(Jet::Display* display, Jet::PString& fallback);
};
