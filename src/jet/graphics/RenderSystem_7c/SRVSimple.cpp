// Simple/evidenced SystemResourceViewer exports (RenderSystem_7c).
#include <jet/RenderSystem.h>

namespace Jet {

void SystemResourceViewer::SetName(const PString& name)
{
    // 0x1000abe0 shared with Modifier::SetName, PString at +8
    *reinterpret_cast<PString*>(reinterpret_cast<char*>(this) + 8) = name;
}

}  // namespace Jet
