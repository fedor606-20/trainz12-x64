#pragma once

// DebugSupport_7c scaffold (Jet debug control/overlays/stats, debug group).
// Evidence-driven placeholders only. Do NOT assume ABI/layout, vtable slots/order,
// MI bases, calling conventions beyond export demangles, object sizes, ownership,
// registration, or runtime trigger behavior without decomp/recovery.
//
// Evidence: D:\trainz12-decomp\exports\DebugSupport_7c.tsv (119 exports, ordinals 1..119),
// trainz12-x64/ghidra_maps/jet/debug/DebugSupport_7c.dll.functions.tsv (254 rows incl.
// CRT/unwind/thunks/internal helpers), vtnames_DebugSupport_7c.dll.tsv,
// notes/agents/DebugSupport_7c.md.
//
// Evidenced export clusters (names/types from demangled exports):
// - Plugin: JetGetPluginsDebugSupport.
// - Jet::DebugControl (primary surface, most exports): protected ctor(RenderKernel*),
//   virtual dtor/Update(float)->Jet::ResultClass<&Modifier::results,Modifier::Results>,
//   singleton data `?singleton@DebugControl@Jet@@1PAV12@A`, Create/DestroyDebugControl,
//   Create* for render/portal stats/list consoles, profiler, debug viewport/texture view,
//   many getters/setters/toggles (wireframe, normals/axis, texture/VSync, portal/render
//   stats/list visibility, debug viewport state/camera/target/alignment, dynamic/static
//   geometry, portal stencil/debug-render-lines/profile state/type, reset camera,
//   screen dump, ProcessLayerState, ProfilerObservesMouse, UseDebugViewportCameraTarget).
//   Exported vftables for DebugControl bases/aspects: DebugAccess, InputUIExtensionObserver,
//   Modifier, ReferenceCount (exact MI layout/adjustor thunks unknown).
// - Jet::DebugTextureView: ctor/copy/assign/dtor, vftable, Add/RemoveTexture overloads
//   (Texture/DisplayTexture, optional Vector4 tint), Render, DrawQuad, DeleteTexture.
// - Jet::PortalListConsole, Jet::PortalStatsConsole, Jet::RenderStatsConsole: ctors for
//   RenderKernel+Point+Size(+float/+bool flags) and LayerGroup variants, copy/assign/dtor,
//   vftables, DisplayStats(float); RenderStatsConsole also Init/IsVisible/SetVisible/
//   FormatNumber helpers exported.
// - vtnames/function map indicate additional internal/interface involvement (DebugViewPort,
//   OSDProfile/OSDConsole, shared/thunked vf slots) and ~52 thunk entries; slot semantics
//   and object relationships must be recovered, not guessed.
//
// Runtime/load context (evidenced, not fully confirmed): DebugSupport not seen loaded at
// main menu per MODLOG; likely debug/session/tooling-triggered (e.g. debug UI/hotkeys/
// session paths). DebugInterface_7c is a coupled companion (DebugControl has DebugAccess
// vftable aspect; render/debug consumers expected). Verify triggers and consumers before
// enabling implemented exports.

namespace Jet {
// Forward declarations only; concrete definitions/layouts/vtables require decomp.
class DebugControl;
class DebugTextureView;
class PortalListConsole;
class PortalStatsConsole;
class RenderStatsConsole;
}  // namespace Jet

extern "C" void __cdecl JetGetPluginsDebugSupport();
