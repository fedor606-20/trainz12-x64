// Differential test of RenderSystem_7c unit U3a: VertexProgram (VertexProgram.cpp) and TextureCombiner
// (TextureCombiner.cpp), ours against RenderSystem_7c_orig.dll in one process.
//
// As in diff_rs_u2b: both modules' imports of Jet::Alloc::New / Delete are hooked (allocations numbered, frees
// logged by name), each scenario runs the same random script on both sides with that side's exports, and the
// logs must be equal. Objects are built with each side's constructor and get a copy of its vftable whose
// API-module slots (the ones answering 0 in the base) log what they are handed. A stand-in Display answers
// capability queries, hands out a program for the register limits and holds a render state block; stand-in lights
// answer their position and direction. Floats are logged as bits. The support flags and register limits are the
// original's statics on both sides: each run starts them from zero.
//
// usage: diff_rs_u3.exe <bin dir with RenderSystem_7c.dll (+ .map), RenderSystem_7c_orig.dll and the Jet modules>
//        [rounds]
#include <windows.h>

#include <cctype>
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
static std::mt19937 rng(9301);
static std::string binDir;

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
static void(__fastcall* AddRef)(const void*);

static int cur = -1;  // the side running
static std::string logText[2];
static std::map<const void*, std::string> names[2];
static int allocCount[2];

static void Log(const char* fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (cur >= 0) logText[cur] += buf;
}

static std::string Name(const void* p)
{
    if (!p) return "null";
    if (cur < 0) return "?";
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

// ---- strings

struct Node {
    void* next;
    void* prev;
    unsigned length, capacity;
    int refs;
    unsigned id;
    char text[1];
};
static const char* kTexts[] = {"vp", "prog.txt", "built-in", "x", "lightDir", "c0", "fog", "a"};
constexpr int kNames = sizeof kTexts / sizeof *kTexts;
static void* nodes[kNames];
static std::string NodeText(const void* n)
{
    if (!n) return "null";
    const Node* x = static_cast<const Node*>(n);
    char b[96];
    std::snprintf(b, sizeof b, "\"%.40s\"/%u", x->text, x->length);
    return b;
}

static std::string Bits(const float* f, int n)
{
    std::string s;
    char b[16];
    for (int i = 0; i < n; ++i) {
        unsigned u;
        std::memcpy(&u, f + i, 4);
        std::snprintf(b, sizeof b, i ? " %08x" : "%08x", u);
        s += b;
    }
    return s;
}

// the scanners' module statics (0x100b7dc8..0x100b8127 in the original): every load starts both sides from zero
static void ResetLexStatics(int s)
{
    static void* ours = OurSym("?gfxLex@Jet@@3UGfxLexStatics@1@A");
    if (s == 0) {
        std::memset(ours, 0, 0x360);
        return;
    }
    // the original's range also holds U3's statics (support flags 0xb8065, 0xb80bc..; TextureCombinerManager's
    // manager 0xb8088; the register limits ..0xb80cf): kept
    unsigned char keep1[3], keep2[4], keep3[0x14];
    std::memcpy(keep1, OrigAt(0xb8065), 3);
    std::memcpy(keep2, OrigAt(0xb8088), 4);
    std::memcpy(keep3, OrigAt(0xb80bc), 0x14);
    std::memset(OrigAt(0xb7dc8), 0, 0x360);
    std::memcpy(OrigAt(0xb8065), keep1, 3);
    std::memcpy(OrigAt(0xb8088), keep2, 4);
    std::memcpy(OrigAt(0xb80bc), keep3, 0x14);
}

// the original's statics both sides use: VertexProgram supported[4] 0xb80bc, supportChecked[4] 0xb80c0, register
// limits 0xb80c4..0xb80cc; TextureCombiner supported 0xb8065, supportChecked 0xb8066
static void ResetStatics()
{
    std::memset(OrigAt(0xb80bc), 0, 0x14);
    std::memset(OrigAt(0xb8065), 0, 2);
}

// ---- the corpus: every embedded program text of the render modules

static std::vector<std::string> vpTexts, tpTexts;
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
                if (s.find("version") != std::string::npos && s.find("rogram") != std::string::npos) vpTexts.push_back(s);
                if (s.find("API") != std::string::npos &&
                    (s.find("ps.") != std::string::npos || s.find("header") != std::string::npos))
                    tpTexts.push_back(s);
            }
            i = j + 1;
        }
    }
    std::printf("-- corpus: %zu vertex program lists, %zu texture programs\n", vpTexts.size(), tpTexts.size());
}
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

// ---- per-side entry points

#define F(name) Proc(mod[s], name)
#define VP "VertexProgram@Jet@@"
#define TC "TextureCombiner@Jet@@"
#define LIGHTS "ABV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@"

struct Col {
    float r, g, b, a;
};

struct Api {
    void** eglVft;
    bool(__cdecl* loadProgram)(void*, const void*, const char*, const void*);
    // VertexProgram
    void*(__fastcall* vpCtor)(void*, void*, void*, void*, int);
    void(__fastcall* vpVbaseDtor)(void*);
    void** vpVft;
    const int* vpVbt;
    bool(__cdecl* vpSupportedS)(void*, float);
    int(__cdecl* maxLocal)(void*);
    int(__cdecl* maxEnv)(void*);
    int(__cdecl* maxTotal)(void*);
    bool(__cdecl* setLocal3)(const void*, int, const float*);
    bool(__cdecl* setLocalC)(const void*, int, const float*);
    bool(__cdecl* setDirect3)(const void*, int, const float*);
    bool(__cdecl* setDirectC)(const void*, int, const float*);
    bool(__cdecl* setEnv3)(const void*, int, const float*);
    bool(__cdecl* setEnvC)(const void*, int, const float*);
    int(__cdecl* infBase)(const void*);
    int(__cdecl* infMax)(const void*);
    void(__cdecl* initInstance)(const void*, void*);
    void(__cdecl* loadCommon)(const void*, const void*, const void*, Col, const void*, const void*);
    bool(__cdecl* setVertexType)(void*, void*);
    void*(__cdecl* getVertexType)(const void*);
    void(__cdecl* setManager)(void*, void*);
    void*(__cdecl* getManager)(void*);
    bool(__cdecl* restore)(void*);
    void(__cdecl* loadConstRegs)(const void*);
    void(__cdecl* setProgramState)(const void*, const void*, Col, const void*, const void*);
    // TextureCombiner
    void*(__fastcall* tcCtor)(void*, void*, void*, int);
    void(__fastcall* tcVbaseDtor)(void*);
    void** tcVft;
    bool(__cdecl* tcSupportedS)(void*, float);
    bool(__cdecl* track)(void*, int, const void*, const float*);
    bool(__cdecl* setReg4)(void*, int, const float*);
    bool(__cdecl* setRegC)(void*, int, const float*);
    bool(__cdecl* setReg3)(void*, int, const float*);
    void(__cdecl* bumpScale)(void*, int, float, bool);
    void(__cdecl* bumpBias)(void*, int, float, bool);
    void(__cdecl* bumpMatrix)(void*, int, const float*, bool);
    void(__cdecl* setCombMgr)(void*, void*);
    void*(__cdecl* getCombMgr)(void*);
} api[2];

static void LoadApi(int s)
{
    Api& a = api[s];
    a.eglVft = static_cast<void**>(F("??_7ExtendedGfxLanguage@Jet@@6B01@@"));
    a.loadProgram = As<decltype(a.loadProgram)>(
        F("?LoadProgram@ExtendedGfxLanguage@Jet@@UAA_NABVPString@2@PBDPBUPreProcessorArray@2@@Z"));
    a.vpCtor = As<decltype(a.vpCtor)>(F("??0" VP "IAE@PAVDisplay@1@PAVVertexType@1@@Z"));
    a.vpVbaseDtor = As<decltype(a.vpVbaseDtor)>(F("??_D" VP "IAEXXZ"));
    a.vpVft = static_cast<void**>(F("??_7" VP "6BExtendedGfxLanguage@1@@"));
    a.vpVbt = static_cast<const int*>(F("??_8" VP "7B@"));
    a.vpSupportedS = As<decltype(a.vpSupportedS)>(F("?SupportedByPlatform@" VP "SA_NPAVDisplay@2@M@Z"));
    a.maxLocal = As<decltype(a.maxLocal)>(F("?GetMaxLocalRegisters@" VP "SAHPAVDisplay@2@@Z"));
    a.maxEnv = As<decltype(a.maxEnv)>(F("?GetMaxEnvRegisters@" VP "SAHPAVDisplay@2@@Z"));
    a.maxTotal = As<decltype(a.maxTotal)>(F("?GetMaxTotalRegisters@" VP "SAHPAVDisplay@2@@Z"));
    a.setLocal3 = As<decltype(a.setLocal3)>(F("?SetLocalConstant@" VP "QBA_NHABVVector3@2@@Z"));
    a.setLocalC = As<decltype(a.setLocalC)>(F("?SetLocalConstant@" VP "QBA_NHABVColor@2@@Z"));
    a.setDirect3 = As<decltype(a.setDirect3)>(F("?SetLocalConstantDirect@" VP "QBA_NHABVVector3@2@@Z"));
    a.setDirectC = As<decltype(a.setDirectC)>(F("?SetLocalConstantDirect@" VP "QBA_NHABVColor@2@@Z"));
    a.setEnv3 = As<decltype(a.setEnv3)>(F("?SetEnvConstant@" VP "QBA_NHABVVector3@2@@Z"));
    a.setEnvC = As<decltype(a.setEnvC)>(F("?SetEnvConstant@" VP "QBA_NHABVColor@2@@Z"));
    a.infBase = As<decltype(a.infBase)>(F("?GetInfluenceBaseRegister@" VP "QBAHXZ"));
    a.infMax = As<decltype(a.infMax)>(F("?GetInfluenceMaxRegister@" VP "QBAHXZ"));
    a.initInstance = As<decltype(a.initInstance)>(F("?InitInstance@" VP "IBAXAAV12@@Z"));
    a.loadCommon = As<decltype(a.loadCommon)>(F("?LoadCommonStates@" VP
        "IBAXABV?$Array@PAUConstantData@VertexProgram@Jet@@$02@2@" LIGHTS "VColor@2@ABUDisplayMaterial@2@ABVMatrix4@2@@Z"));
    a.setVertexType = As<decltype(a.setVertexType)>(F("?SetVertexType@" VP "UAA_NPAVVertexType@2@@Z"));
    a.getVertexType = As<decltype(a.getVertexType)>(F("?GetVertexType@" VP "UBAPAVVertexType@2@XZ"));
    a.setManager = As<decltype(a.setManager)>(F("?SetProgramManager@" VP "UAAXPAVVertexProgramManager@2@@Z"));
    a.getManager = As<decltype(a.getManager)>(F("?GetProgramManager@" VP "UAAPAVVertexProgramManager@2@XZ"));
    a.restore = As<decltype(a.restore)>(F("?RestoreProgram@" VP "UAA_NXZ"));
    a.loadConstRegs = As<decltype(a.loadConstRegs)>(F("?LoadConstantRegisters@" VP "UBAXXZ"));
    a.setProgramState = As<decltype(a.setProgramState)>(F("?SetProgramState@" VP "UBAX" LIGHTS
        "VColor@2@ABUDisplayMaterial@2@ABVMatrix4@2@@Z"));
    a.tcCtor = As<decltype(a.tcCtor)>(F("??0" TC "IAE@PAVDisplay@1@@Z"));
    a.tcVbaseDtor = As<decltype(a.tcVbaseDtor)>(F("??_D" TC "IAEXXZ"));
    a.tcVft = static_cast<void**>(F("??_7" TC "6BExtendedGfxLanguage@1@@"));
    a.tcSupportedS = As<decltype(a.tcSupportedS)>(F("?SupportedByPlatform@" TC "SA_NPAVDisplay@2@M@Z"));
    a.track = As<decltype(a.track)>(F("?TrackConstant@" TC "UAA_NHABVAnsiString@2@ABVVector4@2@@Z"));
    a.setReg4 = As<decltype(a.setReg4)>(F("?SetConstantRegister@" TC "UAA_NHABVVector4@2@@Z"));
    a.setRegC = As<decltype(a.setRegC)>(F("?SetConstantRegister@" TC "QAA_NHABVColor@2@@Z"));
    a.setReg3 = As<decltype(a.setReg3)>(F("?SetConstantRegister@" TC "QAA_NHABVVector3@2@@Z"));
    a.bumpScale = As<decltype(a.bumpScale)>(F("?SetBumpEnvScale@" TC "UAAXHM_N@Z"));
    a.bumpBias = As<decltype(a.bumpBias)>(F("?SetBumpEnvBias@" TC "UAAXHM_N@Z"));
    a.bumpMatrix = As<decltype(a.bumpMatrix)>(F("?SetBumpEnvMatrix@" TC "UAAXHABVVector4@2@_N@Z"));
    a.setCombMgr = As<decltype(a.setCombMgr)>(F("?SetCombinerManager@" TC "UAAXPAVTextureCombinerManager@2@@Z"));
    a.getCombMgr = As<decltype(a.getCombMgr)>(F("?GetCombinerManager@" TC "UAAPAVTextureCombinerManager@2@XZ"));
}

// ---------------------------------------------------------------------------------------------------------------
// stand-ins

static unsigned scenarioSeed;
static int capsCalls;

// the Display: slot 7 a capability, slot 0x51 (+0x144) a program for the register limits; +0x54 the render state
static void* displayVt[0x60];
static void* regsProgram[2];
static unsigned char stateBlock[0x100];
static unsigned char displayObj[0x200];
static bool __cdecl DisplayCaps(const void* d, int cap)
{
    const bool r = ((static_cast<unsigned>(cap) * 2654435761u) ^ scenarioSeed) % 3 != 0;
    Log("  display %s caps %d -> %d\n", Name(d).c_str(), cap, r);
    ++capsCalls;
    return r;
}
static void* __cdecl DisplayMakeProgram(const void* d, void* type)
{
    Log("  display %s makes a program (type %s)\n", Name(d).c_str(), Name(type).c_str());
    return regsProgram[cur];
}

// a VertexType: {vftable, references}; only its reference count is used
static void* typeVt[4];
struct FakeType {
    void** vt = typeVt;
    int refs = 1000;
};
static FakeType types[3];

// a light: {vftable, type, ambient +8, diffuse +0x18, specular +0x28, attenuation +0x38, cut-off +0x48, position,
// direction}; slot 3 the position, slot 5 the direction
struct FakeLight {
    void** vt;
    int type;
    float f[18];  // +8 .. +0x50
    float pos[3], dir[3];
};
static int posCalls, dirCalls;
static const float* __cdecl LightPos(const FakeLight* l)
{
    ++posCalls;
    Log("  light %s position\n", Name(l).c_str());
    return l->pos;
}
static const float* __cdecl LightDir(const FakeLight* l)
{
    ++dirCalls;
    Log("  light %s direction\n", Name(l).c_str());
    return l->dir;
}
static void* lightVt[8];

// logging API slots of the programs and combiners
static int __cdecl LogSlotInt(const void* self) { Log("  slot of %s\n", Name(self).c_str()); return 0; }
static bool __cdecl VpConst(const char* what, const void* self, int reg, const float* v)
{
    Log("  %s %s r%d %s\n", what, Name(self).c_str(), reg, Bits(v, 4).c_str());
    return (reg & 1) != 0;
}
static bool __cdecl VpLocal(const void* self, int reg, const float* v) { return VpConst("local", self, reg, v); }
static bool __cdecl VpDirect(const void* self, int reg, const float* v) { return VpConst("direct", self, reg, v); }
static bool __cdecl VpEnv(const void* self, int reg, const float* v) { return VpConst("env", self, reg, v); }
static bool __cdecl VpLocals(const void* self, int reg, const float* v, int n)
{
    Log("  locals %s r%d n%d %s\n", Name(self).c_str(), reg, n, Bits(v, 4 * n).c_str());
    return true;
}
static int apiValue[2];
static int __cdecl ApiOf(const void* self) { Log("  api of %s\n", Name(self).c_str()); return apiValue[cur]; }
static bool __cdecl NativeProgram(void* self, const void* text)
{
    Log("  native %s [%.200s]\n", Name(self).c_str(), AnsiText(text));
    return std::strlen(AnsiText(text)) % 2 == 0;
}
static int regsLimits[3];
static int __cdecl RegsLocal(const void*) { return regsLimits[0]; }
static int __cdecl RegsEnv(const void*) { return regsLimits[1]; }
static int __cdecl RegsTotal(const void*) { return regsLimits[2]; }
static void __cdecl VpNames(void* self, const void* names) { Log("  names %s %u\n", Name(self).c_str(), *static_cast<const unsigned*>(names)); }
static bool __cdecl VpSymbols(void* self, const void* table, bool native)
{
    Log("  symbols %s %u native %d\n", Name(self).c_str(), *static_cast<const unsigned*>(table), native);
    return true;
}
static bool __cdecl VpInstructions(void* self, const void* list)
{
    Log("  instructions %s %u\n", Name(self).c_str(), *static_cast<const unsigned*>(list));
    return true;
}
static bool __cdecl Compile(void* self) { Log("  compile %s\n", Name(self).c_str()); return true; }
static bool __cdecl VpSetType(void* self, void* type) { Log("  set type %s\n", Name(self).c_str()); return true; }
// (a type is always there: without one the parser copies a registered VertexType, and the test registers none)
static void* __cdecl VpGetType(const void* self) { Log("  get type %s\n", Name(self).c_str()); return &types[0]; }
static bool __cdecl TcAddInstruction(void* self, const void* ins)
{
    Log("  instruction %s op %d\n", Name(self).c_str(), static_cast<const int*>(ins)[1]);
    return true;
}
static bool __cdecl TcNative(void* self, const void* name, const char* text)
{
    Log("  native %s %s [%.200s]\n", Name(self).c_str(), NodeText(*static_cast<void* const*>(name)).c_str(), text);
    return true;
}

static void* vpTestVt[2][35];
static void* tcTestVt[2][18];
static void* regsVt[2][35];
static void MakeVtables(int s)
{
    std::memcpy(vpTestVt[s], api[s].vpVft, sizeof vpTestVt[s]);
    void** v = vpTestVt[s];
    v[4] = reinterpret_cast<void*>(&Compile);
    for (int i : {9, 12, 15, 20, 21, 22, 25}) v[i] = reinterpret_cast<void*>(&LogSlotInt);
    v[12] = reinterpret_cast<void*>(&VpNames);
    v[13] = reinterpret_cast<void*>(&VpSymbols);
    v[14] = reinterpret_cast<void*>(&VpInstructions);
    v[23] = reinterpret_cast<void*>(&VpLocal);
    v[24] = reinterpret_cast<void*>(&VpDirect);
    v[26] = reinterpret_cast<void*>(&VpLocals);
    v[27] = reinterpret_cast<void*>(&VpEnv);
    v[28] = reinterpret_cast<void*>(&VpSetType);
    v[29] = reinterpret_cast<void*>(&VpGetType);
    v[30] = reinterpret_cast<void*>(&RegsLocal);
    v[31] = reinterpret_cast<void*>(&RegsEnv);
    v[32] = reinterpret_cast<void*>(&RegsTotal);
    v[33] = reinterpret_cast<void*>(&ApiOf);
    v[34] = reinterpret_cast<void*>(&NativeProgram);
    std::memcpy(regsVt[s], v, sizeof regsVt[s]);
    std::memcpy(tcTestVt[s], api[s].tcVft, sizeof tcTestVt[s]);
    void** t = tcTestVt[s];
    t[3] = reinterpret_cast<void*>(&TcNative);
    t[4] = reinterpret_cast<void*>(&Compile);
    t[11] = reinterpret_cast<void*>(&TcAddInstruction);
    t[17] = reinterpret_cast<void*>(&ApiOf);
}

// ---------------------------------------------------------------------------------------------------------------

static float RandF(std::mt19937& r)
{
    switch (r() % 8) {
    case 0: return 0.0f;
    case 1: return 1.0f;
    case 2: return -0.0f;
    default: return static_cast<float>(static_cast<int>(r() % 4001) - 2000) / 37.0f;
    }
}

static void DumpVp(const char* tag, const void* p)
{
    const auto* b = static_cast<const unsigned char*>(p);
    auto u32 = [&](int off) { return *reinterpret_cast<const unsigned*>(b + off); };
    auto ptr = [&](int off) { return *reinterpret_cast<void* const*>(b + off); };
    Log("  %s %s: vft %s vbt %s disp %s src [%.120s] file %s prog %s reg %s gen %08x syms %u fb %s | type %s base %d max "
        "%d b40 %u mgr %s rc %u\n",
        tag, Name(p).c_str(), ptr(0) == vpTestVt[cur] ? "test" : ptr(0) == api[cur].vpVft ? "own" : "?",
        ptr(4) == api[cur].vpVbt ? "own" : "?", Name(ptr(8)).c_str(), AnsiText(b + 0xc), NodeText(ptr(0x10)).c_str(),
        NodeText(ptr(0x14)).c_str(), NodeText(ptr(0x18)).c_str(), u32(0x1c), u32(0x20), Name(ptr(0x30)).c_str(),
        Name(ptr(0x34)).c_str(), static_cast<int>(u32(0x38)), static_cast<int>(u32(0x3c)), b[0x40],
        Name(ptr(0x44)).c_str(), u32(0x4c));
}

static void DumpTc(const char* tag, const void* p)
{
    const auto* b = static_cast<const unsigned char*>(p);
    auto u32 = [&](int off) { return *reinterpret_cast<const unsigned*>(b + off); };
    auto ptr = [&](int off) { return *reinterpret_cast<void* const*>(b + off); };
    Log("  %s %s: vft %s disp %s src [%.120s] file %s prog %s gen %08x syms %u | mgr %s changed %u rc %u\n", tag,
        Name(p).c_str(), ptr(0) == tcTestVt[cur] ? "test" : ptr(0) == api[cur].tcVft ? "own" : "?",
        Name(ptr(8)).c_str(), AnsiText(b + 0xc), NodeText(ptr(0x10)).c_str(), NodeText(ptr(0x14)).c_str(), u32(0x1c),
        u32(0x20), Name(ptr(0x60)).c_str(), b[0x5c], u32(0x68));
    const char* labels[] = {"constants", "bump", "matrices"};
    const int offs[] = {0x34, 0x44, 0x50};
    for (int k = 0; k < 3; ++k) {
        const unsigned n = u32(offs[k]);
        void* const* data = static_cast<void* const*>(ptr(offs[k] + 8));
        Log("    %s %u/%u %s:", labels[k], n, u32(offs[k] + 4), Name(data).c_str());
        for (unsigned i = 0; i < n; ++i) {
            const auto* it = static_cast<const int*>(data[i]);
            Log(" %s{%d %s}", Name(it).c_str(), it[0], Bits(reinterpret_cast<const float*>(it + 1), 4).c_str());
        }
        Log("\n");
    }
    {
        const unsigned n = u32(0x20);
        const auto* data = static_cast<const unsigned char*>(ptr(0x28));
        Log("    symbols %u/%u:", n, u32(0x24));
        for (unsigned i = 0; i < n; ++i) {
            const auto* e = data + 16 * i;
            Log(" %s:%d,%d,%d", NodeText(*reinterpret_cast<void* const*>(e)).c_str(), e[4],
                *reinterpret_cast<const int*>(e + 8), *reinterpret_cast<const int*>(e + 12));
        }
        Log("\n");
    }
    void* const* table = static_cast<void* const*>(ptr(0x40));
    Log("    table %s:", Name(table).c_str());
    for (int i = 0; i < 16; ++i)
        if (table[i]) Log(" %d=%s", i, Name(table[i]).c_str());
    Log("\n");
}

static int faults, loadsOk, loadsTried;
static uintptr_t faultAt;

static int FaultFilter(EXCEPTION_POINTERS* e)
{
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
    faultAt = reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}

static bool GuardedLoad(int s, void* obj, const void* name, const char* text)
{
    __try {
        ++loadsTried;
        const bool ok = api[s].loadProgram(obj, name, text, nullptr);
        loadsOk += ok;
        return ok;
    } __except (FaultFilter(GetExceptionInformation())) {
        ++faults;
        const uintptr_t b = reinterpret_cast<uintptr_t>(mod[s]);
        if (faultAt - b < 0x200000) std::printf("load fault on side %d at +0x%x\n", s, static_cast<unsigned>(faultAt - b));
        Log("  FAULT\n");
        return false;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// vertex programs

static void RunPrograms(int s, unsigned seed)
{
    std::mt19937 r(seed);
    auto P = [&](int lo, int hi) { return lo + static_cast<int>(r() % static_cast<unsigned>(hi - lo + 1)); };
    Api& a = api[s];
    ResetStatics();
    scenarioSeed = seed;
    apiValue[s] = P(0, 2);
    regsLimits[0] = P(0, 3) ? P(1, 300) : 0;
    regsLimits[1] = P(1, 300);
    regsLimits[2] = P(0, 3) ? P(1, 300) : 0;
    names[s][displayObj] = "Display";
    names[s][stateBlock] = "State";
    for (int i = 0; i < 3; ++i) {
        char b[8];
        std::snprintf(b, sizeof b, "T%d", i);
        names[s][&types[i]] = b;
        types[i].refs = 1000;
    }
    for (int i = 0; i < 0x40; ++i) reinterpret_cast<float*>(stateBlock)[i] = RandF(r);
    // the program the display hands out for the register limits (kept alive by extra references)
    regsProgram[s] = TestNew(0x50, "Regs%d", 0);
    a.vpCtor(regsProgram[s], nullptr, displayObj, nullptr, 1);
    *static_cast<void**>(regsProgram[s]) = regsVt[s];
    for (int i = 0; i < 50; ++i) AddRef(static_cast<char*>(regsProgram[s]) + 0x48);

    void* prog[3];
    for (int i = 0; i < 3; ++i) {
        prog[i] = TestNew(0x50, "P%d", i);
        FakeType* t = i == 1 ? nullptr : &types[i];
        a.vpCtor(prog[i], nullptr, displayObj, t, 1);
        *static_cast<void**>(prog[i]) = vpTestVt[s];
        DumpVp("new", prog[i]);
    }
    // lights: a FastDynamicArray of 13 per chunk
    FakeLight lights[20];
    const int nLights = P(0, 20);
    for (int i = 0; i < nLights; ++i) {
        FakeLight& l = lights[i];
        l.vt = lightVt;
        l.type = P(0, 5) == 0 ? 3 : P(0, 4);
        for (float& f : l.f) f = RandF(r);
        if (P(0, 2) == 0) l.f[16] = static_cast<float>(P(0, 180));  // a cut-off in degrees
        for (float& f : l.pos) f = RandF(r);
        for (float& f : l.dir) f = RandF(r);
        char b[8];
        std::snprintf(b, sizeof b, "L%d", i);
        names[s][&l] = b;
    }
    struct Chunk {
        Chunk* next;
        Chunk* prev;
        unsigned count;
        void* data[13];
    } chunks[2];
    Chunk* head = nullptr;
    for (int c = 0; c * 13 < nLights; ++c) {
        chunks[c].count = 0;
        for (int i = c * 13; i < nLights && i < c * 13 + 13; ++i) chunks[c].data[chunks[c].count++] = &lights[i];
    }
    if (nLights) {
        head = &chunks[0];
        if (nLights > 13) {
            chunks[0].next = chunks[0].prev = &chunks[1];
            chunks[1].next = chunks[1].prev = &chunks[0];
        } else {
            chunks[0].next = chunks[0].prev = &chunks[0];
        }
    }
    void* lightList = head;

    for (int step = 0; step < 30; ++step) {
        void* p = prog[P(0, 2)];
        const int op = P(0, 19);
        Log("op %d on %s\n", op, Name(p).c_str());
        switch (op) {
        case 0: {
            const float g = static_cast<float>(P(0, 12)) / 4.0f;
            Log("  supported %d\n", a.vpSupportedS(displayObj, g));
            const float h = static_cast<float>(P(0, 12)) / 4.0f;
            Log("  supported %d\n", reinterpret_cast<bool(__cdecl*)(void*, float)>(a.vpVft[0])(p, h));
            break;
        }
        case 1:
            Log("  limits %d %d %d\n", a.maxLocal(displayObj), a.maxEnv(displayObj), a.maxTotal(displayObj));
            break;
        case 2:
        case 3: {
            float v[4];
            for (float& f : v) f = RandF(r);
            const int reg = P(-2, 40);
            const int k = P(0, 5);
            bool res;
            switch (k) {
            case 0: res = a.setLocal3(p, reg, v); break;
            case 1: res = a.setLocalC(p, reg, v); break;
            case 2: res = a.setDirect3(p, reg, v); break;
            case 3: res = a.setDirectC(p, reg, v); break;
            case 4: res = a.setEnv3(p, reg, v); break;
            default: res = a.setEnvC(p, reg, v); break;
            }
            Log("  set %d -> %d\n", k, res);
            break;
        }
        case 4: {  // influences
            *reinterpret_cast<int*>(static_cast<char*>(p) + 0x38) = P(-2, 20);
            const unsigned n = P(0, 5);
            float m[5][16];
            void* maps[5];
            const float* mm[5];
            for (unsigned i = 0; i < n; ++i) {
                for (float& f : m[i]) f = RandF(r);
                mm[i] = m[i];
                maps[i] = &mm[i];
            }
            unsigned arr[3] = {n, 128, reinterpret_cast<unsigned>(maps)};
            reinterpret_cast<void(__cdecl*)(const void*, const void*)>(a.vpVft[18])(p, arr);
            Log("  base %d max %d\n", a.infBase(p), a.infMax(p));
            break;
        }
        case 5:
        case 6:
        case 7:
        case 8: {  // the render-state constants
            const unsigned n = P(0, 12);
            int cd[12][8];
            void* list[12];
            for (unsigned i = 0; i < n; ++i) {
                for (int& x : cd[i]) x = static_cast<int>(r());
                cd[i][0] = P(0, 90);
                cd[i][1] = P(0, 6) ? 4 : P(0, 5);
                cd[i][2] = P(0, 1) ? P(0x17, 0x1d) : P(0, 8) ? P(0x12, 0x27) : P(0, 0x40);  // lights half the time
                cd[i][7] = P(0, 7) ? P(0, 4) : P(5, 7);  // type
                cd[i][4] = P(0, 5) ? P(0, 2) : P(3, 30);  // index
                list[i] = cd[i];
            }
            unsigned arr[3] = {n, 3, reinterpret_cast<unsigned>(list)};
            float mat[17], view[16];
            Col c;
            for (float& f : mat) f = RandF(r);
            for (float& f : view) f = RandF(r);
            if (P(0, 1)) view[3] = view[7] = view[11] = 0.0f, view[15] = 1.0f;
            c.r = RandF(r);
            c.g = RandF(r);
            c.b = RandF(r);
            c.a = RandF(r);
            a.loadCommon(p, arr, &lightList, c, mat, view);
            break;
        }
        case 9: {
            FakeType* t = &types[P(0, 2)];
            Log("  set type %s -> %d, get %s refs %d %d %d\n", Name(t).c_str(), a.setVertexType(p, t),
                Name(a.getVertexType(p)).c_str(), types[0].refs, types[1].refs, types[2].refs);
            break;
        }
        case 10: {
            void* m = P(0, 1) ? displayObj : nullptr;
            a.setManager(p, m);
            Log("  manager %s restore %d\n", Name(a.getManager(p)).c_str(), a.restore(p));
            a.loadConstRegs(p);
            Col c{};
            a.setProgramState(p, &lightList, c, nullptr, nullptr);
            break;
        }
        case 11: {  // InitInstance into a fresh program
            void* q = TestNew(0x50, "Q%d", step);
            a.vpCtor(q, nullptr, nullptr, nullptr, 1);
            a.initInstance(p, q);
            DumpVp("copy", q);
            a.vpVbaseDtor(q);
            TestDelete(q);
            Log("  refs %d %d %d\n", types[0].refs, types[1].refs, types[2].refs);
            break;
        }
        case 12: {
            const char* texts[] = {"!!VP1.0 MOV o[HPOS], v[OPOS]; END", "", "x", "native text\n"};
            Log("  native -> %d\n", reinterpret_cast<bool(__cdecl*)(void*, const void*, const char*)>(a.vpVft[3])(
                                          p, &nodes[P(0, kNames - 1)], texts[P(0, 3)]));
            break;
        }
        default: {  // a real program list through LoadProgram (pre-parser, then the vertex program parser)
            if (vpTexts.empty()) break;
            const std::string& text = vpTexts[r() % vpTexts.size()];
            std::vector<std::string> programs = ProgramNames(text);
            std::string program = programs.empty() || P(0, 9) == 0 ? "none" : programs[r() % programs.size()];
            void* node = CreateNode(*cache, reinterpret_cast<const unsigned char*>(program.c_str()));
            ResetLexStatics(s);
            Log("  load %s -> %d\n", program.c_str(), GuardedLoad(s, p, &node, text.c_str()));
            DestroyNode(*cache, node);
            break;
        }
        }
        DumpVp("now", p);
    }
    for (int i = 0; i < 3; ++i) {
        a.vpVbaseDtor(prog[i]);
        TestDelete(prog[i]);
    }
    Log("refs %d %d %d\n", types[0].refs, types[1].refs, types[2].refs);
    a.vpVbaseDtor(regsProgram[s]);
    TestDelete(regsProgram[s]);
}

// ---------------------------------------------------------------------------------------------------------------
// texture combiners

static void RunCombiners(int s, unsigned seed)
{
    std::mt19937 r(seed);
    auto P = [&](int lo, int hi) { return lo + static_cast<int>(r() % static_cast<unsigned>(hi - lo + 1)); };
    Api& a = api[s];
    ResetStatics();
    scenarioSeed = seed;
    apiValue[s] = P(0, 2);
    names[s][displayObj] = "Display";
    void* tc[2];
    for (int i = 0; i < 2; ++i) {
        tc[i] = TestNew(0x6c, "C%d", i);
        a.tcCtor(tc[i], nullptr, displayObj, 1);
        *static_cast<void**>(tc[i]) = tcTestVt[s];
        DumpTc("new", tc[i]);
    }
    const char* kConstNames[] = {"", "c0", "fogColor", "x"};
    for (int step = 0; step < 30; ++step) {
        void* c = tc[P(0, 1)];
        const int op = P(0, 9);
        Log("op %d on %s\n", op, Name(c).c_str());
        float v[4];
        for (float& f : v) f = RandF(r);
        switch (op) {
        case 0:
            Log("  supported %d %d\n", a.tcSupportedS(displayObj, RandF(r)),
                reinterpret_cast<bool(__cdecl*)(void*, float)>(a.tcVft[0])(c, RandF(r)));
            break;
        case 1:
        case 2: {
            void* name = RealNew(4);
            AnsiFromText(name, nullptr, kConstNames[P(0, 3)]);
            const int index = P(0, 9) ? P(0, 15) : P(16, 40);
            Log("  track %d -> %d\n", index, a.track(c, index, name, v));
            AnsiDtor(name, nullptr);
            RealDelete(name);
            break;
        }
        case 3: {
            const int index = P(0, 15);
            const int k = P(0, 2);
            const bool res = k == 0 ? a.setReg4(c, index, v) : k == 1 ? a.setRegC(c, index, v) : a.setReg3(c, index, v);
            Log("  set %d %d -> %d\n", k, index, res);
            break;
        }
        case 4: a.bumpScale(c, P(0, 4), v[0], P(0, 3) != 0); break;
        case 5: a.bumpBias(c, P(0, 4), v[1], P(0, 3) != 0); break;
        case 6: a.bumpMatrix(c, P(0, 4), v, P(0, 3) != 0); break;
        case 7: {
            void* m = P(0, 1) ? displayObj : nullptr;
            a.setCombMgr(c, m);
            Log("  manager %s\n", Name(a.getCombMgr(c)).c_str());
            break;
        }
        default: {  // a real texture program through LoadProgram
            if (tpTexts.empty()) break;
            const std::string& text = tpTexts[r() % tpTexts.size()];
            ResetLexStatics(s);
            Log("  load -> %d\n", GuardedLoad(s, c, &nodes[P(0, kNames - 1)], text.c_str()));
            break;
        }
        }
        DumpTc("now", c);
    }
    for (int i = 0; i < 2; ++i) {
        a.tcVbaseDtor(tc[i]);
        TestDelete(tc[i]);
    }
}

// ---------------------------------------------------------------------------------------------------------------

static void Compare(const char* part, int round)
{
    ++checks;
    if (logText[0] == logText[1]) return;
    if (const char* dump = std::getenv("RS_U3_DUMP"); dump && !bad)  // both logs of the first mismatch
        for (int s = 0; s < 2; ++s)
            if (FILE* f = std::fopen((std::string(dump) + (s ? ".orig.txt" : ".ours.txt")).c_str(), "w")) {
                std::fputs(logText[s].c_str(), f);
                std::fclose(f);
            }
    if (++bad > 6) return;
    const std::string &x = logText[0], &y = logText[1];
    size_t i = 0;
    while (i < x.size() && i < y.size() && x[i] == y[i]) ++i;
    size_t from = x.rfind("op ", i);
    if (from == std::string::npos || i - from > 3000) from = i > 600 ? i - 600 : 0;
    std::printf("MISMATCH %s round %d\n-- ours:\n%s\n-- original:\n%s\n", part, round,
                x.substr(from, i - from + 600).c_str(), y.substr(from, i - from + 600).c_str());
}

static LONG WINAPI Crashed(EXCEPTION_POINTERS* e)
{
    const auto at = reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress);
    for (int s = 0; s < 2; ++s) {
        const auto b = reinterpret_cast<uintptr_t>(mod[s]);
        if (at - b < 0x200000) std::printf("fault in %s +0x%x\n", s ? "the original" : "ours", static_cast<unsigned>(at - b));
    }
    std::printf("fault %08lx at %p, side %d; log end:\n", e->ExceptionRecord->ExceptionCode,
                e->ExceptionRecord->ExceptionAddress, cur);
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
        std::printf("usage: diff_rs_u3 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 200;
    SetDllDirectoryA(argv[1]);
    binDir = argv[1];
    mod[0] = LoadLibraryA((binDir + "\\RenderSystem_7c.dll").c_str());
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
    AddRef = As<decltype(AddRef)>(Proc(ks, "?AddReference@ReferenceCount@Jet@@QBEXXZ"));
    for (int i = 0; i < kNames; ++i) nodes[i] = CreateNode(*cache, reinterpret_cast<const unsigned char*>(kTexts[i]));
    // stand-in vftables
    for (void*& p : displayVt) p = reinterpret_cast<void*>(&LogSlotInt);
    displayVt[7] = reinterpret_cast<void*>(&DisplayCaps);
    displayVt[0x51] = reinterpret_cast<void*>(&DisplayMakeProgram);
    *reinterpret_cast<void**>(displayObj) = displayVt;
    *reinterpret_cast<void**>(displayObj + 0x54) = stateBlock;
    for (void*& p : typeVt) p = reinterpret_cast<void*>(&LogSlotInt);
    for (void*& p : lightVt) p = reinterpret_cast<void*>(&LogSlotInt);
    lightVt[3] = reinterpret_cast<void*>(&LightPos);
    lightVt[5] = reinterpret_cast<void*>(&LightDir);
    CollectCorpus();
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        MakeVtables(s);
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
    }
    struct {
        const char* name;
        void (*run)(int, unsigned);
    } parts[] = {{"programs", RunPrograms}, {"combiners", RunCombiners}};
    const char* only = std::getenv("RS_U3_ONLY");
    for (auto& p : parts) {
        if (only && !std::strstr((std::string(",") + only + ",").c_str(), (std::string(",") + p.name + ",").c_str()))
            continue;
        for (int round = 0; round < rounds; ++round) {
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
        std::printf("-- %s done (display capability calls %d, loads %d, ok %d, that fault %d)\n", p.name, capsCalls,
                    loadsTried, loadsOk, faults);
        std::printf("   light positions %d, directions %d\n", posCalls, dirCalls);
        capsCalls = faults = loadsTried = loadsOk = posCalls = dirCalls = 0;
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
