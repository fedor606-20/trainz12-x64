# Render3DCore_7c: parallel workstreams

Status: 2026-11-24. The module plan is `notes/Render3DCore.md`. U0 and U1a are merged (25/2500 exports). This file
splits the remaining exports so several agents can work at once without touching the same source or export list.

## Rules for every workstream

- Work in your own copy: `D:\universal modder\r3d-agents\<stream>\src\jet`. The main tree is
  `D:\universal modder\trainz12-x64`; read it for plans and patterns but do not edit it.
- Own only the classes in your export slice (`notes/agents/r3d_workstreams/<stream>.tsv`). Do not implement an export
  from another stream.
- Do not edit shared files in your copy either: `src/jet/CMakeLists.txt`, `README.md`, `MODLOG.md`, `PORT_PLAN.md`,
  `notes/Render3DCore.md`, `implemented.txt`. Add only:
  - `src/jet/graphics/Render3DCore_7c/<Unit>*.cpp`;
  - `src/jet/include/jet/<Unit>*.h` (one header per stream; do not edit `Render3DCore.h`);
  - `src/jet/tests/diff_r3d_<stream>.cpp`;
  - `src/jet/graphics/Render3DCore_7c/implemented.d/<stream>.txt` (one mangled name per implemented export).
- The fragment is merged by `tools/merge_implemented.py`; duplicate names are rejected. Add a name only when the
  symbol is defined and the build links.
- Do not run the game, edit the install, commit or push.
- Use the pre-decompiled material: `notes/agents/r3d_workstreams/<stream>_decomp.c` and `<stream>_asm.txt`.
  Ghidra is not needed; if the material is missing an internal helper, decompile it once in a scratch project
  (`tools/ghidra_decomp_scratch.cmd`) and put the result in your report, not in the shared notes.
- Build in your own directory, e.g.
  `cmake -S D:\universal modder\r3d-agents\<stream>\src\jet -B D:\universal modder\r3d-agents\<stream>\build32 -A Win32`
  then `cmake --build ... --config Release --target Render3DCore_7c diff_r3d_<stream>`.
  The test target appears automatically when `tests/diff_r3d_<stream>.cpp` exists.
- Test directory: copy `D:\universal modder\testbin\*` to
  `D:\universal modder\r3d-agents\<stream>\testdir`, rename `Render3DCore_7c.dll` there to
  `Render3DCore_7c_orig.dll`, copy your built `Render3DCore_7c.dll` beside it, then run
  `diff_r3d_<stream>.exe <testdir> 1000`. Keep the test deterministic (fixed seed) and compare raw bytes/floats.
- Report to `notes/agents/<stream>.md` in the main tree (unique file): status, implemented exports, forwarded
  exports and why, test command and result, layout/NaN/ABI facts, files, blockers.

## Wave 1 (spawned now)

| Stream | Classes | Exports | Decompiled | Status |
|---|---|---:|---|---|
| `u1b_plane` | `Plane` | 44 | `u1b_plane_decomp.c` | ✅ 41 local, 3 forwarded |
| `u1c_orientedbox` | `OrientedBox` + the 9 remaining `AxisAlignedBox` exports | 37 + 9 | `u1c_orientedbox_decomp.c` | ✅ 42 local, 4 forwarded |
| `u1d1_lines` | `Line`, `LineSegment`, `Ray`, `Line2D`, `LineSegment2D`, `Circle2D`, `Polygon2D` | 175 | `u1d1_lines_decomp.c` | ✅ 164 local, 11 forwarded |
| `u1d2_solids` | `Polygon`, `Sphere`, `Ellipsoid`, `Ellipse`, `Cylinder`, `Cone`, `Circle`, `Box` | 129 | `u1d2_solids_decomp.c` | ✅ 113 local, 16 forwarded |
| `u2_splines` | `BSpline*`, `BezierSpline*`, `CardinalSpline*`, `HermiteSpline*`, `Spline*` | 131 | `u2_splines_decomp.c` | ✅ 120 local, 11 forwarded |

`AxisAlignedBox`: U1a (`implemented.txt`, 25 exports) is already merged. U1c owns the other 9 and must not touch the
U1a fragment or `AxisAlignedBox.cpp`; it adds `AxisAlignedBoxTail.cpp` for the missing methods.

Cross-unit calls: if a method needs a class from another stream, leave that method forwarded unless the other class
is already local. `Render3DCore_7c_orig.lib` is generated in `D:/trainz12-decomp/implib` and linked, so calling a
forwarded export by name is allowed, but do not copy another stream's implementation into your files.

## Wave 2 (after wave 1 merges)

| Stream | Classes | Exports | Waiting on |
|---|---|---:|---|
| `u4_caches` | `AlphaBlendCache`, `UnsortedAlphaBlendCache`, `AlphaMaskCache`, `MaterialChunkCache`, `GenericMaterialChunkCache`, `ParticleFXCache`, `ProjectorCache` | 129 | 🟡 56 local, 73 forwarded; singletons/tree rebalancer later |
| `u5a_materials` | `Material`, `NoTexture`, `OneTexture*`, `Gloss`, `Reflective`, `Billboard`, `AutoTex`, `ColorOneTexture`, `MVHSelect`, `GenericShader`, their plugins | 374 | partial: 196 local, 178 forwarded; render/chunk paths pending |
| `u5b_multipass` | `MultiPassRenderState`, `MultiPassLightRender`, the MultiPass materials/plugins/chunks | 254 | 🟡 44 local, 210 forwarded; second pass after U5a/U4/U6 |
| `u9a_particle` | `Particle`, `ParticleAlias`, `ParticleStructure`, `ViewPortParticle` | 149 | done 2026-11-24: 143 local, 6 forwarded |
| `u3_bounds` | `ParticleBoundingVolume`, `ParticleAxisAlignedBoundingBox`, `ParticleBoundingSphere`, `ParticleOrientedBoundingBox`, `ParticleVisibleBoundingBox` | 101 | U9a |
| `u8_camera` | `Camera`, `ViewPort`, `DebugViewPort` | 159 | U9a, U3 |
| `u10_primitives` | `RenderPrimitive`, `Render2DPrimitive`, `Render3DPrimitive`, `RenderBox`, `RenderSphere`, `RenderPlane`, `RenderLines`, `RenderTube`, ... | 230 | U9a |
| `u7_lights` | `Light`, `AmbientLight`, `OmniLight`, `ParallelLight`, `SpotLight`, `CubeMapLight` | 95 | Particle/U9a, DisplayLight |
| `u6_geometry` | `GeometryChunkTemplate` / `GeometryChunkNmlTemplate` instantiations and `GenericGeometryChunk*`: 446 template functions but only 13 direct exports; a code-size unit, not an export race | 13 | done: 13 local, 58 template instantiations |
| `u9b_scene` | `AttachmentPoint/Observer`, `Cell`, `Portal` families, `Mirror`, `ReferencePoint`, `Location`, `Corona`, `Function`, bone/skeleton modifiers | ~200 | U9a |
| `u11_statics` | `BackgroundLoadingThread`, `BackgroundResource`, `Facility`, `JetGetPluginsRender3DCore`, exported singletons/statics | ~50 | everything above |

## Wave 3 (spawned 2026-11-24, after U9a Particle merged)

| Stream | Classes | Exports | Material / status |
|---|---|---:|---|
| `u3_bounds` | `ParticleBoundingVolume`, `ParticleAxisAlignedBoundingBox`, `ParticleBoundingSphere`, `ParticleOrientedBoundingBox`, `ParticleVisibleBoundingBox` | 101 | done: 89 local, 12 forwarded |
| `u8_camera` | `Camera`, `ViewPort`, `DebugViewPort` | 159 | done: 138 local, 18 forwarded |
| `u10_primitives` | `RenderPrimitive`, `Render2DPrimitive`, `Render3DPrimitive`, `RenderBox`, `RenderSphere`, `RenderPlane`, `RenderLines`, `RenderTube`, ... | 230 | done: 168 local, 62 forwarded |
| `u7_lights` | `Light`, `AmbientLight`, `OmniLight`, `ParallelLight`, `SpotLight`, `CubeMapLight` | 95 | done: 90 local, 5 forwarded |
| `u9b_scene` | `AttachmentPoint/Observer`, `Cell`, `Portal` families, `Mirror`, `ReferencePoint`, `Location`, `Corona`, `Function`, bone/skeleton modifiers | 209 | done: 118 local, 91 forwarded |
| `u3b_bounds` | AABB/Sphere/OBB `IsWithinView` and renderable methods | 12 | done: 12 local |
| `u9b2_modifiers` | bone/skeleton/modifier tail | 64 | done: 55 local, 9 forwarded |

## Merge checklist (main agent)

1. Copy each stream's new `.cpp`, `.h`, `tests/diff_r3d_<stream>.cpp` and `implemented.d/<stream>.txt` into the main
   tree. Do not copy its `src/jet/CMakeLists.txt` or `implemented.txt`.
2. Run `cmake -S src/jet -B build32 -A Win32`, build `Render3DCore_7c`, then every `diff_r3d_*` target.
3. Run `python -I tools/check_exports.py build32/Release/Render3DCore_7c.dll D:/trainz12-decomp/exports/Render3DCore_7c.tsv`.
4. Merge the reports into `notes/Render3DCore.md` and `MODLOG.md`, refresh the README progress block with
   `python -I tools/progress.py --exports D:/trainz12-decomp/exports --write`.
5. Only then start the next wave.
