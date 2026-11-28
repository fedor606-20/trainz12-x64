// Render3DCore_7c wave 5 tails: the exported SkeletonModifier::Update takes the secondary Modifier subobject
// (object+0x18), while the local vtable override takes the SkeletonModifier. This cdecl member adaptor has the same
// stack layout as the original (this at [esp+4], the hidden ResultClass pointer at [esp+8], dt at [esp+0xc]) and is
// what implemented.d/tails2.txt exports under the original name; the vtable keeps pointing at the local member.
#include <jet/Render3DCoreBones.h>

namespace Jet {
namespace Tails2 {

struct SkeletonUpdateAdaptor {
    Modifier::Result __cdecl SkeletonUpdate(float dt);
};

Modifier::Result __cdecl SkeletonUpdateAdaptor::SkeletonUpdate(float dt)
{
    Modifier* modifier = reinterpret_cast<Modifier*>(this);
    SkeletonModifier* self = reinterpret_cast<SkeletonModifier*>(reinterpret_cast<char*>(modifier) - 0x18);
    return self->Update(dt);
}

}  // namespace Tails2
}  // namespace Jet
