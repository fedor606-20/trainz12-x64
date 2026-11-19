// Simple/evidenced exports for small RenderSystem classes (batch).
// Behaviour/offsets follow TS12 decomp/exports/original bytes.
#include <jet/Modifier.h>
#include <jet/RenderSystem.h>

namespace Jet {

namespace {
inline char* R(void* p) { return reinterpret_cast<char*>(p); }
inline const char* R(const void* p) { return reinterpret_cast<const char*>(p); }
}  // namespace

bool NonRenderContext::BindToThread() { return true; }      // 0x100015d0
bool NonRenderContext::UnbindFromThread() { return true; }  // 0x100015d0
bool NonRenderContext::Initialize(Display*) { return true; } // 0x100015d0

const PString& SystemResourceViewer::GetName() const
{
    // 0x1001d560 shared body -> +8
    return *reinterpret_cast<const PString*>(R(this) + 8);
}

unsigned int DisplaySaveBuffer::GetType() const
{
    // 0x1000cab0 shared body -> +8
    return *reinterpret_cast<const unsigned int*>(R(this) + 8);
}

const IList<Layer>& LayerGroup::GetLayerList() const
{
    // 0x1000b2b0 shared body -> +0x20
    return *reinterpret_cast<const IList<Layer>*>(R(this) + 0x20);
}

const Rect& LayerGroup::GetViewRect() const
{
    // 0x1000b2c0 shared body -> +0x2c
    return *reinterpret_cast<const Rect*>(R(this) + 0x2c);
}

LayerGroup* LayerManager::GetLayerGroup()
{
    // 0x1003ebc0 shared body -> +4
    return *reinterpret_cast<LayerGroup**>(R(this) + 4);
}

OS* RenderKernel::GetOS()
{
    // 0x1000c1c0 shared body -> +0x8c
    return *reinterpret_cast<OS**>(R(this) + 0x8c);
}

SceneGraph* RenderKernel::GetSceneGraph()
{
    // 0x1000d230 shared body -> +0x80
    return *reinterpret_cast<SceneGraph**>(R(this) + 0x80);
}

Display* RenderKernel::GetDisplay()
{
    // 0x1000d2d0 -> +0x7c
    return *reinterpret_cast<Display**>(R(this) + 0x7c);
}

MaterialManager* RenderKernel::GetMaterialManager()
{
    // 0x1000d2e0 -> +0x90
    return *reinterpret_cast<MaterialManager**>(R(this) + 0x90);
}

Jet::TextureManager* RenderKernel::GetTextureManager()
{
    // 0x1000d2f0 shared body -> +0x94
    return *reinterpret_cast<Jet::TextureManager**>(R(this) + 0x94);
}

RenderKernel* RenderKernel::GetRenderKernel()
{
    // 0x1000d300 -> this - 0x28
    return reinterpret_cast<RenderKernel*>(R(this) - 0x28);
}

bool SceneGraph::IsEmpty() const
{
    // 0x1000d0c0
    const void* const* p = reinterpret_cast<const void* const*>(this);
    return p[0] == 0 && p[1] == 0;
}

ModifierUpdateTask* SceneGraph::GetModifierTask()
{
    // 0x1000cab0 shared body -> +8
    return *reinterpret_cast<ModifierUpdateTask**>(R(this) + 8);
}

void SceneGraph::SetModifierTask(ModifierUpdateTask* task)
{
    // 0x1000cac0 shared body -> +8
    *reinterpret_cast<ModifierUpdateTask**>(R(this) + 8) = task;
}

void Overlay::SetViewPortMode(bool on)
{
    // 0x10038570 -> +0x20 bit 1
    unsigned char& b = reinterpret_cast<unsigned char*>(R(this))[0x20];
    b = static_cast<unsigned char>((b & ~2u) | (on ? 2u : 0u));
}

void Layer::ConvertToWindow(Point& point) const
{
    // 0x100316c0
    LayerGroup* parent = *reinterpret_cast<LayerGroup* const*>(R(this) + 0x1c);
    if (parent) parent->ConvertToWindow(point);
}

void Layer::ConvertToWindowAndClip(Rect& rect) const
{
    // 0x100316e0
    LayerGroup* parent = *reinterpret_cast<LayerGroup* const*>(R(this) + 0x1c);
    if (parent) parent->ConvertToWindowAndClip(rect);
}

void LayerGroup::ConvertToWindow(Point& point) const
{
    // 0x1000b2f0
    const char* p = R(this);
    point.x += *reinterpret_cast<const int*>(p + 0x2c);
    point.y += *reinterpret_cast<const int*>(p + 0x30);
    for (const LayerGroup* g = *reinterpret_cast<LayerGroup* const*>(p + 0x1c); g; g = *reinterpret_cast<LayerGroup* const*>(R(g) + 0x1c)) {
        const char* gp = R(g);
        point.x += *reinterpret_cast<const int*>(gp + 0x2c);
        point.y += *reinterpret_cast<const int*>(gp + 0x30);
    }
}

void LayerGroup::ConvertToWindowAndClip(Rect& rect) const
{
    // 0x10031d30
    const LayerGroup* g = this;
    do {
        const char* p = R(g);
        const int gw = *reinterpret_cast<const int*>(p + 0x34);
        const int gh = *reinterpret_cast<const int*>(p + 0x38);
        if (gw > 0 && gh > 0) {
            if (gw < rect.w + rect.x) rect.w = gw - rect.x;
            if (gh < rect.h + rect.y) rect.h = gh - rect.y;
        }
        rect.x += *reinterpret_cast<const int*>(p + 0x2c);
        rect.y += *reinterpret_cast<const int*>(p + 0x30);
        g = *reinterpret_cast<LayerGroup* const*>(p + 0x1c);
    } while (g);
}

void LayerManager::Reset()
{
    // 0x1000b550
    void* group = *reinterpret_cast<void**>(R(this) + 4);
    if (group) {
        using Fn = void(__thiscall*)(void*);
        (*reinterpret_cast<Fn* const*>(group))[5](group);
    }
}

void LayerGroup::SetViewRect(const Rect& rect)
{
    // 0x1000b2d0
    *reinterpret_cast<Rect*>(R(this) + 0x2c) = rect;
}
}  // namespace Jet
