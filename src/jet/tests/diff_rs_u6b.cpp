// Differential test of RenderSystem_7c unit U6b: AsciiTextureResource (AsciiTextureResource.cpp), ours against
// RenderSystem_7c_orig.dll in one process.
//
// As in diff_rs_u6: both modules' imports of Jet::Alloc::New / Delete are hooked (blocks filled with 0xcd and
// numbered, frees logged by name), each scenario runs the same random script on both sides with that side's
// exports, and the logs must be equal. The inputs are the install's .texture.txt files (all of them are the
// obsolete "key=value" kind) and generated ones of both kinds (damaged lines, every key and value, ConfigData
// texts behind a "version" line, cube textures). A stand-in resource database names files by their path and hands
// out stand-in bitmap resources: real BitmapResource-shaped objects (a copy of KernelResources' vftable keeps the
// RTTI for the dynamic_cast) whose Lock, Unlock, IsAllLoaded, SetPriority, Add/RemoveObserver and GetBitmap log
// and answer from the scenario's random stream. Their bitmaps are the images next to the .texture.txt when they
// exist, else made up (32-bit with all kinds of alpha, 24-bit, 565). The scenario drives loads, observer calls,
// the getters, the locks, GetMip/GetImage, the alpha and colour updates, Save (the file compared) and copies.
//
// usage: diff_rs_u6b.exe <bin dir> [rounds] [userdata dir]
//   env RS_U6B_ECHO (echo the log), RS_U6B_DUMP=<prefix> (both logs of the first mismatch)
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <random>
#include <string>
#include <vector>

static HMODULE mod[2];  // 0 ours, 1 the original
static HMODULE ks, krs;
static unsigned long long checks, bad;
static std::mt19937 rng(6602);

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

// ---- the heap hooks and the logs

using NewFn = void*(__cdecl*)(unsigned);
using DeleteFn = void(__cdecl*)(void*);
static NewFn RealNew;
static DeleteFn RealDelete;
static void** cache;
static void* (*CreateNode)(void*, const unsigned char*);
static void (*DestroyNode)(void*, void*);

static int cur = -1;
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
    static const bool echo = std::getenv("RS_U6B_ECHO") != nullptr;
    if (echo) std::fputs(buf, stdout);
}

static std::map<const void*, std::string> fixedNames;
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
        if (off >= 0 && static_cast<unsigned>(off) <= b->second) {
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
    if (p && n < 0x4000000) std::memset(p, 0xcd, n);
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

static void* TestNew(unsigned n, const char* fmt, int k)
{
    void* p = RealNew(n);
    std::memset(p, 0, n);
    char b[32];
    std::snprintf(b, sizeof b, fmt, k);
    names[cur][p] = b;
    blockSize[cur][p] = n;
    return p;
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
static unsigned long long Fnv(const void* p, size_t n)
{
    unsigned long long h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 1099511628211ull;
    return h;
}
static unsigned FloatBits(float f)
{
    unsigned u;
    std::memcpy(&u, &f, 4);
    return u;
}
template <class T>
static T& At(const void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(const_cast<void*>(p)) + off); }

// ---- per-side entry points

#define AT "AsciiTextureResource@Jet@@"
#define TR "TextureResource@Jet@@"

struct Api {
    void*(__fastcall* ctor)(void*, void*, const void* name, const void* db);
    void*(__fastcall* copy)(void*, void*, const void*);
    void*(__fastcall* assign)(void*, void*, const void*);
    void(__cdecl* setUsePath)(bool);
    bool(__cdecl* allValid)(const void*);
    const void*(__cdecl* bitmapImage)(const void*, const void*, const void*);
    void(__cdecl* updateAlpha)(const void*);
    void(__cdecl* updateAlphaStrict)(const void*);
    void(__cdecl* updateColor)(const void*);
};
static Api api[2];

static void LoadApi(int s)
{
    Api& a = api[s];
    HMODULE m = mod[s];
#define A(field, name) a.field = As<decltype(a.field)>(Proc(m, name))
    A(ctor, "??0" AT "QAE@ABVPString@1@PBVResourceDatabase@1@@Z");
    A(copy, "??0" AT "QAE@ABV01@@Z");
    A(assign, "??4" AT "QAEAAV01@ABV01@@Z");
    A(setUsePath, "?SetUsePath@" AT "SAX_N@Z");
    A(allValid, "?AllValidBitmapsAvailable@" AT "IBA_NXZ");
    A(bitmapImage, "?GetBitmapImage@" AT "IBAPBVBitmap@2@PBV32@0@Z");
    A(updateAlpha, "?UpdateAlphaHint@" AT "IBAXXZ");
    A(updateAlphaStrict, "?UpdateAlphaHintStrict@" AT "IBAXXZ");
    A(updateColor, "?UpdateAverageColor@" AT "IBAXXZ");
#undef A
}

// the original's statics both sides use
static int* maxStreamSize;
static void(__cdecl* setDefault[6])(int);  // wrap s, wrap t, hint, automip, alpha hint, display format
static void(__cdecl* setDefaultCompression)(char*);

// KernelStandard
static void*(__fastcall* bitmapCtor)(void*, void*, const int* size, int format, unsigned char* data);
static void*(__fastcall* bitmapCtor0)(void*, void*);
static void(__fastcall* bitmapDtor)(void*, void*);
static bool(__cdecl* bitmapLoad)(void*, void* const* name, void* const* base, bool keep);

// ---- the bitmaps

struct Image {
    int w = 0, h = 0, format = 0, pitch = 0;
    std::vector<unsigned char> bytes;
    bool ok = false;
};
static std::map<std::string, Image> images;  // loaded once per path, both sides use the same pixels

static const Image& LoadImage(const std::string& path)
{
    auto it = images.find(path);
    if (it != images.end()) return it->second;
    Image& im = images[path];
    const int saved = cur;
    cur = -1;
    if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        void* b = RealNew(0x44);
        bitmapCtor0(b, nullptr);
        void* n = CreateNode(*cache, reinterpret_cast<const unsigned char*>(path.c_str()));
        void* base = nullptr;
        if (bitmapLoad(b, &n, &base, true)) {
            const int* f = static_cast<const int*>(b);
            if (f[0] > 0 && f[1] > 0 && f[0] * f[1] <= 1024 * 1024 && f[3]) {
                im.w = f[0];
                im.h = f[1];
                im.pitch = f[4];
                im.format = f[6];
                const unsigned char* data = reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(f[3]));
                im.bytes.assign(data, data + static_cast<size_t>(im.pitch) * im.h);
                im.ok = true;
            }
        }
        DestroyNode(*cache, n);
        bitmapDtor(b, nullptr);
        RealDelete(b);
    }
    cur = saved;
    return im;
}

static std::string BitmapText(const void* p)
{
    if (!p) return "null";
    const int* b = static_cast<const int*>(p);
    const unsigned size = static_cast<unsigned>(b[4]) * static_cast<unsigned>(b[1] < 0 ? -b[1] : b[1]);
    char t[192];
    std::snprintf(t, sizeof t, "%dx%d pitch %d bpp %d format %d alloc %d data %s %016llx", b[0], b[1], b[4], b[5], b[6],
                  *reinterpret_cast<const unsigned char*>(b + 7),
                  Name(reinterpret_cast<void*>(static_cast<uintptr_t>(b[3]))).c_str(),
                  b[3] ? Fnv(reinterpret_cast<void*>(static_cast<uintptr_t>(b[3])), size) : 0ull);
    return t;
}
static void FreeBitmap(const void* b)
{
    if (!b) return;
    bitmapDtor(const_cast<void*>(b), nullptr);
    HookDelete(const_cast<void*>(b));
}

// ---- the stand-in bitmap resources

static std::mt19937 standRng;
static void* sbVt[2][1 + 32];  // [0]: the copied RTTI locator
static void* otherVt[1 + 32];  // a resource that is no bitmap (the dynamic_cast fails)
struct Stand {
    void* bitmap = nullptr;
    int locks = 0;
    bool lastLock = false;
    std::vector<const void*> observers;
};
static std::map<const void*, Stand> stands[2];
static std::map<std::string, void*> standByName[2];
static int standCount[2];

static int& Status(const void* r) { return At<int>(r, 0x28); }

static int* __cdecl SbLock(const void* r, int* out)
{
    const bool ok = standRng() % 9 != 0;
    stands[cur][r].lastLock = ok;
    if (ok) {
        ++stands[cur][r].locks;
        if (Status(r) != 6 && standRng() % 2) Status(r) = 6;
    }
    *out = ok ? 0 : 1;
    Log("  lock %s: %d status %d\n", Name(r).c_str(), *out, Status(r));
    return out;
}
static void __cdecl SbUnlock(const void* r)
{
    --stands[cur][r].locks;
    Log("  unlock %s\n", Name(r).c_str());
}
static int* __cdecl SbIsAllLoaded(const void* r, int* out)
{
    const int st = Status(r);
    *out = st == 6 ? 0 : st == 8 ? 1 : 5;
    Log("  all loaded %s: %d\n", Name(r).c_str(), *out);
    return out;
}
static void __cdecl SbSetPriority(const void* r, float p) { Log("  priority %s %08x\n", Name(r).c_str(), FloatBits(p)); }
static void __cdecl SbAddObserver(const void* r, const void* o)
{
    Log("  add observer %s %s\n", Name(r).c_str(), Name(o).c_str());
    if (At<unsigned char>(r, 0x30)) {
        (*static_cast<void(__cdecl* const* const*)(const void*, const void*)>(o))[0](o, r);
        return;
    }
    stands[cur][r].observers.push_back(o);
}
static void __cdecl SbRemoveObserver(const void* r, const void* o)
{
    auto& v = stands[cur][r].observers;
    auto it = std::find(v.begin(), v.end(), o);
    Log("  remove observer %s %s: %d\n", Name(r).c_str(), Name(o).c_str(), it != v.end());
    if (it != v.end()) v.erase(it);
}
static const void* __cdecl SbGetBitmap(const void* r) { return stands[cur][r].bitmap; }
template <int N>
static int __cdecl SbTrap(const void* r)
{
    Log("  stand slot %d %s\n", N, Name(r).c_str());
    return 0;
}
template <int... N>
static void FillTraps(void** vt, std::integer_sequence<int, N...>)
{
    ((vt[N] = reinterpret_cast<void*>(&SbTrap<N>)), ...);
}

// a made-up bitmap: 32-bit with a kind of alpha, or 24/16-bit
static void* MakeBitmap(int s, int k)
{
    static const int formats[] = {2, 3, 2, 3, 2, 1, 4};
    static const int sizes[] = {1, 2, 3, 4, 7, 8, 16, 32, 33, 64, 100};
    const int format = formats[standRng() % 7];
    const int size[2] = {sizes[standRng() % 11], sizes[standRng() % 11]};
    void* b = TestNew(0x44, "BMP%d", k);
    bitmapCtor(b, nullptr, size, format, nullptr);
    const int* f = static_cast<const int*>(b);
    unsigned char* data = reinterpret_cast<unsigned char*>(static_cast<uintptr_t>(f[3]));
    const unsigned n = static_cast<unsigned>(f[4]) * static_cast<unsigned>(f[1]);
    const int mode = static_cast<int>(standRng() % 6);  // alpha: opaque, 0/255, any, zero, mostly opaque, edges
    for (unsigned i = 0; i < n; ++i) {
        unsigned char v = static_cast<unsigned char>(standRng());
        if (i % 4 == 3 || (format == 3 && i % 4 == 0)) {
            if (mode == 0) v = 0xff;
            else if (mode == 1) v = v & 1 ? 0xff : 0;
            else if (mode == 3) v = 0;
            else if (mode == 4) v = standRng() % 50 ? 0xff : v;
            else if (mode == 5) v = standRng() % 40 ? (v & 1 ? 0xff : 0) : (v & 1 ? 0xfe : 1);
        }
        data[i] = v;
    }
    (void)s;
    return b;
}

static void* StandFor(int s, const char* name)
{
    auto it = standByName[s].find(name);
    if (it != standByName[s].end()) {
        ++At<int>(it->second, 0x1c);
        return it->second;
    }
    const int k = standCount[s]++;
    void* r = TestNew(0x60, "SB%d", k);
    const unsigned kind = standRng() % 24;
    *static_cast<void**>(r) = kind == 0 ? &otherVt[1] : &sbVt[s][1];
    At<int>(r, 0x1c) = 1000;                     // refs: never down to 0
    Status(r) = standRng() % 3 ? 6 : 2;          // available or queued
    At<unsigned char>(r, 0x30) = standRng() % 4 == 0;  // observers notified already
    Stand& st = stands[s][r];
    const Image& im = LoadImage(name);
    if (im.ok && standRng() % 4) {
        void* b = TestNew(0x44, "BMP%d", 1000 + k);
        const int size[2] = {im.w, im.h};
        bitmapCtor(b, nullptr, size, im.format, nullptr);
        const int* f = static_cast<const int*>(b);
        std::memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(f[3])), im.bytes.data(),
                    std::min(im.bytes.size(), static_cast<size_t>(f[4]) * f[1]));
        st.bitmap = b;
    } else {
        st.bitmap = MakeBitmap(s, k);
    }
    standByName[s][name] = r;
    Log("  stand %s %s: kind %u status %d notified %d bitmap %s\n", Name(r).c_str(),
        std::filesystem::path(name).filename().string().c_str(), kind == 0, Status(r), At<unsigned char>(r, 0x30),
        BitmapText(st.bitmap).c_str());
    return r;
}

// the resource database: slot 1 Get, slot 3 GetFileName (the name as the path)
static void* dbVt[8];
static unsigned char dbObj[0x48];
static int __cdecl DbSlot(void*) { Log("  db slot\n"); return 0; }
static const void* __cdecl DbGet(void*, void* const* name, int flags)
{
    const char* text = *name ? static_cast<const char*>(*name) + 0x18 : "";
    const bool missing = std::strstr(text, "missing") != nullptr || standRng() % 30 == 0;
    Log("  db get %s %d: %s\n", text, flags, missing ? "none" : "found");
    if (missing) return nullptr;
    return StandFor(cur, text);
}
static void** __cdecl DbFileName(void*, void** out, void* const* name)
{
    const char* text = *name ? static_cast<const char*>(*name) + 0x18 : "";
    *out = CreateNode(*cache, reinterpret_cast<const unsigned char*>(text));
    return out;
}

// ---- the inputs

static std::vector<std::string> files;

static void WriteFile(const std::string& path, const std::string& text)
{
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
        std::fwrite(text.data(), 1, text.size(), f);
        std::fclose(f);
    }
}

// generated .texture.txt files of both kinds
static void MakeInputs(const std::string& dir)
{
    std::filesystem::create_directories(dir);
    std::mt19937 g(66021);
    auto R = [&](unsigned n) { return static_cast<unsigned>(g() % n); };
    static const char* imgs[] = {"a.tga", "b.tga", "a.tga", "missing.tga", "\\abs.tga", "/abs2.tga", "sub/c.bmp",
                                 "B.TGA", "..\\up.tga", ""};
    static const char* const keys[][8] = {
        {"Tile", "s", "t", "st", "ts", "none", "x", "TS"},
        {"Hint", "static", "dynamic", "STATIC", "foo", "dynamic", "static", ""},
        {"NormalMapHint", "none", "normalmap", "NormalMap", "bad", "none", "normalmap", ""},
        {"ModifyMap", "none", "flipgreen", "FlipGreen", "x", "none", "flipgreen", ""},
        {"AlphaHint", "opaque", "semitransparent", "masked", "default", "x", "Masked", ""},
        {"Anisotropy", "1", "2", "4", "8", "16", "3", "abc"},
        {"AutoMip", "none", "default", "fastest", "nicest", "x", "NONE", ""},
        {"MagFilter", "nearest", "linear", "default", "x", "Linear", "nearest", ""},
        {"MinFilter", "nearest", "linear", "default", "x", "Linear", "nearest", ""},
        {"MipFilter", "none", "nearest", "linear", "default", "x", "None", ""},
        {"Compression", "none", "dxt1", "dxt3", "dxt5", "atc", "atce", "etc1"},
        {"Dimension", "two", "cube", "volume", "one", "x", "Cube", ""},
    };
    for (int k = 0; k < 240; ++k) {
        std::string t;
        const char* nl = R(3) ? "\r\n" : "\n";
        const bool newer = k % 4 == 3;
        const bool cube = R(4) == 0;
        if (newer) {
            t = std::string(R(8) ? "version 1.00" : "version 2") + nl + (R(10) ? "Texture" : "Textur") + nl + "{" + nl;
            const int prims = cube ? 6 + static_cast<int>(R(2)) - static_cast<int>(R(2)) : 1 + static_cast<int>(R(2) * R(2));
            for (int i = 0; i < prims; ++i) t += std::string("  Primary = \"") + imgs[R(10)] + "\";" + nl;
            if (R(3) == 0)
                for (int i = 0, n = R(4) ? prims : 1 + R(3); i < n; ++i)
                    t += std::string("  Alpha = \"") + imgs[R(10)] + "\";" + nl;
            if (cube) t += std::string("  Dimension = \"cube\";") + nl;
            for (int i = 0, n = R(5); i < n; ++i) {
                const auto& kv = keys[R(12)];
                const char* v = kv[1 + (R(5) ? R(3) : R(7))];
                if (!*v) v = kv[1];
                if (kv == keys[5] && R(2))
                    t += std::string("  ") + kv[0] + " = " + v + ";" + nl;
                else
                    t += std::string("  ") + kv[0] + " = \"" + v + "\";" + nl;
            }
            if (R(2)) t += std::string("  Compression = \"") + keys[10][1 + R(7)] + "\";" + nl;
            if (R(3) == 0) t += std::string("  Hint = \"static\";") + nl;
            t += std::string("}") + nl;
        } else {
            const int prims = cube ? 6 : 1 + static_cast<int>(R(3) == 0);
            for (int i = 0; i < prims; ++i)
                t += std::string(R(6) ? "Primary" : (R(2) ? "primary" : "\tPRIMARY")) + "=" + imgs[R(10)] + nl;
            if (cube && R(2))
                for (int i = 0; i < 6; ++i) t += std::string("Alpha=") + imgs[R(10)] + nl;
            else if (R(3) == 0)
                t += std::string(R(2) ? "Alpha" : "alpha") + "=" + imgs[R(10)] + nl;
            if (R(2)) t += std::string("Compression=") + keys[10][1 + R(7)] + nl;
            if (R(3) == 0) t += std::string("Hint=static") + nl;
            if (cube) t += std::string("Dimension=cube") + nl;
            for (int i = 0, n = R(7); i < n; ++i) {
                switch (R(9)) {
                case 0: t += std::string("garbage line") + nl; break;
                case 1: t += nl; break;
                case 2: t += std::string("Primary = ") + imgs[R(10)] + nl; break;  // "Primary " is no key
                case 3: t += std::string("bump=x") + nl; break;
                case 4: if (R(4) == 0) t += std::string(R(3) ? (R(2) ? "Version=1" : "versionX") : "myversion=1") + nl; break;
                default: {
                    const auto& kv = keys[R(12)];
                    std::string key = kv[0];
                    if (R(4) == 0) for (char& c : key) c = static_cast<char>(std::tolower(c));
                    t += (R(5) ? "" : " ") + key + "=" + kv[1 + (R(5) ? R(3) : R(7))] + nl;
                }
                }
            }
            if (R(5) == 0) t.resize(t.size() - std::strlen(nl));  // no final line end
        }
        const std::string path = dir + "\\syn_" + std::to_string(k) + ".texture.txt";
        WriteFile(path, t);
        files.push_back(path);
    }
}

// ---- the dumps

template <class R, class... T>
static R SC(const void* c, int slot, T... args)
{
    using Fn = R(__cdecl*)(const void*, T...);
    return (*static_cast<Fn* const*>(c))[slot](c, args...);
}

static void DumpList(std::string& t, const void* p, unsigned off)
{
    const unsigned n = At<unsigned>(p, off), cap = At<unsigned>(p, off + 4);
    void* const* items = At<void* const*>(p, off + 8);
    t += " [" + std::to_string(n) + "/" + std::to_string(cap) + " " + Name(items);
    for (unsigned i = 0; i < n && i < 16; ++i) t += " " + Name(items[i]);
    t += "]";
}
static void DumpAscii(const char* tag, const void* p)
{
    const unsigned char* b = static_cast<const unsigned char*>(p);
    std::string t;
    DumpList(t, p, 0x68);
    DumpList(t, p, 0x74);
    DumpList(t, p, 0x80);
    Log("  %s %s: info %s flag %d status %d notified %d lists%s fields %s %02x %02x %s %02x %s\n", tag, Name(p).c_str(),
        Name(At<void*>(p, 0x5c)).c_str(), b[0x60], At<int>(p, 0x28), b[0x30], t.c_str(), Hex(b + 0x8c, 9).c_str(),
        b[0x98], b[0x9c], Hex(b + 0xa0, 0x28).c_str(), b[0xc8], Hex(b + 0xcc, 0xc).c_str());
}

static void DumpGetters(const void* r)
{
    int size[2] = {-1, -1};
    float color[4] = {};
    char comp[9] = {};
    SC<void*>(r, 24, size);
    SC<void*>(r, 21, color);
    SC<void>(r, 26, comp);
    using FloatFn = float(__cdecl*)(const void*);
    const float aniso = (*static_cast<FloatFn* const*>(r))[25](r);
    Log("  getters: avail %d normal %d alpha %d color %s dim %d depth %u size %dx%d aniso %08x comp %s ratio %u "
        "format %d image %d display %d hint %d wrap %d %d filters %d %d %d levels %u automip %d must %d modify %d "
        "mips %d\n",
        SC<bool>(r, 18), SC<int>(r, 19), SC<int>(r, 20), Hex(color, 16).c_str(), SC<int>(r, 22), SC<unsigned>(r, 23),
        size[0], size[1], FloatBits(aniso), Hex(comp, 8).c_str(), SC<unsigned>(r, 27), SC<int>(r, 31), SC<int>(r, 32),
        SC<int>(r, 33), SC<int>(r, 34), SC<int>(r, 35), SC<int>(r, 36), SC<int>(r, 37), SC<int>(r, 38), SC<int>(r, 39),
        SC<unsigned>(r, 40), SC<int>(r, 41), SC<bool>(r, 7), SC<int>(r, 45), SC<bool>(r, 46));
}

static void DestroyResource(void* o)
{
    const auto f = reinterpret_cast<void(__fastcall*)(void*, void*, int)>((*static_cast<void***>(o))[8]);
    f(o, nullptr, 1);
}

// a stand-in's observers told that it has loaded
static void NotifyStand(const void* r)
{
    Status(r) = 6;
    At<unsigned char>(r, 0x30) = 1;
    const std::vector<const void*> obs = stands[cur][r].observers;
    Log("  stand loaded %s, %u observers\n", Name(r).c_str(), static_cast<unsigned>(obs.size()));
    for (auto it = obs.rbegin(); it != obs.rend(); ++it)
        (*static_cast<void(__cdecl* const* const*)(const void*, const void*)>(*it))[0](*it, r);
}

static std::vector<void*> AllStands()
{
    std::vector<void*> v;
    for (auto& [k, st] : stands[cur]) v.push_back(const_cast<void*>(k));
    std::sort(v.begin(), v.end(), [](void* a, void* b) { return Name(a) < Name(b); });
    return v;
}

// ---- the scenario

static void RunAscii(int s, unsigned seed)
{
    std::mt19937 r(seed);
    standRng.seed(seed ^ 0x6b6b);
    stands[s].clear();
    standByName[s].clear();
    standCount[s] = 0;
    const Api& a = api[s];
    int made = 0, saved = 0;

    // the statics, the same on both sides
    *maxStreamSize = 16 << (r() % 8);
    setDefault[0](static_cast<int>(r() % 3));
    setDefault[1](static_cast<int>(r() % 3));
    setDefault[2](static_cast<int>(r() % 2));
    setDefault[3](static_cast<int>(r() % 4));
    setDefault[4](r() % 3 ? 3 : static_cast<int>(r() % 3));
    setDefault[5](1);
    char comp[9] = "lzss";
    if (r() % 3 == 0) comp[0] = 0;
    setDefaultCompression(comp);
    a.setUsePath(r() % 2 != 0);
    Log("statics: max size %d\n", *maxStreamSize);

    std::vector<void*> res;
    auto newResource = [&]() {
        const std::string& f = files[r() % files.size()];
        void* p = TestNew(0xd8, "R%d", made++);
        std::memset(p, 0xcd, 0xd8);
        void* name = CreateNode(*cache, reinterpret_cast<const unsigned char*>(f.c_str()));
        Log("op new %s %s\n", Name(p).c_str(), std::filesystem::path(f).filename().string().c_str());
        a.ctor(p, nullptr, &name, dbObj);
        DestroyNode(*cache, name);
        // the load name the resource system would set: bitmap names are relative to the .texture.txt
        if (At<void*>(p, 0x38)) DestroyNode(*cache, At<void*>(p, 0x38));
        At<void*>(p, 0x38) = CreateNode(*cache, reinterpret_cast<const unsigned char*>(f.c_str()));
        DumpAscii("made", p);
        res.push_back(p);
        return p;
    };
    auto loadIt = [&](void* p) {
        const char* path = static_cast<const char*>(At<void*>(p, 0x38)) + 0x18;
        int result = -7;
        SC<int*>(p, 11, &result, path);
        Log("op load txt %s: %d\n", Name(p).c_str(), result);
        DumpAscii("loaded", p);
        if (result == 0) {
            int lc = -7;
            SC<int*>(p, 12, &lc);
            Log("op load complete %s: %d\n", Name(p).c_str(), lc);
        }
    };
    for (int k = 0, n = 1 + static_cast<int>(r() % 2); k < n; ++k) loadIt(newResource());

    for (int step = 0, steps = 8 + static_cast<int>(r() % 16); step < steps; ++step) {
        void* p = res[r() % res.size()];
        const unsigned nPrim = At<unsigned>(p, 0x68);
        // a failed load can leave fewer alpha bitmaps than primary ones: GetMipLock and GetImage then read the alpha
        // list past its end (in both modules), the others refuse such lists
        const unsigned nAlpha = At<unsigned>(p, 0x74);
        const bool ragged = nAlpha && nAlpha != nPrim;
        switch (r() % 18) {
        case 0:
            loadIt(newResource());
            break;
        case 1:  // load again (the lists grow)
            if (r() % 3) break;
            loadIt(p);
            break;
        case 2: case 3:
            Log("op getters %s\n", Name(p).c_str());
            DumpGetters(p);
            break;
        case 4: {  // a stand-in has loaded
            const auto v = AllStands();
            if (v.empty()) break;
            NotifyStand(v[r() % v.size()]);
            DumpAscii("after", p);
            break;
        }
        case 5: {  // the resource's own states
            const int st = r() % 3 ? 6 : 2;
            if (r() % 2)
                for (void* sb : AllStands()) At<unsigned char>(sb, 0x30) = static_cast<unsigned char>(r() % 3 != 0);
            At<int>(p, 0x28) = st;
            At<unsigned char>(p, 0x30) = static_cast<unsigned char>(r() % 2);
            // (IsAllLoaded reads the first primary even when there is none, in both modules)
            int all = -7;
            const unsigned char dim = At<unsigned char>(p, 0x9c);
            if (nPrim || dim == 1 || dim == 2) SC<int*>(p, 3, &all);
            Log("op states %s %d: valid %d available %d all loaded %d\n", Name(p).c_str(), st, a.allValid(p),
                SC<bool>(p, 18), all);
            if (st == 6) {
                int lr = -7;
                SC<int*>(p, 0, &lr);
                Log("  lock %d\n", lr);
                if (lr == 0) SC<void>(p, 1);
            }
            break;
        }
        case 6: {  // OnLoad through the observer, OnLoadNotify
            const auto v = AllStands();
            const void* obs = static_cast<char*>(p) + 0x64;
            if (r() % 2 && !v.empty()) {
                Log("op on load %s\n", Name(p).c_str());
                (*static_cast<void(__cdecl* const* const*)(const void*, const void*)>(obs))[0](obs, v[r() % v.size()]);
            } else {
                Log("op on load notify %s\n", Name(p).c_str());
                SC<void>(p, 13);
            }
            DumpAscii("after", p);
            break;
        }
        case 7: case 8: {  // the mip lock
            if (ragged) break;
            const bool ok = SC<bool>(p, 42);
            Log("op mip lock %s: %d\n", Name(p).c_str(), ok);
            DumpAscii("locked", p);
            if (ok) {
                for (int k = 0, n = 1 + static_cast<int>(r() % 3); k < n; ++k) {
                    const unsigned face = r() % (nPrim + 1);
                    const unsigned images = At<unsigned>(p, 0x80);
                    int size[2] = {1 + static_cast<int>(r() % 70), 1 + static_cast<int>(r() % 70)};
                    if (images && r() % 2) {
                        const int* b = static_cast<const int*>(At<void* const*>(p, 0x88)[face % images]);
                        size[0] = b[0] >> (r() % 3);
                        size[1] = b[1] >> (r() % 3);
                    }
                    const void* m = SC<const void*>(p, 43, r() % 4, face, size);
                    Log("  mip %u %dx%d: %s\n", face, size[0], size[1], BitmapText(m).c_str());
                    FreeBitmap(m);
                }
                if (r() % 4) Log("  unlock %d\n", SC<bool>(p, 44));
            } else if (r() % 3 == 0) {
                Log("  unlock %d\n", SC<bool>(p, 44));
            }
            break;
        }
        case 9: {  // GetImage
            const unsigned face = r() % (nPrim + 2);
            if (ragged && face >= nAlpha && face < nPrim) break;
            const void* m = SC<const void*>(p, 48, face);
            // a new bitmap's pixels come from KernelStandard's heap uninitialised: only those written are compared
            // (a copy in a format CopyImage handles, or a merge that happened)
            bool written = false;
            if (m) {
                const void* prim = At<void* const*>(p, 0x70)[face];
                const int* pb = static_cast<const int*>(stands[s][prim].bitmap);
                const unsigned na = At<unsigned>(p, 0x74);
                const void* al = na && na >= face ? At<void* const*>(p, 0x7c)[face] : nullptr;
                if (!al) {
                    written = pb[6] >= 1 && pb[6] <= 4;
                } else {
                    const int* ab = static_cast<const int*>(stands[s][al].bitmap);
                    written = stands[s][al].lastLock && (ab[6] == 2 || ab[6] == 3) && (pb[6] == 2 || pb[6] == 3) &&
                              ab[0] == pb[0] && ab[1] == pb[1];
                }
            }
            const std::string text = BitmapText(m);
            Log("op image %s %u: %s\n", Name(p).c_str(), face,
                written || !m ? text.c_str() : ("not written " + text.substr(0, text.find(" data"))).c_str());
            FreeBitmap(m);
            break;
        }
        case 10: {  // the alpha hint and the colour worked out again
            const unsigned k = r() % 4;
            if (r() % 2) At<signed char>(p, 0x91) = static_cast<signed char>(r() % 2 ? -1 : r() % 3);
            if (r() % 2) std::memset(static_cast<char*>(p) + 0xb8, 0, 16);
            Log("op update %s %u\n", Name(p).c_str(), k);
            if (k == 0) a.updateAlpha(p);
            if (k == 1) a.updateAlphaStrict(p);
            if (k >= 2) a.updateColor(p);
            DumpAscii("updated", p);
            break;
        }
        case 11: {  // GetBitmapImage on the stand-ins' bitmaps
            const auto v = AllStands();
            if (v.empty()) break;
            const void* x = stands[s][v[r() % v.size()]].bitmap;
            const void* y = r() % 4 == 0 ? nullptr : r() % 4 == 0 ? x : stands[s][v[r() % v.size()]].bitmap;
            const void* m = a.bitmapImage(p, x, y);
            Log("op bitmap image %s %s %s: %s\n", Name(p).c_str(), Name(x).c_str(), Name(y).c_str(), BitmapText(m).c_str());
            FreeBitmap(m);
            break;
        }
        case 12: {  // Save, the file compared
            char path[MAX_PATH];
            std::snprintf(path, sizeof path, "%s\\u6b_%d_%d.texture", std::getenv("TEMP"), s, saved++);
            DeleteFileA(path);
            const bool ok = SC<bool>(p, 30, path, static_cast<void*>(nullptr));
            std::string content;
            if (FILE* f = std::fopen(path, "rb")) {
                char buf[65536];
                size_t n;
                while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) content.append(buf, n);
                std::fclose(f);
            }
            Log("op save %s: %d file %u %016llx head %s\n", Name(p).c_str(), ok, static_cast<unsigned>(content.size()),
                Fnv(content.data(), content.size()),
                Hex(content.data(), static_cast<unsigned>(std::min<size_t>(content.size(), 0x80))).c_str());
            DeleteFileA(path);
            DumpAscii("saved", p);
            break;
        }
        case 13: {  // the copy and the assignment (not while the images are held: both would free them)
            if (At<unsigned>(p, 0x80)) break;
            void* y = TestNew(0xd8, "R%d", made++);
            std::memset(y, 0xcd, 0xd8);
            a.copy(y, nullptr, p);
            DumpAscii("copy", y);
            a.assign(y, nullptr, p);
            DumpAscii("assigned", y);
            At<void*>(y, 0x5c) = nullptr;
            DestroyResource(y);
            break;
        }
        case 14: {  // a stand-in fails or is queued again
            const auto v = AllStands();
            if (v.empty()) break;
            const void* sb = v[r() % v.size()];
            Status(sb) = r() % 2 ? 8 : 2;
            At<unsigned char>(sb, 0x30) = 0;
            Log("op stand %s status %d\n", Name(sb).c_str(), Status(sb));
            break;
        }
        case 15:
            a.setUsePath(r() % 2 != 0);
            Log("op use path\n");
            break;
        case 16: {  // let one go
            if (res.size() < 2) break;
            const size_t k = r() % res.size();
            Log("op release %s\n", Name(res[k]).c_str());
            DestroyResource(res[k]);
            res.erase(res.begin() + static_cast<long>(k));
            break;
        }
        default: {  // the mip lock held through the getters
            if (ragged || !SC<bool>(p, 42)) break;
            Log("op locked getters %s\n", Name(p).c_str());
            DumpGetters(p);
            SC<bool>(p, 44);
            break;
        }
        }
    }
    for (void* p : res) {
        Log("op release %s\n", Name(p).c_str());
        DestroyResource(p);
    }
    for (void* sb : AllStands()) {
        Stand& st = stands[s][sb];
        Log("stand %s: refs %d locks %d observers %u\n", Name(sb).c_str(), At<int>(sb, 0x1c), st.locks,
            static_cast<unsigned>(st.observers.size()));
        bitmapDtor(st.bitmap, nullptr);
        RealDelete(st.bitmap);
        RealDelete(sb);
    }
}

// ---- running

static void Compare(int round)
{
    ++checks;
    if (logText[0] == logText[1]) return;
    if (const char* dump = std::getenv("RS_U6B_DUMP"); dump && !bad)
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
    std::printf("MISMATCH round %d\n-- ours:\n%s\n-- original:\n%s\n", round, x.substr(from, i - from + 600).c_str(),
                y.substr(from, i - from + 600).c_str());
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
        std::printf("usage: diff_rs_u6b <bin dir> [rounds] [userdata dir]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 300;
    const std::string binDir = argv[1];
    const std::string root = argc > 3 ? argv[3] : binDir + "\\..\\UserData";
    SetDllDirectoryA(argv[1]);
    mod[0] = LoadLibraryA((binDir + "\\RenderSystem_7c.dll").c_str());
    mod[1] = GetModuleHandleA("RenderSystem_7c_orig.dll");
    ks = GetModuleHandleA("KernelStandard_7c.dll");
    krs = LoadLibraryA((binDir + "\\KernelResources_7c.dll").c_str());
    if (!mod[0] || !mod[1] || !ks || !krs) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    // the inputs: every .texture.txt of editing, the first 300 of local and 60 of original (name order), and the
    // generated ones
    for (const char* sub : {"editing", "local", "original"}) {
        std::vector<std::string> found;
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(root + "\\" + sub, ec);
             !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            const std::string p = it->path().string();
            if (p.size() > 12 && _stricmp(p.c_str() + p.size() - 12, ".texture.txt") == 0) found.push_back(p);
            if (found.size() >= 3000) break;
        }
        std::sort(found.begin(), found.end());
        const size_t keep = std::strcmp(sub, "editing") == 0 ? found.size() : std::strcmp(sub, "local") == 0 ? 300 : 60;
        for (size_t i = 0; i < found.size() && i < keep; ++i) files.push_back(found[i]);
    }
    const size_t real = files.size();
    MakeInputs(std::string(std::getenv("TEMP")) + "\\u6b_inputs");
    std::printf("%u real and %u generated .texture.txt files\n", static_cast<unsigned>(real),
                static_cast<unsigned>(files.size() - real));
    RealNew = static_cast<NewFn>(Proc(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    RealDelete = static_cast<DeleteFn>(Proc(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    CreateNode = As<decltype(CreateNode)>(Proc(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    DestroyNode = As<decltype(DestroyNode)>(Proc(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z"));
    cache = static_cast<void**>(Proc(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    bitmapCtor = As<decltype(bitmapCtor)>(Proc(ks, "??0Bitmap@Jet@@QAE@ABUSize@1@W4Format@01@PAE@Z"));
    bitmapCtor0 = As<decltype(bitmapCtor0)>(Proc(ks, "??0Bitmap@Jet@@QAE@XZ"));
    bitmapDtor = As<decltype(bitmapDtor)>(Proc(ks, "??1Bitmap@Jet@@QAE@XZ"));
    bitmapLoad = As<decltype(bitmapLoad)>(Proc(ks, "?Load@Bitmap@Jet@@QAA_NABVPString@2@0_N@Z"));
    maxStreamSize = static_cast<int*>(Proc(mod[1], "?MAX_STREAMING_TEXTURE_SIZE@TextureManager@Jet@@2HA"));
    setDefault[0] = As<void(__cdecl*)(int)>(Proc(mod[1], "?SetDefaultWrapS@" TR "SAXW4TextureWrap@2@@Z"));
    setDefault[1] = As<void(__cdecl*)(int)>(Proc(mod[1], "?SetDefaultWrapT@" TR "SAXW4TextureWrap@2@@Z"));
    setDefault[2] = As<void(__cdecl*)(int)>(Proc(mod[1], "?SetDefaultTextureHint@" TR "SAXW4TextureHint@2@@Z"));
    setDefault[3] = As<void(__cdecl*)(int)>(Proc(mod[1], "?SetDefaultAutoMip@" TR "SAXW4TextureAutoMip@2@@Z"));
    setDefault[4] = As<void(__cdecl*)(int)>(Proc(mod[1], "?SetDefaultAlphaHint@" TR "SAXW4TextureAlpha@2@@Z"));
    setDefault[5] = As<void(__cdecl*)(int)>(Proc(mod[1], "?SetDefaultDisplayFormatId@" TR "SAXW4DFormatId@2@@Z"));
    setDefaultCompression = As<decltype(setDefaultCompression)>(Proc(mod[1], "?SetDefaultFileCompression@" TR "SAXPAD@Z"));
    // KernelResource's Resource reads the resource system's time: a zeroed stand-in (as in diff_rs_u3)
    static unsigned char fakeSystem[0x400];
    As<void(__cdecl*)(void*)>(Proc(GetModuleHandleA("KernelResource_7c.dll"), "?InitRS@Resource@Jet@@CAXPAVResourceSystem@2@@Z"))(fakeSystem);
    // the stand-in bitmap resources: BitmapResource's RTTI, logging slots
    void* const* bmpVt = static_cast<void* const*>(Proc(krs, "??_7BitmapResource@Jet@@6B@"));
    void* const* texVt = static_cast<void* const*>(Proc(mod[1], "??_7" TR "6B@"));
    for (int s = 0; s < 2; ++s) {
        void** vt = &sbVt[s][1];
        sbVt[s][0] = bmpVt[-1];
        FillTraps(vt, std::make_integer_sequence<int, 32>());
        vt[0] = reinterpret_cast<void*>(&SbLock);
        vt[1] = reinterpret_cast<void*>(&SbUnlock);
        vt[3] = reinterpret_cast<void*>(&SbIsAllLoaded);
        vt[4] = reinterpret_cast<void*>(&SbSetPriority);
        vt[5] = reinterpret_cast<void*>(&SbAddObserver);
        vt[6] = reinterpret_cast<void*>(&SbRemoveObserver);
        vt[21] = reinterpret_cast<void*>(&SbGetBitmap);
    }
    std::memcpy(otherVt, sbVt[0], sizeof otherVt);
    otherVt[0] = texVt[-1];
    for (void*& p : dbVt) p = reinterpret_cast<void*>(&DbSlot);
    dbVt[1] = reinterpret_cast<void*>(&DbGet);
    dbVt[3] = reinterpret_cast<void*>(&DbFileName);
    *reinterpret_cast<void**>(dbObj) = dbVt;
    fixedNames[dbObj] = "DB";
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
    }
    if (const char* probe = std::getenv("RS_U6B_PROBE")) {  // a file through KernelStandard's ConfigParser
        unsigned char cp[0x18];
        As<void(__fastcall*)(void*, void*)>(Proc(ks, "??0ConfigParser@Jet@@QAE@XZ"))(cp, nullptr);
        int res = -7;
        As<int*(__cdecl*)(void*, int*, const char*)>(Proc(ks,
            "?Parse@ConfigParser@Jet@@QAA?AV?$ResultClass@$1?results@ConfigParser@Jet@@2QBUResultTable@2@BW4Results@ConfigParser@Jet@@@2@PBD@Z"))(
            cp, &res, probe);
        void* text = nullptr;
        const bool ok = As<bool(__fastcall*)(void*, void*, void**, int)>(Proc(ks, "?SaveString@ConfigParser@Jet@@QBE_NAAVPString@2@H@Z"))(
            cp, nullptr, &text, 0);
        std::printf("parse %d save %d:\n%s\n", res, ok, text ? static_cast<const char*>(text) + 0x18 : "(none)");
        for (int s = 0; s < 2; ++s) {
            cur = s;
            void* p = TestNew(0xd8, "R%d", 0);
            void* name = CreateNode(*cache, reinterpret_cast<const unsigned char*>(probe));
            api[s].ctor(p, nullptr, &name, dbObj);
            At<void*>(p, 0x38) = name;
            int result = -7;
            SC<int*>(p, 11, &result, probe);
            std::printf("side %d load %d\n%s", s, result, logText[s].c_str());
            DumpAscii("probe", p);
            std::printf("%s\n", logText[s].c_str());
            cur = -1;
        }
        return 0;
    }
    char cwd[MAX_PATH];
    std::snprintf(cwd, sizeof cwd, "%s", std::getenv("TEMP"));
    SetCurrentDirectoryA(cwd);
    for (int round = 0; round < rounds; ++round) {
        const unsigned seed = rng();
        for (int s = 0; s < 2; ++s) {
            cur = s;
            logText[s].clear();
            names[s].clear();
            blockSize[s].clear();
            allocCount[s] = 0;
            RunAscii(s, seed);
            cur = -1;
        }
        Compare(round);
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
