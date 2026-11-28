// Unit U9 of WaterSystem_7c: WaterSurface, the height grid the managers fill. Behaviour follows the TS12
// assembly (addresses per function).
#include <jet/WaterSurface.h>

#include <jet/KernelStandard.h>  // Alloc

namespace Jet {

// 0x10015c10. The original leaves width, height and the scale untouched.
WaterSurface::WaterSurface()
{
    data = nullptr;
}

// 0x10015c50. The scale argument is ignored (the original reads and writes this+0x10/+0x14 instead).
WaterSurface::WaterSurface(const Size& size, const Vector2& /*scale*/)
{
    data = nullptr;
    width = size.w;
    height = size.h;
    const unsigned int count = static_cast<unsigned int>(width * height);
    const unsigned long long bytes = static_cast<unsigned long long>(count) * sizeof(float);
    const unsigned int request = static_cast<unsigned int>(bytes) | (bytes >> 32 ? ~0u : 0u);
    data = static_cast<float*>(Alloc::New(request));
}

// 0x10015c20
WaterSurface::WaterSurface(const WaterSurface& o) : WaterGrid<float>(o), scale(o.scale)
{
}

// 0x10015ce0 (identical code folded with WaterGrid<float>::~WaterGrid<float>)
WaterSurface::~WaterSurface() = default;

// 0x10015b60
WaterSurface& __thiscall WaterSurface::operator=(const WaterSurface& o)
{
    WaterGrid<float>::operator=(o);
    scale = o.scale;
    return *this;
}

// 0x10015d00: the scale is stored, then the grid is (re)allocated; the old block is not freed (the original
// leaks it too).
void __thiscall WaterSurface::Initialize(const Size& size, const Vector2& scale_)
{
    scale = scale_;
    width = size.w;
    height = size.h;
    const unsigned int count = static_cast<unsigned int>(width * height);
    const unsigned long long bytes = static_cast<unsigned long long>(count) * sizeof(float);
    const unsigned int request = static_cast<unsigned int>(bytes) | (bytes >> 32 ? ~0u : 0u);
    data = static_cast<float*>(Alloc::New(request));
}

// 0x10015c00: the original never fills the normal
void __thiscall WaterSurface::GetNormal(int /*x*/, int /*y*/, Vector3& /*normal*/)
{
}

// 0x10002130
const Vector2& __thiscall WaterSurface::GetScale()
{
    return scale;
}

}  // namespace Jet
