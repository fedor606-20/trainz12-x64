"""Inventory the dynamic Render3DCore_7c_orig.dll references in Render3DCore_7c."""
import re
import sys
import glob
import os

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                    'src', 'jet', 'graphics', 'Render3DCore_7c')

PAT = re.compile(r'Render3DCore_7c_orig\.dll|OrigRva|OrigProc|OrigModule|SelfData|OrigBase')

def main():
    total = 0
    for path in sorted(glob.glob(os.path.join(ROOT, '*.cpp')) + glob.glob(os.path.join(ROOT, '*.h'))):
        with open(path, encoding='utf-8', errors='replace') as fh:
            lines = fh.read().splitlines()
        hits = [i for i, l in enumerate(lines) if PAT.search(l)]
        if not hits:
            continue
        total += len(hits)
        print('===== %s  (%d lines)' % (os.path.basename(path), len(hits)))
        for i in hits:
            print('%5d: %s' % (i + 1, lines[i].rstrip()))
    print('TOTAL LINES: %d' % total)

if __name__ == '__main__':
    main()
