// Differential test of Render3DCore_7c unit U6: Geometry/TriangleGeometry (the 13 exports) and every concrete
// GeometryChunkTemplate/GeometryChunkNmlTemplate instantiation of the module, against Render3DCore_7c_orig.dll in one
// process.
//
// The 13 exports are called through GetProcAddress by their original mangled names and the object bytes (from +4, past
// the vtable), return values and side effects are compared raw. The templates are header-only in our tree, so our side
// is constructed directly from include/jet/Render3DCoreGeometry.h while the original side is built by the original's
// CreateInstance(unsigned,...) function, reached by its RVA from the pre-decompiled listing
// (notes/agents/r3d_workstreams/u6_geometry_decomp.c). Both objects are then queried through their own vtables with the
// same arguments and the fields, block counts, item views and resize/copy results are compared.
//
// A by-value class return of a member function (the item views, GetRenderMatrix) uses the hidden result pointer after
// `this`; a free-function pointer would use EDX:EAX instead, so every such call goes through a pointer to a non-virtual
// member of a class with no virtuals of its own.
//
// usage: diff_r3d_u6.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <random>

#include <jet/Render3DCoreGeometry.h>

// The shared CMakeLists creates every workstream test without link libraries, so the imports the header-only templates
// need are named here. Render3DCore_7c_orig.lib comes first and is forced in by its Geometry vftable import: the module
// proxies' DllMain only succeeds when they arrive as dependencies of the 3D module (loading KernelStandard_7c.dll first
// in a bare console process answers ERROR_DLL_INIT_FAILED; the same module loaded through Render3DCore_7c_orig.dll
// works), and the import-descriptor order is what decides that here.
#pragma comment(linker, "/include:__imp_??_7Geometry@Jet@@6B@")
#pragma comment(lib, "D:/trainz12-decomp/implib/Render3DCore_7c_orig.lib")
#pragma comment(lib, "D:/trainz12-decomp/implib/KernelStandard_7c.lib")
#pragma comment(lib, "D:/trainz12-decomp/implib/RenderSystem_7c.lib")

namespace {

HMODULE mod[2];
unsigned long long checks, bad;
std::mt19937 rng(0x6e6f6d65);
int printed;

int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }

uint32_t RandBits(bool awkward)
{
    if (!awkward) return static_cast<uint32_t>(rng());
    switch (Pick(0, 9)) {
    case 0: return 0x00000000u;
    case 1: return 0x80000000u;  // -0.0f
    case 2: return 0x7f800000u;  // +inf
    case 3: return 0xff800000u;  // -inf
    case 4: return 0x7fc00000u;  // qNaN
    case 5: return 0xffc00001u;  // qNaN, payload
    case 6: return 0x7fa00000u;  // sNaN
    case 7: return 0xffffffffu;
    case 8: return 0x7fffff7fu;
    default: return static_cast<uint32_t>(rng());
    }
}

void DumpFirst(const unsigned char* a, const unsigned char* b, size_t n, size_t base)
{
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            std::printf("  first difference at +0x%zx: ours %02x orig %02x\n", base + i, a[i], b[i]);
            return;
        }
    }
}

void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        if (printed++ < 60) std::printf("MISMATCH %s\n", what);
    }
}

void CheckBytes(const void* ours, const void* orig, size_t n, const char* what)
{
    ++checks;
    if (std::memcmp(ours, orig, n) != 0) {
        ++bad;
        if (printed++ < 60) {
            std::printf("MISMATCH %s\n", what);
            DumpFirst(static_cast<const unsigned char*>(ours), static_cast<const unsigned char*>(orig), n, 0);
        }
    }
}

void CheckValue(unsigned long long ours, unsigned long long orig, const char* what)
{
    ++checks;
    if (ours != orig) {
        ++bad;
        if (printed++ < 60)
            std::printf("MISMATCH %s: ours %llu (0x%llx) orig %llu (0x%llx)\n", what, ours, ours, orig, orig);
    }
}

// ---- module lookup ---------------------------------------------------------------------------------------------

template <class T>
T Proc(int side, const char* name)
{
    auto p = reinterpret_cast<T>(GetProcAddress(mod[side], name));
    if (!p) {
        std::printf("missing export %s in %s\n", name, side ? "orig" : "ours");
        std::exit(2);
    }
    return p;
}

template <class T>
T MemberProc(int side, const char* name)
{
    FARPROC raw = GetProcAddress(mod[side], name);
    if (!raw) {
        std::printf("missing export %s in %s\n", name, side ? "orig" : "ours");
        std::exit(2);
    }
    static_assert(sizeof(T) == sizeof raw, "member function pointer size");
    T fn;
    std::memcpy(&fn, &raw, sizeof fn);
    return fn;
}

bool IsExecutable(const void* p)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof mbi)) return false;
    return mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                                                       PAGE_EXECUTE_WRITECOPY)) != 0;
}

unsigned VtableLength(const void* object, void** exportedVtable)
{
    void* const vt = *reinterpret_cast<void* const*>(object);
    if (exportedVtable) *exportedVtable = vt;
    unsigned n = 0;
    while (n < 0x200 && IsExecutable(reinterpret_cast<void**>(vt)[n])) ++n;
    return n;
}

// ---- the member ABI -------------------------------------------------------------------------------------------

class ChunkLike {
public:
    int unused;
};

template <class R, class... A>
R CallSlot(void* object, unsigned byteOffset, A... args)
{
    using Fn = R (__cdecl ChunkLike::*)(A...);
    static_assert(sizeof(Fn) == sizeof(void*), "member function pointer size");
    void* const entry = reinterpret_cast<void**>(*reinterpret_cast<void**>(object))[byteOffset / sizeof(void*)];
    Fn fn;
    std::memcpy(&fn, &entry, sizeof fn);
    return (reinterpret_cast<ChunkLike*>(object)->*fn)(args...);
}

using DeleteChunkFn = void* (__thiscall*)(void*, unsigned);

void DestroyChunk(void* chunk)
{
    void* const entry = reinterpret_cast<void**>(*reinterpret_cast<void**>(chunk))[1];
    reinterpret_cast<DeleteChunkFn>(entry)(chunk, 1);
}

void FillPattern(void* data, size_t bytes, uint32_t seed)
{
    auto* p = static_cast<unsigned char*>(data);
    for (size_t i = 0; i < bytes; ++i) p[i] = static_cast<unsigned char>((i * 37 + seed) & 0xff);
}

// ---- the 13 exports -------------------------------------------------------------------------------------------

class GeometryLike {
public:
    int unused;
};
class ViewLike {
public:
    int unused;
};
struct Mat16 {
    float m[16];
};

using Ctor0 = void(__thiscall*)(void*);
using CtorCopy = void(__thiscall*)(void*, const void*);
using Dtor0 = void(__thiscall*)(void*);
using AssignFn = void* (__thiscall*)(void*, const void*);
using StoreMatrixFn = void(__cdecl*)(void*, const void*, const void*);
using InitInstanceFn = void(__cdecl*)(const void*, void*);
using GetRenderMatrixFn = Mat16 (__cdecl GeometryLike::*)(const ViewLike&) const;

struct Api {
    Ctor0 ctor0;
    CtorCopy ctorCopy;
    Dtor0 dtor;
    AssignFn assign;
    StoreMatrixFn storeMatrix;
    InitInstanceFn initInstance;
    GetRenderMatrixFn getRenderMatrix;
    void* vtable;
};

Api MakeApi(int side)
{
    Api a;
    a.ctor0 = Proc<Ctor0>(side, "??0Geometry@Jet@@QAE@XZ");
    a.ctorCopy = Proc<CtorCopy>(side, "??0Geometry@Jet@@QAE@ABV01@@Z");
    a.dtor = Proc<Dtor0>(side, "??1Geometry@Jet@@UAE@XZ");
    a.assign = Proc<AssignFn>(side, "??4Geometry@Jet@@QAEAAV01@ABV01@@Z");
    a.storeMatrix = Proc<StoreMatrixFn>(side, "?StoreMatrix@Geometry@Jet@@QAAXABVViewPort@2@ABVMatrix4@2@@Z");
    a.initInstance = Proc<InitInstanceFn>(side, "?InitInstance@Geometry@Jet@@IBAXAAV12@@Z");
    a.getRenderMatrix =
        MemberProc<GetRenderMatrixFn>(side, "?GetRenderMatrix@Geometry@Jet@@QBA?AVMatrix4@2@ABVViewPort@2@@Z");
    a.vtable = Proc<void*>(side, "??_7Geometry@Jet@@6B@");
    return a;
}

struct TriApi {
    Ctor0 ctor0;
    CtorCopy ctorCopy;
    Dtor0 dtor;
    AssignFn assign;
    void* vtable;
};

TriApi MakeTriApi(int side)
{
    TriApi a;
    a.ctor0 = Proc<Ctor0>(side, "??0TriangleGeometry@Jet@@QAE@XZ");
    a.ctorCopy = Proc<CtorCopy>(side, "??0TriangleGeometry@Jet@@QAE@ABV01@@Z");
    a.dtor = Proc<Dtor0>(side, "??1TriangleGeometry@Jet@@UAE@XZ");
    a.assign = Proc<AssignFn>(side, "??4TriangleGeometry@Jet@@QAEAAV01@ABV01@@Z");
    a.vtable = Proc<void*>(side, "??_7TriangleGeometry@Jet@@6B@");
    return a;
}

constexpr unsigned kGeometrySize = 0x44;
constexpr unsigned kMatrixOffset = 0x04;
constexpr unsigned kMatrixSize = 0x40;

struct Object {
    alignas(16) unsigned char bytes[kGeometrySize + 8];
};

struct ViewPortBuf {
    alignas(8) unsigned char bytes[0x60];
};

void GeometryExports(unsigned rounds)
{
    Api api[2] = {MakeApi(0), MakeApi(1)};

    for (unsigned round = 0; round < rounds; ++round) {
        const bool awkward = (round % 3) == 2;

        // default constructor: the vtable only, the matrix stays as it was (both buffers share a pattern)
        Object d[2];
        std::memset(d[0].bytes, 0xAA, sizeof d[0].bytes);
        std::memset(d[1].bytes, 0xAA, sizeof d[1].bytes);
        api[0].ctor0(d[0].bytes);
        api[1].ctor0(d[1].bytes);
        CheckBytes(d[0].bytes + kMatrixOffset, d[1].bytes + kMatrixOffset, kMatrixSize,
                   "Geometry ctor leaves the matrix uninitialised");

        // vtable: exported pointer, length and the shared answer-0 slots 1..4
        for (int side = 0; side < 2; ++side) {
            void* exported = nullptr;
            const unsigned len = VtableLength(d[side].bytes, &exported);
            CheckValue(len, 5, side ? "orig Geometry vtable length" : "ours Geometry vtable length");
            Check(exported == api[side].vtable, "Geometry exported vtable is the object's vptr");
            for (unsigned slot = 1; slot < 5; ++slot) {
                using StubFn = int(__cdecl*)(const void*);
                auto fn = reinterpret_cast<StubFn>(reinterpret_cast<void**>(exported)[slot]);
                CheckValue(static_cast<unsigned>(fn(d[side].bytes)), 0,
                           side ? "orig Geometry stub slot" : "ours Geometry stub slot");
            }
        }

        // source with random matrix bits, then the copy constructor and the assignment
        Object src;
        for (unsigned i = 0; i < kMatrixSize; i += 4) {
            const uint32_t bits = RandBits(awkward);
            std::memcpy(src.bytes + kMatrixOffset + i, &bits, 4);
        }
        std::memcpy(src.bytes, &api[0].vtable, sizeof(void*));

        Object copy[2];
        std::memset(copy[0].bytes, 0x11, sizeof copy[0].bytes);
        std::memset(copy[1].bytes, 0x22, sizeof copy[1].bytes);
        api[0].ctorCopy(copy[0].bytes, src.bytes);
        api[1].ctorCopy(copy[1].bytes, src.bytes);
        CheckBytes(copy[0].bytes + kMatrixOffset, copy[1].bytes + kMatrixOffset, kMatrixSize, "Geometry copy ctor");

        Object assigned[2];
        std::memset(assigned[0].bytes, 0x33, sizeof assigned[0].bytes);
        std::memset(assigned[1].bytes, 0x44, sizeof assigned[1].bytes);
        void* const ret0 = api[0].assign(assigned[0].bytes, src.bytes);
        void* const ret1 = api[1].assign(assigned[1].bytes, src.bytes);
        Check(ret0 == assigned[0].bytes, "Geometry operator= returns this (ours)");
        Check(ret1 == assigned[1].bytes, "Geometry operator= returns this (orig)");
        CheckBytes(assigned[0].bytes + kMatrixOffset, assigned[1].bytes + kMatrixOffset, kMatrixSize,
                   "Geometry operator=");

        // StoreMatrix: stores only when the viewport number (+0x58) is 0
        static const int numbers[] = {0, 0, 1, -1, 0x7fffffff, static_cast<int>(0x80000000u)};
        for (int n : numbers) {
            ViewPortBuf view;
            FillPattern(view.bytes, sizeof view.bytes, 0x51);
            *reinterpret_cast<int*>(view.bytes + 0x58) = n;
            Object arg;
            FillPattern(arg.bytes, sizeof arg.bytes, 0x71);

            Object dest[2];
            std::memset(dest[0].bytes, 0x55, sizeof dest[0].bytes);
            std::memset(dest[1].bytes, 0x55, sizeof dest[1].bytes);
            api[0].storeMatrix(dest[0].bytes, view.bytes, arg.bytes);
            api[1].storeMatrix(dest[1].bytes, view.bytes, arg.bytes);
            CheckBytes(dest[0].bytes + kMatrixOffset, dest[1].bytes + kMatrixOffset, kMatrixSize,
                       "Geometry StoreMatrix");
        }

        // GetRenderMatrix: the stored matrix comes out, the ViewPort is ignored
        for (int side = 0; side < 2; ++side) {
            ViewPortBuf view;
            FillPattern(view.bytes, sizeof view.bytes, 0x91);
            const Mat16 m = (reinterpret_cast<GeometryLike*>(src.bytes)->*api[side].getRenderMatrix)(
                *reinterpret_cast<const ViewLike*>(view.bytes));
            CheckBytes(m.m, src.bytes + kMatrixOffset, kMatrixSize,
                       side ? "orig GetRenderMatrix" : "ours GetRenderMatrix");
        }

        // InitInstance: the matrix is copied, the target vtable is left alone
        Object target[2];
        std::memset(target[0].bytes, 0x77, sizeof target[0].bytes);
        std::memset(target[1].bytes, 0x88, sizeof target[1].bytes);
        api[0].initInstance(src.bytes, target[0].bytes);
        api[1].initInstance(src.bytes, target[1].bytes);
        CheckBytes(target[0].bytes + kMatrixOffset, target[1].bytes + kMatrixOffset, kMatrixSize,
                   "Geometry InitInstance");
        CheckValue(target[0].bytes[0], 0x77, "InitInstance leaves the target vptr (ours)");
        CheckValue(target[1].bytes[0], 0x88, "InitInstance leaves the target vptr (orig)");

        // destructor: the vtable is rewritten, the matrix stays
        api[0].dtor(d[0].bytes);
        api[1].dtor(d[1].bytes);
        CheckBytes(d[0].bytes + kMatrixOffset, d[1].bytes + kMatrixOffset, kMatrixSize, "Geometry destructor");
        Check(*reinterpret_cast<void**>(d[0].bytes) == api[0].vtable, "Geometry dtor vtable (ours)");
        Check(*reinterpret_cast<void**>(d[1].bytes) == api[1].vtable, "Geometry dtor vtable (orig)");
    }

    // TriangleGeometry: the same fields, a 15-slot vtable (5 Geometry slots + ten more answer-0 stubs)
    TriApi tri[2] = {MakeTriApi(0), MakeTriApi(1)};
    for (unsigned round = 0; round < rounds; ++round) {
        Object t[2];
        std::memset(t[0].bytes, 0xA1, sizeof t[0].bytes);
        std::memset(t[1].bytes, 0xA1, sizeof t[1].bytes);
        tri[0].ctor0(t[0].bytes);
        tri[1].ctor0(t[1].bytes);
        CheckBytes(t[0].bytes + kMatrixOffset, t[1].bytes + kMatrixOffset, kMatrixSize, "TriangleGeometry ctor");

        for (int side = 0; side < 2; ++side) {
            void* exported = nullptr;
            const unsigned len = VtableLength(t[side].bytes, &exported);
            CheckValue(len, 17, side ? "orig TriangleGeometry vtable length" : "ours TriangleGeometry vtable length");
            for (unsigned slot = 1; slot < 17; ++slot) {
                using StubFn = int(__cdecl*)(const void*);
                auto fn = reinterpret_cast<StubFn>(reinterpret_cast<void**>(exported)[slot]);
                CheckValue(static_cast<unsigned>(fn(t[side].bytes)), 0,
                           side ? "orig TriangleGeometry stub slot" : "ours TriangleGeometry stub slot");
            }
        }

        Object src;
        for (unsigned i = 0; i < kMatrixSize; i += 4) {
            const uint32_t bits = RandBits((round % 3) == 2);
            std::memcpy(src.bytes + kMatrixOffset + i, &bits, 4);
        }
        Object copy[2];
        std::memset(copy[0].bytes, 0x11, sizeof copy[0].bytes);
        std::memset(copy[1].bytes, 0x22, sizeof copy[1].bytes);
        tri[0].ctorCopy(copy[0].bytes, src.bytes);
        tri[1].ctorCopy(copy[1].bytes, src.bytes);
        CheckBytes(copy[0].bytes + kMatrixOffset, copy[1].bytes + kMatrixOffset, kMatrixSize,
                   "TriangleGeometry copy ctor");

        Object assigned[2];
        std::memset(assigned[0].bytes, 0x33, sizeof assigned[0].bytes);
        std::memset(assigned[1].bytes, 0x44, sizeof assigned[1].bytes);
        Check(tri[0].assign(assigned[0].bytes, src.bytes) == assigned[0].bytes, "TriangleGeometry operator=");
        Check(tri[1].assign(assigned[1].bytes, src.bytes) == assigned[1].bytes, "orig TriangleGeometry operator=");
        CheckBytes(assigned[0].bytes + kMatrixOffset, assigned[1].bytes + kMatrixOffset, kMatrixSize,
                   "TriangleGeometry operator= bytes");

        tri[0].dtor(t[0].bytes);
        tri[1].dtor(t[1].bytes);
        CheckBytes(t[0].bytes + kMatrixOffset, t[1].bytes + kMatrixOffset, kMatrixSize,
                   "TriangleGeometry destructor");
        Check(*reinterpret_cast<void**>(t[0].bytes) == api[0].vtable, "TriangleGeometry dtor leaves the Geometry vtable (ours)");
        Check(*reinterpret_cast<void**>(t[1].bytes) == api[1].vtable, "TriangleGeometry dtor leaves the Geometry vtable (orig)");
    }
}

// ---- the chunk templates ----------------------------------------------------------------------------------------

alignas(8) unsigned char gDisplay[0x300];
void* gDisplayVtable[80];
alignas(8) unsigned char gVertexType[0x40];
void* gVertexTypeVtable[4];

void __cdecl FakeDeleteThis(void*)
{
}

void* __cdecl FakeGetVertexType(void*, int)
{
    return gVertexType;
}

void InitFakeDisplay()
{
    for (unsigned i = 0; i < 80; ++i) gDisplayVtable[i] = reinterpret_cast<void*>(&FakeDeleteThis);
    gDisplayVtable[0x10c / 4] = reinterpret_cast<void*>(&FakeGetVertexType);
    *reinterpret_cast<void***>(gDisplay) = gDisplayVtable;
    gVertexTypeVtable[0] = reinterpret_cast<void*>(&FakeDeleteThis);
    *reinterpret_cast<void***>(gVertexType) = gVertexTypeVtable;
    *reinterpret_cast<int*>(gVertexType + 4) = 1;
}

using CreateInstance4Fn = Jet::GeometryChunk* (__cdecl*)(const void* self, unsigned int vertices,
                                                        unsigned int primitives, unsigned int indices,
                                                        unsigned int faceNormals);

template <class Chunk>
void CompareChunk(const char* name, Jet::GeometryChunk* ours, Jet::GeometryChunk* orig, bool nml)
{
    (void)sizeof(Chunk);
    auto* const a = reinterpret_cast<unsigned char*>(ours);
    auto* const b = reinterpret_cast<unsigned char*>(orig);

    char what[256];
    auto label = [&](const char* field) {
        std::snprintf(what, sizeof what, "%s %s", name, field);
        return what;
    };

    // 0x2c/0x30/0x34 (the shared index/neighbour/normal arrays) are left as the heap held them by the base
    // constructor when the counts are 0, so they are not comparable.
    const unsigned fields[] = {0x18, 0x1c, 0x20, 0x24, 0x28, 0x38, 0x44, 0x48, 0x4c, 0x50, 0x54, 0x58};
    for (unsigned off : fields) {
        char fieldName[32];
        std::snprintf(fieldName, sizeof fieldName, "field +0x%x", off);
        CheckValue(*reinterpret_cast<unsigned*>(a + off), *reinterpret_cast<unsigned*>(b + off), label(fieldName));
    }
    CheckBytes(a + 0x3c, b + 0x3c, 8, label("opacity/surface"));

    // the vertex block: {references, count, records at +0x10}; only refs/count are written
    void* const ablk = *reinterpret_cast<void**>(a + 0x5c);
    void* const bblk = *reinterpret_cast<void**>(b + 0x5c);
    CheckValue(ablk ? 1 : 0, bblk ? 1 : 0, label("block present"));
    if (!ablk || !bblk) return;
    CheckValue(*reinterpret_cast<unsigned*>(ablk), *reinterpret_cast<unsigned*>(bblk), label("block refs"));
    CheckValue(*reinterpret_cast<unsigned*>(static_cast<unsigned char*>(ablk) + 4),
               *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(bblk) + 4), label("block count"));

    // the normal block of the Nml template (null when absent); a plain template is only 0x60 bytes
    void* const anml = nml ? *reinterpret_cast<void**>(a + 0x60) : nullptr;
    void* const bnml = nml ? *reinterpret_cast<void**>(b + 0x60) : nullptr;
    if (nml) CheckValue(anml ? 1 : 0, bnml ? 1 : 0, label("normal block present"));
    if (anml && bnml) {
        CheckValue(*reinterpret_cast<unsigned*>(anml), *reinterpret_cast<unsigned*>(bnml), label("normal refs"));
        CheckValue(*reinterpret_cast<unsigned*>(static_cast<unsigned char*>(anml) + 4),
                   *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(bnml) + 4), label("normal count"));
    }

    // vtable lengths agree and are the chunk layout
    void* avt = nullptr;
    void* bvt = nullptr;
    const unsigned alen = VtableLength(ours, &avt);
    const unsigned blen = VtableLength(orig, &bvt);
    CheckValue(alen, blen, label("vtable length"));
    CheckValue(alen, 73u, label("vtable length is the chunk layout"));

    // queries through the vtables
    CheckValue(CallSlot<int>(ours, 0xa8, 0), CallSlot<int>(orig, 0xa8, 0), label("GetVertexTypeID"));
    CheckValue(CallSlot<int>(ours, 0xac, 0), CallSlot<int>(orig, 0xac, 0), label("GetVertexSize"));
    CheckValue(CallSlot<int>(ours, 0xb4, 0, 0), CallSlot<int>(orig, 0xb4, 0, 0), label("GetItemStream"));
    CheckValue(CallSlot<int>(ours, 0x60), CallSlot<int>(orig, 0x60), label("GetNumStreams"));
    for (int type = 0; type < 39; ++type)
        CheckValue(CallSlot<int>(ours, 0xb0, type, 0), CallSlot<int>(orig, 0xb0, type, 0), label("GetItemOffset"));

    const void* const ava = CallSlot<const void*>(ours, 0x30, 0, 0);
    const void* const bva = CallSlot<const void*>(orig, 0x30, 0, 0);
    CheckValue(reinterpret_cast<std::uintptr_t>(ava) - reinterpret_cast<std::uintptr_t>(ablk),
               reinterpret_cast<std::uintptr_t>(bva) - reinterpret_cast<std::uintptr_t>(bblk),
               label("GetVertexArray offset"));

    // the item views: stride and the data offset relative to the chunk's own blocks
    static const unsigned viewSlots[] = {0xb8, 0xc0, 0xc8, 0xd0, 0xd8};
    for (unsigned slot : viewSlots) {
        const bool vector3 = slot == 0xc0;
        for (int type = 0; type < 39; ++type) {
            const auto va = CallSlot<Jet::GeometryChunk::Vector4Ptr>(ours, slot, type, 0);
            const auto vb = CallSlot<Jet::GeometryChunk::Vector4Ptr>(orig, slot, type, 0);
            CheckValue(va.stride, vb.stride, label("item view stride"));
            if (va.data == nullptr || vb.data == nullptr) {
                CheckValue(va.data == nullptr ? 1 : 0, vb.data == nullptr ? 1 : 0, label("item view null"));
            } else if (vector3 && type == 4 && anml && bnml) {
                // the normal view of the Nml template comes from its own block at +0x60
                CheckValue(reinterpret_cast<std::uintptr_t>(va.data) - reinterpret_cast<std::uintptr_t>(anml),
                           reinterpret_cast<std::uintptr_t>(vb.data) - reinterpret_cast<std::uintptr_t>(bnml),
                           label("normal view offset"));
            } else {
                CheckValue(reinterpret_cast<std::uintptr_t>(va.data) - reinterpret_cast<std::uintptr_t>(ablk),
                           reinterpret_cast<std::uintptr_t>(vb.data) - reinterpret_cast<std::uintptr_t>(bblk),
                           label("item view offset"));
            }
        }
    }

    // the vertex type ask: both call the same fake display, the result and the stored pointer must agree
    {
        const void* const ta = CallSlot<const void*>(ours, 0x9c, gDisplay);
        const void* const tb = CallSlot<const void*>(orig, 0x9c, gDisplay);
        CheckValue(ta == gVertexType ? 1 : 0, tb == gVertexType ? 1 : 0, label("AssignVertexType result"));
        CheckValue(*reinterpret_cast<unsigned*>(a + 0x54) == reinterpret_cast<unsigned>(gVertexType) ? 1 : 0,
                   *reinterpret_cast<unsigned*>(b + 0x54) == reinterpret_cast<unsigned>(gVertexType) ? 1 : 0,
                   label("AssignVertexType stored"));
        const void* const ta2 = CallSlot<const void*>(ours, 0x9c, gDisplay);
        const void* const tb2 = CallSlot<const void*>(orig, 0x9c, gDisplay);
        Check(ta2 == ta, label("AssignVertexType keeps the type (ours)"));
        Check(tb2 == tb, label("AssignVertexType keeps the type (orig)"));
    }

    const unsigned vertices = *reinterpret_cast<unsigned*>(a + 0x28);
    const unsigned elem = static_cast<unsigned>(CallSlot<int>(ours, 0xac, 0));
    constexpr unsigned kVector3Size = 12;

    for (int step = 0; step < 2; ++step) {
        const unsigned n = step == 0 ? vertices + 3 : (vertices > 1 ? vertices - 1 : 0);
        if (void* p = *reinterpret_cast<void**>(a + 0x5c))
            FillPattern(reinterpret_cast<unsigned char*>(p) + 0x10, vertices * elem, 0x1234);
        if (void* p = *reinterpret_cast<void**>(b + 0x5c))
            FillPattern(reinterpret_cast<unsigned char*>(p) + 0x10, vertices * elem, 0x1234);
        if (anml) FillPattern(reinterpret_cast<unsigned char*>(anml) + 0x10, vertices * kVector3Size, 0x4321);
        if (bnml) FillPattern(reinterpret_cast<unsigned char*>(bnml) + 0x10, vertices * kVector3Size, 0x4321);

        CallSlot<void>(ours, 0x24, nullptr, n, 0, 0);
        CallSlot<void>(orig, 0x24, nullptr, n, 0, 0);

        const unsigned kept = vertices < n ? vertices : n;
        CheckValue(*reinterpret_cast<unsigned*>(a + 0x24), *reinterpret_cast<unsigned*>(b + 0x24),
                   label("resize numVertices"));
        CheckValue(*reinterpret_cast<unsigned*>(a + 0x28), *reinterpret_cast<unsigned*>(b + 0x28),
                   label("resize maxVertices"));
        void* const abl2 = *reinterpret_cast<void**>(a + 0x5c);
        void* const bbl2 = *reinterpret_cast<void**>(b + 0x5c);
        CheckBytes(reinterpret_cast<unsigned char*>(abl2) + 0x10, reinterpret_cast<unsigned char*>(bbl2) + 0x10,
                   kept * elem, label("resize kept records"));
    }

    // duplicate the vertex array (maxVertices records) and compare the copies
    {
        const unsigned n = *reinterpret_cast<unsigned*>(a + 0x28);
        if (void* p = *reinterpret_cast<void**>(a + 0x5c))
            FillPattern(reinterpret_cast<unsigned char*>(p) + 0x10, n * elem, 0x2468);
        if (void* p = *reinterpret_cast<void**>(b + 0x5c))
            FillPattern(reinterpret_cast<unsigned char*>(p) + 0x10, n * elem, 0x2468);
        void* const an2 = nml ? *reinterpret_cast<void**>(a + 0x60) : nullptr;
        void* const bn2 = nml ? *reinterpret_cast<void**>(b + 0x60) : nullptr;
        if (an2) FillPattern(reinterpret_cast<unsigned char*>(an2) + 0x10, n * kVector3Size, 0x8642);
        if (bn2) FillPattern(reinterpret_cast<unsigned char*>(bn2) + 0x10, n * kVector3Size, 0x8642);

        CallSlot<void>(ours, 0x1c, nullptr, 0, 0);
        CallSlot<void>(orig, 0x1c, nullptr, 0, 0);

        void* const ap = *reinterpret_cast<void**>(a + 0x5c);
        void* const bp = *reinterpret_cast<void**>(b + 0x5c);
        CheckValue(*reinterpret_cast<unsigned*>(ap), *reinterpret_cast<unsigned*>(bp), label("duplicate refs"));
        CheckValue(*reinterpret_cast<unsigned*>(static_cast<unsigned char*>(ap) + 4),
                   *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(bp) + 4), label("duplicate count"));
        CheckBytes(reinterpret_cast<unsigned char*>(ap) + 0x10, reinterpret_cast<unsigned char*>(bp) + 0x10,
                   n * elem, label("duplicate records"));
        void* const an3 = nml ? *reinterpret_cast<void**>(a + 0x60) : nullptr;
        void* const bn3 = nml ? *reinterpret_cast<void**>(b + 0x60) : nullptr;
        if (an3 && bn3)
            CheckBytes(reinterpret_cast<unsigned char*>(an3) + 0x10, reinterpret_cast<unsigned char*>(bn3) + 0x10,
                       n * kVector3Size, label("duplicate normals"));
    }

    // CreateInstance(): a copy sharing the vertex block and (Nml) the normal block
    {
        auto* const na = static_cast<unsigned char*>(CallSlot<void*>(ours, 0x0c));
        auto* const nb = static_cast<unsigned char*>(CallSlot<void*>(orig, 0x0c));
        if (!na || !nb) {
            Check(na == nb, label("CreateInstance null"));
        } else {
            CheckValue(*reinterpret_cast<unsigned*>(na + 0x18), *reinterpret_cast<unsigned*>(nb + 0x18),
                       label("CreateInstance flags"));
            CheckValue(*reinterpret_cast<unsigned*>(na + 0x28), *reinterpret_cast<unsigned*>(nb + 0x28),
                       label("CreateInstance maxVertices"));
            CheckValue(*reinterpret_cast<void**>(na + 0x5c) == *reinterpret_cast<void**>(a + 0x5c) ? 1 : 0,
                       *reinterpret_cast<void**>(nb + 0x5c) == *reinterpret_cast<void**>(b + 0x5c) ? 1 : 0,
                       label("CreateInstance shares the block"));
            if (nml)
                CheckValue(*reinterpret_cast<void**>(na + 0x60) == anml ? 1 : 0,
                           *reinterpret_cast<void**>(nb + 0x60) == bnml ? 1 : 0,
                           label("CreateInstance shares the normal block"));
            DestroyChunk(na);
            DestroyChunk(nb);
        }
    }

    // CreateInstance(vertices, primitives, indices, faceNormals)
    {
        const unsigned v = 1 + static_cast<unsigned>(rng() % 5);
        const unsigned p = static_cast<unsigned>(rng() % 7);
        const unsigned i = static_cast<unsigned>(rng() % 9);
        const unsigned f = static_cast<unsigned>(rng() % 3);
        auto* const na = static_cast<unsigned char*>(CallSlot<void*>(ours, 0x08, v, p, i, f));
        auto* const nb = static_cast<unsigned char*>(CallSlot<void*>(orig, 0x08, v, p, i, f));
        if (!na || !nb) {
            Check(na == nb, label("CreateInstance4 null"));
        } else {
            CheckValue(*reinterpret_cast<unsigned*>(na + 0x18), *reinterpret_cast<unsigned*>(nb + 0x18),
                       label("CreateInstance4 flags"));
            CheckValue(*reinterpret_cast<unsigned*>(na + 0x1c), *reinterpret_cast<unsigned*>(nb + 0x1c),
                       label("CreateInstance4 primitives"));
            CheckValue(*reinterpret_cast<unsigned*>(na + 0x24), *reinterpret_cast<unsigned*>(nb + 0x24),
                       label("CreateInstance4 vertices"));
            CheckValue(*reinterpret_cast<unsigned*>(na + 0x28), *reinterpret_cast<unsigned*>(nb + 0x28),
                       label("CreateInstance4 maxVertices"));
            CheckValue(*reinterpret_cast<unsigned*>(static_cast<unsigned char*>(*reinterpret_cast<void**>(na + 0x5c)) + 4),
                       *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(*reinterpret_cast<void**>(nb + 0x5c)) + 4),
                       label("CreateInstance4 block count"));
            if (nml) {
                void* const can = *reinterpret_cast<void**>(na + 0x60);
                void* const cbn = *reinterpret_cast<void**>(nb + 0x60);
                CheckValue(can ? 1 : 0, cbn ? 1 : 0, label("CreateInstance4 normal block present"));
                if (can && cbn)
                    CheckValue(*reinterpret_cast<unsigned*>(static_cast<unsigned char*>(can) + 4),
                               *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(cbn) + 4),
                               label("CreateInstance4 normal block count"));
            }
            DestroyChunk(na);
            DestroyChunk(nb);
        }
    }
}

struct TemplateCase {
    const char* name;
    unsigned rva;
    void (*run)(const char* name, unsigned rva, unsigned rounds);
};

template <class Chunk>
void RunTemplateCase(const char* name, unsigned rva, unsigned rounds)
{
    static_assert(sizeof(Chunk) == 0x60 || sizeof(Chunk) == 0x64, "chunk template layout");
    auto* const base = reinterpret_cast<unsigned char*>(mod[1]);
    auto const create = reinterpret_cast<CreateInstance4Fn>(base + (rva - 0x10000000));

    for (unsigned round = 0; round < rounds; ++round) {
        static const unsigned vertexCounts[] = {1, 2, 5, 8};
        const unsigned vertices = vertexCounts[round % 4];
        const unsigned primitives = static_cast<unsigned>(rng() % 11);
        const unsigned indices = 0;      // the base constructor would Release() an uninitialised array block
        const unsigned faceNormals = 0;  // (the original leaves it uninitialised too)
        const unsigned flags = static_cast<unsigned>(rng() % 0x1f) | ((rng() & 1) ? 0x100u : 0u);

        alignas(16) unsigned char self[0x60];
        std::memset(self, 0xCD, sizeof self);
        *reinterpret_cast<unsigned*>(self + 0x18) = flags;

        Jet::GeometryChunk* const orig = create(self, vertices, primitives, indices, faceNormals);
        if (!orig) {
            Check(false, "original CreateInstance failed");
            continue;
        }
        auto* const memory = static_cast<unsigned char*>(Jet::Alloc::New(sizeof(Chunk)));
        auto* const ours = ::new (memory) Chunk(vertices, primitives, indices, faceNormals, flags);

        CompareChunk<Chunk>(name, ours, orig, sizeof(Chunk) == 0x64);

        DestroyChunk(ours);
        DestroyChunk(orig);
    }
}

#define CASE(alias, rva) {#alias, rva, &RunTemplateCase<Jet::alias>}

const TemplateCase kCases[] = {
    CASE(PackedVertexPCGeometryNmlChunk, 0x1000cd20),
    CASE(PackedVertexPCTGeometryNmlChunk, 0x10043c90),
    CASE(PackedVertexPCTTGeometryNmlChunk, 0x1005b580),
    CASE(PackedVertexPW1ICGeometryNmlChunk, 0x1003f690),
    CASE(PackedVertexPW1ICTGeometryNmlChunk, 0x100450d0),
    CASE(PackedVertexPW1ICTTGeometryNmlChunk, 0x1005cf40),
    CASE(PackedVertexPW2ICGeometryNmlChunk, 0x1003fa20),
    CASE(PackedVertexPW2ICTGeometryNmlChunk, 0x10045450),
    CASE(PackedVertexPW2ICTTGeometryNmlChunk, 0x1005d2c0),
    CASE(PackedVertexPW3ICGeometryNmlChunk, 0x1003fe70),
    CASE(PackedVertexPW3ICTGeometryNmlChunk, 0x10045710),
    CASE(PackedVertexPW3ICTTGeometryNmlChunk, 0x1005d580),
    CASE(PackedVertexPW4ICGeometryNmlChunk, 0x10040130),
    CASE(PackedVertexPW4ICTGeometryNmlChunk, 0x10045aa0),
    CASE(PackedVertexPW4ICTTGeometryNmlChunk, 0x1005d9d0),
    CASE(PackedVertexPWb4ICGeometryNmlChunk, 0x1003e7a0),
    CASE(PackedVertexPWb4ICTGeometryNmlChunk, 0x10044300),
    CASE(PackedVertexPWb4ICTTGeometryNmlChunk, 0x1005bcb0),
    CASE(PackedVertexPCGeometryChunk, 0x1000d010),
    CASE(PackedVertexPCNTGeometryChunk, 0x1001fed0),
    CASE(PackedVertexPCTGeometryChunk, 0x100468f0),
    CASE(PackedVertexPCTTGeometryChunk, 0x1005e8d0),
    CASE(PackedVertexPNGeometryChunk, 0x1000ca00),
    CASE(PackedVertexPNTGeometryChunk, 0x1002ec10),
    CASE(PackedVertexPNTAGeometryChunk, 0x1002c9d0),
    CASE(PackedVertexPNTTGeometryChunk, 0x1005b250),
    CASE(PackedVertexPNTTAGeometryChunk, 0x1002cdf0),
    CASE(PackedVertexPW1ICGeometryChunk, 0x10040740),
    CASE(PackedVertexPW1ICTGeometryChunk, 0x10045fa0),
    CASE(PackedVertexPW1ICTTGeometryChunk, 0x1005df70),
    CASE(PackedVertexPW1INGeometryChunk, 0x1003ea70),
    CASE(PackedVertexPW1INTGeometryChunk, 0x10044510),
    CASE(PackedVertexPW1INTTGeometryChunk, 0x1005bec0),
    CASE(PackedVertexPW2ICGeometryChunk, 0x10040a80),
    CASE(PackedVertexPW2ICTGeometryChunk, 0x100461b0),
    CASE(PackedVertexPW2ICTTGeometryChunk, 0x1005e180),
    CASE(PackedVertexPW2INGeometryChunk, 0x1003ece0),
    CASE(PackedVertexPW2INTGeometryChunk, 0x10044790),
    CASE(PackedVertexPW2INTTGeometryChunk, 0x1005c130),
    CASE(PackedVertexPW3ICGeometryChunk, 0x10040ef0),
    CASE(PackedVertexPW3ICTGeometryChunk, 0x100463d0),
    CASE(PackedVertexPW3ICTTGeometryChunk, 0x1005e390),
    CASE(PackedVertexPW3INGeometryChunk, 0x1003ef60),
    CASE(PackedVertexPW3INTGeometryChunk, 0x10044a10),
    CASE(PackedVertexPW3INTTGeometryChunk, 0x1005c3c0),
    CASE(PackedVertexPW4ICGeometryChunk, 0x10041100),
    CASE(PackedVertexPW4ICTGeometryChunk, 0x100465e0),
    CASE(PackedVertexPW4ICTTGeometryChunk, 0x1005e5b0),
    CASE(PackedVertexPW4INGeometryChunk, 0x1003f360),
    CASE(PackedVertexPW4INTGeometryChunk, 0x10044db0),
    CASE(PackedVertexPW4INTTGeometryChunk, 0x1005c9b0),
    CASE(PackedVertexPWb4ICGeometryChunk, 0x10040420),
    CASE(PackedVertexPWb4ICNTGeometryChunk, 0x1005afd0),
    CASE(PackedVertexPWb4ICTGeometryChunk, 0x10045d90),
    CASE(PackedVertexPWb4ICTTGeometryChunk, 0x1005dd60),
    CASE(PackedVertexPWb4INGeometryChunk, 0x1003e160),
    CASE(PackedVertexPWb4INTGeometryChunk, 0x10043ea0),
    CASE(PackedVertexPWb4INTTGeometryChunk, 0x1005b860),
};
#undef CASE

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: %s <testdir> [rounds]\n", argv[0]);
        return 2;
    }
    const unsigned rounds = argc > 2 ? static_cast<unsigned>(std::atoi(argv[2])) : 1000;
    char dir[MAX_PATH];
    GetFullPathNameA(argv[1], MAX_PATH, dir, nullptr);
    SetDllDirectoryA(dir);
    SetCurrentDirectoryA(dir);

    mod[0] = LoadLibraryA("Render3DCore_7c.dll");
    mod[1] = LoadLibraryA("Render3DCore_7c_orig.dll");
    if (!mod[0] || !mod[1]) {
        std::printf("cannot load the modules in %s (error %lu)\n", dir, GetLastError());
        return 2;
    }
    if (mod[0] == mod[1]) {
        std::printf("the two modules resolved to the same file\n");
        return 2;
    }

    InitFakeDisplay();

    GeometryExports(rounds);

    const unsigned templateRounds = rounds < 8 ? rounds : 8;
    for (const TemplateCase& c : kCases) c.run(c.name, c.rva, templateRounds);

    std::printf("%llu checks, %llu mismatches (%u template classes)\n", checks, bad,
                static_cast<unsigned>(sizeof kCases / sizeof kCases[0]));
    return bad ? 1 : 0;
}
