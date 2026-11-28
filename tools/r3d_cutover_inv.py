"""For every OrigProc("<mangled msvc name>") call site, report whether the
module already defines that symbol locally (found in implemented.d lists) or
only forwards it.

Also lists the raw OrigRva call sites.
"""
import glob
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(os.path.dirname(HERE), 'src', 'jet', 'graphics', 'Render3DCore_7c')

# OrigProc<Fn>("name") or OrigProc("name"); the name is the first quoted string
# after the call's opening parenthesis.
PAT_PROC = re.compile(r'OrigProc\s*(?:<.*?>)?\s*\(\s*"((?:[^"\\]|\\.)*)"', re.S)
PAT_RVA = re.compile(r'OrigRva<([^>]*)>\(\s*(0x[0-9a-fA-F]+)')

def load_implemented():
    names = set()
    for path in glob.glob(os.path.join(ROOT, 'implemented.d', '*.txt')) + [
            os.path.join(ROOT, 'implemented.txt')]:
        if not os.path.exists(path):
            continue
        for line in open(path, encoding='utf-8', errors='replace'):
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            names.add(line.split('\t')[0].split(' ')[0])
    return names

def main():
    impl = load_implemented()
    print('implemented names: %d' % len(impl))
    for path in sorted(glob.glob(os.path.join(ROOT, '*.cpp'))):
        text = open(path, encoding='utf-8', errors='replace').read()
        procs = list(dict.fromkeys(PAT_PROC.findall(text)))
        rvas = list(dict.fromkeys(PAT_RVA.findall(text)))
        if not procs and not rvas:
            continue
        print('== %s' % os.path.basename(path))
        for p in procs:
            state = 'LOCAL' if p in impl else 'FORWARD'
            print('  %-7s %s' % (state, p))
        for typ, addr in rvas:
            print('  RVA     %s  %s' % (addr, typ.strip()))

if __name__ == '__main__':
    main()
