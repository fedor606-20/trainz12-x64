"""Classify each dynamically looked-up name: exported by our DLL? alias or local?"""
import re
import glob
import os
import collections

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'src', 'jet', 'graphics', 'Render3DCore_7c')
DEF = os.path.join(ROOT, 'build32', 'Render3DCore_7c.def')

LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')
CALL = re.compile(r'OrigProc<[^>]*>\(|SolidsDetail::OrigProc\(|OriginalProc<[^>]*>\(|LocalProc<[^>]*>\(')


def mangled_from(text, start):
    i = text.find('"', start)
    if i < 0:
        return None
    out = []
    while True:
        m = LIT.match(text, i)
        if not m:
            break
        out.append(m.group(1))
        k = m.end()
        while k < len(text) and text[k] in ' \t\r\n':
            k += 1
        if k < len(text) and text[k] == '"':
            i = k
        else:
            break
    return ''.join(out)


def load_def():
    exports = {}
    for line in open(DEF, encoding='utf-8', errors='replace'):
        line = line.strip()
        if not line or line.startswith(('LIBRARY', 'EXPORTS', ';')):
            continue
        parts = line.split()
        name = parts[0]
        alias = None
        if '=' in name:
            name, alias = name.split('=', 1)
        exports[name] = alias
    return exports


def main():
    exports = load_def()
    found = collections.defaultdict(set)
    for path in sorted(glob.glob(os.path.join(SRC, '*.cpp'))):
        with open(path, encoding='utf-8', errors='replace') as fh:
            text = fh.read()
        for m in CALL.finditer(text):
            name = mangled_from(text, m.end())
            if name and '?' in name:
                found[name].add(os.path.basename(path))
    for name in sorted(found):
        if name in exports:
            alias = exports[name]
            print('SELF  %-40s %s' % ('ALIAS->' + alias if alias else 'local', name))
        else:
            print('MISS  %-40s %s   [%s]' % ('', name, ','.join(sorted(found[name]))))


if __name__ == '__main__':
    main()
