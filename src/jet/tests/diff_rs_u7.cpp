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
    if (echo) std::fputs(buf, stdout);
}

// the node texts are shared (one cache), so they compare directly
static std::string Text(const void* node)
{
    if (!node) return "null";
    return std::string("'") + (static_cast<const char*>(node) + 0x18) + "'";
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
    if (cur >= 0) Log("  node %s -> %s\n", text ? reinterpret_cast<const char*>(text) : "null", Text(n).c_str());
    return n;
}
// the (text, length) overload: the original inlines its literal keys through this one, ours through the 1-arg
// one, so both are hooked and logged alike (the interned node and its order are what the test compares)
static void* __cdecl HookCreateNodeLen(void* self, const unsigned char* text, unsigned length)
{
    void* n = RealCreateNodeLen(self, text, length);
    if (cur >= 0) {
        const std::string s(text ? reinterpret_cast<const char*>(text) : "", text ? length : 0);
        Log("  node %s -> %s\n", text ? s.c_str() : "null", Text(n).c_str());
    }
    return n;
}
static void __cdecl HookDestroyNode(void* self, void* node)
{
    if (cur >= 0) Log("  unnode %s\n", Text(node).c_str());
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

// ---------------------------------------------------------------------------------------------------------------
// PString helpers (its constructors are not exported; the cache makes the nodes)

static void* MakePString(const char* text)
{
    void* p = RealNew(4);
    *static_cast<void**>(p) = RealCreateNode(*cache, reinterpret_cast<const unsigned char*>(text));
    return p;
}
static void FreePString(void* p)
{
    if (*static_cast<void**>(p)) RealDestroyNode(*cache, *static_cast<void**>(p));
    RealDelete(p);
}

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
static void* materialVtable[0x4c / 4];
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
    for (unsigned i = 0; i < 0x4c / 4; ++i) materialVtable[i] = reinterpret_cast<void*>(&MatGetTexture);
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

static unsigned char textureManagerObj[0x20];
static unsigned char kernelObj[0x100];
static unsigned char displayObj[0x40];
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

static void RunScenarioCase(int s, std::mt19937& r, unsigned kind);

static void RunScenario(int s, std::mt19937& r)
{
    const unsigned kind = r() % 8;
    if (const char* only = std::getenv("RS_U7_ONLY")) return RunScenarioCase(s, r, static_cast<unsigned>(std::atoi(only)));
    return RunScenarioCase(s, r, kind);
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
    managerObj = RealNew(0x20);
    std::memset(managerObj, 0, 0x20);
    *static_cast<void**>(managerObj) = managerVtable;
    fixedNames[managerObj] = "MGR";
    fixedNames[displayObj] = "D";
    fixedNames[textureManagerObj] = "TM";
    fixedNames[dbObj] = "DB";
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
    for (int s = 0; s < 2; ++s) {
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
        PatchImport(mod[s], "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z", reinterpret_cast<void*>(&HookCreateNode));
        PatchImport(mod[s], "?Create@PStringCache@Jet@@QAAPAUNode@12@PBEI@Z",
                    reinterpret_cast<void*>(&HookCreateNodeLen));
        PatchImport(mod[s], "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z", reinterpret_cast<void*>(&HookDestroyNode));
        PatchImport(mod[s], "?LookupPluginType@PluginBase@Jet@@KA?AVResult@2@PAPAVPluginType@2@ABVUId@2@ABVPString@2@@Z",
                    reinterpret_cast<void*>(&HookLookup));
    }
    for (int round = 0; round < rounds; ++round) {
        std::mt19937 r(round * 2654435761u + 17);
        for (int scenario = 0; scenario < 8; ++scenario) {
            materialUsed = 0;
            pick2Full = (r() % 2) != 0;
            for (unsigned i = 0; i < 16; ++i) texturesByType[i] = (r() % 3) == 0;
            plugins[5]->fallbackName = r() % 2 ? "oneten" : "gloss";
            for (int s = 0; s < 2; ++s) {
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
