// Simple/evidenced Display exports batch (RenderSystem_7c).
// Behaviour/offsets follow TS12 decomp/exports; decomp this/stack artifacts mapped to real signatures.
#include <jet/DynamicArray.h>
#include <jet/RenderSystem.h>
#include <jet/VCall.h>
#include <cstdint>

namespace Jet {

namespace {
inline char* D(Display* d) { return reinterpret_cast<char*>(d); }
inline const char* D(const Display* d) { return reinterpret_cast<const char*>(d); }
inline unsigned char* St(Display* d) { return *reinterpret_cast<unsigned char**>(D(d) + 0x54); }
inline const unsigned char* St(const Display* d) { return *reinterpret_cast<const unsigned char* const*>(D(d) + 0x54); }
}  // namespace

void Display::BeginFrame(float /*dt*/)
{
    // 0x1001ca20
    ++*reinterpret_cast<int*>(D(this) + 0x3c);
    D(this)[0x70] = 1;
}

void Display::EndFrame(float /*dt*/)
{
    // 0x1001ca30
    D(this)[0x70] = 0;
}

void Display::FlushState()
{
    // 0x1001ca40
    void* const vbm = *reinterpret_cast<void**>(D(this) + 0x20c);
    if (vbm) VCall<void>(vbm, 0x10);
}

void Display::SetZBiasValues(float zBias, float slope)
{
    // 0x10002020
    *reinterpret_cast<float*>(D(this) + 0x60) = zBias;
    *reinterpret_cast<float*>(D(this) + 0x64) = slope;
}

void Display::DisableAllLights()
{
    // 0x10003d50
    const int count = *reinterpret_cast<int*>(D(this) + 0x50);
    unsigned char* const enabled = *reinterpret_cast<unsigned char**>(St(this) + 0x110);
    for (int i = 0; i < count; ++i) {
        if (enabled[i]) {
            VCall<void, int>(this, 0x224, i);
            *reinterpret_cast<unsigned char**>(St(this) + 0x110)[i] = 0;
        }
    }
}

void Display::SetPolygonOffset(float factor, float units)
{
    // 0x10004400
    unsigned char* st = St(this);
    if (*reinterpret_cast<float*>(st + 0xc4) != factor || *reinterpret_cast<float*>(st + 0xc8) != units) {
        VCall<void, float, float>(this, 0x234, factor, units);
        st = St(this);
        *reinterpret_cast<float*>(st + 0xc4) = factor;
        *reinterpret_cast<float*>(st + 0xc8) = units;
    }
}

void Display::EnablePolygonOffset(DPolygonOffsetMode mode, bool enable)
{
    // 0x10004480
    const int m = static_cast<int>(mode);
    if (m >= 0 && m < 3) {
        unsigned char* st = St(this);
        if (st[0xc0 + m] != static_cast<unsigned char>(enable)) {
            VCall<void, int, bool>(this, 0x238, m, enable);
            St(this)[0xc0 + m] = enable ? 1 : 0;
        }
        return;
    }
    if (m == 5) {
        VCall<void, int, bool>(this, 0x238, m, enable);
        unsigned char* const st = St(this);
        const unsigned char v = enable ? 1 : 0;
        st[0xc0] = v;
        st[0xc1] = v;
        st[0xc2] = v;
    }
}

void Display::ResetDynamicBufferOffsets()
{
    // 0x1001b820
    void* const vbm = *reinterpret_cast<void**>(D(this) + 0x20c);
    VCall<void>(vbm, 0x40);
}

void Display::CheckResetDynamicBufferOffsets()
{
    // 0x10003f60
    if (D(this)[0x213] == 0) {
        void* const vbm = *reinterpret_cast<void**>(D(this) + 0x20c);
        VCall<void>(vbm, 0x40);
    }
}

bool Display::IsNormalized2DViewport() const
{
    // 0x10004580
    const unsigned char* const st = St(this);
    if (*reinterpret_cast<const int*>(st + 4) != 0) return false;
    const float a = *reinterpret_cast<const float*>(st + 8);
    const float b = *reinterpret_cast<const float*>(st + 0xc);
    const float d = a > b ? a - b : b - a;
    return d < 1.01f;
}

void Display::GetFogRange(float& start, float& end) const
{
    // 0x10003b00
    const unsigned char* const st = St(this);
    start = *reinterpret_cast<const float*>(st + 0x90);
    end = *reinterpret_cast<const float*>(st + 0x94);
}

bool Display::GetLightMode(int index) const
{
    // 0x10003cc0
    if (index < 0) return false;
    const int count = *reinterpret_cast<const int*>(D(this) + 0x50);
    if (index >= count) return false;
    const unsigned char* const enabled = *reinterpret_cast<const unsigned char* const*>(St(this) + 0x110);
    return enabled[index] != 0;
}

Rect Display::GetScissor()
{
    // 0x10002930
    return *reinterpret_cast<Rect*>(St(this) + 0x54);
}

void Display::SetScissor(const Rect& rect)
{
    // 0x100028d0
    unsigned char* st = St(this);
    Rect& cur = *reinterpret_cast<Rect*>(st + 0x54);
    if (cur.x != rect.x || cur.y != rect.y || cur.w != rect.w || cur.h != rect.h) {
        VCall<void>(this, 0x194);
        st = St(this);
        *reinterpret_cast<Rect*>(st + 0x54) = rect;
    }
}

void Display::SetViewport(const Rect& rect)
{
    // 0x10002820
    unsigned char* st = St(this);
    Rect& viewport = *reinterpret_cast<Rect*>(st + 0x44);
    if (viewport.x == rect.x && viewport.y == rect.y && viewport.w == rect.w && viewport.h == rect.h) {
        Rect& scissor = *reinterpret_cast<Rect*>(st + 0x54);
        if (scissor.x == rect.x && scissor.y == rect.y && scissor.w == rect.w && scissor.h == rect.h) return;
        VCall<void>(this, 0x194);
    } else {
        VCall<void>(this, 0x190);
        st = St(this);
        *reinterpret_cast<Rect*>(st + 0x44) = rect;
        VCall<void>(this, 0x194);
    }
    st = St(this);
    *reinterpret_cast<Rect*>(st + 0x54) = rect;
}

bool Display::GetAllVideoModes(const Array<Display::VideoMode, 20>*& out) const
{
    // 0x100029e0
    out = reinterpret_cast<const Array<Display::VideoMode, 20>*>(D(this) + 0x230);
    return *reinterpret_cast<void* const*>(D(this) + 0x230) != 0;
}

bool Display::GetDefaultVideoMode(Display::VideoMode& out) const
{
    // 0x10002a30
    out = *reinterpret_cast<const Display::VideoMode*>(D(this) + 0x24c);
    return true;
}

bool Display::GetCurrentVideoMode(Display::VideoMode& out) const
{
    // 0x10002a00
    out = *reinterpret_cast<const Display::VideoMode*>(D(this) + 0x23c);
    return true;
}

bool Display::GetAllVideoFormats(const Array<Display::VideoFormat, 20>*& out) const
{
    // 0x10011dd0
    out = reinterpret_cast<const Array<Display::VideoFormat, 20>*>(D(this) + 0x25c);
    return reinterpret_cast<uintptr_t>(*reinterpret_cast<void* const*>(D(this) + 0x25c)) > 1u;
}

bool Display::GetCurrentVideoFormat(Display::VideoFormat& out) const
{
    // 0x10011d70
    out = *reinterpret_cast<const Display::VideoFormat*>(D(this) + 0x268);
    return true;
}

void __cdecl LayerGroup::Render()
{
    // 0x10021870 (shared no-op body)
}

}  // namespace Jet
