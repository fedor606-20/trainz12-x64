// Differential test of WaterSystem_7c unit U1: our module against WaterSystem_7c_orig.dll in one process.
//
// FrameAnimation and WaterGrid<float>/<Vector2> are driven through their exports with random inputs (objects
// compared byte for byte; returned pointers as offsets into the grid). PNoise builds its tables from CRT rand():
// both CRTs (ours static, the original's msvcr80) start a fresh process at the default seed, so the constructors
// see the same stream and the objects compare byte for byte too. On top of that each side's object is copied
// into the other's with operator= and the noise functions are compared on identical inputs, so the noise math is
// checked even if the seeds ever diverge.
//
// usage: diff_ws_u1.exe <dir with WaterSystem_7c.dll, WaterSystem_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

static HMODULE mod[2];  // 0 ours, 1 the original
static unsigned long long checks, bad;
static std::mt19937 rng(9100);
static int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }
static float FPick() { return std::uniform_real_distribution<float>(-3000.0f, 3000.0f)(rng); }

static void* Proc(int side, const char* name)
{
    void* p = reinterpret_cast<void*>(GetProcAddress(mod[side], name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}

static void Check(bool ok, const std::string& what, const std::string& detail = "")
{
    ++checks;
    if (!ok && ++bad <= 40) std::printf("MISMATCH %s %s\n", what.c_str(), detail.c_str());
}

static void Same(const void* a, const void* b, size_t n, const std::string& what, const std::string& detail = "")
{
    Check(std::memcmp(a, b, n) == 0, what, detail);
}

static void SameFloat(float a, float b, const std::string& what, const std::string& detail = "")
{
    Check(std::memcmp(&a, &b, 4) == 0, what, detail);
}

// thiscall through a fastcall pointer: ecx = this, edx unused, the rest on the stack
using Fn0 = void*(__fastcall*)(void*, void*);                       // no args
using Fn1i = void*(__fastcall*)(void*, void*, int);                 // one int (SetNumOctaves)
using Fn2i = void*(__fastcall*)(void*, void*, int, int);            // FrameAnimation ctor
using Fn1f = void(__cdecl*)(void*, float);                          // Update (QAA)
using Fn3f = float(__cdecl*)(void*, float);                         // Noise1, Turbulence1
using Fn3f2 = float(__cdecl*)(void*, float, float);                 // Turbulence2(x, y)
using Fn3f3 = float(__cdecl*)(void*, float, float, float);          // Turbulence3(x, y, z)
using Fn3f4 = float(__cdecl*)(void*, float, float, float, float);
using Fn3f5 = float(__cdecl*)(void*, float, float, float, float, float);
using FnPF = float(__cdecl*)(void*, const float*);                  // Noise2/3, Turbulence2/3
using GetI = int(__cdecl*)(void*, void*);                            // int getters (dummy 2nd arg ignored by cdecl)
using GetP = void*(__cdecl*)(void*, void*);                          // pointer getters (GetData)
using GetF = float(__cdecl*)(void*, void*);                          // GetDeltaFrame
using GetXY = const void*(__cdecl*)(void*, int, int);               // WaterGrid::Get
using SetXY = void(__cdecl*)(void*, int, int, const void*);         // WaterGrid::Set
using GridCtorS = void*(__fastcall*)(void*, void*, const void*);    // ctor(Size)
using GridAssignCdecl = void(__cdecl*)(void*, const void*);        // WaterGrid::operator= (QAA)
using GridInitS = void(__fastcall*)(void*, void*, const void*);     // Initialize(Size)
using GridAssign = void(__fastcall*)(void*, void*, const void*);   // thiscall operator=

struct Size {
    int w, h;
};

static void TestFrameAnimation(int rounds)
{
    auto ctor = reinterpret_cast<Fn2i>(Proc(0, "??0FrameAnimation@Jet@@QAE@HH@Z"));
    auto ctor1 = reinterpret_cast<Fn2i>(Proc(1, "??0FrameAnimation@Jet@@QAE@HH@Z"));
    auto upd = reinterpret_cast<Fn1f>(Proc(0, "?Update@FrameAnimation@Jet@@QAAXM@Z"));
    auto upd1 = reinterpret_cast<Fn1f>(Proc(1, "?Update@FrameAnimation@Jet@@QAAXM@Z"));
    auto assign = reinterpret_cast<GridAssign>(Proc(0, "??4FrameAnimation@Jet@@QAEAAV01@ABV01@@Z"));
    auto assign1 = reinterpret_cast<GridAssign>(Proc(1, "??4FrameAnimation@Jet@@QAEAAV01@ABV01@@Z"));
    auto dtor = reinterpret_cast<Fn0>(Proc(0, "??1FrameAnimation@Jet@@QAE@XZ"));
    auto dtor1 = reinterpret_cast<Fn0>(Proc(1, "??1FrameAnimation@Jet@@QAE@XZ"));
    auto numFrames = reinterpret_cast<GetI>(Proc(0, "?GetNumFrames@FrameAnimation@Jet@@QBAHXZ"));
    auto numFrames1 = reinterpret_cast<GetI>(Proc(1, "?GetNumFrames@FrameAnimation@Jet@@QBAHXZ"));
    auto current = reinterpret_cast<GetI>(Proc(0, "?GetCurrentFrame@FrameAnimation@Jet@@QBAHXZ"));
    auto current1 = reinterpret_cast<GetI>(Proc(1, "?GetCurrentFrame@FrameAnimation@Jet@@QBAHXZ"));
    auto delta = reinterpret_cast<GetF>(Proc(0, "?GetDeltaFrame@FrameAnimation@Jet@@QBAMXZ"));
    auto delta1 = reinterpret_cast<GetF>(Proc(1, "?GetDeltaFrame@FrameAnimation@Jet@@QBAMXZ"));
    auto period = reinterpret_cast<GetI>(Proc(0, "?GetPeriod@FrameAnimation@Jet@@QBAHXZ"));
    auto period1 = reinterpret_cast<GetI>(Proc(1, "?GetPeriod@FrameAnimation@Jet@@QBAHXZ"));
    auto rate = reinterpret_cast<GetI>(Proc(0, "?GetSampleRate@FrameAnimation@Jet@@QBAHXZ"));
    auto rate1 = reinterpret_cast<GetI>(Proc(1, "?GetSampleRate@FrameAnimation@Jet@@QBAHXZ"));

    unsigned char a[0x20], b[0x20];
    for (int r = 0; r < rounds; ++r) {
        int sampleRate = Pick(1, 30), periodValue = Pick(1, 10);
        std::memset(a, 0xcc, sizeof(a));
        std::memset(b, 0xcc, sizeof(b));
        ctor(a, nullptr, sampleRate, periodValue);
        ctor1(b, nullptr, sampleRate, periodValue);
        Same(a, b, 0x18, "fa ctor");
        Check(numFrames(a, nullptr) == numFrames1(b, nullptr), "fa numframes");
        Check(period(a, nullptr) == period1(b, nullptr), "fa period");
        Check(rate(a, nullptr) == rate1(b, nullptr), "fa rate");
        for (int s = 0; s < 30; ++s) {
            float dt = FPick() * 0.1f;
            upd(a, dt);
            upd1(b, dt);
            Same(a, b, 0x18, "fa update");
            Check(current(a, nullptr) == current1(b, nullptr), "fa current");
            SameFloat(delta(a, nullptr), delta1(b, nullptr), "fa delta");
        }
        unsigned char a2[0x20], b2[0x20];
        std::memset(a2, 0xcc, sizeof(a2));
        std::memset(b2, 0xcc, sizeof(b2));
        const int sr2 = Pick(1, 30), per2 = Pick(1, 10);
        ctor(a2, nullptr, sr2, per2);
        ctor1(b2, nullptr, sr2, per2);
        assign(a, nullptr, a2);
        assign1(b, nullptr, b2);
        Same(a, b, 0x18, "fa assign");
        dtor(a, nullptr);
        dtor1(b, nullptr);
    }
    std::printf("-- fa\n");
}

struct GridNames {
    const char *ctorS, *ctorC, *dtor, *assign, *init, *get, *set, *data;
};

static void TestWaterGrid(const char* part, const GridNames& n, int elem, int rounds)
{
    auto ctorS = reinterpret_cast<GridCtorS>(Proc(0, n.ctorS));
    auto ctorS1 = reinterpret_cast<GridCtorS>(Proc(1, n.ctorS));
    GridCtorS ctorC = nullptr, ctorC1 = nullptr;  // the Vector2 instantiation never uses these in the original
    if (*n.ctorC) {
        ctorC = reinterpret_cast<GridCtorS>(Proc(0, n.ctorC));
        ctorC1 = reinterpret_cast<GridCtorS>(Proc(1, n.ctorC));
    }
    auto dtor = reinterpret_cast<Fn0>(Proc(0, n.dtor));
    auto dtor1 = reinterpret_cast<Fn0>(Proc(1, n.dtor));
    GridAssignCdecl assign = nullptr, assign1 = nullptr;
    if (*n.assign) {
        assign = reinterpret_cast<GridAssignCdecl>(Proc(0, n.assign));
        assign1 = reinterpret_cast<GridAssignCdecl>(Proc(1, n.assign));
    }
    auto init = reinterpret_cast<GridInitS>(Proc(0, n.init));
    auto init1 = reinterpret_cast<GridInitS>(Proc(1, n.init));
    auto get = reinterpret_cast<GetXY>(Proc(0, n.get));
    auto get1 = reinterpret_cast<GetXY>(Proc(1, n.get));
    auto set = reinterpret_cast<SetXY>(Proc(0, n.set));
    auto set1 = reinterpret_cast<SetXY>(Proc(1, n.set));
    auto data = reinterpret_cast<GetP>(Proc(0, n.data));
    auto data1 = reinterpret_cast<GetP>(Proc(1, n.data));

    unsigned char a[0x40], b[0x40];
    for (int r = 0; r < rounds; ++r) {
        Size sz{Pick(1, 40), Pick(1, 40)};
        std::memset(a, 0xcc, sizeof(a));
        std::memset(b, 0xcc, sizeof(b));
        ctorS(a, nullptr, &sz);
        ctorS1(b, nullptr, &sz);
        Same(a + 4, b + 4, 8, std::string(part) + " ctor size");  // width, height (vftable and data ptr differ)
        // The original ctor leaves the elements uninitialised, so make the entire grid deterministic before
        // any copy comparison (otherwise heap garbage differs between the two modules by design).
        unsigned char zero[16] = {};
        for (int y = 0; y < sz.h; ++y)
            for (int x = 0; x < sz.w; ++x) {
                set(a, x, y, zero);
                set1(b, x, y, zero);
            }
        Check(data(a, nullptr) != nullptr && data1(b, nullptr) != nullptr, std::string(part) + " ctor data");
        for (int s = 0; s < 30; ++s) {
            int x = Pick(0, sz.w - 1), y = Pick(0, sz.h - 1);
            unsigned char v[16];
            for (int i = 0; i < elem * 4; ++i) v[i] = static_cast<unsigned char>(rng());
            set(a, x, y, v);
            set1(b, x, y, v);
            const void* pa = get(a, x, y);
            const void* pb = get1(b, x, y);
            Same(pa, pb, elem * 4, std::string(part) + " get");
            Check(static_cast<const unsigned char*>(pa) - static_cast<const unsigned char*>(data(a, nullptr))
                      == static_cast<const unsigned char*>(pb) - static_cast<const unsigned char*>(data1(b, nullptr)),
                  std::string(part) + " get offset");
        }
        unsigned char a2[0x40], b2[0x40];
        std::memset(a2, 0xcc, sizeof(a2));
        std::memset(b2, 0xcc, sizeof(b2));
        if (ctorC) {
            ctorC(a2, nullptr, a);
            ctorC1(b2, nullptr, b);
            Same(a2 + 4, b2 + 4, 8, std::string(part) + " copy size");
            for (int s = 0; s < 10; ++s) {
                int x = Pick(0, sz.w - 1), y = Pick(0, sz.h - 1);
                const void* ca = get(a2, x, y);
                const void* cb = get1(b2, x, y);
                if (std::memcmp(ca, cb, elem * 4) != 0) {
                    char detail[160];
                    std::snprintf(detail, sizeof detail, "%s x=%d y=%d w=%d h=%d a0=%g b0=%g a1=%g b1=%g", part, x, y,
                                  sz.w, sz.h, static_cast<const float*>(ca)[0], static_cast<const float*>(cb)[0],
                                  static_cast<const float*>(ca)[elem > 1 ? 1 : 0],
                                  static_cast<const float*>(cb)[elem > 1 ? 1 : 0]);
                    Same(ca, cb, elem * 4, std::string(part) + " copy get", detail);
                }
            }
            assign(a, a2);
            assign1(b, b2);
            Same(a + 4, b + 4, 8, std::string(part) + " assign size");
            dtor(a2, nullptr);
            dtor1(b2, nullptr);
        }
        dtor(a, nullptr);
        dtor1(b, nullptr);
        // Initialize on a default-constructed pair (allocates without touching the elements)
        unsigned char a3[0x40], b3[0x40];
        std::memset(a3, 0xcc, sizeof(a3));
        std::memset(b3, 0xcc, sizeof(b3));
        init(a3, nullptr, &sz);
        init1(b3, nullptr, &sz);
        Same(a3 + 4, b3 + 4, 8, std::string(part) + " init size");
        dtor(a3, nullptr);
        dtor1(b3, nullptr);
    }
    std::printf("-- %s\n", part);
}

static void TestPNoise(int rounds)
{
    // Both modules build the tables from their own CRT rand(). Reset both generators so the two streams agree
    // (the original imports msvcr80, our module uses the UCRT; both start at seed 1).
    if (HMODULE msvcr80 = GetModuleHandleA("msvcr80.dll"))
        if (auto srand80 = reinterpret_cast<void(__cdecl*)(unsigned)>(GetProcAddress(msvcr80, "srand"))) srand80(1);
    std::srand(1);
    auto ctor = reinterpret_cast<Fn0>(Proc(0, "??0PNoise@Jet@@QAE@XZ"));
    auto ctor1 = reinterpret_cast<Fn0>(Proc(1, "??0PNoise@Jet@@QAE@XZ"));
    auto assign = reinterpret_cast<GridAssign>(Proc(0, "??4PNoise@Jet@@QAEAAV01@ABV01@@Z"));
    auto assign1 = reinterpret_cast<GridAssign>(Proc(1, "??4PNoise@Jet@@QAEAAV01@ABV01@@Z"));
    auto oct = reinterpret_cast<void(__cdecl*)(void*, int)>(Proc(0, "?SetNumOctaves@PNoise@Jet@@QAAXH@Z"));
    auto oct1 = reinterpret_cast<void(__cdecl*)(void*, int)>(Proc(1, "?SetNumOctaves@PNoise@Jet@@QAAXH@Z"));
    auto n1 = reinterpret_cast<Fn3f>(Proc(0, "?Noise1@PNoise@Jet@@QAAMM@Z"));
    auto n1o = reinterpret_cast<Fn3f>(Proc(1, "?Noise1@PNoise@Jet@@QAAMM@Z"));
    auto n2 = reinterpret_cast<FnPF>(Proc(0, "?Noise2@PNoise@Jet@@QAAMQAM@Z"));
    auto n2o = reinterpret_cast<FnPF>(Proc(1, "?Noise2@PNoise@Jet@@QAAMQAM@Z"));
    auto n3 = reinterpret_cast<FnPF>(Proc(0, "?Noise3@PNoise@Jet@@QAAMQAM@Z"));
    auto n3o = reinterpret_cast<FnPF>(Proc(1, "?Noise3@PNoise@Jet@@QAAMQAM@Z"));
    auto t1 = reinterpret_cast<Fn3f>(Proc(0, "?Turbulence1@PNoise@Jet@@QAAMM@Z"));
    auto t1o = reinterpret_cast<Fn3f>(Proc(1, "?Turbulence1@PNoise@Jet@@QAAMM@Z"));
    auto t2 = reinterpret_cast<FnPF>(Proc(0, "?Turbulence2@PNoise@Jet@@QAAMQAM@Z"));
    auto t2o = reinterpret_cast<FnPF>(Proc(1, "?Turbulence2@PNoise@Jet@@QAAMQAM@Z"));
    auto t2s = reinterpret_cast<Fn3f2>(Proc(0, "?Turbulence2@PNoise@Jet@@QAAMMM@Z"));
    auto t2so = reinterpret_cast<Fn3f2>(Proc(1, "?Turbulence2@PNoise@Jet@@QAAMMM@Z"));
    auto t3 = reinterpret_cast<FnPF>(Proc(0, "?Turbulence3@PNoise@Jet@@QAAMQAM@Z"));
    auto t3o = reinterpret_cast<FnPF>(Proc(1, "?Turbulence3@PNoise@Jet@@QAAMQAM@Z"));
    auto t3s = reinterpret_cast<Fn3f3>(Proc(0, "?Turbulence3@PNoise@Jet@@QAAMMMM@Z"));
    auto t3so = reinterpret_cast<Fn3f3>(Proc(1, "?Turbulence3@PNoise@Jet@@QAAMMMM@Z"));
    auto tt = reinterpret_cast<Fn3f4>(Proc(0, "?TurbulenceTiled@PNoise@Jet@@QAAMMMMM@Z"));
    auto tto = reinterpret_cast<Fn3f4>(Proc(1, "?TurbulenceTiled@PNoise@Jet@@QAAMMMMM@Z"));
    auto tl = reinterpret_cast<Fn3f5>(Proc(0, "?TurbulenceLoopingTiled@PNoise@Jet@@QAAMMMMMM@Z"));
    auto tlo = reinterpret_cast<Fn3f5>(Proc(1, "?TurbulenceLoopingTiled@PNoise@Jet@@QAAMMMMMM@Z"));

    // both CRTs start a fresh process at the default seed, so the constructors build identical tables
    static unsigned char a[0x4000], b[0x4000];
    std::memset(a, 0, sizeof(a));
    std::memset(b, 0, sizeof(b));
    ctor(a, nullptr);
    ctor1(b, nullptr);
    // Both CRTs start at seed 1 (reset above), so the constructors see the same stream and the whole 0x383C-byte
    // object compares byte for byte; this is the one check that would need relaxing if a CRT ever diverged.
    Same(a, b, 0x383C, "pnoise ctor");
    // Copy our object into the original so the maths below runs on identical tables even if the seeds diverge.
    assign(b, nullptr, a);

    const int octaves[] = {4, 1, 2, 8, 0};
    for (int o : octaves) {
        oct(a, o);
        oct1(b, o);
        for (int r = 0; r < rounds; ++r) {
            float x = FPick(), y = FPick(), z = FPick();
            SameFloat(n1(a, x), n1o(b, x), "noise1");
            float p2[2] = {x, y}, q2[2] = {x, y};
            SameFloat(n2(a, p2), n2o(b, q2), "noise2");
            float p3[3] = {x, y, z}, q3[3] = {x, y, z};
            SameFloat(n3(a, p3), n3o(b, q3), "noise3");
            SameFloat(t1(a, x), t1o(b, x), "turb1");
            float tp2[2] = {x, y}, tq2[2] = {x, y};
            SameFloat(t2(a, tp2), t2o(b, tq2), "turb2");
            SameFloat(t2s(a, x, y), t2so(b, x, y), "turb2s");
            float tp3[3] = {x, y, z}, tq3[3] = {x, y, z};
            SameFloat(t3(a, tp3), t3o(b, tq3), "turb3");
            SameFloat(t3s(a, x, y, z), t3so(b, x, y, z), "turb3s");
            float w = FPick() * 0.1f + 30.0f;
            SameFloat(tt(a, x, y, z, w), tto(b, x, y, z, w), "tiled");
            float lp = FPick() * 0.1f + 10.0f;
            SameFloat(tl(a, x, y, z, w, lp), tlo(b, x, y, z, w, lp), "looping tiled");
        }
    }

    // the noise math itself, independent of the tables: copy each object into the other side and compare again
    assign(b, nullptr, a);
    for (int r = 0; r < rounds; ++r) {
        float x = FPick(), y = FPick(), z = FPick();
        SameFloat(n1(a, x), n1o(b, x), "noise1 cross");
        float p3[3] = {x, y, z}, q3[3] = {x, y, z};
        SameFloat(n3(a, p3), n3o(b, q3), "noise3 cross");
    }
    std::printf("-- pnoise\n");
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_ws_u1 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 200;
    SetDllDirectoryA(argv[1]);
    std::string dir = argv[1];
    // ours is linked at 0x10000000 without relocations; load it first so the original relocates
    mod[0] = LoadLibraryA((dir + "\\WaterSystem_7c.dll").c_str());
    mod[1] = LoadLibraryA((dir + "\\WaterSystem_7c_orig.dll").c_str());
    if (!mod[0] || !mod[1]) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    TestFrameAnimation(rounds);
    static const GridNames gf = {
        "??0?$WaterGrid@M@Jet@@QAE@ABUSize@1@@Z",
        "??0?$WaterGrid@M@Jet@@QAE@ABV01@@Z",
        "??1?$WaterGrid@M@Jet@@QAE@XZ",
        "??4?$WaterGrid@M@Jet@@QAAXABV01@@Z",
        "?Initialize@?$WaterGrid@M@Jet@@UAEXABUSize@2@@Z",
        "?Get@?$WaterGrid@M@Jet@@QBAABMHH@Z",
        "?Set@?$WaterGrid@M@Jet@@QAAXHHABM@Z",
        "?GetData@?$WaterGrid@M@Jet@@QAAPAMXZ"};
    TestWaterGrid("gridf", gf, 1, rounds / 4);
    static const GridNames gv2 = {
        "??0?$WaterGrid@VVector2@Jet@@@Jet@@QAE@ABUSize@1@@Z",
        "",  // Vector2 has no exported copy ctor (never used in the original)
        "??1?$WaterGrid@VVector2@Jet@@@Jet@@QAE@XZ",
        "",  // no exported operator= either
        "?Initialize@?$WaterGrid@VVector2@Jet@@@Jet@@UAEXABUSize@2@@Z",
        "?Get@?$WaterGrid@VVector2@Jet@@@Jet@@QBAABVVector2@2@HH@Z",
        "?Set@?$WaterGrid@VVector2@Jet@@@Jet@@QAAXHHABVVector2@2@@Z",
        "?GetData@?$WaterGrid@VVector2@Jet@@@Jet@@QAAPAVVector2@2@XZ"};
    TestWaterGrid("gridv2", gv2, 2, rounds / 4);
    TestPNoise(rounds / 4);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
