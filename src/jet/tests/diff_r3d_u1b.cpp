// Differential test of Render3DCore_7c unit U1b: our Jet::Plane against Render3DCore_7c_orig.dll in one process.
//
// Every implemented Plane export is called through GetProcAddress on the same inputs for both modules and the raw
// bytes are compared: the whole 16-byte plane after each mutation, returned Vector3s, float bit patterns (signed
// zero, infinities, quiet and signalling NaNs included) and the enum/bool results. Clip, Split and
// Intersect(const Plane&) still forward to the original and are not tested here.
//
// usage: diff_r3d_u1b.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

struct Vec3 {
    float x, y, z;
};
struct Plane {
    Vec3 normal;
    float d;
};
struct PlaneObj {
    Vec3 normal;
    float d;
};
struct Line {
    void* vt;
    Vec3 dir;
    Vec3 point;
};
struct Segment {
    void* vt;
    Vec3 dir;
    Vec3 point;
    Vec3 end;
};
struct Sphere {
    void* vt;
    Vec3 center;
    float radius;
    float radiusXY;
};
struct Mat4 {
    float m[16];
};
enum ClipResult { FRONT = 1, BACK = 2, COINCIDENT = 3, COPLANAR = 4 };
static_assert(sizeof(Vec3) == 0xc, "Vec3");
static_assert(sizeof(Plane) == 0x10, "Plane");
static_assert(sizeof(Line) == 0x1c, "Line");
static_assert(sizeof(Segment) == 0x28, "Segment");
static_assert(sizeof(Sphere) == 0x18, "Sphere");
static_assert(sizeof(Mat4) == 0x40, "Mat4");

static HMODULE mod[2];
static unsigned long long checks, bad;
static long long reported;
static std::mt19937 rng(0x5eed);

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

// A mix of ordinary values and awkward ones: signed zero, infinities, quiet and signalling NaNs.
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
    case 7: return Bits(0xffa00001u);
    default: return FRange(-1000.0f, 1000.0f);
    }
}

static Vec3 RandomVec(bool awkward)
{
    return Vec3{awkward ? Awkward() : FRange(-100.0f, 100.0f), awkward ? Awkward() : FRange(-100.0f, 100.0f),
                awkward ? Awkward() : FRange(-100.0f, 100.0f)};
}

static Plane RandomPlane(bool awkward)
{
    return Plane{RandomVec(awkward), awkward ? Awkward() : FRange(-100.0f, 100.0f)};
}

static Mat4 RandomMat(bool awkward)
{
    Mat4 m;
    for (float& f : m.m)
        f = awkward ? Awkward() : FRange(-4.0f, 4.0f);
    if (!awkward)
        m.m[3] = m.m[7] = m.m[11] = 0.0f, m.m[12] = m.m[13] = m.m[14] = 0.0f, m.m[15] = 1.0f;
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
    static_assert(sizeof fn == sizeof raw, "member function pointer size");
    std::memcpy(&fn, &raw, sizeof fn);
    return fn;
}

static uint32_t FBits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

static bool SameBytes(const void* a, const void* b, size_t n)
{
    return std::memcmp(a, b, n) == 0;
}

static void Mismatch(const char* what)
{
    ++bad;
    if (reported < 40) {
        std::printf("MISMATCH %s (round %llu)\n", what, checks);
        ++reported;
    }
}

static void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok)
        Mismatch(what);
}

static void CheckPlane(const Plane& a, const Plane& b, const char* what)
{
    ++checks;
    if (!SameBytes(&a, &b, sizeof a)) {
        std::printf(" %s ours (%08x %08x %08x | %08x) orig (%08x %08x %08x | %08x)\n", what, FBits(a.normal.x),
                    FBits(a.normal.y), FBits(a.normal.z), FBits(a.d), FBits(b.normal.x), FBits(b.normal.y),
                    FBits(b.normal.z), FBits(b.d));
        Mismatch(what);
    }
}

static void CheckVec(const Vec3& a, const Vec3& b, const char* what)
{
    ++checks;
    if (!SameBytes(&a, &b, sizeof a)) {
        std::printf(" %s ours (%08x %08x %08x) orig (%08x %08x %08x)\n", what, FBits(a.x), FBits(a.y), FBits(a.z),
                    FBits(b.x), FBits(b.y), FBits(b.z));
        Mismatch(what);
    }
}

static void CheckFloat(float a, float b, const char* what)
{
    ++checks;
    if (FBits(a) != FBits(b)) {
        std::printf(" %s ours %08x orig %08x\n", what, FBits(a), FBits(b));
        Mismatch(what);
    }
}

static void CheckEnum(ClipResult a, ClipResult b, const char* what)
{
    ++checks;
    if (a != b)
        Mismatch(what);
}

// A member-function pointer carries the member ABI (this first, then the hidden return slot), which a free-function
// pointer does not. The class has no virtual functions, so the MSVC pointer is the entry address.
using GetPointFn = Vec3 (__cdecl PlaneObj::*)() const;
using VecArgFn = Vec3 (__cdecl PlaneObj::*)(const Vec3&) const;
using VecLineFn = Vec3 (__cdecl PlaneObj::*)(const Line&) const;
using VecLineTFn = Vec3 (__cdecl PlaneObj::*)(const Line&, float&) const;
using AxisBoxFn = ClipResult (PlaneObj::*)(const Vec3&, const Vec3&, float&, float&, float) const;

struct Api {
    void(__thiscall* ctor0)(Plane*);
    void(__thiscall* ctorCopy)(Plane*, const Plane*);
    void(__thiscall* ctorND)(Plane*, const Vec3*, float);
    void(__thiscall* ctorNP)(Plane*, const Vec3*, const Vec3*);
    void(__thiscall* ctorPPP)(Plane*, const Vec3*, const Vec3*, const Vec3*);
    Plane* (__thiscall* assign)(Plane*, const Plane*);

    void(__cdecl* setPlane)(Plane*, const Plane*);
    void(__cdecl* setPPP)(Plane*, const Vec3*, const Vec3*, const Vec3*);
    void(__cdecl* setNP)(Plane*, const Vec3*, const Vec3*);
    void(__cdecl* setND)(Plane*, const Vec3*, float);
    void(__cdecl* setPoints)(Plane*, const Vec3*);
    void(__cdecl* setDistanceP)(Plane*, const Vec3*);
    void(__cdecl* setDistanceF)(Plane*, float);
    Plane* (__cdecl* normalize)(Plane*);
    const Vec3* (__cdecl* getNormal)(const Plane*);
    void(__cdecl* setNormalV)(Plane*, const Vec3*);
    void(__cdecl* setNormalFFF)(Plane*, float, float, float);
    float(__cdecl* getDistance)(const Plane*);
    GetPointFn getPoint;

    ClipResult(__cdecl* whichSideP)(const Plane*, const Vec3*);
    ClipResult(__cdecl* whichSidePT)(const Plane*, const Vec3*, float);
    ClipResult(__cdecl* whichSideN)(const Plane*, int, const Vec3*);
    ClipResult(__cdecl* whichSideSphere)(const Plane*, const Sphere*);
    ClipResult(__cdecl* whichSideSphereC)(const Plane*, const Vec3*, float);
    AxisBoxFn axisBox;
    bool(__cdecl* isBehind)(const Plane*, const Vec3*, float);
    bool(__cdecl* isInFront)(const Plane*, const Vec3*, float);

    VecLineFn intersectLine;
    VecLineTFn intersectLineT;
    bool(__cdecl* intersectLineB)(const Plane*, bool, const Line*, Vec3*);
    bool(__cdecl* intersectLineBT)(const Plane*, bool, const Line*, Vec3*, float*);
    bool(__cdecl* intersectRayB)(const Plane*, bool, const Line*, Vec3*);
    bool(__cdecl* intersectRayBT)(const Plane*, bool, const Line*, Vec3*, float*);
    bool(__cdecl* intersectSegB)(const Plane*, bool, const Segment*, Vec3*);
    bool(__cdecl* intersectSegBT)(const Plane*, bool, const Segment*, Vec3*, float*);
    float(__cdecl* distance)(const Plane*, const Vec3*);
    VecArgFn project;

    void(__cdecl* transform)(Plane*, const Mat4*, bool, float);
    void(__cdecl* setFromTransform)(Plane*, const Mat4*, bool, const Plane*, float);
    void(__cdecl* transformPlane)(const Mat4*, bool, const Vec3*, float, Vec3*, float*, float);
    void(__cdecl* flip)(Plane*);
};

static Api Load(int side)
{
    Api a{};
    a.ctor0 = Proc<decltype(a.ctor0)>(side, "??0Plane@Jet@@QAE@XZ");
    a.ctorCopy = Proc<decltype(a.ctorCopy)>(side, "??0Plane@Jet@@QAE@ABV01@@Z");
    a.ctorND = Proc<decltype(a.ctorND)>(side, "??0Plane@Jet@@QAE@ABVVector3@1@M@Z");
    a.ctorNP = Proc<decltype(a.ctorNP)>(side, "??0Plane@Jet@@QAE@ABVVector3@1@0@Z");
    a.ctorPPP = Proc<decltype(a.ctorPPP)>(side, "??0Plane@Jet@@QAE@ABVVector3@1@00@Z");
    a.assign = Proc<decltype(a.assign)>(side, "??4Plane@Jet@@QAEAAV01@ABV01@@Z");
    a.setPlane = Proc<decltype(a.setPlane)>(side, "?Set@Plane@Jet@@QAAXABV12@@Z");
    a.setPPP = Proc<decltype(a.setPPP)>(side, "?Set@Plane@Jet@@QAAXABVVector3@2@00@Z");
    a.setNP = Proc<decltype(a.setNP)>(side, "?Set@Plane@Jet@@QAAXABVVector3@2@0@Z");
    a.setND = Proc<decltype(a.setND)>(side, "?Set@Plane@Jet@@QAAXABVVector3@2@M@Z");
    a.setPoints = Proc<decltype(a.setPoints)>(side, "?Set@Plane@Jet@@QAAXPBVVector3@2@@Z");
    a.setDistanceP = Proc<decltype(a.setDistanceP)>(side, "?SetDistance@Plane@Jet@@QAAXABVVector3@2@@Z");
    a.setDistanceF = Proc<decltype(a.setDistanceF)>(side, "?SetDistance@Plane@Jet@@QAAXM@Z");
    a.normalize = Proc<decltype(a.normalize)>(side, "?Normalize@Plane@Jet@@QAAAAV12@XZ");
    a.getNormal = Proc<decltype(a.getNormal)>(side, "?GetNormal@Plane@Jet@@QBAABVVector3@2@XZ");
    a.setNormalV = Proc<decltype(a.setNormalV)>(side, "?SetNormal@Plane@Jet@@QAAXABVVector3@2@@Z");
    a.setNormalFFF = Proc<decltype(a.setNormalFFF)>(side, "?SetNormal@Plane@Jet@@QAAXMMM@Z");
    a.getDistance = Proc<decltype(a.getDistance)>(side, "?GetDistance@Plane@Jet@@QBAMXZ");
    a.getPoint = MemberProc<GetPointFn>(side, "?GetPoint@Plane@Jet@@QBA?AVVector3@2@XZ");
    a.whichSideP = Proc<decltype(a.whichSideP)>(side, "?WhichSide@Plane@Jet@@QBA?AW4ClipResult@2@ABVVector3@2@@Z");
    a.whichSidePT = Proc<decltype(a.whichSidePT)>(side, "?WhichSide@Plane@Jet@@QBA?AW4ClipResult@2@ABVVector3@2@M@Z");
    a.whichSideN = Proc<decltype(a.whichSideN)>(side, "?WhichSide@Plane@Jet@@QBA?AW4ClipResult@2@HPBVVector3@2@@Z");
    a.whichSideSphere =
        Proc<decltype(a.whichSideSphere)>(side, "?WhichSide@Plane@Jet@@QBA?AW4ClipResult@2@ABVSphere@2@@Z");
    a.whichSideSphereC =
        Proc<decltype(a.whichSideSphereC)>(side, "?WhichSideSphere@Plane@Jet@@QBA?AW4ClipResult@2@ABVVector3@2@M@Z");
    a.axisBox = MemberProc<AxisBoxFn>(side, "?WhichSideAxisBox@Plane@Jet@@QBE?AW4ClipResult@2@ABVVector3@2@0AAM1M@Z");
    a.isBehind = Proc<decltype(a.isBehind)>(side, "?IsBehind@Plane@Jet@@QBA_NABVVector3@2@M@Z");
    a.isInFront = Proc<decltype(a.isInFront)>(side, "?IsInFront@Plane@Jet@@QBA_NABVVector3@2@M@Z");
    a.intersectLine = MemberProc<VecLineFn>(side, "?Intersect@Plane@Jet@@QBA?AVVector3@2@ABVLine@2@@Z");
    a.intersectLineT = MemberProc<VecLineTFn>(side, "?Intersect@Plane@Jet@@QBA?AVVector3@2@ABVLine@2@AAM@Z");
    a.intersectLineB =
        Proc<decltype(a.intersectLineB)>(side, "?Intersect@Plane@Jet@@QBA_N_NABVLine@2@AAVVector3@2@@Z");
    a.intersectLineBT =
        Proc<decltype(a.intersectLineBT)>(side, "?Intersect@Plane@Jet@@QBA_N_NABVLine@2@AAVVector3@2@AAM@Z");
    a.intersectRayB = Proc<decltype(a.intersectRayB)>(side, "?Intersect@Plane@Jet@@QBA_N_NABVRay@2@AAVVector3@2@@Z");
    a.intersectRayBT =
        Proc<decltype(a.intersectRayBT)>(side, "?Intersect@Plane@Jet@@QBA_N_NABVRay@2@AAVVector3@2@AAM@Z");
    a.intersectSegB =
        Proc<decltype(a.intersectSegB)>(side, "?Intersect@Plane@Jet@@QBA_N_NABVLineSegment@2@AAVVector3@2@@Z");
    a.intersectSegBT =
        Proc<decltype(a.intersectSegBT)>(side, "?Intersect@Plane@Jet@@QBA_N_NABVLineSegment@2@AAVVector3@2@AAM@Z");
    a.distance = Proc<decltype(a.distance)>(side, "?Distance@Plane@Jet@@QBAMABVVector3@2@@Z");
    a.project = MemberProc<VecArgFn>(side, "?Project@Plane@Jet@@QBA?AVVector3@2@ABV32@@Z");
    a.transform = Proc<decltype(a.transform)>(side, "?Transform@Plane@Jet@@QAAXABVMatrix4@2@_NM@Z");
    a.setFromTransform =
        Proc<decltype(a.setFromTransform)>(side, "?SetFromTransformPlane@Plane@Jet@@QAAXABVMatrix4@2@_NABV12@M@Z");
    a.transformPlane = Proc<decltype(a.transformPlane)>(side,
                                                        "?TransformPlane@Plane@Jet@@SAXABVMatrix4@2@_NABVVector3@2@MAAV42@AAMM@Z");
    a.flip = Proc<decltype(a.flip)>(side, "?Flip@Plane@Jet@@QAAXXZ");
    return a;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u1b <bin dir> [rounds]\n");
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
    Api api[2] = {Load(0), Load(1)};

    Plane planes[2][6];
    PlaneObj* objs[2][6];
    for (int side = 0; side < 2; ++side)
        for (int i = 0; i < 6; ++i)
            objs[side][i] = reinterpret_cast<PlaneObj*>(&planes[side][i]);

    for (int round = 0; round < rounds; ++round) {
        const bool awkward = (round % 7) == 0;
        const Plane a = RandomPlane(awkward);
        const Plane b = RandomPlane(awkward);
        const Vec3 v1 = RandomVec(awkward), v2 = RandomVec(awkward), v3 = RandomVec(awkward);
        const Vec3 q = RandomVec(awkward);
        const float thr = awkward ? Awkward() : FRange(-5.0f, 5.0f);

        // default ctor: must leave the storage untouched (the original is a bare ret)
        {
            unsigned char seed[2][sizeof(Plane)];
            for (int side = 0; side < 2; ++side)
                for (unsigned& w : *reinterpret_cast<unsigned(*)[4]>(seed[side]))
                    w = 0x7fc00000u;
            for (int side = 0; side < 2; ++side) {
                std::memcpy(&planes[side][0], seed[side], sizeof(Plane));
                api[side].ctor0(&planes[side][0]);
            }
            CheckPlane(planes[0][0], planes[1][0], "default ctor");
        }

        // constructors
        for (int side = 0; side < 2; ++side) api[side].ctorND(&planes[side][0], &a.normal, a.d);
        CheckPlane(planes[0][0], planes[1][0], "ctor(normal,d)");
        for (int side = 0; side < 2; ++side) api[side].ctorNP(&planes[side][0], &a.normal, &q);
        CheckPlane(planes[0][0], planes[1][0], "ctor(normal,point)");
        for (int side = 0; side < 2; ++side) api[side].ctorPPP(&planes[side][0], &v1, &v2, &v3);
        CheckPlane(planes[0][0], planes[1][0], "ctor(3 points)");
        for (int side = 0; side < 2; ++side) {
            api[side].ctorND(&planes[side][1], &b.normal, b.d);
            api[side].ctorCopy(&planes[side][0], &planes[side][1]);
        }
        CheckPlane(planes[0][0], planes[1][0], "copy ctor");
        for (int side = 0; side < 2; ++side) {
            api[side].ctorND(&planes[side][2], &a.normal, a.d);
            api[side].assign(&planes[side][0], &planes[side][2]);
        }
        CheckPlane(planes[0][0], planes[1][0], "assignment");

        // Set overloads
        for (int side = 0; side < 2; ++side) {
            api[side].ctorND(&planes[side][0], &a.normal, a.d);
            api[side].ctorND(&planes[side][1], &b.normal, b.d);
            api[side].setPlane(&planes[side][0], &planes[side][1]);
        }
        CheckPlane(planes[0][0], planes[1][0], "Set(Plane)");
        for (int side = 0; side < 2; ++side) api[side].setNP(&planes[side][0], &a.normal, &q);
        CheckPlane(planes[0][0], planes[1][0], "Set(normal,point)");
        for (int side = 0; side < 2; ++side) api[side].setND(&planes[side][0], &a.normal, thr);
        CheckPlane(planes[0][0], planes[1][0], "Set(normal,d)");
        for (int side = 0; side < 2; ++side) api[side].setPPP(&planes[side][0], &v1, &v2, &v3);
        CheckPlane(planes[0][0], planes[1][0], "Set(3 points)");
        const Vec3 pts[3] = {v1, v2, v3};
        for (int side = 0; side < 2; ++side) api[side].setPoints(&planes[side][0], pts);
        CheckPlane(planes[0][0], planes[1][0], "Set(3 point array)");

        // aliasing forms
        for (int side = 0; side < 2; ++side) {
            api[side].ctorND(&planes[side][0], &a.normal, a.d);
            api[side].setNP(&planes[side][0], &planes[side][0].normal, &planes[side][0].normal);
        }
        CheckPlane(planes[0][0], planes[1][0], "Set(normal,normal) alias");
        for (int side = 0; side < 2; ++side) {
            api[side].ctorND(&planes[side][0], &a.normal, a.d);
            api[side].setPPP(&planes[side][0], &planes[side][0].normal, &v2, &v3);
        }
        CheckPlane(planes[0][0], planes[1][0], "Set(p1 alias)");

        // SetDistance / SetNormal
        for (int side = 0; side < 2; ++side) api[side].setDistanceP(&planes[side][0], &q);
        CheckPlane(planes[0][0], planes[1][0], "SetDistance(point)");
        for (int side = 0; side < 2; ++side) api[side].setDistanceF(&planes[side][0], thr);
        CheckPlane(planes[0][0], planes[1][0], "SetDistance(float)");
        for (int side = 0; side < 2; ++side) api[side].setNormalV(&planes[side][0], &q);
        CheckPlane(planes[0][0], planes[1][0], "SetNormal(vector)");
        for (int side = 0; side < 2; ++side) api[side].setNormalFFF(&planes[side][0], q.x, q.y, q.z);
        CheckPlane(planes[0][0], planes[1][0], "SetNormal(fff)");

        // getters
        CheckVec(*api[0].getNormal(&planes[0][0]), *api[1].getNormal(&planes[1][0]), "GetNormal");
        CheckFloat(api[0].getDistance(&planes[0][0]), api[1].getDistance(&planes[1][0]), "GetDistance");
        {
            const Vec3 p0 = (objs[0][0]->*api[0].getPoint)();
            const Vec3 p1 = (objs[1][0]->*api[1].getPoint)();
            CheckVec(p0, p1, "GetPoint");
        }

        // Normalize (random, and the degenerate planes)
        for (int side = 0; side < 2; ++side) api[side].ctorND(&planes[side][0], &a.normal, a.d);
        for (int side = 0; side < 2; ++side) api[side].normalize(&planes[side][0]);
        CheckPlane(planes[0][0], planes[1][0], "Normalize");
        {
            const Vec3 degenerate[5] = {
                Vec3{0.0f, 0.0f, 0.0f}, Vec3{-0.0f, -0.0f, -0.0f}, Vec3{Bits(0x7f800000u), 1.0f, 2.0f},
                Vec3{Bits(0x7fc00000u), 1.0f, 2.0f}, Vec3{Bits(0x7fa00000u), 3.0f, 4.0f}};
            for (const Vec3& n : degenerate) {
                for (int side = 0; side < 2; ++side) api[side].ctorND(&planes[side][0], &n, thr);
                for (int side = 0; side < 2; ++side) api[side].normalize(&planes[side][0]);
                CheckPlane(planes[0][0], planes[1][0], "Normalize degenerate");
            }
        }

        // Flip
        for (int side = 0; side < 2; ++side) api[side].ctorND(&planes[side][0], &a.normal, a.d);
        for (int side = 0; side < 2; ++side) api[side].flip(&planes[side][0]);
        CheckPlane(planes[0][0], planes[1][0], "Flip");

        // WhichSide variants
        CheckEnum(api[0].whichSideP(&planes[0][0], &q), api[1].whichSideP(&planes[1][0], &q), "WhichSide(point)");
        CheckEnum(api[0].whichSidePT(&planes[0][0], &q, thr), api[1].whichSidePT(&planes[1][0], &q, thr),
                  "WhichSide(point,threshold)");
        {
            Sphere s{};
            s.center = q;
            s.radius = thr;
            s.radiusXY = thr;
            CheckEnum(api[0].whichSideSphere(&planes[0][0], &s), api[1].whichSideSphere(&planes[1][0], &s),
                      "WhichSide(Sphere)");
            CheckEnum(api[0].whichSideSphereC(&planes[0][0], &q, thr), api[1].whichSideSphereC(&planes[1][0], &q, thr),
                      "WhichSideSphere");
        }
        {
            Vec3 verts[5];
            for (Vec3& v : verts)
                v = RandomVec(awkward);
            for (int n : {0, 1, 2, 5, -1}) {
                CheckEnum(api[0].whichSideN(&planes[0][0], n, verts), api[1].whichSideN(&planes[1][0], n, verts),
                          "WhichSide(vertices)");
            }
        }
        {
            float pd[2] = {1234.5f, 1234.5f}, rad[2] = {-4321.5f, -4321.5f};
            const Vec3 ext = RandomVec(awkward);
            ClipResult r0 = (objs[0][0]->*api[0].axisBox)(q, ext, pd[0], rad[0], thr);
            ClipResult r1 = (objs[1][0]->*api[1].axisBox)(q, ext, pd[1], rad[1], thr);
            CheckEnum(r0, r1, "WhichSideAxisBox");
            CheckFloat(pd[0], pd[1], "WhichSideAxisBox distance");
            CheckFloat(rad[0], rad[1], "WhichSideAxisBox radius");
        }
        Check(api[0].isBehind(&planes[0][0], &q, thr) == api[1].isBehind(&planes[1][0], &q, thr), "IsBehind");
        Check(api[0].isInFront(&planes[0][0], &q, thr) == api[1].isInFront(&planes[1][0], &q, thr), "IsInFront");

        // Distance
        CheckFloat(api[0].distance(&planes[0][0], &q), api[1].distance(&planes[1][0], &q), "Distance");

        // Intersect: infinite line
        Line line;
        std::memset(&line, 0, sizeof line);
        line.dir = v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f ? Vec3{1.0f, 0.0f, 0.0f} : v1;
        line.point = q;
        {
            const Vec3 r0 = (objs[0][0]->*api[0].intersectLine)(line);
            const Vec3 r1 = (objs[1][0]->*api[1].intersectLine)(line);
            CheckVec(r0, r1, "Intersect(Line)");
            float t0 = 111.5f, t1 = 111.5f;
            const Vec3 s0 = (objs[0][0]->*api[0].intersectLineT)(line, t0);
            const Vec3 s1 = (objs[1][0]->*api[1].intersectLineT)(line, t1);
            CheckVec(s0, s1, "Intersect(Line,t)");
            CheckFloat(t0, t1, "Intersect(Line,t) t");
        }

        // Intersect: bool line / ray / segment, one- and two-sided
        Segment seg;
        std::memset(&seg, 0, sizeof seg);
        seg.dir = line.dir;
        seg.point = line.point;
        seg.end = RandomVec(awkward);
        for (bool twoSided : {false, true}) {
            {
                Vec3 o0{9.0f, 9.0f, 9.0f}, o1{9.0f, 9.0f, 9.0f};
                Check(api[0].intersectLineB(&planes[0][0], twoSided, &line, &o0) ==
                          api[1].intersectLineB(&planes[1][0], twoSided, &line, &o1),
                      "Intersect(bool,Line,out)");
                CheckVec(o0, o1, "Intersect(bool,Line,out) out");
                float t0 = 77.25f, t1 = 77.25f;
                Check(api[0].intersectLineBT(&planes[0][0], twoSided, &line, &o0, &t0) ==
                          api[1].intersectLineBT(&planes[1][0], twoSided, &line, &o1, &t1),
                      "Intersect(bool,Line,out,t)");
                CheckVec(o0, o1, "Intersect(bool,Line,out,t) out");
                CheckFloat(t0, t1, "Intersect(bool,Line,out,t) t");
            }
            {
                Vec3 o0{-9.0f, -9.0f, -9.0f}, o1{-9.0f, -9.0f, -9.0f};
                Check(api[0].intersectRayB(&planes[0][0], twoSided, &line, &o0) ==
                          api[1].intersectRayB(&planes[1][0], twoSided, &line, &o1),
                      "Intersect(bool,Ray,out)");
                CheckVec(o0, o1, "Intersect(bool,Ray,out) out");
                float t0 = -55.0f, t1 = -55.0f;
                Check(api[0].intersectRayBT(&planes[0][0], twoSided, &line, &o0, &t0) ==
                          api[1].intersectRayBT(&planes[1][0], twoSided, &line, &o1, &t1),
                      "Intersect(bool,Ray,out,t)");
                CheckVec(o0, o1, "Intersect(bool,Ray,out,t) out");
                CheckFloat(t0, t1, "Intersect(bool,Ray,out,t) t");
            }
            {
                Vec3 o0{1000.0f, 1000.0f, 1000.0f}, o1{1000.0f, 1000.0f, 1000.0f};
                Check(api[0].intersectSegB(&planes[0][0], twoSided, &seg, &o0) ==
                          api[1].intersectSegB(&planes[1][0], twoSided, &seg, &o1),
                      "Intersect(bool,Segment,out)");
                CheckVec(o0, o1, "Intersect(bool,Segment,out) out");
                float t0 = 3.0f, t1 = 3.0f;
                Check(api[0].intersectSegBT(&planes[0][0], twoSided, &seg, &o0, &t0) ==
                          api[1].intersectSegBT(&planes[1][0], twoSided, &seg, &o1, &t1),
                      "Intersect(bool,Segment,out,t)");
                CheckVec(o0, o1, "Intersect(bool,Segment,out,t) out");
                CheckFloat(t0, t1, "Intersect(bool,Segment,out,t) t");
            }
        }

        // Project
        {
            const Vec3 r0 = (objs[0][0]->*api[0].project)(q);
            const Vec3 r1 = (objs[1][0]->*api[1].project)(q);
            CheckVec(r0, r1, "Project");
        }

        // Transform / SetFromTransformPlane / TransformPlane
        const Mat4 m = RandomMat(awkward);
        for (bool transform : {false, true}) {
            for (int side = 0; side < 2; ++side) api[side].ctorND(&planes[side][0], &a.normal, a.d);
            for (int side = 0; side < 2; ++side) api[side].transform(&planes[side][0], &m, transform, thr);
            CheckPlane(planes[0][0], planes[1][0], "Transform");

            for (int side = 0; side < 2; ++side) {
                api[side].ctorND(&planes[side][0], &a.normal, a.d);
                api[side].ctorND(&planes[side][1], &b.normal, b.d);
                api[side].setFromTransform(&planes[side][0], &m, transform, &planes[side][1], thr);
            }
            CheckPlane(planes[0][0], planes[1][0], "SetFromTransformPlane");

            // source aliases the destination
            for (int side = 0; side < 2; ++side) {
                api[side].ctorND(&planes[side][0], &a.normal, a.d);
                api[side].setFromTransform(&planes[side][0], &m, transform, &planes[side][0], thr);
            }
            CheckPlane(planes[0][0], planes[1][0], "SetFromTransformPlane alias");

            // the static, with a separate out and with the out aliasing the input
            {
                Vec3 in0 = a.normal, in1 = a.normal;
                Vec3 out0{1.0f, 2.0f, 3.0f}, out1{1.0f, 2.0f, 3.0f};
                float od0 = 8.0f, od1 = 8.0f;
                api[0].transformPlane(&m, transform, &in0, a.d, &out0, &od0, thr);
                api[1].transformPlane(&m, transform, &in1, a.d, &out1, &od1, thr);
                CheckVec(out0, out1, "TransformPlane");
                CheckFloat(od0, od1, "TransformPlane d");
                Vec3 io0 = a.normal, io1 = a.normal;
                float iod0 = 8.0f, iod1 = 8.0f;
                api[0].transformPlane(&m, transform, &io0, a.d, &io0, &iod0, thr);
                api[1].transformPlane(&m, transform, &io1, a.d, &io1, &iod1, thr);
                CheckVec(io0, io1, "TransformPlane alias");
                CheckFloat(iod0, iod1, "TransformPlane alias d");
            }
        }
    }

    // directed cases: the epsilon band of the bool Intersect tests, segment bounds, exact matrices
    {
        const Vec3 nrm{2.0f, -3.0f, 5.0f};
        const float dd = 7.0f;
        for (int side = 0; side < 2; ++side) api[side].ctorND(&planes[side][0], &nrm, dd);
        Line l;
        std::memset(&l, 0, sizeof l);
        l.point = Vec3{1.0f, 2.0f, 3.0f};
        const Vec3 dirs[7] = {nrm,
                              Vec3{-nrm.x, -nrm.y, -nrm.z},
                              Vec3{0.0f, 0.0f, 0.0f},
                              Vec3{1.0f, 0.0f, 0.0f},
                              Vec3{Bits(0x7f800000u), 0.0f, 0.0f},
                              Vec3{Bits(0x7fc00000u), 1.0f, 2.0f},
                              Vec3{1.1920929e-07f, 0.0f, 0.0f}};
        const Vec3 ends[5] = {Vec3{1.0f, 2.0f, 3.0f}, Vec3{2.0f, 4.0f, 6.0f}, Vec3{1.5f, 3.0f, 4.5f},
                              Vec3{Bits(0x7fc00000u), 2.0f, 3.0f}, Vec3{1.0f, 2.0f, 3.0f}};
        for (const Vec3& d : dirs) {
            l.dir = d;
            for (bool twoSided : {false, true}) {
                Vec3 o0{5.0f, 6.0f, 7.0f}, o1{5.0f, 6.0f, 7.0f};
                Check(api[0].intersectLineB(&planes[0][0], twoSided, &l, &o0) ==
                          api[1].intersectLineB(&planes[1][0], twoSided, &l, &o1),
                      "directed bool Line");
                float t0 = 0.5f, t1 = 0.5f;
                Check(api[0].intersectLineBT(&planes[0][0], twoSided, &l, &o0, &t0) ==
                          api[1].intersectLineBT(&planes[1][0], twoSided, &l, &o1, &t1),
                      "directed bool Line t");
                CheckVec(o0, o1, "directed bool Line out");
                CheckFloat(t0, t1, "directed bool Line t value");
                Check(api[0].intersectRayBT(&planes[0][0], twoSided, &l, &o0, &t0) ==
                          api[1].intersectRayBT(&planes[1][0], twoSided, &l, &o1, &t1),
                      "directed bool Ray t");
                CheckVec(o0, o1, "directed bool Ray out");
                CheckFloat(t0, t1, "directed bool Ray t value");
                for (const Vec3& e : ends) {
                    Segment s;
                    std::memset(&s, 0, sizeof s);
                    s.dir = d;
                    s.point = l.point;
                    s.end = e;
                    Check(api[0].intersectSegBT(&planes[0][0], twoSided, &s, &o0, &t0) ==
                              api[1].intersectSegBT(&planes[1][0], twoSided, &s, &o1, &t1),
                          "directed bool Segment t");
                    CheckVec(o0, o1, "directed bool Segment out");
                    CheckFloat(t0, t1, "directed bool Segment t value");
                }
            }
        }

        const Mat4 idmat = [] {
            Mat4 m{};
            m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
            return m;
        }();
        Mat4 trmat = idmat;
        trmat.m[12] = 10.0f;
        trmat.m[13] = -20.0f;
        trmat.m[14] = 30.0f;
        const Mat4* mats[2] = {&idmat, &trmat};
        for (const Mat4* mp : mats) {
            for (bool transform : {false, true}) {
                for (int side = 0; side < 2; ++side) api[side].ctorND(&planes[side][0], &nrm, dd);
                for (int side = 0; side < 2; ++side) api[side].transform(&planes[side][0], mp, transform, 1.5f);
                CheckPlane(planes[0][0], planes[1][0], "directed Transform");
                Vec3 out0{0.0f, 0.0f, 0.0f}, out1{0.0f, 0.0f, 0.0f};
                float od0 = 0.0f, od1 = 0.0f;
                api[0].transformPlane(mp, transform, &nrm, dd, &out0, &od0, 1.5f);
                api[1].transformPlane(mp, transform, &nrm, dd, &out1, &od1, 1.5f);
                CheckVec(out0, out1, "directed TransformPlane");
                CheckFloat(od0, od1, "directed TransformPlane d");
            }
        }
    }

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
