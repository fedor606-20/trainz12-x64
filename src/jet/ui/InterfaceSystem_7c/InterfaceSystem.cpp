// InterfaceSystem_7c scaffold (Wave 5, module-owned). Placeholders/stubs only.
// Proxy-first: keep exports forwarded via implemented.txt until ABI/layout/behavior is verified.
// Not yet wired into shared build files (src/jet/CMakeLists.txt, README, progress maps, def/export wiring).

#include "InterfaceSystem.h"

namespace Jet {
namespace UI {

// TODO(implementation, staged per notes/agents/InterfaceSystem_7c.md / CONSOLIDATED_PLAN.md Wave 5):
// U0 export/plumbing skeleton, U1 value/helpers, U2 Region/Base hierarchy/layout,
// U3 Canvas/overlay primitives, U4 State/input routing & DnD, U5 Desktop/InterfaceSystem runtime,
// U6 DialogResource/dialogs, U7 OSD/font overlay, U8-U10 widgets/containers, U11 parity/statics/factories.
// Each step requires recovered ABI/layout/vtable/slot contracts and differential validation.

}  // namespace UI
}  // namespace Jet

extern "C" void __cdecl JetGetPluginsInterfaceSystem() {
    // Evidenced module export (D:\trainz12-decomp\exports\InterfaceSystem_7c.tsv, RVA 0x27040).
    // Expected role (high-level, from maps/reports): register InterfaceSystem/UI plugin and
    // resource/factory types with Jet infrastructure. Exact registration set, IDs, order,
    // parameters/return semantics, and side effects are NOT recovered.
    //
    // Safe scaffold: no-op placeholder. Do NOT enable in implemented.txt and do NOT register
    // guessed factories/types/IDs until verified against the original behavior.
}
