# DebugInterface_7c — analysis / planning report (read-only)

- **Target module:** `DebugInterface_7c.dll` (Jet engine, debug group)
- **Report scope:** static evidence analysis only (exports, function/vtable maps, repo docs/logs). No source/build/docs modified outside this file.
- **Evidence sources reviewed:**
  - `README.md` (progress/status snapshot)
  - `MODLOG.md` (global facts, loaded-module notes, plugin/FastDynamicArray/kernel conventions)
  - `notes/agents/COORDINATION.md`, `notes/agents/STATUS.md`
  - `ghidra_maps/jet/debug/DebugInterface_7c.dll.functions.tsv`
  - `ghidra_maps/jet/debug/vtnames_DebugInterface_7c.dll.tsv`
  - `D:\trainz12-decomp\exports\DebugInterface_7c.tsv`
  - Related context checked: `ghidra_maps/jet/debug/DebugSupport_7c.dll.functions.tsv`, `vtnames_DebugSupport_7c.dll.tsv`, `D:\trainz12-decomp\exports\DebugSupport_7c.tsv`, `src/jet/CMakeLists.txt` (build pattern)

## 1. Overview / inferred role

**Status (README):** debug group module, **3 KB code, 50 functions, 0/38 exports implemented, not started**. MODLOG notes `DebugInterface` was **loaded at menu** in the recorded 32-bit module list (2026-10-18 snapshot), while `DebugSupport` was **not loaded at menu** in that same snapshot.

**What the exported API represents (evidenced by demangled exports):**
- `Jet::DebugAccess`: a small access/control surface exposing debug toggles/state as static data plus static getters/setters/togglers, and a registry of camera-reset targets.
  - Boolean/state flags: `portalState`, `portalDebugMode`, `wireOnTexture`, `globalBoundingVolumes`.
  - Mask: `boundingVolumeMask` (`unsigned long`) with `Set/Get/CheckBoundingVolumeMask`.
  - Generic flags: `controlFlags` (`int*`) with `SetControlFlag(int,int)` / `GetControlFlag(int)`.
  - Camera reset registry: `resetCameraArray` is a static `Jet::FastDynamicArray<Jet::DebugResetCamera*,13, Jet::Less<Jet::DebugResetCamera*,Jet::DebugResetCamera*>>`; `AddResetCameraObject` / `RemoveResetCameraObject` manage entries.
  - Class/object exports also include copy ctor, `operator=`, protected default ctor, protected virtual dtor, and `DebugAccess` vftable.
- `Jet::DebugResetCamera`: lightweight callback/interface-like type.
  - Exports: default/copy ctors, `operator=`, vftable, and two virtual `ResetCamera` overloads: `(Vector3 const&, Vector3 const&)` and `()`.
  - Function map shows `ResetCamera` body at `10001520` with **size 1** (`ret`-only stub in practice), consistent with a default no-op callback base.
- Plugin entry: `JetGetPluginsDebugInterface` exported (ordinal 38), indicating participation in the Jet plugin system (module discovery/registration via `JetGetPlugins<suffix>` as described in MODLOG for Kernel/plugin loading).

**Inferred role (bounded):** DebugInterface appears to be a shared, low-level debug-state/access module (flags/masks/control slots + camera-reset hook registry) that other debug or rendering/input/UI code can query/manipulate. `DebugSupport_7c` is a plausible related consumer/controller (large debug UI/control module in same group), but direct import/use relationships are **not proven** by the artifacts reviewed here (no import dump/xref evidence included). Treat cross-module usage as a coordination hypothesis to verify during implementation.

## 2. Inventory

### 2.1 Export inventory (38 exports)

From `D:\trainz12-decomp\exports\DebugInterface_7c.tsv`:

| Ord | RVA | Symbol | Notes |
|---:|---:|---|---|
| 1 | 0x1170 | `??0DebugAccess@Jet@@IAE@XZ` | protected default ctor |
| 2 | 0x1000 | `??0DebugAccess@Jet@@QAE@ABV01@@Z` | copy ctor |
| 3 | 0x1500 | `??0DebugResetCamera@Jet@@QAE@ABV01@@Z` | copy ctor |
| 4 | 0x1510 | `??0DebugResetCamera@Jet@@QAE@XZ` | default ctor |
| 5 | 0x1180 | `??1DebugAccess@Jet@@MAE@XZ` | protected virtual dtor |
| 6 | 0x1010 | `??4DebugAccess@Jet@@QAEAAV01@ABV01@@Z` | copy assignment |
| 7 | 0x1010 | `??4DebugResetCamera@Jet@@QAEAAV01@ABV01@@Z` | **same RVA as ord 6** |
| 8 | 0x2104 | `??_7DebugAccess@Jet@@6B@` | vftable data |
| 9 | 0x210c | `??_7DebugResetCamera@Jet@@6B@` | vftable data |
| 10 | 0x1190 | `?AddResetCameraObject@DebugAccess@Jet@@QAAXPAVDebugResetCamera@2@@Z` | `void __cdecl(DebugResetCamera*)` |
| 11 | 0x1160 | `?CheckBoundingVolumeMask@DebugAccess@Jet@@SA_NK@Z` | `static bool __cdecl(ulong)` |
| 12 | 0x1150 | `?GetBoundingVolumeMask@DebugAccess@Jet@@SAKXZ` | `static ulong __cdecl()` |
| 13 | 0x1210 | `?GetControlFlag@DebugAccess@Jet@@SAHH@Z` | `static int __cdecl(int)` |
| 14 | 0x1130 | `?GetGlobalBoundingVolumes@DebugAccess@Jet@@SA_NXZ` | `static bool __cdecl()` |
| 15 | 0x10d0 | `?GetPortalDebugMode@DebugAccess@Jet@@SA_NXZ` | `static bool __cdecl()` |
| 16 | 0x10a0 | `?GetPortalState@DebugAccess@Jet@@SA_NXZ` | `static bool __cdecl()` |
| 17 | 0x1100 | `?GetWireOnTexture@DebugAccess@Jet@@SA_NXZ` | `static bool __cdecl()` |
| 18 | 0x11c0 | `?RemoveResetCameraObject@DebugAccess@Jet@@QAAXPAVDebugResetCamera@2@@Z` | `void __cdecl(DebugResetCamera*)` |
| 19 | 0x1520 | `?ResetCamera@DebugResetCamera@Jet@@UAAXABVVector3@2@0@Z` | virtual `__cdecl(Vector3 const&, Vector3 const&)` |
| 20 | 0x1520 | `?ResetCamera@DebugResetCamera@Jet@@UAAXXZ` | virtual `__cdecl()` **same RVA as ord 19** |
| 21 | 0x1140 | `?SetBoundingVolumeMask@DebugAccess@Jet@@SAXK@Z` | `static void __cdecl(ulong)` |
| 22 | 0x11f0 | `?SetControlFlag@DebugAccess@Jet@@SAXHH@Z` | `static void __cdecl(int,int)` |
| 23 | 0x1110 | `?SetGlobalBoundingVolumes@DebugAccess@Jet@@SAX_N@Z` | `static void __cdecl(bool)` |
| 24 | 0x10b0 | `?SetPortalDebugMode@DebugAccess@Jet@@SAX_N@Z` | `static void __cdecl(bool)` |
| 25 | 0x1080 | `?SetPortalState@DebugAccess@Jet@@SAX_N@Z` | `static void __cdecl(bool)` |
| 26 | 0x10e0 | `?SetWireOnTexture@DebugAccess@Jet@@SAX_N@Z` | `static void __cdecl(bool)` |
| 27 | 0x1120 | `?ToggleGlobalBoundingVolumes@DebugAccess@Jet@@SA_NXZ` | `static bool __cdecl()` |
| 28 | 0x10c0 | `?TogglePortalDebugMode@DebugAccess@Jet@@SA_NXZ` | `static bool __cdecl()` |
| 29 | 0x1090 | `?TogglePortalState@DebugAccess@Jet@@SA_NXZ` | `static bool __cdecl()` |
| 30 | 0x10f0 | `?ToggleWireOnTexture@DebugAccess@Jet@@SA_NXZ` | `static bool __cdecl()` |
| 31 | 0x43b8 | `?boundingVolumeMask@DebugAccess@Jet@@1KA` | static data `unsigned long` |
| 32 | 0x43bc | `?controlFlags@DebugAccess@Jet@@1PAHA` | static data `int*` |
| 33 | 0x43b6 | `?globalBoundingVolumes@DebugAccess@Jet@@1_NA` | static data `bool` |
| 34 | 0x43b4 | `?portalDebugMode@DebugAccess@Jet@@1_NA` | static data `bool` |
| 35 | 0x404c | `?portalState@DebugAccess@Jet@@1_NA` | static data `bool` |
| 36 | 0x43e4 | `?resetCameraArray@DebugAccess@Jet@@1V?$FastDynamicArray@...@@2@A` | static data `FastDynamicArray<DebugResetCamera*,13,Less<...>>` |
| 37 | 0x43b5 | `?wireOnTexture@DebugAccess@Jet@@1_NA` | static data `bool` |
| 38 | 0x1520 | `JetGetPluginsDebugInterface` | plugin entry; **same RVA as ord 19/20** |

**Notable export aliasing (evidence-only):**
- `DebugAccess::operator=` and `DebugResetCamera::operator=` share RVA `0x1010`.
- Both `DebugResetCamera::ResetCamera` overloads and `JetGetPluginsDebugInterface` share RVA `0x1520`.
  - Interpretation must be confirmed from bytes/disassembly in implementation phase; possibilities include shared no-op/stub body or aliased implementation compatible with all three call sites/ABIs as emitted. Plan should treat `0x1520` semantics as a key verification point.

### 2.2 Function map inventory (`*.functions.tsv`)

`ghidra_maps/jet/debug/DebugInterface_7c.dll.functions.tsv` lists **61 rows** total (header excluded). Breakdown by evidence:

- **Named Jet API / class functions (26 rows):**
  - `Jet::DebugAccess`: `DebugAccess` copy/default ctors, `operator=`, `vf0` (89 bytes @ `10001020`), getters/setters/toggles for portal/wire/global-bounding/mask, `AddResetCameraObject`, `RemoveResetCameraObject`, `Set/GetControlFlag`, dtor.
  - `Jet::DebugResetCamera`: copy/default ctors, `ResetCamera` (1 byte @ `10001520`).
- **Unnamed/internal globals (9 rows):** `FUN_10001230` (138), `FUN_100012c0` (263), `FUN_100013d0` (103), `FUN_10001440` (19), `FUN_100014d0` (43), `FUN_100015d9` (24), `FUN_100015f1` (150), `FUN_10001687` (9), `FUN_10001690` (18).
  - These are not exported; likely internal helpers (e.g. array/list operations, reset dispatch, plugin init/registration glue, or compiler/runtime helpers). Exact roles unknown without xrefs/disassembly.
- **CRT/SEH/runtime/compiler support and thunks (26 rows):** `__ArrayUnwind`, `` `eh_vector_destructor_iterator' ``, `FID_conflict:\`vector_deleting_destructor'`, `__CRT_INIT@12`, `___DllMainCRTStartup`, `entry`, `___report_gsfailure`, `__SEH_prolog4/epilog4`, `__ValidateImageBase`, `__FindPESection`, `__IsNonwritableInCurrentImage`, `___security_init_cookie`, MSVCR80 imports/thunks (`terminate`, `_unlock`, `__dllonexit`, `_lock`, `initterm`, `initterm_e`, `_amsg_exit`, `__CppXcptFilter`, `_crt_debugger_hook`, `except_handler4_common`, `type_info::_type_info_dtor_internal_method`).

**Size/complexity signal:** exported behavior is mostly tiny accessors/toggles and small registry operations; bulk of non-CRT logic (by size) is concentrated in unnamed `FUN_10001230/12c0/13d0/15f1` and `DebugAccess::vf0`.

### 2.3 Vtable/name map (`vtnames_*.tsv`)

`ghidra_maps/jet/debug/vtnames_DebugInterface_7c.dll.tsv` provides:
- `10001020 -> Jet::DebugAccess::vf0`

This is consistent with a first/identified virtual slot body in the function map; slot semantics relative to the exported vftable objects (`??_7DebugAccess` @ 0x2104, `??_7DebugResetCamera` @ 0x210c) must be confirmed by reading vtable contents/xrefs during implementation (do not assume slot index/name beyond evidence).

## 3. Dependencies and integration points

### 3.1 Evidenced dependencies/contracts

- **Jet plugin system / module loading:** exports `JetGetPluginsDebugInterface` (ord 38). MODLOG documents the common pattern: libraries located/loaded by plugin system, entry resolved as `JetGetPlugins` + suffix and invoked; plugins registered via factories/groups in Kernel/plugin infrastructure. Exact registrations/types exposed by this module are **unknown from exports alone**.
- **FastDynamicArray ABI/layout contract:** `resetCameraArray` is exported as `Jet::FastDynamicArray<Jet::DebugResetCamera*,13, Jet::Less<Jet::DebugResetCamera*,Jet::DebugResetCamera*>>`.
  - MODLOG records that `FastDynamicArray<T,13>` was reconstructed in Kernel work (exported for `PluginConfig*` there): ring of 0x40-byte chunks `{next, prev, count, T items[13]}` on 32-bit, specific iterator/index semantics and operation behavior; "chunk format must stay exact" where arrays are ABI-visible.
  - DebugInterface uses a **different element type and comparator** but same template family/storage shape — implementation must remain binary-compatible for external observers of the exported object and for internal operations matching original behavior.
- **Types referenced by signature:** `Jet::Vector3` appears in `DebugResetCamera::ResetCamera(Vector3 const&, Vector3 const&)` virtual signature; actual Vector3 definition/layout is outside this module's exports and must come from shared Jet math headers/ABI used elsewhere in the project.
- **CRT/runtime:** function map shows linkage to MSVCR80-family CRT helpers/thunks (common for TS12 Jet modules per MODLOG). Build/integration must follow the module's existing CRT/linking conventions.

### 3.2 Likely but unproven consumers/integrators (verify before coding)

- **DebugSupport_7c:** same debug group, much larger API (`Jet::DebugControl`, debug consoles/viewports/texture view/profiler integration). Conceptually aligned with toggling debug state and camera resets, but direct dependency on DebugInterface symbols is **not evidenced** in reviewed artifacts.
- **RenderSystem / render debug paths:** flags like wire-on-texture, bounding volumes, portal debug state are plausible render-debug inputs; no direct xref/import evidence included here.
- **Input/UI/debug command paths:** `controlFlags` (indexed int slots) suggests externally driven control flags, but producer/consumer protocol and valid index range are unknown.

### 3.3 Runtime/load observations

- README: module **not started** (0/38 exports ours).
- MODLOG "What actually loads (main menu, 2026-10-18...)": **DebugInterface loaded at menu**; **DebugSupport not loaded at menu** (with note to re-check in other sessions/states). This implies menu-level in-game replacement is a relevant validation target, but feature-level behavior may require scenarios that exercise debug controls not active at plain menu.

## 4. Proposed implementation units / order (planning)

Goal order prioritizes ABI-visible state and low-risk scaffolding before behavior-sensitive internals. (No implementation performed in this task.)

### U0 — Proxy/forward scaffold
- Add module wiring in `src/jet/debug/DebugInterface_7c/` (or equivalent repo convention) with `implemented.txt` and `.def` generation following existing `jet_module` pattern in `src/jet/CMakeLists.txt`.
- Forward all 38 exports to `DebugInterface_7c_orig.dll` initially.
- **Gate:** in-game/menu load parity, JetLog comparison unchanged, module still co-loads with `_orig` as appropriate for proxy stage.

### U1 — ABI/data layout capture
- Define/export static data with exact types/layout semantics:
  - bool/ulong/int* statics (`portalState`, `portalDebugMode`, `globalBoundingVolumes`, `wireOnTexture`, `boundingVolumeMask`, `controlFlags`).
  - `resetCameraArray` object layout and comparator type identity for `FastDynamicArray<Jet::DebugResetCamera*,13,Less<...>>`.
- Ensure any template/container instantiation matches Kernel-established 32-bit `FastDynamicArray` chunk ABI and operation contracts where externally observable.
- **Risk focus:** comparator semantics and exported object storage (not just behavior).

### U2 — Simple statics API
- Implement getters/setters/toggles: `Get/Set/TogglePortalState`, `Get/Set/TogglePortalDebugMode`, `Get/Set/ToggleWireOnTexture`, `Get/Set/ToggleGlobalBoundingVolumes`, `Get/Set/CheckBoundingVolumeMask`, `Set/GetControlFlag`.
- Verify calling conventions exactly as exported (`static __cdecl` where demangled).
- **Gate:** targeted differential checks (if harness feasible) or focused in-game debug toggles once controllable.

### U3 — DebugResetCamera and reset-camera registry
- Implement `DebugResetCamera` ctors/`operator=`/vftable/virtual `ResetCamera` overloads (including no-op behavior) and `DebugAccess::{Add,Remove}ResetCameraObject`.
- Reproduce registry mutation semantics against `resetCameraArray` (insert/remove/search behavior, duplicates/order if any).
- Confirm shared `0x1520` body behavior for both `ResetCamera` overloads and, critically, its compatibility with `JetGetPluginsDebugInterface` export alias.

### U4 — Plugin entry and internal init/glue
- Implement `JetGetPluginsDebugInterface` (ord 38) with correct ABI/return/registration side effects for this module.
- Analyze/replace unnamed internal functions (`FUN_10001230`, `FUN_100012c0`, `FUN_100013d0`, `FUN_10001440`, `FUN_100014d0`, `FUN_100015d9`, `FUN_100015f1`, `FUN_10001687`, `FUN_10001690`) as needed; determine which are required for correct plugin registration, registry operations, or static initialization.
- **Gate:** plugin discovery/load behavior parity in game (and against any available probe patterns used by repo for other modules).

### U5 — Vtables/ctors/dtors/final cutover
- Finalize `DebugAccess` vftable/`vf0` semantics, protected ctor/dtor behavior, and any compiler-generated support that must remain ABI-compatible in our build.
- Remove forwarding once all 38 exports and required internals are covered; validate without `DebugInterface_7c_orig.dll` present.

## 5. Risks / unknowns

- **Aliased exports at `0x1520`:** `ResetCamera` (both overloads) and `JetGetPluginsDebugInterface` share one RVA. Must not assume trivial equivalence without byte-level/behavior confirmation; plugin entry semantics are especially sensitive.
- **Aliased assignment at `0x1010`:** `DebugAccess::operator=` and `DebugResetCamera::operator=` share implementation; copying/layout assumptions must match original.
- **`controlFlags` ownership/protocol unknown:** exported as `int*` with indexed get/set; allocation, bounds, initialization source, and thread/usage expectations are not evidenced.
- **`FastDynamicArray` comparator/template behavior:** `Less<DebugResetCamera*,DebugResetCamera*>` may imply pointer ordering; insert/remove behavior depends on exact comparator and array algorithm parity (Kernel reconstruction provides the template baseline but not this specialization's usage semantics).
- **Internal unnamed functions:** 9 non-exported `FUN_*` bodies may contain essential logic (registry ops, dispatch, plugin registration). Replacing exported wrappers only could break behavior if internals remain in `_orig` and reach old state/data — needs dependency/xref mapping.
- **Limited validation surface:** module is small and loaded at menu, but debug features may require specific runtime states/tools (debug UI, render debug modes, console/input paths) to exercise. Absence of regressions at menu/JetLog does not prove full correctness.
- **Consumer coupling uncertainty:** without import/xref inventory, risk remains that DebugSupport or rendering modules depend on subtle data/vtable details.

## 6. Cross-agent notes (DebugSupport and related modules)

- **DebugSupport_7c (`debug` group, 16 KB, 201 functions, 0/119 exports, not started, not loaded at menu in MODLOG snapshot):**
  - Evidence (`exports/DebugSupport_7c.tsv`, function/vt maps) shows major types: `Jet::DebugControl` (singleton create/destroy/get, extensive toggles/state for wireframe, normals, texture mode, v-sync, viewport/camera, render/portal stats, profiler hooks, `Update` returning `ResultClass`), `DebugTextureView`, `DebugViewPort`, `PortalListConsole`, `PortalStatsConsole`, `RenderStatsConsole`, OSD/profile/console-related vtable slots.
  - **Coordination point:** likely conceptual pairing with DebugInterface (debug state/access vs debug control/UI surfaces). Before implementing either, obtain/compare import xrefs to confirm whether DebugSupport reads/writes DebugInterface statics, calls `Add/RemoveResetCameraObject`, or uses `controlFlags`.
  - **Sequencing suggestion:** DebugInterface may be a small prerequisite/leaf in the debug stack, but only if its consumers are identified; otherwise parallel analysis is fine, implementation should be coordinated to avoid ABI mismatches.

- **Kernel_7c:**
  - Source-of-truth for reconstructed `FastDynamicArray` behavior/layout in this project (MODLOG: 32-bit chunk ring, 13 items/chunk, iterator/index semantics). DebugInterface’s exported `resetCameraArray` must conform to the same ABI expectations.

- **KernelStandard_7c / shared Jet types:**
  - Potential provider of allocators/containers/utilities used by FastDynamicArray internals and of shared types like `Vector3` used in DebugResetCamera signatures. Verify actual imports during implementation.

- **RenderSystem_7c (and render modules generally):**
  - **Coordination warning (per COORDINATION.md):** RenderSystem_7c has in-flight/uncommitted work owned elsewhere; read-only only. Debug flags exposed here (wire-on-texture, bounding volumes, portal debug) are plausible inputs to render-debug behavior — if/when render modules reference DebugInterface, align names/types but do not modify RenderSystem sources in this effort.

- **Input/UI modules:**
  - `controlFlags` and debug toggles may be driven by debug keybindings/UI; if InputSystem/Interface modules consume DebugInterface in future analysis, coordinate on flag index meanings once discovered.

## 7. Suggested verification approach (for future implementation owner)

- **Static/ABI checks:** export name+ordinal+RVA parity in proxy stage; vftable symbol addresses/slot contents; size/layout assertions for exported/embedded objects (`resetCameraArray`, statics) in 32-bit build.
- **Differential harness (if practical):** load `DebugInterface_7c.dll` and `DebugInterface_7c_orig.dll` in one 32-bit process and compare return values/state transitions for toggles, mask ops, control flag get/set, and camera object add/remove/reset dispatch across randomized and edge-case sequences. Account for/characterize alias behaviors at `0x1010` and `0x1520`.
- **In-game:** menu load with JetLog diff (module is loaded at menu per MODLOG). If debug features can be triggered (DebugSupport or debug console paths, once available/loaded), validate observable effects of flags and camera reset hooks.
- **Cross-module:** when DebugSupport is analyzed/implemented, run integration checks focused on any discovered DebugInterface call sites/state reads.

## 8. Open questions for decompilation/xref pass

1. What does `JetGetPluginsDebugInterface` (RVA `0x1520`, aliased) actually do/register? (return type/value, factory/group IDs, side effects)
2. What are the exact semantics of `DebugAccess::vf0` (`10001020`, 89 bytes) and the full slot layouts of both exported vftables?
3. What do `FUN_10001230/12c0/13d0/1440/14d0/15d9/15f1/1687/1690` implement, and which must move with the public API?
4. How is `controlFlags` allocated/sized/initialized, and what is the valid index domain?
5. What comparator/ordering behavior is required for `resetCameraArray` operations (and do Add/Remove rely on sorted uniqueness or pointer identity/order)?
6. Which modules import/use DebugInterface exports or statics in TS12 (DebugSupport, render modules, game exe, others)?
7. Do `ResetCamera` overloads intentionally share a no-op body in all cases, and is that same body safe/correct for the plugin-entry export alias?