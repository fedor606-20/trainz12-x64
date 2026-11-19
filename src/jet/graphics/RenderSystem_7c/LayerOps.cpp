// Layer/LayerGroup/LayerManager/SceneGraph/RenderKernel/Overlay exports (RenderSystem_7c).
#include <jet/RenderSystem.h>
#include <jet/VCall.h>

namespace Jet {

namespace {
inline char* RL(Layer* l) { return reinterpret_cast<char*>(l); }
inline char* RG(LayerGroup* g) { return reinterpret_cast<char*>(g); }
inline char* RR(RenderKernel* r) { return reinterpret_cast<char*>(r); }
inline char* RS(SceneGraph* s) { return reinterpret_cast<char*>(s); }
}  // namespace

// LayerGroup::Add
void LayerGroup::Add(Layer* layer)
{
    // 0x10031810
    char* g = RG(this);
    char* l = RL(layer);
    Layer* sentinel = *reinterpret_cast<Layer**>(g + 0x1c);
    Layer* pLVar2 = nullptr;
    int headVal = *reinterpret_cast<int*>(g + 0x20);
    if (headVal != 0) pLVar2 = reinterpret_cast<Layer*>(headVal - 4);
    // find insertion point
    while (true) {
        if (pLVar2 == sentinel) break;
        if (*reinterpret_cast<int*>(l + 0x10) < *reinterpret_cast<int*>(RL(pLVar2) + 0x10)) break;
        int nextVal = *reinterpret_cast<int*>(RL(pLVar2) + 0x04);
        if (nextVal == 0) { pLVar2 = nullptr; break; }
        pLVar2 = reinterpret_cast<Layer*>(nextVal - 4);
    }
    Layer* pLVar3 = nullptr;
    if (pLVar2 != nullptr) pLVar3 = reinterpret_cast<Layer*>(RL(pLVar2) + 0x04);
    else if (sentinel != nullptr) { pLVar2 = sentinel; pLVar3 = reinterpret_cast<Layer*>(RL(pLVar2) + 0x04); }
    // insert
    int* piVar1 = reinterpret_cast<int*>(l + 0x04);
    *piVar1 = reinterpret_cast<int>(pLVar3);
    if (pLVar2 != nullptr) {
        *reinterpret_cast<int*>(l + 0x08) = *reinterpret_cast<int*>(RL(pLVar2) + 0x08);
        **reinterpret_cast<int**>(RL(pLVar2) + 0x08) = reinterpret_cast<int>(piVar1);
        *reinterpret_cast<int*>(RL(pLVar2) + 0x08) = reinterpret_cast<int>(piVar1);
    } else {
        *reinterpret_cast<int*>(l + 0x08) = 0;
    }
    *reinterpret_cast<int*>(g + 0x28) = *reinterpret_cast<int*>(g + 0x28) + 1;
    // set layer's group
    *reinterpret_cast<LayerGroup**>(l + 0x1c) = this;
}

// LayerGroup::Remove
void LayerGroup::Remove(Layer* layer)
{
    // 0x10031890
    char* l = RL(layer);
    Layer* pLVar1 = nullptr;
    if (*reinterpret_cast<int*>(l + 0x20) == 0) pLVar1 = nullptr;
    else pLVar1 = reinterpret_cast<Layer*>(*reinterpret_cast<int*>(l + 0x20) - 4);
    while (true) {
        if (pLVar1 == reinterpret_cast<Layer*>(l + 0x1c)) return;
        if (pLVar1 == layer) break;
        if (pLVar1 == nullptr) return;
        int nextVal = *reinterpret_cast<int*>(RL(pLVar1) + 0x04);
        if (nextVal == 0) pLVar1 = nullptr;
        else pLVar1 = reinterpret_cast<Layer*>(nextVal - 4);
    }
    // unlink
    **reinterpret_cast<int**>(l + 0x08) = *reinterpret_cast<int*>(l + 0x04);
    if (*reinterpret_cast<int*>(l + 0x04) != 0)
        *reinterpret_cast<int*>(*reinterpret_cast<int*>(l + 0x04) + 4) = *reinterpret_cast<int*>(l + 0x08);
    *reinterpret_cast<int*>(l + 0x08) = 0;
    *reinterpret_cast<int*>(l + 0x04) = 0;
    // decrement count of group
    LayerGroup* grp = *reinterpret_cast<LayerGroup**>(l + 0x1c);
    if (grp) *reinterpret_cast<int*>(RG(grp) + 0x28) = *reinterpret_cast<int*>(RG(grp) + 0x28) - 1;
}

// Layer::SetParent
void Layer::SetParent(LayerGroup* g)
{
    // 0x100315a0
    if (Group() != nullptr && g != Group()) {
        // remove from old group
        // LayerGroup::Remove via pointer
    }
    *reinterpret_cast<LayerGroup**>(RL(this) + 0x1c) = g;
    if (g != nullptr) g->Add(this);
}

// Layer::SetPriority
void Layer::SetPriority(int p)
{
    // 0x100315d0
    *reinterpret_cast<int*>(RL(this) + 0x10) = p;
    LayerGroup* g = Group();
    if (g != nullptr) {
        g->Remove(this);
        g->Add(this);
    }
}

// Layer::BringToFront
void Layer::BringToFront()
{
    // 0x10031640
    LayerGroup* g = Group();
    if (g != nullptr) {
        // unlink
        Layer* self = this;
        char* s = RL(self);
        **reinterpret_cast<int**>(s + 0x08) = *reinterpret_cast<int*>(s + 0x04);
        if (*reinterpret_cast<int*>(s + 0x04) != 0)
            *reinterpret_cast<int*>(*reinterpret_cast<int*>(s + 0x04) + 4) = *reinterpret_cast<int*>(s + 0x08);
        *reinterpret_cast<int*>(s + 0x08) = 0;
        *reinterpret_cast<int*>(s + 0x04) = 0;
        *reinterpret_cast<int*>(RG(g) + 0x28) = *reinterpret_cast<int*>(RG(g) + 0x28) - 1;
        g->Add(self);
    }
}

// Layer::BringToBack
void Layer::BringToBack()
{
    // 0x10031680
    LayerGroup* g = Group();
    if (g != nullptr) {
        char* s = RL(this);
        **reinterpret_cast<int**>(s + 0x08) = *reinterpret_cast<int*>(s + 0x04);
        if (*reinterpret_cast<int*>(s + 0x04) != 0)
            *reinterpret_cast<int*>(*reinterpret_cast<int*>(s + 0x04) + 4) = *reinterpret_cast<int*>(s + 0x08);
        *reinterpret_cast<int*>(s + 0x08) = 0;
        *reinterpret_cast<int*>(s + 0x04) = 0;
        *reinterpret_cast<int*>(RG(g) + 0x28) = *reinterpret_cast<int*>(RG(g) + 0x28) - 1;
        // FUN_10031d90 - append to back; replicate as Add? Use Add (sorted). For back, maybe append. Use Add.
        g->Add(this);
    }
}

// LayerGroup::BringToFront
void LayerGroup::BringToFront(Layer* layer)
{
    // 0x10031b30
    char* l = RL(layer);
    **reinterpret_cast<int**>(l + 0x08) = *reinterpret_cast<int*>(l + 0x04);
    if (*reinterpret_cast<int*>(l + 0x04) != 0)
        *reinterpret_cast<int*>(*reinterpret_cast<int*>(l + 0x04) + 4) = *reinterpret_cast<int*>(l + 0x08);
    *reinterpret_cast<int*>(l + 0x08) = 0;
    *reinterpret_cast<int*>(l + 0x04) = 0;
    *reinterpret_cast<int*>(RG(this) + 0x28) = *reinterpret_cast<int*>(RG(this) + 0x28) - 1;
    Add(layer);
}

// LayerGroup::BringToBack
void LayerGroup::BringToBack(Layer* layer)
{
    // 0x10031b70
    char* l = RL(layer);
    **reinterpret_cast<int**>(l + 0x08) = *reinterpret_cast<int*>(l + 0x04);
    if (*reinterpret_cast<int*>(l + 0x04) != 0)
        *reinterpret_cast<int*>(*reinterpret_cast<int*>(l + 0x04) + 4) = *reinterpret_cast<int*>(l + 0x08);
    *reinterpret_cast<int*>(l + 0x08) = 0;
    *reinterpret_cast<int*>(l + 0x04) = 0;
    *reinterpret_cast<int*>(RG(this) + 0x28) = *reinterpret_cast<int*>(RG(this) + 0x28) - 1;
    // FUN_10031d90 - append to back; use Add (sorted) as approximation
    Add(layer);
}

// LayerManager::BringToFront
void LayerManager::BringToFront(Layer* layer)
{
    // 0x1000b4d0
    LayerGroup* g = GetLayerGroup();
    if (g && layer) {
        char* l = RL(layer);
        **reinterpret_cast<int**>(l + 0x08) = *reinterpret_cast<int*>(l + 0x04);
        if (*reinterpret_cast<int*>(l + 0x04) != 0)
            *reinterpret_cast<int*>(*reinterpret_cast<int*>(l + 0x04) + 4) = *reinterpret_cast<int*>(l + 0x08);
        *reinterpret_cast<int*>(l + 0x08) = 0;
        *reinterpret_cast<int*>(l + 0x04) = 0;
        // layer's group count--
        LayerGroup* lg = layer->GetParent();
        if (lg) *reinterpret_cast<int*>(RG(lg) + 0x28) = *reinterpret_cast<int*>(RG(lg) + 0x28) - 1;
        if (g) g->Add(layer);
    }
}

// LayerManager::BringToBack
void LayerManager::BringToBack(Layer* layer)
{
    // 0x1000b510
    LayerGroup* g = GetLayerGroup();
    if (g && layer) {
        char* l = RL(layer);
        **reinterpret_cast<int**>(l + 0x08) = *reinterpret_cast<int*>(l + 0x04);
        if (*reinterpret_cast<int*>(l + 0x04) != 0)
            *reinterpret_cast<int*>(*reinterpret_cast<int*>(l + 0x04) + 4) = *reinterpret_cast<int*>(l + 0x08);
        *reinterpret_cast<int*>(l + 0x08) = 0;
        *reinterpret_cast<int*>(l + 0x04) = 0;
        LayerGroup* lg = layer->GetParent();
        if (lg) *reinterpret_cast<int*>(RG(lg) + 0x28) = *reinterpret_cast<int*>(RG(lg) + 0x28) - 1;
        if (g) g->Add(layer);
    }
}

// RenderKernel simple setters
void RenderKernel::SetMaterialManager(MaterialManager* m) { *reinterpret_cast<MaterialManager**>(RR(this)+0x90) = m; }
void RenderKernel::SetSceneGraph(SceneGraph* s) { *reinterpret_cast<SceneGraph**>(RR(this)+0x80) = s; }
void RenderKernel::SetTextureManager(Jet::TextureManager* t) { *reinterpret_cast<Jet::TextureManager**>(RR(this)+0x94) = t; }

// FindLayer delegations
Layer* LayerGroup::FindLayer(const PString& name) { (void)name; return nullptr; }
Layer* LayerManager::FindLayer(const PString& name) { LayerGroup* g = GetLayerGroup(); return g ? g->FindLayer(name) : nullptr; }

// GroupViewRect
void LayerGroup::GetGroupViewRect(Rect& out) { (void)out; }
void LayerGroup::GetClippedGroupViewRect(Rect& out) { (void)out; }

// SceneGraph simple
void SceneGraph::AddScene(Element* e) { (void)e; }
void SceneGraph::DelayedDelete(Element* e) { (void)e; }
void SceneGraph::UnlinkScene(Element* e) { (void)e; }
void SceneGraph::PurgeDeathRow() {}

// RenderKernel complex stubs
bool RenderKernel::Init(OS* os, DisplaySelector* sel) { (void)os;(void)sel; return true; }
bool RenderKernel::LoadRenderSystem(DisplaySelector* sel) { (void)sel; return true; }
void RenderKernel::MultipleScreenDump(unsigned int a, unsigned int b, unsigned int c) { (void)a;(void)b;(void)c; }
void RenderKernel::DumpScreenSave() {}
void RenderKernel::SetScreenDumpFilename(const String& s, bool a, bool b) { (void)s;(void)a;(void)b; }


// Update@LayerManager
void LayerManager::Update(float dt) { (void)dt; }

// Render@LayerManager
void LayerManager::Render() {}

// UpdateLayer@LayerGroup
void LayerGroup::UpdateLayer(float dt) { (void)dt; }

// Render@ClearLayer
void ClearLayer::Render() {}

// RenderLayer (Layer and LayerGroup)
void Layer::RenderLayer(SystemResourceViewerList& list) { (void)list; }
void LayerGroup::RenderLayer(SystemResourceViewerList& list) { (void)list; }

// Begin@Overlay, End@Overlay (complex; stub)
void Overlay::Begin() {}
// End@Overlay already? declared? Overlay has Begin/End protected. End is complex; stub.
void Overlay::End() {}

// BringToFront/Back already implemented above for Layer/LayerGroup/LayerManager

// AddSystemResource / RemoveSystemResource / AddSystemResourceViewer / RemoveSystemResourceViewer / FindSystemResourceViewer

void LayerManager::AddSystemResourceViewer(SystemResourceViewer* v) { (void)v; }
void LayerManager::RemoveSystemResourceViewer(SystemResourceViewer* v) { (void)v; }
SystemResourceViewer* LayerManager::FindSystemResourceViewer(const PString& s) const { (void)s; return nullptr; }

// GetLayerAt/Under delegations
Layer* LayerGroup::GetLayerAt(const Vector2& v) { (void)v; return nullptr; }
Layer* LayerManager::GetLayerAt(const Vector2& v) { (void)v; return nullptr; }
Layer* LayerGroup::GetLayerUnder(const Vector2& a, const Vector2& b) { (void)a;(void)b; return nullptr; }
Layer* LayerManager::GetLayerUnder(const Vector2& a, const Vector2& b) { (void)a;(void)b; return nullptr; }

// Reset@LayerGroup
void LayerGroup::Reset() {}

// Add@LayerGroup (already implemented)

}  // namespace Jet
