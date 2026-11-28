"""Mutation self-check for Render3DCore_7c.

Each catalog entry performs one small, behaviour-changing edit in the local
module, rebuilds it, runs the listed differential tests against the original
module and expects them to fail.  A mutation that survives (tests stay green)
is a coverage hole in the tests, not a success.

The source file and the built DLL are restored in a finally block; the tool
exits nonzero if any mutation survived.

usage:
    python tools/r3d_mutate.py                     # run the whole catalog
    python tools/r3d_mutate.py --only camera-order
    python tools/r3d_mutate.py --list
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
MODULE_DIR = os.path.join(ROOT, 'src', 'jet', 'graphics', 'Render3DCore_7c')
BUILD_DIR = os.path.join(ROOT, 'build32')
DLL = os.path.join(BUILD_DIR, 'Release', 'Render3DCore_7c.dll')
FIXTURE = os.path.join(ROOT, 'testbin-r3d')

# file, old text, new text, label, tests that must fail
CATALOG = [
    (
        'Camera.cpp',
        'BvSetPositionChanged(BvRef(), PosRef(), nextPosition);',
        'BvSetPositionChanged(BvRef(), nextPosition, PosRef());',
        'camera-order',
        ['diff_r3d_u8'],
    ),
    (
        'Particle.cpp',
        'BvSetPositionChanged(boundingVolume, position, value);',
        'BvSetPositionChanged(boundingVolume, value, position);',
        'particle-position-order',
        ['diff_r3d_u9a'],
    ),
    (
        'Cell.cpp',
        '*reinterpret_cast<unsigned int*>(n + 0x28) = 3;',
        '*reinterpret_cast<unsigned int*>(n + 0x28) = 5;',
        'cell-node-field',
        ['diff_r3d_u9b', 'diff_r3d_u9b_rem'],
    ),
    (
        'Cell.cpp',
        'std::memcpy(&self->axisAlignedBox.min.x, node + 0x30, 6 * sizeof(float));',
        'std::memcpy(&self->axisAlignedBox.min.x, node + 0x34, 5 * sizeof(float));',
        'cell-extent-copy',
        ['diff_r3d_u9b', 'diff_r3d_u9b_rem'],
    ),
    (
        'Camera.cpp',
        'm.m[13] = AddSS(AddSS(AddSS(MulSS(o[5], nz), MulSS(o[4], ny)), MulSS(o[3], nx)), m.m[13]);',
        'm.m[13] = AddSS(AddSS(AddSS(MulSS(o[6], nz), MulSS(o[4], ny)), MulSS(o[3], nx)), m.m[13]);',
        'cached-view-row',
        ['diff_r3d_u8'],
    ),
]


def run(cmd, cwd, timeout=900):
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout, text=True)
    return p.returncode, p.stdout


def build_module():
    code, out = run(['cmake', '--build', BUILD_DIR, '--config', 'Release', '--target', 'Render3DCore_7c'], ROOT)
    if code != 0:
        tail = '\n'.join(out.strip().splitlines()[-6:])
        raise RuntimeError('build failed:\n' + tail)


def run_test(name):
    exe = os.path.join(FIXTURE, name + '.exe')
    if not os.path.exists(exe):
        return None
    code, out = run([exe, '.'], FIXTURE)
    last = ''
    for line in out.splitlines():
        if 'checks' in line:
            last = line
    return {'code': code, 'last': last}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', action='append', default=[])
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--keep-going', action='store_true')
    args = ap.parse_args()

    if args.list:
        for entry in CATALOG:
            print(entry[3], '->', ', '.join(entry[4]))
        return 0

    selected = [m for m in CATALOG if not args.only or m[3] in args.only]
    if not selected:
        print('no matching catalog entries')
        return 2

    if not os.path.isdir(FIXTURE):
        print('fixture directory missing:', FIXTURE)
        return 2

    dll_backup = os.path.join(os.environ.get('TEMP', '.'), 'r3d_mutate_dll.bak')
    if os.path.exists(DLL):
        shutil.copy2(DLL, dll_backup)
    else:
        dll_backup = None

    survived = []
    results = []
    for path_name, old, new, label, tests in selected:
        path = os.path.join(MODULE_DIR, path_name)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        if old not in text:
            print('SKIP %-22s pattern not found in %s' % (label, path_name))
            survived.append(label)
            continue
        try:
            mutated = text.replace(old, new, 1)
            if mutated == text:
                raise RuntimeError('mutation did not change the file')
            open(path, 'wb').write(mutated.encode('utf-8'))
            print('--- %s: patched %s' % (label, path_name), flush=True)
            build_module()
            shutil.copy2(DLL, os.path.join(FIXTURE, 'Render3DCore_7c.dll'))
            killed = False
            details = []
            for test in tests:
                r = run_test(test)
                if r is None:
                    details.append(test + ': missing')
                    continue
                bad = r['code'] != 0 or '0 mismatches' not in r['last']
                details.append('%s: %s' % (test, 'FAIL' if bad else 'pass'))
                killed = killed or bad
            print('    %s -> %s' % ('KILLED' if killed else 'SURVIVED', '; '.join(details)), flush=True)
            if not killed:
                survived.append(label)
            results.append({'label': label, 'killed': killed, 'detail': details})
        finally:
            open(path, 'wb').write(original)
            if dll_backup:
                shutil.copy2(dll_backup, DLL)

    # leave the build tree with the restored source compiled
    try:
        build_module()
        shutil.copy2(DLL, os.path.join(FIXTURE, 'Render3DCore_7c.dll'))
    except Exception as exc:  # pragma: no cover
        print('restore rebuild failed:', exc)
        return 2

    print('\n%d/%d mutations killed' % (len(results) - len(survived), len(results)))
    if survived:
        print('SURVIVED: ' + ', '.join(survived))
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
