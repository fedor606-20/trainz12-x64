// Differential test for the U10 remainder: the index builders (box wire, sphere/ellipsoid, cone, cylinder, plane).
// The builders write raw index arrays, so the test fills a large buffer with a sentinel on both sides, calls both
// modules with the same arguments and compares every byte. The volume and vtable exports are checked for their
// local vtable slots.
//
// usage: diff_r3d_u10_rem.exe <dir with Render3DCore_7c.dll and Render3DCore_7c_orig.dll> [rounds]
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
static_assert(sizeof(Vec3) == 0xc, "Vec3");

static HMODULE mod[2];
static unsigned long long checks, bad;
static std::mt19937 rng(0x7105);

static float FRange(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }

static std::string M(const char* cls, const char* pattern)
{
    char buf[512];
    std::snprintf(buf, sizeof buf, pattern, cls);
    return buf;
}

template <class F>
static F Proc(int side, const char* name)
{
    return reinterpret_cast<F>(GetProcAddress(mod[side], name));
}

template <class F>
static F Need(int side, const char* name)
{
    F f = Proc<F>(side, name);
    if (!f) {
        std::printf("missing export %s in %s\n", name, side ? "orig" : "ours");
        std::exit(2);
    }
    return f;
}

static void Check(bool ok, const char* what, int a, int b, int c)
{
    ++checks;
    if (!ok) {
        ++bad;
        std::printf("MISMATCH %s args %d %d %d\n", what, a, b, c);
    }
}

using Fn2 = void(__cdecl*)(void*, int, int, unsigned short*, int);
using Fn3 = void(__cdecl*)(void*, int, int, int, unsigned short*, int);
using Fn4 = void(__cdecl*)(void*, int, int, int, int, unsigned short*, int);

static alignas(16) unsigned char buf[2][0x8000];

static void Run2(const char* name, int a, int b)
{
    Fn2 f[2] = {Need<Fn2>(0, name), Need<Fn2>(1, name)};
    for (int side = 0; side < 2; ++side) {
        std::memset(buf[side], 0xcd, sizeof buf[side]);
        f[side](nullptr, a, b, reinterpret_cast<unsigned short*>(buf[side]), 0);
    }
    Check(std::memcmp(buf[0], buf[1], sizeof buf[0]) == 0, name, a, b, 0);
}

static void Run3(const char* name, int a, int b, int c)
{
    Fn3 f[2] = {Need<Fn3>(0, name), Need<Fn3>(1, name)};
    for (int side = 0; side < 2; ++side) {
        std::memset(buf[side], 0xcd, sizeof buf[side]);
        f[side](nullptr, a, b, c, reinterpret_cast<unsigned short*>(buf[side]), 0);
    }
    Check(std::memcmp(buf[0], buf[1], sizeof buf[0]) == 0, name, a, b, c);
}

static void Run4(const char* name, int a, int b, int c, int d)
{
    Fn4 f[2] = {Need<Fn4>(0, name), Need<Fn4>(1, name)};
    for (int side = 0; side < 2; ++side) {
        std::memset(buf[side], 0xcd, sizeof buf[side]);
        f[side](nullptr, a, b, c, d, reinterpret_cast<unsigned short*>(buf[side]), 0);
    }
    Check(std::memcmp(buf[0], buf[1], sizeof buf[0]) == 0, name, a, b, c);
}

static void RunIndices(int round)
{
    for (int z = 1; z <= 5; ++z)
        for (int y = 1; y <= 5; ++y)
            for (int x = 1; x <= 5; ++x)
                Run4("?BuildIndicesWire@RenderBox@Jet@@IAAXHHHHPAGH@Z", 0, x, y, z);

    for (int a = 1; a <= 8; ++a) {
        for (int b : {1, 2, 3, 5, 9, 17, 33, 40}) {
            Run2("?BuildIndicesSolid@RenderSphere@Jet@@IAAXHHPAGH@Z", a, b);
            Run2("?BuildIndicesWire@RenderSphere@Jet@@IAAXHHPAGH@Z", a, b);
            Run2("?BuildIndicesSolid@RenderEllipsoid@Jet@@IAAXHHPAGH@Z", a, b);
            Run2("?BuildIndicesWire@RenderEllipsoid@Jet@@IAAXHHPAGH@Z", a, b);
        }
    }

    for (int a = 1; a <= 8; ++a) {
        for (int b : {2, 3, 5, 9, 17, 33}) {
            Run2("?BuildIndicesSolid@RenderCone@Jet@@IAAXHHPAGH@Z", a, b);
            Run2("?BuildIndicesWire@RenderCone@Jet@@IAAXHHPAGH@Z", a, b);
        }
    }

    for (int a = 1; a <= 8; ++a) {
        for (int b : {2, 3, 5, 9, 17, 33}) {
            Run2("?BuildIndicesSolid@RenderCylinder@Jet@@IAAXHHPAGH@Z", a, b);
            Run2("?BuildIndicesWire@RenderCylinder@Jet@@IAAXHHPAGH@Z", a, b);
        }
    }

    for (int y = 0; y <= 6; ++y)
        for (int x = 0; x <= 6; ++x) {
            Run3("?BuildIndicesSolid@RenderPlane@Jet@@IAAXHHHPAGH@Z", x, y, 0);
            Run3("?BuildIndicesWire@RenderPlane@Jet@@IAAXHHHPAGH@Z", x, y, 0);
        }
}

static void CheckSlot(const char* cls, const char* vtName)
{
    (void)cls;
    void* vt[2] = {Proc<void*>(0, vtName), Proc<void*>(1, vtName)};
    if (!vt[0] || !vt[1]) return;
    for (int side = 0; side < 2; ++side) {
        void** table = reinterpret_cast<void**>(vt[side]);
        Check(table[0xe4 / 4] != nullptr, "CalculatePrimitiveVolume slot", side, 0, 0);
    }
}

// ---- ray tests --------------------------------------------------------------------------------------------------

struct RayF {
    void* vt;
    Vec3 dir;
    Vec3 point;
};

struct ShapeDef {
    const char* cls;
    const char* name;
    unsigned size;
    unsigned fieldOff;
    unsigned fieldCount;  // after the common scale at 0x164
};

static Vec3 RVec()
{
    return Vec3{FRange(-10.0f, 10.0f), FRange(-10.0f, 10.0f), FRange(-10.0f, 10.0f)};
}

static void RunRay(const ShapeDef& sd, const RayF& ray, int flags)
{
    using Ctor = void(__thiscall*)(void*);
    using Fn = bool(__cdecl*)(const void*, const RayF*, Vec3*);
    Ctor ctor[2] = {Need<Ctor>(0, M(sd.cls, "??0%s@Jet@@QAE@XZ").c_str()),
                    Need<Ctor>(1, M(sd.cls, "??0%s@Jet@@QAE@XZ").c_str())};
    Fn test[2] = {Need<Fn>(0, sd.name), Need<Fn>(1, sd.name)};
    alignas(16) unsigned char obj[2][0x200];
    Vec3 out[2] = {{9, 9, 9}, {9, 9, 9}};
    const float scale[3] = {FRange(0.1f, 4.0f), FRange(0.1f, 4.0f), FRange(0.1f, 4.0f)};
    float fields[4];
    for (unsigned i = 0; i < 4; ++i) fields[i] = FRange(0.1f, 5.0f);
    for (int side = 0; side < 2; ++side) {
        std::memset(obj[side], 0, sizeof obj[side]);
        ctor[side](obj[side]);
        for (int i = 0; i < 3; ++i) std::memcpy(obj[side] + 0x164 + i * 4, &scale[i], 4);
        for (unsigned i = 0; i < sd.fieldCount; ++i) {
            const float v = fields[i];
            std::memcpy(obj[side] + sd.fieldOff + i * 4, &v, 4);
        }
        std::memcpy(obj[side] + 0x180, &flags, 4);
    }
    const bool a = test[0](obj[0], &ray, &out[0]);
    const bool b = test[1](obj[1], &ray, &out[1]);
    ++checks;
    if (a != b || std::memcmp(&out[0], &out[1], sizeof(Vec3)) != 0) {
        ++bad;
        std::printf("MISMATCH %s hit %d/%d out %.9g %.9g %.9g / %.9g %.9g %.9g\n", sd.name, a, b, out[0].x,
                    out[0].y, out[0].z, out[1].x, out[1].y, out[1].z);
        std::printf("  scale %.9g %.9g %.9g fields %.9g %.9g %.9g ray p %.9g %.9g %.9g d %.9g %.9g %.9g flags %d\n",
                    *(float*)(obj[1] + 0x164), *(float*)(obj[1] + 0x168), *(float*)(obj[1] + 0x16c),
                    sd.fieldCount > 0 ? *(float*)(obj[1] + sd.fieldOff) : 0.0f,
                    sd.fieldCount > 1 ? *(float*)(obj[1] + sd.fieldOff + 4) : 0.0f,
                    sd.fieldCount > 2 ? *(float*)(obj[1] + sd.fieldOff + 8) : 0.0f, ray.point.x, ray.point.y,
                    ray.point.z, ray.dir.x, ray.dir.y, ray.dir.z, flags);
    }
}

static void RunRays(int round)
{
    const ShapeDef defs[] = {
        {"RenderBox", "?GeometryTestCollision@RenderBox@Jet@@MBA_NABVRay@2@AAVVector3@2@@Z", 0x194, 0x188, 3},
        {"RenderSphere", "?GeometryTestCollision@RenderSphere@Jet@@MBA_NABVRay@2@AAVVector3@2@@Z", 0x18c, 0x188, 1},
        {"RenderEllipsoid", "?GeometryTestCollision@RenderEllipsoid@Jet@@MBA_NABVRay@2@AAVVector3@2@@Z", 0x194, 0x188, 3},
    };
    for (const ShapeDef& sd : defs) {
        for (int i = 0; i < 6; ++i) {
            RayF ray{};
            ray.dir = RVec();
            ray.point = RVec();
            RunRay(sd, ray, (round & 1) ? 4 : 0);
        }
        // axis-aligned rays through the origin, where the slab/quadratic edge cases live
        for (int axis = 0; axis < 3; ++axis) {
            for (int k = 0; k < 4; ++k) {
                RayF ray{};
                ray.point = Vec3{axis == 0 ? -8.0f : 0.0f, axis == 1 ? -8.0f : 0.0f, axis == 2 ? -8.0f : 0.0f};
                ray.dir.x = axis == 0 ? 1.0f : 0.0f;
                ray.dir.y = axis == 1 ? 1.0f : 0.0f;
                ray.dir.z = axis == 2 ? 1.0f : 0.0f;
                if (k & 1) ray.point = Vec3{0.0f, 0.0f, 0.0f};
                if (k & 2) {
                    ray.dir.x = -ray.dir.x;
                    ray.dir.y = -ray.dir.y;
                    ray.dir.z = -ray.dir.z;
                }
                RunRay(sd, ray, (round & 1) ? 4 : 0);
            }
        }
    }
}


int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u10_rem <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 1;
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
        RunIndices(round);
        RunRays(round);
        CheckSlot("RenderBox", "??_7RenderBox@Jet@@6B@");
        CheckSlot("RenderSphere", "??_7RenderSphere@Jet@@6B@");
        CheckSlot("RenderCone", "??_7RenderCone@Jet@@6B@");
        CheckSlot("RenderCylinder", "??_7RenderCylinder@Jet@@6B@");
        CheckSlot("RenderEllipsoid", "??_7RenderEllipsoid@Jet@@6B@");
        CheckSlot("RenderPlane", "??_7RenderPlane@Jet@@6B@");
        CheckSlot("RenderTube", "??_7RenderTube@Jet@@6B@");
    }

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
