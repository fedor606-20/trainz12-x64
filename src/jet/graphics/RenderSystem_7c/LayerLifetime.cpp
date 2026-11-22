// Win32 lifetime exports use compatibility tables with retail RTTI and local virtual methods.
// Fastcall adapters take the incoming this in ECX and ignore EDX, matching the exported thiscall stack.
#include <jet/RenderSystem.h>

#include <windows.h>

#include <cstring>
#include <new>

// Keep a dedicated word immediately before each exported array for the MSVC RTTI locator.
#define RS_LAYER_VTABLE(sec, name, slots)                        \
    __pragma(section(sec, read, write))                         \
    __declspec(allocate(sec)) void* name##Meta;                 \
    __declspec(allocate(sec)) void* name[slots]
RS_LAYER_VTABLE("RsVtLay", RsLayerVftable, 11);
RS_LAYER_VTABLE("RsVtLayG", RsLayerGroupVftable, 11);
RS_LAYER_VTABLE("RsVtOvl", RsOverlayVftable, 11);
RS_LAYER_VTABLE("RsVtClr", RsClearLayerVftable, 11);
RS_LAYER_VTABLE("RsVtSRV", RsResourceViewerVftable, 3);
RS_LAYER_VTABLE("RsVtSRM", RsResourceManagerVftable, 5);
RS_LAYER_VTABLE("RsVtLayM", RsLayerManagerVftable, 2);
#undef RS_LAYER_VTABLE

namespace Jet {

LayerManager* SharedLayerManager();
LayerGroup* LayerManagerRoot(LayerManager* manager);
Size LayerTargetSize(const Display* display);

namespace {
enum class Table { Layer, Group, Overlay, Clear, Viewer, ResourceManager };

void SetTable(void* object, Table table)
{
    static void** const tables[]{
        RsLayerVftable, RsLayerGroupVftable, RsOverlayVftable, RsClearLayerVftable,
        RsResourceViewerVftable, RsResourceManagerVftable,
    };
    *static_cast<void***>(object) = tables[static_cast<unsigned>(table)];
}

template <class T> T& Field(void* p, unsigned offset)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(p) + offset);
}

template <class T> const T& Field(const void* p, unsigned offset)
{
    return *reinterpret_cast<const T*>(static_cast<const unsigned char*>(p) + offset);
}

void CopyBits(unsigned char& dest, unsigned char src, unsigned mask)
{
    dest = static_cast<unsigned char>((dest & ~mask) | (src & mask));
}
}

struct LayerLifetimeAccess {
    static void Init(Layer& obj, const PString* name, int priority, bool visible,
                     LayerGroup* group, bool implicitRoot)
    {
        obj.link = {};
        SetTable(&obj, Table::Layer);
        if (name) new (&obj.name) PString(*name);
        else new (&obj.name) PString("Not initialised");
        CopyBits(obj.flags, visible ? 1 : 0, 3);
        obj.priority = priority;
        const auto manager = SharedLayerManager();
        if (implicitRoot) {
            obj.kernel = VCall<RenderKernel*>(manager, 0x04);
            obj.group = LayerManagerRoot(manager);
            obj.group->Add(&obj);
        } else {
            obj.kernel = nullptr;
            obj.group = group;
            if (manager) {
                if (!obj.group) obj.group = LayerManagerRoot(manager);
                obj.group->Add(&obj);
                obj.kernel = VCall<RenderKernel*>(manager, 0x04);
            }
        }
    }

    static void SetName(Layer& obj, const PString& name)
    {
        ReplaceName(obj.name, name);
    }

    static void ReplaceName(PString& dest, const PString& source)
    {
        // Retail assignment releases and retains even when the two nodes compare equal.
        dest.~PString();
        new (&dest) PString(source);
    }

    static void Copy(Layer& obj, const Layer& source, bool construct)
    {
        obj.link = source.link;
        if (construct) {
            SetTable(&obj, Table::Layer);
            new (&obj.name) PString(source.name);
        } else {
            SetName(obj, source.name);
        }
        obj.priority = source.priority;
        CopyBits(obj.flags, source.flags, 3);
        obj.kernel = source.kernel;
        obj.group = source.group;
    }

    static void Drop(Layer& obj)
    {
        SetTable(&obj, Table::Layer);
        if (obj.group) obj.group->Remove(&obj);
        obj.name.~PString();
    }

    static void InitGroup(LayerGroup& obj)
    {
        SetTable(&obj, Table::Group);
        obj.sentinel = {&obj.sentinel, &obj.sentinel};
        obj.count = 0;
        obj.view = {};
        obj.flags |= 2;
    }

    static void CopyGroup(LayerGroup& obj, const LayerGroup& source, bool construct)
    {
        Copy(obj, source, construct);
        if (construct) SetTable(&obj, Table::Group);
        obj.sentinel = source.sentinel;
        obj.count = source.count;
        obj.view = source.view;
    }

    static void DropGroup(LayerGroup& obj)
    {
        SetTable(&obj, Table::Group);
        while (obj.count) {
            auto* child = obj.FromLink(obj.sentinel.next);
            obj.Unlink(child);
            VDelete(child);
        }
        Drop(obj);
    }

    static void InitViewer(SystemResourceViewer& obj, RenderKernel* kernel, const PString& name)
    {
        SetTable(&obj, Table::Viewer);
        obj.kernel = kernel;
        new (&obj.name) PString(name);
    }

    static void CopyViewer(SystemResourceViewer& obj, const SystemResourceViewer& source, bool construct)
    {
        if (construct) InitViewer(obj, source.kernel, source.name);
        else {
            obj.kernel = source.kernel;
            obj.SetName(source.name);
        }
    }

    static void DropViewer(SystemResourceViewer& obj)
    {
        SetTable(&obj, Table::Viewer);
        obj.name.~PString();
    }
};

using Life = LayerLifetimeAccess;

void __cdecl Layer::SetName(const PString& name) { Life::SetName(*this, name); }
LayerManager* __cdecl Layer::GetLayerManager() { return SharedLayerManager(); }

void* __fastcall RsLayerDefault(void* p, void*)
{
    Life::Init(*static_cast<Layer*>(p), nullptr, 0, false, nullptr, true);
    return p;
}
void* __fastcall RsLayerNamed(void* p, void*, const void* name, int priority, bool visible)
{
    Life::Init(*static_cast<Layer*>(p), static_cast<const PString*>(name), priority, visible, nullptr, true);
    return p;
}
void* __fastcall RsLayerGrouped(void* p, void*, void* group, const void* name, int priority, bool visible)
{
    Life::Init(*static_cast<Layer*>(p), static_cast<const PString*>(name), priority, visible, static_cast<LayerGroup*>(group), false);
    return p;
}
void* __fastcall RsLayerCopy(void* p, void*, const void* source)
{
    Life::Copy(*static_cast<Layer*>(p), *static_cast<const Layer*>(source), true);
    return p;
}
void* __fastcall RsLayerAssign(void* p, void*, const void* source)
{
    Life::Copy(*static_cast<Layer*>(p), *static_cast<const Layer*>(source), false);
    return p;
}
void __fastcall RsLayerDrop(void* p, void*) { Life::Drop(*static_cast<Layer*>(p)); }

void* __fastcall RsGroupDefault(void* p, void*)
{
    RsLayerDefault(p, nullptr);
    Life::InitGroup(*static_cast<LayerGroup*>(p));
    return p;
}
void* __fastcall RsGroupGrouped(void* p, void*, void* group, const void* name, int priority, bool visible)
{
    RsLayerGrouped(p, nullptr, group, name, priority, visible);
    Life::InitGroup(*static_cast<LayerGroup*>(p));
    return p;
}
void* __fastcall RsGroupCopy(void* p, void*, const void* source)
{
    Life::CopyGroup(*static_cast<LayerGroup*>(p), *static_cast<const LayerGroup*>(source), true);
    return p;
}
void* __fastcall RsGroupAssign(void* p, void*, const void* source)
{
    Life::CopyGroup(*static_cast<LayerGroup*>(p), *static_cast<const LayerGroup*>(source), false);
    return p;
}
void __fastcall RsGroupDrop(void* p, void*) { Life::DropGroup(*static_cast<LayerGroup*>(p)); }

namespace {
void CopyOverlay(void* p, const void* source)
{
    CopyBits(Field<unsigned char>(p, 0x20), Field<unsigned char>(source, 0x20), 7);
    Field<Rect>(p, 0x24) = Field<Rect>(source, 0x24);
    CopyBits(Field<unsigned char>(p, 0x34), Field<unsigned char>(source, 0x34), 3);
    Field<unsigned int>(p, 0x38) = Field<unsigned int>(source, 0x38);
}

void InitOverlay(void* p)
{
    Field<unsigned char>(p, 0x34) |= 3;
    CopyBits(Field<unsigned char>(p, 0x20), 0, 6);
    SetTable(p, Table::Overlay);
    Field<unsigned int>(p, 0x38) = 1;
}

void InitClear(void* p)
{
    SetTable(p, Table::Clear);
    Field<Color>(p, 0x20) = {0, 0, 0, 1};
}
}

void* __fastcall RsOverlayDefault(void* p, void*)
{
    RsLayerDefault(p, nullptr);
    SetTable(p, Table::Overlay);
    return p;
}
void* __fastcall RsOverlayNamed(void* p, void*, const void* name, int priority, bool visible)
{
    RsLayerNamed(p, nullptr, name, priority, visible);
    InitOverlay(p);
    const auto size = LayerTargetSize(static_cast<Layer*>(p)->GetRenderKernel()->GetDisplay());
    Field<Rect>(p, 0x24) = {0, 0, size.w, size.h};
    return p;
}
void* __fastcall RsOverlayGrouped(void* p, void*, void* group, const void* name, int priority, bool visible)
{
    RsLayerGrouped(p, nullptr, group, name, priority, visible);
    try {
        InitOverlay(p);
        auto& clip = Field<Rect>(p, 0x24);
        static_cast<Layer*>(p)->GetParent()->GetClippedGroupViewRect(clip);
        clip.x = clip.y = 0;
    } catch (...) {
        RsLayerDrop(p, nullptr);
        throw;
    }
    return p;
}
void* __fastcall RsOverlayCopy(void* p, void*, const void* source)
{
    RsLayerCopy(p, nullptr, source);
    SetTable(p, Table::Overlay);
    CopyOverlay(p, source);
    return p;
}
void* __fastcall RsOverlayAssign(void* p, void*, const void* source)
{
    RsLayerAssign(p, nullptr, source);
    CopyOverlay(p, source);
    return p;
}

void* __fastcall RsClearNamed(void* p, void*, const void* name, int priority, bool visible)
{
    RsLayerNamed(p, nullptr, name, priority, visible);
    InitClear(p);
    return p;
}
void* __fastcall RsClearGrouped(void* p, void*, void* group, const void* name, int priority, bool visible)
{
    RsLayerGrouped(p, nullptr, group, name, priority, visible);
    InitClear(p);
    return p;
}
void __fastcall RsClearDefaultClosure(void* p, void*)
{
    const PString name("ClearLayer");
    RsClearNamed(p, nullptr, &name, 0, true);
}
void* __fastcall RsClearCopy(void* p, void*, const void* source)
{
    RsLayerCopy(p, nullptr, source);
    SetTable(p, Table::Clear);
    Field<Color>(p, 0x20) = Field<Color>(source, 0x20);
    return p;
}
void* __fastcall RsClearAssign(void* p, void*, const void* source)
{
    RsLayerAssign(p, nullptr, source);
    Field<Color>(p, 0x20) = Field<Color>(source, 0x20);
    return p;
}

Color __cdecl ClearLayer::GetClearColor() { return clearColor; }
void __cdecl ClearLayer::SetClearColor(const Color& color) { clearColor = color; }
void __cdecl ClearLayer::Render()
{
    auto* display = GetRenderKernel()->GetDisplay();
    Rect viewport;
    GetParent()->GetClippedGroupViewRect(viewport);
    viewport.y = LayerTargetSize(display).h - viewport.h - viewport.y;
    display->SetViewport(viewport);
    display->SetClearColor(clearColor);
    VCall<void>(display, 0xd0, 7);
}

const PString& __cdecl SystemResourceViewer::GetName() const { return name; }
void __cdecl SystemResourceViewer::SetName(const PString& value) { Life::ReplaceName(name, value); }

void* __fastcall RsViewerCreate(void* p, void*, void* kernel, const void* name)
{
    Life::InitViewer(*static_cast<SystemResourceViewer*>(p), static_cast<RenderKernel*>(kernel),
                     *static_cast<const PString*>(name));
    return p;
}

void* __fastcall RsViewerCopy(void* p, void*, const void* source)
{
    Life::CopyViewer(*static_cast<SystemResourceViewer*>(p), *static_cast<const SystemResourceViewer*>(source), true);
    return p;
}

void* __fastcall RsViewerAssign(void* p, void*, const void* source)
{
    Life::CopyViewer(*static_cast<SystemResourceViewer*>(p), *static_cast<const SystemResourceViewer*>(source), false);
    return p;
}

void __fastcall RsViewerDrop(void* p, void*) { Life::DropViewer(*static_cast<SystemResourceViewer*>(p)); }

void* __fastcall RsResourceManagerCreate(void* p, void*)
{
    SetTable(p, Table::ResourceManager);
    return p;
}
void* __fastcall RsResourceManagerCopy(void* p, void*, const void*) { return RsResourceManagerCreate(p, nullptr); }
void* __fastcall RsResourceManagerAssign(void* p, void*, const void*) { return p; }
void __fastcall RsResourceManagerDrop(void* p, void*) { SetTable(p, Table::ResourceManager); }

void __fastcall RsLayerManagerDrop(void*, void*);

namespace {
using Destructor = void (__fastcall*)(void*, void*);

template <unsigned stride, Destructor drop>
void* __fastcall DeletingDestructor(void* p, void*, unsigned flags)
{
    if (flags & 2) {
        auto* first = static_cast<unsigned char*>(p);
        auto* header = first - sizeof(unsigned);
        // Continue destroying the remaining array elements if a destructor unwinds.
        struct Remaining {
            unsigned char* first;
            unsigned count;
            void Run() { while (count) drop(first + stride * --count, nullptr); }
            ~Remaining() { Run(); }
        } remaining{first, *reinterpret_cast<unsigned*>(header)};
        remaining.Run();
        if (flags & 1) Alloc::Delete(header);
        return header;
    }
    drop(p, nullptr);
    if (flags & 1) Alloc::Delete(p);
    return p;
}

template <size_t n, class F>
void MirrorTable(HMODULE original, void* (&table)[n], void*& meta, const char* name, F deletingDestructor)
{
    const auto* source = reinterpret_cast<void* const*>(GetProcAddress(original, name));
    std::memcpy(table, source, sizeof table);
    meta = source[-1];
    std::memcpy(reinterpret_cast<unsigned char*>(table) - sizeof meta, &meta, sizeof meta);
    table[0] = reinterpret_cast<void*>(deletingDestructor);
}

constexpr const char* baseVirtuals[]{
    nullptr,
    "?RenderLayer@Layer@Jet@@UAAXAAV?$FastDynamicArray@PAVSystemResourceViewer@Jet@@$0N@U?$Less@PAVSystemResourceViewer@Jet@@PAV12@@2@@2@@Z",
    "?UpdateLayer@Layer@Jet@@UAAXM@Z",
    "?Collides@Layer@Jet@@UBA_NABVVector2@2@0@Z",
    "?Collides@Layer@Jet@@UBA_NABVVector2@2@@Z",
    "?Reset@Layer@Jet@@UAAXXZ",
    nullptr, // abstract Render
    "?Update@Layer@Jet@@MAAXM@Z",
    "?Begin@Layer@Jet@@MAAXXZ",
    "?End@Layer@Jet@@MAAXXZ",
    "?CanRender@Layer@Jet@@MAA_NXZ",
};

struct InitializeLayerVtables {
    InitializeLayerVtables()
    {
        const auto original = GetModuleHandleA("RenderSystem_7c_orig.dll");
        HMODULE self = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(RsLayerVftable), &self);
        MirrorTable(original, RsLayerVftable, RsLayerVftableMeta, "??_7Layer@Jet@@6B@",
                    DeletingDestructor<0x20, RsLayerDrop>);
        MirrorTable(original, RsLayerGroupVftable, RsLayerGroupVftableMeta, "??_7LayerGroup@Jet@@6B@",
                    DeletingDestructor<0x3c, RsGroupDrop>);
        MirrorTable(original, RsClearLayerVftable, RsClearLayerVftableMeta, "??_7ClearLayer@Jet@@6B@",
                    DeletingDestructor<0x30, RsLayerDrop>);
        MirrorTable(original, RsOverlayVftable, RsOverlayVftableMeta, "??_7Overlay@Jet@@6B@",
                    DeletingDestructor<0x3c, RsLayerDrop>);
        MirrorTable(original, RsLayerManagerVftable, RsLayerManagerVftableMeta, "??_7LayerManager@Jet@@6B@",
                    DeletingDestructor<0x44, RsLayerManagerDrop>);
        MirrorTable(original, RsResourceViewerVftable, RsResourceViewerVftableMeta, "??_7SystemResourceViewer@Jet@@6B@",
                    DeletingDestructor<0x0c, RsViewerDrop>);
        MirrorTable(original, RsResourceManagerVftable, RsResourceManagerVftableMeta, "??_7SystemResourceManager@Jet@@6B@",
                    DeletingDestructor<4, RsResourceManagerDrop>);
        // These virtuals use the same stack-this __cdecl ABI as their export. Pure-call slots stay retail.
        const auto get = [self](const char* name) { return reinterpret_cast<void*>(GetProcAddress(self, name)); };
        for (unsigned i = 1; i < 11; ++i) {
            if (!baseVirtuals[i]) continue;
            auto* entry = get(baseVirtuals[i]);
            RsLayerVftable[i] = RsLayerGroupVftable[i] = RsClearLayerVftable[i] = RsOverlayVftable[i] = entry;
        }
        RsLayerGroupVftable[1] = get("?RenderLayer@LayerGroup@Jet@@UAAXAAV?$FastDynamicArray@PAVSystemResourceViewer@Jet@@$0N@U?$Less@PAVSystemResourceViewer@Jet@@PAV12@@2@@2@@Z");
        RsLayerGroupVftable[2] = get("?UpdateLayer@LayerGroup@Jet@@UAAXM@Z");
        RsLayerGroupVftable[5] = get("?Reset@LayerGroup@Jet@@UAAXXZ");
        RsLayerGroupVftable[6] = get("?Render@LayerGroup@Jet@@MAAXXZ");
        RsClearLayerVftable[6] = get("?Render@ClearLayer@Jet@@UAAXXZ");
        RsOverlayVftable[8] = get("?Begin@Overlay@Jet@@MAAXXZ");
        RsOverlayVftable[9] = get("?End@Overlay@Jet@@MAAXXZ");
    }
};

#pragma warning(disable : 4073)
#pragma init_seg(lib)
InitializeLayerVtables initializeLayerVtables;
}

} // namespace Jet
