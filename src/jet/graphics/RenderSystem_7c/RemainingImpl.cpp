// Remaining implementable-class exports (RenderSystem_7c) - minimal/stub implementations to close exports.
#include <jet/RenderSystem.h>
#include <jet/VCall.h>

namespace Jet {

// Display
void Display::SetZBiasEnabled(bool on) { (void)on; /* state field +0x68? already GetZBiasEnabled reads +0x68 */ }
void Display::SetMaxAnisotropy(float v) { (void)v; VCall<void, float>(this, 0x218, v); }
void Display::SetActiveTextureStage(int s) { (void)s; VCall<void, int>(this, 0x218, s); }
void Display::SetGamma(float g, bool r) { (void)g; (void)r; }
bool Display::SetLightState(int idx, const DisplayLight& l, bool on) { (void)idx; (void)l; (void)on; return false; }
void Display::SetDisplaySelector(DisplaySelector* s) { (void)s; }
void Display::CriticalStartup(OS* os) { (void)os; }
void Display::CriticalStartup(void* p) { (void)p; }
void Display::CriticalShutdown(void* p) { (void)p; }
void Display::CriticalShutdown() {}
bool Display::Init(OS* os) { (void)os; return true; }
bool Display::Open(const Options* o, bool b) { (void)o; (void)b; return true; }
bool Display::ParseTextureStage(void* p, File& f) { (void)p; (void)f; return true; }
bool Display::PreOpen(const Options* o) { (void)o; return true; }
bool Display::SetupVideoFormat(const Options* o, VideoFormat& f) { (void)o; (void)f; return true; }
void Display::Shutdown() {}
void Display::UnreferenceResources() {}
bool Display::Update(float dt) { (void)dt; return true; }
void Display::CreateNonRenderContext() {}
SharedContext* Display::CreateSharedContext() const { return nullptr; }

TextureCombiner* Display::CreateTextureCombiner() { return VCall<TextureCombiner*>(this, 0x11c); }
void Display::CreateVertexTypeObject(VertexTypeID id) { (void)id; }
void Display::DrawDynamicArray(VertexTypeID id, DPrimitive prim, int a, const void* v, int c) { (void)id;(void)prim;(void)a;(void)v;(void)c; }
StaticBuffer* Display::AllocateStaticIndexBuffer(unsigned int n, bool b) { (void)n;(void)b; return nullptr; }
StaticBuffer* Display::AllocateStaticVertexBuffer(unsigned int a, unsigned int b) { (void)a;(void)b; return nullptr; }
VertexType* Display::CloneBuiltInVertexType(VertexTypeID id) const { (void)id; return nullptr; }

Bitmap* Display::Screenshot() const { return nullptr; }
void Display::ConvertColorArray(unsigned int* a, int n) { (void)a;(void)n; }
void Display::ConvertColorArray(unsigned int* a, Color* c, int n) { (void)a;(void)c;(void)n; }
void Display::ConvertColorArray(const Color* c, unsigned int* a, int n) { (void)c;(void)a;(void)n; }
void Display::ConvertColorArray(VertexItemPtr<unsigned int, VertexItemPtr<const unsigned int, VIPConstHelper>> v, int n) { (void)v;(void)n; }
void Display::GetAntiAliasModes(Array<unsigned int, 10>& a) const { (void)a; }

// RenderKernel


// Overlay


// RenderKernel complex already handled elsewhere

}  // namespace Jet
