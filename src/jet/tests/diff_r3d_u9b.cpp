// Differential test of Render3DCore_7c unit U9b: the scene-graph classes (AttachmentPoint/Observer, ReferencePoint
// members, Cell state, the Portal family, Mirror, Location, Corona, Function) against Render3DCore_7c_orig.dll in
// one process.
//
// Both modules run the same scenario on the same static object buffers, so raw bytes (including the self pointers
// inside the particle bounding volume) can be compared. Only each object's own vftable word and the heap addresses
// of Cell's node/volume are excluded; Cell's node and volume are compared field by field instead. Awkward rounds use
// signed zero, infinities and quiet/signalling NaNs (x87 copies quiet a signalling NaN, as the original does).
//
// usage: diff_r3d_u9b.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
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
struct Box {
    Vec3 min, max;
};
struct PS {
    void* node;
};
struct It {
    void* node;
};

static_assert(sizeof(Vec2) == 8 && sizeof(Vec3) == 0xc && sizeof(Vec4) == 0x10, "vectors");
static_assert(sizeof(Mat4) == 0x40 && sizeof(Box) == 0x18 && sizeof(PS) == 4 && sizeof(It) == 4, "objects");

static HMODULE mod[2];
static void* pstringCache;
static void* (*CreateNode)(void*, const unsigned char*);
static void (*DeleteAlloc)(void*);
static unsigned long long checks, bad;
static std::mt19937 rng(0x9b5eed);

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

static FARPROC RawProc(int side, const char* name)
{
    FARPROC p = GetProcAddress(mod[side], name);
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

static bool IsImagePointer(int side, uint32_t v)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(mod[side]);
    const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return v >= base && v < base + nt->OptionalHeader.SizeOfImage;
}

static bool ModuleSpecificByteAt(const unsigned char* a, const unsigned char* b, size_t i, size_t n)
{
    for (size_t k = 0; k < 4 && k <= i; ++k) {
        const size_t off = i - k;
        if (off + 4 > n) continue;
        uint32_t u, v;
        std::memcpy(&u, a + off, 4);
        std::memcpy(&v, b + off, 4);
        if (IsImagePointer(0, u) && IsImagePointer(1, v)) return true;
        const bool un = (u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) != 0;
        const bool vn = (v & 0x7f800000u) == 0x7f800000u && (v & 0x007fffffu) != 0;
        if (un && vn) return true;
    }
    return false;
}

static bool SameBytesMasked(const unsigned char* a, const unsigned char* b, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (a[i] == b[i]) continue;
        if (ModuleSpecificByteAt(a, b, i, n)) continue;
        return false;
    }
    return true;
}

static void PrintFirstDiff(const unsigned char* a, const unsigned char* b, size_t n, const char* what)
{
    for (size_t i = 0; i < n; ++i) {
        if (a[i] == b[i]) continue;
        if (ModuleSpecificByteAt(a, b, i, n)) continue;
        std::printf("%s first diff +0x%x ours=%02x orig=%02x\n", what, static_cast<unsigned>(i), a[i], b[i]);
        return;
    }
}

static void CheckBytes(const void* a, const void* b, size_t n, const char* what)
{
    const bool ok = SameBytesMasked(static_cast<const unsigned char*>(a), static_cast<const unsigned char*>(b), n);
    if (!ok)
        PrintFirstDiff(static_cast<const unsigned char*>(a), static_cast<const unsigned char*>(b), n, what);
    Check(ok, what);
}

static void* Node(int index)
{
    static void* made[8];
    static bool init = false;
    if (!init) {
        const char* texts[8] = {"", "alpha", "beta", "gamma", "delta", "host", "leaf", "point"};
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

static PS TextName(const char* text)
{
    PS p;
    p.node = CreateNode(pstringCache, reinterpret_cast<const unsigned char*>(text));
    return p;
}

// ---- module API ----------------------------------------------------------------------------------------------

struct Api {
    void(__thiscall* pCtor0)(void*);
    void(__thiscall* pDtor)(void*);
    void(__cdecl* pSetName)(void*, const PS&);
    void(__cdecl* pSetPosition)(void*, const Vec3&);
    void(__cdecl* pSetOrientation)(void*, const Vec4&);
    void(__cdecl* pAdd)(void*, void*);
    void(__cdecl* pRemoveAll)(void*);
    bool(__cdecl* pRemoveChild)(void*, void*);
    void*(__cdecl* pRemove)(void*);
    void*(__cdecl* pPop)(void*);
    void(__cdecl* pPush)(void*, void*);
    void(__cdecl* pInsertBefore)(void*, It*, It&, void*);
    void(__cdecl* pInsertAfter)(void*, It*, It&, void*);
    void(__cdecl* pRemoveIter)(void*, It*, It&);

    void(__thiscall* obsCtor0)(void*);
    void(__thiscall* obsCtorPoint)(void*, void*);
    void(__thiscall* obsCtorCopy)(void*, const void*);
    void*(__thiscall* obsAssign)(void*, const void*);
    void(__thiscall* obsDtor)(void*);

    void(__thiscall* apCtor0)(void*);
    void(__thiscall* apCtorCopy)(void*, const void*);
    void(__thiscall* apCtorNH)(void*, const PS&, const PS&);
    void(__thiscall* apCtorNHPR)(void*, const PS&, const PS&, const Vec3&, const Vec4&);
    void(__thiscall* apDtor)(void*);
    void*(__thiscall* apAssign)(void*, const void*);
    void(__cdecl* apAdd)(void*, void*);
    bool(__cdecl* apAddNamed)(void*, const PS&, void*);
    void(__cdecl* apPush)(void*, void*);
    void(__cdecl* apInsertBefore)(void*, It*, It&, void*);
    void(__cdecl* apInsertAfter)(void*, It*, It&, void*);
    void*(__cdecl* apRemove)(void*);
    void(__cdecl* apRemoveIter)(void*, It*, It&);
    bool(__cdecl* apRemoveChild)(void*, void*);
    void*(__cdecl* apPop)(void*);
    void*(__cdecl* apCreateInstance)(const void*);
    PS*(__cdecl* apGetHostName)(const void*);
    void(__cdecl* apSetHostName)(void*, const PS&);
    int(__cdecl* apGetID)(const void*);
    bool(__cdecl* apIsOfType)(const void*, int);
    void(__cdecl* apSubNames)(const PS&, PS&, PS&);
    void(__cdecl* apAddObserver)(void*, void*);
    bool(__cdecl* apRemoveObserver)(void*, void*);
    void(__cdecl* apClearObservers)(void*);
    void(__cdecl* apInitInstance)(const void*, void*);

    void(__thiscall* refCtorCopy)(void*, const void*);
    void(__thiscall* refDtor)(void*);
    void*(__thiscall* refAssign)(void*, const void*);
    PS*(__cdecl* refGetName)(const void*);
    void(__cdecl* refSetName)(void*, const PS&);
    Vec3*(__cdecl* refGetPosition)(const void*);
    void(__cdecl* refSetPosition)(void*, const Vec3&);
    Vec4*(__cdecl* refGetOrientation)(const void*);
    void(__cdecl* refSetOrientation)(void*, const Vec4&);

    void(__thiscall* cellCtor0)(void*);
    void(__thiscall* cellCtorCopy)(void*, const void*);
    void*(__thiscall* cellAssign)(void*, const void*);
    void(__thiscall* cellDtor)(void*);
    void(__cdecl* cellGetExtents)(const void*, Box*);
    int(__cdecl* cellGetID)(const void*);
    bool(__cdecl* cellIsOfType)(const void*, int);

    void(__thiscall* portalCtor0)(void*);
    void(__thiscall* portalCtorCopy)(void*, const void*);
    void*(__thiscall* portalAssign)(void*, const void*);
    void(__thiscall* portalDtor)(void*);
    int(__cdecl* portalGetID)(const void*);
    bool(__cdecl* portalIsOfType)(const void*, int);
    bool(__cdecl* portalGetStencil)(const void*);
    void(__cdecl* portalSetStencil)(void*, bool);
    void(__cdecl* portalSetGlobal)(bool);
    bool(__cdecl* portalGetGlobal)();
    unsigned(__cdecl* portalGetTraversed)();
    unsigned(__cdecl* portalGetTotal)();
    void(__cdecl* portalTraverse)(void*, void*, const Mat4*, void*);
    void(__cdecl* portalReset)();

    void(__thiscall* ppCtor)(void*, void*, const Vec2&);
    void(__thiscall* ppCtorCopy)(void*, const void*);
    void*(__thiscall* ppAssign)(void*, const void*);
    void(__thiscall* ppDtor)(void*);
    int(__cdecl* ppGetID)(const void*);
    bool(__cdecl* ppIsOfType)(const void*, int);
    Vec2*(__cdecl* ppGetSize)(const void*);
    void*(__cdecl* ppGetDest)(void*);
    const void*(__cdecl* ppGetDestConst)(const void*);
    bool(__cdecl* ppGetNon)(const void*);
    void(__cdecl* ppSetNon)(void*, bool);

    void(__thiscall* vpCtor)(void*, void*, const Vec2&);
    void(__thiscall* vpCtorCopy)(void*, const void*);
    void*(__thiscall* vpAssign)(void*, const void*);
    void(__thiscall* vpDtor)(void*);
    int(__cdecl* vpGetID)(const void*);
    bool(__cdecl* vpIsOfType)(const void*, int);
    void*(__cdecl* vpGetRef)(void*);
    const void*(__cdecl* vpGetRefConst)(const void*);
    void(__cdecl* vpSetRef)(void*, void*);
    void(__cdecl* vpSetWarp)(void*, const Mat4&);

    void(__thiscall* mCtor)(void*, void*, const Vec2&);
    void(__thiscall* mCtorCopy)(void*, const void*);
    void*(__thiscall* mAssign)(void*, const void*);
    void(__thiscall* mDtor)(void*);
    int(__cdecl* mGetID)(const void*);
    bool(__cdecl* mIsOfType)(const void*, int);
    void*(__cdecl* mGetRef)(void*);
    const void*(__cdecl* mGetRefConst)(const void*);
    void(__cdecl* mSetRef)(void*, void*);

    void(__thiscall* locCtor0)(void*);
    void(__thiscall* locCtor)(void*, const Vec3&, void*);
    void(__thiscall* locCtorCopy)(void*, const void*);
    void*(__thiscall* locAssign)(void*, const void*);
    void(__cdecl* locSet)(void*, const Vec3&, void*);
    void(__cdecl* locGetGlobal)(const void*, Vec3*);

    void(__thiscall* corCtor)(void*);
    void(__thiscall* corDtor)(void*);
    void*(__thiscall* corAssign)(void*, const void*);
    void(__cdecl* corCull)(void*, void*, const Mat4*, Vec3*);
    void(__cdecl* corRender)(void*, Mat4*);

    void(__thiscall* fnCtor0)(void*);
    void(__thiscall* fnCtorCopy)(void*, const void*);
    void*(__thiscall* fnAssign)(void*, const void*);
};

static Api a[2];

static void LoadApi(int s)
{
    Api& q = a[s];
    q.pCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0Particle@Jet@@QAE@XZ");
    q.pDtor = Proc<void(__thiscall*)(void*)>(s, "??1Particle@Jet@@UAE@XZ");
    q.pSetName = Proc<void(__cdecl*)(void*, const PS&)>(s, "?SetName@Particle@Jet@@UAAXABVPString@2@@Z");
    q.pSetPosition = Proc<void(__cdecl*)(void*, const Vec3&)>(s, "?SetPosition@Particle@Jet@@UAAXABVVector3@2@@Z");
    q.pSetOrientation =
        Proc<void(__cdecl*)(void*, const Vec4&)>(s, "?SetOrientation@Particle@Jet@@UAAXABVQuaternion@2@@Z");
    q.pAdd = Proc<void(__cdecl*)(void*, void*)>(s, "?Add@Particle@Jet@@UAAXPAV12@@Z");
    q.pRemoveAll = Proc<void(__cdecl*)(void*)>(s, "?RemoveAll@Particle@Jet@@UAAXXZ");
    q.pRemoveChild = Proc<bool(__cdecl*)(void*, void*)>(s, "?Remove@Particle@Jet@@UAA_NPAV12@@Z");
    q.pRemove = Proc<void*(__cdecl*)(void*)>(s, "?Remove@Particle@Jet@@UAAPAV12@XZ");
    q.pPop = Proc<void*(__cdecl*)(void*)>(s, "?Pop@Particle@Jet@@UAAPAV12@XZ");
    q.pPush = Proc<void(__cdecl*)(void*, void*)>(s, "?Push@Particle@Jet@@UAAXPAV12@@Z");
    q.pInsertBefore = Proc<void(__cdecl*)(void*, It*, It&, void*)>(
        s, "?InsertBefore@Particle@Jet@@UAA?AVIterator@?$ITree@VParticle@Jet@@@2@AAV342@PAV12@@Z");
    q.pInsertAfter = Proc<void(__cdecl*)(void*, It*, It&, void*)>(
        s, "?InsertAfter@Particle@Jet@@UAA?AVIterator@?$ITree@VParticle@Jet@@@2@AAV342@PAV12@@Z");
    q.pRemoveIter = Proc<void(__cdecl*)(void*, It*, It&)>(
        s, "?Remove@Particle@Jet@@UAA?AVIterator@?$ITree@VParticle@Jet@@@2@AAV342@@Z");

    q.obsCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0AttachmentObserver@Jet@@QAE@XZ");
    q.obsCtorPoint = Proc<void(__thiscall*)(void*, void*)>(s, "??0AttachmentObserver@Jet@@QAE@PAVAttachmentPoint@1@@Z");
    q.obsCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0AttachmentObserver@Jet@@QAE@ABV01@@Z");
    q.obsAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4AttachmentObserver@Jet@@QAEAAV01@ABV01@@Z");
    q.obsDtor = Proc<void(__thiscall*)(void*)>(s, "??1AttachmentObserver@Jet@@UAE@XZ");

    q.apCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0AttachmentPoint@Jet@@QAE@XZ");
    q.apCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0AttachmentPoint@Jet@@QAE@ABV01@@Z");
    q.apCtorNH = Proc<void(__thiscall*)(void*, const PS&, const PS&)>(s, "??0AttachmentPoint@Jet@@QAE@ABVPString@1@0@Z");
    q.apCtorNHPR = Proc<void(__thiscall*)(void*, const PS&, const PS&, const Vec3&, const Vec4&)>(
        s, "??0AttachmentPoint@Jet@@QAE@ABVPString@1@0ABVVector3@1@ABVQuaternion@1@@Z");
    q.apDtor = Proc<void(__thiscall*)(void*)>(s, "??1AttachmentPoint@Jet@@UAE@XZ");
    q.apAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4AttachmentPoint@Jet@@QAEAAV01@ABV01@@Z");
    q.apAdd = Proc<void(__cdecl*)(void*, void*)>(s, "?Add@AttachmentPoint@Jet@@UAAXPAVParticle@2@@Z");
    q.apAddNamed =
        Proc<bool(__cdecl*)(void*, const PS&, void*)>(s, "?Add@AttachmentPoint@Jet@@UAA_NABVPString@2@PAVParticle@2@@Z");
    q.apPush = Proc<void(__cdecl*)(void*, void*)>(s, "?Push@AttachmentPoint@Jet@@UAAXPAVParticle@2@@Z");
    q.apInsertBefore = Proc<void(__cdecl*)(void*, It*, It&, void*)>(
        s, "?InsertBefore@AttachmentPoint@Jet@@UAA?AVIterator@?$ITree@VParticle@Jet@@@2@AAV342@PAVParticle@2@@Z");
    q.apInsertAfter = Proc<void(__cdecl*)(void*, It*, It&, void*)>(
        s, "?InsertAfter@AttachmentPoint@Jet@@UAA?AVIterator@?$ITree@VParticle@Jet@@@2@AAV342@PAVParticle@2@@Z");
    q.apRemove = Proc<void*(__cdecl*)(void*)>(s, "?Remove@AttachmentPoint@Jet@@UAAPAVParticle@2@XZ");
    q.apRemoveIter = Proc<void(__cdecl*)(void*, It*, It&)>(
        s, "?Remove@AttachmentPoint@Jet@@UAA?AVIterator@?$ITree@VParticle@Jet@@@2@AAV342@@Z");
    q.apRemoveChild =
        Proc<bool(__cdecl*)(void*, void*)>(s, "?Remove@AttachmentPoint@Jet@@UAA_NPAVParticle@2@@Z");
    q.apPop = Proc<void*(__cdecl*)(void*)>(s, "?Pop@AttachmentPoint@Jet@@UAAPAVParticle@2@XZ");
    q.apCreateInstance =
        Proc<void*(__cdecl*)(const void*)>(s, "?CreateInstance@AttachmentPoint@Jet@@UBAPAVParticle@2@XZ");
    q.apGetHostName =
        Proc<PS*(__cdecl*)(const void*)>(s, "?GetHostName@AttachmentPoint@Jet@@QBAABVPString@2@XZ");
    q.apSetHostName =
        Proc<void(__cdecl*)(void*, const PS&)>(s, "?SetHostName@AttachmentPoint@Jet@@QAAXABVPString@2@@Z");
    q.apGetID = Proc<int(__cdecl*)(const void*)>(s, "?GetID@AttachmentPoint@Jet@@UBAHXZ");
    q.apIsOfType = Proc<bool(__cdecl*)(const void*, int)>(s, "?IsOfType@AttachmentPoint@Jet@@UBA_NH@Z");
    q.apSubNames = Proc<void(__cdecl*)(const PS&, PS&, PS&)>(
        s, "?GetSubNames@AttachmentPoint@Jet@@SAXABVPString@2@AAV32@1@Z");
    q.apAddObserver =
        Proc<void(__cdecl*)(void*, void*)>(s, "?AddObserver@AttachmentPoint@Jet@@UAAXPAVAttachmentObserver@2@@Z");
    q.apRemoveObserver =
        Proc<bool(__cdecl*)(void*, void*)>(s, "?RemoveObserver@AttachmentPoint@Jet@@UAA_NPAVAttachmentObserver@2@@Z");
    q.apClearObservers = Proc<void(__cdecl*)(void*)>(s, "?ClearObservers@AttachmentPoint@Jet@@UAAXXZ");
    q.apInitInstance =
        Proc<void(__cdecl*)(const void*, void*)>(s, "?InitInstance@AttachmentPoint@Jet@@IBAXAAV12@@Z");

    q.refCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0ReferencePoint@Jet@@QAE@ABV01@@Z");
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

    q.cellCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0Cell@Jet@@QAE@XZ");
    q.cellCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0Cell@Jet@@QAE@ABV01@@Z");
    q.cellAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4Cell@Jet@@QAEAAV01@ABV01@@Z");
    q.cellDtor = Proc<void(__thiscall*)(void*)>(s, "??1Cell@Jet@@UAE@XZ");
    q.cellGetExtents =
        Proc<void(__cdecl*)(const void*, Box*)>(s, "?GetExtents@Cell@Jet@@QBA?AVAxisAlignedBox@2@XZ");
    q.cellGetID = Proc<int(__cdecl*)(const void*)>(s, "?GetID@Cell@Jet@@UBAHXZ");
    q.cellIsOfType = Proc<bool(__cdecl*)(const void*, int)>(s, "?IsOfType@Cell@Jet@@UBA_NH@Z");

    q.portalCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0Portal@Jet@@QAE@XZ");
    q.portalCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0Portal@Jet@@QAE@ABV01@@Z");
    q.portalAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4Portal@Jet@@QAEAAV01@ABV01@@Z");
    q.portalDtor = Proc<void(__thiscall*)(void*)>(s, "??1Portal@Jet@@UAE@XZ");
    q.portalGetID = Proc<int(__cdecl*)(const void*)>(s, "?GetID@Portal@Jet@@UBAHXZ");
    q.portalIsOfType = Proc<bool(__cdecl*)(const void*, int)>(s, "?IsOfType@Portal@Jet@@UBA_NH@Z");
    q.portalGetStencil = Proc<bool(__cdecl*)(const void*)>(s, "?GetStencil@Portal@Jet@@QBA_NXZ");
    q.portalSetStencil = Proc<void(__cdecl*)(void*, bool)>(s, "?SetStencil@Portal@Jet@@IAAX_N@Z");
    q.portalSetGlobal = Proc<void(__cdecl*)(bool)>(s, "?SetEnableGlobalStencil@Portal@Jet@@SAX_N@Z");
    q.portalGetGlobal = Proc<bool(__cdecl*)()>(s, "?GetEnableGlobalStencil@Portal@Jet@@SA_NXZ");
    q.portalGetTraversed = Proc<unsigned(__cdecl*)()>(s, "?GetTraversed@Portal@Jet@@SAIXZ");
    q.portalGetTotal = Proc<unsigned(__cdecl*)()>(s, "?GetTotal@Portal@Jet@@SAIXZ");
    q.portalTraverse = Proc<void(__cdecl*)(void*, void*, const Mat4*, void*)>(
        s, "?Traverse@Portal@Jet@@UAAXAAVViewPoint@2@ABVMatrix4@2@PAVCamera@2@@Z");
    q.portalReset = Proc<void(__cdecl*)()>(s, "?Reset@Portal@Jet@@SAXXZ");

    q.ppCtor = Proc<void(__thiscall*)(void*, void*, const Vec2&)>(
        s, "??0PhysicalPortal@Jet@@QAE@PAVCell@1@ABVVector2@1@@Z");
    q.ppCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0PhysicalPortal@Jet@@QAE@ABV01@@Z");
    q.ppAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4PhysicalPortal@Jet@@QAEAAV01@ABV01@@Z");
    q.ppDtor = Proc<void(__thiscall*)(void*)>(s, "??1PhysicalPortal@Jet@@UAE@XZ");
    q.ppGetID = Proc<int(__cdecl*)(const void*)>(s, "?GetID@PhysicalPortal@Jet@@UBAHXZ");
    q.ppIsOfType = Proc<bool(__cdecl*)(const void*, int)>(s, "?IsOfType@PhysicalPortal@Jet@@UBA_NH@Z");
    q.ppGetSize = Proc<Vec2*(__cdecl*)(const void*)>(s, "?GetSize@PhysicalPortal@Jet@@QBAABVVector2@2@XZ");
    q.ppGetDest = Proc<void*(__cdecl*)(void*)>(s, "?GetDestination@PhysicalPortal@Jet@@QAAPAVCell@2@XZ");
    q.ppGetDestConst =
        Proc<const void*(__cdecl*)(const void*)>(s, "?GetDestination@PhysicalPortal@Jet@@QBAPBVCell@2@XZ");
    q.ppGetNon = Proc<bool(__cdecl*)(const void*)>(s, "?GetNonReenterant@PhysicalPortal@Jet@@QBA_NXZ");
    q.ppSetNon = Proc<void(__cdecl*)(void*, bool)>(s, "?SetNonReenterant@PhysicalPortal@Jet@@QAAX_N@Z");

    q.vpCtor = Proc<void(__thiscall*)(void*, void*, const Vec2&)>(
        s, "??0VirtualPortal@Jet@@QAE@PAVCell@1@ABVVector2@1@@Z");
    q.vpCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0VirtualPortal@Jet@@QAE@ABV01@@Z");
    q.vpAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4VirtualPortal@Jet@@QAEAAV01@ABV01@@Z");
    q.vpDtor = Proc<void(__thiscall*)(void*)>(s, "??1VirtualPortal@Jet@@UAE@XZ");
    q.vpGetID = Proc<int(__cdecl*)(const void*)>(s, "?GetID@VirtualPortal@Jet@@UBAHXZ");
    q.vpIsOfType = Proc<bool(__cdecl*)(const void*, int)>(s, "?IsOfType@VirtualPortal@Jet@@UBA_NH@Z");
    q.vpGetRef = Proc<void*(__cdecl*)(void*)>(s, "?GetReference@VirtualPortal@Jet@@QAAPAVParticle@2@XZ");
    q.vpGetRefConst =
        Proc<const void*(__cdecl*)(const void*)>(s, "?GetReference@VirtualPortal@Jet@@QBAPBVParticle@2@XZ");
    q.vpSetRef = Proc<void(__cdecl*)(void*, void*)>(s, "?SetReference@VirtualPortal@Jet@@QAAXPAVParticle@2@@Z");
    q.vpSetWarp = Proc<void(__cdecl*)(void*, const Mat4&)>(s, "?SetWarpMatrix@VirtualPortal@Jet@@AAAXABVMatrix4@2@@Z");

    q.mCtor = Proc<void(__thiscall*)(void*, void*, const Vec2&)>(
        s, "??0Mirror@Jet@@QAE@PAVCell@1@ABVVector2@1@@Z");
    q.mCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0Mirror@Jet@@QAE@ABV01@@Z");
    q.mAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4Mirror@Jet@@QAEAAV01@ABV01@@Z");
    q.mDtor = Proc<void(__thiscall*)(void*)>(s, "??1Mirror@Jet@@UAE@XZ");
    q.mGetID = Proc<int(__cdecl*)(const void*)>(s, "?GetID@Mirror@Jet@@UBAHXZ");
    q.mIsOfType = Proc<bool(__cdecl*)(const void*, int)>(s, "?IsOfType@Mirror@Jet@@UBA_NH@Z");
    q.mGetRef = Proc<void*(__cdecl*)(void*)>(s, "?GetReference@Mirror@Jet@@AAAPAVParticle@2@XZ");
    q.mGetRefConst = Proc<const void*(__cdecl*)(const void*)>(s, "?GetReference@Mirror@Jet@@ABAPBVParticle@2@XZ");
    q.mSetRef = Proc<void(__cdecl*)(void*, void*)>(s, "?SetReference@Mirror@Jet@@AAAXPAVParticle@2@@Z");

    q.locCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0Location@Jet@@QAE@XZ");
    q.locCtor = Proc<void(__thiscall*)(void*, const Vec3&, void*)>(
        s, "??0Location@Jet@@QAE@ABVVector3@1@PAVParticle@1@@Z");
    q.locCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0Location@Jet@@QAE@ABV01@@Z");
    q.locAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4Location@Jet@@QAEAAV01@ABV01@@Z");
    q.locSet = Proc<void(__cdecl*)(void*, const Vec3&, void*)>(s, "?Set@Location@Jet@@QAAXABVVector3@2@PAVParticle@2@@Z");
    q.locGetGlobal =
        Proc<void(__cdecl*)(const void*, Vec3*)>(s, "?GetGlobalLocation@Location@Jet@@QBA?AVVector3@2@XZ");

    q.corCtor = Proc<void(__thiscall*)(void*)>(s, "??0Corona@Jet@@QAE@XZ");
    q.corDtor = Proc<void(__thiscall*)(void*)>(s, "??1Corona@Jet@@QAE@XZ");
    q.corAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4Corona@Jet@@QAEAAV01@ABV01@@Z");
    q.corCull = Proc<void(__cdecl*)(void*, void*, const Mat4*, Vec3*)>(
        s, "?Cull@Corona@Jet@@QAAXAAVViewPoint@2@ABVMatrix4@2@AAVVector3@2@@Z");
    q.corRender = Proc<void(__cdecl*)(void*, Mat4*)>(s, "?Render@Corona@Jet@@QAAXAAVMatrix4@2@@Z");

    q.fnCtor0 = Proc<void(__thiscall*)(void*)>(s, "??0Function@Jet@@QAE@XZ");
    q.fnCtorCopy = Proc<void(__thiscall*)(void*, const void*)>(s, "??0Function@Jet@@QAE@ABV01@@Z");
    q.fnAssign = Proc<void*(__thiscall*)(void*, const void*)>(s, "??4Function@Jet@@QAEAAV01@ABV01@@Z");
}

// ---- snapshots ------------------------------------------------------------------------------------------------

static unsigned char res[2][0x6000];
static size_t at;
static int side;

static void Emit(const void* p, size_t n)
{
    if (at + n > sizeof res[0]) {
        std::printf("snapshot overflow\n");
        std::exit(2);
    }
    std::memcpy(res[side] + at, p, n);
    at += n;
}

static void EmitInt(int v)
{
    Emit(&v, 4);
}

static void EmitBool(bool v)
{
    Emit(&v, 1);
}

static void EmitPtr(const void* p)
{
    Emit(&p, 4);
}

static void EmitObj(const void* p, size_t size)
{
    Emit(static_cast<const unsigned char*>(p) + 4, size - 4);
}

static unsigned char o0[0x200], o1[0x200], o2[0x200], o3[0x200], o4[0x200];

static void Seed(void* p, size_t n)
{
    std::memset(p, 0xa5, n);
}

struct ScenarioData {
    Vec3 pos, pos2;
    Vec4 rot, rot2;
    Vec2 size;
    Mat4 mat;
    Box box;
} sc;

static void MakeScenario(bool awkward)
{
    sc.pos = RandomVec(awkward);
    sc.pos2 = RandomVec(awkward);
    sc.rot = RandomQuat(awkward);
    sc.rot2 = RandomQuat(awkward);
    sc.size = Vec2{Val(awkward), Val(awkward)};
    for (int i = 0; i < 16; ++i) sc.mat.m[i] = Val(awkward);
    sc.box.min = RandomVec(awkward);
    sc.box.max = RandomVec(awkward);
}

// observer callbacks: the original's slots 1/2/3 are void (Particle*), void (Particle*) and void ()
static int addedCalls, removedCalls, shutdownCalls;
static void* lastAdded;
static void* lastRemoved;

static void __cdecl ObsAdded(void*, void* particle)
{
    ++addedCalls;
    lastAdded = particle;
}
static void __cdecl ObsRemoved(void*, void* particle)
{
    ++removedCalls;
    lastRemoved = particle;
}
static void __cdecl ObsShutdown(void* self)
{
    ++shutdownCalls;
    *reinterpret_cast<void**>(static_cast<unsigned char*>(self) + 4) = nullptr;
}
static void* const obsVtable[4] = {nullptr, reinterpret_cast<void*>(&ObsAdded), reinterpret_cast<void*>(&ObsRemoved),
                                   reinterpret_cast<void*>(&ObsShutdown)};

static int ObserverCount(void* ap)
{
    void* const head = *reinterpret_cast<void**>(static_cast<unsigned char*>(ap) + 0x130);
    return head ? static_cast<int>(*reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(head) + 8)) : 0;
}

// ---- sections ------------------------------------------------------------------------------------------------

static void RunAttachment(int s)
{
    // constructors and accessors
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    Seed(o2, sizeof o2);
    a[s].apCtor0(o0);
    a[s].apCtorNH(o1, Name(1), Name(5));
    a[s].apCtorNHPR(o2, Name(2), Name(6), sc.pos, sc.rot);
    EmitObj(o0, 0x134);
    EmitObj(o1, 0x134);
    EmitObj(o2, 0x134);
    Emit(a[s].apGetHostName(o1), 4);
    Emit(a[s].apGetHostName(o2), 4);
    EmitInt(a[s].apGetID(o2));
    EmitBool(a[s].apIsOfType(o2, 4));
    EmitBool(a[s].apIsOfType(o2, 1));
    EmitBool(a[s].apIsOfType(o2, 5));
    PS subNames[7] = {Name(0), Name(1), Name(7), TextName("alpha/beta"), TextName("a/b"), TextName("/x"),
                      TextName("x/")};
    for (int i = 0; i < 7; ++i) {
        PS first, second;
        first.node = nullptr;
        second.node = nullptr;
        PS in = subNames[i];
        a[s].apSubNames(in, first, second);
        Emit(&first.node, 4);
        Emit(&second.node, 4);
    }
    a[s].apDtor(o0);
     a[s].apDtor(o1);
     a[s].apDtor(o2);

    // copy and assignment
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    Seed(o2, sizeof o2);
    a[s].apCtorNHPR(o0, Name(3), Name(5), sc.pos2, sc.rot2);
    a[s].apCtorCopy(o1, o0);
    a[s].apCtor0(o2);
    a[s].apAssign(o2, o0);
    EmitObj(o1, 0x134);
    EmitObj(o2, 0x134);
    // a[s].apDtor(o2);
    // a[s].apDtor(o1);
    a[s].apDtor(o0);

    // tree operations (the observer list stays empty here)
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    Seed(o2, sizeof o2);
    Seed(o3, sizeof o3);
    Seed(o4, sizeof o4);
    a[s].apCtor0(o0);
    for (int i = 1; i <= 4; ++i) a[s].pCtor0(i == 1 ? o1 : (i == 2 ? o2 : (i == 3 ? o3 : o4)));
    a[s].pSetName(o1, Name(1));
    a[s].pSetName(o2, Name(2));
    a[s].pSetName(o3, Name(3));
    a[s].pSetName(o4, Name(4));
    a[s].apAdd(o0, o1);
    a[s].apPush(o0, o2);
    It target, inserted;
    target.node = o1;
    a[s].apInsertAfter(o0, &inserted, target, o3);
    target.node = o2;
    a[s].apInsertBefore(o0, &inserted, target, o4);
    EmitObj(o0, 0x134);
    EmitObj(o1, 0x12c);
    EmitObj(o2, 0x12c);
    EmitObj(o3, 0x12c);
    EmitObj(o4, 0x12c);
    Emit(&inserted.node, 4);
    void* const popped = a[s].apPop(o0);
    Emit(&popped, 4);
    bool const removedChild = a[s].apRemoveChild(o0, o1);
    Emit(&removedChild, 1);
    It it;
    it.node = o3;
    a[s].apRemoveIter(o0, &inserted, it);
    Emit(&inserted.node, 4);
    void* const removedLast = a[s].apRemove(o0);
    Emit(&removedLast, 4);
    EmitObj(o0, 0x134);
    // named add: an empty name adds under the root
    a[s].apAddNamed(o0, Name(0), o4);
    EmitObj(o0, 0x134);
    EmitObj(o4, 0x12c);
    a[s].apDtor(o0);
    a[s].pDtor(o1);
    a[s].pDtor(o2);
    a[s].pDtor(o3);
    a[s].pDtor(o4);

    // observers: slot callbacks, add/remove/clear
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    Seed(o2, sizeof o2);
    a[s].apCtor0(o0);
    a[s].obsCtor0(o1);
    *reinterpret_cast<void**>(o1) = reinterpret_cast<void*>(const_cast<void**>(obsVtable));
    addedCalls = removedCalls = shutdownCalls = 0;
    lastAdded = lastRemoved = nullptr;
    a[s].apAddObserver(o0, o1);
    EmitInt(ObserverCount(o0));
    void* const obsPoint = *reinterpret_cast<void**>(o1 + 4);
    Emit(&obsPoint, 4);
    a[s].pCtor0(o2);
    a[s].apAdd(o0, o2);
    EmitInt(addedCalls);
    Emit(&lastAdded, 4);
    const bool removedObs = a[s].apRemoveObserver(o0, o1);
    EmitBool(removedObs);
    EmitInt(removedCalls);
    Emit(&lastRemoved, 4);
    EmitInt(shutdownCalls);
    EmitInt(ObserverCount(o0));
    const void* const pointAfter = *reinterpret_cast<void**>(o1 + 4);
    Emit(&pointAfter, 4);
    // put it back and clear the list
    a[s].apAddObserver(o0, o1);
    a[s].apClearObservers(o0);
    EmitInt(shutdownCalls);
    EmitInt(ObserverCount(o0));
    a[s].apRemoveChild(o0, o2);
    a[s].apDtor(o0);
    a[s].obsDtor(o1);
    a[s].pDtor(o2);
    (void)pointAfter;

    // point constructor and InitInstance
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    Seed(o2, sizeof o2);
    a[s].apCtorNH(o0, Name(1), Name(6));
    a[s].obsCtorPoint(o1, o0);
    EmitInt(ObserverCount(o0));
    void* const obsPoint2 = *reinterpret_cast<void**>(o1 + 4);
    Emit(&obsPoint2, 4);
    a[s].apCtor0(o2);
    a[s].apInitInstance(o0, o2);
    EmitObj(o2, 0x134);
    a[s].apDtor(o2);
    a[s].obsDtor(o1);
    a[s].apDtor(o0);

    // CreateInstance: a heap AttachmentPoint with the host name copied
    Seed(o0, sizeof o0);
    a[s].apCtorNH(o0, Name(2), Name(6));
    void* const inst = a[s].apCreateInstance(o0);
    EmitBool(inst != nullptr);
    if (inst) {
        Emit(static_cast<unsigned char*>(inst) + 0x12c, 4);
        Emit(static_cast<unsigned char*>(inst) + 0x28, 4);
        const unsigned count = *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(inst) + 0x0c);
        Emit(&count, 4);
        a[s].apDtor(inst);
        DeleteAlloc(inst);
    }
    a[s].apDtor(o0);

    // named add: the child attaches to the named descendant, not to the root
    Seed(o2, sizeof o2);
    Seed(o3, sizeof o3);
    Seed(o4, sizeof o4);
    a[s].apCtor0(o3);
    a[s].pCtor0(o4);
    a[s].pCtor0(o2);
    a[s].pSetName(o4, Name(2));
    a[s].pSetName(o2, Name(3));
    a[s].apAdd(o3, o4);
    const bool namedAdd = a[s].apAddNamed(o3, Name(2), o2);
    EmitBool(namedAdd);
    const void* const targetParent = *reinterpret_cast<void**>(o2 + 0x10);
    Emit(&targetParent, 4);
    EmitObj(o3, 0x134);
    EmitObj(o4, 0x12c);
    // a null PString name adds directly under the root
    a[s].pRemoveChild(o4, o2);
    PS nullName;
    nullName.node = nullptr;
    const bool nullAdd = a[s].apAddNamed(o3, nullName, o2);
    EmitBool(nullAdd);
    const void* const nullParent = *reinterpret_cast<void**>(o2 + 0x10);
    Emit(&nullParent, 4);
    EmitInt(static_cast<int>(*reinterpret_cast<unsigned*>(o3 + 0x0c)));
    a[s].pRemoveChild(o4, o2);
    a[s].pRemoveAll(o3);
    a[s].apDtor(o3);
    a[s].pDtor(o2);
    a[s].pDtor(o4);
}

static void RunReferencePoint(int s)
{
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    Seed(o2, sizeof o2);

    *reinterpret_cast<void**>(o0) = Name(1).node;
    *reinterpret_cast<Vec3*>(o0 + 4) = sc.pos;
    *reinterpret_cast<Vec4*>(o0 + 0x10) = sc.rot;
    a[s].refCtorCopy(o1, o0);
    Emit(o1, 0x20);
    a[s].refCtorCopy(o2, o1);
    a[s].refAssign(o2, o0);
    Emit(o2, 0x20);
    Emit(a[s].refGetName(o0), 4);
    Emit(a[s].refGetName(o0), 0x20);
    Emit(a[s].refGetPosition(o1), 0x10);
    Emit(a[s].refGetOrientation(o2), 0x10);
    a[s].refSetName(o2, Name(3));
    a[s].refSetPosition(o2, sc.pos2);
    a[s].refSetOrientation(o2, sc.rot2);
    Emit(o2, 0x20);
    a[s].refDtor(o2);
    a[s].refDtor(o1);
    a[s].refDtor(o0);
}

static void RunLocation(int s)
{
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    Seed(o2, sizeof o2);
    Seed(o3, sizeof o3);
    a[s].locCtor0(o0);
    a[s].locCtor(o1, sc.pos, nullptr);
    a[s].locCtorCopy(o2, o1);
    a[s].locAssign(o3, o2);
    Emit(o0, 0x10);
    Emit(o1, 0x10);
    Emit(o2, 0x10);
    Emit(o3, 0x10);
    a[s].locSet(o3, sc.pos2, nullptr);
    Emit(o3, 0x10);
    Vec3 global{};
    a[s].locGetGlobal(o3, &global);
    Emit(&global, 0xc);

    // with a particle frame
    Seed(o4, sizeof o4);
    a[s].pCtor0(o4);
    a[s].pSetPosition(o4, sc.pos);
    a[s].pSetOrientation(o4, sc.rot);
    a[s].locSet(o3, sc.pos2, o4);
    a[s].locGetGlobal(o3, &global);
    Emit(&global, 0xc);
    Emit(o3, 0x10);
    a[s].pDtor(o4);
}

static void RunCoronaFunction(int s)
{
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    a[s].corCtor(o0);
    Emit(o0, 0x18);
    a[s].corAssign(o1, o0);
    Emit(o1, 0x18);
    Vec3 v = sc.pos;
    a[s].corCull(o1, nullptr, &sc.mat, &v);
    a[s].corRender(o1, &sc.mat);
    Emit(&v, 0xc);
    Emit(o1, 0x18);
    a[s].corDtor(o1);
    a[s].corDtor(o0);

    Seed(o2, sizeof o2);
    Seed(o3, sizeof o3);
    a[s].fnCtor0(o2);
    a[s].fnCtorCopy(o3, o2);
    using FnSlot = unsigned long(__cdecl*)(const void*);
    void** const fnVt = *reinterpret_cast<void***>(o3);
    EmitInt(static_cast<int>(reinterpret_cast<FnSlot>(fnVt[0])(o3)));
    a[s].fnAssign(o3, o2);
    EmitInt(static_cast<int>(reinterpret_cast<FnSlot>(fnVt[0])(o3)));
}

static void RunPortal(int s)
{
    const unsigned total0 = a[s].portalGetTotal();
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    a[s].portalCtor0(o0);
    EmitObj(o0, 0x134);
    EmitInt(static_cast<int>(a[s].portalGetTotal() - total0));
    EmitInt(a[s].portalGetID(o0));
    EmitBool(a[s].portalIsOfType(o0, 13));
    EmitBool(a[s].portalIsOfType(o0, 1));
    a[s].portalSetStencil(o0, true);
    a[s].portalSetGlobal(true);
    EmitBool(a[s].portalGetStencil(o0));
    EmitBool(a[s].portalGetGlobal());
    a[s].portalSetStencil(o0, false);
    EmitBool(a[s].portalGetStencil(o0));
    a[s].portalSetGlobal(false);
    EmitBool(a[s].portalGetGlobal());
    a[s].portalSetStencil(o0, true);
    EmitBool(a[s].portalGetStencil(o0));

    a[s].portalReset();
    const unsigned before = a[s].portalGetTraversed();
    a[s].portalTraverse(o0, nullptr, &sc.mat, nullptr);
    const unsigned after = a[s].portalGetTraversed();
    EmitInt(static_cast<int>(before));
    EmitInt(static_cast<int>(after));

    a[s].portalCtorCopy(o1, o0);
    EmitObj(o1, 0x134);
    a[s].portalAssign(o1, o0);
    EmitObj(o1, 0x134);
    // a[s].portalDtor(o1);
    a[s].portalDtor(o0);
    EmitInt(static_cast<int>(a[s].portalGetTotal() - total0));
}

static void RunPhysicalPortal(int s)
{
    const unsigned total0 = a[s].portalGetTotal();
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    a[s].ppCtor(o0, o4, sc.size);
    EmitObj(o0, 0x148);
    Emit(a[s].ppGetSize(o0), 8);
    EmitPtr(a[s].ppGetDest(o0));
    EmitPtr(a[s].ppGetDestConst(o0));
    EmitBool(a[s].ppGetNon(o0));
    a[s].ppSetNon(o0, true);
    EmitBool(a[s].ppGetNon(o0));
    EmitInt(a[s].ppGetID(o0));
    EmitBool(a[s].ppIsOfType(o0, 14));
    EmitBool(a[s].ppIsOfType(o0, 13));
    EmitBool(a[s].ppIsOfType(o0, 1));
    EmitBool(a[s].ppIsOfType(o0, 15));
    a[s].ppCtorCopy(o1, o0);
    EmitObj(o1, 0x148);
    a[s].ppAssign(o1, o0);
    EmitObj(o1, 0x148);
    // a[s].ppDtor(o1);
    a[s].ppDtor(o0);
    EmitInt(static_cast<int>(a[s].portalGetTotal() - total0));
}

static void RunVirtualMirror(int s)
{
    const unsigned total0 = a[s].portalGetTotal();
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    Seed(o2, sizeof o2);
    a[s].pCtor0(o2);  // stand-in reference particle
    a[s].vpCtor(o0, o4, sc.size);
    EmitObj(o0, 0x18c);
    a[s].vpSetRef(o0, o2);
    EmitPtr(a[s].vpGetRef(o0));
    EmitPtr(a[s].vpGetRefConst(o0));
    a[s].vpSetWarp(o0, sc.mat);
    EmitObj(o0, 0x18c);
    EmitInt(a[s].vpGetID(o0));
    EmitBool(a[s].vpIsOfType(o0, 15));
    EmitBool(a[s].vpIsOfType(o0, 1));
    EmitBool(a[s].vpIsOfType(o0, 16));
    a[s].vpCtorCopy(o1, o0);
    EmitObj(o1, 0x18c);
    a[s].vpAssign(o1, o0);
    EmitObj(o1, 0x18c);
    // a[s].vpDtor(o1);
    a[s].vpDtor(o0);

    Seed(o1, sizeof o1);
    Seed(o3, sizeof o3);
    a[s].mCtor(o1, o4, sc.size);
    EmitObj(o1, 0x18c);
    a[s].mSetRef(o1, o2);
    EmitPtr(a[s].mGetRef(o1));
    EmitPtr(a[s].mGetRefConst(o1));
    EmitInt(a[s].mGetID(o1));
    EmitBool(a[s].mIsOfType(o1, 16));
    EmitBool(a[s].mIsOfType(o1, 13));
    EmitBool(a[s].mIsOfType(o1, 1));
    a[s].mCtorCopy(o3, o1);
    EmitObj(o3, 0x18c);
    a[s].mAssign(o3, o1);
    EmitObj(o3, 0x18c);
    // a[s].mDtor(o3);
    a[s].mDtor(o1);
    a[s].pDtor(o2);
    EmitInt(static_cast<int>(a[s].portalGetTotal() - total0));
}

static void EmitCellState(void* cell)
{
    unsigned char* const c = static_cast<unsigned char*>(cell);
    void* const node = *reinterpret_cast<void**>(c + 0x12c);
    void* const bv = *reinterpret_cast<void**>(c + 0x4c);
    EmitBool(node != nullptr);
    EmitBool(bv != nullptr);
    EmitBool(bv == c + 0x50);
    if (node) {
        unsigned char* const n = static_cast<unsigned char*>(node);
        Emit(n, 0x10);
        Emit(n + 0x14, 0x18);  // +0x14..+0x2b (skip the uninitialised +0x10 and +0x2c)
        Emit(n + 0x30, 0x18);
        EmitBool((n[0x48] & 1) != 0);
    }
    // the parent's own fields, minus the node pointer: count, ring and array words
    Emit(c + 0x130, 4);
    Emit(c + 0x134, 4);
    Emit(c + 0x138, 0x20);  // ring count/capacity/data/read/write + array count/capacity/data
    if (bv) {
        unsigned char* const b = static_cast<unsigned char*>(bv);
        Emit(b, 4);  // the module-independent CellAxisAlignedBoundingBox vtab (both constructors are the original's)
        Emit(b + 4, 4);
        EmitBool(*reinterpret_cast<void**>(b + 0x98) == cell);
        Emit(b + 0x9c, 4);
        Emit(b + 0xa0, 0x3c);
    }
}

static void RunCell(int s)
{
    Seed(o0, sizeof o0);
    Seed(o1, sizeof o1);
    Seed(o2, sizeof o2);
    a[s].cellCtor0(o0);
    EmitCellState(o0);
    EmitInt(a[s].cellGetID(o0));
    EmitBool(a[s].cellIsOfType(o0, 12));
    EmitBool(a[s].cellIsOfType(o0, 1));
    EmitBool(a[s].cellIsOfType(o0, 13));
    // extents come from the node and are copied through x87
    void* const node0 = *reinterpret_cast<void**>(o0 + 0x12c);
    if (node0) {
        std::memcpy(static_cast<unsigned char*>(node0) + 0x30, &sc.box.min, 0x0c);
        std::memcpy(static_cast<unsigned char*>(node0) + 0x3c, &sc.box.max, 0x0c);
    }
    Box ext{};
    a[s].cellGetExtents(o0, &ext);
    Emit(&ext, sizeof ext);

    // copy and assignment keep the source node and bounding volume
    a[s].cellCtorCopy(o1, o0);
    EmitCellState(o1);
    const bool sameNode = *reinterpret_cast<void**>(o1 + 0x12c) == *reinterpret_cast<void**>(o0 + 0x12c);
    const bool sameVolume = *reinterpret_cast<void**>(o1 + 0x4c) == *reinterpret_cast<void**>(o0 + 0x4c);
    EmitBool(sameNode);
    EmitBool(sameVolume);
    a[s].cellCtor0(o2);
    a[s].cellAssign(o2, o0);
    EmitCellState(o2);
    const bool sameNode2 = *reinterpret_cast<void**>(o2 + 0x12c) == *reinterpret_cast<void**>(o0 + 0x12c);
    EmitBool(sameNode2);

    // only the owner is destroyed: copies share its node and volume (as in the original)
    a[s].cellDtor(o0);
    a[s].cellCtor0(o3);
    a[s].cellDtor(o3);
}

static void CheckLocalExport(const char* name)
{
    FARPROC const p = GetProcAddress(mod[0], name);
    if (!p) {
        std::printf("missing local export %s\n", name);
        ++bad;
        return;
    }
    HMODULE owner = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(p), &owner)) {
        ++bad;
        return;
    }
    if (owner != mod[0]) std::printf("export still forwarded: %s\n", name);
    Check(owner == mod[0], "local export");
}

static void RunVtables()
{
    // the implemented vftables are ours, and the new slots are occupied
    CheckLocalExport("??_7AttachmentPoint@Jet@@6B@");
    CheckLocalExport("??_7AttachmentObserver@Jet@@6B@");
    CheckLocalExport("??_7Cell@Jet@@6B@");
    CheckLocalExport("??_7Portal@Jet@@6B@");
    CheckLocalExport("??_7PhysicalPortal@Jet@@6B@");
    CheckLocalExport("??_7VirtualPortal@Jet@@6B@");
    CheckLocalExport("??_7Mirror@Jet@@6B@");
    CheckLocalExport("??_7Function@Jet@@6B@");
    CheckLocalExport("??0AttachmentPoint@Jet@@QAE@XZ");
    CheckLocalExport("??1AttachmentPoint@Jet@@UAE@XZ");
    CheckLocalExport("?GetExtents@Cell@Jet@@QBA?AVAxisAlignedBox@2@XZ");
    CheckLocalExport("?GetTotal@Portal@Jet@@SAIXZ");

    for (int s = 0; s < 2; ++s) {
        Seed(o0, sizeof o0);
        a[s].apCtor0(o0);
        void** const apVt = *reinterpret_cast<void***>(o0);
        Check(apVt[48] == reinterpret_cast<void*>(RawProc(s, "?AddObserver@AttachmentPoint@Jet@@UAAXPAVAttachmentObserver@2@@Z")),
              "AttachmentPoint slot 48");
        Check(apVt[49] == reinterpret_cast<void*>(RawProc(s, "?RemoveObserver@AttachmentPoint@Jet@@UAA_NPAVAttachmentObserver@2@@Z")),
              "AttachmentPoint slot 49");
        Check(apVt[50] == reinterpret_cast<void*>(RawProc(s, "?ClearObservers@AttachmentPoint@Jet@@UAAXXZ")),
              "AttachmentPoint slot 50");
        Check(apVt[51] == reinterpret_cast<void*>(RawProc(s, "?NotifyObservers@AttachmentPoint@Jet@@MAAX_NPAVParticle@2@@Z")),
              "AttachmentPoint slot 51");
        Check(apVt[7] == reinterpret_cast<void*>(RawProc(s, "?GetID@AttachmentPoint@Jet@@UBAHXZ")),
              "AttachmentPoint GetID slot");
    a[s].apDtor(o0);

        Seed(o1, sizeof o1);
        a[s].obsCtor0(o1);
        void** const obsVt2 = *reinterpret_cast<void***>(o1);
        for (int i = 1; i <= 3; ++i) Check(obsVt2[i] != nullptr, "AttachmentObserver observer slots");
        a[s].obsDtor(o1);

        Seed(o0, sizeof o0);
        a[s].portalCtor0(o0);
        void** const pVt = *reinterpret_cast<void***>(o0);
        Check(pVt[48] == reinterpret_cast<void*>(RawProc(s, "?Traverse@Portal@Jet@@UAAXAAVViewPoint@2@ABVMatrix4@2@PAVCamera@2@@Z")),
              "Portal slot 48");
        for (int i = 49; i <= 51; ++i) Check(pVt[i] != nullptr, "Portal new slots");
        Check(pVt[7] == reinterpret_cast<void*>(RawProc(s, "?GetID@Portal@Jet@@UBAHXZ")), "Portal GetID slot");
        a[s].portalDtor(o0);

        Seed(o0, sizeof o0);
        a[s].cellCtor0(o0);
        void** const cVt = *reinterpret_cast<void***>(o0);
        Check(cVt[48] != nullptr, "Cell slot 48");
        Check(cVt[7] == reinterpret_cast<void*>(RawProc(s, "?GetID@Cell@Jet@@UBAHXZ")), "Cell GetID slot");
        a[s].cellDtor(o0);

        const Vec2 zeroSize{0.0f, 0.0f};
        Seed(o0, sizeof o0);
        a[s].ppCtor(o0, o4, zeroSize);
        void** const ppVt = *reinterpret_cast<void***>(o0);
        Check(ppVt[7] == reinterpret_cast<void*>(RawProc(s, "?GetID@PhysicalPortal@Jet@@UBAHXZ")),
              "PhysicalPortal GetID slot");
        for (int i = 48; i <= 51; ++i) Check(ppVt[i] != nullptr, "PhysicalPortal new slots");
        a[s].ppDtor(o0);

        Seed(o0, sizeof o0);
        a[s].vpCtor(o0, o4, zeroSize);
        void** const vpVt = *reinterpret_cast<void***>(o0);
        Check(vpVt[7] == reinterpret_cast<void*>(RawProc(s, "?GetID@VirtualPortal@Jet@@UBAHXZ")),
              "VirtualPortal GetID slot");
        Check(vpVt[51] != nullptr, "VirtualPortal CreateCamera slot");
        a[s].vpDtor(o0);

        Seed(o0, sizeof o0);
        a[s].mCtor(o0, o4, zeroSize);
        void** const mVt = *reinterpret_cast<void***>(o0);
        Check(mVt[7] == reinterpret_cast<void*>(RawProc(s, "?GetID@Mirror@Jet@@UBAHXZ")), "Mirror GetID slot");
        Check(mVt[3] != nullptr && mVt[51] != nullptr, "Mirror Cull/CreateCamera slots");
        a[s].mDtor(o0);
    }
}

static void RunAll(int s)
{
    side = s;
    at = 0;
    addedCalls = removedCalls = shutdownCalls = 0;
    RunAttachment(s);
    RunReferencePoint(s);
    RunLocation(s);
    RunCoronaFunction(s);
    RunPortal(s);
    RunPhysicalPortal(s);
    RunVirtualMirror(s);
    RunCell(s);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u9b <bin dir> [rounds]\n");
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
    DeleteAlloc = reinterpret_cast<void (*)(void*)>(GetProcAddress(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    if (!CreateNode || !pstringCache || !DeleteAlloc) {
        std::printf("no PString cache / Alloc::Delete\n");
        return 2;
    }
    for (int s = 0; s < 2; ++s) LoadApi(s);

    for (int r = 0; r < rounds; ++r) {
        const bool awkward = (r % 7) == 6;
        MakeScenario(awkward);
        RunAll(0);
        const size_t n = at;
        RunAll(1);
        Check(n == at, "snapshot size");
        if (n != at) break;
        CheckBytes(res[0], res[1], n, "scene");
        if (bad) break;
    }

    RunVtables();

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}

