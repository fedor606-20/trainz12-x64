// Differential test of Render2DCore_7c: our module against Render2DCore_7c_orig.dll in one process.
//
// The renderer is a stand-in: a Display whose virtual slots log their calls (draw calls with the full vertex data),
// textures and a texture manager that log too, and a render kernel tying them together. RenderSystem's own
// non-virtual Display members (the render state cache) run for real on the stand-in's state block. Canvas,
// SelectionBox, SpriteManager and SpriteInstance objects of both modules are driven through their exports with the
// same random inputs; the call logs, the Display state and the objects' memory are compared. SpriteResource (not
// exported) is found through its RTTI and loads generated sprite files.
//
// usage: diff_r2d.exe <dir with Render2DCore_7c.dll, Render2DCore_7c_orig.dll and the Jet modules> [cases]
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <utility>
#include <vector>

static HMODULE ours, orig, ks;
static unsigned long long checks, bad;
static std::mt19937 rng(4242);
static int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }
static float FPick(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }

template <class F>
static F Get(HMODULE m, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}

template <class F>
struct Pair {
    F a, b;
    explicit Pair(const char* name) : a(Get<F>(ours, name)), b(Get<F>(orig, name)) {}
    F operator[](int side) const { return side ? b : a; }
};

struct Rect {
    int x, y, w, h;
};
struct Size {
    int w, h;
};
struct Vec2 {
    float x, y;
};
struct Color {
    float r, g, b, a;
};

// ---- the call log

static std::vector<std::string> callLog;
static void Log(const char* fmt, ...)
{
    char line[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    callLog.emplace_back(line);
}

static uint32_t Bits(float f)
{
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
}

static void Check(bool ok, const char* what, int c, const std::vector<std::string>* a = nullptr,
                  const std::vector<std::string>* b = nullptr)
{
    ++checks;
    if (ok) return;
    if (++bad <= 12) {
        std::printf("  mismatch %s case %d\n", what, c);
        if (a && b) {
            size_t i = 0;
            while (i < a->size() && i < b->size() && (*a)[i] == (*b)[i]) ++i;
            std::printf("    first difference at call %zu of %zu/%zu\n    ours: %s\n    orig: %s\n", i, a->size(), b->size(),
                        i < a->size() ? (*a)[i].c_str() : "-", i < b->size() ? (*b)[i].c_str() : "-");
        }
    }
}

// ---- KernelStandard bits

static void* pstringCache;
using CreateFn = void*(__cdecl*)(void*, const unsigned char*);
static CreateFn createNode;
struct PString {
    void* node;
};
static PString MakePString(const char* s) { return PString{createNode(pstringCache, reinterpret_cast<const unsigned char*>(s))}; }
static const char* Text(const void* pstring)
{
    const char* node = *static_cast<char* const*>(pstring);
    return node ? node + 0x18 : "(null)";
}
using NewFn = void*(__cdecl*)(unsigned);
static NewFn jetNew;

struct BitmapMem {
    int width, height;
    unsigned char unknown08[4];
    unsigned char* data;
    int pitch;
    int bpp;
};

// ---- stand-in textures and texture manager

static int textureIds;
struct FakeTex {
    void** vt;
    int id;
};
static void* texVt[24];
static FakeTex* NewTexture()
{
    auto* t = new FakeTex{texVt, ++textureIds};
    Log("new texture %d", t->id);
    return t;
}
template <int N>
static void __cdecl TexStub(FakeTex* t)
{
    Log("tex %d slot %x", t->id, N * 4);
}
static void* __fastcall TexDelete(FakeTex* t, void*, unsigned flags)
{
    Log("tex %d delete %u", t->id, flags);
    return t;
}
static void __cdecl TexCreate(FakeTex* t, BitmapMem* b, void*, int i0, int i1, int i2, int i3, float f, int i4, int i5,
                              int i6, int i7, int i8, int i9, void*)
{
    Log("tex %d create %dx%d bpp %d %d %d %d %d %08x %d %d %d %d %d %d", t->id, b->width, b->height, b->bpp, i0, i1, i2, i3,
        Bits(f), i4, i5, i6, i7, i8, i9);
}
static void __cdecl TexUpload(FakeTex* t, int stage, void*, BitmapMem* b, int x)
{
    Log("tex %d upload %d %dx%d %d", t->id, stage, b->width, b->height, x);
}
static const unsigned char* pixelBase;
static void __cdecl TexUpdate(FakeTex* t, int stage, const Rect* r, void*, BitmapMem* b, int x)
{
    Log("tex %d update %d rect %d,%d,%d,%d bitmap %dx%d pitch %d at %td %d", t->id, stage, r->x, r->y, r->w, r->h, b->width,
        b->height, b->pitch, b->data - pixelBase, x);
}
static void __cdecl TexBind(FakeTex* t, int stage) { Log("tex %d bind %d", t->id, stage); }
static Size texSize;
static Size* __cdecl TexGetSize(FakeTex* t, Size* out)
{
    Log("tex %d size", t->id);
    *out = texSize;
    return out;
}
static void __cdecl TexTouch(FakeTex* t, int x) { Log("tex %d touch %d", t->id, x); }
static void __cdecl TexSelect(FakeTex* t, int stage) { Log("tex %d select %d", t->id, stage); }

static bool managerFails;
static void* __cdecl MgrGet(void*, const void* db, const void* name, const void* key)
{
    Log("manager get %s db %p key %p", Text(name), db, key != nullptr ? (void*)1 : nullptr);
    return managerFails ? nullptr : NewTexture();
}
static void* mgrVt[8];
static void* manager[4] = {mgrVt};

// ---- the stand-in display and kernel

static unsigned char displayMem[0x400];
static unsigned char displayState[0x400];
static void* displayVt[0x100];
static float texelOffset;

template <int N>
static void __cdecl DisplayStub(void*)
{
    Log("display slot %x", N * 4);
}
static float __cdecl DisplayTexel(void*)
{
    Log("display texel offset");
    return texelOffset;
}
static void* __cdecl DisplayCreateTexture(void*) { return NewTexture(); }
static void __cdecl DisplaySetTexture(void*, int stage) { Log("display set texture %d", stage); }
static void LogVertices(const char* what, int a, int prim, int kind, const uint32_t* v, int count)
{
    std::string s = what;
    char buf[64];
    std::snprintf(buf, sizeof buf, " %x %d %d %d:", a, prim, kind, count);
    s += buf;
    for (int i = 0; i < count * 6; ++i) {
        std::snprintf(buf, sizeof buf, " %08x", v[i]);
        s += buf;
    }
    Log("%s", s.c_str());
}
static void __cdecl DisplayDrawUser(void*, int fmt, int prim, int kind, const uint32_t* v, int count)
{
    LogVertices("draw user", fmt, prim, kind, v, count);
}
static void __cdecl DisplayDraw(void*, int decl, int prim, int kind, const uint32_t* v, int count)
{
    LogVertices("draw", decl, prim, kind, v, count);
}
static int __cdecl DisplayGetDeclaration(void*, int fmt)
{
    Log("display declaration %x", fmt);
    return 0x1000 + fmt;
}

template <int... I>
static void FillStubs(std::integer_sequence<int, I...>, void** table, bool display)
{
    ((table[I] = display ? reinterpret_cast<void*>(&DisplayStub<I>) : reinterpret_cast<void*>(&TexStub<I>)), ...);
}

static unsigned char kernelMem[0x100];
static unsigned char groupMem[0x100];
static unsigned char stateStart[0x400];

static void ResetRenderer()
{
    std::memcpy(displayState, stateStart, sizeof displayState);
    textureIds = 0;
    callLog.clear();
}

static bool SameState() { return true; }

// runs fn for our module (side 0) and the original (side 1) from the same renderer state; compares logs and state
template <class Fn>
static void Both(const char* what, int c, Fn fn)
{
    std::vector<std::string> logs[2];
    unsigned char states[2][0x400];
    const int ids = textureIds;
    unsigned char before[0x400];
    std::memcpy(before, displayState, sizeof before);
    static const bool trace = std::getenv("DIFF_TRACE") != nullptr;
    for (int side = 0; side < 2; ++side) {
        if (trace) std::printf("stage %s case %d side %d\n", what, c, side);
        std::memcpy(displayState, before, sizeof before);
        textureIds = ids;
        callLog.clear();
        fn(side);
        logs[side] = std::move(callLog);
        std::memcpy(states[side], displayState, sizeof displayState);
    }
    callLog.clear();
    Check(logs[0] == logs[1], what, c, &logs[0], &logs[1]);
    Check(std::memcmp(states[0], states[1], sizeof states[0]) == 0, "display state", c);
}

// ---- RectList

struct Node {
    Rect r;
    void* link;
};
struct RectList {
    const Node* head;
};

static void MakeList(std::vector<Node>& nodes, RectList& list, int n)
{
    nodes.resize(n);
    for (int i = 0; i < n; ++i) {
        nodes[i].r = Rect{Pick(-60, 1100), Pick(-60, 800), Pick(-5, 400), Pick(-5, 300)};
        nodes[i].link = i + 1 < n ? static_cast<void*>(&nodes[i + 1].link) : nullptr;
    }
    list.head = n ? &nodes[0] : nullptr;
}

// ---- Canvas

using CanvasCtor = void*(__fastcall*)(void*, void*, void* group, const PString* name, int priority, bool flag);
using CanvasCreate = void(__cdecl*)(void*, const Size*, const Rect*);
using CanvasUpdate = void(__cdecl*)(void*, const RectList*, const RectList*, unsigned long*, int);
using Void0 = void(__cdecl*)(void*);
using VoidII = void(__cdecl*)(void*, int, int);
using VoidF = void(__cdecl*)(void*, float);
using VoidFFF = void(__cdecl*)(void*, float, float, float);

static const int kCanvasSize = 0x2494;

static void TestCanvas(int cases)
{
    const Pair<CanvasCtor> ctor("??0Canvas@Jet@@QAE@PAVLayerGroup@1@ABVPString@1@H_N@Z");
    const Pair<CanvasCreate> create("?Create@Canvas@Jet@@QAAXABUSize@2@ABURect@2@@Z");
    const Pair<CanvasUpdate> update("?Update@Canvas@Jet@@QAAXABVRectList@2@0PAKH@Z");
    const Pair<Void0> render("?Render@Canvas@Jet@@UAAXXZ");
    const Pair<Void0> destroy("?Destroy@Canvas@Jet@@QAAXXZ");
    const Pair<VoidII> setPosition("?SetPosition@Canvas@Jet@@QAAXHH@Z");
    const Pair<VoidFFF> setColor("?SetColor@Canvas@Jet@@QAAXMMM@Z");
    const Pair<VoidF> setOpacity("?SetOpacity@Canvas@Jet@@QAAXM@Z");
    const Pair<VoidF> setScale("?SetScale@Canvas@Jet@@QAAXM@Z");
    const PString name = MakePString("canvas");
    std::vector<unsigned long> pixels(1200 * 900);
    pixelBase = reinterpret_cast<const unsigned char*>(pixels.data());
    for (int c = 0; c < cases; ++c) {
        static unsigned char obj[2][kCanvasSize];
        for (int side = 0; side < 2; ++side) {
            std::memset(obj[side], 0xcd, kCanvasSize);
            ctor[side](obj[side], nullptr, groupMem, &name, 5, true);
            *reinterpret_cast<void**>(obj[side] + 0x18) = kernelMem;
        }
        Check(std::memcmp(obj[0] + 0x3c, obj[1] + 0x3c, kCanvasSize - 0x3c) == 0, "Canvas ctor", c);
        const Size tile = {Pick(0, 4) ? Pick(16, 300) : Pick(-5, 0), Pick(0, 4) ? Pick(16, 300) : Pick(-5, 0)};
        const int tw = tile.w > 0 ? tile.w : 256, th = tile.h > 0 ? tile.h : 256;
        const Rect area = {Pick(-20, 40), Pick(-20, 40), Pick(1, (std::min)(7 * tw, 1100)), Pick(1, (std::min)(5 * th, 800))};
        Both("Canvas::Create", c, [&](int s) { create[s](obj[s], &tile, &area); });
        const int px = Pick(-50, 50), py = Pick(-50, 50);
        const float r = FPick(0, 1), g = FPick(0, 1), b = FPick(0, 1), a = FPick(0, 1), sc = Pick(0, 2) ? 1.0f : FPick(0.2f, 3);
        Both("Canvas setters", c, [&](int s) {
            setPosition[s](obj[s], px, py);
            setColor[s](obj[s], r, g, b);
            setOpacity[s](obj[s], a);
            setScale[s](obj[s], sc);
        });
        std::vector<Node> dn, an;
        RectList dirty, active;
        MakeList(dn, dirty, Pick(0, 40));
        MakeList(an, active, Pick(0, 40));
        const int pitch = 1200;
        Both("Canvas::Update", c, [&](int s) { update[s](obj[s], &dirty, &active, pixels.data(), pitch); });
        Check(std::memcmp(obj[0] + 0xc8, obj[1] + 0xc8, kCanvasSize - 0xc8) == 0, "Canvas after Update", c);
        texelOffset = Pick(0, 2) == 0 ? 0.0f : (Pick(0, 1) ? 0.5f : FPick(-1, 1));
        displayState[0xa0] = static_cast<unsigned char>(Pick(0, 1));  // wire frame on or off
        Both("Canvas::Render", c, [&](int s) { render[s](obj[s]); });
        Both("Canvas::Destroy", c, [&](int s) { destroy[s](obj[s]); });
        Check(std::memcmp(obj[0] + 0x3c, obj[1] + 0x3c, kCanvasSize - 0x3c) == 0, "Canvas after Destroy", c);
    }
}

// ---- SelectionBox

using SelCtor = void*(__fastcall*)(void*, void*, void* group, const PString* name, int priority, bool flag);
using SelRect = void(__fastcall*)(void*, void*, const Rect*);
using SelColor = void(__fastcall*)(void*, void*, const Color*);

static void TestSelectionBox(int cases)
{
    const Pair<SelCtor> ctor("??0SelectionBox@Jet@@QAE@PAVLayerGroup@1@ABVPString@1@H_N@Z");
    const Pair<SelRect> setRect("?SetRect@SelectionBox@Jet@@QAEXABURect@2@@Z");
    const Pair<SelColor> setArea("?SetAreaColor@SelectionBox@Jet@@QAEXABVColor@2@@Z");
    const Pair<SelColor> setDark("?SetDarkColor@SelectionBox@Jet@@QAEXABVColor@2@@Z");
    const Pair<SelColor> setLight("?SetLightColor@SelectionBox@Jet@@QAEXABVColor@2@@Z");
    const Pair<Void0> render("?Render@SelectionBox@Jet@@MAAXXZ");
    const PString name = MakePString("box");
    for (int c = 0; c < cases; ++c) {
        static unsigned char obj[2][0x7c];
        for (int s = 0; s < 2; ++s) {
            std::memset(obj[s], 0xcd, sizeof obj[s]);
            ctor[s](obj[s], nullptr, groupMem, &name, 3, false);
            *reinterpret_cast<void**>(obj[s] + 0x18) = kernelMem;
        }
        Check(std::memcmp(obj[0] + 0x3c, obj[1] + 0x3c, 0x40) == 0, "SelectionBox ctor", c);
        const Rect r = {Pick(-100, 900), Pick(-100, 700), Pick(-10, 500), Pick(-10, 400)};
        const Color ca = {FPick(0, 1), FPick(0, 1), FPick(0, 1), FPick(0, 1)};
        const Color cd = {FPick(0, 1), FPick(0, 1), FPick(0, 1), FPick(0, 1)};
        const Color cl = {FPick(0, 1), FPick(0, 1), FPick(0, 1), FPick(0, 1)};
        const bool colours = Pick(0, 1) != 0;
        Both("SelectionBox::Render", c, [&](int s) {
            setRect[s](obj[s], nullptr, &r);
            if (colours) {
                setArea[s](obj[s], nullptr, &ca);
                setDark[s](obj[s], nullptr, &cd);
                setLight[s](obj[s], nullptr, &cl);
            }
            render[s](obj[s]);
        });
    }
}

// ---- sprites

struct FakeRes {  // a SpriteResource laid out as the original's (0xc8)
    void** vt;
    unsigned char pad04[0x18];
    int refs;  // +0x1c
    unsigned char pad20[0x3c];
    FakeTex* texture;            // +0x5c
    unsigned char attr[0x50];    // +0x60
    void* textureName;           // +0xb0
    Rect uvi;                    // +0xb4
    int declaration;             // +0xc4
};
static_assert(sizeof(FakeRes) == 0xc8);
static int lockCode;
static int* __cdecl ResLock(FakeRes*, int* r)
{
    Log("resource lock");
    *r = lockCode;
    return r;
}
static void __cdecl ResUnlock(FakeRes*) { Log("resource unlock"); }
static void* resVt[32];

static FakeRes* resourceToReturn;
static const void* __cdecl DbGet(void*, const PString* name, int flags)
{
    Log("db get %s %d", Text(name), flags);
    return resourceToReturn;
}
static void* dbVt[8];
static void* fakeDb[4] = {dbVt};

static void RandomAttributes(unsigned char* a)
{
    float* f = reinterpret_cast<float*>(a);
    f[0] = FPick(-2, 2), f[1] = FPick(-2, 2), f[2] = FPick(-5, 5);  // position, priority
    f[3] = FPick(0, 2), f[4] = FPick(0, 2);                          // size
    for (int i = 5; i < 9; ++i) f[i] = FPick(0, 1);                  // uv
    for (int i = 9; i < 13; ++i) f[i] = FPick(0, 1);                 // colour
    f[13] = Pick(0, 2) ? 0.0f : FPick(-7, 7);                        // rotation
    f[14] = FPick(0.1f, 3);                                          // scale
    for (int i = 0; i < 4; ++i) a[0x3c + i] = static_cast<unsigned char>(Pick(0, 3) == 0);
    int* n = reinterpret_cast<int*>(a);
    n[0x40 / 4] = Pick(1, 8), f[0x44 / 4] = FPick(0, 1), n[0x48 / 4] = Pick(1, 13), n[0x4c / 4] = Pick(1, 13);
}

using SpriteCtor = void*(__fastcall*)(void*, void*);
using SpriteOpen = bool(__cdecl*)(void*, void* kernel, const PString*, const void* db);
using SpriteUpdateAttr = void(__cdecl*)(void*, void* kernel);
using SpriteRender = void(__cdecl*)(void*, void* kernel, void** current);
using MgrCtor = void*(__fastcall*)(void*, void*, void* group, const PString* name, int priority, bool flag, bool adjust);
using MgrAdd = void(__cdecl*)(void*, void*);

static void TestSprites(int cases)
{
    const Pair<SpriteCtor> ctor("??0SpriteInstance@Jet@@QAE@XZ");
    const Pair<SpriteOpen> open("?Open@SpriteInstance@Jet@@QAA_NPAVRenderKernel@2@ABVPString@2@PBVResourceDatabase@2@@Z");
    const Pair<SpriteUpdateAttr> updateAttr("?UpdateAttributes@SpriteInstance@Jet@@MAAXPAVRenderKernel@2@@Z");
    const Pair<SpriteRender> render("?Render@SpriteInstance@Jet@@IAAXPAVRenderKernel@2@AAPAVTexture@2@@Z");
    const Pair<VoidF> setPriority("?SetPriority@SpriteInstance@Jet@@QAAXM@Z");
    const Pair<VoidF> setRotation("?SetRotation@SpriteInstance@Jet@@QAAXM@Z");
    const Pair<VoidF> setScale("?SetScale@SpriteInstance@Jet@@QAAXM@Z");
    const Pair<VoidF> setOpacity("?SetOpacity@SpriteInstance@Jet@@QAAXM@Z");
    const Pair<VoidFFF> setColor("?SetColor@SpriteInstance@Jet@@QAAXMMM@Z");
    using SetPos = void(__cdecl*)(void*, const Vec2*, int);
    const Pair<SetPos> setPosition("?SetPosition@SpriteInstance@Jet@@QAAXABVVector2@2@W4PointPosition@2@@Z");
    using SetSz = void(__fastcall*)(void*, void*, const Vec2*);
    const Pair<SetSz> setSize("?SetSize@SpriteInstance@Jet@@QAEXABVVector2@2@@Z");
    using SetScr = void(__fastcall*)(void*, void*, const Size*);
    const Pair<SetScr> setScreenSize("?SetScreenSize@SpriteInstance@Jet@@QAEXABUSize@2@@Z");
    const Pair<VoidII> enableFixed("?EnableFixedPixelSize@SpriteInstance@Jet@@QAAXHH@Z");
    using SetUV = void(__cdecl*)(void*, const Vec2*, const Vec2*);
    const Pair<SetUV> setUV("?SetUV@SpriteInstance@Jet@@QAAXABVVector2@2@0@Z");
    using SetB = void(__cdecl*)(void*, bool);
    const Pair<SetB> setVisible("?SetVisible@SpriteInstance@Jet@@QAAX_N@Z");
    const Pair<SetB> setTest("?SetSpriteAlphaTestMode@SpriteInstance@Jet@@QAAX_N@Z");
    const Pair<SetB> setBlend("?SetSpriteAlphaBlendMode@SpriteInstance@Jet@@QAAX_N@Z");
    using SetFn = void(__cdecl*)(void*, int, float);
    const Pair<SetFn> setTestFn("?SetSpriteAlphaTestFunction@SpriteInstance@Jet@@QAAXW4DFunction@2@M@Z");
    using SetFac = void(__cdecl*)(void*, int, int);
    const Pair<SetFac> setFactors("?SetSpriteAlphaBlendFactor@SpriteInstance@Jet@@QAAXW4Factor@Src@2@W43Dst@2@@Z");
    const Pair<MgrCtor> mgrCtor("??0SpriteManager@Jet@@QAE@PAVLayerGroup@1@ABVPString@1@H_N2@Z");
    const Pair<MgrAdd> mgrAdd("?Add@SpriteManager@Jet@@QAAXPAVSpriteInstance@2@@Z");
    const Pair<MgrAdd> mgrRemove("?Remove@SpriteManager@Jet@@QAAXPAVSpriteInstance@2@@Z");
    const Pair<Void0> mgrRender("?Render@SpriteManager@Jet@@UAAXXZ");
    const Pair<VoidF> mgrUpdate("?Update@SpriteManager@Jet@@UAAXM@Z");
    const Pair<Void0> mgrPurge("?Purge@SpriteManager@Jet@@QAAXXZ");
    using SetAsp = void(__cdecl*)(void*, bool);
    const Pair<SetAsp> mgrAspect("?SetAdjustAspectRatio@SpriteManager@Jet@@QAAX_N@Z");
    const PString name = MakePString("ui/test.sprite.txt");
    const PString mgrName = MakePString("sprites");

    for (int c = 0; c < cases; ++c) {
        // a manager with a few sprites on each side, all built from identical memory
        unsigned char mgr[2][0x4c];
        const bool adjust = Pick(0, 1) != 0;
        for (int s = 0; s < 2; ++s) {
            std::memset(mgr[s], 0xcd, sizeof mgr[s]);
            mgrCtor[s](mgr[s], nullptr, groupMem, &mgrName, 7, true, adjust);
            *reinterpret_cast<void**>(mgr[s] + 0x18) = kernelMem;
        }
        Check(std::memcmp(mgr[0] + 0x3c, mgr[1] + 0x3c, 0x10) == 0 && mgr[0][0x20] == mgr[1][0x20], "SpriteManager ctor", c);
        const int n = Pick(1, 6);
        std::vector<unsigned char*> sprite[2];
        std::vector<FakeRes*> res[2];
        unsigned char garbage[0xd0];
        for (auto& g : garbage) g = static_cast<unsigned char>(rng());
        for (int i = 0; i < n; ++i) {
            FakeRes proto = {};
            proto.vt = resVt;
            proto.refs = 1000;
            proto.texture = nullptr;
            RandomAttributes(proto.attr);
            proto.uvi = Pick(0, 2) ? Rect{-1, -1, -1, -1} : Rect{Pick(-1, 40), Pick(-1, 40), Pick(1, 80), Pick(1, 80)};
            proto.declaration = Pick(0, 1) ? 0 : 0x1013;
            texSize = Size{Pick(1, 128), Pick(1, 128)};
            managerFails = Pick(0, 9) == 0;
            lockCode = Pick(0, 9) == 0 ? 4 : 0;
            const bool noResource = Pick(0, 15) == 0;
            for (int s = 0; s < 2; ++s) {
                auto* r = new FakeRes(proto);
                res[s].push_back(r);
                auto* sp = static_cast<unsigned char*>(jetNew(0xd0));
                std::memcpy(sp, garbage, sizeof garbage);
                ctor[s](sp, nullptr);
                sprite[s].push_back(sp);
            }
            bool ok[2];
            Both("SpriteInstance::Open", c, [&](int s) {
                resourceToReturn = noResource ? nullptr : res[s][i];
                ok[s] = open[s](sprite[s][i], kernelMem, &name, fakeDb);
            });
            Check(ok[0] == ok[1], "Open result", c);
            auto sameSprite = [&](const char* what) {
                // everything but the vtable and the resource pointer
                const unsigned char* a = sprite[0][i];
                const unsigned char* b = sprite[1][i];
                // the manager pointer (+0xbc) differs too: each side has its own manager
                const bool same = std::memcmp(a + 4, b + 4, 0x60) == 0 && std::memcmp(a + 0x68, b + 0x68, 0xbc - 0x68) == 0 &&
                                  std::memcmp(a + 0xc0, b + 0xc0, 0xd0 - 0xc0) == 0;
                if (!same && bad < 6)
                    for (int o = 4; o < 0xd0; o += 4)
                        if ((o < 0x64 || o >= 0x68) && o != 0xbc && std::memcmp(a + o, b + o, 4) != 0) {
                            std::printf("    sprite +%x ours %08x orig %08x\n", o, *reinterpret_cast<const uint32_t*>(a + o),
                                        *reinterpret_cast<const uint32_t*>(b + o));
                            break;
                        }
                Check(same, what, c);
                const FakeRes* ra = res[0][i];
                const FakeRes* rb = res[1][i];
                Check(std::memcmp(ra->attr, rb->attr, 0x50) == 0 && std::memcmp(&ra->uvi, &rb->uvi, 0x14) == 0 &&
                          (ra->texture ? ra->texture->id : 0) == (rb->texture ? rb->texture->id : 0),
                      "resource after Prepare", c);
            };
            sameSprite("sprite after Open");
            if (noResource || !ok[0] || !res[0][i]->texture) {
                // keep the sprite usable for drawing
                for (int s = 0; s < 2; ++s) {
                    *reinterpret_cast<FakeRes**>(sprite[s][i] + 0x64) = res[s][i];
                    if (!res[s][i]->texture) res[s][i]->texture = new FakeTex{texVt, 900 + i};
                }
            }
            Both("Add", c, [&](int s) { mgrAdd[s](mgr[s], sprite[s][i]); });
            // random edits
            for (int e = Pick(0, 8); e > 0; --e) {
                const int k = Pick(0, 11);
                const float f1 = FPick(-3, 3), f2 = FPick(-3, 3), f3 = FPick(0, 1);
                const Vec2 v1 = {FPick(-2, 2), FPick(-2, 2)}, v2 = {FPick(0, 1), FPick(0, 1)};
                const Size scr = {Pick(1, 2000), Pick(1, 1500)};
                const int i1 = Pick(-5, 300), i2 = Pick(-5, 300), anchor = Pick(0, 2);
                const bool b1 = Pick(0, 1) != 0;
                const bool zeroRotation = Pick(0, 1) != 0, visible = Pick(0, 5) != 0;
                static char label[32];
                std::snprintf(label, sizeof label, "sprite edit %d", k);
                Both(label, c, [&](int s) {
                    void* p = sprite[s][i];
                    switch (k) {
                    case 0: setPriority[s](p, f1); break;
                    case 1: setRotation[s](p, zeroRotation ? 0.0f : f1); break;
                    case 2: setScale[s](p, f3 + 0.1f); break;
                    case 3: setColor[s](p, f3, f3 * 0.5f, 1 - f3); setOpacity[s](p, f3); break;
                    case 4: setPosition[s](p, &v1, anchor); break;
                    case 5: setSize[s](p, nullptr, &v2); break;
                    case 6: setScreenSize[s](p, nullptr, &scr); break;
                    case 7: enableFixed[s](p, i1, i2); break;
                    case 8: setUV[s](p, &v1, &v2); break;
                    case 9: setTest[s](p, b1); setTestFn[s](p, i1 & 7, f3); break;
                    case 10: setBlend[s](p, b1); setFactors[s](p, i1 % 13, i2 % 13); break;
                    default: setVisible[s](p, visible); break;
                    }
                });
                std::snprintf(label, sizeof label, "sprite after edit %d", k);
                sameSprite(label);
            }
        }
        // direct attribute updates and draws
        for (int i = 0; i < n; ++i) {
            Both("UpdateAttributes", c, [&](int s) { updateAttr[s](sprite[s][i], kernelMem); });
            Check(std::memcmp(sprite[0][i] + 4, sprite[1][i] + 4, 0x60) == 0, "vertices", c);
            displayState[0xa0] = static_cast<unsigned char>(rng());
            *reinterpret_cast<int*>(displayState + 0x68) = Pick(1, 13);
            *reinterpret_cast<int*>(displayState + 0x6c) = Pick(1, 13);
            *reinterpret_cast<float*>(displayState + 0xcc) = FPick(0, 1);
            *reinterpret_cast<int*>(displayState + 0xd0) = Pick(1, 8);
            void* cur[2] = {nullptr, nullptr};
            const bool sameTexture = Pick(0, 1) != 0;
            Both("SpriteInstance::Render", c, [&](int s) {
                cur[s] = sameTexture ? res[s][i]->texture : nullptr;
                render[s](sprite[s][i], kernelMem, &cur[s]);
            });
        }
        // the manager: sort, draw, update, remove some, purge
        const float dt = FPick(0, 0.1f);
        const bool aspect = Pick(0, 1) != 0;
        Both("SpriteManager::Render", c, [&](int s) {
            mgrAspect[s](mgr[s], aspect);
            mgrRender[s](mgr[s]);
            mgrUpdate[s](mgr[s], dt);
        });
        const int victim = Pick(0, n - 1);
        Both("SpriteManager::Remove", c, [&](int s) {
            mgrRemove[s](mgr[s], sprite[s][victim]);
            mgrRender[s](mgr[s]);
        });
        Both("SpriteManager::Purge", c, [&](int s) { mgrPurge[s](mgr[s]); });
    }
}

// ---- SpriteResource (not exported): found through its RTTI

static void** FindVtable(HMODULE m, const char* rttiName)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const size_t size = nt->OptionalHeader.SizeOfImage;
    const size_t len = std::strlen(rttiName);
    for (size_t i = 0; i + len < size; ++i) {
        if (std::memcmp(base + i, rttiName, len + 1) != 0) continue;
        const auto td = reinterpret_cast<uintptr_t>(base + i - 8);  // TypeDescriptor {vftable, spare, name}
        for (size_t j = 0; j + 20 <= size; j += 4) {
            const auto* col = reinterpret_cast<const uint32_t*>(base + j);
            if (col[0] == 0 && col[1] == 0 && col[3] == td) {  // CompleteObjectLocator
                const auto colAddr = reinterpret_cast<uintptr_t>(col);
                for (size_t k = 0; k + 4 <= size; k += 4)
                    if (*reinterpret_cast<const uint32_t*>(base + k) == colAddr)
                        return reinterpret_cast<void**>(base + k + 4);
            }
        }
    }
    return nullptr;
}

static std::string spritePath;
static PString* __cdecl DbGetFileName(void*, PString* r, const PString*)
{
    *r = MakePString(spritePath.c_str());
    return r;
}

static void WriteSpriteFile(const char* path)
{
    static const char* textures[] = {"tex.tga", "sub/tex.tga", "/abs/tex.tga", "\\abs\\tex.tga", ""};
    static const char* functions[] = {"always", "never", "less", "lessequal", "greater", "notequal", "bogus"};
    static const char* factors[] = {"one", "zero", "srcalpha", "invsrcalpha", "srccolor", "destalpha", "bogus"};
    FILE* f = std::fopen(path, "wb");
    if (Pick(0, 15) == 0) {
        std::fprintf(f, "Other\n{\n  Texture \"x.tga\"\n}\n");
        std::fclose(f);
        return;
    }
    std::fprintf(f, "Sprite\n{\n");
    if (Pick(0, 9)) std::fprintf(f, "  Texture \"%s\"\n", textures[Pick(0, 4)]);
    if (Pick(0, 1)) std::fprintf(f, "  UV %.3f,%.3f,%.3f,%.3f\n", FPick(0, 1), FPick(0, 1), FPick(0, 1), FPick(0, 1));
    if (Pick(0, 1)) std::fprintf(f, "  UVI %d,%d,%d,%d\n", Pick(-1, 30), Pick(-1, 30), Pick(1, 60), Pick(1, 60));
    if (Pick(0, 1)) std::fprintf(f, "  Color %.3f,%.3f,%.3f,%.3f\n", FPick(0, 1), FPick(0, 1), FPick(0, 1), FPick(0, 1));
    if (Pick(0, 1)) std::fprintf(f, "  Size %.3f,%.3f\n", FPick(0, 2), FPick(0, 2));
    if (Pick(0, 1)) std::fprintf(f, "  Position %.3f,%.3f,%.3f\n", FPick(-1, 1), FPick(-1, 1), FPick(-1, 1));
    if (Pick(0, 1)) std::fprintf(f, "  Rotation %.3f\n", FPick(-3, 3));
    if (Pick(0, 1)) std::fprintf(f, "  Scale %.3f\n", FPick(0, 3));
    if (Pick(0, 1)) std::fprintf(f, "  AlphaBlendMode %d\n", Pick(0, 1));
    if (Pick(0, 1)) std::fprintf(f, "  AlphaTestMode %d\n", Pick(0, 1));
    if (Pick(0, 1)) std::fprintf(f, "  AlphaTestFunction \"%s\"\n", functions[Pick(0, 6)]);
    if (Pick(0, 1)) std::fprintf(f, "  AlphaRef %.3f\n", FPick(0, 1));
    if (Pick(0, 1)) std::fprintf(f, "  SrcFactor \"%s\"\n", factors[Pick(0, 6)]);
    if (Pick(0, 1)) std::fprintf(f, "  DstFactor \"%s\"\n", factors[Pick(0, 6)]);
    std::fprintf(f, "}\n");
    std::fclose(f);
}

static void TestSpriteResource(int cases)
{
    void** vt[2] = {FindVtable(ours, ".?AVSpriteResource@Jet@@"), FindVtable(orig, ".?AVSpriteResource@Jet@@")};
    if (!vt[0] || !vt[1]) {
        std::printf("SpriteResource vtable not found\n");
        ++bad;
        return;
    }
    // the KernelResource both Render2DCore modules import (ours no longer forwards to the original)
    HMODULE kr = GetModuleHandleA("KernelResource_7c.dll");
    static unsigned char fakeSystem[0x400];
    auto** rs = reinterpret_cast<void**>(GetProcAddress(kr, "?resourceSystem@Resource@Jet@@0PAVResourceSystem@2@A"));
    if (!*rs) *rs = fakeSystem;
    using ResCtor = void*(__fastcall*)(void*, void*, const PString*, const void*);
    auto resCtor = Get<ResCtor>(kr, "??0Resource@Jet@@QAE@ABVPString@1@PBVResourceDatabase@1@@Z");
    static void* fdbVt[8];
    fdbVt[3] = reinterpret_cast<void*>(&DbGetFileName);
    void* fdb[1] = {fdbVt};
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    spritePath = std::string(tmp) + "diff_r2d.sprite.txt";
    const PString name = MakePString("data/ui/diff.sprite.txt");
    int loaded = 0;
    using LoadTxt = int*(__cdecl*)(void*, int*, const char*);
    using LoadBin = int*(__cdecl*)(void*, int*, const char*);
    for (int c = 0; c < cases; ++c) {
        WriteSpriteFile(spritePath.c_str());
        alignas(8) unsigned char obj[2][0xc8];
        int result[2], binary[2];
        for (int s = 0; s < 2; ++s) {
            std::memset(obj[s], 0, sizeof obj[s]);
            resCtor(obj[s], nullptr, &name, fdb);
            *reinterpret_cast<void***>(obj[s]) = vt[s];
            // the constructor's defaults (0x10003e50)
            float* f = reinterpret_cast<float*>(obj[s] + 0x60);
            f[0] = 0.5f, f[1] = 0.5f, f[2] = 1.0f, f[3] = 1.0f, f[4] = 1.0f, f[5] = 0, f[6] = 0, f[7] = 1, f[8] = 1;
            for (int i = 9; i < 15; ++i) f[i] = i == 13 ? 0.0f : 1.0f;
            int* n = reinterpret_cast<int*>(obj[s] + 0x60);
            n[0x40 / 4] = 7, n[0x48 / 4] = 1, n[0x4c / 4] = 1;
            reinterpret_cast<int*>(obj[s] + 0xb4)[0] = reinterpret_cast<int*>(obj[s] + 0xb4)[1] =
                reinterpret_cast<int*>(obj[s] + 0xb4)[2] = reinterpret_cast<int*>(obj[s] + 0xb4)[3] = -1;
            reinterpret_cast<LoadTxt>(vt[s][11])(obj[s], &result[s], "data/ui/diff.sprite.txt");
            reinterpret_cast<LoadBin>(vt[s][10])(obj[s], &binary[s], "x");
        }
        loaded += result[0] == 0;
        Check(result[0] == result[1] && binary[0] == binary[1], "SpriteResource load result", c);
        Check(std::memcmp(obj[0] + 0x60, obj[1] + 0x60, 0x50) == 0 && std::memcmp(obj[0] + 0xb4, obj[1] + 0xb4, 0x14) == 0,
              "SpriteResource attributes", c);
        const char* ta = Text(obj[0] + 0xb0);
        const char* tb = Text(obj[1] + 0xb0);
        Check(std::strcmp(ta, tb) == 0, "SpriteResource texture name", c);
        using CalcSize = unsigned(__cdecl*)(const void*);
        Check(reinterpret_cast<CalcSize>(vt[0][15])(obj[0]) == reinterpret_cast<CalcSize>(vt[1][15])(obj[1]),
              "SpriteResource CalculateSize", c);
    }
    std::printf("SpriteResource: %d files, %d loaded\n", cases, loaded);
    // Outside the game ConfigParser cannot read files (it opens them, but MemoryFile::Open fails: result 3, also for
    // the game's own driverdefaults.txt), so both modules return 1 here and only the failure path is compared.
    DeleteFileA(spritePath.c_str());
}

static LONG WINAPI Crash(EXCEPTION_POINTERS* e)
{
    const void* at = e->ExceptionRecord->ExceptionAddress;
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<const char*>(at), &m);
    if (m) GetModuleFileNameA(m, name, MAX_PATH);
    std::printf("crash %08lx at %p (%s +%tx), last call: %s\n", e->ExceptionRecord->ExceptionCode, at, name,
                static_cast<const char*>(at) - reinterpret_cast<const char*>(m),
                callLog.empty() ? "-" : callLog.back().c_str());
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetUnhandledExceptionFilter(&Crash);
    if (argc < 2) {
        std::printf("usage: diff_r2d <dir> [cases]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int cases = argc > 2 ? std::atoi(argv[2]) : 2000;
    SetDllDirectoryA(dir.c_str());
    // the original first: its import table pulls msvcr80.dll, which our KernelStandardInit's Crt80::setlocale
    // call needs (see diff_r3d_u6; ours loaded first in a bare process answers ERROR_DLL_INIT_FAILED)
    orig = LoadLibraryA((dir + "\\Render2DCore_7c_orig.dll").c_str());
    ks = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    ours = LoadLibraryA((dir + "\\Render2DCore_7c.dll").c_str());
    if (!ks || !ours || !orig) {
        std::printf("cannot load the modules from %s\n", dir.c_str());
        return 2;
    }
    pstringCache = *Get<void**>(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    createNode = Get<CreateFn>(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    jetNew = Get<NewFn>(ks, "?New@Alloc@Jet@@SAPAXI@Z");

    FillStubs(std::make_integer_sequence<int, 0x100>(), displayVt, true);
    displayVt[0x80 / 4] = reinterpret_cast<void*>(&DisplayTexel);
    displayVt[0x98 / 4] = reinterpret_cast<void*>(&DisplayCreateTexture);
    displayVt[0xa0 / 4] = reinterpret_cast<void*>(&DisplaySetTexture);
    displayVt[0xe0 / 4] = reinterpret_cast<void*>(&DisplayDrawUser);
    displayVt[0xe4 / 4] = reinterpret_cast<void*>(&DisplayDraw);
    displayVt[0x10c / 4] = reinterpret_cast<void*>(&DisplayGetDeclaration);
    FillStubs(std::make_integer_sequence<int, 24>(), texVt, false);
    texVt[0] = reinterpret_cast<void*>(&TexDelete);
    texVt[0x04 / 4] = reinterpret_cast<void*>(&TexCreate);
    texVt[0x0c / 4] = reinterpret_cast<void*>(&TexUpload);
    texVt[0x10 / 4] = reinterpret_cast<void*>(&TexUpdate);
    texVt[0x20 / 4] = reinterpret_cast<void*>(&TexBind);
    texVt[0x38 / 4] = reinterpret_cast<void*>(&TexGetSize);
    texVt[0x3c / 4] = reinterpret_cast<void*>(&TexTouch);
    texVt[0x44 / 4] = reinterpret_cast<void*>(&TexSelect);
    mgrVt[3] = reinterpret_cast<void*>(&MgrGet);
    resVt[0] = reinterpret_cast<void*>(&ResLock);
    resVt[1] = reinterpret_cast<void*>(&ResUnlock);
    dbVt[1] = reinterpret_cast<void*>(&DbGet);

    *reinterpret_cast<void**>(displayMem) = displayVt;
    *reinterpret_cast<void**>(displayMem + 0x54) = displayState;
    *reinterpret_cast<Size*>(displayMem + 0x28c) = Size{1024, 768};  // the target size (no render target)
    *reinterpret_cast<void**>(kernelMem + 0x7c) = displayMem;
    *reinterpret_cast<void**>(kernelMem + 0x94) = manager;
    std::memcpy(stateStart, displayState, sizeof stateStart);

    TestSelectionBox(cases);
    std::printf("selection box: %llu checks, %llu mismatches\n", checks, bad);
    TestCanvas(cases / 4);
    std::printf("canvas: %llu checks, %llu mismatches\n", checks, bad);
    TestSprites(cases / 2);
    std::printf("sprites: %llu checks, %llu mismatches\n", checks, bad);
    TestSpriteResource(cases);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
