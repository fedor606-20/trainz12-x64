// Unit U7 of Render3DCore_7c: Jet::Light and Jet::AmbientLight, transcribed from the TS12 assembly.
//
// Light is DisplayLight (0x50) followed by Particle (0x12c) and the lighting-begin byte at 0x17c: 0x180 bytes.
// Its primary vtable is DisplayLight's eight slots plus SetLightingBegin and operator==(const Light&); the
// secondary one is Particle's. All virtual members are __cdecl, constructors are __thiscall and Light's
// operator= is __cdecl. The display-light position/direction slots are named Slot2..Slot5 in RenderSystem's
// header, so they are overridden under those names and the exported Set/GetLightPosition names alias the plain
// methods (Slot2 itself is a naked bridge because its declared signature has no argument).
//
// Cross-unit work stays in the original: the bounding volume's change notifications and global recalculation
// come from Render3DCore_7c_orig.dll, the display-light fields from RenderSystem_7c, and BatchCalculateDiffuse
// is forwarded. DisplayLight field copies are repeated here with x87, because the original's
// InitInstance/operator= quiet a signalling NaN and the local RenderSystem copy does not.
#include <jet/Render3DCoreBounds.h>
#include <jet/Render3DCoreLights.h>

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <new>

namespace Jet {

namespace {

// addss/subss/mulss/divss with the original's destination (first operand) order.
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

// a float through fld/fstp: a signalling NaN gets its quiet bit, no other bit changes
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

// ---- the particle bounding volume (U3) ----------------------------------------------------------------------

// The bounding volume is U3's class and these notifications are local bodies, so call it directly.

void BvRecalculateGlobalPosition(void* bv)
{
    BoundsAccess::RecalculateGlobalPosition(static_cast<ParticleBoundingVolume*>(bv));
}

void BvRecalculateGlobalOrientation(void* bv)
{
    BoundsAccess::RecalculateGlobalOrientation(static_cast<ParticleBoundingVolume*>(bv));
}

void BvSetPositionChanged(void* bv, const Vector3& oldPosition, const Vector3& newPosition)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetPositionChanged(oldPosition, newPosition);
}

void BvSetOrientationChanged(void* bv, const Quaternion& oldOrientation, const Quaternion& newOrientation)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetOrientationChanged(oldOrientation, newOrientation);
}

void BvSetPositionOrientationChanged(void* bv, const Vector3& oldPosition, const Vector3& newPosition,
                                     const Quaternion& oldOrientation, const Quaternion& newOrientation)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetPositionOrientationChanged(oldPosition, newPosition, oldOrientation,
                                                                             newOrientation);
}

// ---- DisplayLight's 0x50 bytes -------------------------------------------------------------------------------
//
// The original's copy constructor, InitInstance and operator= copy the type word and then every float through
// fld/fstp. Doing the same keeps a signalling NaN identical to the original's.

struct DisplayLightMirror {
    void* vftable;  // +0x00
    int type;       // +0x04
    float field[18];  // +0x08: ambient 4, diffuse 4, specular 4, attenuation 3, spotExponent, spotCutoff, range
};
JET_LAYOUT32(sizeof(DisplayLightMirror) == 0x50);

void CopyDisplayLight(DisplayLight& dst, const DisplayLight& src)
{
    DisplayLightMirror& d = *reinterpret_cast<DisplayLightMirror*>(&dst);
    const DisplayLightMirror& s = *reinterpret_cast<const DisplayLightMirror*>(&src);
    d.type = s.type;
    for (int i = 0; i < 18; ++i) d.field[i] = X87Copy(s.field[i]);
}

// virtual slot calls: the original's light accessors are __cdecl, this on the stack
using Slot2Fn = void(__cdecl*)(Light*, const Vector3*);
using Slot3Fn = const Vector3* (__cdecl*)(const Light*);
using Slot4Fn = void(__cdecl*)(Light*, const Vector3*);
using Slot5Fn = const Vector3* (__cdecl*)(const Light*);

void** PrimaryVtable(Light* o)
{
    return *reinterpret_cast<void***>(o);
}

void* const* PrimaryVtable(const Light* o)
{
    return *reinterpret_cast<void* const* const*>(o);
}

void SetPositionSlot(Light* o, const Vector3& v)
{
    reinterpret_cast<Slot2Fn>(PrimaryVtable(o)[2])(o, &v);
}

const Vector3& GetPositionSlot(const Light* o)
{
    return *reinterpret_cast<Slot3Fn>(PrimaryVtable(o)[3])(o);
}

void SetDirectionSlot(Light* o, const Vector3& v)
{
    reinterpret_cast<Slot4Fn>(PrimaryVtable(o)[4])(o, &v);
}

const Vector3& GetDirectionSlot(const Light* o)
{
    return *reinterpret_cast<Slot5Fn>(PrimaryVtable(o)[5])(o);
}

void* BoundingVolumeOf(Light* o)
{
    return o->GetBoundingVolume();
}

void* BoundingVolumeOf(const Light* o)
{
    return const_cast<Light*>(o)->GetBoundingVolume();
}

const Vector3& BvGlobalPosition(void* bv)
{
    return *reinterpret_cast<const Vector3*>(static_cast<char*>(bv) + 0x7c);
}

const Quaternion& BvGlobalOrientation(void* bv)
{
    return *reinterpret_cast<const Quaternion*>(static_cast<char*>(bv) + 0x88);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Light (0x10007b60..0x10008a40)

Light::Light(DLightTypes type) : DisplayLight(type), Particle(), lightingBegin(1)
{
}

Light::Light(DLightTypes type, const Vector3& position, float radius) : DisplayLight(type), Particle(), lightingBegin(1)
{
    void* const bv = GetBoundingVolume();
    BvSetPositionChanged(bv, GetPosition(), position);

    Vector3& p = const_cast<Vector3&>(GetPosition());
    CopyX87(p.x, position.x);
    CopyX87(p.y, position.y);
    CopyX87(p.z, position.z);
    SetRange(radius);
}

Light::Light(const Light& o) : DisplayLight(o), Particle(o), lightingBegin(o.lightingBegin)
{
    // the base copy already copied the fields; redo the floats through x87 for a signalling NaN
    CopyDisplayLight(*this, o);
}

Light::~Light() {}

// 0x7e90
bool __cdecl Light::operator==(const Light& o) const
{
    if (!DisplayLight::operator==(o)) return false;
    if (lightingBegin != o.lightingBegin) return false;

    const Vector3& d1 = GetDirectionSlot(this);
    const Vector3& d2 = GetDirectionSlot(&o);
    if (!(d1.x == d2.x && d1.y == d2.y && d1.z == d2.z)) return false;

    const Vector3& p1 = GetPositionSlot(this);
    const Vector3& p2 = GetPositionSlot(&o);
    return p1.x == p2.x && p1.y == p2.y && p1.z == p2.z;
}

// 0x7f60
Light& __cdecl Light::operator=(const Light& o)
{
    AssignLightState(o);
    return *this;
}

void Light::AssignLightState(const Light& o)
{
    SetPositionSlot(this, GetPositionSlot(&o));
    SetDirectionSlot(this, GetDirectionSlot(&o));
    CopyDisplayLight(*this, o);
}

// 0x7d90 (the AmbientLight export shares this code)
void Light::InitInstance(Light& o) const
{
    Particle::InitInstance(o);
    CopyDisplayLight(o, *this);
    o.lightingBegin = 0;
}

// 0x7dd0
int __cdecl Light::GetID() const
{
    return 5;
}

// 0x7de0
bool __cdecl Light::IsOfType(int id) const
{
    return id == 5 || id == 1;
}

// 0x7e00
void __cdecl Light::SetLightingBegin()
{
    lightingBegin = 1;
}

// 0x7e10
const Vector3& __cdecl Light::GetLightPosition() const
{
    void* const bv = BoundingVolumeOf(this);
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x10) BvRecalculateGlobalPosition(bv);
    return BvGlobalPosition(bv);
}

// 0x7e30
void __cdecl Light::SetLightPosition(const Vector3& position)
{
    Vector3 local = position;
    if (Particle* const parent = GetParent()) parent->GlobalToLocal(local);
    SetPosition(local);
}

// slot 2: the original's SetLightPosition(const Vector3&), reached through DisplayLight's vtable
extern "C" void __cdecl LightSetLightPositionC(Light* self, const Vector3* position)
{
    self->SetLightPosition(*position);
}

__declspec(naked) int __cdecl Light::Slot2() const
{
    __asm {
        mov eax, dword ptr [esp + 4]
        mov edx, dword ptr [esp + 8]
        push edx
        push eax
        call LightSetLightPositionC
        add esp, 8
        xor eax, eax
        ret
    }
}

// slot 3
int __cdecl Light::Slot3() const
{
    return reinterpret_cast<int>(&GetLightPosition());
}

// slots 4/5: the empty stubs the original Light vtable shares
int __cdecl Light::Slot4() const
{
    return 0;
}

int __cdecl Light::Slot5() const
{
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// AmbientLight (0x10007fa0..0x100083b0 and 0x10070440)

AmbientLight::AmbientLight() : Light(static_cast<DLightTypes>(0))
{
    const Color white{1.0f, 1.0f, 1.0f, 1.0f};
    SetAmbient(white);
}

AmbientLight::AmbientLight(const Vector3& position, float radius, const Color& color)
    : Light(static_cast<DLightTypes>(0), position, radius)
{
    SetAmbient(color);
}

AmbientLight::AmbientLight(const AmbientLight& o) : Light(o) {}

AmbientLight::~AmbientLight() {}

AmbientLight& AmbientLight::operator=(const AmbientLight& o)
{
    AssignLightState(o);
    return *this;
}

// 0x8180
Particle* __cdecl AmbientLight::CreateInstance() const
{
    AmbientLight* const instance = static_cast<AmbientLight*>(Alloc::New(sizeof(AmbientLight)));
    if (!instance) return nullptr;
    ::new (instance) AmbientLight();
    InitInstance(*instance);
    return static_cast<Particle*>(instance);
}

// 0x8220
int __cdecl AmbientLight::GetID() const
{
    return 6;
}

// 0x8230
bool __cdecl AmbientLight::IsOfType(int id) const
{
    return id == 6 || id == 5 || id == 1;
}

// 0x8250: needs Display::ConvertColor and the vertex items; call the original method on our object
void __cdecl AmbientLight::BatchCalculateDiffuse(Display* display, int count, LightVertexPtr positions,
                                                 LightVertexPtr normals, LightColorPtr colors, const Matrix4& world,
                                                 const Matrix4& view, Material* material) const
{
    using Fn = void(__cdecl*)(const AmbientLight*, Display*, int, LightVertexPtr, LightVertexPtr, LightColorPtr,
                              const Matrix4&, const Matrix4&, Material*);
    static Fn fn = OrigProc<Fn>(
        "?BatchCalculateDiffuse@AmbientLight@Jet@@UBAXPAVDisplay@2@HV?$VertexItemPtr@VVector3@Jet@@V?$VertexItemPtr@$$"
        "CBVVector3@Jet@@VVIPConstHelper@2@@2@@2@1V?$VertexItemPtr@KV?$VertexItemPtr@$$CBKVVIPConstHelper@Jet@@@Jet@@@2@"
        "ABVMatrix4@2@3PAVMaterial@2@@Z");
    if (fn) fn(this, display, count, positions, normals, colors, world, view, material);
}

// slot 4: the shared empty SetLightDirection
int __cdecl AmbientLight::Slot4() const
{
    return 0;
}

// slot 5: the original's static zero direction
int __cdecl AmbientLight::Slot5() const
{
    return reinterpret_cast<int>(&GetLightDirection());
}

void __cdecl AmbientLight::SetLightDirection(const Vector3&)
{
}

// 0x83b0
const Vector3& __cdecl AmbientLight::GetLightDirection() const
{
    static const Vector3 zero{0.0f, 0.0f, 0.0f};
    return zero;
}

// 0x7d90
void __cdecl AmbientLight::InitInstance(AmbientLight& o) const
{
    Particle::InitInstance(o);
    CopyDisplayLight(o, *this);
    o.lightingBegin = 0;
}

}  // namespace Jet
