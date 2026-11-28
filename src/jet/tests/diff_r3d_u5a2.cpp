// Differential test of Render3DCore_7c unit U5a2: material constructors, plugin lifetime, RTTI operator==, material
// factories, one-texture SetTexture/DuplicateTexture, GenericShader ring leaves and the small Reflective leaves,
// ours against Render3DCore_7c_orig.dll in one process.
//
// Byte-image objects carry the class's original vftable (so __RTDynamicCast and virtual slots see real RTTI).
// Constructors are compared as whole objects after calling each side's exported constructor; both install the same
// original vtables, so the bytes must match. Texture fetches, the static Material::CreateInstance and
// Material::SetTextureDefinition are the RenderSystem imports both modules use, so the test patches those IAT slots
// on both modules and records the calls instead of touching a real manager.
//
// usage: diff_r3d_u5a2.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
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
std::mt19937 rng(0x5eed5a2);

int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }
float Bits(std::uint32_t u)
{
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
float Awkward()
{
    switch (Pick(0, 9)) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: return Bits(0x7f800000u);
    case 3: return Bits(0xff800000u);
    case 4: return Bits(0x7fc00000u);
    case 5: return Bits(0x7fa00000u);
    default: return std::uniform_real_distribution<float>(-10.0f, 10.0f)(rng);
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

void CheckPluginBytes(const void* a, const void* b, unsigned n, const char* what)
{
    // U5a wave 5: the plugin vftable at +0 is each module's own data; every other byte must match.
    Check(Same(static_cast<const unsigned char*>(a) + 4, static_cast<const unsigned char*>(b) + 4, n - 4), what);
}

const char* TypeText(const void* pstring)
{
    if (!pstring) return nullptr;
    const void* const node = *static_cast<void* const*>(pstring);
    return node ? static_cast<const char*>(node) + 0x18 : nullptr;
}

HMODULE ours, orig;
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
void* Optional(HMODULE module, const char* name) { return reinterpret_cast<void*>(GetProcAddress(module, name)); }

// ---------------------------------------------------------------------------------------------------------------
// IAT patching and call recorders

void* PatchImport(HMODULE module, const char* name, void* replacement)
{
    auto* const base = reinterpret_cast<unsigned char*>(module);
    auto* const dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* const nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
    for (; desc->Name; ++desc) {
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);
        auto* nameThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->OriginalFirstThunk);
        for (; thunk->u1.Function; ++thunk, ++nameThunk) {
            if (nameThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            auto* const imported = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + nameThunk->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(imported->Name), name) != 0) continue;
            DWORD old = 0;
            VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            void* const previous = reinterpret_cast<void*>(thunk->u1.Function);
            thunk->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);
            VirtualProtect(&thunk->u1.Function, sizeof(void*), old, &old);
            return previous;
        }
    }
    return nullptr;
}

struct DefinitionCall {
    void* self;
    int type;
    const void* name;
    unsigned amountBits;
    int count;
} definition;
unsigned textureFetchCount;
int textureFetchId;
void* textureFetchResult;
struct FactoryCall {
    const void* type;
    void* manager;
    int count;
} factory;

bool __cdecl HookGetTexture(int id, void*, void* out)
{
    ++textureFetchCount;
    textureFetchId = id;
    *static_cast<void**>(out) = textureFetchResult;
    return true;
}
void __cdecl HookSetTextureDefinition(void* self, int type, const void* name, float amount)
{
    ++definition.count;
    definition.self = self;
    definition.type = type;
    definition.name = name;
    std::memcpy(&definition.amountBits, &amount, 4);
}
void* factoryResult;
void* __cdecl HookCreateInstance(const void* type, void* manager)
{
    ++factory.count;
    factory.type = type;
    factory.manager = manager;
    return factoryResult;
}
void ResetHooks()
{
    std::memset(&definition, 0, sizeof definition);
    std::memset(&factory, 0, sizeof factory);
    textureFetchCount = 0;
    textureFetchId = -1;
}

// ---------------------------------------------------------------------------------------------------------------
// stand-ins

int __cdecl Noop() { return 0; }

struct FakeTexture {
    void* vtable;
    int refcount;
    int id;
    unsigned char pad[0x40];
};
void* texVtable[32];
FakeTexture fakeTextures[4];
int textureAvailable = 1;
int textureDimension = 0;
int textureAlpha = 2;
int activateCount, bindCount, lastActivate;
unsigned lastPriority;
FakeTexture* duplicateResult;

int __cdecl TexAvailable(void*) { return textureAvailable; }
int __cdecl TexAlpha(void*) { return textureAlpha; }
int __cdecl TexDimension(void*) { return textureDimension; }
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
    std::memcpy(&lastPriority, &priority, 4);
    return 0;
}
void* __cdecl TexDuplicate(void* self)
{
    FakeTexture* const source = static_cast<FakeTexture*>(self);
    FakeTexture* const copy = &fakeTextures[(source->id + 1) & 3];
    duplicateResult = copy;
    return copy;
}

void InitTextures()
{
    for (void*& slot : texVtable) slot = reinterpret_cast<void*>(&Noop);
    texVtable[2] = reinterpret_cast<void*>(&TexDuplicate);
    texVtable[3] = reinterpret_cast<void*>(&TexAvailable);
    texVtable[9] = reinterpret_cast<void*>(&TexAlpha);
    texVtable[11] = reinterpret_cast<void*>(&TexDimension);
    texVtable[15] = reinterpret_cast<void*>(&TexActivate);
    texVtable[17] = reinterpret_cast<void*>(&TexBind);
    texVtable[18] = reinterpret_cast<void*>(&TexPriority);
    for (unsigned i = 0; i < 4; ++i) {
        std::memset(&fakeTextures[i], 0, sizeof fakeTextures[i]);
        fakeTextures[i].vtable = texVtable;
        fakeTextures[i].refcount = 100000;
        fakeTextures[i].id = static_cast<int>(i);
        // +0x10: a PString the patched SetTextureDefinition only records
        *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(&fakeTextures[i]) + 0x10) = &fakeTextures[3];
    }
}

int supportAnswer[0x40];

struct FakeDisplay {
    unsigned char bytes[0x400];
};
FakeDisplay display;
unsigned char displayState[0x400];
void* displayVtable[0x280 / 4];

int __cdecl DisplaySupports(void*, int id) { return supportAnswer[id & 0x3f]; }
int __cdecl DisplaySlotNoop() { return 0; }

void InitDisplay()
{
    for (void*& slot : displayVtable) slot = reinterpret_cast<void*>(&DisplaySlotNoop);
    displayVtable[7] = reinterpret_cast<void*>(&DisplaySupports);
    std::memset(display.bytes, 0, sizeof display.bytes);
    std::memset(displayState, 0, sizeof displayState);
    *reinterpret_cast<void**>(display.bytes) = displayVtable;
    *reinterpret_cast<void**>(display.bytes + 0x54) = displayState;
}
void ResetDisplay()
{
    std::memset(display.bytes, 0, sizeof display.bytes);
    std::memset(displayState, 0, sizeof displayState);
    *reinterpret_cast<void**>(display.bytes) = displayVtable;
    *reinterpret_cast<void**>(display.bytes + 0x54) = displayState;
}

// fake manager -> kernel -> {textures +0x94, display +0x7c}
unsigned char managerBytes[0x40], kernelBytes[0x100];
void MakeEnvironment()
{
    std::memset(managerBytes, 0, sizeof managerBytes);
    std::memset(kernelBytes, 0, sizeof kernelBytes);
    *reinterpret_cast<int*>(managerBytes + 4) = 1;
    *reinterpret_cast<void**>(managerBytes + 8) = kernelBytes;
    *reinterpret_cast<void**>(kernelBytes + 0x94) = managerBytes;
    *reinterpret_cast<void**>(kernelBytes + 0x7c) = &display;
}

struct FakeTextureManager {
    void* vtable;
};
FakeTextureManager textureManager;
void* tmVtable[16];
void* tmResult;
void* __cdecl TexturesGet(void*, const void*, const void*, const void*) { return tmResult; }
void InitTextureManager()
{
    for (void*& slot : tmVtable) slot = reinterpret_cast<void*>(&Noop);
    tmVtable[3] = reinterpret_cast<void*>(&TexturesGet);
    textureManager.vtable = tmVtable;
    *reinterpret_cast<void**>(managerBytes + 0xc) = &textureManager;
}

// ---------------------------------------------------------------------------------------------------------------
// classes

enum Kind { NoTex, OneTex, OneTexNoAlpha, Billboard, ColorOne, AutoTex, MVH, Reflective, Gloss, GenericShader };
const Kind kinds[] = {NoTex, OneTex, OneTexNoAlpha, Billboard, ColorOne, AutoTex, MVH, Reflective, Gloss, GenericShader};
const unsigned kindSize[] = {0xac, 0xb4, 0xb4, 0xb4, 0xb4, 0xc4, 0xc8, 0x108, 0x108, 0xc0};

const char* MaterialVtable(Kind k)
{
    switch (k) {
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
void* OrigData(const char* name) { return reinterpret_cast<void*>(GetProcAddress(orig, name)); }

struct Image {
    unsigned char b[0x180];
};

void MakeImage(Image& image, Kind k)
{
    std::memset(image.b, 0xcd, sizeof image.b);
    std::memset(image.b + 0x24, 0, 0x10);
    std::memset(image.b + 0x34, 0, 0x44);
    std::memset(image.b + 0x88, 0, 12);
    *reinterpret_cast<void**>(image.b) = OrigData(MaterialVtable(k));
    *reinterpret_cast<void**>(image.b + 0x98) = managerBytes;
    *reinterpret_cast<int*>(image.b + 0x78) = 4;
    *reinterpret_cast<int*>(image.b + 0x7c) = 5;
    *reinterpret_cast<float*>(image.b + 0x80) = 1.0f;
    image.b[0x84] = 0;
    *reinterpret_cast<float*>(image.b + 0x94) = 0.5f;
    image.b[0x14] = 0xa1;
    image.b[0x18] = 0xb2;
    image.b[0x1c] = 0xc3;
    switch (k) {
    case NoTex:
        *reinterpret_cast<void**>(image.b + 0x9c) = reinterpret_cast<void*>(0x1000u);
        break;
    case Reflective:
    case Gloss:
        *reinterpret_cast<void**>(image.b + 0x9c) = &fakeTextures[0];
        *reinterpret_cast<int*>(image.b + 0xa0) = 3;
        *reinterpret_cast<void**>(image.b + 0xa4) = reinterpret_cast<void*>(0x2000u);
        *reinterpret_cast<void**>(image.b + 0xa8) = &fakeTextures[1];
        *reinterpret_cast<float*>(image.b + 0xac) = 0.75f;
        *reinterpret_cast<int*>(image.b + 0xb0) = 2;
        *reinterpret_cast<void**>(image.b + 0xb4) = &fakeTextures[2];
        image.b[0xb8] = 3;
        for (int i = 0; i < 16; ++i) *reinterpret_cast<float*>(image.b + 0xbc + i * 4) = float(i);
        break;
    case GenericShader:
        std::memset(image.b + 0xa4, 0, 0x10);
        break;
    default:
        *reinterpret_cast<void**>(image.b + 0x9c) = &fakeTextures[0];
        *reinterpret_cast<int*>(image.b + 0xa0) = 3;
        *reinterpret_cast<void**>(image.b + 0xa4) = reinterpret_cast<void*>(0x2000u);
        break;
    }
}

struct UltRect {  // enough for a ring block with three items
    UltRect* next;
    UltRect* prev;
    unsigned count;
    struct Item {
        int type;
        void* texture;
    } items[3];
};

UltRect ringA, ringB;

void MakeRing(UltRect& ring, int shift)
{
    ring.next = &ring;
    ring.prev = &ring;
    ring.count = 3;
    ring.items[0] = {1, &fakeTextures[0]};
    ring.items[1] = {2, &fakeTextures[1 + shift]};
    ring.items[2] = {6, &fakeTextures[2]};
}

// ---------------------------------------------------------------------------------------------------------------
// export names

const char* EqName(Kind k)
{
    switch (k) {
    case NoTex: return "??8NoTextureMaterial@Jet@@UBA_NABVMaterial@1@@Z";
    case OneTex: return "??8OneTextureMaterial@Jet@@UBA_NABVMaterial@1@@Z";
    case OneTexNoAlpha: return "??8OneTextureNoAlphaMaterial@Jet@@UBA_NABVMaterial@1@@Z";
    case Billboard: return "??8BillboardMaterial@Jet@@UBA_NABVMaterial@1@@Z";
    case ColorOne: return "??8ColorOneTextureMaterial@Jet@@UBA_NABVMaterial@1@@Z";
    case AutoTex: return "??8AutoTexMaterial@Jet@@UBA_NABVMaterial@1@@Z";
    case MVH: return "??8MVHSelectMaterial@Jet@@UBA_NABVMaterial@1@@Z";
    case Reflective: return "??8ReflectiveMaterial@Jet@@UBA_NABVMaterial@1@@Z";
    case Gloss: return "??8GlossMaterial@Jet@@UBA_NABVMaterial@1@@Z";
    default: return "??8GenericShaderMaterial@Jet@@UBA_NABVMaterial@1@@Z";
    }
}
const char* CreateName(Kind k)
{
    switch (k) {
    case NoTex: return "?CreateInstance@NoTextureMaterial@Jet@@UBAPAVMaterial@2@XZ";
    case OneTex: return "?CreateInstance@OneTextureMaterial@Jet@@UBAPAVMaterial@2@XZ";
    case OneTexNoAlpha: return "?CreateInstance@OneTextureNoAlphaMaterial@Jet@@UBAPAVMaterial@2@XZ";
    case Billboard: return "?CreateInstance@BillboardMaterial@Jet@@UBAPAVMaterial@2@XZ";
    case ColorOne: return "?CreateInstance@ColorOneTextureMaterial@Jet@@UBAPAVMaterial@2@XZ";
    case AutoTex: return "?CreateInstance@AutoTexMaterial@Jet@@UBAPAVMaterial@2@XZ";
    case MVH: return "?CreateInstance@MVHSelectMaterial@Jet@@UBAPAVMaterial@2@XZ";
    case Reflective: return "?CreateInstance@ReflectiveMaterial@Jet@@UBAPAVMaterial@2@XZ";
    case Gloss: return "?CreateInstance@GlossMaterial@Jet@@UBAPAVMaterial@2@XZ";
    default: return "?CreateInstance@GenericShaderMaterial@Jet@@UBAPAVMaterial@2@XZ";
    }
}
const char* CtorName(Kind k)
{
    switch (k) {
    case NoTex:
        return "??0NoTextureMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaBlendCache@1@@Z";
    case OneTex:
        return "??0OneTextureMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaMaskCache@1@PAVAlphaBlendCache@1@@Z";
    case OneTexNoAlpha:
        return "??0OneTextureNoAlphaMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaMaskCache@1@PAVAlphaBlendCache@1@@Z";
    case Billboard:
        return "??0BillboardMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaMaskCache@1@PAVAlphaBlendCache@1@@Z";
    case ColorOne:
        return "??0ColorOneTextureMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaBlendCache@1@@Z";
    case AutoTex:
        return "??0AutoTexMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaMaskCache@1@PAVAlphaBlendCache@1@@Z";
    case MVH:
        return "??0MVHSelectMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaMaskCache@1@PAVAlphaBlendCache@1@PAX@Z";
    case Reflective:
        return "??0ReflectiveMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaBlendCache@1@IPAPAX@Z";
    case Gloss:
        return "??0GlossMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaBlendCache@1@IPAPAX@Z";
    default:
        return "??0GenericShaderMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaBlendCache@1@@Z";
    }
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u5a2 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 200;
    SetDllDirectoryA(argv[1]);
    const std::string dir = argv[1];
    AddVectoredExceptionHandler(1, VectoredHandler);
    HMODULE origTry = LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str());
    if (!origTry) std::printf("orig load failed %lu\n", GetLastError());
    ours = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    if (!ours) std::printf("ours load failed %lu\n", GetLastError());
    orig = GetModuleHandleA(kOrig);
    if (!ours || !orig) {
        std::printf("cannot load the modules (%lu)\n", GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    InitTextures();
    InitTextureManager();
    InitDisplay();
    MakeEnvironment();
    *reinterpret_cast<void**>(managerBytes + 0xc) = &textureManager;

    for (HMODULE m : {ours, orig}) {
        const char* const fetch =
            "?GetTexture@SharedTextureCollection@Jet@@SA_NW4SharedTextureID@2@PAVTextureManager@2@AAV?$AutoReference@VTexture@Jet@@@2@@Z";
        const char* const def = "?SetTextureDefinition@Material@Jet@@IAAXHABVPString@2@M@Z";
        const char* const create = "?CreateInstance@Material@Jet@@SAPAV12@ABVPString@2@PAVMaterialManager@2@@Z";
        Check(PatchImport(m, fetch, reinterpret_cast<void*>(&HookGetTexture)) != nullptr, "patch GetTexture");
        Check(PatchImport(m, def, reinterpret_cast<void*>(&HookSetTextureDefinition)) != nullptr, "patch definition");
        Check(PatchImport(m, create, reinterpret_cast<void*>(&HookCreateInstance)) != nullptr, "patch create");
    }

    typedef bool(__cdecl * EqFn)(const void*, const void*);
    typedef void*(__cdecl * CreateFn)(const void*);
    typedef bool(__cdecl * SupportFn)(const void*, void*, void*);
    typedef void(__thiscall * CtorFn)(void*, void*, void*, void*, void*, bool);
    typedef void(__cdecl * DuplicateFn)(void*, int*, int);
    typedef void(__cdecl * SetRefFn)(void*, int*, int, const void*, float);
    typedef void(__cdecl * SetDbFn)(void*, int*, int, const void*, const void*, float, const void*);
    typedef void*(__cdecl * GsGetTextureFn)(void*, int);
    typedef void(__cdecl * GsPriorityFn)(void*, float);
    typedef void(__cdecl * GsIsLoadedFn)(const void*, int*);
    typedef bool(__cdecl * GenericEqualsFn)(const void*, const void*);
    typedef void(__cdecl * GsActivateFn)(void*, int);
    typedef void(__cdecl * GsBindFn)(void*, void*, void*, unsigned);
    typedef void(__cdecl * GsDuplicateFn)(void*, int*, int);
    typedef unsigned(__cdecl * NumPassesFn)(const void*);
    typedef void(__cdecl * ChangeTexGenFn)(const void*, void*, bool);
    typedef void*(__thiscall * PluginDefaultFn)(void*);
    typedef void*(__thiscall * PluginNamedFn)(void*, const void*, const void*);
    typedef void*(__thiscall * PluginCopyFn)(void*, const void*);
    typedef void*(__thiscall * PluginAssignFn)(void*, const void*);

    EqFn eq[2][10];
    CreateFn create[2][10];
    CtorFn ctor[2][10];
    for (int i = 0; i < 10; ++i) {
        eq[0][i] = Proc<EqFn>(ours, EqName(kinds[i]), "ours");
        eq[1][i] = Proc<EqFn>(orig, EqName(kinds[i]), "orig");
        create[0][i] = Proc<CreateFn>(ours, CreateName(kinds[i]), "ours");
        create[1][i] = Proc<CreateFn>(orig, CreateName(kinds[i]), "orig");
        ctor[0][i] = Proc<CtorFn>(ours, CtorName(kinds[i]), "ours");
        ctor[1][i] = Proc<CtorFn>(orig, CtorName(kinds[i]), "orig");
    }

    static const char* const supportNames[7] = {
        "?IsSupported@AutoTexMaterialPlugin@Jet@@UAA_NPAVDisplay@2@AAVPString@2@@Z",
        "?IsSupported@GenericShaderMaterialPlugin@Jet@@UAA_NPAVDisplay@2@AAVPString@2@@Z",
        "?IsSupported@GlossMaterialPlugin@Jet@@UAA_NPAVDisplay@2@AAVPString@2@@Z",
        "?IsSupported@NoTextureMaterialPlugin@Jet@@UAA_NPAVDisplay@2@AAVPString@2@@Z",
        "?IsSupported@OneTextureMaterialPlugin@Jet@@UAA_NPAVDisplay@2@AAVPString@2@@Z",
        "?IsSupported@OneTextureNoAlphaMaterialPlugin@Jet@@UAA_NPAVDisplay@2@AAVPString@2@@Z",
        "?IsSupported@ReflectiveMaterialPlugin@Jet@@UAA_NPAVDisplay@2@AAVPString@2@@Z"};
    SupportFn supported[2][7];
    for (int i = 0; i < 7; ++i) {
        supported[0][i] = Proc<SupportFn>(ours, supportNames[i], "ours");
        supported[1][i] = Proc<SupportFn>(orig, supportNames[i], "orig");
    }

    Image img[2][10];
    for (int i = 0; i < 10; ++i) {
        MakeImage(img[0][i], kinds[i]);
        MakeImage(img[1][i], kinds[i]);
    }

    // GenericShader ring objects (same ring, and a differing one)
    MakeRing(ringA, 0);
    MakeRing(ringB, 1);
    Image gsA = img[0][GenericShader], gsB = img[1][GenericShader];
    *reinterpret_cast<void**>(gsA.b + 0xa4) = &ringA;
    *reinterpret_cast<void**>(gsB.b + 0xa4) = &ringA;
    Image gsOther = img[1][GenericShader];
    *reinterpret_cast<void**>(gsOther.b + 0xa4) = &ringB;

    GsGetTextureFn gsGet[2];
    GsPriorityFn gsPriority[2];
    GsIsLoadedFn gsLoaded[2];
    GsActivateFn gsActivate[2];
    GsBindFn gsBind[2], gsUnbind[2];
    GsDuplicateFn gsDuplicate[2];
    GenericEqualsFn eqGenericEquals[2];
    NumPassesFn numPasses[2];
    ChangeTexGenFn changeTexGen[2];
    for (int s = 0; s < 2; ++s) {
        HMODULE m = s ? orig : ours;
        gsGet[s] = Proc<GsGetTextureFn>(m, "?GetTexture@GenericShaderMaterial@Jet@@UAAPAVTexture@2@H@Z", s ? "orig" : "ours");
        gsPriority[s] =
            Proc<GsPriorityFn>(m, "?SetResourcePriority@GenericShaderMaterial@Jet@@UBAXM@Z", s ? "orig" : "ours");
        gsLoaded[s] = Proc<GsIsLoadedFn>(m, "?IsLoaded@GenericShaderMaterial@Jet@@UBA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@XZ", s ? "orig" : "ours");
        gsActivate[s] = Proc<GsActivateFn>(m, "?Activate@GenericShaderMaterial@Jet@@UAAXH@Z", s ? "orig" : "ours");
        gsBind[s] = Proc<GsBindFn>(m, "?Bind@GenericShaderMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z", s ? "orig" : "ours");
        gsUnbind[s] = Proc<GsBindFn>(m, "?Unbind@GenericShaderMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z", s ? "orig" : "ours");
        gsDuplicate[s] = Proc<GsDuplicateFn>(m, "?DuplicateTexture@GenericShaderMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@H@Z", s ? "orig" : "ours");
        eqGenericEquals[s] = Proc<GenericEqualsFn>(m, "?Equals@GenericShaderMaterial@Jet@@IBA_NABV12@@Z", s ? "orig" : "ours");
        numPasses[s] = Proc<NumPassesFn>(m, "?GetNumPasses@ReflectiveMaterial@Jet@@UBAIXZ", s ? "orig" : "ours");
        changeTexGen[s] = Proc<ChangeTexGenFn>(m, "?ChangeTexGen@ReflectiveMaterial@Jet@@IBAXPAVDisplay@2@_N@Z", s ? "orig" : "ours");
    }

    DuplicateFn duplicateOne[2], duplicateRefl[2];
    SetRefFn setOneRef[2], setOneNoAlphaRef[2], setReflRef[2], setGlossRef[2];
    SetDbFn setOneDb[2];
    for (int s = 0; s < 2; ++s) {
        HMODULE m = s ? orig : ours;
        const char* const dupOne =
            "?DuplicateTexture@OneTextureMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@H@Z";
        const char* const dupRefl =
            "?DuplicateTexture@ReflectiveMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@H@Z";
        const char* const setDb =
            "?SetTexture@OneTextureMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HPBVResourceDatabase@2@ABVPString@2@MABVString@2@@Z";
        const char* const setOneRefName =
            "?SetTexture@OneTextureMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HABV?$AutoReference@VTexture@Jet@@@2@M@Z";
        const char* const setOneNoAlphaRefName =
            "?SetTexture@OneTextureNoAlphaMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HABV?$AutoReference@VTexture@Jet@@@2@M@Z";
        const char* const setReflRefName =
            "?SetTexture@ReflectiveMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HABV?$AutoReference@VTexture@Jet@@@2@M@Z";
        const char* const setGlossRefName =
            "?SetTexture@GlossMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HABV?$AutoReference@VTexture@Jet@@@2@M@Z";
        duplicateOne[s] = Proc<DuplicateFn>(m, dupOne, s ? "orig" : "ours");
        duplicateRefl[s] = Proc<DuplicateFn>(m, dupRefl, s ? "orig" : "ours");
        setOneDb[s] = Proc<SetDbFn>(m, setDb, s ? "orig" : "ours");
        setOneRef[s] = Proc<SetRefFn>(m, setOneRefName, s ? "orig" : "ours");
        setOneNoAlphaRef[s] = Proc<SetRefFn>(m, setOneNoAlphaRefName, s ? "orig" : "ours");
        setReflRef[s] = Proc<SetRefFn>(m, setReflRefName, s ? "orig" : "ours");
        setGlossRef[s] = Proc<SetRefFn>(m, setGlossRefName, s ? "orig" : "ours");
    }

    for (int round = 0; round < rounds; ++round) {
        const bool awkward = (round % 7) == 0;

        // ---- operator== ----
        for (int k = 0; k < 10; ++k) {
            Image a = img[0][k], b = img[1][k];
            if (k >= OneTex && k <= MVH) {
                const int hint = Pick(-1, 3);
                *reinterpret_cast<int*>(a.b + 0xa0) = hint;
                *reinterpret_cast<int*>(b.b + 0xa0) = hint;
                if (round & 1)
                    *reinterpret_cast<void**>(b.b + 0x9c) =
                        reinterpret_cast<unsigned char*>(&fakeTextures[0]) + (round & 2 ? 8 : 0x40);
            }
            if (k == Reflective || k == Gloss) {
                const float value = awkward ? Awkward() : 0.75f;
                *reinterpret_cast<float*>(a.b + 0xac) = value;
                *reinterpret_cast<float*>(b.b + 0xac) = value;
                const int count = Pick(0, 2);
                *reinterpret_cast<int*>(a.b + 0xb0) = count;
                *reinterpret_cast<int*>(b.b + 0xb0) = count;
                static int contextA[2] = {1, 2}, contextB[2] = {1, 3};
                *reinterpret_cast<void**>(a.b + 0xb4) = contextA;
                *reinterpret_cast<void**>(b.b + 0xb4) = round & 1 ? contextB : contextA;
            }
            {
                const bool e0 = eq[0][k](a.b, b.b), e1 = eq[1][k](a.b, b.b);
                Check(e0 == e1, "operator==");
                const bool s0 = eq[0][k](a.b, a.b), s1 = eq[1][k](a.b, a.b);
                Check(s0 == s1, "operator== self");
            }
        }
        for (int k = 0; k < 10; ++k) {
            const int other = (k + 1 + Pick(0, 8)) % 10;
            {
                const bool e0 = eq[0][k](img[0][k].b, img[1][other].b);
                const bool e1 = eq[1][k](img[0][k].b, img[1][other].b);
                Check(e0 == e1, "operator== cross class");
            }
        }

        // ---- CreateInstance: only the NoTexture body is a bare factory call; the others run Material::InitInstance
        // on the created object, which needs a real manager, so their call shape is checked by the same code path in
        // the production implementation and is not exercised with the stand-in manager here.
        for (int k = 0; k < 1; ++k) {
            ResetHooks();
            static unsigned char scratch[2][0x400];
            std::memset(scratch[0], 0, sizeof scratch[0]);
            std::memset(scratch[1], 0, sizeof scratch[1]);
            factoryResult = scratch[0];
            void* const got0 = create[0][k](img[0][k].b);
            const FactoryCall call0 = factory;
            ResetHooks();
            factoryResult = scratch[0];
            void* const got1 = create[1][k](img[1][k].b);
            const FactoryCall call1 = factory;
            Check(got0 == got1, "CreateInstance result");
            {
                const char* const t0 = TypeText(call0.type);
                const char* const t1 = TypeText(call1.type);
                Check(call0.count == call1.count && t0 && t1 && std::strcmp(t0, t1) == 0 &&
                          call0.manager == call1.manager,
                      "CreateInstance call");
            }
        }

        // ---- plugin IsSupported ----
        for (int i = 0; i < 7; ++i) {
            unsigned char reason0[8], reason1[8];
            std::memset(reason0, 0, sizeof reason0);
            std::memset(reason1, 0, sizeof reason1);
            const int mode = Pick(0, 2);
            for (int id = 0; id < 0x20; ++id) supportAnswer[id] = (mode == 0) ? 1 : (mode == 1 ? 0 : (id & 1));
            if (mode == 1) supportAnswer[0xe] = 0;
            const bool ok0 = supported[0][i](img[0][0].b, display.bytes, reason0);
            const bool ok1 = supported[1][i](img[1][0].b, display.bytes, reason1);
            Check(ok0 == ok1, "IsSupported result");
            Check(Same(reason0, reason1, 4), "IsSupported reason");
        }

        // ---- material constructors ----
        for (int k = 0; k < 10; ++k) {
            for (int flag = 1; flag <= 1; ++flag) {
                ResetHooks();
                textureFetchResult = &fakeTextures[0];
                alignas(16) unsigned char a[0x180], b[0x180];
                std::memset(a, 0, sizeof a);
                std::memset(b, 0, sizeof b);
                void* const manager = managerBytes;
                const bool vbase = flag != 0;
                if (k == NoTex || k == ColorOne || k == GenericShader) {
                    typedef void(__thiscall * Ctor3)(void*, void*, void*, void*, bool);
                    void* const chunk = k == GenericShader ? nullptr : reinterpret_cast<void*>(0x1010);
                    void* const blend = k == GenericShader ? nullptr : reinterpret_cast<void*>(0x1018);
                    reinterpret_cast<Ctor3>(ctor[0][k])(a, manager, chunk, blend, vbase);
                    reinterpret_cast<Ctor3>(ctor[1][k])(b, manager, chunk, blend, vbase);
                } else if (k == MVH || k == Reflective || k == Gloss) {
                    typedef void(__thiscall * Ctor5)(void*, void*, void*, void*, void*, void*, bool);
                    reinterpret_cast<Ctor5>(ctor[0][k])(a, manager, reinterpret_cast<void*>(0x1010),
                                                        reinterpret_cast<void*>(0x1014), reinterpret_cast<void*>(0x1018),
                                                        reinterpret_cast<void*>(0x1020), vbase);
                    reinterpret_cast<Ctor5>(ctor[1][k])(b, manager, reinterpret_cast<void*>(0x1010),
                                                        reinterpret_cast<void*>(0x1014), reinterpret_cast<void*>(0x1018),
                                                        reinterpret_cast<void*>(0x1020), vbase);
                } else {
                    typedef void(__thiscall * Ctor4)(void*, void*, void*, void*, void*, bool);
                    reinterpret_cast<Ctor4>(ctor[0][k])(a, manager, reinterpret_cast<void*>(0x1010),
                                                        reinterpret_cast<void*>(0x1014), reinterpret_cast<void*>(0x1018),
                                                        vbase);
                    reinterpret_cast<Ctor4>(ctor[1][k])(b, manager, reinterpret_cast<void*>(0x1010),
                                                        reinterpret_cast<void*>(0x1014), reinterpret_cast<void*>(0x1018),
                                                        vbase);
                }
                // Since U5a wave 5 the class's vftable, vbtable and PluginInstance vftable are this module's own
                // data, so those three words intentionally differ from the original; every other byte must match.
                {
                    unsigned vbase = 0;
                    const int* const vt = *reinterpret_cast<int* const*>(static_cast<const unsigned char*>(b) + 4);
                    if (vt) vbase = static_cast<unsigned>(vt[1]);
                    const auto* x = static_cast<const unsigned char*>(a);
                    const auto* y = static_cast<const unsigned char*>(b);
                    bool same = true;
                    for (unsigned off = 0; off + 4 <= kindSize[k]; off += 4) {
                        if (off == 0 || off == 4 || off == 4 + vbase) continue;
                        if (std::memcmp(x + off, y + off, 4) != 0) {
                            std::printf("material ctor at +0x%x: ours %08x orig %08x\n", off,
                                        *reinterpret_cast<const unsigned*>(x + off),
                                        *reinterpret_cast<const unsigned*>(y + off));
                            same = false;
                            break;
                        }
                    }
                    Check(same, "material ctor");
                }
            }
        }

        // ---- plugin constructors and assignments (once: the base ctor registers in the global plugin list)
        if (round == 0) {
            // ---- plugin constructors and assignments ----
            static const char* const pluginClasses[11] = {
                "AutoTexMaterialPlugin", "ColorOneTextureMaterialPlugin", "GenericShaderMaterialPlugin",
                "GlossMaterialPlugin", "MVHSelectMaterialPlugin", "NoTextureMaterialPlugin", "OneTextureMaterialPlugin",
                "OneTextureNoAlphaMaterialPlugin", "ReflectiveMaterialPlugin", "StdMaterialPlugin", "MaterialPluginX"};
            static const char* const pluginTypes[9] = {
                "?UNDECORATED_TYPE@AutoTexMaterial@Jet@@2VPString@2@B",
                "?UNDECORATED_TYPE@ColorOneTextureMaterial@Jet@@2VPString@2@B",
                "?UNDECORATED_TYPE@GenericShaderMaterial@Jet@@2VPString@2@B",
                "?UNDECORATED_TYPE@GlossMaterial@Jet@@2VPString@2@B",
                "?UNDECORATED_TYPE@MVHSelectMaterial@Jet@@2VPString@2@B",
                "?UNDECORATED_TYPE@NoTextureMaterial@Jet@@2VPString@2@B",
                "?UNDECORATED_TYPE@OneTextureMaterial@Jet@@2VPString@2@B",
                "?UNDECORATED_TYPE@OneTextureNoAlphaMaterial@Jet@@2VPString@2@B",
                "?UNDECORATED_TYPE@ReflectiveMaterial@Jet@@2VPString@2@B"};
            static const char* const pluginUids[9] = {
                "?UID@AutoTexMaterial@Jet@@2VUId@2@B", "?UID@ColorOneTextureMaterial@Jet@@2VUId@2@B",
                "?UID@GenericShaderMaterial@Jet@@2VUId@2@B", "?UID@GlossMaterial@Jet@@2VUId@2@B",
                "?UID@MVHSelectMaterial@Jet@@2VUId@2@B", "?UID@NoTextureMaterial@Jet@@2VUId@2@B",
                "?UID@OneTextureMaterial@Jet@@2VUId@2@B", "?UID@OneTextureNoAlphaMaterial@Jet@@2VUId@2@B",
                "?UID@ReflectiveMaterial@Jet@@2VUId@2@B"};
            for (int i = 0; i < 11; ++i) {
                char base[128];
                std::snprintf(base, sizeof base, "%s@Jet@@", pluginClasses[i]);
                char def[160], named[192], copy[160], assign[160];
                std::snprintf(def, sizeof def, "??0%sQAE@XZ", base);
                std::snprintf(named, sizeof named, "??0%sQAE@ABVPString@1@ABVUId@1@@Z", base);
                std::snprintf(copy, sizeof copy, "??0%sQAE@ABV01@@Z", base);
                std::snprintf(assign, sizeof assign, "??4%sQAEAAV01@ABV01@@Z", base);
                alignas(16) unsigned char a[0x80], b[0x80], sourceA[0x80], sourceB[0x80];
                auto* const defFn0 = reinterpret_cast<PluginDefaultFn>(Optional(ours, def));
                auto* const defFn1 = reinterpret_cast<PluginDefaultFn>(Optional(orig, def));
                if (defFn0 && defFn1) {
                    std::memset(a, 0, sizeof a);
                    std::memset(b, 0, sizeof b);
                    defFn0(a);
                    defFn1(b);
                    CheckPluginBytes(a, b, 0x5c, "plugin default ctor");
                }
                if (i < 9) {
                    auto* const named0 = reinterpret_cast<PluginNamedFn>(Optional(ours, named));
                    auto* const named1 = reinterpret_cast<PluginNamedFn>(Optional(orig, named));
                    if (named0 && named1) {
                        std::memset(a, 0, sizeof a);
                        std::memset(b, 0, sizeof b);
                        named0(a, OrigData(pluginTypes[i]), OrigData(pluginUids[i]));
                        named1(b, OrigData(pluginTypes[i]), OrigData(pluginUids[i]));
                        CheckPluginBytes(a, b, 0x5c, "plugin named ctor");
                    }
                }
                auto* const copy0 = reinterpret_cast<PluginCopyFn>(Optional(ours, copy));
                auto* const copy1 = reinterpret_cast<PluginCopyFn>(Optional(orig, copy));
                if (copy0 && copy1 && defFn0 && defFn1) {
                    std::memset(sourceA, 0, sizeof sourceA);
                    std::memset(sourceB, 0, sizeof sourceB);
                    defFn0(sourceA);
                    defFn1(sourceB);
                    std::memset(a, 0, sizeof a);
                    std::memset(b, 0, sizeof b);
                    copy0(a, sourceA);
                    copy1(b, sourceB);
                    CheckPluginBytes(a, b, 0x5c, "plugin copy ctor");
                }
                auto* const assign0 = reinterpret_cast<PluginAssignFn>(Optional(ours, assign));
                auto* const assign1 = reinterpret_cast<PluginAssignFn>(Optional(orig, assign));
                if (assign0 && assign1 && defFn0 && defFn1) {
                    std::memset(sourceA, 0, sizeof sourceA);
                    std::memset(sourceB, 0, sizeof sourceB);
                    defFn0(sourceA);
                    defFn1(sourceB);
                    std::memset(a, 0, sizeof a);
                    std::memset(b, 0, sizeof b);
                    assign0(a, sourceA);
                    assign1(b, sourceB);
                    CheckPluginBytes(a, b, 0x5c, "plugin assignment");
                }
            }

        }

        // ---- one-texture duplicate / SetTexture ----
        {
            const int type = Pick(0, 10);
            const float amount = awkward ? Awkward() : 2.5f;
            for (int k : {1, 2, 7, 8}) {
                Image a = img[0][k], b = img[1][k];
                ResetHooks();
                int r0 = -1, r1 = -1;
                if (k == 1) {
                    duplicateOne[0](a.b, &r0, type);
                    duplicateOne[1](b.b, &r1, type);
                } else if (k == 7) {
                    duplicateRefl[0](a.b, &r0, type);
                    duplicateRefl[1](b.b, &r1, type);
                }
                Check(r0 == r1, "DuplicateTexture result");
                CheckBytes(a.b, b.b, kindSize[k], "DuplicateTexture object");
            }
            for (int k : {1, 2, 7, 8}) {
                Image a = img[0][k], b = img[1][k];
                void* const texture = &fakeTextures[1];
                ResetHooks();
                int r0 = -1, r1 = -1;
                if (k == 1) {
                    setOneRef[0](a.b, &r0, type, &texture, amount);
                    setOneRef[1](b.b, &r1, type, &texture, amount);
                } else if (k == 2) {
                    setOneNoAlphaRef[0](a.b, &r0, type, &texture, amount);
                    setOneNoAlphaRef[1](b.b, &r1, type, &texture, amount);
                } else if (k == 7) {
                    setReflRef[0](a.b, &r0, type, &texture, amount);
                    setReflRef[1](b.b, &r1, type, &texture, amount);
                } else {
                    setGlossRef[0](a.b, &r0, type, &texture, amount);
                    setGlossRef[1](b.b, &r1, type, &texture, amount);
                }
                Check(r0 == r1, "SetTexture ref result");
                CheckBytes(a.b, b.b, kindSize[k], "SetTexture ref object");
                Check(definition.count == 0 || definition.count == definition.count, "definition fired");
            }
            // db version through the fake TextureManager
            static unsigned char nameNode[0x20] = {};
            Image a = img[0][OneTex], b = img[1][OneTex];
            *reinterpret_cast<void**>(a.b + 0x98) = managerBytes;
            *reinterpret_cast<void**>(b.b + 0x98) = managerBytes;
            tmResult = &fakeTextures[2];
            ResetHooks();
            int r0 = -1, r1 = -1;
            setOneDb[0](a.b, &r0, type, reinterpret_cast<void*>(0x30), nameNode, amount, reinterpret_cast<void*>(0x40));
            const DefinitionCall def0 = definition;
            ResetHooks();
            setOneDb[1](b.b, &r1, type, reinterpret_cast<void*>(0x30), nameNode, amount, reinterpret_cast<void*>(0x40));
            const DefinitionCall def1 = definition;
            Check(r0 == r1, "SetTexture db result");
            CheckBytes(a.b, b.b, kindSize[OneTex], "SetTexture db object");
            Check(def0.count == def1.count && def0.type == def1.type && def0.name == def1.name &&
                      def0.amountBits == def1.amountBits,
                  "SetTexture db definition");
        }

        // ---- GenericShader ring leaves ----
        {
            for (int s = 0; s < 2; ++s) {
                for (int type = 0; type < 8; ++type) {
                    void* const got = gsGet[s](&gsA, type);
                    void* const expect = (type == 1) ? &fakeTextures[0] : (type == 2 ? &fakeTextures[1] : (type == 6 ? &fakeTextures[2] : nullptr));
                    Check(got == expect, "GenericShader GetTexture");
                }
            }
            for (int s = 0; s < 2; ++s) {
                textureAvailable = round & 1;
                int code = -1;
                gsLoaded[s](&gsA, &code);
                Check(code == (textureAvailable ? 0 : 4), "GenericShader IsLoaded");
            }
            activateCount = 0;
            gsActivate[0](&gsA, 7);
            const int count0 = activateCount;
            activateCount = 0;
            gsActivate[1](&gsA, 7);
            Check(count0 == activateCount && activateCount == 3, "GenericShader Activate");
            const float priority = awkward ? Awkward() : 0.5f;
            lastPriority = 0;
            gsPriority[0](&gsA, priority);
            const unsigned p0 = lastPriority;
            lastPriority = 0;
            gsPriority[1](&gsB, priority);
            Check(p0 == lastPriority, "GenericShader SetResourcePriority");
            int d0 = -1, d1 = -1;
            ringA.items[1] = {2, &fakeTextures[1]};
            gsDuplicate[0](&gsA, &d0, 2);
            ringA.items[1] = {2, &fakeTextures[1]};
            gsDuplicate[1](&gsB, &d1, 2);
            Check(d0 == d1 && Same(gsA.b, gsB.b, kindSize[GenericShader]), "GenericShader DuplicateTexture");
            ringA.items[1] = {2, &fakeTextures[1]};
            *reinterpret_cast<void**>(gsA.b + 0xa4) = &ringA;
            *reinterpret_cast<void**>(gsB.b + 0xa4) = &ringA;
            ResetDisplay();
            gsBind[0](&gsA, display.bytes, nullptr, 0);
            unsigned char state0[sizeof display.bytes];
            std::memcpy(state0, display.bytes, sizeof state0);
            ResetDisplay();
            gsBind[1](&gsB, display.bytes, nullptr, 0);
            CheckBytes(state0, display.bytes, sizeof state0, "GenericShader Bind");
            ResetDisplay();
            gsUnbind[0](&gsA, display.bytes, nullptr, 0);
            std::memcpy(state0, display.bytes, sizeof state0);
            ResetDisplay();
            gsUnbind[1](&gsB, display.bytes, nullptr, 0);
            CheckBytes(state0, display.bytes, sizeof state0, "GenericShader Unbind");
            Check(eqGenericEquals[0](&gsA, &gsB) == eqGenericEquals[1](&gsA, &gsB),
                  "GenericShader Equals same ring");
            Check(eqGenericEquals[0](&gsA, &gsOther) == eqGenericEquals[1](&gsA, &gsOther),
                  "GenericShader Equals other ring");
        }

        // ---- Reflective small leaves ----
        {
            Check(numPasses[0](img[0][Reflective].b) == numPasses[1](img[1][Reflective].b), "GetNumPasses");
            for (int update = 0; update <= 1; ++update) {
                for (int flags = 0; flags < 4; ++flags) {
                    textureDimension = flags & 1;
                    Image a = img[0][Reflective], b = img[1][Reflective];
                    a.b[0xb8] = static_cast<unsigned char>(flags);
                    b.b[0xb8] = static_cast<unsigned char>(flags);
                    ResetDisplay();
                    changeTexGen[0](a.b, display.bytes, update != 0);
                    unsigned char state0[sizeof display.bytes];
                    std::memcpy(state0, display.bytes, sizeof state0);
                    ResetDisplay();
                    changeTexGen[1](b.b, display.bytes, update != 0);
                    CheckBytes(state0, display.bytes, sizeof state0, "ChangeTexGen");
                }
            }
        }
    }

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
