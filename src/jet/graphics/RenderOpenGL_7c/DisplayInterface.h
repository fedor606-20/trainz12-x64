// RenderOpenGL_7c: the GL interface base (unit 2). The retail class is internal; this
// declaration follows its vtable order so the WGL backend and the display can share it.
#pragma once

#include <jet/KernelStandard.h>

namespace Jet {

class DisplayInterfaceOpenGL : public ReferenceCount {
public:
    DisplayInterfaceOpenGL();
    ~DisplayInterfaceOpenGL() override;

    // slot 1: backend context creation (WGL overrides it)
    virtual bool __cdecl Create(void* window, void* options);
    virtual void __cdecl Slot2();  // retail stub
    virtual void __cdecl Slot3();  // retail stub

    // slot 4: a space-delimited GL extension list contains `name`
    static bool __cdecl HasExtension(const char* list, const char* name);
    // slot 5: the current GL context advertises `name`
    virtual bool __cdecl IsExtensionSupported(const char* name);

protected:
    void* field04;              // +0x04
    void* field08;              // +0x08
    mutable const char* extensions;  // +0x0c, cached GL_EXTENSIONS
};
JET_LAYOUT32(sizeof(DisplayInterfaceOpenGL) == 0x14);

}  // namespace Jet
