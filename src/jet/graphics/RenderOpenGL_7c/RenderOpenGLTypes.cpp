// RenderOpenGL_7c: display/context descriptors (unit 1 of the port).
#include "RenderOpenGLTypes.h"

#include <jet/VCall.h>

#include <windows.h>

#include <cstdlib>

namespace Jet {

// ---- ContextDescriptorOpenGL

ContextDescriptorOpenGL::ContextDescriptorOpenGL(PlatformType value) : platform(value) {}

ContextDescriptorOpenGL& ContextDescriptorOpenGL::operator=(const ContextDescriptorOpenGL& o)
{
    platform = o.platform;
    return *this;
}

ContextDescriptorOpenGL::PlatformType __cdecl ContextDescriptorOpenGL::GetPlatform() const { return platform; }

// ---- ContextDescriptorOpenGLWGL

ContextDescriptorOpenGLWGL::ContextDescriptorOpenGLWGL()
    : ContextDescriptorOpenGL(PLATFORM_WGL), deviceContext(nullptr), renderContext(nullptr)
{
}

ContextDescriptorOpenGLWGL& ContextDescriptorOpenGLWGL::operator=(const ContextDescriptorOpenGLWGL& o)
{
    ContextDescriptorOpenGL::operator=(o);
    deviceContext = o.deviceContext;
    renderContext = o.renderContext;
    return *this;
}

void* __cdecl ContextDescriptorOpenGLWGL::GetDeviceContext() const { return deviceContext; }
void* __cdecl ContextDescriptorOpenGLWGL::GetRenderContext() const { return renderContext; }

void __cdecl ContextDescriptorOpenGLWGL::SetContext(void* device, void* render)
{
    deviceContext = device;
    renderContext = render;
}

// ---- DisplayDescriptorOpenGL

DisplayDescriptorOpenGL::DisplayDescriptorOpenGL()
    : DisplayDescriptor(static_cast<APIType>(2))
{
    // The retail constructor leaves the context pointer alone; Initialize fills it.
}

DisplayDescriptorOpenGL::DisplayDescriptorOpenGL(const DisplayDescriptorOpenGL& o)
    : DisplayDescriptor(o), context(o.context)
{
}

DisplayDescriptorOpenGL::~DisplayDescriptorOpenGL() = default;

DisplayDescriptorOpenGL& DisplayDescriptorOpenGL::operator=(const DisplayDescriptorOpenGL& o)
{
    DisplayDescriptor::operator=(o);
    context = o.context;
    return *this;
}

const ContextDescriptorOpenGL& __cdecl DisplayDescriptorOpenGL::GetContext() const { return *context; }

ContextDescriptorOpenGL::PlatformType __cdecl DisplayDescriptorOpenGL::GetPlatform() const
{
    return context->GetPlatform();
}

void __cdecl DisplayDescriptorOpenGL::Initialize(DisplayInterfaceOpenGL* iface)
{
    using GetString = const char*(__stdcall*)(unsigned int);
    static const auto getString =
        reinterpret_cast<GetString>(GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetString"));
    const char* version = getString ? getString(0x1f02) : nullptr;
    const char* vendor = getString ? getString(0x1f00) : nullptr;
    const char* renderer = getString ? getString(0x1f01) : nullptr;
    const PString api("OpenGL");
    const PString apiVersion(version ? version : "");
    const PString cardVendor(vendor ? vendor : "");
    const PString card(renderer ? renderer : "");
    SetData(api, apiVersion, cardVendor, card, static_cast<float>(std::atof(version ? version : "")));
    context = VCall<ContextDescriptorOpenGL*>(iface, 0x58);
}

}  // namespace Jet
