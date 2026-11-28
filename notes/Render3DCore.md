# Render3DCore_7c: reconstruction plan

Status: 2026-11-24. Wave 1 is merged: U0 (proxy), U1a (`AxisAlignedBox`), U1b (`Plane`), U1c (`OrientedBox` +
`AxisAlignedBox` tail), U1d1 (lines and 2D), U1d2 (polygons and solids) and U2 (splines). **505/2500 exports** are
local and differentially verified; the rest forwards to `Render3DCore_7c_orig.dll` through the generated `.def`.
Wave 2 (U3, U4, U5, U6, U8, U9, U10) is next; the per-unit export slices and pre-decompiled listings are in
`notes/agents/r3d_workstreams/`.

Sources: `src/jet/graphics/Render3DCore_7c/`, header `src/jet/include/jet/Render3DCore.h`, test
`src/jet/tests/diff_r3d_u1.cpp`. Decompiled from the pristine `D:\trainz12-decomp\bin\Render3DCore_7c.dll` with the
Ghidra project (`ghidra_scripts/DecompileAt.java`, `DumpAsm.java`); the Jet SDK RC1 headers (`Plane.h`, `Camera.h`,
`Line.h`, `Sphere.h`, ...) are a naming/layout reference but not a layout guarantee (TS12 changed some fields, e.g.
`Sphere`'s radius sits at +0x10 after the vftable, while the SDK header's order alone would say +0xc).

## The module

- 958,464-byte DLL, 458 KB of code, 3,168 non-thunk functions, 2,500 exports.
- Imports (dumpbin): `KernelStandard_7c` (the most), `Kernel_7c`, `KernelResource_7c`, `RenderSystem_7c` (320
  exports), `DebugInterface_7c` (3 data flags: `boundingVolumeMask`, `globalBoundingVolumes`, `portalState`),
  `MSVCR80`, `KERNEL32`.
- Downstream imports of this module (MODLOG "module dependencies"): `trainz.exe` 431 exports, `Render3DExtension` 134,
  `Render3DFoundation` 108, `WaterSystem` 80, `RenderSystem` 22.
- Role: the core 3D runtime above RenderSystem: scene graph (particles, cells, portals, mirrors), camera/viewport,
  lights, culling and bounding volumes, spline/primitive math, material implementations and their plugins, render
  caches and blend sorting, render primitives/debug geometry, and the concrete `GeometryChunkTemplate` catalogue.
- 802 of the 3,360 Ghidra functions are unnamed (`Global`, 48.9 KB): helpers, template bodies, thunks. They are not
  exports and are reached by RVA/virtual slot, so they move with the classes that use them, not as a unit.

Largest named classes (exports / functions / code bytes / address range; generated from the export TSV and the Ghidra
map):

| Class | Exports | Functions | Code | Range |
|---|---:|---:|---:|---|
| `Camera` | 83 | 80 | 33 KB | 100097f0..1001a460 |
| `GeometryChunkTemplate<...>` (all instantiations) | 0 | 289 | 28 KB | 1000ca00..10076078 |
| `OrientedBox` | 37 | 33 | 12 KB | 10003480..1004b600 |
| `MultiPassRenderState` | 22 | 21 | 12 KB | 1002d360..1003c900 |
| `GeometryChunkNmlTemplate<...>` | 0 | 157 | 12 KB | 1000cd20..1005f030 |
| `ViewPort` | 68 | 65 | 11 KB | 1000a960..10075f2e |
| `Particle` | 104 | 96 | 10 KB | 10006d80..10075f22 |
| `MultiPassBumpMaterialPlugin` | 19 | 19 | 9 KB | 1002f0a0..10039910 |
| `MultiPassOneTextureMaterialPlugin` | 18 | 16 | 8 KB | 10037670..10039620 |
| `RenderBox` | 15 | 14 | 8 KB | 10050440..1006a020 |
| `ReflectiveMaterial` | 30 | 21 | 7 KB | 10027b00..1005aea0 |
| `MultiPassNoTextureMaterialPlugin` | 16 | 16 | 7 KB | 10034b90..100369a0 |
| `Plane` | 44 | 40 | 7 KB | 10004fe0..10055780 |
| `SplineSegment` | 23 | 19 | 6 KB | 1000f7b0..100703b0 |
| `Polygon2D` | 37 | 23 | 6 KB | 1000a870..100587a0 |
| `AlphaBlendCache` | 48 | 46 | 6 KB | 10001090..10075ef8 |
| `Polygon` | 34 | 33 | 5 KB | 10054e60..100571b0 |
| `Ellipsoid` | 21 | 18 | 5 KB | 10021630..10025420 |
| `RenderPrimitive` | 35 | 32 | 4 KB | 10021cc0..10068e50 |
| `Cell` | 21 | 21 | 4 KB | 10012d30..1003f220 |
| `GenericShaderMaterial` | 29 | 28 | 4 KB | 10026200..100276e0 |
| `ParticleBoundingVolume` | 33 | 28 | 4 KB | 10006710..10052da0 |
| `AxisAlignedBox` | 34 | 27 | 3 KB | 10003170..1000e2c0 |
| `NoTextureMaterial` | 27 | 22 | 3 KB | 1000bf60..10076270 |

142 classes are visible in the exports. The small classes at the end of the list (lights, 2D shapes, materials'
helpers) are not less important: they are shared by the larger ones.

## Units and order

Leaf math first, then caches/materials/templates, then camera/scene state last. The managers and shared statics stay
the original's until the units that read and write them move together, as in RenderSystem U9/U15.

| Unit | Classes / contents | Exports | Status |
|---|---|---:|---|
| U0 | proxy target, `.def` forwarding, build and test harness | - | ✅ 2026-11-24 |
| U1a | `AxisAlignedBox` core (constructors, bounds, corners, clamp, containment, sphere test) | 25 | ✅ 2026-11-24 |
| U1b | `Plane` (41 of 44; `Clip`/`Split` need Polygon, `Intersect(Plane)` returns Line) | 41 | ✅ 2026-11-24 |
| U1c | `OrientedBox` (33/37) and the `AxisAlignedBox` tail (9/9); `MakeBestFit`/`Volume`/`MinimalBoxForAngles` forward to the iterative helpers | 42 | ✅ 2026-11-24 |
| U1d1 | `Line`, `LineSegment`, `Ray`, `Line2D`, `LineSegment2D`, `Circle2D`, `Polygon2D` (163/175; 3 vtables, 3 solids tests and 5 polygon/line separations forward) | 163 | ✅ 2026-11-24 |
| U1d2 | `Polygon`, `Sphere`, `Ellipsoid`, `Ellipse`, `Cylinder`, `Cone`, `Circle`, `Box` (113/129; matrix/line/plane-dependent tests forward) | 113 | ✅ 2026-11-24 |
| U2 | splines: `BSpline`, `BezierSpline`, `CardinalSpline`, `HermiteSpline`, `TCBSpline`, `Spline`, `SplineSegment`, `SplineDefinition` (120/131; `GetRatio` and resource/config I/O forward) | 120 | ✅ 2026-11-24 |
| U3 | particle bounding volumes: `ParticleBoundingVolume`, `ParticleAxisAlignedBoundingBox`, `ParticleBoundingSphere`, `ParticleOrientedBoundingBox`, `ParticleVisibleBoundingBox` | 89/101 | ✅ 2026-11-24 |
| U3b | bounds tail: AABB/Sphere/OBB `IsWithinView` and renderable methods | 12/12 | ✅ 2026-11-24 |
| U4 | caches and sorting: `AlphaBlendCache`, `UnsortedAlphaBlendCache`, `AlphaMaskCache`, `MaterialChunkCache`, `GenericMaterialChunkCache`, `ParticleFXCache`, `ProjectorCache` | 55/129 | 🟡 object-local done; singletons/tree rebalancer later |
| U5a | materials and plugins: `Material`, `NoTexture`, `OneTexture*`, `Gloss`, `Reflective`, `Billboard`, `AutoTex`, `ColorOneTexture`, `MVHSelect`, `GenericShader` | 196/374 | 🟡 state/lifetime/plugins done; render/chunk paths wait on U6 integration |
| U5b | MultiPass family: `MultiPassRenderState`, `MultiPassLightRender`, materials/plugins/chunks | 44/254 | 🟡 leaves done; second pass after U5a/U4/U6 |
| U6 | geometry chunks: `GeometryChunkTemplate` / `GeometryChunkNmlTemplate` instantiations and `GenericGeometryChunk*` | 13/13 | ✅ 2026-11-24; 58 template instantiations header-only |
| U7 | lights: `Light`, `AmbientLight`, `OmniLight`, `ParallelLight`, `SpotLight`, `CubeMapLight` | 89/95 | ✅ 2026-11-24; 6 `BatchCalculateDiffuse`/closure forward |
| U8 | `Camera`, `ViewPort`, `DebugViewPort`: frustum, clip planes, view-cone and oriented-box maths, traversal | 144/159 | ✅ 2026-11-24; 15 viewport/camera paths forward |
| U9a | `Particle`, `ParticleAlias`, `ParticleStructure`, `ViewPortParticle`, `ITree<Particle>` | 143/149 | ✅ 2026-11-24 |
| U9b | `AttachmentPoint/Observer`, `Cell`, `Portal` families, `Mirror`, `ReferencePoint`, `Location`, `Corona`, `Function`, bone/skeleton modifiers | 113/209 | ✅ 2026-11-24; camera/display paths forward |
| U9b2 | bone/skeleton/modifier tail: `BoneModifier`, `BoneBlender`, `SkeletonModifier`, `ModifierHost` | 55/64 | ✅ 2026-11-24; 9 cross-module/RTTI methods forward |
| U10 | render primitives: `RenderPrimitive`, `Render2DPrimitive`, `Render3DPrimitive`, `RenderBox`, `RenderSphere`, `RenderPlane`, `RenderLines`, `RenderTube`, ... | 166/230 | ✅ 2026-11-24; geometry build/collision paths forward to U5/U6 |
| U11 | `BackgroundLoadingThread`, `BackgroundResource`, `Facility`, `JetGetPluginsRender3DCore`, exported singletons/statics | 49/167 | 🟡 background threads/resources done; plugin entry/statics later |

The order inside U5/U6 can change once RenderSystem's material and geometry contracts are frozen; RenderSystem is
already 2687/2687 local but still needs its original for private helpers, RTTI and shared state, so anything in
Render3DCore that calls a RenderSystem private helper by RVA must wait for that helper's cutover.

## U0/U1a: what is ours now

`src/jet/graphics/Render3DCore_7c/AxisAlignedBox.cpp`, header `include/jet/Render3DCore.h`, test
`tests/diff_r3d_u1.cpp`.

- Layout: `{Vector3 min; Vector3 max;}` = 0x18 bytes, no vftable.
- 25 exports: default/min-max/contiguous/strided/copy constructors, assignment, `SetMin`, `SetMax`, `GetMin`,
  `GetMax`, `Zero`, `IsEmpty`, `Maximize`, `Minimize`, `GetCenter`, `CalcCorners`, `Clamp`, `IsWithin` (Vector3,
  box, six planes), `TestCollision` (box, sphere), `operator==`, `operator!=`, `CalcBoxLimits` (protected).
- The other 9 `AxisAlignedBox` exports still forward: the `OrientedBox` constructor/maximise/test, the
  `Cylinder`/`Ellipsoid` tests, the three line tests, and `GetMinMaxCollision`.
- ABI: ordinary members of the original are **`__cdecl`** (the mangled `QAA`; the assembly reads `this` from
  `[esp+4]`), while constructors and the assignment are `__thiscall` (`QAE`). A by-value `Vector3` return puts the
  hidden return slot at `[esp+8]` after `this`; the differential test must call those through a member-function
  pointer, not a free-function pointer, or the hidden slot lands on the wrong side.
- NaN/comparison order is the original's: the assembly uses `COMISS` + `JBE`, so a NaN keeps the **left** operand.
  This matters in `Maximize`, `Minimize`, `CalcBoxLimits`, `Clamp` (a NaN coordinate clamps to `min`, not NaN) and in
  `TestCollision(Sphere)` (a NaN centre gives distance 0, not NaN). All are covered by the test's awkward-value
  rounds (signed zero, infinities, quiet and signalling NaNs).
- `CalcCorners` writes eight corners: the min-z face in `min, max-x, max-xy, max-y` order, then the same on max-z.
- `IsWithin(const Plane* const)` expects the six frustum planes as `{normal, d}` (0x10 bytes each); it picks the
  corner farthest along each normal and rejects the box only when that corner is strictly behind the plane.
- `IsWithin(box)`/`TestCollision(box)` are strict overlap on every axis: touching faces do **not** collide.
- `TestCollision(Sphere)`: `Sphere` is polymorphic, vftable at +0, centre at +4, `radius` at +0x10; the SDK header's
  field order is not the TS12 layout.

Verified: Win32 Release, `diff_r3d_u1` 29,000 checks, 0 mismatches over 1,000 rounds (every 7th an awkward-value
round). No game check yet; this unit does not affect the menu path.

## Build and test

```
cmake -S src/jet -B build32 -A Win32
cmake --build build32 --config Release --target Render3DCore_7c diff_r3d_u1
```

Prepare a test directory with the renamed original and our DLL (keep the game install untouched):

```
copy <game>\bin\Render3DCore_7c.dll <testdir>\Render3DCore_7c_orig.dll
copy build32\Release\Render3DCore_7c.dll <testdir>\Render3DCore_7c.dll
diff_r3d_u1 <testdir> 1000
```

`Render3DCore_7c_orig.lib` has not been generated yet because no U1a code calls a forwarded export. When a unit
does, generate it like the other modules:

```
python tools/gen_implib.py D:/trainz12-decomp/exports/Render3DCore_7c.tsv Render3DCore_7c_orig D:/trainz12-decomp/implib <lib.exe>
```

and add `"${IMPLIB}/Render3DCore_7c_orig.lib"` to the target.

## Risks and open questions

- **802 unnamed functions (48.9 KB)**. They are not exports, so a unit that needs one must decompile it and either
  implement it locally or call the original by RVA until the owning class moves. Do not count them as done when an
  export is forwarded.
- **Shared statics and vtables.** Caches, material registries, plugin tables and `RenderPrimitive::lights` are
  exported data. They stay original-owned until every reader/writer moves together; `DataMirrorList`-style mirrors
  are the pattern from RenderSystem U15.
- **Template catalogue.** U6 has 446 template functions and no direct exports of its own; its symbols exist because
  materials and the game instantiate them. It is ABI-sensitive and should follow RenderSystem's geometry units.
- **RenderSystem dependency.** 320 imports; RenderSystem still needs its original for private helpers and RTTI. A
  Render3DCore unit that calls one of those helpers is not standalone even when its exports are local.
- **Float determinism.** Camera, spline, bounds and multipass code are x87/SSE-order sensitive. Keep operand order
  from the assembly (the project already has `sse_lift.py` for the hard cases).
- **Concurrency.** `BackgroundLoadingThread`/`BackgroundResource` own a thread and a state machine; defer to U11 and
  test lifecycle separately.

## Cross-module notes

- **Render3DFoundation**: imports 108 Render3DCore exports; animation/geometry resources and `SplineResource`/
  `SplineDefinition` ownership must be split. Runtime spline maths (U2) belongs here; resource load/save can stay in
  Foundation.
- **Render3DExtension**: imports 134; decals/haze rely on scene traversal, camera and caches. Do not change cache
  ordering or singleton semantics without checking it.
- **RenderOpenGL / RenderDirectX9**: backends consume Display/Material/Geometry contracts through RenderSystem;
  Render3DCore's material and chunk units must not assume a specific backend's state model.
- **WaterSystem**: imports 80; if it needs material/chunk facilities beyond its local shell, prefer the shared
  RenderSystem contract over duplicating a runtime class.
- **trainz.exe**: 431 imports; the game is the main consumer of camera, scene graph, particles and primitives. A
  U8/U9 cutover needs an in-game check, not only a differential test.


## Wave 4 (2026-11-24)

The parallel wave 4 merged: U4 caches (129/129), U5a materials (241/374), U5b MultiPass (115/254),
U9b scene (200/209), U10 primitives (191/230), U11 statics/plugin entry (59/167), and the U1/U2/U3/U7/U8/U9
tails (+16). Module total **2500/2500** local, 562 forwarded.

New tests (all green): `diff_r3d_u4_rem` 1,102, `diff_r3d_u5a_rem` 14,909 (200 rounds), `diff_r3d_u5b_rem`
6,666, `diff_r3d_u9b_rem` 1,600 (200 rounds), `diff_r3d_u10_rem` 739, `diff_r3d_tails` 57,145 (5,000 rounds);
the existing 20 `diff_r3d_*` suites remain 0 mismatches. `diff_r3d_tails` needs a pristine
`KernelStandard_7c.dll` in its fixture (the original's `s_sqrtLUT` import is pointer-to-array).

Known partials carried into wave 5: U5a data tables/dtors/plugin factories/render paths (133), U5b render/display
and program init (139), U10 live-kernel paths (39), seven U9b methods that bridge private original RVAs, U11
plugin-entry helpers (8), 108 template/ITree/Line/StdMaterialPlugin exports, and the 69 small tails.


## Wave 5 (2026-11-24/25)

Wave 5 merged: U5a final (118 data/vtable/dtor/factory/render exports), U5b final (+91), U10 live paths
(39/39), U9b seven local definitions (six still call private RVAs), 108 template/ITree/Line exports, 43
tails2 and 27 tails. Module **2500/2500**, 163 forwarded. New tests: `u5a_final` 3,776, `u5b_final`
127,177, `tails2` 4,174, `templates` 30,852, `livepaths` 80,002; all 0 mismatches.

Wave 6 covers the last 163: U5b plugin/state (48), simple classes Camera/DebugRender/Tube/Transformation/
Primitive3D/ViewPoint/ViewPort/LightLoader (~76), tails3 (27) and the two material chunk paths + AutoTex
plugin vtable (4).


## Exports complete (2026-11-25)

All 2500 exports are local, 0 forwarders. U5b last 48, simple classes Camera/DebugRender/Tube/
Transformation/Primitive3D/ViewPoint/ViewPort/LightLoader 74, tails3 26, final 15 (MultiPass
SetTexture/DuplicateTexture/IsLoaded and the four OrientedBox helpers). New tests: u5b_last 160,
simple_rem 29,200, tails3 76,066, final15 10,712; every diff_r3d_* green.

`check_exports --forbid-dll` still reports 25 static imports and many dynamic RVA/vtable lookups
into `Render3DCore_7c_orig.dll`; that cutover is the remaining work (notes/agents/static_cutover.md,
notes/agents/dynamic_cutover.md).
