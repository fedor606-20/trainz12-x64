// Differential test of Render3DCore_7c wave 6 simple classes: Transformation, Tube, Primitive3D,
// ParticleFXRenderItem, DebugRender, LightLoader, ViewPoint, Camera GetRay/copy-transform and the Camera
// copy-with-transform constructor.
//
// usage: diff_r3d_simple_rem.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
//
// Objects are made by each module's own constructors and driven through the same exports; bytes outside the vtable
// (and outside pointers that necessarily differ) are compared. Float rounds include signed zero, infinities and
// quiet/signalling NaNs. By-value returns go through member-function pointers so the hidden return slot stays after
// `this`, as the original expects.
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

struct Vec3 {
    float x, y, z;
};
struct Vec2 {
    float x, y;
};
struct Quat {
    float x, y, z, w;
};
struct Mat3 {
    float m[9];
};
struct Mat4 {
    float m[16];
};
struct Color {
    float r, g, b, a;
};
struct Rect {
    int x, y, w, h;
};
struct RayObj {
    unsigned char bytes[0x1c];
};
struct LineObj {
    unsigned char bytes[0x1c];
};
struct SphereObj {
    unsigned char bytes[0x14];
};
struct TransformObj {
    unsigned char bytes[0x30];
};
struct TubeObj {
    unsigned char bytes[0x1c];
};
struct CameraObj {
    unsigned char bytes[0x264];
};
struct ViewPointObj {
    unsigned char bytes[0x70];
};

static_assert(sizeof(Vec3) == 0xc, "Vec3");
static_assert(sizeof(Mat3) == 0x24, "Mat3");
static_assert(sizeof(Mat4) == 0x40, "Mat4");
static_assert(sizeof(TransformObj) == 0x30, "Transform");
static_assert(sizeof(TubeObj) == 0x1c, "Tube");
static_assert(sizeof(SphereObj) == 0x14, "Sphere");
static_assert(sizeof(RayObj) == 0x1c, "Ray");
static_assert(sizeof(CameraObj) == 0x264, "Camera");
static_assert(sizeof(ViewPointObj) == 0x70, "ViewPoint");

static HMODULE mod[2];
static HMODULE renderSystem;
static HMODULE kernelStandard;
static unsigned long long checks, bad;
static std::mt19937 rng(0x51ee7);

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
    case 2: return 1.0f;
    case 3: return -1.0f;
    case 4: return Bits(0x7f800000u);
    case 5: return Bits(0xff800000u);
    case 6: return Bits(0x7fc00000u);
    case 7: return Bits(0xffc00000u);
    case 8: return Bits(0x7f800001u);
    case 9: return Bits(0xffbfffffu);
    case 10: return 1.0e-30f;
    default: return 1.0e30f;
    }
}
static Vec3 RandomVec(bool awkward)
{
    return Vec3{awkward ? Awkward() : FRange(-4.0f, 4.0f), awkward ? Awkward() : FRange(-4.0f, 4.0f),
                awkward ? Awkward() : FRange(-4.0f, 4.0f)};
}
static Mat3 RandomMat3(bool awkward)
{
    Mat3 m;
    for (float& f : m.m) f = awkward ? Awkward() : FRange(-1.0f, 1.0f);
    return m;
}
static Mat4 RandomMat4(bool awkward)
{
    Mat4 m;
    for (float& f : m.m) f = awkward ? Awkward() : FRange(-1.0f, 1.0f);
    for (int i = 0; i < 16; i += 5) m.m[i] = awkward ? Awkward() : FRange(0.5f, 2.0f);
    return m;
}
static Quat RandomQuat(bool awkward)
{
    Quat q{awkward ? Awkward() : FRange(-1.0f, 1.0f), awkward ? Awkward() : FRange(-1.0f, 1.0f),
           awkward ? Awkward() : FRange(-1.0f, 1.0f), awkward ? Awkward() : FRange(0.25f, 1.0f)};
    return q;
}

template <class F>
static F Proc(int side, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(mod[side], name));
    if (!p) {
        std::printf("missing export %s in %s\n", name, side ? "ours" : "orig");
        std::exit(2);
    }
    return p;
}

template <class T>
static T MemberProc(int side, const char* name)
{
    FARPROC raw = GetProcAddress(mod[side], name);
    if (!raw) {
        std::printf("missing export %s in %s\n", name, side ? "ours" : "orig");
        std::exit(2);
    }
    T fn{};
    static_assert(sizeof fn == sizeof raw, "member pointer size");
    std::memcpy(&fn, &raw, sizeof fn);
    return fn;
}

template <class F>
static F ProcRS(const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(renderSystem, name));
    if (!p) {
        std::printf("missing RenderSystem export %s\n", name);
        std::exit(2);
    }
    return p;
}

template <class F>
static F ProcKS(const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(kernelStandard, name));
    if (!p) {
        std::printf("missing KernelStandard export %s\n", name);
        std::exit(2);
    }
    return p;
}

static void Check(bool ok, const char* what, int round)
{
    ++checks;
    if (!ok) {
        ++bad;
        if (bad < 30) std::printf("MISMATCH %s (round %d)\n", what, round);
    }
}

static void CheckBytes(const void* a, const void* b, unsigned n, const char* what, int round)
{
    if (std::memcmp(a, b, n) != 0) {
        ++checks;
        ++bad;
        if (bad < 30) {
            std::printf("MISMATCH %s (round %d, n=%u):", what, round, n);
            const unsigned char* pa = static_cast<const unsigned char*>(a);
            const unsigned char* pb = static_cast<const unsigned char*>(b);
            int shown = 0;
            for (unsigned i = 0; i < n; ++i)
                if (pa[i] != pb[i] && shown++ < 12) std::printf(" [%u] %02x/%02x", i, pa[i], pb[i]);
            std::printf("\n");
        }
        return;
    }
    ++checks;
}

// ---- Transformation ---------------------------------------------------------------------------------------------
struct TransformTest {
    Vec3 Transform(const Vec3& v) const;
    Vec3 InverseTransform(const Vec3& v) const;
    TransformObj operator*(const TransformObj& o);
};
struct TubeTest {
    Vec3 GetCenter(int index);
};

struct TransformApi {
    void(__thiscall* ctor0)(void*);
    void(__thiscall* ctor2)(void*, const Mat3*, const Vec3*);
    void*(__thiscall* assign)(void*, const void*);
    Vec3(__cdecl TransformTest::*transform)(const Vec3&) const;
    Vec3(__cdecl TransformTest::*inverse)(const Vec3&) const;
    void(__cdecl* multiply)(void*, const Mat3*, const Vec3*, void*);
    TransformObj(__cdecl TransformTest::*times)(const TransformObj&);
    void*(__cdecl* timesAssign)(void*, const void*);
    void(__cdecl* transformArray)(void*, int, const Vec3*, Vec3*);
};

static TransformApi LoadTransform(int side)
{
    TransformApi a{};
    a.ctor0 = Proc<decltype(a.ctor0)>(side, "??0Transformation@Jet@@QAE@XZ");
    a.ctor2 = Proc<decltype(a.ctor2)>(side, "??0Transformation@Jet@@QAE@ABVMatrix3@1@ABVVector3@1@@Z");
    a.assign = Proc<decltype(a.assign)>(side, "??4Transformation@Jet@@QAEAAV01@ABV01@@Z");
    a.transform = MemberProc<decltype(a.transform)>(side, "?Transform@Transformation@Jet@@QBA?AVVector3@2@ABV32@@Z");
    a.inverse = MemberProc<decltype(a.inverse)>(side, "?InverseTransform@Transformation@Jet@@QBA?AVVector3@2@ABV32@@Z");
    a.multiply = Proc<decltype(a.multiply)>(side, "?MultiplyTransform@Transformation@Jet@@QBAXABVMatrix3@2@ABVVector3@2@AAV12@@Z");
    a.times = MemberProc<decltype(a.times)>(side, "??DTransformation@Jet@@QAA?AV01@ABV01@@Z");
    a.timesAssign = Proc<decltype(a.timesAssign)>(side, "??XTransformation@Jet@@QAAAAV01@ABV01@@Z");
    a.transformArray = Proc<decltype(a.transformArray)>(side, "?TransformArray@Transformation@Jet@@QAAXHPBVVector3@2@PAV32@@Z");
    return a;
}

static void TransformRound(const TransformApi* api, bool awkward, int round)
{
    TransformObj t[2];
    std::memset(t, 0xcd, sizeof t);
    for (int v = 0; v < 2; ++v) api[v].ctor0(&t[v]);
    CheckBytes(&t[0], &t[1], 0x30, "Transformation()", round);

    const Mat3 m = RandomMat3(awkward);
    const Vec3 tr = RandomVec(awkward);
    for (int v = 0; v < 2; ++v) api[v].ctor2(&t[v], &m, &tr);
    CheckBytes(&t[0], &t[1], 0x30, "Transformation(Matrix3,Vector3)", round);

    TransformObj t2[2];
    std::memset(t2, 0xcd, sizeof t2);
    for (int v = 0; v < 2; ++v) api[v].ctor0(&t2[v]);
    for (int v = 0; v < 2; ++v) api[v].assign(&t2[v], &t[v]);
    CheckBytes(&t2[0], &t2[1], 0x30, "Transformation::operator=", round);

    for (int i = 0; i < 4; ++i) {
        const Vec3 p = RandomVec(awkward);
        const Vec3 ra = (reinterpret_cast<const TransformTest*>(&t[0])->*api[0].transform)(p);
        const Vec3 rb = (reinterpret_cast<const TransformTest*>(&t[1])->*api[1].transform)(p);
        CheckBytes(&ra, &rb, sizeof ra, "Transformation::Transform", round);
        const Vec3 ia = (reinterpret_cast<const TransformTest*>(&t[0])->*api[0].inverse)(p);
        const Vec3 ib = (reinterpret_cast<const TransformTest*>(&t[1])->*api[1].inverse)(p);
        CheckBytes(&ia, &ib, sizeof ia, "Transformation::InverseTransform", round);
    }

    const Mat3 m2 = RandomMat3(awkward);
    const Vec3 tr2 = RandomVec(awkward);
    TransformObj ma[2], mb[2];
    std::memset(ma, 0, sizeof ma);
    std::memset(mb, 0, sizeof mb);
    for (int v = 0; v < 2; ++v) api[v].multiply(&t[v], &m2, &tr2, &ma[v]);
    for (int v = 0; v < 2; ++v) api[v].multiply(&t[v], &m2, &tr2, &mb[v]);
    CheckBytes(&ma[0], &ma[1], 0x30, "Transformation::MultiplyTransform", round);

    TransformObj other[2];
    std::memset(other, 0xcd, sizeof other);
    const Mat3 m3 = RandomMat3(awkward);
    const Vec3 tr3 = RandomVec(awkward);
    for (int v = 0; v < 2; ++v) api[v].ctor2(&other[v], &m3, &tr3);

    const TransformObj ra =
        (reinterpret_cast<TransformTest*>(&t[0])->*api[0].times)(*reinterpret_cast<TransformObj*>(&other[0]));
    const TransformObj rb =
        (reinterpret_cast<TransformTest*>(&t[1])->*api[1].times)(*reinterpret_cast<TransformObj*>(&other[1]));
    CheckBytes(&ra, &rb, 0x30, "Transformation::operator*", round);

    TransformObj ta[2], tb[2];
    std::memset(ta, 0xcd, sizeof ta);
    std::memset(tb, 0xcd, sizeof tb);
    for (int v = 0; v < 2; ++v) {
        memcpy(&ta[v], &t[v], 0x30);
        memcpy(&tb[v], &t[v], 0x30);
    }
    for (int v = 0; v < 2; ++v) api[v].timesAssign(&ta[v], &other[v]);
    for (int v = 0; v < 2; ++v) api[v].timesAssign(&tb[v], &other[v]);
    CheckBytes(&ta[0], &ta[1], 0x30, "Transformation::operator*=", round);

    const int count = Pick(0, 10);
    Vec3 in[10], outa[10], outb[10];
    for (int i = 0; i < 10; ++i) in[i] = RandomVec(awkward);
    std::memset(outa, 0xcd, sizeof outa);
    std::memset(outb, 0xcd, sizeof outb);
    for (int v = 0; v < 2; ++v) api[v].transformArray(v ? &t[v] : &t[0], count, in, v ? outb : outa);
    CheckBytes(outa, outb, sizeof(Vec3) * static_cast<unsigned>(count), "Transformation::TransformArray", round);
}

// ---- Tube -------------------------------------------------------------------------------------------------------
struct TubeApi {
    void(__thiscall* ctor0)(void*);
    void(__thiscall* ctor3)(void*, const Vec3*, const Vec3*, float);
    void*(__thiscall* assign)(void*, const void*);
    const Vec3&(__cdecl* getCenterConst)(const void*, int);
    Vec3(__cdecl TubeTest::*getCenter)(int);
    void(__cdecl* getCenterOut)(const void*, Vec3*, int);
    Vec3*(__cdecl* getCenters)(const void*);
    float(__cdecl* getRadius)(const void*);
    void(__cdecl* setCenter)(void*, const Vec3*, int);
    void(__cdecl* setCenters)(void*, const Vec3*);
    void(__cdecl* setRadius)(void*, float);
    bool(__cdecl* testPoint)(void*, const Vec3*);
    bool(__cdecl* testLine)(void*, const void*);
    bool(__cdecl* testSphere)(void*, const void*);
    bool(__cdecl* testTube)(void*, const void*);
};

static TubeApi LoadTube(int side)
{
    TubeApi a{};
    a.ctor0 = Proc<decltype(a.ctor0)>(side, "??0Tube@Jet@@QAE@XZ");
    a.ctor3 = Proc<decltype(a.ctor3)>(side, "??0Tube@Jet@@QAE@ABVVector3@1@0M@Z");
    a.assign = Proc<decltype(a.assign)>(side, "??4Tube@Jet@@QAEAAV01@ABV01@@Z");
    a.getCenterConst = Proc<decltype(a.getCenterConst)>(side, "?GetCenter@Tube@Jet@@QBAABVVector3@2@H@Z");
    a.getCenter = MemberProc<decltype(a.getCenter)>(side, "?GetCenter@Tube@Jet@@QAA?AVVector3@2@H@Z");
    a.getCenterOut = Proc<decltype(a.getCenterOut)>(side, "?GetCenter@Tube@Jet@@QBAXAAVVector3@2@H@Z");
    a.getCenters = Proc<decltype(a.getCenters)>(side, "?GetCenters@Tube@Jet@@QBAPAVVector3@2@XZ");
    a.getRadius = Proc<decltype(a.getRadius)>(side, "?GetRadius@Tube@Jet@@QBAMXZ");
    a.setCenter = Proc<decltype(a.setCenter)>(side, "?SetCenter@Tube@Jet@@QAAXABVVector3@2@H@Z");
    a.setCenters = Proc<decltype(a.setCenters)>(side, "?SetCenters@Tube@Jet@@QAAXQBVVector3@2@@Z");
    a.setRadius = Proc<decltype(a.setRadius)>(side, "?SetRadius@Tube@Jet@@QAAXM@Z");
    a.testPoint = Proc<decltype(a.testPoint)>(side, "?TestCollision@Tube@Jet@@QAA_NABVVector3@2@@Z");
    a.testLine = Proc<decltype(a.testLine)>(side, "?TestCollision@Tube@Jet@@QAA_NABVLine@2@@Z");
    a.testSphere = Proc<decltype(a.testSphere)>(side, "?TestCollision@Tube@Jet@@QAA_NABVSphere@2@@Z");
    a.testTube = Proc<decltype(a.testTube)>(side, "?TestCollision@Tube@Jet@@QAA_NABV12@@Z");
    return a;
}

static void TubeRound(const TubeApi* api, bool awkward, int round)
{
    TubeObj t[2];
    std::memset(t, 0xcd, sizeof t);
    for (int v = 0; v < 2; ++v) api[v].ctor0(&t[v]);
    CheckBytes(&t[0], &t[1], 0x1c, "Tube()", round);

    const Vec3 a = RandomVec(awkward), b = RandomVec(awkward);
    const float radius = awkward ? Awkward() : FRange(0.01f, 2.0f);
    for (int v = 0; v < 2; ++v) api[v].ctor3(&t[v], &a, &b, radius);
    CheckBytes(&t[0], &t[1], 0x1c, "Tube(Vector3,Vector3,float)", round);

    for (int i = 0; i < 2; ++i) {
        Vec3 ca = (reinterpret_cast<TubeTest*>(&t[0])->*api[0].getCenter)(i);
        Vec3 cb = (reinterpret_cast<TubeTest*>(&t[1])->*api[1].getCenter)(i);
        CheckBytes(&ca, &cb, sizeof ca, "Tube::GetCenter(int)", round);
        CheckBytes(&api[0].getCenterConst(&t[0], i), &api[1].getCenterConst(&t[1], i), sizeof(Vec3),
                   "Tube::GetCenter(int) const", round);
        Vec3 oa, ob;
        std::memset(&oa, 0xcd, sizeof oa);
        std::memset(&ob, 0xcd, sizeof ob);
        api[0].getCenterOut(&t[0], &oa, i);
        api[1].getCenterOut(&t[1], &ob, i);
        CheckBytes(&oa, &ob, sizeof oa, "Tube::GetCenter(Vector3,int)", round);
    }
    float tubeRadiusA = api[0].getRadius(&t[0]);
    float tubeRadiusB = api[1].getRadius(&t[1]);
    uint32_t rbitsA, rbitsB;
    std::memcpy(&rbitsA, &tubeRadiusA, sizeof rbitsA);
    std::memcpy(&rbitsB, &tubeRadiusB, sizeof rbitsB);
    Check(rbitsA == rbitsB, "Tube::GetRadius", round);
    Check(api[0].getCenters(&t[0]) == reinterpret_cast<const Vec3*>(&t[0]) &&
              api[1].getCenters(&t[1]) == reinterpret_cast<const Vec3*>(&t[1]),
          "Tube::GetCenters", round);

    const Vec3 c = RandomVec(awkward);
    for (int v = 0; v < 2; ++v) api[v].setCenter(&t[v], &c, 1);
    CheckBytes(&t[0], &t[1], 0x1c, "Tube::SetCenter", round);
    const Vec3 centers[2] = {RandomVec(awkward), RandomVec(awkward)};
    for (int v = 0; v < 2; ++v) api[v].setCenters(&t[v], centers);
    CheckBytes(&t[0], &t[1], 0x1c, "Tube::SetCenters", round);
    const float r2 = awkward ? Awkward() : FRange(-1.0f, 3.0f);
    for (int v = 0; v < 2; ++v) api[v].setRadius(&t[v], r2);
    CheckBytes(&t[0], &t[1], 0x1c, "Tube::SetRadius", round);

    TubeObj t2[2];
    std::memset(t2, 0xcd, sizeof t2);
    for (int v = 0; v < 2; ++v) api[v].assign(&t2[v], &t[v]);
    CheckBytes(&t2[0], &t2[1], 0x1c, "Tube::operator=", round);

    // Line and Sphere objects from the original module; both calls only read their floats.
    using LineCtor = void(__thiscall*)(void*, const Vec3*, const Vec3*);
    using SphereCtor = void(__thiscall*)(void*, const Vec3*, float);
    LineObj line;
    SphereObj sphere;
    {
        LineCtor lineCtor = Proc<LineCtor>(0, "??0Line@Jet@@QAE@ABVVector3@1@0@Z");
        SphereCtor sphereCtor = Proc<SphereCtor>(0, "??0Sphere@Jet@@QAE@ABVVector3@1@M@Z");
        const Vec3 p0 = RandomVec(awkward), p1 = RandomVec(awkward);
        lineCtor(&line, &p0, &p1);
        const Vec3 sc = RandomVec(awkward);
        sphereCtor(&sphere, &sc, awkward ? Awkward() : FRange(0.01f, 1.0f));
    }
    const Vec3 point = RandomVec(awkward);
    Check(api[0].testPoint(&t[0], &point) == api[1].testPoint(&t[1], &point), "Tube::TestCollision(point)", round);
    Check(api[0].testLine(&t[0], &line) == api[1].testLine(&t[1], &line), "Tube::TestCollision(line)", round);
    Check(api[0].testSphere(&t[0], &sphere) == api[1].testSphere(&t[1], &sphere), "Tube::TestCollision(sphere)", round);
    Check(api[0].testTube(&t[0], &t[1]) == api[1].testTube(&t[1], &t[0]), "Tube::TestCollision(tube)", round);
}

// ---- Simple endlings --------------------------------------------------------------------------------------------
static void SimpleRound(int round)
{
    // Primitive3D / ParticleFXRenderItem: vftable only. The ctor is the whole exported surface; slot 4 (Intersects)
    // must answer false on a zeroed object of either module.
    using Ctor = void(__thiscall*)(void*);
    using CopyCtor = void(__thiscall*)(void*, const void*);
    using Assign = void*(__thiscall*)(void*, const void*);
    struct Obj {
        unsigned char bytes[4];
    };
    Obj p3[2], pfx[2];
    std::memset(p3, 0xcd, sizeof p3);
    std::memset(pfx, 0xcd, sizeof pfx);
    for (int v = 0; v < 2; ++v) {
        Proc<Ctor>(v, "??0Primitive3D@Jet@@QAE@XZ")(&p3[v]);
        Proc<Ctor>(v, "??0ParticleFXRenderItem@Jet@@QAE@XZ")(&pfx[v]);
    }
    Check(reinterpret_cast<uintptr_t*>(&p3[0])[0] != 0, "Primitive3D vftable", round);
    Check(reinterpret_cast<uintptr_t*>(&pfx[0])[0] != 0, "ParticleFXRenderItem vftable", round);
    for (int v = 0; v < 2; ++v) {
        Proc<CopyCtor>(v, "??0Primitive3D@Jet@@QAE@ABV01@@Z")(&p3[v], &p3[v]);
        Proc<CopyCtor>(v, "??0ParticleFXRenderItem@Jet@@QAE@ABV01@@Z")(&pfx[v], &pfx[v]);
        Proc<Assign>(v, "??4Primitive3D@Jet@@QAEAAV01@ABV01@@Z")(&p3[v], &p3[v]);
        Proc<Assign>(v, "??4ParticleFXRenderItem@Jet@@QAEAAV01@ABV01@@Z")(&pfx[v], &pfx[v]);
    }
    // Intersects through the original's export (a folded "return false").
    using IntersectsFn = bool(__cdecl*)(const void*, const void*, const void*, void*);
    Check(!Proc<IntersectsFn>(0, "?Intersects@Primitive3D@Jet@@UBA_NABVIndexedMeshInstance@2@ABVTransformation@2@AAVCollisionResult@2@@Z")(&p3[0], nullptr, nullptr, nullptr),
          "Primitive3D::Intersects", round);

    // DebugRender: empty class; static bounding flags go through DebugInterface_7c.
    using DRCtor = void(__thiscall*)(void*, const Color*, unsigned long);
    Color c{1.0f, 0.25f, 0.5f, 1.0f};
    unsigned char dr[2];
    std::memset(dr, 0xcd, sizeof dr);
    for (int v = 0; v < 2; ++v) Proc<DRCtor>(v, "??0DebugRender@Jet@@QAE@ABVColor@1@K@Z")(&dr[v], &c, 0x1234);
    CheckBytes(&dr[0], &dr[1], 1, "DebugRender()", round);
    using GetColor = const Color*(__cdecl*)(const void*);
    CheckBytes(Proc<GetColor>(0, "?GetBoundingVolumeColor@DebugRender@Jet@@QBAABVColor@2@XZ")(&dr[0]),
               Proc<GetColor>(1, "?GetBoundingVolumeColor@DebugRender@Jet@@QBAABVColor@2@XZ")(&dr[1]), sizeof(Color),
               "DebugRender::GetBoundingVolumeColor", round);
    using SetAll = void(__cdecl*)(bool);
    using GetAll = bool(__cdecl*)();
    using SetMask = void(__cdecl*)(unsigned long);
    using GetMask = unsigned long(__cdecl*)();
    SetAll setAll[2] = {Proc<SetAll>(0, "?SetShowAllBoundingVolumes@DebugRender@Jet@@SAX_N@Z"),
                        Proc<SetAll>(1, "?SetShowAllBoundingVolumes@DebugRender@Jet@@SAX_N@Z")};
    GetAll getAll[2] = {Proc<GetAll>(0, "?GetShowAllBoundingVolumes@DebugRender@Jet@@SA_NXZ"),
                        Proc<GetAll>(1, "?GetShowAllBoundingVolumes@DebugRender@Jet@@SA_NXZ")};
    SetMask setMask[2] = {Proc<SetMask>(0, "?SetBoundingVolumesTestMask@DebugRender@Jet@@SAXK@Z"),
                          Proc<SetMask>(1, "?SetBoundingVolumesTestMask@DebugRender@Jet@@SAXK@Z")};
    GetMask getMask[2] = {Proc<GetMask>(0, "?GetShowAllBoundingTestMask@DebugRender@Jet@@SAKXZ"),
                          Proc<GetMask>(1, "?GetShowAllBoundingTestMask@DebugRender@Jet@@SAKXZ")};
    setAll[1](true);
    setAll[0](true);
    Check(getAll[0]() == getAll[1](), "DebugRender::Set/GetShowAllBoundingVolumes", round);
    setAll[1](false);
    Check(getAll[0]() == getAll[1](), "DebugRender::Set/GetShowAllBoundingVolumes (2)", round);
    setMask[0](0x5a5a5a5a);
    Check(getMask[0]() == getMask[1](), "DebugRender::Set/GetShowAllBoundingTestMask", round);
    using GetBool = bool(__cdecl*)(const void*);
    using SetBool = void(__cdecl*)(void*, bool);
    using GetU32 = unsigned long(__cdecl*)(const void*);
    GetBool getBool[2] = {Proc<GetBool>(0, "?GetShowBoundingVolume@DebugRender@Jet@@QBA_NXZ"),
                          Proc<GetBool>(1, "?GetShowBoundingVolume@DebugRender@Jet@@QBA_NXZ")};
    SetBool setBool[2] = {Proc<SetBool>(0, "?SetShowBoundingVolume@DebugRender@Jet@@QAAX_N@Z"),
                          Proc<SetBool>(1, "?SetShowBoundingVolume@DebugRender@Jet@@QAAX_N@Z")};
    GetU32 getMaskValue[2] = {Proc<GetU32>(0, "?GetBoundingVolumeMask@DebugRender@Jet@@QBAKXZ"),
                              Proc<GetU32>(1, "?GetBoundingVolumeMask@DebugRender@Jet@@QBAKXZ")};
    Check(!getBool[0](&dr[0]) && !getBool[1](&dr[1]), "DebugRender::GetShowBoundingVolume", round);
    setBool[0](&dr[0], true);
    setBool[1](&dr[1], true);
    Check(!getMaskValue[0](&dr[0]) && !getMaskValue[1](&dr[1]), "DebugRender::GetBoundingVolumeMask", round);

    // LightLoader: Load always answers null.
    using LoadFn = void*(__cdecl*)(const void*, const void*, const void*);
    unsigned char ll[2] = {0xcd, 0xcd};
    void* l0 = Proc<LoadFn>(0, "?Load@LightLoader@Jet@@QAAPAVLight@2@ABVPString@2@0@Z")(&ll[0], nullptr, nullptr);
    void* l1 = Proc<LoadFn>(1, "?Load@LightLoader@Jet@@QAAPAVLight@2@ABVPString@2@0@Z")(&ll[1], nullptr, nullptr);
    Check(l0 == l1 && l0 == nullptr, "LightLoader::Load", round);
}

// ---- ViewPoint --------------------------------------------------------------------------------------------------
struct List128 {
    unsigned int count;
    unsigned int capacity;
    void* data;
};
struct ElementObj {
    unsigned char bytes[0xc];
};

struct ViewPointApi {
    void(__thiscall* ctor)(void*, List128*, void*, const void*);
    void*(__thiscall* assign)(void*, const void*);
    void(__thiscall* drop)(void*);
    void(__cdecl* add)(void*, void*, const Mat4*);
    void(__cdecl* addKey)(void*, void*, const Mat4*, unsigned long);
    void(__cdecl* addPortal)(void*, void*, const Mat4*, unsigned int);
    const void*(__cdecl* getCamera)(const void*);
    const void*(__cdecl* getPortal)(const void*);
    void*(__cdecl* getKernel)(const void*);
    const void*(__cdecl* getViewPort)(const void*);
    void(__cdecl* removeAll)(void*);
    void(__cdecl* update)(void*, void*);
    int(__cdecl* calcMip)(void*, void*, float);
};

static ViewPointApi LoadViewPoint(int side)
{
    ViewPointApi a{};
    a.ctor = Proc<decltype(a.ctor)>(side, "??0ViewPoint@Jet@@QAE@PAV?$Array@UVisibleElement@Jet@@$0IA@@1@PAVRenderKernel@1@PBVViewPort@1@@Z");
    a.assign = Proc<decltype(a.assign)>(side, "??4ViewPoint@Jet@@QAEAAV01@ABV01@@Z");
    a.drop = Proc<decltype(a.drop)>(side, "??1ViewPoint@Jet@@QAE@XZ");
    a.add = Proc<decltype(a.add)>(side, "?Add@ViewPoint@Jet@@QAAXPAVElement@2@ABVMatrix4@2@@Z");
    a.addKey = Proc<decltype(a.addKey)>(side, "?Add@ViewPoint@Jet@@QAAXPAVElement@2@ABVMatrix4@2@K@Z");
    a.addPortal = Proc<decltype(a.addPortal)>(side, "?AddPortal@ViewPoint@Jet@@QAAXPAVPortal@2@ABVMatrix4@2@I@Z");
    a.getCamera = Proc<decltype(a.getCamera)>(side, "?GetCamera@ViewPoint@Jet@@QBAPBVCamera@2@XZ");
    a.getPortal = Proc<decltype(a.getPortal)>(side, "?GetPortal@ViewPoint@Jet@@QBAPBVPortal@2@XZ");
    a.getKernel = Proc<decltype(a.getKernel)>(side, "?GetRenderKernel@ViewPoint@Jet@@QBAPAVRenderKernel@2@XZ");
    a.getViewPort = Proc<decltype(a.getViewPort)>(side, "?GetViewPort@ViewPoint@Jet@@QBAPBVViewPort@2@XZ");
    a.removeAll = Proc<decltype(a.removeAll)>(side, "?RemoveAll@ViewPoint@Jet@@QAAXXZ");
    a.update = Proc<decltype(a.update)>(side, "?Update@ViewPoint@Jet@@QAAXPAVCamera@2@@Z");
    a.calcMip = Proc<decltype(a.calcMip)>(side, "?CalcWantedMipLevelsForCull@ViewPoint@Jet@@SAHAAV12@AAVParticle@2@M@Z");
    return a;
}

static void ViewPointRound(const ViewPointApi* api, bool awkward, int round)
{
    using ElemCtor = void(__thiscall*)(void*);
    ElemCtor elemCtor = ProcRS<ElemCtor>("??0Element@Jet@@QAE@XZ");
    ElementObj elem;
    std::memset(&elem, 0xcd, sizeof elem);
    elemCtor(&elem);

    List128 list[2];
    std::memset(list, 0, sizeof list);
    ViewPointObj vp[2];
    std::memset(vp, 0xcd, sizeof vp);
    for (int v = 0; v < 2; ++v)
        api[v].ctor(&vp[v], &list[v], reinterpret_cast<void*>(0x1000 + v), reinterpret_cast<void*>(0x2000 + v));
    // The kernel, list and viewport pointers necessarily differ; every other field must match.
    for (int off = 4; off < 0x70; off += 4) {
        if (off == 8 || off == 0x18) continue;
        CheckBytes(vp[0].bytes + off, vp[1].bytes + off, 4, "ViewPoint(ctor)", round);
    }
    Check(api[0].getKernel(&vp[0]) == reinterpret_cast<void*>(0x1000) && api[1].getKernel(&vp[1]) == reinterpret_cast<void*>(0x1001),
          "ViewPoint::GetRenderKernel", round);
    Check(api[0].getViewPort(&vp[0]) == reinterpret_cast<void*>(0x2000) && api[1].getViewPort(&vp[1]) == reinterpret_cast<void*>(0x2001),
          "ViewPoint::GetViewPort", round);
    Check(api[0].getCamera(&vp[0]) == nullptr && api[1].getCamera(&vp[1]) == nullptr, "ViewPoint::GetCamera", round);
    Check(api[0].getPortal(&vp[0]) == nullptr && api[1].getPortal(&vp[1]) == nullptr, "ViewPoint::GetPortal", round);

    const Mat4 m = RandomMat4(awkward);
    api[0].add(&vp[0], &elem, &m);
    api[1].add(&vp[1], &elem, &m);
    api[0].addKey(&vp[0], &elem, &m, 0xabcdef01u);
    api[1].addKey(&vp[1], &elem, &m, 0xabcdef01u);
    Check(list[0].count == list[1].count && list[0].count == 2, "ViewPoint::Add count", round);
    CheckBytes(list[0].data, list[1].data, list[0].count * 0x48u, "ViewPoint::Add list", round);
    CheckBytes(vp[0].bytes + 0x60, vp[1].bytes + 0x60, 8, "ViewPoint::Add counters", round);

    // A copied ViewPoint keeps the same counters and matrix.
    ViewPointObj copy[2];
    std::memset(copy, 0xcd, sizeof copy);
    for (int v = 0; v < 2; ++v) api[v].assign(&copy[v], &vp[v]);
    for (int off = 4; off < 0x70; off += 4) {
        if (off == 8 || off == 0x18) continue;
        CheckBytes(copy[0].bytes + off, copy[1].bytes + off, 4, "ViewPoint::operator=", round);
    }

    // AddPortal: the new child copies the matrix and the parent links.
    const Mat4 pm = RandomMat4(awkward);
    api[0].addPortal(&vp[0], nullptr, &pm, 7);
    api[1].addPortal(&vp[1], nullptr, &pm, 7);
    const void* child0 = *reinterpret_cast<void* const*>(vp[0].bytes + 0x10);
    const void* child1 = *reinterpret_cast<void* const*>(vp[1].bytes + 0x10);
    Check((child0 != nullptr) == (child1 != nullptr), "ViewPoint::AddPortal child", round);
    if (child0 && child1) {
        const unsigned char* c0 = static_cast<const unsigned char*>(child0);
        const unsigned char* c1 = static_cast<const unsigned char*>(child1);
        for (int off = 4; off < 0x70; off += 4) {
            if (off == 8 || off == 0x18) continue;
            CheckBytes(c0 + off, c1 + off, 4, "ViewPoint::AddPortal child fields", round);
        }
        CheckBytes(c0 + 0x1c, c1 + 0x1c, 0x40, "ViewPoint::AddPortal child matrix", round);
        CheckBytes(c0 + 0x5c, c1 + 0x5c, 4, "ViewPoint::AddPortal child depth-arg", round);
        CheckBytes(c0 + 0x68, c1 + 0x68, 4, "ViewPoint::AddPortal child depth", round);
    }
    api[0].removeAll(&vp[0]);
    api[1].removeAll(&vp[1]);
    CheckBytes(vp[0].bytes + 0x0c, vp[1].bytes + 0x0c, 8, "ViewPoint::RemoveAll links", round);
    CheckBytes(vp[0].bytes + 0x60, vp[1].bytes + 0x60, 8, "ViewPoint::RemoveAll counters", round);
    Check(list[0].count == list[1].count && list[0].count == 0, "ViewPoint::RemoveAll list", round);
    for (int v = 0; v < 2; ++v) {
        if (list[v].data) {
            using DeleteFn = void(__cdecl*)(void*);
            ProcKS<DeleteFn>("?Delete@Alloc@Jet@@SAXPAX@Z")(list[v].data);
        }
        api[v].drop(&vp[v]);
        api[v].drop(&copy[v]);
    }
}

// ---- Camera -----------------------------------------------------------------------------------------------------
struct CameraTest {
    RayObj GetRay(float x, float y) const;
};
struct CameraApi {
    void(__thiscall* ctor0)(void*);
    void(__thiscall* ctor3)(void*, float, float, float);
    void(__thiscall* ctorCopyMatrix)(void*, const void*, const Mat4*);
    void(__thiscall* ctorQuad)(void*, const void*, const Vec2*, const Vec2*, const Vec3*, float);
    void(__thiscall* ctorPoints)(void*, const void*, const Vec3*, const Mat4*);
    void(__thiscall* ctorPoints2)(void*, const void*, const Vec3*, const Mat4*, const Vec3*, bool);
    void(__cdecl* setPosition)(void*, const Vec3*);
    void(__cdecl* setOrientation)(void*, const Quat*);
    void(__cdecl* setPerspective)(void*, float, float);
    void(__cdecl* setTransition)(void*, float, float, float, float);
    void(__cdecl* setOrthographic)(void*, float, float);
    RayObj(__cdecl CameraTest::*getRay)(float, float) const;
};

static CameraApi LoadCamera(int side)
{
    CameraApi a{};
    a.ctor0 = Proc<decltype(a.ctor0)>(side, "??0Camera@Jet@@QAE@XZ");
    a.ctor3 = Proc<decltype(a.ctor3)>(side, "??0Camera@Jet@@QAE@MMM@Z");
    a.ctorCopyMatrix = Proc<decltype(a.ctorCopyMatrix)>(side, "??0Camera@Jet@@QAE@ABV01@ABVMatrix4@1@@Z");
    a.ctorQuad = Proc<decltype(a.ctorQuad)>(side, "??0Camera@Jet@@QAE@ABV01@ABVVector2@1@1ABVVector3@1@M@Z");
    a.ctorPoints = Proc<decltype(a.ctorPoints)>(side, "??0Camera@Jet@@QAE@ABV01@PBVVector3@1@ABVMatrix4@1@@Z");
    a.ctorPoints2 = Proc<decltype(a.ctorPoints2)>(side, "??0Camera@Jet@@QAE@ABV01@PBVVector3@1@ABVMatrix4@1@ABV21@_N@Z");
    a.setPosition = Proc<decltype(a.setPosition)>(side, "?SetPosition@Camera@Jet@@UAAXABVVector3@2@@Z");
    a.setOrientation = Proc<decltype(a.setOrientation)>(side, "?SetOrientation@Camera@Jet@@UAAXABVQuaternion@2@@Z");
    a.setPerspective = Proc<decltype(a.setPerspective)>(side, "?SetPerspective@Camera@Jet@@QAAXMM@Z");
    a.setTransition = Proc<decltype(a.setTransition)>(side, "?SetTransition@Camera@Jet@@QAAXMMMM@Z");
    a.setOrthographic = Proc<decltype(a.setOrthographic)>(side, "?SetOrthographic@Camera@Jet@@QAAXMM@Z");
    a.getRay = MemberProc<decltype(a.getRay)>(side, "?GetRay@Camera@Jet@@QBA?AVRay@2@MM@Z");
    return a;
}

static void CameraRound(const CameraApi* api, bool awkward, int round)
{
    CameraObj cam[2];
    std::memset(cam, 0xcd, sizeof cam);
    for (int v = 0; v < 2; ++v) api[v].ctor3(&cam[v], 0.3f, 0.195f, 0.265f);

    const Vec3 pos = RandomVec(awkward);
    const Quat quat = RandomQuat(awkward);
    for (int v = 0; v < 2; ++v) {
        api[v].setPosition(&cam[v], &pos);
        api[v].setOrientation(&cam[v], &quat);
    }

    const float x = awkward ? Awkward() : FRange(-1.0f, 1.0f);
    const float y = awkward ? Awkward() : FRange(-1.0f, 1.0f);
    const RayObj ra = (reinterpret_cast<const CameraTest*>(&cam[0])->*api[0].getRay)(x, y);
    const RayObj rb = (reinterpret_cast<const CameraTest*>(&cam[1])->*api[1].getRay)(x, y);
    CheckBytes(ra.bytes + 4, rb.bytes + 4, 0x18, "Camera::GetRay (perspective)", round);
    CheckBytes(cam[0].bytes + 0x238, cam[1].bytes + 0x238, 0x24, "Camera::GetRay cached orientation", round);

    for (int v = 0; v < 2; ++v) api[v].setOrthographic(&cam[v], 1.5f, 2.0f);
    const RayObj oa = (reinterpret_cast<const CameraTest*>(&cam[0])->*api[0].getRay)(x, y);
    const RayObj ob = (reinterpret_cast<const CameraTest*>(&cam[1])->*api[1].getRay)(x, y);
    CheckBytes(oa.bytes + 4, ob.bytes + 4, 0x18, "Camera::GetRay (orthographic)", round);

    for (int v = 0; v < 2; ++v) api[v].setTransition(&cam[v], 0.5f, 1.0f, 0.7f, 1.3f);
    const RayObj ta = (reinterpret_cast<const CameraTest*>(&cam[0])->*api[0].getRay)(x, y);
    const RayObj tb = (reinterpret_cast<const CameraTest*>(&cam[1])->*api[1].getRay)(x, y);
    CheckBytes(ta.bytes + 4, tb.bytes + 4, 0x18, "Camera::GetRay (transition)", round);

    // copy + cell transform: compare everything but the vftable (and the two matrix caches that depend on the
    // source's dirty flags; both are compared, they must match).
    CameraObj copy[2];
    std::memset(copy, 0xcd, sizeof copy);
    const Mat4 transform = RandomMat4(awkward);
    for (int v = 0; v < 2; ++v) api[v].ctorCopyMatrix(&copy[v], &cam[v], &transform);
    CheckBytes(copy[0].bytes + 0x2c, copy[1].bytes + 0x2c, 7 * 4, "Camera(copy,Matrix4) position", round);
    CheckBytes(copy[0].bytes + 0x12c, copy[1].bytes + 0x12c, 0x264 - 0x12c, "Camera(copy,Matrix4)", round);

    // the three portal copies (forwarded to the original export)
    CameraObj extra[2];
    const Vec2 v2a{FRange(-2.0f, 2.0f), FRange(-2.0f, 2.0f)};
    const Vec2 v2b{FRange(-2.0f, 2.0f), FRange(-2.0f, 2.0f)};
    const Vec3 v3 = RandomVec(awkward);
    const Vec3 points[4] = {RandomVec(awkward), RandomVec(awkward), RandomVec(awkward), RandomVec(awkward)};
    const float nearDistance = awkward ? Awkward() : FRange(0.1f, 1.0f);
    std::memset(extra, 0xcd, sizeof extra);
    for (int v = 0; v < 2; ++v) api[v].ctorQuad(&extra[v], &cam[v], &v2a, &v2b, &v3, nearDistance);
    CheckBytes(extra[0].bytes + 0x2c, extra[1].bytes + 0x2c, 7 * 4, "Camera(copy,Vector2,Vector2,Vector3,float) position", round);
    CheckBytes(extra[0].bytes + 0x12c, extra[1].bytes + 0x12c, 0x264 - 0x12c, "Camera(copy,Vector2,Vector2,Vector3,float)", round);

    std::memset(extra, 0xcd, sizeof extra);
    for (int v = 0; v < 2; ++v) api[v].ctorPoints(&extra[v], &cam[v], points, &transform);
    CheckBytes(extra[0].bytes + 0x2c, extra[1].bytes + 0x2c, 7 * 4, "Camera(copy,points,Matrix4) position", round);
    CheckBytes(extra[0].bytes + 0x12c, extra[1].bytes + 0x12c, 0x264 - 0x12c, "Camera(copy,points,Matrix4)", round);

    std::memset(extra, 0xcd, sizeof extra);
    for (int v = 0; v < 2; ++v)
        api[v].ctorPoints2(&extra[v], &cam[v], points, &transform, &v3, (round & 1) != 0);
    CheckBytes(extra[0].bytes + 0x2c, extra[1].bytes + 0x2c, 7 * 4, "Camera(copy,points,Matrix4,Vector3,bool) position", round);
    CheckBytes(extra[0].bytes + 0x12c, extra[1].bytes + 0x12c, 0x264 - 0x12c, "Camera(copy,points,Matrix4,Vector3,bool)", round);
}

// ViewPoint::CalcWantedMipLevelsForCull on a camera as the particle.
static void MipRound(const ViewPointApi* vapi, const CameraApi* capi, bool awkward, int round)
{
    CameraObj cam[2];
    std::memset(cam, 0xcd, sizeof cam);
    for (int v = 0; v < 2; ++v) {
        capi[v].ctor3(&cam[v], 0.3f, 0.195f, 0.265f);
        const Vec3 pos = RandomVec(awkward);
        const Quat q = RandomQuat(awkward);
        capi[v].setPosition(&cam[v], &pos);
        capi[v].setOrientation(&cam[v], &q);
    }
    List128 list[2];
    std::memset(list, 0, sizeof list);
    ViewPointObj vp[2];
    std::memset(vp, 0xcd, sizeof vp);
    const float pixelSize = awkward ? Awkward() : FRange(0.05f, 2.0f);
    for (int v = 0; v < 2; ++v) {
        vapi[v].ctor(&vp[v], &list[v], reinterpret_cast<void*>(0x1000 + v), reinterpret_cast<void*>(0x2000 + v));
        *reinterpret_cast<void**>(vp[v].bytes + 4) = &cam[v];
        *reinterpret_cast<float*>(vp[v].bytes + 0x6c) = pixelSize;
    }
    // Give both embedded bounding volumes the same global sphere so the test exercises CalcWantedMipLevelsForCull
    // and not the Particle unit's recalculation.
    void* bvs[2] = {*reinterpret_cast<void**>(cam[0].bytes + 0x4c), *reinterpret_cast<void**>(cam[1].bytes + 0x4c)};
    const Vec3 gc = RandomVec(awkward);
    const Vec3 gp = RandomVec(awkward);
    const Quat gq{0.0f, 0.0f, 0.0f, 1.0f};
    const float bvRadius = awkward ? Awkward() : FRange(0.1f, 2.0f);
    for (int v = 0; v < 2; ++v) {
        *reinterpret_cast<unsigned long*>(reinterpret_cast<char*>(bvs[v]) + 4) = 0;
        *reinterpret_cast<float*>(reinterpret_cast<char*>(bvs[v]) + 0x18) = bvRadius;
        *reinterpret_cast<float*>(reinterpret_cast<char*>(bvs[v]) + 0x1c) = gc.x;
        *reinterpret_cast<float*>(reinterpret_cast<char*>(bvs[v]) + 0x20) = gc.y;
        *reinterpret_cast<float*>(reinterpret_cast<char*>(bvs[v]) + 0x24) = gc.z;
        *reinterpret_cast<float*>(reinterpret_cast<char*>(bvs[v]) + 0x7c) = gp.x;
        *reinterpret_cast<float*>(reinterpret_cast<char*>(bvs[v]) + 0x80) = gp.y;
        *reinterpret_cast<float*>(reinterpret_cast<char*>(bvs[v]) + 0x84) = gp.z;
        *reinterpret_cast<float*>(reinterpret_cast<char*>(bvs[v]) + 0x88) = gq.x;
        *reinterpret_cast<float*>(reinterpret_cast<char*>(bvs[v]) + 0x8c) = gq.y;
        *reinterpret_cast<float*>(reinterpret_cast<char*>(bvs[v]) + 0x90) = gq.z;
        *reinterpret_cast<float*>(reinterpret_cast<char*>(bvs[v]) + 0x94) = gq.w;
    }
    const float f = awkward ? Awkward() : FRange(0.1f, 4.0f);
    const int a = vapi[0].calcMip(&vp[0], &cam[0], f);
    const int b = vapi[1].calcMip(&vp[1], &cam[1], f);
    Check(a == b, "ViewPoint::CalcWantedMipLevelsForCull", round);
}

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* dir = argc > 1 ? argv[1] : ".";
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 200;
    std::string path = dir;
    LoadLibraryA((path + "\\Render3DCore_7c_orig.dll").c_str());
    mod[0] = LoadLibraryA((path + "\\Render3DCore_7c_orig.dll").c_str());
    mod[1] = LoadLibraryA((path + "\\Render3DCore_7c.dll").c_str());
    if (!mod[0] || !mod[1]) {
        std::printf("cannot load the modules from %s\n", dir);
        return 2;
    }
    LoadLibraryA((path + "\\RenderSystem_7c.dll").c_str());
    renderSystem = LoadLibraryA((path + "\\RenderSystem_7c.dll").c_str());
    LoadLibraryA((path + "\\KernelStandard_7c.dll").c_str());
    kernelStandard = LoadLibraryA((path + "\\KernelStandard_7c.dll").c_str());
    LoadLibraryA((path + "\\Kernel_7c.dll").c_str());
    LoadLibraryA((path + "\\KernelResource_7c.dll").c_str());
    LoadLibraryA((path + "\\DebugInterface_7c.dll").c_str());

    const TransformApi transform[2] = {LoadTransform(0), LoadTransform(1)};
    const TubeApi tube[2] = {LoadTube(0), LoadTube(1)};
    const ViewPointApi viewPoint[2] = {LoadViewPoint(0), LoadViewPoint(1)};
    const CameraApi camera[2] = {LoadCamera(0), LoadCamera(1)};

    const char* only = argc > 3 ? argv[3] : "tuvcsm";
    for (int round = 0; round < rounds; ++round) {
        const bool awkward = (round % 7) == 6;
        if (std::strchr(only, 't')) {
            rng.seed(0x51ee7u + static_cast<unsigned>(round));
            TransformRound(transform, awkward, round);
        }
        if (std::strchr(only, 'u')) {
            rng.seed(0x71a3u + static_cast<unsigned>(round));
            TubeRound(tube, awkward, round);
        }
        if (std::strchr(only, 'v')) {
            rng.seed(0x9b1du + static_cast<unsigned>(round));
            ViewPointRound(viewPoint, awkward, round);
        }
        if (std::strchr(only, 'c')) {
            rng.seed(0xc0ffeeu + static_cast<unsigned>(round));
            CameraRound(camera, awkward, round);
        }
        if (std::strchr(only, 'm')) {
            rng.seed(0x1234u + static_cast<unsigned>(round));
            MipRound(viewPoint, camera, awkward, round);
        }
        if (std::strchr(only, 's')) {
            rng.seed(0x5eedu + static_cast<unsigned>(round));
            SimpleRound(round);
        }
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad == 0 ? 0 : 1;
}
