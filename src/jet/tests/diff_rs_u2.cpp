// Differential test of RenderSystem_7c unit U2a (GfxSymbols.cpp): symbol records, the vertex symbol table,
// register operands, instructions and preprocessor defines, ours against RenderSystem_7c_orig.dll in one process.
//
// Both modules' imports of Jet::Alloc::New / Delete are hooked: memory a module allocates is filled with a pattern
// and numbered in order, and every free is logged with that number (or the test object it was), so ownership
// (which destructor frees what) is compared too. Each scenario runs the same random script on both sides with that
// side's exports; after each call the side logs results and the objects' fields, pointers written as names. The
// two logs must be equal.
//
// usage: diff_rs_u2.exe <bin dir with RenderSystem_7c.dll, RenderSystem_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

static HMODULE mod[2];  // 0 ours, 1 the original
static HMODULE ks;
static unsigned long long checks, bad;
static std::mt19937 rng(9200);
static int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }

static void* Proc(HMODULE m, const char* name)
{
    void* p = reinterpret_cast<void*>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}

// ---- KernelStandard and the heap hooks

using NewFn = void*(__cdecl*)(unsigned);
using DeleteFn = void(__cdecl*)(void*);
static NewFn RealNew;
static DeleteFn RealDelete;
static void* (*CreateNode)(void*, const unsigned char*);
static void** cache;
static void*(__fastcall* AnsiFromText)(void*, void*, const char*);
static void(__fastcall* AnsiDtor)(void*, void*);
static const char*(__cdecl* AnsiText)(const void*);

static int cur = -1;  // the side running
static std::string logText[2];
static std::map<const void*, std::string> names[2];  // pointer -> name, per side
static int allocCount[2];

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

// ---- interned names and texts

static const char* kTexts[] = {"pos", "normal", "c0", "tex0", "a", "matrix", "x"};
constexpr int kNames = 7;
static void* nodes[kNames];
static std::string NodeName(const void* n)
{
    if (!n) return "null";
    for (int i = 0; i < kNames; ++i)
        if (nodes[i] == n) return std::string("N") + kTexts[i];
    return "N?";
}

// ---- per-side entry points

#define F(name) Proc(mod[s], name)
template <class T>
static T As(void* p) { return reinterpret_cast<T>(p); }

struct Api {
    // symbols
    void*(__fastcall* symCtor)(void*, void*, int, bool);
    void*(__fastcall* vsymCtor)(void*, void*, int, int, bool);
    void*(__fastcall* tsymCtor)(void*, void*, int, int, bool);
    void(__fastcall* symClosure)(void*);
    void*(__fastcall* symCopy)(void*, void*, const void*);
    void*(__fastcall* vsymCopy)(void*, void*, const void*);
    void*(__fastcall* symAssign)(void*, void*, const void*);
    void*(__fastcall* vsymAssign)(void*, void*, const void*);
    void(__fastcall* symDtor)(void*, void*);
    void(__cdecl* setName)(void*, const char*);
    const void*(__cdecl* getName)(const void*);
    void(__cdecl* addRef)(void*);
    void(__cdecl* decRef)(void*);
    bool(__cdecl* isAnon)(const void*);
    bool(__cdecl* isRef)(const void*);
    bool(__cdecl* like)(const void*, const void*);
    bool(__cdecl* vlike)(const void*, const void*);
    void(__cdecl* append)(void*, void*);
    void*(__cdecl* nextSym)(const void*);
    const void*(__cdecl* unAlias)(const void*);
    void(__cdecl* setApi)(const void*, void*);
    void*(__cdecl* getApi)(const void*);
    void*(__cdecl* zeroApi)(const void*);
    unsigned long(__cdecl* getHint)(const void*);
    void(__cdecl* setHint)(void*, unsigned long);
    // symbol table
    void*(__fastcall* tabCtor)(void*, void*);
    void*(__fastcall* tabCopy)(void*, void*, const void*);
    void*(__fastcall* tabAssign)(void*, void*, const void*);
    void(__fastcall* tabDtor)(void*, void*);
    void*(__cdecl* tabFind)(const void*, const void*);
    void*(__cdecl* tabFindLike)(const void*, const void*);
    void(__cdecl* tabAdd)(void*, void*);
    // registers and instructions
    void*(__fastcall* regCtor)(void*, void*, void*, char*, bool, void*, void*);
    void*(__fastcall* regCopy)(void*, void*, const void*);
    void*(__fastcall* regAssign)(void*, void*, const void*);
    void(__fastcall* regDtor)(void*, void*);
    void*(__fastcall* offCtor)(void*, void*, void*, int);
    void(__fastcall* offDtor)(void*, void*);
    void*(__fastcall* insCtor)(void*, void*, void*);
    void*(__fastcall* vinsCtor)(void*, void*, int, void*);
    void*(__fastcall* tinsCtor)(void*, void*, int, void*);
    void*(__fastcall* insCopy)(void*, void*, const void*);
    void*(__fastcall* vinsCopy)(void*, void*, const void*);
    void*(__fastcall* tinsCopy)(void*, void*, const void*);
    void*(__fastcall* insAssign)(void*, void*, const void*);
    void*(__fastcall* vinsAssign)(void*, void*, const void*);
    // defines
    void*(__fastcall* defCtor)(void*, void*, const void*, const void*);
    void*(__fastcall* defCopy)(void*, void*, const void*);
    void*(__fastcall* defAssign)(void*, void*, const void*);
    void(__fastcall* defDtor)(void*, void*);
    void*(__fastcall* arrCtor)(void*, void*);
    void*(__fastcall* arrCopy)(void*, void*, const void*);
    void(__fastcall* arrDtor)(void*, void*);
    void*(__fastcall* arrAssign)(void*, void*, const void*);
    void*(__fastcall* ppaCtor)(void*, void*);
    void*(__fastcall* ppaCopy)(void*, void*, const void*);
    void(__fastcall* ppaDtor)(void*, void*);
    void*(__fastcall* ppaAssign)(void*, void*, const void*);
    void*(__cdecl* concat)(const void*, void*);
    void(__fastcall* add1)(void*, void*, const void*);
    void(__fastcall* addN)(void*, void*, const void*, unsigned);
    int(__fastcall* addEmpty)(void*, void*, unsigned);
    void*(__fastcall* alloc)(void*, void*, unsigned);
    void*(__fastcall* removeLast)(void*, void*, void*);
    void*(__fastcall* removeAt)(void*, void*, void*, void*);
    void(__fastcall* removeFast)(void*, void*, unsigned);
    void(__fastcall* removeAll)(void*, void*);
    void(__fastcall* reserve)(void*, void*, unsigned);
    void(__fastcall* grow)(void*, void*, unsigned);
    void(__fastcall* shrink)(void*, void*, unsigned);
    void(__fastcall* setCount)(void*, void*, unsigned);
    unsigned(__fastcall* count)(const void*);
    unsigned(__fastcall* size)(const void*);
    bool(__fastcall* asBool)(const void*);
    bool(__fastcall* isEmpty)(const void*);
    void*(__fastcall* at)(void*, void*, unsigned);
    void*(__fastcall* first)(void*, void*, void*);
    void*(__fastcall* last)(void*, void*, void*);
    void*(__fastcall* end)(void*, void*, void*);
    void*(__fastcall* index)(void*, void*, void*, unsigned);
} api[2];

static void LoadApi(int s)
{
    Api& a = api[s];
    a.symCtor = As<decltype(a.symCtor)>(F("??0SymbolEntry@Jet@@QAE@W4SymbolType@1@_N@Z"));
    a.vsymCtor = As<decltype(a.vsymCtor)>(F("??0VertexSymbolEntry@Jet@@QAE@W4VertexSymbolClass@1@W4SymbolType@1@_N@Z"));
    a.tsymCtor = As<decltype(a.tsymCtor)>(F("??0TextureSymbolEntry@Jet@@QAE@W4TextureSymbolClass@1@W4SymbolType@1@_N@Z"));
    a.symClosure = As<decltype(a.symClosure)>(F("??_FSymbolEntry@Jet@@QAEXXZ"));
    a.symCopy = As<decltype(a.symCopy)>(F("??0SymbolEntry@Jet@@QAE@ABU01@@Z"));
    a.vsymCopy = As<decltype(a.vsymCopy)>(F("??0VertexSymbolEntry@Jet@@QAE@ABU01@@Z"));
    a.symAssign = As<decltype(a.symAssign)>(F("??4SymbolEntry@Jet@@QAEAAU01@ABU01@@Z"));
    a.vsymAssign = As<decltype(a.vsymAssign)>(F("??4VertexSymbolEntry@Jet@@QAEAAU01@ABU01@@Z"));
    a.symDtor = As<decltype(a.symDtor)>(F("??1SymbolEntry@Jet@@QAE@XZ"));
    a.setName = As<decltype(a.setName)>(F("?SetName@SymbolEntry@Jet@@QAAXPBD@Z"));
    a.getName = As<decltype(a.getName)>(F("?GetName@SymbolEntry@Jet@@QBAABVPString@2@XZ"));
    a.addRef = As<decltype(a.addRef)>(F("?AddRef@SymbolEntry@Jet@@QAAXXZ"));
    a.decRef = As<decltype(a.decRef)>(F("?DecRef@SymbolEntry@Jet@@QAAXXZ"));
    a.isAnon = As<decltype(a.isAnon)>(F("?IsAnonymous@SymbolEntry@Jet@@QBA_NXZ"));
    a.isRef = As<decltype(a.isRef)>(F("?IsReferenced@SymbolEntry@Jet@@QBA_NXZ"));
    a.like = As<decltype(a.like)>(F("?LikeType@SymbolEntry@Jet@@QBA_NPBU12@@Z"));
    a.vlike = As<decltype(a.vlike)>(F("?LikeType@VertexSymbolEntry@Jet@@QBA_NPBU12@@Z"));
    a.append = As<decltype(a.append)>(F("?AppendSymbol@SymbolEntry@Jet@@QAAXPAU12@@Z"));
    a.nextSym = As<decltype(a.nextSym)>(F("?GetNextSymbol@SymbolEntry@Jet@@QBAPAU12@XZ"));
    a.unAlias = As<decltype(a.unAlias)>(F("?UnAlias@SymbolEntry@Jet@@QBAPBU12@XZ"));
    a.setApi = As<decltype(a.setApi)>(F("?SetApiData@SymbolEntry@Jet@@QBAXPAUApiDataBase@2@@Z"));
    a.getApi = As<decltype(a.getApi)>(F("?GetApiData@SymbolEntry@Jet@@QBAPAUApiDataBase@2@XZ"));
    a.zeroApi = As<decltype(a.zeroApi)>(F("?GetAndZeroApiData@SymbolEntry@Jet@@QBAPAUApiDataBase@2@XZ"));
    a.getHint = As<decltype(a.getHint)>(F("?GetHint@SymbolEntry@Jet@@QBAKXZ"));
    a.setHint = As<decltype(a.setHint)>(F("?SetHint@SymbolEntry@Jet@@QAAXK@Z"));
    a.tabCtor = As<decltype(a.tabCtor)>(F("??0VertexSymbolTable@Jet@@QAE@XZ"));
    a.tabCopy = As<decltype(a.tabCopy)>(F("??0VertexSymbolTable@Jet@@QAE@ABU01@@Z"));
    a.tabAssign = As<decltype(a.tabAssign)>(F("??4VertexSymbolTable@Jet@@QAEAAU01@ABU01@@Z"));
    a.tabDtor = As<decltype(a.tabDtor)>(F("??1VertexSymbolTable@Jet@@QAE@XZ"));
    a.tabFind = As<decltype(a.tabFind)>(F("?Find@VertexSymbolTable@Jet@@QBAPAUVertexSymbolEntry@2@ABVPString@2@@Z"));
    a.tabFindLike = As<decltype(a.tabFindLike)>(F("?FindLike@VertexSymbolTable@Jet@@QBAPAUVertexSymbolEntry@2@PBU32@@Z"));
    a.tabAdd = As<decltype(a.tabAdd)>(F("?AddSymbol@VertexSymbolTable@Jet@@QAAXPAUVertexSymbolEntry@2@@Z"));
    a.regCtor = As<decltype(a.regCtor)>(F("??0RegisterItem@Jet@@QAE@PAUSymbolEntry@1@PAD_NPAURegisterModifier@1@3@Z"));
    a.regCopy = As<decltype(a.regCopy)>(F("??0RegisterItem@Jet@@QAE@PBU01@@Z"));
    a.regAssign = As<decltype(a.regAssign)>(F("??4RegisterItem@Jet@@QAEAAU01@ABU01@@Z"));
    a.regDtor = As<decltype(a.regDtor)>(F("??1RegisterItem@Jet@@QAE@XZ"));
    a.offCtor = As<decltype(a.offCtor)>(F("??0OffsetAddress@Jet@@QAE@PAURegisterItem@1@H@Z"));
    a.offDtor = As<decltype(a.offDtor)>(F("??1OffsetAddress@Jet@@QAE@XZ"));
    a.insCtor = As<decltype(a.insCtor)>(F("??0Instruction@Jet@@QAE@PAV?$Array@PAURegisterItem@Jet@@$0IA@@1@@Z"));
    a.vinsCtor = As<decltype(a.vinsCtor)>(F("??0VertexInstruction@Jet@@QAE@W4VertexOpcode@1@PAV?$Array@PAURegisterItem@Jet@@$0IA@@1@@Z"));
    a.tinsCtor = As<decltype(a.tinsCtor)>(F("??0TextureInstruction@Jet@@QAE@W4TextureOpcode@1@PAV?$Array@PAURegisterItem@Jet@@$0IA@@1@@Z"));
    a.insCopy = As<decltype(a.insCopy)>(F("??0Instruction@Jet@@QAE@ABU01@@Z"));
    a.vinsCopy = As<decltype(a.vinsCopy)>(F("??0VertexInstruction@Jet@@QAE@ABU01@@Z"));
    a.tinsCopy = As<decltype(a.tinsCopy)>(F("??0TextureInstruction@Jet@@QAE@ABU01@@Z"));
    a.insAssign = As<decltype(a.insAssign)>(F("??4Instruction@Jet@@QAEAAU01@ABU01@@Z"));
    a.vinsAssign = As<decltype(a.vinsAssign)>(F("??4VertexInstruction@Jet@@QAEAAU01@ABU01@@Z"));
    a.defCtor = As<decltype(a.defCtor)>(F("??0PreProcessorDefine@Jet@@QAE@ABVAnsiString@1@0@Z"));
    a.defCopy = As<decltype(a.defCopy)>(F("??0PreProcessorDefine@Jet@@QAE@ABU01@@Z"));
    a.defAssign = As<decltype(a.defAssign)>(F("??4PreProcessorDefine@Jet@@QAEAAU01@ABU01@@Z"));
    a.defDtor = As<decltype(a.defDtor)>(F("??1PreProcessorDefine@Jet@@QAE@XZ"));
#define ARR "?$Array@UPreProcessorDefine@Jet@@$02@Jet@@"
    a.arrCtor = As<decltype(a.arrCtor)>(F("??0" ARR "QAE@XZ"));
    a.arrCopy = As<decltype(a.arrCopy)>(F("??0" ARR "QAE@ABV01@@Z"));
    a.arrDtor = As<decltype(a.arrDtor)>(F("??1" ARR "QAE@XZ"));
    a.arrAssign = As<decltype(a.arrAssign)>(F("??4" ARR "QAEAAV01@ABV01@@Z"));
    a.ppaCtor = As<decltype(a.ppaCtor)>(F("??0PreProcessorArray@Jet@@QAE@XZ"));
    a.ppaCopy = As<decltype(a.ppaCopy)>(F("??0PreProcessorArray@Jet@@QAE@ABU01@@Z"));
    a.ppaDtor = As<decltype(a.ppaDtor)>(F("??1PreProcessorArray@Jet@@QAE@XZ"));
    a.ppaAssign = As<decltype(a.ppaAssign)>(F("??4PreProcessorArray@Jet@@QAEAAU01@ABU01@@Z"));
    a.concat = As<decltype(a.concat)>(F("?ConcatenateStrings@PreProcessorArray@Jet@@QBA?AVAnsiString@2@XZ"));
    a.add1 = As<decltype(a.add1)>(F("?Add@" ARR "QAEXABUPreProcessorDefine@2@@Z"));
    a.addN = As<decltype(a.addN)>(F("?Add@" ARR "QAEXPBUPreProcessorDefine@2@I@Z"));
    a.addEmpty = As<decltype(a.addEmpty)>(F("?AddEmpty@" ARR "QAEHI@Z"));
    a.alloc = As<decltype(a.alloc)>(F("?Alloc@" ARR "QAEPAUPreProcessorDefine@2@I@Z"));
    a.removeLast = As<decltype(a.removeLast)>(F("?Remove@" ARR "QAE?AUPreProcessorDefine@2@XZ"));
    a.removeAt = As<decltype(a.removeAt)>(F("?Remove@" ARR "QAE?AVIterator@12@AAV312@@Z"));
    a.removeFast = As<decltype(a.removeFast)>(F("?RemoveFast@" ARR "QAEXI@Z"));
    a.removeAll = As<decltype(a.removeAll)>(F("?RemoveAll@" ARR "QAEXXZ"));
    a.reserve = As<decltype(a.reserve)>(F("?Reserve@" ARR "QAEXI@Z"));
    a.grow = As<decltype(a.grow)>(F("?Grow@" ARR "QAEXI@Z"));
    a.shrink = As<decltype(a.shrink)>(F("?Shrink@" ARR "QAEXI@Z"));
    a.setCount = As<decltype(a.setCount)>(F("?SetCount@" ARR "QAEXI@Z"));
    a.count = As<decltype(a.count)>(F("?Count@" ARR "QBEIXZ"));
    a.size = As<decltype(a.size)>(F("?Size@" ARR "QBEIXZ"));
    a.asBool = As<decltype(a.asBool)>(F("??B" ARR "QBE_NXZ"));
    a.isEmpty = As<decltype(a.isEmpty)>(F("?IsEmpty@" ARR "QBE_NXZ"));
    a.at = As<decltype(a.at)>(F("??A" ARR "QAEAAUPreProcessorDefine@1@I@Z"));
    a.first = As<decltype(a.first)>(F("?First@" ARR "QAE?AVIterator@12@XZ"));
    a.last = As<decltype(a.last)>(F("?Last@" ARR "QAE?AVIterator@12@XZ"));
    a.end = As<decltype(a.end)>(F("?End@" ARR "QAE?AVIterator@12@XZ"));
    a.index = As<decltype(a.index)>(F("?Index@" ARR "QAE?AVIterator@12@I@Z"));
}

// ---- fake API data: a deleting destructor that logs

static int apiIds;
static void* __fastcall ApiDelete(void* self, void*, unsigned flags)
{
    Log("  api delete %s %u\n", Name(self).c_str(), flags);
    names[cur].erase(self);
    RealDelete(self);
    return self;
}
static void* apiVtable[1] = {reinterpret_cast<void*>(&ApiDelete)};
static void* NewApi()
{
    void** p = static_cast<void**>(TestNew(8, "D%d", apiIds++));
    p[0] = apiVtable;
    return p;
}

// ---- part 1: symbols

constexpr int kSyms = 8;
struct SymState {
    void* s[kSyms];
    int kind[kSyms];  // 0 SymbolEntry, 1 vertex, 2 texture
    bool owned[kSyms];
};

static void DumpSym(const char* tag, const void* p)
{
    const unsigned char* b = static_cast<const unsigned char*>(p);
    auto u = [&](int o) { return *reinterpret_cast<const unsigned*>(b + o); };
    auto ptr = [&](int o) { return Name(*reinterpret_cast<void* const*>(b + o)); };
    Log("  %s alias %s type %x f %x %x %x %x hint %x f1c %x f20 %x flags %x f24 %x %x %x %x f34 %x name %s refs %d next %s api %s",
        tag, ptr(0).c_str(), u(4), u(8), u(0xc), u(0x10), u(0x14), u(0x18), u(0x1c), b[0x20], b[0x21], u(0x24), u(0x28),
        u(0x2c), u(0x30), u(0x34), NodeName(*reinterpret_cast<void* const*>(b + 0x38)).c_str(), static_cast<int>(u(0x3c)),
        ptr(0x40).c_str(), ptr(0x44).c_str());
    Log("\n");
}

static void RunSymbols(int s, unsigned seed)
{
    const Api& a = api[s];
    std::mt19937 r(seed);  // the same script on both sides
    auto P = [&](int lo, int hi) { return lo + static_cast<int>(r() % static_cast<unsigned>(hi - lo + 1)); };
    SymState st{};
    for (int i = 0; i < kSyms; ++i) {
        st.kind[i] = P(0, 2);
        st.s[i] = TestNew(st.kind[i] ? 0x4c : 0x48, "S%d", i);
        const int t = P(0, 1);
        const bool ref = P(0, 1) != 0;
        switch (st.kind[i] ? st.kind[i] : P(0, 1) * 3) {
        case 0: a.symCtor(st.s[i], nullptr, t, ref); break;
        case 3: a.symClosure(st.s[i]); break;
        case 1: a.vsymCtor(st.s[i], nullptr, P(0, 3), t, ref); break;
        default: a.tsymCtor(st.s[i], nullptr, P(0, 3), t, ref); break;
        }
    }
    for (int i = 0; i < kSyms; ++i) DumpSym(names[s][st.s[i]].c_str(), st.s[i]);
    const int ops = P(10, 60);
    for (int k = 0; k < ops; ++k) {
        const int op = P(0, 18), i = P(0, kSyms - 1), j = P(0, kSyms - 1);
        void* x = st.s[i];
        void* y = st.s[j];
        Log("op %d %d %d\n", op, i, j);
        switch (op) {
        case 0: {
            const int t = P(-1, kNames - 1);
            a.setName(x, t < 0 ? nullptr : kTexts[t]);
            break;
        }
        case 1: a.addRef(x); break;
        case 2: a.decRef(x); break;
        case 3: Log("  anon %d ref %d\n", a.isAnon(x), a.isRef(x)); break;
        case 4: {  // and against a raw copy of x with one compared (or ignored) field changed
            Log("  like %d\n", a.like(x, y));
            unsigned char probe[0x4c];
            std::memcpy(probe, x, sizeof probe);
            const int offs[] = {4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x21, 0x34, 0x3c};
            const int o = offs[P(0, 10)];
            probe[o] ^= o == 0x21 ? static_cast<unsigned char>(1 << P(0, 2)) : 1;
            Log("  like probe %x %d\n", o, a.like(x, probe));
            break;
        }
        case 5:
            if (st.kind[i] == 1 && st.kind[j] == 1) Log("  vlike %d\n", a.vlike(x, y));
            break;
        case 6:  // append an unowned symbol without a chain of its own
            if (i != j && !st.owned[j] && !*reinterpret_cast<void**>(static_cast<char*>(y) + 0x40)) {
                a.append(x, y);
                st.owned[j] = true;
            }
            break;
        case 7: Log("  next %s unalias %s\n", Name(a.nextSym(x)).c_str(), Name(a.unAlias(x)).c_str()); break;
        case 8: {  // an alias to a symbol without one (owning when that symbol is free)
            if (i == j || *static_cast<void**>(x) || *static_cast<void**>(y)) break;
            *static_cast<void**>(x) = y;
            if (!st.owned[j] && P(0, 1)) {
                static_cast<unsigned char*>(x)[0x21] |= 4;
                st.owned[j] = true;
            }
            break;
        }
        case 9:
            if (!*reinterpret_cast<void**>(static_cast<char*>(x) + 0x44)) a.setApi(x, NewApi());
            break;
        case 10: Log("  api %s\n", Name(a.getApi(x)).c_str()); break;
        case 11: {
            void* d = a.zeroApi(x);
            Log("  zero %s\n", Name(d).c_str());
            if (d) ApiDelete(d, nullptr, 1);
            break;
        }
        case 12: a.setHint(x, r()); Log("  hint %lx\n", a.getHint(x)); break;
        case 13: Log("  name %s\n", NodeName(*static_cast<void* const*>(a.getName(x))).c_str()); break;
        case 14:
        case 15: {  // a copy: dumped, then its pointers cleared and destroyed
            const bool vertex = st.kind[i] == 1 && op == 15;
            void* c = TestNew(0x4c, "C%d", k);
            (vertex ? a.vsymCopy : a.symCopy)(c, nullptr, x);
            DumpSym("copy", c);
            std::memset(c, 0, 4);
            std::memset(static_cast<char*>(c) + 0x40, 0, 8);
            a.symDtor(c, nullptr);
            RealDelete(c);
            names[s].erase(c);
            break;
        }
        case 16:
        case 17: {  // an assignment into a fresh symbol
            const bool vertex = st.kind[i] == 1 && op == 17;
            void* c = TestNew(0x4c, "C%d", k);
            if (vertex)
                a.vsymCtor(c, nullptr, 0, 0, false);
            else
                a.symCtor(c, nullptr, 0, false);
            (vertex ? a.vsymAssign : a.symAssign)(c, nullptr, x);
            DumpSym("assigned", c);
            std::memset(c, 0, 4);
            std::memset(static_cast<char*>(c) + 0x40, 0, 8);
            a.symDtor(c, nullptr);
            RealDelete(c);
            names[s].erase(c);
            break;
        }
        case 18: {  // the fields LikeType compares, set to a few values so that some symbols match
            unsigned char* b = static_cast<unsigned char*>(x);
            const int f = P(0, 5), v = P(0, 1);
            if (f == 0) *reinterpret_cast<int*>(b + 4) = v;
            if (f == 1) *reinterpret_cast<int*>(b + 0xc) = v - 1;
            if (f == 2) *reinterpret_cast<int*>(b + 0x10) = v - 1;
            if (f == 3) *reinterpret_cast<int*>(b + 0x14) = v - 1;
            if (f == 4) *reinterpret_cast<int*>(b + 0x34) = v;
            if (f == 5) b[0x21] = static_cast<unsigned char>((b[0x21] & ~2) | (v << 1));
            if (st.kind[i] == 1 && P(0, 1)) *reinterpret_cast<int*>(b + 0x48) = v;
            break;
        }
        }
        DumpSym(names[s][x].c_str(), x);
    }
    for (int i = 0; i < kSyms; ++i) {  // the unowned ones; their chains and owned aliases go with them
        if (st.owned[i]) continue;
        Log("destroy S%d\n", i);
        a.symDtor(st.s[i], nullptr);
        HookDelete(st.s[i]);
    }
}

// ---- part 2: the vertex symbol table

static void RunTable(int s, unsigned seed)
{
    const Api& a = api[s];
    std::mt19937 r(seed);
    auto P = [&](int lo, int hi) { return lo + static_cast<int>(r() % static_cast<unsigned>(hi - lo + 1)); };
    void* t = TestNew(0xc, "T%d", 0);
    a.tabCtor(t, nullptr);
    const int n = P(0, 300);
    for (int i = 0; i < n; ++i) {
        void* e = TestNew(0x4c, "E%d", i);
        a.vsymCtor(e, nullptr, P(0, 2), P(0, 2), P(0, 1) != 0);
        if (P(0, 3) == 0) a.setName(e, kTexts[P(0, kNames - 1)]);
        if (P(0, 3) == 0) static_cast<unsigned char*>(e)[0x20] = static_cast<unsigned char>(P(0, 1));
        *reinterpret_cast<int*>(static_cast<char*>(e) + 0x34) = P(0, 1);
        a.tabAdd(t, e);
    }
    const unsigned* tw = static_cast<const unsigned*>(t);
    Log("table count %u cap %u data %s\n", tw[0], tw[1], Name(reinterpret_cast<void*>(tw[2])).c_str());
    for (int k = 0; k < 20; ++k) {
        void* node = P(0, 7) ? nodes[P(0, kNames - 1)] : nullptr;
        Log("  find %s -> %s\n", NodeName(node).c_str(), Name(a.tabFind(t, &node)).c_str());
        void* probe = TestNew(0x4c, "Q%d", k);
        a.vsymCtor(probe, nullptr, P(0, 2), P(0, 2), false);
        *reinterpret_cast<int*>(static_cast<char*>(probe) + 0x34) = P(0, 1);
        Log("  like -> %s\n", Name(a.tabFindLike(t, probe)).c_str());
        a.symDtor(probe, nullptr);
        RealDelete(probe);
        names[s].erase(probe);
    }
    for (int c = 0; c < 2; ++c) {  // a copy and an assignment: the pointers only, then the block is freed
        void* u = TestNew(0xc, "U%d", c);
        if (c == 0) {
            a.tabCopy(u, nullptr, t);
        } else {
            a.tabCtor(u, nullptr);
            void* e = TestNew(0x4c, "F%d", 0);
            a.vsymCtor(e, nullptr, 0, 0, false);
            a.tabAdd(u, e);
            a.tabAssign(u, nullptr, t);
            a.symDtor(e, nullptr);
            RealDelete(e);
            names[s].erase(e);
        }
        unsigned* uw = static_cast<unsigned*>(u);
        Log("  copy count %u cap %u same %d\n", uw[0], uw[1],
            uw[0] && std::memcmp(reinterpret_cast<void*>(uw[2]), reinterpret_cast<void*>(tw[2]), uw[0] * 4) == 0);
        uw[0] = 0;
        a.tabDtor(u, nullptr);
        RealDelete(u);
        names[s].erase(u);
    }
    a.tabDtor(t, nullptr);
    HookDelete(t);
}

// ---- part 3: registers and instructions

static void* NewSwizzle(std::mt19937& r)
{
    char* p = static_cast<char*>(TestNew(6, "W%d", static_cast<int>(r() % 1000)));
    const char* opts[] = {"x", "xy", "xyz", "xyzw", "wzyx"};
    std::strcpy(p, opts[r() % 5]);
    return p;
}

static void* NewRegister(const Api& a, std::mt19937& r, int depth, int& ids)
{
    void* g = TestNew(0x18, "R%d", ids++);
    void* m1 = r() % 3 == 0 ? TestNew(4, "M%d", ids++) : nullptr;
    void* m2 = r() % 3 == 0 ? TestNew(4, "M%d", ids++) : nullptr;
    a.regCtor(g, nullptr, reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000 + r() % 16)),
              r() % 2 ? static_cast<char*>(NewSwizzle(r)) : nullptr, r() % 2 != 0, m1, m2);
    if (depth < 2 && r() % 3 == 0) {
        void* o = TestNew(8, "O%d", ids++);
        a.offCtor(o, nullptr, NewRegister(a, r, depth + 1, ids), static_cast<int>(r() % 8));
        static_cast<void**>(g)[1] = o;
    }
    return g;
}

static void DumpReg(const char* tag, const void* g)
{
    const unsigned* w = static_cast<const unsigned*>(g);
    const char* sw = reinterpret_cast<const char*>(w[2]);
    Log("  %s sym %x off %s sw %s flag %d m %s %s\n", tag, w[0], Name(reinterpret_cast<void*>(w[1])).c_str(),
        sw ? sw : "-", static_cast<const unsigned char*>(g)[0xc], Name(reinterpret_cast<void*>(w[4])).c_str(),
        Name(reinterpret_cast<void*>(w[5])).c_str());
    if (w[1]) {
        const unsigned* o = reinterpret_cast<const unsigned*>(w[1]);
        Log("    offset item %s %d\n", Name(reinterpret_cast<void*>(o[0])).c_str(), static_cast<int>(o[1]));
    }
}

static void RunInstructions(int s, unsigned seed)
{
    const Api& a = api[s];
    std::mt19937 r(seed);
    int ids = 0;
    for (int k = 0; k < 6; ++k) {
        void* g = NewRegister(a, r, 0, ids);
        DumpReg("reg", g);
        void* c = TestNew(0x18, "K%d", k);
        a.regCopy(c, nullptr, g);
        DumpReg("copy", c);
        void* d = TestNew(0x18, "J%d", k);
        a.regAssign(d, nullptr, g);
        DumpReg("assigned", d);
        RealDelete(d);
        names[s].erase(d);
        if (unsigned* w = static_cast<unsigned*>(c); w[1]) reinterpret_cast<unsigned*>(w[1])[0] = 0;  // shared item
        Log("destroy copy\n");
        a.regDtor(c, nullptr);
        HookDelete(c);
        Log("destroy reg\n");
        a.regDtor(g, nullptr);
        HookDelete(g);
    }
    for (int k = 0; k < 6; ++k) {
        // an operand array {count, capacity, data} of 0..3 registers
        unsigned* list = static_cast<unsigned*>(TestNew(0xc, "L%d", k));
        const unsigned n = r() % 4;
        list[0] = n;
        list[1] = 128;
        list[2] = reinterpret_cast<unsigned>(TestNew(128 * 4, "B%d", k));
        for (unsigned i = 0; i < n; ++i) reinterpret_cast<void**>(list[2])[i] = NewRegister(a, r, 0, ids);
        const int kind = static_cast<int>(r() % 3);
        void* ins = TestNew(0xc, "I%d", k);
        if (kind == 0) a.insCtor(ins, nullptr, list);
        if (kind == 1) a.vinsCtor(ins, nullptr, static_cast<int>(r() % 40), list);
        if (kind == 2) a.tinsCtor(ins, nullptr, static_cast<int>(r() % 40), list);
        const unsigned* w = static_cast<const unsigned*>(ins);
        Log("ins kind %d ops %s op %x\n", kind, Name(reinterpret_cast<void*>(w[1])).c_str(), kind ? w[2] : 0);
        void* c = TestNew(0xc, "H%d", k);
        if (kind == 0) a.insCopy(c, nullptr, ins);
        if (kind == 1) a.vinsCopy(c, nullptr, ins);
        if (kind == 2) a.tinsCopy(c, nullptr, ins);
        const unsigned* cw = static_cast<const unsigned*>(c);
        Log("  copy ops %s op %x vt same %d\n", Name(reinterpret_cast<void*>(cw[1])).c_str(), kind ? cw[2] : 0,
            cw[0] == w[0]);
        std::memset(c, 0xcd, 0xc);
        (kind == 1 ? a.vinsAssign : a.insAssign)(c, nullptr, ins);
        Log("  assigned ops %s op %x\n", Name(reinterpret_cast<void*>(cw[1])).c_str(), kind == 1 ? cw[2] : 0);
        RealDelete(c);
        names[s].erase(c);
        Log("delete ins\n");
        void** vt = *static_cast<void***>(ins);
        reinterpret_cast<void*(__fastcall*)(void*, void*, unsigned)>(vt[0])(ins, nullptr, 1);
    }
}

// ---- part 4: defines

static void* NewAnsi(const char* text)
{
    void* p = RealNew(4);
    AnsiFromText(p, nullptr, text);
    return p;
}
static void FreeAnsi(void* p)
{
    AnsiDtor(p, nullptr);
    RealDelete(p);
}

static void DumpArray(const char* tag, const void* arr)
{
    const unsigned* w = static_cast<const unsigned*>(arr);
    Log("  %s count %u cap %u:", tag, w[0], w[1]);
    for (unsigned i = 0; i < w[0]; ++i) {
        const void* d = reinterpret_cast<const char*>(w[2]) + i * 8;
        Log(" [%s=%s]", AnsiText(d), AnsiText(static_cast<const char*>(d) + 4));
    }
    Log("\n");
}

static void RunDefines(int s, unsigned seed)
{
    const Api& a = api[s];
    std::mt19937 r(seed);
    auto P = [&](int lo, int hi) { return lo + static_cast<int>(r() % static_cast<unsigned>(hi - lo + 1)); };
    const char* texts[] = {"", "A", "LIGHTS", "1", "SKIN", "4", "FOG_LINEAR"};
    auto NewDefine = [&]() {
        void* n = NewAnsi(texts[P(0, 6)]);
        void* v = NewAnsi(texts[P(0, 6)]);
        void* d = RealNew(8);
        a.defCtor(d, nullptr, n, v);
        FreeAnsi(n);
        FreeAnsi(v);
        return d;
    };
    auto FreeDefine = [&](void* d) {
        a.defDtor(d, nullptr);
        RealDelete(d);
    };
    void* arr[2];
    for (int i = 0; i < 2; ++i) {
        arr[i] = TestNew(0xc, "P%d", i);
        (i ? a.ppaCtor : a.arrCtor)(arr[i], nullptr);
    }
    const int ops = P(10, 60);
    for (int k = 0; k < ops; ++k) {
        const int op = P(0, 19), i = P(0, 1);
        void* x = arr[i];
        const unsigned cnt = static_cast<const unsigned*>(x)[0], cap = static_cast<const unsigned*>(x)[1];
        Log("op %d on P%d\n", op, i);
        switch (op) {
        case 0: {
            void* d = NewDefine();
            a.add1(x, nullptr, d);
            FreeDefine(d);
            break;
        }
        case 1: {
            const unsigned n = static_cast<unsigned>(P(0, 4));
            void* block = RealNew(8 * 4 + 8);
            for (unsigned j = 0; j < n; ++j) {
                void* d = NewDefine();
                std::memcpy(static_cast<char*>(block) + j * 8, d, 8);  // moved into the block
                RealDelete(d);
            }
            a.addN(x, nullptr, block, n);
            for (unsigned j = 0; j < n; ++j) a.defDtor(static_cast<char*>(block) + j * 8, nullptr);
            RealDelete(block);
            break;
        }
        case 2: Log("  addEmpty %d\n", a.addEmpty(x, nullptr, static_cast<unsigned>(P(0, 3)))); break;
        case 3: {
            void* p = a.alloc(x, nullptr, static_cast<unsigned>(P(0, 3)));
            const unsigned* w = static_cast<const unsigned*>(x);
            Log("  alloc at %d\n", static_cast<int>((static_cast<char*>(p) - reinterpret_cast<char*>(w[2])) / 8));
            break;
        }
        case 4:
            if (cnt) {
                void* d = RealNew(8);
                a.removeLast(x, nullptr, d);
                Log("  removed [%s=%s]\n", AnsiText(d), AnsiText(static_cast<char*>(d) + 4));
                FreeDefine(d);
            }
            break;
        case 5: {
            unsigned it[2] = {reinterpret_cast<unsigned>(x), static_cast<unsigned>(P(0, 5))};
            unsigned out[2] = {1, 1};
            a.removeAt(x, nullptr, out, it);
            Log("  removeAt %s %u\n", out[0] == reinterpret_cast<unsigned>(x) ? "self" : out[0] ? "?" : "null", out[1]);
            break;
        }
        case 6:
            if (cnt) a.removeFast(x, nullptr, static_cast<unsigned>(P(0, static_cast<int>(cnt) - 1)));
            break;
        case 7: a.removeAll(x, nullptr); break;
        case 8: a.reserve(x, nullptr, static_cast<unsigned>(P(0, 12))); break;
        case 9: a.grow(x, nullptr, static_cast<unsigned>(P(0, 6))); break;
        case 10: {  // (an empty array with room left would destroy index 0xffffffff on both sides)
            const unsigned n = static_cast<unsigned>(P(0, 9));
            if (cnt || n >= cap) a.shrink(x, nullptr, n);
            break;
        }
        case 11: a.setCount(x, nullptr, static_cast<unsigned>(P(0, 8))); break;
        case 12:
            Log("  count %u size %u bool %d empty %d\n", a.count(x), a.size(x), a.asBool(x), a.isEmpty(x));
            break;
        case 13:
            if (cnt) {
                const unsigned j = static_cast<unsigned>(P(0, static_cast<int>(cnt) - 1));
                void* d = a.at(x, nullptr, j);
                Log("  at %u [%s]\n", j, AnsiText(d));
                void* nd = NewDefine();
                a.defAssign(d, nullptr, nd);
                FreeDefine(nd);
            }
            break;
        case 14: {
            unsigned it[2][2] = {{1, 1}, {1, 1}};
            unsigned it2[2][2] = {{1, 1}, {1, 1}};
            a.first(x, nullptr, it[0]);
            a.last(x, nullptr, it[1]);
            a.end(x, nullptr, it2[0]);
            a.index(x, nullptr, it2[1], static_cast<unsigned>(P(0, 5)));
            for (auto* v : {it[0], it[1], it2[0], it2[1]})
                Log("  it %s %u\n", v[0] == reinterpret_cast<unsigned>(x) ? "self" : v[0] ? "?" : "null", v[1]);
            break;
        }
        case 15: {  // copy construction
            void* c = TestNew(0xc, "Z%d", k);
            (i ? a.ppaCopy : a.arrCopy)(c, nullptr, arr[1 - i]);
            DumpArray("copy", c);
            (i ? a.ppaDtor : a.arrDtor)(c, nullptr);
            RealDelete(c);
            names[s].erase(c);
            break;
        }
        case 16: (i ? a.ppaAssign : a.arrAssign)(x, nullptr, arr[1 - i]); break;
        case 17: {
            void* out = RealNew(4);
            a.concat(arr[1], out);
            Log("  concat \"%s\"\n", AnsiText(out));
            FreeAnsi(out);
            break;
        }
        case 18: {
            void* d = NewDefine();
            void* c = RealNew(8);
            a.defCopy(c, nullptr, d);
            Log("  defcopy [%s=%s]\n", AnsiText(c), AnsiText(static_cast<char*>(c) + 4));
            FreeDefine(c);
            FreeDefine(d);
            break;
        }
        default: break;
        }
        DumpArray("P", x);
    }
    for (int i = 0; i < 2; ++i) {
        (i ? a.ppaDtor : a.arrDtor)(arr[i], nullptr);
        HookDelete(arr[i]);
    }
}

// ---------------------------------------------------------------------------------------------------------------

static void Compare(const char* part, int round)
{
    ++checks;
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

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_rs_u2 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 500;
    SetDllDirectoryA(argv[1]);
    std::string dir = argv[1];
    LoadLibraryA((dir + "\\RenderSystem_7c_orig.dll").c_str());
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
    CreateNode = reinterpret_cast<void* (*)(void*, const unsigned char*)>(Proc(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    cache = static_cast<void**>(Proc(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    AnsiFromText = As<decltype(AnsiFromText)>(Proc(ks, "??0AnsiString@Jet@@QAE@PBD@Z"));
    AnsiDtor = As<decltype(AnsiDtor)>(Proc(ks, "??1AnsiString@Jet@@QAE@XZ"));
    AnsiText = As<decltype(AnsiText)>(Proc(ks, "??BAnsiString@Jet@@QBAPBDXZ"));
    // the names SetName interns are the same nodes as these (one cache): hold a reference to each
    for (int i = 0; i < kNames; ++i) nodes[i] = CreateNode(*cache, reinterpret_cast<const unsigned char*>(kTexts[i]));
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
    }
    struct {
        const char* name;
        void (*run)(int, unsigned);
        int scale;
    } parts[] = {{"symbols", RunSymbols, 4}, {"table", RunTable, 1}, {"instructions", RunInstructions, 1},
                 {"defines", RunDefines, 4}};
    for (auto& p : parts) {
        for (int round = 0; round < rounds * p.scale / 4 + 1; ++round) {
            const unsigned seed = rng();
            for (int s = 0; s < 2; ++s) {
                cur = s;
                logText[s].clear();
                names[s].clear();
                allocCount[s] = 0;
                apiIds = 0;
                p.run(s, seed);
                cur = -1;
            }
            Compare(p.name, round);
        }
        std::printf("-- %s done\n", p.name);
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
