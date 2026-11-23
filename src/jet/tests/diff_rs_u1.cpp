// Differential test of RenderSystem_7c unit U1: our module against RenderSystem_7c_orig.dll in one process.
//
// Vertex records: every exported member of every PackedVertex/GeoLOD/TerrainBlend/StreamedVertex record is found in
// the export table, classified by its undecorated signature and called on both sides with the same record memory
// and arguments (returned pointers are compared as offsets into the record, records byte for byte); the item offset
// tables and other data exports are compared as memory. The other U1 classes (DisplayFormat, DisplayLight,
// DisplayMaterial, RenderTextureConstruct, TextureArgs, TextureStage, DisplayDescriptor, Element, Projector) are
// driven through their exports with random inputs; objects are compared without their vftable pointer.
//
// usage: diff_rs_u1.exe <dir with RenderSystem_7c.dll, RenderSystem_7c_orig.dll and the Jet modules> [rounds] [part]
#include <windows.h>

#include <dbghelp.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <regex>
#include <string>
#include <vector>

#pragma comment(lib, "dbghelp.lib")

static HMODULE mod[2];  // 0 ours, 1 the original
static HMODULE ks;
static unsigned long long checks, bad;
static std::mt19937 rng(9100);
static int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }
static float FPick() { return std::uniform_real_distribution<float>(-1000.0f, 1000.0f)(rng); }

static void* Proc(int side, const char* name)
{
    void* p = reinterpret_cast<void*>(GetProcAddress(mod[side], name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}

static void Check(bool ok, const char* what, const std::string& detail = "")
{
    ++checks;
    if (!ok && ++bad <= 40) std::printf("MISMATCH %s %s\n", what, detail.c_str());
}

static void Same(const void* a, const void* b, size_t n, const char* what, const std::string& detail = "")
{
    Check(std::memcmp(a, b, n) == 0, what, detail);
}

// thiscall through a fastcall pointer: ecx = this, edx unused, the rest on the stack
using Ctor0 = void*(__fastcall*)(void*, void*);
using Ctor1 = void*(__fastcall*)(void*, void*, const void*);

// ---------------------------------------------------------------------------------------------------------------
// vertex records, by signature

struct Export {
    std::string name, text;
};

static std::vector<Export> Exports(HMODULE m)
{
    std::vector<Export> out;
    auto base = reinterpret_cast<const unsigned char*>(m);
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
    auto dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    auto ed = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dir.VirtualAddress);
    auto names = reinterpret_cast<const DWORD*>(base + ed->AddressOfNames);
    for (DWORD i = 0; i < ed->NumberOfNames; ++i) {
        const char* n = reinterpret_cast<const char*>(base + names[i]);
        char text[1024];
        if (!UnDecorateSymbolName(n, text, sizeof text, UNDNAME_COMPLETE)) continue;
        out.push_back({n, text});
    }
    return out;
}

struct Record {
    unsigned char bytes[256];
};

// random dwords, but no signalling NaNs: the original copies floats through x87 fld/fstp, which quiets them
static void Fill(Record& r)
{
    for (int i = 0; i < 256; i += 4) {
        uint32_t x = rng();
        if ((x & 0x7f800000u) == 0x7f800000u && (x & 0x7fffffu) && !(x & 0x400000u)) x |= 0x400000u;
        std::memcpy(r.bytes + i, &x, 4);
    }
}

static void TestVertexRecords(int rounds)
{
    const std::regex rec("Jet::(PackedVertex|GeoLOD|TerrainBlend|StreamedVertex)\\w*::");
    int kinds[16] = {};
    for (const Export& e : Exports(mod[0])) {
        const std::string& t = e.text;
        if (!std::regex_search(t, rec)) continue;
        const char* n = e.name.c_str();
        const bool isStatic = t.find("static ") != std::string::npos;
        const bool isData = t.find('(') == std::string::npos;
        if (isData) {
            // tables and constants: compare the memory (an item offset table at most 39 entries of 12 bytes)
            size_t size = 4;
            if (t.find("vertexItemOffset") != std::string::npos)
                size = t.find("CheckVertexItemTypeInt") != std::string::npos ? 39 * 12
                       : t.find("OffsetLookupCheck") != std::string::npos ? 39 * 8 : 39 * 4;
            else if (t.find("sizeLookup") != std::string::npos)
                size = 16;
            Same(Proc(0, n), Proc(1, n), size, n);
            ++kinds[0];
            continue;
        }
        for (int r = 0; r < rounds; ++r) {
            Record rec2[2];
            Fill(rec2[0]);
            rec2[1] = rec2[0];
            float v[5] = {FPick(), FPick(), FPick(), FPick(), FPick()};
            if (isStatic && t.find("(void)") != std::string::npos) {
                using F = int(__cdecl*)();
                Check(reinterpret_cast<F>(Proc(0, n))() == reinterpret_cast<F>(Proc(1, n))(), n);
                ++kinds[1];
                break;
            } else if (isStatic && t.find("GetItemOffset") != std::string::npos) {
                using F = int(__cdecl*)(int);
                for (int i = 0; i < 39; ++i)
                    Check(reinterpret_cast<F>(Proc(0, n))(i) == reinterpret_cast<F>(Proc(1, n))(i), n,
                          std::to_string(i));
                ++kinds[2];
                break;
            } else if (t.find("__thiscall") != std::string::npos && t.find("operator=") != std::string::npos) {
                Record src;
                Fill(src);
                void* ra = reinterpret_cast<Ctor1>(Proc(0, n))(&rec2[0], nullptr, &src);
                void* rb = reinterpret_cast<Ctor1>(Proc(1, n))(&rec2[1], nullptr, &src);
                Check(ra == &rec2[0] && rb == &rec2[1], n, "return");
                Same(&rec2[0], &rec2[1], sizeof(Record), n);
                ++kinds[3];
            } else if (t.find("__thiscall") != std::string::npos) {  // constructor
                void* ra = reinterpret_cast<Ctor0>(Proc(0, n))(&rec2[0], nullptr);
                void* rb = reinterpret_cast<Ctor0>(Proc(1, n))(&rec2[1], nullptr);
                Check(ra == &rec2[0] && rb == &rec2[1], n, "return");
                Same(&rec2[0], &rec2[1], sizeof(Record), n);
                ++kinds[4];
                break;
            } else if (t.find("float __cdecl") == 0 || t.find("public: float __cdecl") == 0) {
                using F = float(__cdecl*)(void*);
                float a = reinterpret_cast<F>(Proc(0, n))(&rec2[0]);
                float b = reinterpret_cast<F>(Proc(1, n))(&rec2[1]);
                Same(&a, &b, 4, n);
                ++kinds[5];
            } else if (t.find("(int)") != std::string::npos) {
                using F = char*(__cdecl*)(void*, int);
                for (int i = 0; i < 4; ++i)
                    Check(reinterpret_cast<F>(Proc(0, n))(&rec2[0], i) - reinterpret_cast<char*>(&rec2[0]) ==
                              reinterpret_cast<F>(Proc(1, n))(&rec2[1], i) - reinterpret_cast<char*>(&rec2[1]),
                          n, std::to_string(i));
                ++kinds[6];
                break;
            } else if (t.find("(void)") != std::string::npos) {  // accessors and conversions (some answer null)
                using F = char*(__cdecl*)(void*);
                char* a = reinterpret_cast<F>(Proc(0, n))(&rec2[0]);
                char* b = reinterpret_cast<F>(Proc(1, n))(&rec2[1]);
                Check(a && b ? a - reinterpret_cast<char*>(&rec2[0]) == b - reinterpret_cast<char*>(&rec2[1]) : a == b, n);
                ++kinds[7];
                break;
            } else if (t.find("(unsigned long,unsigned long)") != std::string::npos) {
                using F = void(__cdecl*)(void*, unsigned long, unsigned long);
                unsigned long a = rng(), b = rng();
                reinterpret_cast<F>(Proc(0, n))(&rec2[0], a, b);
                reinterpret_cast<F>(Proc(1, n))(&rec2[1], a, b);
                Same(&rec2[0], &rec2[1], sizeof(Record), n);
                ++kinds[8];
            } else if (t.find("const &,float)") != std::string::npos) {
                using F = void(__cdecl*)(void*, const float*, float);
                reinterpret_cast<F>(Proc(0, n))(&rec2[0], v, v[4]);
                reinterpret_cast<F>(Proc(1, n))(&rec2[1], v, v[4]);
                Same(&rec2[0], &rec2[1], sizeof(Record), n);
                ++kinds[9];
            } else if (t.find("const &)") != std::string::npos) {  // setters and operator=(VectorN)
                using F = char*(__cdecl*)(void*, const float*);
                char* a = reinterpret_cast<F>(Proc(0, n))(&rec2[0], v);
                char* b = reinterpret_cast<F>(Proc(1, n))(&rec2[1], v);
                if (t.find("operator=") != std::string::npos)
                    Check(a - reinterpret_cast<char*>(&rec2[0]) == b - reinterpret_cast<char*>(&rec2[1]), n, "return");
                Same(&rec2[0], &rec2[1], sizeof(Record), n);
                ++kinds[10];
            } else {
                std::printf("unclassified %s\n", t.c_str());
                ++bad;
                break;
            }
        }
    }
    std::printf("vertex records: data %d, static %d, item offsets %d, copies %d, ctors %d, floats %d, indexed %d, "
                "accessors %d, weights %d, set4 %d, setters %d\n",
                kinds[0], kinds[1], kinds[2], kinds[3], kinds[4], kinds[5], kinds[6], kinds[7], kinds[8], kinds[9],
                kinds[10]);
}

// ---------------------------------------------------------------------------------------------------------------
// DisplayFormat

static void TestDisplayFormat(int rounds)
{
    using Ctor = void*(__fastcall*)(void*, void*, int);
    using Set = void(__cdecl*)(void*, int);
    using Eq = bool(__cdecl*)(const void*, const void*);
    using Bytes = int(__cdecl*)(const void*);
    using Classify = void(__cdecl*)(const void*);
    using Mask = void(__cdecl*)(unsigned long, unsigned char&, unsigned char&);
    using Name = const char*(__cdecl*)(int);
    for (int r = 0; r < rounds; ++r) {
        unsigned char f[2][12], g[2][12];
        for (int s = 0; s < 2; ++s) std::memset(f[s], 0xcd, 12);
        int id = Pick(-3, 40);
        for (int s = 0; s < 2; ++s)
            reinterpret_cast<Ctor>(Proc(s, "??0DisplayFormat@Jet@@QAE@W4DFormatId@1@@Z"))(f[s], nullptr, id);
        Same(f[0], f[1], 12, "DisplayFormat(id)", std::to_string(id));
        id = Pick(0, 300);
        for (int s = 0; s < 2; ++s) reinterpret_cast<Set>(Proc(s, "?Set@DisplayFormat@Jet@@QAAXW4DFormatId@2@@Z"))(f[s], id);
        Same(f[0], f[1], 12, "DisplayFormat::Set", std::to_string(id));
        Check(reinterpret_cast<Bytes>(Proc(0, "?GetBytes@DisplayFormat@Jet@@QBAHXZ"))(f[0]) ==
                  reinterpret_cast<Bytes>(Proc(1, "?GetBytes@DisplayFormat@Jet@@QBAHXZ"))(f[1]),
              "GetBytes");
        // a second format: a known one, a random layout, or the same layout under id 0 / 20
        unsigned char o[12];
        int k = Pick(0, 3);
        if (k == 0) {
            reinterpret_cast<Set>(Proc(1, "?Set@DisplayFormat@Jet@@QAAXW4DFormatId@2@@Z"))(o, Pick(0, 30));
        } else if (k == 1) {
            for (auto& b : o) b = static_cast<unsigned char>(Pick(0, 3) * 8);
            o[0] = static_cast<unsigned char>(Pick(0, 1) ? 0 : 20);
        } else {
            std::memcpy(o, f[1], 12);
            o[0] = static_cast<unsigned char>(Pick(0, 2) == 0 ? 0 : Pick(0, 1) ? 20 : Pick(0, 30));
        }
        bool a = reinterpret_cast<Eq>(Proc(0, "??8DisplayFormat@Jet@@QBA_NABV01@@Z"))(f[0], o);
        bool b = reinterpret_cast<Eq>(Proc(1, "??8DisplayFormat@Jet@@QBA_NABV01@@Z"))(f[1], o);
        Check(a == b, "DisplayFormat==");
        a = reinterpret_cast<Eq>(Proc(0, "??8DisplayFormat@Jet@@QBA_NABV01@@Z"))(o, f[0]);
        b = reinterpret_cast<Eq>(Proc(1, "??8DisplayFormat@Jet@@QBA_NABV01@@Z"))(o, f[1]);
        Check(a == b, "DisplayFormat== (swapped)");
        // Classify the second one
        std::memcpy(g[0], o, 12);
        std::memcpy(g[1], o, 12);
        if (Pick(0, 1)) g[0][0] = g[1][0] = 0;
        for (int s = 0; s < 2; ++s) reinterpret_cast<Classify>(Proc(s, "?Classify@DisplayFormat@Jet@@QBAXXZ"))(g[s]);
        Same(g[0], g[1], 12, "Classify");
        // copy
        for (int s = 0; s < 2; ++s)
            reinterpret_cast<Ctor1>(Proc(s, "??4DisplayFormat@Jet@@QAEAAV01@ABV01@@Z"))(g[s], nullptr, f[s]);
        Same(g[0], g[1], 12, "DisplayFormat=");
        // masks: runs of bits, never all ones
        unsigned long mask = 0;
        if (Pick(0, 5)) {
            int lo = Pick(0, 31), len = Pick(1, 32 - lo);
            for (int i = 0; i < len; ++i) mask |= 1ul << (lo + i);
            if (Pick(0, 2) == 0) mask |= 1ul << Pick(0, 31);
        }
        if (mask == 0xfffffffful) mask = 0x7fffffff;
        unsigned char sh[2], bi[2];
        for (int s = 0; s < 2; ++s)
            reinterpret_cast<Mask>(Proc(s, "?GetMaskPlacement@DisplayFormat@Jet@@SAXKAAE0@Z"))(mask, sh[s], bi[s]);
        Check(sh[0] == sh[1] && bi[0] == bi[1], "GetMaskPlacement", std::to_string(mask));
    }
    for (int id = -2; id < 40; ++id) {
        const char* a = reinterpret_cast<Name>(Proc(0, "?GetFormatIDString@DisplayFormat@Jet@@SAPBDW4DFormatId@2@@Z"))(id);
        const char* b = reinterpret_cast<Name>(Proc(1, "?GetFormatIDString@DisplayFormat@Jet@@SAPBDW4DFormatId@2@@Z"))(id);
        Check((!a && !b) || (a && b && std::strcmp(a, b) == 0), "GetFormatIDString", std::to_string(id));
    }
}

// ---------------------------------------------------------------------------------------------------------------
// DisplayLight and DisplayMaterial

static float SomeFloat()
{
    switch (Pick(0, 9)) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: return 1.0f;
    default: return FPick();
    }
}

static void TestLight(int rounds)
{
    using CtorT = void*(__fastcall*)(void*, void*, int);
    using SetC = void(__cdecl*)(void*, const float*);
    using SetF = void(__cdecl*)(void*, float);
    using GetF = float(__cdecl*)(const void*);
    using GetP = const char*(__cdecl*)(const void*);
    using GetI = int(__cdecl*)(const void*);
    using Att = void*(__cdecl*)(const void*, float*);
    using Eq = bool(__cdecl*)(const void*, const void*);
    using Init = void(__cdecl*)(const void*, void*);
    using Slot = int(__cdecl*)(const void*);
    const char* setC[] = {"?SetAmbient@DisplayLight@Jet@@QAAXABVColor@2@@Z", "?SetDiffuse@DisplayLight@Jet@@QAAXABVColor@2@@Z",
                          "?SetSpecular@DisplayLight@Jet@@QAAXABVColor@2@@Z"};
    const char* getC[] = {"?GetAmbient@DisplayLight@Jet@@QBAABVColor@2@XZ", "?GetDiffuse@DisplayLight@Jet@@QBAABVColor@2@XZ",
                          "?GetSpecular@DisplayLight@Jet@@QBAABVColor@2@XZ"};
    const char* setF[] = {"?SetConstantAttenuation@DisplayLight@Jet@@QAAXM@Z", "?SetLinearAttenuation@DisplayLight@Jet@@QAAXM@Z",
                          "?SetQuadraticAttenuation@DisplayLight@Jet@@QAAXM@Z", "?SetSpotExponent@DisplayLight@Jet@@QAAXM@Z",
                          "?SetSpotCutoff@DisplayLight@Jet@@QAAXM@Z", "?SetRange@DisplayLight@Jet@@QAAXM@Z"};
    const char* getF[] = {"?GetConstantAttenuation@DisplayLight@Jet@@QBAMXZ", "?GetLinearAttenuation@DisplayLight@Jet@@QBAMXZ",
                          "?GetQuadraticAttenuation@DisplayLight@Jet@@QBAMXZ", "?GetSpotExponent@DisplayLight@Jet@@QBAMXZ",
                          "?GetSpotCutoff@DisplayLight@Jet@@QBAMXZ", "?GetRange@DisplayLight@Jet@@QBAMXZ"};
    for (int r = 0; r < rounds; ++r) {
        alignas(16) unsigned char l[2][0x50], m[2][0x50];
        const bool byDefault = Pick(0, 1) != 0;
        for (int s = 0; s < 2; ++s) {
            std::memset(l[s], 0xab, sizeof l[s]);
            if (byDefault)
                reinterpret_cast<Ctor0>(Proc(s, "??0DisplayLight@Jet@@QAE@XZ"))(l[s], nullptr);
            else
                reinterpret_cast<CtorT>(Proc(s, "??0DisplayLight@Jet@@QAE@W4DLightTypes@1@@Z"))(l[s], nullptr, r % 5);
        }
        Same(l[0] + 4, l[1] + 4, 0x4c, "DisplayLight()");
        for (int step = 0; step < 6; ++step) {
            float c[4] = {SomeFloat(), SomeFloat(), SomeFloat(), SomeFloat()};
            int i = Pick(0, 2), j = Pick(0, 5);
            float f = SomeFloat();
            for (int s = 0; s < 2; ++s) {
                reinterpret_cast<SetC>(Proc(s, setC[i]))(l[s], c);
                reinterpret_cast<SetF>(Proc(s, setF[j]))(l[s], f);
            }
        }
        Same(l[0] + 4, l[1] + 4, 0x4c, "DisplayLight setters");
        for (int i = 0; i < 3; ++i)
            Check(reinterpret_cast<GetP>(Proc(0, getC[i]))(l[0]) - reinterpret_cast<char*>(l[0]) ==
                      reinterpret_cast<GetP>(Proc(1, getC[i]))(l[1]) - reinterpret_cast<char*>(l[1]),
                  getC[i]);
        for (int j = 0; j < 6; ++j) {
            float a = reinterpret_cast<GetF>(Proc(0, getF[j]))(l[0]), b = reinterpret_cast<GetF>(Proc(1, getF[j]))(l[1]);
            Same(&a, &b, 4, getF[j]);
        }
        Check(reinterpret_cast<GetI>(Proc(0, "?GetLightType@DisplayLight@Jet@@QBA?AW4DLightTypes@2@XZ"))(l[0]) ==
                  reinterpret_cast<GetI>(Proc(1, "?GetLightType@DisplayLight@Jet@@QBA?AW4DLightTypes@2@XZ"))(l[1]),
              "GetLightType");
        float va[3], vb[3];
        void* ra = reinterpret_cast<Att>(Proc(0, "?GetAttenuation@DisplayLight@Jet@@QBA?AVVector3@2@XZ"))(l[0], va);
        void* rb = reinterpret_cast<Att>(Proc(1, "?GetAttenuation@DisplayLight@Jet@@QBA?AVVector3@2@XZ"))(l[1], vb);
        Check(ra == va && rb == vb, "GetAttenuation return");
        Same(va, vb, 12, "GetAttenuation");
        // copy, assignment, InitInstance and the virtual slots
        for (int s = 0; s < 2; ++s) {
            std::memset(m[s], 0x5a, sizeof m[s]);
            reinterpret_cast<Ctor1>(Proc(s, "??0DisplayLight@Jet@@QAE@ABV01@@Z"))(m[s], nullptr, l[s]);
        }
        Same(m[0] + 4, m[1] + 4, 0x4c, "DisplayLight(copy)");
        if (Pick(0, 1)) {
            float f = SomeFloat();
            int j = Pick(0, 5);
            for (int s = 0; s < 2; ++s) reinterpret_cast<SetF>(Proc(s, setF[j]))(m[s], f);
        }
        for (int s = 0; s < 2; ++s) {
            bool a = reinterpret_cast<Eq>(Proc(s, "??8DisplayLight@Jet@@UBA_NABV01@@Z"))(l[s], m[s]);
            bool b = (*reinterpret_cast<Eq**>(l[s]))[6](l[s], m[s]);  // the same through the vftable
            Check(a == b, "operator== slot", std::to_string(s));
        }
        Check(reinterpret_cast<Eq>(Proc(0, "??8DisplayLight@Jet@@UBA_NABV01@@Z"))(l[0], m[0]) ==
                  reinterpret_cast<Eq>(Proc(1, "??8DisplayLight@Jet@@UBA_NABV01@@Z"))(l[1], m[1]),
              "DisplayLight==");
        for (int k = 2; k <= 5; ++k)
            Check((*reinterpret_cast<Slot**>(l[0]))[k](l[0]) == (*reinterpret_cast<Slot**>(l[1]))[k](l[1]), "DisplayLight slot",
                  std::to_string(k));
        float c[4] = {SomeFloat(), SomeFloat(), SomeFloat(), SomeFloat()};
        for (int s = 0; s < 2; ++s) {
            reinterpret_cast<SetC>(Proc(s, setC[0]))(m[s], c);
            using Assign = void*(__cdecl*)(void*, const void*);
            Check(reinterpret_cast<Assign>(Proc(s, "??4DisplayLight@Jet@@UAAAAV01@ABV01@@Z"))(l[s], m[s]) == l[s], "operator= return");
            reinterpret_cast<SetC>(Proc(s, setC[1]))(m[s], c);
            reinterpret_cast<Init>(Proc(s, "?InitInstance@DisplayLight@Jet@@QBAXAAV12@@Z"))(m[s], l[s]);
        }
        Same(l[0] + 4, l[1] + 4, 0x4c, "DisplayLight= / InitInstance");
    }
}

static void TestMaterial(int rounds)
{
    using Ne = bool(__cdecl*)(const void*, const void*);
    for (int r = 0; r < rounds; ++r) {
        unsigned char a[2][0x44], b[2][0x44];
        for (int s = 0; s < 2; ++s) {
            std::memset(a[s], 0x77, sizeof a[s]);
            reinterpret_cast<Ctor0>(Proc(s, "??0DisplayMaterial@Jet@@QAE@XZ"))(a[s], nullptr);
        }
        Same(a[0], a[1], 0x44, "DisplayMaterial()");
        float src[17];
        for (float& f : src) f = SomeFloat();
        for (int s = 0; s < 2; ++s) {
            reinterpret_cast<Ctor1>(Proc(s, "??0DisplayMaterial@Jet@@QAE@ABU01@@Z"))(b[s], nullptr, src);
            reinterpret_cast<Ctor1>(Proc(s, "??4DisplayMaterial@Jet@@QAEAAU01@ABU01@@Z"))(a[s], nullptr, src);
        }
        Same(a[0], a[1], 0x44, "DisplayMaterial=");
        Same(b[0], b[1], 0x44, "DisplayMaterial(copy)");
        if (Pick(0, 1)) reinterpret_cast<float*>(b[0])[Pick(0, 16)] = SomeFloat();
        std::memcpy(b[1], b[0], 0x44);
        Check(reinterpret_cast<Ne>(Proc(0, "??9DisplayMaterial@Jet@@QAA_NABU01@@Z"))(a[0], b[0]) ==
                  reinterpret_cast<Ne>(Proc(1, "??9DisplayMaterial@Jet@@QAA_NABU01@@Z"))(a[1], b[1]),
              "DisplayMaterial!=");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// RenderTextureConstruct: the shared context is a stand-in ReferenceCount {vftable, count}

static void TestRenderTextureConstruct(int rounds)
{
    using Ctor5 = void*(__fastcall*)(void*, void*, int, bool, bool, bool, const void*);
    using Dtor = void(__fastcall*)(void*, void*);
    using GetI = int(__cdecl*)(const void*);
    using SetI = void(__cdecl*)(void*, int);
    static void* fakeVtable[4] = {};
    struct Shared {
        void** vftable;
        int count;
    } shared[2] = {{fakeVtable, 100}, {fakeVtable, 100}};
    const char* gets[] = {"?GetSharedContext@RenderTextureConstruct@Jet@@QBAPBVSharedContext@2@XZ",
                          "?GetRenderType@RenderTextureConstruct@Jet@@QBA?AW4TextureTarget@2@XZ",
                          "?GetUseMainContext@RenderTextureConstruct@Jet@@QBA_NXZ",
                          "?GetCreateContext@RenderTextureConstruct@Jet@@QBA_NXZ",
                          "?GetSharedPBuffer@RenderTextureConstruct@Jet@@QBA_NXZ",
                          "?GetActiveDepthLevel@RenderTextureConstruct@Jet@@QBAHXZ"};
    for (int r = 0; r < rounds; ++r) {
        unsigned char c[2][16], d[2][16];
        int target = Pick(0, 5);
        bool b1 = Pick(0, 1), b2 = Pick(0, 1), b3 = Pick(0, 1), withShared = Pick(0, 1);
        int level = Pick(-5, 5);
        for (int s = 0; s < 2; ++s) {
            std::memset(c[s], 0x3c, 16);
            std::memset(d[s], 0x4d, 16);
            const void* sc = withShared ? &shared[s] : nullptr;
            reinterpret_cast<Ctor5>(Proc(s, "??0RenderTextureConstruct@Jet@@QAE@W4TextureTarget@1@_N11PBVSharedContext@1@@Z"))(
                c[s], nullptr, target, b1, b2, b3, sc);
            reinterpret_cast<SetI>(Proc(s, "?SetActiveDepthLevel@RenderTextureConstruct@Jet@@QAAXH@Z"))(c[s], level);
            reinterpret_cast<Ctor1>(Proc(s, "??0RenderTextureConstruct@Jet@@QAE@ABV01@@Z"))(d[s], nullptr, c[s]);
        }
        Check(shared[0].count == shared[1].count, "RenderTextureConstruct counts");
        for (const char* g : gets) {
            int a = reinterpret_cast<GetI>(Proc(0, g))(c[0]), b = reinterpret_cast<GetI>(Proc(1, g))(c[1]);
            if (std::strstr(g, "_NXZ")) a &= 0xff, b &= 0xff;
            if (std::strstr(g, "Shared")) a = a ? 1 : 0, b = b ? 1 : 0;
            Check(a == b, g);
        }
        Check(std::memcmp(c[0] + 4, c[1] + 4, 12) == 0 && std::memcmp(d[0] + 4, d[1] + 4, 12) == 0, "RenderTextureConstruct");
        for (int s = 0; s < 2; ++s) {
            reinterpret_cast<Ctor1>(Proc(s, "??4RenderTextureConstruct@Jet@@QAEAAV01@ABV01@@Z"))(d[s], nullptr, c[s]);
            reinterpret_cast<Dtor>(Proc(s, "??1RenderTextureConstruct@Jet@@QAE@XZ"))(d[s], nullptr);
            reinterpret_cast<Dtor>(Proc(s, "??1RenderTextureConstruct@Jet@@QAE@XZ"))(c[s], nullptr);
        }
        Check(shared[0].count == shared[1].count, "RenderTextureConstruct counts after");
        shared[0].count = shared[1].count = 100;
        std::memset(c[0], 0x11, 16);
        std::memset(c[1], 0x11, 16);
        for (int s = 0; s < 2; ++s)
            reinterpret_cast<Ctor0>(Proc(s, "??0RenderTextureConstruct@Jet@@QAE@XZ"))(c[s], nullptr);
        Same(c[0], c[1], 16, "RenderTextureConstruct()");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// TextureArgs and TextureStage

struct TexArg {
    unsigned char minus, pad[3];
    int mode, loc;
};

static TexArg RandomArg()
{
    TexArg a;
    a.minus = static_cast<unsigned char>(Pick(0, 1));
    for (auto& p : a.pad) p = static_cast<unsigned char>(rng());
    a.mode = Pick(0, 3);
    a.loc = Pick(0, 3);
    return a;
}

static const char* kWords[] = {"COLOR", "ALPHA", "REPLACE", "MODULATE", "ADD", "ADD_SIGNED", "MULTIPLY_ADD", "COMBINE_4",
                               "SUBTRACT", "INTERPOLATE", "DOT3_RGB", "DOT3_RGBA", "TEX", "CONST", "PRIM", "PREV",
                               "MINUS", "#", "STAGE", "color", "tex", "junk", "add_signed"};

static std::string RandomStageText()
{
    std::string t;
    int k = Pick(0, 6);
    if (k == 0) return std::string(Pick(0, 1) ? "STAGE " : "stage ") + std::to_string(Pick(-3, 20));
    if (k == 1) {  // random words
        int n = Pick(1, 12);
        for (int i = 0; i < n; ++i) t += std::string(kWords[Pick(0, 22)]) + (Pick(0, 4) ? " " : "_");
        return t + " # # # # # # # # # # # # # # # #";  // the original reads past the end of short lines (strtok null)
    }
    // a well-formed line, sometimes broken
    static const char* fns[] = {"REPLACE", "MODULATE", "ADD", "ADD_SIGNED", "MULTIPLY_ADD", "COMBINE_4", "SUBTRACT",
                                "INTERPOLATE", "DOT3_RGB", "DOT3_RGBA"};
    static const int args[] = {1, 2, 2, 2, 3, 4, 2, 3, 2, 2};
    static const char* locs[] = {"TEX", "CONST", "PRIM", "PREV"};
    int f = Pick(0, 9);
    t = std::string(Pick(0, 1) ? "COLOR " : "ALPHA ") + fns[f];
    for (int i = 0; i < args[f]; ++i) {
        t += std::string(" ") + locs[Pick(0, 3)] + (Pick(0, 1) ? " COLOR" : "_ALPHA");
        if (Pick(0, 2) == 0) t += " MINUS";
    }
    if (Pick(0, 5) == 0) t += " PREV";  // the token after the last argument
    else t += " #";
    if (Pick(0, 6) == 0) t[Pick(0, static_cast<int>(t.size()) - 1)] = 'Q';
    return t + " # # # # # # # # # # # # # # # #";
}

// the defined part of a TextureArgs: the fields, and the padding only where both sides copied it
static bool SameArgs(const unsigned char* a, const unsigned char* b, bool withPad)
{
    for (int off = 0; off < 0x44; ++off) {
        bool pad = off >= 0xc && off < 0x3c && (off - 0xc) % 12 >= 1 && (off - 0xc) % 12 < 4;
        if (pad && !withPad) continue;
        if (a[off] != b[off]) return false;
    }
    return true;
}

static void TestTextureArgs(int rounds)
{
    using Ctor8 = void*(__fastcall*)(void*, void*, int, unsigned, int, int, const TexArg*, const TexArg*, const TexArg*,
                                    const TexArg*);
    using Decode = bool(__cdecl*)(void*, char*);
    using GetI = int(__cdecl*)(const void*);
    using GetA = int(__cdecl*)(const void*, int);
    const char* gets[] = {"?GetMode@TextureArgs@Jet@@QBA?AW4ModeType@2@XZ", "?GetFn@TextureArgs@Jet@@QBA?AW4DTexFuncType@2@XZ",
                          "?GetNumArgs@TextureArgs@Jet@@QBAHXZ", "?GetStage@TextureArgs@Jet@@QBAHXZ",
                          "?GetMultiplier@TextureArgs@Jet@@QBAIXZ"};
    const char* getA[] = {"?IsArgMinus@TextureArgs@Jet@@QBA_NH@Z", "?GetLoc@TextureArgs@Jet@@QBA?AW4Source@2@H@Z",
                          "?GetArgMode@TextureArgs@Jet@@QBA?AW4ModeType@2@H@Z"};
    for (int r = 0; r < rounds; ++r) {
        unsigned char a[2][0x44], b[2][0x44];
        TexArg ta[4] = {RandomArg(), RandomArg(), RandomArg(), RandomArg()};
        int stage = Pick(-2, 8), mode = Pick(0, 3), fn = Pick(0, 9);
        unsigned mult = static_cast<unsigned>(Pick(0, 5));
        for (int s = 0; s < 2; ++s) {
            std::memset(a[s], 0x99, 0x44);
            reinterpret_cast<Ctor8>(Proc(s, "??0TextureArgs@Jet@@QAE@HIW4ModeType@1@W4DTexFuncType@1@ABUTexArg@1@222@Z"))(
                a[s], nullptr, stage, mult, mode, fn, &ta[0], &ta[1], &ta[2], &ta[3]);
        }
        Check(SameArgs(a[0], a[1], true), "TextureArgs(...)");
        for (int s = 0; s < 2; ++s) {
            std::memset(b[s], 0x66, 0x44);
            reinterpret_cast<Ctor1>(Proc(s, "??0TextureArgs@Jet@@QAE@ABV01@@Z"))(b[s], nullptr, a[s]);
        }
        Check(SameArgs(b[0], b[1], true), "TextureArgs(copy)");
        for (int s = 0; s < 2; ++s) {
            std::memset(b[s], 0x67, 0x44);
            reinterpret_cast<Ctor0>(Proc(s, "??0TextureArgs@Jet@@QAE@XZ"))(b[s], nullptr);
            reinterpret_cast<Ctor1>(Proc(s, "??4TextureArgs@Jet@@QAEAAV01@ABV01@@Z"))(a[s], nullptr, b[s]);
        }
        Check(SameArgs(a[0], a[1], true), "TextureArgs() / operator=");
        // Decode into a decoded or a default object
        std::string text = RandomStageText();
        bool ok[2];
        for (int s = 0; s < 2; ++s) {
            std::vector<char> buf(text.begin(), text.end());
            buf.push_back(0);
            ok[s] = reinterpret_cast<Decode>(Proc(s, "?Decode@TextureArgs@Jet@@QAA_NPAD@Z"))(a[s], buf.data());
        }
        Check(ok[0] == ok[1], "Decode result", text);
        Check(SameArgs(a[0], a[1], true), "Decode", text);
        for (const char* g : gets)
            Check(reinterpret_cast<GetI>(Proc(0, g))(a[0]) == reinterpret_cast<GetI>(Proc(1, g))(a[1]), g);
        for (int i = 0; i < 4; ++i)
            for (const char* g : getA) {
                int x = reinterpret_cast<GetA>(Proc(0, g))(a[0], i), y = reinterpret_cast<GetA>(Proc(1, g))(a[1], i);
                if (std::strstr(g, "Minus")) x &= 0xff, y &= 0xff;
                Check(x == y, g);
            }
    }
}

static std::vector<std::string> buildLog;
static void* buildStage;  // the stage Build passed
static void* __cdecl FakeBuild(void* display, int stage, void* color, void* alpha)
{
    char line[128];
    std::snprintf(line, sizeof line, "build %p %d %d", display, stage,
                  static_cast<int>(static_cast<char*>(alpha) - static_cast<char*>(color)));
    buildStage = color;
    buildLog.push_back(line);
    return reinterpret_cast<void*>(static_cast<intptr_t>(stage * 7));
}

static void TestTextureStage(int rounds)
{
    using CtorD = void*(__fastcall*)(void*, void*, void*);
    using Seg = void(__cdecl*)(void*, int, unsigned, int, int, const TexArg*, const TexArg*, const TexArg*, const TexArg*);
    using Build = void*(__cdecl*)(void*);
    static void* vtable[0x120 / 4];
    vtable[0x11c / 4] = reinterpret_cast<void*>(&FakeBuild);
    struct {
        void** vftable;
    } display = {vtable};
    for (int r = 0; r < rounds; ++r) {
        unsigned char t[2][0x94], u[2][0x94];
        for (int s = 0; s < 2; ++s) {
            std::memset(t[s], 0x21, 0x94);
            reinterpret_cast<CtorD>(Proc(s, "??0TextureStage@Jet@@QAE@PAVDisplay@1@@Z"))(t[s], nullptr, &display);
        }
        Check(SameArgs(t[0], t[1], true) && SameArgs(t[0] + 0x44, t[1] + 0x44, true) &&
                  std::memcmp(t[0] + 0x88, t[1] + 0x88, 12) == 0,
              "TextureStage(display)");
        for (int k = 0; k < 3; ++k) {
            TexArg ta[4] = {RandomArg(), RandomArg(), RandomArg(), RandomArg()};
            int stage = Pick(0, 7), mode = Pick(0, 2), fn = Pick(0, 9);
            unsigned mult = static_cast<unsigned>(Pick(1, 4));
            for (int s = 0; s < 2; ++s)
                reinterpret_cast<Seg>(Proc(s, "?CreateStageSegment@TextureStage@Jet@@QAAXHIW4ModeType@2@W4DTexFuncType@2@ABUTexArg@2@222@Z"))(
                    t[s], stage, mult, mode, fn, &ta[0], &ta[1], &ta[2], &ta[3]);
        }
        // the padding of the arguments a segment did not copy comes from each side's stack
        Check(SameArgs(t[0], t[1], false) && SameArgs(t[0] + 0x44, t[1] + 0x44, false) &&
                  std::memcmp(t[0] + 0x88, t[1] + 0x88, 12) == 0,
              "CreateStageSegment");
        for (int s = 0; s < 2; ++s) {
            reinterpret_cast<Ctor1>(Proc(s, "??0TextureStage@Jet@@QAE@ABV01@@Z"))(u[s], nullptr, t[s]);
            Check(std::memcmp(u[s] + 0x88, t[s] + 0x88, 12) == 0 && SameArgs(u[s], t[s], false), "TextureStage(copy)");
            std::memset(u[s], 0x31, 0x94);
            reinterpret_cast<Ctor1>(Proc(s, "??4TextureStage@Jet@@QAEAAV01@ABV01@@Z"))(u[s], nullptr, t[s]);
            Same(u[s], t[s], 0x94, "TextureStage=");
            buildLog.clear();
            void* got = reinterpret_cast<Build>(Proc(s, "?Build@TextureStage@Jet@@QAAPAXXZ"))(t[s]);
            Check(buildStage == t[s], "Build stage");
            static std::vector<std::string> first;
            static void* firstGot;
            if (s == 0) first = buildLog, firstGot = got;
            else Check(first == buildLog && firstGot == got, "Build");
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// DisplayDescriptor, Element, Projector

static void* MakeNode(const char* text)
{
    using Create = void*(__cdecl*)(void*, const unsigned char*);
    void* cache = *reinterpret_cast<void**>(GetProcAddress(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    return reinterpret_cast<Create>(GetProcAddress(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"))(
        cache, reinterpret_cast<const unsigned char*>(text));
}

static int Refs(void* node) { return node ? *reinterpret_cast<int*>(static_cast<char*>(node) + 0x10) : -1; }

static void TestDescriptor(int rounds)
{
    using CtorT = void*(__fastcall*)(void*, void*, int);
    using SetData = void(__cdecl*)(void*, void* const*, void* const*, void* const*, void* const*, float);
    using Dtor = void(__fastcall*)(void*, void*);
    using GetP = void* const*(__cdecl*)(const void*);
    using GetI = int(__cdecl*)(const void*);
    using GetF = float(__cdecl*)(const void*);
    const char* getP[] = {"?GetApiName@DisplayDescriptor@Jet@@QBAABVPString@2@XZ",
                          "?GetApiVersionString@DisplayDescriptor@Jet@@QBAABVPString@2@XZ",
                          "?GetCardVendorName@DisplayDescriptor@Jet@@QBAABVPString@2@XZ",
                          "?GetCardName@DisplayDescriptor@Jet@@QBAABVPString@2@XZ"};
    const char* words[] = {"OpenGL", "2.1", "NVIDIA Corporation", "GeForce", "Direct3D", "9.0c", "ATI", "Radeon"};
    void* nodes[8];
    for (int i = 0; i < 8; ++i) nodes[i] = MakeNode(words[i]);
    for (int r = 0; r < rounds; ++r) {
        unsigned char d[2][0x1c], e[2][0x1c];
        int type = Pick(0, 3);
        void* args[4];
        for (auto& a : args) a = Pick(0, 4) ? nodes[Pick(0, 7)] : nullptr;
        float number = SomeFloat();
        const bool reassign = Pick(0, 1) != 0;
        int before[8], after[2][8];
        for (int i = 0; i < 8; ++i) before[i] = Refs(nodes[i]);
        for (int s = 0; s < 2; ++s) {
            reinterpret_cast<CtorT>(Proc(s, "??0DisplayDescriptor@Jet@@IAE@W4APIType@01@@Z"))(d[s], nullptr, type);
            reinterpret_cast<SetData>(Proc(s, "?SetData@DisplayDescriptor@Jet@@IAAXABVPString@2@000M@Z"))(
                d[s], &args[0], &args[1], &args[2], &args[3], number);
            reinterpret_cast<Ctor1>(Proc(s, "??0DisplayDescriptor@Jet@@QAE@ABV01@@Z"))(e[s], nullptr, d[s]);
            if (reassign) {
                void* other[4] = {nodes[0], nullptr, nodes[r % 8], args[0]};
                reinterpret_cast<SetData>(Proc(s, "?SetData@DisplayDescriptor@Jet@@IAAXABVPString@2@000M@Z"))(
                    d[s], &other[0], &other[1], &other[2], &other[3], 2.5f);
                reinterpret_cast<Ctor1>(Proc(s, "??4DisplayDescriptor@Jet@@QAEAAV01@ABV01@@Z"))(e[s], nullptr, d[s]);
            }
            for (int i = 0; i < 8; ++i) after[s][i] = Refs(nodes[i]);
            Check(reinterpret_cast<GetI>(Proc(s, "?GetApiType@DisplayDescriptor@Jet@@QBA?AW4APIType@12@XZ"))(e[s]) == type, "GetApiType");
            for (const char* g : getP)
                Check(reinterpret_cast<GetP>(Proc(s, g))(e[s]) == reinterpret_cast<void* const*>(e[s] + (std::strstr(g, "ApiName") ? 8
                                                                                    : std::strstr(g, "Version") ? 0xc
                                                                                    : std::strstr(g, "Vendor") ? 0x14 : 0x18)),
                      g);
            float f = reinterpret_cast<GetF>(Proc(s, "?GetApiVersionNumber@DisplayDescriptor@Jet@@QBAMXZ"))(e[s]);
            Check(std::memcmp(&f, e[s] + 0x10, 4) == 0, "GetApiVersionNumber");
            reinterpret_cast<Dtor>(Proc(s, "??1DisplayDescriptor@Jet@@UAE@XZ"))(e[s], nullptr);
            reinterpret_cast<Dtor>(Proc(s, "??1DisplayDescriptor@Jet@@UAE@XZ"))(d[s], nullptr);
            for (int i = 0; i < 8; ++i) Check(Refs(nodes[i]) == before[i], "DisplayDescriptor refs back", std::to_string(i));
        }
        Same(d[0] + 4, d[1] + 4, 0x18, "DisplayDescriptor");
        Same(e[0] + 4, e[1] + 4, 0x18, "DisplayDescriptor(copy)");
        Same(after[0], after[1], sizeof after[0], "DisplayDescriptor refs");
    }
}

static void TestElementProjector(int rounds)
{
    using SetU = void(__cdecl*)(void*, unsigned);
    using Get = int(__cdecl*)(const void*);
    using Culled = bool(__cdecl*)(const void*, const void*);
    using Reset = void(__cdecl*)();
    using Slot = int(__cdecl*)(const void*);
    for (int r = 0; r < rounds; ++r) {
        unsigned char e[2][12], f[2][12];
        // Each module owns its Element::currentVisited counter; keep them in step.
        for (int s = 0; s < 2; ++s) reinterpret_cast<Reset>(Proc(s, "?ResetVisited@Element@Jet@@SAXXZ"))();
        unsigned frame = static_cast<unsigned>(Pick(0, 5));
        struct {
            unsigned char pad[0x3c];
            unsigned frame;
        } display;
        display.frame = static_cast<unsigned>(Pick(0, 5));
        const bool visit = Pick(0, 1) != 0;
        for (int s = 0; s < 2; ++s) {
            std::memset(e[s], 0x42, 12);
            reinterpret_cast<Ctor0>(Proc(s, "??0Element@Jet@@QAE@XZ"))(e[s], nullptr);
            reinterpret_cast<SetU>(Proc(s, "?SetLastCulledFrame@Element@Jet@@UAAXI@Z"))(e[s], frame);
            if (visit) reinterpret_cast<Get>(Proc(s, "?SetVisited@Element@Jet@@UBAXXZ"))(e[s]);
        }
        Same(e[0] + 4, e[1] + 4, 8, "Element");
        int v[2], c[2], l[2];
        for (int s = 0; s < 2; ++s) {
            v[s] = reinterpret_cast<Get>(Proc(s, "?IsVisited@Element@Jet@@QBA_NXZ"))(e[s]) & 0xff;
            c[s] = reinterpret_cast<Culled>(Proc(s, "?GetCulled@Element@Jet@@QBA_NPBVDisplay@2@@Z"))(e[s], &display);
            l[s] = reinterpret_cast<Get>(Proc(s, "?GetLastCulledFrame@Element@Jet@@QBAIXZ"))(e[s]);
            reinterpret_cast<Ctor1>(Proc(s, "??0Element@Jet@@QAE@ABV01@@Z"))(f[s], nullptr, e[s]);
            reinterpret_cast<Ctor1>(Proc(s, "??4Element@Jet@@QAEAAV01@ABV01@@Z"))(e[s], nullptr, f[s]);
            for (int k = 3; k <= 5; ++k) (*reinterpret_cast<Slot**>(e[s]))[k](e[s]);  // Cull, Render, RenderDebug
        }
        Check(v[0] == v[1] && c[0] == c[1] && l[0] == l[1], "Element getters");
        Same(f[0] + 4, f[1] + 4, 8, "Element(copy)");
        if (Pick(0, 2) == 0) {  // a new visit round leaves each side's elements unvisited
            for (int s = 0; s < 2; ++s) {
                reinterpret_cast<Reset>(Proc(s, "?ResetVisited@Element@Jet@@SAXXZ"))();
                v[s] = reinterpret_cast<Get>(Proc(s, "?IsVisited@Element@Jet@@QBA_NXZ"))(e[s]) & 0xff;
            }
            Check(v[0] == v[1], "Element after ResetVisited");
        }
        unsigned char p[2][4], q[2][4];
        for (int s = 0; s < 2; ++s) {
            reinterpret_cast<Ctor0>(Proc(s, "??0Projector@Jet@@QAE@XZ"))(p[s], nullptr);
            reinterpret_cast<Ctor1>(Proc(s, "??0Projector@Jet@@QAE@ABV01@@Z"))(q[s], nullptr, p[s]);
            reinterpret_cast<Ctor1>(Proc(s, "??4Projector@Jet@@QAEAAV01@ABV01@@Z"))(q[s], nullptr, p[s]);
        }
        Check((*reinterpret_cast<Slot**>(p[0]))[1](p[0]) == (*reinterpret_cast<Slot**>(p[1]))[1](p[1]), "Projector slot 1");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_rs_u1 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 200;
    SetDllDirectoryA(argv[1]);
    std::string dir = argv[1];
    LoadLibraryA((dir + "\\RenderSystem_7c_orig.dll").c_str());
    mod[0] = LoadLibraryA((dir + "\\RenderSystem_7c.dll").c_str());
    mod[1] = GetModuleHandleA("RenderSystem_7c_orig.dll");
    ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const char* only = argc > 3 ? argv[3] : "";
    struct {
        const char* name;
        void (*run)(int);
        int scale;
    } parts[] = {{"vertex", TestVertexRecords, 1}, {"format", TestDisplayFormat, 20}, {"light", TestLight, 5},
                 {"material", TestMaterial, 5}, {"rtc", TestRenderTextureConstruct, 5}, {"args", TestTextureArgs, 20},
                 {"stage", TestTextureStage, 5}, {"descriptor", TestDescriptor, 5}, {"element", TestElementProjector, 5}};
    for (auto& p : parts) {
        if (*only && std::strcmp(only, p.name) != 0) continue;
        std::printf("-- %s\n", p.name);
        p.run(rounds * p.scale);
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
