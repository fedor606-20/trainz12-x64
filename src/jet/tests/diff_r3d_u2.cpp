// Differential test of Render3DCore_7c workstream U2 (splines): our module against Render3DCore_7c_orig.dll in one
// process. Every constructor, Set, array query and math query is called on identical inputs for both modules; the
// resulting objects, chunk layout and raw float bits (including NaN/inf/-0) are compared. Any difference fails.
//
// usage: diff_r3d_u2.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

struct Vec3 {
    float x, y, z;
};
struct SplineObj {
    void** vt;
    void* head;
};
struct SegmentObj {
    void** vt;
    float f[12];
};
struct DefObj {
    void** vt;
    int type;
    unsigned char closed;
    unsigned char pad[3];
    float tension, bias, continuity;
    unsigned count, capacity;
    Vec3* points;
};
struct Chunk {
    Chunk* next;
    Chunk* prev;
    unsigned count;
    SegmentObj* data[13];
};
static_assert(sizeof(Vec3) == 0xc, "Vec3");
static_assert(sizeof(SplineObj) == 8, "SplineObj");
static_assert(sizeof(SegmentObj) == 0x34, "SegmentObj");
static_assert(sizeof(DefObj) == 0x24, "DefObj");
static_assert(sizeof(Chunk) == 0x40, "Chunk");

static HMODULE mod[2];
static unsigned long long checks, bad;
static std::map<std::string, unsigned long long> badBy;
static std::mt19937 rng(0x5eed5eed);
static HMODULE kstd;
using AllocDeleteFn = void(__cdecl*)(void*);
static AllocDeleteFn allocDelete;

static void FreeChunksOnly(SplineObj* s)
{
    if (!s->head || !allocDelete)
        return;
    Chunk* head = reinterpret_cast<Chunk*>(s->head);
    Chunk* c = head;
    do {
        Chunk* next = c->next;
        allocDelete(c);
        c = next;
    } while (c != head);
    s->head = nullptr;
}

static int Pick(int lo, int hi)
{
    return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1));
}

static float FRange(float lo, float hi)
{
    return std::uniform_real_distribution<float>(lo, hi)(rng);
}

static float Bits(uint32_t u)
{
    float f;
    std::memcpy(&f, &u, sizeof f);
    return f;
}

static float Awkward()
{
    switch (Pick(0, 11)) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: return Bits(0x7f800000u);
    case 3: return Bits(0xff800000u);
    case 4: return Bits(0x7fc00000u);
    case 5: return Bits(0xffc00001u);
    case 6: return Bits(0x7fa00000u);
    default: return FRange(-1000.0f, 1000.0f);
    }
}

static Vec3 RandomVec(bool awkward)
{
    return Vec3{awkward ? Awkward() : FRange(-100.0f, 100.0f), awkward ? Awkward() : FRange(-100.0f, 100.0f),
                awkward ? Awkward() : FRange(-100.0f, 100.0f)};
}

template <class F>
static F Proc(int side, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(mod[side], name));
    if (!p) {
        std::printf("missing export %s in %s\n", name, side ? "orig" : "ours");
        std::exit(2);
    }
    return p;
}

template <class T>
static T MemberProc(int side, const char* name)
{
    FARPROC raw = GetProcAddress(mod[side], name);
    if (!raw) {
        std::printf("missing export %s in %s\n", name, side ? "orig" : "ours");
        std::exit(2);
    }
    T fn{};
    static_assert(sizeof fn == sizeof raw, "member function pointer size");
    std::memcpy(&fn, &raw, sizeof fn);
    return fn;
}

static void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        ++badBy[what];
        if (bad < 60)
            std::printf("MISMATCH %s\n", what);
    }
}

static uint32_t FBits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

static void CheckVec(const Vec3& a, const Vec3& b, const char* what)
{
    Check(std::memcmp(&a, &b, sizeof a) == 0, what);
    if (std::memcmp(&a, &b, sizeof a) != 0 && bad < 60)
        std::printf("  %s ours(%08x %08x %08x) orig(%08x %08x %08x)\n", what, FBits(a.x), FBits(a.y), FBits(a.z),
                    FBits(b.x), FBits(b.y), FBits(b.z));
}

static void CheckFloat(float a, float b, const char* what)
{
    Check(std::memcmp(&a, &b, sizeof a) == 0, what);
    if (std::memcmp(&a, &b, sizeof a) != 0 && bad < 60)
        std::printf("  %s ours(%08x) orig(%08x)\n", what, FBits(a), FBits(b));
}

using RatioValFn = Vec3 (__cdecl SplineObj::*)(float) const;
using PosLenValFn = Vec3 (__cdecl SplineObj::*)(float, unsigned) const;
using SegRatioValFn = Vec3 (__cdecl SegmentObj::*)(float) const;
using SegPosLenValFn = Vec3 (__cdecl SegmentObj::*)(float, float, float, unsigned) const;
using SegPtrValFn = const Vec3* (__cdecl SegmentObj::*)() const;

struct SegApi {
    void(__thiscall* ctor0)(SegmentObj*);
    void(__thiscall* ctorVec)(SegmentObj*, const Vec3*);
    void(__thiscall* ctorCopy)(SegmentObj*, const SegmentObj*);
    SegmentObj* (__thiscall* assign)(SegmentObj*, const SegmentObj*);
    void(__thiscall* dtor)(SegmentObj*);
    void(__cdecl* setVec)(SegmentObj*, const Vec3*);
    void(__cdecl* set4)(SegmentObj*, const Vec3*, const Vec3*, const Vec3*, const Vec3*);
    void(__cdecl* set5)(SegmentObj*, const Vec3*, const Vec3*, const Vec3*, const Vec3*, float);
    void(__cdecl* setTcb)(SegmentObj*, const Vec3*, const Vec3*, const Vec3*, const Vec3*, float, float, float);
    void(__cdecl* posRatioVoid)(const SegmentObj*, float, Vec3*);
    SegRatioValFn posRatioVal;
    void(__cdecl* posLenVoid)(const SegmentObj*, float, Vec3*, float, float, unsigned);
    SegPosLenValFn posLenVal;
    float(__cdecl* length)(const SegmentObj*, float, float, unsigned);
    void(__cdecl* firstDeriv)(const SegmentObj*, float, Vec3*);
    void(__cdecl* secondDeriv)(const SegmentObj*, float, Vec3*);
    void(__cdecl* thirdDeriv)(const SegmentObj*, float, Vec3*);
    void(__cdecl* tangent)(const SegmentObj*, float, Vec3*);
    void(__cdecl* normal)(const SegmentObj*, float, Vec3*);
    void(__cdecl* binormal)(const SegmentObj*, float, Vec3*);
    unsigned(__cdecl* determineSteps)(const SegmentObj*, unsigned, float, float, float);
    bool(__cdecl* isNull)(const SegmentObj*);
    SegPtrValFn segmentConstants;
};

struct SplineApi {
    void(__thiscall* ctor0)(SplineObj*);
    void(__thiscall* ctorCopy)(SplineObj*, const SplineObj*);
    SplineObj* (__thiscall* assign)(SplineObj*, const SplineObj*);
    void(__thiscall* dtor)(SplineObj*);
    void(__cdecl* add)(SplineObj*, SegmentObj*);
    bool(__cdecl* remove)(SplineObj*, const SegmentObj*);
    bool(__cdecl* removeDelete)(SplineObj*, SegmentObj*);
    void(__cdecl* removeDeleteAll)(SplineObj*);
    float(__cdecl* length)(const SplineObj*, unsigned);
    void(__cdecl* posRatioVoid)(const SplineObj*, float, Vec3*);
    void(__cdecl* posLenVoid)(const SplineObj*, float, Vec3*, unsigned);
    RatioValFn posRatioVal;
    PosLenValFn posLenVal;
    const void*(__cdecl* segmentArray)(const SplineObj*);

    // per-type Set entries
    void(__cdecl* setBezier)(SplineObj*, unsigned, const Vec3*);
    void(__cdecl* setHermiteT)(SplineObj*, unsigned, const Vec3*, const Vec3*, int);
    void(__cdecl* setHermiteI)(SplineObj*, unsigned, const Vec3*, int);
    void(__cdecl* setCardinal)(SplineObj*, unsigned, const Vec3*, float, int);
    void(__cdecl* setCardinalArr)(SplineObj*, unsigned, const Vec3*, float*, int);
    void(__cdecl* setBSpline)(SplineObj*, unsigned, const Vec3*, float, int);
    void(__cdecl* setBSplineArr)(SplineObj*, unsigned, const Vec3*, float*, int);
    void(__cdecl* setTCB)(SplineObj*, unsigned, const Vec3*, float, float, float, int);
    void(__cdecl* setTCBArr)(SplineObj*, unsigned, const Vec3*, float*, float*, float*, int);
};

struct DefApi {
    void(__thiscall* ctor0)(DefObj*);
    void(__thiscall* ctorCopy)(DefObj*, const DefObj*);
    DefObj* (__thiscall* assign)(DefObj*, const DefObj*);
    void(__thiscall* dtor)(DefObj*);
    SplineObj* (__cdecl* createSpline)(DefObj*);
    void(__cdecl* setType)(DefObj*, int);
    int(__cdecl* getType)(const DefObj*);
    void(__cdecl* setParams)(DefObj*, bool, float, float, float);
    void(__cdecl* getParams)(const DefObj*, bool*, float*, float*, float*);
    void(__cdecl* addPoint)(DefObj*, const Vec3*);
    unsigned(__cdecl* numPoints)(const DefObj*);
    void(__cdecl* removeAll)(DefObj*);
    Vec3*(__cdecl* points)(DefObj*);
    const Vec3*(__cdecl* pointsC)(const DefObj*);
};

static SegApi LoadSeg(int side)
{
    SegApi a{};
    const char* p = "@Jet@@";
    (void)p;
    a.ctor0 = Proc<decltype(a.ctor0)>(side, "??0SplineSegment@Jet@@QAE@XZ");
    a.ctorVec = Proc<decltype(a.ctorVec)>(side, "??0SplineSegment@Jet@@QAE@ABVVector3@1@@Z");
    a.ctorCopy = Proc<decltype(a.ctorCopy)>(side, "??0SplineSegment@Jet@@QAE@ABV01@@Z");
    a.assign = Proc<decltype(a.assign)>(side, "??4SplineSegment@Jet@@QAEAAV01@ABV01@@Z");
    a.dtor = Proc<decltype(a.dtor)>(side, "??1SplineSegment@Jet@@UAE@XZ");
    a.setVec = Proc<decltype(a.setVec)>(side, "?Set@SplineSegment@Jet@@QAAXABVVector3@2@@Z");
    a.set4 = Proc<decltype(a.set4)>(side, "?Set@BezierSplineSegment@Jet@@QAAXABVVector3@2@000@Z");
    a.set5 = Proc<decltype(a.set5)>(side, "?Set@CardinalSplineSegment@Jet@@QAAXABVVector3@2@000M@Z");
    a.setTcb = Proc<decltype(a.setTcb)>(side, "?Set@TCBSplineSegment@Jet@@QAAXABVVector3@2@000MMM@Z");
    a.posRatioVoid = Proc<decltype(a.posRatioVoid)>(side, "?GetPositionByRatio@SplineSegment@Jet@@QBAXMAAVVector3@2@@Z");
    a.posRatioVal = MemberProc<SegRatioValFn>(side, "?GetPositionByRatio@SplineSegment@Jet@@QBA?AVVector3@2@M@Z");
    a.posLenVoid = Proc<decltype(a.posLenVoid)>(side,
                                                "?GetPositionByLength@SplineSegment@Jet@@UBAXMAAVVector3@2@MMI@Z");
    a.posLenVal = MemberProc<SegPosLenValFn>(side, "?GetPositionByLength@SplineSegment@Jet@@QBA?AVVector3@2@MMMI@Z");
    a.length = Proc<decltype(a.length)>(side, "?GetLength@SplineSegment@Jet@@UBAMMMI@Z");
    a.firstDeriv = Proc<decltype(a.firstDeriv)>(side, "?GetFirstDerivative@SplineSegment@Jet@@QBAXMAAVVector3@2@@Z");
    a.secondDeriv = Proc<decltype(a.secondDeriv)>(side,
                                                  "?GetSecondDerivative@SplineSegment@Jet@@QBAXMAAVVector3@2@@Z");
    a.thirdDeriv = Proc<decltype(a.thirdDeriv)>(side, "?GetThirdDerivative@SplineSegment@Jet@@QBAXMAAVVector3@2@@Z");
    a.tangent = Proc<decltype(a.tangent)>(side, "?GetTangent@SplineSegment@Jet@@QBAXMAAVVector3@2@@Z");
    a.normal = Proc<decltype(a.normal)>(side, "?GetNormal@SplineSegment@Jet@@QBAXMAAVVector3@2@@Z");
    a.binormal = Proc<decltype(a.binormal)>(side, "?GetBinormal@SplineSegment@Jet@@QBAXMAAVVector3@2@@Z");
    a.determineSteps = Proc<decltype(a.determineSteps)>(side,
                                                        "?DetermineNumSplineSteps@SplineSegment@Jet@@QBAIIMMM@Z");
    a.isNull = Proc<decltype(a.isNull)>(side, "?IsNullSplineSegment@SplineSegment@Jet@@QBA_NXZ");
    a.segmentConstants = MemberProc<SegPtrValFn>(side, "?GetSegmentConstants@SplineSegment@Jet@@QBAPBVVector3@2@XZ");
    return a;
}

static SplineApi LoadSpline(int side)
{
    SplineApi a{};
    a.ctor0 = Proc<decltype(a.ctor0)>(side, "??0Spline@Jet@@QAE@XZ");
    a.ctorCopy = Proc<decltype(a.ctorCopy)>(side, "??0Spline@Jet@@QAE@ABV01@@Z");
    a.assign = Proc<decltype(a.assign)>(side, "??4Spline@Jet@@QAEAAV01@ABV01@@Z");
    a.dtor = Proc<decltype(a.dtor)>(side, "??1Spline@Jet@@UAE@XZ");
    a.add = Proc<decltype(a.add)>(side, "?Add@Spline@Jet@@QAAXPAVSplineSegment@2@@Z");
    a.remove = Proc<decltype(a.remove)>(side, "?Remove@Spline@Jet@@QAA_NPBVSplineSegment@2@@Z");
    a.removeDelete = Proc<decltype(a.removeDelete)>(side, "?RemoveDelete@Spline@Jet@@QAA_NPAVSplineSegment@2@@Z");
    a.removeDeleteAll = Proc<decltype(a.removeDeleteAll)>(side, "?RemoveDeleteAll@Spline@Jet@@QAAXXZ");
    a.length = Proc<decltype(a.length)>(side, "?GetLength@Spline@Jet@@QBAMI@Z");
    a.posRatioVoid = Proc<decltype(a.posRatioVoid)>(side, "?GetPositionByRatio@Spline@Jet@@UBAXMAAVVector3@2@@Z");
    a.posLenVoid = Proc<decltype(a.posLenVoid)>(side, "?GetPositionByLength@Spline@Jet@@UBAXMAAVVector3@2@I@Z");
    a.posRatioVal = MemberProc<RatioValFn>(side, "?GetPositionByRatio@Spline@Jet@@QBA?AVVector3@2@M@Z");
    a.posLenVal = MemberProc<PosLenValFn>(side, "?GetPositionByLength@Spline@Jet@@QBA?AVVector3@2@MI@Z");
    a.segmentArray = Proc<decltype(a.segmentArray)>(side,
        "?GetSplineSegmentArray@Spline@Jet@@QBAABV?$FastDynamicArray@PAVSplineSegment@Jet@@$0N@U?$Less@PAVSplineSegment@Jet@@PAV12@@2@@2@XZ");

    a.setBezier = Proc<decltype(a.setBezier)>(side, "?Set@BezierSpline@Jet@@QAAXIQBVVector3@2@@Z");
    a.setHermiteT = Proc<decltype(a.setHermiteT)>(side,
        "?Set@HermiteSpline@Jet@@QAAXIQBVVector3@2@0_N@Z");
    a.setHermiteI = Proc<decltype(a.setHermiteI)>(side, "?Set@HermiteSpline@Jet@@QAAXIQBVVector3@2@_N@Z");
    a.setCardinal = Proc<decltype(a.setCardinal)>(side,
        "?Set@CardinalSpline@Jet@@QAAXIQBVVector3@2@M_N@Z");
    a.setCardinalArr = Proc<decltype(a.setCardinalArr)>(side,
        "?Set@CardinalSpline@Jet@@QAAXIQBVVector3@2@QAM_N@Z");
    a.setBSpline = Proc<decltype(a.setBSpline)>(side, "?Set@BSpline@Jet@@QAAXIQBVVector3@2@M_N@Z");
    a.setBSplineArr = Proc<decltype(a.setBSplineArr)>(side, "?Set@BSpline@Jet@@QAAXIQBVVector3@2@QAM_N@Z");
    a.setTCB = Proc<decltype(a.setTCB)>(side, "?Set@TCBSpline@Jet@@QAAXIQBVVector3@2@MMM_N@Z");
    a.setTCBArr = Proc<decltype(a.setTCBArr)>(side, "?Set@TCBSpline@Jet@@QAAXIQBVVector3@2@QAM11_N@Z");
    return a;
}

static DefApi LoadDef(int side)
{
    DefApi a{};
    a.ctor0 = Proc<decltype(a.ctor0)>(side, "??0SplineDefinition@Jet@@QAE@XZ");
    a.ctorCopy = Proc<decltype(a.ctorCopy)>(side, "??0SplineDefinition@Jet@@QAE@ABV01@@Z");
    a.assign = Proc<decltype(a.assign)>(side, "??4SplineDefinition@Jet@@QAEAAV01@ABV01@@Z");
    a.dtor = Proc<decltype(a.dtor)>(side, "??1SplineDefinition@Jet@@UAE@XZ");
    a.createSpline = Proc<decltype(a.createSpline)>(side, "?CreateSpline@SplineDefinition@Jet@@UAAPAVSpline@2@XZ");
    a.setType = Proc<decltype(a.setType)>(side, "?SetType@SplineDefinition@Jet@@QAAXW4SplineType@12@@Z");
    a.getType = Proc<decltype(a.getType)>(side, "?GetType@SplineDefinition@Jet@@QBA?AW4SplineType@12@XZ");
    a.setParams = Proc<decltype(a.setParams)>(side, "?SetParameters@SplineDefinition@Jet@@QAAX_NMMM@Z");
    a.getParams = Proc<decltype(a.getParams)>(side, "?GetParameters@SplineDefinition@Jet@@QBAXAA_NAAM11@Z");
    a.addPoint = Proc<decltype(a.addPoint)>(side, "?AddPoint@SplineDefinition@Jet@@QAAXABVVector3@2@@Z");
    a.numPoints = Proc<decltype(a.numPoints)>(side, "?GetNumPoints@SplineDefinition@Jet@@QBAIXZ");
    a.removeAll = Proc<decltype(a.removeAll)>(side, "?RemoveAllPoints@SplineDefinition@Jet@@QAAXXZ");
    a.points = Proc<decltype(a.points)>(side, "?GetPointArray@SplineDefinition@Jet@@QAAPAVVector3@2@XZ");
    a.pointsC = Proc<decltype(a.pointsC)>(side, "?GetPointArray@SplineDefinition@Jet@@QBAPBVVector3@2@XZ");
    return a;
}

struct Snapshot {
    std::vector<SegmentObj*> segs;
    std::vector<unsigned> chunks;
};

static Snapshot Snap(const SplineObj* s)
{
    Snapshot snap;
    Chunk* head = reinterpret_cast<Chunk*>(s->head);
    if (!head)
        return snap;
    Chunk* c = head;
    do {
        snap.chunks.push_back(c->count);
        for (unsigned i = 0; i < c->count; ++i)
            snap.segs.push_back(c->data[i]);
        c = c->next;
    } while (c != head && snap.chunks.size() < 4096);
    return snap;
}

static void CheckSnap(const SplineObj* ours, const SplineObj* orig, const char* what)
{
    const Snapshot a = Snap(ours), b = Snap(orig);
    Check(a.segs.size() == b.segs.size(), what);
    Check(a.chunks == b.chunks, what);
    if (a.segs.size() != b.segs.size()) {
        if (bad < 30)
            std::printf("  %s count ours %zu orig %zu\n", what, a.segs.size(), b.segs.size());
        return;
    }
    for (size_t i = 0; i < a.segs.size(); ++i) {
        const SegmentObj* sa = a.segs[i];
        const SegmentObj* sb = b.segs[i];
        if ((sa == nullptr) != (sb == nullptr)) {
            Check(false, what);
            return;
        }
        if (sa && std::memcmp(sa->f, sb->f, sizeof sa->f) != 0) {
            ++checks;
            ++bad;
            if (bad < 30) {
                std::printf("  %s segment %zu differs\n    ours", what, i);
                for (float f : sa->f)
                    std::printf(" %08x", FBits(f));
                std::printf("\n    orig");
                for (float f : sb->f)
                    std::printf(" %08x", FBits(f));
                std::printf("\n");
            }
        } else {
            ++checks;
        }
    }
}

static void CheckDef(const DefObj* a, const DefObj* b, const char* what)
{
    Check(a->type == b->type, what);
    Check(a->closed == b->closed, what);
    CheckFloat(a->tension, b->tension, what);
    CheckFloat(a->bias, b->bias, what);
    CheckFloat(a->continuity, b->continuity, what);
    Check(a->count == b->count, what);
    Check(a->capacity == b->capacity, what);
    if (a->count == b->count && a->points && b->points)
        Check(std::memcmp(a->points, b->points, a->count * sizeof(Vec3)) == 0, what);
}

// delete a heap spline through its virtual deleting destructor (slot 0, flag 1)
static void DestroySpline(SplineObj* s)
{
    if (!s)
        return;
    using DelFn = void(__thiscall*)(void*, unsigned);
    DelFn del = *reinterpret_cast<DelFn*>(s->vt);
    del(s, 1);
}

static void ExerciseSegments(const SegApi& ours, const SegApi& orig, SegmentObj* a, SegmentObj* b)
{
    Check(std::memcmp(a->f, b->f, sizeof a->f) == 0, "segment coefficients");
    const float ts[] = {0.0f, 0.25f, 0.5f, 1.0f, 2.0f, -1.0f, Bits(0x7fc00000u), Bits(0x7f800000u)};
    for (float t : ts) {
        Vec3 va{}, vb{};
        ours.posRatioVoid(a, t, &va);
        orig.posRatioVoid(b, t, &vb);
        CheckVec(va, vb, "segment GetPositionByRatio");
        CheckVec((a->*ours.posRatioVal)(t), (b->*orig.posRatioVal)(t), "segment GetPositionByRatio value");

        ours.firstDeriv(a, t, &va);
        orig.firstDeriv(b, t, &vb);
        CheckVec(va, vb, "GetFirstDerivative");
        ours.secondDeriv(a, t, &va);
        orig.secondDeriv(b, t, &vb);
        CheckVec(va, vb, "GetSecondDerivative");
        ours.thirdDeriv(a, t, &va);
        orig.thirdDeriv(b, t, &vb);
        CheckVec(va, vb, "GetThirdDerivative");
        ours.tangent(a, t, &va);
        orig.tangent(b, t, &vb);
        CheckVec(va, vb, "GetTangent");
        ours.normal(a, t, &va);
        orig.normal(b, t, &vb);
        CheckVec(va, vb, "GetNormal");
        ours.binormal(a, t, &va);
        orig.binormal(b, t, &vb);
        CheckVec(va, vb, "GetBinormal");

        for (unsigned steps : {0u, 1u, 4u, 16u}) {
            CheckFloat(ours.length(a, t, t + 1.0f, steps), orig.length(b, t, t + 1.0f, steps), "segment GetLength");
            ours.posLenVoid(a, 0.5f, &va, t, t + 1.0f, steps);
            orig.posLenVoid(b, 0.5f, &vb, t, t + 1.0f, steps);
            CheckVec(va, vb, "segment GetPositionByLength");
            CheckVec((a->*ours.posLenVal)(0.5f, t, t + 1.0f, steps),
                     (b->*orig.posLenVal)(0.5f, t, t + 1.0f, steps), "segment GetPositionByLength value");
        }
    }
    Check(ours.determineSteps(a, 8, 0.001f, 0.0f, 1.0f) == orig.determineSteps(b, 8, 0.001f, 0.0f, 1.0f),
          "DetermineNumSplineSteps");
    Check(ours.isNull(a) == orig.isNull(b), "IsNullSplineSegment");
    Check(std::memcmp((a->*ours.segmentConstants)(), (b->*orig.segmentConstants)(), 12 * sizeof(float)) == 0,
          "GetSegmentConstants");
}

static void ExerciseSplineQueries(const SplineApi& api, SplineObj* a, SplineObj* b)
{
    const Snapshot sa = Snap(a), sb = Snap(b);
    if (sa.segs.empty() || sb.segs.empty())
        return;
    const float total = static_cast<float>(sa.segs.size());
    for (unsigned steps : {0u, 1u, 5u, 17u})
        CheckFloat(api.length(a, steps), api.length(b, steps), "Spline GetLength");

    const float ratios[] = {0.0f, 0.5f, 1.0f, total - 0.5f, total, total + 3.0f, -1.0f, Bits(0x7fc00000u),
                            Bits(0x7f800000u), Bits(0xff800000u)};
    for (float r : ratios) {
        Vec3 va{}, vb{};
        api.posRatioVoid(a, r, &va);
        api.posRatioVoid(b, r, &vb);
        CheckVec(va, vb, "Spline GetPositionByRatio");
        CheckVec((a->*api.posRatioVal)(r), (b->*api.posRatioVal)(r), "Spline GetPositionByRatio value");
    }

    const float lengths[] = {0.0f, 0.3f, 1.7f, 1.0e30f, Bits(0x7fc00000u)};
    for (unsigned steps : {1u, 4u, 16u}) {
        for (float l : lengths) {
            Vec3 va{}, vb{};
            api.posLenVoid(a, l, &va, steps);
            api.posLenVoid(b, l, &vb, steps);
            CheckVec(va, vb, "Spline GetPositionByLength");
            CheckVec((a->*api.posLenVal)(l, steps), (b->*api.posLenVal)(l, steps),
                     "Spline GetPositionByLength value");
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u2 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 500;
    SetDllDirectoryA(argv[1]);
    const std::string dir = argv[1];
    LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str());
    mod[0] = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    mod[1] = GetModuleHandleA("Render3DCore_7c_orig.dll");
    if (!mod[0] || !mod[1]) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    kstd = GetModuleHandleA("KernelStandard_7c.dll");
    if (!kstd)
        kstd = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (kstd)
        allocDelete = reinterpret_cast<AllocDeleteFn>(GetProcAddress(kstd, "?Delete@Alloc@Jet@@SAXPAX@Z"));

    SegApi seg[2] = {LoadSeg(0), LoadSeg(1)};
    SplineApi spl[2] = {LoadSpline(0), LoadSpline(1)};
    DefApi def[2] = {LoadDef(0), LoadDef(1)};

    for (int round = 0; round < rounds; ++round) {
        const bool awkward = (round % 7) == 0;
        Vec3 pts[20];
        for (Vec3& p : pts)
            p = RandomVec(awkward);
        if ((round % 3) == 0) {
            for (int i = 1; i < 12; i += 3)
                pts[i] = pts[0];  // duplicate control points
        }
        Vec3 tangents[20];
        float tensions[20], biases[20], continuities[20];
        for (int i = 0; i < 20; ++i) {
            tangents[i] = RandomVec(awkward);
            tensions[i] = awkward ? Awkward() : FRange(-3.0f, 3.0f);
            biases[i] = awkward ? Awkward() : FRange(-2.0f, 2.0f);
            continuities[i] = awkward ? Awkward() : FRange(-2.0f, 2.0f);
        }
        const float tension = awkward ? Awkward() : FRange(-3.0f, 3.0f);
        const float bias = awkward ? Awkward() : FRange(-2.0f, 2.0f);
        const float continuity = awkward ? Awkward() : FRange(-2.0f, 2.0f);

        // ---- spline construction and array layout, every type and several counts ----
        static const unsigned counts[] = {0, 1, 2, 3, 4, 5, 8, 14, 16};
        for (int closed = 0; closed < 2; ++closed) {
            for (unsigned n : counts) {
                SplineObj objs[2];
                for (int side = 0; side < 2; ++side) {
                    std::memset(&objs[side], 0xcd, sizeof(SplineObj));
                    spl[side].ctor0(&objs[side]);
                }

                for (int type = 0; type < 6; ++type) {
                    for (int side = 0; side < 2; ++side) {
                        SplineObj* o = &objs[side];
                        switch (type) {
                        case 0: spl[side].setHermiteI(o, n, pts, closed); break;
                        case 1: spl[side].setCardinal(o, n, pts, tension, closed); break;
                        case 2: spl[side].setTCB(o, n, pts, tension, bias, continuity, closed); break;
                        case 3: spl[side].setBezier(o, n, pts); break;
                        case 4: spl[side].setBSpline(o, n, pts, tension, closed); break;
                        default: spl[side].setHermiteT(o, n, pts, tangents, closed); break;
                        }
                    }
                    CheckSnap(&objs[0], &objs[1], [&] {
                        static char msg[96];
                        std::snprintf(msg, sizeof msg, "Spline Set type %d n %u closed %d", type, n, closed);
                        return msg;
                    }());

                    // array-parameter overload for the classes that have one
                    if (type == 1 || type == 2 || type == 4) {
                        for (int side = 0; side < 2; ++side) {
                            SplineObj* o = &objs[side];
                            if (type == 1)
                                spl[side].setCardinalArr(o, n, pts, tensions, closed);
                            else if (type == 2)
                                spl[side].setTCBArr(o, n, pts, tensions, biases, continuities, closed);
                            else
                                spl[side].setBSplineArr(o, n, pts, tensions, closed);
                        }
                        CheckSnap(&objs[0], &objs[1], [&] {
                            static char msg[96];
                            std::snprintf(msg, sizeof msg, "Spline Set array type %d n %u closed %d", type, n, closed);
                            return msg;
                        }());
                    }
                }

                // apply one type again for the value queries and per-segment methods
                const int type = round % 6;
                for (int side = 0; side < 2; ++side) {
                    SplineObj* o = &objs[side];
                    switch (type) {
                    case 0: spl[side].setHermiteI(o, n, pts, closed); break;
                    case 1: spl[side].setCardinal(o, n, pts, tension, closed); break;
                    case 2: spl[side].setTCB(o, n, pts, tension, bias, continuity, closed); break;
                    case 3: spl[side].setBezier(o, n, pts); break;
                    case 4: spl[side].setBSpline(o, n, pts, tension, closed); break;
                    default: spl[side].setHermiteT(o, n, pts, tangents, closed); break;
                    }
                }
                ExerciseSplineQueries(spl[0], &objs[0], &objs[1]);
                const Snapshot s0 = Snap(&objs[0]), s1 = Snap(&objs[1]);
                for (size_t i = 0; i < s0.segs.size() && i < s1.segs.size(); ++i)
                    ExerciseSegments(seg[0], seg[1], s0.segs[i], s1.segs[i]);

                // copy constructor and assignment
                SplineObj copies[2], assigned[2];
                for (int side = 0; side < 2; ++side) {
                    spl[side].ctorCopy(&copies[side], &objs[side]);
                    spl[side].ctor0(&assigned[side]);
                    spl[side].assign(&assigned[side], &objs[side]);
                }
                CheckSnap(&copies[0], &copies[1], "Spline copy ctor");
                CheckSnap(&assigned[0], &assigned[1], "Spline assignment");
                // the copy shares the source segment pointers
                Check(Snap(&copies[0]).segs == Snap(&objs[0]).segs, "Spline copy shares segments");
                Check(Snap(&assigned[0]).segs == Snap(&objs[0]).segs, "Spline assignment shares segments");

                // Remove / Add on the shallow copies (the segments still belong to objs; free the copy's chunks
                // without deleting any segment, since the original's copy dtor would double-delete them).
                if (!s0.segs.empty()) {
                    SegmentObj* first0 = s0.segs.front();
                    SegmentObj* first1 = s1.segs.front();
                    Check(spl[0].remove(&copies[0], first0) == spl[1].remove(&copies[1], first1), "Spline Remove");
                    CheckSnap(&copies[0], &copies[1], "Spline Remove layout");
                    Check(spl[0].remove(&copies[0], first0) == spl[1].remove(&copies[1], first1),
                          "Spline Remove twice");
                    spl[0].add(&copies[0], first0);
                    spl[1].add(&copies[1], first1);
                    CheckSnap(&copies[0], &copies[1], "Spline Add layout");
                    spl[0].add(&copies[0], first0);
                    spl[1].add(&copies[1], first1);
                    CheckSnap(&copies[0], &copies[1], "Spline Add duplicate layout");
                }
                FreeChunksOnly(&copies[0]);
                FreeChunksOnly(&copies[1]);
                FreeChunksOnly(&assigned[0]);
                FreeChunksOnly(&assigned[1]);

                // RemoveDelete needs a spline whose segments are not shared with another live owner.
                {
                    SplineObj del[2];
                    for (int side = 0; side < 2; ++side) {
                        spl[side].ctor0(&del[side]);
                        spl[side].setCardinal(&del[side], 4, pts, tension, closed ? 1 : 0);
                    }
                    const Snapshot d0 = Snap(&del[0]), d1 = Snap(&del[1]);
                    if (d0.segs.size() == d1.segs.size() && !d0.segs.empty()) {
                        Check(spl[0].removeDelete(&del[0], d0.segs[1]) ==
                                  spl[1].removeDelete(&del[1], d1.segs[1]),
                              "Spline RemoveDelete");
                        CheckSnap(&del[0], &del[1], "Spline RemoveDelete layout");
                    }
                    for (int side = 0; side < 2; ++side)
                        spl[side].dtor(&del[side]);
                }

                for (int side = 0; side < 2; ++side) {
                    spl[side].dtor(&objs[side]);
                }
            }
        }

        // ---- SplineSegment constructors, copy/assign, Set and math ----
        {
            const Vec3 v = RandomVec(awkward);
            Vec3 c[4] = {RandomVec(awkward), RandomVec(awkward), RandomVec(awkward), RandomVec(awkward)};
            SegmentObj a[2][5];
            for (int side = 0; side < 2; ++side) {
                seg[side].ctor0(&a[side][0]);
                seg[side].ctorVec(&a[side][1], &v);
                seg[side].ctorCopy(&a[side][2], &a[side][1]);
                seg[side].ctor0(&a[side][3]);
                seg[side].assign(&a[side][3], &a[side][1]);
                seg[side].ctor0(&a[side][4]);
                seg[side].setVec(&a[side][4], &v);
            }
            for (int k = 0; k < 5; ++k)
                Check(std::memcmp(a[0][k].f, a[1][k].f, sizeof a[0][k].f) == 0, "SplineSegment ctor/assign");

            SegmentObj b[2][4];
            for (int side = 0; side < 2; ++side) {
                seg[side].ctor0(&b[side][0]);
                seg[side].set4(&b[side][0], &c[0], &c[1], &c[2], &c[3]);
                seg[side].ctor0(&b[side][1]);
                seg[side].set5(&b[side][1], &c[0], &c[1], &c[2], &c[3], tension);
                seg[side].ctor0(&b[side][2]);
                seg[side].setTcb(&b[side][2], &c[0], &c[1], &c[2], &c[3], tension, bias, continuity);
                seg[side].ctor0(&b[side][3]);
                seg[side].set4(&b[side][3], &c[0], &c[1], &c[2], &c[3]);
            }
            for (int k = 0; k < 4; ++k) {
                Check(std::memcmp(b[0][k].f, b[1][k].f, sizeof b[0][k].f) == 0, "SplineSegment Set");
                ExerciseSegments(seg[0], seg[1], &b[0][k], &b[1][k]);
            }
            for (int side = 0; side < 2; ++side)
                for (int k = 0; k < 5; ++k)
                    seg[side].dtor(&a[side][k]);
        }

        // ---- SplineDefinition ----
        {
            DefObj d[2], e[2], f[2];
            for (int side = 0; side < 2; ++side) {
                std::memset(&d[side], 0xcd, sizeof(DefObj));
                def[side].ctor0(&d[side]);
            }
            CheckDef(&d[0], &d[1], "SplineDefinition default");
            const int type = Pick(0, 5);
            const bool closed = Pick(0, 1) != 0;
            const float t = awkward ? Awkward() : FRange(-3.0f, 3.0f);
            const float bb = awkward ? Awkward() : FRange(-2.0f, 2.0f);
            const float cc = awkward ? Awkward() : FRange(-2.0f, 2.0f);
            for (int side = 0; side < 2; ++side) {
                def[side].setType(&d[side], type);
                def[side].setParams(&d[side], closed, t, bb, cc);
            }
            Check(def[0].getType(&d[0]) == def[1].getType(&d[1]), "SplineDefinition GetType");
            bool c0 = false, c1 = false;
            float t0 = 0, t1 = 0, b0 = 0, b1 = 0, k0 = 0, k1 = 0;
            def[0].getParams(&d[0], &c0, &t0, &b0, &k0);
            def[1].getParams(&d[1], &c1, &t1, &b1, &k1);
            Check(c0 == c1, "GetParameters closed");
            CheckFloat(t0, t1, "GetParameters tension");
            CheckFloat(b0, b1, "GetParameters bias");
            CheckFloat(k0, k1, "GetParameters continuity");
            CheckDef(&d[0], &d[1], "SplineDefinition parameters");

            const unsigned addCount = 2 + static_cast<unsigned>(Pick(0, 140));
            for (unsigned i = 0; i < addCount; ++i) {
                const Vec3 p = RandomVec(awkward);
                def[0].addPoint(&d[0], &p);
                def[1].addPoint(&d[1], &p);
            }
            Check(def[0].numPoints(&d[0]) == def[1].numPoints(&d[1]), "GetNumPoints");
            CheckDef(&d[0], &d[1], "SplineDefinition AddPoint");
            Check(std::memcmp(def[0].points(&d[0]), def[1].points(&d[1]),
                              d[0].count * sizeof(Vec3)) == 0, "GetPointArray");
            Check(def[0].pointsC(&d[0]) == def[0].points(&d[0]), "GetPointArray const/non-const");

            for (int side = 0; side < 2; ++side) {
                def[side].ctorCopy(&e[side], &d[side]);
                def[side].ctor0(&f[side]);
                def[side].assign(&f[side], &d[side]);
            }
            CheckDef(&e[0], &e[1], "SplineDefinition copy ctor");
            CheckDef(&f[0], &f[1], "SplineDefinition assignment");

            // CreateSpline: compare the produced segment arrays and delete them
            SplineObj* s0 = def[0].createSpline(&d[0]);
            SplineObj* s1 = def[1].createSpline(&d[1]);
            Check((s0 == nullptr) == (s1 == nullptr), "CreateSpline null");
            if (s0 && s1)
                CheckSnap(s0, s1, "CreateSpline layout");
            DestroySpline(s0);
            DestroySpline(s1);

            for (int side = 0; side < 2; ++side) {
                def[side].removeAll(&d[side]);
                def[side].dtor(&f[side]);
                def[side].dtor(&e[side]);
                def[side].dtor(&d[side]);
            }
        }
    }

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    for (const auto& kv : badBy)
        std::printf("  %-48s %llu\n", kv.first.c_str(), kv.second);
    return bad ? 1 : 0;
}
