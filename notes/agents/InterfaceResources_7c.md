# InterfaceResources_7c analysis (ui, unfinished)

- **Module:** `InterfaceResources_7c.dll` (Jet UI group)
- **Status (README, 2026-11-19):** 34 KB code, 345 functions, **0/169 exports implemented**, not started.
- **Task scope:** read-only analysis/planning only. No source/build/doc/game files modified; this is the only file written.
- **Primary refs consulted:**
  - `README.md` (progress/group table)
  - `MODLOG.md:1534-1540` (render/UI dependency census)
  - `PORT_PLAN.md:88,145` (UI/render ordering context)
  - `notes/agents/COORDINATION.md`, `notes/agents/STATUS.md` (no existing `InterfaceSystem` agent note present)
  - `D:\trainz12-decomp\exports\InterfaceResources_7c.tsv` (169 exported symbols)
  - `ghidra_maps/jet/ui/InterfaceResources_7c.dll.functions.tsv` (384 rows / 383 function entries)
  - `ghidra_maps/jet/ui/vtnames_InterfaceResources_7c.dll.tsv` (63 vtable-slot groups)
  - `ghidra_maps/jet/ui/InterfaceSystem_7c.dll.functions.tsv` and `D:\trainz12-decomp\exports\InterfaceSystem_7c.tsv` (consumer-side wrappers, sampled)
  - `D:\trainz12-decomp\jet-sdk\RC1\out\Inc\inc\ConfigInterfaceResources.h` (export macro only; no API declarations)
  - `notes/KernelResource.md`, `notes/RenderSystem.md` (resource/vtable/Bitmap context)

## 1. Overview / role

`InterfaceResources_7c` is the Jet UI **resource** module: it provides loadable/renderable UI assets and font infrastructure used by higher UI code, rather than the widget/window system itself.

Evidence from exports/functions:
- **Fonts/text:** `Jet::Font`, `Jet::FontInstance`, `Jet::FontInformation`, `Jet::FontResource`, `Jet::FontBaseResource`, font effects `Jet::FontFx`, `Jet::FontFxBitmap`, `Jet::FontFxTT` (glyph/ABC/cache/print/measure APIs).
- **UI resources:** `Jet::BorderResource` (+ `BitmapBorderResource` in function map), `Jet::CursorResource`/`CursorInstance`, `Jet::IconInstance` (+ `IconResource` in function/vtable map).
- **Plugin/resource registration surface:** `JetGetPluginsInterfaceResources` export; `Jet::FontResourceType` and `Jet::ResourceType<CursorResource|BitmapBorderResource|IconResource>` vfuncs present in maps/vtables.

Role in stack (evidenced): `MODLOG.md` records **`InterfaceSystem -> InterfaceResources 32`** imports (also InterfaceSystem imports RenderSystem/Render2DCore/InputSystem). This places InterfaceResources as a prerequisite/provider for InterfaceSystem's UI rendering primitives (borders/cursors/fonts/icons) rather than a peer of the widget logic. `PORT_PLAN.md` lists UI work under 8e (`InterfaceSystem, WaterSystem`) but does not detail InterfaceResources; given the dependency edge, InterfaceResources likely must be available (or stubbed compatibly) before/with InterfaceSystem integration.

## 2. Inventory

### 2.1 Export surface (`exports/InterfaceResources_7c.tsv`)
- **169 exports total** (ordinals 1..169). All currently unimplemented (README).
- Composition (from demangled names): ctors/dtors/op=/vftables for `BorderResource`, `CursorResource`, `CursorInstance`, `Font`, `FontBaseResource`, `FontFxBitmap`, `FontFxTT`, `FontInformation`, `FontInstance`, `FontResource`, `IconInstance`; extensive `Font`/`FontInstance`/`FontResource` measurement and print API (`GetW/GetH/GetWH/Print/Printf`, just/rect/color-list forms); font effects (`Init/Load/Done/Add/RemoveFx`, bitmap/TT char cache & ABC/raw image); cursor/border/icon operations (`CursorResource::{GetBitmap,GetArea,GetOffset,IsAllLoaded,LoadTxt}`, `BorderResource::Render`, `IconInstance::{Render,GetAutoSize,GetAutoHeight,GetW,Init}`, `FontResource::{Load,Unload,CalculateDataSize,CalculateSize,Create}` private-ish in map/export mix, `FontBaseResource::SetOnTheFly`); and `JetGetPluginsInterfaceResources`.

### 2.2 Function map (`ghidra_maps/.../InterfaceResources_7c.dll.functions.tsv`)
- **383 entries** (384 lines incl header). Namespace/class distribution (count, summed bytes):

| Namespace/class | Count | Bytes | Notes |
|---|---:|---:|---|
| Global | 151 | 8402 | helpers/CRT/unnamed `FUN_*`, plugin/import thunks |
| Jet::Font | 40 | 8580 | core text layout/render/measurement; largest single-function surface |
| Jet::FontResource | 26 | 2812 | resource lifecycle/load/create/unload + delegated print/measure |
| Jet::BorderResource | 23 | 283 | small base/resource-side methods (vtable logic largely elsewhere) |
| Jet::FontInstance | 21 | 1667 | instance wrapper delegating to Font/FontResource, name/color/flags |
| Jet::FontFxBitmap | 16 | 2489 | bitmap font effect/load/ABC/pos/cache |
| Jet::IconInstance | 15 | 730 | render/measure/init |
| Jet::FontFxTT | 15 | 1924 | TrueType effect load/init/cache/ABC/raw image |
| MSVCR80.DLL | 13 | 78 | CRT imports/thunks |
| Jet::CursorResource | 13 | 1718 | load/area/bitmap/offset/availability |
| Jet::FontFx | 10 | 80 | base effect interface/vfuncs |
| Jet::ResourceType<BitmapBorderResource> | 6 | 197 | resource type/factory vfuncs |
| Jet::IconResource | 6 | 1514 | non-exported/internal resource behavior (vf11/vf14/vf18/vf19/...) |
| Jet::BitmapBorderResource | 6 | 2857 | heavy render/load logic behind BorderResource |
| Jet::FontInformation | 6 | 790 | tag/color parsing/storage |
| Jet::FontBaseResource | 5 | 216 | base font-resource bits incl `SetOnTheFly` |
| Jet::CursorInstance | 4 | 59 | value/holder |
| Jet::ReferenceCount | 2 | 42 | refcount base vfuncs |
| Jet::FontResourceType | 2 | 575 | `vf3` (142 B), `vf5` (433 B) |
| Jet::ResourceType<CursorResource> | 1 | 111 | `vf3` |
| Jet::ResourceType<IconResource> | 1 | 112 | `vf3` |
| KERNELRESOURCES_7C.DLL::Jet::BitmapBaseInstance | 1 | 6 | imported symbol/thunk evidence |
| MSVCR80.DLL::type_info | 1 | 6 | RTTI support |

- **Largest/complexity hotspots (by function size):** `Font::Print` 1986 B, `BitmapBorderResource::vf18` 1494 B, `FontResource::Create` 1451 B, `CursorResource::LoadTxt` 1129 B, `Font::Print`(other overloads) 1022 B, `FontFxBitmap::Load` 877 B, `Font::CalcWidths` 740 B, `BitmapBorderResource::vf20` 663 B, `Font::GetWordWidth` 638 B, `FontInformation::ExtractTags` 623 B, `FontResource::Load` 604 B, `FontFxBitmap::LoadDR` 585 B, `FontFxTT::Load` 546 B, `IconResource::vf11` 473 B, `BitmapBorderResource::vf11/vf19` 465 B, `IconResource::vf19/vf18` 465/459 B, `Font::NewChar` 437 B, `FontResourceType::vf5` 433 B. These dominate behavior/risk (parsing, layout, raster/cache, resource creation).

- **Notable internal/non-exported classes in map:** `BitmapBorderResource`, `IconResource`, `FontResourceType`, `ResourceType<>` specializations, `ReferenceCount`, `BitmapBaseInstance` import — important for ABI/layout even if not exported by name.

### 2.3 Vtables / RTTI-ish map (`vtnames_InterfaceResources_7c.dll.tsv`)
- **63 slot-group entries**. Key observations:
  - `BorderResource`, `CursorResource`, `FontBaseResource`, `FontResource`, `BitmapBorderResource`, `IconResource` share many **Resource-like base slots** (`vf0..vf7, vf9, vf12, vf13, vf16, vf17` common patterns; `vf8` specialized per concrete resource). This is consistent with KernelResource's `Resource` base contract.
  - `CursorResource` adds `vf18..vf20`; `BitmapBorderResource` adds/overrides `vf11,vf14,vf18..vf20`; `IconResource` has specialized render/load slots (`vf8,vf11,vf14,vf15,vf18,vf19`).
  - `Font` has its own `vf0` (0x10002760); `ReferenceCount` base at 0x100095d4/vf1 0x10004e30.
  - `FontFx` hierarchy is split/multi-vtable (evidence of multiple inheritance / `PluginInstance` subobject): `FontFxBitmap` and `FontFxTT` have both `FontFx`-side and `PluginInstance`-side vtable entries/slots in exports+vtnames; `FontFx` base exposes `vf1,vf2,vf5,vf8..vf11`-like interface points.
  - `ResourceType<BitmapBorderResource|CursorResource|IconResource>` and `FontResourceType` have factory/type vtables (`vf0..vf5` family; `FontResourceType::vf5` large) — aligns with plugin/type registration via `JetGetPluginsInterfaceResources`.
  - `FontBaseResource` shows extended slot range (`vf18..vf32` mapped to a shared stub/address in vnames) — needs careful verification during implementation (could be no-op/defaults).

## 3. Dependencies and interfaces

### 3.1 KernelResource_7c (primary framework dependency)
- **Resource model:** resource classes/vtables mirror Resource-style slots (`Load/LoadTxt/Unload/CalculateSize/CalculateDataSize/IsAllLoaded/GetMustResetWhenFinished/OnLoadNotify` naming appears: e.g. `CursorResource::IsAllLoaded`, `FontResource::{Load,Unload,CalculateDataSize,CalculateSize,Create}`, shared `vf8/vf14/vf18+` specialization pattern). Correct behavior requires matching `Jet::Resource` ABI/layout/semantics (refs/locks/status/observers/db/name/load name/size caches) as documented in `notes/KernelResource.md`.
- **Type registration:** `ResourceType<>` specializations and `FontResourceType` indicate resources are exposed through KernelResource's type/plugin system; `JetGetPluginsInterfaceResources` is the likely registration/export entry point for the module.
- **Databases/names:** cursor/border/icon/font loading takes `ResourceDatabase*`/`PString` name/path forms in several APIs (`BorderResource::Render(..., ResourceDatabase const*, ...)`, `FontInstance/IconInstance::Init(db,name,...)`, `CursorResource/FontResource` ctors/load paths) — must interoperate with KernelResource DB lookup and name conversion rules.

### 3.2 KernelResources_7c (asset/content dependency)
- Function map shows imported `KERNELRESOURCES_7C.DLL::Jet::BitmapBaseInstance` (thunk). Cursor/bitmap-border/icon/font-bitmap paths strongly imply bitmap-backed resources loaded via KernelResources (e.g. `BitmapResource` instances) even where not named explicitly in exports.
- `BitmapBorderResource` and `FontFxBitmap` sizes/loads suggest decoding/compositing from bitmap assets; `BorderResource::Render` and `IconInstance::Render` operate on `Jet::Bitmap` targets, consistent with KernelResources/Render2DCore bitmap types.

### 3.3 KernelStandard_7c (core types/utilities)
- Demangled signatures evidence heavy use of: `PString`, `String`, `FastDynamicArray` (`FontColor` lists), `ResultClass<&Resource::results,...>`, `Rect/Point/Size`, `PackedColor`, `Bitmap`, `Just`, `Font::Flags/Font::ABC/CharPar/LineWidth`-related types, and path/text helpers implied by `LoadTxt/Load/Convert`-like behavior. Correct ABI/layout and helper semantics from KernelStandard are required.

### 3.4 Rendering / 2D surface (Render2DCore_7c, Bitmap API)
- APIs render/measure into or from `Jet::Bitmap` (`Font::{Print,Printf}`, `FontInstance/FontResource::Print*`, `IconInstance::Render`, `BorderResource::Render`, font effect cache/char pos/bitmaps). Module likely depends on Bitmap pixel/rect/blit operations provided by the 2D/render stack rather than owning a device. No direct RenderSystem/3D ownership evidenced in this module's exports; dependency is at the bitmap/primitive level (Render2DCore is done per README).

### 3.5 InterfaceSystem_7c (primary consumer; tight coupling)
- **Dependency edge:** `MODLOG.md:1538` — InterfaceSystem imports 32 symbols from InterfaceResources.
- **Name/wrapper correspondence (sampled):** InterfaceSystem exports `Jet::UI::Border`, `Jet::UI::Cursor`, `Jet::UI::IFont`, `Jet::UI::Icon` (ctors/dtors/assign/compare/bool/PString conversions/rect ops) and OSD font types `Jet::OSDFont`, `Jet::OSDFont8a`, `Jet::OSDFontPlugin`. These map conceptually onto InterfaceResources' `BorderResource/CursorResource/FontResource/FontInstance/IconInstance` assets and font plumbing. Exact call contract/ownership is not specified in-repo but must be treated as a compatibility boundary.
- **Implication:** InterfaceResources implementation choices (resource type names, lookup behavior, instance init semantics, measurement/print results, color/tag handling) will directly affect InterfaceSystem; coordination needed before/while implementing InterfaceSystem.

### 3.6 Runtime/CRT
- MSVCR80 imports present; standard 32-bit MSVC8-era CRT/ABI environment (matches existing Jet modules).

## 4. Proposed implementation units and order

Goal: reach drop-in parity incrementally, prioritizing framework correctness (Resource/Plugin/RTTI/vtables) and testable subcomponents. Suggested decomposition (future work; not started):

1. **U0 — Scaffolding/proxy (build + ABI surface)**
   - Add module skeleton under `src/jet/ui/InterfaceResources_7c/` mirroring existing patterns (exports/def forwarding, `implemented.txt`, CMake wiring) with all 169 exports initially forwarded to original.
   - Import/link dependencies: Kernel_7c, KernelStandard_7c, KernelResource_7c, KernelResources_7c, Render2DCore_7c (and others only if required by symbol resolution).
   - Baseline in-game/menu smoke once proxy loads (coordination required; out of this analysis scope).

2. **U1 — Core data/value types and small holders**
   - `FontInformation` (ctor/dtor/copy/assign/color list/tags) and simple value classes: `CursorInstance`, `IconInstance` basic state/init shells as evidenced, `ReferenceCount` if module-local, shared small helpers.
   - Rationale: low external behavior, enables later composition; `ExtractTags` (623 B) can be split but should be validated early if format is known.

3. **U2 — Resource base integration and type/plugin registration**
   - Verify class layouts/vtable slot mapping against KernelResource (`Resource` 0x5c base, observer/status/ref/lock semantics) for `BorderResource`, `CursorResource`, `FontBaseResource`, `FontResource`, `IconResource`/`BitmapBorderResource`.
   - Implement/align `ResourceType<>` and `FontResourceType` factories and `JetGetPluginsInterfaceResources` registration.
   - Rationale: everything else depends on correct construction, ownership, DB attachment, and type dispatch.

4. **U3 — Font effects subsystem (`FontFx`, `FontFxBitmap`, `FontFxTT`)**
   - Base interface/vfunc contract, bitmap-backed effect (`Load/LoadDR/Init/Done/GetABC/GetCharPos/GetCacheBitmap/GetHeight`) then TT effect (`Load/Init/BeginCacheChars/CacheChar/EndCacheChars/GetABC/GetRawImage/GetMaxCharWidth/GetHeight`).
   - Rationale: `Font` composes/queries effects (`AddFx/RemoveFx/NewChar/GetParam/cache paths`); isolate effect complexity before full text rendering. `FontFxTT` may require platform text/font APIs — confirm from disasm/runtime deps during implementation.

5. **U4 — Font core and font resources**
   - `Font` (charset init, char/ABC/param management, widths/word/layout calc, measurement, print/printf overloads) — largest and most behavior-sensitive code.
   - `FontBaseResource`/`FontResource` (Load/Create/Unload/size/is-available forwarding, `SetOnTheFly`, delegation to `Font`), `FontInstance` wrapper (name/points/color/flags, print/measure delegation).
   - Suggested sub-splits: measurement/layout (`GetW/H/WH/CalcWidths/GetWordWidth/AdjustPosForJust`) before full `Print` pipeline; resource lifecycle before render parity.

6. **U5 — Cursor and border resources**
   - `CursorResource` (`LoadTxt`, bitmap/area/offset, availability) and `CursorInstance` integration points.
   - `BorderResource` + internal `BitmapBorderResource` (`Render`, large vf11/vf18/vf19/vf20 paths) — likely 9-slice/bitmap-border rendering; treat as a distinct complex subunit.

7. **U6 — Icon resources/instances and integration**
   - `IconResource` internals (non-exported vfuncs) and `IconInstance` (`Init`, `Render`, `GetAutoSize/GetAutoHeight/GetW`).

8. **U7 — Differential/integration hardening**
   - Differential tests comparing our DLL vs original (pattern used elsewhere in repo) for: resource load/parse of real assets, text measurement/layout, print output to bitmaps, cursor/border/icon metrics and rendered pixels, plugin/type registration behavior.
   - In-game validation with InterfaceSystem scenarios once InterfaceSystem work begins.

**Ordering rationale:** U2 (Resource/Type ABI) is a gate for all loadable assets; U3 before U4 because font rendering depends on effect/char providers; U5/U6 can follow once bitmap/resource primitives are stable. Keep `implemented.txt`/exports in sync per export (project convention).

## 5. Risks and validation considerations

- **Unknown binary layouts / no Jet API headers:** SDK header provides only export macro; no in-repo declarations for `Font*`, `Cursor*`, `Border*`, `Icon*`, `FontFx*`. Layouts, vtable order/slot counts (esp. multi-vtable `FontFx*` with `PluginInstance`, extended `FontBaseResource` slots), and calling conventions must be inferred from exports, vtnames, disassembly/decomp maps, and differential behavior.
- **KernelResource ABI coupling:** Must exactly match `Resource` object model (fields/status/ref/lock/observer/size-cache semantics per `notes/KernelResource.md`) and type/plugin interfaces. Small mismatches can break loading, caching, or lifetime across modules.
- **Font/text complexity (high risk):** Large functions (`Font::Print` 1986 B, `CalcWidths/GetWordWidth/NewChar`, multiple `Print/Printf/Get*` overloads) imply nontrivial layout, wrapping/justification, color/font tags (`FontInformation::ExtractTags`), and effect interaction. Visual/text regressions are likely without strong diff coverage.
- **FontFxTT/platform dependency (needs verification):** TT path (`Load`, char caching, `GetABC`, `GetRawImage`) may depend on OS font APIs/GDI/FreeType or embedded rasterization details; confirm actual imports/behavior during implementation. Cross-environment determinism could be an issue for tests.
- **BitmapBorder/Icon rendering complexity:** `BitmapBorderResource` contains several very large vfuncs (1494/663/465 B) and `IconResource` sizable load/render vfuncs; likely format-specific parsing and 9-slice/compositing with `ResourceDatabase`/bitmap lookups. Pixel-exact output matters for UI.
- **Cursor resource format/parsing:** `CursorResource::LoadTxt` (1129 B) and area/bitmap/offset accessors indicate text-described cursor assets; parsing quirks must match original.
- **Cross-module state and statics:** Resource/type registries and any module globals live in original until replaced coherently; `JetGetPluginsInterfaceResources` registration must occur at the expected time and be idempotent/compatible.
- **Testing constraints:** UI/resource behavior is best validated by differential tests over real asset files and bitmap comparisons plus in-game checks; reproducing exact `Jet::Bitmap`/`ResourceDatabase` environments requires coordination (build/test harness, access to original DLL/assets). Some parsing edge cases (noted as quirks in other modules) may need to be preserved.
- **Current repo state:** `COORDINATION.md` warns RenderSystem_7c is in-flight/uncommitted and WaterSystem_7c has known pre-existing build issues — avoid touching shared areas; treat `src/`/build outputs as read-only for this task.

## 6. Cross-agent notes

### For `InterfaceSystem_7c` (primary consumer)
- **Hard dependency:** MODLOG shows InterfaceSystem imports 32 InterfaceResources symbols. InterfaceResources provides the underlying font/cursor/border/icon resource & rendering primitives that InterfaceSystem wrappers (`UI::Border`, `UI::Cursor`, `UI::IFont`, `UI::Icon`, `OSDFont/OSDFont8a/OSDFontPlugin`) are expected to use.
- **Coordinate on contracts:** agree on resource type names/IDs, lookup/init flows (by `ResourceDatabase` + name/PString), instance ownership/lifetime, measurement/print semantics (width/height/justification/color tags), cursor rect/offset behavior, and any font plugin/OSD integration points. InterfaceResources should be implemented/tested with InterfaceSystem's expected call patterns in mind (inventory InterfaceSystem's 32 imports once its analysis/implementation begins).
- **Sequencing:** InterfaceSystem cannot be fully validated without compatible InterfaceResources behavior; plan parallel analysis but align interfaces before either side is "done".

### For `KernelResource_7c` / `KernelResources_7c` owners
- **KernelResource:** confirm/consume the canonical `Resource` vtable/layout, status/ref/lock/observer rules, `ResultClass` error conventions, and `ResourceType`/plugin registration mechanism. `FontResourceType` and `ResourceType<>` factories must integrate the same way as existing resource modules.
- **KernelResources:** bitmap-backed assets (cursors, borders, icons, bitmap fonts) likely flow through `BitmapResource`/bitmap instances; ensure required bitmap access/format APIs are available and stable for InterfaceResources' loaders/renderers.

### For rendering/UI-adjacent modules (`Render2DCore_7c`, `RenderSystem_7c`)
- Rendering here targets `Jet::Bitmap`/2D primitives rather than 3D device APIs. Any assumptions about `Bitmap` layout, blit/composite, clipping/rect math, or color packing must match the implementations used by font/border/icon drawing. `Render2DCore_7c` is complete (README) and is the natural lower layer; avoid changes there without coordination.

### General coordination
- `notes/agents/` currently has no `InterfaceSystem` analysis note; this report should be read together with future InterfaceSystem inventory work.
- Respect COORDINATION rules: no shared source/build/doc edits, no contended builds/tests, no commits. If/when implementation starts, claim scope in `notes/agents/COORDINATION.md` (and follow "only write output" constraints of the active task/session).

## Scaffold (Wave 1)

**Created (module-owned, 2026-11-19):**
- 	rainz12-x64/src/jet/ui/InterfaceResources_7c/implemented.txt`n  - Header/comments state proxy-first intent: \"exports taken from our code... all others forward to InterfaceResources_7c_orig.dll when wired.\"
  - Lists **only** JetGetPluginsInterfaceResources (stub/no-op registration; pending ABI/factory/ID verification). No other exports taken from our code.
- 	rainz12-x64/src/jet/ui/InterfaceResources_7c/InterfaceResources.h`n  - Evidence-cited scaffold header (exports/fmap/vtnames + InterfaceSystem/KernelResource/KernelResources headers).
  - Declares extern \"C\" void __cdecl JetGetPluginsInterfaceResources() (evidence: ordinal 169, RVA 0x9360, unmangled extern \"C\", void/no-arg __cdecl in TSV; **return/params still TODO verify**).
  - Placeholder/TODO declarations (mostly commented forward declarations/stubs, **no invented layouts, no vtable contents, no implemented methods**) for key families: registration/types (BitmapBorderResource internal, CursorResource, IconResource, TODO ResourceType<>/FontResourceType factories+IDs/groups), borders (BorderResource, BitmapBorderResource hotspot), cursors (CursorResource, CursorInstance), fonts/effects (FontColor forward, FontFx, FontFxBitmap, FontFxTT with MI/vbtable+dual vftables evidenced but not inferred, FontInformation, Font, FontBaseResource, FontResource, FontInstance), icons (IconResource internal, IconInstance).
  - Documents boundary assumptions as TODOs.
- 	rainz12-x64/src/jet/ui/InterfaceResources_7c/InterfaceResources.cpp`n  - Includes header; no class behavior implemented.
  - Defines JetGetPluginsInterfaceResources as a **safe no-op** stub with TODO to recover/register exact UI resource type/factory set, IDs, names/extensions, parent/group UIds, and registration order/side effects from original decomp.

**Stubbed vs deferred (exports/symbols):**
- **Stubbed (intended for our code when wired, 1/169):** JetGetPluginsInterfaceResources only (no registration/behavior).
- **Deferred (168/169, remain forwarded to original when proxied/wired):** all other exports in D:\trainz12-decomp\exports\InterfaceResources_7c.tsv (BorderResource, CursorResource/CursorInstance, full Font/FontInstance/FontInformation/FontBaseResource/FontResource surface, FontFxBitmap/FontFxTT, IconInstance, etc.). Internal/non-exported fmap/vtnames classes (BitmapBorderResource, IconResource, FontResourceType, ResourceType<> specializations, ReferenceCount, BitmapBaseInstance import/thunk) not implemented. No exported data/vtables/method bodies beyond plugin getter claimed.

**ABI/layout/resource-format assumptions and TODOs (verify with decomp before full impl):**
- **Calling conventions/signatures:** TSV shows __thiscall for most instance/virtual methods, plain exported data/vtables, and unmangled extern \"C\" plugin getter (placeholder __cdecl void()). Verify mangling, const/return-by-value (Rect/Point/Size/PString/PackedColor/ResultClass) ABI, varargs Printf, hidden-struct returns.
- **Class layouts/vtables:** No in-repo Jet decls for Font*/Cursor*/Border*/Icon*/FontFx*. vtnames indicate Resource-like base slots + specializations (CursorResource vf18..vf20; BitmapBorderResource/IconResource large vf11/vf14/vf18+; FontBaseResource extended vf18..vf32; FontFx MI/vbtable with FontFx+PluginInstance subobjects). Do not assume slot order/count/offsets or MI layout; match to Jet::Resource (~0x5c, jet/Resource.h) / plugin bases via decomp+diffs.
- **Resource/type/plugin contracts:** Factory/type/parent/group IDs, registered extensions/names, on-the-fly creation, and exact JetGetPluginsInterfaceResources registration set/order/side effects unknown.
- **Resource formats/parsing:** Large parsers/renderers (CursorResource::LoadTxt ~1129 B; BitmapBorderResource/IconResource big vfuncs; FontFxBitmap Load/LoadDR; FontFxTT Load; FontResource Load/Create) imply format-specific parsing/compositing (9-slice borders, cursor hotspot/area/bitmap refs, icon/text composition, bitmap/TT font data/caches). Formats, tag syntax (FontInformation::ExtractTags), charset/init, and caches unverified.
- **x64/porting:** Preserve object sizes/alignment/packing and vptr/vbptr placement; account for pointer/size_t width differences and 32-bit asset/format assumptions while keeping API ABI compatible with rebuilt consumers.

**Boundary notes (InterfaceSystem / KernelResource / Render2DCore):**
- **InterfaceSystem_7c:** Hard consumer edge InterfaceSystem -> InterfaceResources 32 and UI/OSD wrappers depend on compatible resource type IDs/names, DB+name lookup/init, instance ownership/lifetime, measurement/print semantics (w/h/just/wrap/color tags), cursor rect/bitmap/offset, border/icon render outputs, and font/OSD integration. Coordinate sequencing/contracts.
- **KernelResource_7c:** Canonical Resource model (status/ref/lock/observer/size-cache/load lifecycle), ResourceDatabase, ResultClass, ResourceTypeBase/plugin registration; UI resource types must match base ABI/layout/virtual contract.
- **KernelResources_7c:** Bitmap-backed assets (imported BitmapBaseInstance evidence) likely via BitmapResource/instances and bitmap access semantics.
- **Render2DCore_7c/bitmaps:** Drawing/measurement target Jet::Bitmap; depends on bitmap layout/blit/composite/clip/rect/color packing. Render2DCore complete - do not change without coordination.

**How to wire into build later (without editing shared files now):**
- Under explicit claim/coordination edit shared files later: 	rainz12-x64/src/jet/CMakeLists.txt (add ui/InterfaceResources_7c wiring per pattern) and, if needed for proxy/export mechanics, module export/def/forwarding setup; update README.md/PORT_PLAN.md/tools/progress_exports.tsv/MODLOG.md only as coordinated progress/build work. Do not touch in-flight RenderSystem/WaterSystem-owned or contended files.
- Follow existing module patterns (sources incl. InterfaceResources.cpp + implemented.txt handling, link Kernel/KernelStandard, KernelResource, KernelResources, Render2DCore as required). Keep proxy-first: forward all exports to InterfaceResources_7c_orig.dll except implemented.txt entries (currently only JetGetPluginsInterfaceResources) once wired.
- Before enabling non-trivial exports: verify ABI/layout/formats from decomp, add differential tests over real assets/bitmaps and registration behavior, and validate with InterfaceSystem scenarios.
