# Consolidated plan — unfinished Jet libraries (analysis sweep)

**Date:** 2026-11-19  
**Scope:** 14 unfinished Jet modules analyzed in parallel by read-only agents (no source/build/doc/progress edits by agents except `notes/agents/*` reports). This document consolidates their findings into a dependency-aware order and parallelization guidance.  
**Reports:** one per module in `trainz12-x64/notes/agents/<Module>.md` (all 14 present).  
**Coordination:** `notes/agents/COORDINATION.md`, status log `notes/agents/STATUS.md`.

## 1. Snapshot (all 14 modules)

| Module | Group | Status (README) | Key deps (evidenced/reported) | Main blockers/risks | Recommended posture |
|---|---|---:|---|---|---|
| `RenderSystem_7c` | graphics | partial 2185/2687, WIP U7 (~502 fwd) | KernelStandard/KernelResource/plugin system; transitional mirrors from original RS | **In-flight uncommitted U7 work (owned)**; shared statics/vtables/`uniqueMatId` mirrored until U9; ABI/layout/x87 quirks | Finish U7 remainder → U8 → U9 (staged), **single owner**, do not disturb current WIP |
| `WaterSystem_7c` | graphics | partial 152/490 (U1–U6 done) | RenderSystem material/shader/texture/geometry/display contracts (raw slot usage in done units), Kernel* | **Pre-existing build break (VS18):** `JET_LAYOUT32(sizeof(PhysicsWaterGeometry)==0x7c)` in `WaterGeometry.h:89`, follow-on errors in `tests/diff_ws_u1.cpp`; RS in-flight; U8 shader-heavy | Fix build first (coordinated), then U7→U8→U9→U10 aligned with RS |
| `Render3DCore_7c` | graphics | 0/2500 | RenderSystem (strong), Kernel*; neighbor 3DFoundation/3DExtension/backends | Very large surface, 802 unattributed `Global` funcs, templates/vtables, float determinism, static ownership | Defer implementation start until RS contracts stabilize enough; plan U0→…→U11 per report |
| `Render3DFoundation_7c` | graphics | 0/1084 | RenderSystem, Kernel*; relations to 3DCore/3DExtension/backends/DSMoviePlayer (verify) | 623 unattributed globals, complex layouts/MI/vbtable, float sensitivity, movie/platform uncertainty | After/alongside 3DCore planning with RS alignment; proxy/bootstrap early, stage by subsystem |
| `Render3DExtension_7c` | graphics | 0/186 | **Render3DCore ~134 imports, RenderSystem ~111 imports** (MODLOG); particle/spline/layout ties to 3DCore | Depends on not-started 3DCore and in-flight RS; multi-base vtable/ABI; often not loaded at menu | Later in render stack; start after RS/3DCore contracts clearer |
| `RenderOpenGL_7c` | graphics | 0/51 (live renderer) | RenderSystem (178 imports, Display/U9 coupling), CgFX (`cg.dll`/`cgGL.dll`), WGL/GL; peer DX9 | Large internal/`Global` surface, GL/WGL state, Cg 2.2 now→3.1 x64, extension/cap plugins, RS in-flight | **Priority backend** for 32-bit menu/path; plan from proxy+contract capture → display/texture → Cg path, coordinated with RS U9 |
| `RenderDirectX9_7c` | graphics | 0/41 | RenderSystem Display/texture/program/buffer/combiner contracts; peer OpenGL; D3D9/D3DX | `d3dx9_42` now, x64 `d3dx9_43` or remove D3DX; Cg linkage/behavior; device-lost/reset/COM/state, RS in-flight | Implement after/parallel-to OpenGL contract understanding; useful for launcher-DX checks |
| `InputDirectX_7c` | input | 0/1 (`JetGetPluginsInputDirectX`) | `InputSystem_7c` (ForceEffect/contracts, device/plugin integration), `InputWin32_7c` interaction (kbd/mouse), DirectInput/COM | DI/COM/FF semantics, struct/handle/ABI for x64, enumeration/acquisition quirks | Good **independent-ish** candidate once InputSystem contracts confirmed; validate beyond menu if needed |
| `InterfaceSystem_7c` | ui | 0/2022 (3430 funcs) | Kernel*, Render2DCore/RenderSystem, InputSystem/InputUIExtension, **InterfaceResources_7c** | Huge polymorphic ABI/layout/vtable surface, stateful UI/dialog/focus/DnD, RS in-flight, InterfaceResources missing | Large multi-unit effort; start with ABI/layout capture + foundations only when deps/contracts stable |
| `InterfaceResources_7c` | ui | 0/169 | KernelResource/KernelResources/KernelStandard, Render2DCore; primary consumer InterfaceSystem (32-symbol import noted) | ABI/layout/vtable, font parsing/effects, bitmap/resource ownership, tight coupling to InterfaceSystem | Can be staged early relative to UI if ABI boundary with InterfaceSystem agreed; coordinate ownership/types |
| `DebugInterface_7c` | debug | 0/38 (loaded at menu) | Kernel/plugin/refcount patterns; shared debug-state/flags/camera-reset registry | Small ABI/data layout/vtable details must be exact; consumers (DebugSupport/others) partly unproven | **Low-risk, small**; good early win and prerequisite/contract for DebugSupport |
| `DebugSupport_7c` | debug | 0/119 (not loaded at menu) | `DebugInterface_7c`, Render*/UI/Input/Kernel subsystems (per exports/names) | Conditional/session load context not fully reproduced; broad subsystem coupling | After `DebugInterface`; reproduce load/trigger before deep implementation |
| `Lexer_7c` | not_loaded | 0/185 | KernelStandard-like core types/utilities (typical) | **No evidenced runtime use/load** (docs: not seen loaded / "nothing references it") | **Defer** until new evidence of use appears |
| `DSMoviePlayer_7c` | not_loaded | 0/1 (`JetGetPluginsDSMoviePlayer`) | DirectShow/COM; thunk to `Render3DFoundation_7c::MoviePlayer::SetLooping` (contract risk), movie stack | **Not loaded at menu**; runtime trigger unknown; platform/codec/ABI concerns for x64 | **Defer** until load trigger and `MoviePlayer` contract confirmed |

> Counts/byte/function figures above are as reported in module reports/maps/exports; treat unattributed/`Global` function counts and dependency import counts as planning signals requiring decomp/xref/import-dump confirmation where noted.

## 2. Dependency view (consolidated)

### 2.1 Foundations (already in good shape / external to this sweep)
- Core/kernel modules largely done (`Kernel_7c`, `KernelStandard_7c`, `KernelResource(s)_7c`, platform layers, etc. per README context). `Render2DCore_7c` done. `InputSystem_7c`/`InputWin32_7c` done.
- This sweep focuses on the 14 unfinished Jet libraries; they sit above these foundations.

### 2.2 Critical render spine (highest coupling)
1. **`RenderSystem_7c`** — core render/material/display/kernel/layer/foundation contracts consumed by almost all render/UI/water work. **Currently in-flight (U7).**
2. **3D stack:** `Render3DCore_7c` →/↔ `Render3DFoundation_7c` → `Render3DExtension_7c` (with reported RS/3DCore coupling; exact boundaries to confirm).
3. **Backends:** `RenderOpenGL_7c` (live/priority) and `RenderDirectX9_7c` implement Display/device contracts against RenderSystem.
4. **Consumers/features:** `WaterSystem_7c` depends heavily on RenderSystem (and ultimately render path); UI rendering touches RenderSystem/Render2DCore.

### 2.3 UI/Input/Debug side
- **UI:** `InterfaceResources_7c` ↔ `InterfaceSystem_7c` (boundary/ownership must be explicit) on top of KernelResource(s)/Render2DCore/RenderSystem/Input.
- **Input:** `InputDirectX_7c` plugs into `InputSystem_7c` (+ `InputWin32_7c` coexistence).
- **Debug:** `DebugInterface_7c` (small, loaded) → `DebugSupport_7c` (conditional), both coupled to broader render/UI/input as applicable.

### 2.4 Not-loaded / low-evidence
- `Lexer_7c`, `DSMoviePlayer_7c`: no confirmed menu load/use; defer pending evidence.

## 3. Cross-cutting blockers and constraints

1. **RenderSystem U7 in-flight / uncommitted (owned):** modified `PORT_PLAN.md`, `README.md`, `notes/RenderSystem.md`, `src/jet/graphics/RenderSystem_7c/{MaterialInstance.cpp,implemented.txt}`, `src/jet/tests/diff_rs_u7.cpp`, `tools/progress_exports.tsv`; untracked `MaterialLifetime.cpp`, `MaterialStatics.cpp`, `tools/check_exports.py`. **Do not rework or parallel-edit** these/shared RS files until claimed/committed/stabilized.
2. **WaterSystem pre-existing build break (VS18):** `JET_LAYOUT32(sizeof(PhysicsWaterGeometry)==0x7c)` failure and `diff_ws_u1.cpp` follow-on errors — blocks compile/test iteration for WaterSystem implementation.
3. **Shared-state/static ownership:** transitional mirroring from original DLLs/vtables/statics (esp. RenderSystem, WaterSystem notes) requires careful unit/ownership sequencing.
4. **ABI/layout/vtable/x87-float sensitivity:** pervasive across render/UI/debug modules; preserve original behavior/quirks for drop-in parity.
5. **x64/modernization watchpoints (future, but design-aware now):** D3DX 9.42→43/remove, Cg 2.2→3.1/GLSL path, DirectInput/COM/handle sizes, pointer/packing assumptions.
6. **No confirmed runtime triggers:** `Lexer_7c`, `DSMoviePlayer_7c`, and (for full exercise) `DebugSupport_7c`, `Render3DExtension_7c` may need in-session/driving triggers.
7. **Coordination rule:** 1 owner = 1 module/unit claim in `COORDINATION.md`, no parallel builds/tests touching same outputs, no shared-file edits without claim.

## 4. Recommended execution order (waves) and parallelization

Parallelization means **different modules/units with disjoint file ownership**, only when their required contracts are stable enough. Keep one owner per module/unit and update claims.

### Wave 0 — unblock / stabilize (prerequisites)
- **RS U7 owner:** bring current RenderSystem U7 WIP to a stable checkpoint (tests/build status known), reconcile docs/progress, and commit or clearly freeze/claim before others depend on new behavior. (Coordination action, not performed by this analysis sweep.)
- **WaterSystem build fix (separate claim):** diagnose/fix pre-existing VS18 layout/API drift so `WaterSystem_7c` can build/test again. Keep isolated from RS WIP.

### Wave 1 — low-coupling, small wins (can run in parallel with each other once claimed)
- `DebugInterface_7c` (small, loaded, clear API) — establishes debug-state contract.
- `InputDirectX_7c` (1 export, but non-trivial internals) — independent of render stack except shared process context; aligns to stable InputSystem/InputWin32 contracts.
- `InterfaceResources_7c` **or** early ABI/layout capture for it — can proceed if boundary with InterfaceSystem is documented and files are disjoint (no `InterfaceSystem` implementation yet).

### Wave 2 — render stack foundations (largely sequential / tightly coordinated)
- `RenderSystem_7c` U7 remainder → U8 → U9 (single-owner sequence). U9 is the key contract for backends/3D/water/UI.
- In parallel **only** for analysis/prototyping in isolated branches/notes or disjoint scaffolds: `RenderOpenGL_7c` contract capture (PE imports, vslots/layouts, Global attribution) without assuming unstable RS internals.
- `RenderDirectX9_7c` contract capture similarly, coordinated with OpenGL/RS.

### Wave 3 — 3D feature stack (after/with sufficient RS stability)
- `Render3DCore_7c` staged units (proxy/bootstrap → math/leaf → …) — high effort, own exclusively.
- `Render3DFoundation_7c` staged alongside/after per agreed boundaries with 3DCore/RS.
- `Render3DExtension_7c` later (depends on 3DCore + RS).

### Wave 4 — backends and dependent features (contract-driven)
- `RenderOpenGL_7c` implementation units aligned to RS Display/U9 (priority live path/menu checks).
- `RenderDirectX9_7c` in parallel where Display contract is shared but implementation files are separate.
- `WaterSystem_7c` resume U7→U10 after build fix and with RS material/shader/texture/display contracts stable enough.

### Wave 5 — UI and conditional debug (large, dependency-heavy)
- `InterfaceSystem_7c` in staged units (U0 scaffold → Region/Base → State/Canvas → Desktop → Dialogs → widgets/OSD…) with explicit coordination with `InterfaceResources_7c` and stable Render2DCore/RenderSystem/Input contracts.
- `DebugSupport_7c` after `DebugInterface_7c` and once a reproducible load/trigger context is known.

### Wave 6 — defer unless evidenced
- `Lexer_7c`: implement only if runtime references/load are discovered.
- `DSMoviePlayer_7c`: implement only after confirming load trigger/session context and `Render3DFoundation::MoviePlayer` / DS/COM integration contract.

## 5. Per-module next actions (from reports, consolidated)

| Module | Immediate next actions (before/during implementation) |
|---|---|
| `RenderSystem_7c` | Stabilize/claim WIP; complete U7 remainder carefully (kernel material-manager accessors, display color-material mode, `SetMaterialProperties` with layout care); keep shared statics original until U9; extend diffs/vtable checks. |
| `WaterSystem_7c` | **Fix build break**; confirm RS slot/layout assumptions; proceed U7 base+plugins → U8 materials (split by class) → U9 tiles/water/surface → U10 managers+`JetGetPlugins`; add per-unit diff/harness coverage. |
| `Render3DCore_7c` | Dump/confirm PE imports and consumers; capture class layouts/vtable/slot meanings for core types; start U0 proxy scaffold only under claim when ready. |
| `Render3DFoundation_7c` | Attribute key globals, confirm relationships/imports with 3DCore/3DExtension/movie, plan data/asset needs (animations/meshes/shadows/projectors). |
| `Render3DExtension_7c` | Confirm PE imports and runtime exercise context; coordinate particle/spline/layouts with 3DCore and material/plugin contracts with RS. |
| `RenderOpenGL_7c` | Confirm imports/CG/WGL deps, map Display/texture vslots vs RS, start forwarding proxy + plugin/descriptor/context units; keep Cg path isolated/validated. |
| `RenderDirectX9_7c` | Confirm `d3d9`/`d3dx9_*`/Cg linkage, decide D3DX strategy for x64, align DisplayTexture/Display contracts with RS/OpenGL. |
| `InputDirectX_7c` | Capture DirectInput interfaces/vtbl expectations and device/FF layouts; implement plugin/manager → keyboard/mouse coexistence checks → joysticks/wheels → force feedback. |
| `InterfaceSystem_7c` | Decomp/layout pass for Region/Base/State/Desktop/DialogResource; export-to-unit mapping; define ABI boundaries with InterfaceResources/RenderSystem/Input before coding. |
| `InterfaceResources_7c` | Inventory resource formats/load paths and vtable/type layouts; agree boundary with InterfaceSystem (parsing/caching/GPU prep/lifetime). |
| `DebugInterface_7c` | Capture exact data layout/registry semantics and plugin init; proxy/forward then implement statics/reset/entry/vtables. |
| `DebugSupport_7c` | Identify how/when loaded (session/debug flags/tools); map `DebugControl` and debug-render dependencies on DebugInterface/RS/UI/Input. |
| `Lexer_7c` | No implementation planned; revisit only with concrete xref/runtime evidence. |
| `DSMoviePlayer_7c` | Confirm runtime trigger and `Render3DFoundation::MoviePlayer` ABI/behavior; defer implementation until evidenced/needed. |

## 6. Suggested claims model (for implementation agents)

- **One claim = one module or a clearly bounded unit** recorded in `COORDINATION.md` with owner, scope (files/units), status, and notes.
- Avoid parallel edits to: `README.md`, `PORT_PLAN.md`, `MODLOG.md`, `tools/progress_exports.tsv`, shared `CMakeLists.txt`/test wiring, and any module already claimed/in-flight.
- Prefer writing new code/tests in module-owned paths; integrate/wire shared files in a serialized follow-up step by the claim owner.
- Append completion/results to `STATUS.md` (and update claims) when a unit/analysis task finishes.

## 7. Outputs of this sweep

- Per-module reports: `notes/agents/*_7c.md` (14 files) with inventories, dependencies, proposed unit order, risks, and cross-agent notes.
- This consolidation: `notes/agents/CONSOLIDATED_PLAN.md`.
- Coordination/status maintained in `notes/agents/COORDINATION.md` and `notes/agents/STATUS.md`.

**Overall recommendation:** stabilize/commit RenderSystem U7 and fix the WaterSystem build blocker first; pursue Wave 1 small/independent wins in parallel where safe; treat the 3D/UI stacks as staged, contract-driven programs with strict ownership; defer `Lexer_7c` and `DSMoviePlayer_7c` until runtime evidence requires them.
