"""Remove the dead OrigProc template (no call sites) from the U10 files (CRLF safe)."""
import io
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                    'src', 'jet', 'graphics', 'Render3DCore_7c')

PAT = re.compile(
    rb'template <class F>\r?\n'
    rb'F OrigProc\(const char\* name\)\r?\n'
    rb'\{\r?\n'
    rb'    static HMODULE module = \[\] \{\r?\n'
    rb'        HMODULE m = GetModuleHandleA\("Render3DCore_7c_orig\.dll"\);\r?\n'
    rb'        return m \? m : LoadLibraryA\("Render3DCore_7c_orig\.dll"\);\r?\n'
    rb'    \}\(\);\r?\n'
    rb'    return module \? reinterpret_cast<F>\(GetProcAddress\(module, name\)\) : nullptr;\r?\n'
    rb'\}\r?\n'
    rb'\r?\n')

for name in ('RenderPolygon.cpp', 'RenderPolyline.cpp', 'RenderShapes.cpp', 'RenderPrimitive.cpp'):
    path = os.path.join(ROOT, name)
    data = open(path, 'rb').read()
    new, count = PAT.subn(b'', data, count=1)
    if count:
        open(path, 'wb').write(new)
        print('cleaned %s' % name)
    else:
        print('SKIP %s' % name)
