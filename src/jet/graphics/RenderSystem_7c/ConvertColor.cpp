// Simple/evidenced Display::ConvertColor (RenderSystem_7c).
#include <jet/RenderSystem.h>

namespace Jet {

unsigned long Display::ConvertColor(const Color& c)
{
    // 0x1001b8e0
    if (internalColorFormat == 1) {
        float r = c.r, g = c.g, b = c.b, a = c.a;
        if (r <= 0.0f) r = 0.0f; else if (r > 1.0f) r = 1.0f;
        if (g <= 0.0f) g = 0.0f; else if (g > 1.0f) g = 1.0f;
        if (b <= 0.0f) b = 0.0f; else if (b > 1.0f) b = 1.0f;
        if (a <= 0.0f) a = 0.0f; else if (a > 1.0f) a = 1.0f;
        unsigned char ar = static_cast<unsigned char>(a * 255.0f);
        unsigned char rr = static_cast<unsigned char>(r * 255.0f);
        unsigned char gg = static_cast<unsigned char>(g * 255.0f);
        unsigned char bb = static_cast<unsigned char>(b * 255.0f);
        return (static_cast<unsigned long>(ar) << 24) | (static_cast<unsigned long>(rr) << 16) |
               (static_cast<unsigned long>(gg) << 8) | static_cast<unsigned long>(bb);
    }
    if (internalColorFormat != 2) return 0xffffffffu;
    float r = c.r, g = c.g, b = c.b, a = c.a;
    if (r <= 0.0f) r = 0.0f; else if (r > 1.0f) r = 1.0f;
    if (g <= 0.0f) g = 0.0f; else if (g > 1.0f) g = 1.0f;
    if (b <= 0.0f) b = 0.0f; else if (b > 1.0f) b = 1.0f;
    if (a <= 0.0f) a = 0.0f; else if (a > 1.0f) a = 1.0f;
    unsigned char ar = static_cast<unsigned char>(a * 255.0f);
    unsigned char bb = static_cast<unsigned char>(b * 255.0f);
    unsigned char gg = static_cast<unsigned char>(g * 255.0f);
    unsigned char rr = static_cast<unsigned char>(r * 255.0f);
    return (static_cast<unsigned long>(ar) << 24) | (static_cast<unsigned long>(bb) << 16) |
           (static_cast<unsigned long>(gg) << 8) | static_cast<unsigned long>(rr);
}

Color Display::ConvertColor(unsigned long v)
{
    // 0x1001bc40
    Color out{};
    if (internalColorFormat == 1) {
        out.r = (v >> 16 & 0xff) * (1.0f / 255.0f);
        out.g = (v >> 8 & 0xff) * (1.0f / 255.0f);
        out.b = (v & 0xff) * (1.0f / 255.0f);
    } else if (internalColorFormat == 2) {
        out.r = (v & 0xff) * (1.0f / 255.0f);
        out.g = (v >> 8 & 0xff) * (1.0f / 255.0f);
        out.b = (v >> 16 & 0xff) * (1.0f / 255.0f);
    } else {
        return out;
    }
    out.a = (v >> 24) * (1.0f / 255.0f);
    return out;
}

}  // namespace Jet
