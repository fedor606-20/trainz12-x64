// Unit U11 of Render3DCore_7c: Jet::Facility, the module's one-method facility interface.
//
// The original constructor and copy constructor only write the class vftable (0x10003150/0x10003160), the
// assignment returns `this` (0x100021cb0, shared with DefaultInterpolator<Quaternion> and Function) and the single
// virtual slot at 0x1007d420 is the shared `xor eax,eax; ret` stub (0x100079e0). The class has no fields and no
// virtual destructor, so the local class is 4 bytes and its compiler-emitted table has the same one slot. The
// table is exported under the original name from this translation unit; see implemented.d/u11_rem.txt.
#include <jet/Render3DCoreBackground.h>

namespace Jet {

Facility::Facility() {}

Facility::Facility(const Facility&) {}

Facility& Facility::operator=(const Facility&)
{
    return *this;
}

const PString& __cdecl Facility::GetName() const
{
    return *static_cast<const PString*>(nullptr);
}

}  // namespace Jet
