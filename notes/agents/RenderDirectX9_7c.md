# RenderDirectX9_7c.dll — analysis / future implementation plan (read-only)

**Module (README):** `RenderDirectX9_7c` — 70 KB, 404 functions, 0/41 exports, status `not started`.  
**Group:** graphics. Launcher-selectable alternative renderer to `RenderOpenGL_7c` (`plugins.ini` registers `RenderDirectX9:DirectX9:052FEC1E-...` in the same renderer group as OpenGL; MODLOG 2026-10-26 correction).  
**Context:** `PORT_PLAN.md` step **8f** — “RenderDirectX9_7c, the launcher's other renderer (d3dx9_42; x64 uses d3dx9_43). Checks with DirectX picked in the launcher.” Comes after RenderSystem/Render3D*/RenderOpenGL/Interface/Water inventories/work (8d–8e) and before InputDirectX (8g).

**Evidence sources (read-only):** `README.md`, `PORT_PLAN.md`, `MODLOG.md`, `notes/RenderSystem.md`, `notes/agents/COORDINATION.md`, `D:\trainz12-decomp\exports\RenderDirectX9_7c.tsv` (41 rows), `ghidra_maps/jet/graphics/RenderDirectX9_7c.dll.functions.tsv` (481 mapped entries), `ghidra_maps/jet/graphics/vtnames_RenderDirectX9_7c.dll.tsv`. No `src/jet/graphics/RenderDirectX9_7c/` exists yet.

---

## 1. Overview / role

`RenderDirectX9_7c.dll` is the **Direct3D 9 backend** for Jet’s render stack. Like `RenderOpenGL_7c`, it provides concrete implementations of RenderSystem’s abstract renderer interfaces (Display, textures, vertex/program/combiner/buffer management, shader loading) and exposes them to the engine via the plugin system.

Key facts from repo notes:
- MODLOG: renderers are `RenderDirectX9_7c.dll` (uses `d3dx9_42.dll`) and `RenderOpenGL_7c.dll`; shaders are NVIDIA Cg 2.2. D3DX dependency table notes x64 path as `d3dx9_43` (June 2010 redist) or remove d3dx.
- MODLOG: `DisplayTexture` (RenderSystem base) is derived by RenderOpenGL and RenderDirectX9 (inline its fields in backends). RenderSystem statics/format tables stay original until U9/foundation moves.
- RenderSystem is the API-independent half every render module stands on; DX9 is one of several consumers/implementers in that ecosystem (PORT_PLAN/MODLOG list RenderDirectX9 among modules importing RenderSystem exports; MODLOG cites “RenderDirectX9 143”).

This module is **not the live/default renderer path called out first** for porting (PORT_PLAN: “The live renderer is OpenGL: port RenderOpenGL first; it is also the natural base for a modern backend.”), but it must be ported for launcher parity and add-on/runtime compatibility when users pick DirectX.

---

## 2. Inventory (from exports + Ghidra maps)

### 2.1 Export surface (`exports/RenderDirectX9_7c.tsv`, 41 entries)
- **36 function/method exports** — all `Jet::DisplayDescriptorDirectX9` or `Jet::DisplayTextureDirectX9` (ctors/copy/assign/dtors, Initialize/Destroy, Create/Load/SubLoad/Bind/CheckFormat/Upload/Copy*/GetBitmap/Refresh etc.).
- **2 vftable data exports:** `??_7DisplayDescriptorDirectX9@Jet@@6B@` (rva 0x14480), `??_7DisplayTextureDirectX9@Jet@@6B@` (rva 0x1448c).
- **3 static data exports:** `DisplayTextureDirectX9::canAutoMip` (bool), `mipCubeMapSupport` (int), `mipVolumeMapSupport` (int).
- **1 plugin entry:** `JetGetPluginsRenderDirectX9` (ordinal 41, rva 0xfb30).

**Implication:** exported ABI is very small and texture/descriptor-centric; the main Display/combiner/program/VB-manager implementations are **internal** (not name-exported) but still required for correctness and must match RenderSystem contracts.

### 2.2 Mapped function inventory (`RenderDirectX9_7c.dll.functions.tsv`, 481 mapped entries)
Code-size/naming snapshot by namespace (sum of mapped sizes):
| Namespace | Count | Bytes | Notes |
|---|---:|---:|---|
| `Global` | 260 | 53,073 | Unattributed bulk of logic (likely DisplayDirectX9 body/state, D3D calls, helpers, format/state translation) |
| `Jet::DisplayTextureDirectX9` | 31 | 7,489 | Matches exported/internal texture implementation |
| `Jet::VertexBufferManagerDirectX9` | 30 | 3,622 | Backend VB/IB/static buffer mgmt |
| `Jet::DisplayDirectX9` | 12 | 2,258 | Named core entrypoints only; much may be in `Global` |
| `Jet::TextureCombinerDirectX9` | 21 | 1,854 | Fixed-function/texture-stage combiner backend |
| `Jet::VertexProgramDirectX9` | 40 | 1,086 | Vertex program backend (many small vfuncs) |
| `Jet::CGShaderConstantDirectX9` | 13 | 743 | Cg/D3D9 constant binding |
| `Jet::DisplayDescriptorDirectX9` | 7 | 737 | Exported descriptor |
| `Jet::CGShaderLoaderDirectX9` | 6 | 656 | Shader/effect loading path |
| `Jet::CGShaderTechniqueDirectX9` | 5 | 218 | Technique/pass control |
| `Jet::PluginFactoryOf<Jet::DisplayDirectX9,Jet::Display>` | 4 | 177 | Plugin registration |
| `StateManagerD3D9` | 1 | 60 | D3D state cache/manager hook |
| `RENDERSYSTEM_7C.DLL::Jet::Display` (import refs) | 29 | 174 | Cross-module Display calls |
| `MSVCR80.DLL` / `D3DX9_42.DLL` / `type_info` thunks | 18 | 108 | Runtime/dep thunks |
| other small | — | — | `Jet::ReferenceCount`, `StaticBufferData`, `VPSymbol` |

README states 404 functions total; TSV includes additional thunk/import/typeinfo rows — treat TSV as the actionable map for attribution, while planning against the 404-function module scope.

### 2.3 Notable classes / implementation areas (evidenced)
- **Display/descriptor:** `DisplayDescriptorDirectX9`, `DisplayDirectX9` (vf0..vf11 seen in vtnames/functions), `StateManagerD3D9`.
- **Texture:** `DisplayTextureDirectX9` — rich exported/internal API (Create/Destroy/Load/SubLoad/SubloadFromFrame/Bind overloads/Upload/GetBitmap/GetSurface/GetDepthSurface/GetDXCubeBuffer/CopyMipToMip/ReCreateD3DTextureBuffer/format checks, static mip-support flags).
- **Programs/combiners:** `VertexProgramDirectX9`, `TextureCombinerDirectX9`, `VertexProgramBasic::VPSymbol` (1 fn) — map RenderSystem program/combiner abstractions to D3D9.
- **Buffers:** `VertexBufferManagerDirectX9` plus `VertexBufferManagerDirectX9::StaticBufferData` — D3D9 VB/IB/static buffer backend.
- **Cg/D3D shader integration:** `CGShaderLoaderDirectX9`, `CGShaderTechniqueDirectX9`, `CGShaderConstantDirectX9`.
- **Plugin/factory:** `JetGetPluginsRenderDirectX9`, `PluginFactoryOf<Jet::DisplayDirectX9,Jet::Display>` vtable slots present in vtnames.

### 2.4 External runtime deps visible in map
- **`D3DX9_42.DLL` thunks:** `D3DXCreateEffectFromFileA`, `D3DXLoadSurfaceFromMemory`, `D3DXLoadSurfaceFromSurface`, `D3DXAssembleShader`.
- **`MSVCR80.DLL`** thunks (CRT/runtime). COM/D3D9 device interfaces implied by method names (`GetDevice`, `GetDXBuffer/GetSurface/GetDepthSurface` in texture API) though `d3d9.dll` import rows are not enumerated in the snippets above — verify during impl.
- **Cg 2.2** is stated in MODLOG as shader technology used by renderers; no `cg*.dll` import thunk was observed in the sampled rows, but `CGShader*DirectX9` classes exist — actual Cg runtime linkage (cgD3D9/cg vs D3DX effects/assembly) must be confirmed from full import table/decompile during implementation.

---

## 3. Dependencies and integration

### 3.1 Upstream (must be stable/understood)
- **RenderSystem_7c (critical):** DX9 implements RenderSystem abstractions (`Display`, `DisplayTexture`, vertex/program/combiner, buffer manager interfaces, shader-loader contracts). RenderSystem notes show U9 foundation (`Display`, `RenderKernel`, exported statics/format tables, DisplayTexture statics) still has original-owned/shared-state constraints until moved — backend behavior must align with RenderSystem’s expected virtual slot semantics and state ownership.
- **Kernel/KernelStandard/KernelResource(s):** RenderSystem itself depends only on already-ours core modules; DX9 will follow the same Jet runtime/base contracts (strings, refs, allocs, plugin/resource conventions) as other graphics modules.
- **Render3DCore / Render3DFoundation / Render3DExtension / WaterSystem / Interface / trainz.exe consumers:** depend on RenderSystem API, not usually on DX9 internals; but rendering correctness under DX9 selection is an end-to-end stack concern.

### 3.2 Peer backend: `RenderOpenGL_7c` (primary reference for behavior)
- Same role, different API: OpenGL map shows analogous split (`DisplayOpenGL*`, `DisplayTextureOpenGL`, texture combiner/program/buffer variants NV/ARB/SW/ATI, library/interface classes, Cg loader/constant). DX9 appears simpler in variant count but has D3D device/lost-state/surface specifics and D3DX calls.
- PORT_PLAN/notes prioritize OpenGL first; DX9 should reuse **validated RenderSystem contract understanding** and, where semantics are renderer-neutral, mirror OpenGL behavior/tests adapted to D3D9.

### 3.3 Downstream / selection
- **Launcher / plugins.ini / `JetGetPluginsRenderDirectX9`:** factory must register correctly so DirectX can be picked (step 8f check). Incorrect plugin metadata/vftable/descriptor init will prevent selection or crash early.
- No other modules export-link against DX9 internals evidenced here; coupling is plugin/display-polymorphic.

---

## 4. Proposed implementation units and order

Goal: minimize risk, get plugin selection + device/display bring-up early, then texture/buffer/program paths, then shader/edge features. Keep parity with RenderSystem slot expectations and OpenGL-observed behavior.

1. **U0 — Forwarding/proxy skeleton (in-game safety net)**
   - Build drop-in DLL exporting the 41-entry ABI and `JetGetPluginsRenderDirectX9`, forwarding to `RenderDirectX9_7c_orig.dll` (same pattern used in project). Verify launcher can still enumerate/select DirectX with original behavior.
   - Gate: menu reachability with DX selected, no JetLog regression attributable to proxy.

2. **U1 — Plugin/factory + descriptor + DisplayDirectX9 lifecycle**
   - Implement `PluginFactoryOf<DisplayDirectX9,Display>` and `JetGetPluginsRenderDirectX9`; `DisplayDescriptorDirectX9` (ctor/copy/assign/dtor/Initialize/Destroy/vf0) and `DisplayDirectX9` core vfuncs/lifecycle/state-manager hooks (`StateManagerD3D9`).
   - Attribute `Global` functions called from factory/display init (start from xrefs of named Display/descriptor/factory entries in map).
   - Gate: launcher DirectX selection reaches display init; clean shutdown.

3. **U2 — DisplayTextureDirectX9 (largest exported/internal cluster)**
   - Full texture object: creation/destruction, D3D texture/surface management (`Create`, `ReCreateD3DTextureBuffer`, `DestroyD3DTextureBuffer`, `Upload`), load/subload/frame copy, bind, format conversion/selection (`CheckFormat`), bitmap extraction, mip/cube/volume support flags/statics.
   - Critical because exports/statics and RenderSystem DisplayTexture contract are explicit; `GetBitmap`/format paths are large.
   - Gate: differential/behavior checks vs original on texture ops where harnessable; visual smoke with DX selected.

4. **U3 — VertexBufferManagerDirectX9 + StaticBufferData**
   - Implement VB/IB/static buffer allocation, locking/upload, draw submission helpers as required by Display/Geometry draw paths. Map ~30 vfunc slots (vtnames) to RenderSystem buffer-manager interface.
   - Gate: geometry rendering correctness (meshes/UI/terrain elements that exercise dynamic/static buffers).

5. **U4 — TextureCombinerDirectX9 and fixed-function/state paths**
   - Texture stage/combiner backend (~21 funcs) and any related Display state application; likely needed for non-shader/fallback materials.
   - Gate: materials/effects that use combiner paths match OpenGL/original DX9 output reasonably.

6. **U5 — VertexProgramDirectX9 + VertexProgramBasic symbol support**
   - Vertex program binding/parameter setup; integrate with RenderSystem U3 program objects/managers expectations.
   - Gate: scripted/programmed vertex effects.

7. **U6 — CGShader*DirectX9 (loader/technique/constants) and D3DX shader/effect helpers**
   - `CGShaderLoaderDirectX9`, `CGShaderTechniqueDirectX9`, `CGShaderConstantDirectX9`; resolve D3DX calls (`D3DXCreateEffectFromFileA`, `D3DXAssembleShader`, surface load/copy). Confirm Cg runtime interaction.
   - Gate: Cg/shader-based materials/content load and render.

8. **U7 — Remaining `Global` attribution, caps/edge cases, x64 readiness cleanup**
   - Finish unnamed helpers (format tables, device-lost/reset, present, state cache corners), remove forwarding remnants, align with `d3dx9_43`/x64 strategy decided in step 6/8f.

**Ordering rationale:** plugin/display+texture unblock selection and visible output early; buffers/programs/combiners follow RenderSystem’s rendering pipeline; Cg/D3DX is highest integration risk and can be isolated once core frame path works.

---

## 5. Risks and watchpoints

### 5.1 API / dependency versioning
- **d3dx9_42 (32-bit, legacy) → d3dx9_43 for x64:** evidenced thunks include `D3DXCreateEffectFromFileA`, `D3DXLoadSurfaceFromMemory`, `D3DXLoadSurfaceFromSurface`, `D3DXAssembleShader`. Behavior/availability differences or desire to eliminate D3DX (MODLOG option) affect U6 and possibly texture upload/format paths. Need an explicit strategy consistent with PORT_PLAN/step decisions.
- **Cg 2.2:** shader pipeline compatibility is a known cross-cutting concern (MODLOG step 6/7 context elsewhere). `CGShader*DirectX9` presence without obvious cg import thunks in sampled rows means linkage/compilation path must be verified before rewriting.

### 5.2 D3D9/device semantics
- **Device lost/reset, render-target/context management, COM refcounting/lifetime** — easy to diverge subtly from original; `ReCreateD3DTextureBuffer`, surface handling, and `StateManagerD3D9` are sensitive.
- **Format/capability negotiation:** `CheckFormat`, mip/cube/volume support statics, and descriptor initialization must match content expectations and RenderSystem format IDs.
- **Multithreading/resource streaming assumptions:** RenderSystem TextureManager/streaming behavior (partly in-flight in current tree per coordination notes) may interact with D3D9 usage rules (D3D9 is not generally free-threaded for device calls).

### 5.3 Shader / rendering correctness
- **Fixed-function combiner vs programmable paths:** both TextureCombiner and Vertex/CG program backends exist; wrong path selection or state leakage yields material bugs hard to spot without DX-selected visual tests.
- **Precision/ordering differences** vs OpenGL or original DX9 can affect particle/water/UI edges; need targeted captures with DirectX picked in launcher.

### 5.4 x64 and ABI hazards (future phase)
- Pointer-sized handles/COM interface pointers, structure packing, `int`↔pointer assumptions in RenderSystem/base types, and any thunk/calling-convention details become critical at x64 port time. Keep 32-bit behavior oracle-backed now (differential/in-game) to reduce x64 bring-up ambiguity.
- `d3dx9_43` x64 vs removing D3DX changes build/runtime packaging.

### 5.5 Project/process risks
- **Large `Global` (53 KB, 260 funcs unnamed):** attribution cost is the main schedule risk; requires systematic xref/vtable-guided naming and may reveal non-obvious helpers shared across texture/display/program units.
- **Small export surface but large internal surface:** cannot treat as “41 trivial exports”; internal vtable compatibility with RenderSystem/engine dispatch is the real contract.
- **In-flight RenderSystem work:** `COORDINATION.md` flags uncommitted/in-flight RenderSystem source/tests/docs (U7). DX9 implementation depends on RenderSystem contracts; avoid assumptions that conflict with moving targets, and re-check shared Display/texture/material expectations when RenderSystem changes land.
- **Verification dependency:** step 8f requires checks with DirectX picked in launcher — needs a runnable environment with selectable renderer and comparable captures/logs.

---

## 6. Cross-agent notes

### For **RenderSystem_7c** (owner/in-flight per coordination)
- DX9 is a concrete consumer of `Display`, `DisplayTexture`, texture-combiner/vertex-program, shader-loader, and buffer-manager abstractions. Slot numbering/semantics, `DisplayTexture` inline-field/layout expectations, format tables/capability queries, and any statics moved from “original-owned until U9” directly affect DX9 parity.
- When changing Display/DisplayTexture/manager virtual APIs or shared-state ownership, flag DX9 (and OpenGL) as required backend follow-ups. `DisplayTextureDirectX9` exports/statics (`canAutoMip`, `mipCubeMapSupport`, `mipVolumeMapSupport`) are ABI-visible and must stay stable.
- Useful coordination point: document which vtable slots correspond to DX9-named vfuncs in `vtnames_RenderDirectX9_7c.dll.tsv` once attributed, to keep backend work aligned.

### For **Render3DCore / Render3DFoundation / Render3DExtension**
- 3D modules render through RenderSystem/display abstractions; DX9 selection is an integration test for geometry/material/program/lighting paths they exercise. If 3D-side inventories identify renderer-sensitive assumptions (caps, format support, shader availability), capture them for DX9 U4–U6 validation.
- No evidence of direct 3D→DX9 linking; issues seen only under DirectX likely indicate backend contract gaps rather than 3D API misuse.

### For **RenderOpenGL_7c**
- OpenGL is the primary/live renderer and reference backend. Reuse its proven mapping of RenderSystem concepts (Display lifecycle, texture lifecycle/upload/bind, combiner/program variants, Cg/shader loading) while accounting for D3D9-specific device/surface/state behavior.
- Suggested sharing: common test scenarios/content fixtures and screenshot/JetLog comparison procedures run once with OpenGL, once with DirectX (launcher-selected) to isolate backend-only regressions.
- Note architectural difference: OpenGL map shows multiple implementation variants (NV/ARB/ATI/SW/WGL/library splits); DX9 implementation appears more monolithic (Direct3D9 + D3DX helpers) — attribution strategies differ, but API contracts should converge at RenderSystem level.

### For **WaterSystem / Interface / later steps**
- Water and UI can be sensitive to blend/state/texture/render-target behavior; include DX9-selected smoke tests when those modules are touched (8d/8e/8f chain).

---

## 7. Suggested immediate next actions (when implementation is scheduled)
1. Confirm full PE import table of original `RenderDirectX9_7c.dll` (esp. `d3d9.dll`, any `cg`/`cgD3D9` DLLs) to finalize dependency list beyond D3DX9_42/MSVCR80 thunks seen in function map.
2. Attribute `DisplayDirectX9` vtable/layout and `JetGetPluginsRenderDirectX9` path first (small named surface + factory) to anchor `Global` xrefs.
3. Define DX9-selected in-game test checklist for step 8f (launcher pick, menu, known material/texture-heavy scene, screenshot/log capture) aligned with existing `probe/dupshot` tooling.
4. Decide d3dx handling for 32-bit parity vs x64 (`d3dx9_43` or removal) before deep U6 work.

## Scaffold (Wave 2/4)

### Files created (module-owned)
- `trainz12-x64/src/jet/graphics/RenderDirectX9_7c/implemented.txt` — proxy-first manifest: forwards all 41 exports by default when wired; **no exports enabled**. Documents evidenced ABI-visible entrypoints/symbols only (plugin, descriptor/texture exports, texture statics) and blocks enabling without recovered ABI/behavior/contracts.
- `trainz12-x64/src/jet/graphics/RenderDirectX9_7c/RenderDirectX9.h` — minimal evidence-only placeholder/header documenting module role and evidenced surface (exports/vftables/statics, named internal classes, D3DX9_42 thunks seen in function map, Global attribution gap). Contains namespace/class-name placeholders only; **no layouts, field offsets, vtable slot definitions/ordering, COM/device behavior, or D3D/D3DX/Cg semantics**.
- `trainz12-x64/src/jet/graphics/RenderDirectX9_7c/RenderDirectX9.cpp` — minimal placeholder TU, intentionally no local/exported implementations.

No shared/contended files modified; no build/test wiring added.

### Stubbed vs deferred
- **Stubbed/scaffolded:** documentation-only placeholders and proxy-first `implemented.txt` posture for the 41-entry exported ABI (`JetGetPluginsRenderDirectX9`; `Jet::DisplayDescriptorDirectX9` ctor/copy/assign/dtor/Initialize/Destroy/vf0/vftable; `Jet::DisplayTextureDirectX9` lifecycle/load/bind/upload/surface/copy/format/bitmap APIs/vftable; statics `canAutoMip`, `mipCubeMapSupport`, `mipVolumeMapSupport`).
- **Deferred (must be recovered, not guessed):** all internal behavior and unattributed logic — `Jet::DisplayDirectX9` core/state paths, `StateManagerD3D9`, `VertexBufferManagerDirectX9` (+`StaticBufferData`), `TextureCombinerDirectX9`, `VertexProgramDirectX9`, `CGShader*DirectX9` (loader/technique/constants), `PluginFactoryOf<Jet::DisplayDirectX9,Jet::Display>`, `VertexProgramBasic::VPSymbol`, and the large `Global` region (~260 mapped funcs / ~53 KB) likely containing D3D state/format/translation/helpers. No export or internal implementation enabled.

### Key ABI / RenderSystem-contract / D3DX-Cg TODOs and risks
- **ABI/layout/vtables:** object sizes, inheritance/field offsets, and vtable slot meanings/ordering are unknown beyond evidenced names; `vtnames_RenderDirectX9_7c.dll.tsv` shows aliases/shared-slot cases and duplicate slot labels — slot semantics must be decomp-verified before any local vtable/object implementation. Exported ABI is small (41) but internal vtable compatibility with RS/engine dispatch is the real contract.
- **RenderSystem contracts (in-flight, contract-critical):** must align with RS `Display`/`DisplayTexture`/combiner/program/buffer-manager/shader-loader abstractions and slot semantics; RS U7/U9/shared-statics ownership is in-flight per coordination/consolidated plan. `DisplayTexture` inline-field/layout expectations and format/capability behavior must match RS.
- **D3D9 runtime semantics:** device creation/lost-reset handling, COM refcount/lifetime, render-target/surface/texture management, present/state-cache behavior (`StateManagerD3D9`), and `ReCreateD3DTextureBuffer`/surface paths are correctness-sensitive and unrecovered.
- **Texture/format/capability:** `CheckFormat`, mip/cube/volume support statics, descriptor initialization, upload/copy/get-bitmap paths must match original/content expectations; DX9 usage/threading constraints may interact with RS streaming assumptions.
- **D3DX/Cg linkage:** function map evidences `D3DX9_42.DLL` thunks (`D3DXCreateEffectFromFileA`, `D3DXLoadSurfaceFromMemory`, `D3DXLoadSurfaceFromSurface`, `D3DXAssembleShader`) and `CGShader*DirectX9` classes, but full PE imports (incl. `d3d9.dll`, any `cg`/`cgD3D9` DLLs) and actual Cg-vs-D3DX effect path are unconfirmed. x64 strategy (`d3dx9_43` vs remove D3DX) and Cg 2.2 considerations remain open; isolate/verify before U6 work.
- **Attribution risk:** dominant `Global` surface is the main schedule/accuracy risk; requires systematic xref/vtable-guided attribution anchored on factory/display/texture entrypoints.

### Later build-wiring steps (serialized/shared-file work, not done here)
1. Confirm full original PE import table (`d3d9.dll`, D3DX variant, any Cg runtime DLLs) and finalize dependency strategy.
2. Add proxy/forwarding build wiring for `RenderDirectX9_7c` in module/CMake/progress plumbing (shared/contended files) under explicit claim: produce drop-in DLL exporting the 41-entry ABI and `JetGetPluginsRenderDirectX9` forwarding to `RenderDirectX9_7c_orig.dll`; keep `implemented.txt` empty of enabled exports.
3. Validate launcher DirectX selection/enumeration and basic menu reachability against original behavior (no JetLog regression attributable to proxy).
4. Attribute `PluginFactoryOf<DisplayDirectX9,Display>`/`JetGetPluginsRenderDirectX9`, descriptor/display lifecycle, and vtable/slot meanings first; then stage U2+ (texture, buffers, combiner, vertex program, CG/D3DX) per consolidated plan, coordinating with in-flight RenderSystem (U7/U9) and OpenGL contract understanding before enabling any local exports.
