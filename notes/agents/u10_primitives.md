# U10: Render primitives (RenderPrimitive family, shapes, RenderLines)

Status: 2026-11-24. **168/230 exports are local and differentially verified; 62 forward to
`Render3DCore_7c_orig.dll`.** Win32 Release, `diff_r3d_u10` 1,722,000 checks over 1,000 rounds, 0 mismatches.

Workspace: `D:\universal modder\r3d-agents\u10`. The only shared file that had to be refreshed was
`src/jet/CMakeLists.txt`: the copy predated the `u10` entry in the parallel-test list, so it was re-synced from the
main tree (the merge checklist does not copy it).

## Files added

- `src/jet/include/jet/Render3DCorePrimitives.h` - the whole hierarchy and its layouts.
- `src/jet/graphics/Render3DCore_7c/RenderPrimitive.cpp` - `RenderPrimitive`, `Render2DPrimitive`,
  `Render3DPrimitive`, the reference-counted chunk/material blocks.
- `src/jet/graphics/Render3DCore_7c/RenderShapes.cpp` - `RenderBox`, `RenderSphere`, `RenderCircle`, `RenderEllipse`,
  `RenderCone`, `RenderCylinder`, `RenderEllipsoid`, `RenderPlane`, `RenderTube`.
- `src/jet/graphics/Render3DCore_7c/RenderPolyline.cpp` - `RenderPolyline`, `RenderLine`, `RenderSpline`.
- `src/jet/graphics/Render3DCore_7c/RenderPolygon.cpp` - `RenderPolygon`.
- `src/jet/graphics/Render3DCore_7c/RenderLines.cpp` - `RenderLines` lifetime, pooling and the static state.
- `src/jet/tests/diff_r3d_u10.cpp` - the differential test.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u10_primitives.txt` - 168 mangled names.

## Implemented (168)

- `RenderPrimitive`: all three constructors, copy constructor, assignment, destructor and vtable; `Set/GetOpacity`,
  `Set/GetScale`, `Set/GetGeometryPosition`, `Set/GetGeometryOrientation`, `GetGeometryRadius` (the zero stub),
  `GetGeometryCenter`, `CalculatePrimitiveVolume`, `Cull` (material activate + `ViewPoint::Add`), `Get/SetColor`,
  `GetCreationFlags`, `GetNumChunks`, both `GetChunkArray`/`GetMaterialArray`, `Set/GetTestCollision`,
  `Set/GetDebugRender`, `CreateGeometryChunk`, `InitInstance`, the exported `lights` static.
- `Render2DPrimitive` / `Render3DPrimitive`: constructors, copy, assignment, destructor, vtables and the base
  `IsWithin`/`GeometryTestCollision` zero stubs.
- Every shape: default/copy constructor, assignment, destructor, vtable, `CreateInstance`, `InitInstance`,
  `GetGeometryRadius`, `GetGeometryCenter` where the original defines one, `IsWithin` for the 2D shapes,
  `GetExtents` (box), `GetRadius` (sphere).
- `RenderPolyline`/`RenderLine`/`RenderSpline`: the same set, radius/centre from the scale, the family's virtual
  `BuildGeometry` slot.
- `RenderPolygon`: both vtables, constructors/copy/assignment/destructor, `CreateInstance`, `InitInstance`,
  `GetGeometryRadius` (polygon bounding sphere) and `IsWithin` (`Polygon::IsInside`).
- `RenderLines`: both constructors, copy, assignment, destructor, vtable, `RemoveAll`, `DeleteAll`,
  `SetState`/`GetState`/`Switch` and the exported `enabled` flag.

## Forwarded (62) and why

| Exports | Reason |
|---:|---|
| 15 | `Init` (all classes): needs the material manager and `PString` material names (U5/U4). The vtable slot uses a local trampoline into the original. |
| 11 | `BuildGeometry`: creates and fills the material-owned geometry chunk (U4/U5/U6). Local trampolines keep the virtual slots (`RenderPlane`, `RenderPolygon`, `RenderPolyline`). |
| 14 | `BuildIndicesSolid`/`Wire`: index builders are only called from `BuildGeometry`; they were deferred with it (the box grid decompile is register/argument-ambiguous). |
| 7 | shape-specific `CalculatePrimitiveVolume` (box, line, plane, polygon, polyline, spline, tube): builds U3's `ParticleOrientedBoundingBox` and reads chunk bounds. |
| 6 | `GeometryTestCollision` (box, sphere, cone, cylinder, ellipsoid, tube): ray tests over the shape; the base slot is local, the overrides forward. |
| 2 | `TestCollisionGeometry` (2D/3D): `ConstFaceDescriptor` face tests in the original. |
| 4 | `RenderLines::Add`/`AddPolygon`: construct and `Init` shapes and call the virtual geometry builder. |
| 1 | `RenderLines::Cull`: frame/pool traversal plus per-item model-view maths. |
| 2 | `??_F` compiler-generated default-constructor closures. |

Every forwarded virtual still has a local trampoline where the exported class vtable needs a stable slot (blocked
methods are reached through `GetProcAddress` on `Render3DCore_7c_orig.dll`). Those trampolines are deliberately not
listed in `implemented.d`.

## Layout and ABI facts

- `RenderPrimitive` is 0x188: `ViewPortParticle` 0x000, kernel +0x13c, chunk block +0x140, material block +0x144,
  orientation +0x148, position +0x158, scale +0x164, colour +0x170, creation flags +0x180, flags byte +0x184.
- Chunk/material arrays are `AutoReferenceArray<AutoReference<T>>` blocks `{refs, count, unused[2], items}`; the last
  reference releases each item with `ReferenceCount::ReleaseReference` and frees the block with `Jet::Alloc`. The
  `int` constructor allocates them; the default constructor leaves both null.
- `flags` +0x184: bit 0 orientation non-identity, bit 1 position non-zero, bit 2 scale non-1 (all compared with the
  original's 0.01 tolerance), bit 3 debug render. Assignment copies bits 0..3; `InitInstance` copies 0..2 and keeps
  the destination's bit 3.
- Shape sizes: box 0x194 (half extents +0x188), sphere/circle 0x18c, ellipse/cone/cylinder/plane 0x190,
  ellipsoid/tube 0x194, polyline/line/spline 0x198 (radius +0x188, centre +0x18c), polygon 0x1a4 (Polygon at +0x188).
- Inheritance: 3D shapes derive `Render3DPrimitive`, circle/ellipse/plane/polygon derive `Render2DPrimitive`,
  polyline/line/spline derive `RenderPrimitive` directly. That is what puts `TestCollisionGeometry`/`IsWithin`/
  `GeometryTestCollision` at slots 0xe8/0xec and the polyline family's virtual `BuildGeometry` at 0xe8,
  `RenderPlane`'s at 0xf0.
- Ordinary members are `__cdecl` (this first on the stack), constructors/assignment `__thiscall`, virtuals `__cdecl`.
  A by-value `Vector3` return uses the hidden slot at `[esp+8]`, so the test calls `GetGeometryCenter` through a
  member-function pointer. Array returns use the same hidden-slot convention.
- NaN/fp: orientation and colour copies go through x87 (`fld`/`fstp`, quieting an sNaN) while position and scale are
  integer copies. The fast square root in box/plane/cylinder/tube is transcribed as x87 (`fld`/`fmul st,st(1)`/
  `fmul st(1),st`/`fsubrp`/`fmulp`) because a double expression changes NaN payloads. SSE products and sums keep the
  original destination operand (`_mm_mul_ss`/`_mm_add_ss`/`_mm_comige_ss`): NaN payload selection depends on it.
  `Particle::flags` (+0x48) has only bits 0/1 defined; the test masks them.

## Test

```
cmake -S D:\universal modder\r3d-agents\u10\src\jet -B D:\universal modder\r3d-agents\u10\build32 -A Win32
cmake --build D:\universal modder\r3d-agents\u10\build32 --config Release --target Render3DCore_7c diff_r3d_u10
```

Test directory: `testbin` copied to `r3d-agents\u10\testdir`, original `Render3DCore_7c.dll` renamed to
`Render3DCore_7c_orig.dll`, our DLL plus the main build's live `Kernel_7c`, `KernelStandard_7c`, `KernelResource_7c`,
`KernelResources_7c`, `RenderSystem_7c` and `Render2DCore_7c`.

```
diff_r3d_u10.exe <testdir> 1000
1722000 checks, 0 mismatches
```

The test compares raw field bits (signed zero, infinities, quiet/signalling NaNs; every 7th round is an awkward-value
round, fixed seed), constructor/int-constructor/copy/assignment/`CreateInstance`/`InitInstance` state, reference
counts and block sharing, radius/centre/`IsWithin`, `GetExtents`/`GetRadius`, the exported vtable length and the
shared vtable slots, and the base `CalculatePrimitiveVolume` through `Particle::GetBoundingSphere`.

## Blockers / next pass

- `BuildGeometry` + `BuildIndices*` need a stand-in material and geometry chunk (U4/U5/U6 contract); with those the
  vertex/index output can be diffed exactly. The box grid decompile is the least reliable of the family.
- `Init` needs the material-manager path; `GeometryTestCollision`/`TestCollisionGeometry` need the ray/face contracts
  from U9a and the shape maths; `RenderLines::Add`/`Cull` need the shape `Init` and the view/matrix path.
- No game run: this unit has no menu path. `Render3DCore_7c_orig.dll` remains loaded for the forwarded exports and
  the blocked virtual trampolines, so the unit is not standalone yet.
