// Usage: diff_rs_u13.exe <bin dir>. Compare RenderKernel lifetime and dump-screen exports of
// RenderSystem_7c.dll against RenderSystem_7c_orig.dll with all external collaborators replaced by stand-ins.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using Trace = std::vector<int>;
static HMODULE rebuiltModule;
static Trace* trace;
static unsigned char object[0x100];
static unsigned char nodes[8][0x20];
static unsigned nodeCount;
static unsigned char bitmaps[4][0x40];
static unsigned bitmapCount;

static void Text(const char* s)
{
    trace->push_back(0x100);
    for (; *s; ++s) trace->push_back(static_cast<unsigned char>(*s));
    trace->push_back(0);
}

static int PtrId(const void* p)
{
    if (!p) return 0;
    const auto q = reinterpret_cast<uintptr_t>(p);
    for (unsigned i = 0; i < nodeCount; ++i)
        if (q == reinterpret_cast<uintptr_t>(nodes[i])) return 1 + static_cast<int>(i);
    for (unsigned i = 0; i < bitmapCount; ++i)
        if (q == reinterpret_cast<uintptr_t>(bitmaps[i])) return 10 + static_cast<int>(i);
    if (q == reinterpret_cast<uintptr_t>(object)) return 30;
    return 99;
}

static int NodeId(const void* p)
{
    for (unsigned i = 0; i < nodeCount; ++i)
        if (p == nodes[i]) return static_cast<int>(i);
    return -1;
}

static void* __cdecl FakeCreate(void* cache, const unsigned char* text)
{
    if (nodeCount >= 8) std::exit(2);
    trace->insert(trace->end(), {1, cache ? 1 : 0, static_cast<int>(nodeCount)});
    Text(reinterpret_cast<const char*>(text));
    return nodes[nodeCount++];
}

static void __cdecl FakeDestroy(void* cache, void* node)
{
    trace->insert(trace->end(), {2, cache ? 1 : 0, NodeId(node)});
}

static void __stdcall FakeTaskCtor(void* nodeAddress, float priority, bool thread)
{
    void* self;
    __asm { mov self, ecx }
    const int node = nodeAddress ? NodeId(*static_cast<void**>(nodeAddress)) : -1;
    union { float f; unsigned u; } const p{priority};
    trace->insert(trace->end(), {3, PtrId(self), node, static_cast<int>(p.u), thread ? 1 : 0});
    std::memset(self, 0xcc, 0x28);
}

static void __stdcall FakeStringCtor()
{
    void* self;
    __asm { mov self, ecx }
    trace->insert(trace->end(), {4, PtrId(self)});
    std::memset(self, 0xdd, 4);
}

static void* __cdecl FakeAnsiAssign(void* self, const char* text)
{
    trace->push_back(5);
    Text(text);
    std::memset(self, 0xe0, 8);
    return self;
}

static void* __cdecl FakeNew(unsigned size)
{
    trace->insert(trace->end(), {6, static_cast<int>(size)});
    return nullptr;  // no SceneGraph: the ctor records +0x80 = 0 and the dtor skips its destruction
}

static void __stdcall FakeTaskDtor()
{
    void* self;
    __asm { mov self, ecx }
    trace->insert(trace->end(), {7, PtrId(self)});
}

// The mirrored vtable slot is the scalar deleting destructor: ECX = this, one int on the stack.
static void __stdcall FakeTaskDeletingDtor(int deleting)
{
    void* self;
    __asm { mov self, ecx }
    (void)deleting;
    trace->insert(trace->end(), {7, PtrId(self)});
}

static void __stdcall FakeStringDtor()
{
    void* self;
    __asm { mov self, ecx }
    trace->insert(trace->end(), {8, PtrId(self)});
}

static void __cdecl FakeDelete(void* p)
{
    trace->insert(trace->end(), {9, PtrId(p)});
}

static bool __stdcall FakeRelease()
{
    void* self;
    __asm { mov self, ecx }
    trace->insert(trace->end(), {10, PtrId(self)});
    return true;
}

static void __stdcall FakeAnsiCopyCtor(const void* source)
{
    void* self;
    __asm { mov self, ecx }
    trace->insert(trace->end(), {11, PtrId(self)});
    std::memcpy(self, source, 4);  // stand-in String is one pointer to a file-name literal
}

static void __cdecl FakeAnsiCopyAssign(void* self, const void* source)
{
    trace->insert(trace->end(), {11, PtrId(self)});
    std::memcpy(self, source, 4);
}

static const char* __cdecl FakeStringText(void* self)
{
    trace->insert(trace->end(), {12, PtrId(self)});
    return *static_cast<const char* const*>(self);
}

static bool __cdecl FakeBitmapSave(void* self, const char* fileName)
{
    trace->insert(trace->end(), {13, PtrId(self)});
    Text(fileName);
    return true;
}

static void __stdcall FakeBitmapDtor()
{
    void* self;
    __asm { mov self, ecx }
    trace->insert(trace->end(), {14, PtrId(self)});
}

static bool __cdecl FakeWait(void* self, int timeout)
{
    trace->insert(trace->end(), {15, PtrId(self), timeout});
    return true;
}

static void __cdecl FakeWEventSet(void* self)
{
    trace->insert(trace->end(), {16, PtrId(self)});
}

// The local Task vtable mirror is copied from the original at load. Swap slot 0 for the stand-in destructor only
// while the local RenderKernel destructor runs; restore it before snapshots and vtable checks.
static unsigned* LocalTaskVtable()
{
    void* const data = GetProcAddress(rebuiltModule, "??_7RenderKernel@Jet@@6BTask@1@@");
    if (!data) std::exit(2);
    return static_cast<unsigned*>(data);
}

static void WithFakeTaskDtor(void (__thiscall* fn)(void*), void* self)
{
    unsigned* const mirror = LocalTaskVtable();
    void* const saved = reinterpret_cast<void*>(mirror[0]);
    mirror[0] = static_cast<unsigned>(reinterpret_cast<uintptr_t>(&FakeTaskDeletingDtor));
    fn(self);
    mirror[0] = static_cast<unsigned>(reinterpret_cast<uintptr_t>(saved));
}

static void PatchImport(HMODULE module, const char* name, void* fn, bool required = true)
{
    auto* const base = reinterpret_cast<unsigned char*>(module);
    auto* const nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
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
    if (!patched && required) {
        std::printf("import not found: %s\n", name);
        std::exit(2);
    }
}

static bool InModule(const void* p, HMODULE module)
{
    const auto q = reinterpret_cast<uintptr_t>(p);
    const auto base = reinterpret_cast<uintptr_t>(module);
    return q >= base && q < base + 0x100000;
}

using CtorFn = void(__thiscall*)(void*, void*, float);
using DtorFn = void(__thiscall*)(void*);
using ClosureFn = void(__thiscall*)(void*);
using DumpMemberFn = unsigned(__cdecl*)(void*);
using DumpThreadFn = unsigned(__stdcall*)(void*);

static void* Local(HMODULE module, const char* name)
{
    const void* const fn = reinterpret_cast<void*>(GetProcAddress(module, name));
    if (!fn || !InModule(fn, module)) {
        std::printf("export missing or forwarded: %s (%p, module %p)\n", name, fn, module);
        std::fflush(stdout);
        std::exit(1);
    }
    return const_cast<void*>(fn);
}

struct Snapshot {
    unsigned char bytes[sizeof object];
    const unsigned* taskVtable;
    const unsigned* layerVtable;
};

static Snapshot Capture(const unsigned* originalTask, const unsigned* originalLayer)
{
    Snapshot s{};
    std::memcpy(s.bytes, object, sizeof object);
    (void)originalTask;
    (void)originalLayer;
    s.taskVtable = *reinterpret_cast<const unsigned* const*>(object);
    s.layerVtable = *reinterpret_cast<const unsigned* const*>(object + 0x28);
    return s;
}

static void Normalize(unsigned char* bytes, HMODULE original, HMODULE rebuilt)
{
    for (const int offset : {0, 0x28}) {
        const auto p = *reinterpret_cast<void**>(bytes + offset);
        if (InModule(p, original) || InModule(p, rebuilt))
            std::memcpy(bytes + offset, "\x5a\x5a\x5a\x5a", 4);
    }
    // CriticalSection/WEvent internals and the queue buffer address vary between reinitializations.
    for (unsigned i = 0xb8; i < 0xcc; ++i) bytes[i] = 0x5a;
    for (unsigned i = 0xd4; i < 0xd8; ++i) bytes[i] = 0x5a;
}

static bool Same(const Snapshot& a, const Snapshot& b, HMODULE original, HMODULE rebuilt)
{
    unsigned char x[sizeof object];
    unsigned char y[sizeof object];
    std::memcpy(x, a.bytes, sizeof x);
    std::memcpy(y, b.bytes, sizeof y);
    Normalize(x, original, rebuilt);
    Normalize(y, original, rebuilt);
    return std::memcmp(x, y, sizeof x) == 0;
}

static Trace RunLifetime(CtorFn ctor, DtorFn dtor, Snapshot& afterCtor, Snapshot& afterDtor,
                        const unsigned* originalTask, const unsigned* originalLayer)
{
    trace->clear();
    nodeCount = 0;
    std::memset(object, 0, sizeof object);
    ctor(object, nullptr, 8.0f);
    afterCtor = Capture(originalTask, originalLayer);
    if (InModule(reinterpret_cast<const void*>(dtor), rebuiltModule))
        WithFakeTaskDtor(dtor, object);
    else
        dtor(object);
    afterDtor = Capture(originalTask, originalLayer);
    return *trace;
}

static Trace RunClosure(ClosureFn closure, DtorFn dtor, Snapshot& afterCtor, Snapshot& afterDtor,
                        const unsigned* originalTask, const unsigned* originalLayer)
{
    trace->clear();
    nodeCount = 0;
    std::memset(object, 0, sizeof object);
    closure(object);
    afterCtor = Capture(originalTask, originalLayer);
    if (InModule(reinterpret_cast<const void*>(dtor), rebuiltModule))
        WithFakeTaskDtor(dtor, object);
    else
        dtor(object);
    afterDtor = Capture(originalTask, originalLayer);
    return *trace;
}

struct TestEntry {
    void* bitmap;
    const char* fileName;
};

static void InitQueue(HMODULE original)
{
    void (__stdcall* const init)(void*) = reinterpret_cast<void (__stdcall*)(void*)>(
        reinterpret_cast<unsigned char*>(original) + 0x3a1a0);
    init(object + 0xb8);
}

static void PushEntry(HMODULE original, const TestEntry* entry)
{
    void (*const pushFn)(const TestEntry*) = reinterpret_cast<void (*)(const TestEntry*)>(
        reinterpret_cast<unsigned char*>(original) + 0x3a280);
    void* saved;
    __asm {
        mov     eax, entry
        mov     saved, edi
        lea     edi, object
        add     edi, 0xb8
        push    eax
        call    pushFn
        add     esp, 4
        mov     edi, saved
    }
}

static void PrepareDump(HMODULE original)
{
    std::memset(object, 0, sizeof object);
    InitQueue(original);
    bitmapCount = 1;
    std::memset(bitmaps, 0, sizeof bitmaps);
    const TestEntry entry{bitmaps[0], "dump.bmp"};
    PushEntry(original, &entry);  // queue count at +0xcc becomes 1; +0x84 stays 0
}

template<class F>
static Trace RunDump(HMODULE original, F dump, Snapshot& after)
{
    PrepareDump(original);
    trace->clear();
    dump(object);
    after = Capture(nullptr, nullptr);
    return *trace;
}

static bool VtableMatches(const unsigned* local, const unsigned* original, unsigned slots)
{
    if (!local || !original) return false;
    return std::memcmp(local, original, slots * sizeof(unsigned)) == 0;
}

int main(int argc, char** argv)
{
    if (argc != 2 || !SetDllDirectoryA(argv[1])) {
        std::puts("usage: diff_rs_u13.exe <bin dir>");
        return 2;
    }
    const HMODULE original = LoadLibraryA("RenderSystem_7c_orig.dll");
    const HMODULE rebuilt = LoadLibraryA("RenderSystem_7c.dll");
    if (!original || !rebuilt) {
        std::printf("cannot load RenderSystem DLLs: %lu\n", GetLastError());
        return 2;
    }
    rebuiltModule = rebuilt;

    static const char* const lifetimeImports[] = {
        "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z",
        "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z",
        "??0Task@Jet@@QAE@ABVPString@1@M_N@Z",
        "??0String@Jet@@QAE@XZ",
        "??4AnsiString@Jet@@QAAABV01@PBD@Z",
        "?New@Alloc@Jet@@SAPAXI@Z",
        "?Delete@Alloc@Jet@@SAXPAX@Z",
        "??1Task@Jet@@UAE@XZ",
        "??1String@Jet@@QAE@XZ",
        "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ",
    };
    void* const lifetimeFakes[] = {
        reinterpret_cast<void*>(FakeCreate), reinterpret_cast<void*>(FakeDestroy),
        reinterpret_cast<void*>(FakeTaskCtor), reinterpret_cast<void*>(FakeStringCtor),
        reinterpret_cast<void*>(FakeAnsiAssign), reinterpret_cast<void*>(FakeNew),
        reinterpret_cast<void*>(FakeDelete), reinterpret_cast<void*>(FakeTaskDtor),
        reinterpret_cast<void*>(FakeStringDtor), reinterpret_cast<void*>(FakeRelease),
    };
    for (unsigned i = 0; i < 10; ++i) {
        PatchImport(original, lifetimeImports[i], lifetimeFakes[i]);
        // The local destructor calls the mirrored Task vtable slot, not this IAT entry.
        PatchImport(rebuilt, lifetimeImports[i], lifetimeFakes[i], i != 7);
    }
    // These are consumed by the original's internal queue helpers, which both modules call.
    PatchImport(original, "??0AnsiString@Jet@@QAE@ABV01@@Z", reinterpret_cast<void*>(FakeAnsiCopyCtor));
    PatchImport(original, "??4AnsiString@Jet@@QAAABV01@ABV01@@Z", reinterpret_cast<void*>(FakeAnsiCopyAssign));
    PatchImport(original, "??BString@Jet@@QBAPBDXZ", reinterpret_cast<void*>(FakeStringText));
    PatchImport(original, "?Save@Bitmap@Jet@@QBA_NPBD@Z", reinterpret_cast<void*>(FakeBitmapSave));
    PatchImport(original, "??1Bitmap@Jet@@QAE@XZ", reinterpret_cast<void*>(FakeBitmapDtor));
    PatchImport(original, "?Wait@SyncObject@Jet@@QBA_NK@Z", reinterpret_cast<void*>(FakeWait));
    PatchImport(original, "?Set@WEvent@Jet@@QAAXXZ", reinterpret_cast<void*>(FakeWEventSet));
    // The rebuilt dump calls these imports directly.
    PatchImport(rebuilt, "??BString@Jet@@QBAPBDXZ", reinterpret_cast<void*>(FakeStringText));
    PatchImport(rebuilt, "?Save@Bitmap@Jet@@QBA_NPBD@Z", reinterpret_cast<void*>(FakeBitmapSave));
    PatchImport(rebuilt, "??1Bitmap@Jet@@QAE@XZ", reinterpret_cast<void*>(FakeBitmapDtor));

    const auto origCtor = reinterpret_cast<CtorFn>(GetProcAddress(original, "??0RenderKernel@Jet@@QAE@PAVSceneGraph@1@M@Z"));
    const auto origDtor = reinterpret_cast<DtorFn>(GetProcAddress(original, "??1RenderKernel@Jet@@UAE@XZ"));
    const auto origClosure = reinterpret_cast<ClosureFn>(GetProcAddress(original, "??_FRenderKernel@Jet@@QAEXXZ"));
    const auto origDumpMember = reinterpret_cast<DumpMemberFn>(GetProcAddress(original, "?DumpScreenSave@RenderKernel@Jet@@IAAIXZ"));
    const auto origDumpThread = reinterpret_cast<DumpThreadFn>(GetProcAddress(original, "?DumpScreenSave@RenderKernel@Jet@@KGIPAX@Z"));
    if (!origCtor || !origDtor || !origClosure || !origDumpMember || !origDumpThread) {
        std::puts("original exports missing");
        return 2;
    }

    const auto ctor = reinterpret_cast<CtorFn>(Local(rebuilt, "??0RenderKernel@Jet@@QAE@PAVSceneGraph@1@M@Z"));
    const auto dtor = reinterpret_cast<DtorFn>(Local(rebuilt, "??1RenderKernel@Jet@@UAE@XZ"));
    const auto closure = reinterpret_cast<ClosureFn>(Local(rebuilt, "??_FRenderKernel@Jet@@QAEXXZ"));
    const auto dumpMember = reinterpret_cast<DumpMemberFn>(Local(rebuilt, "?DumpScreenSave@RenderKernel@Jet@@IAAIXZ"));
    const auto dumpThread = reinterpret_cast<DumpThreadFn>(Local(rebuilt, "?DumpScreenSave@RenderKernel@Jet@@KGIPAX@Z"));

    trace = new Trace;
    Snapshot origCtorSnap, origDtorSnap, newCtorSnap, newDtorSnap;
    const Trace expectedCtor = RunLifetime(origCtor, origDtor, origCtorSnap, origDtorSnap, nullptr, nullptr);
    const unsigned* const taskVtable = origCtorSnap.taskVtable;
    const unsigned* const layerVtable = origCtorSnap.layerVtable;
    if (!taskVtable || !layerVtable) {
        std::puts("original vptrs not found");
        return 1;
    }
    const Trace actualCtor = RunLifetime(ctor, dtor, newCtorSnap, newDtorSnap, taskVtable, layerVtable);

    Snapshot origClosureSnap, origClosureDtorSnap, newClosureSnap, newClosureDtorSnap;
    const Trace expectedClosure = RunClosure(origClosure, origDtor, origClosureSnap, origClosureDtorSnap,
                                             taskVtable, layerVtable);
    const Trace actualClosure = RunClosure(closure, dtor, newClosureSnap, newClosureDtorSnap,
                                           taskVtable, layerVtable);

    // The queue helpers need a real buffer. Restore the original Alloc::New only for the dump phase; the
    // lifetime tests above intentionally saw a null SceneGraph from the stand-in.
    void* const realAllocNew = GetProcAddress(GetModuleHandleA("KernelStandard_7c.dll"), "?New@Alloc@Jet@@SAPAXI@Z");
    if (!realAllocNew) std::exit(2);
    PatchImport(original, "?New@Alloc@Jet@@SAPAXI@Z", realAllocNew);
    Snapshot origDumpSnap, newDumpSnap;
    const Trace expectedDump = RunDump(original, origDumpMember, origDumpSnap);
    const Trace actualDump = RunDump(original, dumpMember, newDumpSnap);
    const Trace expectedThread = RunDump(original, origDumpThread, origDumpSnap);
    const Trace actualThread = RunDump(original, dumpThread, newDumpSnap);

    int failures = 0;
    auto Check = [&](const char* name, const Trace& a, const Trace& b) {
        if (a != b) {
            std::printf("%s trace differs: %zu expected, %zu actual\n", name, a.size(), b.size());
            for (size_t i = 0; i < a.size() && i < b.size(); ++i)
                if (a[i] != b[i]) {
                    std::printf("  first mismatch at %zu: %d != %d\n", i, a[i], b[i]);
                    break;
                }
            ++failures;
        }
    };
    auto CheckSnap = [&](const char* name, const Snapshot& a, const Snapshot& b) {
        if (!Same(a, b, original, rebuilt)) {
            std::printf("%s object bytes differ\n", name);
            ++failures;
        }
    };

    Check("ctor/dtor", expectedCtor, actualCtor);
    CheckSnap("ctor", origCtorSnap, newCtorSnap);
    CheckSnap("dtor", origDtorSnap, newDtorSnap);
    Check("closure", expectedClosure, actualClosure);
    CheckSnap("closure", origClosureSnap, newClosureSnap);
    CheckSnap("closure dtor", origClosureDtorSnap, newClosureDtorSnap);
    Check("dump member", expectedDump, actualDump);
    CheckSnap("dump member queue", origDumpSnap, newDumpSnap);
    Check("dump thread", expectedThread, actualThread);
    CheckSnap("dump thread queue", origDumpSnap, newDumpSnap);

    if (!VtableMatches(newCtorSnap.taskVtable, taskVtable, 6)) {
        std::puts("local Task vtable mirror differs");
        ++failures;
    }
    if (!VtableMatches(newCtorSnap.layerVtable, layerVtable, 2)) {
        std::puts("local LayerManager vtable mirror differs");
        ++failures;
    }

    if (failures) {
        std::printf("diff_rs_u13: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("diff_rs_u13: %zu ctor/dtor, %zu closure, %zu dump checks matching\n",
                expectedCtor.size(), expectedClosure.size(), expectedDump.size());
    return 0;
}
