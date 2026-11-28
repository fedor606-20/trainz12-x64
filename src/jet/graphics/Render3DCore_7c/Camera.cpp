// Unit U8 of Render3DCore_7c: Jet::Camera, transcribed from the TS12 assembly (addresses per member).
//
// The object is 0x264 bytes; see Render3DCoreCamera.h for the offsets. Ordinary members are __cdecl (this on the
// stack), constructors/assignment are __thiscall and the virtuals are __cdecl. The orientation matrix is stored
// column-major: +0x238..+0x240 right, +0x244..+0x24c up, +0x250..+0x258 forward. Float expressions keep the
// assembly's operand order; the table-driven magnitude/normalise are the same helpers U1d2 reconstructed
// (0x10008ab0/0x10008b50) and the x87 sequences are inline so the 80-bit intermediates and NaN quieting match.
#include <jet/Render3DCoreCamera.h>

#include <jet/Render3DCoreBounds.h>

#include <windows.h>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <new>

namespace Jet {

namespace {

using namespace SolidsDetail;

constexpr float kDefaultDist = 0.3f;    // CAMERA_DEFAULT_DIST
constexpr float kDefaultHeight = 0.195f;
constexpr float kDefaultWidth = 0.265f;
constexpr float kDefaultFar = 100000.0f;

float Neg(float x)
{
    // -0.0 - x, the original's negation: it also flips the sign of a zero.
    return SubSS(-0.0f, x);
}

// The original returns float values through st(0); an fld/fstp rounds a signalling NaN to a quiet one.
float Ret(float x)
{
    float r;
    X87Copy(r, x);
    return r;
}

// atan(x) with the original's FPATAN(x, 1), doubled in x87 and rounded once to float (GetHorizontalFOV).
float AtanDoubleX87(float x)
{
#if defined(_M_IX86)
    float r;
    __asm {
        fld dword ptr [x]
        fld1
        fpatan
        fadd st, st
        fstp dword ptr [r]
    }
    return r;
#else
    return static_cast<float>(2.0 * std::atan(static_cast<double>(x)));
#endif
}

// atan2(y, x) in x87, returned through st(0) (GetPitch/Yaw/Roll).
float Atan2X87(float y, float x)
{
#if defined(_M_IX86)
    float r;
    __asm {
        fld dword ptr [y]
        fld dword ptr [x]
        fpatan
        fstp dword ptr [r]
    }
    return r;
#else
    return static_cast<float>(std::atan2(static_cast<double>(y), static_cast<double>(x)));
#endif
}

// GetVerticalFOV: the relevant height is computed in SSE, then *0.5 and /near are x87 double precision.
float VerticalFovX87(float height, float nearDistance)
{
#if defined(_M_IX86)
    static const float kHalf = 0.5f;
    float r;
    __asm {
        fld dword ptr [height]
        fmul dword ptr [kHalf]
        fdiv dword ptr [nearDistance]
        fld1
        fpatan
        fadd st, st
        fstp dword ptr [r]
    }
    return r;
#else
    return static_cast<float>(2.0 * std::atan(static_cast<double>(height) * 0.5 / static_cast<double>(nearDistance)));
#endif
}

// The tan half-angle of the original's SetPerspective/SetTransition (x87 FPTAN on the float argument).
// h = 2 * near * tan(vFov/2) and w = h * aspect, all in x87 and rounded once to float each.
void PerspectiveExtentsX87(float vFov, float nearDistance, float aspect, float& outHeight, float& outWidth)
{
#if defined(_M_IX86)
    static const float kHalf = 0.5f;
    float h, w;
    __asm {
        fld dword ptr [nearDistance]
        fld dword ptr [vFov]
        fmul dword ptr [kHalf]
        fptan
        fstp st(0)
        fmul st, st(1)
        fadd st, st
        fst dword ptr [h]
        fmul dword ptr [aspect]
        fstp dword ptr [w]
        fstp st(0)
    }
    outHeight = h;
    outWidth = w;
#else
    const float t = static_cast<float>(std::tan(static_cast<double>(vFov) * 0.5));
    outHeight = 2.0f * nearDistance * t;
    outWidth = outHeight * aspect;
#endif
}

float TanHalfX87(float angle)
{
#if defined(_M_IX86)
    static const float kHalf = 0.5f;
    float r;
    __asm {
        fld dword ptr [angle]
        fmul dword ptr [kHalf]
        fptan
        fstp st(0)
        fstp dword ptr [r]
    }
    return r;
#else
    return static_cast<float>(std::tan(static_cast<double>(angle) * 0.5));
#endif
}

float LengthFast(const Vector3& v)
{
    const float d = AddSS(AddSS(MulSS(v.x, v.x), MulSS(v.y, v.y)), MulSS(v.z, v.z));
    return StdMath::Sqrt(d);
}

// 0x10008b50: v /= |v|, no zero check; the caller has already tested the squared length.
void NormalizeFast(Vector3& v)
{
    const float x = v.x, y = v.y, z = v.z;
    const float inv = StdMath::InvSqrt(AddSS(AddSS(MulSS(x, x), MulSS(y, y)), MulSS(z, z)));
    v.x = MulSS(x, inv);
    v.y = MulSS(y, inv);
    v.z = MulSS(z, inv);
}

float DotOrdered(const Vector3& n, const Vector3& p)
{
    // nx*px + nz*pz + ny*py, the order every plane distance uses
    return AddSS(AddSS(MulSS(n.x, p.x), MulSS(n.z, p.z)), MulSS(n.y, p.y));
}

void NormalizePlane(Vector3& n, float& d, const Vector3& position)
{
    Normalize(n);
    d = DotOrdered(n, position);
}

HMODULE OrigModule()
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("Render3DCore_7c_orig.dll");
        return m ? m : LoadLibraryA("Render3DCore_7c_orig.dll");
    }();
    return module;
}

template <class F>
F OrigProc(const char* name)
{
    HMODULE m = OrigModule();
    return m ? reinterpret_cast<F>(GetProcAddress(m, name)) : nullptr;
}

// The bounding volume is U3's class and every notification below is a local body, so call it directly.
void BvRecalculateGlobalOrientation(void* bv)
{
    BoundsAccess::RecalculateGlobalOrientation(static_cast<ParticleBoundingVolume*>(bv));
}

void BvRecalculateGlobalPosition(void* bv)
{
    BoundsAccess::RecalculateGlobalPosition(static_cast<ParticleBoundingVolume*>(bv));
}

void BvSetPositionChanged(void* bv, const Vector3& oldPosition, const Vector3& newPosition)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetPositionChanged(oldPosition, newPosition);
}

void BvSetOrientationChanged(void* bv, const Quaternion& oldOrientation, const Quaternion& newOrientation)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetOrientationChanged(oldOrientation, newOrientation);
}

void BvSetBothChanged(void* bv, const Vector3& oldPosition, const Vector3& newPosition,
                      const Quaternion& oldOrientation, const Quaternion& newOrientation)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetPositionOrientationChanged(oldPosition, newPosition, oldOrientation,
                                                                             newOrientation);
}

}  // namespace

float Camera::pixelError = 2.0f;

// The element kind of a camera (Particle::GetID is 1).
int __cdecl Camera::GetID() const
{
    return 0xb;
}

bool __cdecl Camera::IsOfType(int id) const
{
    return id == 0xb || id == 1;
}

Particle* __cdecl Camera::CreateInstance() const
{
    Camera* instance = static_cast<Camera*>(Alloc::New(sizeof(Camera)));
    if (!instance) return nullptr;
    ::new (instance) Camera();
    InitInstance(*instance);
    return instance;
}

Element* __cdecl Camera::GetScene()
{
    return scene;
}

Plane* __cdecl Camera::GetFrustum()
{
    if (dirtyViewCone) UpdateViewCone();
    return frustum;
}

const Plane* __cdecl Camera::GetFrustum() const
{
    if (dirtyViewCone) UpdateViewCone();
    return frustum;
}

float __cdecl Camera::GetNearClipDistance() const
{
    return Ret(close);
}

float __cdecl Camera::GetFarClipDistance() const
{
    return Ret(distant);
}

// Relevant near-plane height for the current projection mode (the lerp of the transitional mode).
namespace {
float ModeHeight(int mode, float h, float h2, float blend)
{
    if (mode == 0) return h;
    if (mode == 1) return AddSS(MulSS(SubSS(1.0f, blend), h), MulSS(h2, blend));
    if (mode == 2) return h2;
    return 0.0f;
}

float ModeWidth(int mode, float w, float w2, float blend)
{
    if (mode == 0) return w;
    if (mode == 1) return AddSS(MulSS(SubSS(1.0f, blend), w), MulSS(w2, blend));
    if (mode == 2) return w2;
    return 0.0f;
}
}  // namespace

float __cdecl Camera::GetNearClipHeight() const
{
    return Ret(ModeHeight(projectionMode, height, height2, transition));
}

float __cdecl Camera::GetNearClipWidth() const
{
    return Ret(ModeWidth(projectionMode, width, width2, transition));
}

const Matrix3& __cdecl Camera::GetCachedOrientation() const
{
    return cachedOrientation;
}

const Matrix4& __cdecl Camera::GetCellSpaceViewTransform() const
{
    if (dirtyViewTransform) UpdateCachedViewTransform();
    return cachedViewTransform;
}

const Matrix4& __cdecl Camera::GetViewTransform() const
{
    if (dirtyViewTransform) UpdateCachedViewTransform();
    return cachedViewTransform;
}

const Matrix4& __cdecl Camera::GetGlobalViewTransform() const
{
    if (dirtyGlobalTransform) UpdateGlobalViewTransform();
    return globalViewTransform;
}

float __cdecl Camera::GetHorizontalFOV() const
{
    const float w = ModeWidth(projectionMode, width, width2, transition);
    const float ratio = DivSS(MulSS(w, 0.5f), close);
    return AtanDoubleX87(ratio);
}

float __cdecl Camera::GetVerticalFOV() const
{
    const float h = ModeHeight(projectionMode, height, height2, transition);
    const float nearDistance = close;
    static const float kHalf = 0.5f;
#if defined(_M_IX86)
    float r;
    __asm {
        fld dword ptr [h]
        fmul dword ptr [kHalf]
        fdiv dword ptr [nearDistance]
        fld1
        fpatan
        fadd st, st
        fstp dword ptr [r]
    }
    return r;
#else
    return VerticalFovX87(h, nearDistance);
#endif
}

Vector3 __cdecl Camera::GetRight() const
{
    Vector3 v;
    X87Copy(v.x, cachedOrientation.m[0]);
    X87Copy(v.y, cachedOrientation.m[1]);
    X87Copy(v.z, cachedOrientation.m[2]);
    return v;
}

Vector3 __cdecl Camera::GetUp() const
{
    Vector3 v;
    X87Copy(v.x, cachedOrientation.m[3]);
    X87Copy(v.y, cachedOrientation.m[4]);
    X87Copy(v.z, cachedOrientation.m[5]);
    return v;
}

Vector3 __cdecl Camera::GetForward() const
{
    return Vector3{Neg(cachedOrientation.m[6]), Neg(cachedOrientation.m[7]), Neg(cachedOrientation.m[8])};
}

float __cdecl Camera::GetPitch() const
{
    return Atan2X87(cachedOrientation.m[1], cachedOrientation.m[0]);
}

float __cdecl Camera::GetYaw() const
{
#if defined(_M_IX86)
    float r;
    const float* m = cachedOrientation.m;
    __asm {
        mov eax, m
        fld dword ptr [eax + 24]
        fchs
        fld dword ptr [eax + 28]
        fchs
        fxch
        fpatan
        fstp dword ptr [r]
    }
    return r;
#else
    return Atan2X87(-cachedOrientation.m[7], -cachedOrientation.m[6]);
#endif
}

float __cdecl Camera::GetRoll() const
{
    return Atan2X87(cachedOrientation.m[4], cachedOrientation.m[3]);
}

void __cdecl Camera::SetScene(Element* root)
{
    scene = root;
}

// 0x1000a1f0: tell the bounding volume the old position, store the new one, then ResetViewCone (vtable slot 54).
void __cdecl Camera::SetPosition(const Vector3& nextPosition)
{
    BvSetPositionChanged(BvRef(), PosRef(), nextPosition);
    X87Copy(PosRef().x, nextPosition.x);
    X87Copy(PosRef().y, nextPosition.y);
    X87Copy(PosRef().z, nextPosition.z);
    ResetViewCone();
}

void __cdecl Camera::SetOrientation(const Quaternion& nextOrientation)
{
    BvSetOrientationChanged(BvRef(), OriRef(), nextOrientation);
    X87Copy(OriRef().x, nextOrientation.x);
    X87Copy(OriRef().y, nextOrientation.y);
    X87Copy(OriRef().z, nextOrientation.z);
    X87Copy(OriRef().w, nextOrientation.w);
    ResetViewCone();
}

void __cdecl Camera::SetReferenceFrame(const Vector3& nextPosition, const Quaternion& nextOrientation)
{
    BvSetBothChanged(BvRef(), PosRef(), nextPosition, OriRef(), nextOrientation);
    X87Copy(PosRef().x, nextPosition.x);
    X87Copy(PosRef().y, nextPosition.y);
    X87Copy(PosRef().z, nextPosition.z);
    X87Copy(OriRef().x, nextOrientation.x);
    X87Copy(OriRef().y, nextOrientation.y);
    X87Copy(OriRef().z, nextOrientation.z);
    X87Copy(OriRef().w, nextOrientation.w);
    ResetViewCone();
}

ClipResult __cdecl Camera::Clip3D(const Vector3& center, float radius) const
{
    if (dirtyViewCone) UpdateViewCone();

    const Vector3 c = center;
    int i = 0;
    for (;;) {
        const Plane& p = frustum[i];
        float d = SubSS(AddSS(AddSS(MulSS(p.normal.x, c.x), MulSS(p.normal.z, c.z)),
                              MulSS(p.normal.y, c.y)),
                        p.d);
        int result;
        if (!(d >= 0.0f)) {  // COMISS d,0 / JC: negative and unordered both take this path
            result = (!(radius > Neg(d))) ? BACK : COINCIDENT;  // JA, unordered falls to BACK
        } else if (radius <= d || d != d) {
            result = FRONT;  // JBE also takes an unordered compare
        } else {
            result = COINCIDENT;
        }
        if (result == BACK) return BACK;
        ++i;
        if (result == COINCIDENT) {
            for (; i < 6; ++i) {
                const Plane& q = frustum[i];
                d = SubSS(AddSS(AddSS(MulSS(q.normal.x, c.x), MulSS(q.normal.z, c.z)),
                                MulSS(q.normal.y, c.y)),
                          q.d);
                if (d < 0.0f && !(radius > Neg(d))) return BACK;  // JBE takes unordered (NaN radius) as BACK
            }
            return COINCIDENT;
        }
        if (i >= 6) return FRONT;
    }
}

ClipResult __cdecl Camera::Clip3D(const Vector3* array, int num) const
{
    if (dirtyViewCone) UpdateViewCone();

    int i = 0;
    for (;;) {
        const ClipResult side = frustum[i].WhichSide(num, array);
        if (side == BACK) return BACK;
        ++i;
        if (side == COINCIDENT) {
            for (; i < 6; ++i) {
                if (frustum[i].WhichSide(num, array) == BACK) return BACK;
            }
            return COINCIDENT;
        }
        if (i >= 6) return FRONT;
    }
}

ClipResult __cdecl Camera::ClipNear(Vector3& position) const
{
    if (dirtyViewCone) UpdateViewCone();

    const Plane& p = frustum[0];
    const float d = SubSS(AddSS(AddSS(MulSS(p.normal.z, position.z), MulSS(p.normal.y, position.y)),
                                MulSS(p.normal.x, position.x)),
                          p.d);
    if (d > 0.0001f) return FRONT;
    if (-0.0001f > d) return BACK;  // JA
    return COINCIDENT;
}

bool __cdecl Camera::BigEnough(const Vector3& center, float radius) const
{
    const float dx = SubSS(center.x, PosRef().x);
    const float dz = SubSS(center.z, PosRef().z);
    const float dy = SubSS(center.y, PosRef().y);
    const float sum = AddSS(AddSS(MulSS(dx, dx), MulSS(dz, dz)), MulSS(dy, dy));

    unsigned int bits;
    std::memcpy(&bits, &sum, sizeof bits);
    const unsigned int approxBits = 0x5f3759dfu - (bits >> 1);
    float a;
    std::memcpy(&a, &approxBits, sizeof a);
    const float distance = MulSS(MulSS(a, SubSS(1.5f, MulSS(MulSS(sum, 0.5f), MulSS(a, a)))), sum);
    if (distance <= 0.0001f) return true;
    const float ratio = DivSS(radius, distance);
    return ratio > MulSS(pixelError, 0.0015625f);
}

bool __thiscall Camera::IsReflectionCull() const
{
    return reflectionCull != 0;
}

Matrix4 __cdecl Camera::GetPerspectiveTransform() const
{
    Matrix4 m;
    const float n = close;
    const float f = distant;
    const float a = MulSS(DivSS(n, width), 2.0f);
    const float b = MulSS(DivSS(n, height), 2.0f);
    const float denom = SubSS(f, n);
    const float sum = AddSS(f, n);
    m.m[0] = a;
    m.m[1] = 0.0f;
    m.m[2] = 0.0f;
    m.m[3] = 0.0f;
    m.m[4] = 0.0f;
    m.m[5] = b;
    m.m[6] = 0.0f;
    m.m[7] = 0.0f;
    m.m[8] = 0.0f;
    m.m[9] = 0.0f;
    m.m[10] = SubSS(0.0f, DivSS(sum, denom));
    m.m[11] = -1.0f;
    m.m[12] = 0.0f;
    m.m[13] = 0.0f;
    m.m[14] = DivSS(MulSS(MulSS(f, n), -2.0f), denom);
    m.m[15] = 0.0f;
    return m;
}

float __thiscall Camera::GetRelevantWidth() const
{
    return Ret(ModeWidth(projectionMode, width, width2, transition));
}

float __thiscall Camera::GetRelevantHeight() const
{
    return Ret(ModeHeight(projectionMode, height, height2, transition));
}

void __cdecl Camera::SetNearDistance(float distance)
{
    const float factor = DivSS(distance, close);
    close = distance;
    width = MulSS(width, factor);
    height = MulSS(height, factor);
    width2 = MulSS(width2, factor);
    height2 = MulSS(height2, factor);
    ResetViewCone();
}

void __cdecl Camera::SetFarDistance(float distance)
{
    distant = distance;
    ResetViewCone();
}

void __cdecl Camera::SetPerspective(float vFov, float aspect)
{
    projectionMode = PERSPECTIVE;
    float h, w;
    PerspectiveExtentsX87(vFov, close, aspect, h, w);
    ResetView(close, w, h, 0.0f, 0.0f);
}

void __cdecl Camera::SetTransition(float blend, float vFov, float height_, float aspect)
{
    if (blend <= 0.0f) {
        SetPerspective(vFov, aspect);
        return;
    }
    if (blend >= 1.0f) {
        projectionMode = ORTHOGRAPHIC;
        ResetView(close, 0.0f, 0.0f, MulSS(height_, aspect), height_);
        return;
    }
    transition = blend;
    projectionMode = TRANSITION;
    float h, w;
    PerspectiveExtentsX87(vFov, close, aspect, h, w);
    ResetView(close, w, h, MulSS(height_, aspect), height_);
}

void __cdecl Camera::SetOrthographic(float height_, float aspect)
{
    projectionMode = ORTHOGRAPHIC;
    ResetView(close, 0.0f, 0.0f, MulSS(height_, aspect), height_);
}

Camera::ProjectionMode __cdecl Camera::GetProjectionMode() const
{
    return projectionMode;
}

void __thiscall Camera::SetProjectionData(const Camera& o)
{
    transition = o.transition;
    width = o.width;
    height = o.height;
    width2 = o.width2;
    height2 = o.height2;
    close = o.close;
    distant = o.distant;
    sx = o.sx;
    sy = o.sy;
    for (int i = 0; i < 6; ++i) frustum[i] = o.frustum[i];
    projectionMode = o.projectionMode;
    ResetViewCone();
}

void __cdecl Camera::GetClippingPlanes(float xL, float yT, float xR, float yB, PlaneList& planes) const
{
    using Fn = void(__cdecl*)(const Camera*, float, float, float, float, PlaneList&);
    static Fn fn =
        OrigProc<Fn>("?GetClippingPlanes@Camera@Jet@@QBAXMMMMAAV?$DynamicArray@VPlane@Jet@@$0BA@U?$Less@VPlane@Jet@@V12@@2@@2@@Z");
    if (fn) fn(this, xL, yT, xR, yB, planes);
}

void __cdecl Camera::Cull(ViewPoint& view) const
{
    using Fn = void(__cdecl*)(const Camera*, ViewPoint&);
    static Fn fn = OrigProc<Fn>("?Cull@Camera@Jet@@UBAXAAVViewPoint@2@@Z");
    if (fn) fn(this, view);
}

void __cdecl Camera::Translate(const Vector3& delta)
{
    Particle::Translate(delta);
    ResetViewCone();
}

void __cdecl Camera::Rotate(const Matrix3& rot)
{
    Particle::Rotate(rot);
    ResetViewCone();
}

void __cdecl Camera::Rotate(const Quaternion& rot)
{
    Particle::Rotate(rot);
    ResetViewCone();
}

void __cdecl Camera::SelfRotate(const Matrix3& rot)
{
    Particle::SelfRotate(rot);
    ResetViewCone();
}

void __cdecl Camera::SelfRotate(const Quaternion& rot)
{
    Particle::SelfRotate(rot);
    ResetViewCone();
}

void __cdecl Camera::Orbit(const Vector3& target, float deltaYaw, float deltaPitch, const Vector3& up)
{
    using Fn = void(__cdecl*)(Camera*, const Vector3&, float, float, const Vector3&);
    static Fn fn = OrigProc<Fn>("?Orbit@Camera@Jet@@QAAXABVVector3@2@MM0@Z");
    if (fn) fn(this, target, deltaYaw, deltaPitch, up);
}

void __cdecl Camera::Perspective(Vector3& v) const
{
    const float zeroX = MulSS(v.x, 0.0f);
    const float zeroY = MulSS(v.y, 0.0f);
    const float inv = DivSS(1.0f, AddSS(SubSS(zeroX, MulSS(v.z, 1.0f)), zeroY));
    const float x = v.x, y = v.y, z = v.z;
    const float d = SubSS(distant, close);
    const float nearFar = DivSS(AddSS(distant, close), d);
    const float depth = DivSS(MulSS(MulSS(distant, close), -2.0f), d);
    v.x = MulSS(inv, AddSS(AddSS(MulSS(MulSS(DivSS(close, width), 2.0f), x), zeroY), MulSS(z, 0.0f)));
    v.y = MulSS(AddSS(AddSS(MulSS(MulSS(y, DivSS(close, height)), 2.0f), zeroX), MulSS(z, 0.0f)), inv);
    v.z = MulSS(AddSS(AddSS(AddSS(MulSS(z, SubSS(-0.0f, nearFar)), zeroX), zeroY), depth), inv);
}

void __cdecl Camera::TrainzUnPerspective(float x, float y, float z, Vector3& v) const
{
    // COMISS/JBE chains from the original: unordered compares keep z, so the branch order is observable.
    const float nearDepth = Neg(close);
    const float farDepth = Neg(distant);
    float depth = z;
    if (!(z > nearDepth)) {
        if (farDepth > z) depth = farDepth;
    } else {
        depth = nearDepth;
    }
    v.z = depth;
    v.x = MulSS(DivSS(SubSS(x, 0.5f), sx), depth);
    v.y = MulSS(DivSS(SubSS(y, 0.5f), sy), depth);
}

bool __cdecl Camera::Transform2D(Vector3& v, float tolerance) const
{
    if (dirtyViewCone) UpdateViewCone();

    const Plane& nearPlane = frustum[0];
    if (!(AddSS(AddSS(MulSS(nearPlane.normal.y, v.y), MulSS(nearPlane.normal.z, v.z)),
                MulSS(nearPlane.normal.x, v.x)) -
          nearPlane.d >
          0.0001f))
        return false;

    const Vector3 d{SubSS(v.x, PosRef().x), SubSS(v.y, PosRef().y), SubSS(v.z, PosRef().z)};
    const Vector3 o{cachedOrientation.m[0], cachedOrientation.m[1], cachedOrientation.m[2]};
    const Vector3 u{cachedOrientation.m[3], cachedOrientation.m[4], cachedOrientation.m[5]};
    const Vector3 f{cachedOrientation.m[6], cachedOrientation.m[7], cachedOrientation.m[8]};
    v.x = AddSS(AddSS(MulSS(o.y, d.y), MulSS(o.z, d.z)), MulSS(o.x, d.x));
    v.y = AddSS(AddSS(MulSS(u.x, d.x), MulSS(u.y, d.y)), MulSS(u.z, d.z));
    v.z = AddSS(AddSS(MulSS(f.x, d.x), MulSS(f.y, d.y)), MulSS(f.z, d.z));
    Perspective(v);
    v.x = MulSS(AddSS(v.x, 1.0f), 0.5f);
    v.y = MulSS(AddSS(v.y, 1.0f), 0.5f);
    return tolerance < v.x && v.x < SubSS(1.0f, tolerance) && tolerance < v.y && v.y < SubSS(1.0f, tolerance);
}

bool __thiscall Camera::TrainzTransform2D(Vector3& v, float tolerance) const
{
    if (dirtyViewCone) UpdateViewCone();

    const Plane& nearPlane = frustum[0];
    if (!(AddSS(AddSS(MulSS(nearPlane.normal.y, v.y), MulSS(nearPlane.normal.z, v.z)),
                MulSS(nearPlane.normal.x, v.x)) -
          nearPlane.d >
          0.0001f))
        return false;

    const Vector3 d{SubSS(v.x, PosRef().x), SubSS(v.y, PosRef().y), SubSS(v.z, PosRef().z)};
    const Vector3 o{cachedOrientation.m[0], cachedOrientation.m[1], cachedOrientation.m[2]};
    const Vector3 u{cachedOrientation.m[3], cachedOrientation.m[4], cachedOrientation.m[5]};
    const Vector3 f{cachedOrientation.m[6], cachedOrientation.m[7], cachedOrientation.m[8]};
    const float cx = AddSS(AddSS(MulSS(o.y, d.y), MulSS(o.z, d.z)), MulSS(o.x, d.x));
    const float cy = AddSS(AddSS(MulSS(u.x, d.x), MulSS(u.y, d.y)), MulSS(u.z, d.z));
    const float cz = AddSS(AddSS(MulSS(f.x, d.x), MulSS(f.y, d.y)), MulSS(f.z, d.z));
    const float inverse = DivSS(1.0f, cz);
    v.z = inverse;
    v.x = AddSS(MulSS(MulSS(DivSS(inverse, width), close), cx), 0.5f);
    v.y = AddSS(MulSS(MulSS(DivSS(inverse, height), close), cy), 0.5f);
    return tolerance < v.x && v.x < SubSS(1.0f, tolerance) && tolerance < v.y && v.y < SubSS(1.0f, tolerance);
}


void __cdecl Camera::AlignToTarget(const Vector3& target, const Vector3& up)
{
    Vector3 d{SubSS(target.x, PosRef().x), SubSS(target.y, PosRef().y), SubSS(target.z, PosRef().z)};
    if (AddSS(AddSS(MulSS(d.x, d.x), MulSS(d.y, d.y)), MulSS(d.z, d.z)) == 0.0f) {
        d = Vector3{1.0f, 0.0f, 0.0f};
    } else {
        NormalizeFast(d);
    }

    Vector3 right{SubSS(MulSS(up.z, d.y), MulSS(up.y, d.z)), SubSS(MulSS(up.x, d.z), MulSS(up.z, d.x)),
                  SubSS(MulSS(up.y, d.x), MulSS(up.x, d.y))};
    Vector3 second;
    if (LengthFast(right) <= 0.0001f) {
        right = Vector3{1.0f, 0.0f, 0.0f};
        second = Vector3{0.0f, 1.0f, 0.0f};
    } else {
        NormalizeFast(right);
        second = Vector3{SubSS(MulSS(right.y, d.z), MulSS(d.y, right.z)),
                         SubSS(MulSS(d.x, right.z), MulSS(right.x, d.z)),
                         SubSS(MulSS(right.x, d.y), MulSS(d.x, right.y))};
    }

    Matrix3 m;
    m.m[0] = right.x;
    m.m[1] = right.y;
    m.m[2] = right.z;
    m.m[3] = second.x;
    m.m[4] = second.y;
    m.m[5] = second.z;
    m.m[6] = SubSS(-0.0f, d.x);
    m.m[7] = SubSS(-0.0f, d.y);
    m.m[8] = SubSS(-0.0f, d.z);
    Quaternion q;
    q.Set(m);
    SetOrientation(q);
}

void __cdecl Camera::SetProjected3D(RenderKernel* kernel)
{
    Display* display = kernel ? kernel->GetDisplay() : nullptr;
    if (!display) return;
    switch (projectionMode) {
    case PERSPECTIVE:
        display->Set3DView(width, height, close, distant);
        break;
    case TRANSITION:
        display->Set3DTransitionalView(transition, close, distant, width2, height2, width, height);
        break;
    case ORTHOGRAPHIC:
        display->Set2DView(Neg(MulSS(width2, 0.5f)), MulSS(width2, 0.5f), Neg(MulSS(height2, 0.5f)),
                           MulSS(height2, 0.5f), 0.0f, distant);
        break;
    default:
        break;
    }
}

void __cdecl Camera::SetProjected3DOffset(RenderKernel* kernel, float offset)
{
    Display* display = kernel ? kernel->GetDisplay() : nullptr;
    if (display) display->Set3DViewOffset(width, height, close, distant, offset);
}

void __cdecl Camera::SetRenderView(RenderKernel* kernel)
{
    if (dirtyViewTransform) UpdateCachedViewTransform();
    Display* display = kernel ? kernel->GetDisplay() : nullptr;
    if (display) VCall<void>(display, 0x54, &cachedViewTransform);
}

void __cdecl Camera::Init(float w, float h, float nearDistance)
{
    dirtyViewTransform = 1;
    dirtyGlobalTransform = 1;
    dirtyViewCone = 1;
    reflectionCull = 0;
    projectionMode = PERSPECTIVE;
    scene = nullptr;
    distant = kDefaultFar;
    ResetView(nearDistance, w, h, 0.0f, 0.0f);
}

void __cdecl Camera::ResetView(float nearDistance, float w, float h, float w2, float h2)
{
    const ProjectionMode mode = projectionMode;
    close = nearDistance;
    width = w;
    height = h;
    width2 = w2;
    height2 = h2;
    sx = DivSS(close, ModeWidth(mode, width, width2, transition));
    sy = DivSS(close, ModeHeight(mode, height, height2, transition));

    // cachedOrientation from the quaternion, column-major
    const float x = OriRef().x, y = OriRef().y, z = OriRef().z, qw = OriRef().w;
    const float z2 = MulSS(z, 2.0f), y2 = MulSS(y, 2.0f);
    const float xx = MulSS(x, MulSS(x, 2.0f));
    const float yy = MulSS(y, y2), zz = MulSS(z, z2);
    const float xy = MulSS(x, y2), xz = MulSS(x, z2);
    const float yz = MulSS(y, z2);
    const float wx = MulSS(qw, MulSS(x, 2.0f)), wy = MulSS(qw, y2), wz = MulSS(qw, z2);
    cachedOrientation.m[0] = SubSS(1.0f, AddSS(zz, yy));
    cachedOrientation.m[1] = AddSS(wz, xy);
    cachedOrientation.m[2] = SubSS(xz, wy);
    cachedOrientation.m[3] = SubSS(xy, wz);
    cachedOrientation.m[4] = SubSS(1.0f, AddSS(zz, xx));
    cachedOrientation.m[5] = AddSS(wx, yz);
    cachedOrientation.m[6] = AddSS(wy, xz);
    cachedOrientation.m[7] = SubSS(yz, wx);
    cachedOrientation.m[8] = SubSS(1.0f, AddSS(yy, xx));
    ResetViewCone();
}

void __cdecl Camera::InitInstance(Camera& instance) const
{
    Particle::InitInstance(instance);
    const char* src = reinterpret_cast<const char*>(this);
    char* dst = reinterpret_cast<char*>(&instance);
    for (unsigned off = 0x12c; off < 0x1b0; off += 4) X87Copy(*reinterpret_cast<float*>(dst + off),
                                                             *reinterpret_cast<const float*>(src + off));
    for (unsigned off = 0x1f8; off < 0x238; off += 4) X87Copy(*reinterpret_cast<float*>(dst + off),
                                                              *reinterpret_cast<const float*>(src + off));
    for (unsigned off = 0x1b8; off < 0x1f8; off += 4) X87Copy(*reinterpret_cast<float*>(dst + off),
                                                              *reinterpret_cast<const float*>(src + off));
    for (unsigned off = 0x238; off < 0x25c; off += 4) X87Copy(*reinterpret_cast<float*>(dst + off),
                                                              *reinterpret_cast<const float*>(src + off));
    instance.reflectionCull = reflectionCull;
    instance.scene = scene;
    instance.projectionMode = projectionMode;
    instance.dirtyViewTransform = dirtyViewTransform;
    instance.dirtyGlobalTransform = dirtyGlobalTransform;
    instance.dirtyViewCone = dirtyViewCone;
}

void __cdecl Camera::UpdateViewCone() const
{
    using Fn = void(__cdecl*)(const Camera*);
    static Fn fn = OrigProc<Fn>("?UpdateViewCone@Camera@Jet@@IBAXXZ");
    if (fn) fn(this);
}

void __cdecl Camera::UpdateCachedViewTransform() const
{
    Camera* const self = const_cast<Camera*>(this);
    const float* const o = cachedOrientation.m;
    Matrix4& m = self->cachedViewTransform;
    m.m[0] = o[0];
    m.m[1] = o[3];
    m.m[2] = o[6];
    m.m[3] = 0.0f;
    m.m[4] = o[1];
    m.m[5] = o[4];
    m.m[6] = o[7];
    m.m[7] = 0.0f;
    m.m[8] = o[2];
    m.m[9] = o[5];
    m.m[10] = o[8];
    m.m[11] = 0.0f;
    m.m[12] = 0.0f;
    m.m[13] = 0.0f;
    m.m[14] = 0.0f;
    m.m[15] = 1.0f;
    const float nx = SubSS(-0.0f, PosRef().x);
    const float ny = SubSS(-0.0f, PosRef().y);
    const float nz = SubSS(-0.0f, PosRef().z);
    m.m[12] = AddSS(AddSS(AddSS(MulSS(o[2], nz), MulSS(o[1], ny)), MulSS(o[0], nx)), m.m[12]);
    m.m[13] = AddSS(AddSS(AddSS(MulSS(o[5], nz), MulSS(o[4], ny)), MulSS(o[3], nx)), m.m[13]);
    m.m[14] = AddSS(AddSS(AddSS(MulSS(o[8], nz), MulSS(o[7], ny)), MulSS(o[6], nx)), m.m[14]);
    self->dirtyViewTransform = 0;
}

void __cdecl Camera::UpdateGlobalViewTransform() const
{
    using Fn = void(__cdecl*)(const Camera*);
    static Fn fn = OrigProc<Fn>("?UpdateGlobalViewTransform@Camera@Jet@@MBAXXZ");
    if (fn) fn(this);
}

void __cdecl Camera::ResetViewCone()
{
    const float x = OriRef().x, y = OriRef().y, z = OriRef().z, qw = OriRef().w;
    const float z2 = MulSS(z, 2.0f), y2 = MulSS(y, 2.0f);
    const float xx = MulSS(x, MulSS(x, 2.0f));
    const float yy = MulSS(y, y2), zz = MulSS(z, z2);
    const float xy = MulSS(x, y2), xz = MulSS(x, z2);
    const float yz = MulSS(y, z2);
    const float wx = MulSS(qw, MulSS(x, 2.0f)), wy = MulSS(qw, y2), wz = MulSS(qw, z2);
    cachedOrientation.m[0] = SubSS(1.0f, AddSS(zz, yy));
    cachedOrientation.m[1] = AddSS(wz, xy);
    cachedOrientation.m[2] = SubSS(xz, wy);
    cachedOrientation.m[3] = SubSS(xy, wz);
    cachedOrientation.m[4] = SubSS(1.0f, AddSS(zz, xx));
    cachedOrientation.m[5] = AddSS(wx, yz);
    cachedOrientation.m[6] = AddSS(wy, xz);
    cachedOrientation.m[7] = SubSS(yz, wx);
    cachedOrientation.m[8] = SubSS(1.0f, AddSS(yy, xx));
    dirtyViewCone = 1;
    dirtyViewTransform = 1;
    dirtyGlobalTransform = 1;
}

// ---- constructors, copy and assignment (0x131c0..0x13780, 0x9800, 0x9920) -------------------------------

Camera::Camera()
{
    Init(kDefaultWidth, kDefaultHeight, kDefaultDist);
}

Camera::Camera(float dist, float height_, float width_)
{
    Init(width_, height_, dist);
}

Camera::Camera(const Vector3& position_, const Quaternion& orientation_)
{
    Init(kDefaultWidth, kDefaultHeight, kDefaultDist);
    X87Copy(PosRef().x, position_.x);
    X87Copy(PosRef().y, position_.y);
    X87Copy(PosRef().z, position_.z);
    X87Copy(OriRef().x, orientation_.x);
    X87Copy(OriRef().y, orientation_.y);
    X87Copy(OriRef().z, orientation_.z);
    X87Copy(OriRef().w, orientation_.w);
}

Camera::Camera(const Vector3& position_, const Matrix3& orientation_)
{
    Init(kDefaultWidth, kDefaultHeight, kDefaultDist);
    X87Copy(PosRef().x, position_.x);
    X87Copy(PosRef().y, position_.y);
    X87Copy(PosRef().z, position_.z);
    Quaternion q;
    q.Set(orientation_);
    OriRef().x = q.x;
    OriRef().y = q.y;
    OriRef().z = q.z;
    OriRef().w = q.w;
}

Camera::Camera(const Vector3& position_, float rotateX, float rotateY, float rotateZ)
{
    Init(kDefaultWidth, kDefaultHeight, kDefaultDist);
    X87Copy(PosRef().x, position_.x);
    X87Copy(PosRef().y, position_.y);
    X87Copy(PosRef().z, position_.z);
    Quaternion q;
    q.Set(rotateX, rotateY, rotateZ);
    OriRef().x = q.x;
    OriRef().y = q.y;
    OriRef().z = q.z;
    OriRef().w = q.w;
}

Camera::Camera(const Vector3& position_, const Quaternion& orientation_, float dist, float height_, float width_)
{
    Init(width_, height_, dist);
    X87Copy(PosRef().x, position_.x);
    X87Copy(PosRef().y, position_.y);
    X87Copy(PosRef().z, position_.z);
    X87Copy(OriRef().x, orientation_.x);
    X87Copy(OriRef().y, orientation_.y);
    X87Copy(OriRef().z, orientation_.z);
    X87Copy(OriRef().w, orientation_.w);
}

Camera::Camera(const Vector3& position_, const Matrix3& orientation_, float dist, float height_, float width_)
{
    Init(width_, height_, dist);
    X87Copy(PosRef().x, position_.x);
    X87Copy(PosRef().y, position_.y);
    X87Copy(PosRef().z, position_.z);
    Quaternion q;
    q.Set(orientation_);
    OriRef().x = q.x;
    OriRef().y = q.y;
    OriRef().z = q.z;
    OriRef().w = q.w;
}

Camera::Camera(const Vector3& position_, float roll, float pitch, float yaw, float dist, float height_,
               float width_)
{
    scene = nullptr;
    Init(width_, height_, dist);
    X87Copy(PosRef().x, position_.x);
    X87Copy(PosRef().y, position_.y);
    X87Copy(PosRef().z, position_.z);
    Quaternion q;
    q.Set(roll, pitch, yaw);
    OriRef().x = q.x;
    OriRef().y = q.y;
    OriRef().z = q.z;
    OriRef().w = q.w;
}

Camera::Camera(const Camera& o) : Particle(o)
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
    std::memcpy(frustum, o.frustum, sizeof frustum);
    reflectionCull = o.reflectionCull;
    projectionMode = o.projectionMode;
    std::memcpy(&cachedViewTransform, &o.cachedViewTransform, sizeof cachedViewTransform);
    std::memcpy(&globalViewTransform, &o.globalViewTransform, sizeof globalViewTransform);
    std::memcpy(&cachedOrientation, &o.cachedOrientation, sizeof cachedOrientation);
    dirtyViewTransform = o.dirtyViewTransform;
    dirtyGlobalTransform = o.dirtyGlobalTransform;
    dirtyViewCone = o.dirtyViewCone;
    scene = o.scene;
}

Camera& Camera::operator=(const Camera& o)
{
    if (this == &o) return *this;
    Particle::operator=(o);
    char* dst = reinterpret_cast<char*>(this);
    const char* src = reinterpret_cast<const char*>(&o);
    for (unsigned off = 0x12c; off < 0x1b0; off += 4) X87Copy(*reinterpret_cast<float*>(dst + off),
                                                              *reinterpret_cast<const float*>(src + off));
    reflectionCull = o.reflectionCull;
    projectionMode = o.projectionMode;
    for (unsigned off = 0x1b8; off < 0x25c; off += 4) X87Copy(*reinterpret_cast<float*>(dst + off),
                                                              *reinterpret_cast<const float*>(src + off));
    dirtyViewTransform = o.dirtyViewTransform;
    dirtyGlobalTransform = o.dirtyGlobalTransform;
    dirtyViewCone = o.dirtyViewCone;
    scene = o.scene;
    return *this;
}

Camera::~Camera() = default;

void __cdecl Camera::RenderDebug(Display* display, DebugViewPort& port, Matrix4& m, unsigned long flags) const
{
    using Fn = void(__cdecl*)(const Camera*, Display*, DebugViewPort*, Matrix4*, unsigned long);
    static Fn fn = OrigProc<Fn>("?RenderDebug@Camera@Jet@@UBAXPAVDisplay@2@AAVDebugViewPort@2@AAVMatrix4@2@K@Z");
    if (fn) fn(this, display, &port, &m, flags);
}

}  // namespace Jet
