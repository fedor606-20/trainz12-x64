# U3 bounds: status and report

Status: **done**. The five particle bounding-volume classes are local and differentially verified against
`Render3DCore_7c_orig.dll`: **89/101 exports implemented, 12 forwarded, 0 mismatches** over 1,000 rounds.

Worktree: `D:\universal modder\r3d-agents\u3` (built from the wave-1/2 copy of `trainz12-x64`, not edited in the
main tree). Files added there:

- `src/jet/include/jet/Render3DCoreBounds.h`
- `src/jet/graphics/Render3DCore_7c/ParticleBoundingVolume.cpp`
- `src/jet/graphics/Render3DCore_7c/ParticleAxisAlignedBoundingBox.cpp`
- `src/jet/graphics/Render3DCore_7c/ParticleBoundingSphere.cpp`
- `src/jet/graphics/Render3DCore_7c/ParticleOrientedBoundingBox.cpp`
- `src/jet/graphics/Render3DCore_7c/ParticleVisibleBoundingBox.cpp`
- `src/jet/tests/diff_r3d_u3.cpp`
- `src/jet/graphics/Render3DCore_7c/implemented.d/u3_bounds.txt` (89 mangled names, no comments beyond the header)

No shared file, CMake file, README/MODLOG/PORT_PLAN or other stream fragment was changed.

## Implemented

| Class | Implemented | Forwarded |
|---|---:|---:|
| `ParticleBoundingVolume` | 33/33 (all its exports, including both `SetRawDirtyFlags` overloads, child-union helpers and the global frame recalculations) | 0 |
| `ParticleAxisAlignedBoundingBox` | 18/22 | 4 |
| `ParticleBoundingSphere` | 18/22 | 4 |
| `ParticleOrientedBoundingBox` | 12/16 | 4 |
| `ParticleVisibleBoundingBox` | 8/8 (including both always-true `IsWithinView`) | 0 |

The five `??_7...6B@` vftables and the five destructors are local too. Names are exactly the TSV's; in particular the
`Recalculate*Volume` virtuals are **protected** (`MBA`), not public (`UBA`): the access code is part of the mangled
name, and this matters for export resolution (see ABI notes).

## Forwarded and why

Per class `ParticleAxisAlignedBoundingBox`, `ParticleBoundingSphere` and `ParticleOrientedBoundingBox`:

- `IsWithinView(const ViewPoint&)`, `IsWithinView(const Camera*)` вЂ” need the U8 `Camera`/`ViewPoint` internals
  (`Camera::UpdateViewCone`, `Camera::Clip3D`, the camera frustum at `+0x150`). The OBB vtable aliases the
  `ParticleBoundingSphere` bodies, as the original does.
- `CreateRenderableObject`, `CheckRenderableObjectPosition` вЂ” need the U10 render primitives (`RenderKernel`,
  `Render3DPrimitive`, `RenderBox`, `RenderSphere`).

These 12 exports stay forwarded in `.def`. Each derived class still defines a local override with the right body
shape so its vtable has 19 entries and the slots are correct; those local override bodies call the original export
through `Render3DCore_7c_orig.dll`. The export entries themselves are forwarders, so the vtable and the export table
do not diverge.

## Test

```
cmake -S "D:\universal modder\r3d-agents\u3\src\jet" -B "D:\universal modder\r3d-agents\u3\build32" -A Win32
cmake --build "D:\universal modder\r3d-agents\u3\build32" --config Release --target Render3DCore_7c diff_r3d_u3
```

Test directory is `testbin` with `Render3DCore_7c.dll` renamed to `Render3DCore_7c_orig.dll`, our built DLL copied
beside it, and the live `KernelStandard_7c`, `Kernel_7c`, `KernelResources_7c`, `KernelResource_7c`, `RenderSystem_7c`,
`Render2DCore_7c` DLLs from `trainz12-x64\build32\Release`. Then:

```
diff_r3d_u3.exe <testdir> 1000
282000 checks, 0 mismatches
```

`tools/check_exports.py` on the built DLL: `DLL 2500 exports, TSV 2500; missing/wrong 0, extra 0`. A byte scan of the
89 implemented export entries showed all point into our own image (none is an `_orig` IAT thunk).

The test calls every exported method through `GetProcAddress` on identical buffers for both modules and compares raw
bytes and float bit patterns. It masks only the module-specific vftable words: the object's, the embedded `Sphere`'s at
`+0x08`, and (for `ParticleBoundingSphere`) the local sphere's at `+0xa0`. Every seventh round is an awkward-value
round (signed zero, infinities, quiet NaNs, a quiet NaN with payload, a signalling NaN). Host particles are plain
`Particle`-layout stand-ins (U9a layout) so parent/child dirty-flag walks, `GlobalToLocal`/`LocalToGlobal`, scene
orientation and position are exercised; parent/child volumes are real AABB objects so child recalculation runs too.
The test also calls vtable slots 7..12 (Set/GetLocalVolume), slots 3/4 (collision) and checks the vftable pointer of
every constructed object, `CreateInstance`, `InitInstance`, the destructors and the `ParticleVisibleBoundingBox`
always-true `IsWithinView`.

## Layout and ABI facts

- `ParticleBoundingVolume`: `0xa0` bytes. `+0x00` vftable, `+0x04` dirty flags, `+0x08` embedded `Sphere`
  (its own vftable, centre `+0x0c`, radius `+0x18`), `+0x1c` global sphere centre, `+0x28` `AxisAlignedBox`,
  `+0x40` `OrientedBox`, `+0x7c` global position, `+0x88` global orientation, `+0x98` host `Particle*`,
  `+0x9c` primary volume type (`3` base, `0` sphere, `1` AABB/visible, `2` OBB).
- `ParticleAxisAlignedBoundingBox` / `ParticleOrientedBoundingBox` / `ParticleVisibleBoundingBox`: `0xdc` bytes,
  an `OrientedBox` at `+0xa0`. `ParticleBoundingSphere`: `0xb4` bytes, a `Sphere` at `+0xa0`.
- Dirty bits: `0x01` global sphere centre, `0x02` sphere volume, `0x04` OBB, `0x08` AABB, `0x10` global position,
  `0x20` global orientation. Fresh values use `0xfffff`. `SetRawDirtyFlags(unsigned long)` is an **assignment**, not
  an OR (the `(bool, unsigned long)` overload is the OR/AND variant).
- Virtual slot order is destructor, `IsWithinView(Camera*)`, `IsWithinView(ViewPoint&)`, `TestCollision(LineSegment&)`,
  `TestCollision(Ray&)`, `OnChildAdded`, `OnChildRemoved`, `SetLocalVolume(Sphere/AABB/OBB)`,
  `GetLocalVolume(Sphere/AABB/OBB)`, `CreateInstance`, `CreateRenderableObject`, `CheckRenderableObjectPosition`,
  `RecalculateSphereVolume`, `RecalculateAxisAlignedBoxVolume`, `RecalculateOrientedBoxVolume`. Same-name virtual
  overloads are written in **reverse declaration order** in the header to get those slots.
- Ordinary members are `__cdecl` (`QAA`/`IBA`), constructors and assignments `__thiscall` (`QAE`), destructors
  `UAE`. The `Recalculate*` virtuals are `MBA` (protected virtual `__cdecl`). Declaring them public made the compiler
  emit `UBA` names; the linker then exported the `_orig` import thunks for the `MBA` names while the vtable used the
  local `UBA` functions, so the objects recomputed through a different implementation than the exported entry point.
  The fix is to keep those three declarations under `protected:` in every derived class.
- `ParticleBoundingVolume`'s eighteen base virtual slots are non-pure defaults in the original: `xor eax,eax; ret`
  (false/null/no-op). They are implemented as such, so a bare base object is usable.
- The base ctor sets the embedded sphere vftable and the OBB from a zero AABB; the global orientation is `(0,0,0,1)`.
  The copy ctor copies the sphere centre, global sphere centre and global position with plain moves, but the radius,
  AABB and global orientation through x87 `fld/fstp`; the assignment copies **all** floats through x87. This differs
  for a signalling NaN and is covered by the test.

## Behaviour and NaN notes

- `SetOrientationChanged(old, new)` computes `pos + Rotate(Rotate(center - pos, inverse(old)), new)`;
  `SetPositionOrientationChanged(oldPos,newPos,oldQ,newQ)` computes
  `newPos + Rotate(Rotate(center - oldPos, inverse(oldQ)), newQ)`. The inverse is
  `(-x,-y,-z,w) / (xВІ+yВІ+zВІ+wВІ)` with the assembly's `-0.0 - x` negations and sum order. `RotateByQuaternion`
  must not be called with `out == v`: the original always used a separate output buffer, and the helper writes
  `out[0]` before it reloads `v[0]`.
- `SetPositionChanged` updates the sphere centre as `new + (center - old)` (that operand order matters for NaN
  payloads), then marks ancestors and descendants.
- AABB recalculations: `RecalculateSphereVolume` uses the AABB at `+0x28` (child union plus local shape), the
  radius is `Length(max - min) * 0.5`; `RecalculateAxisAlignedBoxVolume` fills `+0x28` with the child union and merges
  its local OBB (`+0xa0`); `RecalculateOrientedBoxVolume` writes the **base** `OrientedBox` at `+0x40`, not `+0xa0`.
- OBB recalculations use the primary `OrientedBox` at `+0x40`: `RecalculateSphereVolume` = rotated `+0x40` centre plus
  host position, radius `Length(+0x40 extents)`; `RecalculateAxisAlignedBoxVolume` = transformed `+0x40` box into
  `+0x28`; `RecalculateOrientedBoxVolume` fills `+0x40` with the child union, then `Maximize(base, localBox)` when both
  are non-empty, or `base = localBox` when the child union is empty. The collision methods transform `+0x40` and use
  the local `+0xa0` extents for the second `bool&` result.
- Sphere `RecalculateSphereVolume` fills the embedded `Sphere` from the child union and merges the local sphere;
  `RecalculateAxisAlignedBoxVolume` uses the embedded sphere radius and the global centre; `RecalculateOrientedBoxVolume`
  builds the OBB from `(center - host position)` rotated by the host's inverse orientation with the assembly's exact
  grouping (a different term order from `RotateByQuaternion` is a 1-ULP difference and was visible over 200 rounds).
- `GetChildBoundingOrientedBox` transforms each child's OBB by the **child's own** position and orientation, not the
  host's, then uses `OrientedBox::Init` with the 16 corners. Its mangled name is
  `?Init@OrientedBox@Jet@@IAAXHV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@Z` (note `@@2@@Z`, one more
  `@2@` than in the loose decomp lines).
- `OrientedBox::Transform`, `OrientedBox::Init`, `OrientedBox::Maximize` and `AxisAlignedBox(const OrientedBox&)` are
  called through the original `_orig` exports from this unit, because their U1c versions do not reproduce the
  original's NaN operand paths. `Particle`'s U9a layout/methods are reached through small `BoundsDetail` accessors so
  `Render3DCoreParticle.h` and `Render3DCoreLines.h` (both define `ClipResult`) never enter the same translation unit.
- `ParticleBoundingSphere::TestCollision` mirrors the original's host-volume lookup: it recalculates the host's current
  bounding volume sphere/global centre, compares `distanceSquared - rВІ < 0.0001` in x87, and uses the local sphere only
  when its radius is positive and smaller than the host volume radius. The `LineSegment` twin is identical with
  `LineSegment::DistanceSquared`.

## Blockers

None for U3. The 12 forwarded exports wait on U8 (`Camera`/`ViewPoint`) and U10 (render primitives). U3 calls the still
forwarded `Sphere::TestCollision(Ray/LineSegment)` from U1d2; if U1d2 localizes those later, U3 keeps the same call
and should remain byte-identical.

To merge: copy the eight files above into the main tree, rebuild `Render3DCore_7c` and `diff_r3d_u3`, then rerun
`diff_r3d_u3.exe <testdir> 1000` (expected `282000 checks, 0 mismatches`).
