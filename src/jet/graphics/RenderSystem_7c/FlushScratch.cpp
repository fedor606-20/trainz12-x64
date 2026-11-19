// Jet::FlushScratchBuffer (RenderSystem_7c).
#include <jet/KernelStandard.h>
#include <jet/RenderSystem.h>

namespace Jet {
namespace {
void*& ScratchPtr() { static void* p = nullptr; return p; }
int& ScratchSize() { static int s = 0; return s; }
}  // namespace

void __cdecl FlushScratchBuffer()
{
    // 0x1000eed0
    if (ScratchPtr()) {
        Alloc::Delete(ScratchPtr());
        ScratchPtr() = nullptr;
    }
    ScratchSize() = 0;
}

}  // namespace Jet
