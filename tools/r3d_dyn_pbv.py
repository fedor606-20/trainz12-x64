"""Remove the now-unused OrigModule/OrigProc pair from ParticleBoundingView.cpp."""
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                    'src', 'jet', 'graphics', 'Render3DCore_7c')

PAT = re.compile(
    rb'HMODULE OrigModule\(\)\r?\n'
    rb'\{\r?\n'
    rb'    static HMODULE module = \[\] \{\r?\n'
    rb'        HMODULE m = GetModuleHandleA\("Render3DCore_7c_orig\.dll"\);\r?\n'
    rb'        return m \? m : LoadLibraryA\("Render3DCore_7c_orig\.dll"\);\r?\n'
    rb'    \}\(\);\r?\n'
    rb'    return module;\r?\n'
    rb'\}\r?\n'
    rb'\r?\n'
    rb'template <class F>\r?\n'
    rb'F OrigProc\(const char\* name\)\r?\n'
    rb'\{\r?\n'
    rb'    HMODULE m = OrigModule\(\);\r?\n'
    rb'    return m \? reinterpret_cast<F>\(GetProcAddress\(m, name\)\) : nullptr;\r?\n'
    rb'\}\r?\n'
    rb'\r?\n')

path = os.path.join(ROOT, 'ParticleBoundingView.cpp')
data = open(path, 'rb').read()
new, count = PAT.subn(b'', data, count=1)
print('removed' if count else 'pattern not found')
if count:
    open(path, 'wb').write(new)
