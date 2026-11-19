# RenderSystem_7c agent report (read-only analysis)

Date: 2026-11-19  
Scope: `jet/graphics/RenderSystem_7c` (Trainz 2012 TS12, 32-bit drop-in Jet DLL; x64 later).  
This report is planning/analysis only. No source/build/docs/progress files outside this report were modified.

## 1) Current state (done / in-flight / remaining)

### Progress snapshot (from repo notes/maps, not re-counted by build)
- Exports: **2185/2687 ours, 502 forwarded** (`PORT_PLAN.md`, `notes/RenderSystem.md` tail, `implemented.txt` 2185 mapped lines).
- Export universe: `D:\trainz12-decomp\exports\RenderSystem_7c.tsv` has **2687** entries (header + 2687 rows shown as 2688 lines total in earlier readout).
- Function/vtable maps present locally: `ghidra_maps/jet/graphics/RenderSystem_7c.dll.functions.tsv` (212009 bytes), `ghidra_maps/jet/graphics/vtnames_RenderSystem_7c.dll.tsv` (20811 bytes).

### Unit status (per `notes/RenderSystem.md` / `PORT_PLAN.md`)
- **U0** forwarding proxy: done.
- **U1** value types & vertex formats: done (939 exports).
- **U4** modifiers: done.
- **U2** shader languages: done (U2a/b/c/d, incl. Gfx/Pre/Quick/Tex parser pieces).
- **U3** vertex programs & combiners: done (U3a1/a2, U3b1/b2).
- **U5** geometry & rendering plumbing: done (U5a/b1/b2/c).
- **U6** textures: done (U6a/b/c1/c2/d1/d2). Notes record caveats handled in tests (e.g. shared-display/data interactions, cache/statics ownership until U9; see `notes/RenderSystem.md` U6 sections).
- **U7** materials: **in progress / partially ported and differentially tested**.
- **U8** layers: pending.
- **U9** foundation (Display, RenderKernel, selectors/save buffers/contexts/resource types/free/plugin entry, etc.): pending.

### U7 done/in-flight (owned work, do not conflict)
Uncommitted/in-flight files reported in coordination context:
- Modified: `PORT_PLAN.md`, `README.md`, `notes/RenderSystem.md`, `src/jet/graphics/RenderSystem_7c/MaterialInstance.cpp`, `src/jet/graphics/RenderSystem_7c/implemented.txt`, `src/jet/tests/diff_rs_u7.cpp`, `tools/progress_exports.tsv`
- Untracked: `src/jet/graphics/RenderSystem_7c/MaterialLifetime.cpp`, `src/jet/graphics/RenderSystem_7c/MaterialStatics.cpp`, `tools/check_exports.py`

Implemented U7 areas (from `implemented.txt`, sources, `notes/RenderSystem.md`, `MODLOG.md` tail):
- Material manager/config/spec/resource core: `MaterialManager` ctor/dtor/vtable + Init/Create/Add/Remove/Find, `SplitDecoratedMaterialName` (`0x10035670`, `0x10035b80`), `MaterialEntries<T>`/`MaterialSpec`, `MaterialConfig` extractors (`0x100343a0`, `0x10034a30`, `0x10034350`, `0x10034960`, `0x10033d40`, `0x10034120`), `MaterialResource` ctor/copy/assign/dtor/vftable/LoadTxt/CreateInstance (`0x10035fb0`, `0x10035f00`, `0x10035f30`, `0x10035fe0`, `0x10071554`, `0x10036080`, `0x10036060`).
- Material instance/state helpers: `SetName/Rename/VerifyDecoratedName`, `SetTextureDefinition`, `Equals` (Material/Null), `InitInstance` (Material/Null), `GetRequestName`, accessors like `GetTextureManager/GetRenderKernel/GetNumPasses/IsLoaded`, texture-set unsupported/null paths, etc. (`MaterialInstance.cpp`).
- Null/material chunk + lighting: `NullMaterial::CreateGeometryChunk` (`0x10037a30`) with `GeometryChunkTemplate`/`GeometryChunkTemplate.cpp` (`0x10037bb0..0x100383a3`, vftable `0x10071834` context), `Material::SetLightingState` (`0x10032960`) (`MaterialFields.cpp`).
- Lifetime/factory/plugin/data surface (in-flight additions): `PluginFactoryBase<VMaterial>` copy/3-arg/assign/dtor, `MaterialPlugin` copy/3-arg, `Material`/`NullMaterial` ctors/dtors/vbase dtors, `Material::operator<`, `Material::CreateInstance` (UId/name overloads), `NullMaterial::CreateInstance/operator==`, exported vtables/vbtables/results/statics mirrors (`MaterialLifetime.cpp`, `MaterialStatics.cpp`). `MaterialStatics.cpp` copies mirrors from the original RS at load (including vftable locators/meta) and patches only selected NullMaterial slots to local thunks; `RsMatNextUniqueId` advances the **original module's** `?uniqueMatId@Material@Jet@@0KA` and mirrors the exported value (shared ownership caveat until U9).
- Differential harness: `src/jet/tests/diff_rs_u7.cpp` exercises split/create/config extraction/specs/material extraction/get/create-new, MaterialResource load/copy/assign, lighting, chunk, factory/plugin lifetimes, material/null ctors/dtors, CreateInstance(uid/name), operator<, NullMaterial CreateInstance/==, and data export mirrors (scenarios 0–19; env hooks `RS_U7_ECHO`, `RS_U7_DUMP`, `RS_U7_ONLY`, `RS_U7_SIDE`, `RS_U7_TRACE`). Notes/MODLOG report 0-mismatch runs at large round counts and green regressions on RS U6/U6b/U6c/U6c2/U6d suites plus menu checks in prior steps.

### Remaining in U7 (actionable remainder)

**Functions/members explicitly remaining per `notes/RenderSystem.md` (tail):**
- `Jet::Display::GetColorMaterialMode` — `0x10003c60` (14 bytes) — `ghidra_maps/...RenderSystem_7c.dll.functions.tsv`
- `Jet::Display::SetColorMaterialMode` — `0x10003c30` (48 bytes)
- `Jet::Display::SetMaterialProperties` — export present in `D:\trainz12-decomp\exports\RenderSystem_7c.tsv` as `?SetMaterialProperties@Display@Jet@@UAAXABUDisplayMaterial@2@@Z` (missing from `implemented.txt` scan in this environment); treat as U7/U9-bound Display member (depends on Display internals/vtable ownership)
- `Jet::RenderKernel::GetMaterialManager` — export `?GetMaterialManager@RenderKernel@Jet@@QAAPAVMaterialManager@2@@Z` missing from `implemented.txt`
- `Jet::RenderKernel::SetMaterialManager` — export `?SetMaterialManager@RenderKernel@Jet@@QAAXPAVMaterialManager@2@@Z` missing from `implemented.txt`

**Shared data statics kept original until U9 (exported now, but ownership/mirroring strategy remains transitional):**
- `?uniqueMatId@Material@Jet@@0KA` (`RsMatUniqueMatId`) — currently mirrored and advanced in original via `RsMatNextUniqueId` (not fully owned locally)
- `?results@Material@Jet@@2QBUResultTable@2@B`
- `?results@MaterialManager@Jet@@2QBUResultTable@2@B`
- `?DecoratedTypeToken@Material@Jet@@2VPString@2@A`
- `?STRING_NOT_FOUND@Material@Jet@@2VPString@2@A`
- `?PluginGroupName@MaterialPlugin@Jet@@2VPString@2@A`
- `?PluginGroupUID@MaterialPlugin@Jet@@2VUId@2@A`

(See `MaterialStatics.cpp`: copied from `RenderSystem_7c_orig.dll` at load; several vtables/vbtables also mirrored from original with selective slot patching.)

### Remaining outside U7 (RenderSystem-wide, high level)
- **U8 (layers/scene):** `Layer`, `LayerGroup`, `LayerManager`/related scene/resource-view items, `ClearLayer`, `Overlay`, `ElementLoader`, `SceneGraph`, `SystemResource*`, etc. (plan table ~144 exports / 35 functions; grep shows 60+ layer-related missing symbols including `Layer/LayerGroup/ClearLayer/ElementLoader/Overlay` family). Function map includes e.g. `Jet::Layer` at `0x1000ac40`, `0x1000acd0`, `0x1000ad70`, etc.
- **U9 (foundation):** `Display` (large, ~57 KB noted), `DisplaySelector`, `DisplaySaveBuffer`, render/context classes (`NonRenderContext`, `SharedContext`, `ApiDataBase`), `RenderKernel`, resource type registration/free functions/plugin entry (`JetGetPluginsRenderSystem`), and many geometry/display/vertex-program-manager/display-texture/shared-texture/display-selector related symbols still forwarded (large portion of remaining 502). Also final ownership of shared module statics/data/vtables currently mirrored from original.
- **Other still-missing clusters (from export diff scan):** streamed vertex templates, geometry buffer/vertex buffer manager pieces, many `DisplayTexture*`/`SharedTextureCollection*`/`VertexProgram*`/`VertexProgramManager*`/`VertexType`-registry API/display-side pieces, `Display` mode helpers, etc. (treat as U8/U9/foundation-adjacent per plan/notes).

## 2) Remaining units/subsystems and suggested order

> Respect in-flight U7 ownership: do **not** modify current uncommitted/untracked U7 files or rework already-landed U7 behavior. Suggested work assumes coordination/lock before touching U7 remainder.

### 2.1 U7 remainder (close-out)
**Goal:** finish the remaining Material-adjacent API surface without taking U9 foundation ownership prematurely.

Suggested order:
1. **`RenderKernel::{Get,Set}MaterialManager`** (exports present, missing in impl)
   - Key symbols: `?GetMaterialManager@RenderKernel@Jet@@QAAPAVMaterialManager@2@@Z`, `?SetMaterialManager@RenderKernel@Jet@@QAAXPAVMaterialManager@2@@Z`
   - Rationale: isolated pointer accessors; needed by material/material-manager users. Verify exact field offset in `RenderKernel` layout (currently opaque in headers; cross-check decomp/Ghidra/function map when allowed) to avoid layout guesswork.
   - Risk: medium if offset/vtable context assumed incorrectly while `RenderKernel` is still mostly original-owned.

2. **`Display::{Get,Set}ColorMaterialMode`** (`0x10003c60`, `0x10003c30`)
   - Rationale: small, explicitly identified in notes; likely interacts with display state block/color-material state used by material/lighting paths.
   - Risk: medium (state-block field/enum semantics must match original; `SetLightingState` already depends on display state layout/slots).

3. **`Display::SetMaterialProperties`** (`?SetMaterialProperties@Display@Jet@@UAAXABUDisplayMaterial@2@@Z`)
   - Rationale: completes the noted 5-member U7 remainder.
   - Risk: **high** relative to others because it is a Display virtual/member in a largely unported foundation class; may require careful ABI/vtable alignment or temporary thunking strategy consistent with U9 plans.

4. **Do not "finish" the 7 shared data statics in isolation**
   - Keep them original/mirrored until **U9** per existing design (`MaterialStatics.cpp`, notes). Premature ownership can break cross-module identity/RTTI/vtable slot expectations and `uniqueMatId` sequencing with original code still in play.

**U7 key files/classes/addresses (already implemented, for reference):**
- `src/jet/graphics/RenderSystem_7c/MaterialManager.cpp`, `MaterialConfigExtract.cpp`, `MaterialSpec.cpp`, `MaterialResource.cpp`, `MaterialInstance.cpp`, `MaterialFields.cpp`, `NullMaterialFields.cpp`, `GeometryChunkTemplate.cpp`, `MaterialLifetime.cpp`, `MaterialStatics.cpp`
- Headers: `src/jet/include/jet/Material.h`, `RenderTypes.h` (`DisplayMaterial`/light helpers), `RenderSystem.h` (partial), `Geometry.h`/chunk types as relevant
- Tests: `src/jet/tests/diff_rs_u7.cpp`
- Notable addrs: split `0x10035b80`, create `0x10035670`, config extractors `0x100343a0/0x10034a30/0x10034350/0x10034960/0x10033d40/0x10034120`, resource `0x10035f00/0x10035f30/0x10035fb0/0x10035fe0/0x10036060/0x10036080`, chunk `0x10037a30`, lighting `0x10032960`, display color-material mode `0x10003c30/0x10003c60`.

### 2.2 U8 (layers / scene integration) — after U7 close-out is stable
**Scope hints:** `Layer`, `LayerGroup`, `LayerManager`, `ClearLayer`, `Overlay`, `ElementLoader`, scene/resource-view classes (`notes/RenderSystem.md` U8 table; map shows `Layer` at `0x1000ac40+`).

Suggested order:
1. **Layer base/value semantics first** (`Layer` ctor/copy/dtor, visibility/name/priority/group linkage, update/render virtual dispatch scaffolding, collision helpers)
   - Risk: virtual layout and flag semantics; `Render2DCore` (already ported/verified earlier) interacts with layer/display patterns.
2. **`LayerGroup` container/ordering/view-rect operations** (add/find/z-order/group rect transforms)
3. **`ClearLayer` / `Overlay` specializations**
4. **`ElementLoader` / `LayerManager` / scene-graph-related pieces**
5. Integrate with display/render-kernel call sites only as those dependencies become owned (some may defer to U9).

Dependencies/notes: layers depend on `RenderKernel`/`Display`, `PString`, containers, and possibly texture/material/render-2D conventions; verify TS12 vtable slot order/layout before replacing exports.

### 2.3 U9 (foundation) — last, highest coupling/risk
**Scope:** `Display` (57 KB), `DisplaySelector`, `DisplaySaveBuffer`, `NonRenderContext`/`SharedContext`/`ApiDataBase`, `RenderKernel`, resource type registration, plugin entry/free functions, and ownership handoff of module statics/vtables/data currently mirrored from original.

Suggested order:
1. **Inventory and split into sub-slices** by dependency/data ownership (state blocks, format/texture creation paths, vertex/program/buffer manager hooks, selectors/save buffers, kernel init/options).
2. **`RenderKernel` core/init/options** (feeds texture/material/display subsystems) before/alongside the specific Display slices it constructs.
3. **Display state/capability/query and material/color/material-property paths** (ties back to U7 remainder semantics).
4. **Display resource-creation and manager wiring** (`CreateTexture`, vertex/index buffer managers, declarations, render targets, shared contexts) — coordinate with U5/U6 assumptions.
5. **Static/data/vtable ownership migration**: stop mirroring from original for `uniqueMatId`, result tables, plugin/material strings/UIDs, and class vtables/vbtables when safe.
6. **`JetGetPluginsRenderSystem` / resource type registration / final glue**.

Rationale: U9 owns the foundations many earlier units currently reach via original vtables/slots/statics; premature partial moves risk ABI/layout mismatches.

## 3) Dependencies

### What RenderSystem_7c needs from elsewhere
- **Kernel/utility/runtime:** `KernelStandard_7c` (`Alloc`, `PString`/`PStringCache`, `Log`, containers, `ConfigData`/`ConfigParser`, etc.), `KernelResource_7c` (`Resource`, resource DB/system/time hooks), `KernelSystem`/OS option plumbing via `RenderKernel` (when ported).
- **Plugin/type system:** `PluginBase/PluginType/PluginInstance` lookup/register APIs (`LookupPluginType` by UId/name, registration/unregistration paths) — heavily used by material/plugin factories and CreateInstance flows.
- **Memory/layout conventions:** MSVC x86 thiscall/cdecl/fastcall patterns as in original, vftable layout incl. RTTI complete-object locator word before first slot (`MaterialStatics.cpp` comment), virtual-base/vbtable/vtordisp handling for `Material`/`NullMaterial`.
- **Original module during transition (current design):** `RenderSystem_7c_orig.dll` supplies mirrored statics/data/vtable bytes and shared `uniqueMatId` advancement until U9; many unported behaviors still thunk/forward to original exports.

### What other modules need from RenderSystem_7c (dependents / integration consumers)
- **Direct/in-process consumers (per plan/notes context):** `trainz.exe`, `Render2DCore_7c` (already ported), `Render3DCore_7c`, `Render3DFoundation_7c`, `Render3DExtension_7c`, `RenderOpenGL_7c`, `RenderDirectX9_7c` (pending port), `WaterSystem_7c`, `InterfaceSystem_7c`, and various subsystems using materials/textures/geometry/layers/display APIs.
- **Critical contracts to preserve:** exported symbol names/ordinals/mangling, class layouts/sizes/offsets (e.g. `MaterialManager` 0x20, `MaterialSpec` 0x6c, `MaterialResource` 0x64 per `jet/Material.h`), vtable slot order/counts, calling conventions and hidden args (sret/virtual-base flags), refcount semantics, result-table pointer identities/values as expected, and stable behavior of material/config parsing/lookup/creation/lighting/chunk generation.

## 4) Testing and verification considerations

### Existing/available harnesses relevant to RenderSystem
- **Differential tests (ours vs `*_orig.dll` in-process):** `src/jet/tests/diff_rs_u7.cpp` (current U7), and prior RS suites noted as green in MODLOG (`diff_rs_u6d`, `diff_rs_u6c2`, `diff_rs_u6c`, `diff_rs_u6b`, `diff_rs_u6`, `diff_rs_u5*`, `diff_rs_u4`, `diff_rs_u1` etc.). Use the same side-by-side, import-hook/log-normalization methodology for any new U7/U8/U9 work.
- **Build/export checks:** `tools/check_thunks.py`, `tools/progress_exports.tsv`/`tools/progress.py` workflow (mentioned in MODLOG), `compare_vtables.py` for vftable parity/status visibility.
- **Game/runtime checks:** 32-bit laboratory build launched to menu / clean exit with JetLog comparison as recorded in MODLOG/notes; targeted scene/asset checks needed for material/texture/layer/render changes.

### Practical verification guidance for remaining work
- **U7 remainder:** extend `diff_rs_u7` or add narrowly-scoped tests for `Display` color-material mode/material-properties and `RenderKernel` material-manager accessors once safe call contexts/stand-ins exist (be careful: `Display`/`RenderKernel` are foundation-heavy; tests may need scripted state blocks/vtables matching originals).
- **U8:** follow Render2DCore/layer interaction expectations; compare against original for vtable dispatch, rect/group math, z-order, visibility/collision, and lifecycle/refcount behavior.
- **U9:** require staged validation (per-subsystem diffs where feasible, vtable comparisons, menu/game smoke, renderer-specific checks). `RenderOpenGL` is the live renderer path to prioritize; `RenderDirectX9` remains relevant for launcher-selected backend.
- **Watch for known hazards from MODLOG:** heap-garbage/uninitialized fields visible only on recycled memory, hidden-arg/return ABI shape (member vs free function returns for 8-byte structs, EAX class-return pointers), x87 float/SNaN normalization, shared/static state and refcount/node ownership, and temp-name/overflow or other preserved original bugs—tests must match original behavior, not "fixed" behavior, unless intentionally diverging later.

### Blockers / constraints
- **Coordination:** RenderSystem_7c is owned/in-flight with uncommitted work; avoid conflicting edits/tests/doc rewrites outside this report until coordinated.
- **Transitional ownership:** shared statics/data/vtables mirrored from original and `uniqueMatId` advanced in original module (`MaterialStatics.cpp`) block full local ownership until U9 migration.
- **Foundation gaps:** remaining U7 members touch `Display`/`RenderKernel`; `Display` is mostly unported, increasing risk and possibly requiring U9-aligned scaffolding.
- **No build/test execution performed for this report** (read-only analysis). Current pass/fail status for modified in-flight files should be confirmed by the owning agent.

## 5) Cross-agent notes

### WaterSystem
- RenderSystem provides material/texture/geometry/render-state primitives and (later) display/kernel behavior that water/render passes may consume. `WaterSystem_7c` target has had pre-existing build issues noted in MODLOG; coordinate interface/layout expectations but do not assume RS U7 remainder unblocks WaterSystem alone.
- Avoid changing material/layout/vtable contracts or shared static semantics that `WaterSystem` may observe through original/ported boundaries.

### Render3DCore / Render3DFoundation / Render3DExtension
- These modules sit above RenderSystem (textures, materials, geometry chunks/buffers, vertex types/programs, render kernel/display services). Preserve all exported ABI/layout behavior; U9 work (Display/RenderKernel/statics ownership) is especially sensitive for them.
- When porting U8/U9, validate against 3D render paths where feasible (scene/material/geometry usage) since `notes/RenderSystem.md` states many modules and `trainz.exe` stand on RenderSystem.

### RenderOpenGL / RenderDirectX
- Backend DLLs implement `Display`/device behavior consumed via RenderSystem contracts. `RenderOpenGL` is the live/priority backend (modernization base); `RenderDirectX9_7c` is the launcher-selectable alternative and must remain compatible.
- U9 Display/RenderKernel changes (state blocks, texture/buffer/program manager hooks, render-target/context behavior, material property application) must be coordinated so OpenGL and DX9 backends continue to see expected call sequences and data.

### InterfaceSystem
- Likely consumes layers/UI/render-2D-facing RenderSystem APIs (`Layer`/`LayerGroup`/overlays, textures/materials as needed). Stabilize U8 APIs/layouts and avoid regressions in Render2DCore-facing behavior.
- Coordinate sequencing so interface-visible rendering remains intact through 32-bit menu/game verification.

### General cross-agent guidance
- Do not rename/rebase symbols, ordinals, or TS12-quirk behavior relied upon by differential parity. Prefer preserving original bugs/quirks where tests/game expect them.
- Shared module state currently mirrored from `RenderSystem_7c_orig.dll` (vftables/statics/`uniqueMatId`) is a coupling point until U9; communicate before changing ownership/lifecycle.
- Use `notes/RenderSystem.md`, `implemented.txt`, `ghidra_maps/jet/graphics/RenderSystem_7c.dll.functions.tsv`, `vtnames_RenderSystem_7c.dll.tsv`, and `D:\trainz12-decomp\exports\RenderSystem_7c.tsv` as primary references when assigning/verifying remaining symbols.
