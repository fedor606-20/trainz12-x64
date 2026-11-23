// U9: accessors and cached backend changes for the viewport, 2D projection and clear/cull state.
#include <jet/RenderSystem.h>
#include <jet/Bitmap.h>
#include <jet/Geometry.h>
#include <jet/GfxLanguage.h>
#include <jet/Window.h>

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <new>

#include "LegacyPower.h"

namespace Jet {

extern const DCull DisplayInverseCull[3]{DCULL_NONE, static_cast<DCull>(2), static_cast<DCull>(1)};

namespace {


template<class T>
const T& ReadState(const Display* display, unsigned offset)
{
    return *reinterpret_cast<const T*>(display->State() + offset);
}

template<class T>
T& WriteState(Display* display, unsigned offset)
{
    return const_cast<T&>(ReadState<T>(display, offset));
}

bool SameRect(const Rect& a, const Rect& b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool SameColor(const Color& a, const Color& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool Bit(const unsigned char* state, unsigned offset, unsigned bit)
{
    return ((state[offset] >> bit) & 1) != 0;
}

void SetBit(unsigned char* state, unsigned offset, unsigned bit, bool on)
{
    const unsigned char mask = static_cast<unsigned char>(1u << bit);
    state[offset] = static_cast<unsigned char>(on ? (state[offset] | mask) : (state[offset] & ~mask));
}

int LightCount(const Display* display)
{
    return *reinterpret_cast<const int*>(reinterpret_cast<const char*>(display) + 0x50);
}

unsigned char* LightFlags(const Display* display)
{
    auto* state = const_cast<unsigned char*>(display->State());
    return *reinterpret_cast<unsigned char**>(state + 0x110);
}

void SetProjection(Display* display, int kind, const float (&bounds)[6])
{
    const auto& current = ReadState<float[6]>(display, 0x08);
    bool changed = ReadState<int>(display, 0x04) != kind;
    for (unsigned i = 0; i < 6; ++i) changed |= current[i] != bounds[i];
    if (!changed) return;
    WriteState<int>(display, 0x04) = kind;
    auto& dest = WriteState<float[6]>(display, 0x08);
    for (unsigned i = 0; i < 6; ++i) dest[i] = bounds[i];
    VCall<void>(display, 0x180, bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5]);
}
}

const Rect& __cdecl Display::GetViewport() const { return ReadState<Rect>(this, 0x44); }
Rect __cdecl Display::GetScissor() { return ReadState<Rect>(this, 0x54); }

void __cdecl Display::SetViewport(const Rect& rect)
{
    if (!SameRect(GetViewport(), rect)) {
        VCall<void>(this, 0x190, &rect);
        WriteState<Rect>(this, 0x44) = rect;
    } else if (SameRect(ReadState<Rect>(this, 0x54), rect)) {
        return;
    }
    // A changed viewport always resets the scissor, even if that rectangle already matches.
    VCall<void>(this, 0x194, &rect);
    WriteState<Rect>(this, 0x54) = rect;
}

void __cdecl Display::SetScissor(const Rect& rect)
{
    if (SameRect(ReadState<Rect>(this, 0x54), rect)) return;
    VCall<void>(this, 0x194, &rect);
    WriteState<Rect>(this, 0x54) = rect;
}

bool __cdecl Display::GetScissorMode() const { return ReadState<bool>(this, 0x64); }
void __cdecl Display::SetScissorMode(bool on)
{
    if (ReadState<unsigned char>(this, 0x64) == static_cast<unsigned char>(on)) return;
    VCall<void>(this, 0x198, on);
    WriteState<bool>(this, 0x64) = on;
}

const Size& __cdecl Display::GetWindowSize() const { return *reinterpret_cast<const Size*>(Raw() + 0x28c); }
const Size& __cdecl Display::GetTargetSize() const
{
    const auto* target = *reinterpret_cast<const char* const*>(Raw() + 0x5c);
    return target ? *reinterpret_cast<const Size*>(target + 0x10) : GetWindowSize();
}

void __cdecl Display::Set2DView(float l, float r, float b, float t, float n, float f)
{
    const float bounds[]{l, r, b, t, n, f};
    SetProjection(this, 0, bounds);
}

void __cdecl Display::Set2DViewNormalized(bool normalized)
{
    if (!normalized) {
        const auto& size = GetTargetSize();
        Set2DView(0, static_cast<float>(size.w), static_cast<float>(size.h), 0, -1000, 1000);
        return;
    }
    constexpr float bounds[]{0, 1, 1, 0, -1, 1};
    SetProjection(this, 1, bounds);
}

const Color& __cdecl Display::GetClearColor() const { return ReadState<Color>(this, 0x2c); }
float __cdecl Display::GetClearDepth() const { return ReadState<float>(this, 0x3c); }
long __cdecl Display::GetClearStencil() const { return ReadState<long>(this, 0x40); }

void __cdecl Display::SetClearColor(const Color& color)
{
    const auto& old = GetClearColor();
    if (old.r == color.r && old.g == color.g && old.b == color.b && old.a == color.a) return;
    VCall<void>(this, 0x1a0, &color);
    WriteState<Color>(this, 0x2c) = color;
}

void __cdecl Display::SetClearDepth(float depth)
{
    if (GetClearDepth() == depth) return;
    VCall<void>(this, 0x1a4, depth);
    WriteState<float>(this, 0x3c) = depth;
}

void __cdecl Display::SetClearStencil(long stencil)
{
    if (GetClearStencil() == stencil) return;
    VCall<void>(this, 0x1a8, stencil);
    WriteState<long>(this, 0x40) = stencil;
}

DCull __cdecl Display::GetCullMode() const { return ReadState<DCull>(this, 0x28); }
bool __cdecl Display::GetReflectionCull() const { return *reinterpret_cast<const bool*>(Raw() + 0x7c); }

void __cdecl Display::SetCullMode(DCull mode)
{
    if (GetCullMode() == mode) return;
    VCall<void>(this, 0x1d8, GetReflectionCull() ? DisplayInverseCull[mode] : mode);
    WriteState<DCull>(this, 0x28) = mode;
}

void __cdecl Display::SetReflectionCull(bool on)
{
    auto& current = *reinterpret_cast<bool*>(const_cast<char*>(Raw()) + 0x7c);
    if (current == on) return;
    current = on;
    const auto mode = GetCullMode();
    if (mode != DCULL_NONE) {
        WriteState<DCull>(this, 0x28) = DCULL_NONE;
        SetCullMode(mode);
    }
}

DColorMaterialTypes __cdecl Display::GetColorMaterialMode() { return ReadState<DColorMaterialTypes>(this, 0x98); }
void __cdecl Display::SetColorMaterialMode(DColorMaterialTypes mode)
{
    if (GetColorMaterialMode() == mode) return;
    VCall<void>(this, 0x20c, mode);
    WriteState<DColorMaterialTypes>(this, 0x98) = mode;
}

// ---- render state block (blend, depth, alpha test)

void __cdecl Display::GetAlphaBlendFunction(Src::Factor& src, Dst::Factor& dst) const
{
    src = static_cast<Src::Factor>(ReadState<int>(this, 0x68));
    dst = static_cast<Dst::Factor>(ReadState<int>(this, 0x6c));
}

void __cdecl Display::SetAlphaBlendFunction(Src::Factor src, Dst::Factor dst)
{
    if (ReadState<int>(this, 0x68) == src && ReadState<int>(this, 0x6c) == dst) return;
    VCall<void>(this, 0x1dc, src, dst);
    WriteState<int>(this, 0x68) = src;
    WriteState<int>(this, 0x6c) = dst;
}

DBlendEquation __cdecl Display::GetAlphaBlendEquation() const
{
    return static_cast<DBlendEquation>(ReadState<int>(this, 0x70));
}

void __cdecl Display::SetAlphaBlendEquation(DBlendEquation equation)
{
    if (GetAlphaBlendEquation() == equation) return;
    VCall<void>(this, 0x1e0, equation);
    WriteState<int>(this, 0x70) = equation;
}

unsigned long __cdecl Display::GetColorWriteMask() const { return ReadState<unsigned long>(this, 0x9c); }
void __cdecl Display::SetColorWriteMask(unsigned long mask)
{
    if (GetColorWriteMask() == mask) return;
    VCall<void>(this, 0x1ac, 1, mask);
    WriteState<unsigned long>(this, 0x9c) = mask;
}

bool __cdecl Display::GetDepthWriteMode() const { return Bit(State(), 0xa0, 1); }
void __cdecl Display::SetDepthWriteMode(bool on)
{
    if (GetDepthWriteMode() == on) return;
    // The backend shares this slot with the 32-bit color write mask.
    VCall<void>(this, 0x1ac, 2, static_cast<unsigned long>(on));
    SetBit(&WriteState<unsigned char>(this, 0xa0), 0, 1, on);
}

bool __cdecl Display::GetDepthTestMode() const { return Bit(State(), 0xa0, 2); }
void __cdecl Display::SetDepthTestMode(bool on)
{
    if (GetDepthTestMode() == on) return;
    VCall<void>(this, 0x1b4, on);
    SetBit(&WriteState<unsigned char>(this, 0xa0), 0, 2, on);
}

DFunction __cdecl Display::GetDepthFunction() const
{
    return static_cast<DFunction>(ReadState<int>(this, 0xd4));
}

void __cdecl Display::SetDepthFunction(DFunction function)
{
    if (GetDepthFunction() == function) return;
    VCall<void>(this, 0x1b0, function);
    WriteState<int>(this, 0xd4) = function;
}

bool __cdecl Display::GetDepthClamp() const { return Bit(State(), 0xa0, 6); }
void __cdecl Display::SetDepthClamp(bool on)
{
    if (GetDepthClamp() == on) return;
    VCall<void>(this, 0x1b8, on);
    SetBit(&WriteState<unsigned char>(this, 0xa0), 0, 6, on);
}

bool __cdecl Display::GetAlphaTestMode() const { return Bit(State(), 0xa0, 5); }
void __cdecl Display::SetAlphaTestMode(bool on)
{
    if (GetAlphaTestMode() == on) return;
    VCall<void>(this, 0x1e4, on);
    SetBit(&WriteState<unsigned char>(this, 0xa0), 0, 5, on);
}

float __cdecl Display::GetAlphaTestReference() const { return ReadState<float>(this, 0xcc); }
DFunction __cdecl Display::GetAlphaTestFunction() const
{
    return static_cast<DFunction>(ReadState<int>(this, 0xd0));
}

void __cdecl Display::SetAlphaTestFunction(DFunction function, float reference)
{
    // The original clamps through comiss, so an unordered value becomes zero.
    const float clamped = !(reference > 0.0f) ? 0.0f : (reference > 1.0f ? 1.0f : reference);
    if (GetAlphaTestFunction() == function && GetAlphaTestReference() == clamped) return;
    VCall<void>(this, 0x1e8, function, clamped);
    WriteState<int>(this, 0xd0) = function;
    WriteState<float>(this, 0xcc) = clamped;
}

bool __cdecl Display::GetAlphaBlendMode() const { return Bit(State(), 0xa1, 0); }
void __cdecl Display::SetAlphaBlendMode(bool on)
{
    if (GetAlphaBlendMode() == on) return;
    VCall<void>(this, 0x1ec, on);
    SetBit(&WriteState<unsigned char>(this, 0xa1), 0, 0, on);
}

// ---- lighting, fog and ambient state

bool __cdecl Display::GetLightingMode() const { return Bit(State(), 0xa1, 2); }
void __cdecl Display::SetLightingMode(bool on)
{
    if (GetLightingMode() == on) return;
    VCall<void>(this, 0x1f0, on);
    SetBit(&WriteState<unsigned char>(this, 0xa1), 0, 2, on);
}

bool __cdecl Display::GetSeperateSpecularMode() { return ReadState<unsigned char>(this, 0x74) != 0; }
void __cdecl Display::SetSeperateSpecularMode(bool on)
{
    if (ReadState<unsigned char>(this, 0x74) == static_cast<unsigned char>(on)) return;
    VCall<void>(this, 0x1f4, on);
    WriteState<unsigned char>(this, 0x74) = on;
}

bool __cdecl Display::GetLocalViewSpecularMode() const { return ReadState<unsigned char>(this, 0x75) != 0; }
void __cdecl Display::SetLocalViewSpecularMode(bool on)
{
    if (ReadState<unsigned char>(this, 0x75) == static_cast<unsigned char>(on)) return;
    VCall<void>(this, 0x1f8, on);
    WriteState<unsigned char>(this, 0x75) = on;
}

DFogMode __cdecl Display::GetFogMode() const { return static_cast<DFogMode>(ReadState<int>(this, 0x78)); }
void __cdecl Display::SetFogMode(DFogMode mode)
{
    if (GetFogMode() == mode) return;
    VCall<void>(this, 0x1fc, mode);
    WriteState<int>(this, 0x78) = mode;
}

float __cdecl Display::GetFogDensity() const { return ReadState<float>(this, 0x8c); }
void __cdecl Display::SetFogDensity(float density)
{
    if (GetFogDensity() == density) return;
    VCall<void>(this, 0x200, density);
    WriteState<float>(this, 0x8c) = density;
}

void __cdecl Display::GetFogRange(float& start, float& end) const
{
    start = ReadState<float>(this, 0x90);
    end = ReadState<float>(this, 0x94);
}

void __cdecl Display::SetFogRange(float start, float end)
{
    if (ReadState<float>(this, 0x90) == start && ReadState<float>(this, 0x94) == end) return;
    VCall<void>(this, 0x204, start, end);
    WriteState<float>(this, 0x90) = start;
    WriteState<float>(this, 0x94) = end;
}

const Color& __cdecl Display::GetFogColor() const { return ReadState<Color>(this, 0x7c); }
void __cdecl Display::SetFogColor(const Color& color)
{
    if (SameColor(GetFogColor(), color)) return;
    VCall<void>(this, 0x208, &color);
    WriteState<Color>(this, 0x7c) = color;
}

int __cdecl Display::GetNumberOfHardwareLights() const { return LightCount(this); }

bool __cdecl Display::GetLightMode(int index) const
{
    if (index < 0 || index >= LightCount(this)) return false;
    return LightFlags(this)[index] != 0;
}

bool __cdecl Display::SetLightMode(int index, bool on)
{
    if (index < 0 || index >= LightCount(this)) return false;
    if (LightFlags(this)[index] == static_cast<unsigned char>(on)) return true;
    VCall<void>(this, 0x224, index, on);
    LightFlags(this)[index] = on;
    return true;
}

void __cdecl Display::DisableAllLights()
{
    for (int i = 0; i < LightCount(this); ++i) {
        if (!LightFlags(this)[i]) continue;
        VCall<void>(this, 0x224, i, false);
        LightFlags(this)[i] = 0;
    }
}

bool __cdecl Display::SetLightState(int index, const DisplayLight& light, bool on)
{
    if (index < 0 || index >= LightCount(this)) return false;
    VCall<void>(this, 0x22c, index, &light);
    VCall<void>(this, 0x224, index, on);
    LightFlags(this)[index] = on;
    return true;
}

// StartLightingUpdate is virtual in the original; this adapter has its stack-this signature.
void __cdecl RsDisplayStartLightingUpdate(Display* display, int first, const Matrix4& transform)
{
    for (int i = first; i >= 0 && i < LightCount(display); ++i) {
        if (!LightFlags(display)[i]) continue;
        VCall<void>(display, 0x224, i, false);
        LightFlags(display)[i] = 0;
    }
    VCall<void>(display, 0x228, &transform);
}

// Retail's base implementation is empty; renderer subclasses can override it.
void __cdecl RsDisplayEndLightingUpdate(Display*) {}

const Color& __cdecl Display::GetGlobalAmbient() const { return ReadState<Color>(this, 0xa4); }
unsigned long __cdecl Display::GetGlobalAmbientConverted() const { return ReadState<unsigned long>(this, 0xb4); }

void __cdecl Display::SetGlobalAmbient(const Color& color)
{
    if (SameColor(GetGlobalAmbient(), color)) return;
    VCall<void>(this, 0x230, &color);
    auto* state = const_cast<unsigned char*>(State());
    *reinterpret_cast<Color*>(state + 0xa4) = color;
    *reinterpret_cast<unsigned long*>(state + 0xb4) = ConvertColor(color);
}

namespace {
DFormatId* FindRetailInternalColorFormat()
{
    const HMODULE module = GetModuleHandleA("RenderSystem_7c_orig.dll");
    return module ? reinterpret_cast<DFormatId*>(GetProcAddress(module,
        "?internalColorFormat@Display@Jet@@1W4DFormatId@2@A")) : nullptr;
}
}

DFormatId __cdecl Display::GetInternalColorFormat()
{
    // U16: `internalColorFormat` is a mirror. While the retail module is loaded, read its live static:
    // renderer plugins and the differential probes change it at run time. Only fall back to the local
    // copy when the original is absent (the stand-alone configuration).
    extern unsigned char RsRsD037[4];
    static DFormatId* const retail = FindRetailInternalColorFormat();
    return retail ? *retail : *reinterpret_cast<const DFormatId*>(RsRsD037);
}

namespace {
bool IsPackedColorFormat(DFormatId format)
{
    return format == 1 || format == 2;
}

unsigned long PackColor(const Color& color, DFormatId format)
{
    const auto channel = [](float value) -> unsigned long {
        const float clamped = !(value > 0.0f) ? 0.0f : (value > 1.0f ? 1.0f : value);
        return static_cast<unsigned long>(clamped * 255.0f);
    };
    const unsigned long r = channel(color.r);
    const unsigned long g = channel(color.g);
    const unsigned long b = channel(color.b);
    const unsigned long a = channel(color.a);
    return format == 1 ? (a << 24) | (r << 16) | (g << 8) | b
                       : (a << 24) | (b << 16) | (g << 8) | r;
}

void UnpackColor(Color& out, unsigned long packed, DFormatId format)
{
    const auto channel = [packed](unsigned shift) {
        return static_cast<float>((packed >> shift) & 0xff) * 0.003921568859368563f;
    };
    out.r = channel(format == 1 ? 16 : 0);
    out.g = channel(8);
    out.b = channel(format == 1 ? 0 : 16);
    out.a = channel(24);
}

unsigned long SwapRedBlue(unsigned long value)
{
    return (value & 0xff00ff00ul) | ((value & 0x00ff0000ul) >> 16) | ((value & 0x000000fful) << 16);
}
}

unsigned long __cdecl Display::ConvertColor(const Color& color)
{
    const auto format = GetInternalColorFormat();
    return IsPackedColorFormat(format) ? PackColor(color, format) : 0xfffffffful;
}

// This x86 export returns Color through a caller-provided buffer.
Color* __cdecl RsDisplayDecodeColor(Color* out, unsigned long packed)
{
    const auto format = Display::GetInternalColorFormat();
    if (IsPackedColorFormat(format))
        UnpackColor(*out, packed, format);
    return out;
}

void __cdecl Display::ConvertColorArray(const Color* source, unsigned long* dest, int count)
{
    const auto format = GetInternalColorFormat();
    if (!IsPackedColorFormat(format)) return;
    for (int i = 0; i < count; ++i)
        dest[i] = PackColor(source[i], format);
}

void __cdecl Display::ConvertColorArray(unsigned long* source, Color* dest, int count)
{
    const auto format = GetInternalColorFormat();
    if (!IsPackedColorFormat(format)) return;
    for (int i = 0; i < count; ++i)
        UnpackColor(dest[i], source[i], format);
}

void __cdecl Display::ConvertColorArray(unsigned long* colors, int count)
{
    if (GetInternalColorFormat() != 1) return;
    for (int i = 0; i < count; ++i)
        colors[i] = SwapRedBlue(colors[i]);
}

// The exported VertexItemPtr is passed by value as a byte stride followed by a pointer on x86.
void __cdecl RsDisplayConvertStridedColors(int stride, void* first, int count)
{
    if (Display::GetInternalColorFormat() != 1 || count <= 0) return;
    auto* item = static_cast<unsigned char*>(first);
    for (;;) {
        unsigned long packed;
        std::memcpy(&packed, item, sizeof packed);
        packed = SwapRedBlue(packed);
        std::memcpy(item, &packed, sizeof packed);
        if (--count == 0) break;
        item += stride;
    }
}

namespace {
const unsigned char* DisplayBytes(const Display* display)
{
    return reinterpret_cast<const unsigned char*>(display);
}

unsigned char* DisplayBytes(Display* display)
{
    return const_cast<unsigned char*>(DisplayBytes(static_cast<const Display*>(display)));
}
}

bool __cdecl Display::GetWireFrameMode() const { return Bit(State(), 0xa0, 0); }
void __cdecl Display::SetWireFrameMode(bool on)
{
    if (GetWireFrameMode() == on) return;
    VCall<void>(this, 0x18c, on);
    SetBit(&WriteState<unsigned char>(this, 0xa0), 0, 0, on);
}

bool __cdecl Display::GetOutlinesMode() const { return Bit(DisplayBytes(this), 0x71, 0); }
void __cdecl Display::SetOutlinesMode(bool on) { SetBit(DisplayBytes(this), 0x71, 0, on); }
bool __cdecl Display::GetVertexNormalsMode() const { return Bit(DisplayBytes(this), 0x71, 1); }
void __cdecl Display::SetVertexNormalsMode(bool on) { SetBit(DisplayBytes(this), 0x71, 1, on); }
bool __cdecl Display::GetTriangleNormalsMode() const { return Bit(DisplayBytes(this), 0x71, 2); }
void __cdecl Display::SetTriangleNormalsMode(bool on) { SetBit(DisplayBytes(this), 0x71, 2, on); }
bool __cdecl Display::GetLocalAxesMode() const { return Bit(DisplayBytes(this), 0x71, 3); }
void __cdecl Display::SetLocalAxesMode(bool on) { SetBit(DisplayBytes(this), 0x71, 3, on); }

float __cdecl Display::GetNormalLength() const
{
    return *reinterpret_cast<const float*>(Raw() + 0x74);
}
void __cdecl Display::SetNormalLength(float length)
{
    *reinterpret_cast<float*>(DisplayBytes(this) + 0x74) = length;
}

bool __cdecl Display::GetChunkColorMode() const { return DisplayBytes(this)[0x78] != 0; }
void __cdecl Display::SetChunkColorMode(bool on) { DisplayBytes(this)[0x78] = on; }
bool __cdecl Display::GetChunkVertexCountMode() const { return DisplayBytes(this)[0x79] != 0; }
void __cdecl Display::SetChunkVertexCountMode(bool on) { DisplayBytes(this)[0x79] = on; }
bool __cdecl Display::GetStaticGeometryDisplayMode() const { return DisplayBytes(this)[0x7a] != 0; }
void __cdecl Display::SetStaticGeometryDisplayMode(bool on) { DisplayBytes(this)[0x7a] = on; }
bool __cdecl Display::GetDynamicGeometryDisplayMode() const { return DisplayBytes(this)[0x7b] != 0; }
void __cdecl Display::SetDynamicGeometryDisplayMode(bool on) { DisplayBytes(this)[0x7b] = on; }

bool __cdecl Display::GetStencilTestMode() const { return Bit(State(), 0xa0, 3); }
void __cdecl Display::SetStencilTestMode(bool on, bool twoSided)
{
    if (GetStencilTestMode() == on && Bit(State(), 0xa0, 4) == twoSided) return;
    const bool accepted = VCall<bool>(this, 0x1bc, on, twoSided);
    SetBit(&WriteState<unsigned char>(this, 0xa0), 0, 3, accepted && on);
    if (accepted)
        SetBit(&WriteState<unsigned char>(this, 0xa0), 0, 4, twoSided);
}

namespace {
struct StencilFunction {
    DFunction function;
    int reference;
    unsigned long mask;
    bool operator==(const StencilFunction&) const = default;
};

struct StencilOperation {
    DOperation fail;
    DOperation depthFail;
    DOperation pass;
    bool operator==(const StencilOperation&) const = default;
};

struct StencilFace {
    StencilFunction function;
    unsigned long writeMask;
    StencilOperation operation;
};
static_assert(sizeof(StencilFace) == 0x1c);

const StencilFace& FaceState(const Display* display, DFace face)
{
    return reinterpret_cast<const StencilFace*>(display->State() + 0xd8)[static_cast<int>(face)];
}
}

unsigned long __cdecl Display::GetStencilWriteMask(DFace face) const { return FaceState(this, face).writeMask; }
void __cdecl Display::SetStencilWriteMask(unsigned long mask, DFace face)
{
    auto& state = const_cast<StencilFace&>(FaceState(this, face));
    if (state.writeMask == mask) return;
    VCall<void>(this, 0x1c0, face, mask);
    state.writeMask = mask;
}

void __cdecl Display::SetStencilFunction(DFunction function, int reference, unsigned long mask, DFace face)
{
    auto& state = const_cast<StencilFace&>(FaceState(this, face));
    const StencilFunction next{function, reference, mask};
    if (state.function == next) return;
    VCall<void>(this, 0x1c4, face, function, reference, mask);
    state.function = next;
}

void __cdecl Display::SetStencilOperation(DOperation fail, DOperation depthFail, DOperation pass, DFace face)
{
    auto& state = const_cast<StencilFace&>(FaceState(this, face));
    const StencilOperation next{fail, depthFail, pass};
    if (state.operation == next) return;
    VCall<void>(this, 0x1c8, face, fail, depthFail, pass);
    state.operation = next;
}

void __cdecl Display::SetStencilSetup(DFunction function, int reference, unsigned long mask,
                                     DOperation fail, DOperation depthFail, DOperation pass, DFace face)
{
    auto& state = const_cast<StencilFace&>(FaceState(this, face));
    const StencilFunction nextFunction{function, reference, mask};
    const StencilOperation nextOperation{fail, depthFail, pass};
    if (state.function == nextFunction && state.operation == nextOperation) return;
    VCall<void>(this, 0x1cc, face, function, reference, mask, fail, depthFail, pass);
    state.function = nextFunction;
    state.operation = nextOperation;
}

// The retail base virtual uses the stack-this calling convention.
int __cdecl RsDisplayGetNumStencilBits(const Display* display)
{
    return *reinterpret_cast<const int*>(DisplayBytes(display) + 0x4c);
}

void __cdecl Display::SetPolygonOffset(float factor, float units)
{
    if (ReadState<float>(this, 0xc4) == factor && ReadState<float>(this, 0xc8) == units) return;
    VCall<void>(this, 0x234, factor, units);
    WriteState<float>(this, 0xc4) = factor;
    WriteState<float>(this, 0xc8) = units;
}

void __cdecl Display::EnablePolygonOffset(DPolygonOffsetMode mode, bool on)
{
    const int index = static_cast<int>(mode);
    if (index < 3) {
        if (ReadState<unsigned char>(this, 0xc0 + index) == static_cast<unsigned char>(on)) return;
        VCall<void>(this, 0x238, mode, on);
        WriteState<unsigned char>(this, 0xc0 + index) = on;
    } else if (index == 5) {
        VCall<void>(this, 0x238, mode, on);
        for (unsigned i = 0; i < 3; ++i)
            WriteState<unsigned char>(this, 0xc0 + i) = on;
    }
}

int __cdecl Display::GetMaxTextureCombinerStages() { return *reinterpret_cast<const int*>(Raw() + 0x214); }
int __cdecl Display::GetMaxTextureImageUnits() { return *reinterpret_cast<const int*>(Raw() + 0x218); }
int __cdecl Display::GetMaxTextureCoordUnits() { return *reinterpret_cast<const int*>(Raw() + 0x21c); }
bool __cdecl Display::GetAlphaToCoverage() { return Bit(State(), 0xa0, 7); }

bool __cdecl Display::SupportsAntiAliasing() const { return Bit(State(), 0xa1, 4); }
bool __cdecl Display::GetAntiAliasing() const { return Bit(State(), 0xa1, 3); }
void __cdecl Display::SetAntiAliasing(bool on)
{
    if (!SupportsAntiAliasing() || GetAntiAliasing() == on) return;
    SetBit(&WriteState<unsigned char>(this, 0xa1), 0, 3, on);
    VCall<void>(this, 0x1d4, on);
}

namespace {
template<class T>
const T& DisplayField(const Display* display, unsigned offset)
{
    return *reinterpret_cast<const T*>(DisplayBytes(display) + offset);
}

bool ReturnVideoList(const Display* display, unsigned offset, unsigned minimum, void* output)
{
    const void* list = DisplayBytes(display) + offset;
    std::memcpy(output, &list, sizeof list);
    return DisplayField<unsigned>(display, offset) >= minimum;
}

bool CopyVideoFields(const Display* display, unsigned offset, unsigned words, void* output)
{
    auto* dest = static_cast<unsigned char*>(output);
    for (unsigned i = 0; i < words; ++i) {
        unsigned word;
        std::memcpy(&word, DisplayBytes(display) + offset + i * sizeof word, sizeof word);
        std::memcpy(dest + i * sizeof word, &word, sizeof word);
    }
    return true;
}

template<class T>
struct VideoArray {
    unsigned size;
    unsigned capacity;
    T* data;
};

struct VideoFormatWords {
    unsigned fields[8];
};
}

bool __cdecl Display::TextureCombinerSupport() const { return DisplayField<bool>(this, 0x212); }
bool __cdecl Display::VertexProgramSupport() const { return DisplayField<bool>(this, 0x211); }
bool __cdecl Display::HardwareAnimationSupport() const { return DisplayField<bool>(this, 0x210); }
VertexBufferManager* __cdecl Display::GetVertexBufferManager() const { return DisplayField<VertexBufferManager*>(this, 0x20c); }
DisplaySelector* __cdecl Display::GetDisplaySelector() const { return DisplayField<DisplaySelector*>(this, 0x288); }
OS* __cdecl Display::GetOS() const { return DisplayField<OS*>(this, 0x44); }
bool __cdecl Display::IsRenderableState() const { return DisplayField<bool>(this, 0x49); }
unsigned int __cdecl Display::GetCurrentFrame() const { return DisplayField<unsigned>(this, 0x3c); }
float __cdecl Display::GetPhysicalScreenRatio() { return DisplayField<float>(this, 0x6c); }

// Stack-this adapters preserve the retail ABI without declaring a partial vtable or an opaque ABI type.
void* __cdecl RsDisplayGetCombinerManager(const Display* display) { return DisplayField<void*>(display, 0x228); }
void* __cdecl RsDisplayGetProgramManager(const Display* display) { return DisplayField<void*>(display, 0x224); }
bool __cdecl RsDisplayIsBetweenFrames(Display* display) { return DisplayField<bool>(display, 0x70); }
int __cdecl RsDisplayGetScreenMode(const Display* display) { return DisplayField<int>(display, 0x22c); }
const void* __cdecl RsDisplayGetFrameStats(const Display* display) { return DisplayBytes(display) + 0x298; }
const void* __cdecl RsDisplayGetCurrentFrameStats(const Display* display) { return DisplayBytes(display) + 0x2c4; }

bool __cdecl RsDisplayGetAllVideoModes(const Display* display, void* output)
{
    return ReturnVideoList(display, 0x230, 1, output);
}
bool __cdecl RsDisplayGetAllVideoFormats(const Display* display, void* output)
{
    return ReturnVideoList(display, 0x25c, 2, output);
}
bool __cdecl RsDisplayGetCurrentVideoMode(const Display* display, void* output)
{
    return CopyVideoFields(display, 0x23c, 4, output);
}
bool __cdecl RsDisplayGetDefaultVideoMode(const Display* display, void* output)
{
    return CopyVideoFields(display, 0x24c, 4, output);
}
bool __cdecl RsDisplayGetCurrentVideoFormat(const Display* display, void* output)
{
    return CopyVideoFields(display, 0x268, 8, output);
}

void __cdecl RsDisplayGetAntiAliasModes(const Display* display, void* output)
{
    auto& modes = *static_cast<VideoArray<unsigned>*>(output);
    modes.size = 0;
    const auto& formats = DisplayField<VideoArray<VideoFormatWords>>(display, 0x25c);
    for (unsigned i = 0; i < formats.size; ++i) {
        const unsigned samples = formats.data[i].fields[6];
        unsigned j = 0;
        while (j < modes.size && modes.data[j] != samples) ++j;
        if (j != modes.size) continue;
        if (modes.size == modes.capacity) {
            const unsigned capacity = ((modes.size + 10) / 10) * 10;
            auto* data = static_cast<unsigned*>(Alloc::New(capacity * sizeof(unsigned)));
            if (modes.data) {
                std::memcpy(data, modes.data, modes.size * sizeof(unsigned));
                Alloc::Delete(modes.data);
            }
            modes.capacity = capacity;
            modes.data = data;
        }
        modes.data[modes.size++] = samples;
    }
}

namespace {
template<unsigned N>
bool CacheViewParams(Display* display, int mode, const float (&params)[N])
{
    bool changed = ReadState<int>(display, 0x04) != mode;
    for (unsigned i = 0; i < N; ++i)
        changed |= ReadState<float>(display, 0x08 + 4 * i) != params[i];
    if (!changed) return false;
    WriteState<int>(display, 0x04) = mode;
    for (unsigned i = 0; i < N; ++i)
        WriteState<float>(display, 0x08 + 4 * i) = params[i];
    return true;
}

struct ViewParams {
    int mode;
    float parameters[8];
};
}

void __cdecl Display::Set3DTransitionalView(float a, float b, float c, float d, float e, float f, float g)
{
    const float params[]{d, e, f, g, b, c, a};
    if (CacheViewParams(this, 2, params))
        VCall<void>(this, 0x184, a, b, c, d, e, f, g);
}

void __cdecl Display::Set3DView(float width, float height, float nearDistance, float farDistance)
{
    const float params[]{width, height, nearDistance, farDistance};
    if (CacheViewParams(this, 3, params))
        VCall<void>(this, 0x188, width, height, nearDistance, farDistance);
}

void __cdecl Display::Set3DViewOffset(float width, float height, float nearDistance, float farDistance, float offset)
{
    const float params[]{width, height, nearDistance, farDistance, offset};
    if (CacheViewParams(this, 4, params))
        VCall<void>(this, 0x188, width, height, nearDistance + offset, farDistance);
}

void __cdecl RsDisplayGetViewModeParams(const Display* display, void* out)
{
    auto& params = *static_cast<ViewParams*>(out);
    params.mode = ReadState<int>(display, 0x04);
    for (unsigned i = 0; i < 8; ++i)
        params.parameters[i] = ReadState<float>(display, 0x08 + 4 * i);
}

void __cdecl RsDisplaySetViewModeParams(Display* display, const void* in)
{
    const auto& params = *static_cast<const ViewParams*>(in);
    if (!CacheViewParams(display, params.mode, params.parameters)) return;
    const auto& p = params.parameters;
    switch (params.mode) {
    case 0:
    case 1: VCall<void>(display, 0x180, p[0], p[1], p[2], p[3], p[4], p[5]); break;
    case 2: VCall<void>(display, 0x184, p[6], p[4], p[5], p[0], p[1], p[2], p[3]); break;
    case 3: VCall<void>(display, 0x188, p[0], p[1], p[2], p[3]); break;
    case 4: VCall<void>(display, 0x188, p[0], p[1], p[2] + p[4], p[3]); break;
    }
}

void __cdecl RsDisplayLoadProjectionMatrix(Display* display, const Matrix4& matrix)
{
    WriteState<int>(display, 0x04) = 5;
    VCall<void>(display, 0x19c, &matrix);
}

bool __cdecl Display::IsNormalized2DViewport() const
{
    return ReadState<int>(this, 0x04) == 0 &&
           std::fabs(static_cast<double>(ReadState<float>(this, 0x0c)) - ReadState<float>(this, 0x08)) < 1.01f;
}

void __cdecl Display::SetTextureCoordGen(int stage, DTexGenMode mode, unsigned coordinates)
{
    if (ReadState<DTexGenMode>(this, 0xb8) == mode && ReadState<unsigned>(this, 0xbc) == coordinates) return;
    WriteState<DTexGenMode>(this, 0xb8) = mode;
    WriteState<unsigned>(this, 0xbc) = coordinates;
    VCall<void>(this, 0x240, stage, mode, coordinates);
}

bool __cdecl RsDisplayGetTextureMode(const Display* display) { return ReadState<bool>(display, 0x65); }
bool __cdecl RsDisplayGetGlobalTextureMode(const Display* display) { return DisplayField<bool>(display, 0x4a); }

void __cdecl RsDisplaySetTextureMode(Display* display, bool on)
{
    if (RsDisplayGetTextureMode(display) == on) return;
    WriteState<bool>(display, 0x65) = on;
    if (RsDisplayGetGlobalTextureMode(display))
        VCall<void>(display, 0x214, on);
}

void __cdecl RsDisplaySetGlobalTextureMode(Display* display, bool on)
{
    if (RsDisplayGetGlobalTextureMode(display) == on) return;
    DisplayBytes(display)[0x4a] = on;
    VCall<void>(display, 0x214, on && RsDisplayGetTextureMode(display));
}

void __cdecl Display::SetRenderTarget(DisplayTexture* texture, unsigned index)
{
    *reinterpret_cast<DisplayTexture**>(DisplayBytes(this) + 0x5c) = texture;
    VCall<void>(this, 0x1d0, texture, index);
}

void __cdecl RsDisplayEnableProjectedTexture(Display* display, int stage, bool on, bool useAlpha)
{
    VCall<void>(display, 0x218, stage, on, useAlpha);
    WriteState<bool>(display, 0x66) = on;
}

// The retail base class leaves active-stage selection to renderer subclasses.
void __cdecl RsDisplaySetActiveTextureStage(Display*, int) {}

namespace {
struct ContextStorage {
    FARPROC vtable;
    unsigned references;
};

FARPROC ContextVtable(const char* name)
{
    // Keep the retail table whenever the original module is loaded: these base contexts cross the DLL
    // boundary, and the retail owners (renderer plugins, trainz.exe) call slots in it directly. The
    // local table remains the stand-alone fallback.
    if (HMODULE const original = GetModuleHandleA("RenderSystem_7c_orig.dll")) {
        if (FARPROC const table = GetProcAddress(original, name)) return table;
    }
    HMODULE const self = GetModuleHandleA("RenderSystem_7c.dll");
    return self ? GetProcAddress(self, name) : nullptr;
}

constexpr auto sharedContextVtable = "??_7SharedContext@Jet@@6B@";
constexpr auto nonRenderContextVtable = "??_7NonRenderContext@Jet@@6B@";
constexpr auto displayContextVtable = "??_7DisplayContext@Display@Jet@@6B@";
constexpr auto saveBufferVtable = "??_7DisplaySaveBuffer@Jet@@6B@";
constexpr auto selectorVtable = "??_7DisplaySelector@Jet@@6B@";
constexpr auto elementLoaderVtable = "??_7ElementLoader@Jet@@6B@";
constexpr unsigned displayContextOwnedSlot = 0x110;

void* ConstructBaseContext(void* storage, const char* vtableExport)
{
    return new (storage) ContextStorage{ContextVtable(vtableExport), 1};
}

void DestroyBaseContext(void* storage, const char* vtableExport)
{
    static_cast<ContextStorage*>(storage)->vtable = ContextVtable(vtableExport);
    using Destructor = void(__thiscall*)(void*);
    const auto base = reinterpret_cast<Destructor>(GetProcAddress(
        GetModuleHandleA("KernelStandard_7c.dll"), "??1ReferenceCount@Jet@@MAE@XZ"));
    base(storage);
}

void* CreateBaseContext(const char* vtableExport)
{
    auto* storage = Alloc::New(sizeof(ContextStorage));
    return storage ? ConstructBaseContext(storage, vtableExport) : nullptr;
}
}  // namespace

extern "C" void* __fastcall RsDisplayContextCtor(unsigned char* state, void*)
{
    const auto table = ContextVtable(displayContextVtable);
    std::memcpy(state, &table, sizeof table);
    // Initialize only the fields written by the original constructor.
    std::memset(state + 0x04, 0, 0x60);
    std::memset(state + 0x64, 0, 3);
    std::memset(state + 0x68, 0, 0x0e);
    std::memset(state + 0x78, 0, 0x28);
    std::memset(state + 0xa4, 0, 0x1c);
    std::memset(state + 0xc0, 0, 3);
    std::memset(state + 0xc4, 0, 0x94);
    auto put = [state](unsigned offset, unsigned value) {
        std::memcpy(state + offset, &value, sizeof value);
    };
    for (auto offset : {0x3c, 0x8c, 0x94, 0xa4, 0xa8, 0xac, 0xb0, 0x120, 0x130, 0x140, 0x150})
        put(offset, 0x3f800000);
    for (auto offset : {0xb4, 0xe4, 0x100}) put(offset, 0xffffffff);
    for (auto offset : {0xd8, 0xf4}) put(offset, 7);
    put(0x04, 5);
    put(0x28, 1);
    put(0x68, 1);
    put(0x9c, 0xf);
    put(0xbc, 2);
    put(0xd4, 2);
    state[0x74] = 1;
    state[0xa0] = 6;
    state[0xa1] &= 0xe0;
    return state;
}

extern "C" void __fastcall RsDisplayContextDtor(unsigned char* state, void*)
{
    const auto table = ContextVtable(displayContextVtable);
    std::memcpy(state, &table, sizeof table);
    void* buffer;
    std::memcpy(&buffer, state + displayContextOwnedSlot, sizeof buffer);
    if (buffer) Alloc::Delete(buffer);
    buffer = nullptr;
    std::memcpy(state + displayContextOwnedSlot, &buffer, sizeof buffer);
}

extern "C" void* __fastcall RsDisplayContextAssign(unsigned char* state, void*, const unsigned char* source)
{
    if (state == source) return state;
    std::memcpy(state + 0x04, source + 0x04, 0x63);
    std::memcpy(state + 0x68, source + 0x68, 0x0e);
    std::memcpy(state + 0x78, source + 0x78, 0x29);
    state[0xa1] = (state[0xa1] & 0xe0) | (source[0xa1] & 0x1f);
    std::memcpy(state + 0xa4, source + 0xa4, 0x1f);
    // The original copies the buffer pointer verbatim, not the allocation.
    std::memcpy(state + 0xc4, source + 0xc4, 0x94);
    return state;
}

extern "C" void* __fastcall RsDisplayContextCopy(unsigned char* state, void*, const unsigned char* source)
{
    const auto table = ContextVtable(displayContextVtable);
    std::memcpy(state, &table, sizeof table);
    return RsDisplayContextAssign(state, nullptr, source);
}

extern "C" void* __fastcall RsVideoModeInit(unsigned* out, void*)
{
    std::fill_n(out, 4, 0u);
    return out;
}

extern "C" void* __fastcall RsVideoModeCreate(unsigned* out, void*, unsigned a, unsigned b, unsigned c, unsigned d)
{
    out[0] = a; out[1] = b; out[2] = c; out[3] = d;
    return out;
}

extern "C" void* __fastcall RsVideoFormatInit(unsigned* out, void*)
{
    std::fill_n(out, 8, 0u);
    return out;
}

extern "C" void* __fastcall RsVideoFormatCreate(unsigned* out, void*, unsigned a, unsigned b, unsigned c,
    unsigned d, unsigned e, unsigned f, unsigned g, unsigned h)
{
    out[0] = a; out[1] = b; out[2] = c; out[3] = e;
    out[4] = f; out[5] = d; out[6] = g; out[7] = h;
    return out;
}

bool __cdecl RsVideoModeEq(const unsigned* a, const unsigned* b) { return std::equal(a, a + 4, b); }
bool __cdecl RsVideoModeNe(const unsigned* a, const unsigned* b) { return !RsVideoModeEq(a, b); }
bool __cdecl RsVideoModeLt(const unsigned* a, const unsigned* b)
{
    return std::lexicographical_compare(a, a + 4, b, b + 4);
}
unsigned* __cdecl RsVideoModeAssign(unsigned* a, const unsigned* b)
{
    if (a != b) std::copy_n(b, 4, a);
    return a;
}
bool __cdecl RsVideoFormatEq(const unsigned* a, const unsigned* b) { return std::equal(a, a + 8, b); }
bool __cdecl RsVideoFormatNe(const unsigned* a, const unsigned* b) { return !RsVideoFormatEq(a, b); }
bool __cdecl RsVideoFormatComparePixel(const unsigned* a, const unsigned* b)
{
    return std::equal(a + 2, a + 6, b + 2) && a[7] == b[7];
}
unsigned* __cdecl RsVideoFormatAssign(unsigned* a, const unsigned* b)
{
    if (a != b) std::copy_n(b, 8, a);
    return a;
}

// Keep retail vtables for objects that cross the DLL boundary.
void* __cdecl RsDisplayCreateSharedContext(const Display*)
{
    return CreateBaseContext(sharedContextVtable);
}

void* __cdecl RsDisplayCreateNonRenderContext(const Display*)
{
    return CreateBaseContext(nonRenderContextVtable);
}

extern "C" void* __fastcall RsSharedContextCtor(void* storage, void*)
{
    return ConstructBaseContext(storage, sharedContextVtable);
}

extern "C" void* __fastcall RsNonRenderContextCtor(void* storage, void*)
{
    return ConstructBaseContext(storage, nonRenderContextVtable);
}

extern "C" void __fastcall RsSharedContextDtor(void* storage, void*)
{
    DestroyBaseContext(storage, sharedContextVtable);
}

extern "C" void __fastcall RsNonRenderContextDtor(void* storage, void*)
{
    DestroyBaseContext(storage, nonRenderContextVtable);
}

extern "C" void* __fastcall RsSaveBufferCtor(void* storage, void*, unsigned type)
{
    ConstructBaseContext(storage, saveBufferVtable);
    *(reinterpret_cast<unsigned*>(storage) + 2) = type;
    return storage;
}
extern "C" void __fastcall RsSaveBufferDtor(void* storage, void*) { DestroyBaseContext(storage, saveBufferVtable); }
extern "C" void* __fastcall RsSelectorCtor(void* storage, void*) { return ConstructBaseContext(storage, selectorVtable); }
extern "C" void __fastcall RsSelectorDtor(void* storage, void*) { DestroyBaseContext(storage, selectorVtable); }

extern "C" void* __fastcall RsElementLoaderCtor(FARPROC* storage, void*)
{
    *storage = ContextVtable(elementLoaderVtable);
    return storage;
}
extern "C" void* __fastcall RsElementLoaderCopy(FARPROC* storage, void*, const void*)
{
    return RsElementLoaderCtor(storage, nullptr);
}
extern "C" void __fastcall RsElementLoaderDtor(FARPROC* storage, void*) { *storage = ContextVtable(elementLoaderVtable); }
extern "C" void* __fastcall RsElementLoaderAssign(void* storage, void*, const void*) { return storage; }

unsigned __cdecl RsSaveBufferType(const unsigned* buffer) { return buffer[2]; }
bool __cdecl RsNonRenderThread(void*) { return true; }
bool __cdecl RsNonRenderInit(void*, Display*) { return true; }

Window* __cdecl RsDisplayGetWindow(const Display* display) { return DisplayField<Window*>(display, 0x40); }
bool __cdecl RsDisplayCheckPlatform(Display*, OS*) { return false; }
bool __cdecl RsDisplayGetZBias(Display* display) { return DisplayField<bool>(display, 0x68); }
void __cdecl RsDisplaySetZBias(Display* display, float scale, float bias)
{
    auto* values = reinterpret_cast<float*>(DisplayBytes(display) + 0x60);
    values[0] = scale;
    values[1] = bias;
}
void __cdecl RsDisplayIgnoreToggle(Display*, bool) {}
void __cdecl RsDisplayIgnoreFloat(Display*, float) {}
void __cdecl RsDisplayIgnoreResources(Display*) {}
void __cdecl RsDisplayIgnoreVertex(const Display*, void*, const void*, const char*) {}
void* __cdecl RsDisplayNullFactory(Display*) { return nullptr; }

void __cdecl RsDisplayBeginFrame(Display* display, float)
{
    ++*reinterpret_cast<unsigned*>(DisplayBytes(display) + 0x3c);
    DisplayBytes(display)[0x70] = 1;
}
void __cdecl RsDisplayEndFrame(Display* display, float) { DisplayBytes(display)[0x70] = 0; }
extern "C" void __fastcall RsDisplayFlushState(Display* display, void*)
{
    if (const auto* manager = DisplayField<void*>(display, 0x20c))
        VCall<void>(manager, 0x10);
}
bool __cdecl RsDisplayUpdate(Display* display, float)
{
    auto* window = RsDisplayGetWindow(display);
    const bool alive = *reinterpret_cast<const void* const*>(
        reinterpret_cast<const unsigned char*>(window) + 0x118) || window->HandleMessages();
    if (const auto* manager = DisplayField<void*>(display, 0x20c))
        VCall<void>(manager, 0x48);
    return alive;
}

bool __cdecl RsDisplayGetVertexWeightMode(const Display* display) { return Bit(display->State(), 0xa1, 1); }
void __cdecl RsDisplaySetVertexWeightMode(Display* display, bool on)
{
    if (RsDisplayGetVertexWeightMode(display) == on) return;
    VCall<void>(display, 0x210);
    SetBit(&WriteState<unsigned char>(display, 0xa1), 0, 1, on);
}
void __cdecl RsDisplayResetDynamicBuffers(Display* display)
{
    VCall<void>(DisplayField<void*>(display, 0x20c), 0x40);
}
void __cdecl RsDisplayCheckDynamicBuffers(Display* display)
{
    if (DisplayBytes(display)[0x213] == 0) RsDisplayResetDynamicBuffers(display);
}
void* __cdecl RsDisplayAllocStaticVertex(Display* display, unsigned size, unsigned flags)
{
    return VCall<void*>(DisplayField<void*>(display, 0x20c), 0x14, size, flags);
}
void* __cdecl RsDisplayAllocStaticIndex(Display* display, unsigned size, bool wide)
{
    return VCall<void*>(DisplayField<void*>(display, 0x20c), 0x18, size, wide);
}
void __cdecl RsDisplayDrawDynamicArray(Display* display, int format, int primitive, int flags, const void* data, int count)
{
    // Cache the table itself, not just the declaration callback, across both calls.
    const auto* table = *reinterpret_cast<const FARPROC* const*>(display);
    const auto declaration = reinterpret_cast<void*(__cdecl*)(Display*, int)>(table[0x10c / sizeof(void*)]);
    void* const vertexType = declaration(display, format);
    const auto draw = reinterpret_cast<void(__cdecl*)(Display*, void*, int, int, const void*, int)>(
        table[0xe4 / sizeof(void*)]);
    draw(display, vertexType, primitive, flags, data, count);
}
VertexType* __cdecl RsDisplayGetBuiltInVertex(const Display* display, int id)
{
    return id < 99 ? reinterpret_cast<VertexType* const*>(DisplayBytes(display) + 0x80)[id] : nullptr;
}
VertexType* __cdecl RsDisplayCloneBuiltInVertex(const Display* display, int id)
{
    if (id >= 99) return nullptr;
    return new VertexType(*RsDisplayGetBuiltInVertex(display, id));
}
int __cdecl RsDisplayGetTargetPPI(const Display* display)
{
    int ppi;
    const auto* options = DisplayField<OS*>(display, 0x44)->GetOptions();
    return options->Get("screenppi", ppi, -1) ? ppi : -1;
}

Bitmap* __cdecl RsDisplayScreenshot(const Display* display)
{
    if (DisplayField<void*>(display, 0x5c)) return nullptr;
    const Size size = display->GetWindowSize();
    std::unique_ptr<void, decltype(&Alloc::Delete)> memory(Alloc::New(sizeof(Bitmap)), &Alloc::Delete);
    if (!memory) return nullptr;
    auto* bitmap = ::new (memory.get()) Bitmap(size, Bitmap::ARGB, nullptr);
    const auto destroy = [](Bitmap* p) { p->~Bitmap(); };
    const std::unique_ptr<Bitmap, decltype(destroy)> capture(bitmap, destroy);
    const Rect area{0, 0, size.w, size.h};
    return VCall<bool>(display, 0xd4, &area, bitmap->GetPtr()) ? bitmap->FlipY() : nullptr;
}
void __cdecl RsDisplaySetSelector(Display* display, void* selector)
{
    auto& previous = *reinterpret_cast<ReferenceCount**>(DisplayBytes(display) + 0x288);
    if (previous) {
        if (previous == selector) return;
        previous->ReleaseReference();
        previous = nullptr;
    }
    if (!selector) {
        previous = static_cast<ReferenceCount*>(CreateBaseContext(selectorVtable));
        return;
    }
    previous = static_cast<ReferenceCount*>(selector);
    previous->AddReference();
}

bool __cdecl RsDisplayInitializeState(Display* display, unsigned char* context,
                                      bool initializeDevice, bool resetContext, const Options* options)
{
    auto* const device = reinterpret_cast<unsigned char*>(display);
    const auto word = [context](unsigned offset) -> unsigned& {
        return *reinterpret_cast<unsigned*>(context + offset);
    };
    const auto lights = [device] { return *reinterpret_cast<int*>(device + 0x50); };
    auto& lightFlags = *reinterpret_cast<unsigned char**>(context + 0x110);
    auto* const color = reinterpret_cast<Color*>(context + 0xa4);
    if (initializeDevice) {
        for (int id = 0; id < 99; ++id) VCall<void>(display, 0x220, id);
        VCall<void>(display, 0x23c, options);
        if (device[0x211]) *reinterpret_cast<VertexProgramManager**>(device + 0x224) = new VertexProgramManager(display);
        if (device[0x212]) *reinterpret_cast<TextureCombinerManager**>(device + 0x228) = new TextureCombinerManager(display);
        if (options->Get("SwapAtStart")) device[0x294] = 1;
        else if (options->Get("SwapAtEnd")) device[0x294] = 0;
    }
    if (initializeDevice || resetContext) {
        lightFlags = static_cast<unsigned char*>(Alloc::New(lights()));
        for (int i = 0; i < lights(); ++i) lightFlags[i] = 0;
        context[0xa1] &= 0xfa;
        context[0xa0] = (context[0xa0] & 0x9f) | 4;
        context[0x64] = context[0x65] = context[0x74] = 1;
        context[0x75] = context[0xc0] = context[0xc1] = context[0xc2] = 0;
        for (unsigned offset : {0x70, 0xcc, 0x6c, 0x40, 0x2c, 0x30, 0x34, 0x38, 0xb8, 0xc4, 0xc8,
                                0xdc, 0xe0, 0xe8, 0xec, 0xf0, 0xf8, 0xfc, 0x104, 0x108, 0x10c})
            word(offset) = 0;
        word(0x9c) = 0xf;
        word(0xd0) = word(0xd8) = word(0xf4) = 7;
        word(0x28) = word(0x68) = 1;
        word(0xd4) = word(0xbc) = 2;
        word(0x3c) = word(0xa4) = word(0xa8) = word(0xac) = word(0xb0) = 0x3f800000;
        word(0xb4) = Display::ConvertColor(*color);
        word(0xe4) = word(0x100) = ~0u;
        if (context[0xa1] & 0x10) context[0xa1] |= 8;
        word(4) = 5;
    }

    const auto flag = [context](unsigned offset, unsigned bit) {
        return (context[offset] >> bit) & 1;
    };
    VCall<void>(display, 0x1ac, 1, word(0x9c));
    if (VCall<bool>(display, 0x1c, 0x20)) VCall<void>(display, 0x1e0, word(0x70));
    VCall<void>(display, 0x1b4, flag(0xa0, 2));
    VCall<void>(display, 0x1b0, word(0xd4));
    VCall<void>(display, 0x198, context[0x64]);
    VCall<void>(display, 0x1d8, word(0x28));
    VCall<void>(display, 0x1e4, flag(0xa0, 5));
    VCall<void>(display, 0x1e8, word(0xd0), word(0xcc));
    VCall<void>(display, 0x1ec, flag(0xa1, 0));
    VCall<void>(display, 0x1dc, word(0x68), word(0x6c));
    VCall<void>(display, 0x1f0, flag(0xa1, 2));
    VCall<void>(display, 0x1f4, context[0x74]);
    VCall<void>(display, 0x1f8, context[0x75]);
    VCall<void>(display, 0x214, context[0x65]);
    VCall<void>(display, 0x1bc, flag(0xa0, 3), flag(0xa0, 4));
    VCall<void>(display, 0x1a8, word(0x40));
    VCall<void>(display, 0x1a0, context + 0x2c);
    VCall<void>(display, 0x1a4, word(0x3c));
    if (VCall<bool>(display, 0x1c, 0x10)) VCall<void>(display, 0x1b8, flag(0xa0, 6));
    VCall<void>(display, 0x240, 0, word(0xb8), word(0xbc));
    VCall<void>(display, 0x230, color);
    VCall<void>(display, 0x234, word(0xc4), word(0xc8));
    for (unsigned i = 0; i < 3; ++i) VCall<void>(display, 0x238, i, context[0xc0 + i]);

    switch (word(4)) {
    case 0: case 1:
        VCall<void>(display, 0x180, word(8), word(0xc), word(0x10), word(0x14), word(0x18), word(0x1c));
        break;
    case 3:
        VCall<void>(display, 0x188, word(8), word(0xc), word(0x10), word(0x14));
        break;
    case 4: {
        const float farPlane = *reinterpret_cast<float*>(context + 0x18) + *reinterpret_cast<float*>(context + 0x10);
        VCall<void>(display, 0x188, word(8), word(0xc), farPlane, word(0x14));
        break;
    }
    }
    if (word(0x44) || word(0x48) || word(0x4c) || word(0x50)) {
        VCall<void>(display, 0x190, context + 0x44);
        VCall<void>(display, 0x194, context + 0x54);
    }
    context[0x66] = 0;
    VCall<void>(display, 0x1fc, word(0x78));
    VCall<void>(display, 0x208, context + 0x7c);
    VCall<void>(display, 0x200, word(0x8c));
    VCall<void>(display, 0x204, word(0x90), word(0x94));
    VCall<void>(display, 0x20c, word(0x98));
    VCall<void>(display, 0x18c, flag(0xa0, 0));
    VCall<void>(display, 0x1ac, 2, flag(0xa0, 1));
    VCall<void>(display, 0x230, color);
    word(0xb4) = Display::ConvertColor(*color);
    const auto stencil = [&](unsigned side, unsigned offset) {
        VCall<void>(display, 0x1c0, side, word(offset + 0xc));
        VCall<void>(display, 0x1cc, side, word(offset), word(offset + 4), word(offset + 8),
                    word(offset + 0x10), word(offset + 0x14), word(offset + 0x18));
    };
    stencil(0, 0xd8);
    if (VCall<bool>(display, 0x1c, 0xc)) stencil(1, 0xf4);
    for (int i = 0; i < lights(); ++i) VCall<void>(display, 0x224, i, lightFlags[i]);
    VCall<void>(display, 0x21c, context + 0x114);
    VCall<void>(display, 0x1d4, flag(0xa1, 3));
    return true;
}

namespace {
constexpr const char* extensions[] = {
    "D_MULTI_TEXTURE", "D_MULTI_TEX_COMBINE", "D_MULTI_TEX_COMBINE4", "D_MULTI_TEX_MULTIPLY_ADD",
    "D_MULTI_TEX_FULL_SCALE", "D_AUTOMIP", "D_ALPHA_TO_COVERAGE", "D_ALPHA_TO_COVERAGE_ATI",
    "D_RENDER_TO_TEXTURE", "D_DEPTH_TEXTURE", "D_STENCIL_BUFFER", "D_DEPTH_STENCIL_BUFFER",
    "D_STENCIL_TWO_SIDE", "D_STENCIL_WRAP", "D_TEXGEN_OBJECT_SPACE", "D_BUFFER_SAVES",
    "D_DEPTH_CLAMP", "D_VERTEX_PROGRAM", "D_VERTEX_PROGRAM_1", "D_VERTEX_PROGRAM_2",
    "D_VERTEX_PROGRAM_3", "D_PIXEL_SHADER_1_1", "D_PIXEL_SHADER_1_3", "D_PIXEL_SHADER_1_4",
    "D_PIXEL_SHADER_2_0", "D_FRAGMENT_PROGRAM_1", "D_SPHERE_MAPPING", "D_CUBE_MAPPING",
    "D_DOT3_BUMPMAPPING", "D_HW_VERTEX_PROCESSING", "D_VERTEX_BUFFERS", "D_CONCURRENT_CONTEXTS",
    "D_BLEND_EQUATION",
};
}
const char* __cdecl RsDisplayGetExtension(const Display*, int id) { return extensions[id]; }
bool __cdecl RsDisplayCheckExtension(const Display* display, const PString& name)
{
    for (unsigned i = 0; i < std::size(extensions); ++i)
        if (name.Compare(extensions[i]) == 0) return VCall<bool>(display, 0x1c, i);
    return false;
}

namespace {
// Each token is a stable pointer key from this module, not a text lookup.
constexpr const char* compressionTokens[] = {
    "TC_S3TC", "TC_DXT1", "TC_ETC1_RGB8", "TC_PAL_TEX", "TC_AMD_3DC",
    "TC_AMD_ATC", "TC_LATC", "TC_PVRTC", "TC_ETC1",
};
}
const char* __cdecl RsDisplayCompressionName(const Display*, unsigned id)
{
    return id < std::size(compressionTokens) ? compressionTokens[id] : nullptr;
}
int __cdecl RsDisplayCompressionId(const Display*, const char* name)
{
    for (unsigned i = 0; i < std::size(compressionTokens); ++i)
        if (compressionTokens[i] == name) return static_cast<int>(i);
    return -1;
}

namespace {
// VC80's _CIpow uses XMM registers; keep this assembly out of the caller's loop.
__declspec(noinline) unsigned short GammaSample(int sample, float power)
{
    constexpr double step = 1.0 / 256.0, limit = 65535.0, half = 0.5;
    const auto fallback = [=] {
        const auto rounded = std::pow(sample * step, power) * limit + half;
        return static_cast<unsigned short>(!(rounded > 0) ? 0 : rounded >= limit ? 65535 : rounded);
    };
#if defined(_M_IX86)
    static const auto ciPow = FindLegacyCiPow();
    if (!ciPow) return fallback();
    unsigned short control, truncate;
    int converted;
    __asm {
        fild dword ptr [sample]
        fmul qword ptr [step]
        fld dword ptr [power]
        call dword ptr [ciPow]
        fmul qword ptr [limit]
        fadd qword ptr [half]
        fldz
        fxch st(1)
        fcomi st, st(1)
        ja positive
        fstp st(0)
        jmp convert
    positive:
        fstp st(1)
        fld qword ptr [limit]
        fcomi st, st(1)
        jb saturated
        fstp st(0)
        jmp convert
    saturated:
        fstp st(1)
    convert:
        fnstcw control
        mov ax, control
        or ax, 0c00h
        mov truncate, ax
        fldcw truncate
        fistp dword ptr [converted]
        fldcw control
    }
    return static_cast<unsigned short>(converted);
#else
    return fallback();
#endif
}
}

void __cdecl RsDisplaySetGamma(Display* display, float gamma, bool force)
{
    auto* bytes = DisplayBytes(display);
    auto& supported = *reinterpret_cast<int*>(bytes + 0x2f0);
    auto& previous = *reinterpret_cast<float*>(bytes + 0x2f4);
    if (supported == 0)
        supported = GetDeviceGammaRamp(GetDC(nullptr), bytes + 0x2f8) ? 1 : -1;
    if (supported < 0) return;
    if (gamma < 0.5f) gamma = 0.5f;
    else if (gamma > 2.0f) gamma = 2.0f;
    if (!force && gamma == previous) return;

    unsigned short ramp[3][256];
    const float power = 1.0f / gamma;
    for (int i = 0; i < 256; ++i)
        ramp[0][i] = ramp[1][i] = ramp[2][i] = GammaSample(i + 1, power);
    SetDeviceGammaRamp(GetDC(nullptr), ramp);
    previous = gamma;
}

void __cdecl RsDisplayRestoreGamma(Display* display)
{
    auto* bytes = DisplayBytes(display);
    if (*reinterpret_cast<const int*>(bytes + 0x2f0) > 0) {
        SetDeviceGammaRamp(GetDC(nullptr), bytes + 0x2f8);
        *reinterpret_cast<float*>(bytes + 0x2f4) = 1.0f;
    }
}

void __cdecl RsDisplayCriticalStartup(Display* display)
{
    const auto gamma = DisplayField<float>(display, 0x2f4);
    RsDisplaySetGamma(display, gamma, true);
}
void __cdecl RsDisplayCriticalShutdown(Display* display)
{
    RsDisplayRestoreGamma(display);
    if (DisplayField<int>(display, 0x22c) == 2) {
        if (auto* window = RsDisplayGetWindow(display)) {
            window->SetCapture(false);
            VCall<bool>(display, 0x08, Rect{0, 0, 640, 480});
        }
    }
}
int __cdecl RsDisplayCriticalStartupCallback(void* display)
{
    RsDisplayCriticalStartup(static_cast<Display*>(display));
    return 0;
}
int __cdecl RsDisplayCriticalShutdownCallback(void* display)
{
    RsDisplayCriticalShutdown(static_cast<Display*>(display));
    return 0;
}

void __cdecl RsDisplayShutdown(Display* display)
{
    RsDisplayRestoreGamma(display);
    auto* bytes = DisplayBytes(display);
    auto* vertices = reinterpret_cast<VertexType* const*>(bytes + 0x80);
    for (unsigned i = 0; i < 99; ++i)
        if (vertices[i]) vertices[i]->ReleaseReference();
    VertexType::Shutdown();

    using Delete = void*(__thiscall*)(void*, unsigned);
    for (unsigned offset : {0x20c, 0x224, 0x228}) {
        auto& manager = *reinterpret_cast<void**>(bytes + offset);
        if (manager)
            (*reinterpret_cast<Delete* const*>(manager))[offset == 0x20c ? 0 : 1](manager, 1);
        manager = nullptr;
    }
}

// Display is a view of a retail object in this header; don't add a partial C++ vtable.
// This adapter is exported under the original virtual method's stack-this signature.
bool __cdecl RsDisplayInit(Display* display, OS* os)
{
    auto* bytes = DisplayBytes(display);
    if (bytes[0x48] != 0) return true;

    *reinterpret_cast<OS**>(bytes + 0x44) = os;
    Window* const window = os->CreateNewWindow();
    *reinterpret_cast<Window**>(bytes + 0x40) = window;
    os->SetWindow(window);
    bytes[0x48] = 1;

    *reinterpret_cast<void**>(bytes + 0xef8) = ExceptionHandler::AddCriticalShutdown(RsDisplayCriticalShutdownCallback, display);
    *reinterpret_cast<void**>(bytes + 0xefc) = ExceptionHandler::AddCriticalStartup(RsDisplayCriticalStartupCallback, display);
    return true;
}

void __cdecl RsDisplaySetMaterialProperties(Display* display, const DisplayMaterial& material)
{
    if (!(WriteState<DisplayMaterial>(display, 0x114) != material)) return;
    VCall<void>(display, 0x21c, &material);
    new (&WriteState<DisplayMaterial>(display, 0x114)) DisplayMaterial(material);
}

}  // namespace Jet
