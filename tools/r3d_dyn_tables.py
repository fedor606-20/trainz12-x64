"""List the vtable/vbtable/UId names MaterialLifetime/MaterialPlugins pass to LocalData."""
import re
import os

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                    'src', 'jet', 'graphics', 'Render3DCore_7c')
DEF = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'build32',
                   'Render3DCore_7c.def')

LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')
CALL = re.compile(r'(?:Finish|Begin|MaterialUid|Vtable)\(')


def load_def():
    exports = {}
    for line in open(DEF, encoding='utf-8', errors='replace'):
        line = line.strip()
        if not line or line.startswith(('LIBRARY', 'EXPORTS', ';')):
            continue
        parts = line.split()
        name = parts[0]
        if '=' in name:
            name = name.split('=', 1)[0]
        exports[name] = True
    return exports


def main():
    exports = load_def()
    missing = set()
    seen = set()
    for fn in ('MaterialLifetime.cpp', 'MaterialPlugins.cpp', 'U5aRem.cpp', 'MultiPhaseState.cpp',
               'MultiPassState.cpp', 'MultiPassPlugins.cpp', 'U5bLifetime.cpp', 'U5bLastPlugins.cpp',
               'U5bFinalState.cpp', 'U5bFinalChunks.cpp', 'U5aFinal.cpp'):
        path = os.path.join(ROOT, fn)
        if not os.path.exists(path):
            continue
        text = open(path, encoding='utf-8', errors='replace').read()
        for m in CALL.finditer(text):
            i = text.find('"', m.end())
            if i < 0:
                continue
            out = []
            while True:
                lm = LIT.match(text, i)
                if not lm:
                    break
                out.append(lm.group(1))
                k = lm.end()
                while k < len(text) and text[k] in ' \t\r\n':
                    k += 1
                if k < len(text) and text[k] == '"':
                    i = k
                else:
                    break
            name = ''.join(out)
            if '?' not in name:
                continue
            if name in seen:
                continue
            seen.add(name)
            if name not in exports:
                print('MISSING  %s   (%s)' % (name, fn))
                missing.add(name)
    print('checked %d names, missing %d' % (len(seen), len(missing)))


if __name__ == '__main__':
    main()
