"""Pass D: replace the remaining orig-module lookups whose targets already have local bodies."""
import io
import os

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                    'src', 'jet', 'graphics', 'Render3DCore_7c')


def patch(name, old, new):
    path = os.path.join(ROOT, name)
    text = io.open(path, encoding='utf-8', newline='').read()
    crlf = '\r\n' in text
    if crlf:
        old = old.replace('\n', '\r\n')
        new = new.replace('\n', '\r\n')
    if old not in text:
        print('SKIP %s' % name)
        return False
    io.open(path, 'w', encoding='utf-8', newline='').write(text.replace(old, new, 1))
    print('patched %s' % name)
    return True


patch('ParticleBoundingVolume.cpp', '''// U1c's OrientedBox::Transform uses a slightly different operand order for NaN payloads; the original export is the
// reference here (and the same call the U9a Particle code makes).
void TransformOrig(OrientedBox& box, const Matrix3& m, const Vector3& t)
{
    using Fn = void(__cdecl*)(OrientedBox*, const Matrix3*, const Vector3*);
    static Fn fn = OrigProc<Fn>("?Transform@OrientedBox@Jet@@QAAXABVMatrix3@2@ABVVector3@2@@Z");
    if (fn)
        fn(&box, &m, &t);
    else
        box.Transform(m, t);
}

void InitOrig(OrientedBox& box, int count, VertexItemPtr<const Vector3, VIPConstHelper> items)
{
    using Fn = void(__cdecl*)(OrientedBox*, int, VertexItemPtr<const Vector3, VIPConstHelper>);
    static Fn fn = OrigProc<Fn>(
        "?Init@OrientedBox@Jet@@IAAXHV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@Z");
    if (fn)
        fn(&box, count, items);
}

void MaximizeOrig(OrientedBox& box, const OrientedBox& other)
{
    using Fn = void(__cdecl*)(OrientedBox*, const OrientedBox*);
    static Fn fn = OrigProc<Fn>("?Maximize@OrientedBox@Jet@@QAAXABV12@@Z");
    if (fn)
        fn(&box, &other);
    else
        box.Maximize(other);
}''', '''// U1c owns all three bodies and they are local, so call them directly.
void TransformOrig(OrientedBox& box, const Matrix3& m, const Vector3& t)
{
    box.Transform(m, t);
}

void InitOrig(OrientedBox& box, int count, VertexItemPtr<const Vector3, VIPConstHelper> items)
{
    box.Init(count, items);
}

void MaximizeOrig(OrientedBox& box, const OrientedBox& other)
{
    box.Maximize(other);
}''')

patch('ParticleOrientedBoundingBox.cpp', '''void MaximizeOrig(OrientedBox& box, const OrientedBox& other)
{
    using Fn = void(__cdecl*)(OrientedBox*, const OrientedBox*);
    static Fn fn = OrigProc<Fn>("?Maximize@OrientedBox@Jet@@QAAXABV12@@Z");
    if (fn)
        fn(&box, &other);
    else
        box.Maximize(other);
}''', '''void MaximizeOrig(OrientedBox& box, const OrientedBox& other)
{
    box.Maximize(other);
}''')

patch('ParticleOrientedBoundingBox.cpp', '''void BuildAlignedBox(AxisAlignedBox& out, const OrientedBox& box)
{
    using Fn = void(__thiscall*)(AxisAlignedBox*, const OrientedBox*);
    static Fn fn = OrigProc<Fn>("??0AxisAlignedBox@Jet@@QAE@ABVOrientedBox@1@@Z");
    if (fn)
        fn(&out, &box);
    else
        out = AxisAlignedBox(box);
}''', '''void BuildAlignedBox(AxisAlignedBox& out, const OrientedBox& box)
{
    out = AxisAlignedBox(box);
}''')

patch('ParticleAxisAlignedBoundingBox.cpp', '''// U1c's AxisAlignedBox(OrientedBox) constructor has a different NaN path from the original; use the original export.
void BuildAlignedBox(AxisAlignedBox& out, const OrientedBox& box)
{
    using Fn = void(__thiscall*)(AxisAlignedBox*, const OrientedBox*);
    static Fn fn = OrigProc<Fn>("??0AxisAlignedBox@Jet@@QAE@ABVOrientedBox@1@@Z");
    if (fn)
        fn(&out, &box);
    else
        out = AxisAlignedBox(box);
}''', '''// U1c's AxisAlignedBox(OrientedBox) constructor is local.
void BuildAlignedBox(AxisAlignedBox& out, const OrientedBox& box)
{
    out = AxisAlignedBox(box);
}''')
