// Differential test of Render3DCore_7c unit U8: our Camera, ViewPort and DebugViewPort against
// Render3DCore_7c_orig.dll in one process.
//
// Objects are built by each module's own constructors and driven through the same calls; every camera/viewport
// field outside the vftable, every returned float (raw bits), vector, matrix, frustum plane and Rect is compared.
// Float rounds include signed zero, infinities and quiet/signalling NaNs. ViewPort needs a live LayerManager, a
// RenderKernel with an OS at +0x8c and an Options object; the test builds those from the local RenderSystem and
// KernelStandard DLLs.
//
// usage: diff_r3d_u8.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

struct Vec2 {
    float x, y;
};
struct Vec3 {
    float x, y, z;
};
struct Quat {
    float x, y, z, w;
};
struct Mat3 {
    float m[9];
};
struct Mat4 {
    float m[16];
};
struct Plane {
    Vec3 normal;
    float d;
};
struct Rect {
    int x, y, w, h;
};
struct Color {
    float r, g, b, a;
};
struct Point {
    int x, y;
};

static_assert(sizeof(Vec3) == 0xc, "Vec3");
static_assert(sizeof(Quat) == 0x10, "Quat");
static_assert(sizeof(Mat3) == 0x24, "Mat3");
static_assert(sizeof(Mat4) == 0x40, "Mat4");
static_assert(sizeof(Plane) == 0x10, "Plane");
static_assert(sizeof(Rect) == 0x10, "Rect");

static const unsigned kCameraSize = 0x264;
static const unsigned kViewPortSize = 0xf4;
static const unsigned kDebugViewPortSize = 0xfc;

static HMODULE mod[2];
static HMODULE renderSystem;
static void* sharedLayerManagerSlot;
static void* g_fakeKernel;
static void* __cdecl GetManagerKernelStub(const void*)
{
    return g_fakeKernel;
}
static unsigned long long checks, bad, nanPayload;
static std::mt19937 rng(0x5eed);

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
    switch (Pick(0, 11)) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: return Bits(0x7f800000u);
    case 3: return Bits(0xff800000u);
    case 4: return Bits(0x7fc00000u);
    case 5: return Bits(0xffc00001u);
    case 6: return Bits(0x7fa00000u);
    default: return FRange(-4.0f, 4.0f);
    }
}
static Vec3 RandomVec(bool awkward, float lo = -10.0f, float hi = 10.0f)
{
    return Vec3{awkward ? Awkward() : FRange(lo, hi), awkward ? Awkward() : FRange(lo, hi),
                awkward ? Awkward() : FRange(lo, hi)};
}
static Quat RandomQuat(bool awkward)
{
    Quat q{awkward ? Awkward() : FRange(-1.0f, 1.0f), awkward ? Awkward() : FRange(-1.0f, 1.0f),
           awkward ? Awkward() : FRange(-1.0f, 1.0f), awkward ? Awkward() : FRange(-1.0f, 1.0f)};
    if (!awkward) {
        const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        if (len > 1e-6f) {
            q.x /= len;
            q.y /= len;
            q.z /= len;
            q.w /= len;
        }
    }
    return q;
}
static Mat3 RandomMat3(bool awkward)
{
    Mat3 m;
    for (float& f : m.m) f = awkward ? Awkward() : FRange(-1.0f, 1.0f);
    return m;
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

template <class T>
static T MemberProc(int side, const char* name)
{
    FARPROC raw = GetProcAddress(mod[side], name);
    if (!raw) {
        std::printf("missing export %s in %s\n", name, side ? "orig" : "ours");
        std::exit(2);
    }
    T fn{};
    static_assert(sizeof fn == sizeof raw, "member pointer size");
    std::memcpy(&fn, &raw, sizeof fn);
    return fn;
}

static void Check(bool ok, const char* what, int round)
{
    ++checks;
    if (!ok) {
        ++bad;
        if (bad < 25) std::printf("MISMATCH %s (round %d)\n", what, round);
    }
}

static uint32_t FBits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

static bool SameFloats(const float* a, const float* b, int n)
{
    return std::memcmp(a, b, sizeof(float) * static_cast<size_t>(n)) == 0;
}

static void CheckFloats(const float* a, const float* b, int n, const char* what, int round)
{
    ++checks;
    bool ok = true;
    for (int i = 0; i < n; ++i) {
        const uint32_t u = FBits(a[i]), v = FBits(b[i]);
        if (u == v) continue;
        const bool un = (u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) != 0;
        const bool vn = (v & 0x7f800000u) == 0x7f800000u && (v & 0x007fffffu) != 0;
        if (un && vn) {
            ++nanPayload;
            continue;
        }
        ok = false;
        if (bad < 25) {
            std::printf("MISMATCH %s (round %d):", what, round);
            for (int j = i; j < n; ++j)
                if (FBits(a[j]) != FBits(b[j]) && !((FBits(a[j]) & 0x7f800000u) == 0x7f800000u &&
                                                    (FBits(a[j]) & 0x007fffffu) &&
                                                    (FBits(b[j]) & 0x7f800000u) == 0x7f800000u &&
                                                    (FBits(b[j]) & 0x007fffffu)))
                    std::printf(" [%d] ours %08x orig %08x", j, FBits(a[j]), FBits(b[j]));
            std::printf("\n");
        }
        break;
    }
    if (!ok) ++bad;
}

struct CameraObj {
    unsigned char bytes[kCameraSize];
};
struct ViewPortObj {
    unsigned char bytes[kViewPortSize];
};
struct DebugViewPortObj {
    unsigned char bytes[kDebugViewPortSize];
};

struct CameraObjMem {
    unsigned char bytes[kCameraSize];
};

struct CameraApi {
    void(__thiscall* ctor0)(void*);
    void(__thiscall* ctor3)(void*, float, float, float);
    void(__thiscall* ctorPosQuat)(void*, const Vec3*, const Quat*);
    void(__thiscall* ctorPosMat3)(void*, const Vec3*, const Mat3*);
    void(__thiscall* ctorPosEuler)(void*, const Vec3*, float, float, float);
    void(__thiscall* ctorPosQuat3)(void*, const Vec3*, const Quat*, float, float, float);
    void(__thiscall* ctorPosMat33)(void*, const Vec3*, const Mat3*, float, float, float);
    void(__thiscall* ctorPosEuler3)(void*, const Vec3*, float, float, float, float, float, float);
    void(__thiscall* ctorCopy)(void*, const void*);
    void(__thiscall* drop)(void*);
    void(__thiscall* assign)(void*, const void*);
    void(__cdecl* setPosition)(void*, const Vec3*);
    void(__cdecl* setOrientation)(void*, const Quat*);
    void(__cdecl* setReferenceFrame)(void*, const Vec3*, const Quat*);
    void(__cdecl* setScene)(void*, void*);
    void(__cdecl* setNear)(void*, float);
    void(__cdecl* setFar)(void*, float);
    void(__cdecl* setPerspective)(void*, float, float);
    void(__cdecl* setTransition)(void*, float, float, float, float);
    void(__cdecl* setOrthographic)(void*, float, float);
    void(__thiscall* setProjectionData)(void*, const void*);
    void(__cdecl* resetViewCone)(void*);
    void(__cdecl* updateCached)(void*);
    void(__cdecl* translate)(void*, const Vec3*);
    void(__cdecl* rotateM)(void*, const Mat3*);
    void(__cdecl* rotateQ)(void*, const Quat*);
    void(__cdecl* selfRotateM)(void*, const Mat3*);
    void(__cdecl* selfRotateQ)(void*, const Quat*);
    void(__cdecl* alignToTarget)(void*, const Vec3*, const Vec3*);
    void(__cdecl* orbit)(void*, const Vec3*, float, float, const Vec3*);
    void(__cdecl* perspective)(void*, Vec3*);
    void(__cdecl* trainzUnPerspective)(void*, float, float, float, Vec3*);
    bool(__cdecl* transform2D)(void*, Vec3*, float);
    bool(__thiscall* trainzTransform2D)(void*, Vec3*, float);
    bool(__cdecl* bigEnough)(void*, const Vec3*, float);
    int(__cdecl* clip3DSphere)(void*, const Vec3*, float);
    int(__cdecl* clip3DPoints)(void*, const Vec3*, int);
    int(__cdecl* clipNear)(void*, Vec3*);
    float(__cdecl* getNear)(void*);
    float(__cdecl* getFar)(void*);
    float(__cdecl* getNearWidth)(void*);
    float(__cdecl* getNearHeight)(void*);
    float(__cdecl* getHorizontalFov)(void*);
    float(__cdecl* getVerticalFov)(void*);
    float(__cdecl* getPitch)(void*);
    float(__cdecl* getYaw)(void*);
    float(__cdecl* getRoll)(void*);
    float(__thiscall* getRelevantWidth)(void*);
    float(__thiscall* getRelevantHeight)(void*);
    bool(__thiscall* isReflectionCull)(void*);
    int(__cdecl* getProjectionMode)(void*);
    const Plane*(__cdecl* getFrustum)(void*);
    const Mat3*(__cdecl* getCachedOrientation)(void*);
    const Mat4*(__cdecl* getViewTransform)(void*);
    const Mat4*(__cdecl* getCellSpaceViewTransform)(void*);
    const Mat4*(__cdecl* getGlobalViewTransform)(void*);
    Vec3 (__cdecl CameraObjMem::*getRight)() const;
    Vec3 (__cdecl CameraObjMem::*getUp)() const;
    Vec3 (__cdecl CameraObjMem::*getForward)() const;
    Mat4 (__cdecl CameraObjMem::*getPerspectiveTransform)() const;
    void(__cdecl* getClippingPlanes)(void*, float, float, float, float, void*);
};

static CameraApi LoadCamera(int side)
{
    CameraApi a{};
    a.ctor0 = Proc<decltype(a.ctor0)>(side, "??0Camera@Jet@@QAE@XZ");
    a.ctor3 = Proc<decltype(a.ctor3)>(side, "??0Camera@Jet@@QAE@MMM@Z");
    a.ctorPosQuat = Proc<decltype(a.ctorPosQuat)>(side, "??0Camera@Jet@@QAE@ABVVector3@1@ABVQuaternion@1@@Z");
    a.ctorPosMat3 = Proc<decltype(a.ctorPosMat3)>(side, "??0Camera@Jet@@QAE@ABVVector3@1@ABVMatrix3@1@@Z");
    a.ctorPosEuler = Proc<decltype(a.ctorPosEuler)>(side, "??0Camera@Jet@@QAE@ABVVector3@1@MMM@Z");
    a.ctorPosQuat3 = Proc<decltype(a.ctorPosQuat3)>(side, "??0Camera@Jet@@QAE@ABVVector3@1@ABVQuaternion@1@MMM@Z");
    a.ctorPosMat33 = Proc<decltype(a.ctorPosMat33)>(side, "??0Camera@Jet@@QAE@ABVVector3@1@ABVMatrix3@1@MMM@Z");
    a.ctorPosEuler3 =
        Proc<decltype(a.ctorPosEuler3)>(side, "??0Camera@Jet@@QAE@ABVVector3@1@MMMMMM@Z");
    a.ctorCopy = Proc<decltype(a.ctorCopy)>(side, "??0Camera@Jet@@QAE@ABV01@@Z");
    a.drop = Proc<decltype(a.drop)>(side, "??1Camera@Jet@@UAE@XZ");
    a.assign = Proc<decltype(a.assign)>(side, "??4Camera@Jet@@QAEAAV01@ABV01@@Z");
    a.setPosition = Proc<decltype(a.setPosition)>(side, "?SetPosition@Camera@Jet@@UAAXABVVector3@2@@Z");
    a.setOrientation = Proc<decltype(a.setOrientation)>(side, "?SetOrientation@Camera@Jet@@UAAXABVQuaternion@2@@Z");
    a.setReferenceFrame =
        Proc<decltype(a.setReferenceFrame)>(side, "?SetReferenceFrame@Camera@Jet@@UAAXABVVector3@2@ABVQuaternion@2@@Z");
    a.setScene = Proc<decltype(a.setScene)>(side, "?SetScene@Camera@Jet@@QAAXPAVElement@2@@Z");
    a.setNear = Proc<decltype(a.setNear)>(side, "?SetNearDistance@Camera@Jet@@QAAXM@Z");
    a.setFar = Proc<decltype(a.setFar)>(side, "?SetFarDistance@Camera@Jet@@QAAXM@Z");
    a.setPerspective = Proc<decltype(a.setPerspective)>(side, "?SetPerspective@Camera@Jet@@QAAXMM@Z");
    a.setTransition = Proc<decltype(a.setTransition)>(side, "?SetTransition@Camera@Jet@@QAAXMMMM@Z");
    a.setOrthographic = Proc<decltype(a.setOrthographic)>(side, "?SetOrthographic@Camera@Jet@@QAAXMM@Z");
    a.setProjectionData = Proc<decltype(a.setProjectionData)>(side, "?SetProjectionData@Camera@Jet@@QAEXABV12@@Z");
    a.resetViewCone = Proc<decltype(a.resetViewCone)>(side, "?ResetViewCone@Camera@Jet@@UAAXXZ");
    a.updateCached = Proc<decltype(a.updateCached)>(side, "?UpdateCachedViewTransform@Camera@Jet@@IBAXXZ");
    a.translate = Proc<decltype(a.translate)>(side, "?Translate@Camera@Jet@@UAAXABVVector3@2@@Z");
    a.rotateM = Proc<decltype(a.rotateM)>(side, "?Rotate@Camera@Jet@@UAAXABVMatrix3@2@@Z");
    a.rotateQ = Proc<decltype(a.rotateQ)>(side, "?Rotate@Camera@Jet@@UAAXABVQuaternion@2@@Z");
    a.selfRotateM = Proc<decltype(a.selfRotateM)>(side, "?SelfRotate@Camera@Jet@@UAAXABVMatrix3@2@@Z");
    a.selfRotateQ = Proc<decltype(a.selfRotateQ)>(side, "?SelfRotate@Camera@Jet@@UAAXABVQuaternion@2@@Z");
    a.alignToTarget = Proc<decltype(a.alignToTarget)>(side, "?AlignToTarget@Camera@Jet@@QAAXABVVector3@2@0@Z");
    a.orbit = Proc<decltype(a.orbit)>(side, "?Orbit@Camera@Jet@@QAAXABVVector3@2@MM0@Z");
    a.perspective = Proc<decltype(a.perspective)>(side, "?Perspective@Camera@Jet@@QBAXAAVVector3@2@@Z");
    a.trainzUnPerspective =
        Proc<decltype(a.trainzUnPerspective)>(side, "?TrainzUnPerspective@Camera@Jet@@QBAXMMMAAVVector3@2@@Z");
    a.transform2D = Proc<decltype(a.transform2D)>(side, "?Transform2D@Camera@Jet@@QBA_NAAVVector3@2@M@Z");
    a.trainzTransform2D =
        Proc<decltype(a.trainzTransform2D)>(side, "?TrainzTransform2D@Camera@Jet@@QBE_NAAVVector3@2@M@Z");
    a.bigEnough = Proc<decltype(a.bigEnough)>(side, "?BigEnough@Camera@Jet@@QBA_NABVVector3@2@M@Z");
    a.clip3DSphere = Proc<decltype(a.clip3DSphere)>(side, "?Clip3D@Camera@Jet@@QBA?AW4ClipResult@2@ABVVector3@2@M@Z");
    a.clip3DPoints = Proc<decltype(a.clip3DPoints)>(side, "?Clip3D@Camera@Jet@@QBA?AW4ClipResult@2@PBVVector3@2@H@Z");
    a.clipNear = Proc<decltype(a.clipNear)>(side, "?ClipNear@Camera@Jet@@QBA?AW4ClipResult@2@AAVVector3@2@@Z");
    a.getNear = Proc<decltype(a.getNear)>(side, "?GetNearClipDistance@Camera@Jet@@QBAMXZ");
    a.getFar = Proc<decltype(a.getFar)>(side, "?GetFarClipDistance@Camera@Jet@@QBAMXZ");
    a.getNearWidth = Proc<decltype(a.getNearWidth)>(side, "?GetNearClipWidth@Camera@Jet@@QBAMXZ");
    a.getNearHeight = Proc<decltype(a.getNearHeight)>(side, "?GetNearClipHeight@Camera@Jet@@QBAMXZ");
    a.getHorizontalFov = Proc<decltype(a.getHorizontalFov)>(side, "?GetHorizontalFOV@Camera@Jet@@QBAMXZ");
    a.getVerticalFov = Proc<decltype(a.getVerticalFov)>(side, "?GetVerticalFOV@Camera@Jet@@QBAMXZ");
    a.getPitch = Proc<decltype(a.getPitch)>(side, "?GetPitch@Camera@Jet@@QBAMXZ");
    a.getYaw = Proc<decltype(a.getYaw)>(side, "?GetYaw@Camera@Jet@@QBAMXZ");
    a.getRoll = Proc<decltype(a.getRoll)>(side, "?GetRoll@Camera@Jet@@QBAMXZ");
    a.getRelevantWidth = Proc<decltype(a.getRelevantWidth)>(side, "?GetRelevantWidth@Camera@Jet@@IBEMXZ");
    a.getRelevantHeight = Proc<decltype(a.getRelevantHeight)>(side, "?GetRelevantHeight@Camera@Jet@@IBEMXZ");
    a.isReflectionCull = Proc<decltype(a.isReflectionCull)>(side, "?IsReflectionCull@Camera@Jet@@QBE_NXZ");
    a.getProjectionMode = Proc<decltype(a.getProjectionMode)>(side, "?GetProjectionMode@Camera@Jet@@QBA?AW4ProjectionMode@12@XZ");
    a.getFrustum = Proc<decltype(a.getFrustum)>(side, "?GetFrustum@Camera@Jet@@QAAPAVPlane@2@XZ");
    a.getCachedOrientation =
        Proc<decltype(a.getCachedOrientation)>(side, "?GetCachedOrientation@Camera@Jet@@QBAABVMatrix3@2@XZ");
    a.getViewTransform = Proc<decltype(a.getViewTransform)>(side, "?GetViewTransform@Camera@Jet@@QBAABVMatrix4@2@XZ");
    a.getCellSpaceViewTransform =
        Proc<decltype(a.getCellSpaceViewTransform)>(side, "?GetCellSpaceViewTransform@Camera@Jet@@QBAABVMatrix4@2@XZ");
    a.getGlobalViewTransform =
        Proc<decltype(a.getGlobalViewTransform)>(side, "?GetGlobalViewTransform@Camera@Jet@@UBAABVMatrix4@2@XZ");
    a.getRight = MemberProc<decltype(a.getRight)>(side, "?GetRight@Camera@Jet@@QBA?AVVector3@2@XZ");
    a.getUp = MemberProc<decltype(a.getUp)>(side, "?GetUp@Camera@Jet@@QBA?AVVector3@2@XZ");
    a.getForward = MemberProc<decltype(a.getForward)>(side, "?GetForward@Camera@Jet@@QBA?AVVector3@2@XZ");
    a.getPerspectiveTransform =
        MemberProc<decltype(a.getPerspectiveTransform)>(side, "?GetPerspectiveTransform@Camera@Jet@@QBA?AVMatrix4@2@XZ");
    a.getClippingPlanes =
        Proc<decltype(a.getClippingPlanes)>(side, "?GetClippingPlanes@Camera@Jet@@QBAXMMMMAAV?$DynamicArray@VPlane@Jet@@$0BA@U?$Less@VPlane@Jet@@V12@@2@@2@@Z");
    return a;
}

static void ClearVolumeFlags(void* object)
{
    uint32_t p;
    std::memcpy(&p, static_cast<unsigned char*>(object) + 0x4c, 4);
    if (p) *reinterpret_cast<unsigned int*>(static_cast<uintptr_t>(p) + 4) = 0;
}

static void CompareBoundingVolume(const CameraObj* a, const CameraObj* b, const char* what, int round)
{
    uint32_t pa, pb;
    std::memcpy(&pa, a->bytes + 0x4c, 4);
    std::memcpy(&pb, b->bytes + 0x4c, 4);
    const unsigned char* x = reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(pa));
    const unsigned char* y = reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(pb));
    bool ok = true;
    for (int off = 4; off < 0xa0; off += 4) {
        if (off == 0x08 || off == 0x98) continue;  // Sphere vftable; host pointer
        uint32_t u, v;
        std::memcpy(&u, x + off, 4);
        std::memcpy(&v, y + off, 4);
        if (u == v) continue;
        const bool un = (u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) != 0;
        const bool vn = (v & 0x7f800000u) == 0x7f800000u && (v & 0x007fffffu) != 0;
        if (un && vn) {
            ++nanPayload;
            continue;
        }
        if (bad < 25)
            std::printf("MISMATCH %s (round %d) volume +0x%x ours %08x orig %08x\n", what, round, off, u, v);
        ok = false;
        break;
    }
    Check(ok, what, round);
}

static void CompareCamera(const CameraObj* a, const CameraObj* b, const char* what, int round)
{
    CheckFloats(reinterpret_cast<const float*>(a->bytes + 0x2c), reinterpret_cast<const float*>(b->bytes + 0x2c), 7,
                what, round);
    CheckFloats(reinterpret_cast<const float*>(a->bytes + 0x12c), reinterpret_cast<const float*>(b->bytes + 0x12c),
                (0x264 - 0x12c) / 4, what, round);
    CompareBoundingVolume(a, b, what, round);
}

struct PlaneList {
    void* head;
};


struct ViewPortApi {
    void(__thiscall* ctor0)(void*);
    void(__thiscall* ctorCamGroup)(void*, void*, void*);
    void(__thiscall* ctorCamRectGroup)(void*, void*, const Rect*, void*);
    void(__thiscall* ctorCopy)(void*, const void*);
    void(__thiscall* drop)(void*);
    void(__thiscall* assign)(void*, const void*);
    void(__cdecl* setLocalPosition)(void*, int, int);
    void(__cdecl* getLocalPosition)(void*, int*, int*);
    void(__cdecl* getPosition)(void*, int*, int*);
    void(__cdecl* setSize)(void*, int, int);
    void(__cdecl* getSize)(void*, int*, int*);
    void(__cdecl* setLetterBox)(void*, bool);
    void(__cdecl* translate)(void*, int, int);
    void(__cdecl* scale1)(void*, float);
    void(__cdecl* scale2)(void*, float, float);
    void(__cdecl* transform)(void*, Vec3*);
    void(__cdecl* viewTransformPoint)(void*, Vec3*);
    void(__cdecl* setClearColor)(void*, const Color*);
    void(__cdecl* setClearDepth)(void*, float);
    void(__cdecl* setClearStencil)(void*, long);
    void(__cdecl* setClearMode)(void*, unsigned long);
    void(__cdecl* setUserData)(void*, unsigned long);
    void(__cdecl* setRenderHint)(void*, int);
    void(__cdecl* setRenderFlags)(void*, unsigned long);
    void(__cdecl* setWindowViewport)(void*, bool);
    void(__cdecl* setAlphaMaskReference)(void*, float);
    void(__cdecl* setCamera)(void*, void*);
    const Rect*(__cdecl* getLocalExtents)(void*);
    void(__cdecl* getExtents)(void*, Rect*);
    int(__cdecl* getLocalX)(void*);
    int(__cdecl* getX)(void*);
    int(__cdecl* getLocalY)(void*);
    int(__cdecl* getY)(void*);
    int(__cdecl* getWidth)(void*);
    int(__cdecl* getHeight)(void*);
    void*(__cdecl* getCamera)(void*);
    bool(__cdecl* getLetterBox)(void*);
    void(__cdecl* adjustAspectRatio)(void*, float*, float*);
    unsigned long(__cdecl* getUserData)(void*);
    int(__cdecl* getRenderHint)(void*);
    unsigned long(__cdecl* getRenderFlags)(void*);
    unsigned long(__cdecl* getViewportNumber)(void*);
    bool(__cdecl* getWindowViewport)(void*);
    float(__cdecl* getAlphaMaskReference)(void*);
    unsigned long(__cdecl* getTraversalNumber)();
    const Color*(__cdecl* getClearColor)(void*);
    float(__cdecl* getClearDepth)(void*);
    unsigned long(__cdecl* getClearStencil)(void*);
    unsigned long(__cdecl* getClearMode)(void*);
    bool(__cdecl* collides1)(void*, const Vec2*);
    bool(__cdecl* collides2)(void*, const Vec2*, const Vec2*);
    void(__cdecl* clear)(void*);
    const void*(__cdecl* getName)(void*);
};

struct DebugViewPortApi {
    void(__thiscall* ctor)(void*, void*);
    void(__thiscall* ctorCopy)(void*, const void*);
    void(__thiscall* drop)(void*);
    void(__thiscall* assign)(void*, const void*);
    void*(__cdecl* getViewPort)(void*);
};

static ViewPortApi LoadViewPort(int side)
{
    ViewPortApi a{};
    a.ctor0 = Proc<decltype(a.ctor0)>(side, "??0ViewPort@Jet@@QAE@XZ");
    a.ctorCamGroup = Proc<decltype(a.ctorCamGroup)>(side, "??0ViewPort@Jet@@QAE@PAVCamera@1@PAVLayerGroup@1@@Z");
    a.ctorCamRectGroup =
        Proc<decltype(a.ctorCamRectGroup)>(side, "??0ViewPort@Jet@@QAE@PAVCamera@1@ABURect@1@PAVLayerGroup@1@@Z");
    a.ctorCopy = Proc<decltype(a.ctorCopy)>(side, "??0ViewPort@Jet@@QAE@ABV01@@Z");
    a.drop = Proc<decltype(a.drop)>(side, "??1ViewPort@Jet@@UAE@XZ");
    a.assign = Proc<decltype(a.assign)>(side, "??4ViewPort@Jet@@QAEAAV01@ABV01@@Z");
    a.setLocalPosition = Proc<decltype(a.setLocalPosition)>(side, "?SetLocalPosition@ViewPort@Jet@@QAAXHH@Z");
    a.getLocalPosition = Proc<decltype(a.getLocalPosition)>(side, "?GetLocalPosition@ViewPort@Jet@@QBAXAAH0@Z");
    a.getPosition = Proc<decltype(a.getPosition)>(side, "?GetPosition@ViewPort@Jet@@QBAXAAH0@Z");
    a.setSize = Proc<decltype(a.setSize)>(side, "?SetSize@ViewPort@Jet@@QAAXHH@Z");
    a.getSize = Proc<decltype(a.getSize)>(side, "?GetSize@ViewPort@Jet@@QBAXAAH0@Z");
    a.setLetterBox = Proc<decltype(a.setLetterBox)>(side, "?SetLetterBoxMode@ViewPort@Jet@@QAAX_N@Z");
    a.translate = Proc<decltype(a.translate)>(side, "?Translate@ViewPort@Jet@@QAAXHH@Z");
    a.scale1 = Proc<decltype(a.scale1)>(side, "?Scale@ViewPort@Jet@@QAAXM@Z");
    a.scale2 = Proc<decltype(a.scale2)>(side, "?Scale@ViewPort@Jet@@QAAXMM@Z");
    a.transform = Proc<decltype(a.transform)>(side, "?Transform@ViewPort@Jet@@QAAXAAVVector3@2@@Z");
    a.viewTransformPoint = Proc<decltype(a.viewTransformPoint)>(side, "?ViewTransformPoint@ViewPort@Jet@@QAAXAAVVector3@2@@Z");
    a.setClearColor = Proc<decltype(a.setClearColor)>(side, "?SetClearColor@ViewPort@Jet@@QAAXABVColor@2@@Z");
    a.setClearDepth = Proc<decltype(a.setClearDepth)>(side, "?SetClearDepth@ViewPort@Jet@@QAAXM@Z");
    a.setClearStencil = Proc<decltype(a.setClearStencil)>(side, "?SetClearStencil@ViewPort@Jet@@QAAXJ@Z");
    a.setClearMode = Proc<decltype(a.setClearMode)>(side, "?SetClearMode@ViewPort@Jet@@QAAXK@Z");
    a.setUserData = Proc<decltype(a.setUserData)>(side, "?SetUserData@ViewPort@Jet@@QAAXK@Z");
    a.setRenderHint = Proc<decltype(a.setRenderHint)>(side, "?SetRenderHint@ViewPort@Jet@@QAAXW4RenderHint@2@@Z");
    a.setRenderFlags = Proc<decltype(a.setRenderFlags)>(side, "?SetRenderFlags@ViewPort@Jet@@QAAXK@Z");
    a.setWindowViewport = Proc<decltype(a.setWindowViewport)>(side, "?SetWindowViewport@ViewPort@Jet@@QAAX_N@Z");
    a.setAlphaMaskReference =
        Proc<decltype(a.setAlphaMaskReference)>(side, "?SetAlphaMaskReference@ViewPort@Jet@@QAAXM@Z");
    a.setCamera = Proc<decltype(a.setCamera)>(side, "?SetCamera@ViewPort@Jet@@QAAXPAVCamera@2@@Z");
    a.getLocalExtents = Proc<decltype(a.getLocalExtents)>(side, "?GetLocalExtents@ViewPort@Jet@@QBAABURect@2@XZ");
    a.getExtents = Proc<decltype(a.getExtents)>(side, "?GetExtents@ViewPort@Jet@@QBAXAAURect@2@@Z");
    a.getLocalX = Proc<decltype(a.getLocalX)>(side, "?GetLocalX@ViewPort@Jet@@QBAHXZ");
    a.getX = Proc<decltype(a.getX)>(side, "?GetX@ViewPort@Jet@@QBAHXZ");
    a.getLocalY = Proc<decltype(a.getLocalY)>(side, "?GetLocalY@ViewPort@Jet@@QBAHXZ");
    a.getY = Proc<decltype(a.getY)>(side, "?GetY@ViewPort@Jet@@QBAHXZ");
    a.getWidth = Proc<decltype(a.getWidth)>(side, "?GetWidth@ViewPort@Jet@@QBAHXZ");
    a.getHeight = Proc<decltype(a.getHeight)>(side, "?GetHeight@ViewPort@Jet@@QBAHXZ");
    a.getCamera = Proc<decltype(a.getCamera)>(side, "?GetCamera@ViewPort@Jet@@QBAPAVCamera@2@XZ");
    a.getLetterBox = Proc<decltype(a.getLetterBox)>(side, "?GetLetterBoxMode@ViewPort@Jet@@QBA_NXZ");
    a.adjustAspectRatio = Proc<decltype(a.adjustAspectRatio)>(side, "?AdjustAspectRatio@ViewPort@Jet@@QBAXAAM0@Z");
    a.getUserData = Proc<decltype(a.getUserData)>(side, "?GetUserData@ViewPort@Jet@@QBAKXZ");
    a.getRenderHint = Proc<decltype(a.getRenderHint)>(side, "?GetRenderHint@ViewPort@Jet@@QBA?AW4RenderHint@2@XZ");
    a.getRenderFlags = Proc<decltype(a.getRenderFlags)>(side, "?GetRenderFlags@ViewPort@Jet@@QBAKXZ");
    a.getViewportNumber = Proc<decltype(a.getViewportNumber)>(side, "?GetViewportNumber@ViewPort@Jet@@QBAKXZ");
    a.getWindowViewport = Proc<decltype(a.getWindowViewport)>(side, "?GetWindowViewport@ViewPort@Jet@@QBA_NXZ");
    a.getAlphaMaskReference = Proc<decltype(a.getAlphaMaskReference)>(side, "?GetAlphaMaskReference@ViewPort@Jet@@QBAMXZ");
    a.getTraversalNumber = Proc<decltype(a.getTraversalNumber)>(side, "?GetTraversalNumber@ViewPort@Jet@@SAKXZ");
    a.getClearColor = Proc<decltype(a.getClearColor)>(side, "?GetClearColor@ViewPort@Jet@@QBAABVColor@2@XZ");
    a.getClearDepth = Proc<decltype(a.getClearDepth)>(side, "?GetClearDepth@ViewPort@Jet@@QBAMXZ");
    a.getClearStencil = Proc<decltype(a.getClearStencil)>(side, "?GetClearStencil@ViewPort@Jet@@QBAKXZ");
    a.getClearMode = Proc<decltype(a.getClearMode)>(side, "?GetClearMode@ViewPort@Jet@@QBAKXZ");
    a.collides1 = Proc<decltype(a.collides1)>(side, "?Collides@ViewPort@Jet@@UBA_NABVVector2@2@@Z");
    a.collides2 = Proc<decltype(a.collides2)>(side, "?Collides@ViewPort@Jet@@UBA_NABVVector2@2@0@Z");
    a.clear = Proc<decltype(a.clear)>(side, "?Clear@ViewPort@Jet@@QAAXXZ");
    a.getName = reinterpret_cast<decltype(a.getName)>(GetProcAddress(renderSystem, "?GetName@Layer@Jet@@QBAABVPString@2@XZ"));
    return a;
}

static DebugViewPortApi LoadDebugViewPort(int side)
{
    DebugViewPortApi a{};
    a.ctor = Proc<decltype(a.ctor)>(side, "??0DebugViewPort@Jet@@QAE@PAVViewPort@1@@Z");
    a.ctorCopy = Proc<decltype(a.ctorCopy)>(side, "??0DebugViewPort@Jet@@QAE@ABV01@@Z");
    a.drop = Proc<decltype(a.drop)>(side, "??1DebugViewPort@Jet@@UAE@XZ");
    a.assign = Proc<decltype(a.assign)>(side, "??4DebugViewPort@Jet@@QAEAAV01@ABV01@@Z");
    a.getViewPort = Proc<decltype(a.getViewPort)>(side, "?GetViewPort@DebugViewPort@Jet@@QBAPAVViewPort@2@XZ");
    return a;
}

static void CompareViewPort(const ViewPortObj* a, const ViewPortObj* b, const ViewPortApi* api, const char* what, int round)
{
    // Layer name, then the ViewPort fields; the camera pointer and the two self references inside the embedded
    // ViewPoint differ between the two objects.
    const void* na = api[0].getName(const_cast<ViewPortObj*>(a));
    const void* nb = api[1].getName(const_cast<ViewPortObj*>(b));
    Check(na && nb && std::strcmp(static_cast<const char*>(na) + 0x18, static_cast<const char*>(nb) + 0x18) == 0, what, round);
    CheckFloats(reinterpret_cast<const float*>(a->bytes + 0x24), reinterpret_cast<const float*>(b->bytes + 0x24),
                (0x78 - 0x24) / 4, what, round);
    // visible list header (empty list: count/capacity/data)
    Check(std::memcmp(a->bytes + 0x78, b->bytes + 0x78, 0xc) == 0, what, round);
    // embedded ViewPoint: +0, +4, +8 (list pointer), +0xc..+0x14 (three pointers), +0x18 (self), +0x5c..+0x6c
    CheckFloats(reinterpret_cast<const float*>(a->bytes + 0x84), reinterpret_cast<const float*>(b->bytes + 0x84), 2, what, round);
    CheckFloats(reinterpret_cast<const float*>(a->bytes + 0x90), reinterpret_cast<const float*>(b->bytes + 0x90), 3, what, round);
    CheckFloats(reinterpret_cast<const float*>(a->bytes + 0xe0), reinterpret_cast<const float*>(b->bytes + 0xe0), 5, what, round);
}

static void ViewPortRound(const CameraApi* capi, const ViewPortApi* api, const DebugViewPortApi* dapi, void* group,
                          bool awkward, int round)
{
    CameraObj cam[2];
    std::memset(cam, 0xcd, sizeof cam);
    for (int v = 0; v < 2; ++v) capi[v].ctor0(&cam[v].bytes);

    ViewPortObj vp[2][4];
    std::memset(vp, 0xcd, sizeof vp);
    for (int v = 0; v < 2; ++v) api[v].ctor0(&vp[v][0].bytes);
    CompareViewPort(&vp[0][0], &vp[1][0], api, "ViewPort()", round);

    for (int v = 0; v < 2; ++v) api[v].ctorCamGroup(&vp[v][1].bytes, &cam[v].bytes, group);
    CompareViewPort(&vp[0][1], &vp[1][1], api, "ViewPort(camera,group)", round);

    const Rect rect{awkward ? Pick(-50, 50) : Pick(-5, 20), Pick(-7, 30), Pick(0, 640), Pick(0, 480)};
    for (int v = 0; v < 2; ++v) api[v].ctorCamRectGroup(&vp[v][2].bytes, &cam[v].bytes, &rect, group);
    CompareViewPort(&vp[0][2], &vp[1][2], api, "ViewPort(camera,rect,group)", round);

    for (int v = 0; v < 2; ++v) api[v].ctorCopy(&vp[v][3].bytes, &vp[v][1].bytes);
    CompareViewPort(&vp[0][3], &vp[1][3], api, "ViewPort copy", round);
    // Assign between viewports of the same group: Layer::operator= copies the group links, so a cross-group
    // assignment corrupts the layer lists in both the original and this build.
    for (int v = 0; v < 2; ++v) api[v].assign(&vp[v][3].bytes, &vp[v][2].bytes);
    CompareViewPort(&vp[0][3], &vp[1][3], api, "ViewPort assign", round);

    const int x = Pick(-20, 20), y = Pick(-20, 20);
    for (int v = 0; v < 2; ++v) api[v].setLocalPosition(&vp[v][1].bytes, x, y);
    CompareViewPort(&vp[0][1], &vp[1][1], api, "SetLocalPosition", round);
    for (int v = 0; v < 2; ++v) api[v].translate(&vp[v][1].bytes, x, y);
    CompareViewPort(&vp[0][1], &vp[1][1], api, "Translate", round);
    for (int v = 0; v < 2; ++v) api[v].setSize(&vp[v][1].bytes, 640 + x, 480 + y);
    CompareViewPort(&vp[0][1], &vp[1][1], api, "SetSize", round);
    const float scaleValue = awkward ? Awkward() : FRange(0.25f, 2.0f);
    for (int v = 0; v < 2; ++v) api[v].scale1(&vp[v][1].bytes, scaleValue);
    CompareViewPort(&vp[0][1], &vp[1][1], api, "Scale(float)", round);
    const float scaleX = awkward ? Awkward() : FRange(0.5f, 2.0f);
    const float scaleY = awkward ? Awkward() : FRange(0.5f, 2.0f);
    for (int v = 0; v < 2; ++v) api[v].scale2(&vp[v][1].bytes, scaleX, scaleY);
    CompareViewPort(&vp[0][1], &vp[1][1], api, "Scale(float,float)", round);

    for (int v = 0; v < 2; ++v) api[v].setLetterBox(&vp[v][1].bytes, true);
    CompareViewPort(&vp[0][1], &vp[1][1], api, "SetLetterBoxMode(true)", round);
    CompareCamera(&cam[0], &cam[1], "SetLetterBoxMode camera", round);
    for (int v = 0; v < 2; ++v) api[v].setLetterBox(&vp[v][1].bytes, false);
    CompareViewPort(&vp[0][1], &vp[1][1], api, "SetLetterBoxMode(false)", round);
    CompareCamera(&cam[0], &cam[1], "SetLetterBoxMode camera off", round);

    Color color{awkward ? Awkward() : FRange(0, 1), awkward ? Awkward() : FRange(0, 1),
                awkward ? Awkward() : FRange(0, 1), awkward ? Awkward() : FRange(0, 1)};
    for (int v = 0; v < 2; ++v) api[v].setClearColor(&vp[v][1].bytes, &color);
    const float clearDepthValue = awkward ? Awkward() : FRange(0, 1);
    for (int v = 0; v < 2; ++v) api[v].setClearDepth(&vp[v][1].bytes, clearDepthValue);
    const long stencilValue = awkward ? Pick(-100, 100) : x;
    const unsigned long clearModeValue = static_cast<unsigned long>(Pick(0, 15));
    const unsigned long userDataValue = static_cast<unsigned long>(Pick(0, 1000));
    const int renderHintValue = Pick(0, 3);
    const unsigned long renderFlagsValue = static_cast<unsigned long>(Pick(0, 255));
    for (int v = 0; v < 2; ++v) api[v].setClearStencil(&vp[v][1].bytes, stencilValue);
    for (int v = 0; v < 2; ++v) api[v].setClearMode(&vp[v][1].bytes, clearModeValue);
    for (int v = 0; v < 2; ++v) api[v].setUserData(&vp[v][1].bytes, userDataValue);
    for (int v = 0; v < 2; ++v) api[v].setRenderHint(&vp[v][1].bytes, renderHintValue);
    for (int v = 0; v < 2; ++v) api[v].setRenderFlags(&vp[v][1].bytes, renderFlagsValue);
    for (int v = 0; v < 2; ++v) api[v].setWindowViewport(&vp[v][1].bytes, (round & 1) != 0);
    const float alphaValue = awkward ? Awkward() : FRange(0, 2);
    for (int v = 0; v < 2; ++v) api[v].setAlphaMaskReference(&vp[v][1].bytes, alphaValue);
    CompareViewPort(&vp[0][1], &vp[1][1], api, "viewport state setters", round);

    unsigned long values[2][10];
    for (int v = 0; v < 2; ++v) {
        unsigned long* o = values[v];
        int a = 0, b = 0, c = 0, d = 0;
        api[v].getLocalPosition(&vp[v][1].bytes, &a, &b);
        api[v].getSize(&vp[v][1].bytes, &c, &d);
        o[0] = static_cast<unsigned long>(a);
        o[1] = static_cast<unsigned long>(b);
        o[2] = static_cast<unsigned long>(c);
        o[3] = static_cast<unsigned long>(d);
        o[4] = api[v].getUserData(&vp[v][1].bytes);
        o[5] = static_cast<unsigned long>(api[v].getRenderHint(&vp[v][1].bytes));
        o[6] = api[v].getRenderFlags(&vp[v][1].bytes);
        o[7] = api[v].getViewportNumber(&vp[v][1].bytes);
        o[8] = api[v].getClearStencil(&vp[v][1].bytes);
        o[9] = api[v].getClearMode(&vp[v][1].bytes);
    }
    Check(std::memcmp(values[0], values[1], sizeof values[0]) == 0, "viewport integer getters", round);
    float floats[2][8];
    for (int v = 0; v < 2; ++v) {
        float* o = floats[v];
        o[0] = api[v].getAlphaMaskReference(&vp[v][1].bytes);
        o[1] = api[v].getClearDepth(&vp[v][1].bytes);
        o[2] = api[v].getTraversalNumber() * 1.0f;
        o[3] = api[v].getLetterBox(&vp[v][1].bytes) ? 1.0f : 0.0f;
        o[4] = api[v].getWindowViewport(&vp[v][1].bytes) ? 1.0f : 0.0f;
        o[5] = api[v].collides1(&vp[v][1].bytes, reinterpret_cast<const Vec2*>(&x)) ? 1.0f : 0.0f;
        o[6] = api[v].collides2(&vp[v][1].bytes, reinterpret_cast<const Vec2*>(&x), reinterpret_cast<const Vec2*>(&y)) ? 1.0f : 0.0f;
        o[7] = 0.0f;
    }
    CheckFloats(floats[0], floats[1], 8, "viewport float getters", round);
    CheckFloats(reinterpret_cast<const float*>(api[0].getClearColor(&vp[0][1].bytes)),
                reinterpret_cast<const float*>(api[1].getClearColor(&vp[1][1].bytes)), 4, "GetClearColor", round);

    {
        Rect ea{}, eb{};
        api[0].getExtents(&vp[0][1].bytes, &ea);
        api[1].getExtents(&vp[1][1].bytes, &eb);
        Check(std::memcmp(&ea, &eb, sizeof ea) == 0, "GetExtents", round);
        int ax = api[0].getX(&vp[0][1].bytes), ay = api[0].getY(&vp[0][1].bytes);
        int bx = api[1].getX(&vp[1][1].bytes), by = api[1].getY(&vp[1][1].bytes);
        Check(ax == bx && ay == by, "GetX/GetY", round);
        float rx = 0.5f, ry = 0.25f, sx = 0.5f, sy = 0.25f;
        api[0].adjustAspectRatio(&vp[0][1].bytes, &rx, &ry);
        api[1].adjustAspectRatio(&vp[1][1].bytes, &sx, &sy);
        Check(FBits(rx) == FBits(sx) && FBits(ry) == FBits(sy), "AdjustAspectRatio", round);
    }

    Vec3 ta = RandomVec(awkward), tb = ta;
    api[0].transform(&vp[0][1].bytes, &ta);
    api[1].transform(&vp[1][1].bytes, &tb);
    CheckFloats(reinterpret_cast<const float*>(&ta), reinterpret_cast<const float*>(&tb), 3, "Transform", round);

    Vec3 va = RandomVec(awkward), vb = va;
    api[0].viewTransformPoint(&vp[0][1].bytes, &va);
    api[1].viewTransformPoint(&vp[1][1].bytes, &vb);
    CheckFloats(reinterpret_cast<const float*>(&va), reinterpret_cast<const float*>(&vb), 3, "ViewTransformPoint", round);

    DebugViewPortObj dvp[2][2];
    std::memset(dvp, 0xcd, sizeof dvp);
    for (int v = 0; v < 2; ++v) {
        dapi[v].ctor(&dvp[v][0].bytes, &vp[v][1].bytes);
    }
    CheckFloats(reinterpret_cast<const float*>(dvp[0][0].bytes + 0x24), reinterpret_cast<const float*>(dvp[1][0].bytes + 0x24),
                0x34 / 4, "DebugViewPort fields", round);
    Check(*(void**)(dvp[0][0].bytes + 0x20) != nullptr && *(void**)(dvp[1][0].bytes + 0x20) != nullptr,
          "DebugViewPort camera", round);
    Check(*(void**)(dvp[0][0].bytes + 0xf4) != nullptr && *(void**)(dvp[1][0].bytes + 0xf4) != nullptr &&
              *(unsigned*)(dvp[0][0].bytes + 0xf8) == *(unsigned*)(dvp[1][0].bytes + 0xf8),
          "DebugViewPort source", round);
    Check(dapi[0].getViewPort(&dvp[0][0].bytes) == &vp[0][1].bytes && dapi[1].getViewPort(&dvp[1][0].bytes) == &vp[1][1].bytes,
          "DebugViewPort::GetViewPort", round);
    for (int v = 0; v < 2; ++v) dapi[v].ctorCopy(&dvp[v][1].bytes, &dvp[v][0].bytes);
    Check(*(void**)(dvp[0][1].bytes + 0xf4) != nullptr && *(void**)(dvp[1][1].bytes + 0xf4) != nullptr &&
              *(unsigned*)(dvp[0][1].bytes + 0xf8) == *(unsigned*)(dvp[1][1].bytes + 0xf8),
          "DebugViewPort copy", round);
    for (int v = 0; v < 2; ++v) dapi[v].assign(&dvp[v][0].bytes, &dvp[v][1].bytes);
    Check(*(void**)(dvp[0][0].bytes + 0xf4) != nullptr && *(void**)(dvp[1][0].bytes + 0xf4) != nullptr &&
              *(unsigned*)(dvp[0][0].bytes + 0xf8) == *(unsigned*)(dvp[1][0].bytes + 0xf8),
          "DebugViewPort assign", round);
    // DebugViewPort's copy shares the camera pointer with its source; dropping both would delete it twice, as it
    // would in the original.
    for (int v = 0; v < 2; ++v) dapi[v].drop(&dvp[v][0].bytes);

    for (int v = 0; v < 2; ++v) {
        api[v].clear(&vp[v][0].bytes);
        api[v].drop(&vp[v][3].bytes);
        api[v].drop(&vp[v][2].bytes);
        api[v].drop(&vp[v][1].bytes);
        api[v].drop(&vp[v][0].bytes);
        capi[v].drop(&cam[v].bytes);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u8 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 500;
    SetDllDirectoryA(argv[1]);
    const std::string dir = argv[1];
    LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str());
    mod[0] = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    mod[1] = GetModuleHandleA("Render3DCore_7c_orig.dll");
    renderSystem = GetModuleHandleA("RenderSystem_7c.dll");
    if (!renderSystem) renderSystem = LoadLibraryA((dir + "\\RenderSystem_7c.dll").c_str());
    if (!mod[0] || !mod[1] || !renderSystem) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    // A live LayerManager, a fake RenderKernel with an OS at +0x8c and an empty Options object are enough for the
    // ViewPort constructors: Layer registers the layer and Initialize reads OS::GetOptions().
    HMODULE kernelStandard = GetModuleHandleA("KernelStandard_7c.dll");
    if (!kernelStandard) kernelStandard = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!kernelStandard) {
        std::printf("cannot load KernelStandard_7c.dll\n");
        return 2;
    }
    using ManagerCtor = void(__thiscall*)(void*);
    using GroupCtor = void(__thiscall*)(void*);
    using OptionsCtor = void(__thiscall*)(void*);
    auto managerCtor = reinterpret_cast<ManagerCtor>(GetProcAddress(renderSystem, "??0LayerManager@Jet@@QAE@XZ"));
    auto groupCtor = reinterpret_cast<GroupCtor>(GetProcAddress(renderSystem, "??0LayerGroup@Jet@@QAE@XZ"));
    auto optionsCtor = reinterpret_cast<OptionsCtor>(GetProcAddress(kernelStandard, "??0Options@Jet@@QAE@XZ"));
    sharedLayerManagerSlot = GetProcAddress(renderSystem, "?layerManager@Layer@Jet@@1PAVLayerManager@2@A");
    if (!managerCtor || !groupCtor || !optionsCtor || !sharedLayerManagerSlot) {
        std::printf("missing RenderSystem/KernelStandard construction exports\n");
        return 2;
    }

    alignas(16) unsigned char managerMemory[0x100] = {};
    alignas(16) unsigned char groupMemory[0x40] = {};
    alignas(16) unsigned char kernelMemory[0x90] = {};
    alignas(16) unsigned char osMemory[0x18] = {};
    alignas(16) unsigned char optionsMemory[0x0c] = {};
    alignas(16) unsigned char displayMemory[0x100] = {};
    alignas(16) unsigned char targetMemory[0x20] = {};
    void* oldManager = *static_cast<void**>(sharedLayerManagerSlot);
    *static_cast<void**>(sharedLayerManagerSlot) = nullptr;
    managerCtor(managerMemory);
    static FARPROC managerTable[8];
    FARPROC* originalTable = *reinterpret_cast<FARPROC**>(managerMemory);
    for (int i = 0; i < 8; ++i) managerTable[i] = originalTable[i];
    g_fakeKernel = kernelMemory;
    managerTable[1] = reinterpret_cast<FARPROC>(GetManagerKernelStub);
    *reinterpret_cast<FARPROC**>(managerMemory) = managerTable;
    optionsCtor(optionsMemory);
    *reinterpret_cast<void**>(osMemory + 0x0c) = optionsMemory;
    *reinterpret_cast<void**>(kernelMemory + 0x8c) = osMemory;
    *reinterpret_cast<int*>(targetMemory + 0x10) = 640;
    *reinterpret_cast<int*>(targetMemory + 0x14) = 480;
    *reinterpret_cast<void**>(displayMemory + 0x5c) = targetMemory;
    *reinterpret_cast<void**>(kernelMemory + 0x7c) = displayMemory;
    *static_cast<void**>(sharedLayerManagerSlot) = managerMemory;
    groupCtor(groupMemory);

    CameraApi api[2] = {LoadCamera(0), LoadCamera(1)};
    ViewPortApi vpApi[2] = {LoadViewPort(0), LoadViewPort(1)};
    DebugViewPortApi dvpApi[2] = {LoadDebugViewPort(0), LoadDebugViewPort(1)};

    for (int round = 0; round < rounds; ++round) {
        const bool awkward = (round % 7) == 0;
        const Vec3 pos = RandomVec(awkward);
        const Quat quat = RandomQuat(awkward);
        const Mat3 mat = RandomMat3(awkward);
        const float f1 = awkward ? Awkward() : FRange(-3.0f, 3.0f);
        const float f2 = awkward ? Awkward() : FRange(-3.0f, 3.0f);
        const float f3 = awkward ? Awkward() : FRange(-3.0f, 3.0f);

        CameraObj cams[2][3];
        std::memset(cams, 0xcd, sizeof cams);
        const Vec3* positions[2] = {&pos, &pos};

        for (int v = 0; v < 2; ++v) {
            api[v].ctor0(&cams[v][0].bytes);
            api[v].ctor3(&cams[v][1].bytes, f1, f2, f3);
            api[v].ctorPosQuat(&cams[v][2].bytes, &pos, &quat);
        }
        CompareCamera(&cams[0][0], &cams[1][0], "Camera()", round);
        CompareCamera(&cams[0][1], &cams[1][1], "Camera(float,float,float)", round);
        CompareCamera(&cams[0][2], &cams[1][2], "Camera(pos,quat)", round);

        for (int v = 0; v < 2; ++v) {
            api[v].ctorPosMat3(&cams[v][0].bytes, &pos, &mat);
            api[v].ctorPosEuler(&cams[v][0].bytes, &pos, f1, f2, f3);
            api[v].ctorPosQuat3(&cams[v][0].bytes, &pos, &quat, f1, f2, f3);
            api[v].ctorPosMat33(&cams[v][0].bytes, &pos, &mat, f1, f2, f3);
            api[v].ctorPosEuler3(&cams[v][0].bytes, &pos, f1, f2, f3, f1, f2, f3);
        }
        CompareCamera(&cams[0][0], &cams[1][0], "Camera(pos,mat3,...)", round);

        // copy, assignment, copy constructor
        for (int v = 0; v < 2; ++v) {
            api[v].ctorPosQuat(&cams[v][1].bytes, &pos, &quat);
            // a clean volume lets SetReferenceFrame run its centre delta
            ClearVolumeFlags(&cams[v][1].bytes);
            api[v].setReferenceFrame(&cams[v][1].bytes, &pos, &quat);
            api[v].ctorCopy(&cams[v][2].bytes, &cams[v][1].bytes);
            api[v].assign(&cams[v][0].bytes, &cams[v][1].bytes);
        }
        CompareCamera(&cams[0][2], &cams[1][2], "Camera copy", round);
        CompareCamera(&cams[0][0], &cams[1][0], "Camera assign", round);

        // transforms through the vtable subset
        for (int v = 0; v < 2; ++v) {
            ClearVolumeFlags(&cams[v][0].bytes);
            api[v].setPosition(&cams[v][0].bytes, &pos);
            api[v].setOrientation(&cams[v][0].bytes, &quat);
            if (api[v].updateCached) api[v].updateCached(&cams[v][0].bytes);
        }
        CompareCamera(&cams[0][0], &cams[1][0], "SetPosition/SetOrientation", round);

        // projection setters
        for (int v = 0; v < 2; ++v) {
            api[v].setNear(&cams[v][0].bytes, f1);
            api[v].setFar(&cams[v][0].bytes, f2);
        }
        CompareCamera(&cams[0][0], &cams[1][0], "SetNear/SetFar", round);
        for (int v = 0; v < 2; ++v) api[v].setPerspective(&cams[v][0].bytes, f1, f2);
        CompareCamera(&cams[0][0], &cams[1][0], "SetPerspective", round);
        for (int v = 0; v < 2; ++v) api[v].setOrthographic(&cams[v][0].bytes, f1, f2);
        CompareCamera(&cams[0][0], &cams[1][0], "SetOrthographic", round);
                const float blend = awkward ? Awkward() : FRange(-0.5f, 1.5f);
        for (int v = 0; v < 2; ++v) api[v].setTransition(&cams[v][0].bytes, blend, f1, f2, f3);
        CompareCamera(&cams[0][0], &cams[1][0], "SetTransition", round);

        // frustum and every scalar/vector/matrix getter
        const Plane* fa = api[0].getFrustum(&cams[0][0].bytes);
        const Plane* fb = api[1].getFrustum(&cams[1][0].bytes);
        CheckFloats(reinterpret_cast<const float*>(fa), reinterpret_cast<const float*>(fb), 24, "GetFrustum", round);
        CompareCamera(&cams[0][0], &cams[1][0], "UpdateViewCone", round);

        for (int v = 0; v < 2; ++v) {
            api[v].setProjectionData(&cams[v][0].bytes, &cams[v][1].bytes);
        }
        CompareCamera(&cams[0][0], &cams[1][0], "SetProjectionData", round);

        float scalars[2][12];
        for (int v = 0; v < 2; ++v) {
            float* s = scalars[v];
            s[0] = api[v].getNear(&cams[v][0].bytes);
            s[1] = api[v].getFar(&cams[v][0].bytes);
            s[2] = api[v].getNearWidth(&cams[v][0].bytes);
            s[3] = api[v].getNearHeight(&cams[v][0].bytes);
            s[4] = api[v].getHorizontalFov(&cams[v][0].bytes);
            s[5] = api[v].getVerticalFov(&cams[v][0].bytes);
            s[6] = api[v].getPitch(&cams[v][0].bytes);
            s[7] = api[v].getYaw(&cams[v][0].bytes);
            s[8] = api[v].getRoll(&cams[v][0].bytes);
            s[9] = api[v].getRelevantWidth(&cams[v][0].bytes);
            s[10] = api[v].getRelevantHeight(&cams[v][0].bytes);
            s[11] = api[v].getProjectionMode(&cams[v][0].bytes) ? 1.0f : 0.0f;
        }
        CheckFloats(scalars[0], scalars[1], 12, "getters", round);

        Vec3 vecs[2][3];
        for (int v = 0; v < 2; ++v) {
            CameraObjMem* o = reinterpret_cast<CameraObjMem*>(&cams[v][0].bytes);
            vecs[v][0] = (o->*api[v].getRight)();
            vecs[v][1] = (o->*api[v].getUp)();
            vecs[v][2] = (o->*api[v].getForward)();
        }
        CheckFloats(reinterpret_cast<const float*>(vecs[0]), reinterpret_cast<const float*>(vecs[1]), 9, "Right/Up/Forward", round);

        const Mat3* oa = api[0].getCachedOrientation(&cams[0][0].bytes);
        const Mat3* ob = api[1].getCachedOrientation(&cams[1][0].bytes);
        CheckFloats(oa->m, ob->m, 9, "GetCachedOrientation", round);
        const Mat4* va = api[0].getViewTransform(&cams[0][0].bytes);
        const Mat4* vb = api[1].getViewTransform(&cams[1][0].bytes);
        CheckFloats(va->m, vb->m, 16, "GetViewTransform", round);
        const Mat4* ga = api[0].getGlobalViewTransform(&cams[0][0].bytes);
        const Mat4* gb = api[1].getGlobalViewTransform(&cams[1][0].bytes);
        CheckFloats(ga->m, gb->m, 16, "GetGlobalViewTransform", round);
        Mat4 pa{}, pb{};
        for (int v = 0; v < 2; ++v) {
            CameraObjMem* o = reinterpret_cast<CameraObjMem*>(&cams[v][0].bytes);
            Mat4 m = (o->*api[v].getPerspectiveTransform)();
            (v ? pb : pa) = m;
        }
        CheckFloats(pa.m, pb.m, 16, "GetPerspectiveTransform", round);

        // clipping and enough test
        const Vec3 center = RandomVec(awkward, -3.0f, 3.0f);
        const float radius = awkward ? Awkward() : FRange(-5.0f, 50.0f);
        Check(api[0].clip3DSphere(&cams[0][0].bytes, &center, radius) == api[1].clip3DSphere(&cams[1][0].bytes, &center, radius),
              "Clip3D(sphere)", round);
        Check(api[0].bigEnough(&cams[0][0].bytes, &center, radius) == api[1].bigEnough(&cams[1][0].bytes, &center, radius),
              "BigEnough", round);
        Vec3 pts[4] = {center, RandomVec(awkward), RandomVec(awkward), RandomVec(awkward)};
        Check(api[0].clip3DPoints(&cams[0][0].bytes, pts, 4) == api[1].clip3DPoints(&cams[1][0].bytes, pts, 4),
              "Clip3D(points)", round);
        Vec3 nearA = RandomVec(awkward);
        Vec3 nearB = nearA;
        const int ca = api[0].clipNear(&cams[0][0].bytes, &nearA);
        const int cb = api[1].clipNear(&cams[1][0].bytes, &nearB);
        Check(ca == cb && SameFloats(reinterpret_cast<const float*>(&nearA), reinterpret_cast<const float*>(&nearB), 3),
              "ClipNear", round);

        // projection of a point
        Vec3 pv = RandomVec(awkward);
        Vec3 pw = pv;
        api[0].perspective(&cams[0][0].bytes, &pv);
        api[1].perspective(&cams[1][0].bytes, &pw);
        CheckFloats(reinterpret_cast<const float*>(&pv), reinterpret_cast<const float*>(&pw), 3, "Perspective", round);

        Vec3 tv = RandomVec(awkward);
        Vec3 tw = tv;
        const int ta = api[0].transform2D(&cams[0][0].bytes, &tv, f1) ? 1 : 0;
        const int tb = api[1].transform2D(&cams[1][0].bytes, &tw, f1) ? 1 : 0;
        Check(ta == tb, "Transform2D result", round);
        CheckFloats(reinterpret_cast<const float*>(&tv), reinterpret_cast<const float*>(&tw), 3, "Transform2D vector", round);

        tv = RandomVec(awkward);
        tw = tv;
        const int tta = api[0].trainzTransform2D(&cams[0][0].bytes, &tv, f1) ? 1 : 0;
        const int ttb = api[1].trainzTransform2D(&cams[1][0].bytes, &tw, f1) ? 1 : 0;
        Check(tta == ttb, "TrainzTransform2D result", round);
        CheckFloats(reinterpret_cast<const float*>(&tv), reinterpret_cast<const float*>(&tw), 3, "TrainzTransform2D vector", round);

        Vec3 uv = RandomVec(awkward);
        Vec3 uw = uv;
        api[0].trainzUnPerspective(&cams[0][0].bytes, f1, f2, f3, &uv);
        api[1].trainzUnPerspective(&cams[1][0].bytes, f1, f2, f3, &uw);
        CheckFloats(reinterpret_cast<const float*>(&uv), reinterpret_cast<const float*>(&uw), 3, "TrainzUnPerspective", round);

        // clipping planes list
        PlaneList la{}, lb{};
        api[0].getClippingPlanes(&cams[0][0].bytes, f1, f2, f3, f1, &la);
        api[1].getClippingPlanes(&cams[1][0].bytes, f1, f2, f3, f1, &lb);
        if (la.head && lb.head) {
            struct Chunk {
                Chunk* next;
                Chunk* prev;
                unsigned int count;
                Plane data[16];
            };
            const Chunk* xa = static_cast<const Chunk*>(la.head);
            const Chunk* xb = static_cast<const Chunk*>(lb.head);
            Check(xa->count == xb->count, "GetClippingPlanes count", round);
            CheckFloats(reinterpret_cast<const float*>(xa->data), reinterpret_cast<const float*>(xb->data),
                        static_cast<int>(4 * sizeof(Plane) / sizeof(float)), "GetClippingPlanes", round);
        } else {
            Check(la.head != nullptr && lb.head != nullptr, "GetClippingPlanes allocated", round);
        }

        // target alignment, orbit, rotation
        const Vec3 target = RandomVec(awkward);
        const Vec3 up = RandomVec(awkward);
        for (int v = 0; v < 2; ++v) api[v].alignToTarget(&cams[v][0].bytes, &target, &up);
        CompareCamera(&cams[0][0], &cams[1][0], "AlignToTarget", round);
        for (int v = 0; v < 2; ++v) api[v].orbit(&cams[v][0].bytes, &target, f1, f2, &up);
        CompareCamera(&cams[0][0], &cams[1][0], "Orbit", round);
        for (int v = 0; v < 2; ++v) api[v].translate(&cams[v][0].bytes, &target);
        CompareCamera(&cams[0][0], &cams[1][0], "Translate", round);
        for (int v = 0; v < 2; ++v) api[v].rotateM(&cams[v][0].bytes, &mat);
        CompareCamera(&cams[0][0], &cams[1][0], "Rotate(Matrix3)", round);
        for (int v = 0; v < 2; ++v) api[v].rotateQ(&cams[v][0].bytes, &quat);
        CompareCamera(&cams[0][0], &cams[1][0], "Rotate(Quaternion)", round);
        for (int v = 0; v < 2; ++v) api[v].selfRotateM(&cams[v][0].bytes, &mat);
        CompareCamera(&cams[0][0], &cams[1][0], "SelfRotate(Matrix3)", round);
        for (int v = 0; v < 2; ++v) api[v].selfRotateQ(&cams[v][0].bytes, &quat);
        CompareCamera(&cams[0][0], &cams[1][0], "SelfRotate(Quaternion)", round);

        for (int v = 0; v < 2; ++v) api[v].drop(&cams[v][1].bytes);

        ViewPortRound(api, vpApi, dvpApi, groupMemory, awkward, round);
    }

    *static_cast<void**>(sharedLayerManagerSlot) = oldManager;

    std::printf("%llu checks, %llu mismatches, %llu NaN-payload-only differences\n", checks, bad, nanPayload);
    return bad ? 1 : 0;
}
