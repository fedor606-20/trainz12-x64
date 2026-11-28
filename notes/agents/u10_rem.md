# U10 remainder (wave 4): 26 more render-primitive exports

Status: 2026-11-24. **191/230 U10 exports are local; 39 still forward to `Render3DCore_7c_orig.dll`.**
`diff_r3d_u10_rem` 36,950 checks over 50 rounds, 0 mismatches (indices: every combination of the segment counts
below; rays: random and axis-aligned, both creation-flag states). The existing tests stay green:
`diff_r3d_u10` 516,600 checks / 300 rounds, `diff_r3d_u6` 293,480, `diff_r3d_u7` 75,000, `diff_r3d_u8` 77,500.

Built Win32 Release in `build32-r3d-u10` (`Render3DCore_7c`), and the new test compiled by hand because the parallel
test list in `src/jet/CMakeLists.txt` is a fixed `foreach` and is not mine to edit:

```
cl /nologo /EHsc /GR /Zc:wchar_t- /Zp8 /MD /O2 /D_CRT_SECURE_NO_WARNINGS ^
   src\jet\tests\diff_r3d_u10_rem.cpp /Fe:testdir-u10rem\diff_r3d_u10_rem.exe
```

The integrator only needs to add `u10_rem` to the `foreach(t ...)` list at `src/jet/CMakeLists.txt:258`.

## Implemented in this slice (26)

- **Volume (9)** - `RenderPrimitive::CalculateBoxVolume` (0x68c10) and the eight exported
  `CalculatePrimitiveVolume` entries (`RenderPrimitive` 0x68ba0 and the seven shape overrides, all tail jumps to
  `CalculateBoxVolume`). The helper allocates a `ParticleOrientedBoundingBox`, attaches it with
  `Particle::SetBoundingVolume`, builds an `OrientedBox` from the chunk's position stream
  (`GetVector3Array(R3D_ITEM_POSITION, 0)`, `GetNumVertices`) and calls the volume's slot +0x24
  (`SetLocalVolume(const OrientedBox&)`).
- **`RenderPrimitive::GetGeometryCenter`** - already coded in `RenderPrimitive.cpp`; it was simply missing from the
  fragment.
- **Index builders (11)** - `RenderBox::BuildIndicesWire`, the shared sphere/ellipsoid solid and wire
  (0x69550/0x696d0), cone solid/wire (0x63480/0x63520), cylinder solid/wire (0x641a0/0x642b0), plane solid/wire
  (0x679d0/0x67a60). Pure array writers; no material or chunk contract.
- **Ray tests (3)** - `RenderBox`, `RenderSphere`, `RenderEllipsoid` `GeometryTestCollision` (0x62760, 0x697e0,
  0x656d0): scaled-space slab/quadratic tests with the original's `t` choice.
- **Compiler closures (2)** - `??_FRender2DPrimitive` and `??_FRender3DPrimitive` from `__fastcall` helpers through
  `/alternatename`, the pattern used by U9b/U11. Each is the int constructor with allocation 1 plus the class vtable.

## Forwarded (39) and why

| Exports | Reason |
|---:|---|
| 15 | `Init` (base + every shape): the base one needs a live `RenderKernel` and its `MaterialManager` (`kernel+0x90`), builds the `RPM_Color_%X%s` name and drives `MaterialManager::Create`, the four `Material::Set*Color` calls and the chunk array. A test would need a real kernel/display. |
| 11 | `BuildGeometry`: creates the material's `GeometryChunk`, fills the position/colour streams and sets the primitive; the decodes exist but need U5/U6 live objects to diff. |
| 4 | `RenderLines::Add` (3) and `AddPolygon`: construct shapes and call the virtual `Init`/geometry builder, i.e. the same kernel/material path. |
| 1 | `RenderLines::Cull`: frame/pool traversal plus per-item model-view maths and `ViewPoint::Add`; needs a view point and kernel display. |
| 2 | `TestCollisionGeometry` (2D/3D): `ConstFaceDescriptor` is only forward-declared in our headers and the original's face test needs the geometry-face contract. |
| 3 | `RenderCone`/`RenderCylinder`/`RenderTube` `GeometryTestCollision`: cone/tube edge (cap/ring) logic was not finished; cylinder was decoded and linked but its inside/cap fallback still differed on several ray cases, so it was left forwarded rather than shipped unverified. |
| 1 | `RenderBox::BuildIndicesSolid`: its argument mapping differs from the wire builder (the first int is unused, the three counts are args 2..4) and the ring section is long; deferred rather than guessed. |
| 2 | `RenderTube::BuildIndicesSolid`/`Wire`: height-selected cap/ring meshes; long and not verified. |

`RenderLine` and `RenderSpline` now declare their own `CalculatePrimitiveVolume` override in the header so the two
exported symbols exist; the base `RenderPolyline` body still does the work.

## Layout and ABI facts learned

- **`CalculateBoxVolume`** needs the chunk from `chunkArray.block->data[0]`; the volume slot is +0x24, not the
  particle's vtable. The `ParticleOrientedBoundingBox` constructor takes the host primitive.
- **Index builder arguments are not uniform.** Box wire uses arg1..arg3 as `(x, y, z)`; box solid ignores arg1 and
  reads `(x, y, z)` from args 2..4. Sphere/ellipsoid/cone/cylinder use `(segments, vertexBase, indices)`; plane uses
  `(x, y, flags, indices)`. All builders truncate indices to 16 bits exactly where the assembly does.
- **Sphere/ellipsoid mesh details**: the top-cap tail resets the running vertex index to 1 before the ring body
  (otherwise the bottom cap starts one too high); the wire builder's first ring value starts at `vertexBase-2` and
  decreases by 2 per ring, while the second starts at `segments-1` and decreases by 2.
- **Cylinder solid** iterates `b-4`, not `b-2`; the two trailing cap triangles are fixed patterns.
- **Sphere/ellipsoid ray test**: `a=|d/scale|²`, `b=2*(p·d)/scale²`, `c=|p/scale|²-r²`,
  `disc=b²-4ac`. The assembly returns false when `0 > disc` (unordered passes), and when creationFlags bit 2 is clear
  and `0 >= c` ordered. For `disc==0` it takes `-b/2a`; otherwise it picks `(-b-sqrt)/2a` when that is not
  `< 0` ordered, else `(sqrt-b)/2a`. NaN inputs reach the point write with a NaN `t` (both comparisons are unordered).
- **Box ray test** is slab clipping with `eps=(|hz|+|hx|+|hy|)*1e-5`; a point exactly on a face counts as outside and
  the epsilon only widens the final accept. The inside/exit form is used when `creationFlags & 4`.
- **x87/SSE order**: `-0.0f - x` (not `-x`) matters in the volume/ray paths, and the point rebuild keeps the product
  as the `addss` destination (`dir*t + point`), as the module's shared `Vector3` helpers do. Intrinsic helpers
  (`FAdd/FSub/FMul/FDiv`, `_mm_comi*_ss`) reproduce the comparison directions including unordered cases.
- The two small public helpers `FAdd` etc. live in an anonymous namespace; ordinary members of these classes are
  `__cdecl` (this on the stack), virtuals `__cdecl`/`thiscall` as their mangled names say.

## Files

- `src/jet/graphics/Render3DCore_7c/U10Volume.cpp` (new) - volume.
- `src/jet/graphics/Render3DCore_7c/U10Indices.cpp` (new) - 11 index builders.
- `src/jet/graphics/Render3DCore_7c/U10Collision.cpp` (new) - box/sphere/ellipsoid ray tests and the two closures.
- `src/jet/graphics/Render3DCore_7c/RenderPrimitive.cpp` - unchanged (the base `GetGeometryCenter` was already there).
- `src/jet/graphics/Render3DCore_7c/RenderShapes.cpp`, `RenderPolyline.cpp`, `RenderPolygon.cpp` - removed the
  forwarded `CalculatePrimitiveVolume` bodies that are now local, and restored the cylinder ray forwarder.
- `src/jet/include/jet/Render3DCorePrimitives.h` - `RenderLine`/`RenderSpline` `CalculatePrimitiveVolume` overrides.
- `src/jet/tests/diff_r3d_u10_rem.cpp` (new) - differential test.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u10_rem.txt` (new) - the 26 names.

## Test

`diff_r3d_u10_rem <dir> [rounds]` compares the whole index buffer (64 KiB, 0xcd pre-filled) byte for byte for every
combination of small counts: box wire x/y/z 1..5, sphere/ellipsoid segments 1..8 x vertex bases {1,2,3,5,9,17,33,40},
cone/cylinder segments 1..8 x bases {2,3,5,9,17,33}, plane 0..6 x 0..6. It also runs 24 random plus axis-aligned rays
per shape and both creation-flag states, comparing the hit flag and the exact output point bits, and checks that the
four `CalculatePrimitiveVolume` vtable slots are populated on both sides.

Test directory: `testdir-u10rem` (copy of `testdir-u10` with the fresh DLL). No game run: none of these exports is on
the menu path; the 39 forwarded ones still rely on `Render3DCore_7c_orig.dll`.
