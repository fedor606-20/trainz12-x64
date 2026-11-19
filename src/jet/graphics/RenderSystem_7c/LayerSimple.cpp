// Simple/evidenced Layer exports (RenderSystem_7c).
// Behaviour and layout follow TS12 decomp/exports/original bytes.
#include <jet/PString.h>
#include <jet/RenderSystem.h>

namespace Jet {

LayerManager* Layer::layerManager = nullptr; // 0x100b8078

namespace {
inline char* Raw(Layer* layer) { return reinterpret_cast<char*>(layer); }
inline const char* Raw(const Layer* layer) { return reinterpret_cast<const char*>(layer); }
}

const PString& Layer::GetName() const
{
    // 0x1003ea80
    return *reinterpret_cast<const PString*>(Raw(this) + 0x0c);
}

int Layer::GetPriority() const
{
    // 0x100106d0
    return *reinterpret_cast<const int*>(Raw(this) + 0x10);
}

bool Layer::IsVisible() const
{
    // 0x1000ae40
    return (reinterpret_cast<const unsigned char*>(Raw(this))[0x14] & 1u) != 0;
}

bool Layer::IsGroup() const
{
    // 0x1000aef0
    return ((reinterpret_cast<const unsigned char*>(Raw(this))[0x14] >> 1) & 1u) != 0;
}

int Layer::operator<(const Layer& other) const
{
    // 0x1000ae10
    return (*reinterpret_cast<const int*>(Raw(this) + 0x10) < other.GetPriority()) ? 1 : 0;
}

void Layer::SetName(const PString& name)
{
    // 0x1000ae70
    *reinterpret_cast<PString*>(Raw(this) + 0x0c) = name;
}

void Layer::SetVisible(bool visible)
{
    // 0x1000ae50
    unsigned char& flags = reinterpret_cast<unsigned char*>(Raw(this))[0x14];
    flags = static_cast<unsigned char>((flags & ~1u) | (visible ? 1u : 0u));
}

RenderKernel* Layer::GetRenderKernel() const
{
    // 0x1000ae00
    return *reinterpret_cast<RenderKernel* const*>(Raw(this) + 0x18);
}

LayerGroup* Layer::GetParent() const
{
    // 0x10010940
    return *reinterpret_cast<LayerGroup* const*>(Raw(this) + 0x1c);
}

LayerManager* Layer::GetLayerManager()
{
    // 0x1000ae30
    return layerManager;
}

void Layer::SetRenderKernel(RenderKernel* renderKernel)
{
    // 0x10021a60 (shared body)
    *reinterpret_cast<RenderKernel**>(Raw(this) + 0x18) = renderKernel;
}

bool Layer::CanRender()
{
    // 0x100015d0 (shared body)
    return true;
}

void Layer::Begin()
{
    // 0x10021870 (shared body)
}

void Layer::End()
{
    // 0x10021870 (shared body)
}

void Layer::Reset()
{
    // 0x10021870 (shared body)
}

void Layer::Update(float)
{
    // 0x10021870 (shared body)
}

void Layer::UpdateLayer(float dt)
{
    // 0x1000aed0
    if (IsVisible())
        Update(dt);
}

bool Layer::Collides(const Vector2&) const
{
    // 0x10009500 (shared body)
    return false;
}

bool Layer::Collides(const Vector2&, const Vector2&) const
{
    // 0x10009500 (shared body)
    return false;
}

}  // namespace Jet
