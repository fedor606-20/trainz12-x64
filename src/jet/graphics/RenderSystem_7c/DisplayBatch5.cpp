// Simple/evidenced Display exports batch 5 (RenderSystem_7c).
#include <jet/RenderSystem.h>
#include <jet/VCall.h>

namespace Jet {

namespace {
inline char* D(Display* d) { return reinterpret_cast<char*>(d); }
inline unsigned char* St(Display* d) { return *reinterpret_cast<unsigned char**>(D(d) + 0x54); }
}  // namespace

void Display::SetTextureMode(bool on)
{
    // 0x100031f0
    unsigned char* st = St(this);
    if (st[0x65] != (on ? 1 : 0)) {
        st[0x65] = on ? 1 : 0;
        if (D(this)[0x4a]) {
            VCall<void>(this, 0x214);
        }
    }
}

void Display::SetGlobalTextureMode(bool on)
{
    // 0x10003260
    if (D(this)[0x4a] != (on ? 1 : 0)) {
        D(this)[0x4a] = on ? 1 : 0;
        VCall<void>(this, 0x214);
    }
}

bool Display::SetLightMode(int idx, bool on)
{
    // 0x10003c70
    if (idx < 0) return false;
    const int count = *reinterpret_cast<int*>(D(this) + 0x50);
    if (idx >= count) return false;
    unsigned char* const enabled = *reinterpret_cast<unsigned char**>(St(this) + 0x110);
    if (enabled[idx] != (on ? 1 : 0)) {
        VCall<void, int>(this, 0x224, idx);
        enabled[idx] = on ? 1 : 0;
    }
    return true;
}



}  // namespace Jet
