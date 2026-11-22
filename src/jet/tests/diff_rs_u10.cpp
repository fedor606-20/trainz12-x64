// Usage: diff_rs_u10.exe <bin dir>. Compare pixel conversion, scratch storage and repitching in Win32.
#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using Trace = std::vector<int>;
static Trace* trace;

template <class F>
static F Proc(HMODULE module, const char* name)
{
    const auto proc = GetProcAddress(module, name);
    if (!proc) {
        std::printf("missing export: %s\n", name);
        std::exit(2);
    }
    return reinterpret_cast<F>(proc);
}

struct Format {
    unsigned char id, bits, channels[10];
};
static Format Make(unsigned id, unsigned bits)
{
    Format f{};
    f.id = static_cast<unsigned char>(id);
    f.bits = static_cast<unsigned char>(bits);
    return f;
}
static unsigned Bpp(const Format& f) { return f.bits >> 3; }
static unsigned Bits(unsigned id)
{
    switch (id) {
    case 5: case 6: return 24;
    case 7: case 8: case 14: case 15: case 22: return 16;
    case 13: case 16: return 8;
    default: return 32;
    }
}

using Raw = bool(__cdecl*)(const Format&, const Format&, void*, void*, int, int, int, int);
using AllocFn = bool(__cdecl*)(const Format&, const Format&, void*, void*&, int, int, int);
using ScratchFn = bool(__cdecl*)(const Format&, const Format&, void*&, int&, int&, int&, int, int);
using RepitchFn = void(__cdecl*)(Format&, void*&, int&, int&, int&, int, int);
using FlushFn = void(__cdecl*)();

struct Api {
    Raw raw;
    AllocFn alloc;
    ScratchFn scratch;
    RepitchFn repitch;
    FlushFn flush;
};
static Api api;
static void Load(HMODULE m)
{
    api.raw = Proc<Raw>(m, "?Convert@Jet@@YA_NABVDisplayFormat@1@0PAX1HHHH@Z");
    api.alloc = Proc<AllocFn>(m, "?Convert@Jet@@YA_NABVDisplayFormat@1@0PAXAAPAXHHH@Z");
    api.scratch = Proc<ScratchFn>(m, "?Convert@Jet@@YA_NABVDisplayFormat@1@0AAPAXAAH22HH@Z");
    api.repitch = Proc<RepitchFn>(m, "?Repitch@Jet@@YAXAAVDisplayFormat@1@AAPAXAAH22HH@Z");
    api.flush = Proc<FlushFn>(m, "?FlushScratchBuffer@Jet@@YAXXZ");
}

static void Record(const unsigned char* p, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) trace->push_back(p[i]);
}

static void RawCases()
{
    static const unsigned pairs[][2] = {
        {1, 2}, {1, 7}, {1, 21}, {1, 22}, {2, 1}, {2, 7}, {2, 21}, {2, 22},
        {3, 4}, {3, 5}, {3, 6}, {3, 8}, {4, 3}, {4, 5}, {4, 6},
        {5, 3}, {5, 4}, {5, 6}, {5, 8}, {6, 5}, {6, 8}, {13, 3}, {13, 4}, {13, 6},
        {14, 3}, {14, 4}, {14, 6}, {14, 13}, {15, 1}, {15, 2}, {16, 1}, {16, 2},
        {21, 1}, {21, 2},
    };
    for (unsigned k = 0; k < sizeof(pairs) / sizeof(pairs[0]); ++k) {
        trace->push_back(-1000 - static_cast<int>(k));
        const Format from = Make(pairs[k][0], Bits(pairs[k][0]));
        const Format to = Make(pairs[k][1], Bits(pairs[k][1]));
        std::array<unsigned char, 128> src{}, dst{};
        for (unsigned i = 0; i < src.size(); ++i) src[i] = static_cast<unsigned char>(i * 17 + k);
        dst.fill(0x5a);
        const bool ok = api.raw(from, to, src.data(), dst.data(), 5, 4, 3, 2);
        trace->push_back(ok);
        Record(dst.data(), 48);
    }

    const Format a = Make(1, 32), b = Make(3, 24);
    std::array<unsigned char, 128> src{}, dst{};
    for (unsigned i = 0; i < src.size(); ++i) src[i] = static_cast<unsigned char>(i + 1);
    dst.fill(0x5a);
    trace->push_back(api.raw(a, b, src.data(), dst.data(), 0, 4, 3, 2));
    Record(dst.data(), 48);

    for (unsigned toBits : {15u, 16u}) {
        trace->push_back(-2000 - static_cast<int>(toBits));
        const Format from = Make(0, 32), to = Make(0, toBits);
        dst.fill(0x5a);
        trace->push_back(api.raw(from, to, src.data(), dst.data(), 5, 4, 3, 2));
        Record(dst.data(), 48);
    }
}

static void Wrappers()
{
    api.flush();
    const Format from = Make(1, 32), to = Make(1, 32);
    std::array<unsigned char, 128> src{};
    for (unsigned i = 0; i < src.size(); ++i) src[i] = static_cast<unsigned char>(i * 7 + 3);

    void* first = nullptr;
    trace->push_back(api.alloc(from, to, src.data(), first, 5, 3, 2));
    Record(static_cast<const unsigned char*>(first), 24);

    void* data = src.data();
    int pitch = 5, a = 1, b = 2;
    trace->push_back(api.scratch(from, to, data, pitch, a, b, 3, 2));
    void* smallPtr = data;
    Record(static_cast<const unsigned char*>(data), 24);
    trace->insert(trace->end(), {pitch, a, b});

    data = src.data();
    pitch = 5;
    a = b = 0;
    trace->push_back(api.scratch(from, to, data, pitch, a, b, 2, 1));
    trace->push_back(data == smallPtr);  // no shrink
    Record(static_cast<const unsigned char*>(data), 24);

    data = src.data();
    pitch = 5;
    a = 0;
    b = 0;
    trace->push_back(api.scratch(from, to, data, pitch, a, b, 9, 3));
    trace->push_back(data != smallPtr);  // grew
    Record(static_cast<const unsigned char*>(data), 24);

    // Repitch from base+11: (b * pitch + a) * bpp = 11
    Format l8 = Make(13, 8);
    std::array<unsigned char, 128> repitchSrc{};
    for (unsigned i = 0; i < repitchSrc.size(); ++i) repitchSrc[i] = static_cast<unsigned char>(i * 3 + 1);
    void* p = repitchSrc.data() + 5;
    int rp = 5, ra = 1, rb = 2;
    api.repitch(l8, p, rp, ra, rb, 3, 2);
    trace->insert(trace->end(), {rp, ra, rb});
    // A later supported scratch conversion is small enough not to reallocate; its pointer is the scratch base.
    void* base = repitchSrc.data();
    int bp = 3, ba = 0, bb = 0;
    const Format srcFmt = Make(1, 32), dstFmt = Make(2, 32);
    trace->push_back(api.scratch(srcFmt, dstFmt, base, bp, ba, bb, 1, 1));
    trace->push_back(static_cast<int>(static_cast<unsigned char*>(p) - static_cast<unsigned char*>(base)));
    Record(static_cast<const unsigned char*>(base), 8);

    void* before = base;
    api.flush();
    api.flush();
    base = repitchSrc.data();
    bp = 3;
    ba = bb = 0;
    trace->push_back(api.scratch(srcFmt, dstFmt, base, bp, ba, bb, 1, 1));
    trace->push_back(base != before);
}

static Trace Run(HMODULE m)
{
    Load(m);
    Trace result;
    trace = &result;
    RawCases();
    Wrappers();
    trace = nullptr;
    return result;
}

int main(int argc, char** argv)
{
    if (argc != 2 || !SetDllDirectoryA(argv[1])) {
        std::puts("usage: diff_rs_u10.exe <bin dir>");
        return 2;
    }
    const HMODULE original = LoadLibraryA("RenderSystem_7c_orig.dll");
    const HMODULE rebuilt = LoadLibraryA("RenderSystem_7c.dll");
    if (!original || !rebuilt) {
        std::printf("cannot load RenderSystem DLLs: %lu\n", GetLastError());
        return 2;
    }
    const auto baseline = Run(original);
    const auto port = Run(rebuilt);
    if (baseline != port) {
        size_t i = 0;
        while (i < baseline.size() && i < port.size() && baseline[i] == port[i]) ++i;
        std::printf("pixel trace mismatch at %zu (sizes %zu/%zu)\n", i, baseline.size(), port.size());
        if (i < baseline.size() && i < port.size())
            std::printf("original=%d rebuilt=%d\n", baseline[i], port[i]);
        return 1;
    }
    std::printf("pixel conversions: %zu matching trace entries\n", port.size());
    return 0;
}
