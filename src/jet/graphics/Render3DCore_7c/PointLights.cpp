// Unit U7 of Render3DCore_7c: Jet::OmniLight, Jet::ParallelLight and Jet::SpotLight, transcribed from the TS12
// assembly. See Render3DCoreLights.h for the layouts.
//
// The position/direction slots are DisplayLight's Slot2..Slot5 here: ParallelLight overrides slot 4
// (SetLightDirection) and slot 5 (GetLightDirection); OmniLight's direction is the static zero vector. The
// exported virtual names alias the plain methods (implemented.d `name=symbol`).
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

float fdiv(float a, float b)
{
    __asm {
        movss xmm0, a
        divss xmm0, b
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

// The bounding volume is U3's class and these notifications are local bodies, so call it directly.

void BvRecalculateGlobalOrientation(void* bv)
{
    BoundsAccess::RecalculateGlobalOrientation(static_cast<ParticleBoundingVolume*>(bv));
}

void BvSetOrientationChanged(void* bv, const Quaternion& oldOrientation, const Quaternion& newOrientation)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetOrientationChanged(oldOrientation, newOrientation);
}

struct DisplayLightMirror {
    void* vftable;
    int type;
    float field[18];
};
JET_LAYOUT32(sizeof(DisplayLightMirror) == 0x50);

void CopyDisplayLight(DisplayLight& dst, const DisplayLight& src)
{
    DisplayLightMirror& d = *reinterpret_cast<DisplayLightMirror*>(&dst);
    const DisplayLightMirror& s = *reinterpret_cast<const DisplayLightMirror*>(&src);
    d.type = s.type;
    for (int i = 0; i < 18; ++i) d.field[i] = X87Copy(s.field[i]);
}

const Quaternion& BvGlobalOrientation(void* bv)
{
    return *reinterpret_cast<const Quaternion*>(static_cast<char*>(bv) + 0x88);
}

// the direction stored at +0x18c for a global orientation quaternion (the original computes it in SSE order)
void DirectionFromOrientation(Vector3& direction, const Quaternion& q)
{
    const float z2 = fmul(q.z, 2.0f);
    const float y2 = fmul(q.y, 2.0f);
    const float x2 = fmul(q.x, 2.0f);

    const float xz = fmul(q.x, z2);
    const float wy = fmul(q.w, y2);
    const float sum1 = fadd(xz, wy);

    const float yy = fmul(y2, q.y);
    const float xx = fmul(x2, q.x);
    const float sum2 = fadd(xx, yy);
    const float oneMinus = fsub(1.0f, sum2);

    const float yz = fmul(z2, q.y);
    const float wx = fmul(x2, q.w);
    const float diff = fsub(yz, wx);

    direction.x = fsub(-0.0f, sum1);
    direction.y = fsub(-0.0f, diff);
    direction.z = fsub(-0.0f, oneMinus);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// OmniLight (0x1003e5b0, 0x10042190..0x10042d00)

OmniLight::OmniLight() : Light(static_cast<DLightTypes>(1))
{
    const Color white{1.0f, 1.0f, 1.0f, 1.0f};
    SetDiffuse(white);
    SetConstantAttenuation(0.0f);
    SetLinearAttenuation(0.5f);
    SetQuadraticAttenuation(0.0f);
}

OmniLight::OmniLight(const Vector3& position, float radius, const Color& color)
    : Light(static_cast<DLightTypes>(1), position, radius)
{
    SetDiffuse(color);
    SetConstantAttenuation(0.0f);
    SetLinearAttenuation(0.0f);
    SetQuadraticAttenuation(fdiv(1.0f, fmul(fmul(radius, radius), 0.1f)));
    SetLightPosition(position);
}

OmniLight::OmniLight(const OmniLight& o)
    : Light(o), unknown180(o.unknown180), unknown184(o.unknown184), unknown188(o.unknown188)
{
    CopyX87(unknown180, o.unknown180);
    CopyX87(unknown184, o.unknown184);
    CopyX87(unknown188, o.unknown188);
}

OmniLight::~OmniLight() {}

OmniLight& OmniLight::operator=(const OmniLight& o)
{
    AssignLightState(o);
    CopyX87(unknown180, o.unknown180);
    CopyX87(unknown184, o.unknown184);
    CopyX87(unknown188, o.unknown188);
    return *this;
}

// 0x422b0
Particle* __cdecl OmniLight::CreateInstance() const
{
    OmniLight* const instance = static_cast<OmniLight*>(Alloc::New(sizeof(OmniLight)));
    if (!instance) return nullptr;
    ::new (instance) OmniLight();
    InitInstance(*instance);
    return static_cast<Particle*>(instance);
}

// 0x3e5b0
int __cdecl OmniLight::GetID() const
{
    return 8;
}

// 0x42350
bool __cdecl OmniLight::IsOfType(int id) const
{
    return id == 8 || id == 5 || id == 1;
}

// 0x42370
void __cdecl OmniLight::InitInstance(OmniLight& o) const
{
    Particle::InitInstance(o);
    CopyDisplayLight(o, *this);
    o.lightingBegin = 0;
    CopyX87(o.unknown180, unknown180);
    CopyX87(o.unknown184, unknown184);
    CopyX87(o.unknown188, unknown188);
}

// 0x423d0: the original's field list (the type word, spot exponent and cutoff are not compared)
bool __cdecl OmniLight::operator==(const OmniLight& o) const
{
    if (lightingBegin != o.lightingBegin) return false;

    const Vector3& p1 = GetLightPosition();
    const Vector3& p2 = o.GetLightPosition();
    if (!(p1.x == p2.x && p1.y == p2.y && p1.z == p2.z)) return false;

    const Color& a1 = GetAmbient();
    const Color& a2 = o.GetAmbient();
    if (!(a1.r == a2.r && a1.g == a2.g && a1.b == a2.b && a1.a == a2.a)) return false;
    const Color& d1 = GetDiffuse();
    const Color& d2 = o.GetDiffuse();
    if (!(d1.r == d2.r && d1.g == d2.g && d1.b == d2.b && d1.a == d2.a)) return false;
    const Color& s1 = GetSpecular();
    const Color& s2 = o.GetSpecular();
    if (!(s1.r == s2.r && s1.g == s2.g && s1.b == s2.b && s1.a == s2.a)) return false;

    if (!(GetConstantAttenuation() == o.GetConstantAttenuation() &&
          GetLinearAttenuation() == o.GetLinearAttenuation() &&
          GetQuadraticAttenuation() == o.GetQuadraticAttenuation()))
        return false;
    if (!(unknown180 == o.unknown180 && unknown184 == o.unknown184 && unknown188 == o.unknown188)) return false;
    return GetRange() == o.GetRange();
}

// 0x426c0: forwarded, the original has the display, material and vertex-item contract
void __cdecl OmniLight::BatchCalculateDiffuse(Display* display, int count, LightVertexPtr positions,
                                              LightVertexPtr normals, LightColorPtr colors, const Matrix4& world,
                                              const Matrix4& view, Material* material) const
{
    using Fn = void(__cdecl*)(const OmniLight*, Display*, int, LightVertexPtr, LightVertexPtr, LightColorPtr,
                              const Matrix4&, const Matrix4&, Material*);
    static Fn fn = OrigProc<Fn>(
        "?BatchCalculateDiffuse@OmniLight@Jet@@UBAXPAVDisplay@2@HV?$VertexItemPtr@VVector3@Jet@@V?$VertexItemPtr@$$"
        "CBVVector3@Jet@@VVIPConstHelper@2@@2@@2@1V?$VertexItemPtr@KV?$VertexItemPtr@$$CBKVVIPConstHelper@Jet@@@Jet@@@2@"
        "ABVMatrix4@2@3PAVMaterial@2@@Z");
    if (fn) fn(this, display, count, positions, normals, colors, world, view, material);
}

// slot 4: the shared empty SetLightDirection
int __cdecl OmniLight::Slot4() const
{
    return 0;
}

// slot 5
int __cdecl OmniLight::Slot5() const
{
    return reinterpret_cast<int>(&GetLightDirection());
}

void __cdecl OmniLight::SetLightDirection(const Vector3&)
{
}

// 0x42cb0
const Vector3& __cdecl OmniLight::GetLightDirection() const
{
    static const Vector3 zero{0.0f, 0.0f, 0.0f};
    return zero;
}

// ---------------------------------------------------------------------------------------------------------------
// ParallelLight (0x1004d140..0x1004df00)

ParallelLight::ParallelLight(DLightTypes type) : Light(type)
{
    SetRange(100000.0f);
    const Color white{1.0f, 1.0f, 1.0f, 1.0f};
    SetDiffuse(white);

    void* const bv = GetBoundingVolume();
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);
    const Quaternion& q = BvGlobalOrientation(bv);
    CopyX87(globalOrientation.x, q.x);
    CopyX87(globalOrientation.y, q.y);
    CopyX87(globalOrientation.z, q.z);
    CopyX87(globalOrientation.w, q.w);
    DirectionFromOrientation(lightDirection, globalOrientation);
}

ParallelLight::ParallelLight(const Vector3& position, const Quaternion& orientation, float radius, const Color& color,
                             DLightTypes type)
    : Light(type, position, radius)
{
    void* const bv = GetBoundingVolume();
    BvSetOrientationChanged(bv, GetOrientation(), orientation);
    Quaternion& q = const_cast<Quaternion&>(GetOrientation());
    CopyX87(q.x, orientation.x);
    CopyX87(q.y, orientation.y);
    CopyX87(q.z, orientation.z);
    CopyX87(q.w, orientation.w);

    SetDiffuse(color);

    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);
    const Quaternion& g = BvGlobalOrientation(bv);
    CopyX87(globalOrientation.x, g.x);
    CopyX87(globalOrientation.y, g.y);
    CopyX87(globalOrientation.z, g.z);
    CopyX87(globalOrientation.w, g.w);
    DirectionFromOrientation(lightDirection, globalOrientation);
}

ParallelLight::ParallelLight(const ParallelLight& o)
    : Light(o), unknown180(o.unknown180), unknown184(o.unknown184), unknown188(o.unknown188),
      lightDirection(o.lightDirection), globalOrientation(o.globalOrientation)
{
}

ParallelLight::~ParallelLight() {}

ParallelLight& ParallelLight::operator=(const ParallelLight& o)
{
    AssignLightState(o);
    CopyX87(unknown180, o.unknown180);
    CopyX87(unknown184, o.unknown184);
    CopyX87(unknown188, o.unknown188);
    CopyX87(lightDirection.x, o.lightDirection.x);
    CopyX87(lightDirection.y, o.lightDirection.y);
    CopyX87(lightDirection.z, o.lightDirection.z);
    CopyX87(globalOrientation.x, o.globalOrientation.x);
    CopyX87(globalOrientation.y, o.globalOrientation.y);
    CopyX87(globalOrientation.z, o.globalOrientation.z);
    CopyX87(globalOrientation.w, o.globalOrientation.w);
    return *this;
}

// 0x4d310
Particle* __cdecl ParallelLight::CreateInstance() const
{
    ParallelLight* const instance = static_cast<ParallelLight*>(Alloc::New(sizeof(ParallelLight)));
    if (!instance) return nullptr;
    ::new (instance) ParallelLight(DLIGHT_DEFAULT);
    InitInstance(*instance);
    return static_cast<Particle*>(instance);
}

// 0x4d3b0
int __cdecl ParallelLight::GetID() const
{
    return 7;
}

// 0x4d3c0
bool __cdecl ParallelLight::IsOfType(int id) const
{
    return id == 7 || id == 5 || id == 1;
}

// 0x4d3e0
void __cdecl ParallelLight::InitInstance(ParallelLight& o) const
{
    Particle::InitInstance(o);
    CopyDisplayLight(o, *this);
    o.lightingBegin = 0;
    CopyX87(o.globalOrientation.x, globalOrientation.x);
    CopyX87(o.globalOrientation.y, globalOrientation.y);
    CopyX87(o.globalOrientation.z, globalOrientation.z);
    CopyX87(o.globalOrientation.w, globalOrientation.w);
    CopyX87(o.unknown180, unknown180);
    CopyX87(o.unknown184, unknown184);
    CopyX87(o.unknown188, unknown188);
    CopyX87(o.lightDirection.x, lightDirection.x);
    CopyX87(o.lightDirection.y, lightDirection.y);
    CopyX87(o.lightDirection.z, lightDirection.z);
}

// 0x4d490
void __cdecl ParallelLight::SetLightDirection(const Vector3& direction)
{
    const Vector3 target{fsub(-0.0f, direction.x), fsub(-0.0f, direction.y), fsub(-0.0f, direction.z)};
    const Vector3 up{1.0f, 0.0f, 0.0f};
    Quaternion q;
    q.SetZAxis(target, up);
    if (Particle* const parent = GetParent()) parent->GlobalToLocal(q);
    SetOrientation(q);
}

// 0x4d530
const Vector3& __cdecl ParallelLight::GetLightDirection() const
{
    void* bv = const_cast<ParallelLight*>(this)->GetBoundingVolume();
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);

    const Quaternion& q = BvGlobalOrientation(bv);
    if (!(globalOrientation.w == q.w && globalOrientation.x == q.x && globalOrientation.y == q.y &&
          globalOrientation.z == q.z)) {
        if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);
        const Quaternion& g = BvGlobalOrientation(bv);
        CopyX87(globalOrientation.x, g.x);
        CopyX87(globalOrientation.y, g.y);
        CopyX87(globalOrientation.z, g.z);
        CopyX87(globalOrientation.w, g.w);
        DirectionFromOrientation(lightDirection, globalOrientation);
    }
    return lightDirection;
}

extern "C" void __cdecl ParallelSetLightDirectionC(ParallelLight* self, const Vector3* direction)
{
    self->SetLightDirection(*direction);
}

__declspec(naked) int __cdecl ParallelLight::Slot4() const
{
    __asm {
        mov eax, dword ptr [esp + 4]
        mov edx, dword ptr [esp + 8]
        push edx
        push eax
        call ParallelSetLightDirectionC
        add esp, 8
        xor eax, eax
        ret
    }
}

int __cdecl ParallelLight::Slot5() const
{
    return reinterpret_cast<int>(&GetLightDirection());
}

// 0x4d9e0: forwarded
void __cdecl ParallelLight::BatchCalculateDiffuse(Display* display, int count, LightVertexPtr positions,
                                                  LightVertexPtr normals, LightColorPtr colors, const Matrix4& world,
                                                  const Matrix4& view, Material* material) const
{
    using Fn = void(__cdecl*)(const ParallelLight*, Display*, int, LightVertexPtr, LightVertexPtr, LightColorPtr,
                              const Matrix4&, const Matrix4&, Material*);
    static Fn fn = OrigProc<Fn>(
        "?BatchCalculateDiffuse@ParallelLight@Jet@@UBAXPAVDisplay@2@HV?$VertexItemPtr@VVector3@Jet@@V?$VertexItemPtr@$$"
        "CBVVector3@Jet@@VVIPConstHelper@2@@2@@2@1V?$VertexItemPtr@KV?$VertexItemPtr@$$CBKVVIPConstHelper@Jet@@@Jet@@@2@"
        "ABVMatrix4@2@3PAVMaterial@2@@Z");
    if (fn) fn(this, display, count, positions, normals, colors, world, view, material);
}

// ---------------------------------------------------------------------------------------------------------------
// SpotLight (0x10070440..0x10071110)

SpotLight::SpotLight() : ParallelLight(static_cast<DLightTypes>(2))
{
    const Color white{1.0f, 1.0f, 1.0f, 1.0f};
    SetDiffuse(white);
    SetSpotCutoff(10.0f);
    SetSpotExponent(1.0f);
    SetConstantAttenuation(0.0f);
    SetLinearAttenuation(0.5f);
    SetQuadraticAttenuation(0.0f);
}

SpotLight::SpotLight(const Vector3& position, const Quaternion& orientation, float radius, float cutoff, float exponent,
                     const Color& color)
    : ParallelLight(position, orientation, radius, color, static_cast<DLightTypes>(2))
{
    float clamped = cutoff;
    if (clamped > 90.0f) clamped = 90.0f;
    SetSpotCutoff(clamped);
    SetSpotExponent(exponent);
    SetConstantAttenuation(0.0f);
    SetLinearAttenuation(fdiv(1.0f, fmul(radius, 0.1f)));
    SetQuadraticAttenuation(0.0f);
    SetLightPosition(position);
}

SpotLight::SpotLight(const SpotLight& o)
    : ParallelLight(o), unknown1a8(o.unknown1a8), unknown1ac(o.unknown1ac), unknown1b0(o.unknown1b0)
{
    CopyX87(unknown1a8, o.unknown1a8);
    CopyX87(unknown1ac, o.unknown1ac);
    CopyX87(unknown1b0, o.unknown1b0);
}

SpotLight::~SpotLight() {}

SpotLight& SpotLight::operator=(const SpotLight& o)
{
    ParallelLight::operator=(o);
    CopyX87(unknown1a8, o.unknown1a8);
    CopyX87(unknown1ac, o.unknown1ac);
    CopyX87(unknown1b0, o.unknown1b0);
    return *this;
}

// 0x70540
Particle* __cdecl SpotLight::CreateInstance() const
{
    SpotLight* const instance = static_cast<SpotLight*>(Alloc::New(sizeof(SpotLight)));
    if (!instance) return nullptr;
    ::new (instance) SpotLight();
    InitInstance(*instance);
    return static_cast<Particle*>(instance);
}

// 0x70600
int __cdecl SpotLight::GetID() const
{
    return 9;
}

// 0x70610
bool __cdecl SpotLight::IsOfType(int id) const
{
    return id == 9 || id == 7 || id == 5 || id == 1;
}

// 0x70630
void __cdecl SpotLight::InitInstance(SpotLight& o) const
{
    ParallelLight::InitInstance(o);
    CopyX87(o.unknown1a8, unknown1a8);
    CopyX87(o.unknown1ac, unknown1ac);
    CopyX87(o.unknown1b0, unknown1b0);
}

// 0x70800: forwarded
void __cdecl SpotLight::BatchCalculateDiffuse(Display* display, int count, LightVertexPtr positions,
                                              LightVertexPtr normals, LightColorPtr colors, const Matrix4& world,
                                              const Matrix4& view, Material* material) const
{
    using Fn = void(__cdecl*)(const SpotLight*, Display*, int, LightVertexPtr, LightVertexPtr, LightColorPtr,
                              const Matrix4&, const Matrix4&, Material*);
    static Fn fn = OrigProc<Fn>(
        "?BatchCalculateDiffuse@SpotLight@Jet@@UBAXPAVDisplay@2@HV?$VertexItemPtr@VVector3@Jet@@V?$VertexItemPtr@$$"
        "CBVVector3@Jet@@VVIPConstHelper@2@@2@@2@1V?$VertexItemPtr@KV?$VertexItemPtr@$$CBKVVIPConstHelper@Jet@@@Jet@@@2@"
        "ABVMatrix4@2@3PAVMaterial@2@@Z");
    if (fn) fn(this, display, count, positions, normals, colors, world, view, material);
}

// `default constructor closure': the compiler-generated `ParallelLight x;` entry. It is a thiscall with no
// stack arguments that pushes DLightTypes(3) and calls the constructor; a fastcall with an ignored second
// register matches that ABI, and the linker alias gives it the original decorated name.
extern "C" void __fastcall JetParallelLightDefaultCtor(void* self, void*)
{
    ::new (self) ParallelLight(DLIGHT_DEFAULT);
}
#pragma comment(linker, "/alternatename:??_FParallelLight@Jet@@QAEXXZ=_JetParallelLightDefaultCtor@8")

}  // namespace Jet
