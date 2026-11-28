// Differential test of Render3DCore_7c unit U9a: our Particle/ViewPortParticle/ParticleStructure against
// Render3DCore_7c_orig.dll in one process.
//
// Both modules work on the same object addresses (the buffers are reused between the two runs), so the raw object
// bytes, including the self pointers in the embedded bounding volume and the child/sibling links, can be compared
// directly. The only excluded bytes are the object's own vftable pointer. Every method is called through
// GetProcAddress on identical inputs; the awkward rounds include signed zero, infinities and quiet/signalling NaNs.
//
// usage: diff_r3d_u9a.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cmath>
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
struct Plane {
    Vec3 normal;
    float d;
};
struct Sphere {
    void* vtable;
    Vec3 center;
    float radius;
};
struct Mat3 {
    float m[9];
};
struct Mat4 {
    float m[16];
};
struct Box {
    Vec3 min, max;
};
struct OBox {
    Vec3 center;
    Vec3 axis[3];
    Vec3 extents;
};
struct PS {
    void* node;
};
struct It {
    void* node;
};
struct RefPoint {
    void* name;
    Vec3 position;
    Vec4 orientation;
};
struct RefChunk {
    RefChunk* next;
    RefChunk* prev;
    uint32_t count;
    RefPoint data[3];
};

static_assert(sizeof(Vec2) == 8 && sizeof(Vec3) == 0xc && sizeof(Vec4) == 0x10, "vectors");
static_assert(sizeof(Plane) == 0x10 && sizeof(Sphere) == 0x14 && sizeof(Mat3) == 0x24 && sizeof(Mat4) == 0x40, "math");
static_assert(sizeof(Box) == 0x18 && sizeof(OBox) == 0x3c && sizeof(PS) == 4 && sizeof(It) == 4, "objects");
static_assert(sizeof(RefPoint) == 0x20, "RefPoint");

static HMODULE mod[2];
static void* pstringCache;
static void* (*CreateNode)(void*, const unsigned char*);
static unsigned long long checks, bad;
static std::mt19937 rng(0x9a5eed);

static const unsigned kObjSize = 0x12c;
static const unsigned kViewSize = 0x13c;
static const unsigned kStructSize = 0x140;

static unsigned char buffer[kObjSize + 0x100];
static unsigned char nodes[10][kStructSize + 0x40];
static unsigned char snapOurs[sizeof nodes + sizeof buffer];
static unsigned char snapOrig[sizeof nodes + sizeof buffer];

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
    default: return FRange(-100.0f, 100.0f);
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
    Vec4 q{Val(awkward), Val(awkward), Val(awkward), Val(awkward)};
    if (!awkward && Pick(0, 3) != 0) {
        const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        if (len > 0.01f) {
            q.x /= len;
            q.y /= len;
            q.z /= len;
            q.w /= len;
        }
    }
    return q;
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

static uint32_t FBits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

static bool SameBytes(const void* a, const void* b, size_t n)
{
    return std::memcmp(a, b, n) == 0;
}

static bool IsImagePointer(int side, uint32_t v)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(mod[side]);
    const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return v >= base && v < base + nt->OptionalHeader.SizeOfImage;
}

// compare an object; words that point into either module (the object's vftable and the embedded volume's sphere
// vftable) are module specific, everything else is compared raw
static void CheckObj(const void* a, const void* b, size_t size, const char* what)
{
    const unsigned char* x = static_cast<const unsigned char*>(a);
    const unsigned char* y = static_cast<const unsigned char*>(b);
    bool ok = true;
    for (size_t i = 0; i + 4 <= size; i += 4) {
        uint32_t u, v;
        std::memcpy(&u, x + i, 4);
        std::memcpy(&v, y + i, 4);
        if (u == v) continue;
        if (IsImagePointer(0, u) && IsImagePointer(1, v)) continue;
        std::printf("%s first diff +0x%x ours=%08x orig=%08x\n", what, static_cast<unsigned>(i), u, v);
        ok = false;
        break;
    }
    Check(ok, what);
}

static bool IsNaNBits(uint32_t u)
{
    return (u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) != 0;
}

static void CheckBytes(const void* a, const void* b, size_t n, const char* what)
{
    const unsigned char* x = static_cast<const unsigned char*>(a);
    const unsigned char* y = static_cast<const unsigned char*>(b);
    bool ok = true;
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        uint32_t u, v;
        std::memcpy(&u, x + i, 4);
        std::memcpy(&v, y + i, 4);
        if (u == v) continue;
        if (IsImagePointer(0, u) && IsImagePointer(1, v)) continue;
        if (IsNaNBits(u) && IsNaNBits(v)) continue;
        std::printf("%s first diff +0x%x ours=%08x orig=%08x\n", what, static_cast<unsigned>(i), u, v);
        ok = false;
        break;
    }
    if (ok) ok = std::memcmp(x + i, y + i, n - i) == 0;
    Check(ok, what);
}

static void* Node(int side, int index)
{
    (void)side;
    return nodes[index];
}

static PS Name(int index)
{
    static void* made[8];
    static bool init = false;
    if (!init) {
        const char* texts[8] = {"", "alpha", "beta", "gamma", "delta", "host", "leaf", "point"};
        for (int i = 0; i < 8; ++i)
            made[i] = CreateNode(pstringCache, reinterpret_cast<const unsigned char*>(texts[i]));
        init = true;
    }
    PS p;
    p.node = made[index];
    return p;
}

// the tree chunk of FastDynamicArray<...,21>, used to read EnumerateParticles output
struct FdaChunk {
    FdaChunk* next;
    FdaChunk* prev;
    uint32_t count;
    void* data[21];
};
struct Fda {
    FdaChunk* head;
};

static int FdaCount(const Fda& a)
{
    int n = 0;
    FdaChunk* c = a.head;
    if (!c) return 0;
    do {
        n += static_cast<int>(c->count);
        c = c->next;
    } while (c != a.head);
    return n;
}

static void FdaItems(const Fda& a, std::vector<void*>& out)
{
    out.clear();
    FdaChunk* c = a.head;
    if (!c) return;
    do {
        for (uint32_t i = 0; i < c->count; ++i) out.push_back(c->data[i]);
        c = c->next;
    } while (c != a.head);
}

// one stand-in PlaneList chunk (DynamicArray<Plane,16>) for IsWithin / GetAllWithin
struct PlaneChunk {
    PlaneChunk* next;
    PlaneChunk* prev;
    uint32_t count;
    Plane data[16];
};

struct Api {
    void(__thiscall* ctor0)(void*);
    void(__thiscall* ctorCopy)(void*, const void*);
    void*(__thiscall* assign)(void*, const void*);
    void(__thiscall* dtor)(void*);
    void(__cdecl* setName)(void*, const PS&);
    void(__cdecl* setPosition)(void*, const Vec3&);
    void(__cdecl* setOrientation)(void*, const Vec4&);
    void(__cdecl* setReferenceFrame)(void*, const Vec3&, const Vec4&);
    void(__cdecl* setPositionXY)(void*, const Vec2&);
    void(__cdecl* setEnabled)(void*, bool);
    void(__cdecl* setCullSizeCheck)(void*, bool);
    void(__cdecl* add)(void*, void*);
    bool(__cdecl* addNamed)(void*, const PS&, void*);
    void(__cdecl* push)(void*, void*);
    void(__cdecl* insertBefore)(void*, It*, It&, void*);
    void(__cdecl* insertAfter)(void*, It*, It&, void*);
    void*(__cdecl* removeLast)(void*);
    bool(__cdecl* removeChild)(void*, void*);
    void*(__cdecl* removeNamed)(void*, const PS&);
    void(__cdecl* removeIter)(void*, It*, It&);
    void*(__cdecl* pop)(void*);
    void(__cdecl* removeAll)(void*);
    void(__cdecl* removeAllChildren)(void*);
    void(__cdecl* deleteAll)(void*);
    void(__cdecl* setParent)(void*, void*);
    void*(__cdecl* findByName)(void*, const PS&);
    bool(__cdecl* enumerate)(void*, Fda*);
    void(__cdecl* cull)(void*, void*, const Mat4*);
    void(__cdecl* renderDebug)(void*, void*, void*, void*, unsigned long);
    bool(__cdecl* isWithin)(const void*, const PlaneChunk*);
    bool(__cdecl* getAllWithinPlanes)(void*, const PlaneChunk*, Fda*);
    bool(__cdecl* getAllWithinPlanesConst)(const void*, const PlaneChunk*, Fda*);
    bool(__cdecl* getAllWithinSphere)(void*, const Sphere*, Fda*);
    bool(__cdecl* getAllWithinSphereConst)(const void*, const Sphere*, Fda*);
    Vec3* (__cdecl* getPosition)(const void*);
    Vec4* (__cdecl* getOrientation)(const void*);
    void(__cdecl* getPositionXY)(const void*, Vec2*);
    bool(__cdecl* getEnabled)(const void*);
    bool(__cdecl* getCullSizeCheck)(const void*);
    Sphere* (__cdecl* getBoundingSphere)(const void*, bool);
    Box* (__cdecl* getAxisAlignedBox)(const void*, bool);
    OBox* (__cdecl* getOrientedBox)(const void*, bool);
    void(__cdecl* getGlobalSphere)(const void*, Sphere*, bool);
    Vec3* (__cdecl* getGlobalSphereCenter)(const void*, bool);
    void(__cdecl* localToGlobalVec)(const void*, Vec3*);
    void(__cdecl* localToGlobalQuat)(const void*, Vec4*);
    void(__cdecl* localToGlobalPlane)(const void*, Plane*);
    void(__cdecl* localToGlobalBox)(const void*, OBox*);
    void(__cdecl* localToGlobalBoth)(const void*, Vec3*, Vec4*);
    void(__cdecl* globalToLocalVec)(const void*, Vec3*);
    void(__cdecl* globalToLocalQuat)(const void*, Vec4*);
    void(__cdecl* globalToLocalPlane)(const void*, Plane*);
    void(__cdecl* globalToLocalBox)(const void*, OBox*);
    void(__cdecl* globalToLocalBoth)(const void*, Vec3*, Vec4*);
    void(__cdecl* localToLocalVec)(const void*, const void*, Vec3*);
    void(__cdecl* localToLocalQuat)(const void*, const void*, Vec4*);
    void(__cdecl* localToLocalPlane)(const void*, const void*, Plane*);
    void(__cdecl* localToLocalBox)(const void*, const void*, OBox*);
    void(__cdecl* localToLocalBoth)(const void*, const void*, Vec3*, Vec4*);
    void(__cdecl* translate)(void*, const Vec3&);
    void(__cdecl* rotateQuat)(void*, const Vec4&);
    void(__cdecl* rotateMat)(void*, const Mat3&);
    void(__cdecl* selfRotateQuat)(void*, const Vec4&);
    void(__cdecl* pointXAt)(void*, const Vec3&, const Vec3&);
    void(__cdecl* getModelView)(const void*, Mat4*, const Mat4*);
    void(__thiscall* vpCtor0)(void*);
    void(__thiscall* vpCtorCopy)(void*, const void*);
    void*(__thiscall* vpAssign)(void*, const void*);
    void(__thiscall* vpDtor)(void*);
    void(__cdecl* vpAddAlias)(void*, void*);
    unsigned long(__cdecl* vpGetRenderFlags)(const void*);
    void(__cdecl* vpSetRenderFlags)(void*, unsigned long);
    void(__thiscall* aliasCtor)(void*, void*);
    void(__thiscall* aliasDtor)(void*);
    void(__cdecl* aliasSetAliasTo)(void*, void*);
    void*(__cdecl* aliasGetAliasTo)(const void*);
    int(__cdecl* aliasGetID)(const void*);
    void(__thiscall* stCtor0)(void*);
    void(__thiscall* stCtorCopy)(void*, const void*);
    void*(__thiscall* stAssign)(void*, const void*);
    void(__thiscall* stDtor)(void*);
    void(__cdecl* stAddReferencePoint)(void*, const RefPoint&);
    void*(__cdecl* stGetReferencePointList)(void*);
    bool(__cdecl* stAddAttachment)(void*, void*, bool);
    void* (__cdecl* createInstance)(const void*);
    bool(__cdecl* addNamed2)(void*, const PS&, void*);
    int(__cdecl* getID)(const void*);
    bool(__cdecl* isOfType)(const void*, int);
};

struct Scenario {
    Vec3 pos, targetPos;
    Vec4 rot, targetRot;
    Vec3 delta;
    Vec4 rot2;
    Mat3 m3;
    Mat4 view;
    Vec3 v;
    Vec4 q;
    Plane pl;
    OBox ob;
    bool enabled, cull;
};

static Scenario sc;

static void MakeScenario(bool awkward)
{
    sc.pos = RandomVec(awkward);
    sc.targetPos = RandomVec(awkward);
    sc.rot = RandomQuat(awkward);
    sc.targetRot = RandomQuat(awkward);
    sc.delta = RandomVec(awkward);
    sc.rot2 = RandomQuat(awkward);
    for (int i = 0; i < 9; ++i) sc.m3.m[i] = Val(awkward);
    for (int i = 0; i < 16; ++i) sc.view.m[i] = Val(awkward);
    sc.v = RandomVec(awkward);
    sc.q = RandomQuat(awkward);
    sc.pl.normal = RandomVec(awkward);
    sc.pl.d = Val(awkward);
    std::memset(&sc.ob, 0, sizeof sc.ob);
    sc.ob.center = RandomVec(awkward);
    for (int i = 0; i < 3; ++i) sc.ob.axis[i] = RandomVec(awkward);
    sc.ob.extents = RandomVec(awkward);
    sc.enabled = Pick(0, 1) != 0;
    sc.cull = Pick(0, 1) != 0;
}

static Api a[2];

static void LoadApi(int s)
{
    Api& q = a[s];
    q.ctor0 = Proc<void(__thiscall*)(void*)>(s, "??0Particle@Jet@@QAE@XZ");
    q.ctorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0Particle@Jet@@QAE@ABV01@@Z");
    q.assign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4Particle@Jet@@QAEAAV01@ABV01@@Z");
    q.dtor = Proc<void(__thiscall*)(void*)>(s, "??1Particle@Jet@@UAE@XZ");
    q.setName = Proc<void(__cdecl*)(void*, const PS&)>(s, "?SetName@Particle@Jet@@UAAXABVPString@2@@Z");
    q.setPosition = Proc<void(__cdecl*)(void*, const Vec3&)>(s, "?SetPosition@Particle@Jet@@UAAXABVVector3@2@@Z");
    q.setOrientation =
        Proc<void(__cdecl*)(void*, const Vec4&)>(s, "?SetOrientation@Particle@Jet@@UAAXABVQuaternion@2@@Z");
    q.setReferenceFrame = Proc<void(__cdecl*)(void*, const Vec3&, const Vec4&)>(
        s, "?SetReferenceFrame@Particle@Jet@@UAAXABVVector3@2@ABVQuaternion@2@@Z");
    q.setPositionXY =
        Proc<void(__cdecl*)(void*, const Vec2&)>(s, "?SetPositionXY@Particle@Jet@@UAAXABVVector2@2@@Z");
    q.setEnabled = Proc<void(__cdecl*)(void*, bool)>(s, "?SetEnabled@Particle@Jet@@QAAX_N@Z");
    q.setCullSizeCheck = Proc<void(__cdecl*)(void*, bool)>(s, "?SetCullSizeCheck@Particle@Jet@@QAAX_N@Z");
    q.add = Proc<void(__cdecl*)(void*, void*)>(s, "?Add@Particle@Jet@@UAAXPAV12@@Z");
    q.addNamed =
        Proc<bool(__cdecl*)(void*, const PS&, void*)>(s, "?Add@Particle@Jet@@UAA_NABVPString@2@PAV12@@Z");
    q.push = Proc<void(__cdecl*)(void*, void*)>(s, "?Push@Particle@Jet@@UAAXPAV12@@Z");
    q.insertBefore = Proc<void(__cdecl*)(void*, It*, It&, void*)>(
        s, "?InsertBefore@Particle@Jet@@UAA?AVIterator@?$ITree@VParticle@Jet@@@2@AAV342@PAV12@@Z");
    q.insertAfter = Proc<void(__cdecl*)(void*, It*, It&, void*)>(
        s, "?InsertAfter@Particle@Jet@@UAA?AVIterator@?$ITree@VParticle@Jet@@@2@AAV342@PAV12@@Z");
    q.removeLast = Proc<void*(__cdecl*)(void*)>(s, "?Remove@Particle@Jet@@UAAPAV12@XZ");
    q.removeChild = Proc<bool(__cdecl*)(void*, void*)>(s, "?Remove@Particle@Jet@@UAA_NPAV12@@Z");
    q.removeNamed = Proc<void*(__cdecl*)(void*, const PS&)>(s, "?Remove@Particle@Jet@@UAAPAV12@ABVPString@2@@Z");
    q.removeIter = Proc<void(__cdecl*)(void*, It*, It&)>(
        s, "?Remove@Particle@Jet@@UAA?AVIterator@?$ITree@VParticle@Jet@@@2@AAV342@@Z");
    q.pop = Proc<void*(__cdecl*)(void*)>(s, "?Pop@Particle@Jet@@UAAPAV12@XZ");
    q.removeAll = Proc<void(__cdecl*)(void*)>(s, "?RemoveAll@Particle@Jet@@UAAXXZ");
    q.removeAllChildren = Proc<void(__cdecl*)(void*)>(s, "?RemoveAllChildren@Particle@Jet@@UAAXXZ");
    q.deleteAll = Proc<void(__cdecl*)(void*)>(s, "?DeleteAll@Particle@Jet@@UAAXXZ");
    q.setParent = Proc<void(__cdecl*)(void*, void*)>(s, "?SetParent@Particle@Jet@@UAAXPAV12@@Z");
    q.findByName =
        Proc<void*(__cdecl*)(void*, const PS&)>(s, "?FindByName@Particle@Jet@@QAAPAV12@ABVPString@2@@Z");
    q.cull = Proc<void(__cdecl*)(void*, void*, const Mat4*)>(
        s, "?Cull@Particle@Jet@@UAAXAAVViewPoint@2@ABVMatrix4@2@@Z");
    q.renderDebug = Proc<void(__cdecl*)(void*, void*, void*, void*, unsigned long)>(
        s, "?RenderDebug@Particle@Jet@@UAAXPAVDisplay@2@AAVDebugViewPort@2@AAVMatrix4@2@K@Z");
    q.isWithin = Proc<bool(__cdecl*)(const void*, const PlaneChunk*)>(
        s, "?IsWithin@Particle@Jet@@UBA_NABV?$DynamicArray@VPlane@Jet@@$0BA@U?$Less@VPlane@Jet@@V12@@2@@2@@Z");
    q.getAllWithinPlanes = Proc<bool(__cdecl*)(void*, const PlaneChunk*, Fda*)>(
        s,
        "?GetAllWithin@Particle@Jet@@UAA_NABV?$DynamicArray@VPlane@Jet@@$0BA@U?$Less@VPlane@Jet@@V12@@2@@2@AAV?$"
        "FastDynamicArray@PAVParticle@Jet@@$0BF@U?$Less@PAVParticle@Jet@@PAV12@@2@@2@@Z");
    q.getAllWithinPlanesConst = Proc<bool(__cdecl*)(const void*, const PlaneChunk*, Fda*)>(
        s,
        "?GetAllWithin@Particle@Jet@@UBA_NABV?$DynamicArray@VPlane@Jet@@$0BA@U?$Less@VPlane@Jet@@V12@@2@@2@AAV?$"
        "FastDynamicArray@PBVParticle@Jet@@$0BF@U?$Less@PBVParticle@Jet@@PBV12@@2@@2@@Z");
    q.getAllWithinSphere = Proc<bool(__cdecl*)(void*, const Sphere*, Fda*)>(
        s,
        "?GetAllWithin@Particle@Jet@@UAA_NABVSphere@2@AAV?$FastDynamicArray@PAVParticle@Jet@@$0BF@U?$Less@PAVParticle@"
        "Jet@@PAV12@@2@@2@@Z");
    q.getAllWithinSphereConst = Proc<bool(__cdecl*)(const void*, const Sphere*, Fda*)>(
        s,
        "?GetAllWithin@Particle@Jet@@UBA_NABVSphere@2@AAV?$FastDynamicArray@PBVParticle@Jet@@$0BF@U?$Less@PBVParticle@"
        "Jet@@PBV12@@2@@2@@Z");
    q.enumerate = Proc<bool(__cdecl*)(void*, Fda*)>(
        s,
        "?EnumerateParticles@Particle@Jet@@QAA_NAAV?$FastDynamicArray@PAVParticle@Jet@@$0BF@U?$Less@PAVParticle@Jet@@"
        "PAV12@@2@@2@@Z");
    q.getPosition = Proc<Vec3* (__cdecl*)(const void*)>(s, "?GetPosition@Particle@Jet@@QBAABVVector3@2@XZ");
    q.getOrientation = Proc<Vec4* (__cdecl*)(const void*)>(s, "?GetOrientation@Particle@Jet@@QBAABVQuaternion@2@XZ");
    q.getPositionXY = Proc<void(__cdecl*)(const void*, Vec2*)>(s, "?GetPositionXY@Particle@Jet@@QBA?AVVector2@2@XZ");
    q.getEnabled = Proc<bool(__cdecl*)(const void*)>(s, "?GetEnabled@Particle@Jet@@QBA_NXZ");
    q.getCullSizeCheck = Proc<bool(__cdecl*)(const void*)>(s, "?GetCullSizeCheck@Particle@Jet@@QBA_NXZ");
    q.getBoundingSphere =
        Proc<Sphere* (__cdecl*)(const void*, bool)>(s, "?GetBoundingSphere@Particle@Jet@@QBAABVSphere@2@_N@Z");
    q.getAxisAlignedBox = Proc<Box* (__cdecl*)(const void*, bool)>(
        s, "?GetAxisAlignedBoundingBox@Particle@Jet@@QBAABVAxisAlignedBox@2@_N@Z");
    q.getOrientedBox = Proc<OBox* (__cdecl*)(const void*, bool)>(
        s, "?GetOrientedBoundingBox@Particle@Jet@@QBAABVOrientedBox@2@_N@Z");
    q.getGlobalSphere = Proc<void(__cdecl*)(const void*, Sphere*, bool)>(
        s, "?GetGlobalBoundingSphere@Particle@Jet@@QBA?AVSphere@2@_N@Z");
    q.getGlobalSphereCenter = Proc<Vec3* (__cdecl*)(const void*, bool)>(
        s, "?GetGlobalBoundingSphereCenter@Particle@Jet@@QBAABVVector3@2@_N@Z");
    q.localToGlobalVec =
        Proc<void(__cdecl*)(const void*, Vec3*)>(s, "?LocalToGlobal@Particle@Jet@@QBAXAAVVector3@2@@Z");
    q.localToGlobalQuat =
        Proc<void(__cdecl*)(const void*, Vec4*)>(s, "?LocalToGlobal@Particle@Jet@@QBAXAAVQuaternion@2@@Z");
    q.localToGlobalPlane =
        Proc<void(__cdecl*)(const void*, Plane*)>(s, "?LocalToGlobal@Particle@Jet@@QBAXAAVPlane@2@@Z");
    q.localToGlobalBox =
        Proc<void(__cdecl*)(const void*, OBox*)>(s, "?LocalToGlobal@Particle@Jet@@QBAXAAVOrientedBox@2@@Z");
    q.localToGlobalBoth = Proc<void(__cdecl*)(const void*, Vec3*, Vec4*)>(
        s, "?LocalToGlobal@Particle@Jet@@QBAXAAVVector3@2@AAVQuaternion@2@@Z");
    q.globalToLocalVec =
        Proc<void(__cdecl*)(const void*, Vec3*)>(s, "?GlobalToLocal@Particle@Jet@@QBAXAAVVector3@2@@Z");
    q.globalToLocalQuat =
        Proc<void(__cdecl*)(const void*, Vec4*)>(s, "?GlobalToLocal@Particle@Jet@@QBAXAAVQuaternion@2@@Z");
    q.globalToLocalPlane =
        Proc<void(__cdecl*)(const void*, Plane*)>(s, "?GlobalToLocal@Particle@Jet@@QBAXAAVPlane@2@@Z");
    q.globalToLocalBox =
        Proc<void(__cdecl*)(const void*, OBox*)>(s, "?GlobalToLocal@Particle@Jet@@QBAXAAVOrientedBox@2@@Z");
    q.globalToLocalBoth = Proc<void(__cdecl*)(const void*, Vec3*, Vec4*)>(
        s, "?GlobalToLocal@Particle@Jet@@QBAXAAVVector3@2@AAVQuaternion@2@@Z");
    q.localToLocalVec = Proc<void(__cdecl*)(const void*, const void*, Vec3*)>(
        s, "?LocalToLocal@Particle@Jet@@QBAXPBV12@AAVVector3@2@@Z");
    q.localToLocalQuat = Proc<void(__cdecl*)(const void*, const void*, Vec4*)>(
        s, "?LocalToLocal@Particle@Jet@@QBAXPBV12@AAVQuaternion@2@@Z");
    q.localToLocalPlane = Proc<void(__cdecl*)(const void*, const void*, Plane*)>(
        s, "?LocalToLocal@Particle@Jet@@QBAXPBV12@AAVPlane@2@@Z");
    q.localToLocalBox = Proc<void(__cdecl*)(const void*, const void*, OBox*)>(
        s, "?LocalToLocal@Particle@Jet@@QBAXPBV12@AAVOrientedBox@2@@Z");
    q.localToLocalBoth = Proc<void(__cdecl*)(const void*, const void*, Vec3*, Vec4*)>(
        s, "?LocalToLocal@Particle@Jet@@QBAXPBV12@AAVVector3@2@AAVQuaternion@2@@Z");
    q.translate = Proc<void(__cdecl*)(void*, const Vec3&)>(s, "?Translate@Particle@Jet@@QAAXABVVector3@2@@Z");
    q.rotateQuat =
        Proc<void(__cdecl*)(void*, const Vec4&)>(s, "?Rotate@Particle@Jet@@QAAXABVQuaternion@2@@Z");
    q.rotateMat = Proc<void(__cdecl*)(void*, const Mat3&)>(s, "?Rotate@Particle@Jet@@QAAXABVMatrix3@2@@Z");
    q.selfRotateQuat =
        Proc<void(__cdecl*)(void*, const Vec4&)>(s, "?SelfRotate@Particle@Jet@@QAAXABVQuaternion@2@@Z");
    q.pointXAt = Proc<void(__cdecl*)(void*, const Vec3&, const Vec3&)>(
        s, "?PointXAt@Particle@Jet@@QAAXABVVector3@2@0@Z");
    q.getModelView = Proc<void(__cdecl*)(const void*, Mat4*, const Mat4*)>(
        s, "?GetModelView@Particle@Jet@@QBAXAAVMatrix4@2@ABV32@@Z");
    q.vpCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0ViewPortParticle@Jet@@QAE@XZ");
    q.vpCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0ViewPortParticle@Jet@@QAE@ABV01@@Z");
    q.vpAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4ViewPortParticle@Jet@@QAEAAV01@ABV01@@Z");
    q.vpDtor = Proc<void(__thiscall*)(void*)>(s, "??1ViewPortParticle@Jet@@UAE@XZ");
    q.vpAddAlias = Proc<void(__cdecl*)(void*, void*)>(s, "?AddAlias@ViewPortParticle@Jet@@QAAXPAVParticleAlias@2@@Z");
    q.vpGetRenderFlags = Proc<unsigned long(__cdecl*)(const void*)>(s, "?GetRenderFlags@ViewPortParticle@Jet@@QBAKXZ");
    q.vpSetRenderFlags = Proc<void(__cdecl*)(void*, unsigned long)>(s, "?SetRenderFlags@ViewPortParticle@Jet@@QAAXK@Z");
    q.aliasCtor = Proc<void(__thiscall*)(void*, void*)>(s, "??0ParticleAlias@Jet@@QAE@PAVViewPortParticle@1@@Z");
    q.aliasDtor = Proc<void(__thiscall*)(void*)>(s, "??1ParticleAlias@Jet@@UAE@XZ");
    q.aliasSetAliasTo = Proc<void(__cdecl*)(void*, void*)>(s, "?SetAliasTo@ParticleAlias@Jet@@QAAXPAVViewPortParticle@2@@Z");
    q.aliasGetAliasTo = Proc<void* (__cdecl*)(const void*)>(s, "?GetAliasTo@ParticleAlias@Jet@@QBAPAVViewPortParticle@2@XZ");
    q.aliasGetID = Proc<int(__cdecl*)(const void*)>(s, "?GetID@ParticleAlias@Jet@@UBAHXZ");
    q.stCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0ParticleStructure@Jet@@QAE@XZ");
    q.stCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0ParticleStructure@Jet@@QAE@ABV01@@Z");
    q.stAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4ParticleStructure@Jet@@QAEAAV01@ABV01@@Z");
    q.stDtor = Proc<void(__thiscall*)(void*)>(s, "??1ParticleStructure@Jet@@UAE@XZ");
    q.stAddReferencePoint = Proc<void(__cdecl*)(void*, const RefPoint&)>(s, "?AddReferencePoint@ParticleStructure@Jet@@QAAXABVReferencePoint@2@@Z");
    q.stGetReferencePointList = Proc<void* (__cdecl*)(void*)>(s, "?GetReferencePointList@ParticleStructure@Jet@@QAAAAV?$DynamicArray@VReferencePoint@Jet@@$02U?$Less@VReferencePoint@Jet@@V12@@2@@2@XZ");
    q.stAddAttachment = Proc<bool(__cdecl*)(void*, void*, bool)>(s, "?AddAttachmentPoint@ParticleStructure@Jet@@QAA_NPAVAttachmentPoint@2@_N@Z");
    q.createInstance = Proc<void* (__cdecl*)(const void*)>(s, "?CreateInstance@Particle@Jet@@UBAPAV12@XZ");
    q.addNamed2 = Proc<bool(__cdecl*)(void*, const PS&, void*)>(s, "?Add@Particle@Jet@@UAA_NABVPString@2@PAV12@@Z");
    q.getID = Proc<int(__cdecl*)(const void*)>(s, "?GetID@Particle@Jet@@UBAHXZ");
    q.isOfType = Proc<bool(__cdecl*)(const void*, int)>(s, "?IsOfType@Particle@Jet@@UBA_NH@Z");
}

static void ZeroObjects(int count = 1)
{
    for (int i = 0; i < count; ++i) std::memset(nodes[i], 0, kStructSize + 0x40);
}

static void RunCtor(int s, bool awkward)
{
    ZeroObjects();
    a[s].ctor0(nodes[0]);
    if (s == 0) {
        std::memcpy(snapOurs, nodes[0], kObjSize);
        Check(*reinterpret_cast<void**>(nodes[0] + 0x4c) == nodes[0] + 0x50, "ctor bv self pointer");
    } else {
        std::memcpy(snapOrig, nodes[0], kObjSize);
        Check(*reinterpret_cast<void**>(nodes[0] + 0x4c) == nodes[0] + 0x50, "orig ctor bv self pointer");
    }
    a[s].dtor(nodes[0]);
    if (s == 0)
        std::memcpy(snapOurs + sizeof buffer, nodes[0], kObjSize);
    else
        std::memcpy(snapOrig + sizeof buffer, nodes[0], kObjSize);
    (void)awkward;
}

static void RunMethods(int s, bool awkward)
{
    void* o = nodes[0];
    ZeroObjects(2);
    a[s].ctor0(o);
    // a clean volume lets the setters run their centre delta; a fresh object has every dirty bit set
    *reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(o) + 0x54) = 0;
    const PS nm = Name(Pick(1, 7) >= 1 ? 1 : 0);
    a[s].setName(o, nm);
    a[s].setPosition(o, sc.pos);
    a[s].setOrientation(o, sc.rot);
    a[s].setEnabled(o, sc.enabled);
    a[s].setCullSizeCheck(o, sc.cull);
    const bool enabled = a[s].getEnabled(o);
    const bool cull = a[s].getCullSizeCheck(o);
    Vec3 gotPos;
    Vec4 gotRot;
    Vec2 gotXY;
    gotPos = *a[s].getPosition(o);
    gotRot = *a[s].getOrientation(o);
    a[s].getPositionXY(o, &gotXY);
    if (s == 0) {
        std::memcpy(snapOurs, nodes[0], kObjSize);
        std::memcpy(snapOurs + sizeof buffer, &gotPos, sizeof gotPos);
        std::memcpy(snapOurs + sizeof buffer + 0x10, &gotRot, sizeof gotRot);
        std::memcpy(snapOurs + sizeof buffer + 0x20, &gotXY, sizeof gotXY);
        std::memcpy(snapOurs + sizeof buffer + 0x30, &enabled, 1);
        std::memcpy(snapOurs + sizeof buffer + 0x31, &cull, 1);
    } else {
        std::memcpy(snapOrig, nodes[0], kObjSize);
        std::memcpy(snapOrig + sizeof buffer, &gotPos, sizeof gotPos);
        std::memcpy(snapOrig + sizeof buffer + 0x10, &gotRot, sizeof gotRot);
        std::memcpy(snapOrig + sizeof buffer + 0x20, &gotXY, sizeof gotXY);
        std::memcpy(snapOrig + sizeof buffer + 0x30, &enabled, 1);
        std::memcpy(snapOrig + sizeof buffer + 0x31, &cull, 1);
    }
}

static void RunBounds(int s, bool awkward)
{
    void* o = nodes[0];
    ZeroObjects(2);
    a[s].ctor0(o);
    a[s].setPosition(o, sc.pos);
    a[s].setOrientation(o, sc.rot);

    Sphere sphere{};
    Box box{};
    OBox obox{};
    Sphere global{};
    Vec3 center{};
    sphere = *a[s].getBoundingSphere(o, true);
    box = *a[s].getAxisAlignedBox(o, true);
    obox = *a[s].getOrientedBox(o, true);
    a[s].getGlobalSphere(o, &global, true);
    center = *a[s].getGlobalSphereCenter(o, true);

    unsigned char out[0x100];
    std::memset(out, 0, sizeof out);
    std::memcpy(out, &sphere, sizeof sphere);
    std::memcpy(out + 0x20, &box, sizeof box);
    std::memcpy(out + 0x40, &obox, sizeof obox);
    std::memcpy(out + 0x80, &global, sizeof global);
    std::memcpy(out + 0xa0, &center, sizeof center);
    // the Sphere returned by value has the module's own vftable; its other fields must match
    if (s == 0)
        std::memcpy(snapOurs + sizeof buffer, out, sizeof out);
    else
        std::memcpy(snapOrig + sizeof buffer, out, sizeof out);
}

static void RunTransforms(int s, bool awkward)
{
    void* o = nodes[0];
    void* target = nodes[1];
    ZeroObjects(2);
    a[s].ctor0(o);
    a[s].ctor0(target);
    a[s].setPosition(o, sc.pos);
    a[s].setOrientation(o, sc.rot);
    a[s].setPosition(target, sc.targetPos);
    a[s].setOrientation(target, sc.targetRot);

    Vec3 v = sc.v;
    Vec4 q = sc.q;
    Plane pl = sc.pl;
    OBox ob = sc.ob;

    Vec3 v2 = v;
    Vec4 q2 = q;
    Plane pl2 = pl;
    OBox ob2 = ob;
    Vec3 bothP = v;
    Vec4 bothQ = q;
    Vec3 l2v = v;
    Vec4 l2q = q;
    Plane l2p = pl;
    OBox l2b = ob;

    a[s].localToGlobalVec(o, &v2);
    a[s].localToGlobalQuat(o, &q2);
    a[s].localToGlobalPlane(o, &pl2);
    a[s].localToGlobalBox(o, &ob2);
    a[s].localToGlobalBoth(o, &bothP, &bothQ);
    Vec3 gv = v;
    Vec4 gq = q;
    Plane gp = pl;
    OBox gb = ob;
    a[s].globalToLocalVec(o, &gv);
    a[s].globalToLocalQuat(o, &gq);
    a[s].globalToLocalPlane(o, &gp);
    a[s].globalToLocalBox(o, &gb);
    Vec3 gbp = v;
    Vec4 gbq = q;
    a[s].globalToLocalBoth(o, &gbp, &gbq);
    a[s].localToLocalVec(o, target, &l2v);
    a[s].localToLocalQuat(o, target, &l2q);
    a[s].localToLocalPlane(o, target, &l2p);
    a[s].localToLocalBox(o, target, &l2b);
    Vec3 l2bp = v;
    Vec4 l2bq = q;
    a[s].localToLocalBoth(o, target, &l2bp, &l2bq);

    unsigned char out[0x200];
    std::memset(out, 0, sizeof out);
    size_t at = 0;
    std::memcpy(out + at, &v2, sizeof v2); at += 0x10;
    std::memcpy(out + at, &q2, sizeof q2); at += 0x10;
    std::memcpy(out + at, &pl2, sizeof pl2); at += 0x10;
    std::memcpy(out + at, &ob2, sizeof ob2); at += 0x40;
    std::memcpy(out + at, &bothP, sizeof bothP); at += 0x10;
    std::memcpy(out + at, &bothQ, sizeof bothQ); at += 0x10;
    std::memcpy(out + at, &gv, sizeof gv); at += 0x10;
    std::memcpy(out + at, &gq, sizeof gq); at += 0x10;
    std::memcpy(out + at, &gp, sizeof gp); at += 0x10;
    std::memcpy(out + at, &gb, sizeof gb); at += 0x40;
    std::memcpy(out + at, &gbp, sizeof gbp); at += 0x10;
    std::memcpy(out + at, &gbq, sizeof gbq); at += 0x10;
    std::memcpy(out + at, &l2v, sizeof l2v); at += 0x10;
    std::memcpy(out + at, &l2q, sizeof l2q); at += 0x10;
    std::memcpy(out + at, &l2p, sizeof l2p); at += 0x10;
    std::memcpy(out + at, &l2b, sizeof l2b); at += 0x40;
    std::memcpy(out + at, &l2bp, sizeof l2bp); at += 0x10;
    std::memcpy(out + at, &l2bq, sizeof l2bq); at += 0x10;
    if (s == 0)
        std::memcpy(snapOurs + sizeof buffer, out, sizeof out);
    else
        std::memcpy(snapOrig + sizeof buffer, out, sizeof out);
}

static void RunMisc(int s, bool awkward)
{
    void* o = nodes[0];
    ZeroObjects(2);
    a[s].ctor0(o);
    a[s].setPosition(o, sc.pos);
    a[s].setOrientation(o, sc.rot);
    a[s].translate(o, sc.delta);
    a[s].selfRotateQuat(o, sc.rot2);
    a[s].rotateQuat(o, sc.rot2);
    a[s].rotateMat(o, sc.m3);
    a[s].pointXAt(o, sc.v, sc.delta);
    Mat4 mv{};
    a[s].getModelView(o, &mv, &sc.view);
    if (s == 0) {
        std::memcpy(snapOurs, nodes[0], kObjSize);
        std::memcpy(snapOurs + sizeof buffer, &mv, sizeof mv);
    } else {
        std::memcpy(snapOrig, nodes[0], kObjSize);
        std::memcpy(snapOrig + sizeof buffer, &mv, sizeof mv);
    }
}

static void RunTree(int s, bool awkward)
{
    (void)awkward;
    ZeroObjects(6);
    void* root = nodes[0];
    void* c1 = nodes[1];
    void* c2 = nodes[2];
    void* c3 = nodes[3];
    void* c4 = nodes[4];
    void* c5 = nodes[5];
    a[s].ctor0(root);
    a[s].ctor0(c1);
    a[s].ctor0(c2);
    a[s].ctor0(c3);
    a[s].ctor0(c4);
    a[s].ctor0(c5);
    a[s].setName(c1, Name(1));
    a[s].setName(c2, Name(2));
    a[s].setName(c3, Name(3));
    a[s].setName(c4, Name(4));
    a[s].setName(c5, Name(5));

    a[s].add(root, c1);
    a[s].add(root, c2);
    a[s].push(root, c3);
    It it2;
    it2.node = c2;
    It inserted;
    a[s].insertAfter(root, &inserted, it2, c4);
    It it3;
    it3.node = c3;
    a[s].insertBefore(root, &inserted, it3, c5);

    Fda list{};
    const bool enumerated = a[s].enumerate(root, &list);
    std::vector<void*> items;
    FdaItems(list, items);
    void* found = a[s].findByName(root, Name(4));
    const bool removed = a[s].removeChild(root, c4);
    void* popped = a[s].pop(root);
    void* removedName = a[s].removeNamed(root, Name(2));
    void* last = a[s].removeLast(root);
    const int id = a[s].getID(root);
    const bool ofType = a[s].isOfType(root, 1);

    unsigned char out[0x800];
    std::memset(out, 0, sizeof out);
    int counts[6];
    int at = 0;
    for (int i = 0; i < 6; ++i) {
        std::memcpy(out + at, nodes[i], kObjSize);
        at += kObjSize;
        counts[i] = 0;
    }
    std::memcpy(out + at, &enumerated, 1);
    at += 4;
    const int n = static_cast<int>(items.size());
    std::memcpy(out + at, &n, 4);
    at += 4;
    for (size_t i = 0; i < items.size() && i < 8; ++i) {
        std::ptrdiff_t off = static_cast<unsigned char*>(items[i]) - nodes[0];
        int v = static_cast<int>(off / kStructSize);
        std::memcpy(out + at, &v, 4);
        at += 4;
    }
    for (int i = 0; i < 6; ++i) counts[i] = FBits(0);  // keep the array used
    (void)counts;
    std::ptrdiff_t offs[5] = {
        found ? static_cast<unsigned char*>(found) - nodes[0] : -1,
        removed ? 1 : 0,
        popped ? static_cast<unsigned char*>(popped) - nodes[0] : -1,
        removedName ? static_cast<unsigned char*>(removedName) - nodes[0] : -1,
        last ? static_cast<unsigned char*>(last) - nodes[0] : -1,
    };
    std::memcpy(out + at, offs, sizeof offs);
    at += sizeof offs;
    std::memcpy(out + at, &id, 4);
    at += 4;
    std::memcpy(out + at, &ofType, 1);
    at += 4;
    if (s == 0)
        std::memcpy(snapOurs + sizeof buffer, out, sizeof out);
    else
        std::memcpy(snapOrig + sizeof buffer, out, sizeof out);

    a[s].removeAll(root);
    a[s].removeAllChildren(root);
}

static void RunCopyAssign(int s, bool awkward)
{
    ZeroObjects(3);
    void* src = nodes[0];
    void* dst = nodes[1];
    void* other = nodes[2];
    a[s].ctor0(src);
    a[s].ctor0(other);
    a[s].setName(src, Name(1));
    a[s].setPosition(src, sc.pos);
    a[s].setOrientation(src, sc.rot);
    a[s].setEnabled(src, true);
    a[s].setCullSizeCheck(src, true);

    a[s].ctorCopy(dst, src);
    a[s].setName(other, Name(2));
    a[s].setPosition(other, sc.targetPos);
    a[s].assign(other, src);

    unsigned char out[3 * kObjSize];
    std::memcpy(out, nodes[0], kObjSize);
    std::memcpy(out + kObjSize, nodes[1], kObjSize);
    std::memcpy(out + 2 * kObjSize, nodes[2], kObjSize);
    if (s == 0)
        std::memcpy(snapOurs, out, sizeof out);
    else
        std::memcpy(snapOrig, out, sizeof out);
}

static void RunViewPort(int s, bool awkward)
{
    ZeroObjects(2);
    void* o = nodes[0];
    a[s].vpCtor0(o);
    a[s].setPosition(o, sc.pos);
    if (s == 0) {
        std::memcpy(snapOurs, nodes[0], kViewSize);
    } else {
        std::memcpy(snapOrig, nodes[0], kViewSize);
    }
    const unsigned long flags = a[s].vpGetRenderFlags(o);
    Check(flags == 1, "render flags default");
}

static void RunWithin(int s, bool awkward)
{
    ZeroObjects(4);
    void* root = nodes[0];
    void* c1 = nodes[1];
    void* c2 = nodes[2];
    void* c3 = nodes[3];
    a[s].ctor0(root);
    a[s].ctor0(c1);
    a[s].ctor0(c2);
    a[s].ctor0(c3);
    a[s].setPosition(root, Vec3{0.0f, 0.0f, 0.0f});
    a[s].setPosition(c1, Vec3{1.0f, 0.0f, 0.0f});
    a[s].setPosition(c2, Vec3{0.0f, 1.0f, 0.0f});
    a[s].setPosition(c3, Vec3{0.0f, 0.0f, 1.0f});
    a[s].add(root, c1);
    a[s].add(root, c2);
    a[s].add(root, c3);

    PlaneChunk chunk;
    chunk.next = chunk.prev = &chunk;
    chunk.count = 2;
    chunk.data[0].normal = sc.v;
    chunk.data[0].d = sc.pl.d;
    chunk.data[1].normal = sc.delta;
    chunk.data[1].d = sc.rot2.x;

    Fda list{};
    const bool rootInside = a[s].isWithin(root, &chunk);
    const bool allPlanes = a[s].getAllWithinPlanes(root, &chunk, &list);
    std::vector<void*> items;
    FdaItems(list, items);
    Fda listConst{};
    const bool allPlanesConst = a[s].getAllWithinPlanesConst(root, &chunk, &listConst);
    std::vector<void*> itemsConst;
    FdaItems(listConst, itemsConst);

    Sphere sphere;
    std::memset(&sphere, 0, sizeof sphere);
    sphere.center = sc.targetPos;
    sphere.radius = sc.rot.w;
    Fda listS{};
    const bool allSphere = a[s].getAllWithinSphere(root, &sphere, &listS);
    std::vector<void*> itemsS;
    FdaItems(listS, itemsS);
    Fda listSC{};
    const bool allSphereConst = a[s].getAllWithinSphereConst(root, &sphere, &listSC);
    std::vector<void*> itemsSC;
    FdaItems(listSC, itemsSC);

    unsigned char out[0x200];
    std::memset(out, 0, sizeof out);
    int at = 0;
    const bool bools[6] = {rootInside, allPlanes, allPlanesConst, allSphere, allSphereConst, false};
    std::memcpy(out + at, bools, sizeof bools);
    at += 8;
    const std::vector<void*>* lists[4] = {&items, &itemsConst, &itemsS, &itemsSC};
    for (int i = 0; i < 4; ++i) {
        const int n = static_cast<int>(lists[i]->size());
        std::memcpy(out + at, &n, 4);
        at += 4;
        for (size_t j = 0; j < lists[i]->size() && j < 4; ++j) {
            const int idx = static_cast<int>((static_cast<unsigned char*>((*lists[i])[j]) - nodes[0]) / kStructSize);
            std::memcpy(out + at, &idx, 4);
            at += 4;
        }
    }
    if (s == 0)
        std::memcpy(snapOurs + sizeof buffer, out, sizeof out);
    else
        std::memcpy(snapOrig + sizeof buffer, out, sizeof out);
    a[s].removeAll(root);  // detach the children; the test objects live in static buffers, not Jet heap
    a[s].dtor(root);
    a[s].dtor(c1);
    a[s].dtor(c2);
    a[s].dtor(c3);
}

static void RunClone(int s, bool awkward)
{
    ZeroObjects(4);
    void* src = nodes[0];
    void* c1 = nodes[1];
    void* c2 = nodes[2];
    void* target = nodes[3];
    a[s].ctor0(src);
    a[s].ctor0(c1);
    a[s].ctor0(c2);
    a[s].ctor0(target);
    a[s].setName(src, Name(1));
    a[s].setPosition(src, sc.pos);
    a[s].setOrientation(src, sc.rot);
    a[s].setEnabled(src, true);
    a[s].setCullSizeCheck(src, true);
    a[s].setName(c1, Name(2));
    a[s].setPosition(c1, sc.targetPos);
    a[s].add(src, c1);
    a[s].setName(c2, Name(3));
    a[s].add(src, c2);

    // Add(hostName, child) must attach to the named descendant, not to the root
    a[s].setName(target, Name(4));
    const bool namedAdd = a[s].addNamed2(src, Name(2), target);

    void* clone = a[s].createInstance(src);
    unsigned char out[0x200];
    std::memset(out, 0, sizeof out);
    size_t at = 0;
    const bool have = clone != nullptr;
    std::memcpy(out + at, &have, 1); at += 4;
    if (clone) {
        const unsigned char* b = static_cast<unsigned char*>(clone);
        std::memcpy(out + at, b + 0x24, 4); at += 4;   // cell node (null)
        std::memcpy(out + at, b + 0x28, 4); at += 4;   // name node
        std::memcpy(out + at, b + 0x2c, 8); at += 8;   // position
        std::memcpy(out + at, b + 0x34, 4); at += 4;
        std::memcpy(out + at, b + 0x38, 0x10); at += 0x10;  // orientation
        const unsigned char cloneFlags = static_cast<unsigned char>(b[0x48] & 3);
        std::memcpy(out + at, &cloneFlags, 1); at += 4;      // the two defined flag bits only
        const void* bv = *reinterpret_cast<void**>(const_cast<unsigned char*>(b) + 0x4c);
        int bvSelf = (bv == b + 0x50) ? 1 : 0;
        std::memcpy(out + at, &bvSelf, 4); at += 4;
        const unsigned count = *reinterpret_cast<const unsigned*>(b + 0x0c);
        std::memcpy(out + at, &count, 4); at += 4;
        int childNames[4] = {0, 0, 0, 0};
        const void* child = *reinterpret_cast<void* const*>(b + 0x14);
        for (int i = 0; i < 4 && child; ++i) {
            const unsigned char* cb = static_cast<const unsigned char*>(child);
            const void* node = *reinterpret_cast<void* const*>(cb + 0x28);
            for (int n = 0; n < 8; ++n)
                if (Name(n).node == node) childNames[i] = n + 1;
            child = *reinterpret_cast<void* const*>(cb + 0x1c);
        }
        std::memcpy(out + at, childNames, sizeof childNames); at += sizeof childNames;
        const int id = a[s].getID(clone);
        std::memcpy(out + at, &id, 4); at += 4;
        a[s].dtor(clone);  // heap object: the children clones are heap too
    }

    // the named add changed the target's parent to c1
    const void* targetParent = *reinterpret_cast<void* const*>(static_cast<unsigned char*>(target) + 0x10);
    const int namedAddParent = (targetParent == c1) ? 1 : (targetParent == src ? 2 : 0);
    std::memcpy(out + at, &namedAdd, 1); at += 4;
    std::memcpy(out + at, &namedAddParent, 4); at += 4;

    if (s == 0)
        std::memcpy(snapOurs + sizeof buffer, out, sizeof out);
    else
        std::memcpy(snapOrig + sizeof buffer, out, sizeof out);

    // detach everything and free only childless static objects
    a[s].removeChild(c1, target);
    a[s].removeAll(src);
    a[s].dtor(src);
    a[s].dtor(c1);
    a[s].dtor(c2);
    a[s].dtor(target);
}

static void StoreRefs(unsigned char* out, size_t& at, void* list)
{
    RefChunk* c = list ? *reinterpret_cast<RefChunk**>(list) : nullptr;
    unsigned int n = 0;
    if (c) {
        do {
            n += c->count;
            c = c->next;
        } while (c != *reinterpret_cast<RefChunk**>(list));
    }
    std::memcpy(out + at, &n, 4);
    at += 4;
    if (!c) return;
    RefChunk* const first = *reinterpret_cast<RefChunk**>(list);
    RefChunk* cc = first;
    unsigned int left = n;
    do {
        for (uint32_t i = 0; i < cc->count && left; ++i, --left) {
            const RefPoint& r = cc->data[i];
            std::memcpy(out + at, &r.name, 4); at += 4;
            std::memcpy(out + at, &r.position, sizeof r.position); at += sizeof r.position;
            std::memcpy(out + at, &r.orientation, sizeof r.orientation); at += sizeof r.orientation;
        }
        cc = cc->next;
    } while (cc != first);
}

static void RunClasses(int s, bool awkward)
{
    (void)awkward;
    ZeroObjects(8);
    void* vp1 = nodes[0];
    void* vp2 = nodes[1];
    void* alias = nodes[2];
    void* alSrc = nodes[3];
    void* alDst = nodes[4];
    void* alOther = nodes[5];
    void* st = nodes[6];
    void* stCopy = nodes[7];

    a[s].vpCtor0(vp1);
    a[s].vpCtor0(vp2);
    a[s].setName(vp1, Name(1));
    a[s].setPosition(vp1, sc.pos);
    a[s].setName(vp2, Name(2));
    a[s].vpSetRenderFlags(vp1, 0x2u);
    a[s].vpSetRenderFlags(vp2, 0x8u);

    a[s].aliasCtor(alias, vp1);
    const void* aliasTo1 = a[s].aliasGetAliasTo(alias);
    const int aliasId = a[s].aliasGetID(alias);
    const unsigned long aliasFlags = a[s].vpGetRenderFlags(alias);
    const unsigned vp1Count = *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(vp1) + 0x130);
    void* vp1Alias = *reinterpret_cast<void**>(*reinterpret_cast<void**>(static_cast<unsigned char*>(vp1) + 0x138));
    a[s].aliasSetAliasTo(alias, vp2);
    const unsigned vp1CountAfter = *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(vp1) + 0x130);
    const unsigned vp2CountAfter = *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(vp2) + 0x130);
    const void* aliasTo2 = a[s].aliasGetAliasTo(alias);
    a[s].aliasDtor(alias);
    const unsigned vp2CountFinal = *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(vp2) + 0x130);

    // ViewPortParticle copy and assignment
    a[s].vpCtor0(alSrc);
    a[s].setName(alSrc, Name(3));
    a[s].setPosition(alSrc, sc.targetPos);
    a[s].vpSetRenderFlags(alSrc, 0x4u);
    a[s].vpCtorCopy(alDst, alSrc);
    a[s].vpCtor0(alOther);
    a[s].setName(alOther, Name(4));
    a[s].vpSetRenderFlags(alOther, 0x10u);
    a[s].vpAssign(alOther, alSrc);

    // ParticleStructure with reference points, copy and assignment
    void* stAssigned = nodes[8];
    a[s].stCtor0(st);
    for (int i = 0; i < 3; ++i) {
        RefPoint r;
        r.name = Name(i).node;
        r.position = sc.pos;
        r.position.x += static_cast<float>(i);
        r.orientation = sc.rot;
        a[s].stAddReferencePoint(st, r);
    }
    a[s].stCtorCopy(stCopy, st);
    a[s].stCtor0(stAssigned);
    a[s].stAssign(stAssigned, st);

    unsigned char out[0x200];
    std::memset(out, 0, sizeof out);
    size_t at = 0;
    std::memcpy(out + at, &aliasTo1, 4); at += 4;
    std::memcpy(out + at, &aliasId, 4); at += 4;
    std::memcpy(out + at, &aliasFlags, 4); at += 4;
    std::memcpy(out + at, &vp1Count, 4); at += 4;
    std::memcpy(out + at, &vp1Alias, 4); at += 4;
    std::memcpy(out + at, &vp1CountAfter, 4); at += 4;
    std::memcpy(out + at, &vp2CountAfter, 4); at += 4;
    std::memcpy(out + at, &aliasTo2, 4); at += 4;
    std::memcpy(out + at, &vp2CountFinal, 4); at += 4;
    StoreRefs(out, at, a[s].stGetReferencePointList(stCopy));
    StoreRefs(out, at, a[s].stGetReferencePointList(stAssigned));

    if (s == 0)
        std::memcpy(snapOurs + sizeof buffer, out, sizeof out);
    else
        std::memcpy(snapOrig + sizeof buffer, out, sizeof out);

    // st owns the embedded volume; copies and assignments keep the source's bv pointer (the original's
    // semantics) and must not be destroyed the same way, so only the owning object and the plain view ports go.
    a[s].stDtor(st);
    a[s].vpDtor(alSrc);
    a[s].vpDtor(vp1);
    a[s].vpDtor(vp2);
}

static void RunAttach(int s, bool awkward)
{
    (void)awkward;
    ZeroObjects(5);
    void* ps = nodes[0];
    void* child = nodes[1];
    void* attHost = nodes[2];
    void* attRoot = nodes[3];
    a[s].stCtor0(ps);
    a[s].ctor0(child);
    a[s].ctor0(attHost);
    a[s].ctor0(attRoot);
    a[s].setName(ps, Name(1));
    a[s].setName(child, Name(2));
    a[s].add(ps, child);
    a[s].setName(attHost, Name(3));
    a[s].setName(attRoot, Name(4));
    // the AttachmentPoint's host name PString sits at +0x12c (the class adds it after Particle)
    *reinterpret_cast<void**>(static_cast<unsigned char*>(attHost) + 0x12c) = Name(2).node;
    *reinterpret_cast<void**>(static_cast<unsigned char*>(attRoot) + 0x12c) = nullptr;

    const bool byHost = a[s].stAddAttachment(ps, attHost, true);
    const bool toRoot = a[s].stAddAttachment(ps, attRoot, false);
    // a name with no match must fail
    void* attMiss = nodes[4];
    a[s].ctor0(attMiss);
    *reinterpret_cast<void**>(static_cast<unsigned char*>(attMiss) + 0x12c) = Name(5).node;
    const bool miss = a[s].stAddAttachment(ps, attMiss, true);

    unsigned char out[0x200];
    std::memset(out, 0, sizeof out);
    size_t at = 0;
    std::memcpy(out + at, &byHost, 1); at += 4;
    std::memcpy(out + at, &toRoot, 1); at += 4;
    std::memcpy(out + at, &miss, 1); at += 4;
    const void* childParent = *reinterpret_cast<void**>(static_cast<unsigned char*>(attHost) + 0x10);
    const void* rootParent = *reinterpret_cast<void**>(static_cast<unsigned char*>(attRoot) + 0x10);
    std::memcpy(out + at, &childParent, 4); at += 4;
    std::memcpy(out + at, &rootParent, 4); at += 4;
    if (s == 0)
        std::memcpy(snapOurs + sizeof buffer, out, sizeof out);
    else
        std::memcpy(snapOrig + sizeof buffer, out, sizeof out);
    // detach the static stand-ins first: their scalar deleting destructor would call Jet::Alloc::Delete
    a[s].removeChild(child, attHost);
    a[s].removeAll(ps);
    a[s].stDtor(ps);
    a[s].dtor(child);
    a[s].dtor(attHost);
    a[s].dtor(attRoot);
    a[s].dtor(attMiss);
}

static void RunVtable(int s)
{
    ZeroObjects();
    a[s].ctor0(nodes[0]);
    void** vt = *reinterpret_cast<void***>(nodes[0]);
    const char* slots[48] = {
        "??1Particle@Jet@@UAE@XZ",
        nullptr, nullptr,
        "?Cull@Particle@Jet@@UAAXAAVViewPoint@2@ABVMatrix4@2@@Z",
        nullptr,
        "?RenderDebug@Particle@Jet@@UAAXPAVDisplay@2@AAVDebugViewPort@2@AAVMatrix4@2@K@Z",
        "?CreateInstance@Particle@Jet@@UBAPAV12@XZ",
        "?GetID@Particle@Jet@@UBAHXZ",
        "?IsOfType@Particle@Jet@@UBA_NH@Z",
        "?SetName@Particle@Jet@@UAAXABVPString@2@@Z",
        "?SetParent@Particle@Jet@@UAAXPAV12@@Z",
        "?SetPosition@Particle@Jet@@UAAXABVVector3@2@@Z",
        "?SetPositionXY@Particle@Jet@@UAAXABVVector2@2@@Z",
        "?SetOrientation@Particle@Jet@@UAAXABVQuaternion@2@@Z",
        "?SetReferenceFrame@Particle@Jet@@UAAXABVVector3@2@ABVQuaternion@2@@Z",
        "?GetOpacity@Particle@Jet@@UBAMXZ",
        "?SetOpacity@Particle@Jet@@UAAXM@Z",
        "?Add@Particle@Jet@@UAA_NABVPString@2@PAV12@@Z",
        "?Add@Particle@Jet@@UAAXPAV12@@Z",
        "?Push@Particle@Jet@@UAAXPAV12@@Z",
        "?InsertBefore@Particle@Jet@@UAA?AVIterator@?$ITree@VParticle@Jet@@@2@AAV342@PAV12@@Z",
        "?InsertAfter@Particle@Jet@@UAA?AVIterator@?$ITree@VParticle@Jet@@@2@AAV342@PAV12@@Z",
        "?Remove@Particle@Jet@@UAA?AVIterator@?$ITree@VParticle@Jet@@@2@AAV342@@Z",
        "?Remove@Particle@Jet@@UAAPAV12@ABVPString@2@@Z",
        "?Remove@Particle@Jet@@UAA_NPAV12@@Z",
        "?Remove@Particle@Jet@@UAAPAV12@XZ",
        "?Pop@Particle@Jet@@UAAPAV12@XZ",
        "?RemoveAll@Particle@Jet@@UAAXXZ",
        "?RemoveAllChildren@Particle@Jet@@UAAXXZ",
        "?DeleteAll@Particle@Jet@@UAAXXZ",
        "?CheckVisible@Particle@Jet@@UAA_NABVViewPoint@2@@Z",
        "?IsContainer@Particle@Jet@@UBA_NXZ",
        "?FindParentContainer@Particle@Jet@@UAAPAV12@XZ",
        "?FindParentContainer@Particle@Jet@@UBAPBV12@XZ",
        "?TestCollision@Particle@Jet@@UAA_NABVRay@2@AAVCollisionResult@2@PBV12@@Z",
        "?TestCollisionGeometry@Particle@Jet@@UBA_NABVRay@2@AAMAAVConstFaceDescriptor@2@@Z",
        "?IsWithin@Particle@Jet@@UBA_NABV?$DynamicArray@VPlane@Jet@@$0BA@U?$Less@VPlane@Jet@@V12@@2@@2@@Z",
        "?GetAllWithin@Particle@Jet@@UBA_NABVSphere@2@AAV?$FastDynamicArray@PBVParticle@Jet@@$0BF@U?$Less@PBVParticle@Jet@@PBV12@@2@@2@@Z",
        "?GetAllWithin@Particle@Jet@@UAA_NABVSphere@2@AAV?$FastDynamicArray@PAVParticle@Jet@@$0BF@U?$Less@PAVParticle@Jet@@PAV12@@2@@2@@Z",
        "?GetAllWithin@Particle@Jet@@UBA_NABV?$DynamicArray@VPlane@Jet@@$0BA@U?$Less@VPlane@Jet@@V12@@2@@2@AAV?$FastDynamicArray@PBVParticle@Jet@@$0BF@U?$Less@PBVParticle@Jet@@PBV12@@2@@2@@Z",
        "?GetAllWithin@Particle@Jet@@UAA_NABV?$DynamicArray@VPlane@Jet@@$0BA@U?$Less@VPlane@Jet@@V12@@2@@2@AAV?$FastDynamicArray@PAVParticle@Jet@@$0BF@U?$Less@PAVParticle@Jet@@PAV12@@2@@2@@Z",
        "?GetFacilities@Particle@Jet@@UBA_NAAV?$FastDynamicArray@PBVFacility@Jet@@$0N@U?$Less@PBVFacility@Jet@@PBV12@@2@@2@@Z",
        "?GetFacilities@Particle@Jet@@UAA_NAAV?$FastDynamicArray@PAVFacility@Jet@@$0N@U?$Less@PAVFacility@Jet@@PAV12@@2@@2@@Z",
        "?GetFacility@Particle@Jet@@UAAPAVFacility@2@ABVPString@2@@Z",
        "?GetFacility@Particle@Jet@@UBAPBVFacility@2@ABVPString@2@@Z",
        "?SetBoundingVolume@Particle@Jet@@UAA_NPAVParticleBoundingVolume@2@@Z",
        "?SetRawPosition@Particle@Jet@@MAAXABVVector3@2@@Z",
        "?SetRawOrientation@Particle@Jet@@MAAXABVQuaternion@2@@Z",
    };
    for (int i = 6; i < 48; ++i) {
        if (!slots[i]) continue;
        if (i == 34 || i == 35) continue;  // forwarded wrappers, their vtable entry is our local stub
        void* expected = reinterpret_cast<void*>(GetProcAddress(mod[s], slots[i]));
        if (!expected) {
            std::printf("vtable slot %d export missing: %s\n", i, slots[i]);
            ++bad;
            continue;
        }
        if (vt[i] != expected) std::printf("  slot %d ours=%p expects=%p %s\n", i, vt[i], expected, slots[i]);
        Check(vt[i] == expected, "vtable slot");
    }
    a[s].dtor(nodes[0]);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u9a <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 2000;
    SetDllDirectoryA(argv[1]);
    const std::string dir = argv[1];
    mod[1] = LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str());
    LoadLibraryA((dir + "\\RenderSystem_7c_orig.dll").c_str());
    mod[0] = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    CreateNode = reinterpret_cast<void* (*)(void*, const unsigned char*)>(
        GetProcAddress(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    pstringCache = *reinterpret_cast<void**>(GetProcAddress(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    if (!CreateNode || !pstringCache) {
        std::printf("no PString cache\n");
        return 2;
    }
    for (int s = 0; s < 2; ++s) LoadApi(s);

    for (int r = 0; r < rounds; ++r) {
        const bool awkward = (r % 7) == 6;
        MakeScenario(awkward);
        for (int s = 0; s < 2; ++s) RunCtor(s, awkward);
        CheckObj(snapOurs, snapOrig, kObjSize, "ctor");
        CheckObj(snapOurs + sizeof buffer, snapOrig + sizeof buffer, kObjSize, "dtor");

        for (int s = 0; s < 2; ++s) RunMethods(s, awkward);
        CheckObj(snapOurs, snapOrig, kObjSize, "setters");
        CheckBytes(snapOurs + sizeof buffer, snapOrig + sizeof buffer, 0x40, "getters");

        for (int s = 0; s < 2; ++s) RunBounds(s, awkward);
        // the returned Sphere's vftable is module specific
        CheckBytes(snapOurs + sizeof buffer + 4, snapOrig + sizeof buffer + 4, 0x1c, "bounds sphere");
        CheckBytes(snapOurs + sizeof buffer + 0x20, snapOrig + sizeof buffer + 0x20, 0x60 - 4, "bounds boxes");
        CheckBytes(snapOurs + sizeof buffer + 0x84, snapOrig + sizeof buffer + 0x84, 0x10, "global sphere fields");
        CheckBytes(snapOurs + sizeof buffer + 0xa0, snapOrig + sizeof buffer + 0xa0, 0x10, "global sphere center");

        for (int s = 0; s < 2; ++s) RunTransforms(s, awkward);
        CheckBytes(snapOurs + sizeof buffer, snapOrig + sizeof buffer, 0x200, "transforms");

        for (int s = 0; s < 2; ++s) RunMisc(s, awkward);
        CheckObj(snapOurs, snapOrig, kObjSize, "misc object");
        CheckBytes(snapOurs + sizeof buffer, snapOrig + sizeof buffer, 0x40, "model view");

        for (int s = 0; s < 2; ++s) RunTree(s, awkward);
        for (int i = 0; i < 6; ++i)
            CheckObj(snapOurs + sizeof buffer + i * kObjSize, snapOrig + sizeof buffer + i * kObjSize, kObjSize, "tree object");
        CheckBytes(snapOurs + sizeof buffer + 6 * kObjSize, snapOrig + sizeof buffer + 6 * kObjSize,
                   0x800 - 6 * kObjSize, "tree results");

        for (int s = 0; s < 2; ++s) RunWithin(s, awkward);
        CheckBytes(snapOurs + sizeof buffer, snapOrig + sizeof buffer, 0x200, "within");

        for (int s = 0; s < 2; ++s) RunClone(s, awkward);
        CheckBytes(snapOurs + sizeof buffer, snapOrig + sizeof buffer, 0x200, "clone/add-named");

        for (int s = 0; s < 2; ++s) RunCopyAssign(s, awkward);
        for (int i = 0; i < 3; ++i)
            CheckObj(snapOurs + i * kObjSize, snapOrig + i * kObjSize, kObjSize, "copy/assign object");

        for (int s = 0; s < 2; ++s) RunAttach(s, awkward);
        CheckBytes(snapOurs + sizeof buffer, snapOrig + sizeof buffer, 0x200, "attachment");

        for (int s = 0; s < 2; ++s) RunClasses(s, awkward);
        CheckBytes(snapOurs + sizeof buffer, snapOrig + sizeof buffer, 0x200, "classes");

        for (int s = 0; s < 2; ++s) RunViewPort(s, awkward);
        CheckObj(snapOurs, snapOrig, kViewSize, "viewport particle");
        for (int s = 0; s < 2; ++s) a[s].vpDtor(nodes[0]);
    }

    for (int s = 0; s < 2; ++s) RunVtable(s);

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
