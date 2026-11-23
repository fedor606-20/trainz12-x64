// Differential test of RenderSystem_7c unit U2b: ExtendedGfxLanguage (ExtendedGfxLanguage.cpp) and the texture
// combiner's parse records, ours against RenderSystem_7c_orig.dll in one process.
//
// As in diff_rs_u2: both modules' imports of Jet::Alloc::New / Delete are hooked (allocations numbered, frees
// logged by name), each scenario runs the same random script on both sides with that side's exports, and the
// logs must be equal. A language object gets a copy of its vftable whose slot 8 (the derived class's Load) logs
// what it is handed; LoadFromFile reads files from %TEMP% through a stand-in ResourceDatabase.
//
// usage: diff_rs_u2b.exe <bin dir with RenderSystem_7c.dll, RenderSystem_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

static HMODULE mod[2];  // 0 ours, 1 the original
static HMODULE ks;
static unsigned long long checks, bad;
static std::mt19937 rng(9300);

static void* Proc(HMODULE m, const char* name)
{
    void* p = reinterpret_cast<void*>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}
template <class T>
static T As(void* p) { return reinterpret_cast<T>(p); }

// ---- KernelStandard and the heap hooks

using NewFn = void*(__cdecl*)(unsigned);
using DeleteFn = void(__cdecl*)(void*);
static NewFn RealNew;
static DeleteFn RealDelete;
static void* (*CreateNode)(void*, const unsigned char*);
static void (*DestroyNode)(void*, void*);
static void** cache;
static void*(__fastcall* AnsiFromText)(void*, void*, const char*);
static void(__fastcall* AnsiDtor)(void*, void*);
static const char*(__cdecl* AnsiText)(const void*);
static bool(__fastcall* ReleaseRef)(const void*);
static void(__fastcall* AddRef)(const void*);

static int cur = -1;  // the side running
static std::string logText[2];
static std::map<const void*, std::string> names[2];  // pointer -> name, per side
static int allocCount[2];

static void Log(const char* fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (cur >= 0) logText[cur] += buf;
}

static std::string Name(const void* p)
{
    if (!p) return "null";
    if (cur < 0) return "?";  // (nothing is logged then)
    auto it = names[cur].find(p);
    if (it != names[cur].end()) return it->second;
    return "?";
}

static void* __cdecl HookNew(unsigned n)
{
    void* p = RealNew(n);
    std::memset(p, 0xcd, n);
    if (cur >= 0) {
        char b[32];
        std::snprintf(b, sizeof b, "A%d", allocCount[cur]++);
        names[cur][p] = b;
        Log("  new %u -> %s\n", n, b);
    }
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
        auto* nm = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; nm->u1.AddressOfData; ++nm, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(nm->u1.Ordinal)) continue;
            if (std::strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + nm->u1.AddressOfData)->Name, name)) continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            iat->u1.Function = reinterpret_cast<uintptr_t>(fn);
            VirtualProtect(&iat->u1.Function, sizeof(void*), old, &old);
        }
    }
}

// a test object: memory from the real heap, filled, named
static void* TestNew(unsigned n, const char* fmt, int k)
{
    void* p = RealNew(n);
    std::memset(p, 0xcd, n);
    char b[32];
    std::snprintf(b, sizeof b, fmt, k);
    names[cur][p] = b;
    return p;
}
static void TestDelete(void* p)
{
    names[cur].erase(p);
    RealDelete(p);
}

// ---- strings: interned names (one reference held for the whole run), AnsiStrings in 4-byte boxes

struct Node {
    void* next;
    void* prev;
    unsigned length, capacity;
    int refs;
    unsigned id;
    char text[1];
};
static const char* kTexts[] = {"vp",  "tex.txt", "a.txt", ".txt", "txt", "x", "c0",   "lightDir",
                               "pos", "ab.txt",  "q.TXT", "",     "z.txtx", "fog", "built-in"};
constexpr int kNames = sizeof kTexts / sizeof *kTexts;
static void* nodes[kNames];
static std::string NodeText(const void* n)
{
    if (!n) return "null";
    const Node* x = static_cast<const Node*>(n);
    char b[64];
    std::snprintf(b, sizeof b, "\"%s\"/%u", x->text, x->length);
    return b;
}
static void* NewAnsi(const char* s)
{
    void* p = RealNew(4);
    AnsiFromText(p, nullptr, s);
    return p;
}
static void FreeAnsi(void* p)
{
    AnsiDtor(p, nullptr);
    RealDelete(p);
}

// ---- per-side entry points

#define F(name) Proc(mod[s], name)
#define EGL "ExtendedGfxLanguage@Jet@@"

struct Api {
    void*(__fastcall* ctor)(void*, void*, void*, int);
    void(__fastcall* vbaseDtor)(void*);
    bool(__cdecl* supported)(void*, float);
    bool(__cdecl* loadProgram)(void*, const void*, const char*, const void*);
    bool(__cdecl* loadFromFile)(void*, const void*, const void*, const void*, const void*);
    bool(__cdecl* loadNative)(void*, const void*, const char*);
    bool(__cdecl* compile)(void*);
    bool(__cdecl* bind)(const void*);
    bool(__cdecl* end)(const void*);
    void*(__cdecl* getDisplay)(const void*);
    const void*(__cdecl* getSource)(const void*);
    const void*(__cdecl* getFilename)(const void*);
    const void*(__cdecl* getProgramName)(const void*);
    const void*(__cdecl* getRegisteredName)(const void*);
    void(__cdecl* setFilename)(void*, const void*);
    void(__cdecl* setProgramName)(void*, const void*);
    void(__cdecl* setRegisteredName)(void*, const void*);
    float(__cdecl* getGeneration)(const void*);
    void(__cdecl* setGeneration)(void*, float);
    unsigned long(__cdecl* getUserData)(const void*);
    void(__cdecl* setUserData)(const void*, unsigned long);
    const void*(__cdecl* getFallback)(const void*);
    void(__cdecl* setFallback)(const void*, const void*);
    bool(__cdecl* getSymbolId)(const void*, const void*, void*);
    int(__cdecl* getLocalConstant)(const void*, const void*);
    const void*(__fastcall* getSymbolNameList)(const void*);
    void(__cdecl* initInstance)(const void*, void*);
    void(__cdecl* processDefines)(const void*, void*, const void*);
    void(__cdecl* addSymbolName)(void*, const void*, int, bool, int);
    void** vftable;
    void** vftableRc;
    const int* vbtable;
    // texture combiner records
    void*(__fastcall* modCtor)(void*, void*, int);
    void*(__fastcall* modAssign)(void*, void*, const void*);
    void*(__fastcall* regCtor)(void*, void*, int, int);
    void*(__fastcall* regCopy)(void*, void*, const void*);
    void(__fastcall* regDtor)(void*, void*);
    void*(__fastcall* regAssign)(void*, void*, const void*);
    void*(__fastcall* insCtor)(void*, void*, int, void*, void*, int, float, bool);
    void*(__fastcall* insCopy)(void*, void*, const void*);
    void*(__fastcall* insDeep)(void*, void*, const void*);
    void*(__fastcall* insAssign)(void*, void*, const void*);
    void(__fastcall* insDtor)(void*, void*);
    void** insVftable;
} api[2];

static void LoadApi(int s)
{
    Api& a = api[s];
    a.ctor = As<decltype(a.ctor)>(F("??0" EGL "IAE@PAVDisplay@1@@Z"));
    a.vbaseDtor = As<decltype(a.vbaseDtor)>(F("??_D" EGL "IAEXXZ"));
    a.loadProgram = As<decltype(a.loadProgram)>(F("?LoadProgram@" EGL "UAA_NABVPString@2@PBDPBUPreProcessorArray@2@@Z"));
    a.loadFromFile = As<decltype(a.loadFromFile)>(F("?LoadFromFile@" EGL "UAA_NPBVResourceDatabase@2@ABVPString@2@1PBUPreProcessorArray@2@@Z"));
    a.loadNative = As<decltype(a.loadNative)>(F("?LoadNativeFormat@" EGL "UAA_NABVPString@2@PBD@Z"));
    a.compile = As<decltype(a.compile)>(F("?CompileProgram@" EGL "UAA_NXZ"));
    a.bind = As<decltype(a.bind)>(F("?Bind@" EGL "UBA_NXZ"));
    a.end = As<decltype(a.end)>(F("?End@" EGL "UBA_NXZ"));
    a.getDisplay = As<decltype(a.getDisplay)>(F("?GetDisplay@" EGL "QBAPAVDisplay@2@XZ"));
    a.getSource = As<decltype(a.getSource)>(F("?GetProgramString@" EGL "QBAABVAnsiString@2@XZ"));
    a.getFilename = As<decltype(a.getFilename)>(F("?GetFilename@" EGL "QBAABVPString@2@XZ"));
    a.getProgramName = As<decltype(a.getProgramName)>(F("?GetProgramName@" EGL "QBAABVPString@2@XZ"));
    a.getRegisteredName = As<decltype(a.getRegisteredName)>(F("?GetRegisteredName@" EGL "QBAABVPString@2@XZ"));
    a.setFilename = As<decltype(a.setFilename)>(F("?SetFilename@" EGL "QAAXABVPString@2@@Z"));
    a.setProgramName = As<decltype(a.setProgramName)>(F("?SetProgramName@" EGL "QAAXABVPString@2@@Z"));
    a.setRegisteredName = As<decltype(a.setRegisteredName)>(F("?SetRegisteredName@" EGL "QAAXABVPString@2@@Z"));
    a.getGeneration = As<decltype(a.getGeneration)>(F("?GetGeneration@" EGL "QBAMXZ"));
    a.setGeneration = As<decltype(a.setGeneration)>(F("?SetGeneration@" EGL "QAAXM@Z"));
    a.getUserData = As<decltype(a.getUserData)>(F("?GetUserData@" EGL "QBAKXZ"));
    a.setUserData = As<decltype(a.setUserData)>(F("?SetUserData@" EGL "QBAXK@Z"));
    a.getFallback = As<decltype(a.getFallback)>(F("?GetFallbackProgram@" EGL "QBAPBV12@XZ"));
    a.setFallback = As<decltype(a.setFallback)>(F("?SetFallbackProgram@" EGL "QBAXPBV12@@Z"));
    a.getSymbolId = As<decltype(a.getSymbolId)>(F("?GetSymbolId@" EGL "QBA_NABVPString@2@AAUGfxSymbolId@2@@Z"));
    a.getLocalConstant = As<decltype(a.getLocalConstant)>(F("?GetLocalConstantNumber@" EGL "QBAHABVPString@2@@Z"));
    a.getSymbolNameList = As<decltype(a.getSymbolNameList)>(F("?GetSymbolNameList@" EGL "QBEABV?$Array@USymbolName@ExtendedGfxLanguage@Jet@@$02@2@XZ"));
    a.initInstance = As<decltype(a.initInstance)>(F("?InitInstance@" EGL "IBAXAAV12@@Z"));
    a.processDefines = As<decltype(a.processDefines)>(F("?ProcessDefines@" EGL "IBAXAAVAnsiString@2@PBUPreProcessorArray@2@@Z"));
    a.addSymbolName = As<decltype(a.addSymbolName)>(F("?AddSymbolName@" EGL "IAAXABVPString@2@H_NH@Z"));
    a.vftable = static_cast<void**>(F("??_7" EGL "6B01@@"));
    a.vftableRc = static_cast<void**>(F("??_7" EGL "6BReferenceCount@1@@"));
    a.vbtable = static_cast<const int*>(F("??_8" EGL "7B@"));
    a.supported = As<decltype(a.supported)>(a.vftable[0]);
#define TC "TextureCombiner@Jet@@"
    a.modCtor = As<decltype(a.modCtor)>(F("??0ModifierItem@" TC "QAE@W4ModifierType@12@@Z"));
    a.modAssign = As<decltype(a.modAssign)>(F("??4ModifierItem@" TC "QAEAAU012@ABU012@@Z"));
    a.regCtor = As<decltype(a.regCtor)>(F("??0RegisterItem@" TC "QAE@W4RegType@12@H@Z"));
    a.regCopy = As<decltype(a.regCopy)>(F("??0RegisterItem@" TC "QAE@PBU012@@Z"));
    a.regDtor = As<decltype(a.regDtor)>(F("??1RegisterItem@" TC "QAE@XZ"));
    a.regAssign = As<decltype(a.regAssign)>(F("??4RegisterItem@" TC "QAEAAU012@ABU012@@Z"));
    a.insCtor = As<decltype(a.insCtor)>(F("??0Instruction@" TC "QAE@W4TextureOpcode@12@PAU?$ParseList@URegisterItem@TextureCombiner@Jet@@@2@PAU?$ParseList@UModifierItem@TextureCombiner@Jet@@@2@W4TextureAddressType@12@M_N@Z"));
    a.insCopy = As<decltype(a.insCopy)>(F("??0Instruction@" TC "QAE@ABU012@@Z"));
    a.insDeep = As<decltype(a.insDeep)>(F("??0Instruction@" TC "QAE@PBU012@@Z"));
    a.insAssign = As<decltype(a.insAssign)>(F("??4Instruction@" TC "QAEAAU012@ABU012@@Z"));
    a.insDtor = As<decltype(a.insDtor)>(F("??1Instruction@" TC "UAE@XZ"));
    a.insVftable = static_cast<void**>(F("??_7Instruction@" TC "6B@"));
}

// ---------------------------------------------------------------------------------------------------------------
// languages

constexpr unsigned kLangSize = 0x3c;
#define TAIL " ;;;;;;;;;;;;;;;;"
static void* testVtable[2][9];  // per side: the module's slots, slot 8 our logging Load
static int loadCount;

static std::string MemText(const void* mf)
{
    const char* start = *reinterpret_cast<const char* const*>(static_cast<const char*>(mf) + 4);
    const unsigned size = *reinterpret_cast<const unsigned*>(static_cast<const char*>(mf) + 8);
    if (!start) return "(no data)";
    return std::string(start, start + (size > 300 ? 300 : size)) + "|" + std::to_string(size);
}

static bool __cdecl LoggingLoad(void* self, void* file, const void* name, const void* fileName)
{
    Log("  Load %s [%s] name %s file %s cur %u\n", Name(self).c_str(), MemText(file).c_str(),
        NodeText(*static_cast<void* const*>(name)).c_str(), NodeText(*static_cast<void* const*>(fileName)).c_str(),
        *reinterpret_cast<const char* const*>(file) - *reinterpret_cast<const char* const*>(static_cast<const char*>(file) + 4));
    return ++loadCount % 3 != 0;
}

// the stand-in ResourceDatabase: slot 3 maps a name to a file in %TEMP% (a node the caller owns)
static std::string tempDir;
static void* __cdecl FakeGetFileName(const void* db, void** out, const void* name)
{
    const void* n = *static_cast<void* const*>(name);
    std::string path = tempDir + "rs_u2b_" + (n ? static_cast<const Node*>(n)->text : "null") + ".src";
    Log("  db %s GetFileName %s\n", Name(db).c_str(), NodeText(n).c_str());
    *out = CreateNode(*cache, reinterpret_cast<const unsigned char*>(path.c_str()));
    return out;
}
static void* dbVtable[4] = {nullptr, nullptr, nullptr, reinterpret_cast<void*>(&FakeGetFileName)};
static struct {
    void** vt = dbVtable;
} fakeDb;

static void DumpLang(const char* tag, const void* l)
{
    const auto* b = static_cast<const unsigned char*>(l);
    auto u32 = [&](int off) { return *reinterpret_cast<const unsigned*>(b + off); };
    auto ptr = [&](int off) { return *reinterpret_cast<void* const*>(b + off); };
    float g;
    std::memcpy(&g, b + 0x1c, 4);
    Log("  %s %s: vft %s vbt %s disp %s src \"%s\" file %s prog %s reg %s gen %g user %u fb %s rc %u\n", tag,
        Name(l).c_str(), ptr(0) == testVtable[cur] ? "test" : ptr(0) == api[cur].vftable ? "own" : "?",
        ptr(4) == api[cur].vbtable ? "own" : "?", Name(ptr(8)).c_str(), AnsiText(b + 0xc), NodeText(ptr(0x10)).c_str(),
        NodeText(ptr(0x14)).c_str(), NodeText(ptr(0x18)).c_str(), g, u32(0x2c), Name(ptr(0x30)).c_str(), u32(0x38));
    const unsigned n = u32(0x20), cap = u32(0x24);
    const auto* data = static_cast<const unsigned char*>(ptr(0x28));
    Log("    symbols %u/%u %s:", n, cap, Name(data).c_str());
    for (unsigned i = 0; i < n; ++i) {
        const auto* e = data + 16 * i;
        Log(" %s:%d,%d,%d", NodeText(*reinterpret_cast<void* const*>(e)).c_str(), e[4],
            *reinterpret_cast<const int*>(e + 8), *reinterpret_cast<const int*>(e + 12));
    }
    Log("\n");
}

static void RunLanguages(int s, unsigned seed)
{
    std::mt19937 r(seed);
    auto P = [&](int lo, int hi) { return lo + static_cast<int>(r() % static_cast<unsigned>(hi - lo + 1)); };
    Api& a = api[s];
    loadCount = 0;
    std::memcpy(testVtable[s], a.vftable, sizeof testVtable[s]);
    testVtable[s][8] = reinterpret_cast<void*>(&LoggingLoad);
    names[s][&fakeDb] = "db";
    void* display = TestNew(4, "Display%d", 0);
    void* lang[3];
    for (int i = 0; i < 3; ++i) {
        lang[i] = TestNew(kLangSize, "L%d", i);
        a.ctor(lang[i], nullptr, i == 2 ? nullptr : display, 1);
        if (i < 2) *static_cast<void**>(lang[i]) = testVtable[s];
        DumpLang("new", lang[i]);
    }
    auto Rc = [&](void* l) { return static_cast<char*>(l) + a.vbtable[1] + 4; };
    auto AnyName = [&]() { return &nodes[P(0, kNames - 1)]; };
    auto SymName = [&]() { return &nodes[P(4, 8)]; };  // a few names, so lookups find what was added
    // each with a tail longer than any value: ProcessDefines resumes its search one value length past the end
    // of each replacement, which must stay inside the text
    const char* kSources[] = {"mov o0, v0\nfog\n" TAIL, "lightDir c0 x x" TAIL, TAIL, "x" TAIL, "aaaa x aaaa" TAIL,
                              "vp fog vp vp" TAIL, "built-in lightDir" TAIL};
    const char* kDefNames[] = {"x", "fog", "vp", "lightDir", "aa", "c0", "q"};
    const char* kDefValues[] = {"", "y", "longer value", "x", "fog", "c0 c0", "vpvp"};
    // define arrays: {count, capacity, data} with data = AnsiString pairs
    auto NewDefines = [&](int n) {
        auto* arr = static_cast<unsigned*>(TestNew(12, "D%d", n));
        arr[0] = n;
        arr[1] = n + P(0, 2);
        void** data = n ? static_cast<void**>(RealNew(8 * arr[1])) : nullptr;
        for (int i = 0; i < n; ++i) {
            AnsiFromText(&data[2 * i], nullptr, kDefNames[P(0, 6)]);
            AnsiFromText(&data[2 * i + 1], nullptr, kDefValues[P(0, 6)]);
        }
        arr[2] = reinterpret_cast<unsigned>(data);
        return arr;
    };
    auto FreeDefines = [&](unsigned* arr) {
        void** data = reinterpret_cast<void**>(arr[2]);
        for (unsigned i = 0; i < 2 * arr[0]; ++i) AnsiDtor(&data[i], nullptr);
        if (data) RealDelete(data);
        TestDelete(arr);
    };
    for (int step = 0; step < 40; ++step) {
        void* l = lang[P(0, 2)];
        const int op = P(0, 22);
        Log("op %d on %s\n", op, Name(l).c_str());
        switch (op) {
        case 0: a.setFilename(l, AnyName()); break;
        case 1: a.setProgramName(l, AnyName()); break;
        case 2: a.setRegisteredName(l, AnyName()); break;
        case 3: {
            float g = static_cast<float>(P(0, 40)) / 8.0f;
            a.setGeneration(l, g);
            Log("  gen %g\n", a.getGeneration(l));
            break;
        }
        case 4: a.setUserData(l, r()); Log("  user %lu\n", a.getUserData(l)); break;
        case 5: {
            void* f = P(0, 3) ? lang[P(0, 2)] : nullptr;
            a.setFallback(l, f);
            Log("  fallback %s\n", Name(a.getFallback(l)).c_str());
            break;
        }
        case 6:
        case 7: a.addSymbolName(l, SymName(), P(-2, 9), P(0, 1) != 0, P(-1, 3)); break;
        case 8: {
            unsigned char id[12];
            std::memset(id, 0xee, sizeof id);
            bool ok = a.getSymbolId(l, SymName(), id);
            Log("  id %d %u %d %d\n", ok, id[0], *reinterpret_cast<int*>(id + 4), *reinterpret_cast<int*>(id + 8));
            Log("  local %d\n", a.getLocalConstant(l, SymName()));
            break;
        }
        case 9: {
            const void* list = a.getSymbolNameList(l);
            Log("  list +%d\n", static_cast<int>(static_cast<const char*>(list) - static_cast<const char*>(l)));
            Log("  disp %s src %s\n", Name(a.getDisplay(l)).c_str(), AnsiText(a.getSource(l)));
            Log("  names %s %s %s\n", NodeText(*static_cast<void* const*>(a.getFilename(l))).c_str(),
                NodeText(*static_cast<void* const*>(a.getProgramName(l))).c_str(),
                NodeText(*static_cast<void* const*>(a.getRegisteredName(l))).c_str());
            break;
        }
        case 10: {
            void* to = lang[P(0, 2)];
            if (to == l) break;
            a.initInstance(l, to);
            DumpLang("copy", to);
            break;
        }
        case 11:
        case 12: {
            void* text = NewAnsi(kSources[P(0, 6)]);
            unsigned* d = P(0, 4) ? NewDefines(P(0, 4)) : nullptr;
            // shared or not: a second AnsiString on the same block sees the in-place writes
            void* twin = nullptr;
            if (P(0, 1)) {
                twin = RealNew(4);
                std::memcpy(twin, text, 4);
                ++*(*reinterpret_cast<int**>(twin) - 1);
            }
            a.processDefines(l, text, d);
            Log("  defines -> \"%s\"%s%s\n", AnsiText(text), twin ? " twin " : "", twin ? AnsiText(twin) : "");
            if (twin) FreeAnsi(twin);
            FreeAnsi(text);
            if (d) FreeDefines(d);
            break;
        }
        case 13:
        case 14: {
            unsigned* d = P(0, 2) ? NewDefines(P(0, 3)) : nullptr;
            bool ok = a.loadProgram(l, AnyName(), kSources[P(0, 6)], d);
            Log("  loadProgram %d\n", ok);
            if (d) FreeDefines(d);
            break;
        }
        case 15:
        case 16: {
            unsigned* d = P(0, 2) ? NewDefines(P(0, 3)) : nullptr;
            bool ok = a.loadFromFile(l, &fakeDb, AnyName(), AnyName(), d);
            Log("  loadFromFile %d\n", ok);
            if (d) FreeDefines(d);
            break;
        }
        case 17:
            Log("  stubs %d %d %d %d %d\n", a.supported(l, 1.5f), a.loadNative(l, AnyName(), "x"), a.compile(l),
                a.bind(l), a.end(l));
            break;
        case 18: AddRef(Rc(l)); Log("  addref\n"); break;
        case 19: AddRef(Rc(l)); Log("  release %d\n", ReleaseRef(Rc(l))); break;
        default: break;
        }
        DumpLang("L", l);
    }
    // the fallbacks hold references: keep only those to a later object (no cycles), so deleting an object
    // releases its fallback; then delete each through its reference count
    for (int i = 0; i < 3; ++i) {
        const void* f = a.getFallback(lang[i]);
        int j = 0;
        while (j < 3 && lang[j] != f) ++j;
        if (f && j <= i) a.setFallback(lang[i], nullptr);
    }
    for (void* l : lang) {
        const unsigned rc = *reinterpret_cast<unsigned*>(Rc(l) + 4);
        Log("refs %s %u\n", Name(l).c_str(), rc);  // less one for each deleted object it was the fallback of
        for (unsigned i = 1; i < rc; ++i) ReleaseRef(Rc(l));
        Log("final release %s %d\n", Name(l).c_str(), ReleaseRef(Rc(l)));
    }
    TestDelete(display);
}

// ---------------------------------------------------------------------------------------------------------------
// texture combiner records

static void DumpMods(const void* list)
{
    if (!list) {
        Log(" -");
        return;
    }
    const auto* l = static_cast<void* const*>(list);
    Log(" [%s %s/%s #%d:", Name(list).c_str(), Name(l[0]).c_str(), Name(l[1]).c_str(), reinterpret_cast<const int*>(l)[2]);
    for (auto* m = static_cast<const int*>(l[0]); m; m = reinterpret_cast<const int*>(m[1]))
        Log(" %s=%d", Name(m).c_str(), m[0]);
    Log("]");
}
static void DumpReg(const void* reg)
{
    const auto* r = static_cast<const int*>(reg);
    Log(" {%s %d %d", Name(reg).c_str(), r[0], r[1]);
    DumpMods(reinterpret_cast<void*>(r[2]));
    DumpMods(reinterpret_cast<void*>(r[3]));
    Log(" next %s}", Name(reinterpret_cast<void*>(r[4])).c_str());
}
static void DumpIns(const char* tag, const void* ins)
{
    const auto* w = static_cast<const int*>(ins);
    float v;
    std::memcpy(&v, w + 10, 4);
    Log("  %s %s vft %s op %d addr %d flag %d f18 %x %x %x %x value %g\n   regs", tag, Name(ins).c_str(),
        reinterpret_cast<void* const*>(ins)[0] == api[cur].insVftable ? "own" : "?", w[1], w[4],
        static_cast<const unsigned char*>(ins)[0x14], w[6], w[7], w[8], w[9], v);
    if (const auto* l = reinterpret_cast<void* const*>(w[2])) {
        Log(" [%s #%d:", Name(l).c_str(), reinterpret_cast<const int*>(l)[2]);
        for (const void* r = l[0]; r; r = reinterpret_cast<void* const*>(r)[4]) DumpReg(r);
        Log("]");
    } else {
        Log(" -");
    }
    Log("\n   mods");
    DumpMods(reinterpret_cast<void*>(w[3]));
    Log("\n");
}

static void RunRecords(int s, unsigned seed)
{
    std::mt19937 r(seed);
    auto P = [&](int lo, int hi) { return lo + static_cast<int>(r() % static_cast<unsigned>(hi - lo + 1)); };
    Api& a = api[s];
    int k = 0;
    auto List = [&]() {
        auto* l = static_cast<void**>(TestNew(12, "List%d", k++));
        l[0] = l[1] = nullptr;
        l[2] = nullptr;
        return l;
    };
    auto Push = [&](void** l, void* item, int nextOff) {
        if (!l[1])
            l[0] = item;
        else
            *reinterpret_cast<void**>(static_cast<char*>(l[1]) + nextOff) = item;
        ++reinterpret_cast<int*>(l)[2];
        l[1] = item;
    };
    auto Mods = [&]() -> void** {
        if (!P(0, 2)) return nullptr;
        void** l = List();
        for (int n = P(0, 3); n; --n) {
            void* m = TestNew(8, "M%d", k++);
            a.modCtor(m, nullptr, P(0, 6));
            Push(l, m, 4);
        }
        return l;
    };
    auto Reg = [&]() {
        void* x = TestNew(0x14, "R%d", k++);
        a.regCtor(x, nullptr, P(0, 9), P(-1, 7));
        reinterpret_cast<void**>(x)[2] = Mods();
        reinterpret_cast<void**>(x)[3] = Mods();
        return x;
    };
    for (int round = 0; round < 6; ++round) {
        void** regs = P(0, 3) ? List() : nullptr;
        if (regs)
            for (int n = P(0, 4); n; --n) Push(regs, Reg(), 0x10);
        void* ins = TestNew(0x2c, "I%d", k++);
        a.insCtor(ins, nullptr, P(0, 30), regs, Mods(), P(0, 4), static_cast<float>(P(-8, 8)) / 4, P(0, 1) != 0);
        DumpIns("ins", ins);
        // deep and shallow copies
        void* deep = TestNew(0x2c, "I%d", k++);
        a.insDeep(deep, nullptr, ins);
        DumpIns("deep", deep);
        void* shallow = TestNew(0x2c, "I%d", k++);
        a.insCopy(shallow, nullptr, deep);
        DumpIns("shallow", shallow);
        void* assigned = TestNew(0x2c, "I%d", k++);
        std::memset(assigned, 0x5a, 0x2c);
        a.insAssign(assigned, nullptr, ins);
        DumpIns("assigned", assigned);
        TestDelete(assigned);
        TestDelete(shallow);
        // register copies
        if (regs && regs[0]) {
            void* rc = TestNew(0x14, "R%d", k++);
            a.regCopy(rc, nullptr, regs[0]);
            Log("  regcopy");
            DumpReg(rc);
            Log("\n");
            void* ra = TestNew(0x14, "R%d", k++);
            a.regAssign(ra, nullptr, rc);
            Log("  regassign");
            DumpReg(ra);
            Log("\n");
            TestDelete(ra);
            a.regDtor(rc, nullptr);
            TestDelete(rc);
            void* m1 = TestNew(8, "M%d", k++);
            void* m2 = TestNew(8, "M%d", k++);
            a.modCtor(m1, nullptr, 5);
            a.modCtor(m2, nullptr, 2);
            reinterpret_cast<void**>(m2)[1] = m1;
            a.modAssign(m1, nullptr, m2);
            Log("  modassign %d %s\n", static_cast<int*>(m1)[0], Name(reinterpret_cast<void**>(m1)[1]).c_str());
            TestDelete(m1);
            TestDelete(m2);
        }
        // destruction: the deep copy through its deleting destructor, the original in place
        if (P(0, 1)) {
            using Del = void*(__fastcall*)(void*, void*, unsigned);
            reinterpret_cast<Del>((*static_cast<void***>(deep))[0])(deep, nullptr, 1);
        } else {
            a.insDtor(deep, nullptr);
            DumpIns("dead", deep);
            TestDelete(deep);
        }
        a.insDtor(ins, nullptr);
        DumpIns("dead", ins);
        TestDelete(ins);
    }
}

// ---------------------------------------------------------------------------------------------------------------

static const char* dumpPart;  // argv[3]: print this part's first log (to look at what a part does)
static void Compare(const char* part, int round)
{
    ++checks;
    if (dumpPart && round < 6 && !std::strcmp(dumpPart, part)) std::printf("%s", logText[1].c_str());
    if (logText[0] == logText[1]) return;
    if (++bad > 8) return;
    const std::string &x = logText[0], &y = logText[1];
    size_t i = 0;
    while (i < x.size() && i < y.size() && x[i] == y[i]) ++i;
    size_t from = x.rfind("op ", i);
    if (from == std::string::npos || i - from > 2000) from = i > 400 ? i - 400 : 0;
    std::printf("MISMATCH %s round %d\n-- ours:\n%s\n-- original:\n%s\n", part, round,
                x.substr(from, i - from + 400).c_str(), y.substr(from, i - from + 400).c_str());
}

// ---------------------------------------------------------------------------------------------------------------
// the shader-language parsers (internal to the module): the original's entry points by address, ours by the names
// the linker map gives them (RenderSystem_7c.map next to the DLL)

static std::string binDir;
static void* OurSym(const char* name)
{
    FILE* f = std::fopen((binDir + "\\RenderSystem_7c.map").c_str(), "r");
    if (!f) {
        std::printf("RenderSystem_7c.map missing next to the DLL\n");
        std::exit(2);
    }
    char line[1024];
    unsigned long va = 0;
    while (std::fgets(line, sizeof line, f)) {
        char sym[512];
        unsigned sec, off;
        unsigned long v;
        if (std::sscanf(line, " %x:%x %511s %lx", &sec, &off, sym, &v) == 4 && !std::strcmp(sym, name)) {
            va = v;
            break;
        }
    }
    std::fclose(f);
    if (!va) {
        std::printf("%s not in the map\n", name);
        std::exit(2);
    }
    return reinterpret_cast<char*>(mod[0]) + (va - 0x10000000);
}
static void* OrigAt(unsigned rva) { return reinterpret_cast<char*>(mod[1]) + rva; }

static void*(__fastcall* MemFileCtor)(void*, void*, const void*, unsigned, bool, int);
static void(__fastcall* MemFileDtor)(void*);

// the corpus: every embedded program text of the render modules (the built-in vertex programs and the like)
static std::vector<std::string> corpus;
static void CollectCorpus()
{
    const char* files[] = {"Render3DCore_7c.dll", "Render3DFoundation_7c.dll", "Render3DExtension_7c.dll",
                           "WaterSystem_7c.dll", "RenderOpenGL_7c.dll", "RenderDirectX9_7c.dll", "trainz.exe"};
    for (const char* name : files) {
        FILE* f = std::fopen((binDir + "\\" + name).c_str(), "rb");
        if (!f) continue;
        std::vector<char> d;
        char buf[65536];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
        std::fclose(f);
        size_t i = 0;
        while (i < d.size()) {
            size_t j = i;
            while (j < d.size() && (static_cast<unsigned char>(d[j]) >= 0x20 || d[j] == '\r' || d[j] == '\n' || d[j] == '\t'))
                ++j;
            if (j - i > 120 && j < d.size() && d[j] == 0) {
                std::string s(d.begin() + i, d.begin() + j);
                if (s.find("version") != std::string::npos &&
                    (s.find("rogram") != std::string::npos || s.find("ombiner") != std::string::npos))
                    corpus.push_back(s);
            }
            i = j + 1;
        }
    }
    std::printf("-- corpus: %zu texts\n", corpus.size());
}

// a corpus text, as it is or damaged (cut, tokens dropped or doubled, bytes changed); `words` is the corpus split
// at blanks, plus punctuation
static std::string SampleOf(std::mt19937& r, const std::vector<std::string>& corpus, std::vector<std::string>& words,
                            const std::vector<const char*>& punct, const char* head)
{
    if (words.empty()) {
        for (const std::string& t : corpus) {
            size_t i = 0;
            while (i < t.size()) {
                while (i < t.size() && std::strchr(" \t\r\n", t[i])) ++i;
                size_t j = i;
                while (j < t.size() && !std::strchr(" \t\r\n", t[j])) ++j;
                if (j > i) words.push_back(t.substr(i, j - i));
                i = j;
            }
        }
        for (const char* p : punct)
            for (int k = 0; k < 40; ++k) words.push_back(p);
    }
    const int how = static_cast<int>(r() % 10);
    if (how >= 8) {  // token soup, or a piece cut out of the middle of a text
        std::string s = r() % 2 ? head : "";
        if (how == 8) {
            for (int n = static_cast<int>(r() % 300); n; --n) {
                s += words[r() % words.size()];
                s += " \n\t"[r() % 3];
            }
        } else {
            const std::string& t = corpus[r() % corpus.size()];
            const size_t at = r() % t.size();
            s += t.substr(at, r() % 4000);
        }
        return s;
    }
    std::string s = corpus[r() % corpus.size()];
    if (how == 7) {  // long, with bytes above 0x7f: tokens across the scanner's 8 KB reads
        while (s.size() < 20000) s += corpus[r() % corpus.size()];
        for (int k = static_cast<int>(r() % 200); k; --k) s[r() % s.size()] = static_cast<char>(0x80 + r() % 0x80);
        return s;
    }
    if (how >= 4) return s;
    for (int k = 1 + static_cast<int>(r() % 4); k; --k) {
        if (s.empty()) break;
        const size_t at = r() % s.size();
        const size_t len = 1 + r() % 12;
        switch (how) {
        case 0: s.erase(at, len); break;
        case 1: s.insert(at, s.substr(at, len)); break;
        case 2: s[at] = "{}()=;,.#\n x0\"/*\x80\xff"[r() % 18]; break;
        case 3: s.resize(at); break;
        }
    }
    return s;
}
static std::string Sample(std::mt19937& r)
{
    static std::vector<std::string> words;
    static const std::vector<const char*> punct = {"{", "}", "(", ")", "=", ";", ",", "#if", "#else", "#endif",
                                                   "#define", "\"", "/*", "*/", "//", "version", "2.0", "1.5",
                                                   "\xe9t\xe9", "\x80\xff"};
    return SampleOf(r, corpus, words, punct, "version 2.0\n");
}

static void DumpNames(const char* tag, const void* listHead)
{
    struct Chunk {
        Chunk* next;
        Chunk* prev;
        int count;
        void* names[13];
    };
    const auto* head = static_cast<const Chunk*>(listHead);
    Log("  %s:", tag);
    if (head) {
        const Chunk* c = head;
        do {
            Log(" [%s %d:", Name(c).c_str(), c->count);
            for (int i = 0; i < c->count; ++i) Log(" %s", NodeText(c->names[i]).c_str());
            Log("]");
            c = c->next;
        } while (c != head);
    }
    Log("\n");
}

static unsigned char asmResult;
static int parsedOk, parsedBad;  // the original's results, to see how much of the error paths is reached
static void OrigQuickCtor(void* obj, void* file)
{
    void* f = OrigAt(0x2f830);
    __asm {
        pushad
        mov ecx, file
        push obj
        call f
        popad
    }
}
static bool OrigQuickRun(void* obj, void* name)
{
    void* f = OrigAt(0x2f930);
    __asm {
        pushad
        mov edi, obj
        mov eax, name
        call f
        mov asmResult, al
        popad
    }
    return asmResult != 0;
}
static void OrigQuickDtor(void* obj)
{
    void* f = OrigAt(0x2f8d0);
    __asm {
        pushad
        push obj
        call f
        popad
    }
}

static void RunQuick(int s, unsigned seed)
{
    std::mt19937 r(seed);
    static void (*ourCtor)(void*, void*);
    static bool (*ourRun)(void*, const void*);
    static const void* (*ourNames)(void*);
    static void (*ourDtor)(void*);
    if (!ourCtor) {
        ourCtor = As<decltype(ourCtor)>(OurSym("_RsTestQuickCtor"));
        ourRun = As<decltype(ourRun)>(OurSym("_RsTestQuickRun"));
        ourNames = As<decltype(ourNames)>(OurSym("_RsTestQuickNames"));
        ourDtor = As<decltype(ourDtor)>(OurSym("_RsTestQuickDtor"));
    }
    for (int k = 0; k < 3; ++k) {
        const std::string text = Sample(r);
        const char* names[] = {"MPOneTexMaterial_Vtx", "", "x"};
        void* file = RealNew(0x14);
        MemFileCtor(file, nullptr, text.c_str(), static_cast<unsigned>(text.size()), false, 0);
        void* name = NewAnsi(names[r() % 3]);
        void* obj = RealNew(0x400);
        std::memset(obj, 0xcd, 0x400);
        Log("quick %d: %u bytes\n", k, static_cast<unsigned>(text.size()));
        bool ok;
        const void* list;
        if (s == 0) {
            ourCtor(obj, file);
            ok = ourRun(obj, name);
            list = *static_cast<void* const*>(ourNames(obj));
        } else {
            OrigQuickCtor(obj, file);
            ok = OrigQuickRun(obj, name);
            list = *reinterpret_cast<void* const*>(static_cast<char*>(obj) + 0x80);
        }
        Log("  ok %d\n", ok);
        if (s == 1) ++(ok ? parsedOk : parsedBad);
        DumpNames("names", list);
        if (s == 0)
            ourDtor(obj);
        else
            OrigQuickDtor(obj);
        RealDelete(obj);
        FreeAnsi(name);
        MemFileDtor(file);
        RealDelete(file);
    }
}

// ---- a Lemon driver alone: made-up token streams (every terminal carries a text, so any rule finds strings)

struct EngineSpec {
    const char* ourParse;
    const char* ourFree;
    unsigned driver;     // the original's: ecx stack, eax major, edx minor, [esp+4] context
    unsigned freeStack;  // esi stack
    int terminals;
    int ctxSize;      // the context the reduce actions see (the parser last)
    int errorOffset;  // the context's error flag
    void (*prepare)(unsigned char* ctx, int variant);  // the rest of the context
    void (*finish)(int side, unsigned char* ctx, void* parser);  // log and free what the actions left
    void* (*makeParser)(int side);
    void (*freeParser)(int side, void* parser);
    void (*origDriver)(void* stack, int major, void* minor, void* ctx);  // when the convention is another one
    unsigned stackSize;  // 0x4bc when not given
};

static void OrigDriver(unsigned rva, void* stack, int major, void* minor, void* ctx)
{
    void* f = OrigAt(rva);
    __asm {
        pushad
        push ctx
        mov ecx, stack
        mov eax, major
        mov edx, minor
        call f
        add esp, 4
        popad
    }
}
static void OrigFreeStack(unsigned rva, void* stack)
{
    void* f = OrigAt(rva);
    __asm {
        pushad
        mov esi, stack
        call f
        popad
    }
}

static void FreeNameList(void* head)  // the chunks the reduce actions made (0x1002ce20 order)
{
    struct Chunk {
        Chunk* next;
        Chunk* prev;
        int count;
        void* n[13];
    };
    auto* first = static_cast<Chunk*>(head);
    if (!first) return;
    auto release = [](Chunk* c) {
        for (int i = 0; i < c->count; ++i)
            if (c->n[i]) DestroyNode(*cache, c->n[i]);
        HookDelete(c);
    };
    Chunk* c = first->next;
    while (c != first) {
        Chunk* next = c->next;
        release(c);
        c = next;
    }
    release(first);
}

using ParseFn = void(__cdecl*)(void*, int, void*, void*);
using FreeFn = void(__cdecl*)(void*);

static void* TokenText(std::mt19937& r)  // a token's minor: a number-like text in Jet's heap
{
    char text[16];
    std::snprintf(text, sizeof text, "%d.%d", static_cast<int>(r() % 100), static_cast<int>(r() % 10));
    void* m = RealNew(static_cast<unsigned>(std::strlen(text) + 1));
    std::strcpy(static_cast<char*>(m), text);
    return m;
}

// one token through a side's driver; false when it faults (the texture parser's actions read address 0 on some
// streams, in both modules: the stack is left as it is then)
// zeroes the stack below the caller: the vertex parser's driver reads a slot of it that nothing sets (rules
// without a value); between two tokens the game's scanner leaves zeroes there
static __declspec(noinline) void ScrubStack()
{
    volatile char area[0x1000];
    for (char& c : const_cast<char(&)[0x1000]>(area)) c = 0;
}

static uintptr_t faultAddress;  // the last fault caught (RS_U2B_FAULTS=1 prints it)
static int FaultAt(EXCEPTION_POINTERS* e)
{
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
    faultAddress = reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress);
    if (std::getenv("RS_U2B_FAULTS")) {
        HMODULE m = nullptr;
        char path[MAX_PATH] = "";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<const char*>(faultAddress), &m))
            GetModuleFileNameA(m, path, MAX_PATH);
        std::printf("caught a fault in %s +0x%x\n", path, static_cast<unsigned>(faultAddress - reinterpret_cast<uintptr_t>(m)));
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
static bool GuardedParse(int s, const EngineSpec& e, ParseFn parse, void* stack, int major, void* minor, void* ctx)
{
    __try {
        if (s == 0)
            parse(stack, major, minor, ctx);
        else if (e.origDriver)
            e.origDriver(stack, major, minor, ctx);
        else
            OrigDriver(e.driver, stack, major, minor, ctx);
        return true;
    } __except (FaultAt(GetExceptionInformation())) {
        return false;
    }
}
static bool GuardedFinish(const EngineSpec& e, int side, unsigned char* ctx, void* parser)
{
    __try {
        e.finish(side, ctx, parser);
        return true;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}

// our driver's state after `stream` from scratch, nothing logged: {error countdown, stack index} ({-1000, -1000}
// when the stream faults)
static std::pair<int, int> Probe(const EngineSpec& e, ParseFn parse, FreeFn free, void* parser,
                                 const std::vector<int>& stream)
{
    const int saved = cur;
    cur = -1;
    std::mt19937 r(1);
    unsigned char ctx[0x80] = {};
    e.prepare(ctx, 0);
    *reinterpret_cast<void**>(ctx + e.ctxSize - 4) = parser;
    void* stack = RealNew(e.stackSize ? e.stackSize : 0x4bc);
    *static_cast<int*>(stack) = -1;
    bool fault = false;
    for (int t : stream) {
        void* const text = t ? TokenText(r) : nullptr;
        ScrubStack();
        if (!GuardedParse(0, e, parse, stack, t, text, ctx)) {
            fault = true;
            break;
        }
    }
    std::pair<int, int> state{static_cast<int*>(stack)[1], static_cast<int*>(stack)[0]};
    if (fault)
        state = {-1000, -1000};
    else
        free(stack);
    GuardedFinish(e, -1, ctx, parser);
    cur = saved;
    return state;
}

// a stream the grammar mostly takes: each next token one our driver shifts without an error (the error countdown
// goes down by one), now and then any token
static std::vector<int> GrammarStream(std::mt19937& r, const EngineSpec& e, ParseFn parse, FreeFn free, void* parser)
{
    std::vector<int> stream;
    const int n = static_cast<int>(r() % (r() % 4 ? 40 : 160));
    const int bad = static_cast<int>(r() % 3);  // how often a wrong token is let in (never, rarely, more)
    for (int i = 0; i < n; ++i) {
        std::vector<int> order;
        for (int t = 1; t <= e.terminals; ++t) order.push_back(t);
        for (int k = static_cast<int>(order.size()) - 1; k > 0; --k) std::swap(order[k], order[r() % (k + 1)]);
        int pick = 0;
        if (bad && static_cast<int>(r() % 20) < bad) {
            pick = order[0];
        } else {
            const int before = stream.empty() ? -1 : Probe(e, parse, free, parser, stream).first;
            for (int t : order) {
                stream.push_back(t);
                const std::pair<int, int> after = Probe(e, parse, free, parser, stream);
                stream.pop_back();
                if (after.first == before - 1 && after.second >= 0) {
                    pick = t;
                    break;
                }
            }
        }
        if (!pick) pick = order[0];  // nothing fits: an error, then recovery
        stream.push_back(pick);
    }
    return stream;
}

static void RunEngine(int s, unsigned seed, const EngineSpec& e)
{
    std::mt19937 r(seed);
    static std::map<std::string, void*> syms;
    if (!syms[e.ourParse]) {
        syms[e.ourParse] = OurSym(e.ourParse);
        syms[e.ourFree] = OurSym(e.ourFree);
    }
    const auto ourParse = reinterpret_cast<ParseFn>(syms[e.ourParse]);
    const auto ourFree = reinterpret_cast<FreeFn>(syms[e.ourFree]);
    // the probes always run our driver with our parser object; the logged run uses the side's
    const int saved = cur;
    cur = -1;
    void* probeParser = e.makeParser(0);
    cur = saved;
    void* parser = e.makeParser(s);
    for (int round = 0; round < 4; ++round) {
        unsigned char ctx[0x80];
        std::memset(ctx, 0, sizeof ctx);
        const int variant = static_cast<int>(r() % 4);
        e.prepare(ctx, variant);
        *reinterpret_cast<void**>(ctx + e.ctxSize - 4) = parser;
        void* stack = TestNew(e.stackSize ? e.stackSize : 0x4bc, "Stack%d", round);
        *static_cast<int*>(stack) = -1;
        std::vector<int> stream;
        if (round % 2) {
            stream = GrammarStream(r, e, ourParse, ourFree, probeParser);
        } else {
            const int n = static_cast<int>(r() % (r() % 4 ? 60 : 400));
            const int bias = static_cast<int>(r() % e.terminals) + 1;  // one token more frequent (lists, nesting)
            for (int i = 0; i < n; ++i) {
                int major = r() % 3 ? bias : static_cast<int>(r() % e.terminals) + 1;
                if (r() % 50 == 0) major = 0;
                stream.push_back(major);
            }
        }
        stream.push_back(0);
        const int n = static_cast<int>(stream.size()) - 1;
        Log("engine %d (variant %d): %d tokens:", round, variant, n);
        for (int t : stream) Log(" %d", t);
        Log("\n");
        bool fault = false;
        for (int i = 0; i <= n; ++i) {
            const int major = stream[i];
            void* minor = nullptr;
            if (major) {  // (a rule may atof() its token: never null)
                char text[16];
                std::snprintf(text, sizeof text, "%d.%d", static_cast<int>(r() % 100), static_cast<int>(r() % 10));
                minor = TestNew(static_cast<unsigned>(std::strlen(text) + 1), "T%d", i);
                std::strcpy(static_cast<char*>(minor), text);
            }
            ScrubStack();
            if (!GuardedParse(s, e, ourParse, stack, major, minor, ctx)) {
                Log("  %d -> crashed\n", major);
                fault = true;
                break;
            }
            const int idx = *static_cast<int*>(stack);
            Log("  %d -> top %d err %d flag %d", major, idx, static_cast<int*>(stack)[1], ctx[e.errorOffset]);
            if (idx >= 0) {
                const int* top = reinterpret_cast<int*>(static_cast<int*>(stack)[2]);
                Log(" [%d %d]", top[0], top[1]);
            }
            Log("\n");
        }
        if (fault)  // (the stack is left: a half-done reduce may have freed what it holds)
            ;
        else if (s == 0)
            ourFree(stack);
        else
            OrigFreeStack(e.freeStack, stack);
        if (!GuardedFinish(e, s, ctx, parser)) Log("  finish crashed\n");
    }
    e.freeParser(s, parser);
    cur = -1;
    e.freeParser(0, probeParser);
    cur = saved;
}

// the quick parser: context {names list, error, parser}; a stand-in parser for the original's error message (a
// String at +0x7c, the line at +0x84)
static void QuickPrepare(unsigned char* ctx, int)
{
    *reinterpret_cast<void**>(ctx) = ctx + 0x30;  // the list head lives in the context buffer
}
static void QuickFinish(int side, unsigned char* ctx, void*)
{
    void* head = *reinterpret_cast<void**>(ctx + 0x30);
    if (side >= 0) DumpNames("names", head);
    FreeNameList(head);
}
static void* QuickParser(int)
{
    void* parser = RealNew(0x200);
    std::memset(parser, 0, 0x200);
    AnsiFromText(static_cast<char*>(parser) + 0x7c, nullptr, "test");
    *reinterpret_cast<int*>(static_cast<char*>(parser) + 0x84) = 7;
    return parser;
}
static void FreeQuickParser(int, void* parser)
{
    AnsiDtor(static_cast<char*>(parser) + 0x7c, nullptr);
    RealDelete(parser);
}

static void RunQuickEngine(int s, unsigned seed)
{
    static const EngineSpec spec = {"_RsTestQuickParse", "_RsTestQuickFree", 0x2ed30, 0x2e860, 11, 12, 4,
                                    QuickPrepare, QuickFinish, QuickParser, FreeQuickParser};
    RunEngine(s, seed, spec);
}

// ---- the pre-parser: a stand-in language (slot 0 SupportedByPlatform, slot 7 Defines, the Display at +8 with its
// capability query in slot 7, the generation at +0x1c)

static bool __cdecl FakeSupported(void* self, float generation)
{
    const bool ok = static_cast<int>(generation * 8) % 5 != 0;
    Log("  supported %s %g -> %d\n", Name(self).c_str(), generation, ok);
    return ok;
}
static bool __cdecl FakeDefines(const void* self, const void* name)
{
    const void* node = *static_cast<void* const*>(name);
    unsigned h = 0;
    for (const char* p = node ? static_cast<const Node*>(node)->text : ""; *p; ++p) h = h * 31 + static_cast<unsigned char>(*p);
    const bool yes = h % 2 != 0;
    Log("  defines %s %s -> %d\n", Name(self).c_str(), NodeText(node).c_str(), yes);
    return yes;
}
static bool __cdecl FakeCapability(void* self, int index)
{
    const bool yes = index % 3 == 0;
    Log("  capability %s %d -> %d\n", Name(self).c_str(), index, yes);
    return yes;
}
static bool __cdecl FakeFalse() { return false; }
static void* langVtable[9] = {reinterpret_cast<void*>(&FakeSupported), &FakeFalse, &FakeFalse, &FakeFalse, &FakeFalse,
                              &FakeFalse, &FakeFalse, reinterpret_cast<void*>(&FakeDefines), &FakeFalse};
static void* displayVtable[8] = {&FakeFalse, &FakeFalse, &FakeFalse, &FakeFalse, &FakeFalse, &FakeFalse, &FakeFalse,
                                 reinterpret_cast<void*>(&FakeCapability)};
static struct {
    void** vt = displayVtable;
} fakeDisplay;
static struct {
    void** vt = langVtable;
    void* vb = nullptr;
    void* display = &fakeDisplay;
    unsigned char rest[0x34] = {};
} fakeLang;
static float LangGeneration() { return *reinterpret_cast<float*>(reinterpret_cast<char*>(&fakeLang) + 0x1c); }

static void OrigPreCtor(void* obj, int param, void* file)
{
    void* f = OrigAt(0x2b9d0);
    __asm {
        pushad
        push file
        push param
        push obj
        call f
        popad
    }
}
static bool OrigPreRun(void* obj, const void* program, const void* file, void* lang)
{
    void* f = OrigAt(0x2bbd0);
    __asm {
        pushad
        mov edi, obj
        push lang
        push file
        push program
        call f
        add esp, 12
        mov asmResult, al
        popad
    }
    return asmResult != 0;
}
static void OrigPreDtor(void* obj)
{
    void* f = OrigAt(0x2baf0);
    __asm {
        pushad
        push obj
        call f
        popad
    }
}

struct PreApi {
    void (*ctor)(void*, int, void*);
    bool (*run)(void*, const void*, const void*, void*);
    const void* (*output)(void*);
    void (*dtor)(void*);
};
static PreApi& OurPre()
{
    static PreApi a;
    if (!a.ctor) {
        a.ctor = As<decltype(a.ctor)>(OurSym("_RsTestPreCtor"));
        a.run = As<decltype(a.run)>(OurSym("_RsTestPreRun"));
        a.output = As<decltype(a.output)>(OurSym("_RsTestPreOutput"));
        a.dtor = As<decltype(a.dtor)>(OurSym("_RsTestPreDtor"));
    }
    return a;
}
static const char* PreOutput(int s, void* obj)
{
    return AnsiText(s == 0 ? OurPre().output(obj) : static_cast<char*>(obj) + 0x88);
}

// a made-up program file using every feature of the pre-parser's syntax (found by walking its DFA): API blocks,
// API_NATIVE with a binding part, nested %ifdef/%ifndef/%elseif/%else/%endif with ( | & ! ), API(name),
// extension(capability) and program names, generation
static std::string PreText(std::mt19937& r, std::vector<std::string>& programs)
{
    static const char* apis[] = {"common", "OpenGL", "DirectX", "Other"};
    static const char* flags[] = {"extrusion", "point", "color", "fog", "skin"};
    static const char* caps[] = {"D_MULTI_TEXTURE", "D_AUTOMIP", "D_VERTEX_PROGRAM", "D_BLEND_EQUATION", "D_NOPE"};
    auto P = [&](int n) { return static_cast<int>(r() % n); };
    std::string t = "# made up\n\nversion " + std::string(P(6) ? "2.0" : "1.5") + "\n\n";
    t += "vertexProgram(Made" + std::to_string(P(3)) + ") {\n";
    for (int i = 0, n = 1 + P(4); i < n; ++i) {
        const std::string name = "prog" + std::to_string(i);
        programs.push_back(name);
        t += "  program " + name + " (P" + std::to_string(P(5)) + ")";
        if (P(4)) {
            t += " =";
            for (int k = 0, m = 1 + P(3); k < m; ++k) t += std::string(k ? ", " : " ") + flags[P(5)];
        }
        t += ";\n";
    }
    if (P(2)) t += "  generation = " + std::to_string(1 + P(3)) + "." + std::to_string(P(10)) + ";\n";
    t += "}\n\n";
    std::function<std::string(int)> expr = [&](int depth) -> std::string {
        switch (depth > 2 ? P(3) : P(7)) {
        case 0: return flags[P(5)];
        case 1: return std::string("API(") + apis[P(4)] + ")";
        case 2: return std::string("extension(") + caps[P(5)] + ")";
        case 3: return "!" + expr(depth + 1);
        case 4: return "(" + expr(depth + 1) + ")";
        case 5: return expr(depth + 1) + " | " + expr(depth + 1);
        default: return expr(depth + 1) + " & " + expr(depth + 1);
        }
    };
    std::function<std::string(int)> body = [&](int depth) -> std::string {
        std::string b;
        for (int i = 0, n = 1 + P(5); i < n; ++i) {
            switch (depth > 3 ? 0 : P(7)) {
            case 0: b += "  MOV r" + std::to_string(P(9)) + ", c[" + std::to_string(P(9)) + "];\n"; break;
            case 6: b += "  PARAM v = { 0, 1 };\n"; break;
            case 1: b += "  { DP4 a, b; }\n"; break;
            case 2: b += "  /* note */ # comment\n"; break;
            default: {
                static const char* broken[] = {"", "", "", "", " &", " | (", " !"};  // an expression left open
                b += std::string(P(3) ? "%ifdef " : "%ifndef ") + expr(0) + broken[P(7)] + "\n" + body(depth + 1);
                for (int k = P(3); k; --k) b += "%elseif " + expr(0) + "\n" + body(depth + 1);
                if (P(2)) b += "%else\n" + body(depth + 1);
                b += "%endif\n";
            }
            }
        }
        return b;
    };
    for (int i = 0, n = 1 + P(4); i < n; ++i) {
        if (P(3)) {
            std::string a = apis[P(4)];
            if (P(3) == 0) a += std::string("|") + apis[P(4)];
            t += "API(" + a + ")" + (P(4) ? "" : " =") + "\n{\n" + body(0) + "}\n\n";
        } else {
            t += "API_NATIVE(" + std::string(apis[P(4)]) + ")";
            if (P(2)) t += " : { PARAM x = 1; }";
            t += "\n{\n  !!ARBvp1.0 { native = 2; }\n" + body(0) + "}\n\n";
        }
    }
    if (P(5) == 0) t.erase(r() % t.size(), 1 + r() % 6);  // now and then broken
    return t;
}

// the program names a text declares ("program <name>"), to ask for one of them
static std::vector<std::string> ProgramNames(const std::string& t)
{
    std::vector<std::string> out;
    for (size_t at = t.find("program "); at != std::string::npos; at = t.find("program ", at + 8)) {
        size_t i = at + 8;
        while (i < t.size() && t[i] == ' ') ++i;
        size_t j = i;
        while (j < t.size() && (std::isalnum(static_cast<unsigned char>(t[j])) || t[j] == '_')) ++j;
        if (j > i) out.push_back(t.substr(i, j - i));
    }
    return out;
}

// the scanners' module statics (0x100b7dc8..0x100b8127 in the original): a damaged text can leave them in any
// state, which the next run inherits (as in the original); most scenarios start both sides from zero
static void ResetLexStatics(int s)
{
    static void* ours = OurSym("?gfxLex@Jet@@3UGfxLexStatics@1@A");
    std::memset(s == 0 ? ours : OrigAt(0xb7dc8), 0, 0x360);
}

// a run that may fault (as the original does): -1 then
static int GuardedPreRun(int s, void* obj, const void* program, const void* file)
{
    __try {
        return s == 0 ? OurPre().run(obj, program, file, &fakeLang) : OrigPreRun(obj, program, file, &fakeLang);
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return -1;
    }
}

static void RunPre(int s, unsigned seed)
{
    std::mt19937 r(seed);
    if (r() % 4) ResetLexStatics(s);
    names[s][&fakeLang] = "lang";
    names[s][&fakeDisplay] = "display";
    for (int k = 0; k < 3; ++k) {
        std::vector<std::string> programs;
        const int kind = static_cast<int>(r() % 3);
        const std::string text = kind == 0 ? PreText(r, programs) : kind == 1 ? Sample(r) : corpus[r() % corpus.size()];
        if (kind) programs = ProgramNames(text);
        std::string program = "";
        const int pick = static_cast<int>(r() % 6);
        if (pick && !programs.empty()) program = programs[r() % programs.size()];
        else if (pick == 1) program = "nosuchprogram";
        void* programNode = program.empty() ? nullptr : CreateNode(*cache, reinterpret_cast<const unsigned char*>(program.c_str()));
        void* fileNode = r() % 2 ? CreateNode(*cache, reinterpret_cast<const unsigned char*>("prog.txt")) : nullptr;
        const int param = static_cast<int>(r() % 3);
        *reinterpret_cast<float*>(reinterpret_cast<char*>(&fakeLang) + 0x1c) = 0.25f;  // so the store shows
        void* file = RealNew(0x14);
        MemFileCtor(file, nullptr, text.c_str(), static_cast<unsigned>(text.size()), false, 0);
        void* obj = RealNew(0x400);
        std::memset(obj, 0xcd, 0x400);
        Log("pre %d: %u bytes, program \"%s\"\n", k, static_cast<unsigned>(text.size()), program.c_str());
        if (s == 0)
            OurPre().ctor(obj, param, file);
        else
            OrigPreCtor(obj, param, file);
        const int ok = GuardedPreRun(s, obj, &programNode, &fileNode);
        if (ok < 0) {  // both modules read address 0 on such a text (an #if stack popped empty...)
            Log("  crashed\n");
            ResetLexStatics(s);
        }
        Log("  ok %d gen %g\n  out [%s]\n", ok, LangGeneration(), PreOutput(s, obj));
        if (s == 1) ++(ok > 0 ? parsedOk : parsedBad);
        if (s == 0)
            OurPre().dtor(obj);
        else
            OrigPreDtor(obj);
        RealDelete(obj);
        MemFileDtor(file);
        RealDelete(file);
        if (programNode) DestroyNode(*cache, programNode);
        if (fileNode) DestroyNode(*cache, fileNode);
    }
}

// the pre-parser's driver alone: context {version, generation, param, lang, names (+0x10), file, program (+0x18),
// error (+0x1c), found, -, parser (+0x20)}; the parser a real one of the side's (rule 13 writes into its output)
static void* emptyFile;
static void PrePrepare(unsigned char* ctx, int variant)
{
    static const char* programs[] = {nullptr, "12.3", "5.5", "x"};
    *reinterpret_cast<float*>(ctx + 4) = 1.0f;
    *reinterpret_cast<void**>(ctx + 0xc) = &fakeLang;
    *reinterpret_cast<const char**>(ctx + 0x14) = "file";
    *reinterpret_cast<const char**>(ctx + 0x18) = programs[variant];
}
static void PreFinish(int side, unsigned char* ctx, void* parser)
{
    void* list = *reinterpret_cast<void**>(ctx + 0x10);
    if (side >= 0) {
        float v[2];
        std::memcpy(v, ctx, sizeof v);
        Log("  ctx version %g generation %g found %d out [%s]\n", v[0], v[1], ctx[0x1d], PreOutput(side, parser));
        DumpNames("names", list ? *static_cast<void**>(list) : nullptr);
    }
    if (list) {
        FreeNameList(*static_cast<void**>(list));
        HookDelete(list);
    }
}
static void* PreParserObject(int side)
{
    if (!emptyFile) {
        emptyFile = RealNew(0x14);
        MemFileCtor(emptyFile, nullptr, "", 0, false, 0);
    }
    void* obj = RealNew(0x400);
    std::memset(obj, 0, 0x400);
    if (side == 0)
        OurPre().ctor(obj, 0, emptyFile);
    else
        OrigPreCtor(obj, 0, emptyFile);
    return obj;
}
static void FreePreParserObject(int side, void* obj)
{
    if (side == 0)
        OurPre().dtor(obj);
    else
        OrigPreDtor(obj);
    RealDelete(obj);
}

static void RunPreEngine(int s, unsigned seed)
{
    static const EngineSpec spec = {"_RsTestPreParse", "_RsTestPreFree", 0x2cad0, 0x2c210, 0x1d, 0x24, 0x1c,
                                    PrePrepare, PreFinish, PreParserObject, FreePreParserObject};
    names[s][&fakeLang] = "lang";
    RunEngine(s, seed, spec);
}

// ---------------------------------------------------------------------------------------------------------------
// the texture program parser (TextureCombiner::Load's front end). A stand-in combiner logs what the parser hands
// it: slot 3 LoadNativeFormat, 4 CompileProgram, 11 an instruction, 13 TrackConstant; SetProgramName writes its +0x14.

static std::vector<std::string> tpCorpus;
static void CollectTpCorpus()
{
    const char* files[] = {"Render3DCore_7c.dll", "Render3DFoundation_7c.dll", "Render3DExtension_7c.dll",
                           "WaterSystem_7c.dll", "RenderOpenGL_7c.dll", "RenderDirectX9_7c.dll", "trainz.exe"};
    for (const char* name : files) {
        FILE* f = std::fopen((binDir + "\\" + name).c_str(), "rb");
        if (!f) continue;
        std::vector<char> d;
        char buf[65536];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
        std::fclose(f);
        size_t i = 0;
        while (i < d.size()) {
            size_t j = i;
            while (j < d.size() && (static_cast<unsigned char>(d[j]) >= 0x20 || d[j] == '\r' || d[j] == '\n' || d[j] == '\t'))
                ++j;
            if (j - i > 120 && j < d.size() && d[j] == 0) {
                std::string s(d.begin() + i, d.begin() + j);
                if (s.find("API") != std::string::npos &&
                    (s.find("ps.") != std::string::npos || s.find("header") != std::string::npos))
                    tpCorpus.push_back(s);
            }
            i = j + 1;
        }
    }
    std::printf("-- texture program corpus: %zu texts\n", tpCorpus.size());
}
static std::string TpSample(std::mt19937& r)
{
    static std::vector<std::string> w;
    static const std::vector<const char*> punct = {
        "{", "}", "(", ")", ",", ";", "-", "+", "[", "]", "^", "\"", "/*", "*/", "//", "#", "\n", "API(common)",
        "API(OpenGL)", "API(DirectX)", "API_NATIVE(OpenGL)", "API_NATIVE(DirectX)", "header(x)", "reg", "c[^",
        "version", "1.1", "ps.1.1", "ps.1.4", "_bx2", ".rgb", "c0", "r1", "t2", "tex(cube)", "\xe9t\xe9", "\x80\xff"};
    return SampleOf(r, tpCorpus, w, punct, "version 1.1\n");
}

static int tpCompiles;
static bool __cdecl CombNative(void* self, const void* name, const char* text)
{
    const bool ok = (text ? std::strlen(text) : 0) % 5 != 0;
    Log("  native %s name %s [%s] -> %d\n", Name(self).c_str(), NodeText(*static_cast<void* const*>(name)).c_str(),
        text ? text : "(null)", ok);
    return ok;
}
static bool __cdecl CombCompile(void* self)
{
    const bool ok = ++tpCompiles % 4 != 0;
    Log("  compile %s -> %d\n", Name(self).c_str(), ok);
    return ok;
}
static void __cdecl CombInstruction(void*, const void* ins) { DumpIns("instruction", ins); }
static bool __cdecl CombConstant(void* self, int index, const void* name, const float* v)
{
    const char* t = AnsiText(name);
    Log("  constant %s %d [%s] %g %g %g %g\n", Name(self).c_str(), index, t ? t : "(null)", v[0], v[1], v[2], v[3]);
    return true;
}
static void* combVtable[18] = {&FakeFalse, &FakeFalse, &FakeFalse, reinterpret_cast<void*>(&CombNative),
                               reinterpret_cast<void*>(&CombCompile), &FakeFalse, &FakeFalse, &FakeFalse,
                               &FakeFalse, &FakeFalse, &FakeFalse, reinterpret_cast<void*>(&CombInstruction),
                               &FakeFalse, reinterpret_cast<void*>(&CombConstant), &FakeFalse, &FakeFalse,
                               &FakeFalse, &FakeFalse};
static struct {
    void** vt = combVtable;
    void* vb = nullptr;
    void* display = &fakeDisplay;
    unsigned char rest[0x34] = {};
} fakeComb;
static void*& CombProgramName() { return *reinterpret_cast<void**>(reinterpret_cast<char*>(&fakeComb) + 0x14); }

static void(__cdecl* AnsiAssign)(void*, const char*);
static void* TpCode(int s)  // the code-block text, a module static (0x100b8800)
{
    static void* ours = OurSym("?gfxTpCode@Jet@@3VAnsiString@1@A");
    return s == 0 ? ours : OrigAt(0xb8800);
}
static unsigned char* LexStatics(int s)
{
    static void* ours = OurSym("?gfxLex@Jet@@3UGfxLexStatics@1@A");
    return static_cast<unsigned char*>(s == 0 ? ours : OrigAt(0xb7dc8));
}
static void ResetTp(int s)
{
    ResetLexStatics(s);
    AnsiAssign(TpCode(s), "");
}
static void LogTpState(int s)
{
    const unsigned char* st = LexStatics(s);
    const int* t = reinterpret_cast<const int*>(st + 0x2c8);
    const char* code = AnsiText(TpCode(s));
    Log("  scanner depth %d cond %d braces %d block %d native %d stack %d %d, code [%s]\n", t[0], t[1], t[2], t[3],
        st[0x29f], reinterpret_cast<const int*>(st + 0x1d8)[0], reinterpret_cast<const int*>(st + 0x1d8)[1],
        code ? code : "(null)");
}

static void OrigTpCtor(void* obj, void* file, int apiKind)
{
    void* f = OrigAt(0x45dc0);  // tpFlexLexer(file), then what TextureCombiner::Load sets
    __asm {
        pushad
        mov ecx, file
        push obj
        call f
        popad
    }
    auto* o = static_cast<unsigned char*>(obj);
    *reinterpret_cast<void**>(o) = OrigAt(0x72830);
    *reinterpret_cast<void**>(o + 0x7c) = nullptr;
    *reinterpret_cast<int*>(o + 0x80) = 1;
    *reinterpret_cast<int*>(o + 0x84) = apiKind;
}
static bool OrigTpRun(void* obj, const void* file, void* lang)
{
    void* f = OrigAt(0x462d0);
    __asm {
        pushad
        push lang
        push file
        mov ecx, obj
        call f
        add esp, 8
        mov asmResult, al
        popad
    }
    return asmResult != 0;
}
static void OrigTpDtor(void* obj)
{
    void* f = OrigAt(0x40610);
    __asm {
        pushad
        push obj
        call f
        popad
    }
}
static struct {
    void (*ctor)(void*, void*, int);
    bool (*run)(void*, const void*, void*);
    void (*dtor)(void*);
    void (*destroy)(void*);
    int (*line)(void*);
} ourTp;
static void LoadOurTp()
{
    if (ourTp.ctor) return;
    ourTp.ctor = As<decltype(ourTp.ctor)>(OurSym("_RsTestTpCtor"));
    ourTp.run = As<decltype(ourTp.run)>(OurSym("_RsTestTpRun"));
    ourTp.dtor = As<decltype(ourTp.dtor)>(OurSym("_RsTestTpDtor"));
    ourTp.destroy = As<decltype(ourTp.destroy)>(OurSym("_RsTestTpDestroy"));
    ourTp.line = As<decltype(ourTp.line)>(OurSym("_RsTestTpLine"));
}

static int GuardedTpRun(int s, void* obj, const void* file)
{
    __try {
        return s == 0 ? ourTp.run(obj, file, &fakeComb) : OrigTpRun(obj, file, &fakeComb);
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return -1;
    }
}

static void RunTp(int s, unsigned seed)
{
    std::mt19937 r(seed);
    LoadOurTp();
    if (r() % 4) ResetTp(s);
    names[s][&fakeComb] = "comb";
    names[s][&fakeDisplay] = "display";
    for (int k = 0; k < 3; ++k) {
        const std::string text = r() % 3 == 0 ? tpCorpus[r() % tpCorpus.size()] : TpSample(r);
        const int apiKind = static_cast<int>(r() % 3);
        void* fileNode = r() % 2 ? CreateNode(*cache, reinterpret_cast<const unsigned char*>("comb.texture.txt")) : nullptr;
        void* file = RealNew(0x14);
        MemFileCtor(file, nullptr, text.c_str(), static_cast<unsigned>(text.size()), false, 0);
        void* obj = RealNew(0x400);
        std::memset(obj, 0xcd, 0x400);
        tpCompiles = 0;
        Log("tp %d: %u bytes, api %d\n", k, static_cast<unsigned>(text.size()), apiKind);
        if (s == 0)
            ourTp.ctor(obj, file, apiKind);
        else
            OrigTpCtor(obj, file, apiKind);
        const int ok = GuardedTpRun(s, obj, &fileNode);
        if (ok < 0) Log("  crashed\n");
        Log("  ok %d line %d program %s\n", ok,
            s == 0 ? ourTp.line(obj) : *reinterpret_cast<int*>(static_cast<char*>(obj) + 0x80),
            NodeText(CombProgramName()).c_str());
        LogTpState(s);
        if (ok < 0) ResetTp(s);
        if (CombProgramName()) DestroyNode(*cache, CombProgramName());
        CombProgramName() = nullptr;
        if (s == 1) ++(ok > 0 ? parsedOk : parsedBad);
        if (s == 0)
            ourTp.dtor(obj);
        else
            OrigTpDtor(obj);
        RealDelete(obj);
        MemFileDtor(file);
        RealDelete(file);
        if (fileNode) DestroyNode(*cache, fileNode);
    }
}

// ---- the texture parser's driver alone: context {version, program name, api, header list (+0xc), lang, file,
// parser (+0x18), sections (+0x1c), constant names (+0x20..+0x28), error (+0x2c)}

static void* TpVftable(int s, const char* ours, unsigned orig)
{
    return s == 0 ? OurSym(ours) : OrigAt(orig);
}
static void DumpTpItems(const char* tag, const void* list, int depth);
static void DumpTpItem(const void* item, int depth)
{
    static void* vt[2][6];
    if (!vt[cur][0]) {
        const int s = cur;
        vt[s][0] = TpVftable(s, "??_7TexApiSection@Jet@@6B@", 0x727cc);
        vt[s][1] = TpVftable(s, "??_7TexHeaderItem@Jet@@6B@", 0x727dc);
        vt[s][2] = TpVftable(s, "??_7TexRegisterDefinition@Jet@@6B@", 0x727e4);
        vt[s][3] = TpVftable(s, "??_7TexApiDataItem@Jet@@6B@", 0x727ec);
        vt[s][4] = TpVftable(s, "??_7TexApiCodeLine@Jet@@6BTexApiDataItem@1@@", 0x727f4);
        vt[s][5] = TpVftable(s, "??_7TexItem@Jet@@6B@", 0x727d4);
    }
    const auto* w = static_cast<const int*>(item);
    const void* v = reinterpret_cast<void* const*>(item)[0];
    int kind = 0;
    while (kind < 6 && vt[cur][kind] != v) ++kind;
    static const char* kinds[] = {"section", "header", "regdef", "data", "line", "item", "?"};
    Log("  %*s%s %s next %s", depth * 2, "", kinds[kind], Name(item).c_str(), Name(reinterpret_cast<void*>(w[1])).c_str());
    switch (kind) {
    case 0: {
        const char* t = AnsiText(w + 3);
        Log(" text [%s]\n", t ? t : "(null)");
        DumpTpItems("lines", reinterpret_cast<void*>(w[2]), depth + 1);
        break;
    }
    case 2: {
        const char* t = AnsiText(w + 4);
        Log(" kind %d index %d name [%s] value %s", w[2], w[3], t ? t : "(null)", Name(reinterpret_cast<void*>(w[5])).c_str());
        if (const auto* c = reinterpret_cast<const int*>(w[5])) {
            float f[4];
            std::memcpy(f, c + 1, sizeof f);
            Log(" {%d %g %g %g %g #%d}", c[0], f[0], f[1], f[2], f[3], c[5]);
        }
        Log("\n");
        break;
    }
    case 4: Log(" kind %d\n", w[2]); DumpIns("ins", w + 3); break;
    default: Log(" +8 %d\n", w[2]); break;
    }
}
static void DumpTpItems(const char* tag, const void* list, int depth)
{
    if (!list) {
        Log("  %*s%s -\n", depth * 2, "", tag);
        return;
    }
    const auto* l = static_cast<void* const*>(list);
    Log("  %*s%s %s %s/%s #%d\n", depth * 2, "", tag, Name(list).c_str(), Name(l[0]).c_str(), Name(l[1]).c_str(),
        reinterpret_cast<const int*>(l)[2]);
    int n = 0;
    for (const void* i = l[0]; i && n < 200; i = reinterpret_cast<void* const*>(i)[1], ++n) DumpTpItem(i, depth + 1);
}
static void TpPrepare(unsigned char* ctx, int variant)
{
    *reinterpret_cast<int*>(ctx + 8) = variant % 3;
    *reinterpret_cast<void**>(ctx + 0x10) = &fakeComb;
    *reinterpret_cast<const char**>(ctx + 0x14) = "file";
}
static void TpFinish(int side, unsigned char* ctx, void*)
{
    if (side >= 0) {
        float v;
        std::memcpy(&v, ctx, 4);
        Log("  ctx version %g program %s\n", v, NodeText(*reinterpret_cast<void**>(ctx + 4)).c_str());
        DumpTpItems("header", *reinterpret_cast<void**>(ctx + 0xc), 0);
        DumpTpItems("sections", *reinterpret_cast<void**>(ctx + 0x1c), 0);
        Log("  names #%d:", *reinterpret_cast<int*>(ctx + 0x28));
        for (const int* e = *reinterpret_cast<int**>(ctx + 0x20); e; e = reinterpret_cast<const int*>(e[2]))
            Log(" %s=%d %s", Name(e).c_str(), e[0], NodeText(reinterpret_cast<void*>(e[1])).c_str());
        Log("\n");
    }
    if (side == 1) {
        void* f = OrigAt(0x44bf0);
        __asm {
            pushad
            push ctx
            call f
            popad
        }
    } else {
        LoadOurTp();
        ourTp.destroy(ctx);
    }
}
static void* TpParser(int)
{
    void* parser = RealNew(0x200);
    std::memset(parser, 0, 0x200);
    *reinterpret_cast<int*>(static_cast<char*>(parser) + 0x80) = 7;
    return parser;
}
static void FreeTpParser(int, void* parser) { RealDelete(parser); }

static void RunTpEngine(int s, unsigned seed)
{
    static const EngineSpec spec = {"_RsTestTpParse", "_RsTestTpFree", 0x44830, 0x43630, 0x20, 0x1c, 0x2c,
                                    TpPrepare, TpFinish, TpParser, FreeTpParser};
    names[s][&fakeComb] = "comb";
    RunEngine(s, seed, spec);
}

// ---------------------------------------------------------------------------------------------------------------
// the vertex program parser (VertexProgram::Load's front end, run over the pre-parser's output). A stand-in program
// logs what it is handed: slot 3 LoadNativeFormat, 4 CompileProgram, 12 the names, 13 the symbol table, 14 the
// instructions, 28 a vertex type, 29/30 queries; SetProgramName writes its +0x14.

static std::vector<std::string> vpCorpus;  // our pre-parser's output for every program of the corpus
static std::vector<std::string> vpNative;  // ... of made-up files, the programs with a native block
static void CollectVpCorpus()
{
    const int saved = cur;
    cur = -1;
    std::vector<std::pair<std::string, std::string>> inputs;  // {text, program}
    for (const std::string& text : corpus)
        for (const std::string& program : ProgramNames(text)) inputs.push_back({text, program});
    std::mt19937 r(77);
    const size_t real = inputs.size();
    for (int k = 0; k < 400; ++k) {
        std::vector<std::string> programs;
        const std::string text = PreText(r, programs);
        for (const std::string& program : programs) inputs.push_back({text, program});
    }
    for (size_t i = 0; i < inputs.size(); ++i) {
        const std::string& text = inputs[i].first;
        const std::string& program = inputs[i].second;
        {  // (a made-up file needs a language that answers its #ifs: the pre-parser test's stand-in)
            void* file = RealNew(0x14);
            MemFileCtor(file, nullptr, text.c_str(), static_cast<unsigned>(text.size()), false, 0);
            void* obj = RealNew(0x400);
            std::memset(obj, 0, 0x400);
            void* node = CreateNode(*cache, reinterpret_cast<const unsigned char*>(program.c_str()));
            void* fileNode = nullptr;
            OurPre().ctor(obj, 0, file);
            ResetLexStatics(0);
            if (OurPre().run(obj, &node, &fileNode, &fakeLang)) {
                const char* out = PreOutput(0, obj);
                if (out && *out) {
                    if (i < real)
                        vpCorpus.push_back(out);
                    else if (std::strstr(out, "#NATIVE"))
                        vpNative.push_back(out);
                }
            }
            OurPre().dtor(obj);
            RealDelete(obj);
            MemFileDtor(file);
            RealDelete(file);
            DestroyNode(*cache, node);
        }
    }
    ResetLexStatics(0);
    cur = saved;
    std::printf("-- vertex program corpus: %zu texts, %zu native\n", vpCorpus.size(), vpNative.size());
}
static std::string VpSample(std::mt19937& r)
{
    static std::vector<std::string> w;
    static const std::vector<const char*> punct = {
        "{", "}", "(", ")", ",", ";", "=", "[", "]", ".", "-", "+", "#LINE 3\n", "#CODE 0\n", "#ENDCODE 0\n",
        "#NATIVE 0\n", "#ENDNATIVE 0\n", "#BINDING 0\n", "#ENDBINDING\n", "#VERTEX PNT\n", "%MAXCONST", "\n", "MOV",
        "ADD", "DP4", "MAD", "SWZ", "ARL", "POW", "R0", "c[0]", "v[0]", "o[HPOS]", "A0.x", ".xyzw", "1.0", "-2",
        "\xe9t\xe9", "\x80\xff"};
    return SampleOf(r, vpCorpus, w, punct, "#CODE 0\n");
}
// a native program as the pre-parser writes it (its native text sometimes longer)
static std::string VpMadeUp(std::mt19937& r)
{
    std::string s = vpNative[r() % vpNative.size()];
    if (r() % 2) {
        const size_t at = s.find("#ENDNATIVE");
        if (at != std::string::npos)
            for (int k = static_cast<int>(r() % 6); k; --k) s.insert(at, "MOV result.position, vertex.position;\n");
    }
    return s;
}

static int vpCalls;
static int vpTypeCalls, vpNativeCalls, vpCrashes;  // on the original's side: how often the run got that far
static int vpTypeSet;  // slot 29's answer this scenario
static void DumpSymbol(const char* tag, const void* sym);
static bool __cdecl ProgNative(void* self, const void* name, const char* text)
{
    const bool ok = (text ? std::strlen(text) : 0) % 5 != 0;
    vpNativeCalls += cur == 1;
    Log("  native %s name %s [%s] -> %d\n", Name(self).c_str(), NodeText(*static_cast<void* const*>(name)).c_str(),
        text ? text : "(null)", ok);
    return ok;
}
static bool __cdecl ProgCompile(void* self)
{
    const bool ok = ++vpCalls % 4 != 0;
    Log("  compile %s -> %d\n", Name(self).c_str(), ok);
    return ok;
}
static void __cdecl ProgNames(void*, const void* list) { DumpNames("program names", *static_cast<void* const*>(list)); }
static bool __cdecl ProgSymbols(void* self, const void* table, bool native)
{
    const auto* t = static_cast<const unsigned*>(table);
    Log("  symbols %s native %d #%u:\n", Name(self).c_str(), native, t[0]);
    for (unsigned i = 0; i < t[0] && i < 400; ++i) DumpSymbol("   ", reinterpret_cast<void* const*>(t[2])[i]);
    const bool ok = ++vpCalls % 5 != 0;
    Log("  -> %d\n", ok);
    return ok;
}
static void DumpOperand(const void* reg, int depth)
{
    if (!reg) {
        Log(" null");
        return;
    }
    const auto* r = static_cast<const int*>(reg);
    const char* sw = reinterpret_cast<const char*>(r[2]);
    Log(" {%s sym %s sw %s [%s] flag %d", Name(reg).c_str(), Name(reinterpret_cast<void*>(r[0])).c_str(),
        Name(sw).c_str(), sw ? sw : "", static_cast<const unsigned char*>(reg)[0xc]);
    if (const auto* o = reinterpret_cast<const int*>(r[1])) {
        Log(" offset %s %d", Name(o).c_str(), o[1]);
        if (depth < 3) DumpOperand(reinterpret_cast<void*>(o[0]), depth + 1);
    }
    Log("}");
}
static bool __cdecl ProgInstructions(void* self, const void* array)
{
    const auto* a = static_cast<const unsigned*>(array);
    Log("  instructions %s #%u:\n", Name(self).c_str(), a[0]);
    for (unsigned i = 0; i < a[0] && i < 400; ++i) {
        const auto* ins = reinterpret_cast<const int*>(reinterpret_cast<void* const*>(a[2])[i]);
        if (!ins) {
            Log("   null\n");
            continue;
        }
        Log("   %s op %d ops", Name(ins).c_str(), ins[2]);
        if (const auto* l = reinterpret_cast<const unsigned*>(ins[1])) {
            Log(" %s #%u:", Name(l).c_str(), l[0]);
            for (unsigned j = 0; j < l[0] && j < 16; ++j) DumpOperand(reinterpret_cast<void* const*>(l[2])[j], 0);
        }
        Log("\n");
    }
    const bool ok = ++vpCalls % 6 != 0;
    Log("  -> %d\n", ok);
    return ok;
}
static void __cdecl ProgVertexType(void* self, const void* vt) { vpTypeCalls += cur == 1; Log("  vertex type %s %s\n", Name(self).c_str(), Name(vt).c_str()); }
static int __cdecl ProgHasType(void*) { return vpTypeSet; }
static int __cdecl ProgRegisters(void*) { return 96; }
static void* progVtable[31];
static struct {
    void** vt = progVtable;
    void* vb = nullptr;
    void* display = &fakeDisplay;
    unsigned char rest[0x34] = {};
} fakeProg;
static void*& ProgProgramName() { return *reinterpret_cast<void**>(reinterpret_cast<char*>(&fakeProg) + 0x14); }

static bool vpEngineRun;
static void DumpSymbol(const char* tag, const void* sym)
{
    if (!sym) {
        Log("%s null\n", tag);
        return;
    }
    const auto* w = static_cast<const int*>(sym);
    const auto* b = static_cast<const unsigned char*>(sym);
    // a program parameter's value (+0x24) is the original's stack garbage when the text gives none: not compared;
    // nor is +0x20 in the driver-alone runs (after a syntax error the original's unset value holds what formatting
    // the message left on the stack)
    const bool garbage = w[0x12] == 1;
    Log("%s %s class %d type %d f08 %d %d %d %d hint %d f1c %d f20 %d flags %x v %08x %08x %08x %08x f34 %d name %s "
        "refs %d alias %s next %s\n",
        tag, Name(sym).c_str(), w[0x12], w[1], w[2], w[3], w[4], w[5], w[6], w[7], vpEngineRun ? 0 : b[0x20], b[0x21],
        garbage ? 0 : w[9], garbage ? 0 : w[10], garbage ? 0 : w[11], garbage ? 0 : w[12], w[13],
        NodeText(reinterpret_cast<void*>(w[14])).c_str(), w[15], Name(reinterpret_cast<void*>(w[0])).c_str(),
        Name(reinterpret_cast<void*>(w[16])).c_str());
}

static void LogVpState(int s)
{
    const unsigned char* st = LexStatics(s);
    const int* t = reinterpret_cast<const int*>(st + 0x34c);
    Log("  scanner depth %d cond %d count %d flag %d stack %d %d\n", t[0], t[1], t[2], st[0x2db],
        reinterpret_cast<const int*>(st + 0x228)[0], reinterpret_cast<const int*>(st + 0x228)[1]);
}
static void ResetVp(int s)
{
    ResetLexStatics(s);
}

static void OrigVpCtor(void* obj, void* file)
{
    void* f = OrigAt(0x537b0);  // vpFlexLexer(file), then what VertexProgram::Load sets
    __asm {
        pushad
        mov ecx, file
        push obj
        call f
        popad
    }
    auto* o = static_cast<unsigned char*>(obj);
    *reinterpret_cast<void**>(o) = OrigAt(0x73c08);
    *reinterpret_cast<void**>(o + 0x7c) = nullptr;
    *reinterpret_cast<void**>(o + 0x80) = nullptr;
    *reinterpret_cast<int*>(o + 0x84) = 1;
    *reinterpret_cast<void**>(o + 0x88) = nullptr;
}
static bool OrigVpRun(void* obj, const void* program, const void* file, void* prog)
{
    void* f = OrigAt(0x54f00);
    __asm {
        pushad
        push prog
        push file
        push program
        mov ecx, obj
        call f
        add esp, 12
        mov asmResult, al
        popad
    }
    return asmResult != 0;
}
static void OrigVpDtor(void* obj)
{
    void* f = OrigAt(0x496d0);
    __asm {
        pushad
        push obj
        call f
        popad
    }
}
static struct {
    void (*ctor)(void*, void*);
    bool (*run)(void*, const void*, const void*, void*);
    void (*dtor)(void*);
    int (*line)(void*);
    void (*destroy)(void*);
} ourVp;
static void LoadOurVp()
{
    if (ourVp.ctor) return;
    ourVp.ctor = As<decltype(ourVp.ctor)>(OurSym("_RsTestVpCtor"));
    ourVp.run = As<decltype(ourVp.run)>(OurSym("_RsTestVpRun"));
    ourVp.dtor = As<decltype(ourVp.dtor)>(OurSym("_RsTestVpDtor"));
    ourVp.line = As<decltype(ourVp.line)>(OurSym("_RsTestVpLine"));
    ourVp.destroy = As<decltype(ourVp.destroy)>(OurSym("_RsTestVpDestroy"));
    for (void*& slot : progVtable) slot = reinterpret_cast<void*>(&FakeFalse);
    progVtable[3] = reinterpret_cast<void*>(&ProgNative);
    progVtable[4] = reinterpret_cast<void*>(&ProgCompile);
    progVtable[12] = reinterpret_cast<void*>(&ProgNames);
    progVtable[13] = reinterpret_cast<void*>(&ProgSymbols);
    progVtable[14] = reinterpret_cast<void*>(&ProgInstructions);
    progVtable[28] = reinterpret_cast<void*>(&ProgVertexType);
    progVtable[29] = reinterpret_cast<void*>(&ProgHasType);
    progVtable[30] = reinterpret_cast<void*>(&ProgRegisters);
}

static int GuardedVpRun(int s, void* obj, const void* program, const void* file)
{
    __try {
        return s == 0 ? ourVp.run(obj, program, file, &fakeProg) : OrigVpRun(obj, program, file, &fakeProg);
    } __except (FaultAt(GetExceptionInformation())) {
        return -1;
    }
}

static void RunVp(int s, unsigned seed)
{
    std::mt19937 r(seed);
    LoadOurVp();
    if (r() % 4) ResetVp(s);
    names[s][&fakeProg] = "prog";
    names[s][&fakeDisplay] = "display";
    for (int k = 0; k < 3; ++k) {
        const int kind = static_cast<int>(r() % 4);
        const std::string text = kind == 0 ? vpCorpus[r() % vpCorpus.size()] : kind == 1 ? VpMadeUp(r) : VpSample(r);
        vpTypeSet = r() % 4 != 0;  // (0 asks for VertexType::GetVertexType, which faults here: no types are known)
        void* programNode = CreateNode(*cache, reinterpret_cast<const unsigned char*>(r() % 4 ? "prog" : "x"));
        void* fileNode = r() % 2 ? CreateNode(*cache, reinterpret_cast<const unsigned char*>("vp.txt")) : nullptr;
        void* file = RealNew(0x14);
        MemFileCtor(file, nullptr, text.c_str(), static_cast<unsigned>(text.size()), false, 0);
        void* obj = RealNew(0x400);
        std::memset(obj, 0xcd, 0x400);
        vpCalls = 0;
        Log("vp %d: %u bytes, has type %d\n", k, static_cast<unsigned>(text.size()), vpTypeSet);
        if (s == 0)
            ourVp.ctor(obj, file);
        else
            OrigVpCtor(obj, file);
        const int ok = GuardedVpRun(s, obj, &programNode, &fileNode);
        vpCrashes += s == 1 && ok < 0;
        if (s == 1 && kind <= 1 && std::getenv("RS_U2B_SHOWVP"))
            std::printf("==== made-up program (ok %d, line %d):\n%s\n", ok,
                        *reinterpret_cast<int*>(static_cast<char*>(obj) + 0x84), text.c_str());
        if (ok < 0) Log("  crashed\n");
        Log("  ok %d line %d program %s\n", ok,
            s == 0 ? ourVp.line(obj) : *reinterpret_cast<int*>(static_cast<char*>(obj) + 0x84),
            NodeText(ProgProgramName()).c_str());
        LogVpState(s);
        if (ok < 0) ResetVp(s);
        if (ProgProgramName()) DestroyNode(*cache, ProgProgramName());
        ProgProgramName() = nullptr;
        if (s == 1) ++(ok > 0 ? parsedOk : parsedBad);
        if (s == 0)
            ourVp.dtor(obj);
        else
            OrigVpDtor(obj);
        RealDelete(obj);
        MemFileDtor(file);
        RealDelete(file);
        DestroyNode(*cache, programNode);
        if (fileNode) DestroyNode(*cache, fileNode);
    }
}

// ---- the vertex parser's driver alone: context {version, program (+4), symbols (+8), instructions (+0x14),
// file, program name, parser (+0x28), names (+0x2c), vertex type (+0x30), blocks, native (+0x40), error (+0x44)}

static void OrigVpDriver(void* stack, int major, void* minor, void* ctx)
{
    void* f = OrigAt(0x518c0);
    __asm {
        pushad
        push major
        mov edi, stack
        mov ecx, minor
        mov edx, ctx
        call f
        add esp, 4
        popad
    }
}
static void VpPrepare(unsigned char* ctx, int)
{
    *reinterpret_cast<void**>(ctx + 4) = &fakeProg;
    *reinterpret_cast<const char**>(ctx + 0x20) = "file";
    *reinterpret_cast<const char**>(ctx + 0x24) = "prog";
}
static void VpFinish(int side, unsigned char* ctx, void*)
{
    if (side >= 0) {
        Log("  ctx native %s [%s] type %s\n", Name(*reinterpret_cast<void**>(ctx + 0x40)).c_str(),
            *reinterpret_cast<char**>(ctx + 0x40) ? *reinterpret_cast<char**>(ctx + 0x40) : "",
            NodeText(*reinterpret_cast<void**>(ctx + 0x30)).c_str());
        ProgSymbols(&fakeProg, ctx + 8, false);
        ProgInstructions(&fakeProg, ctx + 0x14);
        DumpNames("names", *reinterpret_cast<void**>(ctx + 0x2c));
    }
    if (side == 1) {
        void* f = OrigAt(0x32050);
        __asm {
            pushad
            push ctx
            call f
            popad
        }
    } else {
        LoadOurVp();
        ourVp.destroy(ctx);
    }
}

static void* VpParser(int)  // what the error messages read: the file (+0x7c) and program (+0x80) names, the line
{
    void* parser = RealNew(0x200);
    std::memset(parser, 0, 0x200);
    *reinterpret_cast<int*>(static_cast<char*>(parser) + 0x84) = 7;
    return parser;
}

static void RunVpEngine(int s, unsigned seed)
{
    static const EngineSpec spec = {"_RsTestVpParse", "_RsTestVpFree", 0x518c0, 0x4e5c0, 0x64, 0x2c, 0x44,
                                    VpPrepare, VpFinish, VpParser, FreeTpParser, OrigVpDriver, 0x96c};
    LoadOurVp();
    names[s][&fakeProg] = "prog";
    vpCalls = 0;
    vpEngineRun = true;
    RunEngine(s, seed, spec);
    vpEngineRun = false;
}

static void WriteSources()
{
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    tempDir = tmp;
    // files for half the names (the rest fail to open)
    const char* bodies[] = {"lightDir fog x\n" TAIL, TAIL, "vp vp\r\nc0" TAIL, "aaaa" TAIL,
                            "x fog lightDir vp aa q c0 xx" TAIL};
    for (int i = 0; i < kNames; i += 2) {
        std::string path = tempDir + "rs_u2b_" + kTexts[i] + ".src";
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) continue;
        std::fputs(bodies[i % 5], f);
        std::fclose(f);
    }
}

// an unexpected fault: where, and the end of the side's log
static LONG WINAPI Crashed(EXCEPTION_POINTERS* e)
{
    const auto at = reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress);
    for (int s = 0; s < 2; ++s) {
        const auto b = reinterpret_cast<uintptr_t>(mod[s]);
        if (at - b < 0x200000) std::printf("fault in %s +0x%x\n", s ? "the original" : "ours", static_cast<unsigned>(at - b));
    }
    HMODULE m = nullptr;
    char path[MAX_PATH] = "";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<const char*>(e->ExceptionRecord->ExceptionAddress), &m))
        GetModuleFileNameA(m, path, MAX_PATH);
    std::printf("fault %08lx at %p (%s +0x%x), side %d; log end:\n", e->ExceptionRecord->ExceptionCode,
                e->ExceptionRecord->ExceptionAddress, path,
                static_cast<unsigned>(at - reinterpret_cast<uintptr_t>(m)), cur);
    if (cur >= 0) {
        const std::string& t = logText[cur];
        std::printf("%s\n", t.substr(t.size() > 3000 ? t.size() - 3000 : 0).c_str());
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(&Crashed);
    if (argc < 2) {
        std::printf("usage: diff_rs_u2b <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 400;
    SetDllDirectoryA(argv[1]);
    std::string dir = argv[1];
    LoadLibraryA((dir + "\\RenderSystem_7c_orig.dll").c_str());
    binDir = dir;
    if (argc > 3) dumpPart = argv[3];
    mod[0] = LoadLibraryA((dir + "\\RenderSystem_7c.dll").c_str());
    mod[1] = GetModuleHandleA("RenderSystem_7c_orig.dll");
    ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    RealNew = static_cast<NewFn>(Proc(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    RealDelete = static_cast<DeleteFn>(Proc(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    CreateNode = As<decltype(CreateNode)>(Proc(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    DestroyNode = As<decltype(DestroyNode)>(Proc(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z"));
    cache = static_cast<void**>(Proc(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    AnsiFromText = As<decltype(AnsiFromText)>(Proc(ks, "??0AnsiString@Jet@@QAE@PBD@Z"));
    AnsiDtor = As<decltype(AnsiDtor)>(Proc(ks, "??1AnsiString@Jet@@QAE@XZ"));
    AnsiText = As<decltype(AnsiText)>(Proc(ks, "??BAnsiString@Jet@@QBAPBDXZ"));
    AnsiAssign = As<decltype(AnsiAssign)>(Proc(ks, "??4AnsiString@Jet@@QAAABV01@PBD@Z"));
    ReleaseRef = As<decltype(ReleaseRef)>(Proc(ks, "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ"));
    AddRef = As<decltype(AddRef)>(Proc(ks, "?AddReference@ReferenceCount@Jet@@QBEXXZ"));
    for (int i = 0; i < kNames; ++i)
        nodes[i] = *kTexts[i] ? CreateNode(*cache, reinterpret_cast<const unsigned char*>(kTexts[i])) : nullptr;
    WriteSources();
    MemFileCtor = As<decltype(MemFileCtor)>(Proc(ks, "??0MemoryFile@Jet@@QAE@PBXI_NH@Z"));
    MemFileDtor = As<decltype(MemFileDtor)>(Proc(ks, "??1MemoryFile@Jet@@QAE@XZ"));
    CollectCorpus();
    CollectTpCorpus();
    CollectVpCorpus();
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
    }
    struct {
        const char* name;
        void (*run)(int, unsigned);
        int scale;
    } parts[] = {{"languages", RunLanguages, 4}, {"records", RunRecords, 2}, {"quick", RunQuick, 2}, {"qkengine", RunQuickEngine, 2},
                 {"pre", RunPre, 2}, {"ppengine", RunPreEngine, 2}, {"tp", RunTp, 3}, {"tpengine", RunTpEngine, 2},
                 {"vp", RunVp, 3}, {"vpengine", RunVpEngine, 2}};
    const char* only = std::getenv("RS_U2B_ONLY");  // a comma list of parts to run (mutation runs)
    for (auto& p : parts) {
        if (only && !std::strstr((std::string(",") + only + ",").c_str(), (std::string(",") + p.name + ",").c_str()))
            continue;
        for (int round = 0; round < rounds * p.scale / 4 + 1; ++round) {
            const unsigned seed = rng();
            for (int s = 0; s < 2; ++s) {
                cur = s;
                logText[s].clear();
                names[s].clear();
                allocCount[s] = 0;
                p.run(s, seed);
                cur = -1;
            }
            Compare(p.name, round);
        }
        std::printf("-- %s done (parses ok %d, failed %d)\n", p.name, parsedOk, parsedBad);
        if (vpTypeCalls || vpNativeCalls || vpCrashes)
            std::printf("   vertex types set %d, native programs %d, runs that fault %d\n", vpTypeCalls, vpNativeCalls,
                        vpCrashes);
        vpTypeCalls = vpNativeCalls = vpCrashes = 0;
        parsedOk = parsedBad = 0;
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
