// Differential test of Render3DCore_7c workstream U1d2: Polygon, Sphere, Ellipsoid, Ellipse, Cylinder, Cone and
// Circle against Render3DCore_7c_orig.dll in one process.
//
// Every local export is called through GetProcAddress on the same inputs for both modules. Return values and raw
// float bits (quiet/signalling NaNs, infinities, signed zero) are compared. Objects are compared field by field
// (the vftable pointer and the allocated vertex pointer differ by module/allocator, so the vtable is checked
// against each module's exported vftable and vertex storage by contents).
//
// usage: diff_r3d_u1d2.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

struct V2 {
    float x, y;
};
struct V3 {
    float x, y, z;
};
struct PlaneL {
    V3 n;
    float d;
};
struct SphereL {
    void* vt;
    V3 c;
    float r;
};
struct CircleL {
    V3 n;
    float d;
    V3 c;
    float r;
};
struct CylinderL {
    void* vt;
    V3 c[2];
    float r;
};
struct ConeL {
    V3 c[2];
    float r;
    float cosTheta;
};
struct EllipseL {
    V3 n;
    float d;
    V3 c;
    float major, minor;
    V3 point;
};
struct EllipsoidL {
    void* vt;
    V3 n;
    float d;
    V3 c;
    float major, minor, up;
    V3 point;
};
struct PolyL {
    void* vt;
    V3 n;
    float d;
    int num;
    V3* verts;
};
struct LineL {
    void* vt;
    V3 dir;
    V3 point;
};
struct BoxL {
    V3 min, max;
};
struct Obj {
    int dummy;
};
static_assert(sizeof(V2) == 8, "V2");
static_assert(sizeof(V3) == 0xc, "V3");
static_assert(sizeof(SphereL) == 0x14, "Sphere");
static_assert(sizeof(CircleL) == 0x20, "Circle");
static_assert(sizeof(CylinderL) == 0x20, "Cylinder");
static_assert(sizeof(ConeL) == 0x20, "Cone");
static_assert(sizeof(EllipseL) == 0x30, "Ellipse");
static_assert(sizeof(EllipsoidL) == 0x38, "Ellipsoid");
static_assert(sizeof(PolyL) == 0x1c, "Polygon");
static_assert(sizeof(LineL) == 0x1c, "Line");
static_assert(sizeof(BoxL) == 0x18, "Box");

static HMODULE mod[2];
static void* vft[2][4];  // Sphere, Ellipsoid, Cylinder, Polygon
static unsigned long long checks, bad;
static std::mt19937 rng(0x51d2);

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

static float F(bool awkward)
{
    return awkward ? Awkward() : FRange(-20.0f, 20.0f);
}

static V3 RV(bool awkward)
{
    return V3{F(awkward), F(awkward), F(awkward)};
}

template <class T>
static T Proc(int side, const char* name)
{
    auto p = reinterpret_cast<T>(GetProcAddress(mod[side], name));
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
        if (bad < 40)
            std::printf("MISMATCH %s\n", what);
    }
}

static uint32_t FBits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

static void CheckBytes(const void* a, const void* b, size_t n, const char* what)
{
    const bool ok = std::memcmp(a, b, n) == 0;
    if (!ok) {
        ++checks;
        ++bad;
        if (bad < 40) {
            std::printf("MISMATCH %s:", what);
            const auto* pa = static_cast<const unsigned char*>(a);
            const auto* pb = static_cast<const unsigned char*>(b);
            for (size_t i = 0; i < n; ++i)
                if (pa[i] != pb[i])
                    std::printf(" [%02x] %02x/%02x", static_cast<unsigned>(i), pa[i], pb[i]);
            std::printf("\n");
        }
        return;
    }
    ++checks;
}

static void CheckF(float a, float b, const char* what)
{
    Check(FBits(a) == FBits(b), what);
    if (FBits(a) != FBits(b) && bad < 40)
        std::printf("%s: ours %08x orig %08x\n", what, FBits(a), FBits(b));
}

static void CheckV3(const V3& a, const V3& b, const char* what)
{
    CheckBytes(&a, &b, sizeof a, what);
}

static void Poison(void* p, size_t n)
{
    auto* w = static_cast<uint32_t*>(p);
    for (size_t i = 0; i < n / 4; ++i)
        w[i] = 0x7fc00000u;
}

// ---------------------------------------------------------------------------------------------------------------
// Circle

static void TestCircle(int round, bool awkward)
{
    // default ctor
    CircleL dc[2];
    Poison(dc, sizeof dc);
    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(CircleL*)>(s, "??0Circle@Jet@@QAE@XZ")(&dc[s]);
    CheckBytes(&dc[0], &dc[1], sizeof(CircleL), "Circle::Circle()");

    CircleL src{};
    src.n = RV(awkward);
    src.d = F(awkward);
    src.c = RV(awkward);
    src.r = F(awkward);

    // copy ctor / assignment
    for (int s = 0; s < 2; ++s) {
        Poison(&dc[s], sizeof dc[s]);
        Proc<void(__thiscall*)(CircleL*, const CircleL*)>(s, "??0Circle@Jet@@QAE@ABV01@@Z")(&dc[s], &src);
    }
    CheckBytes(&dc[0], &dc[1], sizeof(CircleL), "Circle copy ctor");

    CircleL src2{};
    src2.n = RV(awkward);
    src2.d = F(awkward);
    src2.c = RV(awkward);
    src2.r = F(awkward);
    for (int s = 0; s < 2; ++s)
        Proc<CircleL*(__thiscall*)(CircleL*, const CircleL*)>(s, "??4Circle@Jet@@QAEAAV01@ABV01@@Z")(&dc[s], &src2);
    CheckBytes(&dc[0], &dc[1], sizeof(CircleL), "Circle::operator=");

    // plane/centre/radius ctor
    PlaneL plane{};
    plane.n = RV(awkward);
    plane.d = F(awkward);
    const V3 center = RV(awkward);
    const float radius = F(awkward);
    for (int s = 0; s < 2; ++s) {
        Poison(&dc[s], sizeof dc[s]);
        Proc<void(__thiscall*)(CircleL*, const V3*, float, const PlaneL*)>(
            s, "??0Circle@Jet@@QAE@ABVVector3@1@MABVPlane@1@@Z")(&dc[s], &center, radius, &plane);
    }
    CheckBytes(&dc[0], &dc[1], sizeof(CircleL), "Circle(center,radius,plane)");

    // accessors
    const V3 setCenter = RV(awkward);
    for (int s = 0; s < 2; ++s) {
        using SetCenterFn = void(__cdecl*)(CircleL*, V3);
        Proc<SetCenterFn>(s, "?SetCenter@Circle@Jet@@QAAXVVector3@2@@Z")(&dc[s], setCenter);
    }
    CheckBytes(&dc[0], &dc[1], sizeof(CircleL), "Circle::SetCenter");
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(CircleL*, float)>(s, "?SetRadius@Circle@Jet@@QAAXM@Z")(&dc[s], radius);
    CheckBytes(&dc[0], &dc[1], sizeof(CircleL), "Circle::SetRadius");

    const V3* c0 = Proc<const V3*(__cdecl*)(const CircleL*)>(0, "?GetCenter@Circle@Jet@@QBAABVVector3@2@XZ")(&dc[0]);
    const V3* c1 = Proc<const V3*(__cdecl*)(const CircleL*)>(1, "?GetCenter@Circle@Jet@@QBAABVVector3@2@XZ")(&dc[1]);
    CheckV3(*c0, *c1, "Circle::GetCenter");
    CheckF(Proc<float(__cdecl*)(const CircleL*)>(0, "?GetRadius@Circle@Jet@@QBAMXZ")(&dc[0]),
           Proc<float(__cdecl*)(const CircleL*)>(1, "?GetRadius@Circle@Jet@@QBAMXZ")(&dc[1]), "Circle::GetRadius");

    // TestCollision(Sphere)
    SphereL sphere{};
    sphere.c = RV(awkward);
    sphere.r = F(awkward);
    const bool b0 = Proc<bool(__cdecl*)(CircleL*, const SphereL*)>(
        0, "?TestCollision@Circle@Jet@@QAA_NABVSphere@2@@Z")(&dc[0], &sphere);
    const bool b1 = Proc<bool(__cdecl*)(CircleL*, const SphereL*)>(
        1, "?TestCollision@Circle@Jet@@QAA_NABVSphere@2@@Z")(&dc[1], &sphere);
    Check(b0 == b1, "Circle::TestCollision(Sphere)");
    (void)round;
}

// ---------------------------------------------------------------------------------------------------------------
// Cone

static void TestCone(int round, bool awkward)
{
    ConeL dc[2];
    Poison(dc, sizeof dc);
    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(ConeL*)>(s, "??0Cone@Jet@@QAE@XZ")(&dc[s]);
    CheckBytes(&dc[0], &dc[1], sizeof(ConeL), "Cone::Cone()");

    const V3 c0 = RV(awkward);
    V3 c1 = RV(awkward);
    if ((round % 5) == 0)
        c1 = c0;
    const float radius = F(awkward);
    for (int s = 0; s < 2; ++s) {
        Poison(&dc[s], sizeof dc[s]);
        Proc<void(__thiscall*)(ConeL*, const V3*, const V3*, float)>(
            s, "??0Cone@Jet@@QAE@ABVVector3@1@0M@Z")(&dc[s], &c0, &c1, radius);
    }
    CheckBytes(&dc[0], &dc[1], sizeof(ConeL), "Cone(center1,center2,radius)");

    for (int s = 0; s < 2; ++s)
        Proc<ConeL*(__thiscall*)(ConeL*, const ConeL*)>(s, "??4Cone@Jet@@QAEAAV01@ABV01@@Z")(&dc[s], &dc[s ^ 1]);
    CheckBytes(&dc[0], &dc[1], sizeof(ConeL), "Cone::operator=");

    // Re-seed both to a known state.
    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(ConeL*, const V3*, const V3*, float)>(
            s, "??0Cone@Jet@@QAE@ABVVector3@1@0M@Z")(&dc[s], &c0, &c1, radius);

    for (int i = 0; i < 2; ++i) {
        const V3& r0 = Proc<const V3&(__cdecl*)(const ConeL*, int)>(0, "?GetCenter@Cone@Jet@@QBAABVVector3@2@H@Z")(&dc[0], i);
        const V3& r1 = Proc<const V3&(__cdecl*)(const ConeL*, int)>(1, "?GetCenter@Cone@Jet@@QBAABVVector3@2@H@Z")(&dc[1], i);
        CheckV3(r0, r1, "Cone::GetCenter(int) const");
    }
    using ConeValueFn = V3(__cdecl Obj::*)(int);
    ConeValueFn cv0 = MemberProc<ConeValueFn>(0, "?GetCenter@Cone@Jet@@QAA?AVVector3@2@H@Z");
    ConeValueFn cv1 = MemberProc<ConeValueFn>(1, "?GetCenter@Cone@Jet@@QAA?AVVector3@2@H@Z");
    CheckV3((reinterpret_cast<Obj*>(&dc[0])->*cv0)(1), (reinterpret_cast<Obj*>(&dc[1])->*cv1)(1), "Cone::GetCenter(int)");
    V3 outs[2] = {};
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(const ConeL*, V3*, int)>(s, "?GetCenter@Cone@Jet@@QBAXAAVVector3@2@H@Z")(&dc[s], &outs[s], 0);
    CheckV3(outs[0], outs[1], "Cone::GetCenter(Vector3&,int)");
    CheckF(Proc<float(__cdecl*)(const ConeL*)>(0, "?GetCosTheta@Cone@Jet@@QBAMXZ")(&dc[0]),
           Proc<float(__cdecl*)(const ConeL*)>(1, "?GetCosTheta@Cone@Jet@@QBAMXZ")(&dc[1]), "Cone::GetCosTheta");
    CheckF(Proc<float(__cdecl*)(const ConeL*)>(0, "?GetRadius@Cone@Jet@@QBAMXZ")(&dc[0]),
           Proc<float(__cdecl*)(const ConeL*)>(1, "?GetRadius@Cone@Jet@@QBAMXZ")(&dc[1]), "Cone::GetRadius");

    const V3 setCenter = RV(awkward);
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(ConeL*, const V3*, int)>(s, "?SetCenter@Cone@Jet@@QAAXABVVector3@2@H@Z")(&dc[s], &setCenter,
                                                                                                    round & 1);
    CheckBytes(&dc[0], &dc[1], sizeof(ConeL), "Cone::SetCenter");
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(ConeL*, float)>(s, "?SetRadius@Cone@Jet@@QAAXM@Z")(&dc[s], radius);
    CheckBytes(&dc[0], &dc[1], sizeof(ConeL), "Cone::SetRadius");
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(ConeL*)>(s, "?CalculateCosTheta@Cone@Jet@@IAAXXZ")(&dc[s]);
    CheckBytes(&dc[0], &dc[1], sizeof(ConeL), "Cone::CalculateCosTheta");

    const V3 point = RV(awkward);
    const bool b0 = Proc<bool(__cdecl*)(ConeL*, const V3*)>(0, "?TestCollision@Cone@Jet@@QAA_NABVVector3@2@@Z")(&dc[0], &point);
    const bool b1 = Proc<bool(__cdecl*)(ConeL*, const V3*)>(1, "?TestCollision@Cone@Jet@@QAA_NABVVector3@2@@Z")(&dc[1], &point);
    Check(b0 == b1, "Cone::TestCollision(Vector3)");
}

// ---------------------------------------------------------------------------------------------------------------
// Cylinder

static void TestCylinder(int round, bool awkward)
{
    CylinderL dc[2];
    Poison(dc, sizeof dc);
    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(CylinderL*)>(s, "??0Cylinder@Jet@@QAE@XZ")(&dc[s]);
    Check(*reinterpret_cast<void**>(&dc[0]) == vft[0][2], "Cylinder default vtable");
    Check(*reinterpret_cast<void**>(&dc[1]) == vft[1][2], "Cylinder default vtable orig");
    CheckBytes(&dc[0].c, &dc[1].c, sizeof dc[0].c, "Cylinder() centers");
    CheckF(dc[0].r, dc[1].r, "Cylinder() radius");

    const V3 c0 = RV(awkward);
    const V3 c1 = RV(awkward);
    const float radius = F(awkward);
    for (int s = 0; s < 2; ++s) {
        Poison(&dc[s], sizeof dc[s]);
        Proc<void(__thiscall*)(CylinderL*, const V3*, const V3*, float)>(
            s, "??0Cylinder@Jet@@QAE@ABVVector3@1@0M@Z")(&dc[s], &c0, &c1, radius);
    }
    CheckBytes(&dc[0].c, &dc[1].c, sizeof dc[0].c, "Cylinder(centers,radius)");
    CheckF(dc[0].r, dc[1].r, "Cylinder(centers,radius) radius");

    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(CylinderL*, const CylinderL*)>(s, "??0Cylinder@Jet@@QAE@ABV01@@Z")(&dc[s], &dc[s ^ 1]);
    CheckBytes(&dc[0].c, &dc[1].c, sizeof dc[0].c, "Cylinder copy ctor");
    CheckF(dc[0].r, dc[1].r, "Cylinder copy ctor radius");
    Check(*reinterpret_cast<void**>(&dc[0]) == vft[0][2], "Cylinder copy vtable");

    const CylinderL* src = &dc[0];
    for (int s = 0; s < 2; ++s)
        Proc<CylinderL*(__thiscall*)(CylinderL*, const CylinderL*)>(s, "??4Cylinder@Jet@@QAEAAV01@ABV01@@Z")(&dc[s], src);
    CheckBytes(&dc[0].c, &dc[1].c, sizeof dc[0].c, "Cylinder::operator=");
    CheckF(dc[0].r, dc[1].r, "Cylinder::operator= radius");

    V3 centersIn[2] = {RV(awkward), RV(awkward)};
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(CylinderL*, const V3*)>(s, "?SetCenters@Cylinder@Jet@@QAAXQBVVector3@2@@Z")(&dc[s],
                                                                                                        centersIn);
    CheckBytes(&dc[0].c, &dc[1].c, sizeof dc[0].c, "Cylinder::SetCenters");
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(CylinderL*, float)>(s, "?SetRadius@Cylinder@Jet@@QAAXM@Z")(&dc[s], radius);
    CheckF(dc[0].r, dc[1].r, "Cylinder::SetRadius");
    CheckF(Proc<float(__cdecl*)(const CylinderL*)>(0, "?GetRadius@Cylinder@Jet@@QBAMXZ")(&dc[0]),
           Proc<float(__cdecl*)(const CylinderL*)>(1, "?GetRadius@Cylinder@Jet@@QBAMXZ")(&dc[1]),
           "Cylinder::GetRadius");
    const V3* gc0 = Proc<const V3*(__cdecl*)(const CylinderL*)>(0, "?GetCenters@Cylinder@Jet@@QBAPBVVector3@2@XZ")(&dc[0]);
    const V3* gc1 = Proc<const V3*(__cdecl*)(const CylinderL*)>(1, "?GetCenters@Cylinder@Jet@@QBAPBVVector3@2@XZ")(&dc[1]);
    CheckBytes(gc0, gc1, sizeof(V3) * 2, "Cylinder::GetCenters");

    // virtual slots (local wrappers for the orig or the false stub)
    BoxL box{};
    box.min = RV(awkward);
    box.max = RV(awkward);
    Check(Proc<bool(__cdecl*)(const CylinderL*, const BoxL*)>(0,
                                                              "?TestCollision@Cylinder@Jet@@UBA_NABVAxisAlignedBox@2@@Z")(&dc[0], &box) ==
              Proc<bool(__cdecl*)(const CylinderL*, const BoxL*)>(1,
                                                                  "?TestCollision@Cylinder@Jet@@UBA_NABVAxisAlignedBox@2@@Z")(&dc[1], &box),
          "Cylinder::TestCollision(Box)");

    CylinderL other{};
    other.c[0] = RV(awkward);
    other.c[1] = RV(awkward);
    other.r = F(awkward);
    bool gcv[2] = {false, false};
    for (int s = 0; s < 2; ++s)
        gcv[s] = Proc<bool(__cdecl*)(const CylinderL*, const CylinderL*)>(
            s, "?TestCollision@Cylinder@Jet@@UBA_NABV12@@Z")(&dc[s], &other);
    Check(gcv[0] == gcv[1], "Cylinder::TestCollision(Cylinder)");

    SphereL sphere{};
    sphere.c = RV(awkward);
    sphere.r = F(awkward);
    Check(Proc<bool(__cdecl*)(const CylinderL*, const SphereL*)>(0,
                                                                 "?TestCollision@Cylinder@Jet@@UBA_NABVSphere@2@@Z")(&dc[0], &sphere) ==
              Proc<bool(__cdecl*)(const CylinderL*, const SphereL*)>(1,
                                                                     "?TestCollision@Cylinder@Jet@@UBA_NABVSphere@2@@Z")(&dc[1], &sphere),
          "Cylinder::TestCollision(Sphere)");

    EllipsoidL ell{};
    ell.n = RV(awkward);
    ell.d = F(awkward);
    ell.c = RV(awkward);
    ell.major = F(awkward);
    ell.minor = F(awkward);
    ell.up = F(awkward);
    ell.point = RV(awkward);
    bool ellHit[2] = {false, false};
    for (int s = 0; s < 2; ++s) {
        ell.vt = vft[s][1];
        ellHit[s] = Proc<bool(__cdecl*)(const CylinderL*, const EllipsoidL*)>(
            s, "?TestCollision@Cylinder@Jet@@UBA_NABVEllipsoid@2@@Z")(&dc[s], &ell);
    }
    Check(ellHit[0] == ellHit[1], "Cylinder::TestCollision(Ellipsoid)");

    LineL line{};
    line.vt = vft[0][0];  // unused by the two field-only paths below
    line.dir = RV(awkward);
    line.point = RV(awkward);
    Check(Proc<bool(__cdecl*)(CylinderL*, const LineL*)>(0,
                                                         "?TestCollision@Cylinder@Jet@@UAA_NABVLine@2@@Z")(&dc[0], &line) ==
              Proc<bool(__cdecl*)(CylinderL*, const LineL*)>(1,
                                                             "?TestCollision@Cylinder@Jet@@UAA_NABVLine@2@@Z")(&dc[1], &line),
          "Cylinder::TestCollision(Line)");
    const V3 point = RV(awkward);
    Check(Proc<bool(__cdecl*)(CylinderL*, const V3*)>(0,
                                                      "?TestCollision@Cylinder@Jet@@UAA_NABVVector3@2@@Z")(&dc[0], &point) ==
              Proc<bool(__cdecl*)(CylinderL*, const V3*)>(1,
                                                          "?TestCollision@Cylinder@Jet@@UAA_NABVVector3@2@@Z")(&dc[1], &point),
          "Cylinder::TestCollision(Vector3)");
}

// ---------------------------------------------------------------------------------------------------------------
// Ellipsoid

static void TestEllipsoid(int round, bool awkward)
{
    EllipsoidL dc[2];
    Poison(dc, sizeof dc);
    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(EllipsoidL*)>(s, "??0Ellipsoid@Jet@@QAE@XZ")(&dc[s]);
    Check(*reinterpret_cast<void**>(&dc[0]) == vft[0][1], "Ellipsoid default vtable");
    Check(*reinterpret_cast<void**>(&dc[1]) == vft[1][1], "Ellipsoid default vtable orig");
    CheckBytes(reinterpret_cast<char*>(&dc[0]) + 4, reinterpret_cast<char*>(&dc[1]) + 4, sizeof(EllipsoidL) - 4,
               "Ellipsoid()");

    PlaneL plane{};
    plane.n = RV(awkward);
    plane.d = F(awkward);
    const V3 center = RV(awkward);
    const V3 point = RV(awkward);
    const float major = F(awkward), minor = F(awkward), up = F(awkward);
    for (int s = 0; s < 2; ++s) {
        Poison(&dc[s], sizeof dc[s]);
        Proc<void(__thiscall*)(EllipsoidL*, const V3*, float, float, float, const PlaneL*, const V3*)>(
            s, "??0Ellipsoid@Jet@@QAE@ABVVector3@1@MMMABVPlane@1@0@Z")(&dc[s], &center, major, minor, up, &plane,
                                                                       &point);
    }
    if (round == 0) {
        std::printf("ell ctor plane %.8x %.8x %.8x %.8x major %.8x\n", FBits(plane.n.x), FBits(plane.n.y),
                    FBits(plane.n.z), FBits(plane.d), FBits(major));
        for (int s = 0; s < 2; ++s) {
            std::printf("  side %d:", s);
            for (int i = 0; i < 14; ++i)
                std::printf(" %08x", static_cast<uint32_t*>(static_cast<void*>(&dc[s]))[i]);
            std::printf("\n");
        }
    }
    CheckBytes(reinterpret_cast<char*>(&dc[0]) + 4, reinterpret_cast<char*>(&dc[1]) + 4, sizeof(EllipsoidL) - 4,
               "Ellipsoid(...)");

    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(EllipsoidL*, const EllipsoidL*)>(s, "??0Ellipsoid@Jet@@QAE@ABV01@@Z")(&dc[s],
                                                                                                    &dc[s ^ 1]);
    CheckBytes(reinterpret_cast<char*>(&dc[0]) + 4, reinterpret_cast<char*>(&dc[1]) + 4, sizeof(EllipsoidL) - 4,
               "Ellipsoid copy ctor");
    for (int s = 0; s < 2; ++s)
        Proc<EllipsoidL*(__thiscall*)(EllipsoidL*, const EllipsoidL*)>(
            s, "??4Ellipsoid@Jet@@QAEAAV01@ABV01@@Z")(&dc[s], &dc[s ^ 1]);
    CheckBytes(reinterpret_cast<char*>(&dc[0]) + 4, reinterpret_cast<char*>(&dc[1]) + 4, sizeof(EllipsoidL) - 4,
               "Ellipsoid::operator=");

    // accessors: reset both to a known state first
    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(EllipsoidL*, const V3*, float, float, float, const PlaneL*, const V3*)>(
            s, "??0Ellipsoid@Jet@@QAE@ABVVector3@1@MMMABVPlane@1@0@Z")(&dc[s], &center, major, minor, up, &plane,
                                                                       &point);
    CheckV3(*Proc<const V3*(__cdecl*)(const EllipsoidL*)>(0, "?GetCenter@Ellipsoid@Jet@@QBAABVVector3@2@XZ")(&dc[0]),
            *Proc<const V3*(__cdecl*)(const EllipsoidL*)>(1, "?GetCenter@Ellipsoid@Jet@@QBAABVVector3@2@XZ")(&dc[1]),
            "Ellipsoid::GetCenter");
    CheckF(Proc<float(__cdecl*)(const EllipsoidL*)>(0, "?GetMajor@Ellipsoid@Jet@@QBAMXZ")(&dc[0]),
           Proc<float(__cdecl*)(const EllipsoidL*)>(1, "?GetMajor@Ellipsoid@Jet@@QBAMXZ")(&dc[1]),
           "Ellipsoid::GetMajor");
    CheckF(Proc<float(__cdecl*)(const EllipsoidL*)>(0, "?GetMinor@Ellipsoid@Jet@@QBAMXZ")(&dc[0]),
           Proc<float(__cdecl*)(const EllipsoidL*)>(1, "?GetMinor@Ellipsoid@Jet@@QBAMXZ")(&dc[1]),
           "Ellipsoid::GetMinor");
    CheckF(Proc<float(__cdecl*)(const EllipsoidL*)>(0, "?GetUp@Ellipsoid@Jet@@QBAMXZ")(&dc[0]),
           Proc<float(__cdecl*)(const EllipsoidL*)>(1, "?GetUp@Ellipsoid@Jet@@QBAMXZ")(&dc[1]),
           "Ellipsoid::GetUp");

    const V3 newCenter = RV(awkward);
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(EllipsoidL*, const V3*)>(s, "?SetCenter@Ellipsoid@Jet@@QAAXABVVector3@2@@Z")(&dc[s],
                                                                                                        &newCenter);
    CheckBytes(reinterpret_cast<char*>(&dc[0]) + 4, reinterpret_cast<char*>(&dc[1]) + 4, sizeof(EllipsoidL) - 4,
               "Ellipsoid::SetCenter");
    for (int s = 0; s < 2; ++s) {
        Proc<void(__cdecl*)(EllipsoidL*, float)>(s, "?SetMajor@Ellipsoid@Jet@@QAAXM@Z")(&dc[s], major);
        Proc<void(__cdecl*)(EllipsoidL*, float)>(s, "?SetMinor@Ellipsoid@Jet@@QAAXM@Z")(&dc[s], minor);
        Proc<void(__cdecl*)(EllipsoidL*, float)>(s, "?SetUp@Ellipsoid@Jet@@QAAXM@Z")(&dc[s], up);
    }
    CheckBytes(reinterpret_cast<char*>(&dc[0]) + 4, reinterpret_cast<char*>(&dc[1]) + 4, sizeof(EllipsoidL) - 4,
               "Ellipsoid setters");

    BoxL box{};
    box.min = RV(awkward);
    box.max = RV(awkward);
    Check(Proc<bool(__cdecl*)(const EllipsoidL*, const BoxL*)>(
              0, "?TestCollision@Ellipsoid@Jet@@UBA_NABVAxisAlignedBox@2@@Z")(&dc[0], &box) ==
              Proc<bool(__cdecl*)(const EllipsoidL*, const BoxL*)>(
                  1, "?TestCollision@Ellipsoid@Jet@@UBA_NABVAxisAlignedBox@2@@Z")(&dc[1], &box),
          "Ellipsoid::TestCollision(Box)");
    CylinderL cyl{};
    cyl.c[0] = RV(awkward);
    cyl.c[1] = RV(awkward);
    cyl.r = F(awkward);
    Check(Proc<bool(__cdecl*)(const EllipsoidL*, const CylinderL*)>(
              0, "?TestCollision@Ellipsoid@Jet@@UBA_NABVCylinder@2@@Z")(&dc[0], &cyl) ==
              Proc<bool(__cdecl*)(const EllipsoidL*, const CylinderL*)>(
                  1, "?TestCollision@Ellipsoid@Jet@@UBA_NABVCylinder@2@@Z")(&dc[1], &cyl),
          "Ellipsoid::TestCollision(Cylinder)");
    SphereL sphere{};
    sphere.c = RV(awkward);
    sphere.r = F(awkward);
    Check(Proc<bool(__cdecl*)(const EllipsoidL*, const SphereL*)>(
              0, "?TestCollision@Ellipsoid@Jet@@UBA_NABVSphere@2@@Z")(&dc[0], &sphere) ==
              Proc<bool(__cdecl*)(const EllipsoidL*, const SphereL*)>(
                  1, "?TestCollision@Ellipsoid@Jet@@UBA_NABVSphere@2@@Z")(&dc[1], &sphere),
          "Ellipsoid::TestCollision(Sphere)");
    bool ellEll[2] = {false, false};
    for (int s = 0; s < 2; ++s)
        ellEll[s] = Proc<bool(__cdecl*)(const EllipsoidL*, const EllipsoidL*)>(
            s, "?TestCollision@Ellipsoid@Jet@@UBA_NABV12@@Z")(&dc[s], &dc[s]);
    Check(ellEll[0] == ellEll[1], "Ellipsoid::TestCollision(Ellipsoid)");
    PlaneL plane2{};
    plane2.n = RV(awkward);
    plane2.d = F(awkward);
    Check(Proc<bool(__cdecl*)(EllipsoidL*, const PlaneL*)>(0, "?TestCollision@Ellipsoid@Jet@@UAA_NABVPlane@2@@Z")(
              &dc[0], &plane2) ==
              Proc<bool(__cdecl*)(EllipsoidL*, const PlaneL*)>(1, "?TestCollision@Ellipsoid@Jet@@UAA_NABVPlane@2@@Z")(
                  &dc[1], &plane2),
          "Ellipsoid::TestCollision(Plane)");
    LineL line{};
    line.vt = vft[0][0];
    line.dir = RV(awkward);
    line.point = RV(awkward);
    Check(Proc<bool(__cdecl*)(EllipsoidL*, const LineL*)>(0, "?TestCollision@Ellipsoid@Jet@@UAA_NABVLine@2@@Z")(
              &dc[0], &line) ==
              Proc<bool(__cdecl*)(EllipsoidL*, const LineL*)>(1, "?TestCollision@Ellipsoid@Jet@@UAA_NABVLine@2@@Z")(
                  &dc[1], &line),
          "Ellipsoid::TestCollision(Line)");
    const V3 point3 = RV(awkward);
    Check(Proc<bool(__cdecl*)(EllipsoidL*, const V3*)>(0, "?TestCollision@Ellipsoid@Jet@@UAA_NABVVector3@2@@Z")(
              &dc[0], &point3) ==
              Proc<bool(__cdecl*)(EllipsoidL*, const V3*)>(1, "?TestCollision@Ellipsoid@Jet@@UAA_NABVVector3@2@@Z")(
                  &dc[1], &point3),
          "Ellipsoid::TestCollision(Vector3)");
    (void)round;
}

// ---------------------------------------------------------------------------------------------------------------
// Ellipse

static void TestEllipse(int round, bool awkward)
{
    EllipseL dc[2];
    Poison(dc, sizeof dc);
    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(EllipseL*)>(s, "??0Ellipse@Jet@@QAE@XZ")(&dc[s]);
    CheckBytes(&dc[0], &dc[1], sizeof(EllipseL), "Ellipse::Ellipse()");

    const V3 center = RV(awkward);
    const V3 point = RV(awkward);
    const float major = F(awkward), minor = F(awkward);
    PlaneL plane{};
    plane.n = RV(awkward);
    plane.d = F(awkward);
    for (int s = 0; s < 2; ++s) {
        Poison(&dc[s], sizeof dc[s]);
        Proc<void(__thiscall*)(EllipseL*, const V3*, float, float, const V3*, const PlaneL*)>(
            s, "??0Ellipse@Jet@@QAE@ABVVector3@1@MM0ABVPlane@1@@Z")(&dc[s], &center, major, minor, &point, &plane);
    }
    CheckBytes(&dc[0], &dc[1], sizeof(EllipseL), "Ellipse(center,major,minor,point,plane)");

    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(EllipseL*, const EllipseL*)>(s, "??0Ellipse@Jet@@QAE@ABV01@@Z")(&dc[s], &dc[s ^ 1]);
    CheckBytes(&dc[0], &dc[1], sizeof(EllipseL), "Ellipse copy ctor");
    for (int s = 0; s < 2; ++s)
        Proc<EllipseL*(__thiscall*)(EllipseL*, const EllipseL*)>(
            s, "??4Ellipse@Jet@@QAEAAV01@ABV01@@Z")(&dc[s], &dc[s ^ 1]);
    CheckBytes(&dc[0], &dc[1], sizeof(EllipseL), "Ellipse::operator=");
    (void)round;
}

// ---------------------------------------------------------------------------------------------------------------
// Sphere

static void TestSphere(int round, bool awkward)
{
    SphereL dc[2];
    Poison(dc, sizeof dc);
    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(SphereL*)>(s, "??0Sphere@Jet@@QAE@XZ")(&dc[s]);
    Check(*reinterpret_cast<void**>(&dc[0]) == vft[0][0], "Sphere default vtable");
    Check(*reinterpret_cast<void**>(&dc[1]) == vft[1][0], "Sphere default vtable orig");

    const V3 center = RV(awkward);
    const float radius = F(awkward);
    for (int s = 0; s < 2; ++s) {
        Poison(&dc[s], sizeof dc[s]);
        Proc<void(__thiscall*)(SphereL*, const V3*, float)>(s, "??0Sphere@Jet@@QAE@ABVVector3@1@M@Z")(
            &dc[s], &center, radius);
    }
    CheckBytes(reinterpret_cast<char*>(&dc[0]) + 4, reinterpret_cast<char*>(&dc[1]) + 4, sizeof(SphereL) - 4,
               "Sphere(center,radius)");

    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(SphereL*, const SphereL*)>(s, "??0Sphere@Jet@@QAE@ABV01@@Z")(&dc[s], &dc[s ^ 1]);
    CheckBytes(reinterpret_cast<char*>(&dc[0]) + 4, reinterpret_cast<char*>(&dc[1]) + 4, sizeof(SphereL) - 4,
               "Sphere copy ctor");
    for (int s = 0; s < 2; ++s)
        Proc<SphereL*(__thiscall*)(SphereL*, const SphereL*)>(
            s, "??4Sphere@Jet@@QAEAAV01@ABV01@@Z")(&dc[s], &dc[s ^ 1]);
    CheckBytes(reinterpret_cast<char*>(&dc[0]) + 4, reinterpret_cast<char*>(&dc[1]) + 4, sizeof(SphereL) - 4,
               "Sphere::operator=");

    // Reset to known values.
    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(SphereL*, const V3*, float)>(s, "??0Sphere@Jet@@QAE@ABVVector3@1@M@Z")(
            &dc[s], &center, radius);

    CheckV3(*Proc<const V3*(__cdecl*)(const SphereL*)>(0, "?GetCenter@Sphere@Jet@@QBAABVVector3@2@XZ")(&dc[0]),
            *Proc<const V3*(__cdecl*)(const SphereL*)>(1, "?GetCenter@Sphere@Jet@@QBAABVVector3@2@XZ")(&dc[1]),
            "Sphere::GetCenter const");
    CheckV3(*Proc<V3*(__cdecl*)(SphereL*)>(0, "?GetCenter@Sphere@Jet@@QAAAAVVector3@2@XZ")(&dc[0]),
            *Proc<V3*(__cdecl*)(SphereL*)>(1, "?GetCenter@Sphere@Jet@@QAAAAVVector3@2@XZ")(&dc[1]),
            "Sphere::GetCenter");
    CheckF(Proc<float(__cdecl*)(const SphereL*)>(0, "?GetRadius@Sphere@Jet@@QBAMXZ")(&dc[0]),
           Proc<float(__cdecl*)(const SphereL*)>(1, "?GetRadius@Sphere@Jet@@QBAMXZ")(&dc[1]), "Sphere::GetRadius");

    const V3 newCenter = RV(awkward);
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(SphereL*, const V3*)>(s, "?SetCenter@Sphere@Jet@@QAAXABVVector3@2@@Z")(&dc[s], &newCenter);
    CheckBytes(reinterpret_cast<char*>(&dc[0]) + 4, reinterpret_cast<char*>(&dc[1]) + 4, sizeof(SphereL) - 4,
               "Sphere::SetCenter");
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(SphereL*, float)>(s, "?SetRadius@Sphere@Jet@@QAAXM@Z")(&dc[s], radius);
    CheckF(dc[0].r, dc[1].r, "Sphere::SetRadius");

    const V3 point = RV(awkward);
    Check(Proc<bool(__cdecl*)(const SphereL*, const V3*)>(0, "?TestCollision@Sphere@Jet@@QBA_NABVVector3@2@@Z")(
              &dc[0], &point) ==
              Proc<bool(__cdecl*)(const SphereL*, const V3*)>(1, "?TestCollision@Sphere@Jet@@QBA_NABVVector3@2@@Z")(
                  &dc[1], &point),
          "Sphere::TestCollision(Vector3)");
    const float otherRadius = F(awkward);
    Check(Proc<bool(__cdecl*)(const SphereL*, const V3*, float)>(
              0, "?TestCollision@Sphere@Jet@@QBA_NABVVector3@2@M@Z")(&dc[0], &point, otherRadius) ==
              Proc<bool(__cdecl*)(const SphereL*, const V3*, float)>(
                  1, "?TestCollision@Sphere@Jet@@QBA_NABVVector3@2@M@Z")(&dc[1], &point, otherRadius),
          "Sphere::TestCollision(Vector3,float)");

    SphereL other{};
    other.c = RV(awkward);
    other.r = F(awkward);
    Check(Proc<bool(__cdecl*)(const SphereL*, const SphereL*)>(0, "?TestCollision@Sphere@Jet@@QBA_NABV12@@Z")(
              &dc[0], &other) ==
              Proc<bool(__cdecl*)(const SphereL*, const SphereL*)>(1, "?TestCollision@Sphere@Jet@@QBA_NABV12@@Z")(
                  &dc[1], &other),
          "Sphere::TestCollision(Sphere)");
    BoxL box{};
    box.min = RV(awkward);
    box.max = RV(awkward);
    Check(Proc<bool(__cdecl*)(const SphereL*, const BoxL*)>(
              0, "?TestCollision@Sphere@Jet@@QBA_NABVAxisAlignedBox@2@@Z")(&dc[0], &box) ==
              Proc<bool(__cdecl*)(const SphereL*, const BoxL*)>(
                  1, "?TestCollision@Sphere@Jet@@QBA_NABVAxisAlignedBox@2@@Z")(&dc[1], &box),
          "Sphere::TestCollision(Box)");
    LineL line{};
    line.vt = vft[0][0];
    line.dir = RV(awkward);
    line.point = RV(awkward);
    Check(Proc<bool(__cdecl*)(const SphereL*, const LineL*)>(0, "?TestCollision@Sphere@Jet@@QBA_NABVLine@2@@Z")(
              &dc[0], &line) ==
              Proc<bool(__cdecl*)(const SphereL*, const LineL*)>(1, "?TestCollision@Sphere@Jet@@QBA_NABVLine@2@@Z")(
                  &dc[1], &line),
          "Sphere::TestCollision(Line)");
    PlaneL plane{};
    plane.n = RV(awkward);
    plane.d = F(awkward);
    Check(Proc<bool(__cdecl*)(const SphereL*, const PlaneL*)>(0, "?TestCollision@Sphere@Jet@@QBA_NABVPlane@2@@Z")(
              &dc[0], &plane) ==
              Proc<bool(__cdecl*)(const SphereL*, const PlaneL*)>(1, "?TestCollision@Sphere@Jet@@QBA_NABVPlane@2@@Z")(
                  &dc[1], &plane),
          "Sphere::TestCollision(Plane)");
    CylinderL cyl{};
    cyl.c[0] = RV(awkward);
    cyl.c[1] = RV(awkward);
    cyl.r = F(awkward);
    bool cylHit[2] = {false, false};
    for (int s = 0; s < 2; ++s) {
        cyl.vt = vft[s][2];
        cylHit[s] = Proc<bool(__cdecl*)(const SphereL*, const CylinderL*)>(
            s, "?TestCollision@Sphere@Jet@@QBA_NABVCylinder@2@@Z")(&dc[s], &cyl);
    }
    Check(cylHit[0] == cylHit[1], "Sphere::TestCollision(Cylinder)");
    EllipsoidL ell{};
    ell.c = RV(awkward);
    ell.major = F(awkward);
    ell.minor = F(awkward);
    ell.up = F(awkward);
    ell.point = RV(awkward);
    bool ellHit[2] = {false, false};
    for (int s = 0; s < 2; ++s) {
        ell.vt = vft[s][1];
        ellHit[s] = Proc<bool(__cdecl*)(const SphereL*, const EllipsoidL*)>(
            s, "?TestCollision@Sphere@Jet@@QBA_NABVEllipsoid@2@@Z")(&dc[s], &ell);
    }
    Check(ellHit[0] == ellHit[1], "Sphere::TestCollision(Ellipsoid)");

    // Maximize
    SphereL oth[2];
    for (int s = 0; s < 2; ++s) {
        Proc<void(__thiscall*)(SphereL*, const V3*, float)>(s, "??0Sphere@Jet@@QAE@ABVVector3@1@M@Z")(
            &oth[s], &other.c, other.r);
        Proc<void(__cdecl*)(SphereL*, const SphereL*)>(s, "?Maximize@Sphere@Jet@@UAAXABV12@@Z")(&dc[s], &oth[s]);
    }
    CheckBytes(reinterpret_cast<char*>(&dc[0]) + 4, reinterpret_cast<char*>(&dc[1]) + 4, sizeof(SphereL) - 4,
               "Sphere::Maximize");

    // CheckVisibility over six planes
    PlaneL frustum[6];
    for (PlaneL& p : frustum) {
        p.n = RV(awkward);
        p.d = F(awkward);
    }
    Check(Proc<bool(__cdecl*)(SphereL*, const PlaneL*)>(0, "?CheckVisibility@Sphere@Jet@@QAA_NPBVPlane@2@@Z")(
              &dc[0], frustum) ==
              Proc<bool(__cdecl*)(SphereL*, const PlaneL*)>(1, "?CheckVisibility@Sphere@Jet@@QAA_NPBVPlane@2@@Z")(
                  &dc[1], frustum),
          "Sphere::CheckVisibility");
    (void)round;
}

// ---------------------------------------------------------------------------------------------------------------
// Polygon

static void CheckPoly(const PolyL& a, const PolyL& b, const char* what)
{
    CheckBytes(&a.n, &b.n, sizeof a.n, what);
    CheckF(a.d, b.d, what);
    Check(a.num == b.num, what);
    if (a.num == b.num && a.num > 0)
        CheckBytes(a.verts, b.verts, sizeof(V3) * static_cast<size_t>(a.num), what);
}

static void FillPolyVerts(V3* verts, int n, bool awkward, int variant)
{
    for (int i = 0; i < n; ++i)
        verts[i] = RV(awkward);
    if (variant == 0)
        return;
    // duplicates / near-duplicates for RemoveDuplicates and ReduceWithTolerance
    for (int i = 1; i < n; ++i) {
        const int kind = Pick(0, 3);
        if (kind == 0)
            verts[i] = verts[i - 1];
        else if (kind == 1) {
            verts[i] = verts[i - 1];
            verts[i].x += FRange(-1e-4f, 1e-4f);
        }
    }
    if (n > 2 && (variant & 1))
        verts[n - 1] = verts[0];
}

static void TestPolygon(int round, bool awkward)
{
    // default ctor
    PolyL dc[2];
    Poison(dc, sizeof dc);
    for (int s = 0; s < 2; ++s)
        Proc<void(__thiscall*)(PolyL*)>(s, "??0Polygon@Jet@@QAE@XZ")(&dc[s]);
    Check(*reinterpret_cast<void**>(&dc[0]) == vft[0][3], "Polygon default vtable");
    Check(*reinterpret_cast<void**>(&dc[1]) == vft[1][3], "Polygon default vtable orig");
    Check(dc[0].num == dc[1].num && dc[0].num == 0, "Polygon() numVerts");
    Check(dc[0].verts == nullptr && dc[1].verts == nullptr, "Polygon() verts");

    // vertex ctor
    const int n = 3 + Pick(0, 6);
    V3 verts[12];
    FillPolyVerts(verts, n, awkward, round & 3);
    for (int s = 0; s < 2; ++s) {
        Poison(&dc[s], sizeof dc[s]);
        Proc<void(__thiscall*)(PolyL*, int, const V3*)>(s, "??0Polygon@Jet@@QAE@HPBVVector3@1@@Z")(&dc[s], n, verts);
    }
    Check(*reinterpret_cast<void**>(&dc[0]) == vft[0][3], "Polygon(verts) vtable");
    Check(*reinterpret_cast<void**>(&dc[1]) == vft[1][3], "Polygon(verts) vtable orig");
    CheckPoly(dc[0], dc[1], "Polygon(int,Vector3*)");

    // copy ctor / assignment
    PolyL cp[2];
    for (int s = 0; s < 2; ++s) {
        Poison(&cp[s], sizeof cp[s]);
        Proc<void(__thiscall*)(PolyL*, const PolyL*)>(s, "??0Polygon@Jet@@QAE@ABV01@@Z")(&cp[s], &dc[s]);
    }
    Check(*reinterpret_cast<void**>(&cp[0]) == vft[0][3], "Polygon copy vtable");
    Check(*reinterpret_cast<void**>(&cp[1]) == vft[1][3], "Polygon copy vtable orig");
    CheckPoly(cp[0], cp[1], "Polygon copy ctor");
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(PolyL*)>(s, "??1Polygon@Jet@@UAE@XZ")(&cp[s]);

    PolyL asg[2];
    for (int s = 0; s < 2; ++s) {
        Poison(&asg[s], sizeof asg[s]);
        Proc<void(__thiscall*)(PolyL*)>(s, "??0Polygon@Jet@@QAE@XZ")(&asg[s]);
        Proc<PolyL*(__cdecl*)(PolyL*, const PolyL*)>(s, "??4Polygon@Jet@@QAAAAV01@ABV01@@Z")(&asg[s], &dc[s]);
    }
    CheckPoly(asg[0], asg[1], "Polygon::operator=(Polygon)");
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(PolyL*)>(s, "??1Polygon@Jet@@UAE@XZ")(&asg[s]);

    // Polygon2D ctor and assignment
    V2 verts2[12];
    for (int i = 0; i < n; ++i) {
        verts2[i].x = verts[i].x;
        verts2[i].y = verts[i].y;
    }
    struct Poly2D {
        int num;
        const V2* verts;
    } p2{n, verts2};
    PolyL p2c[2];
    for (int s = 0; s < 2; ++s) {
        Poison(&p2c[s], sizeof p2c[s]);
        Proc<void(__thiscall*)(PolyL*, const Poly2D*)>(s, "??0Polygon@Jet@@QAE@ABVPolygon2D@1@@Z")(&p2c[s], &p2);
    }
    CheckPoly(p2c[0], p2c[1], "Polygon(Polygon2D)");
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(PolyL*)>(s, "??1Polygon@Jet@@UAE@XZ")(&p2c[s]);
    for (int s = 0; s < 2; ++s) {
        Poison(&dc[s], sizeof dc[s]);
        Proc<void(__thiscall*)(PolyL*)>(s, "??0Polygon@Jet@@QAE@XZ")(&dc[s]);
        Proc<PolyL*(__cdecl*)(PolyL*, const Poly2D*)>(s, "??4Polygon@Jet@@QAAAAV01@ABVPolygon2D@1@@Z")(&dc[s], &p2);
    }
    CheckPoly(dc[0], dc[1], "Polygon::operator=(Polygon2D)");

    // accessors / mutators
    Check(Proc<int(__cdecl*)(const PolyL*)>(0, "?GetNumVerts@Polygon@Jet@@QBAHXZ")(&dc[0]) ==
              Proc<int(__cdecl*)(const PolyL*)>(1, "?GetNumVerts@Polygon@Jet@@QBAHXZ")(&dc[1]),
          "Polygon::GetNumVerts");
    const int idx = Pick(0, n - 1);
    CheckV3(*Proc<const V3*(__cdecl*)(const PolyL*, int)>(0, "?GetVertex@Polygon@Jet@@QBAABVVector3@2@H@Z")(&dc[0], idx),
            *Proc<const V3*(__cdecl*)(const PolyL*, int)>(1, "?GetVertex@Polygon@Jet@@QBAABVVector3@2@H@Z")(&dc[1], idx),
            "Polygon::GetVertex const&");
    using VertValueFn = V3(__cdecl Obj::*)(int);
    VertValueFn vv0 = MemberProc<VertValueFn>(0, "?GetVertex@Polygon@Jet@@QAA?AVVector3@2@H@Z");
    VertValueFn vv1 = MemberProc<VertValueFn>(1, "?GetVertex@Polygon@Jet@@QAA?AVVector3@2@H@Z");
    CheckV3((reinterpret_cast<Obj*>(&dc[0])->*vv0)(idx), (reinterpret_cast<Obj*>(&dc[1])->*vv1)(idx),
            "Polygon::GetVertex value");
    V3 v3out[2] = {};
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(const PolyL*, V3*, int)>(s, "?GetVertex@Polygon@Jet@@QBAXAAVVector3@2@H@Z")(&dc[s],
                                                                                                       &v3out[s], idx);
    CheckV3(v3out[0], v3out[1], "Polygon::GetVertex(Vector3&)");
    V2 v2out[2] = {};
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(const PolyL*, V2*, int)>(s, "?GetVertex@Polygon@Jet@@QBAXAAVVector2@2@H@Z")(&dc[s],
                                                                                                       &v2out[s], idx);
    CheckBytes(&v2out[0], &v2out[1], sizeof(V2), "Polygon::GetVertex(Vector2&)");
    const V3* arr0 = Proc<const V3*(__cdecl*)(const PolyL*)>(0, "?GetVertexArray@Polygon@Jet@@QBAPBVVector3@2@XZ")(&dc[0]);
    const V3* arr1 = Proc<const V3*(__cdecl*)(const PolyL*)>(1, "?GetVertexArray@Polygon@Jet@@QBAPBVVector3@2@XZ")(&dc[1]);
    CheckBytes(arr0, arr1, sizeof(V3) * static_cast<size_t>(dc[0].num), "Polygon::GetVertexArray");

    const V3 setV = RV(awkward);
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(PolyL*, const V3*, int)>(s, "?SetVertex@Polygon@Jet@@QAAXABVVector3@2@H@Z")(&dc[s], &setV,
                                                                                                       idx);
    CheckPoly(dc[0], dc[1], "Polygon::SetVertex");

    // offsets / flip / duplicates / reduce
    const V3 off = RV(awkward);
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(PolyL*, const V3*)>(s, "?Offset@Polygon@Jet@@QAAXABVVector3@2@@Z")(&dc[s], &off);
    CheckPoly(dc[0], dc[1], "Polygon::Offset");
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(PolyL*)>(s, "?Flip@Polygon@Jet@@QAAXXZ")(&dc[s]);
    CheckPoly(dc[0], dc[1], "Polygon::Flip");

    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(PolyL*)>(s, "?RemoveDuplicates@Polygon@Jet@@QAAXXZ")(&dc[s]);
    CheckPoly(dc[0], dc[1], "Polygon::RemoveDuplicates");

    const float tolerance = awkward ? Awkward() : FRange(0.0f, 1.0f);
    CheckPoly(dc[0], dc[1], "Polygon::ReduceWithTolerance");

    if (dc[0].num >= 3 && dc[1].num >= 3) {
        for (int s = 0; s < 2; ++s)
            Proc<void(__cdecl*)(PolyL*)>(s, "?Reset@Polygon@Jet@@QAAXXZ")(&dc[s]);
        CheckPoly(dc[0], dc[1], "Polygon::Reset");
    }

    // inside tests
    const V3 probe = RV(awkward);
    const char* names[] = {"?IsInsideXY@Polygon@Jet@@QBA_NABVVector3@2@@Z",
                           "?IsInsideYZ@Polygon@Jet@@QBA_NABVVector3@2@@Z",
                           "?IsInsideZX@Polygon@Jet@@QBA_NABVVector3@2@@Z",
                           "?IsInsideXYNegative@Polygon@Jet@@QBA_NABVVector3@2@@Z",
                           "?IsInsideYZNegative@Polygon@Jet@@QBA_NABVVector3@2@@Z",
                           "?IsInsideZXNegative@Polygon@Jet@@QBA_NABVVector3@2@@Z",
                           "?IsInside@Polygon@Jet@@QBA_NABVVector3@2@@Z"};
    for (const char* nm : names)
        Check(Proc<bool(__cdecl*)(const PolyL*, const V3*)>(0, nm)(&dc[0], &probe) ==
                  Proc<bool(__cdecl*)(const PolyL*, const V3*)>(1, nm)(&dc[1], &probe),
              nm);

    float dist[2] = {Bits(0x7fc00000u), Bits(0x7fc00000u)};
    const bool in0 = Proc<bool(__cdecl*)(const PolyL*, const V3*, float*)>(
        0, "?IsInsideXY@Polygon@Jet@@QBA_NABVVector3@2@AAM@Z")(&dc[0], &probe, &dist[0]);
    const bool in1 = Proc<bool(__cdecl*)(const PolyL*, const V3*, float*)>(
        1, "?IsInsideXY@Polygon@Jet@@QBA_NABVVector3@2@AAM@Z")(&dc[1], &probe, &dist[1]);
    Check(in0 == in1, "Polygon::IsInsideXY(dist)");
    CheckF(dist[0], dist[1], "Polygon::IsInsideXY(dist out)");

    // centre and bounding sphere
    V3 centers[2] = {};
    centers[0] = RV(awkward);
    centers[1] = centers[0];
    const bool cc0 = Proc<bool(__cdecl*)(const PolyL*, V3*)>(0, "?CalculateCenter@Polygon@Jet@@QBA_NAAVVector3@2@@Z")(
        &dc[0], &centers[0]);
    const bool cc1 = Proc<bool(__cdecl*)(const PolyL*, V3*)>(1, "?CalculateCenter@Polygon@Jet@@QBA_NAAVVector3@2@@Z")(
        &dc[1], &centers[1]);
    Check(cc0 == cc1, "Polygon::CalculateCenter result");
    CheckV3(centers[0], centers[1], "Polygon::CalculateCenter out");

    SphereL spheres[2];
    Poison(spheres, sizeof spheres);
    const bool bs0 = Proc<bool(__cdecl*)(const PolyL*, SphereL*)>(
        0, "?CalculateBoundingSphere@Polygon@Jet@@QBA_NAAVSphere@2@@Z")(&dc[0], &spheres[0]);
    const bool bs1 = Proc<bool(__cdecl*)(const PolyL*, SphereL*)>(
        1, "?CalculateBoundingSphere@Polygon@Jet@@QBA_NAAVSphere@2@@Z")(&dc[1], &spheres[1]);
    Check(bs0 == bs1, "Polygon::CalculateBoundingSphere result");
    CheckBytes(reinterpret_cast<char*>(&spheres[0]) + 4, reinterpret_cast<char*>(&spheres[1]) + 4,
               sizeof(SphereL) - 4, "Polygon::CalculateBoundingSphere sphere");

    // Set(int): shrink, equal and grow
    int valid = n;
    for (int target : {1, n, n + 3, 0}) {
        const int before = dc[0].num;
        Proc<void(__cdecl*)(PolyL*, int)>(0, "?Set@Polygon@Jet@@QAAXH@Z")(&dc[0], target);
        Proc<void(__cdecl*)(PolyL*, int)>(1, "?Set@Polygon@Jet@@QAAXH@Z")(&dc[1], target);
        int common = target < before ? target : before;
        if (common > valid)
            common = valid;
        if (common > 0 && dc[0].num > 0 && dc[1].num > 0)
            CheckBytes(dc[0].verts, dc[1].verts, sizeof(V3) * static_cast<size_t>(common),
                       "Polygon::Set(int) contents");
        Check(dc[0].num == dc[1].num, "Polygon::Set(int) count");
        if (target < valid)
            valid = target;
    }

    for (int s = 0; s < 2; ++s) {
        Proc<void(__cdecl*)(PolyL*, int, const V3*)>(s, "?Set@Polygon@Jet@@QAAXHPBVVector3@2@@Z")(&dc[s], n, verts);
    }
    CheckPoly(dc[0], dc[1], "Polygon::Set(int,Vector3*)");
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(PolyL*, int, const V2*)>(s, "?Set@Polygon@Jet@@QAAXHPBVVector2@2@@Z")(&dc[s], n, verts2);
    CheckPoly(dc[0], dc[1], "Polygon::Set(int,Vector2*)");

    // destructors: the array is released and the pointer cleared
    for (int s = 0; s < 2; ++s)
        Proc<void(__cdecl*)(PolyL*)>(s, "??1Polygon@Jet@@UAE@XZ")(&dc[s]);
    Check(dc[0].verts == nullptr && dc[1].verts == nullptr, "Polygon::~Polygon");
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u1d2 <bin dir> [rounds]\n");
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
    const char* vnames[4] = {"??_7Sphere@Jet@@6B@", "??_7Ellipsoid@Jet@@6B@", "??_7Cylinder@Jet@@6B@",
                             "??_7Polygon@Jet@@6B@"};
    for (int v = 0; v < 4; ++v)
        for (int s = 0; s < 2; ++s)
            vft[s][v] = reinterpret_cast<void*>(GetProcAddress(mod[s], vnames[v]));

    for (int round = 0; round < rounds; ++round) {
        const bool awkward = (round % 7) == 0;
        TestCircle(round, awkward);
        TestCone(round, awkward);
        TestCylinder(round, awkward);
        TestEllipse(round, awkward);
        TestEllipsoid(round, awkward);
        TestSphere(round, awkward);
        TestPolygon(round, awkward);
    }

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
