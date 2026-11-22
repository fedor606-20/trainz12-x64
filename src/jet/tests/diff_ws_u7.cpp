// Differential test of WaterSystem_7c U7a: the water material factories' lifetime and IsSupported.
//
// The RenderSystem base methods and the plugin-type reference-count thunks are patched to fake implementations
// in both modules, so each side sees identical base behaviour. The factories' own vtables, allocation trace,
// object bytes and virtual calls are compared. CreateInstance stays the original slot in this batch.
//
// usage: diff_ws_u7.exe <dir with WaterSystem_7c.dll, WaterSystem_7c_orig.dll and the Jet modules>
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

static HMODULE mod[2];
static unsigned long long checks, bad;
static int cur = -1;
static std::string logText[2];
static std::map<const void*, std::string> names[2];
static int allocCount[2];

using NewFn = void*(__cdecl*)(unsigned);
using DeleteFn = void(__cdecl*)(void*);
static NewFn RealNew;
static DeleteFn RealDelete;

static void Log(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (cur >= 0) logText[cur] += buf;
}

static std::string Name(const void* p)
{
    if (!p) return "null";
    auto it = names[cur].find(p);
    return it == names[cur].end() ? "?" : it->second;
}

static void* __cdecl HookNew(unsigned n)
{
    void* p = RealNew(n);
    std::memset(p, 0xcd, n);
    char b[32];
    std::snprintf(b, sizeof b, "A%d", allocCount[cur]++);
    names[cur][p] = b;
    Log("  new %u -> %s\n", n, b);
    return p;
}

static void __cdecl HookDelete(void* p)
{
    if (cur >= 0 && p) {
        Log("  free %s\n", Name(p).c_str());
        names[cur].erase(p);
    }
    RealDelete(p);
}

static void PatchImport(HMODULE m, const char* name, void* fn)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk
                                                                                          : d->FirstThunk));
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; thunk->u1.AddressOfData; ++thunk, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal)) continue;
            if (std::strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + thunk->u1.AddressOfData)->Name, name))
                continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            iat->u1.Function = reinterpret_cast<uintptr_t>(fn);
            VirtualProtect(&iat->u1.Function, sizeof(void*), old, &old);
        }
    }
}

static void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok && ++bad <= 40) std::printf("MISMATCH %s\n", what);
}

template <class T>
static T& At(void* p, unsigned off)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(p) + off);
}

template <class T>
static void* Slot(T fn)
{
    static_assert(sizeof fn == sizeof(void*), "single-inheritance member function pointer must be one slot");
    void* p;
    std::memcpy(&p, &fn, sizeof p);
    return p;
}

// ---- stand-ins for the imported RenderSystem/Kernel base methods ----

static char Tag(void* p) { return static_cast<char>(At<unsigned char>(p, 0x20)); }

struct BaseFake {
    void __thiscall Ctor(const void*, const void*)
    {
        std::memset(static_cast<unsigned char*>(static_cast<void*>(this)) + 4, 0, 0x1c);
        At<unsigned>(this, 4) = 0x11111111;
        Log("  base ctor %c\n", Tag(this));
    }
    void __thiscall Copy(const BaseFake& other)
    {
        std::memcpy(static_cast<unsigned char*>(static_cast<void*>(this)) + 4,
                    static_cast<const unsigned char*>(static_cast<const void*>(&other)) + 4, 0x1c);
        Log("  base copy %c\n", Tag(this));
    }
    BaseFake& __thiscall Assign(const BaseFake& other)
    {
        std::memcpy(static_cast<unsigned char*>(static_cast<void*>(this)) + 4,
                    static_cast<const unsigned char*>(static_cast<const void*>(&other)) + 4, 0x1c);
        Log("  base assign %c\n", Tag(this));
        return *this;
    }
    void __thiscall Dtor() { Log("  base dtor %c\n", Tag(this)); }
};

struct ThunkFake {
    void __thiscall AddRef() { Log("  addref %c\n", Tag(this)); }
    void __thiscall Release() { Log("  release %c\n", Tag(this)); }
    void* __thiscall CreateBare()
    {
        Log("  createBare %c\n", Tag(this));
        return nullptr;
    }
};

static void* NewObject(char tag)
{
    void* p = HookNew(0x40);
    At<unsigned char>(p, 0x20) = static_cast<unsigned char>(tag);
    return p;
}

static void DumpObject(void* p, const char* what)
{
    Log("  %s %c:", what, Tag(p));
    for (unsigned i = 4; i < 0x24; ++i) Log(" %02x", At<unsigned char>(p, i));
    Log("\n");
}

using Ctor0 = void*(__thiscall*)(void*);
using Ctor2 = void*(__thiscall*)(void*, const void*, const void*);
using Copy = void*(__thiscall*)(void*, const void*);
using Dtor = void(__thiscall*)(void*);
using Assign = void*(__thiscall*)(void*, const void*);

struct Api {
    Ctor0 def, def1;
    Ctor2 named, named1;
    Copy copy, copy1;
    Dtor dtor, dtor1;
    Assign assign, assign1;
};

static Api api[3];  // 0 Bump, 1 OneTex, 2 Ocean

template <class T>
static T Fn(int s, const char* name)
{
    return reinterpret_cast<T>(GetProcAddress(mod[s], name));
}

static void CallVirtuals(void* p)
{
    void** vt = At<void**>(p, 0);
    reinterpret_cast<void(__thiscall*)(void*)>(vt[0])(p);
    reinterpret_cast<void(__thiscall*)(void*)>(vt[1])(p);
    reinterpret_cast<void*(__thiscall*)(void*)>(vt[3])(p);
}

static bool CallSupported(void* p, unsigned char a, unsigned char b)
{
    void* display = HookNew(0x220);
    At<unsigned char>(display, 0x211) = a;
    At<unsigned char>(display, 0x212) = b;
    unsigned char fallback[4] = {};
    void** vt = At<void**>(p, 0);
    bool const r = reinterpret_cast<bool(__thiscall*)(void*, void*, void*)>(vt[4])(p, display, fallback);
    Log("  supported %u,%u -> %d\n", a, b, r ? 1 : 0);
    HookDelete(display);
    return r;
}

static void DestroyVirtual(void* p)
{
    void** vt = At<void**>(p, 0);
    void* r = reinterpret_cast<void*(__thiscall*)(void*, unsigned)>(vt[2])(p, 1);
    Check(r == p, "vector dtor returns this");
}

static void DefaultRun(int s)
{
    void* p = NewObject('B');
    api[0].def(p);
    DumpObject(p, "default B");
    CallVirtuals(p);
    CallSupported(p, 0, 0);
    CallSupported(p, 1, 0);
    CallSupported(p, 0, 1);
    CallSupported(p, 1, 1);
    DestroyVirtual(p);
}

static void NamedRun(int s)
{
    void* p = NewObject('O');
    const unsigned args[8] = {};
    api[2].named(p, args, args + 4);
    DumpObject(p, "named O");
    CallVirtuals(p);
    CallSupported(p, 1, 1);
    DestroyVirtual(p);
}

static void CopyRun(int s)
{
    void* src = NewObject('S');
    api[1].def(src);
    void* dst = NewObject('D');
    api[1].copy(dst, src);
    DumpObject(src, "copy src");
    DumpObject(dst, "copy dst");
    DestroyVirtual(dst);
    DestroyVirtual(src);
}

static void AssignRun(int s)
{
    void* src = NewObject('S');
    api[0].def(src);
    void* dst = NewObject('D');
    api[0].def(dst);
    api[0].assign(dst, src);
    DumpObject(dst, "assigned");
    DestroyVirtual(dst);
    DestroyVirtual(src);
}

static void OceanRun(int s)
{
    void* p = NewObject('W');
    api[2].def(p);
    DumpObject(p, "default W");
    CallSupported(p, 1, 1);
    api[2].dtor(p);  // the exported base dtor, not the vtable's deleting destructor
}

static void RunSide(int s, void (*run)(int))
{
    cur = s;
    logText[s].clear();
    names[s].clear();
    allocCount[s] = 0;
    run(s);
    cur = -1;
}

static void CheckVtable(const char* name, bool localDtor)
{
    void** ours = reinterpret_cast<void**>(GetProcAddress(mod[0], name));
    void** orig = reinterpret_cast<void**>(GetProcAddress(mod[1], name));
    Check(ours && orig, name);
    if (!ours || !orig) return;
    Check(ours[-1] == orig[-1], "vtable RTTI locator");
    Check(ours[0] == orig[0], "vtable slot 0");
    Check(ours[1] == orig[1], "vtable slot 1");
    Check(ours[3] == orig[3], "vtable slot 3");
    if (localDtor)
        Check(ours[2] != orig[2] && ours[4] != orig[4], "local dtor and IsSupported slots");
    else
        Check(ours[2] == orig[2] && ours[4] != orig[4], "original dtor, local IsSupported slot");
}

static void Compare(const char* name)
{
    Check(logText[0] == logText[1], name);
    if (logText[0] != logText[1]) {
        std::printf("---- %s: ours ----\n%s---- original ----\n%s", name, logText[0].c_str(), logText[1].c_str());
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_ws_u7 <bin dir>\n");
        return 2;
    }
    const std::string dir = argv[1];
    SetDllDirectoryA(dir.c_str());
    // the mirror initialiser in our DLL reads the original's vtables, so load the original first
    mod[1] = LoadLibraryA((dir + "\\WaterSystem_7c_orig.dll").c_str());
    const DWORD err1 = GetLastError();
    mod[0] = LoadLibraryA((dir + "\\WaterSystem_7c.dll").c_str());
    const DWORD err0 = GetLastError();
    HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load modules from %s (ours=%p err=%lu orig=%p err=%lu ks=%p)\n", argv[1],
                    static_cast<void*>(mod[0]), err0, static_cast<void*>(mod[1]), err1,
                    static_cast<void*>(ks));
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    RealNew = reinterpret_cast<NewFn>(GetProcAddress(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    RealDelete = reinterpret_cast<DeleteFn>(GetProcAddress(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));

    CheckVtable("??_7WaterBumpMaterialPlugin@Jet@@6B@", true);
    CheckVtable("??_7WaterMaterialFresWavePlugin@Jet@@6B@", false);
    CheckVtable("??_7WaterMaterialOneTexPlugin@Jet@@6B@", true);
    CheckVtable("??_7WaterOceanMaterialPlugin@Jet@@6B@", true);

    for (int s = 0; s < 2; ++s) {
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
        PatchImport(mod[s], "??0MaterialPlugin@Jet@@QAE@ABVPString@1@ABVUId@1@@Z", Slot(&BaseFake::Ctor));
        PatchImport(mod[s], "??0MaterialPlugin@Jet@@QAE@ABV01@@Z", Slot(&BaseFake::Copy));
        PatchImport(mod[s], "??4MaterialPlugin@Jet@@QAEAAV01@ABV01@@Z", Slot(&BaseFake::Assign));
        PatchImport(mod[s], "??1MaterialPlugin@Jet@@UAE@XZ", Slot(&BaseFake::Dtor));
        PatchImport(mod[s], "?AddReference@PluginType@Jet@@UBAXXZ", Slot(&ThunkFake::AddRef));
        PatchImport(mod[s], "?ReleaseReference@PluginType@Jet@@UBAXXZ", Slot(&ThunkFake::Release));
        PatchImport(mod[s], "?CreateInstance@MaterialPlugin@Jet@@MBAPAVMaterial@2@XZ", Slot(&ThunkFake::CreateBare));

        api[0].def = Fn<Ctor0>(s, "??0WaterBumpMaterialPlugin@Jet@@QAE@XZ");
        api[0].named = Fn<Ctor2>(s, "??0WaterBumpMaterialPlugin@Jet@@IAE@ABVPString@1@ABVUId@1@@Z");
        api[0].copy = Fn<Copy>(s, "??0WaterBumpMaterialPlugin@Jet@@QAE@ABV01@@Z");
        api[0].dtor = Fn<Dtor>(s, "??1WaterBumpMaterialPlugin@Jet@@UAE@XZ");
        api[0].assign = Fn<Assign>(s, "??4WaterBumpMaterialPlugin@Jet@@QAEAAV01@ABV01@@Z");
        api[1].def = Fn<Ctor0>(s, "??0WaterMaterialOneTexPlugin@Jet@@QAE@XZ");
        api[1].copy = Fn<Copy>(s, "??0WaterMaterialOneTexPlugin@Jet@@QAE@ABV01@@Z");
        api[1].dtor = Fn<Dtor>(s, "??1WaterMaterialOneTexPlugin@Jet@@UAE@XZ");
        api[1].assign = Fn<Assign>(s, "??4WaterMaterialOneTexPlugin@Jet@@QAEAAV01@ABV01@@Z");
        api[2].def = Fn<Ctor0>(s, "??0WaterOceanMaterialPlugin@Jet@@QAE@XZ");
        api[2].named = Fn<Ctor2>(s, "??0WaterOceanMaterialPlugin@Jet@@IAE@ABVPString@1@ABVUId@1@@Z");
        api[2].copy = Fn<Copy>(s, "??0WaterOceanMaterialPlugin@Jet@@QAE@ABV01@@Z");
        api[2].dtor = Fn<Dtor>(s, "??1WaterOceanMaterialPlugin@Jet@@UAE@XZ");
        api[2].assign = Fn<Assign>(s, "??4WaterOceanMaterialPlugin@Jet@@QAEAAV01@ABV01@@Z");
    }

    struct {
        const char* name;
        void (*run)(int);
    } tests[] = {{"default Bump", DefaultRun}, {"named Ocean", NamedRun}, {"copy OneTex", CopyRun},
                 {"assign Bump", AssignRun}, {"Ocean base dtor", OceanRun}};
    for (const auto& t : tests) {
        RunSide(0, t.run);
        RunSide(1, t.run);
        Compare(t.name);
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
