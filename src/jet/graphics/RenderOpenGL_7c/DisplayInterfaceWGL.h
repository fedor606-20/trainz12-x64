// RenderOpenGL_7c: the WGL context wrapper used by the OpenGL display and its textures.
#pragma once

#include <jet/RenderTypes.h>

#include <windows.h>

namespace Jet {

class DisplayInterfaceWGL {
public:
    DisplayInterfaceWGL();
    ~DisplayInterfaceWGL();

    bool Create(HWND window, int colorBits, int depthBits, int stencilBits, int alphaBits, bool stereo);
    void Destroy();
    void SwapBuffers();
    void SetVsync(bool on);
    void MakeCurrent();

    bool IsExtensionSupported(const char* name) const;
    static bool HasExtension(const char* list, const char* name);

    HWND Window() const { return window; }
    HDC DeviceContext() const { return deviceContext; }
    HGLRC RenderContext() const { return renderContext; }
    int PixelFormat() const { return pixelFormat; }
    const char* Extensions() const;

    // retail layout ("DisplayInterfaceWGL", 0x48 bytes): the public prefix is enough for our own use;
    // the remaining bytes are kept so a display object made by us has the retail footprint.
    void* vtable;  // +0x00
    int referenceCount;  // +0x04
    unsigned char flags;  // +0x08
    unsigned char pad[3];
    void* owner;          // +0x0c
    HWND window;           // +0x10
    void* os;              // +0x14
    void* library;         // +0x18
    HDC deviceContext;     // +0x1c
    HGLRC renderContext;   // +0x20
    HGLRC sharedContext;   // +0x24
    void* savedDc;         // +0x28
    int savedPixelFormat;  // +0x2c
    void* pbuffer;         // +0x30
    int pixelFormat;       // +0x34
    unsigned char stateFlags;  // +0x38
    unsigned char pad2[3];
    int samples;           // +0x3c
    int width;             // +0x40
    int height;            // +0x44
};
JET_LAYOUT32(sizeof(DisplayInterfaceWGL) == 0x48);

}  // namespace Jet
