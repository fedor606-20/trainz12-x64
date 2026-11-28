// Differential test of Render3DCore_7c unit U10: our render-primitive classes against Render3DCore_7c_orig.dll in one
// process. Both modules receive the same inputs; object state (selected fields, block counts, float bit patterns
// including NaN/inf/-0), return values and the vtable layout are compared.
//
// usage: diff_r3d_u10.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
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
struct Vec4 {
    float x, y, z, w;
};
static_assert(sizeof(Vec3) == 0xc, "Vec3");
static_assert(sizeof(Vec4) == 0x10, "Vec4");

static HMODULE mod[2];
static unsigned long long checks, bad;
static std::mt19937 rng(0x5eed);

static int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }
static float FRange(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }

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

static Vec3 RandomVec(bool awkward)
{
    return Vec3{awkward ? Awkward() : FRange(-100.0f, 100.0f), awkward ? Awkward() : FRange(-100.0f, 100.0f),
                awkward ? Awkward() : FRange(-100.0f, 100.0f)};
}

template <class F>
static F Proc(int side, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(mod[side], name));
    return p;
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

static void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        std::printf("MISMATCH %s\n", what);
    }
}

static uint32_t U32(const void* p, unsigned off)
{
    uint32_t v;
    std::memcpy(&v, static_cast<const char*>(p) + off, 4);
    return v;
}

static float F32(const void* p, unsigned off)
{
    float v;
    std::memcpy(&v, static_cast<const char*>(p) + off, 4);
    return v;
}

static bool SameBits(float a, float b)
{
    return U32(&a, 0) == U32(&b, 0);
}

static void CheckBits(const char* what, float a, float b)
{
    if (!SameBits(a, b)) std::printf("%s: ours %08x orig %08x\n", what, U32(&a, 0), U32(&b, 0));
    Check(SameBits(a, b), what);
}

static void CheckU32(const char* what, uint32_t a, uint32_t b)
{
    if (a != b) std::printf("%s: ours %08x orig %08x\n", what, a, b);
    Check(a == b, what);
}

static void CheckVec(const char* what, const Vec3& a, const Vec3& b)
{
    CheckBits(what, a.x, b.x);
    CheckBits(what, a.y, b.y);
    if (!SameBits(a.z, b.z)) std::printf("%s.z\n", what);
    Check(SameBits(a.z, b.z), what);
}

// ---- the object snapshot ----------------------------------------------------------------------------------------

struct ClassDef {
    const char* name;
    unsigned size;
    unsigned fieldOff;  // shape-specific bytes, 0 when none
    unsigned fieldCount;
    bool polygonFields;  // skip the secondary vptr at +0x188
    bool lines;
};

static const ClassDef classes[] = {
    {"RenderPrimitive", 0x188, 0, 0, false, false},
    {"Render2DPrimitive", 0x188, 0, 0, false, false},
    {"Render3DPrimitive", 0x188, 0, 0, false, false},
    {"RenderBox", 0x194, 0x188, 3, false, false},
    {"RenderSphere", 0x18c, 0x188, 1, false, false},
    {"RenderCircle", 0x18c, 0x188, 1, false, false},
    {"RenderEllipse", 0x190, 0x188, 2, false, false},
    {"RenderCone", 0x190, 0x188, 2, false, false},
    {"RenderCylinder", 0x190, 0x188, 2, false, false},
    {"RenderEllipsoid", 0x194, 0x188, 3, false, false},
    {"RenderPlane", 0x190, 0x188, 2, false, false},
    {"RenderTube", 0x194, 0x188, 3, false, false},
    {"RenderPolyline", 0x198, 0x188, 4, false, false},
    {"RenderLine", 0x198, 0x188, 4, false, false},
    {"RenderSpline", 0x198, 0x188, 4, false, false},
    {"RenderPolygon", 0x1a4, 0x188, 5, true, false},
    {"RenderLines", 0x188, 0, 0, false, true},
};

static void ComparePrimitiveState(const void* a, const void* b, const ClassDef& cd, const char* what)
{
    char label[256];
    for (unsigned off = 4; off < 0x4c; off += 4) {
        uint32_t va = U32(a, off), vb = U32(b, off);
        if (off == 0x48) {  // Particle::flags: only bits 0/1 are defined
            va &= 3;
            vb &= 3;
        }
        if (va != vb) {
            std::snprintf(label, sizeof label, "%s particle+%x", what, off);
            Check(false, label);
            return;
        }
    }
    CheckU32("renderFlags", U32(a, 0x12c), U32(b, 0x12c));
    CheckU32("aliasCount", U32(a, 0x130), U32(b, 0x130));
    CheckU32("aliasCapacity", U32(a, 0x134), U32(b, 0x134));
    CheckU32("aliases-null", U32(a, 0x138) == 0, U32(b, 0x138) == 0);
    CheckU32("kernel bytes", U32(a, 0x13c), U32(b, 0x13c));
    const uint32_t ca = U32(a, 0x140), cb = U32(b, 0x140);
    const uint32_t ma = U32(a, 0x144), mb = U32(b, 0x144);
    CheckU32("chunks-null", ca == 0, cb == 0);
    CheckU32("materials-null", ma == 0, mb == 0);
    if (ca && cb) {
        CheckU32("chunk refs", U32(reinterpret_cast<void*>(ca), 0), U32(reinterpret_cast<void*>(cb), 0));
        CheckU32("chunk count", U32(reinterpret_cast<void*>(ca), 4), U32(reinterpret_cast<void*>(cb), 4));
    }
    if (ma && mb) {
        CheckU32("material refs", U32(reinterpret_cast<void*>(ma), 0), U32(reinterpret_cast<void*>(mb), 0));
        CheckU32("material count", U32(reinterpret_cast<void*>(ma), 4), U32(reinterpret_cast<void*>(mb), 4));
    }
    for (unsigned off = 0x148; off < 0x170; ++off) {
        if (U32(a, off) != U32(b, off)) {
            std::snprintf(label, sizeof label, "%s transform+%x", what, off);
            Check(false, label);
            break;
        }
    }
    for (unsigned off = 0x170; off < 0x180; ++off) {
        if (U32(a, off) != U32(b, off)) {
            std::snprintf(label, sizeof label, "%s color+%x", what, off);
            Check(false, label);
            break;
        }
    }
    CheckU32("creationFlags", U32(a, 0x180), U32(b, 0x180));
    CheckU32("flags low", U32(a, 0x184) & 0x0f, U32(b, 0x184) & 0x0f);
    if (cd.fieldCount) {
        const unsigned start = cd.fieldOff + (cd.polygonFields ? 4 : 0);
        for (unsigned i = 0; i < cd.fieldCount; ++i) {
            const unsigned off = start + i * 4;
            if (U32(a, off) != U32(b, off)) {
                std::snprintf(label, sizeof label, "%s field+%x", what, off);
                Check(false, label);
                return;
            }
        }
        Check(true, what);
    }
}

static void CompareLinesState(const void* a, const void* b, const char* what)
{
    for (unsigned off = 4; off < 0x4c; off += 4) {
        if (U32(a, off) != U32(b, off)) {
            Check(false, what);
            return;
        }
    }
    CheckU32("spheres count", U32(a, 0x12c), U32(b, 0x12c));
    CheckU32("spheres cap", U32(a, 0x130), U32(b, 0x130));
    CheckU32("spheres data-null", U32(a, 0x134) == 0, U32(b, 0x134) == 0);
    CheckU32("sphereCount", U32(a, 0x138), U32(b, 0x138));
    CheckU32("sphereCap", U32(a, 0x13c), U32(b, 0x13c));
    CheckU32("lines count", U32(a, 0x140), U32(b, 0x140));
    CheckU32("lines cap", U32(a, 0x144), U32(b, 0x144));
    CheckU32("lines data-null", U32(a, 0x148) == 0, U32(b, 0x148) == 0);
    CheckU32("pool count", U32(a, 0x14c), U32(b, 0x14c));
    CheckU32("pool cap", U32(a, 0x150), U32(b, 0x150));
    CheckU32("pool data-null", U32(a, 0x154) == 0, U32(b, 0x154) == 0);
    CheckU32("lineCount", U32(a, 0x158), U32(b, 0x158));
    CheckU32("lineCap", U32(a, 0x15c), U32(b, 0x15c));
    CheckU32("polys count", U32(a, 0x160), U32(b, 0x160));
    CheckU32("polys cap", U32(a, 0x164), U32(b, 0x164));
    CheckU32("polys data-null", U32(a, 0x168) == 0, U32(b, 0x168) == 0);
    CheckU32("poly pool count", U32(a, 0x16c), U32(b, 0x16c));
    CheckU32("poly pool cap", U32(a, 0x170), U32(b, 0x170));
    CheckU32("poly pool data-null", U32(a, 0x174) == 0, U32(b, 0x174) == 0);
    CheckU32("polygonCount", U32(a, 0x178), U32(b, 0x178));
    CheckU32("polygonCap", U32(a, 0x17c), U32(b, 0x17c));
    CheckU32("lastFrame", U32(a, 0x180), U32(b, 0x180));
    CheckU32("lines kernel-null", U32(a, 0x184) == 0, U32(b, 0x184) == 0);
}

// ---- vtable layout ----------------------------------------------------------------------------------------------

struct Obj {
    void* vptr;
};

static bool IsExecutable(const void* p)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof mbi)) return false;
    return mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                                                       PAGE_EXECUTE_WRITECOPY)) != 0;
}

static bool IsModuleCode(int side, void* p)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(mod[side]);
    const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    const uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (std::memcmp(sec->Name, ".text", 5) != 0) continue;
        const uintptr_t lo = base + sec->VirtualAddress;
        return addr >= lo && addr < lo + sec->Misc.VirtualSize;
    }
    return false;
}

static void CheckVtableLength(int side, void* object, const char* vtableName, unsigned& length)
{
    void* const vt = *reinterpret_cast<void**>(object);
    void* const exported = Proc<void*>(side, vtableName);
    Check(vt == exported, vtableName);
    unsigned n = 0;
    while (n < 0x200 && IsModuleCode(side, reinterpret_cast<void**>(vt)[n])) ++n;
    length = n;
}

static void CheckVtableSlot(int side, void* object, unsigned slot, const char* name, const char* what)
{
    if (!name || !*name) return;
    void* const fn = Proc<void*>(side, name);
    if (!fn) {
        Check(false, what);
        return;
    }
    void* const actual = reinterpret_cast<void**>(*reinterpret_cast<void**>(object))[slot / 4];
    if (actual != fn) std::printf("%s slot %x: vtable %p export %p\n", what, slot, actual, fn);
    Check(actual == fn, what);
}

// ---- test driver ------------------------------------------------------------------------------------------------

struct Api {
    void(__thiscall* ctor0)(void*);
    void(__thiscall* ctorInt)(void*, int);
    void(__thiscall* ctorCopy)(void*, const void*);
    void(__thiscall* dtor)(void*);
    void* (__thiscall* assign)(void*, const void*);
    void* (__cdecl* createInstance)(const void*);
    void(__cdecl* initInstance)(void*, const void*);
    float(__cdecl* radius)(const void*);
    void(__cdecl* calculate)(void*);
    bool(__cdecl* isWithin)(const void*, const Vec3*);
    void(__cdecl* setPos)(void*, const Vec3*);
    void(__cdecl* setScale)(void*, const Vec3*);
    void(__cdecl* setOrient)(void*, const Vec4*);
    const Vec3* (__cdecl* getPos)(const void*);
    const Vec3* (__cdecl* getScale)(const void*);
    const Vec4* (__cdecl* getOrient)(const void*);
    void(__cdecl* setColor)(void*, const Vec4*);
    const Vec4* (__cdecl* getColor)(const void*);
    int(__cdecl* getCreationFlags)(const void*);
    unsigned(__cdecl* getNumChunks)(const void*);
    void(__cdecl* setTestCollision)(void*, bool);
    bool(__cdecl* getTestCollision)(const void*);
    void(__cdecl* setDebugRender)(void*, bool);
    bool(__cdecl* getDebugRender)(const void*);
    float(__cdecl* getOpacity)(const void*);
    void(__cdecl* setOpacity)(void*, float);
};

static std::string M(const char* cls, const char* pattern)
{
    char buf[512];
    std::snprintf(buf, sizeof buf, pattern, cls);
    return buf;
}

static Api LoadApi(int side, const char* cls)
{
    Api a{};
    a.ctor0 = Proc<decltype(a.ctor0)>(side, M(cls, "??0%s@Jet@@QAE@XZ").c_str());
    a.ctorInt = Proc<decltype(a.ctorInt)>(side, M(cls, "??0%s@Jet@@QAE@H@Z").c_str());
    a.ctorCopy = Proc<decltype(a.ctorCopy)>(side, M(cls, "??0%s@Jet@@QAE@ABV01@@Z").c_str());
    a.dtor = Proc<decltype(a.dtor)>(side, M(cls, "??1%s@Jet@@UAE@XZ").c_str());
    a.assign = Proc<decltype(a.assign)>(side, M(cls, "??4%s@Jet@@QAEAAV01@ABV01@@Z").c_str());
    a.createInstance = Proc<decltype(a.createInstance)>(side, M(cls, "?CreateInstance@%s@Jet@@UBAPAVParticle@2@XZ").c_str());
    a.initInstance = Proc<decltype(a.initInstance)>(side, M(cls, "?InitInstance@%s@Jet@@IBAXAAV12@@Z").c_str());
    a.radius = Proc<decltype(a.radius)>(side, M(cls, "?GetGeometryRadius@%s@Jet@@MBAMXZ").c_str());
    a.calculate = Proc<decltype(a.calculate)>(side, M(cls, "?CalculatePrimitiveVolume@%s@Jet@@MAAXXZ").c_str());
    a.isWithin = Proc<decltype(a.isWithin)>(side, M(cls, "?IsWithin@%s@Jet@@MBA_NABVVector3@2@@Z").c_str());
    a.setPos = Proc<decltype(a.setPos)>(side, "?SetGeometryPosition@RenderPrimitive@Jet@@UAAXABVVector3@2@@Z");
    a.setScale = Proc<decltype(a.setScale)>(side, "?SetScale@RenderPrimitive@Jet@@UAAXABVVector3@2@@Z");
    a.setOrient = Proc<decltype(a.setOrient)>(side, "?SetGeometryOrientation@RenderPrimitive@Jet@@UAAXABVQuaternion@2@@Z");
    a.getPos = Proc<decltype(a.getPos)>(side, "?GetGeometryPosition@RenderPrimitive@Jet@@UBAABVVector3@2@XZ");
    a.getScale = Proc<decltype(a.getScale)>(side, "?GetScale@RenderPrimitive@Jet@@UBAABVVector3@2@XZ");
    a.getOrient = Proc<decltype(a.getOrient)>(side, "?GetGeometryOrientation@RenderPrimitive@Jet@@UBAABVQuaternion@2@XZ");
    a.setColor = Proc<decltype(a.setColor)>(side, "?SetColor@RenderPrimitive@Jet@@QAAXABVColor@2@@Z");
    a.getColor = Proc<decltype(a.getColor)>(side, "?GetColor@RenderPrimitive@Jet@@QBAABVColor@2@XZ");
    a.getCreationFlags = Proc<decltype(a.getCreationFlags)>(side, "?GetCreationFlags@RenderPrimitive@Jet@@QBAHXZ");
    a.getNumChunks = Proc<decltype(a.getNumChunks)>(side, "?GetNumChunks@RenderPrimitive@Jet@@QBAIXZ");
    a.setTestCollision = Proc<decltype(a.setTestCollision)>(side, "?SetTestCollision@RenderPrimitive@Jet@@QAAX_N@Z");
    a.getTestCollision = Proc<decltype(a.getTestCollision)>(side, "?GetTestCollision@RenderPrimitive@Jet@@QBA_NXZ");
    a.setDebugRender = Proc<decltype(a.setDebugRender)>(side, "?SetDebugRender@RenderPrimitive@Jet@@QAAX_N@Z");
    a.getDebugRender = Proc<decltype(a.getDebugRender)>(side, "?GetDebugRender@RenderPrimitive@Jet@@QBA_NXZ");
    a.getOpacity = Proc<decltype(a.getOpacity)>(side, "?GetOpacity@RenderPrimitive@Jet@@UBAMXZ");
    a.setOpacity = Proc<decltype(a.setOpacity)>(side, "?SetOpacity@RenderPrimitive@Jet@@UAAXM@Z");
    return a;
}

using CenterFn = Vec3(__cdecl Obj::*)() const;

static CenterFn CenterProc(int side, const char* cls)
{
    FARPROC raw = GetProcAddress(mod[side], M(cls, "?GetGeometryCenter@%s@Jet@@MBA?AVVector3@2@XZ").c_str());
    CenterFn fn{};
    if (raw) std::memcpy(&fn, &raw, sizeof fn);
    return fn;
}

static void DestroyObject(int side, void* object)
{
    using Fn = void(__thiscall*)(void*, int);
    reinterpret_cast<Fn>(*reinterpret_cast<void**>(object))(object, 0);
}

// Particle::InitInstance/copy share the source's bounding-volume pointer; before destroying a clone point it at the
// clone's own embedded volume, as the owning object must.
static void FixOwnVolume(void* object)
{
    void* const embedded = reinterpret_cast<char*>(object) + 0x50;
    std::memcpy(reinterpret_cast<char*>(object) + 0x4c, &embedded, 4);
}

static void RunClass(const ClassDef& cd, int round)
{
    const bool awkward = (round % 7) == 0;
    Api api[2] = {LoadApi(0, cd.name), LoadApi(1, cd.name)};
    alignas(16) unsigned char obj[2][0x200];
    const char* name = cd.name;

    if (!api[0].ctor0 || !api[1].ctor0) return;

    // default constructor
    std::memset(obj[0], 0xa5, sizeof obj[0]);
    std::memset(obj[1], 0xa5, sizeof obj[1]);
    api[0].ctor0(obj[0]);
    api[1].ctor0(obj[1]);
    if (cd.lines) {
        CompareLinesState(obj[0], obj[1], name);
        using Fn = void(__cdecl*)(void*);
        Fn removeAll[2] = {Proc<Fn>(0, "?RemoveAll@RenderLines@Jet@@UAAXXZ"),
                           Proc<Fn>(1, "?RemoveAll@RenderLines@Jet@@UAAXXZ")};
        Fn deleteAll[2] = {Proc<Fn>(0, "?DeleteAll@RenderLines@Jet@@UAAXXZ"),
                           Proc<Fn>(1, "?DeleteAll@RenderLines@Jet@@UAAXXZ")};
        if (removeAll[0] && removeAll[1]) {
            removeAll[0](obj[0]);
            removeAll[1](obj[1]);
            CompareLinesState(obj[0], obj[1], "RenderLines RemoveAll");
        }
        if (deleteAll[0] && deleteAll[1]) {
            deleteAll[0](obj[0]);
            deleteAll[1](obj[1]);
            CompareLinesState(obj[0], obj[1], "RenderLines DeleteAll");
        }
        if (api[0].ctorCopy && api[1].ctorCopy) {
            alignas(16) unsigned char cp[2][0x200];
        std::memset(cp[0], 0xa5, sizeof cp[0]);
        std::memset(cp[1], 0xa5, sizeof cp[1]);
            api[0].ctorCopy(cp[0], obj[0]);
            api[1].ctorCopy(cp[1], obj[1]);
            CompareLinesState(cp[0], cp[1], "RenderLines copy ctor");
            FixOwnVolume(cp[0]);
        FixOwnVolume(cp[1]);
        api[0].dtor(cp[0]);
            api[1].dtor(cp[1]);
        }
        if (api[0].assign && api[1].assign) {
            alignas(16) unsigned char dst[2][0x200];
        std::memset(dst[0], 0xa5, sizeof dst[0]);
        std::memset(dst[1], 0xa5, sizeof dst[1]);
            api[0].ctor0(dst[0]);
            api[1].ctor0(dst[1]);
            api[0].assign(dst[0], obj[0]);
            api[1].assign(dst[1], obj[1]);
            CompareLinesState(dst[0], dst[1], "RenderLines assignment");
            FixOwnVolume(dst[0]);
        FixOwnVolume(dst[1]);
        api[0].dtor(dst[0]);
            api[1].dtor(dst[1]);
        }
        unsigned len0 = 0, len1 = 0;
        CheckVtableLength(0, obj[0], "??_7RenderLines@Jet@@6B@", len0);
        CheckVtableLength(1, obj[1], "??_7RenderLines@Jet@@6B@", len1);
        CheckU32("RenderLines vtable length", len0, len1);
        if (api[0].dtor) api[0].dtor(obj[0]);
        if (api[1].dtor) api[1].dtor(obj[1]);
        return;
    }

    // construction with the chunk-slot count (RenderPrimitive family)
    if (api[0].ctorInt && api[1].ctorInt && cd.fieldCount == 0 && !cd.lines) {
        alignas(16) unsigned char o2[2][0x200];
        std::memset(o2[0], 0xa5, sizeof o2[0]);
        std::memset(o2[1], 0xa5, sizeof o2[1]);
        api[0].ctorInt(o2[0], 0);
        api[1].ctorInt(o2[1], 0);
        char label[128];
        std::snprintf(label, sizeof label, "%s ctor(int)", name);
        ComparePrimitiveState(o2[0], o2[1], cd, label);
        if (api[0].dtor) api[0].dtor(o2[0]);
        if (api[1].dtor) api[1].dtor(o2[1]);
    }

    // setters and getters on the same inputs
    const Vec3 pos = RandomVec(awkward), scale = RandomVec(awkward);
    Vec4 orient{awkward ? Awkward() : FRange(-2.0f, 2.0f), awkward ? Awkward() : FRange(-2.0f, 2.0f),
                awkward ? Awkward() : FRange(-2.0f, 2.0f), awkward ? Awkward() : FRange(-2.0f, 2.0f)};
    const Vec4 color{awkward ? Awkward() : FRange(0.0f, 1.0f), awkward ? Awkward() : FRange(0.0f, 1.0f),
                     awkward ? Awkward() : FRange(0.0f, 1.0f), awkward ? Awkward() : FRange(0.0f, 1.0f)};
    const bool skipVolume = api[0].calculate != nullptr && std::strcmp(name, "RenderPrimitive") != 0;
    for (int side = 0; side < 2; ++side) {
        if (!skipVolume) {
            api[side].setPos(obj[side], &pos);
            api[side].setScale(obj[side], &scale);
            api[side].setOrient(obj[side], &orient);
        }
        api[side].setColor(obj[side], &color);
        api[side].setTestCollision(obj[side], (round & 1) != 0);
        api[side].setDebugRender(obj[side], (round & 2) != 0);
        api[side].setOpacity(obj[side], awkward ? Awkward() : FRange(0.0f, 1.0f));
    }
    char label[160];
    std::snprintf(label, sizeof label, "%s setters", name);
    if (cd.lines)
        CompareLinesState(obj[0], obj[1], label);
    else
        ComparePrimitiveState(obj[0], obj[1], cd, label);
    CheckVec("GetGeometryPosition", *api[0].getPos(obj[0]), *api[1].getPos(obj[1]));
    CheckVec("GetScale", *api[0].getScale(obj[0]), *api[1].getScale(obj[1]));
    CheckBits("GetGeometryOrientation.x", api[0].getOrient(obj[0])->x, api[1].getOrient(obj[1])->x);
    CheckBits("GetGeometryOrientation.w", api[0].getOrient(obj[0])->w, api[1].getOrient(obj[1])->w);
    CheckBits("GetColor.r", api[0].getColor(obj[0])->x, api[1].getColor(obj[1])->x);
    CheckU32("GetCreationFlags", api[0].getCreationFlags(obj[0]), api[1].getCreationFlags(obj[1]));
    CheckU32("GetNumChunks", api[0].getNumChunks(obj[0]), api[1].getNumChunks(obj[1]));
    CheckU32("GetTestCollision", api[0].getTestCollision(obj[0]), api[1].getTestCollision(obj[1]));
    CheckU32("GetDebugRender", api[0].getDebugRender(obj[0]), api[1].getDebugRender(obj[1]));
    CheckBits("GetOpacity", api[0].getOpacity(obj[0]), api[1].getOpacity(obj[1]));

    // shape fields: awkward radius/extents, then the virtual geometry queries
    float fieldVals[8];
    for (unsigned i = 0; i < 8; ++i) fieldVals[i] = awkward ? Awkward() : FRange(0.1f, 50.0f);
    for (int side = 0; side < 2; ++side) {
        for (unsigned i = 0; i < cd.fieldCount; ++i) {
            const unsigned off = cd.fieldOff + (cd.polygonFields ? 4 : 0) + i * 4;
            float v = fieldVals[i];
            if (cd.polygonFields && i == 0) v = 0.0f;
            std::memcpy(reinterpret_cast<char*>(obj[side]) + off, &v, 4);
        }
        if (cd.polygonFields) {
            const int vend = 0;
            std::memcpy(reinterpret_cast<char*>(obj[side]) + 0x19c, &vend, 4);
            const void* nullverts = nullptr;
            std::memcpy(reinterpret_cast<char*>(obj[side]) + 0x1a0, &nullverts, 4);
        }
    }
    std::snprintf(label, sizeof label, "%s fields", name);
    if (cd.lines)
        CompareLinesState(obj[0], obj[1], label);
    else
        ComparePrimitiveState(obj[0], obj[1], cd, label);

    if (api[0].radius && api[1].radius) {
        const float ra = api[0].radius(obj[0]);
        const float rb = api[1].radius(obj[1]);
        std::snprintf(label, sizeof label, "%s GetGeometryRadius", name);
        CheckBits(label, ra, rb);
    }
    CenterFn center[2] = {CenterProc(0, name), CenterProc(1, name)};
    if (center[0] && center[1]) {
        const Vec3 ca = (reinterpret_cast<Obj*>(obj[0])->*center[0])();
        const Vec3 cb = (reinterpret_cast<Obj*>(obj[1])->*center[1])();
        std::snprintf(label, sizeof label, "%s GetGeometryCenter", name);
        CheckVec(label, ca, cb);
    }
    if (api[0].isWithin && api[1].isWithin) {
        const Vec3 p = RandomVec(awkward);
        std::snprintf(label, sizeof label, "%s IsWithin", name);
        Check(api[0].isWithin(obj[0], &p) == api[1].isWithin(obj[1], &p), label);
    }

    // a known kernel pointer, so the copies compare it
    for (int side = 0; side < 2; ++side) {
        const uint32_t sentinel = 0x12345678u;
        std::memcpy(reinterpret_cast<char*>(obj[side]) + 0x13c, &sentinel, 4);
    }

    // copy constructor and assignment share the reference-counted blocks
    if (api[0].ctorCopy && api[1].ctorCopy) {
        alignas(16) unsigned char cp[2][0x200];
        std::memset(cp[0], 0xa5, sizeof cp[0]);
        std::memset(cp[1], 0xa5, sizeof cp[1]);
        api[0].ctorCopy(cp[0], obj[0]);
        api[1].ctorCopy(cp[1], obj[1]);
        std::snprintf(label, sizeof label, "%s copy ctor", name);
        if (cd.lines)
            CompareLinesState(cp[0], cp[1], label);
        else
            ComparePrimitiveState(cp[0], cp[1], cd, label);
        if (!cd.lines) {
            CheckU32("copy shares chunks", U32(cp[0], 0x140) == U32(obj[0], 0x140),
                     U32(cp[1], 0x140) == U32(obj[1], 0x140));
            CheckU32("copy shares materials", U32(cp[0], 0x144) == U32(obj[0], 0x144),
                     U32(cp[1], 0x144) == U32(obj[1], 0x144));
        }
        FixOwnVolume(cp[0]);
        FixOwnVolume(cp[1]);
        api[0].dtor(cp[0]);
        api[1].dtor(cp[1]);
    }
    if (api[0].assign && api[1].assign) {
        alignas(16) unsigned char dst[2][0x200];
        std::memset(dst[0], 0xa5, sizeof dst[0]);
        std::memset(dst[1], 0xa5, sizeof dst[1]);
        api[0].ctor0(dst[0]);
        api[1].ctor0(dst[1]);
        api[0].assign(dst[0], obj[0]);
        api[1].assign(dst[1], obj[1]);
        std::snprintf(label, sizeof label, "%s assignment", name);
        if (cd.lines)
            CompareLinesState(dst[0], dst[1], label);
        else
            ComparePrimitiveState(dst[0], dst[1], cd, label);
        FixOwnVolume(dst[0]);
        FixOwnVolume(dst[1]);
        api[0].dtor(dst[0]);
        api[1].dtor(dst[1]);
    }

    // a clone from CreateInstance
    if (api[0].createInstance && api[1].createInstance) {
        void* const ca = api[0].createInstance(obj[0]);
        void* const cb = api[1].createInstance(obj[1]);
        CheckU32("CreateInstance non-null", ca != nullptr, cb != nullptr);
        if (ca && cb) {
            std::snprintf(label, sizeof label, "%s CreateInstance", name);
            if (cd.lines)
                CompareLinesState(ca, cb, label);
            else
                ComparePrimitiveState(ca, cb, cd, label);
            Check(ca != obj[0] && cb != obj[1], "CreateInstance is a new object");
            using Fn = void(__thiscall*)(void*, int);
            api[0].dtor(ca);
            api[1].dtor(cb);
        }
    }

    // InitInstance into a fresh object
    if (api[0].initInstance && api[1].initInstance && api[0].ctor0 && api[1].ctor0) {
        alignas(16) unsigned char dst[2][0x200];
        std::memset(dst[0], 0xa5, sizeof dst[0]);
        std::memset(dst[1], 0xa5, sizeof dst[1]);
        api[0].ctor0(dst[0]);
        api[1].ctor0(dst[1]);
        api[0].initInstance(obj[0], dst[0]);
        api[1].initInstance(obj[1], dst[1]);
        std::snprintf(label, sizeof label, "%s InitInstance", name);
        if (cd.lines)
            CompareLinesState(dst[0], dst[1], label);
        else
            ComparePrimitiveState(dst[0], dst[1], cd, label);
        api[0].dtor(dst[0]);
        api[1].dtor(dst[1]);
    }

    // shape-specific accessors
    if (std::strcmp(name, "RenderBox") == 0) {
        using Fn = const Vec3* (__cdecl*)(const void*);
        Fn g0 = Need<Fn>(0, "?GetExtents@RenderBox@Jet@@QBAABVVector3@2@XZ");
        Fn g1 = Need<Fn>(1, "?GetExtents@RenderBox@Jet@@QBAABVVector3@2@XZ");
        CheckVec("GetExtents", *g0(obj[0]), *g1(obj[1]));
    }
    if (std::strcmp(name, "RenderSphere") == 0) {
        using Fn = float(__cdecl*)(const void*);
        Fn g0 = Need<Fn>(0, "?GetRadius@RenderSphere@Jet@@QBAMXZ");
        Fn g1 = Need<Fn>(1, "?GetRadius@RenderSphere@Jet@@QBAMXZ");
        CheckBits("GetRadius", g0(obj[0]), g1(obj[1]));
    }

    // vtable layout: same length, and the shared slots resolve to the same exports on each side
    const std::string vtName = std::strcmp(name, "RenderPolygon") == 0
        ? std::string("??_7RenderPolygon@Jet@@6BRender2DPrimitive@1@@")
        : M(name, "??_7%s@Jet@@6B@");
    unsigned len0 = 0, len1 = 0;
    CheckVtableLength(0, obj[0], vtName.c_str(), len0);
    CheckVtableLength(1, obj[1], vtName.c_str(), len1);
    CheckU32("vtable length", len0, len1);
    if (std::strcmp(name, "RenderPolygon") == 0) {
        unsigned pl0 = 0, pl1 = 0;
        CheckVtableLength(0, reinterpret_cast<char*>(obj[0]) + 0x188, "??_7RenderPolygon@Jet@@6BPolygon@1@@", pl0);
        CheckVtableLength(1, reinterpret_cast<char*>(obj[1]) + 0x188, "??_7RenderPolygon@Jet@@6BPolygon@1@@", pl1);        CheckU32("polygon vtable length", pl0, pl1);
    }
    CheckVtableSlot(0, obj[0], 0x0c, "?Cull@RenderPrimitive@Jet@@UAAXAAVViewPoint@2@ABVMatrix4@2@@Z", "Cull slot");
    CheckVtableSlot(1, obj[1], 0x0c, "?Cull@RenderPrimitive@Jet@@UAAXAAVViewPoint@2@ABVMatrix4@2@@Z", "orig Cull slot");
    CheckVtableSlot(0, obj[0], 0x3c, "?GetOpacity@RenderPrimitive@Jet@@UBAMXZ", "GetOpacity slot");
    CheckVtableSlot(1, obj[1], 0x3c, "?GetOpacity@RenderPrimitive@Jet@@UBAMXZ", "orig GetOpacity slot");
    CheckVtableSlot(0, obj[0], 0xc0, "?SetScale@RenderPrimitive@Jet@@UAAXABVVector3@2@@Z", "SetScale slot");
    CheckVtableSlot(1, obj[1], 0xc0, "?SetScale@RenderPrimitive@Jet@@UAAXABVVector3@2@@Z", "orig SetScale slot");
    CheckVtableSlot(1, obj[1], 0xd8, "?Init@RenderPrimitive@Jet@@MAA_NPAVRenderKernel@2@ABVColor@2@H@Z", "orig Init slot");
    if (api[0].radius) {
        char nm[256];
        std::snprintf(nm, sizeof nm, "?GetGeometryRadius@%s@Jet@@MBAMXZ", name);
        CheckVtableSlot(0, obj[0], 0xdc, nm, "radius slot");
        CheckVtableSlot(1, obj[1], 0xdc, nm, "orig radius slot");
    }

    // base CalculatePrimitiveVolume drives the particle's embedded bounding volume
    if (!skipVolume && api[0].calculate && api[1].calculate) {
        api[0].calculate(obj[0]);
        api[1].calculate(obj[1]);
        using FnGet = const void* (__cdecl*)(const void*, bool);
        FnGet gs0 = Proc<FnGet>(0, "?GetBoundingSphere@Particle@Jet@@QBAABVSphere@2@_N@Z");
        FnGet gs1 = Proc<FnGet>(1, "?GetBoundingSphere@Particle@Jet@@QBAABVSphere@2@_N@Z");
        if (gs0 && gs1) {
            const void* const s0 = gs0(obj[0], false);
            const void* const s1 = gs1(obj[1], false);
            Check(s0 && s1, "bounding sphere non-null");
            if (s0 && s1) {
                bool same = true;
                for (unsigned off = 4; off < 0x14; ++off)
                    if (static_cast<const unsigned char*>(s0)[off] != static_cast<const unsigned char*>(s1)[off])
                        same = false;
                Check(same, "CalculatePrimitiveVolume");
            }
        }
    }

    if (api[0].dtor) api[0].dtor(obj[0]);
    if (api[1].dtor) api[1].dtor(obj[1]);
}

static void RunLinesStatics(int round)
{
    using FnSet = void(__cdecl*)(bool);
    using FnGet = bool(__cdecl*)(void);
    FnSet set[2] = {Need<FnSet>(0, "?SetState@RenderLines@Jet@@SAX_N@Z"),
                    Need<FnSet>(1, "?SetState@RenderLines@Jet@@SAX_N@Z")};
    FnGet get[2] = {Need<FnGet>(0, "?GetState@RenderLines@Jet@@SA_NXZ"),
                    Need<FnGet>(1, "?GetState@RenderLines@Jet@@SA_NXZ")};
    FnGet sw[2] = {Need<FnGet>(0, "?Switch@RenderLines@Jet@@SA_NXZ"),
                   Need<FnGet>(1, "?Switch@RenderLines@Jet@@SA_NXZ")};
    const bool state = (round & 1) != 0;
    for (int side = 0; side < 2; ++side) set[side](state);
    Check(get[0]() == get[1](), "RenderLines GetState");
    Check(sw[0]() == sw[1](), "RenderLines Switch return");
    Check(get[0]() == get[1](), "RenderLines state after Switch");
    for (int side = 0; side < 2; ++side) set[side](false);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u10 <bin dir> [rounds]\n");
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

    for (int round = 0; round < rounds; ++round) {
        for (const ClassDef& cd : classes) RunClass(cd, round);
        RunLinesStatics(round);
    }

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
