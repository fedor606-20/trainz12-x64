// Simple/evidenced exports batch 3 (RenderSystem_7c).
#include <jet/KernelStandard.h>
#include <jet/RenderSystem.h>
#include <jet/VCall.h>
#include <windows.h>

namespace Jet {

namespace {
inline char* RD(Display* d) { return reinterpret_cast<char*>(d); }
inline char* RR(RenderKernel* r) { return reinterpret_cast<char*>(r); }
inline char* RS(SceneGraph* s) { return reinterpret_cast<char*>(s); }
}  // namespace

const char* Display::GetExtensionString(DExtension idx) const
{
    // 0x1001b890 - data table not reconstructed; return nullptr for unknown
    (void)idx;
    return nullptr;
}

void Display::SetRenderTarget(DisplayTexture* tex, unsigned int idx)
{
    // 0x10003220
    if (tex) {
        *reinterpret_cast<unsigned int*>(reinterpret_cast<char*>(tex) + 0x5c) = idx;
        VCall<void, unsigned int>(tex, 0x1d0, idx);
    }
}

void Display::RestoreGamma()
{
    // 0x1001c910
    if (*reinterpret_cast<int*>(RD(this) + 0x2f0) > 0) {
        HDC hdc = GetDC(NULL);
        SetDeviceGammaRamp(hdc, RD(this) + 0x2f8);
        *reinterpret_cast<float*>(RD(this) + 0x2f4) = 1.0f;
    }
}

int Display::GetTargetPPI() const
{
    // 0x1001b7a0
    OS* os = GetOS();
    if (!os) return -1;
    Options* opts = os->GetOptions();
    if (!opts) return -1;
    int value = -1;
    if (opts->Get("screenppi", value, -1)) return value;
    return -1;
}

void SceneGraph::Update(float /*dt*/)
{
    // 0x1003be90
    void* p = *reinterpret_cast<void**>(RS(this) + 8);
    if (p) VCall<void>(p, 0x2c);
}

SceneGraph::~SceneGraph()
{
    // 0x.... scalar (placeholder no-op)
}

void RenderKernel::DestroySceneGraph()
{
    // 0x1000d200
    SceneGraph* sg = *reinterpret_cast<SceneGraph**>(RR(this) + 0x80);
    if (sg) {
        sg->~SceneGraph();
        Alloc::Delete(sg);
    }
    *reinterpret_cast<SceneGraph**>(RR(this) + 0x80) = nullptr;
}

void RenderKernel::DumpScreen(unsigned int idx)
{
    // 0x1003a010
    if (idx == 0) {
        Display* d = GetDisplay();
        *reinterpret_cast<int*>(RR(this) + 0x9c) = *reinterpret_cast<int*>(reinterpret_cast<char*>(d) + 0x3c) + 1;
        *reinterpret_cast<unsigned char*>(RR(this) + 0x99) = 1;
    } else {
        *reinterpret_cast<int*>(RR(this) + 0x9c) = static_cast<int>(idx);
        *reinterpret_cast<unsigned char*>(RR(this) + 0x99) = 1;
    }
}

}  // namespace Jet
