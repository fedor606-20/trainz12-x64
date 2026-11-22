// Usage: diff_rs_u8.exe <bin dir> (original and rebuilt DLLs in the same directory).
#include <windows.h>

#include <bit>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <random>
#include <string>
#include <vector>

struct Rect { int x, y, w, h; };
struct alignas(4) OverlayBytes { unsigned char data[0x3c]; };

template <class T>
static T Proc(HMODULE mod, const char* name)
{
    const auto p = GetProcAddress(mod, name);
    if (!p) {
        std::printf("missing export: %s\n", name);
        std::exit(2);
    }
    return reinterpret_cast<T>(p);
}

struct Api {
    using QueryBool = bool (__cdecl*)(const void*);
    using SetBool = void (__cdecl*)(void*, bool);

    int (__cdecl* getPriority)(const void*);
    QueryBool isVisible, isGroup;
    SetBool setVisible, setViewport;
    void (__cdecl* setClipping)(void*, const Rect*);
    // MSVC x86 cdecl member ABI: explicit this first, hidden struct-result buffer second.
    Rect* (__cdecl* getClipping)(const void*, Rect*);

    explicit Api(HMODULE mod)
        : getPriority(Proc<decltype(getPriority)>(mod, "?GetPriority@Layer@Jet@@QBAHXZ")),
          isVisible(Proc<QueryBool>(mod, "?IsVisible@Layer@Jet@@QBA_NXZ")),
          isGroup(Proc<QueryBool>(mod, "?IsGroup@Layer@Jet@@QBA_NXZ")),
          setVisible(Proc<SetBool>(mod, "?SetVisible@Layer@Jet@@QAAX_N@Z")),
          setViewport(Proc<SetBool>(mod, "?SetViewPortMode@Overlay@Jet@@QAAX_N@Z")),
          setClipping(Proc<decltype(setClipping)>(mod, "?SetClipping@Overlay@Jet@@QAAXABURect@2@@Z")),
          getClipping(Proc<decltype(getClipping)>(mod, "?GetClipping@Overlay@Jet@@QBA?AURect@2@XZ"))
    {}
};

struct Node { Node* next; Node* prev; };
struct Point { int x, y; };
struct Vec { float x, y; };
struct Color { float r, g, b, a; };
struct Group;
struct Layer {
    const FARPROC* vptr{};
    Node link{};
    const void* name{};
    int priority{};
    unsigned char flags{};
    void* kernel{};
    Group* parent{};
};
struct Group {
    Layer layer;
    Node sentinel{&sentinel, &sentinel};
    int count{};
    Rect view{};
};
static_assert(sizeof(Layer) == 0x20 && offsetof(Layer, link) == 4);
static_assert(sizeof(Group) == 0x3c && offsetof(Group, view) == 0x2c);

static std::vector<int>* trace;
static void* managerKernel;
static void* __cdecl GetManagerKernel(const void*)
{
    trace->push_back(701);
    return managerKernel;
}
static void* __fastcall DeleteChild(Layer* child, void*, unsigned flags)
{
    trace->insert(trace->end(), {710, child->priority, static_cast<int>(flags),
        !child->link.next && !child->link.prev});
    return child;
}
static void __cdecl Viewport(void*, const Rect* r)
{
    trace->insert(trace->end(), {721, r->x, r->y, r->w, r->h});
}
static void __cdecl Scissor(void*, const Rect* r)
{
    trace->insert(trace->end(), {722, r->x, r->y, r->w, r->h});
}
static void __cdecl ClearColor(void*, const Color* c)
{
    trace->insert(trace->end(), {723, std::bit_cast<int>(c->r), std::bit_cast<int>(c->g),
        std::bit_cast<int>(c->b), std::bit_cast<int>(c->a)});
}
static void __cdecl ClearBuffers(void*, unsigned mask)
{
    trace->insert(trace->end(), {724, static_cast<int>(mask)});
}
static void __cdecl Projection(void*, float l, float r, float b, float t, float n, float f)
{
    trace->insert(trace->end(), {0x180, std::bit_cast<int>(l), std::bit_cast<int>(r), std::bit_cast<int>(b),
        std::bit_cast<int>(t), std::bit_cast<int>(n), std::bit_cast<int>(f)});
}
static void __cdecl SetupOverlay(void*) { trace->push_back(0x50); }
template <int slot> static void __cdecl OneParameter(void*, int n)
{
    trace->insert(trace->end(), {slot, n & 0xff});
}
template <int slot> static void __cdecl TwoParameters(void*, int a, int b)
{
    trace->insert(trace->end(), {slot, a, b & 0xff});
}
static void __cdecl Render(Layer* l, const void* list)
{
    trace->push_back(100 + l->priority);
    trace->push_back(*static_cast<const int*>(list));
}
static void __cdecl Update(Layer* l, float dt)
{
    trace->push_back(200 + l->priority);
    trace->push_back(static_cast<int>(dt * 4));
}
static bool __cdecl At(Layer* l, const Vec* p)
{
    trace->push_back(300 + l->priority);
    return l->priority == static_cast<int>(p->x);
}
static bool __cdecl Under(Layer* l, const Vec* a, const Vec* b)
{
    trace->push_back(400 + l->priority);
    return l->priority == static_cast<int>(a->x + b->y);
}
static void __cdecl Reset(Layer* l) { trace->push_back(500 + l->priority); }
static void __cdecl Begin(Layer*) { trace->push_back(601); }
static void __cdecl Draw(Layer*) { trace->push_back(602); }
static void __cdecl End(Layer*) { trace->push_back(603); }
static bool __cdecl CanRender(Layer* l)
{
    trace->push_back(600);
    return l->priority > 0;
}

struct Viewer {
    const FARPROC* vptr;
    void* kernel;
    const void* name;
};
struct ViewerChunk {
    ViewerChunk* next;
    ViewerChunk* prev;
    unsigned count;
    Viewer* viewers[13];
};
struct ResourceLayerManager {
    const FARPROC* vptr;
    Group root;
    ViewerChunk* viewers;
};
static_assert(offsetof(ResourceLayerManager, viewers) == 0x40 && sizeof(Viewer) == 0x0c);

static void LogViewers(const ViewerChunk* head)
{
    if (head) {
        const auto* c = head;
        do {
            trace->push_back(static_cast<int>(c->count));
            trace->push_back(c->next->prev == c);
            for (unsigned i = 0; i < c->count; ++i)
                trace->push_back(static_cast<int>(reinterpret_cast<UINT_PTR>(c->viewers[i]->kernel)));
            c = c->next;
        } while (c != head);
    }
    trace->push_back(-1);
}

static void __cdecl RenderManagerRoot(const void*, ViewerChunk* const* viewers)
{
    trace->push_back(731);
    LogViewers(*viewers);
}

static void __cdecl UpdateManagerRoot(const void*, float dt)
{
    trace->insert(trace->end(), {732, std::bit_cast<int>(dt)});
}

static void* __fastcall DeleteViewer(Viewer* viewer, void*, unsigned flags)
{
    trace->insert(trace->end(), {733, static_cast<int>(reinterpret_cast<UINT_PTR>(viewer->kernel)),
        static_cast<int>(flags)});
    return viewer;
}

static void ResourceManagerTrace(HMODULE mod, HMODULE original, const void* alpha, const void* beta)
{
    using Default = void* (__thiscall*)(void*);
    using Copy = void* (__thiscall*)(void*, const void*);
    using Drop = void (__thiscall*)(void*);
    using ManagerViewer = void (__cdecl*)(ResourceLayerManager*, Viewer*);
    const auto managerCtor = Proc<Default>(mod, "??0LayerManager@Jet@@QAE@XZ");
    const auto managerCopy = Proc<Copy>(mod, "??0LayerManager@Jet@@QAE@ABV01@@Z");
    const auto managerAssign = Proc<Copy>(mod, "??4LayerManager@Jet@@QAEAAV01@ABV01@@Z");
    const auto managerDrop = Proc<Drop>(mod, "??1LayerManager@Jet@@UAE@XZ");
    const auto add = Proc<ManagerViewer>(mod, "?AddSystemResourceViewer@LayerManager@Jet@@QAAXPAVSystemResourceViewer@2@@Z");
    const auto remove = Proc<ManagerViewer>(mod, "?RemoveSystemResourceViewer@LayerManager@Jet@@QAAXPAVSystemResourceViewer@2@@Z");
    const auto find = Proc<Viewer*(__cdecl*)(ResourceLayerManager*, const void*)>(
        mod, "?FindSystemResourceViewer@LayerManager@Jet@@QBAPAVSystemResourceViewer@2@ABVPString@2@@Z");
    const auto update = Proc<void(__cdecl*)(ResourceLayerManager*, float)>(mod, "?Update@LayerManager@Jet@@QAAXM@Z");
    const auto render = Proc<void(__cdecl*)(ResourceLayerManager*)>(mod, "?Render@LayerManager@Jet@@QAAXXZ");
    const auto viewerCtor = Proc<void*(__thiscall*)(void*, void*, const void*)>(
        mod, "??0SystemResourceViewer@Jet@@QAE@PAVRenderKernel@1@ABVPString@1@@Z");
    const auto viewerCopy = Proc<Copy>(mod, "??0SystemResourceViewer@Jet@@QAE@ABV01@@Z");
    const auto viewerAssign = Proc<Copy>(mod, "??4SystemResourceViewer@Jet@@QAEAAV01@ABV01@@Z");
    const auto viewerDrop = Proc<Drop>(mod, "??1SystemResourceViewer@Jet@@UAE@XZ");
    const auto setName = Proc<void(__cdecl*)(Viewer*, const void*)>(mod, "?SetName@SystemResourceViewer@Jet@@QAAXABVPString@2@@Z");
    const auto getName = Proc<const void* const*(__cdecl*)(Viewer*)>(mod, "?GetName@SystemResourceViewer@Jet@@QBAABVPString@2@XZ");
    const auto viewerTable = Proc<const FARPROC*>(mod, "??_7SystemResourceViewer@Jet@@6B@");
    const auto managerTable = Proc<const FARPROC*>(mod, "??_7LayerManager@Jet@@6B@");
    auto** shared = Proc<void**>(original, "?layerManager@Layer@Jet@@1PAVLayerManager@2@A");
    auto* oldManager = *shared;
    *shared = nullptr;

    ResourceLayerManager a{}, b{}, c{};
    Viewer vs[30]{};
    const FARPROC stubViewerTable[]{reinterpret_cast<FARPROC>(DeleteViewer)};
    const FARPROC rootTable[]{nullptr, reinterpret_cast<FARPROC>(RenderManagerRoot),
        reinterpret_cast<FARPROC>(UpdateManagerRoot)};
    trace->push_back(managerCtor(&a) == &a);
    trace->insert(trace->end(), {a.vptr == managerTable, *shared == &a, !a.viewers, !a.root.layer.parent,
        a.root.layer.flags & 3, a.root.sentinel.next == &a.root.sentinel});
    const auto* rootName = static_cast<const char*>(a.root.layer.name) + 0x18;
    trace->push_back(!std::strcmp(rootName, "LayerManager"));
    for (int i = 0; i < 30; ++i) {
        trace->push_back(viewerCtor(&vs[i], reinterpret_cast<void*>(static_cast<UINT_PTR>(i + 1)), &alpha) == &vs[i]);
        trace->push_back(vs[i].vptr == viewerTable && *getName(&vs[i]) == alpha);
        vs[i].vptr = stubViewerTable;
        add(&a, &vs[i]);
    }
    setName(&vs[15], &beta);
    trace->insert(trace->end(), {find(&a, &alpha) == &vs[0], find(&a, &beta) == &vs[15]});
    Viewer duplicate{}, none{};
    viewerCopy(&duplicate, &vs[15]);
    trace->push_back(duplicate.vptr == viewerTable && duplicate.kernel == vs[15].kernel && duplicate.name == beta);
    viewerAssign(&duplicate, &vs[16]);
    trace->push_back(duplicate.vptr == viewerTable && duplicate.kernel == vs[16].kernel && duplicate.name == alpha);
    viewerDrop(&duplicate);
    remove(&a, &none);
    remove(&a, &vs[4]);
    remove(&a, &vs[15]);
    trace->push_back(!find(&a, &beta));
    LogViewers(a.viewers);
    managerCopy(&b, &a);
    trace->push_back(b.viewers != a.viewers && *shared == &a);
    LogViewers(b.viewers);
    managerCopy(&c, &a);
    remove(&c, &vs[29]);
    managerAssign(&c, &a);
    LogViewers(c.viewers);
    a.root.layer.vptr = rootTable;
    update(&a, 1.75f);
    render(&a);

    managerDrop(&c);
    managerDrop(&b);
    trace->insert(trace->end(), {*shared == &a, !b.viewers, !c.viewers});
    managerDrop(&a);
    trace->insert(trace->end(), {*shared == nullptr, !a.viewers});
    for (auto& v : vs) viewerDrop(&v);

    const auto resCtor = Proc<Default>(mod, "??0SystemResourceManager@Jet@@QAE@XZ");
    const auto resCopy = Proc<Copy>(mod, "??0SystemResourceManager@Jet@@QAE@ABV01@@Z");
    const auto resAssign = Proc<Copy>(mod, "??4SystemResourceManager@Jet@@QAEAAV01@ABV01@@Z");
    const auto resDrop = Proc<Drop>(mod, "??1SystemResourceManager@Jet@@UAE@XZ");
    const auto resTable = Proc<const FARPROC*>(mod, "??_7SystemResourceManager@Jet@@6B@");
    const FARPROC* m1 = nullptr;
    const FARPROC* m2 = nullptr;
    trace->push_back(resCtor(&m1) == &m1 && m1 == resTable);
    trace->push_back(resCopy(&m2, &m1) == &m2 && m2 == resTable);
    m2 = nullptr;
    trace->push_back(resAssign(&m2, &m1) == &m2 && m2 == nullptr);
    resDrop(&m2);
    trace->push_back(m2 == resTable);
    *shared = oldManager;
}

static void SceneGraphTrace(HMODULE mod)
{
    struct Scene { ViewerChunk* scenes; ViewerChunk* deathRow; Viewer* task; };
    using Copy = Scene* (__thiscall*)(Scene*, const Scene*);
    using SceneOp = void (__cdecl*)(Scene*, Viewer*);
    const auto ctor = Proc<Scene*(__thiscall*)(Scene*, Viewer*)>(mod, "??0SceneGraph@Jet@@QAE@PAVModifierUpdateTask@1@@Z");
    const auto closure = Proc<void(__thiscall*)(Scene*)>(mod, "??_FSceneGraph@Jet@@QAEXXZ");
    const auto copy = Proc<Copy>(mod, "??0SceneGraph@Jet@@QAE@ABV01@@Z");
    const auto assign = Proc<Copy>(mod, "??4SceneGraph@Jet@@QAEAAV01@ABV01@@Z");
    const auto drop = Proc<void(__thiscall*)(Scene*)>(mod, "??1SceneGraph@Jet@@QAE@XZ");
    const auto empty = Proc<bool(__cdecl*)(Scene*)>(mod, "?IsEmpty@SceneGraph@Jet@@QBA_NXZ");
    const auto add = Proc<SceneOp>(mod, "?AddScene@SceneGraph@Jet@@QAAXPAVElement@2@@Z");
    const auto unlink = Proc<SceneOp>(mod, "?UnlinkScene@SceneGraph@Jet@@QAAXPAVElement@2@@Z");
    const auto enqueue = Proc<SceneOp>(mod, "?DelayedDelete@SceneGraph@Jet@@QAAXPAVElement@2@@Z");
    const auto purge = Proc<void(__cdecl*)(Scene*)>(mod, "?PurgeDeathRow@SceneGraph@Jet@@QAAXXZ");
    const auto getTask = Proc<Viewer*(__cdecl*)(Scene*)>(mod, "?GetModifierTask@SceneGraph@Jet@@QAAPAVModifierUpdateTask@2@XZ");
    const auto setTask = Proc<SceneOp>(mod, "?SetModifierTask@SceneGraph@Jet@@QAAXPAVModifierUpdateTask@2@@Z");
    const auto update = Proc<void(__cdecl*)(Scene*, float)>(mod, "?Update@SceneGraph@Jet@@QAAXM@Z");
    FARPROC table[12]{};
    table[0] = reinterpret_cast<FARPROC>(DeleteViewer);
    table[11] = reinterpret_cast<FARPROC>(UpdateManagerRoot);
    Viewer task{table, reinterpret_cast<void*>(99), nullptr};
    Viewer elements[17]{};
    Scene a{}, b{}, c{};
    const auto mark = [](bool result) { trace->push_back(result); };
    mark(ctor(&a, &task) == &a && getTask(&a) == &task && empty(&a));
    update(&a, 0.75f);
    setTask(&a, nullptr);
    update(&a, 0.5f);
    setTask(&a, &task);
    for (int i = 0; i < 17; ++i) {
        elements[i] = {table, reinterpret_cast<void*>(i + 1), nullptr};
        add(&a, &elements[i]);
    }
    mark(!empty(&a));
    unlink(&a, &elements[4]);
    unlink(&a, &elements[4]);
    enqueue(&a, &elements[4]);
    mark(copy(&b, &a) == &b && getTask(&b) == &task && b.scenes != a.scenes && b.deathRow != a.deathRow);
    LogViewers(b.scenes);
    LogViewers(b.deathRow);
    ctor(&c, &task);
    add(&c, &elements[4]);
    mark(assign(&c, &a) == &c && getTask(&c) == &task);
    LogViewers(c.scenes);
    LogViewers(c.deathRow);
    enqueue(&a, nullptr);
    purge(&a);
    mark(!a.deathRow && !empty(&a));
    drop(&a);
    drop(&b);
    drop(&c);
    mark(empty(&a) && !getTask(&a));
    closure(&a);
    mark(empty(&a) && getTask(&a));
    drop(&a);
}

static std::vector<int> GroupTrace(HMODULE mod)
{
    const auto setParent = Proc<void(__cdecl*)(Layer*, Group*)>(mod, "?SetParent@Layer@Jet@@QAAXPAVLayerGroup@2@@Z");
    const auto setPriority = Proc<void(__cdecl*)(Layer*, int)>(mod, "?SetPriority@Layer@Jet@@QAAXH@Z");
    const auto layerFront = Proc<void(__cdecl*)(Layer*)>(mod, "?BringToFront@Layer@Jet@@QAAXXZ");
    const auto layerBack = Proc<decltype(layerFront)>(mod, "?BringToBack@Layer@Jet@@QAAXXZ");
    const auto add = Proc<void(__cdecl*)(Group*, Layer*)>(mod, "?Add@LayerGroup@Jet@@QAAXPAVLayer@2@@Z");
    const auto remove = Proc<decltype(add)>(mod, "?Remove@LayerGroup@Jet@@QAAXPAVLayer@2@@Z");
    const auto groupFront = Proc<decltype(add)>(mod, "?BringToFront@LayerGroup@Jet@@QAAXPAVLayer@2@@Z");
    const auto groupBack = Proc<decltype(add)>(mod, "?BringToBack@LayerGroup@Jet@@QAAXPAVLayer@2@@Z");
    const auto getParent = Proc<Group*(__cdecl*)(const Layer*)>(mod, "?GetParent@Layer@Jet@@QBAPAVLayerGroup@2@XZ");
    const auto getName = Proc<const void**(__cdecl*)(const Layer*)>(mod, "?GetName@Layer@Jet@@QBAABVPString@2@XZ");
    const auto setKernel = Proc<void(__cdecl*)(Layer*, void*)>(mod, "?SetRenderKernel@Layer@Jet@@QAAXPAVRenderKernel@2@@Z");
    const auto getKernel = Proc<void*(__cdecl*)(const Layer*)>(mod, "?GetRenderKernel@Layer@Jet@@QBAPAVRenderKernel@2@XZ");
    const auto less = Proc<int(__cdecl*)(const Layer*, const Layer*)>(mod, "??MLayer@Jet@@QBAHABV01@@Z");
    const auto getList = Proc<const Node*(__cdecl*)(const Group*)>(mod, "?GetLayerList@LayerGroup@Jet@@QBAABV?$IList@VLayer@Jet@@@2@XZ");
    const auto getView = Proc<const Rect*(__cdecl*)(const Group*)>(mod, "?GetViewRect@LayerGroup@Jet@@QBAABURect@2@XZ");
    const auto setView = Proc<void(__cdecl*)(Group*, const Rect*)>(mod, "?SetViewRect@LayerGroup@Jet@@QAAXABURect@2@@Z");
    const auto getGroupView = Proc<void(__cdecl*)(Group*, Rect*)>(mod, "?GetGroupViewRect@LayerGroup@Jet@@QAAXAAURect@2@@Z");
    const auto getClippedView = Proc<decltype(getGroupView)>(mod, "?GetClippedGroupViewRect@LayerGroup@Jet@@QAAXAAURect@2@@Z");
    const auto groupPoint = Proc<void(__cdecl*)(const Group*, Point*)>(mod, "?ConvertToWindow@LayerGroup@Jet@@QBAXAAUPoint@2@@Z");
    const auto groupClip = Proc<void(__cdecl*)(const Group*, Rect*)>(mod, "?ConvertToWindowAndClip@LayerGroup@Jet@@QBAXAAURect@2@@Z");
    const auto layerPoint = Proc<void(__cdecl*)(const Layer*, Point*)>(mod, "?ConvertToWindow@Layer@Jet@@QBAXAAUPoint@2@@Z");
    const auto layerClip = Proc<void(__cdecl*)(const Layer*, Rect*)>(mod, "?ConvertToWindowAndClip@Layer@Jet@@QBAXAAURect@2@@Z");
    const auto find = Proc<Layer*(__cdecl*)(Group*, const void**)>(mod, "?FindLayer@LayerGroup@Jet@@QAAPAVLayer@2@ABVPString@2@@Z");
    const auto at = Proc<Layer*(__cdecl*)(Group*, const Vec*)>(mod, "?GetLayerAt@LayerGroup@Jet@@QAAPAVLayer@2@ABVVector2@2@@Z");
    const auto under = Proc<Layer*(__cdecl*)(Group*, const Vec*, const Vec*)>(mod, "?GetLayerUnder@LayerGroup@Jet@@QAAPAVLayer@2@ABVVector2@2@0@Z");
    const auto render = Proc<void(__cdecl*)(Group*, const void*)>(mod, "?RenderLayer@LayerGroup@Jet@@UAAXAAV?$FastDynamicArray@PAVSystemResourceViewer@Jet@@$0N@U?$Less@PAVSystemResourceViewer@Jet@@PAV12@@2@@2@@Z");
    const auto update = Proc<void(__cdecl*)(Group*, float)>(mod, "?UpdateLayer@LayerGroup@Jet@@UAAXM@Z");
    const auto reset = Proc<void(__cdecl*)(Group*)>(mod, "?Reset@LayerGroup@Jet@@UAAXXZ");
    const auto emptyRender = Proc<decltype(reset)>(mod, "?Render@LayerGroup@Jet@@MAAXXZ");
    struct Manager { const FARPROC* vptr{}; Group root; };
    static_assert(offsetof(Manager, root) == 4);
    const auto managerRoot = Proc<Group*(__cdecl*)(Manager*)>(mod, "?GetLayerGroup@LayerManager@Jet@@QAAPAVLayerGroup@2@XZ");
    const auto managerFind = Proc<Layer*(__cdecl*)(Manager*, const void**)>(mod, "?FindLayer@LayerManager@Jet@@QAAPAVLayer@2@ABVPString@2@@Z");
    const auto managerAt = Proc<Layer*(__cdecl*)(Manager*, const Vec*)>(mod, "?GetLayerAt@LayerManager@Jet@@QAAPAVLayer@2@ABVVector2@2@@Z");
    const auto managerUnder = Proc<Layer*(__cdecl*)(Manager*, const Vec*, const Vec*)>(mod, "?GetLayerUnder@LayerManager@Jet@@QAAPAVLayer@2@ABVVector2@2@0@Z");
    const auto managerFront = Proc<void(__cdecl*)(Manager*, Layer*)>(mod, "?BringToFront@LayerManager@Jet@@QAAXPAVLayer@2@@Z");
    const auto managerBack = Proc<decltype(managerFront)>(mod, "?BringToBack@LayerManager@Jet@@QAAXPAVLayer@2@@Z");
    const auto managerReset = Proc<void(__cdecl*)(Manager*)>(mod, "?Reset@LayerManager@Jet@@QAAXXZ");
    const auto baseRender = Proc<void(__cdecl*)(Layer*, const void*)>(mod, "?RenderLayer@Layer@Jet@@UAAXAAV?$FastDynamicArray@PAVSystemResourceViewer@Jet@@$0N@U?$Less@PAVSystemResourceViewer@Jet@@PAV12@@2@@2@@Z");
    const auto baseUpdate = Proc<void(__cdecl*)(Layer*, float)>(mod, "?UpdateLayer@Layer@Jet@@UAAXM@Z");
    const auto baseAt = Proc<decltype(&At)>(mod, "?Collides@Layer@Jet@@UBA_NABVVector2@2@@Z");
    const auto baseUnder = Proc<decltype(&Under)>(mod, "?Collides@Layer@Jet@@UBA_NABVVector2@2@0@Z");
    const auto baseCanRender = Proc<decltype(&CanRender)>(mod, "?CanRender@Layer@Jet@@MAA_NXZ");
    const auto baseReset = Proc<decltype(&Reset)>(mod, "?Reset@Layer@Jet@@UAAXXZ");
    const auto baseBegin = Proc<decltype(&Begin)>(mod, "?Begin@Layer@Jet@@MAAXXZ");
    const auto baseEnd = Proc<decltype(&End)>(mod, "?End@Layer@Jet@@MAAXXZ");
    const auto baseNoUpdate = Proc<decltype(&Update)>(mod, "?Update@Layer@Jet@@MAAXM@Z");

    std::vector<int> log;
    trace = &log;
    const FARPROC leafTable[]{nullptr, reinterpret_cast<FARPROC>(Render), reinterpret_cast<FARPROC>(Update),
        reinterpret_cast<FARPROC>(Under), reinterpret_cast<FARPROC>(At), reinterpret_cast<FARPROC>(Reset),
        reinterpret_cast<FARPROC>(Draw), reinterpret_cast<FARPROC>(Update), reinterpret_cast<FARPROC>(Begin),
        reinterpret_cast<FARPROC>(End), reinterpret_cast<FARPROC>(CanRender)};
    const FARPROC groupTable[]{nullptr, reinterpret_cast<FARPROC>(render), reinterpret_cast<FARPROC>(update),
        nullptr, nullptr, reinterpret_cast<FARPROC>(reset)};
    Manager manager;
    Group& root = manager.root;
    Group nested;
    root.layer.vptr = nested.layer.vptr = groupTable;
    root.layer.flags = nested.layer.flags = 3;
    nested.layer.priority = 2;
    Layer leaves[5];
    int names[5]{};
    for (int i = 0; i < 5; ++i) {
        leaves[i].name = &names[i];
        leaves[i].vptr = leafTable;
        leaves[i].priority = i / 2;
        setParent(&leaves[i], i < 3 ? &root : &nested);
    }
    setParent(&nested.layer, &root);
    const auto id = [&](const Layer* p) {
        if (!p) return -1;
        if (p == &root.layer) return 10;
        if (p == &nested.layer) return 11;
        for (int i = 0; i < 5; ++i) if (p == &leaves[i]) return i;
        return -2;
    };
    const auto nodeId = [&](const Node* p) {
        if (!p) return -1;
        if (p == &root.sentinel) return 20;
        if (p == &nested.sentinel) return 21;
        if (p == &nested.layer.link) return 11;
        for (int i = 0; i < 5; ++i) if (p == &leaves[i].link) return i;
        return -2;
    };
    auto snapshot = [&] {
        for (const auto* g : {&root, &nested}) {
            const auto* list = getList(g);
            log.insert(log.end(), {list == &g->sentinel, g->count, nodeId(list->next), nodeId(list->prev)});
        }
        for (const auto* l : {&nested.layer, &leaves[0], &leaves[1], &leaves[2], &leaves[3], &leaves[4]}) {
            const auto* p = getParent(l);
            log.insert(log.end(), {l->priority, p ? id(&p->layer) : -1,
                nodeId(l->link.next), nodeId(l->link.prev)});
        }
    };
    snapshot();
    remove(&root, &leaves[4]); // nonmember leaves both lists intact
    remove(&nested, &leaves[4]); // explicit removal does not change the parent pointer
    snapshot();
    add(&nested, &leaves[4]);
    groupBack(&root, &leaves[1]);
    snapshot();
    groupFront(&root, &leaves[1]);
    snapshot();
    log.push_back(managerRoot(&manager) == &root);
    managerBack(&manager, &leaves[1]);
    snapshot();
    managerFront(&manager, &leaves[1]);
    snapshot();

    std::mt19937 rng(1729);
    for (int i = 0; i < 256; ++i) {
        auto* l = &leaves[rng() % 5];
        switch (rng() % 4) {
            case 0: setParent(l, (rng() & 1) ? &root : &nested); break;
            case 1: setPriority(l, static_cast<int>(rng() % 5) - 2); break;
            case 2: layerFront(l); break;
            case 3: layerBack(l); break;
        }
        snapshot();
    }
    setKernel(&leaves[0], &root);
    log.insert(log.end(), {getKernel(&leaves[0]) == &root, getName(&leaves[0]) == &leaves[0].name,
        less(&leaves[0], &leaves[1])});
    const Rect rootView{4, -7, 40, 30};
    const Rect nestedView{11, 6, 20, 0}; // no clipping when either dimension is nonpositive
    setView(&root, &rootView);
    setView(&nested, &nestedView);
    setParent(&leaves[0], &nested);
    const auto* view = getView(&nested);
    log.insert(log.end(), {view == &nested.view, view->x, view->y, view->w, view->h});
    for (bool viaLayer : {false, true}) {
        Point p{-2, 3};
        Rect r{6, 15, 60, 51};
        if (viaLayer) {
            layerPoint(&leaves[0], &p);
            layerClip(&leaves[0], &r);
        } else {
            groupPoint(&nested, &p);
            groupClip(&nested, &r);
        }
        log.insert(log.end(), {p.x, p.y, r.x, r.y, r.w, r.h});
    }
    auto** sharedManager = Proc<void**>(GetModuleHandleA("RenderSystem_7c_orig.dll"), "?layerManager@Layer@Jet@@1PAVLayerManager@2@A");
    auto* priorManager = *sharedManager;
    const FARPROC managerTable[]{nullptr, reinterpret_cast<FARPROC>(GetManagerKernel)};
    manager.vptr = managerTable;
    alignas(4) unsigned char kernel[0x80]{}, display[0x294]{}, target[0x18]{};
    auto* displayPtr = display;
    auto* targetPtr = target;
    const Point windowSize{800, 600}, targetSize{350, 700};
    std::memcpy(kernel + 0x7c, &displayPtr, sizeof displayPtr);
    std::memcpy(display + 0x28c, &windowSize, sizeof windowSize);
    std::memcpy(target + 0x10, &targetSize, sizeof targetSize);
    managerKernel = kernel;
    auto logView = [&](Group* group, bool clip) {
        Rect rect{};
        (clip ? getClippedView : getGroupView)(group, &rect);
        log.insert(log.end(), {rect.x, rect.y, rect.w, rect.h, group->layer.kernel == kernel});
    };
    *sharedManager = nullptr;
    logView(&nested, false); // no kernel and no manager
    logView(&root, true);
    *sharedManager = &manager;
    logView(&nested, false); // falls back to the window size and caches the kernel
    logView(&nested, true);  // root obtains and caches its kernel too
    *sharedManager = nullptr;
    std::memcpy(display + 0x5c, &targetPtr, sizeof targetPtr);
    logView(&nested, false); // cached kernel, now with an active render target
    const Rect explicitView{3, 6, 90, 900};
    setView(&nested, &explicitView);
    logView(&nested, true);
    logView(&nested, false);
    *sharedManager = priorManager;
    managerKernel = nullptr;
    for (int i = -3; i <= 3; ++i) {
        const void* name = i < 0 || i >= 5 ? nullptr : &names[i];
        const Vec a{static_cast<float>(i), 0}, b{0, 1};
        log.push_back(id(find(&root, &name)));
        log.push_back(id(at(&root, &a)));
        log.push_back(id(under(&root, &a, &b)));
        log.push_back(id(managerFind(&manager, &name)));
        log.push_back(id(managerAt(&manager, &a)));
        log.push_back(id(managerUnder(&manager, &a, &b)));
    }
    const int renderMarker = 345;
    render(&root, &renderMarker);
    nested.layer.flags &= ~1;
    render(&root, &renderMarker);
    update(&root, 2.25f);
    reset(&root);
    managerReset(&manager);
    emptyRender(&root);
    Layer test;
    test.vptr = leafTable;
    const Vec zero{};
    log.insert(log.end(), {baseAt(&test, &zero), baseUnder(&test, &zero, &zero), baseCanRender(&test)});
    baseBegin(&test);
    baseEnd(&test);
    baseReset(&test);
    baseNoUpdate(&test, 1);
    for (int mode = 0; mode < 3; ++mode) {
        test.flags = mode != 0;
        test.priority = mode == 2;
        baseRender(&test, &renderMarker);
        baseUpdate(&test, 0.75f);
    }
    trace = nullptr;
    return log;
}

static std::vector<int> LifetimeTrace(HMODULE mod)
{
    using Default = void* (__thiscall*)(void*);
    using Named = void* (__thiscall*)(void*, const void*, int, bool);
    using Grouped = void* (__thiscall*)(void*, Group*, const void*, int, bool);
    using Copy = void* (__thiscall*)(void*, const void*);
    using Drop = void (__thiscall*)(void*);
    const auto groupDefault = Proc<Default>(mod, "??0LayerGroup@Jet@@QAE@XZ");
    const auto groupGrouped = Proc<Grouped>(mod, "??0LayerGroup@Jet@@QAE@PAV01@ABVPString@1@H_N@Z");
    const auto groupCopy = Proc<Copy>(mod, "??0LayerGroup@Jet@@QAE@ABV01@@Z");
    const auto groupAssign = Proc<Copy>(mod, "??4LayerGroup@Jet@@QAEAAV01@ABV01@@Z");
    const auto groupDrop = Proc<Drop>(mod, "??1LayerGroup@Jet@@UAE@XZ");
    const auto clearClosure = Proc<Drop>(mod, "??_FClearLayer@Jet@@QAEXXZ");
    const auto setName = Proc<void(__cdecl*)(Layer*, const void*)>(mod, "?SetName@Layer@Jet@@QAAXABVPString@2@@Z");
    const auto getManager = Proc<void*(__cdecl*)(Layer*)>(mod, "?GetLayerManager@Layer@Jet@@QAAPAVLayerManager@2@XZ");
    const auto add = Proc<void(__cdecl*)(Group*, Layer*)>(mod, "?Add@LayerGroup@Jet@@QAAXPAVLayer@2@@Z");
    const auto original = GetModuleHandleA("RenderSystem_7c_orig.dll");
    auto** sharedManager = Proc<void**>(original, "?layerManager@Layer@Jet@@1PAVLayerManager@2@A");
    auto* priorManager = *sharedManager;
    const auto ks = GetModuleHandleA("KernelStandard_7c.dll");
    const auto cache = *Proc<void**>(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    const auto create = Proc<void*(__cdecl*)(void*, const char*)>(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    const auto destroy = Proc<void(__cdecl*)(void*, void*)>(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    if (!cache) {
        std::puts("PString cache not initialized");
        std::exit(2);
    }
    const auto refs = [](const void* p, unsigned off) {
        int value;
        std::memcpy(&value, static_cast<const unsigned char*>(p) + off, sizeof value);
        return value;
    };
    const int refBaseline = refs(cache, 4);
    void* alpha = create(cache, "diff-u8-alpha");
    void* beta = create(cache, "diff-u8-beta");
    const FARPROC managerTable[]{nullptr, reinterpret_cast<FARPROC>(GetManagerKernel)};
    struct Manager { const FARPROC* vptr; Group root; } manager{managerTable};
    alignas(4) unsigned char kernel[0x80]{}, display[0x294]{};
    auto* displayPtr = display;
    const Point size{320, 240};
    std::memcpy(kernel + 0x7c, &displayPtr, sizeof displayPtr);
    std::memcpy(display + 0x28c, &size, sizeof size);
    managerKernel = kernel;
    *sharedManager = &manager;
    std::vector<int> log;
    trace = &log;
    Group local;
    local.layer.parent = &manager.root;
    local.layer.kernel = kernel;
    local.view = {3, 5, 100, 80};
    const FARPROC* baseTable = Proc<const FARPROC*>(mod, "??_7Layer@Jet@@6B@");
    const auto text = [](const void* node) { return node ? static_cast<const char*>(node) + 0x18 : ""; };
    const auto snap = [&](const OverlayBytes& bytes, const FARPROC* table, unsigned size) {
        const auto& l = *reinterpret_cast<const Layer*>(bytes.data);
        const bool selfLinks = l.link.next == &l.link || l.link.prev == &l.link;
        log.insert(log.end(), {l.vptr == table, l.priority, l.flags, l.kernel == kernel,
            l.parent == &manager.root, l.parent == &local, !!l.link.next, !!l.link.prev, selfLinks});
        const char* s = text(l.name);
        while (*s) log.push_back(static_cast<unsigned char>(*s++));
        log.push_back(0);
        if (size > sizeof(Layer))
            for (unsigned i = sizeof(Layer); i < size; ++i) log.push_back(bytes.data[i]);
    };
    for (const char* cls : {"Layer", "Overlay", "ClearLayer"}) {
        const std::string scope = std::string(cls) + "@Jet@@";
        const auto table = Proc<const FARPROC*>(mod, ("??_7" + scope + "6B@").c_str());
        const auto named = Proc<Named>(mod, ("??0" + scope + "QAE@ABVPString@1@H_N@Z").c_str());
        const auto grouped = Proc<Grouped>(mod, ("??0" + scope + "QAE@PAVLayerGroup@1@ABVPString@1@H_N@Z").c_str());
        const auto copy = Proc<Copy>(mod, ("??0" + scope + "QAE@ABV01@@Z").c_str());
        const auto assign = Proc<Copy>(mod, ("??4" + scope + "QAEAAV01@ABV01@@Z").c_str());
        const auto drop = Proc<Drop>(mod, ("??1" + scope + "UAE@XZ").c_str());
        const unsigned bytes = !std::strcmp(cls, "Layer") ? 0x20 : !std::strcmp(cls, "Overlay") ? 0x3c : 0x30;
        for (unsigned fill : {0x00, 0xa5, 0xff}) {
            OverlayBytes a, b;
            std::memset(&a, fill, sizeof a);
            std::memset(&b, fill ^ 0xffu, sizeof b);
            log.push_back(grouped(&a, &local, &alpha, -7, true) == &a);
            snap(a, table, bytes);
            log.push_back(getManager(reinterpret_cast<Layer*>(&a)) == &manager);
            setName(reinterpret_cast<Layer*>(&a), &beta);
            setName(reinterpret_cast<Layer*>(&a), &beta);
            log.push_back(copy(&b, &a) == &b);
            snap(b, table, bytes);
            log.push_back(assign(&b, &a) == &b);
            snap(b, table, bytes);
            drop(&b);
            drop(&a);
            log.push_back(*reinterpret_cast<const FARPROC**>(a.data) == baseTable);
            log.push_back(local.count);
            log.push_back(named(&a, &beta, 42, false) == &a);
            snap(a, table, bytes);
            drop(&a);
            log.push_back(manager.root.count);
            if (bytes == 0x30) clearClosure(&a);
            else Proc<Default>(mod, ("??0" + scope + "QAE@XZ").c_str())(&a);
            snap(a, table, bytes);
            drop(&a);
        }
    }
    {
        const auto clearCtor = Proc<Grouped>(mod, "??0ClearLayer@Jet@@QAE@PAVLayerGroup@1@ABVPString@1@H_N@Z");
        const auto clearDrop = Proc<Drop>(mod, "??1ClearLayer@Jet@@UAE@XZ");
        const auto getColor = Proc<Color*(__cdecl*)(void*, Color*)>(mod, "?GetClearColor@ClearLayer@Jet@@QAA?AVColor@2@XZ");
        const auto setColor = Proc<void(__cdecl*)(void*, const Color*)>(mod, "?SetClearColor@ClearLayer@Jet@@QAAXABVColor@2@@Z");
        const auto render = Proc<void(__cdecl*)(void*)>(mod, "?Render@ClearLayer@Jet@@UAAXXZ");
        OverlayBytes obj{};
        FARPROC displayTable[0x1a0 / 4 + 1]{};
        displayTable[0x190 / 4] = reinterpret_cast<FARPROC>(Viewport);
        displayTable[0x194 / 4] = reinterpret_cast<FARPROC>(Scissor);
        displayTable[0x1a0 / 4] = reinterpret_cast<FARPROC>(ClearColor);
        displayTable[0xd0 / 4] = reinterpret_cast<FARPROC>(ClearBuffers);
        alignas(4) unsigned char state[0x64]{}, target[0x18]{};
        auto* vptr = displayTable;
        auto* statePtr = state;
        auto* targetPtr = target;
        const Point targetSize{90, 70};
        std::memcpy(display, &vptr, sizeof vptr);
        std::memcpy(display + 0x54, &statePtr, sizeof statePtr);
        std::memcpy(target + 0x10, &targetSize, sizeof targetSize);
        clearCtor(&obj, &local, &alpha, 5, true);
        const Color custom{0.25f, 0.50f, 0.75f, 1.0f};
        setColor(&obj, &custom);
        Color color{};
        log.push_back(getColor(&obj, &color) == &color);
        log.push_back(std::memcmp(&color, &custom, sizeof color) == 0);
        render(&obj);
        render(&obj); // unchanged viewport/scissor/color suppress backend state calls, not the clear
        std::memcpy(display + 0x5c, &targetPtr, sizeof targetPtr);
        render(&obj);
        clearDrop(&obj);
    }
    {
        const auto overlayCtor = Proc<Grouped>(mod, "??0Overlay@Jet@@QAE@PAVLayerGroup@1@ABVPString@1@H_N@Z");
        const auto overlayDrop = Proc<Drop>(mod, "??1Overlay@Jet@@UAE@XZ");
        const auto overlayBegin = Proc<void(__cdecl*)(void*)>(mod, "?Begin@Overlay@Jet@@MAAXXZ");
        const auto overlayEnd = Proc<decltype(overlayBegin)>(mod, "?End@Overlay@Jet@@MAAXXZ");
        const Api api(mod);
        FARPROC displayTable[0x1f0 / 4 + 1]{};
        displayTable[0x50 / 4] = reinterpret_cast<FARPROC>(SetupOverlay);
        displayTable[0x180 / 4] = reinterpret_cast<FARPROC>(Projection);
        displayTable[0x190 / 4] = reinterpret_cast<FARPROC>(Viewport);
        displayTable[0x194 / 4] = reinterpret_cast<FARPROC>(Scissor);
        displayTable[0x18c / 4] = reinterpret_cast<FARPROC>(OneParameter<0x18c>);
        displayTable[0x198 / 4] = reinterpret_cast<FARPROC>(OneParameter<0x198>);
        displayTable[0x1ac / 4] = reinterpret_cast<FARPROC>(TwoParameters<0x1ac>);
        displayTable[0x1b4 / 4] = reinterpret_cast<FARPROC>(OneParameter<0x1b4>);
        displayTable[0x1d8 / 4] = reinterpret_cast<FARPROC>(OneParameter<0x1d8>);
        displayTable[0x1dc / 4] = reinterpret_cast<FARPROC>(TwoParameters<0x1dc>);
        displayTable[0x1e4 / 4] = reinterpret_cast<FARPROC>(OneParameter<0x1e4>);
        displayTable[0x1ec / 4] = reinterpret_cast<FARPROC>(OneParameter<0x1ec>);
        displayTable[0x1f0 / 4] = reinterpret_cast<FARPROC>(OneParameter<0x1f0>);
        alignas(4) unsigned char state[0xa4]{};
        auto* vptr = displayTable;
        auto* statePtr = state;
        std::memset(display + 0x5c, 0, sizeof(void*));
        std::memcpy(display, &vptr, sizeof vptr);
        std::memcpy(display + 0x54, &statePtr, sizeof statePtr);
        for (int mode = 0; mode < 4; ++mode) {
            OverlayBytes obj{};
            std::memset(state, 0, sizeof state);
            display[0x7c] = (mode & 2) != 0; // reverse culling
            state[0x64] = mode & 1;
            state[0x28] = (mode & 2) ? 2 : 1;
            state[0xa0] = (mode & 1) ? 0x27 : 0;
            state[0xa1] = (mode & 2) ? 0x04 : 0x01;
            overlayCtor(&obj, &local, &alpha, 5, true);
            api.setViewport(&obj, (mode & 2) != 0);
            const Rect clip{9, -6, 170, 33};
            api.setClipping(&obj, &clip);
            for (int pass = 0; pass < 2; ++pass) {
                overlayBegin(&obj);
                log.insert(log.end(), std::begin(state), std::end(state));
                overlayEnd(&obj);
                log.insert(log.end(), std::begin(state), std::end(state));
            }
            overlayDrop(&obj);
        }
    }
    const auto groupTable = Proc<const FARPROC*>(mod, "??_7LayerGroup@Jet@@6B@");
    Group a, b;
    log.push_back(groupDefault(&a) == &a);
    log.push_back(a.layer.vptr == groupTable && a.sentinel.next == &a.sentinel && a.count == 0);
    a.view = {1, 2, 3, 4};
    log.push_back(groupCopy(&b, &a) == &b);
    log.push_back(b.sentinel.next == &a.sentinel && b.view.h == 4);
    a.view.h = 9;
    log.push_back(groupAssign(&b, &a) == &b && b.view.h == 9);
    groupDrop(&b);
    groupDrop(&a);
    log.push_back(groupGrouped(&a, &local, &alpha, 17, true) == &a);
    const FARPROC childTable[]{reinterpret_cast<FARPROC>(DeleteChild)};
    Layer one, two;
    one.vptr = two.vptr = childTable;
    one.priority = 20;
    two.priority = 10;
    add(&a, &one);
    add(&a, &two);
    groupDrop(&a);
    log.insert(log.end(), {a.count, a.sentinel.next == &a.sentinel, local.count, manager.root.count,
        refs(alpha, 0x10), refs(beta, 0x10)});
    ResourceManagerTrace(mod, original, alpha, beta);
    SceneGraphTrace(mod);
    {
        using Delete = void* (__thiscall*)(void*, unsigned);
        const auto ctor = Proc<void*(__thiscall*)(void*, void*, const void*)>(
            mod, "??0SystemResourceViewer@Jet@@QAE@PAVRenderKernel@1@ABVPString@1@@Z");
        const auto table = Proc<const FARPROC*>(mod, "??_7SystemResourceViewer@Jet@@6B@");
        const auto deleting = reinterpret_cast<Delete>(table[0]);
        const auto alloc = Proc<void*(__cdecl*)(unsigned)>(ks, "?New@Alloc@Jet@@SAPAXI@Z");
        unsigned baseline = refs(alpha, 0x10);
        auto* single = alloc(sizeof(Viewer));
        ctor(single, nullptr, &alpha);
        log.push_back(deleting(single, 1) == single);
        auto* header = static_cast<unsigned*>(alloc(sizeof(unsigned) + 2 * sizeof(Viewer)));
        *header = 2;
        auto* first = reinterpret_cast<Viewer*>(header + 1);
        ctor(first, nullptr, &alpha);
        ctor(first + 1, nullptr, &alpha);
        log.push_back(deleting(first, 3) == header);
        log.push_back(refs(alpha, 0x10) == baseline);
    }
    destroy(cache, beta);
    destroy(cache, alpha);
    log.push_back(refs(cache, 4) - refBaseline);
    *sharedManager = priorManager;
    managerKernel = nullptr;
    trace = nullptr;
    return log;
}

static bool CheckVtables(HMODULE original, HMODULE rebuilt)
{
    struct Table { const char* name; unsigned slots; unsigned pure; };
    const Table tables[]{
        {"??_7Layer@Jet@@6B@", 11, 1u << 6},
        {"??_7LayerGroup@Jet@@6B@", 11, 0},
        {"??_7ClearLayer@Jet@@6B@", 11, 0},
        {"??_7Overlay@Jet@@6B@", 11, 1u << 6},
        {"??_7LayerManager@Jet@@6B@", 2, 0x2},
        {"??_7SystemResourceViewer@Jet@@6B@", 3, 0x6},
        {"??_7SystemResourceManager@Jet@@6B@", 5, 0x1e},
    };
    for (const auto& t : tables) {
        const auto a = Proc<const FARPROC*>(original, t.name);
        const auto b = Proc<const FARPROC*>(rebuilt, t.name);
        if (a == b || !a[-1] || a[-1] != b[-1]) {
            std::printf("bad RTTI locator: %s\n", t.name);
            return false;
        }
        for (unsigned i = 0; i < t.slots; ++i) {
            MEMORY_BASIC_INFORMATION mbi{};
            const bool pure = (t.pure & (1u << i)) != 0;
            if (pure ? b[i] != a[i] :
                !VirtualQuery(reinterpret_cast<const void*>(b[i]), &mbi, sizeof mbi) || mbi.AllocationBase != rebuilt) {
                std::printf("bad vtable slot: %s[%u]\n", t.name, i);
                return false;
            }
        }
    }
    std::puts("7 vtables: RTTI and slot ownership checked");
    return true;
}

int main(int argc, char** argv)
{
    // Resolve even a relative bin directory before loading DLLs next to their dependencies.
    char bin[MAX_PATH];
    const auto len = argc == 2 ? GetFullPathNameA(argv[1], MAX_PATH, bin, nullptr) : 0;
    if (!len || len >= MAX_PATH || !SetCurrentDirectoryA(bin) || !SetDllDirectoryA(bin)) {
        std::puts("usage: diff_rs_u8.exe <bin dir>");
        return 2;
    }
    HMODULE original = LoadLibraryA("RenderSystem_7c_orig.dll");
    if (!original) {
        std::printf("cannot load original: %lu\n", GetLastError());
        return 2;
    }
    HMODULE rebuilt = LoadLibraryA("RenderSystem_7c.dll");
    if (!rebuilt) {
        std::printf("cannot load replacement: %lu\n", GetLastError());
        return 2;
    }
    const Api orig(original), ours(rebuilt);
    if (!CheckVtables(original, rebuilt)) return 1;
    int cases = 0, mismatches = 0;
    for (unsigned flag = 0; flag < 256; ++flag) {
        for (bool enabled : {false, true}) {
            OverlayBytes a;
            std::memset(a.data, 0xa5, sizeof a.data);
            const int priority = static_cast<int>(flag) * 1000000 - 128000000;
            std::memcpy(a.data + 0x10, &priority, sizeof priority);
            a.data[0x14] = a.data[0x20] = static_cast<unsigned char>(flag);
            const Rect input{priority, -priority - 1, 0, 0x7fffffff};
            OverlayBytes b = a;
            bool match = ours.getPriority(&a) == orig.getPriority(&b) &&
                         ours.isVisible(&a) == orig.isVisible(&b) &&
                         ours.isGroup(&a) == orig.isGroup(&b);
            ours.setVisible(&a, enabled);
            orig.setVisible(&b, enabled);
            match &= std::memcmp(&a, &b, sizeof a) == 0 && ours.isVisible(&a) == enabled;
            ours.setViewport(&a, enabled);
            orig.setViewport(&b, enabled);
            match &= std::memcmp(&a, &b, sizeof a) == 0;
            ours.setClipping(&a, &input);
            orig.setClipping(&b, &input);
            match &= std::memcmp(&a, &b, sizeof a) == 0;
            Rect ra{}, rb{};
            const auto pa = ours.getClipping(&a, &ra);
            const auto pb = orig.getClipping(&b, &rb);
            match &= pa == &ra && pb == &rb && std::memcmp(&ra, &rb, sizeof ra) == 0 &&
                     std::memcmp(&ra, &input, sizeof ra) == 0;
            ++cases;
            if (!match) {
                if (mismatches < 8) std::printf("mismatch: flags=%02x enabled=%d\n", flag, enabled);
                ++mismatches;
            }
        }
    }
    std::printf("diff_rs_u8: %d cases, %d mismatches\n", cases, mismatches);
    const auto expected = GroupTrace(original);
    const auto actual = GroupTrace(rebuilt);
    if (expected != actual) {
        size_t first = 0;
        while (first < expected.size() && first < actual.size() && expected[first] == actual[first]) ++first;
        std::printf("layer group mismatch at trace entry %zu (original %zu entries, rebuilt %zu)\n",
                    first, expected.size(), actual.size());
        if (first < expected.size() && first < actual.size())
            std::printf("expected %d, got %d\n", expected[first], actual[first]);
        return 1;
    }
    std::printf("layer group: %zu matching trace entries\n", actual.size());
    const auto lifetimeExpected = LifetimeTrace(original);
    const auto lifetimeActual = LifetimeTrace(rebuilt);
    if (lifetimeExpected != lifetimeActual) {
        size_t first = 0;
        while (first < lifetimeExpected.size() && first < lifetimeActual.size() &&
               lifetimeExpected[first] == lifetimeActual[first]) ++first;
        std::printf("layer lifetime mismatch at trace entry %zu (%zu original, %zu rebuilt)\n",
                    first, lifetimeExpected.size(), lifetimeActual.size());
        if (first < lifetimeExpected.size() && first < lifetimeActual.size())
            std::printf("expected %d, got %d\n", lifetimeExpected[first], lifetimeActual[first]);
        return 1;
    }
    std::printf("layer lifetime: %zu matching trace entries\n", lifetimeActual.size());
    return mismatches != 0;
}
