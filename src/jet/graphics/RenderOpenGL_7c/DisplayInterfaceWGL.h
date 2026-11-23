// RenderOpenGL_7c: a minimal WGL context for the local backend (unit 2).
// The retail class also carries pbuffer/shared-context paths; those are added when
// the display port needs them.
#pragma once

#include "DisplayInterface.h"

#include <jet/Window.h>

namespace Jet {

class DisplayInterfaceWGL : public DisplayInterfaceOpenGL {
public:
    DisplayInterfaceWGL();
    ~DisplayInterfaceWGL() override;

    bool __cdecl Create(void* window, void* options) override;
    void __cdecl Destroy();

    void* __cdecl GetDeviceContext() const;
    void* __cdecl GetRenderContext() const;
    void __cdecl SwapBuffers();

    Window* __cdecl GetWindow() const;
    int __cdecl GetPixelFormat() const;

private:
    Window* window;       // +0x10
    int pixelFormat;      // +0x34
    void* deviceContext;  // HDC
    void* renderContext;  // HGLRC
};
JET_LAYOUT32(sizeof(DisplayInterfaceWGL) == 0x24);

}  // namespace Jet
