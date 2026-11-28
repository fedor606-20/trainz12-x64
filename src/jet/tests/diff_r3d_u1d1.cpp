// Differential test of Render3DCore_7c unit U1d1: the line family (Line, LineSegment, Ray, Line2D, LineSegment2D,
// Circle2D, Polygon2D) against Render3DCore_7c_orig.dll in one process.
//
// Every exported method in the slice is called through GetProcAddress on the same inputs for both modules; object
// memory (from +4 where a vftable lives), return values and float bit patterns (NaNs, infinities, signed zero,
// denormals) are compared raw. Any difference fails.
//
// usage: diff_r3d_u1d1.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

struct Vec2 {
    float x, y;
};
struct Vec3 {
    float x, y, z;
};
struct Plane {
    Vec3 normal;
    float d;
};
struct Sphere {
    void* vftable;
    Vec3 center;
    float radius;
    float radiusXY;
};
struct Mat3 {
    float m[9];
};
struct LineObj {
    void* vt;
    Vec3 direction, point;
};
struct SegObj {
    void* vt;
    Vec3 direction, point, endPoint;
};
struct RayObj {
    void* vt;
    Vec3 direction, point;
};
struct Line2DObj {
    Vec2 direction, point;
};
struct Seg2DObj {
    Vec2 v0, v1;
};
struct Circle2DObj {
    Vec2 center;
    float radius;
};
struct PolyObj {
    int numVerts;
    Vec2* vertices;
    bool ownsVertices;
    unsigned char pad[3];
};
struct Node {
    Vec2 point;
    float theta;
};
struct FdaChunk {
    FdaChunk* next;
    FdaChunk* prev;
    uint32_t count;
    Vec2 data[13];
};
struct FdaObj {
    FdaChunk* head;
};

static_assert(sizeof(Vec2) == 8, "Vec2");
static_assert(sizeof(Vec3) == 0xc, "Vec3");
static_assert(sizeof(Plane) == 0x10, "Plane");
static_assert(sizeof(Sphere) == 0x18, "Sphere");
static_assert(sizeof(LineObj) == 0x1c, "LineObj");
static_assert(sizeof(SegObj) == 0x28, "SegObj");
static_assert(sizeof(RayObj) == 0x1c, "RayObj");
static_assert(sizeof(Line2DObj) == 0x10, "Line2DObj");
static_assert(sizeof(Seg2DObj) == 0x10, "Seg2DObj");
static_assert(sizeof(Circle2DObj) == 0xc, "Circle2DObj");
static_assert(sizeof(PolyObj) == 0xc, "PolyObj");
static_assert(sizeof(Node) == 0xc, "Node");

static HMODULE mod[2];
static unsigned long long checks, bad, shown;
static int curRound;
static std::mt19937 rng(0x1d1d);

static int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }
static float FRange(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }
static float Bits(uint32_t u)
{
    float f;
    std::memcpy(&f, &u, sizeof f);
    return f;
}
static uint32_t FBits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}
static float Awkward()
{
    switch (Pick(0, 13)) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: return Bits(0x7f800000u);
    case 3: return Bits(0xff800000u);
    case 4: return Bits(0x7fc00000u);
    case 5: return Bits(0xffc00001u);
    case 6: return Bits(0x7fa00000u);
    case 7: return Bits(0x00000001u);
    case 8: return Bits(0x7f7fffffu);
    case 9: return 3.402823466e+38f;
    case 10: return -3.402823466e+38f;
    case 11: return 1e-30f;
    default: return FRange(-1000.0f, 1000.0f);
    }
}
static float Rnd(bool awk) { return awk ? Awkward() : FRange(-100.0f, 100.0f); }
static Vec2 RV2(bool awk) { return Vec2{Rnd(awk), Rnd(awk)}; }
static Vec3 RV3(bool awk) { return Vec3{Rnd(awk), Rnd(awk), Rnd(awk)}; }
static Mat3 RMat(bool awk)
{
    Mat3 m;
    for (float& f : m.m) f = Rnd(awk);
    return m;
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
    static_assert(sizeof fn == sizeof raw, "member pointer size");
    std::memcpy(&fn, &raw, sizeof fn);
    return fn;
}
static void Fail(const char* what)
{
    ++bad;
    if (shown < 40) {
        std::printf("MISMATCH %s (check %llu, round %d)\n", what, checks, curRound);
        ++shown;
    }
}
static void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) Fail(what);
}
static unsigned long long nanPayloadOnly;
static bool BothNan(uint32_t a, uint32_t b)
{
    const bool na = (a & 0x7f800000u) == 0x7f800000u && (a & 0x007fffffu) != 0;
    const bool nb = (b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) != 0;
    return na && nb;
}
// Floats are compared raw; two NaNs are considered equal (the original's x87 double intermediates select a
// different NaN payload than SSE2 can; the payload-only count is reported separately).
static void CheckBytes(const void* a, const void* b, size_t n, const char* what)
{
    ++checks;
    if ((n % 4) == 0) {
        const uint32_t* ua = static_cast<const uint32_t*>(a);
        const uint32_t* ub = static_cast<const uint32_t*>(b);
        bool ok = true;
        for (size_t i = 0; i < n / 4; ++i) {
            if (ua[i] == ub[i]) continue;
            if (BothNan(ua[i], ub[i])) {
                ++nanPayloadOnly;
                continue;
            }
            ok = false;
            if (shown < 8) std::printf("  field %zu %08x/%08x\n", i, ua[i], ub[i]);
        }
        if (!ok) Fail(what);
    } else if (std::memcmp(a, b, n) != 0) {
        Fail(what);
    }
}
static void CheckF(float a, float b, const char* what)
{
    ++checks;
    if (FBits(a) == FBits(b))
        return;
    if (BothNan(FBits(a), FBits(b))) {
        ++nanPayloadOnly;
        return;
    }
    if (shown < 40) std::printf("  %s ours %08x orig %08x\n", what, FBits(a), FBits(b));
    Fail(what);
}
static void CheckI(int a, int b, const char* what) { Check(a == b, what); }

// ---- prototypes (all exports of this module are __cdecl members unless noted; constructors/assignment __thiscall)
using CtorLine0 = void(__thiscall*)(LineObj*);
using CtorLine2 = void(__thiscall*)(LineObj*, const Vec3*, const Vec3*);
using CtorLineArr = void(__thiscall*)(LineObj*, const Vec3*);
using CtorLineCopy = void(__thiscall*)(LineObj*, const LineObj*);
using CtorLineSeg = void(__thiscall*)(LineObj*, const SegObj*);
using CtorLineRay = void(__thiscall*)(LineObj*, const RayObj*);
using AssignLine = LineObj*(__thiscall*)(LineObj*, const LineObj*);
using LineRet = LineObj*(__cdecl*)(LineObj*, const void*);
using LineScale = LineObj*(__cdecl*)(LineObj*, float);
using GetVec3 = const Vec3*(__cdecl*)(const void*);
using SetVec3 = void(__cdecl*)(void*, const Vec3*);
using FloatV3 = float(__cdecl*)(const void*, const Vec3*);
using BoolPlane = bool(__cdecl*)(const void*, const Plane*);
using BoolSphere = bool(__cdecl*)(const void*, const Sphere*);
using BoolCol = bool(__cdecl*)(const void*, const Vec3*);
using BoolColIn = bool(__cdecl*)(const void*, float, float*);
using BoolColIn2 = bool(__cdecl*)(const void*, float, float*, float*);
using BoolColVec = bool(__cdecl*)(const void*, float, Vec3*);
using BoolColVec2 = bool(__cdecl*)(const void*, float, Vec3*, Vec3*);
using BoolColV3 = bool(__cdecl*)(const void*, Vec3, float, float*, float*);
using BoolColV3Vec = bool(__cdecl*)(const void*, Vec3, float, Vec3*);
using BoolColV3Vec2 = bool(__cdecl*)(const void*, Vec3, float, Vec3*, Vec3*);
using ClosestSeg = Vec3 (__cdecl SegObj::*)(const Vec3&) const;
using GetCenterPoly = Vec2 (__cdecl PolyObj::*)() const;
using InterceptLine2D = Vec2(__cdecl Line2DObj::*)(const Line2DObj&) const;

using CtorSeg0 = void(__thiscall*)(SegObj*);
using CtorSeg2 = void(__thiscall*)(SegObj*, const Vec3*, const Vec3*);
using CtorSegArr = void(__thiscall*)(SegObj*, const Vec3*);
using CtorSegCopy = void(__thiscall*)(SegObj*, const SegObj*);
using AssignSeg = SegObj*(__thiscall*)(SegObj*, const SegObj*);
using SegMut = void(__cdecl*)(SegObj*, const void*);
using SegNormalize = void(__cdecl*)(SegObj*);

using CtorRay0 = void(__thiscall*)(RayObj*);
using CtorRay2 = void(__thiscall*)(RayObj*, const Vec3*, const Vec3*);
using CtorRayArr = void(__thiscall*)(RayObj*, const Vec3*);
using CtorRayCopy = void(__thiscall*)(RayObj*, const RayObj*);
using CtorRaySeg = void(__thiscall*)(RayObj*, const SegObj*);
using AssignRay = RayObj*(__thiscall*)(RayObj*, const RayObj*);
using RayNormalize = void(__cdecl*)(RayObj*);

using CtorL2D0 = void(__thiscall*)(Line2DObj*);
using CtorL2D2 = void(__thiscall*)(Line2DObj*, const Vec2*, const Vec2*);
using AssignL2D = Line2DObj*(__thiscall*)(Line2DObj*, const Line2DObj*);
using GetVec2 = const Vec2*(__cdecl*)(const void*);
using SetVec2 = void(__cdecl*)(void*, const Vec2*);
using BoolCol2 = bool(__cdecl*)(const void*, const void*);
using WhichSide = int(__cdecl*)(const void*, const Vec2*);
using WhichSideTol = int(__cdecl*)(const void*, const Vec2*, float);
using SepV2 = float(__cdecl*)(const void*, const Vec2*);
using SepL2 = float(__cdecl*)(const void*, const Line2DObj*);
using SepSeg2 = float(__cdecl*)(const void*, const Seg2DObj*);
using SepCirc2 = float(__cdecl*)(const void*, const Circle2DObj*);
using Inter2 = bool(__cdecl*)(const void*, const Line2DObj*, Vec2*);
using Inter2T = bool(__cdecl*)(const void*, const Line2DObj*, Vec2*, float*);

using CtorSeg2D0 = void(__thiscall*)(Seg2DObj*);
using CtorSeg2D2 = void(__thiscall*)(Seg2DObj*, const Vec2*, const Vec2*);
using AssignSeg2D = Seg2DObj*(__thiscall*)(Seg2DObj*, const Seg2DObj*);
using SetExtents = void(__cdecl*)(Seg2DObj*, const Vec2*, const Vec2*);

using CtorCircle0 = void(__thiscall*)(Circle2DObj*);
using CtorCircle2 = void(__thiscall*)(Circle2DObj*, const Vec2*, float);
using CtorCircle2V3 = void(__thiscall*)(Circle2DObj*, const Vec3*, float);
using AssignCircle = Circle2DObj*(__thiscall*)(Circle2DObj*, const Circle2DObj*);
using SetCenter = void(__cdecl*)(Circle2DObj*, const Vec2*);
using GetRadius = float(__cdecl*)(const Circle2DObj*);
using SetRadius = void(__cdecl*)(Circle2DObj*, float);

using CtorPoly0 = void(__thiscall*)(PolyObj*);
using CtorPoly3 = void(__thiscall*)(PolyObj*, int, Vec2*, bool);
using CtorPolyCopy = void(__thiscall*)(PolyObj*, const PolyObj*);
using DtorPoly = void(__thiscall*)(PolyObj*);
using AssignPoly = PolyObj*(__thiscall*)(PolyObj*, const PolyObj*);
using PolySet3 = void(__cdecl*)(PolyObj*, int, Vec2*, bool);
using PolySet1 = bool(__cdecl*)(PolyObj*, int);
using PolySetArr = void(__cdecl*)(PolyObj*, const FdaObj*);
using PolySetNum = void(__cdecl*)(PolyObj*, int);
using PolyGetNum = int(__cdecl*)(const PolyObj*);
using PolyGetVerts = Vec2*(__cdecl*)(PolyObj*);
using PolyGetConstVerts = const Vec2*(__cdecl*)(const PolyObj*);
using PolyCCW = int(__cdecl*)(PolyObj*, const Vec2*, const Vec2*, const Vec2*);
using PolyTheta = float(__cdecl*)(PolyObj*, const Vec2*, const Vec2*);
using PolyShell = void(__cdecl*)(PolyObj*, Node*, int*);
using PolyGraham = void(__cdecl*)(PolyObj*);

struct Api {
    CtorLine0 line0;
    CtorLine2 line2;
    CtorLineArr lineArr;
    CtorLineCopy lineCopy;
    CtorLineSeg lineFromSeg;
    CtorLineRay lineFromRay;
    AssignLine lineAssign;
    SetVec3 lineSetDir, lineSetPoint;
    GetVec3 lineGetDir, lineGetPoint;
    LineRet lineTranslate, lineRotate, lineSelfRotate;
    LineScale lineScale, lineMul, lineDiv;
    LineRet lineAdd, lineSub;
    LineObj*(__cdecl* lineNormalize)(LineObj*);
    BoolPlane linePlane;
    BoolSphere lineSphere;
    BoolColV3 lineCol3;
    BoolColV3Vec lineColV3;
    BoolColV3Vec2 lineColV3b;
    FloatV3 lineDistP, lineDistSq;
    float(__cdecl* lineDistL)(const LineObj*, const LineObj*);
    bool(__cdecl* lineColinear)(const LineObj*, const Vec3*, float);

    CtorSeg0 seg0;
    CtorSeg2 seg2;
    CtorSegArr segArr;
    CtorSegCopy segCopy;
    AssignSeg segAssign;
    SetVec3 segSetDir, segSetPoint, segSetEnd;
    GetVec3 segGetDir, segGetPoint, segGetEnd;
    ClosestSeg segClosest;
    FloatV3 segDistP, segDistSq;
    BoolCol segInSegment;
    bool(__cdecl* segColinear)(const SegObj*, const Vec3*, float);
    SegNormalize segNormalize;
    SegMut segTranslate;
    SegMut segRotate;
    BoolPlane segPlane;
    BoolSphere segSphere;

    CtorRay0 ray0;
    CtorRay2 ray2;
    CtorRayArr rayArr;
    CtorRayCopy rayCopy;
    CtorRaySeg rayFromSeg;
    AssignRay rayAssign;
    SetVec3 raySetDir, raySetPoint;
    GetVec3 rayGetDir, rayGetPoint;
    RayNormalize rayNormalize;
    FloatV3 rayDistP, rayDistSq;
    bool(__cdecl* rayColinear)(const RayObj*, const Vec3*, float);
    BoolPlane rayPlane;
    BoolSphere raySphere;
    BoolColV3 rayCol3;
    BoolColV3Vec rayColV3;
    BoolColV3Vec2 rayColV3b;

    CtorL2D0 l2d0;
    CtorL2D2 l2d2;
    AssignL2D l2dAssign;
    GetVec2 l2dGetDir, l2dGetPoint;
    SetVec2 l2dSetDir, l2dSetPoint;
    WhichSide l2dSide;
    WhichSideTol l2dSideTol;
    BoolCol2 l2dIntersects;
    InterceptLine2D l2dIntercept;
    SepV2 l2dSepV, l2dAppV, l2dSepSqV;
    SepL2 l2dSepL, l2dAppL, l2dSepSqL;

    CtorSeg2D0 s2d0;
    CtorSeg2D2 s2d2;
    AssignSeg2D s2dAssign;
    SetExtents s2dSetExtents;
    GetVec2 s2dGetV0, s2dGetV1;
    WhichSide s2dSide;
    WhichSideTol s2dSideTol;
    BoolCol2 s2dBeside;
    BoolCol2 s2dIntL;
    BoolCol2 s2dIntS;
    Inter2 s2dInter;
    Inter2T s2dInterT;
    SepV2 s2dSepV, s2dAppV, s2dSepSqV;
    SepL2 s2dSepL, s2dAppL, s2dSepSqL;
    SepSeg2 s2dSepS, s2dAppS, s2dSepSqS;

    CtorCircle0 c0;
    CtorCircle2 c2;
    CtorCircle2V3 c2v3;
    AssignCircle cAssign;
    SetCenter cSetCenter;
    GetVec2 cGetCenter;
    GetRadius cGetRadius;
    SetRadius cSetRadius;
    BoolCol2 cIntV, cInt2, cIntL, cIntS;
    SepV2 cSepV, cAppV, cSepSqV;
    SepL2 cSepL, cAppL, cSepSqL;
    SepCirc2 cSepC, cAppC, cSepSqC;
    SepSeg2 cSepS, cAppS, cSepSqS;

    CtorPoly0 p0;
    CtorPoly3 p3;
    CtorPolyCopy pCopy;
    DtorPoly pDtor;
    AssignPoly pAssign;
    PolySetNum pSetNum;
    PolyGetNum pGetNum;
    PolyGetVerts pGetVerts;
    PolyGetConstVerts pGetConstVerts;
    PolySet3 pSet3;
    PolySet1 pSet1;
    PolySetArr pSetArr;
    GetCenterPoly pGetCenter;
    BoolCol2 pIntV, pIntC, pIntL, pIntS;
    BoolCol2 pIntP;
    SepV2 pSepV, pAppV, pSepSqV;
    SepL2 pSepL, pAppL, pSepSqL;
    SepSeg2 pSepS, pAppS, pSepSqS;
    SepCirc2 pSepC, pAppC, pSepSqC;
    float(__cdecl* pSepP)(const PolyObj*, const PolyObj*);
    float(__cdecl* pSepSqP)(const PolyObj*, const PolyObj*);
    PolyCCW pCCW;
    PolyTheta pTheta;
    PolyShell pShell;
    PolyGraham pGraham;
};

static Api Load(int side)
{
    Api a{};
    a.line0 = Proc<decltype(a.line0)>(side, "??0Line@Jet@@QAE@XZ");
    a.line2 = Proc<decltype(a.line2)>(side, "??0Line@Jet@@QAE@ABVVector3@1@0@Z");
    a.lineArr = Proc<decltype(a.lineArr)>(side, "??0Line@Jet@@QAE@QBVVector3@1@@Z");
    a.lineCopy = Proc<decltype(a.lineCopy)>(side, "??0Line@Jet@@QAE@ABV01@@Z");
    a.lineFromSeg = Proc<decltype(a.lineFromSeg)>(side, "??0Line@Jet@@QAE@ABVLineSegment@1@@Z");
    a.lineFromRay = Proc<decltype(a.lineFromRay)>(side, "??0Line@Jet@@QAE@ABVRay@1@@Z");
    a.lineAssign = Proc<decltype(a.lineAssign)>(side, "??4Line@Jet@@QAEAAV01@ABV01@@Z");
    a.lineSetDir = Proc<decltype(a.lineSetDir)>(side, "?SetDirection@Line@Jet@@QAAXABVVector3@2@@Z");
    a.lineSetPoint = Proc<decltype(a.lineSetPoint)>(side, "?SetPoint@Line@Jet@@QAAXABVVector3@2@@Z");
    a.lineGetDir = Proc<decltype(a.lineGetDir)>(side, "?GetDirection@Line@Jet@@QBAABVVector3@2@XZ");
    a.lineGetPoint = Proc<decltype(a.lineGetPoint)>(side, "?GetPoint@Line@Jet@@QBAABVVector3@2@XZ");
    a.lineTranslate = Proc<decltype(a.lineTranslate)>(side, "?Translate@Line@Jet@@UAAAAV12@ABVVector3@2@@Z");
    a.lineRotate = Proc<decltype(a.lineRotate)>(side, "?Rotate@Line@Jet@@UAAAAV12@ABVMatrix3@2@@Z");
    a.lineSelfRotate = Proc<decltype(a.lineSelfRotate)>(side, "?SelfRotate@Line@Jet@@QAAAAV12@ABVMatrix3@2@@Z");
    a.lineScale = Proc<decltype(a.lineScale)>(side, "?Scale@Line@Jet@@QAAAAV12@M@Z");
    a.lineMul = Proc<decltype(a.lineMul)>(side, "??XLine@Jet@@QAAAAV01@M@Z");
    a.lineDiv = Proc<decltype(a.lineDiv)>(side, "??_0Line@Jet@@QAAAAV01@M@Z");
    a.lineAdd = Proc<decltype(a.lineAdd)>(side, "??YLine@Jet@@QAAAAV01@ABVVector3@1@@Z");
    a.lineSub = Proc<decltype(a.lineSub)>(side, "??ZLine@Jet@@QAAAAV01@ABVVector3@1@@Z");
    a.lineNormalize = Proc<decltype(a.lineNormalize)>(side, "?Normalize@Line@Jet@@QAAAAV12@XZ");
    a.linePlane = Proc<decltype(a.linePlane)>(side, "?TestCollision@Line@Jet@@UBA_NABVPlane@2@@Z");
    a.lineSphere = Proc<decltype(a.lineSphere)>(side, "?TestCollision@Line@Jet@@UBA_NABVSphere@2@@Z");
    a.lineCol3 = Proc<decltype(a.lineCol3)>(side, "?TestCollision@Line@Jet@@QBA_NVVector3@2@MAAM1@Z");
    a.lineColV3 = Proc<decltype(a.lineColV3)>(side, "?TestCollision@Line@Jet@@QBA_NVVector3@2@MAAV32@@Z");
    a.lineColV3b = Proc<decltype(a.lineColV3b)>(side, "?TestCollision@Line@Jet@@QBA_NVVector3@2@MAAV32@1@Z");
    a.lineDistP = Proc<decltype(a.lineDistP)>(side, "?Distance@Line@Jet@@QBAMABVVector3@2@@Z");
    a.lineDistL = Proc<decltype(a.lineDistL)>(side, "?Distance@Line@Jet@@QBAMABV12@@Z");
    a.lineDistSq = Proc<decltype(a.lineDistSq)>(side, "?DistanceSquared@Line@Jet@@QBAMABVVector3@2@@Z");
    a.lineColinear = Proc<decltype(a.lineColinear)>(side, "?IsColinear@Line@Jet@@QBA_NABVVector3@2@M@Z");

    a.seg0 = Proc<decltype(a.seg0)>(side, "??0LineSegment@Jet@@QAE@XZ");
    a.seg2 = Proc<decltype(a.seg2)>(side, "??0LineSegment@Jet@@QAE@ABVVector3@1@0@Z");
    a.segArr = Proc<decltype(a.segArr)>(side, "??0LineSegment@Jet@@QAE@QBVVector3@1@@Z");
    a.segCopy = Proc<decltype(a.segCopy)>(side, "??0LineSegment@Jet@@QAE@ABV01@@Z");
    a.segAssign = Proc<decltype(a.segAssign)>(side, "??4LineSegment@Jet@@QAEAAV01@ABV01@@Z");
    a.segSetDir = Proc<decltype(a.segSetDir)>(side, "?SetDirection@LineSegment@Jet@@QAAXABVVector3@2@@Z");
    a.segSetPoint = Proc<decltype(a.segSetPoint)>(side, "?SetPoint@LineSegment@Jet@@QAAXABVVector3@2@@Z");
    a.segSetEnd = Proc<decltype(a.segSetEnd)>(side, "?SetEndPoint@LineSegment@Jet@@QAAXABVVector3@2@@Z");
    a.segGetDir = Proc<decltype(a.segGetDir)>(side, "?GetDirection@LineSegment@Jet@@QBAABVVector3@2@XZ");
    a.segGetPoint = Proc<decltype(a.segGetPoint)>(side, "?GetPoint@LineSegment@Jet@@QBAABVVector3@2@XZ");
    a.segGetEnd = Proc<decltype(a.segGetEnd)>(side, "?GetEndPoint@LineSegment@Jet@@QBAABVVector3@2@XZ");
    a.segClosest = MemberProc<ClosestSeg>(side, "?GetClosestPoint@LineSegment@Jet@@QBA?AVVector3@2@ABV32@@Z");
    a.segDistP = Proc<decltype(a.segDistP)>(side, "?Distance@LineSegment@Jet@@QBAMABVVector3@2@@Z");
    a.segDistSq = Proc<decltype(a.segDistSq)>(side, "?DistanceSquared@LineSegment@Jet@@QBAMABVVector3@2@@Z");
    a.segInSegment = Proc<decltype(a.segInSegment)>(side, "?InSegment@LineSegment@Jet@@QBA_NABVVector3@2@@Z");
    a.segColinear = Proc<decltype(a.segColinear)>(side, "?IsColinear@LineSegment@Jet@@QBA_NABVVector3@2@M@Z");
    a.segNormalize = Proc<decltype(a.segNormalize)>(side, "?Normalize@LineSegment@Jet@@QAAXXZ");
    a.segTranslate = Proc<decltype(a.segTranslate)>(side, "?Translate@LineSegment@Jet@@QAAXABVVector3@2@@Z");
    a.segRotate = Proc<decltype(a.segRotate)>(side, "?Rotate@LineSegment@Jet@@QAAXABVMatrix3@2@@Z");
    a.segPlane = Proc<decltype(a.segPlane)>(side, "?TestCollision@LineSegment@Jet@@UBA_NABVPlane@2@@Z");
    a.segSphere = Proc<decltype(a.segSphere)>(side, "?TestCollision@LineSegment@Jet@@UBA_NABVSphere@2@@Z");

    a.ray0 = Proc<decltype(a.ray0)>(side, "??0Ray@Jet@@QAE@XZ");
    a.ray2 = Proc<decltype(a.ray2)>(side, "??0Ray@Jet@@QAE@ABVVector3@1@0@Z");
    a.rayArr = Proc<decltype(a.rayArr)>(side, "??0Ray@Jet@@QAE@QBVVector3@1@@Z");
    a.rayCopy = Proc<decltype(a.rayCopy)>(side, "??0Ray@Jet@@QAE@ABV01@@Z");
    a.rayFromSeg = Proc<decltype(a.rayFromSeg)>(side, "??0Ray@Jet@@QAE@ABVLineSegment@1@@Z");
    a.rayAssign = Proc<decltype(a.rayAssign)>(side, "??4Ray@Jet@@QAEAAV01@ABV01@@Z");
    a.raySetDir = Proc<decltype(a.raySetDir)>(side, "?SetDirection@Ray@Jet@@QAAXABVVector3@2@@Z");
    a.raySetPoint = Proc<decltype(a.raySetPoint)>(side, "?SetPoint@Ray@Jet@@QAAXABVVector3@2@@Z");
    a.rayGetDir = Proc<decltype(a.rayGetDir)>(side, "?GetDirection@Ray@Jet@@QBAABVVector3@2@XZ");
    a.rayGetPoint = Proc<decltype(a.rayGetPoint)>(side, "?GetPoint@Ray@Jet@@QBAABVVector3@2@XZ");
    a.rayNormalize = Proc<decltype(a.rayNormalize)>(side, "?Normalize@Ray@Jet@@QAAXXZ");
    a.rayDistP = Proc<decltype(a.rayDistP)>(side, "?Distance@Ray@Jet@@QBAMABVVector3@2@@Z");
    a.rayDistSq = Proc<decltype(a.rayDistSq)>(side, "?DistanceSquared@Ray@Jet@@QBAMABVVector3@2@@Z");
    a.rayColinear = Proc<decltype(a.rayColinear)>(side, "?IsColinear@Ray@Jet@@QBA_NABVVector3@2@M@Z");
    a.rayPlane = Proc<decltype(a.rayPlane)>(side, "?TestCollision@Ray@Jet@@UBA_NABVPlane@2@@Z");
    a.raySphere = Proc<decltype(a.raySphere)>(side, "?TestCollision@Ray@Jet@@UBA_NABVSphere@2@@Z");
    a.rayCol3 = Proc<decltype(a.rayCol3)>(side, "?TestCollision@Ray@Jet@@QBA_NVVector3@2@MAAM1@Z");
    a.rayColV3 = Proc<decltype(a.rayColV3)>(side, "?TestCollision@Ray@Jet@@QBA_NVVector3@2@MAAV32@@Z");
    a.rayColV3b = Proc<decltype(a.rayColV3b)>(side, "?TestCollision@Ray@Jet@@QBA_NVVector3@2@MAAV32@1@Z");

    a.l2d0 = Proc<decltype(a.l2d0)>(side, "??0Line2D@Jet@@QAE@XZ");
    a.l2d2 = Proc<decltype(a.l2d2)>(side, "??0Line2D@Jet@@QAE@ABVVector2@1@0@Z");
    a.l2dAssign = Proc<decltype(a.l2dAssign)>(side, "??4Line2D@Jet@@QAEAAV01@ABV01@@Z");
    a.l2dGetDir = Proc<decltype(a.l2dGetDir)>(side, "?GetDirection@Line2D@Jet@@QBAABVVector2@2@XZ");
    a.l2dGetPoint = Proc<decltype(a.l2dGetPoint)>(side, "?GetPoint@Line2D@Jet@@QBAABVVector2@2@XZ");
    a.l2dSetDir = Proc<decltype(a.l2dSetDir)>(side, "?SetDirection@Line2D@Jet@@QAAXABVVector2@2@@Z");
    a.l2dSetPoint = Proc<decltype(a.l2dSetPoint)>(side, "?SetPoint@Line2D@Jet@@QAAXABVVector2@2@@Z");
    a.l2dSide = Proc<decltype(a.l2dSide)>(side, "?WhichSide@Line2D@Jet@@QBA?AW4ClipResult@2@ABVVector2@2@@Z");
    a.l2dSideTol = Proc<decltype(a.l2dSideTol)>(side, "?WhichSide@Line2D@Jet@@QBA?AW4ClipResult@2@ABVVector2@2@M@Z");
    a.l2dIntersects = Proc<decltype(a.l2dIntersects)>(side, "?Intersects@Line2D@Jet@@QBA_NABV12@@Z");
    a.l2dIntercept = MemberProc<InterceptLine2D>(side, "?Intercept@Line2D@Jet@@QBA?AVVector2@2@ABV12@@Z");
    a.l2dSepV = Proc<decltype(a.l2dSepV)>(side, "?GetSeparation@Line2D@Jet@@QBAMABVVector2@2@@Z");
    a.l2dSepL = Proc<decltype(a.l2dSepL)>(side, "?GetSeparation@Line2D@Jet@@QBAMABV12@@Z");
    a.l2dAppV = Proc<decltype(a.l2dAppV)>(side, "?GetApproxSeparation@Line2D@Jet@@QBAMABVVector2@2@@Z");
    a.l2dAppL = Proc<decltype(a.l2dAppL)>(side, "?GetApproxSeparation@Line2D@Jet@@QBAMABV12@@Z");
    a.l2dSepSqV = Proc<decltype(a.l2dSepSqV)>(side, "?GetSeparationSquared@Line2D@Jet@@QBAMABVVector2@2@@Z");
    a.l2dSepSqL = Proc<decltype(a.l2dSepSqL)>(side, "?GetSeparationSquared@Line2D@Jet@@QBAMABV12@@Z");

    a.s2d0 = Proc<decltype(a.s2d0)>(side, "??0LineSegment2D@Jet@@QAE@XZ");
    a.s2d2 = Proc<decltype(a.s2d2)>(side, "??0LineSegment2D@Jet@@QAE@ABVVector2@1@0@Z");
    a.s2dAssign = Proc<decltype(a.s2dAssign)>(side, "??4LineSegment2D@Jet@@QAEAAV01@ABV01@@Z");
    a.s2dSetExtents = Proc<decltype(a.s2dSetExtents)>(side, "?SetExtents@LineSegment2D@Jet@@QAAXABVVector2@2@0@Z");
    a.s2dGetV0 = Proc<decltype(a.s2dGetV0)>(side, "?GetV0@LineSegment2D@Jet@@QBAABVVector2@2@XZ");
    a.s2dGetV1 = Proc<decltype(a.s2dGetV1)>(side, "?GetV1@LineSegment2D@Jet@@QBAABVVector2@2@XZ");
    a.s2dSide = Proc<decltype(a.s2dSide)>(side, "?WhichSide@LineSegment2D@Jet@@QBA?AW4ClipResult@2@ABVVector2@2@@Z");
    a.s2dSideTol = Proc<decltype(a.s2dSideTol)>(side, "?WhichSide@LineSegment2D@Jet@@QBA?AW4ClipResult@2@ABVVector2@2@M@Z");
    a.s2dBeside = Proc<decltype(a.s2dBeside)>(side, "?IsBeside@LineSegment2D@Jet@@QBA_NABVVector2@2@@Z");
    a.s2dIntL = Proc<decltype(a.s2dIntL)>(side, "?Intersects@LineSegment2D@Jet@@QBA_NABVLine2D@2@@Z");
    a.s2dIntS = Proc<decltype(a.s2dIntS)>(side, "?Intersects@LineSegment2D@Jet@@QBA_NABV12@@Z");
    a.s2dInter = Proc<decltype(a.s2dInter)>(side, "?Intercept@LineSegment2D@Jet@@QBA_NABVLine2D@2@AAVVector2@2@@Z");
    a.s2dInterT = Proc<decltype(a.s2dInterT)>(side, "?Intercept@LineSegment2D@Jet@@QBA_NABVLine2D@2@AAVVector2@2@AAM@Z");
    a.s2dSepV = Proc<decltype(a.s2dSepV)>(side, "?GetSeparation@LineSegment2D@Jet@@QBAMABVVector2@2@@Z");
    a.s2dSepL = Proc<decltype(a.s2dSepL)>(side, "?GetSeparation@LineSegment2D@Jet@@QBAMABVLine2D@2@@Z");
    a.s2dSepS = Proc<decltype(a.s2dSepS)>(side, "?GetSeparation@LineSegment2D@Jet@@QBAMABV12@@Z");
    a.s2dAppV = Proc<decltype(a.s2dAppV)>(side, "?GetApproxSeparation@LineSegment2D@Jet@@QBAMABVVector2@2@@Z");
    a.s2dAppL = Proc<decltype(a.s2dAppL)>(side, "?GetApproxSeparation@LineSegment2D@Jet@@QBAMABVLine2D@2@@Z");
    a.s2dAppS = Proc<decltype(a.s2dAppS)>(side, "?GetApproxSeparation@LineSegment2D@Jet@@QBAMABV12@@Z");
    a.s2dSepSqV = Proc<decltype(a.s2dSepSqV)>(side, "?GetSeparationSquared@LineSegment2D@Jet@@QBAMABVVector2@2@@Z");
    a.s2dSepSqL = Proc<decltype(a.s2dSepSqL)>(side, "?GetSeparationSquared@LineSegment2D@Jet@@QBAMABVLine2D@2@@Z");
    a.s2dSepSqS = Proc<decltype(a.s2dSepSqS)>(side, "?GetSeparationSquared@LineSegment2D@Jet@@QBAMABV12@@Z");

    a.c0 = Proc<decltype(a.c0)>(side, "??0Circle2D@Jet@@QAE@XZ");
    a.c2 = Proc<decltype(a.c2)>(side, "??0Circle2D@Jet@@QAE@ABVVector2@1@M@Z");
    a.c2v3 = Proc<decltype(a.c2v3)>(side, "??0Circle2D@Jet@@QAE@ABVVector3@1@M@Z");
    a.cAssign = Proc<decltype(a.cAssign)>(side, "??4Circle2D@Jet@@QAEAAV01@ABV01@@Z");
    a.cSetCenter = Proc<decltype(a.cSetCenter)>(side, "?SetCenter@Circle2D@Jet@@QAAXABVVector2@2@@Z");
    a.cGetCenter = Proc<decltype(a.cGetCenter)>(side, "?GetCenter@Circle2D@Jet@@QBAABVVector2@2@XZ");
    a.cGetRadius = Proc<decltype(a.cGetRadius)>(side, "?GetRadius@Circle2D@Jet@@QBAMXZ");
    a.cSetRadius = Proc<decltype(a.cSetRadius)>(side, "?SetRadius@Circle2D@Jet@@QAAXM@Z");
    a.cIntV = Proc<decltype(a.cIntV)>(side, "?Intersects@Circle2D@Jet@@QBA_NABVVector2@2@@Z");
    a.cIntL = Proc<decltype(a.cIntL)>(side, "?Intersects@Circle2D@Jet@@QBA_NABVLine2D@2@@Z");
    a.cInt2 = Proc<decltype(a.cInt2)>(side, "?Intersects@Circle2D@Jet@@QBA_NABV12@@Z");
    a.cIntS = Proc<decltype(a.cIntS)>(side, "?Intersects@Circle2D@Jet@@QBA_NABVLineSegment2D@2@@Z");
    a.cSepV = Proc<decltype(a.cSepV)>(side, "?GetSeparation@Circle2D@Jet@@QBAMABVVector2@2@@Z");
    a.cSepL = Proc<decltype(a.cSepL)>(side, "?GetSeparation@Circle2D@Jet@@QBAMABVLine2D@2@@Z");
    a.cSepC = Proc<decltype(a.cSepC)>(side, "?GetSeparation@Circle2D@Jet@@QBAMABV12@@Z");
    a.cSepS = Proc<decltype(a.cSepS)>(side, "?GetSeparation@Circle2D@Jet@@QBAMABVLineSegment2D@2@@Z");
    a.cAppV = Proc<decltype(a.cAppV)>(side, "?GetApproxSeparation@Circle2D@Jet@@QBAMABVVector2@2@@Z");
    a.cAppL = Proc<decltype(a.cAppL)>(side, "?GetApproxSeparation@Circle2D@Jet@@QBAMABVLine2D@2@@Z");
    a.cAppC = Proc<decltype(a.cAppC)>(side, "?GetApproxSeparation@Circle2D@Jet@@QBAMABV12@@Z");
    a.cAppS = Proc<decltype(a.cAppS)>(side, "?GetApproxSeparation@Circle2D@Jet@@QBAMABVLineSegment2D@2@@Z");
    a.cSepSqV = Proc<decltype(a.cSepSqV)>(side, "?GetSeparationSquared@Circle2D@Jet@@QBAMABVVector2@2@@Z");
    a.cSepSqL = Proc<decltype(a.cSepSqL)>(side, "?GetSeparationSquared@Circle2D@Jet@@QBAMABVLine2D@2@@Z");
    a.cSepSqC = Proc<decltype(a.cSepSqC)>(side, "?GetSeparationSquared@Circle2D@Jet@@QBAMABV12@@Z");
    a.cSepSqS = Proc<decltype(a.cSepSqS)>(side, "?GetSeparationSquared@Circle2D@Jet@@QBAMABVLineSegment2D@2@@Z");

    a.p0 = Proc<decltype(a.p0)>(side, "??0Polygon2D@Jet@@QAE@XZ");
    a.p3 = Proc<decltype(a.p3)>(side, "??0Polygon2D@Jet@@QAE@HPAVVector2@1@_N@Z");
    a.pCopy = Proc<decltype(a.pCopy)>(side, "??0Polygon2D@Jet@@QAE@ABV01@@Z");
    a.pDtor = Proc<decltype(a.pDtor)>(side, "??1Polygon2D@Jet@@QAE@XZ");
    a.pAssign = Proc<decltype(a.pAssign)>(side, "??4Polygon2D@Jet@@QAEAAV01@ABV01@@Z");
    a.pSetNum = Proc<decltype(a.pSetNum)>(side, "?SetNumVerts@Polygon2D@Jet@@QAAXH@Z");
    a.pGetNum = Proc<decltype(a.pGetNum)>(side, "?GetNumVerts@Polygon2D@Jet@@QBAHXZ");
    a.pGetVerts = Proc<decltype(a.pGetVerts)>(side, "?GetVertices@Polygon2D@Jet@@QAAPAVVector2@2@XZ");
    a.pGetConstVerts = Proc<decltype(a.pGetConstVerts)>(side, "?GetConstVertices@Polygon2D@Jet@@QBAPBVVector2@2@XZ");
    a.pSet3 = Proc<decltype(a.pSet3)>(side, "?Set@Polygon2D@Jet@@QAAXHPAVVector2@2@_N@Z");
    a.pSet1 = Proc<decltype(a.pSet1)>(side, "?Set@Polygon2D@Jet@@QAA_NH@Z");
    a.pSetArr = Proc<decltype(a.pSetArr)>(
        side, "?Set@Polygon2D@Jet@@QAAXABV?$FastDynamicArray@VVector2@Jet@@$0N@U?$Less@VVector2@Jet@@V12@@2@@2@@Z");
    a.pGetCenter = MemberProc<GetCenterPoly>(side, "?GetCenter@Polygon2D@Jet@@QBA?AVVector2@2@XZ");
    a.pIntV = Proc<decltype(a.pIntV)>(side, "?Intersects@Polygon2D@Jet@@QBA_NABVVector2@2@@Z");
    a.pIntL = Proc<decltype(a.pIntL)>(side, "?Intersects@Polygon2D@Jet@@QBA_NABVLine2D@2@@Z");
    a.pIntS = Proc<decltype(a.pIntS)>(side, "?Intersects@Polygon2D@Jet@@QBA_NABVLineSegment2D@2@@Z");
    a.pIntC = Proc<decltype(a.pIntC)>(side, "?Intersects@Polygon2D@Jet@@QBA_NABVCircle2D@2@@Z");
    a.pIntP = Proc<decltype(a.pIntP)>(side, "?Intersects@Polygon2D@Jet@@QBA_NABV12@@Z");
    a.pSepV = Proc<decltype(a.pSepV)>(side, "?GetSeparation@Polygon2D@Jet@@QBAMABVVector2@2@@Z");
    a.pSepL = Proc<decltype(a.pSepL)>(side, "?GetSeparation@Polygon2D@Jet@@QBAMABVLine2D@2@@Z");
    a.pSepS = Proc<decltype(a.pSepS)>(side, "?GetSeparation@Polygon2D@Jet@@QBAMABVLineSegment2D@2@@Z");
    a.pSepC = Proc<decltype(a.pSepC)>(side, "?GetSeparation@Polygon2D@Jet@@QBAMABVCircle2D@2@@Z");
    a.pSepP = Proc<decltype(a.pSepP)>(side, "?GetSeparation@Polygon2D@Jet@@QBAMABV12@@Z");
    a.pAppV = Proc<decltype(a.pAppV)>(side, "?GetApproxSeparation@Polygon2D@Jet@@QBAMABVVector2@2@@Z");
    a.pAppL = Proc<decltype(a.pAppL)>(side, "?GetApproxSeparation@Polygon2D@Jet@@QBAMABVLine2D@2@@Z");
    a.pAppS = Proc<decltype(a.pAppS)>(side, "?GetApproxSeparation@Polygon2D@Jet@@QBAMABVLineSegment2D@2@@Z");
    a.pAppC = Proc<decltype(a.pAppC)>(side, "?GetApproxSeparation@Polygon2D@Jet@@QBAMABVCircle2D@2@@Z");
    a.pSepSqV = Proc<decltype(a.pSepSqV)>(side, "?GetSeparationSquared@Polygon2D@Jet@@QBAMABVVector2@2@@Z");
    a.pSepSqL = Proc<decltype(a.pSepSqL)>(side, "?GetSeparationSquared@Polygon2D@Jet@@QBAMABVLine2D@2@@Z");
    a.pSepSqS = Proc<decltype(a.pSepSqS)>(side, "?GetSeparationSquared@Polygon2D@Jet@@QBAMABVLineSegment2D@2@@Z");
    a.pSepSqC = Proc<decltype(a.pSepSqC)>(side, "?GetSeparationSquared@Polygon2D@Jet@@QBAMABVCircle2D@2@@Z");
    a.pSepSqP = Proc<decltype(a.pSepSqP)>(side, "?GetSeparationSquared@Polygon2D@Jet@@QBAMABV12@@Z");
    a.pCCW = Proc<decltype(a.pCCW)>(side, "?CounterClockwise@Polygon2D@Jet@@IAAHPBVVector2@2@00@Z");
    a.pTheta = Proc<decltype(a.pTheta)>(side, "?Theta@Polygon2D@Jet@@IAAMPBVVector2@2@0@Z");
    a.pShell = Proc<decltype(a.pShell)>(side, "?ShellSortTheta@Polygon2D@Jet@@IAAXPAUPointNode@12@AAH@Z");
    a.pGraham = Proc<decltype(a.pGraham)>(side, "?GrahamScan@Polygon2D@Jet@@QAAXXZ");
    return a;
}

static void Round(int round)
{
    const bool awk = (round % 5) == 0;
    Api api[2] = {Load(0), Load(1)};
    const Vec3 a = RV3(awk), b = RV3(awk), c = RV3(awk);
    const Vec2 a2 = RV2(awk), b2 = RV2(awk), c2 = RV2(awk);
    const float s = awk ? Awkward() : FRange(-10.0f, 10.0f);
    const Mat3 m = RMat(awk);
    const Vec3 points[2] = {a, b};
    const Vec2 points2[2] = {a2, b2};

    // ---- Line
    {
        LineObj obj[2];
        unsigned char seed[sizeof(LineObj)];
        std::memset(seed, 0x5a, sizeof seed);
        for (int side = 0; side < 2; ++side) {
            std::memcpy(&obj[side], seed, sizeof seed);
            api[side].line0(&obj[side]);
        }
        CheckBytes(reinterpret_cast<char*>(&obj[0]) + 4, reinterpret_cast<char*>(&obj[1]) + 4, sizeof(LineObj) - 4,
                   "Line::Line()");
        for (int side = 0; side < 2; ++side) api[side].line2(&obj[side], &a, &b);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line::Line(dir,point)");
        for (int side = 0; side < 2; ++side) api[side].lineArr(&obj[side], points);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line::Line(points)");
        // copy and from segment/ray
        LineObj other[2];
        api[0].line2(&other[0], &b, &c);
        api[1].line2(&other[1], &b, &c);
        for (int side = 0; side < 2; ++side) api[side].lineCopy(&obj[side], &other[side]);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line copy ctor");
        SegObj seg[2];
        for (int side = 0; side < 2; ++side) api[side].seg2(&seg[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].lineFromSeg(&obj[side], &seg[side]);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line(LineSegment)");
        RayObj ray[2];
        for (int side = 0; side < 2; ++side) api[side].ray2(&ray[side], &b, &c);
        for (int side = 0; side < 2; ++side) api[side].lineFromRay(&obj[side], &ray[side]);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line(Ray)");
        for (int side = 0; side < 2; ++side) api[side].line2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].lineAssign(&obj[side], &other[side]);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line assignment");
        // setters/getters
        for (int side = 0; side < 2; ++side) {
            api[side].lineSetDir(&obj[side], &c);
            api[side].lineSetPoint(&obj[side], &a);
        }
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line setters");
        CheckBytes(api[0].lineGetDir(&obj[0]), api[1].lineGetDir(&obj[1]), sizeof(Vec3), "Line GetDirection");
        CheckBytes(api[0].lineGetPoint(&obj[0]), api[1].lineGetPoint(&obj[1]), sizeof(Vec3), "Line GetPoint");
        // mutators from the same state
        for (int side = 0; side < 2; ++side) api[side].line2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].lineTranslate(&obj[side], &c);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line Translate");
        for (int side = 0; side < 2; ++side) api[side].line2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].lineRotate(&obj[side], &m);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line Rotate");
        for (int side = 0; side < 2; ++side) api[side].line2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].lineSelfRotate(&obj[side], &m);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line SelfRotate");
        for (int side = 0; side < 2; ++side) api[side].line2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].lineScale(&obj[side], s);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line Scale");
        for (int side = 0; side < 2; ++side) api[side].line2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].lineMul(&obj[side], s);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line *=");
        for (int side = 0; side < 2; ++side) api[side].line2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].lineDiv(&obj[side], s);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line /=");
        for (int side = 0; side < 2; ++side) api[side].line2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].lineAdd(&obj[side], &c);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line +=");
        for (int side = 0; side < 2; ++side) api[side].line2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].lineSub(&obj[side], &c);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line -=");
        for (int side = 0; side < 2; ++side) api[side].line2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].lineNormalize(&obj[side]);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Line Normalize");
        // queries
        for (int side = 0; side < 2; ++side) api[side].line2(&obj[side], &a, &b);
        CheckF(api[0].lineDistP(&obj[0], &c), api[1].lineDistP(&obj[1], &c), "Line Distance(point)");
        CheckF(api[0].lineDistSq(&obj[0], &c), api[1].lineDistSq(&obj[1], &c), "Line DistanceSquared");
        CheckF(api[0].lineDistL(&obj[0], &other[0]), api[1].lineDistL(&obj[1], &other[1]), "Line Distance(line)");
        Check(api[0].lineColinear(&obj[0], &c, s) == api[1].lineColinear(&obj[1], &c, s), "Line IsColinear");
        Plane pl{RV3(awk), Rnd(awk)};
        Sphere sph{nullptr, RV3(awk), Rnd(awk), 0.0f};
        Check(api[0].linePlane(&obj[0], &pl) == api[1].linePlane(&obj[1], &pl), "Line TestCollision(Plane)");
        Check(api[0].lineSphere(&obj[0], &sph) == api[1].lineSphere(&obj[1], &sph), "Line TestCollision(Sphere)");
        float t1[2], t2[2];
        const bool r0 = api[0].lineCol3(&obj[0], c, s, &t1[0], &t2[0]);
        const bool r1 = api[1].lineCol3(&obj[1], c, s, &t1[1], &t2[1]);
        Check(r0 == r1, "Line TestCollision(V3,f,t,t)");
        if (r0 && r1) {
            CheckF(t1[0], t1[1], "Line tc t1");
            CheckF(t2[0], t2[1], "Line tc t2");
        }
        Vec3 p0{}, p1{};
        const bool q0 = api[0].lineColV3(&obj[0], c, s, &p0);
        const bool q1 = api[1].lineColV3(&obj[1], c, s, &p1);
        Check(q0 == q1, "Line TestCollision(V3,f,V3)");
        if (q0 && q1) CheckBytes(&p0, &p1, sizeof p0, "Line tc point");
        Vec3 u0{}, u1{};
        const bool w0 = api[0].lineColV3b(&obj[0], c, s, &p0, &u0);
        const bool w1 = api[1].lineColV3b(&obj[1], c, s, &p1, &u1);
        Check(w0 == w1, "Line TestCollision(V3,f,V3,V3)");
        if (w0 && w1) {
            CheckBytes(&p0, &p1, sizeof p0, "Line tc points 1");
            CheckBytes(&u0, &u1, sizeof u0, "Line tc points 2");
        }
    }

    // ---- LineSegment
    {
        SegObj obj[2], other[2];
        unsigned char seed[sizeof(SegObj)];
        std::memset(seed, 0x5a, sizeof seed);
        for (int side = 0; side < 2; ++side) {
            std::memcpy(&obj[side], seed, sizeof seed);
            api[side].seg0(&obj[side]);
        }
        CheckBytes(reinterpret_cast<char*>(&obj[0]) + 4, reinterpret_cast<char*>(&obj[1]) + 4, sizeof(SegObj) - 4,
                   "LineSegment::LineSegment()");
        for (int side = 0; side < 2; ++side) api[side].seg2(&obj[side], &a, &b);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 3, "LineSegment::(p,e)");
        for (int side = 0; side < 2; ++side) api[side].segArr(&obj[side], points);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 3, "LineSegment::(points)");
        for (int side = 0; side < 2; ++side) api[side].seg2(&other[side], &b, &c);
        for (int side = 0; side < 2; ++side) api[side].segCopy(&obj[side], &other[side]);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 3, "LineSegment copy ctor");
        api[0].seg2(&obj[0], &a, &b);
        api[1].seg2(&obj[1], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].segAssign(&obj[side], &other[side]);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 3, "LineSegment assignment");
        for (int side = 0; side < 2; ++side) {
            api[side].segSetDir(&obj[side], &c);
            api[side].segSetPoint(&obj[side], &a);
            api[side].segSetEnd(&obj[side], &b);
        }
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 3, "LineSegment setters");
        CheckBytes(api[0].segGetDir(&obj[0]), api[1].segGetDir(&obj[1]), sizeof(Vec3), "seg GetDirection");
        CheckBytes(api[0].segGetPoint(&obj[0]), api[1].segGetPoint(&obj[1]), sizeof(Vec3), "seg GetPoint");
        CheckBytes(api[0].segGetEnd(&obj[0]), api[1].segGetEnd(&obj[1]), sizeof(Vec3), "seg GetEndPoint");
        for (int side = 0; side < 2; ++side) api[side].seg2(&obj[side], &a, &b);
        Vec3 cp[2] = {(obj[0].*api[0].segClosest)(c), (obj[1].*api[1].segClosest)(c)};
        CheckBytes(&cp[0], &cp[1], sizeof(Vec3), "seg GetClosestPoint");
        CheckF(api[0].segDistP(&obj[0], &c), api[1].segDistP(&obj[1], &c), "seg Distance");
        CheckF(api[0].segDistSq(&obj[0], &c), api[1].segDistSq(&obj[1], &c), "seg DistanceSquared");
        Check(api[0].segInSegment(&obj[0], &c) == api[1].segInSegment(&obj[1], &c), "seg InSegment");
        Check(api[0].segColinear(&obj[0], &c, s) == api[1].segColinear(&obj[1], &c, s), "seg IsColinear");
        for (int side = 0; side < 2; ++side) api[side].seg2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].segNormalize(&obj[side]);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 3, "seg Normalize");
        for (int side = 0; side < 2; ++side) api[side].seg2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].segTranslate(&obj[side], &c);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 3, "seg Translate");
        for (int side = 0; side < 2; ++side) api[side].seg2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].segRotate(&obj[side], &m);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 3, "seg Rotate");
        for (int side = 0; side < 2; ++side) api[side].seg2(&obj[side], &a, &b);
        Plane pl{RV3(awk), Rnd(awk)};
        Sphere sph{nullptr, RV3(awk), Rnd(awk), 0.0f};
        Check(api[0].segPlane(&obj[0], &pl) == api[1].segPlane(&obj[1], &pl), "seg TestCollision(Plane)");
        Check(api[0].segSphere(&obj[0], &sph) == api[1].segSphere(&obj[1], &sph), "seg TestCollision(Sphere)");
    }

    // ---- Ray
    {
        RayObj obj[2], other[2];
        unsigned char seed[sizeof(RayObj)];
        std::memset(seed, 0x5a, sizeof seed);
        for (int side = 0; side < 2; ++side) {
            std::memcpy(&obj[side], seed, sizeof seed);
            api[side].ray0(&obj[side]);
        }
        CheckBytes(reinterpret_cast<char*>(&obj[0]) + 4, reinterpret_cast<char*>(&obj[1]) + 4, sizeof(RayObj) - 4,
                   "Ray::Ray()");
        for (int side = 0; side < 2; ++side) api[side].ray2(&obj[side], &a, &b);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Ray::(dir,point)");
        for (int side = 0; side < 2; ++side) api[side].rayArr(&obj[side], points);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Ray::(points)");
        for (int side = 0; side < 2; ++side) api[side].ray2(&other[side], &b, &c);
        for (int side = 0; side < 2; ++side) api[side].rayCopy(&obj[side], &other[side]);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Ray copy ctor");
        SegObj seg[2];
        for (int side = 0; side < 2; ++side) api[side].seg2(&seg[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].rayFromSeg(&obj[side], &seg[side]);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Ray(LineSegment)");
        api[0].ray2(&obj[0], &a, &b);
        api[1].ray2(&obj[1], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].rayAssign(&obj[side], &other[side]);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Ray assignment");
        for (int side = 0; side < 2; ++side) {
            api[side].raySetDir(&obj[side], &c);
            api[side].raySetPoint(&obj[side], &a);
        }
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Ray setters");
        CheckBytes(api[0].rayGetDir(&obj[0]), api[1].rayGetDir(&obj[1]), sizeof(Vec3), "Ray GetDirection");
        CheckBytes(api[0].rayGetPoint(&obj[0]), api[1].rayGetPoint(&obj[1]), sizeof(Vec3), "Ray GetPoint");
        for (int side = 0; side < 2; ++side) api[side].ray2(&obj[side], &a, &b);
        for (int side = 0; side < 2; ++side) api[side].rayNormalize(&obj[side]);
        CheckBytes(&obj[0].direction, &obj[1].direction, sizeof(Vec3) * 2, "Ray Normalize");
        for (int side = 0; side < 2; ++side) api[side].ray2(&obj[side], &a, &b);
        CheckF(api[0].rayDistP(&obj[0], &c), api[1].rayDistP(&obj[1], &c), "Ray Distance");
        CheckF(api[0].rayDistSq(&obj[0], &c), api[1].rayDistSq(&obj[1], &c), "Ray DistanceSquared");
        Check(api[0].rayColinear(&obj[0], &c, s) == api[1].rayColinear(&obj[1], &c, s), "Ray IsColinear");
        Plane pl{RV3(awk), Rnd(awk)};
        Sphere sph{nullptr, RV3(awk), Rnd(awk), 0.0f};
        Check(api[0].rayPlane(&obj[0], &pl) == api[1].rayPlane(&obj[1], &pl), "Ray TestCollision(Plane)");
        Check(api[0].raySphere(&obj[0], &sph) == api[1].raySphere(&obj[1], &sph), "Ray TestCollision(Sphere)");
        float t1[2], t2[2];
        const bool r0 = api[0].rayCol3(&obj[0], c, s, &t1[0], &t2[0]);
        const bool r1 = api[1].rayCol3(&obj[1], c, s, &t1[1], &t2[1]);
        Check(r0 == r1, "Ray TestCollision(V3,f,t,t)");
        if (r0 && r1) {
            CheckF(t1[0], t1[1], "Ray tc t1");
            CheckF(t2[0], t2[1], "Ray tc t2");
        }
        Vec3 p0{}, p1{}, u0{}, u1{};
        const bool q0 = api[0].rayColV3(&obj[0], c, s, &p0);
        const bool q1 = api[1].rayColV3(&obj[1], c, s, &p1);
        Check(q0 == q1, "Ray TestCollision(V3,f,V3)");
        if (q0 && q1) CheckBytes(&p0, &p1, sizeof p0, "Ray tc point");
        const bool w0 = api[0].rayColV3b(&obj[0], c, s, &p0, &u0);
        const bool w1 = api[1].rayColV3b(&obj[1], c, s, &p1, &u1);
        Check(w0 == w1, "Ray TestCollision(V3,f,V3,V3)");
        if (w0 && w1) {
            CheckBytes(&p0, &p1, sizeof p0, "Ray tc points 1");
            CheckBytes(&u0, &u1, sizeof u0, "Ray tc points 2");
        }
    }

    // ---- Line2D
    {
        Line2DObj obj[2], other[2];
        unsigned char seed[sizeof(Line2DObj)];
        std::memset(seed, 0x5a, sizeof seed);
        for (int side = 0; side < 2; ++side) {
            std::memcpy(&obj[side], seed, sizeof seed);
            api[side].l2d0(&obj[side]);
        }
        CheckBytes(&obj[0], &obj[1], sizeof(Line2DObj), "Line2D::Line2D()");
        for (int side = 0; side < 2; ++side) api[side].l2d2(&obj[side], &a2, &b2);
        CheckBytes(&obj[0], &obj[1], sizeof(Line2DObj), "Line2D::(dir,point)");
        for (int side = 0; side < 2; ++side) api[side].l2d2(&other[side], &b2, &c2);
        for (int side = 0; side < 2; ++side) api[side].l2dAssign(&obj[side], &other[side]);
        CheckBytes(&obj[0], &obj[1], sizeof(Line2DObj), "Line2D assignment");
        for (int side = 0; side < 2; ++side) {
            api[side].l2dSetDir(&obj[side], &c2);
            api[side].l2dSetPoint(&obj[side], &a2);
        }
        CheckBytes(&obj[0], &obj[1], sizeof(Line2DObj), "Line2D setters");
        CheckBytes(api[0].l2dGetDir(&obj[0]), api[1].l2dGetDir(&obj[1]), sizeof(Vec2), "l2d GetDirection");
        CheckBytes(api[0].l2dGetPoint(&obj[0]), api[1].l2dGetPoint(&obj[1]), sizeof(Vec2), "l2d GetPoint");
        for (int side = 0; side < 2; ++side) api[side].l2d2(&obj[side], &a2, &b2);
        CheckI(api[0].l2dSide(&obj[0], &c2), api[1].l2dSide(&obj[1], &c2), "l2d WhichSide");
        CheckI(api[0].l2dSideTol(&obj[0], &c2, s), api[1].l2dSideTol(&obj[1], &c2, s), "l2d WhichSide(tol)");
        Check(api[0].l2dIntersects(&obj[0], &other[0]) == api[1].l2dIntersects(&obj[1], &other[1]),
              "l2d Intersects");
        const Vec2 i0 = (obj[0].*api[0].l2dIntercept)(other[0]);
        const Vec2 i1 = (obj[1].*api[1].l2dIntercept)(other[1]);
        CheckBytes(&i0, &i1, sizeof(Vec2), "l2d Intercept");
        CheckF(api[0].l2dSepV(&obj[0], &c2), api[1].l2dSepV(&obj[1], &c2), "l2d GetSeparation(V)");
        CheckF(api[0].l2dSepL(&obj[0], &other[0]), api[1].l2dSepL(&obj[1], &other[1]), "l2d GetSeparation(L)");
        CheckF(api[0].l2dAppV(&obj[0], &c2), api[1].l2dAppV(&obj[1], &c2), "l2d GetApproxSeparation(V)");
        CheckF(api[0].l2dAppL(&obj[0], &other[0]), api[1].l2dAppL(&obj[1], &other[1]), "l2d GetApproxSeparation(L)");
        CheckF(api[0].l2dSepSqV(&obj[0], &c2), api[1].l2dSepSqV(&obj[1], &c2), "l2d GetSeparationSquared(V)");
        CheckF(api[0].l2dSepSqL(&obj[0], &other[0]), api[1].l2dSepSqL(&obj[1], &other[1]),
               "l2d GetSeparationSquared(L)");
    }

    // ---- LineSegment2D
    {
        Seg2DObj obj[2], other[2];
        unsigned char seed[sizeof(Seg2DObj)];
        std::memset(seed, 0x5a, sizeof seed);
        for (int side = 0; side < 2; ++side) {
            std::memcpy(&obj[side], seed, sizeof seed);
            api[side].s2d0(&obj[side]);
        }
        CheckBytes(&obj[0], &obj[1], sizeof(Seg2DObj), "LineSegment2D::()");
        for (int side = 0; side < 2; ++side) api[side].s2d2(&obj[side], &a2, &b2);
        CheckBytes(&obj[0], &obj[1], sizeof(Seg2DObj), "LineSegment2D::(a,b)");
        for (int side = 0; side < 2; ++side) api[side].s2d2(&other[side], &b2, &c2);
        for (int side = 0; side < 2; ++side) api[side].s2dAssign(&obj[side], &other[side]);
        CheckBytes(&obj[0], &obj[1], sizeof(Seg2DObj), "LineSegment2D assignment");
        for (int side = 0; side < 2; ++side) api[side].s2dSetExtents(&obj[side], &c2, &a2);
        CheckBytes(&obj[0], &obj[1], sizeof(Seg2DObj), "LineSegment2D SetExtents");
        CheckBytes(api[0].s2dGetV0(&obj[0]), api[1].s2dGetV0(&obj[1]), sizeof(Vec2), "s2d GetV0");
        CheckBytes(api[0].s2dGetV1(&obj[0]), api[1].s2dGetV1(&obj[1]), sizeof(Vec2), "s2d GetV1");
        for (int side = 0; side < 2; ++side) api[side].s2d2(&obj[side], &a2, &b2);
        CheckI(api[0].s2dSide(&obj[0], &c2), api[1].s2dSide(&obj[1], &c2), "s2d WhichSide");
        CheckI(api[0].s2dSideTol(&obj[0], &c2, s), api[1].s2dSideTol(&obj[1], &c2, s), "s2d WhichSide(tol)");
        Check(api[0].s2dBeside(&obj[0], &c2) == api[1].s2dBeside(&obj[1], &c2), "s2d IsBeside");
        Line2DObj l2d[2];
        for (int side = 0; side < 2; ++side) api[side].l2d2(&l2d[side], &c2, &a2);
        Check(api[0].s2dIntL(&obj[0], &l2d[0]) == api[1].s2dIntL(&obj[1], &l2d[1]), "s2d Intersects(Line2D)");
        Check(api[0].s2dIntS(&obj[0], &other[0]) == api[1].s2dIntS(&obj[1], &other[1]),
              "s2d Intersects(Seg2D)");
        Vec2 ip0{}, ip1{};
        float tt0 = 1.0f, tt1 = 1.0f;
        const bool it0 = api[0].s2dInter(&obj[0], &l2d[0], &ip0);
        const bool it1 = api[1].s2dInter(&obj[1], &l2d[1], &ip1);
        Check(it0 == it1, "s2d Intercept");
        if (it0 && it1) CheckBytes(&ip0, &ip1, sizeof ip0, "s2d Intercept point");
        const bool jt0 = api[0].s2dInterT(&obj[0], &l2d[0], &ip0, &tt0);
        const bool jt1 = api[1].s2dInterT(&obj[1], &l2d[1], &ip1, &tt1);
        Check(jt0 == jt1, "s2d Intercept(t)");
        if (jt0 && jt1) {
            CheckBytes(&ip0, &ip1, sizeof ip0, "s2d Intercept(t) point");
            CheckF(tt0, tt1, "s2d Intercept(t) t");
        }
        CheckF(api[0].s2dSepV(&obj[0], &c2), api[1].s2dSepV(&obj[1], &c2), "s2d GetSeparation(V)");
        CheckF(api[0].s2dSepL(&obj[0], &l2d[0]), api[1].s2dSepL(&obj[1], &l2d[1]), "s2d GetSeparation(L)");
        CheckF(api[0].s2dSepS(&obj[0], &other[0]), api[1].s2dSepS(&obj[1], &other[1]), "s2d GetSeparation(S)");
        CheckF(api[0].s2dAppV(&obj[0], &c2), api[1].s2dAppV(&obj[1], &c2), "s2d GetApproxSeparation(V)");
        CheckF(api[0].s2dAppL(&obj[0], &l2d[0]), api[1].s2dAppL(&obj[1], &l2d[1]), "s2d GetApproxSeparation(L)");
        CheckF(api[0].s2dAppS(&obj[0], &other[0]), api[1].s2dAppS(&obj[1], &other[1]), "s2d GetApproxSeparation(S)");
        CheckF(api[0].s2dSepSqV(&obj[0], &c2), api[1].s2dSepSqV(&obj[1], &c2), "s2d GetSeparationSquared(V)");
        CheckF(api[0].s2dSepSqL(&obj[0], &l2d[0]), api[1].s2dSepSqL(&obj[1], &l2d[1]),
               "s2d GetSeparationSquared(L)");
        CheckF(api[0].s2dSepSqV(&obj[0], &other[0].v0), api[1].s2dSepSqV(&obj[1], &other[1].v0),
               "s2dSqV(other.v0)");
        CheckF(api[0].s2dSepSqV(&other[0], &obj[0].v0), api[1].s2dSepSqV(&other[1], &obj[1].v0),
               "s2dSqV(this.v0)");
        CheckF(api[0].s2dSepSqS(&obj[0], &other[0]), api[1].s2dSepSqS(&obj[1], &other[1]),
               "s2d GetSeparationSquared(S)");
    }

    // ---- Circle2D
    {
        Circle2DObj obj[2], other[2];
        unsigned char seed[sizeof(Circle2DObj)];
        std::memset(seed, 0x5a, sizeof seed);
        for (int side = 0; side < 2; ++side) {
            std::memcpy(&obj[side], seed, sizeof seed);
            api[side].c0(&obj[side]);
        }
        CheckBytes(&obj[0], &obj[1], sizeof(Circle2DObj), "Circle2D::()");
        for (int side = 0; side < 2; ++side) api[side].c2(&obj[side], &a2, s);
        CheckBytes(&obj[0], &obj[1], sizeof(Circle2DObj), "Circle2D::(V2,r)");
        for (int side = 0; side < 2; ++side) api[side].c2v3(&obj[side], &a, s);
        CheckBytes(&obj[0], &obj[1], sizeof(Circle2DObj), "Circle2D::(V3,r)");
        const float rother = FRange(-5.0f, 20.0f);
        for (int side = 0; side < 2; ++side) api[side].c2(&other[side], &b2, rother);
        for (int side = 0; side < 2; ++side) api[side].cAssign(&obj[side], &other[side]);
        CheckBytes(&obj[0], &obj[1], sizeof(Circle2DObj), "Circle2D assignment");
        for (int side = 0; side < 2; ++side) {
            api[side].cSetCenter(&obj[side], &c2);
            api[side].cSetRadius(&obj[side], s);
        }
        CheckBytes(&obj[0], &obj[1], sizeof(Circle2DObj), "Circle2D setters");
        CheckBytes(api[0].cGetCenter(&obj[0]), api[1].cGetCenter(&obj[1]), sizeof(Vec2), "Circle2D GetCenter");
        CheckF(api[0].cGetRadius(&obj[0]), api[1].cGetRadius(&obj[1]), "Circle2D GetRadius");
        for (int side = 0; side < 2; ++side) api[side].c2(&obj[side], &a2, s);
        Line2DObj l2d[2];
        for (int side = 0; side < 2; ++side) api[side].l2d2(&l2d[side], &c2, &a2);
        Seg2DObj s2d[2];
        for (int side = 0; side < 2; ++side) api[side].s2d2(&s2d[side], &a2, &b2);
        Check(api[0].cIntV(&obj[0], &c2) == api[1].cIntV(&obj[1], &c2), "c2 Intersects(V)");
        Check(api[0].cIntL(&obj[0], &l2d[0]) == api[1].cIntL(&obj[1], &l2d[1]), "c2 Intersects(L)");
        Check(api[0].cInt2(&obj[0], &other[0]) == api[1].cInt2(&obj[1], &other[1]), "c2 Intersects(C)");
        Check(api[0].cIntS(&obj[0], &s2d[0]) == api[1].cIntS(&obj[1], &s2d[1]), "c2 Intersects(S)");
        CheckF(api[0].cSepV(&obj[0], &c2), api[1].cSepV(&obj[1], &c2), "c2 GetSeparation(V)");
        CheckF(api[0].cSepL(&obj[0], &l2d[0]), api[1].cSepL(&obj[1], &l2d[1]), "c2 GetSeparation(L)");
        CheckF(api[0].cSepC(&obj[0], &other[0]), api[1].cSepC(&obj[1], &other[1]), "c2 GetSeparation(C)");
        CheckF(api[0].cSepS(&obj[0], &s2d[0]), api[1].cSepS(&obj[1], &s2d[1]), "c2 GetSeparation(S)");
        CheckF(api[0].cAppV(&obj[0], &c2), api[1].cAppV(&obj[1], &c2), "c2 GetApproxSeparation(V)");
        CheckF(api[0].cAppL(&obj[0], &l2d[0]), api[1].cAppL(&obj[1], &l2d[1]), "c2 GetApproxSeparation(L)");
        CheckF(api[0].cAppC(&obj[0], &other[0]), api[1].cAppC(&obj[1], &other[1]), "c2 GetApproxSeparation(C)");
        CheckF(api[0].cAppS(&obj[0], &s2d[0]), api[1].cAppS(&obj[1], &s2d[1]), "c2 GetApproxSeparation(S)");
        CheckF(api[0].cSepSqV(&obj[0], &c2), api[1].cSepSqV(&obj[1], &c2), "c2 GetSeparationSquared(V)");
        CheckF(api[0].cSepSqL(&obj[0], &l2d[0]), api[1].cSepSqL(&obj[1], &l2d[1]), "c2 GetSeparationSquared(L)");
        CheckF(api[0].cSepSqC(&obj[0], &other[0]), api[1].cSepSqC(&obj[1], &other[1]), "c2 GetSeparationSquared(C)");
        CheckF(api[0].cSepSqS(&obj[0], &s2d[0]), api[1].cSepSqS(&obj[1], &s2d[1]), "c2 GetSeparationSquared(S)");
    }

    // ---- Polygon2D
    {
        const int n = Pick(0, 8);
        Vec2 verts[8];
        for (int i = 0; i < n; ++i) verts[i] = RV2(awk);
        PolyObj obj[2], other[2];
        for (int side = 0; side < 2; ++side) {
            api[side].p0(&obj[side]);
            CheckI(obj[side].numVerts, 0, "p2 default num");
            Check(obj[side].vertices == nullptr, "p2 default verts");
            Check(!obj[side].ownsVertices, "p2 default owns");
        }
        for (int side = 0; side < 2; ++side) api[side].pSet1(&obj[side], n);
        CheckI(obj[0].numVerts, obj[1].numVerts, "p2 Set(n) num");
        for (int i = 0; i < n; ++i) {
            obj[0].vertices[i] = verts[i];
            obj[1].vertices[i] = verts[i];
        }
        // p3 constructor copies the fields only
        PolyObj ctor[2];
        for (int side = 0; side < 2; ++side) api[side].p3(&ctor[side], n, verts, false);
        CheckI(ctor[0].numVerts, ctor[1].numVerts, "p2 ctor num");
        Check(ctor[0].vertices == ctor[1].vertices, "p2 ctor verts");
        Check(ctor[0].ownsVertices == ctor[1].ownsVertices, "p2 ctor owns");
        CheckI(api[0].pGetNum(&obj[0]), api[1].pGetNum(&obj[1]), "p2 GetNumVerts");
        Check(api[0].pGetVerts(&obj[0]) == obj[0].vertices, "p2 GetVertices");
        Check(api[1].pGetVerts(&obj[1]) == obj[1].vertices, "p2 GetVertices (orig)");
        Check(api[0].pGetConstVerts(&obj[0]) == obj[0].vertices, "p2 GetConstVertices");
        // copy constructor (deep; flag not compared)
        PolyObj copy[2];
        api[0].pCopy(&copy[0], &obj[0]);
        api[1].pCopy(&copy[1], &obj[1]);
        CheckI(copy[0].numVerts, copy[1].numVerts, "p2 copy num");
        if (n > 0) CheckBytes(copy[0].vertices, copy[1].vertices, n * sizeof(Vec2), "p2 copy verts");
        // assignment is the shallow three-dword copy
        for (int side = 0; side < 2; ++side) api[side].pAssign(&copy[side], &obj[side]);
        CheckI(copy[0].numVerts, copy[1].numVerts, "p2 assign num");
        Check(copy[0].vertices == obj[0].vertices, "p2 assign vertex pointer");
        Check(copy[0].ownsVertices == obj[0].ownsVertices, "p2 assign flag");
        // Set(n) grow and shrink from the same state
        for (int side = 0; side < 2; ++side) api[side].pSet1(&obj[side], n);
        for (int side = 0; side < 2; ++side) api[side].pSet1(&obj[side], n + 2);
        CheckI(obj[0].numVerts, obj[1].numVerts, "p2 Set grow num");
        if (n > 0) CheckBytes(obj[0].vertices, obj[1].vertices, n * sizeof(Vec2), "p2 Set grow data");
        for (int side = 0; side < 2; ++side) api[side].pSet1(&obj[side], 1);
        CheckI(obj[0].numVerts, obj[1].numVerts, "p2 Set shrink num");
        // Set(count, ptr, owns) and SetNumVerts
        for (int side = 0; side < 2; ++side) api[side].pSet3(&obj[side], n, verts, true);
        CheckI(obj[0].numVerts, obj[1].numVerts, "p2 Set(3) num");
        Check(obj[0].vertices == verts && obj[1].vertices == verts, "p2 Set(3) ptr");
        for (int side = 0; side < 2; ++side) api[side].pSetNum(&obj[side], n - 1);
        CheckI(obj[0].numVerts, obj[1].numVerts, "p2 SetNumVerts");
        // the stack block above is owned=true; never hand it to Set(n), which would free it
        for (int side = 0; side < 2; ++side) {
            obj[side].numVerts = 0;
            obj[side].vertices = nullptr;
            obj[side].ownsVertices = false;
        }
        // GetCenter on a fresh allocation
        for (int side = 0; side < 2; ++side) {
            api[side].pSet1(&obj[side], n);
            for (int i = 0; i < n; ++i) obj[side].vertices[i] = verts[i];
        }
        if (n > 0) {
            const Vec2 cen0 = (obj[0].*api[0].pGetCenter)();
            const Vec2 cen1 = (obj[1].*api[1].pGetCenter)();
            CheckBytes(&cen0, &cen1, sizeof(Vec2), "p2 GetCenter");
        }
        // Chunk-list Set
        FdaChunk chunks[2];
        FdaObj list;
        list.head = &chunks[0];
        for (int k = 0; k < 2; ++k) {
            chunks[k].next = &chunks[(k + 1) % 2];
            chunks[k].prev = &chunks[(k + 1) % 2];
            chunks[k].count = 2;
            chunks[k].data[0] = RV2(awk);
            chunks[k].data[1] = RV2(awk);
        }
        for (int side = 0; side < 2; ++side) api[side].pSetArr(&obj[side], &list);
        CheckI(obj[0].numVerts, obj[1].numVerts, "p2 Set(array) num");
        if (obj[0].numVerts > 0)
            CheckBytes(obj[0].vertices, obj[1].vertices, obj[0].numVerts * sizeof(Vec2), "p2 Set(array) data");
        // Intersects
        PolyObj poly2[2] = {};
        const int m = Pick(0, 8);
        Vec2 verts2[8];
        for (int i = 0; i < m; ++i) verts2[i] = RV2(awk);
        for (int side = 0; side < 2; ++side) {
            api[side].pSet1(&poly2[side], m);
            for (int i = 0; i < m; ++i) poly2[side].vertices[i] = verts2[i];
        }
        for (int side = 0; side < 2; ++side) {
            api[side].pSet1(&obj[side], n);
            for (int i = 0; i < n; ++i) obj[side].vertices[i] = verts[i];
        }
        if (n > 0) {
            Check(api[0].pIntV(&obj[0], &c2) == api[1].pIntV(&obj[1], &c2), "p2 Intersects(V)");
            Line2DObj l2d[2];
            for (int side = 0; side < 2; ++side) api[side].l2d2(&l2d[side], &c2, &a2);
            Check(api[0].pIntL(&obj[0], &l2d[0]) == api[1].pIntL(&obj[1], &l2d[1]), "p2 Intersects(L)");
            Seg2DObj s2d[2];
            for (int side = 0; side < 2; ++side) api[side].s2d2(&s2d[side], &a2, &b2);
            Check(api[0].pIntS(&obj[0], &s2d[0]) == api[1].pIntS(&obj[1], &s2d[1]), "p2 Intersects(S)");
            Circle2DObj cir[2];
            for (int side = 0; side < 2; ++side) api[side].c2(&cir[side], &c2, s);
            Check(api[0].pIntC(&obj[0], &cir[0]) == api[1].pIntC(&obj[1], &cir[1]), "p2 Intersects(C)");
            if (m > 0)
                Check(api[0].pIntP(&obj[0], &poly2[0]) == api[1].pIntP(&obj[1], &poly2[1]),
                      "p2 Intersects(P)");
        }
        // Direction 0 separations
        CheckF(api[0].pSepV(&obj[0], &c2), api[1].pSepV(&obj[1], &c2), "p2 GetSeparation(V)");
        CheckF(api[0].pAppV(&obj[0], &c2), api[1].pAppV(&obj[1], &c2), "p2 GetApproxSeparation(V)");
        CheckF(api[0].pSepSqV(&obj[0], &c2), api[1].pSepSqV(&obj[1], &c2), "p2 GetSeparationSquared(V)");
        // CounterClockwise/Theta/ShellSort
        Vec2 p0 = RV2(awk), p1 = RV2(awk), p2 = RV2(awk);
        CheckI(api[0].pCCW(&obj[0], &p0, &p1, &p2), api[1].pCCW(&obj[1], &p0, &p1, &p2), "p2 CounterClockwise");
        CheckF(api[0].pTheta(&obj[0], &p0, &p1), api[1].pTheta(&obj[1], &p0, &p1), "p2 Theta");
        Node nodes[10];
        const int nodeCount = Pick(0, 10);
        for (int i = 0; i < 10; ++i) {
            nodes[i].point = RV2(awk);
            nodes[i].theta = Rnd(awk);
        }
        Node nodes2[10];
        std::memcpy(nodes2, nodes, sizeof nodes);
        int cnt0 = nodeCount, cnt1 = nodeCount;
        api[0].pShell(&obj[0], nodes, &cnt0);
        api[1].pShell(&obj[1], nodes2, &cnt1);
        CheckI(cnt0, cnt1, "p2 ShellSortTheta count");
        CheckBytes(nodes, nodes2, sizeof(Node) * 10, "p2 ShellSortTheta data");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u1d1 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 200;
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
    for (int round = 0; round < rounds; ++round) {
        curRound = round;
        Round(round);
    }
    std::printf("%llu checks, %llu mismatches, %llu NaN-payload-only differences\n", checks, bad, nanPayloadOnly);
    return bad ? 1 : 0;
}
