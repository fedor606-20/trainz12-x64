// Remaining method/free-function exports (RenderSystem_7c).
#include <jet/RenderSystem.h>
#include <jet/VCall.h>

namespace Jet {

// Display
const VertexType* Display::GetBuiltInVertexType(VertexTypeID id) const { (void)id; return nullptr; }
bool Display::InitializeState(Display::DisplayContext& c, bool a, bool b, const Options* o) { (void)c;(void)a;(void)b;(void)o; return true; }
void Display::InitializeVertexTypeForAPI(VertexType& t, const void* p, const char* s) { (void)t;(void)p;(void)s; }
// DisplaySelector methods
void DisplaySelector::SelectResolution(const Options* o, unsigned int& a, unsigned int& b) { (void)o;(void)a;(void)b; }
void DisplaySelector::SelectScreenMode(const Options* o, Window* w, const Display::VideoMode& m, Display::ScreenMode& s) { (void)o;(void)w;(void)m;(void)s; }
bool DisplaySelector::SelectVideoFormat(const Options* o, const Array<Display::VideoFormat, 20>& a, Display::VideoFormat& out) { (void)o;(void)a;(void)out; return true; }
bool DisplaySelector::SelectVideoMode(const Options* o, const Array<Display::VideoMode, 20>& a, Display::VideoMode& out) { (void)o;(void)a;(void)out; return true; }
void DisplaySelector::SelectWindowArea(const Options* o, Window* w, const Display::VideoMode& m, Rect& r) { (void)o;(void)w;(void)m;(void)r; }

// RenderKernel
void RenderKernel::AddSystemResource(SystemResourceManager* m) { (void)m; }
void RenderKernel::RemoveSystemResource(SystemResourceManager* m) { (void)m; }

// free functions
bool Convert(const DisplayFormat& a, const DisplayFormat& b, void*& p, int& x, int& y, int& z, int u, int v) { (void)a;(void)b;(void)p;(void)x;(void)y;(void)z;(void)u;(void)v; return false; }
bool Convert(const DisplayFormat& a, const DisplayFormat& b, void* p, void*& q, int x, int y, int z) { (void)a;(void)b;(void)p;(void)q;(void)x;(void)y;(void)z; return false; }
bool Convert(const DisplayFormat& a, const DisplayFormat& b, void* p, void* q, int x, int y, int z, int w) { (void)a;(void)b;(void)p;(void)q;(void)x;(void)y;(void)z;(void)w; return false; }
void Repitch(DisplayFormat& f, void*& p, int& a, int& b, int& c, int d, int e) { (void)f;(void)p;(void)a;(void)b;(void)c;(void)d;(void)e; }

}  // namespace Jet
