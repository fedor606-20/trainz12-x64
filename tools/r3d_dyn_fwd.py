"""For every dynamically looked-up export, locate its local definition and say whether it is a forwarder."""
import os
import re
import glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src', 'jet', 'graphics', 'Render3DCore_7c')

NAMES = [l.strip() for l in open(os.path.join(ROOT, 'tools', 'r3d_dyn_names.txt'), encoding='utf-8')
         if l.strip() and not l.startswith('TOTAL')]


def method_of(mangled):
    # ?Method@Class@Jet@@...
    m = re.match(r'\?+([^@]+)@([^@]+)@Jet@@', mangled)
    if not m:
        return None, None
    method, cls = m.group(1), m.group(2)
    if method.startswith('$') or method in ('0', '1', '4', '8', '_7', '_8', '_B', '_F'):
        return None, None
    return method, cls


def main():
    files = {}
    for path in glob.glob(os.path.join(SRC, '*.cpp')):
        files[os.path.basename(path)] = open(path, encoding='utf-8', errors='replace').read()
    for name in NAMES:
        method, cls = method_of(name)
        if not method:
            print('SPECIAL %s' % name)
            continue
        pat = re.compile(r'\b%s::%s\s*\(' % (re.escape(cls), re.escape(method)))
        hits = []
        for fn, text in files.items():
            for m in pat.finditer(text):
                line_no = text.count('\n', 0, m.start()) + 1
                tail = text[m.end():m.end() + 700]
                fwd = 'OrigProc' in tail.split('{', 1)[-1][:500] if '{' in tail else False
                hits.append((fn, line_no, fwd))
        if not hits:
            print('NODEF   %-45s %s' % (cls + '::' + method, name))
        elif all(h[2] for h in hits):
            print('FWD     %-45s %s' % (cls + '::' + method, hits[0][0]))
        elif any(not h[2] for h in hits):
            real = [h for h in hits if not h[2]]
            print('LOCAL   %-45s %s:%d' % (cls + '::' + method, real[0][0], real[0][1]))


if __name__ == '__main__':
    main()
