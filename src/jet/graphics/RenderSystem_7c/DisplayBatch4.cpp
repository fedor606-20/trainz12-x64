// Simple/evidenced exports batch 4 (RenderSystem_7c).
#include <jet/RenderSystem.h>
#include <jet/VCall.h>

namespace Jet {

namespace {
inline char* D(Display* d) { return reinterpret_cast<char*>(d); }
inline unsigned char* St(Display* d) { return *reinterpret_cast<unsigned char**>(D(d) + 0x54); }
}  // namespace

void Display::SetAntiAliasing(bool on)
{
    unsigned char* st = St(this);
    if ((st[0xa1] & 0x10) && (((st[0xa1] >> 3) & 1) != (on ? 1 : 0))) {
        unsigned char* s = St(this);
        s[0xa1] = static_cast<unsigned char>((s[0xa1] & ~8) | (on ? 8 : 0));
        VCall<void>(this, 0x1d4);
    }
}

void Overlay::SetClipping(const Rect& r)
{
    *reinterpret_cast<Rect*>(reinterpret_cast<char*>(this) + 0x24) = r;
}

Rect Overlay::GetClipping() const
{
    return *reinterpret_cast<const Rect*>(reinterpret_cast<const char*>(this) + 0x24);
}

}  // namespace Jet
