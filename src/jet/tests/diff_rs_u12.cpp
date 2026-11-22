// Usage: diff_rs_u12.exe <bin dir>. Compare JetGetPluginsRenderSystem's factory registrations in Win32.
// Imports of UId, Alloc, PStringCache and ResourceTypeBase are patched in both modules, so the test records
// the exact registration sequence without depending on global plugin state or leaking factories.
#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using Trace = std::vector<int>;
static Trace* trace;
static unsigned char objects[10][0x40];
static unsigned objectCount;
static unsigned char nodes[10][0x18];
static unsigned nodeCount;

static void PatchImport(HMODULE m, const char* name, void* fn)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    unsigned patched = 0;
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            if (std::strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData)->Name, name)) continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            iat->u1.Function = reinterpret_cast<uintptr_t>(fn);
            VirtualProtect(&iat->u1.Function, sizeof(void*), old, &old);
            ++patched;
        }
    }
    if (!patched) {
        std::printf("import not found: %s\n", name);
        std::exit(2);
    }
}

static void Text(const char* s)
{
    trace->push_back(0x0100);
    for (; *s; ++s) trace->push_back(static_cast<unsigned char>(*s));
    trace->push_back(0);
}

static void* __stdcall FakeUId(const char* text)
{
    void* self;
    __asm { mov self, ecx }  // the original calls this import with __thiscall
    auto* bytes = static_cast<unsigned char*>(self);
    const size_t length = std::strlen(text);
    for (size_t i = 0; i < 16; ++i) bytes[i] = i < length ? static_cast<unsigned char>(text[i]) : 0;
    trace->push_back(1);
    Text(text);
    return self;
}

static void* __cdecl FakeNew(unsigned size)
{
    if (objectCount >= 10 || size > sizeof objects[0]) std::exit(2);
    trace->insert(trace->end(), {2, static_cast<int>(size), static_cast<int>(objectCount)});
    return objects[objectCount++];
}

static void* __cdecl FakeCreate(void* cache, const unsigned char* text)
{
    if (nodeCount >= 10) std::exit(2);
    trace->insert(trace->end(), {3, static_cast<int>(!cache), static_cast<int>(nodeCount)});
    Text(reinterpret_cast<const char*>(text));
    return nodes[nodeCount++];
}

static void* __stdcall FakeTypeCtor(const void* ext, const unsigned char* id, const unsigned char* parent)
{
    void* self;
    __asm { mov self, ecx }  // the original calls this import with __thiscall
    trace->insert(trace->end(), {4, -1});
    for (unsigned i = 0; i < nodeCount; ++i)
        if (ext == nodes[i]) trace->back() = static_cast<int>(i);
    for (int i = 0; i < 4; ++i) trace->push_back(static_cast<int>(reinterpret_cast<const uint32_t*>(id)[i]));
    for (int i = 0; i < 4; ++i) trace->push_back(static_cast<int>(reinterpret_cast<const uint32_t*>(parent)[i]));
    return self;
}

static void __cdecl FakeDestroy(void* cache, void* node)
{
    trace->insert(trace->end(), {5, static_cast<int>(!cache), -1});
    for (unsigned i = 0; i < nodeCount; ++i)
        if (node == nodes[i]) trace->back() = static_cast<int>(i);
}

static const std::array<int, 10> kSlotCounts{7, 7, 7, 7, 7, 6, 6, 6, 6, 6};

static void DumpVtables()
{
    for (unsigned i = 0; i < objectCount; ++i) {
        const auto* vtable = static_cast<const unsigned*>(*reinterpret_cast<void**>(objects[i]));
        trace->insert(trace->end(), {6, static_cast<int>(i)});
        for (int j = -1; j < kSlotCounts[i]; ++j)
            trace->push_back(static_cast<int>(vtable[j]));
    }
}

static Trace Run(HMODULE module, void (*plugins)())
{
    trace->clear();
    objectCount = nodeCount = 0;
    plugins();
    DumpVtables();
    return *trace;
}

int main(int argc, char** argv)
{
    if (argc != 2 || !SetDllDirectoryA(argv[1])) {
        std::puts("usage: diff_rs_u12.exe <bin dir>");
        return 2;
    }
    const HMODULE original = LoadLibraryA("RenderSystem_7c_orig.dll");
    const HMODULE rebuilt = LoadLibraryA("RenderSystem_7c.dll");
    if (!original || !rebuilt) {
        std::printf("cannot load RenderSystem DLLs: %lu\n", GetLastError());
        return 2;
    }
    const auto* base = reinterpret_cast<unsigned char*>(rebuilt);
    const auto plugins = reinterpret_cast<void (*)()>(GetProcAddress(rebuilt, "JetGetPluginsRenderSystem"));
    if (!plugins || plugins < reinterpret_cast<void (*)()>(base) ||
        plugins >= reinterpret_cast<void (*)()>(base + 0x100000)) {
        std::puts("JetGetPluginsRenderSystem is not implemented locally");
        return 1;
    }

    PatchImport(original, "??0UId@Jet@@QAE@PBD@Z", reinterpret_cast<void*>(FakeUId));
    PatchImport(original, "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(FakeNew));
    PatchImport(original, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z", reinterpret_cast<void*>(FakeCreate));
    PatchImport(original, "??0ResourceTypeBase@Jet@@QAE@ABVPString@1@ABVUId@1@1@Z",
        reinterpret_cast<void*>(FakeTypeCtor));
    PatchImport(original, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z", reinterpret_cast<void*>(FakeDestroy));
    PatchImport(rebuilt, "??0UId@Jet@@QAE@PBD@Z", reinterpret_cast<void*>(FakeUId));
    PatchImport(rebuilt, "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(FakeNew));
    PatchImport(rebuilt, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z", reinterpret_cast<void*>(FakeCreate));
    PatchImport(rebuilt, "??0ResourceTypeBase@Jet@@QAE@ABVPString@1@ABVUId@1@1@Z",
        reinterpret_cast<void*>(FakeTypeCtor));
    PatchImport(rebuilt, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z", reinterpret_cast<void*>(FakeDestroy));

    trace = new Trace;
    const Trace expected = Run(original, reinterpret_cast<void (*)()>(
        GetProcAddress(original, "JetGetPluginsRenderSystem")));
    const Trace actual = Run(rebuilt, plugins);
    if (expected != actual) {
        std::printf("JetGetPluginsRenderSystem differs: %zu expected entries, %zu actual\n",
            expected.size(), actual.size());
        for (size_t i = 0; i < expected.size() && i < actual.size(); ++i)
            if (expected[i] != actual[i]) {
                std::printf("first difference at %zu: %d != %d\n", i, expected[i], actual[i]);
                break;
            }
        return 1;
    }
    std::printf("diff_rs_u12: %zu checks matching\n", expected.size());
    return 0;
}
