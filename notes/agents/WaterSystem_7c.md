# WaterSystem_7c (graphics) — analysis/planning report

**Scope:** read-only analysis only (no source/build/doc/test edits, no builds, no commits).  
**Written:** 2026-11-19.  
**Agent output:** this file only.

## 1. Sources consulted

- `notes/WaterSystem.md` (unit inventory/order, per-unit notes, status table)
- `README.md` (module status: 92 KB, 714 functions, 152/490 exports, partial)
- `src/jet/graphics/WaterSystem_7c/implemented.txt` (152 implemented export names, U1–U6)
- `src/jet/graphics/WaterSystem_7c/*.{cpp,h}` (U1–U6 implementations and local RS shell/internal helpers)
- `src/jet/include/jet/Water{Animation,Geometry,Grid,ManagerConfig,TextureAnimation}.h`
- `ghidra_maps/jet/graphics/WaterSystem_7c.dll.functions.tsv`, `.../vtnames_WaterSystem_7c.dll.tsv`
- `D:\trainz12-decomp\exports\WaterSystem_7c.tsv` (491 export rows incl. header)
- `src/jet/CMakeLists.txt` (module wiring, `JET_WS_BUILD`, only `diff_ws_u1` test target present)
- `notes/RenderSystem.md` (RS progress/dependencies/U7 material work), `MODLOG.md` (WaterSystem history + build issue), `notes/agents/COORDINATION.md`, `notes/agents/STATUS.md`

## 2. Current state

### 2.1 Module-level

- **Module:** `WaterSystem_7c` (graphics), 92 KB code, 714 functions, **152/490 exports implemented** (`README.md`, `implemented.txt` count), remainder forwarded to `WaterSystem_7c_orig.dll` via generated `.def` pattern used by other modules.
- **CMake:** built as a drop-in Jet module (`JET_WS_BUILD` defined) linked against `KernelStandard_7c.lib` and `Kernel_7c.lib` (`src/jet/CMakeLists.txt`); only differential test target present is `diff_ws_u1` (U1). No WaterSystem-specific link to RenderSystem import lib shown in the `foreach` block beyond standard Jet libs, while source uses RenderSystem symbols via `__declspec(dllimport)` in `RenderSystemShell.h` / raw vtable usage (build wiring should be verified when implementation resumes; read-only note).
- **Tests:** `src/jet/tests/diff_ws_u1.cpp` exists (17 KB) comparing U1 classes vs `WaterSystem_7c_orig.dll`. No `diff_ws_u2..u6` targets found in CMake; later done units appear to rely on original-forwarded surroundings and/or manual/game checks per MODLOG notes (not independently evidenced here).

### 2.2 Done units (U1–U6) — per `notes/WaterSystem.md` and code

| Unit | Scope (notes) | Key source/header evidence | Exports | Status |
|---|---|---|---:|---|
| U1 | `FrameAnimation`, `PNoise`, `WaterGrid<float>/<Vector2>` | `FrameAnimation.cpp`, `PNoise.cpp`, `WaterGrid.cpp`, `include/jet/WaterGrid.h`; notes: POD frame clock, 0x383C Perlin tables from CRT rand, grid `{vftable,width,height,data}` alloc via Jet::Alloc guard | 40 | ours (2026-10-26) |
| U2 | `WaterManagerConfig`, `AnimProp`, `MaterialProp`, `TileProp`, `PhysicsProp` | `WaterManagerConfig.cpp`, `include/jet/WaterManagerConfig.h` (0x60/0x50/0x24/0x0c/0x160 layouts, Load/Save/getters, interned-key-style parsing/db slot-3 lookup helpers) | 36 | ours (2026-10-27) |
| U3 | `DisplacementHull` | `DisplacementHull.cpp`; derives from water grid height field loaded via bitmap/db resolution (notes) | 6 | ours (2026-10-27) |
| U4 | `WaterAnimation`, `WaterAnimationPN` | `WaterAnimation.cpp`, `include/jet/WaterAnimation.h` (WA 0x24, PN 0x3870, plugin/lookup hierarchy, PN turbulence sampling/rescale, stubs `GetXYFrame`/`Init` quirks kept); `RegisterWaterAnimationFactory()` present for U10 use | 24 | ours (2026-10-27) |
| U5 | `WaterGeometry`, `PhysicsWaterGeometry`, `WaterGeometryAnimation`, `WaterGeometryVS` | `WaterGeometry.cpp`, `include/jet/WaterGeometry.h`, `RenderSystemShell.h`; raw RS interactions (material slot 46 create chunk, chunk slots/`GetIndexArray`, upload/render via material slot 14), physics buffers/wave coeffs, frame morph, VP attributes/rand waves; quirks documented in notes | 33 | ours (2026-10-27) |
| U6 | `WaterTextureAnimation` | `WaterTextureAnimation.cpp`, `include/jet/WaterTextureAnimation.h`; bump textures from animation grids/raw ARGB, uses imported RenderSystem `Texture` ctor with original vftable write (`/alternatename` hack per notes), RenderKernel texture manager slot 38 + DisplayTexture slots 1/3 raw VCall, `CalculateBumpMap` quirks/x87 behavior kept | 13 | ours (2026-10-27) |

**Implementation characteristics visible in code:** done units favor binary-faithful layouts/behavior (x87/quirks preserved), use local `RenderSystemShell.h` / raw vtable-slot calls instead of reconstructed RS APIs, and keep module/plugin statics/vftables original where owning unit/state is not yet replaced (see `notes/WaterSystem.md` U1–U6 notes and "Module state ... stays the original's until the units that own it are ours").

### 2.3 Remaining units (U7–U10) — per `notes/WaterSystem.md`

| Unit | Classes (notes) | Address ranges (notes) | Exports (notes) | Status |
|---|---|---|---:|---|
| U7 | `WaterMaterial` (base) + `WaterBumpMaterialPlugin`, `WaterOceanMaterialPlugin`, `WaterMaterialFresWavePlugin`, `WaterMaterialOneTexPlugin` | `10003530..10003700, 1000D080, 1000D220, 10007240..10007340, 10012900..10012F00, 10013E60..10013F20, 100159D0..10015AD0` | ~35 | ⬜ |
| U8 | `WaterBumpMaterial`, `WaterOceanMaterial`, `WaterMaterialFresWave`, `WaterMaterialOneTex` | `10005710..10006B20, 1000D290..10011930, 10013040..10013800, 10013FD0..10015410` | ~150 | ⬜ |
| U9 | `WaterTile`, `Water`, `WaterTiled`, `WaterSurface` | `10003700..10003F20, 10015C10..10015D00, 100171A0..10017810` | ~40 | ⬜ |
| U10 | `WaterManager`, `WaterManagerGeneric`, `WaterManagerOceanSim`, `JetGetPluginsWaterSystem` | `100092A0..1000CB90, 100162F0` | ~35 | ⬜ |

`notes/WaterSystem.md` also states: module statics, vftables, and shader strings `aVswatermat*`/`aPswatermat*` at `100207A0..10039004` stay the original's until owning units are ours.

## 3. Remaining work details (function-map grounded)

Counts/sizes below are grouped from `ghidra_maps/jet/graphics/WaterSystem_7c.dll.functions.tsv` (714 rows total) to refine the note-level view; they are planning aids, not a substitute for per-function decomp/ownership verification.

### U7 — Water material base and material plugins (~55 funcs, ~4.2 KB by namespace grouping)

- **`Jet::WaterMaterial` (base):** 25 funcs / ~1.53 KB (sampled): accessors/state (`Get/Set*Color`, `Get/SetLightDir`, `Get/SetLightColor`, UV/reflect/detail/VP flags: `GetDetailAnim`, `Set/GetUVScrollVelocity`, `Set/IsReflectEyeVector`, `GetUseVertexProgram`), lifecycle/instance (`WaterMaterial`, `~WaterMaterial`, `InitInstance` 390B at `0x1000d080`, `vbase_destructor`, `vf0/vf1/vf2/vf9/vf10/vf22` thunks/slots), and render integration hook `CreateGeometryChunk` (51B at `0x100036c0`).
- **Plugin classes:** `WaterBumpMaterialPlugin` (7 funcs, 352B), `WaterMaterialFresWavePlugin` (8 funcs, 1646B, includes `CreatePluginResources` 992B), `WaterMaterialOneTexPlugin` (5 funcs, 299B), `WaterOceanMaterialPlugin` (6 funcs, 332B), plus shared `IsSupported`/vtable thunks present in map (`IsSupported` at `0x10015a70` under bump plugin namespace in TSV; confirm ownership during implementation).
- **Related plugin factories in map:** `Jet::PluginFactoryOf<Jet::WaterAnimationPN,Jet::WaterAnimation>::vf3` (`0x10016320`), and manager factories appear (`...WaterManagerOceanSim/Generic...`) — registration/export surface ties into U10 even if some factory code lives in nearby ranges.
- **Role:** base material contract/state + plugin/factory objects that create/register concrete water material types used by managers/config (`MaterialProp.materialName` e.g. `Default.m.WaterMaterialFresWave` in U2 defaults).

### U8 — Concrete water materials (~125 funcs, ~29.9 KB, heaviest render logic)

- **`Jet::WaterMaterialFresWave`:** 41 funcs / ~17.9 KB. Dominant cost/complexity: `Initialize` (2034B), `InitInstance` (1269B), `RenderNumChunks` (4431B), `SetupVertexProgramConstants` (3584B), `InitVertexProgramms` (746B), texture/light/detail calculations and updates (`CalculateDetailTexture`, `CalculateWaterColorTexture`, `CalculateLightAnimTexture`, `CalculateLerpTexture` absent here but present in bump/ocean; freswave has its own set), `CreateGeometryChunk`, `SetTexture` overloads, chunk cache/render/update paths.
- **`Jet::WaterBumpMaterial`:** 39 funcs / ~5.3 KB. Includes `SetupVertexProgramConstants` (1648B), `CalculateLerpTexture` (732B), `RenderSquare` (194B), `CreateVertexType` (385B), render/state/bind/activate/texture paths.
- **`Jet::WaterOceanMaterial`:** 21 funcs / ~4.7 KB. Includes `SetupVertexProgramConstants` (1651B), `CalculateLerpTexture` (732B), `CreateVertexType` (462B), `RenderChunk` (422B), wave attributes/update/render helpers.
- **`Jet::WaterMaterialOneTex`:** 24 funcs / ~2.0 KB. Simpler single-texture material (set/dup/get texture, bind/unbind, render chunk/num/primitives, create geometry chunk, minimal init/state).
- **Role:** actual water shading/render paths, vertex/texture state setup, per-frame/per-chunk parameter uploads, texture animation/detail/reflection interactions; likely primary consumers of RS vertex programs, texture combiners/material APIs, display state, and chunk/vertex-type creation.

### U9 — Water tiles, surface, scene objects (~89 funcs, ~6.2 KB + large vtable/thunk surface)

- **`Jet::WaterTile`:** 17 funcs / ~1.41 KB (`WaterTile` ctors/dtor/assign, set/get geometry & material, light/position, cull/render helpers, `vf0..vf5`).
- **`Jet::Water`:** 56 funcs / ~2.78 KB but extensive adjustor-thunk/vf surface in map (`vf4..vf47` etc.) indicating multi-interface/multiple-inheritance vtable layout complexity; operations include add/remove tile, lights, cull, AABB (`GetAxisAlignedBoundingBox` 1271B).
- **`Jet::WaterTiled`:** 8 funcs / ~1.58 KB (ctor/dtor/assign/copy-like forms, `GetTile`, `Render` 942B, `Cull`).
- **`Jet::WaterSurface`:** 8 funcs / ~414 B (ctors, `Initialize`, `GetScale/GetNormal`, `vf1`, operator=).
- **Role:** runtime composition/render-culling units that bind geometry/materials/lighting and present tiled water surfaces to the render pipeline.

### U10 — Water managers and plugin entry (~55 funcs, ~11.5 KB incl. config)

- **`Jet::WaterManager` (base):** 8 funcs / ~887 B (ctor/dtor/vbase, `CreateInstance`, `Initialize`, `GetConfig`, vtable slots).
- **`Jet::WaterManagerGeneric`:** 15 funcs / ~3.83 KB (ctor/dtor/init, `CreateCubeMapTexture` 1620B, `Update`, material/texture/reflection/scale accessors).
- **`Jet::WaterManagerOceanSim`:** 17 funcs / ~2.63 KB (ctor/dtor/init, `CreateCubeMapTexture` 947B, `Update` 189B, geometry/texture/material/tile/scale/reflection accessors).
- **`Jet::WaterManagerConfig`:** 13 funcs / ~3.84 KB in map (implemented in U2; manager-side construction/load overloads appear in function ranges — confirm remaining unimplemented exports/overloads belong here or already covered).
- **Plugin/entry points:** `Global JetGetPluginsWaterSystem` (`0x100162f0`, 45B, currently forwarded per `implemented.txt` absence) and `PluginFactoryOf<Jet::WaterManagerGeneric/OceanSim,Jet::WaterManager>::vf3` entries in map. U4 already provides `RegisterWaterAnimationFactory()`; U10 is the natural owner of module plugin registration/export.
- **Role:** top-level selection/creation/configuration/update of water systems, resource/material/geometry/tile wiring, reflection/cubemap setup, and module plugin publication to the engine.

## 4. Suggested implementation order and rationale

Follow `notes/WaterSystem.md` order **U7 → U8 → U9 → U10**:

1. **U7 (base material + plugins):** establishes material base ABI/state and factory/plugin objects concrete materials/managers depend on; smaller surface (~35 exports per notes) and prerequisite for U8 `CreateInstance`/`InitInstance` contracts.
2. **U8 (concrete materials):** largest and most render-critical block; depends on U7 interfaces and heavily on RS material/shader/texture/chunk APIs. Splitting by material class (e.g. `OneTex` → `Bump`/`Ocean` → `FresWave`) can reduce risk, but export/vtable coherence must be preserved per class.
3. **U9 (tile/surface/water objects):** composes geometry (U5), materials (U7/U8), textures/animation data (U4/U6) into renderable/cullable scene entities; vtable/layout correctness (esp. `Water`) is critical.
4. **U10 (managers + `JetGetPluginsWaterSystem`):** final orchestration and module entry/registration; best last because it instantiates/configures U4–U9 components and owns externally visible plugin wiring. Keep `JetGetPluginsWaterSystem`/factory registration consistent with existing `RegisterWaterAnimationFactory()` and engine expectations.

Within each unit: prefer class/vftable-coherent slices (ctors/dtors/vtable, then init/config/state, then render/update hot paths), mirroring prior RS/KS practice, but this is planning only.

## 5. Dependencies and integration points

### 5.1 RenderSystem (critical, cross-cutting)

- **Already used by done units:** `Texture`/`DisplayTexture`/`RenderKernel` texture manager (U6), `GeometryChunk`/vertex arrays/indices/upload, `StandardChunkHint`, material-driven chunk creation and render dispatch, `Display`/`ViewPort`/`DisplayLight`/`Matrix4` (U5). `RenderSystemShell.h` currently exposes only minimal imported shells/types.
- **Expected for U7–U10 (from function names/notes):** RenderSystem material base/manager/resource/plugin interfaces; vertex types/programs and texture combiners/shaders (`InitVertexProgramms`, VP constants/setup, shader strings in module data range `100207A0..10039004` noted as staying original until owning units replaced); display state/bind/unbind/activate/pass rendering; texture/resource priority/loading/duplication; geometry chunk templates/creation for specific packed vertex formats (vtable names in TSV show `GeometryChunkTemplate<PackedVertexPCTUD/PNTT/PA2A4T/PNT/PA1A2T,...>` entries in module context); possibly render-to-texture/cubemap/reflection camera support for managers/material reflection paths (`CreateCubeMapTexture`, `SetReflectionCamera/Type`).
- **Current RS status/risk:** `README.md` shows `RenderSystem_7c` partial (2185/2687). `notes/RenderSystem.md` tail records active U7 material work (manager/create/extractors/resource, recent sessions) and states many material/manager pieces are now ours while others and shared statics remain original until U9 — **high coupling** for WaterSystem U7/U8.
- **Coordination warning:** `notes/agents/COORDINATION.md` marks RenderSystem_7c source/tests/docs as **owned/in-flight/uncommitted**; do not modify without coordination (analysis may read).

### 5.2 Other Jet/render modules

- **Kernel/KernelStandard/KernelResource(s):** config parsing, strings, alloc/refcount/plugin/resource primitives, DB/file access patterns already used in U2/U4/U6; managers/material resources may require additional resource/plugin APIs.
- **Render3DCore/Render3DFoundation/OpenGL/DX9 (indirect):** consume/provide rendering services through RS/Display abstractions; WaterSystem correctness ultimately depends on stable RS+backend behavior in-game, but implementation planning is primarily against RS contracts.
- **Module-internal:** U2 `WaterManagerConfig` feeds U10; U4 animation objects/frames feed U5/U6/U8/U10; U5 geometry + U6 textures feed U8/U9; U7/U8 materials feed U9/U10.

### 5.3 Data/static ownership

- `notes/WaterSystem.md`: "Module state (statics, vftables, the shader strings `aVswatermat*`/`aPswatermat*` at 100207A0..10039004) stays the original's until the units that own it are ours" — remaining units likely must introduce/claim specific vftables/statics/factory instances in lockstep with exported functions to avoid mixed-ABI crashes.

## 6. Risks and blockers

1. **Pre-existing WaterSystem build break (blocking):** `MODLOG.md` (2026-11-19 housekeeping note): *"the WaterSystem_7c target does not build in the current VS 18 2026 configuration (`WaterGeometry.h:89 JET_LAYOUT32(sizeof(PhysicsWaterGeometry) == 0x7c)` fails, and `tests/diff_ws_u1.cpp` has follow-on API errors). Verified pre-existing at HEAD..."* This is outside this task to fix but blocks compile/test iteration for any implementation claim.
2. **RenderSystem in-flight and partially reconstructed:** uncommitted RS U7 material changes and partial export coverage create API/layout/static instability risk for WaterSystem materials/managers. Raw slot-number calls in existing WaterSystem code increase sensitivity to RS ABI details.
3. **Large, shader-heavy U8 surface:** FresWave alone contains multi-KB init/render/VP-constant routines (4.4 KB / 3.6 KB / 2.0 KB functions) with likely x87/SSE/NaN/operand-order-sensitive math (RS had similar quirks) and complex RS program/texture/display interactions — high regression risk and hard to validate without good differential harnesses.
4. **Complex vtable/inheritance in U9:** `Water` shows many vf adjustor thunks/slots in the map; incorrect layout or thunk ownership can break polymorphic calls across original/ours boundary.
5. **Plugin/export correctness in U10:** `JetGetPluginsWaterSystem` and factory registration must match engine plugin discovery; partial replacement while plugin data/statics remain original can fail at runtime.
6. **Limited existing automated coverage:** only `diff_ws_u1` target is wired; U2–U6 lack visible dedicated diff targets, and U7–U10 will need new render/material/manager comparison strategies (stand-ins, logging displays, real asset/config inputs) consistent with RS/KS practices.
7. **No-build/no-edit constraint for this task:** findings cannot be validated by compiling the broken target or by probing decomp/source beyond read-only inspection.

## 7. Cross-agent notes (RenderSystem and render modules)

- **RenderSystem_7c (owned/in-flight per coordination):** avoid source/test/doc edits without an explicit claim. WaterSystem U7/U8 will directly stress `Material`/material-manager/resource/plugin paths, vertex programs/texture combiners, `GeometryChunk`/vertex-type creation, texture/display state, and possibly cubemap/reflection APIs. Any RS slot/layout/signature changes are high-impact; please surface stable slot contracts/layout assumptions when material U7 APIs settle.
- **U5/U6 integration already encodes RS slot numbers:** WaterGeometry uses material slot 46 (`CreateGeometryChunk`) and slot 14 (render) and chunk raw slots (e.g. 49/47/4/31 per notes); WaterTextureAnimation uses RenderKernel slot 38 and DisplayTexture slots 1/3. U7–U10 implementations will likely need more such contracts — coordinate/document rather than diverge silently.
- **Shader strings / vftables / statics:** WaterSystem notes mark shader strings `aVswatermat*`/`aPswatermat*` (`100207A0..10039004`) as original-owned until relevant units are replaced; align with RS-side shader/program ownership to prevent dangling references.
- **Shared blocker:** resolving the pre-existing WaterSystem build failure (`WaterGeometry.h:89` `sizeof(PhysicsWaterGeometry) == 0x7c`, `diff_ws_u1.cpp` API drift) is a prerequisite for implementation/testing; coordinate ownership before changing `src/jet/graphics/WaterSystem_7c/*`, headers, or tests because this module is otherwise pending but the tree is partially in-flight elsewhere.
- **Render backends / 3D modules:** no direct code changes implied here, but runtime validation of U8/U9/U10 will depend on RS+Display (OpenGL/DirectX9) behavior; report observed render-state/material-program assumptions if discovered during later implementation.
- **Process:** if an implementation session starts, add a claim in `notes/agents/COORDINATION.md`, respect the no-shared-build contention rule, and append status to `notes/agents/STATUS.md` on completion.

## 8. Recommended next analysis steps (read-only, before implementation)

- Decompile/inspect per-function bodies for `Jet::WaterMaterial` and the four plugin classes to confirm vtable layouts, base-class offsets (`vbase_destructor` usage), and factory/resource registration contracts.
- Map U8 concrete material vtables against RS `Material` virtual interface expectations (slot numbers/counts), especially `Initialize/InitInstance/CreateInstance/CreateGeometryChunk/Render*/SetupVertexProgramConstants/Bind/Unbind/Activate/SetTexture`.
- Extract/identify shader string tables in `100207A0..10039004` and correlate with FresWave/Bump/Ocean program init paths.
- For U9, resolve `Water` inheritance/interfaces to explain the large adjustor-thunk set and confirm object sizes/layouts before coding.
- Trace `JetGetPluginsWaterSystem` and manager/plugin factory flow to determine exact registration list and dependency on `RegisterWaterAnimationFactory()`.
- Reconfirm the exact cause of the `JET_LAYOUT32(sizeof(PhysicsWaterGeometry) == 0x7c)` failure and `diff_ws_u1.cpp` API mismatch in a coordinated implementation session (likely layout/header drift or toolchain packing/define differences).

## 9. Summary

- **Done:** U1–U6 (152/490 exports) covering config/properties, grids/noise/animation data, displacement hull, geometry (incl. physics/VS variants), and texture animation — implemented with binary-faithful quirks and raw RS vtable integration.
- **Remaining:** U7–U10 (~338 exports) — material base/plugins, four concrete shader-heavy materials (~30 KB), tile/surface/water scene objects, and managers + plugin entry. Order per notes is appropriate.
- **Main blockers/risks:** pre-existing WaterSystem build failure in VS 18 config, RenderSystem partial/in-flight material work, heavy U8 render/shader complexity, U9 vtable/layout complexity, and limited differential test coverage for later units.
- **Primary cross-agent dependency:** `RenderSystem_7c` material/shader/geometry/texture/display contracts and shared static ownership; coordinate closely and avoid touching in-flight RS sources.
