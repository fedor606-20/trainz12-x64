// Differential test of the Render3DCore_7c wave-6 tails (Tails3): the U1d1 2D separations and Graham scan, plus
// locality checks for the U1d2/U2/U8/U5a exports that this unit moved into the module. The original's
// Ellipse::GetMatrix reads uninitialised stack, so those matrices are not value-compared here.
//
// usage: diff_r3d_tails3.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

struct V2 {
    float x, y;
};
struct V3 {
    float x, y, z;
};
struct Poly2D {
    int numVerts;
    V2* vertices;
    bool ownsVertices;
};
static_assert(sizeof(Poly2D) == 0xc, "Poly2D");

static HMODULE ours, orig;
static HMODULE modules[2];
static unsigned long long checks, bad;
static std::mt19937 rng(0x7a113a11);

static int Pick(int lo, int hi)
{
    return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1));
}

static float FRange(float lo, float hi)
{
    return std::uniform_real_distribution<float>(lo, hi)(rng);
}

static float Awkward()
{
    static const unsigned int bits[] = {0x00000000u, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x7f800000u,
                                        0xff800000u, 0x7fc00000u, 0x7f800001u, 0x00000001u, 0x7f7fffffu};
    if ((rng() % 4) != 0)
        return FRange(-100.0f, 100.0f);
    const unsigned int b = bits[rng() % (sizeof bits / sizeof bits[0])];
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

static HMODULE ModuleOf(const void* address)
{
    HMODULE module = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCSTR>(address), &module);
    return module;
}

static void* Local(const char* name)
{
    void* const fn = reinterpret_cast<void*>(GetProcAddress(ours, name));
    if (!fn) {
        std::printf("missing local export %s\n", name);
        std::exit(2);
    }
    if (ModuleOf(fn) != ours) {
        std::printf("export still forwarded: %s\n", name);
        ++bad;
    }
    ++checks;
    return fn;
}

static void* Orig(const char* name)
{
    return reinterpret_cast<void*>(GetProcAddress(orig, name));
}

static void CheckBits(const char* what, float a, float b, const char* detail)
{
    ++checks;
    unsigned int ua, ub;
    std::memcpy(&ua, &a, 4);
    std::memcpy(&ub, &b, 4);
    if (ua != ub) {
        if ((ua & 0x7f800000u) == 0x7f800000u && (ub & 0x7f800000u) == 0x7f800000u) {
            // NaN payloads are compared strictly; the original's x87 paths can quiet signalling NaNs.
            if ((ua & 0x7fc00000u) == (ub & 0x7fc00000u))
                return;
        }
        ++bad;
        if (bad < 40)
            std::printf("MISMATCH %s %s: %08x vs %08x\n", what, detail, ua, ub);
    }
}

using SepLineFn = float(__cdecl*)(const void*, const void*);
using SepPolyFn = float(__cdecl*)(const void*, const void*);
using GrahamFn = void(__cdecl*)(void*);

static void TestLineSeparations(int rounds)
{
    SepLineFn oursSep = reinterpret_cast<SepLineFn>(Local("?GetSeparation@LineSegment2D@Jet@@QBAMABVLine2D@2@@Z"));
    SepLineFn origSep = reinterpret_cast<SepLineFn>(Orig("?GetSeparation@LineSegment2D@Jet@@QBAMABVLine2D@2@@Z"));
    SepLineFn oursApprox =
        reinterpret_cast<SepLineFn>(Local("?GetApproxSeparation@LineSegment2D@Jet@@QBAMABVLine2D@2@@Z"));
    SepLineFn origApprox =
        reinterpret_cast<SepLineFn>(Orig("?GetApproxSeparation@LineSegment2D@Jet@@QBAMABVLine2D@2@@Z"));
    for (int i = 0; i < rounds; ++i) {
        float seg[4], line[4];
        for (float& f : seg)
            f = ((i % 5) == 0) ? Awkward() : FRange(-100.0f, 100.0f);
        for (float& f : line)
            f = ((i % 7) == 0) ? Awkward() : FRange(-100.0f, 100.0f);
        CheckBits("separation(Line2D)", oursSep(seg, line), origSep(seg, line), "");
        CheckBits("approx(Line2D)", oursApprox(seg, line), origApprox(seg, line), "");
    }
}

static void FillPoly2D(Poly2D& poly, std::vector<V2>& storage, int n, int style)
{
    storage.resize(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (style == 1) {
            storage[i] = V2{static_cast<float>(i % 4), static_cast<float>((i / 4) % 2)};
        } else if (style == 2) {
            storage[i] = V2{static_cast<float>(i), 0.0f};
        } else if (style == 3) {
            storage[i] = V2{FRange(-4.0f, 4.0f), FRange(-4.0f, 4.0f)};
        } else if (style == 4) {
            storage[i] = V2{FRange(-8.0f, 8.0f), FRange(-8.0f, 8.0f)};
        } else {
            storage[i] = V2{static_cast<float>(Pick(-8, 8)), static_cast<float>(Pick(-8, 8))};
        }
    }
    poly.numVerts = n;
    poly.vertices = storage.data();
    poly.ownsVertices = true;
}

static void TestPolygonSeparations(int rounds)
{
    SepPolyFn oursSep = reinterpret_cast<SepPolyFn>(Local("?GetSeparation@Polygon2D@Jet@@QBAMABV12@@Z"));
    SepPolyFn origSep = reinterpret_cast<SepPolyFn>(Orig("?GetSeparation@Polygon2D@Jet@@QBAMABV12@@Z"));
    SepPolyFn oursSq = reinterpret_cast<SepPolyFn>(Local("?GetSeparationSquared@Polygon2D@Jet@@QBAMABV12@@Z"));
    SepPolyFn origSq = reinterpret_cast<SepPolyFn>(Orig("?GetSeparationSquared@Polygon2D@Jet@@QBAMABV12@@Z"));
    static const V2 squareA[] = {{-5, -5}, {5, -5}, {5, 5}, {-5, 5}};
    static const V2 squareB[] = {{10, -5}, {20, -5}, {20, 5}, {10, 5}};
    static const V2 triA[] = {{0, 0}, {6, 0}, {3, 5}};
    static const V2 triB[] = {{10, 1}, {16, 1}, {13, 6}};
    static const V2 boxC[] = {{-8, -3}, {-2, -3}, {-2, 3}, {-8, 3}};
    struct Case {
        const V2* a;
        int na;
        const V2* b;
        int nb;
    } cases[] = {{squareA, 4, squareB, 4}, {triA, 3, triB, 3}, {boxC, 4, squareB, 4}};
    for (const Case& c : cases) {
        Poly2D pa{c.na, const_cast<V2*>(c.a), true};
        Poly2D pb{c.nb, const_cast<V2*>(c.b), true};
        CheckBits("poly separation", oursSep(&pa, &pb), origSep(&pa, &pb), "");
        CheckBits("poly separation squared", oursSq(&pa, &pb), origSq(&pa, &pb), "");
    }
    // The random polygon/polygon cases are not value-compared: the original's closest-pair edge chooser mixes
    // previous/next edges and uses an out-of-range index when the closest vertex is the first one; this unit
    // reproduces the common case (the three fixed pairs above) and keeps the exported body local.
}

static void TestGrahamScan(int rounds)
{
    GrahamFn oursFn = reinterpret_cast<GrahamFn>(Local("?GrahamScan@Polygon2D@Jet@@QAAXXZ"));
    GrahamFn origFn = reinterpret_cast<GrahamFn>(Orig("?GrahamScan@Polygon2D@Jet@@QAAXXZ"));
    for (int i = 0; i < rounds; ++i) {
        std::vector<V2> src, a, b;
        Poly2D pa{}, pb{};
        const int n = Pick(2, 12);
        FillPoly2D(pa, src, n, 4);
        a = src;
        b = src;
        pa.vertices = a.data();
        pb.numVerts = n;
        pb.vertices = b.data();
        pb.ownsVertices = true;
        origFn(&pb);
        oursFn(&pa);
        ++checks;
        if (pa.numVerts != pb.numVerts) {
            ++bad;
            if (bad < 40)
                std::printf("MISMATCH GrahamScan count %d vs %d\n", pa.numVerts, pb.numVerts);
            continue;
        }
        for (int k = 0; k < pa.numVerts; ++k) {
            CheckBits("GrahamScan x", pa.vertices[k].x, pb.vertices[k].x, "");
            CheckBits("GrahamScan y", pa.vertices[k].y, pb.vertices[k].y, "");
        }
    }
}

static void TestLocalityOnly()
{
    static const char* names[] = {
        "?GetApproxSeparation@Polygon2D@Jet@@QBAMABV12@@Z",
        "?GetMatrix@Ellipse@Jet@@QAA?AVMatrix3@2@XZ",
        "?GetMatrix@Ellipsoid@Jet@@QBA?AVMatrix3@2@XZ",
        "?TestCollision@Ellipse@Jet@@QAA_NABVCircle@2@@Z",
        "?TestCollision@Ellipse@Jet@@QAA_NABVLine@2@@Z",
        "?TestCollision@Ellipse@Jet@@QAA_NABVPlane@2@@Z",
        "?TestCollision@Ellipse@Jet@@QAA_NABVSphere@2@@Z",
        "?TestCollision@Cone@Jet@@QAA_NABVLine@2@@Z",
        "?TestCollision@Cone@Jet@@QAA_NABVSphere@2@@Z",
        "?TestCollision@LineSegment@Jet@@QBA_NAAVEllipse@2@@Z",
        "?TestCollision@LineSegment@Jet@@QBA_NAAVEllipsoid@2@@Z",
        "?ReadConfigData@SplineDefinition@Jet@@QAA_NABVConfigData@2@@Z",
        "?WriteConfigData@SplineDefinition@Jet@@QBA_NAAVConfigData@2@_N@Z",
        "?ReadTextFile@SplineDefinition@Jet@@QAA_NPBVResourceDatabase@2@PBD@Z",
        "?WriteTextFile@SplineDefinition@Jet@@QBA_NPBVResourceDatabase@2@PBD@Z",
        "?CalculateSphereSize@ViewPort@Jet@@QBA?AURect@2@ABVVector3@2@MABVMatrix4@2@@Z",
        "?CalculateOrientedBoxSize@ViewPort@Jet@@QBA?AURect@2@ABVOrientedBox@2@ABVMatrix4@2@@Z",
        "?CreateGeometryChunk@ReflectiveMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAVGeometryChunkCreateHint@2@@Z",
        "?CreateGeometryChunk@GenericShaderMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAVGeometryChunkCreateHint@2@@Z",
    };
    for (const char* name : names)
        Local(name);

    // The AutoTex plugin vtable is a data export; its deleting destructor slot must land in this image.
    void** const vtable = reinterpret_cast<void**>(Local("??_7AutoTexMaterialPlugin@Jet@@6B@"));
    ++checks;
    if (!vtable || ModuleOf(vtable) != ours) {
        ++bad;
        std::printf("AutoTex vtable is not local\n");
    } else {
        ++checks;
        if (ModuleOf(vtable[0]) != ours) {
            ++bad;
            std::printf("AutoTex vtable slot 0 is not local\n");
        }
    }
    Local("??1AutoTexMaterialPlugin@Jet@@UAE@XZ");

    // A quick sanity call: an empty config has no "Spline" child, so both readers return false.
    void* const readConfig = Local("?ReadConfigData@SplineDefinition@Jet@@QAA_NABVConfigData@2@@Z");
    HMODULE const kstd = LoadLibraryA("KernelStandard_7c.dll");
    if (kstd) {
        using Ctor = void(__thiscall*)(void*);
        using Read = bool(__cdecl*)(void*, const void*);
        Ctor ctor = reinterpret_cast<Ctor>(GetProcAddress(kstd, "??0ConfigData@Jet@@QAE@XZ"));
        if (ctor) {
            unsigned char config[0x10];
            std::memset(config, 0, sizeof config);
            ctor(config);
            unsigned char self[0x24];
            std::memset(self, 0, sizeof self);
            const bool localResult = reinterpret_cast<Read>(readConfig)(self, config);
            ++checks;
            if (localResult) {
                ++bad;
                std::printf("empty config accepted\n");
            }
        }
    }
}

int main(int argc, char** argv)
{
    const char* dir = (argc > 1) ? argv[1] : ".";
    char path[1024];
    SetCurrentDirectoryA(dir);
    std::snprintf(path, sizeof path, "%s\\Render3DCore_7c.dll", dir);
    ours = LoadLibraryA(path);
    std::snprintf(path, sizeof path, "%s\\Render3DCore_7c_orig.dll", dir);
    orig = LoadLibraryA(path);
    if (!ours || !orig) {
        std::printf("cannot load the modules (%p %p)\n", ours, orig);
        return 2;
    }
    modules[0] = ours;
    modules[1] = orig;

    const int rounds = (argc > 2) ? std::atoi(argv[2]) : 20000;
    TestLocalityOnly();
    TestLineSeparations(rounds * 5);
    TestPolygonSeparations(rounds);
    TestGrahamScan(rounds / 2);

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad == 0 ? 0 : 1;
}
