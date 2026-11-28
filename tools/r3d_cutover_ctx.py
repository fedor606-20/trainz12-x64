"""Print context around every OrigProc/OrigRva/OrigModule-use call site."""
import glob
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(os.path.dirname(HERE), 'src', 'jet', 'graphics', 'Render3DCore_7c')

PAT = re.compile(r'OrigProc\s*(?:<.*?>)?\s*\(|OrigRva\s*<')
SELF = re.compile(r'^(?:[A-Za-z_][\w:<>~\*&\s,]*\s+)?[A-Za-z_]\w*(?:::[A-Za-z_~]\w*)?\s*\(')

def main():
    for path in sorted(glob.glob(os.path.join(ROOT, '*.cpp'))):
        lines = open(path, encoding='utf-8', errors='replace').read().splitlines()
        hits = [i for i, l in enumerate(lines) if PAT.search(l) and 'OrigModule' not in l]
        if not hits:
            continue
        print('\n######## %s' % os.path.basename(path))
        last = -99
        for i in hits:
            if i - last < 8:
                print('  ...')
                last = i
                continue
            lo, hi = max(0, i - 7), min(len(lines), i + 3)
            # nearest preceding function-looking line
            fn = ''
            for j in range(i, max(0, i - 60), -1):
                if SELF.match(lines[j]) and not lines[j].rstrip().endswith(';'):
                    fn = lines[j].strip()
                    break
            print('-- line %d   in: %s' % (i + 1, fn))
            for j in range(lo, hi):
                print('   %5d %s' % (j + 1, lines[j]))
            last = i

if __name__ == '__main__':
    main()
