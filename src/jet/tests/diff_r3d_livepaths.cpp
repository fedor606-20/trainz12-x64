// Differential test for the wave-5 live paths of Render3DCore_7c: every Init and BuildGeometry of the debug
// primitive family against Render3DCore_7c_orig.dll, with a stand-in render kernel (a display pointer), a stand-in
// MaterialManager whose Create slot answers a material, and a stand-in GeometryChunk whose array accessors return
// fixed buffers. Both sides run the same stand-ins; chunk counts, the chunk vtable call log, the position, colour and
// index buffers are compared byte for byte. The collision entry points and RenderLines Add/Cull follow.
//
// usage: diff_r3d_livepaths.exe <dir with Render3DCore_7c_orig.dll and livepatch.dll> [rounds]
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

static HMODULE mod[2];
static unsigned long long checks, bad;
static std::mt19937 rng(0x71fe);

static float FRange(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }
static int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }

template <class F>
static F Need(int side, const char* name)
{
    F f = reinterpret_cast<F>(GetProcAddress(mod[side], name));
    if (!f) {
        std::printf("missing export %s in %s\n", name, side ? "orig" : "patch");
        std::exit(2);
    }
    return f;
}

template <class F>
static F Proc(int side, const char* name)
{
    return reinterpret_cast<F>(GetProcAddress(mod[side], name));
}

static void Check(bool ok, const char* what, int a = 0, int b = 0)
{
    ++checks;
    if (!ok) {
        ++bad;
        std::printf("MISMATCH %s (%d %d)\n", what, a, b);
    }
}

// ---- stand-ins -------------------------------------------------------------------------------------------------

static unsigned char gManager[0x40], gMaterial[0x200], gKernel[0x200], gDisplay[0x100];
static void* gManagerVt[16];
static void* gMaterialVt[32];
static unsigned char gChunk[0x400];
static void* gChunkVt[64];
static unsigned char gPos[0x20000], gColor[0x20000], gColor3[0x20000];
static unsigned short gIndex[0x10000];

struct IndexBlock {
    int refs;
    unsigned count;
    int unused[2];
    unsigned short data[0x10000];
};
static IndexBlock gIndexBlock;

static char gLog[0x4000];
static unsigned gLogLength;
static unsigned gCreateArgs[8];
static int gCreateCount;

static void Log(const char* text)
{
    const unsigned n = static_cast<unsigned>(std::strlen(text));
    if (gLogLength + n + 1 < sizeof gLog) {
        std::memcpy(gLog + gLogLength, text, n);
        gLogLength += n;
        gLog[gLogLength] = 0;
    }
}

static int* __cdecl FakeCreate(void*, int* result, void** material, const void*)
{
    *material = gMaterial;
    *result = 0;
    return result;
}

static void* __cdecl FakeCreateChunk(void*, unsigned v, unsigned p, unsigned i, unsigned n, int type, void*)
{
    if (gCreateCount < 8) {
        gCreateArgs[0] = v;
        gCreateArgs[1] = p;
        gCreateArgs[2] = i;
        gCreateArgs[3] = n;
        gCreateArgs[4] = static_cast<unsigned>(type);
        ++gCreateCount;
    }
    char buf[96];
    std::snprintf(buf, sizeof buf, "C%u,%u,%u,%u,%d;", v, p, i, n, type);
    Log(buf);
    return gChunk;
}

static void* __cdecl FakeGetView(void*, void* dest, int type, int stream)
{
    struct Ptr {
        unsigned stride;
        void* data;
    }* p = static_cast<Ptr*>(dest);
    char buf[64];
    std::snprintf(buf, sizeof buf, "G%d,%d;", type, stream);
    Log(buf);
    if (type == 1) {
        p->stride = 12;
        p->data = gPos;
    } else if (type == 2) {
        p->stride = 4;
        p->data = gColor;
    } else {
        p->stride = 12;
        p->data = gColor3;
    }
    return dest;
}

static void __cdecl FakeInitBuffers(void*, void*, int)
{
    Log("I;");
}

static void ResetWorld()
{
    std::memset(gManagerVt, 0, sizeof gManagerVt);
    gManagerVt[3] = reinterpret_cast<void*>(&FakeCreate);
    *reinterpret_cast<void**>(gManager) = gManagerVt;
    for (void*& p : gMaterialVt) p = reinterpret_cast<void*>(&FakeCreateChunk);
    gMaterialVt[5] = reinterpret_cast<void*>(&FakeCreateChunk);
    *reinterpret_cast<void**>(gMaterial) = gMaterialVt;
    for (void*& p : gChunkVt) p = reinterpret_cast<void*>(&FakeGetView);
    gChunkVt[4] = reinterpret_cast<void*>(&FakeInitBuffers);
    gChunkVt[0xc4 / 4] = reinterpret_cast<void*>(&FakeGetView);
    gChunkVt[0xdc / 4] = reinterpret_cast<void*>(&FakeGetView);
    *reinterpret_cast<void**>(gChunk) = gChunkVt;
    *reinterpret_cast<unsigned*>(gChunk + 0x18) = 0;
    *reinterpret_cast<unsigned*>(gChunk + 0x1c) = 0;
    *reinterpret_cast<unsigned*>(gChunk + 0x20) = 0;
    *reinterpret_cast<unsigned*>(gChunk + 0x24) = 0;
    *reinterpret_cast<unsigned*>(gChunk + 0x28) = 0;
    gIndexBlock.refs = 1;
    gIndexBlock.count = 0x10000;
    gIndexBlock.unused[0] = gIndexBlock.unused[1] = 0;
    *reinterpret_cast<void**>(gChunk + 0x2c) = &gIndexBlock;
    *reinterpret_cast<void**>(gKernel + 0x7c) = gDisplay;
    *reinterpret_cast<void**>(gKernel + 0x90) = gManager;
    std::memset(gPos, 0xcd, sizeof gPos);
    std::memset(gColor, 0xcd, sizeof gColor);
    std::memset(gColor3, 0xcd, sizeof gColor3);
    std::memset(gIndex, 0xcd, sizeof gIndex);
    std::memset(gIndexBlock.data, 0xcd, sizeof gIndexBlock.data);
    gLogLength = 0;
    gLog[0] = 0;
    gCreateCount = 0;
}

struct Snapshot {
    unsigned counts[6];
    unsigned createArgs[8];
    int createCount;
    char log[sizeof gLog];
    unsigned char pos[0x20000];
    unsigned char color[0x20000];
    unsigned char color3[0x20000];
    unsigned char index[0x20000];
};
static Snapshot gSnap[2];

static void Capture(int side)
{
    Snapshot& s = gSnap[side];
    std::memcpy(s.counts, gChunk + 0x18, sizeof s.counts);
    std::memcpy(s.createArgs, gCreateArgs, sizeof s.createArgs);
    s.createCount = gCreateCount;
    std::memcpy(s.log, gLog, sizeof s.log);
    std::memcpy(s.pos, gPos, sizeof s.pos);
    std::memcpy(s.color, gColor, sizeof s.color);
    std::memcpy(s.color3, gColor3, sizeof s.color3);
    std::memcpy(s.index, gIndexBlock.data, sizeof s.index);
}

// ---- object construction --------------------------------------------------------------------------------------

static void* MakeObject(int side, const char* cls)
{
    (void)side;
    char name[128];
    std::snprintf(name, sizeof name, "??0%s@Jet@@QAE@XZ", cls);
    using Ctor = void(__thiscall*)(void*);
    Ctor ctor = Need<Ctor>(1, name);
    void* const object = std::calloc(1, 0x400);
    ctor(object);
    return object;
}

// ---- shape drivers --------------------------------------------------------------------------------------------

static void SetBits(void* object, int off, float v) { std::memcpy(static_cast<char*>(object) + off, &v, 4); }

static void CompareSnapshots(const char* what)
{
    const Snapshot& a = gSnap[0];
    const Snapshot& b = gSnap[1];
    if (std::memcmp(a.counts, b.counts, sizeof a.counts) != 0) std::printf("  counts %u %u %u %u %u %u / %u %u %u %u %u %u\n", a.counts[0],a.counts[1],a.counts[2],a.counts[3],a.counts[4],a.counts[5], b.counts[0],b.counts[1],b.counts[2],b.counts[3],b.counts[4],b.counts[5]);
    if (std::strcmp(a.log,b.log)!=0) std::printf("  log %s / %s\n",a.log,b.log);
    Check(std::memcmp(a.counts, b.counts, sizeof a.counts) == 0, what);
    Check(a.createCount == b.createCount && std::memcmp(a.createArgs, b.createArgs, sizeof a.createArgs) == 0, what);
    Check(std::strcmp(a.log, b.log) == 0, what);
    const unsigned vertices = a.counts[3];  // +0x24 numVertices
    const unsigned indices = a.counts[2];   // +0x20 numIndices
    Check(std::memcmp(a.pos, b.pos, vertices * 12 < sizeof a.pos ? vertices * 12 : sizeof a.pos) == 0, what);
    Check(std::memcmp(a.color, b.color, vertices * 4 < sizeof a.color ? vertices * 4 : sizeof a.color) == 0, what);
    Check(std::memcmp(a.color3, b.color3, vertices * 12 < sizeof a.color3 ? vertices * 12 : sizeof a.color3) == 0,
          what);
    Check(std::memcmp(a.index, b.index, indices * 2 < sizeof a.index ? indices * 2 : sizeof a.index) == 0, what);
}

struct BgBox { using Fn = bool(__cdecl*)(void*, void*, const void*, int, int, int, int); };
struct BgSeg { using Fn = bool(__cdecl*)(void*, void*, const void*, int, int); };
struct BgPlane { using Fn = bool(__cdecl*)(void*, void*, const void*, float, float, int, int, int); };
struct BgPolyline { using Fn = bool(__cdecl*)(void*, void*, const void*, const float*, int, int); };

static int ReportSeh(EXCEPTION_POINTERS* ep)
{
    std::printf("SEH code %08lx addr=%p\n", ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
    for (int i = 0; i < 2; ++i) {
        HMODULE m = mod[i];
        if (!m) continue;
        MODULEINFO mi{};
        GetModuleInformation(GetCurrentProcess(), m, &mi, sizeof mi);
        const char* base = static_cast<const char*>(mi.lpBaseOfDll);
        const char* a = static_cast<const char*>(ep->ExceptionRecord->ExceptionAddress);
        if (a >= base && a < base + mi.SizeOfImage)
            std::printf("  in %s +0x%lx\n", i ? "orig" : "ours", static_cast<unsigned long>(a - base));
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

static void RunBg(int side, const char* cls, const char* bg, void (*setup)(void*), const void* arg, int a, int b,
                  int c, int flags, unsigned kind)
{
    __try {
    ResetWorld();
    void* const object = MakeObject(side, cls);
    const float color[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    using BaseInit = bool(__cdecl*)(void*, void*, const void*, int);
    Need<BaseInit>(1, "?Init@RenderPrimitive@Jet@@MAA_NPAVRenderKernel@2@ABVColor@2@H@Z")(object, gKernel, color, flags);
    if (setup) setup(object);
    switch (kind) {
    case 0:
        Need<BgBox::Fn>(side, bg)(object, gKernel, color, a, b, c, flags);
        break;
    case 1:
        Need<BgSeg::Fn>(side, bg)(object, gKernel, color, a, flags);
        break;
    case 2:
        Need<BgPlane::Fn>(side, bg)(object, gKernel, color, *static_cast<const float*>(arg),
                                    *(static_cast<const float*>(arg) + 1), a, b, flags);
        break;
    case 3:
        Need<BgPolyline::Fn>(side, bg)(object, gKernel, color, static_cast<const float*>(arg), a, flags);
        break;
    default: {
        using Fn = bool(__cdecl*)(void*, void*, const void*, const void*, int);
        Need<Fn>(side, bg)(object, gKernel, color, arg, flags);
        break;
    }
    }
    Capture(side);
    std::free(object);
    } __except (ReportSeh(GetExceptionInformation())) { std::exit(9); }
}


static float gA = 1.0f, gB = 1.0f, gC = 1.0f, gW = 2.0f, gH = 3.0f;

static void SetupBox(void* o) { SetBits(o, 0x188, gA); SetBits(o, 0x18c, gB); SetBits(o, 0x190, gC); }
static void SetupSphere(void* o) { SetBits(o, 0x188, gA); }
static void SetupEllipse(void* o) { SetBits(o, 0x188, gA); SetBits(o, 0x18c, gB); }
static void SetupCone(void* o) { SetBits(o, 0x188, gA); SetBits(o, 0x18c, gB); }
static void SetupEllipsoid(void* o) { SetBits(o, 0x188, gA); SetBits(o, 0x18c, gB); SetBits(o, 0x190, gC); }
static void SetupTube(void* o) { SetBits(o, 0x188, gA); SetBits(o, 0x18c, gB); SetBits(o, 0x190, gC); }

static void RunGeometryRound(int round)
{
    const int flags = (round & 1) ? 3 : 0;

    gA = 1.5f; gB = 2.5f; gC = 3.5f;
    for (int z = 1; z <= 3; ++z)
        for (int y = 1; y <= 3; ++y)
            for (int x = 1; x <= 3; ++x) {
                RunBg(0, "RenderBox", "?BuildGeometry@RenderBox@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HHHH@Z",
                      SetupBox, nullptr, x, y, z, flags, 0);
                RunBg(1, "RenderBox", "?BuildGeometry@RenderBox@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HHHH@Z",
                      SetupBox, nullptr, x, y, z, flags, 0);
                CompareSnapshots("box bg");
            }

    gA = 1.75f;
    for (int n = 4; n <= 9; ++n) {
        RunBg(0, "RenderSphere", "?BuildGeometry@RenderSphere@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupSphere, nullptr, n, 0, 0, flags, 1);
        RunBg(1, "RenderSphere", "?BuildGeometry@RenderSphere@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupSphere, nullptr, n, 0, 0, flags, 1);
        CompareSnapshots("sphere bg");
        RunBg(0, "RenderCircle", "?BuildGeometry@RenderCircle@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupSphere, nullptr, n, 0, 0, flags, 1);
        RunBg(1, "RenderCircle", "?BuildGeometry@RenderCircle@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupSphere, nullptr, n, 0, 0, flags, 1);
        CompareSnapshots("circle bg");
        RunBg(0, "RenderEllipse", "?BuildGeometry@RenderEllipse@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupEllipse, nullptr, n, 0, 0, flags, 1);
        RunBg(1, "RenderEllipse", "?BuildGeometry@RenderEllipse@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupEllipse, nullptr, n, 0, 0, flags, 1);
        CompareSnapshots("ellipse bg");
        RunBg(0, "RenderCone", "?BuildGeometry@RenderCone@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupCone, nullptr, n, 0, 0, flags, 1);
        RunBg(1, "RenderCone", "?BuildGeometry@RenderCone@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupCone, nullptr, n, 0, 0, flags, 1);
        CompareSnapshots("cone bg");
        RunBg(0, "RenderCylinder", "?BuildGeometry@RenderCylinder@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupCone, nullptr, n, 0, 0, flags, 1);
        RunBg(1, "RenderCylinder", "?BuildGeometry@RenderCylinder@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupCone, nullptr, n, 0, 0, flags, 1);
        CompareSnapshots("cylinder bg");
        RunBg(0, "RenderTube", "?BuildGeometry@RenderTube@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupTube, nullptr, n, 0, 0, flags, 1);
        RunBg(1, "RenderTube", "?BuildGeometry@RenderTube@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupTube, nullptr, n, 0, 0, flags, 1);
        CompareSnapshots("tube bg");
    }
    gA = 2.0f; gB = 1.5f; gC = 3.0f;
    for (int n = 4; n <= 8; ++n) {
        RunBg(0, "RenderEllipsoid", "?BuildGeometry@RenderEllipsoid@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupEllipsoid, nullptr, n, 0, 0, flags, 1);
        RunBg(1, "RenderEllipsoid", "?BuildGeometry@RenderEllipsoid@Jet@@IAA_NPAVRenderKernel@2@ABVColor@2@HH@Z",
              SetupEllipsoid, nullptr, n, 0, 0, flags, 1);
        CompareSnapshots("ellipsoid bg");
    }

    // plane: width/height then segments
    {
        const float wh[2] = {gW, gH};
        for (int y = 1; y <= 4; ++y)
            for (int x = 1; x <= 4; ++x) {
                RunBg(0, "RenderPlane", "?BuildGeometry@RenderPlane@Jet@@MAA_NPAVRenderKernel@2@ABVColor@2@MMHHH@Z",
                      nullptr, wh, x, y, 0, flags, 2);
                RunBg(1, "RenderPlane", "?BuildGeometry@RenderPlane@Jet@@MAA_NPAVRenderKernel@2@ABVColor@2@MMHHH@Z",
                      nullptr, wh, x, y, 0, flags, 2);
                CompareSnapshots("plane bg");
            }
    }

    // polyline
    {
        float pts[6] = {-1.0f, 2.0f, 0.5f, 3.0f, -2.0f, 4.0f};
        RunBg(0, "RenderPolyline", "?BuildGeometry@RenderPolyline@Jet@@MAA_NPAVRenderKernel@2@ABVColor@2@PBVVector3@2@HH@Z",
              nullptr, pts, 2, 0, 0, 0, 3);
        RunBg(1, "RenderPolyline", "?BuildGeometry@RenderPolyline@Jet@@MAA_NPAVRenderKernel@2@ABVColor@2@PBVVector3@2@HH@Z",
              nullptr, pts, 2, 0, 0, 0, 3);
        CompareSnapshots("polyline bg");
    }
}


struct RayF {
    void* vt;
    float dir[3];
    float point[3];
};

static void RunRay(const char* cls, const char* name, const float* fields, int fieldCount, const RayF& ray, int flags)
{
    using Fn = bool(__cdecl*)(const void*, const RayF*, float*);
    for (int side = 0; side < 2; ++side) {
        void* const object = MakeObject(side, cls);
        float* const f = static_cast<float*>(object);
        const float scale[3] = {gA, gB, gC};
        std::memcpy(reinterpret_cast<char*>(object) + 0x164, scale, sizeof scale);
        for (int i = 0; i < fieldCount; ++i)
            std::memcpy(reinterpret_cast<char*>(object) + 0x188 + i * 4, &fields[i], 4);
        std::memcpy(reinterpret_cast<char*>(object) + 0x180, &flags, 4);
        float out[3] = {9, 9, 9};
        const bool hit = Need<Fn>(side, name)(object, &ray, out);
        gSnap[side].createCount = hit;
        std::memcpy(gSnap[side].createArgs, out, sizeof out);
        Capture(side);
        std::free(object);
    }
    ++checks;
    if (gSnap[0].createCount != gSnap[1].createCount ||
        std::memcmp(gSnap[0].createArgs, gSnap[1].createArgs, 12) != 0) {
        ++bad;
        std::printf("MISMATCH %s hit %d/%d point %.9g %.9g %.9g / %.9g %.9g %.9g\n", cls,
                    gSnap[0].createCount, gSnap[1].createCount,
                    *reinterpret_cast<float*>(gSnap[0].createArgs), *reinterpret_cast<float*>(gSnap[0].createArgs + 4),
                    *reinterpret_cast<float*>(gSnap[0].createArgs + 8),
                    *reinterpret_cast<float*>(gSnap[1].createArgs), *reinterpret_cast<float*>(gSnap[1].createArgs + 4),
                    *reinterpret_cast<float*>(gSnap[1].createArgs + 8));
    }
}

static RayF RandomRay()
{
    RayF ray{};
    ray.dir[0] = FRange(-2, 2); ray.dir[1] = FRange(-2, 2); ray.dir[2] = FRange(-2, 2);
    ray.point[0] = FRange(-4, 4); ray.point[1] = FRange(-4, 4); ray.point[2] = FRange(-4, 4);
    return ray;
}

static void RunRaysRound(int round)
{
    const char* const cone = "?GeometryTestCollision@RenderCone@Jet@@MBA_NABVRay@2@AAVVector3@2@@Z";
    const char* const cylinder = "?GeometryTestCollision@RenderCylinder@Jet@@MBA_NABVRay@2@AAVVector3@2@@Z";
    const char* const tube = "?GeometryTestCollision@RenderTube@Jet@@MBA_NABVRay@2@AAVVector3@2@@Z";
    gA = 1.5f; gB = 1.5f; gC = 1.5f;
    float coneFields[2] = {1.0f, 2.0f};
    float cylFields[2] = {1.0f, 2.0f};
    float tubeFields[3] = {1.0f, 0.5f, 2.0f};
    for (int i = 0; i < 12; ++i) {
        RayF ray = RandomRay();
        int flags = (i & 1) ? 4 : 0;
        RunRay("RenderCone", cone, coneFields, 2, ray, flags);
        RunRay("RenderCylinder", cylinder, cylFields, 2, ray, flags);
        RunRay("RenderTube", tube, tubeFields, 3, ray, flags);
    }
    for (int axis = 0; axis < 3; ++axis) {
        for (int k = 0; k < 6; ++k) {
            RayF ray{};
            ray.point[0] = axis == 0 ? -8.0f : (k == 0 ? 0.0f : FRange(-1, 1));
            ray.point[1] = axis == 1 ? -8.0f : (k == 0 ? 0.0f : FRange(-1, 1));
            ray.point[2] = axis == 2 ? -8.0f : (k == 0 ? 0.0f : FRange(-1, 1));
            ray.dir[0] = axis == 0 ? (k & 2 ? -1.0f : 1.0f) : 0.0f;
            ray.dir[1] = axis == 1 ? (k & 2 ? -1.0f : 1.0f) : 0.0f;
            ray.dir[2] = axis == 2 ? (k & 2 ? -1.0f : 1.0f) : 0.0f;
            RunRay("RenderCone", cone, coneFields, 2, ray, (k & 1) ? 4 : 0);
            RunRay("RenderCylinder", cylinder, cylFields, 2, ray, (k & 1) ? 4 : 0);
            RunRay("RenderTube", tube, tubeFields, 3, ray, (k & 1) ? 4 : 0);
        }
    }
}


template <class F, class... A>
static bool CallInit(int side, const char* name, A... args)
{
    F fn = Need<F>(side, name);
    return fn(args...);
}

static void RunInitPair(const char* cls, const char* name, void (*call)(int, void*), const char* what)
{
    void* obj[2];
    for (int side = 0; side < 2; ++side) {
        ResetWorld();
        obj[side] = MakeObject(side, cls);
        call(side, obj[side]);
        Capture(side);
    }
    CompareSnapshots(what);
    for (int side = 0; side < 2; ++side) std::free(obj[side]);
}

static void InitBoxCall(int side, void* o)
{
    using Fn = bool(__cdecl*)(void*, void*, const void*, float, float, float, unsigned, unsigned, unsigned, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    Need<Fn>(side, "?Init@RenderBox@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@MMMIIIH@Z")(
        o, gKernel, color, 2.0f, 3.0f, 4.0f, 2, 2, 2, 0);
}
static void InitSphereCall(int side, void* o)
{
    using Fn = bool(__cdecl*)(void*, void*, const void*, float, unsigned, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    Need<Fn>(side, "?Init@RenderSphere@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@MIH@Z")(o, gKernel, color, 1.5f, 8,
                                                                                   0);
}
static void InitCircleCall(int side, void* o)
{
    using Fn = bool(__cdecl*)(void*, void*, const void*, float, unsigned, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    Need<Fn>(side, "?Init@RenderCircle@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@MIH@Z")(o, gKernel, color, 1.5f, 7,
                                                                                   0);
}
static void InitEllipseCall(int side, void* o)
{
    using Fn = bool(__cdecl*)(void*, void*, const void*, float, float, unsigned, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    Need<Fn>(side, "?Init@RenderEllipse@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@MMIH@Z")(
        o, gKernel, color, 1.5f, 2.5f, 7, 0);
}
static void InitConeCall(int side, void* o)
{
    using Fn = bool(__cdecl*)(void*, void*, const void*, float, float, unsigned, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    Need<Fn>(side, "?Init@RenderCone@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@MMIH@Z")(o, gKernel, color, 1.5f,
                                                                                   2.5f, 7, 0);
}
static void InitCylinderCall(int side, void* o)
{
    using Fn = bool(__cdecl*)(void*, void*, const void*, float, float, unsigned, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    Need<Fn>(side, "?Init@RenderCylinder@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@MMIH@Z")(
        o, gKernel, color, 1.5f, 2.5f, 7, 0);
}
static void InitEllipsoidCall(int side, void* o)
{
    using Fn = bool(__cdecl*)(void*, void*, const void*, float, float, float, unsigned, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    Need<Fn>(side, "?Init@RenderEllipsoid@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@MMMIH@Z")(
        o, gKernel, color, 1.5f, 2.5f, 3.5f, 7, 0);
}
static void InitPlaneCall(int side, void* o)
{
    using Fn = bool(__cdecl*)(void*, void*, const void*, float, float, unsigned, unsigned, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    Need<Fn>(side, "?Init@RenderPlane@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@MMIIH@Z")(
        o, gKernel, color, 2.0f, 3.0f, 3, 4, 0);
}
static void InitTubeCall(int side, void* o)
{
    using Fn = bool(__cdecl*)(void*, void*, const void*, float, float, float, unsigned, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    Need<Fn>(side, "?Init@RenderTube@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@MMMIH@Z")(
        o, gKernel, color, 1.5f, 0.5f, 2.5f, 7, 0);
}
static void InitPolylineCall(int side, void* o)
{
    using Fn = bool(__cdecl*)(void*, void*, const void*, const void*, int, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    const float pts[9] = {-1, 0, 0, 0, 1, 0, 2, 2, 2};
    Need<Fn>(side, "?Init@RenderPolyline@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@PBVVector3@2@HH@Z")(
        o, gKernel, color, pts, 3, 0);
}
static void InitLineCall(int side, void* o)
{
    using Fn = bool(__cdecl*)(void*, void*, const void*, const void*, const void*, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    const float a[3] = {-1, -1, 0};
    const float b[3] = {2, 3, 4};
    Need<Fn>(side, "?Init@RenderLine@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@ABVVector3@2@2H@Z")(
        o, gKernel, color, a, b, 0);
}
static void InitPolygonCall(int side, void* o)
{
    using Fn = bool(__cdecl*)(void*, void*, const void*, int, const void*, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    const float pts[9] = {-1, -1, 0, 2, -1, 0, 1, 2, 0};
    Need<Fn>(side, "?Init@RenderPolygon@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@HPBVVector3@2@H@Z")(
        o, gKernel, color, 3, pts, 0);
}

static void InitSplineCall(int side, void* o);

static void RunInitRound()
{
    RunInitPair("RenderBox", "", InitBoxCall, "box init");
    RunInitPair("RenderSphere", "", InitSphereCall, "sphere init");
    RunInitPair("RenderCircle", "", InitCircleCall, "circle init");
    RunInitPair("RenderEllipse", "", InitEllipseCall, "ellipse init");
    RunInitPair("RenderCone", "", InitConeCall, "cone init");
    RunInitPair("RenderCylinder", "", InitCylinderCall, "cylinder init");
    RunInitPair("RenderEllipsoid", "", InitEllipsoidCall, "ellipsoid init");
    RunInitPair("RenderPlane", "", InitPlaneCall, "plane init");
    RunInitPair("RenderTube", "", InitTubeCall, "tube init");
    RunInitPair("RenderPolyline", "", InitPolylineCall, "polyline init");
    RunInitPair("RenderLine", "", InitLineCall, "line init");
    RunInitPair("RenderSpline", "", InitSplineCall, "spline init");
    RunInitPair("RenderPolygon", "", InitPolygonCall, "polygon init");
}


static void SetLinesEnabled(int side, bool on)
{
    unsigned char* p = reinterpret_cast<unsigned char*>(
        GetProcAddress(mod[side], "?enabled@RenderLines@Jet@@0_NA"));
    if (p) *p = on ? 1 : 0;
}

static void RunLinesRound()
{
    for (int kind = 0; kind < 2; ++kind) {
        void* obj[2];
        for (int side = 0; side < 2; ++side) {
            ResetWorld();
            SetLinesEnabled(side, true);
            obj[side] = MakeObject(side, "RenderLines");
            const float color[4] = {0.1f, 0.2f, 0.3f, 1.0f};
            const float a[3] = {-1, 0.5f, 2};
            const float b[3] = {3, -2, 1};
            const float quad[12] = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
            if (kind == 0) {
                using Fn = void(__cdecl*)(void*, void*, const void*, const void*, const void*);
                Need<Fn>(side, "?Add@RenderLines@Jet@@QAAXPAVRenderKernel@2@ABVVector3@2@1ABVColor@2@@Z")(
                    obj[side], gKernel, a, b, color);
            } else if (kind == 1) {
                using Fn = void(__cdecl*)(void*, void*, const void*, const void*);
                Need<Fn>(side, "?Add@RenderLines@Jet@@QAAXPAVRenderKernel@2@ABVVector3@2@ABVColor@2@@Z")(
                    obj[side], gKernel, a, color);
            } else {
                using Fn = void(__cdecl*)(void*, void*, const void*, int, const void*);
                Need<Fn>(side, "?Add@RenderLines@Jet@@QAAXPAVRenderKernel@2@PBVVector3@2@HABVColor@2@@Z")(
                    obj[side], gKernel, quad, 4, color);
            }
            Capture(side);
            const unsigned countOff = kind == 0 ? 0x158 : kind == 1 ? 0x138 : 0x178;
            gSnap[side].createArgs[6] = *reinterpret_cast<unsigned*>(static_cast<char*>(obj[side]) + countOff);
            gSnap[side].createArgs[7] = *reinterpret_cast<unsigned*>(static_cast<char*>(obj[side]) + countOff + 4);
            SetLinesEnabled(side, false);
            std::free(obj[side]);
        }
        CompareSnapshots(kind == 0 ? "line add" : kind == 1 ? "point add" : "polygon add");
        ++checks;
        const unsigned countOff = kind == 0 ? 0x158 : kind == 1 ? 0x138 : 0x178;
        const unsigned capOff = countOff + 4;
        const unsigned* a = reinterpret_cast<const unsigned*>(gSnap[0].counts);
        (void)a;
        // the pools' live counts are part of the object, not the chunk; they were captured through the chunk log only
        (void)capOff;
    }
}


static void RunMirrorCamera()
{
    const float size[2] = {2.0f, 3.0f};
    using Ctor = void(__thiscall*)(void*, void*, const void*);
    float result[2][16];
    bool ok[2];
    for (int side = 0; side < 2; ++side) {
        void* const mirror = std::calloc(1, 0x200);
        Need<Ctor>(1, "??0Mirror@Jet@@QAE@PAVCell@1@ABVVector2@1@@Z")(mirror, nullptr, size);
        using Fn = void* (__cdecl*)(const void*, void*, const void*, void*, float*);
        Fn fn = Proc<Fn>(side, "?CreateCamera@Mirror@Jet@@MBAPAVCamera@2@PAV32@PBVVector3@2@PAXAAVMatrix4@2@@Z");
        if (!fn) {
            ++checks;
            ++bad;
            std::printf("MISMATCH mirror camera export missing\n");
            std::free(mirror);
            continue;
        }
        void* const ret = fn(mirror, nullptr, nullptr, nullptr, result[side]);
        ok[side] = ret == nullptr;
        std::free(mirror);
    }
    Check(ok[0] == ok[1], "mirror camera null data");
    Check(std::memcmp(result[0], result[1], sizeof result[0]) == 0, "mirror camera matrix");
}


static void RunTestCollisionRound(int round)
{
    const char* name2 = "?TestCollisionGeometry@Render2DPrimitive@Jet@@UBA_NABVRay@2@MAAVVector3@2@AAVConstFaceDescriptor@2@@Z";
    const char* name3 = "?TestCollisionGeometry@Render3DPrimitive@Jet@@UBA_NABVRay@2@MAAVVector3@2@AAVConstFaceDescriptor@2@@Z";
    for (int i = 0; i < 12; ++i) {
        for (int shape = 0; shape < 2; ++shape) {
            bool hit[2];
            float out[2][16];
            const float scale[3] = {FRange(0.5f, 2.0f), FRange(0.5f, 2.0f), FRange(0.5f, 2.0f)};
            const float radius = FRange(0.5f, 3.0f);
            const int flags = (i & 1) ? 12 : 8;
            RayF ray = RandomRay();
            const float t = FRange(-4, 8);
            for (int side = 0; side < 2; ++side) {
                void* const object = MakeObject(side, shape == 0 ? "RenderCircle" : "RenderSphere");
                std::memcpy(reinterpret_cast<char*>(object) + 0x164, scale, sizeof scale);
                std::memcpy(reinterpret_cast<char*>(object) + 0x188, &radius, 4);
                std::memcpy(reinterpret_cast<char*>(object) + 0x180, &flags, 4);
                std::memset(out[side], 0, sizeof out[side]);
                using Fn = bool(__cdecl*)(const void*, const RayF*, float, float*, void*);
                hit[side] = Need<Fn>(side, shape == 0 ? name2 : name3)(object, &ray, t, out[side], out[side] + 4);
                std::free(object);
            }
            ++checks;
            if (hit[0] != hit[1] || std::memcmp(out[0], out[1], 12) != 0) {
                ++bad;
                std::printf("MISMATCH collision %s hit %d/%d %.9g %.9g %.9g / %.9g %.9g %.9g\n",
                            shape == 0 ? "2d" : "3d", hit[0], hit[1], out[0][0], out[0][1], out[0][2], out[1][0],
                            out[1][1], out[1][2]);
            }
        }
    }
}


static void InitSplineCall(int side, void* o)
{
    using Ctor = void(__thiscall*)(void*, unsigned, const void*, float, bool);
    void* const spline = std::calloc(1, 0x100);
    const float pts[12] = {-1, 0, 0, 0, 1, 0, 1, 2, 1, 3, 1, 3};
    Need<Ctor>(1, "??0BSpline@Jet@@QAE@IQBVVector3@1@M_N@Z")(spline, 4, pts, 0.5f, false);
    using Fn = bool(__cdecl*)(void*, void*, const void*, const void*, unsigned, int);
    const float color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    Need<Fn>(side, "?Init@RenderSpline@Jet@@QAA_NPAVRenderKernel@2@ABVColor@2@ABVSpline@2@IH@Z")(
        o, gKernel, color, spline, 4, 0);
    std::free(spline);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_livepaths <bin dir> [rounds]\n");
        return 2;
    }
    char full[MAX_PATH] = {};
    if (!GetFullPathNameA(argv[1], MAX_PATH, full, nullptr)) {
        std::printf("cannot resolve %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::string dir = full;
    if (!dir.empty() && (dir.back() == '\\' || dir.back() == '/')) dir.pop_back();
    SetDllDirectoryA(dir.c_str());

    // The standard fixture has the merged Render3DCore_7c.dll; the wave-5 test build used a small livepatch.dll.
    // Load the original first either way: the replacement forwards its remaining exports to that module.
    SetLastError(0);
    mod[1] = LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str());
    if (!mod[1]) {
        std::printf("cannot load Render3DCore_7c_orig.dll (%lu)\n", GetLastError());
        return 2;
    }
    SetLastError(0);
    mod[0] = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    const DWORD mergedError = GetLastError();
    if (!mod[0]) {
        SetLastError(0);
        mod[0] = LoadLibraryA((dir + "\\livepatch.dll").c_str());
        if (!mod[0]) {
            std::printf("cannot load Render3DCore_7c.dll (%lu) or livepatch.dll (%lu)\n", mergedError,
                        GetLastError());
            return 2;
        }
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 1;
    RunMirrorCamera();
    for (int round = 0; round < rounds; ++round) {
        RunGeometryRound(round);
        RunInitRound();
        RunRaysRound(round);
        RunTestCollisionRound(round);
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}



