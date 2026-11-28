// Differential test of Render3DCore_7c unit U1a: our AxisAlignedBox against Render3DCore_7c_orig.dll in one process.
//
// Every method is called through GetProcAddress on the same inputs for both modules. The box memory, return values
// and float bit patterns (including quiet NaNs, infinities and signed zero) are compared; any difference fails.
//
// usage: diff_r3d_u1.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
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
struct BoxObj {
    Vec3 min, max;
};
static_assert(sizeof(BoxObj) == sizeof(Box), "BoxObj");
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
static_assert(sizeof(Vec3) == 0xc, "Vec3");
static_assert(sizeof(Box) == 0x18, "Box");
static_assert(sizeof(Plane) == 0x10, "Plane");
static_assert(sizeof(Sphere) == 0x18, "Sphere");

static HMODULE mod[2];
static unsigned long long checks, bad;
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
    case 2: return Bits(0x7f800000u);   // +inf
    case 3: return Bits(0xff800000u);   // -inf
    case 4: return Bits(0x7fc00000u);   // qNaN
    case 5: return Bits(0xffc00001u);   // qNaN, payload
    case 6: return Bits(0x7fa00000u);   // sNaN
    default: return FRange(-1000.0f, 1000.0f);
    }
}

static Vec3 RandomVec(bool awkward)
{
    return Vec3{awkward ? Awkward() : FRange(-100.0f, 100.0f), awkward ? Awkward() : FRange(-100.0f, 100.0f),
                awkward ? Awkward() : FRange(-100.0f, 100.0f)};
}

static Box RandomBox(bool awkward)
{
    Box b{RandomVec(awkward), RandomVec(awkward)};
    return b;
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

static bool SameBytes(const void* a, const void* b, size_t n)
{
    return std::memcmp(a, b, n) == 0;
}

static uint32_t FBits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

static void PrintBox(const char* tag, const Box& b)
{
    std::printf("%s min(%08x %08x %08x) max(%08x %08x %08x)\n", tag, FBits(b.min.x), FBits(b.min.y), FBits(b.min.z),
                FBits(b.max.x), FBits(b.max.y), FBits(b.max.z));
}

static void CheckBox(const Box& a, const Box& b, const char* what)
{
    const bool ok = SameBytes(&a, &b, sizeof a);
    if (!ok) {
        PrintBox(" ours", a);
        PrintBox(" orig", b);
    }
    Check(ok, what);
}

static void CheckVec(const Vec3& a, const Vec3& b, const char* what)
{
    if (!SameBytes(&a, &b, sizeof a))
        std::printf("%s: ours(%08x %08x %08x) orig(%08x %08x %08x)\n", what, FBits(a.x), FBits(a.y), FBits(a.z),
                    FBits(b.x), FBits(b.y), FBits(b.z));
    Check(SameBytes(&a, &b, sizeof a), what);
}

// A member-function pointer carries the member ABI (the hidden return slot after `this`), which a free-function
// pointer does not: calling a cdecl member that returns Vector3 through a free-function type puts the hidden slot
// on the wrong side. The class has no virtual functions, so the MSVC pointer is the entry address.
using CenterFn = Vec3 (__cdecl BoxObj::*)() const;
using ClampFn = Vec3 (__cdecl BoxObj::*)(const Vec3&) const;

// The exported entry points, one pair per side.
struct Api {
    void(__thiscall* ctor0)(Box*);
    void(__thiscall* ctor2)(Box*, const Vec3*, const Vec3*);
    void(__thiscall* ctorArr)(Box*, unsigned, const Vec3*);
    void(__thiscall* ctorItem)(Box*, unsigned, unsigned, const Vec3*);
    void(__thiscall* ctorCopy)(Box*, const Box*);
    Box* (__thiscall* assign)(Box*, const Box*);
    void(__cdecl* setMin)(Box*, const Vec3*);
    void(__cdecl* setMax)(Box*, const Vec3*);
    const Vec3* (__cdecl* getMin)(const Box*);
    const Vec3* (__cdecl* getMax)(const Box*);
    void(__cdecl* zero)(Box*);
    bool(__cdecl* isEmpty)(const Box*);
    void(__cdecl* maximize)(Box*, const Box*);
    void(__cdecl* minimize)(Box*, const Box*);
    CenterFn getCenter;
    void(__cdecl* calcCorners)(const Box*, Vec3*);
    ClampFn clamp;
    bool(__cdecl* isWithinVec)(const Box*, const Vec3*);
    bool(__cdecl* isWithinBox)(const Box*, const Box*);
    bool(__cdecl* isWithinPlanes)(const Box*, const Plane*);
    bool(__cdecl* collideBox)(const Box*, const Box*);
    bool(__cdecl* collideSphere)(const Box*, const Sphere*);
    bool(__cdecl* eq)(const Box*, const Box*);
    bool(__cdecl* ne)(const Box*, const Box*);
};

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

static Api Load(int side)
{
    Api a{};
    a.ctor0 = Proc<decltype(a.ctor0)>(side, "??0AxisAlignedBox@Jet@@QAE@XZ");
    a.ctor2 = Proc<decltype(a.ctor2)>(side, "??0AxisAlignedBox@Jet@@QAE@ABVVector3@1@0@Z");
    a.ctorArr = Proc<decltype(a.ctorArr)>(side, "??0AxisAlignedBox@Jet@@QAE@IPBVVector3@1@@Z");
    a.ctorItem = Proc<decltype(a.ctorItem)>(side,
                                            "??0AxisAlignedBox@Jet@@QAE@IV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@1@@Z");
    a.ctorCopy = Proc<decltype(a.ctorCopy)>(side, "??0AxisAlignedBox@Jet@@QAE@ABV01@@Z");
    a.assign = Proc<decltype(a.assign)>(side, "??4AxisAlignedBox@Jet@@QAEAAV01@ABV01@@Z");
    a.setMin = Proc<decltype(a.setMin)>(side, "?SetMin@AxisAlignedBox@Jet@@QAAXABVVector3@2@@Z");
    a.setMax = Proc<decltype(a.setMax)>(side, "?SetMax@AxisAlignedBox@Jet@@QAAXABVVector3@2@@Z");
    a.getMin = Proc<decltype(a.getMin)>(side, "?GetMin@AxisAlignedBox@Jet@@QBAABVVector3@2@XZ");
    a.getMax = Proc<decltype(a.getMax)>(side, "?GetMax@AxisAlignedBox@Jet@@QBAABVVector3@2@XZ");
    a.zero = Proc<decltype(a.zero)>(side, "?Zero@AxisAlignedBox@Jet@@QAAXXZ");
    a.isEmpty = Proc<decltype(a.isEmpty)>(side, "?IsEmpty@AxisAlignedBox@Jet@@QBA_NXZ");
    a.maximize = Proc<decltype(a.maximize)>(side, "?Maximize@AxisAlignedBox@Jet@@QAAXABV12@@Z");
    a.minimize = Proc<decltype(a.minimize)>(side, "?Minimize@AxisAlignedBox@Jet@@QAAXABV12@@Z");
    a.getCenter = MemberProc<CenterFn>(side, "?GetCenter@AxisAlignedBox@Jet@@QBA?AVVector3@2@XZ");
    a.calcCorners = Proc<decltype(a.calcCorners)>(side, "?CalcCorners@AxisAlignedBox@Jet@@QBAXPAVVector3@2@@Z");
    a.clamp = MemberProc<ClampFn>(side, "?Clamp@AxisAlignedBox@Jet@@QBA?AVVector3@2@ABV32@@Z");
    a.isWithinVec = Proc<decltype(a.isWithinVec)>(side, "?IsWithin@AxisAlignedBox@Jet@@QBA_NABVVector3@2@@Z");
    a.isWithinBox = Proc<decltype(a.isWithinBox)>(side, "?IsWithin@AxisAlignedBox@Jet@@QBA_NABV12@@Z");
    a.isWithinPlanes = Proc<decltype(a.isWithinPlanes)>(side, "?IsWithin@AxisAlignedBox@Jet@@QBA_NQBVPlane@2@@Z");
    a.collideBox = Proc<decltype(a.collideBox)>(side, "?TestCollision@AxisAlignedBox@Jet@@QBA_NABV12@@Z");
    a.collideSphere = Proc<decltype(a.collideSphere)>(side,
                                                     "?TestCollision@AxisAlignedBox@Jet@@QBA_NABVSphere@2@@Z");
    a.eq = Proc<decltype(a.eq)>(side, "??8AxisAlignedBox@Jet@@QBA_NABV01@@Z");
    a.ne = Proc<decltype(a.ne)>(side, "??9AxisAlignedBox@Jet@@QBA_NABV01@@Z");
    return a;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u1 <bin dir> [rounds]\n");
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

    Box boxes[2][3];
    alignas(16) unsigned char seed[2][sizeof(Box)];
    for (int side = 0; side < 2; ++side)
        for (unsigned& w : *reinterpret_cast<unsigned(*)[6]>(seed[side]))
            w = 0x7fc00000u;  // a visible fill; the default ctor must leave it

    for (int round = 0; round < rounds; ++round) {
        const bool awkward = (round % 7) == 0;
        Box a = RandomBox(awkward), b = RandomBox(awkward);

        // default ctor: must leave the storage untouched (the original is a bare `ret`)
        for (int side = 0; side < 2; ++side) {
            std::memcpy(&boxes[side][0], seed[side], sizeof(Box));
            api[side].ctor0(&boxes[side][0]);
        }
        CheckBox(boxes[0][0], boxes[1][0], "default ctor");

        // min/max ctor
        for (int side = 0; side < 2; ++side) api[side].ctor2(&boxes[side][0], &a.min, &a.max);
        CheckBox(boxes[0][0], boxes[1][0], "min/max ctor");

        // contiguous and strided array ctors
        Vec3 points[5] = {a.min, a.max, b.min, b.max, RandomVec(awkward)};
        for (int count : {0, 1, 2, 5}) {
            for (int side = 0; side < 2; ++side) api[side].ctorArr(&boxes[side][1], count, points);
            CheckBox(boxes[0][1], boxes[1][1], "array ctor");
        }
        for (unsigned stride : {0xcu, 0x18u, 0x28u}) {
            const int itemCount = 4;
            unsigned char raw[2][4 * 0x28 + 0x10];
            std::memcpy(raw[0], points, sizeof(Vec3) * 4);
            std::memcpy(raw[1], points, sizeof(Vec3) * 4);
            // place four points at the requested stride
            for (int i = 0; i < itemCount; ++i) {
                std::memcpy(raw[0] + i * stride, &points[i], sizeof(Vec3));
                std::memcpy(raw[1] + i * stride, &points[i], sizeof(Vec3));
            }
            for (int side = 0; side < 2; ++side)
                api[side].ctorItem(&boxes[side][1], itemCount, stride, reinterpret_cast<const Vec3*>(raw[side]));
            CheckBox(boxes[0][1], boxes[1][1], "strided item ctor");
        }

        // copy ctor and assignment
        for (int side = 0; side < 2; ++side) {
            api[side].ctor2(&boxes[side][1], &a.min, &a.max);
            api[side].ctor2(&boxes[side][2], &b.min, &b.max);
            api[side].ctorCopy(&boxes[side][0], &boxes[side][1]);
        }
        CheckBox(boxes[0][0], boxes[1][0], "copy ctor");
        for (int side = 0; side < 2; ++side) api[side].assign(&boxes[side][0], &boxes[side][2]);
        CheckBox(boxes[0][0], boxes[1][0], "assignment");

        // setters and getters
        for (int side = 0; side < 2; ++side) {
            api[side].setMin(&boxes[side][0], &b.min);
            api[side].setMax(&boxes[side][0], &b.max);
        }
        CheckBox(boxes[0][0], boxes[1][0], "SetMin/SetMax");
        CheckVec(*api[0].getMin(&boxes[0][0]), *api[1].getMin(&boxes[1][0]), "GetMin");
        CheckVec(*api[0].getMax(&boxes[0][0]), *api[1].getMax(&boxes[1][0]), "GetMax");

        // zero / empty
        for (int side = 0; side < 2; ++side) api[side].zero(&boxes[side][0]);
        CheckBox(boxes[0][0], boxes[1][0], "Zero");
        Check(api[0].isEmpty(&boxes[0][0]) == api[1].isEmpty(&boxes[1][0]), "IsEmpty");
        for (int side = 0; side < 2; ++side) api[side].ctor2(&boxes[side][0], &a.min, &a.max);
        Check(api[0].isEmpty(&boxes[0][0]) == api[1].isEmpty(&boxes[1][0]), "IsEmpty non-zero");

        // maximise/minimise
        for (int side = 0; side < 2; ++side) {
            api[side].ctor2(&boxes[side][1], &a.min, &a.max);
            api[side].ctor2(&boxes[side][2], &b.min, &b.max);
            api[side].maximize(&boxes[side][1], &boxes[side][2]);
        }
        CheckBox(boxes[0][1], boxes[1][1], "Maximize");
        for (int side = 0; side < 2; ++side) {
            api[side].ctor2(&boxes[side][1], &a.min, &a.max);
            api[side].minimize(&boxes[side][1], &boxes[side][2]);
        }
        CheckBox(boxes[0][1], boxes[1][1], "Minimize");

        // centre, corners and clamp (member calls for the returned Vector3)
        BoxObj* objs[2] = {reinterpret_cast<BoxObj*>(&boxes[0][0]), reinterpret_cast<BoxObj*>(&boxes[1][0])};
        Vec3 centers[2] = {(objs[0]->*api[0].getCenter)(), (objs[1]->*api[1].getCenter)()};
        CheckVec(centers[0], centers[1], "GetCenter");
        Vec3 corners[2][8];
        for (int side = 0; side < 2; ++side) api[side].calcCorners(&boxes[side][0], corners[side]);
        Check(SameBytes(corners[0], corners[1], sizeof corners[0]), "CalcCorners");
        const Vec3 p = RandomVec(awkward);
        CheckVec((objs[0]->*api[0].clamp)(p), (objs[1]->*api[1].clamp)(p), "Clamp");

        // inside, overlap, equality
        const Vec3 q = RandomVec(awkward);
        Check(api[0].isWithinVec(&boxes[0][0], &q) == api[1].isWithinVec(&boxes[1][0], &q), "IsWithin(Vector3)");
        for (int side = 0; side < 2; ++side) api[side].ctor2(&boxes[side][1], &b.min, &b.max);
        Check(api[0].isWithinBox(&boxes[0][0], &boxes[0][1]) == api[1].isWithinBox(&boxes[1][0], &boxes[1][1]),
              "IsWithin(Box)");
        Check(api[0].collideBox(&boxes[0][0], &boxes[0][1]) == api[1].collideBox(&boxes[1][0], &boxes[1][1]),
              "TestCollision(Box)");
        Check(api[0].eq(&boxes[0][0], &boxes[0][1]) == api[1].eq(&boxes[1][0], &boxes[1][1]), "operator==");
        Check(api[0].ne(&boxes[0][0], &boxes[0][1]) == api[1].ne(&boxes[1][0], &boxes[1][1]), "operator!=");

        // six frustum planes
        Plane planes[6];
        for (Plane& pl : planes) {
            pl.normal = RandomVec(awkward);
            pl.d = awkward ? Awkward() : FRange(-100.0f, 100.0f);
        }
        Check(api[0].isWithinPlanes(&boxes[0][0], planes) == api[1].isWithinPlanes(&boxes[1][0], planes),
              "IsWithin(Plane*)");

        // sphere
        Sphere sphere{};
        sphere.center = RandomVec(awkward);
        sphere.radius = awkward ? Awkward() : FRange(-10.0f, 200.0f);
        sphere.radiusXY = sphere.radius;
        Check(api[0].collideSphere(&boxes[0][0], &sphere) == api[1].collideSphere(&boxes[1][0], &sphere),
              "TestCollision(Sphere)");
    }

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
