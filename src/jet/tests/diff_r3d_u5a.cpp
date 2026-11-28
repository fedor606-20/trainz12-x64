// Differential test of Render3DCore_7c unit U5a: our material methods against Render3DCore_7c_orig.dll in one process.
//
// The original module constructs the material objects in the game, so this test never calls a constructor: it builds
// byte images with the class's original vftable and the TS12 field offsets, gives both modules the same image, and
// compares the whole object, the return value and (for the Bind/display-state members) the render state the calls
// leave in a stand-in Display. Texture fields point at a stand-in texture whose virtuals record the calls.
//
// usage: diff_r3d_u5a.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

namespace {

LONG CALLBACK VectoredHandler(EXCEPTION_POINTERS* info)
{
    HMODULE module = nullptr;
    const void* address = info->ExceptionRecord->ExceptionAddress;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCSTR>(address), &module);
    char name[MAX_PATH] = "?";
    if (module) GetModuleFileNameA(module, name, sizeof name);
    std::printf("FAULT %p (%s+%x) in %s code %08x\n", address,
                module ? name : "?", module ? unsigned(std::uintptr_t(address) - std::uintptr_t(module)) : 0,
                module ? name : "?", info->ExceptionRecord->ExceptionCode);
    std::fflush(stdout);
    return EXCEPTION_CONTINUE_SEARCH;
}

constexpr unsigned kObjSize = 0x140;
constexpr unsigned kStateSize = 0x400;
unsigned long long checks, bad;
std::mt19937 rng(0x5eed5a);

int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }
float Bits(std::uint32_t u)
{
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
float Awkward()
{
    switch (Pick(0, 13)) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: return Bits(0x7f800000u);
    case 3: return Bits(0xff800000u);
    case 4: return Bits(0x7fc00000u);
    case 5: return Bits(0xffc00001u);
    case 6: return Bits(0x7fa00000u);
    case 7: return Bits(0xffa00000u);
    case 8: return 1.0f;
    case 9: return -1.0f;
    default: return std::uniform_real_distribution<float>(-1000.0f, 1000.0f)(rng);
    }
}
void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        std::printf("MISMATCH %s\n", what);
    }
}
bool Same(const void* a, const void* b, unsigned n)
{
    return std::memcmp(a, b, n) == 0;
}
void PrintDiff(const unsigned char* a, const unsigned char* b, unsigned n, const char* what)
{
    for (unsigned i = 0; i < n; i += 4) {
        if (std::memcmp(a + i, b + i, 4) != 0) {
            std::printf("%s at +0x%x: ours %08x orig %08x\n", what, i, *reinterpret_cast<const unsigned*>(a + i),
                        *reinterpret_cast<const unsigned*>(b + i));
            return;
        }
    }
}

HMODULE ours, orig;

template <class F>
F Proc(HMODULE module, const char* name, const char* side)
{
    auto fn = reinterpret_cast<F>(GetProcAddress(module, name));
    if (!fn) {
        std::printf("missing export %s in %s\n", name, side);
        std::exit(2);
    }
    return fn;
}

// ---------------------------------------------------------------------------------------------------------------
// stand-in texture: slot 3 IsAvailable, 9 GetAlphaHint, 11 GetDimension, 15 Activate, 17 Bind, 18 SetResourcePriority

int texAvailable = 1, texAlpha = 2, texDimension = 0;
int activateCount, bindCount, lastActivate;
float lastPriority;

int __cdecl Noop() { return 0; }
int __cdecl TexAvailable(void*) { return texAvailable; }
int __cdecl TexAlpha(void*) { return texAlpha; }
int __cdecl TexDimension(void*) { return texDimension; }
int __cdecl TexActivate(void*, int how)
{
    ++activateCount;
    lastActivate = how;
    return 0;
}
int __cdecl TexBind(void*, unsigned)
{
    ++bindCount;
    return 0;
}
int __cdecl TexPriority(void*, float priority)
{
    lastPriority = priority;
    return 0;
}

struct FakeTexture {
    void* vtable;
    int refcount;
    unsigned char pad[0x100];
};
void* texVtable[32];

FakeTexture* FakeTex()
{
    static FakeTexture texture;
    static bool ready;
    if (!ready) {
        for (void*& slot : texVtable) slot = reinterpret_cast<void*>(&Noop);
        texVtable[3] = reinterpret_cast<void*>(&TexAvailable);
        texVtable[9] = reinterpret_cast<void*>(&TexAlpha);
        texVtable[11] = reinterpret_cast<void*>(&TexDimension);
        texVtable[15] = reinterpret_cast<void*>(&TexActivate);
        texVtable[17] = reinterpret_cast<void*>(&TexBind);
        texVtable[18] = reinterpret_cast<void*>(&TexPriority);
        texture.vtable = texVtable;
        texture.refcount = 1000;
        ready = true;
    }
    return &texture;
}

// ---------------------------------------------------------------------------------------------------------------
// stand-in Display: the material state members call RenderSystem's exported setters, which touch +0x54 (state) and
// +0x7c (reflection cull); every virtual slot they reach is a no-op.

struct FakeDisplay {
    unsigned char bytes[0x200];
};
unsigned char displayState[kStateSize];
void* displayVtable[0x280 / 4];

FakeDisplay* MakeDisplay()
{
    static FakeDisplay display;
    for (void*& slot : displayVtable) slot = reinterpret_cast<void*>(&Noop);
    std::memset(display.bytes, 0, sizeof display.bytes);
    *reinterpret_cast<void**>(display.bytes) = displayVtable;
    *reinterpret_cast<void**>(display.bytes + 0x54) = displayState;
    return &display;
}
void ResetDisplay(int blendBit)
{
    std::memset(displayState, 0x5a, sizeof displayState);
    if (blendBit) displayState[0xa1] |= 1;
    displayState[0x28] = 0;
    displayState[0x65] = 0;
    displayState[0x75] = 0;
    displayState[0x98] = 0;
    std::memset(displayState + 0x68, 0, 8);
    displayState[0xb8] = 0;
    std::memset(displayState + 0xbc, 0, 4);
}

// ---------------------------------------------------------------------------------------------------------------
// material images

enum Kind { NoTex, OneTex, OneTexNoAlpha, Billboard, ColorOne, AutoTex, MVH, Reflective, Gloss, GenericShader };

const char* VtableName(Kind kind)
{
    switch (kind) {
    case NoTex: return "??_7NoTextureMaterial@Jet@@6BMaterial@1@@";
    case OneTex: return "??_7OneTextureMaterial@Jet@@6BMaterial@1@@";
    case OneTexNoAlpha: return "??_7OneTextureNoAlphaMaterial@Jet@@6BMaterial@1@@";
    case Billboard: return "??_7BillboardMaterial@Jet@@6BMaterial@1@@";
    case ColorOne: return "??_7ColorOneTextureMaterial@Jet@@6BMaterial@1@@";
    case AutoTex: return "??_7AutoTexMaterial@Jet@@6BMaterial@1@@";
    case MVH: return "??_7MVHSelectMaterial@Jet@@6BMaterial@1@@";
    case Reflective: return "??_7ReflectiveMaterial@Jet@@6BMaterial@1@@";
    case Gloss: return "??_7GlossMaterial@Jet@@6BMaterial@1@@";
    default: return "??_7GenericShaderMaterial@Jet@@6BMaterial@1@@";
    }
}

struct Obj {
    unsigned char b[kObjSize];
};
void* classVtable[10];

void InitObject(Obj& o, Kind kind, int variant)
{
    std::memset(o.b, 0xcd, sizeof o.b);
    std::memset(o.b + 0x24, 0, 0x10);  // the PStrings
    std::memset(o.b + 0x34, 0, 0x44);  // DisplayMaterial
    std::memset(o.b + 0x88, 0, 12);  // both ring heads and the texture count
    *reinterpret_cast<int*>(o.b + 0x8c) = 0;
    *reinterpret_cast<void**>(o.b) = classVtable[kind];
    *reinterpret_cast<void**>(o.b + 0x98) = nullptr;
    *reinterpret_cast<void**>(o.b + 0x78) = reinterpret_cast<void*>(4 + (variant & 1));
    *reinterpret_cast<void**>(o.b + 0x7c) = reinterpret_cast<void*>(5 + (variant & 1));
    *reinterpret_cast<float*>(o.b + 0x80) = 1.0f;
    o.b[0x84] = static_cast<unsigned char>(variant & 3);
    *reinterpret_cast<float*>(o.b + 0x94) = 0.5f;
    o.b[0x14] = 0xa1;
    o.b[0x18] = 0xb2;
    o.b[0x1c] = 0xc3;
    switch (kind) {
    case NoTex:
        *reinterpret_cast<void**>(o.b + 0x9c) = reinterpret_cast<void*>(0x1000u);
        break;
    case AutoTex:
    case MVH:
        *reinterpret_cast<void**>(o.b + 0x9c) = FakeTex();
        *reinterpret_cast<int*>(o.b + 0xa0) = 3;
        *reinterpret_cast<void**>(o.b + 0xa4) = reinterpret_cast<void*>(0x2000u);
        for (int i = 0; i < 4; ++i) *reinterpret_cast<float*>(o.b + 0xa8 + i * 4) = float(i + 1) * 0.25f;
        if (kind == MVH) *reinterpret_cast<void**>(o.b + 0xb8) = reinterpret_cast<void*>(0x3000u);
        break;
    case Reflective:
    case Gloss: {
        *reinterpret_cast<void**>(o.b + 0x9c) = FakeTex();
        *reinterpret_cast<int*>(o.b + 0xa0) = 3;
        *reinterpret_cast<void**>(o.b + 0xa4) = reinterpret_cast<void*>(0x2000u);
        *reinterpret_cast<void**>(o.b + 0xa8) = FakeTex();
        *reinterpret_cast<float*>(o.b + 0xac) = 0.75f;
        *reinterpret_cast<int*>(o.b + 0xb0) = 2;
        static int context[2] = {11, 12};
        *reinterpret_cast<void**>(o.b + 0xb4) = context;
        o.b[0xb8] = 3;
        for (int i = 0; i < 16; ++i) *reinterpret_cast<float*>(o.b + 0xbc + i * 4) = float(i);
        break;
    }
    case GenericShader:
        std::memset(o.b + 0xa4, 0, 0x10);
        *reinterpret_cast<int*>(o.b + 0xb0) = 2;
        break;
    default:
        *reinterpret_cast<void**>(o.b + 0x9c) = FakeTex();
        *reinterpret_cast<int*>(o.b + 0xa0) = 3;
        *reinterpret_cast<void**>(o.b + 0xa4) = reinterpret_cast<void*>(0x2000u);
        break;
    }
}

// the two objects always start identical; each method gets its own pair
struct Pair {
    Obj a, b;
    Pair(Kind kind, int variant = 0)
    {
        InitObject(a, kind, variant);
        InitObject(b, kind, variant);
    }
};

void CompareObject(const Pair& p, const char* what)
{
    const bool ok = Same(p.a.b, p.b.b, sizeof p.a.b);
    if (!ok) PrintDiff(p.a.b, p.b.b, sizeof p.a.b, what);
    Check(ok, what);
}
void CompareObjectTail(const Pair& p, unsigned from, const char* what)
{
    const bool ok = Same(p.a.b + from, p.b.b + from, sizeof p.a.b - from);
    if (!ok) PrintDiff(p.a.b + from, p.b.b + from, sizeof p.a.b - from, what);
    Check(ok, what);
}

// ---------------------------------------------------------------------------------------------------------------
// method table

using GetTypeFn = const void* (__cdecl*)(const void*);
using VoidFn = void (__cdecl*)(void*);
using ConstVoidFn = void (__cdecl*)(const void*);
using ThisVoidFn = void (__thiscall*)(const void*);
using PtrFn = void (__cdecl*)(void*, void*);
using GetPtrFn = void* (__cdecl*)(void*);
using IntFn = int (__cdecl*)(void*);
using SetIntFn = void (__cdecl*)(void*, int);
using GetTexFn = void* (__cdecl*)(void*, int);
using GetTexConstFn = const void* (__cdecl*)(const void*, int);
using FloatFn = void (__cdecl*)(void*, float);
using ResultFn = int* (__cdecl*)(const void*, int*);
using DupFn = int* (__cdecl*)(void*, int*, int);
using PassFn = void (__cdecl*)(void*, void*, void*, unsigned);
using StateFn = void (__cdecl*)(const void*, void*);
using EqualsFn = bool (__cdecl*)(const void*, const void*);
using InitFn = void (__cdecl*)(const void*, void*);
using TexGenFn = void (__cdecl*)(void*, float, float, float, float);
using GetTexGenFn = void (__cdecl*)(const void*, float*, float*, float*, float*);
using ColorFn = void (__cdecl*)(void*, const void*);
using GetColorFn = const void* (__cdecl*)(const void*);
using MatrixFn = void (__cdecl*)(void*, const void*);

const char* kGetType[10] = {
    "?GetType@NoTextureMaterial@Jet@@UBAABVPString@2@XZ",
    "?GetType@OneTextureMaterial@Jet@@UBAABVPString@2@XZ",
    "?GetType@OneTextureNoAlphaMaterial@Jet@@UBAABVPString@2@XZ",
    "?GetType@BillboardMaterial@Jet@@UBAABVPString@2@XZ",
    "?GetType@ColorOneTextureMaterial@Jet@@UBAABVPString@2@XZ",
    "?GetType@AutoTexMaterial@Jet@@UBAABVPString@2@XZ",
    "?GetType@MVHSelectMaterial@Jet@@UBAABVPString@2@XZ",
    "?GetType@ReflectiveMaterial@Jet@@UBAABVPString@2@XZ",
    "?GetType@GlossMaterial@Jet@@UBAABVPString@2@XZ",
    "?GetType@GenericShaderMaterial@Jet@@UBAABVPString@2@XZ",
};

void TestGetType()
{
    for (int k = 0; k < 10; ++k) {
        auto f0 = Proc<GetTypeFn>(ours, kGetType[k], "ours");
        auto f1 = Proc<GetTypeFn>(orig, kGetType[k], "orig");
        Obj a, b;
        std::memset(&a, 0, sizeof a);
        std::memset(&b, 0, sizeof b);
        const void* r0 = f0(&a);
        const void* r1 = f1(&b);
        // U5a wave 5: the UNDECORATED_TYPE object is each module's own static; its interned node (and text) is shared.
        const void* n0 = r0 ? *reinterpret_cast<void* const*>(r0) : nullptr;
        const void* n1 = r1 ? *reinterpret_cast<void* const*>(r1) : nullptr;
        const char* t0 = n0 ? static_cast<const char*>(n0) + 0x18 : nullptr;
        const char* t1 = n1 ? static_cast<const char*>(n1) + 0x18 : nullptr;
        const bool ok = t0 && t1 && std::strcmp(t0, t1) == 0;
        Check(ok, kGetType[k]);
    }
}

void TestScalarSetters(int round)
{
    static const char* setPriority[3] = {
        "?SetResourcePriority@OneTextureMaterial@Jet@@UBAXM@Z",
        "?SetResourcePriority@OneTextureNoAlphaMaterial@Jet@@UBAXM@Z",
        "?SetResourcePriority@ReflectiveMaterial@Jet@@UBAXM@Z",
    };
    static const Kind kinds[3] = {OneTex, OneTexNoAlpha, Reflective};
    for (int i = 0; i < 3; ++i) {
        auto f0 = Proc<FloatFn>(ours, setPriority[i], "ours");
        auto f1 = Proc<FloatFn>(orig, setPriority[i], "orig");
        const float value = (round % 7 == 0) ? Awkward() : float(Pick(-1000, 1000)) * 0.125f;
        Pair p(kinds[i]);
        activateCount = bindCount = 0;
        f0(&p.a, value);
        const float seen0 = lastPriority;
        f1(&p.b, value);
        const float seen1 = lastPriority;
        CompareObject(p, setPriority[i]);
        Check(std::memcmp(&seen0, &seen1, 4) == 0, "SetResourcePriority texture priority");
    }

    static const char* setHint[2] = {
        "?SetAlphaHint@OneTextureMaterial@Jet@@QAAXW4TextureAlpha@2@@Z",
        "?SetAlphaHint@OneTextureNoAlphaMaterial@Jet@@QAAXW4TextureAlpha@2@@Z",
    };
    static const Kind setHintKinds[2] = {OneTex, OneTexNoAlpha};
    for (int i = 0; i < 2; ++i) {
        auto f0 = Proc<SetIntFn>(ours, setHint[i], "ours");
        auto f1 = Proc<SetIntFn>(orig, setHint[i], "orig");
        Pair p(setHintKinds[i]);
        const int hint = Pick(-5, 100);
        f0(&p.a, hint);
        f1(&p.b, hint);
        CompareObject(p, setHint[i]);
    }
    {
        auto f0 = Proc<SetIntFn>(ours, "?SetAlphaHint@GenericShaderMaterial@Jet@@QAAXW4TextureAlpha@2@@Z", "ours");
        auto f1 = Proc<SetIntFn>(orig, "?SetAlphaHint@GenericShaderMaterial@Jet@@QAAXW4TextureAlpha@2@@Z", "orig");
        Pair p(GenericShader);
        const int hint = Pick(-5, 100);
        f0(&p.a, hint);
        f1(&p.b, hint);
        CompareObject(p, "GenericShaderMaterial::SetAlphaHint");
    }

    static const char* getHint[3] = {
        "?GetAlphaHint@OneTextureMaterial@Jet@@QBA?AW4TextureAlpha@2@XZ",
        "?GetAlphaHint@OneTextureNoAlphaMaterial@Jet@@QBA?AW4TextureAlpha@2@XZ",
        "?GetAlphaHint@GenericShaderMaterial@Jet@@QAA?AW4TextureAlpha@2@XZ",
    };
    static const Kind hintKinds[3] = {OneTex, OneTexNoAlpha, GenericShader};
    for (int i = 0; i < 3; ++i) {
        auto f0 = Proc<IntFn>(ours, getHint[i], "ours");
        auto f1 = Proc<IntFn>(orig, getHint[i], "orig");
        Pair p(hintKinds[i]);
        const int v = Pick(-10, 50);
        *reinterpret_cast<int*>(p.a.b + (hintKinds[i] == GenericShader ? 0xb0 : 0xa0)) = v;
        *reinterpret_cast<int*>(p.b.b + (hintKinds[i] == GenericShader ? 0xb0 : 0xa0)) = v;
        const int r0 = f0(&p.a);
        const int r1 = f1(&p.b);
        Check(r0 == r1, getHint[i]);
    }

    static const char* setStage[2] = {
        "?SetDefaultTextureStage@OneTextureMaterial@Jet@@QAAXPAX@Z",
        "?SetDefaultTextureStage@OneTextureNoAlphaMaterial@Jet@@QAAXPAX@Z",
    };
    static const Kind stageKinds[2] = {OneTex, OneTexNoAlpha};
    for (int i = 0; i < 2; ++i) {
        auto f0 = Proc<PtrFn>(ours, setStage[i], "ours");
        auto f1 = Proc<PtrFn>(orig, setStage[i], "orig");
        Pair p(stageKinds[i]);
        void* stage = reinterpret_cast<void*>(static_cast<std::intptr_t>(Pick(-100, 100)));
        f0(&p.a, stage);
        f1(&p.b, stage);
        CompareObject(p, setStage[i]);
    }
    static const char* getStage[2] = {
        "?GetDefaultTextureStage@OneTextureMaterial@Jet@@QAAPAXXZ",
        "?GetDefaultTextureStage@OneTextureNoAlphaMaterial@Jet@@QAAPAXXZ",
    };
    static const Kind getStageKinds[2] = {OneTex, OneTexNoAlpha};
    for (int i = 0; i < 2; ++i) {
        auto f0 = Proc<GetPtrFn>(ours, getStage[i], "ours");
        auto f1 = Proc<GetPtrFn>(orig, getStage[i], "orig");
        Pair p(getStageKinds[i]);
        Check(f0(&p.a) == f1(&p.b), getStage[i]);
    }
}

void TestAutoTexParameters(int round)
{
    auto set0 = Proc<TexGenFn>(ours, "?SetTextureGenerationParameters@AutoTexMaterial@Jet@@QAAXMMMM@Z", "ours");
    auto set1 = Proc<TexGenFn>(orig, "?SetTextureGenerationParameters@AutoTexMaterial@Jet@@QAAXMMMM@Z", "orig");
    Pair p(AutoTex);
    float v[4];
    for (float& x : v) x = (round % 5 == 0) ? Awkward() : std::uniform_real_distribution<float>(-500, 500)(rng);
    set0(&p.a, v[0], v[1], v[2], v[3]);
    set1(&p.b, v[0], v[1], v[2], v[3]);
    CompareObject(p, "AutoTexMaterial::SetTextureGenerationParameters");

    auto get0 = Proc<GetTexGenFn>(ours, "?GetTextureGenerationParameters@AutoTexMaterial@Jet@@QBAXAAM000@Z", "ours");
    auto get1 = Proc<GetTexGenFn>(orig, "?GetTextureGenerationParameters@AutoTexMaterial@Jet@@QBAXAAM000@Z", "orig");
    float o0[4], o1[4];
    std::memset(o0, 0, sizeof o0);
    std::memset(o1, 0, sizeof o1);
    get0(&p.a, &o0[0], &o0[1], &o0[2], &o0[3]);
    get1(&p.b, &o1[0], &o1[1], &o1[2], &o1[3]);
    Check(Same(o0, o1, sizeof o0), "AutoTexMaterial::GetTextureGenerationParameters");
}

void TestMVHColor(int round)
{
    auto set0 = Proc<ColorFn>(ours, "?SetStageColor@MVHSelectMaterial@Jet@@QAAXAAVColor@2@@Z", "ours");
    auto set1 = Proc<ColorFn>(orig, "?SetStageColor@MVHSelectMaterial@Jet@@QAAXAAVColor@2@@Z", "orig");
    float color[4];
    for (float& x : color) x = (round % 5 == 0) ? Awkward() : std::uniform_real_distribution<float>(-10, 10)(rng);
    Pair p(MVH);
    set0(&p.a, color);
    set1(&p.b, color);
    CompareObject(p, "MVHSelectMaterial::SetStageColor");

    auto get0 = Proc<GetColorFn>(ours, "?GetStageColor@MVHSelectMaterial@Jet@@QBAABVColor@2@XZ", "ours");
    auto get1 = Proc<GetColorFn>(orig, "?GetStageColor@MVHSelectMaterial@Jet@@QBAABVColor@2@XZ", "orig");
    const void* r0 = get0(&p.a);
    const void* r1 = get1(&p.b);
    Check(r0 == static_cast<void*>(p.a.b + 0xa8) && r1 == static_cast<void*>(p.b.b + 0xa8),
          "MVHSelectMaterial::GetStageColor");
}

void TestCubeMapMatrix(int round)
{
    auto f0 = Proc<MatrixFn>(ours, "?SetCubeMapMatrix@ReflectiveMaterial@Jet@@QAAXABVMatrix4@2@@Z", "ours");
    auto f1 = Proc<MatrixFn>(orig, "?SetCubeMapMatrix@ReflectiveMaterial@Jet@@QAAXABVMatrix4@2@@Z", "orig");
    float matrix[16];
    for (float& x : matrix) x = (round % 5 == 0) ? Awkward() : std::uniform_real_distribution<float>(-100, 100)(rng);
    Pair p(Reflective);
    f0(&p.a, matrix);
    f1(&p.b, matrix);
    CompareObject(p, "ReflectiveMaterial::SetCubeMapMatrix");
}

void TestGetTexture()
{
    struct Entry {
        const char* name;
        Kind kind;
    };
    static const Entry entries[] = {
        {"?GetTexture@NoTextureMaterial@Jet@@UAAPAVTexture@2@H@Z", NoTex},
        {"?GetTexture@OneTextureMaterial@Jet@@UAAPAVTexture@2@H@Z", OneTex},
        {"?GetTexture@OneTextureNoAlphaMaterial@Jet@@UAAPAVTexture@2@H@Z", OneTexNoAlpha},
        {"?GetTexture@ReflectiveMaterial@Jet@@UAAPAVTexture@2@H@Z", Reflective},
        {"?GetTexture@GlossMaterial@Jet@@UAAPAVTexture@2@H@Z", Gloss},
        {"?GetTexture@GenericShaderMaterial@Jet@@UAAPAVTexture@2@H@Z", GenericShader},
    };
    static const Entry constEntries[] = {
        {"?GetTexture@NoTextureMaterial@Jet@@UBAPBVTexture@2@H@Z", NoTex},
        {"?GetTexture@OneTextureMaterial@Jet@@UBAPBVTexture@2@H@Z", OneTex},
        {"?GetTexture@OneTextureNoAlphaMaterial@Jet@@UBAPBVTexture@2@H@Z", OneTexNoAlpha},
        {"?GetTexture@ReflectiveMaterial@Jet@@UBAPBVTexture@2@H@Z", Reflective},
        {"?GetTexture@GlossMaterial@Jet@@UBAPBVTexture@2@H@Z", Gloss},
        {"?GetTexture@GenericShaderMaterial@Jet@@UBAPBVTexture@2@H@Z", GenericShader},
    };
    const int types[] = {-1, 0, 1, 2, 9, 10};
    for (const Entry& e : entries) {
        auto f0 = Proc<GetTexFn>(ours, e.name, "ours");
        auto f1 = Proc<GetTexFn>(orig, e.name, "orig");
        for (int type : types) {
            Pair p(e.kind);
            Check(f0(&p.a, type) == f1(&p.b, type), e.name);
        }
    }
    for (const Entry& e : constEntries) {
        auto f0 = Proc<GetTexConstFn>(ours, e.name, "ours");
        auto f1 = Proc<GetTexConstFn>(orig, e.name, "orig");
        for (int type : types) {
            Pair p(e.kind);
            Check(f0(&p.a, type) == f1(&p.b, type), e.name);
        }
    }
}

void TestIsLoaded()
{
    struct E {
        const char* name;
        Kind kind;
    };
    static const E entries[] = {
        {"?IsLoaded@OneTextureMaterial@Jet@@UBA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@XZ",
         OneTex},
        {"?IsLoaded@OneTextureNoAlphaMaterial@Jet@@UBA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@XZ",
         OneTexNoAlpha},
        {"?IsLoaded@ReflectiveMaterial@Jet@@UBA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@XZ",
         Reflective},
    };
    for (const E& e : entries) {
        auto f0 = Proc<ResultFn>(ours, e.name, "ours");
        auto f1 = Proc<ResultFn>(orig, e.name, "orig");
        for (int available : {0, 1}) {
            texAvailable = available;
            Pair p(e.kind);
            int r0 = -1, r1 = -1;
            f0(&p.a, &r0);
            f1(&p.b, &r1);
            Check(r0 == r1, e.name);
        }
        texAvailable = 1;
    }
}

void TestActivate()
{
    struct E {
        const char* name;
        Kind kind;
    };
    static const E entries[] = {
        {"?Activate@NoTextureMaterial@Jet@@UAAXH@Z", NoTex},
        {"?Activate@OneTextureMaterial@Jet@@UAAXH@Z", OneTex},
        {"?Activate@OneTextureNoAlphaMaterial@Jet@@UAAXH@Z", OneTexNoAlpha},
        {"?Activate@ReflectiveMaterial@Jet@@UAAXH@Z", Reflective},
        {"?Activate@GlossMaterial@Jet@@UAAXH@Z", Gloss},
    };
    for (const E& e : entries) {
        auto f0 = Proc<SetIntFn>(ours, e.name, "ours");
        auto f1 = Proc<SetIntFn>(orig, e.name, "orig");
        for (int detail : {-1, 0, 1, 1000}) {
            Pair p(e.kind);
            activateCount = 0;
            lastActivate = 0;
            f0(&p.a, detail);
            const int c0 = activateCount, d0 = lastActivate;
            activateCount = 0;
            lastActivate = 0;
            f1(&p.b, detail);
            Check(c0 == activateCount && d0 == lastActivate, e.name);
        }
    }
}

void TestCacheAlphaHint()
{
    struct E {
        const char* name;
        Kind kind;
    };
    static const E entries[] = {
        {"?CacheAlphaHint@OneTextureMaterial@Jet@@UBEXXZ", OneTex},
        {"?CacheAlphaHint@OneTextureNoAlphaMaterial@Jet@@UBEXXZ", OneTexNoAlpha},
        {"?CacheAlphaHint@GlossMaterial@Jet@@UBEXXZ", Gloss},
    };
    for (const E& e : entries) {
        auto f0 = Proc<ThisVoidFn>(ours, e.name, "ours");
        auto f1 = Proc<ThisVoidFn>(orig, e.name, "orig");
        for (int alpha : {0, 1, 2, 7}) {
            texAlpha = alpha;
            texAvailable = 1;
            Pair p(e.kind);
            f0(&p.a);
            f1(&p.b);
            CompareObject(p, e.name);
        }
        texAlpha = 2;
    }
}

void TestDuplicateTextureNoTexture()
{
    auto f0 = Proc<DupFn>(ours, "?DuplicateTexture@NoTextureMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@H@Z", "ours");
    auto f1 = Proc<DupFn>(orig, "?DuplicateTexture@NoTextureMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@H@Z", "orig");
    Pair p(NoTex);
    int r0 = -1, r1 = -1;
    f0(&p.a, &r0, 1);
    f1(&p.b, &r1, 1);
    Check(r0 == r1, "NoTextureMaterial::DuplicateTexture");
    CompareObject(p, "NoTextureMaterial::DuplicateTexture object");
}

void TestBindUnbind(int round)
{
    struct E {
        const char* bind;
        const char* unbind;
        Kind kind;
    };
    static const E entries[] = {
        {"?Bind@NoTextureMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
         "?Unbind@NoTextureMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z", NoTex},
        {"?Bind@OneTextureMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
         "?Unbind@OneTextureMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z", OneTex},
        {"?Bind@OneTextureNoAlphaMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
         "?Unbind@OneTextureNoAlphaMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z", OneTexNoAlpha},
        {"?Bind@ColorOneTextureMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
         "?Unbind@ColorOneTextureMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z", ColorOne},
        {"?Bind@AutoTexMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
         "?Unbind@AutoTexMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z", AutoTex},
        {"?Unbind@GlossMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
         "?Unbind@GlossMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z", Gloss},
        {"?Unbind@ReflectiveMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z",
         "?Unbind@ReflectiveMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z", Reflective},
    };
    FakeDisplay* display = MakeDisplay();
    for (const E& e : entries) {
        const bool hasBind = std::strstr(e.bind, "@Bind@") != nullptr;
        if (hasBind) {
            auto f0 = Proc<PassFn>(ours, e.bind, "ours");
            auto f1 = Proc<PassFn>(orig, e.bind, "orig");
            for (unsigned pass : {0u, 1u, 2u}) {
                Pair p(e.kind);
                ResetDisplay(round & 1);
                f0(&p.a, display, nullptr, pass);
                unsigned char stateA[kStateSize];
                std::memcpy(stateA, displayState, kStateSize);
                ResetDisplay(round & 1);
                f1(&p.b, display, nullptr, pass);
                CompareObject(p, e.bind);
                Check(Same(stateA, displayState, kStateSize), "Bind display state");
            }
        }
        auto u0 = Proc<PassFn>(ours, e.unbind, "ours");
        auto u1 = Proc<PassFn>(orig, e.unbind, "orig");
        for (unsigned pass : {0u, 1u}) {
            Pair p(e.kind);
            ResetDisplay(round & 1);
            u0(&p.a, display, nullptr, pass);
            unsigned char stateA[kStateSize];
            std::memcpy(stateA, displayState, kStateSize);
            ResetDisplay(round & 1);
            u1(&p.b, display, nullptr, pass);
            CompareObject(p, e.unbind);
            Check(Same(stateA, displayState, kStateSize), "Unbind display state");
        }
    }
}

void TestDisplayState(int round)
{
    struct E {
        const char* set;
        const char* clear;
        Kind kind;
    };
    static const E entries[] = {
        {"?SetDisplayState@NoTextureMaterial@Jet@@IBAXPAVDisplay@2@@Z",
         "?ClearDisplayState@NoTextureMaterial@Jet@@IBAXPAVDisplay@2@@Z", NoTex},
        {"?SetDisplayState@OneTextureMaterial@Jet@@MBAXPAVDisplay@2@@Z",
         "?ClearDisplayState@OneTextureMaterial@Jet@@MBAXPAVDisplay@2@@Z", OneTex},
        {"?SetDisplayState@OneTextureNoAlphaMaterial@Jet@@MBAXPAVDisplay@2@@Z",
         "?ClearDisplayState@OneTextureNoAlphaMaterial@Jet@@MBAXPAVDisplay@2@@Z", OneTexNoAlpha},
        {"?SetDisplayState@ColorOneTextureMaterial@Jet@@MBAXPAVDisplay@2@@Z",
         "?ClearDisplayState@ColorOneTextureMaterial@Jet@@MBAXPAVDisplay@2@@Z", ColorOne},
        {"?SetDisplayState@MVHSelectMaterial@Jet@@MBAXPAVDisplay@2@@Z",
         "?ClearDisplayState@MVHSelectMaterial@Jet@@MBAXPAVDisplay@2@@Z", MVH},
        {nullptr, "?ClearDisplayState@AutoTexMaterial@Jet@@MBAXPAVDisplay@2@@Z", AutoTex},
        {nullptr, "?ClearDisplayState@ReflectiveMaterial@Jet@@MBAXPAVDisplay@2@@Z", Reflective},
    };
    FakeDisplay* display = MakeDisplay();
    for (const E& e : entries) {
        if (e.set) {
            auto f0 = Proc<StateFn>(ours, e.set, "ours");
            auto f1 = Proc<StateFn>(orig, e.set, "orig");
            for (int variant : {0, 1, 2, 3}) {
                Pair p(e.kind, variant);
                ResetDisplay(round & 1);
                f0(&p.a, display);
                unsigned char stateA[kStateSize];
                std::memcpy(stateA, displayState, kStateSize);
                ResetDisplay(round & 1);
                f1(&p.b, display);
                CompareObject(p, e.set);
                Check(Same(stateA, displayState, kStateSize), "SetDisplayState state");
            }
        }
        auto c0 = Proc<StateFn>(ours, e.clear, "ours");
        auto c1 = Proc<StateFn>(orig, e.clear, "orig");
        for (int variant : {0, 2, 3}) {
            Pair p(e.kind, variant);
            ResetDisplay(round & 1);
            c0(&p.a, display);
            unsigned char stateA[kStateSize];
            std::memcpy(stateA, displayState, kStateSize);
            ResetDisplay(round & 1);
            c1(&p.b, display);
            CompareObject(p, e.clear);
            Check(Same(stateA, displayState, kStateSize), "ClearDisplayState state");
        }
    }
}

void TestEquals(int round)
{
    struct E {
        const char* name;
        Kind kind;
    };
    static const E entries[] = {
        {"?Equals@NoTextureMaterial@Jet@@IBA_NABV12@@Z", NoTex},
        {"?Equals@OneTextureMaterial@Jet@@IBA_NABV12@@Z", OneTex},
        {"?Equals@OneTextureNoAlphaMaterial@Jet@@IBA_NABV12@@Z", OneTexNoAlpha},
        {"?Equals@BillboardMaterial@Jet@@IBA_NABV12@@Z", Billboard},
        {"?Equals@ColorOneTextureMaterial@Jet@@IBA_NABV12@@Z", ColorOne},
        {"?Equals@AutoTexMaterial@Jet@@IBA_NABV12@@Z", AutoTex},
        {"?Equals@MVHSelectMaterial@Jet@@IBA_NABV12@@Z", MVH},
        {"?Equals@ReflectiveMaterial@Jet@@IBA_NABV12@@Z", Reflective},
        {"?Equals@GlossMaterial@Jet@@IBA_NABV12@@Z", Gloss},
    };
    for (const E& e : entries) {
        auto f0 = Proc<EqualsFn>(ours, e.name, "ours");
        auto f1 = Proc<EqualsFn>(orig, e.name, "orig");
        for (int variant = 0; variant < 5; ++variant) {
            Pair p(e.kind, round & 3);
            Obj same, different;
            InitObject(same, e.kind, round & 3);
            InitObject(different, e.kind, round & 3);
            if (e.kind == Reflective || e.kind == Gloss) {
                if (variant & 1) *reinterpret_cast<float*>(different.b + 0xac) =
                    (round % 7 == 0) ? Awkward() : 0.25f;
                if (variant & 2) *reinterpret_cast<void**>(different.b + 0xa8) = nullptr;
            } else if (variant & 1) {
                *reinterpret_cast<int*>(different.b + 0xa0) = 9;
            } else if (variant & 2) {
                *reinterpret_cast<void**>(different.b + 0x9c) = nullptr;
            }
            const bool r0a = f0(&p.a, &same);
            const bool r0b = f0(&p.a, &different);
            const bool r1a = f1(&p.b, &same);
            const bool r1b = f1(&p.b, &different);
            Check(r0a == r1a && r0b == r1b, e.name);
        }
    }
}

void TestInitInstance()
{
    struct E {
        const char* name;
        Kind kind;
    };
    static const E entries[] = {
        {"?InitInstance@OneTextureMaterial@Jet@@IBAXAAV12@@Z", OneTex},
        {"?InitInstance@OneTextureNoAlphaMaterial@Jet@@IBAXAAV12@@Z", OneTexNoAlpha},
        {"?InitInstance@BillboardMaterial@Jet@@IBAXAAV12@@Z", Billboard},
        {"?InitInstance@ColorOneTextureMaterial@Jet@@IBAXAAVOneTextureMaterial@2@@Z", ColorOne},
        {"?InitInstance@AutoTexMaterial@Jet@@IBAXAAV12@@Z", AutoTex},
        {"?InitInstance@MVHSelectMaterial@Jet@@IBAXAAV12@@Z", MVH},
    };
    for (const E& e : entries) {
        auto f0 = Proc<InitFn>(ours, e.name, "ours");
        auto f1 = Proc<InitFn>(orig, e.name, "orig");
        Pair p(e.kind);
        Obj destA, destB;
        std::memset(&destA, 0, sizeof destA);
        std::memset(&destB, 0, sizeof destB);
        *reinterpret_cast<void**>(destA.b) = classVtable[e.kind];
        *reinterpret_cast<void**>(destB.b) = classVtable[e.kind];
        *reinterpret_cast<void**>(destA.b + 0x9c) = nullptr;
        *reinterpret_cast<void**>(destB.b + 0x9c) = nullptr;
        f0(&p.a, &destA);
        f1(&p.b, &destB);
        CompareObjectTail(p, 0x9c, e.name);
        // the class fields the instance received
        const bool ok = Same(destA.b + 0x9c, destB.b + 0x9c, kObjSize - 0x9c);
        if (!ok) PrintDiff(destA.b + 0x9c, destB.b + 0x9c, kObjSize - 0x9c, e.name);
        Check(ok, "InitInstance destination fields");
    }
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: %s <testdir> [rounds]\n", argv[0]);
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 100;
    AddVectoredExceptionHandler(1, VectoredHandler);
    char path[MAX_PATH];
    std::snprintf(path, sizeof path, "%s\\Render3DCore_7c.dll", argv[1]);
    ours = LoadLibraryA(path);
    if (!ours) {
        std::printf("cannot load %s\n", path);
        return 2;
    }
    std::snprintf(path, sizeof path, "%s\\Render3DCore_7c_orig.dll", argv[1]);
    orig = LoadLibraryA(path);
    if (!orig) {
        std::printf("cannot load %s\n", path);
        return 2;
    }
    for (int k = 0; k < 10; ++k) {
        classVtable[k] = GetProcAddress(orig, VtableName(static_cast<Kind>(k)));
        if (!classVtable[k]) {
            std::printf("missing vtable %s\n", VtableName(static_cast<Kind>(k)));
            return 2;
        }
    }
    FakeTex();
    MakeDisplay();

    for (int round = 0; round < rounds; ++round) {
        TestGetType();
        TestScalarSetters(round);
        TestAutoTexParameters(round);
        TestMVHColor(round);
        TestCubeMapMatrix(round);
        TestGetTexture();
        TestIsLoaded();
        TestActivate();
        TestCacheAlphaHint();
        TestDuplicateTextureNoTexture();
        TestBindUnbind(round);
        TestDisplayState(round);
        TestEquals(round);
        TestInitInstance();
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}

