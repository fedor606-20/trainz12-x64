// RenderOpenGL_7c: WGL context creation.
#include "DisplayInterfaceWGL.h"

#include "OpenGLExtensions.h"

#include <cstring>

namespace Jet {

DisplayInterfaceWGL::DisplayInterfaceWGL()
    : vtable(nullptr), referenceCount(1), flags(0), owner(nullptr), window(nullptr), os(nullptr), library(nullptr),
      deviceContext(nullptr), renderContext(nullptr), sharedContext(nullptr), savedDc(nullptr), savedPixelFormat(0),
      pbuffer(nullptr), pixelFormat(0), stateFlags(0), samples(0), width(0), height(0)
{
    memset(pad, 0, sizeof pad);
    memset(pad2, 0, sizeof pad2);
}

DisplayInterfaceWGL::~DisplayInterfaceWGL() { Destroy(); }

bool DisplayInterfaceWGL::Create(HWND window_, int colorBits, int depthBits, int stencilBits, int alphaBits,
                                 bool stereo)
{
    Destroy();
    window = window_;
    if (!window) return false;
    deviceContext = GetDC(window);
    if (!deviceContext) return false;

    PIXELFORMATDESCRIPTOR want{};
    want.nSize = sizeof want;
    want.nVersion = 1;
    want.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    if (stereo) want.dwFlags |= PFD_STEREO;
    want.iPixelType = PFD_TYPE_RGBA;
    want.cColorBits = static_cast<BYTE>(colorBits);
    want.cDepthBits = static_cast<BYTE>(depthBits);
    want.cStencilBits = static_cast<BYTE>(stencilBits);
    want.cAlphaBits = static_cast<BYTE>(alphaBits);
    want.iLayerType = PFD_MAIN_PLANE;

    pixelFormat = ChoosePixelFormat(deviceContext, &want);
    if (!pixelFormat || !SetPixelFormat(deviceContext, pixelFormat, &want)) {
        Destroy();
        return false;
    }
    renderContext = wglCreateContext(deviceContext);
    if (!renderContext || !wglMakeCurrent(deviceContext, renderContext)) {
        Destroy();
        return false;
    }
    GL::LoadExtensions();
    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    width = vp[2];
    height = vp[3];
    return true;
}

void DisplayInterfaceWGL::Destroy()
{
    if (renderContext) {
        if (wglGetCurrentContext() == renderContext) wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(renderContext);
        renderContext = nullptr;
    }
    sharedContext = nullptr;
    if (deviceContext && window) ReleaseDC(window, deviceContext);
    deviceContext = nullptr;
    window = nullptr;
    pixelFormat = 0;
    pbuffer = nullptr;
}

void DisplayInterfaceWGL::SwapBuffers()
{
    if (deviceContext) ::SwapBuffers(deviceContext);
}

void DisplayInterfaceWGL::MakeCurrent()
{
    if (deviceContext && renderContext) wglMakeCurrent(deviceContext, renderContext);
}

void DisplayInterfaceWGL::SetVsync(bool on)
{
    if (GL::ext.SwapInterval) GL::ext.SwapInterval(on ? 1 : 0);
}

const char* DisplayInterfaceWGL::Extensions() const
{
    if (GL::ext.GetExtensionsStringARB && deviceContext)
        return GL::ext.GetExtensionsStringARB(deviceContext);
    return GL::ExtensionString();
}

bool DisplayInterfaceWGL::HasExtension(const char* list, const char* name) { return GL::HasExtension(list, name); }

bool DisplayInterfaceWGL::IsExtensionSupported(const char* name) const
{
    return HasExtension(Extensions(), name) || GL::IsSupported(name);
}

}  // namespace Jet
