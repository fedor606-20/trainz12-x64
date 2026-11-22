// Build from an x86 MSVC prompt: cl /W4 /MT /EHsc tni_allocator_probe.cpp /link /manifest:embed
// Place the game's private Microsoft.VC80.CRT assembly beside the test executable.
// Usage: tni_allocator_probe.exe <absolute DLL path> <original|patched>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.VC80.CRT' " \
    "version='8.0.50727.6195' processorArchitecture='x86' publicKeyToken='1fc8b3b9a1e18e3b'\"")

namespace {
using AllocFn = void* (__cdecl*)(size_t);
using FreeFn = void (__cdecl*)(void*);
AllocFn crtAlloc;
FreeFn crtFree;
void* sentinel;
unsigned allocCalls, freeCalls, sentinelFrees;

void* __cdecl ObserveAlloc(size_t size)
{
    ++allocCalls;
    return crtAlloc(size);
}

void __cdecl ObserveFree(void* ptr)
{
    ++freeCalls;
    if (ptr == sentinel)
        ++sentinelFrees;
    crtFree(ptr);
}
}

int main(int argc, char** argv)
{
    static_assert(sizeof(void*) == 4);
    if (argc != 3 || (std::strcmp(argv[2], "original") && std::strcmp(argv[2], "patched"))) {
        std::puts("Usage: tni_allocator_probe.exe <absolute DLL path> <original|patched>");
        return 2;
    }
    // Block the preferred image base to exercise the fix under relocation.
    auto* preferredBase = reinterpret_cast<void*>(0x10000000u);
    void* reservation = VirtualAlloc(preferredBase, 0x1000, MEM_RESERVE, PAGE_NOACCESS);
    const HMODULE dll = LoadLibraryA(argv[1]);
    if (!dll) {
        std::printf("TNI load failed: %lu\n", GetLastError());
        return 2;
    }
    if (reinterpret_cast<void*>(dll) == preferredBase) {
        std::puts("TNI was not relocated.");
        return 2;
    }
    const HMODULE crt = GetModuleHandleA("msvcr80.dll");
    if (!crt)
        return 2;
    const auto setAllocator = reinterpret_cast<void (__cdecl*)(AllocFn, FreeFn)>(
        GetProcAddress(dll, "TNISetAllocator"));
    const auto tniAlloc = reinterpret_cast<void* (__cdecl*)(size_t, unsigned)>(
        GetProcAddress(dll, "TNIAllocMem"));
    const auto tniFree = reinterpret_cast<FreeFn>(GetProcAddress(dll, "TNIFreeMem"));
    crtAlloc = reinterpret_cast<AllocFn>(GetProcAddress(crt, "malloc"));
    crtFree = reinterpret_cast<FreeFn>(GetProcAddress(crt, "free"));
    const auto msize = reinterpret_cast<size_t (__cdecl*)(void*)>(GetProcAddress(crt, "_msize"));
    if (!setAllocator || !tniAlloc || !tniFree || !crtAlloc || !crtFree || !msize)
        return 2;

    // The RVAs are specific to the pristine DLL pinned by tools/fix_tni_shutdown.py.
    const auto base = reinterpret_cast<const unsigned char*>(dll);
    sentinel = *reinterpret_cast<void* const*>(base + 0x12204);
    if (!sentinel || *reinterpret_cast<const DWORD*>(base + 0x12210)
                  || *reinterpret_cast<const DWORD*>(base + 0x12214))
        return 2;
    const auto bytes = msize(sentinel);
    setAllocator(ObserveAlloc, ObserveFree);
    if (tniAlloc(0, 0) != nullptr || allocCalls != 0)
        return 2;
    void* other = tniAlloc(32, 0);
    if (!other || other == sentinel)
        return 2;
    tniFree(other);
    if (!FreeLibrary(dll))
        return 2;
    if (reservation && !VirtualFree(reservation, 0, MEM_RELEASE))
        return 2;

    std::printf("Relocated DLL: CRT sentinel size %zu; callback alloc/free %u/%u; sentinel callback frees %u\n",
                bytes, allocCalls, freeCalls, sentinelFrees);
    const bool expectPatched = std::strcmp(argv[2], "patched") == 0;
    return bytes == 24 && allocCalls == 1 && freeCalls == (expectPatched ? 1u : 2u)
           && sentinelFrees == (expectPatched ? 0u : 1u) ? 0 : 1;
}
