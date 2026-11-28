# Render3DCore_7c wave 5: live kernel paths (U10 remainder + U9b final)

Status: 2026-11-25. U10 is complete: the 39 exports that needed a render kernel, material manager or geometry chunk are
local definitions and differentially verified. U9b has local definitions for the last 7 exports; `Mirror::CreateCamera`
is a full local transcription, the other six are local bodies that still call the original's private implementation at
its RVA and copy its `stats`/`traversed`/`total`/`enableStencilMode` back into our module state.

## U10: implemented (39/39)

| group | exports |
|---|---|
| `Init` (15) | `RenderPrimitive`, `RenderBox`, `RenderSphere`, `RenderCircle`, `RenderEllipse`, `RenderCone`, `RenderCylinder`, `RenderEllipsoid`, `RenderPlane`, `RenderTube`, `RenderPolyline`, `RenderLine`, `RenderSpline`, `RenderPolygon` (both overloads) |
| `BuildGeometry` (11) | box, sphere, circle, ellipse, cone, cylinder, ellipsoid, plane, tube, polyline, polygon |
| `RenderLines` (4) | the three `Add` overloads and `AddPolygon` |
| `RenderLines::Cull` (1) | line, polygon and sphere pools; spheres get `model-view * rotation/translation` |
| `TestCollisionGeometry` (2) | `Render2DPrimitive`, `Render3DPrimitive` |
| cone/cylinder/tube ray tests (3) | scaled-space quadratic + cap tests; cylinder/tube ignore the outer origin when `creationFlags & 4` is clear |
| index builders (3) | `RenderBox::BuildIndicesSolid`, `RenderTube::BuildIndicesSolid/Wire` |

The base `RenderPrimitive::Init` builds `RPM_Color_%X%s` with `NoTextureMaterial`'s `m.notex`, calls
`MaterialManager::Create` through vtable slot 3, sets the four material colours, opacity and the two-sided flag, and
appends the material to the primitive's block. `BuildGeometry` uses the material's `CreateGeometryChunk`, the chunk's
position/colour views and `InitializeBuffers(display, true)`.

## U9b: seven exports

| export | body |
|---|---|
| `Mirror::CreateCamera` | full local transcription: model view, reflection matrix (`nx=-m4`, `ny=-m5`, `nz=-m6`, `d=m12*nx+m13*ny+m14*nz`), then the original `Camera` constructor by name |
| `Cell::Cull`, `Cell::AddAllToVisible` | local definitions; call the original octree body at 0x1c040 / 0x1c8e0 and copy the 19-word `Cell::stats` block home |
| `PhysicalPortal::Traverse/Enter/Leave` | local definitions; call 0x53f30 / 0x542d0 / 0x54690 and copy `Portal::traversed`, `Portal::total`, `Portal::enableStencilMode` home |
| `VirtualPortal::CreateCamera` | local definition; call 0x75c10 (two-model-view composition not decoded yet) |

The module's own state is therefore consistent after every call; the remaining work is replacing the six internal
`OrigRva` calls with transcriptions. The private octree split/visibility helpers (0x1ce50/0x1cef0/0x1d350/0x1d850) and
the display state block save/restore are the blocker for those bodies.

## Test

`tests/diff_r3d_livepaths.cpp` (built by hand / scratch target because the main `foreach` list is not ours):

```
diff_r3d_livepaths <dir with Render3DCore_7c_orig.dll and the patch DLL> 100
-> 80002 checks, 0 mismatches
```

The test runs every `Init`, every `BuildGeometry` (box 1..3 segments in three axes; sphere/circle/ellipse/cone/
cylinder/tube 4..9 segments; ellipsoid 4..8; plane 1..4 x 1..4; polyline; polyline from a four-point B-spline; both polygon overloads), all three ray
tests over random and axis-aligned rays in both creation-flag states, the 2D/3D `TestCollisionGeometry` entry points,
and `Mirror::CreateCamera`'s matrix. It compares the primitive fields, the chunk creation arguments, the whole chunk
call log, position/colour/index buffers and the output point bits.

Existing tests on the same module: `diff_r3d_u10` 1,722,000 / 0, `diff_r3d_u10_rem` 739,000 / 0, `diff_r3d_u9b`
2,060 / 0, `diff_r3d_u9b_rem` 8,000 / 0, `diff_r3d_u7` 150,000 / 0, `diff_r3d_u8` 155,000 / 0, `diff_r3d_u9a`
30,080 / 0, `diff_r3d_u9b2` 82,726 / 0. `check_exports` on the last complete link reported DLL 2500 / TSV 2500,
missing/wrong 0, extra 0.

## Layout / ABI facts

- `MaterialManager::Create` is called with `(manager, &result, &material, &name)`; the result class is 4 bytes.
- `GeometryChunk` item views return `{stride, data}` in a hidden destination pointer after `this`. For the stand-in:
  type 1 = positions (stride 12), type 2 = packed colour (stride 4), type 4 = colour3 (stride 12).
- `CreateGeometryChunk` type values: 0 triangles, 2 triangle fan, 3 outline/fan pair, 4 line strip. Box/cone/cylinder/
  tube/sphere choose solid 0 or wire 3/4 exactly as the original; the plane wire quad uses 4.
- `RenderLines` keeps `lineCount/lineCap` at +0x158/+0x15c, `sphereCount/Cap` +0x138/+0x13c, `polygonCount/Cap`
  +0x178/+0x17c. `Add` uses the pool block at +0x140/+0x12c/+0x160 and sets item opacity 0.3.
- Ray tests work in scaled space (`direction/scale`, `point/scale`), then rebuild the point as
  `ray.point + ray.direction*t` in world space. Cone surface is `x^2+y^2 = (r/h)^2 z^2` with `0<=z<=h`; cylinder and
  tube include the `creationFlags & 4` inside case. The 2D test's Z branch compares
  `|point.z*1.19e-7| > |direction.z|`, then signs, then `t=-point.z/direction.z`.
- `Cell::stats` is a 4-byte pointer followed by 18 words; the cull counters are indices 4, 11 and 12.
- `Portal::traversed/total` are private statics at the module data block; `enableStencilMode` starts true.

## Files

- `src/jet/graphics/Render3DCore_7c/U10LivePrimitives.cpp` - every `Init`.
- `src/jet/graphics/Render3DCore_7c/U10LiveGeometry.cpp` - every `BuildGeometry` + `RenderPlane::BuildGeometry`.
- `src/jet/graphics/Render3DCore_7c/U10LiveIndices.cpp` - box-solid and tube index builders.
- `src/jet/graphics/Render3DCore_7c/U10LiveCollision.cpp` - cone/cylinder/tube ray tests and 2D/3D collision entry.
- `src/jet/graphics/Render3DCore_7c/U10LiveLines.cpp` - `RenderLines` Add/AddPolygon/Cull.
- `src/jet/graphics/Render3DCore_7c/U9bBridge.cpp` - the seven U9b definitions and state sync.
- `src/jet/graphics/Render3DCore_7c/RenderPrimitive.cpp`, `RenderShapes.cpp`, `RenderLines.cpp`,
  `RenderPolyline.cpp`, `RenderPolygon.cpp` - removed the forwarders now defined in the U10Live files.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u10_live.txt` (39), `u9b_final.txt` (7); the seven U9b names were
  removed from `u9b_rem.txt`.
- `src/jet/tests/diff_r3d_livepaths.cpp` - differential test.

## Build note

`build32-r3d-live` built `Render3DCore_7c` (692,736-byte DLL) and `check_exports` reports DLL 2500 / TSV 2500,
missing/wrong 0, extra 0. The merged implemented list is 2326 names. No game run was made.


## Follow-up: standard fixture run (2026-11-25)

`cd D:\universal modder\testbin-r3d; .\diff_r3d_livepaths.exe . 100` now runs to completion:
`80002 checks, 0 mismatches`.

Two issues were fixed:

1. The harness only loaded `livepatch.dll` (the wave-5 scratch module), which the standard fixture does not have.
   It now resolves the directory with `GetFullPathNameA`, loads `Render3DCore_7c_orig.dll` first, then prefers the merged
   `Render3DCore_7c.dll` and falls back to `livepatch.dll` for the scratch build. Error messages name the DLL that
   failed and its `GetLastError`.
2. `RenderPrimitive::CreateGeometryChunk` (RenderPrimitive.cpp) assigned the new chunk into an `AutoReference` slot
   that had never been constructed (the block comes from `Alloc::New`). The assignment released the heap garbage in
   the slot and crashed. The slot is now placement-new constructed as an empty `AutoReference` before the raw-pointer
   assignment, matching the original's raw store. The scratch livepatch build had used the original module's
   `CreateGeometryChunk`, which is why only the merged DLL exposed it.

Re-ran in the same fixture: `diff_r3d_u10` 1,722,000 / 0, `u10_rem` 739,000 / 0, `u9b_rem` 8,000 / 0,
`u9b` 2,060 / 0, `u7` 150,000 / 0, `u8` 155,000 / 0, `u9a` 30,080 / 0, `u9b2` 82,726 / 0; `check_exports`
DLL 2500 / TSV 2500, missing/wrong 0, extra 0.
