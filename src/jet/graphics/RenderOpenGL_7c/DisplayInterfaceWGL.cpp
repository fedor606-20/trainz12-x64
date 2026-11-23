// RenderOpenGL_7c: a minimal WGL context for the local backend (unit 2).
#include "DisplayInterfaceWGL.h"

#include <windows.h>

namespace Jet {

DisplayInterfaceWGL::DisplayInterfaceWGL()
    : window(nullptr), pixelFormat(0), deviceContext(nullptr), renderContext(nullptr)
{
}

DisplayInterfaceWGL::~DisplayInterfaceWGL() { Destroy(); }

bool __cdecl DisplayInterfaceWGL::Create(void* rawWindow, void*)
{
    Destroy();
    window = static_cast<Window*>(rawWindow);
    if (!window || !window->Handle()) return false;

    HWND const hwnd = window->Handle();
    HDC const dc = GetDC(hwnd);
    if (!dc) return false;

    PIXELFORMATDESCRIPTOR want{};
    want.nSize = sizeof want;
    want.nVersion = 1;
    want.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    want.iPixelType = PFD_TYPE_RGBA;
    want.cColorBits = 32;
    want.cDepthBits = 24;
    want.cStencilBits = 8;
    want.iLayerType = PFD_MAIN_PLANE;

    const int format = ChoosePixelFormat(dc, &want);
    if (!format || !SetPixelFormat(dc, format, &want)) {
        ReleaseDC(hwnd, dc);
        return false;
    }
    HGLRC const rc = wglCreateContext(dc);
    if (!rc || !wglMakeCurrent(dc, rc)) {
        if (rc) wglDeleteContext(rc);
        ReleaseDC(hwnd, dc);
        return false;
    }
    pixelFormat = format;
    deviceContext = dc;
    renderContext = rc;
    return true;
}

void __cdecl DisplayInterfaceWGL::Destroy()
{
    if (renderContext) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(static_cast<HGLRC>(renderContext));
        renderContext = nullptr;
    }
    if (deviceContext && window && window->Handle()) {
        ReleaseDC(window->Handle(), static_cast<HDC>(deviceContext));
    }
    deviceContext = nullptr;
    pixelFormat = 0;
    window = nullptr;
}

void* __cdecl DisplayInterfaceWGL::GetDeviceContext() const { return deviceContext; }
void* __cdecl DisplayInterfaceWGL::GetRenderContext() const { return renderContext; }

void __cdecl DisplayInterfaceWGL::SwapBuffers()
{
    if (deviceContext) ::SwapBuffers(static_cast<HDC>(deviceContext));
}

Window* __cdecl DisplayInterfaceWGL::GetWindow() const { return window; }
int __cdecl DisplayInterfaceWGL::GetPixelFormat() const { return pixelFormat; }

}  // namespace Jet
