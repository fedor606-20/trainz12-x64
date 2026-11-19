// InterfaceResources_7c scaffold (Wave 1, module-owned). Placeholders/stubs only.
// Most exports remain forwarded via implemented.txt until ABI/layout/behavior is verified.
// Not yet wired into shared build files (src/jet/CMakeLists.txt, README, progress maps).

#include "InterfaceResources.h"

namespace Jet {

// TODO(impl units, see notes/agents/InterfaceResources_7c.md): U1 small holders/value types
// (FontInformation, CursorInstance, IconInstance basics), U2 Resource/ResourceType/Plugin
// registration + class layout/vtable mapping, U3 FontFx/FontFxBitmap/FontFxTT, U4 Font core
// and font resources/instances, U5 Cursor/Border/BitmapBorderResource, U6 IconResource/instance,
// U7 differential/integration hardening.
//
// TODO(resource formats): recover .txt/binary parse formats for cursors/borders/icons/fonts,
// font tag syntax, bitmap/TT font effect config, and any on-the-fly creation behavior.
// TODO(x64): preserve original struct sizes/alignment/packing, vtable/vbtable layout for MI
// (FontFx*), calling conventions, and pointer/size_t width differences from 32-bit original.

}  // namespace Jet

extern "C" void __cdecl JetGetPluginsInterfaceResources() {
    // Evidence: exported plugin getter for this module (exports ordinal 169).
    // Expected role: register UI resource types/factories (borders/cursors/fonts/icons and
    // related FontResourceType/ResourceType<> specializations) with Jet's plugin/resource
    // infrastructure so InterfaceSystem and resource loading can resolve them.
    // Safe scaffold: no-op. Do NOT register guessed factory IDs, parent/group UIds, or type
    // names; recover exact registration set/order and side effects from original decomp first.
    //
    // Pattern when verified (illustrative only):
    //   new Jet::ResourceType<Jet::CursorResource>(Jet::PString("<ext>"), Jet::UId("<id>"), Jet::UId("<parent>"));
    //   ... (bitmap border / icon / font resource types) ...
}
