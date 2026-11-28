// Differential test of Render3DCore_7c unit U9b2: BoneModifier, BoneBlender and SkeletonModifier against
// Render3DCore_7c_orig.dll in one process.
//
// The classes are laid out by hand (Render3DCoreBones.h): the buffers are pre-filled with one pattern and every
// non-vtable/non-pointer field is compared. PString is one interned-cache pointer, so equal text gives the same node
// on both sides; the modifier lists (FastDynamicArray chunks) are walked by layout and compared by priority and
// identity. BoneModifier's unexported pure slots are stubs in the original vtable; the test only uses the exported
// methods and the array/state they leave behind.
//
// usage: diff_r3d_u9b2.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

struct PS {
    void* node;
};
static_assert(sizeof(PS) == 4, "PString");

static const size_t kBlenderSize = 0x14;
static const size_t kBoneModifierSize = 0x20;
static const size_t kSkeletonSize = 0x34;
static const size_t kParticleSize = 0x12c;

static HMODULE mod[2];
static unsigned long long checks, bad;
static std::mt19937 rng(0x9b2);

static void* pstringCache;
static void* (*CreateNode)(void*, const unsigned char*);
static void (*DestroyNode)(void*, void*);
static void (*DeleteAlloc)(void*);

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
    default: return FRange(-100.0f, 100.0f);
    }
}

static uint32_t FBits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

static void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        std::printf("MISMATCH %s\n", what);
    }
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

static PS MakeString(const char* text)
{
    PS s{CreateNode(pstringCache, reinterpret_cast<const unsigned char*>(text))};
    return s;
}

static void FreeString(PS& s)
{
    if (s.node) DestroyNode(pstringCache, s.node);
    s.node = nullptr;
}

struct AP {
    // PString
    void(__cdecl* getRoot)(const void*, PS*);
    PS* (__cdecl* boneBlenderName)(const void*);
    void(__cdecl* setRoot)(void*, const PS*);
    float(__cdecl* getPriority)(const void*);
    void(__cdecl* getModifierName)(const void*, PS*);
    void(__cdecl* setComplete)(void*, bool);
    bool(__cdecl* getUpdateFlag)(const void*, unsigned long);
    void(__cdecl* setUpdateFlag)(void*, unsigned long, bool);
    void* (__cdecl* getSkeleton)(void*);
    void(__thiscall* boneModifierCtor)(void*, const PS*, float, const PS*, unsigned long);
    void(__thiscall* boneModifierCtor0)(void*);
    void(__thiscall* boneModifierCopy)(void*, const void*);
    void(__thiscall* boneModifierDtor)(void*);
    void* (__thiscall* boneModifierAssign)(void*, const void*);
    void(__thiscall* blenderCtor)(void*, void*, const PS*, void*);
    void(__thiscall* blenderCopy)(void*, const void*);
    void(__thiscall* blenderDtor)(void*);
    void* (__thiscall* blenderAssign)(void*, const void*);
    bool(__cdecl* blenderAdd)(void*, void*);
    bool(__cdecl* blenderInsertBefore)(void*, void*);
    bool(__cdecl* blenderInsertBeforeOther)(void*, void*, const void*);
    bool(__cdecl* blenderInsertAfter)(void*, void*);
    bool(__cdecl* blenderInsertAfterOther)(void*, void*, const void*);
    bool(__cdecl* blenderRemoveModifier)(void*, const void*);
    bool(__cdecl* blenderRemoveName)(void*, const PS*);
    void(__thiscall* blenderRemoveAll)(void*);
    void* (__cdecl* blenderFirstReplaceable)(void*, float);
    void* (__cdecl* blenderGetBone)(void*);
    void(__cdecl* blenderSetBone)(void*, void*);
    void(__thiscall* skeletonCtor)(void*, void*, float);
    void(__thiscall* skeletonCopy)(void*, const void*);
    void(__thiscall* skeletonDtor)(void*);
    void* (__thiscall* skeletonAssign)(void*, const void*);
    void(__cdecl* skeletonGetRoot)(const void*, PS*);
    int(__cdecl* skeletonNumBones)(const void*);
    void* (__cdecl* skeletonGetBlender)(void*, const PS*);
    bool(__cdecl* skeletonRemoveBlender)(void*, void*, bool);
    unsigned int(__cdecl* skeletonRemoveBlenders)(void*, const void*, bool);
    void(__cdecl* skeletonUnlink)(void*);
    void(__cdecl* skeletonRemoveModifier)(void*, void*, const void*);
    void(__cdecl* skeletonRemoveName)(void*, void*, const PS*);
    void(__cdecl* skeletonRemoveAll)(void*);
    void(__cdecl* skeletonUpdateModifiers)(void*, void*, float);
    void(__cdecl* skeletonUpdate)(void*, void*, float);
    void(__thiscall* particleCtor)(void*);
    void(__thiscall* particleDtor)(void*);
    void(__cdecl* particleSetName)(void*, const PS*);
    void(__cdecl* particleAdd)(void*, void*);
    void(__cdecl* particleGetName)(const void*, PS*);
};

static AP Load(int side)
{
    AP a{};
    a.getRoot = Proc<void(__cdecl*)(const void*, PS*)>(side, "?GetRootBone@BoneModifier@Jet@@UBA?AVPString@2@XZ");
    a.boneBlenderName =
        Proc<PS*(__cdecl*)(const void*)>(side, "?GetBoneName@BoneBlender@Jet@@QBAABVPString@2@XZ");
    a.setRoot = Proc<void(__cdecl*)(void*, const PS*)>(side, "?SetRootBone@BoneModifier@Jet@@UAAXABVPString@2@@Z");
    a.getPriority = nullptr;
    a.getModifierName = nullptr;
    a.setComplete = nullptr;
    a.getUpdateFlag =
        Proc<bool(__cdecl*)(const void*, unsigned long)>(side, "?GetUpdateFlag@BoneModifier@Jet@@UBA_NK@Z");
    a.setUpdateFlag =
        Proc<void(__cdecl*)(void*, unsigned long, bool)>(side, "?SetUpdateFlag@BoneModifier@Jet@@UAAXK_N@Z");
    a.getSkeleton = Proc<void*(__cdecl*)(void*)>(side, "?GetSkeleton@BoneModifier@Jet@@UAAPAVSkeletonModifier@2@XZ");
    a.boneModifierCtor = Proc<void(__thiscall*)(void*, const PS*, float, const PS*, unsigned long)>(
        side, "??0BoneModifier@Jet@@QAE@ABVPString@1@M0K@Z");
    a.boneModifierCtor0 = Proc<void(__thiscall*)(void*)>(side, "??0BoneModifier@Jet@@QAE@XZ");
    a.boneModifierCopy =
        Proc<void(__thiscall*)(void*, const void*)>(side, "??0BoneModifier@Jet@@QAE@ABV01@@Z");
    a.boneModifierDtor = Proc<void(__thiscall*)(void*)>(side, "??1BoneModifier@Jet@@UAE@XZ");
    a.boneModifierAssign =
        Proc<void*(__thiscall*)(void*, const void*)>(side, "??4BoneModifier@Jet@@QAEAAV01@ABV01@@Z");
    a.blenderCtor = Proc<void(__thiscall*)(void*, void*, const PS*, void*)>(
        side, "??0BoneBlender@Jet@@QAE@PAVSkeletonModifier@1@ABVPString@1@PAVParticle@1@@Z");
    a.blenderCopy = Proc<void(__thiscall*)(void*, const void*)>(side, "??0BoneBlender@Jet@@QAE@ABV01@@Z");
    a.blenderDtor = Proc<void(__thiscall*)(void*)>(side, "??1BoneBlender@Jet@@QAE@XZ");
    a.blenderAssign = Proc<void*(__thiscall*)(void*, const void*)>(side, "??4BoneBlender@Jet@@QAEAAV01@ABV01@@Z");
    a.blenderAdd = Proc<bool(__cdecl*)(void*, void*)>(side, "?Add@BoneBlender@Jet@@UAA_NPAVBoneModifier@2@@Z");
    a.blenderInsertBefore =
        Proc<bool(__cdecl*)(void*, void*)>(side, "?InsertBefore@BoneBlender@Jet@@UAA_NPAVBoneModifier@2@@Z");
    a.blenderInsertBeforeOther = Proc<bool(__cdecl*)(void*, void*, const void*)>(
        side, "?InsertBefore@BoneBlender@Jet@@UAA_NPAVBoneModifier@2@PBV32@@Z");
    a.blenderInsertAfter =
        Proc<bool(__cdecl*)(void*, void*)>(side, "?InsertAfter@BoneBlender@Jet@@UAA_NPAVBoneModifier@2@@Z");
    a.blenderInsertAfterOther = Proc<bool(__cdecl*)(void*, void*, const void*)>(
        side, "?InsertAfter@BoneBlender@Jet@@UAA_NPAVBoneModifier@2@PBV32@@Z");
    a.blenderRemoveModifier =
        Proc<bool(__cdecl*)(void*, const void*)>(side, "?Remove@BoneBlender@Jet@@UAA_NPBVModifier@2@@Z");
    a.blenderRemoveName =
        Proc<bool(__cdecl*)(void*, const PS*)>(side, "?Remove@BoneBlender@Jet@@UAA_NABVPString@2@@Z");
    a.blenderRemoveAll = Proc<void(__thiscall*)(void*)>(side, "?RemoveAll@BoneBlender@Jet@@UAEXXZ");
    a.blenderFirstReplaceable = Proc<void*(__cdecl*)(void*, float)>(
        side, "?GetFirstReplaceable@BoneBlender@Jet@@QAAPAVBoneModifier@2@M@Z");
    a.blenderGetBone =
        Proc<void*(__cdecl*)(void*)>(side, "?GetBone@BoneBlender@Jet@@QAAPAVParticle@2@XZ");
    a.blenderSetBone =
        Proc<void(__cdecl*)(void*, void*)>(side, "?SetBone@BoneBlender@Jet@@QAAXPAVParticle@2@@Z");
    a.skeletonCtor =
        Proc<void(__thiscall*)(void*, void*, float)>(side, "??0SkeletonModifier@Jet@@QAE@PAVParticle@1@M@Z");
    a.skeletonCopy = Proc<void(__thiscall*)(void*, const void*)>(side, "??0SkeletonModifier@Jet@@QAE@ABV01@@Z");
    a.skeletonDtor = Proc<void(__thiscall*)(void*)>(side, "??1SkeletonModifier@Jet@@UAE@XZ");
    a.skeletonAssign =
        Proc<void*(__thiscall*)(void*, const void*)>(side, "??4SkeletonModifier@Jet@@QAEAAV01@ABV01@@Z");
    a.skeletonGetRoot =
        Proc<void(__cdecl*)(const void*, PS*)>(side, "?GetRootBone@SkeletonModifier@Jet@@QBA?AVPString@2@XZ");
    a.skeletonNumBones = Proc<int(__cdecl*)(const void*)>(side, "?GetNumBones@SkeletonModifier@Jet@@QBAHXZ");
    a.skeletonGetBlender = Proc<void*(__cdecl*)(void*, const PS*)>(
        side, "?GetBoneBlender@SkeletonModifier@Jet@@IAAPAVBoneBlender@2@ABVPString@2@@Z");
    a.skeletonRemoveBlender = Proc<bool(__cdecl*)(void*, void*, bool)>(
        side, "?RemoveBlender@SkeletonModifier@Jet@@QAA_NPAVBoneBlender@2@_N@Z");
    a.skeletonRemoveBlenders = Proc<unsigned int(__cdecl*)(void*, const void*, bool)>(
        side, "?RemoveBlenders@SkeletonModifier@Jet@@QAAIABV?$FastDynamicArray@PAVBoneBlender@Jet@@$0N@U?$Less@PAVBoneBlender@Jet@@PAV12@@2@@2@_N@Z");
    a.skeletonUnlink =
        Proc<void(__cdecl*)(void*)>(side, "?UnlinkBlenders@SkeletonModifier@Jet@@IAAXXZ");
    a.skeletonRemoveModifier = Proc<void(__cdecl*)(void*, void*, const void*)>(
        side, "?Remove@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResultTable@2@BW4Results@ModifierHost@Jet@@@2@PBVModifier@2@@Z");
    a.skeletonRemoveName = Proc<void(__cdecl*)(void*, void*, const PS*)>(
        side, "?Remove@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResultTable@2@BW4Results@ModifierHost@Jet@@@2@ABVPString@2@@Z");
    a.skeletonRemoveAll = Proc<void(__cdecl*)(void*)>(side, "?RemoveAll@SkeletonModifier@Jet@@UAAXXZ");
    a.skeletonUpdateModifiers = Proc<void(__cdecl*)(void*, void*, float)>(
        side, "?UpdateModifiers@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResultTable@2@BW4Results@ModifierHost@Jet@@@2@M@Z");
    a.skeletonUpdate = Proc<void(__cdecl*)(void*, void*, float)>(
        side, "?Update@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@Modifier@Jet@@2QBUResultTable@2@BW4Results@Modifier@Jet@@@2@M@Z");
    a.particleCtor = Proc<void(__thiscall*)(void*)>(side, "??0Particle@Jet@@QAE@XZ");
    a.particleDtor = Proc<void(__thiscall*)(void*)>(side, "??1Particle@Jet@@UAE@XZ");
    a.particleSetName =
        Proc<void(__cdecl*)(void*, const PS*)>(side, "?SetName@Particle@Jet@@UAAXABVPString@2@@Z");
    a.particleAdd = Proc<void(__cdecl*)(void*, void*)>(side, "?Add@Particle@Jet@@UAAXPAV12@@Z");
    a.particleGetName =
        Proc<void(__cdecl*)(const void*, PS*)>(side, "?GetName@Particle@Jet@@QBAABVPString@2@XZ");
    return a;
}

// FastDynamicArray<T*,13>: {Chunk* head}; Chunk {next, prev, count, T data[13]}.
struct Chunk {
    Chunk* next;
    Chunk* prev;
    uint32_t count;
    void* data[13];
};

typedef std::vector<void*> Ptrs;

static Ptrs ArrayItems(void* head)
{
    Ptrs out;
    if (!head) return out;
    Chunk* c = static_cast<Chunk*>(head);
    do {
        for (uint32_t i = 0; i < c->count; ++i) out.push_back(c->data[i]);
        c = c->next;
    } while (c != head);
    return out;
}

static std::vector<uint32_t> Priorities(const AP& api, const Ptrs& items)
{
    std::vector<uint32_t> out;
    for (void* p : items) out.push_back(FBits(api.getPriority(p)));
    return out;
}

static void CheckPriorities(const AP& a0, const AP& a1, const Ptrs& p0, const Ptrs& p1, const char* what)
{
    const bool same = p0.size() == p1.size() && Priorities(a0, p0) == Priorities(a1, p1);
    if (!same) {
        std::printf("%s: ours %zu [", what, p0.size());
        for (uint32_t v : Priorities(a0, p0)) std::printf(" %08x", v);
        std::printf(" ] orig %zu [", p1.size());
        for (uint32_t v : Priorities(a1, p1)) std::printf(" %08x", v);
        std::printf(" ]\n");
    }
    Check(same, what);
}

// SkeletonModifier::Update overrides the secondary Modifier base: the vtable slot lives in the Modifier subobject
// at +0x18 and takes a Modifier*, exactly as the original's exported body does. Call it through the vtable.
static void CallSkeletonUpdate(void* skeleton, int* result, float dt)
{
    char* modifier = static_cast<char*>(skeleton) + 0x18;
    void** vt = *reinterpret_cast<void***>(modifier);
    auto fn = reinterpret_cast<void(__cdecl*)(void*, void*, float)>(vt[8]);
    fn(modifier, result, dt);
}

static void* FnAddr(const void* fn) { return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(fn)); }

// The vtable order is part of the ABI. For the slots whose export is local, each side's slot must be that side's
// export; the original's own vtable is the reference for the overload reversal (Remove(name) at 5,
// Remove(modifier) at 6; InsertBefore(m, before) at 1, InsertBefore(m) at 2, and the same pair for InsertAfter).
static void CheckBlenderVtable(const AP& a, void* blender, const char* what)
{
    void** vt = *reinterpret_cast<void***>(blender);
    Check(vt[0] == FnAddr(a.blenderAdd), what);
    Check(vt[1] == FnAddr(a.blenderInsertBeforeOther), what);
    Check(vt[2] == FnAddr(a.blenderInsertBefore), what);
    Check(vt[3] == FnAddr(a.blenderInsertAfterOther), what);
    Check(vt[4] == FnAddr(a.blenderInsertAfter), what);
    Check(vt[5] == FnAddr(a.blenderRemoveName), what);
    Check(vt[6] == FnAddr(a.blenderRemoveModifier), what);
    Check(vt[7] == FnAddr(a.blenderRemoveAll), what);
}

static void CheckBoneModifierVtable(const AP& a, void* modifier, const char* what)
{
    void** vt = *reinterpret_cast<void***>(modifier);
    Check(vt[10] == FnAddr(a.getSkeleton), what);
    Check(vt[11] == FnAddr(a.getRoot), what);
    Check(vt[12] == FnAddr(a.setRoot), what);
    Check(vt[21] == FnAddr(a.getUpdateFlag), what);
    Check(vt[22] == FnAddr(a.setUpdateFlag), what);
}

static void CheckSkeletonVtable(const AP& a, void* skeleton, const char* what)
{
    void** list = *reinterpret_cast<void***>(skeleton);
    Check(list[7] == FnAddr(a.skeletonRemoveName), what);
    Check(list[8] == FnAddr(a.skeletonRemoveModifier), what);
    Check(list[9] == FnAddr(a.skeletonRemoveAll), what);
    Check(list[10] == FnAddr(a.skeletonUpdateModifiers), what);
}

static bool IndexOf(const Ptrs& items, void* p)
{
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i] == p) return true;
    return false;
}

static void Prefill(void* p, size_t n)
{
    std::memset(p, 0x7f, n);
}

// Object field comparison helpers. Vptrs and the FastDynamicArray chunk pointer are per side; everything else must
// match. Name nodes are shared by the interning cache, so pointers are comparable when the text is equal.
static void CheckBoneModifierFields(const void* a, const void* b, const char* what)
{
    const bool ok = std::memcmp(static_cast<const char*>(a) + 4, static_cast<const char*>(b) + 4, 0x1c) == 0;
    if (!ok) {
        std::printf("%s: ours", what);
        for (int i = 4; i < 0x20; i += 4) std::printf(" %08x", *reinterpret_cast<const uint32_t*>(static_cast<const char*>(a) + i));
        std::printf(" orig");
        for (int i = 4; i < 0x20; i += 4) std::printf(" %08x", *reinterpret_cast<const uint32_t*>(static_cast<const char*>(b) + i));
        std::printf("\n");
    }
    Check(ok, what);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u9b2 <bin dir> [rounds]\n");
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

    HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!ks) ks = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!ks) {
        std::printf("cannot load KernelStandard_7c.dll\n");
        return 2;
    }
    CreateNode = reinterpret_cast<void* (*)(void*, const unsigned char*)>(
        GetProcAddress(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    DestroyNode = reinterpret_cast<void (*)(void*, void*)>(
        GetProcAddress(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z"));
    pstringCache = *reinterpret_cast<void**>(GetProcAddress(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    DeleteAlloc = reinterpret_cast<void (*)(void*)>(GetProcAddress(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    if (!CreateNode || !DestroyNode || !pstringCache || !DeleteAlloc) {
        std::printf("KernelStandard_7c PString/Alloc imports missing\n");
        return 2;
    }

    AP api[2] = {Load(0), Load(1)};
    // Modifier's own methods live in RenderSystem_7c (the same module serves both Render3DCore sides).
    {
        HMODULE rs = GetModuleHandleA("RenderSystem_7c.dll");
        if (!rs) rs = LoadLibraryA((dir + "\\RenderSystem_7c.dll").c_str());
        if (!rs) {
            std::printf("cannot load RenderSystem_7c.dll\n");
            return 2;
        }
        auto priority = reinterpret_cast<float(__cdecl*)(const void*)>(
            GetProcAddress(rs, "?GetPriority@Modifier@Jet@@UBAMXZ"));
        auto name = reinterpret_cast<void(__cdecl*)(const void*, PS*)>(
            GetProcAddress(rs, "?GetName@Modifier@Jet@@UBA?AVPString@2@XZ"));
        auto complete =
            reinterpret_cast<void(__cdecl*)(void*, bool)>(GetProcAddress(rs, "?SetComplete@Modifier@Jet@@UAAX_N@Z"));
        if (!priority || !name || !complete) {
            std::printf("RenderSystem_7c Modifier imports missing\n");
            return 2;
        }
        for (AP& a : api) {
            a.getPriority = priority;
            a.getModifierName = name;
            a.setComplete = complete;
        }
    }

    PS boneName = MakeString("b.test");
    PS rootName = MakeString("root");
    PS otherName = MakeString("other");
    PS missingName = MakeString("missing");
    PS modifierName = MakeString("modifier");

    alignas(16) unsigned char mods[2][8][kBoneModifierSize];
    alignas(16) unsigned char blenders[2][3][kBlenderSize];
    alignas(16) unsigned char skeletons[2][2][kSkeletonSize];

    for (int round = 0; round < rounds; ++round) {
        const bool awkward = (round % 7) == 0;

        // ---- default and full BoneModifier construction ------------------------------------------------------
        for (int s = 0; s < 2; ++s) {
            Prefill(mods[s][0], kBoneModifierSize);
            api[s].boneModifierCtor0(mods[s][0]);
        }
        CheckBoneModifierFields(mods[0][0], mods[1][0], "BoneModifier default ctor");

        const float priority = awkward ? Awkward() : FRange(-5.0f, 5.0f);
        const unsigned long flags = static_cast<unsigned long>(Pick(0, 7));
        for (int s = 0; s < 2; ++s) {
            Prefill(mods[s][0], kBoneModifierSize);
            api[s].boneModifierCtor(mods[s][0], &modifierName, priority, &rootName, flags);
        }
        CheckBoneModifierFields(mods[0][0], mods[1][0], "BoneModifier ctor");
        for (int s = 0; s < 2; ++s) CheckBoneModifierVtable(api[s], mods[s][0], "BoneModifier vtable");
        Check(api[0].getPriority(mods[0][0]) == api[1].getPriority(mods[1][0]) ||
                  (FBits(api[0].getPriority(mods[0][0])) == FBits(api[1].getPriority(mods[1][0]))),
              "BoneModifier GetPriority");
        PS roots[2]{};
        api[0].getRoot(&mods[0][0], &roots[0]);
        api[1].getRoot(&mods[1][0], &roots[1]);
        Check(roots[0].node == roots[1].node, "BoneModifier GetRootBone");
        Check(api[0].getSkeleton(mods[0][0]) == api[1].getSkeleton(mods[1][0]), "BoneModifier GetSkeleton null");
        for (unsigned long f = 1; f <= 4; f <<= 1)
            Check(api[0].getUpdateFlag(mods[0][0], f) == api[1].getUpdateFlag(mods[1][0], f),
                  "BoneModifier GetUpdateFlag");
        api[0].setUpdateFlag(mods[0][0], 1, true);
        api[1].setUpdateFlag(mods[1][0], 1, true);
        api[0].setUpdateFlag(mods[0][0], 4, false);
        api[1].setUpdateFlag(mods[1][0], 4, false);
        CheckBoneModifierFields(mods[0][0], mods[1][0], "BoneModifier SetUpdateFlag");
        api[0].setRoot(mods[0][0], &boneName);
        api[1].setRoot(mods[1][0], &boneName);
        CheckBoneModifierFields(mods[0][0], mods[1][0], "BoneModifier SetRootBone");

        // copy and assignment
        for (int s = 0; s < 2; ++s) {
            Prefill(mods[s][1], kBoneModifierSize);
            api[s].boneModifierCopy(mods[s][1], mods[s][0]);
        }
        CheckBoneModifierFields(mods[0][1], mods[1][1], "BoneModifier copy ctor");
        for (int s = 0; s < 2; ++s) {
            Prefill(mods[s][2], kBoneModifierSize);
            api[s].boneModifierCtor(mods[s][2], &missingName, 1.0f, &otherName, 3);
            api[s].boneModifierAssign(mods[s][2], mods[s][0]);
        }
        CheckBoneModifierFields(mods[0][2], mods[1][2], "BoneModifier assignment");
        for (int s = 0; s < 2; ++s)
            for (int i = 0; i < 3; ++i) api[s].boneModifierDtor(mods[s][i]);

        // ---- BoneBlender: sorted modifier list ---------------------------------------------------------------
        for (int s = 0; s < 2; ++s) {
            Prefill(blenders[s][0], kBlenderSize);
            api[s].blenderCtor(blenders[s][0], nullptr, &boneName, nullptr);
        }
        Check(api[0].boneBlenderName(blenders[0][0])->node == api[1].boneBlenderName(blenders[1][0])->node,
              "BoneBlender ctor name");
        Check(*reinterpret_cast<void**>(blenders[0][0] + 4) == nullptr &&
                  *reinterpret_cast<void**>(blenders[1][0] + 4) == nullptr,
              "BoneBlender ctor host");
        Check(*reinterpret_cast<void**>(blenders[0][0] + 0x10) == nullptr &&
                  *reinterpret_cast<void**>(blenders[1][0] + 0x10) == nullptr,
              "BoneBlender ctor empty list");
        for (int s = 0; s < 2; ++s) CheckBlenderVtable(api[s], blenders[s][0], "BoneBlender vtable");

        const int count = Pick(1, 6);
        for (int i = 0; i < count; ++i) {
            const float p = (i % 3 == 0) ? priority : (awkward && i == 1 ? Awkward() : FRange(-5.0f, 5.0f));
            const unsigned long f = (i % 2) ? 1ul : 0ul;
            for (int s = 0; s < 2; ++s) {
                Prefill(mods[s][i], kBoneModifierSize);
                api[s].boneModifierCtor(mods[s][i], &modifierName, p, &rootName, f);
            }
        }
        // A different name for the string-remove test.
        for (int s = 0; s < 2; ++s) api[s].setComplete(mods[s][0], false);
        PS modNameCopy = MakeString("modifier");

        for (int i = 0; i < count; ++i) {
            bool r[2];
            switch (i % 5) {
            case 0:
                r[0] = api[0].blenderAdd(blenders[0][0], mods[0][i]);
                r[1] = api[1].blenderAdd(blenders[1][0], mods[1][i]);
                Check(r[0] == r[1], "BoneBlender Add");
                break;
            case 1:
                r[0] = api[0].blenderInsertBefore(blenders[0][0], mods[0][i]);
                r[1] = api[1].blenderInsertBefore(blenders[1][0], mods[1][i]);
                Check(r[0] == r[1], "BoneBlender InsertBefore");
                break;
            case 2:
                r[0] = api[0].blenderInsertAfter(blenders[0][0], mods[0][i]);
                r[1] = api[1].blenderInsertAfter(blenders[1][0], mods[1][i]);
                Check(r[0] == r[1], "BoneBlender InsertAfter");
                break;
            case 3:
                if (i > 0) {
                    r[0] = api[0].blenderInsertBeforeOther(blenders[0][0], mods[0][i], mods[0][i - 1]);
                    r[1] = api[1].blenderInsertBeforeOther(blenders[1][0], mods[1][i], mods[1][i - 1]);
                    Check(r[0] == r[1], "BoneBlender InsertBefore(other)");
                } else {
                    r[0] = api[0].blenderInsertBefore(blenders[0][0], mods[0][i]);
                    r[1] = api[1].blenderInsertBefore(blenders[1][0], mods[1][i]);
                    Check(r[0] == r[1], "BoneBlender InsertBefore");
                }
                break;
            default:
                if (i > 0) {
                    r[0] = api[0].blenderInsertAfterOther(blenders[0][0], mods[0][i], mods[0][i - 1]);
                    r[1] = api[1].blenderInsertAfterOther(blenders[1][0], mods[1][i], mods[1][i - 1]);
                    Check(r[0] == r[1], "BoneBlender InsertAfter(other)");
                } else {
                    r[0] = api[0].blenderInsertAfter(blenders[0][0], mods[0][i]);
                    r[1] = api[1].blenderInsertAfter(blenders[1][0], mods[1][i]);
                    Check(r[0] == r[1], "BoneBlender InsertAfter");
                }
                break;
            }
            Ptrs order0 = ArrayItems(*reinterpret_cast<void**>(blenders[0][0] + 0x10));
            Ptrs order1 = ArrayItems(*reinterpret_cast<void**>(blenders[1][0] + 0x10));
            CheckPriorities(api[0], api[1], order0, order1, "BoneBlender list order");
        }

        Check(api[0].blenderFirstReplaceable(blenders[0][0], priority) != nullptr ||
                  api[1].blenderFirstReplaceable(blenders[1][0], priority) == nullptr ||
                  FBits(api[0].getPriority(mods[0][0])) != FBits(priority),
              "BoneBlender GetFirstReplaceable presence");

        {
            Ptrs o0 = ArrayItems(*reinterpret_cast<void**>(blenders[0][0] + 0x10));
            Ptrs o1 = ArrayItems(*reinterpret_cast<void**>(blenders[1][0] + 0x10));
            if (!o0.empty() && !o1.empty()) {
                const bool r0 = api[0].blenderRemoveModifier(blenders[0][0], mods[0][count - 1]);
                const bool r1 = api[1].blenderRemoveModifier(blenders[1][0], mods[1][count - 1]);
                Check(r0 == r1, "BoneBlender Remove(modifier)");
                Ptrs n0 = ArrayItems(*reinterpret_cast<void**>(blenders[0][0] + 0x10));
                Ptrs n1 = ArrayItems(*reinterpret_cast<void**>(blenders[1][0] + 0x10));
                CheckPriorities(api[0], api[1], n0, n1, "BoneBlender list after Remove(modifier)");
            }
            const bool a0 = api[0].blenderRemoveModifier(blenders[0][0], mods[0][0]);
            const bool a1 = api[1].blenderRemoveModifier(blenders[1][0], mods[1][0]);
            Check(a0 == a1 || !a0, "BoneBlender Remove(absent)");
            const bool s0 = api[0].blenderRemoveName(blenders[0][0], &missingName);
            const bool s1 = api[1].blenderRemoveName(blenders[1][0], &missingName);
            Check(s0 == s1, "BoneBlender Remove(missing name)");
        }
        FreeString(modNameCopy);

        // copy / assignment keep the name and the list order
        for (int s = 0; s < 2; ++s) {
            Prefill(blenders[s][1], kBlenderSize);
            api[s].blenderCopy(blenders[s][1], blenders[s][0]);
        }
        Check(api[0].boneBlenderName(blenders[0][1])->node == api[1].boneBlenderName(blenders[1][1])->node,
              "BoneBlender copy name");
        CheckPriorities(api[0], api[1], ArrayItems(*reinterpret_cast<void**>(blenders[0][1] + 0x10)),
                        ArrayItems(*reinterpret_cast<void**>(blenders[1][1] + 0x10)), "BoneBlender copy list");
        for (int s = 0; s < 2; ++s) {
            Prefill(blenders[s][2], kBlenderSize);
            api[s].blenderCtor(blenders[s][2], nullptr, &otherName, nullptr);
            api[s].blenderAssign(blenders[s][2], blenders[s][0]);
        }
        Check(api[0].boneBlenderName(blenders[0][2])->node == api[1].boneBlenderName(blenders[1][2])->node,
              "BoneBlender assign name");
        CheckPriorities(api[0], api[1], ArrayItems(*reinterpret_cast<void**>(blenders[0][2] + 0x10)),
                        ArrayItems(*reinterpret_cast<void**>(blenders[1][2] + 0x10)), "BoneBlender assign list");

        // GetBone/SetBone
        void* bone0 = reinterpret_cast<void*>(0x1234);
        void* bone1 = reinterpret_cast<void*>(0x5678);
        api[0].blenderSetBone(blenders[0][2], bone0);
        api[1].blenderSetBone(blenders[1][2], bone1);
        void* got0 = api[0].blenderGetBone(blenders[0][2]);
        void* got1 = api[1].blenderGetBone(blenders[1][2]);
        Check(got0 == bone0 && got1 == bone1, "BoneBlender GetBone/SetBone");

        for (int s = 0; s < 2; ++s) {
            api[s].blenderRemoveAll(blenders[s][0]);
            api[s].blenderRemoveAll(blenders[s][1]);
            api[s].blenderRemoveAll(blenders[s][2]);
        }
        Check(ArrayItems(*reinterpret_cast<void**>(blenders[0][0] + 0x10)).empty() &&
                  ArrayItems(*reinterpret_cast<void**>(blenders[1][0] + 0x10)).empty(),
              "BoneBlender RemoveAll");
        for (int s = 0; s < 2; ++s) {
            for (int i = 0; i < count; ++i) api[s].boneModifierDtor(mods[s][i]);
            for (int i = 0; i < 3; ++i) api[s].blenderDtor(blenders[s][i]);
        }

        // ---- SkeletonModifier: state, wrappers and an empty blender list --------------------------------------
        for (int s = 0; s < 2; ++s) {
            Prefill(skeletons[s][0], kSkeletonSize);
            api[s].skeletonCtor(skeletons[s][0], nullptr, priority);
        }
        {
            PS names[2]{};
            api[0].getModifierName(skeletons[0][0] + 0x18, &names[0]);
            api[1].getModifierName(skeletons[1][0] + 0x18, &names[1]);
            Check(names[0].node == names[1].node, "SkeletonModifier name");
            Check(api[0].getPriority(skeletons[0][0] + 0x18) == api[1].getPriority(skeletons[1][0] + 0x18) ||
                      (FBits(api[0].getPriority(skeletons[0][0] + 0x18)) ==
                       FBits(api[1].getPriority(skeletons[1][0] + 0x18))),
                  "SkeletonModifier priority");
            Check(*reinterpret_cast<void**>(skeletons[0][0] + 0x1c) == nullptr &&
                      *reinterpret_cast<void**>(skeletons[1][0] + 0x1c) == nullptr,
                  "SkeletonModifier host");
            Check(*reinterpret_cast<uint32_t*>(skeletons[0][0] + 0xc) == 0 &&
                      *reinterpret_cast<uint32_t*>(skeletons[1][0] + 0xc) == 0,
                  "SkeletonModifier list empty");
        }

        for (int s = 0; s < 2; ++s) CheckSkeletonVtable(api[s], skeletons[s][0], "SkeletonModifier vtable");
        Check(api[0].skeletonNumBones(skeletons[0][0]) == api[1].skeletonNumBones(skeletons[1][0]),
              "SkeletonModifier GetNumBones empty");
        {
            PS r0{}, r1{};
            api[0].skeletonGetRoot(skeletons[0][0], &r0);
            api[1].skeletonGetRoot(skeletons[1][0], &r1);
            Check(r0.node == r1.node && r0.node == nullptr, "SkeletonModifier GetRootBone empty");
            Check(api[0].skeletonGetBlender(skeletons[0][0], &boneName) == nullptr &&
                      api[1].skeletonGetBlender(skeletons[1][0], &boneName) == nullptr,
                  "SkeletonModifier GetBoneBlender empty");
        }

        // Remove of a standalone modifier (not in the list) and the two result wrappers.
        for (int s = 0; s < 2; ++s) {
            Prefill(mods[s][0], kBoneModifierSize);
            api[s].boneModifierCtor(mods[s][0], &modifierName, 0.5f, &rootName, 0);
        }
        {
            int r0 = -1, r1 = -1;
            api[0].skeletonRemoveModifier(skeletons[0][0], &r0, mods[0][0]);
            api[1].skeletonRemoveModifier(skeletons[1][0], &r1, mods[1][0]);
            Check(r0 == r1, "SkeletonModifier Remove(modifier) code");
            int s0 = -1, s1 = -1;
            api[0].skeletonRemoveName(skeletons[0][0], &s0, &modifierName);
            api[1].skeletonRemoveName(skeletons[1][0], &s1, &modifierName);
            Check(s0 == s1, "SkeletonModifier Remove(name) code");
            int u0 = -1, u1 = -1;
            api[0].skeletonUpdateModifiers(skeletons[0][0], &u0, 0.25f);
            api[1].skeletonUpdateModifiers(skeletons[1][0], &u1, 0.25f);
            Check(u0 == u1, "SkeletonModifier UpdateModifiers code");
            int v0 = -1, v1 = -1;
            CallSkeletonUpdate(skeletons[0][0], &v0, 0.25f);
            CallSkeletonUpdate(skeletons[1][0], &v1, 0.25f);
            Check(v0 == v1, "SkeletonModifier Update code");
            api[0].setComplete(skeletons[0][0], true);
            api[1].setComplete(skeletons[1][0], true);
            int w0 = -1, w1 = -1;
            CallSkeletonUpdate(skeletons[0][0], &w0, 0.25f);
            CallSkeletonUpdate(skeletons[1][0], &w1, 0.25f);
            Check(w0 == w1, "SkeletonModifier Update complete code");
            api[0].skeletonRemoveAll(skeletons[0][0]);
            api[1].skeletonRemoveAll(skeletons[1][0]);
        }
        // copy and assignment of the empty skeleton
        for (int s = 0; s < 2; ++s) {
            Prefill(skeletons[s][1], kSkeletonSize);
            api[s].skeletonCopy(skeletons[s][1], skeletons[s][0]);
            api[s].skeletonAssign(skeletons[s][1], skeletons[s][0]);
        }
        Check(api[0].skeletonNumBones(skeletons[0][1]) == api[1].skeletonNumBones(skeletons[1][1]),
              "SkeletonModifier copy GetNumBones");
        for (int s = 0; s < 2; ++s) {
            api[s].boneModifierDtor(mods[s][0]);
            api[s].skeletonDtor(skeletons[s][0]);
            api[s].skeletonDtor(skeletons[s][1]);
        }

        // ---- SkeletonModifier: bones from a particle tree ----------------------------------------------------
        if ((round % 3) == 0) {
            alignas(16) unsigned char rootsBuf[2][kParticleSize];
            alignas(16) unsigned char boneBuf[2][3][kParticleSize];
            static const char* boneText[3] = {"b.one", "b.two", "b.three"};
            PS boneNames[2][3];
            for (int s = 0; s < 2; ++s) {
                Prefill(rootsBuf[s], kParticleSize);
                api[s].particleCtor(rootsBuf[s]);
                Prefill(&boneBuf[s][0][0], kParticleSize);
                api[s].particleCtor(boneBuf[s][0]);
                Prefill(&boneBuf[s][1][0], kParticleSize);
                api[s].particleCtor(boneBuf[s][1]);
                Prefill(&boneBuf[s][2][0], kParticleSize);
                api[s].particleCtor(boneBuf[s][2]);
                for (int i = 0; i < 3; ++i) {
                    boneNames[s][i] = MakeString(boneText[i]);
                    api[s].particleSetName(boneBuf[s][i], &boneNames[s][i]);
                    api[s].particleAdd(rootsBuf[s], boneBuf[s][i]);
                }
            }
            for (int s = 0; s < 2; ++s) {
                Prefill(skeletons[s][0], kSkeletonSize);
                api[s].skeletonCtor(skeletons[s][0], rootsBuf[s], priority);
            }
            const int n0 = api[0].skeletonNumBones(skeletons[0][0]);
            const int n1 = api[1].skeletonNumBones(skeletons[1][0]);
            if (n0 != n1) std::printf("bone count ours %d orig %d\n", n0, n1);
            Check(n0 == n1 && n0 == 3, "SkeletonModifier particle-tree bone count");
            PS r0{}, r1{};
            api[0].skeletonGetRoot(skeletons[0][0], &r0);
            api[1].skeletonGetRoot(skeletons[1][0], &r1);
            Check(r0.node == r1.node, "SkeletonModifier particle-tree root bone");
            Check(r0.node != nullptr && *reinterpret_cast<uint32_t*>(static_cast<char*>(r0.node) + 8) == 5,
                  "SkeletonModifier root bone length");
            void* b0 = api[0].skeletonGetBlender(skeletons[0][0], &boneNames[0][1]);
            void* b1 = api[1].skeletonGetBlender(skeletons[1][0], &boneNames[1][1]);
            Check((b0 == nullptr) == (b1 == nullptr), "SkeletonModifier GetBoneBlender hit");
            if (b0 && b1)
                Check(*reinterpret_cast<void**>(static_cast<char*>(b0) + 8) ==
                          *reinterpret_cast<void**>(static_cast<char*>(b1) + 8),
                      "SkeletonModifier blender name");
            // Update walks the blenders through the original's UpdateBoneModifiers thunk (no modifiers -> true).
            int u0 = -1, u1 = -1;
            CallSkeletonUpdate(skeletons[0][0], &u0, 0.5f);
            CallSkeletonUpdate(skeletons[1][0], &u1, 0.5f);
            Check(u0 == u1, "SkeletonModifier particle-tree Update code");
            // Unlink and remove the first blender.
            api[0].skeletonUnlink(skeletons[0][0]);
            api[1].skeletonUnlink(skeletons[1][0]);
            Check(*reinterpret_cast<void**>(static_cast<char*>(b0) + 0xc) == nullptr &&
                      *reinterpret_cast<void**>(static_cast<char*>(b1) + 0xc) == nullptr,
                  "SkeletonModifier UnlinkBlenders");
            const bool rm0 = api[0].skeletonRemoveBlender(skeletons[0][0], b0, true);
            const bool rm1 = api[1].skeletonRemoveBlender(skeletons[1][0], b1, true);
            Check(rm0 == rm1 && rm0, "SkeletonModifier RemoveBlender");
            Check(api[0].skeletonNumBones(skeletons[0][0]) == api[1].skeletonNumBones(skeletons[1][0]),
                  "SkeletonModifier bones after RemoveBlender");
            for (int s = 0; s < 2; ++s) {
                api[s].skeletonDtor(skeletons[s][0]);
                for (int i = 0; i < 3; ++i) api[s].particleDtor(boneBuf[s][i]);
                api[s].particleDtor(rootsBuf[s]);
            }
            for (int s = 0; s < 2; ++s)
                for (int i = 0; i < 3; ++i) FreeString(boneNames[s][i]);
        }
    }

    FreeString(boneName);
    FreeString(rootName);
    FreeString(otherName);
    FreeString(missingName);
    FreeString(modifierName);

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
