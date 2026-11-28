"""List every mangled name looked up dynamically from Render3DCore_7c_orig.dll."""
import re
import glob
import os
import collections

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                    'src', 'jet', 'graphics', 'Render3DCore_7c')

# Join string literals: "a" "b"
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')
CALL = re.compile(r'OrigProc<[^>]*>\(|SolidsDetail::OrigProc\(|OrigProc\(')

def mangled_from(text, start):
    """Collect concatenated string literals starting at/after `start`."""
    i = text.find('"', start)
    if i < 0:
        return None
    out = []
    while True:
        m = LIT.match(text, i)
        if not m:
            break
        out.append(m.group(1))
        j = m.end()
        # skip whitespace/newlines between adjacent literals
        k = j
        while k < len(text) and text[k] in ' \t\r\n':
            k += 1
        if k < len(text) and text[k] == '"':
            i = k
        else:
            break
    return ''.join(out)

def main():
    found = collections.defaultdict(list)
    for path in sorted(glob.glob(os.path.join(ROOT, '*.cpp'))):
        with open(path, encoding='utf-8', errors='replace') as fh:
            text = fh.read()
        for m in CALL.finditer(text):
            name = mangled_from(text, m.end())
            if name and '?' in name:
                found[name].append(os.path.basename(path))
    for name in sorted(found):
        print('%-6d %s   %s' % (len(found[name]), name, ','.join(sorted(set(found[name])))))
    print('TOTAL NAMES: %d' % len(found))

if __name__ == '__main__':
    main()
