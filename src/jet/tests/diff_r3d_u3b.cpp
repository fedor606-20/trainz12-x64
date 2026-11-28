// Differential test of Render3DCore_7c unit U3b: the twelve bounding-volume view and renderable methods U3 forwarded
// (ParticleAxisAlignedBoundingBox / ParticleBoundingSphere / ParticleOrientedBoundingBox:
// IsWithinView(ViewPoint&), IsWithinView(Camera*), CreateRenderableObject, CheckRenderableObjectPosition).
//
// Every export is called through GetProcAddress on identical buffers for both modules and the raw results, object
// words and returned primitive fields are compared. Cameras are real U8 objects built through each module's exported
// constructor; the ViewPoint is the original's 0x70-byte stand-in with the camera pointer at +0x04 (its vtable is
// never used by these paths). The renderable methods get a fake RenderKernel whose +0x90 material manager returns a
// failure from Create, so RenderPrimitive::Init stops before the material/geometry path while still exercising the
// allocation, argument computation, dirty recalculation, replacement and position/orientation code. The primitive
// vtable words and the heap block pointers are module specific and are compared structurally.
//
// usage: diff_r3d_u3b.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
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
    Bv base;
    OBox localBox;              // +0xa0
};
struct SphereInput {
    Bv base;
    SphereData localSphere;     // +0xa0
};
struct ViewStub {
    void* vtable;               // +0x00 (unused here)
    void* camera;               // +0x04
    unsigned char rest[0x68];
};

static_assert(sizeof(Vec3) == 0xc && sizeof(Vec4) == 0x10, "math");
static_assert(sizeof(SphereData) == 0x14 && sizeof(Box) == 0x18 && sizeof(OBox) == 0x3c, "shapes");
static_assert(sizeof(Bv) == 0xa0, "Bv");
static_assert(sizeof(AabbInput) == 0xdc && sizeof(SphereInput) == 0xb4, "derived");
static_assert(sizeof(ViewStub) == 0x70, "ViewStub");

// The Particle scene node layout the volumes read (U9a): ITree at +0x0c, position +0x2c, orientation +0x38,
// flags +0x48, bounding volume +0x4c.
struct ParticleStub {
    void* vtable;
    uint32_t visited;
    uint32_t lastCulledFrame;
    uint32_t count;
    ParticleStub* parent;
    ParticleStub* head;
    ParticleStub* tail;
    ParticleStub* next;
    ParticleStub* prev;
    void* cellNode;
    void* name;
    Vec3 position;
    Vec4 orientation;
    uint8_t flags;
    uint8_t padding[3];
    void* volume;
    uint8_t rest[0x12c - 0x50];
};
static_assert(sizeof(ParticleStub) == 0x12c, "ParticleStub");

enum Kind { K_AABB, K_BS, K_OBB };
static const char* KindName(Kind kind)
{
    return kind == K_AABB ? "aabb" : kind == K_BS ? "bs" : "obb";
}
static int KindSize(Kind kind)
{
    return kind == K_BS ? 0xb4 : 0xdc;
}
static bool IsVtableWord(Kind kind, int offset)
{
    if (offset == 0 || offset == 8) return true;
    return kind == K_BS && offset == 0xa0;
}

static HMODULE mod[2];
static HMODULE kernelStandard;
static unsigned long long checks, bad, nanPayload;
static std::mt19937 rng(0x33b0u);
static const char* currentTest = "";
static const char* currentKind = "";
static int currentRound = 0;
static ParticleStub hostParticle;
static alignas(16) unsigned char object[0xdc];
static alignas(16) unsigned char snapshot[2][0xdc];
static alignas(16) unsigned char camera[2][0x264];
static ViewStub viewStub[2];
static alignas(16) unsigned char fakeKernel[0x200];
static alignas(16) unsigned char fakeManager[0x40];
static void* fakeManagerVtable[8];

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
    // flags select the dirty/recalculation paths the methods take; keep them in the original's lower six bits
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

// A single-precision multiply/subtract pair matching the library's SSE operand order (the AABB extents comparison).
static float MulHalf(float v)
{
    return v * 0.5f;
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
        std::printf("MISMATCH %s (%s,%s,round %d)\n", what, currentKind, currentTest, currentRound);
    }
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
            if (((u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu)) &&
                ((v & 0x7f800000u) == 0x7f800000u && (v & 0x007fffffu))) {
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
static void CheckField(bool ok, const char* what, const char* field)
{
    if (!ok) std::printf("%s field %s\n", what, field);
    Check(ok, what);
}
static void CheckWord(bool ok, const char* what, const char* field, unsigned a, unsigned b)
{
    if (!ok) std::printf("%s field %s ours=%08x orig=%08x\n", what, field, a, b);
    Check(ok, what);
}

// ---------------------------------------------------------------- fake RenderKernel / MaterialManager

// MaterialManager::Create(Material*&, const PString&) returns a ResultClass by value, so it has the hidden result
// pointer after `this` (see tests/diff_rs_u7.cpp). Returning a nonzero code makes RenderPrimitive::Init fail before it
// touches a material, which is enough to compare the bounding-volume side of the renderable methods.
static int* __cdecl FakeCreate(void*, int* result, void** material, const void*)
{
    *material = nullptr;
    *result = 0x40;  // any nonzero MaterialManager::Results code
    return result;
}

static void PrepareFakeKernel()
{
    std::memset(fakeKernel, 0, sizeof fakeKernel);
    std::memset(fakeManager, 0, sizeof fakeManager);
    std::memset(fakeManagerVtable, 0, sizeof fakeManagerVtable);
    fakeManagerVtable[3] = reinterpret_cast<void*>(&FakeCreate);
    *reinterpret_cast<void**>(fakeManager) = fakeManagerVtable;
    *reinterpret_cast<void**>(fakeKernel + 0x90) = fakeManager;
}

// ---------------------------------------------------------------- api

struct Api {
    void(__thiscall* aabbCtor)(void*, ParticleStub*);
    void(__thiscall* bsCtor)(void*, ParticleStub*);
    void(__thiscall* obbCtor)(void*, ParticleStub*);

    void(__thiscall* camCtor)(void*, const Vec3*, const Vec4*);
    int(__cdecl* clip3D)(const void*, const Vec3*, float);

    bool(__cdecl* aabbView)(const void*, const void*);
    bool(__cdecl* aabbCam)(const void*, const void*);
    bool(__cdecl* bsView)(const void*, const void*);
    bool(__cdecl* bsCam)(const void*, const void*);
    bool(__cdecl* obbView)(const void*, const void*);
    bool(__cdecl* obbCam)(const void*, const void*);

    void* (__cdecl* aabbCreate)(const void*, void*, const void*);
    void* (__cdecl* aabbCheck)(const void*, void*, void*);
    void* (__cdecl* bsCreate)(const void*, void*, const void*);
    void* (__cdecl* bsCheck)(const void*, void*, void*);
    void* (__cdecl* obbCreate)(const void*, void*, const void*);
    void* (__cdecl* obbCheck)(const void*, void*, void*);

    void(__thiscall* boxCtor)(void*);
    void(__thiscall* sphereCtor)(void*);
    void* boxVtable;
    void* sphereVtable;
};

static Api api[2];
static void* (__cdecl* allocNew)(unsigned);

static void LoadApi(int side)
{
    Api& a = api[side];
    a.aabbCtor = Proc<decltype(a.aabbCtor)>(side, "??0ParticleAxisAlignedBoundingBox@Jet@@QAE@PAVParticle@1@@Z");
    a.bsCtor = Proc<decltype(a.bsCtor)>(side, "??0ParticleBoundingSphere@Jet@@QAE@PAVParticle@1@@Z");
    a.obbCtor = Proc<decltype(a.obbCtor)>(side, "??0ParticleOrientedBoundingBox@Jet@@QAE@PAVParticle@1@@Z");
    a.camCtor = Proc<decltype(a.camCtor)>(side, "??0Camera@Jet@@QAE@ABVVector3@1@ABVQuaternion@1@@Z");
    a.clip3D = Proc<decltype(a.clip3D)>(side, "?Clip3D@Camera@Jet@@QBA?AW4ClipResult@2@ABVVector3@2@M@Z");
    a.aabbView = Proc<decltype(a.aabbView)>(
        side, "?IsWithinView@ParticleAxisAlignedBoundingBox@Jet@@UBA_NABVViewPoint@2@@Z");
    a.aabbCam = Proc<decltype(a.aabbCam)>(
        side, "?IsWithinView@ParticleAxisAlignedBoundingBox@Jet@@UBA_NPBVCamera@2@@Z");
    a.bsView = Proc<decltype(a.bsView)>(
        side, "?IsWithinView@ParticleBoundingSphere@Jet@@UBA_NABVViewPoint@2@@Z");
    a.bsCam = Proc<decltype(a.bsCam)>(
        side, "?IsWithinView@ParticleBoundingSphere@Jet@@UBA_NPBVCamera@2@@Z");
    a.obbView = Proc<decltype(a.obbView)>(
        side, "?IsWithinView@ParticleOrientedBoundingBox@Jet@@UBA_NABVViewPoint@2@@Z");
    a.obbCam = Proc<decltype(a.obbCam)>(
        side, "?IsWithinView@ParticleOrientedBoundingBox@Jet@@UBA_NPBVCamera@2@@Z");
    a.aabbCreate = Proc<decltype(a.aabbCreate)>(
        side, "?CreateRenderableObject@ParticleAxisAlignedBoundingBox@Jet@@UBAPAVRender3DPrimitive@2@PAVRenderKernel@2@ABVColor@2@@Z");
    a.aabbCheck = Proc<decltype(a.aabbCheck)>(
        side, "?CheckRenderableObjectPosition@ParticleAxisAlignedBoundingBox@Jet@@UBAPAVRender3DPrimitive@2@PAVRenderKernel@2@PAV32@@Z");
    a.bsCreate = Proc<decltype(a.bsCreate)>(
        side, "?CreateRenderableObject@ParticleBoundingSphere@Jet@@UBAPAVRender3DPrimitive@2@PAVRenderKernel@2@ABVColor@2@@Z");
    a.bsCheck = Proc<decltype(a.bsCheck)>(
        side, "?CheckRenderableObjectPosition@ParticleBoundingSphere@Jet@@UBAPAVRender3DPrimitive@2@PAVRenderKernel@2@PAV32@@Z");
    a.obbCreate = Proc<decltype(a.obbCreate)>(
        side, "?CreateRenderableObject@ParticleOrientedBoundingBox@Jet@@UBAPAVRender3DPrimitive@2@PAVRenderKernel@2@ABVColor@2@@Z");
    a.obbCheck = Proc<decltype(a.obbCheck)>(
        side, "?CheckRenderableObjectPosition@ParticleOrientedBoundingBox@Jet@@UBAPAVRender3DPrimitive@2@PAVRenderKernel@2@PAV32@@Z");
    a.boxCtor = Proc<decltype(a.boxCtor)>(side, "??0RenderBox@Jet@@QAE@XZ");
    a.sphereCtor = Proc<decltype(a.sphereCtor)>(side, "??0RenderSphere@Jet@@QAE@XZ");
    a.boxVtable = Vtable(side, "??_7RenderBox@Jet@@6B@");
    a.sphereVtable = Vtable(side, "??_7RenderSphere@Jet@@6B@");
}

// ---------------------------------------------------------------- scene helpers

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

static void* MakePrimitive(int side, bool sphereLike)
{
    const unsigned size = sphereLike ? 0x18c : 0x194;
    void* const memory = allocNew(size);
    if (sphereLike)
        api[side].sphereCtor(memory);
    else
        api[side].boxCtor(memory);
    return memory;
}

static void PrepareObject(int side, Kind kind, const Bv& input)
{
    switch (kind) {
    case K_AABB: api[side].aabbCtor(object, &hostParticle); break;
    case K_BS: api[side].bsCtor(object, &hostParticle); break;
    case K_OBB: api[side].obbCtor(object, &hostParticle); break;
    }
    if (kind == K_AABB) {
        AabbInput derived;
        std::memcpy(&derived.base, &input, sizeof(Bv));
        derived.base.host = &hostParticle;
        derived.localBox = OBox{Vec3{1, 2, 3}, {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}}, Vec3{4, 5, 6}};
        ApplyInput(kind, object, &derived);
    } else if (kind == K_BS) {
        SphereInput derived;
        std::memcpy(&derived.base, &input, sizeof(Bv));
        derived.base.host = &hostParticle;
        derived.localSphere = SphereData{nullptr, Vec3{1, 2, 3}, 7.0f};
        ApplyInput(kind, object, &derived);
    } else {
        AabbInput derived;
        std::memcpy(&derived.base, &input, sizeof(Bv));
        derived.base.host = &hostParticle;
        derived.localBox = OBox{Vec3{1, 2, 3}, {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}}, Vec3{4, 5, 6}};
        ApplyInput(kind, object, &derived);
    }
    hostParticle.volume = object;
}

static void SetVolumeFlags(uint32_t flags)
{
    std::memcpy(object + 4, &flags, 4);
}

static void* CallCheck(int side, Kind kind, void* primitive)
{
    switch (kind) {
    case K_AABB: return api[side].aabbCheck(object, fakeKernel, primitive);
    case K_BS: return api[side].bsCheck(object, fakeKernel, primitive);
    default: return api[side].obbCheck(object, fakeKernel, primitive);
    }
}

static void* CallCreate(int side, Kind kind, const Vec4& color)
{
    switch (kind) {
    case K_AABB: return api[side].aabbCreate(object, fakeKernel, &color);
    case K_BS: return api[side].bsCreate(object, fakeKernel, &color);
    default: return api[side].obbCreate(object, fakeKernel, &color);
    }
}

struct PrimInfo {
    void* vtable;
    void* kernel;
    unsigned char geometry[0x38];  // 0x148..0x180
    uint32_t creationFlags;
    unsigned char flags;           // 0x184
    float shape[3];                // 0x188 (one float for a sphere)
    float particlePosition[3];     // +0x2c
    float particleOrientation[4];  // +0x38
    unsigned char particleFlags;
    uint32_t chunkRefs, chunkCount, materialRefs, materialCount;
};

static void CapturePrimitive(const void* p, Kind kind, PrimInfo& info)
{
    const unsigned char* const b = static_cast<const unsigned char*>(p);
    std::memset(&info, 0, sizeof info);
    std::memcpy(&info.vtable, b, 4);
    std::memcpy(&info.kernel, b + 0x13c, 4);
    std::memcpy(info.geometry, b + 0x148, sizeof info.geometry);
    std::memcpy(&info.creationFlags, b + 0x180, 4);
    info.flags = b[0x184];
    std::memcpy(info.shape, b + 0x188, kind == K_BS ? 4 : sizeof info.shape);
    std::memcpy(info.particlePosition, b + 0x2c, sizeof info.particlePosition);
    std::memcpy(info.particleOrientation, b + 0x38, sizeof info.particleOrientation);
    info.particleFlags = b[0x48];
    const void* const chunkBlock = *reinterpret_cast<const void* const*>(b + 0x140);
    const void* const materialBlock = *reinterpret_cast<const void* const*>(b + 0x144);
    if (chunkBlock) {
        std::memcpy(&info.chunkRefs, chunkBlock, 4);
        std::memcpy(&info.chunkCount, static_cast<const unsigned char*>(chunkBlock) + 4, 4);
    }
    if (materialBlock) {
        std::memcpy(&info.materialRefs, materialBlock, 4);
        std::memcpy(&info.materialCount, static_cast<const unsigned char*>(materialBlock) + 4, 4);
    }
}
 
static bool IsNaNBits(uint32_t u)
{
    return (u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) != 0;
}

static bool SameBitsOrNaN(const void* a, const void* b, size_t n)
{
    const unsigned char* x = static_cast<const unsigned char*>(a);
    const unsigned char* y = static_cast<const unsigned char*>(b);
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
        return false;
    }
    return std::memcmp(x + i, y + i, n - i) == 0;
}

static void ComparePrimitive(Kind kind, const void* a, const void* b, const char* what)
{
    PrimInfo x, y;
    CapturePrimitive(a, kind, x);
    CapturePrimitive(b, kind, y);
    const bool sphereLike = kind == K_BS;
    CheckField(x.vtable == (sphereLike ? api[0].sphereVtable : api[0].boxVtable), what, "vtable-ours");
    CheckField(y.vtable == (sphereLike ? api[1].sphereVtable : api[1].boxVtable), what, "vtable-orig");
    CheckWord(x.kernel == y.kernel, what, "kernel", reinterpret_cast<uintptr_t>(x.kernel),
              reinterpret_cast<uintptr_t>(y.kernel));
    CheckField(std::memcmp(x.geometry, y.geometry, sizeof x.geometry) == 0, what, "geometry");
    CheckWord(x.creationFlags == y.creationFlags, what, "creationFlags", x.creationFlags, y.creationFlags);
    // RenderPrimitive's constructor preserves the upper five bits of flags at +0x184 (`and 0xf8; or 8`), so they are
    // whatever the recycled allocation held; U10's local constructor zeroes them. Only bits 0..3 are defined.
    CheckWord((x.flags & 0x0f) == (y.flags & 0x0f), what, "flags", x.flags & 0x0f, y.flags & 0x0f);
    CheckField(SameBitsOrNaN(x.shape, y.shape, sphereLike ? 4 : sizeof x.shape), what, "shape");
    CheckField(SameBitsOrNaN(x.particlePosition, y.particlePosition, sizeof x.particlePosition), what, "position");
    CheckField(SameBitsOrNaN(x.particleOrientation, y.particleOrientation, sizeof x.particleOrientation), what,
               "orientation");
    CheckField(x.chunkRefs == y.chunkRefs && x.chunkCount == y.chunkCount, what, "chunks");
    CheckField(x.materialRefs == y.materialRefs && x.materialCount == y.materialCount, what, "materials");
}

// ---------------------------------------------------------------- test body

static void TestRound(int round)
{
    const bool awkward = (round % 7) == 0;
    hostParticle.position = RandomVec(awkward);
    hostParticle.orientation = RandomQuat(awkward);
    hostParticle.parent = nullptr;
    hostParticle.count = 0;
    hostParticle.head = nullptr;
    hostParticle.tail = nullptr;
    hostParticle.next = nullptr;
    hostParticle.prev = nullptr;

    const Vec3 camPosition = RandomVec(awkward);
    const Vec4 camOrientation = RandomQuat(awkward);
    const Vec4 color{FRange(0.0f, 1.0f), FRange(0.0f, 1.0f), FRange(0.0f, 1.0f), FRange(0.0f, 1.0f)};

    static const Kind kinds[3] = {K_AABB, K_BS, K_OBB};
    for (Kind kind : kinds) {
        currentKind = KindName(kind);
        Bv input;
        RandomBv(input, awkward);

        bool viewResult[2][3] = {};
        void* created[2] = {};
        void* checked[2] = {};

        const char* const viewNames[3] = {"view/camera", "view/null", "view/no-dirty"};
        for (int side = 0; side < 2; ++side) {
            PrepareObject(side, kind, input);
            hostParticle.volume = object;
            api[side].camCtor(camera[side], &camPosition, &camOrientation);
            std::memset(&viewStub[side], 0, sizeof viewStub[side]);
            viewStub[side].camera = camera[side];
            currentTest = viewNames[0];
            switch (kind) {
            case K_AABB:
                viewResult[side][0] = api[side].aabbView(object, &viewStub[side]);
                break;
            case K_BS:
                viewResult[side][0] = api[side].bsView(object, &viewStub[side]);
                break;
            default:
                viewResult[side][0] = api[side].obbView(object, &viewStub[side]);
                break;
            }
            std::memcpy(snapshot[side], object, KindSize(kind));
        }
        Check(viewResult[0][0] == viewResult[1][0], "IsWithinView(ViewPoint) result");
        CheckObject(kind, snapshot[0], snapshot[1], "IsWithinView(ViewPoint) state");

        for (int side = 0; side < 2; ++side) {
            PrepareObject(side, kind, input);
            hostParticle.volume = object;
            api[side].camCtor(camera[side], &camPosition, &camOrientation);
            viewStub[side].camera = nullptr;
            currentTest = viewNames[1];
            switch (kind) {
            case K_AABB:
                viewResult[side][1] = api[side].aabbView(object, &viewStub[side]);
                break;
            case K_BS:
                viewResult[side][1] = api[side].bsView(object, &viewStub[side]);
                break;
            default:
                viewResult[side][1] = api[side].obbView(object, &viewStub[side]);
                break;
            }
            std::memcpy(snapshot[side], object, KindSize(kind));
        }
        Check(viewResult[0][1] == viewResult[1][1], "IsWithinView(null view) result");
        Check(viewResult[0][1] == false && viewResult[1][1] == false, "IsWithinView(null view) false");
        CheckObject(kind, snapshot[0], snapshot[1], "IsWithinView(null view) state");

        for (int side = 0; side < 2; ++side) {
            PrepareObject(side, kind, input);
            hostParticle.volume = object;
            api[side].camCtor(camera[side], &camPosition, &camOrientation);
            currentTest = viewNames[0];
            switch (kind) {
            case K_AABB:
                viewResult[side][2] = api[side].aabbCam(object, camera[side]);
                break;
            case K_BS:
                viewResult[side][2] = api[side].bsCam(object, camera[side]);
                break;
            default:
                viewResult[side][2] = api[side].obbCam(object, camera[side]);
                break;
            }
            std::memcpy(snapshot[side], object, KindSize(kind));
        }
        Check(viewResult[0][2] == viewResult[1][2], "IsWithinView(Camera) result");
        if (viewResult[0][2] != viewResult[1][2]) {
            const Bv* const a = reinterpret_cast<const Bv*>(snapshot[0]);
            const Bv* const b = reinterpret_cast<const Bv*>(snapshot[1]);
            std::printf("  sphere ours r=%08x c=(%08x %08x %08x) orig r=%08x c=(%08x %08x %08x)\n",
                        *reinterpret_cast<const uint32_t*>(&a->sphere.radius),
                        *reinterpret_cast<const uint32_t*>(&a->globalSphereCenter.x),
                        *reinterpret_cast<const uint32_t*>(&a->globalSphereCenter.y),
                        *reinterpret_cast<const uint32_t*>(&a->globalSphereCenter.z),
                        *reinterpret_cast<const uint32_t*>(&b->sphere.radius),
                        *reinterpret_cast<const uint32_t*>(&b->globalSphereCenter.x),
                        *reinterpret_cast<const uint32_t*>(&b->globalSphereCenter.y),
                        *reinterpret_cast<const uint32_t*>(&b->globalSphereCenter.z));
            alignas(16) unsigned char camCopy[2][0x264];
            for (int srcCam = 0; srcCam < 2; ++srcCam) {
                std::memcpy(camCopy[0], camera[srcCam], sizeof camCopy[0]);
                std::memcpy(camCopy[1], camera[srcCam], sizeof camCopy[1]);
                const int r0 = api[0].clip3D(camCopy[0], &a->globalSphereCenter, a->sphere.radius);
                const int r1 = api[1].clip3D(camCopy[1], &a->globalSphereCenter, a->sphere.radius);
                std::printf("  clip3D(cam%d) ours=%d orig=%d frustumSame=%d\n", srcCam, r0, r1,
                            std::memcmp(camCopy[0] + 0x150, camCopy[1] + 0x150, 0x60) == 0);
            }
        }
        CheckObject(kind, snapshot[0], snapshot[1], "IsWithinView(Camera) state");

        // second call with the cone already up to date (the original then skips UpdateViewCone)
        bool second[2];
        for (int side = 0; side < 2; ++side) {
            currentTest = viewNames[2];
            switch (kind) {
            case K_AABB: second[side] = api[side].aabbCam(object, camera[side]); break;
            case K_BS: second[side] = api[side].bsCam(object, camera[side]); break;
            default: second[side] = api[side].obbCam(object, camera[side]); break;
            }
            Check(second[side] == viewResult[side][2], "IsWithinView(Camera) repeat");
        }
        Check(second[0] == second[1], "IsWithinView(Camera) repeat compare");

        // CreateRenderableObject with the fake kernel (Init stops at the material manager)
        for (int side = 0; side < 2; ++side) {
            PrepareObject(side, kind, input);
            hostParticle.volume = object;
            currentTest = "create";
            created[side] = CallCreate(side, kind, color);
            std::memcpy(snapshot[side], object, KindSize(kind));
        }
        CheckObject(kind, snapshot[0], snapshot[1], "CreateRenderableObject volume state");
        ComparePrimitive(kind, created[0], created[1], "CreateRenderableObject primitive");

        // CheckRenderableObjectPosition, matching size: the original keeps the object and only moves it
        void* oldPrimitive[2] = {};
        for (int side = 0; side < 2; ++side) {
            PrepareObject(side, kind, input);
            hostParticle.volume = object;
            SetVolumeFlags(0);
            const bool sphereLike = kind == K_BS;
            void* const prim = MakePrimitive(side, sphereLike);
            const Bv* const volume = reinterpret_cast<const Bv*>(object);
            if (sphereLike) {
                const float radius = volume->sphere.radius;
                std::memcpy(static_cast<unsigned char*>(prim) + 0x188, &radius, 4);
            } else if (kind == K_AABB) {
                const float e[3] = {MulHalf(volume->box.max.x - volume->box.min.x),
                                    MulHalf(volume->box.max.y - volume->box.min.y),
                                    MulHalf(volume->box.max.z - volume->box.min.z)};
                std::memcpy(static_cast<unsigned char*>(prim) + 0x188, e, sizeof e);
            } else {
                std::memcpy(static_cast<unsigned char*>(prim) + 0x188, &volume->obb.extents, 0xc);
            }
            std::memset(static_cast<unsigned char*>(prim) + 0x170, 0x00, 0x10);
            const float oldColor[4] = {0.25f, 0.5f, 0.75f, 1.0f};
            std::memcpy(static_cast<unsigned char*>(prim) + 0x170, oldColor, sizeof oldColor);  // copied by a rebuild
            *reinterpret_cast<void**>(static_cast<unsigned char*>(prim) + 0x13c) = fakeKernel;
            oldPrimitive[side] = prim;
            currentTest = "check-same";
            checked[side] = CallCheck(side, kind, prim);
        }
        Check(checked[0] == oldPrimitive[0] && checked[1] == oldPrimitive[1], "Check keeps a matching primitive");
        ComparePrimitive(kind, checked[0], checked[1], "Check matching primitive");

        // different size: the old primitive is deleted and a new one built with the volume's size/centre/orientation
        for (int side = 0; side < 2; ++side) {
            PrepareObject(side, kind, input);
            hostParticle.volume = object;
            SetVolumeFlags(kind == K_AABB ? 0x08 : kind == K_BS ? 0x03 : 0x27);
            const bool sphereLike = kind == K_BS;
            void* const prim = MakePrimitive(side, sphereLike);
            if (sphereLike) {
                const float wrongRadius = -12345.0f;
                std::memcpy(static_cast<unsigned char*>(prim) + 0x188, &wrongRadius, 4);
            } else {
                const float wrong[3] = {-12345.0f, 54321.0f, 0.5f};
                std::memcpy(static_cast<unsigned char*>(prim) + 0x188, wrong, sizeof wrong);
            }
            std::memset(static_cast<unsigned char*>(prim) + 0x170, 0x00, 0x10);
            const float oldColor[4] = {0.125f, 0.375f, 0.625f, 0.875f};
            std::memcpy(static_cast<unsigned char*>(prim) + 0x170, oldColor, sizeof oldColor);  // copied by a rebuild
            *reinterpret_cast<void**>(static_cast<unsigned char*>(prim) + 0x13c) = fakeKernel;
            oldPrimitive[side] = prim;
            currentTest = "check-diff";
            checked[side] = CallCheck(side, kind, prim);
            std::memcpy(snapshot[side], object, KindSize(kind));
        }
        // The allocator can hand the just-freed block back, so the rebuild is identified by the new state, not by the
        // pointer: Init stops before the geometry but has already stored the colour and creation flags, and the extents
        // stay at the constructor's zero.
        PrimInfo rebuilt[2];
        CapturePrimitive(checked[0], kind, rebuilt[0]);
        CapturePrimitive(checked[1], kind, rebuilt[1]);
        const int shapeWords = kind == K_BS ? 1 : 3;
        for (int s = 0; s < 2; ++s) {
            const Bv* const volume = reinterpret_cast<const Bv*>(snapshot[s]);
            Check(rebuilt[s].creationFlags == 4u, "Check rebuilt primitive creation flags");
            if (kind == K_BS)
                Check(rebuilt[s].shape[0] == volume->sphere.radius, "Check rebuilt primitive size");
            else
                for (int i = 0; i < shapeWords; ++i) Check(rebuilt[s].shape[i] == 0.0f, "Check rebuilt primitive size");
        }
        CheckObject(kind, snapshot[0], snapshot[1], "Check rebuilding volume state");
        ComparePrimitive(kind, checked[0], checked[1], "Check rebuilt primitive");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u3b <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 500;
    SetDllDirectoryA(argv[1]);
    const std::string dir = argv[1];
    LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str());
    mod[0] = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    mod[1] = GetModuleHandleA("Render3DCore_7c_orig.dll");
    kernelStandard = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!mod[0] || !mod[1] || !kernelStandard) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    allocNew = reinterpret_cast<decltype(allocNew)>(GetProcAddress(kernelStandard, "?New@Alloc@Jet@@SAPAXI@Z"));
    if (!allocNew) {
        std::printf("missing KernelStandard Alloc exports\n");
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    LoadApi(0);
    LoadApi(1);
    PrepareFakeKernel();
    std::memset(&hostParticle, 0, sizeof hostParticle);

    for (int round = 0; round < rounds; ++round) {
        currentRound = round;
        currentTest = "round";
        TestRound(round);
    }

    std::printf("%llu checks, %llu mismatches, %llu NaN-payload-only differences\n", checks, bad, nanPayload);
    return bad ? 1 : 0;
}
