"""Replace the orig-module lookups of functions that already have local bodies with direct calls."""
import io
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                    'src', 'jet', 'graphics', 'Render3DCore_7c')

OLD_CAMERA = '''using BvProcRecalc = void(__cdecl*)(void*);
using BvProcChanged = void(__cdecl*)(void*, const Vector3&, const Vector3&);
using BvProcQuatChanged = void(__cdecl*)(void*, const Quaternion&, const Quaternion&);
using BvProcBoth = void(__cdecl*)(void*, const Vector3&, const Quaternion&, const Vector3&, const Quaternion&);

void BvRecalculateGlobalOrientation(void* bv)
{
    using Fn = void(__cdecl*)(void*);
    static Fn fn = OrigProc<Fn>("?RecalculateGlobalOrientation@ParticleBoundingVolume@Jet@@IBAXXZ");
    if (fn) fn(bv);
}

void BvRecalculateGlobalPosition(void* bv)
{
    using Fn = void(__cdecl*)(void*);
    static Fn fn = OrigProc<Fn>("?RecalculateGlobalPosition@ParticleBoundingVolume@Jet@@IBAXXZ");
    if (fn) fn(bv);
}

void BvSetPositionChanged(void* bv, const Vector3& next, const Vector3& previous)
{
    static BvProcChanged fn = OrigProc<BvProcChanged>("?SetPositionChanged@ParticleBoundingVolume@Jet@@QAAXABVVector3@2@0@Z");
    if (fn) fn(bv, next, previous);
}

void BvSetOrientationChanged(void* bv, const Quaternion& next, const Quaternion& previous)
{
    static BvProcQuatChanged fn =
        OrigProc<BvProcQuatChanged>("?SetOrientationChanged@ParticleBoundingVolume@Jet@@QAAXABVQuaternion@2@0@Z");
    if (fn) fn(bv, next, previous);
}

void BvSetBothChanged(void* bv, const Vector3& nextPos, const Quaternion& nextRot, const Vector3& prevPos,
                      const Quaternion& prevRot)
{
    static BvProcBoth fn =
        OrigProc<BvProcBoth>("?SetPositionOrientationChanged@ParticleBoundingVolume@Jet@@QAAXABVVector3@2@0ABVQuaternion@2@1@Z");
    if (fn) fn(bv, nextPos, nextRot, prevPos, prevRot);
}
'''

NEW_CAMERA = '''// The bounding volume is U3's class and every notification below is a local body, so call it directly.
void BvRecalculateGlobalOrientation(void* bv)
{
    static_cast<ParticleBoundingVolume*>(bv)->RecalculateGlobalOrientation();
}

void BvRecalculateGlobalPosition(void* bv)
{
    static_cast<ParticleBoundingVolume*>(bv)->RecalculateGlobalPosition();
}

void BvSetPositionChanged(void* bv, const Vector3& next, const Vector3& previous)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetPositionChanged(next, previous);
}

void BvSetOrientationChanged(void* bv, const Quaternion& next, const Quaternion& previous)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetOrientationChanged(next, previous);
}

void BvSetBothChanged(void* bv, const Vector3& nextPos, const Quaternion& nextRot, const Vector3& prevPos,
                      const Quaternion& prevRot)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetPositionOrientationChanged(nextPos, nextRot, prevPos, prevRot);
}
'''


def replace_block(path, old, new):
    text = io.open(path, encoding='utf-8', newline='').read()
    crlf = '\r\n' in text
    if crlf:
        old = old.replace('\n', '\r\n')
        new = new.replace('\n', '\r\n')
    if old not in text:
        print('SKIP %s' % os.path.basename(path))
        return False
    io.open(path, 'w', encoding='utf-8', newline='').write(text.replace(old, new, 1))
    print('patched %s' % os.path.basename(path))
    return True


def add_include(path, header):
    text = io.open(path, encoding='utf-8', newline='').read()
    line = '#include <%s>' % header
    if line in text:
        return
    eol = '\r\n' if '\r\n' in text else '\n'
    anchor = '#include <windows.h>'
    text = text.replace(anchor, line + eol + eol + anchor, 1)
    io.open(path, 'w', encoding='utf-8', newline='').write(text)
    print('  + %s in %s' % (header, os.path.basename(path)))


cam = os.path.join(ROOT, 'Camera.cpp')
if replace_block(cam, OLD_CAMERA, NEW_CAMERA):
    add_include(cam, 'jet/Render3DCoreBounds.h')
