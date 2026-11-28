// Unit U9b of Render3DCore_7c: Location, Corona, Function, and the ReferencePoint members that U9a's header leaves
// inline. ReferencePoint has no out-of-line members of its own; the exports are the inline functions' COMDAT copies,
// which the .def exports under their original names.
#include <jet/Render3DCoreScene.h>

namespace Jet {

float X87CopyScene(float f)
{
    __asm {
        fld dword ptr f
        fstp dword ptr f
    }
    return f;
}

// ---- Location ------------------------------------------------------------------------------------------------

Location::Location() : particle(nullptr) {}

Location::Location(const Vector3& position_, Particle* particle_)
{
    position.x = X87CopyScene(position_.x);
    position.y = X87CopyScene(position_.y);
    position.z = X87CopyScene(position_.z);
    particle = particle_;
}

Location::Location(const Location& o)
{
    position.x = X87CopyScene(o.position.x);
    position.y = X87CopyScene(o.position.y);
    position.z = X87CopyScene(o.position.z);
    particle = o.particle;
}

Location& Location::operator=(const Location& o)
{
    position.x = X87CopyScene(o.position.x);
    position.y = X87CopyScene(o.position.y);
    position.z = X87CopyScene(o.position.z);
    particle = o.particle;
    return *this;
}

void __cdecl Location::Set(const Vector3& position_, Particle* particle_)
{
    position.x = X87CopyScene(position_.x);
    position.y = X87CopyScene(position_.y);
    position.z = X87CopyScene(position_.z);
    particle = particle_;
}

Vector3 __cdecl Location::GetGlobalLocation() const
{
    Vector3 result;
    result.x = position.x;
    result.y = position.y;
    result.z = position.z;
    if (particle) particle->LocalToGlobal(result);
    return result;
}

// ---- Corona --------------------------------------------------------------------------------------------------

Corona::Corona()
    : a(0.5f), b(10000.0f), c(100.0f), d(0.5f), e(0.1f), f(0.0f)
{
}

Corona::~Corona() {}

Corona& Corona::operator=(const Corona& o)
{
    a = o.a;
    b = o.b;
    c = o.c;
    d = o.d;
    e = o.e;
    f = o.f;
    return *this;
}

void __cdecl Corona::Cull(ViewPoint&, const Matrix4&, Vector3&) {}
void __cdecl Corona::Render(Matrix4&) {}

// ---- Function ------------------------------------------------------------------------------------------------

Function::Function() {}
Function::Function(const Function&) {}

Function& Function::operator=(const Function&)
{
    return *this;
}

unsigned long __cdecl Function::GetType() const
{
    return 0;
}

// ---- ReferencePoint ------------------------------------------------------------------------------------------
//
// Keep the inline getters/setters as real out-of-line COMDAT definitions: the export table names them, and a
// pointer-to-member is an odr-use the optimiser cannot remove.
namespace {

using RP = ReferencePoint;
using GetNameFn = const PString& (__cdecl RP::*)() const;
using GetPositionFn = const Vector3& (__cdecl RP::*)() const;
using GetOrientationFn = const Quaternion& (__cdecl RP::*)() const;
using SetNameFn = void (__cdecl RP::*)(const PString&);
using SetPositionFn = void (__cdecl RP::*)(const Vector3&);
using SetOrientationFn = void (__cdecl RP::*)(const Quaternion&);

GetNameFn const refPointGetName = &RP::GetName;
GetPositionFn const refPointGetPosition = &RP::GetPosition;
GetOrientationFn const refPointGetOrientation = &RP::GetOrientation;
SetNameFn const refPointSetName = &RP::SetName;
SetPositionFn const refPointSetPosition = &RP::SetPosition;
SetOrientationFn const refPointSetOrientation = &RP::SetOrientation;

// copy constructor, assignment and destructor (used by ParticleStructure as well; kept out of line here)
__declspec(noinline) void TouchReferencePoint(RP& dst, const RP& src)
{
    RP copy(src);
    dst = src;
    dst.SetName(src.GetName());
    dst.SetPosition(src.GetPosition());
    dst.SetOrientation(src.GetOrientation());
    copy.~RP();
}

}  // namespace

}  // namespace Jet
