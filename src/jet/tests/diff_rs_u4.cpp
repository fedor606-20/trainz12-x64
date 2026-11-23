// Differential test of RenderSystem_7c unit U4 (Modifier, ModifierHost, ModifierList, ModifierUpdateTask): our
// module against RenderSystem_7c_orig.dll in one process.
//
// Each scenario builds, on each side, two lists (a ModifierList and a ModifierUpdateTask) and six modifiers with
// that side's exports, then runs the same random script of calls: the inserts by priority, removes, finds,
// updates, host changes (through lists and a stand-in kernel), copies and assignments. The modifiers' Update slot
// (8) and destructor and the lists' Remove(const Modifier*) slot are wrapped to log their calls; Update answers
// from a script (2 detach, 3 delete). After every call the side logs the result and a snapshot of every list
// (order, frame counter, flag) and modifier (host, name, priority, complete bit, frame stamp). The logs of the two
// sides must be equal. Objects are filled with the same pattern before construction, so fields the constructors
// leave alone compare too.
//
// usage: diff_rs_u4.exe <bin dir with RenderSystem_7c.dll, RenderSystem_7c_orig.dll and the Jet modules> [rounds]
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
static std::mt19937 rng(9400);
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

// ---- KernelStandard: heap and interned strings

using NewFn = void*(__cdecl*)(unsigned);
using DeleteFn = void(__cdecl*)(void*);
using CreateFn = void*(__cdecl*)(void* cache, const unsigned char* text);
using DestroyFn = void(__cdecl*)(void* cache, void* node);
static NewFn New;
static DeleteFn Delete;
static CreateFn Create;
static DestroyFn Destroy;
static void** cache;

// ---- per-side entry points

using Ctor0 = void*(__fastcall*)(void*, void*);
using CtorCopy = void*(__fastcall*)(void*, void*, const void*);
using CtorName = void*(__fastcall*)(void*, void*, const void* name, float priority);
using CtorPtrName = void*(__fastcall*)(void*, void*, void* host, const void* name, float priority);
using Assign = void*(__fastcall*)(void*, void*, const void*);
using CheckFn = int*(__fastcall*)(void*, void*, int* ret, const void* m);
using TouchFn = bool(__fastcall*)(void*, void*, const void* m, unsigned long frame);
using SetMHFn = void(__fastcall*)(void*, void*, void* host, const void* m);
using InitFn = void(__cdecl*)(const void* self, void* to);
using DtorFn = void*(__fastcall*)(void*, void*, unsigned flags);

struct Api {
    Ctor0 mDefault, lDefault, tDefault;
    CtorName mName;
    CtorPtrName mHost, mKernel;
    CtorCopy mCopy, lCopy;
    Assign mAssign, lAssign;
    CheckFn check;
    TouchFn touch;
    SetMHFn setModifierHost;
    InitFn init;
} api[2];

static void LoadApi(int s)
{
    HMODULE m = mod[s];
    Api& a = api[s];
    a.mDefault = static_cast<Ctor0>(Proc(m, "??0Modifier@Jet@@QAE@XZ"));
    a.lDefault = static_cast<Ctor0>(Proc(m, "??0ModifierList@Jet@@QAE@XZ"));
    a.tDefault = static_cast<Ctor0>(Proc(m, "??0ModifierUpdateTask@Jet@@QAE@XZ"));
    a.mName = static_cast<CtorName>(Proc(m, "??0Modifier@Jet@@QAE@ABVPString@1@M@Z"));
    a.mHost = static_cast<CtorPtrName>(Proc(m, "??0Modifier@Jet@@QAE@PAVModifierHost@1@ABVPString@1@M@Z"));
    a.mKernel = static_cast<CtorPtrName>(Proc(m, "??0Modifier@Jet@@QAE@PAVRenderKernel@1@ABVPString@1@M@Z"));
    a.mCopy = static_cast<CtorCopy>(Proc(m, "??0Modifier@Jet@@QAE@ABV01@@Z"));
    a.lCopy = static_cast<CtorCopy>(Proc(m, "??0ModifierList@Jet@@QAE@ABV01@@Z"));
    a.mAssign = static_cast<Assign>(Proc(m, "??4Modifier@Jet@@QAEAAV01@ABV01@@Z"));
    a.lAssign = static_cast<Assign>(Proc(m, "??4ModifierList@Jet@@QAEAAV01@ABV01@@Z"));
    a.check = static_cast<CheckFn>(Proc(m,
        "?CheckPresence@ModifierList@Jet@@QAE?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResultTable@2@BW4Results@"
        "ModifierHost@Jet@@@2@PBVModifier@2@@Z"));
    a.touch = static_cast<TouchFn>(Proc(m, "?TouchModifier@ModifierHost@Jet@@IBE_NPBVModifier@2@K@Z"));
    a.setModifierHost = static_cast<SetMHFn>(Proc(m, "?SetModifierHost@ModifierHost@Jet@@IBEXPAV12@PBVModifier@2@@Z"));
    a.init = static_cast<InitFn>(Proc(m, "?InitInstance@Modifier@Jet@@IBAXAAV12@@Z"));
}

// virtual slots, all __cdecl with `this` first (and the result pointer second)
template <class R, class... A>
static R V(void* self, int slot, A... args)
{
    void** vt = *static_cast<void***>(self);
    return reinterpret_cast<R(__cdecl*)(void*, A...)>(vt[slot])(self, args...);
}
static int VR(void* self, int slot) { int r = -1; return *V<int*>(self, slot, &r); }
template <class... A>
static int VR(void* self, int slot, A... args)
{
    int r = -1;
    return *V<int*>(self, slot, &r, args...);
}
static void DestroyJet(void* o)
{
    void** vt = *static_cast<void***>(o);
    reinterpret_cast<DtorFn>(vt[0])(o, nullptr, 1);
}

// ---- the scenario state of one side

constexpr int kMods = 6, kNames = 4;
const float kPriorities[] = {0.25f, 0.5f, 1.0f, 2.0f};

struct Side {
    int s;
    void* list[2];  // 0 a ModifierList, 1 a ModifierUpdateTask
    void* m[kMods];
    bool alive[kMods];
    unsigned updateCalls[kMods];
    void* modVt[1 + 9];
    void* listVt[2][1 + 12];
    void* origModDtor;
    void* origRemove[2];
    char kernel[0x84];
    char graph[0xc];
    std::string log;
};
static Side side[2];
static void* names[kNames];             // interned nodes (shared by both sides: one KernelStandard)
static std::vector<int> updateScript[kMods];
static std::map<void*, std::pair<int, int>> owner;  // modifier or list -> {side, index}

static void Log(Side& sd, const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    sd.log += buf;
}

static int ModIndex(Side& sd, const void* p)
{
    for (int i = 0; i < kMods; ++i)
        if (sd.m[i] == p && sd.alive[i]) return i;
    return p ? 90 : -1;
}
static int HostIndex(Side& sd, const void* p)
{
    for (int i = 0; i < 2; ++i)
        if (sd.list[i] == p) return i;
    return p ? 9 : -1;
}
static int NameIndex(const void* node)
{
    for (int i = 0; i < kNames; ++i)
        if (names[i] == node) return i;
    return 99;
}

struct RawNode {
    RawNode *next, *prev;
    void* value;
};
static RawNode* Head(void* list) { return *reinterpret_cast<RawNode**>(static_cast<char*>(list) + 8); }

static int Occurrences(Side& sd, const void* m)
{
    int n = 0;
    for (void* l : sd.list)
        for (RawNode* x = Head(l)->next; x != Head(l); x = x->next) n += x->value == m;
    return n;
}

static void Snapshot(Side& sd)
{
    for (int l = 0; l < 2; ++l) {
        const char* b = static_cast<const char*>(sd.list[l]);
        Log(sd, "  L%d size %u busy %d frame %lu:", l, *reinterpret_cast<const unsigned*>(b + 0xc), b[0x10],
            *reinterpret_cast<const unsigned long*>(b + 0x14));
        for (RawNode* x = Head(sd.list[l])->next; x != Head(sd.list[l]); x = x->next) Log(sd, " %d", ModIndex(sd, x->value));
        Log(sd, "\n");
    }
    for (int i = 0; i < kMods; ++i) {
        if (!sd.alive[i]) continue;
        const char* b = static_cast<const char*>(sd.m[i]);
        Log(sd, "  M%d host %d name %d pri %08x bits %02x frame %08x\n", i, HostIndex(sd, *reinterpret_cast<void* const*>(b + 4)),
            NameIndex(*reinterpret_cast<void* const*>(b + 8)), *reinterpret_cast<const unsigned*>(b + 0xc),
            static_cast<unsigned char>(b[0x10]), *reinterpret_cast<const unsigned*>(b + 0x14));
    }
}

// ---- wrapped slots

static int* __cdecl TestUpdate(void* self, int* ret, float dt)
{
    auto [s, i] = owner.at(self);
    Side& sd = side[s];
    const std::vector<int>& script = updateScript[i];
    int r = script[sd.updateCalls[i]++ % script.size()];
    const int n = Occurrences(sd, self);
    if (r == 3 && n != 1) r = 2;  // never delete a modifier another node still points at
    Log(sd, "  update M%d %g -> %d\n", i, dt, r);
    *ret = r;
    return ret;
}

static void* __fastcall TestModDtor(void* self, void*, unsigned flags)
{
    auto [s, i] = owner.at(self);
    Side& sd = side[s];
    Log(sd, "  delete M%d %u\n", i, flags);
    sd.alive[i] = false;
    owner.erase(self);
    return reinterpret_cast<DtorFn>(sd.origModDtor)(self, nullptr, flags);
}

static int* __cdecl TestListRemove(void* self, int* ret, const void* m)
{
    auto [s, l] = owner.at(self);
    Side& sd = side[s];
    int* r = reinterpret_cast<int*(__cdecl*)(void*, int*, const void*)>(sd.origRemove[l])(self, ret, m);
    Log(sd, "  L%d.Remove(M%d) -> %d\n", l, ModIndex(sd, m), *r);
    return r;
}

// ---- scripts

struct Op {
    int kind, a, b, c;
    float f;
};
struct Scenario {
    int ctor[kMods], ctorArg[kMods], name[kMods], pri[kMods];
    std::vector<Op> ops;
};

static Scenario MakeScenario(int nOps)
{
    Scenario sc;
    for (int i = 0; i < kMods; ++i) {
        sc.ctor[i] = Pick(0, 3);
        sc.ctorArg[i] = Pick(0, 3);
        sc.name[i] = Pick(0, kNames - 1);
        sc.pri[i] = Pick(0, 3);
        updateScript[i].clear();
        for (int k = 0, n = Pick(1, 6); k < n; ++k) {
            int r = Pick(0, 9);
            updateScript[i].push_back(r < 7 ? 0 : r == 7 ? 2 : r == 8 ? 3 : 1);
        }
    }
    for (int k = 0; k < nOps; ++k)
        sc.ops.push_back({Pick(0, 23), Pick(0, kMods - 1), Pick(0, kMods - 1), Pick(0, 3),
                          static_cast<float>(Pick(1, 64)) / 8.0f});
    return sc;
}

static void* Fresh(unsigned size)
{
    void* p = New(size);
    std::memset(p, 0xcd, size);
    return p;
}

static void* KernelVariant(Side& sd, int v)
{
    // 0 none, 1 no scene graph, 2 no modifier task, 3 the task list
    if (v == 0) return nullptr;
    *reinterpret_cast<void**>(sd.kernel + 0x80) = v == 1 ? nullptr : sd.graph;
    *reinterpret_cast<void**>(sd.graph + 8) = v == 2 ? nullptr : sd.list[1];
    return sd.kernel;
}

static void Wrap(Side& sd, int l, int slots)
{
    void** vt = *static_cast<void***>(sd.list[l]);
    std::memcpy(sd.listVt[l], vt - 1, (slots + 1) * sizeof(void*));
    sd.origRemove[l] = vt[8];
    sd.listVt[l][1 + 8] = reinterpret_cast<void*>(TestListRemove);
    *static_cast<void***>(sd.list[l]) = sd.listVt[l] + 1;
    owner[sd.list[l]] = {sd.s, l};
}

static void Run(Side& sd, const Scenario& sc)
{
    const Api& a = api[sd.s];
    sd.log.clear();
    for (int l = 0; l < 2; ++l) {
        sd.list[l] = Fresh(0x18);
        (l ? a.tDefault : a.lDefault)(sd.list[l], nullptr);
        Wrap(sd, l, l ? 12 : 11);
    }
    for (int i = 0; i < kMods; ++i) {
        sd.m[i] = Fresh(0x18);
        sd.alive[i] = true;
        sd.updateCalls[i] = 0;
        owner[sd.m[i]] = {sd.s, i};
    }
    for (int i = 0; i < kMods; ++i) {
        // the vtable is wrapped after construction, so a host constructor inserts with the real one
        void* name = &names[sc.name[i]];
        const float pri = kPriorities[sc.pri[i]];
        switch (sc.ctor[i]) {
        case 0: a.mDefault(sd.m[i], nullptr); break;
        case 1: a.mName(sd.m[i], nullptr, name, pri); break;
        case 2: a.mHost(sd.m[i], nullptr, sc.ctorArg[i] < 2 ? sd.list[sc.ctorArg[i]] : nullptr, name, pri); break;
        default: a.mKernel(sd.m[i], nullptr, KernelVariant(sd, sc.ctorArg[i]), name, pri); break;
        }
        void** vt = *static_cast<void***>(sd.m[i]);
        if (i == 0) {
            std::memcpy(sd.modVt, vt - 1, sizeof sd.modVt);
            sd.origModDtor = vt[0];
            sd.modVt[1 + 0] = reinterpret_cast<void*>(TestModDtor);
            sd.modVt[1 + 8] = reinterpret_cast<void*>(TestUpdate);
        }
        *static_cast<void***>(sd.m[i]) = sd.modVt + 1;
    }
    Log(sd, "built\n");
    Snapshot(sd);

    for (size_t k = 0; k < sc.ops.size(); ++k) {
        const Op& op = sc.ops[k];
        void* L = sd.list[op.c & 1];
        void* m = sd.m[op.a];
        void* o = sd.m[op.b];
        if (!sd.alive[op.a] || (!sd.alive[op.b] && op.kind != 0)) continue;  // (same skip on both sides)
        Log(sd, "op %zu kind %d a %d b %d c %d f %g\n", k, op.kind, op.a, op.b, op.c, op.f);
        int r = -7;
        switch (op.kind) {
        case 0: r = VR(L, 2, m); break;                 // Add
        case 1: r = VR(L, 4, m); break;                 // InsertBefore(m)
        case 2: r = VR(L, 6, m); break;                 // InsertAfter(m)
        case 3: r = VR(L, 3, m, static_cast<const void*>(o)); break;  // InsertBefore(m, o)
        case 4: r = VR(L, 5, m, static_cast<const void*>(o)); break;  // InsertAfter(m, o)
        case 5: r = VR(L, 8, static_cast<const void*>(m)); break;     // Remove(m)
        case 6: r = VR(L, 7, static_cast<const void*>(&names[op.c])); break;  // Remove(name)
        case 7: r = ModIndex(sd, V<void*>(L, 1, static_cast<const void*>(&names[op.c]))); break;  // Find
        case 8: {
            int ret = -1;
            r = *a.check(L, nullptr, &ret, m);
            break;
        }
        case 9: V<void>(L, 9); break;  // RemoveAll
        case 10: r = VR(L, 10, op.f); break;
        case 11: V<void>(sd.list[1], 11, op.f); break;  // ModifierUpdateTask::Update
        case 12: r = VR(m, 6, op.c < 2 ? sd.list[op.c] : nullptr); break;  // SetHost(host)
        case 13: r = VR(m, 5, KernelVariant(sd, op.c)); break;             // SetHost(kernel)
        case 14: V<void>(m, 2, static_cast<const void*>(&names[op.c])); break;
        case 15: V<void>(m, 4, kPriorities[op.c]); break;
        case 16: V<void>(m, 7, (op.c & 1) != 0); break;
        case 17: {
            void* node = nullptr;
            V<void*>(m, 1, &node);
            const float p = V<float>(m, 3);
            Log(sd, "  name %d pri %g\n", NameIndex(node), p);
            if (node) Destroy(*cache, node);
            break;
        }
        case 18: {  // copy construction
            void* c = Fresh(0x18);
            a.mCopy(c, nullptr, m);
            const char* b = static_cast<const char*>(c);
            Log(sd, "  copy host %d name %d pri %08x bits %02x frame %08x\n", HostIndex(sd, *reinterpret_cast<void* const*>(b + 4)),
                NameIndex(*reinterpret_cast<void* const*>(b + 8)), *reinterpret_cast<const unsigned*>(b + 0xc),
                static_cast<unsigned char>(b[0x10]), *reinterpret_cast<const unsigned*>(b + 0x14));
            DestroyJet(c);  // the real vtable: not in any list
            break;
        }
        case 19:
            if (op.a != op.b) a.mAssign(m, nullptr, o);
            break;
        case 20:
            if (op.a != op.b) a.init(o, m);  // m becomes like o
            break;
        case 21: {  // a list copy and an assignment, then both destroyed (their RemoveAll detaches the modifiers)
            void* c = Fresh(0x18);
            a.lCopy(c, nullptr, L);
            void* d = Fresh(0x18);
            a.lDefault(d, nullptr);
            a.lAssign(d, nullptr, sd.list[(op.c + 1) & 1]);
            for (void* x : {c, d}) {
                const char* b = static_cast<const char*>(x);
                Log(sd, "  copy size %u busy %d frame %lu:", *reinterpret_cast<const unsigned*>(b + 0xc), b[0x10],
                    *reinterpret_cast<const unsigned long*>(b + 0x14));
                for (RawNode* n = Head(x)->next; n != Head(x); n = n->next) Log(sd, " %d", ModIndex(sd, n->value));
                Log(sd, "\n");
            }
            DestroyJet(c);
            DestroyJet(d);
            break;
        }
        case 22:
            r = a.touch(L, nullptr, m, static_cast<unsigned long>(op.c));
            break;
        default:
            a.setModifierHost(L, nullptr, op.c < 2 ? sd.list[op.c] : nullptr, m);
            break;
        }
        Log(sd, "  -> %d\n", r);
        Snapshot(sd);
    }

    // lists first (their RemoveAll detaches), then the modifiers that are left
    for (int l = 0; l < 2; ++l) {
        DestroyJet(sd.list[l]);
        owner.erase(sd.list[l]);
    }
    for (int i = 0; i < kMods; ++i)
        if (sd.alive[i]) DestroyJet(sd.m[i]);
    Log(sd, "end\n");
}

static void Compare(int round)
{
    ++checks;
    if (side[0].log == side[1].log) return;
    if (++bad > 10) return;
    const std::string &x = side[0].log, &y = side[1].log;
    size_t i = 0;
    while (i < x.size() && i < y.size() && x[i] == y[i]) ++i;
    size_t from = x.rfind("op ", i);
    if (from == std::string::npos) from = 0;
    std::printf("MISMATCH round %d\n-- ours:\n%s\n-- original:\n%s\n", round, x.substr(from, i - from + 300).c_str(),
                y.substr(from, i - from + 300).c_str());
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_rs_u4 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 2000;
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
    New = static_cast<NewFn>(Proc(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    Delete = static_cast<DeleteFn>(Proc(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    Create = static_cast<CreateFn>(Proc(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    Destroy = static_cast<DestroyFn>(Proc(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z"));
    cache = static_cast<void**>(Proc(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    if (!*cache) {
        std::printf("no PString cache\n");
        return 2;
    }
    names[0] = nullptr;
    const char* texts[] = {"", "wheel", "smoke", "lights"};
    for (int i = 1; i < kNames; ++i) names[i] = Create(*cache, reinterpret_cast<const unsigned char*>(texts[i]));
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        side[s].s = s;
    }
    for (int r = 0; r < rounds; ++r) {
        Scenario sc = MakeScenario(Pick(5, 60));
        for (int s = 0; s < 2; ++s) Run(side[s], sc);
        Compare(r);
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
