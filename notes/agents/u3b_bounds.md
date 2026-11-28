# U3b bounds: status and report

Status: **done**. All twelve `ParticleBoundingVolume` derived exports U3 forwarded are local and differentially verified
against `Render3DCore_7c_orig.dll`: **12/12 implemented, 0 forwarded from this slice, 0 mismatches** over 1,000 rounds
(`158000 checks`). The U3 regression test still passes (`282000 checks, 0 mismatches`).

Worktree: `D:\universal modder\r3d-agents\u3b`. The only shared file touched is `src/jet/CMakeLists.txt`, re-synced
from the main tree because the copy predated the `u3b` entry in the parallel-test list (the main tree already contains
`u3b`; the merge must not copy it back).

## Implemented (12, all in `ParticleBoundingView.cpp`)

| Class | Export |
|---|---|
| `ParticleAxisAlignedBoundingBox` | `IsWithinView(ViewPoint&)`, `IsWithinView(Camera*)`, `CreateRenderableObject`, `CheckRenderableObjectPosition` |
| `ParticleBoundingSphere` | same four |
| `ParticleOrientedBoundingBox` | same four |

The four `ParticleVisibleBoundingBox` view methods were already local in U3 (always true), so nothing from that class
was forwarded. No export from this slice stays forwarded.

## Files

- added `src/jet/graphics/Render3DCore_7c/ParticleBoundingView.cpp`
- added `src/jet/tests/diff_r3d_u3b.cpp`
- added `src/jet/graphics/Render3DCore_7c/implemented.d/u3b_bounds.txt` (12 mangled names)
- edited `ParticleAxisAlignedBoundingBox.cpp`, `ParticleBoundingSphere.cpp`, `ParticleOrientedBoundingBox.cpp`:
  removed the U3 forwarded bodies for these twelve methods (the rest of those units is unchanged); comments updated.

## Test

```
cmake -S "D:\universal modder\r3d-agents\u3b\src\jet" -B "D:\universal modder\r3d-agents\u3b\build32" -A Win32
cmake --build "D:\universal modder\r3d-agents\u3b\build32" --config Release --target Render3DCore_7c diff_r3d_u3b
```

`testdir` is the full `testbin` copy with `Render3DCore_7c.dll` renamed to `Render3DCore_7c_orig.dll`, our DLL beside
it, and the main build's live `KernelStandard_7c`, `Kernel_7c`, `KernelResource_7c`, `KernelResources_7c`,
`RenderSystem_7c`, `Render2DCore_7c`. Then:

```
diff_r3d_u3b.exe <testdir> 1000
158000 checks, 0 mismatches
```

Also re-ran after the U3b change:

```
diff_r3d_u3.exe  <testdir> 1000   -> 282000 checks, 0 mismatches
diff_r3d_u8.exe  <testdir> 200    ->  31000 checks, 0 mismatches
diff_r3d_u10.exe <testdir> 100    -> 172200 checks, 0 mismatches
check_exports.py build32/Release/Render3DCore_7c.dll Render3DCore_7c.tsv
  -> DLL 2500 exports, TSV 2500; missing/wrong 0, extra 0
```

The twelve export entries resolve inside our image (no `_orig` import thunk). The OBB `IsWithinView` entries have the
same RVAs as the sphere ones (`0x1E0C0`/`0x1E120`) - identical-code folding keeps the original's aliasing.

## What the test covers

- Real `Camera` objects built through each module's `Camera(pos, quat)` constructor; a 0x70-byte `ViewPoint` stand-in
  with the camera pointer at +0x04 (the only field these paths read).
- All six `IsWithinView` exports: valid camera, null camera pointer (both return false), the same camera with the cone
  already up to date, dirty-flag recalculation paths, and object bytes before/after.
- `CreateRenderableObject` for all three classes. The fake kernel's `+0x90` material manager returns a failure from
  `Create`, so `RenderPrimitive::Init` stops before the material/geometry path; the allocation, dirty recalculation,
  size arguments, sphere radius, colour/creation flags, debug-bit clear, position and returned object are compared.
- `CheckRenderableObjectPosition` in both directions: an exactly matching primitive (kept and moved) and a mismatched
  one (destroyed, rebuilt, colour copied, position set; OBB also orientation). The allocator may hand the freed block
  back, so a rebuild is identified by the resulting state, not the pointer.
- 1,000 rounds, fixed seed, every 7th round uses signed zero, infinities, quiet/signalling NaNs (including NaN sphere
  radii and NaN OBB extents).

## Layout / ABI facts

- `ViewPoint` remains a U9 class; only the camera pointer at +0x04 is read. `Camera::GetFrustum()` (local U8) performs
  the `+0x25e` dirty-view-cone update; `Camera::Clip3D` is reached through `Render3DCore_7c_orig.dll` (see blocker).
- AABB `IsWithinView` uses `AxisAlignedBox::IsWithin(const Plane*)` on `camera->GetFrustum()`.
- Sphere/OBB `IsWithinView` recalculates the global sphere centre on dirty bits 0/1, the sphere volume on bit 2, and
  answers `Clip3D(globalSphereCenter, sphere.radius) != BACK` (2). The OBB exports really do use the embedded base
  sphere, not the OBB extents.
- `CreateRenderableObject` allocates through `Alloc::New` and the local U10 constructors; AABB sizes are `max-min`
  (x87 subtract, as in the assembly), OBB sizes are `2*extents`; both pass `(1,1,1)` segments and creation flags `4`,
  then clear bit 3 of `+0x184`.
- Sphere creation calls `RenderPrimitive::Init(...,4)`, then `RenderSphere::BuildGeometry(...,0x10,4)` (the header's
  `creationFlags`/`segments` names are swapped relative to the ABI; the third stack word is the segment count), then
  the virtual `CalculatePrimitiveVolume` at vtable slot `0xe4`, exactly like the original.
- `CheckRenderableObjectPosition` compares half extents for AABB (SSE `mulss` by 0.5), raw extents for OBB, and the
  radius for the sphere. `!=` follows the original's unordered-comparison behaviour (NaN means "equal", i.e. no
  rebuild). On rebuild the old primitive is destroyed through vtable slot 0 with argument 1, a new one is constructed,
  and the old colour is copied through `Init`.
- OBB `CheckRenderableObjectPosition` sets the position to `globalSphereCenter`, then the orientation to
  `Quaternion::Set(OBB axes) * globalOrientation` (the second operand is `globalOrientation`; multiplication order is
  the same as U3's `MultiplyQuaternions` and was exercised with NaN orientations).

## Blockers / findings for the main agent

1. **U8 `Camera::Clip3D` has a NaN bug.** In the COINCIDENT continuation scan it uses `radius <= Neg(d)`; the original
   compares `COMISS radius,-d` and `JBE`, which treats an unordered comparison as true and returns `BACK` for a NaN
   radius. `Camera::Clip3D` is therefore called through `Render3DCore_7c_orig.dll` from the sphere/OBB view methods.
   This is the only reason those methods still touch the original; U3's calls to U1c's NaN-affected `OrientedBox`
   methods are the precedent. The local fix is one comparison in `Camera.cpp`:
   `if (d < 0.0f && !(radius > Neg(d))) return BACK;` - then the wrapper in `ParticleBoundingView.cpp` can be removed
   and the methods are fully local.
2. `RenderPrimitive::Init` and `RenderSphere::BuildGeometry` are still forwarded by U10, so `CreateRenderableObject`
   calls them through the original (the accessor keeps `Init` non-virtual like the original). When U10 localizes them
   the call sites stay valid.
3. U10's local `RenderPrimitive(int)` sets `flags` to 8 while the original constructor is
   `flags = (flags & 0xf8) | 8`, preserving the upper five bits of the recycled allocation. They are undefined bits
   (only 0..3 are used), so the test masks them; worth noting for U10 if byte-exact objects matter.
4. No game run; no anti-cheat or install change.

To merge: copy `ParticleBoundingView.cpp`, `diff_r3d_u3b.cpp`, `implemented.d/u3b_bounds.txt` and the three edited
`Particle*Bounding*.cpp` files into the main tree, rebuild `Render3DCore_7c` and `diff_r3d_u3b`, rerun the command
above (expected `158000 checks, 0 mismatches`). Do not copy `CMakeLists.txt` (main already has `u3b`).
