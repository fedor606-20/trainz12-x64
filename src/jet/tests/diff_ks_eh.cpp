// Differential test of KernelStandard unit U12c (ExceptionHandler.cpp) on its ordinary paths: Main with functions
// that return or throw a Jet::Exception / AssertEx, the critical lists, Init, Assert with prompting off, the copy
// members of HardwareException and the report dialog's list box. The same script runs through
// KernelStandard_7c_orig.dll's exports and through ours; the logs must match. Prompting stays off, so no report is
// ever written or shown.
//
// usage: diff_ks_eh.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll>
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// stand-ins with Jet's class names: the handler's catch clauses match thrown types by name
namespace Jet {
class Exception {
public:
    virtual ~Exception() {}
    unsigned char data[0x110c] = {};
};
}  // namespace Jet
class AssertEx : public Jet::Exception {};

static HMODULE ours, orig;
static HMODULE cur;  // the side running now (its own statics since U14b)

template <class F>
static F Get(HMODULE m, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}

struct Side {
    HMODULE m;
    const char* tag;
    template <class F>
    F f(const char* name) const { return Get<F>(m, name); }
};

static std::vector<long long> logv;
static std::vector<int> lines;
static void Log(long long v, int line)
{
    logv.push_back(v);
    lines.push_back(line);
}
#define L(v) Log(static_cast<long long>(v), __LINE__)

static long long Hash(const void* p, size_t n)
{
    unsigned long long h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 1099511628211ull;
    return static_cast<long long>(h);
}
static void LogText(const char* s, int line)
{
    if (!s) return Log(-1, line);
    Log(static_cast<long long>(std::strlen(s)), line);
    Log(Hash(s, std::strlen(s)), line);
}
#define LT(s) LogText((s), __LINE__)

template <class T>
static T& Data(const char* name) { return *Get<T*>(cur, name); }

using CallBack = int(__cdecl*)(void*);
using CtorP = void*(__fastcall*)(void*, void*, const void*);
using Dtor = void(__fastcall*)(void*, void*);
using Ctor0 = void*(__fastcall*)(void*, void*);

static std::vector<int> calls;
static const Side* current;
static int depth;

static int __cdecl Record(void* arg)
{
    calls.push_back(static_cast<int>(reinterpret_cast<intptr_t>(arg)));
    return 7;
}
static int __cdecl Reenter(void*)  // runs the shutdown list again once from inside it
{
    calls.push_back(100 + depth);
    if (depth++ == 0) current->f<void(__cdecl*)()>("?CriticalShutdown@ExceptionHandler@Jet@@SAXXZ")();
    return 0;
}
static int __cdecl ThrowsInt(void*)
{
    calls.push_back(-1);
    throw 5;
}
static int __cdecl Returns(void* arg) { return 42 + static_cast<int>(reinterpret_cast<intptr_t>(arg)); }
static int __cdecl ThrowsJet(void*) { throw Jet::Exception(); }
static int __cdecl ThrowsAssert(void*) { throw AssertEx(); }

struct Shutdown {
    void* arg;
    CallBack fn;
    Shutdown* next;
};

static void LogList(Shutdown* s, int line)
{
    int n = 0;
    for (; s && n < 50; s = s->next, ++n) Log(reinterpret_cast<intptr_t>(s->arg), line);
    Log(n, line);
}

static void LogCalls(int line)
{
    Log(static_cast<long long>(calls.size()), line);
    for (int c : calls) Log(c, line);
    calls.clear();
}

static void Scenario(const Side& s)
{
    cur = s.m;
    logv.clear();
    lines.clear();
    current = &s;
    Shutdown*& down = Data<Shutdown*>("?criticalShutdown@ExceptionHandler@Jet@@0PAVShutdown@12@A");
    Shutdown*& up = Data<Shutdown*>("?criticalStartup@ExceptionHandler@Jet@@0PAVShutdown@12@A");
    bool& prompt = Data<bool>("?isPromptAssertions@ExceptionHandler@Jet@@0_NA");
    bool& ignore = Data<bool>("?ignore@ExceptionHandler@Jet@@0_NA");
    Shutdown* const down0 = down;
    Shutdown* const up0 = up;
    const bool prompt0 = prompt, ignore0 = ignore;
    down = nullptr;
    up = nullptr;
    prompt = false;
    ignore = false;

    const auto addDown = s.f<void*(__cdecl*)(CallBack, void*)>("?AddCriticalShutdown@ExceptionHandler@Jet@@SAPAXP6AHPAX@Z0@Z");
    const auto addUp = s.f<void*(__cdecl*)(CallBack, void*)>("?AddCriticalStartup@ExceptionHandler@Jet@@SAPAXP6AHPAX@Z0@Z");
    const auto remDown = s.f<void(__cdecl*)(void*)>("?RemoveCriticalShutdown@ExceptionHandler@Jet@@SAXPAX@Z");
    const auto remUp = s.f<void(__cdecl*)(void*)>("?RemoveCriticalStartup@ExceptionHandler@Jet@@SAXPAX@Z");
    const auto runDown = s.f<void(__cdecl*)()>("?CriticalShutdown@ExceptionHandler@Jet@@SAXXZ");
    const auto runUp = s.f<void(__cdecl*)()>("?CriticalStartup@ExceptionHandler@Jet@@SAXXZ");
    const auto main_ =
        s.f<int(__cdecl*)(CallBack, void*, void*)>("?Main@ExceptionHandler@Jet@@SAHP6AHPAX@Z0PAVThread@2@@Z");

    std::printf("%s: lists\n", s.tag);
    // the lists: newest first, removal by handle, running front to back
    void* h[6];
    for (int i = 0; i < 6; ++i) h[i] = addDown(&Record, reinterpret_cast<void*>(static_cast<intptr_t>(i + 1)));
    for (int i = 0; i < 6; ++i) L(h[i] == down || (h[i] != nullptr));
    LogList(down, __LINE__);
    remDown(h[2]);
    remDown(h[5]);  // the head
    remDown(h[0]);  // the tail
    remDown(reinterpret_cast<void*>(0x1234));  // not in the list
    LogList(down, __LINE__);
    runDown();
    LogCalls(__LINE__);
    void* g[3];
    for (int i = 0; i < 3; ++i) g[i] = addUp(&Record, reinterpret_cast<void*>(static_cast<intptr_t>(10 + i)));
    runUp();
    LogCalls(__LINE__);
    LogList(up, __LINE__);
    // re-entry: allowed while the list is not empty
    void* r = addDown(&Reenter, reinterpret_cast<void*>(99));
    depth = 0;
    runDown();
    LogCalls(__LINE__);
    remDown(r);
    // a throwing callback stops the walk (the rest is not called)
    void* t = addDown(&ThrowsInt, nullptr);
    runDown();
    LogCalls(__LINE__);
    remDown(t);
    runDown();
    LogCalls(__LINE__);
    // empty lists
    for (void* x : {h[1], h[3], h[4]}) remDown(x);
    for (void* x : g) remUp(x);
    L(down == nullptr && up == nullptr);
    runDown();
    runUp();
    LogCalls(__LINE__);

    std::printf("%s: main\n", s.tag);
    // Main: a return value, a Jet::Exception (reported: prompting is off, so nothing shows) and an assertion,
    // each followed by the shutdown list; with ignore set the function is not run
    void* k = addDown(&Record, reinterpret_cast<void*>(77));
    for (int i = 0; i < 3; ++i) {
        L(main_(&Returns, reinterpret_cast<void*>(static_cast<intptr_t>(i)), nullptr));
        LogCalls(__LINE__);
    }
    L(main_(&ThrowsJet, nullptr, nullptr));
    LogCalls(__LINE__);
    L(main_(&ThrowsAssert, nullptr, nullptr));
    LogCalls(__LINE__);
    ignore = true;
    L(main_(&Returns, nullptr, nullptr));
    ignore = false;
    LogCalls(__LINE__);
    L(s.f<int(__cdecl*)(void*)>("?ProcessException@ExceptionHandler@Jet@@CAHAAVException@2@@Z")(nullptr));
    remDown(k);

    std::printf("%s: assert\n", s.tag);
    // Assert with prompting off: an AssertEx with the AssertInit file, line and reason
    s.f<void(__cdecl*)(const char*, int, const char*)>("?AssertInit@ExceptionHandler@Jet@@SAXPBDH0@Z")(
        "assert.cpp", 321, "x != 0");
    L(Data<int>("?line@ExceptionHandler@Jet@@0HA"));
    s.f<void(__cdecl*)(bool)>("?SetAssertions@ExceptionHandler@Jet@@SAX_N@Z")(true);
    L(prompt);
    s.f<void(__cdecl*)(bool)>("?SetAssertions@ExceptionHandler@Jet@@SAX_N@Z")(false);
    L(prompt);
    try {
        s.f<bool(__cdecl*)(const char*, ...)>("?Assert@ExceptionHandler@Jet@@SA_NPBDZZ")("value %d", 5);
        L(-1);
    } catch (AssertEx& e) {
        const unsigned char* d = e.data;
        LT(reinterpret_cast<const char*>(d));          // description
        LT(reinterpret_cast<const char*>(d + 0x1000)); // reason
        L(*reinterpret_cast<const int*>(d + 0x1104));
        LT(*reinterpret_cast<const char* const*>(d + 0x1108));
        using ClassF = const char*(__cdecl*)(const void*);
        LT((*reinterpret_cast<ClassF* const*>(&e))[1](&e));
    }

    std::printf("%s: init\n", s.tag);
    // Init: CoreDumps, CorePrefix (10 characters at most), CoreOptions
    char* prefix = Get<char*>(cur, "?corePrefix@ExceptionHandler@Jet@@0PADA");
    bool& dumps = Data<bool>("?doCoreDump@ExceptionHandler@Jet@@0_NA");
    int& opts = Data<int>("?dumpOptions@ExceptionHandler@Jet@@0HA");
    char prefix0[16];
    std::memcpy(prefix0, prefix, sizeof prefix0);
    const bool dumps0 = dumps;
    const int opts0 = opts;
    for (const char* raw : {"", "-CoreDumps=0 -CorePrefix=abc -CoreOptions=3", "-CorePrefix=0123456789xyz",
                            "-CoreDumps -CoreOptions=-7", "-CorePrefix=exactly10c"}) {  // an empty CorePrefix copies from null
        unsigned char o[0xc];
        s.f<Ctor0>("??0Options@Jet@@QAE@XZ")(o, nullptr);
        s.f<void(__cdecl*)(void*, const char*)>("?ProcessRaw@Options@Jet@@QAAXPBD@Z")(o, raw);
        s.f<void(__cdecl*)(const void*)>("?Init@ExceptionHandler@Jet@@SAXPBVOptions@2@@Z")(o);
        LT(prefix);
        L(dumps);
        L(opts);
    }
    std::memcpy(prefix, prefix0, sizeof prefix0);
    dumps = dumps0;
    opts = opts0;

    std::printf("%s: hw\n", s.tag);
    // HardwareException: copies, assignment, class name (on a block made by hand)
    {
        unsigned char a[0x1114], b[0x1114];
        for (int i = 0; i < 0x1114; ++i) a[i] = static_cast<unsigned char>(i * 13 + 1);
        a[0x1003] = 0;
        a[0x1103] = 0;
        *reinterpret_cast<void**>(a) = s.f<void*>("??_7HardwareException@Jet@@6B@");
        std::memset(b, 0x5a, sizeof b);
        s.f<CtorP>("??0HardwareException@Jet@@QAE@ABV01@@Z")(b, nullptr, a);
        L(*reinterpret_cast<void**>(b) == *reinterpret_cast<void**>(a));
        L(Hash(b + 4, sizeof b - 4));
        std::memset(b + 4, 0x33, sizeof b - 4);
        s.f<CtorP>("??4HardwareException@Jet@@QAEAAV01@ABV01@@Z")(b, nullptr, a);
        L(Hash(b + 4, sizeof b - 4));
        LT(s.f<const char*(__cdecl*)(const void*)>("?Class@HardwareException@Jet@@UBAPBDXZ")(b));
        s.f<Dtor>("??1HardwareException@Jet@@UAE@XZ")(b, nullptr);
        L(*reinterpret_cast<void**>(b) == s.f<void*>("??_7Exception@Jet@@6B@"));
    }

    std::printf("%s: dialog\n", s.tag);
    // the report dialog's list box, on a hidden window
    {
        HWND dlg = CreateWindowExA(0, "STATIC", "", WS_POPUP, 0, 0, 400, 300, nullptr, nullptr, nullptr, nullptr);
        HWND list = CreateWindowExA(0, "LISTBOX", "", WS_CHILD | WS_HSCROLL | LBS_NOINTEGRALHEIGHT, 0, 0, 380, 280, dlg,
                                    reinterpret_cast<HMENU>(0x3ea), nullptr, nullptr);
        HDC dc = GetDC(list);
        const auto logLine = s.f<void(__cdecl*)(void*, void*, const char*, ...)>("?LogLine@ExceptionHandler@Jet@@SAXPAX0PBDZZ");
        logLine(dlg, dc, "one\r\ntwo %d\r\n\r\nthree, a much longer line than the others %s\r\n", 2, "!");
        L(SendMessageA(list, LB_GETHORIZONTALEXTENT, 0, 0));
        logLine(dlg, dc, "");
        L(SendMessageA(list, LB_GETHORIZONTALEXTENT, 0, 0));
        logLine(dlg, dc, "no line break");
        L(SendMessageA(list, LB_GETHORIZONTALEXTENT, 0, 0));
        logLine(dlg, dc, "a long first line, wider than the next one\r\nshort\r\nmid-sized line");
        L(SendMessageA(list, LB_GETHORIZONTALEXTENT, 0, 0));
        ReleaseDC(list, dc);
        unsigned char e[0x1110] = {};
        *reinterpret_cast<void**>(e) = s.f<void*>("??_7Exception@Jet@@6B@");
        std::strcpy(reinterpret_cast<char*>(e + 4), "stack line 1\r\nstack line 2\r\n");
        std::strcpy(reinterpret_cast<char*>(e + 0x1004), "a reason");
        *reinterpret_cast<int*>(e + 0x1108) = 12;
        *reinterpret_cast<const char**>(e + 0x110c) = "file.cpp";
        s.f<void(__cdecl*)(void*, void*)>("?InitExceptionDialog@ExceptionHandler@Jet@@SAXPAXAAVException@2@@Z")(dlg, e);
        const int n = static_cast<int>(SendMessageA(list, LB_GETCOUNT, 0, 0));
        L(n);
        for (int i = 0; i < n; ++i) {
            char text[512] = {};
            SendMessageA(list, LB_GETTEXT, i, reinterpret_cast<LPARAM>(text));
            LT(text);
        }
        L(SendMessageA(list, LB_GETHORIZONTALEXTENT, 0, 0));
        DestroyWindow(dlg);
    }

    down = down0;
    up = up0;
    prompt = prompt0;
    ignore = ignore0;
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* e) -> LONG {
        HMODULE m = nullptr;
        char mod[MAX_PATH] = "?";
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCSTR>(e->ExceptionRecord->ExceptionAddress), &m);
        if (m) GetModuleFileNameA(m, mod, MAX_PATH);
        std::printf("stopped: %08lx in %s +%lx after line %d\n", e->ExceptionRecord->ExceptionCode, mod,
                    static_cast<unsigned long>(static_cast<char*>(e->ExceptionRecord->ExceptionAddress) - reinterpret_cast<char*>(m)),
                    lines.empty() ? 0 : lines.back());
        return EXCEPTION_EXECUTE_HANDLER;
    });
    SetDllDirectoryA(argv[1]);
    // the original first: our KernelStandardInit calls the msvcr80 setlocale through Crt80, and only the
    // original module's import table pulls msvcr80.dll into the process (loading ours first in a bare
    // process answers ERROR_DLL_INIT_FAILED, same quirk as diff_r3d_u6)
    orig = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c_orig.dll").c_str());
    ours = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c.dll").c_str());
    if (!ours || !orig || ours == orig) return 2;
    Scenario({orig, "orig"});
    const std::vector<long long> a = logv;
    const std::vector<int> la = lines;
    Scenario({ours, "ours"});
    const std::vector<long long>& b = logv;
    size_t bad = 0;
    for (size_t i = 0; i < a.size() || i < b.size(); ++i) {
        const long long x = i < a.size() ? a[i] : -999, y = i < b.size() ? b[i] : -999;
        if (x != y && ++bad <= 10)
            std::printf("  step %zu (line %d): orig %lld ours %lld\n", i, i < la.size() ? la[i] : 0, x, y);
    }
    std::printf("exception handler scenario: %zu results each, %zu mismatches\n", a.size(), bad);
    return bad ? 1 : 0;
}
