// Differential test of RenderSystem_7c unit U5: VertexType and its registry (VertexType.cpp), RenderingCache and
// RenderingCacheManager, VertexBufferManager, StaticBuffer and the chunk hints (RenderCaches.cpp), GeometryChunk
// (GeometryChunk.cpp: part "chunks", and part "interp" for the interpolation and copy), ours against
// RenderSystem_7c_orig.dll in one process.
//
// As in diff_rs_u3: both modules' imports of Jet::Alloc::New / Delete are hooked (blocks filled with 0xcd and
// numbered, frees logged by name), each scenario runs the same random script on both sides with that side's
// exports, and the logs must be equal. A stand-in Display logs the API data calls (slots 0x3c / 0x40); caches get a
// copy of their vftable whose slots log what they are handed. VertexType's statics (display, the registry) are the
// original's on both sides: each side starts with an empty registry and no display.
//
// usage: diff_rs_u5.exe <bin dir with RenderSystem_7c.dll, RenderSystem_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

static HMODULE mod[2];  // 0 ours, 1 the original
static HMODULE ks;
static unsigned long long checks, bad;
static std::mt19937 rng(5501);

static void* Proc(HMODULE m, const char* name)
{
    void* p = reinterpret_cast<void*>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}
template <class T>
static T As(void* p) { return reinterpret_cast<T>(p); }

// ---- KernelStandard and the heap hooks

using NewFn = void*(__cdecl*)(unsigned);
using DeleteFn = void(__cdecl*)(void*);
static NewFn RealNew;
static DeleteFn RealDelete;
static void* (*CreateNode)(void*, const unsigned char*);
static void** cache;
static void(__fastcall* AddRef)(const void*);
static bool(__fastcall* ReleaseRef)(const void*);

static int cur = -1;  // the side running
static std::string logText[2];
static std::map<const void*, std::string> names[2];
static std::map<const void*, unsigned> blockSize[2];
static int allocCount[2];

static void Log(const char* fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (cur >= 0) logText[cur] += buf;
    static const bool echo = std::getenv("RS_U5_ECHO") != nullptr;
    if (echo) std::fputs(buf, stdout);
}

static std::map<const void*, std::string> fixedNames;
// a block's name, or a block's name and the offset into it
static std::string Name(const void* p)
{
    if (!p) return "null";
    auto f = fixedNames.find(p);
    if (f != fixedNames.end()) return f->second;
    if (cur < 0) return "?";
    auto it = names[cur].find(p);
    if (it != names[cur].end()) return it->second;
    auto b = blockSize[cur].upper_bound(p);
    if (b != blockSize[cur].begin()) {
        --b;
        const auto off = static_cast<const char*>(p) - static_cast<const char*>(b->first);
        if (off >= 0 && static_cast<unsigned>(off) < b->second) {
            char t[64];
            std::snprintf(t, sizeof t, "%s+0x%x", names[cur][b->first].c_str(), static_cast<unsigned>(off));
            return t;
        }
    }
    return "?";
}

static void* __cdecl HookNew(unsigned n)
{
    void* p = RealNew(n);
    if (p && n < 0x1000000) std::memset(p, 0xcd, n);
    if (cur >= 0 && p) {
        blockSize[cur][p] = n;
        char b[32];
        std::snprintf(b, sizeof b, "A%d", allocCount[cur]++);
        names[cur][p] = b;
        Log("  new %u -> %s\n", n, b);
    }
    return p;
}
static void __cdecl HookDelete(void* p)
{
    if (cur >= 0 && p) {
        Log("  free %s\n", Name(p).c_str());
        names[cur].erase(p);
        blockSize[cur].erase(p);
    }
    RealDelete(p);
}

static void PatchImport(HMODULE m, const char* name, void* fn)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        auto* nm = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; nm->u1.AddressOfData; ++nm, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(nm->u1.Ordinal)) continue;
            if (std::strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + nm->u1.AddressOfData)->Name, name)) continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            iat->u1.Function = reinterpret_cast<uintptr_t>(fn);
            VirtualProtect(&iat->u1.Function, sizeof(void*), old, &old);
        }
    }
}

// a test-made block, named (filled with 0xcd like the hooked heap)
static void* TestNew(unsigned n, const char* fmt, int k)
{
    void* p = RealNew(n);
    std::memset(p, 0xcd, n);
    char b[32];
    std::snprintf(b, sizeof b, fmt, k);
    names[cur][p] = b;
    blockSize[cur][p] = n;
    return p;
}
static void TestDelete(void* p)
{
    names[cur].erase(p);
    blockSize[cur].erase(p);
    RealDelete(p);
}

// ---- strings

struct Node {
    void* next;
    void* prev;
    unsigned length, capacity;
    int refs;
    unsigned id;
    char text[1];
};
static const char* kTexts[] = {"Position", "Normal", "uv", "tangent", "Color0", "skin", "Texture1", "", "PNT",
                               "PCT", "grass"};
constexpr int kNames = sizeof kTexts / sizeof *kTexts;
static void* nodes[kNames + 1];  // the last one stays null
static std::string NodeText(const void* n)
{
    if (!n) return "null";
    const Node* x = static_cast<const Node*>(n);
    char b[96];
    std::snprintf(b, sizeof b, "\"%.40s\"/%u", x->text, x->length);
    return b;
}
static std::string Bits(float f)
{
    unsigned u;
    std::memcpy(&u, &f, 4);
    char b[16];
    std::snprintf(b, sizeof b, "%08x", u);
    return b;
}
static std::string Hex(const void* p, unsigned n)
{
    std::string s;
    char b[4];
    for (unsigned i = 0; i < n; ++i) {
        std::snprintf(b, sizeof b, "%02x", static_cast<const unsigned char*>(p)[i]);
        s += b;
    }
    return s;
}

// ---- the stand-in display (API data slots 0x3c add, 0x40 remove) and view

static void* displayVt[0x60];
static unsigned char displayObj[0x100], viewObj[0x40], displayObj2[0x100];
static int __cdecl DisplaySlot(const void* d) { Log("  display slot of %s\n", Name(d).c_str()); return 0; }
static void __cdecl DisplayAdd(const void* d, unsigned long* data)
{
    Log("  api add %s %s (%lx)\n", Name(d).c_str(), Name(reinterpret_cast<char*>(data) - 0x14).c_str(), *data);
}
static void __cdecl DisplayRemove(const void* d, unsigned long* data)
{
    Log("  api remove %s %s (%lx)\n", Name(d).c_str(), Name(reinterpret_cast<char*>(data) - 0x14).c_str(), *data);
}

// ---- per-side entry points

#define VT "VertexType@Jet@@"
#define RC "RenderingCache@Jet@@"
#define RCM "RenderingCacheManager@Jet@@"
#define VBM "VertexBufferManager@Jet@@"
#define DV "PAVDisplay@2@PAVViewPort@2@"

struct Api {
    // VertexType
    void*(__fastcall* ctor)(void*, void*, int);
    void*(__fastcall* copy)(void*, void*, const void*);
    void* vft;
    bool(__cdecl* eq)(const void*, const void*);
    bool(__cdecl* cmp)(const void*, const void*);
    void(__cdecl* createStreams)(void*, unsigned);
    void(__cdecl* createItems)(void*, unsigned, unsigned);
    void(__cdecl* addItem)(void*, unsigned, int, int, int, int, const void*, int, int);
    void(__cdecl* addShared)(void*, int, int, const void*);
    void(__cdecl* setApiData)(void*, const void*, unsigned long);
    unsigned long(__cdecl* getApiData)(const void*);
    unsigned long(__cdecl* getApiFlags)(const void*);
    void(__cdecl* setApiFlags)(void*, unsigned long);
    unsigned long(__cdecl* getUser)(const void*);
    void(__cdecl* setUser)(void*, unsigned long);
    int(__cdecl* getId)(const void*);
    unsigned(__cdecl* numStreams)(const void*);
    const void*(__cdecl* streamList)(const void*);
    unsigned(__cdecl* vertexSize)(const void*, unsigned);
    const void*(__cdecl* getItem)(const void*, int);
    int(__cdecl* itemOffset)(const void*, int);
    int(__cdecl* itemStream)(const void*, int);
    int(__cdecl* itemDataType)(const void*, int);
    int(__cdecl* itemSemantic)(const void*, int);
    int(__cdecl* itemCount)(const void*, int);
    bool(__cdecl* namedItem)(const void*, const void*, unsigned*, unsigned*, const void**, const void**);
    void(__cdecl* itemTypes)(const void*, void*);
    int(__cdecl* compatible)(const void*, int, int*, int);
    const void*(__cdecl* regName)(const void*);
    bool(__cdecl* registerName)(void*, const void*);
    unsigned(__cdecl* elemSize)(int);
    const void*(__cdecl* defaultName)(int);
    void*(__cdecl* getType)(const void*);
    void(__cdecl* shutdown)();
    void(__cdecl* configure)(void*, void*);
    // RenderingCache
    void*(__fastcall* rcCtor)(void*, void*, const void*, float, int);
    void(__fastcall* rcVbaseDtor)(void*, void*);
    void* rcVft;
    void* rcRcVft;
    const int* rcVbt;
    void(__cdecl* setHost)(void*, void*);
    void(__cdecl* activate)(void*, void*, void*);
    void(__cdecl* flush)(void*, void*, void*);
    void(__cdecl* enter)(void*, void*, void*, bool);
    void(__cdecl* leave)(void*, void*, void*, bool);
    void*(__cdecl* getHost)(const void*);
    const void*(__cdecl* getName)(const void*);
    float(__cdecl* getPriority)(const void*);
    void(__cdecl* setPriority)(void*, float);
    bool(__cdecl* less)(void*, const void*);
    bool(__cdecl* isEnabled)(const void*);
    void(__cdecl* enable)(void*, bool);
    bool(__cdecl* getGeo)(const void*);
    void(__cdecl* setGeo)(void*, bool);
    // RenderingCacheManager
    void*(__fastcall* mCtor)(void*, void*);
    void(__fastcall* mDtor)(void*, void*);
    void*(__fastcall* mAssign)(void*, void*, const void*);
    void(__cdecl* add)(void*, void*);
    void(__cdecl* remove)(void*, void*);
    void*(__cdecl* find)(void*, const void*);
    void(__cdecl* begin)(void*, void*, void*, bool);
    void(__cdecl* end)(void*, void*, void*);
    void(__cdecl* mEnter)(void*, void*, void*, bool);
    void(__cdecl* mLeave)(void*, void*, void*, bool);
    // VertexBufferManager
    void*(__fastcall* vbCtor)(void*, void*);
    void* vbVft;
    bool(__cdecl* initialize)(void*, void*, const void*);
    void(__cdecl* getConfig)(void*, const void*);
    void*(__cdecl* stats)(const void*, void*);
    unsigned(__cdecl* dynSize)(const void*);
    unsigned(__cdecl* dynIndex)(const void*);
    // StaticBuffer
    void*(__fastcall* sbCtor)(void*, void*);
    void* sbVft;
    // hints
    void*(__fastcall* hintCtor0[4])(void*, void*);
    void*(__fastcall* hintCtor2[4])(void*, void*, unsigned long, unsigned long);
    void*(__fastcall* hintAssign[4])(void*, void*, const void*);
    bool(__cdecl* supportsSystem)(const void*, unsigned long);
    bool(__cdecl* supportsUser)(const void*, unsigned long);
};
static Api api[2];

static void LoadApi(int s)
{
    Api& a = api[s];
    HMODULE m = mod[s];
#define F(name) Proc(m, name)
    a.ctor = As<decltype(a.ctor)>(F("??0" VT "QAE@W4VertexTypeID@1@@Z"));
    a.copy = As<decltype(a.copy)>(F("??0" VT "QAE@ABV01@@Z"));
    a.vft = F("??_7" VT "6B@");
    a.eq = As<decltype(a.eq)>(F("??8" VT "UBA_NABV01@@Z"));
    a.cmp = As<decltype(a.cmp)>(F("?CompareVertex@" VT "UBA_NABV12@@Z"));
    a.createStreams = As<decltype(a.createStreams)>(F("?CreateStreams@" VT "QAAXI@Z"));
    a.createItems = As<decltype(a.createItems)>(F("?CreateItems@" VT "QAAXII@Z"));
    a.addItem = As<decltype(a.addItem)>(F("?AddItem@" VT "QAAXIW4VertexItemType@2@HW4DVertexItemDataType@2@"
                                          "W4VertexItemSemanticType@2@ABVPString@2@HW4BoolEnum@12@@Z"));
    a.addShared = As<decltype(a.addShared)>(F("?AddItem@" VT "QAAXW4VertexItemType@2@0ABVPString@2@@Z"));
    a.setApiData = As<decltype(a.setApiData)>(F("?SetApiData@" VT "QAAXPBVDisplay@2@K@Z"));
    a.getApiData = As<decltype(a.getApiData)>(F("?GetApiData@" VT "QBAKXZ"));
    a.getApiFlags = As<decltype(a.getApiFlags)>(F("?GetApiFlags@" VT "QBAKXZ"));
    a.setApiFlags = As<decltype(a.setApiFlags)>(F("?SetApiFlags@" VT "QAAXK@Z"));
    a.getUser = As<decltype(a.getUser)>(F("?GetUserData@" VT "QBAKXZ"));
    a.setUser = As<decltype(a.setUser)>(F("?SetUserData@" VT "QAAXK@Z"));
    a.getId = As<decltype(a.getId)>(F("?GetVertexTypeID@" VT "QBA?AW4VertexTypeID@2@XZ"));
    a.numStreams = As<decltype(a.numStreams)>(F("?GetNumStreams@" VT "QBAIXZ"));
    a.streamList = As<decltype(a.streamList)>(F("?GetStreamList@" VT "QBAPBUStream@12@XZ"));
    a.vertexSize = As<decltype(a.vertexSize)>(F("?GetVertexSize@" VT "QBAII@Z"));
    a.getItem = As<decltype(a.getItem)>(F("?GetItem@" VT "QBAPBUItem@12@W4VertexItemType@2@@Z"));
    a.itemOffset = As<decltype(a.itemOffset)>(F("?GetItemOffset@" VT "QBAHW4VertexItemType@2@@Z"));
    a.itemStream = As<decltype(a.itemStream)>(F("?GetItemStream@" VT "QBAHW4VertexItemType@2@@Z"));
    a.itemDataType = As<decltype(a.itemDataType)>(F("?GetItemDataType@" VT "QBA?AW4DVertexItemDataType@2@W4VertexItemType@2@@Z"));
    a.itemSemantic = As<decltype(a.itemSemantic)>(F("?GetItemSemanticType@" VT "QBA?AW4VertexItemSemanticType@2@W4VertexItemType@2@@Z"));
    a.itemCount = As<decltype(a.itemCount)>(F("?GetItemElementCount@" VT "QBAHW4VertexItemType@2@@Z"));
    a.namedItem = As<decltype(a.namedItem)>(F("?GetNamedItemData@" VT "QBA_NABVPString@2@AAI1AAPBUStream@12@AAPBUItem@12@@Z"));
    a.itemTypes = As<decltype(a.itemTypes)>(F("?GetVertexItemTypes@" VT "QBAXAAV?$FastDynamicArray@W4VertexItemType@Jet@@$0N@U?$Less@W4VertexItemType@Jet@@W412@@2@@2@@Z"));
    a.compatible = As<decltype(a.compatible)>(F("?GetCompatibleVertexItemTypes@" VT "QBAHW4VertexItemSemanticType@2@PAW4VertexItemType@2@H@Z"));
    a.regName = As<decltype(a.regName)>(F("?GetRegisteredName@" VT "QBAABVPString@2@XZ"));
    a.registerName = As<decltype(a.registerName)>(F("?RegisterName@" VT "QAA_NABVPString@2@@Z"));
    a.elemSize = As<decltype(a.elemSize)>(F("?GetElementTypeSize@" VT "SAIW4DVertexItemDataType@2@@Z"));
    a.defaultName = As<decltype(a.defaultName)>(F("?GetDefaultVertexItemName@" VT "SAABVPString@2@W4VertexItemType@2@@Z"));
    a.getType = As<decltype(a.getType)>(F("?GetVertexType@" VT "SAPAV12@ABVPString@2@@Z"));
    a.shutdown = As<decltype(a.shutdown)>(F("?Shutdown@" VT "SAXXZ"));
    a.configure = As<decltype(a.configure)>(F("?ConfigureStream@" VT "IAAXPAUStream@12@@Z"));

    a.rcCtor = As<decltype(a.rcCtor)>(F("??0" RC "QAE@ABVPString@1@M@Z"));
    a.rcVbaseDtor = As<decltype(a.rcVbaseDtor)>(F("??_D" RC "QAEXXZ"));
    a.rcVft = F("??_7" RC "6B01@@");
    a.rcRcVft = F("??_7" RC "6BReferenceCount@1@@");
    a.rcVbt = static_cast<const int*>(F("??_8" RC "7B@"));
    a.setHost = As<decltype(a.setHost)>(F("?SetHost@" RC "UAAXPAVRenderingCacheManager@2@@Z"));
    a.activate = As<decltype(a.activate)>(F("?Activate@" RC "UAAX" DV "@Z"));
    a.flush = As<decltype(a.flush)>(F("?Flush@" RC "UAAX" DV "@Z"));
    a.enter = As<decltype(a.enter)>(F("?Enter@" RC "UAAX" DV "_N@Z"));
    a.leave = As<decltype(a.leave)>(F("?Leave@" RC "UAAX" DV "_N@Z"));
    a.getHost = As<decltype(a.getHost)>(F("?GetHost@" RC "QBAPAVRenderingCacheManager@2@XZ"));
    a.getName = As<decltype(a.getName)>(F("?GetName@" RC "QBAABVPString@2@XZ"));
    a.getPriority = As<decltype(a.getPriority)>(F("?GetPriority@" RC "QBAMXZ"));
    a.setPriority = As<decltype(a.setPriority)>(F("?SetPriority@" RC "QAAXM@Z"));
    a.less = As<decltype(a.less)>(F("??M" RC "QAA_NABV01@@Z"));
    a.isEnabled = As<decltype(a.isEnabled)>(F("?IsEnabled@" RC "QBA_NXZ"));
    a.enable = As<decltype(a.enable)>(F("?Enable@" RC "QAAX_N@Z"));
    a.getGeo = As<decltype(a.getGeo)>(F("?GetRendersGeometry@" RC "QBA_NXZ"));
    a.setGeo = As<decltype(a.setGeo)>(F("?SetRendersGeometry@" RC "QAAX_N@Z"));

    a.mCtor = As<decltype(a.mCtor)>(F("??0" RCM "QAE@XZ"));
    a.mDtor = As<decltype(a.mDtor)>(F("??1" RCM "QAE@XZ"));
    a.mAssign = As<decltype(a.mAssign)>(F("??4" RCM "QAEAAV01@ABV01@@Z"));
    a.add = As<decltype(a.add)>(F("?AddCache@" RCM "QAAXPAVRenderingCache@2@@Z"));
    a.remove = As<decltype(a.remove)>(F("?RemoveCache@" RCM "QAAXPAVRenderingCache@2@@Z"));
    a.find = As<decltype(a.find)>(F("?FindCache@" RCM "QAAPAVRenderingCache@2@ABVPString@2@@Z"));
    a.begin = As<decltype(a.begin)>(F("?Begin@" RCM "QAAX" DV "_N@Z"));
    a.end = As<decltype(a.end)>(F("?End@" RCM "QAAX" DV "@Z"));
    a.mEnter = As<decltype(a.mEnter)>(F("?Enter@" RCM "QAAX" DV "_N@Z"));
    a.mLeave = As<decltype(a.mLeave)>(F("?Leave@" RCM "QAAX" DV "_N@Z"));

    a.vbCtor = As<decltype(a.vbCtor)>(F("??0" VBM "IAE@XZ"));
    a.vbVft = F("??_7" VBM "6B@");
    a.initialize = As<decltype(a.initialize)>(F("?Initialize@" VBM "UAA_NPAVDisplay@2@PBVOptions@2@@Z"));
    a.getConfig = As<decltype(a.getConfig)>(F("?GetConfiguration@" VBM "MAAXPBVOptions@2@@Z"));
    a.stats = As<decltype(a.stats)>(F("?GetBufferStats@" VBM "QBA?AUBufferStats@12@XZ"));
    a.dynSize = As<decltype(a.dynSize)>(F("?GetDefaultDynamicBufferSize@" VBM "QBAIXZ"));
    a.dynIndex = As<decltype(a.dynIndex)>(F("?GetDefaultDynamicIndexSize@" VBM "QBAIXZ"));

    a.sbCtor = As<decltype(a.sbCtor)>(F("??0StaticBuffer@Jet@@QAE@XZ"));
    a.sbVft = F("??_7StaticBuffer@Jet@@6B@");

    const char* hints[4] = {"GeometryChunkCreateHint", "StandardChunkHint", "DatabaseChunkHint",
                            "TerrainMorphChunkHint"};
    for (int k = 0; k < 4; ++k) {
        const std::string h = hints[k];
        a.hintCtor0[k] = As<void*(__fastcall*)(void*, void*)>(F(("??0" + h + "@Jet@@QAE@XZ").c_str()));
        a.hintCtor2[k] =
            As<void*(__fastcall*)(void*, void*, unsigned long, unsigned long)>(F(("??0" + h + "@Jet@@QAE@KK@Z").c_str()));
        a.hintAssign[k] = As<void*(__fastcall*)(void*, void*, const void*)>(
            F(("??4" + h + "@Jet@@QAEAAV01@ABV01@@Z").c_str()));
    }
    a.supportsSystem = As<decltype(a.supportsSystem)>(F("?SupportsSystemHint@GeometryChunkCreateHint@Jet@@QBA_NK@Z"));
    a.supportsUser = As<decltype(a.supportsUser)>(F("?SupportsUserHint@GeometryChunkCreateHint@Jet@@QBA_NK@Z"));
#undef F
}

// the original's VertexType statics both sides use
static void** displayStatic;
static void (*origShutdown)();
static void ClearChunkRegistry();
static void FreshStatics()
{
    origShutdown();
    *displayStatic = nullptr;
    ClearChunkRegistry();
}

// ---- vertex types

struct Item {
    int type;
    void* name;
    int offset, count, dataType, semantic, sharedWith;
    unsigned char flag, pad[3];
};
struct Stream {
    unsigned stride, capacity, count;
    Item* items;
};
struct Type {
    void* vft;
    int refs;
    int id;
    unsigned streamCount;
    Stream* streams;
    unsigned long apiData, apiFlags, user;
    void* name;
};

static void DumpType(const char* tag, const void* p)
{
    const Type* t = static_cast<const Type*>(p);
    Log("  %s %s: vft %s refs %d id %d streams %u %s api %lx flags %lx user %lx name %s\n", tag, Name(p).c_str(),
        t->vft == api[cur].vft ? "own" : "other", t->refs, t->id, t->streamCount, Name(t->streams).c_str(), t->apiData,
        t->apiFlags, t->user, NodeText(t->name).c_str());
    for (unsigned k = 0; k < t->streamCount && t->streams; ++k) {
        const Stream& s = t->streams[k];
        Log("    stream %u: stride %u capacity %u count %u items %s\n", k, s.stride, s.capacity, s.count,
            Name(s.items).c_str());
        if (s.items)
            Log("      block count %u\n", reinterpret_cast<const unsigned*>(s.items)[-1]);
        for (unsigned j = 0; j < s.capacity && s.items; ++j) {
            const Item& i = s.items[j];
            Log("      %u: type %d name %s offset %d count %d data %d sem %d shared %d flag %02x\n", j, i.type,
                NodeText(i.name).c_str(), i.offset, i.count, i.dataType, i.semantic, i.sharedWith, i.flag);
        }
    }
}

struct Fda {
    struct Chunk {
        Chunk* next;
        Chunk* prev;
        unsigned count;
        int data[13];
    }* head;
};
static void DumpAndFreeFda(Fda& f)
{
    std::string s;
    char b[16];
    if (Fda::Chunk* c = f.head) {
        do {
            std::snprintf(b, sizeof b, "[%u]", c->count);
            s += b;
            for (unsigned k = 0; k < c->count; ++k) {
                std::snprintf(b, sizeof b, " %d", c->data[k]);
                s += b;
            }
            c = c->next;
        } while (c != f.head);
        Fda::Chunk* c2 = f.head->next;
        while (c2 != f.head) {
            Fda::Chunk* n = c2->next;
            HookDelete(c2);
            c2 = n;
        }
        HookDelete(f.head);
    }
    f.head = nullptr;
    Log("  item types %s\n", s.c_str());
}

static int RandType(std::mt19937& r)
{
    static const int common[] = {1, 2, 3, 4, 5, 9, 10, 15, 16, 23, 30, 38};
    return r() % 4 ? common[r() % 12] : static_cast<int>(r() % 39);
}

// the reference counts of the test's names and the default item names
static std::vector<int> NameRefs(const Api& a)
{
    std::vector<int> v;
    for (int k = 0; k < kNames; ++k) v.push_back(static_cast<const Node*>(nodes[k])->refs);
    for (int k = 0; k < 39; ++k) v.push_back(static_cast<const Node*>(*static_cast<void* const*>(a.defaultName(k)))->refs);
    return v;
}

static void RunTypes(int s, unsigned seed)
{
    std::mt19937 r(seed);
    const Api& a = api[s];
    const std::vector<int> refsBefore = NameRefs(a);
    std::vector<void*> types;
    int made = 0;
    auto make = [&](int id) {
        void* p = TestNew(0x24, "T%d", made++);
        a.ctor(p, nullptr, id);
        types.push_back(p);
        Log("op new type %s id %d\n", Name(p).c_str(), id);
        return p;
    };
    auto name = [&]() { return &nodes[r() % (kNames + 1)]; };
    auto build = [&](void* t) {
        const unsigned ns = 1 + r() % 3;
        a.createStreams(t, ns);
        for (unsigned k = 0; k < ns; ++k) {
            const unsigned cap = r() % 6;
            if (r() % 8) a.createItems(t, k, cap);
            const unsigned adds = cap + r() % 2;
            for (unsigned j = 0; j < adds; ++j) {
                if (j > 0 && r() % 5 == 0) {
                    const Item* items = static_cast<const Type*>(t)->streams[k].items;
                    const unsigned have = static_cast<const Type*>(t)->streams[k].count;
                    const int shared = have && items ? items[r() % have].type : RandType(r);
                    a.addShared(t, RandType(r), shared, name());
                    continue;
                }
                const int type = RandType(r);
                const int count = r() % 3 ? -1 : r() % 6 ? static_cast<int>(r() % 5) : -2 - static_cast<int>(r() % 3);
                const int dt = r() % 3 ? 0 : static_cast<int>(r() % 9);
                const int sem = r() % 3 ? 1 : static_cast<int>(r() % 8);
                const int offset = r() % 4 ? -1 : static_cast<int>(r() % 40);
                static const int flags[] = {-1, -1, 0, 1, 2};
                a.addItem(t, k, type, count, dt, sem, name(), offset, flags[r() % 5]);
            }
        }
    };
    const int n = 1 + r() % 4;
    for (int k = 0; k < n; ++k) {
        void* t = make(static_cast<int>(r() % 60));
        build(t);
        DumpType("built", t);
    }
    for (int step = 0, steps = 10 + r() % 30; step < steps; ++step) {
        void* t = types[r() % types.size()];
        void* u = types[r() % types.size()];
        const int type = RandType(r);
        switch (r() % 19) {
        case 0: {
            void* c = TestNew(0x24, "T%d", made++);
            Log("op copy %s -> %s\n", Name(t).c_str(), Name(c).c_str());
            a.copy(c, nullptr, t);
            types.push_back(c);
            DumpType("copy", c);
            Log("  same %d %d / %d\n", a.eq(c, t), a.cmp(c, t), a.eq(t, c));
            break;
        }
        case 1:
            Log("op eq %s %s: %d %d\n", Name(t).c_str(), Name(u).c_str(), a.eq(t, u), a.cmp(t, u));
            break;
        case 2: {
            const void* d = r() % 3 ? static_cast<const void*>(r() % 2 ? displayObj : displayObj2) : nullptr;
            const unsigned long v = r() % 3 ? r() % 4 : 0;
            Log("op api data %s %s %lx\n", Name(t).c_str(), Name(d).c_str(), v);
            a.setApiData(t, d, v);
            Log("  -> %lx display %s\n", a.getApiData(t), Name(*displayStatic).c_str());
            break;
        }
        case 3: {
            const unsigned long f = r(), x = r();
            a.setApiFlags(t, f);
            a.setUser(t, x);
            Log("op flags %s %lx %lx id %d streams %u list %s\n", Name(t).c_str(), a.getApiFlags(t), a.getUser(t),
                a.getId(t), a.numStreams(t), Name(a.streamList(t)).c_str());
            break;
        }
        case 4: {
            Log("op item %s %d: %s offset %d stream %d data %d sem %d count %d\n", Name(t).c_str(), type,
                Name(a.getItem(t, type)).c_str(), a.itemOffset(t, type), a.itemStream(t, type), a.itemDataType(t, type),
                a.itemSemantic(t, type), a.itemCount(t, type));
            break;
        }
        case 5: {
            const unsigned ns = static_cast<const Type*>(t)->streamCount;
            if (ns) {
                const unsigned k = r() % ns;
                Log("op vertex size %s %u: %u\n", Name(t).c_str(), k, a.vertexSize(t, k));
            }
            break;
        }
        case 6: {
            unsigned si = 77, ii = 77;
            const void *sp = nullptr, *ip = nullptr;
            void** nm = name();
            const bool ok = a.namedItem(t, nm, &si, &ii, &sp, &ip);
            Log("op named %s %s: %d %u %u %s %s\n", Name(t).c_str(), NodeText(*nm).c_str(), ok, si, ii,
                Name(sp).c_str(), Name(ip).c_str());
            break;
        }
        case 7: {
            Fda f{nullptr};
            Log("op item types %s\n", Name(t).c_str());
            a.itemTypes(t, &f);
            if (r() % 2) a.itemTypes(u, &f);
            DumpAndFreeFda(f);
            break;
        }
        case 8: {
            int out[64];
            std::memset(out, 0xee, sizeof out);
            const int sem = static_cast<int>(r() % 8);
            const int k = a.compatible(t, sem, out, 2);
            std::string o;
            for (int i = 0; i < k && i < 64; ++i) o += " " + std::to_string(out[i]);
            Log("op compatible %s %d: %d%s\n", Name(t).c_str(), sem, k, o.c_str());
            break;
        }
        case 9: {
            void** nm = name();
            const bool ok = a.registerName(t, nm);
            Log("op register %s %s: %d name %s\n", Name(t).c_str(), NodeText(*nm).c_str(), ok,
                NodeText(*static_cast<void* const*>(a.regName(t))).c_str());
            DumpType("registered", t);
            break;
        }
        case 10: {
            void** nm = name();
            void* got = a.getType(nm);
            Log("op get type %s: %s\n", NodeText(*nm).c_str(), Name(got).c_str());
            if (got) {
                DumpType("got", got);
                ReleaseRef(got);
            }
            break;
        }
        case 11: {
            const int dt = static_cast<int>(r() % 9);
            const int it = static_cast<int>(r() % 39);
            Log("op statics %d: %u; %d: %s\n", dt, a.elemSize(dt), it,
                NodeText(*static_cast<void* const*>(a.defaultName(it))).c_str());
            break;
        }
        case 12: {
            // ConfigureStream on a stream (does nothing until it is full)
            Type* tt = static_cast<Type*>(t);
            if (tt->streamCount) {
                Stream* st = &tt->streams[r() % tt->streamCount];
                if (st->items && st->count == st->capacity)
                    for (unsigned j = 0; j < st->count; ++j)
                        if (r() % 3 == 0) st->items[j].offset = -1;
                Log("op configure %s %s\n", Name(t).c_str(), Name(st).c_str());
                a.configure(t, st);
                DumpType("configured", t);
            }
            break;
        }
        case 13: {
            if (types.size() > 1) {
                auto at = types.begin() + r() % types.size();
                Log("op release %s\n", Name(*at).c_str());
                ReleaseRef(*at);
                types.erase(at);
            }
            break;
        }
        case 14: {
            Log("op rebuild-like new\n");
            void* t2 = make(static_cast<int>(r() % 60));
            build(t2);
            DumpType("built", t2);
            break;
        }
        case 15: {
            // the remaining items of a stream (AddItem past the end does nothing)
            Type* tt = static_cast<Type*>(t);
            if (tt->streamCount) {
                const unsigned k = r() % tt->streamCount;
                Log("op add more %s %u\n", Name(t).c_str(), k);
                a.addItem(t, k, type, -1, 0, 1, name(), -1, -1);
                DumpType("more", t);
            }
            break;
        }
        case 16:
            Log("op shutdown\n");
            a.shutdown();
            break;
        case 17: {
            // a copy that differs from its source in one field of one item slot (or a stream's stride/capacity)
            Type* src = static_cast<Type*>(t);
            if (!src->streamCount) break;
            void* c = TestNew(0x24, "T%d", made++);
            a.copy(c, nullptr, t);
            types.push_back(c);
            Type* tc = static_cast<Type*>(c);
            const unsigned sk = r() % tc->streamCount;
            Stream& st = tc->streams[sk];
            const int what = static_cast<int>(r() % 10);
            const unsigned ik = st.capacity ? r() % st.capacity : 0;
            Item* it = st.capacity && st.items ? &st.items[ik] : nullptr;
            int* field = nullptr;
            switch (what) {
            case 0: field = reinterpret_cast<int*>(&st.stride); break;
            case 1: field = reinterpret_cast<int*>(&st.capacity); break;
            case 2: field = it ? &it->type : nullptr; break;
            case 3: field = it ? &it->offset : nullptr; break;
            case 4: field = it ? &it->count : nullptr; break;
            case 5: field = it ? &it->dataType : nullptr; break;
            case 6: field = it ? &it->semantic : nullptr; break;
            case 7: field = it ? &it->sharedWith : nullptr; break;
            case 8: field = reinterpret_cast<int*>(&st.count); break;
            default:
                if (it) it->flag = static_cast<unsigned char>(it->flag ^ (1 + r() % 0xff));
                break;
            }
            int was = 0;
            if (field) {
                was = *field;
                *field = what == 1 ? (was ? was - 1 : 0) : was + 1;
            }
            Log("op tweak %s -> %s field %d: %d %d %d %d\n", Name(t).c_str(), Name(c).c_str(), what, a.cmp(c, t),
                a.cmp(t, c), a.eq(c, t), a.eq(t, c));
            if (field) *field = was;
            if (what == 9 && it) it->flag = src->streams[sk].items[ik].flag;
            break;
        }
        default:
            DumpType("look", t);
            break;
        }
    }
    Log("op shutdown\n");
    a.shutdown();
    for (void* t : types) {
        Log("op release %s\n", Name(t).c_str());
        ReleaseRef(t);
    }
    // every name reference given back (the default names included): the change over the scenario
    const std::vector<int> refsAfter = NameRefs(a);
    std::string refs;
    for (size_t k = 0; k < refsAfter.size(); ++k) refs += " " + std::to_string(refsAfter[k] - refsBefore[k]);
    Log("op name refs%s\n", refs.c_str());
}

// ---- caches

static void* cacheTestVt[2][8];
static void __cdecl CSetHost(void* self, void* host)
{
    Log("  set host %s %s\n", Name(self).c_str(), Name(host).c_str());
    api[cur].setHost(self, host);
}
static void __cdecl CActivate(void* self, void* d, void* v) { Log("  activate %s %s %s\n", Name(self).c_str(), Name(d).c_str(), Name(v).c_str()); }
static void __cdecl CReset(void* self) { Log("  reset %s\n", Name(self).c_str()); }
static void __cdecl CBegin(void* self, void* d, void* v) { Log("  begin %s %s %s\n", Name(self).c_str(), Name(d).c_str(), Name(v).c_str()); }
static void __cdecl CEnd(void* self, void* d, void* v) { Log("  end %s %s %s\n", Name(self).c_str(), Name(d).c_str(), Name(v).c_str()); }
static void __cdecl CFlush(void* self, void* d, void* v) { Log("  flush %s %s %s\n", Name(self).c_str(), Name(d).c_str(), Name(v).c_str()); }
static void __cdecl CEnter(void* self, void* d, void* v, bool b)
{
    Log("  enter %s %s %s %d\n", Name(self).c_str(), Name(d).c_str(), Name(v).c_str(), b);
}
static void __cdecl CLeave(void* self, void* d, void* v, bool b)
{
    Log("  leave %s %s %s %d\n", Name(self).c_str(), Name(d).c_str(), Name(v).c_str(), b);
}
static void MakeCacheVtables(int s)
{
    void** v = cacheTestVt[s];
    v[0] = reinterpret_cast<void*>(&CSetHost);
    v[1] = reinterpret_cast<void*>(&CActivate);
    v[2] = reinterpret_cast<void*>(&CReset);
    v[3] = reinterpret_cast<void*>(&CBegin);
    v[4] = reinterpret_cast<void*>(&CEnd);
    v[5] = reinterpret_cast<void*>(&CFlush);
    v[6] = reinterpret_cast<void*>(&CEnter);
    v[7] = reinterpret_cast<void*>(&CLeave);
}

struct CacheObj {
    void* vft;
    void* next;
    void* prev;
    const int* vbt;
    void* name;
    float priority;
    void* host;
    unsigned char geo, enabled, pad[2];
    void* rcVft;
    int refs;
};
static_assert(sizeof(CacheObj) == 0x28, "cache");
struct Mgr {
    void* next;
    void* prev;
    unsigned count;
    unsigned char geometryOnly, pad[3];
};

static void DumpCache(const char* tag, const void* p)
{
    const CacheObj* c = static_cast<const CacheObj*>(p);
    Log("  %s %s: vft %s next %s prev %s vbt %s name %s prio %s host %s geo %d en %d pad %s rc %s refs %d\n", tag,
        Name(p).c_str(), c->vft == api[cur].rcVft ? "own" : c->vft == cacheTestVt[cur] ? "test" : "other",
        Name(c->next).c_str(), Name(c->prev).c_str(), c->vbt == api[cur].rcVbt ? "own" : "other",
        NodeText(c->name).c_str(), Bits(c->priority).c_str(), Name(c->host).c_str(), c->geo, c->enabled,
        Hex(c->pad, 2).c_str(), c->rcVft == api[cur].rcRcVft ? "own" : "other", c->refs);
}
static void DumpMgr(const void* m)
{
    const Mgr* g = static_cast<const Mgr*>(m);
    std::string s;
    const char* end = static_cast<const char*>(m) - 4;
    int guard = 0;
    for (const char* n = static_cast<const char*>(g->next); n && n - 4 != end && guard < 64; ++guard) {
        const CacheObj* c = reinterpret_cast<const CacheObj*>(n - 4);
        s += " " + Name(c) + "(" + Bits(c->priority) + ")";
        n = static_cast<const char*>(c->next);
    }
    Log("  manager %s: count %u geo %d prev %s list%s\n", Name(m).c_str(), g->count, g->geometryOnly,
        Name(g->prev).c_str(), s.c_str());
}

static void RunCaches(int s, unsigned seed)
{
    std::mt19937 r(seed);
    const Api& a = api[s];
    std::vector<void*> caches, mgrs;
    int made = 0;
    auto prio = [&]() {
        static const float ps[] = {0.f, 1.f, -1.f, 0.5f, 2.f, 100.f, -0.f};
        return r() % 4 ? ps[r() % 7] : static_cast<float>(static_cast<int>(r() % 2000) - 1000) / 7.f;
    };
    auto newCache = [&]() {
        void* p = TestNew(0x28, "C%d", made++);
        void** nm = &nodes[r() % (kNames + 1)];
        const float pr = prio();
        Log("op new cache %s %s %s\n", Name(p).c_str(), NodeText(*nm).c_str(), Bits(pr).c_str());
        a.rcCtor(p, nullptr, nm, pr, 1);
        DumpCache("made", p);
        *static_cast<void**>(p) = cacheTestVt[s];
        caches.push_back(p);
    };
    for (int k = 0; k < 2; ++k) {
        void* m = TestNew(0x10, "M%d", k);
        a.mCtor(m, nullptr);
        mgrs.push_back(m);
        DumpMgr(m);
    }
    for (int k = 0, n = 2 + r() % 5; k < n; ++k) newCache();
    for (int step = 0, steps = 10 + r() % 40; step < steps; ++step) {
        void* c = caches.empty() ? nullptr : caches[r() % caches.size()];
        void* c2 = caches.empty() ? nullptr : caches[r() % caches.size()];
        void* m = mgrs[r() % mgrs.size()];
        void* d = r() % 2 ? displayObj : displayObj2;
        void* v = r() % 3 ? viewObj : nullptr;
        const bool b = r() % 2;
        switch (r() % 16) {
        case 0:
            newCache();
            break;
        case 1:
        case 2: {
            // only a cache without a host (or null)
            void* x = c && !static_cast<CacheObj*>(c)->host ? c : nullptr;
            if (!x && r() % 3) break;
            Log("op add %s %s\n", Name(m).c_str(), Name(x).c_str());
            a.add(m, x);
            DumpMgr(m);
            break;
        }
        case 3:
            if (!c) break;
            Log("op remove %s %s\n", Name(m).c_str(), Name(c).c_str());
            a.remove(m, c);
            DumpMgr(m);
            break;
        case 4: {
            void** nm = &nodes[r() % (kNames + 1)];
            Log("op find %s %s: %s\n", Name(m).c_str(), NodeText(*nm).c_str(), Name(a.find(m, nm)).c_str());
            break;
        }
        case 5:
            if (!c) break;
            {
                const float pr = prio();
                Log("op priority %s %s\n", Name(c).c_str(), Bits(pr).c_str());
                a.setPriority(c, pr);
                Log("  -> %s\n", Bits(a.getPriority(c)).c_str());
                for (void* g : mgrs) DumpMgr(g);
            }
            break;
        case 6:
            Log("op begin %s %d\n", Name(m).c_str(), b);
            a.begin(m, d, v, b);
            break;
        case 7:
            Log("op end %s\n", Name(m).c_str());
            a.end(m, d, v);
            break;
        case 8:
            Log("op enter %s %d\n", Name(m).c_str(), b);
            a.mEnter(m, d, v, b);
            break;
        case 9:
            Log("op leave %s %d\n", Name(m).c_str(), b);
            a.mLeave(m, d, v, b);
            break;
        case 10:
            if (!c) break;
            a.enable(c, r() % 2);
            a.setGeo(c, r() % 2);
            Log("op flags %s: %d %d host %s name %s\n", Name(c).c_str(), a.isEnabled(c), a.getGeo(c),
                Name(a.getHost(c)).c_str(), NodeText(*static_cast<void* const*>(a.getName(c))).c_str());
            break;
        case 11:
            if (!c) break;
            Log("op less %s %s: %d\n", Name(c).c_str(), Name(c2).c_str(), a.less(c, c2));
            break;
        case 12:
            if (!c) break;
            Log("op cache calls %s\n", Name(c).c_str());
            a.flush(c, d, v);
            a.activate(c, d, v);
            a.enter(c, d, v, b);
            a.leave(c, d, v, b);
            break;
        case 13:
            if (!c) break;
            {
                // released: out of its manager
                auto at = std::find(caches.begin(), caches.end(), c);
                Log("op release %s\n", Name(c).c_str());
                ReleaseRef(static_cast<char*>(c) + 0x20);
                caches.erase(at);
                for (void* g : mgrs) DumpMgr(g);
            }
            break;
        case 14:
            if (!c) break;
            DumpCache("look", c);
            break;
        default: {
            void* copy = TestNew(0x10, "M%d", 100 + step);
            std::memset(copy, 0x5a, 0x10);
            a.mAssign(copy, nullptr, m);
            Log("op assign %s\n", Name(m).c_str());
            const bool same = !std::memcmp(copy, m, 0x10);
            Log("  same %d\n", same);
            TestDelete(copy);
            break;
        }
        }
    }
    // a manager let go of with caches in it, then the caches (half through the vbase destructor)
    Log("op manager destructors\n");
    for (void* m : mgrs) {
        a.mDtor(m, nullptr);
        TestDelete(m);
    }
    for (void* c : caches) {
        DumpCache("last", c);
        if (r() % 2) {
            Log("op vbase dtor %s\n", Name(c).c_str());
            a.rcVbaseDtor(c, nullptr);
            TestDelete(c);
        } else {
            Log("op release %s\n", Name(c).c_str());
            ReleaseRef(static_cast<char*>(c) + 0x20);
        }
    }
}

// ---- buffers and hints

static unsigned char optionsObj[0x40];
static void RunMisc(int s, unsigned seed)
{
    std::mt19937 r(seed);
    const Api& a = api[s];
    // VertexBufferManager
    {
        void* p = TestNew(0x78, "V%d", 0);
        a.vbCtor(p, nullptr);
        Log("op vbm made: vft %s %s\n", *static_cast<void**>(p) == a.vbVft ? "own" : "other", Hex(p, 0x6c).c_str() + 8);
        Log("  sizes %u %u\n", a.dynSize(p), a.dynIndex(p));
        Log("  init %d\n", a.initialize(p, displayObj, optionsObj));
        if (r() % 2) a.getConfig(p, optionsObj);
        Log("  config %u %u display %s\n", a.dynSize(p), a.dynIndex(p), Name(static_cast<void**>(p)[1]).c_str());
        for (int k = 0; k < 0x12; ++k) static_cast<unsigned*>(p)[4 + k] = r();
        unsigned st[0x12 + 2];
        std::memset(st, 0xee, sizeof st);
        void* ret = a.stats(p, st);
        Log("  stats %s ret %s\n", Hex(st, sizeof st).c_str(), ret == st ? "out" : "other");
        // the slots that answer 0
        static const int zero[] = {2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 19, 21, 22};
        void** vt = *static_cast<void***>(p);
        std::string z;
        for (int k : zero) z += std::to_string(As<int(__cdecl*)(void*, int, int)>(vt[k])(p, 1, 2)) + " ";
        static const int empty[] = {4, 14, 15, 16, 17, 18};
        for (int k : empty) As<void(__cdecl*)(void*, int, int)>(vt[k])(p, 1, 2);
        Log("  zero slots %s\n", z.c_str());
        Log("op vbm delete\n");
        As<void*(__fastcall*)(void*, void*, unsigned)>(vt[0])(p, nullptr, 1);
    }
    // StaticBuffer
    {
        void* p = TestNew(8, "S%d", 0);
        a.sbCtor(p, nullptr);
        Log("op static buffer: vft %s refs %d\n", *static_cast<void**>(p) == a.sbVft ? "own" : "other",
            static_cast<int*>(p)[1]);
        AddRef(p);
        Log("  release %d\n", ReleaseRef(p));
        Log("  release %d\n", ReleaseRef(p));
    }
    // hints
    static const unsigned sizes[4] = {0x10, 0x20, 0x24, 0x24};
    for (int k = 0; k < 4; ++k) {
        void* h = TestNew(0x24, "H%d", k);
        void* h2 = TestNew(0x24, "H%d", 10 + k);
        const unsigned long sys = r() % 3 ? r() : r() % 8, user = r();
        a.hintCtor0[k](h, nullptr);
        Log("op hint %d: %s\n", k, Hex(h, sizes[k]).c_str());
        a.hintCtor2[k](h2, nullptr, sys, user);
        Log("op hint %d %lx %lx: %s\n", k, sys, user, Hex(h2, sizes[k]).c_str());
        for (int j = 0; j < 4; ++j) {
            const unsigned long bit = 1ul << (r() % 32) | (r() % 4 ? 0 : r());
            Log("  supports %lx: %d %d\n", bit, a.supportsSystem(h2, bit), a.supportsUser(h2, bit));
        }
        for (unsigned j = 0; j < sizes[k]; ++j) static_cast<unsigned char*>(h2)[j] = static_cast<unsigned char>(r());
        a.hintAssign[k](h, nullptr, h2);
        Log("  assigned %s\n", Hex(h, 0x24).c_str());
        TestDelete(h);
        TestDelete(h2);
    }
}

// ---- geometry chunks (U5b1)

#define GC "GeometryChunk@Jet@@"
#define GC_FDA "ABV?$FastDynamicArray@W4VertexItemType@Jet@@$0N@U?$Less@W4VertexItemType@Jet@@W412@@2@@2@H@Z"

struct ApiC {
    void*(__fastcall* ctor0)(void*, void*);
    void*(__fastcall* ctor5)(void*, void*, unsigned, unsigned, unsigned, unsigned, unsigned);
    void** vft;
    void*(__fastcall* assign)(void*, void*, const void*);
    void(__cdecl* initInstance)(const void*, void*);
    bool(__cdecl* initBuffers)(void*, void*, bool);
    unsigned long(__cdecl* getFlags)(const void*);
    void(__cdecl* setFlags)(void*, unsigned long);
    void(__cdecl* setConstFlags)(const void*, unsigned long);
    bool(__cdecl* unshared)(const void*);
    unsigned(__cdecl* getU[8])(const void*);  // vertices, max vertices, indices, max indices, primitives, face normals, face neighbours, user
    void(__cdecl* setU[6])(void*, unsigned);  // vertices, max vertices, indices, primitives, face normals, user
    float(__cdecl* getOpacity)(const void*);
    float(__cdecl* getSurface)(const void*);
    void(__cdecl* setSurface)(void*, float);
    void*(__cdecl* getRefUser)(const void*);
    void(__cdecl* setRefUser)(void*, void*);
    void*(__cdecl* getIndexArray)(const void*);
    void*(__cdecl* getFaceNormals)(const void*);
    void*(__cdecl* getFaceNeighbors)(const void*);
    void(__cdecl* setIndexArray)(void*, void*);
    void(__cdecl* setFaceNormals)(void*, void*);
    void(__cdecl* setFaceNeighbors)(void*, void**);
    void(__cdecl* dupFaceNormals)(void*);
    void(__cdecl* dupFaceNeighbors)(void*);
    void(__cdecl* resizeFaceNormals)(void*, unsigned);
    void(__cdecl* resizeFaceNeighbors)(void*, unsigned);
    bool(__cdecl* vertexIndex3)(const void*, int, int, unsigned*);
    unsigned(__cdecl* vertexIndex2)(const void*, int, int);
    void(__cdecl* triangle)(const void*, int, unsigned long*, unsigned long*, unsigned long*);
    bool(__cdecl* addProp)(void*, const void*);
    bool(__cdecl* setProp)(void*, const void*, float);
    bool(__cdecl* getProp)(const void*, const void*, float*);
    bool(__cdecl* removeProp)(void*, const void*);
    bool(__cdecl* propExists)(const void*, const void*);
    int(__cdecl* addInfluence)(void*, int);
    bool(__cdecl* assignInfluence)(void*, int, const void*);
    void(__cdecl* resetMatrices)(void*);
    void*(__cdecl* influenceArray)(const void*);
    void(__cdecl* setVertexType)(const void*, void*, int, int);
    void(__cdecl* setOpacity)(void*, void*, float);
    void(__cdecl* renderColor)(const void*, void*);
    void(__cdecl* renderOutlines)(const void*, void*);
    void(__cdecl* renderVertexNormals)(const void*, void*);
    void(__cdecl* renderTriangleNormals)(const void*, void*);
    void(__cdecl* renderAxes)(const void*, void*);
    bool(__cdecl* registerName)(const void*, void* (*)());
    void*(__cdecl* createRegistered)(const void*);
    void(__cdecl* releaseRegistrations)();
    void*(__cdecl* udword)(const void*, void*, int, int);
    void*(__cdecl* udwordConst)(const void*, void*, int, int);
    // virtual slots called directly
    void(__cdecl* dupUnshared)(void*, void*);
    void(__cdecl* dupIndex)(void*, void*);
    void(__cdecl* resizeIndex)(void*, void*, unsigned);
    void(__cdecl* resizeVertex)(void*, void*, unsigned, int, int);
    void(__cdecl* append)(void*, void*, const void*);
    void(__cdecl* setIndexDynamic)(void*);
    bool(__cdecl* getIndexDynamic)(const void*);
    void(__cdecl* setIndexDirty)(const void*, bool);
    bool(__cdecl* getIndexDirty)(const void*);
    void(__cdecl* updateLighting)(void*, void*, const void*, void*, const void*, const void*);
    void(__cdecl* resetToAmbient)(void*, void*, const void*, const void*);
    void(__cdecl* renderDebug)(const void*, void*);
    bool(__cdecl* loadType)(const void*, const void*, unsigned*, bool*);
    bool(__cdecl* loadData)(void*, const void*, void**);
};
static ApiC apiC[2];

static void LoadApiC(int s)
{
    ApiC& a = apiC[s];
    HMODULE m = mod[s];
#define F(name) Proc(m, name)
#define A(field, name) a.field = As<decltype(a.field)>(F(name))
    A(ctor0, "??0" GC "QAE@XZ");
    A(ctor5, "??0" GC "QAE@IIIII@Z");
    a.vft = static_cast<void**>(F("??_7" GC "6B@"));
    A(assign, "??4" GC "AAEABV01@ABV01@@Z");
    A(initInstance, "?InitInstance@" GC "IBAXAAV12@@Z");
    A(initBuffers, "?InitializeBuffers@" GC "UAA_NPAVDisplay@2@_N@Z");
    A(getFlags, "?GetFlags@" GC "QBAKXZ");
    A(setFlags, "?SetFlags@" GC "QAAXK@Z");
    A(setConstFlags, "?SetConstFlags@" GC "IBAXK@Z");
    A(unshared, "?ContainsUnsharedData@" GC "QBA_NXZ");
    const char* gets[8] = {"?GetNumVertices@" GC "QBAIXZ", "?GetMaxVertices@" GC "QBAIXZ", "?GetNumIndices@" GC "QBAIXZ",
                           "?GetMaxIndices@" GC "QBAIXZ",  "?GetNumPrimitives@" GC "QBAIXZ", "?GetNumFaceNormals@" GC "QBAIXZ",
                           "?GetNumFaceNeighbors@" GC "QBAIXZ", "?GetUserData@" GC "QBAKXZ"};
    for (int k = 0; k < 8; ++k) a.getU[k] = As<unsigned(__cdecl*)(const void*)>(F(gets[k]));
    const char* sets[6] = {"?SetNumVertices@" GC "QAAXI@Z", "?SetMaxVertices@" GC "QAAXI@Z", "?SetNumIndices@" GC "QAAXI@Z",
                           "?SetNumPrimitives@" GC "QAAXI@Z", "?SetNumFaceNormals@" GC "QAAXI@Z", "?SetUserData@" GC "QAAXK@Z"};
    for (int k = 0; k < 6; ++k) a.setU[k] = As<void(__cdecl*)(void*, unsigned)>(F(sets[k]));
    A(getOpacity, "?GetOpacity@" GC "QBAMXZ");
    A(getSurface, "?GetSurfaceArea@" GC "QBAMXZ");
    A(setSurface, "?SetSurfaceArea@" GC "QAAXM@Z");
    A(getRefUser, "?GetRefefencedUserData@" GC "QBAPAVReferenceCount@2@XZ");
    A(setRefUser, "?SetReferencedUserData@" GC "QAAXPAVReferenceCount@2@@Z");
    A(getIndexArray, "?GetIndexArray@" GC "QBAPBGXZ");
    A(getFaceNormals, "?GetFaceNormalArray@" GC "QBAPBVVector3@2@XZ");
    A(getFaceNeighbors, "?GetFaceNeighborArray@" GC "QBAPBUFaceNeighbors@12@XZ");
    A(setIndexArray, "?SetIndexArray@" GC "QAAXV?$AutoReferenceArray@G@2@@Z");
    A(setFaceNormals, "?SetFaceNormalArray@" GC "QAAXV?$AutoReferenceArray@VVector3@Jet@@@2@@Z");
    A(setFaceNeighbors, "?SetFaceNeighborArray@" GC "QAAXAAV?$AutoReferenceArray@UFaceNeighbors@GeometryChunk@Jet@@@2@@Z");
    A(dupFaceNormals, "?DuplicateFaceNormalArray@" GC "QAAXXZ");
    A(dupFaceNeighbors, "?DuplicateFaceNeighborArray@" GC "QAAXXZ");
    A(resizeFaceNormals, "?ResizeFaceNormalArray@" GC "QAAXI@Z");
    A(resizeFaceNeighbors, "?ResizeFaceNeighborArray@" GC "QAAXI@Z");
    A(vertexIndex3, "?GetVertexIndex@" GC "QBA_NHHAAI@Z");
    A(vertexIndex2, "?GetVertexIndex@" GC "QBAIHH@Z");
    A(triangle, "?GetTriangleIndexes@" GC "QBAXHAAK00@Z");
    A(addProp, "?AddChunkProperty@" GC "QAA_NABVPString@2@@Z");
    A(setProp, "?SetChunkProperty@" GC "QAA_NABVPString@2@M@Z");
    A(getProp, "?GetChunkProperty@" GC "QBA_NABVPString@2@AAM@Z");
    A(removeProp, "?RemoveChunkProperty@" GC "QAA_NABVPString@2@@Z");
    A(propExists, "?ChunkPropertyExists@" GC "QBA_NABVPString@2@@Z");
    A(addInfluence, "?AddInfluenceMatrix@" GC "QAAHH@Z");
    A(assignInfluence, "?AssignInfluenceMatrix@" GC "QAA_NHPBVMatrix4@2@@Z");
    A(resetMatrices, "?ResetMatrixArray@" GC "QAAXXZ");
    A(influenceArray, "?GetInfluenceArray@" GC "QBAABV?$Array@PAUMatrixMap@VertexProgram@Jet@@$0IA@@2@XZ");
    A(setVertexType, "?SetVertexType@" GC "QBAXPAVDisplay@2@W4VertexTypeID@2@H@Z");
    A(setOpacity, "?SetOpacity@" GC "QAAXPAVDisplay@2@M@Z");
    A(renderColor, "?RenderChunkColor@" GC "QBAXPAVDisplay@2@@Z");
    A(renderOutlines, "?RenderOutlines@" GC "QBAXPAVDisplay@2@@Z");
    A(renderVertexNormals, "?RenderVertexNormals@" GC "QBAXPAVDisplay@2@@Z");
    A(renderTriangleNormals, "?RenderTriangleNormals@" GC "QBAXPAVDisplay@2@@Z");
    A(renderAxes, "?RenderAxes@" GC "QBAXPAVDisplay@2@@Z");
    A(registerName, "?RegisterName@" GC "SA_NABVPString@2@P6APAV12@XZ@Z");
    A(createRegistered, "?CreateRegisteredInstance@" GC "SAPAV12@ABVPString@2@@Z");
    A(releaseRegistrations, "?ReleaseRegistrationData@" GC "SAXXZ");
    A(udword, "?GetUDwordArray@" GC "UAA?AV?$VertexItemPtr@KV?$VertexItemPtr@$$CBKVVIPConstHelper@Jet@@@Jet@@@2@W4VertexItemType@2@H@Z");
    A(udwordConst, "?GetUDwordArray@" GC "UBA?AV?$VertexItemPtr@$$CBKVVIPConstHelper@Jet@@@2@W4VertexItemType@2@H@Z");
    A(dupUnshared, "?DuplicateUnsharedArrays@" GC "UAAXPAVDisplay@2@@Z");
    A(dupIndex, "?DuplicateIndexArray@" GC "UAAXPAVDisplay@2@@Z");
    A(resizeIndex, "?ResizeIndexArray@" GC "UAAXPAVDisplay@2@I@Z");
    A(resizeVertex, "?ResizeVertexArray@" GC "UAAXPAVDisplay@2@IHH@Z");
    A(append, "?AppendChunk@" GC "UAAXPAVDisplay@2@ABV12@@Z");
    A(setIndexDynamic, "?SetIndexDataDynamic@" GC "UAAXXZ");
    A(getIndexDynamic, "?GetIndexDataDynamic@" GC "UBA_NXZ");
    A(setIndexDirty, "?SetIndexDataDirty@" GC "UBAX_N@Z");
    A(getIndexDirty, "?GetIndexDataDirty@" GC "UBA_NXZ");
    A(updateLighting, "?UpdateLighting@" GC "UAAXPAVDisplay@2@PBVDisplayLight@2@PAVMaterial@2@ABVMatrix4@2@3@Z");
    A(resetToAmbient, "?ResetToAmbient@" GC "UAAXPAVDisplay@2@PBVMaterial@2@ABVColor@2@@Z");
    A(renderDebug, "?RenderDebug@" GC "UBAXPAVDisplay@2@@Z");
    A(loadType, "?GetLoadChunkDataType@" GC "UBA_NABV?$Array@V?$Array@UGeometryStreamType@Jet@@$00@Jet@@$01@2@AAIAA_N@Z");
    A(loadData, "?LoadChunkData@" GC "UAA_NABV?$Array@UGeometryStreamType@Jet@@$00@2@QAPAX@Z");
#undef A
#undef F
}

// the chunk registry (the original's static): emptied between sides, its bucket chunks freed outside the logs
static void ClearChunkRegistry()
{
    struct Table {
        void** buckets;
        unsigned count, size;
    }* t = static_cast<Table*>(Proc(mod[1], "?registrations@" GC "1V?$HashTable@IPAURegistrationData@" GC
                                                "URegistrationDataOp@23@$0N@@2@A"));
    for (unsigned k = 0; k < t->size; ++k) {
        void** head = static_cast<void**>(t->buckets[k]);
        if (!head) continue;
        for (void** c = static_cast<void**>(head[0]); c != head;) {
            void** n = static_cast<void**>(c[0]);
            RealDelete(c);
            c = n;
        }
        RealDelete(head);
        t->buckets[k] = nullptr;
    }
    t->count = 0;
}

// the test's stand-ins for what the stream chunks provide: per chunk a vertex type and vertex arrays
struct ChunkState {
    void* vtype;            // the side's VertexType (slot 0xa4)
    int buffers;            // slot 0x60 (slot 0x5c answers 1)
    std::vector<unsigned char> arrays[3];
    bool shared[3];
    int staticIndex;        // slot 0x94
    bool positions, normals, colors;  // slots 0xc0 / 0xc4 types 1 and 4, slot 0xdc type 2 (offsets 0, 12, 24)
    unsigned posStride, norStride, colStride;
};
static std::map<const void*, ChunkState> chunkState[2];
static void* chunkTestVt[2][73];

static ChunkState& St(const void* c) { return chunkState[cur][c]; }
static bool __cdecl CInitBuffers(void* self, void* d, bool b) { Log("  init buffers %s %s %d\n", Name(self).c_str(), Name(d).c_str(), b); return true; }
static void __cdecl CDupVertex(void* self, void* d, int i, int j) { Log("  dup vertex %s %s %d %d\n", Name(self).c_str(), Name(d).c_str(), i, j); }
static void __cdecl CResizeVertex(void* self, void* d, unsigned n, int i, int j)
{
    Log("  resize vertex %s %s %u %d %d\n", Name(self).c_str(), Name(d).c_str(), n, i, j);
    ChunkState& st = St(self);
    if (j >= 0 && j < 3) {
        const unsigned stride = static_cast<const Stream*>(static_cast<const Type*>(st.vtype)->streams)[j].stride;
        st.arrays[j].resize(static_cast<size_t>(n) * stride + 4, 0xab);
    }
}
static void* __cdecl CVertexArray(void* self, int i, int j)
{
    ChunkState& st = St(self);
    Log("  vertex array %s %d %d\n", Name(self).c_str(), i, j);
    return j >= 0 && j < 3 ? st.arrays[j].data() : nullptr;
}
static int __cdecl COne(void*) { return 1; }
static int __cdecl CBuffers(void* self) { return St(self).buffers; }
static bool __cdecl CShared(void* self, int i, int j) { return St(self).shared[j % 3]; }
static void __cdecl CVertexDirty(void* self, bool b, int i, int j) { Log("  vertex dirty %s %d %d %d\n", Name(self).c_str(), b, i, j); }
static int __cdecl CStaticIndex(void* self, int i) { Log("  static index %s %d\n", Name(self).c_str(), i); return St(self).staticIndex; }
static void __cdecl CSetStaticIndex(void* self, void* b, int i) { Log("  set static index %s %s %d\n", Name(self).c_str(), Name(b).c_str(), i); St(self).staticIndex = 0; }
static void __cdecl CSetVertexType(void* self, void* t, int k) { Log("  set vertex type %s %s %d\n", Name(self).c_str(), Name(t).c_str(), k); }
static void* __cdecl CGetVertexType(void* self, int i) { return St(self).vtype; }
static int __cdecl CItemStream(void* self, int type, int k) { Log("  item stream %s %d %d\n", Name(self).c_str(), type, k); return 7; }
static void* __cdecl CFloatArray(void* self, unsigned* out, int type, int k)
{
    ChunkState& st = St(self);
    Log("  float array %s %d %d\n", Name(self).c_str(), type, k);
    out[0] = type == 4 ? st.norStride : st.posStride;
    unsigned char* const b = st.arrays[0].data();
    out[1] = static_cast<unsigned>(reinterpret_cast<uintptr_t>(type == 1 && st.positions ? b : type == 4 && st.normals ? b + 12 : nullptr));
    return out;
}
static void* __cdecl CUDwordArray(void* self, unsigned* out, int type, int k)
{
    ChunkState& st = St(self);
    Log("  udword array %s %d %d\n", Name(self).c_str(), type, k);
    out[0] = st.colStride;
    out[1] = static_cast<unsigned>(reinterpret_cast<uintptr_t>(type == 2 && st.colors ? st.arrays[0].data() + 24 : nullptr));
    return out;
}
static void MakeChunkVtable(int s)
{
    void** v = chunkTestVt[s];
    for (int k = 0; k < 73; ++k) v[k] = apiC[s].vft[k];
    v[4] = reinterpret_cast<void*>(&CInitBuffers);
    v[7] = reinterpret_cast<void*>(&CDupVertex);
    v[9] = reinterpret_cast<void*>(&CResizeVertex);
    v[12] = v[13] = reinterpret_cast<void*>(&CVertexArray);
    v[23] = reinterpret_cast<void*>(&COne);
    v[24] = reinterpret_cast<void*>(&CBuffers);
    v[28] = reinterpret_cast<void*>(&CShared);
    v[31] = reinterpret_cast<void*>(&CVertexDirty);
    v[37] = reinterpret_cast<void*>(&CStaticIndex);
    v[38] = reinterpret_cast<void*>(&CSetStaticIndex);
    v[40] = reinterpret_cast<void*>(&CSetVertexType);
    v[41] = reinterpret_cast<void*>(&CGetVertexType);
    v[45] = reinterpret_cast<void*>(&CItemStream);
    v[48] = v[49] = reinterpret_cast<void*>(&CFloatArray);
    v[55] = reinterpret_cast<void*>(&CUDwordArray);
}

// the stand-in display: slots 0xa8 / 0xac / 0x1f0 (debug state), 0xe0 (lines), 0x10c (vertex types)
static void* chunkDisplayVt[0x80];
static unsigned char chunkDisplay[0x100], chunkState54[0x100];
static bool debugSaved;
// how many vertices of each coming line draw are written (the outlines leave the rest of their buffer unwritten)
static std::vector<unsigned> lineDefined;
static bool __cdecl DGet(void* d) { Log("  display a8\n"); return debugSaved; }
static void __cdecl DPut(void* d, int b) { Log("  display ac %d\n", b & 0xff); }
static void __cdecl DState(void* d, int b) { Log("  display 1f0 %d\n", b & 0xff); }
static void __cdecl DLines(void* d, int a, int b, int n, const void* v, int count)
{
    unsigned shown = static_cast<unsigned>(count);
    if (!lineDefined.empty()) {
        if (lineDefined.front() < shown) shown = lineDefined.front();
        lineDefined.erase(lineDefined.begin());
    }
    if (shown > 0x40) shown = 0x40;
    Log("  lines %d %d %d %d: %s\n", a, b, n, count, Hex(v, shown * 16).c_str());
}
static void* __cdecl DType(void* d, int id) { Log("  display type %d\n", id); return reinterpret_cast<void*>(static_cast<uintptr_t>(0x5000 + id)); }

// the stand-in light (slot 1 lights the vertices) and material
static void* lightTestVt[4];
static const unsigned char* lightBase;  // the lit chunk's stream 0 (pointers are logged as offsets into it)
static long Off(const void* p) { return p ? static_cast<long>(static_cast<const unsigned char*>(p) - lightBase) : -1; }
static unsigned char lightObj[0x10], materialObj[0x80];
static void __cdecl LLight(const void* l, void* d, unsigned n, unsigned ps, const void* p, unsigned ns, const void* nn,
                           unsigned cs, const void* c, const void* a, const void* b, void* m)
{
    Log("  light %s %u %u %ld %u %ld %u %ld %s %s %s\n", Name(d).c_str(), n, ps, Off(p), ns, Off(nn), cs, Off(c),
        Name(a).c_str(), Name(b).c_str(), Name(m).c_str());
}

struct Chunk {
    void* vft;
    int refs;
    void* props;
    unsigned infCount, infCap;
    void** infData;
    unsigned flags, prims, indices, vertices, maxVertices;
    int* index;
    int* neighbors;
    int* normals;
    unsigned numNormals;
    float opacity, surface;
    unsigned user;
    void* refUser;
};
static_assert(sizeof(Chunk) == 0x4c, "chunk");

static std::string BlockText(const int* b, unsigned each)
{
    if (!b) return "null";
    char t[64];
    std::snprintf(t, sizeof t, "%s refs %d count %d ", Name(b).c_str(), b[0], b[1]);
    const unsigned n = static_cast<unsigned>(b[1]) * each;
    return t + Hex(b + 4, n > 0x60 ? 0x60 : n);
}
static void DumpChunk(const char* tag, const void* p)
{
    const Chunk* c = static_cast<const Chunk*>(p);
    std::string props;
    if (c->props) {
        void** h = static_cast<void**>(c->props);
        void** k = h;
        do {
            const unsigned n = reinterpret_cast<unsigned*>(k)[2];
            props += " [" + Name(k) + " " + std::to_string(n) + "]";
            for (unsigned i = 0; i < n; ++i) {
                const unsigned* e = reinterpret_cast<unsigned*>(k) + 3 + i * 2;
                props += " " + NodeText(reinterpret_cast<void*>(static_cast<uintptr_t>(e[0]))) + "=" + Hex(e + 1, 4);
            }
            k = static_cast<void**>(k[0]);
        } while (k != h);
    }
    std::string inf;
    for (unsigned k = 0; k < c->infCount && c->infData; ++k) {
        const unsigned* m = static_cast<const unsigned*>(c->infData[k]);
        inf += " " + Name(m) + (m ? "{" + Name(reinterpret_cast<void*>(static_cast<uintptr_t>(m[0]))) + "," + std::to_string(m[1]) + "}" : "");
    }
    Log("  %s %s: vft %s refs %d flags %x prims %u indices %u vertices %u max %u normals %u opacity %s surface %s user %x ref %s\n"
        "    props%s\n    influences %u/%u %s:%s\n    index %s\n    neighbors %s\n    normals %s\n",
        tag, Name(p).c_str(), c->vft == apiC[cur].vft ? "own" : c->vft == chunkTestVt[cur] ? "test" : "other", c->refs,
        c->flags, c->prims, c->indices, c->vertices, c->maxVertices, c->numNormals, Bits(c->opacity).c_str(),
        Bits(c->surface).c_str(), c->user, Name(c->refUser).c_str(), props.c_str(), c->infCount, c->infCap,
        Name(c->infData).c_str(), inf.c_str(), BlockText(c->index, 2).c_str(), BlockText(c->neighbors, 12).c_str(),
        BlockText(c->normals, 12).c_str());
}

// a float's bits (a float returned by value would pass through the x87 stack and lose its signalling NaNs)
static unsigned RandFloatBits(std::mt19937& r)
{
    float f;
    switch (r() % 9) {
    case 0: f = 0.f; break;
    case 1: return 0x7fc00001u + (r() % 0x3fffff);  // a quiet NaN with its own payload
    case 2: return 0x7f800001u + (r() % 0x3fffff);  // a signalling NaN (the x87 copies quiet it)
    case 3: f = static_cast<float>(static_cast<int>(r() % 200001) - 100000) / 7.f; break;  // inexact sums
    default: f = static_cast<float>(static_cast<int>(r() % 2001) - 1000) / 64.f; break;
    }
    unsigned u;
    std::memcpy(&u, &f, 4);
    return u;
}
#define RANDF(var)                                  \
    float var;                                      \
    {                                               \
        const unsigned bits_ = RandFloatBits(r);    \
        std::memcpy(&var, &bits_, 4);               \
    }

static int creatorCalls;
static void* CreatorA() { Log("  creator A\n"); ++creatorCalls; return reinterpret_cast<void*>(0xa0a0); }
static void* CreatorB() { Log("  creator B\n"); return reinterpret_cast<void*>(0xb0b0); }

static void RunChunks(int s, unsigned seed)
{
    std::mt19937 r(seed);
    const ApiC& a = apiC[s];
    const Api& v = api[s];
    chunkState[s].clear();
    const std::vector<int> refsBefore = NameRefs(v);
    const bool released = r() % 2 != 0;
    std::vector<void*> chunks;
    int made = 0, blocks = 0;
    // a vertex type: positions, normals, a colour in stream 0; texture coordinates in stream 1
    void* vt = TestNew(0x24, "VT%d", 0);
    v.ctor(vt, nullptr, 3);
    const int streams = 1 + static_cast<int>(r() % 2);
    v.createStreams(vt, streams);
    v.createItems(vt, 0, 3);
    v.addItem(vt, 0, 1, -1, 0, 1, &nodes[0], -1, -1);
    v.addItem(vt, 0, 4, -1, 0, 1, &nodes[1], -1, -1);
    v.addItem(vt, 0, 2, -1, 0, 1, &nodes[4], -1, -1);
    if (streams > 1) {
        v.createItems(vt, 1, 1);
        v.addItem(vt, 1, 15, -1, 0, 1, &nodes[2], -1, -1);
    }
    DumpType("vertex type", vt);
    const unsigned stride0 = static_cast<const Type*>(vt)->streams[0].stride;
    auto newBlock = [&](unsigned n, unsigned each) {
        int* b = static_cast<int*>(TestNew(n * each + 0x10, "B%d", blocks++));
        b[0] = 1;
        b[1] = static_cast<int>(n);
        for (unsigned k = 0; k < n * each; ++k) reinterpret_cast<unsigned char*>(b + 4)[k] = static_cast<unsigned char>(r());
        return b;
    };
    auto release = [&](int* b) {  // the test's reference
        if (b && --b[0] == 0) HookDelete(b);
    };
    auto setup = [&](void* c, unsigned verts) {
        ChunkState& st = chunkState[s][c];
        st.vtype = vt;
        st.buffers = streams;
        st.staticIndex = static_cast<int>(r() % 2);
        for (int j = 0; j < 3; ++j) st.shared[j] = r() % 2;
        const unsigned cap = verts + 8;
        for (int j = 0; j < streams; ++j) {
            const unsigned sd = static_cast<const Type*>(vt)->streams[j].stride;
            st.arrays[j].resize(cap * sd + 4);
            for (auto& b : st.arrays[j]) b = static_cast<unsigned char>(r());
        }
        st.posStride = stride0;
        st.norStride = stride0;
        st.colStride = stride0;
        unsigned char* base = st.arrays[0].data();
        for (unsigned k = 0; k < cap; ++k)
            for (int f = 0; f < 6; ++f) {
                RANDF(x)
                std::memcpy(base + k * stride0 + f * 4, &x, 4);
            }
        st.positions = r() % 8 != 0;
        st.normals = r() % 8 != 0;
        st.colors = r() % 8 != 0;
    };
    auto make = [&]() {
        void* c = TestNew(0x4c, "C%d", made++);
        const unsigned prims = r() % 6, verts = 3 + r() % 8;
        static const unsigned types[] = {0, 1, 2, 4, 8, 0x10, 3, 4, 4, 4, 4, 4};
        const unsigned flags = types[r() % 12] | (r() % 2 ? 0x20 : 0) | (r() % 4 ? 0 : 0x1000);
        if (r() % 3) {
            Log("op new chunk %s %u %u\n", Name(c).c_str(), prims, verts);
            a.ctor5(c, nullptr, verts, prims, r() % 3 ? prims * 3 + (r() % 2 ? 3 : 0) : 0, r() % 2 ? prims : 0, flags);
        } else {
            Log("op new chunk %s\n", Name(c).c_str());
            a.ctor0(c, nullptr);
            a.setU[0](c, verts);
            a.setU[3](c, prims);
            a.setFlags(c, flags);
        }
        DumpChunk("made", c);
        *static_cast<void**>(c) = chunkTestVt[s];
        setup(c, static_cast<const Chunk*>(c)->vertices);
        // indices within the vertex count
        Chunk* ch = static_cast<Chunk*>(c);
        if (ch->index)
            for (int k = 0; k < ch->index[1]; ++k)
                reinterpret_cast<unsigned short*>(ch->index + 4)[k] = static_cast<unsigned short>(r() % ch->vertices);
        chunks.push_back(c);
        return c;
    };
    for (int k = 0, n = 1 + r() % 3; k < n; ++k) make();
    // the display's debug state
    *reinterpret_cast<void**>(chunkDisplay + 0x54) = chunkState54;
    for (int step = 0, steps = 10 + r() % 30; step < steps; ++step) {
        void* c = chunks[r() % chunks.size()];
        void* c2 = chunks[r() % chunks.size()];
        Chunk* ch = static_cast<Chunk*>(c);
        void** nm = r() % 8 ? &nodes[r() % 3] : &nodes[r() % (kNames + 1)];
        static void* had;
        if (ch->props && r() % 2) {
            const unsigned* first = static_cast<const unsigned*>(ch->props);
            had = reinterpret_cast<void*>(static_cast<uintptr_t>(first[3 + 2 * (r() % first[2])]));
            nm = &had;
        }
        switch (r() % 30) {
        case 0:
            make();
            break;
        case 1: {
            const int p = static_cast<int>(r() % 8) - 1, vv = static_cast<int>(r() % 4) - (r() % 8 ? 0 : 3);
            unsigned out = 0xeeee;
            // (without an index array both sides read address 0x10)
            if (!ch->index || ch->index[1] < static_cast<int>(ch->prims) * 3 + 3) break;
            const bool ok = a.vertexIndex3(c, p, vv, &out);
            Log("op vertex index %s %d %d: %d %u / %u\n", Name(c).c_str(), p, vv, ok, out,
                p >= 0 && static_cast<unsigned>(p) < ch->prims && vv >= 0 && vv < 3 ? a.vertexIndex2(c, p, vv) : 0u);
            std::string all;
            for (unsigned q = 0; q < ch->prims; ++q)
                for (int w = 0; w < 3; ++w) {
                    unsigned o2 = 0xeeee;
                    const bool k2 = a.vertexIndex3(c, static_cast<int>(q), w, &o2);
                    all += " " + std::to_string(k2) + ":" + std::to_string(o2) + "/" +
                           std::to_string(a.vertexIndex2(c, static_cast<int>(q), w));
                }
            Log("  all%s\n", all.c_str());
            break;
        }
        case 2: {
            if (!ch->prims) break;
            const int p = static_cast<int>(r() % ch->prims);
            unsigned long x = 1, y = 2, z = 3;
            if (!ch->index || ch->index[1] < static_cast<int>(ch->prims) * 3 + 3) break;
            a.triangle(c, p, &x, &y, &z);
            Log("op triangle %s %d: %lu %lu %lu\n", Name(c).c_str(), p, x, y, z);
            for (unsigned q = 0; q < ch->prims; ++q) {
                a.triangle(c, static_cast<int>(q), &x, &y, &z);
                Log("  %u: %lu %lu %lu\n", q, x, y, z);
            }
            break;
        }
        case 3:
            Log("op add prop %s %s: %d\n", Name(c).c_str(), NodeText(*nm).c_str(), a.addProp(c, nm));
            {
                std::vector<int> added;
                for (int k = r() % 8; k > 0; --k) {
                    const int id = static_cast<int>(r() % kNames);
                    void** n2 = &nodes[id];
                    RANDF(x)
                    Log("  add %s: %d %d\n", NodeText(*n2).c_str(), a.addProp(c, n2), a.setProp(c, n2, x));
                    added.push_back(id);
                }
                if (r() % 2 && !added.empty())
                    for (int k = r() % 7; k > 0; --k) {
                        void** n2 = &nodes[r() % 4 ? added[r() % added.size()] : static_cast<int>(r() % kNames)];
                    float x = 3.f;
                    const bool g = a.getProp(c, n2, &x);
                    Log("  get %s: %d %s; remove %d\n", NodeText(*n2).c_str(), g, Bits(x).c_str(), a.removeProp(c, n2));
                    DumpChunk("removed", c);
                    }
            }
            DumpChunk("props", c);
            break;
        case 4: {
            RANDF(x)
            Log("op set prop %s %s %s: %d\n", Name(c).c_str(), NodeText(*nm).c_str(), Bits(x).c_str(), a.setProp(c, nm, x));
            break;
        }
        case 5: {
            float x = 7.f;
            const bool ok = a.getProp(c, nm, &x);
            Log("op get prop %s %s: %d %s exists %d\n", Name(c).c_str(), NodeText(*nm).c_str(), ok, Bits(x).c_str(),
                a.propExists(c, nm));
            break;
        }
        case 6:
            Log("op remove prop %s %s: %d\n", Name(c).c_str(), NodeText(*nm).c_str(), a.removeProp(c, nm));
            DumpChunk("props", c);
            break;
        case 7: {
            const int i = static_cast<int>(r() % 6);
            Log("op add influence %s %d: %d\n", Name(c).c_str(), i, a.addInfluence(c, i));
            const void* m = r() % 2 ? static_cast<const void*>(materialObj) : nullptr;
            Log("  assign %d\n", a.assignInfluence(c, static_cast<int>(r() % 6), m));
            Log("  array %s\n", Name(a.influenceArray(c)).c_str());
            if (r() % 6 == 0) a.resetMatrices(c);
            DumpChunk("influences", c);
            break;
        }
        case 8: {
            int* b = newBlock(r() % 12, 2);
            for (int k = 0; k < b[1]; ++k) reinterpret_cast<unsigned short*>(b + 4)[k] = static_cast<unsigned short>(r() % ch->vertices);
            ++b[0];  // the by-value copy
            Log("op set index array %s %s\n", Name(c).c_str(), Name(b).c_str());
            a.setIndexArray(c, r() % 6 ? b : (--b[0], nullptr));
            release(b);
            DumpChunk("index", c);
            break;
        }
        case 9: {
            int* b = r() % 6 ? newBlock(r() % 6, 12) : nullptr;
            if (b) ++b[0];
            Log("op set face normals %s %s\n", Name(c).c_str(), Name(b).c_str());
            a.setFaceNormals(c, b);
            release(b);
            DumpChunk("normals", c);
            break;
        }
        case 10: {
            void* b = r() % 6 ? newBlock(r() % 6, 12) : nullptr;
            Log("op set face neighbors %s %s\n", Name(c).c_str(), Name(b).c_str());
            a.setFaceNeighbors(c, &b);
            release(static_cast<int*>(b));
            DumpChunk("neighbors", c);
            break;
        }
        case 11:
            Log("op duplicate %s\n", Name(c).c_str());
            a.dupIndex(c, chunkDisplay);
            if (ch->numNormals <= (ch->normals ? static_cast<unsigned>(ch->normals[1]) : 0u)) a.dupFaceNormals(c);
            if (ch->neighbors && ch->prims <= static_cast<unsigned>(ch->neighbors[1])) a.dupFaceNeighbors(c);
            a.dupUnshared(c, chunkDisplay);
            DumpChunk("duplicated", c);
            break;
        case 12: {
            const unsigned n = r() % 14;
            Log("op resize %s %u\n", Name(c).c_str(), n);
            switch (r() % 3) {
            case 0: a.resizeIndex(c, chunkDisplay, n); break;
            case 1:
                if (ch->numNormals <= (ch->normals ? static_cast<unsigned>(ch->normals[1]) : 0u)) a.resizeFaceNormals(c, n);
                break;
            default: a.resizeFaceNeighbors(c, n); break;
            }
            a.resizeVertex(c, chunkDisplay, n, 0, 0);
            DumpChunk("resized", c);
            break;
        }
        case 13: {
            if (c == c2) break;
            void* to = TestNew(0x4c, "C%d", made++);
            a.ctor0(to, nullptr);
            *static_cast<void**>(to) = chunkTestVt[s];
            setup(to, 4);
            if (r() % 2) a.addInfluence(to, 9);
            Log("op init instance %s -> %s\n", Name(c).c_str(), Name(to).c_str());
            a.initInstance(c, to);
            DumpChunk("instance", to);
            chunks.push_back(to);
            Log("  assign %s\n", Name(a.assign(to, nullptr, c)).c_str());
            break;
        }
        case 14: {
            Log("op flags %s\n", Name(c).c_str());
            a.setIndexDynamic(c);
            a.setIndexDirty(c, r() % 2);
            Log("  %d %d %d flags %lx\n", a.getIndexDynamic(c), a.getIndexDirty(c), a.unshared(c), a.getFlags(c));
            a.setConstFlags(c, a.getFlags(c) ^ (r() % 2 ? 0x1000 : 0));
            std::string g;
            for (int k = 0; k < 8; ++k) g += " " + std::to_string(a.getU[k](c));
            Log("  counts%s opacity %s surface %s ref %s arrays %s %s %s\n", g.c_str(), Bits(a.getOpacity(c)).c_str(),
                Bits(a.getSurface(c)).c_str(), Name(a.getRefUser(c)).c_str(), Name(a.getIndexArray(c)).c_str(),
                Name(a.getFaceNormals(c)).c_str(), Name(a.getFaceNeighbors(c)).c_str());
            break;
        }
        case 15: {
            RANDF(x)
            a.setSurface(c, x);
            a.setU[5](c, r());
            a.setU[1](c, r() % 20);
            if (r() % 2) a.setU[2](c, r() % 20 < ch->indices ? ch->indices : 0);
            if (r() % 3 == 0) a.setU[4](c, ch->normals ? static_cast<unsigned>(ch->normals[1]) : 0);
            Log("op set counts %s\n", Name(c).c_str());
            DumpChunk("counts", c);
            break;
        }
        case 16: {
            void* u = r() % 4 ? TestNew(8, "U%d", blocks++) : nullptr;
            if (u) api[s].sbCtor(u, nullptr);
            Log("op ref user %s %s\n", Name(c).c_str(), Name(u).c_str());
            a.setRefUser(c, u);
            if (u) ReleaseRef(u);
            DumpChunk("ref user", c);
            break;
        }
        case 17: {
            RANDF(o)
            Log("op opacity %s %s\n", Name(c).c_str(), Bits(o).c_str());
            a.setOpacity(c, chunkDisplay, o);
            Log("  colors %s\n", Hex(chunkState[s][c].arrays[0].data(), 0x60).c_str());
            break;
        }
        case 18: {
            Log("op ambient %s\n", Name(c).c_str());
            float amb[4];
            for (float& f : amb) {
                const unsigned u = RandFloatBits(r);
                std::memcpy(&f, &u, 4);
            }
            for (int k = 0; k < 0x80 / 4; ++k) {
                const float f = static_cast<float>(static_cast<int>(r() % 100)) / 50.f;
                std::memcpy(materialObj + k * 4, &f, 4);
            }
            a.resetToAmbient(c, chunkDisplay, materialObj, amb);
            Log("  colors %s\n", Hex(chunkState[s][c].arrays[0].data(), 0x60).c_str());
            break;
        }
        case 19: {
            Log("op lighting %s\n", Name(c).c_str());
            static unsigned char m1[64], m2[64];
            fixedNames[m1] = "M1";
            fixedNames[m2] = "M2";
            lightBase = chunkState[s][c].arrays[0].data();
            a.updateLighting(c, chunkDisplay, lightObj, materialObj, m1, m2);
            break;
        }
        case 20:
        case 21: {
            chunkDisplay[0x71] = static_cast<unsigned char>(r() % 3 ? r() % 16 : 0);
            if (r() % 2) chunkDisplay[0x71] |= 4;  // the triangle normals more often
            chunkDisplay[0x78] = static_cast<unsigned char>(r() % 4 == 0);
            chunkDisplay[0x79] = static_cast<unsigned char>(r() % 4 == 0);
            chunkState54[0xa1] = static_cast<unsigned char>(r());
            RANDF(len)
            if (r() % 3 == 0) {
                const unsigned u = 0x7fc00001u + r() % 0x3fffff;
                std::memcpy(&len, &u, 4);
            }
            std::memcpy(chunkDisplay + 0x74, &len, 4);
            debugSaved = r() % 2;
            // the outlines read three indices per primitive, the triangle normals one face normal per primitive
            if (ch->prims && (!ch->index || static_cast<unsigned>(ch->index[1]) < ch->prims * 3)) chunkDisplay[0x71] &= ~5;
            else if (ch->prims) {  // every index inside the vertex array (both sides read whatever it points at)
                const size_t have = chunkState[s][c].arrays[0].size() / stride0;
                for (unsigned k = 0; k < ch->prims * 3; ++k)
                    if (reinterpret_cast<const unsigned short*>(ch->index + 4)[k] >= have) chunkDisplay[0x71] &= ~5;
            }
            if (ch->numNormals > ch->prims || (ch->normals && static_cast<unsigned>(ch->normals[1]) < ch->numNormals))
                chunkDisplay[0x71] &= ~4;
            Log("op debug %s %02x %d %d state %02x\n", Name(c).c_str(), chunkDisplay[0x71], chunkDisplay[0x78],
                chunkDisplay[0x79], chunkState54[0xa1]);
            {
                const ChunkState& st = chunkState[s][c];
                const unsigned char f = chunkDisplay[0x71];
                lineDefined.clear();
                if (f & 0xf || chunkDisplay[0x78] || chunkDisplay[0x79]) {
                    if (f & 1 && ch->flags & 4 && ch->prims && st.positions) lineDefined.push_back((ch->prims - 1) * 2 + 6);
                    if (f & 2 && ch->vertices && st.positions && st.normals) lineDefined.push_back(ch->vertices * 2);
                    if (f & 4 && ch->flags & 4 && ch->numNormals && ch->normals && st.positions)
                        lineDefined.push_back(ch->numNormals * 2);
                    if (f & 8) lineDefined.push_back(12);
                }
            }
            a.renderDebug(c, chunkDisplay);
            Log("  state %02x\n", chunkState54[0xa1]);
            a.renderColor(c, chunkDisplay);
            if (r() % 3 == 0) a.renderAxes(c, chunkDisplay);
            break;
        }
        case 22:
            Log("op set vertex type %s\n", Name(c).c_str());
            a.setVertexType(c, chunkDisplay, static_cast<int>(r() % 40), static_cast<int>(r() % 3));
            break;
        case 23: {
            if (c == c2) break;
            // the other chunk's indices stay within its vertices; ours must hold the sum
            Chunk* o = static_cast<Chunk*>(c2);
            if (!o->index || o->index[1] < static_cast<int>(o->indices)) break;
            if (o->normals ? static_cast<unsigned>(o->normals[1]) < o->numNormals : o->numNormals != 0) break;
            if (ch->normals && static_cast<unsigned>(ch->normals[1]) < ch->numNormals) break;
            if (!ch->normals && ch->numNormals) break;
            ChunkState& so = chunkState[s][c2];
            for (int j = 0; j < streams; ++j)
                if (so.arrays[j].size() < static_cast<size_t>(o->vertices) * static_cast<const Type*>(vt)->streams[j].stride) goto skip;
            Log("op append %s %s\n", Name(c).c_str(), Name(c2).c_str());
            a.append(c, chunkDisplay, c2);
            DumpChunk("appended", c);
            Log("  vertices %s\n", Hex(chunkState[s][c].arrays[0].data(), 0x80).c_str());
        skip:
            break;
        }
        case 24: {
            Log("op udword %s\n", Name(c).c_str());
            unsigned out[2] = {9, 9}, out2[2] = {9, 9};
            a.udword(c, out, 2, 0);
            a.udwordConst(c, out2, 2, 0);
            Log("  %u %u %u %u init %d\n", out[0], out[1], out2[0], out2[1], a.initBuffers(c, chunkDisplay, true));
            break;
        }
        case 25: {
            // formats: arrays of streams of items built from the vertex type, some changed
            struct Item { void* name; unsigned dataType, count, offset; };
            struct StreamT { unsigned stride, count, cap; Item* items; };
            struct Fmt { unsigned count, cap; StreamT* data; };
            const Type* t = static_cast<const Type*>(vt);
            const unsigned nf = r() % 4;
            std::vector<Fmt> fmts(nf);
            std::vector<std::vector<StreamT>> sts(nf);
            std::vector<std::vector<std::vector<Item>>> its(nf);
            for (unsigned f = 0; f < nf; ++f) {
                const unsigned ns = t->streamCount + (r() % 4 == 0 ? 1 : 0) - (r() % 4 == 0 && t->streamCount > 1 ? 1 : 0);
                sts[f].resize(ns);
                its[f].resize(ns);
                for (unsigned j = 0; j < ns; ++j) {
                    const Stream* ts = j < t->streamCount ? &t->streams[j] : nullptr;
                    sts[f][j].stride = ts ? ts->stride + (r() % 5 == 0 ? 4 : 0) : 8;
                    const unsigned ni = ts ? ts->count : 1;
                    for (unsigned k = 0; k < ni; ++k) {
                        Item it{ts ? ts->items[k].name : nodes[3], ts ? static_cast<unsigned>(ts->items[k].dataType) : 7,
                                ts ? static_cast<unsigned>(ts->items[k].count) : 2,
                                ts ? static_cast<unsigned>(ts->items[k].offset) : 0};
                        switch (r() % 8) {
                        case 0: it.dataType ^= 1; break;
                        case 1: it.offset += 4; break;
                        case 2: it.name = nodes[r() % kNames]; break;
                        default: break;
                        }
                        if (r() % 6) its[f][j].push_back(it);
                    }
                    sts[f][j].count = static_cast<unsigned>(its[f][j].size());
                    sts[f][j].cap = sts[f][j].count;
                    sts[f][j].items = its[f][j].data();
                }
                fmts[f] = {ns, ns, sts[f].data()};
            }
            struct FmtList { unsigned count, cap; Fmt* data; } all{nf, nf, fmts.data()};
            unsigned index = 77;
            bool exact = true;
            const bool ok = a.loadType(c, &all, &index, &exact);
            Log("op load type %s %u: %d %u %d\n", Name(c).c_str(), nf, ok, index, exact);
            if (nf && ok) {
                const Fmt& f = fmts[index];
                std::vector<std::vector<unsigned char>> data(f.count);
                std::vector<void*> ptrs(f.count);
                for (unsigned j = 0; j < f.count; ++j) {
                    data[j].resize(static_cast<size_t>(f.data[j].stride) * (ch->vertices + 1) + 64);
                    for (auto& b : data[j]) b = static_cast<unsigned char>(r());
                    ptrs[j] = data[j].data();
                }
                ChunkState& st = chunkState[s][c];
                bool fits = true;
                for (int j = 0; j < streams; ++j)
                    if (st.arrays[j].size() < static_cast<size_t>(ch->vertices) * t->streams[j].stride) fits = false;
                if (fits) {
                    Log("  load %d\n", a.loadData(c, &f, ptrs.data()));
                    Log("  vertices %s\n", Hex(st.arrays[0].data(), 0x80).c_str());
                }
            }
            break;
        }
        case 26: {
            void** n2 = &nodes[r() % 4];
            Log("op register %s: %d\n", NodeText(*n2).c_str(),
                a.registerName(n2, r() % 2 ? &CreatorA : r() % 2 ? &CreatorB : nullptr));
            break;
        }
        case 27: {
            void** n2 = &nodes[r() % 4];
            const void* got = a.createRegistered(n2);
            Log("op create %s: %p\n", NodeText(*n2).c_str(), got);
            break;
        }
        default:
            DumpChunk("look", c);
            break;
        }
    }
    if (released) {
        Log("op release registrations\n");
        a.releaseRegistrations();
    }
    for (void* c : chunks) {
        Log("op release %s\n", Name(c).c_str());
        DumpChunk("last", c);
        ReleaseRef(c);
    }
    ReleaseRef(vt);
    // every name reference given back (registry entries kept only when not released)
    if (released) {
        const std::vector<int> refsAfter = NameRefs(v);
        std::string refs;
        for (size_t k = 0; k < refsAfter.size(); ++k) refs += " " + std::to_string(refsAfter[k] - refsBefore[k]);
        Log("op name refs%s\n", refs.c_str());
    }
}

// ---- part "interp" (U5b2): InterpolateChunkData, InterpolateAllChunkData, CopyChunkData, GetAllChunkWeights
//
// Chunks get a test vftable: slot 41 hands out a made-up vertex type (one stream of items with a count and a data
// type), slot 44 answers whether the chunk has an item, slots 46..55 hand out one array per item type, slot 31
// logs. The interpolation slots themselves (61..70) stay the side's own, so their calls into each other go through
// the real code. Every array of the written chunk is dumped after each call.

struct InterpArray {
    int type;
    unsigned stride;
    bool present;  // slot 44 answers 0 (else -1)
    std::vector<unsigned char> data;
};
struct InterpState {
    Type* vtype;
    std::vector<InterpArray> arrays;
};
static std::map<const void*, InterpState> interpState[2];
static void* interpTestVt[2][73];

static InterpArray* FindArray(const void* c, int type)
{
    for (auto& a : interpState[cur][c].arrays)
        if (a.type == type) return &a;
    return nullptr;
}
static void* __cdecl IVertexType(void* self, int stream)
{
    Log("  vertex type %s %d\n", Name(self).c_str(), stream);
    return interpState[cur][self].vtype;
}
static int __cdecl IItemOffset(void* self, int type, int k)
{
    const InterpArray* a = FindArray(self, type);
    Log("  item offset %s %d %d\n", Name(self).c_str(), type, k);
    return a && a->present ? 0 : -1;
}
template <int Slot>
static void* __cdecl IArray(void* self, unsigned* out, int type, int k)
{
    const InterpArray* a = FindArray(self, type);
    Log("  array %d %s %d %d\n", Slot, Name(self).c_str(), type, k);
    out[0] = a ? a->stride : 0;
    out[1] = a ? static_cast<unsigned>(reinterpret_cast<uintptr_t>(a->data.data())) : 0;
    return out;
}
static void __cdecl IDirty(void* self, int b, int i, int j) { Log("  vertex dirty %s %d %d %d\n", Name(self).c_str(), b, i, j); }
static void MakeInterpVtable(int s)
{
    void** v = interpTestVt[s];
    for (int k = 0; k < 73; ++k) v[k] = apiC[s].vft[k];
    v[31] = reinterpret_cast<void*>(&IDirty);
    v[41] = reinterpret_cast<void*>(&IVertexType);
    v[44] = reinterpret_cast<void*>(&IItemOffset);
    v[46] = reinterpret_cast<void*>(&IArray<46>);
    v[47] = reinterpret_cast<void*>(&IArray<47>);
    v[48] = reinterpret_cast<void*>(&IArray<48>);
    v[49] = reinterpret_cast<void*>(&IArray<49>);
    v[50] = reinterpret_cast<void*>(&IArray<50>);
    v[51] = reinterpret_cast<void*>(&IArray<51>);
    v[52] = reinterpret_cast<void*>(&IArray<52>);
    v[53] = reinterpret_cast<void*>(&IArray<53>);
    v[54] = reinterpret_cast<void*>(&IArray<54>);
    v[55] = reinterpret_cast<void*>(&IArray<55>);
}

struct ItemSpec {
    int type, count, dataType;
    bool inType, present;
};

static void RunInterp(int s, unsigned seed)
{
    std::mt19937 r(seed);
    const ApiC& a = apiC[s];
    interpState[s].clear();
    using F61 = void(__cdecl*)(void*, const void*, unsigned, unsigned, unsigned, const float*, const void*, int);
    using F62 = void(__cdecl*)(void*, const void*, unsigned, unsigned, unsigned, const float*, unsigned, unsigned, const void*, int);
    using F63 = void(__cdecl*)(void*, const void*, unsigned, unsigned, unsigned, const float*, const void*, int);
    using F64 = void(__cdecl*)(void*, const void*, unsigned, unsigned, unsigned, const void*, int);
    using F65 = void(__cdecl*)(void*, const void*, unsigned, unsigned, const float*, unsigned, unsigned, const void*, int);
    using F66 = void(__cdecl*)(void*, const void*, unsigned, unsigned, const float*, const void*, int);
    using F67 = void(__cdecl*)(void*, const void*, unsigned, unsigned, const void*, int);
    using F68 = void(__cdecl*)(void*, const void*, unsigned, const float*, unsigned, unsigned, const void*, int);
    using F69 = void(__cdecl*)(void*, const void*, unsigned, const float*, const void*, int);
    using F70 = void(__cdecl*)(void*, const void*, unsigned, unsigned, unsigned, const void*, int);
    using FTri = void(__cdecl*)(void*, const void*, unsigned, unsigned, unsigned, unsigned, const float*, const void*, int);
    using FWeights = void(__cdecl*)(const void*, const float*, const float*, const float*, float*);
    static FTri tri[2];
    static FWeights weights[2];
    if (!tri[s]) {
        tri[s] = As<FTri>(Proc(mod[s], "?InterpolateChunkData@GeometryChunk@Jet@@QAAXPBV12@IIIIABVVector3@2@ABV?$FastDynamicArray@W4VertexItemType@Jet@@$0N@U?$Less@W4VertexItemType@Jet@@W412@@2@@2@H@Z"));
        weights[s] = As<FWeights>(Proc(mod[s], "?GetAllChunkWeights@GeometryChunk@Jet@@QBAXABVVector3@2@00QAV32@@Z"));
    }

    // the item layout: positions (type 1, 3 floats) always, then a few others; the second chunk's differs a little
    static const int kTypes[] = {2, 3, 4, 5, 6, 7, 15};
    std::vector<ItemSpec> spec{{1, 3, 7, true, true}};
    for (int t : kTypes) {
        if (r() % 3 == 0) continue;
        static const int counts[] = {1, 1, 2, 3, 4, 4, 0, 5};
        const int count = counts[r() % 8];
        const int dataType = count == 1 && r() % 2 ? 8 : r() % 8 ? 7 : 3;
        spec.push_back({t, count, dataType, true, r() % 8 != 0});
    }
    const bool copyOps = r() % 4 == 0;  // copies may also meet an item the vertex type lacks
    const unsigned cap = 6 + r() % 10;
    std::vector<void*> chunks;
    auto make = [&](int k, const std::vector<ItemSpec>& items) {
        void* c = TestNew(0x4c, "C%d", k);
        a.ctor0(c, nullptr);
        static_cast<Chunk*>(c)->vertices = 1 + r() % cap;
        *static_cast<void**>(c) = interpTestVt[s];
        InterpState& st = interpState[s][c];
        st.vtype = new Type{};
        st.vtype->streamCount = 1;
        st.vtype->streams = new Stream{};
        Item* list = new Item[items.size()]{};
        unsigned n = 0;
        for (const ItemSpec& i : items) {
            if (i.inType) {
                list[n].type = i.type;
                list[n].count = i.count;
                list[n].dataType = i.dataType;
                ++n;
            }
            InterpArray ar;
            ar.type = i.type;
            ar.present = i.present;
            ar.stride = static_cast<unsigned>(i.count > 0 ? i.count : 1) * 4 + (r() % 3) * 4;
            ar.data.resize(ar.stride * (cap + 2));
            for (size_t b = 0; b + 4 <= ar.data.size(); b += 4) {
                unsigned u = i.dataType == 8 ? static_cast<unsigned>(r()) : RandFloatBits(r);
                if (i.dataType == 8 && r() % 4 == 0) u = static_cast<unsigned>(r() % 100);
                std::memcpy(&ar.data[b], &u, 4);
            }
            st.arrays.push_back(std::move(ar));
        }
        st.vtype->streams->count = n;
        st.vtype->streams->capacity = n;
        st.vtype->streams->items = list;
        chunks.push_back(c);
        return c;
    };
    void* c0 = make(0, spec);
    std::vector<ItemSpec> spec2 = spec;
    for (size_t k = 1; k < spec2.size(); ++k) {
        switch (r() % 10) {
        case 0: spec2[k].count = 1 + r() % 4; break;
        case 1: spec2[k].dataType = spec2[k].dataType == 7 ? 8 : 7; break;
        case 2: spec2[k].present = false; break;
        case 3: if (copyOps) spec2[k].inType = false; break;
        }
    }
    void* c1 = make(1, spec2);
    // a few positions of the second chunk on those of the first (the 0 / 1 shortcuts)
    {
        InterpArray* p0 = &interpState[s][c0].arrays[0];
        InterpArray* p1 = &interpState[s][c1].arrays[0];
        for (unsigned k = 0; k < cap; ++k)
            if (r() % 3 == 0) std::memcpy(&p1->data[k * p1->stride], &p0->data[(r() % cap) * p0->stride], 12);
        // and some plain ones, so lengths and weights are finite
        for (InterpArray* p : {p0, p1})
            for (unsigned k = 0; k < cap; ++k)
                if (r() % 2)
                    for (int j = 0; j < 3; ++j) {
                        const float f = static_cast<float>(static_cast<int>(r() % 2001) - 1000) / 16.f;
                        std::memcpy(&p->data[k * p->stride + j * 4], &f, 4);
                    }
    }
    auto dump = [&](void* c) {
        for (const InterpArray& ar : interpState[s][c].arrays)
            Log("  %s type %d: %s\n", Name(c).c_str(), ar.type, Hex(ar.data.data(), static_cast<unsigned>(ar.data.size())).c_str());
    };
    // the item type list: a FastDynamicArray<VertexItemType, 13> of one or two chunks (or none)
    struct TypeChunk {
        TypeChunk *next, *prev;
        unsigned count;
        int data[13];
    };
    TypeChunk tc[2]{};
    void* typeList = nullptr;
    auto makeTypes = [&]() {
        typeList = nullptr;
        const int n = r() % 8 == 0 ? 0 : 1 + static_cast<int>(r() % 16);
        if (!n) return;
        std::string text;
        for (int k = 0; k < n; ++k) {
            const int t = r() % 6 == 0 ? 9 : r() % 3 == 0 ? 1 : kTypes[r() % 7];
            TypeChunk& c = tc[k / 13];
            c.data[c.count++] = t;
            text += " " + std::to_string(t);
        }
        if (n > 13) {
            tc[0].next = tc[0].prev = &tc[1];
            tc[1].next = tc[1].prev = &tc[0];
        } else {
            tc[0].next = tc[0].prev = &tc[0];
        }
        typeList = &tc[0];
        Log("  types%s\n", text.c_str());
    };
    auto randBits = [&](unsigned n) {
        std::vector<float> v(n);
        for (auto& f : v) {
            const unsigned u = RandFloatBits(r);
            std::memcpy(&f, &u, 4);
        }
        return v;
    };
    const int steps = 3 + r() % 6;
    for (int step = 0; step < steps; ++step) {
        tc[0] = tc[1] = TypeChunk{};
        makeTypes();
        const void* types = &typeList;
        void* dst = r() % 2 ? c1 : c0;
        const void* src = r() % 6 == 0 ? dst : dst == c1 ? c0 : c1;
        const unsigned i0 = r() % cap, i1 = r() % cap, i2 = r() % 5 == 0 ? i0 : r() % cap;
        const unsigned dn = static_cast<const Chunk*>(dst)->vertices;
        const unsigned first = r() % cap;
        const unsigned count = r() % 5 == 0 ? 0 : r() % (cap - first + 1);
        const int stream = static_cast<int>(r() % 3);
        const int op = copyOps ? 11 : static_cast<int>(r() % 12);  // with items missing from a type only copies
        Log("op %d %s <- %s %u %u %u first %u count %u n %u stream %d\n", op, Name(dst).c_str(), Name(src).c_str(), i0,
            i1, i2, first, count, dn, stream);
        switch (op) {
        case 0: {
            std::vector<float> pos = randBits(3);
            const InterpArray& sp = interpState[s][const_cast<void*>(src)].arrays[0];
            if (r() % 3 == 0) std::memcpy(pos.data(), &sp.data[(r() % 2 ? i0 : i1) * sp.stride], 12);
            As<F61>(a.vft[61])(dst, src, i0, i1, first, pos.data(), types, stream);
            break;
        }
        case 1: {
            const std::vector<float> w = randBits(count * 3 + 3);
            As<F62>(a.vft[62])(dst, src, i0, i1, i2, w.data(), first, count, types, stream);
            break;
        }
        case 2: {
            const std::vector<float> w = randBits(dn * 3);
            As<F63>(a.vft[63])(dst, src, i0, i1, i2, w.data(), types, stream);
            break;
        }
        case 3:
            As<F64>(a.vft[64])(dst, src, i0, i1, i2, types, stream);
            break;
        case 4: {
            const std::vector<float> w = randBits(count * 2 + 2);
            As<F65>(a.vft[65])(dst, src, i0, i1, w.data(), first, count, types, stream);
            break;
        }
        case 5: {
            const std::vector<float> w = randBits(dn * 2);
            As<F66>(a.vft[66])(dst, src, i0, i1, w.data(), types, stream);
            break;
        }
        case 6:
            As<F67>(a.vft[67])(dst, src, i0, i1, types, stream);
            break;
        case 7: {
            const std::vector<float> w = randBits(count + 1);
            As<F68>(a.vft[68])(dst, src, i0, w.data(), first, count, types, stream);
            break;
        }
        case 8: {
            const std::vector<float> w = randBits(dn);
            As<F69>(a.vft[69])(dst, src, i0, w.data(), types, stream);
            break;
        }
        case 9: {
            std::vector<float> pos = randBits(3);
            if (r() % 2) {
                const InterpArray& sp = interpState[s][const_cast<void*>(src)].arrays[0];
                for (int j = 0; j < 3; ++j) {
                    float x, y;
                    std::memcpy(&x, &sp.data[i0 * sp.stride + j * 4], 4);
                    std::memcpy(&y, &sp.data[i1 * sp.stride + j * 4], 4);
                    pos[j] = (x + y) * 0.5f;
                }
            }
            tri[s](dst, src, i0, i1, i2, first, pos.data(), types, stream);
            break;
        }
        case 10: {
            const InterpArray& sp = interpState[s][const_cast<void*>(src)].arrays[0];
            std::vector<float> out(dn * 3 + 3);
            std::memset(out.data(), 0xcd, out.size() * 4);
            weights[s](dst, reinterpret_cast<const float*>(&sp.data[i0 * sp.stride]),
                       reinterpret_cast<const float*>(&sp.data[i1 * sp.stride]),
                       reinterpret_cast<const float*>(&sp.data[i2 * sp.stride]), out.data());
            Log("  weights %s\n", Hex(out.data(), static_cast<unsigned>(out.size() * 4)).c_str());
            break;
        }
        default: {  // 11: copy
            const unsigned from = r() % cap;
            const unsigned n = r() % (cap - (from > first ? from : first) + 1);
            As<F70>(a.vft[70])(dst, src, from, first, n, types, stream);
            break;
        }
        }
        dump(dst);
    }
    for (void* c : chunks) {
        InterpState& st = interpState[s][c];
        delete[] st.vtype->streams->items;
        delete st.vtype->streams;
        delete st.vtype;
        Log("op release %s\n", Name(c).c_str());
        ReleaseRef(c);
    }
}

// ---- running

static void Compare(const char* part, int round)
{
    ++checks;
    if (logText[0] == logText[1]) return;
    if (const char* dump = std::getenv("RS_U5_DUMP"); dump && !bad)
        for (int s = 0; s < 2; ++s)
            if (FILE* f = std::fopen((std::string(dump) + (s ? ".orig.txt" : ".ours.txt")).c_str(), "w")) {
                std::fputs(logText[s].c_str(), f);
                std::fclose(f);
            }
    if (++bad > 6) return;
    const std::string &x = logText[0], &y = logText[1];
    size_t i = 0;
    while (i < x.size() && i < y.size() && x[i] == y[i]) ++i;
    size_t from = x.rfind("op ", i);
    if (from == std::string::npos || i - from > 3000) from = i > 600 ? i - 600 : 0;
    std::printf("MISMATCH %s round %d\n-- ours:\n%s\n-- original:\n%s\n", part, round,
                x.substr(from, i - from + 600).c_str(), y.substr(from, i - from + 600).c_str());
}

static LONG WINAPI Crashed(EXCEPTION_POINTERS* e)
{
    const auto at = reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress);
    for (int s = 0; s < 2; ++s) {
        const auto b = reinterpret_cast<uintptr_t>(mod[s]);
        if (at - b < 0x200000) std::printf("fault in %s +0x%x\n", s ? "the original" : "ours", static_cast<unsigned>(at - b));
    }
    std::printf("fault %08lx at %p, side %d; log end:\n", e->ExceptionRecord->ExceptionCode,
                e->ExceptionRecord->ExceptionAddress, cur);
    if (cur >= 0) {
        const std::string& t = logText[cur];
        std::printf("%s\n", t.substr(t.size() > 3000 ? t.size() - 3000 : 0).c_str());
    }
    std::fflush(stdout);
    ExitProcess(3);
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(&Crashed);
    if (argc < 2) {
        std::printf("usage: diff_rs_u5 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 300;
    SetDllDirectoryA(argv[1]);
    const std::string binDir = argv[1];
    mod[0] = LoadLibraryA((binDir + "\\RenderSystem_7c.dll").c_str());
    mod[1] = GetModuleHandleA("RenderSystem_7c_orig.dll");
    ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    RealNew = static_cast<NewFn>(Proc(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    RealDelete = static_cast<DeleteFn>(Proc(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    CreateNode = As<decltype(CreateNode)>(Proc(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    cache = static_cast<void**>(Proc(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    AddRef = As<decltype(AddRef)>(Proc(ks, "?AddReference@ReferenceCount@Jet@@QBEXXZ"));
    ReleaseRef = As<decltype(ReleaseRef)>(Proc(ks, "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ"));
    for (int i = 0; i < kNames; ++i) nodes[i] = CreateNode(*cache, reinterpret_cast<const unsigned char*>(kTexts[i]));
    nodes[kNames] = nullptr;
    for (void*& p : displayVt) p = reinterpret_cast<void*>(&DisplaySlot);
    displayVt[0x3c / 4] = reinterpret_cast<void*>(&DisplayAdd);
    displayVt[0x40 / 4] = reinterpret_cast<void*>(&DisplayRemove);
    *reinterpret_cast<void**>(displayObj) = displayVt;
    *reinterpret_cast<void**>(displayObj2) = displayVt;
    fixedNames[displayObj] = "D0";
    fixedNames[displayObj2] = "D1";
    fixedNames[viewObj] = "V";
    // a real Options with both buffer sizes set
    As<void*(__fastcall*)(void*, void*)>(Proc(ks, "??0Options@Jet@@QAE@XZ"))(optionsObj, nullptr);
    As<void(__cdecl*)(void*, const char*)>(Proc(ks, "?ProcessRaw@Options@Jet@@QAAXPBD@Z"))(
        optionsObj, "-DynamicVertexBufferSize=123456 -DynamicIndexBufferSize=4321");
    for (void*& p : chunkDisplayVt) p = reinterpret_cast<void*>(&DisplaySlot);
    chunkDisplayVt[0xa8 / 4] = reinterpret_cast<void*>(&DGet);
    chunkDisplayVt[0xac / 4] = reinterpret_cast<void*>(&DPut);
    chunkDisplayVt[0x1f0 / 4] = reinterpret_cast<void*>(&DState);
    chunkDisplayVt[0xe0 / 4] = reinterpret_cast<void*>(&DLines);
    chunkDisplayVt[0x10c / 4] = reinterpret_cast<void*>(&DType);
    *reinterpret_cast<void**>(chunkDisplay) = chunkDisplayVt;
    fixedNames[chunkDisplay] = "CD";
    for (void*& p : lightTestVt) p = reinterpret_cast<void*>(&DisplaySlot);
    lightTestVt[1] = reinterpret_cast<void*>(&LLight);
    *reinterpret_cast<void**>(lightObj) = lightTestVt;
    fixedNames[lightObj] = "L";
    fixedNames[materialObj] = "MAT";
    displayStatic = static_cast<void**>(Proc(mod[1], "?display@VertexType@Jet@@1PBVDisplay@2@B"));
    origShutdown = As<void (*)()>(Proc(mod[1], "?Shutdown@VertexType@Jet@@SAXXZ"));
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        MakeCacheVtables(s);
        LoadApiC(s);
        MakeChunkVtable(s);
        MakeInterpVtable(s);
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
    }
    struct {
        const char* name;
        void (*run)(int, unsigned);
    } parts[] = {{"types", RunTypes}, {"caches", RunCaches}, {"misc", RunMisc}, {"chunks", RunChunks}, {"interp", RunInterp}};
    const char* only = std::getenv("RS_U5_ONLY");
    for (auto& p : parts) {
        if (only && !std::strstr((std::string(",") + only + ",").c_str(), (std::string(",") + p.name + ",").c_str()))
            continue;
        for (int round = 0; round < rounds; ++round) {
            const unsigned seed = rng();
            for (int s = 0; s < 2; ++s) {
                FreshStatics();
                cur = s;
                logText[s].clear();
                names[s].clear();
                blockSize[s].clear();
                allocCount[s] = 0;
                p.run(s, seed);
                cur = -1;
            }
            Compare(p.name, round);
        }
        std::printf("-- %s done\n", p.name);
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
