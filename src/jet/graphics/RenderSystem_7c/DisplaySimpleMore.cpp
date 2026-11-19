// Simple/evidenced Display exports (RenderSystem_7c), batch 2.
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

Window* Display::GetWindow() const { return *reinterpret_cast<Window* const*>(DRaw(this) + 0x40); }
OS* Display::GetOS() const { return *reinterpret_cast<OS* const*>(DRaw(this) + 0x44); }
const Size& Display::GetWindowSize() const { return *reinterpret_cast<const Size*>(DRaw(this) + 0x28c); }
DisplaySelector* Display::GetDisplaySelector() const { return *reinterpret_cast<DisplaySelector* const*>(DRaw(this) + 0x288); }
ScreenMode Display::GetCurrentScreenMode() const { return *reinterpret_cast<const ScreenMode*>(DRaw(this) + 0x22c); }
bool Display::GetScissorMode() const { return DState(this)[0x64] != 0; }
const Rect& Display::GetViewport() const { return *reinterpret_cast<const Rect*>(DState(this) + 0x44); }
const Color& Display::GetClearColor() const { return *reinterpret_cast<const Color*>(DState(this) + 0x2c); }
float Display::GetClearDepth() const { return *reinterpret_cast<const float*>(DState(this) + 0x3c); }
long Display::GetClearStencil() const { return *reinterpret_cast<const long*>(DState(this) + 0x40); }
DCull Display::GetCullMode() const { return *reinterpret_cast<const DCull*>(DState(this) + 0x28); }
DBlendEquation Display::GetAlphaBlendEquation() const { return *reinterpret_cast<const DBlendEquation*>(DState(this) + 0x70); }
DFogMode Display::GetFogMode() const { return *reinterpret_cast<const DFogMode*>(DState(this) + 0x78); }
const Color& Display::GetFogColor() const { return *reinterpret_cast<const Color*>(DState(this) + 0x7c); }
const Color& Display::GetGlobalAmbient() const { return *reinterpret_cast<const Color*>(DState(this) + 0xa4); }
unsigned long Display::GetGlobalAmbientConverted() const { return *reinterpret_cast<const unsigned long*>(DState(this) + 0xb4); }
bool Display::GetTextureMode() const { return DState(this)[0x65] != 0; }
bool Display::GetGlobalTextureMode() const { return DRaw(this)[0x4a] != 0; }
int Display::GetNumStencilBits() const { return *reinterpret_cast<const int*>(DRaw(this) + 0x4c); }
TextureCombinerManager* Display::GetTextureCombinerManager() const { return *reinterpret_cast<TextureCombinerManager* const*>(DRaw(this) + 0x228); }
bool Display::TextureCombinerSupport() const { return DRaw(this)[0x212] != 0; }
VertexProgramManager* Display::GetVertexProgramManager() const { return *reinterpret_cast<VertexProgramManager* const*>(DRaw(this) + 0x224); }
bool Display::VertexProgramSupport() const { return DRaw(this)[0x211] != 0; }
bool Display::HardwareAnimationSupport() const { return DRaw(this)[0x210] != 0; }
VertexBufferManager* Display::GetVertexBufferManager() const { return *reinterpret_cast<VertexBufferManager* const*>(DRaw(this) + 0x20c); }
bool Display::CheckPlatform(OS*) { return false; }
unsigned int Display::GetCurrentFrame() const { return *reinterpret_cast<const unsigned int*>(DRaw(this) + 0x3c); }
int Display::GetNumberOfHardwareLights() const { return *reinterpret_cast<const int*>(DRaw(this) + 0x50); }
bool Display::GetZBiasEnabled() { return DRaw(this)[0x68] != 0; }
bool Display::IsBetweenBeginAndEndFrame() { return DRaw(this)[0x70] != 0; }
const Display::DisplayFrameStats& Display::GetFrameStats() const { return *reinterpret_cast<const DisplayFrameStats*>(DRaw(this) + 0x298); }
const Display::DisplayFrameStats& Display::GetCurrentFrameStats() const { return *reinterpret_cast<const DisplayFrameStats*>(DRaw(this) + 0x2c4); }
const Size& Display::GetTargetSize() const {
    const char* p = *reinterpret_cast<const char* const*>(DRaw(this) + 0x5c);
    if (p) return *reinterpret_cast<const Size*>(p + 0x10);
    return *reinterpret_cast<const Size*>(DRaw(this) + 0x28c);
}
int Display::GetMaxTextureCombinerStages() { return *reinterpret_cast<int*>(DRaw(this) + 0x214); }
int Display::GetMaxTextureImageUnits() { return *reinterpret_cast<int*>(DRaw(this) + 0x218); }
int Display::GetMaxTextureCoordUnits() { return *reinterpret_cast<int*>(DRaw(this) + 0x21c); }
bool Display::GetAlphaToCoverage() { return (DState(this)[0xa0] >> 7) != 0; }
bool Display::GetOutlinesMode() const { return (reinterpret_cast<const unsigned char*>(DRaw(this))[0x71] & 1u) != 0; }
void Display::SetOutlinesMode(bool on) { unsigned char& b = reinterpret_cast<unsigned char*>(DRaw(this))[0x71]; b = static_cast<unsigned char>((b & ~1u) | (on ? 1u : 0u)); }
bool Display::GetVertexNormalsMode() const { return (reinterpret_cast<const unsigned char*>(DRaw(this))[0x71] & 2u) != 0; }
void Display::SetVertexNormalsMode(bool on) { unsigned char& b = reinterpret_cast<unsigned char*>(DRaw(this))[0x71]; b = static_cast<unsigned char>((b & ~2u) | (on ? 2u : 0u)); }
bool Display::GetTriangleNormalsMode() const { return (reinterpret_cast<const unsigned char*>(DRaw(this))[0x71] & 4u) != 0; }
void Display::SetTriangleNormalsMode(bool on) { unsigned char& b = reinterpret_cast<unsigned char*>(DRaw(this))[0x71]; b = static_cast<unsigned char>((b & ~4u) | (on ? 4u : 0u)); }
void Display::SetNormalLength(float length) { *reinterpret_cast<float*>(DRaw(this) + 0x74) = length; }
bool Display::GetLocalAxesMode() const { return (reinterpret_cast<const unsigned char*>(DRaw(this))[0x71] & 8u) != 0; }
void Display::SetLocalAxesMode(bool on) { unsigned char& b = reinterpret_cast<unsigned char*>(DRaw(this))[0x71]; b = static_cast<unsigned char>((b & ~8u) | (on ? 8u : 0u)); }
void Display::SetChunkColorMode(bool on) { DRaw(this)[0x78] = on ? 1 : 0; }
void Display::SetChunkVertexCountMode(bool on) { DRaw(this)[0x79] = on ? 1 : 0; }
void Display::SetDynamicGeometryDisplayMode(bool on) { DRaw(this)[0x7b] = on ? 1 : 0; }
void Display::SetStaticGeometryDisplayMode(bool on) { DRaw(this)[0x7a] = on ? 1 : 0; }
bool Display::GetSeperateSpecularMode() { return DState(this)[0x74] != 0; }
bool Display::GetLocalViewSpecularMode() const { return DState(this)[0x75] != 0; }

}  // namespace Jet
