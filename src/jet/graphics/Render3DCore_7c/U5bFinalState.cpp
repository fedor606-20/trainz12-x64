// Render3DCore_7c U5b wave 5 (U5bFinal): MultiPassRenderState layout, lifetime and the state leaves that only touch
// the state block, the display and the chunk's virtual geometry interface.
//
// The four property/scissor/pass bodies (StateSetup, LoadRenderProperties x2, SetScissor, RenderSingleLightPasses)
// still call the original's entry points: they read per-light properties that only the original's material plugins
// produce, and the differential test compares their effects through the object image. The exported state methods in
// this file are nevertheless local code.
#include "U5bFinal.h"
#include "U5bLast.h"

#include <jet/DynamicArray.h>
#include <jet/Render3DCoreLights.h>

#include <cstring>

namespace Jet {
namespace MultiPass {
namespace Final {

namespace {

constexpr unsigned int kDisplay = 0x04;
constexpr unsigned int kView = 0x08;
constexpr unsigned int kMaterial = 0x0c;
constexpr unsigned int kChunk = 0x10;
constexpr unsigned int kNumIndices = 0x14;
constexpr unsigned int kIndices = 0x18;
constexpr unsigned int kNumPrimitives = 0x1c;
constexpr unsigned int kPrimitive = 0x20;
constexpr unsigned int kFlag24 = 0x24;
constexpr unsigned int kFlag25 = 0x25;
constexpr unsigned int kFlag26 = 0x26;
constexpr unsigned int kOpacity = 0x28;
constexpr unsigned int kLists = 0x2c;
constexpr unsigned int kMatrix = 0x44;
constexpr unsigned int kBasis = 0x84;
constexpr unsigned int kViewMatrix = 0xa8;
constexpr unsigned int kScissor = 0xe8;
constexpr unsigned int kProgram = 0xf8;
constexpr unsigned int kIndicesArray = 0xfc;
constexpr unsigned int kTextures = 0x108;

using FnCreateNewLightPass = void(__thiscall*)(void*, void*, void*);
using FnStateSetup = bool(__cdecl*)(void*, const void*, const void*);
using FnStateRenderSingle = void(__cdecl*)(void*, void*);
using FnStateSetScissor = void(__cdecl*)(void*, const void*, const void*);
using FnStateLoadTextures = void(__cdecl*)(void*, const void*);
using FnStateLoadPropsVP = void(__cdecl*)(void*, void*, const void*, void*);
using FnStateLoadPropsTC = void(__cdecl*)(void*, void*, const void*, void*);
using FnStateRenderGeometry = bool(__cdecl*)(void*, void*);
using FnStateStateCleanUp = void(__cdecl*)(void*);
using FnStateSetBlendState = void(__thiscall*)(void*);

FnStateSetup OrigStateSetup() { static FnStateSetup f = reinterpret_cast<FnStateSetup>(Original("?StateSetup@MultiPassRenderState@Jet@@IAA_NPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z")); return f; }
FnStateRenderSingle OrigRenderSingle() { static FnStateRenderSingle f = reinterpret_cast<FnStateRenderSingle>(Original("?RenderSingleLightPasses@MultiPassRenderState@Jet@@QAAXPAUMPLightRender@12@@Z")); return f; }
FnStateSetScissor OrigSetScissor() { static FnStateSetScissor f = reinterpret_cast<FnStateSetScissor>(Original("?SetScissor@MultiPassRenderState@Jet@@QBAXPBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@ABV?$Array@I$00@2@@Z")); return f; }
FnStateLoadTextures OrigLoadTextures() { static FnStateLoadTextures f = reinterpret_cast<FnStateLoadTextures>(Original("?LoadTextures@MultiPassRenderState@Jet@@QAAXABV?$Array@UMPTexture@MultiPassRenderState@Jet@@$03@2@@Z")); return f; }
FnStateLoadPropsVP OrigLoadPropsVP() { static FnStateLoadPropsVP f = reinterpret_cast<FnStateLoadPropsVP>(Original("?LoadRenderProperties@MultiPassRenderState@Jet@@QAAXAAVDisplayLight@2@ABV?$Array@ULightProperty@MultiPassRenderState@Jet@@$04@2@PAVVertexProgram@2@@Z")); return f; }
FnStateLoadPropsTC OrigLoadPropsTC() { static FnStateLoadPropsTC f = reinterpret_cast<FnStateLoadPropsTC>(Original("?LoadRenderProperties@MultiPassRenderState@Jet@@QAAXAAVDisplayLight@2@ABV?$Array@ULightProperty@MultiPassRenderState@Jet@@$04@2@PAVTextureCombiner@2@@Z")); return f; }

// The FastDynamicArray<Light*> heads: {Chunk* head}, chunk {next, prev, count, data[13]}.
struct ListHead {
    void* head;
};

unsigned int ListItems(const void* headAddress)
{
    const unsigned char* const head = *reinterpret_cast<unsigned char* const*>(headAddress);
    if (!head) return 0;
    unsigned int total = 0;
    const unsigned char* chunk = head;
    do {
        total += *reinterpret_cast<const unsigned int*>(chunk + 8);
        chunk = *reinterpret_cast<unsigned char* const*>(chunk);
    } while (chunk != head);
    return total;
}

void AppendItem(void* headAddress, void* item)
{
    unsigned char** const headSlot = static_cast<unsigned char**>(headAddress);
    unsigned char* head = *headSlot;
    if (!head) {
        unsigned char* const chunk = static_cast<unsigned char*>(Alloc::New(0x40));
        if (!chunk) {
            *headSlot = nullptr;
            return;
        }
        *reinterpret_cast<unsigned int*>(chunk + 8) = 0;
        *headSlot = chunk;
        *reinterpret_cast<unsigned char**>(chunk + 4) = chunk;
        *reinterpret_cast<unsigned char**>(chunk) = chunk;
        head = chunk;
    } else {
        unsigned char* tail = *reinterpret_cast<unsigned char**>(head + 4);
        if (*reinterpret_cast<unsigned int*>(tail + 8) == 13) {
            unsigned char* const chunk = static_cast<unsigned char*>(Alloc::New(0x40));
            if (!chunk) return;
            *reinterpret_cast<unsigned int*>(chunk + 8) = 0;
            *reinterpret_cast<unsigned char**>(chunk) = head;
            *reinterpret_cast<unsigned char**>(chunk + 4) = tail;
            *reinterpret_cast<unsigned char**>(head + 4) = chunk;
            *reinterpret_cast<unsigned char**>(tail) = chunk;
            tail = chunk;
        }
        const unsigned int count = *reinterpret_cast<unsigned int*>(tail + 8);
        *reinterpret_cast<void**>(tail + 0x0c + count * 4) = item;
        *reinterpret_cast<unsigned int*>(tail + 8) = count + 1;
        return;
    }
    const unsigned int count = *reinterpret_cast<unsigned int*>(head + 8);
    *reinterpret_cast<void**>(head + 0x0c + count * 4) = item;
    *reinterpret_cast<unsigned int*>(head + 8) = count + 1;
}

void FreeList(void* headAddress)
{
    unsigned char** const headSlot = static_cast<unsigned char**>(headAddress);
    unsigned char* const head = *headSlot;
    if (!head) return;
    unsigned char* chunk = head;
    do {
        unsigned char* const next = *reinterpret_cast<unsigned char**>(chunk);
        Alloc::Delete(chunk);
        chunk = next;
    } while (chunk != head);
    *headSlot = nullptr;
}

void CopyList(void* destination, const void* source)
{
    unsigned char** const dst = static_cast<unsigned char**>(destination);
    const unsigned char* const head = *reinterpret_cast<unsigned char* const*>(source);
    *dst = nullptr;
    if (!head) return;
    const unsigned char* chunk = head;
    do {
        const unsigned int count = *reinterpret_cast<const unsigned int*>(chunk + 8);
        for (unsigned int i = 0; i < count; ++i)
            AppendItem(destination, *reinterpret_cast<void* const*>(chunk + 0x0c + i * 4));
        chunk = *reinterpret_cast<const unsigned char* const*>(chunk);
    } while (chunk != head);
}

void* FirstItem(void* headAddress)
{
    unsigned char* const head = *static_cast<unsigned char**>(headAddress);
    if (!head) return nullptr;
    if (!*reinterpret_cast<unsigned int*>(head + 8)) return nullptr;
    return *reinterpret_cast<void**>(head + 0x0c);
}

// out = a * b, row-major, with the original's SSE accumulation order (the NaN payload of the first factors wins).
void MultiplyMatrix(const float* a, float* b)
{
    float tmp[16];
    std::memcpy(tmp, b, sizeof tmp);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float sum = a[row * 4 + 3] * tmp[3 * 4 + col];
            sum = a[row * 4 + 1] * tmp[1 * 4 + col] + sum;
            sum = sum + a[row * 4 + 2] * tmp[2 * 4 + col];
            sum = sum + a[row * 4 + 0] * tmp[0 * 4 + col];
            b[row * 4 + col] = sum;
        }
    }
}

void SetBlend(void* self)
{
    unsigned char* const p = Bytes(self);
    void* const display = *reinterpret_cast<void**>(p + kDisplay);
    void* const material = *reinterpret_cast<void**>(p + kMaterial);
    const bool flag24 = p[kFlag24] != 0;
    const bool flag25 = p[kFlag25] != 0;
    const bool flag26 = p[kFlag26] != 0;
    Display* const d = static_cast<Display*>(display);

    if (flag24) {
        if (flag25) {
            d->SetAlphaBlendMode(true);
            if (Bytes(material)[0x84] & 2) {
                d->SetAlphaBlendFunction(static_cast<Src::Factor>(4), static_cast<Dst::Factor>(5));
            } else {
                d->SetAlphaBlendFunction(static_cast<Src::Factor>(*reinterpret_cast<int*>(Bytes(material) + 0x78)),
                                         static_cast<Dst::Factor>(*reinterpret_cast<int*>(Bytes(material) + 0x7c)));
            }
        } else {
            d->SetAlphaBlendMode(false);
        }
        return;
    }
    if (flag25) {
        if (Bytes(material)[0x84] & 2) {
            d->SetAlphaBlendFunction(static_cast<Src::Factor>(4), static_cast<Dst::Factor>(5));
        } else {
            d->SetAlphaBlendFunction(static_cast<Src::Factor>(*reinterpret_cast<int*>(Bytes(material) + 0x78)),
                                     static_cast<Dst::Factor>(0));
        }
        return;
    }
    if (flag26) {
        d->SetAlphaBlendMode(true);
        d->SetAlphaBlendFunction(static_cast<Src::Factor>(1), static_cast<Dst::Factor>(3));
        return;
    }
    d->SetAlphaBlendMode(true);
    d->SetAlphaBlendFunction(static_cast<Src::Factor>(1), static_cast<Dst::Factor>(1));
}

}  // namespace

void* __thiscall Thunks::StateCtor1(const void* material, void* display, void* view, void* chunk,
                                    const void* lights, float opacity, bool opaque, const void* modelView,
                                    const void* textures, bool smoothBlend, bool flag)
{
    unsigned char* const p = Bytes(this);
    const bool many = ListItems(lights) > 1 || flag;
    *reinterpret_cast<int*>(p) = many ? 2 : 0;
    *reinterpret_cast<void**>(p + kDisplay) = display;
    *reinterpret_cast<void**>(p + kView) = view;
    *reinterpret_cast<void**>(p + kMaterial) = const_cast<void*>(material);
    *reinterpret_cast<void**>(p + kChunk) = chunk;
    *reinterpret_cast<unsigned int*>(p + kNumIndices) = 0;
    *reinterpret_cast<void**>(p + kIndices) = nullptr;
    *reinterpret_cast<unsigned int*>(p + kNumPrimitives) = 0;
    *reinterpret_cast<int*>(p + kPrimitive) = 0;
    p[kFlag24] = 0;
    p[kFlag25] = opaque ? 1 : 0;
    p[kFlag26] = smoothBlend ? 1 : 0;
    *reinterpret_cast<float*>(p + kOpacity) = opacity;
    for (unsigned int i = 0; i < 6; ++i) *reinterpret_cast<void**>(p + kLists + i * 4) = nullptr;
    std::memset(p + kMatrix, 0, 0x40);
    std::memset(p + kBasis, 0, 0x24);
    std::memset(p + kViewMatrix, 0, 0x40);
    *reinterpret_cast<unsigned int*>(p + kProgram) = 0;
    *reinterpret_cast<unsigned int*>(p + kIndicesArray) = 0;
    *reinterpret_cast<unsigned int*>(p + kIndicesArray + 4) = 0;
    *reinterpret_cast<void**>(p + kIndicesArray + 8) = nullptr;
    *reinterpret_cast<void**>(p + kTextures) = const_cast<void*>(textures);
    StateSetup(this, lights, modelView);
    return this;
}

void* __thiscall Thunks::StateCtor2(const void* material, void* display, void* view, void* chunk,
                                    const void* lights, float opacity, bool opaque, const void* modelView,
                                    const void* textures, bool smoothBlend, bool flag, unsigned int numIndices,
                                    const unsigned short* indices, unsigned int numPrimitives, int primitive)
{
    unsigned char* const p = Bytes(this);
    const bool many = ListItems(lights) > 1 || flag;
    *reinterpret_cast<int*>(p) = many ? 3 : 1;
    *reinterpret_cast<void**>(p + kDisplay) = display;
    *reinterpret_cast<void**>(p + kView) = view;
    *reinterpret_cast<void**>(p + kMaterial) = const_cast<void*>(material);
    *reinterpret_cast<void**>(p + kChunk) = chunk;
    *reinterpret_cast<unsigned int*>(p + kNumIndices) = numIndices;
    *reinterpret_cast<void**>(p + kIndices) = const_cast<unsigned short*>(indices);
    *reinterpret_cast<unsigned int*>(p + kNumPrimitives) = numPrimitives;
    *reinterpret_cast<int*>(p + kPrimitive) = primitive;
    p[kFlag24] = 0;
    p[kFlag25] = opaque ? 1 : 0;
    p[kFlag26] = smoothBlend ? 1 : 0;
    *reinterpret_cast<float*>(p + kOpacity) = opacity;
    for (unsigned int i = 0; i < 6; ++i) *reinterpret_cast<void**>(p + kLists + i * 4) = nullptr;
    std::memset(p + kMatrix, 0, 0x40);
    std::memset(p + kBasis, 0, 0x24);
    std::memset(p + kViewMatrix, 0, 0x40);
    *reinterpret_cast<unsigned int*>(p + kProgram) = 0;
    *reinterpret_cast<unsigned int*>(p + kIndicesArray) = 0;
    *reinterpret_cast<unsigned int*>(p + kIndicesArray + 4) = 0;
    *reinterpret_cast<void**>(p + kIndicesArray + 8) = nullptr;
    *reinterpret_cast<void**>(p + kTextures) = const_cast<void*>(textures);
    StateSetup(this, lights, modelView);
    return this;
}

void __thiscall Thunks::StateDtor()
{
    unsigned char* const p = Bytes(this);
    if (void* const data = *reinterpret_cast<void**>(p + kIndicesArray + 8)) {
        *reinterpret_cast<unsigned int*>(p + kIndicesArray) = 0;
        Alloc::Delete(data);
    }
    for (int i = 5; i >= 0; --i) FreeList(p + kLists + i * 4);
}

bool __cdecl StateSetup(void* self, const void* lights, const void* modelView)
{
    return Last::StateSetup(self, lights, modelView);
}

void __cdecl StateSetScissor(void* self, const void* lights, const void* scissor)
{
    Last::StateSetScissor(self, lights, scissor);
}

void __cdecl StateLoadTextures(void* self, const void* textures)
{
    unsigned char* const p = Bytes(self);
    const ArrayHeader& array = *static_cast<const ArrayHeader*>(textures);
    const unsigned int count = array.count;
    const PropertyPair* const pairs = static_cast<const PropertyPair*>(array.data);
    const ArrayHeader& textureArray = *reinterpret_cast<const ArrayHeader*>(p + kTextures);
    void** const texturesData = static_cast<void**>(textureArray.data);
    void** const stateData = *reinterpret_cast<void***>(p + kIndicesArray + 8);
    for (unsigned int i = 0; i < count; ++i) {
        void* const texture = texturesData[pairs[i].property];
        const unsigned int index = pairs[i].value;
        if (stateData[index] == texture) continue;
        reinterpret_cast<void(__cdecl*)(void*, unsigned int)>((*reinterpret_cast<void***>(texture))[0x44 / 4])(
            texture, index);
        stateData[index] = texture;
    }
}

void __cdecl StateGetAmbientLights(void* self, float r, float g, float b, float a)
{
    (void)a;
    unsigned char* const p = Bytes(self);
    float remaining[3] = {r, g, b};
    while (remaining[0] > 0.0f || remaining[1] > 0.0f || remaining[2] > 0.0f) {
        Color color{};
        for (int channel = 0; channel < 3; ++channel) {
            float value = remaining[channel];
            if (value > 1.0f) {
                value = 1.0f;
                remaining[channel] -= 1.0f;
            } else {
                remaining[channel] = 0.0f;
            }
            (&color.r)[channel] = value;
        }
        color.a = 1.0f;
        void* const memory = Alloc::New(sizeof(AmbientLight));
        if (!memory) continue;
        AmbientLight* const light = ::new (memory) AmbientLight(Vector3(0.0f, 0.0f, 0.0f), 1.0f, color);
        AppendItem(p + kLists, light);
    }

    bool any = false;
    for (int list = 0; list < 6 && !any; ++list) any = ListItems(p + kLists + list * 4) != 0;
    if (!any) {
        Color color{0.0f, 0.0f, 0.0f, 1.0f};
        void* const memory = Alloc::New(sizeof(AmbientLight));
        if (memory) {
            AmbientLight* const light = ::new (memory) AmbientLight(Vector3(0.0f, 0.0f, 0.0f), 1.0f, color);
            AppendItem(p + kLists, light);
        }
    }
}

void* __cdecl StateGetAmbientLight0(void* self)
{
    unsigned char* const p = Bytes(self);
    if (ListItems(p + kLists) == 0) {
        Color color{0.0f, 0.0f, 0.0f, 1.0f};
        void* const memory = Alloc::New(sizeof(AmbientLight));
        if (memory) {
            AmbientLight* const light = ::new (memory) AmbientLight(Vector3(0.0f, 0.0f, 0.0f), 1.0f, color);
            AppendItem(p + kLists, light);
        }
    }
    return FirstItem(p + kLists);
}

void __cdecl StateDebugRenderSingleLightPasses(void*, void*) {}
void __cdecl StateDebugRenderMixedLightPasses(void*, void*) {}


void __cdecl StateLoadPropertiesVP(void* self, void* light, const void* properties, void* program)
{
    Last::StateLoadPropertiesVP(self, light, properties, program);
}

void __cdecl StateLoadPropertiesTC(void* self, void* light, const void* properties, void* combiner)
{
    Last::StateLoadPropertiesTC(self, light, properties, combiner);
}

void __cdecl StateRenderSingleLightPasses(void* self, void* lights)
{
    Last::StateRenderSingleLightPasses(self, lights);
}

bool __cdecl StateRenderGeometry(void* self, void* program)
{
    unsigned char* const p = Bytes(self);
    const int mode = *reinterpret_cast<int*>(p);
    if (p[kFlag24]) {
        unsigned int a = 0;
        unsigned int b = 0;
        if (mode == 3) {
            a = *reinterpret_cast<unsigned int*>(p + kNumIndices);
            b = reinterpret_cast<unsigned int>(*reinterpret_cast<void**>(p + kIndices));
        } else if (mode != 2) {
            return true;
        }
        using SetupFn = bool(__cdecl*)(void*, void*, void*, void*, unsigned int, unsigned int);
        void* const chunk = *reinterpret_cast<void**>(p + kChunk);
        const bool ok = reinterpret_cast<SetupFn>((*reinterpret_cast<void***>(chunk))[0x44 / 4])(
            chunk, *reinterpret_cast<void**>(p + kDisplay), *reinterpret_cast<void**>(p + kView), program, a, b);
        if (!ok) return false;
        *reinterpret_cast<void**>(p + kProgram) = program;
    }

    void* const chunk = *reinterpret_cast<void**>(p + kChunk);
    void** const vtable = *reinterpret_cast<void***>(chunk);
    void* const display = *reinterpret_cast<void**>(p + kDisplay);
    void* const view = *reinterpret_cast<void**>(p + kView);
    switch (mode) {
    case 0:
        reinterpret_cast<void(__cdecl*)(void*, void*, void*, void*, int, int)>(vtable[0x38 / 4])(chunk, display, view,
                                                                                                 program, 1, 0);
        break;
    case 1:
        reinterpret_cast<void(__cdecl*)(void*, void*, void*, void*, unsigned int, void*, unsigned int, int, int, int)>(
            vtable[0x3c / 4])(chunk, display, view, program, *reinterpret_cast<unsigned int*>(p + kNumIndices),
                              *reinterpret_cast<void**>(p + kIndices), *reinterpret_cast<unsigned int*>(p + kNumPrimitives),
                              *reinterpret_cast<int*>(p + kPrimitive), 1, 0);
        break;
    case 2:
        if (*reinterpret_cast<void**>(p + kProgram) != program)
            reinterpret_cast<void(__cdecl*)(void*, void*, void*, int)>(vtable[0x4c / 4])(
                chunk, *reinterpret_cast<void**>(p + kProgram), program, 0);
        reinterpret_cast<void(__cdecl*)(void*, void*, void*, void*, int)>(vtable[0x54 / 4])(chunk, display, view,
                                                                                            program, 0);
        *reinterpret_cast<void**>(p + kProgram) = program;
        break;
    case 3:
        if (*reinterpret_cast<void**>(p + kProgram) != program)
            reinterpret_cast<void(__cdecl*)(void*, void*, void*, int)>(vtable[0x4c / 4])(
                chunk, *reinterpret_cast<void**>(p + kProgram), program, 0);
        reinterpret_cast<void(__cdecl*)(void*, void*, void*, unsigned int, void*, unsigned int, int, int)>(
            vtable[0x50 / 4])(chunk, display, view, *reinterpret_cast<unsigned int*>(p + kNumIndices),
                              *reinterpret_cast<void**>(p + kIndices), *reinterpret_cast<unsigned int*>(p + kNumPrimitives),
                              *reinterpret_cast<int*>(p + kPrimitive), 0);
        *reinterpret_cast<void**>(p + kProgram) = program;
        break;
    default:
        break;
    }
    if (p[kFlag24]) {
        p[kFlag24] = 0;
        SetBlend(self);
    }
    return true;
}

void __cdecl StateCleanUp(void* self)
{
    unsigned char* const p = Bytes(self);
    Display* const display = static_cast<Display*>(*reinterpret_cast<void**>(p + kDisplay));
    display->SetScissor(*reinterpret_cast<Rect*>(p + kScissor));
    const int mode = *reinterpret_cast<int*>(p);
    if ((mode == 2 || mode == 3) && !p[kFlag24]) {
        void* const chunk = *reinterpret_cast<void**>(p + kChunk);
        void* const program = *reinterpret_cast<void**>(p + kProgram);
        reinterpret_cast<void(__cdecl*)(void*, void*, void*)>((*reinterpret_cast<void***>(chunk))[0x58 / 4])(
            chunk, display, program);
    }
    const unsigned int count = *reinterpret_cast<unsigned int*>(p + kIndicesArray);
    void** const data = *reinterpret_cast<void***>(p + kIndicesArray + 8);
    for (unsigned int i = 0; i < count; ++i) {
        if (data[i])
            reinterpret_cast<void(__cdecl*)(void*, unsigned int)>((*reinterpret_cast<void***>(display))[0x12c / 4])(
                display, i);
    }
    for (int list = 0; list < 6; ++list) {
        unsigned char* const head = *reinterpret_cast<unsigned char**>(p + kLists + list * 4);
        if (!head) continue;
        unsigned char* chunk = head;
        do {
            const unsigned int n = *reinterpret_cast<unsigned int*>(chunk + 8);
            for (unsigned int i = 0; i < n; ++i) {
                void* const light = *reinterpret_cast<void**>(chunk + 0x0c + i * 4);
                if (light)
                    reinterpret_cast<void(__cdecl*)(void*, int)>((*reinterpret_cast<void***>(light))[0])(light, 1);
            }
            chunk = *reinterpret_cast<unsigned char**>(chunk);
        } while (chunk != head);
    }
    *reinterpret_cast<void**>(p + kProgram) = nullptr;
}

void SetAlphaBlendModeRaw(void* display, unsigned int value)
{
    using Fn = void(__cdecl*)(void*, int);
    static Fn fn = [] {
        HMODULE rs = GetModuleHandleA("RenderSystem_7c.dll");
        if (!rs) rs = LoadLibraryA("RenderSystem_7c.dll");
        return rs ? reinterpret_cast<Fn>(GetProcAddress(rs, "?SetAlphaBlendMode@Display@Jet@@QAAX_N@Z")) : nullptr;
    }();
    if (fn) fn(display, static_cast<int>(value));
}

void __thiscall Thunks::StateSetBlendState() { SetBlend(this); }

void __cdecl StateSetupExternalRender(void* self)
{
    unsigned char* const p = Bytes(self);
    Display* const display = static_cast<Display*>(*reinterpret_cast<void**>(p + kDisplay));
    display->SetScissor(*reinterpret_cast<Rect*>(p + kScissor));
    const unsigned int count = *reinterpret_cast<unsigned int*>(p + kIndicesArray);
    void** const data = *reinterpret_cast<void***>(p + kIndicesArray + 8);
    for (unsigned int i = 0; i < count; ++i) {
        if (!data[i]) continue;
        reinterpret_cast<void(__cdecl*)(void*, unsigned int)>((*reinterpret_cast<void***>(display))[0x12c / 4])(
            display, i);
    }
}

void* __thiscall Thunks::StateCopyCtor(const void* source)
{
    const unsigned char* const s = Bytes(source);
    unsigned char* const d = Bytes(this);
    std::memcpy(d, s, 0x44);
    for (unsigned int i = 0; i < 6; ++i) CopyList(d + kLists + i * 4, s + kLists + i * 4);
    std::memcpy(d + kMatrix, s + kMatrix, 0x40);
    std::memcpy(d + kBasis, s + kBasis, 0x24);
    std::memcpy(d + kViewMatrix, s + kViewMatrix, 0x40);
    std::memcpy(d + kScissor, s + kScissor, 0x10);
    *reinterpret_cast<void**>(d + kProgram) = *reinterpret_cast<void* const*>(s + kProgram);
    *reinterpret_cast<unsigned int*>(d + kIndicesArray) = 0;
    *reinterpret_cast<unsigned int*>(d + kIndicesArray + 4) = 0;
    *reinterpret_cast<void**>(d + kIndicesArray + 8) = nullptr;
    const unsigned int count = *reinterpret_cast<const unsigned int*>(s + kIndicesArray);
    if (count) {
        unsigned int* const array = static_cast<unsigned int*>(Alloc::New(count * 4));
        std::memcpy(array, *reinterpret_cast<void* const*>(s + kIndicesArray + 8), count * 4);
        *reinterpret_cast<unsigned int*>(d + kIndicesArray) = count;
        *reinterpret_cast<unsigned int*>(d + kIndicesArray + 4) = count;
        *reinterpret_cast<void**>(d + kIndicesArray + 8) = array;
    }
    *reinterpret_cast<void**>(d + kTextures) = *reinterpret_cast<void* const*>(s + kTextures);
    return this;
}

}  // namespace Final
}  // namespace MultiPass
}  // namespace Jet
