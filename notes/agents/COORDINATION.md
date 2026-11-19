# Agent coordination board

Purpose: parallel work on unfinished modules without stepping on each other.

## Rules
- Do **not** edit shared source/build/doc files from parallel analysis tasks: `src/**`, `CMakeLists.txt`, `README.md`, `PORT_PLAN.md`, `MODLOG.md`, `tools/progress_exports.tsv`, `implemented.txt` etc., unless you are the explicitly assigned implementation owner and have recorded a claim below.
- Do **not** run builds/tests that contend on shared build directories or install/game files unless coordinated.
- Do **not** commit or push.
- Write your findings only to your assigned file under `notes/agents/`.
- Append a one-line status update to `notes/agents/STATUS.md` when done (module, file written, key next step, blockers/conflicts).

## Current tree warnings (2026-11-19 snapshot)
- `trainz12-x64` has uncommitted in-progress RenderSystem U7 work: modified `PORT_PLAN.md`, `README.md`, `notes/RenderSystem.md`, `src/jet/graphics/RenderSystem_7c/MaterialInstance.cpp`, `src/jet/graphics/RenderSystem_7c/implemented.txt`, `src/jet/tests/diff_rs_u7.cpp`, `tools/progress_exports.tsv`; untracked `src/jet/graphics/RenderSystem_7c/MaterialLifetime.cpp`, `src/jet/graphics/RenderSystem_7c/MaterialStatics.cpp`, `tools/check_exports.py`.
- Treat RenderSystem_7c source/test/docs as **owned/in-flight** until committed. Analysis may read it; implementation agents must not modify it without coordination.
- WaterSystem_7c has known pre-existing build issue noted in MODLOG (check before claiming implementation).

## Consolidated output (2026-11-19)
- **All 14 module reports completed:** `notes/agents/*_7c.md`.
- **Unified dependency/order plan:** `notes/agents/CONSOLIDATED_PLAN.md` (dependency view, execution waves, parallelization guidance, per-module next actions, cross-cutting blockers).
- **Highlights:** stabilize/commit RS U7 WIP and fix WaterSystem pre-existing build break before dependent implementation; Wave 1 low-coupling candidates `DebugInterface_7c`, `InputDirectX_7c`, (with boundary agreement) `InterfaceResources_7c`; render spine centered on `RenderSystem_7c` → 3D stack/backends/Water; defer `Lexer_7c`/`DSMoviePlayer_7c` until runtime evidence.

## Claims
Add a line: `- <module>: <status> - <session/agent> - <scope/files> - <timestamp>`
- `WaterSystem_7c`: in-progress (partially unblocked) - lead (main) - fixed reported VS18 compile blockers (`sizeof(PhysicsWaterGeometry)` 0x7c via pad5c in `src/jet/include/jet/WaterGeometry.h`, `src/jet/tests/diff_ws_u1.cpp` compile issues); `diff_ws_u1` builds, `WaterSystem_7c` still fails to link (63 unresolved: import/signature/export mismatches). Continue targeted ABI/link fixes under claim, no behavior changes - 2026-11-19
- `DebugInterface_7c`: scaffolded (pending ABI verify + build wiring claim) - subagent ses_e0f130288ffejGyQsRZKGybtqG - `src/jet/debug/DebugInterface_7c/*`, `notes/agents/DebugInterface_7c.md` (module-owned) - 2026-11-19
- `InputDirectX_7c`: scaffolded (pending ABI/DI verify + build wiring claim) - subagent ses_e0f130286ffeEjpObrMJWQx1hJ - `src/jet/input/InputDirectX_7c/*`, `notes/agents/InputDirectX_7c.md` (module-owned) - 2026-11-19
- `InterfaceResources_7c`: scaffolded (pending ABI/layout/resource-format verify + build wiring claim) - subagent ses_e0f08edc9ffef6w1t9dnmbIe5g - `src/jet/ui/InterfaceResources_7c/*`, `notes/agents/InterfaceResources_7c.md` (module-owned) - 2026-11-19
- `Render3DCore_7c`: scaffolded (pending ABI/vtable/RS-contract verify + build wiring claim) - subagent ses_e0ebeda9effezehoaLCWITLO1V - `src/jet/graphics/Render3DCore_7c/*`, `notes/agents/Render3DCore_7c.md` (module-owned) - 2026-11-19
- `DebugSupport_7c`: scaffolded (pending ABI/runtime-trigger verify + build wiring claim) - subagent ses_e0ebeda99ffeKvH6zm6lrKSNLG - `src/jet/debug/DebugSupport_7c/*`, `notes/agents/DebugSupport_7c.md` (module-owned) - 2026-11-19
- `Render3DFoundation_7c`: scaffolded (pending ABI/layout/registration + RS/3D-contract verify + build wiring claim) - subagent ses_e0eb8eb0bffdZ0EZTh6fQeUl2t - `src/jet/graphics/Render3DFoundation_7c/*`, `notes/agents/Render3DFoundation_7c.md` (module-owned) - 2026-11-19
- `Render3DExtension_7c`: scaffolded (pending ABI/layout/3DCore-RS contract + runtime-trigger verify + build wiring claim) - subagent ses_e0eb8eb09ffezn6Pl0KjgeDwWm - `src/jet/graphics/Render3DExtension_7c/*`, `notes/agents/Render3DExtension_7c.md` (module-owned) - 2026-11-19
- `RenderOpenGL_7c`: scaffolded (pending ABI/RS Display-contract/GL-WGL-Cg verify + build wiring claim) - subagent ses_e0eb8eafaffencx7jiUqSJE9zp - `src/jet/graphics/RenderOpenGL_7c/*`, `notes/agents/RenderOpenGL_7c.md` (module-owned) - 2026-11-19
- `RenderDirectX9_7c`: scaffolded (pending ABI/RS Display-contract/D3D9-D3DX-Cg verify + build wiring claim) - subagent ses_e0eb8eaf1ffes1EdOyiWocE98L - `src/jet/graphics/RenderDirectX9_7c/*`, `notes/agents/RenderDirectX9_7c.md` (module-owned) - 2026-11-19
- `InterfaceSystem_7c`: scaffolded (pending ABI/layout/vtable/UI-deps verify + build wiring claim) - subagent ses_e0eb8ead9ffeuvpv1CYbF2zw2z - `src/jet/ui/InterfaceSystem_7c/*`, `notes/agents/InterfaceSystem_7c.md` (module-owned) - 2026-11-19
- `Lexer_7c`: deferred (Wave 6) - no runtime load/use evidence; scaffold only if evidenced - 2026-11-19
- `DSMoviePlayer_7c`: deferred (Wave 6) - not loaded at menu/trigger+contract unconfirmed; scaffold only if evidenced - 2026-11-19
