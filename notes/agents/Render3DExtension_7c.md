# Render3DExtension_7c.dll — analysis & planning report (read-only)

**Module:** `Render3DExtension_7c` (group: graphics)  
**Repo:** `D:\universal modder\trainz12-x64`  
**Status (per `README.md` 2026-11-19):** 38 KB, 326 functions, 0/186 exports, ⬜ not started. Notes: “not loaded at menu but part of render stack consideration”.

**Evidence sources read:** `README.md`, `notes/RenderSystem.md`, `notes/agents/COORDINATION.md`, `notes/agents/STATUS.md`, `MODLOG.md` (Render3DExtension mentions), `PORT_PLAN.md` (no direct mention found), `ghidra_maps/jet/graphics/Render3DExtension_7c.dll.functions.tsv`, `ghidra_maps/jet/graphics/vtnames_Render3DExtension_7c.dll.tsv`, `D:\trainz12-decomp\exports\Render3DExtension_7c.tsv`. No `src/jet/graphics/Render3DExtension_7c/` exists yet.

## 1. Overview / role

Evidence from exports and function names indicates this is a render-side extension module providing at least three feature clusters:

1. **Decal system** — base `Decal` plus `DecalCache`, `DecalParticle`, `PointDecal`, `ProjectedDecal`.
   - Geometry/content operations: `AlignGeometryToPoints/Normals`, `MoveGeometry`, `ClipGeometryToPolygon/Plane`, `SetupGeometryIndices`, `MergeDecalGeometry`, `NormalizeChunk`.
   - Runtime: material/geometry/light binding (`Get/SetMaterial`, `Get/SetGeometryChunk`, `Get/SetDecalLights`, `Get/SetTargetTransparent`), `Cull`, `Render`, `InitInstance`, cache `AddDecal/Begin/End/Reset`.
   - Export surface includes many decal/particle ctors/dtors/operators, virtuals, and `DecalCacheID` static.

2. **Haze / atmospheric chunk-caching materials** — `HazeChunkCache`, `PreHazeChunkCache`, `AlphaBlendCache`, `HazeMaterial`, `HazeOneTextureMaterial`, `HazeMaterialPlugin`, `HazeOneTextureMaterialPlugin`, plus `JetGetPluginsRender3DExtension`.
   - Material/plugin API: `IsSupported`, `CreateInstance`, type strings/UIDs (`HazeMaterial::DECORATED_TYPE/UNDECORATED_TYPE/UID`, `HazeOneTextureMaterial::*`), texture set/duplicate/bind/render/cache/create-geometry operations, plugin factory exports.
   - Cache/render pipeline hints: `Add`, `Begin/End/Enter/Leave/Reset/Update`, `FlushPipe`, `GetExtents/PointVisible/PointsExtents`, offset/scroll/scale/texture controls (`InitOffsetTexture`, `SetHazeTexture`, `ReleaseHazeTexture`, `SetOffset*`, `SetScrollSpeed`, `SetScaleTime`).
   - Plugin entry export: `JetGetPluginsRender3DExtension` (ordinal 27 / RVA 0x1000 in export TSV; function at 0x1000a190, size 5 in function map) — likely the module’s plugin registration surface.

3. **Fly-through / spline tooling** — `FlyThrough`, `RenderSpline`, `ParticleVisibleBoundingBox` (small/vf-heavy namespaces present in map).
   - `FlyThrough`: path/look-at spline definitions/accessors/speed/looping, `InitSplines`, `Load/SaveConfigData`, `Load/SaveFile`, `Reset/ResetPointArrays`, `GetNextPoint`, `AddPoint`, `TestRender`.
   - `MODLOG.md` note (2026-10-26, ConfigData::Write context): “Only Render3DCore / Render3DExtension import ConfigData::Write (for a debug spline.txt), so this unit is hardly exercised in normal play.” This is direct evidence tying FlyThrough/spline debug output to this module’s usage profile.

**Characterization:** an “extension” layer over 3D/render core (decals, haze materials/caches, spline/flythrough utilities), exposed partly through render/material plugin mechanisms rather than only plain exports.

## 2. Inventory (evidence-backed)

### 2.1 Size / function map (`ghidra_maps/.../Render3DExtension_7c.dll.functions.tsv`)
- **508 function rows total** (508 data rows, 463 non-`FUN_`/non-`thunk_FUN_` named in a quick check; remaining named include CRT/security/init helpers e.g. `__CRT_INIT@12`, `___DllMainCRTStartup`, `___security_init_cookie`).
- **Total mapped code size:** 39,572 bytes (sum of `size` column).
- **Global namespace:** 153 functions, 8,157 bytes (includes CRT/init/security and 45+ `FUN_*` helpers; largest unnamed helpers ~963..113 bytes each).

**Namespaces by mapped bytes (descending):**
| Namespace | funcs | bytes | notable evidence |
|---|---:|---:|---|
| `Global` | 153 | 8157 | CRT/security/init + unnamed helpers |
| `Jet::Decal` | 27 | 6885 | heavy geometry ops: `ClipGeometryToPlane` 1463, `NormalizeChunk` 1341, `MergeDecalGeometry` 680, `ClipGeometryToPolygon` 622, etc. |
| `Jet::FlyThrough` | 21 | 6100 | `SaveFile` 901, `SaveConfigData` 896, `InitSplines` 821, `GetNextPoint` 697, `LoadConfigData` 690, `LoadFile` 344 |
| `Jet::HazeChunkCache` | 27 | 6039 | `GetExtents` 2085, `End` 1054, `Begin` 786, `HazeChunkCache` ctor 550, `PointsExtents` 424 |
| `Jet::GeometryChunkNmlTemplate<Jet::PackedVertexPT,Jet::GeometryChunkSingleStream>` | 63 | 2099 | many small vf/slot impls (0x10007ca0 onward) |
| `Jet::ProjectedDecal` | 7 | 1559 | ctor/dtor/init/create/initinstance sized methods |
| `Jet::DecalParticle` | 54 | 1248 | many vf/thunk-like small entries + ctor/dtor/init/cull/render/create |
| `Jet::PointDecal` | 7 | 1121 | |
| `Jet::GeometryChunkTemplate<Jet::PackedVertexPT,Jet::GeometryChunkSingleStream>` | 6 | 1036 | vf impls |
| `Jet::PreHazeChunkCache` | 8 | 1010 | `FlushPipe` 116, cache lifecycle methods |
| `Jet::DecalCache` | 12 | 933 | begin/end/reset/add/vbase |
| `Jet::HazeMaterial` | 24 | 865 | create/cache/activate/bind/render/state helpers |
| `Jet::HazeOneTextureMaterial` | 26 | 761 | |
| `Jet::HazeMaterialPlugin` | 10 | 685 | plugin create/support/ctor/dtor |
| `Jet::HazeOneTextureMaterialPlugin` | 8 | 621 | |
| `Jet::RenderSpline` | 20 | 150 | mostly vf entries (small) |
| `Jet::ParticleVisibleBoundingBox` | 19 | 144 | mostly vf entries (small) |
| `Jet::AlphaBlendCache` | 2 | 45 | minimal mapped funcs |
| `Jet::ReferenceCount` | 1 | 36 | `vf1` only in map |

(README reports 326 functions; TSV/Ghidra map reports 508 — common discrepancy source is counting method vs all mapped functions/thunks/helpers; planning should treat TSV/map as the implementation inventory baseline.)

### 2.2 Export surface (`D:\trainz12-decomp\exports\Render3DExtension_7c.tsv`)
- **186 exports total.**
- Kinds (rough demangled classification): ~144 methods/ctors/dtors/operators, 26 vtables/vbtables, 14 static-data exports, 1 plugin entry, plus named statics.
- **Plugin entry:** `JetGetPluginsRender3DExtension`.
- **Exported named statics / IDs / types (examples):**
  - `Jet::DecalCache::DecalCacheID`
  - `Jet::HazeChunkCache::HazeChunkCacheID`
  - `Jet::PreHazeChunkCache::PreHazeChunkCacheID`
  - `Jet::HazeMaterial::DECORATED_TYPE`, `UNDECORATED_TYPE`, `UID`
  - `Jet::HazeOneTextureMaterial::DECORATED_TYPE`, `UNDECORATED_TYPE`, `UID`
- **Exported vtables/vbtables present for:** `Decal`, `DecalCache` (incl. `vbtable`, RC/RenderingCache vftables), `DecalParticle` (Decal/Particle bases), `FlyThrough`, `HazeChunkCache` (multiple bases), `HazeMaterial`/`HazeOneTextureMaterial` (Material/PluginInstance + vbtable), haze material plugin classes, `PointDecal`/`ProjectedDecal` (Decal/Particle bases), `PreHazeChunkCache`.
- **Notable exported behavior/API names:** decal init/render/cull/clip/merge/normalize, material/plugin `IsSupported`/`CreateInstance`/type getters/texture ops, chunk cache lifecycle and rendering hooks, flythrough load/save/init/spline access, particle create/render/cull/opacity/id/type.

### 2.3 VTable/name map observations (`vtnames_Render3DExtension_7c.dll.tsv`)
- Shared slot naming shows related/inherited hierarchies:
  - `Decal`, `DecalParticle`, `PointDecal`, `ProjectedDecal`, `RenderSpline` share many `vf*` assignments in the map (particle/decal family overlap).
  - `DecalCache`, `HazeChunkCache`, `PreHazeChunkCache`, `AlphaBlendCache` and geometry chunk template entries share some slots (rendering cache/reference-count/template relationships hinted).
  - `ParticleVisibleBoundingBox` and `RenderSpline` have extensive vf tables but small mapped method bodies in namespaces (many slots likely thunks/pure/forwarded or tiny impls).
- Multiple-base vtables are exported (e.g. `for Jet::RenderingCache`, `for Jet::ReferenceCount`, `for Jet::Material`, `for Jet::PluginInstance`, `for Jet::Particle/Decal`) — important for ABI/layout and thunk correctness.

### 2.4 Existing implementation artifacts
- **No** `src/jet/graphics/Render3DExtension_7c/`, no module tests, no prior module note under `notes/Render3DExtension*.md` found.
- Module is only referenced in status/docs/maps/exports and MODLOG context; no committed/in-progress source found in-tree for this DLL.

## 3. Dependencies (evidenced + implied)

### 3.1 Inter-module dependencies (evidenced)
- `MODLOG.md` render-module dependency summary (2026-10-26): **`Render3DExtension -> Render3DCore 134, RenderSystem 111`** (Jet imports).
- `MODLOG.md`: “Render3DCore / Render3DExtension import ConfigData::Write (for a debug spline.txt)”.
- `README.md` / `MODLOG.md`: not loaded at menu; loaded-module inventory lists RenderOpenGL/RenderSystem/Render3DCore/Render3DFoundation as render stack pieces in general, but extension absent at menu. Load conditions in gameplay/editor/specific effects not directly evidenced here.

### 3.2 API/types implied by demangled names (to be confirmed against imports/headers during implementation)
- **From RenderSystem (likely):** `Display`, `ViewPort`, `RenderKernel`, `Material`, `MaterialManager`, `Texture`, `GeometryChunk`, `GeometryChunkCreateHint`, `DisplayLight`, `Matrix4`, `Vector2/3/4`, `Quaternion`, `Plane`, `Line`, `Rect`, `AutoReference<T>`, `FastDynamicArray`, `PString`, `UId`, `String`, `ResourceDatabase`, `ConfigData`, `ResultClass<...>`, rendering cache/material/particle base contracts.
- **From Render3DCore (likely, given 134 imports):** particle system base/types (`Particle`, particle visibility/bounds?), spline/render-spline related types or scene/3D helpers used by decals/flythrough; exact split with RenderSystem must be resolved from import table when implementing.
- **From Kernel*/Jet core (transitively usual): containers, reference counting, config/resource primitives if not already via RS/3DCore wrappers.

> Planning note: `Render3DCore_7c` and `Render3DFoundation_7c` are 0/2500 and 0/1084 (not started) per README. This extension’s heavy import on Render3DCore means header/layout/vtable compatibility with 3DCore is a prerequisite or must be developed in lockstep.

## 4. Proposed implementation units / order (for future work)

No `src/` work done here; proposal is based on namespace sizes, export groupings, and dependency shape.

### Unit 0 — Proxy & scaffolding (baseline, no behavior ownership)
- Add module skeleton (`src/jet/graphics/Render3DExtension_7c/`), exports/def generation from `exports/Render3DExtension_7c.tsv` or existing tooling conventions, forward all to original (`*_orig.dll`) to establish load/link parity.
- Capture import table facts (actual imported modules/symbols) before ownership decisions.
- **Exit:** module loads identically where applicable; thunk/export coverage tooling clean.

### Unit 1 — Shared statics/data and plugin entry
- Own/export: `JetGetPluginsRender3DExtension`, named static data (`DecalCacheID`, haze cache IDs, haze material type strings/UIDs), simple data/vtable exports once owning classes require them.
- Rationale: small surface but ABI-visible; plugin/material registration depends on stable IDs/types/UIDs.
- **Risk:** static initialization order and CRT interactions; keep CRT/Global helpers as original until final.

### Unit 2 — Rendering caches (Decal/AlphaBlend/PreHaze/Haze)
- Classes: `DecalCache`, `AlphaBlendCache`, `PreHazeChunkCache`, `HazeChunkCache` and their exported lifecycle/render/update methods.
- Haze cache is the largest cache block (6 KB mapped) with geometry/screen-space extents, visibility tests, offset texture/scroll/scale state, buffered/flush behavior.
- **Dependencies:** `RenderingCache` base contract, `Display/ViewPort/GeometryChunk/Matrix4/Material/Texture` from RenderSystem; likely `Material`/viewpoint/light structs compatible with 3D stack.
- **Test idea:** stand-in Display/ViewPort/GeometryChunk/Material stubs; compare call/state sequences and extents math against original where feasible.

### Unit 3 — Haze materials and plugins
- Classes: `HazeMaterial`, `HazeOneTextureMaterial`, `HazeMaterialPlugin`, `HazeOneTextureMaterialPlugin`.
- Own IsSupported/CreateInstance/equality/type/texture/bind/activate/cache/render-chunk paths and plugin vtables.
- **Dependencies:** `Material`/`MaterialManager`/`PluginInstance`/`RenderKernel` interfaces and result types; cache APIs from Unit 2.
- **Test idea:** plugin query/creation with stub display/capability flags and material manager; compare decorated/undecorated type behavior and vtable dispatch.

### Unit 4 — Decal core
- Class: `Jet::Decal` (largest behavioral namespace, 6.9 KB).
- Own construction/copy/init, material/geometry/lights/flags, culling/rendering, and geometry mutation algorithms (clip/align/move/merge/normalize/index setup).
- **Dependencies:** GeometryChunk manipulation APIs, material/light/matrix/vector math, possibly create-hint/allocation behavior from RenderSystem/3DCore.
- **Risk:** float-heavy clipping/normalization can be sensitive to x87/SSE/NaN/order (see `notes/RenderSystem.md` quirks preserved elsewhere); needs differential tests on representative meshes/planes.

### Unit 5 — Particle/placed decals and spline/flythrough
- Classes: `DecalParticle`, `PointDecal`, `ProjectedDecal`, `RenderSpline`, `ParticleVisibleBoundingBox`, `FlyThrough`.
- Own Init/CreateInstance/Cull/Render/opacity/type behavior and flythrough config/file IO + spline evaluation.
- **Dependencies:** `Particle` base and 3DCore spline/particle types (likely imported from Render3DCore), `ConfigData` read/write behavior, `ResourceDatabase`.
- **Test idea:** flythrough load/save round-trips against original on real/config-generated files; particle/decal init with synthetic geometry/material stubs.

### Unit 6 — Template instantiations / geometry chunk specializations
- `Jet::GeometryChunkNmlTemplate<Jet::PackedVertexPT,Jet::GeometryChunkSingleStream>` (63 funcs) and `Jet::GeometryChunkTemplate<...>` (6 funcs).
- Determine whether these are exported template instantiations, internal helpers, or required for specific material/chunk paths; implement alongside owning callers (likely Units 2/4) to satisfy layout/vtable expectations.

### Unit 7 — Global/CRT tail and finalization
- Attribute remaining `Global FUN_*` helpers, confirm all exports/vtables/data owned, move any shared statics ownership only when coordinated (U9-style in RenderSystem).
- Remove forwarding once module and dependency contracts are stable.

**Suggested sequencing rationale:** data/plugin IDs -> caches -> haze materials/plugins -> decal core math/render -> particle/flythrough users -> template/cleanup. Reorder if import analysis shows different hard prerequisites (especially 3DCore particle/spline interfaces).

## 5. Risks, unknowns, and load conditions

### 5.1 Load / exercise conditions
- **README:** “Noted as not loaded at menu but part of render stack consideration.”
- **MODLOG (2026-10-26):** “Not loaded at the menu: ... Render3DExtension, RenderDirectX9. Re-check inside a driving session.” (RenderDirectX9 caveat noted elsewhere; extension still marked not loaded at menu.)
- **MODLOG:** ConfigData::Write debug `spline.txt` path involving Render3DCore/Render3DExtension is “hardly exercised in normal play”.
- **Implication:** validating in-menu parity may be insufficient; in-session/editor/specific-content scenarios may be required to exercise decals/haze/flythrough. Actual runtime triggers not fully evidenced in read materials.

### 5.2 Technical risks
- **Upstream dependency incompleteness:** heavy imports from `Render3DCore` (134) and `RenderSystem` (111). `Render3DCore`/`Render3DFoundation` are not started; RenderSystem is in-flight (U7, uncommitted per coordination). ABI/layout/vtable-slot mismatches are likely until contracts stabilize.
- **Multiple inheritance / vtable exports:** vtables/vbtables for `RenderingCache`, `ReferenceCount`, `Material`, `PluginInstance`, `Particle`, `Decal` combinations must match TS12 exactly (slot order, adjustor thunks, deleting dtors/covariant returns as applicable).
- **Static exported data:** IDs, type strings, UIDs, vftable symbols are ABI surface; initialization timing and ownership (ours vs original) must be managed carefully.
- **Float/geometry sensitivity:** decal clipping/normalization and haze extent/projection math are algorithmic and float-order sensitive; differential testing should expect/handle FP quirks (RenderSystem notes show x87/SSE operand-order issues mattered elsewhere).
- **Attribution gap:** 153 Global functions / ~8 KB include unnamed helpers and CRT pieces; template/inline/helper attribution needed before safe partial ownership.
- **No existing harness:** no module diff tests or src folder; test scaffolding must model Display/ViewPort/Material/GeometryChunk/Particle/ConfigData interactions.

### 5.3 Process/coordination risks
- `notes/agents/COORDINATION.md`: RenderSystem_7c source/tests/docs are in-flight/uncommitted — do not modify; treat interfaces as moving.
- Working tree already has unrelated in-progress changes (RenderSystem U7, progress/tooling files); analysis-only work must avoid contamination.

## 6. Cross-agent notes

### For `RenderSystem_7c`
- **Direct dependency:** Render3DExtension imports ~111 RenderSystem symbols (MODLOG). Expected touchpoints by name: `Display`, `ViewPort`, `RenderKernel`, `Material`/manager/plugins/result types, `Texture`, `GeometryChunk`/create hints, math types, containers/strings/resources/config.
- RenderSystem currently has active/uncommitted U7 work (`Material*` area) per coordination; haze/material plugin behavior here will depend on stable `Material`/plugin/factory contracts and exported statics. Coordinate on any Material/PluginInstance vtable layout assumptions before implementing extension materials.
- RenderSystem notes show static-data ownership deferral patterns (`*_orig` import library until foundation/U9) — same strategy likely needed here.

### For `Render3DCore_7c` and `Render3DFoundation_7c`
- **Critical dependency:** Render3DExtension imports ~134 Render3DCore symbols (MODLOG), highest among its render deps. Likely provides/defines `Particle` base, spline/visibility/render-spline support, and/or 3D object contracts used by `DecalParticle/PointDecal/ProjectedDecal/RenderSpline/ParticleVisibleBoundingBox/FlyThrough`.
- Because 3DCore/3DFoundation are not started (0 exports), implementing this extension before/without matching their class layouts and vtables is high-risk. Recommend: extract/import requirements from Render3DExtension refs early when 3DCore starts, or define shared headers in coordination.
- Verify whether `ConfigData::Write` debug spline output path crosses 3DCore/Extension boundaries in ways that affect runtime behavior.

### For renderer backends (`RenderOpenGL_7c`, `RenderDirectX9_7c`)
- No direct evidence in read artifacts that renderers import Render3DExtension; dependency is more likely one-way (extension uses RenderSystem/Display abstractions implemented by renderers).
- Extension should remain API-backend-agnostic through `Display`/material/geometry interfaces; any renderer-specific assumptions (caps, texture formats, multipass/haze path) must be validated against both backends when those paths are exercised.
- Load condition differs by renderer choice/session; menu-only OpenGL checks are insufficient to claim extension parity.

### General coordination
- Before implementation claims: inspect actual PE import table of `Render3DExtension_7c.dll`, map unnamed globals, and identify vtable slot names from 3DCore/RenderSystem headers/maps.
- Suggested first implementation milestone: proxy + import/export inventory + differential harness for pure data/plugin entry and one isolated subsystem (e.g. FlyThrough config IO or Decal pure geometry helpers) once dependency stubs are stable.
- Any future edits touching RenderSystem interfaces/docs/build while it is in-flight must be coordinated per `notes/agents/COORDINATION.md`.

## Scaffold (Wave 3)

**Date:** 2026-11-19 (module-owned, proxy-first). No shared/contended files edited; no builds/tests run.

### Files created
- `src/jet/graphics/Render3DExtension_7c/implemented.txt`
  - Proxy-first forwarding intent (when wired): all 186 exports forwarded to `Render3DExtension_7c_orig.dll`; **no exports enabled**.
  - Documents evidenced unmangled plugin entry `JetGetPluginsRender3DExtension` (ordinal 186, RVA 0xa190, ~5 bytes) and warns registration side effects/order/deps must be recovered (no-op unsafe).
  - Summarizes evidenced export/map clusters and ABI/static/vtable dependency caveats (decal, haze caches/materials/plugins, flythrough/spline/visibility, geometry-chunk templates, exported IDs/type strings/UIDs/vtables+vbtables).
- `src/jet/graphics/Render3DExtension_7c/Render3DExtension.h`
  - Documentation/placeholders only; no guessed ABI/layouts/offsets, vtable+vbtable/slot order, calling conventions, MI/adjustor-thunk structure, RTTI, static ownership/init, or behavior.
  - Declares evidenced entrypoint `extern "C" void JetGetPluginsRender3DExtension();` (not implemented/enabled).
  - Records evidenced clusters/classes and exported statics (Decal/Cache/Particle/PointDecal/ProjectedDecal; HazeChunkCache/PreHazeChunkCache/AlphaBlendCache; HazeMaterial(s)/HazeMaterialPlugin(s); FlyThrough/RenderSpline/ParticleVisibleBoundingBox; `GeometryChunkTemplate`/`GeometryChunkNmlTemplate<Jet::PackedVertexPT,Jet::GeometryChunkSingleStream>`) and dependency/runtime notes.
- `src/jet/graphics/Render3DExtension_7c/Render3DExtension.cpp`
  - Includes header and contains TODO/staged implementation outline only; placeholder `JetGetPluginsRender3DExtension` body is non-implemented/non-exported with explicit warning that no-op/partial registration is unsafe pending decomp + stable dependency contracts.
  - Not wired into shared build/test/progress surfaces.

### Stubbed vs deferred
- **Stubbed/documented now:** module-local proxy-forwarding intent (`implemented.txt` empty of enabled symbols), evidenced plugin-entry symbol declaration/placeholder, cluster+static+dependency/runtime notes, and staged unit TODOs.
- **Deferred (all behavior/ABI):** every exported method/ctor/dtor/operator, all vtables/vbtables/adjustor thunks, all static data/IDs/type strings/UID ownership+init, `JetGetPluginsRender3DExtension` registration semantics, class layouts/MI/slot order/calling conventions, Global helper attribution (~153 funcs / ~8 KB incl unnamed/CRT), geometry-chunk template role, and any differential/runtime harness. **No local exports enabled.**

### Key ABI/dependency/runtime-trigger TODOs and risks
- **ABI/layout:** recover exact sizes, field offsets, MI/vbtable layout, exported vtable+vbtable slot order and adjustor thunks for Decal/Particle/RenderingCache/ReferenceCount/Material/PluginInstance combinations; verify template specializations and static init/ownership (IDs, DECORATED/UNDECORATED TYPE, UIDs).
- **Dependencies (critical):** MODLOG imports Render3DCore (~134) and RenderSystem (~111). Likely needs stable RenderSystem contracts (Display/ViewPort/RenderKernel/Material* /PluginInstance/Texture/GeometryChunk*/math/containers/resources/config) and Render3DCore contracts (Particle base, spline/visibility/render-spline/types). `Render3DCore`/`Render3DFoundation` are 0-exports/not started and RenderSystem U7 is in-flight/owned/uncommitted — do not assume stable layouts/interfaces.
- **Runtime/trigger:** README/MODLOG: not loaded at menu (re-check driving/editor/session/specific-content). `ConfigData::Write` debug `spline.txt` via Render3DCore/Render3DExtension reported “hardly exercised in normal play”; menu-only parity insufficient.
- **Technical risks:** float-heavy decal clip/normalize and haze extent/projection math (FP/x87/SSE sensitivity), 508 mapped funcs vs 326 reported (inventory baseline = maps/exports), unattributed globals/thunks, and 45+ small `FUN_*`/template-vf helpers with unclear ownership.

### Later build-wiring steps (separate, coordinated claim; not done)
1. Confirm actual PE import table and align stub/forwarding assumptions with real imported symbols and dependency split.
2. Add module proxy/original-forward build wiring (e.g. CMake target/DEF/export handling) following project conventions, keeping `implemented.txt` empty until a symbol is proven safe to own.
3. Wire forwarding to `Render3DExtension_7c_orig.dll` and validate load/export parity in an appropriate runtime context (not menu-only).
4. Update shared surfaces only under coordinated ownership/serialization: `src/jet/CMakeLists.txt`, `README.md`, `PORT_PLAN.md`/module docs as applicable, `MODLOG.md`, and `tools/progress_exports.tsv`.
5. Enable exports incrementally (starting only with decomp-verified, dependency-stable units such as data/plugin entry) with differential/harness coverage; never claim parity without recovered ABI/contracts.
