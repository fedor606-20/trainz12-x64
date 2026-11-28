// Differential test of Render3DCore_7c unit U3: the particle bounding volumes against Render3DCore_7c_orig.dll in
// one process.
//
// Every method is called through GetProcAddress on identical inputs (the same buffers are reused for both sides),
// so the object bytes, returned floats and dirty flags can be compared. The vftable pointers of the object and of
// its embedded Sphere are module specific and are the only words skipped. The host particle is a plain 0x12c-byte
// stand-in with the TS12 layout (Particle is verified separately by U9a); parent and child volumes are ordinary
// AxisAlignedBoundingBox objects so the tree-walking and recalculation paths are exercised too.
//
// usage: diff_r3d_u3.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <random>
#include <string>

struct Vec3 {
    float x, y, z;
};
struct Vec4 {
    float x, y, z, w;
};
struct Mat3 {
    float m[9];
};
struct SphereData {
    void* vtable;
    Vec3 center;
    float radius;
};
struct Box {
    Vec3 min, max;
};
struct OBox {
    Vec3 center;
    Vec3 axis[3];
    Vec3 extents;
};

// ParticleBoundingVolume and the derived layouts (see Render3DCoreBounds.h).
struct Bv {
    void* vtable;               // +0x00
    uint32_t flags;             // +0x04
    SphereData sphere;          // +0x08
    Vec3 globalSphereCenter;    // +0x1c
    Box box;                    // +0x28
    OBox obb;                   // +0x40
    Vec3 globalPosition;        // +0x7c
    Vec4 globalOrientation;     // +0x88
    void* host;                 // +0x98
    int primaryType;            // +0x9c
};
struct AabbInput {
    Bv base;                    // +0x00
    OBox localBox;              // +0xa0
};
struct SphereInput {
    Bv base;                    // +0x00
    SphereData localSphere;     // +0xa0
};

static_assert(sizeof(Vec3) == 0xc && sizeof(Vec4) == 0x10 && sizeof(Mat3) == 0x24, "math");
static_assert(sizeof(SphereData) == 0x14 && sizeof(Box) == 0x18 && sizeof(OBox) == 0x3c, "shapes");
static_assert(sizeof(Bv) == 0xa0, "Bv");
static_assert(sizeof(AabbInput) == 0xdc, "AabbInput");
static_assert(sizeof(SphereInput) == 0xb4, "SphereInput");

// The Particle scene node layout needed by the volumes (U9a): ITree at +0x0c, name +0x28, position +0x2c,
// orientation +0x38, flags +0x48, bounding volume +0x4c.
struct ParticleStub {
    void* vtable;                 // +0x00
    uint32_t visited;             // +0x04
    uint32_t lastCulledFrame;     // +0x08
    uint32_t count;               // +0x0c
    ParticleStub* parent;         // +0x10
    ParticleStub* head;           // +0x14
    ParticleStub* tail;           // +0x18
    ParticleStub* next;           // +0x1c
    ParticleStub* prev;           // +0x20
    void* cellNode;               // +0x24
    void* name;                   // +0x28
    Vec3 position;                // +0x2c
    Vec4 orientation;             // +0x38
    uint8_t flags;                // +0x48
    uint8_t padding[3];
    void* volume;                 // +0x4c
    uint8_t rest[0x12c - 0x50];
};
static_assert(sizeof(ParticleStub) == 0x12c, "ParticleStub");

struct SceneIn {
    Vec3 hostPosition;
    Vec4 hostOrientation;
    Vec3 parentPosition;
    Vec4 parentOrientation;
    int childCount;
    Vec3 childPosition[3];
    Vec4 childOrientation[3];
    AabbInput parent;
    AabbInput child[3];
};

enum Kind { K_PBV, K_AABB, K_BS, K_OBB, K_VIS };

static int KindSize(Kind kind)
{
    switch (kind) {
    case K_PBV: return 0xa0;
    case K_BS: return 0xb4;
    default: return 0xdc;
    }
}

static bool IsVtableWord(Kind kind, int offset)
{
    if (offset == 0 || offset == 8) return true;
    return kind == K_BS && offset == 0xa0;
}

static HMODULE mod[2];
static unsigned long long checks, bad, nanPayload;
static std::mt19937 rng(0x33b0);

static alignas(16) unsigned char object[0xdc];
static alignas(16) unsigned char destObject[0xdc];
static alignas(16) unsigned char snapshot[2][0xdc];
static alignas(16) unsigned char result[2][0x200];
static alignas(16) unsigned char parentVolume[0xdc];
static alignas(16) unsigned char childVolume[3][0xdc];
static ParticleStub hostParticle;
static ParticleStub parentParticle;
static ParticleStub childParticle[3];
static const char* currentTest = "";
static const char* currentKind = "";

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
    return awkward ? Awkward() : FRange(-20.0f, 20.0f);
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

static Box RandomBox(bool awkward)
{
    return Box{RandomVec(awkward), RandomVec(awkward)};
}

static OBox RandomOBox(bool awkward)
{
    OBox b;
    b.center = RandomVec(awkward);
    for (Vec3& axis : b.axis) axis = RandomVec(awkward);
    b.extents = RandomVec(awkward);
    return b;
}

static void RandomBv(Bv& bv, bool awkward)
{
    std::memset(&bv, 0, sizeof bv);
    bv.flags = static_cast<uint32_t>(awkward ? Pick(0, 0x3f) : Pick(0, 0xfffff));
    bv.sphere.center = RandomVec(awkward);
    bv.sphere.radius = awkward ? Awkward() : FRange(-2.0f, 20.0f);
    bv.globalSphereCenter = RandomVec(awkward);
    bv.box = RandomBox(awkward);
    bv.obb = RandomOBox(awkward);
    bv.globalPosition = RandomVec(awkward);
    bv.globalOrientation = RandomQuat(awkward);
    bv.primaryType = Pick(0, 3);
}

static void RandomAabb(AabbInput& in, bool awkward)
{
    RandomBv(in.base, awkward);
    in.localBox = RandomOBox(awkward);
}

static void RandomSphereInput(SphereInput& in, bool awkward)
{
    RandomBv(in.base, awkward);
    std::memset(&in.localSphere, 0, sizeof in.localSphere);
    in.localSphere.center = RandomVec(awkward);
    in.localSphere.radius = awkward ? Awkward() : FRange(-2.0f, 20.0f);
}

static void FillScene(SceneIn& scene, bool awkward)
{
    scene.hostPosition = RandomVec(awkward);
    scene.hostOrientation = RandomQuat(awkward);
    scene.parentPosition = RandomVec(awkward);
    scene.parentOrientation = RandomQuat(awkward);
    scene.childCount = Pick(0, 3);
    for (int i = 0; i < 3; ++i) {
        scene.childPosition[i] = RandomVec(awkward);
        scene.childOrientation[i] = RandomQuat(awkward);
        RandomAabb(scene.child[i], awkward);
        scene.child[i].base.host = &childParticle[i];
    }
    RandomAabb(scene.parent, awkward);
    scene.parent.base.host = &parentParticle;
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

static void* Vtable(int side, const char* name)
{
    return reinterpret_cast<void*>(GetProcAddress(mod[side], name));
}

static void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        std::printf("MISMATCH %s (%s,%s)\n", what, currentKind, currentTest);
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
        if (IsNaNBits(u) && IsNaNBits(v)) {
            ++nanPayload;
            continue;
        }
        std::printf("%s first diff +0x%x ours=%08x orig=%08x\n", what, static_cast<unsigned>(i), u, v);
        ok = false;
        break;
    }
    if (ok) ok = std::memcmp(x + i, y + i, n - i) == 0;
    Check(ok, what);
}

static void CheckObject(Kind kind, const void* a, const void* b, const char* what)
{
    const int size = KindSize(kind);
    const unsigned char* x = static_cast<const unsigned char*>(a);
    const unsigned char* y = static_cast<const unsigned char*>(b);
    bool ok = true;
    for (int i = 0; i + 4 <= size; i += 4) {
        if (IsVtableWord(kind, i)) continue;
        uint32_t u, v;
        std::memcpy(&u, x + i, 4);
        std::memcpy(&v, y + i, 4);
        if (u != v) {
            if (IsNaNBits(u) && IsNaNBits(v)) {
                ++nanPayload;
                continue;
            }
            std::printf("%s first diff +0x%x ours=%08x orig=%08x\n", what, static_cast<unsigned>(i), u, v);
            ok = false;
            break;
        }
    }
    Check(ok, what);
}

// ---------------------------------------------------------------- api

struct Api {
    // ParticleBoundingVolume
    void(__thiscall* pbvCtorHost)(void*, ParticleStub*);
    void(__thiscall* pbvCtorCopy)(void*, const void*);
    void*(__thiscall* pbvAssign)(void*, const void*);
    void(__thiscall* pbvDtor)(void*);
    void* pbvVtable;
    void* sphereVtable;
    void* (__cdecl* getHost)(void*);
    const void* (__cdecl* getHostConst)(const void*);
    unsigned long(__cdecl* getDirtyFlags)(const void*);
    bool(__cdecl* isFlagDirty)(const void*, unsigned long);
    void(__cdecl* setDirtyFlags)(void*, unsigned long);
    void(__cdecl* setParentDirtyFlags)(void*, unsigned long);
    void(__cdecl* setChildDirtyFlags)(void*, unsigned long);
    void(__cdecl* setRawDirtyFlagsBool)(void*, bool, unsigned long);
    void(__cdecl* setRawDirtyFlags)(void*, unsigned long);
    void(__cdecl* setPositionChanged)(void*, const Vec3*, const Vec3*);
    void(__cdecl* setOrientationChanged)(void*, const Vec4*, const Vec4*);
    void(__cdecl* setPositionOrientationChanged)(void*, const Vec3*, const Vec3*, const Vec4*, const Vec4*);
    const void* (__cdecl* getBoundingSphere)(const void*, bool);
    const void* (__cdecl* getGlobalSphereCenter)(const void*, bool);
    const void* (__cdecl* getOrientedBox)(const void*, bool);
    const void* (__cdecl* getAxisAlignedBox)(const void*, bool);
    const void* (__cdecl* getGlobalPosition)(const void*);
    const void* (__cdecl* getGlobalOrientation)(const void*);
    int(__cdecl* getPrimaryType)(const void*);
    void(__cdecl* getChildSphere)(const void*, void*);
    void(__cdecl* getChildBox)(const void*, void*);
    void(__cdecl* getChildAlignedBox)(const void*, void*);
    void(__cdecl* pbvInitInstance)(const void*, void*);

    // ParticleAxisAlignedBoundingBox
    void(__thiscall* aabbCtor)(void*, ParticleStub*);
    void(__thiscall* aabbCtorCopy)(void*, const void*);
    void*(__thiscall* aabbAssign)(void*, const void*);
    void(__thiscall* aabbDtor)(void*);
    void* aabbVtable;
    void* (__cdecl* aabbCreateInstance)(const void*, ParticleStub*);
    void(__cdecl* aabbInitInstance)(const void*, void*);
    void(__cdecl* aabbSetSphere)(void*, const SphereData*);
    void(__cdecl* aabbSetBox)(void*, const Box*);
    void(__cdecl* aabbSetObb)(void*, const OBox*);
    void(__cdecl* aabbGetSphere)(const void*, void*);
    void(__cdecl* aabbGetBox)(const void*, void*);
    void(__cdecl* aabbGetObb)(const void*, void*);
    bool(__cdecl* aabbTestRay)(const void*, const void*, bool*);
    bool(__cdecl* aabbTestSegment)(const void*, const void*, bool*);
    void(__cdecl* aabbRecalcSphere)(const void*);
    void(__cdecl* aabbRecalcBox)(const void*);
    void(__cdecl* aabbRecalcObb)(const void*);

    // ParticleBoundingSphere
    void(__thiscall* bsCtor)(void*, ParticleStub*);
    void(__thiscall* bsCtorCopy)(void*, const void*);
    void*(__thiscall* bsAssign)(void*, const void*);
    void(__thiscall* bsDtor)(void*);
    void* bsVtable;
    void* (__cdecl* bsCreateInstance)(const void*, ParticleStub*);
    void(__cdecl* bsInitInstance)(const void*, void*);
    void(__cdecl* bsSetSphere)(void*, const SphereData*);
    void(__cdecl* bsSetBox)(void*, const Box*);
    void(__cdecl* bsSetObb)(void*, const OBox*);
    void(__cdecl* bsGetSphere)(const void*, void*);
    void(__cdecl* bsGetBox)(const void*, void*);
    void(__cdecl* bsGetObb)(const void*, void*);
    bool(__cdecl* bsTestRay)(const void*, const void*, bool*);
    bool(__cdecl* bsTestSegment)(const void*, const void*, bool*);
    void(__cdecl* bsRecalcSphere)(const void*);
    void(__cdecl* bsRecalcBox)(const void*);
    void(__cdecl* bsRecalcObb)(const void*);

    // ParticleOrientedBoundingBox
    void(__thiscall* obbCtor)(void*, ParticleStub*);
    void(__thiscall* obbCtorCopy)(void*, const void*);
    void*(__thiscall* obbAssign)(void*, const void*);
    void(__thiscall* obbDtor)(void*);
    void* obbVtable;
    void* (__cdecl* obbCreateInstance)(const void*, ParticleStub*);
    void(__cdecl* obbInitInstance)(const void*, void*);
    bool(__cdecl* obbTestRay)(const void*, const void*, bool*);
    bool(__cdecl* obbTestSegment)(const void*, const void*, bool*);
    void(__cdecl* obbRecalcSphere)(const void*);
    void(__cdecl* obbRecalcBox)(const void*);
    void(__cdecl* obbRecalcObb)(const void*);

    // ParticleVisibleBoundingBox
    void(__thiscall* visCtor)(void*, ParticleStub*);
    void(__thiscall* visCtorCopy)(void*, const void*);
    void*(__thiscall* visAssign)(void*, const void*);
    void(__thiscall* visDtor)(void*);
    void* visVtable;
    void* (__cdecl* visCreateInstance)(const void*, ParticleStub*);
    bool(__cdecl* visIsWithinViewPoint)(const void*, const void*);
    bool(__cdecl* visIsWithinCamera)(const void*, const void*);
};

static Api api[2];

static void LoadApi(int side)
{
    Api& a = api[side];
    a.pbvCtorHost = Proc<decltype(a.pbvCtorHost)>(side, "??0ParticleBoundingVolume@Jet@@QAE@PAVParticle@1@@Z");
    a.pbvCtorCopy = Proc<decltype(a.pbvCtorCopy)>(side, "??0ParticleBoundingVolume@Jet@@QAE@ABV01@@Z");
    a.pbvAssign = Proc<decltype(a.pbvAssign)>(side, "??4ParticleBoundingVolume@Jet@@QAEAAV01@ABV01@@Z");
    a.pbvDtor = Proc<decltype(a.pbvDtor)>(side, "??1ParticleBoundingVolume@Jet@@UAE@XZ");
    a.pbvVtable = Vtable(side, "??_7ParticleBoundingVolume@Jet@@6B@");
    a.sphereVtable = Vtable(side, "??_7Sphere@Jet@@6B@");
    a.getHost = Proc<decltype(a.getHost)>(side, "?GetHost@ParticleBoundingVolume@Jet@@QAAPAVParticle@2@XZ");
    a.getHostConst = Proc<decltype(a.getHostConst)>(side, "?GetHost@ParticleBoundingVolume@Jet@@QBAPBVParticle@2@XZ");
    a.getDirtyFlags = Proc<decltype(a.getDirtyFlags)>(side, "?GetDirtyFlags@ParticleBoundingVolume@Jet@@QBAKXZ");
    a.isFlagDirty = Proc<decltype(a.isFlagDirty)>(side, "?IsFlagDirty@ParticleBoundingVolume@Jet@@QBA_NK@Z");
    a.setDirtyFlags = Proc<decltype(a.setDirtyFlags)>(side, "?SetDirtyFlags@ParticleBoundingVolume@Jet@@QAAXK@Z");
    a.setParentDirtyFlags =
        Proc<decltype(a.setParentDirtyFlags)>(side, "?SetParentDirtyFlags@ParticleBoundingVolume@Jet@@QAAXK@Z");
    a.setChildDirtyFlags =
        Proc<decltype(a.setChildDirtyFlags)>(side, "?SetChildDirtyFlags@ParticleBoundingVolume@Jet@@QAAXK@Z");
    a.setRawDirtyFlagsBool =
        Proc<decltype(a.setRawDirtyFlagsBool)>(side, "?SetRawDirtyFlags@ParticleBoundingVolume@Jet@@IBAX_NK@Z");
    a.setRawDirtyFlags =
        Proc<decltype(a.setRawDirtyFlags)>(side, "?SetRawDirtyFlags@ParticleBoundingVolume@Jet@@IBAXK@Z");
    a.setPositionChanged =
        Proc<decltype(a.setPositionChanged)>(side, "?SetPositionChanged@ParticleBoundingVolume@Jet@@QAAXABVVector3@2@0@Z");
    a.setOrientationChanged = Proc<decltype(a.setOrientationChanged)>(
        side, "?SetOrientationChanged@ParticleBoundingVolume@Jet@@QAAXABVQuaternion@2@0@Z");
    a.setPositionOrientationChanged = Proc<decltype(a.setPositionOrientationChanged)>(
        side, "?SetPositionOrientationChanged@ParticleBoundingVolume@Jet@@QAAXABVVector3@2@0ABVQuaternion@2@1@Z");
    a.getBoundingSphere = Proc<decltype(a.getBoundingSphere)>(
        side, "?GetBoundingSphere@ParticleBoundingVolume@Jet@@QBAABVSphere@2@_N@Z");
    a.getGlobalSphereCenter = Proc<decltype(a.getGlobalSphereCenter)>(
        side, "?GetGlobalBoundingSphereCenter@ParticleBoundingVolume@Jet@@QBAABVVector3@2@_N@Z");
    a.getOrientedBox = Proc<decltype(a.getOrientedBox)>(
        side, "?GetOrientedBoundingBox@ParticleBoundingVolume@Jet@@QBAABVOrientedBox@2@_N@Z");
    a.getAxisAlignedBox = Proc<decltype(a.getAxisAlignedBox)>(
        side, "?GetAxisAlignedBoundingBox@ParticleBoundingVolume@Jet@@QBAABVAxisAlignedBox@2@_N@Z");
    a.getGlobalPosition = Proc<decltype(a.getGlobalPosition)>(
        side, "?GetGlobalPosition@ParticleBoundingVolume@Jet@@QBAABVVector3@2@XZ");
    a.getGlobalOrientation = Proc<decltype(a.getGlobalOrientation)>(
        side, "?GetGlobalOrientation@ParticleBoundingVolume@Jet@@QBAABVQuaternion@2@XZ");
    a.getPrimaryType = Proc<decltype(a.getPrimaryType)>(
        side, "?GetPrimaryVolumeType@ParticleBoundingVolume@Jet@@QBA?AW4PrimaryVolumeType@12@XZ");
    a.getChildSphere = Proc<decltype(a.getChildSphere)>(
        side, "?GetChildBoundingSphere@ParticleBoundingVolume@Jet@@IBAXAAVSphere@2@@Z");
    a.getChildBox = Proc<decltype(a.getChildBox)>(
        side, "?GetChildBoundingOrientedBox@ParticleBoundingVolume@Jet@@IBAXAAVOrientedBox@2@@Z");
    a.getChildAlignedBox = Proc<decltype(a.getChildAlignedBox)>(
        side, "?GetChildBoundingAxisAlignBox@ParticleBoundingVolume@Jet@@IBAXAAVAxisAlignedBox@2@@Z");
    a.pbvInitInstance = Proc<decltype(a.pbvInitInstance)>(
        side, "?InitInstance@ParticleBoundingVolume@Jet@@IBAXAAV12@@Z");

    a.aabbCtor = Proc<decltype(a.aabbCtor)>(side, "??0ParticleAxisAlignedBoundingBox@Jet@@QAE@PAVParticle@1@@Z");
    a.aabbCtorCopy = Proc<decltype(a.aabbCtorCopy)>(side, "??0ParticleAxisAlignedBoundingBox@Jet@@QAE@ABV01@@Z");
    a.aabbAssign = Proc<decltype(a.aabbAssign)>(side, "??4ParticleAxisAlignedBoundingBox@Jet@@QAEAAV01@ABV01@@Z");
    a.aabbDtor = Proc<decltype(a.aabbDtor)>(side, "??1ParticleAxisAlignedBoundingBox@Jet@@UAE@XZ");
    a.aabbVtable = Vtable(side, "??_7ParticleAxisAlignedBoundingBox@Jet@@6B@");
    a.aabbCreateInstance =
        Proc<decltype(a.aabbCreateInstance)>(side, "?CreateInstance@ParticleAxisAlignedBoundingBox@Jet@@UBAPAVParticleBoundingVolume@2@PAVParticle@2@@Z");
    a.aabbInitInstance = Proc<decltype(a.aabbInitInstance)>(
        side, "?InitInstance@ParticleAxisAlignedBoundingBox@Jet@@IBAXAAV12@@Z");
    a.aabbSetSphere = Proc<decltype(a.aabbSetSphere)>(
        side, "?SetLocalVolume@ParticleAxisAlignedBoundingBox@Jet@@UAAXABVSphere@2@@Z");
    a.aabbSetBox = Proc<decltype(a.aabbSetBox)>(
        side, "?SetLocalVolume@ParticleAxisAlignedBoundingBox@Jet@@UAAXABVAxisAlignedBox@2@@Z");
    a.aabbSetObb = Proc<decltype(a.aabbSetObb)>(
        side, "?SetLocalVolume@ParticleAxisAlignedBoundingBox@Jet@@UAAXABVOrientedBox@2@@Z");
    a.aabbGetSphere = Proc<decltype(a.aabbGetSphere)>(
        side, "?GetLocalVolume@ParticleAxisAlignedBoundingBox@Jet@@UBAXAAVSphere@2@@Z");
    a.aabbGetBox = Proc<decltype(a.aabbGetBox)>(
        side, "?GetLocalVolume@ParticleAxisAlignedBoundingBox@Jet@@UBAXAAVAxisAlignedBox@2@@Z");
    a.aabbGetObb = Proc<decltype(a.aabbGetObb)>(
        side, "?GetLocalVolume@ParticleAxisAlignedBoundingBox@Jet@@UBAXAAVOrientedBox@2@@Z");
    a.aabbTestRay = Proc<decltype(a.aabbTestRay)>(
        side, "?TestCollision@ParticleAxisAlignedBoundingBox@Jet@@UBA_NABVRay@2@AA_N@Z");
    a.aabbTestSegment = Proc<decltype(a.aabbTestSegment)>(
        side, "?TestCollision@ParticleAxisAlignedBoundingBox@Jet@@UBA_NABVLineSegment@2@AA_N@Z");
    a.aabbRecalcSphere = Proc<decltype(a.aabbRecalcSphere)>(
        side, "?RecalculateSphereVolume@ParticleAxisAlignedBoundingBox@Jet@@MBAXXZ");
    a.aabbRecalcBox = Proc<decltype(a.aabbRecalcBox)>(
        side, "?RecalculateAxisAlignedBoxVolume@ParticleAxisAlignedBoundingBox@Jet@@MBAXXZ");
    a.aabbRecalcObb = Proc<decltype(a.aabbRecalcObb)>(
        side, "?RecalculateOrientedBoxVolume@ParticleAxisAlignedBoundingBox@Jet@@MBAXXZ");

    a.bsCtor = Proc<decltype(a.bsCtor)>(side, "??0ParticleBoundingSphere@Jet@@QAE@PAVParticle@1@@Z");
    a.bsCtorCopy = Proc<decltype(a.bsCtorCopy)>(side, "??0ParticleBoundingSphere@Jet@@QAE@ABV01@@Z");
    a.bsAssign = Proc<decltype(a.bsAssign)>(side, "??4ParticleBoundingSphere@Jet@@QAEAAV01@ABV01@@Z");
    a.bsDtor = Proc<decltype(a.bsDtor)>(side, "??1ParticleBoundingSphere@Jet@@UAE@XZ");
    a.bsVtable = Vtable(side, "??_7ParticleBoundingSphere@Jet@@6B@");
    a.bsCreateInstance =
        Proc<decltype(a.bsCreateInstance)>(side, "?CreateInstance@ParticleBoundingSphere@Jet@@UBAPAVParticleBoundingVolume@2@PAVParticle@2@@Z");
    a.bsInitInstance = Proc<decltype(a.bsInitInstance)>(
        side, "?InitInstance@ParticleBoundingSphere@Jet@@IBAXAAV12@@Z");
    a.bsSetSphere = Proc<decltype(a.bsSetSphere)>(
        side, "?SetLocalVolume@ParticleBoundingSphere@Jet@@UAAXABVSphere@2@@Z");
    a.bsSetBox = Proc<decltype(a.bsSetBox)>(
        side, "?SetLocalVolume@ParticleBoundingSphere@Jet@@UAAXABVAxisAlignedBox@2@@Z");
    a.bsSetObb = Proc<decltype(a.bsSetObb)>(
        side, "?SetLocalVolume@ParticleBoundingSphere@Jet@@UAAXABVOrientedBox@2@@Z");
    a.bsGetSphere = Proc<decltype(a.bsGetSphere)>(
        side, "?GetLocalVolume@ParticleBoundingSphere@Jet@@UBAXAAVSphere@2@@Z");
    a.bsGetBox = Proc<decltype(a.bsGetBox)>(
        side, "?GetLocalVolume@ParticleBoundingSphere@Jet@@UBAXAAVAxisAlignedBox@2@@Z");
    a.bsGetObb = Proc<decltype(a.bsGetObb)>(
        side, "?GetLocalVolume@ParticleBoundingSphere@Jet@@UBAXAAVOrientedBox@2@@Z");
    a.bsTestRay = Proc<decltype(a.bsTestRay)>(
        side, "?TestCollision@ParticleBoundingSphere@Jet@@UBA_NABVRay@2@AA_N@Z");
    a.bsTestSegment = Proc<decltype(a.bsTestSegment)>(
        side, "?TestCollision@ParticleBoundingSphere@Jet@@UBA_NABVLineSegment@2@AA_N@Z");
    a.bsRecalcSphere = Proc<decltype(a.bsRecalcSphere)>(
        side, "?RecalculateSphereVolume@ParticleBoundingSphere@Jet@@MBAXXZ");
    a.bsRecalcBox = Proc<decltype(a.bsRecalcBox)>(
        side, "?RecalculateAxisAlignedBoxVolume@ParticleBoundingSphere@Jet@@MBAXXZ");
    a.bsRecalcObb = Proc<decltype(a.bsRecalcObb)>(
        side, "?RecalculateOrientedBoxVolume@ParticleBoundingSphere@Jet@@MBAXXZ");

    a.obbCtor = Proc<decltype(a.obbCtor)>(side, "??0ParticleOrientedBoundingBox@Jet@@QAE@PAVParticle@1@@Z");
    a.obbCtorCopy = Proc<decltype(a.obbCtorCopy)>(side, "??0ParticleOrientedBoundingBox@Jet@@QAE@ABV01@@Z");
    a.obbAssign = Proc<decltype(a.obbAssign)>(side, "??4ParticleOrientedBoundingBox@Jet@@QAEAAV01@ABV01@@Z");
    a.obbDtor = Proc<decltype(a.obbDtor)>(side, "??1ParticleOrientedBoundingBox@Jet@@UAE@XZ");
    a.obbVtable = Vtable(side, "??_7ParticleOrientedBoundingBox@Jet@@6B@");
    a.obbCreateInstance =
        Proc<decltype(a.obbCreateInstance)>(side, "?CreateInstance@ParticleOrientedBoundingBox@Jet@@UBAPAVParticleBoundingVolume@2@PAVParticle@2@@Z");
    a.obbInitInstance = Proc<decltype(a.obbInitInstance)>(
        side, "?InitInstance@ParticleOrientedBoundingBox@Jet@@IBAXAAV12@@Z");
    a.obbTestRay = Proc<decltype(a.obbTestRay)>(
        side, "?TestCollision@ParticleOrientedBoundingBox@Jet@@UBA_NABVRay@2@AA_N@Z");
    a.obbTestSegment = Proc<decltype(a.obbTestSegment)>(
        side, "?TestCollision@ParticleOrientedBoundingBox@Jet@@UBA_NABVLineSegment@2@AA_N@Z");
    a.obbRecalcSphere = Proc<decltype(a.obbRecalcSphere)>(
        side, "?RecalculateSphereVolume@ParticleOrientedBoundingBox@Jet@@MBAXXZ");
    a.obbRecalcBox = Proc<decltype(a.obbRecalcBox)>(
        side, "?RecalculateAxisAlignedBoxVolume@ParticleOrientedBoundingBox@Jet@@MBAXXZ");
    a.obbRecalcObb = Proc<decltype(a.obbRecalcObb)>(
        side, "?RecalculateOrientedBoxVolume@ParticleOrientedBoundingBox@Jet@@MBAXXZ");

    a.visCtor = Proc<decltype(a.visCtor)>(side, "??0ParticleVisibleBoundingBox@Jet@@QAE@PAVParticle@1@@Z");
    a.visCtorCopy = Proc<decltype(a.visCtorCopy)>(side, "??0ParticleVisibleBoundingBox@Jet@@QAE@ABV01@@Z");
    a.visAssign = Proc<decltype(a.visAssign)>(side, "??4ParticleVisibleBoundingBox@Jet@@QAEAAV01@ABV01@@Z");
    a.visDtor = Proc<decltype(a.visDtor)>(side, "??1ParticleVisibleBoundingBox@Jet@@UAE@XZ");
    a.visVtable = Vtable(side, "??_7ParticleVisibleBoundingBox@Jet@@6B@");
    a.visCreateInstance =
        Proc<decltype(a.visCreateInstance)>(side, "?CreateInstance@ParticleVisibleBoundingBox@Jet@@UBAPAVParticleBoundingVolume@2@PAVParticle@2@@Z");
    a.visIsWithinViewPoint = Proc<decltype(a.visIsWithinViewPoint)>(
        side, "?IsWithinView@ParticleVisibleBoundingBox@Jet@@UBA_NABVViewPoint@2@@Z");
    a.visIsWithinCamera = Proc<decltype(a.visIsWithinCamera)>(
        side, "?IsWithinView@ParticleVisibleBoundingBox@Jet@@UBA_NPBVCamera@2@@Z");
}

// ---------------------------------------------------------------- scene

static void ApplyInput(Kind kind, void* dst, const void* src)
{
    const int size = KindSize(kind);
    unsigned char* d = static_cast<unsigned char*>(dst);
    const unsigned char* s = static_cast<const unsigned char*>(src);
    for (int i = 0; i + 4 <= size; i += 4) {
        if (IsVtableWord(kind, i)) continue;
        std::memcpy(d + i, s + i, 4);
    }
}

static void PrepareScene(int side, const SceneIn& scene)
{
    std::memset(&hostParticle, 0, sizeof hostParticle);
    hostParticle.position = scene.hostPosition;
    hostParticle.orientation = scene.hostOrientation;
    hostParticle.parent = &parentParticle;

    std::memset(&parentParticle, 0, sizeof parentParticle);
    parentParticle.position = scene.parentPosition;
    parentParticle.orientation = scene.parentOrientation;
    parentParticle.volume = parentVolume;

    for (int i = 0; i < 3; ++i) {
        std::memset(&childParticle[i], 0, sizeof childParticle[i]);
        childParticle[i].position = scene.childPosition[i];
        childParticle[i].orientation = scene.childOrientation[i];
        childParticle[i].parent = &hostParticle;
        childParticle[i].volume = childVolume[i];
    }
    hostParticle.count = static_cast<uint32_t>(scene.childCount);
    if (scene.childCount > 0) {
        hostParticle.head = &childParticle[0];
        hostParticle.tail = &childParticle[scene.childCount - 1];
        for (int i = 0; i < scene.childCount; ++i) {
            childParticle[i].next = i + 1 < scene.childCount ? &childParticle[i + 1] : nullptr;
            childParticle[i].prev = i > 0 ? &childParticle[i - 1] : nullptr;
        }
    }

    api[side].aabbCtor(parentVolume, &parentParticle);
    ApplyInput(K_AABB, parentVolume, &scene.parent);
    for (int i = 0; i < 3; ++i) {
        api[side].aabbCtor(childVolume[i], &childParticle[i]);
        ApplyInput(K_AABB, childVolume[i], &scene.child[i]);
    }
}

static void PrepareObject(int side, Kind kind, const void* input)
{
    switch (kind) {
    case K_PBV: api[side].pbvCtorHost(object, &hostParticle); break;
    case K_AABB: api[side].aabbCtor(object, &hostParticle); break;
    case K_BS: api[side].bsCtor(object, &hostParticle); break;
    case K_OBB: api[side].obbCtor(object, &hostParticle); break;
    case K_VIS: api[side].visCtor(object, &hostParticle); break;
    }
    ApplyInput(kind, object, input);
    hostParticle.volume = object;
}

static void PrepareDest(int side, Kind kind)
{
    switch (kind) {
    case K_PBV: api[side].pbvCtorHost(destObject, &hostParticle); break;
    case K_AABB: api[side].aabbCtor(destObject, &hostParticle); break;
    case K_BS: api[side].bsCtor(destObject, &hostParticle); break;
    case K_OBB: api[side].obbCtor(destObject, &hostParticle); break;
    case K_VIS: api[side].visCtor(destObject, &hostParticle); break;
    }
}

template <class F>
static void Run(Kind kind, const SceneIn& scene, const void* input, const char* what, F body)
{
    currentTest = what;
    for (int side = 0; side < 2; ++side) {
        PrepareScene(side, scene);
        PrepareObject(side, kind, input);
        body(side);
        std::memcpy(snapshot[side], object, KindSize(kind));
    }
    CheckObject(kind, snapshot[0], snapshot[1], what);
}

// ---------------------------------------------------------------- test body

static void TestRound(int round)
{
    const bool awkward = (round % 7) == 0;
    SceneIn scene;
    FillScene(scene, awkward);

    AabbInput boxInput;
    RandomAabb(boxInput, awkward);
    boxInput.base.host = &hostParticle;

    SphereInput sphereInput;
    RandomSphereInput(sphereInput, awkward);
    sphereInput.base.host = &hostParticle;

    const Vec3 vecA = RandomVec(awkward), vecB = RandomVec(awkward);
    const Vec4 quatA = RandomQuat(awkward), quatB = RandomQuat(awkward);
    const SphereData sphereA{nullptr, RandomVec(awkward), awkward ? Awkward() : FRange(-5.0f, 30.0f)};
    const Box boxA = RandomBox(awkward);
    const OBox obbA = RandomOBox(awkward);

    // ray / segment data (direction +0x04, point +0x10, end +0x1c)
    alignas(16) unsigned char ray[0x1c];
    std::memset(ray, 0, sizeof ray);
    std::memcpy(ray + 4, &vecA, sizeof vecA);
    std::memcpy(ray + 0x10, &vecB, sizeof vecB);
    alignas(16) unsigned char segment[0x28];
    std::memset(segment, 0, sizeof segment);
    std::memcpy(segment + 4, &vecA, sizeof vecA);
    std::memcpy(segment + 0x10, &vecB, sizeof vecB);
    std::memcpy(segment + 0x1c, &vecA, sizeof vecA);

    static const Kind kinds[5] = {K_PBV, K_AABB, K_BS, K_OBB, K_VIS};
    for (Kind kind : kinds) {
        currentKind = kind == K_PBV ? "pbv" : kind == K_AABB ? "aabb" : kind == K_BS ? "bs" : kind == K_OBB ? "obb" : "vis";
        const void* input = &boxInput;
        if (kind == K_BS) input = &sphereInput;
        const int size = KindSize(kind);

        // host constructor: the raw object after construction
        Run(kind, scene, input, "constructor", [](int) {});

        // vftable pointers: object, and the embedded Sphere
        {
            PrepareScene(0, scene);
            PrepareObject(0, kind, input);
            void* expected = kind == K_PBV ? api[0].pbvVtable
                             : kind == K_AABB ? api[0].aabbVtable
                             : kind == K_BS ? api[0].bsVtable
                             : kind == K_OBB ? api[0].obbVtable
                                             : api[0].visVtable;
            Check(*(void**)object == expected, "object vftable");
            Check(((Bv*)object)->sphere.vtable == api[0].sphereVtable, "embedded sphere vftable");
            if (kind == K_BS) Check(((SphereInput*)object)->localSphere.vtable == api[0].sphereVtable, "local sphere vftable");
            PrepareScene(1, scene);
            PrepareObject(1, kind, input);
            expected = kind == K_PBV ? api[1].pbvVtable
                       : kind == K_AABB ? api[1].aabbVtable
                       : kind == K_BS ? api[1].bsVtable
                       : kind == K_OBB ? api[1].obbVtable
                                       : api[1].visVtable;
            Check(*(void**)object == expected, "orig object vftable");
        }

        // copy constructor
        currentTest = "copy constructor";
        for (int side = 0; side < 2; ++side) {
            PrepareScene(side, scene);
            PrepareObject(side, kind, input);
            PrepareDest(side, kind);
            switch (kind) {
            case K_PBV: api[side].pbvCtorCopy(destObject, object); break;
            case K_AABB: api[side].aabbCtorCopy(destObject, object); break;
            case K_BS: api[side].bsCtorCopy(destObject, object); break;
            case K_OBB: api[side].obbCtorCopy(destObject, object); break;
            case K_VIS: api[side].visCtorCopy(destObject, object); break;
            }
            std::memcpy(result[side], destObject, size);
        }
        CheckObject(kind, result[0], result[1], "copy constructor");

        // assignment
        currentTest = "assignment";
        for (int side = 0; side < 2; ++side) {
            PrepareScene(side, scene);
            PrepareObject(side, kind, input);
            PrepareDest(side, kind);
            ApplyInput(kind, destObject, kind == K_BS ? static_cast<const void*>(&sphereInput) : static_cast<const void*>(&boxInput));
            switch (kind) {
            case K_PBV: api[side].pbvAssign(destObject, object); break;
            case K_AABB: api[side].aabbAssign(destObject, object); break;
            case K_BS: api[side].bsAssign(destObject, object); break;
            case K_OBB: api[side].obbAssign(destObject, object); break;
            case K_VIS: api[side].visAssign(destObject, object); break;
            }
            std::memcpy(result[side], destObject, size);
        }
        CheckObject(kind, result[0], result[1], "assignment");

        // base getters and flags
        Run(kind, scene, input, "GetHost", [&](int side) {
            void* h = api[side].getHost(object);
            const void* ch = api[side].getHostConst(object);
            std::memcpy(result[side], &h, 4);
            std::memcpy(result[side] + 4, &ch, 4);
            const unsigned long flags = api[side].getDirtyFlags(object);
            std::memcpy(result[side] + 8, &flags, 4);
            const int type = api[side].getPrimaryType(object);
            std::memcpy(result[side] + 12, &type, 4);
        });
        CheckBytes(result[0], result[1], 16, "GetHost/GetDirtyFlags/GetPrimaryVolumeType");

        Run(kind, scene, input, "IsFlagDirty", [&](int side) {
            uint32_t bits = 0;
            for (uint32_t mask : {1u, 2u, 4u, 8u, 0x10u, 0x20u, 0x80000000u}) {
                if (api[side].isFlagDirty(object, mask)) bits |= mask;
            }
            std::memcpy(result[side], &bits, 4);
        });
        CheckBytes(result[0], result[1], 4, "IsFlagDirty");

        Run(kind, scene, input, "SetDirtyFlags", [&](int side) {
            api[side].setDirtyFlags(object, 0x24);
            api[side].setParentDirtyFlags(object, 0x11);
            api[side].setChildDirtyFlags(object, 0x09);
        });
        Run(kind, scene, input, "SetRawDirtyFlags", [&](int side) {
            api[side].setRawDirtyFlagsBool(object, true, 0x30);
            api[side].setRawDirtyFlagsBool(object, false, 0x0c);
            api[side].setRawDirtyFlags(object, 0x12345);
        });

        // returned volumes / frames with and without recalculation
        Run(kind, scene, input, "GetBoundingSphere", [&](int side) {
            const void* p = api[side].getBoundingSphere(object, false);
            std::memcpy(result[side], p, 0x14);
        });
        CheckBytes(result[0] + 4, result[1] + 4, 0x10, "GetBoundingSphere(false)");

        Run(kind, scene, input, "GetBoundingSphere recalc", [&](int side) {
            const void* p = api[side].getBoundingSphere(object, true);
            std::memcpy(result[side], p, 0x14);
        });
        CheckBytes(result[0] + 4, result[1] + 4, 0x10, "GetBoundingSphere(true) fields");

        Run(kind, scene, input, "GetGlobalBoundingSphereCenter", [&](int side) {
            const void* p = api[side].getGlobalSphereCenter(object, true);
            std::memcpy(result[side], p, 0xc);
        });
        CheckBytes(result[0], result[1], 0xc, "GetGlobalBoundingSphereCenter");

        Run(kind, scene, input, "GetAxisAlignedBoundingBox", [&](int side) {
            const void* p = api[side].getAxisAlignedBox(object, true);
            std::memcpy(result[side], p, 0x18);
        });
        CheckBytes(result[0], result[1], 0x18, "GetAxisAlignedBoundingBox");

        Run(kind, scene, input, "GetOrientedBoundingBox", [&](int side) {
            const void* p = api[side].getOrientedBox(object, true);
            std::memcpy(result[side], p, 0x3c);
        });
        CheckBytes(result[0], result[1], 0x3c, "GetOrientedBoundingBox");


        Run(kind, scene, input, "GetGlobalPosition", [&](int side) {
            const void* p = api[side].getGlobalPosition(object);
            std::memcpy(result[side], p, 0xc);
        });
        CheckBytes(result[0], result[1], 0xc, "GetGlobalPosition");

        Run(kind, scene, input, "GetGlobalOrientation", [&](int side) {
            const void* p = api[side].getGlobalOrientation(object);
            std::memcpy(result[side], p, 0x10);
        });
        CheckBytes(result[0], result[1], 0x10, "GetGlobalOrientation");

        // setters
        Run(kind, scene, input, "SetPositionChanged", [&](int side) {
            api[side].setPositionChanged(object, &vecA, &vecB);
        });
        Run(kind, scene, input, "SetOrientationChanged", [&](int side) {
            api[side].setOrientationChanged(object, &quatA, &quatB);
        });
        Run(kind, scene, input, "SetPositionOrientationChanged", [&](int side) {
            api[side].setPositionOrientationChanged(object, &vecA, &vecB, &quatA, &quatB);
        });

        // direct child-union helpers
        Run(kind, scene, input, "GetChildBoundingSphere", [&](int side) {
            std::memset(result[side], 0xcd, 0x14);
            *reinterpret_cast<void**>(result[side]) = api[side].sphereVtable;  // the union path calls Sphere::Maximize
            api[side].getChildSphere(object, result[side]);
        });
        CheckBytes(result[0] + 4, result[1] + 4, 0x10, "GetChildBoundingSphere fields");

        Run(kind, scene, input, "GetChildBoundingOrientedBox", [&](int side) {
            std::memset(result[side], 0xcd, 0x3c);
            api[side].getChildBox(object, result[side]);
        });
        CheckBytes(result[0], result[1], 0x3c, "GetChildBoundingOrientedBox");


        Run(kind, scene, input, "GetChildBoundingAxisAlignBox", [&](int side) {
            std::memset(result[side], 0xcd, 0x18);
            api[side].getChildAlignedBox(object, result[side]);
        });
        CheckBytes(result[0], result[1], 0x18, "GetChildBoundingAxisAlignBox");

        // CreateInstance and InitInstance
        {
            currentTest = "CreateInstance";
            void* created[2] = {nullptr, nullptr};
            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                switch (kind) {
                case K_PBV: created[side] = nullptr; break;
                case K_AABB: created[side] = api[side].aabbCreateInstance(object, &childParticle[0]); break;
                case K_BS: created[side] = api[side].bsCreateInstance(object, &childParticle[0]); break;
                case K_OBB: created[side] = api[side].obbCreateInstance(object, &childParticle[0]); break;
                case K_VIS: created[side] = api[side].visCreateInstance(object, &childParticle[0]); break;
                }
                if (created[side]) std::memcpy(result[side], created[side], size);
            }
            if (kind != K_PBV) {
                Check(created[0] && created[1], "CreateInstance non-null");
                if (created[0] && created[1]) {
                    CheckObject(kind, result[0], result[1], "CreateInstance object");
                    void* expected = kind == K_AABB ? api[0].aabbVtable
                                     : kind == K_BS ? api[0].bsVtable
                                     : kind == K_OBB ? api[0].obbVtable
                                                     : api[0].visVtable;
                    Check(*(void**)created[0] == expected, "CreateInstance vftable");
                    using DeleteFn = void(__thiscall*)(void*, int);
                    reinterpret_cast<DeleteFn>((*(void***)created[0])[0])(created[0], 1);
                }
            }
        }

        {
            currentTest = "InitInstance";
            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                PrepareDest(side, kind);
                ApplyInput(kind, destObject, kind == K_BS ? static_cast<const void*>(&sphereInput) : static_cast<const void*>(&boxInput));
                switch (kind) {
                case K_PBV: api[side].pbvInitInstance(object, destObject); break;
                case K_AABB: api[side].aabbInitInstance(object, destObject); break;
                case K_BS: api[side].bsInitInstance(object, destObject); break;
                case K_OBB: api[side].obbInitInstance(object, destObject); break;
                case K_VIS: PrepareDest(side, K_VIS); break;  // no InitInstance export
                }
                std::memcpy(result[side], destObject, size);
            }
            CheckObject(kind, result[0], result[1], "InitInstance");
        }

        // class-specific local volume and collision methods
        if (kind == K_AABB || kind == K_OBB || kind == K_VIS) {
            Run(kind, scene, input, "AABB SetLocalVolume(Sphere)", [&](int side) {
                api[side].aabbSetSphere(object, &sphereA);
            });
            Run(kind, scene, input, "AABB SetLocalVolume(AABB)", [&](int side) {
                api[side].aabbSetBox(object, &boxA);
            });
            Run(kind, scene, input, "AABB SetLocalVolume(OBB)", [&](int side) {
                api[side].aabbSetObb(object, &obbA);
            });

            const auto getSphere = [&](int side) {
                std::memset(result[side], 0xcd, 0x14);
                api[side].aabbGetSphere(object, result[side]);
            };
            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                getSphere(side);
            }
            CheckBytes(result[0] + 4, result[1] + 4, 0x10, "AABB GetLocalVolume(Sphere)");

            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                std::memset(result[side], 0xcd, 0x18);
                api[side].aabbGetBox(object, result[side]);
            }
            CheckBytes(result[0], result[1], 0x18, "AABB GetLocalVolume(AABB)");

            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                std::memset(result[side], 0xcd, 0x3c);
                api[side].aabbGetObb(object, result[side]);
            }
            CheckBytes(result[0], result[1], 0x3c, "AABB GetLocalVolume(OBB)");

            currentTest = "AABB TestCollision Ray";
            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                bool hit = false;
                const bool r = api[side].aabbTestRay(object, ray, &hit);
                std::memcpy(result[side], &r, 1);
                std::memcpy(result[side] + 1, &hit, 1);
            }
            CheckBytes(result[0], result[1], 2, "AABB TestCollision(Ray)");

            currentTest = "AABB TestCollision Segment";
            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                bool hit = false;
                const bool r = api[side].aabbTestSegment(object, segment, &hit);
                std::memcpy(result[side], &r, 1);
                std::memcpy(result[side] + 1, &hit, 1);
            }
            CheckBytes(result[0], result[1], 2, "AABB TestCollision(LineSegment)");

            Run(kind, scene, input, "AABB RecalculateSphereVolume", [&](int side) {
                api[side].aabbRecalcSphere(object);
            });
            Run(kind, scene, input, "AABB RecalculateAxisAlignedBoxVolume", [&](int side) {
                api[side].aabbRecalcBox(object);
            });
            Run(kind, scene, input, "AABB RecalculateOrientedBoxVolume", [&](int side) {
                api[side].aabbRecalcObb(object);
            });
        }

        if (kind == K_BS) {
            Run(kind, scene, input, "Sphere SetLocalVolume(Sphere)", [&](int side) {
                api[side].bsSetSphere(object, &sphereA);
            });
            Run(kind, scene, input, "Sphere SetLocalVolume(AABB)", [&](int side) {
                api[side].bsSetBox(object, &boxA);
            });
            Run(kind, scene, input, "Sphere SetLocalVolume(OBB)", [&](int side) {
                api[side].bsSetObb(object, &obbA);
            });
            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                std::memset(result[side], 0xcd, 0x14);
                api[side].bsGetSphere(object, result[side]);
            }
            CheckBytes(result[0] + 4, result[1] + 4, 0x10, "Sphere GetLocalVolume(Sphere)");
            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                std::memset(result[side], 0xcd, 0x18);
                api[side].bsGetBox(object, result[side]);
            }
            CheckBytes(result[0], result[1], 0x18, "Sphere GetLocalVolume(AABB)");
            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                std::memset(result[side], 0xcd, 0x3c);
                api[side].bsGetObb(object, result[side]);
            }
            CheckBytes(result[0], result[1], 0x3c, "Sphere GetLocalVolume(OBB)");

            currentTest = "Sphere TestCollision Ray";
            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                bool hit = false;
                const bool r = api[side].bsTestRay(object, ray, &hit);
                std::memcpy(result[side], &r, 1);
                std::memcpy(result[side] + 1, &hit, 1);
            }
            CheckBytes(result[0], result[1], 2, "Sphere TestCollision(Ray)");

            currentTest = "Sphere TestCollision Segment";
            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                bool hit = false;
                const bool r = api[side].bsTestSegment(object, segment, &hit);
                std::memcpy(result[side], &r, 1);
                std::memcpy(result[side] + 1, &hit, 1);
            }
            CheckBytes(result[0], result[1], 2, "Sphere TestCollision(LineSegment)");

            Run(kind, scene, input, "Sphere RecalculateSphereVolume", [&](int side) {
                api[side].bsRecalcSphere(object);
            });
            Run(kind, scene, input, "Sphere RecalculateAxisAlignedBoxVolume", [&](int side) {
                api[side].bsRecalcBox(object);
            });
            Run(kind, scene, input, "Sphere RecalculateOrientedBoxVolume", [&](int side) {
                api[side].bsRecalcObb(object);
            });
        }

        if (kind == K_OBB) {
            currentTest = "OBB TestCollision Ray";
            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                bool hit = false;
                const bool r = api[side].obbTestRay(object, ray, &hit);
                std::memcpy(result[side], &r, 1);
                std::memcpy(result[side] + 1, &hit, 1);
            }
            CheckBytes(result[0], result[1], 2, "OBB TestCollision(Ray)");
            currentTest = "OBB TestCollision Segment";
            for (int side = 0; side < 2; ++side) {
                PrepareScene(side, scene);
                PrepareObject(side, kind, input);
                bool hit = false;
                const bool r = api[side].obbTestSegment(object, segment, &hit);
                std::memcpy(result[side], &r, 1);
                std::memcpy(result[side] + 1, &hit, 1);
            }
            CheckBytes(result[0], result[1], 2, "OBB TestCollision(LineSegment)");
            Run(kind, scene, input, "OBB RecalculateSphereVolume", [&](int side) {
                api[side].obbRecalcSphere(object);
            });
            Run(kind, scene, input, "OBB RecalculateAxisAlignedBoxVolume", [&](int side) {
                api[side].obbRecalcBox(object);
            });
            Run(kind, scene, input, "OBB RecalculateOrientedBoxVolume", [&](int side) {
                api[side].obbRecalcObb(object);
            });
        }

        // vtable slots 7..12 (Set/GetLocalVolume) and 3/4 (collision), for every class
        currentTest = "vtable SetLocalVolume/GetLocalVolume";
        for (int side = 0; side < 2; ++side) {
            PrepareScene(side, scene);
            PrepareObject(side, kind, input);
            void** vt = *reinterpret_cast<void***>(object);
            using SetS = void(__cdecl*)(void*, const SphereData*);
            using SetB = void(__cdecl*)(void*, const Box*);
            using SetO = void(__cdecl*)(void*, const OBox*);
            using GetS = void(__cdecl*)(const void*, void*);
            using GetB = void(__cdecl*)(const void*, void*);
            using GetO = void(__cdecl*)(const void*, void*);
            reinterpret_cast<SetS>(vt[7])(object, &sphereA);
            reinterpret_cast<SetB>(vt[8])(object, &boxA);
            reinterpret_cast<SetO>(vt[9])(object, &obbA);
            std::memset(result[side], 0xcd, 0x88);
            reinterpret_cast<GetS>(vt[10])(object, result[side]);
            reinterpret_cast<GetB>(vt[11])(object, result[side] + 0x14);
            reinterpret_cast<GetO>(vt[12])(object, result[side] + 0x2c);
            std::memcpy(result[side] + 0x68, object, KindSize(kind));
        }
        CheckBytes(result[0] + 4, result[1] + 4, 0x10, "vtable GetLocalVolume(Sphere)");
        CheckBytes(result[0] + 0x14, result[1] + 0x14, 0x18, "vtable GetLocalVolume(AABB)");
        CheckBytes(result[0] + 0x2c, result[1] + 0x2c, 0x3c, "vtable GetLocalVolume(OBB)");
        CheckObject(kind, result[0] + 0x68, result[1] + 0x68, "vtable SetLocalVolume object");

        currentTest = "vtable TestCollision";
        for (int side = 0; side < 2; ++side) {
            PrepareScene(side, scene);
            PrepareObject(side, kind, input);
            void** vt = *reinterpret_cast<void***>(object);
            using Test = bool(__cdecl*)(const void*, const void*, bool*);
            bool hitA = false, hitB = false;
            const bool a = reinterpret_cast<Test>(vt[3])(object, segment, &hitA);
            const bool b = reinterpret_cast<Test>(vt[4])(object, ray, &hitB);
            std::memcpy(result[side], &a, 1);
            std::memcpy(result[side] + 1, &hitA, 1);
            std::memcpy(result[side] + 2, &b, 1);
            std::memcpy(result[side] + 3, &hitB, 1);
        }
        CheckBytes(result[0], result[1], 4, "vtable TestCollision slots");

        // destructor: sets the ParticleBoundingVolume vftable
        currentTest = "destructor";
        for (int side = 0; side < 2; ++side) {
            PrepareScene(side, scene);
            PrepareObject(side, kind, input);
            switch (kind) {
            case K_PBV: api[side].pbvDtor(object); break;
            case K_AABB: api[side].aabbDtor(object); break;
            case K_BS: api[side].bsDtor(object); break;
            case K_OBB: api[side].obbDtor(object); break;
            case K_VIS: api[side].visDtor(object); break;
            }
            std::memcpy(result[side], object, size);
            Check(*(void**)object == api[side].pbvVtable, "destructor sets base vftable");
        }
        CheckObject(kind, result[0], result[1], "destructor fields");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u3 <bin dir> [rounds]\n");
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
    LoadApi(0);
    LoadApi(1);

    for (int round = 0; round < rounds; ++round) TestRound(round);

    std::printf("%llu checks, %llu mismatches, %llu NaN-payload-only differences\n", checks, bad, nanPayload);
    return bad ? 1 : 0;
}
