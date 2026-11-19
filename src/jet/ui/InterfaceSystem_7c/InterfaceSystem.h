#pragma once

// Wave 5 scaffold for InterfaceSystem_7c (Jet UI, Trainz 2012 x64 port).
// Evidence (read-only):
// - README.md / notes/agents/CONSOLIDATED_PLAN.md / notes/agents/InterfaceSystem_7c.md:
//   319 KB, 3358 functions, 0/2022 exports; Wave 5, dependency-heavy, RS in-flight.
// - D:\trainz12-decomp\exports\InterfaceSystem_7c.tsv: 2022 exports (header+2022 rows total 2023).
// - ghidra_maps/jet/ui/InterfaceSystem_7c.dll.functions.tsv: 3430 mapped functions.
// - ghidra_maps/jet/ui/vtnames_InterfaceSystem_7c.dll.functions/vtnames_InterfaceSystem_7c.dll.tsv:
//   276 vtable-slot name rows with heavy slot sharing/aliasing and multi-base/vbase evidence.
// - Patterns: src/jet/ui/InterfaceResources_7c/, src/jet/graphics/Render2DCore_7c/.
//
// STATUS: placeholders/documentation only. No ABI, layout, vtable/vbtable/slot order, RTTI,
// calling convention, registration, ownership, threading, or behavioral contracts are verified
// here. Do NOT treat declarations as final and do NOT invent behavior.

// ---- Evidenced entrypoints / module-level symbols ----
// - JetGetPluginsInterfaceSystem (exports ordinal/name present, unmangled extern "C", RVA 0x27040
//   in exports). Exact signature/return semantics and registration side effects/order must be
//   recovered from original decomp/differential tests before implementation/enabling.
// - Jet::UI::InterfaceSystem::uid (exported data symbol evidenced in exports).
// - Jet::UI::Region::GetInterfaceSystem() (static, evidenced) and Desktop/LoadDialog overloads
//   take Jet::UI::InterfaceSystem::LoadOptions (enum/ABI unknown).

extern "C" void __cdecl JetGetPluginsInterfaceSystem();

// ---- Major evidenced clusters (names from exports/functions/vtables; not exhaustive) ----
// Core hierarchy/runtime:
//   Jet::UI::Region (+ Region::Relative), Base, Auxiliary, Event, Value,
//   State, NormalState, SelectBase/RegionSelect, Desktop, InterfaceSystem, InterfaceTask.
//   Region shows tree/layout/relative/clamp/focus/canvas/property/clone/factory behavior;
//   Desktop shows state/dialog management, AllocRegionType/Create(Control|SubControl),
//   LoadDialog/SaveDialog/DisplayMessage/Get*; vbase destructors and multi-base slots evident.
//
// Dialog/resource loading:
//   Jet::UI::DialogResource / DialogInstance, ResourceType<Jet::UI::DialogResource>,
//   Jet::UI::Dialog and specializations (DialogFiles, DialogOpenFile, DialogMessage,
//   DialogPopup, DialogQuery, DialogToolTip) with config/resource-database/CRC/size/load paths.
//
// Widgets/controls (broad set): Group, EditBox, Scroller, Window, SplitWin, RollUp,
//   TreeView/ListView/Menu/MenuItem/DropDown/Tab/Slider/Spin/Progress/Paragraph,
//   Button/Check/Radio/Switch/Static/Cluster/Custom/Image/Border/Cursor/Icon/IFont,
//   IProperties/IProp*, GControl, Sep, Row, Timer, DragnDrop, etc., many with
//   CreateInstance/GetProperties/SetProperties/GetConfigItem and PluginFactoryOf<...,Region>.
//
// Input and drag-and-drop:
//   Jet::InputUIExtension, InputUIExtensionState-parameterized binding/event APIs,
//   DragNDropState, DNDPack, DNDControlsPack, DNDShadowControlPack and related flows.
//
// Rendering-facing UI/overlays:
//   Jet::Canvas, Jet::SelectionBox, Jet::UI::InterfaceSprite(+Control),
//   Jet::OSD/OSDConsole/OSDLine/OSDFont/OSDFont8a/OSDFontPlugin/OSDProfile/OSDSystem.
//   OSD ctors/methods reference RenderKernel/LayerGroup/FontInstance/Bitmap/texture concepts.
//
// UI value/style wrappers:
//   Jet::UI::Border, Cursor, Icon, Image, IFont (distinct from InterfaceResources implementations;
//   cross-module contracts must be verified).

// ---- Dependency / ABI recovery TODOs (must be done before enabling exports) ----
// - Recover class/struct sizes, field layout/packing/alignment, and construction/destruction
//   semantics (including vbase/vbtable) for Region/Base/State/Desktop/DialogResource and key
//   controls/overlays; vtnames show shared/aliased slots across classes.
// - Recover vftable/vbtable layout, slot order/count, adjustor thunks, and MI relationships;
//   do not map by name alone.
// - Recover factory/resource-type/plugin registration: JetGetPluginsInterfaceSystem, InterfaceSystem
//   uid, ResourceType<DialogResource>, and many PluginFactoryOf registrations/IDs/order/side effects.
// - Recover config/property/dialog loading and callback/event/state/focus/capture/DnD contracts.
// - Verify dependency boundaries and binary contracts with:
//   * InterfaceResources_7c (not started): Border/Cursor/Font/Icon resources/instances, parsing,
//     ownership/lifetime/caching/GPU prep and UI wrapper compatibility.
//   * RenderSystem_7c (partial, in-flight, owned elsewhere) and Render2DCore_7c (done):
//     RenderKernel/LayerGroup/Bitmap/FontInstance/texture/display/canvas/OSD contracts.
//   * InputSystem_7c/InputWin32_7c (done), InputDirectX_7c (not started): InputUIExtension,
//     event delivery/focus/capture/activation semantics.
//   * Kernel/KernelStandard/KernelResource(s): PString, ConfigData, ResourceDatabase/Resource,
//     UId, plugin/refcount/base/container/string/file/math/time primitives.
// - x64 port note: preserve original 32-bit ABI-observable layout/behavior for drop-in parity;
//   pointer/size_t width and packing differences must be handled carefully when moving to x64.

namespace Jet {
namespace UI {

// Intentionally minimal: no guessed class layouts, no invented methods/fields/vtables.
class InterfaceSystem;

}  // namespace UI
}  // namespace Jet
