# Render3DFoundation_7c — analysis/planning report

Date: 2026-11-19 (local analysis session)  
Scope: **read-only** inventory/planning for unfinished module `Render3DFoundation_7c` (group: `graphics`). No source/build/docs/tests/game files modified except this report and `notes/agents/STATUS.md`.

## 1) Overview / role (evidence + hypotheses)

**Evidence (README / repo state):**
- `README.md`: Jet graphics module, **194 KB code, 1453 functions (README), 0/1084 exports ours, status not started**. Repo currently has no `src/jet/graphics/Render3DFoundation_7c/` implementation tree.
- Plugin/export surface includes `JetGetPluginsRender3DFoundation` (`D:\trainz12-decomp\exports\Render3DFoundation_7c.tsv`, ord 1084, RVA 0x218a0), indicating a standard Jet plugin registration entrypoint.

**Inventory-derived role (from exported/named classes and function map):**
- Mid/high-level 3D "foundation" features above `RenderSystem_7c` primitives: **animation/modifier + animator runtime, movie texture/player resources, geometry/influence/LOD/material/texture config-data and mesh/particle helpers, shadow generation/volume/blob/cache/rendering, texture projectors and render textures**.
- Major namespaces by code bytes (`ghidra_maps/jet/graphics/Render3DFoundation_7c.dll.functions.tsv`): `Jet::GeometryConfigData` (82 funcs, 29,852 B), `Jet::VolumeMesh` (30, 14,002 B), `Jet::ShadowVolumeGeom` (28, 13,289 B), `Jet::AnimationModifier` (86, 11,705 B), `Jet::TextureProjector` (37, 9,443 B), `Jet::AnimationModifierLoader` (22, 9,260 B), `Jet::ShadowVolume` (16, 8,719 B), `Jet::MovieCanvas` (32, 7,474 B), plus substantial `Animator`, `Influence*`, `Movie*`, `Shadow*`, `ObjectLOD`, `RenderTexture`, `ProjectorCache`, etc.

**Hypothesis:** this DLL provides reusable 3D feature implementations/config loaders consumed by scene/renderer/game modules (`Render3DCore`, `Render3DExtension`, render backends, `trainz.exe`, content/runtime systems), while `RenderSystem_7c` supplies the API-agnostic display/material/geometry/texture/layer core. Exact layering/call graph with `Render3DCore` vs `Render3DExtension` requires import/cross-reference analysis beyond this pass.

## 2) Evidence sources examined

- `trainz12-x64/README.md`, `trainz12-x64/notes/agents/COORDINATION.md`, `notes/agents/STATUS.md`
- `trainz12-x64/notes/RenderSystem.md`, `trainz12-x64/notes/WaterSystem.md` (pattern/reference)
- `trainz12-x64/ghidra_maps/jet/graphics/Render3DFoundation_7c.dll.functions.tsv` (1642 rows incl header)
- `trainz12-x64/ghidra_maps/jet/graphics/vtnames_Render3DFoundation_7c.dll.tsv` (220 rows incl header)
- `D:\trainz12-decomp\exports\Render3DFoundation_7c.tsv` (1084 exports)
- Peer context samples: `ghidra_maps/.../Render3DCore_7c.dll.functions.tsv`, `Render3DExtension_7c.dll.functions.tsv`, `RenderOpenGL_7c.dll.functions.tsv`, `RenderDirectX9_7c.dll.functions.tsv`
- Repo scan: no `src/jet/graphics/Render3DFoundation_7c` exists yet; no import-map files for this module found under `trainz12-x64`/`trainz12-decomp/exports`.

## 3) Inventory

### 3.1 Quantitative summary

| Metric | Value | Evidence |
|---|---:|---|
| Exported names | 1084 | `exports/Render3DFoundation_7c.tsv` |
| Ghidra functions | 1642 | `...functions.tsv` |
| Named-namespace functions | 1019 (165,469 B) | namespace != `Global` |
| `Global` functions | 623 (34,532 B) | namespace == `Global` |
| Total mapped function bytes | 200,001 B | sum of size column |
| Vftable exports | 43 | mangled `??_7*` |
| Vbtable exports | 2 | mangled `??_8*` |
| Ctor exports | 87 | mangled `??0*` (heuristic) |
| Dtor/ deleting-dtor exports | 46 | `??1/??_G/??_E/??_D` (heuristic) |
| Vtable slot name entries | 220 | `vtnames_...tsv` |
| Shared slot entries (same addr/name mapped to multiple vtable slots/classes) | 133 | multi-class `all_vftable_slots` |

Notes: `Global` functions are still attributed in Ghidra as `Global/FUN_*` in this map and are the main unattributed-code pool for this module.

### 3.2 Top namespace clusters (by function bytes)

| Namespace | funcs | bytes | Notes |
|---|---:|---:|---|
| `Global` | 623 | 34,532 | helpers/thunks/unattributed logic to classify |
| `Jet::GeometryConfigData` | 82 | 29,852 | largest named cluster; config/token/definition parsing & accessors (exports show many `GeometryConfigDataTokens::*_TOKEN`) |
| `Jet::VolumeMesh` | 30 | 14,002 | shadow-volume mesh construction/render helpers |
| `Jet::ShadowVolumeGeom` | 28 | 13,289 | edge/face extrusion, visibility, influence/shadow-volume geometry |
| `Jet::AnimationModifier` | 86 | 11,705 | animation playback/modifier state, events, bones, observers |
| `Jet::TextureProjector` | 37 | 9,443 | projector setup/cull/draw/state; paired with occluded variant |
| `Jet::AnimationModifierLoader` | 22 | 9,260 | loading/instantiation support for animation modifiers |
| `Jet::ShadowVolume` | 16 | 8,719 | per-light/object shadow volume object logic |
| `Jet::MovieCanvas` | 32 | 7,474 | movie playback/render surface control |
| `Jet::Animator` | 27 | 5,064 | animation add/get/attach/results API |
| `Jet::GeometryDefinitionGeneric` | 44 | 4,830 | generic geometry definition data |
| `Jet::InfluenceBone` / `Jet::InfluenceMesh` | 16 / 15 | 4,163 / 2,993 | skinning/influence data |
| `Jet::MovieTextureResource` | 48 | 3,924 | `.texture`-like movie texture resource behavior (ResourceObserver/Textur eResource-like exports) |
| `Jet::OccludedTextureProjector` | 12 | 3,858 | occlusion-aware projector variant |
| `Jet::MovieTexture` | 44 | 3,497 | texture facade over movie playback |
| `Jet::MaterialConfigData` / `Jet::TextureConfigData` | 13 / 8 | 2,966 / 2,862 | material/texture config records |
| `Jet::ShadowProgramStore` / `Jet::ShadowCaster` / `Jet::ShadowVolumeGenerator` / `Jet::ShadowCache` | 9 / 19 / 15 / 22 | 2,258 / 2,226 / 1,848 / 1,345 | shadow program selection/caching/generation pipeline |
| `Jet::ParticleBone` | 65 | 1,726 | many small methods/slots; particle-bone integration |

(See `functions.tsv` for full list; only top clusters shown.)

### 3.3 Exported/vtable-visible class families (representative)

**Animation & playback/modifiers**
- `Jet::AnimationEvent`, `Jet::AnimationModifier` (+ `BoneData`), `Jet::AnimationObserver`, `Jet::AnimationModifierLoader`
- Resources: `Jet::AnimationResource`, `Jet::AnimationTxtResource`
- Runtime/controllers: `Jet::Animator`, `Jet::AnimatorChannels`, `AnimationMaintenanceObserver` (slot names in vt map)
- Resource/plugin types exported: `Jet::ResourceType<Jet::AnimationResource>`, `Jet::ResourceType<Jet::AnimationTxtResource>`

**Movie subsystem**
- `Jet::MoviePlayer` (plugin-instance style: multiple vftables incl `for Jet::PluginInstance`, vbtable exported; `CreateInstance(OS*)`, play/pause/stop/loop/time state, `uid`, `results`)
- `Jet::MovieStream`, `Jet::MovieJetFileReader` (file/media subtype handling; `MEDIASUBTYPE_*` statics exported)
- `Jet::MovieTextureResource` (ResourceObserver + TextureResource-facing vftables), `Jet::MovieTextureResourceType`
- `Jet::MovieTexture`, `Jet::MovieCanvas`, `Jet::RenderTexture`

**Geometry / influence / particles / config-data**
- `Jet::GeometryBaseResource`, `Jet::GeometryConfigData` (+ many `GeometryConfigDataTokens::*` data exports), `Jet::GeometryDefinitionBase/Generic`, `Jet::GenericMeshAppData`
- `Jet::InfluenceDefinition`, `Jet::InfluenceBone`, `Jet::InfluenceMesh`, `Jet::ParticleBone`
- `Jet::MaterialConfigData`, `Jet::TextureConfigData`, `Jet::MeshOverrides`, `Jet::MeshFileSupport` (operator export seen), `Jet::ObjectLOD`, `Jet::LightTarget`, `Jet::VolumeMesh`

**Shadows & projection**
- Shadow objects/generation: `Jet::Shadow`, `Jet::ShadowBlob`, `Jet::ShadowGenerator`, `Jet::ShadowBlobGenerator`, `Jet::ShadowVolumeGenerator`, `Jet::ShadowCaster`, `Jet::ShadowVolume`, `Jet::ShadowVolumeGeom`, `Jet::ShadowInfluenceMesh`, `Jet::ShadowDarkLayer`
- Shadow geometry/chunks/programs/cache: `Jet::ShadowBaseChunk`, `Jet::ShadowSWChunk`, `Jet::ShadowHWChunk`, `Jet::ShadowProgramStore`, `Jet::ShadowCache` (has RC+RenderingCache vftables and vbtable), `Jet::ShadowTypes`
- Projectors: `Jet::TextureProjector`, `Jet::OccludedTextureProjector`, `Jet::ProjectorCache` (present in function map; exports include projector methods/statics)

**Plugin entry**
- `JetGetPluginsRender3DFoundation` (global export) — likely registers resource types/factories (animation txt/resource, movie texture resource type, etc.). Exact registration contents require decompile/RE of the function.

### 3.4 Vtable/ABI clusters of note (`vtnames_Render3DFoundation_7c.dll.tsv`)

High slot counts (complex/large interfaces): `Jet::ShadowBaseChunk` (51), `Jet::ParticleBone` (44), `Jet::MovieTexture` (16), `Jet::AnimationResource` (15), `Jet::AnimationModifier` (13), `Jet::MovieTextureResource` (10), `Jet::MovieCanvas` (9), `Jet::Texture` (7 named in map), `Jet::ShadowCache` (6).

**Observed slot-sharing patterns (evidence of MI/shared implementations/adjustors in-module):**
- `ShadowBaseChunk` shares many slot addresses with `ShadowSWChunk` / `ShadowHWChunk` (common base/chunk interface behavior).
- `ParticleBone` shares many slot addresses with `TextureProjector` and `OccludedTextureProjector` (particle/projector interface overlap in vt naming).
- `MovieTexture` shares some slot names/entries with `Jet::Texture` in the vt file; `AnimationResource` with `AnimationTxtResource`/`GeometryBaseResource`/`MovieTextureResource` for some slots — **hypothesis:** common base/Resource/Texture-style virtual slots resolved to shared implementations or naming artifacts in the vt map; confirm against actual vtable contents/layout during implementation.
- Multiple inheritance / secondary bases indicated by dual vftables and vbtable exports: `Jet::MoviePlayer` (`for MoviePlayer`, `for PluginInstance`, `vbtable`), `Jet::ShadowCache` (`for ReferenceCount`, `for RenderingCache`, `vbtable`), `Jet::TextureProjector` / `Jet::OccludedTextureProjector` (`for Particle`, `for Projector`), `Jet::MovieTextureResource` (`for ResourceObserver`, `for TextureResource`).

## 4) Dependencies

### 4.1 Engine dependencies (evidenced by signatures / Jet architecture)

Exports/functions reference types and services provided elsewhere in Jet (primarily `RenderSystem_7c`, plus Kernel*): `Jet::Display`, `Jet::RenderKernel`, `Jet::ResourceDatabase`, `Jet::Texture`, `Jet::TextureManager`, `Jet::TextureResource`, `Jet::DisplayTexture`, `Jet::DisplayFormat`, `Jet::Material`, `Jet::GeometryChunk`, `Jet::Geometry`, `Jet::VertexProgram`/vertex types, `Jet::LayerGroup`, `Jet::Particle`/`SkeletonModifier`/`Light`/`Camera`/`ViewPort`/`Matrix4`/math types, `Jet::PString`, `Jet::ConfigData`, `Jet::OS`, `Jet::ResultClass`, `Jet::File`/bitmap readers, plugin/resource base classes (`PluginInstance`, `Resource`, `ResourceObserver`, `ResourceType<>`). 

**Implication:** `RenderSystem_7c` API/ABI and core resource/material/geometry contracts are a hard prerequisite for correct behavior; Kernel/KernelStandard/KernelResource/KernelResources provide memory/strings/files/config/resources.

### 4.2 Inter-render-module relationships (coordination-relevant)

- **`RenderSystem_7c` (in-flight, U7 partial in tree):** foundational dependency and main ABI contract. Coordinate on `Texture/TextureResource/Movie` texture interactions, render-target/display hooks, geometry/vertex-program usage, and any assumptions about manager singletons/static state.
- **`Render3DCore_7c` / `Render3DExtension_7c` (not started):** adjacent 3D layers; peer maps show different feature sets (e.g. alpha-blend cache, decals in core/extension samples). Potential shared base types/data imports or consumer/producer relationships (e.g. shadow/projector/LOD integration) — **hypothesis, needs cross-import evidence**.
- **`RenderOpenGL_7c` / `RenderDirectX9_7c`:** backend modules implement `Display`/API objects; Foundation calls through abstract Display/RenderKernel/texture/material interfaces (no backend-specific code expected, but verify).
- **`WaterSystem_7c` (partial):** may interact with materials/geometry/render passes indirectly; coordinate only if shared runtime objects collide.
- **`DSMoviePlayer_7c` (not loaded at menu per README):** movie playback in Foundation (`MoviePlayer/MovieStream/MovieJetFileReader`) may wrap/replace or interoperate with it — **unknown; requires decompile/runtime tracing**.
- **Game/consumers:** README import counts show broad consumers of RenderSystem; `trainz.exe` and other modules likely consume Foundation exports too (not enumerated here) — treat exported ABI/layouts as externally visible.

### 4.3 Data/static dependencies

- 43 vftables, 2 vbtables and numerous data exports (tokens/statics like `GeometryConfigDataTokens::*`, `MoviePlayer::uid/results`, shadow static state, unique IDs/counters implied by names) must match original layout/ownership semantics. Shared/module statics and plugin registration state must be planned carefully (RenderSystem notes show many statics kept original until late units).

## 5) Proposed source layout and unit/order (planning)

**Goal:** mirror existing repo patterns (`notes/RenderSystem.md`, `notes/WaterSystem.md`, `src/jet/graphics/*`) — proxy/forwarding first if used by project, then leaf/value/data units, resources/loaders, feature subsystems, registration/foundation last; differential tests per unit where feasible.

> Unit boundaries below are a **planning hypothesis** from namespace/function clustering and export families; refine after decompile/import analysis and attribution of `Global` helpers.

| Proposed unit | Scope (candidate classes/areas) | Rationale / deps | Testability notes |
|---|---|---|---|
| U0 | Forwarding/proxy skeleton + export/def/vtable/data plumbing for all 1084 exports | Establish drop-in ABI surface early (per module workflow) | link/load, export presence, thunk checks |
| U1 | Pure data/config tokens and value records: `GeometryConfigDataTokens` statics/data, small POD-ish records (`LightTarget`, parts of `MeshOverrides`, ID structs) | Lowest behavior risk; many config systems depend on keys | static/data equality, token string/identity checks |
| U2 | Config-data containers/parsers: `TextureConfigData`, `MaterialConfigData`, `GeometryConfigData`, `GeometryDefinitionBase/Generic`, `GenericMeshAppData` | Central 30KB cluster; feeds geometry/material/animation setup | parse real `.txt`/config assets from game/content; compare structures field-by-field |
| U3 | Influence/mesh helpers: `InfluenceDefinition`, `InfluenceBone`, `InfluenceMesh`, `MeshFileSupport`-related pieces (as attributable) | Needed by animation/shadow skinning paths | synthetic + real mesh/influence data |
| U4 | Animation core: `AnimationEvent`, `AnimationModifier`/`BoneData`, `AnimationObserver`, `AnimationResource`, `AnimationTxtResource`, `ResourceType<AnimationResource/Txt>` | Core runtime + resources; moderate isolation | load `.animation/.txt` assets if present; event/timing/bone state diffs |
| U5 | Animator/controllers: `AnimationModifierLoader`, `Animator`, `AnimatorChannels`, `AnimationMaintenanceObserver` | Builds on U4; integrates with skeleton/resource DB | scripted add/play/seek scenarios vs original |
| U6 | Movie I/O and playback core: `MovieStream`, `MovieJetFileReader`, `MoviePlayer` (+ plugin vftables/vbtable) | Needed before movie textures/canvas; platform/format sensitive | file-open/metadata/state-machine tests with sample media; may need stubs for OS/codec paths |
| U7 | Movie textures/canvas/render target: `MovieTextureResource`/Type, `MovieTexture`, `MovieCanvas`, `RenderTexture` | Depends on U6 and RenderSystem texture/display APIs | stand-in Display/TextureManager, frame/update/bind logging |
| U8 | Projectors and render-texture helpers: `TextureProjector`, `OccludedTextureProjector`, `ProjectorCache` | Mid complexity; interacts with culling/render state | matrix/cull/draw-call comparison against stand-in Display/RenderKernel |
| U9 | Shadow primitives/program store: `ShadowTypes`, `ShadowProgramStore`, `ShadowDarkLayer`, shadow chunk classes (`ShadowBaseChunk/SW/HW`), `ShadowInfluenceMesh` | Foundation for volume/blob generation | vertex/chunk buffer contents, program selection, API-call traces |
| U10 | Shadow geometry & generation: `ShadowVolumeGeom`, `VolumeMesh`, `ShadowVolume`, `ShadowGenerator/BlobGenerator/VolumeGenerator`, `ShadowCaster`, `ShadowBlob`, `ShadowCache`, `ObjectLOD`/integration pieces as needed | Highest math/algorithm concentration (14KB+13KB clusters) | geometry edge/volume output diffs on deterministic fixtures; perf/float sensitivity risk |
| U11 | Particles/remaining mesh glue: `ParticleBone`, any remaining `GeometryBaseResource`/mesh-instance helpers, unattributed `Global` helpers assigned by call graph | Tie-ins across animation/shadow/geometry | targeted per-API tests once owners known |
| U12 | Plugin registration/final statics: `JetGetPluginsRender3DFoundation`, resource-type registrations, module-global state ownership | Last, once factories/classes exist | plugin enumeration/registration parity, in-game menu/session checks |

Suggested implementation order inside clusters: constants/tokens -> data/layout -> loaders/parsers -> pure logic -> rendering/display-integrated behavior -> managers/caches/statics/registration.

## 6) Risks and data needs

### 6.1 Technical risks

- **ABI/layout fidelity:** 43 vftables, 2 vbtables, MI classes (MoviePlayer, ShadowCache, Projectors, MovieTextureResource) and many exported ctors/dtors/operators require exact object layouts, adjustor thunks, vtordisps, and vtable ordering.
- **Unattributed code:** 623 `Global` functions (34.5 KB) must be attributed to owning units; misattribution can break ordering/tests.
- **Math/float determinism:** shadow volume generation, projector transforms, animation interpolation likely sensitive to x87/SSE evaluation order and NaN/edge-case behavior (cf. RenderSystem notes).
- **Heavy external coupling:** broad use of `Display`, `RenderKernel`, `TextureManager`, geometry/material/vertex-program APIs; behavior may depend on subtle RenderSystem contracts or call ordering.
- **Movie subsystem platform deps:** `MoviePlayer/MovieStream/MovieJetFileReader` may depend on OS facilities, codecs, or DirectShow/related components; `DSMoviePlayer_7c` role unclear.
- **Config/asset parsing quirks:** `GeometryConfigData`/resource loaders likely contain format-specific and legacy quirks; need byte/structure-level parity.
- **Shared statics/singletons:** caches/program stores/projector caches/shadow caches and exported data symbols may be read/written across modules; premature relocation can regress other modules.
- **RenderSystem in-flight:** `COORDINATION.md` flags active RenderSystem U7 work; avoid depending on unstable internal implementation details.

### 6.2 Data / environment needs for future implementation

- Decompiled/annotated bodies for key clusters (especially `GeometryConfigData`, shadow geom/volume, animation loader/modifier, movie player/resource) — **not present in repo**.
- Import table / cross-module xref dump for `Render3DFoundation_7c.dll` to confirm dependencies and data-imported statics/ownership.
- Real asset corpus from a licensed Trainz 2012 install: animation resources (`.animation`, `.txt` variants if applicable), movie assets/formats used by `MoviePlayer`, geometry/material/texture config files, meshes exercising influences/LOD/shadows/projectors.
- Differential-test harness utilities: stand-in `Display`/`RenderKernel`/`ResourceDatabase`/`TextureManager` with call/state logging (extend patterns from RenderSystem/WaterSystem tests).
- In-game validation assets/scenes: menu load, content with animated objects, projected textures/decals-if-related, dynamic shadows, and movie-textured surfaces (post-early units).
- Confirmation of `JetGetPluginsRender3DFoundation` registration side effects and expected factory/resource-type IDs/names.

### 6.3 Suggested validation strategy (future, when implementing)

1. U0 export/ABI parity: all 1084 exports present, ordinals/names/demangles match, vtable/vbtable/data symbols resolved correctly; `check_thunks.py`/`compare_vtables.py` style checks.
2. Per-unit differential tests (our DLL + original in-process) on deterministic inputs and real assets; compare parsed structures, runtime state, and API-call sequences.
3. Progressive in-game smoke: startup/menu (plugin registration), then targeted scenes for animation, movies, shadows/projectors.

## 7) Cross-agent notes (related render modules)

- **`RenderSystem_7c` (owned/in-flight per coordination):** primary dependency. Do not modify RS code/tests/docs; align to its exported contracts only. Reuse/extend its stand-in Display/RenderKernel/test patterns for Foundation diff tests. Watch for shared concepts (`Texture`/`RenderTextureConstruct`/material-geometry lifetimes/vertex programs) and any exported statics Foundation may read.
- **`Render3DCore_7c`:** likely peer/consumer/neighbor in 3D pipeline (AlphaBlendCache and scene/render caches visible in its map sample). Coordinate class/layout ownership and avoid duplicating or reinterpreting shared abstractions without evidence.
- **`Render3DExtension_7c`:** small extension module (Decal/DecalCache prominent in sample). May depend on Foundation or RenderSystem services for decals/extended rendering; align registration and shared utility expectations.
- **`RenderOpenGL_7c` / `RenderDirectX9_7c`:** backend implementers behind Display abstraction; Foundation should remain backend-agnostic except via RenderSystem interfaces. Coordinate if projector/shadow/render-texture paths require specific backend capabilities.
- **`WaterSystem_7c`:** separate partial module; no direct conflict expected, but integration scenes may exercise common render/material paths.
- **Global coordination:** respect `notes/agents/COORDINATION.md` (no shared source/build/doc edits, no builds/tests/commits in this analysis task). Any future claim on implementation should be recorded in the coordination board before touching shared files, especially given RenderSystem in-flight state.

## 8) Open questions / next analysis steps (recommended)

1. **Decompile and attribute `JetGetPluginsRender3DFoundation`** to enumerate registered types/factories and initialization order.
2. **Produce an import/xref profile** (which RenderSystem/Kernel/etc. exports and data symbols are consumed) to firm up dependency ordering and static ownership.
3. **Map `Global` 623 functions** to classes/subsystems by call graph, strings, and prolog patterns; split into unit-owned helper files.
4. **Recover concrete object layouts** for MI classes (`MoviePlayer`, `ShadowCache`, `TextureProjector/OccludedTextureProjector`, `MovieTextureResource`, `Shadow*Chunk`) from ctors/vftables/field accesses.
5. **Identify asset formats exercised** by animation/movie/geometry config loaders using real game content samples (read-only external analysis).
6. **Clarify boundary with Render3DCore/Extension** via export-import overlap and runtime usage once other agents publish their reports.

## Scaffold (Wave 2/3)

**Date:** 2026-11-19 (scaffold prep, module-owned only; no shared files/build/test/game touched)

### Files created
- `trainz12-x64/src/jet/graphics/Render3DFoundation_7c/implemented.txt`
  - Proxy-first manifest header: forwards all exports by default when wired; **no exports enabled**.
  - Records evidence: 1084 exports (ord 1..1084), 1642 funcs (623 Global/1019 named, 200001 mapped bytes), 220 vt slot entries / 43 vftable+2 vbtable export families.
  - Evidenced entrypoint: `JetGetPluginsRender3DFoundation` (ord 1084, RVA 0x218a0, 40 bytes, Global/IMPORTED in function map) — registration side effects unknown, **not** treated as safe to stub/enable.
- `trainz12-x64/src/jet/graphics/Render3DFoundation_7c/Render3DFoundation.h`
  - Documentation-only scaffold header listing evidenced module role and major clusters: animation/modifier/animator/loaders/resources, movie player/stream/reader/texture/canvas/render texture, geometry/influence/LOD/mesh/config-data/tokens, shadows (volumes/blobs/chunks/generators/casters/cache/program store/types) and projectors/render targets; notes MI/vbtable/large-vt families and slot-sharing evidence.
  - Declares `extern "C" void JetGetPluginsRender3DFoundation()` as evidenced export (ABI/side effects TBD); explicitly forbids guessed ABI/layout/vtable/slot/order/behavior.
- `trainz12-x64/src/jet/graphics/Render3DFoundation_7c/Render3DFoundation.cpp`
  - Placeholder implementation with staged TODOs only; `JetGetPluginsRender3DFoundation` body intentionally left unimplemented/unexported (no-op would drop required registrations). No behavior guessed.

### Stubbed vs deferred
- **Stubbed (enabled):** none. All 1084 exports remain forwarded/proxy-first by policy and lack of trivially-safe, behavior-independent evidence.
- **Declared only (not exported/implemented):** `JetGetPluginsRender3DFoundation` (known name/ordinal/RVA/size) — deferred until registrations/order/IDs/side effects recovered from decomp.
- **Deferred (all subsystems):** animation/event/modifier/observer/resource/loader/animator, movie I/O/playback/textures/canvas, geometry/influence/particle/LOD/config-data, shadow generation/volume/blob/chunk/cache/caster/program/projector subsystems, MI layouts/vtables/vbtables/adjustors, exported data/statics/tokens, and 623 unattributed `Global` functions pending attribution.

### Key ABI/dependency TODOs and risks
- **ABI/layout/vtable:** recover exact object layouts, calling conventions, vtable/vbtable slot order, adjustor thunks, RTTI/data-symbol layout for 43 vftables/2 vbtables and MI classes (`MoviePlayer`, `ShadowCache`, `TextureProjector/OccludedTextureProjector`, `MovieTextureResource`, `Shadow*Chunk` families, etc.); 220 vt slot entries show extensive sharing that must be validated against real tables.
- **Unattributed code/data:** attribute 623 `Global` functions (34.5 KB) and confirm ownership of exported statics/tokens (e.g. `GeometryConfigDataTokens::*`, movie/shadow/cache state) before moving state.
- **Entry/registration:** decompile `JetGetPluginsRender3DFoundation` for factory/resource-type registrations and init order before any local enablement.
- **Dependencies/contracts:** strong coupling to RenderSystem_7c and Kernel* APIs/types; relationships with Render3DCore/3DExtension, render backends, DSMoviePlayer, WaterSystem, and game consumers need import/xref confirmation. RenderSystem U7 is in-flight (per coordination) — do not assume unstable internals.
- **Behavior sensitivity:** shadow/projector/animation math and movie/platform paths may be float- and environment-sensitive; config/asset parsing likely has format quirks. 3D stack is large (1084 exports) so misordered ABI/static ownership is high-risk.

### Later build-wiring steps (under separate claim)
1. Claim wiring scope in `notes/agents/COORDINATION.md` and ensure RenderSystem/shared-file state is stable/non-conflicting.
2. Add module target/source wiring in shared build files (e.g. `src/jet/CMakeLists.txt`) and any proxy/forwarding/DEF-export mechanism consistent with repo conventions, keeping all exports forwarded initially.
3. Add/align progress/export tracking entries (`tools/progress_exports.tsv`, `README.md`, `PORT_PLAN.md`) only as part of that claimed wiring/implementation step.
4. Enable individual exports in `implemented.txt` only after decomp-verified signatures/layouts/behavior/registration and differential checks; keep `JetGetPluginsRender3DFoundation` forwarded until proven. No builds/tests/game runs performed in this scaffold task.
