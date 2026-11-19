# Render3DCore_7c analysis report (read-only)

Date: 2026-11-19
Scope: inventory/planning only. No source/build/doc edits performed outside this report.

## 1. Module overview

- **Binary (installed/original reference):** `D:\trainz12-decomp\bin\Render3DCore_7c.dll`, 958,464 bytes (2026 snapshot path from decomp workspace).
- **README / repo status (`README.md`):** 458 KB code, 3,168 functions, 0/2500 exports, **not started**. Group: Graphics (Jet engine). Step 8e in `PORT_PLAN.md` lists `Render3DCore, Render3DFoundation, RenderOpenGL, then InterfaceSystem, WaterSystem`.
- **Exports:** `D:\trainz12-decomp\exports\Render3DCore_7c.tsv` has 2,501 lines including header => **2,500 exported symbols** (ordinals 1..2500). Last export: ordinal 2500 `JetGetPluginsRender3DCore` (RVA 0x608e0).
- **Ghidra function map:** `ghidra_maps/jet/graphics/Render3DCore_7c.dll.functions.tsv` (224,160 bytes) has **3,361 lines total** (header + 3,360 entries). `source` column values include `ANALYSIS`/`DEFAULT`; **802 entries are in `Global` namespace** (unnamed/helpers/thunks/misc) by quick grouping, the rest namespaced under `Jet::*` classes/templates. This is a major attribution surface (cf. RenderSystem notes: 96 KB unnamed helpers there).
- **Vtable/name map:** `ghidra_maps/jet/graphics/vtnames_Render3DCore_7c.dll.tsv` (420,227 bytes) contains many `Jet::...::vf*` / `??_7...`-related naming entries; useful for class hierarchy, slot ownership, and shared/adjustor vtables.
- **Source tree:** no `src/jet/graphics/Render3DCore_7c/` folder yet, no `implemented.txt`, and `src/jet/CMakeLists.txt` has no `jet_module(Render3DCore_7c)` entry (current graphics modules wired: `Render2DCore_7c`, `RenderSystem_7c`, `WaterSystem_7c`).
- **Likely role (evidence + naming):** core 3D runtime above `RenderSystem_7c`: scene/particle graph, camera and viewport, lights, culling/bounding volumes, render primitives/debug geometry, splines/paths, rendering caches/material chunk caches and blend/mask sorting, material implementations/plugins, and large sets of specialized `GeometryChunk` template instantiations for packed vertex formats. It is distinct from neighboring modules by observed class sets:
  - `Render3DFoundation_7c` exports/functions begin with animation/geometry resource & definition types (`AnimationEvent/Modifier/Observer/Resource`, `Animator`, `GeometryBaseResource`, `GeometryConfigData`, `GeometryDefinition*`, `InfluenceBone`, ...; maps: `Render3DFoundation_7c.dll.functions.tsv`, `vtnames_Render3DFoundation_7c.dll.tsv`).
  - `Render3DExtension_7c` exports/functions show decals/flythrough/haze/material-cache extensions (`Decal`, `DecalCache`, `DecalParticle`, `FlyThrough`, `Haze*`, `PreHazeChunkCache`, etc.).
  - API backends `RenderOpenGL_7c` / `RenderDirectX9_7c` export mostly backend-specific Display/texture/shader/state implementations (maps show `Display*OpenGL/DirectX9`, texture, CG shader wrappers, etc.).
  Hypothesis: Render3DCore provides the shared 3D scene/material/geometry runtime consumed by Foundation/Extension/game/render backends, but exact import graph was not exhaustively proven here.

## 2. Inventory (maps/exports evidence)

### 2.1 Export composition (from `exports/Render3DCore_7c.tsv`)

- Exports are overwhelmingly decorated `Jet::` C++ symbols: constructors/destructors, methods, operators, vftables (`??_7...`), `typeid`/RTTI helpers, static data, and free/plugin entry points.
- Plugin/factory-related exports visible early/mid export list include many material plugin classes (examples): `StdMaterialPluginTemplate<AutoTexMaterial>`, `AutoTexMaterialPlugin`, `ColorOneTextureMaterialPlugin`, `GenericShaderMaterialPlugin`, `GlossMaterialPlugin`, `MVHSelectMaterialPlugin`, `MultiPassBump*MaterialPlugin`, `MultiPassBumpSpec*MaterialPlugin`, `MultiPassNoTextureMaterialPlugin`, `MultiPassOneTextureMaterialPlugin`, `NoTextureMaterialPlugin`, `OneTextureMaterialPlugin`, etc. (pattern from export scan).
- **Single plugin registration export:** `JetGetPluginsRender3DCore` @ ordinal 2500 / RVA 0x608e0.
- Many exported specializations of geometry chunk templates for packed vertex types, e.g. `Jet::GeometryChunkTemplate<struct Jet::PackedVertexPNTA,class Jet::GeometryChunkSingleStream>` (ctors and vftable exports present), and numerous other `PackedVertex*` combinations implied by vtable/function names (see §2.3).

### 2.2 Function map clusters (top namespaces by entry count, approximate)

From `ghidra_maps/.../Render3DCore_7c.dll.functions.tsv` (naive namespace prefix grouping):

| Namespace prefix | Entries | Notes |
|---|---:|---|
| `Global` | 802 | unattributed helpers/thunks/duplicated bodies; must be classified over time |
| `Jet::Particle` | 96 | particle base/runtime |
| `Jet::Camera` | 80 | large camera/frustum/view math cluster |
| `Jet::ViewPort` | 65 | viewport/render-target traversal/state |
| `Jet::GeometryChunkTemplate<Jet::PackedVertexPN,Jet::GeometryChunkSingleStream>` | 57 | representative template specialization (many others exist) |
| `Jet::GenericGeometryChunkMultiStream` | 47 | generic multistream geometry path |
| `Jet::Plane` | 40 | math/culling primitive |
| `Jet::AlphaBlendCache` | 37 | transparency/blend sorting cache |
| `Jet::MultiPassNoTextureMaterial` | 36 | multipass material path |
| `Jet::Polygon` | 33 | geometry/math |
| `Jet::OrientedBox` | 33 | bounds/collision/culling |
| `Jet::RenderPrimitive` | 32 | debug/render primitive base/helpers |
| `Jet::BackgroundResource` | 32 | async/background resource state machine/data |
| others (27-29 each) |  | `GenericShaderMaterial`, `ITree<Jet::Particle>`, `ParticleBoundingVolume`, `SkeletonModifier`, `MultiPassOneTextureMaterial`, `AttachmentPoint`, `AxisAlignedBox`, `AutoTexMaterial`, etc. |

**Interpretation:** Render3DCore is not a thin wrapper; it contains substantial algorithmic code (camera math, bounds/culling, splines, blend sorting, multipass/material logic, geometry chunk specializations).

### 2.3 Classes/systems visible in exports, functions, and vtables (representative)

- **Scene graph / spatial / particles:** `Particle`, `ParticleStructure`, `ParticleAlias`, `AttachmentPoint`/`AttachmentObserver`, `ITree<Particle>`, `Cell`, `Portal`/`PhysicalPortal`/`VirtualPortal`, `Mirror` (vtable names), bounding volumes (`ParticleBoundingVolume`, `ParticleAxisAlignedBoundingBox`, `ParticleBoundingSphere`, `ParticleVisibleBoundingBox` implied by vt names/functions), `ViewPortParticle`.
- **Camera & view:** `Camera` (many large functions: ctor variants, clip planes, view cone updates, etc.), `ViewPort`, `DebugViewPort` (vtable names). Largest functions include `Jet::ViewPort::CalculateOrientedBoxSize` (5413 bytes) and multiple large `Jet::Camera::*` routines (5101, 3533, 3152, 2901, 1968... bytes) — indicates complex frustum/culling/projection logic.
- **Lighting:** `Light`, `AmbientLight`, `OmniLight`, `ParallelLight`, `SpotLight`, `CubeMapLight` (exports/vtables/functions observed).
- **Rendering caches / sorting:** `AlphaBlendCache`, `AlphaMaskCache`, `MaterialChunkCache`, `GenericMaterialChunkCache`, `ParticleFXCache`, `ProjectorCache`, `UnsortedAlphaBlendCache` (vftable slots and methods present). Static/singleton-like data exports observed in tail of export list: `singleton` members for several caches, `BackgroundLoadingThread/Resource` tables/names, `RenderPrimitive::lights`, etc.
- **Materials & plugins:** base material-facing exports plus concrete materials/plugins such as `AutoTexMaterial`, `BillboardMaterial`, `ColorOneTextureMaterial`, `GenericShaderMaterial`, `GlossMaterial`, `MVHSelectMaterial`, `NoTextureMaterial`, `OneTextureMaterial`, `OneTextureNoAlphaMaterial`, `ReflectiveMaterial`, multipass families (`MultiPassNoTextureMaterial`, `MultiPassOneTextureMaterial`, `MultiPassBumpMaterial`, `MultiPassBump2Material`, `MultiPassBumpSpec*`, etc.), and associated `*MaterialPlugin`/`StdMaterialPlugin*` types. `MaterialPluginX` also exported.
- **Geometry chunks / vertex specialization:** many `GeometryChunkTemplate<PackedVertex*, GeometryChunkSingleStream>`, `GeometryChunkNmlTemplate<...>`, and `GenericGeometryChunkSingleStream`/`MultiStream` entries. This suggests Render3DCore owns or instantiates a large catalog of concrete chunk behaviors/templates beyond what is already in RenderSystem’s core geometry units.
- **Debug/render primitives:** `RenderPrimitive`, `Render2DPrimitive`, `Render3DPrimitive`, `RenderBox`, `RenderSphere`, `RenderCircle`, `RenderCone`, `RenderCylinder`, `RenderEllipse`, `RenderEllipsoid`, `RenderLine`/`RenderLines`/`RenderPolyline`, `RenderPlane`, `RenderPolygon`, `RenderSpline`, `RenderTube` (vtable/function names).
- **Math/geometry utilities:** `AxisAlignedBox`, `OrientedBox`, `Plane`, `Line`, `LineSegment`, `Ray`, `Sphere`, `Cylinder`, `Ellipse`, `Ellipsoid`, `Polygon`, `Polygon2D`, `Circle2D`, etc.
- **Splines/paths:** `Spline`, `SplineSegment`, `SplineDefinition`, `SplineResource` (vt names), `BSpline`, `BezierSpline`, `CardinalSpline`, `HermiteSpline` and segment variants/functions implied by class grouping and exports patterns in decomp outputs (spline functions appear among large routines, e.g. `Jet::SplineSegment::GetLength`, `Jet::SplineDefinition::WriteConfigData`).
- **Async/background loading:** `BackgroundLoadingThread`, `BackgroundResource` (ctors, FSM/state tables/input/action name tables exported).
- **Misc:** `Facility` (vtable seen), `Function`, `Corona` cache static (`cache` in exports tail context), `MultiPassRenderState` and `MultiPass*Chunk*` helpers (functions/vtables), skeleton/bone-related runtime pieces in core? (`BoneBlender`, `BoneModifier`, `SkeletonModifier` appear in class groups) — may overlap conceptually with Foundation animation but are present/exported from this DLL (verify ownership carefully during implementation).

### 2.4 Scale/complexity signals

- 2,500 exports / ~3.3k mapped functions / 458 KB code implies dense API surface and many small-to-medium virtual/template methods plus some very large routines.
- High `Global` count suggests significant inlining/template emission, adjustor thunks, compiler-generated helpers, and/or unattributed shared code.
- Large number of concrete material and geometry-chunk instantiations indicates build-time/ABI-sensitive template layout work and likely repetitive but error-prone implementations.

## 3. Dependencies and relationships

### 3.1 Upstream dependencies (likely required)

- **`Kernel_7c`, `KernelStandard_7c`, `KernelResource_7c`, `KernelResources_7c`** (baseline for Jet modules; RenderSystem already depends on these and Render3DCore will almost certainly need the same core/runtime/resource/string/math/container services). Evidence: all existing graphics modules link these in `CMakeLists.txt`; Jet architecture pattern.
- **`RenderSystem_7c`** is the primary graphics foundation dependency (**high confidence**): Render3DCore naming/exports imply heavy use of RenderSystem abstractions (`Material`, `MaterialManager`, `GeometryChunk`, `VertexType`/packed vertex formats, `Display`/`DisplayLight`, `Texture`/`TextureManager`, `RenderKernel`, rendering caches/layers concepts, etc.). README notes Render3DCore imports 320 RenderSystem exports (module relationship context in README group text style is consistent with "render modules stand on RenderSystem"); implementing before RenderSystem is mature would be high risk.
- **Windows/CRT/external libs as needed**: to be determined from actual imports during build (D3D/OpenGL not expected directly except maybe none; backends are separate modules).

### 3.2 Downstream / sibling consumers (likely)

- **`Render3DFoundation_7c`** and **`Render3DExtension_7c`**: strong architectural siblings above/alongside core 3D facilities (Foundation = animation/geometry resource definitions/loaders; Extension = decals/haze/flythrough). They will likely share types and call into Render3DCore scene/material/primitive/camera APIs. Coordination needed on ownership boundaries (e.g. spline resources/definitions vs spline primitives, skeleton/bone modifier boundaries).
- **`RenderOpenGL_7c`, `RenderDirectX9_7c`**: backend modules that implement/consume render-system display interfaces; likely interact with Render3DCore scene/material/camera state indirectly through RenderSystem contracts and/or directly for device-dependent paths. Exact coupling unknown.
- **`WaterSystem_7c`**: graphics module in same group; may consume scene/material/geometry services (hypothesis, verify via import analysis when implementing).
- **`InterfaceSystem_7c` / UI, `Render2DCore_7c`**: possible indirect consumers via shared render/kernel services (lower confidence for direct Render3DCore dependency).
- **`trainz.exe` and game-side modules**: likely major consumers given 2,500-export engine DLL (hypothesis; README states engine DLLs serve the game, but per-DLL import counts from game were not extracted here).

**Note:** A precise import/export dependency graph (which modules import Render3DCore symbols, and Render3DCore’s import table) was not fully enumerated in this pass; use `dumpbin /imports` or repo tooling in a future analysis pass before implementation ordering is finalized.

## 4. Proposed unit breakdown and implementation order

Rationale: follow RenderSystem/WaterSystem pattern — start with leaf/data/math types and registries, then self-contained subsystems with limited external state, defer modules with heavy global manager/render-kernel/display coupling and exported statics/singletons until late (foundation/U-last equivalent). Prefer units that can be differentially tested in isolation or with stand-in Display/Material/Geometry objects.

**U0. Proxy/bootstrap (prerequisite, coordinated build change)**
- Add `src/jet/graphics/Render3DCore_7c/` with empty `implemented.txt`, register `jet_module(Render3DCore_7c)` in `src/jet/CMakeLists.txt` (pattern: `JET_R3DC_BUILD` define, link original implib/`_orig` as needed, include dirs/gen tables only if required). Goal: forward all exports to original, establish build/test harness.
- Deliverable: clean proxy build, no behavior change.

**U1. Pure math/geometry value types (leaf)**
- Candidates: `AxisAlignedBox`, `OrientedBox`, `Plane`, `Ray`, `Line`/`LineSegment`, `Sphere`, `Cylinder`, `Ellipse`, `Ellipsoid`, `Polygon`, `Polygon2D`, `Circle2D`, small helpers (`Zero`, comparisons, transforms where pure). 
- Rationale: many exports, few/no runtime singletons, good for direct signature/ABI differential tests. Large methods (`MinimalBoxForAngles`, `CalcCorners`, `GrahamScan`, etc.) can be sub-sliced.
- Risk: SSE/x87/floating-point ordering quirks (RenderSystem had many) — keep assembly-visible evaluation order.

**U2. Spline primitives and definitions (mostly algorithmic)**
- Candidates: `BSpline/Segment`, `Bezier*`, `Cardinal*`, `Hermite*` (if present), `SplineSegment`, `SplineDefinition`, config/text IO helpers where separable. 
- Rationale: self-contained math/interpolation; can test against original with deterministic control points/parameters. Separate file/resource-coupled parts (`SplineResource` load/store integration) if they pull in resource DB behavior.

**U3. Bounding volumes & particle spatial helpers**
- Candidates: `ParticleBoundingVolume`, `ParticleAxisAlignedBoundingBox`, `ParticleBoundingSphere`, `ParticleVisibleBoundingBox`, collision/intersection helpers used by scene traversal.
- Rationale: dependencies mostly on U1 types; enables later particle/cell/culling work.

**U4. Rendering caches and sort/state helpers (careful with statics)**
- Candidates: `AlphaBlendCache`, `UnsortedAlphaBlendCache`, `AlphaMaskCache`, `MaterialChunkCache`, `GenericMaterialChunkCache`, `ParticleFXCache`, `ProjectorCache` (or split by cache). 
- Rationale: important and export-heavy, but likely touch Display state and global singletons (`singleton` data exports observed). Start with object-local behavior and exported methods; keep singleton storage/original state forwarded until a coordinated ownership point.
- Test strategy: stand-in Display/state logger and scripted primitive insertion/sort/bucket behavior comparisons.

**U5. Materials and material plugins (high volume)**
- Candidates: concrete material classes (`NoTexture`, `OneTexture*`, `Gloss`, `Reflective`, `Billboard`, `AutoTex`, `ColorOneTexture`, `GenericShaderMaterial`, `MVHSelect`, multipass families) and their plugin/factory types. Possibly split: (a) simple non-multipass, (b) multipass/no-texture/one-texture, (c) bump/spec/multitexture variants, (d) plugin registration/selection glue.
- Rationale: many exports but patterned code; depends on RenderSystem Material/MaterialManager/Texture/Display contracts and U4 caches. Multipass/render-state loaders (`MultiPassRenderState::LoadRenderProperties` is large) should be isolated subunits.
- Dependency: requires stable RenderSystem material/program/texture behavior and possibly shader/program resources.

**U6. Geometry chunk specializations and generic chunk helpers**
- Candidates: `GenericGeometryChunkSingleStream`, `GenericGeometryChunkMultiStream`, and families of `GeometryChunkTemplate<...>` / `GeometryChunkNmlTemplate<...>` instantiations (group by vertex layout families to reduce risk).
- Rationale: large export/template surface tightly coupled to RenderSystem `GeometryChunk`/`VertexType`/buffer APIs. Do after core RenderSystem geometry path (U5/U6 in RenderSystem notes) is well understood/stable.
- Build note: may require careful symbol naming/vtable emission and possibly data/table extraction if any per-vertex/per-format tables are embedded.

**U7. Lights and light application helpers**
- Candidates: `Light`, `AmbientLight`, `OmniLight`, `ParallelLight`, `SpotLight`, `CubeMapLight` and `RenderPrimitive::lights`/lighting support.
- Rationale: interacts with Display/Material passes and scene traversal; place after material/cache basics.

**U8. Camera, viewport, and culling/traversal core**
- Candidates: `Camera`, `ViewPort`, `DebugViewPort`, clip planes/frustum/cone/oriented-box sizing, viewport-space conversions.
- Rationale: very large/critical routines but central coupling point to Display and scene graph traversal. Best deferred until math, bounds, and particle structures are stable.

**U9. Scene graph / particles / attachment / portals / cells**
- Candidates: `Particle`, `ParticleAlias`, `ParticleStructure`, `AttachmentPoint`/`AttachmentObserver`, `ITree<Particle>`, `Cell`, `Portal` families, `Mirror`, visibility/traversal/cull flows.
- Rationale: highest behavioral coupling and likely most stateful; depends on bounds, camera/viewport, materials/caches. Good candidate for late integration with in-game/session tests.

**U10. Render primitives and debug rendering helpers**
- Candidates: `RenderPrimitive` hierarchy and concrete primitives (`RenderBox`, `RenderSphere`, `RenderLine(s)`, etc.), geometry build/test/collision helpers.
- Rationale: can be split and partly tested in isolation, but some may depend on lights/material/chunk paths; schedule opportunistically once required dependencies exist.

**U11. Background loading/resources and plugin entry / module statics**
- Candidates: `BackgroundLoadingThread`, `BackgroundResource` FSM/state tables, `JetGetPluginsRender3DCore`, exported singletons/data (`singleton`, `results`, name/state tables), `Facility`/misc remaining exports.
- Rationale: module-global state, threading/async behavior, and plugin registration should be finalized late with clear ownership (similar to "foundation/statics last" practice in RenderSystem U9).

**Suggested implementation cadence:** U0 -> U1 -> U2 -> U3 -> (U10 subset if independent) -> U4 (partial, statics forwarded) -> U5 -> U6 -> U7 -> U8 -> U9 -> U4/U11 finalization. Reassess after each unit using `tools/check_thunks.py`, `compare_vtables.py`-style checks (if applicable), and differential tests.

## 5. Risks, unknowns, and data/build integration needs

### 5.1 Risks

- **Scale and API breadth:** 2,500 exports across many interdependent classes/templates; easy to regress ABI/layout/vtable order.
- **Floating-point determinism:** camera/frustum, spline length, bounds fitting, sorting, and multipass/material math may be x87/SSE-order sensitive (observed as a recurring issue in RenderSystem notes). Large routines increase risk.
- **Large unattributed `Global` surface (802 entries):** helper/thunk ownership and calling conventions may be unclear until mapped to exports/classes.
- **Template/specialization explosion:** many `GeometryChunkTemplate`/`GeometryChunkNmlTemplate` and material variants can create subtle symbol/instantiation differences.
- **Global/static state ownership:** cache singletons, FSM tables, thread IDs, and module-level settings likely must remain original-owned until a final coordinated switch (cf. RenderSystem/KernelStandard guidance).
- **Concurrency/async behavior:** `BackgroundLoadingThread`/`BackgroundResource` may involve threading, timing, and state-machine edge cases hard to reproduce deterministically.
- **Cross-module coupling:** Render3DCore appears central to 3D stack; partial replacement may break if dependent modules expect exact internal behavior/data layout.

### 5.2 Unknowns / evidence gaps (to close before implementation)

- Exact **import table** of `Render3DCore_7c.dll` and full **reverse dependency graph** (which modules import which Render3DCore exports, especially trainz.exe/Foundation/Extension/OpenGL/DirectX/Water/UI).
- Precise **class layouts** and ownership for scene graph (`Particle*`, `Cell`, `Portal`), `Camera`, `ViewPort`, caches, and material internals (need Ghidra/decompile + header/name evidence in a dedicated pass).
- Which exported statics/data are **written/read by address** from other modules (critical for safe ownership transfer).
- Whether any **build-time extracted tables/data blobs** are required (RenderSystem needed scanner/parser/LUT tables from original; Render3DCore may or may not — not yet identified).
- Threading model and lifecycle for background loading and cache singletons (init/shutdown order, module/plugin init hooks).
- Exact semantics of `JetGetPluginsRender3DCore` (what plugin/resource/factory registrations it performs).

### 5.3 Build/integration needs (future, not performed)

- Add `Render3DCore_7c` proxy target in `src/jet/CMakeLists.txt` with `implemented.txt` forwarding (pattern used by other modules).
- Provide/include headers under `src/jet/include/jet/` for reconstructed layouts/types as units land (e.g. `Render3DCore.h` or split headers), keeping ABI/layout compatibility.
- Plan import libraries/`_orig` linkage strategy for mixed forwarded/implemented exports and original-owned statics (same as RenderSystem/WaterSystem).
- Add differential test executables under `src/jet/tests/` per unit, with stand-in `Display`/kernel/resource objects as needed.
- If any data tables must be extracted from the original binary at build time, integrate generator steps in CMake similar to RenderSystem/KernelStandard/ConfigParser patterns (only after confirming necessity and local source path availability).

## 6. Cross-agent notes (for related module agents)

### 6.1 `RenderSystem_7c` (currently in-flight per coordination)
- **Dependency direction:** Render3DCore is a major consumer of RenderSystem abstractions (geometry chunks, vertex formats, materials, textures, display/render kernel, shaders/programs/caches interfaces). Render3DCore implementation should track RenderSystem’s evolving layouts/APIs and avoid duplicating ownership assumptions.
- **High-value shared contracts:** `GeometryChunk` virtual interface/slot meanings, `VertexType`/packed vertex item layout, `Material`/`MaterialManager` behavior, `Display` call signatures/state semantics, `Texture`/`DisplayTexture` behavior, and multipass/program support.
- **Coordination caution:** RenderSystem has uncommitted in-progress work (see `notes/agents/COORDINATION.md`); do not treat current source/test behavior as frozen except what is committed/documented.

### 6.2 `Render3DFoundation_7c`
- Foundation exports/functions focus on animation and geometry resources/definitions (`Animation*`, `Animator*`, `GeometryBaseResource`, `GeometryConfigData`, `GeometryDefinition*`, `InfluenceBone`, etc.). Expect tight coupling to Render3DCore scene/bone/spline/geometry primitives or shared data structures.
- **Boundary to clarify:** `SplineResource`/`SplineDefinition`/spline segment types appear in Render3DCore naming as well as resource/definition concepts in Foundation — define ownership (runtime math vs resource/load/save) before parallel implementation.
- **Skeleton/bone overlap:** `BoneModifier`, `BoneBlender`, `SkeletonModifier` appear in Render3DCore class groups; Foundation handles animation resources/modifier loading. Coordinate type ownership and call paths to avoid duplicate/conflicting implementations.

### 6.3 `Render3DExtension_7c`
- Extension currently shows decals, flythrough, haze and related materials/caches (`Decal*`, `FlyThrough`, `Haze*`, `PreHazeChunkCache`). These likely layer on Render3DCore scene traversal, view/camera, materials, and chunk/cache infrastructure.
- **Useful from Render3DCore:** stable particle/cell/portal/view frustum APIs, material/chunk cache behavior, and render primitive/geometry submission contracts.
- **Risk to flag:** if Extension expects specific cache/singleton semantics or render ordering in core caches, early core rewrites can have outsized effects.

### 6.4 `RenderOpenGL_7c` and `RenderDirectX9_7c`
- Backend modules implement device/API-specific Display/texture/shader paths (evidence from their function maps). Render3DCore’s camera/material/chunk/caching code will exercise Display vtable contracts heavily.
- **Cross-check needed:** any assumptions Render3DCore makes about Display state flags, draw call argument forms, vertex/index buffer management, or shader/program binding must match both backends and RenderSystem’s abstract contract.
- **Testing implication:** differential tests for Render3DCore units that emit Display calls should log full call sequences/arguments against a stand-in Display, as done in RenderSystem work.

### 6.5 `WaterSystem_7c`
- WaterSystem already depends on RenderSystem classes (see `src/jet/graphics/WaterSystem_7c/RenderSystemShell.h`, notes) and may interact with scene/material/rendering facilities that overlap with Render3DCore domains (materials, geometry, textures, render passes) depending on integration points.
- **Info to share:** Render3DCore contains material/cache/geometry-heavy subsystems; if WaterSystem uses non-trivial 3D material/chunk facilities beyond current shell assumptions, coordinate interfaces rather than duplicating local shells.
- **Build status note:** `notes/agents/COORDINATION.md` warns WaterSystem has a known pre-existing build issue; avoid attributing regressions to WaterSystem when probing Render3DCore interactions.

### 6.6 `InterfaceSystem_7c` / UI agents (future)
- If UI/debug overlays or 3D picking/viewport helpers depend on Render3DCore camera/viewport/ray/primitive utilities, U1/U8/U10 APIs and ABI details will be relevant. Publish stable signatures/layouts early for shared math/camera types.

## 7. Recommended immediate next analysis steps (no implementation)

1. Dump and compare **imports/exports usage graph** for `Render3DCore_7c.dll` vs `Render3DFoundation/Extension/OpenGL/DirectX9/WaterSystem/trainz.exe` to confirm real dependency edges.
2. Identify **exported/static data symbols** and likely externally-addressed globals (singletons, tables, config flags) before any ownership transfer plan.
3. Produce a **class inventory by vftable/RTTI** from `vtnames` and decomp labels to separate primary classes from template instantiations and adjustor thunks.
4. Triage `Global` functions into buckets (thunks, vector deleting dtors, helpers, inlined/template bodies) to estimate true unique logic.
5. Draft U0 proxy integration notes (CMake/implib/_orig naming, include strategy) in coordination with RenderSystem in-flight work, but do not apply until coordinated.

---
*End of read-only report.*

## Scaffold (Wave 2)

**Date:** 2026-11-19 (module-owned scaffold prep; no shared build/doc wiring)

### Files created
- `src/jet/graphics/Render3DCore_7c/implemented.txt` — proxy-first: no exports enabled; all symbols intended to forward via future wiring. Notes `JetGetPluginsRender3DCore` (ordinal 2500, RVA 0x608e0, ~80 bytes per function map) as the only evidenced module entrypoint; registration side effects must be recovered from decomp before enabling (no-op not assumed safe).
- `src/jet/graphics/Render3DCore_7c/Render3DCore.h` — evidence-based scaffold header citing exports/functions/vtnames/module notes; documents major class/system clusters to implement later (plugin/material registration, materials/multipass, geometry-chunk template specializations, render caches/singleton-like state, camera/viewport, scene graph/particles/cells/portals, lights, math primitives, splines/resources, render primitives, background loading, exported statics). Declares `extern "C" void JetGetPluginsRender3DCore()` as evidenced export (unimplemented/unexported). States that all ABI/layout/vtable/vbtable/slot order/calling conventions/MI/RTTI/static ownership/behavior must be recovered from original decomp/maps before enabling exports.
- `src/jet/graphics/Render3DCore_7c/Render3DCore.cpp` — minimal placeholder TU only (no class bodies, no guessed vtables/method semantics). Includes staged TODOs and a documented-but-unexported placeholder for `JetGetPluginsRender3DCore` (kept forwarded; implementation deferred until registrations/order/IDs/side effects are known).

### Stubbed vs deferred
- **Stubbed (source-only, not exported):** placeholder `JetGetPluginsRender3DCore` symbol present in source for recovery work; not listed in `implemented.txt` and must remain forwarded until verified.
- **Deferred (all functionality):** every exported class/method/vtable/static/data item, plugin/factory registrations, material and geometry-chunk template instantiations, camera/viewport/particle/cell/portal/light/spline/primitive systems, render caches, background-loading FSM/thread behavior, and ~802 Global/unattributed helpers/thunks. Default remains proxy-first forwarding when build wiring is added.

### Key ABI / RenderSystem-dependency TODOs and risks
- **ABI/layout/vtable recovery:** 2500 exports / ~3360 mapped functions, dense template/specialization surface and shared/complex vftable slot groups (vtnames evidence), possible MI/vbtables/adjustor thunks, and 802 Global entries requiring attribution. No layouts, slot orders, calling conventions, RTTI, or mangling assumptions made.
- **RenderSystem dependency (critical):** core contracts (Display/Material/MaterialManager/Texture/GeometryChunk/VertexType/RenderKernel, multipass/program/caches/state semantics) are upstream and must align with `RenderSystem_7c`. **RenderSystem U7 is in-flight/owned/uncommitted** (coordination) — do not treat current RS behavior/layouts as frozen; coordinate before relying on internals.
- **Cross-module boundaries:** confirm import/consumer graph and coordinate ownership with `Render3DFoundation_7c` / `Render3DExtension_7c` (spline/resource vs runtime, bone/skeleton/modifier, particle/spline/layout), backends `RenderOpenGL_7c`/`RenderDirectX9_7c` (Display/vtable expectations), and potential consumers (`WaterSystem_7c`, UI/trainz.exe).
- **Static/data ownership:** exported singletons/tables/FSM/thread state (cache `singleton`s, `Background*` tables/names, `RenderPrimitive::lights`, etc.) may be ABI-visible/address-referenced; keep original-owned/forwarded until ownership is proven safe.
- **Behavioral risks:** x87/SSE/float-order sensitivity in camera/frustum/bounds/spline/sort/multipass paths; large routines; async/threading/lifecycle for `BackgroundLoadingThread`/`BackgroundResource`; template instantiation/symbol-emission parity for many `GeometryChunk*`/`PackedVertex*` variants.
- **Unknowns:** exact `JetGetPluginsRender3DCore` registration set/order/IDs/side effects, full PE import table and runtime consumers, any build-time extracted tables/data blobs, and precise load/init/shutdown ordering.

### Later build-wiring steps (under separate claim)
- Coordinate shared-file contention with owners (especially in-flight RenderSystem) before touching shared artifacts.
- Add `src/jet/graphics/Render3DCore_7c/` target in `src/jet/CMakeLists.txt` (e.g. `jet_module(Render3DCore_7c)` / `JET_R3DC_BUILD`) with correct include dirs and linkage to original implib/`_orig` proxy, mirroring existing module patterns; keep all exports forwarded initially.
- Provide/register `Render3DCore_7c_orig` proxy artifacts per project conventions without changing other modules' wiring.
- Under that same claim only: update shared docs/progress maps as appropriate (`README.md`, `PORT_PLAN.md`, `tools/progress_exports.tsv` when exports are actually taken from our code).
- Add per-unit differential tests/harnesses in `src/jet/tests/` once ABI/contracts are understood (stand-in Display/kernel/resource objects), avoiding contended build/test runs.