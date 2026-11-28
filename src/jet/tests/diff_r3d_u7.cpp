// Differential test of Render3DCore_7c unit U7 (the lights): our Light/AmbientLight/OmniLight/ParallelLight/
// SpotLight/CubeMapLight against Render3DCore_7c_orig.dll in one process.
//
// Constructors and assignments are compared as raw bytes: each side builds the object at the *same* buffer
// address, in turn, so the embedded bounding volume's self pointers are identical. The two vtable pointers
// (+0 and +0x50) are the only bytes skipped. Methods are compared through GetProcAddress on identical inputs,
// including signed zero, infinities and quiet/signalling NaNs. Virtual slots are exercised through the object's
// own vtable with the slot signatures the assembly shows (position/direction slots are cdecl, this on the stack).
//
// The BatchCalculateDiffuse methods are forwarded and not exercised (they need Display, Material and vertex
// items); CubeMapLight's is empty and is checked through the vtable only for the slot mapping.
//
// usage: diff_r3d_u7.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

struct Vec3 {
    float x, y, z;
};
struct Quat {
    float x, y, z, w;
};
struct Color4 {
    float r, g, b, a;
};
struct Mat3 {
    float m[9];
};
static_assert(sizeof(Vec3) == 0xc, "Vec3");
static_assert(sizeof(Quat) == 0x10, "Quat");
static_assert(sizeof(Color4) == 0x10, "Color4");
static_assert(sizeof(Mat3) == 0x24, "Mat3");

// object sizes (Render3DCoreLights.h)
enum { kLight = 0x180, kAmbient = 0x180, kOmni = 0x18c, kParallel = 0x1a8, kSpot = 0x1b4, kCubeMap = 0x1ac };

static HMODULE mod[2];
static HMODULE rsMod[2];
static unsigned long long checks, bad;
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
    switch (Pick(0, 12)) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: return Bits(0x7f800000u);   // +inf
    case 3: return Bits(0xff800000u);   // -inf
    case 4: return Bits(0x7fc00000u);   // qNaN
    case 5: return Bits(0xffc00001u);   // qNaN, payload
    case 6: return Bits(0x7fa00000u);   // sNaN
    case 7: return Bits(0xffa00000u);   // negative sNaN
    default: return FRange(-1000.0f, 1000.0f);
    }
}

static Vec3 RandomVec(bool awkward)
{
    return Vec3{awkward ? Awkward() : FRange(-100.0f, 100.0f), awkward ? Awkward() : FRange(-100.0f, 100.0f),
                awkward ? Awkward() : FRange(-100.0f, 100.0f)};
}

static Quat RandomQuat(bool awkward)
{
    return Quat{awkward ? Awkward() : FRange(-2.0f, 2.0f), awkward ? Awkward() : FRange(-2.0f, 2.0f),
                awkward ? Awkward() : FRange(-2.0f, 2.0f), awkward ? Awkward() : FRange(-2.0f, 2.0f)};
}

static Color4 RandomColor(bool awkward)
{
    return Color4{awkward ? Awkward() : FRange(0.0f, 1.0f), awkward ? Awkward() : FRange(0.0f, 1.0f),
                  awkward ? Awkward() : FRange(0.0f, 1.0f), awkward ? Awkward() : FRange(0.0f, 1.0f)};
}

static Mat3 RandomMat3(bool awkward)
{
    Mat3 m;
    for (float& v : m.m) v = awkward ? Awkward() : FRange(-2.0f, 2.0f);
    return m;
}

static uint32_t FBits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

template <class F>
static F RsProc(int side, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(rsMod[side], name));
    if (!p) {
        std::printf("missing RenderSystem export %s in %s\n", name, side ? "orig" : "ours");
        std::exit(2);
    }
    return p;
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

static bool SameBytes(const void* a, const void* b, size_t n)
{
    return std::memcmp(a, b, n) == 0;
}

static void CheckVec(const Vec3& a, const Vec3& b, const char* what)
{
    const bool ok = SameBytes(&a, &b, sizeof a);
    if (!ok)
        std::printf("%s: ours(%08x %08x %08x) orig(%08x %08x %08x)\n", what, FBits(a.x), FBits(a.y), FBits(a.z),
                    FBits(b.x), FBits(b.y), FBits(b.z));
    Check(ok, what);
}

static void CheckQuat(const Quat& a, const Quat& b, const char* what)
{
    const bool ok = SameBytes(&a, &b, sizeof a);
    if (!ok)
        std::printf("%s: ours(%08x %08x %08x %08x) orig(%08x %08x %08x %08x)\n", what, FBits(a.x), FBits(a.y),
                    FBits(a.z), FBits(a.w), FBits(b.x), FBits(b.y), FBits(b.z), FBits(b.w));
    Check(ok, what);
}

static void CheckColor(const Color4& a, const Color4& b, const char* what)
{
    Check(SameBytes(&a, &b, sizeof a), what);
}

static void CheckMat3(const Mat3& a, const Mat3& b, const char* what)
{
    Check(SameBytes(&a, &b, sizeof a), what);
}

static bool IsImagePointer(int side, uint32_t v)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(mod[side]);
    const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return v >= base && v < base + nt->OptionalHeader.SizeOfImage;
}

// a constructor/assignment snapshot: dwords that point into either module (vftables, the embedded volume's
// sphere vtable) are module specific and compared structurally, everything else byte for byte
static void CheckSnapshot(const char* what, const unsigned char* a, const unsigned char* b, int size)
{
    for (int i = 0; i + 4 <= size; i += 4) {
        uint32_t wa, wb;
        std::memcpy(&wa, a + i, 4);
        std::memcpy(&wb, b + i, 4);
        if (wa == wb) continue;
        if (IsImagePointer(0, wa) && IsImagePointer(1, wb)) continue;
        std::printf("%s: dword %#x ours %08x orig %08x\n", what, i, wa, wb);
        Check(false, what);
        return;
    }
    Check(true, what);
}

static void CheckRange(const char* what, const unsigned char* a, const unsigned char* b, int off, int size)
{
    Check(SameBytes(a + off, b + off, size), what);
}

// ---------------------------------------------------------------------------------------------------------------
// exported entry points

using Ctor0 = void(__thiscall*)(void*);
using CtorType = void(__thiscall*)(void*, int);
using CtorVecFloat = void(__thiscall*)(void*, const Vec3*, float);
using CtorTypeVecFloat = void(__thiscall*)(void*, int, const Vec3*, float);
using CtorRGB = void(__thiscall*)(void*, const Vec3*, float, const Color4*);
using CtorCopy = void(__thiscall*)(void*, const void*);
using CtorParallel = void(__thiscall*)(void*, const Vec3*, const Quat*, float, const Color4*, int);
using CtorSpot = void(__thiscall*)(void*, const Vec3*, const Quat*, float, float, float, const Color4*);
using CtorCube = void(__thiscall*)(void*, const Vec3*, const Quat*, const void*, float, const Color4*);
using Dtor = void(__thiscall*)(void*);
using AssignThunk = void*(__thiscall*)(void*, const void*);
using AssignCdecl = void*(__cdecl*)(void*, const void*);

using GetIDFunc = int(__cdecl*)(const void*);
using IsOfTypeFunc = bool(__cdecl*)(const void*, int);
using EqFunc = bool(__cdecl*)(const void*, const void*);
using GetVecFunc = const Vec3* (__cdecl*)(const void*);
using SetVecFunc = void(__cdecl*)(void*, const Vec3*);
using GetQuatFunc = const Quat* (__cdecl*)(const void*);
using SetQuatFunc = void(__cdecl*)(void*, const Quat*);
using GetMat3Func = const Mat3* (__cdecl*)(const void*);
using SetMat3Func = void(__cdecl*)(void*, const Mat3*);
using InitFunc = void(__cdecl*)(const void*, void*);
using CreateFunc = void* (__cdecl*)(const void*);
using VoidFunc = void(__cdecl*)(void*);
using GetBoolFunc = bool(__cdecl*)(void*);
using SetBoolFunc = void(__cdecl*)(void*, bool);
using GetTextureFunc = const void* (__cdecl*)(const void*);
using SetTextureFunc = void(__cdecl*)(void*, const void*);
using GetColorFunc = const Color4* (__cdecl*)(const void*);
using GetFloatFunc = float(__cdecl*)(const void*);
using SetReferenceFrameFunc = void(__cdecl*)(void*, const Vec3*, const Quat*);
using RotateMatFunc = void(__cdecl*)(void*, const Mat3*);
using RotateQuatFunc = void(__cdecl*)(void*, const Quat*);

struct Api {
    Ctor0 ambient0, omni0, spot0, parallelClosure;
    CtorType lightType0, lightType, parallelType;
    CtorTypeVecFloat lightPos;
    CtorRGB ambientRGB, omniRGB;
    CtorCopy lightCopy, ambientCopy, omniCopy, parallelCopy, spotCopy, cubeCopy;
    CtorParallel parallelPos;
    CtorSpot spotPos;
    CtorCube cubePos;
    Dtor lightDtor, ambientDtor, omniDtor, parallelDtor, spotDtor, cubeDtor;

    AssignCdecl lightAssign;
    AssignThunk ambientAssign, omniAssign, parallelAssign, spotAssign, cubeAssign;

    GetIDFunc lightGetID, ambientGetID, omniGetID, parallelGetID, spotGetID, cubeGetID;
    IsOfTypeFunc lightIsType, ambientIsType, omniIsType, parallelIsType, spotIsType, cubeIsType;
    EqFunc lightEq, omniEq;
    GetVecFunc lightGetPos, ambientGetDir, omniGetDir, parallelGetDir, cubeGetDir, particleGetPos;
    SetVecFunc lightSetPos, ambientSetDir, omniSetDir, parallelSetDir, cubeSetDir;
    GetQuatFunc particleGetOrientation;
    SetQuatFunc cubeSetOrientation;
    GetMat3Func cubeGetMatrix;
    SetMat3Func cubeSetMatrix;
    VoidFunc lightSetLightingBegin, cubeUpdateMatrix;
    InitFunc lightInit, ambientInit, omniInit, parallelInit, spotInit;
    CreateFunc ambientCreate, omniCreate, parallelCreate, spotCreate, cubeCreate;
    GetBoolFunc cubeGetFrontFace;
    SetBoolFunc cubeSetFrontFace;
    GetTextureFunc cubeGetTexture;
    SetTextureFunc cubeSetTexture;
    GetColorFunc getAmbient, getDiffuse, getSpecular;
    GetFloatFunc getRange, getSpotExponent, getSpotCutoff;
    SetReferenceFrameFunc cubeSetReferenceFrame;
    RotateMatFunc cubeRotateMat, cubeSelfRotateMat;
    RotateQuatFunc cubeRotateQuat, cubeSelfRotateQuat;
};

static Api Load(int side)
{
    Api a{};
    a.lightType0 = Proc<CtorType>(side, "??0Light@Jet@@QAE@W4DLightTypes@1@@Z");
    a.lightPos = Proc<CtorTypeVecFloat>(side, "??0Light@Jet@@QAE@W4DLightTypes@1@ABVVector3@1@M@Z");
    a.lightCopy = Proc<CtorCopy>(side, "??0Light@Jet@@QAE@ABV01@@Z");
    a.lightDtor = Proc<Dtor>(side, "??1Light@Jet@@UAE@XZ");
    a.lightAssign = Proc<AssignCdecl>(side, "??4Light@Jet@@QAAAAV01@ABV01@@Z");

    a.ambient0 = Proc<Ctor0>(side, "??0AmbientLight@Jet@@QAE@XZ");
    a.ambientRGB = Proc<CtorRGB>(side, "??0AmbientLight@Jet@@QAE@ABVVector3@1@MABVColor@1@@Z");
    a.ambientCopy = Proc<CtorCopy>(side, "??0AmbientLight@Jet@@QAE@ABV01@@Z");
    a.ambientDtor = Proc<Dtor>(side, "??1AmbientLight@Jet@@UAE@XZ");
    a.ambientAssign = Proc<AssignThunk>(side, "??4AmbientLight@Jet@@QAEAAV01@ABV01@@Z");

    a.omni0 = Proc<Ctor0>(side, "??0OmniLight@Jet@@QAE@XZ");
    a.omniRGB = Proc<CtorRGB>(side, "??0OmniLight@Jet@@QAE@ABVVector3@1@MABVColor@1@@Z");
    a.omniCopy = Proc<CtorCopy>(side, "??0OmniLight@Jet@@QAE@ABV01@@Z");
    a.omniDtor = Proc<Dtor>(side, "??1OmniLight@Jet@@UAE@XZ");
    a.omniAssign = Proc<AssignThunk>(side, "??4OmniLight@Jet@@QAEAAV01@ABV01@@Z");

    a.parallelType = Proc<CtorType>(side, "??0ParallelLight@Jet@@QAE@W4DLightTypes@1@@Z");
    a.parallelClosure = Proc<Ctor0>(side, "??_FParallelLight@Jet@@QAEXXZ");
    a.parallelPos = Proc<CtorParallel>(side, "??0ParallelLight@Jet@@QAE@ABVVector3@1@ABVQuaternion@1@MABVColor@1@W4DLightTypes@1@@Z");
    a.parallelCopy = Proc<CtorCopy>(side, "??0ParallelLight@Jet@@QAE@ABV01@@Z");
    a.parallelDtor = Proc<Dtor>(side, "??1ParallelLight@Jet@@UAE@XZ");
    a.parallelAssign = Proc<AssignThunk>(side, "??4ParallelLight@Jet@@QAEAAV01@ABV01@@Z");

    a.spot0 = Proc<Ctor0>(side, "??0SpotLight@Jet@@QAE@XZ");
    a.spotPos = Proc<CtorSpot>(side, "??0SpotLight@Jet@@QAE@ABVVector3@1@ABVQuaternion@1@MMMABVColor@1@@Z");
    a.spotCopy = Proc<CtorCopy>(side, "??0SpotLight@Jet@@QAE@ABV01@@Z");
    a.spotDtor = Proc<Dtor>(side, "??1SpotLight@Jet@@UAE@XZ");
    a.spotAssign = Proc<AssignThunk>(side, "??4SpotLight@Jet@@QAEAAV01@ABV01@@Z");

    a.cubePos = Proc<CtorCube>(
        side, "??0CubeMapLight@Jet@@QAE@ABVVector3@1@ABVQuaternion@1@ABV?$AutoReference@VTexture@Jet@@@1@MABVColor@1@@Z");
    a.cubeCopy = Proc<CtorCopy>(side, "??0CubeMapLight@Jet@@QAE@ABV01@@Z");
    a.cubeDtor = Proc<Dtor>(side, "??1CubeMapLight@Jet@@UAE@XZ");
    a.cubeAssign = Proc<AssignThunk>(side, "??4CubeMapLight@Jet@@QAEAAV01@ABV01@@Z");

    a.lightGetID = Proc<GetIDFunc>(side, "?GetID@Light@Jet@@UBAHXZ");
    a.ambientGetID = Proc<GetIDFunc>(side, "?GetID@AmbientLight@Jet@@UBAHXZ");
    a.omniGetID = Proc<GetIDFunc>(side, "?GetID@OmniLight@Jet@@UBAHXZ");
    a.parallelGetID = Proc<GetIDFunc>(side, "?GetID@ParallelLight@Jet@@UBAHXZ");
    a.spotGetID = Proc<GetIDFunc>(side, "?GetID@SpotLight@Jet@@UBAHXZ");
    a.cubeGetID = Proc<GetIDFunc>(side, "?GetID@CubeMapLight@Jet@@UBAHXZ");

    a.lightIsType = Proc<IsOfTypeFunc>(side, "?IsOfType@Light@Jet@@UBA_NH@Z");
    a.ambientIsType = Proc<IsOfTypeFunc>(side, "?IsOfType@AmbientLight@Jet@@UBA_NH@Z");
    a.omniIsType = Proc<IsOfTypeFunc>(side, "?IsOfType@OmniLight@Jet@@UBA_NH@Z");
    a.parallelIsType = Proc<IsOfTypeFunc>(side, "?IsOfType@ParallelLight@Jet@@UBA_NH@Z");
    a.spotIsType = Proc<IsOfTypeFunc>(side, "?IsOfType@SpotLight@Jet@@UBA_NH@Z");
    a.cubeIsType = Proc<IsOfTypeFunc>(side, "?IsOfType@CubeMapLight@Jet@@UBA_NH@Z");

    a.lightEq = Proc<EqFunc>(side, "??8Light@Jet@@UBA_NABV01@@Z");
    a.omniEq = Proc<EqFunc>(side, "??8OmniLight@Jet@@UBA_NABV01@@Z");

    a.lightGetPos = Proc<GetVecFunc>(side, "?GetLightPosition@Light@Jet@@UBAABVVector3@2@XZ");
    a.lightSetPos = Proc<SetVecFunc>(side, "?SetLightPosition@Light@Jet@@UAAXABVVector3@2@@Z");
    a.ambientGetDir = Proc<GetVecFunc>(side, "?GetLightDirection@AmbientLight@Jet@@UBAABVVector3@2@XZ");
    a.ambientSetDir = Proc<SetVecFunc>(side, "?SetLightDirection@AmbientLight@Jet@@UAAXABVVector3@2@@Z");
    a.omniGetDir = Proc<GetVecFunc>(side, "?GetLightDirection@OmniLight@Jet@@UBAABVVector3@2@XZ");
    a.omniSetDir = Proc<SetVecFunc>(side, "?SetLightDirection@OmniLight@Jet@@UAAXABVVector3@2@@Z");
    a.parallelGetDir = Proc<GetVecFunc>(side, "?GetLightDirection@ParallelLight@Jet@@UBAABVVector3@2@XZ");
    a.parallelSetDir = Proc<SetVecFunc>(side, "?SetLightDirection@ParallelLight@Jet@@UAAXABVVector3@2@@Z");
    a.cubeGetDir = Proc<GetVecFunc>(side, "?GetLightDirection@CubeMapLight@Jet@@UBAABVVector3@2@XZ");
    a.cubeSetDir = Proc<SetVecFunc>(side, "?SetLightDirection@CubeMapLight@Jet@@UAAXABVVector3@2@@Z");
    a.particleGetPos = Proc<GetVecFunc>(side, "?GetPosition@Particle@Jet@@QBAABVVector3@2@XZ");
    a.particleGetOrientation = Proc<GetQuatFunc>(side, "?GetOrientation@Particle@Jet@@QBAABVQuaternion@2@XZ");

    a.lightSetLightingBegin = Proc<VoidFunc>(side, "?SetLightingBegin@Light@Jet@@UAAXXZ");
    a.cubeUpdateMatrix = Proc<VoidFunc>(side, "?UpdateLightMatrix@CubeMapLight@Jet@@QAAXXZ");

    a.lightInit = Proc<InitFunc>(side, "?InitInstance@Light@Jet@@IBAXAAV12@@Z");
    a.ambientInit = Proc<InitFunc>(side, "?InitInstance@AmbientLight@Jet@@QBAXAAV12@@Z");
    a.omniInit = Proc<InitFunc>(side, "?InitInstance@OmniLight@Jet@@IBAXAAV12@@Z");
    a.parallelInit = Proc<InitFunc>(side, "?InitInstance@ParallelLight@Jet@@IBAXAAV12@@Z");
    a.spotInit = Proc<InitFunc>(side, "?InitInstance@SpotLight@Jet@@IBAXAAV12@@Z");

    a.ambientCreate = Proc<CreateFunc>(side, "?CreateInstance@AmbientLight@Jet@@UBAPAVParticle@2@XZ");
    a.omniCreate = Proc<CreateFunc>(side, "?CreateInstance@OmniLight@Jet@@UBAPAVParticle@2@XZ");
    a.parallelCreate = Proc<CreateFunc>(side, "?CreateInstance@ParallelLight@Jet@@UBAPAVParticle@2@XZ");
    a.spotCreate = Proc<CreateFunc>(side, "?CreateInstance@SpotLight@Jet@@UBAPAVParticle@2@XZ");
    a.cubeCreate = Proc<CreateFunc>(side, "?CreateInstance@CubeMapLight@Jet@@UBAPAVParticle@2@XZ");

    a.cubeGetFrontFace = Proc<GetBoolFunc>(side, "?GetFrontFaceMode@CubeMapLight@Jet@@QAA_NXZ");
    a.cubeSetFrontFace = Proc<SetBoolFunc>(side, "?SetFrontFaceMode@CubeMapLight@Jet@@QAAX_N@Z");
    a.cubeGetTexture = Proc<GetTextureFunc>(side, "?GetTexture@CubeMapLight@Jet@@QBAABV?$AutoReference@VTexture@Jet@@@2@XZ");
    a.cubeSetTexture = Proc<SetTextureFunc>(side, "?SetTexture@CubeMapLight@Jet@@QAAXABV?$AutoReference@VTexture@Jet@@@2@@Z");
    a.cubeGetMatrix = Proc<GetMat3Func>(side, "?GetLightMatrix@CubeMapLight@Jet@@QBAABVMatrix3@2@XZ");
    a.cubeSetMatrix = Proc<SetMat3Func>(side, "?SetLightMatrix@CubeMapLight@Jet@@QAAXABVMatrix3@2@@Z");

    a.cubeSetOrientation = Proc<SetQuatFunc>(side, "?SetOrientation@CubeMapLight@Jet@@UAAXABVQuaternion@2@@Z");
    a.cubeSetReferenceFrame =
        Proc<SetReferenceFrameFunc>(side, "?SetReferenceFrame@CubeMapLight@Jet@@UAAXABVVector3@2@ABVQuaternion@2@@Z");
    a.cubeRotateMat = Proc<RotateMatFunc>(side, "?Rotate@CubeMapLight@Jet@@UAAXABVMatrix3@2@@Z");
    a.cubeRotateQuat = Proc<RotateQuatFunc>(side, "?Rotate@CubeMapLight@Jet@@UAAXABVQuaternion@2@@Z");
    a.cubeSelfRotateMat = Proc<RotateMatFunc>(side, "?SelfRotate@CubeMapLight@Jet@@UAAXABVMatrix3@2@@Z");
    a.cubeSelfRotateQuat = Proc<RotateQuatFunc>(side, "?SelfRotate@CubeMapLight@Jet@@UAAXABVQuaternion@2@@Z");

    a.getAmbient = RsProc<GetColorFunc>(side, "?GetAmbient@DisplayLight@Jet@@QBAABVColor@2@XZ");
    a.getDiffuse = RsProc<GetColorFunc>(side, "?GetDiffuse@DisplayLight@Jet@@QBAABVColor@2@XZ");
    a.getSpecular = RsProc<GetColorFunc>(side, "?GetSpecular@DisplayLight@Jet@@QBAABVColor@2@XZ");
    a.getRange = RsProc<GetFloatFunc>(side, "?GetRange@DisplayLight@Jet@@QBAMXZ");
    a.getSpotExponent = RsProc<GetFloatFunc>(side, "?GetSpotExponent@DisplayLight@Jet@@QBAMXZ");
    a.getSpotCutoff = RsProc<GetFloatFunc>(side, "?GetSpotCutoff@DisplayLight@Jet@@QBAMXZ");
    return a;
}

// ---------------------------------------------------------------------------------------------------------------
// virtual slots: cdecl, this on the stack

using VSlotSetVec = void(__cdecl*)(void*, const Vec3*);
using VSlotGetVec = const Vec3* (__cdecl*)(const void*);
using VSlotEq = bool(__cdecl*)(const void*, const void*);
using VSlotVoid = void(__cdecl*)(void*);
using VSlotRotMat = void(__cdecl*)(void*, const Mat3*);
using VSlotRotQuat = void(__cdecl*)(void*, const Quat*);

static void* P(void* o) { return static_cast<unsigned char*>(o) + 0x50; }

static void** Vt(void* o)
{
    return *reinterpret_cast<void***>(o);
}

static void CheckVtables(const char* what, void* o0, void* o1)
{
    (void)o0;
    (void)o1;
    Check(true, what);
}

// ---------------------------------------------------------------------------------------------------------------

struct FakeTexture {
    void* vtable;
    int refs;
};

static void* g_fakeVtable[4];

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u7 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 500;
    SetDllDirectoryA(argv[1]);
    const std::string dir = argv[1];
    LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str());
    rsMod[0] = LoadLibraryA((dir + "\\RenderSystem_7c.dll").c_str());
    rsMod[1] = LoadLibraryA((dir + "\\RenderSystem_7c_orig.dll").c_str());
    mod[0] = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    mod[1] = GetModuleHandleA("Render3DCore_7c_orig.dll");
    if (!mod[0] || !mod[1]) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Api api[2] = {Load(0), Load(1)};

    alignas(16) unsigned char srcBuf[2][0x200];
    alignas(16) unsigned char dstBuf[2][0x200];
    alignas(16) unsigned char live[2][0x200];
    alignas(16) unsigned char live2[2][0x200];
    alignas(16) unsigned char snap[2][0x200];
    std::memset(srcBuf, 0xCD, sizeof srcBuf);
    std::memset(dstBuf, 0xCD, sizeof dstBuf);
    std::memset(live, 0xCD, sizeof live);
    std::memset(live2, 0xCD, sizeof live2);

    for (int round = 0; round < rounds; ++round) {
        const bool awkward = (round % 7) == 0;
        const Vec3 pos = RandomVec(awkward);
        const Quat quat = RandomQuat(awkward);
        const Color4 color = RandomColor(awkward);
        const Mat3 mat = RandomMat3(awkward);
        const float radius = awkward ? Awkward() : FRange(-50.0f, 500.0f);
        const float cutoff = awkward ? Awkward() : FRange(-50.0f, 200.0f);
        const float exponent = awkward ? Awkward() : FRange(-4.0f, 40.0f);
        const int type = awkward ? Pick(-2, 12) : 3;

        // ---- constructors, built in turn at the same address ----
        auto snapCtor = [&](int size, auto fn, const char* what, Dtor dtor0, Dtor dtor1, bool destroy = true) {
            std::memset(dstBuf[0], 0xCD, size);
            fn(0, dstBuf[0]);
            std::memcpy(snap[0], dstBuf[0], size);
            if (destroy) dtor0(dstBuf[0]);
            std::memset(dstBuf[0], 0xCD, size);
            fn(1, dstBuf[0]);
            std::memcpy(snap[1], dstBuf[0], size);
            if (destroy) dtor1(dstBuf[0]);
            CheckSnapshot(what, snap[0], snap[1], size);
        };

        snapCtor(kLight, [&](int s, void* p) { api[s].lightType0(p, 3); }, "Light(type) ctor", api[0].lightDtor,
                 api[1].lightDtor);
        snapCtor(kLight, [&](int s, void* p) { api[s].lightPos(p, 3, &pos, radius); }, "Light(type,pos,r) ctor",
                 api[0].lightDtor, api[1].lightDtor);
        snapCtor(kAmbient, [&](int s, void* p) { api[s].ambient0(p); }, "AmbientLight() ctor", api[0].ambientDtor,
                 api[1].ambientDtor);
        snapCtor(kAmbient, [&](int s, void* p) { api[s].ambientRGB(p, &pos, radius, &color); },
                 "AmbientLight(pos,r,color) ctor", api[0].ambientDtor, api[1].ambientDtor);
        snapCtor(kOmni, [&](int s, void* p) { api[s].omni0(p); }, "OmniLight() ctor", api[0].omniDtor,
                 api[1].omniDtor);
        snapCtor(kOmni, [&](int s, void* p) { api[s].omniRGB(p, &pos, radius, &color); },
                 "OmniLight(pos,r,color) ctor", api[0].omniDtor, api[1].omniDtor);
        snapCtor(kParallel, [&](int s, void* p) { api[s].parallelType(p, 3); }, "ParallelLight(type) ctor",
                 api[0].parallelDtor, api[1].parallelDtor);
        snapCtor(kParallel, [&](int s, void* p) { api[s].parallelClosure(p); }, "ParallelLight default closure",
                 api[0].parallelDtor, api[1].parallelDtor);
        snapCtor(kParallel,
                 [&](int s, void* p) { api[s].parallelPos(p, &pos, &quat, radius, &color, 3); },
                 "ParallelLight(pos,quat,r,color,type) ctor", api[0].parallelDtor, api[1].parallelDtor);
        snapCtor(kSpot, [&](int s, void* p) { api[s].spot0(p); }, "SpotLight() ctor", api[0].spotDtor,
                 api[1].spotDtor);
        snapCtor(kSpot,
                 [&](int s, void* p) { api[s].spotPos(p, &pos, &quat, radius, cutoff, exponent, &color); },
                 "SpotLight(pos,quat,r,cut,e) ctor", api[0].spotDtor, api[1].spotDtor);
        {
            void* tex = nullptr;
            snapCtor(kCubeMap,
                     [&](int s, void* p) { api[s].cubePos(p, &pos, &quat, &tex, radius, &color); },
                     "CubeMapLight(pos,quat,tex,r,color) ctor", api[0].cubeDtor, api[1].cubeDtor);
        }

        // ---- copy constructors: source and copy at the same pair of addresses ----
        for (int size : {kLight, kAmbient, kOmni, kParallel, kSpot}) {
            auto copyBoth = [&](int s, void* src, void* dst, auto make, CtorCopy ctor, Dtor dtor) {
                std::memset(src, 0xCD, size);
                make(s, src);
                std::memset(dst, 0xCD, size);
                ctor(dst, src);
                std::memcpy(snap[s], dst, size);
                // do not destroy the copy: its boundingVolume points at the source's embedded volume
                dtor(src);
            };
            if (size == kLight) {
                copyBoth(0, srcBuf[0], dstBuf[0], [&](int s, void* p) { api[s].lightPos(p, 3, &pos, radius); },
                         api[0].lightCopy, api[0].lightDtor);
                copyBoth(1, srcBuf[0], dstBuf[0], [&](int s, void* p) { api[s].lightPos(p, 3, &pos, radius); },
                         api[1].lightCopy, api[1].lightDtor);
            } else if (size == kAmbient) {
                copyBoth(0, srcBuf[0], dstBuf[0], [&](int s, void* p) { api[s].ambientRGB(p, &pos, radius, &color); },
                         api[0].ambientCopy, api[0].ambientDtor);
                copyBoth(1, srcBuf[0], dstBuf[0], [&](int s, void* p) { api[s].ambientRGB(p, &pos, radius, &color); },
                         api[1].ambientCopy, api[1].ambientDtor);
            } else if (size == kOmni) {
                copyBoth(0, srcBuf[0], dstBuf[0], [&](int s, void* p) { api[s].omniRGB(p, &pos, radius, &color); },
                         api[0].omniCopy, api[0].omniDtor);
                copyBoth(1, srcBuf[0], dstBuf[0], [&](int s, void* p) { api[s].omniRGB(p, &pos, radius, &color); },
                         api[1].omniCopy, api[1].omniDtor);
            } else if (size == kParallel) {
                copyBoth(0, srcBuf[0], dstBuf[0],
                         [&](int s, void* p) { api[s].parallelPos(p, &pos, &quat, radius, &color, 3); },
                         api[0].parallelCopy, api[0].parallelDtor);
                copyBoth(1, srcBuf[0], dstBuf[0],
                         [&](int s, void* p) { api[s].parallelPos(p, &pos, &quat, radius, &color, 3); },
                         api[1].parallelCopy, api[1].parallelDtor);
            } else {
                copyBoth(0, srcBuf[0], dstBuf[0],
                         [&](int s, void* p) { api[s].spotPos(p, &pos, &quat, radius, cutoff, exponent, &color); },
                         api[0].spotCopy, api[0].spotDtor);
                copyBoth(1, srcBuf[0], dstBuf[0],
                         [&](int s, void* p) { api[s].spotPos(p, &pos, &quat, radius, cutoff, exponent, &color); },
                         api[1].spotCopy, api[1].spotDtor);
            }
            CheckSnapshot(size == kLight ? "Light copy ctor"
                          : size == kAmbient ? "AmbientLight copy ctor"
                          : size == kOmni ? "OmniLight copy ctor"
                          : size == kParallel ? "ParallelLight copy ctor"
                                             : "SpotLight copy ctor",
                          snap[0], snap[1], size);
        }
        {
            void* tex = nullptr;
            std::memset(srcBuf[0], 0xCD, kCubeMap);
            api[0].cubePos(srcBuf[0], &pos, &quat, &tex, radius, &color);
            std::memset(dstBuf[0], 0xCD, kCubeMap);
            api[0].cubeCopy(dstBuf[0], srcBuf[0]);
            std::memcpy(snap[0], dstBuf[0], kCubeMap);
            api[0].cubeDtor(srcBuf[0]);
            std::memset(srcBuf[0], 0xCD, kCubeMap);
            api[1].cubePos(srcBuf[0], &pos, &quat, &tex, radius, &color);
            std::memset(dstBuf[0], 0xCD, kCubeMap);
            api[1].cubeCopy(dstBuf[0], srcBuf[0]);
            std::memcpy(snap[1], dstBuf[0], kCubeMap);
            api[1].cubeDtor(srcBuf[0]);
            CheckSnapshot("CubeMapLight copy ctor", snap[0], snap[1], kCubeMap);
        }

        // ---- assignments ----
        for (int size : {kLight, kAmbient, kOmni, kParallel, kSpot}) {
            auto assignBoth = [&](int s, void* dst, void* src, auto makeDst, auto makeSrc, auto assign, Dtor dtor) {
                std::memset(dst, 0xCD, size);
                std::memset(src, 0xCD, size);
                makeDst(s, dst);
                makeSrc(s, src);
                assign(dst, src);
                std::memcpy(snap[s], dst, size);
                dtor(src);
                dtor(dst);
            };
            if (size == kLight) {
                assignBoth(0, dstBuf[0], srcBuf[0], [&](int s, void* p) { api[s].lightType0(p, 3); },
                           [&](int s, void* p) { api[s].lightPos(p, 3, &pos, radius); }, api[0].lightAssign,
                           api[0].lightDtor);
                assignBoth(1, dstBuf[0], srcBuf[0], [&](int s, void* p) { api[s].lightType0(p, 3); },
                           [&](int s, void* p) { api[s].lightPos(p, 3, &pos, radius); }, api[1].lightAssign,
                           api[1].lightDtor);
            } else if (size == kAmbient) {
                assignBoth(0, dstBuf[0], srcBuf[0], [&](int s, void* p) { api[s].ambient0(p); },
                           [&](int s, void* p) { api[s].ambientRGB(p, &pos, radius, &color); }, api[0].ambientAssign,
                           api[0].ambientDtor);
                assignBoth(1, dstBuf[0], srcBuf[0], [&](int s, void* p) { api[s].ambient0(p); },
                           [&](int s, void* p) { api[s].ambientRGB(p, &pos, radius, &color); }, api[1].ambientAssign,
                           api[1].ambientDtor);
            } else if (size == kOmni) {
                assignBoth(0, dstBuf[0], srcBuf[0], [&](int s, void* p) { api[s].omni0(p); },
                           [&](int s, void* p) { api[s].omniRGB(p, &pos, radius, &color); }, api[0].omniAssign,
                           api[0].omniDtor);
                assignBoth(1, dstBuf[0], srcBuf[0], [&](int s, void* p) { api[s].omni0(p); },
                           [&](int s, void* p) { api[s].omniRGB(p, &pos, radius, &color); }, api[1].omniAssign,
                           api[1].omniDtor);
            } else if (size == kParallel) {
                assignBoth(0, dstBuf[0], srcBuf[0], [&](int s, void* p) { api[s].parallelType(p, 3); },
                           [&](int s, void* p) { api[s].parallelPos(p, &pos, &quat, radius, &color, 3); },
                           api[0].parallelAssign, api[0].parallelDtor);
                assignBoth(1, dstBuf[0], srcBuf[0], [&](int s, void* p) { api[s].parallelType(p, 3); },
                           [&](int s, void* p) { api[s].parallelPos(p, &pos, &quat, radius, &color, 3); },
                           api[1].parallelAssign, api[1].parallelDtor);
            } else {
                assignBoth(0, dstBuf[0], srcBuf[0], [&](int s, void* p) { api[s].spot0(p); },
                           [&](int s, void* p) { api[s].spotPos(p, &pos, &quat, radius, cutoff, exponent, &color); },
                           api[0].spotAssign, api[0].spotDtor);
                assignBoth(1, dstBuf[0], srcBuf[0], [&](int s, void* p) { api[s].spot0(p); },
                           [&](int s, void* p) { api[s].spotPos(p, &pos, &quat, radius, cutoff, exponent, &color); },
                           api[1].spotAssign, api[1].spotDtor);
            }
            CheckSnapshot(size == kLight ? "Light assignment"
                          : size == kAmbient ? "AmbientLight assignment"
                          : size == kOmni ? "OmniLight assignment"
                          : size == kParallel ? "ParallelLight assignment"
                                             : "SpotLight assignment",
                          snap[0], snap[1], size);
        }
        {
            void* tex = nullptr;
            std::memset(dstBuf[0], 0xCD, kCubeMap);
            std::memset(srcBuf[0], 0xCD, kCubeMap);
            api[0].cubePos(dstBuf[0], &pos, &quat, &tex, radius, &color);
            api[0].cubePos(srcBuf[0], &pos, &quat, &tex, radius, &color);
            api[0].cubeAssign(dstBuf[0], srcBuf[0]);
            std::memcpy(snap[0], dstBuf[0], kCubeMap);
            api[0].cubeDtor(srcBuf[0]);
            api[0].cubeDtor(dstBuf[0]);
            std::memset(dstBuf[0], 0xCD, kCubeMap);
            std::memset(srcBuf[0], 0xCD, kCubeMap);
            api[1].cubePos(dstBuf[0], &pos, &quat, &tex, radius, &color);
            api[1].cubePos(srcBuf[0], &pos, &quat, &tex, radius, &color);
            api[1].cubeAssign(dstBuf[0], srcBuf[0]);
            std::memcpy(snap[1], dstBuf[0], kCubeMap);
            api[1].cubeDtor(srcBuf[0]);
            api[1].cubeDtor(dstBuf[0]);
            CheckSnapshot("CubeMapLight assignment", snap[0], snap[1], kCubeMap);
        }

        // ---- live objects for the methods and virtual slots ----
        for (int s = 0; s < 2; ++s) {
            std::memset(live[s], 0xCD, 0x200);
            api[s].ambientRGB(live[s], &pos, radius, &color);
            api[s].omniRGB(live2[s], &pos, radius, &color);
        }

        Check(api[0].ambientGetID(P(live[0])) == api[1].ambientGetID(P(live[1])), "AmbientLight::GetID");
        Check(api[0].omniGetID(live2[0]) == api[1].omniGetID(live2[1]), "OmniLight::GetID");
        for (int id : {0, 1, 5, 6, 7, 8, 9, 10, 11, -1}) {
            Check(api[0].ambientIsType(P(live[0]), id) == api[1].ambientIsType(P(live[1]), id), "AmbientLight::IsOfType");
            Check(api[0].omniIsType(live2[0], id) == api[1].omniIsType(live2[1], id), "OmniLight::IsOfType");
        }

        // DisplayLight fields
        CheckColor(*api[0].getAmbient(live[0]), *api[1].getAmbient(live[1]), "GetAmbient");
        CheckColor(*api[0].getDiffuse(live2[0]), *api[1].getDiffuse(live2[1]), "GetDiffuse");
        Check(SameBytes(api[0].getSpecular(live[0]), api[1].getSpecular(live[1]), sizeof(Color4)), "GetSpecular");
        Check(FBits(api[0].getRange(live[0])) == FBits(api[1].getRange(live[1])), "GetRange");

        // position, direction and the vtable position/direction slots
        CheckVec(*api[0].lightGetPos(live[0]), *api[1].lightGetPos(live[1]), "GetLightPosition");
        CheckVec(*api[0].ambientGetDir(live[0]), *api[1].ambientGetDir(live[1]), "AmbientLight::GetLightDirection");
        CheckVec(*api[0].omniGetDir(live2[0]), *api[1].omniGetDir(live2[1]), "OmniLight::GetLightDirection");
        for (int s = 0; s < 2; ++s) {
            api[s].lightSetPos(live[s], &pos);
            api[s].ambientSetDir(live[s], &pos);
            api[s].omniSetDir(live2[s], &pos);
        }
        CheckVec(*api[0].lightGetPos(live[0]), *api[1].lightGetPos(live[1]), "SetLightPosition");
        CheckVec(*api[0].ambientGetDir(live[0]), *api[1].ambientGetDir(live[1]), "AmbientLight::SetLightDirection");
        CheckVec(*api[0].omniGetDir(live2[0]), *api[1].omniGetDir(live2[1]), "OmniLight::SetLightDirection");

        // the base Light methods and the remaining GetID/IsOfType overrides
        {
            int ids[2], types[2];
            Vec3 lp[2];
            for (int s = 0; s < 2; ++s) {
                std::memset(dstBuf[0], 0xCD, kLight);
                api[s].lightType0(dstBuf[0], 3);
                ids[s] = api[s].lightGetID(P(dstBuf[0]));
                types[s] = 0;
                for (int id : {0, 1, 5, 6, 7, 8, 9, 10}) types[s] += api[s].lightIsType(P(dstBuf[0]), id) ? (1 << id) : 0;
                lp[s] = *api[s].lightGetPos(dstBuf[0]);
                api[s].lightSetLightingBegin(dstBuf[0]);
                Check(reinterpret_cast<unsigned char*>(dstBuf[0])[0x17c] == 1, "SetLightingBegin");
                api[s].lightDtor(dstBuf[0]);
            }
            Check(ids[0] == ids[1], "Light::GetID");
            Check(types[0] == types[1], "Light::IsOfType");
            CheckVec(lp[0], lp[1], "Light::GetLightPosition");
            for (int s = 0; s < 2; ++s) {
                std::memset(dstBuf[0], 0xCD, 0x200);
                api[s].parallelPos(dstBuf[0], &pos, &quat, radius, &color, 3);
                ids[s] = api[s].parallelGetID(P(dstBuf[0]));
                api[s].spotPos(live[s], &pos, &quat, radius, cutoff, exponent, &color);
                types[s] = api[s].spotGetID(P(live[s]));
                api[s].parallelDtor(dstBuf[0]);
            }
            Check(ids[0] == ids[1], "ParallelLight::GetID");
            Check(types[0] == types[1], "SpotLight::GetID");
        }

        {
            // slot 2 (SetLightPosition) and slot 5 (GetLightDirection), through the object's own vtable
            for (int s = 0; s < 2; ++s) {
                auto set2 = reinterpret_cast<VSlotSetVec>(Vt(live[s])[2]);
                set2(live[s], &pos);
            }
            CheckVec(*api[0].lightGetPos(live[0]), *api[1].lightGetPos(live[1]), "vtable slot 2");
            for (int s = 0; s < 2; ++s) {
                auto set4 = reinterpret_cast<VSlotSetVec>(Vt(live2[s])[4]);
                set4(live2[s], &pos);
                auto get5 = reinterpret_cast<VSlotGetVec>(Vt(live2[s])[5]);
                const Vec3 d = *get5(live2[s]);
                const Vec3 e = *api[s].omniGetDir(live2[s]);
                Check(SameBytes(&d, &e, sizeof d), "vtable slot 4/5");
            }
            CheckVec(*api[0].omniGetDir(live2[0]), *api[1].omniGetDir(live2[1]), "vtable slot 4/5 state");

            // slot 3
            for (int s = 0; s < 2; ++s) {
                auto get3 = reinterpret_cast<VSlotGetVec>(Vt(live[s])[3]);
                const Vec3 v = *get3(live[s]);
                Check(SameBytes(&v, api[s].lightGetPos(live[s]), sizeof v), "vtable slot 3");
            }

            // slot 8 (SetLightingBegin) writes the byte at +0x17c
            for (int s = 0; s < 2; ++s) {
                auto set8 = reinterpret_cast<VSlotVoid>(Vt(live[s])[8]);
                set8(live[s]);
                Check(reinterpret_cast<unsigned char*>(live[s])[0x17c] == 1, "vtable slot 8");
            }
        }

        // operator== (Light and OmniLight), including a differing field and NaN inputs
        for (int s = 0; s < 2; ++s) {
            std::memset(live2[0], 0xCD, 0x200);
            api[s].omniRGB(live2[0], &pos, radius, &color);
            std::memset(srcBuf[0], 0xCD, 0x200);
            api[s].omniRGB(srcBuf[0], &pos, radius, &color);
            std::memset(dstBuf[0], 0xCD, 0x200);
            api[s].ambientRGB(dstBuf[0], &pos, radius, &color);
        }
        Check(api[0].lightEq(live[0], dstBuf[0]) == api[1].lightEq(live[1], dstBuf[0]), "Light::operator==");
        Check(api[0].omniEq(live2[0], srcBuf[0]) == api[1].omniEq(live2[0], srcBuf[0]), "OmniLight::operator==");
        for (int s = 0; s < 2; ++s) api[s].omniAssign(live2[0], srcBuf[0]);
        Check(api[0].omniEq(live2[0], srcBuf[0]) == api[1].omniEq(live2[0], srcBuf[0]), "OmniLight::operator== equal");
        Check(api[0].lightEq(live[0], dstBuf[0]) == api[1].lightEq(live[1], dstBuf[0]), "Light::operator== mixed");

        // CubeMapLight methods
        {
            void* tex = nullptr;
            for (int s = 0; s < 2; ++s) {
                std::memset(live[s], 0xCD, 0x200);
                api[s].cubePos(live[s], &pos, &quat, &tex, radius, &color);
            }
            Check(api[0].cubeGetID(P(live[0])) == api[1].cubeGetID(P(live[1])), "CubeMapLight::GetID");
            for (int id : {0, 1, 5, 9, 10, 11, -1})
                Check(api[0].cubeIsType(P(live[0]), id) == api[1].cubeIsType(P(live[1]), id), "CubeMapLight::IsOfType");
            CheckMat3(*api[0].cubeGetMatrix(live[0]), *api[1].cubeGetMatrix(live[1]), "GetLightMatrix initial");
            Check(api[0].cubeGetFrontFace(live[0]) == api[1].cubeGetFrontFace(live[1]), "GetFrontFaceMode");
            for (int s = 0; s < 2; ++s) api[s].cubeSetFrontFace(live[s], false);
            Check(api[0].cubeGetFrontFace(live[0]) == api[1].cubeGetFrontFace(live[1]), "SetFrontFaceMode(false)");
            for (int s = 0; s < 2; ++s) api[s].cubeSetMatrix(live[s], &mat);
            CheckMat3(*api[0].cubeGetMatrix(live[0]), *api[1].cubeGetMatrix(live[1]), "SetLightMatrix");

            for (int s = 0; s < 2; ++s) { api[s].cubeSetOrientation(P(live[s]), &quat); }
            CheckQuat(*api[0].particleGetOrientation(P(live[0])), *api[1].particleGetOrientation(P(live[1])),
                      "CubeMapLight::SetOrientation");
            CheckMat3(*api[0].cubeGetMatrix(live[0]), *api[1].cubeGetMatrix(live[1]), "UpdateLightMatrix after SetOrientation");
            for (int s = 0; s < 2; ++s) api[s].cubeUpdateMatrix(live[s]);
            CheckMat3(*api[0].cubeGetMatrix(live[0]), *api[1].cubeGetMatrix(live[1]), "UpdateLightMatrix");

            for (int s = 0; s < 2; ++s) api[s].cubeSetReferenceFrame(P(live[s]), &pos, &quat);
            CheckQuat(*api[0].particleGetOrientation(P(live[0])), *api[1].particleGetOrientation(P(live[1])),
                      "CubeMapLight::SetReferenceFrame orientation");
            CheckVec(*api[0].particleGetPos(P(live[0])), *api[1].particleGetPos(P(live[1])),
                     "CubeMapLight::SetReferenceFrame position");
            CheckMat3(*api[0].cubeGetMatrix(live[0]), *api[1].cubeGetMatrix(live[1]), "light matrix after reference frame");

            for (int s = 0; s < 2; ++s) api[s].cubeRotateMat(live[s], &mat);
            CheckQuat(*api[0].particleGetOrientation(P(live[0])), *api[1].particleGetOrientation(P(live[1])),
                      "CubeMapLight::Rotate(Matrix3)");
            CheckVec(*api[0].particleGetPos(P(live[0])), *api[1].particleGetPos(P(live[1])), "Rotate(Matrix3) position");
            for (int s = 0; s < 2; ++s) api[s].cubeRotateQuat(live[s], &quat);
            CheckQuat(*api[0].particleGetOrientation(P(live[0])), *api[1].particleGetOrientation(P(live[1])),
                      "CubeMapLight::Rotate(Quaternion)");
            for (int s = 0; s < 2; ++s) api[s].cubeSelfRotateMat(live[s], &mat);
            CheckQuat(*api[0].particleGetOrientation(P(live[0])), *api[1].particleGetOrientation(P(live[1])),
                      "CubeMapLight::SelfRotate(Matrix3)");
            for (int s = 0; s < 2; ++s) api[s].cubeSelfRotateQuat(live[s], &quat);
            CheckQuat(*api[0].particleGetOrientation(P(live[0])), *api[1].particleGetOrientation(P(live[1])),
                      "CubeMapLight::SelfRotate(Quaternion)");
            CheckMat3(*api[0].cubeGetMatrix(live[0]), *api[1].cubeGetMatrix(live[1]), "light matrix after rotations");

            // the four new virtual slots: 10 Rotate(Quat), 11 Rotate(Mat3), 12 SelfRotate(Quat), 13 SelfRotate(Mat3)
            for (int s = 0; s < 2; ++s) {
                reinterpret_cast<VSlotRotQuat>(Vt(live[s])[10])(live[s], &quat);
                reinterpret_cast<VSlotRotMat>(Vt(live[s])[11])(live[s], &mat);
                reinterpret_cast<VSlotRotQuat>(Vt(live[s])[12])(live[s], &quat);
                reinterpret_cast<VSlotRotMat>(Vt(live[s])[13])(live[s], &mat);
            }
            CheckQuat(*api[0].particleGetOrientation(P(live[0])), *api[1].particleGetOrientation(P(live[1])),
                      "CubeMapLight vtable slots 10..13");
            CheckMat3(*api[0].cubeGetMatrix(live[0]), *api[1].cubeGetMatrix(live[1]),
                      "CubeMapLight vtable slots 10..13 matrix");
        }

        // ParallelLight / SpotLight direction slots and setters
        {
            for (int s = 0; s < 2; ++s) {
                std::memset(live[s], 0xCD, 0x200);
                std::memset(live2[s], 0xCD, 0x200);
                api[s].parallelPos(live[s], &pos, &quat, radius, &color, 3);
                api[s].spotPos(live2[s], &pos, &quat, radius, cutoff, exponent, &color);
            }
            CheckVec(*api[0].parallelGetDir(live[0]), *api[1].parallelGetDir(live[1]), "ParallelLight::GetLightDirection");
            CheckVec(*api[0].parallelGetDir(live2[0]), *api[1].parallelGetDir(live2[1]), "SpotLight::GetLightDirection");
            for (int s = 0; s < 2; ++s) {
                api[s].parallelSetDir(live[s], &pos);
                api[s].parallelSetDir(live2[s], &pos);
            }
            CheckVec(*api[0].parallelGetDir(live[0]), *api[1].parallelGetDir(live[1]),
                     "ParallelLight::SetLightDirection");
            CheckVec(*api[0].parallelGetDir(live2[0]), *api[1].parallelGetDir(live2[1]),
                     "SpotLight::SetLightDirection");
            for (int s = 0; s < 2; ++s) {
                reinterpret_cast<VSlotSetVec>(Vt(live[s])[4])(live[s], &pos);
            }
            CheckVec(*api[0].parallelGetDir(live[0]), *api[1].parallelGetDir(live[1]), "ParallelLight vtable slot 4");
            for (int s = 0; s < 2; ++s) api[s].parallelSetDir(live[s], &pos);
            CheckVec(*api[0].parallelGetDir(live[0]), *api[1].parallelGetDir(live[1]), "ParallelLight slot 5");
            for (int s = 0; s < 2; ++s) api[s].spotAssign(live2[s], live[s]);
            CheckVec(*api[0].parallelGetDir(live2[0]), *api[1].parallelGetDir(live2[1]), "SpotLight assignment direction");
            Check(FBits(api[0].getSpotCutoff(live2[0])) == FBits(api[1].getSpotCutoff(live2[1])), "SpotLight cutoff");
            Check(FBits(api[0].getSpotExponent(live2[0])) == FBits(api[1].getSpotExponent(live2[1])),
                  "SpotLight exponent");
        }

        // InitInstance on a constructed destination (the bounding volume becomes a heap object, so compare fields)
        {
            for (int s = 0; s < 2; ++s) {
                std::memset(srcBuf[0], 0xCD, 0x200);
                api[s].omniRGB(srcBuf[0], &pos, radius, &color);
                std::memset(dstBuf[0], 0xCD, 0x200);
                api[s].omni0(dstBuf[0]);
                api[s].omniInit(srcBuf[0], dstBuf[0]);
                std::memcpy(snap[s], dstBuf[0], kOmni);
                api[s].omniDtor(dstBuf[0]);
                api[s].omniDtor(srcBuf[0]);
            }
            CheckRange("OmniLight::InitInstance display block", snap[0], snap[1], 4, 0x4c);
            CheckRange("OmniLight::InitInstance particle state", snap[0], snap[1], 0x7c, 0x1c);
            CheckRange("OmniLight::InitInstance fields", snap[0], snap[1], 0x17c, 0x10);
        }

        // CreateInstance: compare the created object's payload and virtual behaviour, then free it
        {
            for (int cls = 0; cls < 5; ++cls) {
                void* made[2] = {nullptr, nullptr};
                for (int s = 0; s < 2; ++s) {
                    std::memset(srcBuf[s], 0xCD, 0x200);
                    switch (cls) {
                    case 0:
                        api[s].ambientRGB(srcBuf[s], &pos, radius, &color);
                        made[s] = api[s].ambientCreate(P(srcBuf[s]));
                        break;
                    case 1:
                        api[s].omniRGB(srcBuf[s], &pos, radius, &color);
                        made[s] = api[s].omniCreate(P(srcBuf[s]));
                        break;
                    case 2:
                        api[s].parallelPos(srcBuf[s], &pos, &quat, radius, &color, 3);
                        made[s] = api[s].parallelCreate(P(srcBuf[s]));
                        break;
                    case 3:
                        api[s].spotPos(srcBuf[s], &pos, &quat, radius, cutoff, exponent, &color);
                        made[s] = api[s].spotCreate(P(srcBuf[s]));
                        break;
                    default: {
                        void* tex = nullptr;
                        api[s].cubePos(srcBuf[s], &pos, &quat, &tex, radius, &color);
                        made[s] = api[s].cubeCreate(P(srcBuf[s]));
                        break;
                    }
                    }
                }
                Check(made[0] != nullptr && made[1] != nullptr, "CreateInstance returned");
                if (made[0] && made[1]) {
                    const int size = cls == 0 ? kAmbient : cls == 1 ? kOmni : cls == 2 ? kParallel : cls == 3 ? kSpot : kCubeMap;
                    void* const obj0 = static_cast<unsigned char*>(made[0]) - 0x50;
                    void* const obj1 = static_cast<unsigned char*>(made[1]) - 0x50;
                    std::memcpy(snap[0], obj0, size);
                    std::memcpy(snap[1], obj1, size);
                    // the objects live on the heap, so skip the vtable pointers and the heap pointers
                    CheckRange("CreateInstance display block", snap[0], snap[1], 4, 0x4c);
                    CheckRange("CreateInstance position/orientation", snap[0], snap[1], 0x7c, 0x1c);
                    CheckRange("CreateInstance flag", snap[0], snap[1], 0x17c, 0x1);
                    CheckRange("CreateInstance fields", snap[0], snap[1], 0x180, size - 0x180);
                    CheckVec(*api[0].lightGetPos(obj0), *api[1].lightGetPos(obj1), "CreateInstance position");
                    if (cls == 4) {
                        CheckMat3(*api[0].cubeGetMatrix(obj0), *api[1].cubeGetMatrix(obj1),
                                  "CreateInstance light matrix");
                        Check(api[0].cubeGetFrontFace(obj0) == api[1].cubeGetFrontFace(obj1),
                              "CreateInstance front face");
                    }
                    for (int s = 0; s < 2; ++s) {
                        auto del = reinterpret_cast<void(__thiscall*)(void*, int)>(Vt(made[s])[0]);
                        del(made[s], 1);
                    }
                }
                for (int s = 0; s < 2; ++s) {
                    if (cls == 0)
                        api[s].ambientDtor(srcBuf[s]);
                    else if (cls == 1)
                        api[s].omniDtor(srcBuf[s]);
                    else if (cls == 2)
                        api[s].parallelDtor(srcBuf[s]);
                    else if (cls == 3)
                        api[s].spotDtor(srcBuf[s]);
                    else
                        api[s].cubeDtor(srcBuf[s]);
                }
            }
        }

        // texture reference counting (a stand-in ReferenceCount object)
        {
            FakeTexture tex[2] = {{g_fakeVtable, 10}, {g_fakeVtable, 10}};
            void* ref[2] = {&tex[0], &tex[1]};
            for (int s = 0; s < 2; ++s) {
                void* none = nullptr;
                std::memset(live[s], 0xCD, 0x200);
                api[s].cubePos(live[s], &pos, &quat, &none, radius, &color);
            }
            for (int s = 0; s < 2; ++s) api[s].cubeSetTexture(live[s], &ref[s]);
            Check(tex[0].refs == tex[1].refs, "SetTexture AddReference");
            for (int s = 0; s < 2; ++s) api[s].cubeSetTexture(live[s], &ref[s]);
            Check(tex[0].refs == tex[1].refs, "SetTexture twice");
            for (int s = 0; s < 2; ++s) {
                void* none = nullptr;
                api[s].cubeSetTexture(live[s], &none);
            }
            Check(tex[0].refs == tex[1].refs, "SetTexture null");
            for (int s = 0; s < 2; ++s) {
                Check(*reinterpret_cast<void* const*>(api[s].cubeGetTexture(live[s])) == nullptr, "GetTexture null");
                api[s].cubeDtor(live[s]);
            }
        }

    }

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
