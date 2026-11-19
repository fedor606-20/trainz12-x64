// Simple/evidenced Display exports batch 6 (RenderSystem_7c).
#include <jet/RenderSystem.h>
#include <jet/VCall.h>

namespace Jet {

namespace {
inline char* D(Display* d) { return reinterpret_cast<char*>(d); }
inline unsigned char* St(Display* d) { return *reinterpret_cast<unsigned char**>(D(d) + 0x54); }
}  // namespace

bool Display::GetZBiasEnabled() { return D(this)[0x68] != 0; }
bool Display::GetAlphaBlendMode() { unsigned char* st = St(this); return (st[0xa1] & 1) != 0; }
bool Display::GetAlphaTestMode() { unsigned char* st = St(this); return ((st[0xa0] >> 5) & 1) != 0; }
bool Display::GetDepthTestMode() { unsigned char* st = St(this); return ((st[0xa0] >> 2) & 1) != 0; }
bool Display::GetDepthWriteMode() { unsigned char* st = St(this); return ((st[0xa0] >> 1) & 1) != 0; }
bool Display::GetAlphaToCoverage() { return (St(this)[0xa0] >> 7) != 0; }
bool Display::GetStencilTestMode() { unsigned char* st = St(this); return ((st[0xa0] >> 3) & 1) != 0; }
bool Display::GetVertexWeightMode() { unsigned char* st = St(this); return ((st[0xa1] >> 1) & 1) != 0; }
bool Display::GetSeperateSpecularMode() { return St(this)[0x74] != 0; }
bool Display::GetLocalViewSpecularMode() const { return St(this)[0x75] != 0; }
bool Display::GetDynamicGeometryDisplayMode() const { return D(this)[0x7b] != 0; }
bool Display::GetStaticGeometryDisplayMode() const { return D(this)[0x7a] != 0; }
bool Display::GetChunkVertexCountMode() const { return D(this)[0x79] != 0; }
bool Display::GetChunkColorMode() const { return D(this)[0x78] != 0; }
bool Display::GetOutlinesMode() const { return (reinterpret_cast<unsigned char*>(D(this))[0x71] & 1u) != 0; }
bool Display::GetVertexNormalsMode() const { return (reinterpret_cast<unsigned char*>(D(this))[0x71] & 2u) != 0; }
bool Display::GetTriangleNormalsMode() const { return (reinterpret_cast<unsigned char*>(D(this))[0x71] & 4u) != 0; }
bool Display::GetLocalAxesMode() const { return (reinterpret_cast<unsigned char*>(D(this))[0x71] & 8u) != 0; }
bool Display::GetReflectionCull() const { return D(this)[0x7c] != 0; }
bool Display::IsRenderableState() const { return D(this)[0x49] != 0; }
bool Display::HardwareAnimationSupport() const { return D(this)[0x210] != 0; }
bool Display::VertexProgramSupport() const { return D(this)[0x211] != 0; }
bool Display::TextureCombinerSupport() const { return D(this)[0x212] != 0; }
bool Display::GetGlobalTextureMode() const { return D(this)[0x4a] != 0; }
bool Display::GetTextureMode() const { return St(this)[0x65] != 0; }
bool Display::IsBetweenBeginAndEndFrame() { return D(this)[0x70] != 0; }
bool Display::SupportsAntiAliasing() const { return (St(this)[0xa1] & 0x10) != 0; }
bool Display::GetAntiAliasing() const { return (St(this)[0xa1] >> 3) & 1; }
float Display::GetPhysicalScreenRatio() { return *reinterpret_cast<float*>(D(this) + 0x6c); }
float Display::GetNormalLength() const { return *reinterpret_cast<const float*>(D(this) + 0x74); }
float Display::GetClearDepth() const { return *reinterpret_cast<const float*>(St(this) + 0x3c); }
float Display::GetFogDensity() const { return *reinterpret_cast<const float*>(St(this) + 0x8c); }
float Display::GetAlphaTestReference() const { return *reinterpret_cast<const float*>(St(this) + 0xcc); }
long Display::GetClearStencil() const { return *reinterpret_cast<const long*>(St(this) + 0x40); }
unsigned long Display::GetColorWriteMask() const { return *reinterpret_cast<const unsigned long*>(St(this) + 0x9c); }
unsigned long Display::GetGlobalAmbientConverted() const { return *reinterpret_cast<const unsigned long*>(St(this) + 0xb4); }
unsigned long Display::GetStencilWriteMask(DFace face) const { return *reinterpret_cast<const unsigned long*>(St(this) + 0xe4 + static_cast<int>(face) * 0x1c); }
DCull Display::GetCullMode() const { return *reinterpret_cast<const DCull*>(St(this) + 0x28); }
DBlendEquation Display::GetAlphaBlendEquation() const { return *reinterpret_cast<const DBlendEquation*>(St(this) + 0x70); }
DFogMode Display::GetFogMode() const { return *reinterpret_cast<const DFogMode*>(St(this) + 0x78); }
DFunction Display::GetDepthFunction() const { return *reinterpret_cast<const DFunction*>(St(this) + 0xd4); }
DFunction Display::GetAlphaTestFunction() const { return static_cast<DFunction>(*reinterpret_cast<const float*>(St(this) + 0xd0)); }
DColorMaterialTypes Display::GetColorMaterialMode() { return static_cast<DColorMaterialTypes>(*reinterpret_cast<const int*>(St(this) + 0x98)); }
int Display::GetNumStencilBits() const { return *reinterpret_cast<const int*>(D(this) + 0x4c); }
int Display::GetMaxTextureCombinerStages() { return *reinterpret_cast<int*>(D(this) + 0x214); }
int Display::GetMaxTextureImageUnits() { return *reinterpret_cast<int*>(D(this) + 0x218); }
int Display::GetMaxTextureCoordUnits() { return *reinterpret_cast<int*>(D(this) + 0x21c); }
int Display::GetNumberOfHardwareLights() const { return *reinterpret_cast<const int*>(D(this) + 0x50); }
unsigned int Display::GetCurrentFrame() const { return *reinterpret_cast<const unsigned int*>(D(this) + 0x3c); }
Window* Display::GetWindow() const { return *reinterpret_cast<Window* const*>(D(this) + 0x40); }
OS* Display::GetOS() const { return *reinterpret_cast<OS* const*>(D(this) + 0x44); }
const Size& Display::GetWindowSize() const { return *reinterpret_cast<const Size*>(D(this) + 0x28c); }
DisplaySelector* Display::GetDisplaySelector() const { return *reinterpret_cast<DisplaySelector* const*>(D(this) + 0x288); }
Display::ScreenMode Display::GetCurrentScreenMode() const { return *reinterpret_cast<const ScreenMode*>(D(this) + 0x22c); }
const Rect& Display::GetViewport() const { return *reinterpret_cast<const Rect*>(St(this) + 0x44); }
const Color& Display::GetClearColor() const { return *reinterpret_cast<const Color*>(St(this) + 0x2c); }
const Color& Display::GetFogColor() const { return *reinterpret_cast<const Color*>(St(this) + 0x7c); }
const Color& Display::GetGlobalAmbient() const { return *reinterpret_cast<const Color*>(St(this) + 0xa4); }
TextureCombinerManager* Display::GetTextureCombinerManager() const { return *reinterpret_cast<TextureCombinerManager* const*>(D(this) + 0x228); }
VertexProgramManager* Display::GetVertexProgramManager() const { return *reinterpret_cast<VertexProgramManager* const*>(D(this) + 0x224); }
VertexBufferManager* Display::GetVertexBufferManager() const { return *reinterpret_cast<VertexBufferManager* const*>(D(this) + 0x20c); }
bool Display::CheckPlatform(OS*) { return false; }
const Display::DisplayFrameStats& Display::GetFrameStats() const { return *reinterpret_cast<const DisplayFrameStats*>(D(this) + 0x298); }
const Display::DisplayFrameStats& Display::GetCurrentFrameStats() const { return *reinterpret_cast<const DisplayFrameStats*>(D(this) + 0x2c4); }
const Size& Display::GetTargetSize() const {
    const char* p = *reinterpret_cast<const char* const*>(D(this) + 0x5c);
    if (p) return *reinterpret_cast<const Size*>(p + 0x10);
    return *reinterpret_cast<const Size*>(D(this) + 0x28c);
}
bool Display::GetAlphaToCoverage() { return (St(this)[0xa0] >> 7) != 0; }
bool Display::TextureCombinerSupport() const { return D(this)[0x212] != 0; }
bool Display::VertexProgramSupport() const { return D(this)[0x211] != 0; }
bool Display::HardwareAnimationSupport() const { return D(this)[0x210] != 0; }
bool Display::GetOutlinesMode() const { return (reinterpret_cast<const unsigned char*>(D(this))[0x71] & 1u) != 0; }
bool Display::GetVertexNormalsMode() const { return (reinterpret_cast<const unsigned char*>(D(this))[0x71] & 2u) != 0; }
bool Display::GetTriangleNormalsMode() const { return (reinterpret_cast<const unsigned char*>(D(this))[0x71] & 4u) != 0; }
bool Display::GetLocalAxesMode() const { return (reinterpret_cast<const unsigned char*>(D(this))[0x71] & 8u) != 0; }
bool Display::GetChunkColorMode() const { return D(this)[0x78] != 0; }
bool Display::GetChunkVertexCountMode() const { return D(this)[0x79] != 0; }
bool Display::GetDynamicGeometryDisplayMode() const { return D(this)[0x7b] != 0; }
bool Display::GetStaticGeometryDisplayMode() const { return D(this)[0x7a] != 0; }
bool Display::GetReflectionCull() const { return D(this)[0x7c] != 0; }
bool Display::IsRenderableState() const { return D(this)[0x49] != 0; }
float Display::GetPhysicalScreenRatio() { return *reinterpret_cast<float*>(D(this) + 0x6c); }
float Display::GetNormalLength() const { return *reinterpret_cast<const float*>(D(this) + 0x74); }
float Display::GetClearDepth() const { return *reinterpret_cast<const float*>(St(this) + 0x3c); }
float Display::GetClearStencil() const { return 0; }  // placeholder
int Display::GetNumStencilBits() const { return *reinterpret_cast<const int*>(D(this) + 0x4c); }
int Display::GetMaxTextureCombinerStages() { return *reinterpret_cast<int*>(D(this) + 0x214); }
int Display::GetMaxTextureImageUnits() { return *reinterpret_cast<int*>(D(this) + 0x218); }
int Display::GetMaxTextureCoordUnits() { return *reinterpret_cast<int*>(D(this) + 0x21c); }
unsigned int Display::GetCurrentFrame() const { return *reinterpret_cast<const unsigned int*>(D(this) + 0x3c); }
int Display::GetNumberOfHardwareLights() const { return *reinterpret_cast<const int*>(D(this) + 0x50); }
bool Display::IsBetweenBeginAndEndFrame() { return D(this)[0x70] != 0; }
bool Display::SupportsAntiAliasing() const { return (St(this)[0xa1] & 0x10) != 0; }
bool Display::GetAntiAliasing() const { return (St(this)[0xa1] >> 3) & 1; }
bool Display::GetSeperateSpecularMode() { return St(this)[0x74] != 0; }
bool Display::GetLocalViewSpecularMode() const { return St(this)[0x75] != 0; }
bool Display::GetGlobalAmbientConverted() const { return *reinterpret_cast<const unsigned long*>(St(this) + 0xb4); }

}  // namespace Jet
