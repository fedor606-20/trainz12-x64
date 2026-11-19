// Simple/evidenced ClearLayer exports (RenderSystem_7c).
#include <jet/RenderSystem.h>

namespace Jet {

Color ClearLayer::GetClearColor()
{
    // 0x10004db0 shared with Texture::GetAverageColor, color at +0x20
    return *reinterpret_cast<Color*>(reinterpret_cast<char*>(this) + 0x20);
}

void ClearLayer::SetClearColor(const Color& c)
{
    // 0x1000b1a0
    *reinterpret_cast<Color*>(reinterpret_cast<char*>(this) + 0x20) = c;
}

}  // namespace Jet
