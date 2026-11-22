// Usage: diff_rs_u9.exe <bin dir>. Compare Display state and backend calls in Win32.
#include <windows.h>

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

struct Rect { int x, y, w, h; };
struct Size { int w, h; };
struct Color { float r, g, b, a; };
struct MaterialState { float channels[17]; };
using Trace = std::vector<int>;
static Trace* trace;

template<class F>
static F Proc(HMODULE module, const char* name)
{
    const auto proc = GetProcAddress(module, name);
    if (!proc) {
        std::printf("missing export: %s\n", name);
        std::exit(2);
    }
    return reinterpret_cast<F>(proc);
}

template<class F>
static F Ordinal(HMODULE module, unsigned ordinal)
{
    const auto proc = GetProcAddress(module, MAKEINTRESOURCEA(ordinal));
    if (!proc) {
        std::printf("missing export ordinal: %u\n", ordinal);
        std::exit(2);
    }
    return reinterpret_cast<F>(proc);
}

template<class T, int slot>
static void __cdecl Value(void*, T value) { trace->insert(trace->end(), {slot, std::bit_cast<int>(value)}); }
template<int slot>
static void __cdecl Mode(void*, bool value) { trace->insert(trace->end(), {slot, value}); }
template<int slot>
static void __cdecl ObserveRect(void*, const Rect* r) { trace->insert(trace->end(), {slot, r->x, r->y, r->w, r->h}); }
static void __cdecl Projection(void*, float a, float b, float c, float d, float e, float f)
{
    trace->push_back(0x180);
    for (float value : {a, b, c, d, e, f}) trace->push_back(std::bit_cast<int>(value));
}
static void __cdecl ClearColor(void*, const Color* c)
{
    trace->push_back(0x1a0);
    for (float value : {c->r, c->g, c->b, c->a}) trace->push_back(std::bit_cast<int>(value));
}

static void __cdecl MaterialProperties(void*, const MaterialState* material)
{
    trace->push_back(0x21c);
    for (float value : material->channels) trace->push_back(std::bit_cast<int>(value));
}

struct Manager {
    const FARPROC* vptr;
    int references;
    int id;
};

static void __cdecl DeleteManager(const Manager* m) { trace->insert(trace->end(), {0x901, m->id, m->references}); }
static bool __cdecl InitManager(Manager* m, void* kernel)
{
    const Manager* installed = nullptr;
    std::memcpy(&installed, static_cast<const unsigned char*>(kernel) + 0x90, sizeof installed);
    trace->insert(trace->end(), {0x902, m->id, m->references, installed == m});
    return m->id == 1;
}

static void CheckKernelMaterial(HMODULE m)
{
    const auto get = Proc<Manager*(__cdecl*)(void*)>(m, "?GetMaterialManager@RenderKernel@Jet@@QAAPAVMaterialManager@2@XZ");
    const auto set = Proc<void(__cdecl*)(void*, Manager*)>(m, "?SetMaterialManager@RenderKernel@Jet@@QAAXPAVMaterialManager@2@@Z");
    const FARPROC table[]{reinterpret_cast<FARPROC>(DeleteManager), nullptr, reinterpret_cast<FARPROC>(InitManager)};
    alignas(4) unsigned char kernel[0x98]{};
    Manager a{table, 1, 1}, b{table, 1, 2};
    trace->push_back(!get(kernel));
    set(kernel, &a);
    trace->insert(trace->end(), {get(kernel) == &a, a.references});
    a.references = 1; // drop the test's reference; replacement now releases the last one
    set(kernel, &b);
    trace->insert(trace->end(), {get(kernel) == &b, a.references, b.references});
    set(kernel, &b); // even equal pointers are released, retained and initialized again
    trace->insert(trace->end(), {get(kernel) == &b, b.references});
    set(kernel, nullptr);
    trace->insert(trace->end(), {!get(kernel), b.references});
    set(kernel, nullptr);
}

template<int slot, class T>
static void __cdecl Pair(void*, int first, T second)
{
    trace->insert(trace->end(), {slot, first, std::bit_cast<int>(second)});
}

static unsigned char* replacementState;
static void __cdecl WriteMask(void* display, int target, unsigned long mask)
{
    unsigned char* current;
    std::memcpy(&current, static_cast<unsigned char*>(display) + 0x54, sizeof current);
    int previousMask;
    std::memcpy(&previousMask, current + 0x9c, sizeof previousMask);
    trace->insert(trace->end(), {0x1ac, target, std::bit_cast<int>(mask), previousMask, current[0xa0]});
    if (replacementState)
        std::memcpy(static_cast<unsigned char*>(display) + 0x54, &replacementState, sizeof replacementState);
}

static void CheckBlendDepth(HMODULE m)
{
    using SetBool = void(__cdecl*)(void*, bool);
    using GetBool = bool(__cdecl*)(const void*);
    using SetEnum = void(__cdecl*)(void*, int);
    using GetEnum = int(__cdecl*)(const void*);
    const auto setMask = Proc<void(__cdecl*)(void*, unsigned long)>(m, "?SetColorWriteMask@Display@Jet@@QAAXK@Z");
    const auto getMask = Proc<unsigned long(__cdecl*)(const void*)>(m, "?GetColorWriteMask@Display@Jet@@QBAKXZ");
    const auto setDepthFunc = Proc<SetEnum>(m, "?SetDepthFunction@Display@Jet@@QAAXW4DFunction@2@@Z");
    const auto getDepthFunc = Proc<GetEnum>(m, "?GetDepthFunction@Display@Jet@@QBA?AW4DFunction@2@XZ");
    const auto setBlendFunc = Proc<void(__cdecl*)(void*, int, int)>(m, "?SetAlphaBlendFunction@Display@Jet@@QAAXW4Factor@Src@2@W43Dst@2@@Z");
    const auto getBlendFunc = Proc<void(__cdecl*)(const void*, int*, int*)>(m, "?GetAlphaBlendFunction@Display@Jet@@QBAXAAW4Factor@Src@2@AAW43Dst@2@@Z");
    const auto setBlendEq = Proc<SetEnum>(m, "?SetAlphaBlendEquation@Display@Jet@@QAAXW4DBlendEquation@2@@Z");
    const auto getBlendEq = Proc<GetEnum>(m, "?GetAlphaBlendEquation@Display@Jet@@QBA?AW4DBlendEquation@2@XZ");
    const auto setAlphaFunc = Proc<void(__cdecl*)(void*, int, float)>(m, "?SetAlphaTestFunction@Display@Jet@@QAAXW4DFunction@2@M@Z");
    const auto getAlphaFunc = Proc<GetEnum>(m, "?GetAlphaTestFunction@Display@Jet@@QBA?AW4DFunction@2@XZ");
    const auto getAlphaRef = Proc<float(__cdecl*)(const void*)>(m, "?GetAlphaTestReference@Display@Jet@@QBAMXZ");
    const SetBool setters[]{
        Proc<SetBool>(m, "?SetDepthWriteMode@Display@Jet@@QAAX_N@Z"),
        Proc<SetBool>(m, "?SetDepthTestMode@Display@Jet@@QAAX_N@Z"),
        Proc<SetBool>(m, "?SetDepthClamp@Display@Jet@@QAAX_N@Z"),
        Proc<SetBool>(m, "?SetAlphaTestMode@Display@Jet@@QAAX_N@Z"),
        Proc<SetBool>(m, "?SetAlphaBlendMode@Display@Jet@@QAAX_N@Z"),
    };
    const GetBool getters[]{
        Proc<GetBool>(m, "?GetDepthWriteMode@Display@Jet@@QBA_NXZ"),
        Proc<GetBool>(m, "?GetDepthTestMode@Display@Jet@@QBA_NXZ"),
        Proc<GetBool>(m, "?GetDepthClamp@Display@Jet@@QBA_NXZ"),
        Proc<GetBool>(m, "?GetAlphaTestMode@Display@Jet@@QBA_NXZ"),
        Proc<GetBool>(m, "?GetAlphaBlendMode@Display@Jet@@QBA_NXZ"),
    };

    FARPROC table[0x1ec / 4 + 1]{};
    table[0x1ac / 4] = reinterpret_cast<FARPROC>(WriteMask);
    table[0x1b0 / 4] = reinterpret_cast<FARPROC>(Value<int, 0x1b0>);
    table[0x1b4 / 4] = reinterpret_cast<FARPROC>(Mode<0x1b4>);
    table[0x1b8 / 4] = reinterpret_cast<FARPROC>(Mode<0x1b8>);
    table[0x1dc / 4] = reinterpret_cast<FARPROC>(Pair<0x1dc, int>);
    table[0x1e0 / 4] = reinterpret_cast<FARPROC>(Value<int, 0x1e0>);
    table[0x1e4 / 4] = reinterpret_cast<FARPROC>(Mode<0x1e4>);
    table[0x1e8 / 4] = reinterpret_cast<FARPROC>(Pair<0x1e8, float>);
    table[0x1ec / 4] = reinterpret_cast<FARPROC>(Mode<0x1ec>);
    alignas(4) unsigned char display[0x58]{}, state[0xd8]{}, alternate[0xd8]{};
    const auto* tablePtr = table;
    std::memcpy(display, &tablePtr, sizeof tablePtr);
    const auto* statePtr = state;
    std::memcpy(display + 0x54, &statePtr, sizeof statePtr);

    const float nan = std::bit_cast<float>(UINT32_C(0x7fc12345));
    const float inf = std::numeric_limits<float>::infinity();
    for (int round = 0; round < 64; ++round) {
        trace->push_back(-20000 - round);
        state[0xa0] = static_cast<unsigned char>(round * 5);
        state[0xa1] = static_cast<unsigned char>(round * 3);
        for (int i = 0; i < 5; ++i) {
            const bool start = getters[i](display);
            trace->push_back(start);
            setters[i](display, start);
            setters[i](display, !start);
            setters[i](display, !start);
            trace->push_back(getters[i](display));
        }
        const auto mask = static_cast<unsigned long>(0xf1230000u + round);
        setMask(display, mask);
        setMask(display, mask);
        setDepthFunc(display, round % 8);
        setDepthFunc(display, round % 8);
        setBlendFunc(display, round % 6, 0);
        setBlendFunc(display, round % 6, 1);
        setBlendFunc(display, round % 6, 1);
        setBlendEq(display, round % 5);
        setBlendEq(display, round % 5);
        int src, dst;
        getBlendFunc(display, &src, &dst);
        trace->insert(trace->end(), {std::bit_cast<int>(getMask(display)), getDepthFunc(display),
            getBlendEq(display), src, dst});
        for (float value : {nan, -inf, -1.0f, -0.0f, +0.0f, 0.25f, 1.0f, 2.0f, inf}) {
            setAlphaFunc(display, round % 8, value);
            setAlphaFunc(display, round % 8, value);
            trace->insert(trace->end(), {getAlphaFunc(display), std::bit_cast<int>(getAlphaRef(display))});
        }
        trace->insert(trace->end(), state, state + sizeof state);
    }
    replacementState = alternate;
    setMask(display, 13);
    replacementState = nullptr;
    trace->push_back(std::bit_cast<int>(getMask(display)));
    trace->insert(trace->end(), state, state + sizeof state);
    trace->insert(trace->end(), alternate, alternate + sizeof alternate);
}

template<int slot>
static void __cdecl LightingColor(void*, const Color* color)
{
    trace->push_back(slot);
    for (float value : {color->r, color->g, color->b, color->a})
        trace->push_back(std::bit_cast<int>(value));
}

static unsigned char* nextLightFlags;
static void __cdecl LightMode(void* display, int index, bool on)
{
    trace->insert(trace->end(), {0x224, index, on});
    if (!nextLightFlags) return;
    auto* object = static_cast<unsigned char*>(display);
    unsigned char* state;
    std::memcpy(&state, object + 0x54, sizeof state);
    std::memcpy(state + 0x110, &nextLightFlags, sizeof nextLightFlags);
    const int count = 1;
    std::memcpy(object + 0x50, &count, sizeof count);
    nextLightFlags = nullptr;
}

static void __cdecl LightingTransform(void*, const float* transform)
{
    trace->insert(trace->end(), {0x228, std::bit_cast<int>(transform[0]), std::bit_cast<int>(transform[15])});
}

static void __cdecl LightData(void*, int index, const float* light)
{
    trace->insert(trace->end(), {0x22c, index, std::bit_cast<int>(light[0])});
}

static void __cdecl FogRange(void*, float start, float end)
{
    trace->insert(trace->end(), {0x204, std::bit_cast<int>(start), std::bit_cast<int>(end)});
}

static void CheckLighting(HMODULE m)
{
    using SetBool = void(__cdecl*)(void*, bool);
    using GetBool = bool(__cdecl*)(const void*);
    using SetColor = void(__cdecl*)(void*, const Color*);
    using GetColor = const Color*(__cdecl*)(const void*);
    const SetBool modes[]{
        Proc<SetBool>(m, "?SetLightingMode@Display@Jet@@QAAX_N@Z"),
        Proc<SetBool>(m, "?SetSeperateSpecularMode@Display@Jet@@QAAX_N@Z"),
        Proc<SetBool>(m, "?SetLocalViewSpecularMode@Display@Jet@@QAAX_N@Z"),
    };
    const GetBool getters[]{
        Proc<GetBool>(m, "?GetLightingMode@Display@Jet@@QBA_NXZ"),
        Proc<GetBool>(m, "?GetSeperateSpecularMode@Display@Jet@@QAA_NXZ"),
        Proc<GetBool>(m, "?GetLocalViewSpecularMode@Display@Jet@@QBA_NXZ"),
    };
    const auto fogMode = Proc<void(__cdecl*)(void*, int)>(m, "?SetFogMode@Display@Jet@@QAAXW4DFogMode@2@@Z");
    const auto getFogMode = Proc<int(__cdecl*)(const void*)>(m, "?GetFogMode@Display@Jet@@QBA?AW4DFogMode@2@XZ");
    const auto fogDensity = Proc<void(__cdecl*)(void*, float)>(m, "?SetFogDensity@Display@Jet@@QAAXM@Z");
    const auto getFogDensity = Proc<float(__cdecl*)(const void*)>(m, "?GetFogDensity@Display@Jet@@QBAMXZ");
    const auto fogRange = Proc<void(__cdecl*)(void*, float, float)>(m, "?SetFogRange@Display@Jet@@QAAXMM@Z");
    const auto getFogRange = Proc<void(__cdecl*)(const void*, float*, float*)>(m, "?GetFogRange@Display@Jet@@QBAXAAM0@Z");
    const auto fogColor = Proc<SetColor>(m, "?SetFogColor@Display@Jet@@QAAXABVColor@2@@Z");
    const auto getFogColor = Proc<GetColor>(m, "?GetFogColor@Display@Jet@@QBAABVColor@2@XZ");
    const auto ambient = Proc<SetColor>(m, "?SetGlobalAmbient@Display@Jet@@QAAXABVColor@2@@Z");
    const auto getAmbient = Proc<GetColor>(m, "?GetGlobalAmbient@Display@Jet@@QBAABVColor@2@XZ");
    const auto converted = Proc<unsigned long(__cdecl*)(const void*)>(m, "?GetGlobalAmbientConverted@Display@Jet@@QBAKXZ");
    const auto setLight = Proc<bool(__cdecl*)(void*, int, bool)>(m, "?SetLightMode@Display@Jet@@QAA_NH_N@Z");
    const auto getLight = Proc<bool(__cdecl*)(const void*, int)>(m, "?GetLightMode@Display@Jet@@QBA_NH@Z");
    const auto setData = Proc<bool(__cdecl*)(void*, int, const float*, bool)>(m, "?SetLightState@Display@Jet@@QAA_NHABVDisplayLight@2@_N@Z");
    const auto count = Proc<int(__cdecl*)(const void*)>(m, "?GetNumberOfHardwareLights@Display@Jet@@QBAHXZ");
    const auto disable = Proc<void(__cdecl*)(void*)>(m, "?DisableAllLights@Display@Jet@@QAAXXZ");
    const auto begin = Proc<void(__cdecl*)(void*, int, const float*)>(m, "?StartLightingUpdate@Display@Jet@@UAAXHABVMatrix4@2@@Z");
    const auto end = Proc<void(__cdecl*)(void*)>(m, "?EndLightingUpdate@Display@Jet@@UAAXXZ");
    // this is the original's live static: our U11 export is a mirror, and the code under test reads the original
    auto* format = Proc<int*>(GetModuleHandleA("RenderSystem_7c_orig.dll"),
        "?internalColorFormat@Display@Jet@@1W4DFormatId@2@A");
    const int savedFormat = *format;
    FARPROC table[0x230 / 4 + 1]{};
    table[0x1f0 / 4] = reinterpret_cast<FARPROC>(Mode<0x1f0>);
    table[0x1f4 / 4] = reinterpret_cast<FARPROC>(Mode<0x1f4>);
    table[0x1f8 / 4] = reinterpret_cast<FARPROC>(Mode<0x1f8>);
    table[0x1fc / 4] = reinterpret_cast<FARPROC>(Value<int, 0x1fc>);
    table[0x200 / 4] = reinterpret_cast<FARPROC>(Value<float, 0x200>);
    table[0x204 / 4] = reinterpret_cast<FARPROC>(FogRange);
    table[0x208 / 4] = reinterpret_cast<FARPROC>(LightingColor<0x208>);
    table[0x224 / 4] = reinterpret_cast<FARPROC>(LightMode);
    table[0x228 / 4] = reinterpret_cast<FARPROC>(LightingTransform);
    table[0x22c / 4] = reinterpret_cast<FARPROC>(LightData);
    table[0x230 / 4] = reinterpret_cast<FARPROC>(LightingColor<0x230>);

    alignas(4) unsigned char display[0x58]{}, state[0x114]{};
    const auto* tablePtr = table;
    const auto* statePtr = state;
    std::memcpy(display, &tablePtr, sizeof tablePtr);
    std::memcpy(display + 0x54, &statePtr, sizeof statePtr);
    const int numLights = 4;
    const float transform[16]{2.0f};
    const float light[]{3.0f};
    const float nan = std::bit_cast<float>(UINT32_C(0x7fc12345));
    for (int round = 0; round < 64; ++round) {
        trace->push_back(-30000 - round);
        unsigned char flags[]{1, 0, 1, 1}, alternate[]{1, 1, 1, 1};
        auto* flagsPtr = flags;
        std::memcpy(state + 0x110, &flagsPtr, sizeof flagsPtr);
        std::memcpy(display + 0x50, &numLights, sizeof numLights);
        state[0xa1] = static_cast<unsigned char>(round);
        for (int i = 0; i < 3; ++i) {
            const bool value = (round >> i) & 1;
            modes[i](display, value);
            modes[i](display, value);
            trace->push_back(getters[i](display));
        }
        *format = round % 3;
        const float density = (round % 3 == 0) ? nan : (round & 1) ? 0.5f : -0.0f;
        const Color color{density, 0.5f, 1.25f, 0.75f};
        for (int repeat = 0; repeat < 2; ++repeat) {
            fogMode(display, round % 4);
            fogDensity(display, density);
            fogRange(display, density, 2.0f);
            fogColor(display, &color);
            ambient(display, &color);
        }
        float start, finish;
        getFogRange(display, &start, &finish);
        trace->insert(trace->end(), {getFogMode(display), std::bit_cast<int>(getFogDensity(display)),
            std::bit_cast<int>(start), std::bit_cast<int>(finish), std::bit_cast<int>(converted(display)),
            static_cast<int>(reinterpret_cast<const unsigned char*>(getFogColor(display)) - state),
            static_cast<int>(reinterpret_cast<const unsigned char*>(getAmbient(display)) - state)});
        trace->insert(trace->end(), {count(display), getLight(display, -1), getLight(display, 4),
            setLight(display, -1, true), setData(display, 4, light, true)});
        trace->insert(trace->end(), {setLight(display, 1, true), getLight(display, 1),
            setLight(display, 1, true), setData(display, 1, light, true)});
        begin(display, -1, transform);
        begin(display, 2, transform);
        if (round & 1) nextLightFlags = alternate;
        disable(display);
        end(display);
        trace->push_back(count(display));
        trace->insert(trace->end(), flags, flags + numLights);
        trace->insert(trace->end(), alternate, alternate + numLights);
        trace->insert(trace->end(), state, state + 0x110);
        nextLightFlags = nullptr;
    }
    *format = savedFormat;
}

static void CheckPackedColor(HMODULE m)
{
    const auto get = Proc<int(__cdecl*)()>(m, "?GetInternalColorFormat@Display@Jet@@SA?AW4DFormatId@2@XZ");
    const auto encode = Proc<unsigned long(__cdecl*)(const Color*)>(m, "?ConvertColor@Display@Jet@@SAKABVColor@2@@Z");
    const auto decode = Proc<Color*(__cdecl*)(Color*, unsigned long)>(m, "?ConvertColor@Display@Jet@@SA?AVColor@2@K@Z");
    // this is the original's live static: our U11 export is a mirror, and the code under test reads the original
    auto* format = Proc<int*>(GetModuleHandleA("RenderSystem_7c_orig.dll"),
        "?internalColorFormat@Display@Jet@@1W4DFormatId@2@A");
    const int savedFormat = *format;
    const float nan = std::bit_cast<float>(UINT32_C(0x7fc12345));
    for (const int mode : {-1, 0, 1, 2, 3}) {
        *format = mode;
        trace->push_back(get());
        for (unsigned i = 0; i < 256; ++i) {
            const unsigned long packed = (i << 24) | ((i ^ 63) << 16) | ((i ^ 127) << 8) | (i ^ 255);
            Color out{-1.0f, -2.0f, -3.0f, -4.0f};
            trace->push_back(decode(&out, packed) == &out);
            for (float value : {out.r, out.g, out.b, out.a})
                trace->push_back(std::bit_cast<int>(value));
            const float ramp = static_cast<float>(i) / 255.0f;
            const Color input{ramp, 0.25f, 1.0f - ramp, ramp};
            trace->push_back(std::bit_cast<int>(encode(&input)));
        }
        const Color edge{nan, 0.5f, 2.0f, -0.0f};
        trace->push_back(std::bit_cast<int>(encode(&edge)));
    }
    *format = savedFormat;
}

static void CheckColorArrays(HMODULE m)
{
    const auto encode = Proc<void(__cdecl*)(const Color*, unsigned long*, int)>(
        m, "?ConvertColorArray@Display@Jet@@SAXPBVColor@2@PAKH@Z");
    const auto decode = Proc<void(__cdecl*)(unsigned long*, Color*, int)>(
        m, "?ConvertColorArray@Display@Jet@@SAXPAKPAVColor@2@H@Z");
    const auto swap = Proc<void(__cdecl*)(unsigned long*, int)>(
        m, "?ConvertColorArray@Display@Jet@@SAXPAKH@Z");
    const auto strided = Proc<void(__cdecl*)(int, void*, int)>(
        m, "?ConvertColorArray@Display@Jet@@SAXV?$VertexItemPtr@KV?$VertexItemPtr@$$CBKVVIPConstHelper@Jet@@@Jet@@@2@H@Z");
    // this is the original's live static: our U11 export is a mirror, and the code under test reads the original
    auto* format = Proc<int*>(GetModuleHandleA("RenderSystem_7c_orig.dll"),
        "?internalColorFormat@Display@Jet@@1W4DFormatId@2@A");
    const int savedFormat = *format;
    const float nan = std::bit_cast<float>(UINT32_C(0x7fc12345));
    const Color source[]{
        {nan, -0.0f, 2.0f, 0.5f}, {1.0f, 0.5f, 0.1f, 0.0f}, {0.125f, 0.25f, 0.5f, 0.75f},
        {0.9f, 0.4f, 0.3f, 0.6f}, {-2.0f, 0.6f, 0.7f, 2.0f}, {0.2f, 0.6f, 0.3f, 0.61f},
        {0.33f, nan, 0.1f, 0.5f}, {0.0f, 1.0f, nan, 0.4f}, {1.0f, 1.0f, 1.0f, nan},
    };
    for (int mode : {-1, 0, 1, 2, 3}) {
        *format = mode;
        for (int n : {-1, 0, 1, 3, 4, 5, 9}) {
            unsigned long packed[11], input[11];
            Color decoded[11];
            for (unsigned i = 0; i < 11; ++i) {
                packed[i] = 0xdeadbeeful;
                input[i] = 0x010e3367ul * (i + 1);
                decoded[i] = {-1.0f, -2.0f, -3.0f, -4.0f};
            }
            encode(source, packed + 1, n);
            decode(input + 1, decoded + 1, n);
            swap(input + 1, n);
            for (unsigned i = 0; i < 11; ++i) {
                trace->push_back(std::bit_cast<int>(packed[i]));
                trace->push_back(std::bit_cast<int>(input[i]));
                for (float value : {decoded[i].r, decoded[i].g, decoded[i].b, decoded[i].a})
                    trace->push_back(std::bit_cast<int>(value));
            }
        }
        for (int stride : {-7, 0, 4, 7}) {
            for (int n : {-1, 0, 4, 5}) {
                unsigned char bytes[40];
                for (unsigned i = 0; i < sizeof bytes; ++i)
                    bytes[i] = static_cast<unsigned char>(i + 101);
                strided(stride, bytes + (stride < 0 ? 31 : 3), n);
                trace->insert(trace->end(), bytes, bytes + sizeof bytes);
            }
        }
    }
    *format = savedFormat;
}

static void CheckGeometryModes(HMODULE m)
{
    using SetBool = void(__cdecl*)(void*, bool);
    using GetBool = bool(__cdecl*)(const void*);
    const char* names[]{"WireFrameMode", "OutlinesMode", "VertexNormalsMode", "TriangleNormalsMode",
        "LocalAxesMode", "ChunkColorMode", "ChunkVertexCountMode", "StaticGeometryDisplayMode",
        "DynamicGeometryDisplayMode"};
    SetBool setters[9];
    GetBool getters[9];
    for (int i = 0; i < 9; ++i) {
        char symbol[96];
        std::snprintf(symbol, sizeof symbol, "?Get%s@Display@Jet@@QBA_NXZ", names[i]);
        getters[i] = Proc<GetBool>(m, symbol);
        std::snprintf(symbol, sizeof symbol, "?Set%s@Display@Jet@@QAAX_N@Z", names[i]);
        setters[i] = Proc<SetBool>(m, symbol);
    }
    const auto setLength = Proc<void(__cdecl*)(void*, float)>(m, "?SetNormalLength@Display@Jet@@QAAXM@Z");
    const auto getLength = Proc<float(__cdecl*)(const void*)>(m, "?GetNormalLength@Display@Jet@@QBAMXZ");
    FARPROC table[0x18c / 4 + 1]{};
    table[0x18c / 4] = reinterpret_cast<FARPROC>(Mode<0x18c>);
    alignas(4) unsigned char display[0x80]{}, state[0xa4]{};
    const auto* tablePtr = table;
    const auto* statePtr = state;
    std::memcpy(display, &tablePtr, sizeof tablePtr);
    std::memcpy(display + 0x54, &statePtr, sizeof statePtr);
    for (int round = 0; round < 64; ++round) {
        display[0x71] = static_cast<unsigned char>(round * 7);
        state[0xa0] = static_cast<unsigned char>(round * 5);
        for (int i = 0; i < 9; ++i) {
            const bool before = getters[i](display);
            trace->push_back(before);
            setters[i](display, before);
            setters[i](display, !before);
            setters[i](display, !before);
            trace->insert(trace->end(), {getters[i](display), display[0x71], state[0xa0]});
        }
        for (float value : {-0.0f, 0.0f, static_cast<float>(round), std::bit_cast<float>(UINT32_C(0x7fc12345))}) {
            setLength(display, value);
            trace->push_back(std::bit_cast<int>(getLength(display)));
        }
        trace->insert(trace->end(), display + 0x6c, display + sizeof display);
    }
}

static bool acceptStencil;
static bool __cdecl StencilMode(void*, bool on, bool twoSided)
{
    trace->insert(trace->end(), {0x1bc, on, twoSided, acceptStencil});
    return acceptStencil;
}

static void __cdecl StencilMask(void* display, int face, unsigned long mask)
{
    trace->insert(trace->end(), {0x1c0, face, std::bit_cast<int>(mask)});
    if (replacementState)
        std::memcpy(static_cast<unsigned char*>(display) + 0x54, &replacementState, sizeof replacementState);
}

static void __cdecl StencilFunctionCall(void*, int face, int function, int reference, unsigned long mask)
{
    trace->insert(trace->end(), {0x1c4, face, function, reference, std::bit_cast<int>(mask)});
}

static void __cdecl StencilOperationCall(void*, int face, int fail, int depthFail, int pass)
{
    trace->insert(trace->end(), {0x1c8, face, fail, depthFail, pass});
}

static void __cdecl StencilSetupCall(void*, int face, int function, int reference, unsigned long mask,
                                    int fail, int depthFail, int pass)
{
    trace->insert(trace->end(), {0x1cc, face, function, reference, std::bit_cast<int>(mask), fail, depthFail, pass});
}

static void __cdecl PolygonOffset(void*, float factor, float units)
{
    trace->insert(trace->end(), {0x234, std::bit_cast<int>(factor), std::bit_cast<int>(units)});
}

static void __cdecl PolygonMode(void*, int mode, bool on)
{
    trace->insert(trace->end(), {0x238, mode, on});
}

static void CheckStencilPolygon(HMODULE m)
{
    using GetInt = int(__cdecl*)(const void*);
    using GetBool = bool(__cdecl*)(const void*);
    const auto getBits = Proc<GetInt>(m, "?GetNumStencilBits@Display@Jet@@UBAHXZ");
    const auto getMode = Proc<GetBool>(m, "?GetStencilTestMode@Display@Jet@@QBA_NXZ");
    const auto mode = Proc<void(__cdecl*)(void*, bool, bool)>(m, "?SetStencilTestMode@Display@Jet@@QAAX_N0@Z");
    const auto getMask = Proc<unsigned long(__cdecl*)(const void*, int)>(m, "?GetStencilWriteMask@Display@Jet@@QBAKW4DFace@2@@Z");
    const auto mask = Proc<void(__cdecl*)(void*, unsigned long, int)>(m, "?SetStencilWriteMask@Display@Jet@@QAAXKW4DFace@2@@Z");
    const auto function = Proc<void(__cdecl*)(void*, int, int, unsigned long, int)>(
        m, "?SetStencilFunction@Display@Jet@@QAAXW4DFunction@2@HKW4DFace@2@@Z");
    const auto operation = Proc<void(__cdecl*)(void*, int, int, int, int)>(
        m, "?SetStencilOperation@Display@Jet@@QAAXW4DOperation@2@00W4DFace@2@@Z");
    const auto setup = Proc<void(__cdecl*)(void*, int, int, unsigned long, int, int, int, int)>(
        m, "?SetStencilSetup@Display@Jet@@QAAXW4DFunction@2@HKW4DOperation@2@11W4DFace@2@@Z");
    const auto polygon = Proc<void(__cdecl*)(void*, float, float)>(m, "?SetPolygonOffset@Display@Jet@@QAAXMM@Z");
    const auto enable = Proc<void(__cdecl*)(void*, int, bool)>(m, "?EnablePolygonOffset@Display@Jet@@QAAXW4DPolygonOffsetMode@2@_N@Z");
    const auto getCombiner = Proc<GetInt>(m, "?GetMaxTextureCombinerStages@Display@Jet@@QAAHXZ");
    const auto getImage = Proc<GetInt>(m, "?GetMaxTextureImageUnits@Display@Jet@@QAAHXZ");
    const auto getCoord = Proc<GetInt>(m, "?GetMaxTextureCoordUnits@Display@Jet@@QAAHXZ");
    const auto getCoverage = Proc<GetBool>(m, "?GetAlphaToCoverage@Display@Jet@@QAA_NXZ");

    FARPROC table[0x238 / 4 + 1]{};
    table[0x1bc / 4] = reinterpret_cast<FARPROC>(StencilMode);
    table[0x1c0 / 4] = reinterpret_cast<FARPROC>(StencilMask);
    table[0x1c4 / 4] = reinterpret_cast<FARPROC>(StencilFunctionCall);
    table[0x1c8 / 4] = reinterpret_cast<FARPROC>(StencilOperationCall);
    table[0x1cc / 4] = reinterpret_cast<FARPROC>(StencilSetupCall);
    table[0x234 / 4] = reinterpret_cast<FARPROC>(PolygonOffset);
    table[0x238 / 4] = reinterpret_cast<FARPROC>(PolygonMode);
    alignas(4) unsigned char display[0x220]{}, state[0x12c]{}, alternate[0x12c]{};
    const auto* tablePtr = table;
    const auto* statePtr = state;
    std::memcpy(display, &tablePtr, sizeof tablePtr);
    std::memcpy(display + 0x54, &statePtr, sizeof statePtr);
    const int bits = 8, caps[]{2, 4, 6};
    std::memcpy(display + 0x4c, &bits, sizeof bits);
    std::memcpy(display + 0x214, caps, sizeof caps);
    for (int round = 0; round < 64; ++round) {
        trace->push_back(-40000 - round);
        state[0xa0] = static_cast<unsigned char>(round * 5);
        trace->insert(trace->end(), {getBits(display), getCombiner(display), getImage(display),
            getCoord(display), getCoverage(display), getMode(display)});
        acceptStencil = true;
        mode(display, true, true);
        mode(display, true, true);
        acceptStencil = false;
        mode(display, true, false);
        trace->insert(trace->end(), {getMode(display), state[0xa0]});
        acceptStencil = true;
        mode(display, false, false);
        for (int face = 0; face < 3; ++face) {
            const unsigned long value = 0xf1230000ul + face + round;
            mask(display, value, face);
            mask(display, value, face);
            function(display, 4, -round, value ^ 7, face);
            function(display, 4, -round, value ^ 7, face);
            operation(display, 1, 2, 3, face);
            operation(display, 1, 2, 3, face);
            setup(display, 4, -round, value ^ 7, 1, 2, 3, face); // equal to separate setters
            setup(display, 7, round, value ^ 11, 4, 5, 2, face);
            setup(display, 7, round, value ^ 11, 4, 5, 2, face);
            trace->push_back(std::bit_cast<int>(getMask(display, face)));
        }
        const auto factor = (round & 1) ? std::bit_cast<float>(UINT32_C(0x7fc12345)) : 0.0f;
        polygon(display, factor, -0.0f);
        polygon(display, factor, +0.0f);
        polygon(display, factor, 1.0f);
        for (int index : {0, 1, 2, 3, 4, 5, 6}) {
            enable(display, index, true);
            enable(display, index, true);
        }
        enable(display, 5, false);
        trace->insert(trace->end(), state, state + sizeof state);
    }
    replacementState = alternate;
    mask(display, 713, 0);
    replacementState = nullptr;
    trace->insert(trace->end(), state, state + sizeof state);
    trace->insert(trace->end(), alternate, alternate + sizeof alternate);
}

static void __cdecl AntiAliasMode(void* display, bool on)
{
    const unsigned char* state;
    std::memcpy(&state, static_cast<unsigned char*>(display) + 0x54, sizeof state);
    trace->insert(trace->end(), {0x1d4, on, state[0xa1]});
}

static void CheckDisplayInfo(HMODULE m)
{
    using GetBool = bool(__cdecl*)(const void*);
    using GetPtr = const unsigned char*(__cdecl*)(const void*);
    using GetRecord = bool(__cdecl*)(const void*, void*);
    const GetBool flags[]{
        Proc<GetBool>(m, "?TextureCombinerSupport@Display@Jet@@QBA_NXZ"),
        Proc<GetBool>(m, "?VertexProgramSupport@Display@Jet@@QBA_NXZ"),
        Proc<GetBool>(m, "?HardwareAnimationSupport@Display@Jet@@QBA_NXZ"),
        Proc<GetBool>(m, "?IsBetweenBeginAndEndFrame@Display@Jet@@UAA_NXZ"),
        Proc<GetBool>(m, "?IsRenderableState@Display@Jet@@QBA_NXZ"),
        Proc<GetBool>(m, "?SupportsAntiAliasing@Display@Jet@@QBA_NXZ"),
        Proc<GetBool>(m, "?GetAntiAliasing@Display@Jet@@QBA_NXZ"),
    };
    const GetPtr pointers[]{
        Proc<GetPtr>(m, "?GetDisplaySelector@Display@Jet@@QBAPAVDisplaySelector@2@XZ"),
        Proc<GetPtr>(m, "?GetOS@Display@Jet@@QBAPAVOS@2@XZ"),
        Proc<GetPtr>(m, "?GetVertexBufferManager@Display@Jet@@QBAPAVVertexBufferManager@2@XZ"),
        Proc<GetPtr>(m, "?GetTextureCombinerManager@Display@Jet@@UBAPAVTextureCombinerManager@2@XZ"),
        Proc<GetPtr>(m, "?GetVertexProgramManager@Display@Jet@@UBAPAVVertexProgramManager@2@XZ"),
        Proc<GetPtr>(m, "?GetFrameStats@Display@Jet@@QBAABUDisplayFrameStats@12@XZ"),
        Proc<GetPtr>(m, "?GetCurrentFrameStats@Display@Jet@@QBAABUDisplayFrameStats@12@XZ"),
    };
    const GetRecord copies[]{
        Proc<GetRecord>(m, "?GetCurrentVideoMode@Display@Jet@@QBA_NAAUVideoMode@12@@Z"),
        Proc<GetRecord>(m, "?GetDefaultVideoMode@Display@Jet@@QBA_NAAUVideoMode@12@@Z"),
        Proc<GetRecord>(m, "?GetCurrentVideoFormat@Display@Jet@@QBA_NAAUVideoFormat@12@@Z"),
    };
    const GetRecord lists[]{
        Proc<GetRecord>(m, "?GetAllVideoModes@Display@Jet@@QBA_NAAPBV?$Array@UVideoMode@Display@Jet@@$0BE@@2@@Z"),
        Proc<GetRecord>(m, "?GetAllVideoFormats@Display@Jet@@QBA_NAAPBV?$Array@UVideoFormat@Display@Jet@@$0BE@@2@@Z"),
    };
    const auto screen = Proc<int(__cdecl*)(const void*)>(m, "?GetCurrentScreenMode@Display@Jet@@QBA?AW4ScreenMode@12@XZ");
    const auto frame = Proc<unsigned(__cdecl*)(const void*)>(m, "?GetCurrentFrame@Display@Jet@@QBAIXZ");
    const auto ratio = Proc<float(__cdecl*)(void*)>(m, "?GetPhysicalScreenRatio@Display@Jet@@QAAMXZ");
    const auto setAA = Proc<void(__cdecl*)(void*, bool)>(m, "?SetAntiAliasing@Display@Jet@@QAAX_N@Z");
    const auto aaModes = Proc<void(__cdecl*)(const void*, void*)>(m, "?GetAntiAliasModes@Display@Jet@@QBAXAAV?$Array@I$09@2@@Z");
    const auto kernel = GetModuleHandleA("KernelStandard_7c.dll");
    const auto allocate = Proc<void*(__cdecl*)(unsigned)>(kernel, "?New@Alloc@Jet@@SAPAXI@Z");
    const auto release = Proc<void(__cdecl*)(void*)>(kernel, "?Delete@Alloc@Jet@@SAXPAX@Z");

    FARPROC table[0x1d4 / 4 + 1]{};
    table[0x1d4 / 4] = reinterpret_cast<FARPROC>(AntiAliasMode);
    alignas(4) unsigned char display[0x300]{}, state[0xa4]{};
    const auto* tablePtr = table;
    const auto* statePtr = state;
    std::memcpy(display, &tablePtr, sizeof tablePtr);
    std::memcpy(display + 0x54, &statePtr, sizeof statePtr);
    for (unsigned i = 0; i < 0x4c; ++i)
        display[0x23c + i] = static_cast<unsigned char>(i + 13);
    unsigned tag = 0;
    for (int offset : {0x288, 0x44, 0x20c, 0x228, 0x224}) {
        const auto* ptr = display + 0x10 + (++tag * 4);
        std::memcpy(display + offset, &ptr, sizeof ptr);
    }
    for (auto get : pointers)
        trace->push_back(static_cast<int>(reinterpret_cast<std::uintptr_t>(get(display)) -
            reinterpret_cast<std::uintptr_t>(display)));
    for (auto get : copies) {
        unsigned char out[36];
        std::memset(out, 0xa5, sizeof out);
        trace->push_back(get(display, out));
        trace->insert(trace->end(), out, out + sizeof out);
    }
    const int mode = 2;
    const unsigned frameNumber = 0x12345678u;
    const float physicalRatio = 1.125f;
    std::memcpy(display + 0x22c, &mode, sizeof mode);
    std::memcpy(display + 0x3c, &frameNumber, sizeof frameNumber);
    std::memcpy(display + 0x6c, &physicalRatio, sizeof physicalRatio);
    trace->insert(trace->end(), {screen(display), std::bit_cast<int>(frame(display)),
                                  std::bit_cast<int>(ratio(display))});
    for (int round = 0; round < 32; ++round) {
        display[0x212] = (round & 1) != 0;
        display[0x211] = (round & 2) != 0;
        display[0x210] = (round & 4) != 0;
        display[0x70] = (round & 8) != 0;
        display[0x49] = (round & 16) != 0;
        state[0xa1] = static_cast<unsigned char>(round);
        for (auto get : flags) trace->push_back(get(display));
        setAA(display, true);
        setAA(display, true);
        setAA(display, false);
        trace->push_back(state[0xa1]);
    }

    struct Format { unsigned words[8]; } formats[30]{};
    for (unsigned i = 0; i < 30; ++i)
        formats[i].words[6] = ((i % 15) * 7) % 15;
    auto* formatPtr = formats;
    std::memcpy(display + 0x264, &formatPtr, sizeof formatPtr);
    for (unsigned size : {0u, 1u, 2u, 30u}) {
        std::memcpy(display + 0x230, &size, sizeof size);
        std::memcpy(display + 0x25c, &size, sizeof size);
        for (auto get : lists) {
            const unsigned char* list{};
            trace->push_back(get(display, &list));
            trace->push_back(static_cast<int>(list - display));
        }
        for (unsigned capacity : {0u, 3u, 10u, 21u}) {
            auto* data = capacity ? static_cast<unsigned*>(allocate(capacity * sizeof(unsigned))) : nullptr;
            for (unsigned i = 0; i < capacity; ++i) data[i] = 0xf1230000u + i;
            struct { unsigned size, capacity; unsigned* data; } out{capacity, capacity, data};
            aaModes(display, &out);
            trace->insert(trace->end(), {static_cast<int>(out.size), static_cast<int>(out.capacity), out.data == data});
            for (unsigned i = 0; i < out.size; ++i)
                trace->push_back(static_cast<int>(out.data[i]));
            if (out.data) release(out.data);
        }
    }
}

template<class T>
static T ReadField(const void* object, unsigned offset)
{
    T value;
    std::memcpy(&value, static_cast<const unsigned char*>(object) + offset, sizeof value);
    return value;
}

template<int slot, class... Args>
static void __cdecl ObserveView(void* display, Args... args)
{
    const auto* state = ReadField<const unsigned char*>(display, 0x54);
    trace->insert(trace->end(), {slot, ReadField<int>(state, 4), std::bit_cast<int>(args)...});
}

static void __cdecl ObserveMatrix(void* display, const float* matrix)
{
    const auto* state = ReadField<const unsigned char*>(display, 0x54);
    trace->insert(trace->end(), {0x19c, ReadField<int>(state, 4), std::bit_cast<int>(matrix[0]),
                                  std::bit_cast<int>(matrix[15])});
}

static void __cdecl ObserveTarget(void* display, const void* texture, unsigned index)
{
    trace->insert(trace->end(), {0x1d0, texture != nullptr, ReadField<const void*>(display, 0x5c) == texture,
                                  std::bit_cast<int>(index)});
}

static void __cdecl ObserveTextureMode(void* display, bool on)
{
    const auto* state = ReadField<const unsigned char*>(display, 0x54);
    trace->insert(trace->end(), {0x214, on, ReadField<unsigned char>(display, 0x4a), state[0x65]});
}

static void __cdecl ObserveProjectedTexture(void* display, int stage, bool on, bool alpha)
{
    const auto* state = ReadField<const unsigned char*>(display, 0x54);
    trace->insert(trace->end(), {0x218, stage, on, alpha, state[0x66]});
}

static void __cdecl ObserveTexGen(void* display, int stage, int mode, unsigned count)
{
    const auto* state = ReadField<const unsigned char*>(display, 0x54);
    trace->insert(trace->end(), {0x240, stage, mode, static_cast<int>(count),
                                  ReadField<int>(state, 0xb8), ReadField<int>(state, 0xbc)});
}

static void CheckViewAndTexture(HMODULE m)
{
    struct Params { int mode; float values[8]; };
    using GetBool = bool(__cdecl*)(const void*);
    using SetBool = void(__cdecl*)(void*, bool);
    const auto get = Proc<void(__cdecl*)(const void*, Params*)>(m, "?GetViewModeParams@Display@Jet@@QBAXAAUViewModeParams@12@@Z");
    const auto set = Proc<void(__cdecl*)(void*, const Params*)>(m, "?SetViewModeParams@Display@Jet@@QAAXABUViewModeParams@12@@Z");
    const auto normalized = Proc<GetBool>(m, "?IsNormalized2DViewport@Display@Jet@@QBA_NXZ");
    const auto transition = Proc<void(__cdecl*)(void*, float, float, float, float, float, float, float)>(
        m, "?Set3DTransitionalView@Display@Jet@@QAAXMMMMMMM@Z");
    const auto perspective = Proc<void(__cdecl*)(void*, float, float, float, float)>(m, "?Set3DView@Display@Jet@@QAAXMMMM@Z");
    const auto offset = Proc<void(__cdecl*)(void*, float, float, float, float, float)>(m, "?Set3DViewOffset@Display@Jet@@QAAXMMMMM@Z");
    const auto load = Proc<void(__cdecl*)(void*, const float*)>(m, "?LoadProjectionMatrix@Display@Jet@@UAAXABVMatrix4@2@@Z");
    const auto target = Proc<void(__cdecl*)(void*, void*, unsigned)>(m, "?SetRenderTarget@Display@Jet@@QAAXPAVDisplayTexture@2@I@Z");
    const auto texGen = Proc<void(__cdecl*)(void*, int, int, unsigned)>(m, "?SetTextureCoordGen@Display@Jet@@QAAXHW4DTexGenMode@2@I@Z");
    const auto texture = Proc<SetBool>(m, "?SetTextureMode@Display@Jet@@UAAX_N@Z");
    const auto global = Proc<SetBool>(m, "?SetGlobalTextureMode@Display@Jet@@UAAX_N@Z");
    const auto getTexture = Proc<GetBool>(m, "?GetTextureMode@Display@Jet@@UBA_NXZ");
    const auto getGlobal = Proc<GetBool>(m, "?GetGlobalTextureMode@Display@Jet@@UBA_NXZ");
    const auto projected = Proc<void(__cdecl*)(void*, int, bool, bool)>(m, "?EnableProjectedTexture@Display@Jet@@UAAXH_N0@Z");
    const auto active = Proc<void(__cdecl*)(void*, int)>(m, "?SetActiveTextureStage@Display@Jet@@UAAXH@Z");

    FARPROC table[0x240 / 4 + 1]{};
    table[0x180 / 4] = reinterpret_cast<FARPROC>(ObserveView<0x180, float, float, float, float, float, float>);
    table[0x184 / 4] = reinterpret_cast<FARPROC>(ObserveView<0x184, float, float, float, float, float, float, float>);
    table[0x188 / 4] = reinterpret_cast<FARPROC>(ObserveView<0x188, float, float, float, float>);
    table[0x19c / 4] = reinterpret_cast<FARPROC>(ObserveMatrix);
    table[0x1d0 / 4] = reinterpret_cast<FARPROC>(ObserveTarget);
    table[0x214 / 4] = reinterpret_cast<FARPROC>(ObserveTextureMode);
    table[0x218 / 4] = reinterpret_cast<FARPROC>(ObserveProjectedTexture);
    table[0x240 / 4] = reinterpret_cast<FARPROC>(ObserveTexGen);
    alignas(4) unsigned char display[0x80]{}, state[0xc0]{};
    const auto* tablePtr = table;
    const auto* statePtr = state;
    std::memcpy(display, &tablePtr, sizeof tablePtr);
    std::memcpy(display + 0x54, &statePtr, sizeof statePtr);
    const float matrix[16]{2.5f};
    const float nan = std::bit_cast<float>(UINT32_C(0x7fc12345));
    for (int round = 0; round < 64; ++round) {
        trace->push_back(-50000 - round);
        const float edge = round & 1 ? nan : 6.0f;
        for (int repeat = 0; repeat < 2; ++repeat)
            transition(display, 1, 2, 3, 4, 5, edge, 7);
        for (int repeat = 0; repeat < 2; ++repeat)
            perspective(display, 8, 9, edge, 11);
        for (int repeat = 0; repeat < 2; ++repeat)
            offset(display, 12, 13, 0.25f, 15, edge);
        Params stored{};
        get(display, &stored);
        trace->push_back(stored.mode);
        for (float value : stored.values) trace->push_back(std::bit_cast<int>(value));
        for (int mode : {-1, 0, 1, 2, 3, 4, 5, 9}) {
            const Params input{mode, {0, 1, 2, 3, 0.125f, 5, 6, edge}};
            set(display, &input);
            set(display, &input);
            trace->push_back(normalized(display));
        }
        for (float span : {1.0f, 1.01f, 1.02f, -1.0f, nan}) {
            const Params input{0, {0, span, 2, 3, 4, 5, 6, 7}};
            set(display, &input);
            trace->push_back(normalized(display));
        }
        load(display, matrix);
        load(display, matrix);
        target(display, display + 0x70, 7);
        target(display, display + 0x70, 7);
        target(display, nullptr, 0);
        texture(display, true);
        global(display, true);
        global(display, true);
        texture(display, false);
        global(display, false);
        trace->insert(trace->end(), {getTexture(display), getGlobal(display)});
        texGen(display, 1, 2, 3);
        texGen(display, 7, 2, 3); // stage is not part of the cache key
        texGen(display, 1, 2, 4);
        projected(display, 1, true, false);
        projected(display, 1, true, false);
        projected(display, 3, false, true);
        active(display, 3);
        trace->insert(trace->end(), state, state + sizeof state);
    }
}

static bool __cdecl InitTextureManager(Manager* manager, void* kernel)
{
    trace->insert(trace->end(), {0x903, manager->id, manager->references,
        ReadField<Manager*>(kernel, 0x94) == manager});
    return manager->id == 1;
}

struct SceneItem {
    const FARPROC* table;
    int id;
    void* kernel;
};

// The SceneGraph destructor calls the old deleting-virtual ABI: ECX is the object.
static void* __fastcall DeleteSceneAdapter(SceneItem* item, void*, unsigned flags)
{
    trace->insert(trace->end(), {0x904, item->id, static_cast<int>(flags)});
    if (item->kernel) {
        void* replacement = nullptr;
        std::memcpy(static_cast<unsigned char*>(item->kernel) + 0x80, &replacement, sizeof replacement);
    }
    return item;
}

static void CheckDisplayContextLifetime(HMODULE m)
{
    const auto construct = Proc<void*(__thiscall*)(void*)>(m, "??0DisplayContext@Display@Jet@@QAE@XZ");
    const auto destroy = Proc<void(__thiscall*)(void*)>(m, "??1DisplayContext@Display@Jet@@UAE@XZ");
    const auto copy = Proc<void*(__thiscall*)(void*, const void*)>(m, "??0DisplayContext@Display@Jet@@QAE@ABU012@@Z");
    const auto assign = Proc<void*(__thiscall*)(void*, const void*)>(m, "??4DisplayContext@Display@Jet@@QAEAAU012@ABU012@@Z");
    const auto allocate = Proc<void*(__cdecl*)(unsigned)>(GetModuleHandleA("KernelStandard_7c.dll"),
        "?New@Alloc@Jet@@SAPAXI@Z");
    for (unsigned char fill : {0x00, 0x5a, 0xff}) {
        alignas(4) unsigned char state[0x160];
        std::memset(state, fill, sizeof state);
        trace->push_back(construct(state) == state);
        trace->insert(trace->end(), state, state + sizeof state);
        if (fill != 0) {
            auto* owned = allocate(32);
            if (!owned) std::abort();
            std::memcpy(state + 0x110, &owned, sizeof owned);
        }
        std::memset(state, 0, sizeof(FARPROC));
        destroy(state);
        trace->insert(trace->end(), state, state + sizeof state);
        alignas(4) unsigned char source[sizeof state];
        for (unsigned i = 0; i < sizeof source; ++i) source[i] = static_cast<unsigned char>(i * 13 + fill);
        std::memset(state, fill ^ 0xff, sizeof state);
        trace->push_back(copy(state, source) == state);
        trace->insert(trace->end(), state, state + sizeof state);
        std::memset(state, fill ^ 0xff, sizeof state);
        trace->push_back(assign(state, source) == state);
        trace->insert(trace->end(), state, state + sizeof state);
        trace->push_back(assign(state, state) == state);
        trace->insert(trace->end(), state, state + sizeof state);
    }
}

static void CheckVideoValues(HMODULE m)
{
    using Init = void*(__thiscall*)(unsigned*);
    using Compare = bool(__cdecl*)(const unsigned*, const unsigned*);
    using Assign = unsigned*(__cdecl*)(unsigned*, const unsigned*);
    const auto mode = Proc<void*(__thiscall*)(unsigned*, unsigned, unsigned, unsigned, unsigned)>(
        m, "??0VideoMode@Display@Jet@@QAE@IIII@Z");
    const auto format = Proc<void*(__thiscall*)(unsigned*, unsigned, unsigned, unsigned, unsigned,
        unsigned, unsigned, unsigned, unsigned)>(m, "??0VideoFormat@Display@Jet@@QAE@IIIIIIII@Z");
    const char* initializers[]{
        "??0VideoMode@Display@Jet@@QAE@XZ",
        "??0VideoFormat@Display@Jet@@QAE@XZ",
        "??0?$Streamed3Vertex@UStreamedVertexPCTBase@Jet@@UStreamedVertexP@2@UStreamedVertexC@2@UStreamedVertexT@2@@Jet@@QAE@XZ",
        "??0?$Streamed3Vertex@UStreamedVertexPNTBase@Jet@@UStreamedVertexP@2@UStreamedVertexN@2@UStreamedVertexT@2@@Jet@@QAE@XZ"};
    unsigned a[9], b[9];
    for (const auto* name : initializers) {
        std::fill_n(a, 9, 17);
        trace->push_back(Proc<Init>(m, name)(a) == a);
        trace->insert(trace->end(), a, a + 9);
    }
    for (bool isFormat : {false, true}) {
        const auto eq = Proc<Compare>(m, isFormat ? "??8VideoFormat@Display@Jet@@QBA_NABU012@@Z" :
            "??8VideoMode@Display@Jet@@QBA_NABU012@@Z");
        const auto ne = Proc<Compare>(m, isFormat ? "??9VideoFormat@Display@Jet@@QBA_NABU012@@Z" :
            "??9VideoMode@Display@Jet@@QBA_NABU012@@Z");
        const auto extra = Proc<Compare>(m, isFormat ? "?ComparePixelFormat@VideoFormat@Display@Jet@@QBA_NABU123@@Z" :
            "??MVideoMode@Display@Jet@@QBA_NABU012@@Z");
        const auto assign = Proc<Assign>(m, isFormat ? "??4VideoFormat@Display@Jet@@QAAAAU012@ABU012@@Z" :
            "??4VideoMode@Display@Jet@@QAAAAU012@ABU012@@Z");
        const auto count = isFormat ? 8u : 4u;
        std::fill_n(a, 9, 17);
        trace->push_back(isFormat ? format(a, 1, 2, 3, 4, 5, 6, 7, 8) == a : mode(a, 1, 2, 3, 4) == a);
        trace->insert(trace->end(), a, a + 9);
        std::fill_n(b, 9, 23);
        trace->push_back(assign(b, a) == b);
        trace->insert(trace->end(), b, b + 9);
        trace->insert(trace->end(), {eq(a, b), ne(a, b), extra(a, b)});
        for (unsigned i = 0; i < count; ++i) {
            b[i] = 0xffffffff;
            trace->insert(trace->end(), {eq(a, b), ne(a, b), extra(a, b), extra(b, a)});
            b[i] = a[i];
        }
    }
}

static void CheckRenderValues(HMODULE m)
{
    const auto ks = GetModuleHandleA("KernelStandard_7c.dll");
    const auto alloc = Proc<void*(__cdecl*)(unsigned)>(ks, "?New@Alloc@Jet@@SAPAXI@Z");
    using Default = void*(__thiscall*)(void*);
    using Copy = void*(__thiscall*)(void*, const void*);
    using Destroy = void(__thiscall*)(void*);
    for (unsigned copy : {5, 7, 9, 11, 13, 15}) {
        const unsigned count = copy < 9 ? 1 : copy < 13 ? 3 : 4;
        const unsigned destroy = 236 + (copy - 5) / 2;
        auto* owned = static_cast<unsigned*>(alloc(sizeof(unsigned)));
        *owned = count == 1 ? 1u : 2u;
        unsigned* source[4]{owned, nullptr, owned, nullptr};
        unsigned* dest[5]{};
        auto* guard = reinterpret_cast<unsigned*>(0x1020);
        for (auto& slot : dest) slot = guard;
        trace->push_back(Ordinal<Default>(m, copy + 1)(dest) == dest);
        for (auto* slot : dest) trace->push_back(slot == nullptr ? 0 : slot == guard ? 1 : -1);
        trace->push_back(Ordinal<Copy>(m, copy)(dest, source) == dest);
        for (unsigned i = 0; i < count; ++i) trace->push_back(dest[i] == source[i]);
        trace->push_back(static_cast<int>(*owned));
        Ordinal<Destroy>(m, destroy)(dest);
        trace->push_back(static_cast<int>(*owned));
        Ordinal<Destroy>(m, destroy)(source);
        trace->push_back(source[0] == nullptr);
        trace->push_back(dest[count] == guard);
    }

    unsigned src[18]{0x7f800001u, 0x80000000u, 0x7fc12345u, 1, 2, 3};
    src[16] = 0x11223344;
    src[17] = 0x55667788;
    unsigned dst[19];
    const auto capture = [&] {
        for (unsigned word : dst) trace->push_back(std::bit_cast<int>(word));
    };
    for (unsigned id : {191, 192, 233}) {
        std::memset(dst, 0xa5, sizeof dst);
        trace->push_back(Ordinal<Default>(m, id)(dst) == dst);
        capture();
    }
    for (unsigned id : {550, 551, 558, 565}) {
        std::memset(dst, 0xa5, sizeof dst);
        trace->push_back(Ordinal<Copy>(m, id)(dst, src) == dst);
        capture();
    }
    void* const element = reinterpret_cast<void*>(0x1234);
    Ordinal<void*(__thiscall*)(void*, const void*, void*)>(m, 231)(dst, src, element);
    capture();
    Ordinal<void*(__thiscall*)(void*, const void*, void*, unsigned)>(m, 232)(dst, src, element, 31);
    capture();
    Ordinal<void*(__cdecl*)(const void*, void*)>(m, 1610)(src, dst);
    capture();
    Ordinal<void(__cdecl*)(void*, float)>(m, 2113)(dst, 6.0f);
    capture();
    Ordinal<void(__cdecl*)(void*, float, float, float, float)>(m, 2114)(dst, 8.0f, 7.0f, 6.0f, 5.0f);
    capture();
}

static void* __cdecl BufferVertex(const void*, unsigned a, unsigned b)
{
    trace->insert(trace->end(), {0x14, static_cast<int>(a), static_cast<int>(b)});
    return reinterpret_cast<void*>(0x1414);
}
static void* __cdecl BufferIndex(const void*, unsigned a, bool b)
{
    trace->insert(trace->end(), {0x18, static_cast<int>(a), b});
    return reinterpret_cast<void*>(0x1818);
}
template<int Slot>
static void __cdecl RecordSlot(const void*) { trace->push_back(Slot); }
static void* __cdecl DynamicDeclaration(const void*, int format)
{
    trace->insert(trace->end(), {0x10c, format});
    return reinterpret_cast<void*>(0x1234);
}
static void __cdecl DynamicDraw(const void*, const void* type, int primitive, int flags, const void* data, int count)
{
    trace->insert(trace->end(), {0xe4, reinterpret_cast<int>(type), primitive, flags, reinterpret_cast<int>(data), count});
}

static void CheckDisplayFrameAndBuffers(HMODULE m)
{
    alignas(void*) unsigned char display[0xf00]{}, state[0x148]{}, window[0x134]{};
    FARPROC table[0x214 / sizeof(void*)]{}, buffers[0x4c / sizeof(void*)]{};
    const FARPROC* manager = buffers;
    *reinterpret_cast<FARPROC**>(display) = table;
    *reinterpret_cast<void**>(display + 0x40) = window;
    *reinterpret_cast<void**>(display + 0x54) = state;
    *reinterpret_cast<void**>(display + 0x20c) = &manager;
    *reinterpret_cast<void**>(window + 0x118) = window; // The parent window owns message dispatch.
    table[0x10c / 4] = reinterpret_cast<FARPROC>(&DynamicDeclaration);
    table[0xe4 / 4] = reinterpret_cast<FARPROC>(&DynamicDraw);
    table[0x210 / 4] = reinterpret_cast<FARPROC>(&RecordSlot<0x210>);
    buffers[0x10 / 4] = reinterpret_cast<FARPROC>(&RecordSlot<0x10>);
    buffers[0x14 / 4] = reinterpret_cast<FARPROC>(&BufferVertex);
    buffers[0x18 / 4] = reinterpret_cast<FARPROC>(&BufferIndex);
    buffers[0x40 / 4] = reinterpret_cast<FARPROC>(&RecordSlot<0x40>);
    buffers[0x48 / 4] = reinterpret_cast<FARPROC>(&RecordSlot<0x48>);

    using Frame = void(__cdecl*)(void*, float);
    using Action = void(__cdecl*)(void*);
    Proc<Frame>(m, "?BeginFrame@Display@Jet@@UAAXM@Z")(display, 0.25f);
    trace->insert(trace->end(), {*reinterpret_cast<int*>(display + 0x3c), display[0x70]});
    Proc<Frame>(m, "?EndFrame@Display@Jet@@UAAXM@Z")(display, 0.25f);
    trace->insert(trace->end(), {*reinterpret_cast<int*>(display + 0x3c), display[0x70]});
    Proc<void(__thiscall*)(void*)>(m, "?FlushState@Display@Jet@@UAEXXZ")(display);
    trace->push_back(Proc<bool(__cdecl*)(void*, float)>(m, "?Update@Display@Jet@@UAA_NM@Z")(display, 0.25f));
    const auto checkBuffers = Proc<Action>(m, "?CheckResetDynamicBufferOffsets@Display@Jet@@QAAXXZ");
    checkBuffers(display);
    display[0x213] = 1;
    checkBuffers(display);
    Proc<Action>(m, "?ResetDynamicBufferOffsets@Display@Jet@@IAAXXZ")(display);
    const auto vertex = Proc<void*(__cdecl*)(void*, unsigned, unsigned)>(m,
        "?AllocateStaticVertexBuffer@Display@Jet@@UAAPBVStaticBuffer@2@II@Z")(display, 144, 173);
    const auto index = Proc<void*(__cdecl*)(void*, unsigned, bool)>(m,
        "?AllocateStaticIndexBuffer@Display@Jet@@UAAPBVStaticBuffer@2@I_N@Z")(display, 345, true);
    trace->insert(trace->end(), {reinterpret_cast<int>(vertex), reinterpret_cast<int>(index)});
    Proc<void(__cdecl*)(void*, int, int, int, const void*, int)>(m,
        "?DrawDynamicArray@Display@Jet@@UAAXW4VertexTypeID@2@W4DPrimitive@2@HPBXH@Z")(
        display, 3, 5, 7, reinterpret_cast<void*>(0x5678), 11);
    const auto weight = Proc<void(__cdecl*)(void*, bool)>(m, "?SetVertexWeightMode@Display@Jet@@QAAX_N@Z");
    const auto getWeight = Proc<bool(__cdecl*)(void*)>(m, "?GetVertexWeightMode@Display@Jet@@QBA_NXZ");
    state[0xa1] = 0xe1;
    for (bool value : {true, true, false, false}) {
        weight(display, value);
        trace->insert(trace->end(), {state[0xa1], getWeight(display)});
    }
    const auto extension = Proc<const char*(__cdecl*)(void*, int)>(m,
        "?GetExtensionString@Display@Jet@@QBAPBDW4DExtension@2@@Z");
    for (int i = 0; i < 33; ++i) {
        for (const char* text = extension(display, i); *text; ++text) trace->push_back(*text);
        trace->push_back(0);
    }
    const auto getCompression = Proc<const char*(__cdecl*)(void*, unsigned)>(m,
        "?ConvertTexCompressionString@Display@Jet@@QBAPBDW4DTexCompression@2@@Z");
    const auto findCompression = Proc<int(__cdecl*)(void*, const char*)>(m,
        "?ConvertTexCompressionString@Display@Jet@@QBA?AW4DTexCompression@2@PBD@Z");
    const auto retail = GetModuleHandleA("RenderSystem_7c_orig.dll");
    const auto retailCompression = Proc<decltype(getCompression)>(retail,
        "?ConvertTexCompressionString@Display@Jet@@QBAPBDW4DTexCompression@2@@Z");
    for (unsigned i = 0; i <= 9; ++i) {
        const auto* token = getCompression(display, i);
        trace->insert(trace->end(), {token == retailCompression(display, i), findCompression(display, token)});
    }
    trace->push_back(findCompression(display, "not a compression token"));
    const auto* lookup = reinterpret_cast<const int*>(GetProcAddress(m, "?primitiveLookup@GeometryChunk@Jet@@2QBW4DPrimitive@2@B"));
    const auto* inverse = reinterpret_cast<const int*>(GetProcAddress(m,
        "?inverseCull@?1??SetCullMode@Display@Jet@@QAAXW4DCull@3@@Z@4QBW443@B"));
    if (!lookup || !inverse) std::exit(2);
    trace->insert(trace->end(), lookup, lookup + 32);
    trace->insert(trace->end(), inverse, inverse + 3);
}

static bool screenshotAvailable;
static bool __cdecl ReadScreenshot(const void*, const Rect* area, unsigned* pixels)
{
    trace->insert(trace->end(), {area->x, area->y, area->w, area->h});
    if (!screenshotAvailable) return false;
    for (int y = 0; y < area->h; ++y)
        for (int x = 0; x < area->w; ++x)
            pixels[y * area->w + x] = 0xff000000u | (y << 8) | x;
    return true;
}
static void CheckDisplayScreenshot(HMODULE m)
{
    alignas(void*) unsigned char display[0x294]{}, target[0x18]{};
    FARPROC table[0xd8 / sizeof(void*)]{};
    *reinterpret_cast<FARPROC**>(display) = table;
    *reinterpret_cast<Size*>(display + 0x28c) = {5, 3};
    table[0xd4 / 4] = reinterpret_cast<FARPROC>(&ReadScreenshot);
    const auto shot = Proc<unsigned char*(__cdecl*)(void*)>(m, "?Screenshot@Display@Jet@@QBAPAVBitmap@2@XZ");
    const auto ks = GetModuleHandleA("KernelStandard_7c.dll");
    const auto destroy = Proc<void(__thiscall*)(void*)>(ks, "??1Bitmap@Jet@@QAE@XZ");
    const auto release = Proc<void(__cdecl*)(void*)>(ks, "?Delete@Alloc@Jet@@SAXPAX@Z");

    for (bool targetBound : {false, true}) {
        *reinterpret_cast<void**>(display + 0x5c) = targetBound ? target : nullptr;
        for (bool available : {false, true}) {
            screenshotAvailable = available;
            auto* bitmap = shot(display);
            trace->push_back(bitmap != nullptr);
            if (!bitmap) continue;
            trace->insert(trace->end(), {*reinterpret_cast<int*>(bitmap), *reinterpret_cast<int*>(bitmap + 4)});
            const auto* pixels = *reinterpret_cast<int**>(bitmap + 0x0c);
            trace->insert(trace->end(), pixels, pixels + 15);
            destroy(bitmap);
            release(bitmap);
        }
    }
}

static void CheckRenderValueContexts(HMODULE m)
{
    const auto retail = GetModuleHandleA("RenderSystem_7c_orig.dll");
    const auto ks = GetModuleHandleA("KernelStandard_7c.dll");
    const auto referenceTable = GetProcAddress(ks, "??_7ReferenceCount@Jet@@6B@");
    const auto saveTable = GetProcAddress(retail, "??_7DisplaySaveBuffer@Jet@@6B@");
    const auto selectorTable = GetProcAddress(retail, "??_7DisplaySelector@Jet@@6B@");
    const auto loaderTable = GetProcAddress(retail, "??_7ElementLoader@Jet@@6B@");
    const auto release = Proc<bool(__thiscall*)(void*)>(ks, "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ");
    using Dtor = void(__thiscall*)(void*);
    using Default = void*(__thiscall*)(void*);
    using Copy = void*(__thiscall*)(void*, const void*);
    struct Context { FARPROC table; int refs; int type; };

    Context saved{};
    Ordinal<void*(__thiscall*)(void*, unsigned)>(m, 46)(&saved, 0x5234);
    const auto getType = Proc<int(__cdecl*)(const void*)>(m, "?GetType@DisplaySaveBuffer@Jet@@QBAIXZ");
    trace->insert(trace->end(), {saved.table == saveTable, saved.refs, getType(&saved)});
    Ordinal<Dtor>(m, 253)(&saved);
    trace->push_back(saved.table == referenceTable);

    FARPROC loader[2]{};
    Ordinal<Default>(m, 55)(loader);
    Ordinal<Copy>(m, 54)(loader + 1, loader);
    trace->insert(trace->end(), {loader[0] == loaderTable, loader[1] == loaderTable});
    loader[1] = nullptr;
    Ordinal<Copy>(m, 339)(loader + 1, loader);
    trace->push_back(loader[1] == nullptr);
    Ordinal<Dtor>(m, 258)(loader + 1);
    trace->push_back(loader[1] == loaderTable);

    alignas(void*) unsigned char display[0x28c]{};
    auto& selector = *reinterpret_cast<Context**>(display + 0x288);
    const auto setSelector = Proc<void(__cdecl*)(void*, void*)>(m, "?SetDisplaySelector@Display@Jet@@QAAXPAVDisplaySelector@2@@Z");
    Context provided{};
    Ordinal<Default>(m, 47)(&provided);
    trace->insert(trace->end(), {provided.table == selectorTable, provided.refs});
    setSelector(display, nullptr);
    trace->insert(trace->end(), {selector->table == selectorTable, selector->refs});
    setSelector(display, &provided);
    trace->insert(trace->end(), {selector == &provided, provided.refs});
    setSelector(display, &provided);
    trace->insert(trace->end(), {selector == &provided, provided.refs});
    setSelector(display, nullptr);
    trace->insert(trace->end(), {selector->table == selectorTable, selector->refs, provided.refs});
    release(selector);
    Ordinal<Dtor>(m, 254)(&provided);
    trace->push_back(provided.table == referenceTable);
}

class ProbeImport {
    ULONG_PTR* slot_ = nullptr;
    ULONG_PTR original_ = 0;

    void Store(ULONG_PTR value)
    {
        DWORD old;
        if (!VirtualProtect(slot_, sizeof(*slot_), PAGE_READWRITE, &old)) std::exit(2);
        *slot_ = value;
        if (!VirtualProtect(slot_, sizeof(*slot_), old, &old)) std::exit(2);
    }

public:
    ProbeImport(HMODULE m, const char* name, FARPROC replacement)
    {
        auto* base = reinterpret_cast<unsigned char*>(m);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        auto* library = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(
            base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        for (; library->Name; ++library) {
            if (!library->OriginalFirstThunk) continue;
            auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + library->OriginalFirstThunk);
            auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + library->FirstThunk);
            for (; names->u1.AddressOfData; ++names, ++slots) {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
                const auto* entry = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
                if (std::strcmp(entry->Name, name) != 0) continue;
                slot_ = &slots->u1.Function;
                original_ = *slot_;
                Store(reinterpret_cast<ULONG_PTR>(replacement));
                return;
            }
        }
        std::printf("missing probe import: %s\n", name);
        std::exit(2);
    }
    ~ProbeImport() { Store(original_); }
    ProbeImport(const ProbeImport&) = delete;
    ProbeImport& operator=(const ProbeImport&) = delete;
};

static bool gammaAvailable;
static HDC WINAPI ProbeGetDC(HWND window)
{
    trace->insert(trace->end(), {0x310, window == nullptr});
    return reinterpret_cast<HDC>(0x1337);
}
static BOOL WINAPI ProbeReadGamma(HDC dc, LPVOID data)
{
    trace->insert(trace->end(), {0x311, dc == reinterpret_cast<HDC>(0x1337)});
    if (!gammaAvailable) return FALSE;
    auto* ramp = static_cast<unsigned short*>(data);
    for (unsigned i = 0; i < 768; ++i) ramp[i] = static_cast<unsigned short>(i * 11);
    return TRUE;
}
static BOOL WINAPI ProbeWriteGamma(HDC dc, LPVOID data)
{
    trace->insert(trace->end(), {0x312, dc == reinterpret_cast<HDC>(0x1337)});
    auto* ramp = static_cast<const unsigned short*>(data);
    trace->insert(trace->end(), ramp, ramp + 768);
    return TRUE;
}

static void CheckDisplayGamma(HMODULE m)
{
    const ProbeImport getDC(m, "GetDC", reinterpret_cast<FARPROC>(&ProbeGetDC));
    const ProbeImport readGamma(m, "GetDeviceGammaRamp", reinterpret_cast<FARPROC>(&ProbeReadGamma));
    const ProbeImport writeGamma(m, "SetDeviceGammaRamp", reinterpret_cast<FARPROC>(&ProbeWriteGamma));
    const auto set = Proc<void(__cdecl*)(void*, float, bool)>(m, "?SetGamma@Display@Jet@@QAAXM_N@Z");
    const auto restore = Proc<void(__cdecl*)(void*)>(m, "?RestoreGamma@Display@Jet@@QAAXXZ");
    const auto down = Proc<void(__cdecl*)(void*)>(m, "?CriticalShutdown@Display@Jet@@IAAXXZ");
    const auto up = Proc<void(__cdecl*)(void*)>(m, "?CriticalStartup@Display@Jet@@IAAXXZ");
    const auto upCb = Proc<int(__cdecl*)(void*)>(m, "?CriticalStartup@Display@Jet@@KAHPAX@Z");
    const auto downCb = Proc<int(__cdecl*)(void*)>(m, "?CriticalShutdown@Display@Jet@@KAHPAX@Z");
    for (bool available : {false, true}) {
        alignas(void*) unsigned char display[0xf00]{};
        gammaAvailable = available;
        for (float gamma : {0.2f, 0.5f, 1.0f, 1.25f, 1.9f, 2.0f, 6.0f,
                 std::numeric_limits<float>::quiet_NaN()}) {
            for (bool force : {false, true}) {
                set(display, gamma, force);
                trace->insert(trace->end(), {*reinterpret_cast<const int*>(display + 0x2f0),
                    *reinterpret_cast<const int*>(display + 0x2f4)});
            }
        }
        up(display);
        trace->push_back(upCb(display));
        restore(display);
        down(display);
        trace->push_back(downCb(display));
    }
}

static void CheckRenderKernelInit(HMODULE m)
{
    const auto ks = GetModuleHandleA("KernelStandard_7c.dll");
    const auto osCtor = Proc<void*(__thiscall*)(void*, const char*, const char*, void*, HWND)>(ks,
        "??0OS@Jet@@QAE@PBD0PAXPAUHWND__@@@Z");
    const auto osDtor = Proc<void(__thiscall*)(void*)>(ks, "??1OS@Jet@@QAE@XZ");
    const auto getOptions = Proc<void*(__cdecl*)(const void*)>(ks, "?GetOptions@OS@Jet@@QBAPAVOptions@2@XZ");
    const auto raw = Proc<void(__cdecl*)(void*, const char*)>(ks, "?ProcessRaw@Options@Jet@@QAAXPBD@Z");
    const auto kCtor = Ordinal<void*(__thiscall*)(void*, void*, float)>(m, 162);
    const auto kDtor = Ordinal<void(__thiscall*)(void*)>(m, 291);
    const auto init = Proc<bool(__cdecl*)(void*, void*, void*)>(m,
        "?Init@RenderKernel@Jet@@UAA_NPAVOS@2@PAVDisplaySelector@2@@Z");
    const auto dump = Proc<void(__cdecl*)(void*, unsigned)>(m, "?DumpScreen@RenderKernel@Jet@@QAAXI@Z");
    const auto multiple = Proc<void(__cdecl*)(void*, unsigned, unsigned, unsigned)>(m,
        "?MultipleScreenDump@RenderKernel@Jet@@QAAXIII@Z");
    const auto ppi = Proc<int(__cdecl*)(void*)>(m, "?GetTargetPPI@Display@Jet@@QBAHXZ");
    auto* noPath = reinterpret_cast<bool*>(GetProcAddress(GetModuleHandleA("RenderSystem_7c_orig.dll"),
        "?noPath@AsciiTextureResource@Jet@@1_NA"));
    const bool savedNoPath = *noPath;
    for (unsigned scenario : {0, 1, 2}) {
        const bool loadRenderer = scenario != 0;
        const bool hasManagers = scenario != 2;
        alignas(void*) unsigned char os[0x18]{}, kernel[0xe8]{}, display[0x48]{};
        osCtor(os, "rs-probe", "", nullptr, nullptr);
        kCtor(kernel, nullptr, 0.0f);
        auto* options = getOptions(os);
        raw(options, loadRenderer ? "-NullRender -NoAsciiTexPath -screenppi=144" : "");
        *reinterpret_cast<void**>(display + 0x44) = os;
        trace->push_back(ppi(display));
        auto& rendering = *reinterpret_cast<void**>(kernel + 0x7c);
        auto& textures = *reinterpret_cast<void**>(kernel + 0x94);
        auto& materials = *reinterpret_cast<void**>(kernel + 0x90);
        // Preexisting managers must be preserved without callbacks or refcount changes.
        void* const existingManager = reinterpret_cast<void*>(0x1114);
        textures = materials = hasManagers ? existingManager : nullptr;
        *noPath = false;
        const bool initialized = init(kernel, os, nullptr);
        const bool correctManagers = hasManagers ? textures == existingManager && materials == existingManager
            : textures && materials && textures != materials;
        if (initialized != loadRenderer || (rendering != nullptr) != loadRenderer || !correctManagers) {
            std::puts("RenderKernel::Init did not preserve the expected renderer/manager state");
            std::exit(1);
        }
        trace->insert(trace->end(), {initialized, *noPath, kernel[0x24],
            *reinterpret_cast<void**>(kernel + 0x8c) == os});
        if (hasManagers) textures = materials = nullptr;
        void* const savedDisplay = rendering;
        rendering = display;
        *reinterpret_cast<unsigned*>(display + 0x3c) = 24;
        for (unsigned first : {0, 11}) {
            dump(kernel, first);
            multiple(kernel, first, 29, 2);
            trace->insert(trace->end(), {kernel[0x99], *reinterpret_cast<int*>(kernel + 0x9c),
                *reinterpret_cast<int*>(kernel + 0xa0), *reinterpret_cast<int*>(kernel + 0xa4),
                *reinterpret_cast<int*>(kernel + 0xa8)});
        }
        rendering = savedDisplay;
        kDtor(kernel);
        osDtor(os);
    }
    *noPath = savedNoPath;
}

static bool formatListAvailable;
static bool __cdecl QueryVideoFormats(void*)
{
    trace->push_back(0x254);
    return formatListAvailable;
}
static bool __cdecl RestoreVideoMode(void*, const void*)
{
    trace->push_back(0x258);
    return true;
}

static void CheckDisplaySelection(HMODULE m)
{
    using Mode = std::array<unsigned, 4>;
    using Format = std::array<unsigned, 8>;
    struct List { unsigned count, capacity; const void* entries; };
    const auto ks = GetModuleHandleA("KernelStandard_7c.dll");
    using Destroy = void(__thiscall*)(void*);
    const auto osCtor = Proc<void*(__thiscall*)(void*, const char*, const char*, void*, HWND)>(ks,
        "??0OS@Jet@@QAE@PBD0PAXPAUHWND__@@@Z");
    const auto osDtor = Proc<Destroy>(ks, "??1OS@Jet@@QAE@XZ");
    const auto getOptions = Proc<void*(__cdecl*)(const void*)>(ks, "?GetOptions@OS@Jet@@QBAPAVOptions@2@XZ");
    const auto raw = Proc<void(__cdecl*)(void*, const char*)>(ks, "?ProcessRaw@Options@Jet@@QAAXPBD@Z");
    const auto createWindow = Proc<void*(__cdecl*)(void*)>(ks, "?CreateNewWindow@OS@Jet@@QBAPAVWindow@2@XZ");
    const auto destroyWindow = Proc<Destroy>(ks, "??1Window@Jet@@QAE@XZ");
    const auto freeMemory = Proc<void(__cdecl*)(void*)>(ks, "?Delete@Alloc@Jet@@SAXPAX@Z");
    using Select = bool(__cdecl*)(void*, const void*, const List*, void*);
    const auto resolution = Ordinal<void(__cdecl*)(void*, const void*, unsigned*, unsigned*)>(m, 2098);
    const auto screen = Ordinal<void(__cdecl*)(void*, const void*, void*, const Mode*, int*)>(m, 2099);
    const auto area = Ordinal<void(__cdecl*)(void*, const void*, void*, const Mode*, Rect*)>(m, 2102);
    const auto findMode = Ordinal<Select>(m, 2101);
    const auto findFormat = Ordinal<Select>(m, 2100);
    const auto setup = Ordinal<bool(__cdecl*)(void*, const void*, Format*)>(m, 2389);
    FARPROC selectorTable[7]{};
    auto* selector = selectorTable;
    selectorTable[3] = reinterpret_cast<FARPROC>(resolution);
    selectorTable[6] = reinterpret_cast<FARPROC>(findFormat);

    alignas(void*) unsigned char os[0x18]{};
    auto* window = static_cast<unsigned char*>(createWindow(os));
    const Mode desktop{640, 400, 24, 60};
    const Mode modes[]{
        {1024, 768, 16, 60}, {1024, 768, 32, 75}, {1024, 768, 24, 60},
        {640, 480, 16, 75}, {640, 480, 32, 60}, {1920, 1440, 24, 60},
        {1234, 768, 24, 60}, {640, 456, 32, 85}, {640, 456, 32, 60},
    };
    constexpr const char* scenarios[]{
        "", "-320 -640 -1920 -Fullscreen", "-640 -Height=456 -WindowCentered",
        "-Width=1234 -WindowPosY=-15", "-640 -WindowPosX=100",
        "-WindowHorizontalCentered -WindowPosY=17", "-WindowVerticalCentered -Height=480",
        "-Frequency=60 -ColorBits=17", "-Frequency=99 -ColorBits=24",
        "-ColorBits=16 -DepthBits=0 -AlphaBits=0 -multiSample=8",
        "-ColorBits=24 -DepthBits=16 -AlphaBits=8 -multiSample=2 -StencilBits=8",
        "-DisableStencil -StencilBits=16 -multiSample=0", "-ColorBits=-2 -DepthBits=-2",
    };
    const List modeList{static_cast<unsigned>(std::size(modes)), 20, modes};
    std::mt19937 random(0x101f1c0);
    for (const char* scenario : scenarios) {
        osCtor(os, "rs-probe", "", nullptr, nullptr);
        auto* options = getOptions(os);
        raw(options, scenario);
        unsigned width = 9, height = 10;
        resolution(&selector, options, &width, &height);
        int screenMode = 99;
        screen(&selector, options, window, &desktop, &screenMode);
        Rect rect{};
        area(&selector, options, window, &desktop, &rect);
        trace->insert(trace->end(), {static_cast<int>(width), static_cast<int>(height), screenMode,
            rect.x, rect.y, rect.w, rect.h});
        *reinterpret_cast<void**>(window + 0x118) = window;
        screen(&selector, options, window, &desktop, &screenMode);
        trace->push_back(screenMode);
        *reinterpret_cast<void**>(window + 0x118) = nullptr;
        Mode chosenMode{1, 2, 3, 4};
        trace->push_back(findMode(&selector, options, &modeList, &chosenMode));
        trace->insert(trace->end(), chosenMode.begin(), chosenMode.end());

        for (unsigned round = 0; round < 160; ++round) {
            Format candidates[20];
            const auto roll = [&](unsigned limit) { return static_cast<unsigned>(random() % limit); };
            for (unsigned i = 0; i < 20; ++i)
                candidates[i] = {i, 91 - i, 8 * roll(5), 8 * roll(5), 8 * roll(2), 8 * roll(3), roll(8), roll(8)};
            const List formatList{static_cast<unsigned>(round % 21), 20, candidates};
            Format chosen{11, 12, 13, 14, 15, 16, 17, 18};
            trace->push_back(findFormat(&selector, options, &formatList, &chosen));
            trace->insert(trace->end(), chosen.begin(), chosen.end());
        }
        osDtor(os);
    }

    alignas(void*) unsigned char display[0x294]{};
    FARPROC displayTable[0x25c / sizeof(void*)]{};
    displayTable[0x254 / 4] = reinterpret_cast<FARPROC>(&QueryVideoFormats);
    displayTable[0x258 / 4] = reinterpret_cast<FARPROC>(&RestoreVideoMode);
    *reinterpret_cast<FARPROC**>(display) = displayTable;
    *reinterpret_cast<void**>(display + 0x40) = window;
    *reinterpret_cast<void**>(display + 0x288) = &selector;
    *reinterpret_cast<int*>(display + 0x22c) = 2;
    osCtor(os, "rs-probe", "", nullptr, nullptr);
    auto* options = getOptions(os);
    Format chosen{};
    Format candidate{8, 7, 32, 24, 8, 8, 4, 0};
    auto& list = *reinterpret_cast<List*>(display + 0x25c);
    list = {1, 20, &candidate};
    formatListAvailable = false;
    trace->push_back(setup(display, options, &chosen));
    formatListAvailable = true;
    trace->push_back(setup(display, options, &chosen));
    trace->insert(trace->end(), chosen.begin(), chosen.end());
    list.count = 0;
    trace->push_back(setup(display, options, &chosen));
    osDtor(os);
    destroyWindow(window);
    freeMemory(window);
}

static void CheckDisplayInit(HMODULE m)
{
    const HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
    const auto init = Proc<bool(__cdecl*)(void*, void*)>(m, "?Init@Display@Jet@@UAA_NPAVOS@2@@Z");
    const auto getWindow = Proc<void*(__cdecl*)(void*)>(ks, "?GetWindow@OS@Jet@@QBAPAVWindow@2@XZ");
    const auto removeDown = Proc<void(__cdecl*)(void*)>(ks, "?RemoveCriticalShutdown@ExceptionHandler@Jet@@SAXPAX@Z");
    const auto removeUp = Proc<void(__cdecl*)(void*)>(ks, "?RemoveCriticalStartup@ExceptionHandler@Jet@@SAXPAX@Z");
    const auto destroyWindow = Proc<void(__thiscall*)(void*)>(ks, "??1Window@Jet@@QAE@XZ");
    const auto freeMemory = Proc<void(__cdecl*)(void*)>(ks, "?Delete@Alloc@Jet@@SAXPAX@Z");
    alignas(void*) unsigned char os[0x40]{};
    alignas(void*) unsigned char display[0xf00]{};
    const auto field = [&](unsigned offset) {
        void* value;
        std::memcpy(&value, display + offset, sizeof value);
        return value;
    };

    if (!init(display, os) || display[0x48] != 1 || field(0x44) != os ||
        !field(0x40) || field(0x40) != getWindow(os) || !field(0xef8) || !field(0xefc)) {
        std::puts("Display::Init did not establish window and callback state");
        std::exit(1);
    }
    unsigned char initialized[sizeof display];
    std::memcpy(initialized, display, sizeof display);
    if (!init(display, nullptr) || std::memcmp(display, initialized, sizeof display) != 0) {
        std::puts("Display::Init changed an already initialized display");
        std::exit(1);
    }
    removeUp(field(0xefc));
    removeDown(field(0xef8));
    destroyWindow(field(0x40));
    freeMemory(field(0x40));
    trace->push_back(0x10db0);
}

static void CheckContextFactories(HMODULE m)
{
    using Factory = void*(__cdecl*)(const void*);
    const auto retail = GetModuleHandleA("RenderSystem_7c_orig.dll");
    const auto release = Proc<bool(__thiscall*)(const void*)>(GetModuleHandleA("KernelStandard_7c.dll"),
        "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ");
    const char* factories[]{"?CreateSharedContext@Display@Jet@@UBAPAVSharedContext@2@XZ",
        "?CreateNonRenderContext@Display@Jet@@UBAPAVNonRenderContext@2@XZ"};
    const char* vtables[]{"??_7SharedContext@Jet@@6B@", "??_7NonRenderContext@Jet@@6B@"};
    const char* constructors[]{"??0SharedContext@Jet@@IAE@XZ", "??0NonRenderContext@Jet@@IAE@XZ"};
    const char* destructors[]{"??1SharedContext@Jet@@MAE@XZ", "??1NonRenderContext@Jet@@MAE@XZ"};
    const auto referenceTable = GetProcAddress(GetModuleHandleA("KernelStandard_7c.dll"),
        "??_7ReferenceCount@Jet@@6B@");
    for (unsigned i = 0; i < 2; ++i) {
        const auto create = Proc<Factory>(m, factories[i]);
        const auto expected = GetProcAddress(retail, vtables[i]);
        const auto construct = Proc<void*(__thiscall*)(void*)>(m, constructors[i]);
        const auto destroy = Proc<void(__thiscall*)(void*)>(m, destructors[i]);
        struct {
            FARPROC table;
            unsigned references;
            unsigned guard;
        } storage{nullptr, 99, 0x12345678};
        trace->insert(trace->end(), {construct(&storage) == &storage,
            storage.table == expected, static_cast<int>(storage.references)});
        storage.references = 7;
        destroy(&storage);
        trace->insert(trace->end(), {storage.table == referenceTable,
            static_cast<int>(storage.references), static_cast<int>(storage.guard)});
        auto* first = create(nullptr);
        auto* second = create(nullptr);
        trace->push_back(first && second && first != second);
        for (const auto* context : {first, second}) {
            if (!context) continue;
            FARPROC actual;
            unsigned references;
            std::memcpy(&actual, context, sizeof actual);
            std::memcpy(&references, static_cast<const char*>(context) + sizeof actual, sizeof references);
            trace->insert(trace->end(), {expected && expected == actual, static_cast<int>(references), release(context)});
        }
    }
}

static void CheckKernelOwnership(HMODULE m)
{
    using GetPtr = void*(__cdecl*)(void*);
    using SetPtr = void(__cdecl*)(void*, void*);
    const auto display = Proc<GetPtr>(m, "?GetDisplay@RenderKernel@Jet@@QAAPAVDisplay@2@XZ");
    const auto os = Proc<GetPtr>(m, "?GetOS@RenderKernel@Jet@@QAAPAVOS@2@XZ");
    const auto self = Proc<GetPtr>(m, "?GetRenderKernel@RenderKernel@Jet@@MAAPAV12@XZ");
    const auto getScene = Proc<GetPtr>(m, "?GetSceneGraph@RenderKernel@Jet@@QAAPAVSceneGraph@2@XZ");
    const auto setScene = Proc<SetPtr>(m, "?SetSceneGraph@RenderKernel@Jet@@QAAXPAVSceneGraph@2@@Z");
    const auto destroy = Proc<void(__cdecl*)(void*)>(m, "?DestroySceneGraph@RenderKernel@Jet@@QAAXXZ");
    const auto getTexture = Proc<GetPtr>(m, "?GetTextureManager@RenderKernel@Jet@@QAAPAVTextureManager@2@XZ");
    const auto setTexture = Proc<SetPtr>(m, "?SetTextureManager@RenderKernel@Jet@@QAAXPAVTextureManager@2@@Z");
    const auto add = Proc<SetPtr>(m, "?AddSystemResource@RenderKernel@Jet@@QAAXPAVSystemResourceManager@2@@Z");
    const auto remove = Proc<SetPtr>(m, "?RemoveSystemResource@RenderKernel@Jet@@QAAXPAVSystemResourceManager@2@@Z");
    const auto construct = Proc<void*(__thiscall*)(void*, SceneItem*)>(m, "??0SceneGraph@Jet@@QAE@PAVModifierUpdateTask@1@@Z");
    const auto addScene = Proc<void(__cdecl*)(void*, SceneItem*)>(m, "?AddScene@SceneGraph@Jet@@QAAXPAVElement@2@@Z");
    const auto enqueue = Proc<void(__cdecl*)(void*, SceneItem*)>(m, "?DelayedDelete@SceneGraph@Jet@@QAAXPAVElement@2@@Z");
    const auto allocate = Proc<void*(__cdecl*)(unsigned)>(GetModuleHandleA("KernelStandard_7c.dll"), "?New@Alloc@Jet@@SAPAXI@Z");

    alignas(4) unsigned char kernel[0x98]{};
    auto* tag = kernel + 4;
    std::memcpy(kernel + 0x7c, &tag, sizeof tag);
    tag = kernel + 8;
    std::memcpy(kernel + 0x8c, &tag, sizeof tag);
    trace->insert(trace->end(), {display(kernel) == kernel + 4, os(kernel) == kernel + 8,
        self(kernel + 0x28) == kernel, getScene(kernel) == nullptr, getTexture(kernel) == nullptr});

    const FARPROC managerTable[]{reinterpret_cast<FARPROC>(DeleteManager), nullptr,
        reinterpret_cast<FARPROC>(InitTextureManager)};
    Manager a{managerTable, 1, 1}, b{managerTable, 1, 2};
    setTexture(kernel, &a);
    trace->insert(trace->end(), {getTexture(kernel) == &a, a.references});
    a.references = 1;
    setTexture(kernel, &b);
    setTexture(kernel, &b);
    setTexture(kernel, nullptr);
    trace->insert(trace->end(), {getTexture(kernel) == nullptr, b.references});

    const FARPROC deleteTable[]{reinterpret_cast<FARPROC>(DeleteSceneAdapter)};
    SceneItem taskA{deleteTable, 1, kernel}, taskB{deleteTable, 2, nullptr};
    SceneItem normal{deleteTable, 3, nullptr}, delayed{deleteTable, 4, nullptr};
    auto* graphA = construct(allocate(0x0c), &taskA);
    auto* graphB = construct(allocate(0x0c), &taskB);
    addScene(graphA, &normal);
    enqueue(graphA, &delayed);
    setScene(kernel, graphA);
    trace->push_back(getScene(kernel) == graphA);
    setScene(kernel, graphB);
    trace->push_back(getScene(kernel) == graphB);
    destroy(kernel);
    destroy(kernel);
    trace->push_back(getScene(kernel) == nullptr);

    struct Block { Block* next; Block* prev; unsigned count; int* items[13]; };
    int resources[27]{};
    const auto list = [&] {
        const auto* head = ReadField<Block*>(kernel, 0x88);
        if (!head) {
            trace->push_back(0);
            return;
        }
        const auto* block = head;
        for (int i = 0; i < 4; ++i) {
            trace->insert(trace->end(), {static_cast<int>(block->count),
                block->next->prev == block, block->prev->next == block});
            for (unsigned j = 0; j < block->count && j < 13; ++j)
                trace->push_back(block->items[j] ? static_cast<int>(block->items[j] - resources) : -1);
            block = block->next;
            if (block == head) break;
        }
    };
    remove(kernel, resources);
    for (auto& resource : resources) add(kernel, &resource);
    add(kernel, resources + 2);
    add(kernel, nullptr);
    list();
    remove(kernel, resources + 2);
    remove(kernel, nullptr);
    remove(kernel, resources + 26);
    list();
    for (auto& resource : resources) remove(kernel, &resource);
    list();
}

static Trace Run(HMODULE m)
{
    using SetRect = void(__cdecl*)(void*, const Rect*);
    using SetBool = void(__cdecl*)(void*, bool);
    using GetBool = bool(__cdecl*)(const void*);
    using GetterSize = const Size*(__cdecl*)(const void*);
    const auto viewport = Proc<SetRect>(m, "?SetViewport@Display@Jet@@QAAXABURect@2@@Z");
    const auto scissor = Proc<SetRect>(m, "?SetScissor@Display@Jet@@QAAXABURect@2@@Z");
    const auto getViewport = Proc<const Rect*(__cdecl*)(void*)>(m, "?GetViewport@Display@Jet@@QBAABURect@2@XZ");
    const auto getScissor = Proc<Rect*(__cdecl*)(void*, Rect*)>(m, "?GetScissor@Display@Jet@@QAA?AURect@2@XZ");
    const auto mode = Proc<SetBool>(m, "?SetScissorMode@Display@Jet@@QAAX_N@Z");
    const auto getMode = Proc<GetBool>(m, "?GetScissorMode@Display@Jet@@QBA_NXZ");
    const auto getWindow = Proc<GetterSize>(m, "?GetWindowSize@Display@Jet@@QBAABUSize@2@XZ");
    const auto getTarget = Proc<GetterSize>(m, "?GetTargetSize@Display@Jet@@QBAABUSize@2@XZ");
    const auto proj = Proc<void(__cdecl*)(void*, float, float, float, float, float, float)>(m, "?Set2DView@Display@Jet@@QAAXMMMMMM@Z");
    const auto normalized = Proc<SetBool>(m, "?Set2DViewNormalized@Display@Jet@@QAAX_N@Z");
    const auto setColor = Proc<void(__cdecl*)(void*, const Color*)>(m, "?SetClearColor@Display@Jet@@QAAXABVColor@2@@Z");
    const auto getColor = Proc<const Color*(__cdecl*)(void*)>(m, "?GetClearColor@Display@Jet@@QBAABVColor@2@XZ");
    const auto setDepth = Proc<void(__cdecl*)(void*, float)>(m, "?SetClearDepth@Display@Jet@@QAAXM@Z");
    const auto getDepth = Proc<float(__cdecl*)(void*)>(m, "?GetClearDepth@Display@Jet@@QBAMXZ");
    const auto setStencil = Proc<void(__cdecl*)(void*, long)>(m, "?SetClearStencil@Display@Jet@@QAAXJ@Z");
    const auto getStencil = Proc<long(__cdecl*)(void*)>(m, "?GetClearStencil@Display@Jet@@QBAJXZ");
    const auto setCull = Proc<void(__cdecl*)(void*, int)>(m, "?SetCullMode@Display@Jet@@QAAXW4DCull@2@@Z");
    const auto getCull = Proc<int(__cdecl*)(void*)>(m, "?GetCullMode@Display@Jet@@QBA?AW4DCull@2@XZ");
    const auto reflect = Proc<SetBool>(m, "?SetReflectionCull@Display@Jet@@QAAX_N@Z");
    const auto getReflect = Proc<GetBool>(m, "?GetReflectionCull@Display@Jet@@QBA_NXZ");
    const auto setMaterialMode = Proc<void(__cdecl*)(void*, int)>(m, "?SetColorMaterialMode@Display@Jet@@QAAXW4DColorMaterialTypes@2@@Z");
    const auto getMaterialMode = Proc<int(__cdecl*)(void*)>(m, "?GetColorMaterialMode@Display@Jet@@QAA?AW4DColorMaterialTypes@2@XZ");
    const auto materialProperties = Proc<void(__cdecl*)(void*, const MaterialState*)>(
        m, "?SetMaterialProperties@Display@Jet@@UAAXABUDisplayMaterial@2@@Z");

    FARPROC table[0x21c / 4 + 1]{};
    table[0x180 / 4] = reinterpret_cast<FARPROC>(Projection);
    table[0x190 / 4] = reinterpret_cast<FARPROC>(ObserveRect<0x190>);
    table[0x194 / 4] = reinterpret_cast<FARPROC>(ObserveRect<0x194>);
    table[0x198 / 4] = reinterpret_cast<FARPROC>(Mode<0x198>);
    table[0x1a0 / 4] = reinterpret_cast<FARPROC>(ClearColor);
    table[0x1a4 / 4] = reinterpret_cast<FARPROC>(Value<float, 0x1a4>);
    table[0x1a8 / 4] = reinterpret_cast<FARPROC>(Value<long, 0x1a8>);
    table[0x1d8 / 4] = reinterpret_cast<FARPROC>(Value<int, 0x1d8>);
    table[0x20c / 4] = reinterpret_cast<FARPROC>(Value<int, 0x20c>);
    table[0x21c / 4] = reinterpret_cast<FARPROC>(MaterialProperties);
    alignas(4) unsigned char display[0x294]{}, state[0x158]{}, renderTarget[0x18]{};
    auto* tablePtr = table;
    auto* statePtr = state;
    std::memcpy(display, &tablePtr, sizeof tablePtr);
    std::memcpy(display + 0x54, &statePtr, sizeof statePtr);
    const Size window{1280, 720}, target{640, 512};
    std::memcpy(display + 0x28c, &window, sizeof window);
    std::memcpy(renderTarget + 0x10, &target, sizeof target);
    const float nan = std::bit_cast<float>(UINT32_C(0x7fc12345));

    Trace result;
    trace = &result;
    std::mt19937 rng(1729);
    for (int round = 0; round < 64; ++round) {
        result.push_back(-10000 - round);
        auto* targetPtr = (round & 1) ? renderTarget : nullptr;
        std::memcpy(display + 0x5c, &targetPtr, sizeof targetPtr);
        const auto* w = getWindow(display);
        const auto* t = getTarget(display);
        result.insert(result.end(), {w->w, w->h, t->w, t->h, w == t});
        const Rect a{static_cast<int>(rng() % 50) - 25, round, 640, 480};
        const Rect b{round, -round, 125, 200};
        scissor(display, &a);
        viewport(display, &a); // viewport changed, even though scissor already matches
        viewport(display, &a); // neither changed
        scissor(display, &b);
        viewport(display, &a); // only scissor changes
        Rect out{};
        result.push_back(getScissor(display, &out) == &out);
        result.insert(result.end(), {out.x, out.y, out.w, out.h,
            static_cast<int>(reinterpret_cast<const unsigned char*>(getViewport(display)) - state)});
        mode(display, true);
        mode(display, true);
        result.push_back(getMode(display));
        mode(display, false);
        normalized(display, false);
        normalized(display, true);
        normalized(display, true);
        const float edge = (round & 2) ? nan : 1.25f;
        proj(display, -0.0f, 2.0f, edge, 0.0f, -3.5f, 4.5f);
        proj(display, +0.0f, 2.0f, edge, 0.0f, -3.5f, 4.5f);
        const Color color{edge, 0.25f, 0.5f, 1.0f};
        setColor(display, &color);
        setColor(display, &color);
        setDepth(display, edge);
        setDepth(display, edge);
        setStencil(display, -round);
        setStencil(display, -round);
        result.insert(result.end(), {std::bit_cast<int>(getColor(display)->r),
            std::bit_cast<int>(getDepth(display)), static_cast<int>(getStencil(display))});
        setCull(display, round % 3);
        reflect(display, true);
        reflect(display, true);
        setCull(display, (round + 1) % 3);
        result.insert(result.end(), {getCull(display), getReflect(display)});
        reflect(display, false);
        setMaterialMode(display, round % 4);
        setMaterialMode(display, round % 4);
        result.push_back(getMaterialMode(display));
        MaterialState material{};
        material.channels[0] = edge;
        material.channels[7] = 0.75f;
        material.channels[16] = static_cast<float>(round % 3);
        materialProperties(display, &material);
        materialProperties(display, &material);
        result.insert(result.end(), state, state + sizeof state);
    }
    CheckKernelMaterial(m);
    CheckBlendDepth(m);
    CheckLighting(m);
    CheckPackedColor(m);
    CheckColorArrays(m);
    CheckGeometryModes(m);
    CheckStencilPolygon(m);
    CheckDisplayInfo(m);
    CheckViewAndTexture(m);
    CheckKernelOwnership(m);
    CheckDisplayInit(m);
    CheckContextFactories(m);
    CheckDisplayContextLifetime(m);
    CheckVideoValues(m);
    CheckRenderValues(m);
    CheckDisplayFrameAndBuffers(m);
    CheckDisplayScreenshot(m);
    CheckRenderValueContexts(m);
    CheckDisplayGamma(m);
    CheckRenderKernelInit(m);
    CheckDisplaySelection(m);
    return result;
}

int main(int argc, char** argv)
{
    if (argc != 2 || !SetDllDirectoryA(argv[1])) {
        std::puts("usage: diff_rs_u9.exe <bin dir>");
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
        std::printf("Display trace mismatch at %zu (sizes %zu/%zu)\n", i, baseline.size(), port.size());
        if (i < baseline.size() && i < port.size())
            std::printf("original=%08x rebuilt=%08x\n", static_cast<unsigned>(baseline[i]), static_cast<unsigned>(port[i]));
        return 1;
    }
    std::printf("Display: 64 rounds, %zu matching trace entries\n", port.size());
    return 0;
}
