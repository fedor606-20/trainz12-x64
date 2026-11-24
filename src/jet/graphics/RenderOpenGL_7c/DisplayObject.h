// RenderOpenGL_7c: the local display object, its 161-slot vtable and the display plugin factories.
#pragma once

#include <windows.h>

namespace Jet {

// Creates an OpenGL display. `xterm` selects the windowed XTerm variant (0xfac) over the base one (0xfa8).
void* __cdecl CreateDisplayObject(bool xterm);
void __cdecl DestroyDisplayObject(void* display);

}  // namespace Jet
