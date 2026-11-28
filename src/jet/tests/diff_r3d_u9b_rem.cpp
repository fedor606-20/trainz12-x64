// Differential test of the U9b wave-4 remainder: ReferencePoint, the Cell statistics/tree operations and the
// PhysicalPortal camera/backface state, against Render3DCore_7c_orig.dll in one process.
//
// Every scenario runs on the same seeded buffers on both sides; the returned record (bytes plus counters) must be
// identical. Heap pointers are compared only for equality relationships, never as raw values.
//
// usage: diff_r3d_u9b_rem.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

struct Vec2 {
    float x, y;
};
struct Vec3 {
    float x, y, z;
};
struct Vec4 {
    float x, y, z, w;
};
struct Mat4 {
    float m[16];
};
struct PS {
    void* node;
};
struct It {
    void* node;
};

static_assert(sizeof(Vec2) == 8 && sizeof(Vec3) == 0xc && sizeof(Vec4) == 0x10, "vectors");
static_assert(sizeof(Mat4) == 0x40 && sizeof(PS) == 4 && sizeof(It) == 4, "objects");

static HMODULE mod[2];
static void* pstringCache;
static void* (*CreateNode)(void*, const unsigned char*);
static unsigned long long checks, bad;
static std::mt19937 rng(0x9b4e57);

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
    switch (Pick(0, 9)) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: return Bits(0x7f800000u);
    case 3: return Bits(0xff800000u);
    case 4: return Bits(0x7fc00000u);
    case 5: return Bits(0xffc00001u);
    case 6: return Bits(0x7fa00000u);
    default: return FRange(-10.0f, 10.0f);
    }
}

static float Val(bool awkward)
{
    return awkward ? Awkward() : FRange(-10.0f, 10.0f);
}

static Vec3 RandomVec(bool awkward)
{
    return Vec3{Val(awkward), Val(awkward), Val(awkward)};
}

static Vec4 RandomQuat(bool awkward)
{
    return Vec4{Val(awkward), Val(awkward), Val(awkward), Val(awkward)};
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

static void CheckBytes(const void* a, const void* b, size_t n, const char* what)
{
    const bool ok = std::memcmp(a, b, n) == 0;
    if (!ok) {
        for (size_t i = 0; i < n; ++i) {
            if (static_cast<const unsigned char*>(a)[i] != static_cast<const unsigned char*>(b)[i]) {
                std::printf("%s first diff +0x%x ours=%02x orig=%02x\n", what, static_cast<unsigned>(i),
                            static_cast<const unsigned char*>(a)[i], static_cast<const unsigned char*>(b)[i]);
                break;
            }
        }
    }
    Check(ok, what);
}

static void* Node(int index)
{
    static void* made[8];
    static bool init = false;
    if (!init) {
        const char* texts[8] = {"", "alpha", "beta", "gamma", "delta", "host", "leaf", "path/leaf"};
        for (int i = 0; i < 8; ++i)
            made[i] = CreateNode(pstringCache, reinterpret_cast<const unsigned char*>(texts[i]));
        init = true;
    }
    return made[index];
}

static PS Name(int index)
{
    PS p;
    p.node = Node(index);
    return p;
}

static void Seed(void* p, size_t n)
{
    auto* b = static_cast<unsigned char*>(p);
    for (size_t i = 0; i < n; ++i) b[i] = static_cast<unsigned char>(0xa0 + i * 7);
}

// ---- scenarios -----------------------------------------------------------------------------------------------

struct Record {
    std::vector<unsigned char> bytes;
    std::vector<unsigned int> words;

    void Byte(const void* p, size_t n) { bytes.insert(bytes.end(), static_cast<const unsigned char*>(p), static_cast<const unsigned char*>(p) + n); }
    void Word(unsigned int v) { words.push_back(v); }
    void Bool(bool v) { words.push_back(v ? 1u : 0u); }
};

struct Api {
    void(__thiscall* refCtor0)(void*);
    void(__thiscall* refCtorCopy)(void*, const void*);
    void(__thiscall* refCtorFull)(void*, const PS&, const Vec3&, const Vec4&);
    void(__thiscall* refDtor)(void*);
    void*(__thiscall* refAssign)(void*, const void*);
    PS*(__cdecl* refGetName)(const void*);
    void(__cdecl* refSetName)(void*, const PS&);
    Vec3*(__cdecl* refGetPosition)(const void*);
    void(__cdecl* refSetPosition)(void*, const Vec3&);
    Vec4*(__cdecl* refGetOrientation)(const void*);
    void(__cdecl* refSetOrientation)(void*, const Vec4&);
    void(__cdecl* refSubNames)(const PS&, PS&, PS&);

    unsigned(__cdecl* cellGetStat)(int);
    const unsigned*(__cdecl* cellGetStats)();
    void(__cdecl* cellReset)();
    void(__cdecl* cellResetAll)();
    void(__thiscall* cellCtor0)(void*);
    void(__thiscall* cellCtorCopy)(void*, const void*);
    void(__thiscall* cellDtor)(void*);
    void(__cdecl* cellAdd)(void*, void*);
    bool(__cdecl* cellRemove)(void*, void*);

    void(__thiscall* pCtor0)(void*);
    void(__thiscall* pCtorCopy)(void*, const void*);
    void(__thiscall* pDtor)(void*);
    void(__cdecl* pSetPosition)(void*, const Vec3&);

    void(__thiscall* cameraCtor0)(void*);
    void(__thiscall* cameraCtorPos)(void*, const Vec3&, const Vec4&);
    void(__cdecl* cameraSetPosition)(void*, const Vec3&);

    void(__thiscall* ppCtor)(void*, void*, const Vec2&);
    void(__thiscall* ppDtor)(void*);
    void(__cdecl* ppCull)(void*, void*, const Mat4*);
    bool(__cdecl* ppBack)(const void*, const void*);
    void*(__cdecl* ppCreateCamera)(const void*, void*, const Vec3*, void*, Mat4*);
};

static Api a[2];

static void LoadApi(int s)
{
    Api& q = a[s];
    q.refCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0ReferencePoint@Jet@@QAE@XZ");
    q.refCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0ReferencePoint@Jet@@QAE@ABV01@@Z");
    q.refCtorFull = Proc<void(__thiscall*)(void*, const PS&, const Vec3&, const Vec4&)>(
        s, "??0ReferencePoint@Jet@@QAE@ABVPString@1@ABVVector3@1@ABVQuaternion@1@@Z");
    q.refDtor = Proc<void(__thiscall*)(void*)>(s, "??1ReferencePoint@Jet@@QAE@XZ");
    q.refAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4ReferencePoint@Jet@@QAEAAV01@ABV01@@Z");
    q.refGetName = Proc<PS*(__cdecl*)(const void*)>(s, "?GetName@ReferencePoint@Jet@@QBAABVPString@2@XZ");
    q.refSetName = Proc<void(__cdecl*)(void*, const PS&)>(s, "?SetName@ReferencePoint@Jet@@QAAXABVPString@2@@Z");
    q.refGetPosition = Proc<Vec3*(__cdecl*)(const void*)>(s, "?GetPosition@ReferencePoint@Jet@@QBAABVVector3@2@XZ");
    q.refSetPosition =
        Proc<void(__cdecl*)(void*, const Vec3&)>(s, "?SetPosition@ReferencePoint@Jet@@QAAXABVVector3@2@@Z");
    q.refGetOrientation =
        Proc<Vec4*(__cdecl*)(const void*)>(s, "?GetOrientation@ReferencePoint@Jet@@QBAABVQuaternion@2@XZ");
    q.refSetOrientation =
        Proc<void(__cdecl*)(void*, const Vec4&)>(s, "?SetOrientation@ReferencePoint@Jet@@QAAXABVQuaternion@2@@Z");
    q.refSubNames = Proc<void(__cdecl*)(const PS&, PS&, PS&)>(
        s, "?GetSubNames@ReferencePoint@Jet@@SAXABVPString@2@AAV32@1@Z");

    q.cellGetStat = Proc<unsigned(__cdecl*)(int)>(s, "?GetStat@Cell@Jet@@SAIW4Stats@12@@Z");
    q.cellGetStats = Proc<const unsigned*(__cdecl*)()>(s, "?GetStats@Cell@Jet@@SAPBIXZ");
    q.cellReset = Proc<void(__cdecl*)()>(s, "?Reset@Cell@Jet@@SAXXZ");
    q.cellResetAll = Proc<void(__cdecl*)()>(s, "?ResetAll@Cell@Jet@@SAXXZ");
    q.cellCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0Cell@Jet@@QAE@XZ");
    q.cellCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0Cell@Jet@@QAE@ABV01@@Z");
    q.cellDtor = Proc<void(__thiscall*)(void*)>(s, "??1Cell@Jet@@UAE@XZ");
    q.cellAdd = Proc<void(__cdecl*)(void*, void*)>(s, "?Add@Cell@Jet@@UAAXPAVParticle@2@@Z");
    q.cellRemove = Proc<bool(__cdecl*)(void*, void*)>(s, "?Remove@Cell@Jet@@UAA_NPAVParticle@2@@Z");

    q.pCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0Particle@Jet@@QAE@XZ");
    q.pCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0Particle@Jet@@QAE@ABV01@@Z");
    q.pDtor = Proc<void(__thiscall*)(void*)>(s, "??1Particle@Jet@@UAE@XZ");
    q.pSetPosition = Proc<void(__cdecl*)(void*, const Vec3&)>(s, "?SetPosition@Particle@Jet@@UAAXABVVector3@2@@Z");

    q.cameraCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0Camera@Jet@@QAE@XZ");
    q.cameraCtorPos = Proc<void(__thiscall*)(void*, const Vec3&, const Vec4&)>(
        s, "??0Camera@Jet@@QAE@ABVVector3@1@ABVQuaternion@1@@Z");
    q.cameraSetPosition =
        Proc<void(__cdecl*)(void*, const Vec3&)>(s, "?SetPosition@Camera@Jet@@UAAXABVVector3@2@@Z");

    q.ppCtor = Proc<void(__thiscall*)(void*, void*, const Vec2&)>(s, "??0PhysicalPortal@Jet@@QAE@PAVCell@1@ABVVector2@1@@Z");
    q.ppDtor = Proc<void(__thiscall*)(void*)>(s, "??1PhysicalPortal@Jet@@UAE@XZ");
    q.ppCull = Proc<void(__cdecl*)(void*, void*, const Mat4*)>(
        s, "?Cull@PhysicalPortal@Jet@@UAAXAAVViewPoint@2@ABVMatrix4@2@@Z");
    q.ppBack = Proc<bool(__cdecl*)(const void*, const void*)>(
        s, "?IsBackfacing@PhysicalPortal@Jet@@IBA_NPBVCamera@2@@Z");
    q.ppCreateCamera = Proc<void*(__cdecl*)(const void*, void*, const Vec3*, void*, Mat4*)>(
        s, "?CreateCamera@PhysicalPortal@Jet@@MBAPAVCamera@2@PAV32@PBVVector3@2@PAXAAVMatrix4@2@@Z");
}

static void RunReferencePoint(int s, Record& r)
{
    alignas(16) unsigned char o0[0x20];
    alignas(16) unsigned char o1[0x20];
    alignas(16) unsigned char o2[0x20];
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    Seed(o2, sizeof o2);

    a[s].refCtor0(o0);
    r.Byte(o0, 0x20);  // only the name is cleared; the rest keeps the seed
    a[s].refCtor0(o0);

    const Vec3 p1 = RandomVec(checks % 7 == 0);
    const Vec4 q1 = RandomQuat(checks % 7 == 0);
    a[s].refCtorFull(o1, Name(1), p1, q1);
    r.Byte(o1, 0x20);

    a[s].refCtorCopy(o2, o1);
    r.Byte(o2, 0x20);

    const Vec3 p2 = RandomVec(checks % 7 == 0);
    const Vec4 q2 = RandomQuat(checks % 7 == 0);
    a[s].refAssign(o2, o0);
    r.Byte(o2, 0x20);
    a[s].refSetName(o2, Name(3));
    a[s].refSetPosition(o2, p2);
    a[s].refSetOrientation(o2, q2);
    r.Byte(o2, 0x20);
    r.Byte(a[s].refGetName(o2), 4);
    r.Byte(a[s].refGetPosition(o2), 0xc);
    r.Byte(a[s].refGetOrientation(o2), 0x10);

    PS first{};
    PS second{};
    a[s].refSubNames(Name(7), first, second);
    r.Byte(&first.node, 4);
    r.Byte(&second.node, 4);
    const char* ft = first.node ? reinterpret_cast<const char*>(first.node) + 0x18 : "";
    const char* st = second.node ? reinterpret_cast<const char*>(second.node) + 0x18 : "";
    r.Byte(ft, std::strlen(ft) + 1);
    r.Byte(st, std::strlen(st) + 1);

    a[s].refDtor(o2);
    a[s].refDtor(o1);
}

static void RunCellStats(int s, Record& r)
{
    a[s].cellResetAll();
    for (int i = 0; i < 19; ++i) r.Word(a[s].cellGetStat(i));

    alignas(16) unsigned char cell0[0x158];
    alignas(16) unsigned char cell1[0x158];
    Seed(cell0, sizeof cell0);
    Seed(cell1, sizeof cell1);
    a[s].cellCtor0(cell0);
    a[s].cellCtor0(cell1);
    {
        // the cell's private bounding volume copies its extents from the cell node at +0x30..0x44 (vtable slot 17)
        char* const cell = reinterpret_cast<char*>(cell0);
        char* const bv = *reinterpret_cast<char**>(cell + 0x4c);
        char* const node = *reinterpret_cast<char**>(cell + 0x12c);
        unsigned* const dirty = reinterpret_cast<unsigned*>(bv + 4);
        *dirty |= 8u;
        const float extents[6] = {1.5f, -2.25f, 3.0f, 4.0f, -5.5f, 6.25f};
        std::memcpy(node + 0x30, extents, sizeof extents);
        void** const vt = *reinterpret_cast<void***>(bv);
        reinterpret_cast<void(__cdecl*)(void*)>(vt[17])(bv);
        r.Word(*dirty);
        for (int i = 0; i < 6; ++i) {
            float f;
            std::memcpy(&f, bv + 0x28 + 4 * i, 4);
            unsigned u;
            std::memcpy(&u, &f, 4);
            r.Word(u);
        }
    }
    for (int i = 0; i < 19; ++i) r.Word(a[s].cellGetStat(i));

    a[s].cellReset();
    for (int i = 0; i < 19; ++i) r.Word(a[s].cellGetStat(i));

    a[s].cellDtor(cell1);
    a[s].cellDtor(cell0);
}

static void RunCellTree(int s, Record& r)
{
    alignas(16) unsigned char cell[0x158];
    alignas(16) unsigned char p0[0x12c];
    alignas(16) unsigned char p1[0x12c];
    a[s].cellCtor0(cell);
    a[s].pCtor0(p0);
    a[s].pCtor0(p1);

    const Vec3 v0 = RandomVec(checks % 7 == 0);
    const Vec3 v1 = RandomVec(checks % 7 == 0);
    a[s].pSetPosition(p0, v0);
    a[s].pSetPosition(p1, v1);
    a[s].cellAdd(cell, p0);
    a[s].cellAdd(cell, p1);
    void* const n0 = *reinterpret_cast<void**>(p0 + 0x24);
    void* const n1 = *reinterpret_cast<void**>(p1 + 0x24);
    r.Bool(n0 != nullptr && n0 == n1);
    r.Word(*reinterpret_cast<unsigned*>(static_cast<unsigned char*>(n0) + 0x24));
    r.Byte(static_cast<unsigned char*>(n0) + 0x30, 0x18);
    r.Word(*reinterpret_cast<unsigned*>(cell + 0x130));
    r.Word(a[s].cellGetStat(0));

    // a second add of the same particle is a no-op
    a[s].cellAdd(cell, p0);
    r.Word(*reinterpret_cast<unsigned*>(static_cast<unsigned char*>(n0) + 0x24));
    r.Word(*reinterpret_cast<unsigned*>(cell + 0x130));
    r.Bool(a[s].cellRemove(cell, p1));
    r.Bool(*reinterpret_cast<void**>(p1 + 0x24) == nullptr);
    r.Word(*reinterpret_cast<unsigned*>(cell + 0x130));
    r.Word(a[s].cellGetStat(0));

    // re-add, then remove the other one: the original's root rebuild is only exercised with a non-empty node
    a[s].cellAdd(cell, p1);
    r.Bool(a[s].cellRemove(cell, p0));
    r.Bool(*reinterpret_cast<void**>(p0 + 0x24) == nullptr);
    r.Word(*reinterpret_cast<unsigned*>(cell + 0x130));
    r.Word(a[s].cellGetStat(0));

    a[s].cellDtor(cell);
    a[s].pDtor(p1);
    a[s].pDtor(p0);
}

static void RunPortalCamera(int s, Record& r)
{
    alignas(16) unsigned char cell[0x158];
    alignas(16) unsigned char camera[0x264];
    alignas(16) unsigned char dest[0x158];
    alignas(16) unsigned char portal[0x148];
    Seed(cell, sizeof cell);
    Seed(camera, sizeof camera);
    Seed(dest, sizeof dest);
    Seed(portal, sizeof portal);
    a[s].cellCtor0(cell);
    a[s].cellCtor0(dest);
    a[s].cameraCtor0(camera);

    const Vec3 srcPos = RandomVec(checks % 7 == 0);
    const Vec3 dstPos = RandomVec(checks % 7 == 0);
    a[s].pSetPosition(cell, srcPos);
    a[s].pSetPosition(dest, dstPos);

    Vec2 size{1.0f, 2.0f};
    a[s].ppCtor(portal, dest, size);
    // put the portal in the source cell so CreateCamera takes the source-position path too
    a[s].cellAdd(cell, portal);

    const bool back = a[s].ppBack(portal, camera);
    r.Bool(back);

    Vec3 pos = RandomVec(checks % 7 == 0);
    Mat4 warp;
    std::memset(&warp, 0xee, sizeof warp);
    alignas(16) unsigned char out[0x264];
    Seed(out, sizeof out);
    void* const made = a[s].ppCreateCamera(portal, camera, &pos, out, &warp);
    r.Bool(made == out);
    r.Byte(&warp, sizeof warp);
    r.Byte(out, 0x264);

    // null destination memory returns null and still writes the warp matrix
    Mat4 warp2;
    std::memset(&warp2, 0xee, sizeof warp2);
    r.Bool(a[s].ppCreateCamera(portal, camera, &pos, nullptr, &warp2) == nullptr);
    r.Byte(&warp2, sizeof warp2);

    a[s].ppDtor(portal);
    a[s].cellDtor(dest);
    a[s].cellDtor(cell);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u9b_rem <bin dir> [rounds]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 200;

    SetCurrentDirectoryA(dir.c_str());
    LoadLibraryA("KernelStandard_7c.dll");
    LoadLibraryA("Kernel_7c.dll");
    LoadLibraryA("KernelResource_7c.dll");
    LoadLibraryA("KernelResources_7c.dll");
    LoadLibraryA("Render2DCore_7c.dll");
    LoadLibraryA("RenderSystem_7c.dll");
    mod[1] = LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str());
    mod[0] = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    CreateNode = reinterpret_cast<void* (*)(void*, const unsigned char*)>(
        GetProcAddress(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    pstringCache = *reinterpret_cast<void**>(GetProcAddress(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    if (!CreateNode || !pstringCache) {
        std::printf("no PString cache\n");
        return 2;
    }
    for (int s = 0; s < 2; ++s) LoadApi(s);

    for (int round = 0; round < rounds; ++round) {
        Record rec[2];
        rng.seed(0x1000u + round);
        RunReferencePoint(0, rec[0]);
        rng.seed(0x1000u + round);
        RunReferencePoint(1, rec[1]);
        if (rec[0].bytes != rec[1].bytes) {
            size_t n = rec[0].bytes.size() < rec[1].bytes.size() ? rec[0].bytes.size() : rec[1].bytes.size();
            for (size_t i = 0; i < n; ++i) if (rec[0].bytes[i] != rec[1].bytes[i]) {
                std::printf("ReferencePoint first diff +0x%x ours=%02x orig=%02x\n", (unsigned)i, rec[0].bytes[i], rec[1].bytes[i]); break;
            }
        }
        Check(rec[0].bytes == rec[1].bytes, "ReferencePoint bytes");
        Check(rec[0].words == rec[1].words, "ReferencePoint words");

        rec[0] = Record();
        rec[1] = Record();
        rng.seed(0x2000u + round);
        RunCellStats(0, rec[0]);
        rng.seed(0x2000u + round);
        RunCellStats(1, rec[1]);
        Check(rec[0].words == rec[1].words, "Cell stats");

        rec[0] = Record();
        rec[1] = Record();
        rng.seed(0x3000u + round);
        RunCellTree(0, rec[0]);
        rng.seed(0x3000u + round);
        RunCellTree(1, rec[1]);
        if (rec[0].bytes != rec[1].bytes) {
            size_t n = rec[0].bytes.size() < rec[1].bytes.size() ? rec[0].bytes.size() : rec[1].bytes.size();
            for (size_t i = 0; i < n; ++i) if (rec[0].bytes[i] != rec[1].bytes[i]) {
                std::printf("Cell tree first diff +0x%x ours=%02x orig=%02x\n", (unsigned)i, rec[0].bytes[i], rec[1].bytes[i]); break;
            }
        }
        Check(rec[0].bytes == rec[1].bytes, "Cell tree bytes");
        Check(rec[0].words == rec[1].words, "Cell tree words");

        rec[0] = Record();
        rec[1] = Record();
        rng.seed(0x4000u + round);
        RunPortalCamera(0, rec[0]);
        rng.seed(0x4000u + round);
        RunPortalCamera(1, rec[1]);
        if (rec[0].bytes != rec[1].bytes) {
            size_t n = rec[0].bytes.size() < rec[1].bytes.size() ? rec[0].bytes.size() : rec[1].bytes.size();
            for (size_t i = 0; i < n; ++i) if (rec[0].bytes[i] != rec[1].bytes[i]) {
                std::printf("Portal camera first diff +0x%x ours=%02x orig=%02x\n", (unsigned)i, rec[0].bytes[i], rec[1].bytes[i]); break;
            }
        }
        Check(rec[0].bytes == rec[1].bytes, "PhysicalPortal camera bytes");
        Check(rec[0].words == rec[1].words, "PhysicalPortal camera words");

        ++checks;  // one round
    }

    std::printf("%llu checks, %llu mismatches over %d rounds\n", checks, bad, rounds);
    return bad ? 1 : 0;
}
