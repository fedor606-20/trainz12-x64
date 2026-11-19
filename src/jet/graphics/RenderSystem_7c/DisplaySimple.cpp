// Simple Display exports/data (RenderSystem_7c).
// internalColorFormat is initialized to 2 (ABGR8888) per TS12 original data/decomp.
#include <jet/RenderSystem.h>
#include <jet/RenderTypes.h>

namespace Jet {

namespace {
inline char* DisplayPtr(Display* display) { return reinterpret_cast<char*>(display); }
inline const char* DisplayPtr(const Display* display) { return reinterpret_cast<const char*>(display); }
}  // namespace

DFormatId Display::internalColorFormat = static_cast<DFormatId>(2);

DFormatId Display::GetInternalColorFormat()
{
    // 0x10004500
    return internalColorFormat;
}

float Display::GetPhysicalScreenRatio()
{
    // 0x10002050
    return *reinterpret_cast<float*>(DisplayPtr(this) + 0x6c);
}

bool Display::IsRenderableState() const
{
    // 0x100029d0
    return DisplayPtr(this)[0x49] != 0;
}

bool Display::GetReflectionCull() const
{
    // 0x10003620
    return DisplayPtr(this)[0x7c] != 0;
}

float Display::GetNormalLength() const
{
    // 0x10004330
    return *reinterpret_cast<const float*>(DisplayPtr(this) + 0x74);
}

bool Display::GetChunkColorMode() const
{
    // 0x10004390
    return DisplayPtr(this)[0x78] != 0;
}

bool Display::GetChunkVertexCountMode() const
{
    // 0x100043b0
    return DisplayPtr(this)[0x79] != 0;
}

bool Display::GetDynamicGeometryDisplayMode() const
{
    // 0x100043d0
    return DisplayPtr(this)[0x7b] != 0;
}

bool Display::GetStaticGeometryDisplayMode() const
{
    // 0x100043f0
    return DisplayPtr(this)[0x7a] != 0;
}

}  // namespace Jet
