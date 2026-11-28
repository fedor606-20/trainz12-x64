// Unit U7 of Render3DCore_7c: Jet::CubeMapLight, transcribed from the TS12 assembly.
//
// 0x1ac bytes: Light (0x180), the texture AutoReference (one pointer) at 0x180, a 3x3 light matrix at 0x184
// and the front-face byte at 0x1a8. The primary vtable has 14 slots; slots 10..13 are
// Rotate(Quaternion), Rotate(Matrix3), SelfRotate(Quaternion), SelfRotate(Matrix3).
//
// The texture storage is an AutoReference but kept as a raw pointer here, because the original's destructor
// releases it exactly once and an AutoReference member would release it a second time.
#include <jet/Render3DCoreBounds.h>
#include <jet/Render3DCoreLights.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

float fadd(float a, float b)
{
    __asm {
        movss xmm0, a
        addss xmm0, b
        movss a, xmm0
    }
    return a;
}

float fsub(float a, float b)
{
    __asm {
        movss xmm0, a
        subss xmm0, b
        movss a, xmm0
    }
    return a;
}

float fmul(float a, float b)
{
    __asm {
        movss xmm0, a
        mulss xmm0, b
        movss a, xmm0
    }
    return a;
}

float X87Copy(float f)
{
    unsigned int u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) && !(u & 0x00400000u))
        u |= 0x00400000u;
    std::memcpy(&f, &u, 4);
    return f;
}

void CopyX87(float& d, float s)
{
    d = X87Copy(s);
}

// The bounding volume is U3's class; its change notification is a local body, so call it directly.
using BvProcSetQuat = void(__cdecl*)(void*, const Quaternion&, const Quaternion&);

void BvSetOrientationChanged(void* bv, const Quaternion& oldOrientation, const Quaternion& newOrientation)
{
    static BvProcSetQuat const fn = [](void* volume, const Quaternion& oldValue, const Quaternion& newValue) {
        static_cast<ParticleBoundingVolume*>(volume)->SetOrientationChanged(oldValue, newValue);
    };
    fn(bv, oldOrientation, newOrientation);
}

}  // namespace

CubeMapLight::CubeMapLight(const Vector3& position, const Quaternion& orientation,
                           const AutoReference<Texture>& texture_, float radius, const Color& color)
    : Light(static_cast<DLightTypes>(4), position, radius), texture(texture_.Get()), lightMatrix(), frontFaceMode(true)
{
    if (texture) texture->AddReference();

    SetDiffuse(color);

    void* const bv = GetBoundingVolume();
    BvSetOrientationChanged(bv, GetOrientation(), orientation);
    Quaternion& q = const_cast<Quaternion&>(GetOrientation());
    CopyX87(q.x, orientation.x);
    CopyX87(q.y, orientation.y);
    CopyX87(q.z, orientation.z);
    CopyX87(q.w, orientation.w);

    UpdateLightMatrix();
}

CubeMapLight::CubeMapLight(const CubeMapLight& o)
    : Light(o), texture(o.texture), lightMatrix(o.lightMatrix), frontFaceMode(o.frontFaceMode)
{
    if (texture) texture->AddReference();
}

CubeMapLight::~CubeMapLight()
{
    if (texture) texture->ReleaseReference();
}

CubeMapLight& CubeMapLight::operator=(const CubeMapLight& o)
{
    AssignLightState(o);

    Texture* const newTexture = o.texture;
    Texture* const oldTexture = texture;
    texture = newTexture;
    if (newTexture) newTexture->AddReference();
    if (oldTexture) oldTexture->ReleaseReference();

    for (int i = 0; i < 9; ++i) CopyX87(lightMatrix.m[i], o.lightMatrix.m[i]);
    frontFaceMode = o.frontFaceMode;
    return *this;
}

// 0x21090
Particle* __cdecl CubeMapLight::CreateInstance() const
{
    CubeMapLight* const instance = static_cast<CubeMapLight*>(Alloc::New(sizeof(CubeMapLight)));
    if (!instance) return nullptr;
    ::new (instance)
        CubeMapLight(GetLightPosition(), GetOrientation(), GetTexture(), GetRange(), GetDiffuse());
    InitInstance(*instance);
    return static_cast<Particle*>(instance);
}

// 0x21060
int __cdecl CubeMapLight::GetID() const
{
    return 10;
}

// 0x21070
bool __cdecl CubeMapLight::IsOfType(int id) const
{
    return id == 10 || id == 5 || id == 1;
}

// the shared empty function: the cube map light adds no diffuse
void __cdecl CubeMapLight::BatchCalculateDiffuse(Display*, int, LightVertexPtr, LightVertexPtr, LightColorPtr,
                                                 const Matrix4&, const Matrix4&, Material*) const
{
}

// 0x21160
void __cdecl CubeMapLight::SetTexture(const AutoReference<Texture>& texture_)
{
    Texture* const newTexture = texture_.Get();
    Texture* const oldTexture = texture;
    texture = newTexture;
    if (newTexture) newTexture->AddReference();
    if (oldTexture) oldTexture->ReleaseReference();
}

// 0x21190
const AutoReference<Texture>& __cdecl CubeMapLight::GetTexture() const
{
    return *reinterpret_cast<const AutoReference<Texture>*>(&texture);
}

// 0x211a0
void __cdecl CubeMapLight::SetLightMatrix(const Matrix3& matrix)
{
    for (int i = 0; i < 9; ++i) CopyX87(lightMatrix.m[i], matrix.m[i]);
}

// 0x21200
const Matrix3& __cdecl CubeMapLight::GetLightMatrix() const
{
    return lightMatrix;
}

// 0x21210
void __cdecl CubeMapLight::SetFrontFaceMode(bool frontFace)
{
    frontFaceMode = frontFace;
}

// 0x21220
bool __cdecl CubeMapLight::GetFrontFaceMode()
{
    return frontFaceMode;
}

// 0x21310
void __cdecl CubeMapLight::SetOrientation(const Quaternion& orientation)
{
    Particle::SetOrientation(orientation);
    UpdateLightMatrix();
}

// 0x21420
void __cdecl CubeMapLight::SetReferenceFrame(const Vector3& position, const Quaternion& orientation)
{
    Particle::SetReferenceFrame(position, orientation);
    UpdateLightMatrix();
}

// 0x21350 / 0x21370
void __cdecl CubeMapLight::Rotate(const Matrix3& rotation)
{
    Particle::Rotate(rotation);
    UpdateLightMatrix();
}

void __cdecl CubeMapLight::Rotate(const Quaternion& rotation)
{
    Particle::Rotate(rotation);
    UpdateLightMatrix();
}

// 0x21390 / 0x213e0
void __cdecl CubeMapLight::SelfRotate(const Matrix3& rotation)
{
    Particle::SelfRotate(rotation);
    UpdateLightMatrix();
}

void __cdecl CubeMapLight::SelfRotate(const Quaternion& rotation)
{
    Particle::SelfRotate(rotation);
    UpdateLightMatrix();
}

// slot 4: the shared empty SetLightDirection
int __cdecl CubeMapLight::Slot4() const
{
    return 0;
}

// slot 5
int __cdecl CubeMapLight::Slot5() const
{
    return reinterpret_cast<int>(&GetLightDirection());
}

void __cdecl CubeMapLight::SetLightDirection(const Vector3&)
{
}

// 0x215d0
const Vector3& __cdecl CubeMapLight::GetLightDirection() const
{
    static const Vector3 zero{0.0f, 0.0f, 0.0f};
    return zero;
}

// 0x21480: the 3x3 rotation matrix of the particle orientation, in the original's SSE order
void __cdecl CubeMapLight::UpdateLightMatrix()
{
    const Quaternion& q = GetOrientation();
    const float x = q.x;
    const float y = q.y;
    const float z = q.z;
    const float w = q.w;

    const float z2 = fmul(z, 2.0f);
    const float y2 = fmul(y, 2.0f);
    const float x2 = fmul(x, 2.0f);

    const float zz = fmul(z, z2);  // 2zz
    const float xx = fmul(x, x2);  // 2xx
    const float xz = fmul(x, z2);  // 2xz
    const float xy = fmul(x, y2);  // 2xy
    const float yz = fmul(y, z2);  // 2yz
    const float wx = fmul(w, x2);  // 2wx
    const float wy = fmul(w, y2);  // 2wy
    const float wz = fmul(w, z2);  // 2wz
    const float yy = fmul(y, y2);  // 2yy

    lightMatrix.m[0] = fsub(1.0f, fadd(zz, yy));
    lightMatrix.m[1] = fadd(wz, xy);
    lightMatrix.m[2] = fsub(xz, wy);
    lightMatrix.m[3] = fsub(xy, wz);
    lightMatrix.m[4] = fsub(1.0f, fadd(zz, xx));
    lightMatrix.m[5] = fadd(wx, yz);
    lightMatrix.m[6] = fadd(wy, xz);
    lightMatrix.m[7] = fsub(yz, wx);
    lightMatrix.m[8] = fsub(1.0f, fadd(yy, xx));
}

}  // namespace Jet
