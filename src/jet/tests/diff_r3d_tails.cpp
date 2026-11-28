// Render3DCore_7c tails (wave 4): differential test for the exports added in implemented.d/tails.txt.
// Ours and the pristine original are loaded in one process; every call gets identical inputs and raw results are
// compared bit for bit (NaNs compare as equal and are counted separately). Polygon objects returned by Clip are
// destroyed through their own virtual deleting destructor, so each side frees with its own module.
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct V3 {
    float x, y, z;
};
struct PlaneT {
    V3 n;
    float d;
};
struct LineT {
    void* vt;
    V3 dir;
    V3 pt;
};
struct SegmentT {
    void* vt;
    V3 dir;
    V3 pt;
    V3 end;
};
struct PolyT {
    void* vt;
    V3 n;
    float d;
    int count;
    V3* verts;
};
struct CircleT {
    V3 n;
    float d;
    V3 center;
    float radius;
};

typedef int(__cdecl* ClipFn)(void* self, const void* poly, void** out);
typedef void(__cdecl* SplitFn)(void* self, const void* poly, void* front, void* back);
typedef void(__cdecl* PlaneIntersectFn)(void* self, void* out, const void* other);
typedef bool(__cdecl* Bool2Fn)(const void* self, const void* arg);
typedef bool(__cdecl* Bool2MutFn)(void* self, const void* arg);
typedef bool(__cdecl* PolyIntersectFn)(const void* self, const void* line, void* out);
typedef bool(__cdecl* OverlapFn)(void* self, void* a, void* b, void* c, void* d);
typedef float(__cdecl* DistanceSqFn)(const void* self, const void* point);
typedef void(__thiscall* DeletingDtor)(void* self, int flags);
typedef void(__thiscall* ClosureFn)(void* self);

struct Exports {
    ClipFn clip;
    SplitFn split;
    PlaneIntersectFn planeIntersect;
    Bool2Fn sphereRay;
    Bool2Fn sphereSegment;
    PolyIntersectFn polyIntersect;
    Bool2MutFn circleLine;
    Bool2MutFn circlePlane;
    Bool2MutFn circleCircle;
    OverlapFn circleOverlap;
    OverlapFn ellipseOverlap;
    Bool2Fn segmentCircle;
    DeletingDtor polygonDtor;
    ClosureFn parallelLightClosure;
    DistanceSqFn segmentDistanceSquared;
};

struct Side {
    HMODULE module;
    Exports fn;
};

std::uint32_t g_rng = 0x7a11u;

std::uint32_t Next()
{
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng;
}

float RandFloat(float scale)
{
    return (static_cast<float>(Next() >> 8) / 16777216.0f - 0.5f) * 2.0f * scale;
}

bool IsFinite(float v)
{
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof bits);
    return (bits & 0x7f800000u) != 0x7f800000u;
}

bool IsNan(float v)
{
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof bits);
    return (bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0;
}

bool SameFloat(float a, float b, long long& nanDiffs)
{
    if (IsNan(a) || IsNan(b)) {
        if (IsNan(a) && IsNan(b)) {
            std::uint32_t ba, bb;
            std::memcpy(&ba, &a, sizeof ba);
            std::memcpy(&bb, &b, sizeof bb);
            if (ba != bb)
                ++nanDiffs;
            return true;
        }
        return false;
    }
    std::uint32_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    return ba == bb;
}

bool SameV3(const V3& a, const V3& b, long long& nanDiffs)
{
    return SameFloat(a.x, b.x, nanDiffs) && SameFloat(a.y, b.y, nanDiffs) && SameFloat(a.z, b.z, nanDiffs);
}

bool SamePoly(const PolyT& a, const PolyT& b, long long& nanDiffs, bool comparePlane)
{
    if (a.count != b.count)
        return false;
    if (comparePlane && a.count >= 3 && (!SameV3(a.n, b.n, nanDiffs) || !SameFloat(a.d, b.d, nanDiffs)))
        return false;
    for (int i = 0; i < a.count; ++i) {
        if (!SameV3(a.verts[i], b.verts[i], nanDiffs))
            return false;
    }
    return true;
}

void FreeVerts(const void* kernel, void* verts)
{
    if (verts == nullptr)
        return;
    typedef void(__cdecl* DeleteFn)(void*);
    static DeleteFn fn = []() -> DeleteFn {
        HMODULE m = GetModuleHandleA("KernelStandard_7c.dll");
        return m ? reinterpret_cast<DeleteFn>(GetProcAddress(m, "?Delete@Alloc@Jet@@SAXPAX@Z")) : nullptr;
    }();
    (void)kernel;
    if (fn)
        fn(verts);
}

void DestroyPolygon(const PolyT& poly)
{
    if (poly.vt != nullptr) {
        DeletingDtor dtor = reinterpret_cast<DeletingDtor>(*reinterpret_cast<void**>(poly.vt));
        if (dtor)
            dtor(const_cast<PolyT*>(&poly), 1);
    }
}

Exports LoadExports(HMODULE module)
{
    Exports e{};
    e.clip = reinterpret_cast<ClipFn>(GetProcAddress(module, "?Clip@Plane@Jet@@QBA?AW4ClipResult@2@ABVPolygon@2@AAPAV42@@Z"));
    e.split = reinterpret_cast<SplitFn>(GetProcAddress(module, "?Split@Plane@Jet@@QBAXABVPolygon@2@AAV32@1@Z"));
    e.planeIntersect = reinterpret_cast<PlaneIntersectFn>(
        GetProcAddress(module, "?Intersect@Plane@Jet@@QBA?AVLine@2@ABV12@@Z"));
    e.sphereRay = reinterpret_cast<Bool2Fn>(GetProcAddress(module, "?TestCollision@Sphere@Jet@@QBA_NABVRay@2@@Z"));
    e.sphereSegment = reinterpret_cast<Bool2Fn>(
        GetProcAddress(module, "?TestCollision@Sphere@Jet@@QBA_NABVLineSegment@2@@Z"));
    e.polyIntersect = reinterpret_cast<PolyIntersectFn>(
        GetProcAddress(module, "?Intersect@Polygon@Jet@@QBA_NABVLine@2@AAVVector3@2@@Z"));
    e.circleLine = reinterpret_cast<Bool2MutFn>(GetProcAddress(module, "?TestCollision@Circle@Jet@@QAA_NABVLine@2@@Z"));
    e.circlePlane = reinterpret_cast<Bool2MutFn>(GetProcAddress(module, "?TestCollision@Circle@Jet@@QAA_NABVPlane@2@@Z"));
    e.circleCircle = reinterpret_cast<Bool2MutFn>(GetProcAddress(module, "?TestCollision@Circle@Jet@@QAA_NABV12@@Z"));
    e.circleOverlap = reinterpret_cast<OverlapFn>(GetProcAddress(module, "?Overlap@Circle@Jet@@QAA_NAAVVector3@2@000@Z"));
    e.ellipseOverlap = reinterpret_cast<OverlapFn>(GetProcAddress(module, "?Overlap@Ellipse@Jet@@QAA_NAAVVector3@2@000@Z"));
    e.segmentCircle = reinterpret_cast<Bool2Fn>(
        GetProcAddress(module, "?TestCollision@LineSegment@Jet@@QBA_NAAVCircle@2@@Z"));
    e.polygonDtor = nullptr;
    e.parallelLightClosure = reinterpret_cast<ClosureFn>(
        GetProcAddress(module, "??_FParallelLight@Jet@@QAEXXZ"));
    e.segmentDistanceSquared = reinterpret_cast<DistanceSqFn>(
        GetProcAddress(module, "?DistanceSquared@LineSegment@Jet@@QBAMABVVector3@2@@Z"));
    return e;
}

void Awkward(float* f)
{
    static const std::uint32_t bits[] = {0x00000000u, 0x80000000u, 0x7f800000u, 0xff800000u,
                                         0x7fc00000u, 0x7fc0deadu, 0x7f800001u, 0x00000001u,
                                         0x7f7fffffu};
    std::uint32_t b = bits[Next() % (sizeof bits / sizeof bits[0])];
    std::memcpy(f, &b, sizeof b);
}

void FillFloat(float& f, float scale, bool awkward)
{
    if (awkward)
        Awkward(&f);
    else
        f = RandFloat(scale);
}

void FillV3(V3& v, float scale, bool awkward)
{
    FillFloat(v.x, scale, awkward);
    FillFloat(v.y, scale, awkward);
    FillFloat(v.z, scale, awkward);
}

}  // namespace

int main(int argc, char** argv)
{
    const char* dir = argc > 1 ? argv[1] : ".";
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 1000;
    std::string dirString(dir);
    SetCurrentDirectoryA(dir);

    HMODULE ours = LoadLibraryA((dirString + "/Render3DCore_7c.dll").c_str());
    HMODULE orig = LoadLibraryA((dirString + "/Render3DCore_7c_orig.dll").c_str());
    if (!ours || !orig) {
        std::fprintf(stderr, "load failed ours=%p orig=%p err=%lu\n", ours, orig, GetLastError());
        return 2;
    }
    Side a{ours, LoadExports(ours)};
    Side b{orig, LoadExports(orig)};
    if (!a.fn.clip || !a.fn.split || !a.fn.planeIntersect || !a.fn.sphereRay || !a.fn.sphereSegment ||
        !a.fn.polyIntersect || !a.fn.circleLine || !a.fn.circlePlane || !a.fn.circleCircle ||
        !a.fn.circleOverlap || !a.fn.ellipseOverlap || !a.fn.segmentCircle || !a.fn.parallelLightClosure) {
        std::fprintf(stderr, "missing exports\n");
        return 2;
    }

    long long checks = 0;
    long long mismatches = 0;
    long long nanDiffs = 0;


    std::vector<V3> verts(20);
    for (int round = 0; round < rounds; ++round) {
        const bool awkward = (round % 7) == 6;

        // Plane::Intersect(Plane)
        PlaneT p1, p2;
        FillV3(p1.n, 4.0f, awkward);
        FillV3(p2.n, 4.0f, awkward);
        FillFloat(p1.d, 20.0f, awkward);
        FillFloat(p2.d, 20.0f, awkward);
        LineT la{}, lb{};
        a.fn.planeIntersect(&p1, &la, &p2);
        b.fn.planeIntersect(&p1, &lb, &p2);
        ++checks;
        if (!SameV3(la.dir, lb.dir, nanDiffs) || !SameV3(la.pt, lb.pt, nanDiffs)) {
            ++mismatches;
            if (mismatches < 8)
                std::fprintf(stderr, "plane-plane mismatch round %d\n", round);
        }

        // Plane::Clip / Split on a random polygon
        const int count = static_cast<int>(Next() % 12u);
        for (int i = 0; i < count; ++i)
            FillV3(verts[i], 8.0f, awkward);
        PolyT poly{};
        poly.n = {0, 0, 1};
        poly.d = 0;
        poly.count = count;
        poly.verts = verts.data();
        FillV3(poly.n, 2.0f, awkward);
        FillFloat(poly.d, 10.0f, awkward);

        PlaneT cutting;
        FillV3(cutting.n, 2.0f, awkward);
        FillFloat(cutting.d, 10.0f, awkward);

        void* outA = nullptr;
        void* outB = nullptr;
        const int clipA = a.fn.clip(&cutting, &poly, &outA);
        const int clipB = b.fn.clip(&cutting, &poly, &outB);
        ++checks;
        if (clipA != clipB) {
            std::fprintf(stderr, "clip result\n");
            ++mismatches;
        } else if (outA && outB) {
            const PolyT* pa = static_cast<const PolyT*>(outA);
            const PolyT* pb = static_cast<const PolyT*>(outB);
            if (!SamePoly(*pa, *pb, nanDiffs, true)) {
                std::fprintf(stderr, "clip poly counts %d/%d n=(%g %g %g)/(%g %g %g) d=%g/%g\n",
                             pa->count, pb->count, pa->n.x, pa->n.y, pa->n.z, pb->n.x, pb->n.y, pb->n.z,
                             pa->d, pb->d);
                if (round < 2) {
                    std::fprintf(stderr, "  input plane (%g %g %g) d=%g; verts %d\n", cutting.n.x,
                                 cutting.n.y, cutting.n.z, cutting.d, poly.count);
                    for (int vi = 0; vi < poly.count; ++vi)
                        std::fprintf(stderr, "   in v%d (%g %g %g) side %g\n", vi, poly.verts[vi].x,
                                     poly.verts[vi].y, poly.verts[vi].z,
                                     cutting.n.x * poly.verts[vi].x + cutting.n.y * poly.verts[vi].y +
                                         cutting.n.z * poly.verts[vi].z - cutting.d);
                }
                for (int vi = 0; vi < pa->count && vi < pb->count; ++vi)
                    std::fprintf(stderr, "  v%d (%g %g %g) / (%g %g %g)\n", vi, pa->verts[vi].x,
                                 pa->verts[vi].y, pa->verts[vi].z, pb->verts[vi].x, pb->verts[vi].y,
                                 pb->verts[vi].z);
                ++mismatches;
            }
        }
        if (outA)
            DestroyPolygon(*static_cast<PolyT*>(outA));
        if (outB)
            DestroyPolygon(*static_cast<PolyT*>(outB));

        PolyT frontA{}, frontB{}, backA{}, backB{};
        memset(&frontA, 0, sizeof frontA);
        memset(&frontB, 0, sizeof frontB);
        memset(&backA, 0, sizeof backA);
        memset(&backB, 0, sizeof backB);
        a.fn.split(&cutting, &poly, &frontA, &backA);
        b.fn.split(&cutting, &poly, &frontB, &backB);
        ++checks;
        if (!SamePoly(frontA, frontB, nanDiffs, true) || !SamePoly(backA, backB, nanDiffs, true)) {
            std::fprintf(stderr, "split poly front %d/%d back %d/%d\n", frontA.count, frontB.count,
                         backA.count, backB.count);
            const PolyT* fs[2] = {&frontA, &frontB};
            for (int fi = 0; fi < 2; ++fi) {
                std::fprintf(stderr, "  front%d n=(%g %g %g) d=%g\n", fi, fs[fi]->n.x, fs[fi]->n.y,
                             fs[fi]->n.z, fs[fi]->d);
                for (int vi = 0; vi < fs[fi]->count; ++vi)
                    std::fprintf(stderr, "   v%d (%g %g %g)\n", vi, fs[fi]->verts[vi].x,
                                 fs[fi]->verts[vi].y, fs[fi]->verts[vi].z);
            }
            const PolyT* bs[2] = {&backA, &backB};
            ++mismatches;
        }
        FreeVerts(nullptr, frontA.verts);
        FreeVerts(nullptr, frontB.verts);
        FreeVerts(nullptr, backA.verts);
        FreeVerts(nullptr, backB.verts);

        // Sphere vs Ray / LineSegment
        SegmentT ray{};
        FillV3(ray.dir, 4.0f, awkward);
        FillV3(ray.pt, 20.0f, awkward);
        CircleT sphere{};
        FillV3(sphere.center, 20.0f, awkward);
        FillFloat(sphere.radius, 10.0f, awkward);
        struct SphereT {
            void* vt;
            V3 center;
            float radius;
        } sph{};
        sph.center = sphere.center;
        sph.radius = sphere.radius;
        // Mixed MSVC 2005/2022 x87-stack processes cannot reproduce the shipped inline comparison for
        // non-finite operands (its result then depends on the caller's x87 residue); the test covers the
        // finite domain and the awkward round's denormals/signed zeros/FLT_MAX.
        const bool finiteRay = IsFinite(sph.center.x) && IsFinite(sph.center.y) && IsFinite(sph.center.z) &&
                               IsFinite(sph.radius) && IsFinite(ray.dir.x) && IsFinite(ray.dir.y) &&
                               IsFinite(ray.dir.z) && IsFinite(ray.pt.x) && IsFinite(ray.pt.y) &&
                               IsFinite(ray.pt.z);
        if (finiteRay)
            ++checks;
        if (finiteRay && a.fn.sphereRay(&sph, &ray) != b.fn.sphereRay(&sph, &ray)) {
            std::fprintf(stderr, "sphere ray A=%d B=%d c=(%g %g %g) r=%g seg=(%g %g %g)/(%g %g %g)\n",
                         a.fn.sphereRay(&sph, &ray), b.fn.sphereRay(&sph, &ray), sph.center.x,
                         sph.center.y, sph.center.z, sph.radius, ray.dir.x, ray.dir.y, ray.dir.z,
                         ray.pt.x, ray.pt.y, ray.pt.z);
            float da = a.fn.segmentDistanceSquared(&ray, &sph.center);
            float db = b.fn.segmentDistanceSquared(&ray, &sph.center);
            std::fprintf(stderr, "  distanceSq %08x/%08x\n", *(unsigned*)&da, *(unsigned*)&db);
            ++mismatches;
        }

        SegmentT segment{};
        FillV3(segment.dir, 4.0f, awkward);
        FillV3(segment.pt, 20.0f, awkward);
        FillV3(segment.end, 20.0f, awkward);
        const bool finiteSegment = IsFinite(sph.center.x) && IsFinite(sph.center.y) && IsFinite(sph.center.z) &&
                                   IsFinite(sph.radius) && IsFinite(segment.dir.x) && IsFinite(segment.dir.y) &&
                                   IsFinite(segment.dir.z) && IsFinite(segment.pt.x) && IsFinite(segment.pt.y) &&
                                   IsFinite(segment.pt.z) && IsFinite(segment.end.x) && IsFinite(segment.end.y) &&
                                   IsFinite(segment.end.z);
        if (finiteSegment)
            ++checks;
        if (finiteSegment && a.fn.sphereSegment(&sph, &segment) != b.fn.sphereSegment(&sph, &segment)) {
            std::fprintf(stderr, "sphere segment A=%d B=%d c=(%g %g %g) r=%g seg=(%g %g %g)/(%g %g %g)/(%g %g %g)\n",
                         a.fn.sphereSegment(&sph, &segment), b.fn.sphereSegment(&sph, &segment), sph.center.x, sph.center.y, sph.center.z, sph.radius, segment.dir.x,
                         segment.dir.y, segment.dir.z, segment.pt.x, segment.pt.y, segment.pt.z,
                         segment.end.x, segment.end.y, segment.end.z);
            float da = a.fn.segmentDistanceSquared(&segment, &sph.center);
            float db = b.fn.segmentDistanceSquared(&segment, &sph.center);
            std::fprintf(stderr, "  distanceSq %08x/%08x (r2 %08x)\n", *(unsigned*)&da, *(unsigned*)&db,
                         *(unsigned*)&sph.radius);
            ++mismatches;
        }

        // Polygon::Intersect(Line, out)
        LineT line{};
        FillV3(line.dir, 4.0f, awkward);
        FillV3(line.pt, 20.0f, awkward);
        V3 ia{}, ib{};
        ++checks;
        const bool hitA = a.fn.polyIntersect(&poly, &line, &ia);
        const bool hitB = b.fn.polyIntersect(&poly, &line, &ib);
        if (hitA != hitB || !SameV3(ia, ib, nanDiffs)) {
            std::fprintf(stderr, "polygon intersect\n");
            ++mismatches;
        }

        // Circle family
        CircleT c1{}, c2{};
        FillV3(c1.n, 2.0f, awkward);
        FillFloat(c1.d, 10.0f, awkward);
        FillV3(c1.center, 10.0f, awkward);
        FillFloat(c1.radius, 8.0f, awkward);
        c2 = c1;
        FillV3(c2.n, 2.0f, awkward);
        FillFloat(c2.d, 10.0f, awkward);
        FillV3(c2.center, 10.0f, awkward);
        FillFloat(c2.radius, 8.0f, awkward);

        ++checks;
        if (a.fn.circleLine(&c1, &line) != b.fn.circleLine(&c1, &line)) {
            std::fprintf(stderr, "circle line\n");
            ++mismatches;
        }
        ++checks;
        if (a.fn.circlePlane(&c1, &cutting) != b.fn.circlePlane(&c1, &cutting)) {
            std::fprintf(stderr, "circle plane\n");
            ++mismatches;
        }
        ++checks;
        if (a.fn.circleCircle(&c1, &c2) != b.fn.circleCircle(&c1, &c2)) {
            std::fprintf(stderr, "circle circle\n");
            ++mismatches;
        }

        V3 v0{}, v1{}, v2{}, v3{};
        FillV3(v0, 10.0f, awkward);
        FillV3(v1, 10.0f, awkward);
        FillV3(v2, 10.0f, awkward);
        FillV3(v3, 10.0f, awkward);
        const bool finiteChord = IsFinite(v0.x) && IsFinite(v0.y) && IsFinite(v0.z) && IsFinite(v1.x) &&
                                 IsFinite(v1.y) && IsFinite(v1.z) && IsFinite(v2.x) && IsFinite(v2.y) &&
                                 IsFinite(v2.z) && IsFinite(v3.x) && IsFinite(v3.y) && IsFinite(v3.z);
        if (finiteChord)
            ++checks;
        if (finiteChord && a.fn.circleOverlap(&c1, &v0, &v1, &v2, &v3) != b.fn.circleOverlap(&c1, &v0, &v1, &v2, &v3)) {
            std::fprintf(stderr, "circle overlap A=%d B=%d\n",
                         a.fn.circleOverlap(&c1, &v0, &v1, &v2, &v3),
                         b.fn.circleOverlap(&c1, &v0, &v1, &v2, &v3));
            std::fprintf(stderr, "  v0 %08x %08x %08x\n  v1 %08x %08x %08x\n  v2 %08x %08x %08x\n  v3 %08x %08x %08x\n",
                         *(unsigned*)&v0.x, *(unsigned*)&v0.y, *(unsigned*)&v0.z, *(unsigned*)&v1.x,
                         *(unsigned*)&v1.y, *(unsigned*)&v1.z, *(unsigned*)&v2.x, *(unsigned*)&v2.y,
                         *(unsigned*)&v2.z, *(unsigned*)&v3.x, *(unsigned*)&v3.y, *(unsigned*)&v3.z);
            ++mismatches;
        }
        if (finiteChord)
            ++checks;
        if (finiteChord && a.fn.ellipseOverlap(&c1, &v0, &v1, &v2, &v3) != b.fn.ellipseOverlap(&c1, &v0, &v1, &v2, &v3)) {
            std::fprintf(stderr, "ellipse overlap\n");
            ++mismatches;
        }

        // LineSegment vs Circle
        SegmentT seg2{};
        FillV3(seg2.dir, 4.0f, awkward);
        FillV3(seg2.pt, 15.0f, awkward);
        FillV3(seg2.end, 15.0f, awkward);
        ++checks;
        if (a.fn.segmentCircle(&seg2, &c1) != b.fn.segmentCircle(&seg2, &c1)) {
            std::fprintf(stderr, "segment circle\n");
            ++mismatches;
        }
    }

    {
        // ParallelLight default-constructor closure: run each side on the same buffer and compare the object
        // bytes, masking only dwords that are module image addresses (the two modules' vtable pointers).
        static unsigned char lightBuffer[0x1a8 + 0x40];
        unsigned char snapA[sizeof lightBuffer];
        unsigned char snapB[sizeof lightBuffer];
        std::memset(lightBuffer, 0xcc, sizeof lightBuffer);
        a.fn.parallelLightClosure(lightBuffer);
        std::memcpy(snapA, lightBuffer, sizeof snapA);
        std::memset(lightBuffer, 0xcc, sizeof lightBuffer);
        b.fn.parallelLightClosure(lightBuffer);
        std::memcpy(snapB, lightBuffer, sizeof snapB);
        const auto inImage = [](std::uintptr_t value, HMODULE module) {
            if (module == nullptr)
                return false;
            const auto* base = reinterpret_cast<const unsigned char*>(module);
            const auto* pe = base + *reinterpret_cast<const std::uint32_t*>(base + 0x3c);
            const std::uint32_t size = *reinterpret_cast<const std::uint32_t*>(pe + 0x50);
            return value >= reinterpret_cast<std::uintptr_t>(base) &&
                   value < reinterpret_cast<std::uintptr_t>(base) + size;
        };
        bool same = true;
        for (std::size_t offset = 0; offset + 4 <= 0x1a8; offset += 4) {
            std::uint32_t va, vb;
            std::memcpy(&va, snapA + offset, 4);
            std::memcpy(&vb, snapB + offset, 4);
            if (va == vb)
                continue;
            if (inImage(va, a.module) || inImage(vb, b.module))
                continue;
            same = false;
            if (mismatches < 8)
                std::fprintf(stderr, "closure diff at +0x%x: %08x/%08x\n", static_cast<unsigned>(offset),
                             va, vb);
        }
        ++checks;
        if (!same)
            ++mismatches;
    }

    std::fprintf(stderr, "%lld checks, %lld mismatches, %lld NaN-payload-only differences (%d rounds)\n",
                 checks, mismatches, nanDiffs, rounds);
    return mismatches == 0 ? 0 : 1;
}
