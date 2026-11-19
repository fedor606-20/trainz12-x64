// Additional simple/evidenced Display getters (RenderSystem_7c).
#include <jet/RenderSystem.h>

namespace Jet {

namespace {
inline const char* DRaw(const Display* d) { return reinterpret_cast<const char*>(d); }
inline const unsigned char* DState(const Display* d) { return *reinterpret_cast<const unsigned char* const*>(DRaw(d) + 0x54); }
}  // namespace

bool Display::SupportsAntiAliasing() const
{
    // 0x100032d0 -> (state+0xa1) bit 4
    return (DState(this)[0xa1] & 0x10u) != 0;
}

bool Display::GetAntiAliasing() const
{
    // 0x10003340 -> (state+0xa1) bit 3
    return (DState(this)[0xa1] & 0x08u) != 0;
}

void Display::GetAlphaBlendFunction(Src::Factor& src, Dst::Factor& dst) const
{
    // 0x100033b0 -> state+0x68, state+0x6c
    src = *reinterpret_cast<const Src::Factor*>(DState(this) + 0x68);
    dst = *reinterpret_cast<const Dst::Factor*>(DState(this) + 0x6c);
}

unsigned long Display::GetColorWriteMask() const
{
    // 0x10003630 -> state+0x9c
    return *reinterpret_cast<const unsigned long*>(DState(this) + 0x9c);
}

bool Display::GetDepthWriteMode() const
{
    // 0x10003680 -> (state+0xa0) bit 1
    return (DState(this)[0xa0] & 0x02u) != 0;
}

bool Display::GetDepthTestMode() const
{
    // 0x100036f0 -> (state+0xa0) bit 2
    return (DState(this)[0xa0] & 0x04u) != 0;
}

DFunction Display::GetDepthFunction() const
{
    // 0x10003760 -> state+0xd4
    return *reinterpret_cast<const DFunction*>(DState(this) + 0xd4);
}

bool Display::GetDepthClamp() const
{
    // 0x100037f0 -> (state+0xa0) bit 6
    return (DState(this)[0xa0] & 0x40u) != 0;
}

bool Display::GetAlphaTestMode() const
{
    // 0x10003810 -> (state+0xa0) bit 5
    return (DState(this)[0xa0] & 0x20u) != 0;
}

float Display::GetAlphaTestReference() const
{
    // 0x10003880 -> state+0xcc
    return *reinterpret_cast<const float*>(DState(this) + 0xcc);
}

DFunction Display::GetAlphaTestFunction() const
{
    // 0x10003890 -> state+0xd0
    return *reinterpret_cast<const DFunction*>(DState(this) + 0xd0);
}

bool Display::GetAlphaBlendMode() const
{
    // 0x10003920 -> (state+0xa1) bit 0
    return (DState(this)[0xa1] & 0x01u) != 0;
}

bool Display::GetLightingMode() const
{
    // 0x10003970 -> (state+0xa1) bit 2
    return (DState(this)[0xa1] & 0x04u) != 0;
}

float Display::GetFogDensity() const
{
    // 0x10003aa0 -> state+0x8c
    return *reinterpret_cast<const float*>(DState(this) + 0x8c);
}

DColorMaterialTypes Display::GetColorMaterialMode()
{
    // 0x10003c60 -> state+0x98
    return *reinterpret_cast<const DColorMaterialTypes*>(DState(this) + 0x98);
}

bool Display::GetVertexWeightMode() const
{
    // 0x10003f00 -> (state+0xa1) bit 1
    return (DState(this)[0xa1] & 0x02u) != 0;
}

bool Display::GetStencilTestMode() const
{
    // 0x10003f80 -> (state+0xa0) bit 3
    return (DState(this)[0xa0] & 0x08u) != 0;
}

unsigned long Display::GetStencilWriteMask(DFace face) const
{
    // 0x10004030 -> state+0xe4 + face*0x1c
    const unsigned char* st = DState(this);
    const int f = *reinterpret_cast<const int*>(&face);
    return *reinterpret_cast<const unsigned long*>(st + 0xe4 + f * 0x1c);
}

bool Display::GetWireFrameMode() const
{
    // 0x10004250 -> (state+0xa0) bit 0
    return (DState(this)[0xa0] & 0x01u) != 0;
}

}  // namespace Jet
