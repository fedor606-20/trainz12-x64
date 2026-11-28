// Differential test of Render3DCore_7c Final15: the multipass material texture virtuals (SetTexture x2,
// DuplicateTexture, IsLoaded on MultiPassNoTextureMaterial / MultiPassOneTextureMaterial / MultiPassBumpMaterial)
// and the four OrientedBox best-fit exports, ours against Render3DCore_7c_orig.dll in one process.
//
// The material entries run on synthetic objects with recording vtables: a fake texture manager (slot 3), fake
// textures (slots 2/3/0x48) and a fake material (slot 0x5c). Material::SetTextureDefinition is intercepted at its
// RenderSystem_7c export, so both modules log the same call without touching real material state.
//
// usage: diff_r3d_final15.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

LONG CALLBACK VectoredHandler(EXCEPTION_POINTERS* info)
{
    HMODULE module = nullptr;
    const void* address = info->ExceptionRecord->ExceptionAddress;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCSTR>(address), &module);
    char name[MAX_PATH] = "?";
    if (module) GetModuleFileNameA(module, name, sizeof name);
    std::printf("FAULT %p code %08x in %s+%x\n", address, info->ExceptionRecord->ExceptionCode, name,
                module ? unsigned(std::uintptr_t(address) - std::uintptr_t(module)) : 0);
    std::fflush(stdout);
    return EXCEPTION_CONTINUE_SEARCH;
}

unsigned long long checks, bad;
void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        static int shown = 0;
        if (shown++ < 40) std::printf("MISMATCH %s\n", what);
    }
}

HMODULE ours, orig, renderSystem;
const char* kOrig = "Render3DCore_7c_orig.dll";

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

struct Patch {
    unsigned char* target = nullptr;
    unsigned char saved[5] = {};
    bool active = false;
};
void PatchEnter(Patch& patch, void* target, void* hook)
{
    patch.target = static_cast<unsigned char*>(target);
    DWORD old = 0;
    VirtualProtect(patch.target, 5, PAGE_EXECUTE_READWRITE, &old);
    std::memcpy(patch.saved, patch.target, 5);
    patch.target[0] = 0xe9;
    const int delta = static_cast<int>(reinterpret_cast<std::uintptr_t>(hook) -
                                       (reinterpret_cast<std::uintptr_t>(patch.target) + 5));
    std::memcpy(patch.target + 1, &delta, 4);
    VirtualProtect(patch.target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), patch.target, 5);
    patch.active = true;
}
void PatchRestore(Patch& patch)
{
    if (!patch.active) return;
    DWORD old = 0;
    VirtualProtect(patch.target, 5, PAGE_EXECUTE_READWRITE, &old);
    std::memcpy(patch.target, patch.saved, 5);
    VirtualProtect(patch.target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), patch.target, 5);
    patch.active = false;
}

// ---- export signatures (the original cdecl ABI: this, hidden ResultClass pointer, then the arguments) -----------
using SetTextureDbFn = void(__cdecl*)(void*, void*, int, const void*, const void*, float, const void*);
using SetTextureRefFn = void(__cdecl*)(void*, void*, int, const void*, float);
using DuplicateTextureFn = void(__cdecl*)(void*, void*, int);
using IsLoadedFn = void(__cdecl*)(const void*, void*);
using MakeBestFitArrayFn = void(__cdecl*)(void*, unsigned int, const float*);
using MakeBestFitItemsFn = void(__cdecl*)(void*, unsigned int, unsigned int, const void*);
using BoxVolumeFn = float(__cdecl*)(const float*, unsigned int, unsigned int, const void*);
using MinimalBoxFn = void(__cdecl*)(void*, int, unsigned int, const void*, float*);

struct Entry {
    DuplicateTextureFn duplicate;
    IsLoadedFn isLoaded;
    SetTextureDbFn setDb;
    SetTextureRefFn setRef;
};
Entry entry[2][3];  // [side][class], class 0 NoTexture, 1 OneTexture, 2 Bump

const char* kN[3] = {"NoTexture", "OneTexture", "Bump"};
const char* kDupName[3] = {"?DuplicateTexture@MultiPassNoTextureMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@H@Z",
                           "?DuplicateTexture@MultiPassOneTextureMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@H@Z",
                           "?DuplicateTexture@MultiPassBumpMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@H@Z"};
const char* kLoadName[2] = {"?IsLoaded@MultiPassOneTextureMaterial@Jet@@UBA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@XZ",
                            "?IsLoaded@MultiPassBumpMaterial@Jet@@UBA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@XZ"};
const char* kDbName[3] = {"?SetTexture@MultiPassNoTextureMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HPBVResourceDatabase@2@ABVPString@2@MABVString@2@@Z",
                          "?SetTexture@MultiPassOneTextureMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HPBVResourceDatabase@2@ABVPString@2@MABVString@2@@Z",
                          "?SetTexture@MultiPassBumpMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HPBVResourceDatabase@2@ABVPString@2@MABVString@2@@Z"};
const char* kRefName[3] = {"?SetTexture@MultiPassNoTextureMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HABV?$AutoReference@VTexture@Jet@@@2@M@Z",
                           "?SetTexture@MultiPassOneTextureMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HABV?$AutoReference@VTexture@Jet@@@2@M@Z",
                           "?SetTexture@MultiPassBumpMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HABV?$AutoReference@VTexture@Jet@@@2@M@Z"};

// ---- recording objects ----------------------------------------------------------------------------------------
// Object header at +0: vftable, +4: ReferenceCount's counter, +8: our id. A Texture's PString sits at +0x10
// (never dereferenced: SetTextureDefinition is intercepted).
struct Fake {
    void** vt;    // +0
    int count;    // +4
    int id;       // +8
    unsigned char name[0x40];  // +0xc onward, covers +0x10
};

enum Op { OP_DUPLICATE = 1, OP_AVAILABLE, OP_PRIORITY, OP_UPDATE, OP_CREATE, OP_DEFINITION };
struct Log {
    int count;
    int op[128];
    int id[128];
    int aux[128];
};
Log logs[2];
unsigned long long opTotals[8];
int g_side;

int IdOf(const void* p)
{
    if (!p) return 0;
    return static_cast<const Fake*>(p)->id;
}
void Push(int op, const void* p, int aux = 0)
{
    Log& l = logs[g_side];
    if (l.count < 128) {
        l.op[l.count] = op;
        l.id[l.count] = IdOf(p);
        l.aux[l.count] = aux;
        ++l.count;
    }
    ++opTotals[op];
}
Fake* g_duplicateResult[2];
int g_available[2][4];  // per side, by texture id 1..4

void* __cdecl RecDuplicate(void* self)
{
    Push(OP_DUPLICATE, self);
    Fake* copy = g_duplicateResult[g_side];
    if (copy) copy->count = 1;
    return copy;
}
bool __cdecl RecAvailable(void* self)
{
    const int id = IdOf(self);
    Push(OP_AVAILABLE, self);
    return g_available[g_side][id & 3] != 0;
}
void __cdecl RecPriority(void* self, float priority)
{
    unsigned bits;
    std::memcpy(&bits, &priority, 4);
    Push(OP_PRIORITY, self, int(bits));
}
void __cdecl RecUpdate(void* self) { Push(OP_UPDATE, self); }
void* __cdecl RecCreate(void* manager, const void* db, const void* name, const void* request)
{
    Push(OP_CREATE, manager, (db != nullptr) | ((name != nullptr) << 1) | ((request != nullptr) << 2));
    return g_duplicateResult[g_side];
}
void __cdecl RecDefinition(void* self, int type, const void* name, float weight)
{
    unsigned bits;
    std::memcpy(&bits, &weight, 4);
    Push(OP_DEFINITION, self, (type & 0xff) | (int(bits) << 8));
    (void)name;
}
void* __cdecl RecUnused() { return nullptr; }

void* textureVt[24];
void* managerVt[8];
void* materialVt[32];

struct Fixture {
    Fake textures[5];      // ids 1..4 used; 0 unused
    Fake manager;
    Fake materialManager;
    unsigned char material[0x100];
};

void ResetSide(int side, Fixture& f)
{
    std::memset(&f, 0, sizeof f);
    for (int i = 0; i < 5; ++i) {
        f.textures[i].vt = textureVt;
        f.textures[i].count = 2;
        f.textures[i].id = i;
    }
    f.manager.vt = managerVt;
    f.manager.count = 1000;
    f.manager.id = 0x200;
    f.materialManager.vt = managerVt;
    f.materialManager.count = 1000;
    f.materialManager.id = 0x201;
    // manager object's +0xc must hold the texture manager
    *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(&f.materialManager) + 0x0c) = &f.manager;
    // material buffer
    *reinterpret_cast<void**>(f.material + 0x00) = materialVt;
    void* mm = &f.materialManager;
    std::memcpy(f.material + 0x98, &mm, 4);
    const float priority = 0.75f;
    std::memcpy(f.material + 0x94, &priority, 4);
    // texture slots
    void* t[4] = {&f.textures[1], &f.textures[2], &f.textures[3], &f.textures[4]};
    std::memcpy(f.material + 0xa4, &t[0], 4);
    std::memcpy(f.material + 0xa8, &t[1], 4);
    std::memcpy(f.material + 0xc4, &t[2], 4);
    std::memcpy(f.material + 0xd0, &t[3], 4);
    const unsigned alphaHint = 7;
    std::memcpy(f.material + 0xc8, &alphaHint, 4);
    g_available[side][0] = 0;  // id 0
    g_available[side][1] = 1;
    g_available[side][2] = 1;
    g_available[side][3] = 1;
    g_duplicateResult[side] = &f.textures[0];  // id 0 = the duplicate target
    logs[side].count = 0;
}

unsigned SlotValue(const Fixture& f, int slot)
{
    const void* p = nullptr;
    std::memcpy(&p, f.material + slot, 4);
    return unsigned(IdOf(p));
}
int CountOf(const Fixture& f, int slot)
{
    const void* p = nullptr;
    std::memcpy(&p, f.material + slot, 4);
    return p ? static_cast<const Fake*>(p)->count : -1;
}

void CompareLogs(const char* what)
{
    Check(logs[0].count == logs[1].count, what);
    const int n = logs[0].count < logs[1].count ? logs[0].count : logs[1].count;
    for (int i = 0; i < n; ++i) {
        Check(logs[0].op[i] == logs[1].op[i], what);
        Check(logs[0].id[i] == logs[1].id[i], what);
        Check(logs[0].aux[i] == logs[1].aux[i], what);
    }
}

void CompareFixture(Fixture& a, Fixture& b, const char* what)
{
    for (int slot : {0xa4, 0xa8, 0xc4, 0xd0}) {
        Check(SlotValue(a, slot) == SlotValue(b, slot), what);
        Check(CountOf(a, slot) == CountOf(b, slot), what);
    }
    Check(std::memcmp(a.material + 0xc8, b.material + 0xc8, 4) == 0, what);
    for (int i = 0; i < 4; ++i) Check(a.textures[i].count == b.textures[i].count, what);
}

// ---- OrientedBox ----------------------------------------------------------------------------------------------
MakeBestFitArrayFn bestFitArray[2];
MakeBestFitItemsFn bestFitItems[2];
BoxVolumeFn boxVolume[2];
MinimalBoxFn minimalBox[2];

unsigned RandBits()
{
    const unsigned r = unsigned(std::rand());
    return (r << 16) ^ unsigned(std::rand()) ^ (unsigned(std::rand()) << 8);
}
float RandFloat(float scale)
{
    return (float(std::rand()) / float(RAND_MAX) * 2.0f - 1.0f) * scale;
}

void TestOrientedBox(int rounds)
{
    for (int round = 0; round < rounds; ++round) {
        unsigned char points[64 * 16];
        std::memset(points, 0, sizeof points);
        const unsigned count = 1 + unsigned(std::rand()) % 24;
        for (unsigned i = 0; i < count; ++i) {
            float v[3] = {RandFloat(50.0f), RandFloat(50.0f), RandFloat(50.0f)};
            std::memcpy(points + i * 12, v, 12);
        }
        if (round % 5 == 0) {  // duplicate points
            for (unsigned i = 1; i < count; ++i) std::memcpy(points + i * 12, points, 12);
        }
        unsigned char boxA[0x40], boxB[0x40];
        std::memset(boxA, 0xa5, sizeof boxA);
        std::memset(boxB, 0xa5, sizeof boxB);
        bestFitArray[1](boxB, count, reinterpret_cast<const float*>(points));
        bestFitArray[0](boxA, count, reinterpret_cast<const float*>(points));
        Check(std::memcmp(boxA, boxB, 0x3c) == 0, "MakeBestFit(array) box");

        // strided view: Vector3 at the start of every 0x10-byte record
        unsigned char strided[24 * 16];
        std::memset(strided, 0, sizeof strided);
        for (unsigned i = 0; i < count; ++i) std::memcpy(strided + i * 16, points + i * 12, 12);
        std::memset(boxA, 0x5a, sizeof boxA);
        std::memset(boxB, 0x5a, sizeof boxB);
        bestFitItems[1](boxB, count, 0x10, strided);
        bestFitItems[0](boxA, count, 0x10, strided);
        Check(std::memcmp(boxA, boxB, 0x3c) == 0, "MakeBestFit(items) box");

        float angles[3] = {RandFloat(6.5f), RandFloat(6.5f), RandFloat(6.5f)};
        const float va = boxVolume[0](angles, count, 0xc, points);
        const float vb = boxVolume[1](angles, count, 0xc, points);
        unsigned ba, bb;
        std::memcpy(&ba, &va, 4);
        std::memcpy(&bb, &vb, 4);
        Check(ba == bb, "Volume float");

        std::memset(boxA, 0x11, sizeof boxA);
        std::memset(boxB, 0x22, sizeof boxB);
        minimalBox[1](boxB, 8, 0xc, points, angles);
        minimalBox[0](boxA, 8, 0xc, points, angles);
        Check(std::memcmp(boxA, boxB, 0x3c) == 0, "MinimalBoxForAngles box");
    }
}

void TestMaterials()
{
    static Fixture a, b;
    const int specialTypes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xb, 0xc, 0xd, 0xe, 0x10};
    unsigned char dummyName[0x20] = {};
    unsigned char dummyRequest[0x20] = {};

    for (int cls = 0; cls < 3; ++cls) {
        for (int type : specialTypes) {
            // DuplicateTexture
            ResetSide(0, a);
            ResetSide(1, b);
            void* outA = nullptr;
            void* outB = nullptr;
            int codeA = -1, codeB = -1;
            outA = &codeA;
            outB = &codeB;
            g_side = 0;
            entry[0][cls].duplicate(a.material, outA, type);
            g_side = 1;
            entry[1][cls].duplicate(b.material, outB, type);
            Check(codeA == codeB, "DuplicateTexture result");
            CompareLogs("DuplicateTexture log");
            CompareFixture(a, b, "DuplicateTexture state");
        }

        // IsLoaded (OneTexture and Bump only)
        if (cls >= 1) {
            for (int avail = 0; avail < 2; ++avail) {
                for (int present = 0; present < 2; ++present) {
                    ResetSide(0, a);
                    ResetSide(1, b);
                    g_available[0][1] = g_available[0][2] = g_available[0][3] = avail;
                    g_available[1][1] = g_available[1][2] = g_available[1][3] = avail;
                    if (!present) {
                        const int zero = 0;
                        std::memcpy(a.material + 0xc4, &zero, 4);
                        std::memcpy(b.material + 0xc4, &zero, 4);
                        std::memcpy(a.material + 0xd0, &zero, 4);
                        std::memcpy(b.material + 0xd0, &zero, 4);
                    }
                    int codeA = -1, codeB = -1;
                    g_side = 0;
                    entry[0][cls].isLoaded(a.material, &codeA);
                    g_side = 1;
                    entry[1][cls].isLoaded(b.material, &codeB);
                    Check(codeA == codeB, "IsLoaded result");
                    CompareLogs("IsLoaded log");
                    CompareFixture(a, b, "IsLoaded state");
                }
            }
        }

        // SetTexture (AutoReference)
        for (int useNull = 0; useNull < 2; ++useNull) {
            for (float amount : {0.0f, 1.5f, -3.25f}) {
                ResetSide(0, a);
                ResetSide(1, b);
                Fake* refObjA = useNull ? nullptr : &a.textures[1];
                Fake* refObjB = useNull ? nullptr : &b.textures[1];
                const void* refA = &refObjA;
                const void* refB = &refObjB;
                int codeA = -1, codeB = -1;
                g_side = 0;
                entry[0][cls].setRef(a.material, &codeA, 1, refA, amount);
                g_side = 1;
                entry[1][cls].setRef(b.material, &codeB, 1, refB, amount);
                Check(codeA == codeB, "SetTexture(ref) result");
                CompareLogs("SetTexture(ref) log");
                CompareFixture(a, b, "SetTexture(ref) state");

                for (int type : specialTypes) {
                    ResetSide(0, a);
                    ResetSide(1, b);
                    codeA = codeB = -1;
                    Fake* ref2A = useNull ? nullptr : &a.textures[2];
                    Fake* ref2B = useNull ? nullptr : &b.textures[2];
                    g_side = 0;
                    entry[0][cls].setRef(a.material, &codeA, type, &ref2A, amount);
                    g_side = 1;
                    entry[1][cls].setRef(b.material, &codeB, type, &ref2B, amount);
                    Check(codeA == codeB, "SetTexture(ref,type) result");
                    CompareLogs("SetTexture(ref,type) log");
                    CompareFixture(a, b, "SetTexture(ref,type) state");
                }
            }
        }

        // SetTexture (database)
        for (int useNull = 0; useNull < 2; ++useNull) {
            for (float amount : {0.0f, 2.0f, -0.5f}) {
                for (int type : specialTypes) {
                    ResetSide(0, a);
                    ResetSide(1, b);
                    g_duplicateResult[0] = useNull ? nullptr : &a.textures[4];
                    g_duplicateResult[1] = useNull ? nullptr : &b.textures[4];
                    int codeA = -1, codeB = -1;
                    g_side = 0;
                    entry[0][cls].setDb(a.material, &codeA, type, nullptr, dummyName, amount, dummyRequest);
                    g_side = 1;
                    entry[1][cls].setDb(b.material, &codeB, type, nullptr, dummyName, amount, dummyRequest);
                    Check(codeA == codeB, "SetTexture(db) result");
                    CompareLogs("SetTexture(db) log");
                    CompareFixture(a, b, "SetTexture(db) state");
                }
            }
        }
    }
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_final15 <bin dir> [rounds]\n");
        return 2;
    }
    char fullDir[MAX_PATH] = {};
    GetFullPathNameA(argv[1], MAX_PATH, fullDir, nullptr);
    SetDllDirectoryA(fullDir);
    const std::string dir = fullDir;
    for (const char* name : {"KernelStandard_7c_orig.dll", "Kernel_7c_orig.dll", "KernelResource_7c_orig.dll",
                             "KernelResources_7c_orig.dll", "RenderSystem_7c_orig.dll", "Render2DCore_7c_orig.dll"}) {
        if (!LoadLibraryA((dir + "\\" + name).c_str())) std::printf("preload %s failed %lu\n", name, GetLastError());
    }
    if (!LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str())) std::printf("orig load failed %lu\n", GetLastError());
    ours = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    if (!ours) std::printf("ours load failed %lu\n", GetLastError());
    orig = GetModuleHandleA(kOrig);
    renderSystem = GetModuleHandleA("RenderSystem_7c.dll");
    if (!renderSystem) renderSystem = LoadLibraryA("RenderSystem_7c.dll");
    if (!ours || !orig || !renderSystem) {
        std::printf("cannot load the modules (%lu)\n", GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    AddVectoredExceptionHandler(1, VectoredHandler);

    // vtables
    for (int i = 0; i < 24; ++i) textureVt[i] = reinterpret_cast<void*>(&RecUnused);
    textureVt[0] = reinterpret_cast<void*>(&RecUnused);
    textureVt[1] = reinterpret_cast<void*>(&RecUnused);
    textureVt[2] = reinterpret_cast<void*>(&RecDuplicate);
    textureVt[3] = reinterpret_cast<void*>(&RecAvailable);
    textureVt[0x48 / 4] = reinterpret_cast<void*>(&RecPriority);
    for (int i = 0; i < 8; ++i) managerVt[i] = reinterpret_cast<void*>(&RecUnused);
    managerVt[3] = reinterpret_cast<void*>(&RecCreate);
    for (int i = 0; i < 32; ++i) materialVt[i] = reinterpret_cast<void*>(&RecUnused);
    materialVt[0x5c / 4] = reinterpret_cast<void*>(&RecUpdate);

    for (int side = 0; side < 2; ++side) {
        HMODULE module = side ? orig : ours;
        bestFitArray[side] = Proc<MakeBestFitArrayFn>(
            module, "?MakeBestFit@OrientedBox@Jet@@QAAXIPBVVector3@2@@Z", side ? "orig" : "ours");
        bestFitItems[side] = Proc<MakeBestFitItemsFn>(
            module, "?MakeBestFit@OrientedBox@Jet@@QAAXIV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@Z",
            side ? "orig" : "ours");
        boxVolume[side] = Proc<BoxVolumeFn>(
            module, "?Volume@OrientedBox@Jet@@KAMPBMIV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@Z",
            side ? "orig" : "ours");
        minimalBox[side] = Proc<MinimalBoxFn>(
            module, "?MinimalBoxForAngles@OrientedBox@Jet@@IAAXHV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@QAM@Z",
            side ? "orig" : "ours");
        for (int cls = 0; cls < 3; ++cls) {
            entry[side][cls].duplicate = Proc<DuplicateTextureFn>(module, kDupName[cls], side ? "orig" : "ours");
            entry[side][cls].setDb = Proc<SetTextureDbFn>(module, kDbName[cls], side ? "orig" : "ours");
            entry[side][cls].setRef = Proc<SetTextureRefFn>(module, kRefName[cls], side ? "orig" : "ours");
            entry[side][cls].isLoaded = Proc<IsLoadedFn>(module, kLoadName[cls == 2 ? 1 : 0], side ? "orig" : "ours");
        }
    }

    Patch definitionPatch;
    if (void* const address = reinterpret_cast<void*>(
            GetProcAddress(renderSystem, "?SetTextureDefinition@Material@Jet@@IAAXHABVPString@2@M@Z")))
        PatchEnter(definitionPatch, address, reinterpret_cast<void*>(&RecDefinition));
    else
        std::printf("missing RenderSystem SetTextureDefinition\n");

    const int rounds = argc > 2 ? std::atoi(argv[2]) : 200;
    TestMaterials();
    TestOrientedBox(rounds);

    PatchRestore(definitionPatch);
    std::printf("events: dup %llu avail %llu prio %llu update %llu create %llu def %llu\n",
                opTotals[OP_DUPLICATE], opTotals[OP_AVAILABLE], opTotals[OP_PRIORITY], opTotals[OP_UPDATE],
                opTotals[OP_CREATE], opTotals[OP_DEFINITION]);
    for (int op : {OP_DUPLICATE, OP_AVAILABLE, OP_PRIORITY, OP_UPDATE, OP_CREATE, OP_DEFINITION})
        Check(opTotals[op] > 0, "event coverage");
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
