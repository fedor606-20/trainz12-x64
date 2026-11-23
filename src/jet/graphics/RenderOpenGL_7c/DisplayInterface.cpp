// RenderOpenGL_7c: the GL interface base (unit 2).
#include "DisplayInterface.h"

#include <windows.h>

#include <cstring>

namespace Jet {

namespace {
using GetStringFn = const char*(__stdcall*)(unsigned int);

GetStringFn GetString()
{
    static const auto fn =
        reinterpret_cast<GetStringFn>(GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetString"));
    return fn;
}
}

DisplayInterfaceOpenGL::DisplayInterfaceOpenGL()
    : field04(nullptr), field08(nullptr), extensions(nullptr)
{
}

DisplayInterfaceOpenGL::~DisplayInterfaceOpenGL() = default;

bool __cdecl DisplayInterfaceOpenGL::Create(void*, void*) { return false; }
void __cdecl DisplayInterfaceOpenGL::Slot2() {}
void __cdecl DisplayInterfaceOpenGL::Slot3() {}

bool __cdecl DisplayInterfaceOpenGL::HasExtension(const char* list, const char* name)
{
    if (!list || !name || !*name) return false;
    const std::size_t length = std::strlen(name);
    const char* at = list;
    while ((at = std::strstr(at, name)) != nullptr) {
        if ((at == list || at[-1] == ' ') && (at[length] == ' ' || at[length] == '\0')) return true;
        at += length;
    }
    return false;
}

bool __cdecl DisplayInterfaceOpenGL::IsExtensionSupported(const char* name)
{
    const GetStringFn getString = GetString();
    if (!getString) return false;
    if (!extensions) extensions = getString(0x1f03);  // GL_EXTENSIONS
    return extensions ? HasExtension(extensions, name) : false;
}

}  // namespace Jet
