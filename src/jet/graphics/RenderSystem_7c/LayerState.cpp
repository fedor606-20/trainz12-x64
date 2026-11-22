// U8: local layer state only. Construction, group membership and rendering still use the original.
#include <jet/RenderSystem.h>

namespace Jet {

Size LayerTargetSize(const Display* display);

namespace {
template<class T>
T& State(Display* display, unsigned offset)
{
    return *reinterpret_cast<T*>(const_cast<unsigned char*>(display->State()) + offset);
}

void DisplayFlag(Display* display, unsigned offset, unsigned mask, unsigned slot, bool enabled)
{
    if (((State<unsigned char>(display, offset) & mask) != 0) == enabled) return;
    VCall<void>(display, slot, enabled);
    auto& bits = State<unsigned char>(display, offset);
    bits = static_cast<unsigned char>((bits & ~mask) | (enabled ? mask : 0));
}

void DepthTest(Display* display, bool enabled)
{
    if (((State<unsigned char>(display, 0xa0) & 2) != 0) != enabled) {
        VCall<void>(display, 0x1ac, 2, enabled);
        auto& bits = State<unsigned char>(display, 0xa0);
        bits = static_cast<unsigned char>((bits & ~2u) | (enabled ? 2u : 0u));
    }
}

}

void __cdecl Layer::RenderLayer(SystemResourceViewerList&)
{
    if (!IsVisible() || !VCall<bool>(this, 0x28)) return;
    VCall<void>(this, 0x20); // Begin
    VCall<void>(this, 0x18); // Render
    VCall<void>(this, 0x24); // End
}

void __cdecl Layer::UpdateLayer(float dt)
{
    if (IsVisible()) VCall<void>(this, 0x1c, dt);
}

bool __cdecl Layer::Collides(const Vector2&) const { return false; }
bool __cdecl Layer::Collides(const Vector2&, const Vector2&) const { return false; }
void __cdecl Layer::Reset() {}
void __cdecl Layer::Update(float) {}
void __cdecl Layer::Begin() {}
void __cdecl Layer::End() {}
bool __cdecl Layer::CanRender() { return true; }

int __cdecl Layer::GetPriority() const { return priority; }  // 0x100106d0
bool __cdecl Layer::IsVisible() const { return (flags & 1) != 0; }  // 0x1000ae40
bool __cdecl Layer::IsGroup() const { return (flags & 2) != 0; }  // 0x1000aef0

void __cdecl Layer::SetVisible(bool visible)  // 0x1000ae50
{
    flags = static_cast<unsigned char>((flags & ~1u) | (visible ? 1u : 0u));
}

void __cdecl Overlay::SetViewPortMode(bool enabled)  // 0x10038570
{
    overlayFlags = static_cast<unsigned char>((overlayFlags & ~2u) | (enabled ? 2u : 0u));
}

void __cdecl Overlay::SetClipping(const Rect& rect) { clipping = rect; }  // 0x10038c00
Rect __cdecl Overlay::GetClipping() const { return clipping; }  // 0x10038c20

void __cdecl Overlay::Begin()
{
    auto* display = GetRenderKernel()->GetDisplay();
    overlayFlags = static_cast<unsigned char>((overlayFlags & ~1u) | (State<unsigned char>(display, 0x64) & 1));
    display->SetScissorMode(true);
    Rect viewport;
    GetParent()->GetGroupViewRect(viewport);
    viewport.y = LayerTargetSize(display).h - viewport.h - viewport.y;
    display->SetViewport(viewport);
    if (overlayFlags & 2) display->Set2DViewNormalized(true);
    else display->Set2DView(0, static_cast<float>(viewport.w), static_cast<float>(viewport.h), 0, -1000, 1000);

    Rect scissor = clipping;
    GetParent()->ConvertToWindowAndClip(scissor);
    scissor.y = LayerTargetSize(display).h - scissor.h - scissor.y;
    display->SetScissor(scissor);
    VCall<void>(display, 0x50);

    DisplayFlag(display, 0xa1, 4, 0x1f0, false);
    DisplayFlag(display, 0xa0, 0x20, 0x1e4, false);
    DisplayFlag(display, 0xa1, 1, 0x1ec, true);
    if (State<int>(display, 0x68) != 4 || State<int>(display, 0x6c) != 5) {
        VCall<void>(display, 0x1dc, 4, 5);
        State<int>(display, 0x68) = 4;
        State<int>(display, 0x6c) = 5;
    }
    overlayFlags = static_cast<unsigned char>((overlayFlags & ~4u) | ((State<unsigned char>(display, 0xa0) & 1) << 2));
    DisplayFlag(display, 0xa0, 1, 0x18c, false);
    savedDepthFlags = static_cast<unsigned char>((savedDepthFlags & ~3u) | ((State<unsigned char>(display, 0xa0) >> 1) & 3));
    savedCullMode = State<int>(display, 0x28);
    DepthTest(display, false);
    DisplayFlag(display, 0xa0, 4, 0x1b4, false);
    display->SetCullMode(DCULL_NONE);
}

void __cdecl Overlay::End()
{
    auto* display = GetRenderKernel()->GetDisplay();
    DisplayFlag(display, 0xa0, 1, 0x18c, (overlayFlags & 4) != 0);
    DisplayFlag(display, 0xa1, 1, 0x1ec, false);
    display->SetScissorMode((overlayFlags & 1) != 0);
    DepthTest(display, (savedDepthFlags & 1) != 0);
    DisplayFlag(display, 0xa0, 4, 0x1b4, (savedDepthFlags & 2) != 0);
    display->SetCullMode(static_cast<DCull>(savedCullMode));
}

}  // namespace Jet
