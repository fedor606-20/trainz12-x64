"""Show the def entry (with DATA flag / alias) for every vtable/UId name the material ctors ask for."""
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
        raw = parts[0]
        rest = ' '.join(parts[1:])
        name = raw.split('=', 1)[0]
        exports[name] = (raw, rest)
    return exports


def main():
    exports = load_def()
    seen = []
    for fn in ('MaterialLifetime.cpp', 'MaterialPlugins.cpp'):
        path = os.path.join(ROOT, fn)
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
            if '?' in name and name not in seen:
                seen.append(name)
    for name in seen:
        raw, rest = exports.get(name, ('<not exported>', ''))
        alias = 'ALIAS' if '=' in raw else 'DATA' if 'DATA' in rest else 'CODE'
        print('%-5s %s' % (alias, name))


if __name__ == '__main__':
    main()
