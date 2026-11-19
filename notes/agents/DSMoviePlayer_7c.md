# DSMoviePlayer_7c analysis (read-only)

Date: 2026-11-19
Scope: analysis/planning only; no source/build/doc edits outside this note.
Assignment context: unfinished Jet module **DSMoviePlayer_7c** (group: `not_loaded`), Trainz 2012 64-bit port project.

## Evidence reviewed

- `README.md` (progress table/groups): DSMoviePlayer_7c listed as **58 KB, 728 functions, 0/1 exports, not started** under **Not seen loaded**; group note: "movie player (not loaded at the menu; re-check in a session)".
- `PORT_PLAN.md`: "Modules not seen loaded (Lexer, movie players) come last or not at all." (Phase 2 granularity/status notes).
- `MODLOG.md` grep hits:
  - "with one export (RenderDirectX9 21%, InputDirectX 25%, DSMoviePlayer 26%): named via vtables/RTTI later."
  - "not in trainz.exe's static import table and were not seen loaded at the menu (same process-probe that caught every other module): ... DebugSupport, **DSMoviePlayer**, VFWMoviePlayer, Render3DExtension, RenderDirectX9. Re-check inside a driving session."
- `notes/agents/COORDINATION.md`: RenderSystem_7c in-flight/uncommitted; WaterSystem_7c has known pre-existing build issue (implementation coordination context only).
- Exports: `D:\trainz12-decomp\exports/DSMoviePlayer_7c.tsv` (and repo copy context): **1 export**, ordinal 1, RVA `0x28d0`, mangled/demangled `JetGetPluginsDSMoviePlayer`.
- Ghidra maps:
  - `ghidra_maps/jet/not_loaded/DSMoviePlayer_7c.dll.functions.tsv` (759 rows, code bytes total 59,543)
  - `ghidra_maps/jet/not_loaded/vtnames_DSMoviePlayer_7c.dll.tsv` (330 rows, 102 vftables, 331 functions in vftables per NameVtableFunctions log)
  - `D:\trainz12-decomp\out\ExportFunctionStats_DSMoviePlayer_7c.dll.log`: `functions=759 named=527 (69.4%) thunks=31 code_bytes=59543`
  - `D:\trainz12-decomp\out\NameVtableFunctions_DSMoviePlayer_7c.dll.log`: `vftables=102 functions_in_vftables=331 renamed=330`
- Repo scans: no existing `notes/*movie*`; no `JetGetPluginsDSMoviePlayer`/movie-player implementation references under `src/jet/**` in this workspace; comparable plugin-export patterns exist in `Kernel_7c`, `KernelResources_7c`, `Render2DCore_7c`, `WaterSystem_7c` (all via `JetGetPlugins*` registering factories).

## 1. Overview / likely role

**Assessment (evidence-based):** DSMoviePlayer_7c is a **Jet plugin DLL implementing DirectShow-backed movie playback** and exposing it to the engine through the standard Jet plugin entrypoint.

Why:
- Single exported symbol is `JetGetPluginsDSMoviePlayer` (`exports/DSMoviePlayer_7c.tsv`), matching the Jet plugin registration convention used by other modules (`extern "C" void __cdecl JetGetPlugins*()` registering factories/instances).
- Jet/plugin classes present in maps: `Jet::DSMoviePlayer` (4 vf entries: `vf0`/`vf1` at `10001dc0/100019a0` and small wrappers at `10002740/10002730`), `Jet::PluginFactoryOf<Jet::DSMoviePlayer,Jet::MoviePlayer>` (`vf0-vf3`, including thunks at `10004b34/10004b3a` and implementations at `100029e0/10002960`), `Jet::MovieJetFileReader` (7 slots, mostly thunks), `Jet::DSMovieReader` (9 slots), `Jet::DSMovieOutputPin` (29 slots), `Jet::DSVideoRenderer` (8 slots), `Jet::DSVideoInputPin` (2 slots), `Jet::ReferenceCount`.
- Large body of DirectShow/base-filter-style classes inferred by naming: `CDynamicOutputPin` (52), `CBaseRenderer` (52), `CPosPassThru`/`CRendererPosPassThru`, `CRendererInputPin` (31), `CBaseVideoRenderer` (29), `CMediaSample` (22), `CMemAllocator` (14), `CEnumPins`/`CEnumMediaTypes` (8 each), `CDeferredCommand` (6). Vtable slot sharing across `C*` and `Jet::DS*` names in `vtnames` is consistent with COM-style interfaces/multiple-inheritance/filter-graph pin/renderer plumbing (exact interface IDs/signatures not proven here).
- External thunk to `RENDER3DFOUNDATION_7C.DLL::Jet::MoviePlayer::SetLooping` (`10004b28`) indicates integration with a Jet movie-player abstraction defined/owned outside this DLL (Render3DFoundation side).

This is not evidence of current runtime use in normal menu flow; it is evidence of capability/implementation domain.

## 2. Inventory (from available maps/logs)

### 2.1 Export surface
- `JetGetPluginsDSMoviePlayer` @ RVA `0x28d0` (ordinal 1). No other public exports listed.

### 2.2 Function-map summary (`DSMoviePlayer_7c.dll.functions.tsv`)
- Rows: **759**; total code bytes: **59,543**; non-unwind rows **584 / 57,718 bytes**.
- Naming/sources: **527 named (69.4%)**, 232 `DEFAULT`, 525 `ANALYSIS`, 2 `IMPORTED`; **31 thunks**.
- Notable buckets (approx, by category heuristic): DirectShow/base-filter impl 265 funcs / 21,481 bytes; global unknown `FUN_*` 210 / 25,675 bytes; Jet/plugin/movie impl 66 / 8,073 bytes; unwind 175 / 1,825 bytes; CRT/support 20 / 2,204 bytes; CRT imports 16 / 96 bytes; global named/support 1 / 157 bytes; small thunks 4 / 20 bytes.
- Unnamed work remains substantial (`FUN_*` + unnamed globals/support around filter-graph/media logic), though vtable naming coverage is relatively good.

### 2.3 Jet/plugin-facing entities (map evidence)
- `Global::JetGetPluginsDSMoviePlayer` (`100028d0`, 143 bytes, IMPORTED in map/exported).
- `Jet::PluginFactoryOf<Jet::DSMoviePlayer,Jet::MoviePlayer>` vf3 `10002960` (126), vf2 `100029e0` (36), vf0/vf1 thunks `10004b34/10004b3a`.
- `Jet::DSMoviePlayer` vf0 `10001dc0` (559), vf1 `100019a0` (909), plus `10002740` (20), `10002730` (8).
- `Jet::DSMovieReader` vf0 `100079d0` (209), vf3 `10001190` (36), vf4-vf7/vf11 etc. across `10001250..10007ec0` (9 entries total).
- `Jet::MovieJetFileReader` vf1 `10001d90` plus vf2-vf7 as thunks at `10004b22/16/1c/04/10/0a`.
- `Jet::DSMovieOutputPin` 29 vfunc entries (sizes from ~3 to 380 bytes; notable larger: vf7 `10003550` 380, vf6 `10003190` 344, vf4 `10003040` 326, vf5 `10003400` 326, vf9 `10003960` 285).
- `Jet::DSVideoRenderer` vf23 `10004230` (331), vf43 `10004380` (1395, largest Jet-labeled routine), vf44 `100040a0` (174), vf36 `10004150` (183), vf7 `10004940` (191), vf24 `10004900` (61), vf13 `10004210` (32), vf3 `10003e10` (36).
- `Jet::DSVideoInputPin` vf3 `10003c90` (35), vf13 `10003cc0` (37).
- `Jet::ReferenceCount` vf1 `10001000` (36), vf0 thunk `10004b2e`.

### 2.4 DirectShow/base-class entities (representative, from functions/vtables)
- `CBaseRenderer` (52 funcs) including large logic such as vf38/vf39/vf5/vf6 and vf13 variants.
- `CRendererInputPin` (31), `CBaseVideoRenderer` (29, includes very large vf23/vf48/vf45 paths), `CDynamicOutputPin` (52).
- `CMediaSample` (22, full set including allocation/flags/timing-style vfunc names), `CMemAllocator` (14), `CEnumPins`/`CEnumMediaTypes` (8 each).
- `CPosPassThru` (41), `CRendererPosPassThru` (2), `CDeferredCommand` (6).
- `vtnames` shows extensive slot aliasing (e.g. some entries shared across `CDynamicOutputPin`, `CRendererInputPin`, `Jet::DSMovieOutputPin`, `Jet::DSVideoInputPin`, and base renderer classes), consistent with inherited/COM-style vtable layouts in DirectShow baseclasses code; treat as map-level naming, not a confirmed ABI spec without deeper inspection.

### 2.5 External references / thunks (direct evidence)
- `JetGetPluginsDSMoviePlayer` (export/import record in map) @ `100028d0`.
- `KERNELSTANDARD_7C.DLL::Jet::Alloc::New` thunk @ `10002830`.
- `RENDER3DFOUNDATION_7C.DLL::Jet::MoviePlayer::SetLooping` thunk @ `10004b28`.
- MSVCR80 imports/thunks present (e.g. `operator_delete[]`, `memcpy`, `memset`, `_vsnprintf`, CRT init/lock/unlock/exception helpers, `type_info` dtor helper) — expected for VC8-era DLL.
- Other internal thunks: `thunk_FUN_10003b50`, `thunk_FUN_1000e9c0`, `thunk_FUN_10005180`, `thunk_FUN_1000a2d0`, and Jet/MovieJetFileReader/ReferenceCount/PluginFactory adjustor-like thunks.

## 3. Evidence of usage / load conditions

**Observed/documented:**
- `README.md`: module in **Not seen loaded** group; "movie player (not loaded at the menu; re-check in a session)".
- `PORT_PLAN.md`: "Modules not seen loaded (Lexer, movie players) come last or not at all."
- `MODLOG.md`: not in `trainz.exe` static import table and **not seen loaded at the menu** in prior process-probe context; explicitly advises **re-check inside a driving session** (listed alongside DebugSupport, VFWMoviePlayer, Render3DExtension, RenderDirectX9).

**Not evidenced in this repo snapshot:**
- No code references to `DSMoviePlayer_7c`/`JetGetPluginsDSMoviePlayer` under `src/jet/**`.
- No dedicated notes for this module in `trainz12-x64/notes` or `D:\trainz12-decomp\notes` (from scans performed).
- No runtime trace/log proving when it is requested (e.g. intro/cutscene/GUI movie/scripted playback scenario) — unknown.

**Planning implication:** load/use conditions remain **unconfirmed**. Any implementation/verification plan must include targeted runtime capture in plausible scenarios (boot/menu variants, session load, in-session events, any known movie-bearing content) rather than assuming menu-load coverage.

## 4. Dependencies and integration constraints

### 4.1 Direct evidence from maps
- **Jet kernel/plugin/runtime:** `Jet::Alloc` (KernelStandard), `Jet::ReferenceCount`, Jet plugin factory machinery (`PluginFactoryOf<Jet::DSMoviePlayer,Jet::MoviePlayer>`), `JetGetPluginsDSMoviePlayer` export contract.
- **Movie player API ownership outside DLL:** `RENDER3DFOUNDATION_7C.DLL::Jet::MoviePlayer::SetLooping` thunk implies this DLL implements or participates in the `Jet::MoviePlayer` interface whose base/related behavior lives in Render3DFoundation (repo status: `Render3DFoundation_7c` is 194 KB, 1453 functions, **0/1084 exports** started).
- **Windows media stack (strongly implied by class inventory):** DirectShow-style filters/renderers/pins/samples/allocators/enums/pos-pass-thru/deferred commands (`CBaseRenderer`, `CBaseVideoRenderer`, `CDynamicOutputPin`, `CRendererInputPin`, `CMediaSample`, `CMemAllocator`, etc.). Actual DirectShow COM interfaces/calls are not itemized in TSVs but class names and structure are characteristic; COM initialization/apartment model and filter graph behavior are likely requirements.
- **CRT/toolchain:** MSVCR80-era runtime symbols present.

### 4.2 Likely but not fully proven here
- Interaction with Jet file/resource abstractions for media sources (`Jet::MovieJetFileReader`, `Jet::DSMovieReader` names) — requires code-level confirmation.
- Specific codec/container/filter dependencies (system-installed decoders/renderers) — not derivable from maps alone.
- Threading model (decoder/filter threads, callback/render synchronization) — probable given DirectShow architecture but not evidenced in this note.

### 4.3 64-bit port considerations (risk-facing)
- COM/DirectShow APIs exist on x64 Windows, but **all internal object layouts, vtables, reference counting, pointer-sized members, and Jet/Render3DFoundation ABI contracts must be 64-bit correct**; this module is pointer/interface-heavy by nature.
- Cross-module interface parity with `Render3DFoundation_7c` (`Jet::MoviePlayer` including `SetLooping`) is mandatory; cannot be finalized in isolation while Render3DFoundation is unimplemented.
- Availability/behavior differences of codecs/filters/render paths between 32-bit and 64-bit environments may affect playback even if code is correct.
- Historically "not seen loaded" status reduces confidence in easy in-game validation; regressions may be latent.

## 5. Proposed implementation units / order (if/when prioritized)

This is a planning recommendation only (no implementation performed). Preserve Jet plugin/export ABI and behavior.

1. **U0: Scaffolding & contract capture (read-only prerequisite work)**
   - Confirm `Jet::MoviePlayer` interface/vtable/layout and related types in Render3DFoundation/Ghidra maps; locate callers/creation sites and expected plugin IDs/names/groups used by `JetGetPluginsDSMoviePlayer`.
   - Document expected behavior of `PluginFactoryOf<Jet::DSMoviePlayer,Jet::MoviePlayer>` (create/destroy/refcount/SetLooping linkage) and `MovieJetFileReader` semantics.

2. **U1: Plugin/export/factory wiring**
   - Recreate `JetGetPluginsDSMoviePlayer` and factory registration exactly (IDs/names/group as in original data).
   - Minimal `Jet::ReferenceCount`/`Jet::DSMoviePlayer` lifetime and factory interoperability; validate registration/discovery in isolation if harness exists.

3. **U2: Jet-side reader/player control surface**
   - `Jet::DSMovieReader` and `Jet::MovieJetFileReader` behavior, open/read/seek/teardown against Jet data sources.
   - `Jet::DSMoviePlayer` control methods (play/pause/stop/loop/state/query) consistent with `Jet::MoviePlayer` contract.

4. **U3: DirectShow graph components (incremental)**
   - Output/input pin classes (`Jet::DSMovieOutputPin`, `Jet::DSVideoInputPin`, `CDynamicOutputPin`, `CRendererInputPin`) and media type negotiation/enumeration (`CEnumPins`, `CEnumMediaTypes`, `CMediaSample`, `CMemAllocator`).
   - Renderer path (`CBaseRenderer`/`CBaseVideoRenderer`/`Jet::DSVideoRenderer`, largest/riskiest: `Jet::DSVideoRenderer::vf43` at 1395 bytes) and clock/position helpers (`CPosPassThru`, `CRendererPosPassThru`, `CDeferredCommand`).

5. **U4: Integration, resource/COM lifetime, and differential/runtime validation**
   - End-to-end graph build/run/teardown, looping (`SetLooping` dependency), synchronization, and error paths.
   - Differential comparison where feasible (original vs reconstructed behavior) and in-game scenario checks once a reliable trigger is identified.

Ordering rationale: stable export/factory and cross-module interface first, then data/control, then complex media pipeline internals, because verification depends on the former and the latter carries most ambiguity.

## 6. Priority / deferral recommendation

**Recommendation: defer (low near-term priority), but keep as a "needs runtime trigger confirmation" item.**

Rationale:
- Classified as **not_loaded** and **not seen at menu** (README/PORT_PLAN/MODLOG); may be unused in common flows or only loaded in specific scenarios not yet reproduced/documented.
- Only **1 export** and plugin-style surface suggests limited coupling at process startup, but runtime usefulness depends on actual playback requests.
- High implementation/verification cost: 759 functions / ~58 KB of mostly DirectShow/COM-style media code, 210 unnamed `FUN_*`, large renderer/filter logic, external codec/filter/system dependencies.
- Hard dependency/context coupling with **unstarted `Render3DFoundation_7c`** (`Jet::MoviePlayer` / `SetLooping` contract) blocks confident ABI-complete implementation.

**When to revisit / raise priority:**
- A concrete in-game/content trigger is identified (intro/cutscene/UI/movie-script path) that loads this DLL.
- `Render3DFoundation_7c` (and any owning movie-player interface) is sufficiently understood/implemented to define the contract.
- Media playback becomes required for parity in targeted scenarios.

## 7. Risks and unknowns

- **Trigger uncertainty:** no confirmed load/use scenario in available evidence; "re-check inside a driving session" is suggested but unverified here.
- **Interface/ABI unknowns:** exact `Jet::MoviePlayer` vtable/layout/semantics and plugin registration metadata (IDs/names/group) not extracted in this pass; Render3DFoundation dependency unresolved.
- **COM/DirectShow complexity:** graph negotiation, threading/apartment rules, sample timing, allocator contracts, and teardown ordering are error-prone and weakly reflected by names alone.
- **Unnamed logic:** 210 `FUN_*` functions and substantial global code remain unnamed; behavior recovery will require decompilation-level analysis.
- **Environmental dependence:** playback correctness may depend on installed 32-bit/x64 DirectShow filters/codecs and rendering path, not just DLL logic.
- **Potential confusion with other movie modules:** MODLOG also mentions `VFWMoviePlayer` as separate/not-loaded — features/triggers may differ; do not conflate without evidence.
- **Differential testing difficulty:** without a known deterministic trigger/content, automated original-vs-reimpl comparison is hard to scope.

## 8. Cross-agent notes

- **Read-only/no-write scope respected:** this report only adds `notes/agents/DSMoviePlayer_7c.md`.
- **Render3DFoundation_7c dependency:** coordinate before implementation; `RENDER3DFOUNDATION_7C.DLL::Jet::MoviePlayer::SetLooping` thunk and likely base interface ownership mean interface discovery there is a prerequisite.
- **Runtime validation owners:** whoever profiles in-game/module loads should explicitly test plausible movie scenarios and record whether `DSMoviePlayer_7c` (or `VFWMoviePlayer`) loads; update coordination/evidence if found.
- **No conflict with current in-flight work:** RenderSystem_7c is marked owned/in-flight in coordination snapshot; DSMoviePlayer_7c appears independent, but avoid touching shared build/docs/sources per constraints.
- **Not a substitute for Lexer/other not_loaded decisions:** separate inventories needed; deferral rationale here is module-specific.

## Summary

DSMoviePlayer_7c (58 KB, 759 funcs, 1 export `JetGetPluginsDSMoviePlayer`) presents strong static evidence of a **DirectShow-based Jet `MoviePlayer` plugin** (factory, Jet movie/reader/pin/renderer classes, DirectShow baseclass-style infrastructure) but is **not observed loaded at menu** and has unresolved trigger and Render3DFoundation interface dependencies. Recommended stance: **defer implementation pending runtime-trigger confirmation and MoviePlayer contract clarification**, while using this inventory to scope future work if playback support becomes necessary.
