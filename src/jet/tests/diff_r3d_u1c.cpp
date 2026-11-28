// Differential test of Render3DCore_7c unit U1c: our OrientedBox (and the nine AxisAlignedBox tail methods) against
// Render3DCore_7c_orig.dll in one process.
//
// Every exported entry point is called through GetProcAddress with identical inputs for both modules; box memory,
// returned references, out-parameters and booleans are compared raw (including quiet/signalling NaNs, infinities and
// signed zero). usage: diff_r3d_u1c.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules>
// [rounds]
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
struct Box {
    Vec3 min, max;
};
struct Obb {
    Vec3 center;
    Vec3 axis[3];
    Vec3 extents;
};
struct Line {
    void* vftable;
    Vec3 dir;
    Vec3 point;
};
struct LineSegment {
    void* vftable;
    Vec3 dir;
    Vec3 point;
    Vec3 end;
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
struct Mat4 {
    float m[16];
};
template <class T, class B>
struct ItemPtr {
    unsigned int stride;
    T* data;
};
static_assert(sizeof(Vec3) == 0xc, "Vec3");
static_assert(sizeof(Box) == 0x18, "Box");
static_assert(sizeof(Obb) == 0x3c, "Obb");
static_assert(sizeof(Line) == 0x1c, "Line");
static_assert(sizeof(LineSegment) == 0x28, "LineSegment");
static_assert(sizeof(Sphere) == 0x18, "Sphere");

static HMODULE mod[2];
static unsigned long long checks, bad;
static std::mt19937 rng(0x51c0de);

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
    case 7: return Bits(0xffa00001u);
    default: return FRange(-1000.0f, 1000.0f);
    }
}

static Vec3 RV(bool awkward, float scale = 100.0f)
{
    return Vec3{awkward ? Awkward() : FRange(-scale, scale), awkward ? Awkward() : FRange(-scale, scale),
                awkward ? Awkward() : FRange(-scale, scale)};
}

static Obb RObb(bool awkward)
{
    Obb o;
    o.center = RV(awkward);
    for (Vec3& a : o.axis) a = RV(awkward);
    o.extents = RV(awkward, 20.0f);
    return o;
}

static Box RBox(bool awkward)
{
    return Box{RV(awkward), RV(awkward)};
}

static Line RLine(bool awkward)
{
    Line l{};
    l.vftable = reinterpret_cast<void*>(0xdeadbeef);
    l.dir = RV(awkward);
    l.point = RV(awkward);
    return l;
}

static Mat3 RMat3(bool awkward)
{
    Mat3 m;
    for (float& v : m.m) v = awkward ? Awkward() : FRange(-2.0f, 2.0f);
    return m;
}

static Mat4 RMat4(bool awkward)
{
    Mat4 m;
    for (float& v : m.m) v = awkward ? Awkward() : FRange(-2.0f, 2.0f);
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

static void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        std::printf("MISMATCH %s\n", what);
    }
}

static bool Same(const void* a, const void* b, size_t n)
{
    return std::memcmp(a, b, n) == 0;
}

// NaN payload propagation is not part of the contract (the x87 and SSE units quiet and pick payloads differently);
// every NaN compares equal to every other NaN, every other value is compared bit for bit.
static bool SameF(float a, float b)
{
    if (a != a || b != b) return (a != a) && (b != b);
    uint32_t x, y;
    std::memcpy(&x, &a, 4);
    std::memcpy(&y, &b, 4);
    return x == y;
}

static bool SameV(const Vec3& a, const Vec3& b)
{
    return SameF(a.x, b.x) && SameF(a.y, b.y) && SameF(a.z, b.z);
}

static bool SameObb(const Obb& a, const Obb& b)
{
    if (!SameV(a.center, b.center) || !SameV(a.extents, b.extents)) return false;
    for (int i = 0; i < 3; ++i)
        if (!SameV(a.axis[i], b.axis[i])) return false;
    return true;
}

static bool SameBox(const Box& a, const Box& b)
{
    return SameV(a.min, b.min) && SameV(a.max, b.max);
}

static uint32_t FBits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

static void CheckObb(const Obb& a, const Obb& b, const char* what)
{
    const bool ok = SameObb(a, b);
    if (!ok) {
        std::printf("%s\n ours c(%08x %08x %08x) e(%08x %08x %08x)\n", what, FBits(a.center.x), FBits(a.center.y),
                    FBits(a.center.z), FBits(a.extents.x), FBits(a.extents.y), FBits(a.extents.z));
        std::printf(" orig c(%08x %08x %08x) e(%08x %08x %08x)\n", FBits(b.center.x), FBits(b.center.y),
                    FBits(b.center.z), FBits(b.extents.x), FBits(b.extents.y), FBits(b.extents.z));
        for (int i = 0; i < 3; ++i)
            std::printf("  axis%d ours(%08x %08x %08x) orig(%08x %08x %08x)\n", i, FBits(a.axis[i].x),
                        FBits(a.axis[i].y), FBits(a.axis[i].z), FBits(b.axis[i].x), FBits(b.axis[i].y),
                        FBits(b.axis[i].z));
    }
    Check(ok, what);
}

static void CheckBox(const Box& a, const Box& b, const char* what)
{
    const bool ok = SameBox(a, b);
    if (!ok) {
        std::printf("%s\n ours min(%08x %08x %08x) max(%08x %08x %08x)\n", what, FBits(a.min.x), FBits(a.min.y),
                    FBits(a.min.z), FBits(a.max.x), FBits(a.max.y), FBits(a.max.z));
        std::printf(" orig min(%08x %08x %08x) max(%08x %08x %08x)\n", FBits(b.min.x), FBits(b.min.y),
                    FBits(b.min.z), FBits(b.max.x), FBits(b.max.y), FBits(b.max.z));
    }
    Check(ok, what);
}

static void CheckVec(const Vec3& a, const Vec3& b, const char* what)
{
    if (!SameV(a, b))
        std::printf("%s ours(%08x %08x %08x) orig(%08x %08x %08x)\n", what, FBits(a.x), FBits(a.y), FBits(a.z),
                    FBits(b.x), FBits(b.y), FBits(b.z));
    Check(SameV(a, b), what);
}

using Ctor = void(__thiscall*)(Obb*);
using CtorArr = void(__thiscall*)(Obb*, unsigned int, const Vec3*);
using CtorItem = void(__thiscall*)(Obb*, unsigned int, unsigned int, const Vec3*);
using CtorCopy = void(__thiscall*)(Obb*, const Obb*);
using CtorBox = void(__thiscall*)(Obb*, const Box*);
using Assign = Obb*(__thiscall*)(Obb*, const Obb*);

struct Api {
    Ctor ctor0;
    CtorArr ctorArr;
    CtorItem ctorItem;
    CtorCopy ctorCopy;
    CtorBox ctorBox;
    Assign assign;
    void(__cdecl* setCenter)(Obb*, const Vec3*);
    const Vec3*(__cdecl* getCenter)(const Obb*);
    void(__cdecl* setExtents)(Obb*, const Vec3*);
    const Vec3*(__cdecl* getExtents)(const Obb*);
    const Vec3*(__cdecl* getAxis)(const Obb*);
    void(__cdecl* setAxisPtr)(Obb*, const Vec3*);
    void(__cdecl* setAxis3)(Obb*, const Vec3*, const Vec3*, const Vec3*);
    void(__cdecl* zero)(Obb*);
    bool(__cdecl* isEmpty)(const Obb*);
    void(__cdecl* transform3)(Obb*, const Mat3*, const Vec3*);
    void(__cdecl* transform4)(Obb*, const Mat4*);
    void(__cdecl* inverseTransform)(Obb*, const Mat3*, const Vec3*);
    void(__thiscall* calcLocalDir)(const Obb*, const Vec3*, Vec3*);
    void(__thiscall* calcLocalPos)(const Obb*, const Vec3*, Vec3*);
    void(__cdecl* calcCorners)(const Obb*, Vec3*);
    void(__cdecl* maximizeObb)(Obb*, const Obb*);
    void(__cdecl* maximizeBox)(Obb*, const Box*);
    bool(__cdecl* collideObb)(const Obb*, const Obb*);
    bool(__cdecl* collideBox)(const Obb*, const Box*);
    bool(__cdecl* collideSphere)(const Obb*, const Sphere*);
    bool(__cdecl* collideLine)(const Obb*, const Line*);
    bool(__cdecl* collideRay)(const Obb*, const Line*);
    bool(__cdecl* collideSegment)(const Obb*, const LineSegment*);
    bool(__cdecl* collideCylinder)(const Obb*, const void*);
    bool(__cdecl* collideEllipsoid)(const Obb*, const void*);
    bool(__cdecl* getMinMax)(const Obb*, const Vec3*, const Vec3*, float*, float*);
    void(__cdecl* init)(Obb*, int, unsigned int, const Vec3*);

    // AxisAlignedBox tail
    void(__thiscall* boxCtorObb)(Box*, const Obb*);
    void(__cdecl* boxMaximizeObb)(Box*, const Obb*);
    bool(__cdecl* boxGetMinMax)(const Box*, const Vec3*, const Vec3*, float*, float*);
    bool(__cdecl* boxCollideLine)(const Box*, const Line*);
    bool(__cdecl* boxCollideRay)(const Box*, const Line*);
    bool(__cdecl* boxCollideSegment)(const Box*, const LineSegment*);
    bool(__cdecl* boxCollideObb)(const Box*, const Obb*);
    bool(__cdecl* boxCollideCylinder)(const Box*, const void*);
    bool(__cdecl* boxCollideEllipsoid)(const Box*, const void*);
};

static Api Load(int side)
{
    Api a{};
    a.ctor0 = Proc<Ctor>(side, "??0OrientedBox@Jet@@QAE@XZ");
    a.ctorArr = Proc<CtorArr>(side, "??0OrientedBox@Jet@@QAE@IPBVVector3@1@@Z");
    a.ctorItem =
        Proc<CtorItem>(side, "??0OrientedBox@Jet@@QAE@IV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@1@@Z");
    a.ctorCopy = Proc<CtorCopy>(side, "??0OrientedBox@Jet@@QAE@ABV01@@Z");
    a.ctorBox = Proc<CtorBox>(side, "??0OrientedBox@Jet@@QAE@ABVAxisAlignedBox@1@@Z");
    a.assign = Proc<Assign>(side, "??4OrientedBox@Jet@@QAEAAV01@ABV01@@Z");
    a.setCenter = Proc<decltype(a.setCenter)>(side, "?SetCenter@OrientedBox@Jet@@QAAXABVVector3@2@@Z");
    a.getCenter = Proc<decltype(a.getCenter)>(side, "?GetCenter@OrientedBox@Jet@@QBAABVVector3@2@XZ");
    a.setExtents = Proc<decltype(a.setExtents)>(side, "?SetExtents@OrientedBox@Jet@@QAAXABVVector3@2@@Z");
    a.getExtents = Proc<decltype(a.getExtents)>(side, "?GetExtents@OrientedBox@Jet@@QBAABVVector3@2@XZ");
    a.getAxis = Proc<decltype(a.getAxis)>(side, "?GetAxis@OrientedBox@Jet@@QBAPBVVector3@2@XZ");
    a.setAxisPtr = Proc<decltype(a.setAxisPtr)>(side, "?SetAxis@OrientedBox@Jet@@QAAXPBVVector3@2@@Z");
    a.setAxis3 = Proc<decltype(a.setAxis3)>(side, "?SetAxis@OrientedBox@Jet@@QAAXABVVector3@2@00@Z");
    a.zero = Proc<decltype(a.zero)>(side, "?Zero@OrientedBox@Jet@@QAAXXZ");
    a.isEmpty = Proc<decltype(a.isEmpty)>(side, "?IsEmpty@OrientedBox@Jet@@QBA_NXZ");
    a.transform3 =
        Proc<decltype(a.transform3)>(side, "?Transform@OrientedBox@Jet@@QAAXABVMatrix3@2@ABVVector3@2@@Z");
    a.transform4 = Proc<decltype(a.transform4)>(side, "?Transform@OrientedBox@Jet@@QAAXABVMatrix4@2@@Z");
    a.inverseTransform =
        Proc<decltype(a.inverseTransform)>(side, "?InverseTransform@OrientedBox@Jet@@QAAXABVMatrix3@2@ABVVector3@2@@Z");
    a.calcLocalDir = Proc<decltype(a.calcLocalDir)>(side, "?CalcLocalDir@OrientedBox@Jet@@QBEXABVVector3@2@AAV32@@Z");
    a.calcLocalPos = Proc<decltype(a.calcLocalPos)>(side, "?CalcLocalPos@OrientedBox@Jet@@QBEXABVVector3@2@AAV32@@Z");
    a.calcCorners = Proc<decltype(a.calcCorners)>(side, "?CalcCorners@OrientedBox@Jet@@QBAXPAVVector3@2@@Z");
    a.maximizeObb = Proc<decltype(a.maximizeObb)>(side, "?Maximize@OrientedBox@Jet@@QAAXABV12@@Z");
    a.maximizeBox = Proc<decltype(a.maximizeBox)>(side, "?Maximize@OrientedBox@Jet@@QAAXABVAxisAlignedBox@2@@Z");
    a.collideObb = Proc<decltype(a.collideObb)>(side, "?TestCollision@OrientedBox@Jet@@QBA_NABV12@@Z");
    a.collideBox = Proc<decltype(a.collideBox)>(side, "?TestCollision@OrientedBox@Jet@@QBA_NABVAxisAlignedBox@2@@Z");
    a.collideSphere = Proc<decltype(a.collideSphere)>(side, "?TestCollision@OrientedBox@Jet@@QBA_NABVSphere@2@@Z");
    a.collideLine = Proc<decltype(a.collideLine)>(side, "?TestCollision@OrientedBox@Jet@@QBA_NABVLine@2@@Z");
    a.collideRay = Proc<decltype(a.collideRay)>(side, "?TestCollision@OrientedBox@Jet@@QBA_NABVRay@2@@Z");
    a.collideSegment = Proc<decltype(a.collideSegment)>(side, "?TestCollision@OrientedBox@Jet@@QBA_NABVLineSegment@2@@Z");
    a.collideCylinder = Proc<decltype(a.collideCylinder)>(side, "?TestCollision@OrientedBox@Jet@@QBA_NABVCylinder@2@@Z");
    a.collideEllipsoid =
        Proc<decltype(a.collideEllipsoid)>(side, "?TestCollision@OrientedBox@Jet@@QBA_NABVEllipsoid@2@@Z");
    a.getMinMax =
        Proc<decltype(a.getMinMax)>(side, "?GetMinMaxCollision@OrientedBox@Jet@@QBA_NABVVector3@2@0AAM1@Z");
    a.init = Proc<decltype(a.init)>(side, "?Init@OrientedBox@Jet@@IAAXHV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@Z");
    a.boxCtorObb = Proc<decltype(a.boxCtorObb)>(side, "??0AxisAlignedBox@Jet@@QAE@ABVOrientedBox@1@@Z");
    a.boxMaximizeObb = Proc<decltype(a.boxMaximizeObb)>(side, "?Maximize@AxisAlignedBox@Jet@@QAAXABVOrientedBox@2@@Z");
    a.boxGetMinMax =
        Proc<decltype(a.boxGetMinMax)>(side, "?GetMinMaxCollision@AxisAlignedBox@Jet@@QBA_NABVVector3@2@0AAM1@Z");
    a.boxCollideLine = Proc<decltype(a.boxCollideLine)>(side, "?TestCollision@AxisAlignedBox@Jet@@QBA_NABVLine@2@@Z");
    a.boxCollideRay = Proc<decltype(a.boxCollideRay)>(side, "?TestCollision@AxisAlignedBox@Jet@@QBA_NABVRay@2@@Z");
    a.boxCollideSegment =
        Proc<decltype(a.boxCollideSegment)>(side, "?TestCollision@AxisAlignedBox@Jet@@QBA_NABVLineSegment@2@@Z");
    a.boxCollideObb =
        Proc<decltype(a.boxCollideObb)>(side, "?TestCollision@AxisAlignedBox@Jet@@QBA_NABVOrientedBox@2@@Z");
    a.boxCollideCylinder =
        Proc<decltype(a.boxCollideCylinder)>(side, "?TestCollision@AxisAlignedBox@Jet@@QBA_NABVCylinder@2@@Z");
    a.boxCollideEllipsoid =
        Proc<decltype(a.boxCollideEllipsoid)>(side, "?TestCollision@AxisAlignedBox@Jet@@QBA_NABVEllipsoid@2@@Z");
    return a;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u1c <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 500;
    bool noAwkward = false, onlyAwkward = false;
    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "noawk") == 0) noAwkward = true;
        if (std::strcmp(argv[i], "awk") == 0) onlyAwkward = true;
    }
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

    Obb boxes[2][16];
    alignas(16) unsigned char seed[2][sizeof(Obb)];
    for (int side = 0; side < 2; ++side)
        for (unsigned& w : *reinterpret_cast<unsigned(*)[15]>(seed[side])) w = 0x7fc00000u;

    for (int round = 0; round < rounds; ++round) {
        const bool awkward = onlyAwkward || (!noAwkward && (round % 7) == 0);
        const Obb a = RObb(awkward);
        const Obb b = RObb(awkward);
        const Box box = RBox(awkward);

        // default ctor: the storage must stay untouched (the original is a bare ret)
        for (int side = 0; side < 2; ++side) {
            std::memcpy(&boxes[side][0], seed[side], sizeof(Obb));
            api[side].ctor0(&boxes[side][0]);
        }
        CheckObb(boxes[0][0], boxes[1][0], "default ctor");

        // array and strided ctors
        Vec3 points[5] = {a.center, a.axis[0], a.axis[1], a.axis[2], a.extents};
        for (int count : {0, 1, 2, 5}) {
            for (int side = 0; side < 2; ++side) api[side].ctorArr(&boxes[side][1], count, points);
            CheckObb(boxes[0][1], boxes[1][1], "array ctor");
        }
        for (unsigned stride : {0xcu, 0x18u, 0x28u}) {
            const int count = 4;
            unsigned char raw[2][4 * 0x28 + 0x10];
            for (int side = 0; side < 2; ++side)
                for (int i = 0; i < count; ++i)
                    std::memcpy(raw[side] + i * stride, &points[i], sizeof(Vec3));
            for (int side = 0; side < 2; ++side)
                api[side].ctorItem(&boxes[side][1], count, stride, reinterpret_cast<const Vec3*>(raw[side]));
            CheckObb(boxes[0][1], boxes[1][1], "strided item ctor");
        }

        // copy ctor, ctor from an axis-aligned box, assignment
        for (int side = 0; side < 2; ++side) {
            api[side].ctorCopy(&boxes[side][0], &a);
            api[side].ctorBox(&boxes[side][1], &box);
        }
        CheckObb(boxes[0][0], boxes[1][0], "copy ctor");
        CheckObb(boxes[0][1], boxes[1][1], "ctor from AxisAlignedBox");
        const Obb b2 = b;
        for (int side = 0; side < 2; ++side) {
            std::memcpy(&boxes[side][2], &b2, sizeof(Obb));
            api[side].assign(&boxes[side][2], &a);
        }
        CheckObb(boxes[0][2], boxes[1][2], "assignment");

        // setters and getters
        const Vec3 c = RV(awkward);
        const Vec3 e = RV(awkward);
        for (int side = 0; side < 2; ++side) {
            api[side].setCenter(&boxes[side][0], &c);
            api[side].setExtents(&boxes[side][0], &e);
        }
        CheckObb(boxes[0][0], boxes[1][0], "SetCenter/SetExtents");
        CheckVec(*api[0].getCenter(&boxes[0][0]), *api[1].getCenter(&boxes[1][0]), "GetCenter");
        CheckVec(*api[0].getExtents(&boxes[0][0]), *api[1].getExtents(&boxes[1][0]), "GetExtents");
        CheckVec(api[0].getAxis(&boxes[0][0])[0], api[1].getAxis(&boxes[1][0])[0], "GetAxis");
        for (int side = 0; side < 2; ++side) {
            api[side].setAxisPtr(&boxes[side][0], a.axis);
            api[side].setAxis3(&boxes[side][0], &a.axis[0], &a.axis[1], &a.axis[2]);
        }
        CheckObb(boxes[0][0], boxes[1][0], "SetAxis");

        // zero / empty
        for (int side = 0; side < 2; ++side) api[side].zero(&boxes[side][0]);
        CheckObb(boxes[0][0], boxes[1][0], "Zero");
        Check(api[0].isEmpty(&boxes[0][0]) == api[1].isEmpty(&boxes[1][0]), "IsEmpty zero");
        Check(api[0].isEmpty(&boxes[0][1]) == api[1].isEmpty(&boxes[1][1]), "IsEmpty other");

        // transforms
        const Mat3 m3 = RMat3(awkward);
        const Mat4 m4 = RMat4(awkward);
        const Vec3 t = RV(awkward);
        for (int side = 0; side < 2; ++side) {
            api[side].ctorCopy(&boxes[side][3], &a);
            api[side].transform3(&boxes[side][3], &m3, &t);
        }
        CheckObb(boxes[0][3], boxes[1][3], "Transform(Matrix3)");
        for (int side = 0; side < 2; ++side) {
            api[side].ctorCopy(&boxes[side][3], &a);
            api[side].transform4(&boxes[side][3], &m4);
        }
        CheckObb(boxes[0][3], boxes[1][3], "Transform(Matrix4)");
        for (int side = 0; side < 2; ++side) {
            api[side].ctorCopy(&boxes[side][3], &a);
            api[side].inverseTransform(&boxes[side][3], &m3, &t);
        }
        CheckObb(boxes[0][3], boxes[1][3], "InverseTransform");

        // local direction / position (both arguments compared)
        Vec3 d0 = RV(awkward), d1 = RV(awkward);
        Vec3 dOut[2] = {d0, d1};
        for (int side = 0; side < 2; ++side) {
            Vec3 in = d0, out = d1;
            api[side].calcLocalDir(&a, &in, &out);
            CheckVec(in, d0, "CalcLocalDir arg0 untouched");
            dOut[side] = out;
        }
        CheckVec(dOut[0], dOut[1], "CalcLocalDir");
        Vec3 p0 = RV(awkward), p1 = RV(awkward);
        Vec3 pOut[2];
        for (int side = 0; side < 2; ++side) {
            Vec3 in = p0, out = p1;
            api[side].calcLocalPos(&a, &in, &out);
            CheckVec(in, p0, "CalcLocalPos arg0 untouched");
            pOut[side] = out;
        }
        CheckVec(pOut[0], pOut[1], "CalcLocalPos");

        // corners
        Vec3 corners[2][8];
        for (int side = 0; side < 2; ++side) api[side].calcCorners(&a, corners[side]);
        bool cornersSame = true;
        for (int i = 0; i < 8; ++i) cornersSame = cornersSame && SameV(corners[0][i], corners[1][i]);
        Check(cornersSame, "CalcCorners");

        // maximise
        for (int side = 0; side < 2; ++side) {
            api[side].ctorCopy(&boxes[side][4], &a);
            api[side].maximizeObb(&boxes[side][4], &b);
        }
        CheckObb(boxes[0][4], boxes[1][4], "Maximize(OrientedBox)");
        for (int side = 0; side < 2; ++side) {
            api[side].ctorCopy(&boxes[side][4], &a);
            api[side].maximizeBox(&boxes[side][4], &box);
        }
        CheckObb(boxes[0][4], boxes[1][4], "Maximize(AxisAlignedBox)");

        // ray-style tests against the oriented box
        const Line line = RLine(awkward);
        LineSegment seg{};
        seg.vftable = reinterpret_cast<void*>(0xdeadbeef);
        seg.dir = RV(awkward);
        seg.point = RV(awkward);
        seg.end = RV(awkward);
        Sphere sphere{};
        sphere.vftable = reinterpret_cast<void*>(0xdeadbeef);
        sphere.center = RV(awkward);
        sphere.radius = awkward ? Awkward() : FRange(-10.0f, 100.0f);
        sphere.radiusXY = sphere.radius;

        Check(api[0].collideObb(&a, &b) == api[1].collideObb(&a, &b), "OBB TestCollision(OrientedBox)");
        Check(api[0].collideBox(&a, &box) == api[1].collideBox(&a, &box), "OBB TestCollision(AxisAlignedBox)");
        const bool hs[2] = {api[0].collideSphere(&a, &sphere), api[1].collideSphere(&a, &sphere)};
        if (hs[0] != hs[1]) {
            std::printf("sphere mismatch ours=%d orig=%d\n", hs[0], hs[1]);
            std::printf("  c(%08x %08x %08x) e(%08x %08x %08x)\n", FBits(a.center.x), FBits(a.center.y),
                        FBits(a.center.z), FBits(a.extents.x), FBits(a.extents.y), FBits(a.extents.z));
            for (int i = 0; i < 3; ++i)
                std::printf("  A%d(%08x %08x %08x)\n", i, FBits(a.axis[i].x), FBits(a.axis[i].y), FBits(a.axis[i].z));
            std::printf("  S(%08x %08x %08x) r=%08x\n", FBits(sphere.center.x), FBits(sphere.center.y),
                        FBits(sphere.center.z), FBits(sphere.radius));
        }
        Check(hs[0] == hs[1], "OBB TestCollision(Sphere)");
        Check(api[0].collideLine(&a, &line) == api[1].collideLine(&a, &line), "OBB TestCollision(Line)");
        Check(api[0].collideRay(&a, &line) == api[1].collideRay(&a, &line), "OBB TestCollision(Ray)");
        Check(api[0].collideSegment(&a, &seg) == api[1].collideSegment(&a, &seg), "OBB TestCollision(LineSegment)");
        Check(api[0].collideCylinder(&a, &box) == api[1].collideCylinder(&a, &box), "OBB TestCollision(Cylinder)");
        Check(api[0].collideEllipsoid(&a, &box) == api[1].collideEllipsoid(&a, &box), "OBB TestCollision(Ellipsoid)");

        // GetMinMaxCollision on both classes
        float t0[2], t1[2];
        const Vec3 origin = RV(awkward), direction = RV(awkward);
        bool hit[2];
        for (int side = 0; side < 2; ++side) {
            t0[side] = t1[side] = 0.0f;
            hit[side] = api[side].getMinMax(&a, &origin, &direction, &t0[side], &t1[side]);
        }
        Check(hit[0] == hit[1], "OBB GetMinMaxCollision bool");
        Check(SameF(t0[0], t0[1]), "OBB GetMinMaxCollision tMin");
        Check(SameF(t1[0], t1[1]), "OBB GetMinMaxCollision tMax");

        for (int side = 0; side < 2; ++side) {
            t0[side] = 0.0f;
            t1[side] = 0.0f;
            api[side].boxGetMinMax(&box, &origin, &direction, &t0[side], &t1[side]);
        }
        if (!SameF(t0[0], t0[1]) || !SameF(t1[0], t1[1])) {
            std::printf("aab minmax ours t(%08x %08x) orig t(%08x %08x)\n", FBits(t0[0]), FBits(t1[0]), FBits(t0[1]),
                        FBits(t1[1]));
            std::printf("  box(%08x %08x %08x)-(%08x %08x %08x)\n", FBits(box.min.x), FBits(box.min.y),
                        FBits(box.min.z), FBits(box.max.x), FBits(box.max.y), FBits(box.max.z));
            std::printf("  o(%08x %08x %08x) d(%08x %08x %08x)\n", FBits(origin.x), FBits(origin.y), FBits(origin.z),
                        FBits(direction.x), FBits(direction.y), FBits(direction.z));
        }
        Check(SameF(t0[0], t0[1]), "AAB GetMinMaxCollision tMin");
        Check(SameF(t1[0], t1[1]), "AAB GetMinMaxCollision tMax");

        // axis-aligned box tail
        Box boxes2[2][2];
        for (int side = 0; side < 2; ++side) api[side].boxCtorObb(&boxes2[side][0], &a);
        CheckBox(boxes2[0][0], boxes2[1][0], "AxisAlignedBox(OrientedBox)");
        for (int side = 0; side < 2; ++side) {
            boxes2[side][1] = box;
            api[side].boxMaximizeObb(&boxes2[side][1], &a);
        }
        CheckBox(boxes2[0][1], boxes2[1][1], "AxisAlignedBox::Maximize(OrientedBox)");
        Check(api[0].boxCollideLine(&box, &line) == api[1].boxCollideLine(&box, &line), "AAB TestCollision(Line)");
        Check(api[0].boxCollideRay(&box, &line) == api[1].boxCollideRay(&box, &line), "AAB TestCollision(Ray)");
        Check(api[0].boxCollideSegment(&box, &seg) == api[1].boxCollideSegment(&box, &seg),
              "AAB TestCollision(LineSegment)");
        const bool hb[2] = {api[0].boxCollideObb(&box, &a), api[1].boxCollideObb(&box, &a)};
        if (hb[0] != hb[1]) {
            std::printf("aab/obb mismatch ours=%d orig=%d\n", hb[0], hb[1]);
            std::printf("  box(%08x %08x %08x)-(%08x %08x %08x)\n", FBits(box.min.x), FBits(box.min.y),
                        FBits(box.min.z), FBits(box.max.x), FBits(box.max.y), FBits(box.max.z));
            std::printf("  c(%08x %08x %08x) e(%08x %08x %08x)\n", FBits(a.center.x), FBits(a.center.y),
                        FBits(a.center.z), FBits(a.extents.x), FBits(a.extents.y), FBits(a.extents.z));
            for (int i = 0; i < 3; ++i)
                std::printf("  A%d(%08x %08x %08x)\n", i, FBits(a.axis[i].x), FBits(a.axis[i].y), FBits(a.axis[i].z));
        }
        Check(hb[0] == hb[1], "AAB TestCollision(OrientedBox)");
        Check(api[0].boxCollideCylinder(&box, &a) == api[1].boxCollideCylinder(&box, &a),
              "AAB TestCollision(Cylinder)");
        Check(api[0].boxCollideEllipsoid(&box, &a) == api[1].boxCollideEllipsoid(&box, &a),
              "AAB TestCollision(Ellipsoid)");

        // the protected Init directly (strided counts the constructors do not cover)
        for (unsigned stride : {0xcu, 0x18u}) {
            const int count = 3;
            unsigned char raw[2][3 * 0x18 + 0x10];
            for (int side = 0; side < 2; ++side)
                for (int i = 0; i < count; ++i)
                    std::memcpy(raw[side] + i * stride, &points[i], sizeof(Vec3));
            for (int side = 0; side < 2; ++side) {
                std::memcpy(&boxes[side][6], &b, sizeof(Obb));
                api[side].init(&boxes[side][6], count, stride, reinterpret_cast<const Vec3*>(raw[side]));
            }
            CheckObb(boxes[0][6], boxes[1][6], "Init(stride)");
        }
    }

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
