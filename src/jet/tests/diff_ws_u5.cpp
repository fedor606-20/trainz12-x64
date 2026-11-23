// Differential test of WaterSystem_7c unit U5 tail: WaterGeometryAnimation lifetime.
//
// The four exports are called through GetProcAddress on our module and on WaterSystem_7c_orig.dll. Both
// modules' Jet::Alloc imports are hooked, so every chunk/item allocation and free is logged with a name;
// the object, its chunk rings and the allocation trace must match. Items and the geometry are fake objects
// with stand-in deleting destructors, so the destructor's ownership is checked without the rest of U5/U7.
//
// usage: diff_ws_u5.exe <dir with WaterSystem_7c.dll, WaterSystem_7c_orig.dll and the Jet modules>
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>

static HMODULE mod[2];  // 0 ours, 1 the original
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

// a test block from the real allocator, named and logged on the current side
static void* NamedNew(unsigned n, const char* name)
{
    void* p = RealNew(n);
    std::memset(p, 0xcd, n);
    names[cur][p] = name;
    Log("  new %u -> %s\n", n, name);
    return p;
}

// ---- stand-in objects owned by the animation

// the callers use __thiscall deleting destructors: this in ECX, flags on the stack, callee pops the flags.
// A non-static member function is the only way to spell that convention on MSVC x86; its address is the vtable slot.
struct GridFake {
    void* __thiscall Dtor(unsigned)
    {
        void* p = static_cast<void*>(this);
        if (void* data = At<void*>(p, 0xc)) HookDelete(data);
        HookDelete(p);
        return p;
    }
};

struct GeometryFake {
    void* __thiscall ScalarDtor(unsigned flag)
    {
        void* p = static_cast<void*>(this);
        Log("  geometry scalar dtor flag=%u\n", flag);
        HookDelete(p);
        return p;
    }
    void* __thiscall VectorDtor(unsigned flag)
    {
        void* p = static_cast<void*>(this);
        Log("  geometry vector dtor flag=%u\n", flag);
        HookDelete(p);
        return p;
    }
};

template <class T>
static void* DtorSlot(void* (T::*fn)(unsigned))
{
    static_assert(sizeof fn == sizeof(void*), "single-inheritance member function pointer must be one slot");
    void* p;
    std::memcpy(&p, &fn, sizeof p);
    return p;
}

static void* gridVt[2];
static void* geometryVt[2];

static void* MakeGrid(const char* item, const char* data)
{
    void* p = NamedNew(0x10, item);
    At<void*>(p, 0) = gridVt;
    void* d = NamedNew(0x20, data);
    At<void*>(p, 0xc) = d;
    return p;
}

static void* MakeGeometry(const char* name)
{
    void* p = NamedNew(0x10, name);
    At<void*>(p, 0) = &geometryVt[0];  // slot 0: scalar deleting destructor
    At<void*>(p, 4) = &geometryVt[1];  // slot +4: vector deleting destructor, as the original calls
    return p;
}

// FastDynamicArray<*,13>: {next, prev, count, data[13]}, a ring of 0x40-byte chunks
static void* MakeChunk(const char* name, void* next, void* prev, std::initializer_list<void*> items)
{
    void* c = NamedNew(0x40, name);
    At<void*>(c, 0) = next;
    At<void*>(c, 4) = prev;
    At<unsigned>(c, 8) = static_cast<unsigned>(items.size());
    unsigned i = 0;
    for (void* item : items) At<void*>(c, 0xc + 4 * i++) = item;
    for (; i < 13; ++i) At<void*>(c, 0xc + 4 * i) = reinterpret_cast<void*>(0xcdcdcdcdu);
    return c;
}

static void* MakeAnimation(const char* tag)
{
    const std::string g = std::string(tag) + "G";
    const std::string f0 = std::string(tag) + "F0", f1 = std::string(tag) + "F1";
    const std::string x0 = std::string(tag) + "X0";
    void* a = NamedNew(0x1c, tag);
    At<void*>(a, 0) = MakeGeometry(g.c_str());
    void* i0 = MakeGrid((tag + std::string("I0")).c_str(), (tag + std::string("D0")).c_str());
    void* i1 = MakeGrid((tag + std::string("I1")).c_str(), (tag + std::string("D1")).c_str());
    void* i2 = MakeGrid((tag + std::string("I2")).c_str(), (tag + std::string("D2")).c_str());
    void* i3 = MakeGrid((tag + std::string("I3")).c_str(), (tag + std::string("D3")).c_str());
    void* j0 = MakeGrid((tag + std::string("J0")).c_str(), (tag + std::string("E0")).c_str());
    void* j1 = MakeGrid((tag + std::string("J1")).c_str(), (tag + std::string("E1")).c_str());
    void* fc1 = MakeChunk((tag + std::string("C1")).c_str(), nullptr, nullptr, {i3});
    void* fc0 = MakeChunk((tag + std::string("C0")).c_str(), fc1, fc1, {i0, i1, i2});
    At<void*>(fc1, 0) = fc0;
    At<void*>(fc1, 4) = fc0;
    void* xc0 = MakeChunk((tag + std::string("K0")).c_str(), nullptr, nullptr, {j0, j1});
    At<void*>(xc0, 0) = xc0;
    At<void*>(xc0, 4) = xc0;
    At<void*>(a, 4) = fc0;
    At<void*>(a, 8) = xc0;
    At<int>(a, 0xc) = 2;
    At<int>(a, 0x10) = 5;
    At<int>(a, 0x14) = 7;
    At<unsigned>(a, 0x18) = 0x3e800000u;  // 0.25f
    return a;
}

static void DumpObject(const void* a)
{
    Log("  object g=%s frames=%s xy=%s cur=%d w=%d h=%d scale=%08x\n",
        Name(At<void*>(const_cast<void*>(a), 0)).c_str(), Name(At<void*>(const_cast<void*>(a), 4)).c_str(),
        Name(At<void*>(const_cast<void*>(a), 8)).c_str(), At<int>(const_cast<void*>(a), 0xc),
        At<int>(const_cast<void*>(a), 0x10), At<int>(const_cast<void*>(a), 0x14),
        At<unsigned>(const_cast<void*>(a), 0x18));
}

static void DumpChunks(void* head, const char* tag)
{
    if (!head) {
        Log("  %s -\n", tag);
        return;
    }
    void* c = head;
    for (int guard = 0; guard < 8; ++guard) {
        Log("  %s next=%s prev=%s n=%u:", tag, Name(At<void*>(c, 0)).c_str(), Name(At<void*>(c, 4)).c_str(),
            At<unsigned>(c, 8));
        const unsigned n = At<unsigned>(c, 8);
        for (unsigned i = 0; i < n && i < 13; ++i) Log(" %s", Name(At<void*>(c, 0xc + 4 * i)).c_str());
        Log("\n");
        c = At<void*>(c, 0);
        if (c == head) break;
    }
}

// ---- per-side entry points

template <class T>
static T Fn(int side, const char* name)
{
    void* p = reinterpret_cast<void*>(GetProcAddress(mod[side], name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return reinterpret_cast<T>(p);
}

using Ctor = void(__fastcall*)(void*, void*);
using Copy = void(__fastcall*)(void*, void*, const void*);
using Dtor = void(__fastcall*)(void*, void*);
using Assign = void*(__fastcall*)(void*, void*, const void*);

struct Api {
    Ctor ctor, ctor1;
    Copy copy, copy1;
    Dtor dtor, dtor1;
    Assign assign, assign1;
};

static Api api;

static void Compare(const char* what)
{
    ++checks;
    if (logText[0] == logText[1]) return;
    ++bad;
    if (bad > 40) return;
    std::printf("MISMATCH %s\n--- ours ---\n%s--- orig ---\n%s", what, logText[0].c_str(), logText[1].c_str());
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

static void CtorRun(int s)
{
    unsigned char a[0x1c], b[0x1c];
    std::memset(a, 0xcc, sizeof a);
    std::memset(b, 0xcc, sizeof b);
    (s ? api.ctor1 : api.ctor)(a, nullptr);
    (s ? api.ctor1 : api.ctor)(b, nullptr);
    DumpObject(a);
    DumpObject(b);
    (s ? api.dtor1 : api.dtor)(a, nullptr);
    (s ? api.dtor1 : api.dtor)(b, nullptr);
    DumpObject(a);
}

static void CopyRun(int s)
{
    void* src = MakeAnimation("S");
    unsigned char dst[0x1c];
    std::memset(dst, 0xcc, sizeof dst);
    (s ? api.copy1 : api.copy)(dst, nullptr, src);
    DumpObject(dst);
    DumpChunks(At<void*>(dst, 4), "frames");
    DumpChunks(At<void*>(dst, 8), "xy");
    (s ? api.dtor1 : api.dtor)(dst, nullptr);
}

static void AssignRun(int s)
{
    void* src = MakeAnimation("S");
    void* dst = MakeAnimation("D");
    (s ? api.assign1 : api.assign)(dst, nullptr, src);
    DumpObject(dst);
    DumpChunks(At<void*>(dst, 4), "frames");
    DumpChunks(At<void*>(dst, 8), "xy");
    (s ? api.dtor1 : api.dtor)(dst, nullptr);
}

static void SelfAssignRun(int s)
{
    void* a = MakeAnimation("S");
    (s ? api.assign1 : api.assign)(a, nullptr, a);
    DumpObject(a);
    DumpChunks(At<void*>(a, 4), "frames");
    DumpChunks(At<void*>(a, 8), "xy");
    (s ? api.dtor1 : api.dtor)(a, nullptr);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_ws_u5 <bin dir>\n");
        return 2;
    }
    const std::string dir = argv[1];
    SetDllDirectoryA(dir.c_str());
    // RenderSystem_7c's data mirrors read the renamed original at load: it must be in memory first.
    LoadLibraryA((dir + "\\RenderSystem_7c_orig.dll").c_str());
    mod[0] = LoadLibraryA((dir + "\\WaterSystem_7c.dll").c_str());
    const DWORD err0 = GetLastError();
    mod[1] = LoadLibraryA((dir + "\\WaterSystem_7c_orig.dll").c_str());
    const DWORD err1 = GetLastError();
    HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load the modules from %s (local=%p err=%lu orig=%p err=%lu ks=%p)\n",
                    argv[1], static_cast<void*>(mod[0]), err0,
                    static_cast<void*>(mod[1]), err1, static_cast<void*>(ks));
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    RealNew = reinterpret_cast<NewFn>(GetProcAddress(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    RealDelete = reinterpret_cast<DeleteFn>(GetProcAddress(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    for (int s = 0; s < 2; ++s) {
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
    }
    api.ctor = Fn<Ctor>(0, "??0WaterGeometryAnimation@Jet@@QAE@XZ");
    api.ctor1 = Fn<Ctor>(1, "??0WaterGeometryAnimation@Jet@@QAE@XZ");
    api.copy = Fn<Copy>(0, "??0WaterGeometryAnimation@Jet@@QAE@ABV01@@Z");
    api.copy1 = Fn<Copy>(1, "??0WaterGeometryAnimation@Jet@@QAE@ABV01@@Z");
    api.dtor = Fn<Dtor>(0, "??1WaterGeometryAnimation@Jet@@QAE@XZ");
    api.dtor1 = Fn<Dtor>(1, "??1WaterGeometryAnimation@Jet@@QAE@XZ");
    api.assign = Fn<Assign>(0, "??4WaterGeometryAnimation@Jet@@QAEAAV01@ABV01@@Z");
    api.assign1 = Fn<Assign>(1, "??4WaterGeometryAnimation@Jet@@QAEAAV01@ABV01@@Z");

    gridVt[0] = gridVt[1] = DtorSlot(&GridFake::Dtor);
    geometryVt[0] = DtorSlot(&GeometryFake::ScalarDtor);
    geometryVt[1] = DtorSlot(&GeometryFake::VectorDtor);

    struct {
        const char* name;
        void (*run)(int);
    } tests[] = {{"ctor", CtorRun}, {"copy", CopyRun}, {"assign", AssignRun}, {"self assign", SelfAssignRun}};
    for (const auto& t : tests) {
        RunSide(0, t.run);
        RunSide(1, t.run);
        Compare(t.name);
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
