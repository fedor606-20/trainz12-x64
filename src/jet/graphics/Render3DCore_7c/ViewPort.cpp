// Unit U8 of Render3DCore_7c: Jet::ViewPort, transcribed from the TS12 assembly.
//
// 0xf4 bytes; see Render3DCoreCamera.h for the offsets. The Layer base is the local RenderSystem implementation:
// its constructor/destructor/assignment and ConvertToWindow are called by name. The embedded ViewPoint (+0x84,
// 0x70 bytes) belongs to the U9 scene unit; only its block is touched here and its heap children are the
// original's. Rendering/traversal slots (Begin, End, Render, Update, RenderLayer, hit tests) still forward to
// Render3DCore_7c_orig.dll, because they walk ViewPoint and the caches, which are other units.
#include <jet/Render3DCoreCamera.h>
#include "SimpleRemain.h"

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

using namespace SolidsDetail;

HMODULE OrigModule()
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("Render3DCore_7c_orig.dll");
        return m ? m : LoadLibraryA("Render3DCore_7c_orig.dll");
    }();
    return module;
}

template <class F>
F OrigProc(const char* name)
{
    HMODULE m = OrigModule();
    return m ? reinterpret_cast<F>(GetProcAddress(m, name)) : nullptr;
}

float X87CopyRet(float x)
{
    float r;
    X87Copy(r, x);
    return r;
}

void AssignEmbeddedViewPoint(void* dst, const void* src)
{
    *static_cast<ViewPoint*>(dst) = *static_cast<const ViewPoint*>(src);
}

void DropViewPoint(void* viewPoint)
{
    if (!viewPoint) return;
    static_cast<ViewPoint*>(viewPoint)->~ViewPoint();
    Alloc::Delete(viewPoint);
}

}  // namespace

unsigned long ViewPort::numViewports = 0;
unsigned long ViewPort::traversalNumber = 0;

ViewPort::ViewPort()
    : Layer(nullptr, PString("Not initialised"), 0, false)
{
    camera = nullptr;
    renderFlags = 1;
    userData = 0;
    viewportNumber = numViewports;
    ++numViewports;
    renderHint = RENDER_DEFAULT;
    windowViewport = true;
    closestDistance = 1.0f;
    InitializeViewPoint();
    Initialize(nullptr, nullptr);
}

ViewPort::ViewPort(Camera* camera_, LayerGroup* group)
    : Layer(group, PString("ViewPort"), 0, true)
{
    camera = nullptr;
    renderFlags = 1;
    userData = 0;
    viewportNumber = numViewports;
    ++numViewports;
    renderHint = RENDER_DEFAULT;
    windowViewport = true;
    closestDistance = 1.0f;
    InitializeViewPoint();

    // The original reads this->group (the Layer ctor resolves a null group to the manager root).
    Rect clipped{};
    LayerGroup* resolved = GetParent();
    if (resolved) resolved->GetClippedGroupViewRect(clipped);
    Initialize(camera_, &clipped);
}

ViewPort::ViewPort(Camera* camera_, const Rect& rect, LayerGroup* group)
    : Layer(group, PString("ViewPort"), 0, true)
{
    camera = nullptr;
    renderFlags = 1;
    userData = 0;
    viewportNumber = numViewports;
    ++numViewports;
    renderHint = RENDER_DEFAULT;
    windowViewport = true;
    closestDistance = 1.0f;
    InitializeViewPoint();
    Initialize(camera_, &rect);
}

ViewPort::ViewPort(const ViewPort& o) : Layer(o)
{
    camera = o.camera;
    extents = o.extents;
    clearColor.r = X87CopyRet(o.clearColor.r);
    clearColor.g = X87CopyRet(o.clearColor.g);
    clearColor.b = X87CopyRet(o.clearColor.b);
    clearColor.a = X87CopyRet(o.clearColor.a);
    clearDepth = X87CopyRet(o.clearDepth);
    clearStencil = o.clearStencil;
    clearMode = o.clearMode;
    renderFlags = o.renderFlags;
    userData = o.userData;
    viewportNumber = o.viewportNumber;
    windowViewport = o.windowViewport;
    renderHint = o.renderHint;
    alphaMaskReference = X87CopyRet(o.alphaMaskReference);
    viewportFlags = o.viewportFlags;
    closestDistance = X87CopyRet(o.closestDistance);
    aspectX = X87CopyRet(o.aspectX);
    aspectY = X87CopyRet(o.aspectY);
    visibleList = o.visibleList;
    std::memcpy(viewPoint, o.viewPoint, sizeof viewPoint);
}

ViewPort& ViewPort::operator=(const ViewPort& o)
{
    if (this == &o) return *this;
    Layer::operator=(o);
    camera = o.camera;
    extents = o.extents;
    clearColor.r = X87CopyRet(o.clearColor.r);
    clearColor.g = X87CopyRet(o.clearColor.g);
    clearColor.b = X87CopyRet(o.clearColor.b);
    clearColor.a = X87CopyRet(o.clearColor.a);
    clearDepth = X87CopyRet(o.clearDepth);
    clearStencil = o.clearStencil;
    clearMode = o.clearMode;
    renderFlags = o.renderFlags;
    userData = o.userData;
    viewportNumber = o.viewportNumber;
    windowViewport = o.windowViewport;
    renderHint = o.renderHint;
    alphaMaskReference = X87CopyRet(o.alphaMaskReference);
    viewportFlags = (viewportFlags & ~1u) | (o.viewportFlags & 1u);
    closestDistance = X87CopyRet(o.closestDistance);
    aspectX = X87CopyRet(o.aspectX);
    aspectY = X87CopyRet(o.aspectY);
    visibleList = o.visibleList;
    AssignEmbeddedViewPoint(viewPoint, o.viewPoint);
    return *this;
}

ViewPort::~ViewPort()
{
    DropViewPoint(*reinterpret_cast<void**>(viewPoint + 0x10));  // +0x94
    DropViewPoint(*reinterpret_cast<void**>(viewPoint + 0x0c));  // +0x90
    visibleList.count = 0;
    if (visibleList.data) {
        Alloc::Delete(visibleList.data);
        visibleList.data = nullptr;
    }
}

void ViewPort::InitializeViewPoint()
{
    void** vp = reinterpret_cast<void**>(viewPoint);
    vp[0] = Kernel();                  // +0x84
    vp[1] = nullptr;                   // +0x88
    vp[2] = &visibleList;              // +0x8c
    vp[3] = nullptr;                   // +0x90
    vp[4] = nullptr;                   // +0x94
    vp[5] = nullptr;                   // +0x98
    vp[6] = this;                      // +0x9c
    vp[0x5c / 4] = nullptr;            // +0xe0
    vp[0x60 / 4] = nullptr;            // +0xe4
    vp[0x64 / 4] = nullptr;            // +0xe8
    vp[0x68 / 4] = nullptr;            // +0xec
    *reinterpret_cast<float*>(viewPoint + 0x6c) = 0.0f;  // +0xf0
}

void __cdecl ViewPort::Initialize(Camera* camera_, const Rect* rect)
{
    camera = camera_;
    if (rect) {
        extents = *rect;
        if (extents.h < extents.w) {
            aspectX = static_cast<float>(extents.h) / static_cast<float>(extents.w);
            aspectY = 1.0f;
        } else {
            aspectX = 1.0f;
            aspectY = static_cast<float>(extents.w) / static_cast<float>(extents.h);
        }
    } else {
        aspectX = 1.0f;
        aspectY = 1.0f;
        extents = Rect{0, 0, 0, 0};
    }

    clearColor = Color{0.2f, 0.2f, 0.2f, 1.0f};
    viewportFlags &= ~1u;
    SetVisible(true);
    clearDepth = 1.0f;
    clearStencil = 0;
    clearMode = 7;
    windowViewport = true;
    closestDistance = 1.0f;

    RenderKernel* kernel = Kernel();
    OS* os = kernel ? kernel->GetOS() : nullptr;
    Options* options = os ? os->GetOptions() : nullptr;
    if (options) options->Get("AlphaMaskValue", alphaMaskReference, 0.0f);
}

void __cdecl ViewPort::SetLocalPosition(int x, int y)
{
    extents.x = x;
    extents.y = y;
}

void __cdecl ViewPort::GetLocalPosition(int& x, int& y) const
{
    x = extents.x;
    y = extents.y;
}

void __cdecl ViewPort::GetPosition(int& x, int& y) const
{
    Point p{extents.x, extents.y};
    ConvertToWindow(p);
    x = p.x;
    y = p.y;
}

void __cdecl ViewPort::SetSize(int w, int h)
{
    extents.w = w;
    extents.h = h;
    if (h < w) {
        aspectX = static_cast<float>(h) / static_cast<float>(w);
        aspectY = 1.0f;
    } else {
        aspectX = 1.0f;
        aspectY = static_cast<float>(w) / static_cast<float>(h);
    }
}

void __cdecl ViewPort::GetSize(int& w, int& h) const
{
    w = extents.w;
    h = extents.h;
}

void __cdecl ViewPort::SetLetterBoxMode(bool enabled)
{
    if ((viewportFlags & 1u) == (enabled ? 1u : 0u)) return;
    viewportFlags ^= ((enabled ? 1u : 0u) ^ viewportFlags) & 1u;
    Camera* cam = camera;
    if (!cam) return;
    if (viewportFlags & 1u) {
        const int height = static_cast<int>(static_cast<float>(extents.h) * 0.5625f + 0.5f);
        const int inset = static_cast<int>(static_cast<float>(extents.h - height) * 0.5f + 0.5f);
        extents.y += inset;
        extents.h = height;
        cam->SetPerspective(cam->GetVerticalFOV(), 0.5625f);
    } else {
        const int height = static_cast<int>(static_cast<float>(extents.h) * 1.7777778f + 0.5f);
        const int delta = height - extents.h;
        extents.y += static_cast<int>(-0.5f - static_cast<float>(delta) * 0.5f);
        extents.h = height;
        const float aspect = static_cast<float>(extents.w) / static_cast<float>(extents.h);
        cam->SetPerspective(cam->GetVerticalFOV(), aspect);
    }
}

void __cdecl ViewPort::Translate(int dx, int dy)
{
    extents.x += dx;
    extents.y += dy;
}

void __cdecl ViewPort::Scale(float scale)
{
    extents.w = static_cast<int>(static_cast<float>(extents.w) * scale);
    extents.h = static_cast<int>(static_cast<float>(extents.h) * scale);
}

void __cdecl ViewPort::Scale(float sx, float sy)
{
    extents.w = static_cast<int>(static_cast<float>(extents.w) * sx);
    extents.h = static_cast<int>(static_cast<float>(extents.h) * sy);
}

void __cdecl ViewPort::Transform(Vector3& v)
{
    Point p{extents.x, extents.y};
    ConvertToWindow(p);
    const float x = AddSS(MulSS(static_cast<float>(extents.w - 1), v.x), static_cast<float>(p.x));
    const float y = AddSS(MulSS(static_cast<float>(extents.h - 1), SubSS(1.0f, v.y)), static_cast<float>(p.y));
    v.x = x;
    v.y = y;
}

bool __cdecl ViewPort::CanRender()
{
    RenderKernel* kernel = Kernel();
    Display* display = kernel ? kernel->GetDisplay() : nullptr;
    if (!display) return false;
    const Size& size = display->GetTargetSize();
    return extents.x < size.w && extents.y < size.h && extents.w > 0 && extents.h > 0;
}

bool __cdecl ViewPort::Collides(const Vector2&) const
{
    return true;
}

bool __cdecl ViewPort::Collides(const Vector2&, const Vector2&) const
{
    return true;
}

void __cdecl ViewPort::Clear()
{
}

void __cdecl ViewPort::SetClearColor(const Color& color)
{
    clearColor = color;
}

void __cdecl ViewPort::SetClearDepth(float depth)
{
    clearDepth = depth;
}

void __cdecl ViewPort::SetClearStencil(long value)
{
    clearStencil = static_cast<unsigned long>(value);
}

void __cdecl ViewPort::SetClearMode(unsigned long mode)
{
    clearMode = mode;
}

const Color& __cdecl ViewPort::GetClearColor() const
{
    return clearColor;
}

float __cdecl ViewPort::GetClearDepth() const
{
    return clearDepth;
}

unsigned long __cdecl ViewPort::GetClearStencil() const
{
    return clearStencil;
}

unsigned long __cdecl ViewPort::GetClearMode() const
{
    return clearMode;
}

const Rect& __cdecl ViewPort::GetLocalExtents() const
{
    return extents;
}

void __cdecl ViewPort::GetExtents(Rect& rect) const
{
    rect = extents;
    ConvertToWindow(reinterpret_cast<Point&>(rect));  // the original converts the x/y pair of the rect
}

int __cdecl ViewPort::GetLocalX() const
{
    return extents.x;
}

int __cdecl ViewPort::GetX() const
{
    Point p{extents.x, extents.y};
    ConvertToWindow(p);
    return p.x;
}

int __cdecl ViewPort::GetLocalY() const
{
    return extents.y;
}

int __cdecl ViewPort::GetY() const
{
    Point p{extents.x, extents.y};
    ConvertToWindow(p);
    return p.y;
}

int __cdecl ViewPort::GetWidth() const
{
    return extents.w;
}

int __cdecl ViewPort::GetHeight() const
{
    return extents.h;
}

void __cdecl ViewPort::SetCamera(Camera* value)
{
    camera = value;
}

Camera* __cdecl ViewPort::GetCamera() const
{
    return camera;
}

bool __cdecl ViewPort::GetLetterBoxMode() const
{
    return (viewportFlags & 1u) != 0;
}

void __cdecl ViewPort::AdjustAspectRatio(float& x, float& y) const
{
    x = MulSS(aspectX, x);
    y = MulSS(aspectY, y);
}

const Array<VisibleElement, 128>& __thiscall ViewPort::GetVisibleList() const
{
    return visibleList;
}

void __cdecl ViewPort::WindowSpaceConvert(Rect& rect) const
{
    if (!windowViewport) return;
    RenderKernel* kernel = Kernel();
    Display* display = kernel ? kernel->GetDisplay() : nullptr;
    if (!display) return;
    const Size& size = display->GetTargetSize();
    rect.y = (size.h - rect.h) - rect.y;
}

void __cdecl ViewPort::SetUserData(unsigned long value)
{
    userData = value;
}

unsigned long __cdecl ViewPort::GetUserData() const
{
    return userData;
}

void __cdecl ViewPort::SetRenderHint(RenderHint hint)
{
    renderHint = hint;
}

RenderHint __cdecl ViewPort::GetRenderHint() const
{
    return renderHint;
}

void __cdecl ViewPort::SetRenderFlags(unsigned long flags)
{
    renderFlags = flags;
}

unsigned long __cdecl ViewPort::GetRenderFlags() const
{
    return renderFlags;
}

unsigned long __cdecl ViewPort::GetViewportNumber() const
{
    return viewportNumber;
}

void __cdecl ViewPort::SetWindowViewport(bool enabled)
{
    windowViewport = enabled;
}

bool __cdecl ViewPort::GetWindowViewport() const
{
    return windowViewport;
}

void __cdecl ViewPort::SetAlphaMaskReference(float value)
{
    alphaMaskReference = value;
}

float __cdecl ViewPort::GetAlphaMaskReference() const
{
    return alphaMaskReference;
}

unsigned long __cdecl ViewPort::GetTraversalNumber()
{
    return traversalNumber;
}

void __cdecl ViewPort::ViewTransformPoint(Vector3& point)
{
    if (!camera) {
        point.x = point.y = point.z = 0.0f;
        return;
    }
    const Matrix4& m = camera->GetGlobalViewTransform();
    const float w = AddSS(AddSS(AddSS(MulSS(m.m[11], point.z), MulSS(m.m[7], point.y)),
                                MulSS(m.m[3], point.x)),
                          m.m[15]);
    const float inverse = DivSS(1.0f, w);
    const float x = MulSS(AddSS(AddSS(AddSS(MulSS(m.m[8], point.z), MulSS(m.m[4], point.y)),
                                     MulSS(m.m[0], point.x)),
                               m.m[12]),
                          inverse);
    const float y = MulSS(AddSS(AddSS(AddSS(MulSS(m.m[9], point.z), MulSS(m.m[5], point.y)),
                                     MulSS(m.m[1], point.x)),
                               m.m[13]),
                          inverse);
    const float z = MulSS(AddSS(AddSS(AddSS(MulSS(m.m[10], point.z), MulSS(m.m[6], point.y)),
                                     MulSS(m.m[2], point.x)),
                               m.m[14]),
                          inverse);
    point.x = x;
    point.y = y;
    point.z = z;

    const float depth = SubSS(0.0f, z);
    if (depth > camera->GetNearClipDistance()) {
        const float savedDepth = depth;
        camera->Perspective(point);
        Transform(point);
        point.z = savedDepth;
        RenderKernel* kernel = Kernel();
        Display* display = kernel ? kernel->GetDisplay() : nullptr;
        if (display) {
            const Size& size = display->GetTargetSize();
            point.x = SubSS(SubSS(static_cast<float>(size.w), point.x), 1.0f);
            point.y = SubSS(SubSS(static_cast<float>(size.h), point.y), 1.0f);
        }
    } else {
        point.z = -1.0f;
    }
}

// ---- slots that still belong to the U9 scene / render caches ------------------------------------------------

void __cdecl ViewPort::RenderLayer(SystemResourceViewerList& list)
{
    using Fn = void(__cdecl*)(ViewPort*, SystemResourceViewerList&);
    static Fn fn = OrigProc<Fn>("?RenderLayer@ViewPort@Jet@@UAAXAAV?$FastDynamicArray@PAVSystemResourceViewer@Jet@@$0N@U?$Less@PAVSystemResourceViewer@Jet@@PAV12@@2@@2@@Z");
    if (fn) fn(this, list);
}

void __cdecl ViewPort::Begin()
{
    using Fn = void(__cdecl*)(ViewPort*);
    static Fn fn = OrigProc<Fn>("?Begin@ViewPort@Jet@@UAAXXZ");
    if (fn) fn(this);
}

void __cdecl ViewPort::End()
{
    using Fn = void(__cdecl*)(ViewPort*);
    static Fn fn = OrigProc<Fn>("?End@ViewPort@Jet@@UAAXXZ");
    if (fn) fn(this);
}

void __cdecl ViewPort::Render()
{
    using Fn = void(__cdecl*)(ViewPort*);
    static Fn fn = OrigProc<Fn>("?Render@ViewPort@Jet@@UAAXXZ");
    if (fn) fn(this);
}

void __cdecl ViewPort::Update(float dt)
{
    using Fn = void(__cdecl*)(ViewPort*, float);
    static Fn fn = OrigProc<Fn>("?Update@ViewPort@Jet@@UAAXM@Z");
    if (fn) fn(this, dt);
}

Element* __cdecl ViewPort::GetElementAt(const Vector2& point)
{
    using Fn = Element*(__cdecl*)(ViewPort*, const Vector2&);
    static Fn fn = OrigProc<Fn>("?GetElementAt@ViewPort@Jet@@UAAPAVElement@2@ABVVector2@2@@Z");
    return fn ? fn(this, point) : nullptr;
}

void __cdecl ViewPort::GetAllWithin(const Vector2& a, const Vector2& b, ElementPtrList& elements)
{
    using Fn = void(__cdecl*)(ViewPort*, const Vector2&, const Vector2&, ElementPtrList&);
    static Fn fn = OrigProc<Fn>("?GetAllWithin@ViewPort@Jet@@UAAXABVVector2@2@0AAV?$FastDynamicArray@PAVElement@Jet@@$0N@U?$Less@PAVElement@Jet@@PAV12@@2@@2@@Z");
    if (fn) fn(this, a, b, elements);
}

}  // namespace Jet
