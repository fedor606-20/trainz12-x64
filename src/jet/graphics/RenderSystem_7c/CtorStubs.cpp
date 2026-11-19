// Ctor/dtor/assign/scalar/Update stubs (RenderSystem_7c).
#include <jet/RenderSystem.h>

namespace Jet {

// ClearLayer
ClearLayer::ClearLayer() {}
ClearLayer::ClearLayer(const ClearLayer&) {}
ClearLayer::~ClearLayer() {}
ClearLayer& ClearLayer::operator=(const ClearLayer& o) { (void)o; return *this; }

// Display
Display::Display(bool) {}
Display::~Display() {}
// DisplayContext
Display::DisplayContext::DisplayContext() {}
Display::DisplayContext::DisplayContext(const Display::DisplayContext&) {}
Display::DisplayContext& Display::DisplayContext::operator=(const Display::DisplayContext& o) { (void)o; return *this; }
Display::DisplayContext::~DisplayContext() {}
// DisplaySaveBuffer
DisplaySaveBuffer::DisplaySaveBuffer(unsigned int) {}
DisplaySaveBuffer::~DisplaySaveBuffer() {}
// DisplaySelector
DisplaySelector::DisplaySelector() {}
DisplaySelector::~DisplaySelector() {}

// Layer
Layer::Layer(const Layer&) {}
Layer::Layer(const PString&, int, bool) {}
Layer::Layer(LayerGroup*, const PString&, int, bool) {}
Layer::~Layer() {}
Layer& Layer::operator=(const Layer& o) { (void)o; return *this; }

// LayerGroup
LayerGroup::LayerGroup(const LayerGroup&) {}
LayerGroup::LayerGroup(LayerGroup*, const PString&, int, bool) {}
LayerGroup::~LayerGroup() {}
LayerGroup& LayerGroup::operator=(const LayerGroup& o) { (void)o; return *this; }

// LayerManager
LayerManager::LayerManager(const LayerManager&) {}
LayerManager::~LayerManager() {}
LayerManager& LayerManager::operator=(const LayerManager& o) { (void)o; return *this; }

// NonRenderContext
NonRenderContext::NonRenderContext() {}
NonRenderContext::~NonRenderContext() {}

// Overlay
Overlay::Overlay(const Overlay& o) : Layer(o) {}
Overlay::Overlay(const PString& n, int p, bool f) : Layer(n, p, f) {}
Overlay::Overlay(LayerGroup* g, const PString& n, int p, bool f) : Layer(g, n, p, f) {}
Overlay::~Overlay() {}
Overlay& Overlay::operator=(const Overlay& o) { (void)o; return *this; }

// RenderKernel
RenderKernel::RenderKernel(SceneGraph*, float) {}
RenderKernel::~RenderKernel() {}

// SceneGraph
SceneGraph::SceneGraph(const SceneGraph&) {}
SceneGraph::SceneGraph(ModifierUpdateTask*) {}
SceneGraph& SceneGraph::operator=(const SceneGraph& o) { (void)o; return *this; }

// SharedContext
SharedContext::SharedContext() {}
SharedContext::~SharedContext() {}

// SystemResourceManager
SystemResourceManager::SystemResourceManager() {}
SystemResourceManager::SystemResourceManager(const SystemResourceManager&) {}
SystemResourceManager::~SystemResourceManager() {}

// SystemResourceViewer
SystemResourceViewer::SystemResourceViewer() {}
SystemResourceViewer::SystemResourceViewer(const SystemResourceViewer&) {}
SystemResourceViewer::SystemResourceViewer(RenderKernel*, const PString&) {}
SystemResourceViewer::~SystemResourceViewer() {}
SystemResourceViewer& SystemResourceViewer::operator=(const SystemResourceViewer& o) { (void)o; return *this; }

// Minimal classes





TextureRes::TextureRes() {}
TextureRes::TextureRes(const TextureRes&) {}
TextureRes::~TextureRes() {}
TextureRes& TextureRes::operator=(const TextureRes& o) { (void)o; return *this; }





// scalar deleting destructors (??_F) - empty
// (not needed as separate; compiler generates)

}  // namespace Jet
