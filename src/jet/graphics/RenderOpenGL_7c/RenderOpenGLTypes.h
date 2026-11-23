// RenderOpenGL_7c: display/context descriptors (unit 1 of the port).
#pragma once

#include <jet/RenderTypes.h>

namespace Jet {

class DisplayInterfaceOpenGL;

// ContextDescriptorOpenGL: the platform tag of a context (retail: 4 bytes, WGL = 2).
class ContextDescriptorOpenGL {
public:
    enum PlatformType { PLATFORM_WGL = 2 };

    ContextDescriptorOpenGL& operator=(const ContextDescriptorOpenGL& o);
    PlatformType __cdecl GetPlatform() const;

protected:
    explicit ContextDescriptorOpenGL(PlatformType platform);

private:
    PlatformType platform;  // +0x00
};
JET_LAYOUT32(sizeof(ContextDescriptorOpenGL) == 4);

// ContextDescriptorOpenGLWGL: the WGL device/render context pair (retail: 0x0c bytes).
class ContextDescriptorOpenGLWGL : public ContextDescriptorOpenGL {
public:
    ContextDescriptorOpenGLWGL();
    ContextDescriptorOpenGLWGL& operator=(const ContextDescriptorOpenGLWGL& o);

    void* __cdecl GetDeviceContext() const;
    void* __cdecl GetRenderContext() const;
    void __cdecl SetContext(void* deviceContext, void* renderContext);

private:
    void* deviceContext;  // +0x04
    void* renderContext;  // +0x08
};
JET_LAYOUT32(sizeof(ContextDescriptorOpenGLWGL) == 0x0c);

// DisplayDescriptorOpenGL: DisplayDescriptor plus the context the interface reports (retail: 0x20).
class DisplayDescriptorOpenGL : public DisplayDescriptor {
public:
    DisplayDescriptorOpenGL();
    DisplayDescriptorOpenGL(const DisplayDescriptorOpenGL& o);
    ~DisplayDescriptorOpenGL() override;
    DisplayDescriptorOpenGL& operator=(const DisplayDescriptorOpenGL& o);

    const ContextDescriptorOpenGL& __cdecl GetContext() const;
    ContextDescriptorOpenGL::PlatformType __cdecl GetPlatform() const;
    void __cdecl Initialize(DisplayInterfaceOpenGL* iface);

private:
    ContextDescriptorOpenGL* context;  // +0x1c
};
JET_LAYOUT32(sizeof(DisplayDescriptorOpenGL) == 0x20);

}  // namespace Jet
