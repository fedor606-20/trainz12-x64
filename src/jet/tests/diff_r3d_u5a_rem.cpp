// Differential test of Render3DCore_7c unit U5a (wave 4): the render, cache, chunk-creation and display-state paths.
// Ours against Render3DCore_7c_orig.dll in one process. Every export is called through GetProcAddress on both
// modules with identical material/chunk/display images. Material::SetLightingState is replaced in both IATs with a
// recorder; the four U4 cache Add functions are detoured; the display and the object mutation are compared.
//
// usage: diff_r3d_u5a_rem.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

#include <jet/Math.h>

using Jet::Matrix4;

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
std::mt19937 rng(0x5eed5a4);
int gSide;

int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }
float Bits(std::uint32_t u)
{
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
float Awkward()
{
    switch (Pick(0, 11)) {
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
void CheckBytes(const void* a, const void* b, unsigned n, const char* what)
{
    if (std::memcmp(a, b, n) != 0) {
        const auto* x = static_cast<const unsigned char*>(a);
        const auto* y = static_cast<const unsigned char*>(b);
        for (unsigned i = 0; i < n; i += 4)
            if (std::memcmp(x + i, y + i, 4) != 0) {
                std::printf("%s at +0x%x: ours %08x orig %08x\n", what, i,
                            *reinterpret_cast<const unsigned*>(x + i), *reinterpret_cast<const unsigned*>(y + i));
                break;
            }
    }
    Check(std::memcmp(a, b, n) == 0, what);
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
void* Optional(HMODULE module, const char* name) { return reinterpret_cast<void*>(GetProcAddress(module, name)); }

// ---------------------------------------------------------------------------------------------------------------
// IAT patching and detours

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

void Detour(void* target, void* replacement)
{
    if (!target) return;
    DWORD old = 0;
    VirtualProtect(target, 8, PAGE_EXECUTE_READWRITE, &old);
    auto* const code = static_cast<unsigned char*>(target);
    code[0] = 0xe9;
    *reinterpret_cast<std::int32_t*>(code + 1) =
        static_cast<std::int32_t>(reinterpret_cast<unsigned char*>(replacement) - code - 5);
    VirtualProtect(target, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 8);
}

// ---------------------------------------------------------------------------------------------------------------
// fake Display

int __cdecl Noop() { return 0; }
unsigned char displayBytes[0x280];
unsigned char displayState[0x400];
void* displayVtable[0x280 / 4];

void ResetDisplay()
{
    std::memset(displayBytes, 0, sizeof displayBytes);
    std::memset(displayState, 0, sizeof displayState);
    *reinterpret_cast<void**>(displayBytes) = displayVtable;
    *reinterpret_cast<void**>(displayBytes + 0x54) = displayState;
}

// ---------------------------------------------------------------------------------------------------------------
// fake textures

struct FakeTexture {
    void* vtable;
    int refcount;
    int id;
    unsigned char pad[0x40];
};
void* texVtable[32];
void* texManagerVtable[32];
FakeTexture fakeTextures[4];
int texAvailable = 1, texDimension = 0, texAlpha = 1;
int texBindCount, texPriorityCount;
unsigned texLastPriority;
FakeTexture* texCreated;

int __cdecl TexAvailable(void*) { return texAvailable; }
int __cdecl TexAlpha(void*) { return texAlpha; }
int __cdecl TexDimension(void*) { return texDimension; }
int __cdecl TexBind(void*, unsigned) { ++texBindCount; return 0; }
int __cdecl TexPriority(void*, float p) { ++texPriorityCount; std::memcpy(&texLastPriority, &p, 4); return 0; }
void* __cdecl TexCreate(void*, const void*, const char*, const void*)
{
    texCreated = &fakeTextures[2];
    ++fakeTextures[2].refcount;
    return &fakeTextures[2];
}
void InitTextures()
{
    for (void*& slot : texVtable) slot = reinterpret_cast<void*>(&Noop);
    for (void*& slot : texManagerVtable) slot = reinterpret_cast<void*>(&Noop);
    texVtable[3] = reinterpret_cast<void*>(&TexAvailable);
    texVtable[9] = reinterpret_cast<void*>(&TexAlpha);
    texVtable[11] = reinterpret_cast<void*>(&TexDimension);
    texVtable[17] = reinterpret_cast<void*>(&TexBind);
    texVtable[18] = reinterpret_cast<void*>(&TexPriority);
    texManagerVtable[3] = reinterpret_cast<void*>(&TexCreate);
    for (unsigned i = 0; i < 4; ++i) {
        std::memset(&fakeTextures[i], 0, sizeof fakeTextures[i]);
        fakeTextures[i].vtable = texVtable;
        fakeTextures[i].refcount = 100000;
        fakeTextures[i].id = static_cast<int>(i);
        // +0x10 is the texture name PString used by the reference SetTexture overload
        *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(&fakeTextures[i]) + 0x10) = &fakeTextures[3];
    }
}

// ---------------------------------------------------------------------------------------------------------------
// recorders

struct SLSArgs {
    const void* self;
    const void* display;
    const void* modelView;
    const void* chunk;
    const void* program;
    const void* lights;
    const void* view;
    unsigned opacity;
    int unk;
    unsigned mat1[16];
    unsigned mat2[16];
    int has1, has2;
};
SLSArgs sls[2][8];
int slsCount[2];

void __cdecl HookSLS(const void* self, const void* display, const void* modelView, const void* chunk,
                     const void* program, const void* lights, const void* view, float opacity, bool unk)
{
    const int s = gSide;
    if (slsCount[s] < 8) {
        SLSArgs& a = sls[s][slsCount[s]++];
        a.self = self;
        a.display = display;
        a.modelView = modelView;
        a.chunk = chunk;
        a.program = program;
        a.lights = lights;
        a.view = view;
        std::memcpy(&a.opacity, &opacity, 4);
        a.unk = unk ? 1 : 0;
        a.has1 = modelView != nullptr;
        a.has2 = view != nullptr;
        if (modelView) std::memcpy(a.mat1, modelView, sizeof a.mat1);
        else std::memset(a.mat1, 0, sizeof a.mat1);
        if (view) std::memcpy(a.mat2, view, sizeof a.mat2);
        else std::memset(a.mat2, 0, sizeof a.mat2);
    }
}

struct DefArgs {
    const void* self;
    int type;
    const void* name;
    unsigned amount;
};
DefArgs defs[2][8];
int defCount[2];
void __cdecl HookDef(const void* self, int type, const void* name, float amount)
{
    const int s = gSide;
    if (defCount[s] < 8) {
        DefArgs& a = defs[s][defCount[s]++];
        a.self = self;
        a.type = type;
        a.name = name;
        std::memcpy(&a.amount, &amount, 4);
    }
}

struct CacheArgs {
    const void* cache;
    const void* display;
    const void* view;
    const void* material;
    const void* chunk;
    const void* lights;
    const void* modelView;
    unsigned distance;
};
CacheArgs cacheCalls[2][8];
int cacheCount[2];

void RecordCache(const void* cache, const void* display, const void* view, const void* material, const void* chunk,
                 const void* lights, const void* modelView, float distance = 0.0f)
{
    const int s = gSide;
    if (cacheCount[s] < 8) {
        CacheArgs& a = cacheCalls[s][cacheCount[s]++];
        a.cache = cache;
        a.display = display;
        a.view = view;
        a.material = material;
        a.chunk = chunk;
        a.lights = lights;
        a.modelView = modelView;
        std::memcpy(&a.distance, &distance, 4);
    }
}
void __cdecl HookMaterialAdd(void* cache, void* display, void* view, const void* material, void* chunk,
                             const void* lights, const void* modelView)
{
    RecordCache(cache, display, view, material, chunk, lights, modelView);
}
void __cdecl HookAlphaBlendAdd(void* cache, void* display, void* view, const void* material, void* chunk,
                               const void* lights, const void* modelView)
{
    RecordCache(cache, display, view, material, chunk, lights, modelView);
}
void __cdecl HookAlphaMaskAdd(void* cache, const void* material, void* chunk, const void* lights,
                              const void* modelView)
{
    RecordCache(cache, nullptr, nullptr, material, chunk, lights, modelView);
}
void __cdecl HookUnsortedAdd(void* cache, const void* material, void* chunk, const void* lights,
                             const void* modelView, float distance)
{
    RecordCache(cache, nullptr, nullptr, material, chunk, lights, modelView, distance);
}

// ---------------------------------------------------------------------------------------------------------------
// fake material, chunk, camera

void* materialVtable[0x100 / 4];
int alphaHintSet = 1;
int loadedCode = 0;

int* __cdecl MatIsLoaded(const void*, int* out) { *out = loadedCode; return out; }
void __fastcall MatCacheHint(void* self, void*)
{
    *reinterpret_cast<int*>(static_cast<unsigned char*>(self) + 0xa0) = alphaHintSet;
}
void __cdecl MatSetDisplay(const void*, void*) {}
void __cdecl MatClearDisplay(const void*, void*) {}

void InitMaterialVtable()
{
    for (void*& slot : materialVtable) slot = reinterpret_cast<void*>(&Noop);
    materialVtable[0x58 / 4] = reinterpret_cast<void*>(&MatIsLoaded);
    materialVtable[0x5c / 4] = reinterpret_cast<void*>(&MatCacheHint);
    materialVtable[0x60 / 4] = reinterpret_cast<void*>(&MatSetDisplay);
    materialVtable[0x64 / 4] = reinterpret_cast<void*>(&MatClearDisplay);
}

unsigned char fakeBlendCache[0x80];
void* blendVtable[16];

unsigned char materialBytes[2][0x120];

void InitMaterial(int side, unsigned size)
{
    std::memset(materialBytes[side], 0, sizeof materialBytes[side]);
    *reinterpret_cast<void**>(materialBytes[side]) = materialVtable;
    *reinterpret_cast<float*>(materialBytes[side] + 0x20) = 0.5f;
    *reinterpret_cast<float*>(materialBytes[side] + 0x80) = 1.0f;
    *reinterpret_cast<float*>(materialBytes[side] + 0x94) = 0.25f;
    *reinterpret_cast<int*>(materialBytes[side] + 0xa0) = alphaHintSet;
    if (size >= 0xb4) *reinterpret_cast<void**>(materialBytes[side] + 0x9c) = &fakeTextures[0];
    if (size >= 0xc8) *reinterpret_cast<void**>(materialBytes[side] + 0xb8) = nullptr;
}

unsigned char chunkBytes[2][0x80];
void* chunkVtable[0x200 / 4];
unsigned chunkRecords[2][8];
int chunkRecordCount[2];
void* fakeProgram;

const void* __cdecl ChunkProgramFn(void*, int) { return fakeProgram; }
void __cdecl ChunkRenderGeometry(void* self, const void*, const void*, const void* program, bool, int)
{
    const int s = gSide;
    if (chunkRecordCount[s] < 8)
        chunkRecords[s][chunkRecordCount[s]++] = 0x38000000u | unsigned(std::uintptr_t(program) & 0xffff);
}
void __cdecl ChunkRenderPrimitives(void* self, const void*, const void*, const void*, unsigned numIndices,
                                   const unsigned short*, unsigned numPrimitives, int, bool, int)
{
    const int s = gSide;
    if (chunkRecordCount[s] < 8)
        chunkRecords[s][chunkRecordCount[s]++] = 0x3c000000u | ((numIndices & 0xff) << 8) | (numPrimitives & 0xff);
}
void __cdecl ChunkDirty(void*, int, int) {}
void __cdecl ChunkNoItem(void*, void* out, int, int)
{
    *reinterpret_cast<unsigned*>(out) = 0;
    *reinterpret_cast<void**>(static_cast<unsigned char*>(out) + 4) = nullptr;
}

void InitChunkVtable()
{
    for (void*& slot : chunkVtable) slot = reinterpret_cast<void*>(&Noop);
    chunkVtable[0xf0 / 4] = reinterpret_cast<void*>(&ChunkProgramFn);
    chunkVtable[0x38 / 4] = reinterpret_cast<void*>(&ChunkRenderGeometry);
    chunkVtable[0x3c / 4] = reinterpret_cast<void*>(&ChunkRenderPrimitives);
    chunkVtable[0x7c / 4] = reinterpret_cast<void*>(&ChunkDirty);
    chunkVtable[0xb8 / 4] = reinterpret_cast<void*>(&ChunkNoItem);
    chunkVtable[0xbc / 4] = reinterpret_cast<void*>(&ChunkNoItem);
    chunkVtable[0xc0 / 4] = reinterpret_cast<void*>(&ChunkNoItem);
    chunkVtable[0xc4 / 4] = reinterpret_cast<void*>(&ChunkNoItem);
}

void InitChunk(int side)
{
    std::memset(chunkBytes[side], 0, sizeof chunkBytes[side]);
    *reinterpret_cast<void**>(chunkBytes[side]) = chunkVtable;
    *reinterpret_cast<int*>(chunkBytes[side] + 0x1c) = side + 3;
    *reinterpret_cast<int*>(chunkBytes[side] + 0x20) = side + 5;
    *reinterpret_cast<int*>(chunkBytes[side] + 0x24) = 7;
    *reinterpret_cast<float*>(chunkBytes[side] + 0x3c) = 0.5f;
}

unsigned char cameraBytes[2][0x300];
unsigned char viewPortBytes[2][0x40];

void InitCamera(int side)
{
    std::memset(cameraBytes[side], 0, sizeof cameraBytes[side]);
    for (int i = 0; i < 16; ++i)
        reinterpret_cast<float*>(cameraBytes[side] + 0x1b8)[i] = float(i + 1);
    std::memset(viewPortBytes[side], 0, sizeof viewPortBytes[side]);
    *reinterpret_cast<void**>(viewPortBytes[side]) = displayVtable;
    *reinterpret_cast<void**>(viewPortBytes[side] + 0x20) = cameraBytes[side];
}

using RenderFn = void(__cdecl*)(const void*, void*, void*, void*, const void*, const void*, int);

}  // namespace

int main(int argc, char** argv)
{
    const char* dir = argc > 1 ? argv[1] : ".";
    SetCurrentDirectoryA(dir);
    AddVectoredExceptionHandler(1, VectoredHandler);
    ours = LoadLibraryA("Render3DCore_7c.dll");
    orig = LoadLibraryA("Render3DCore_7c_orig.dll");
    if (!ours || !orig) {
        std::printf("load failed %lu\n", GetLastError());
        return 1;
    }
    for (void*& slot : displayVtable) slot = reinterpret_cast<void*>(&Noop);
    ResetDisplay();
    InitTextures();
    InitMaterialVtable();
    InitChunkVtable();
    for (void*& slot : blendVtable) slot = reinterpret_cast<void*>(&Noop);
    blendVtable[0x24 / 4] = reinterpret_cast<void*>(&HookAlphaBlendAdd);
    // the original cache singletons must be non-null before its CacheChunk code reads their vtable
    static unsigned char fakeBlendObject[0x80];
    static unsigned char fakeMaskObject[0x80];
    static unsigned char fakeUnsortedObject[0x80];
    static unsigned char fakeMaterialObject[0x80];
    static void* maskVtable[16];
    static void* unsortedVtable[16];
    for (void*& slot : maskVtable) slot = reinterpret_cast<void*>(&Noop);
    for (void*& slot : unsortedVtable) slot = reinterpret_cast<void*>(&Noop);
    maskVtable[0x24 / 4] = reinterpret_cast<void*>(&HookAlphaMaskAdd);
    unsortedVtable[0x24 / 4] = reinterpret_cast<void*>(&HookUnsortedAdd);
    *reinterpret_cast<void**>(fakeBlendObject) = blendVtable;
    *reinterpret_cast<void**>(fakeBlendCache) = blendVtable;
    *reinterpret_cast<void**>(fakeMaskObject) = maskVtable;
    *reinterpret_cast<void**>(fakeUnsortedObject) = unsortedVtable;
    *reinterpret_cast<void**>(fakeMaterialObject) = blendVtable;
    auto setSingleton = [](const char* name, void* value) {
        auto* const slot = reinterpret_cast<void**>(Optional(orig, name));
        if (slot) *slot = value;
    };
    setSingleton("?singleton@AlphaBlendCache@Jet@@2PAV12@A", fakeBlendObject);
    setSingleton("?singleton@AlphaMaskCache@Jet@@2PAV12@A", fakeMaskObject);
    setSingleton("?singleton@UnsortedAlphaBlendCache@Jet@@2PAV12@A", fakeUnsortedObject);
    setSingleton("?singleton@MaterialChunkCache@Jet@@2PAV12@A", fakeMaterialObject);

    const char* const slsName =
        "?SetLightingState@Material@Jet@@QBAXPAVDisplay@2@PBVMatrix4@2@PBVGeometryChunk@2@PBVVertexProgram@2@PBV?$"
        "FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@1M_N@Z";
    const char* const defName = "?SetTextureDefinition@Material@Jet@@IAAXHABVPString@2@M@Z";
    for (HMODULE m : {ours, orig}) {
        if (!PatchImport(m, slsName, reinterpret_cast<void*>(&HookSLS)))
            std::printf("warning: no SetLightingState import\n");
        if (!PatchImport(m, defName, reinterpret_cast<void*>(&HookDef)))
            std::printf("warning: no SetTextureDefinition import\n");
    }
    const char* const addMaterial =
        "?Add@MaterialChunkCache@Jet@@QAAXPAVDisplay@2@PAVViewPort@2@PBVMaterial@2@PAVGeometryChunk@2@PBV?$"
        "FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z";
    const char* const addBlend =
        "?Add@AlphaBlendCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@PBVMaterial@2@PAVGeometryChunk@2@PBV?$"
        "FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z";
    const char* const addMask =
        "?Add@AlphaMaskCache@Jet@@UAAXPBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$"
        "0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z";
    const char* const addUnsorted =
        "?Add@UnsortedAlphaBlendCache@Jet@@UAAXPBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@"
        "Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@M@Z";
    Detour(Optional(orig, addMaterial), reinterpret_cast<void*>(&HookMaterialAdd));
    Detour(Optional(orig, addBlend), reinterpret_cast<void*>(&HookAlphaBlendAdd));
    Detour(Optional(orig, addMask), reinterpret_cast<void*>(&HookAlphaMaskAdd));
    Detour(Optional(orig, addUnsorted), reinterpret_cast<void*>(&HookUnsortedAdd));

    const int rounds = argc > 2 ? std::atoi(argv[2]) : 200;

    // -----------------------------------------------------------------------------------------------------------
    // CreateGeometryChunk
    {
        using CreateFn = void*(__cdecl*)(const void*, unsigned, unsigned, unsigned, unsigned, int, void*);
        struct Hint {
            unsigned version, size, system, user;
            unsigned char bits;
            unsigned char pad[3];
            unsigned a, b, c, d;
        };
        const char* names[] = {
            "?CreateGeometryChunk@NoTextureMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAVGeometryChunkCreateHint@2@@Z",
            "?CreateGeometryChunk@OneTextureMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAVGeometryChunkCreateHint@2@@Z",
            "?CreateGeometryChunk@OneTextureNoAlphaMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAVGeometryChunkCreateHint@2@@Z",
            "?CreateGeometryChunk@ColorOneTextureMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAVGeometryChunkCreateHint@2@@Z",
            "?CreateGeometryChunk@AutoTexMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAVGeometryChunkCreateHint@2@@Z",
        };
        for (const char* name : names) {
            CreateFn fn[2] = {Proc<CreateFn>(ours, name, "ours"), Proc<CreateFn>(orig, name, "orig")};
            static unsigned char createManager[0x20];
            static unsigned char createKernel[0x100];
            *reinterpret_cast<void**>(createKernel + 0x7c) = displayBytes;
            *reinterpret_cast<void**>(createManager + 8) = createKernel;
            for (int round = 0; round < rounds; ++round) {
                InitMaterial(0, 0x120);
                InitMaterial(1, 0x120);
                *reinterpret_cast<void**>(materialBytes[0] + 0x98) = createManager;
                *reinterpret_cast<void**>(materialBytes[1] + 0x98) = createManager;
                Hint hint;
                std::memset(&hint, 0, sizeof hint);
                hint.version = 2;
                hint.size = (round & 2) ? 1 : 2;
                hint.system = (round & 1) ? 1 : 0;
                hint.bits = static_cast<unsigned char>(Pick(0, 3));
                const unsigned v = 5 + round % 4, p = 2 + round % 3, i = 1 + round % 5, f = round % 4;
                void* c[2];
                for (int s = 0; s < 2; ++s) c[s] = fn[s](materialBytes[s], v, p, i, f, 0, (round & 1) ? &hint : nullptr);
                if (!c[0] || !c[1]) {
                    Check(c[0] == c[1], "CreateGeometryChunk null");
                    continue;
                }
                auto* b0 = static_cast<unsigned char*>(c[0]);
                auto* b1 = static_cast<unsigned char*>(c[1]);
                CheckBytes(b0 + 0x18, b1 + 0x18, 0x14, "CreateGeometryChunk counts");
                CheckBytes(b0 + 0x38, b1 + 0x38, 0x08, "CreateGeometryChunk tail");
                const unsigned* blk0 = *reinterpret_cast<const unsigned**>(b0 + 0x5c);
                const unsigned* blk1 = *reinterpret_cast<const unsigned**>(b1 + 0x5c);
                Check(blk0 && blk1 && blk0[0] == blk1[0] && blk0[1] == blk1[1], "CreateGeometryChunk block");
                // the chunks are intentionally leaked: the template allocations are the same on both sides
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------
    // render paths
    struct RenderEntry {
        const char* rc;
        const char* rnc;
        const char* rp;
        unsigned size;
    };
    const RenderEntry renderEntries[] = {
        {"?RenderChunk@NoTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@H@Z",
         "?RenderNumChunks@NoTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAPAVGeometryChunk@2@PAPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PAPBVMatrix4@2@IH@Z",
         "?RenderPrimitives@NoTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@IPBGIW4DPrimitive@2@H@Z",
         0xac},
        {"?RenderChunk@OneTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@H@Z",
         "?RenderNumChunks@OneTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAPAVGeometryChunk@2@PAPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PAPBVMatrix4@2@IH@Z",
         "?RenderPrimitives@OneTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@IPBGIW4DPrimitive@2@H@Z",
         0xb4},
        {"?RenderChunk@OneTextureNoAlphaMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@H@Z",
         "?RenderNumChunks@OneTextureNoAlphaMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAPAVGeometryChunk@2@PAPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PAPBVMatrix4@2@IH@Z",
         "?RenderPrimitives@OneTextureNoAlphaMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@IPBGIW4DPrimitive@2@H@Z",
         0xb4},
        {"?RenderChunk@BillboardMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@H@Z",
         "?RenderNumChunks@BillboardMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAPAVGeometryChunk@2@PAPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PAPBVMatrix4@2@IH@Z",
         "?RenderPrimitives@BillboardMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@IPBGIW4DPrimitive@2@H@Z",
         0xb4},
        {"?RenderChunk@ColorOneTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@H@Z",
         "?RenderNumChunks@ColorOneTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAPAVGeometryChunk@2@PAPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PAPBVMatrix4@2@IH@Z",
         "?RenderPrimitives@ColorOneTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@IPBGIW4DPrimitive@2@H@Z",
         0xb4},
        {"?RenderChunk@MVHSelectMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@H@Z",
         "?RenderNumChunks@MVHSelectMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAPAVGeometryChunk@2@PAPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PAPBVMatrix4@2@IH@Z",
         "?RenderPrimitives@MVHSelectMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@IPBGIW4DPrimitive@2@H@Z",
         0xc8},
        {"?RenderChunk@AutoTexMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@H@Z",
         "?RenderNumChunks@AutoTexMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAPAVGeometryChunk@2@PAPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PAPBVMatrix4@2@IH@Z",
         "?RenderPrimitives@AutoTexMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@IPBGIW4DPrimitive@2@H@Z",
         0xc4},
        {"?RenderChunk@GenericShaderMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@H@Z",
         "?RenderNumChunks@GenericShaderMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAPAVGeometryChunk@2@PAPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PAPBVMatrix4@2@IH@Z",
         "?RenderPrimitives@GenericShaderMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@IPBGIW4DPrimitive@2@H@Z",
         0xc0},
    };
    using RncFn = void(__cdecl*)(const void*, void*, void*, void**, const void**, const void**, unsigned, int);
    using RpFn = void(__cdecl*)(const void*, void*, void*, void*, const void*, const void*, const unsigned short*,
                                unsigned, int, int);
    for (const RenderEntry& e : renderEntries) {
        const bool billboard = std::strstr(e.rc, "Billboard") != nullptr;
        RenderFn rc[2] = {Proc<RenderFn>(ours, e.rc, "ours"), Proc<RenderFn>(orig, e.rc, "orig")};
        RncFn rnc[2] = {Proc<RncFn>(ours, e.rnc, "ours"), Proc<RncFn>(orig, e.rnc, "orig")};
        RpFn rp[2] = {Proc<RpFn>(ours, e.rp, "ours"), Proc<RpFn>(orig, e.rp, "orig")};
        for (int mode = 0; mode < 3; ++mode) {
            unsigned char snap[2][0x280];
            SLSArgs slsSnap[2][8];
            int slsSnapCount[2];
            unsigned chunkSnap[2][8];
            int chunkSnapCount[2];
            for (int s = 0; s < 2; ++s) {
                InitMaterial(s, e.size);
                InitChunk(s);
                InitCamera(s);
                ResetDisplay();
                gSide = s;
                slsCount[s] = chunkRecordCount[s] = 0;
                const bool withView = (mode != 2);
                Matrix4 mv;
                for (int k = 0; k < 16; ++k) mv.m[k] = float(k) + 0.25f;
                static void* lightList = reinterpret_cast<void*>(0x33333333);
                void* lightsArray[1] = {&lightList};
                void* chunk = chunkBytes[s];
                void* view = withView ? viewPortBytes[s] : nullptr;
                unsigned short indices[4] = {0, 1, 2, 3};
                if (mode == 0) {
                    rc[s](materialBytes[s], displayBytes, view, chunk, lightsArray, &mv, 0);
                } else if (mode == 1) {
                    void* chunks[1] = {chunk};
                    const void* lists[1] = {&lightList};
                    const void* mvs[1] = {&mv};
                    rnc[s](materialBytes[s], displayBytes, view, chunks, lists, mvs, 1, 0);
                } else {
                    rp[s](materialBytes[s], displayBytes, view, chunk, lightsArray, &mv, indices, 4, 3, 0);
                }
                std::memcpy(snap[s], displayBytes, sizeof snap[s]);
                for (int i = 0; i < slsCount[s]; ++i) slsSnap[s][i] = sls[s][i];
                slsSnapCount[s] = slsCount[s];
                for (int i = 0; i < chunkRecordCount[s]; ++i) chunkSnap[s][i] = chunkRecords[s][i];
                chunkSnapCount[s] = chunkRecordCount[s];
            }
            CheckBytes(snap[0], snap[1], sizeof snap[0], "render display");
            Check(slsSnapCount[0] == slsSnapCount[1], "SLS count");
            const int n = slsSnapCount[0] < slsSnapCount[1] ? slsSnapCount[0] : slsSnapCount[1];
            for (int i = 0; i < n; ++i) {
                Check(slsSnap[0][i].chunk == chunkBytes[0] && slsSnap[1][i].chunk == chunkBytes[1], "SLS chunk");
                Check(slsSnap[0][i].program == fakeProgram && slsSnap[1][i].program == fakeProgram, "SLS program");
                Check(slsSnap[0][i].opacity == slsSnap[1][i].opacity, "SLS opacity");
                Check(slsSnap[0][i].unk == slsSnap[1][i].unk, "SLS unk");
                Check(slsSnap[0][i].has2 == slsSnap[1][i].has2, "SLS view null");
                Check(slsSnap[0][i].has1 == slsSnap[1][i].has1, "SLS modelView null");
                if (!billboard) CheckBytes(slsSnap[0][i].mat2, slsSnap[1][i].mat2, 0x40, "SLS view");
                CheckBytes(slsSnap[0][i].mat1, slsSnap[1][i].mat1, 0x40, "SLS modelView");
            }
            Check(chunkSnapCount[0] == chunkSnapCount[1], "chunk record count");
            for (int i = 0; i < chunkSnapCount[0] && i < chunkSnapCount[1]; ++i)
                Check(chunkSnap[0][i] == chunkSnap[1][i], "chunk record");
        }
    }

    std::printf("phase: cache\n");
    // -----------------------------------------------------------------------------------------------------------
    // CacheChunk
    {
        struct CacheEntry { const char* name; unsigned size; };
        const CacheEntry entries[] = {
            {"?CacheChunk@NoTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z", 0xac},
            {"?CacheChunk@OneTextureMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z", 0xb4},
            {"?CacheChunk@OneTextureNoAlphaMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z", 0xb4},
            {"?CacheChunk@MVHSelectMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z", 0xc8},
            {"?CacheChunk@GlossMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z", 0x108},
            {"?CacheChunk@GenericShaderMaterial@Jet@@UBAXPAVDisplay@2@PAVViewPort@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z", 0xc0},
        };
        using CacheFn = void(__cdecl*)(const void*, void*, void*, void*, const void*, const void*);
        for (const CacheEntry& e : entries) {
            CacheFn fn[2] = {Proc<CacheFn>(ours, e.name, "ours"), Proc<CacheFn>(orig, e.name, "orig")};
            for (int round = 0; round < rounds; ++round) {
                CacheArgs snap[2][4];
                int snapCount[2];
                for (int s = 0; s < 2; ++s) {
                    InitMaterial(s, e.size);
                    InitChunk(s);
                    InitCamera(s);
                    ResetDisplay();
                    gSide = s;
                    cacheCount[s] = 0;
                    *reinterpret_cast<int*>(materialBytes[s] + 0xa0) = (round % 3) - 1;  // -1, 0, 1
                    *reinterpret_cast<float*>(materialBytes[s] + 0x20) = (round & 4) ? 4.75f : 0.5f;
                    alphaHintSet = 2;
                    void* lights[1] = {reinterpret_cast<void*>(0x44444444)};
                    Matrix4 mv;
                    for (int k = 0; k < 16; ++k) mv.m[k] = float(k) + 0.5f;
                    if (e.size == 0xc0) {
                        *reinterpret_cast<void**>(materialBytes[s] + 0x9c) = reinterpret_cast<void*>(0x55555555);
                        *reinterpret_cast<void**>(materialBytes[s] + 0xa0) = fakeBlendCache;
                    }
                    fn[s](materialBytes[s], displayBytes, viewPortBytes[s], chunkBytes[s], lights, &mv);
                    for (int i = 0; i < cacheCount[s] && i < 4; ++i) snap[s][i] = cacheCalls[s][i];
                    snapCount[s] = cacheCount[s];
                }
                Check(snapCount[0] == snapCount[1], "CacheChunk call count");
                for (int i = 0; i < snapCount[0] && i < snapCount[1]; ++i) {
                    Check(snap[0][i].material == materialBytes[0] && snap[1][i].material == materialBytes[1],
                          "CacheChunk material");
                    Check(snap[0][i].chunk == chunkBytes[0] && snap[1][i].chunk == chunkBytes[1], "CacheChunk chunk");
                    Check(snap[0][i].distance == snap[1][i].distance, "CacheChunk distance");
                    Check((snap[0][i].display == displayBytes) == (snap[1][i].display == displayBytes),
                          "CacheChunk display");
                    Check((snap[0][i].view == viewPortBytes[0]) == (snap[1][i].view == viewPortBytes[1]),
                          "CacheChunk view");
                }
            }
        }
    }

    std::printf("phase: display\n");
    // -----------------------------------------------------------------------------------------------------------
    // AutoTex/Reflective SetDisplayState and Reflective/Gloss Bind
    {
        using StateFn = void(__cdecl*)(const void*, void*);
        using BindFn = void(__cdecl*)(void*, void*, void*, unsigned);
        struct SE { const char* name; unsigned size; bool bind; };
        const SE entries[] = {
            {"?SetDisplayState@AutoTexMaterial@Jet@@MBAXPAVDisplay@2@@Z", 0xc4, false},
            {"?SetDisplayState@ReflectiveMaterial@Jet@@IBAXPAVDisplay@2@_N@Z", 0x108, false},
            {"?Bind@ReflectiveMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z", 0x108, true},
            {"?Bind@GlossMaterial@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@I@Z", 0x108, true},
        };
        for (const SE& e : entries) {
            for (int round = 0; round < rounds; ++round) {
                unsigned char snap[2][0x280];
                unsigned char matSnap[2][0x120];
                int texCalls[2][4];
                for (int s = 0; s < 2; ++s) {
                    InitMaterial(s, e.size);
                    InitTextures();
                    InitCamera(s);
                    ResetDisplay();
                    gSide = s;
                    *reinterpret_cast<void**>(materialBytes[s] + 0xa8) = &fakeTextures[0];
                    *reinterpret_cast<float*>(materialBytes[s] + 0xac) = 0.75f;
                    *reinterpret_cast<unsigned*>(materialBytes[s] + 0xb0) = (round % 3) + 1;
                    static void* contexts[3] = {reinterpret_cast<void*>(0x70000001), reinterpret_cast<void*>(0x70000002),
                                                reinterpret_cast<void*>(0x70000003)};
                    *reinterpret_cast<void***>(materialBytes[s] + 0xb4) = contexts;
                    *reinterpret_cast<unsigned char*>(materialBytes[s] + 0xb8) = static_cast<unsigned char>(round & 3);
                    *reinterpret_cast<float*>(materialBytes[s] + 0xbc) = 0.5f;
                    texBindCount = texPriorityCount = 0;
                    texAvailable = (round & 4) ? 1 : 0;
                    texDimension = round & 1;
                    if (e.bind) {
                        BindFn fn[2] = {Proc<BindFn>(ours, e.name, "ours"), Proc<BindFn>(orig, e.name, "orig")};
                        fn[s](materialBytes[s], displayBytes, viewPortBytes[s], (round & 2) ? 1u : 0u);
                    } else {
                        StateFn fn[2] = {Proc<StateFn>(ours, e.name, "ours"), Proc<StateFn>(orig, e.name, "orig")};
                        if (e.size == 0xc4)
                            fn[s](materialBytes[s], displayBytes);
                        else
                            reinterpret_cast<void(__cdecl*)(const void*, void*, bool)>(fn[s])(
                                materialBytes[s], displayBytes, (round & 1) != 0);
                    }
                    std::memcpy(snap[s], displayBytes, sizeof snap[s]);
                    std::memcpy(matSnap[s], materialBytes[s], sizeof matSnap[s]);
                    texCalls[s][0] = texBindCount;
                    texCalls[s][1] = static_cast<int>(texLastPriority);
                    texCalls[s][2] = texAvailable;
                    texCalls[s][3] = texDimension;
                }
                CheckBytes(snap[0], snap[1], sizeof snap[0], "SetDisplayState display");
                CheckBytes(matSnap[0], matSnap[1], sizeof matSnap[0], "SetDisplayState material");
                Check(texCalls[0][0] == texCalls[1][0], "texture Bind count");
            }
        }
    }

    std::printf("phase: spherical\n");
    // -----------------------------------------------------------------------------------------------------------
    // ReflectiveMaterial::SetSphericalTexureCoordinates
    {
        using SphericalFn = void(__cdecl*)(void*, const void*);
        SphericalFn fn[2] = {
            Proc<SphericalFn>(ours, "?SetSphericalTexureCoordinates@ReflectiveMaterial@Jet@@SAXPAVGeometryChunk@2@PBVMatrix4@2@@Z", "ours"),
            Proc<SphericalFn>(orig, "?SetSphericalTexureCoordinates@ReflectiveMaterial@Jet@@SAXPAVGeometryChunk@2@PBVMatrix4@2@@Z", "orig")};
        struct View3 { unsigned stride; const void* data; };
        struct View2 { unsigned stride; const void* data; };
        static View3 v3;
        static View2 v2;
        void* sphVtable[0x200 / 4];
        auto getV3 = +[](void*, View3* out, int, int) { *out = v3; };
        auto getV2 = +[](void*, View2* out, int, int) { *out = v2; };
        for (void*& slot : sphVtable) slot = reinterpret_cast<void*>(&Noop);
        sphVtable[0xc4 / 4] = reinterpret_cast<void*>(getV3);
        sphVtable[0xbc / 4] = reinterpret_cast<void*>(getV2);
        sphVtable[0x7c / 4] = reinterpret_cast<void*>(&ChunkDirty);
        static float normals[8][3];
        static float uvs[8][2];
        for (int round = 0; round < rounds; ++round) {
            for (int i = 0; i < 8; ++i)
                for (int k = 0; k < 3; ++k) normals[i][k] = Awkward() * 0.1f;
            std::memset(uvs, 0, sizeof uvs);
            v3.stride = 12;
            v3.data = normals;
            v2.stride = 8;
            v2.data = uvs;
            Matrix4 m;
            for (int i = 0; i < 16; ++i) m.m[i] = Awkward() * 0.1f;
            unsigned char chunk[0x60];
            std::memset(chunk, 0, sizeof chunk);
            *reinterpret_cast<void**>(chunk) = sphVtable;
            *reinterpret_cast<unsigned*>(chunk + 0x24) = 8;
            gSide = 0;
            fn[0](chunk, &m);
            float out[2][16];
            std::memcpy(out[0], uvs, sizeof uvs);
            std::memset(uvs, 0, sizeof uvs);
            gSide = 1;
            fn[1](chunk, &m);
            std::memcpy(out[1], uvs, sizeof uvs);
            CheckBytes(out[0], out[1], sizeof uvs, "spherical uv");
        }
    }

    std::printf("phase: settexture\n");
    // -----------------------------------------------------------------------------------------------------------
    // GenericShaderMaterial::SetTexture (both overloads)
    {
        using SetDbFn = void(__cdecl*)(void*, int*, int, const void*, const void*, float, const void*);
        using SetRefFn = void(__cdecl*)(void*, int*, int, const void*, float);
        SetDbFn db[2] = {
            Proc<SetDbFn>(ours, "?SetTexture@GenericShaderMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HPBVResourceDatabase@2@ABVPString@2@MABVString@2@@Z", "ours"),
            Proc<SetDbFn>(orig, "?SetTexture@GenericShaderMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HPBVResourceDatabase@2@ABVPString@2@MABVString@2@@Z", "orig")};
        SetRefFn rf[2] = {
            Proc<SetRefFn>(ours, "?SetTexture@GenericShaderMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HABV?$AutoReference@VTexture@Jet@@@2@M@Z", "ours"),
            Proc<SetRefFn>(orig, "?SetTexture@GenericShaderMaterial@Jet@@UAA?AV?$ResultClass@$1?results@Material@Jet@@2QBUResultTable@2@BW4Results@Material@Jet@@@2@HABV?$AutoReference@VTexture@Jet@@@2@M@Z", "orig")};
        void* managerVtable[8] = {reinterpret_cast<void*>(&Noop), reinterpret_cast<void*>(&Noop),
                                  reinterpret_cast<void*>(&Noop), reinterpret_cast<void*>(&Noop),
                                  reinterpret_cast<void*>(&Noop), reinterpret_cast<void*>(&Noop),
                                  reinterpret_cast<void*>(&Noop), reinterpret_cast<void*>(&Noop)};
        (void)managerVtable;
        for (int round = 0; round < rounds; ++round) {
            for (int mode = 0; mode < 2; ++mode) {
                const float amount = Awkward();
                int result[2];
                DefArgs dsnap[2][4];
                int dsCount[2];
                int ringType[2][4], ringTex[2][4], ringCount[2];
                for (int s = 0; s < 2; ++s) {
                    InitMaterial(s, 0xc0);
                    InitTextures();
                    gSide = s;
                    defCount[s] = 0;
                    texPriorityCount = 0;
                    *reinterpret_cast<void**>(materialBytes[s] + 0x98) = nullptr;
                    // materialManager -> +0xc TextureManager
                    static unsigned char manager[0x40];
                    static unsigned char textureManagerObject[8];
                    *reinterpret_cast<void**>(textureManagerObject) = texManagerVtable;
                    *reinterpret_cast<void**>(manager + 0xc) = textureManagerObject;
                    *reinterpret_cast<void**>(materialBytes[s] + 0x98) = manager;
                    *reinterpret_cast<void**>(materialBytes[s] + 0xa4) = nullptr;
                    int* out = &result[s];
                    const int type = (round & 8) ? 16 : (round % 3 == 0 ? 6 : 1);
                    if (mode == 0) {
                        void* nameBox = &fakeTextures[3];
                        void* requestBox = &fakeTextures[3];
                        db[s](materialBytes[s], out, type, nullptr, &nameBox, amount, &requestBox);
                    } else {
                        *reinterpret_cast<void**>(materialBytes[s] + 0x9c) = nullptr;
                        void* ref = (round & 1) ? &fakeTextures[1] : nullptr;
                        rf[s](materialBytes[s], out, type, &ref, amount);
                    }
                    for (int i = 0; i < defCount[s] && i < 4; ++i) dsnap[s][i] = defs[s][i];
                    dsCount[s] = defCount[s];
                    // walk the ring semantically (block addresses differ)
                    ringCount[s] = 0;
                    auto* head = static_cast<unsigned char*>(*reinterpret_cast<void**>(materialBytes[s] + 0xa4));
                    if (head) {
                        auto* block = head;
                        do {
                            const unsigned count = *reinterpret_cast<unsigned*>(block + 8);
                            for (unsigned i = 0; i < count && ringCount[s] < 4; ++i) {
                                ringType[s][ringCount[s]] = *reinterpret_cast<int*>(block + 0xc + i * 8);
                                ringTex[s][ringCount[s]] = *reinterpret_cast<int*>(
                                    static_cast<unsigned char*>(*reinterpret_cast<void**>(block + 0x10 + i * 8)) + 8);
                                ++ringCount[s];
                            }
                            block = *reinterpret_cast<unsigned char**>(block);
                        } while (block != head);
                    }
                }
                Check(result[0] == result[1], "SetTexture result");
                Check(dsCount[0] == dsCount[1], "SetTexture definition count");
                for (int i = 0; i < dsCount[0] && i < dsCount[1]; ++i) {
                    Check(dsnap[0][i].type == dsnap[1][i].type, "definition type");
                    if (dsnap[0][i].amount != dsnap[1][i].amount)
                        std::printf("amount ours %08x orig %08x type %d\n", dsnap[0][i].amount,
                                    dsnap[1][i].amount, dsnap[0][i].type);
                    Check(dsnap[0][i].amount == dsnap[1][i].amount, "definition amount");
                }
                Check(ringCount[0] == ringCount[1], "ring count");
                for (int i = 0; i < ringCount[0] && i < ringCount[1]; ++i) {
                    Check(ringType[0][i] == ringType[1][i], "ring type");
                    Check(ringTex[0][i] == ringTex[1][i], "ring texture");
                }
            }
        }
    }

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
