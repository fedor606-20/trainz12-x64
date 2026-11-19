# DebugSupport_7c — detailed analysis/planning report

**Scope:** read-only analysis only (no source/build/doc edits, no builds/tests/game runs).  
**Report target:** `trainz12-x64/notes/agents/DebugSupport_7c.md`  
**Date:** 2026-11-19

## 1) Overview / role (evidenced)

- **Module/group:** `DebugSupport_7c.dll` in **Debug** group (`README.md` progress table: 16 KB, 201 functions, 0/119 exports, not started). Debug group also includes `DebugInterface_7c.dll` (3 KB, 50 functions, 0/38 exports, not started).
- **Observed runtime status:** `MODLOG.md` ("What actually loads (main menu, 2026-10-18)") lists `DebugInterface` among loaded Jet modules at main menu, but explicitly states **"Not loaded at the menu: ... DebugSupport..."**. README group note: "Not seen loaded at the menu; re-check in a session." This strongly suggests DebugSupport is debug/session/tooling functionality loaded conditionally (session/debug UI/features), not part of baseline menu startup.
- **Primary role (from exports):** implements runtime debug control and overlays/stats for rendering/debugging:
  - `Jet::DebugControl` singleton/facade: create/destroy, per-frame `Update(float)`, toggles/getters/setters for wireframe, vertex/face normals, object axis, texture mode, VSync, render/portal stats consoles, portal list/stats, debug viewport state/camera/target/alignment, dynamic/static geometry visibility, portal stencil/debug-render-lines modes, profiler state/type/visibility/mouse observation, camera reset, screen dump (`D:\trainz12-decomp\exports\DebugSupport_7c.tsv`).
  - Debug visualization/output helpers: `Jet::DebugTextureView` (add/remove textures/display textures with optional tint/`Vector4`, render/draw quad/delete), console classes `Jet::RenderStatsConsole`, `Jet::PortalStatsConsole`, `Jet::PortalListConsole` (constructors for direct/kernel+layer-group contexts, `DisplayStats`, render-stats init/visibility/formatting).
  - Plugin entry: `JetGetPluginsDebugSupport` export present.
- **Relationship to DebugInterface:** `DebugControl` exposes/uses debug-access semantics (vtable entries in `DebugAccess` slot family; export includes four `DebugControl` vftables including `for 'DebugAccess'`: `??_7DebugControl@Jet@@6BDebugAccess@1@@`). `DebugInterface_7c` exports `Jet::DebugAccess` (flags/masks, control flags, reset-camera observer registration) and `Jet::DebugResetCamera` — i.e. likely the shared interface/base/registry side consumed by render/debug systems; DebugSupport appears to provide the concrete debug control/overlay implementation.

## 2) Inventory (specific, evidenced)

### 2.1 Binary/export-level facts
- **README:** DebugSupport_7c — 16 KB code, 201 functions, 0/119 exports ours.
- **Exports TSV:** `D:\trainz12-decomp\exports\DebugSupport_7c.tsv` contains 120 lines total (header + ordinals 1..119), so **119 exported symbols**.
  - Exported vftables (8): `DebugControl` for `DebugAccess`, `InputUIExtensionObserver`, `Modifier`, `ReferenceCount`; `DebugTextureView`; `PortalListConsole`; `PortalStatsConsole`; `RenderStatsConsole`.
  - Exported data: `?singleton@DebugControl@Jet@@1PAV12@A` (protected static).
  - Exported plugin entry: `JetGetPluginsDebugSupport`.
  - Exported API surface is concentrated in `Jet::DebugControl` (most methods), plus constructors/operators/destructors/helpers for `DebugTextureView`, `Portal*Console`, `RenderStatsConsole`.
  - Notable signatures:
    - `DebugControl::Update(float)` returns `Jet::ResultClass<&Modifier::results, Modifier::Results>` (`__cdecl`, virtual in export demangle).
    - `DebugControl::Create*` methods take `RenderKernel*` / UI geometry (`Point/Size/Rect`) / sort/profile/visibility flags and bool "attach/register" style trailing args in some cases.
    - `CreateProfiler(..., Jet::Profile::SortOption, bool, bool)` and `CreatePortalStatsConsole(..., bool, float, bool)` indicate layered/optional registration behavior.

### 2.2 Ghidra function map (`ghidra_maps/jet/debug/DebugSupport_7c.dll.functions.tsv`)
- **254 function rows** total (header + 253 entries in file). This is broader than README's "201 functions" because it includes CRT/init/unwind/SECURITY/thunks/import stubs/unattributed `FUN_*`/`Unwind@*` entries.
- Namespace/type buckets (by name matches in map):
  - `Jet::DebugControl`: 89 rows
  - `Jet::DebugTextureView`: 23 rows
  - `Jet::RenderStatsConsole`: 9 rows
  - `Jet::PortalStatsConsole`: 5 rows
  - `Jet::PortalListConsole`: 8 rows
  - `Jet::DebugViewPort`: 25 rows (mostly vf/thunk entries; few named direct methods beyond vf0 in this map view)
  - `OSD*` (OSDProfile/OSDConsole): 10 rows
  - **52 thunk functions** (`thunk == true`), many as cross-base/interface vtable adapters.
- Size hotspots (largest named functions from map): `RenderStatsConsole::DisplayStats` (2930 bytes, 0x10003c50), `DebugTextureView::Render` (601), `PortalStatsConsole::DisplayStats` (577), `DebugControl::CreateProfiler` (397), `RenderStatsConsole` ctors (~395/400), `DebugControl::DebugControl` (304), etc. This points to stats formatting/rendering and control wiring as main implementation complexity, not algorithms.
- Other notable internals: `Catch_All@...`, CRT startup/cookie/SEH helpers, MSVCR80 imports (`terminate`, `_unlock/__dllonexit/_lock`, `initterm*`, `_amsg_exit`, `__CppXcptFilter`, `_crt_debugger_hook`, `memcpy`, etc.), `JetGetPluginsDebugSupport` marked `IMPORTED` in function map source column (entry point record style).

### 2.3 Vtable/name mapping (`vtnames_DebugSupport_7c.dll.tsv`)
- Confirms class/interface structure and slot sharing/thunks:
  - `Jet::DebugControl` has multiple vftable bases exported (DebugAccess, Modifier, InputUIExtensionObserver, ReferenceCount slots visible) and repeated/aliased vf slots in map.
  - `Jet::DebugTextureView` vf0-vf10 mapped (with shared slots against DebugViewPort/OSDProfile/OSDConsole in naming).
  - `Jet::PortalListConsole`, `PortalStatsConsole`, `RenderStatsConsole` share vf0 at 0x10001290.
  - `Jet::DebugViewPort` vf0-vf12 present.
  - `Jet::OSDProfile` and `Jet::OSDConsole` vf slots present/thunked.
- Interpretation: DebugSupport likely implements/derives from several render/UI/debug base interfaces defined elsewhere (RenderSystem/Interface/DebugInterface/Input UI extension contracts). Exact MI layout must be recovered from asm/RTTI before implementation.

### 2.4 Companion module inventory: DebugInterface_7c (cross-relevant)
- **Exports TSV:** `D:\trainz12-decomp\exports\DebugInterface_7c.tsv`, 38 exported symbols.
  - `Jet::DebugAccess`: ctor/dtor/assign/copy, vftable, static getters/setters/toggles for portal/portal-debug/wire-on-texture/global-bounding-volumes/bounding-volume-mask/control flags, `Add/RemoveResetCameraObject`.
  - `Jet::DebugResetCamera`: ctor/copy/assign, vftable, virtual `ResetCamera()` overloads.
  - Protected/static data exports: `portalState`, `portalDebugMode`, `wireOnTexture`, `globalBoundingVolumes`, `boundingVolumeMask`, `controlFlags`, `resetCameraArray` (`FastDynamicArray<DebugResetCamera*,13,Less<...>>`).
  - `JetGetPluginsDebugInterface` export.
- **Function map:** 62 rows (includes CRT/SEH/unwind/FUNs); 24 `DebugAccess`, 3 `DebugResetCamera` named rows.
- **Role:** lightweight shared debug-state/access API and reset-camera registry, likely depended on by render modules and used as base/contract for DebugSupport's control.

## 3) Dependencies (evidenced + inferred, labeled)

### 3.1 Evidenced dependencies/couplings
- **DebugInterface <-> DebugSupport:** DebugSupport exports `DebugControl` vftable for `DebugAccess` base; DebugControl API and behavior conceptually extend DebugAccess concerns. DebugInterface defines exported static debug flags/masks/control flags and reset-camera array. These modules are tightly coupled by ABI/types.
- **Render stack coupling:**
  - `MODLOG.md`: "Render module dependencies (Jet imports): Render3DCore -> RenderSystem 320, **DebugInterface 3**" (evidences DebugInterface is used by Render3DCore).
  - DebugSupport symbols take/return render types: `RenderKernel*`, `LayerGroup*`, `ViewPort*`, `Camera*`, `Texture*`, `DisplayTexture*`, `FontInstance*`, `DebugViewPort*`, `Layer*`, `Point/Size/Rect`, `Vector3/Vector4`, `Profile::SortOption` (`exports/DebugSupport_7c.tsv`).
- **Input/UI extension coupling:** vftable `DebugControl ... for InputUIExtensionObserver`; methods like `ProfilerObservesMouse(InputUIExtension*)`.
- **Kernel/standard runtime coupling (from types/behavior):** `Jet::String`, containers/arrays implied by texture management and console formatting, `ResultClass`/`Modifier` results, singleton lifetime management, likely `ReferenceCount` conventions consistent with core Jet modules.
- **CRT/toolchain:** function map shows MSVCR80 CRT imports and CRT init/unwind machinery (same ecosystem as original TS12 modules).

### 3.2 Inferred dependencies (needs verification during implementation)
- Likely imports from **KernelStandard_7c** (strings, arrays, results/helpers), **Kernel_7c** (plugin/refcount/base utilities), **RenderSystem_7c / render modules** (RenderKernel, display/texture/camera/layer/viewport/debug viewport/OSD/profile interfaces), possibly **InputSystem/Interface** for UI extension observer integration. Exact import list per DLL must be dumped from original binary (`dump_imports.py`/exports environment) before coding/build wiring.

### 3.3 External consumers / load context
- No direct in-repo evidence of DebugSupport consumers in this workspace; `MODLOG.md` consumer examples around there list DebugSupport as an importer count in another context line but not as a dependency target. Practical assumption: debug UI/hotkeys/session tools or render debug paths create/use it when enabled. Must be validated in-game in a context where debug features are reachable.

## 4) Load conditions (important for test strategy)

- **Menu-time evidence:** not loaded at main menu (`MODLOG.md`); DebugInterface *is* loaded at menu. README marks DebugSupport as "Not seen loaded at menu; re-check in a session".
- **Planning implication:** differential/in-game verification cannot rely on menu-only checks. Need to identify a reproducible trigger (driving session, debug keybind, developer option, console/profile toggle path) that causes `CreateDebugControl`/plugin registration to run and surfaces stats/viewport/texture overlays.
- **Risk:** features may be partially dead/legacy in TS12 build 61388; absence of load may reflect unused paths rather than broken replacement.

## 5) Proposed implementation units / order (planning)

> General constraint reminders: keep forwarding proxy viable, preserve exact ABI/layout/calling conventions/vtable layout, move coupled state together, verify against original before expanding. Given 0/119 now, start with a non-behavioral baseline.

1. **U0 — Proxy/harness baseline (DebugSupport_7c)**
   - Create module skeleton with `jet_module(DebugSupport_7c)` forwarding all 119 exports to `DebugSupport_7c_orig.dll` (`implemented.txt` empty), add to build once coordinated (note: shared build/src edits are currently constrained by COORDINATION).
   - Goal: in-game load parity/no regressions in contexts where it loads; establish import-lib/orig-DLL naming and plugin export plumbing.

2. **U1 — DebugInterface_7c prerequisite/parallel tiny module (strongly recommended early)**
   - 38 exports, mostly static flags/control + small observer list (`FastDynamicArray`-shaped exported data) + virtual reset-camera base.
   - Rationale: DebugControl inherits `DebugAccess`; Render3DCore imports DebugInterface. Getting this ABI-correct early reduces cross-module layout/semantic mismatch. Can be proxied first, then implemented with differential checks on static state/flag behavior.

3. **U2 — DebugControl core lifecycle/state (DebugSupport)**
   - `CreateDebugControl/DestroyDebugControl`, singleton storage/export, ctor/dtor, vftables/layout, `Update`, basic getters/setters/toggles that only mutate internal flags and/or call simple dependencies.
   - Focus: exact MI layout, calling conventions (`__cdecl` members prevalent per demangles), `ResultClass` return ABI, singleton semantics.
   - Defer heavy rendering/console content generation if it complicates bring-up.

4. **U3 — Camera/viewport/debug render modes**
   - Debug viewport create/state/screen pos, camera pos/target/alignment/reset paths, `UseDebugViewportCameraTarget`, `ProcessLayerState`, geometry visibility and render-mode toggles (wireframe/normals/axis/texture/stencil/debug-render-lines/vsync hooks as applicable).
   - Likely depends on RenderSystem/Render3D viewport/camera contracts; may need vtable-offset knowledge consistent with existing RenderSystem work.

5. **U4 — Consoles and profiling integration**
   - `RenderStatsConsole` / `PortalStatsConsole` / `PortalListConsole` (ctor variants, `DisplayStats`, init/visibility/formatting) and `Create*Console`, profiler create/toggle/state/type/mouse-observe.
   - `RenderStatsConsole::DisplayStats` is the largest function (2930 bytes) — highest reverse-engineering/test effort in module; split verification by console type.

6. **U5 — DebugTextureView and screen dump / remaining features**
   - Texture add/remove (Texture vs DisplayTexture overloads, optional tint), `Render`, `DrawQuad`, `DeleteTexture`, `DumpScreen`, any remaining toggles.
   - Often easier after display/texture call patterns are known from RenderSystem.

7. **Final — remove proxy forwarding, validate in real debug/session scenario**

**Ordering rationale:** establish ABI and load/plugin behavior first, then state/lifecycle, then feature groups by dependency weight. Because DebugInterface is small and shared, implementing/proxying it coherently with DebugSupport is lower risk than treating DebugSupport in isolation.

## 6) Risks / unknowns

- **Load/verification gap:** module not seen at menu; without a known trigger, in-game functional verification is hard. Need a repro case (session mode, debug hotkey/config, or forced creation path) before claiming correctness.
- **Multiple inheritance / complex vtables:** 4 exported DebugControl vftables and many shared/thunked slots across `DebugViewPort/OSD*` suggest non-trivial MI and adapter thunks. Layout errors will break callers at runtime.
- **Cross-module interface contracts:** heavy use of RenderKernel/Display/Texture/Layer/Camera/Profile/OSD/Viewport/InputUIExtension types whose concrete layouts/vtable slots are owned by other modules (some in-progress, e.g. RenderSystem). Mismatched assumptions can cause subtle crashes.
- **RenderSystem in-flight:** `COORDINATION.md` notes uncommitted RenderSystem U7 work and marks RenderSystem source/tests/docs as owned/in-flight — avoid touching it; DebugSupport may need to consume evolving render interfaces, so coordination is required.
- **Calling conventions/return ABI:** many methods `__cdecl` in demangles; `Update` returns a `ResultClass` by value; member calling conventions and hidden args must match MSVC8/x86 exactly.
- **Static data/singleton ownership:** exported `singleton` and DebugInterface statics (`resetCameraArray`, flags, masks, control flags) must have correct storage/export semantics and thread/lifetime behavior.
- **Possibly dead/rare paths:** if never loaded in normal scenarios, implementation priority/value is lower and testing confidence limited; still required for completeness if export-compatible replacement is desired.
- **CRT/runtime details:** MSVCR80-era module behavior/SEH/allocator assumptions may matter; follow existing project pattern (drop-in DLL ABI/toolchain constraints).

## 7) Cross-agent notes

- **DebugInterface_7c:** likely companion/prerequisite API module (38 exports). Exported DebugAccess state/observers and DebugResetCamera base are directly relevant; Render3DCore depends on DebugInterface (`MODLOG.md`). Coordinate any DebugInterface implementation with DebugSupport to keep base layout/vftables/data exports consistent.
- **RenderSystem_7c / Render3DCore/Render3DFoundation/RenderOpenGL:** DebugSupport consumes render-kernel, viewport, layer, texture/display, camera, profiling/OSD concepts. RenderSystem is currently in-flight/uncommitted per `COORDINATION.md`; do not modify its files. Expect DebugSupport integration to depend on stable render interfaces/vtable contracts.
- **KernelStandard_7c / Kernel_7c:** expected providers of strings, containers, result types, refcount/plugin/runtime primitives used by exported signatures (e.g. `String`, `ResultClass`, arrays, `ReferenceCount`/`Modifier` patterns). Reuse established project conventions/layouts.
- **Input/UI layers (InterfaceSystem/InputSystem context):** `InputUIExtensionObserver` and `ProfilerObservesMouse(InputUIExtension*)` imply coupling; confirm actual interface owners/layouts before implementing observer callbacks.
- **Testing coordination:** because DebugSupport is not menu-loaded, agents should share any discovered repro (specific session, debug toggle, config flag, or hotkey path) in coordination/status notes before investing in deep behavior matching.
- **No source edits performed:** this report is the only write output produced for this task.

## 8) Evidence sources consulted
- `trainz12-x64/README.md` (progress/status/group notes)
- `trainz12-x64/MODLOG.md` (loaded modules at menu, render dependencies, general port facts)
- `trainz12-x64/PORT_PLAN.md` (overall sequencing/context)
- `trainz12-x64/notes/agents/COORDINATION.md`, `trainz12-x64/notes/agents/STATUS.md`
- `trainz12-x64/ghidra_maps/jet/debug/DebugSupport_7c.dll.functions.tsv`
- `trainz12-x64/ghidra_maps/jet/debug/vtnames_DebugSupport_7c.dll.tsv`
- `trainz12-x64/ghidra_maps/jet/debug/DebugInterface_7c.dll.functions.tsv`
- `trainz12-x64/ghidra_maps/jet/debug/vtnames_DebugInterface_7c.dll.tsv`
- `D:\trainz12-decomp\exports\DebugSupport_7c.tsv`
- `D:\trainz12-decomp\exports\DebugInterface_7c.tsv`
- Workspace scan: no existing `src/jet/**DebugSupport*` or `src/jet/debug/` files found at time of analysis

## 9) Scaffold (2026-11-19, module-owned, proxy-first)

**Files created (owned paths only):**
- `src/jet/debug/DebugSupport_7c/implemented.txt` — proxy-first; only `JetGetPluginsDebugSupport` listed as a trivially-safe no-op stub. All other 118 exports (DebugControl/DebugTextureView/Portal*Console/RenderStatsConsole methods/ctors/dtors/operators, 8 vftables, `singleton`, plugin-adjacent internals as exported) remain forwarded to `DebugSupport_7c_orig.dll` until verified.
- `src/jet/debug/DebugSupport_7c/DebugSupport.h` — evidence-only placeholder documenting 119 exported symbols/clusters (plugin, DebugControl singleton/update/create*/state/toggles/camera/viewport, consoles, texture view, vftables/data) and that ABI/layout/vtables/registration/runtime hooks must be recovered from decomp before enabling exports.
- `src/jet/debug/DebugSupport_7c/DebugSupport.cpp` — minimal `JetGetPluginsDebugSupport` no-op stub with warnings not to guess ABI/layout/behavior or enable additional exports.

**Stubbed vs deferred:**
- Stubbed (trivially safe): `JetGetPluginsDebugSupport` only (no registration side effects).
- Deferred (forwarded/unspecified until recovery): every other export and all behavior — class layouts/MI bases, vtable slot order and adjustor thunks (DebugControl DebugAccess/InputUIExtensionObserver/Modifier/ReferenceCount aspects; DebugViewPort/OSD* sharing per vtnames), `DebugControl::singleton` storage/ownership, `Update` `ResultClass<&Modifier::results,...>` return ABI, console/texture-view/render/profiling logic, plugin registration/factory semantics, and any runtime hooks.

**Key ABI/runtime-trigger TODOs and risks:**
- **Not menu-loaded (evidenced):** likely debug/session/tooling-triggered (`-debugall`/debug UI/hotkeys/session paths noted as context); real load/trigger/consumers unconfirmed — need repro before enabling/validating beyond proxy.
- **Complex MI/vtables/thunks:** 4 exported DebugControl vftables, shared/thunked slots with DebugViewPort/OSDProfile/OSDConsole, ~52 thunks in function map; layout/slot errors will break callers. Must recover from decomp/RTTI/asm.
- **Coupled companion:** DebugInterface_7c provides `DebugAccess` base semantics and exported debug state/reset-camera registry; Render3DCore depends on DebugInterface (MODLOG). Keep ABI consistent; do not assume DebugInterface internals here.
- **Cross-module contracts:** heavy dependence on RenderSystem/render-kernel, viewport/camera/layer/texture/display/font/profile/OSD, and InputUIExtensionObserver types/layouts/vslots (RenderSystem U7 in-flight/uncommitted per coordination) — avoid touching in-flight/shared files.
- **Calling/return ABI and statics:** `__cdecl` methods, by-value `ResultClass` return, exported singleton/data ownership/lifetime must match MSVC8/x86 original.

**Later build-wiring steps (separate claim; shared/contended files):**
- Add `src/jet/debug/DebugSupport_7c/` sources to `src/jet/CMakeLists.txt` via `jet_module(DebugSupport_7c)` with proxy/fwd pattern consistent with other debug modules, ensure `implemented.txt` drives forwarding for all non-stubbed exports.
- Arrange `DebugSupport_7c_orig.dll` placement/import-lib naming per project conventions; do not modify README/PORT_PLAN/progress exports or in-flight RenderSystem files.
- Verify in a context where DebugSupport actually loads (session/debug trigger) once ABI/runtime hooks and dependencies are recovered.
