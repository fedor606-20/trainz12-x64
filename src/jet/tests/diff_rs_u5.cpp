// Differential test of RenderSystem_7c unit U5a: VertexType and its registry (VertexType.cpp), RenderingCache and
// RenderingCacheManager, VertexBufferManager, StaticBuffer and the chunk hints (RenderCaches.cpp), ours against
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
static void FreshStatics()
{
    origShutdown();
    *displayStatic = nullptr;
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
    displayStatic = static_cast<void**>(Proc(mod[1], "?display@VertexType@Jet@@1PBVDisplay@2@B"));
    origShutdown = As<void (*)()>(Proc(mod[1], "?Shutdown@VertexType@Jet@@SAXXZ"));
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        MakeCacheVtables(s);
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
    }
    struct {
        const char* name;
        void (*run)(int, unsigned);
    } parts[] = {{"types", RunTypes}, {"caches", RunCaches}, {"misc", RunMisc}};
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
