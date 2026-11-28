// Differential test of WaterSystem_7c U9: WaterTile, Water, WaterTiled, WaterSurface.
//
// Loads our module and WaterSystem_7c_orig.dll in one process, patches the imported allocation, clip, viewpoint
// and render entry points to recording stubs, and runs the same scenarios on both sides. Object bytes, reference
// counts, allocation order, matrices and clip calls are compared. Virtuals are called through the exported
// functions or through each module's own vtable, with the real calling conventions.
//
// usage: diff_ws_u9.exe <dir with WaterSystem_7c.dll, WaterSystem_7c_orig.dll and the Jet modules>
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string>

static HMODULE mod[2];
static int cur = -1;
static unsigned long long checks, bad;static std::string logText[2];

using NewFn = void*(__cdecl*)(unsigned);
using DeleteFn = void(__cdecl*)(void*);
static NewFn RealNew;
static DeleteFn RealDelete;
static std::map<const void*, std::string> allocNames[2];
static int allocCount[2];
static void* lastBigAlloc[2];

static void Log(const char* fmt, ...)
{
    if (cur < 0) return;
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    logText[cur] += buf;
}

static unsigned int Bits(float f)
{
    unsigned int u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

static void* __cdecl HookNew(unsigned n)
{
    void* p = RealNew(n);
    std::memset(p, 0xcd, n);
    char name[24];
    std::snprintf(name, sizeof name, "A%d", allocCount[cur]++);
    allocNames[cur][p] = name;
    if (n == 0xdc) lastBigAlloc[cur] = p;
    Log("  new %u -> %s\n", n, name);
    return p;
}

static void __cdecl HookDelete(void* p)
{
    if (cur >= 0 && p) {
        auto it = allocNames[cur].find(p);
        Log("  free %s\n", it == allocNames[cur].end() ? "?" : it->second.c_str());
        if (it != allocNames[cur].end()) allocNames[cur].erase(it);
    }
    RealDelete(p);
}

static int PatchImport(HMODULE m, const char* name, void* fn)
{
    int patched = 0;
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk
                                                                                          : d->FirstThunk));
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; thunk->u1.AddressOfData; ++thunk, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal)) continue;
            if (std::strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + thunk->u1.AddressOfData)->Name, name))
                continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            iat->u1.Function = reinterpret_cast<uintptr_t>(fn);
            VirtualProtect(&iat->u1.Function, sizeof(void*), old, &old);
            ++patched;
        }
    }
    return patched;
}

template <class T>
static T Fn(int s, const char* name)
{
    return reinterpret_cast<T>(GetProcAddress(mod[s], name));
}

static void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok && ++bad <= 40) std::printf("MISMATCH %s\n", what);
}

static void CompareLog(const char* name)
{
    Check(logText[0] == logText[1], name);
    if (logText[0] != logText[1]) {
        std::printf("---- %s ours ----\n%s---- orig ----\n%s", name, logText[0].c_str(), logText[1].c_str());
    }
}

static void CompareLogWithout(const char* name, const char* line)
{
    for (int s = 0; s < 2; ++s) {
        std::string& text = logText[s];
        for (size_t at; (at = text.find(line)) != std::string::npos;)
            text.erase(at, std::strlen(line));
    }
    CompareLog(name);
}

static void RunSide(int s, void (*run)())
{
    cur = s;
    logText[s].clear();
    allocNames[s].clear();
    allocCount[s] = 0;
    run();
    cur = -1;
}

static void CompareBytes(const void* a, const void* b, unsigned n, const char* what)
{
    Check(std::memcmp(a, b, n) == 0, what);
}

// ---------------------------------------------------------------------------------------------------------------
// fake objects

struct Vec3 {
    float x, y, z;
};
struct Vec4 {
    float x, y, z, w;
};
struct Mat4 {
    float m[16];
};
struct Size {
    int w, h;
};

enum { FRONT_ = 1, BACK_ = 2 };

static int clipResult[2] = {FRONT_, FRONT_};

static void* geomVt[8];
static void* matVt[8];
static void* refVt[4];
static void* dispVt[32];
static int matVb[2] = {0, 0x5c};
static unsigned char geomMem[2][0x80];
static unsigned char matMem[2][0x80];

static void __cdecl ActivateStub(void* self, int detail)
{
    Log("  activate %d\n", detail);
}

struct GeometryRenderFake {
    void __thiscall Render(void* display, void* viewport, const void* lights, Mat4* matrix);
};

void __thiscall GeometryRenderFake::Render(void* display, void* viewport, const void* lights, Mat4* matrix)
{
    (void)this;
    (void)display;
    (void)viewport;
    Log("  geom render lights=%d", lights != nullptr);
    for (int i = 0; i < 16; ++i) Log(" %08x", Bits(matrix->m[i]));
    Log("\n");
}

template <class T>
static void* Slot(T fn)
{
    static_assert(sizeof fn == sizeof(void*), "single-inheritance member function pointer must be one slot");
    void* p;
    std::memcpy(&p, &fn, sizeof p);
    return p;
}

static void __cdecl SetWorldMatrixStub(void* self, const Mat4* matrix)
{
    Log("  display matrix");
    for (int i = 0; i < 16; ++i) Log(" %08x", Bits(matrix->m[i]));
    Log("\n");
}

static void __cdecl NoopStub()
{
}

static int __cdecl Clip1Stub(void* self, const Vec3* position, float radius)
{
    Log("  clip1 %08x %08x %08x r=%08x\n", Bits(position->x), Bits(position->y), Bits(position->z), Bits(radius));
    return clipResult[cur];
}

static int __cdecl Clip2Stub(void* self, const Vec3* points, int count)
{
    Log("  clip2 n=%d", count);
    for (int i = 0; i < count; ++i)
        Log(" [%08x %08x %08x]", Bits(points[i].x), Bits(points[i].y), Bits(points[i].z));
    Log("\n");
    // exercise both branches: cells left of the anchor are outside
    return points[0].x < 0.0f ? BACK_ : FRONT_;
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep)
{
    std::printf("CRASH code=%08lx addr=%p info=%p\n", ep->ExceptionRecord->ExceptionCode,
                ep->ExceptionRecord->ExceptionAddress,
                reinterpret_cast<void*>(ep->ExceptionRecord->ExceptionInformation[0]));
    return EXCEPTION_EXECUTE_HANDLER;
}

static void __cdecl AddStub(void* self, void* element, const Mat4* matrix)
{
    Log("  viewpoint add");
    for (int i = 0; i < 16; ++i) Log(" %08x", Bits(matrix->m[i]));
    Log("\n");
}

static void InitFakes(int s)
{
    geomVt[0] = reinterpret_cast<void*>(&NoopStub);
    geomVt[4] = Slot(&GeometryRenderFake::Render);
    matVt[4] = reinterpret_cast<void*>(&ActivateStub);
    refVt[0] = reinterpret_cast<void*>(&NoopStub);
    refVt[1] = reinterpret_cast<void*>(&NoopStub);
    dispVt[0x54 / 4] = reinterpret_cast<void*>(&SetWorldMatrixStub);

    std::memset(geomMem[s], 0, sizeof geomMem[s]);
    std::memset(matMem[s], 0, sizeof matMem[s]);

    auto* geom = geomMem[s];
    *reinterpret_cast<void**>(geom) = geomVt;
    *reinterpret_cast<int*>(geom + 4) = 1000;  // ReferenceCount
    *reinterpret_cast<int*>(geom + 0x10) = 5;
    *reinterpret_cast<int*>(geom + 0x14) = 5;
    *reinterpret_cast<float*>(geom + 0x18) = 2.0f;
    *reinterpret_cast<float*>(geom + 0x1c) = 3.0f;
    *reinterpret_cast<float*>(geom + 0x20) = 1.0f;
    *reinterpret_cast<float*>(geom + 0x24) = -1.0f;
    *reinterpret_cast<float*>(geom + 0x28) = -2.0f;
    *reinterpret_cast<float*>(geom + 0x2c) = -3.0f;
    *reinterpret_cast<float*>(geom + 0x30) = 4.0f;
    *reinterpret_cast<float*>(geom + 0x34) = 5.0f;
    *reinterpret_cast<float*>(geom + 0x38) = 6.0f;
    *reinterpret_cast<float*>(geom + 0x3c) = 2.5f;

    auto* mat = matMem[s];
    *reinterpret_cast<void**>(mat) = matVt;
    // PluginInstance's vbptr and the vbtable entry for the ReferenceCount virtual base
    *reinterpret_cast<void**>(mat + 4) = matVb;
    *reinterpret_cast<void**>(mat + 0x60) = refVt;
    *reinterpret_cast<int*>(mat + 0x64) = 1000;
}

static void* Geom(int s) { return geomMem[s]; }
static void* Mat(int s) { return matMem[s]; }
static int GeomRefs(int s) { return *reinterpret_cast<int*>(geomMem[s] + 4); }
static int MatRefs(int s) { return *reinterpret_cast<int*>(matMem[s] + 0x64); }

// the exports, per calling convention
using Ctor0 = void*(__thiscall*)(void*);
using CtorCopy = void*(__thiscall*)(void*, const void*);
using Dtor = void(__thiscall*)(void*);
using Assign = void*(__thiscall*)(void*, const void*);
using Cdecl1 = void(__cdecl*)(void*);
using CdeclVoid = void(__cdecl*)(void*, const void*);
using CdeclPtr = void*(__cdecl*)(void*);
using ThisPtr = void*(__thiscall*)(void*);
using CdeclBool = bool(__cdecl*)(void*, const void*);
using SetPosV = void(__cdecl*)(void*, const Vec3*);
using ThisPos = void(__thiscall*)(void*, const Vec3*);
using ThisVoidV = void(__thiscall*)(void*, const void*);
using VectThis = const Vec3*(__thiscall*)(const void*);
using ThisVoid = void(__thiscall*)(void*, const void*);
using ThisTwo = void(__thiscall*)(void*, const void*, const void*);

// ---------------------------------------------------------------------------------------------------------------
// WaterTile

static void WaterTileRun()
{
    InitFakes(cur);
    alignas(8) unsigned char tile[0x40];
    alignas(8) unsigned char copy[0x40];
    alignas(8) unsigned char other[0x40];
    std::memset(tile, 0xcd, sizeof tile);
    std::memset(copy, 0xcd, sizeof copy);
    std::memset(other, 0xcd, sizeof other);

    auto ctor = Fn<Ctor0>(cur, "??0WaterTile@Jet@@QAE@XZ");
    auto ctorCopy = Fn<CtorCopy>(cur, "??0WaterTile@Jet@@QAE@ABV01@@Z");
    auto dtor = Fn<Dtor>(cur, "??1WaterTile@Jet@@UAE@XZ");
    auto assign = Fn<Assign>(cur, "??4WaterTile@Jet@@QAEAAV01@ABV01@@Z");
    auto setPosition = Fn<ThisPos>(cur, "?SetPosition@WaterTile@Jet@@UAEXABVVector3@2@@Z");
    auto getPosition = Fn<VectThis>(cur, "?GetPosition@WaterTile@Jet@@QBEABVVector3@2@XZ");
    auto setGeometry = Fn<ThisVoidV>(cur, "?SetGeometry@WaterTile@Jet@@UAEXPAVWaterGeometry@2@@Z");
    auto setMaterial = Fn<ThisVoidV>(cur, "?SetMaterial@WaterTile@Jet@@UAEXPAVWaterMaterial@2@@Z");
    auto getGeometry = Fn<ThisPtr>(cur, "?GetGeometry@WaterTile@Jet@@QAEPAVWaterGeometry@2@XZ");
    auto getMaterial = Fn<ThisPtr>(cur, "?GetMaterial@WaterTile@Jet@@QAEPAVWaterMaterial@2@XZ");
    auto setLight = Fn<ThisVoid>(cur, "?SetLight@WaterTile@Jet@@QAEXPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@@Z");

    ctor(tile);
    void** tileVt = *reinterpret_cast<void***>(tile);
    Log("  tile ctor");
    for (unsigned i = 4; i < 0x24; ++i) Log(" %02x", tile[i]);
    Log("\n");

    Vec3 p{1.5f, -2.25f, 3.75f};
    setPosition(tile, &p);
    const Vec3* got = getPosition(tile);
    Check(got == reinterpret_cast<Vec3*>(tile + 0xc), "tile GetPosition");
    Log("  tile pos %08x %08x %08x\n", Bits(got->x), Bits(got->y), Bits(got->z));

    setGeometry(tile, Geom(cur));
    setMaterial(tile, Mat(cur));
    setLight(tile, reinterpret_cast<const void*>(0x12345678));
    Log("  refs after set %d %d ptrs %d %d\n", GeomRefs(cur), MatRefs(cur), getGeometry(tile) == Geom(cur),
        getMaterial(tile) == Mat(cur));
    setGeometry(tile, nullptr);
    setMaterial(tile, nullptr);
    Log("  refs after clear %d %d ptrs %d %d\n", GeomRefs(cur), MatRefs(cur), getGeometry(tile) == nullptr,
        getMaterial(tile) == nullptr);
    setGeometry(tile, Geom(cur));
    setMaterial(tile, Mat(cur));

    setLight(tile, reinterpret_cast<const void*>(0x12345678));
    Log("  refs after set %d %d ptrs %d %d\n", GeomRefs(cur), MatRefs(cur), getGeometry(tile) == Geom(cur),
        getMaterial(tile) == Mat(cur));
    setGeometry(tile, nullptr);
    setMaterial(tile, nullptr);
    Log("  refs after clear %d %d ptrs %d %d\n", GeomRefs(cur), MatRefs(cur), getGeometry(tile) == nullptr,
        getMaterial(tile) == nullptr);
    setGeometry(tile, Geom(cur));
    setMaterial(tile, Mat(cur));

    setLight(tile, reinterpret_cast<const void*>(0x12345678));
    Log("  light %p\n", *reinterpret_cast<void**>(tile + 0x20));

    // copy constructor and assignment
    ctorCopy(copy, tile);
    Log("  copy");
    for (unsigned i = 4; i < 0x18; ++i) Log(" %02x", copy[i]);
    Log(" geometry=%d material=%d light=%p refs %d %d\n", getGeometry(copy) == Geom(cur),
        getMaterial(copy) == Mat(cur), *reinterpret_cast<void**>(copy + 0x20), GeomRefs(cur), MatRefs(cur));

    std::memset(other, 0xcd, sizeof other);
    ctor(other);
    Vec3 otherPos{9.0f, 8.0f, 7.0f};
    setPosition(other, &otherPos);
    assign(other, tile);
    Log("  assigned");
    for (unsigned i = 4; i < 0x18; ++i) Log(" %02x", other[i]);
    Log(" geometry=%d material=%d light=%p refs %d %d\n", getGeometry(other) == Geom(cur),
        getMaterial(other) == Mat(cur), *reinterpret_cast<void**>(other + 0x20), GeomRefs(cur), MatRefs(cur));

    setLight(other, nullptr);
    dtor(other);
    dtor(copy);
    Log("  refs after dtor %d %d\n", GeomRefs(cur), MatRefs(cur));
    dtor(tile);
    Log("  refs after dtor2 %d %d\n", GeomRefs(cur), MatRefs(cur));

    // the vtable's own override slots must hold this module's exports
    Check(tileVt[4] == reinterpret_cast<void*>(Fn<void*>(cur, "?Render@WaterTile@Jet@@UAAXPAVDisplay@2@AAVViewPort@2@AAVMatrix4@2@@Z")),
          "WaterTile Render slot");
    Check(tileVt[6] == reinterpret_cast<void*>(Fn<void*>(cur, "?CullTile@WaterTile@Jet@@UAEXAAVViewPoint@2@ABVMatrix4@2@ABVVector3@2@@Z")),
          "WaterTile CullTile slot");
    Check(tileVt[7] == reinterpret_cast<void*>(setGeometry), "WaterTile SetGeometry slot");
    Check(tileVt[8] == reinterpret_cast<void*>(setMaterial), "WaterTile SetMaterial slot");
    Check(tileVt[9] == reinterpret_cast<void*>(setPosition), "WaterTile SetPosition slot");
}

static void WaterTileRenderRun()
{
    InitFakes(cur);
    alignas(8) unsigned char tile[0x40];
    auto ctor = Fn<Ctor0>(cur, "??0WaterTile@Jet@@QAE@XZ");
    auto setGeometry = Fn<ThisVoidV>(cur, "?SetGeometry@WaterTile@Jet@@UAEXPAVWaterGeometry@2@@Z");
    auto setMaterial = Fn<ThisVoidV>(cur, "?SetMaterial@WaterTile@Jet@@UAEXPAVWaterMaterial@2@@Z");
    auto render = Fn<void(__cdecl*)(void*, void*, void*, Mat4*)>(cur,
        "?Render@WaterTile@Jet@@UAAXPAVDisplay@2@AAVViewPort@2@AAVMatrix4@2@@Z");
    ctor(tile);
    setGeometry(tile, Geom(cur));
    setMaterial(tile, Mat(cur));
    Mat4 m;
    for (int i = 0; i < 16; ++i) m.m[i] = float(i) * 0.5f;
    render(tile, reinterpret_cast<void*>(0x1111), reinterpret_cast<void*>(0x2222), &m);
}

static void WaterTileCullRun()
{
    InitFakes(cur);
    alignas(8) unsigned char tile[0x40];
    alignas(8) unsigned char view[0x10];
    auto ctor = Fn<Ctor0>(cur, "??0WaterTile@Jet@@QAE@XZ");
    auto setPosition = Fn<ThisPos>(cur, "?SetPosition@WaterTile@Jet@@UAEXABVVector3@2@@Z");
    auto setGeometry = Fn<ThisVoidV>(cur, "?SetGeometry@WaterTile@Jet@@UAEXPAVWaterGeometry@2@@Z");
    auto setMaterial = Fn<ThisVoidV>(cur, "?SetMaterial@WaterTile@Jet@@UAEXPAVWaterMaterial@2@@Z");
    auto cullTile = Fn<void(__thiscall*)(void*, void*, const Mat4*, const Vec3*)>(cur,
        "?CullTile@WaterTile@Jet@@UAEXAAVViewPoint@2@ABVMatrix4@2@ABVVector3@2@@Z");
    ctor(tile);
    setGeometry(tile, Geom(cur));
    setMaterial(tile, Mat(cur));
    Vec3 tilePos{1.25f, -3.5f, 8.0f};
    setPosition(tile, &tilePos);
    std::memset(view, 0, sizeof view);
    *reinterpret_cast<void**>(view + 4) = reinterpret_cast<void*>(0x3333);  // camera
    Mat4 m;
    for (int i = 0; i < 16; ++i) m.m[i] = 1.0f + float(i);
    Vec3 origin{10.0f, 20.0f, 30.0f};
    cullTile(tile, view, &m, &origin);
}

// ---------------------------------------------------------------------------------------------------------------
// Water

static void* MakeTile(int s, const Vec3& position)
{
    auto ctor = Fn<Ctor0>(s, "??0WaterTile@Jet@@QAE@XZ");
    auto setPosition = Fn<ThisPos>(s, "?SetPosition@WaterTile@Jet@@UAEXABVVector3@2@@Z");
    auto setGeometry = Fn<ThisVoidV>(s, "?SetGeometry@WaterTile@Jet@@UAEXPAVWaterGeometry@2@@Z");
    auto setMaterial = Fn<ThisVoidV>(s, "?SetMaterial@WaterTile@Jet@@UAEXPAVWaterMaterial@2@@Z");
    void* tile = HookNew(0x24);
    ctor(tile);
    setPosition(tile, &position);
    setGeometry(tile, Geom(s));
    setMaterial(tile, Mat(s));
    return tile;
}

static void WaterRun()
{
    InitFakes(cur);
    alignas(8) unsigned char water[0x180];
    std::memset(water, 0xcd, sizeof water);
    auto ctor = Fn<Ctor0>(cur, "??0Water@Jet@@QAE@XZ");
    auto dtor = Fn<Dtor>(cur, "??1Water@Jet@@UAE@XZ");
    auto isEmpty = Fn<bool(__thiscall*)(void*)>(cur, "?IsEmpty@Water@Jet@@QAE_NXZ");
    auto addTile = Fn<void(__cdecl*)(void*, void*)>(cur, "?AddTile@Water@Jet@@UAAXPAVWaterTile@2@@Z");
    auto removeTile = Fn<void(__cdecl*)(void*, void*)>(cur, "?RemoveTile@Water@Jet@@UAAXPAVWaterTile@2@@Z");
    auto setLight = Fn<CdeclBool>(cur, "?SetLight@Water@Jet@@UAA_NPAVDisplayLight@2@@Z");
    auto getLight = Fn<CdeclPtr>(cur, "?GetLight@Water@Jet@@UAAPAVDisplayLight@2@XZ");
    auto getBox = Fn<void(__cdecl*)(void*, Vec3*, Vec3*)>(cur,
        "?GetAxisAlignedBoundingBox@Water@Jet@@UAAXAAVVector3@2@0@Z");
    auto cull = Fn<void(__cdecl*)(void*, void*, const Mat4*)>(cur, "?Cull@Water@Jet@@UAAXAAVViewPoint@2@ABVMatrix4@2@@Z");

    ctor(water);
    void** waterVt = *reinterpret_cast<void***>(water);
    Log("  water ctor empty=%d heads=%p %p\n", isEmpty(water), *reinterpret_cast<void**>(water + 0x12c),
        *reinterpret_cast<void**>(water + 0x130));

    // a particle position and orientation, through the real Particle virtuals
    void** vt = waterVt;
    Vec3 pos{100.0f, -50.0f, 25.0f};
    reinterpret_cast<SetPosV>(vt[11])(water, &pos);
    Vec4 q{0.5f, -0.25f, 0.125f, 0.8f};
    float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    q.x /= len, q.y /= len, q.z /= len, q.w /= len;
    reinterpret_cast<void(__cdecl*)(void*, const Vec4*)>(vt[13])(water, &q);

    void* t1 = MakeTile(cur, Vec3{1.0f, 2.0f, 3.0f});
    void* t2 = MakeTile(cur, Vec3{-4.0f, 5.5f, -6.25f});
    addTile(water, t1);
    unsigned char* tilesChunk = *reinterpret_cast<unsigned char**>(water + 0x12c);
    Log("  after add1 empty=%d chunkcount=%u item=%d\n", isEmpty(water),
        *reinterpret_cast<unsigned*>(tilesChunk + 8), *reinterpret_cast<void**>(tilesChunk + 0xc) == t1);
    addTile(water, t2);
    tilesChunk = *reinterpret_cast<unsigned char**>(water + 0x12c);
    Log("  after add2 chunkcount=%u\n", *reinterpret_cast<unsigned*>(tilesChunk + 8));

    Vec3 mn{1, 2, 3}, mx{4, 5, 6};
    getBox(water, &mn, &mx);
    Log("  box min %08x %08x %08x max %08x %08x %08x\n", Bits(mn.x), Bits(mn.y), Bits(mn.z), Bits(mx.x), Bits(mx.y),
        Bits(mx.z));

    // lights: DLIGHT_DEFAULT (3) accepted, another type rejected, null clears
    alignas(8) unsigned char light[0x60];
    std::memset(light, 0, sizeof light);
    *reinterpret_cast<int*>(light + 4) = 3;
    bool lightOk = setLight(water, light);
    void* gotLight = getLight(water);
    Log("  setlight ok=%d get=%d\n", lightOk, gotLight == light);
    Log("  light arr count=%u\n", *reinterpret_cast<unsigned*>(*reinterpret_cast<unsigned char**>(water + 0x130) + 8));
    Log("  setlight null=%d getnull=%d\n", setLight(water, nullptr), getLight(water) == nullptr);
    *reinterpret_cast<int*>(light + 4) = 2;
    Log("  setlight type2=%d getnull=%d\n", setLight(water, light), getLight(water) == nullptr);
    *reinterpret_cast<int*>(light + 4) = 3;
    setLight(water, light);

    // cull: two tiles, patched clip and viewpoint
    alignas(8) unsigned char view[0x10];
    std::memset(view, 0, sizeof view);
    *reinterpret_cast<void**>(view + 4) = reinterpret_cast<void*>(0x4444);
    Mat4 m;
    for (int i = 0; i < 16; ++i) m.m[i] = 0.25f * float(i + 1);
    cull(water, view, &m);

    removeTile(water, t1);
    tilesChunk = *reinterpret_cast<unsigned char**>(water + 0x12c);
    Log("  after remove empty=%d count=%u\n", isEmpty(water), *reinterpret_cast<unsigned*>(tilesChunk + 8));
    getBox(water, &mn, &mx);
    Log("  box2 min %08x %08x %08x max %08x %08x %08x\n", Bits(mn.x), Bits(mn.y), Bits(mn.z), Bits(mx.x), Bits(mx.y),
        Bits(mx.z));

    dtor(water);
    Log("  after dtor\n");

    Check(waterVt[48] == reinterpret_cast<void*>(addTile), "Water AddTile slot");
    Check(waterVt[49] == reinterpret_cast<void*>(removeTile), "Water RemoveTile slot");
    Check(waterVt[50] == reinterpret_cast<void*>(setLight), "Water SetLight slot");
    Check(waterVt[51] == reinterpret_cast<void*>(getLight), "Water GetLight slot");
    Check(waterVt[52] == reinterpret_cast<void*>(getBox), "Water GetAABB slot");
}

static void WaterCopyRun()
{
    InitFakes(cur);
    alignas(8) unsigned char a[0x180];
    alignas(8) unsigned char b[0x180];
    std::memset(a, 0xcd, sizeof a);
    std::memset(b, 0xcd, sizeof b);
    auto ctor = Fn<Ctor0>(cur, "??0Water@Jet@@QAE@XZ");
    auto assign = Fn<Assign>(cur, "??4Water@Jet@@QAEAAV01@ABV01@@Z");
    auto dtor = Fn<Dtor>(cur, "??1Water@Jet@@UAE@XZ");
    auto addTile = Fn<void(__cdecl*)(void*, void*)>(cur, "?AddTile@Water@Jet@@UAAXPAVWaterTile@2@@Z");
    ctor(a);
    addTile(a, MakeTile(cur, Vec3{1, 2, 3}));
    ctor(b);
    assign(b, a);
    Log("  assign head=%d lights=%p\n", *reinterpret_cast<void**>(b + 0x12c) != nullptr,
        *reinterpret_cast<void**>(b + 0x130));
    unsigned char* bchunk = *reinterpret_cast<unsigned char**>(b + 0x12c);
    unsigned char* achunk = *reinterpret_cast<unsigned char**>(a + 0x12c);
    Log("  assign item %d count=%u\n", *reinterpret_cast<void**>(bchunk + 0xc) == *reinterpret_cast<void**>(achunk + 0xc),
        *reinterpret_cast<unsigned*>(bchunk + 8));
    // the assignment is shallow; detach b so the two destructors do not delete the same tile
    *reinterpret_cast<void**>(b + 0x12c) = nullptr;
    *reinterpret_cast<void**>(b + 0x130) = nullptr;
    dtor(b);
    // self-assignment empties the arrays (the original has no guard)
    assign(a, a);
    Log("  self assign empty=%p %p\n", *reinterpret_cast<void**>(a + 0x12c), *reinterpret_cast<void**>(a + 0x130));
    *reinterpret_cast<void**>(a + 0x12c) = nullptr;
    *reinterpret_cast<void**>(a + 0x130) = nullptr;
    dtor(a);
    Log("  after copy dtor\n");
}

// ---------------------------------------------------------------------------------------------------------------
// WaterTiled

static void WaterTiledRenderRun()
{
    InitFakes(cur);
    alignas(8) unsigned char tiled[0x180];
    std::memset(tiled, 0xcd, sizeof tiled);
    auto ctor = Fn<void*(__thiscall*)(void*, void*, void*, const Size*)>(cur,
        "??0WaterTiled@Jet@@QAE@PAVWaterMaterial@1@PAVWaterGeometry@1@ABUSize@1@@Z");
    auto dtor = Fn<Dtor>(cur, "??1WaterTiled@Jet@@UAE@XZ");
    auto getTile = Fn<CdeclPtr>(cur, "?GetTile@WaterTiled@Jet@@QAAPAVWaterTile@2@XZ");
    auto render = Fn<void(__cdecl*)(void*, void*, void*, Mat4*)>(cur,
        "?Render@WaterTiled@Jet@@UAAXPAVDisplay@2@AAVViewPort@2@AAVMatrix4@2@@Z");
    auto cull = Fn<void(__cdecl*)(void*, void*, const Mat4*)>(cur,
        "?Cull@WaterTiled@Jet@@UAAXAAVViewPoint@2@ABVMatrix4@2@@Z");

    Size size{2, 3};
    ctor(tiled, Mat(cur), Geom(cur), &size);
    void* tile = getTile(tiled);
    Log("  tiled size %d %d tile=%d geo=%d mat=%d\n", *reinterpret_cast<int*>(tiled + 0x130),
        *reinterpret_cast<int*>(tiled + 0x134), tile == *reinterpret_cast<void**>(tiled + 0x12c),
        *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(tile) + 0x18) == Geom(cur),
        *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(tile) + 0x1c) == Mat(cur));
    Log("  bbox adopted=%d ptr=%d\n", *reinterpret_cast<void**>(tiled + 0x4c) == lastBigAlloc[cur],
        *reinterpret_cast<void**>(tiled + 0x4c) != nullptr);

    // camera and viewport stand-ins
    alignas(8) unsigned char camera[0x60];
    alignas(8) unsigned char viewport[0x40];
    alignas(8) unsigned char display[0x10];
    std::memset(camera, 0, sizeof camera);
    std::memset(viewport, 0, sizeof viewport);
    *reinterpret_cast<float*>(camera + 0x2c) = 7.3f;
    *reinterpret_cast<float*>(camera + 0x30) = -4.1f;
    *reinterpret_cast<float*>(camera + 0x34) = 0.0f;
    *reinterpret_cast<void**>(viewport + 0x20) = camera;
    *reinterpret_cast<void**>(display) = dispVt;

    Mat4 m;
    for (int i = 0; i < 16; ++i) m.m[i] = 0.5f * float(i) - 3.0f;
    render(tiled, display, viewport, &m);

    alignas(8) unsigned char view[0x10];
    std::memset(view, 0, sizeof view);
    *reinterpret_cast<void**>(view + 4) = camera;
    cull(tiled, view, &m);

    dtor(tiled);
    Log("  after tiled dtor\n");
}

static void WaterTiledCopyRun()
{
    InitFakes(cur);
    alignas(8) unsigned char a[0x180];
    alignas(8) unsigned char b[0x180];
    std::memset(a, 0xcd, sizeof a);
    std::memset(b, 0xcd, sizeof b);
    auto ctor = Fn<void*(__thiscall*)(void*, void*, void*, const Size*)>(cur,
        "??0WaterTiled@Jet@@QAE@PAVWaterMaterial@1@PAVWaterGeometry@1@ABUSize@1@@Z");
    auto assign = Fn<Assign>(cur, "??4WaterTiled@Jet@@QAEAAV01@ABV01@@Z");
    auto dtor = Fn<Dtor>(cur, "??1WaterTiled@Jet@@UAE@XZ");
    Size size{4, 2};
    ctor(a, Mat(cur), Geom(cur), &size);
    ctor(b, Mat(cur), Geom(cur), &size);
    assign(b, a);
    Log("  assign tile=%d size=%d %d\n", *reinterpret_cast<void**>(b + 0x12c) == *reinterpret_cast<void**>(a + 0x12c),
        *reinterpret_cast<int*>(b + 0x130), *reinterpret_cast<int*>(b + 0x134));
    // the assignment is shallow; detach b so the two destructors do not delete the same tile
    *reinterpret_cast<void**>(b + 0x12c) = nullptr;
    dtor(b);
    dtor(a);
    Log("  after copy dtor\n");
}

// ---------------------------------------------------------------------------------------------------------------
// WaterSurface

static void WaterSurfaceRun()
{
    auto ctor0 = Fn<Ctor0>(cur, "??0WaterSurface@Jet@@QAE@XZ");
    auto ctorSize = Fn<void*(__thiscall*)(void*, const Size*, const float*)>(cur,
        "??0WaterSurface@Jet@@QAE@ABUSize@1@ABVVector2@1@@Z");
    auto ctorCopy = Fn<CtorCopy>(cur, "??0WaterSurface@Jet@@QAE@ABV01@@Z");
    auto assign = Fn<Assign>(cur, "??4WaterSurface@Jet@@QAEAAV01@ABV01@@Z");
    auto dtor = Fn<Dtor>(cur, "??1WaterSurface@Jet@@UAE@XZ");
    auto initialize = Fn<ThisTwo>(cur, "?Initialize@WaterSurface@Jet@@UAEXABUSize@2@ABVVector2@2@@Z");
    auto getNormal = Fn<void(__thiscall*)(void*, int, int, Vec3*)>(cur,
        "?GetNormal@WaterSurface@Jet@@UAEXHHAAVVector3@2@@Z");
    auto getScale = Fn<const float*(__thiscall*)(void*)>(cur, "?GetScale@WaterSurface@Jet@@QAEABVVector2@2@XZ");

    alignas(8) unsigned char a[0x20];
    alignas(8) unsigned char b[0x20];
    std::memset(a, 0xcd, sizeof a);
    std::memset(b, 0xcd, sizeof b);
    ctor0(a);
    Log("  default");
    for (unsigned i = 4; i < 0x18; ++i) Log(" %02x", a[i]);
    Log("\n");

    std::memset(a, 0xcd, sizeof a);
    Size size{4, 3};
    float scale[2] = {9.0f, -9.0f};
    ctorSize(a, &size, scale);
    void** surfaceVt = *reinterpret_cast<void***>(a);
    Log("  sized wh=%d %d scale=%08x %08x data=%d\n", *reinterpret_cast<int*>(a + 4),
        *reinterpret_cast<int*>(a + 8), Bits(*reinterpret_cast<float*>(a + 0x10)),
        Bits(*reinterpret_cast<float*>(a + 0x14)), *reinterpret_cast<void**>(a + 0xc) != nullptr);
    float* data = *reinterpret_cast<float**>(a + 0xc);
    for (int i = 0; i < 12; ++i) data[i] = float(i) + 0.5f;

    float newScale[2] = {2.5f, 6.25f};
    Size newSize{2, 2};
    initialize(a, &newSize, newScale);  // leaks the 4x3 block, as the original does
    Log("  init wh=%d %d scale=%08x %08x\n", *reinterpret_cast<int*>(a + 4),
        *reinterpret_cast<int*>(a + 8), Bits(*reinterpret_cast<float*>(a + 0x10)),
        Bits(*reinterpret_cast<float*>(a + 0x14)));
    Log("  getscale=%d\n", reinterpret_cast<const unsigned char*>(getScale(a)) == a + 0x10);

    Vec3 normal{7, 8, 9};
    getNormal(a, 1, 1, &normal);
    Log("  normal %08x %08x %08x\n", Bits(normal.x), Bits(normal.y), Bits(normal.z));

    ctorCopy(b, a);
    Log("  copy wh=%d %d scale=%08x %08x\n", *reinterpret_cast<int*>(b + 4), *reinterpret_cast<int*>(b + 8),
        Bits(*reinterpret_cast<float*>(b + 0x10)), Bits(*reinterpret_cast<float*>(b + 0x14)));
    float* bdata = *reinterpret_cast<float**>(b + 0xc);
    for (int i = 0; i < 4; ++i) bdata[i] = 100.0f + float(i);
    assign(a, b);
    Log("  assign wh=%d %d scale=%08x %08x\n", *reinterpret_cast<int*>(a + 4), *reinterpret_cast<int*>(a + 8),
        Bits(*reinterpret_cast<float*>(a + 0x10)), Bits(*reinterpret_cast<float*>(a + 0x14)));
    float* adata = *reinterpret_cast<float**>(a + 0xc);
    Log("  data %08x %08x %08x %08x\n", Bits(adata[0]), Bits(adata[1]), Bits(adata[2]), Bits(adata[3]));
    dtor(b);
    dtor(a);

    Check(surfaceVt[2] == reinterpret_cast<void*>(initialize), "WaterSurface Initialize slot");
    Check(surfaceVt[3] == reinterpret_cast<void*>(getNormal), "WaterSurface GetNormal slot");
    Check(surfaceVt[0] != nullptr, "WaterSurface inherited Initialize slot");
}

// ---------------------------------------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_ws_u9 <bin dir>\n");
        return 2;
    }
    const std::string dir = argv[1];
    SetDllDirectoryA(dir.c_str());
    SetUnhandledExceptionFilter(CrashFilter);
    mod[1] = LoadLibraryA((dir + "\\WaterSystem_7c_orig.dll").c_str());
    const DWORD err1 = GetLastError();
    mod[0] = LoadLibraryA((dir + "\\WaterSystem_7c.dll").c_str());
    const DWORD err0 = GetLastError();
    HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load modules from %s (ours=%p err=%lu orig=%p err=%lu ks=%p)\n", argv[1],
                    static_cast<void*>(mod[0]), err0, static_cast<void*>(mod[1]), err1,
                    static_cast<void*>(ks));
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("modules loaded: ours=%p orig=%p ks=%p\n", static_cast<void*>(mod[0]), static_cast<void*>(mod[1]),
                static_cast<void*>(ks));
    RealNew = reinterpret_cast<NewFn>(GetProcAddress(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    RealDelete = reinterpret_cast<DeleteFn>(GetProcAddress(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));

    for (int s = 0; s < 2; ++s) {
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
        PatchImport(mod[s], "?Clip3D@Camera@Jet@@QBA?AW4ClipResult@2@ABVVector3@2@M@Z",
                    reinterpret_cast<void*>(&Clip1Stub));
        PatchImport(mod[s], "?Clip3D@Camera@Jet@@QBA?AW4ClipResult@2@PBVVector3@2@H@Z",
                    reinterpret_cast<void*>(&Clip2Stub));
        PatchImport(mod[s], "?Add@ViewPoint@Jet@@QAAXPAVElement@2@ABVMatrix4@2@@Z",
                    reinterpret_cast<void*>(&AddStub));
    }
    // the visible bounding box is destroyed through Render3DCore_7c's own deleting destructor; the local
    // WaterSystem build had its own thunk table there, so its free of that block is not in our trace (the
    // allocation still comes from and returns to Jet::Alloc)
    struct {
        const char* name;
        void (*run)();
    } tests[] = {
        {"WaterTile lifetime", WaterTileRun},
        {"WaterTile render", WaterTileRenderRun},
        {"WaterTile cull", WaterTileCullRun},
        {"Water lifetime/add/cull", WaterRun},
        {"WaterTiled render/cull", WaterTiledRenderRun},
        {"WaterSurface", WaterSurfaceRun},
    };
    int onlySide = -1;
    if (argc >= 4) onlySide = std::atoi(argv[3]);
    for (const auto& t : tests) {
        if (argc >= 3 && std::strstr(t.name, argv[2]) == nullptr) continue;
        clipResult[0] = clipResult[1] = FRONT_;
        std::printf("run %s\n", t.name);
        if (onlySide < 0 || onlySide == 0) RunSide(0, t.run);
        if (onlySide < 0 || onlySide == 1) RunSide(1, t.run);
        if (onlySide < 0) {
            if (t.run == WaterTiledRenderRun)
                CompareLogWithout(t.name, "  free A1\n");
            else
                CompareLog(t.name);
        }
        else std::printf("%s\n", logText[onlySide].c_str());
    }
    // the clip path that skips the tile/viewpoint work
    clipResult[0] = clipResult[1] = BACK_;
    std::printf("run WaterTile cull outside\n");
    RunSide(0, WaterTileCullRun);
    RunSide(1, WaterTileCullRun);
    CompareLog("WaterTile cull outside");

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}

