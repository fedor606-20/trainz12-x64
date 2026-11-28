// Unit U8 of Render3DCore_7c: Jet::DebugViewPort, transcribed from the TS12 assembly.
//
// 0xfc bytes: the ViewPort base, the source viewport at +0xf4 and a flag word at +0xf8. The constructor builds its
// own default Camera, places it 10 m below and 30 m behind (along +z) the source camera, and points it at the
// source camera position. Update/Render walk the scene and the display state, which other units own, so they
// forward to Render3DCore_7c_orig.dll.
#include <jet/Render3DCoreCamera.h>

#include <windows.h>

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

}  // namespace

DebugViewPort::DebugViewPort(ViewPort* source)
    : ViewPort(static_cast<Camera*>(nullptr), static_cast<LayerGroup*>(nullptr))
{
    // The base ViewPort constructor ran with no camera; install the debug camera now.
    Camera* debugCamera = static_cast<Camera*>(Alloc::New(sizeof(Camera)));
    if (debugCamera) ::new (debugCamera) Camera();
    SetCamera(debugCamera);

    sourceViewPort = source;
    debugFlags = 0;
    SetName(PString("DebugViewport"));

    Camera* sourceCamera = source ? source->GetCamera() : nullptr;
    if (sourceCamera && debugCamera) {
        const Vector3 p = sourceCamera->GetPosition();
        const Vector3 placed{AddSS(p.x, 0.0f), SubSS(p.y, 10.0f), AddSS(p.z, 30.0f)};
        debugCamera->SetPosition(placed);
        const Vector3 target{p.z, p.x, SubSS(p.y, 10.0f)};
        const Vector3 up{0.0f, 0.0f, 1.0f};
        debugCamera->AlignToTarget(target, up);
    }

    clearColor.r = 0.2f;
    clearColor.g = 0.0f;
    clearColor.b = 0.0f;
    clearColor.a = 1.0f;
    SetPriority(1);
}

DebugViewPort::DebugViewPort(const DebugViewPort& o) : ViewPort(o)
{
    sourceViewPort = o.sourceViewPort;
    debugFlags = o.debugFlags;
}

DebugViewPort& DebugViewPort::operator=(const DebugViewPort& o)
{
    if (this == &o) return *this;
    ViewPort::operator=(o);
    sourceViewPort = o.sourceViewPort;
    debugFlags = o.debugFlags;
    return *this;
}

DebugViewPort::~DebugViewPort()
{
    if (Camera* cam = GetCamera()) VDelete(cam);
}

ViewPort* __cdecl DebugViewPort::GetViewPort() const
{
    return sourceViewPort;
}

void __cdecl DebugViewPort::Update(float dt)
{
    using Fn = void(__cdecl*)(DebugViewPort*, float);
    static Fn fn = OrigProc<Fn>("?Update@DebugViewPort@Jet@@UAAXM@Z");
    if (fn) fn(this, dt);
}

void __cdecl DebugViewPort::Render()
{
    using Fn = void(__cdecl*)(DebugViewPort*);
    static Fn fn = OrigProc<Fn>("?Render@DebugViewPort@Jet@@UAAXXZ");
    if (fn) fn(this);
}

}  // namespace Jet
