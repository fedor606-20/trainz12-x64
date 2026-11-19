// DebugSupport_7c scaffold (Wave 1, module-owned). Minimal/evidence-only placeholders.
// Default is proxy-first forwarding for all exports; only JetGetPluginsDebugSupport is
// stubbed here as a trivially-safe no-op and listed in implemented.txt.
//
// IMPORTANT: No guessed ABI/layout/behavior. Before enabling any other export:
// - Recover exact class layouts, MI/vftable bases and slot order/adjustor thunks for
//   DebugControl (DebugAccess/InputUIExtensionObserver/Modifier/ReferenceCount aspects),
//   DebugTextureView, consoles, and related DebugViewPort/OSD* interfaces.
// - Verify calling conventions/return ABI (e.g. DebugControl::Update ResultClass return,
//   __cdecl members per demangles) and exported static data/singleton ownership.
// - Identify plugin registration/runtime hook semantics (JetGetPluginsDebugSupport) and
//   actual debug/session load triggers/consumers.
// - Confirm dependencies on DebugInterface_7c, RenderSystem/render kernel/viewport/camera/
//   texture/layer/profile types, and Input UI extension observer contracts.
//
// Evidence sources:
// - D:\trainz12-decomp\exports\DebugSupport_7c.tsv (119 exports)
// - trainz12-x64/ghidra_maps/jet/debug/DebugSupport_7c.dll.functions.tsv
// - trainz12-x64/ghidra_maps/jet/debug/vtnames_DebugSupport_7c.dll.tsv
// - trainz12-x64/notes/agents/DebugSupport_7c.md

#include "DebugSupport.h"

extern "C" void __cdecl JetGetPluginsDebugSupport() {
    // Placeholder only. Original module exports JetGetPluginsDebugSupport (RVA 0x2950 in
    // evidence); registration/factory behavior is unknown and must be recovered from decomp.
    // No side effects are performed here to keep the stub trivially safe while forwarded-by-default.
}
