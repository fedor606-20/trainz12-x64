# RenderOpenGL_7c - analysis and implementation plan (2026-11-19)

## Scope and constraints
- Read-only analysis of available repo metadata and maps. No source/build/doc/game changes outside this report.
- Goal: planning report for a future drop-in 32-bit replacement of `RenderOpenGL_7c.dll`, with awareness of the later x64/modern-backend phase.
- This module has no `src/jet/graphics/RenderOpenGL_7c` implementation in-tree and no per-module note under `notes/` yet. `notes/agents/COORDINATION.md` / `STATUS.md` do not list an active owner/claim for `RenderOpenGL_7c`.

## Evidence consulted
- `README.md` (progress snapshot): Jet graphics module **RenderOpenGL_7c**, 135 KB code, 944 functions, **0/51 exports ours**, status not started.
- `PORT_PLAN.md`: Phase 2 keeps modules playable 32-bit; RenderDirectX9 is the other renderer selected by launcher/plugins.ini. Phase 3: "live renderer is OpenGL: port RenderOpenGL first; it is also the natural base for a modern backend"; step 8e places `Render3DCore, Render3DFoundation, RenderOpenGL` before InterfaceSystem/WaterSystem; step 8f covers RenderDirectX9 (d3dx9_42 now, d3dx9_43 for x64). Step 6/7 decisions: Cg 3.1 x64 runtime for x64, GLSL translation later (not a blocker).
- `MODLOG.md` (selected confirmed facts):
  - `RenderOpenGL_7c.dll` ~152 KB module size noted in early census; renderer in use at menu is OpenGL (RenderOpenGL_7c), not DirectX 9. plugins.ini registers RenderDirectX9 and RenderOpenGL in the same renderer group; launcher choice loads one.
  - "Only RenderOpenGL_7c imports Cg: cg.dll (13 calls) and cgGL.dll (15 calls). The calls are the CgFX effect API (`cgCreateEffectFromFile`, techniques and passes, `cgGLRegisterStates`, `cgGLGetLatestProfile`) plus parameter setters. Profiles named: vp40 / fp40 / gp4vp / gp4fp. Compile defines name SpeedTree profiles (`-DSPEEDTREE_PIXEL_PROFILE=`). The user's content (UserData, Builtin) holds no loose .fx/.cg/.cgfx: the shaders are built in."
  - SpeedTree shaders (`bin/plugins/SpeedTreeShaders/*.fx`) are compiled by RenderOpenGL through Cg (TNISpeedTree draws via streams/TNI, separate from backend port).
  - Render module deps: `RenderOpenGL -> RenderSystem 178 only` (Jet imports). RenderSystem stands on Kernel/KernelStandard/KernelResource/KernelResources (already ours per project state).
  - Win32 note: game uses OpenGL, which leaves x87 precision at 53 bits (D3D9 without FPU_PRESERVE would set 24 bits) - relevant for differential parity when comparing render-adjacent math behavior.
- `notes/RenderSystem.md`: RenderSystem is the API-independent renderer half (abstract `Display`, vertex/geometry/texture/material/program APIs). 855/2687 RenderSystem exports imported by anyone, including **RenderOpenGL 178** and RenderDirectX9 143. U9 (foundation: `Display` 57 KB, DisplaySelector/DisplaySaveBuffer/NonRenderContext/SharedContext/ApiDataBase/RenderKernel/ResourceType/free functions/`JetGetPluginsRenderSystem`) is last; many statics/factories/managers remain original-owned until U9. `DisplayTexture` base/null and renderer-specific texture creation paths, vertex buffer/program/combiner manager construction, and display initialization are RenderSystem contract points.
- `D:\trainz12-decomp\exports\RenderOpenGL_7c.tsv` (51 export rows, ordinal+RVA+mangled+demangled).
- `ghidra_maps/jet/graphics/RenderOpenGL_7c.dll.functions.tsv` (1052 function rows: `entry`, `size`, `namespace`, `name`, `source`, `thunk`).
- `ghidra_maps/jet/graphics\vtnames_RenderOpenGL_7c.dll.tsv` (vtable-slot naming map; used as corroborating namespace/slot evidence).
- `ghidra_maps/jet/graphics/RenderDirectX9_7c.dll.functions.tsv` and `D:\trainz12-decomp\exports\RenderDirectX9_7c.tsv` for backend parity comparison (DX9: 41 exports, 481 mapped functions).
- `notes/agents/COORDINATION.md`, `notes/agents/STATUS.md`: shared-file/write restrictions; RenderSystem source/docs currently in-flight.

## 1. Overview / role (driver/backend)

`RenderOpenGL_7c` is the **OpenGL backend driver** for Trainz 2012's Jet renderer. At runtime (menu) the live renderer is OpenGL, so correctness/regressions are immediately user-visible. It plugs into the RenderSystem abstraction rather than exposing a large exported API: the export surface is only 51 entries, dominated by `DisplayTextureOpenGL`, `DisplayDescriptorOpenGL`/WGL `ContextDescriptor` types, their vtables/data, and `JetGetPluginsRenderOpenGL` (ordinal 51, RVA 0xf7a0). The bulk of implementation is internal and registered/consumed through plugin factories and virtual interfaces.

Primary responsibilities evidenced by maps/exports:
- **Plugin registration/entry:** `JetGetPluginsRenderOpenGL` registers backend capabilities with Jet/RenderSystem plugin infrastructure.
- **Display/device/platform layer:** WGL/OpenGL context and descriptor management (`ContextDescriptorOpenGL`, `ContextDescriptorOpenGLWGL`, `DisplayDescriptorOpenGL`, `DisplayInterfaceOpenGL/WGL`, `DisplayLibraryOpenGL/WGL`, `DisplayOpenGL`, `DisplayOpenGLXTerm`, `NonRenderContextWGL`, shared/PBuffer-related internals).
- **API texture implementation:** concrete `DisplayTextureOpenGL` implementing RenderSystem's `DisplayTexture` contract (creation, bind/load/subload/copy, render-context attachment, format handling, sampler/state, bitmap readback paths).
- **Shader/effect integration (Cg/CgFX):** `CGShaderLoaderOpenGL`, `CGShaderConstantOpenGL`, `CGShaderTechniqueOpenGL` using `cg*`/`cgGL*` APIs.
- **Capability-specific GPU plugins:** texture combiner implementations (`TextureCombinerOpenGLNV`, `NVfp`, `ATI`, `ARB`, plugin base), vertex program implementations (`VertexProgramOpenGLNV`, `ARB`, plugin/basic symbol helper), vertex buffer managers (`VertexBufferManagerOpenGLARB`, `SW`, plugin, static buffer data helpers). These likely correspond to extension/profile-based selection registered at runtime.
- **Render target/save buffer helpers:** `DisplaySaveBufferOpenGL` and PBuffer/non-render/shared context helpers (namespace evidence).

Strategic role for port: this is the first backend to port on the x64 path and the base for a future modern backend (per PORT_PLAN), so implementation should preserve current semantics while isolating GL/Cg/WGL specifics behind boundaries that can later be swapped.

## 2. Inventory

### 2.1 Export table (`exports/RenderOpenGL_7c.tsv`)
- **51 exports total**:
  - `JetGetPluginsRenderOpenGL` (ord 51, RVA 0x0f7a0).
  - `Jet::DisplayDescriptorOpenGL` vftable (ord 13, RVA 0x27820) and `Jet::DisplayTextureOpenGL` vftable (ord 14, RVA 0x27644).
  - 4 static data exports for `DisplayTextureOpenGL`: `anisotropySupport` (0x3c339), `bound` (0x3c340), `canAutoMip` (0x3c33a), `numObjects` (0x3c33c).
  - Methods/types exposed: `ContextDescriptorOpenGL` (protected ctor, PlatformType/get/copy), `ContextDescriptorOpenGLWGL` (ctor/copy, device/render context get/set), `DisplayDescriptorOpenGL` (ctors/dtor/copy, GetContext/Platform/Initialize), `DisplayTextureOpenGL` (ctors/dtor/copy, Create/Destroy/CreateRenderContext/CreateInstance/Load/SubLoad/SubloadFromFrame/Bind x2/BindTexture/UnbindRenderTexture/SetBasicState/CheckFormat/CopyMipToMip/CopySamplerAttributesFrom/GetBitmap/GetTextureBitmap/GetDataLost/GetJetFormat/GetAPITexture/GetRenderContext/GetDisplay/GetWinInterface/InitInstance/RefreshFromChangedSettings).
- Notable ABI/signature details from demangled exports: mix of `__thiscall` and `__cdecl` virtual/member forms, some `const`/`__thiscall` getters (e.g. `GetBitmap`), pointer/reference returns to internal types (`OpenGLFormat*`, `PBufferContext*`, `DisplayOpenGL*`, `DisplayInterfaceWGL*`). 32-bit MSVC8 naming/layout compatibility matters for drop-in replacement.

### 2.2 Function map (`ghidra_maps/jet/graphics/RenderOpenGL_7c.dll.functions.tsv`)
- **1052 mapped function rows**, **108 thunks**. Namespace distribution (count / total bytes, largest first): `Global` 537 / 80,302; `Jet::DisplayOpenGLXTerm` 146 / 13,612; `Jet::DisplayLibraryOpenGL` 3 / 10,711 (one very large routine dominates); `Jet::DisplayTextureOpenGL` 30 / 7,440; `Jet::TextureCombinerOpenGLNV` 21 / 6,898; `Jet::DisplayInterfaceWGL` 22 / 3,500; `Jet::VertexProgramOpenGLNV` 34 / 2,509; `Jet::VertexBufferManagerOpenGLARB` 24 / 1,791; `Jet::VertexProgramOpenGLARB` 34 / 1,413; `Jet::DisplayOpenGL` 13 / 1,285; `Jet::CGShaderLoaderOpenGL` 6 / 1,275; etc.
- Large `Global` bucket (305 ANALYSIS, 230 DEFAULT, 2 IMPORTED) indicates many unattributed helpers: likely GL extension/capability probing, format/state tables and converters, WGL helpers, draw/state utility paths, and static/plugin registration glue. This is a major decomp/reconstruction cost/risk area.
- Function-size histogram is dominated by very small routines (many 6-16 byte wrappers/thunks/getters/vf stubs), with a smaller number of large implementation functions (`DisplayLibraryOpenGL`, texture/combiner/program paths, CG loader).

### 2.3 Key classes/components (evidenced)
- **Context/descriptor/platform:** `ContextDescriptorOpenGL` (PlatformType, 3 funcs), `ContextDescriptorOpenGLWGL` (SetContext/GetDeviceContext/GetRenderContext), `DisplayDescriptorOpenGL` (ctor/dtor/copy/vf0/Initialize/GetContext/GetPlatform).
- **Texture backend:** `DisplayTextureOpenGL` (30 funcs) with exported statics and full lifecycle/upload/copy/bind/render-texture behavior; `InitInstance` present (Ghidra row 0x10007730, exported 0x7730).
- **Display interfaces/libraries:** `DisplayInterfaceOpenGL` (4), `DisplayInterfaceWGL` (22) plus nested `WinPBufferContext`, `SharedContextWGL`, `PBufferData` vfuncs (from vtnames map); `DisplayLibraryOpenGL` (3, very large), `DisplayLibraryWGL` (3); `DisplaySaveBufferOpenGL` (3); `NonRenderContextWGL` (4).
- **Display core:** `DisplayOpenGL` (13) and `DisplayOpenGLXTerm` (146 funcs) - primary concrete display implementation/driver variant(s); vtnames show large vf tables (many slots shared/aliased with base/plugin slots).
- **Cg shader pipeline:** `CGShaderLoaderOpenGL` (vf0-vf5, 1275 bytes total, includes large vf1 ~774 bytes), `CGShaderConstantOpenGL` (vf0-vf13), `CGShaderTechniqueOpenGL` (vf0-vf5). Uses 13 `CG.DLL` and 15 `CGGL.DLL` import thunks (CgFX/effect/technique/pass/parameter/profile/state APIs listed in 2.4).
- **Texture combiner plugins:** `TextureCombinerOpenGLNV` (21), `TextureCombinerOpenGLNVfp` (9), `TextureCombinerOpenGLATI` (14), `TextureCombinerOpenGLARB` (8), `TextureCombinerOpenGLPlugin` base (8), plus factories for each.
- **Vertex program plugins:** `VertexProgramOpenGLNV` (34) incl. `ProgramId` helper, `VertexProgramOpenGLARB` (34), `VertexProgramOpenGLPlugin` (7), `VertexProgramBasic::VPSymbol` (1), factories.
- **Vertex buffer manager plugins:** `VertexBufferManagerOpenGLARB` (24) incl. `StaticBufferData`, `VertexBufferManagerOpenGLSW` (12), `VertexBufferManagerOpenGLPlugin` (10), `StaticBuffer` helper (1), factories.
- **Plugin factories (namespace evidence):** factories for display variants (`DisplayOpenGL`, `DisplayOpenGLXTerm`) and for combiner/vertex-program/VB manager capability implementations; `JetGetPluginsRenderOpenGL` is the exported aggregator.

### 2.4 External Cg/CgGL imports (thunks evidenced in function map)
- `CGGL.DLL` (15): `cgGLSetParameter1f/2f/3f/4f`, `cgGLSetParameter1fv/3fv/4fv`, `cgGLSetMatrixParameterfc/fr`, `cgGLSetMatrixParameterArrayfr`, `cgGLSetupSampler`, `cgGLSetManageTextureParameters`, `cgGLRegisterStates`, `cgGLSetDebugMode`, `cgGLGetLatestProfile`.
- `CG.DLL` (13): `cgGetArrayParameter`, `cgCreateContext`, `cgCreateEffectFromFile`, `cgDestroyEffect`, `cgDestroyContext`, `cgGetProfileString`, `cgGetNamedTechnique`, `cgValidateTechnique`, `cgGetTechniqueName`, `cgGetNamedEffectParameter`, `cgGetFirstPass`, `cgSetPassState`, `cgResetPassState`.
- No loose shader assets in content: effects are built-in and/or engine-owned paths; SpeedTree `.fx` compiled through this path (MODLOG).

### 2.5 Parity reference: RenderDirectX9
- `exports/RenderDirectX9_7c.tsv`: 41 exports, `JetGetPluginsRenderDirectX9`, DX9 display/descriptor/texture classes and CG shader classes (`CGShaderConstantDirectX9`, `CGShaderLoaderDirectX9`, `CGShaderTechniqueDirectX9`) - confirming shared shader-abstraction concepts across backends but different API/device implementations.
- `RenderDirectX9_7c.dll.functions.tsv`: 481 mapped functions (smaller than OpenGL map), DX9-specific namespaces (`DisplayDirectX9`, `DisplayTextureDirectX9`, `VertexBufferManagerDirectX9`, `TextureCombinerDirectX9`, `VertexProgramDirectX9`, `StateManagerD3D9`, etc.). Useful for understanding Display/plugin slot contracts and behavior parity, not code reuse.

## 3. Dependencies

### 3.1 Jet/engine dependencies (evidenced/planned)
- **RenderSystem_7c (primary, evidenced):** `RenderOpenGL -> RenderSystem 178` imports (`MODLOG.md`, `notes/RenderSystem.md`). Expected to consume/implement RenderSystem abstractions: `Display` and derived display infrastructure, `DisplayTexture`/texture creation and formats, vertex type/buffer/program/combiner/manager interfaces, render state/material/light hooks, kernel/display initialization contracts. **Critical sequencing constraint:** RenderSystem U9 (Display, RenderKernel, managers/statics/factories/plugin registration) is last and currently upstream work is in-flight (U7). Many RenderSystem-side internals are still owned by the original and evolve.
- **Kernel/KernelStandard/KernelResource/KernelResources:** RenderSystem itself depends only on these (all reported ours in project status); RenderOpenGL likely relies on Jet memory/strings/containers/plugin/result types and CRT conventions consistent with the rest of Jet. Exact direct import list beyond RenderSystem/Cg/system DLLs not fully enumerated here; verify from `dump_imports.py`/IAT during implementation.
- **Render3DCore/Render3DFoundation:** higher-level scene/render consumers of Display/RenderSystem state (not direct build dependency of backend DLL, but behavioral contract). PORT_PLAN schedules RenderOpenGL in/after Render3DCore/Foundation work (8e).
- **SpeedTree path:** `plugins/SpeedTreeShaders/*.fx` compiled via RenderOpenGL Cg path; TNISpeedTree integration is external plugin/TNI-stream based, but backend must keep CgFX behavior compatible for SpeedTree materials/effects.

### 3.2 External/system/runtime dependencies (evidenced/inferred)
- **Cg 2.2-era runtime (32-bit original):** `cg.dll`, `cgGL.dll` import thunks evidenced. Used for CgFX effects, techniques/passes, parameter binding, GL state registration/profile selection.
- **OpenGL/WGL/Win32:** implied by `WGL` context/interface classes, `HDC/HGLRC`-style descriptor methods, PBuffer/shared/non-render contexts, window/display enumeration/creation in `DisplayLibrary*`/`DisplayInterface*`. System DLL imports (opengl32, gdi32, user32, kernel32) should be confirmed from PE imports before coding.
- **MSVC8 CRT:** module is an original-era x86 MSVC binary; map shows `MSVCR80.DLL` thunks/`type_info` usage. Drop-in build must match ABI/CRT expectations of surrounding Jet modules (project convention: shared CRT /MD per recent MODLOG note).
- **Renderer selection:** `plugins.ini` registers OpenGL and DirectX9 in same renderer group; launcher picks backend.

## 4. Proposed implementation units and order

Because RenderOpenGL is tightly coupled to RenderSystem's abstract Display/texture/program/combiner/buffer contracts and those contracts are not fully reconstructed/owned yet, implementation should be staged to minimize churn and avoid fighting in-flight RenderSystem changes.

### Phase A - prerequisites and discovery (before/alongside early coding)
1. **Read/record ownership and contracts:** confirm RenderSystem U9 API/layout status (`Display`, `DisplayTexture`, `VertexBufferManager`, `VertexProgram`/basic, `TextureCombiner`, `DisplayDescriptor`, render context/state objects, plugin factory signatures, any data exports/statics touched). Coordinate with RenderSystem agent; do not duplicate or fork headers prematurely.
2. **PE/import/vtable contract capture:** dump RenderOpenGL imports (all DLLs/functions), export ordinals/signatures, RTTI/vftables, and key class layouts/sizes/offsets from Ghidra/asm. Identify which vfunc slots correspond to RenderSystem base slots (especially `DisplayTextureOpenGL` vtable RVA 0x27644 and descriptor vtable RVA 0x27820). Extract/attribute `Global` helpers by call graph from exported/plugin roots.
3. **Runtime behavior oracle setup:** menu/launcher selection checks, GL capability/profile logging (if safe), screenshot/log baselines on original module; plan differential harness only where deterministic (pure descriptor/format helpers may be testable in-process with stand-ins, but most GL/WGL behavior is context/device dependent).

### Phase B - drop-in scaffolding
4. **U0 proxy DLL:** forwarding module preserving 51 exports, ordinals, names, and data exports to `RenderOpenGL_7c_orig.dll`; include `JetGetPluginsRenderOpenGL` forwarding and vtable/data export identity. Verify in-game (OpenGL selected) and JetLog parity as a pure harness baseline.
5. **Plugin/type registration skeleton:** reconstruct `JetGetPluginsRenderOpenGL` and factory registration once RenderSystem plugin factory interfaces are known. Keep original implementations for display/combiner/program/VBM until each unit is ready; factories can be introduced incrementally if safe.

### Phase C - self-contained/platform-adjacent units (after contract capture)
6. **Descriptors/contexts:** `ContextDescriptorOpenGL`, `ContextDescriptorOpenGLWGL`, `DisplayDescriptorOpenGL` (exported methods, simple state). Low behavioral complexity, but field layout must match RenderSystem callers.
7. **DisplayTextureOpenGL (high value, exported):** implement exported texture lifecycle/API in stages: object construction/init/copy, format mapping (`CheckFormat`, GetJetFormat/GetAPITexture), create/destroy, bind/unbind and sampler attributes, load/subload/copy/readback paths, render-context/PBuffer-related methods. This is a key integration point and already partially specified by exports and RenderSystem `DisplayTexture` semantics (see `notes/RenderSystem.md` U6c1/U9 interactions).
8. **WGL interface/library glue:** `DisplayInterfaceOpenGL/WGL`, `DisplayLibraryOpenGL/WGL`, `NonRenderContextWGL`, shared/PBuffer contexts, save buffer. `DisplayLibraryOpenGL` contains a very large function (~10.7 KB namespace total in 3 funcs) likely handling device/mode/context/extension setup - treat as a dedicated risky unit.

### Phase D - main display and render-path integration (depends on RenderSystem Display/U9)
9. **DisplayOpenGL / DisplayOpenGLXTerm:** core backend implementing RenderSystem `Display` virtuals, render-state submission, draw calls, capability flags, interaction with vertex buffers/programs/combiners/material/lighting state. This is the largest behavioral surface (159 funcs combined) and must align with RenderSystem U9 expectations and Render3D usage.
10. **Buffer/program/combiner plugins:** `VertexBufferManagerOpenGL{ARB,SW,Plugin}`, `VertexProgramOpenGL{NV,ARB,Plugin}` (+ basic/symbol helpers), `TextureCombinerOpenGL{NV,NVfp,ATI,ARB,Plugin}` and factories. Implement by capability family; prefer starting with paths actually selected on target test hardware/menu content, but keep registration/selection logic compatible.

### Phase E - Cg/CgFX shader path (can be parallelized carefully, but validate early)
11. **CGShaderLoader/Technique/Constant (OpenGL):** reconstruct CgFX effect loading and pass/parameter binding over evidenced `cg/cgGL` calls. Validate profile selection (`cgGLGetLatestProfile`, vp40/fp40/gp4*), technique validation, sampler/texture parameter management, and matrix/array parameter setting. Preserve behavior for built-in effects and SpeedTree shader compilation/loading path.

### Phase F - stabilization and x64-oriented refactor readiness
12. **Parity, cleanup, and abstraction boundaries:** remove dependence on original internals only when safe; document GL extension assumptions and isolate platform/context/GL-call sites for future backend swap. For x64 phase: replace Cg 2.2 with decided Cg 3.1 x64 runtime API-compatible path first; defer GLSL translation. Audit pointer-sized handles/state (HDC/HGLRC/GL pointers), struct packing, and any RenderSystem ABI changes introduced by x64 rebuild.

## 5. Risks and hard problems

### 5.1 State management and context correctness
- OpenGL is highly stateful; backend must mirror RenderSystem's expected state cache/dirtying semantics and interaction with materials, vertex programs/combiners, texture stages/units, render targets, and multipass draws.
- WGL specifics: current/render contexts, shared contexts, PBuffer/non-render contexts, render-to-texture flows (`CreateRenderContext`, `SubloadFromFrame`, save buffer helpers), thread affinity, context loss/recreation behavior. Any mismatch can cause black screens, flicker, wrong textures, or hard-to-reproduce hangs/crashes.
- Static mutable state in exported data (`DisplayTextureOpenGL::{anisotropySupport,bound,canAutoMip,numObjects}`) and likely internal globals must have correct lifetime/init order and cross-module visibility.

### 5.2 Shaders / Cg path
- CgFX path is central (evidenced imports and loader namespace): effect file loading, named techniques/passes, parameter lookup, state application/reset, sampler/texture parameter management. Failures may degrade silently or break large parts of rendering.
- Runtime/profile compatibility: original uses cg/cgGL (2.2-era implied); x64 plan chooses Cg 3.1 x64 runtime. Need to confirm behavior/profile parity (`vp40/fp40/gp4vp/gp4fp`) and debug-mode differences.
- SpeedTree shader compilation depends on RenderOpenGL Cg path (`bin/plugins/SpeedTreeShaders/*.fx`) - regression can break SpeedTree vegetation even if core scene appears fine.
- Future GLSL backend (PORT_PLAN) is non-trivial: CgFX semantics, pass/state management, and parameter naming/binding conventions must be captured before translation.

### 5.3 Extension/capability plugin complexity
- Multiple combiner/vertex-program/VB-manager implementations (NV/NVfp/ATI/ARB/SW/plugin) suggest runtime selection by GPU/extensions. Reproducing selection/registration order and fallback behavior is important for compatibility across hardware/drivers.
- `DisplayOpenGLXTerm` and large `Global` helper pool may encode extension strings/probing, capability tables, and fast paths; unattributed code increases risk of missing edge-case support.

### 5.4 RenderSystem coupling and in-flight upstream work
- 178 RenderSystem imports: this backend cannot be treated in isolation. Contract depends on `Display`, `DisplayTexture`, vertex/geometry, material/lighting, and manager interfaces whose ownership is moving in RenderSystem (U7 ongoing, U9 last). Implementing against stale assumptions will create rework/conflicts.
- Exported RenderSystem data/statics and vtable slot layouts must match exactly (32-bit MSVC8 virtual dispatch/calling conventions). `DisplayTextureOpenGL` mixes calling conventions (`__thiscall`/`__cdecl`/`__thiscall` returns) as evidenced by exports.

### 5.5 x64 considerations (future phase, but should inform design now)
- WGL/OpenGL handle types and pointers become 64-bit; audit all stored HDC/HGLRC/GL function pointers/texture IDs if represented as pointer-sized values.
- Struct/layout/ABI changes will occur when RenderSystem and backend are rebuilt together for x64; keep a clear mapping from 32-bit offsets/vslots to named fields.
- x87/SSE floating behavior: Win32 path benefits from 53-bit x87 precision in OpenGL context; x64 math/codegen differs. Rendering/math parity issues may appear as subtle visual differences.
- Cg 3.1 x64 runtime availability/redistribution and any API deprecations vs 2.2 must be validated; keep shader-loading code abstractable for later GLSL.

### 5.6 Testing/observability challenges
- In-process differential testing against original OpenGL DLL is hard for real GL calls (requires window/context, driver-dependent results, non-deterministic timing/state). Pure logic (descriptor parsing, format tables, registration data) is easier to isolate.
- Primary confidence path will likely be: proxy parity, targeted unit diffs where feasible, menu/frame progression checks, screenshot/JetLog comparisons on fixed content/hardware, and A/B with RenderDirectX9-selected mode for content-level sanity.

## 6. Cross-agent notes

### For `RenderSystem_7c` (highest coupling)
- `RenderOpenGL_7c` is a concrete consumer/implementor of RenderSystem display/texture/shader/buffer abstractions (178 imported RenderSystem exports evidenced). Any change to `Display`, `DisplayTexture`, `DisplayDescriptor`/context types, vertex/program/combiner/buffer manager vtables, plugin factory interfaces, or module-owned statics directly affects this backend.
- Before implementing backend units, need stable/recorded contracts for: Display virtual interface and initialization flow (`RenderKernel`/`Display` creation, `InitializeState` side effects), DisplayTexture creation/binding API and format conversion hooks, render-target/PBuffer expectations, vertex buffer upload/draw submission API, vertex program/texture combiner manager ownership, and plugin registration (`JetGetPlugins*`).
- Coordinate around RenderSystem U9 and remaining data/statics migration; `COORDINATION.md` marks RenderSystem source/tests/docs as in-flight. This report should inform sequencing, not trigger parallel edits to RenderSystem.

### For `Render3DCore_7c` / `Render3DFoundation_7c` / `Render3DExtension_7c`
- These modules drive scene/render submission through RenderSystem and ultimately the active Display backend. Backend correctness requirements come from their draw/state/material/geometry usage patterns.
- When RenderOpenGL is implemented/tested, use Render3D call paths and real in-game scenes (menu, fixed session) as behavioral oracles; regressions may manifest as state leaks between passes, incorrect multipass/material binding, or geometry/program mismatch rather than outright crashes.
- Extension/plugin capability expectations (combiner/program features) should be validated against Render3D feature usage, not just minimal menu rendering.

### For `RenderDirectX9_7c` (parallel backend)
- RenderDirectX9 shares the same launcher/plugin-group role and similar abstraction targets (Display/texture, CG shader classes, combiner/vertex program/VB manager families, plugin registration) but uses D3D9 device/state model (`StateManagerD3D9`, DX9-specific formats/resources). Use it as a contract/reference for:
  - expected plugin kinds and factory/registration behavior (`JetGetPluginsRender*`),
  - Display/texture virtual method semantics and calling conventions,
  - Cg integration shape (DX9 has `CGShader*DirectX9` counterparts) and differences vs `cgGL*` path.
- Parity goal is user-visible behavior under the same content/session when the respective renderer is selected; implementation details will diverge. Step 8f (DX9, d3dx9_42 -> d3dx9_43 x64) should stay aligned on shared RenderSystem contracts to avoid backend-specific hacks leaking upward.

### General project notes
- Live renderer is OpenGL at menu, so integration risk is high early; keep a reversible proxy/fallback workflow (`*_orig.dll`) and prefer incremental units with game checks.
- Cg 3.1 x64 runtime is the decided x64 path; avoid committing to Cg 2.2-only assumptions in new abstractions. GLSL modernization is a later backend task after semantics are preserved.
- This analysis is based on exported maps/notes available now; before implementation, refresh against current Ghidra/decomp artifacts for `Global` attribution, full PE imports, and exact RenderSystem vslot/layout contracts.

## Scaffold (Wave 2/4)

**Date:** 2026-11-19 (scaffold prep, module-owned only). **Proxy-first:** no exports enabled, no behavior implemented, not wired into shared build/progress files.

### Files created
- `src/jet/graphics/RenderOpenGL_7c/implemented.txt` — forwarding-default manifest (all 51 exports remain original/proxy); documents JetGetPluginsRenderOpenGL, exported vtables/statics, and exported descriptor/context/texture methods as not-to-enable without recovery.
- `src/jet/graphics/RenderOpenGL_7c/RenderOpenGL.h` — evidence-indexed placeholder header documenting module role, export/data inventory (ords/RVAs), evidenced internal clusters (display/WGL, texture, Cg/CgFX, combiner/vertex-program/VBM/plugin families), and Global/unattributed surface; declares `extern "C" void JetGetPluginsRenderOpenGL();` only as an unevidenced/TBD placeholder (not exported).
- `src/jet/graphics/RenderOpenGL_7c/RenderOpenGL.cpp` — placeholder TU with staged TODOs and an unexported `JetGetPluginsRenderOpenGL` stub body that explicitly does nothing (no-op unsafe) pending decomp/recovery.

### Stubbed vs deferred
- **Stubbed (placeholders only, not exported):** `JetGetPluginsRenderOpenGL` symbol declaration/body scaffold with warnings; namespace/file structure and evidence comments. No C++ exported methods, no vtables, and no static data definitions provided.
- **Deferred (must remain forwarded/original until evidenced):** every export in `exports/RenderOpenGL_7c.tsv` (51), including plugin entry registration semantics, both vtable data exports, 4 `DisplayTextureOpenGL` statics, and all 44 exported descriptor/context/texture methods; all 1052 mapped/internal routines (537 Global/unattributed), GL/WGL context/device/render-target behavior, CgFX/cg/cgGL integration, capability probing/selection, and RenderSystem contract alignment.

### Key ABI / RenderSystem-contract / GL-Cg TODOs and risks
- **ABI/layout/vtable:** 32-bit MSVC8 drop-in requires exact export ordinals/names/signatures/CC (mixed `__thiscall`/`__cdecl` per exports), class layouts/offsets, vtable/vbtable/slot order and base-slot mapping (esp. `DisplayTextureOpenGL` RVA 0x27644, `DisplayDescriptorOpenGL` RVA 0x27820, DisplayOpenGL/XTerm large slot sets per vtnames), RTTI, and exported static storage/init/lifetime (`anisotropySupport`, `bound`, `canAutoMip`, `numObjects`). No layouts/slots guessed.
- **RenderSystem contract (critical, in-flight):** backend consumes/implements RS abstractions (Display, DisplayTexture, descriptor/context, vertex/program/combiner/buffer-manager, RenderKernel/display init/state, plugin factories). MODLOG/notes evidence ~178 RS imports; **RS U7 in-flight and U9 last/contract-critical** — must coordinate and recover exact contracts/vslots/statics ownership before enabling local behavior or wiring.
- **GL/WGL/platform state:** WGL HDC/HGLRC/current/shared/non-render/PBuffer contexts, render-to-texture/save-buffer paths, extension/capability probing and fallback, thread affinity/loss/recreation, and state-cache/dirtying semantics are unknown and must mirror original. `DisplayLibraryOpenGL` contains very large routine(s) (risk hotspot).
- **Cg/CgFX:** 13 `cg.dll` + 15 `cgGL.dll` CgFX/effect/technique/pass/parameter/profile/state thunks evidenced; profiles/behavior (vp40/fp40/gp4*), built-in effects, and SpeedTree `bin/plugins/SpeedTreeShaders/*.fx` compilation path must remain compatible. Original cg/cgGL 2.2-era implied; x64 plan targets Cg 3.1 (GLSL deferred) — keep abstractions portable, do not assume 2.2-only behavior.
- **Capability plugins:** NV/NVfp/ATI/ARB/SW combiner, NV/ARB vertex-program, ARB/SW VBM families and factories imply runtime selection/registration-order sensitivity; 537 Global funcs unattributed (helpers/tables/probing/glue).
- **Testing:** live OpenGL menu renderer => high regression visibility; GL/WGL behavior hard to diff deterministically. Proxy/forwarding parity is the safe baseline.

### Later build-wiring steps (separate, coordinated claims; not done here)
1. Capture PE imports/deps and recover exports/vtables/RTTI/layouts/vslots/data ownership from original; attribute Global/plugin roots; align with RS U9 contracts (no shared-file edits while RS in-flight).
2. Add module-owned proxy/forwarding build target under `src/jet/graphics/RenderOpenGL_7c/` preserving 51 exports/ordinals and data exports to `RenderOpenGL_7c_orig.dll`; verify launcher-OpenGL/menu parity.
3. Wire into shared build/progress only in a serialized claim: update `src/jet/CMakeLists.txt`, `README.md`/`PORT_PLAN.md` only if appropriate, and `tools/progress_exports.tsv` (per project rules) with minimal, accurate entries.
4. Enable local implementations incrementally (descriptors/contexts → DisplayTextureOpenGL → WGL/library → Display/display-core → capability plugins → Cg path) only after evidenced ABI/RS/GL-Cg parity and targeted validation; keep reversible fallback to original.
