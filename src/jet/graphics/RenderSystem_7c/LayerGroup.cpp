// U8: shared x86 Layer/LayerGroup intrusive list and traversal. Lifetime still uses the original.
#include <jet/RenderSystem.h>
#include <jet/Modifier.h>

#include <windows.h>

#include <cstddef>

extern void* RsLayerManagerVftable[2];
extern "C" void RsMirrorSync(const char* name);  // U11: keep exported data mirrors in step

extern unsigned char RsRsD019[32];
extern unsigned char RsRsD020[32];

namespace Jet {

int __cdecl Layer::operator<(const Layer& other) const { return priority < other.priority; }
const PString& __cdecl Layer::GetName() const { return name; }
LayerGroup* __cdecl Layer::GetParent() const { return group; }
RenderKernel* __cdecl Layer::GetRenderKernel() const { return kernel; }
void __cdecl Layer::SetRenderKernel(RenderKernel* value) { kernel = value; }

void __cdecl Layer::SetParent(LayerGroup* parent)
{
    if (group) group->Remove(this);
    group = parent;
    if (group) group->Add(this);
}

void __cdecl Layer::SetPriority(int value)
{
    priority = value;
    if (group) {
        group->Remove(this);
        group->Add(this);
    }
}

void __cdecl Layer::BringToFront() { if (group) group->BringToFront(this); }
void __cdecl Layer::BringToBack() { if (group) group->BringToBack(this); }
void __cdecl Layer::ConvertToWindow(Point& point) const { if (group) group->ConvertToWindow(point); }
void __cdecl Layer::ConvertToWindowAndClip(Rect& rect) const { if (group) group->ConvertToWindowAndClip(rect); }

Layer* LayerGroup::FromLink(Detail::LayerLink* link)
{
    return reinterpret_cast<Layer*>(reinterpret_cast<unsigned char*>(link) - offsetof(Layer, link));
}

void LayerGroup::Insert(Layer* layer, bool beforeEqual)
{
    auto* pos = sentinel.next;
    while (pos != &sentinel) {
        const int current = FromLink(pos)->priority;
        if (layer->priority < current || (beforeEqual && layer->priority == current)) break;
        pos = pos->next;
    }
    layer->link = {pos, pos->prev};
    pos->prev->next = &layer->link;
    pos->prev = &layer->link;
    ++count;
}

void LayerGroup::Unlink(Layer* layer)
{
    layer->link.prev->next = layer->link.next;
    layer->link.next->prev = layer->link.prev;
    layer->link = {};
    --count;
}

void __cdecl LayerGroup::Add(Layer* layer) { Insert(layer, false); }

void __cdecl LayerGroup::Remove(Layer* layer)
{
    for (auto* pos = sentinel.next; pos != &sentinel; pos = pos->next) {
        if (FromLink(pos) == layer) {
            Unlink(layer);
            return;
        }
    }
}

void __cdecl LayerGroup::BringToFront(Layer* layer) { Unlink(layer); Insert(layer, false); }
void __cdecl LayerGroup::BringToBack(Layer* layer) { Unlink(layer); Insert(layer, true); }

Layer* __cdecl LayerGroup::FindLayer(const PString& name)
{
    for (auto* pos = sentinel.next; pos != &sentinel; pos = pos->next) {
        auto* layer = FromLink(pos);
        if (layer->GetName() == name) return layer;
        if (layer->IsGroup())
            if (auto* found = static_cast<LayerGroup*>(layer)->FindLayer(name)) return found;
    }
    return nullptr;
}

Layer* __cdecl LayerGroup::GetLayerAt(const Vector2& point)
{
    for (auto* pos = sentinel.next; pos != &sentinel; pos = pos->next) {
        auto* layer = FromLink(pos);
        if (layer->IsGroup()) {
            if (auto* found = static_cast<LayerGroup*>(layer)->GetLayerAt(point)) return found;
        } else if (VCall<bool>(layer, 0x10, &point)) {
            return layer;
        }
    }
    return nullptr;
}

Layer* __cdecl LayerGroup::GetLayerUnder(const Vector2& a, const Vector2& b)
{
    for (auto* pos = sentinel.next; pos != &sentinel; pos = pos->next) {
        auto* layer = FromLink(pos);
        if (layer->IsGroup()) {
            if (auto* found = static_cast<LayerGroup*>(layer)->GetLayerUnder(a, b)) return found;
        } else if (VCall<bool>(layer, 0x0c, &a, &b)) {
            return layer;
        }
    }
    return nullptr;
}

void __cdecl LayerGroup::UpdateLayer(float dt)
{
    for (auto* pos = sentinel.next; pos != &sentinel;) {
        auto* layer = FromLink(pos);
        VCall<void>(layer, 0x08, dt);
        pos = layer->link.next;
    }
}

void __cdecl LayerGroup::RenderLayer(SystemResourceViewerList& list)
{
    if (!IsVisible()) return;
    for (auto* pos = sentinel.next; pos != &sentinel;) {
        auto* layer = FromLink(pos);
        VCall<void>(layer, 0x04, &list);
        pos = layer->link.next;
    }
}

void __cdecl LayerGroup::Reset()
{
    for (auto* pos = sentinel.next; pos != &sentinel;) {
        auto* layer = FromLink(pos);
        VCall<void>(layer, 0x14);
        pos = layer->link.next;
    }
}

void __cdecl LayerGroup::Render() {}  // original shares Layer::Begin's empty body

const IList<Layer>& __cdecl LayerGroup::GetLayerList() const
{
    // The ABI exposes the list type; we only need its sentinel and count here.
    return *reinterpret_cast<const IList<Layer>*>(&sentinel);
}

const Rect& __cdecl LayerGroup::GetViewRect() const { return view; }
void __cdecl LayerGroup::SetViewRect(const Rect& rect) { view = rect; }

static LayerManager*& SharedLayerManagerSlot()
{
    // U16: local storage. LayerManager::Init sets it and Drop clears it, as the retail static did.
    static LayerManager* manager;
    return manager;
}

LayerManager* SharedLayerManager() { return SharedLayerManagerSlot(); }

Size LayerTargetSize(const Display* d)
{
    return d->GetTargetSize();
}

Size LayerGroup::DisplaySize()
{
    if (!kernel)
        if (auto* manager = SharedLayerManager()) kernel = VCall<RenderKernel*>(manager, 0x04);
    return kernel ? LayerTargetSize(kernel->GetDisplay()) : Size{};
}

void __cdecl LayerGroup::GetGroupViewRect(Rect& out)
{
    out.x = view.x;
    out.y = view.y;
    const Size size = view.w > 0 && view.h > 0 ? Size{view.w, view.h} : DisplaySize();
    out.w = size.w;
    out.h = size.h;
    if (group) {
        Point origin{out.x, out.y};
        group->ConvertToWindow(origin);
        out.x = origin.x;
        out.y = origin.y;
    }
}

void __cdecl LayerGroup::GetClippedGroupViewRect(Rect& out)
{
    if (!group) {
        const auto size = DisplaySize();
        out = {0, 0, size.w, size.h};
        return;
    }
    group->GetClippedGroupViewRect(out);
    out.x += view.x;
    out.y += view.y;
    out.w -= view.x;
    out.h -= view.y;
    if (view.w > 0 && view.h > 0) {
        if (view.w < out.w) out.w = view.w;
        if (view.h < out.h) out.h = view.h;
    }
}

void __cdecl LayerGroup::ConvertToWindow(Point& point) const
{
    for (auto* g = this; g; g = g->GetParent()) {
        point.x += g->view.x;
        point.y += g->view.y;
    }
}

void __cdecl LayerGroup::ConvertToWindowAndClip(Rect& rect) const
{
    for (auto* g = this; g; g = g->GetParent()) {
        const auto& r = g->view;
        if (r.w > 0 && r.h > 0) {
            if (r.w < rect.w + rect.x) rect.w = r.w - rect.x;
            if (r.h < rect.h + rect.y) rect.h = r.h - rect.y;
        }
        rect.x += r.x;
        rect.y += r.y;
    }
}

// RenderSystem stores both the root group and the owning viewer array inline.
class LayerManager {
public:
    LayerGroup* __cdecl GetLayerGroup();
    Layer* __cdecl FindLayer(const PString& name);
    Layer* __cdecl GetLayerAt(const Vector2& point);
    Layer* __cdecl GetLayerUnder(const Vector2& a, const Vector2& b);
    void __cdecl BringToFront(Layer* layer);
    void __cdecl BringToBack(Layer* layer);
    void __cdecl Reset();
    void __cdecl AddSystemResourceViewer(SystemResourceViewer* viewer);
    void __cdecl RemoveSystemResourceViewer(SystemResourceViewer* viewer);
    SystemResourceViewer* __cdecl FindSystemResourceViewer(const PString& name) const;
    void __cdecl Update(float dt);
    void __cdecl Render();

    void Init();
    void Copy(const LayerManager& source);
    void Assign(const LayerManager& source);
    void Drop();
private:
    void** vptr;
    LayerGroup root;
    SystemResourceViewerList viewers;
};
JET_LAYOUT32(sizeof(LayerManager) == 0x44);

LayerGroup* __cdecl LayerManager::GetLayerGroup() { return &root; }
LayerGroup* LayerManagerRoot(LayerManager* manager) { return manager->GetLayerGroup(); }
Layer* __cdecl LayerManager::FindLayer(const PString& name) { return root.FindLayer(name); }
Layer* __cdecl LayerManager::GetLayerAt(const Vector2& point) { return root.GetLayerAt(point); }
Layer* __cdecl LayerManager::GetLayerUnder(const Vector2& a, const Vector2& b) { return root.GetLayerUnder(a, b); }
void __cdecl LayerManager::BringToFront(Layer* layer) { root.BringToFront(layer); }
void __cdecl LayerManager::BringToBack(Layer* layer) { root.BringToBack(layer); }
void __cdecl LayerManager::Reset() { VCall<void>(&root, 0x14); }

void __cdecl LayerManager::AddSystemResourceViewer(SystemResourceViewer* viewer) { viewers.Add(viewer); }
void __cdecl LayerManager::RemoveSystemResourceViewer(SystemResourceViewer* viewer) { viewers.Remove(viewers.Find(viewer)); }

SystemResourceViewer* __cdecl LayerManager::FindSystemResourceViewer(const PString& name) const
{
    for (auto i = viewers.First(); i != viewers.End(); ++i)
        if ((*i)->GetName() == name) return *i;
    return nullptr;
}

namespace {
FARPROC LayerManagerSymbol(const char* name)
{
    return GetProcAddress(GetModuleHandleA("RenderSystem_7c_orig.dll"), name);
}

void** LayerManagerTable()
{
    return RsLayerManagerVftable;
}
}

void __cdecl LayerManager::Update(float dt)
{
    // U16: the mirrored info block, initialised locally like the retail static initialiser.
    auto* const info = reinterpret_cast<Profile::Info*>(::RsRsD020);
    {
        Profile profile(info);
        VCall<void>(&root, 0x08, dt);
    }
    RsMirrorSync("?__info@?1??Update@LayerManager@Jet@@QAAXM@Z@4UInfo@Profile@3@A");
}

void __cdecl LayerManager::Render()
{
    auto* const info = reinterpret_cast<Profile::Info*>(::RsRsD019);
    {
        Profile profile(info);
        VCall<void>(&root, 0x04, &viewers);
    }
    RsMirrorSync("?__info@?1??Render@LayerManager@Jet@@QAAXXZ@4UInfo@Profile@3@A");
}

void* __fastcall RsGroupGrouped(void*, void*, void*, const void*, int, bool);
void* __fastcall RsGroupCopy(void*, void*, const void*);
void* __fastcall RsGroupAssign(void*, void*, const void*);
void __fastcall RsGroupDrop(void*, void*);

void LayerManager::Init()
{
    vptr = LayerManagerTable();
    const PString name("LayerManager");
    RsGroupGrouped(&root, nullptr, nullptr, &name, 0, true);
    new (&viewers) SystemResourceViewerList();
    SharedLayerManagerSlot() = this;
}

void LayerManager::Copy(const LayerManager& source)
{
    vptr = LayerManagerTable();
    RsGroupCopy(&root, nullptr, &source.root);
    new (&viewers) SystemResourceViewerList();
    try {
        viewers.AppendChunks(source.viewers);
    } catch (...) {
        viewers.~SystemResourceViewerList();
        RsGroupDrop(&root, nullptr);
        throw;
    }
}

void LayerManager::Assign(const LayerManager& source)
{
    RsGroupAssign(&root, nullptr, &source.root);
    viewers.RemoveAll();              // assignment frees chunks, not the viewers
    viewers.AppendChunks(source.viewers);
}

void LayerManager::Drop()
{
    vptr = LayerManagerTable();
    for (auto i = viewers.First(); i != viewers.End(); ++i)
        if (auto* viewer = *i) VDelete(viewer);
    viewers.RemoveAll();
    if (SharedLayerManagerSlot() == this) SharedLayerManagerSlot() = nullptr;
    viewers.~SystemResourceViewerList();
    RsGroupDrop(&root, nullptr);
}

void* __fastcall RsLayerManagerCreate(void* p, void*) { static_cast<LayerManager*>(p)->Init(); return p; }
void* __fastcall RsLayerManagerCopy(void* p, void*, const void* source)
{
    static_cast<LayerManager*>(p)->Copy(*static_cast<const LayerManager*>(source));
    return p;
}
void* __fastcall RsLayerManagerAssign(void* p, void*, const void* source)
{
    static_cast<LayerManager*>(p)->Assign(*static_cast<const LayerManager*>(source));
    return p;
}
void __fastcall RsLayerManagerDrop(void* p, void*) { static_cast<LayerManager*>(p)->Drop(); }

class SceneGraph {
public:
    explicit SceneGraph(ModifierUpdateTask* task);
    SceneGraph(const SceneGraph& source);
    SceneGraph& operator=(const SceneGraph& source);
    ~SceneGraph();

    bool __cdecl IsEmpty() const;
    void __cdecl AddScene(Element* scene);
    void __cdecl UnlinkScene(Element* scene);
    void __cdecl DelayedDelete(Element* scene);
    void __cdecl PurgeDeathRow();
    ModifierUpdateTask* __cdecl GetModifierTask();
    void __cdecl SetModifierTask(ModifierUpdateTask* task);
    void __cdecl Update(float dt);

private:
    using SceneList = FastDynamicArray<Element*>;
    static void DeleteAll(SceneList& list);
    SceneList scenes;
    SceneList deathRow;
    ModifierUpdateTask* modifierTask;
};
JET_LAYOUT32(sizeof(SceneGraph) == 0x0c);

SceneGraph::SceneGraph(ModifierUpdateTask* task) : modifierTask(task ? task : new ModifierUpdateTask) {}

SceneGraph::SceneGraph(const SceneGraph& source) : modifierTask(source.modifierTask)
{
    scenes.AppendChunks(source.scenes);
    deathRow.AppendChunks(source.deathRow);
}

SceneGraph& SceneGraph::operator=(const SceneGraph& source)
{
    // Like the original, assignment frees the chunks without deleting scenes
    // and shallow-copies the task pointer.
    scenes.RemoveAll();
    scenes.AppendChunks(source.scenes);
    deathRow.RemoveAll();
    deathRow.AppendChunks(source.deathRow);
    modifierTask = source.modifierTask;
    return *this;
}

void SceneGraph::DeleteAll(SceneList& list)
{
    for (auto i = list.First(); i != list.End(); ++i)
        if (auto* scene = *i) VDelete(scene);
    list.RemoveAll();
}

SceneGraph::~SceneGraph()
{
    PurgeDeathRow();
    DeleteAll(scenes);
    if (modifierTask) VDelete(modifierTask);
    modifierTask = nullptr;
}

bool __cdecl SceneGraph::IsEmpty() const { return !scenes.head && !deathRow.head; }
void __cdecl SceneGraph::AddScene(Element* scene) { scenes.Add(scene); }
void __cdecl SceneGraph::UnlinkScene(Element* scene) { scenes.Remove(scenes.Find(scene)); }
void __cdecl SceneGraph::DelayedDelete(Element* scene) { deathRow.Add(scene); }
void __cdecl SceneGraph::PurgeDeathRow() { DeleteAll(deathRow); }
ModifierUpdateTask* __cdecl SceneGraph::GetModifierTask() { return modifierTask; }
void __cdecl SceneGraph::SetModifierTask(ModifierUpdateTask* task) { modifierTask = task; }
void __cdecl SceneGraph::Update(float dt) { if (modifierTask) VCall<void>(modifierTask, 0x2c, dt); }
void __fastcall RsSceneGraphDefaultClosure(void* p, void*) { new (p) SceneGraph(nullptr); }

void RsUpdateLayers(void* layers, float dt) { static_cast<LayerManager*>(layers)->Update(dt); }
void RsRenderLayers(void* layers) { static_cast<LayerManager*>(layers)->Render(); }
void RsUpdateScene(SceneGraph* scene, float dt) { scene->Update(dt); }
void RsPurgeScene(SceneGraph* scene) { scene->PurgeDeathRow(); }
void RsDestroyScene(SceneGraph* scene)
{
    scene->~SceneGraph();
    Alloc::Delete(scene);
}

void __cdecl RenderKernel::SetSceneGraph(SceneGraph* graph)
{
    auto& current = *reinterpret_cast<SceneGraph**>(reinterpret_cast<unsigned char*>(this) + 0x80);
    if (auto* previous = current) RsDestroyScene(previous);
    current = graph;
}

void __cdecl RenderKernel::DestroySceneGraph()
{
    SetSceneGraph(nullptr);
}

}  // namespace Jet
