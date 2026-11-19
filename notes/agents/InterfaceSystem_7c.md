# InterfaceSystem_7c — analysis/planning report (read-only)

**Module:** `InterfaceSystem_7c.dll` (Jet UI, group: `ui`)  
**Status (README, 2026-11-19):** 319 KB code, 3358 functions, **0/2022 exports ours**, not started.  
**Port target:** 32-bit drop-in Jet module rewritten one unit at a time, validated by differential tests against the original in the same process, then later built for x64 (per repo approach in `README.md` / `PORT_PLAN.md`).

## Scope / constraints of this report

- Read-only analysis only. No source/build/doc/game/test/git changes were made.
- Based on local evidence: `README.md`, `notes/agents/COORDINATION.md`, `notes/agents/STATUS.md`, `notes/RenderSystem.md`, `notes/InputSystem.md`, `notes/KernelResource.md`, `notes/WaterSystem.md`, Ghidra maps in `ghidra_maps/jet/ui/` (`InterfaceSystem_7c.dll.functions.tsv`, `vtnames_InterfaceSystem_7c.dll.tsv`), and export lists in `D:\trainz12-decomp\exports\` (`InterfaceSystem_7c.tsv`, `InterfaceResources_7c.tsv`). No `src/jet/ui/` implementation exists yet.
- `COORDINATION.md` (2026-11-19 snapshot) flags in-flight/uncommitted **RenderSystem_7c** work and a known pre-existing **WaterSystem_7c** build issue; treat RenderSystem sources/tests/docs as owned/in-flight for implementation coordination.

## Overview / role (inferred from evidence)

`InterfaceSystem_7c` is the main Jet UI/runtime library: it owns the UI object model (region/control hierarchy, states, desktop/top-level system, dialogs/windows, common widgets, drag-and-drop, and OSD/overlay objects) and exposes a large C++ ABI surface to the rest of the engine/game.

**Primary responsibilities (evidence-backed):**

- **UI core & composition:** `Jet::UI::Region` (+ `Region::Relative`), `Jet::UI::Base`, `Jet::UI::Auxiliary`, `Jet::UI::Event`, `Jet::UI::Value`, `Jet::UI::State`/`Jet::UI::NormalState`, `Jet::UI::Desktop`, `Jet::UI::InterfaceSystem` (functions/vtables/exports present). Region APIs include tree/composition, layout/relative positioning, hit/input dispatch hooks, cloning/factory creation, focus/canvas/fronting helpers (e.g. `Add`, `AddControl`, `AddRoot`, `ClampPos/ClampResize`, `CreateInstance`, `BringToFront`, `BringCanvasToFront`, `SetFocus`, `GetInterfaceSystem`).
- **Desktop & dialog lifecycle:** `Desktop` manages dialogs/state/region allocation and loading (`AddDialog`, `GetState`, `AllocRegionType`, `CreateControl/CreateSubControl`, `LoadDialog` overloads). `Jet::UI::DialogResource` is a resource-backed dialog loader/creator (`Create`, CRC/size helpers, vtable slots present); many dialog/widget types exist as concrete `Region` subclasses.
- **Input/UI bridge & interaction state:** `Jet::InputUIExtension` and `Jet::UI::InputUIExtensionState`-taking APIs, keyboard/char event handling, focus/capture-related exports (`SetFocus`/`GetFocus`, `BringToFront`), and drag-and-drop classes (`DragNDropState`, `DNDPack`, `DNDControlsPack`, `DNDShadowControlPack`, etc.).
- **Rendering-facing UI primitives/overlays:** `Jet::Canvas` plus OSD family `Jet::OSD`, `Jet::OSDConsole`, `Jet::OSDLine`, `Jet::OSDFont`, `Jet::OSDFont8a`, `Jet::OSDFontPlugin`, `Jet::OSDProfile`, `Jet::OSDSystem`, `Jet::SelectionBox`, `Jet::UI::InterfaceSprite` (many vtable slots/functions/exports; constructors take `RenderKernel`/`LayerGroup`/`FontInstance` in OSD cases).
- **UI resource/value wrappers:** lightweight UI-side types such as `Jet::UI::IFont`, `Border`, `Cursor`, `Icon`, `Image` (many exports; used for properties/drawing/config binding) distinct from heavier resource implementations in `InterfaceResources_7c`.
- **Plugin/registry entrypoint:** exported `JetGetPluginsInterfaceSystem` and `Jet::UI::InterfaceSystem::uid`; vtable/name evidence for plugin factories (e.g. `PluginFactoryOf<Jet::UI::Desktop,Jet::UI::InterfaceSystem>`, many `PluginFactoryOf<...,Jet::UI::Region>` for widgets/dialogs) and `ResourceType<Jet::UI::DialogResource>`.

## Inventory (evidence summary)

### Quantitative snapshot

| Source | Evidence |
|---|---|
| `README.md` | 319 KB, 3358 functions, 0/2022 exports |
| `D:\trainz12-decomp\exports\InterfaceSystem_7c.tsv` | 2023 lines total (header + **2022 exports**) |
| `ghidra_maps/jet/ui/InterfaceSystem_7c.dll.functions.tsv` | 3431 lines total (header + **3430 mapped functions**) |
| `ghidra_maps/jet/ui/vtnames_InterfaceSystem_7c.dll.tsv` | 277 lines total (header + **276 vtable-slot name rows**) |
| `D:\trainz12-decomp\exports\InterfaceResources_7c.tsv` | 170 lines total (header + **169 exports**, sibling UI resources module) |
| `ghidra_maps/jet/ui/InterfaceResources_7c.dll.functions.tsv` | 385 mapped functions (header+384 rows observed earlier) |

> Function/export naming in Ghidra and exports is substantial but not a complete semantic layout spec; class sizes, exact slot meanings, and some internal helpers require decompilation/disassembly during implementation.

### Major classes / subsystems

**Core hierarchy & runtime**
- `Jet::UI::Region` (306 mapped functions): central node/control with relative layout (`Region::Relative`), child/tree ops, properties/config serialization hooks, cloning/factory instantiation, focus/visibility/canvas ordering/input hooks. Vtable/vbtable evidence (`??_8Region...`, vbase destructor, multiple vftable bases in other controls) indicates non-trivial inheritance/layout.
- `Jet::UI::Base`, `Jet::UI::Auxiliary`: foundational control/base types (functions/vtables present).
- `Jet::UI::Desktop` (60 mapped functions): top-level UI host/state/dialog management, region allocation/creation, dialog loading.
- `Jet::UI::InterfaceSystem` (small direct function set but key entrypoint): plugin-facing system type, `uid`, `JetGetPluginsInterfaceSystem`.
- `Jet::UI::State` and `Jet::UI::NormalState` (~114 mapped functions combined by namespace grep): interaction/state machine, event/key binding, focus, selection/drag behavior (`RegisterCallbackSystem`, `BindEvent/BindKey`, focus/select/bring-to-front helpers; many `NormalState::vf*` slots in vtnames).
- `Jet::UI::Event`, `Jet::UI::Value`: event/value primitives.

**Input & drag-and-drop integration**
- `Jet::InputUIExtension` (+ observer linkage visible via `OSDProfile` base in vtables), `Jet::UI::InputUIExtensionState` parameters in bind APIs.
- `Jet::UI::DragNDropState`, `Jet::UI::DNDPack`, `DNDControlsPack`, `DNDShadowControlPack` and DnD-related methods (`Abort`, `Animate`, etc.).

**Dialogs & dialog resource loading**
- `Jet::UI::Dialog` and specialized dialogs: `DialogFiles`, `DialogOpenFile`, `DialogMessage`, `DialogPopup`, `DialogQuery`, `DialogToolTip` (constructors/destructors/vtables/factories/exports present).
- `Jet::UI::DialogResource` / `DialogInstance`: resource-backed dialog definition/load/create path, CRC and size accounting; `ResourceType<Jet::UI::DialogResource>` in vtnames.

**Widgets / controls (broad set; `Jet::UI::*` export grouping by class shows heavy coverage)**
- High-count/central: `Region` (288 export-name hits), `State` (119), `Group` (113), `EditBox` (86), `Scroller` (86), `Window` (72), `TreeView` (61), `Dialog` (60), `Menu` (56), `ListView` (53), `Progress` (47), `Paragraph` (46), `Switch` (41), `MenuItem`/`DropDown` (35), `SplitWin`/`Icon` (34), `IFont` (33), `Button`/`RegionSelect`/`Tab` (31), etc.
- Additional observed types: `Check`, `Radio`, `Cluster`, `Custom`, `GControl`, `Image`, `Border`, `Cursor`, `Static`, `Slider`, `Spin`, `RollUp`, `IProperties`/`IProp*`, `Timer`, `SelectBase`, `InterfaceTask`, `InterfaceSprite`, etc. (from exports/functions/vtnames).
- Widget `CreateInstance`/properties/config methods are widespread (`GetProperties(..., ConfigData&, bool)`, `GetConfigItem`, `Region::CreateInstance` from `PString`/`ConfigData`).

**Overlays / on-screen display & canvas**
- `Jet::Canvas` and layered UI/overlay objects: `OSD`, `OSDConsole`, `OSDLine`, `OSDFont`, `OSDFont8a`, `OSDFontPlugin`, `OSDProfile`, `OSDSystem`, `SelectionBox`, `UI::InterfaceSprite` (143 mapped functions in this family by grep). Shared/overlapping vtable slots in `vtnames` suggest common base/ABI relationships among canvas/OSD/sprite classes.

**UI value/style helper types**
- `Jet::UI::Border`, `Cursor`, `Icon`, `Image`, `IFont` (142 export-name hits across these types): parsing/assignment/comparison/conversion and draw/init helpers used by widgets/states/resources.

## Dependencies (evidenced and inferred)

### External runtime dependencies (from export signatures/maps/notes)

- **Render2DCore_7c / RenderSystem_7c (graphics path):** `Jet::RenderKernel`, `Jet::LayerGroup`, `Jet::Bitmap`, `Jet::FontInstance`, texture/material/display concepts implied by OSD/canvas/font-texture creation (`CreateFontTexture(Bitmap)`, OSD ctors taking `RenderKernel`/`LayerGroup`, `GetBitmap`/canvas opacity/bring-canvas-to-front APIs). `Render2DCore_7c` is complete per README; `RenderSystem_7c` is **partial and in-flight** (319/3358? no—README shows RenderSystem 351 KB 3628 funcs 2185/2687 partial) and must be treated as owned/in-progress for implementation changes.
- **InputSystem_7c (input path):** `InputUIExtension`, `InputUIExtensionState`, keyboard/char event types in UI APIs, focus/event binding semantics; `InputSystem_7c` is marked done in README, but `InputDirectX_7c` is not started. `notes/InputSystem.md` documents `InputExtension`/`InputUIExtension` shapes and event flow relevant to UI integration.
- **Kernel core modules:** `Kernel_7c`/`KernelStandard_7c`/`KernelResource_7c`/`KernelResources_7c` provide foundational types and services widely used in signatures: `PString`, `ConfigData`, `ResourceDatabase`, `Resource*`, containers/strings/files/config/math/time/sync primitives, plugin/reference-count/base infrastructure (consistent with project patterns). `DialogResource` and dialog loading paths explicitly depend on resource/database/config APIs.
- **InterfaceResources_7c (sibling UI resources, not started):** separate module (169 exports) provides `BorderResource`, `BitmapBorderResource`, `CursorResource`/`CursorInstance`, `Font`/`FontInformation`/`FontBaseResource`/`FontResource`/`FontFx*`, `IconResource`/`IconInstance`, and plugin entry `JetGetPluginsInterfaceResources`. InterfaceSystem consumes/references these resource types via UI wrappers and rendering/font/cursor setup.
- **Other engine/game callers (likely):** not enumerated here, but this is a large public Jet UI ABI (2022 exports) expected to be used by `trainz.exe`, tools, and other Jet/game modules; replacement must preserve exported names, calling conventions, layouts, and behaviors.

### Internal dependency character

- High coupling between **Region/Base**, **State/Desktop**, **Dialog/DialogResource**, **widgets**, and **OSD/Canvas** subsystems.
- Significant **vtable/slot-driven** behavior and callback/binding infrastructure; several vtable slots are shared/aliased across classes in `vtnames` maps (must be interpreted carefully during implementation).
- **Config-driven UI construction** (`ConfigData`, region/dialog instantiation/load, property get/set) is a cross-cutting dependency.

## Proposed unit breakdown / implementation order (critical given size)

Goal: minimize integration risk by landing ABI-stable foundations, then runtime/input/render bridges, then high-fanout controls/dialogs, with differential tests per unit. Suggested granularity keeps each unit testable and limits cross-cutting layout churn. (Exact function-to-unit mapping should be refined from decomp/disasm when implementation starts.)

| Order | Unit (suggested) | Scope / key classes | Rationale / prerequisites |
|---:|---|---|---|
| U0 | ABI scaffolding & export plumbing | `JetGetPluginsInterfaceSystem`, module init/static registry hooks as needed, `.def`/forwarding strategy, generated maps usage, `src/jet/ui/InterfaceSystem_7c/` skeleton | Establish replaceable DLL skeleton and stable export surface early; keep unimplemented exports forwarding if following repo pattern. |
| U1 | UI value types & lightweight helpers | `Jet::UI::Event`, `Value`, `Region::Relative`, `Border`, `Cursor`, `Icon`, `Image`, `IFont`, small utilities | Low-level data/property types used broadly by regions/widgets/config; easiest to validate in isolation. |
| U2 | Region/Base core & hierarchy/layout | `Region`, `Base`, `Auxiliary`, `RegionSelect` basics, child/tree ops, relative/layout/clamp/clone/type/property plumbing | `Region` is the highest-leverage dependency (306 mapped funcs); must stabilize object layout and core virtual behavior before widgets. |
| U3 | Canvas/overlay primitives integration | `Canvas`, `InterfaceSprite`, `SelectionBox`, minimal render-facing hooks needed by Region/Desktop/OSD | Provides drawing/compositing contracts shared with OSD and controls; depends on Render2DCore/RenderSystem interfaces. |
| U4 | State/input routing & DnD | `State`, `NormalState`, `InputUIExtension` bridge, key/char/mouse/focus/capture dispatch, `DragNDropState`/DNDPack* | Central interaction semantics; depends on U2 and InputSystem contracts; high behavioral risk so isolate early. |
| U5 | Desktop & InterfaceSystem runtime | `Desktop`, `InterfaceSystem`, `InterfaceTask`/registration, region allocation/factory lookup, dialog load orchestration | Integrates core, state, factories, and resources; enables end-to-end dialog loading tests. |
| U6 | Dialog resources & dialog base/specializations | `DialogResource`, `DialogInstance`, `Dialog`, `DialogFiles/OpenFile/Message/Popup/Query/ToolTip` | Needed for real UI data flows; depends on KernelResource/ConfigData and U2/U5 creation paths. |
| U7 | OSD/font overlay subsystem | `OSDSystem`, `OSD`, `OSDConsole`, `OSDLine`, `OSDFont/8a/Plugin`, `OSDProfile` | Heavy RenderKernel/LayerGroup/FontInstance/texture interaction; can be staged after core runtime and canvas contracts are stable. |
| U8 | Basic widgets | `Button`, `Check`, `Radio`, `Static`, `Group`, `Cluster`, `Custom`, `Image`/icon-using controls, `Progress`, `Slider`, `Spin` | Common controls with comparatively simpler composition dependencies; build on stable Region/State/Desktop behavior. |
| U9 | Text/edit & menu/list widgets | `EditBox`, `Paragraph`, `Menu`, `MenuItem`, `ListView`, `TreeView`, `Tab`, `DropDown`, `Scroller` | Higher interaction/state/render complexity and font/text dependencies; likely many edge-case behaviors. |
| U10 | Complex containers/dialog-heavy widgets & properties UI | `Window`, `SplitWin`, `RollUp`, `IProperties`/`IProp*`, remaining specialized controls and callback/template glue | Tackle after primitives and common widgets are trustworthy; expect layout/focus/callback subtleties. |
| U11 | Final parity, statics, factories, cleanup | Global/static initializers, factory/resource-type registration completeness, all remaining exports, behavior/quirk parity passes | Close long-tail exports and stateful/global behavior once core correctness is established. |

**Ordering notes:** keep RenderSystem-facing and InterfaceResources-facing boundaries explicit at each step; prefer differential coverage on exported behavior and class-level invariants before broad in-game validation.

## Risks

- **Scale and ABI surface:** 319 KB / ~3430 mapped functions / 2022 exports across many polymorphic classes make partial replacement easy to destabilize; long-tail parity work is significant.
- **Layout and virtual-dispatch complexity:** multiple vftables/vbtables, vbase destructors, adjustor thunks, and shared/aliased slot names in vtnames (e.g. common `ReferenceCount`/canvas/desktop/system slots) increase risk of wrong object layout, slot order, or calling-convention mismatches.
- **Cross-module binary contracts:** UI types interact with `RenderKernel`/`LayerGroup`/`Bitmap`/`FontInstance`, `InputUIExtension*`, `ResourceDatabase`/`ConfigData`/`PString`, and sibling resource types; mismatches can fail far from the originating unit.
- **Behavioral/statefulness:** desktop/state/focus/capture/drag/modal/dialog lifecycles, config parsing/loading, and callback/event routing are stateful and likely contain original quirks that matter to compatibility.
- **In-flight dependencies:** `RenderSystem_7c` is partial and explicitly in-flight per coordination notes; `InterfaceResources_7c` is not started. Coordinated ABI assumptions are required.
- **Testing difficulty:** many widgets/OSD/dialog paths may be hard to exercise without the full game; constructing faithful differential harnesses for UI/render/input interactions will be non-trivial compared to lower-level modules.
- **Static/registry/plugin behavior:** `JetGetPluginsInterfaceSystem`, `uid`, factories, and resource-type registrations may depend on initialization order and global state not fully visible from exports alone.
- **Repository coordination constraints:** current tree has unrelated in-progress/uncommitted work (notably RenderSystem); any future implementation must avoid contended shared files/build outputs without explicit ownership.

## Cross-agent notes

### InterfaceResources_7c (ui, not started)

- `InterfaceResources_7c` (169 exports, ~384 mapped funcs) owns concrete UI resources: borders (`BorderResource`, `BitmapBorderResource`), cursors (`CursorResource`, `CursorInstance`), fonts and font effects (`Font`, `FontInformation`, `FontBaseResource`, `FontResource`, `FontFx*`, `FontInstance`), icons (`IconResource`, `IconInstance`), resource types/factories, and `JetGetPluginsInterfaceResources`.
- `InterfaceSystem_7c` contains UI-facing wrappers/types (`Jet::UI::Border/Cursor/Icon/Image/IFont`) and uses font/cursor/image/border concepts in widgets, dialogs, and OSD setup. **Coordination needed on:** resource load/init/draw contracts, string-key/property parsing compatibility, reference counting/ownership, and any shared IDs/enums/struct layouts crossing the module boundary.
- Recommend defining/recording the boundary early (which side owns parsing, GPU/bitmap preparation, caching, and lifetime) to avoid duplicated or incompatible implementations.

### RenderSystem_7c / Render2DCore_7c (graphics)

- `Render2DCore_7c` is done (per README) and likely underpins 2D drawing primitives; `RenderSystem_7c` is partial and currently in-flight (coordination warning). `InterfaceSystem_7c` depends on render-facing types/APIs (`RenderKernel`, `LayerGroup`, `Bitmap`, `FontInstance`, OSD font texture creation, canvas/overlay behavior).
- **Do not modify RenderSystem implementation/test/docs from parallel work without coordination.** InterfaceSystem planning/implementation should consume the currently exposed/owned RenderSystem ABI and note any required contracts discovered (vtable slots, object ownership, threading/update expectations).
- OSD/canvas parity will likely require careful alignment with RenderSystem display/texture/font paths rather than treating UI rendering as isolated logic.

### Input modules (InputSystem_7c, InputWin32_7c, InputDirectX_7c)

- `InputSystem_7c` and `InputWin32_7c` are done per README; `InputDirectX_7c` is not started. `InterfaceSystem_7c` integrates input through `InputUIExtension`, `InputUIExtensionState`, key/char event handling, focus/capture, and drag-and-drop behavior.
- `notes/InputSystem.md` indicates Input/extension/observer patterns and that some emulation/input-unit internals are already established; UI-side event routing must match these contracts (including activation/exclusivity/update semantics where applicable).
- **Coordination needed on:** event delivery ordering, focus/capture ownership, DnD state transitions, and any assumptions about device availability/driver-specific behavior (especially if `InputDirectX_7c` work begins later).

## Suggested next analysis/implementation prep steps (non-binding)

1. Produce a class/layout inventory for `Region`/`Base`/`State`/`Desktop`/`DialogResource` (sizes, vftable/vbtable layouts, key fields) from disassembly/decomp before coding U1-U5.
2. Enumerate all `InterfaceSystem_7c` exports by owning class/unit and mark cross-module type dependencies to drive `.def`/forwarding and test scaffolding.
3. Define minimal differential harness strategy: headless object/state tests first (construction, config/property round-trips, event/state transitions), then renderer/input-coupled tests with controlled stand-ins where possible.
4. Record ABI touchpoints with `InterfaceResources_7c` and in-flight `RenderSystem_7c` in coordination/status docs when implementation ownership is assigned.

## Scaffold (Wave 5)

**Date:** 2026-11-19 (module-owned scaffold prep; read-only w.r.t. shared/contended files)

### Files created (module-owned)
- `trainz12-x64/src/jet/ui/InterfaceSystem_7c/implemented.txt`
  - Proxy-first: header comment only, **no exports enabled** (all 2022 remain forwarded to `InterfaceSystem_7c_orig.dll` when wired). `JetGetPluginsInterfaceSystem` is evidenced (exports/maps, RVA 0x27040) but left forwarded until registration ABI/side effects/order are recovered and proven trivially-safe.
- `trainz12-x64/src/jet/ui/InterfaceSystem_7c/InterfaceSystem.h`
  - Evidence-documented placeholder header: cites README/CONSOLIDATED_PLAN/agent notes, exports TSV (2022), fmap (3430 funcs) and vtnames (276 slot rows). Documents evidenced entrypoints/symbols (`JetGetPluginsInterfaceSystem`, `Jet::UI::InterfaceSystem::uid`, `Region::GetInterfaceSystem`, `Desktop::LoadDialog` with `InterfaceSystem::LoadOptions`) and major clusters (Region/Base/State/Desktop/InterfaceSystem/runtime; DialogResource/Dialog*; broad widget/control set; InputUIExtension/State/DnD; Canvas/SelectionBox/InterfaceSprite/OSD*; UI Border/Cursor/Icon/Image/IFont wrappers). Explicitly states no ABI/layout/vtable/vbtable/slot/RTTI/registration/ownership/behavior is verified; no class layouts invented.
- `trainz12-x64/src/jet/ui/InterfaceSystem_7c/InterfaceSystem.cpp`
  - Minimal placeholder TU including the header, notes staged TODO units, and provides a **no-op** `extern "C" void __cdecl JetGetPluginsInterfaceSystem()` body for future local use only (not enabled/exported via `implemented.txt`).

No shared/contended files modified (`src/jet/CMakeLists.txt`, `README.md`, `PORT_PLAN.md`, `MODLOG.md`, `tools/progress_exports.tsv`, other modules, RenderSystem in-flight files, `notes/agents/STATUS.md`, `notes/agents/COORDINATION.md`). No builds/tests run.

### Stubbed vs deferred
- **Stubbed (safe, local only):** documented placeholder header/source and a no-op plugin-entry symbol in-module; `implemented.txt` kept empty of enabled exports (proxy-first, forward-all intent when wired).
- **Deferred (must not be guessed):** every exported/implemented behavior and all 2022 exports; plugin/resource/factory registration set/IDs/order/side effects; all class/struct layouts, vftable/vbtable/slot counts/order, adjustor thunks, MI/vbase relationships; config/property/dialog load/save/clone semantics; event/state/focus/capture/DnD routing and lifecycle; render/canvas/OSD/font-texture behavior; callbacks, statics/global init, and cross-module ownership contracts.

### Key ABI / dependency TODOs and risks
- **Scale/ABI:** 319 KB, ~3430 mapped funcs, 2022 exports, many polymorphic types; long-tail parity and partial replacement are high-risk.
- **Layout/vtables:** Region/Base/State/Desktop/DialogResource and widgets/OSD show vbase destructors and heavily shared/aliased slot names across classes in vtnames—slot meaning/order and object layout require decomp/disasm, not name-only mapping.
- **Critical deps (contracts must be recovered/aligned):**
  - `InterfaceResources_7c` (not started): UI Border/Cursor/IFont/Icon/Image wrappers vs concrete resources/instances (font/cursor/border/icon parsing, ownership/lifetime/caching/GPU prep) — boundary TBD.
  - `RenderSystem_7c` (partial, **in-flight/owned**) and `Render2DCore_7c` (done): RenderKernel/LayerGroup/Bitmap/FontInstance/texture/display/canvas/OSD APIs and ownership/threading expectations.
  - `InputSystem_7c`/`InputWin32_7c` (done), `InputDirectX_7c` (not started): InputUIExtension/InputUIExtensionState, event delivery order, focus/capture, DnD transitions/activation.
  - Kernel*/KernelResource(s): PString/ConfigData/ResourceDatabase/Resource/UId/plugin/refcount/container primitives and DialogResource/registry behavior.
- **Behavioral statefulness:** desktop/state/focus/capture/drag/modal/dialog lifecycles, config-driven construction/property round-trips, callbacks, and original quirks likely matter for compatibility.
- **Statics/init:** `JetGetPluginsInterfaceSystem`, `uid`, factories and resource-type registrations may depend on init order/global state.
- **x64:** preserve 32-bit ABI-observable layout/behavior for drop-in parity; pointer/size_t/packing differences need care.

### Later build-wiring steps (serialized/shared-file follow-up, by claim owner)
1. Add `InterfaceSystem_7c` to the 32-bit Jet module list in `trainz12-x64/src/jet/CMakeLists.txt` (scaffold pattern used by InterfaceResources_7c/Render2DCore_7c): `jet_module(InterfaceSystem_7c)` with `_CRT_SECURE_NO_WARNINGS` and evidenced `target_link_libraries` to original Jet import libs (likely Kernel*/KernelResource(s), RenderSystem/Render2DCore, InputSystem, InterfaceResources as contracts are confirmed) — **do not guess unresolved deps**.
2. Keep `implemented.txt` forwarding-all until a specific export is ABI-verified and trivially safe to enable; when enabling, follow existing def/gen flow and update `tools/progress_exports.tsv`/docs in the same coordinated/claimed change.
3. Record module wiring/progress in `README.md`/`PORT_PLAN.md`/`MODLOG.md` and consolidate coordination/status updates only as the claimed owner in a serialized step. Coordinate with in-flight RenderSystem and with InterfaceResources boundary before integration.
