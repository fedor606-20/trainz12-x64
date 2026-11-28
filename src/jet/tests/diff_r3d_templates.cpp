// Differential test of the Render3DCore_7c wave-5 template/free-operator exports:
//   GeometryChunkTemplate<PackedVertexPNTA|PNTTA, GeometryChunkSingleStream>   (56)
//   ITree<Particle>                                                           (37)
//   StdMaterialPluginTemplate<AutoTexMaterial>                                (7)
//   the six Line free operators, DefaultInterpolator<Quaternion>               (8)
// Each export is called through GetProcAddress on both modules with identical state and the raw result and side
// effects are compared. The deep GeometryChunkTemplate behaviour is already covered by diff_r3d_u6; here the two
// exported instantiations are reached through their exported constructors/factory/vtable, not the header.
//
// usage: diff_r3d_templates.exe <dir with Render3DCore_7c.dll and Render3DCore_7c_orig.dll> [rounds]
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

namespace {

HMODULE mod[2];
unsigned long long checks, bad;
int printed;
unsigned rounds = 64;

int Pick(int lo, int hi) { return lo + static_cast<int>(rand() % (hi - lo + 1)); }

uint32_t RandBits(bool awkward)
{
    if (!awkward) return static_cast<uint32_t>(rand()) * 2654435761u + 0x9e3779b9u;
    static const uint32_t kBits[] = {0x00000000u, 0x80000000u, 0x7f800000u, 0xff800000u, 0x7fc00000u,
                                     0xffc00001u, 0x7fa00000u, 0xffffffffu, 0x7fffff7fu, 0x00800000u};
    return kBits[Pick(0, static_cast<int>(sizeof kBits / sizeof kBits[0]) - 1)];
}

float RandFloat()
{
    float f;
    const uint32_t u = RandBits((rand() & 3) == 0);
    std::memcpy(&f, &u, 4);
    return f;
}

void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        if (printed++ < 80) std::printf("MISMATCH %s\n", what);
    }
}

void CheckBytes(const void* a, const void* b, size_t n, const char* what)
{
    ++checks;
    if (std::memcmp(a, b, n) != 0) {
        ++bad;
        if (printed++ < 80) {
            std::printf("MISMATCH %s\n", what);
            const auto* pa = static_cast<const unsigned char*>(a);
            const auto* pb = static_cast<const unsigned char*>(b);
            for (size_t i = 0; i < n; ++i)
                if (pa[i] != pb[i]) {
                    std::printf("  first difference at +0x%zx: ours %02x orig %02x\n", i, pa[i], pb[i]);
                    break;
                }
        }
    }
}

void CheckValue(unsigned long long a, unsigned long long b, const char* what)
{
    ++checks;
    if (a != b) {
        ++bad;
        if (printed++ < 80)
            std::printf("MISMATCH %s: ours %llu (0x%llx) orig %llu (0x%llx)\n", what, a, a, b, b);
    }
}

template <class T>
T Proc(int side, const char* name)
{
    auto p = reinterpret_cast<T>(GetProcAddress(mod[side], name));
    if (!p) {
        std::printf("missing export %s in %s\n", name, side ? "orig" : "ours");
        std::exit(2);
    }
    return p;
}

template <class T>
T ProcIn(HMODULE module, const char* name)
{
    auto p = reinterpret_cast<T>(GetProcAddress(module, name));
    if (!p) {
        std::printf("missing export %s in %s\n", name, module ? "module" : "null");
        std::exit(2);
    }
    return p;
}

bool IsExecutable(const void* p)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof mbi)) return false;
    return mbi.State == MEM_COMMIT &&
           (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

unsigned VtableLength(const void* object)
{
    const void* const vt = *reinterpret_cast<void* const*>(object);
    unsigned n = 0;
    while (n < 0x200 && IsExecutable(reinterpret_cast<void* const*>(vt)[n])) ++n;
    return n;
}

unsigned VtableDataLength(void* const* vt)
{
    unsigned n = 0;
    while (n < 0x200 && IsExecutable(vt[n])) ++n;
    return n;
}

// ---- the member-function ABI (the same helper as diff_r3d_u6) ---------------------------------------------------

class CallLike {
public:
    int unused;
};

template <class R, class... A>
R CallSlot(void* object, unsigned byteOffset, A... args)
{
    using Fn = R (__cdecl CallLike::*)(A...);
    static_assert(sizeof(Fn) == sizeof(void*), "member function pointer size");
    void* const entry = reinterpret_cast<void**>(*reinterpret_cast<void**>(object))[byteOffset / sizeof(void*)];
    Fn fn;
    std::memcpy(&fn, &entry, sizeof fn);
    return (reinterpret_cast<CallLike*>(object)->*fn)(args...);
}

using DeleteChunkFn = void* (__thiscall*)(void*, unsigned);

void DestroyChunk(void* chunk)
{
    // GeometryChunk derives ReferenceCount, whose DeleteThis is slot 0; the deleting destructor is slot 1.
    void* const entry = reinterpret_cast<void**>(*reinterpret_cast<void**>(chunk))[1];
    reinterpret_cast<DeleteChunkFn>(entry)(chunk, 1);
}

// ---- Line free operators ----------------------------------------------------------------------------------------

struct Vec3 {
    float x, y, z;
};
struct Mat3 {
    float m[9];
};
struct LineRaw {
    void* vt;
    Vec3 direction;
    Vec3 point;
};
static_assert(sizeof(LineRaw) == 0x1c, "Line");

void LineExports()
{
    using AddFn = LineRaw(__cdecl*)(const LineRaw&, const Vec3&);
    using ScaleFn = LineRaw(__cdecl*)(const LineRaw&, float);
    using ScaleLeftFn = LineRaw(__cdecl*)(float, const LineRaw&);
    using MatrixFn = LineRaw(__cdecl*)(const Mat3&, const LineRaw&);

    const auto add = Proc<AddFn>(0, "??HJet@@YA?AVLine@0@ABV10@ABVVector3@0@@Z");
    const auto sub = Proc<AddFn>(0, "??GJet@@YA?AVLine@0@ABV10@ABVVector3@0@@Z");
    const auto mulL = Proc<ScaleFn>(0, "??DJet@@YA?AVLine@0@ABV10@M@Z");
    const auto mulR = Proc<ScaleLeftFn>(0, "??DJet@@YA?AVLine@0@MABV10@@Z");
    const auto mat = Proc<MatrixFn>(0, "??DJet@@YA?AVLine@0@ABVMatrix3@0@ABV10@@Z");
    const auto div = Proc<ScaleFn>(0, "??KJet@@YA?AVLine@0@ABV10@M@Z");

    const auto addO = Proc<AddFn>(1, "??HJet@@YA?AVLine@0@ABV10@ABVVector3@0@@Z");
    const auto subO = Proc<AddFn>(1, "??GJet@@YA?AVLine@0@ABV10@ABVVector3@0@@Z");
    const auto mulLO = Proc<ScaleFn>(1, "??DJet@@YA?AVLine@0@ABV10@M@Z");
    const auto mulRO = Proc<ScaleLeftFn>(1, "??DJet@@YA?AVLine@0@MABV10@@Z");
    const auto matO = Proc<MatrixFn>(1, "??DJet@@YA?AVLine@0@ABVMatrix3@0@ABV10@@Z");
    const auto divO = Proc<ScaleFn>(1, "??KJet@@YA?AVLine@0@ABV10@M@Z");

    for (unsigned i = 0; i < rounds; ++i) {
        LineRaw line;
        line.vt = nullptr;
        line.direction = {RandFloat(), RandFloat(), RandFloat()};
        line.point = {RandFloat(), RandFloat(), RandFloat()};
        const Vec3 delta = {RandFloat(), RandFloat(), RandFloat()};
        const float scale = RandFloat();
        Mat3 m;
        for (float& f : m.m) f = RandFloat();

        const LineRaw a = add(line, delta);
        const LineRaw b = addO(line, delta);
        CheckBytes(&a.direction, &b.direction, sizeof(Vec3) * 2, "Line operator+");
        const LineRaw c = sub(line, delta);
        const LineRaw d = subO(line, delta);
        CheckBytes(&c.direction, &d.direction, sizeof(Vec3) * 2, "Line operator-");
        const LineRaw e = mulL(line, scale);
        const LineRaw f = mulLO(line, scale);
        CheckBytes(&e.direction, &f.direction, sizeof(Vec3) * 2, "Line operator*(Line,float)");
        const LineRaw g = mulR(scale, line);
        const LineRaw h = mulRO(scale, line);
        CheckBytes(&g.direction, &h.direction, sizeof(Vec3) * 2, "Line operator*(float,Line)");
        const LineRaw k = mat(m, line);
        const LineRaw l = matO(m, line);
        CheckBytes(&k.direction, &l.direction, sizeof(Vec3) * 2, "Line operator*(Matrix3,Line)");
        const LineRaw n = div(line, scale);
        const LineRaw o = divO(line, scale);
        CheckBytes(&n.direction, &o.direction, sizeof(Vec3) * 2, "Line operator/(Line,float)");
    }
}

// ---- DefaultInterpolator<Quaternion> ----------------------------------------------------------------------------

struct Quat {
    float x, y, z, w;
};

void InterpolatorExports()
{
    using AssignFn = void* (__thiscall*)(void*, const void*);
    using InterpFn = Quat(__cdecl*)(const Quat&, const Quat&, float);
    const auto assignOurs = Proc<AssignFn>(0, "??4?$DefaultInterpolator@VQuaternion@Jet@@@Jet@@QAEAAV01@ABV01@@Z");
    const auto assignOrig = Proc<AssignFn>(1, "??4?$DefaultInterpolator@VQuaternion@Jet@@@Jet@@QAEAAV01@ABV01@@Z");
    const auto interpOurs = Proc<InterpFn>(0, "?Interpolate@?$DefaultInterpolator@VQuaternion@Jet@@@Jet@@SA?AVQuaternion@2@ABV32@0M@Z");
    const auto interpOrig = Proc<InterpFn>(1, "?Interpolate@?$DefaultInterpolator@VQuaternion@Jet@@@Jet@@SA?AVQuaternion@2@ABV32@0M@Z");

    unsigned char a[4] = {}, b[4] = {};
    CheckValue(reinterpret_cast<uintptr_t>(assignOurs(a, b)), reinterpret_cast<uintptr_t>(a), "Interpolator::operator=");
    CheckValue(reinterpret_cast<uintptr_t>(assignOrig(b, a)), reinterpret_cast<uintptr_t>(b), "orig Interpolator::operator=");

    for (unsigned i = 0; i < rounds; ++i) {
        const Quat q0 = {RandFloat(), RandFloat(), RandFloat(), RandFloat()};
        const Quat q1 = {RandFloat(), RandFloat(), RandFloat(), RandFloat()};
        const float t = RandFloat();
        const Quat ra = interpOurs(q0, q1, t);
        const Quat rb = interpOrig(q0, q1, t);
        CheckBytes(&ra, &rb, sizeof ra, "DefaultInterpolator<Quaternion>::Interpolate");
    }
}

// ---- ITree<Particle> --------------------------------------------------------------------------------------------

#define IT(s) "?$ITree@VParticle@Jet@@@Jet@@" s

constexpr unsigned kParticleSize = 0x140;  // more than the original's 0x12c
constexpr unsigned kMaxNodes = 8;

struct It {
    void* node;
    It(void* n = nullptr) : node(n) {}
    // An explicit copy constructor/destructor makes this non-trivial for the ABI, so the callers use the same
    // hidden result pointer the original's Iterator uses (the callee's `ret 8`).
    It(const It& o) : node(o.node) {}
    ~It() {}
};

struct Harness {
    unsigned char nodes[kMaxNodes][kParticleSize];
    int nodeCount;
    int calls[64][3];  // slot, self index+1, arg index+1
    int callCount;
};

Harness* gHarness;
void* fakeVtable[96];

int NodeIndexOf(Harness& h, const void* p)
{
    if (!p) return 0;
    for (int i = 0; i < h.nodeCount; ++i)
        if (p == h.nodes[i]) return i + 1;
    return -1;
}

int NodeIndex(const void* p) { return NodeIndexOf(*gHarness, p); }

void Record(int slot, const void* self, const void* arg)
{
    if (gHarness->callCount < 64) {
        gHarness->calls[gHarness->callCount][0] = slot;
        gHarness->calls[gHarness->callCount][1] = NodeIndex(self);
        gHarness->calls[gHarness->callCount][2] = NodeIndex(arg);
    }
    ++gHarness->callCount;
}

// A deleting destructor is __thiscall; a free function cannot use that convention, so the slot is a member.
struct DtorStub {
    void* __thiscall dtor(int flags)
    {
        Record(0, this, reinterpret_cast<void*>(static_cast<uintptr_t>(flags)));
        return this;
    }
};

void __cdecl SlotDeleteAll(void* self) { Record(29, self, nullptr); }
void __cdecl SlotRemoveAllChildren(void* self) { Record(28, self, nullptr); }
void __cdecl SlotAdd(void* self, void* child) { Record(18, self, child); }
bool __cdecl SlotRemove(void* self, void* child)
{
    Record(24, self, child);
    return true;
}
int __cdecl SlotNoop() { return 0; }

int __cdecl DisplayNoop() { return 0; }
void* displayVtable[0x200 / 4];
unsigned char fakeDisplay[0x400];

void* textureManagerVtable[16];
unsigned char fakeTextureManager[8];

void InitDisplay()
{
    for (void*& slot : displayVtable) slot = reinterpret_cast<void*>(&DisplayNoop);
    std::memset(fakeDisplay, 0, sizeof fakeDisplay);
    *reinterpret_cast<void**>(fakeDisplay) = displayVtable;
    for (void*& slot : textureManagerVtable) slot = reinterpret_cast<void*>(&DisplayNoop);
    std::memset(fakeTextureManager, 0, sizeof fakeTextureManager);
    *reinterpret_cast<void**>(fakeTextureManager) = textureManagerVtable;
}

void InitVtable()
{
    for (void*& slot : fakeVtable) slot = reinterpret_cast<void*>(&SlotNoop);
    const auto dtorMember = &DtorStub::dtor;
    void* dtorEntry = nullptr;
    std::memcpy(&dtorEntry, &dtorMember, sizeof dtorEntry);
    fakeVtable[0] = dtorEntry;
    fakeVtable[18] = reinterpret_cast<void*>(&SlotAdd);
    fakeVtable[24] = reinterpret_cast<void*>(&SlotRemove);
    fakeVtable[28] = reinterpret_cast<void*>(&SlotRemoveAllChildren);
    fakeVtable[29] = reinterpret_cast<void*>(&SlotDeleteAll);
}

void InitHarness(Harness& h, int nodes)
{
    std::memset(&h, 0, sizeof h);
    h.nodeCount = nodes;
    for (int i = 0; i < nodes; ++i) *reinterpret_cast<void**>(h.nodes[i]) = fakeVtable;
}

uint32_t& T(Harness& h, int node, int field)  // field 0..5 (count,parent,head,tail,next,prev)
{
    return *reinterpret_cast<uint32_t*>(h.nodes[node] + 0xc + field * 4);
}

void* Tree(Harness& h, int node) { return h.nodes[node] + 0xc; }

void RootChildren(Harness& h, int children)
{
    T(h, 0, 0) = static_cast<uint32_t>(children);
    T(h, 0, 2) = children ? reinterpret_cast<uint32_t>(h.nodes[1]) : 0;
    T(h, 0, 3) = children ? reinterpret_cast<uint32_t>(h.nodes[children]) : 0;
    for (int i = 1; i <= children; ++i) {
        T(h, i, 1) = reinterpret_cast<uint32_t>(h.nodes[0]);
        T(h, i, 4) = i < children ? reinterpret_cast<uint32_t>(h.nodes[i + 1]) : 0;
        T(h, i, 5) = i > 1 ? reinterpret_cast<uint32_t>(h.nodes[i - 1]) : 0;
    }
}

// root -> {1, 3}, 1 -> {2}
void NestedTree(Harness& h)
{
    RootChildren(h, 3);
    T(h, 0, 0) = 2;
    T(h, 0, 3) = reinterpret_cast<uint32_t>(h.nodes[3]);
    T(h, 3, 5) = reinterpret_cast<uint32_t>(h.nodes[1]);
    T(h, 1, 0) = 1;
    T(h, 1, 2) = reinterpret_cast<uint32_t>(h.nodes[2]);
    T(h, 1, 3) = reinterpret_cast<uint32_t>(h.nodes[2]);
    T(h, 1, 4) = reinterpret_cast<uint32_t>(h.nodes[3]);  // node1's sibling
    T(h, 2, 1) = reinterpret_cast<uint32_t>(h.nodes[1]);
    T(h, 2, 4) = 0;
    T(h, 2, 5) = 0;
}

struct Snapshot {
    uint32_t nodes[kMaxNodes][6];
    int calls[64][3];
    int callCount;
};

void Capture(Harness& h, Snapshot& s)
{
    std::memset(&s, 0, sizeof s);
    for (int i = 0; i < h.nodeCount; ++i) {
        s.nodes[i][0] = T(h, i, 0);  // count is a number
        for (int f = 1; f < 6; ++f)  // the five links are node pointers: compare indices
            s.nodes[i][f] = static_cast<uint32_t>(NodeIndexOf(h, reinterpret_cast<void*>(T(h, i, f))));
    }
    std::memcpy(s.calls, h.calls, sizeof h.calls);
    s.callCount = h.callCount;
}

void CompareState(Harness& a, Harness& b, const char* what)
{
    Snapshot sa, sb;
    Capture(a, sa);
    Capture(b, sb);
    CheckBytes(&sa, &sb, sizeof sa, what);
}

struct ItApi {
    using CtorFn = void(__thiscall*)(void*);
    using AssignFn = void* (__thiscall*)(void*, const void*);
    using CtFn = unsigned int(__thiscall*)(const void*);
    using VoidFn = void(__thiscall*)(void*);
    // Iterator is returned through the hidden result pointer (the callee's `ret 8`); spelling it explicitly
    // avoids depending on the test compiler's class-return classification.
    using AddFn = void(__thiscall*)(void*, It*, void*);
    using AccessFn = void(__thiscall*)(void*, It*);
    using InsFn = void(__thiscall*)(void*, It*, It*, void*);
    using RemItFn = void(__thiscall*)(void*, It*, It*);
    using RemNodeFn = bool(__thiscall*)(void*, void*);
    using PopFn = void*(__thiscall*)(void*);
    using ParentFn = void(__thiscall*)(void*, void*);
    using IndexFn = void(__thiscall*)(void*, It*, unsigned long);
    using IndexCFn = void(__thiscall*)(const void*, It*, unsigned long);
    using ExistsFn = bool(__thiscall*)(const void*, It*);
    using ExistsCFn = bool(__thiscall*)(const void*, const It*);
    using TraverseFn = void(__thiscall*)(void*, void(__cdecl*)(void*));
    using IRemoveFn = void(__thiscall*)(void*, void*);

    CtorFn ctor;
    VoidFn dtor;
    AssignFn assign;
    AddFn add, push;
    InsFn insertBefore, insertAfter;
    RemItFn removeIt;
    RemNodeFn removeNode;
    PopFn removeAny, pop;
    VoidFn removeAll, removeAllChildren, deleteAll;
    ParentFn setParent;
    IndexFn index;
    IndexCFn indexC;
    CtFn count, countAll;
    ExistsFn exists;
    ExistsCFn existsC;
    AccessFn first, last, end, next, prev, parent;
    // const-returning accessors have the same ABI (sret pointer)
    AccessFn firstC, lastC, endC, nextC, prevC, parentC;
    TraverseFn traverse, traverseC;
    IRemoveFn iRemove, iDelete;
};

ItApi Api(int side)
{
    ItApi a{};
    a.ctor = Proc<ItApi::CtorFn>(side, "??0" IT("QAE@XZ"));
    a.dtor = Proc<ItApi::VoidFn>(side, "??1" IT("QAE@XZ"));
    a.assign = Proc<ItApi::AssignFn>(side, "??4" IT("QAEAAV01@ABV01@@Z"));
    a.add = Proc<ItApi::AddFn>(side, "?Add@" IT("QAE?AVIterator@12@PAVParticle@2@@Z"));
    a.push = Proc<ItApi::AddFn>(side, "?Push@" IT("QAE?AVIterator@12@PAVParticle@2@@Z"));
    a.insertBefore = Proc<ItApi::InsFn>(side, "?InsertBefore@" IT("QAE?AVIterator@12@AAV312@PAVParticle@2@@Z"));
    a.insertAfter = Proc<ItApi::InsFn>(side, "?InsertAfter@" IT("QAE?AVIterator@12@AAV312@PAVParticle@2@@Z"));
    a.removeIt = Proc<ItApi::RemItFn>(side, "?Remove@" IT("QAE?AVIterator@12@AAV312@@Z"));
    a.removeNode = Proc<ItApi::RemNodeFn>(side, "?Remove@" IT("QAE_NPAVParticle@2@@Z"));
    a.removeAny = Proc<ItApi::PopFn>(side, "?Remove@" IT("QAEPAVParticle@2@XZ"));
    a.pop = Proc<ItApi::PopFn>(side, "?Pop@" IT("QAEPAVParticle@2@XZ"));
    a.removeAll = Proc<ItApi::VoidFn>(side, "?RemoveAll@" IT("QAEXXZ"));
    a.removeAllChildren = Proc<ItApi::VoidFn>(side, "?RemoveAllChildren@" IT("QAEXXZ"));
    a.deleteAll = Proc<ItApi::VoidFn>(side, "?DeleteAll@" IT("QAEXXZ"));
    a.setParent = Proc<ItApi::ParentFn>(side, "?SetParent@" IT("QAEXPAVParticle@2@@Z"));
    a.index = Proc<ItApi::IndexFn>(side, "?Index@" IT("QAE?AVIterator@12@K@Z"));
    a.indexC = Proc<ItApi::IndexCFn>(side, "?Index@" IT("QBE?AVConstIterator@12@K@Z"));
    a.count = Proc<ItApi::CtFn>(side, "?Count@" IT("QBEIXZ"));
    a.countAll = Proc<ItApi::CtFn>(side, "?CountAll@" IT("QBEIXZ"));
    a.exists = Proc<ItApi::ExistsFn>(side, "?Exists@" IT("QBE_NAAVConstIterator@12@@Z"));
    a.existsC = Proc<ItApi::ExistsCFn>(side, "?Exists@" IT("QBE_NABVIterator@12@@Z"));
    a.first = Proc<ItApi::AccessFn>(side, "?First@" IT("QAE?AVIterator@12@XZ"));
    a.last = Proc<ItApi::AccessFn>(side, "?Last@" IT("QAE?AVIterator@12@XZ"));
    a.end = Proc<ItApi::AccessFn>(side, "?End@" IT("QAE?AVIterator@12@XZ"));
    a.next = Proc<ItApi::AccessFn>(side, "?Next@" IT("QAE?AVIterator@12@XZ"));
    a.prev = Proc<ItApi::AccessFn>(side, "?Prev@" IT("QAE?AVIterator@12@XZ"));
    a.parent = Proc<ItApi::AccessFn>(side, "?Parent@" IT("QAE?AVIterator@12@XZ"));
    a.firstC = Proc<ItApi::AccessFn>(side, "?First@" IT("QBE?AVConstIterator@12@XZ"));
    a.lastC = Proc<ItApi::AccessFn>(side, "?Last@" IT("QBE?AVConstIterator@12@XZ"));
    a.endC = Proc<ItApi::AccessFn>(side, "?End@" IT("QBE?AVConstIterator@12@XZ"));
    a.nextC = Proc<ItApi::AccessFn>(side, "?Next@" IT("QBE?AVConstIterator@12@XZ"));
    a.prevC = Proc<ItApi::AccessFn>(side, "?Prev@" IT("QBE?AVConstIterator@12@XZ"));
    a.parentC = Proc<ItApi::AccessFn>(side, "?Parent@" IT("QBE?AVConstIterator@12@XZ"));
    a.traverse = Proc<ItApi::TraverseFn>(side, "?Traverse@" IT("QAEXP6AXPAVParticle@2@@Z@Z"));
    a.traverseC = Proc<ItApi::TraverseFn>(side, "?Traverse@" IT("QBEXP6AXPBVParticle@2@@Z@Z"));
    a.iRemove = Proc<ItApi::IRemoveFn>(side, "?IRemove@" IT("AAEXVIterator@12@@Z"));
    a.iDelete = Proc<ItApi::IRemoveFn>(side, "?IDelete@" IT("AAEXVIterator@12@@Z"));
    return a;
}

int gTraverse;
void* gTraverseOrder[16];

void __cdecl TraverseRec(void* node)
{
    if (gTraverse < 16) gTraverseOrder[gTraverse] = node;
    ++gTraverse;
}

// Every export has a different return type, so the checks below are paired calls, not a generic runner.
void TreeExports()
{
    const ItApi o = Api(0);
    const ItApi g = Api(1);

    // constructor
    {
        Harness a, b;
        InitHarness(a, 1);
        InitHarness(b, 1);
        std::memset(a.nodes[0], 0, kParticleSize);
        std::memset(b.nodes[0], 0, kParticleSize);
        o.ctor(Tree(a, 0));
        g.ctor(Tree(b, 0));
        CompareState(a, b, "ITree ctor");
    }

    // add / push / first / last / end / next / prev / parent / count
    for (int op = 0; op < 2; ++op) {
        Harness a, b;
        InitHarness(a, 5);
        InitHarness(b, 5);
        It ra{}, rb{};
        gHarness = &a;
        if (op == 0) {
            o.add(Tree(a, 0), &ra, a.nodes[1]);
            It r2, r3;
            o.add(Tree(a, 0), &r2, a.nodes[2]);
            o.add(Tree(a, 0), &r3, a.nodes[3]);
            CheckValue(r2.node == a.nodes[2], 1, "Add return 2");
            CheckValue(r3.node == a.nodes[3], 1, "Add return 3");
        } else {
            o.push(Tree(a, 0), &ra, a.nodes[1]);
            It r2, r3;
            o.push(Tree(a, 0), &r2, a.nodes[2]);
            o.push(Tree(a, 0), &r3, a.nodes[3]);
            CheckValue(r2.node == a.nodes[2], 1, "Push return 2");
            CheckValue(r3.node == a.nodes[3], 1, "Push return 3");
        }
        gHarness = &b;
        if (op == 0) {
            g.add(Tree(b, 0), &rb, b.nodes[1]);
            It r2, r3;
            g.add(Tree(b, 0), &r2, b.nodes[2]);
            g.add(Tree(b, 0), &r3, b.nodes[3]);
        } else {
            g.push(Tree(b, 0), &rb, b.nodes[1]);
            It r2, r3;
            g.push(Tree(b, 0), &r2, b.nodes[2]);
            g.push(Tree(b, 0), &r3, b.nodes[3]);
        }
        CompareState(a, b, op == 0 ? "ITree Add" : "ITree Push");
        gHarness = &a;
        const int raIndex = NodeIndex(ra.node);
        gHarness = &b;
        CheckValue(static_cast<unsigned>(raIndex), static_cast<unsigned>(NodeIndex(rb.node)), "Add/Push return");
        CheckValue(o.count(Tree(a, 0)), g.count(Tree(b, 0)), "ITree Count");

        // The two harnesses have different addresses; compare node indices while each one is current.
        int ia[14], ib[14];
        const auto access = [](auto fn, void* self) {
            It t;
            fn(self, &t);
            return NodeIndex(t.node);
        };
        gHarness = &a;
        ia[0] = access(o.first, Tree(a, 0));
        ia[1] = access(o.last, Tree(a, 0));
        ia[2] = access(o.end, Tree(a, 0));
        ia[3] = access(o.parent, Tree(a, 1));
        ia[4] = access(o.next, Tree(a, 1));
        ia[5] = access(o.prev, Tree(a, 1));
        ia[6] = access(o.firstC, Tree(a, 0));
        ia[7] = access(o.lastC, Tree(a, 0));
        ia[8] = access(o.endC, Tree(a, 0));
        ia[9] = access(o.parentC, Tree(a, 1));
        ia[10] = access(o.nextC, Tree(a, 1));
        ia[11] = access(o.prevC, Tree(a, 1));
        gHarness = &b;
        ib[0] = access(g.first, Tree(b, 0));
        ib[1] = access(g.last, Tree(b, 0));
        ib[2] = access(g.end, Tree(b, 0));
        ib[3] = access(g.parent, Tree(b, 1));
        ib[4] = access(g.next, Tree(b, 1));
        ib[5] = access(g.prev, Tree(b, 1));
        ib[6] = access(g.firstC, Tree(b, 0));
        ib[7] = access(g.lastC, Tree(b, 0));
        ib[8] = access(g.endC, Tree(b, 0));
        ib[9] = access(g.parentC, Tree(b, 1));
        ib[10] = access(g.nextC, Tree(b, 1));
        ib[11] = access(g.prevC, Tree(b, 1));
        static const char* const kAccessorNames[] = {"First", "Last", "End", "Parent", "Next", "Prev",
                                                     "First const", "Last const", "End const", "Parent const",
                                                     "Next const", "Prev const"};
        for (int i = 0; i < 12; ++i) CheckValue(static_cast<unsigned>(ia[i]), static_cast<unsigned>(ib[i]),
                                                kAccessorNames[i]);
    }

    // pop / remove() / remove(node) / remove(iterator) / removeAll
    {
        Harness a, b;
        InitHarness(a, 5);
        InitHarness(b, 5);
        RootChildren(a, 3);
        RootChildren(b, 3);
        gHarness = &a;
        void* const pa = o.pop(Tree(a, 0));
        const int paIndex = NodeIndex(pa);
        gHarness = &b;
        void* const pb = g.pop(Tree(b, 0));
        CompareState(a, b, "ITree Pop");
        CheckValue(static_cast<unsigned>(paIndex), static_cast<unsigned>(NodeIndex(pb)), "Pop return");

        InitHarness(a, 5);
        InitHarness(b, 5);
        RootChildren(a, 3);
        RootChildren(b, 3);
        gHarness = &a;
        void* const ra = o.removeAny(Tree(a, 0));
        const int raIndex = NodeIndex(ra);
        gHarness = &b;
        void* const rb = g.removeAny(Tree(b, 0));
        CompareState(a, b, "ITree Remove()");
        CheckValue(static_cast<unsigned>(raIndex), static_cast<unsigned>(NodeIndex(rb)), "Remove() return");

        InitHarness(a, 5);
        InitHarness(b, 5);
        RootChildren(a, 3);
        RootChildren(b, 3);
        gHarness = &a;
        const bool ba = o.removeNode(Tree(a, 0), a.nodes[2]);
        gHarness = &b;
        const bool bb = g.removeNode(Tree(b, 0), b.nodes[2]);
        CompareState(a, b, "ITree Remove(node)");
        CheckValue(ba, bb, "Remove(node) return");
        CheckValue(o.removeNode(Tree(a, 0), a.nodes[4]), g.removeNode(Tree(b, 0), b.nodes[4]), "Remove(other)");

        InitHarness(a, 5);
        InitHarness(b, 5);
        RootChildren(a, 3);
        RootChildren(b, 3);
        It ia(a.nodes[2]);
        It ib(b.nodes[2]);
        It na, nb;
        gHarness = &a;
        o.removeIt(Tree(a, 0), &na, &ia);
        const int naIndex = NodeIndex(na.node);
        gHarness = &b;
        g.removeIt(Tree(b, 0), &nb, &ib);
        CompareState(a, b, "ITree Remove(iterator)");
        CheckValue(static_cast<unsigned>(naIndex), static_cast<unsigned>(NodeIndex(nb.node)),
                   "Remove(iterator) return");

        InitHarness(a, 5);
        InitHarness(b, 5);
        RootChildren(a, 3);
        RootChildren(b, 3);
        gHarness = &a;
        o.removeAll(Tree(a, 0));
        gHarness = &b;
        g.removeAll(Tree(b, 0));
        CompareState(a, b, "ITree RemoveAll");
    }

    // insert before / after
    for (int op = 0; op < 2; ++op) {
        Harness a, b;
        InitHarness(a, 5);
        InitHarness(b, 5);
        RootChildren(a, 3);
        RootChildren(b, 3);
        It ta(a.nodes[2]);
        It tb(b.nodes[2]);
        It ra, rb;
        gHarness = &a;
        if (op == 0)
            o.insertBefore(Tree(a, 0), &ra, &ta, a.nodes[4]);
        else
            o.insertAfter(Tree(a, 0), &ra, &ta, a.nodes[4]);
        const int raIndex = NodeIndex(ra.node);
        gHarness = &b;
        if (op == 0)
            g.insertBefore(Tree(b, 0), &rb, &tb, b.nodes[4]);
        else
            g.insertAfter(Tree(b, 0), &rb, &tb, b.nodes[4]);
        CompareState(a, b, op == 0 ? "ITree InsertBefore" : "ITree InsertAfter");
        CheckValue(static_cast<unsigned>(raIndex), static_cast<unsigned>(NodeIndex(rb.node)), "Insert return");
    }

    // index / exists
    {
        Harness a, b;
        InitHarness(a, 5);
        InitHarness(b, 5);
        RootChildren(a, 3);
        RootChildren(b, 3);
        for (unsigned i = 0; i < 3; ++i) {
            It ta, tb;
            gHarness = &a;
            o.index(Tree(a, 0), &ta, i);
            const int ia = NodeIndex(ta.node);
            o.indexC(Tree(a, 0), &ta, i);
            const int iac = NodeIndex(ta.node);
            gHarness = &b;
            g.index(Tree(b, 0), &tb, i);
            const int ib = NodeIndex(tb.node);
            g.indexC(Tree(b, 0), &tb, i);
            CheckValue(static_cast<unsigned>(ia), static_cast<unsigned>(ib), "ITree Index");
            CheckValue(static_cast<unsigned>(iac), static_cast<unsigned>(NodeIndex(tb.node)), "ITree Index const");
        }
        It present(a.nodes[2]);
        It absent(a.nodes[4]);
        It presentB(b.nodes[2]);
        It absentB(b.nodes[4]);
        CheckValue(o.exists(Tree(a, 0), &present), g.exists(Tree(b, 0), &presentB), "ITree Exists present");
        CheckValue(o.exists(Tree(a, 0), &absent), g.exists(Tree(b, 0), &absentB), "ITree Exists absent");
        CheckValue(o.existsC(Tree(a, 0), &present), g.existsC(Tree(b, 0), &presentB), "ITree Exists const present");
        CheckValue(o.existsC(Tree(a, 0), &absent), g.existsC(Tree(b, 0), &absentB), "ITree Exists const absent");
    }

    // countAll / traverse / removeAllChildren / deleteAll / destructor
    {
        Harness a, b;
        InitHarness(a, 5);
        InitHarness(b, 5);
        NestedTree(a);
        NestedTree(b);
        CheckValue(o.countAll(Tree(a, 0)), g.countAll(Tree(b, 0)), "ITree CountAll");

        gTraverse = 0;
        gHarness = &a;
        o.traverse(Tree(a, 0), TraverseRec);
        int orderA[16];
        const int na = gTraverse;
        for (int i = 0; i < na && i < 16; ++i) orderA[i] = NodeIndex(gTraverseOrder[i]);
        gTraverse = 0;
        gHarness = &b;
        g.traverse(Tree(b, 0), TraverseRec);
        const int nb = gTraverse;
        CheckValue(static_cast<unsigned>(na), static_cast<unsigned>(nb), "ITree Traverse count");
        for (int i = 0; i < na && i < nb; ++i)
            CheckValue(static_cast<unsigned>(orderA[i]),
                       static_cast<unsigned>(NodeIndex(gTraverseOrder[i])), "ITree Traverse order");

        gTraverse = 0;
        gHarness = &a;
        o.traverseC(Tree(a, 0), TraverseRec);
        const int nca = gTraverse;
        gTraverse = 0;
        gHarness = &b;
        g.traverseC(Tree(b, 0), TraverseRec);
        CheckValue(static_cast<unsigned>(nca), static_cast<unsigned>(gTraverse), "ITree Traverse const count");

        InitHarness(a, 5);
        InitHarness(b, 5);
        NestedTree(a);
        NestedTree(b);
        gHarness = &a;
        o.removeAllChildren(Tree(a, 0));
        gHarness = &b;
        g.removeAllChildren(Tree(b, 0));
        CompareState(a, b, "ITree RemoveAllChildren");

        InitHarness(a, 5);
        InitHarness(b, 5);
        NestedTree(a);
        NestedTree(b);
        gHarness = &a;
        o.deleteAll(Tree(a, 0));
        gHarness = &b;
        g.deleteAll(Tree(b, 0));
        CompareState(a, b, "ITree DeleteAll");

        InitHarness(a, 5);
        InitHarness(b, 5);
        NestedTree(a);
        NestedTree(b);
        gHarness = &a;
        o.dtor(Tree(a, 0));
        gHarness = &b;
        g.dtor(Tree(b, 0));
        CompareState(a, b, "ITree destructor");
    }

    // assign / setParent / IRemove / IDelete
    {
        Harness a, b;
        InitHarness(a, 4);
        InitHarness(b, 4);
        for (int f = 0; f < 6; ++f) {
            T(a, 1, f) = static_cast<uint32_t>(0x11110000u + f);
            T(b, 1, f) = static_cast<uint32_t>(0x11110000u + f);
        }
        o.assign(Tree(a, 0), Tree(a, 1));
        g.assign(Tree(b, 0), Tree(b, 1));
        CompareState(a, b, "ITree operator=");

        InitHarness(a, 4);
        InitHarness(b, 4);
        RootChildren(a, 3);
        RootChildren(b, 3);
        gHarness = &a;
        o.setParent(Tree(a, 1), a.nodes[3]);
        gHarness = &b;
        g.setParent(Tree(b, 1), b.nodes[3]);
        CompareState(a, b, "ITree SetParent");

        InitHarness(a, 4);
        InitHarness(b, 4);
        RootChildren(a, 3);
        RootChildren(b, 3);
        gHarness = &a;
        o.iRemove(Tree(a, 0), a.nodes[2]);
        gHarness = &b;
        g.iRemove(Tree(b, 0), b.nodes[2]);
        CompareState(a, b, "ITree IRemove");

        InitHarness(a, 4);
        InitHarness(b, 4);
        RootChildren(a, 3);
        RootChildren(b, 3);
        gHarness = &a;
        o.iDelete(Tree(a, 0), a.nodes[2]);
        gHarness = &b;
        g.iDelete(Tree(b, 0), b.nodes[2]);
        CompareState(a, b, "ITree IDelete");
    }
}

// ---- GeometryChunkTemplate instantiations ------------------------------------------------------------------------

#define GEO(v) "?$GeometryChunkTemplate@U" v "@Jet@@VGeometryChunkSingleStream@2@@Jet@@"

struct GeoApi {
    using Ctor0 = void(__thiscall*)(void*);
    using Ctor5 = void(__thiscall*)(void*, unsigned, unsigned, unsigned, unsigned, unsigned);
    using Dtor = void(__thiscall*)(void*);
    using Create0 = void*(__cdecl*)(const void*);
    using Create4 = void*(__cdecl*)(const void*, unsigned, unsigned, unsigned, unsigned);
    using DupIndex = void(__cdecl*)(void*, void*);
    using DupVertex = void(__cdecl*)(void*, void*, int, int);
    using Resize = void(__cdecl*)(void*, void*, unsigned, int, int);
    using GetArray = void*(__cdecl*)(void*, int, int);
    using GetConstArray = const void*(__cdecl*)(const void*, int, int);
    using GetId = int(__cdecl*)(const void*, int);
    using GetSize = int(__cdecl*)(const void*, int);
    using GetOffset = int(__cdecl*)(const void*, int, int);
    using InitInstance = void(__cdecl*)(const void*, void*);

    Ctor0 ctor0;
    Ctor5 ctor5;
    Dtor dtor;
    Create0 create0;
    Create4 create4;
    DupIndex dupIndex;
    DupVertex dupVertex;
    Resize resize;
    GetArray getArray;
    GetConstArray getConstArray;
    GetId getId;
    GetSize getSize;
    GetOffset getOffset;
    InitInstance initInstance;
    void* vtable;
};

GeoApi Geo(int side, const char* vertex)
{
    GeoApi a{};
    const auto name = [vertex](const char* suffix) {
        return "?$GeometryChunkTemplate@U" + std::string(vertex) +
               "@Jet@@VGeometryChunkSingleStream@2@@Jet@@" + suffix;
    };
    a.ctor0 = Proc<GeoApi::Ctor0>(side, ("??0" + name("QAE@XZ")).c_str());
    a.ctor5 = Proc<GeoApi::Ctor5>(side, ("??0" + name("QAE@IIIII@Z")).c_str());
    a.dtor = Proc<GeoApi::Dtor>(side, ("??1" + name("MAE@XZ")).c_str());
    a.create0 = Proc<GeoApi::Create0>(side, ("?CreateInstance@" + name("UBAPAVGeometryChunk@2@XZ")).c_str());
    a.create4 = Proc<GeoApi::Create4>(side, ("?CreateInstance@" + name("UBAPAVGeometryChunk@2@IIII@Z")).c_str());
    a.dupIndex =
        Proc<GeoApi::DupIndex>(side, ("?DuplicateIndexArray@" + name("UAAXPAVDisplay@2@@Z")).c_str());
    a.dupVertex = Proc<GeoApi::DupVertex>(
        side, ("?DuplicateVertexArray@" + name("UAAXPAVDisplay@2@HH@Z")).c_str());
    a.resize =
        Proc<GeoApi::Resize>(side, ("?ResizeVertexArray@" + name("UAAXPAVDisplay@2@IHH@Z")).c_str());
    a.getArray = Proc<GeoApi::GetArray>(side, ("?GetVertexArray@" + name("UAAPAXHH@Z")).c_str());
    a.getConstArray =
        Proc<GeoApi::GetConstArray>(side, ("?GetVertexArray@" + name("UBAPBXHH@Z")).c_str());
    a.getId = Proc<GeoApi::GetId>(side, ("?GetVertexTypeID@" + name("UBA?AW4VertexTypeID@2@H@Z")).c_str());
    a.getSize = Proc<GeoApi::GetSize>(side, ("?GetVertexSize@" + name("UBAHH@Z")).c_str());
    a.getOffset =
        Proc<GeoApi::GetOffset>(side, ("?GetItemOffset@" + name("UBAHW4VertexItemType@2@H@Z")).c_str());
    a.initInstance = Proc<GeoApi::InitInstance>(side, ("?InitInstance@" + name("IBAXAAV12@@Z")).c_str());
    a.vtable = reinterpret_cast<void*>(GetProcAddress(mod[side], ("??_7" + name("6B@")).c_str()));
    if (!a.vtable) {
        std::printf("missing vtable %s\n", vertex);
        std::exit(2);
    }
    return a;
}

struct ItemView {
    unsigned int stride;
    void* data;
};

unsigned DwordAt(const void* p, unsigned off)
{
    return *reinterpret_cast<const unsigned*>(static_cast<const unsigned char*>(p) + off);
}
void* PointerAt(const void* p, unsigned off)
{
    return *reinterpret_cast<void* const*>(static_cast<const unsigned char*>(p) + off);
}

void CompareGeometryObject(const char* vertex, void* a, void* b, const GeoApi& o, const GeoApi& g)
{
    CheckValue(VtableLength(a), VtableLength(b), "chunk vtable length");
    CheckValue(VtableLength(a), 73u, "chunk vtable length 73");

    static const unsigned kFields[] = {0x18, 0x1c, 0x20, 0x24, 0x28, 0x38, 0x3c, 0x40, 0x44};
    for (unsigned off : kFields) CheckValue(DwordAt(a, off), DwordAt(b, off), "chunk field");

    void* const ab = PointerAt(a, 0x5c);
    void* const bb = PointerAt(b, 0x5c);
    CheckValue(ab ? DwordAt(ab, 0) : 0xffffffffu, bb ? DwordAt(bb, 0) : 0xffffffffu, "vertex block refs");
    CheckValue(ab ? DwordAt(ab, 4) : 0xffffffffu, bb ? DwordAt(bb, 4) : 0xffffffffu, "vertex block count");

    // direct exports
    CheckValue(o.getId(a, 0), g.getId(b, 0), "GetVertexTypeID export");
    CheckValue(o.getSize(a, 0), g.getSize(b, 0), "GetVertexSize export");
    for (int type = 0; type < 39; ++type)
        CheckValue(o.getOffset(a, type, 0), g.getOffset(b, type, 0), "GetItemOffset export");
    CheckValue(reinterpret_cast<uintptr_t>(o.getArray(a, 0, 0)) - reinterpret_cast<uintptr_t>(ab),
               reinterpret_cast<uintptr_t>(g.getArray(b, 0, 0)) - reinterpret_cast<uintptr_t>(bb),
               "GetVertexArray export offset");
    CheckValue(reinterpret_cast<uintptr_t>(o.getConstArray(a, 0, 0)) - reinterpret_cast<uintptr_t>(ab),
               reinterpret_cast<uintptr_t>(g.getConstArray(b, 0, 0)) - reinterpret_cast<uintptr_t>(bb),
               "GetVertexArray const export offset");

    // the same queries through the vtables (the exported virtuals)
    CheckValue(CallSlot<int>(a, 0xa8, 0), CallSlot<int>(b, 0xa8, 0), "GetVertexTypeID vtable");
    CheckValue(CallSlot<int>(a, 0xac, 0), CallSlot<int>(b, 0xac, 0), "GetVertexSize vtable");
    CheckValue(CallSlot<int>(a, 0xb4, 0, 0), CallSlot<int>(b, 0xb4, 0, 0), "GetItemStream");
    CheckValue(CallSlot<int>(a, 0x60), CallSlot<int>(b, 0x60), "GetNumStreams");
    for (int type = 0; type < 39; ++type)
        CheckValue(CallSlot<int>(a, 0xb0, type, 0), CallSlot<int>(b, 0xb0, type, 0), "GetItemOffset vtable");
    CheckValue(reinterpret_cast<uintptr_t>(CallSlot<const void*>(a, 0x30, 0, 0)) - reinterpret_cast<uintptr_t>(ab),
               reinterpret_cast<uintptr_t>(CallSlot<const void*>(b, 0x30, 0, 0)) - reinterpret_cast<uintptr_t>(bb),
               "GetVertexArray vtable offset");
    static const unsigned kViewSlots[] = {0xb8, 0xc0, 0xc8, 0xd0, 0xd8};
    for (unsigned slot : kViewSlots) {
        for (int type = 0; type < 39; ++type) {
            const ItemView va = CallSlot<ItemView>(a, slot, type, 0);
            const ItemView vb = CallSlot<ItemView>(b, slot, type, 0);
            CheckValue(va.stride, vb.stride, "item view stride");
            CheckValue(va.data == nullptr, vb.data == nullptr, "item view null");
            if (va.data && vb.data)
                CheckValue(reinterpret_cast<uintptr_t>(va.data) - reinterpret_cast<uintptr_t>(ab),
                           reinterpret_cast<uintptr_t>(vb.data) - reinterpret_cast<uintptr_t>(bb),
                           "item view offset");
        }
    }
}

void GeometryInstance(const char* vertex)
{
    GeoApi o = Geo(0, vertex);
    GeoApi g = Geo(1, vertex);

    // CreateInstance() shares the source's vertex block: both sides start from a default-constructed chunk.
    alignas(8) unsigned char sourceA[0x60], sourceB[0x60];
    std::memset(sourceA, 0xcd, sizeof sourceA);
    std::memset(sourceB, 0xcd, sizeof sourceB);
    o.ctor0(sourceA);
    g.ctor0(sourceB);
    void* const a = o.create0(sourceA);
    void* const b = g.create0(sourceB);
    if (!a || !b) {
        Check(false, "CreateInstance() returned null");
        return;
    }
    CompareGeometryObject(vertex, a, b, o, g);

    // a non-empty copy
    void* const a4 = o.create4(a, 5, 2, 6, 1);
    void* const b4 = g.create4(b, 5, 2, 6, 1);
    if (a4 && b4) CompareGeometryObject(vertex, a4, b4, o, g);

    // Array maintenance (DuplicateVertexArray/ResizeVertexArray) is covered by diff_r3d_u6 through the same
    // vtable; here the factory and the size arithmetic of the two exports are what matters.

    // InitInstance shares the source block
    alignas(8) unsigned char sourceIA[0x60], sourceIB[0x60];
    std::memset(sourceIA, 0xcd, sizeof sourceIA);
    std::memset(sourceIB, 0xcd, sizeof sourceIB);
    o.ctor0(sourceIA);
    g.ctor0(sourceIB);
    void* const ai = o.create0(sourceIA);
    void* const bi = g.create0(sourceIB);
    if (a4 && b4 && ai && bi) {
        o.initInstance(a4, ai);
        g.initInstance(b4, bi);
        CheckValue(DwordAt(PointerAt(ai, 0x5c), 0), DwordAt(PointerAt(bi, 0x5c), 0), "InitInstance block refs");
        CheckValue(DwordAt(ai, 0x24), DwordAt(bi, 0x24), "InitInstance numVertices");
    }

    // constructors and destructors on raw storage
    {
        alignas(8) unsigned char ca[0x60], cb[0x60];
        std::memset(ca, 0xcd, sizeof ca);
        std::memset(cb, 0xcd, sizeof cb);
        o.ctor0(ca);
        g.ctor0(cb);
        static const unsigned kScalars[] = {0x18, 0x1c, 0x20, 0x24, 0x28, 0x38, 0x3c, 0x40, 0x44};
        for (unsigned off : kScalars) CheckValue(DwordAt(ca, off), DwordAt(cb, off), "ctor0 scalar");
        o.dtor(ca);
        g.dtor(cb);

        alignas(8) unsigned char c5a[0x60], c5b[0x60];
        std::memset(c5a, 0xcd, sizeof c5a);
        std::memset(c5b, 0xcd, sizeof c5b);
        o.ctor5(c5a, 3, 1, 2, 0, 0);
        g.ctor5(c5b, 3, 1, 2, 0, 0);
        for (unsigned off : kScalars) CheckValue(DwordAt(c5a, off), DwordAt(c5b, off), "ctor5 scalar");
        CheckValue(DwordAt(PointerAt(c5a, 0x5c), 1), DwordAt(PointerAt(c5b, 0x5c), 1), "ctor5 block count");
        o.dtor(c5a);
        g.dtor(c5b);
    }

    o.dtor(sourceA);
    g.dtor(sourceB);
    o.dtor(sourceIA);
    g.dtor(sourceIB);
    DestroyChunk(a);
    DestroyChunk(b);
    if (a4) DestroyChunk(a4);
    if (b4) DestroyChunk(b4);
    if (ai) DestroyChunk(ai);
    if (bi) DestroyChunk(bi);
}

void GeometryExports()
{
    GeometryInstance("PackedVertexPNTA");
    GeometryInstance("PackedVertexPNTTA");
}

// ---- StdMaterialPluginTemplate<AutoTexMaterial> -------------------------------------------------------------------

#define STDT(s) "?$StdMaterialPluginTemplate@VAutoTexMaterial@Jet@@@Jet@@" s

struct PluginApi {
    using Ctor0 = void(__thiscall*)(void*);
    using CtorNamed = void(__thiscall*)(void*, const void*, const void*);
    using CtorCopy = void(__thiscall*)(void*, const void*);
    using Dtor = void(__thiscall*)(void*);
    using Assign = void*(__thiscall*)(void*, const void*);
    using Create = void*(__cdecl*)(const void*, void*, void*);
    using IsSupported = bool(__cdecl*)(void*, void*, void*);

    Ctor0 ctor0;
    CtorNamed ctorNamed;
    CtorCopy ctorCopy;
    Dtor dtor;
    Assign assign;
    Create create;
    IsSupported isSupported;
    void* vtable;
};

PluginApi Plugin(int side)
{
    PluginApi a{};
    a.ctor0 = Proc<PluginApi::Ctor0>(side, "??0" STDT("QAE@XZ"));
    a.ctorNamed = Proc<PluginApi::CtorNamed>(side, "??0" STDT("QAE@ABVPString@1@ABVUId@1@@Z"));
    a.ctorCopy = Proc<PluginApi::CtorCopy>(side, "??0" STDT("QAE@ABV01@@Z"));
    a.dtor = Proc<PluginApi::Dtor>(side, "??1" STDT("UAE@XZ"));
    a.assign = Proc<PluginApi::Assign>(side, "??4" STDT("QAEAAV01@ABV01@@Z"));
    a.create = Proc<PluginApi::Create>(side, "?CreateInstance@" STDT("UBAPAVMaterial@2@PAVMaterialManager@2@PAVRenderKernel@2@@Z"));
    a.vtable = reinterpret_cast<void*>(GetProcAddress(mod[side], "??_7" STDT("6B@")));
    if (!a.vtable) {
        std::printf("missing StdMaterialPluginTemplate vtable\n");
        std::exit(2);
    }
    a.isSupported = reinterpret_cast<PluginApi::IsSupported>(reinterpret_cast<void**>(a.vtable)[4]);
    return a;
}

void PluginExports()
{
    const PluginApi o = Plugin(0);
    const PluginApi g = Plugin(1);

    // the vtable has the original's seven executable slots; the role of slot 4 is IsSupported
    CheckValue(VtableDataLength(reinterpret_cast<void**>(o.vtable)),
               VtableDataLength(reinterpret_cast<void**>(g.vtable)), "Std plugin vtable length");
    CheckValue(VtableDataLength(reinterpret_cast<void**>(o.vtable)), 7u, "Std plugin vtable length 7");
    unsigned char reason[8] = {};
    CheckValue(o.isSupported(nullptr, nullptr, reason), g.isSupported(nullptr, nullptr, reason),
               "Std plugin IsSupported");

    unsigned char a[0x2c], b[0x2c];
    std::memset(a, 0xcd, sizeof a);
    std::memset(b, 0xcd, sizeof b);
    o.ctor0(a);
    g.ctor0(b);
    CheckBytes(a + 4, b + 4, 0x28, "Std plugin default ctor fields");
    // the three cache pointers are zeroed
    CheckValue(DwordAt(a, 0x20) | DwordAt(a, 0x24) | DwordAt(a, 0x28), 0, "Std plugin caches zeroed");

    // named constructor with the original's AutoTex type string and UID
    const void* const type = reinterpret_cast<const void*>(
        GetProcAddress(mod[1], "?UNDECORATED_TYPE@AutoTexMaterial@Jet@@2VPString@2@B"));
    const void* const uid = reinterpret_cast<const void*>(GetProcAddress(mod[1], "?UID@AutoTexMaterial@Jet@@2VUId@2@B"));
    if (type && uid) {
        unsigned char na[0x2c], nb[0x2c];
        std::memset(na, 0xcd, sizeof na);
        std::memset(nb, 0xcd, sizeof nb);
        o.ctorNamed(na, type, uid);
        g.ctorNamed(nb, type, uid);
        CheckBytes(na + 4, nb + 4, 0x28, "Std plugin named ctor fields");
    }

    // copy constructor and assignment copy the three cache slots
    unsigned char src[0x2c];
    std::memset(src, 0, sizeof src);
    o.ctor0(src);  // a valid base object for the copy/assignment to read
    *reinterpret_cast<void**>(src + 0x20) = reinterpret_cast<void*>(0x11111111u);
    *reinterpret_cast<void**>(src + 0x24) = reinterpret_cast<void*>(0x22222222u);
    *reinterpret_cast<void**>(src + 0x28) = reinterpret_cast<void*>(0x33333333u);
    unsigned char ca[0x2c], cb[0x2c];
    std::memset(ca, 0xcd, sizeof ca);
    std::memset(cb, 0xcd, sizeof cb);
    o.ctorCopy(ca, src);
    g.ctorCopy(cb, src);
    CheckBytes(ca + 0x20, cb + 0x20, 0xc, "Std plugin copy ctor caches");
    CheckValue(DwordAt(ca, 0x20), 0x11111111u, "copy ctor cache0");
    CheckValue(DwordAt(ca, 0x28), 0x33333333u, "copy ctor cache2");

    unsigned char da[0x2c], db[0x2c];
    std::memset(da, 0xcd, sizeof da);
    std::memset(db, 0xcd, sizeof db);
    o.ctor0(da);
    g.ctor0(db);
    CheckValue(reinterpret_cast<uintptr_t>(o.assign(da, src)), reinterpret_cast<uintptr_t>(da), "Std plugin assign");
    CheckValue(reinterpret_cast<uintptr_t>(g.assign(db, src)), reinterpret_cast<uintptr_t>(db), "orig Std plugin assign");
    CheckBytes(da + 0x20, db + 0x20, 0xc, "Std plugin assign caches");

    // destructor with null caches (the assignment copied the sentinel cache pointers above)
    *reinterpret_cast<void**>(da + 0x20) = nullptr;
    *reinterpret_cast<void**>(da + 0x24) = nullptr;
    *reinterpret_cast<void**>(da + 0x28) = nullptr;
    *reinterpret_cast<void**>(db + 0x20) = nullptr;
    *reinterpret_cast<void**>(db + 0x24) = nullptr;
    *reinterpret_cast<void**>(db + 0x28) = nullptr;
    o.dtor(da);
    g.dtor(db);
    CheckValue(DwordAt(da, 0x20), DwordAt(db, 0x20), "Std plugin dtor leaves cache0");

    // The template CreateInstance itself is exported and local (check_exports + the link); its body needs a
    // real MaterialManager/RenderKernel, which this process does not own. The factory path is exercised by U5a2.
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: %s <testdir> [rounds]\n", argv[0]);
        return 2;
    }
    rounds = argc > 2 ? static_cast<unsigned>(std::atoi(argv[2])) : 64;
    srand(0x7e3d7e3d);
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    char dir[MAX_PATH];
    GetFullPathNameA(argv[1], MAX_PATH, dir, nullptr);
    SetDllDirectoryA(dir);
    SetCurrentDirectoryA(dir);

    mod[1] = LoadLibraryA("Render3DCore_7c_orig.dll");
    mod[0] = LoadLibraryA("Render3DCore_7c.dll");
    if (!mod[0] || !mod[1]) {
        std::printf("cannot load the modules in %s (error %lu)\n", dir, GetLastError());
        return 2;
    }
    if (mod[0] == mod[1]) {
        std::printf("the two modules resolved to the same file\n");
        return 2;
    }

    InitVtable();
    InitDisplay();
    LineExports();
    InterpolatorExports();
    TreeExports();
    GeometryExports();
    PluginExports();

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
