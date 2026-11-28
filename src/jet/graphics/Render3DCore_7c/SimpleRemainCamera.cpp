// Render3DCore_7c wave 6: the remaining Camera paths.
//
// 0x10013ee0 GetRay: it first rebuilds cachedOrientation from the orientation quaternion (the same sequence as
// ResetView), then makes a ray whose direction is -forward in orthographic mode and
// sx*right + sy*up + near*(1,1,1) - near*forward in perspective/transition mode; the origin is the camera position
// (offset by sx*right + sy*up in orthographic mode). The direction is normalised with 0x10008b50.
//
// 0x10017320 (Camera const&, Matrix4 const&): copy the Camera-specific fields, transform the position and the six
// frustum planes by the matrix, refresh the cached view transform and premultiply it by the inverse matrix.
// The other three copy/quad constructors (0x10017650, 0x100182a0, 0x10019070) are the portal clipping paths; they
// are declared here and still call the original export until their plane/quad transcription lands.
#include "SimpleRemain.h"

#include <cstring>

namespace Jet {

namespace {

using namespace SolidsDetail;

// 0x10008e10: out = a + b, componentwise, a is ecx, b is edx.
void AddVector3(Vector3& out, const Vector3& a, const Vector3& b)
{
    out.x = AddSS(a.x, b.x);
    out.y = AddSS(a.y, b.y);
    out.z = AddSS(a.z, b.z);
}

// 0x10010930: perspective divide of matrix * (x, y, z, 1). Row-vector layout: the translation is the fourth
// column (m[12..14]), w is m[3]*x + m[7]*y + m[11]*z + m[15].
Vector3 TransformPoint(const Matrix4& m, const Vector3& v)
{
    const float* M = m.m;
    float x = AddSS(MulSS(M[8], v.z), MulSS(M[4], v.y));
    x = AddSS(x, MulSS(M[0], v.x));
    x = AddSS(x, M[12]);
    float y = AddSS(MulSS(M[9], v.z), MulSS(M[1], v.x));
    y = AddSS(y, MulSS(M[5], v.y));
    y = AddSS(y, M[13]);
    float z = AddSS(MulSS(M[10], v.z), MulSS(M[2], v.x));
    z = AddSS(z, MulSS(M[6], v.y));
    z = AddSS(z, M[14]);
    float w = AddSS(MulSS(M[11], v.z), MulSS(M[3], v.x));
    w = AddSS(w, MulSS(M[7], v.y));
    w = AddSS(w, M[15]);
    const float inv = DivSS(1.0f, w);
    Vector3 out;
    out.x = MulSS(x, inv);
    out.y = MulSS(y, inv);
    out.z = MulSS(z, inv);
    return out;
}

// 0x10012bd0: transform a plane by a matrix. The normal is M^T * n; d' is the original's quadratic form
// ((d*nx + m12)*nx + (d*ny + m13)*ny) + (d*nz + m14)*nz (for a rotation this is d + t.n).
void TransformPlane(Plane& out, const Matrix4& m, const Plane& p)
{
    const float* M = m.m;
    const float nx = p.normal.x, ny = p.normal.y, nz = p.normal.z;
    out.normal.x = AddSS(AddSS(MulSS(ny, M[4]), MulSS(nz, M[8])), MulSS(nx, M[0]));
    out.normal.y = AddSS(AddSS(MulSS(nx, M[1]), MulSS(ny, M[5])), MulSS(nz, M[9]));
    out.normal.z = AddSS(AddSS(MulSS(nx, M[2]), MulSS(ny, M[6])), MulSS(nz, M[10]));
    const float sx = MulSS(AddSS(MulSS(p.d, out.normal.x), M[12]), out.normal.x);
    const float sy = MulSS(AddSS(MulSS(p.d, out.normal.y), M[13]), out.normal.y);
    const float sz = MulSS(AddSS(MulSS(p.d, out.normal.z), M[14]), out.normal.z);
    out.d = AddSS(AddSS(sy, sz), sx);
}

// 0x1001ab30: out = a * b, row-major 4x4. The original's term order and left-associative grouping per element
// are kept (they differ from a plain nested loop, which matters in the last bits).
Matrix4 MultiplyMatrix(const Matrix4& a, const Matrix4& b)
{
    const float* A = a.m;
    const float* B = b.m;
    auto term = [&](int k, int col) { return MulSS(A[k], B[(k & 3) * 4 + col]); };
    auto sum4 = [](float a0, float a1, float a2, float a3) { return AddSS(AddSS(AddSS(a0, a1), a2), a3); };
    Matrix4 out;
    out.m[0] = sum4(term(1, 0), term(3, 0), term(2, 0), term(0, 0));
    out.m[1] = sum4(term(1, 1), term(3, 1), term(2, 1), term(0, 1));
    out.m[2] = sum4(term(1, 2), term(3, 2), term(2, 2), term(0, 2));
    out.m[3] = sum4(term(1, 3), term(3, 3), term(2, 3), term(0, 3));

    out.m[4] = sum4(term(7, 0), term(6, 0), term(4, 0), term(5, 0));
    out.m[5] = sum4(term(7, 1), term(6, 1), term(4, 1), term(5, 1));
    out.m[6] = sum4(term(7, 2), term(6, 2), term(4, 2), term(5, 2));
    out.m[7] = sum4(term(7, 3), term(6, 3), term(4, 3), term(5, 3));

    out.m[8] = sum4(term(11, 0), term(10, 0), term(8, 0), term(9, 0));
    out.m[9] = sum4(term(11, 1), term(10, 1), term(8, 1), term(9, 1));
    out.m[10] = sum4(term(11, 2), term(10, 2), term(8, 2), term(9, 2));
    out.m[11] = sum4(term(11, 3), term(10, 3), term(8, 3), term(9, 3));

    out.m[12] = sum4(term(15, 0), term(14, 0), term(12, 0), term(13, 0));
    out.m[13] = sum4(term(15, 1), term(14, 1), term(12, 1), term(13, 1));
    out.m[14] = sum4(term(15, 2), term(14, 2), term(12, 2), term(13, 2));
    out.m[15] = sum4(term(15, 3), term(14, 3), term(12, 3), term(13, 3));
    return out;
}

// The orientation block of ResetView/GetRay, in the original's exact SSE order.
void BuildCachedOrientation(Matrix3& orientation, const Quaternion& q)
{
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    const float z2 = MulSS(z, 2.0f), y2 = MulSS(y, 2.0f);
    const float xx = MulSS(x, MulSS(x, 2.0f));
    const float yy = MulSS(y, y2), zz = MulSS(z, z2);
    const float xy = MulSS(x, y2), xz = MulSS(x, z2);
    const float yz = MulSS(y, z2);
    const float wx = MulSS(w, MulSS(x, 2.0f)), wy = MulSS(w, y2), wz = MulSS(w, z2);
    orientation.m[0] = SubSS(1.0f, AddSS(zz, yy));
    orientation.m[1] = AddSS(wz, xy);
    orientation.m[2] = SubSS(xz, wy);
    orientation.m[3] = SubSS(xy, wz);
    orientation.m[4] = SubSS(1.0f, AddSS(zz, xx));
    orientation.m[5] = AddSS(wx, yz);
    orientation.m[6] = AddSS(wy, xz);
    orientation.m[7] = SubSS(yz, wx);
    orientation.m[8] = SubSS(1.0f, AddSS(yy, xx));
}

}  // namespace

Ray __cdecl Camera::GetRay(float x, float y) const
{
    Camera& self = *const_cast<Camera*>(this);
    BuildCachedOrientation(self.cachedOrientation, OriRef());
    const float* m = self.cachedOrientation.m;
    const Vector3& position = PosRef();

    Ray ray;
    if (projectionMode == ORTHOGRAPHIC) {
        ray.direction.x = SubSS(-0.0f, m[6]);
        ray.direction.y = SubSS(-0.0f, m[7]);
        ray.direction.z = SubSS(-0.0f, m[8]);
        const float sx = MulSS(SubSS(x, 0.5f), self.width2);
        const float sy = MulSS(SubSS(0.5f, y), self.height2);
        Vector3 offset;
        offset.x = AddSS(AddSS(MulSS(m[3], sy), MulSS(m[0], sx)), MulSS(m[6], 0.0f));
        offset.y = AddSS(AddSS(MulSS(m[4], sy), MulSS(m[1], sx)), MulSS(m[7], 0.0f));
        offset.z = AddSS(AddSS(MulSS(m[5], sy), MulSS(m[2], sx)), MulSS(m[8], 0.0f));
        AddVector3(ray.point, offset, position);
    } else if (projectionMode == PERSPECTIVE || projectionMode == TRANSITION) {
        const float sx = MulSS(SubSS(x, 0.5f), self.width);
        const float sy = MulSS(SubSS(0.5f, y), self.height);
        const float rx = AddSS(MulSS(m[0], sx), MulSS(m[3], sy));
        const float ry = AddSS(MulSS(m[1], sx), MulSS(m[4], sy));
        const float rz = AddSS(MulSS(m[2], sx), MulSS(m[5], sy));
        const float nx = MulSS(SubSS(-0.0f, m[6]), self.close);
        const float ny = MulSS(SubSS(-0.0f, m[7]), self.close);
        const float nz = MulSS(SubSS(-0.0f, m[8]), self.close);
        ray.direction.x = AddSS(nx, rx);
        ray.direction.y = AddSS(ny, ry);
        ray.direction.z = AddSS(nz, rz);
        X87Copy(ray.point.x, position.x);
        X87Copy(ray.point.y, position.y);
        X87Copy(ray.point.z, position.z);
    }
    Normalize(ray.direction);
    return ray;
}

// 0x10017320: copy + cell-space transform.
Camera::Camera(const Camera& o, const Matrix4& transform) : Particle()
{
    X87Copy(transition, o.transition);
    X87Copy(width, o.width);
    X87Copy(height, o.height);
    X87Copy(width2, o.width2);
    X87Copy(height2, o.height2);
    X87Copy(close, o.close);
    X87Copy(distant, o.distant);
    X87Copy(sx, o.sx);
    X87Copy(sy, o.sy);
    reflectionCull = o.reflectionCull;
    projectionMode = o.projectionMode;
    std::memcpy(&cachedViewTransform, &o.cachedViewTransform, sizeof cachedViewTransform);
    std::memcpy(&globalViewTransform, &o.globalViewTransform, sizeof globalViewTransform);
    std::memcpy(&cachedOrientation, &o.cachedOrientation, sizeof cachedOrientation);
    dirtyViewTransform = o.dirtyViewTransform;
    dirtyGlobalTransform = o.dirtyGlobalTransform;
    dirtyViewCone = o.dirtyViewCone;
    scene = o.scene;

    PosRef() = TransformPoint(transform, o.PosRef());
    for (int i = 0; i < 6; ++i) TransformPlane(frustum[i], transform, o.frustum[i]);
    if (o.dirtyViewTransform) o.UpdateCachedViewTransform();
    Matrix4 inverse = transform;
    inverse.Invert();
    const Matrix4 result = MultiplyMatrix(inverse, o.cachedViewTransform);
    for (int i = 0; i < 16; ++i) X87Copy(cachedViewTransform.m[i], result.m[i]);
    dirtyViewTransform = 0;
    dirtyViewCone = 0;
}

// The three remaining portal copies keep calling the original export (same pattern as the twelve U8 methods).
Camera::Camera(const Camera& o, const Vector2& min, const Vector2& max, const Vector3& position, float nearDistance)
{
    using Fn = void(__thiscall*)(Camera*, const Camera&, const Vector2&, const Vector2&, const Vector3&, float);
    static Fn fn = reinterpret_cast<Fn>(SolidsDetail::OrigProc(
        "??0Camera@Jet@@QAE@ABV01@ABVVector2@1@1ABVVector3@1@M@Z"));
    if (fn) fn(this, o, min, max, position, nearDistance);
}

Camera::Camera(const Camera& o, const Vector3* points, const Matrix4& transform)
{
    using Fn = void(__thiscall*)(Camera*, const Camera&, const Vector3*, const Matrix4&);
    static Fn fn = reinterpret_cast<Fn>(
        SolidsDetail::OrigProc("??0Camera@Jet@@QAE@ABV01@PBVVector3@1@ABVMatrix4@1@@Z"));
    if (fn) fn(this, o, points, transform);
}

Camera::Camera(const Camera& o, const Vector3* points, const Matrix4& transform, const Vector3& position,
               bool reflection)
{
    using Fn = void(__thiscall*)(Camera*, const Camera&, const Vector3*, const Matrix4&, const Vector3&, bool);
    static Fn fn = reinterpret_cast<Fn>(
        SolidsDetail::OrigProc("??0Camera@Jet@@QAE@ABV01@PBVVector3@1@ABVMatrix4@1@ABV21@_N@Z"));
    if (fn) fn(this, o, points, transform, position, reflection);
}

}  // namespace Jet
