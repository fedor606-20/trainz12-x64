// Differential test of the U7 material config extractors, MaterialManager's Create /
// SplitDecoratedMaterialName and MaterialResource (notes/RenderSystem.md), ours against
// RenderSystem_7c_orig.dll in one process.
//
// As in diff_rs_u6d: both modules' Jet::Alloc and PStringCache Create (both the (text) and the (text, length)
// overloads) and Destroy imports are hooked and logged,
// each scenario runs the same script on both sides and the logs must be equal. Stand-ins: a texture manager
// (only reference counted), a render kernel (a display at +0x7c), material blobs (the virtual base
// ReferenceCount through the vbptr at +4, every virtual slot logging, the fields dumped after), texture blobs
// and material plugin blobs whose IsSupported / CreateInstance the scenarios script. The plugin walk goes
// through a hooked LookupPluginType import so both sides see the same types. Config trees are made through
// KernelStandard's ConfigData, once per side over the same shared cache nodes.
//
// Kinds 10..19 add the Material lifetime/factory/data surface: PluginFactoryBase<VMaterial> and MaterialPlugin
// copy/3-arg construction, assignment and destruction, Material and NullMaterial construction with their
// vptr/vbptr/virtual-base and manager traffic, both static Material::CreateInstance overloads (the UId overload
// goes through a second LookupPluginType hook), Material::operator<, NullMaterial::CreateInstance/operator==,
// and the exported vtable/statics mirrors. Code pointers are normalized to self/orig labels and generated
// unique-ID node text is suppressed, so only replay-stable facts are compared.
//
// usage: diff_rs_u7.exe <bin dir> [rounds]
//   env RS_U7_ECHO (echo the log), RS_U7_DUMP=<prefix> (each mismatch's logs: <prefix>.<n>.ours.txt / .orig.txt),
//   RS_U7_ONLY=<scenario> (run only that scenario kind)
#define NOMINMAX
#include <windows.h>

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
static std::mt19937 rng(20261118);

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

// ---------------------------------------------------------------------------------------------------------------
// logging

static int cur = -1;
static std::string logText[2];
static std::map<const void*, std::string> fixedNames;

static void Log(const char* fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (cur >= 0) logText[cur] += buf;
    static const bool echo = std::getenv("RS_U7_ECHO") != nullptr;
    if (echo) {
        std::fputs(buf, stdout);
        std::fflush(stdout);
    }
}

// the node texts are shared (one cache), so they compare directly
static std::string Text(const void* node)
{
    if (!node) return "null";
    return std::string("'") + (static_cast<const char*>(node) + 0x18) + "'";
}

// a UId is 16 raw bytes; log them without interpreting generated text
static std::string HexUid(const void* uid)
{
    char hex[33];
    const auto* b = static_cast<const unsigned char*>(uid);
    for (unsigned i = 0; i < 16; ++i) std::snprintf(hex + 2 * i, 3, "%02x", b[i]);
    return hex;
}

// interned nodes come from one shared cache, so a node address is stable across the two sides; track them
// (and the Alloc::New traffic) so raw pointer fields can be dumped as equal labels on both sides
static unsigned nodeMade[2], nodeKilled[2];
static void* nodeList[2][4096];
static unsigned nodeCount[2];
// the lifetime/factory scenarios only compare normalized field bytes and node counts: the DLL's generated
// unique-ID nodes contain a shared counter whose text necessarily differs between the two sequential passes
static bool quietNodes;
// case 18 compares exported data objects: a copied vftable slot may legally point into either module
static bool dataMode;

static void TrackNode(void* n)
{
    if (cur < 0 || !n) return;
    ++nodeMade[cur];
    for (unsigned i = 0; i < nodeCount[cur]; ++i)
        if (nodeList[cur][i] == n) return;
    if (nodeCount[cur] < 4096) nodeList[cur][nodeCount[cur]++] = n;
}
static const char* NodeLabel(const void* p, int s)
{
    if (!p) return "null";
    for (unsigned i = 0; i < nodeCount[s]; ++i) {
        const auto* n = static_cast<const unsigned char*>(nodeList[s][i]);
        if (p == n) return "node";
        if (p == n + 0x18) return "text";
    }
    return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// the hooked imports (both modules)

using NewFn = void*(__cdecl*)(unsigned);
using DeleteFn = void(__cdecl*)(void*);
using NodeFn = void*(__cdecl*)(void*, const unsigned char*);
using NodeLenFn = void*(__cdecl*)(void*, const unsigned char*, unsigned);
using NodeDestroyFn = void(__cdecl*)(void*, void*);

static NewFn RealNew;
static DeleteFn RealDelete;
static NodeFn RealCreateNode;
static NodeLenFn RealCreateNodeLen;
static NodeDestroyFn RealDestroyNode;
static void** cache;

static void* names[2][8192];
static unsigned nameCount[2];

static const char* Name(const void* p)
{
    static char buf[32];
    if (!p) return "null";
    auto f = fixedNames.find(p);
    if (f != fixedNames.end()) return f->second.c_str();
    for (unsigned i = 0; i < nameCount[cur]; ++i)
        if (names[cur][i] == p) {
            std::snprintf(buf, sizeof buf, "A%u", i);
            return buf;
        }
    return "?";
}

static void* __cdecl HookNew(unsigned n)
{
    void* p = RealNew(n);
    if (cur >= 0 && p) {
        if (nameCount[cur] < 8192) names[cur][nameCount[cur]++] = p;
        Log("  new %u -> %s\n", n, Name(p));
    }
    return p;
}
static void __cdecl HookDelete(void* p)
{
    if (cur >= 0 && p) Log("  free %s\n", Name(p));
    RealDelete(p);
}
static void* __cdecl HookCreateNode(void* self, const unsigned char* text)
{
    void* n = RealCreateNode(self, text);
    TrackNode(n);
    if (cur >= 0) {
        if (quietNodes)
            Log("  node\n");
        else
            Log("  node %s -> %s\n", text ? reinterpret_cast<const char*>(text) : "null", Text(n).c_str());
    }
    return n;
}
// the (text, length) overload: the original inlines its literal keys through this one, ours through the 1-arg
// one, so both are hooked and logged alike (the interned node and its order are what the test compares)
static void* __cdecl HookCreateNodeLen(void* self, const unsigned char* text, unsigned length)
{
    void* n = RealCreateNodeLen(self, text, length);
    TrackNode(n);
    if (cur >= 0) {
        if (quietNodes)
            Log("  node\n");
        else {
            const std::string s(text ? reinterpret_cast<const char*>(text) : "", text ? length : 0);
            Log("  node %s -> %s\n", text ? s.c_str() : "null", Text(n).c_str());
        }
    }
    return n;
}
static void __cdecl HookDestroyNode(void* self, void* node)
{
    if (cur >= 0) {
        ++nodeKilled[cur];
        if (quietNodes)
            Log("  unnode\n");
        else
            Log("  unnode %s\n", Text(node).c_str());
    }
    RealDestroyNode(self, node);
}

static void PatchImport(HMODULE m, const char* name, void* fn)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto* dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir->VirtualAddress); d->Name; ++d) {
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        auto* orig = reinterpret_cast<IMAGE_THUNK_DATA*>(
            base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        for (; orig->u1.AddressOfData; ++thunk, ++orig) {
            if (IMAGE_SNAP_BY_ORDINAL(orig->u1.Ordinal)) continue;
            auto* iname = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + orig->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(iname->Name), name) == 0) {
                DWORD old;
                VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
                thunk->u1.Function = reinterpret_cast<ULONG_PTR>(fn);
                VirtualProtect(&thunk->u1.Function, sizeof(void*), old, &old);
            }
        }
    }
}

// the value an import slot had before it was patched (so a hook can delegate outside the scripted scenarios)
static void* ImportFunction(HMODULE m, const char* name)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto* dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir->VirtualAddress); d->Name; ++d) {
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        auto* orig = reinterpret_cast<IMAGE_THUNK_DATA*>(
            base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        for (; orig->u1.AddressOfData; ++thunk, ++orig) {
            if (IMAGE_SNAP_BY_ORDINAL(orig->u1.Ordinal)) continue;
            auto* iname = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + orig->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(iname->Name), name) == 0)
                return reinterpret_cast<void*>(thunk->u1.Function);
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// PString helpers (its constructors are not exported; the cache makes the nodes)

static void* MakePString(const char* text)
{
    void* p = RealNew(4);
    void* n = RealCreateNode(*cache, reinterpret_cast<const unsigned char*>(text));
    TrackNode(n);
    *static_cast<void**>(p) = n;
    return p;
}
// a UId whose first bytes spell a scripted plugin tag (the hook below maps it back to that Plugin blob)
static void* MakeUid(const char* tag)
{
    void* p = RealNew(16);
    std::memset(p, 0, 16);
    std::strncpy(static_cast<char*>(p), tag, 15);
    return p;
}
static void FreePString(void* p)
{
    if (*static_cast<void**>(p)) RealDestroyNode(*cache, *static_cast<void**>(p));
    RealDelete(p);
}
// the shared cache's accounting (PStringCache::Node::refs at +0x10, PStringCache::liveRefs at +4)
static int NodeRefs(const void* node)
{
    return node ? *reinterpret_cast<const int*>(static_cast<const unsigned char*>(node) + 0x10) : 0;
}
static int CacheLiveRefs() { return *reinterpret_cast<const int*>(static_cast<const unsigned char*>(*cache) + 4); }

// ---------------------------------------------------------------------------------------------------------------
// stand-ins

// The material object as TS12 lays it out: the vfptr at +0 (pointing at a vtable kept outside the object), the
// virtual-base vbptr at +4 (the reference count subobject at +0xc: its vfptr then the count), the name and
// request fields from +0x24 on and the two lists at +0x88 / +0x90. The methods the extractors call are the
// vtable's slots at 0x20 (GetTexture), 0x24 (SetTextureRef), 0x28 (SetTextureName) and 0x48 (AddProperty).
struct MaterialBlob {
    void* vfptr;               // +0x00
    void* vbptr;               // +0x04
    unsigned char body[0x9c];  // +0x08 ...
};
static MaterialBlob materialPool[16];
static void* materialVtable[0x60 / 4];  // slot 0x54 (the operator< key) is exercised by a later scenario
static void* materialVbtable[2];
static void* materialRefVtable[2];
static unsigned materialUsed;
static void* texturePool[4];
static bool texturesByType[16];

static void* __cdecl MatDeleteThis(void* self) { Log("  %s.deleteThis\n", Name(self)); return nullptr; }
static void* __cdecl MatRefCountAdd(void* self) { Log("  %s.rcAdd\n", Name(self)); return nullptr; }
static void* __cdecl MatRefCountRelease(void* self) { Log("  %s.rcRelease\n", Name(self)); return nullptr; }
static void* __cdecl MatGetTexture(void* self, int type)
{
    Log("  %s.getTexture %d\n", Name(self), type);
    return (type >= 0 && type < 16 && texturesByType[type]) ? texturePool[type % 4] : nullptr;
}
static void* __cdecl MatSetTextureRef(void* self, int* result, int type, void* ref, float amount)
{
    Log("  %s.setTextureRef %d %s %08x\n", Name(self), type, Name(*static_cast<void**>(ref)),
        *reinterpret_cast<unsigned*>(&amount));
    *result = 0;
    return result;
}
static void* __cdecl MatSetTextureName(void* self, int* result, int type, const void* db, const void* name,
                                      float amount, const void* fallback)
{
    Log("  %s.setTextureName %d %s %08x\n", Name(self), type, Text(*static_cast<void* const*>(name)).c_str(),
        *reinterpret_cast<unsigned*>(&amount));
    *result = 0;
    return result;
}
static void* __cdecl MatAddProperty(void* self, const void* key, const void* value)
{
    Log("  %s.addProperty %s=%s\n", Name(self), Text(*static_cast<void* const*>(key)).c_str(),
        Text(*static_cast<void* const*>(value)).c_str());
    return nullptr;
}

static void InitMaterial(void* memory, const char* tag)
{
    auto* blob = static_cast<MaterialBlob*>(memory);
    std::memset(blob, 0, sizeof(MaterialBlob));
    for (unsigned i = 0; i < 0x60 / 4; ++i) materialVtable[i] = reinterpret_cast<void*>(&MatGetTexture);
    materialVtable[0x20 / 4] = reinterpret_cast<void*>(&MatGetTexture);
    materialVtable[0x24 / 4] = reinterpret_cast<void*>(&MatSetTextureRef);
    materialVtable[0x28 / 4] = reinterpret_cast<void*>(&MatSetTextureName);
    materialVtable[0x48 / 4] = reinterpret_cast<void*>(&MatAddProperty);
    materialVbtable[0] = nullptr;
    materialVbtable[1] = reinterpret_cast<void*>(8);
    materialRefVtable[0] = reinterpret_cast<void*>(&MatDeleteThis);
    materialRefVtable[1] = reinterpret_cast<void*>(&MatDeleteThis);
    blob->vfptr = materialVtable;
    blob->vbptr = materialVbtable;
    *reinterpret_cast<void**>(blob->body + 4) = materialRefVtable;  // the reference count's vfptr at +0x0c
    *reinterpret_cast<int*>(blob->body + 8) = 1;                    // its count at +0x10
    fixedNames[memory] = tag;
    fixedNames[blob->body + 4] = tag;
}
static void* NextMaterial(const char* tag)
{
    void* m = &materialPool[materialUsed < 16 ? materialUsed++ : 15];
    InitMaterial(m, tag);
    return m;
}

// Material::operator< calls a virtual slot at 0x54 that hands out a key. The stand-in keeps the wrapper
// at +0x20 (outside the fields the other scenarios touch) and answers both the pointer-return and the
// sret-return shapes, so the call is harmless whichever the two modules use.
static void* keyedVtable[0x60 / 4];
static void* __cdecl MatLessKey(void* self)
{
    void* key = *reinterpret_cast<void**>(static_cast<unsigned char*>(self) + 0x20);
    Log("  key\n");
    return key;
}
static void SetupKeyed(void* obj, void* key)
{
    auto* b = static_cast<unsigned char*>(obj);
    std::memset(b, 0, 0xc0);
    std::memcpy(keyedVtable, materialVtable, sizeof keyedVtable);
    keyedVtable[0x54 / 4] = reinterpret_cast<void*>(&MatLessKey);
    *reinterpret_cast<void**>(b) = keyedVtable;
    *reinterpret_cast<void**>(b + 4) = materialVbtable;
    *reinterpret_cast<void**>(b + 8) = materialRefVtable;
    *reinterpret_cast<int*>(b + 0xc) = 1;
    *reinterpret_cast<void**>(b + 0x20) = key;
}

// the plugin walk is scripted: the type name maps to a plugin blob (the vfptr outside the object, as a real
// PluginType has it)
struct Plugin {
    void* vfptr = nullptr;
    const char* tag = "P";
    bool supported = true;
    bool fallback = false;
    std::string fallbackName = "oneten";
    void* result = nullptr;
};
static void* pluginVtable[8];
static std::vector<Plugin*> plugins;

static void* __cdecl PluginAddRef(void* self) { Log("  %s.addref\n", static_cast<Plugin*>(self)->tag); return nullptr; }
static void* __cdecl PluginRelease(void* self) { Log("  %s.release\n", static_cast<Plugin*>(self)->tag); return nullptr; }
static void* __cdecl PluginDelete(void* self) { Log("  %s.delete\n", static_cast<Plugin*>(self)->tag); return nullptr; }
static void* __cdecl PluginCreateBare(void* self)
{
    Log("  %s.createBare\n", static_cast<Plugin*>(self)->tag);
    return nullptr;
}
static unsigned char __cdecl PluginSupported(void* self, void* display, void* fallback)
{
    auto* p = static_cast<Plugin*>(self);
    Log("  %s.supported d=%s\n", p->tag, display ? "yes" : "no");
    if (p->fallback) {
        // a plugin's fallback name assignment: the old node goes first
        void*& node = *static_cast<void**>(fallback);
        Log("  %s.fallback %s\n", p->tag, p->fallbackName.c_str());
        if (node) RealDestroyNode(*cache, node);
        node = RealCreateNode(*cache, reinterpret_cast<const unsigned char*>(p->fallbackName.c_str()));
    }
    return p->supported ? 1 : 0;
}
static void* __cdecl PluginCreate(void* self, void* manager, void* kernel)
{
    auto* p = static_cast<Plugin*>(self);
    Log("  %s.create m=%s k=%s\n", p->tag, manager ? "yes" : "no", kernel ? "yes" : "no");
    return p->result;
}

// the hooked LookupPluginType (Result LookupPluginType(PluginType**, const UId&, const PString&))
static int* __cdecl HookLookup(int* sret, void** out, const void* uid, const void* name)
{
    const void* node = *static_cast<void* const*>(name);
    const char* text = node ? static_cast<const char*>(node) + 0x18 : nullptr;
    Log("  lookup %s\n", text ? text : "null");
    for (Plugin* p : plugins) {
        if (text && std::strcmp(text, p->tag) == 0) {
            *out = p;
            Log("  %s.addref\n", p->tag);
            *sret = 0;
            return sret;
        }
    }
    *out = nullptr;
    *sret = 3;
    return sret;
}

// the UId-only overload of LookupPluginType: used by Material::CreateInstance(const UId&, ...). Outside the
// scripted factory scenarios the original import is called unchanged, so kinds 0..9 keep their behaviour.
static bool uidHookEnabled;
using LookupUidFn = int* (__cdecl*)(int*, void**, const void*);
static LookupUidFn RealLookupUid;

static bool UidTag(const void* uid, const char* tag)
{
    const auto* b = static_cast<const unsigned char*>(uid);
    const std::size_t n = std::strlen(tag);
    if (n >= 16) return false;
    for (std::size_t i = 0; i < n; ++i)
        if (b[i] != static_cast<unsigned char>(tag[i])) return false;
    for (std::size_t i = n; i < 16; ++i)
        if (b[i] != 0) return false;
    return true;
}
static int* __cdecl HookLookupUid(int* sret, void** out, const void* uid)
{
    if (!uidHookEnabled) {
        if (RealLookupUid) return RealLookupUid(sret, out, uid);
        *out = nullptr;
        *sret = 3;
        return sret;
    }
    Log("  lookupUid %s\n", HexUid(uid).c_str());
    for (Plugin* p : plugins) {
        if (UidTag(uid, p->tag)) {
            *out = p;
            Log("  %s.addref\n", p->tag);
            *sret = 0;
            return sret;
        }
    }
    *out = nullptr;
    *sret = 3;
    return sret;
}

// the manager stand-in (GetMaterial and CreateNewMaterial go through its Create slot)
static void* managerObj;
static void* managerVtable[8];
static void* managerMaterial;
static int managerCode;

static void* __cdecl ManagerCreate(void* self, int* result, void** out, const void* name)
{
    Log("  manager.create %s\n", Text(*static_cast<void* const*>(name)).c_str());
    *out = managerCode == 0 ? managerMaterial : nullptr;
    *result = managerCode;
    return result;
}
// Material's dtor unregisters itself from its manager through slot 0x14
static void __cdecl ManagerRemove(void* self, void* material) { Log("  mgr.remove %s\n", Name(material)); }

static unsigned char textureManagerObj[0x20];
static unsigned char kernelObj[0x100];
static unsigned char displayObj[0x220];  // +0x50 the light count, +0x54 the state block, +0x20c the manager
static unsigned char dbObj[0x20];

// the database stand-in's slot 3 (GetFileName): the name's text, or dbMapped when set (the load then parses
// that file)
static void* dbVtable[8];
static const char* dbMapped;

static void* __cdecl DbGetFileName(void* self, void* ret, const void* name)
{
    const char* text = nullptr;
    if (name) {
        void* node = *static_cast<void* const*>(name);
        if (node) text = static_cast<const char*>(node) + 0x18;
    }
    Log("  %s.getFileName '%s'\n", Name(self), text ? text : "null");
    const char* answer = dbMapped ? dbMapped : text;
    *static_cast<void**>(ret) =
        answer ? RealCreateNode(*cache, reinterpret_cast<const unsigned char*>(answer)) : nullptr;
    return ret;  // the class return goes back in EAX, as the callers expect
}

// ---------------------------------------------------------------------------------------------------------------
// the chunk and lighting stand-ins (U7: NullMaterial::CreateGeometryChunk and Material::SetLightingState)

// an interior pointer named through the allocation it sits in ("A<alloc>+<offset>"), the way the chunk's
// vertex data points inside its block
static std::string Near(const void* p)
{
    const auto* b = static_cast<const unsigned char*>(p);
    const void* best = nullptr;
    unsigned bestOffset = 0;
    for (unsigned i = 0; i < nameCount[cur]; ++i) {
        const auto* s = static_cast<const unsigned char*>(names[cur][i]);
        if (s <= b && (!best || s > best)) {
            best = s;
            bestOffset = static_cast<unsigned>(b - s);
        }
    }
    if (!best) return Name(p);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s+%x", Name(best), bestOffset);
    return buf;
}

// the vertex buffer the display slots hand out: a reference count (slot 0 the delete) and a name
struct FakeBuffer {
    void* vtable;
    int refs;
};
static FakeBuffer buffers[8];
static void* bufferVtable[4];
static unsigned bufferUsed;
static void __cdecl BufferDeleteThis(void* self) { Log("  %s.deleteThis\n", Name(self)); }

// the vertex type the display's declaration slot hands out: a reference count and a streams block whose first
// word is the stride
struct FakeVertexType {
    void* vtable;
    int refs;
    unsigned id;
    unsigned streams;
    void* streamList;
};
static FakeVertexType vertexType;
static unsigned vertexTypeStream[4];
static void __cdecl VertexTypeDeleteThis(void* self) { Log("  %s.deleteThis\n", Name(self)); }

// the program's slot 17 (0x44): (self, lights, four floats by value, the material block, the transform)
struct ColorArg {
    float v[4];
};
static unsigned char programObj[8];
static void* programVtable[24];
static unsigned char matrixObj[0x40];

static void __cdecl ProgramSlot17(void* self, const void* lights, ColorArg color, const void* block,
                                  const void* transform)
{
    Log("  %s.light %s %08x%08x%08x%08x", Name(self), Name(lights),
        *reinterpret_cast<const unsigned*>(&color.v[0]), *reinterpret_cast<const unsigned*>(&color.v[1]),
        *reinterpret_cast<const unsigned*>(&color.v[2]), *reinterpret_cast<const unsigned*>(&color.v[3]));
    const auto* m = static_cast<const unsigned char*>(block);
    for (unsigned i = 0; i < 0x44; i += 4) Log(" %08x", *reinterpret_cast<const unsigned*>(m + i));
    Log(" w=%s\n", Name(transform));
}

// the display's slots: 0x90 lights(count, matrix), 0x94 end, 0xcc material, 0x104/0x108 make vertex/index
// buffer, 0x10c declaration, 0x1f0 lighting mode, 0x224/0x22c one light
static void* displayVtable[0x230 / 4];
static unsigned char stateBlock[0x120];
static unsigned char lightSet[16];
static int displayMaxLights;
static bool displayNoBuffer;   // the make-buffer slots answer null
static int managerUploadCode;  // 8 answers false, 11 answers -1, 0 both succeed
static unsigned char managerObj2[8];

static void __cdecl DisplayLights(void* self, unsigned count, const void* matrix)
{
    Log("  %s.lights %u %s\n", Name(self), count, Name(matrix));
}
static void __cdecl DisplayEnd(void* self) { Log("  %s.end\n", Name(self)); }
static void __cdecl DisplayMaterialSlot(void* self, const void* material)
{
    Log("  %s.material", Name(self));
    const auto* m = static_cast<const unsigned char*>(material);
    for (unsigned i = 0; i < 0x44; i += 4) Log(" %08x", *reinterpret_cast<const unsigned*>(m + i));
    Log("\n");
}
static void* __cdecl DisplayMakeVertex(void* self, unsigned vertices, unsigned stride)
{
    FakeBuffer* b = displayNoBuffer ? nullptr : &buffers[bufferUsed < 8 ? bufferUsed++ : 7];
    Log("  %s.makeVertex %u %u -> %s\n", Name(self), vertices, stride, b ? Name(b) : "null");
    return b;
}
static void* __cdecl DisplayMakeIndex(void* self, unsigned count, unsigned)
{
    FakeBuffer* b = displayNoBuffer ? nullptr : &buffers[bufferUsed < 8 ? bufferUsed++ : 7];
    Log("  %s.makeIndex %u -> %s\n", Name(self), count, b ? Name(b) : "null");
    return b;
}
static void* __cdecl DisplayDeclaration(void* self, int format)
{
    Log("  %s.declaration %d -> VT\n", Name(self), format);
    return &vertexType;
}
static void __cdecl DisplayLightingMode(void* self, int on) { Log("  %s.mode %d\n", Name(self), on); }
static void __cdecl DisplayLightSlot(void* self, unsigned i, int on) { Log("  %s.light %u %d\n", Name(self), i, on); }
static void __cdecl DisplayOneLight(void* self, unsigned i, void* light)
{
    Log("  %s.setlight %u %s\n", Name(self), i, Name(light));
}
static unsigned char __cdecl ManagerUpload(void* self, void* type, void* buffer, void* data, unsigned count,
                                           int one, void* out, int zero)
{
    Log("  %s.uploadV %s %s %s %u %d %d\n", Name(self), Near(type).c_str(), Name(buffer), Near(data).c_str(), count,
        one, zero);
    *static_cast<unsigned*>(out) = 0;
    return managerUploadCode == 8 ? 0 : 1;
}
static int __cdecl ManagerUploadIndex(void* self, void* buffer, void* data, unsigned count, int one, void* out)
{
    Log("  %s.uploadI %s %s %u %d\n", Name(self), Name(buffer), Near(data).c_str(), count, one);
    *static_cast<unsigned*>(out) = 0;
    return managerUploadCode == 1 ? -1 : 0;
}

// the light ring of a FastDynamicArray<DisplayLight*, 13> (chunks of 13 {next, prev, count, items})
struct LightChunk {
    LightChunk* next;
    LightChunk* prev;
    unsigned count;
    void* items[13];
};
static LightChunk lightChunks[2];
static void* lightPool[4];
static unsigned char chunkObj[0x50];  // the chunk SetLightingState reads the flags of

static void* BuildLights(std::mt19937& r, unsigned chunks)
{
    if (!chunks) return nullptr;
    for (unsigned c = 0; c < chunks; ++c) {
        LightChunk& chunk = lightChunks[c];
        chunk.count = c + 1 == chunks ? r() % 14 : 13;
        for (unsigned i = 0; i < chunk.count; ++i) chunk.items[i] = lightPool[r() % 4];
    }
    for (unsigned c = 0; c < chunks; ++c) {
        lightChunks[c].next = &lightChunks[(c + 1) % chunks];
        lightChunks[c].prev = &lightChunks[(c + chunks - 1) % chunks];
    }
    return &lightChunks[0];
}

static void ResetLightingStandins()
{
    std::memset(stateBlock, 0, sizeof stateBlock);
    std::memset(lightSet, 0, sizeof lightSet);
    *reinterpret_cast<unsigned char**>(stateBlock + 0x110) = lightSet;
    displayMaxLights = 0;
    displayNoBuffer = false;
    managerUploadCode = 0;
    bufferUsed = 0;
    for (auto& b : buffers) b.refs = 1;
    vertexType.refs = 1;
    *reinterpret_cast<int*>(displayObj + 0x50) = 0;
    *reinterpret_cast<void**>(displayObj + 0x54) = stateBlock;
    *reinterpret_cast<void**>(displayObj + 0x20c) = managerObj2;
}

// ---------------------------------------------------------------------------------------------------------------
// the per-side API

struct Api {
    void* (__thiscall* managerCtor)(void*, void*);
    void* (__fastcall* managerDtor)(void*, void*);
    void (__cdecl* managerAdd)(void*, void*);
    int* (__cdecl* create)(void*, int*, void**, const void*);
    int* (__cdecl* split)(int*, const void*, void*, void*);
    void* (__cdecl* extractSpec)(void*, const void*);
    void* (__cdecl* extractSpecs)(void*, void*, const void*);
    void* (__cdecl* extractMaterial)(void*, void*, const void*, const void*);
    void* (__cdecl* extractMaterials)(void*, void*, void*, const void*, const void*);
    void* (__cdecl* getMaterial)(void*, void*, const void*, const void*);
    void* (__cdecl* createNew)(void*, void*, void*, const void*);
    void (__fastcall* specDtor)(void*, void*);
    // MaterialResource (the dtor as __fastcall like the manager's, the rest as declared)
    void* (__thiscall* resCtor)(void*, const void*, const void*);
    void* (__thiscall* resCtorCopy)(void*, const void*);
    void* (__thiscall* resAssign)(void*, const void*);
    void (__fastcall* resDtor)(void*, void*);
    int* (__cdecl* resLoadTxt)(void*, int*, const char*);
    void* (__cdecl* resCreateInstance)(const void*, void*, const void*);
    // the null material's chunk and the material's lighting state
    void* (__cdecl* createChunk)(void*, unsigned, unsigned, unsigned, unsigned, int, void*);
    void (__cdecl* setLighting)(void*, void*, const void*, const void*, const void*, const void*, const void*, float,
                                int);
    // U7 lifetime/factory exports. The ctors/dtors/assign are called as fastcall (ECX=self, EDX=dummy,
    // stack args); the comparison, the two static factories and the two virtual NullMaterial methods are
    // plain cdecl helpers with everything on the stack. A hidden virtual-base flag ends the ctor arg lists.
    void* (__fastcall* pfbCopy)(void*, void*, const void*);
    void* (__fastcall* pfbCtor3)(void*, void*, const void*, const void*, const void*);
    void (__fastcall* pfbDtor)(void*, void*);
    void* (__fastcall* pfbAssign)(void*, void*, const void*);
    void* (__fastcall* mpCopy)(void*, void*, const void*);
    void* (__fastcall* mpCtor3)(void*, void*, const void*, const void*);
    void* (__fastcall* matCtor)(void*, void*, void*, void*);
    void (__fastcall* matDtor)(void*, void*);
    void (__fastcall* matVbaseDtor)(void*, void*);
    bool (__cdecl* matLess)(const void*, const void*);
    void* (__cdecl* matCreateUid)(const void*, void*);
    void* (__cdecl* matCreateName)(const void*, void*);
    void* (__fastcall* nullCtor)(void*, void*, const void*, void*);
    void (__fastcall* nullDtor)(void*, void*);
    void (__fastcall* nullVbaseDtor)(void*, void*);
    void* (__cdecl* nullCreate)(void*);
    bool (__cdecl* nullEqual)(const void*, const void*);
    // data exports (vftables/vbtables/statics; read as addresses, never called through)
    void* matVftable;
    void* nullVftable;
    void* mpVftable;
    void* pfbVftable;
    void* matVbtable;
    void* nullVbtable;
    void* uniqueMatId;
    void* matResults;
    void* mgrResults;
    void* decoratedToken;
    void* stringNotFound;
    void* pluginGroupName;
    void* pluginGroupUid;
};
static Api api[2];

static void LoadApi(int s)
{
    HMODULE m = mod[s];
    Api& a = api[s];
    a.managerCtor = As<decltype(a.managerCtor)>(Proc(m, "??0MaterialManager@Jet@@QAE@PAVTextureManager@1@@Z"));
    a.managerDtor = As<decltype(a.managerDtor)>(Proc(m, "??1MaterialManager@Jet@@UAE@XZ"));
    a.managerAdd = As<decltype(a.managerAdd)>(Proc(m, "?Add@MaterialManager@Jet@@UAAXPAVMaterial@2@@Z"));
    a.create = As<decltype(a.create)>(
        Proc(m, "?Create@MaterialManager@Jet@@UAA?AV?$ResultClass@$1?results@MaterialManager@Jet@@2QBUResultTable@2@"
                "BW4Results@MaterialManager@Jet@@@2@AAPAVMaterial@2@ABVPString@2@@Z"));
    a.split = As<decltype(a.split)>(
        Proc(m, "?SplitDecoratedMaterialName@MaterialManager@Jet@@SA?AV?$ResultClass@$1?results@MaterialManager@"
                "Jet@@2QBUResultTable@2@BW4Results@MaterialManager@Jet@@@2@ABVPString@2@AAV42@1@Z"));
    a.extractSpec = As<decltype(a.extractSpec)>(
        Proc(m, "?ExtractMaterialSpec@MaterialConfig@Jet@@QBAPAUMaterialSpec@2@ABVConfigData@2@@Z"));
    a.extractSpecs = As<decltype(a.extractSpecs)>(
        Proc(m, "?ExtractMaterialSpecs@MaterialConfig@Jet@@QBA?AV?$FastDynamicArray@PAUMaterialSpec@Jet@@$0N@U?$"
                "Less@PAUMaterialSpec@Jet@@PAU12@@2@@2@ABVConfigData@2@@Z"));
    a.extractMaterial = As<decltype(a.extractMaterial)>(
        Proc(m, "?ExtractMaterial@MaterialConfig@Jet@@QBAPAVMaterial@2@PAVMaterialManager@2@PBVResourceDatabase@2@"
                "ABVConfigData@2@@Z"));
    a.extractMaterials = As<decltype(a.extractMaterials)>(
        Proc(m, "?ExtractMaterials@MaterialConfig@Jet@@QBA?AV?$FastDynamicArray@PAVMaterial@Jet@@$0N@U?$Less@PAVMaterial@"
                "Jet@@PAV12@@2@@2@PAVMaterialManager@2@PBVResourceDatabase@2@ABVConfigData@2@@Z"));
    a.getMaterial = As<decltype(a.getMaterial)>(
        Proc(m, "?GetMaterial@MaterialConfig@Jet@@QBAPAVMaterial@2@PAVMaterialManager@2@PBVResourceDatabase@2@ABU"
                "MaterialSpec@2@@Z"));
    a.createNew = As<decltype(a.createNew)>(
        Proc(m, "?CreateNewMaterial@MaterialConfig@Jet@@IBAPAVMaterial@2@PAVMaterialManager@2@PAV32@ABVPString@2@@Z"));
    a.specDtor = As<decltype(a.specDtor)>(Proc(m, "??1MaterialSpec@Jet@@QAE@XZ"));
    a.resCtor = As<decltype(a.resCtor)>(Proc(m, "??0MaterialResource@Jet@@QAE@ABVPString@1@PBVResourceDatabase@1@@Z"));
    a.resCtorCopy = As<decltype(a.resCtorCopy)>(Proc(m, "??0MaterialResource@Jet@@QAE@ABV01@@Z"));
    a.resAssign = As<decltype(a.resAssign)>(Proc(m, "??4MaterialResource@Jet@@QAEAAV01@ABV01@@Z"));
    a.resDtor = As<decltype(a.resDtor)>(Proc(m, "??1MaterialResource@Jet@@UAE@XZ"));
    a.resLoadTxt = As<decltype(a.resLoadTxt)>(Proc(m,
        "?LoadTxt@MaterialResource@Jet@@MAA?AV?$ResultClass@$1?results@Resource@Jet@@2QBUResultTable@2@"
        "BW4Results@Resource@Jet@@@2@PBD@Z"));
    a.resCreateInstance = As<decltype(a.resCreateInstance)>(Proc(m,
        "?CreateInstance@MaterialResource@Jet@@UBAPAVMaterial@2@PAVMaterialManager@2@PBVResourceDatabase@2@@Z"));
    a.createChunk = As<decltype(a.createChunk)>(Proc(m,
        "?CreateGeometryChunk@NullMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAV"
        "GeometryChunkCreateHint@2@@Z"));
    a.setLighting = As<decltype(a.setLighting)>(Proc(m,
        "?SetLightingState@Material@Jet@@QBAXPAVDisplay@2@PBVMatrix4@2@PBVGeometryChunk@2@PBVVertexProgram@2@PBV?$"
        "FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@1M_N@Z"));
    // the new lifetime/factory exports
    a.pfbCopy = As<decltype(a.pfbCopy)>(Proc(m, "??0?$PluginFactoryBase@VMaterial@Jet@@@Jet@@QAE@ABV01@@Z"));
    a.pfbCtor3 = As<decltype(a.pfbCtor3)>(Proc(m, "??0?$PluginFactoryBase@VMaterial@Jet@@@Jet@@QAE@ABVPString@1@ABVUId@1@1@Z"));
    a.pfbDtor = As<decltype(a.pfbDtor)>(Proc(m, "??1?$PluginFactoryBase@VMaterial@Jet@@@Jet@@UAE@XZ"));
    a.pfbAssign = As<decltype(a.pfbAssign)>(Proc(m, "??4?$PluginFactoryBase@VMaterial@Jet@@@Jet@@QAEAAV01@ABV01@@Z"));
    a.mpCopy = As<decltype(a.mpCopy)>(Proc(m, "??0MaterialPlugin@Jet@@QAE@ABV01@@Z"));
    a.mpCtor3 = As<decltype(a.mpCtor3)>(Proc(m, "??0MaterialPlugin@Jet@@QAE@ABVPString@1@ABVUId@1@@Z"));
    a.matCtor = As<decltype(a.matCtor)>(Proc(m, "??0Material@Jet@@QAE@PAVMaterialManager@1@@Z"));
    a.matDtor = As<decltype(a.matDtor)>(Proc(m, "??1Material@Jet@@MAE@XZ"));
    a.matVbaseDtor = As<decltype(a.matVbaseDtor)>(Proc(m, "??_DMaterial@Jet@@IAEXXZ"));
    a.matLess = As<decltype(a.matLess)>(Proc(m, "??MMaterial@Jet@@QBA_NABV01@@Z"));
    a.matCreateUid = As<decltype(a.matCreateUid)>(Proc(m, "?CreateInstance@Material@Jet@@SAPAV12@ABVUId@2@PAVMaterialManager@2@@Z"));
    a.matCreateName = As<decltype(a.matCreateName)>(Proc(m, "?CreateInstance@Material@Jet@@SAPAV12@ABVPString@2@PAVMaterialManager@2@@Z"));
    a.nullCtor = As<decltype(a.nullCtor)>(Proc(m, "??0NullMaterial@Jet@@QAE@ABVPString@1@@Z"));
    a.nullDtor = As<decltype(a.nullDtor)>(Proc(m, "??1NullMaterial@Jet@@UAE@XZ"));
    a.nullVbaseDtor = As<decltype(a.nullVbaseDtor)>(Proc(m, "??_DNullMaterial@Jet@@QAEXXZ"));
    a.nullCreate = As<decltype(a.nullCreate)>(Proc(m, "?CreateInstance@NullMaterial@Jet@@UBAPAVMaterial@2@XZ"));
    a.nullEqual = As<decltype(a.nullEqual)>(Proc(m, "??8NullMaterial@Jet@@UBA_NABVMaterial@1@@Z"));
    // the data mirrors
    a.matVftable = Proc(m, "??_7Material@Jet@@6B01@@");
    a.nullVftable = Proc(m, "??_7NullMaterial@Jet@@6BMaterial@1@@");
    a.mpVftable = Proc(m, "??_7MaterialPlugin@Jet@@6B@");
    a.pfbVftable = Proc(m, "??_7?$PluginFactoryBase@VMaterial@Jet@@@Jet@@6B@");
    a.matVbtable = Proc(m, "??_8Material@Jet@@7B@");
    a.nullVbtable = Proc(m, "??_8NullMaterial@Jet@@7B@");
    a.uniqueMatId = Proc(m, "?uniqueMatId@Material@Jet@@0KA");
    a.matResults = Proc(m, "?results@Material@Jet@@2QBUResultTable@2@B");
    a.mgrResults = Proc(m, "?results@MaterialManager@Jet@@2QBUResultTable@2@B");
    a.decoratedToken = Proc(m, "?DecoratedTypeToken@Material@Jet@@2VPString@2@A");
    a.stringNotFound = Proc(m, "?STRING_NOT_FOUND@Material@Jet@@2VPString@2@A");
    a.pluginGroupName = Proc(m, "?PluginGroupName@MaterialPlugin@Jet@@2VPString@2@A");
    a.pluginGroupUid = Proc(m, "?PluginGroupUID@MaterialPlugin@Jet@@2VUId@2@A");
}

// the config building (KernelStandard, shared by both sides). The ctor/dtor are __thiscall; operator() is the
// odd one out: __cdecl with `this` as the first stack argument (the original was built that way).
static void* (__thiscall* configCtor)(void*);
static void* (__cdecl* configChild)(void*, const void*, const void*);  // operator()(this, key, value)
static void* (__cdecl* configBlock)(void*, const void*);               // operator()(this, key)
static void* (__fastcall* configDtor)(void*, void*);

static void AddPair(void* parent, const char* key, const char* value)
{
    void* k = MakePString(key);
    void* v = MakePString(value);
    configChild(parent, k, v);
    FreePString(v);
    FreePString(k);
}
static void* AddBlock(void* parent, const char* key, const char* value)
{
    void* k = MakePString(key);
    void* c;
    if (value) {
        void* v = MakePString(value);
        c = configChild(parent, k, v);
        FreePString(v);
    } else {
        c = configBlock(parent, k);
    }
    FreePString(k);
    return c;
}

// the material files LoadTxt parses are written through ConfigParser::Save (KernelStandard, shared): the same
// "version x.xx" and tree text the parser reads back
static void* (__thiscall* parserCtor)(void*);
static void (__fastcall* parserDtor)(void*, void*);
static void (__cdecl* parserSetConfig)(void*, void*);
static void (__cdecl* parserSetVersion)(void*, float);
static bool (__fastcall* parserSave)(const void*, void*, const char*, bool, int);

// the material fields a dump reads back
static void DumpSpec(std::string& out, const unsigned char* spec)
{
    if (!spec) {
        out += "  spec null\n";
        return;
    }
    char buf[256];
    std::snprintf(buf, sizeof buf, "  spec name=%s rename=%s\n", Text(*reinterpret_cast<void* const*>(spec)).c_str(),
                  Text(*reinterpret_cast<void* const*>(spec + 4)).c_str());
    out += buf;
    out += "  spec mat";
    for (unsigned i = 0; i < 0x44; i += 4) {
        std::snprintf(buf, sizeof buf, " %08x", *reinterpret_cast<const unsigned*>(spec + 8 + i));
        out += buf;
    }
    std::snprintf(buf, sizeof buf, "\n  spec v=%08x %08x %08x flag=%02x flags=%02x%02x%02x%02x%02x%02x%02x%02x\n",
                  *reinterpret_cast<const unsigned*>(spec + 0x4c), *reinterpret_cast<const unsigned*>(spec + 0x50),
                  *reinterpret_cast<const unsigned*>(spec + 0x54), spec[0x58], spec[0x64], spec[0x65], spec[0x66],
                  spec[0x67], spec[0x68], spec[0x69], spec[0x6a], spec[0x6b]);
    out += buf;
    for (int list = 0; list < 2; ++list) {
        const unsigned offset = list ? 0x60 : 0x5c;
        const unsigned itemSize = list ? 12 : 8;
        auto* head = *reinterpret_cast<void* const*>(spec + offset);
        std::snprintf(buf, sizeof buf, "  spec %s:", list ? "tex" : "prop");
        out += buf;
        if (head) {
            auto* chunk = static_cast<const unsigned char*>(head);
            const auto* first = chunk;
            do {
                const unsigned count = *reinterpret_cast<const unsigned*>(chunk + 8);
                for (unsigned i = 0; i < count; ++i) {
                    const unsigned char* item = chunk + 12 + i * itemSize;
                    if (list)
                        std::snprintf(buf, sizeof buf, " (%d %s %08x)", *reinterpret_cast<const int*>(item),
                                      Text(*reinterpret_cast<void* const*>(item + 4)).c_str(),
                                      *reinterpret_cast<const unsigned*>(item + 8));
                    else
                        std::snprintf(buf, sizeof buf, " (%s=%s)", Text(*reinterpret_cast<void* const*>(item)).c_str(),
                                      Text(*reinterpret_cast<void* const*>(item + 4)).c_str());
                    out += buf;
                }
                chunk = *reinterpret_cast<const unsigned char* const*>(chunk);
            } while (chunk != first);
        }
        out += "\n";
    }
}

static void DumpMaterial(std::string& out, const void* material)
{
    char buf[64];
    if (!material) {
        out += "  material null\n";
        return;
    }
    const auto* m = static_cast<const unsigned char*>(material);
    std::snprintf(buf, sizeof buf, "  material %s:", Name(material));
    out += buf;
    for (unsigned i = 0x34; i < 0x9c; i += 4) {
        std::snprintf(buf, sizeof buf, " %08x", *reinterpret_cast<const unsigned*>(m + i));
        out += buf;
    }
    out += "\n";
}

// a MaterialResource's identity fields: its own vtable (each side writes its own), the spec pointer and the
// MaterialConfig subobject's vtable (both sides write the original's shared one)
static void* resVtable[2];
static void* cfgVtable;

static void DumpResource(std::string& out, const void* res, int s, const char* tag)
{
    const auto* m = static_cast<const unsigned char*>(res);
    char buf[128];
    std::snprintf(buf, sizeof buf, "  %s vf=%s spec=%s cfgv=%s\n", tag,
                  *reinterpret_cast<void* const*>(m) == resVtable[s] ? "self" : "OTHER",
                  *reinterpret_cast<void* const*>(m + 0x5c) ? "set" : "null",
                  *reinterpret_cast<void* const*>(m + 0x60) == cfgVtable ? "mc" : "OTHER");
    out += buf;
}

// ---------------------------------------------------------------------------------------------------------------
// normalized dumps for the lifetime/data scenarios. Each side's vptrs point at that side's table (a different
// image base), so code pointers become self/orig labels; shared nodes and Alloc::New blocks keep their labels.

static unsigned ImageSize(HMODULE m)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return nt->OptionalHeader.SizeOfImage;
}
static bool InImage(const void* p, HMODULE m)
{
    const auto* b = reinterpret_cast<const unsigned char*>(m);
    const void* const lo = b;
    const void* const hi = b + ImageSize(m);
    return b && p >= lo && p < hi;
}
static std::string Label(const void* p, int s)
{
    char buf[32];
    if (!p) return "0";
    auto f = fixedNames.find(p);
    if (f != fixedNames.end()) return f->second;
    for (unsigned i = 0; i < nameCount[s]; ++i)
        if (names[s][i] == p) {
            std::snprintf(buf, sizeof buf, "A%u", i);
            return buf;
        }
    if (const char* n = NodeLabel(p, s)) return n;
    if (dataMode && (InImage(p, mod[0]) || InImage(p, mod[1]) || InImage(p, ks))) return "code";
    if (InImage(p, mod[s])) return "self";
    if (InImage(p, mod[1 - s])) return "orig";
    if (InImage(p, ks)) return "ks";
    std::snprintf(buf, sizeof buf, "%08x", static_cast<unsigned>(reinterpret_cast<ULONG_PTR>(p)));
    return buf;
}
static void DumpFields(const void* obj, unsigned from, unsigned to, int s, const char* tag)
{
    if (!obj) {
        Log("  %s null\n", tag);
        return;
    }
    const auto* b = static_cast<const unsigned char*>(obj);
    Log("  %s", tag);
    for (unsigned i = from; i <= to; i += 4) Log(" %x:%s", i, Label(*reinterpret_cast<void* const*>(b + i), s).c_str());
    Log("\n");
}
static void DumpVtable(const void* vt, int s, const char* tag, unsigned slots)
{
    if (!vt) {
        Log("  %s null\n", tag);
        return;
    }
    Log("  %s", tag);
    const auto* words = static_cast<const void* const*>(vt);
    for (unsigned i = 0; i < slots; ++i) Log(" %s", Label(words[i], s).c_str());
    Log("\n");
}
static void* Scratch(unsigned size)
{
    void* p = RealNew(size);
    std::memset(p, 0xcc, size);
    return p;
}

// ---------------------------------------------------------------------------------------------------------------
// scenarios (each fills the log on the current side)

static void* configRoot;
static char tempDir[256];

// the per-round script parameters both sides see
static bool pick2Full;

// `textures` < 0: the old random count; else exactly that many definitions (LoadTxt's walk crosses into a
// second ring chunk above seven)
static void* BuildSpecConfig(void* parent, std::mt19937& r, bool full, bool rename, bool oddKeys, int textures)
{
    void* c = AddBlock(parent, "Material", "name.m.onetex");
    if (full) {
        AddPair(c, "Ambient", "1 0 0 1");
        AddPair(c, "Diffuse", "0.5 0.5 0.5 1");
        if (oddKeys) AddPair(c, "Specilar", "0 1 0 1");
        AddPair(c, "Emissive", "0 0 0 1");
        AddPair(c, "Shininess", "12.5");
        AddPair(c, "Opacity", "0.75");
        AddPair(c, "TwoSided", "1");
    }
    if (rename) AddPair(c, "Rename", "renamed.m.gloss");
    if (r() % 2) AddPair(c, "SrcBlend", r() % 2 ? "srcAlpha" : "one");
    if (r() % 2) AddPair(c, "DstBlend", "zero");
    if (full || r() % 2) {
        void* p = AddBlock(c, "Properties", nullptr);
        const int n = static_cast<int>(r() % 4);
        for (int i = 0; i < n; ++i) AddPair(p, i % 2 ? "Amount" : "Name", "prop");
        AddPair(p, "Name", "value");
    }
    const int count = textures < 0 ? (full ? 3 : static_cast<int>(r() % 3)) : textures;
    static const char* const kinds[] = {"ambient", "diffuse", "Reflect", "lookup3", "nosuch", "opacity"};
    static const char* const fileKeys[] = {"Bitmap", "Name", "Bitmap"};
    static const char* const fileNames[] = {"tex.bmp", "other.dds", "..\\parent.tga", "sub\\dir\\c.dds",
                                            "C:/abs/x.dds", ""};
    for (int i = 0; i < count; ++i) {
        void* t = AddBlock(c, "Texture", kinds[r() % 6]);
        if (r() % 3) AddPair(t, fileKeys[i % 3], fileNames[r() % 6]);
        if (r() % 2) AddPair(t, "Name", "other.dds");
        if (r() % 2) AddPair(t, "Amount", "0.5");
    }
    return c;
}

// a "version" line and the tree text on disk, for MaterialResource::LoadTxt to parse
static void WriteMaterialFile(std::mt19937& r, const char* path, float version, bool haveMaterial)
{
    if (haveMaterial) {
        BuildSpecConfig(configRoot, r, r() % 2 != 0, r() % 2 != 0, r() % 2 != 0, static_cast<int>(r() % 12));
    } else {
        AddPair(configRoot, "SomethingElse", "1");
    }
    void* parser = RealNew(0x18);
    std::memset(parser, 0, 0x18);
    parserCtor(parser);
    parserSetConfig(parser, configRoot);
    parserSetVersion(parser, version);
    Log("  file version=%d material=%d\n", static_cast<int>(version), haveMaterial ? 1 : 0);
    parserSave(parser, nullptr, path, false, 0);
    parserSetConfig(parser, nullptr);  // the tree stays the scenario's; the parser dtor must not delete it
    parserDtor(parser, nullptr);
    RealDelete(parser);
}

// a GeometryChunkTemplate's written fields: its own vtable (inside the module's image), the counts and the
// blocks (refs and count only: the item size and the capacity are heap garbage in both modules)
static void DumpChunk(std::string& out, const void* c, int s, const char* tag)
{
    char buf[256];
    if (!c) {
        std::snprintf(buf, sizeof buf, "  %s null\n", tag);
        out += buf;
        return;
    }
    const auto* m = static_cast<const unsigned char*>(c);
    const auto* const vt = reinterpret_cast<const unsigned char*>(*reinterpret_cast<void* const*>(m));
    const auto* const base = reinterpret_cast<const unsigned char*>(mod[s]);
    const bool self = vt >= base && vt < base + 0x200000;
    std::snprintf(buf, sizeof buf, "  %s vf=%s flags=%08x prims=%u idx=%u vtx=%u max=%u faces=%u op=%08x\n", tag,
                  self ? "self" : "OTHER", *reinterpret_cast<const unsigned*>(m + 0x18),
                  *reinterpret_cast<const unsigned*>(m + 0x1c), *reinterpret_cast<const unsigned*>(m + 0x20),
                  *reinterpret_cast<const unsigned*>(m + 0x24), *reinterpret_cast<const unsigned*>(m + 0x28),
                  *reinterpret_cast<const unsigned*>(m + 0x38), *reinterpret_cast<const unsigned*>(m + 0x3c));
    out += buf;
    std::snprintf(buf, sizeof buf, "  fields vb=%s ib=%s vt=%s vp=%s\n", Name(*reinterpret_cast<void* const*>(m + 0x4c)),
                  Name(*reinterpret_cast<void* const*>(m + 0x50)), Name(*reinterpret_cast<void* const*>(m + 0x54)),
                  Name(*reinterpret_cast<void* const*>(m + 0x58)));
    out += buf;
    static const unsigned blocks[] = {0x2c, 0x30, 0x34, 0x5c};
    for (unsigned off : blocks) {
        auto* const b = *reinterpret_cast<void* const*>(m + off);
        if (!b) {
            std::snprintf(buf, sizeof buf, "  block %x null\n", off);
        } else {
            std::snprintf(buf, sizeof buf, "  block %x %s refs=%d count=%u\n", off, Near(b).c_str(),
                          *reinterpret_cast<const int*>(b), *reinterpret_cast<const unsigned*>(
                                                               static_cast<const unsigned char*>(b) + 4));
        }
        out += buf;
    }
}

static void RunScenarioCase(int s, std::mt19937& r, unsigned kind);

static void RunScenario(int s, std::mt19937& r)
{
    unsigned kind = r() % 20;
    if (const char* only = std::getenv("RS_U7_ONLY")) kind = static_cast<unsigned>(std::atoi(only));
    quietNodes = kind >= 10;
    RunScenarioCase(s, r, kind);
}
static void RunScenarioCase(int s, std::mt19937& r, unsigned kind)
{
    switch (kind) {
    case 0: {  // the split
        static const char* const names[] = {
            "roofm.onetex", "a.b.c", ".x", "x.", "nodot", "empty.", "m.onetex", "00000007%%tailm.gloss",
            "many.dots.here.m.reflect", "M.UPPER.M.CASE", "trail.", "..", "tab\tm.onetex", "long.m.notex"};
        const char* name = names[r() % (sizeof names / sizeof names[0])] + (r() % 5 == 0 ? 1 : 0);
        void* in = MakePString(name);
        void* type = r() % 3 ? MakePString("oldtype") : MakePString(nullptr);
        void* plain = r() % 3 ? MakePString("oldplain") : MakePString(nullptr);
        int result = 0;
        api[s].split(&result, in, type, plain);
        Log("  split '%s' -> %d type=%s plain=%s\n", name, result, Text(*static_cast<void* const*>(type)).c_str(),
            Text(*static_cast<void* const*>(plain)).c_str());
        FreePString(plain);
        FreePString(type);
        FreePString(in);
        break;
    }
    case 1: {  // Create on a fresh manager (and one with the name already in its table)
        void* manager = RealNew(0x20);
        std::memset(manager, 0, 0x20);
        api[s].managerCtor(manager, textureManagerObj);
        static const char* const names[] = {
            "plain.m.onetex", "nodot", "unknown.m.type", "sup.m.onetex", "unsup.m.onetex", "fall.m.onetex",
            "nullinst.m.onetex", "samefall.m.onetex", "twoskips.m.gloss", "case.m.CASE"};
        const char* name = names[r() % (sizeof names / sizeof names[0])];
        void* in = MakePString(name);
        void* material = reinterpret_cast<void*>(r() % 2 ? 0xcdcdcdcd : 0);
        if (r() % 3 == 0) {
            // a material of the same name is already in the table
            void* known = NextMaterial("KNOWN");
            void* node = RealCreateNode(*cache, reinterpret_cast<const unsigned char*>(name));
            *reinterpret_cast<void**>(static_cast<unsigned char*>(known) + 0x24) = node;
            api[s].managerAdd(manager, known);
            Log("  seeded\n");
        }
        int result = 0;
        api[s].create(manager, &result, &material, in);
        Log("  create '%s' -> %d material=%s\n", name, result, material ? Name(material) : "null");
        FreePString(in);
        api[s].managerDtor(manager, nullptr);
        RealDelete(manager);
        break;
    }
    case 2:
    case 3: {  // the spec extractors over a config tree
        // BuildSpecConfig hands back the "Material" item (what the single-item extractors take); the list
        // extractors walk the root's "Material" children. The tree's nodes come from KernelStandard's ConfigData
        // pool, so only the root is destroyed (its destructor deletes the children); the item is not freed here.
        void* config = BuildSpecConfig(configRoot, r, pick2Full, r() % 2 != 0, r() % 2 != 0, -1);
        if (r() % 4 == 0) {
            void* list = RealNew(4);
            *static_cast<void**>(list) = nullptr;
            if (r() % 2) {
                api[s].extractSpecs(nullptr, list, configRoot);
                Log("  extractSpecs\n");
                auto* head = *static_cast<unsigned char**>(list);
                if (head) {
                    auto* chunk = head;
                    const auto* first = chunk;
                    int index = 0;
                    do {
                        const unsigned count = *reinterpret_cast<unsigned*>(chunk + 8);
                        for (unsigned i = 0; i < count; ++i) {
                            unsigned char* spec = *reinterpret_cast<unsigned char**>(chunk + 12 + 4 * i);
                            std::string dump;
                            DumpSpec(dump, spec);
                            Log("  [%d]%s", index++, dump.c_str() + 2);
                            api[s].specDtor(spec, nullptr);
                            RealDelete(spec);
                        }
                        chunk = *reinterpret_cast<unsigned char**>(chunk);
                    } while (chunk != first);
                    // the list's chunks go (the same on both sides)
                    chunk = head;
                    do {
                        auto* next = *reinterpret_cast<unsigned char**>(chunk);
                        RealDelete(chunk);
                        chunk = next;
                    } while (chunk != first);
                }
            } else {
                managerMaterial = NextMaterial("NEW");
                managerCode = 0;
                api[s].extractMaterials(nullptr, list, managerObj, dbObj, configRoot);
                Log("  extractMaterials\n");
                DumpMaterial(logText[cur], managerMaterial);
                auto* head = *static_cast<unsigned char**>(list);
                if (head) {
                    auto* chunk = head;
                    const auto* first = chunk;
                    do {
                        auto* next = *reinterpret_cast<unsigned char**>(chunk);
                        RealDelete(chunk);
                        chunk = next;
                    } while (chunk != first);
                }
            }
            RealDelete(list);
        } else {
            unsigned char* spec = static_cast<unsigned char*>(api[s].extractSpec(nullptr, config));
            std::string dump;
            DumpSpec(dump, spec);
            Log("%s", dump.c_str());
            if (spec) {
                api[s].specDtor(spec, nullptr);
                RealDelete(spec);
            }
        }
        break;
    }
    case 4: {  // GetMaterial through the stand-in manager
        // the same single "Material" item; only the root is destroyed
        void* config = BuildSpecConfig(configRoot, r, r() % 2 != 0, true, r() % 2 != 0, -1);
        unsigned char* spec = static_cast<unsigned char*>(api[s].extractSpec(nullptr, config));
        managerMaterial = NextMaterial("NEW");
        managerCode = r() % 5 == 0 ? 2 : 0;
        Log("  getMaterial code=%d\n", managerCode);
        void* material = api[s].getMaterial(nullptr, managerObj, dbObj, spec);
        DumpMaterial(logText[cur], material);
        if (spec) {
            api[s].specDtor(spec, nullptr);
            RealDelete(spec);
        }
        break;
    }
    case 5: {  // CreateNewMaterial over the stand-in manager
        void* base = NextMaterial("BASE");
        // the base's state: two properties and two definitions in its lists
        static unsigned char lists[0x200];
        std::memset(lists, 0, sizeof lists);
        auto* propChunk = lists;
        *reinterpret_cast<void**>(propChunk) = propChunk;
        *reinterpret_cast<void**>(propChunk + 4) = propChunk;
        *reinterpret_cast<unsigned*>(propChunk + 8) = 2;
        for (int i = 0; i < 2; ++i) {
            void* k = RealCreateNode(*cache, reinterpret_cast<const unsigned char*>(i ? "key2" : "key1"));
            void* v = RealCreateNode(*cache, reinterpret_cast<const unsigned char*>(i ? "val2" : "val1"));
            *reinterpret_cast<void**>(propChunk + 12 + 8 * i) = k;
            *reinterpret_cast<void**>(propChunk + 16 + 8 * i) = v;
        }
        auto* texChunk = lists + 0x80;
        *reinterpret_cast<void**>(texChunk) = texChunk;
        *reinterpret_cast<void**>(texChunk + 4) = texChunk;
        *reinterpret_cast<unsigned*>(texChunk + 8) = 2;
        for (int i = 0; i < 2; ++i) {
            void* n = RealCreateNode(*cache, reinterpret_cast<const unsigned char*>(i ? "tex2" : "tex1"));
            *reinterpret_cast<int*>(texChunk + 12 + 12 * i) = i ? 1 : 0;
            *reinterpret_cast<void**>(texChunk + 16 + 12 * i) = n;
            *reinterpret_cast<float*>(texChunk + 20 + 12 * i) = i ? 0.25f : 1.0f;
        }
        auto* b = static_cast<unsigned char*>(base);
        *reinterpret_cast<void**>(b + 0x88) = propChunk;
        *reinterpret_cast<void**>(b + 0x90) = texChunk;
        *reinterpret_cast<unsigned*>(b + 0x8c) = 2;
        *reinterpret_cast<unsigned*>(b + 0x78) = r() % 2;
        *reinterpret_cast<unsigned*>(b + 0x7c) = r() % 2;
        managerMaterial = NextMaterial("NEW");
        managerCode = r() % 6 == 0 ? 3 : 0;
        void* name = MakePString(r() % 2 ? "clonem.onetex" : "x");
        Log("  createNew code=%d\n", managerCode);
        void* material = api[s].createNew(nullptr, managerObj, base, name);
        DumpMaterial(logText[cur], material);
        FreePString(name);
        break;
    }
    case 6: {  // MaterialResource: ctor, LoadTxt, CreateInstance and the dtor
        char path[512];
        std::snprintf(path, sizeof path, "%srs_u7_material.txt", tempDir);
        const bool withOwner = r() % 2 != 0;
        const bool goodVersion = r() % 5 != 0;
        const bool haveMaterial = r() % 6 != 0;
        WriteMaterialFile(r, path, goodVersion ? 2.0f : 1.0f, haveMaterial);
        char upper[512];
        std::snprintf(upper, sizeof upper, "%s", path);
        _strupr(upper);
        dbMapped = withOwner && r() % 2 != 0 ? upper : nullptr;
        void* res = RealNew(0x64);
        std::memset(res, 0, 0x64);
        void* name = MakePString(r() % 2 ? "mym.onetex" : "converted.m.gloss");
        api[s].resCtor(res, name, withOwner ? dbObj : nullptr);
        DumpResource(logText[cur], res, s, "ctor");
        Log("  loadTxt owner=%d mapped=%d\n", withOwner ? 1 : 0, dbMapped ? 1 : 0);
        int result = -1;
        api[s].resLoadTxt(res, &result, path);
        Log("  loadTxt -> %d\n", result);
        unsigned char* spec = *reinterpret_cast<unsigned char**>(static_cast<unsigned char*>(res) + 0x5c);
        std::string dump;
        DumpSpec(dump, spec);
        Log("%s", dump.c_str());
        if (spec) {
            managerMaterial = NextMaterial("NEW");
            managerCode = r() % 5 == 0 ? 2 : 0;
            Log("  createInstance code=%d\n", managerCode);
            void* material = api[s].resCreateInstance(res, managerObj, dbObj);
            DumpMaterial(logText[cur], material);
            if (r() % 2) {  // a reload: the old spec goes before the new one is extracted
                WriteMaterialFile(r, path, 2.0f, true);
                Log("  reload\n");
                api[s].resLoadTxt(res, &result, path);
                Log("  loadTxt -> %d\n", result);
                spec = *reinterpret_cast<unsigned char**>(static_cast<unsigned char*>(res) + 0x5c);
                dump.clear();
                DumpSpec(dump, spec);
                Log("%s", dump.c_str());
            }
        }
        api[s].resDtor(res, nullptr);
        RealDelete(res);
        FreePString(name);
        dbMapped = nullptr;
        break;
    }
    case 7: {  // MaterialResource copy and assignment: the spec pointer is shared without a reference
        char path[512];
        std::snprintf(path, sizeof path, "%srs_u7_material.txt", tempDir);
        WriteMaterialFile(r, path, 2.0f, true);
        void* res = RealNew(0x64);
        std::memset(res, 0, 0x64);
        void* name = MakePString(r() % 2 ? "copym.onetex" : "a.b.c");
        api[s].resCtor(res, name, r() % 2 ? dbObj : nullptr);
        int result = -1;
        api[s].resLoadTxt(res, &result, path);
        Log("  loadTxt -> %d\n", result);
        void* res2 = RealNew(0x64);
        std::memset(res2, 0, 0x64);
        api[s].resCtorCopy(res2, res);
        DumpResource(logText[cur], res2, s, "copy");
        Log("  shared=%d\n",
            *reinterpret_cast<void**>(static_cast<unsigned char*>(res2) + 0x5c) ==
                    *reinterpret_cast<void**>(static_cast<unsigned char*>(res) + 0x5c)
                ? 1
                : 0);
        api[s].resAssign(res2, res);
        DumpResource(logText[cur], res2, s, "assign");
        // the shallow copy shares the spec with no reference count: only one of the two may free it
        *reinterpret_cast<void**>(static_cast<unsigned char*>(res2) + 0x5c) = nullptr;
        api[s].resDtor(res2, nullptr);
        RealDelete(res2);
        api[s].resDtor(res, nullptr);
        RealDelete(res);
        FreePString(name);
        break;
    }
    case 8: {  // Material::SetLightingState: the program path and the display's light slots
        ResetLightingStandins();
        std::memset(chunkObj, 0, sizeof chunkObj);
        *reinterpret_cast<unsigned*>(chunkObj + 0x18) = r() % 8 == 0 ? 0x20u : (r() & 7);  // the lit bit 0x20
        displayMaxLights = static_cast<int>(r() % 5);
        *reinterpret_cast<int*>(displayObj + 0x50) = displayMaxLights;
        for (unsigned i = 0; i < 4; ++i) {  // the state colour; NaNs quiet through the x87 copy
            unsigned bits = r();
            if (r() % 4 == 0) bits = 0x7f800000u | (r() & 0x3fffff);
            *reinterpret_cast<unsigned*>(stateBlock + 0xa4 + 4 * i) = bits;
        }
        stateBlock[0xa1] = static_cast<unsigned char>(r() & 4);
        const bool flag = (r() % 2) != 0;
        const bool withProgram = (r() % 4) != 0;
        // the no-program path reads the lights array without a null check in both modules
        void* lights = BuildLights(r, withProgram ? static_cast<unsigned>(r() % 3) : 1u + static_cast<unsigned>(r() % 2));
        void* material = NextMaterial("MAT");
        unsigned opacity = r();
        if (r() % 6 == 0) opacity = 0x7f800000u | (r() & 0x7fffff);
        void* transform = r() % 2 ? matrixObj : nullptr;
        Log("  lighting program=%d flag=%d lights=%s max=%d opacity=%08x\n", withProgram ? 1 : 0, flag ? 1 : 0,
            lights ? "ring" : "null", displayMaxLights, opacity);
        api[s].setLighting(material, displayObj, transform, chunkObj, withProgram ? programObj : nullptr, lights,
                           nullptr, *reinterpret_cast<float*>(&opacity), flag ? 1 : 0);
        Log("  state a1=%02x set=%02x%02x%02x%02x\n", stateBlock[0xa1], lightSet[0], lightSet[1], lightSet[2],
            lightSet[3]);
        DumpMaterial(logText[cur], material);
        break;
    }
    case 9: {  // NullMaterial::CreateGeometryChunk and the GeometryChunkTemplate's slots
        ResetLightingStandins();
        displayNoBuffer = r() % 8 == 0;
        managerUploadCode = r() % 8 == 0 ? (r() % 2 ? 8 : 1) : 0;
        const unsigned v = r() % 4, p = r() % 4, i = r() % 3, f = r() % 3;
        const int type = static_cast<int>(r() % 6);
        unsigned char hintBuf[0x24];
        void* hint = nullptr;
        const int hintKind = static_cast<int>(r() % 4);
        unsigned hintSize = 0x10;
        if (hintKind) {
            std::memset(hintBuf, 0, sizeof hintBuf);
            *reinterpret_cast<unsigned*>(hintBuf + 0) = r() % 4 == 0 ? 1u : 2u;  // version 1 sets dynamic data
            *reinterpret_cast<unsigned*>(hintBuf + 4) = 2;
            *reinterpret_cast<unsigned*>(hintBuf + 8) = hintKind >= 2 ? 1u : (r() & 1);
            *reinterpret_cast<unsigned*>(hintBuf + 0xc) = r();
            hintBuf[0x10] = static_cast<unsigned char>(r() & 7);
            hintSize = hintKind == 3 ? 0x24u : hintKind == 2 ? 0x20u : 0x10u;
            hint = hintBuf;
        }
        Log("  chunk %u %u %u %u type=%d hint=%d size=%x\n", v, p, i, f, type, hintKind, hintSize);
        void* chunk = api[s].createChunk(nullptr, v, p, i, f, type, hint);
        DumpChunk(logText[cur], chunk, s, "made");
        if (!chunk) break;
        auto* vt = *reinterpret_cast<void***>(chunk);
        // a pattern in the vertex data (the block's items are heap garbage otherwise)
        void* data = As<void*(__cdecl*)(void*, int, int)>(vt[13])(chunk, 0, 0);
        Log("  vertexData %s\n", Near(data).c_str());
        for (unsigned k = 0; k < v * 0x20; ++k) static_cast<unsigned char*>(data)[k] = static_cast<unsigned char>(k * 7 + v);
        // every item offset and view
        Log("  offsets");
        for (int t = 0; t < 39; ++t) Log(" %d", As<int(__cdecl*)(void*, int, int)>(vt[44])(chunk, t, 0));
        Log("\n");
        static const int viewSlots[] = {46, 47, 48, 49, 50, 51, 52, 53, 54, 55};
        for (int t = 0; t < 39; t += 7) {
            Log("  view t=%d", t);
            for (int s2 = 0; s2 < 10; ++s2) {
                unsigned out[2] = {9, 9};
                As<void*(__cdecl*)(void*, void*, int, int)>(vt[viewSlots[s2]])(chunk, out, t, 0);
                Log(" %u %s", out[0], out[1] == 9 ? "same" : Near(reinterpret_cast<void*>(out[1])).c_str());
            }
            Log("\n");
        }
        Log("  typeId %d size %d\n", As<int(__cdecl*)(void*, int)>(vt[42])(chunk, 0),
            As<int(__cdecl*)(void*, int)>(vt[43])(chunk, 0));
        // the buffer cycle, sometimes with the dynamic bits set and the uploads failing
        if (r() % 2) *reinterpret_cast<unsigned*>(static_cast<unsigned char*>(chunk) + 0x18) |= r() % 2 ? 0x10000u : 0x100u;
        const bool upload = (r() % 2) != 0;
        Log("  initBuffers %d\n", As<int(__cdecl*)(void*, void*, int)>(vt[4])(chunk, displayObj, upload ? 1 : 0) & 0xff);
        DumpChunk(logText[cur], chunk, s, "inited");
        Log("  loadBuffers %d\n", As<int(__cdecl*)(void*, void*)>(vt[5])(chunk, displayObj) & 0xff);
        As<void(__cdecl*)(void*, void*, int, int)>(vt[7])(chunk, displayObj, 0, 0);
        DumpChunk(logText[cur], chunk, s, "duped");
        As<void(__cdecl*)(void*, void*, unsigned, int, int)>(vt[9])(chunk, displayObj, v + 1, 0, 0);
        DumpChunk(logText[cur], chunk, s, "resized");
        As<void(__cdecl*)(void*, void*)>(vt[8])(chunk, displayObj);
        DumpChunk(logText[cur], chunk, s, "dupIndex");
        void* copy = As<void*(__cdecl*)(void*)>(vt[3])(chunk);
        DumpChunk(logText[cur], copy, s, "copy");
        if (copy) As<void(__fastcall*)(void*, void*, unsigned)>(vt[1])(copy, nullptr, 1);
        void* copy2 = As<void*(__cdecl*)(void*, unsigned, unsigned, unsigned, unsigned)>(vt[2])(chunk, 1, 2, 1, 1);
        DumpChunk(logText[cur], copy2, s, "copy2");
        if (copy2) As<void(__fastcall*)(void*, void*, unsigned)>(vt[1])(copy2, nullptr, 1);
        As<void(__fastcall*)(void*, void*, unsigned)>(vt[1])(chunk, nullptr, 1);
        break;
    }
    case 10: {  // PluginFactoryBase<VMaterial>: 3-arg ctor, copy ctor, shared assignment and dtor
        void* src = Scratch(0x60);
        void* dst = Scratch(0x60);
        void* asg = Scratch(0x60);
        void* third = Scratch(0x40);  // the ctor's third (reference) argument, zeroed stand-in
        std::memset(third, 0, 0x40);
        void* name = MakePString(r() % 2 ? "factory.one" : "factory.two");
        void* uid = MakeUid(r() % 2 ? "onetex" : "gloss");
        Log("  pfb ctor3 name=%s uid=%s\n", Text(*static_cast<void* const*>(name)).c_str(), HexUid(uid).c_str());
        api[s].pfbCtor3(src, nullptr, name, uid, third);
        Log("  pfb ctor vptr=%s own=%d\n", Label(*static_cast<void* const*>(src), s).c_str(),
            *reinterpret_cast<void**>(src) == api[s].pfbVftable ? 1 : 0);
        DumpFields(src, 0, 0x5c, s, "pfb src");
        api[s].pfbCopy(dst, nullptr, src);
        Log("  pfb copy vptr=%s own=%d\n", Label(*static_cast<void* const*>(dst), s).c_str(),
            *reinterpret_cast<void**>(dst) == api[s].pfbVftable ? 1 : 0);
        DumpFields(dst, 0, 0x5c, s, "pfb copy");
        api[s].pfbCtor3(asg, nullptr, name, uid, third);
        api[s].pfbAssign(asg, nullptr, src);
        DumpFields(asg, 0, 0x5c, s, "pfb assign");
        // the three objects share one name node; destroy only two of them here and use an independent
        // instance for the third so the string cache never sees a double release
        api[s].pfbDtor(asg, nullptr);
        api[s].pfbDtor(dst, nullptr);
        void* solo = Scratch(0x60);
        void* soloName = MakePString("factory.solo");
        void* soloUid = MakeUid("gloss");
        api[s].pfbCtor3(solo, nullptr, soloName, soloUid, third);
        api[s].pfbDtor(solo, nullptr);
        Log("  pfb dtors done\n");
        FreePString(soloName);
        RealDelete(soloUid);
        RealDelete(solo);
        FreePString(name);
        RealDelete(uid);
        RealDelete(src);
        RealDelete(dst);
        RealDelete(asg);
        RealDelete(third);
        break;
    }
    case 11: {  // MaterialPlugin: copy / 3-arg ctor, and the factory's dtor/assign as shared aliases
        void* src = Scratch(0x80);
        void* dst = Scratch(0x80);
        void* asg = Scratch(0x80);
        void* name = MakePString(r() % 2 ? "plugin.gloss" : "plugin.reflect");
        void* uid = MakeUid(r() % 2 ? "gloss" : "reflect");
        // the original has no hidden virtual-base flag here: __thiscall(this, name, id), so the harness passes
        // only the dummy EDX argument and the two real stack arguments
        api[s].mpCtor3(src, nullptr, name, uid);
        Log("  mp ctor3 vptr=%s own=%d\n", Label(*static_cast<void* const*>(src), s).c_str(),
            *reinterpret_cast<void**>(src) == api[s].mpVftable ? 1 : 0);
        DumpFields(src, 0, 0x7c, s, "mp src");
        api[s].mpCopy(dst, nullptr, src);
        Log("  mp copy vptr=%s own=%d\n", Label(*static_cast<void* const*>(dst), s).c_str(),
            *reinterpret_cast<void**>(dst) == api[s].mpVftable ? 1 : 0);
        DumpFields(dst, 0, 0x7c, s, "mp copy");
        api[s].mpCtor3(asg, nullptr, name, uid);
        api[s].pfbAssign(asg, nullptr, src);  // the base assignment is the alias
        DumpFields(asg, 0, 0x7c, s, "mp assign");
        // src/dst/asg share one name node; two destructors plus one independent instance keep the cache balanced
        api[s].pfbDtor(asg, nullptr);
        api[s].pfbDtor(dst, nullptr);
        void* solo = Scratch(0x80);
        void* soloName = MakePString("plugin.solo");
        void* soloUid = MakeUid("gloss");
        api[s].mpCtor3(solo, nullptr, soloName, soloUid);
        api[s].pfbDtor(solo, nullptr);
        FreePString(soloName);
        RealDelete(soloUid);
        RealDelete(solo);
        FreePString(name);
        RealDelete(uid);
        RealDelete(src);
        RealDelete(dst);
        RealDelete(asg);
        break;
    }
    case 12: {  // Material ctor with manager=0 / managerObj, fields, vtable/vbtable/vtordisp, dtor variants
        for (int mgrKind = 0; mgrKind < 2; ++mgrKind) {
            for (int ending = 0; ending < 2; ++ending) {
                void* obj = Scratch(0xc0);
                *reinterpret_cast<int*>(static_cast<unsigned char*>(managerObj) + 4) = 5;
                Log("  mat ctor mgr=%d end=%d\n", mgrKind, ending);
                api[s].matCtor(obj, nullptr, mgrKind ? managerObj : nullptr, reinterpret_cast<void*>(1));
                DumpFields(obj, 0, 0x98, s, "mat");
                const auto* b = static_cast<const unsigned char*>(obj);
                Log("  mat hdr vf=%s vb=%s vdisp=%s\n", Label(*reinterpret_cast<void* const*>(b), s).c_str(),
                    Label(*reinterpret_cast<void* const*>(b + 4), s).c_str(),
                    Label(*reinterpret_cast<void* const*>(b + 8), s).c_str());
                Log("  mat own vf=%d vb=%d mgrrefs=%d\n",
                    *reinterpret_cast<void**>(obj) == api[s].matVftable ? 1 : 0,
                    *reinterpret_cast<void**>(static_cast<unsigned char*>(obj) + 4) == api[s].matVbtable ? 1 : 0,
                    *reinterpret_cast<int*>(static_cast<unsigned char*>(managerObj) + 4));
                if (!ending) {
                    // the exported Material virtual dtor receives the virtual-base-adjusted pointer Material+0xa0
                    api[s].matDtor(static_cast<unsigned char*>(obj) + 0xa0, nullptr);
                    DumpFields(obj, 0, 0x98, s, "mat dtor");
                } else {
                    api[s].matVbaseDtor(obj, nullptr);
                    DumpFields(obj, 0, 0x8, s, "mat vbase");
                }
                Log("  mgrrefs after=%d\n", *reinterpret_cast<int*>(static_cast<unsigned char*>(managerObj) + 4));
                RealDelete(obj);
            }
        }
        break;
    }
    case 13: {  // NullMaterial ctor / dtor / vbase dtor over one shared name node
        void* name = MakePString(r() % 2 ? "nullmat.a" : "nullmat.b");
        const unsigned m0 = nodeMade[cur], k0 = nodeKilled[cur];
        void* node = *static_cast<void* const*>(name);
        // the cache is shared and side 0 ran first: log only the changes this scenario makes
        const int r0 = NodeRefs(node), l0 = CacheLiveRefs();
        Log("  null start\n");
        void* obj = Scratch(0xc0);  // NullMaterial has a 0xa0-byte Material plus the 0xac-byte virtual base
        Log("  null ctor name=%s\n", Text(node).c_str());
        api[s].nullCtor(obj, nullptr, name, reinterpret_cast<void*>(1));
        DumpFields(obj, 0, 0x5c, s, "null");
        Log("  null own vf=%d vptr=%s nodes +%u/-%u refs%+d live%+d\n",
            *reinterpret_cast<void**>(obj) == api[s].nullVftable ? 1 : 0,
            Label(*reinterpret_cast<void* const*>(obj), s).c_str(), nodeMade[cur] - m0, nodeKilled[cur] - k0,
            NodeRefs(node) - r0, CacheLiveRefs() - l0);
        api[s].nullDtor(static_cast<unsigned char*>(obj) + 0xa4, nullptr);
        DumpFields(obj, 0, 0x5c, s, "null dtor");
        Log("  nodes +%u/-%u refs%+d live%+d\n", nodeMade[cur] - m0, nodeKilled[cur] - k0, NodeRefs(node) - r0,
            CacheLiveRefs() - l0);
        void* obj2 = Scratch(0xc0);
        api[s].nullCtor(obj2, nullptr, name, reinterpret_cast<void*>(1));
        api[s].nullVbaseDtor(obj2, nullptr);
        DumpFields(obj2, 0, 0x8, s, "null vbase");
        Log("  nodes +%u/-%u refs%+d live%+d\n", nodeMade[cur] - m0, nodeKilled[cur] - k0, NodeRefs(node) - r0,
            CacheLiveRefs() - l0);
        FreePString(name);
        RealDelete(obj);
        RealDelete(obj2);
        break;
    }
    case 14: {  // the static Material::CreateInstance(const UId&, MaterialManager*) through the UId hook
        static const char* const tags[] = {"onetex", "gloss", "reflect", "oneten", "CASE", "twoskips"};
        const char* tag = tags[r() % (sizeof tags / sizeof tags[0])];
        void* uid = MakeUid(tag);
        void* mgr = managerObj;  // the original dereferences it unconditionally for the kernel pointer
        *reinterpret_cast<int*>(static_cast<unsigned char*>(managerObj) + 4) = 5;
        uidHookEnabled = true;
        Log("  createUid %s mgr=%d\n", tag, mgr ? 1 : 0);
        void* mat = api[s].matCreateUid(uid, mgr);
        uidHookEnabled = false;
        Log("  createUid -> %s\n", Label(mat, s).c_str());
        DumpFields(mat, 0, 0x98, s, "uid mat");
        Log("  mgrrefs=%d\n", *reinterpret_cast<int*>(static_cast<unsigned char*>(managerObj) + 4));
        RealDelete(uid);
        break;
    }
    case 15: {  // the static Material::CreateInstance(const PString&, MaterialManager*) through the name hook
        static const char* const names[] = {"onetex", "gloss", "reflect", "oneten", "CASE", "twoskips"};
        const char* name = names[r() % (sizeof names / sizeof names[0])];
        void* in = MakePString(name);
        void* mgr = managerObj;  // the original dereferences it unconditionally for the kernel pointer
        *reinterpret_cast<int*>(static_cast<unsigned char*>(managerObj) + 4) = 5;
        Log("  createName %s mgr=%d\n", name, mgr ? 1 : 0);
        void* mat = api[s].matCreateName(in, mgr);
        Log("  createName -> %s\n", Label(mat, s).c_str());
        DumpFields(mat, 0, 0x98, s, "name mat");
        Log("  mgrrefs=%d\n", *reinterpret_cast<int*>(static_cast<unsigned char*>(managerObj) + 4));
        FreePString(in);
        break;
    }
    case 16: {  // Material::operator< over scripted slot-0x54 keys (check the comparison inverts)
        static const char* const keys[] = {"aa", "bb", "aa", "cc"};
        const char* ka = keys[r() % (sizeof keys / sizeof keys[0])];
        const char* kb = keys[r() % (sizeof keys / sizeof keys[0])];
        void* pa = MakePString(ka);
        void* pb = MakePString(kb);
        void* a = Scratch(0xc0);
        void* b = Scratch(0xc0);
        SetupKeyed(a, pa);
        SetupKeyed(b, pb);
        Log("  less keys '%s' '%s'\n", ka, kb);
        const bool ab = api[s].matLess(a, b);
        const bool ba = api[s].matLess(b, a);
        Log("  less %d %d inv=%d\n", ab ? 1 : 0, ba ? 1 : 0, ab != ba ? 1 : 0);
        FreePString(pb);
        FreePString(pa);
        RealDelete(a);
        RealDelete(b);
        break;
    }
    case 17: {  // NullMaterial::CreateInstance and operator== (same-side Null vftable vs a generic material)
        void* name = MakePString("nullequal");
        void* a = Scratch(0xc0);
        void* b = Scratch(0xc0);
        api[s].nullCtor(a, nullptr, name, reinterpret_cast<void*>(1));
        api[s].nullCtor(b, nullptr, name, reinterpret_cast<void*>(1));
        Log("  null vt a=%d b=%d\n", *reinterpret_cast<void**>(a) == api[s].nullVftable ? 1 : 0,
            *reinterpret_cast<void**>(b) == api[s].nullVftable ? 1 : 0);
        void* created = api[s].nullCreate(a);
        Log("  created %s\n", Label(created, s).c_str());
        // +0x14..+0x1c and the high bits of +0x84 are left uninitialised by Alloc::New; +0x24 holds a name
        // node created inside KernelStandard (outside the patched imports). Compare only the deterministic state.
        DumpFields(created, 0, 0x10, s, "created");
        DumpFields(created, 0x20, 0x20, s, "created");
        DumpFields(created, 0x28, 0x80, s, "created");
        DumpFields(created, 0x88, 0x98, s, "created");
        Log("  created 84=%02x\n",
            *reinterpret_cast<const unsigned char*>(static_cast<const unsigned char*>(created) + 0x84) & 0x0f);
        const bool same = api[s].nullEqual(a, b);
        // the original's operator== runs __RTDynamicCast, so the "other" object must carry a real RTTI-bearing
        // vftable: a test stand-in would make the runtime walk a bogus type_info pointer
        void* generic = Scratch(0xc0);
        *reinterpret_cast<int*>(static_cast<unsigned char*>(managerObj) + 4) = 5;
        api[s].matCtor(generic, nullptr, managerObj, reinterpret_cast<void*>(1));
        const bool diff = api[s].nullEqual(a, generic);
        Log("  eq same=%d generic=%d\n", same ? 1 : 0, diff ? 1 : 0);
        api[s].matDtor(static_cast<unsigned char*>(generic) + 0xa0, nullptr);
        RealDelete(generic);
        api[s].nullDtor(static_cast<unsigned char*>(b) + 0xa4, nullptr);
        api[s].nullDtor(static_cast<unsigned char*>(a) + 0xa4, nullptr);
        FreePString(name);
        RealDelete(a);
        RealDelete(b);
        break;
    }
    case 18: {  // the data exports: statics, the 4-byte results words and the vtable/vbtable mirrors
        dataMode = true;
        Log("  data unique=%s\n", *static_cast<const unsigned*>(api[s].uniqueMatId) ? "set" : "zero");
        Log("  results mat=%s mgr=%s\n", Label(*static_cast<void* const*>(api[s].matResults), s).c_str(),
            Label(*static_cast<void* const*>(api[s].mgrResults), s).c_str());
        Log("  decorated=%s notFound=%s group=%s\n", Text(*static_cast<void* const*>(api[s].decoratedToken)).c_str(),
            Text(*static_cast<void* const*>(api[s].stringNotFound)).c_str(),
            Text(*static_cast<void* const*>(api[s].pluginGroupName)).c_str());
        Log("  groupUid=%s\n", HexUid(api[s].pluginGroupUid).c_str());
        DumpVtable(api[s].pfbVftable, s, "pfb vt", 3);
        DumpVtable(api[s].mpVftable, s, "mp vt", 6);
        DumpVtable(api[s].matVftable, s, "mat vt", 23);
        DumpVtable(api[s].nullVftable, s, "null vt", 23);
        DumpVtable(api[s].matVbtable, s, "mat vb", 2);
        DumpVtable(api[s].nullVbtable, s, "null vb", 2);
        dataMode = false;
        break;
    }
    case 19: {  // the combined lifetime chain: factory -> plugin -> material + null material, then unwind
        const unsigned m0 = nodeMade[cur], k0 = nodeKilled[cur];
        void* name = MakePString(r() % 2 ? "chain.a" : "chain.b");
        void* uid = MakeUid("onetex");
        void* fac = Scratch(0x60);
        void* plug = Scratch(0x80);
        void* third = Scratch(0x40);
        std::memset(third, 0, 0x40);
        api[s].pfbCtor3(fac, nullptr, name, uid, third);
        // a copy ctor would not register plug with the plugin system, but its destructor unregisters by UId;
        // construct an independent registered MaterialPlugin so each destructor has a matching construction
        api[s].mpCtor3(plug, nullptr, name, uid);
        void* mat = Scratch(0xc0);
        *reinterpret_cast<int*>(static_cast<unsigned char*>(managerObj) + 4) = 5;
        api[s].matCtor(mat, nullptr, managerObj, reinterpret_cast<void*>(1));
        void* null = Scratch(0xc0);
        api[s].nullCtor(null, nullptr, name, reinterpret_cast<void*>(1));
        DumpFields(fac, 0, 0x5c, s, "chain fac");
        DumpFields(plug, 0, 0x7c, s, "chain plug");
        DumpFields(mat, 0, 0x98, s, "chain mat");
        DumpFields(null, 0, 0x5c, s, "chain null");
        Log("  chain own fac=%d plug=%d mat=%d null=%d\n",
            *reinterpret_cast<void**>(fac) == api[s].pfbVftable ? 1 : 0,
            *reinterpret_cast<void**>(plug) == api[s].mpVftable ? 1 : 0,
            *reinterpret_cast<void**>(mat) == api[s].matVftable ? 1 : 0,
            *reinterpret_cast<void**>(null) == api[s].nullVftable ? 1 : 0);
        api[s].nullDtor(static_cast<unsigned char*>(null) + 0xa4, nullptr);
        api[s].matDtor(static_cast<unsigned char*>(mat) + 0xa0, nullptr);
        api[s].pfbDtor(plug, nullptr);
        api[s].pfbDtor(fac, nullptr);
        Log("  chain nodes +%u/-%u mgrrefs=%d\n", nodeMade[cur] - m0, nodeKilled[cur] - k0,
            *reinterpret_cast<int*>(static_cast<unsigned char*>(managerObj) + 4));
        FreePString(name);
        RealDelete(uid);
        RealDelete(fac);
        RealDelete(plug);
        RealDelete(mat);
        RealDelete(null);
        RealDelete(third);
        break;
    }
    default:
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// comparison

static void Compare(int round)
{
    if (logText[0] == logText[1]) return;
    ++bad;
    std::size_t at = 0;
    while (at < logText[0].size() && at < logText[1].size() && logText[0][at] == logText[1][at]) ++at;
    if (const char* dump = std::getenv("RS_U7_DUMP")) {
        for (int s = 0; s < 2; ++s) {
            char path[512];
            std::snprintf(path, sizeof path, "%s.%llu%s.txt", dump, bad, s ? ".orig" : ".ours");
            if (FILE* f = std::fopen(path, "w")) {
                std::fputs(logText[s].c_str(), f);
                std::fclose(f);
            }
        }
    }
    std::printf("MISMATCH round %d at %zu\n-- ours:\n%s\n-- original:\n%s\n", round, at,
                logText[0].substr(at, 4000).c_str(), logText[1].substr(at, 4000).c_str());
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_rs_u7 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 500;
    const std::string binDir = argv[1];
    SetDllDirectoryA(argv[1]);
    LoadLibraryA((binDir + "\\RenderSystem_7c_orig.dll").c_str());
    mod[0] = LoadLibraryA((binDir + "\\RenderSystem_7c.dll").c_str());
    mod[1] = GetModuleHandleA("RenderSystem_7c_orig.dll");
    ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    RealNew = As<NewFn>(Proc(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    RealDelete = As<DeleteFn>(Proc(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    RealCreateNode = As<NodeFn>(Proc(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    RealCreateNodeLen = As<NodeLenFn>(Proc(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBEI@Z"));
    RealDestroyNode = As<NodeDestroyFn>(Proc(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z"));
    cache = static_cast<void**>(Proc(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    configCtor = As<decltype(configCtor)>(Proc(ks, "??0ConfigData@Jet@@QAE@XZ"));
    configChild = As<decltype(configChild)>(Proc(ks, "??RConfigData@Jet@@QAAAAV01@ABVPString@1@0@Z"));
    configBlock = As<decltype(configBlock)>(Proc(ks, "??RConfigData@Jet@@QAAAAV01@ABVPString@1@@Z"));
    configDtor = As<decltype(configDtor)>(Proc(ks, "??1ConfigData@Jet@@QAE@XZ"));
    parserCtor = As<decltype(parserCtor)>(Proc(ks, "??0ConfigParser@Jet@@QAE@XZ"));
    parserDtor = As<decltype(parserDtor)>(Proc(ks, "??1ConfigParser@Jet@@UAE@XZ"));
    parserSetConfig = As<decltype(parserSetConfig)>(Proc(ks, "?SetConfig@ConfigParser@Jet@@QAAXPAVConfigData@2@@Z"));
    parserSetVersion = As<decltype(parserSetVersion)>(Proc(ks, "?SetVersion@ConfigParser@Jet@@QAAXM@Z"));
    parserSave = As<decltype(parserSave)>(Proc(ks, "?Save@ConfigParser@Jet@@QBE_NPBD_NH@Z"));
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        resVtable[s] = Proc(mod[s], "??_7MaterialResource@Jet@@6B@");
    }
    cfgVtable = Proc(mod[1], "??_7MaterialConfig@Jet@@6B@");
    GetTempPathA(sizeof tempDir, tempDir);
    // KernelResource's Resource reads the resource system's time: a zeroed stand-in (as in diff_rs_u3)
    static unsigned char fakeSystem[0x400];
    As<void(__cdecl*)(void*)>(Proc(GetModuleHandleA("KernelResource_7c.dll"),
                                  "?InitRS@Resource@Jet@@CAXPAVResourceSystem@2@@Z"))(fakeSystem);
    // the stand-ins
    *reinterpret_cast<void**>(textureManagerObj) = &MatDeleteThis;
    for (unsigned i = 1; i < 0x20 / 4; ++i) reinterpret_cast<void**>(textureManagerObj)[i] = &MatRefCountAdd;
    std::memset(kernelObj, 0, sizeof kernelObj);
    *reinterpret_cast<void**>(kernelObj + 0x7c) = displayObj;
    std::memset(dbObj, 0, sizeof dbObj);
    dbVtable[3] = &DbGetFileName;
    *reinterpret_cast<void**>(dbObj) = dbVtable;
    managerVtable[3] = &ManagerCreate;
    managerVtable[0x14 / 4] = reinterpret_cast<void*>(&ManagerRemove);  // Material's dtor unregisters here
    managerObj = RealNew(0x20);
    std::memset(managerObj, 0, 0x20);
    *static_cast<void**>(managerObj) = managerVtable;
    fixedNames[managerObj] = "MGR";
    fixedNames[displayObj] = "D";
    fixedNames[textureManagerObj] = "TM";
    fixedNames[dbObj] = "DB";
    // the U7 chunk and lighting stand-ins
    displayVtable[0x90 / 4] = reinterpret_cast<void*>(&DisplayLights);
    displayVtable[0x94 / 4] = reinterpret_cast<void*>(&DisplayEnd);
    displayVtable[0xcc / 4] = reinterpret_cast<void*>(&DisplayMaterialSlot);
    displayVtable[0x104 / 4] = reinterpret_cast<void*>(&DisplayMakeVertex);
    displayVtable[0x108 / 4] = reinterpret_cast<void*>(&DisplayMakeIndex);
    displayVtable[0x10c / 4] = reinterpret_cast<void*>(&DisplayDeclaration);
    displayVtable[0x1f0 / 4] = reinterpret_cast<void*>(&DisplayLightingMode);
    displayVtable[0x224 / 4] = reinterpret_cast<void*>(&DisplayLightSlot);
    displayVtable[0x22c / 4] = reinterpret_cast<void*>(&DisplayOneLight);
    *reinterpret_cast<void***>(displayObj) = displayVtable;
    static void* bufferManagerVtable[16];
    bufferManagerVtable[0x20 / 4] = reinterpret_cast<void*>(&ManagerUpload);
    bufferManagerVtable[0x2c / 4] = reinterpret_cast<void*>(&ManagerUploadIndex);
    std::memset(managerObj2, 0, sizeof managerObj2);
    *reinterpret_cast<void***>(managerObj2) = bufferManagerVtable;
    programVtable[0x44 / 4] = reinterpret_cast<void*>(&ProgramSlot17);
    std::memset(programObj, 0, sizeof programObj);
    *reinterpret_cast<void***>(programObj) = programVtable;
    bufferVtable[0] = reinterpret_cast<void*>(&BufferDeleteThis);
    bufferVtable[1] = reinterpret_cast<void*>(&BufferDeleteThis);
    for (unsigned i = 0; i < 8; ++i) {
        buffers[i].vtable = bufferVtable;
        buffers[i].refs = 1;
        fixedNames[&buffers[i]] = std::string("VB") + static_cast<char>('0' + i);
    }
    vertexType.vtable = bufferVtable;
    vertexType.refs = 1;
    vertexType.id = 18;
    vertexType.streams = 1;
    vertexType.streamList = vertexTypeStream;
    vertexTypeStream[0] = 32;
    fixedNames[&vertexType] = "VT";
    fixedNames[managerObj2] = "BM";
    fixedNames[programObj] = "PRG";
    fixedNames[matrixObj] = "MX";
    fixedNames[chunkObj] = "CHK";
    for (unsigned i = 0; i < 4; ++i) {
        lightPool[i] = &lightChunks[0].items[i];  // just named addresses for the logs
        fixedNames[lightPool[i]] = std::string("L") + static_cast<char>('0' + i);
    }
    ResetLightingStandins();
    for (unsigned i = 0; i < 4; ++i) {
        texturePool[i] = RealNew(0x10);
        std::memset(texturePool[i], 0, 0x10);
        *static_cast<void**>(texturePool[i]) = &MatRefCountAdd;
        *reinterpret_cast<int*>(static_cast<unsigned char*>(texturePool[i]) + 4) = 1;
        fixedNames[texturePool[i]] = std::string("T") + static_cast<char>('0' + i);
    }
    // the scripted plugin types (the hooked LookupPluginType hands them out by name)
    struct Script {
        const char* name;
        bool supported;
        bool fallback;
        const char* fallbackName;
        int resultKind;
    };
    static const Script scripts[] = {
        {"onetex", true, false, "", 0},   {"type", true, false, "", 0},   {"gloss", true, false, "", 1},
        {"reflect", true, false, "", 0},  {"unknown", false, false, "", 0}, {"notex", false, true, "oneten", 0},
        {"oneten", true, false, "", 0},   {"CASE", true, false, "", 2},   {"twoskips", true, false, "", 0},
    };
    for (unsigned i = 0; i < sizeof scripts / sizeof scripts[0]; ++i) {
        auto* p = new Plugin;
        p->tag = scripts[i].name;
        p->supported = scripts[i].supported;
        p->fallback = scripts[i].fallback;
        p->fallbackName = scripts[i].fallbackName;
        p->vfptr = pluginVtable;
        plugins.push_back(p);
    }
    pluginVtable[0] = reinterpret_cast<void*>(&PluginAddRef);
    pluginVtable[1] = reinterpret_cast<void*>(&PluginRelease);
    pluginVtable[2] = reinterpret_cast<void*>(&PluginDelete);
    pluginVtable[3] = reinterpret_cast<void*>(&PluginCreateBare);
    pluginVtable[4] = reinterpret_cast<void*>(&PluginSupported);
    pluginVtable[5] = reinterpret_cast<void*>(&PluginCreate);
    // capture the UId-only LookupPluginType before it is patched, so the hook can delegate outside kinds 14/19
    RealLookupUid = As<LookupUidFn>(ImportFunction(
        mod[1], "?LookupPluginType@PluginBase@Jet@@KA?AVResult@2@PAPAVPluginType@2@ABVUId@2@@Z"));
    for (int s = 0; s < 2; ++s) {
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
        PatchImport(mod[s], "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z", reinterpret_cast<void*>(&HookCreateNode));
        PatchImport(mod[s], "?Create@PStringCache@Jet@@QAAPAUNode@12@PBEI@Z",
                    reinterpret_cast<void*>(&HookCreateNodeLen));
        PatchImport(mod[s], "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z", reinterpret_cast<void*>(&HookDestroyNode));
        PatchImport(mod[s], "?LookupPluginType@PluginBase@Jet@@KA?AVResult@2@PAPAVPluginType@2@ABVUId@2@ABVPString@2@@Z",
                    reinterpret_cast<void*>(&HookLookup));
        PatchImport(mod[s], "?LookupPluginType@PluginBase@Jet@@KA?AVResult@2@PAPAVPluginType@2@ABVUId@2@@Z",
                    reinterpret_cast<void*>(&HookLookupUid));
    }
    for (int round = 0; round < rounds; ++round) {
        std::mt19937 r(round * 2654435761u + 17);
        for (int scenario = 0; scenario < 20; ++scenario) {
            materialUsed = 0;
            pick2Full = (r() % 2) != 0;
            for (unsigned i = 0; i < 16; ++i) texturesByType[i] = (r() % 3) == 0;
            plugins[5]->fallbackName = r() % 2 ? "oneten" : "gloss";
            for (int s = 0; s < 2; ++s) {
                if (const char* only = std::getenv("RS_U7_SIDE")) if (std::atoi(only) != s) continue;
                // the material stand-ins are memset again for each side: the pool is shared memory, so a side
                // must not see the name fields the other side left on a material the same plugin hands out
                materialUsed = 0;
                for (Plugin* p : plugins) p->result = p->tag[0] == 'g' ? nullptr : NextMaterial("PLUGIN");
                cur = s;
                logText[s].clear();
                nameCount[s] = 0;
                configRoot = RealNew(0x10);
                std::memset(configRoot, 0, 0x10);
                configCtor(configRoot);
                std::mt19937 sideR = r;  // both sides replay the same script
                RunScenario(s, sideR);
                configDtor(configRoot, nullptr);
                RealDelete(configRoot);
                cur = -1;
            }
            ++checks;
            Compare(round);
        }
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
