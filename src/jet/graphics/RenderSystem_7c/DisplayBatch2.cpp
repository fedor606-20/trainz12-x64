// Simple/evidenced Display setters batch 2 (RenderSystem_7c).
#include <jet/RenderSystem.h>
#include <jet/VCall.h>

namespace Jet {

namespace {
inline char* D(Display* d) { return reinterpret_cast<char*>(d); }
inline unsigned char* St(Display* d) { return *reinterpret_cast<unsigned char**>(D(d) + 0x54); }
}  // namespace

void Display::SetScissorMode(bool on)
{
    unsigned char* st = St(this);
    if (st[0x64] != (on ? 1 : 0)) {
        VCall<void>(this, 0x198);
        St(this)[0x64] = on ? 1 : 0;
    }
}

void Display::SetAlphaBlendEquation(DBlendEquation eq)
{
    unsigned char* st = St(this);
    if (*reinterpret_cast<int*>(st + 0x70) != static_cast<int>(eq)) {
        VCall<void>(this, 0x1e0);
        *reinterpret_cast<int*>(St(this) + 0x70) = static_cast<int>(eq);
    }
}

void Display::SetClearColor(const Color& c)
{
    unsigned char* st = St(this);
    Color& cur = *reinterpret_cast<Color*>(st + 0x2c);
    if (cur.r != c.r || cur.g != c.g || cur.b != c.b || cur.a != c.a) {
        VCall<void>(this, 0x1a0);
        st = St(this);
        *reinterpret_cast<Color*>(st + 0x2c) = c;
    }
}

void Display::SetClearDepth(float v)
{
    unsigned char* st = St(this);
    if (*reinterpret_cast<float*>(st + 0x3c) != v) {
        VCall<void>(this, 0x1a4);
        *reinterpret_cast<float*>(St(this) + 0x3c) = v;
    }
}

void Display::SetClearStencil(long v)
{
    unsigned char* st = St(this);
    if (*reinterpret_cast<long*>(st + 0x40) != v) {
        VCall<void>(this, 0x1a8);
        *reinterpret_cast<long*>(St(this) + 0x40) = v;
    }
}

void Display::SetCullMode(DCull mode)
{
    unsigned char* st = St(this);
    if (*reinterpret_cast<int*>(st + 0x28) != static_cast<int>(mode)) {
        if (D(this)[0x7c]) {
            VCall<void, int>(this, 0x1d8, static_cast<int>(mode));
        }
        st = St(this);
        *reinterpret_cast<int*>(st + 0x28) = static_cast<int>(mode);
    }
}

void Display::SetReflectionCull(bool on)
{
    if (D(this)[0x7c] != (on ? 1 : 0)) {
        D(this)[0x7c] = on ? 1 : 0;
        unsigned char* st = St(this);
        if (*reinterpret_cast<int*>(st + 0x28) != 0) {
            *reinterpret_cast<int*>(st + 0x28) = 0;
            SetCullMode(DCULL_NONE);
        }
    }
}

void Display::SetColorWriteMask(unsigned long v)
{
    unsigned char* st = St(this);
    if (*reinterpret_cast<unsigned long*>(st + 0x9c) != v) {
        VCall<void, int>(this, 0x1ac, 1);
        *reinterpret_cast<unsigned long*>(St(this) + 0x9c) = v;
    }
}

void Display::SetDepthWriteMode(bool on)
{
    unsigned char* st = St(this);
    if (((st[0xa0] >> 1) & 1) != (on ? 1 : 0)) {
        VCall<void, int, bool>(this, 0x1ac, 2, on);
        unsigned char* s = St(this);
        s[0xa0] = static_cast<unsigned char>((s[0xa0] & ~2) | (on ? 2 : 0));
    }
}

void Display::SetDepthTestMode(bool on)
{
    unsigned char* st = St(this);
    if (((st[0xa0] >> 2) & 1) != (on ? 1 : 0)) {
        VCall<void>(this, 0x1b4);
        unsigned char* s = St(this);
        s[0xa0] = static_cast<unsigned char>((s[0xa0] & ~4) | (on ? 4 : 0));
    }
}

void Display::SetDepthFunction(DFunction fn)
{
    unsigned char* st = St(this);
    if (*reinterpret_cast<int*>(st + 0xd4) != static_cast<int>(fn)) {
        VCall<void>(this, 0x1b0);
        *reinterpret_cast<int*>(St(this) + 0xd4) = static_cast<int>(fn);
    }
}

void Display::SetDepthClamp(bool on)
{
    unsigned char* st = St(this);
    if (((st[0xa0] >> 6) & 1) != (on ? 1 : 0)) {
        VCall<void>(this, 0x1b8);
        unsigned char* s = St(this);
        s[0xa0] = static_cast<unsigned char>((s[0xa0] & ~0x40) | (on ? 0x40 : 0));
    }
}

void Display::SetAlphaTestMode(bool on)
{
    unsigned char* st = St(this);
    if (((st[0xa0] >> 5) & 1) != (on ? 1 : 0)) {
        VCall<void>(this, 0x1e4);
        unsigned char* s = St(this);
        s[0xa0] = static_cast<unsigned char>((s[0xa0] & ~0x20) | (on ? 0x20 : 0));
    }
}

void Display::SetAlphaBlendMode(bool on)
{
    unsigned char* st = St(this);
    if ((st[0xa1] & 1) != (on ? 1 : 0)) {
        VCall<void>(this, 0x1ec);
        unsigned char* s = St(this);
        s[0xa1] = static_cast<unsigned char>((s[0xa1] & ~1) | (on ? 1 : 0));
    }
}

void Display::SetLightingMode(bool on)
{
    unsigned char* st = St(this);
    if (((st[0xa1] >> 2) & 1) != (on ? 1 : 0)) {
        VCall<void>(this, 0x1f0);
        unsigned char* s = St(this);
        s[0xa1] = static_cast<unsigned char>((s[0xa1] & ~4) | (on ? 4 : 0));
    }
}

void Display::SetSeperateSpecularMode(bool on)
{
    unsigned char* st = St(this);
    if (st[0x74] != (on ? 1 : 0)) {
        VCall<void>(this, 0x1f4);
        St(this)[0x74] = on ? 1 : 0;
    }
}

void Display::SetLocalViewSpecularMode(bool on)
{
    unsigned char* st = St(this);
    if (st[0x75] != (on ? 1 : 0)) {
        VCall<void>(this, 0x1f8);
        St(this)[0x75] = on ? 1 : 0;
    }
}

void Display::SetFogMode(DFogMode mode)
{
    unsigned char* st = St(this);
    if (*reinterpret_cast<int*>(st + 0x78) != static_cast<int>(mode)) {
        VCall<void>(this, 0x1fc);
        *reinterpret_cast<int*>(St(this) + 0x78) = static_cast<int>(mode);
    }
}

void Display::SetFogDensity(float v)
{
    unsigned char* st = St(this);
    if (*reinterpret_cast<float*>(st + 0x8c) != v) {
        VCall<void>(this, 0x200);
        *reinterpret_cast<float*>(St(this) + 0x8c) = v;
    }
}

void Display::SetFogColor(const Color& c)
{
    unsigned char* st = St(this);
    Color& cur = *reinterpret_cast<Color*>(st + 0x7c);
    if (cur.r != c.r || cur.g != c.g || cur.b != c.b || cur.a != c.a) {
        VCall<void>(this, 0x208);
        st = St(this);
        *reinterpret_cast<Color*>(st + 0x7c) = c;
    }
}

void Display::SetGlobalAmbient(const Color& c)
{
    unsigned char* st = St(this);
    Color& cur = *reinterpret_cast<Color*>(st + 0xa4);
    if (cur.r != c.r || cur.g != c.g || cur.b != c.b || cur.a != c.a) {
        VCall<void>(this, 0x20c);
        st = St(this);
        *reinterpret_cast<Color*>(st + 0xa4) = c;
    }
}

void Display::SetVertexWeightMode(bool on)
{
    unsigned char* st = St(this);
    if (((st[0xa1] >> 1) & 1) != (on ? 1 : 0)) {
        VCall<void>(this, 0x210);
        unsigned char* s = St(this);
        s[0xa1] = static_cast<unsigned char>((s[0xa1] & ~2) | (on ? 2 : 0));
    }
}

void Display::SetStencilWriteMask(unsigned long mask, DFace face)
{
    unsigned char* st = St(this);
    int idx = static_cast<int>(face);
    if (*reinterpret_cast<DFace*>(st + 0xe4 + idx * 0x1c) != face) {
        VCall<void>(this, 0x1c0);
        *reinterpret_cast<DFace*>(St(this) + 0xe4 + idx * 0x1c) = face;
    }
}

void Display::SetWireFrameMode(bool on)
{
    unsigned char* st = St(this);
    if ((st[0xa0] & 1) != (on ? 1 : 0)) {
        VCall<void>(this, 0x18c);
        unsigned char* s = St(this);
        s[0xa0] = static_cast<unsigned char>((s[0xa0] & ~1) | (on ? 1 : 0));
    }
}

void Display::SetAlphaTestFunction(DFunction fn, float ref)
{
    float r = ref;
    if (r <= 0.0f) r = 0.0f;
    else if (r > 1.0f) r = 1.0f;
    unsigned char* st = St(this);
    if (*reinterpret_cast<float*>(st + 0xd0) != static_cast<float>(fn) ||
        *reinterpret_cast<float*>(st + 0xcc) != r) {
        VCall<void, int, float>(this, 0x1e8, static_cast<int>(fn), r);
        st = St(this);
        *reinterpret_cast<float*>(st + 0xd0) = static_cast<float>(fn);
        *reinterpret_cast<float*>(st + 0xcc) = r;
    }
}

void Display::SetFogRange(float start, float end)
{
    unsigned char* st = St(this);
    if (*reinterpret_cast<float*>(st + 0x90) != start ||
        *reinterpret_cast<float*>(st + 0x94) != end) {
        VCall<void, float, float>(this, 0x204, start, end);
        st = St(this);
        *reinterpret_cast<float*>(st + 0x90) = start;
        *reinterpret_cast<float*>(st + 0x94) = end;
    }
}

void Display::SetAlphaBlendFunction(Src::Factor src, Dst::Factor dst)
{
    unsigned char* st = St(this);
    if (*reinterpret_cast<int*>(st + 0x68) != static_cast<int>(src) ||
        *reinterpret_cast<int*>(st + 0x6c) != static_cast<int>(dst)) {
        VCall<void, int>(this, 0x1dc, static_cast<int>(src));
        st = St(this);
        *reinterpret_cast<int*>(st + 0x68) = static_cast<int>(src);
        *reinterpret_cast<int*>(st + 0x6c) = static_cast<int>(dst);
    }
}

}  // namespace Jet
