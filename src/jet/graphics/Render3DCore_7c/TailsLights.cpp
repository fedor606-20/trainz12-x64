// Render3DCore_7c tails: the ParallelLight default-constructor closure U7 left forwarded. The original is
// `push 3; call ParallelLight::ParallelLight(DLightTypes); ret` with `this` arriving in ECX; the fastcall helper
// receives that pointer in ECX too and constructs the light in place, the same pattern the other modules use for
// their `??_F` closures.
#include <jet/Render3DCoreLights.h>

#include <new>

namespace Jet {

void __fastcall ParallelLightDefaultCtorClosure(ParallelLight* self)
{
    ::new (reinterpret_cast<void*>(self)) ParallelLight(static_cast<DLightTypes>(3));
}

}  // namespace Jet
