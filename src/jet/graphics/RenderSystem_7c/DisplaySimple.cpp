// Simple Display exports (RenderSystem_7c).
#include <jet/RenderSystem.h>

namespace Jet {

namespace {
inline char* DisplayPtr(Display* display) { return reinterpret_cast<char*>(display); }
inline const char* DisplayPtr(const Display* display) { return reinterpret_cast<const char*>(display); }
}  // namespace

DFormatId Display::internalColorFormat = static_cast<DFormatId>(2);

DFormatId Display::GetInternalColorFormat()
{
    return internalColorFormat;
}

float Display::GetPhysicalScreenRatio()
{
    return *reinterpret_cast<float*>(DisplayPtr(this) + 0x6c);
}

bool Display::IsRenderableState() const
{
    return DisplayPtr(this)[0x49] != 0;
}

bool Display::GetReflectionCull() const
{
    return DisplayPtr(this)[0x7c] != 0;
}

float Display::GetNormalLength() const
{
    return *reinterpret_cast<const float*>(DisplayPtr(this) + 0x74);
}

bool Display::GetChunkColorMode() const
{
    return DisplayPtr(this)[0x78] != 0;
}

bool Display::GetChunkVertexCountMode() const
{
    return DisplayPtr(this)[0x79] != 0;
}

bool Display::GetDynamicGeometryDisplayMode() const
{
    return DisplayPtr(this)[0x7b] != 0;
}

bool Display::GetStaticGeometryDisplayMode() const
{
    return DisplayPtr(this)[0x7a] != 0;
}

}  // namespace Jet
