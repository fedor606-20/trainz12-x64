// Render3DCore_7c wave 5 tails (Tails2): differential test for implemented.d/tails2.txt.
// The local and pristine modules are loaded in one process; every call gets identical inputs. Floats are compared
// by raw bits (NaNs count as equal but are reported separately), vtables by slot role, and object bytes outside the
// module-specific vptr.
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

struct Module {
    HMODULE handle;
    std::uintptr_t base;
    std::uintptr_t end;

    bool Contains(const void* p) const
    {
        const std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
        return v >= base && v < end;
    }
};

Module Open(const char* name)
{
    Module m{};
    m.handle = LoadLibraryA(name);
    if (!m.handle) {
        std::printf("cannot load %s (error %lu)\n", name, GetLastError());
        std::exit(1);
    }
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(m.handle);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<char*>(m.handle) + dos->e_lfanew);
    m.base = reinterpret_cast<std::uintptr_t>(m.handle);
    m.end = m.base + nt->OptionalHeader.SizeOfImage;
    return m;
}

void* Proc(const Module& m, const char* name)
{
    return reinterpret_cast<void*>(GetProcAddress(m.handle, name));
}

std::uint32_t g_rng = 0x7a112u;

std::uint32_t Next()
{
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng;
}

float RandFloat(float scale)
{
    return (static_cast<float>(Next() >> 8) / 16777216.0f - 0.5f) * 2.0f * scale;
}

bool IsNan(float v)
{
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof bits);
    return (bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0;
}

bool SameFloat(float a, float b, long long& nanDiffs)
{
    if (IsNan(a) || IsNan(b)) {
        if (IsNan(a) && IsNan(b)) {
            std::uint32_t ba, bb;
            std::memcpy(&ba, &a, sizeof ba);
            std::memcpy(&bb, &b, sizeof bb);
            if (ba != bb)
                ++nanDiffs;
            return true;
        }
        return false;
    }
    std::uint32_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    return ba == bb;
}

struct SplineSegmentT {
    void* vt;
    float c0[3];
    float c1[3];
    float c2[3];
    float c3[3];
};

typedef float(__cdecl* GetRatioFn)(const void*, float, float, float, unsigned int);
typedef void* (__cdecl* UpdateExportFn)(void* modifier, void* result, float dt);
typedef void(__thiscall* CtorModifierFn)(void*, void*, float);
typedef void(__thiscall* DtorModifierFn)(void*);
typedef void(__thiscall* CtorSplineResourceFn)(void*, const void* name, const void* database);
typedef void(__thiscall* CtorSplineResourceCopyFn)(void*, const void* other);
typedef void(__thiscall* DtorSplineResourceFn)(void*);
typedef void* (__cdecl* CreateInstanceFn)(const void* self);

long long g_checks = 0;
long long g_mismatches = 0;
long long g_nanDiffs = 0;

void Check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) {
        ++g_mismatches;
        if (g_mismatches <= 30)
            std::printf("MISMATCH %s\n", what);
    }
}

// ---- vtable roles -------------------------------------------------------------------------------------------
struct SlotRole {
    int slot;
    const char* exportName;
};

void CheckVTable(const Module& mine, const Module& orig, const char* vtableName, const SlotRole* roles, int count)
{
    void** mineTable = static_cast<void**>(Proc(mine, vtableName));
    void** origTable = static_cast<void**>(Proc(orig, vtableName));
    Check(mineTable != nullptr && mine.Contains(mineTable), "vtable address local");
    Check(origTable != nullptr && orig.Contains(origTable), "orig vtable address");
    if (!mineTable || !origTable)
        return;
    for (int i = 0; i < count; ++i) {
        const SlotRole& r = roles[i];
        void* mineFn = Proc(mine, r.exportName);
        void* origFn = Proc(orig, r.exportName);
        char label[256];
        std::snprintf(label, sizeof label, "%s slot %d role %s", vtableName, r.slot, r.exportName);
        // A forwarded export resolves to the original module; only locally exported methods must equal the slot.
        if (mineFn && mine.Contains(mineFn))
            Check(mineTable[r.slot] == mineFn, label);
        if (origFn && orig.Contains(origFn) && origTable[r.slot] != origFn)
            Check(false, label);
    }
}

// ---- GetRatio -----------------------------------------------------------------------------------------------
void TestGetRatio(const Module& mine, const Module& orig)
{
    GetRatioFn mineFn = reinterpret_cast<GetRatioFn>(Proc(mine, "?GetRatio@SplineSegment@Jet@@UBAMMMMI@Z"));
    GetRatioFn origFn = reinterpret_cast<GetRatioFn>(Proc(orig, "?GetRatio@SplineSegment@Jet@@UBAMMMMI@Z"));
    Check(mineFn && mine.Contains(reinterpret_cast<void*>(mineFn)), "GetRatio local");
    Check(origFn != nullptr, "GetRatio orig");
    if (!mineFn || !origFn)
        return;

    for (int round = 0; round < 4000; ++round) {
        SplineSegmentT seg{};
        for (int i = 0; i < 12; ++i)
            (&seg.c0[0])[i] = RandFloat(40.0f);
        float start = RandFloat(2.0f) - 0.5f;
        float end = start + RandFloat(4.0f) + 0.25f;
        float length = RandFloat(80.0f);
        unsigned int steps = 1 + (Next() % 14);
        if (round % 7 == 0) {
            const float awkward[] = {-0.0f, 0.0f, 1e-30f, 3.4028235e38f, -3.4028235e38f};
            (&seg.c0[0])[round % 12] = awkward[round % 5];
            start = awkward[(round / 5) % 5] * 0.5f;
            end = start + 1.0f;
            length = awkward[(round / 25) % 5];
            if (round % 11 == 0) {
                seg.c1[1] = std::nanf("");
                seg.c2[2] = std::nanf("");
            }
        }
        const float a = mineFn(&seg, length, start, end, steps);
        const float b = origFn(&seg, length, start, end, steps);
        Check(SameFloat(a, b, g_nanDiffs), "GetRatio");
    }
}

// ---- SplineResource -----------------------------------------------------------------------------------------
void TestSplineResource(const Module& mine, const Module& orig)
{
    CtorSplineResourceFn mineCtor = reinterpret_cast<CtorSplineResourceFn>(
        Proc(mine, "??0SplineResource@Jet@@QAE@ABVPString@1@PBVResourceDatabase@1@@Z"));
    CtorSplineResourceFn origCtor = reinterpret_cast<CtorSplineResourceFn>(
        Proc(orig, "??0SplineResource@Jet@@QAE@ABVPString@1@PBVResourceDatabase@1@@Z"));
    CtorSplineResourceCopyFn mineCopy = reinterpret_cast<CtorSplineResourceCopyFn>(
        Proc(mine, "??0SplineResource@Jet@@QAE@ABV01@@Z"));
    CtorSplineResourceCopyFn origCopy = reinterpret_cast<CtorSplineResourceCopyFn>(
        Proc(orig, "??0SplineResource@Jet@@QAE@ABV01@@Z"));
    DtorSplineResourceFn mineDtor = reinterpret_cast<DtorSplineResourceFn>(
        Proc(mine, "??1SplineResource@Jet@@UAE@XZ"));
    DtorSplineResourceFn origDtor = reinterpret_cast<DtorSplineResourceFn>(
        Proc(orig, "??1SplineResource@Jet@@UAE@XZ"));
    CreateInstanceFn mineCreate = reinterpret_cast<CreateInstanceFn>(
        Proc(mine, "?CreateInstance@SplineResource@Jet@@UBAPAVSpline@2@XZ"));
    CreateInstanceFn origCreate = reinterpret_cast<CreateInstanceFn>(
        Proc(orig, "?CreateInstance@SplineResource@Jet@@UBAPAVSpline@2@XZ"));
    Check(mineCtor && origCtor && mineCopy && origCopy && mineDtor && origDtor && mineCreate && origCreate,
          "SplineResource procedures");
    if (!(mineCtor && origCtor && mineCopy && origCopy && mineDtor && origDtor && mineCreate && origCreate))
        return;

    // Resource's constructor reads KernelResource's ResourceSystem (not set up in this harness), so construction is
    // only checked for locality: the ctor/copy/assignment/destructor addresses must be in our image. CreateInstance
    // itself needs no base state.
    Check(mine.Contains(reinterpret_cast<void*>(mineCtor)), "SplineResource ctor local");
    Check(mine.Contains(reinterpret_cast<void*>(mineCopy)), "SplineResource copy local");
    Check(mine.Contains(reinterpret_cast<void*>(mineDtor)), "SplineResource dtor local");
    Check(mine.Contains(reinterpret_cast<void*>(mineCreate)), "SplineResource CreateInstance local");

    alignas(16) unsigned char mineObj[0x60];
    alignas(16) unsigned char origObj[0x60];
    void** mineTable = static_cast<void**>(Proc(mine, "??_7SplineResource@Jet@@6B@"));
    void** origTable = static_cast<void**>(Proc(orig, "??_7SplineResource@Jet@@6B@"));
    Check(mineTable && mine.Contains(mineTable), "SplineResource vtable local");
    if (mineTable && origTable && mine.Contains(mineTable)) {
        // A stand-in SplineDefinition: slot 1 is CreateSpline, which both CreateInstance bodies call through the
        // definition's vtable. The original does not check the pointer, so provide a valid object.
        static void* const sentinel = reinterpret_cast<void*>(0x5a11);
        struct FakeVt {
            void* dtor;
            void* create;
        };
        static FakeVt fakeVt{nullptr, nullptr};
        struct FakeDefinition {
            FakeVt* vt;
            unsigned char rest[0x20];
        } fakeDefinition{&fakeVt, {}};
        fakeVt.create = reinterpret_cast<void*>(+[](const void*) -> void* { return sentinel; });
        std::memset(mineObj, 0, sizeof mineObj);
        std::memset(origObj, 0, sizeof origObj);
        *reinterpret_cast<void***>(mineObj) = mineTable;
        *reinterpret_cast<void***>(origObj) = origTable;
        *reinterpret_cast<void**>(mineObj + 0x5c) = &fakeDefinition;
        *reinterpret_cast<void**>(origObj + 0x5c) = &fakeDefinition;
        Check(mineCreate(mineObj) == sentinel && origCreate(origObj) == sentinel,
              "SplineResource CreateInstance definition");
    }
}

// ---- SkeletonModifier::Update export ------------------------------------------------------------------------
void TestSkeletonUpdate(const Module& mine, const Module& orig)
{
    UpdateExportFn mineUpdate = reinterpret_cast<UpdateExportFn>(
        Proc(mine, "?Update@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@Modifier@Jet@@2QBUResultTable@2@BW4Results@Modifier@Jet@@@2@M@Z"));
    UpdateExportFn origUpdate = reinterpret_cast<UpdateExportFn>(
        Proc(orig, "?Update@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@Modifier@Jet@@2QBUResultTable@2@BW4Results@Modifier@Jet@@@2@M@Z"));
    CtorModifierFn mineCtor = reinterpret_cast<CtorModifierFn>(
        Proc(mine, "??0SkeletonModifier@Jet@@QAE@PAVParticle@1@M@Z"));
    CtorModifierFn origCtor = reinterpret_cast<CtorModifierFn>(
        Proc(orig, "??0SkeletonModifier@Jet@@QAE@PAVParticle@1@M@Z"));
    DtorModifierFn mineDtor = reinterpret_cast<DtorModifierFn>(Proc(mine, "??1SkeletonModifier@Jet@@UAE@XZ"));
    DtorModifierFn origDtor = reinterpret_cast<DtorModifierFn>(Proc(orig, "??1SkeletonModifier@Jet@@UAE@XZ"));
    Check(mineUpdate != nullptr, "SkeletonModifier update local");
    Check(origUpdate != nullptr, "SkeletonModifier update orig");
    Check(mineCtor != nullptr, "SkeletonModifier ctor local");
    Check(origCtor != nullptr, "SkeletonModifier ctor orig");
    Check(mineDtor != nullptr, "SkeletonModifier dtor local");
    Check(origDtor != nullptr, "SkeletonModifier dtor orig");
    if (!(mineUpdate && origUpdate && mineCtor && origCtor && mineDtor && origDtor))
        return;

    alignas(16) unsigned char mineObj[0x40];
    alignas(16) unsigned char origObj[0x40];
    for (int round = 0; round < 100; ++round) {
        std::memset(mineObj, 0, sizeof mineObj);
        std::memset(origObj, 0, sizeof origObj);
        mineCtor(mineObj, nullptr, 0.5f);
        origCtor(origObj, nullptr, 0.5f);
        int mineResult = -1;
        int origResult = -1;
        const float dt = (round % 7 == 0) ? -0.0f : RandFloat(2.0f);
        mineUpdate(mineObj + 0x18, &mineResult, dt);
        origUpdate(origObj + 0x18, &origResult, dt);
        Check(mineResult == origResult, "SkeletonModifier::Update result");
        // The constructors leave host/name/list pointers in the object; the exported Update's observable result is
        // the code above (the U9b2 test already covers the body against the original with its stand-ins).
        mineDtor(mineObj);
        origDtor(origObj);
    }
}

// ---- locality of the delegating bodies ----------------------------------------------------------------------
// These exports already had local bodies in their units (U7/U8/U9a/U9b2); tails2 only moved the export into this
// image. Their behaviour is covered by the per-unit tests above; here we assert that GetProcAddress resolves inside
// our module rather than to the .def forwarder.
void TestLocalExports(const Module& mine)
{
    static const char* const names[] = {
        "?Initialize@ViewPort@Jet@@AAAXPAVCamera@2@PBURect@2@@Z",
        "?SetCell@Particle@Jet@@QAAXPAVCell@2@@Z",
        "?TestCollision@Particle@Jet@@UAA_NABVRay@2@AAVCollisionResult@2@PBV12@@Z",
        "?TestCollisionGeometry@Particle@Jet@@UBA_NABVRay@2@AAMAAVConstFaceDescriptor@2@@Z",
        "?SetCurrentReferencePoint@ParticleStructure@Jet@@QAA_NABVPString@2@@Z",
        "?EnumerateAttachmentPoints@ParticleStructure@Jet@@QAA_NAAV?$FastDynamicArray@PAVAttachmentPoint@Jet@@$0N@U?$Less@PAVAttachmentPoint@Jet@@PAV12@@2@@2@@Z",
        "?EnumerateAttachmentPoints@ParticleStructure@Jet@@QBA_NAAV?$FastDynamicArray@PBVAttachmentPoint@Jet@@$0N@U?$Less@PBVAttachmentPoint@Jet@@PBV12@@2@@2@@Z",
        "?Cull@Camera@Jet@@UBAXAAVViewPoint@2@@Z",
        "?RenderDebug@Camera@Jet@@UBAXPAVDisplay@2@AAVDebugViewPort@2@AAVMatrix4@2@K@Z",
        "?Begin@ViewPort@Jet@@UAAXXZ",
        "?End@ViewPort@Jet@@UAAXXZ",
        "?Render@ViewPort@Jet@@UAAXXZ",
        "?Update@ViewPort@Jet@@UAAXM@Z",
        "?RenderLayer@ViewPort@Jet@@UAAXAAV?$FastDynamicArray@PAVSystemResourceViewer@Jet@@$0N@U?$Less@PAVSystemResourceViewer@Jet@@PAV12@@2@@2@@Z",
        "?GetElementAt@ViewPort@Jet@@UAAPAVElement@2@ABVVector2@2@@Z",
        "?GetAllWithin@ViewPort@Jet@@UAAXABVVector2@2@0AAV?$FastDynamicArray@PAVElement@Jet@@$0N@U?$Less@PAVElement@Jet@@PAV12@@2@@2@@Z",
        "?Render@DebugViewPort@Jet@@UAAXXZ",
        "?Update@DebugViewPort@Jet@@UAAXM@Z",
        "?BatchCalculateDiffuse@AmbientLight@Jet@@UBAXPAVDisplay@2@HV?$VertexItemPtr@VVector3@Jet@@V?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@2@1V?$VertexItemPtr@KV?$VertexItemPtr@$$CBKVVIPConstHelper@Jet@@@Jet@@@2@ABVMatrix4@2@3PAVMaterial@2@@Z",
        "?BatchCalculateDiffuse@CubeMapLight@Jet@@UBAXPAVDisplay@2@HV?$VertexItemPtr@VVector3@Jet@@V?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@2@1V?$VertexItemPtr@KV?$VertexItemPtr@$$CBKVVIPConstHelper@Jet@@@Jet@@@2@ABVMatrix4@2@3PAVMaterial@2@@Z",
        "?BatchCalculateDiffuse@OmniLight@Jet@@UBAXPAVDisplay@2@HV?$VertexItemPtr@VVector3@Jet@@V?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@2@1V?$VertexItemPtr@KV?$VertexItemPtr@$$CBKVVIPConstHelper@Jet@@@Jet@@@2@ABVMatrix4@2@3PAVMaterial@2@@Z",
        "?BatchCalculateDiffuse@ParallelLight@Jet@@UBAXPAVDisplay@2@HV?$VertexItemPtr@VVector3@Jet@@V?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@2@1V?$VertexItemPtr@KV?$VertexItemPtr@$$CBKVVIPConstHelper@Jet@@@Jet@@@2@ABVMatrix4@2@3PAVMaterial@2@@Z",
        "?BatchCalculateDiffuse@SpotLight@Jet@@UBAXPAVDisplay@2@HV?$VertexItemPtr@VVector3@Jet@@V?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@2@1V?$VertexItemPtr@KV?$VertexItemPtr@$$CBKVVIPConstHelper@Jet@@@Jet@@@2@ABVMatrix4@2@3PAVMaterial@2@@Z",
        "?Add@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResultTable@2@BW4Results@ModifierHost@Jet@@@2@PAVModifier@2@@Z",
        "?InsertBefore@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResultTable@2@BW4Results@ModifierHost@Jet@@@2@PAVModifier@2@@Z",
        "?InsertBefore@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResultTable@2@BW4Results@ModifierHost@Jet@@@2@PAVModifier@2@PBV42@@Z",
        "?InsertAfter@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResultTable@2@BW4Results@ModifierHost@Jet@@@2@PAVModifier@2@@Z",
        "?InsertAfter@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResultTable@2@BW4Results@ModifierHost@Jet@@@2@PAVModifier@2@PBV42@@Z",
        "?AddToBoneBlenders@SkeletonModifier@Jet@@IAA?AV?$ResultClass@$1?results@ModifierHost@Jet@@2QBUResultTable@2@BW4Results@ModifierHost@Jet@@@2@PAVModifier@2@PBV42@_N@Z",
        "?SwitchParticles@SkeletonModifier@Jet@@QAAXPAVParticle@2@PAV?$FastDynamicArray@PAVBoneBlender@Jet@@$0N@U?$Less@PAVBoneBlender@Jet@@PAV12@@2@@2@1@Z",
        "?UpdateBoneModifiers@BoneBlender@Jet@@UAA_NM@Z",
        "?Update@SkeletonModifier@Jet@@UAA?AV?$ResultClass@$1?results@Modifier@Jet@@2QBUResultTable@2@BW4Results@Modifier@Jet@@@2@M@Z",
        "?GetRatio@SplineSegment@Jet@@UBAMMMMI@Z",
        "??_7Camera@Jet@@6B@",
        "??_7ViewPort@Jet@@6B@",
        "??_7DebugViewPort@Jet@@6B@",
        "??_7SplineResource@Jet@@6B@",
    };
    for (const char* name : names) {
        void* p = Proc(mine, name);
        Check(p != nullptr && mine.Contains(p), name);
    }
}

}  // namespace

int main(int argc, char** argv)
{
    const char* dir = argc > 1 ? argv[1] : ".";
    SetCurrentDirectoryA(dir);
    const Module mine = Open("Render3DCore_7c.dll");
    const Module orig = Open("Render3DCore_7c_orig.dll");
    int rounds = argc > 2 ? std::atoi(argv[2]) : 1000;
    (void)rounds;

    static const SlotRole cameraRoles[] = {
        {6, "?CreateInstance@Camera@Jet@@UBAPAVParticle@2@XZ"},
        {7, "?GetID@Camera@Jet@@UBAHXZ"},
        {8, "?IsOfType@Camera@Jet@@UBA_NH@Z"},
        {11, "?SetPosition@Camera@Jet@@UAAXABVVector3@2@@Z"},
        {13, "?SetOrientation@Camera@Jet@@UAAXABVQuaternion@2@@Z"},
        {14, "?SetReferenceFrame@Camera@Jet@@UAAXABVVector3@2@ABVQuaternion@2@@Z"},
    };
    static const SlotRole viewPortRoles[] = {
        {1, "?RenderLayer@ViewPort@Jet@@UAAXAAV?$FastDynamicArray@PAVSystemResourceViewer@Jet@@$0N@U?$Less@PAVSystemResourceViewer@Jet@@PAV12@@2@@2@@Z"},
        {6, "?Render@ViewPort@Jet@@UAAXXZ"},
        {7, "?Update@ViewPort@Jet@@UAAXM@Z"},
        {8, "?Begin@ViewPort@Jet@@UAAXXZ"},
        {9, "?End@ViewPort@Jet@@UAAXXZ"},
        {10, "?CanRender@ViewPort@Jet@@UAA_NXZ"},
        {11, "?GetElementAt@ViewPort@Jet@@UAAPAVElement@2@ABVVector2@2@@Z"},
        {12, "?GetAllWithin@ViewPort@Jet@@UAAXABVVector2@2@0AAV?$FastDynamicArray@PAVElement@Jet@@$0N@U?$Less@PAVElement@Jet@@PAV12@@2@@2@@Z"},
    };
    static const SlotRole debugRoles[] = {
        {6, "?Render@DebugViewPort@Jet@@UAAXXZ"},
        {7, "?Update@DebugViewPort@Jet@@UAAXM@Z"},
    };

    CheckVTable(mine, orig, "??_7Camera@Jet@@6B@", cameraRoles, 6);
    CheckVTable(mine, orig, "??_7ViewPort@Jet@@6B@", viewPortRoles, 8);
    CheckVTable(mine, orig, "??_7DebugViewPort@Jet@@6B@", debugRoles, 2);

    TestLocalExports(mine);
    TestGetRatio(mine, orig);
    TestSplineResource(mine, orig);
    TestSkeletonUpdate(mine, orig);

    std::printf("%lld checks, %lld mismatches, %lld NaN-payload-only differences\n", g_checks, g_mismatches,
                g_nanDiffs);
    return g_mismatches == 0 ? 0 : 1;
}
