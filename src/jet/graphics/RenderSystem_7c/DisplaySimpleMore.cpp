// Simple/evidenced Display exports (RenderSystem_7c), batch 2.
// Behaviour/offsets follow TS12 decomp/exports/original bytes.
#include <jet/Math.h>
#include <jet/RenderSystem.h>
#include <jet/SmallTypes.h>

namespace Jet {

namespace {
inline char* DRaw(Display* d) { return reinterpret_cast<char*>(d); }
inline const char* DRaw(const Display* d) { return reinterpret_cast<const char*>(d); }
inline unsigned char* DState(Display* d) { return *reinterpret_cast<unsigned char**>(DRaw(d) + 0x54); }
inline const unsigned char* DState(const Display* d) { return *reinterpret_cast<const unsigned char* const*>(DRaw(d) + 0x54); }
}  // namespace

Window* Display::GetWindow() const
{
    // 0x1000fc00 shared body -> +0x40
    return *reinterpret_cast<Window* const*>(DRaw(this) + 0x40);
}

OS* Display::GetOS() const
{
    // 0x1000fc40 shared body -> +0x44
    return *reinterpret_cast<OS* const*>(DRaw(this) + 0x44);
}

const Size& Display::GetWindowSize() const
{
    // 0x100029a0 -> +0x28c
    return *reinterpret_cast<const Size*>(DRaw(this) + 0x28c);
}

DisplaySelector* Display::GetDisplaySelector() const
{
    // 0x100029b0 -> +0x288
    return *reinterpret_cast<DisplaySelector* const*>(DRaw(this) + 0x288);
}

Display::ScreenMode Display::GetCurrentScreenMode() const
{
    // 0x100029c0 -> +0x22c
    return *reinterpret_cast<const ScreenMode*>(DRaw(this) + 0x22c);
}

bool Display::GetScissorMode() const
{
    // 0x10002990 -> state+0x64
    return DState(this)[0x64] != 0;
}

const Rect& Display::GetViewport() const
{
    // 0x10002a60 -> state+0x44
    return *reinterpret_cast<const Rect*>(DState(this) + 0x44);
}

const Color& Display::GetClearColor() const
{
    // 0x10003450 -> state+0x2c
    return *reinterpret_cast<const Color*>(DState(this) + 0x2c);
}

float Display::GetClearDepth() const
{
    // 0x100034e0 -> state+0x3c
    return *reinterpret_cast<const float*>(DState(this) + 0x3c);
}

long Display::GetClearStencil() const
{
    // 0x10003540 -> state+0x40
    return *reinterpret_cast<const long*>(DState(this) + 0x40);
}

DCull Display::GetCullMode() const
{
    // 0x10003580 -> state+0x28
    return *reinterpret_cast<const DCull*>(DState(this) + 0x28);
}

DBlendEquation Display::GetAlphaBlendEquation() const
{
    // 0x10003410 -> state+0x70
    return *reinterpret_cast<const DBlendEquation*>(DState(this) + 0x70);
}

DFogMode Display::GetFogMode() const
{
    // 0x10003a60 -> state+0x78
    return *reinterpret_cast<const DFogMode*>(DState(this) + 0x78);
}

const Color& Display::GetFogColor() const
{
    // 0x10003ba0 -> state+0x7c
    return *reinterpret_cast<const Color*>(DState(this) + 0x7c);
}

const Color& Display::GetGlobalAmbient() const
{
    // 0x10003df0 -> state+0xa4
    return *reinterpret_cast<const Color*>(DState(this) + 0xa4);
}

unsigned long Display::GetGlobalAmbientConverted() const
{
    // 0x10003e00 -> state+0xb4
    return *reinterpret_cast<const unsigned long*>(DState(this) + 0xb4);
}

bool Display::GetTextureMode() const
{
    // 0x100031e0 -> state+0x65
    return DState(this)[0x65] != 0;
}

bool Display::GetGlobalTextureMode() const
{
    // 0x10003250 -> +0x4a
    return DRaw(this)[0x4a] != 0;
}

int Display::GetNumStencilBits() const
{
    // 0x10003530 shared body -> +0x4c
    return *reinterpret_cast<const int*>(DRaw(this) + 0x4c);
}

TextureCombinerManager* Display::GetTextureCombinerManager() const
{
    // 0x100041f0 -> +0x228
    return *reinterpret_cast<TextureCombinerManager* const*>(DRaw(this) + 0x228);
}

bool Display::TextureCombinerSupport() const
{
    // 0x10004200 -> +0x212
    return DRaw(this)[0x212] != 0;
}

VertexProgramManager* Display::GetVertexProgramManager() const
{
    // 0x10004210 -> +0x224
    return *reinterpret_cast<VertexProgramManager* const*>(DRaw(this) + 0x224);
}

bool Display::VertexProgramSupport() const
{
    // 0x10004220 -> +0x211
    return DRaw(this)[0x211] != 0;
}

bool Display::HardwareAnimationSupport() const
{
    // 0x10004230 -> +0x210
    return DRaw(this)[0x210] != 0;
}

VertexBufferManager* Display::GetVertexBufferManager() const
{
    // 0x10004240 -> +0x20c
    return *reinterpret_cast<VertexBufferManager* const*>(DRaw(this) + 0x20c);
}

bool Display::CheckPlatform(OS*)
{
    // 0x10009500 shared body
    return false;
}

unsigned int Display::GetCurrentFrame() const
{
    // 0x100219b0 shared body -> +0x3c
    return *reinterpret_cast<const unsigned int*>(DRaw(this) + 0x3c);
}

int Display::GetNumberOfHardwareLights() const
{
    // 0x10046fb0 shared body -> +0x50
    return *reinterpret_cast<const int*>(DRaw(this) + 0x50);
}

bool Display::GetZBiasEnabled()
{
    // 0x10002040 -> +0x68
    return DRaw(this)[0x68] != 0;
}

bool Display::IsBetweenBeginAndEndFrame()
{
    // 0x10002060 -> +0x70
    return DRaw(this)[0x70] != 0;
}

const Display::DisplayFrameStats& Display::GetFrameStats() const
{
    // 0x10004510 -> +0x298
    return *reinterpret_cast<const DisplayFrameStats*>(DRaw(this) + 0x298);
}

const Display::DisplayFrameStats& Display::GetCurrentFrameStats() const
{
    // 0x10004520 -> +0x2c4
    return *reinterpret_cast<const DisplayFrameStats*>(DRaw(this) + 0x2c4);
}

const Size& Display::GetTargetSize() const
{
    // 0x1001b780
    const char* p = *reinterpret_cast<const char* const*>(DRaw(this) + 0x5c);
    if (p)
        return *reinterpret_cast<const Size*>(p + 0x10);
    return *reinterpret_cast<const Size*>(DRaw(this) + 0x28c);
}

int Display::GetMaxTextureCombinerStages()
{
    // 0x10004530 -> +0x214
    return *reinterpret_cast<int*>(DRaw(this) + 0x214);
}

int Display::GetMaxTextureImageUnits()
{
    // 0x10004540 -> +0x218
    return *reinterpret_cast<int*>(DRaw(this) + 0x218);
}

int Display::GetMaxTextureCoordUnits()
{
    // 0x10004550 -> +0x21c
    return *reinterpret_cast<int*>(DRaw(this) + 0x21c);
}

bool Display::GetAlphaToCoverage()
{
    // 0x10004560 -> (state+0xa0) bit 7
    return (DState(this)[0xa0] >> 7) != 0;
}

bool Display::GetOutlinesMode() const
{
    // 0x100042a0 -> +0x71 bit 0
    return (reinterpret_cast<const unsigned char*>(DRaw(this))[0x71] & 1u) != 0;
}

void Display::SetOutlinesMode(bool on)
{
    // 0x100042b0
    unsigned char& b = reinterpret_cast<unsigned char*>(DRaw(this))[0x71];
    b = static_cast<unsigned char>((b & ~1u) | (on ? 1u : 0u));
}

bool Display::GetVertexNormalsMode() const
{
    // 0x100042d0 -> +0x71 bit 1
    return (reinterpret_cast<const unsigned char*>(DRaw(this))[0x71] & 2u) != 0;
}

void Display::SetVertexNormalsMode(bool on)
{
    // 0x100042e0
    unsigned char& b = reinterpret_cast<unsigned char*>(DRaw(this))[0x71];
    b = static_cast<unsigned char>((b & ~2u) | (on ? 2u : 0u));
}

bool Display::GetTriangleNormalsMode() const
{
    // 0x10004300 -> +0x71 bit 2
    return (reinterpret_cast<const unsigned char*>(DRaw(this))[0x71] & 4u) != 0;
}

void Display::SetTriangleNormalsMode(bool on)
{
    // 0x10004310
    unsigned char& b = reinterpret_cast<unsigned char*>(DRaw(this))[0x71];
    b = static_cast<unsigned char>((b & ~4u) | (on ? 4u : 0u));
}

void Display::SetNormalLength(float length)
{
    // 0x10004340 -> +0x74
    *reinterpret_cast<float*>(DRaw(this) + 0x74) = length;
}

bool Display::GetLocalAxesMode() const
{
    // 0x10004350 -> +0x71 bit 3
    return (reinterpret_cast<const unsigned char*>(DRaw(this))[0x71] & 8u) != 0;
}

void Display::SetLocalAxesMode(bool on)
{
    // 0x10004360
    unsigned char& b = reinterpret_cast<unsigned char*>(DRaw(this))[0x71];
    b = static_cast<unsigned char>((b & ~8u) | (on ? 8u : 0u));
}

void Display::SetChunkColorMode(bool on)
{
    // 0x10004380 -> +0x78
    DRaw(this)[0x78] = on ? 1 : 0;
}

void Display::SetChunkVertexCountMode(bool on)
{
    // 0x100043a0 -> +0x79
    DRaw(this)[0x79] = on ? 1 : 0;
}

void Display::SetDynamicGeometryDisplayMode(bool on)
{
    // 0x100043c0 -> +0x7b
    DRaw(this)[0x7b] = on ? 1 : 0;
}

void Display::SetStaticGeometryDisplayMode(bool on)
{
    // 0x100043e0 -> +0x7a
    DRaw(this)[0x7a] = on ? 1 : 0;
}

bool Display::GetSeperateSpecularMode()
{
    // 0x100039e0 -> state+0x74
    return DState(this)[0x74] != 0;
}

bool Display::GetLocalViewSpecularMode() const
{
    // 0x10003a20 -> state+0x75
    return DState(this)[0x75] != 0;
}

}  // namespace Jet
