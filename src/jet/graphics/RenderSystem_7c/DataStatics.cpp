// U11: local storage for every remaining data export. The original DLL still owns the live objects;
// these byte-for-byte mirrors are filled at load. Owned writers call RsMirrorSync after changing a
// mutable static; trainz.exe resolves six of these names at runtime, so a stale snapshot would be visible.
#include <windows.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "DataMirrorList.h"

#pragma warning(disable : 4073)
#pragma init_seg(lib)

namespace Jet {
class RenderKernel;
int __cdecl RsRenderKernelUpdate(RenderKernel*, float);
}

#define X(var, name, size) alignas(4) unsigned char var[size];
RS_PLAIN_DATA(X)
#undef X

#pragma section("RsDsV01", read, write)
__declspec(allocate("RsDsV01")) __declspec(align(4)) void* RsRsV001Meta;
__declspec(allocate("RsDsV01")) __declspec(align(4)) unsigned char RsRsV001[80 * 4];

#pragma section("RsDsV02", read, write)
__declspec(allocate("RsDsV02")) __declspec(align(4)) void* RsRsV002Meta;
__declspec(allocate("RsDsV02")) __declspec(align(4)) unsigned char RsRsV002[2 * 4];

#pragma section("RsDsV03", read, write)
__declspec(allocate("RsDsV03")) __declspec(align(4)) void* RsRsV003Meta;
__declspec(allocate("RsDsV03")) __declspec(align(4)) unsigned char RsRsV003[1 * 4];

#pragma section("RsDsV04", read, write)
__declspec(allocate("RsDsV04")) __declspec(align(4)) void* RsRsV004Meta;
__declspec(allocate("RsDsV04")) __declspec(align(4)) unsigned char RsRsV004[4 * 4];

#pragma section("RsDsV05", read, write)
__declspec(allocate("RsDsV05")) __declspec(align(4)) void* RsRsV005Meta;
__declspec(allocate("RsDsV05")) __declspec(align(4)) unsigned char RsRsV005[7 * 4];

#pragma section("RsDsV06", read, write)
__declspec(allocate("RsDsV06")) __declspec(align(4)) void* RsRsV006Meta;
__declspec(allocate("RsDsV06")) __declspec(align(4)) unsigned char RsRsV006[2 * 4];

#pragma section("RsDsV07", read, write)
__declspec(allocate("RsDsV07")) __declspec(align(4)) void* RsRsV007Meta;
__declspec(allocate("RsDsV07")) __declspec(align(4)) unsigned char RsRsV007[5 * 4];

#pragma section("RsDsV08", read, write)
__declspec(allocate("RsDsV08")) __declspec(align(4)) void* RsRsV008Meta;
__declspec(allocate("RsDsV08")) __declspec(align(4)) unsigned char RsRsV008[2 * 4];

#pragma section("RsDsV09", read, write)
__declspec(allocate("RsDsV09")) __declspec(align(4)) void* RsRsV009Meta;
__declspec(allocate("RsDsV09")) __declspec(align(4)) unsigned char RsRsV009[6 * 4];

#pragma section("RsDsV10", read, write)
__declspec(allocate("RsDsV10")) __declspec(align(4)) void* RsRsV010Meta;
__declspec(allocate("RsDsV10")) __declspec(align(4)) unsigned char RsRsV010[2 * 4];

// The exported table starts one word after its locator. C++ forbids a constant-expression cast between
// the separate globals, so check the layout rule here and the actual addresses when the mirrors load.
struct VTableAdjacency { void* meta; unsigned char table[4]; };
static_assert(offsetof(VTableAdjacency, table) == sizeof(void*), "RTTI locator must precede a vtable");

alignas(4) unsigned char RsRsVb001[2 * 4];

namespace {

extern "C" bool RsFillLocalVtable(unsigned char* table, void** meta, const char* name);

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

struct MirrorEntry {
    unsigned char* local;
    const char* name;
    std::size_t size;
    void** meta;
};

void WireLocalSlots(unsigned char* table)
{
    if (table == RsRsV009) {
        const auto update = &Jet::RsRenderKernelUpdate;
        std::memcpy(table + 0x10, &update, sizeof update);
    }
}

MirrorEntry entries[] = {
#define X(var, name, size) { var, name, size, nullptr },
RS_PLAIN_DATA(X)
#undef X
#define X(var, name, slots) { var, name, slots * 4, &var##Meta },
RS_VFTABLE_DATA(X)
#undef X
#define X(var, name, words) { reinterpret_cast<unsigned char*>(var), name, words * 4, nullptr },
RS_VBTABLE_DATA(X)
#undef X
};

struct MirrorAll {
    MirrorAll()
    {
        for (const MirrorEntry& e : entries) {
            void* const source = OrigSym(e.name);
            if (!source) {
                RsFillLocalVtable(e.local, e.meta, e.name);
                continue;
            }
            if (e.meta && reinterpret_cast<unsigned char*>(e.meta + 1) != e.local) std::abort();
            std::memcpy(e.local, source, e.size);
            if (e.meta) {
                void* const locator = *reinterpret_cast<void**>(static_cast<unsigned char*>(source) - 4);
                *e.meta = locator;
                std::memcpy(e.local - 4, &locator, sizeof locator);
            }
            WireLocalSlots(e.local);
        }
    }
} mirrorAll;

}  // namespace

// Called by the ported writers after they change the original's live static. A data export is a snapshot, not
// an alias, so the mirror must be refreshed explicitly whenever the owner writes. Only mutable statics need it.
extern "C" void RsMirrorSync(const char* name)
{
    for (const MirrorEntry& e : entries) {
        if (std::strcmp(e.name, name) != 0) continue;
        void* const source = OrigSym(name);
        if (!source) return;
        if (e.meta) {
            void* const locator = *reinterpret_cast<void**>(static_cast<unsigned char*>(source) - 4);
            *e.meta = locator;
            std::memcpy(e.local - 4, &locator, sizeof locator);
        }
        std::memcpy(e.local, source, e.size);
        WireLocalSlots(e.local);
        return;
    }
}
