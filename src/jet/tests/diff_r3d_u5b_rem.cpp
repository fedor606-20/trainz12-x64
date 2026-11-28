// Differential test of Render3DCore_7c U5b wave 4: the multipass material, plugin and geometry-chunk lifetimes,
// CreateInstance/InitInstance and equality, ours against Render3DCore_7c_orig.dll in one process.
//
// The objects are flat buffers; the constructors install the original class vtables and call the same RenderSystem
// base constructors, so the whole object image is compared. RenderSystem_7c's SharedTextureCollection::GetTexture
// and Material::CreateInstance are patched at their entry points (both sides resolve them by address), so no real
// manager or texture array is needed.
//
// usage: diff_r3d_u5b_rem.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
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
std::mt19937 rng(0x5eed5b4);

int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }

void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        std::printf("MISMATCH %s\n", what);
    }
}
bool Same(const void* a, const void* b, unsigned n) { return std::memcmp(a, b, n) == 0; }
void CheckBytes(const void* a, const void* b, unsigned n, const char* what)
{
    if (!Same(a, b, n)) {
        const auto* x = static_cast<const unsigned char*>(a);
        const auto* y = static_cast<const unsigned char*>(b);
        for (unsigned i = 0; i < n; i += 4)
            if (std::memcmp(x + i, y + i, 4) != 0) {
                std::printf("%s at +0x%x: ours %08x orig %08x\n", what, i,
                            *reinterpret_cast<const unsigned*>(x + i), *reinterpret_cast<const unsigned*>(y + i));
                break;
            }
    }
    Check(Same(a, b, n), what);
}

void CheckPlugin(const void* a, const void* b, const char* what)
{
    unsigned char x[0x90] = {};
    unsigned char y[0x90] = {};
    std::memcpy(x, a, 0x70);
    std::memcpy(y, b, 0x70);
    std::memset(x + 0x34, 0, 0x3c);
    std::memset(y + 0x34, 0, 0x3c);
    CheckBytes(x, y, 0x70, what);
    bool names = true;
    for (unsigned i = 0; i < 15; ++i) {
        const char* const sa = *reinterpret_cast<const char* const*>(static_cast<const unsigned char*>(a) + 0x34 + i * 4);
        const char* const sb = *reinterpret_cast<const char* const*>(static_cast<const unsigned char*>(b) + 0x34 + i * 4);
        if ((sa == nullptr) != (sb == nullptr) || (sa && std::strcmp(sa, sb) != 0)) names = false;
    }
    Check(names, "plugin name table text");
}
void CheckObject(const void* a, const void* b, unsigned n, const char* what)
{
    unsigned char x[0x120] = {};
    unsigned char y[0x120] = {};
    std::memcpy(x, a, n);
    std::memcpy(y, b, n);
    if (n >= 0xc4) {
        std::memset(x + 0xc0, 0, 4);
        std::memset(y + 0xc0, 0, 4);
    }
    // PString nodes created by Material::InitInstance are per-instance allocations with identical text.
    for (unsigned offset = 0x24; offset <= 0x30; offset += 4) {
        std::memset(x + offset, 0, 4);
        std::memset(y + offset, 0, 4);
    }
    CheckBytes(x, y, n, what);
}

// The geometry-chunk template constructor allocates several per-instance objects (addresses differ). The unit's own
// contribution is the two vtable slots and the refcounted buffer pointer; compare those, and for the destructor
// also that the buffer reference was dropped.
void CheckChunkCtor(const void* a, const void* b)
{
    const auto* x = static_cast<const unsigned char*>(a);
    const auto* y = static_cast<const unsigned char*>(b);
    Check(*reinterpret_cast<const void* const*>(x) == *reinterpret_cast<const void* const*>(y), "chunk ctor +0");
    Check(*reinterpret_cast<const void* const*>(x + 0x60) == *reinterpret_cast<const void* const*>(y + 0x60),
          "chunk ctor +0x60");
    Check((*reinterpret_cast<void* const*>(x + 0x5c) == nullptr) ==
              (*reinterpret_cast<void* const*>(y + 0x5c) == nullptr),
          "chunk ctor +0x5c");
}
void CheckChunkDtor(const void* a, const void* b)
{
    const auto* x = static_cast<const unsigned char*>(a);
    const auto* y = static_cast<const unsigned char*>(b);
    Check(*reinterpret_cast<const void* const*>(x) == *reinterpret_cast<const void* const*>(y), "chunk dtor +0");
    Check(*reinterpret_cast<const void* const*>(x + 0x60) == *reinterpret_cast<const void* const*>(y + 0x60),
          "chunk dtor +0x60");
    Check(*reinterpret_cast<void* const*>(x + 0x5c) == *reinterpret_cast<void* const*>(y + 0x5c),
          "chunk dtor +0x5c");
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

// ---------------------------------------------------------------------------------------------------------------
// entry-point patches (5-byte relative jump; the hooks never call the replaced body)

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

void* OrigData(const char* name) { return reinterpret_cast<void*>(GetProcAddress(orig, name)); }

// ---------------------------------------------------------------------------------------------------------------
// stand-ins

unsigned textureFetches;
int lastTextureId;
void* textureResult;

bool __cdecl HookGetTexture(int id, void*, void* out)
{
    ++textureFetches;
    lastTextureId = id;
    *static_cast<void**>(out) = textureResult;
    return true;
}

void* factoryResult;
int factoryCalls;
const void* factoryType;
void* factoryManager;
void* __cdecl HookCreateInstance(const void* type, void* manager)
{
    ++factoryCalls;
    factoryType = type;
    factoryManager = manager;
    return factoryResult;
}

void ResetHooks()
{
    textureFetches = 0;
    lastTextureId = -1;
    factoryCalls = 0;
    factoryType = nullptr;
    factoryManager = nullptr;
}

struct FakeTexture {
    void* vtable;
    int refcount;
    unsigned char pad[0x40];
};
FakeTexture fakeTextures[4];

struct FakeCache {
    unsigned char bytes[0x40];
};
FakeCache cacheA[2], cacheB[2];
int cacheVbtable[2];

struct FakeLight {
    void* vtable;
    int refcount;
    unsigned char pad[0x40];
};
FakeLight light0[2], light1[2];
void* lightVtable[8];
void* managerVtable[8];
bool __cdecl ManagerRemove(void*, void*, int) { return false; }

void ResetRefs(int cacheCount, int lightCount)
{
    *reinterpret_cast<int*>(cacheA[0].bytes + 0x10) = cacheCount;
    *reinterpret_cast<int*>(cacheB[0].bytes + 0x10) = cacheCount;
    light0[0].refcount = lightCount;
    light1[0].refcount = lightCount;
}

void InitFakes()
{
    for (unsigned i = 0; i < 4; ++i) {
        std::memset(&fakeTextures[i], 0, sizeof fakeTextures[i]);
        fakeTextures[i].refcount = 100000;
    }
    cacheVbtable[0] = 0;
    cacheVbtable[1] = 0;
    for (unsigned i = 0; i < 8; ++i) lightVtable[i] = reinterpret_cast<void*>(&ManagerRemove);
    for (unsigned i = 0; i < 8; ++i) managerVtable[i] = reinterpret_cast<void*>(&ManagerRemove);
    for (int side = 0; side < 2; ++side) {
        std::memset(&cacheA[side], 0, sizeof cacheA[side]);
        std::memset(&cacheB[side], 0, sizeof cacheB[side]);
        *reinterpret_cast<void**>(cacheA[side].bytes + 0x0c) = cacheVbtable;
        *reinterpret_cast<void**>(cacheB[side].bytes + 0x0c) = cacheVbtable;
        *reinterpret_cast<int*>(cacheA[side].bytes + 0x10) = 1000;
        *reinterpret_cast<int*>(cacheB[side].bytes + 0x10) = 1000;
        std::memset(&light0[side], 0, sizeof light0[side]);
        std::memset(&light1[side], 0, sizeof light1[side]);
        light0[side].vtable = lightVtable;
        light1[side].vtable = lightVtable;
        light0[side].refcount = 1000;
        light1[side].refcount = 1000;
    }
}

// fake manager -> kernel -> {textures +0x94}
unsigned char managerBytes[0x40], kernelBytes[0x100];
void MakeEnvironment()
{
    std::memset(managerBytes, 0, sizeof managerBytes);
    std::memset(kernelBytes, 0, sizeof kernelBytes);
    *reinterpret_cast<void**>(managerBytes) = managerVtable;
    *reinterpret_cast<int*>(managerBytes + 4) = 100000;
    *reinterpret_cast<void**>(managerBytes + 8) = kernelBytes;
    *reinterpret_cast<void**>(kernelBytes + 0x94) = managerBytes;
}

// ---------------------------------------------------------------------------------------------------------------
// class tables

enum MaterialKind { NoTex, OneTex, Bump, Bump2, BumpSpec, BumpSpec2 };
const MaterialKind materialKinds[] = {NoTex, OneTex, Bump, Bump2, BumpSpec, BumpSpec2};
const unsigned materialSize[] = {0xd0, 0xdc, 0xe0, 0xe0, 0xe0, 0xe0};
const unsigned materialVbase[] = {0xc8, 0xd4, 0xd8, 0xd8, 0xd8, 0xd8};

const char* MaterialName(MaterialKind k)
{
    switch (k) {
    case NoTex: return "MultiPassNoTextureMaterial";
    case OneTex: return "MultiPassOneTextureMaterial";
    case Bump: return "MultiPassBumpMaterial";
    case Bump2: return "MultiPassBump2Material";
    case BumpSpec: return "MultiPassBumpSpecMaterial";
    default: return "MultiPassBumpSpec2Material";
    }
}
const char* MaterialType(MaterialKind k)
{
    switch (k) {
    case NoTex: return "?UNDECORATED_TYPE@MultiPassNoTextureMaterial@Jet@@2VPString@2@B";
    case OneTex: return "?UNDECORATED_TYPE@MultiPassOneTextureMaterial@Jet@@2VPString@2@B";
    case Bump: return "?UNDECORATED_TYPE@MultiPassBumpMaterial@Jet@@2VPString@2@B";
    case Bump2: return "?UNDECORATED_TYPE@MultiPassBump2Material@Jet@@2VPString@2@B";
    case BumpSpec: return "?UNDECORATED_TYPE@MultiPassBumpSpecMaterial@Jet@@2VPString@2@B";
    default: return "?UNDECORATED_TYPE@MultiPassBumpSpec2Material@Jet@@2VPString@2@B";
    }
}

void ExportNames(MaterialKind k, char* ctor, size_t ctorSize, char* dtor, size_t dtorSize, char* vbase,
                 size_t vbaseSize, char* create, size_t createSize, char* init, size_t initSize, char* equals,
                 size_t equalsSize, char* eq, size_t eqSize)
{
    const char* const cls = MaterialName(k);
    if (k == NoTex) {
        std::snprintf(ctor, ctorSize,
                      "??0%s@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaBlendCache@1@"
                      "PAUMPLightRender@MultiPassRenderState@1@3@Z", cls);
    } else {
        std::snprintf(ctor, ctorSize,
                      "??0%s@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaBlendCache@1@"
                      "PAUMPLightRender@MultiPassRenderState@1@3PAVVertexType@1@@Z", cls);
    }
    std::snprintf(dtor, dtorSize, "??1%s@Jet@@UAE@XZ", cls);
    std::snprintf(vbase, vbaseSize, "??_D%s@Jet@@QAEXXZ", cls);
    std::snprintf(create, createSize, "?CreateInstance@%s@Jet@@UBAPAVMaterial@2@XZ", cls);
    if (k == NoTex || k == OneTex || k == Bump || k == BumpSpec)
        std::snprintf(init, initSize, "?InitInstance@%s@Jet@@IBAXAAV12@@Z", cls);
    else
        init[0] = 0;
    if (k == NoTex || k == OneTex || k == Bump || k == BumpSpec)
        std::snprintf(equals, equalsSize, "?Equals@%s@Jet@@IBA_NABV12@@Z", cls);
    else
        equals[0] = 0;
    std::snprintf(eq, eqSize, "??8%s@Jet@@UBA_NABVMaterial@1@@Z", cls);
}

struct Image {
    unsigned char b[0x100];
};

void MakeImage(Image& image, MaterialKind k)
{
    std::memset(image.b, 0xcd, sizeof image.b);
    std::memset(image.b + 0x24, 0, 0x10);
    std::memset(image.b + 0x34, 0, 0x44);
    std::memset(image.b + 0x88, 0, 12);
    *reinterpret_cast<void**>(image.b) = OrigData(
        (std::string("??_7") + MaterialName(k) + "@Jet@@6BMaterial@1@@").c_str());
    *reinterpret_cast<void**>(image.b + 0x98) = managerBytes;
    *reinterpret_cast<int*>(image.b + 0x78) = 4;
    *reinterpret_cast<int*>(image.b + 0x7c) = 5;
    *reinterpret_cast<float*>(image.b + 0x80) = 1.0f;
    image.b[0x84] = 0;
    *reinterpret_cast<float*>(image.b + 0x94) = 0.5f;
    image.b[0x14] = 0xa1;
    image.b[0x18] = 0xb2;
    image.b[0x1c] = 0xc3;
    *reinterpret_cast<void**>(image.b + 0x9c) = reinterpret_cast<void*>(0x1000u);
    *reinterpret_cast<void**>(image.b + 0xa0) = reinterpret_cast<void*>(0x1004u);
    *reinterpret_cast<void**>(image.b + 0xa4) = &fakeTextures[0];
    *reinterpret_cast<void**>(image.b + 0xa8) = &fakeTextures[1];
    image.b[0xac] = 0;
    image.b[0xad] = 0;
    image.b[0xae] = 0;
    image.b[0xaf] = 0;
    *reinterpret_cast<void**>(image.b + 0xb0) = &light0[0];
    *reinterpret_cast<void**>(image.b + 0xb4) = &light1[0];
    *reinterpret_cast<int*>(image.b + 0xb8) = 0;
    *reinterpret_cast<int*>(image.b + 0xbc) = 0;
    *reinterpret_cast<void**>(image.b + 0xc0) = nullptr;
    *reinterpret_cast<void**>(image.b + 0xc4) = &fakeTextures[2];
    *reinterpret_cast<int*>(image.b + 0xc8) = -1;
    *reinterpret_cast<void**>(image.b + 0xcc) = &fakeTextures[3];
    *reinterpret_cast<void**>(image.b + 0xd0) = &fakeTextures[3];
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u5b_rem <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 50;
    char fullDir[MAX_PATH] = {};
    GetFullPathNameA(argv[1], MAX_PATH, fullDir, nullptr);
    SetDllDirectoryA(fullDir);
    const std::string dir = fullDir;
    AddVectoredExceptionHandler(1, VectoredHandler);
    // The local proxies' static initialisers expect the renamed originals to be present already.
    for (const char* name : {"KernelStandard_7c_orig.dll", "Kernel_7c_orig.dll", "KernelResource_7c_orig.dll",
                             "KernelResources_7c_orig.dll", "RenderSystem_7c_orig.dll", "Render2DCore_7c_orig.dll"}) {
        if (!LoadLibraryA((dir + "\\" + name).c_str()))
            std::printf("preload %s failed %lu\n", name, GetLastError());
    }
    HMODULE origTry = LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str());
    if (!origTry) std::printf("orig load failed %lu\n", GetLastError());
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

    InitFakes();
    MakeEnvironment();

    Patch fetchPatch, createPatch;
    PatchEnter(fetchPatch,
               reinterpret_cast<void*>(GetProcAddress(
                   renderSystem, "?GetTexture@SharedTextureCollection@Jet@@SA_NW4SharedTextureID@2@PAVTextureManager@2@"
                                 "AAV?$AutoReference@VTexture@Jet@@@2@@Z")),
               reinterpret_cast<void*>(&HookGetTexture));
    PatchEnter(createPatch,
               reinterpret_cast<void*>(GetProcAddress(
                   renderSystem, "?CreateInstance@Material@Jet@@SAPAV12@ABVPString@2@PAVMaterialManager@2@@Z")),
               reinterpret_cast<void*>(&HookCreateInstance));

    typedef void*(__thiscall * Ctor5)(void*, void*, void*, void*, void*, void*, bool);
    typedef void*(__thiscall * Ctor6)(void*, void*, void*, void*, void*, void*, void*, bool);
    typedef void(__thiscall * DtorFn)(void*);
    typedef void*(__cdecl * CreateFn)(const void*);
    typedef void(__cdecl * InitFn)(const void*, void*);
    typedef bool(__cdecl * EqualsFn)(const void*, const void*);
    typedef bool(__cdecl * EqFn)(const void*, const void*);

    Ctor5 ctor5[2][6];
    Ctor6 ctor6[2][6];
    DtorFn dtor[2][6], vbase[2][6];
    CreateFn create[2][6];
    InitFn init[2][6];
    EqualsFn equals[2][6];
    EqFn eq[2][6];
    for (int k = 0; k < 6; ++k) {
        char ctor[256], dtorName[128], vbaseName[128], createName[160], initName[160], equalsName[160], eqName[160];
        ExportNames(materialKinds[k], ctor, sizeof ctor, dtorName, sizeof dtorName, vbaseName, sizeof vbaseName,
                    createName, sizeof createName, initName, sizeof initName, equalsName, sizeof equalsName, eqName,
                    sizeof eqName);
        for (int s = 0; s < 2; ++s) {
            HMODULE m = s ? orig : ours;
            const char* side = s ? "orig" : "ours";
            if (materialKinds[k] == NoTex) {
                ctor5[s][k] = Proc<Ctor5>(m, ctor, side);
                ctor6[s][k] = nullptr;
            } else {
                ctor6[s][k] = Proc<Ctor6>(m, ctor, side);
                ctor5[s][k] = nullptr;
            }
            dtor[s][k] = Proc<DtorFn>(m, dtorName, side);
            vbase[s][k] = Proc<DtorFn>(m, vbaseName, side);
            create[s][k] = Proc<CreateFn>(m, createName, side);
            init[s][k] = initName[0] ? Proc<InitFn>(m, initName, side) : nullptr;
            equals[s][k] = equalsName[0] ? Proc<EqualsFn>(m, equalsName, side) : nullptr;
            eq[s][k] = Proc<EqFn>(m, eqName, side);
        }
    }

    // plugin classes: {class, has a 17-arg named ctor}
    struct PluginClass {
        const char* name;
        bool named;
    };
    const PluginClass plugins[] = {
        {"MultiPassBumpMaterialPlugin", true},      {"MultiPassBump2MaterialPlugin", false},
        {"MultiPassBumpSpecMaterialPlugin", true},  {"MultiPassBumpSpec2MaterialPlugin", false},
        {"MultiPassNoTextureMaterialPlugin", false}, {"MultiPassOneTextureMaterialPlugin", true}};

    Image images[2][6];
    for (int k = 0; k < 6; ++k) {
        MakeImage(images[0][k], materialKinds[k]);
        MakeImage(images[1][k], materialKinds[k]);
    }

    for (int round = 0; round < rounds; ++round) {
        // ---- material constructors, destructors and virtual-base destructors ----
        for (int k = 0; k < 6; ++k) {
            alignas(16) unsigned char a[0x120], b[0x120];
            auto construct = [&](int side, unsigned char* dst) {
                ResetHooks();
                textureResult = &fakeTextures[0];
                std::memset(dst, 0, 0x120);
                ResetRefs(1000, 1000);
                if (materialKinds[k] == NoTex) {
                    ctor5[side][k](dst, managerBytes, cacheA[0].bytes, cacheB[0].bytes, &light0[0], &light1[0],
                                   true);
                } else {
                    ctor6[side][k](dst, managerBytes, cacheA[0].bytes, cacheB[0].bytes, &light0[0], &light1[0],
                                   &fakeTextures[2], true);
                }
                Check(textureFetches == (materialKinds[k] == NoTex ? 2 : materialKinds[k] == OneTex ? 3 : 4),
                      "ctor texture fetches");
            };
            construct(0, a);
            construct(1, b);
            CheckObject(a, b, materialSize[k], "material ctor");
            Check(*reinterpret_cast<int*>(cacheA[0].bytes + 0x10) == 1001, "chunk cache after ctor");
            Check(light0[0].refcount == 1001 && light1[0].refcount == 1001, "light after ctor");

            ResetRefs(1000, 1000);
            // the exported virtual destructor receives the virtual-base subobject
            dtor[0][k](a + materialVbase[k]);
            Check(*reinterpret_cast<int*>(cacheA[0].bytes + 0x10) == 999, "chunk cache after dtor");
            Check(light0[0].refcount == 999 && light1[0].refcount == 999, "light after dtor");
            ResetRefs(1000, 1000);
            dtor[1][k](b + materialVbase[k]);
            Check(*reinterpret_cast<int*>(cacheA[0].bytes + 0x10) == 999, "chunk cache after dtor orig");
            Check(light0[0].refcount == 999 && light1[0].refcount == 999, "light after dtor orig");
            CheckObject(a, b, materialSize[k], "material dtor");

            // and the vbase destructor entry, which takes the object start
            construct(0, a);
            construct(1, b);
            ResetRefs(1000, 1000);
            vbase[0][k](a);
            ResetRefs(1000, 1000);
            vbase[1][k](b);
            CheckObject(a, b, materialSize[k], "material vbase dtor");
        }

        // ---- CreateInstance and InitInstance ----
        for (int k = 0; k < 6; ++k) {
            ResetHooks();
            static unsigned char scratch[2][0x200];
            std::memset(scratch[0], 0, sizeof scratch[0]);
            std::memset(scratch[1], 0, sizeof scratch[1]);
            factoryResult = scratch[0];
            void* const got0 = create[0][k](images[0][k].b);
            const int calls0 = factoryCalls;
            const void* const type0 = factoryType;
            void* const manager0 = factoryManager;
            Check(got0 == scratch[0], "CreateInstance returns the factory object");
            ResetHooks();
            factoryResult = scratch[1];
            void* const got1 = create[1][k](images[1][k].b);
            Check(got1 == scratch[1], "CreateInstance orig returns the factory object");
            Check(calls0 == factoryCalls && type0 == factoryType && manager0 == factoryManager,
                  "CreateInstance call");
            CheckObject(scratch[0], scratch[1], 0xd4, "CreateInstance instance");
        }
        for (int k = 0; k < 6; ++k) {
            if (!init[0][k]) continue;
            alignas(16) unsigned char target0[0x120], target1[0x120];
            std::memset(target0, 0, sizeof target0);
            std::memset(target1, 0, sizeof target1);
            std::memcpy(target0, images[0][k].b, 0x100);
            std::memcpy(target1, images[1][k].b, 0x100);
            *reinterpret_cast<void**>(target0 + 0xa4) = &fakeTextures[0];
            *reinterpret_cast<void**>(target1 + 0xa4) = &fakeTextures[0];
            *reinterpret_cast<void**>(target0 + 0xa8) = nullptr;
            *reinterpret_cast<void**>(target1 + 0xa8) = nullptr;
            init[0][k](images[0][k].b, target0);
            init[1][k](images[1][k].b, target1);
            CheckObject(target0, target1, 0xd4, "InitInstance");
        }

        // ---- equality ----
        for (int k = 0; k < 6; ++k) {
            Check(eq[0][k](images[0][k].b, images[1][k].b) == eq[1][k](images[0][k].b, images[1][k].b),
                  "operator== equal images");
            Check(eq[0][k](images[0][k].b, images[0][k].b) == eq[1][k](images[0][k].b, images[0][k].b),
                  "operator== self");
            const int other = (k + 1 + Pick(0, 4)) % 6;
            {
                const bool e0 = eq[0][k](images[0][k].b, images[1][other].b);
                const bool e1 = eq[1][k](images[0][k].b, images[1][other].b);
                Check(e0 == e1, "operator== cross class");
            }
            if (equals[0][k]) {
                Check(equals[0][k](images[0][k].b, images[1][k].b) == equals[1][k](images[0][k].b, images[1][k].b),
                      "Equals equal");
                Image changed = images[1][k];
                changed.b[0xac] ^= 1;
                *reinterpret_cast<void**>(changed.b + 0xd0) = &fakeTextures[1];
                Check(equals[0][k](images[0][k].b, changed.b) == equals[1][k](images[0][k].b, changed.b),
                      "Equals changed");
            }
        }

        // ---- plugins (once; the base constructor registers in the global plugin list) ----
        if (round == 0) {
            typedef void*(__thiscall * PluginDefaultFn)(void*);
            typedef void*(__thiscall * PluginCopyFn)(void*, const void*);
            typedef void*(__thiscall * PluginNamedFn)(void*, const void*, const void*, const char*, const char*,
                                                      const char*, const char*, const char*, const char*, const char*,
                                                      const char*, const char*, const char*, const char*, const char*,
                                                      const char*, const char*, const char*);
            typedef void*(__thiscall * PluginAssignFn)(void*, const void*);
            typedef void(__thiscall * PluginDtorFn)(void*);
            for (const PluginClass& plugin : plugins) {
                const std::string base = std::string(plugin.name) + "@Jet@@";
                const std::string def = "??0" + base + "QAE@XZ";
                const std::string named = "??0" + base + "QAE@ABVPString@1@ABVUId@1@PBD22222222222222@Z";
                const std::string copy = "??0" + base + "QAE@ABV01@@Z";
                const std::string assign = "??4" + base + "QAEAAV01@ABV01@@Z";
                const std::string dtor = "??1" + base + "UAE@XZ";
                alignas(16) unsigned char a[0x90], b[0x90], source[2][0x90];
                PluginDefaultFn defFn[2];
                PluginCopyFn copyFn[2];
                PluginNamedFn namedFn[2];
                PluginAssignFn assignFn[2];
                PluginDtorFn dtorFn[2];
                for (int s = 0; s < 2; ++s) {
                    HMODULE m = s ? orig : ours;
                    defFn[s] = Proc<PluginDefaultFn>(m, def.c_str(), s ? "orig" : "ours");
                    copyFn[s] = Proc<PluginCopyFn>(m, copy.c_str(), s ? "orig" : "ours");
                    namedFn[s] = plugin.named ? Proc<PluginNamedFn>(m, named.c_str(), s ? "orig" : "ours") : nullptr;
                    assignFn[s] = Proc<PluginAssignFn>(m, assign.c_str(), s ? "orig" : "ours");
                    dtorFn[s] = Proc<PluginDtorFn>(m, dtor.c_str(), s ? "orig" : "ours");
                }
                std::memset(a, 0, sizeof a);
                std::memset(b, 0, sizeof b);
                defFn[0](a);
                defFn[1](b);
                CheckPlugin(a, b, "plugin default ctor");
                if (plugin.named) {
                    std::memset(a, 0, sizeof a);
                    std::memset(b, 0, sizeof b);
                    const void* const type = OrigData(MaterialType(Bump));
                    const void* const uid = OrigData("?UID@MultiPassBumpMaterial@Jet@@2VUId@2@B");
                    namedFn[0](a, type, uid, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                               nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                    namedFn[1](b, type, uid, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                               nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                    CheckPlugin(a, b, "plugin named ctor");
                }
                std::memset(source[0], 0, sizeof source[0]);
                std::memset(source[1], 0, sizeof source[1]);
                defFn[0](source[0]);
                defFn[1](source[1]);
                std::memset(a, 0, sizeof a);
                std::memset(b, 0, sizeof b);
                copyFn[0](a, source[0]);
                copyFn[1](b, source[1]);
                CheckPlugin(a, b, "plugin copy ctor");
                std::memset(a, 0, sizeof a);
                std::memset(b, 0, sizeof b);
                defFn[0](a);
                defFn[1](b);
                assignFn[0](a, source[0]);
                assignFn[1](b, source[1]);
                CheckPlugin(a, b, "plugin assignment");
                // destroy the default objects (no cache references in them)
                std::memset(a, 0, sizeof a);
                std::memset(b, 0, sizeof b);
                defFn[0](a);
                defFn[1](b);
                dtorFn[0](a);
                dtorFn[1](b);
                CheckPlugin(a, b, "plugin dtor");
            }
        }

        // ---- geometry-chunk constructors and destructors ----
        if (round == 0) {
            typedef void*(__thiscall * ChunkCtorFn)(void*, unsigned, unsigned, unsigned, unsigned, unsigned);
            typedef void(__thiscall * ChunkDtorFn)(void*);
            const char* chunkCtor[2] = {
                "??0MultiPassBumpChunkT@Jet@@QAE@IIIII@Z", "??0MultiPassBumpChunkT2@Jet@@QAE@IIIII@Z"};
            const char* chunkDtor[2] = {
                "??1MultiPassBumpChunkT@Jet@@UAE@XZ", "??1MultiPassBumpChunkT2@Jet@@UAE@XZ"};
            for (int c = 0; c < 2; ++c) {
                ChunkCtorFn ctor[2];
                ChunkDtorFn dtorFn[2];
                for (int s = 0; s < 2; ++s) {
                    HMODULE m = s ? orig : ours;
                    ctor[s] = Proc<ChunkCtorFn>(m, chunkCtor[c], s ? "orig" : "ours");
                    dtorFn[s] = Proc<ChunkDtorFn>(m, chunkDtor[c], s ? "orig" : "ours");
                }
                alignas(16) unsigned char a[0x100], b[0x100];
                std::memset(a, 0, sizeof a);
                std::memset(b, 0, sizeof b);
                ctor[0](a, 1, 1, 1, 1, 1);
                ctor[1](b, 1, 1, 1, 1, 1);
                CheckChunkCtor(a, b);
                dtorFn[0](a);
                dtorFn[1](b);
                CheckChunkDtor(a, b);
            }
        }
    }

    PatchRestore(createPatch);
    PatchRestore(fetchPatch);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
